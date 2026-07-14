#pragma once

#include "lupine/math/Vec3.hpp"
#include <vector>
#include <array>

namespace lupine {
namespace navigation {

/**
 * A single convex navigation polygon within a NavMesh3D.
 *
 * Vertices are stored counter-clockwise when viewed from above (looking down the
 * world +Y axis). The `neighbors` array is parallel to the edge list: edge `i`
 * runs from vertices[i] to vertices[(i+1) % n], and neighbors[i] holds the index
 * of the polygon sharing that edge (or -1 if the edge is a border).
 *
 * The walkable surface is treated as a height field: pathfinding and adjacency
 * are resolved on the horizontal (XZ) projection while every stored vertex keeps
 * its true world Y, so the path that comes out follows the terrain elevation.
 */
struct NavPolygon3D {
    std::vector<math::Vec3> vertices;
    std::vector<int> neighbors;

    math::Vec3 Centroid() const;
};

/**
 * NavMesh3D - a connected mesh of convex navigation polygons on a (mostly
 * horizontal) walkable surface, plus the pathfinding queries over it.
 *
 * Geometry is supplied as world-space convex polygons (typically triangles
 * emitted by the Recast-style NavMeshBaker). After all polygons are added,
 * BuildConnectivity() links those that share an edge so A* and the funnel
 * string-pull can route across the whole surface.
 *
 * Up is the world +Y axis. Containment, winding and the funnel operate on the
 * XZ projection; vertex elevation is preserved so the resulting path is fully
 * three-dimensional. This type is rendering- and node-agnostic and depends only
 * on math primitives, so it lives in the core library.
 */
class NavMesh3D {
public:
    void Clear();

    /** Append a triangle (any winding; normalized to CCW-from-above). */
    void AddTriangle(const math::Vec3& a, const math::Vec3& b, const math::Vec3& c);

    /** Append a convex polygon (any winding; normalized to CCW-from-above). */
    void AddPolygon(const std::vector<math::Vec3>& verts);

    /** Compute edge adjacency between all polygons. Call after adding geometry. */
    void BuildConnectivity(float epsilon = 0.01f);

    bool IsEmpty() const { return m_Polygons.empty(); }
    size_t PolygonCount() const { return m_Polygons.size(); }
    const std::vector<NavPolygon3D>& Polygons() const { return m_Polygons; }

    /**
     * Index of the polygon whose XZ projection contains `p` and whose surface is
     * vertically closest to `p`, or -1 if none. `verticalTolerance` rejects
     * polygons whose surface is farther than this in Y (<= 0 disables the test).
     */
    int FindContainingPolygon(const math::Vec3& p, float verticalTolerance = 0.0f) const;

    /**
     * Index of the polygon nearest to `p` in 3D. `outClamped` receives the
     * closest point on that polygon's surface. Returns -1 only when empty.
     */
    int FindNearestPolygon(const math::Vec3& p, math::Vec3& outClamped) const;

    /** Closest point on the mesh surface to `p` (returns `p` if mesh empty). */
    math::Vec3 GetClosestPoint(const math::Vec3& p) const;

    /** Surface elevation (world Y) at horizontal position (x, z), if covered. */
    bool SampleHeight(float x, float z, float& outY) const;

    /**
     * Find a path from `start` to `end`. The returned path begins at the clamped
     * start point and ends at the clamped end point, with intermediate corners
     * carrying their true surface elevation. Returns false if the mesh is empty
     * or the endpoints are not connected.
     *
     * `agentRadius` insets the corridor away from walls so an agent of that
     * radius can follow the path without clipping geometry (portals narrower
     * than 2*radius collapse to their midpoint).
     */
    bool FindPath(const math::Vec3& start, const math::Vec3& end,
                  std::vector<math::Vec3>& outPath, float agentRadius = 0.0f) const;

private:
    std::vector<NavPolygon3D> m_Polygons;

    bool AStar(int startPoly, int endPoly, const math::Vec3& end,
               std::vector<int>& outPolyPath) const;

    void StringPull(const math::Vec3& start, const math::Vec3& end,
                    const std::vector<int>& polyPath,
                    std::vector<math::Vec3>& outPath, float agentRadius) const;

    bool GetPortal(int from, int to, math::Vec3& outLeft, math::Vec3& outRight) const;
};

/**
 * Standalone planar polygon helpers used by the navigation baker and mesh. All
 * operate on the horizontal XZ plane with world +Y as up. Kept public so tests
 * and tooling can exercise them directly.
 */
namespace geom3d {

/** Twice the signed area of triangle (a,b,c) projected to XZ (CCW-from-above > 0). */
float TriArea2XZ(const math::Vec3& a, const math::Vec3& b, const math::Vec3& c);

/** Signed area of `poly` projected to XZ (CCW-from-above > 0). */
float SignedAreaXZ(const std::vector<math::Vec3>& poly);

/** True if `p` lies inside the convex polygon `poly` (XZ projection, CCW). */
bool PointInConvexXZ(const math::Vec3& p, const std::vector<math::Vec3>& poly);

/** True if `p` lies inside the simple polygon `poly` (XZ projection). */
bool PointInPolygonXZ(const math::Vec3& p, const std::vector<math::Vec3>& poly);

/**
 * Ear-clip a simple polygon (concave permitted, no holes) using its XZ
 * projection, preserving each vertex's Y. Output triangles reference indices
 * into `poly`. Returns false on degenerate input.
 */
bool TriangulateXZ(const std::vector<math::Vec3>& poly,
                   std::vector<std::array<int, 3>>& outTris);

} // namespace geom3d

} // namespace navigation
} // namespace lupine
