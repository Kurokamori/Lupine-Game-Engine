#pragma once

#include "lupine/navigation/NavMesh2D.hpp"
#include "lupine/navigation/Avoidance2D.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace lupine {
namespace navigation {

/**
 * NavigationServer2D - global registry of navigation regions and obstacles plus
 * the single entry point for path queries and local collision avoidance.
 *
 * Regions publish a world-space outline (and optional holes). Static obstacles
 * publish a world-space polygon that is carved out of any region that contains
 * it. The server lazily triangulates everything into one combined NavMesh2D
 * (linking polygons that share edges across all regions) whenever the geometry
 * changes, so neighbouring regions connect automatically.
 *
 * For dynamic, per-frame collision avoidance, agents and obstacles register
 * lightweight avoidance records (position + radius) and agents call
 * ComputeAvoidanceVelocity() to obtain an ORCA-corrected velocity.
 *
 * This is a pure core service: it depends only on math, NavMesh2D and the ORCA
 * solver, never on the scene tree, rendering, or runtime.
 */
class NavigationServer2D {
public:
    static NavigationServer2D& GetInstance();

    /* ===================== Regions (static geometry) ===================== */

    /**
     * Register or replace a region's world-space geometry. `outline` is the
     * walkable boundary; `holes` are carved out. Marks the mesh dirty.
     */
    void UpdateRegion(const std::string& regionId,
                      const std::vector<math::Vec2>& outline,
                      const std::vector<std::vector<math::Vec2>>& holes,
                      bool enabled);

    /** Remove a region entirely. No-op if the id is unknown. */
    void RemoveRegion(const std::string& regionId);

    /** Convex-polygon count contributed by a region after the last rebuild. */
    size_t GetRegionPolygonCount(const std::string& regionId);

    /* ===================== Static carving obstacles ===================== */

    /**
     * Register or replace a static obstacle polygon (world space) that carves a
     * hole into any region whose outline fully contains it. Marks the mesh dirty.
     */
    void UpdateCarveObstacle(const std::string& obstacleId,
                             const std::vector<math::Vec2>& polygon,
                             bool enabled);

    void RemoveCarveObstacle(const std::string& obstacleId);

    /* ===================== Dynamic avoidance ===================== */

    /** Register/refresh an avoidance agent (called once per frame by agents). */
    void UpdateAvoidanceAgent(const std::string& agentId, const math::Vec2& position,
                              float radius, const math::Vec2& prefVelocity, float maxSpeed);

    void RemoveAvoidanceAgent(const std::string& agentId);

    /** Register/refresh a dynamic circular avoidance obstacle. */
    void UpdateAvoidanceObstacle(const std::string& obstacleId,
                                 const math::Vec2& position, float radius);

    void RemoveAvoidanceObstacle(const std::string& obstacleId);

    /**
     * Compute a collision-free velocity for a previously-registered avoidance
     * agent, considering all other avoidance agents within `neighborDistance`
     * and all avoidance obstacles. Returns the agent's preferred velocity
     * unchanged if it is unknown.
     */
    math::Vec2 ComputeAvoidanceVelocity(const std::string& agentId, float timeStep,
                                        float neighborDistance, int maxNeighbors,
                                        float timeHorizon, float timeHorizonObstacle);

    /* ===================== Queries ===================== */

    /**
     * Compute a path between two world points across all enabled regions.
     * `agentRadius` insets the corridor from walls. Returns false if no
     * navigable connection exists; the path is clamped to the surface.
     */
    bool QueryPath(const math::Vec2& from, const math::Vec2& to,
                   std::vector<math::Vec2>& outPath, float agentRadius = 0.0f);

    /** Closest navigable point to `p` (returns `p` if no regions are baked). */
    math::Vec2 GetClosestPoint(const math::Vec2& p);

    /** True if `p` lies on the navigable surface. */
    bool IsPointNavigable(const math::Vec2& p);

    /** Drop all regions, obstacles and avoidance records. */
    void Clear();

    /**
     * Monotonic version counter, bumped on every static-geometry change. Agents
     * poll this to detect when their cached paths must be recomputed.
     */
    uint64_t GetVersion() const { return m_Version; }

    size_t GetRegionCount() const { return m_Regions.size(); }
    size_t GetObstacleCount() const { return m_CarveObstacles.size(); }

private:
    NavigationServer2D() = default;

    void RebuildIfDirty();

    struct RegionData {
        std::vector<math::Vec2> outline;
        std::vector<std::vector<math::Vec2>> holes;
        bool enabled = true;
        size_t polygonCount = 0;
    };

    struct CarveObstacleData {
        std::vector<math::Vec2> polygon;
        bool enabled = true;
    };

    struct AvoidanceAgentData {
        math::Vec2 position;
        float radius = 1.0f;
        math::Vec2 prefVelocity;
        float maxSpeed = 0.0f;
    };

    struct AvoidanceObstacleData {
        math::Vec2 position;
        float radius = 1.0f;
    };

    std::unordered_map<std::string, RegionData> m_Regions;
    std::unordered_map<std::string, CarveObstacleData> m_CarveObstacles;
    std::unordered_map<std::string, AvoidanceAgentData> m_AvoidanceAgents;
    std::unordered_map<std::string, AvoidanceObstacleData> m_AvoidanceObstacles;

    NavMesh2D m_CombinedMesh;
    bool m_Dirty = false;
    uint64_t m_Version = 0;
};

} // namespace navigation
} // namespace lupine
