#include "lupine/animation/AnimationStateMachine.hpp"
#include <algorithm>
#include <cmath>

namespace lupine {
namespace animation {

namespace {
constexpr float kEpsilon = 1e-5f;
}

void LayerRuntime::Init(const AnimationLayer& layer) {
    m_Layer = &layer;
    Reset();
}

void LayerRuntime::Reset() {
    m_Phase = 0.0f;
    m_PrevPhase = 0.0f;
    m_Fading = false;
    m_FromState.clear();
    m_FromPhase = 0.0f;
    m_FadeTime = 0.0f;
    m_FadeDuration = 0.0f;
    m_TravelPending = false;
    if (m_Layer) {
        m_Current = m_Layer->stateMachine.entry;
        if (m_Current.empty() && !m_Layer->stateMachine.states.empty()) {
            m_Current = m_Layer->stateMachine.states.front().name;
        }
    }
}

void LayerRuntime::Travel(const std::string& state, float duration) {
    m_TravelPending = true;
    m_TravelTarget = state;
    m_TravelDuration = duration;
}

const AnimationClip* LayerRuntime::ResolveStateClip(const AnimationState& state,
                                                    const BlendContext& ctx) const {
    if (!ctx.resolveClip) return nullptr;
    return ctx.resolveClip(state.clip, std::string());
}

float LayerRuntime::StateDuration(const AnimationState& state, const BlendContext& ctx) const {
    float speed = std::fabs(state.speed) > kEpsilon ? std::fabs(state.speed) : 1.0f;
    if (state.isBlendTree) {
        if (!state.blendTree) return 0.0f;
        return BlendTree::EffectiveDuration(*state.blendTree, ctx) / speed;
    }
    const AnimationClip* clip = ResolveStateClip(state, ctx);
    if (!clip) return 0.0f;
    return clip->EffectiveLength() / speed;
}

TargetValueMap LayerRuntime::EvaluateState(const AnimationState& state, float phase,
                                           const BlendContext& ctx) const {
    if (state.isBlendTree) {
        if (!state.blendTree) return {};
        return BlendTree::Evaluate(*state.blendTree, ctx, phase);
    }
    const AnimationClip* clip = ResolveStateClip(state, ctx);
    if (!clip) return {};
    AnimationPose pose;
    ClipSampler::Sample(*clip, std::clamp(phase, 0.0f, 1.0f) * clip->EffectiveLength(), 1.0f, pose);
    return pose.Finalize();
}

void LayerRuntime::AdvancePhase(float& phase, float duration, float dt, bool& looped) const {
    looped = false;
    if (duration <= kEpsilon) {
        return;
    }
    phase += dt / duration;
    while (phase >= 1.0f) {
        phase -= 1.0f;
        looped = true;
    }
    if (phase < 0.0f) {
        phase = 0.0f;
    }
}

void LayerRuntime::BeginFade(const std::string& toState, float duration, StateChange& change) {
    change.changed = true;
    change.from = m_Current;
    change.to = toState;
    if (duration <= kEpsilon) {
        m_Current = toState;
        m_Phase = 0.0f;
        m_PrevPhase = 0.0f;
        m_Fading = false;
        return;
    }
    m_FromState = m_Current;
    m_FromPhase = m_Phase;
    m_FadeDuration = duration;
    m_FadeTime = 0.0f;
    m_Fading = true;
    m_Current = toState;
    m_Phase = 0.0f;
    m_PrevPhase = 0.0f;
}

bool LayerRuntime::TryTransition(const BlendContext& ctx, StateChange& change) {
    if (!m_Layer || !ctx.params) return false;

    const StateMachineData& sm = m_Layer->stateMachine;
    const StateTransition* best = nullptr;

    for (const StateTransition& t : sm.transitions) {
        if (!(t.from == m_Current || t.IsAnyState())) continue;
        if (t.to == m_Current) continue;
        if (t.hasExitTime && m_Phase < t.exitTime) continue;

        bool ok = true;
        for (const TransitionCondition& cond : t.conditions) {
            if (!ctx.params->EvaluateCondition(cond)) { ok = false; break; }
        }
        // A transition with no conditions requires an exit time to fire on its own.
        if (ok && t.conditions.empty() && !t.hasExitTime) ok = false;
        if (!ok) continue;

        if (!best || t.priority > best->priority) {
            best = &t;
        }
    }

    if (!best) return false;

    // Triggers are consumed by the firing transition (mutable through the tree).
    const_cast<AnimationParameters*>(ctx.params)->ConsumeTriggersFor(best->conditions);
    BeginFade(best->to, best->duration, change);
    return true;
}

TargetValueMap LayerRuntime::Update(float dt, const BlendContext& ctx,
                                    std::vector<MethodCall>& methodCalls, StateChange& change) {
    if (!m_Layer) return {};

    const StateMachineData& sm = m_Layer->stateMachine;

    if (m_TravelPending) {
        m_TravelPending = false;
        if (!m_TravelTarget.empty() && m_TravelTarget != m_Current && sm.FindState(m_TravelTarget)) {
            BeginFade(m_TravelTarget, m_TravelDuration, change);
        }
    }

    const AnimationState* cur = sm.FindState(m_Current);
    if (!cur) {
        if (!sm.states.empty()) {
            m_Current = sm.entry.empty() ? sm.states.front().name : sm.entry;
            cur = sm.FindState(m_Current);
        }
        if (!cur) return {};
    }

    float dur = StateDuration(*cur, ctx);
    m_PrevPhase = m_Phase;
    bool looped = false;
    AdvancePhase(m_Phase, dur, dt, looped);

    if (m_Fading) {
        const AnimationState* fromState = sm.FindState(m_FromState);
        if (fromState) {
            float fdur = StateDuration(*fromState, ctx);
            bool fl = false;
            AdvancePhase(m_FromPhase, fdur, dt, fl);
        }
        m_FadeTime += dt;
        if (m_FadeTime >= m_FadeDuration) {
            m_Fading = false;
        }
    }

    if (!m_Fading) {
        if (TryTransition(ctx, change)) {
            cur = sm.FindState(m_Current);
            if (!cur) return {};
        }
    }

    // Fire method tracks for a clip-state that advanced without changing this frame.
    if (cur && !cur->isBlendTree && !change.changed) {
        const AnimationClip* clip = ResolveStateClip(*cur, ctx);
        if (clip) {
            float len = clip->EffectiveLength();
            if (looped) {
                ClipSampler::CollectMethodCalls(*clip, m_PrevPhase * len, len, methodCalls);
                ClipSampler::CollectMethodCalls(*clip, -1.0f, m_Phase * len, methodCalls);
            } else {
                ClipSampler::CollectMethodCalls(*clip, m_PrevPhase * len, m_Phase * len, methodCalls);
            }
        }
    }

    TargetValueMap curMap = cur ? EvaluateState(*cur, m_Phase, ctx) : TargetValueMap{};

    if (m_Fading) {
        const AnimationState* fromState = sm.FindState(m_FromState);
        TargetValueMap fromMap = fromState ? EvaluateState(*fromState, m_FromPhase, ctx) : TargetValueMap{};
        float t = m_FadeDuration > kEpsilon ? std::clamp(m_FadeTime / m_FadeDuration, 0.0f, 1.0f) : 1.0f;
        return BlendTree::BlendMaps({ {fromMap, 1.0f - t}, {curMap, t} });
    }

    return curMap;
}

TargetValueMap LayerRuntime::EvaluateReference(const BlendContext& ctx) const {
    if (!m_Layer) return {};
    const AnimationState* cur = m_Layer->stateMachine.FindState(m_Current);
    if (!cur) return {};
    return EvaluateState(*cur, 0.0f, ctx);
}

} // namespace animation
} // namespace lupine
