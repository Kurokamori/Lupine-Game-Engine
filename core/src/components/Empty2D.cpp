#include "lupine/components/Empty2D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include <algorithm>
#include <array>
#include <limits>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

Empty2D::Empty2D() : Component("Empty2D") {}
Empty2D::Empty2D(const std::string& name) : Component(name) {}
Empty2D::~Empty2D() {}

void Empty2D::DefineProperties() {
    DefineProperty(PROPERTY_ENUM_GROUP(mode, static_cast<int>(Empty2DDisplayMode::Point),
        "Marker", Point, Volume));
    DefineProperty(PROPERTY_DEFAULT_GROUP(size, Vec2, Vec2(100.0f, 100.0f), "Marker"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(color, Color, Color(0.4f, 0.9f, 1.0f, 0.9f), "Marker"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(pointSize, 16.0f, 1.0f, 4096.0f, 1.0f, "Marker"));
}

Empty2DDisplayMode Empty2D::GetDisplayMode() const {
    return static_cast<Empty2DDisplayMode>(GetPropertyValue<int>("mode"));
}
void Empty2D::SetDisplayMode(Empty2DDisplayMode mode) {
    SetPropertyValue<int>("mode", static_cast<int>(mode));
}

Vec2 Empty2D::GetSize() const { return GetPropertyValue<Vec2>("size"); }
void Empty2D::SetSize(const Vec2& size) { SetPropertyValue<Vec2>("size", size); }

Color Empty2D::GetColor() const { return GetPropertyValue<Color>("color"); }
void Empty2D::SetColor(const Color& color) { SetPropertyValue<Color>("color", color); }

float Empty2D::GetPointSize() const { return GetPropertyValue<float>("pointSize"); }
void Empty2D::SetPointSize(float pointSize) { SetPropertyValue<float>("pointSize", pointSize); }

bool Empty2D::OnGizmoScale(float scaleDelta, int axis, bool is3D) {
    if (is3D || GetDisplayMode() != Empty2DDisplayMode::Volume) {
        return false;
    }
    Vec2 size = GetSize();
    if (axis == 0 || axis == -1) {
        size.x = std::max(0.1f, size.x + scaleDelta * size.x);
    }
    if (axis == 1 || axis == -1) {
        size.y = std::max(0.1f, size.y + scaleDelta * size.y);
    }
    SetSize(size);
    return true;
}

void Empty2D::buildDrawCommands(RenderContext& ctx) {
    Scene* scene = GetOwner() ? GetOwner()->GetScene() : nullptr;
    if (!scene || !scene->IsInEditor()) {
        return; // editor-only visualization, renders nothing at runtime
    }

    Node2D* node = dynamic_cast<Node2D*>(GetOwner());
    if (!node) {
        return;
    }

    const Color color = GetColor();

    if (GetDisplayMode() == Empty2DDisplayMode::Point) {
        const Vec2 c = node->GetGlobalPosition();
        const float h = GetPointSize();
        ctx.drawLine(Vec3(c.x - h, c.y, 0.0f), Vec3(c.x + h, c.y, 0.0f), color, 2.0f);
        ctx.drawLine(Vec3(c.x, c.y - h, 0.0f), Vec3(c.x, c.y + h, 0.0f), color, 2.0f);
        return;
    }

    const Vec2 half = GetSize() * 0.5f;
    const Mat4 m = node->GetGlobalTransformMatrix();
    const std::array<Vec2, 4> local = {
        Vec2(-half.x, -half.y), Vec2(half.x, -half.y),
        Vec2(half.x, half.y), Vec2(-half.x, half.y)
    };
    std::array<Vec3, 4> world;
    for (size_t i = 0; i < 4; ++i) {
        world[i] = m.TransformPoint(Vec3(local[i].x, local[i].y, 0.0f));
    }
    for (size_t i = 0; i < 4; ++i) {
        ctx.drawLine(world[i], world[(i + 1) % 4], color, 2.0f);
    }
}

AABB Empty2D::getWorldBounds() const {
    Node2D* node = dynamic_cast<Node2D*>(GetOwner());
    if (!node) {
        return AABB();
    }

    if (GetDisplayMode() == Empty2DDisplayMode::Point) {
        const Vec2 c = node->GetGlobalPosition();
        const float h = GetPointSize();
        return AABB(Vec3(c.x - h, c.y - h, -1.0f), Vec3(c.x + h, c.y + h, 1.0f));
    }

    const Vec2 half = GetSize() * 0.5f;
    const Mat4 m = node->GetGlobalTransformMatrix();
    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float maxX = -std::numeric_limits<float>::max();
    float maxY = -std::numeric_limits<float>::max();
    const std::array<Vec2, 4> local = {
        Vec2(-half.x, -half.y), Vec2(half.x, -half.y),
        Vec2(half.x, half.y), Vec2(-half.x, half.y)
    };
    for (const Vec2& l : local) {
        const Vec3 w = m.TransformPoint(Vec3(l.x, l.y, 0.0f));
        minX = std::min(minX, w.x);
        minY = std::min(minY, w.y);
        maxX = std::max(maxX, w.x);
        maxY = std::max(maxY, w.y);
    }
    return AABB(Vec3(minX, minY, -1.0f), Vec3(maxX, maxY, 1.0f));
}

RenderLayer Empty2D::getRenderLayer() const {
    return RenderLayer::Transparent;
}

SpatialType Empty2D::getSpatialType() const {
    return SpatialType::World2D;
}

} // namespace components
} // namespace lupine
