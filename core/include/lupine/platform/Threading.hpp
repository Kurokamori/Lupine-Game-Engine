#pragma once

#include "PlatformDetection.hpp"
#include <mutex>
#include <condition_variable>
#include <thread>
#include <functional>
#include <memory>
#include <atomic>
#include <string>

// Undefine Windows macros that conflict with our code
// This must be done BEFORE the class declarations to avoid conflicts
#if defined(LUPINE_PLATFORM_WINDOWS)
    #ifdef Yield
        #undef Yield
    #endif
#endif

namespace lupine {
namespace platform {

/**
 * Cross-platform mutex wrapper.
 * Provides a simple interface for mutual exclusion.
 */
class Mutex {
public:
    Mutex() = default;
    ~Mutex() = default;

    // Non-copyable
    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;

    /**
     * Locks the mutex, blocking if necessary.
     */
    void Lock();

    /**
     * Tries to lock the mutex without blocking.
     * @return True if lock was acquired, false otherwise
     */
    bool TryLock();

    /**
     * Unlocks the mutex.
     */
    void Unlock();

    /**
     * Gets the underlying std::mutex.
     * @return Reference to the underlying mutex
     */
    std::mutex& GetNativeMutex();

private:
    std::mutex m_Mutex;
};

/**
 * RAII lock guard for automatic mutex management.
 * Locks the mutex on construction and unlocks on destruction.
 */
class LockGuard {
public:
    /**
     * Constructs a lock guard and locks the mutex.
     * @param mutex Mutex to lock
     */
    explicit LockGuard(Mutex& mutex);

    /**
     * Destructor - unlocks the mutex.
     */
    ~LockGuard();

    // Non-copyable and non-movable
    LockGuard(const LockGuard&) = delete;
    LockGuard& operator=(const LockGuard&) = delete;
    LockGuard(LockGuard&&) = delete;
    LockGuard& operator=(LockGuard&&) = delete;

private:
    Mutex& m_Mutex;
};

/**
 * Cross-platform condition variable wrapper.
 * Used for thread synchronization and signaling.
 */
class ConditionVariable {
public:
    ConditionVariable() = default;
    ~ConditionVariable() = default;

    // Non-copyable
    ConditionVariable(const ConditionVariable&) = delete;
    ConditionVariable& operator=(const ConditionVariable&) = delete;

    /**
     * Waits on the condition variable.
     * The mutex must be locked before calling this.
     * @param mutex Mutex to use for synchronization
     */
    void Wait(Mutex& mutex);

    /**
     * Waits on the condition variable with a predicate.
     * @param mutex Mutex to use for synchronization
     * @param predicate Predicate function to check condition
     */
    template<typename Predicate>
    void Wait(Mutex& mutex, Predicate predicate) {
        std::unique_lock<std::mutex> lock(mutex.GetNativeMutex(), std::adopt_lock);
        m_CondVar.wait(lock, predicate);
        lock.release(); // Release ownership without unlocking
    }

    /**
     * Waits on the condition variable with a timeout.
     * @param mutex Mutex to use for synchronization
     * @param timeoutMs Timeout in milliseconds
     * @return True if signaled, false if timed out
     */
    bool WaitFor(Mutex& mutex, int64_t timeoutMs);

    /**
     * Waits on the condition variable with a timeout and predicate.
     * @param mutex Mutex to use for synchronization
     * @param timeoutMs Timeout in milliseconds
     * @param predicate Predicate function to check condition
     * @return True if condition met, false if timed out
     */
    template<typename Predicate>
    bool WaitFor(Mutex& mutex, int64_t timeoutMs, Predicate predicate) {
        std::unique_lock<std::mutex> lock(mutex.GetNativeMutex(), std::adopt_lock);
        bool result = m_CondVar.wait_for(lock, std::chrono::milliseconds(timeoutMs), predicate);
        lock.release(); // Release ownership without unlocking
        return result;
    }

    /**
     * Signals one waiting thread.
     */
    void NotifyOne();

    /**
     * Signals all waiting threads.
     */
    void NotifyAll();

private:
    std::condition_variable m_CondVar;
};

/**
 * Cross-platform thread wrapper.
 * Provides a simple interface for creating and managing threads.
 */
class Thread {
public:
    /**
     * Constructs a thread without starting it.
     */
    Thread();

    /**
     * Constructs a thread and starts it with the given function.
     * @param func Function to run in the thread
     */
    explicit Thread(std::function<void()> func);

    /**
     * Destructor - joins the thread if joinable.
     */
    ~Thread();

    // Non-copyable but movable
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;
    Thread(Thread&& other) noexcept;
    Thread& operator=(Thread&& other) noexcept;

    /**
     * Starts the thread with the given function.
     * @param func Function to run in the thread
     */
    void Start(std::function<void()> func);

    /**
     * Waits for the thread to finish.
     */
    void Join();

    /**
     * Detaches the thread, allowing it to run independently.
     */
    void Detach();

    /**
     * Checks if the thread is joinable.
     * @return True if joinable, false otherwise
     */
    bool IsJoinable() const;

