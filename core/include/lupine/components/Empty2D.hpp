#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include <string>

namespace lupine {
namespace components {

/**
 * Empty2DDisplayMode - How an Empty2D marker is visualized in the editor.
 */
enum class Empty2DDisplayMode {
    Point,   // Crosshair drawn at the node origin (position only)
    Volume   // Rectangle showing the node position, rotation and scaled size
};

/**
 * Empty2D Component
 *
 * A purely editorial marker for a 2D transform node. It draws NOTHING at
 * runtime; in the editor it visualizes the owning Node2D so that otherwise
 * invisible transform nodes (spawn points, anchors, metadata holders) can be
 * seen and positioned. It is the 2D analogue of Godot's Marker2D / a
 * transparent ColorRect used purely for layout.
 *
 * Two display modes:
 * - Point:  an axis-aligned crosshair centered on the node origin. Marks a
 *           single position; ignores scale.
 * - Volume: a rectangle of `size` (in the node's local space) transformed by
 *           the node's full global transform, so it shows position, rotation
 *           and scale. The rectangle is resizable with the editor scale gizmo.
 */
class Empty2D : public core::Component, public IRenderableComponent {
public:
    Empty2D();
    explicit Empty2D(const std::string& name);
    virtual ~Empty2D();

    std::string GetTypeName() const override { return "Empty2D"; }
    void DefineProperties() override;

    // Editor gizmo hook - resizes the volume rectangle instead of the node.
    bool OnGizmoScale(float scaleDelta, int axis, bool is3D) override;

    // ===== Properties =====
    Empty2DDisplayMode GetDisplayMode() const;
    void SetDisplayMode(Empty2DDisplayMode mode);

    math::Vec2 GetSize() const;
    void SetSize(const math::Vec2& size);

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
