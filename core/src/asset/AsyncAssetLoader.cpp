#include "lupine/asset/AsyncAssetLoader.hpp"
#include "lupine/asset/ArchetypeInstance.hpp"
#include "lupine/asset/Asset.hpp"
#include "lupine/core/ArchetypeRegistry.hpp"
#include "lupine/core/ArchetypeDefinition.hpp"
#include "lupine/platform/FileSystem.hpp"
#include "lupine/logger/Logger.hpp"

namespace lupine {
namespace asset {

enum class AsyncAssetKind {
    Instance,
    Definition
};

struct AsyncAssetLoader::Request {
    Handle handle = 0;
    AsyncAssetKind kind = AsyncAssetKind::Instance;
    std::string path;

    // Streaming bookkeeping (main-thread / mutex-guarded). priority orders the
    // pending queue; sequence is the FIFO tie-break; dispatched flips true once
    // the request has been handed to a worker (and so can no longer be
    // re-prioritized or cancelled-while-queued).
    int priority = 0;
    uint64_t sequence = 0;
    bool dispatched = false;

    // Payload produced by the worker thread, published under the loader mutex by
    // setting backgroundDone last. After backgroundDone is observed true by the
    // main thread, the worker no longer touches this record.
    AssetRef<ArchetypeInstance> instance;       // parsed instance (Kind::Instance)
    core::ArchetypeDefinition definition;       // parsed definition (Kind::Definition)
    std::string className;
    bool backgroundDone = false;
    bool backgroundSuccess = false;

    // Main-thread state, settled in Pump().
    AsyncLoadStatus status = AsyncLoadStatus::Pending;
    bool finalized = false;
    bool cancelled = false;

