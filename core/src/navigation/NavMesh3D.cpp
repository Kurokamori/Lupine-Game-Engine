#include "lupine/navigation/NavMesh3D.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>

namespace lupine {
namespace navigation {

using math::Vec3;

namespace {

constexpr float kEpsilon = 1e-4f;

bool VEqualXZ(const Vec3& a, const Vec3& b, float eps = kEpsilon) {
    return std::fabs(a.x - b.x) <= eps && std::fabs(a.z - b.z) <= eps;
}

bool VEqual3(const Vec3& a, const Vec3& b, float eps = kEpsilon) {
    return std::fabs(a.x - b.x) <= eps && std::fabs(a.y - b.y) <= eps &&
           std::fabs(a.z - b.z) <= eps;
}

Vec3 ClosestPointOnSegment3D(const Vec3& p, const Vec3& a, const Vec3& b) {
    Vec3 ab = b - a;
    float lenSq = ab.LengthSquared();
    if (lenSq <= kEpsilon * kEpsilon) {
        return a;
    }
    float t = (p - a).Dot(ab) / lenSq;
    t = std::max(0.0f, std::min(1.0f, t));
    return a + ab * t;
}

// Newell's method: area-weighted normal of an arbitrary planar polygon.
Vec3 PolygonNormal(const std::vector<Vec3>& poly) {
    Vec3 n = Vec3::Zero();
    size_t count = poly.size();
    for (size_t i = 0; i < count; ++i) {
        const Vec3& cur = poly[i];
        const Vec3& nxt = poly[(i + 1) % count];
        n.x += (cur.y - nxt.y) * (cur.z + nxt.z);
        n.y += (cur.z - nxt.z) * (cur.x + nxt.x);
        n.z += (cur.x - nxt.x) * (cur.y + nxt.y);
    }
    return n.Normalized();
}

// Surface elevation of a near-horizontal polygon at horizontal position (x, z).
float PlaneHeightXZ(const std::vector<Vec3>& poly, float x, float z) {
    if (poly.empty()) {
        return 0.0f;
    }
    Vec3 n = PolygonNormal(poly);
    const Vec3& v0 = poly[0];
    if (std::fabs(n.y) < 1e-5f) {
        return v0.y;
    }
    return v0.y - (n.x * (x - v0.x) + n.z * (z - v0.z)) / n.y;
}

// Closest point to `p` on the convex, near-horizontal polygon `poly`.
Vec3 ClosestPointOnConvex3D(const Vec3& p, const std::vector<Vec3>& poly) {
    if (poly.empty()) {
        return p;
    }
    if (geom3d::PointInConvexXZ(p, poly)) {
        return Vec3(p.x, PlaneHeightXZ(poly, p.x, p.z), p.z);
    }
    Vec3 best = poly[0];
    float bestDistSq = std::numeric_limits<float>::max();
    size_t n = poly.size();
    for (size_t i = 0; i < n; ++i) {
        Vec3 cand = ClosestPointOnSegment3D(p, poly[i], poly[(i + 1) % n]);
        float distSq = (cand - p).LengthSquared();
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            best = cand;
        }
    }
    return best;
}

} // namespace

// ============================================================================
// Geometry helpers
// ============================================================================

namespace geom3d {

float TriArea2XZ(const Vec3& a, const Vec3& b, const Vec3& c) {
    return (b.x - a.x) * (c.z - a.z) - (c.x - a.x) * (b.z - a.z);
}

float SignedAreaXZ(const std::vector<Vec3>& poly) {
    float area = 0.0f;
    size_t n = poly.size();
    for (size_t i = 0; i < n; ++i) {
        const Vec3& a = poly[i];
        const Vec3& b = poly[(i + 1) % n];
        area += (a.x * b.z - b.x * a.z);
    }
    return area * 0.5f;
}

bool PointInConvexXZ(const Vec3& p, const std::vector<Vec3>& poly) {
    size_t n = poly.size();
    if (n < 3) {
        return false;
    }
    for (size_t i = 0; i < n; ++i) {
        const Vec3& a = poly[i];
        const Vec3& b = poly[(i + 1) % n];
        if (TriArea2XZ(a, b, p) < -kEpsilon) {
            return false;
        }
    }
    return true;
}

bool PointInPolygonXZ(const Vec3& p, const std::vector<Vec3>& poly) {
    size_t n = poly.size();
    bool inside = false;
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const Vec3& vi = poly[i];
        const Vec3& vj = poly[j];
        bool straddles = (vi.z > p.z) != (vj.z > p.z);
        if (straddles) {
            float xCross = vj.x + (p.z - vj.z) / (vi.z - vj.z) * (vi.x - vj.x);
            if (p.x < xCross) {
                inside = !inside;
            }
        }
    }
    return inside;
}

bool TriangulateXZ(const std::vector<Vec3>& polyIn,
                   std::vector<std::array<int, 3>>& outTris) {
    size_t count = polyIn.size();
    if (count < 3) {
        return false;
    }

    std::vector<int> indices(count);
    bool reversed = SignedAreaXZ(polyIn) < 0.0f;
    for (size_t i = 0; i < count; ++i) {
        indices[i] = reversed ? static_cast<int>(count - 1 - i) : static_cast<int>(i);
    }

    auto triArea = [&](int ia, int ib, int ic) {
        return TriArea2XZ(polyIn[ia], polyIn[ib], polyIn[ic]);
    };
    auto pointInTri = [&](int ip, int ia, int ib, int ic) {
        float d1 = triArea(ia, ib, ip);
        float d2 = triArea(ib, ic, ip);
        float d3 = triArea(ic, ia, ip);
        bool hasNeg = (d1 < -kEpsilon) || (d2 < -kEpsilon) || (d3 < -kEpsilon);
        bool hasPos = (d1 > kEpsilon) || (d2 > kEpsilon) || (d3 > kEpsilon);
        return !(hasNeg && hasPos);
    };

    int guard = static_cast<int>(count) * static_cast<int>(count) + 16;

    while (indices.size() > 3 && guard-- > 0) {
        bool earClipped = false;
        size_t n = indices.size();
        for (size_t i = 0; i < n; ++i) {
            int ip = indices[(i + n - 1) % n];
            int ic = indices[i];
            int in = indices[(i + 1) % n];

            if (triArea(ip, ic, in) <= kEpsilon) {
                continue; // reflex or collinear corner
            }

            bool contains = false;
            for (size_t k = 0; k < n; ++k) {
                int idx = indices[k];
                if (idx == ip || idx == ic || idx == in) {
                    continue;
                }
                if (pointInTri(idx, ip, ic, in)) {
                    contains = true;
                    break;
                }
            }
            if (contains) {
                continue;
            }

            outTris.push_back({ip, ic, in});
            indices.erase(indices.begin() + static_cast<long>(i));
            earClipped = true;
            break;
        }
        if (!earClipped) {
            return false; // degenerate / self-intersecting input
        }
    }

    if (indices.size() == 3) {
        outTris.push_back({indices[0], indices[1], indices[2]});
        return true;
    }
    return false;
}

} // namespace geom3d

