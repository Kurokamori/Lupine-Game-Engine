#include "lupine/components/NavigationRegion2D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/navigation/NavigationServer2D.hpp"
#include "lupine/rendering/RenderContext.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

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

NavigationRegion2D::NavigationRegion2D()
    : Component("NavigationRegion2D") {}

NavigationRegion2D::NavigationRegion2D(const std::string& name)
    : Component(name) {}

NavigationRegion2D::~NavigationRegion2D() {
    Unregister();
}

void NavigationRegion2D::DefineProperties() {
    DefineProperty(PropertyDescriptor("outline", PropertyValueType::FloatArray,
        PropertyJsonDefault(nlohmann::json::array()), PropertyHint(), "Navigation"));
    DefineProperty(PropertyDescriptor("holes", PropertyValueType::Array,
        PropertyJsonDefault(nlohmann::json::array()), PropertyHint(), "Navigation"));
}

void NavigationRegion2D::DefineSignals() {
    RegisterSignal({"baked", {}, "Emitted after the region geometry is re-triangulated and published."});
}

void NavigationRegion2D::OnReady() {
    Bake();
}

void NavigationRegion2D::OnUpdate(float deltaTime) {
    (void)deltaTime;
    if (OwnerTransformChanged()) {
        Bake();
    }
}

void NavigationRegion2D::OnDestroy() {
    Unregister();
}

void NavigationRegion2D::OnExitTree() {
    Unregister();
}

void NavigationRegion2D::OnPropertyChanged(const std::string& propertyName,
                                           const nlohmann::json& newValue) {
    (void)newValue;
    if (propertyName == "outline" || propertyName == "holes" || propertyName == "enabled") {
        Bake();
    }
}

bool NavigationRegion2D::GetEnabledRegion() const {
    return IsEnabled();
}

void NavigationRegion2D::SetEnabledRegion(bool enabled) {
    SetEnabled(enabled);
    Bake();
}

void NavigationRegion2D::SetOutline(const std::vector<Vec2>& outline) {
    ComponentProperty* prop = GetPropertyRegistry().GetProperty("outline");
    if (prop) {
        prop->SetValueFromJson(ToFlatVertices(outline));
    }
}

std::vector<Vec2> NavigationRegion2D::GetOutline() const {
    const ComponentProperty* prop = GetPropertyRegistry().GetProperty("outline");
    if (!prop) {
        return {};
    }
    return ParseFlatVertices(prop->GetValueAsJson());
}

void NavigationRegion2D::SetHoles(const std::vector<std::vector<Vec2>>& holes) {
    ComponentProperty* prop = GetPropertyRegistry().GetProperty("holes");
    if (!prop) {
        return;
    }
    nlohmann::json arr = nlohmann::json::array();
    for (const std::vector<Vec2>& hole : holes) {
        arr.push_back(ToFlatVertices(hole));
    }
    prop->SetValueFromJson(arr);
}

std::vector<std::vector<Vec2>> NavigationRegion2D::GetHoles() const {
    std::vector<std::vector<Vec2>> result;
    const ComponentProperty* prop = GetPropertyRegistry().GetProperty("holes");
    if (!prop) {
        return result;
    }
    nlohmann::json j = prop->GetValueAsJson();
    if (!j.is_array()) {
        return result;
    }
    for (const nlohmann::json& hole : j) {
        std::vector<Vec2> verts = ParseFlatVertices(hole);
        if (verts.size() >= 3) {
            result.push_back(std::move(verts));
        }
    }
    return result;
}

void NavigationRegion2D::AddHole(const std::vector<Vec2>& hole) {
    std::vector<std::vector<Vec2>> holes = GetHoles();
    holes.push_back(hole);
    SetHoles(holes);
}

void NavigationRegion2D::ClearHoles() {
    SetHoles({});
}

std::string NavigationRegion2D::RegionId() const {
    return GetUUID().ToString();
}

void NavigationRegion2D::Unregister() {
    if (m_Registered) {
        navigation::NavigationServer2D::GetInstance().RemoveRegion(RegionId());
        m_Registered = false;
    }
}

bool NavigationRegion2D::OwnerTransformChanged() {
    Node2D* owner = dynamic_cast<Node2D*>(GetOwner());
    if (!owner) {
        return false;
    }
    Vec2 pos = owner->GetGlobalPosition();
    float rot = owner->GetGlobalRotation();
    Vec2 scale = owner->GetGlobalScale();
    if (!m_HasCachedTransform) {
        return false;
    }
    return std::fabs(pos.x - m_CachedPosition.x) > kTransformEpsilon ||
           std::fabs(pos.y - m_CachedPosition.y) > kTransformEpsilon ||
           std::fabs(rot - m_CachedRotation) > kTransformEpsilon ||
           std::fabs(scale.x - m_CachedScale.x) > kTransformEpsilon ||
           std::fabs(scale.y - m_CachedScale.y) > kTransformEpsilon;
}

