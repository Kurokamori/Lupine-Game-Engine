#include "lupine/components/Timer.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/logger/Logger.hpp"

namespace lupine {
namespace components {

using namespace core;

Timer::Timer()
    : Component("Timer")
    , m_TimeoutCallback(nullptr)
    , m_HasTriggered(false)
    , m_FireCount(0)
{
}

Timer::Timer(const std::string& name)
    : Component(name)
    , m_TimeoutCallback(nullptr)
    , m_HasTriggered(false)
    , m_FireCount(0)
{
}

Timer::~Timer() {

}

void Timer::DefineProperties() {

    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(duration, 1.0f, 0.0f, 3600.0f, 0.1f, "Timer"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(elapsed, 0.0f, 0.0f, 3600.0f, 0.1f, "Timer"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(loop, Bool, false, "Timer"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(repeatCount, Int, -1, "Timer"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(autoStart, Bool, false, "Timer"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(running, Bool, false, "Timer"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(ignoreTimeScale, Bool, false, "Timer"));
}

void Timer::OnAwake() {

    if (GetAutoStart()) {
        Start();
    }
}

void Timer::OnUpdate(float deltaTime) {

    if (!IsRunning()) {
        return;
    }

    float dt = deltaTime;

    float currentElapsed = GetElapsed();
    currentElapsed += dt;
    SetElapsed(currentElapsed);

    float duration = GetDuration();
    if (currentElapsed >= duration) {

        if (!m_HasTriggered) {
            TriggerTimeout();
            m_FireCount++;
            m_HasTriggered = true;
        }

        int repeatCount = GetRepeatCount();
        bool repeatsRemaining = (repeatCount < 0) || (m_FireCount < repeatCount);
        if (GetLoop() && repeatsRemaining) {

            float overshoot = currentElapsed - duration;
            SetElapsed(overshoot);
            m_HasTriggered = false;
        } else {

            SetRunning(false);
            SetElapsed(duration);
        }
    }
}

void Timer::Start() {
    SetRunning(true);
    m_HasTriggered = false;
    m_FireCount = 0;

}

void Timer::Stop() {
    SetRunning(false);

}

void Timer::Reset() {
    SetElapsed(0.0f);
    m_HasTriggered = false;
    m_FireCount = 0;

}

void Timer::Restart() {
    Reset();
    Start();

}

bool Timer::IsFinished() const {
    return GetElapsed() >= GetDuration();
}

float Timer::GetTimeRemaining() const {
    float remaining = GetDuration() - GetElapsed();
    return remaining > 0.0f ? remaining : 0.0f;
}

float Timer::GetDuration() const {
    return GetPropertyValue<float>("duration");
}

void Timer::SetDuration(float duration) {
    SetPropertyValue<float>("duration", duration);
}

float Timer::GetElapsed() const {
    return GetPropertyValue<float>("elapsed");
}

void Timer::SetElapsed(float elapsed) {
    SetPropertyValue<float>("elapsed", elapsed);
}

bool Timer::GetLoop() const {
    return GetPropertyValue<bool>("loop");
}

void Timer::SetLoop(bool loop) {
    SetPropertyValue<bool>("loop", loop);
}

int Timer::GetRepeatCount() const {
    return GetPropertyValue<int>("repeatCount");
}

void Timer::SetRepeatCount(int repeatCount) {
    SetPropertyValue<int>("repeatCount", repeatCount);
}

bool Timer::GetAutoStart() const {
    return GetPropertyValue<bool>("autoStart");
}

void Timer::SetAutoStart(bool autoStart) {
    SetPropertyValue<bool>("autoStart", autoStart);
}

bool Timer::IsRunning() const {
    return GetPropertyValue<bool>("running");
}

void Timer::SetRunning(bool running) {
    SetPropertyValue<bool>("running", running);
}

bool Timer::GetIgnoreTimeScale() const {
    return GetPropertyValue<bool>("ignoreTimeScale");
}

void Timer::SetIgnoreTimeScale(bool ignore) {
    SetPropertyValue<bool>("ignoreTimeScale", ignore);
}

void Timer::SetTimeoutCallback(TimeoutCallback callback) {
    m_TimeoutCallback = callback;

}

void Timer::ClearTimeoutCallback() {
    m_TimeoutCallback = nullptr;

}

void Timer::TriggerTimeout() {

    if (m_TimeoutCallback) {
        m_TimeoutCallback();
    }
    Emit("timeout");
}

void Timer::DefineSignals() {
    RegisterSignal({"timeout", {}, "Emitted when the timer reaches its wait time."});
}

}
}