    /**
     * Gets the thread ID.
     * @return Thread ID
     */
    std::thread::id GetId() const;

    /**
     * Gets the number of hardware threads available.
     * @return Number of hardware threads
     */
    static unsigned int GetHardwareConcurrency();

    /**
     * Gets the current thread ID.
     * @return Current thread ID
     */
    static std::thread::id GetCurrentThreadId();

    /**
     * Yields the current thread, allowing other threads to run.
     */
    static void Yield();

    /**
     * Sleeps the current thread for the specified duration.
     * @param milliseconds Duration in milliseconds
     */
    static void Sleep(int64_t milliseconds);

private:
    std::thread m_Thread;
};

/**
 * Thread-safe counter using atomic operations.
 */
class AtomicCounter {
public:
    /**
     * Constructs an atomic counter with an initial value.
     * @param initialValue Initial value (default: 0)
     */
    explicit AtomicCounter(int32_t initialValue = 0);

    /**
     * Increments the counter and returns the new value.
     * @return New value after increment
     */
    int32_t Increment();

    /**
     * Decrements the counter and returns the new value.
     * @return New value after decrement
     */
    int32_t Decrement();

    /**
     * Gets the current value.
     * @return Current value
     */
    int32_t Get() const;

    /**
     * Sets a new value.
     * @param value New value
     */
    void Set(int32_t value);

    /**
     * Adds a value to the counter and returns the new value.
     * @param value Value to add
     * @return New value after addition
     */
    int32_t Add(int32_t value);

    /**
     * Compares the current value with expected and sets new value if equal.
     * @param expected Expected value
     * @param desired Desired new value
     * @return True if exchange was performed, false otherwise
     */
    bool CompareExchange(int32_t& expected, int32_t desired);

private:
    std::atomic<int32_t> m_Value;
};

/**
 * Simple thread pool for executing tasks concurrently.
 */
class ThreadPool {
public:
    /**
     * Constructs a thread pool with the specified number of threads.
     * @param numThreads Number of threads (default: hardware concurrency)
     */
    explicit ThreadPool(size_t numThreads = 0);

    /**
     * Destructor - waits for all tasks to complete and stops threads.
     */
    ~ThreadPool();

    // Non-copyable and non-movable
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    /**
     * Enqueues a task to be executed by the thread pool.
     * @param task Task function to execute
     */
    void Enqueue(std::function<void()> task);

    /**
     * Waits for all tasks to complete.
     */
    void WaitForCompletion();

    /**
     * Gets the number of threads in the pool.
     * @return Number of threads
     */
    size_t GetThreadCount() const;

    /**
     * Gets the number of pending tasks.
     * @return Number of pending tasks
     */
    size_t GetPendingTaskCount() const;

private:
    /**
     * Worker thread function.
     * @param index Thread index
     */
    void WorkerThread(size_t index);

    std::vector<Thread> m_Threads;
    std::vector<std::function<void()>> m_Tasks;
    Mutex m_QueueMutex;
    ConditionVariable m_Condition;
    std::atomic<bool> m_ShouldStop;
    std::atomic<size_t> m_ActiveTasks;
};

/**
 * Read-Write lock for scenarios with many readers and few writers.
 */
class ReadWriteLock {
public:
    ReadWriteLock();
    ~ReadWriteLock() = default;

    // Non-copyable
    ReadWriteLock(const ReadWriteLock&) = delete;
    ReadWriteLock& operator=(const ReadWriteLock&) = delete;

    /**
     * Acquires a read lock (shared access).
     * Multiple threads can hold read locks simultaneously.
     */
    void LockRead();

    /**
     * Releases a read lock.
     */
    void UnlockRead();

    /**
     * Acquires a write lock (exclusive access).
     * Only one thread can hold a write lock.
     */
    void LockWrite();

    /**
     * Releases a write lock.
     */
    void UnlockWrite();

private:
    Mutex m_Mutex;
    ConditionVariable m_ReadCondition;
    ConditionVariable m_WriteCondition;
    std::atomic<int32_t> m_ReaderCount;
    std::atomic<bool> m_WriterActive;
    std::atomic<int32_t> m_WaitingWriters;
};

/**
 * RAII read lock guard.
 */
class ReadLockGuard {
public:
    explicit ReadLockGuard(ReadWriteLock& lock);
    ~ReadLockGuard();

    // Non-copyable and non-movable
    ReadLockGuard(const ReadLockGuard&) = delete;
    ReadLockGuard& operator=(const ReadLockGuard&) = delete;

private:
    ReadWriteLock& m_Lock;
};

/**
 * RAII write lock guard.
 */
class WriteLockGuard {
public:
    explicit WriteLockGuard(ReadWriteLock& lock);
    ~WriteLockGuard();

    // Non-copyable and non-movable
    WriteLockGuard(const WriteLockGuard&) = delete;
    WriteLockGuard& operator=(const WriteLockGuard&) = delete;

private:
    ReadWriteLock& m_Lock;
};

} // namespace platform
} // namespace lupine