// ============================================================================
// NavPolygon3D
// ============================================================================

Vec3 NavPolygon3D::Centroid() const {
    if (vertices.empty()) {
        return Vec3::Zero();
    }
    Vec3 sum = Vec3::Zero();
    for (const Vec3& v : vertices) {
        sum += v;
    }
    return sum / static_cast<float>(vertices.size());
}

// ============================================================================
// NavMesh3D
// ============================================================================

void NavMesh3D::Clear() {
    m_Polygons.clear();
}

void NavMesh3D::AddTriangle(const Vec3& a, const Vec3& b, const Vec3& c) {
    AddPolygon({a, b, c});
}

void NavMesh3D::AddPolygon(const std::vector<Vec3>& verts) {
    if (verts.size() < 3) {
        return;
    }
    NavPolygon3D poly;
    poly.vertices = verts;
    if (geom3d::SignedAreaXZ(poly.vertices) < 0.0f) {
        std::reverse(poly.vertices.begin(), poly.vertices.end());
    }
    poly.neighbors.assign(poly.vertices.size(), -1);
    m_Polygons.push_back(std::move(poly));
}

void NavMesh3D::BuildConnectivity(float epsilon) {
    for (NavPolygon3D& poly : m_Polygons) {
        poly.neighbors.assign(poly.vertices.size(), -1);
    }

    size_t count = m_Polygons.size();
    for (size_t a = 0; a < count; ++a) {
        NavPolygon3D& pa = m_Polygons[a];
        size_t na = pa.vertices.size();
        for (size_t ea = 0; ea < na; ++ea) {
            if (pa.neighbors[ea] != -1) {
                continue;
            }
            const Vec3& a0 = pa.vertices[ea];
            const Vec3& a1 = pa.vertices[(ea + 1) % na];

            for (size_t b = a + 1; b < count && pa.neighbors[ea] == -1; ++b) {
                NavPolygon3D& pb = m_Polygons[b];
                size_t nb = pb.vertices.size();
                for (size_t eb = 0; eb < nb; ++eb) {
                    if (pb.neighbors[eb] != -1) {
                        continue;
                    }
                    const Vec3& b0 = pb.vertices[eb];
                    const Vec3& b1 = pb.vertices[(eb + 1) % nb];
                    // Shared edges run in opposite directions between like-wound
                    // polygons; match on the XZ projection so coincident floor
                    // edges at slightly different Y still connect.
                    if (VEqualXZ(a0, b1, epsilon) && VEqualXZ(a1, b0, epsilon)) {
                        pa.neighbors[ea] = static_cast<int>(b);
                        pb.neighbors[eb] = static_cast<int>(a);
                        break;
                    }
                }
            }
        }
    }
}

