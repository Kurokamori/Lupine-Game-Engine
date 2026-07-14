#include "lupine/components/Tween.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/SignalDispatcher.hpp"
#include "lupine/scripting/NodeRef.hpp"
#include "lupine/animation/AnimationInterp.hpp"
#include <algorithm>

namespace lupine {
namespace components {

using namespace core;

namespace {

bool IsTransformChannel(const std::string& channel) {
    return channel == "position_2d"
        || channel == "global_position_2d"
        || channel == "rotation_2d"
        || channel == "scale_2d"
        || channel == "position_3d"
        || channel == "rotation_3d"
        || channel == "scale_3d";
}

} // namespace

Tween::Tween()
    : Component("Tween") {
}

Tween::Tween(const std::string& name)
    : Component(name) {
}

Tween::~Tween() {
}

void Tween::DefineProperties() {
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(duration, 1.0f, 0.0f, 3600.0f, 0.1f, "Tween"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(elapsed, 0.0f, 0.0f, 3600.0f, 0.1f, "Tween"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(easing, String, std::string("linear"), "Tween"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(loop, Bool, false, "Tween"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(autoRemove, Bool, false, "Tween"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(running, Bool, false, "Tween"));
}

void Tween::DefineSignals() {
    RegisterSignal({"finished", {}, "Emitted when the tween reaches its target."});
    RegisterSignal({"step", {}, "Emitted every frame while the tween is running."});
}

void Tween::OnAwake() {
}

void Tween::OnUpdate(float deltaTime) {
    if (!IsRunning()) {
        return;
    }

    CaptureFromIfNeeded();

    float duration = GetDuration();
    float elapsed = GetElapsed() + deltaTime;
    SetElapsed(elapsed);

    float t = (duration > 0.0f) ? (elapsed / duration) : 1.0f;

    if (t >= 1.0f) {
        ApplyEased(1.0f);
        Emit("step");

        if (GetLoop()) {
            SetElapsed(0.0f);
            m_Captured = false;
            m_HasTriggered = false;
        } else {
            SetRunning(false);
            if (!m_HasTriggered) {
                m_HasTriggered = true;
                Finish();
            }
        }
    } else {
        ApplyEased(t);
        Emit("step");
    }
}

void Tween::Configure(const std::string& channel, const nlohmann::json& toValue,
                      float duration, const std::string& easing) {
    SetChannel(channel);
    SetToValue(toValue);
    SetDuration(duration);
    SetEasing(easing);
}

void Tween::SetChannel(const std::string& channel) {
    m_Channel = channel;
}

void Tween::SetToValue(const nlohmann::json& value) {
    m_To = value;
}

// Leaves m_Captured false: the capture pass is still needed to resolve the target
// property's key shape, it just skips reading the live value.
void Tween::SetFrom(const nlohmann::json& value) {
    m_From = value;
    m_HasExplicitFrom = true;
    m_Captured = false;
}

void Tween::Start() {
    SetElapsed(0.0f);
    m_Captured = false;
    m_HasTriggered = false;
    SetRunning(true);
}

void Tween::Stop() {
    SetRunning(false);
}

void Tween::Reset() {
    SetElapsed(0.0f);
    m_Captured = false;
    m_HasTriggered = false;
}

void Tween::Restart() {
    Reset();
    Start();
}

bool Tween::IsFinished() const {
    return GetElapsed() >= GetDuration();
}

float Tween::GetProgress() const {
    float duration = GetDuration();
    if (duration <= 0.0f) {
        return 1.0f;
    }
    return std::clamp(GetElapsed() / duration, 0.0f, 1.0f);
}

void Tween::CaptureFromIfNeeded() {
    if (m_Captured) {
        return;
    }

    ResolveChannelShape();

    if (!m_HasExplicitFrom) {
        m_From = ReadChannelValue();
    }

    // Both endpoints must share a shape for LerpJson to interpolate them, and a
    // keyed property object cannot be lerped directly. The start value defines the
    // baseline a partial target falls back to, so it is canonicalized first.
    m_From = ToCanonicalArray(m_From, m_PropertyKeys, nlohmann::json());
    m_To = ToCanonicalArray(m_To, m_PropertyKeys, m_From);

    m_Captured = true;
}

void Tween::ResolveChannelShape() {
    m_PropertyKeys.clear();

    if (IsTransformChannel(m_Channel)) {
        return;
    }

    scripting::NodeRef ref = scripting::NodeRef::FromRaw(GetOwner(), nullptr);
    if (!ref.IsValid()) {
        return;
    }

    const std::vector<std::string>* keys = ObjectKeyOrder(ref.Get(m_Channel));
    if (keys != nullptr) {
        m_PropertyKeys = *keys;
    }
}

const std::vector<std::string>* Tween::ObjectKeyOrder(const nlohmann::json& value) {
    static const std::vector<std::string> kColor = {"r", "g", "b", "a"};
    static const std::vector<std::string> kVec4 = {"x", "y", "z", "w"};
    static const std::vector<std::string> kRect = {"x", "y", "w", "h"};
    static const std::vector<std::string> kVec3 = {"x", "y", "z"};
    static const std::vector<std::string> kVec2 = {"x", "y"};

    if (!value.is_object()) {
        return nullptr;
    }

    auto matches = [&value](const std::vector<std::string>& keys) -> bool {
        for (const std::string& key : keys) {
            nlohmann::json::const_iterator it = value.find(key);
            if (it == value.end() || !it->is_number()) {
                return false;
            }
        }
        return true;
    };

    // Widest key sets first: a Vec4 also satisfies the Vec2/Vec3 checks. Quat
    // ({w,x,y,z}) matches kVec4; only the index mapping matters, and it round-trips
    // through the same key list on the way back out.
    if (matches(kColor)) return &kColor;
    if (matches(kVec4)) return &kVec4;
    if (matches(kRect)) return &kRect;
    if (matches(kVec3)) return &kVec3;
    if (matches(kVec2)) return &kVec2;

    return nullptr;
}

nlohmann::json Tween::ToCanonicalArray(const nlohmann::json& value,
                                       const std::vector<std::string>& keys,
                                       const nlohmann::json& fallback) {
    if (keys.empty() || !value.is_object()) {
        return value;
    }

    nlohmann::json out = nlohmann::json::array();
    for (std::size_t i = 0; i < keys.size(); ++i) {
        nlohmann::json::const_iterator it = value.find(keys[i]);
        if (it != value.end() && it->is_number()) {
            out.push_back(it->get<double>());
        } else if (fallback.is_array() && i < fallback.size() && fallback[i].is_number()) {
            out.push_back(fallback[i].get<double>());
        } else {
            out.push_back(0.0);
        }
    }
    return out;
}

nlohmann::json Tween::FromCanonicalArray(const nlohmann::json& value,
                                         const std::vector<std::string>& keys) {
    if (keys.empty() || !value.is_array()) {
        return value;
    }

    nlohmann::json out = nlohmann::json::object();
    for (std::size_t i = 0; i < keys.size(); ++i) {
        out[keys[i]] = (i < value.size() && value[i].is_number()) ? value[i].get<double>() : 0.0;
    }
    return out;
}

void Tween::ApplyEased(float t01) {
    if (m_From.is_null() || m_To.is_null()) {
        return;
    }
    float eased = animation::ApplyEasing(GetEasing(), t01);
    WriteChannelValue(animation::LerpJson(m_From, m_To, eased));
}

void Tween::Finish() {
    Emit("finished");

    if (GetAutoRemove()) {
        core::Node* owner = GetOwner();
        Tween* self = this;
        core::SignalDispatcher::Get().QueueDeferredSlot(
            [owner, self](const core::SignalArgs&) {
                if (!owner) {
                    return;
                }
                for (const auto& c : owner->GetComponents()) {
                    if (c.get() == self) {
                        owner->RemoveComponent(c);
                        break;
                    }
                }
            }, {});
    }
}

nlohmann::json Tween::ReadChannelValue() const {
    scripting::NodeRef ref = scripting::NodeRef::FromRaw(GetOwner(), nullptr);
    if (!ref.IsValid()) {
        return nlohmann::json();
    }

    if (m_Channel == "position_2d") {
        math::Vec2 p = ref.GetPosition2D();
        return nlohmann::json::array({p.x, p.y});
    }
    if (m_Channel == "global_position_2d") {
        math::Vec2 p = ref.GetGlobalPosition2D();
        return nlohmann::json::array({p.x, p.y});
    }
    if (m_Channel == "rotation_2d") {
        return ref.GetRotation2D();
    }
    if (m_Channel == "scale_2d") {
        math::Vec2 s = ref.GetScale2D();
        return nlohmann::json::array({s.x, s.y});
    }
    if (m_Channel == "position_3d") {
        math::Vec3 p = ref.GetPosition3D();
        return nlohmann::json::array({p.x, p.y, p.z});
    }
    if (m_Channel == "rotation_3d") {
        math::Vec3 r = ref.GetRotation3D();
        return nlohmann::json::array({r.x, r.y, r.z});
    }
    if (m_Channel == "scale_3d") {
        math::Vec3 s = ref.GetScale3D();
        return nlohmann::json::array({s.x, s.y, s.z});
    }

    return ref.Get(m_Channel);
}

void Tween::WriteChannelValue(const nlohmann::json& value) const {
    scripting::NodeRef ref = scripting::NodeRef::FromRaw(GetOwner(), nullptr);
    if (!ref.IsValid()) {
        return;
    }

    if (m_Channel == "position_2d") {
        ref.SetPosition2D(animation::JsonComponent(value, 0), animation::JsonComponent(value, 1));
    } else if (m_Channel == "global_position_2d") {
        ref.SetGlobalPosition2D(animation::JsonComponent(value, 0), animation::JsonComponent(value, 1));
    } else if (m_Channel == "rotation_2d") {
        ref.SetRotation2D(value.is_number() ? value.get<float>() : animation::JsonComponent(value, 0));
    } else if (m_Channel == "scale_2d") {
        ref.SetScale2D(animation::JsonComponent(value, 0), animation::JsonComponent(value, 1));
    } else if (m_Channel == "position_3d") {
        ref.SetPosition3D(animation::JsonComponent(value, 0), animation::JsonComponent(value, 1), animation::JsonComponent(value, 2));
    } else if (m_Channel == "rotation_3d") {
        ref.SetRotation3D(animation::JsonComponent(value, 0), animation::JsonComponent(value, 1), animation::JsonComponent(value, 2));
    } else if (m_Channel == "scale_3d") {
        ref.SetScale3D(animation::JsonComponent(value, 0), animation::JsonComponent(value, 1), animation::JsonComponent(value, 2));
    } else {
        // The property bag stores values verbatim, so a keyed property must be written
        // back as an object: an array would leave every channel lookup missing.
        ref.Set(m_Channel, FromCanonicalArray(value, m_PropertyKeys));
    }
}

float Tween::GetDuration() const { return GetPropertyValue<float>("duration"); }
void Tween::SetDuration(float duration) { SetPropertyValue<float>("duration", duration); }
float Tween::GetElapsed() const { return GetPropertyValue<float>("elapsed"); }
void Tween::SetElapsed(float elapsed) { SetPropertyValue<float>("elapsed", elapsed); }
std::string Tween::GetEasing() const { return GetPropertyValue<std::string>("easing"); }
void Tween::SetEasing(const std::string& easing) { SetPropertyValue<std::string>("easing", easing); }
bool Tween::GetLoop() const { return GetPropertyValue<bool>("loop"); }
void Tween::SetLoop(bool loop) { SetPropertyValue<bool>("loop", loop); }
bool Tween::GetAutoRemove() const { return GetPropertyValue<bool>("autoRemove"); }
void Tween::SetAutoRemove(bool autoRemove) { SetPropertyValue<bool>("autoRemove", autoRemove); }
bool Tween::IsRunning() const { return GetPropertyValue<bool>("running"); }
void Tween::SetRunning(bool running) { SetPropertyValue<bool>("running", running); }

} // namespace components
} // namespace lupine
