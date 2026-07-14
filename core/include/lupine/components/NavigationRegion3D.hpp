#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/navigation/NavMeshBaker.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include <cstdint>
#include <vector>

namespace lupine {
namespace components {

/**
 * NavigationRegion3D Component
 *
 * Bakes a walkable 3D navigation surface from the collision (and optionally
 * render) geometry of a subtree, the Recast way, and publishes it to the global
 * navigation::NavigationServer3D for pathfinding.
 *
 * Attach it to a Node3D. On Bake() it gathers triangles from CollisionMesh3D
 * shapes (and StaticMesh3D / PrimitiveMesh3D when mesh geometry is enabled)
 * either within its own subtree or across the whole scene, transforms them to
 * world space, voxelizes and filters them for the configured agent dimensions,
 * subtracts any static carve volumes registered on the server, and emits the
 * resulting walkable triangles. Adjacent regions connect automatically.
 *
 * Baking is comparatively expensive, so it runs on OnReady (when autoBake is
 * set), when a bake parameter changes in the editor, when Bake() is called
 * explicitly, and when the server's carve-volume set changes - not every frame.
 */
class NavigationRegion3D : public core::Component, public IRenderableComponent {
public:
    NavigationRegion3D();
    explicit NavigationRegion3D(const std::string& name);
    virtual ~NavigationRegion3D();

    std::string GetTypeName() const override { return "NavigationRegion3D"; }
    void DefineProperties() override;
    void DefineSignals() override;

    // IRenderableComponent - editor-only wireframe of the baked navmesh.
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

    bool GetEnabledRegion() const;
    void SetEnabledRegion(bool enabled);

    // ===== Bake configuration =====
    float GetCellSize() const;
    void SetCellSize(float value);
    float GetCellHeight() const;
    void SetCellHeight(float value);
    float GetAgentHeight() const;
    void SetAgentHeight(float value);
    float GetAgentRadius() const;
    void SetAgentRadius(float value);
    float GetAgentMaxClimb() const;
    void SetAgentMaxClimb(float value);
    float GetMaxSlopeDegrees() const;
    void SetMaxSlopeDegrees(float value);
    int GetMinRegionArea() const;
    void SetMinRegionArea(int value);
    int GetGeometrySource() const;
    void SetGeometrySource(int value);
    bool GetIncludeCollision() const;
    void SetIncludeCollision(bool value);
    bool GetIncludeMeshes() const;
    void SetIncludeMeshes(bool value);

    /** Gather geometry, run the baker, and publish the navmesh to the server. */
    void Bake();

    /** Triangles produced by the last bake (world space). */
    const std::vector<navigation::NavMeshTriangle>& GetBakedTriangles() const { return m_BakedTriangles; }

    /** Number of convex polygons in the server after the last bake. */
    size_t GetPolygonCount() const { return m_BakedPolygonCount; }

private:
    std::string RegionId() const;
    void Unregister();
    void CollectGeometry(std::vector<navigation::NavMeshTriangle>& out) const;
    navigation::NavMeshBakeConfig BuildConfig() const;

    std::vector<navigation::NavMeshTriangle> m_BakedTriangles;
    size_t m_BakedPolygonCount = 0;
    bool m_Registered = false;
    uint64_t m_LastCarveVersion = 0;
};

} // namespace components
} // namespace lupine
