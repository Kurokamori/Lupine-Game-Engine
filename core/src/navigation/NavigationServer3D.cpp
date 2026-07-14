#include "lupine/navigation/NavigationServer3D.hpp"
#include "lupine/navigation/Avoidance2D.hpp"

#include <algorithm>

namespace lupine {
namespace navigation {

using math::Vec2;
using math::Vec3;

NavigationServer3D& NavigationServer3D::GetInstance() {
    static NavigationServer3D instance;
    return instance;
}

/* ===================== Regions ===================== */

void NavigationServer3D::UpdateRegion(const std::string& regionId,
                                      const std::vector<NavMeshTriangle>& triangles,
                                      bool enabled) {
    RegionData& data = m_Regions[regionId];
    data.triangles = triangles;
    data.enabled = enabled;
    m_Dirty = true;
    ++m_Version;
}

void NavigationServer3D::RemoveRegion(const std::string& regionId) {
    if (m_Regions.erase(regionId) > 0) {
        m_Dirty = true;
        ++m_Version;
    }
}

size_t NavigationServer3D::GetRegionPolygonCount(const std::string& regionId) {
    RebuildIfDirty();
    auto it = m_Regions.find(regionId);
    return (it != m_Regions.end()) ? it->second.polygonCount : 0;
}

/* ===================== Carve volumes ===================== */

void NavigationServer3D::UpdateCarveVolume(const std::string& volumeId,
                                           const math::AABB& box, bool enabled) {
    CarveData& data = m_CarveVolumes[volumeId];
    data.box = box;
    data.enabled = enabled;
    ++m_CarveVersion;
}

void NavigationServer3D::RemoveCarveVolume(const std::string& volumeId) {
    if (m_CarveVolumes.erase(volumeId) > 0) {
        ++m_CarveVersion;
    }
}

std::vector<NavMeshCarveVolume> NavigationServer3D::GetActiveCarveVolumes() const {
    std::vector<NavMeshCarveVolume> result;
    result.reserve(m_CarveVolumes.size());
    for (const auto& entry : m_CarveVolumes) {
        if (entry.second.enabled) {
            NavMeshCarveVolume v;
            v.box = entry.second.box;
            result.push_back(v);
        }
    }
    return result;
}

/* ===================== Dynamic avoidance ===================== */

void NavigationServer3D::UpdateAvoidanceAgent(const std::string& agentId, const Vec3& position,
                                              float radius, const Vec3& prefVelocity, float maxSpeed) {
    AvoidanceAgentData& data = m_AvoidanceAgents[agentId];
    data.position = position;
    data.radius = radius;
    data.prefVelocity = prefVelocity;
    data.maxSpeed = maxSpeed;
}

void NavigationServer3D::RemoveAvoidanceAgent(const std::string& agentId) {
    m_AvoidanceAgents.erase(agentId);
}

void NavigationServer3D::UpdateAvoidanceObstacle(const std::string& obstacleId,
                                                 const Vec3& position, float radius) {
    AvoidanceObstacleData& data = m_AvoidanceObstacles[obstacleId];
    data.position = position;
    data.radius = radius;
}

void NavigationServer3D::RemoveAvoidanceObstacle(const std::string& obstacleId) {
    m_AvoidanceObstacles.erase(obstacleId);
}

math::Vec3 NavigationServer3D::ComputeAvoidanceVelocity(const std::string& agentId, float timeStep,
                                                        float neighborDistance, int maxNeighbors,
                                                        float timeHorizon, float timeHorizonObstacle) {
    auto selfIt = m_AvoidanceAgents.find(agentId);
    if (selfIt == m_AvoidanceAgents.end()) {
        return Vec3::Zero();
    }
    const AvoidanceAgentData& self = selfIt->second;

    auto toXZ = [](const Vec3& v) { return Vec2(v.x, v.z); };
    const Vec2 selfPos = toXZ(self.position);
    const Vec2 prefVel = toXZ(self.prefVelocity);

    float neighborDistSq = neighborDistance * neighborDistance;

    std::vector<std::pair<float, ORCAAgentInput>> candidates;
    for (const auto& entry : m_AvoidanceAgents) {
        if (entry.first == agentId) {
            continue;
        }
        const AvoidanceAgentData& other = entry.second;
        Vec2 otherPos = toXZ(other.position);
        float distSq = (otherPos - selfPos).LengthSquared();
        if (distSq > neighborDistSq) {
            continue;
        }
        ORCAAgentInput input;
        input.position = otherPos;
        input.velocity = toXZ(other.prefVelocity);
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
        Vec2 obstaclePos = toXZ(obstacle.position);
        float distSq = (obstaclePos - selfPos).LengthSquared();
        float reach = neighborDistance + obstacle.radius;
        if (distSq > reach * reach) {
            continue;
        }
        ORCADisc disc;
        disc.position = obstaclePos;
        disc.radius = obstacle.radius;
        discs.push_back(disc);
    }

    ORCAAgentInput selfInput;
    selfInput.position = selfPos;
    selfInput.velocity = prefVel;
    selfInput.radius = self.radius;

    Vec2 result = ComputeORCAVelocity(selfInput, prefVel, self.maxSpeed,
                                      neighbors, discs, timeStep, timeHorizon, timeHorizonObstacle);
    return Vec3(result.x, 0.0f, result.y);
}

/* ===================== Rebuild ===================== */

void NavigationServer3D::RebuildIfDirty() {
    if (!m_Dirty) {
        return;
    }
    m_CombinedMesh.Clear();

    for (auto& entry : m_Regions) {
        RegionData& region = entry.second;
        region.polygonCount = 0;
        if (!region.enabled) {
            continue;
        }
        size_t before = m_CombinedMesh.PolygonCount();
        for (const NavMeshTriangle& t : region.triangles) {
            m_CombinedMesh.AddTriangle(t.a, t.b, t.c);
        }
        region.polygonCount = m_CombinedMesh.PolygonCount() - before;
    }

    m_CombinedMesh.BuildConnectivity();
    m_Dirty = false;
}

/* ===================== Queries ===================== */

bool NavigationServer3D::QueryPath(const Vec3& from, const Vec3& to,
                                   std::vector<Vec3>& outPath, float agentRadius) {
    RebuildIfDirty();
    return m_CombinedMesh.FindPath(from, to, outPath, agentRadius);
}

Vec3 NavigationServer3D::GetClosestPoint(const Vec3& p) {
    RebuildIfDirty();
    return m_CombinedMesh.GetClosestPoint(p);
}

bool NavigationServer3D::IsPointNavigable(const Vec3& p, float verticalTolerance) {
    RebuildIfDirty();
    return m_CombinedMesh.FindContainingPolygon(p, verticalTolerance) >= 0;
}

bool NavigationServer3D::SampleHeight(float x, float z, float& outY) {
    RebuildIfDirty();
    return m_CombinedMesh.SampleHeight(x, z, outY);
}

void NavigationServer3D::Clear() {
    bool had = !m_Regions.empty() || !m_CarveVolumes.empty();
    m_Regions.clear();
    m_CarveVolumes.clear();
    m_AvoidanceAgents.clear();
    m_AvoidanceObstacles.clear();
    m_CombinedMesh.Clear();
    m_Dirty = false;
    if (had) {
        ++m_Version;
        ++m_CarveVersion;
    }
}

} // namespace navigation
} // namespace lupine
