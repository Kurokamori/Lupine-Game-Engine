#include "lupine/components/RayCast2D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/SceneManager.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/physics2d/Physics2DWorld.hpp"
#include <algorithm>
#include <cmath>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

RayCast2D::RayCast2D()
    : RayCast2D("RayCast2D") {
}

RayCast2D::RayCast2D(const std::string& name)
    : Component(name),
      m_TargetPosition(0.0f, 50.0f),
      m_CollisionMask(0xFFFFFFFFu),
      m_ExcludeParent(true),
      m_VisibleInGame(false),
      m_DebugColor(0.0f, 0.6f, 0.7f, 0.8f),
      m_DebugColorHit(1.0f, 0.3f, 0.2f, 0.9f),
      m_IsColliding(false),
      m_CollisionPoint(0.0f, 0.0f),
      m_CollisionNormal(0.0f, 0.0f),
      m_Collider(nullptr) {
}

void RayCast2D::DefineProperties() {
    DefineProperty(PROPERTY_DEFAULT_GROUP(targetPosition, Vec2, math::Vec2(0.0f, 50.0f), "RayCast"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(excludeParent, Bool, true, "RayCast"));
    DefineProperty(PROPERTY_GROUP(collisionMask, Int, static_cast<int>(0xFFFFFFFF), Layers2D, "", "Collision"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(visibleInGame, Bool, false, "Debug"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(debugColor, Color, math::Color(0.0f, 0.6f, 0.7f, 0.8f), "Debug"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(debugColorHit, Color, math::Color(1.0f, 0.3f, 0.2f, 0.9f), "Debug"));
}

void RayCast2D::SyncFromProperties() {
    m_TargetPosition = GetPropertyValue<math::Vec2>("targetPosition");
    m_ExcludeParent = GetPropertyValue<bool>("excludeParent");
    m_CollisionMask = static_cast<uint32_t>(GetPropertyValue<int>("collisionMask"));
    m_VisibleInGame = GetPropertyValue<bool>("visibleInGame");
    m_DebugColor = GetPropertyValue<math::Color>("debugColor");
    m_DebugColorHit = GetPropertyValue<math::Color>("debugColorHit");
}

void RayCast2D::OnReady() {
    SyncFromProperties();
}

void RayCast2D::OnPhysicsProcess(float /*deltaTime*/) {
    if (!IsEnabled()) {
        m_IsColliding = false;
        m_Collider = nullptr;
        return;
    }
    UpdateRaycast();
}

void RayCast2D::OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) {
    if (propertyName == "targetPosition") {
        m_TargetPosition = math::Vec2(newValue["x"].get<float>(), newValue["y"].get<float>());
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

void RayCast2D::SetTargetPosition(const math::Vec2& target) {
    m_TargetPosition = target;
    SetPropertyValue("targetPosition", target);
}

void RayCast2D::SetCollisionMask(uint32_t mask) {
    m_CollisionMask = mask;
    SetPropertyValue("collisionMask", static_cast<int>(mask));
}

void RayCast2D::ForceRaycastUpdate() {
    UpdateRaycast();
}

void RayCast2D::ComputeWorldRay(math::Vec2& outOrigin, math::Vec2& outTarget) const {
    auto* node2D = dynamic_cast<Node2D*>(m_Owner);
    if (!node2D) {
        outOrigin = math::Vec2(0.0f, 0.0f);
        outTarget = m_TargetPosition;
        return;
    }

    outOrigin = node2D->GetGlobalPosition();

    float rot = node2D->GetGlobalRotation();
    float c = std::cos(rot);
    float s = std::sin(rot);
    math::Vec2 rotated(
        m_TargetPosition.x * c - m_TargetPosition.y * s,
        m_TargetPosition.x * s + m_TargetPosition.y * c);
    outTarget = outOrigin + rotated;
}

bool RayCast2D::IsExcludedNode(const Node* hitNode) const {
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

void RayCast2D::UpdateRaycast() {
    m_IsColliding = false;
    m_Collider = nullptr;
    m_ColliderBodyId = UUID(0);

    auto* sceneManager = SceneManager::GetInstance();
    if (!sceneManager) return;

    auto* physicsWorld = sceneManager->GetPhysics2DWorld();
    if (!physicsWorld) return;

    math::Vec2 origin, target;
    ComputeWorldRay(origin, target);

    math::Vec2 delta = target - origin;
    float maxDistance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    if (maxDistance < 1e-5f) return;

    math::Vec2 direction(delta.x / maxDistance, delta.y / maxDistance);

    std::vector<physics2d::RaycastHit2D> hits =
        physicsWorld->RaycastAll(origin, direction, maxDistance, nullptr, m_CollisionMask);

    if (hits.empty()) return;

    std::sort(hits.begin(), hits.end(),
              [](const physics2d::RaycastHit2D& a, const physics2d::RaycastHit2D& b) {
                  return a.fraction < b.fraction;
              });

    for (const physics2d::RaycastHit2D& hit : hits) {
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

nlohmann::json RayCast2D::CallMethod(const std::string& method, const nlohmann::json& args) {
    auto argF = [&](size_t i, float fallback) -> float {
        if (args.is_array() && i < args.size() && args[i].is_number()) {
            return args[i].get<float>();
        }
        return fallback;
    };
    auto argI = [&](size_t i, int fallback) -> int {
        if (args.is_array() && i < args.size() && args[i].is_number_integer()) {
            return args[i].get<int>();
        }
        return fallback;
    };
    auto argB = [&](size_t i, bool fallback) -> bool {
        if (args.is_array() && i < args.size() && args[i].is_boolean()) {
            return args[i].get<bool>();
        }
        return fallback;
    };
    auto vec2Json = [](const Vec2& v) -> nlohmann::json {
        nlohmann::json o;
        o["x"] = v.x; o["y"] = v.y;
        return o;
    };

    if (method == "is_colliding") {
        return m_IsColliding;
    } else if (method == "get_collider") {
        return Node::NodeArg(m_Collider);
    } else if (method == "get_collision_point") {
        return vec2Json(m_CollisionPoint);
    } else if (method == "get_collision_normal") {
        return vec2Json(m_CollisionNormal);
    } else if (method == "force_raycast_update") {
        ForceRaycastUpdate();
        return m_IsColliding;
    } else if (method == "set_target_position") {
        SetTargetPosition(Vec2(argF(0, 0.0f), argF(1, 0.0f)));
    } else if (method == "get_target_position") {
        return vec2Json(m_TargetPosition);
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

void RayCast2D::buildDrawCommands(RenderContext& ctx) {
    if (!m_Owner) return;

    SyncFromProperties();

    RenderCamera* camera = ctx.getCamera();
    bool editorView = camera && camera->isEditorCamera;
    if (!editorView && !m_VisibleInGame) {
        return;
    }

    math::Vec2 origin, target;
    ComputeWorldRay(origin, target);

    const math::Color& lineColor = m_IsColliding ? m_DebugColorHit : m_DebugColor;
    ctx.drawLine(Vec3(origin.x, origin.y, 0.0f), Vec3(target.x, target.y, 0.0f), lineColor, 1.5f);

    float tipRadius = 4.0f;
    ctx.drawCircle(Vec3(target.x, target.y, 0.0f), tipRadius, m_DebugColor, false);

    if (m_IsColliding) {
        ctx.drawCircle(Vec3(m_CollisionPoint.x, m_CollisionPoint.y, 0.0f), tipRadius, m_DebugColorHit, true);
        math::Vec2 normalEnd = m_CollisionPoint + m_CollisionNormal * 16.0f;
        ctx.drawLine(Vec3(m_CollisionPoint.x, m_CollisionPoint.y, 0.0f),
                     Vec3(normalEnd.x, normalEnd.y, 0.0f), m_DebugColorHit, 1.0f);
    }
}

AABB RayCast2D::getWorldBounds() const {
    math::Vec2 origin, target;
    ComputeWorldRay(origin, target);

    float minX = std::min(origin.x, target.x);
    float minY = std::min(origin.y, target.y);
    float maxX = std::max(origin.x, target.x);
    float maxY = std::max(origin.y, target.y);

    float padding = 16.0f;
    return AABB(
        Vec3(minX - padding, minY - padding, -0.1f),
        Vec3(maxX + padding, maxY + padding, 0.1f));
}

RenderLayer RayCast2D::getRenderLayer() const {
    return RenderLayer::Transparent;
}

SpatialType RayCast2D::getSpatialType() const {
    return SpatialType::World2D;
}

} // namespace components
} // namespace lupine