void NavigationRegion2D::Bake() {
    Node2D* owner = dynamic_cast<Node2D*>(GetOwner());

    std::vector<Vec2> outlineLocal = GetOutline();
    if (outlineLocal.size() < 3) {
        m_BakedPolygonCount = 0;
        Unregister();
        return;
    }

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
    auto toWorld = [&](const Vec2& local) -> Vec2 {
        Vec2 scaled(local.x * gs.x, local.y * gs.y);
        Vec2 rotated(scaled.x * cs - scaled.y * sn, scaled.x * sn + scaled.y * cs);
        return gp + rotated;
    };

    std::vector<Vec2> outlineWorld;
    outlineWorld.reserve(outlineLocal.size());
    for (const Vec2& v : outlineLocal) {
        outlineWorld.push_back(toWorld(v));
    }

    std::vector<std::vector<Vec2>> holesWorld;
    for (const std::vector<Vec2>& hole : GetHoles()) {
        std::vector<Vec2> world;
        world.reserve(hole.size());
        for (const Vec2& v : hole) {
            world.push_back(toWorld(v));
        }
        holesWorld.push_back(std::move(world));
    }

    navigation::NavigationServer2D& server = navigation::NavigationServer2D::GetInstance();
    server.UpdateRegion(RegionId(), outlineWorld, holesWorld, GetEnabledRegion());
    m_Registered = true;
    m_BakedPolygonCount = server.GetRegionPolygonCount(RegionId());

    m_HasCachedTransform = true;
    m_CachedPosition = gp;
    m_CachedRotation = gr;
    m_CachedScale = gs;

    Emit("baked");
}

nlohmann::json NavigationRegion2D::CallMethod(const std::string& method,
                                              const nlohmann::json& args) {
    auto argAt = [&](size_t i) -> nlohmann::json {
        if (args.is_array() && i < args.size()) {
            return args[i];
        }
        return nlohmann::json();
    };

    if (method == "bake" || method == "rebake") {
        Bake();
    } else if (method == "set_enabled") {
        nlohmann::json a = argAt(0);
        SetEnabledRegion(a.is_boolean() ? a.get<bool>() : true);
    } else if (method == "is_enabled") {
        return GetEnabledRegion();
    } else if (method == "set_outline") {
        SetOutline(ParseFlatVertices(argAt(0)));
        Bake();
    } else if (method == "get_outline") {
        return ToFlatVertices(GetOutline());
    } else if (method == "add_hole") {
        AddHole(ParseFlatVertices(argAt(0)));
        Bake();
    } else if (method == "clear_holes") {
        ClearHoles();
        Bake();
    } else if (method == "get_polygon_count") {
        return static_cast<int>(m_BakedPolygonCount);
    }
    return nlohmann::json();
}

math::Vec2 NavigationRegion2D::LocalToWorld(const Vec2& local) const {
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

void NavigationRegion2D::buildDrawCommands(RenderContext& ctx) {
    Scene* scene = GetOwner() ? GetOwner()->GetScene() : nullptr;
    if (!scene || !scene->IsInEditor()) {
        return; // editor-only visualization
    }

    std::vector<Vec2> outline = GetOutline();
    if (outline.size() < 2) {
        return;
    }

    math::Color outlineColor = GetEnabledRegion()
        ? math::Color(0.25f, 0.85f, 0.45f, 0.85f)
        : math::Color(0.5f, 0.5f, 0.5f, 0.6f);
    math::Color holeColor(0.9f, 0.35f, 0.25f, 0.85f);

    auto drawRing = [&](const std::vector<Vec2>& ring, const math::Color& color) {
        size_t n = ring.size();
        for (size_t i = 0; i < n; ++i) {
            Vec2 a = LocalToWorld(ring[i]);
            Vec2 b = LocalToWorld(ring[(i + 1) % n]);
            ctx.drawLine(math::Vec3(a.x, a.y, 0.0f), math::Vec3(b.x, b.y, 0.0f), color, 2.0f);
        }
    };

    drawRing(outline, outlineColor);
    for (const std::vector<Vec2>& hole : GetHoles()) {
        if (hole.size() >= 2) {
            drawRing(hole, holeColor);
        }
    }
}

AABB NavigationRegion2D::getWorldBounds() const {
    std::vector<Vec2> outline = GetOutline();
    if (outline.empty()) {
        return AABB();
    }
    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float maxX = -std::numeric_limits<float>::max();
    float maxY = -std::numeric_limits<float>::max();
    for (const Vec2& local : outline) {
        Vec2 w = LocalToWorld(local);
        minX = std::min(minX, w.x);
        minY = std::min(minY, w.y);
        maxX = std::max(maxX, w.x);
        maxY = std::max(maxY, w.y);
    }
    return AABB(math::Vec3(minX, minY, -1.0f), math::Vec3(maxX, maxY, 1.0f));
}

RenderLayer NavigationRegion2D::getRenderLayer() const {
    return RenderLayer::Transparent;
}

SpatialType NavigationRegion2D::getSpatialType() const {
    return SpatialType::World2D;
}

} // namespace components
} // namespace lupine
