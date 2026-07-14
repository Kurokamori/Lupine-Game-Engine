#include "lupine/components/NavigationObstacle3D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/navigation/NavigationServer3D.hpp"
#include "lupine/rendering/RenderContext.hpp"

#include <cmath>

namespace lupine {
namespace components {

using namespace core;
using math::Vec3;
using math::AABB;

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kMoveEpsilon = 1e-3f;

float ArgFloat(const nlohmann::json& args, size_t i, float def = 0.0f) {
    if (args.is_array() && i < args.size() && args[i].is_number()) {
        return args[i].get<float>();
    }
    return def;
}

bool ArgBool(const nlohmann::json& args, size_t i, bool def = false) {
    if (args.is_array() && i < args.size() && args[i].is_boolean()) {
        return args[i].get<bool>();
    }
    return def;
}

} // namespace

NavigationObstacle3D::NavigationObstacle3D()
    : Component("NavigationObstacle3D") {}

NavigationObstacle3D::NavigationObstacle3D(const std::string& name)
    : Component(name) {}

NavigationObstacle3D::~NavigationObstacle3D() {
    UnregisterAll();
}

void NavigationObstacle3D::DefineProperties() {
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(radius, 1.0f, 0.0f, 1000.0f, 0.1f, "Avoidance"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(avoidanceEnabled, Bool, true, "Avoidance"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(height, 2.0f, 0.1f, 1000.0f, 0.1f, "Carve"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(carveNavMesh, Bool, false, "Carve"));
}

float NavigationObstacle3D::GetRadius() const { return GetPropertyValue<float>("radius"); }
void NavigationObstacle3D::SetRadius(float radius) { SetPropertyValue<float>("radius", radius); }
float NavigationObstacle3D::GetHeight() const { return GetPropertyValue<float>("height"); }
void NavigationObstacle3D::SetHeight(float height) { SetPropertyValue<float>("height", height); }
bool NavigationObstacle3D::GetAvoidanceEnabled() const { return GetPropertyValue<bool>("avoidanceEnabled"); }
void NavigationObstacle3D::SetAvoidanceEnabled(bool enabled) { SetPropertyValue<bool>("avoidanceEnabled", enabled); }
bool NavigationObstacle3D::GetCarveNavMesh() const { return GetPropertyValue<bool>("carveNavMesh"); }
void NavigationObstacle3D::SetCarveNavMesh(bool carve) { SetPropertyValue<bool>("carveNavMesh", carve); }

std::string NavigationObstacle3D::ObstacleId() const {
    return GetUUID().ToString();
}

AABB NavigationObstacle3D::BuildCarveBox() const {
    Node3D* owner = dynamic_cast<Node3D*>(GetOwner());
    Vec3 center = owner ? owner->GetGlobalPosition() : Vec3::Zero();
    Vec3 scale = owner ? owner->GetGlobalScale() : Vec3::One();
    float r = GetRadius();
    float halfH = GetHeight() * 0.5f;
    Vec3 he(r * std::fabs(scale.x), halfH * std::fabs(scale.y), r * std::fabs(scale.z));
    return AABB(center - he, center + he);
}

void NavigationObstacle3D::RefreshAvoidance() {
    navigation::NavigationServer3D& server = navigation::NavigationServer3D::GetInstance();
    if (GetAvoidanceEnabled() && IsEnabled()) {
        Node3D* owner = dynamic_cast<Node3D*>(GetOwner());
        Vec3 pos = owner ? owner->GetGlobalPosition() : Vec3::Zero();
        Vec3 scale = owner ? owner->GetGlobalScale() : Vec3::One();
        float r = GetRadius() * std::max(std::fabs(scale.x), std::fabs(scale.z));
        server.UpdateAvoidanceObstacle(ObstacleId(), pos, r);
        m_AvoidanceRegistered = true;
    } else if (m_AvoidanceRegistered) {
        server.RemoveAvoidanceObstacle(ObstacleId());
        m_AvoidanceRegistered = false;
    }
}

void NavigationObstacle3D::RefreshCarve(bool force) {
    navigation::NavigationServer3D& server = navigation::NavigationServer3D::GetInstance();
    if (GetCarveNavMesh() && IsEnabled()) {
        if (force || OwnerMoved() || !m_CarveRegistered) {
            server.UpdateCarveVolume(ObstacleId(), BuildCarveBox(), true);
            m_CarveRegistered = true;
            Node3D* owner = dynamic_cast<Node3D*>(GetOwner());
            m_CachedPosition = owner ? owner->GetGlobalPosition() : Vec3::Zero();
            m_HasCachedPosition = true;
        }
    } else if (m_CarveRegistered) {
        server.RemoveCarveVolume(ObstacleId());
        m_CarveRegistered = false;
    }
}

bool NavigationObstacle3D::OwnerMoved() {
    Node3D* owner = dynamic_cast<Node3D*>(GetOwner());
    if (!owner || !m_HasCachedPosition) {
        return true;
    }
    Vec3 pos = owner->GetGlobalPosition();
    return (pos - m_CachedPosition).LengthSquared() > kMoveEpsilon * kMoveEpsilon;
}

void NavigationObstacle3D::UnregisterAll() {
    navigation::NavigationServer3D& server = navigation::NavigationServer3D::GetInstance();
    if (m_AvoidanceRegistered) {
        server.RemoveAvoidanceObstacle(ObstacleId());
        m_AvoidanceRegistered = false;
    }
    if (m_CarveRegistered) {
        server.RemoveCarveVolume(ObstacleId());
        m_CarveRegistered = false;
    }
}

void NavigationObstacle3D::Bake() {
    RefreshAvoidance();
    RefreshCarve(true);
}

void NavigationObstacle3D::OnReady() {
    RefreshAvoidance();
    RefreshCarve(true);
}

void NavigationObstacle3D::OnUpdate(float deltaTime) {
    (void)deltaTime;
    RefreshAvoidance();
    RefreshCarve(false);
}

void NavigationObstacle3D::OnDestroy() {
    UnregisterAll();
}

void NavigationObstacle3D::OnExitTree() {
    UnregisterAll();
}

void NavigationObstacle3D::OnPropertyChanged(const std::string& propertyName,
                                             const nlohmann::json& newValue) {
    (void)newValue;
    if (propertyName == "radius" || propertyName == "height" ||
        propertyName == "avoidanceEnabled" || propertyName == "carveNavMesh" ||
        propertyName == "enabled") {
        RefreshAvoidance();
        RefreshCarve(true);
    }
}

nlohmann::json NavigationObstacle3D::CallMethod(const std::string& method,
                                                const nlohmann::json& args) {
    if (method == "set_radius") {
        SetRadius(ArgFloat(args, 0, 1.0f));
        Bake();
    } else if (method == "get_radius") {
        return GetRadius();
    } else if (method == "set_height") {
        SetHeight(ArgFloat(args, 0, 2.0f));
        Bake();
    } else if (method == "get_height") {
        return GetHeight();
    } else if (method == "set_avoidance_enabled") {
        SetAvoidanceEnabled(ArgBool(args, 0, true));
        RefreshAvoidance();
    } else if (method == "is_avoidance_enabled") {
        return GetAvoidanceEnabled();
    } else if (method == "set_carve") {
        SetCarveNavMesh(ArgBool(args, 0, false));
        RefreshCarve(true);
    } else if (method == "get_carve") {
        return GetCarveNavMesh();
    } else if (method == "bake" || method == "rebake") {
        Bake();
    }
    return nlohmann::json();
}

void NavigationObstacle3D::buildDrawCommands(RenderContext& ctx) {
    Scene* scene = GetOwner() ? GetOwner()->GetScene() : nullptr;
    if (!scene || !scene->IsInEditor()) {
        return; // editor-only visualization
    }
    Node3D* owner = dynamic_cast<Node3D*>(GetOwner());
    Vec3 center = owner ? owner->GetGlobalPosition() : Vec3::Zero();
    float r = GetRadius();

    if (GetAvoidanceEnabled()) {
        math::Color ringColor(0.95f, 0.65f, 0.2f, 0.85f);
        const int segs = 32;
        Vec3 prev(center.x + r, center.y, center.z);
        for (int i = 1; i <= segs; ++i) {
            float a = 2.0f * kPi * (static_cast<float>(i) / segs);
            Vec3 cur(center.x + r * std::cos(a), center.y, center.z + r * std::sin(a));
            ctx.drawLine(prev, cur, ringColor, 1.5f);
            prev = cur;
        }
    }

    if (GetCarveNavMesh()) {
        math::Color boxColor(0.9f, 0.3f, 0.25f, 0.85f);
        AABB box = BuildCarveBox();
        Vec3 c[8];
        for (int i = 0; i < 8; ++i) {
            c[i] = box.GetCorner(i);
        }
        const int edges[12][2] = {
            {0, 1}, {1, 3}, {3, 2}, {2, 0},
            {4, 5}, {5, 7}, {7, 6}, {6, 4},
            {0, 4}, {1, 5}, {2, 6}, {3, 7}};
        for (auto& e : edges) {
            ctx.drawLine(c[e[0]], c[e[1]], boxColor, 1.5f);
        }
    }
}

AABB NavigationObstacle3D::getWorldBounds() const {
    return BuildCarveBox();
}

RenderLayer NavigationObstacle3D::getRenderLayer() const {
    return RenderLayer::Transparent;
}

SpatialType NavigationObstacle3D::getSpatialType() const {
    return SpatialType::World3D;
}

} // namespace components
} // namespace lupine