int NavMesh3D::FindContainingPolygon(const Vec3& p, float verticalTolerance) const {
    int best = -1;
    float bestDy = std::numeric_limits<float>::max();
    for (size_t i = 0; i < m_Polygons.size(); ++i) {
        const std::vector<Vec3>& verts = m_Polygons[i].vertices;
        if (!geom3d::PointInConvexXZ(p, verts)) {
            continue;
        }
        float surfaceY = PlaneHeightXZ(verts, p.x, p.z);
        float dy = std::fabs(surfaceY - p.y);
        if (verticalTolerance > 0.0f && dy > verticalTolerance) {
            continue;
        }
        if (dy < bestDy) {
            bestDy = dy;
            best = static_cast<int>(i);
        }
    }
    return best;
}

int NavMesh3D::FindNearestPolygon(const Vec3& p, Vec3& outClamped) const {
    int best = -1;
    float bestDistSq = std::numeric_limits<float>::max();
    for (size_t i = 0; i < m_Polygons.size(); ++i) {
        Vec3 cand = ClosestPointOnConvex3D(p, m_Polygons[i].vertices);
        float distSq = (cand - p).LengthSquared();
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            best = static_cast<int>(i);
            outClamped = cand;
        }
    }
    return best;
}

Vec3 NavMesh3D::GetClosestPoint(const Vec3& p) const {
    if (m_Polygons.empty()) {
        return p;
    }
    Vec3 clamped = p;
    FindNearestPolygon(p, clamped);
    return clamped;
}

bool NavMesh3D::SampleHeight(float x, float z, float& outY) const {
    Vec3 probe(x, 0.0f, z);
    int best = -1;
    for (size_t i = 0; i < m_Polygons.size(); ++i) {
        if (geom3d::PointInConvexXZ(probe, m_Polygons[i].vertices)) {
            best = static_cast<int>(i);
            break;
        }
    }
    if (best < 0) {
        return false;
    }
    outY = PlaneHeightXZ(m_Polygons[best].vertices, x, z);
    return true;
}

bool NavMesh3D::GetPortal(int from, int to, Vec3& outLeft, Vec3& outRight) const {
    const NavPolygon3D& poly = m_Polygons[from];
    size_t n = poly.vertices.size();
    for (size_t e = 0; e < n; ++e) {
        if (poly.neighbors[e] == to) {
            // For a CCW-from-above polygon the exit edge (v[e] -> v[e+1]) has
            // v[e] on the right of travel and v[e+1] on the left.
            outRight = poly.vertices[e];
            outLeft = poly.vertices[(e + 1) % n];
            return true;
        }
    }
    return false;
}

bool NavMesh3D::AStar(int startPoly, int endPoly, const Vec3& end,
                      std::vector<int>& outPolyPath) const {
    size_t count = m_Polygons.size();
    std::vector<float> gScore(count, std::numeric_limits<float>::max());
    std::vector<int> cameFrom(count, -1);
    std::vector<bool> closed(count, false);

    std::vector<Vec3> centroids(count);
    for (size_t i = 0; i < count; ++i) {
        centroids[i] = m_Polygons[i].Centroid();
    }

    auto heuristic = [&](int poly) {
        return (centroids[poly] - end).Length();
    };

    using PQNode = std::pair<float, int>; // (fScore, polygon)
    std::priority_queue<PQNode, std::vector<PQNode>, std::greater<PQNode>> open;

    gScore[startPoly] = 0.0f;
    open.push({heuristic(startPoly), startPoly});

    while (!open.empty()) {
        int current = open.top().second;
        open.pop();
        if (closed[current]) {
            continue;
        }
        if (current == endPoly) {
            outPolyPath.clear();
            for (int node = endPoly; node != -1; node = cameFrom[node]) {
                outPolyPath.push_back(node);
            }
            std::reverse(outPolyPath.begin(), outPolyPath.end());
            return true;
        }
        closed[current] = true;

        const NavPolygon3D& poly = m_Polygons[current];
        for (int neighbor : poly.neighbors) {
            if (neighbor < 0 || closed[neighbor]) {
                continue;
            }
            float tentative = gScore[current] +
                (centroids[neighbor] - centroids[current]).Length();
            if (tentative < gScore[neighbor]) {
                gScore[neighbor] = tentative;
                cameFrom[neighbor] = current;
                open.push({tentative + heuristic(neighbor), neighbor});
            }
        }
    }
    return false;
}

