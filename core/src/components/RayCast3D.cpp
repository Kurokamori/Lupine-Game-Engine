#include "lupine/components/RayCast3D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/SceneManager.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/physics3d/Physics3DWorld.hpp"
#include <algorithm>
#include <cmath>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

RayCast3D::RayCast3D()
    : RayCast3D("RayCast3D") {
}

RayCast3D::RayCast3D(const std::string& name)
    : Component(name),
      m_TargetPosition(0.0f, -1.0f, 0.0f),
      m_ExcludeParent(true),
      m_CollisionMask(0xFFFFFFFFu),
      m_VisibleInGame(false),
      m_DebugColor(0.0f, 0.6f, 0.7f, 0.8f),
      m_DebugColorHit(1.0f, 0.3f, 0.2f, 0.9f),
      m_IsColliding(false),
      m_CollisionPoint(0.0f, 0.0f, 0.0f),
      m_CollisionNormal(0.0f, 0.0f, 0.0f),
      m_Collider(nullptr) {
}

void RayCast3D::DefineProperties() {
    DefineProperty(PROPERTY_DEFAULT_GROUP(targetPosition, Vec3, math::Vec3(0.0f, -1.0f, 0.0f), "RayCast"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(excludeParent, Bool, true, "RayCast"));
    DefineProperty(PROPERTY_GROUP(collisionMask, Int, static_cast<int>(0xFFFFFFFF), Layers3D, "", "Collision"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(visibleInGame, Bool, false, "Debug"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(debugColor, Color, math::Color(0.0f, 0.6f, 0.7f, 0.8f), "Debug"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(debugColorHit, Color, math::Color(1.0f, 0.3f, 0.2f, 0.9f), "Debug"));
}

void RayCast3D::SyncFromProperties() {
    m_TargetPosition = GetPropertyValue<math::Vec3>("targetPosition");
    m_ExcludeParent = GetPropertyValue<bool>("excludeParent");
    m_CollisionMask = static_cast<uint32_t>(GetPropertyValue<int>("collisionMask"));
    m_VisibleInGame = GetPropertyValue<bool>("visibleInGame");
    m_DebugColor = GetPropertyValue<math::Color>("debugColor");
    m_DebugColorHit = GetPropertyValue<math::Color>("debugColorHit");
}

void RayCast3D::OnReady() {
    SyncFromProperties();
}

void RayCast3D::OnPhysicsProcess(float /*deltaTime*/) {
    if (!IsEnabled()) {
        m_IsColliding = false;
        m_Collider = nullptr;
        return;
    }
    UpdateRaycast();
}

void RayCast3D::OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) {
    if (propertyName == "targetPosition") {
        m_TargetPosition = math::Vec3(newValue["x"].get<float>(), newValue["y"].get<float>(),
                                      newValue["z"].get<float>());
    } else if (propertyName == "excludeParent") {
        m_ExcludeParent = newValue.get<bool>();
    } else if (propertyName == "collisionMask") {
        m_CollisionMask = static_cast<uint32_t>(newValue.get<int>());
    } else if (propertyName == "visibleInGame") {
        m_VisibleInGame = newValue.get<bool>();
    } else if (propertyName == "debugColor") {
        m_DebugColor = math::Color(newValue["r"].get<float>(), newValue["g"].get<float>(),
                                   newValue["b"].get<float>(), newValue["a"].get<float>());
    } else if (propertyName == "debugColorHit") {
        m_DebugColorHit = math::Color(newValue["r"].get<float>(), newValue["g"].get<float>(),
                                      newValue["b"].get<float>(), newValue["a"].get<float>());
    }
}

void RayCast3D::SetTargetPosition(const math::Vec3& target) {
    m_TargetPosition = target;
    SetPropertyValue("targetPosition", target);
}

void RayCast3D::SetCollisionMask(uint32_t mask) {
    m_CollisionMask = mask;
    SetPropertyValue("collisionMask", static_cast<int>(mask));
}

void RayCast3D::ForceRaycastUpdate() {
    UpdateRaycast();
}

void RayCast3D::ComputeWorldRay(math::Vec3& outOrigin, math::Vec3& outTarget) const {
    auto* node3D = dynamic_cast<Node3D*>(m_Owner);
    if (!node3D) {
        outOrigin = math::Vec3(0.0f, 0.0f, 0.0f);
        outTarget = m_TargetPosition;
        return;
    }

    outOrigin = node3D->GetGlobalPosition();
    math::Quat rot = node3D->GetGlobalRotation();
    outTarget = outOrigin + (rot * m_TargetPosition);
}

bool RayCast3D::IsExcludedNode(const Node* hitNode) const {
    if (!hitNode) return false;
    if (hitNode == m_Owner) return true;
    if (!m_Owner) return false;

    const Node* ancestor = m_Owner->GetParent();
    while (ancestor) {
        if (ancestor == hitNode) {
            return true;
        }
        ancestor = ancestor->GetParent();
    }
    return false;
}

void RayCast3D::UpdateRaycast() {
    m_IsColliding = false;
    m_Collider = nullptr;
    m_ColliderBodyId = UUID(0);

    auto* sceneManager = SceneManager::GetInstance();
    if (!sceneManager) return;

    auto* physicsWorld = sceneManager->GetPhysics3DWorld();
    if (!physicsWorld) return;

    math::Vec3 origin, target;
    ComputeWorldRay(origin, target);

    math::Vec3 delta = target - origin;
    float maxDistance = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
    if (maxDistance < 1e-5f) return;

    math::Vec3 direction(delta.x / maxDistance, delta.y / maxDistance, delta.z / maxDistance);

    std::vector<physics3d::RaycastHit3D> hits =
        physicsWorld->RaycastAll(origin, direction, maxDistance, nullptr, m_CollisionMask);

    if (hits.empty()) return;

    std::sort(hits.begin(), hits.end(),
              [](const physics3d::RaycastHit3D& a, const physics3d::RaycastHit3D& b) {
                  return a.fraction < b.fraction;
              });

    for (const physics3d::RaycastHit3D& hit : hits) {
        if (!hit.hit) continue;
        Node* hitNode = physicsWorld->GetBodyNode(hit.bodyId);
        if (m_ExcludeParent && IsExcludedNode(hitNode)) {
            continue;
        }
        m_IsColliding = true;
        m_CollisionPoint = hit.point;
        m_CollisionNormal = hit.normal;
        m_Collider = hitNode;
        m_ColliderBodyId = hit.bodyId;
        return;
    }
}

nlohmann::json RayCast3D::CallMethod(const std::string& method, const nlohmann::json& args) {
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
    auto argI = [&](size_t i, int fallback) -> int {
        if (args.is_array() && i < args.size() && args[i].is_number()) {
            return args[i].get<int>();
        }
        return fallback;
    };
    auto vec3Json = [](const Vec3& v) -> nlohmann::json {
        nlohmann::json o;
        o["x"] = v.x; o["y"] = v.y; o["z"] = v.z;
        return o;
    };

    if (method == "is_colliding") {
        return m_IsColliding;
    } else if (method == "get_collider") {
        return Node::NodeArg(m_Collider);
    } else if (method == "get_collision_point") {
        return vec3Json(m_CollisionPoint);
    } else if (method == "get_collision_normal") {
        return vec3Json(m_CollisionNormal);
    } else if (method == "force_raycast_update") {
        ForceRaycastUpdate();
        return m_IsColliding;
    } else if (method == "set_target_position") {
        SetTargetPosition(Vec3(argF(0, 0.0f), argF(1, 0.0f), argF(2, 0.0f)));
    } else if (method == "get_target_position") {
        return vec3Json(m_TargetPosition);
    } else if (method == "set_collision_mask") {
        SetCollisionMask(static_cast<uint32_t>(argI(0, static_cast<int>(0xFFFFFFFF))));
    } else if (method == "get_collision_mask") {
        return static_cast<int>(m_CollisionMask);
    } else if (method == "set_exclude_parent") {
        SetExcludeParent(argB(0, true));
    } else if (method == "get_exclude_parent") {
        return m_ExcludeParent;
    } else if (method == "set_enabled") {
        SetEnabled(argB(0, true));
    } else if (method == "is_enabled") {
        return IsEnabled();
    }
    return nlohmann::json();
}

void RayCast3D::buildDrawCommands(RenderContext& ctx) {
    if (!m_Owner) return;

    SyncFromProperties();

    RenderCamera* camera = ctx.getCamera();
    bool editorView = camera && camera->isEditorCamera;
    if (!editorView && !m_VisibleInGame) {
        return;
    }

    math::Vec3 origin, target;
    ComputeWorldRay(origin, target);

    const math::Color& lineColor = m_IsColliding ? m_DebugColorHit : m_DebugColor;
    ctx.drawLine(origin, target, lineColor, 1.5f);

    if (m_IsColliding) {
        math::Vec3 normalEnd = m_CollisionPoint + m_CollisionNormal * 0.25f;
        ctx.drawLine(m_CollisionPoint, normalEnd, m_DebugColorHit, 1.0f);
    }
}

AABB RayCast3D::getWorldBounds() const {
    math::Vec3 origin, target;
    ComputeWorldRay(origin, target);

    float padding = 0.25f;
    return AABB(
        Vec3(std::min(origin.x, target.x) - padding,
             std::min(origin.y, target.y) - padding,
             std::min(origin.z, target.z) - padding),
        Vec3(std::max(origin.x, target.x) + padding,
             std::max(origin.y, target.y) + padding,
             std::max(origin.z, target.z) + padding));
}

RenderLayer RayCast3D::getRenderLayer() const {
    return RenderLayer::Transparent;
}

SpatialType RayCast3D::getSpatialType() const {
    return SpatialType::World3D;
}

} // namespace components
} // namespace lupine
