#include "lupine/components/PathFollow3D.hpp"
#include "lupine/components/Curve3D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/scripting/NodeRef.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

namespace {
constexpr float kPi = 3.14159265358979323846f;
}

PathFollow3D::PathFollow3D()
    : Component("PathFollow3D")
{
}

PathFollow3D::PathFollow3D(const std::string& name)
    : Component(name)
{
}

PathFollow3D::~PathFollow3D() {
}

void PathFollow3D::DefineProperties() {
    DefineProperty(PROPERTY_DEFAULT_GROUP(pathNode, NodePath, std::string(""), "Path"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(progressRatio, 0.0f, 0.0f, 1.0f, 0.001f, "Path"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(offset, Float, 0.0f, "Path"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(hOffset, Float, 0.0f, "Offset"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(vOffset, Float, 0.0f, "Offset"));

    DefineProperty(PROPERTY_ENUM_GROUP(rotationMode, 1, "Rotation", None, Forward, YawOnly));
    DefineProperty(PROPERTY_DEFAULT_GROUP(flipForward, Bool, false, "Rotation"));

    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(speed, 0.0f, 0.0f, 1000.0f, 0.1f, "Movement"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(loop, Bool, true, "Movement"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(pingPong, Bool, false, "Movement"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(autoStart, Bool, true, "Movement"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(previewInEditor, Bool, true, "Debug"));
}

void PathFollow3D::OnAwake() {
    if (GetAutoStart()) {
        m_Playing = true;
    }
}

void PathFollow3D::OnReady() {
    ApplyToOwner();
}

void PathFollow3D::OnUpdate(float deltaTime) {
    if (m_Playing) {
        Curve3D* path = ResolvePath();
        float length = path ? path->GetCurveLength() : 0.0f;
        float speed = GetSpeed();

        if (length > 0.0001f && speed != 0.0f) {
            float delta = (speed * deltaTime) / length * static_cast<float>(m_Direction);
            float r = GetProgressRatio() + delta;

            if (GetPingPong()) {
                if (r >= 1.0f) {
                    r = 1.0f;
                    m_Direction = -1;
                    if (!GetLoop()) m_Playing = false;
                } else if (r <= 0.0f) {
                    r = 0.0f;
                    m_Direction = 1;
                    if (!GetLoop()) m_Playing = false;
                }
            } else if (GetLoop()) {
                r = NormalizeRatio(r);
            } else if (r >= 1.0f) {
                r = 1.0f;
                m_Playing = false;
            } else if (r < 0.0f) {
                r = 0.0f;
            }

            SetProgressRatioRaw(r);
        }
    }

    ApplyToOwner();
}

// ===== Following control =====

void PathFollow3D::StartFollowing() {
    m_Playing = true;
}

void PathFollow3D::StopFollowing() {
    m_Playing = false;
}

void PathFollow3D::Reset() {
    m_Direction = 1;
    SetProgressRatioRaw(0.0f);
    ApplyToOwner();
}

float PathFollow3D::NormalizeRatio(float ratio) const {
    if (GetLoop()) {
        ratio = ratio - std::floor(ratio);
        return ratio;
    }
    return std::max(0.0f, std::min(1.0f, ratio));
}

// ===== Property accessors =====

float PathFollow3D::GetProgressRatio() const { return GetPropertyValue<float>("progressRatio"); }

void PathFollow3D::SetProgressRatioRaw(float ratio) {
    SetPropertyValue<float>("progressRatio", ratio);
}

void PathFollow3D::SetProgressRatio(float ratio) {
    SetProgressRatioRaw(NormalizeRatio(ratio));
    ApplyToOwner();
}

float PathFollow3D::GetProgressDistance() const {
    Curve3D* path = ResolvePath();
    float length = path ? path->GetCurveLength() : 0.0f;
    return GetProgressRatio() * length;
}

void PathFollow3D::SetProgressDistance(float distance) {
    Curve3D* path = ResolvePath();
    float length = path ? path->GetCurveLength() : 0.0f;
    if (length > 0.0001f) {
        SetProgressRatio(distance / length);
    }
}

std::string PathFollow3D::GetPathNodePath() const { return GetPropertyValue<std::string>("pathNode"); }
void PathFollow3D::SetPathNodePath(const std::string& path) { SetPropertyValue<std::string>("pathNode", path); }

float PathFollow3D::GetOffset() const { return GetPropertyValue<float>("offset"); }
void PathFollow3D::SetOffset(float offset) { SetPropertyValue<float>("offset", offset); }

float PathFollow3D::GetHOffset() const { return GetPropertyValue<float>("hOffset"); }
void PathFollow3D::SetHOffset(float h) { SetPropertyValue<float>("hOffset", h); }

float PathFollow3D::GetVOffset() const { return GetPropertyValue<float>("vOffset"); }
void PathFollow3D::SetVOffset(float v) { SetPropertyValue<float>("vOffset", v); }

PathFollow3D::RotationMode PathFollow3D::GetRotationMode() const {
    return static_cast<RotationMode>(GetPropertyValue<int>("rotationMode"));
}
void PathFollow3D::SetRotationMode(RotationMode mode) {
    SetPropertyValue<int>("rotationMode", static_cast<int>(mode));
}

bool PathFollow3D::GetFlipForward() const { return GetPropertyValue<bool>("flipForward"); }
void PathFollow3D::SetFlipForward(bool flip) { SetPropertyValue<bool>("flipForward", flip); }

float PathFollow3D::GetSpeed() const { return GetPropertyValue<float>("speed"); }
void PathFollow3D::SetSpeed(float speed) { SetPropertyValue<float>("speed", speed); }

bool PathFollow3D::GetLoop() const { return GetPropertyValue<bool>("loop"); }
void PathFollow3D::SetLoop(bool loop) { SetPropertyValue<bool>("loop", loop); }

bool PathFollow3D::GetPingPong() const { return GetPropertyValue<bool>("pingPong"); }
void PathFollow3D::SetPingPong(bool pingPong) { SetPropertyValue<bool>("pingPong", pingPong); }

bool PathFollow3D::GetAutoStart() const { return GetPropertyValue<bool>("autoStart"); }
void PathFollow3D::SetAutoStart(bool autoStart) { SetPropertyValue<bool>("autoStart", autoStart); }

// ===== Resolution / application =====

Curve3D* PathFollow3D::ResolvePath() const {
    Node* owner = GetOwner();
    if (!owner) {
        return nullptr;
    }

    Node* target = nullptr;
    std::string path = GetPathNodePath();
    if (!path.empty() && path != ".") {
        scripting::NodeRef ref = scripting::NodeRef::FromRaw(owner, nullptr).FindNode(path);
        target = ref.IsValid() ? ref.Lock().get() : nullptr;
    }
    if (!target) {
        target = owner->GetParent();
    }
    if (!target) {
        target = owner;
    }

    std::shared_ptr<Curve3D> curve = target->GetComponent<Curve3D>();
    return curve.get();
}

void PathFollow3D::ApplyToOwner() {
    Curve3D* path = ResolvePath();
    Node3D* owner3D = dynamic_cast<Node3D*>(GetOwner());
    if (!path || !owner3D) {
        return;
    }

    float length = path->GetCurveLength();
    float t = GetProgressRatio();
    if (length > 0.0001f) {
        t += GetOffset() / length;
    }
    t = NormalizeRatio(t);

    Vec3 worldPos = path->SampleCurveWorld(t);
    Vec3 fwd = path->SampleTangentWorld(t);
    Vec3 up = path->SampleUpVectorWorld(t);

    float hOff = GetHOffset();
    float vOff = GetVOffset();
    if (hOff != 0.0f || vOff != 0.0f) {
        Vec3 right = fwd.Cross(up).Normalized();
        worldPos = worldPos + right * hOff + up * vOff;
    }

    RotationMode mode = GetRotationMode();

    // Convert world transform into the owner's local space (handles followers
    // that are not direct children of the path node).
    Node3D* parent = dynamic_cast<Node3D*>(owner3D->GetParent());
    if (parent) {
        Mat4 parentInv = parent->GetGlobalTransformMatrix().Inverse();
        owner3D->SetPosition(parentInv.TransformPoint(worldPos));
    } else {
        owner3D->SetPosition(worldPos);
    }

    if (mode == RotationMode::None) {
        return;
    }

    Quat worldRot;
    if (mode == RotationMode::YawOnly) {
        Vec3 flat(fwd.x, 0.0f, fwd.z);
        if (flat.LengthSquared() < 0.0001f) {
            flat = fwd;
        }
        worldRot = Quat::LookRotation(flat.Normalized(), Vec3::Up());
    } else {
        if (fwd.LengthSquared() < 0.0001f) {
            worldRot = owner3D->GetRotation();
        } else {
            worldRot = Quat::LookRotation(fwd.Normalized(), up);
        }
    }

    if (GetFlipForward()) {
        worldRot = worldRot * Quat::FromAxisAngle(Vec3::Up(), kPi);
    }

    if (parent) {
        owner3D->SetRotation(parent->GetGlobalRotation().Inverse() * worldRot);
    } else {
        owner3D->SetRotation(worldRot);
    }
}

// ===== Scripting bridge =====

nlohmann::json PathFollow3D::CallMethod(const std::string& method, const nlohmann::json& args) {
    auto argF = [&](size_t i, float fallback) -> float {
        if (args.is_array() && i < args.size() && args[i].is_number()) {
            return args[i].get<float>();
        }
        return fallback;
    };
    auto argB = [&](size_t i, bool fallback) -> bool {
        if (args.is_array() && i < args.size() && args[i].is_boolean()) {
            return args[i].get<bool>();
        }
        return fallback;
    };

    if (method == "start_following") {
        StartFollowing();
    } else if (method == "stop_following") {
        StopFollowing();
    } else if (method == "reset") {
        Reset();
    } else if (method == "is_following") {
        return IsFollowing();
    } else if (method == "get_progress_ratio") {
        return GetProgressRatio();
    } else if (method == "set_progress_ratio") {
        SetProgressRatio(argF(0, 0.0f));
    } else if (method == "get_progress_distance") {
        return GetProgressDistance();
    } else if (method == "set_progress_distance") {
        SetProgressDistance(argF(0, 0.0f));
    } else if (method == "set_speed") {
        SetSpeed(argF(0, 0.0f));
    } else if (method == "get_speed") {
        return GetSpeed();
    } else if (method == "set_offset") {
        SetOffset(argF(0, 0.0f));
    } else if (method == "set_h_offset") {
        SetHOffset(argF(0, 0.0f));
    } else if (method == "set_v_offset") {
        SetVOffset(argF(0, 0.0f));
    } else if (method == "set_loop") {
        SetLoop(argB(0, true));
    } else if (method == "set_ping_pong") {
        SetPingPong(argB(0, false));
    } else if (method == "set_flip_forward") {
        SetFlipForward(argB(0, false));
    } else if (method == "apply") {
        ApplyToOwner();
    }

    return nlohmann::json();
}

// ===== IRenderableComponent =====

void PathFollow3D::buildDrawCommands(RenderContext& ctx) {
    if (!IsEnabled() || !GetOwner()) {
        return;
    }

    Scene* scene = GetOwner()->GetScene();
    bool inEditor = scene && scene->IsInEditor();

    // Editor live-preview: snap the owner along the path while editing.
    if (inEditor && GetPropertyValue<bool>("previewInEditor")) {
        ApplyToOwner();

        Node3D* owner3D = dynamic_cast<Node3D*>(GetOwner());
        if (owner3D) {
            Vec3 pos = owner3D->GetGlobalPosition();
            ctx.drawBox(pos, Vec3(0.2f, 0.2f, 0.2f), Color(1.0f, 0.85f, 0.2f, 0.9f), true);
        }
    }
}

AABB PathFollow3D::getWorldBounds() const {
    Node3D* owner3D = dynamic_cast<Node3D*>(GetOwner());
    Vec3 c = owner3D ? owner3D->GetGlobalPosition() : Vec3::Zero();
    return AABB(c - Vec3(0.25f), c + Vec3(0.25f));
}

RenderLayer PathFollow3D::getRenderLayer() const {
    return RenderLayer::Transparent;
}

SpatialType PathFollow3D::getSpatialType() const {
    return SpatialType::World3D;
}

} // namespace components
} // namespace lupine
