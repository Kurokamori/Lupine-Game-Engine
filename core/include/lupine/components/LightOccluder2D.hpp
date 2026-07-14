#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include <string>
#include <vector>

namespace lupine {
namespace components {

/**
 * Cull mode for a 2D light occluder.
 *
 * Controls which side of an occluder edge blocks light.
 * The test is performed per-light using the
 * winding of the edge relative to the light position:
 *
 * - Disabled:         both sides of every edge cast shadows.
 * - Clockwise:        only edges wound clockwise as seen from the light cast.
 * - CounterClockwise: only edges wound counter-clockwise as seen from the light cast.
 *
 * One-sided culling lets a closed polygon cast shadows from its outline while
 * its interior stays lit (or vice-versa), avoiding double-occlusion artifacts.
 */
enum class OccluderCullMode2D {
    Disabled = 0,
    Clockwise = 1,
    CounterClockwise = 2
};

/**
 * A single world-space occluder edge (segment) produced by a LightOccluder2D.
 */
struct OccluderEdge2D {
    math::Vec2 a;
    math::Vec2 b;
};

/**
 * LightOccluder2D Component
 *
 * Defines an occluder polygon/polyline that blocks light cast by Light2D
 * components that have shadows enabled. The occluder is authored in the owning
 * Node2D's local space and transformed to world space (position/rotation/scale)
 * when shadows are computed.
 *
 * Features:
 * - Arbitrary polygon or open polyline (closed flag)
 * - One-sided or two-sided occlusion via cull mode
 * - Editor visualization of the occluder outline
 * - Full programmatic point control (scripting via CallMethod)
 *
 * To cast shadows: add a LightOccluder2D to a Node2D, define its points, then
 * enable "shadowEnabled" on any Light2D whose range reaches the occluder.
 */
class LightOccluder2D : public core::Component, public IRenderableComponent {
public:
    LightOccluder2D();
    explicit LightOccluder2D(const std::string& name);
    virtual ~LightOccluder2D();

    // ISerializable interface
    std::string GetTypeName() const override { return "LightOccluder2D"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) override;
    nlohmann::json CallMethod(const std::string& method, const nlohmann::json& args) override;

    // ===== Polygon Properties =====

    /**
     * Get/Set the occluder points in local space.
     */
    std::vector<math::Vec2> GetPolygon() const;
    void SetPolygon(const std::vector<math::Vec2>& points);

    /**
     * Get number of polygon points.
     */
    size_t GetPointCount() const;

    /**
     * Get/Set whether the polygon is closed (last point connects to first).
     * Open polylines (false) only cast from their explicit segments.
     */
    bool GetClosed() const;
    void SetClosed(bool closed);

    /**
     * Get/Set the cull mode for one-sided/two-sided occlusion.
     */
    OccluderCullMode2D GetCullMode() const;
    void SetCullMode(OccluderCullMode2D mode);

    /**
     * Get/Set whether this occluder participates in shadow casting.
     * (Distinct from the component's base Enabled flag, which also gates it.)
     */
    bool GetOccluderEnabled() const;
    void SetOccluderEnabled(bool enabled);

    // ===== World-Space Query =====

    /**
     * Append this occluder's world-space edges to the output vector.
     * Honors the closed flag and the owner Node2D global transform.
     * Disabled occluders (component disabled or occluderEnabled false) append nothing.
     */
    void AppendWorldEdges(std::vector<OccluderEdge2D>& outEdges) const;

    // ===== IRenderableComponent Interface (editor visualization) =====

    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override;

private:
    int CullModeToInt(OccluderCullMode2D mode) const;
    OccluderCullMode2D IntToCullMode(int value) const;

    math::Vec2 LocalToWorld(const math::Vec2& local) const;
};

} // namespace components
} // namespace lupine
