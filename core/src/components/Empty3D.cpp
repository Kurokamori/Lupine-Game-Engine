#include "lupine/components/Empty3D.hpp"
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

namespace {
// Eight cube corners in unit-half-extent local space.
static const std::array<Vec3, 8> kCubeCorners = {
    Vec3(-1.0f, -1.0f, -1.0f), Vec3(1.0f, -1.0f, -1.0f),
    Vec3(1.0f,  1.0f, -1.0f),  Vec3(-1.0f, 1.0f, -1.0f),
    Vec3(-1.0f, -1.0f,  1.0f), Vec3(1.0f, -1.0f,  1.0f),
    Vec3(1.0f,  1.0f,  1.0f),  Vec3(-1.0f, 1.0f,  1.0f)
};
// Twelve cube edges as corner-index pairs.
static const std::array<std::pair<int, int>, 12> kCubeEdges = {
    std::pair<int, int>{0, 1}, {1, 2}, {2, 3}, {3, 0},
    {4, 5}, {5, 6}, {6, 7}, {7, 4},
    {0, 4}, {1, 5}, {2, 6}, {3, 7}
};
} // namespace

Empty3D::Empty3D() : Component("Empty3D") {}
Empty3D::Empty3D(const std::string& name) : Component(name) {}
Empty3D::~Empty3D() {}

void Empty3D::DefineProperties() {
    DefineProperty(PROPERTY_ENUM_GROUP(mode, static_cast<int>(Empty3DDisplayMode::Point),
        "Marker", Point, Volume));
    DefineProperty(PROPERTY_DEFAULT_GROUP(size, Vec3, Vec3(1.0f, 1.0f, 1.0f), "Marker"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(color, Color, Color(0.4f, 0.9f, 1.0f, 0.9f), "Marker"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(pointSize, 0.5f, 0.01f, 1000.0f, 0.01f, "Marker"));
}

Empty3DDisplayMode Empty3D::GetDisplayMode() const {
    return static_cast<Empty3DDisplayMode>(GetPropertyValue<int>("mode"));
}
void Empty3D::SetDisplayMode(Empty3DDisplayMode mode) {
    SetPropertyValue<int>("mode", static_cast<int>(mode));
}

Vec3 Empty3D::GetSize() const { return GetPropertyValue<Vec3>("size"); }
void Empty3D::SetSize(const Vec3& size) { SetPropertyValue<Vec3>("size", size); }

Color Empty3D::GetColor() const { return GetPropertyValue<Color>("color"); }
void Empty3D::SetColor(const Color& color) { SetPropertyValue<Color>("color", color); }

float Empty3D::GetPointSize() const { return GetPropertyValue<float>("pointSize"); }
void Empty3D::SetPointSize(float pointSize) { SetPropertyValue<float>("pointSize", pointSize); }

bool Empty3D::OnGizmoScale(float scaleDelta, int axis, bool is3D) {
    if (!is3D || GetDisplayMode() != Empty3DDisplayMode::Volume) {
        return false;
    }
    Vec3 size = GetSize();
    if (axis == 0 || axis == -1) {
        size.x = std::max(0.01f, size.x + scaleDelta * size.x);
    }
    if (axis == 1 || axis == -1) {
        size.y = std::max(0.01f, size.y + scaleDelta * size.y);
    }
    if (axis == 2 || axis == -1) {
        size.z = std::max(0.01f, size.z + scaleDelta * size.z);
    }
    SetSize(size);
    return true;
}

void Empty3D::buildDrawCommands(RenderContext& ctx) {
    Scene* scene = GetOwner() ? GetOwner()->GetScene() : nullptr;
    if (!scene || !scene->IsInEditor()) {
        return; // editor-only visualization, renders nothing at runtime
    }

    Node3D* node = dynamic_cast<Node3D*>(GetOwner());
    if (!node) {
        return;
    }

    const Color color = GetColor();

    if (GetDisplayMode() == Empty3DDisplayMode::Point) {
        const Vec3 c = node->GetGlobalPosition();
        const float h = GetPointSize();
        ctx.drawLine(Vec3(c.x - h, c.y, c.z), Vec3(c.x + h, c.y, c.z), color, 2.0f);
        ctx.drawLine(Vec3(c.x, c.y - h, c.z), Vec3(c.x, c.y + h, c.z), color, 2.0f);
        ctx.drawLine(Vec3(c.x, c.y, c.z - h), Vec3(c.x, c.y, c.z + h), color, 2.0f);
        return;
    }

    const Vec3 half = GetSize() * 0.5f;
    const Mat4 m = node->GetGlobalTransformMatrix();
    std::array<Vec3, 8> world;
    for (size_t i = 0; i < 8; ++i) {
        world[i] = m.TransformPoint(Vec3(
            kCubeCorners[i].x * half.x,
            kCubeCorners[i].y * half.y,
            kCubeCorners[i].z * half.z));
    }
    for (const std::pair<int, int>& edge : kCubeEdges) {
        ctx.drawLine(world[edge.first], world[edge.second], color, 2.0f);
    }
}

AABB Empty3D::getWorldBounds() const {
    Node3D* node = dynamic_cast<Node3D*>(GetOwner());
    if (!node) {
        return AABB();
    }

    if (GetDisplayMode() == Empty3DDisplayMode::Point) {
        const Vec3 c = node->GetGlobalPosition();
        const float h = GetPointSize();
        return AABB(c - Vec3(h, h, h), c + Vec3(h, h, h));
    }

    const Vec3 half = GetSize() * 0.5f;
    const Mat4 m = node->GetGlobalTransformMatrix();
    Vec3 lo(std::numeric_limits<float>::max());
    Vec3 hi(-std::numeric_limits<float>::max());
    for (const Vec3& corner : kCubeCorners) {
        const Vec3 w = m.TransformPoint(Vec3(
            corner.x * half.x, corner.y * half.y, corner.z * half.z));
        lo.x = std::min(lo.x, w.x); lo.y = std::min(lo.y, w.y); lo.z = std::min(lo.z, w.z);
        hi.x = std::max(hi.x, w.x); hi.y = std::max(hi.y, w.y); hi.z = std::max(hi.z, w.z);
    }
    return AABB(lo, hi);
}

RenderLayer Empty3D::getRenderLayer() const {
    return RenderLayer::Transparent;
}

SpatialType Empty3D::getSpatialType() const {
    return SpatialType::World3D;
}

} // namespace components
} // namespace lupine
