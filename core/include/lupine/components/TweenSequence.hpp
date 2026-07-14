#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <vector>

namespace lupine {
namespace core { class Node; }

namespace components {

/**
 * TweenSequence Component
 *
 * Runs an ordered list of steps - property tweens, delays (intervals), and
 * method callbacks - and emits "finished" when the whole sequence completes.
 * It is the runtime backing for the scripting sequence builder (create_sequence
 * / append / append_interval / append_callback / parallel / set_loops).
 *
 * Steps form phases: a step appended with `parallel = true` joins the previous
 * step's phase and runs alongside it; otherwise it starts a new phase that begins
 * only after the previous phase fully completes. Property tweens are executed by
 * spawning child Tween components on the target node (reusing the easing/interp
 * machinery) and are cleaned up when the phase advances.
 */
class TweenSequence : public core::Component {
public:
    TweenSequence();
    explicit TweenSequence(const std::string& name);
    virtual ~TweenSequence();

    std::string GetTypeName() const override { return "TweenSequence"; }
    void DefineProperties() override;
    void DefineSignals() override;

    void OnUpdate(float deltaTime) override;

    // ===== Builder (scripting API). Each returns nothing; call before Play(). =====
    // A tween step on `target` (nullptr => the sequence's owner node).
    void AppendTween(core::Node* target, const std::string& channel, const nlohmann::json& toValue,
                     float duration, const std::string& easing, bool parallel);
    // A delay step.
    void AppendInterval(float duration, bool parallel);
    // A method-call step on `target` (nullptr => owner). The method resolves on
    // the target node's script(s), like a signal handler.
    void AppendCallback(core::Node* target, const std::string& method, bool parallel);

    // ===== Control =====
    void Play();
    void Stop();
    void Reset();
    void Restart();
    void Kill();  // deferred self-removal from the owner
    bool IsRunning() const;
    bool IsFinished() const;

    // ===== Config =====
    int GetLoops() const;
    void SetLoops(int loops);  // -1 = infinite
    bool GetAutoRemove() const;
    void SetAutoRemove(bool autoRemove);

    size_t GetStepCount() const { return m_Steps.size(); }

private:
    enum class StepKind { Tween, Interval, Callback };

    struct Step {
        StepKind kind = StepKind::Interval;
        bool parallel = false;
        std::weak_ptr<core::Node> target;
        std::string channel;
        nlohmann::json to;
        float duration = 0.0f;
        std::string easing = "linear";
        std::string method;
    };

    void BeginPhase(size_t firstStepIndex);
    bool PhaseComplete();
    void EndPhase();
    void Finish();
    size_t PhaseEndExclusive(size_t firstStepIndex) const;

    std::vector<Step> m_Steps;

    bool m_Running = false;
    bool m_Finished = false;
    size_t m_PhaseStart = 0;          // index of the first step in the active phase
    size_t m_PhaseEnd = 0;            // one past the last step in the active phase
    int m_LoopsRemaining = 0;         // counts down from GetLoops() when finite
    bool m_PhaseActive = false;

    // Per-active-phase runtime state, parallel to steps [m_PhaseStart, m_PhaseEnd).
    std::vector<std::shared_ptr<core::Component>> m_ActiveTweens;  // spawned child tweens
    std::vector<float> m_IntervalRemaining;                        // per interval step
};

} // namespace components
} // namespace lupine
