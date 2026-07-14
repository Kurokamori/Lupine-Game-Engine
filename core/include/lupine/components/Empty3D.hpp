#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include <string>

namespace lupine {
namespace components {

/**
 * Empty3DDisplayMode - How an Empty3D marker is visualized in the editor.
 */
enum class Empty3DDisplayMode {
    Point,   // Three-axis cross drawn at the node origin (position only)
    Volume   // Wireframe cube showing the node position, rotation and scaled size
};

/**
 * Empty3D Component
 *
 * A purely editorial marker for a 3D transform node. It draws NOTHING at
 * runtime; in the editor it visualizes the owning Node3D so that otherwise
 * invisible transform nodes (spawn points, anchors, metadata holders) can be
 * seen and positioned. It is the 3D analogue of Godot's Marker3D.
 *
 * Two display modes:
 * - Point:  a three-axis cross centered on the node origin. Marks a single
 *           position; ignores scale.
 * - Volume: a wireframe cube of `size` (in the node's local space) transformed
 *           by the node's full global transform, so it shows position, rotation
 *           and scale. The cube is resizable with the editor scale gizmo.
 */
class Empty3D : public core::Component, public IRenderableComponent {
public:
    Empty3D();
    explicit Empty3D(const std::string& name);
    virtual ~Empty3D();

    std::string GetTypeName() const override { return "Empty3D"; }
    void DefineProperties() override;

    // Editor gizmo hook - resizes the volume cube instead of the node.
    bool OnGizmoScale(float scaleDelta, int axis, bool is3D) override;

    // ===== Properties =====
    Empty3DDisplayMode GetDisplayMode() const;
    void SetDisplayMode(Empty3DDisplayMode mode);

    math::Vec3 GetSize() const;
    void SetSize(const math::Vec3& size);

    math::Color GetColor() const;
    void SetColor(const math::Color& color);

    float GetPointSize() const;
    void SetPointSize(float pointSize);

    // ===== IRenderableComponent =====
    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override;
};

} // namespace components
} // namespace lupine
