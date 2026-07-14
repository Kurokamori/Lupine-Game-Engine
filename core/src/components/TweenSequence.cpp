#include "lupine/components/TweenSequence.hpp"
#include "lupine/components/Tween.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/SignalDispatcher.hpp"

namespace lupine {
namespace components {

using namespace core;

namespace {

std::weak_ptr<core::Node> WeakOf(core::Node* node) {
    if (!node) return {};
    try {
        return std::weak_ptr<core::Node>(node->shared_from_this());
    } catch (const std::bad_weak_ptr&) {
        return {};
    }
}

components::Tween* AsTween(const std::shared_ptr<core::Component>& comp) {
    return dynamic_cast<components::Tween*>(comp.get());
}

} // namespace

TweenSequence::TweenSequence()
    : Component("TweenSequence") {
}

TweenSequence::TweenSequence(const std::string& name)
    : Component(name) {
}

TweenSequence::~TweenSequence() {
}

void TweenSequence::DefineProperties() {
    DefineProperty(PROPERTY_DEFAULT_GROUP(loops, Int, 1, "Tween Sequence"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(autoRemove, Bool, false, "Tween Sequence"));
}

void TweenSequence::DefineSignals() {
    RegisterSignal({"finished", {}, "Emitted when the whole sequence completes."});
    RegisterSignal({"step_finished", {}, "Emitted when a phase of the sequence completes."});
}

size_t TweenSequence::PhaseEndExclusive(size_t firstStepIndex) const {
    size_t j = firstStepIndex + 1;
    while (j < m_Steps.size() && m_Steps[j].parallel) {
        ++j;
    }
    return j;
}

void TweenSequence::AppendTween(core::Node* target, const std::string& channel,
                                const nlohmann::json& toValue, float duration,
                                const std::string& easing, bool parallel) {
    Step step;
    step.kind = StepKind::Tween;
    step.parallel = parallel && !m_Steps.empty();
    step.target = WeakOf(target);
    step.channel = channel;
    step.to = toValue;
    step.duration = duration;
    step.easing = easing.empty() ? "linear" : easing;
    m_Steps.push_back(std::move(step));
}

void TweenSequence::AppendInterval(float duration, bool parallel) {
    Step step;
    step.kind = StepKind::Interval;
    step.parallel = parallel && !m_Steps.empty();
    step.duration = duration;
    m_Steps.push_back(std::move(step));
}

void TweenSequence::AppendCallback(core::Node* target, const std::string& method, bool parallel) {
    Step step;
    step.kind = StepKind::Callback;
    step.parallel = parallel && !m_Steps.empty();
    step.target = WeakOf(target);
    step.method = method;
    m_Steps.push_back(std::move(step));
}

void TweenSequence::BeginPhase(size_t firstStepIndex) {
    m_PhaseStart = firstStepIndex;
    m_PhaseEnd = PhaseEndExclusive(firstStepIndex);
    m_ActiveTweens.clear();
    m_IntervalRemaining.clear();
    m_PhaseActive = true;

    core::Node* owner = GetOwner();

    for (size_t i = m_PhaseStart; i < m_PhaseEnd; ++i) {
        Step& step = m_Steps[i];
        std::shared_ptr<core::Node> lockedTarget = step.target.lock();
        core::Node* node = lockedTarget ? lockedTarget.get() : owner;

        switch (step.kind) {
            case StepKind::Tween: {
                if (!node) break;
                auto tween = std::make_shared<components::Tween>();
                tween->RegisterProperties();
                tween->Configure(step.channel, step.to, step.duration, step.easing);
                tween->SetAutoRemove(true);
                node->AddComponent(tween);
                tween->Start();
                m_ActiveTweens.push_back(tween);
                break;
            }
            case StepKind::Interval: {
                m_IntervalRemaining.push_back(step.duration);
                break;
            }
            case StepKind::Callback: {
                if (node) {
                    node->InvokeSignalMethod(step.method, {});
                }
                break;
            }
        }
    }
}

bool TweenSequence::PhaseComplete() {
    for (const auto& comp : m_ActiveTweens) {
        components::Tween* tween = AsTween(comp);
        if (tween && !tween->IsFinished()) {
            return false;
        }
    }
    for (float remaining : m_IntervalRemaining) {
        if (remaining > 0.0f) {
            return false;
        }
    }
    return true;
}

void TweenSequence::EndPhase() {
    // Child tweens auto-remove themselves on finish; releasing our references is
    // enough during normal advance. Any still-running tween (early Stop/Reset) is
    // removed explicitly by the caller via the owner before this runs.
    m_ActiveTweens.clear();
    m_IntervalRemaining.clear();
    m_PhaseActive = false;
}

void TweenSequence::OnUpdate(float deltaTime) {
    if (!m_Running || !m_PhaseActive) {
        return;
    }

    for (float& remaining : m_IntervalRemaining) {
        remaining -= deltaTime;
    }

    if (!PhaseComplete()) {
        return;
    }

    Emit("step_finished");
    EndPhase();

    size_t next = m_PhaseEnd;
    if (next < m_Steps.size()) {
        BeginPhase(next);
        return;
    }

    // Reached the end of the sequence.
    if (GetLoops() < 0) {
        BeginPhase(0);
        return;
    }
    --m_LoopsRemaining;
    if (m_LoopsRemaining > 0) {
        BeginPhase(0);
        return;
    }
    Finish();
}

void TweenSequence::Play() {
    m_Running = true;
    m_Finished = false;
    m_LoopsRemaining = GetLoops();
    if (m_Steps.empty()) {
        Finish();
        return;
    }
    BeginPhase(0);
}

void TweenSequence::Stop() {
    if (m_PhaseActive) {
        // Safe here: Stop is called from script, not during the owner's component
        // iteration, so removing still-running child tweens won't invalidate it.
        for (const auto& comp : m_ActiveTweens) {
            if (comp) {
                core::Node* owner = comp->GetOwner();
                if (owner) {
                    owner->RemoveComponent(comp);
                }
            }
        }
    }
    EndPhase();
    m_Running = false;
}

void TweenSequence::Reset() {
    Stop();
    m_Finished = false;
    m_PhaseStart = 0;
    m_PhaseEnd = 0;
}

void TweenSequence::Restart() {
    Reset();
    Play();
}

void TweenSequence::Kill() {
    Stop();
    core::Node* owner = GetOwner();
    TweenSequence* self = this;
    core::SignalDispatcher::Get().QueueDeferredSlot(
        [owner, self](const core::SignalArgs&) {
            if (!owner) return;
            for (const auto& c : owner->GetComponents()) {
                if (c.get() == self) {
                    owner->RemoveComponent(c);
                    break;
                }
            }
        }, {});
}

bool TweenSequence::IsRunning() const {
    return m_Running;
}

bool TweenSequence::IsFinished() const {
    return m_Finished;
}

void TweenSequence::Finish() {
    m_Running = false;
    m_Finished = true;
    m_PhaseActive = false;
    Emit("finished");

    if (GetAutoRemove()) {
        core::Node* owner = GetOwner();
        TweenSequence* self = this;
        core::SignalDispatcher::Get().QueueDeferredSlot(
            [owner, self](const core::SignalArgs&) {
                if (!owner) return;
                for (const auto& c : owner->GetComponents()) {
                    if (c.get() == self) {
                        owner->RemoveComponent(c);
                        break;
                    }
                }
            }, {});
    }
}

int TweenSequence::GetLoops() const { return GetPropertyValue<int>("loops"); }
void TweenSequence::SetLoops(int loops) { SetPropertyValue<int>("loops", loops); }
bool TweenSequence::GetAutoRemove() const { return GetPropertyValue<bool>("autoRemove"); }
void TweenSequence::SetAutoRemove(bool autoRemove) { SetPropertyValue<bool>("autoRemove", autoRemove); }

} // namespace components
} // namespace lupine
