#include "lupine/rendering/MeshSimplifier.hpp"
#include "lupine/math/Vec3.hpp"

#include <vector>
#include <array>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <limits>

namespace lupine {

using math::Vec3;

namespace {

// Symmetric 4x4 quadric stored as its 10 upper-triangular coefficients.
struct Quadric {
    double q[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    // Add a plane (a,b,c,d) with (a,b,c) a unit normal, weighted by `weight`.
    void AddPlane(double a, double b, double c, double d, double weight) {
        q[0] += weight * a * a;
        q[1] += weight * a * b;
        q[2] += weight * a * c;
        q[3] += weight * a * d;
        q[4] += weight * b * b;
        q[5] += weight * b * c;
        q[6] += weight * b * d;
        q[7] += weight * c * c;
        q[8] += weight * c * d;
        q[9] += weight * d * d;
    }

    void Add(const Quadric& other) {
        for (int i = 0; i < 10; ++i) {
            q[i] += other.q[i];
        }
    }

    // Evaluate v^T Q v for point p.
    double Error(const Vec3& p) const {
        double x = static_cast<double>(p.x);
        double y = static_cast<double>(p.y);
        double z = static_cast<double>(p.z);
        return q[0] * x * x + 2.0 * q[1] * x * y + 2.0 * q[2] * x * z + 2.0 * q[3] * x
             + q[4] * y * y + 2.0 * q[5] * y * z + 2.0 * q[6] * y
             + q[7] * z * z + 2.0 * q[8] * z
             + q[9];
    }
};

struct WeldKey {
    int64_t x;
    int64_t y;
    int64_t z;
    bool operator==(const WeldKey& o) const { return x == o.x && y == o.y && z == o.z; }
};

struct WeldKeyHash {
    std::size_t operator()(const WeldKey& k) const {
        std::size_t h = static_cast<std::size_t>(k.x) * 73856093u;
        h ^= static_cast<std::size_t>(k.y) * 19349663u;
        h ^= static_cast<std::size_t>(k.z) * 83492791u;
        return h;
    }
};

struct Triangle {
    uint32_t v[3];
    bool alive = true;
};

// Lazily-validated heap entry: only collapsed if the recorded endpoint versions
// still match the current versions (otherwise the edge has been superseded).
struct EdgeEntry {
    double cost;
    uint32_t a;
    uint32_t b;
    uint32_t versionA;
    uint32_t versionB;
    bool operator>(const EdgeEntry& o) const { return cost > o.cost; }
};

inline uint64_t EdgeId(uint32_t a, uint32_t b) {
    if (a > b) {
        std::swap(a, b);
    }
    return (static_cast<uint64_t>(a) << 32) | static_cast<uint64_t>(b);
}

Vec3 FaceNormal(const Vec3& a, const Vec3& b, const Vec3& c) {
    return (b - a).Cross(c - a);
}

} // namespace

MeshData MeshSimplifier::Simplify(const MeshData& input, float targetRatio) {
    if (targetRatio >= 1.0f || input.indices.size() < 3 || input.vertices.empty()) {
        return input;
    }
    targetRatio = std::clamp(targetRatio, 0.01f, 1.0f);

    // Determine a welding epsilon relative to the mesh extent so that vertices
    // duplicated at seams (common in imported meshes) collapse as one surface.
    Vec3 boundsMin(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    Vec3 boundsMax(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
    for (const Vertex& v : input.vertices) {
        boundsMin.x = std::min(boundsMin.x, v.position.x);
        boundsMin.y = std::min(boundsMin.y, v.position.y);
        boundsMin.z = std::min(boundsMin.z, v.position.z);
        boundsMax.x = std::max(boundsMax.x, v.position.x);
        boundsMax.y = std::max(boundsMax.y, v.position.y);
        boundsMax.z = std::max(boundsMax.z, v.position.z);
    }
    float diagonal = (boundsMax - boundsMin).Length();
    double epsilon = diagonal > 0.0f ? static_cast<double>(diagonal) * 1e-5 : 1e-6;
    double invEpsilon = 1.0 / epsilon;

    // Weld vertices by quantized position.
    std::vector<Vec3> positions;
    std::vector<uint32_t> representative;     // welded -> original vertex (for attributes)
    std::vector<uint32_t> origToWeld(input.vertices.size());
    std::unordered_map<WeldKey, uint32_t, WeldKeyHash> weldMap;
    weldMap.reserve(input.vertices.size());

    for (size_t i = 0; i < input.vertices.size(); ++i) {
        const Vec3& p = input.vertices[i].position;
        WeldKey key{
            static_cast<int64_t>(std::llround(p.x * invEpsilon)),
            static_cast<int64_t>(std::llround(p.y * invEpsilon)),
            static_cast<int64_t>(std::llround(p.z * invEpsilon))
        };
        auto it = weldMap.find(key);
        if (it != weldMap.end()) {
            origToWeld[i] = it->second;
        } else {
            uint32_t weldIndex = static_cast<uint32_t>(positions.size());
            weldMap.emplace(key, weldIndex);
            positions.push_back(p);
            representative.push_back(static_cast<uint32_t>(i));
            origToWeld[i] = weldIndex;
        }
    }

    // Build welded triangles, dropping any that became degenerate after welding.
    std::vector<Triangle> triangles;
    triangles.reserve(input.indices.size() / 3);
    for (size_t i = 0; i + 2 < input.indices.size(); i += 3) {
        uint32_t a = origToWeld[input.indices[i]];
        uint32_t b = origToWeld[input.indices[i + 1]];
        uint32_t c = origToWeld[input.indices[i + 2]];
        if (a == b || b == c || a == c) {
            continue;
        }
        triangles.push_back(Triangle{{a, b, c}, true});
    }

    if (triangles.size() < 2) {
        return input;
    }

    const uint32_t vertexCount = static_cast<uint32_t>(positions.size());

    // Count how many triangles reference each undirected edge (1 == boundary).
    std::unordered_map<uint64_t, int> edgeFaceCount;
    edgeFaceCount.reserve(triangles.size() * 3);
    for (const Triangle& tri : triangles) {
        edgeFaceCount[EdgeId(tri.v[0], tri.v[1])]++;
        edgeFaceCount[EdgeId(tri.v[1], tri.v[2])]++;
        edgeFaceCount[EdgeId(tri.v[2], tri.v[0])]++;
    }

    // Accumulate per-vertex quadrics (area-weighted face planes + boundary planes).
    std::vector<Quadric> quadrics(vertexCount);
    std::vector<std::vector<uint32_t>> vertTriangles(vertexCount);

    for (uint32_t ti = 0; ti < triangles.size(); ++ti) {
        const Triangle& tri = triangles[ti];
        const Vec3& p0 = positions[tri.v[0]];
        const Vec3& p1 = positions[tri.v[1]];
        const Vec3& p2 = positions[tri.v[2]];

        Vec3 faceN = FaceNormal(p0, p1, p2);
        double area2 = static_cast<double>(faceN.Length());
        Vec3 n = area2 > 0.0 ? Vec3(faceN.x / static_cast<float>(area2),
                                    faceN.y / static_cast<float>(area2),
                                    faceN.z / static_cast<float>(area2))
                             : Vec3(0.0f, 0.0f, 0.0f);
        double d = -static_cast<double>(n.Dot(p0));
        double weight = area2 * 0.5; // triangle area

        for (int k = 0; k < 3; ++k) {
            quadrics[tri.v[k]].AddPlane(n.x, n.y, n.z, d, weight);
            vertTriangles[tri.v[k]].push_back(ti);
        }

        // Preserve open boundaries: add a plane perpendicular to the face along
        // each boundary edge, weighted heavily so the silhouette is anchored.
        const uint32_t order[3][2] = {{0, 1}, {1, 2}, {2, 0}};
        for (int e = 0; e < 3; ++e) {
            uint32_t va = tri.v[order[e][0]];
            uint32_t vb = tri.v[order[e][1]];
            if (edgeFaceCount[EdgeId(va, vb)] != 1) {
                continue;
            }
            Vec3 edgeDir = positions[vb] - positions[va];
            Vec3 perp = edgeDir.Cross(n);
            float perpLen = perp.Length();
            if (perpLen <= 0.0f) {
                continue;
            }
            perp = Vec3(perp.x / perpLen, perp.y / perpLen, perp.z / perpLen);
            double pd = -static_cast<double>(perp.Dot(positions[va]));
            double boundaryWeight = (weight + 1e-6) * 100.0;
            quadrics[va].AddPlane(perp.x, perp.y, perp.z, pd, boundaryWeight);
            quadrics[vb].AddPlane(perp.x, perp.y, perp.z, pd, boundaryWeight);
        }
    }

    std::vector<uint32_t> version(vertexCount, 0);
    std::vector<bool> vertexAlive(vertexCount, true);

    // Pick the best survivor position/cost for collapsing edge (a,b).
    auto evaluateCollapse = [&](uint32_t a, uint32_t b, Vec3& outTarget) -> double {
        Quadric combined = quadrics[a];
        combined.Add(quadrics[b]);

        Vec3 candidates[3] = {
            positions[a],
            positions[b],
            Vec3((positions[a].x + positions[b].x) * 0.5f,
                 (positions[a].y + positions[b].y) * 0.5f,
                 (positions[a].z + positions[b].z) * 0.5f)
        };

        double bestCost = std::numeric_limits<double>::max();
        for (const Vec3& c : candidates) {
            double cost = combined.Error(c);
            if (cost < bestCost) {
                bestCost = cost;
                outTarget = c;
            }
        }
        return bestCost;
    };

    // Reject a collapse that would invert any incident face.
    auto wouldFlip = [&](uint32_t keep, uint32_t remove, const Vec3& target) -> bool {
        for (uint32_t pass = 0; pass < 2; ++pass) {
            uint32_t base = (pass == 0) ? keep : remove;
            for (uint32_t ti : vertTriangles[base]) {
                if (!triangles[ti].alive) {
                    continue;
                }
                const Triangle& tri = triangles[ti];
                // Triangles containing both endpoints are removed by the collapse.
                bool hasKeep = false;
                bool hasRemove = false;
                for (int k = 0; k < 3; ++k) {
                    if (tri.v[k] == keep) hasKeep = true;
                    if (tri.v[k] == remove) hasRemove = true;
                }
                if (hasKeep && hasRemove) {
                    continue;
                }

                Vec3 oldP[3];
                Vec3 newP[3];
                for (int k = 0; k < 3; ++k) {
                    uint32_t vk = tri.v[k];
                    oldP[k] = positions[vk];
                    if (vk == keep || vk == remove) {
                        newP[k] = target;
                    } else {
                        newP[k] = positions[vk];
                    }
                }
                Vec3 oldN = FaceNormal(oldP[0], oldP[1], oldP[2]);
                Vec3 newN = FaceNormal(newP[0], newP[1], newP[2]);
                if (newN.Length() <= 0.0f) {
                    return true; // collapsed to a sliver
                }
                if (oldN.Dot(newN) < 0.0f) {
                    return true;
                }
            }
        }
        return false;
    };

    using Heap = std::priority_queue<EdgeEntry, std::vector<EdgeEntry>, std::greater<EdgeEntry>>;
    Heap heap;

    // Seed the heap with every unique edge.
    std::unordered_set<uint64_t> seenEdges;
    seenEdges.reserve(triangles.size() * 3);
    for (const Triangle& tri : triangles) {
        const uint32_t order[3][2] = {{0, 1}, {1, 2}, {2, 0}};
        for (int e = 0; e < 3; ++e) {
            uint32_t a = tri.v[order[e][0]];
            uint32_t b = tri.v[order[e][1]];
            uint64_t id = EdgeId(a, b);
            if (!seenEdges.insert(id).second) {
                continue;
            }
            Vec3 target;
            double cost = evaluateCollapse(a, b, target);
            heap.push(EdgeEntry{cost, a, b, version[a], version[b]});
        }
    }

    int aliveTriangles = static_cast<int>(triangles.size());
    int targetTriangles = std::max(1, static_cast<int>(std::lround(aliveTriangles * targetRatio)));

    while (aliveTriangles > targetTriangles && !heap.empty()) {
        EdgeEntry entry = heap.top();
        heap.pop();

        uint32_t a = entry.a;
        uint32_t b = entry.b;
        if (!vertexAlive[a] || !vertexAlive[b]) {
            continue;
        }
        if (entry.versionA != version[a] || entry.versionB != version[b]) {
            continue; // superseded
        }

        Vec3 target;
        evaluateCollapse(a, b, target);
        if (wouldFlip(a, b, target)) {
            continue;
        }

        // Collapse b into a. `a` survives at the chosen target position.
        positions[a] = target;
        quadrics[a].Add(quadrics[b]);

        for (uint32_t ti : vertTriangles[b]) {
            Triangle& tri = triangles[ti];
            if (!tri.alive) {
                continue;
            }
            for (int k = 0; k < 3; ++k) {
                if (tri.v[k] == b) {
                    tri.v[k] = a;
                }
            }
            if (tri.v[0] == tri.v[1] || tri.v[1] == tri.v[2] || tri.v[0] == tri.v[2]) {
                tri.alive = false;
                --aliveTriangles;
            } else {
                vertTriangles[a].push_back(ti);
            }
        }

        vertexAlive[b] = false;
        version[a]++;

        // Recompute edges from `a` to each surviving neighbor.
        std::unordered_set<uint32_t> neighbors;
        for (uint32_t ti : vertTriangles[a]) {
            const Triangle& tri = triangles[ti];
            if (!tri.alive) {
                continue;
            }
            for (int k = 0; k < 3; ++k) {
                uint32_t vk = tri.v[k];
                if (vk != a && vertexAlive[vk]) {
                    neighbors.insert(vk);
                }
            }
        }
        for (uint32_t n : neighbors) {
            Vec3 nTarget;
            double cost = evaluateCollapse(a, n, nTarget);
            heap.push(EdgeEntry{cost, a, n, version[a], version[n]});
        }
    }

    // Compact surviving geometry into an output mesh.
    std::vector<uint32_t> weldToOut(vertexCount, std::numeric_limits<uint32_t>::max());
    MeshData output;
    output.submeshes = input.submeshes;

    for (const Triangle& tri : triangles) {
        if (!tri.alive) {
            continue;
        }
        uint32_t outIdx[3];
        for (int k = 0; k < 3; ++k) {
            uint32_t w = tri.v[k];
            if (weldToOut[w] == std::numeric_limits<uint32_t>::max()) {
                weldToOut[w] = static_cast<uint32_t>(output.vertices.size());
                Vertex vert = input.vertices[representative[w]];
                vert.position = positions[w];
                vert.normal = Vec3(0.0f, 0.0f, 0.0f);
                output.vertices.push_back(vert);
            }
            outIdx[k] = weldToOut[w];
        }
        output.indices.push_back(outIdx[0]);
        output.indices.push_back(outIdx[1]);
        output.indices.push_back(outIdx[2]);
    }

    if (output.vertices.empty() || output.indices.size() < 3) {
        return input;
    }

    // Recompute smooth normals from the decimated geometry.
    for (size_t i = 0; i + 2 < output.indices.size(); i += 3) {
        uint32_t i0 = output.indices[i];
        uint32_t i1 = output.indices[i + 1];
        uint32_t i2 = output.indices[i + 2];
        Vec3 fn = FaceNormal(output.vertices[i0].position,
                             output.vertices[i1].position,
                             output.vertices[i2].position);
        output.vertices[i0].normal = output.vertices[i0].normal + fn;
        output.vertices[i1].normal = output.vertices[i1].normal + fn;
        output.vertices[i2].normal = output.vertices[i2].normal + fn;
    }
    for (Vertex& v : output.vertices) {
        float len = v.normal.Length();
        if (len > 0.0f) {
            v.normal = Vec3(v.normal.x / len, v.normal.y / len, v.normal.z / len);
        } else {
            v.normal = Vec3(0.0f, 1.0f, 0.0f);
        }
    }

    output.calculateBounds();
    return output;
}

} // namespace lupine
