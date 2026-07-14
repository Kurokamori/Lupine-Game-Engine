#pragma once

#include "lupine/math/Vec3.hpp"
#include "lupine/math/AABB.hpp"
#include <vector>

namespace lupine {
namespace navigation {

/** A world-space input/output triangle for navmesh baking. */
struct NavMeshTriangle {
    math::Vec3 a;
    math::Vec3 b;
    math::Vec3 c;
};

/**
 * A world-space box volume that is subtracted from the walkable surface during
 * baking (used by static NavigationObstacle3D blockers). Any voxel column whose
 * walkable surface falls inside the box is marked non-walkable, and the agent
 * radius erosion then keeps paths clear of its edges.
 */
struct NavMeshCarveVolume {
    math::AABB box;
};

/**
 * Recast-style navmesh bake parameters. Lengths are in world units; the voxel
 * resolution is (cellSize x cellSize) in the horizontal plane and cellHeight
 * vertically.
 */
struct NavMeshBakeConfig {
    float cellSize = 0.3f;            // Horizontal voxel size (XZ).
    float cellHeight = 0.2f;          // Vertical voxel size (Y).
    float agentHeight = 2.0f;         // Required vertical clearance to walk.
    float agentRadius = 0.5f;         // Walkable surface is eroded by this.
    float agentMaxClimb = 0.4f;       // Max step height an agent can climb.
    float maxSlopeDegrees = 45.0f;    // Surfaces steeper than this are unwalkable.
    int minRegionArea = 8;            // Connected surfaces smaller than this many
                                      // voxels are discarded as noise.
    bool hasBounds = false;           // If false, bounds are derived from input.
    math::Vec3 boundsMin;             // Bake volume minimum (world).
    math::Vec3 boundsMax;             // Bake volume maximum (world).
};

/**
 * NavMeshBaker - builds a walkable navigation surface from arbitrary world-space
 * collision/mesh triangles, the way Recast does:
 *
 *   1. Voxelize every input triangle into a solid heightfield, tagging each
 *      span walkable or not by its slope.
 *   2. Filter the heightfield for the agent: reclaim low-hanging walkable
 *      ledges, drop ledge spans the agent would fall off, and remove spans
 *      without enough head-room (agentHeight).
 *   3. Build a compact heightfield of the top walkable surfaces with 4-way
 *      connectivity gated by agentMaxClimb and head-room.
 *   4. Subtract carve volumes, then erode the walkable area by agentRadius.
 *   5. Flood-fill the surface into connected regions, discard tiny islands, and
 *      emit a triangulated walkable mesh (greedy-merged for flat areas, smooth
 *      across slopes via shared corner heights).
 *
 * The output triangles are world-space and ready to feed NavMesh3D (which
 * normalizes their winding and links shared edges). The baker is a pure core
 * service: it has no scene, rendering or runtime dependencies and is fully
 * deterministic, so it can be unit-tested headlessly.
 */
class NavMeshBaker {
public:
    static bool Bake(const NavMeshBakeConfig& config,
                     const std::vector<NavMeshTriangle>& inputTriangles,
                     const std::vector<NavMeshCarveVolume>& carveVolumes,
                     std::vector<NavMeshTriangle>& outNavTriangles);

    /** Axis-aligned bounds enclosing every input triangle vertex. */
    static math::AABB ComputeBounds(const std::vector<NavMeshTriangle>& triangles);
};

} // namespace navigation
} // namespace lupine
