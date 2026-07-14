#include "lupine/navigation/NavMesh2D.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>

namespace lupine {
namespace navigation {

using math::Vec2;

namespace {

constexpr float kEpsilon = 1e-4f;

bool VEqual(const Vec2& a, const Vec2& b, float eps = kEpsilon) {
    return std::fabs(a.x - b.x) <= eps && std::fabs(a.y - b.y) <= eps;
}

float CrossZ(const Vec2& a, const Vec2& b) {
    return a.x * b.y - a.y * b.x;
}

float TriArea2(const Vec2& a, const Vec2& b, const Vec2& c) {
    return (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
}

Vec2 ClosestPointOnSegment(const Vec2& p, const Vec2& a, const Vec2& b) {
    Vec2 ab = b - a;
    float lenSq = ab.LengthSquared();
    if (lenSq <= kEpsilon * kEpsilon) {
        return a;
    }
    float t = (p - a).Dot(ab) / lenSq;
    t = std::max(0.0f, std::min(1.0f, t));
    return a + ab * t;
}

bool PointInConvexCCW(const Vec2& p, const std::vector<Vec2>& poly) {
    size_t n = poly.size();
    if (n < 3) {
        return false;
    }
    for (size_t i = 0; i < n; ++i) {
        const Vec2& a = poly[i];
        const Vec2& b = poly[(i + 1) % n];
        if (CrossZ(b - a, p - a) < -kEpsilon) {
            return false;
        }
    }
    return true;
}

} // namespace

// ============================================================================
// Geometry helpers
// ============================================================================

namespace geom {

float SignedArea(const std::vector<Vec2>& poly) {
    float area = 0.0f;
    size_t n = poly.size();
    for (size_t i = 0; i < n; ++i) {
        const Vec2& a = poly[i];
        const Vec2& b = poly[(i + 1) % n];
        area += (a.x * b.y - b.x * a.y);
    }
    return area * 0.5f;
}

bool PointInTriangle(const Vec2& p, const Vec2& a, const Vec2& b, const Vec2& c) {
    float d1 = TriArea2(a, b, p);
    float d2 = TriArea2(b, c, p);
    float d3 = TriArea2(c, a, p);
    bool hasNeg = (d1 < -kEpsilon) || (d2 < -kEpsilon) || (d3 < -kEpsilon);
    bool hasPos = (d1 > kEpsilon) || (d2 > kEpsilon) || (d3 > kEpsilon);
    return !(hasNeg && hasPos);
}

bool PointInPolygon(const Vec2& p, const std::vector<Vec2>& poly) {
    size_t n = poly.size();
    bool inside = false;
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const Vec2& vi = poly[i];
        const Vec2& vj = poly[j];
        bool straddles = (vi.y > p.y) != (vj.y > p.y);
        if (straddles) {
            float xCross = vj.x + (p.y - vj.y) / (vi.y - vj.y) * (vi.x - vj.x);
            if (p.x < xCross) {
                inside = !inside;
            }
        }
    }
    return inside;
}

Vec2 ClosestPointOnConvex(const Vec2& p, const std::vector<Vec2>& poly) {
    if (poly.empty()) {
        return p;
    }
    if (PointInConvexCCW(p, poly)) {
        return p;
    }
    Vec2 best = poly[0];
    float bestDistSq = std::numeric_limits<float>::max();
    size_t n = poly.size();
    for (size_t i = 0; i < n; ++i) {
        Vec2 cand = ClosestPointOnSegment(p, poly[i], poly[(i + 1) % n]);
        float distSq = (cand - p).LengthSquared();
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            best = cand;
        }
    }
    return best;
}

bool TriangulateSimple(const std::vector<Vec2>& polyIn,
                       std::vector<std::array<Vec2, 3>>& outTris) {
    if (polyIn.size() < 3) {
        return false;
    }

    std::vector<Vec2> poly = polyIn;
    if (SignedArea(poly) < 0.0f) {
        std::reverse(poly.begin(), poly.end());
    }

    std::vector<int> indices(poly.size());
    for (size_t i = 0; i < poly.size(); ++i) {
        indices[i] = static_cast<int>(i);
    }

    int guard = static_cast<int>(poly.size()) * static_cast<int>(poly.size()) + 16;

    while (indices.size() > 3 && guard-- > 0) {
        bool earClipped = false;
        size_t count = indices.size();
        for (size_t i = 0; i < count; ++i) {
            int ip = indices[(i + count - 1) % count];
            int ic = indices[i];
            int in = indices[(i + 1) % count];
            const Vec2& a = poly[ip];
            const Vec2& b = poly[ic];
            const Vec2& c = poly[in];

            if (CrossZ(b - a, c - b) <= kEpsilon) {
                continue; // reflex or collinear corner
            }

            bool contains = false;
            for (size_t k = 0; k < count; ++k) {
                int idx = indices[k];
                if (idx == ip || idx == ic || idx == in) {
                    continue;
                }
                const Vec2& q = poly[idx];
                if (VEqual(q, a) || VEqual(q, b) || VEqual(q, c)) {
                    continue;
                }
                if (PointInTriangle(q, a, b, c)) {
                    contains = true;
                    break;
                }
            }
            if (contains) {
                continue;
            }

            outTris.push_back({a, b, c});
            indices.erase(indices.begin() + i);
            earClipped = true;
            break;
        }
        if (!earClipped) {
            return false; // degenerate / self-intersecting input
        }
    }

    if (indices.size() == 3) {
        outTris.push_back({poly[indices[0]], poly[indices[1]], poly[indices[2]]});
        return true;
    }
    return false;
}

namespace {

// Locate a vertex of `merged` that is mutually visible from the hole's
// maximum-x vertex `M`, used as the anchor for a bridge edge. Returns the index
// into `merged`, or -1 if no visible vertex could be found.
int FindBridgeVertex(const std::vector<Vec2>& merged, const Vec2& M) {
    size_t n = merged.size();
    float bestX = std::numeric_limits<float>::max();
    int edgeStart = -1;
    Vec2 intersection;

    for (size_t i = 0; i < n; ++i) {
        const Vec2& a = merged[i];
        const Vec2& b = merged[(i + 1) % n];
        bool straddles = (a.y > M.y) != (b.y > M.y);
        if (!straddles) {
            continue;
        }
        float t = (M.y - a.y) / (b.y - a.y);
        float xCross = a.x + t * (b.x - a.x);
        if (xCross < M.x - kEpsilon) {
            continue; // intersection is to the left of M
        }
        if (xCross < bestX) {
            bestX = xCross;
            edgeStart = static_cast<int>(i);
            intersection = Vec2(xCross, M.y);
        }
    }

    if (edgeStart < 0) {
        // Fallback: nearest vertex by distance.
        int best = -1;
        float bestDistSq = std::numeric_limits<float>::max();
        for (size_t i = 0; i < n; ++i) {
            float distSq = (merged[i] - M).LengthSquared();
            if (distSq < bestDistSq) {
                bestDistSq = distSq;
                best = static_cast<int>(i);
            }
        }
        return best;
    }

    const Vec2& ea = merged[edgeStart];
    const Vec2& eb = merged[(edgeStart + 1) % n];
    int candidate = (ea.x >= eb.x) ? edgeStart : static_cast<int>((edgeStart + 1) % n);

    if (VEqual(intersection, merged[candidate])) {
        return candidate;
    }

    Vec2 P = merged[candidate];
    int best = candidate;
    float bestAngle = std::numeric_limits<float>::max();
    float bestDistSq = std::numeric_limits<float>::max();

    for (size_t i = 0; i < n; ++i) {
        const Vec2& prev = merged[(i + n - 1) % n];
        const Vec2& cur = merged[i];
        const Vec2& next = merged[(i + 1) % n];
        bool reflex = CrossZ(cur - prev, next - cur) < 0.0f;
        if (!reflex) {
            continue;
        }
        if (!PointInTriangle(cur, M, intersection, P)) {
            continue;
        }
        Vec2 dir = cur - M;
        float angle = std::fabs(std::atan2(dir.y, dir.x));
        float distSq = dir.LengthSquared();
        if (angle < bestAngle - kEpsilon ||
            (std::fabs(angle - bestAngle) <= kEpsilon && distSq < bestDistSq)) {
            bestAngle = angle;
            bestDistSq = distSq;
            best = static_cast<int>(i);
        }
    }
    return best;
}

} // namespace

bool TriangulateWithHoles(const std::vector<Vec2>& outline,
                          const std::vector<std::vector<Vec2>>& holes,
                          std::vector<std::array<Vec2, 3>>& outTris) {
    if (holes.empty()) {
        return TriangulateSimple(outline, outTris);
    }
    if (outline.size() < 3) {
        return false;
    }

    std::vector<Vec2> merged = outline;
    if (SignedArea(merged) < 0.0f) {
        std::reverse(merged.begin(), merged.end());
    }

    // Normalize hole winding to clockwise and order by descending max-x vertex.
    struct HoleEntry {
        std::vector<Vec2> verts;
        int maxXIndex;
        float maxX;
    };
    std::vector<HoleEntry> entries;
    for (const std::vector<Vec2>& holeIn : holes) {
        if (holeIn.size() < 3) {
            continue;
        }
        std::vector<Vec2> hole = holeIn;
        if (SignedArea(hole) > 0.0f) {
            std::reverse(hole.begin(), hole.end());
        }
        int maxIdx = 0;
        for (size_t i = 1; i < hole.size(); ++i) {
            if (hole[i].x > hole[maxIdx].x) {
                maxIdx = static_cast<int>(i);
            }
        }
        entries.push_back({hole, maxIdx, hole[maxIdx].x});
    }
    std::sort(entries.begin(), entries.end(),
              [](const HoleEntry& a, const HoleEntry& b) { return a.maxX > b.maxX; });

    for (const HoleEntry& entry : entries) {
        const Vec2 M = entry.verts[entry.maxXIndex];
        int bridge = FindBridgeVertex(merged, M);
        if (bridge < 0) {
            return false;
        }

        std::vector<Vec2> next;
        next.reserve(merged.size() + entry.verts.size() + 2);
        for (int i = 0; i <= bridge; ++i) {
            next.push_back(merged[i]);
        }
        size_t hn = entry.verts.size();
        for (size_t k = 0; k <= hn; ++k) {
            next.push_back(entry.verts[(entry.maxXIndex + k) % hn]);
        }
        next.push_back(merged[bridge]);
        for (size_t i = bridge + 1; i < merged.size(); ++i) {
            next.push_back(merged[i]);
        }
        merged = std::move(next);
    }

    return TriangulateSimple(merged, outTris);
}

} // namespace geom

// ============================================================================
// NavPolygon
// ============================================================================

Vec2 NavPolygon::Centroid() const {
    if (vertices.empty()) {
        return Vec2::Zero();
    }
    Vec2 sum = Vec2::Zero();
    for (const Vec2& v : vertices) {
        sum += v;
    }
    return sum / static_cast<float>(vertices.size());
}

// ============================================================================
// NavMesh2D
// ============================================================================

void NavMesh2D::Clear() {
    m_Polygons.clear();
}

void NavMesh2D::AddPolygon(const std::vector<Vec2>& verts) {
    if (verts.size() < 3) {
        return;
    }
    NavPolygon poly;
    poly.vertices = verts;
    if (geom::SignedArea(poly.vertices) < 0.0f) {
        std::reverse(poly.vertices.begin(), poly.vertices.end());
    }
    poly.neighbors.assign(poly.vertices.size(), -1);
    m_Polygons.push_back(std::move(poly));
}

bool NavMesh2D::AddTriangulatedOutline(const std::vector<Vec2>& outline,
                                       const std::vector<std::vector<Vec2>>& holes) {
    std::vector<std::array<Vec2, 3>> tris;
    if (!geom::TriangulateWithHoles(outline, holes, tris)) {
        return false;
    }
    for (const std::array<Vec2, 3>& tri : tris) {
        AddPolygon({tri[0], tri[1], tri[2]});
    }
    return true;
}

void NavMesh2D::BuildConnectivity(float epsilon) {
    for (NavPolygon& poly : m_Polygons) {
        poly.neighbors.assign(poly.vertices.size(), -1);
    }

    size_t count = m_Polygons.size();
    for (size_t a = 0; a < count; ++a) {
        NavPolygon& pa = m_Polygons[a];
        size_t na = pa.vertices.size();
        for (size_t ea = 0; ea < na; ++ea) {
            if (pa.neighbors[ea] != -1) {
                continue;
            }
            const Vec2& a0 = pa.vertices[ea];
            const Vec2& a1 = pa.vertices[(ea + 1) % na];

            for (size_t b = a + 1; b < count && pa.neighbors[ea] == -1; ++b) {
                NavPolygon& pb = m_Polygons[b];
                size_t nb = pb.vertices.size();
                for (size_t eb = 0; eb < nb; ++eb) {
                    if (pb.neighbors[eb] != -1) {
                        continue;
                    }
                    const Vec2& b0 = pb.vertices[eb];
                    const Vec2& b1 = pb.vertices[(eb + 1) % nb];
                    // Shared edges run in opposite directions between CCW polys.
                    if (VEqual(a0, b1, epsilon) && VEqual(a1, b0, epsilon)) {
                        pa.neighbors[ea] = static_cast<int>(b);
                        pb.neighbors[eb] = static_cast<int>(a);
                        break;
                    }
                }
            }
        }
    }
}

int NavMesh2D::FindContainingPolygon(const Vec2& p) const {
    for (size_t i = 0; i < m_Polygons.size(); ++i) {
        if (PointInConvexCCW(p, m_Polygons[i].vertices)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int NavMesh2D::FindNearestPolygon(const Vec2& p, Vec2& outClamped) const {
    int best = -1;
    float bestDistSq = std::numeric_limits<float>::max();
    for (size_t i = 0; i < m_Polygons.size(); ++i) {
        Vec2 cand = geom::ClosestPointOnConvex(p, m_Polygons[i].vertices);
        float distSq = (cand - p).LengthSquared();
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            best = static_cast<int>(i);
            outClamped = cand;
        }
    }
    return best;
}

Vec2 NavMesh2D::GetClosestPoint(const Vec2& p) const {
    if (m_Polygons.empty()) {
        return p;
    }
    Vec2 clamped = p;
    FindNearestPolygon(p, clamped);
    return clamped;
}

bool NavMesh2D::GetPortal(int from, int to, Vec2& outLeft, Vec2& outRight) const {
    const NavPolygon& poly = m_Polygons[from];
    size_t n = poly.vertices.size();
    for (size_t e = 0; e < n; ++e) {
        if (poly.neighbors[e] == to) {
            // For a CCW polygon the exit edge (v[e] -> v[e+1]) has v[e] on the
            // right of travel and v[e+1] on the left.
            outRight = poly.vertices[e];
            outLeft = poly.vertices[(e + 1) % n];
            return true;
        }
    }
    return false;
}

bool NavMesh2D::AStar(int startPoly, int endPoly,
                      const Vec2& start, const Vec2& end,
                      std::vector<int>& outPolyPath) const {
    (void)start;
    size_t count = m_Polygons.size();
    std::vector<float> gScore(count, std::numeric_limits<float>::max());
    std::vector<int> cameFrom(count, -1);
    std::vector<bool> closed(count, false);

    std::vector<Vec2> centroids(count);
    for (size_t i = 0; i < count; ++i) {
        centroids[i] = m_Polygons[i].Centroid();
    }

    auto heuristic = [&](int poly) {
        return (centroids[poly] - end).Length();
    };

    using Node = std::pair<float, int>; // (fScore, polygon)
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;

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

        const NavPolygon& poly = m_Polygons[current];
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

void NavMesh2D::StringPull(const Vec2& start, const Vec2& end,
                           const std::vector<int>& polyPath,
                           std::vector<Vec2>& outPath, float agentRadius) const {
    std::vector<std::pair<Vec2, Vec2>> portals; // (left, right)
    portals.push_back({start, start});
    for (size_t i = 0; i + 1 < polyPath.size(); ++i) {
        Vec2 left, right;
        if (GetPortal(polyPath[i], polyPath[i + 1], left, right)) {
            if (agentRadius > 0.0f) {
                // Inset both endpoints toward the portal centre so the agent's
                // centre stays at least `agentRadius` from each wall vertex.
                Vec2 edge = right - left;
                float width = edge.Length();
                if (width > 2.0f * agentRadius && width > kEpsilon) {
                    Vec2 dir = edge / width;
                    left = left + dir * agentRadius;
                    right = right - dir * agentRadius;
                } else {
                    Vec2 mid = (left + right) * 0.5f;
                    left = mid;
                    right = mid;
                }
            }
            portals.push_back({left, right});
        }
    }
    portals.push_back({end, end});

    outPath.clear();
    Vec2 apex = start;
    Vec2 left = start;
    Vec2 right = start;
    int apexIndex = 0;
    int leftIndex = 0;
    int rightIndex = 0;
    outPath.push_back(apex);

    int n = static_cast<int>(portals.size());
    for (int i = 1; i < n; ++i) {
        const Vec2& pLeft = portals[i].first;
        const Vec2& pRight = portals[i].second;

        // Tighten the right side.
        if (TriArea2(apex, right, pRight) <= 0.0f) {
            if (VEqual(apex, right) || TriArea2(apex, left, pRight) > 0.0f) {
                right = pRight;
                rightIndex = i;
            } else {
                if (!VEqual(outPath.back(), left)) {
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
        if (TriArea2(apex, left, pLeft) >= 0.0f) {
            if (VEqual(apex, left) || TriArea2(apex, right, pLeft) < 0.0f) {
                left = pLeft;
                leftIndex = i;
            } else {
                if (!VEqual(outPath.back(), right)) {
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

    if (outPath.empty() || !VEqual(outPath.back(), end)) {
        outPath.push_back(end);
    }
}

bool NavMesh2D::FindPath(const Vec2& start, const Vec2& end,
                         std::vector<Vec2>& outPath, float agentRadius) const {
    outPath.clear();
    if (m_Polygons.empty()) {
        return false;
    }

    Vec2 clampedStart;
    Vec2 clampedEnd;
    int startPoly = FindNearestPolygon(start, clampedStart);
    int endPoly = FindNearestPolygon(end, clampedEnd);
    if (startPoly < 0 || endPoly < 0) {
        return false;
    }

    if (startPoly == endPoly) {
        outPath.push_back(clampedStart);
        if (!VEqual(clampedStart, clampedEnd)) {
            outPath.push_back(clampedEnd);
        }
        return true;
    }

    std::vector<int> polyPath;
    if (!AStar(startPoly, endPoly, clampedStart, clampedEnd, polyPath)) {
        return false;
    }

    StringPull(clampedStart, clampedEnd, polyPath, outPath, agentRadius);
    return true;
}

} // namespace navigation
} // namespace lupine
