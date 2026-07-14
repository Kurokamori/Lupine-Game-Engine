#include "lupine/platform/Threading.hpp"
#include "lupine/logger/Logger.hpp"

#if defined(LUPINE_PLATFORM_WINDOWS)
    #ifdef Yield
        #undef Yield
    #endif
#endif

namespace lupine {
namespace platform {

void Mutex::Lock() {
    m_Mutex.lock();
}

bool Mutex::TryLock() {
    return m_Mutex.try_lock();
}

void Mutex::Unlock() {
    m_Mutex.unlock();
}

std::mutex& Mutex::GetNativeMutex() {
    return m_Mutex;
}

LockGuard::LockGuard(Mutex& mutex) : m_Mutex(mutex) {
    m_Mutex.Lock();
}

LockGuard::~LockGuard() {
    m_Mutex.Unlock();
}

void ConditionVariable::NotifyOne() {
    m_CondVar.notify_one();
}

void ConditionVariable::NotifyAll() {
    m_CondVar.notify_all();
}

Thread::Thread() = default;

Thread::Thread(std::function<void()> func) {
    Start(func);
}

Thread::~Thread() {
    if (m_Thread.joinable()) {

        m_Thread.join();
    }
}

Thread::Thread(Thread&& other) noexcept : m_Thread(std::move(other.m_Thread)) {
}

Thread& Thread::operator=(Thread&& other) noexcept {
    if (this != &other) {
        if (m_Thread.joinable()) {
            m_Thread.join();
        }
        m_Thread = std::move(other.m_Thread);
    }
    return *this;
}

void Thread::Start(std::function<void()> func) {
    if (m_Thread.joinable()) {

        return;
    }
    m_Thread = std::thread(func);
}

void Thread::Join() {
    if (m_Thread.joinable()) {
        m_Thread.join();
    }
}

void Thread::Detach() {
    if (m_Thread.joinable()) {
        m_Thread.detach();
    }
}

bool Thread::IsJoinable() const {
    return m_Thread.joinable();
}

std::thread::id Thread::GetId() const {
    return m_Thread.get_id();
}

unsigned int Thread::GetHardwareConcurrency() {
    return std::thread::hardware_concurrency();
}

std::thread::id Thread::GetCurrentThreadId() {
    return std::this_thread::get_id();
}

void Thread::Yield() {
    std::this_thread::yield();
}

void Thread::Sleep(int64_t milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

AtomicCounter::AtomicCounter(int32_t initialValue) : m_Value(initialValue) {
}

int32_t AtomicCounter::Increment() {
    return ++m_Value;
}

int32_t AtomicCounter::Decrement() {
    return --m_Value;
}

int32_t AtomicCounter::Get() const {
    return m_Value.load();
}

void AtomicCounter::Set(int32_t value) {
    m_Value.store(value);
}

int32_t AtomicCounter::Add(int32_t value) {
    return m_Value.fetch_add(value) + value;
}

bool AtomicCounter::CompareExchange(int32_t& expected, int32_t desired) {
    return m_Value.compare_exchange_strong(expected, desired);
}

ThreadPool::ThreadPool(size_t numThreads) : m_ShouldStop(false), m_ActiveTasks(0) {
    if (numThreads == 0) {
        numThreads = Thread::GetHardwareConcurrency();
        if (numThreads == 0) {
            numThreads = 4;
        }
    }

    m_Threads.reserve(numThreads);
    for (size_t i = 0; i < numThreads; ++i) {
        m_Threads.emplace_back([this, i]() { WorkerThread(i); });
    }
}

ThreadPool::~ThreadPool() {

    {
        LockGuard lock(m_QueueMutex);
        m_ShouldStop = true;
    }

    m_Condition.NotifyAll();

    for (auto& thread : m_Threads) {
        if (thread.IsJoinable()) {
            thread.Join();
        }
    }

}

void ThreadPool::Enqueue(std::function<void()> task) {
    {
        LockGuard lock(m_QueueMutex);
        m_Tasks.push_back(std::move(task));
    }
    m_Condition.NotifyOne();
}

void ThreadPool::WaitForCompletion() {
    while (true) {
        {
            LockGuard lock(m_QueueMutex);
            if (m_Tasks.empty() && m_ActiveTasks == 0) {
                break;
            }
        }
        Thread::Sleep(1);
    }
}

size_t ThreadPool::GetThreadCount() const {
    return m_Threads.size();
}

size_t ThreadPool::GetPendingTaskCount() const {
    LockGuard lock(const_cast<Mutex&>(m_QueueMutex));
    return m_Tasks.size();
}

void ThreadPool::WorkerThread(size_t index) {

    while (true) {
        std::function<void()> task;

        {
            LockGuard lock(m_QueueMutex);
            m_Condition.Wait(m_QueueMutex, [this]() {
                return m_ShouldStop || !m_Tasks.empty();
            });

            if (m_ShouldStop && m_Tasks.empty()) {
                break;
            }

            if (!m_Tasks.empty()) {
                task = std::move(m_Tasks.front());
                m_Tasks.erase(m_Tasks.begin());
                ++m_ActiveTasks;
            }
        }

        if (task) {
            try {
                task();
            } catch (const std::exception& e) {
                LOG_ERROR(LogCategory::Core, "ThreadPool: worker {} task threw: {}", index, e.what());
            } catch (...) {
                LOG_ERROR(LogCategory::Core, "ThreadPool: worker {} task threw an unknown exception", index);
            }
            --m_ActiveTasks;
        }
    }

}

ReadWriteLock::ReadWriteLock() : m_ReaderCount(0), m_WriterActive(false), m_WaitingWriters(0) {
}

void ReadWriteLock::LockRead() {
    LockGuard lock(m_Mutex);

    m_ReadCondition.Wait(m_Mutex, [this]() -> bool {
        return !m_WriterActive.load() && m_WaitingWriters.load() == 0;
    });

    ++m_ReaderCount;
}

void ReadWriteLock::UnlockRead() {
    LockGuard lock(m_Mutex);
    --m_ReaderCount;

    if (m_ReaderCount == 0) {
        m_WriteCondition.NotifyOne();
    }
}

void ReadWriteLock::LockWrite() {
    LockGuard lock(m_Mutex);
    ++m_WaitingWriters;

    m_WriteCondition.Wait(m_Mutex, [this]() -> bool {
        return m_ReaderCount.load() == 0 && !m_WriterActive.load();
    });

    --m_WaitingWriters;
    m_WriterActive = true;
}

void ReadWriteLock::UnlockWrite() {
    LockGuard lock(m_Mutex);
    m_WriterActive = false;

    if (m_WaitingWriters > 0) {
        m_WriteCondition.NotifyOne();
    } else {
        m_ReadCondition.NotifyAll();
    }
}

ReadLockGuard::ReadLockGuard(ReadWriteLock& lock) : m_Lock(lock) {
    m_Lock.LockRead();
}

ReadLockGuard::~ReadLockGuard() {
    m_Lock.UnlockRead();
}

WriteLockGuard::WriteLockGuard(ReadWriteLock& lock) : m_Lock(lock) {
    m_Lock.LockWrite();
}

WriteLockGuard::~WriteLockGuard() {
    m_Lock.UnlockWrite();
}

}
}
