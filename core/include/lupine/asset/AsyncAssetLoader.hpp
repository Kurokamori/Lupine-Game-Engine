#pragma once

#include "lupine/asset/Asset.hpp"
#include "lupine/platform/Threading.hpp"
#include <cstdint>
#include <string>
#include <functional>
#include <unordered_map>
#include <vector>
#include <memory>
#include <atomic>

namespace lupine {
namespace asset {

class ArchetypeInstance;

/**
 * Status of an asynchronous load request.
 */
enum class AsyncLoadStatus {
    Unknown,    // No such handle (never issued, or already forgotten)
    Pending,    // Queued or loading on a worker thread
    Loaded,     // Completed successfully; the result is available
    Failed,     // Completed with an error (read/parse/registration failure)
    Cancelled   // Cancelled before completion
};

/**
 * Standard priority bands for streamed loads. A request's priority is a plain
 * int (higher = more urgent); these are convenience values. The loader services
 * the highest-priority queued request first, breaking ties by submission order
 * (FIFO). Priority only governs which queued request is dispatched next once a
 * streaming slot frees up; a request that has already started on a worker thread
 * is never preempted.
 */
enum class AsyncLoadPriority : int {
    Low      = -100,
    Normal   = 0,
    High     = 100,
    Critical = 1000
};

/**
 * AsyncAssetLoader - background loading of archetype assets.
 *
 * Loads archetype instances (.ares) and archetype definitions
 * (.archetype/.lua/.rb/.py) off the main thread, then finalizes the
 * registry-dependent work on the main thread when Pump() is called.
 *
 * Threading model:
 *  - A request enqueues a job onto an internal thread pool. The worker performs
 *    only the self-contained, thread-safe work: reading the file and parsing it
 *    (for definitions, also the expensive directive/export regex parsing).
 *  - The registry-touching step (resolving an instance's field defaults, or
 *    registering a parsed definition) is deferred to Pump(), which the engine
 *    calls once per frame on the main thread. This keeps the ArchetypeRegistry
 *    single-threaded while moving all file I/O and parsing off the main thread.
 *
 * Priority streaming:
 *  - Submitting a request does not necessarily hand it to a worker immediately.
 *    The loader keeps an internal queue of not-yet-dispatched requests and only
 *    runs up to a configurable number concurrently (the streaming budget,
 *    SetMaxConcurrentLoads). When a worker finishes it pulls the next request,
 *    always choosing the highest priority (ties broken FIFO by submission
 *    order). A high-priority request submitted while many lower-priority ones
 *    are still queued therefore jumps ahead of them. SetPriority() re-orders a
 *    request that has not started yet. This bounds the I/O/parse load a burst of
 *    requests can put on the machine and lets gameplay-critical assets stream in
 *    ahead of speculative/background ones.
 *
 * Requests are referenced by an opaque handle. The loader retains each request's
 * record (status + result) until the owner calls Forget(), or until Reset()
 * clears everything on project unload. All public methods are main-thread only
 * except where noted; the worker side is internally synchronized.
 */
class AsyncAssetLoader {
public:
    using Handle = uint64_t;

    // Invoked on the main thread from Pump() when an instance request completes.
    // The instance pointer is the loaded asset, or nullptr on failure/cancel.
    // The loader retains a reference, so the pointer stays valid until Forget().
    using InstanceCallback = std::function<void(Handle, ArchetypeInstance*)>;

    // Invoked on the main thread from Pump() when a definition request completes.
    // success is whether the definition parsed and registered; className is the
    // archetype class name (empty on failure).
    using DefinitionCallback = std::function<void(Handle, bool /*success*/, const std::string& /*className*/)>;

    static AsyncAssetLoader& GetInstance();

    /**
     * Begin loading an archetype instance (.ares) by res:// path.
     * @param resPath  res:// (or physical) path to the .ares file.
     * @param callback Optional completion callback (main thread, from Pump()).
     * @param priority Streaming priority (higher = serviced first; see
     *                 AsyncLoadPriority). Governs queue order while the request
     *                 waits for a streaming slot.
     * @return A handle (> 0) for status/result queries, or 0 if resPath is empty.
     */
    Handle LoadArchetypeInstanceAsync(const std::string& resPath, InstanceCallback callback = nullptr,
                                      int priority = static_cast<int>(AsyncLoadPriority::Normal));

    /**
     * Begin loading and registering an archetype definition file.
     * @param filePath Path to the .archetype/.lua/.rb/.py definition source.
     * @param callback Optional completion callback (main thread, from Pump()).
     * @param priority Streaming priority (higher = serviced first).
     * @return A handle (> 0), or 0 if filePath is empty.
     */
    Handle LoadArchetypeDefinitionAsync(const std::string& filePath, DefinitionCallback callback = nullptr,
                                        int priority = static_cast<int>(AsyncLoadPriority::Normal));

