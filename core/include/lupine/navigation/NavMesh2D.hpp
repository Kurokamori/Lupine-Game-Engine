#pragma once

#include "lupine/math/Vec2.hpp"
#include <vector>
#include <array>

namespace lupine {
namespace navigation {

/**
 * A single convex navigation polygon within a NavMesh2D.
 *
 * Vertices are stored in counter-clockwise winding order in world space. The
 * `neighbors` array is parallel to the edge list: edge `i` runs from
 * vertices[i] to vertices[(i+1) % n], and neighbors[i] holds the index of the
 * polygon sharing that edge (or -1 if the edge is a border).
 */
struct NavPolygon {
    std::vector<math::Vec2> vertices;
    std::vector<int> neighbors;

    math::Vec2 Centroid() const;
};

/**
 * NavMesh2D - a connected mesh of convex navigation polygons plus the
 * pathfinding queries that operate over it.
 *
 * The mesh is populated either by adding pre-triangulated convex polygons
 * directly (AddPolygon) or by triangulating arbitrary simple outlines with
 * optional holes (AddTriangulatedOutline). After all geometry is added,
 * BuildConnectivity() links polygons that share an edge so that A* and the
 * funnel string-pull can route across the whole surface.
 *
 * All coordinates are world space. This type is rendering- and node-agnostic;
 * it depends only on math primitives so it can live in the core library.
 */
class NavMesh2D {
public:
    void Clear();

    /** Append a convex polygon (vertices in any winding; normalized to CCW). */
    void AddPolygon(const std::vector<math::Vec2>& verts);

    /**
     * Triangulate a simple (possibly concave) outline with optional holes and
     * append the resulting triangles as navigation polygons. Returns false if
     * triangulation failed (degenerate / self-intersecting input).
     */
    bool AddTriangulatedOutline(const std::vector<math::Vec2>& outline,
                                const std::vector<std::vector<math::Vec2>>& holes);

    /** Compute edge adjacency between all polygons. Call after adding geometry. */
    void BuildConnectivity(float epsilon = 0.01f);

    bool IsEmpty() const { return m_Polygons.empty(); }
    size_t PolygonCount() const { return m_Polygons.size(); }
    const std::vector<NavPolygon>& Polygons() const { return m_Polygons; }

    /** Index of the polygon containing `p`, or -1 if none contains it. */
    int FindContainingPolygon(const math::Vec2& p) const;

    /**
     * Index of the polygon nearest to `p`. `outClamped` receives the closest
     * point on that polygon (equal to `p` if it is already inside). Returns -1
     * only when the mesh is empty.
     */
    int FindNearestPolygon(const math::Vec2& p, math::Vec2& outClamped) const;

    /** Closest point on the mesh surface to `p` (returns `p` if mesh empty). */
    math::Vec2 GetClosestPoint(const math::Vec2& p) const;

    /**
     * Find a path from `start` to `end`. The returned path begins at the
     * clamped start point and ends at the clamped end point. Returns false if
     * the mesh is empty or the endpoints are not connected.
     *
     * `agentRadius` insets the corridor away from walls so an agent of that
     * radius can follow the path without clipping geometry (portals narrower
     * than 2*radius collapse to their midpoint).
     */
    bool FindPath(const math::Vec2& start, const math::Vec2& end,
                  std::vector<math::Vec2>& outPath, float agentRadius = 0.0f) const;

private:
    std::vector<NavPolygon> m_Polygons;

    bool AStar(int startPoly, int endPoly,
               const math::Vec2& start, const math::Vec2& end,
               std::vector<int>& outPolyPath) const;

    void StringPull(const math::Vec2& start, const math::Vec2& end,
                    const std::vector<int>& polyPath,
                    std::vector<math::Vec2>& outPath, float agentRadius) const;

    bool GetPortal(int from, int to, math::Vec2& outLeft, math::Vec2& outRight) const;
};

/**
 * Standalone 2D polygon geometry helpers used by the navigation baker. Kept
 * public so tests and tooling can exercise them directly.
 */
namespace geom {

float SignedArea(const std::vector<math::Vec2>& poly);

bool PointInTriangle(const math::Vec2& p,
                     const math::Vec2& a, const math::Vec2& b, const math::Vec2& c);

bool PointInPolygon(const math::Vec2& p, const std::vector<math::Vec2>& poly);

/** Closest point to `p` lying on or inside the convex polygon `poly`. */
math::Vec2 ClosestPointOnConvex(const math::Vec2& p, const std::vector<math::Vec2>& poly);

/**
 * Ear-clip a simple polygon (concave permitted, no holes, no self-intersection)
 * into triangles. Returns false on degenerate input.
 */
bool TriangulateSimple(const std::vector<math::Vec2>& poly,
                       std::vector<std::array<math::Vec2, 3>>& outTris);

/**
 * Triangulate an outer outline with zero or more holes. Holes are merged into
 * the outline via mutually-visible bridge edges, then ear-clipped. Returns
 * false on degenerate input.
 */
bool TriangulateWithHoles(const std::vector<math::Vec2>& outline,
                          const std::vector<std::vector<math::Vec2>>& holes,
                          std::vector<std::array<math::Vec2, 3>>& outTris);

} // namespace geom

} // namespace navigation
} // namespace lupine
