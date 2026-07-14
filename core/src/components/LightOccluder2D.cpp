#include "lupine/components/LightOccluder2D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/rendering/RenderContext.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace lupine {
namespace components {

using namespace core;
using math::Vec2;
using math::Vec3;
using math::Color;

namespace {

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

LightOccluder2D::LightOccluder2D()
    : Component("LightOccluder2D") {}

LightOccluder2D::LightOccluder2D(const std::string& name)
    : Component(name) {}

LightOccluder2D::~LightOccluder2D() {}

void LightOccluder2D::DefineProperties() {
    DefineProperty(PROPERTY_DEFAULT_GROUP(occluderEnabled, Bool, true, "Occluder"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(closed, Bool, true, "Occluder"));
    DefineProperty(PROPERTY_ENUM_GROUP(cullMode, 0, "Occluder", Disabled, Clockwise, CounterClockwise));
    DefineProperty(PropertyDescriptor("polygon", PropertyValueType::FloatArray,
        PropertyJsonDefault(nlohmann::json::array()), PropertyHint(), "Occluder"));
}

std::vector<Vec2> LightOccluder2D::GetPolygon() const {
    const ComponentProperty* prop = GetPropertyRegistry().GetProperty("polygon");
    if (!prop) {
        return {};
    }
    return ParseFlatVertices(prop->GetValueAsJson());
}

void LightOccluder2D::SetPolygon(const std::vector<Vec2>& points) {
    ComponentProperty* prop = GetPropertyRegistry().GetProperty("polygon");
    if (prop) {
        prop->SetValueFromJson(ToFlatVertices(points));
    }
}

size_t LightOccluder2D::GetPointCount() const {
    return GetPolygon().size();
}

bool LightOccluder2D::GetClosed() const {
    return GetPropertyValue<bool>("closed");
}

void LightOccluder2D::SetClosed(bool closed) {
    SetPropertyValue<bool>("closed", closed);
}

OccluderCullMode2D LightOccluder2D::GetCullMode() const {
    return IntToCullMode(GetPropertyValue<int>("cullMode"));
}

void LightOccluder2D::SetCullMode(OccluderCullMode2D mode) {
    SetPropertyValue<int>("cullMode", CullModeToInt(mode));
}

bool LightOccluder2D::GetOccluderEnabled() const {
    return GetPropertyValue<bool>("occluderEnabled");
}

void LightOccluder2D::SetOccluderEnabled(bool enabled) {
    SetPropertyValue<bool>("occluderEnabled", enabled);
}

int LightOccluder2D::CullModeToInt(OccluderCullMode2D mode) const {
    return static_cast<int>(mode);
}

OccluderCullMode2D LightOccluder2D::IntToCullMode(int value) const {
    if (value < 0 || value > 2) return OccluderCullMode2D::Disabled;
    return static_cast<OccluderCullMode2D>(value);
}

Vec2 LightOccluder2D::LocalToWorld(const Vec2& local) const {
    const Node2D* owner = dynamic_cast<const Node2D*>(GetOwner());
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

void LightOccluder2D::AppendWorldEdges(std::vector<OccluderEdge2D>& outEdges) const {
    if (!IsEnabled() || !GetOccluderEnabled()) {
        return;
    }

    std::vector<Vec2> local = GetPolygon();
    size_t n = local.size();
    if (n < 2) {
        return;
    }

    std::vector<Vec2> world;
    world.reserve(n);
    for (const Vec2& p : local) {
        world.push_back(LocalToWorld(p));
    }

    size_t edgeCount = GetClosed() ? n : (n - 1);
    for (size_t i = 0; i < edgeCount; ++i) {
        OccluderEdge2D edge;
        edge.a = world[i];
        edge.b = world[(i + 1) % n];
        outEdges.push_back(edge);
    }
}

void LightOccluder2D::OnPropertyChanged(const std::string& propertyName,
                                        const nlohmann::json& newValue) {
    (void)propertyName;
    (void)newValue;
}

nlohmann::json LightOccluder2D::CallMethod(const std::string& method,
                                           const nlohmann::json& args) {
    auto argAt = [&](size_t i) -> nlohmann::json {
        if (args.is_array() && i < args.size()) {
            return args[i];
        }
        return nlohmann::json();
    };

    if (method == "set_polygon") {
        SetPolygon(ParseFlatVertices(argAt(0)));
    } else if (method == "get_polygon") {
        return ToFlatVertices(GetPolygon());
    } else if (method == "set_closed") {
        nlohmann::json a = argAt(0);
        SetClosed(a.is_boolean() ? a.get<bool>() : true);
    } else if (method == "is_closed") {
        return GetClosed();
    } else if (method == "set_cull_mode") {
        nlohmann::json a = argAt(0);
        SetCullMode(IntToCullMode(a.is_number() ? a.get<int>() : 0));
    } else if (method == "get_cull_mode") {
        return CullModeToInt(GetCullMode());
    } else if (method == "set_occluder_enabled") {
        nlohmann::json a = argAt(0);
        SetOccluderEnabled(a.is_boolean() ? a.get<bool>() : true);
    } else if (method == "is_occluder_enabled") {
        return GetOccluderEnabled();
    } else if (method == "get_point_count") {
        return static_cast<int>(GetPointCount());
    }
    return nlohmann::json();
}

void LightOccluder2D::buildDrawCommands(RenderContext& ctx) {
    Scene* scene = GetOwner() ? GetOwner()->GetScene() : nullptr;
    if (!scene || !scene->IsInEditor()) {
        return; // editor-only visualization
    }

    std::vector<OccluderEdge2D> edges;
    AppendWorldEdges(edges);

    Color outlineColor = GetOccluderEnabled()
        ? Color(0.3f, 0.7f, 1.0f, 0.85f)
        : Color(0.5f, 0.5f, 0.5f, 0.6f);

    for (const OccluderEdge2D& edge : edges) {
        ctx.drawLine(Vec3(edge.a.x, edge.a.y, 0.0f),
                     Vec3(edge.b.x, edge.b.y, 0.0f),
                     outlineColor, 2.0f);
    }
}

AABB LightOccluder2D::getWorldBounds() const {
    std::vector<Vec2> local = GetPolygon();
    const Node2D* owner = dynamic_cast<const Node2D*>(GetOwner());
    Vec2 center = owner ? owner->GetGlobalPosition() : Vec2::Zero();

    if (local.empty()) {
        return AABB(Vec3(center.x, center.y, -1.0f), Vec3(center.x, center.y, 1.0f));
    }

    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float maxY = std::numeric_limits<float>::lowest();
    for (const Vec2& p : local) {
        Vec2 w = LocalToWorld(p);
        minX = std::min(minX, w.x);
        minY = std::min(minY, w.y);
        maxX = std::max(maxX, w.x);
        maxY = std::max(maxY, w.y);
    }
    return AABB(Vec3(minX, minY, -1.0f), Vec3(maxX, maxY, 1.0f));
}

RenderLayer LightOccluder2D::getRenderLayer() const {
    return RenderLayer::Transparent;
}

SpatialType LightOccluder2D::getSpatialType() const {
    return SpatialType::World2D;
}

} // namespace components
} // namespace lupine
