#pragma once

#include "lupine/navigation/NavMesh3D.hpp"
#include "lupine/navigation/NavMeshBaker.hpp"
#include "lupine/math/Vec3.hpp"
#include "lupine/math/AABB.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace lupine {
namespace navigation {

/**
 * NavigationServer3D - global registry of 3D navigation regions, static carve
 * volumes and avoidance agents, plus the single entry point for 3D path queries
 * and local collision avoidance.
 *
 * Unlike the 2D server (which triangulates authored outlines), 3D regions are
 * baked from collision/mesh geometry by the NavMeshBaker inside the
 * NavigationRegion3D component, which then publishes the resulting world-space
 * walkable triangles here. The server merges all enabled regions into one
 * connected NavMesh3D so that adjacent regions link automatically.
 *
 * Static obstacles publish a world-space carve box; regions read the active
 * carve set at bake time so paths route around placed blockers, and they poll
 * GetCarveVersion() to rebake when it changes.
 *
 * Local avoidance is solved on the horizontal (XZ) plane with the shared ORCA
 * solver: agents keep their navmesh elevation while steering around neighbours
 * and dynamic obstacles in the ground plane.
 *
 * This is a pure core service: it depends only on math, NavMesh3D, the baker and
 * the ORCA solver, never on the scene tree, rendering, or runtime.
 */
class NavigationServer3D {
public:
    static NavigationServer3D& GetInstance();

    /* ===================== Regions (baked geometry) ===================== */

    /** Register or replace a region's baked world-space walkable triangles. */
    void UpdateRegion(const std::string& regionId,
                      const std::vector<NavMeshTriangle>& triangles,
                      bool enabled);

    void RemoveRegion(const std::string& regionId);

    /** Convex-polygon count contributed by a region after the last rebuild. */
    size_t GetRegionPolygonCount(const std::string& regionId);

    /* ===================== Static carve volumes ===================== */

    /**
     * Register or replace a world-space box that is subtracted from any region
     * baked after this call. Bumps the carve version so regions rebake.
     */
    void UpdateCarveVolume(const std::string& volumeId, const math::AABB& box, bool enabled);

    void RemoveCarveVolume(const std::string& volumeId);

    /** Active (enabled) carve volumes, for a region to feed into the baker. */
    std::vector<NavMeshCarveVolume> GetActiveCarveVolumes() const;

    /** Monotonic counter bumped whenever the carve-volume set changes. */
    uint64_t GetCarveVersion() const { return m_CarveVersion; }

    /* ===================== Dynamic avoidance (XZ ORCA) ===================== */

    void UpdateAvoidanceAgent(const std::string& agentId, const math::Vec3& position,
                              float radius, const math::Vec3& prefVelocity, float maxSpeed);

    void RemoveAvoidanceAgent(const std::string& agentId);

    void UpdateAvoidanceObstacle(const std::string& obstacleId,
                                 const math::Vec3& position, float radius);

    void RemoveAvoidanceObstacle(const std::string& obstacleId);

    /**
     * Collision-free velocity for a registered avoidance agent, solved on the XZ
     * plane. The returned velocity has Y = 0 (elevation is owned by the navmesh).
     */
    math::Vec3 ComputeAvoidanceVelocity(const std::string& agentId, float timeStep,
                                        float neighborDistance, int maxNeighbors,
                                        float timeHorizon, float timeHorizonObstacle);

    /* ===================== Queries ===================== */

    bool QueryPath(const math::Vec3& from, const math::Vec3& to,
                   std::vector<math::Vec3>& outPath, float agentRadius = 0.0f);

    math::Vec3 GetClosestPoint(const math::Vec3& p);

    bool IsPointNavigable(const math::Vec3& p, float verticalTolerance = 0.0f);

    /** Surface elevation (world Y) at horizontal position (x, z), if covered. */
    bool SampleHeight(float x, float z, float& outY);

    void Clear();

    /** Monotonic version, bumped on every static-geometry change. */
    uint64_t GetVersion() const { return m_Version; }

    size_t GetRegionCount() const { return m_Regions.size(); }
    size_t GetCarveVolumeCount() const { return m_CarveVolumes.size(); }

private:
    NavigationServer3D() = default;

    void RebuildIfDirty();

    struct RegionData {
        std::vector<NavMeshTriangle> triangles;
        bool enabled = true;
        size_t polygonCount = 0;
    };

    struct CarveData {
        math::AABB box;
        bool enabled = true;
    };

    struct AvoidanceAgentData {
        math::Vec3 position;
        float radius = 1.0f;
        math::Vec3 prefVelocity;
        float maxSpeed = 0.0f;
    };

    struct AvoidanceObstacleData {
        math::Vec3 position;
        float radius = 1.0f;
    };

    std::unordered_map<std::string, RegionData> m_Regions;
    std::unordered_map<std::string, CarveData> m_CarveVolumes;
    std::unordered_map<std::string, AvoidanceAgentData> m_AvoidanceAgents;
    std::unordered_map<std::string, AvoidanceObstacleData> m_AvoidanceObstacles;

    NavMesh3D m_CombinedMesh;
    bool m_Dirty = false;
    uint64_t m_Version = 0;
    uint64_t m_CarveVersion = 0;
};

} // namespace navigation
} // namespace lupine
