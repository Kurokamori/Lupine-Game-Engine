#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/math/Vec2.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include <vector>

namespace lupine {
namespace components {

/**
 * NavigationObstacle2D Component
 *
 * Two complementary roles, both optional:
 *
 * 1. Dynamic avoidance (default): registers a circular avoidance disc of
 *    `radius` at the owner's position every frame. NavigationAgent2D agents
 *    with avoidance enabled steer around it via ORCA. Use this for moving
 *    hazards the navmesh cannot bake (other actors, doors, vehicles).
 *
 * 2. Static carving (`carveNavMesh`): publishes a local-space `vertices`
 *    polygon that is cut out of any NavigationRegion2D whose outline fully
 *    contains it, so paths route around it. Use this for placed, immovable
 *    blockers. Re-bakes when the owner transform or the polygon changes.
 */
class NavigationObstacle2D : public core::Component, public IRenderableComponent {
public:
    NavigationObstacle2D();
    explicit NavigationObstacle2D(const std::string& name);
    virtual ~NavigationObstacle2D();

    std::string GetTypeName() const override { return "NavigationObstacle2D"; }
    void DefineProperties() override;

    // IRenderableComponent - editor-only debug visualization.
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

    float GetRadius() const;
    void SetRadius(float radius);
    bool GetAvoidanceEnabled() const;
    void SetAvoidanceEnabled(bool enabled);
    bool GetCarveNavMesh() const;
    void SetCarveNavMesh(bool carve);

    void SetVertices(const std::vector<math::Vec2>& vertices);
    std::vector<math::Vec2> GetVertices() const;

    /** Recompute and publish the static carve polygon (if carving is enabled). */
    void Bake();

private:
    std::string ObstacleId() const;
    void RefreshAvoidance();
    void UnregisterAll();
    bool OwnerTransformChanged();
    void UpdateCachedTransform();
    math::Vec2 LocalToWorld(const math::Vec2& local) const;

    bool m_AvoidanceRegistered = false;
    bool m_CarveRegistered = false;

    bool m_HasCachedTransform = false;
    math::Vec2 m_CachedPosition;
    float m_CachedRotation = 0.0f;
    math::Vec2 m_CachedScale = math::Vec2::One();
};

} // namespace components
} // namespace lupine
