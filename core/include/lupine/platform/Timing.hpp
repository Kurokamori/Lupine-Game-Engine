#pragma once

#include "PlatformDetection.hpp"
#include <cstdint>
#include <chrono>

namespace lupine {
namespace platform {

/**
 * High-resolution timing utilities for profiling and time measurement.
 * Uses std::chrono for cross-platform compatibility.
 */
class Timing {
public:
    // Type aliases for convenience
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;
    using Duration = Clock::duration;

    /**
     * Gets the current high-resolution time point.
     * This is monotonic and suitable for profiling.
     * @return Current time point
     */
    static TimePoint Now();

    /**
     * Gets the time elapsed between two time points in seconds.
     * @param start Start time point
     * @param end End time point
     * @return Elapsed time in seconds
     */
    static double GetElapsedSeconds(const TimePoint& start, const TimePoint& end);

    /**
     * Gets the time elapsed between two time points in milliseconds.
     * @param start Start time point
     * @param end End time point
     * @return Elapsed time in milliseconds
     */
    static double GetElapsedMilliseconds(const TimePoint& start, const TimePoint& end);

    /**
     * Gets the time elapsed between two time points in microseconds.
     * @param start Start time point
     * @param end End time point
     * @return Elapsed time in microseconds
     */
    static int64_t GetElapsedMicroseconds(const TimePoint& start, const TimePoint& end);

    /**
     * Gets the time elapsed between two time points in nanoseconds.
     * @param start Start time point
     * @param end End time point
     * @return Elapsed time in nanoseconds
     */
    static int64_t GetElapsedNanoseconds(const TimePoint& start, const TimePoint& end);

    /**
     * Sleeps for the specified number of milliseconds.
     * @param milliseconds Number of milliseconds to sleep
     */
    static void SleepMilliseconds(int64_t milliseconds);

    /**
     * Sleeps for the specified number of microseconds.
     * @param microseconds Number of microseconds to sleep
     */
    static void SleepMicroseconds(int64_t microseconds);
};

/**
 * Simple RAII timer for profiling code sections.
 * Usage:
 *   {
 *       ScopedTimer timer("MyFunction");
 *       // Code to profile
 *   } // Automatically logs elapsed time on destruction
 */
class ScopedTimer {
public:
    /**
     * Constructs a scoped timer and starts timing.
     * @param name Name of the timed section (for logging)
     * @param logOnDestroy If true, automatically logs elapsed time on destruction
     */
    explicit ScopedTimer(const char* name, bool logOnDestroy = true);

    /**
     * Destructor - logs elapsed time if enabled.
     */
    ~ScopedTimer();

    /**
     * Gets the elapsed time since construction in seconds.
     * @return Elapsed time in seconds
     */
    double GetElapsedSeconds() const;

    /**
     * Gets the elapsed time since construction in milliseconds.
     * @return Elapsed time in milliseconds
     */
    double GetElapsedMilliseconds() const;

    /**
     * Resets the timer to the current time.
     */
    void Reset();

private:
    const char* m_Name;
    Timing::TimePoint m_StartTime;
    bool m_LogOnDestroy;
};

/**
 * Simple stopwatch for manual timing control.
 */
class Stopwatch {
public:
    /**
     * Constructs a stopwatch in stopped state.
     */
    Stopwatch();

    /**
     * Starts or resumes the stopwatch.
     */
    void Start();

    /**
     * Stops the stopwatch.
     */
    void Stop();

    /**
     * Resets the stopwatch to zero and stops it.
     */
    void Reset();

    /**
     * Checks if the stopwatch is currently running.
     * @return True if running, false otherwise
     */
    bool IsRunning() const;

    /**
     * Gets the elapsed time in seconds (including accumulated time from previous runs).
     * @return Elapsed time in seconds
     */
    double GetElapsedSeconds() const;

    /**
     * Gets the elapsed time in milliseconds (including accumulated time from previous runs).
     * @return Elapsed time in milliseconds
     */
    double GetElapsedMilliseconds() const;

private:
    Timing::TimePoint m_StartTime;
    Timing::Duration m_AccumulatedTime;
    bool m_IsRunning;
};

} // namespace platform
} // namespace lupine
