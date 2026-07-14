#include "lupine/platform/Timing.hpp"
#include "lupine/logger/Logger.hpp"
#include <thread>

namespace lupine {
namespace platform {

Timing::TimePoint Timing::Now() {
    return Clock::now();
}

double Timing::GetElapsedSeconds(const TimePoint& start, const TimePoint& end) {
    auto duration = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
    return duration.count();
}

double Timing::GetElapsedMilliseconds(const TimePoint& start, const TimePoint& end) {
    auto duration = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(end - start);
    return duration.count();
}

int64_t Timing::GetElapsedMicroseconds(const TimePoint& start, const TimePoint& end) {
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    return duration.count();
}

int64_t Timing::GetElapsedNanoseconds(const TimePoint& start, const TimePoint& end) {
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    return duration.count();
}

void Timing::SleepMilliseconds(int64_t milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

void Timing::SleepMicroseconds(int64_t microseconds) {
    std::this_thread::sleep_for(std::chrono::microseconds(microseconds));
}

ScopedTimer::ScopedTimer(const char* name, bool logOnDestroy)
    : m_Name(name), m_StartTime(Timing::Now()), m_LogOnDestroy(logOnDestroy) {
}

ScopedTimer::~ScopedTimer() {
    if (m_LogOnDestroy) {
        double elapsed = GetElapsedMilliseconds();
        LOG_DEBUG(LogCategory::Core, "[Timing] {} took {:.3f} ms", m_Name, elapsed);
    }
}

double ScopedTimer::GetElapsedSeconds() const {
    return Timing::GetElapsedSeconds(m_StartTime, Timing::Now());
}

double ScopedTimer::GetElapsedMilliseconds() const {
    return Timing::GetElapsedMilliseconds(m_StartTime, Timing::Now());
}

void ScopedTimer::Reset() {
    m_StartTime = Timing::Now();
}

Stopwatch::Stopwatch()
    : m_StartTime(Timing::Now()), m_AccumulatedTime(Timing::Duration::zero()), m_IsRunning(false) {
}

void Stopwatch::Start() {
    if (!m_IsRunning) {
        m_StartTime = Timing::Now();
        m_IsRunning = true;
    }
}

void Stopwatch::Stop() {
    if (m_IsRunning) {
        auto now = Timing::Now();
        m_AccumulatedTime += (now - m_StartTime);
        m_IsRunning = false;
    }
}

void Stopwatch::Reset() {
    m_AccumulatedTime = Timing::Duration::zero();
    m_StartTime = Timing::Now();
    m_IsRunning = false;
}

bool Stopwatch::IsRunning() const {
    return m_IsRunning;
}

double Stopwatch::GetElapsedSeconds() const {
    auto totalTime = m_AccumulatedTime;
    if (m_IsRunning) {
        totalTime += (Timing::Now() - m_StartTime);
    }
    return std::chrono::duration_cast<std::chrono::duration<double>>(totalTime).count();
}

double Stopwatch::GetElapsedMilliseconds() const {
    auto totalTime = m_AccumulatedTime;
    if (m_IsRunning) {
        totalTime += (Timing::Now() - m_StartTime);
    }
    return std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(totalTime).count();
}

}
}