    InstanceCallback instanceCallback;
    DefinitionCallback definitionCallback;
};

AsyncAssetLoader& AsyncAssetLoader::GetInstance() {
    static AsyncAssetLoader instance;
    return instance;
}

AsyncAssetLoader::~AsyncAssetLoader() = default;

void AsyncAssetLoader::EnsurePool() {
    if (m_Pool) {
        return;
    }
    unsigned int hardware = platform::Thread::GetHardwareConcurrency();
    size_t threadCount = (hardware > 4) ? 4 : 2;
    m_Pool = std::make_unique<platform::ThreadPool>(threadCount);
}

AsyncAssetLoader::Handle AsyncAssetLoader::LoadArchetypeInstanceAsync(const std::string& resPath,
                                                                      InstanceCallback callback,
                                                                      int priority) {
    if (resPath.empty()) {
        return 0;
    }

    Handle handle = m_NextHandle.fetch_add(1, std::memory_order_relaxed);

    std::shared_ptr<Request> request = std::make_shared<Request>();
    request->handle = handle;
    request->kind = AsyncAssetKind::Instance;
    request->path = resPath;
    request->priority = priority;
    request->instanceCallback = std::move(callback);

    {
        platform::LockGuard lock(m_Mutex);
        EnsurePool();
        request->sequence = m_NextSequence++;
        m_Requests[handle] = request;
        m_Pending.push_back(request);
        DispatchPending();
    }

    return handle;
}

AsyncAssetLoader::Handle AsyncAssetLoader::LoadArchetypeDefinitionAsync(const std::string& filePath,
                                                                        DefinitionCallback callback,
                                                                        int priority) {
    if (filePath.empty()) {
        return 0;
    }

    Handle handle = m_NextHandle.fetch_add(1, std::memory_order_relaxed);

    std::shared_ptr<Request> request = std::make_shared<Request>();
    request->handle = handle;
    request->kind = AsyncAssetKind::Definition;
    request->path = filePath;
    request->priority = priority;
    request->definitionCallback = std::move(callback);

    {
        platform::LockGuard lock(m_Mutex);
        EnsurePool();
        request->sequence = m_NextSequence++;
        m_Requests[handle] = request;
        m_Pending.push_back(request);
        DispatchPending();
    }

    return handle;
}

size_t AsyncAssetLoader::EffectiveMaxConcurrent() const {
    if (m_MaxConcurrent > 0) {
        return m_MaxConcurrent;
    }
    return m_Pool ? m_Pool->GetThreadCount() : 1;
}

void AsyncAssetLoader::DispatchPending() {
    const size_t budget = EffectiveMaxConcurrent();

    while (m_InFlight < budget && !m_Pending.empty()) {
        // Select the highest-priority queued request, breaking ties by the
        // lowest submission sequence (FIFO). A linear scan is fine: the queue is
        // only the backlog beyond the streaming budget, never the full request
        // set, and asset loads are not issued thousands-per-frame.
        std::vector<std::shared_ptr<Request>>::iterator best = m_Pending.end();
        for (std::vector<std::shared_ptr<Request>>::iterator it = m_Pending.begin();
             it != m_Pending.end(); ++it) {
            const std::shared_ptr<Request>& candidate = *it;
            if (candidate->cancelled) {
                continue;
            }
            if (best == m_Pending.end() ||
                candidate->priority > (*best)->priority ||
                (candidate->priority == (*best)->priority && candidate->sequence < (*best)->sequence)) {
                best = it;
            }
        }

        if (best == m_Pending.end()) {
            // Everything left in the queue was cancelled while waiting; drop it
            // and let Pump() finalize each as Cancelled. The records already had
            // backgroundDone set by Cancel(), so nothing else is required here.
            m_Pending.clear();
            break;
        }

        std::shared_ptr<Request> request = *best;
        m_Pending.erase(best);
        request->dispatched = true;
        ++m_InFlight;
        m_Pool->Enqueue([this, request]() { WorkerRun(request); });
    }
}

void AsyncAssetLoader::WorkerRun(std::shared_ptr<Request> request) {
    bool cancelledAtStart = false;
    {
        platform::LockGuard lock(m_Mutex);
        cancelledAtStart = request->cancelled;
    }

    if (cancelledAtStart) {
        platform::LockGuard lock(m_Mutex);
        request->backgroundSuccess = false;
        request->backgroundDone = true;
        if (m_InFlight > 0) {
            --m_InFlight;
        }
        DispatchPending();
        return;
    }

    bool success = false;
    AssetRef<ArchetypeInstance> instance;
    core::ArchetypeDefinition definition;
    std::string className;

    if (request->kind == AsyncAssetKind::Instance) {
        std::string contents;
        if (ArchetypeInstance::ReadFileContents(request->path, contents)) {
            AssetRef<ArchetypeInstance> parsed(new ArchetypeInstance());
            if (parsed->ParseFromString(request->path, contents)) {
                instance = parsed;
                className = parsed->GetArchetypeClass();
                success = true;
            }
        }
    } else {
        // Resolve res:// (or UUID) to something the registry's file reader can
        // open outside pack mode; in pack mode the resolved path stays pack-visible.
        std::string definitionPath = request->path;
        if (!platform::FileSystem::Exists(definitionPath)) {
            std::string resolved = Asset::ResolveAssetPath(definitionPath);
            if (!resolved.empty()) {
                definitionPath = resolved;
            }
        }
        definition = core::ArchetypeRegistry::GetInstance().ParseDefinitionFile(definitionPath);
        className = definition.className;
        success = definition.isValid;
    }

    {
        platform::LockGuard lock(m_Mutex);
        request->instance = instance;
        request->definition = std::move(definition);
        request->className = std::move(className);
        request->backgroundSuccess = success;
        request->backgroundDone = true;
        if (m_InFlight > 0) {
            --m_InFlight;
        }
        // A streaming slot just freed; pull the next-highest-priority queued
        // request onto the pool. Enqueue takes the pool's own queue lock, never
        // m_Mutex, so issuing it under m_Mutex cannot deadlock.
        DispatchPending();
    }
}

void AsyncAssetLoader::Pump() {
    std::vector<std::shared_ptr<Request>> ready;
    {
        platform::LockGuard lock(m_Mutex);
        for (std::unordered_map<Handle, std::shared_ptr<Request>>::iterator it = m_Requests.begin();
             it != m_Requests.end(); ++it) {
            if (it->second->backgroundDone && !it->second->finalized) {
                ready.push_back(it->second);
            }
        }
    }

    for (std::shared_ptr<Request>& request : ready) {
        // Once backgroundDone is set the worker is finished with this record, and
        // Pump/Cancel/Forget are all main-thread, so the finalize step below runs
        // without the loader mutex (it touches the ArchetypeRegistry, which must
        // not be held under our lock).
        AsyncLoadStatus status;
        if (request->cancelled) {
            status = AsyncLoadStatus::Cancelled;
        } else if (!request->backgroundSuccess) {
            status = AsyncLoadStatus::Failed;
        } else if (request->kind == AsyncAssetKind::Instance) {
            if (request->instance) {
                request->instance->FinalizeResolve();
                status = AsyncLoadStatus::Loaded;
            } else {
                status = AsyncLoadStatus::Failed;
            }
        } else {
            bool registered =
                core::ArchetypeRegistry::GetInstance().RegisterParsedDefinition(request->definition);
            status = registered ? AsyncLoadStatus::Loaded : AsyncLoadStatus::Failed;
        }

        InstanceCallback instanceCallback;
        DefinitionCallback definitionCallback;
        ArchetypeInstance* instancePtr = nullptr;
        std::string className;
        {
            platform::LockGuard lock(m_Mutex);
            request->status = status;
            request->finalized = true;
            instanceCallback = request->instanceCallback;
            definitionCallback = request->definitionCallback;
            instancePtr = request->instance.Get();
            className = request->className;
        }

        if (request->kind == AsyncAssetKind::Instance && instanceCallback) {
            instanceCallback(request->handle,
                             status == AsyncLoadStatus::Loaded ? instancePtr : nullptr);
        } else if (request->kind == AsyncAssetKind::Definition && definitionCallback) {
            definitionCallback(request->handle, status == AsyncLoadStatus::Loaded, className);
        }
    }
}

AsyncLoadStatus AsyncAssetLoader::GetStatus(Handle handle) const {
    platform::LockGuard lock(m_Mutex);
    std::unordered_map<Handle, std::shared_ptr<Request>>::const_iterator it = m_Requests.find(handle);
    if (it == m_Requests.end()) {
        return AsyncLoadStatus::Unknown;
    }
    return it->second->status;
}

bool AsyncAssetLoader::IsComplete(Handle handle) const {
    platform::LockGuard lock(m_Mutex);
    std::unordered_map<Handle, std::shared_ptr<Request>>::const_iterator it = m_Requests.find(handle);
    if (it == m_Requests.end()) {
        return true;
    }
    AsyncLoadStatus status = it->second->status;
    return status == AsyncLoadStatus::Loaded ||
           status == AsyncLoadStatus::Failed ||
           status == AsyncLoadStatus::Cancelled;
}

AssetRef<ArchetypeInstance> AsyncAssetLoader::GetArchetypeInstance(Handle handle) const {
    platform::LockGuard lock(m_Mutex);
    std::unordered_map<Handle, std::shared_ptr<Request>>::const_iterator it = m_Requests.find(handle);
    if (it == m_Requests.end() || it->second->status != AsyncLoadStatus::Loaded) {
        return AssetRef<ArchetypeInstance>();
    }
    return it->second->instance;
}

std::string AsyncAssetLoader::GetClassName(Handle handle) const {
    platform::LockGuard lock(m_Mutex);
    std::unordered_map<Handle, std::shared_ptr<Request>>::const_iterator it = m_Requests.find(handle);
    if (it == m_Requests.end()) {
        return std::string();
    }
    return it->second->className;
}

void AsyncAssetLoader::SetPriority(Handle handle, int priority) {
    platform::LockGuard lock(m_Mutex);
    std::unordered_map<Handle, std::shared_ptr<Request>>::iterator it = m_Requests.find(handle);
    if (it == m_Requests.end()) {
        return;
    }
    // Re-prioritizing only matters while the request is still queued; once
    // dispatched the priority is frozen (it is already running). The next
    // DispatchPending() (this submit/worker-completion) honors the new value.
    if (!it->second->dispatched && !it->second->finalized) {
        it->second->priority = priority;
    }
}

int AsyncAssetLoader::GetPriority(Handle handle) const {
    platform::LockGuard lock(m_Mutex);
    std::unordered_map<Handle, std::shared_ptr<Request>>::const_iterator it = m_Requests.find(handle);
    if (it == m_Requests.end()) {
        return 0;
    }
    return it->second->priority;
}

void AsyncAssetLoader::SetMaxConcurrentLoads(size_t maxConcurrent) {
    platform::LockGuard lock(m_Mutex);
    m_MaxConcurrent = maxConcurrent;
    // Raising the budget may free slots for queued work; dispatch immediately.
    DispatchPending();
}

size_t AsyncAssetLoader::GetMaxConcurrentLoads() const {
    platform::LockGuard lock(m_Mutex);
    return EffectiveMaxConcurrent();
}

size_t AsyncAssetLoader::GetInFlightCount() const {
    platform::LockGuard lock(m_Mutex);
    return m_InFlight;
}

size_t AsyncAssetLoader::GetQueuedCount() const {
    platform::LockGuard lock(m_Mutex);
    size_t count = 0;
    for (const std::shared_ptr<Request>& request : m_Pending) {
        if (!request->cancelled) {
            ++count;
        }
    }
    return count;
}

void AsyncAssetLoader::Cancel(Handle handle) {
    platform::LockGuard lock(m_Mutex);
    std::unordered_map<Handle, std::shared_ptr<Request>>::iterator it = m_Requests.find(handle);
    if (it == m_Requests.end()) {
        return;
    }
    std::shared_ptr<Request> request = it->second;
    if (request->finalized) {
        return;
    }
    request->cancelled = true;
    if (!request->dispatched) {
        // Still queued: no worker will ever run it, so mark it done here and pull
        // it from the queue. Pump() then finalizes it as Cancelled.
        request->backgroundSuccess = false;
        request->backgroundDone = true;
        RemoveFromPending(handle);
    }
}

void AsyncAssetLoader::Forget(Handle handle) {
    platform::LockGuard lock(m_Mutex);
    std::unordered_map<Handle, std::shared_ptr<Request>>::iterator it = m_Requests.find(handle);
    if (it == m_Requests.end()) {
        return;
    }
    std::shared_ptr<Request> request = it->second;
    if (!request->finalized) {
        request->cancelled = true;
    }
    if (!request->dispatched) {
        RemoveFromPending(handle);
    }
    m_Requests.erase(it);
}

void AsyncAssetLoader::RemoveFromPending(Handle handle) {
    for (std::vector<std::shared_ptr<Request>>::iterator it = m_Pending.begin();
         it != m_Pending.end(); ++it) {
        if ((*it)->handle == handle) {
            m_Pending.erase(it);
            return;
        }
    }
}

void AsyncAssetLoader::Reset() {
    {
        platform::LockGuard lock(m_Mutex);
        for (std::unordered_map<Handle, std::shared_ptr<Request>>::iterator it = m_Requests.begin();
             it != m_Requests.end(); ++it) {
            it->second->cancelled = true;
        }
        // Drop the queue first so worker-completion DispatchPending() during the
        // drain below finds nothing to start.
        m_Pending.clear();
    }

    if (m_Pool) {
        m_Pool->WaitForCompletion();
    }

    platform::LockGuard lock(m_Mutex);
    m_Requests.clear();
    m_InFlight = 0;
}

size_t AsyncAssetLoader::GetPendingCount() const {
    platform::LockGuard lock(m_Mutex);
    size_t count = 0;
    for (std::unordered_map<Handle, std::shared_ptr<Request>>::const_iterator it = m_Requests.begin();
         it != m_Requests.end(); ++it) {
        if (!it->second->finalized) {
            ++count;
        }
    }
    return count;
}

} // namespace asset
} // namespace lupine
