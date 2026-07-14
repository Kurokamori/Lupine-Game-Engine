#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/math/Vec3.hpp"
#include "lupine/rendering/RenderWorld.hpp"

namespace lupine {
namespace components {

/**
 * NavigationObstacle3D Component
 *
 * Two complementary roles, both optional:
 *
 * 1. Dynamic avoidance (default): registers a circular avoidance disc of
 *    `radius` at the owner's position every frame. NavigationAgent3D agents with
 *    avoidance enabled steer around it on the horizontal plane via ORCA. Use it
 *    for moving hazards the navmesh cannot bake (actors, doors, vehicles).
 *
 * 2. Static carving (`carveNavMesh`): publishes a world-space box (radius x
 *    height x radius) to the NavigationServer3D. Any NavigationRegion3D baked
 *    afterwards subtracts the box from its walkable surface, so paths route
 *    around the placed blocker. Re-publishes when the owner moves.
 */
class NavigationObstacle3D : public core::Component, public IRenderableComponent {
public:
    NavigationObstacle3D();
    explicit NavigationObstacle3D(const std::string& name);
    virtual ~NavigationObstacle3D();

    std::string GetTypeName() const override { return "NavigationObstacle3D"; }
    void DefineProperties() override;

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
    float GetHeight() const;
    void SetHeight(float height);
    bool GetAvoidanceEnabled() const;
    void SetAvoidanceEnabled(bool enabled);
    bool GetCarveNavMesh() const;
    void SetCarveNavMesh(bool carve);

    /** Re-publish the carve volume and avoidance disc to the server. */
    void Bake();

private:
    std::string ObstacleId() const;
    void RefreshAvoidance();
    void RefreshCarve(bool force);
    void UnregisterAll();
    bool OwnerMoved();

    math::AABB BuildCarveBox() const;

    bool m_AvoidanceRegistered = false;
    bool m_CarveRegistered = false;

    bool m_HasCachedPosition = false;
    math::Vec3 m_CachedPosition;
};

} // namespace components
} // namespace lupine
