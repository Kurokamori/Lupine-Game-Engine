#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/math/Vec2.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include <vector>

namespace lupine {
namespace components {

/**
 * NavigationRegion2D Component
 *
 * Defines a walkable area for 2D pathfinding. Attach it to a Node2D and give it
 * an outline polygon (and optionally holes). On bake the outline is triangulated
 * into convex navigation polygons, transformed into world space using the owner
 * node's global transform, and published to the navigation::NavigationServer2D.
 *
 * Multiple regions whose edges touch are connected automatically by the server,
 * so a level can be assembled from several overlapping or adjacent regions.
 *
 * Geometry is authored in the owner node's local space:
 * - outline: flat list of vertices [x0, y0, x1, y1, ...] forming the boundary.
 * - holes:   array of flat vertex lists, each carving an obstacle out of the
 *            walkable area.
 */
class NavigationRegion2D : public core::Component, public IRenderableComponent {
public:
    NavigationRegion2D();
    explicit NavigationRegion2D(const std::string& name);
    virtual ~NavigationRegion2D();

    std::string GetTypeName() const override { return "NavigationRegion2D"; }
    void DefineProperties() override;
    void DefineSignals() override;

    // IRenderableComponent - editor-only debug visualization of the nav geometry.
    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override;

    void OnReady() override;
    void OnUpdate(float deltaTime) override;
    void OnDestroy() override;
    void OnExitTree() override;
    void OnPropertyChanged(const std::string& propertyName,
                           const nlohmann::json& newValue) override;

    nlohmann::json CallMethod(const std::string& method,
                              const nlohmann::json& args) override;

    // ===== Enabled =====
    bool GetEnabledRegion() const;
    void SetEnabledRegion(bool enabled);

    // ===== Outline =====
    void SetOutline(const std::vector<math::Vec2>& outline);
    std::vector<math::Vec2> GetOutline() const;

    // ===== Holes =====
    void SetHoles(const std::vector<std::vector<math::Vec2>>& holes);
    std::vector<std::vector<math::Vec2>> GetHoles() const;
    void AddHole(const std::vector<math::Vec2>& hole);
    void ClearHoles();

    /** Re-triangulate and publish the region to the navigation server. */
    void Bake();

    /** Number of convex polygons produced by the last bake. */
    size_t GetPolygonCount() const { return m_BakedPolygonCount; }

private:
    std::string RegionId() const;
    void Unregister();
    bool OwnerTransformChanged();
    math::Vec2 LocalToWorld(const math::Vec2& local) const;

    size_t m_BakedPolygonCount = 0;
    bool m_Registered = false;

    bool m_HasCachedTransform = false;
    math::Vec2 m_CachedPosition;
    float m_CachedRotation = 0.0f;
    math::Vec2 m_CachedScale = math::Vec2::One();
};

} // namespace components
} // namespace lupine
