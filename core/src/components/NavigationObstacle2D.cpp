#include "lupine/components/NavigationObstacle2D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/navigation/NavigationServer2D.hpp"
#include "lupine/rendering/RenderContext.hpp"

#include <algorithm>
#include <cmath>

namespace lupine {
namespace components {

using namespace core;
using math::Vec2;

namespace {

constexpr float kTransformEpsilon = 1e-3f;

std::vector<Vec2> ParseFlatVertices(const nlohmann::json& j) {
    std::vector<Vec2> result;
    if (!j.is_array()) {
        return result;
    }
    for (size_t i = 0; i + 1 < j.size(); i += 2) {
        if (j[i].is_number() && j[i + 1].is_number()) {
            result.push_back(Vec2(j[i].get<float>(), j[i + 1].get<float>()));
        }
    }
    return result;
}

nlohmann::json ToFlatVertices(const std::vector<Vec2>& verts) {
    nlohmann::json arr = nlohmann::json::array();
    for (const Vec2& v : verts) {
        arr.push_back(v.x);
        arr.push_back(v.y);
    }
    return arr;
}

} // namespace

NavigationObstacle2D::NavigationObstacle2D()
    : Component("NavigationObstacle2D") {}

NavigationObstacle2D::NavigationObstacle2D(const std::string& name)
    : Component(name) {}

NavigationObstacle2D::~NavigationObstacle2D() {
    UnregisterAll();
}

void NavigationObstacle2D::DefineProperties() {
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(radius, 32.0f, 0.0f, 10000.0f, 0.5f, "Navigation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(avoidanceEnabled, Bool, true, "Navigation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(carveNavMesh, Bool, false, "Navigation"));
    DefineProperty(PropertyDescriptor("vertices", PropertyValueType::FloatArray,
        PropertyJsonDefault(nlohmann::json::array()), PropertyHint(), "Navigation"));
}

float NavigationObstacle2D::GetRadius() const { return GetPropertyValue<float>("radius"); }
void NavigationObstacle2D::SetRadius(float radius) { SetPropertyValue<float>("radius", radius); }
bool NavigationObstacle2D::GetAvoidanceEnabled() const { return GetPropertyValue<bool>("avoidanceEnabled"); }
void NavigationObstacle2D::SetAvoidanceEnabled(bool enabled) { SetPropertyValue<bool>("avoidanceEnabled", enabled); }
bool NavigationObstacle2D::GetCarveNavMesh() const { return GetPropertyValue<bool>("carveNavMesh"); }
void NavigationObstacle2D::SetCarveNavMesh(bool carve) { SetPropertyValue<bool>("carveNavMesh", carve); }

void NavigationObstacle2D::SetVertices(const std::vector<Vec2>& vertices) {
    ComponentProperty* prop = GetPropertyRegistry().GetProperty("vertices");
    if (prop) {
        prop->SetValueFromJson(ToFlatVertices(vertices));
    }
}

std::vector<Vec2> NavigationObstacle2D::GetVertices() const {
    const ComponentProperty* prop = GetPropertyRegistry().GetProperty("vertices");
    if (!prop) {
        return {};
    }
    return ParseFlatVertices(prop->GetValueAsJson());
}

std::string NavigationObstacle2D::ObstacleId() const {
    return GetUUID().ToString();
}

bool NavigationObstacle2D::OwnerTransformChanged() {
    Node2D* owner = dynamic_cast<Node2D*>(GetOwner());
    if (!owner || !m_HasCachedTransform) {
        return false;
    }
    Vec2 pos = owner->GetGlobalPosition();
    float rot = owner->GetGlobalRotation();
    Vec2 scale = owner->GetGlobalScale();
    return std::fabs(pos.x - m_CachedPosition.x) > kTransformEpsilon ||
           std::fabs(pos.y - m_CachedPosition.y) > kTransformEpsilon ||
           std::fabs(rot - m_CachedRotation) > kTransformEpsilon ||
           std::fabs(scale.x - m_CachedScale.x) > kTransformEpsilon ||
           std::fabs(scale.y - m_CachedScale.y) > kTransformEpsilon;
}

void NavigationObstacle2D::UpdateCachedTransform() {
    Node2D* owner = dynamic_cast<Node2D*>(GetOwner());
    if (!owner) {
        m_HasCachedTransform = false;
        return;
    }
    m_HasCachedTransform = true;
    m_CachedPosition = owner->GetGlobalPosition();
    m_CachedRotation = owner->GetGlobalRotation();
    m_CachedScale = owner->GetGlobalScale();
}

void NavigationObstacle2D::Bake() {
    navigation::NavigationServer2D& server = navigation::NavigationServer2D::GetInstance();

    std::vector<Vec2> verticesLocal = GetVertices();
    if (!GetCarveNavMesh() || !IsEnabled() || verticesLocal.size() < 3) {
        if (m_CarveRegistered) {
            server.RemoveCarveObstacle(ObstacleId());
            m_CarveRegistered = false;
        }
        return;
    }

    Node2D* owner = dynamic_cast<Node2D*>(GetOwner());
    Vec2 gp = Vec2::Zero();
    float gr = 0.0f;
    Vec2 gs = Vec2::One();
    if (owner) {
        gp = owner->GetGlobalPosition();
        gr = owner->GetGlobalRotation();
        gs = owner->GetGlobalScale();
    }
    float cs = std::cos(gr);
    float sn = std::sin(gr);

    std::vector<Vec2> world;
    world.reserve(verticesLocal.size());
    for (const Vec2& local : verticesLocal) {
        Vec2 scaled(local.x * gs.x, local.y * gs.y);
        Vec2 rotated(scaled.x * cs - scaled.y * sn, scaled.x * sn + scaled.y * cs);
        world.push_back(gp + rotated);
    }

    server.UpdateCarveObstacle(ObstacleId(), world, true);
    m_CarveRegistered = true;
}

void NavigationObstacle2D::RefreshAvoidance() {
    navigation::NavigationServer2D& server = navigation::NavigationServer2D::GetInstance();
    Node2D* owner = dynamic_cast<Node2D*>(GetOwner());

    bool active = GetAvoidanceEnabled() && IsEnabled() && GetRadius() > 0.0f;
    if (!active) {
        if (m_AvoidanceRegistered) {
            server.RemoveAvoidanceObstacle(ObstacleId());
            m_AvoidanceRegistered = false;
        }
        return;
    }

    Vec2 pos = owner ? owner->GetGlobalPosition() : Vec2::Zero();
    Vec2 scale = owner ? owner->GetGlobalScale() : Vec2::One();
    float worldRadius = GetRadius() * (std::fabs(scale.x) + std::fabs(scale.y)) * 0.5f;
    server.UpdateAvoidanceObstacle(ObstacleId(), pos, worldRadius);
    m_AvoidanceRegistered = true;
}

void NavigationObstacle2D::UnregisterAll() {
    navigation::NavigationServer2D& server = navigation::NavigationServer2D::GetInstance();
    if (m_AvoidanceRegistered) {
        server.RemoveAvoidanceObstacle(ObstacleId());
        m_AvoidanceRegistered = false;
    }
    if (m_CarveRegistered) {
        server.RemoveCarveObstacle(ObstacleId());
        m_CarveRegistered = false;
    }
}

void NavigationObstacle2D::OnReady() {
    Bake();
    RefreshAvoidance();
    UpdateCachedTransform();
}

void NavigationObstacle2D::OnUpdate(float deltaTime) {
    (void)deltaTime;
    RefreshAvoidance();
    if (OwnerTransformChanged()) {
        Bake();
        UpdateCachedTransform();
    }
}

void NavigationObstacle2D::OnDestroy() {
    UnregisterAll();
}

void NavigationObstacle2D::OnExitTree() {
    UnregisterAll();
}

void NavigationObstacle2D::OnPropertyChanged(const std::string& propertyName,
                                             const nlohmann::json& newValue) {
    (void)newValue;
    if (propertyName == "vertices" || propertyName == "carveNavMesh" || propertyName == "enabled") {
        Bake();
    }
    if (propertyName == "radius" || propertyName == "avoidanceEnabled" || propertyName == "enabled") {
        RefreshAvoidance();
    }
}

nlohmann::json NavigationObstacle2D::CallMethod(const std::string& method,
                                                const nlohmann::json& args) {
    auto argAt = [&](size_t i) -> nlohmann::json {
        if (args.is_array() && i < args.size()) {
            return args[i];
        }
        return nlohmann::json();
    };

    if (method == "set_radius") {
        nlohmann::json a = argAt(0);
        SetRadius(a.is_number() ? a.get<float>() : 32.0f);
        RefreshAvoidance();
    } else if (method == "get_radius") {
        return GetRadius();
    } else if (method == "set_avoidance_enabled") {
        nlohmann::json a = argAt(0);
        SetAvoidanceEnabled(a.is_boolean() ? a.get<bool>() : true);
        RefreshAvoidance();
    } else if (method == "is_avoidance_enabled") {
        return GetAvoidanceEnabled();
    } else if (method == "set_carve") {
        nlohmann::json a = argAt(0);
        SetCarveNavMesh(a.is_boolean() ? a.get<bool>() : false);
        Bake();
    } else if (method == "set_vertices") {
        SetVertices(ParseFlatVertices(argAt(0)));
        Bake();
    } else if (method == "get_vertices") {
        return ToFlatVertices(GetVertices());
    } else if (method == "bake" || method == "rebake") {
        Bake();
        RefreshAvoidance();
    }
    return nlohmann::json();
}

math::Vec2 NavigationObstacle2D::LocalToWorld(const Vec2& local) const {
    Node2D* owner = dynamic_cast<Node2D*>(GetOwner());
    if (!owner) {
        return local;
    }
    Vec2 gp = owner->GetGlobalPosition();
    float gr = owner->GetGlobalRotation();
    Vec2 gs = owner->GetGlobalScale();
    float cs = std::cos(gr);
    float sn = std::sin(gr);
    Vec2 scaled(local.x * gs.x, local.y * gs.y);
    Vec2 rotated(scaled.x * cs - scaled.y * sn, scaled.x * sn + scaled.y * cs);
    return gp + rotated;
}

void NavigationObstacle2D::buildDrawCommands(RenderContext& ctx) {
    Scene* scene = GetOwner() ? GetOwner()->GetScene() : nullptr;
    if (!scene || !scene->IsInEditor()) {
        return; // editor-only visualization
    }

    Node2D* owner = dynamic_cast<Node2D*>(GetOwner());
    Vec2 center = owner ? owner->GetGlobalPosition() : Vec2::Zero();

    if (GetAvoidanceEnabled() && GetRadius() > 0.0f) {
        Vec2 scale = owner ? owner->GetGlobalScale() : Vec2::One();
        float worldRadius = GetRadius() * (std::fabs(scale.x) + std::fabs(scale.y)) * 0.5f;
        ctx.drawCircle(math::Vec3(center.x, center.y, 0.0f), worldRadius,
                       math::Color(0.95f, 0.6f, 0.15f, 0.7f), false);
    }

    if (GetCarveNavMesh()) {
        std::vector<Vec2> verts = GetVertices();
        size_t n = verts.size();
        math::Color carveColor(0.9f, 0.35f, 0.25f, 0.85f);
        for (size_t i = 0; n >= 2 && i < n; ++i) {
            Vec2 a = LocalToWorld(verts[i]);
            Vec2 b = LocalToWorld(verts[(i + 1) % n]);
            ctx.drawLine(math::Vec3(a.x, a.y, 0.0f), math::Vec3(b.x, b.y, 0.0f), carveColor, 2.0f);
        }
    }
}

AABB NavigationObstacle2D::getWorldBounds() const {
    Node2D* owner = dynamic_cast<Node2D*>(GetOwner());
    Vec2 center = owner ? owner->GetGlobalPosition() : Vec2::Zero();

    float minX = center.x;
    float minY = center.y;
    float maxX = center.x;
    float maxY = center.y;

    Vec2 scale = owner ? owner->GetGlobalScale() : Vec2::One();
    float worldRadius = GetRadius() * (std::fabs(scale.x) + std::fabs(scale.y)) * 0.5f;
    minX = std::min(minX, center.x - worldRadius);
    minY = std::min(minY, center.y - worldRadius);
    maxX = std::max(maxX, center.x + worldRadius);
    maxY = std::max(maxY, center.y + worldRadius);

    for (const Vec2& local : GetVertices()) {
        Vec2 w = LocalToWorld(local);
        minX = std::min(minX, w.x);
        minY = std::min(minY, w.y);
        maxX = std::max(maxX, w.x);
        maxY = std::max(maxY, w.y);
    }
    return AABB(math::Vec3(minX, minY, -1.0f), math::Vec3(maxX, maxY, 1.0f));
}

RenderLayer NavigationObstacle2D::getRenderLayer() const {
    return RenderLayer::Transparent;
}

SpatialType NavigationObstacle2D::getSpatialType() const {
    return SpatialType::World2D;
}

} // namespace components
} // namespace lupine
