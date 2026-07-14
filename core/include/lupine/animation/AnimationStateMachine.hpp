#pragma once

#include "lupine/animation/AnimationData.hpp"
#include "lupine/animation/BlendTree.hpp"
#include "lupine/animation/ClipSampler.hpp"
#include <string>
#include <vector>

namespace lupine {
namespace animation {

// Reported when a layer changes state (for the state_changed signal).
struct StateChange {
    bool changed = false;
    std::string from;
    std::string to;
};

/**
 * LayerRuntime - drives one animation layer's state machine.
 *
 * Holds the current state and its normalized phase, advances time, evaluates
 * outgoing/Any-State transitions (priority, exit time, parameter conditions),
 * cross-fades between states, and produces the layer's resolved pose each frame.
 * Method tracks on the active clip-state fire as the playhead crosses them.
 * References the layer data owned by the AnimationTree's graph.
 */
class LayerRuntime {
public:
    void Init(const AnimationLayer& layer);
    void Reset();

    // Advance one frame; returns the layer's resolved pose. Appends any method-track
    // calls crossed and reports a state change.
    TargetValueMap Update(float dt, const BlendContext& ctx,
                          std::vector<MethodCall>& methodCalls, StateChange& change);

    // The active state's pose sampled at phase 0 (additive-layer reference).
    TargetValueMap EvaluateReference(const BlendContext& ctx) const;

    // Request a cross-fade to a named state on the next update.
    void Travel(const std::string& state, float duration = 0.2f);

    const std::string& CurrentState() const { return m_Current; }
    bool IsValid() const { return m_Layer != nullptr; }

private:
    float StateDuration(const AnimationState& state, const BlendContext& ctx) const;
    TargetValueMap EvaluateState(const AnimationState& state, float phase, const BlendContext& ctx) const;
    const AnimationClip* ResolveStateClip(const AnimationState& state, const BlendContext& ctx) const;
    void AdvancePhase(float& phase, float duration, float dt, bool& looped) const;
    void BeginFade(const std::string& toState, float duration, StateChange& change);
    bool TryTransition(const BlendContext& ctx, StateChange& change);

    const AnimationLayer* m_Layer = nullptr;
    std::string m_Current;
    float m_Phase = 0.0f;
    float m_PrevPhase = 0.0f;

    bool m_Fading = false;
    std::string m_FromState;
    float m_FromPhase = 0.0f;
    float m_FadeTime = 0.0f;
    float m_FadeDuration = 0.0f;

    bool m_TravelPending = false;
    std::string m_TravelTarget;
    float m_TravelDuration = 0.2f;
};

} // namespace animation
} // namespace lupine
