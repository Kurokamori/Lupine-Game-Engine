#include "lupine/navigation/NavigationServer2D.hpp"

#include <algorithm>

namespace lupine {
namespace navigation {

using math::Vec2;

NavigationServer2D& NavigationServer2D::GetInstance() {
    static NavigationServer2D instance;
    return instance;
}

/* ===================== Regions ===================== */

void NavigationServer2D::UpdateRegion(const std::string& regionId,
                                      const std::vector<Vec2>& outline,
                                      const std::vector<std::vector<Vec2>>& holes,
                                      bool enabled) {
    RegionData& data = m_Regions[regionId];
    data.outline = outline;
    data.holes = holes;
    data.enabled = enabled;
    m_Dirty = true;
    ++m_Version;
}

void NavigationServer2D::RemoveRegion(const std::string& regionId) {
    if (m_Regions.erase(regionId) > 0) {
        m_Dirty = true;
        ++m_Version;
    }
}

size_t NavigationServer2D::GetRegionPolygonCount(const std::string& regionId) {
    RebuildIfDirty();
    auto it = m_Regions.find(regionId);
    return (it != m_Regions.end()) ? it->second.polygonCount : 0;
}

/* ===================== Carve obstacles ===================== */

void NavigationServer2D::UpdateCarveObstacle(const std::string& obstacleId,
                                             const std::vector<Vec2>& polygon,
                                             bool enabled) {
    CarveObstacleData& data = m_CarveObstacles[obstacleId];
    data.polygon = polygon;
    data.enabled = enabled;
    m_Dirty = true;
    ++m_Version;
}

void NavigationServer2D::RemoveCarveObstacle(const std::string& obstacleId) {
    if (m_CarveObstacles.erase(obstacleId) > 0) {
        m_Dirty = true;
        ++m_Version;
    }
}

/* ===================== Dynamic avoidance ===================== */

void NavigationServer2D::UpdateAvoidanceAgent(const std::string& agentId, const Vec2& position,
                                              float radius, const Vec2& prefVelocity, float maxSpeed) {
    AvoidanceAgentData& data = m_AvoidanceAgents[agentId];
    data.position = position;
    data.radius = radius;
    data.prefVelocity = prefVelocity;
    data.maxSpeed = maxSpeed;
}

void NavigationServer2D::RemoveAvoidanceAgent(const std::string& agentId) {
    m_AvoidanceAgents.erase(agentId);
}

void NavigationServer2D::UpdateAvoidanceObstacle(const std::string& obstacleId,
                                                 const Vec2& position, float radius) {
    AvoidanceObstacleData& data = m_AvoidanceObstacles[obstacleId];
    data.position = position;
    data.radius = radius;
}

void NavigationServer2D::RemoveAvoidanceObstacle(const std::string& obstacleId) {
    m_AvoidanceObstacles.erase(obstacleId);
}

math::Vec2 NavigationServer2D::ComputeAvoidanceVelocity(const std::string& agentId, float timeStep,
                                                        float neighborDistance, int maxNeighbors,
                                                        float timeHorizon, float timeHorizonObstacle) {
    auto selfIt = m_AvoidanceAgents.find(agentId);
    if (selfIt == m_AvoidanceAgents.end()) {
        return Vec2::Zero();
    }
    const AvoidanceAgentData& self = selfIt->second;

    float neighborDistSq = neighborDistance * neighborDistance;

    // Gather neighbouring agents (nearest first, capped at maxNeighbors).
    std::vector<std::pair<float, ORCAAgentInput>> candidates;
    for (const auto& entry : m_AvoidanceAgents) {
        if (entry.first == agentId) {
            continue;
        }
        const AvoidanceAgentData& other = entry.second;
        float distSq = (other.position - self.position).LengthSquared();
        if (distSq > neighborDistSq) {
            continue;
        }
        ORCAAgentInput input;
        input.position = other.position;
        input.velocity = other.prefVelocity;
        input.radius = other.radius;
        candidates.push_back({distSq, input});
    }
    std::sort(candidates.begin(), candidates.end(),
              [](const std::pair<float, ORCAAgentInput>& a,
                 const std::pair<float, ORCAAgentInput>& b) { return a.first < b.first; });

    std::vector<ORCAAgentInput> neighbors;
    int cap = (maxNeighbors > 0) ? maxNeighbors : static_cast<int>(candidates.size());
    for (int i = 0; i < static_cast<int>(candidates.size()) && i < cap; ++i) {
        neighbors.push_back(candidates[i].second);
    }

    std::vector<ORCADisc> discs;
    for (const auto& entry : m_AvoidanceObstacles) {
        const AvoidanceObstacleData& obstacle = entry.second;
        float distSq = (obstacle.position - self.position).LengthSquared();
        float reach = neighborDistance + obstacle.radius;
        if (distSq > reach * reach) {
            continue;
        }
        ORCADisc disc;
        disc.position = obstacle.position;
        disc.radius = obstacle.radius;
        discs.push_back(disc);
    }

    ORCAAgentInput selfInput;
    selfInput.position = self.position;
    selfInput.velocity = self.prefVelocity;
    selfInput.radius = self.radius;

    return ComputeORCAVelocity(selfInput, self.prefVelocity, self.maxSpeed,
                               neighbors, discs, timeStep, timeHorizon, timeHorizonObstacle);
}

/* ===================== Rebuild ===================== */

void NavigationServer2D::RebuildIfDirty() {
    if (!m_Dirty) {
        return;
    }
    m_CombinedMesh.Clear();

    for (auto& entry : m_Regions) {
        RegionData& region = entry.second;
        region.polygonCount = 0;
        if (!region.enabled || region.outline.size() < 3) {
            continue;
        }

        std::vector<std::vector<Vec2>> holes = region.holes;

        // A carve obstacle contributes a hole to this region if all of its
        // vertices lie within the region outline.
        for (const auto& obstacleEntry : m_CarveObstacles) {
            const CarveObstacleData& obstacle = obstacleEntry.second;
            if (!obstacle.enabled || obstacle.polygon.size() < 3) {
                continue;
            }
            bool contained = true;
            for (const Vec2& v : obstacle.polygon) {
                if (!geom::PointInPolygon(v, region.outline)) {
                    contained = false;
                    break;
                }
            }
            if (contained) {
                holes.push_back(obstacle.polygon);
            }
        }

        size_t before = m_CombinedMesh.PolygonCount();
        m_CombinedMesh.AddTriangulatedOutline(region.outline, holes);
        region.polygonCount = m_CombinedMesh.PolygonCount() - before;
    }

    m_CombinedMesh.BuildConnectivity();
    m_Dirty = false;
}

/* ===================== Queries ===================== */

bool NavigationServer2D::QueryPath(const Vec2& from, const Vec2& to,
                                   std::vector<Vec2>& outPath, float agentRadius) {
    RebuildIfDirty();
    return m_CombinedMesh.FindPath(from, to, outPath, agentRadius);
}

Vec2 NavigationServer2D::GetClosestPoint(const Vec2& p) {
    RebuildIfDirty();
    return m_CombinedMesh.GetClosestPoint(p);
}

bool NavigationServer2D::IsPointNavigable(const Vec2& p) {
    RebuildIfDirty();
    return m_CombinedMesh.FindContainingPolygon(p) >= 0;
}

void NavigationServer2D::Clear() {
    bool had = !m_Regions.empty() || !m_CarveObstacles.empty();
    m_Regions.clear();
    m_CarveObstacles.clear();
    m_AvoidanceAgents.clear();
    m_AvoidanceObstacles.clear();
    m_CombinedMesh.Clear();
    m_Dirty = false;
    if (had) {
        ++m_Version;
    }
}

} // namespace navigation
} // namespace lupine