void NavMesh3D::StringPull(const Vec3& start, const Vec3& end,
                           const std::vector<int>& polyPath,
                           std::vector<Vec3>& outPath, float agentRadius) const {
    std::vector<std::pair<Vec3, Vec3>> portals; // (left, right)
    portals.push_back({start, start});
    for (size_t i = 0; i + 1 < polyPath.size(); ++i) {
        Vec3 left, right;
        if (GetPortal(polyPath[i], polyPath[i + 1], left, right)) {
            if (agentRadius > 0.0f) {
                Vec3 edge = right - left;
                float width = edge.Length();
                if (width > 2.0f * agentRadius && width > kEpsilon) {
                    Vec3 dir = edge / width;
                    left = left + dir * agentRadius;
                    right = right - dir * agentRadius;
                } else {
                    Vec3 mid = (left + right) * 0.5f;
                    left = mid;
                    right = mid;
                }
            }
            portals.push_back({left, right});
        }
    }
    portals.push_back({end, end});

    outPath.clear();
    Vec3 apex = start;
    Vec3 left = start;
    Vec3 right = start;
    int apexIndex = 0;
    int leftIndex = 0;
    int rightIndex = 0;
    outPath.push_back(apex);

    int n = static_cast<int>(portals.size());
    for (int i = 1; i < n; ++i) {
        const Vec3& pLeft = portals[i].first;
        const Vec3& pRight = portals[i].second;

        // Tighten the right side.
        if (geom3d::TriArea2XZ(apex, right, pRight) <= 0.0f) {
            if (VEqualXZ(apex, right) || geom3d::TriArea2XZ(apex, left, pRight) > 0.0f) {
                right = pRight;
                rightIndex = i;
            } else {
                if (!VEqual3(outPath.back(), left)) {
                    outPath.push_back(left);
                }
                apex = left;
                apexIndex = leftIndex;
                left = apex;
                right = apex;
                leftIndex = apexIndex;
                rightIndex = apexIndex;
                i = apexIndex;
                continue;
            }
        }

        // Tighten the left side.
        if (geom3d::TriArea2XZ(apex, left, pLeft) >= 0.0f) {
            if (VEqualXZ(apex, left) || geom3d::TriArea2XZ(apex, right, pLeft) < 0.0f) {
                left = pLeft;
                leftIndex = i;
            } else {
                if (!VEqual3(outPath.back(), right)) {
                    outPath.push_back(right);
                }
                apex = right;
                apexIndex = rightIndex;
                left = apex;
                right = apex;
                leftIndex = apexIndex;
                rightIndex = apexIndex;
                i = apexIndex;
                continue;
            }
        }
    }

    if (outPath.empty() || !VEqual3(outPath.back(), end)) {
        outPath.push_back(end);
    }
}

bool NavMesh3D::FindPath(const Vec3& start, const Vec3& end,
                         std::vector<Vec3>& outPath, float agentRadius) const {
    outPath.clear();
    if (m_Polygons.empty()) {
        return false;
    }

    Vec3 clampedStart;
    Vec3 clampedEnd;
    int startPoly = FindNearestPolygon(start, clampedStart);
    int endPoly = FindNearestPolygon(end, clampedEnd);
    if (startPoly < 0 || endPoly < 0) {
        return false;
    }

    if (startPoly == endPoly) {
        outPath.push_back(clampedStart);
        if (!VEqual3(clampedStart, clampedEnd)) {
            outPath.push_back(clampedEnd);
        }
        return true;
    }

    std::vector<int> polyPath;
    if (!AStar(startPoly, endPoly, clampedEnd, polyPath)) {
        return false;
    }

    StringPull(clampedStart, clampedEnd, polyPath, outPath, agentRadius);
    return true;
}

} // namespace navigation
} // namespace lupine