    /**
     * Main-thread per-frame pump. Finalizes background work that has completed
     * (resolve instances / register definitions) and fires completion callbacks.
     * Cheap when there is nothing to finalize.
     */
    void Pump();

    /**
     * Query the status of a request.
     */
    AsyncLoadStatus GetStatus(Handle handle) const;

    /**
     * True once a request has finished (Loaded, Failed, or Cancelled). Also true
     * for an Unknown handle so that pollers never spin forever on a stale handle.
     */
    bool IsComplete(Handle handle) const;

    /**
     * Retrieve the loaded instance for a completed request. Returns an invalid
     * ref unless the status is Loaded. The loader keeps its own reference until
     * Forget(); the returned ref is an independent owning reference.
     */
    AssetRef<ArchetypeInstance> GetArchetypeInstance(Handle handle) const;

    /**
     * The archetype class name parsed for a request (instance or definition).
     * Empty until the request has been finalized, or if it failed.
     */
    std::string GetClassName(Handle handle) const;

    /**
     * Re-prioritize a queued request. Only affects a request that has not yet
     * been dispatched to a worker (its dispatch order changes); a no-op once the
     * request has started loading, completed, or is unknown.
     */
    void SetPriority(Handle handle, int priority);

    /**
     * The priority a request was submitted/last set with. Returns 0 for an
     * unknown handle.
     */
    int GetPriority(Handle handle) const;

    /**
     * Set the streaming budget: the maximum number of requests that may be
     * loading on worker threads at the same time. Pass 0 to track the thread
     * pool size automatically (the default). Lowering it lets queued lower-
     * priority work back off so a spike of requests does not saturate I/O;
     * raising it immediately dispatches any requests that now fit.
     */
    void SetMaxConcurrentLoads(size_t maxConcurrent);

    /**
     * The effective streaming budget (resolves the 0 = auto default to the
     * current worker thread count).
     */
    size_t GetMaxConcurrentLoads() const;

    /** Number of requests currently loading on a worker thread. */
    size_t GetInFlightCount() const;

    /** Number of requests queued but not yet dispatched to a worker. */
    size_t GetQueuedCount() const;

    /**
     * Cancel a request. If it is still pending, its eventual result is discarded;
     * if it has already completed this is a no-op. The record remains queryable
     * (status Cancelled) until Forget().
     */
    void Cancel(Handle handle);

    /**
     * Drop the retained record for a handle, releasing any held asset reference.
     * A still-pending handle is cancelled first. After this GetStatus is Unknown.
     */
    void Forget(Handle handle);

    /**
     * Cancel everything and clear all records. Blocks until in-flight worker jobs
     * have drained, so that no background job touches engine state after this
     * returns. Call on project unload/reload.
     */
    void Reset();

    /**
     * Number of requests that have not yet been finalized (diagnostics/tests).
     */
    size_t GetPendingCount() const;

private:
    AsyncAssetLoader() = default;
    ~AsyncAssetLoader();
    AsyncAssetLoader(const AsyncAssetLoader&) = delete;
    AsyncAssetLoader& operator=(const AsyncAssetLoader&) = delete;

    struct Request;

    void EnsurePool();
    void WorkerRun(std::shared_ptr<Request> request);

    // Dispatch as many queued requests to the pool as the streaming budget
    // allows, in priority order. Must be called with m_Mutex held.
    void DispatchPending();

    // Effective streaming budget (resolves the auto/0 default). Must be called
    // with m_Mutex held.
    size_t EffectiveMaxConcurrent() const;

    // Erase a request from the pending queue by handle (no-op if absent). Must
    // be called with m_Mutex held.
    void RemoveFromPending(Handle handle);

    mutable platform::Mutex m_Mutex;
    std::unique_ptr<platform::ThreadPool> m_Pool;
    std::unordered_map<Handle, std::shared_ptr<Request>> m_Requests;

    // Requests submitted but not yet handed to a worker, in arbitrary order;
    // DispatchPending() selects the highest-priority entry each time a slot
    // frees up. Entries are removed when dispatched or cancelled.
    std::vector<std::shared_ptr<Request>> m_Pending;

    std::atomic<Handle> m_NextHandle{1};

    // Monotonic submission counter used as the FIFO tie-break for equal-priority
    // queued requests.
    uint64_t m_NextSequence = 1;

    // Number of requests currently executing on a worker thread.
    size_t m_InFlight = 0;

    // Streaming budget; 0 means "track the worker thread count" (the default).
    size_t m_MaxConcurrent = 0;
};

} // namespace asset
} // namespace lupine
