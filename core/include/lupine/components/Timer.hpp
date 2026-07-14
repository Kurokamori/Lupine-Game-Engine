#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include <functional>
#include <string>

namespace lupine {
namespace components {

/**
 * Timer Component
 *
 * A non-rendering utility component for time-based events.
 * Counts time up/down and triggers callbacks when it finishes.
 * Core for gameplay timing and delayed actions.
 *
 * Features:
 * - Configurable duration
 * - Looping support
 * - Auto-start capability
 * - Time scale control (scaled/unscaled time)
 * - Callback/signal on timeout
 * - Manual start/stop/reset control
 */
class Timer : public core::Component {
public:
    using TimeoutCallback = std::function<void()>;

    Timer();
    explicit Timer(const std::string& name);
    virtual ~Timer();

    // ISerializable interface
    std::string GetTypeName() const override { return "Timer"; }
    void DefineProperties() override;
    void DefineSignals() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnUpdate(float deltaTime) override;

    // ===== Timer Control =====
    
    /**
     * Start the timer (begins counting from current elapsed time)
     */
    void Start();
    
    /**
     * Stop the timer (pauses counting)
     */
    void Stop();
    
    /**
     * Reset the timer (sets elapsed to 0 and stops if not auto-starting)
     */
    void Reset();
    
    /**
     * Restart the timer (reset + start)
     */
    void Restart();
    
    /**
     * Check if timer has finished (elapsed >= duration)
     */
    bool IsFinished() const;
    
    /**
     * Get the time remaining until timeout (duration - elapsed)
     */
    float GetTimeRemaining() const;

    // ===== Property Accessors =====
    
    // Duration (target time in seconds)
    float GetDuration() const;
    void SetDuration(float duration);
    
    // Elapsed (current elapsed time, runtime)
    float GetElapsed() const;
    void SetElapsed(float elapsed);
    
    // Loop (restart automatically when done)
    bool GetLoop() const;
    void SetLoop(bool loop);

    // Repeat count (maximum number of times the timer fires while looping).
    // -1 means repeat indefinitely (the default); any value >= 1 fires that many
    // times in total and then stops. Ignored when looping is disabled (one-shot).
    int GetRepeatCount() const;
    void SetRepeatCount(int repeatCount);

    // Number of times the timer has fired since the last Start()/Reset().
    int GetFireCount() const { return m_FireCount; }

    // Auto-start (starts when component becomes active)
    bool GetAutoStart() const;
    void SetAutoStart(bool autoStart);
    
    // Running (is the timer currently counting)
    bool IsRunning() const;
    void SetRunning(bool running);
    
    // Ignore time scale (if true, uses unscaled real time)
    bool GetIgnoreTimeScale() const;
    void SetIgnoreTimeScale(bool ignore);

    // ===== Callback Management =====
    
    /**
     * Set the timeout callback
     * This function will be called when elapsed >= duration
     */
    void SetTimeoutCallback(TimeoutCallback callback);
    
    /**
     * Clear the timeout callback
     */
    void ClearTimeoutCallback();

private:
    /**
     * Trigger the timeout callback
     */
    void TriggerTimeout();

    // Callback for timeout event
    TimeoutCallback m_TimeoutCallback;

    // Internal flag to prevent multiple timeout triggers in same frame
    bool m_HasTriggered;

    // Runtime count of timeouts fired since the last Start()/Reset(). Used to
    // honour a finite repeat count without serializing transient state.
    int m_FireCount;
};

} // namespace components
} // namespace lupine
