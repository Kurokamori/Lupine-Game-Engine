#include "lupine/navigation/NavMeshBaker.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace lupine {
namespace navigation {

using math::Vec3;
using math::AABB;

namespace {

constexpr int kNullArea = 0;
constexpr int kWalkableArea = 63;
constexpr int kMaxHeight = 0xffff;
constexpr int kNotConnected = -1;

// Recast neighbour direction offsets: 0=-X, 1=+Z, 2=+X, 3=-Z.
constexpr int kDirX[4] = {-1, 0, 1, 0};
constexpr int kDirZ[4] = {0, 1, 0, -1};

// ============================================================================
// Solid heightfield
// ============================================================================

struct Span {
    int smin;
    int smax;
    int area;
    int next;
};

struct Heightfield {
    int w = 0;
    int h = 0;
    float cs = 0.0f;
    float ch = 0.0f;
    Vec3 bmin;
    Vec3 bmax;
    std::vector<int> columns; // w*h heads, -1 = empty
    std::vector<Span> spans;  // pool
};

void AddSpan(Heightfield& hf, int x, int z, int smin, int smax, int area, int mergeThr) {
    const int ci = x + z * hf.w;
    int head = hf.columns[ci];
    Span s{smin, smax, area, -1};

    int prev = -1;
    int cur = head;
    while (cur != -1) {
        Span cs = hf.spans[cur];
        if (cs.smin > s.smax) {
            break; // current span sits entirely above the new one
        } else if (cs.smax < s.smin) {
            prev = cur;
            cur = cs.next; // current span sits entirely below
        } else {
            // Overlap or touch: merge into the new span and unlink current.
            if (cs.smin < s.smin) s.smin = cs.smin;
            if (cs.smax > s.smax) s.smax = cs.smax;
            if (std::abs(s.smax - cs.smax) <= mergeThr) {
                s.area = std::max(s.area, cs.area);
            }
            int nxt = cs.next;
            if (prev == -1) {
                head = nxt;
            } else {
                hf.spans[prev].next = nxt;
            }
            cur = nxt;
        }
    }

    int idx = static_cast<int>(hf.spans.size());
    s.next = cur;
    hf.spans.push_back(s);
    if (prev == -1) {
        head = idx;
    } else {
        hf.spans[prev].next = idx;
    }
    hf.columns[ci] = head;
}

// Split polygon `in` (nin verts) by the axis-aligned plane coord == split along
// `axis` (0=x,1=y,2=z). out1 receives the part <= split, out2 the part >= split.
// Boundary vertices land in both. Buffers must hold up to nin+1 vertices.
void DividePoly(const Vec3* in, int nin, Vec3* out1, int* nout1,
                Vec3* out2, int* nout2, float split, int axis) {
    float d[16];
    for (int i = 0; i < nin; ++i) {
        d[i] = split - in[i][axis];
    }
    int m = 0;
    int n = 0;
    for (int i = 0, j = nin - 1; i < nin; j = i, ++i) {
        bool ina = d[j] >= 0.0f;
        bool inb = d[i] >= 0.0f;
        if (ina != inb) {
            float s = d[j] / (d[j] - d[i]);
            Vec3 v = in[j] + (in[i] - in[j]) * s;
            out1[m++] = v;
            out2[n++] = v;
            if (d[i] > 0.0f) {
                out1[m++] = in[i];
            } else if (d[i] < 0.0f) {
                out2[n++] = in[i];
            }
        } else {
            if (d[i] >= 0.0f) {
                out1[m++] = in[i];
                if (d[i] != 0.0f) {
                    continue;
                }
            }
            out2[n++] = in[i];
        }
    }
    *nout1 = m;
    *nout2 = n;
}

void RasterizeTriangle(Heightfield& hf, const Vec3& v0, const Vec3& v1, const Vec3& v2,
                       int area, int mergeThr) {
    const float cs = hf.cs;
    const float ics = 1.0f / hf.cs;
    const float ich = 1.0f / hf.ch;
    const float by = hf.bmax.y - hf.bmin.y;

    Vec3 tmin = v0;
    Vec3 tmax = v0;
    auto vmin = [](Vec3& a, const Vec3& b) {
        a.x = std::min(a.x, b.x); a.y = std::min(a.y, b.y); a.z = std::min(a.z, b.z);
    };
    auto vmax = [](Vec3& a, const Vec3& b) {
        a.x = std::max(a.x, b.x); a.y = std::max(a.y, b.y); a.z = std::max(a.z, b.z);
    };
    vmin(tmin, v1); vmin(tmin, v2);
    vmax(tmax, v1); vmax(tmax, v2);

    if (tmax.x < hf.bmin.x || tmin.x > hf.bmax.x ||
        tmax.z < hf.bmin.z || tmin.z > hf.bmax.z) {
        return;
    }

    int z0 = static_cast<int>((tmin.z - hf.bmin.z) * ics);
    int z1 = static_cast<int>((tmax.z - hf.bmin.z) * ics);
    z0 = std::max(0, std::min(z0, hf.h - 1));
    z1 = std::max(0, std::min(z1, hf.h - 1));

    Vec3 buf[32];
    Vec3* in = buf;
    Vec3* inrow = buf + 8;
    Vec3* p1 = buf + 16;
    Vec3* p2 = buf + 24;
    in[0] = v0; in[1] = v1; in[2] = v2;
    int nvIn = 3;

    for (int z = z0; z <= z1; ++z) {
        const float cz = hf.bmin.z + z * cs;
        int nvRow = 0;
        DividePoly(in, nvIn, inrow, &nvRow, p1, &nvIn, cz + cs, 2);
        std::swap(in, p1);
        if (nvRow < 3) {
            continue;
        }
        // Clip away everything below the row's lower bound; keep the strip.
        int nv2 = 0;
        DividePoly(inrow, nvRow, p1, &nv2, p2, &nvRow, cz, 2);
        // p2/nvRow now holds the row strip (>= cz). Use it for column clipping.
        std::swap(inrow, p2);
        int rowCount = nvRow;
        if (rowCount < 3) {
            continue;
        }

        float minX = inrow[0].x;
        float maxX = inrow[0].x;
        for (int i = 1; i < rowCount; ++i) {
            minX = std::min(minX, inrow[i].x);
            maxX = std::max(maxX, inrow[i].x);
        }
        int x0 = static_cast<int>((minX - hf.bmin.x) * ics);
        int x1 = static_cast<int>((maxX - hf.bmin.x) * ics);
        x0 = std::max(0, std::min(x0, hf.w - 1));
        x1 = std::max(0, std::min(x1, hf.w - 1));

        int nvCol = rowCount;
        for (int x = x0; x <= x1; ++x) {
            const float cx = hf.bmin.x + x * cs;
            int nv = 0;
            DividePoly(inrow, nvCol, p1, &nv, p2, &nvCol, cx + cs, 0);
            std::swap(inrow, p2);
            if (nv < 3) {
                continue;
            }
            float smin = p1[0].y;
            float smax = p1[0].y;
            for (int i = 1; i < nv; ++i) {
                smin = std::min(smin, p1[i].y);
                smax = std::max(smax, p1[i].y);
            }
            smin -= hf.bmin.y;
            smax -= hf.bmin.y;
            if (smax < 0.0f || smin > by) {
                continue;
            }
            smin = std::max(0.0f, smin);
            smax = std::min(by, smax);
            int ismin = std::max(0, static_cast<int>(std::floor(smin * ich)));
            int ismax = std::max(ismin + 1, static_cast<int>(std::ceil(smax * ich)));
            AddSpan(hf, x, z, ismin, ismax, area, mergeThr);
        }
    }
}

// ============================================================================
// Heightfield filtering
// ============================================================================

void FilterLowHangingWalkableObstacles(Heightfield& hf, int walkableClimb) {
    for (int z = 0; z < hf.h; ++z) {
        for (int x = 0; x < hf.w; ++x) {
            int prev = -1;
            bool prevWalkable = false;
            int prevArea = kNullArea;
            for (int si = hf.columns[x + z * hf.w]; si != -1; prev = si, si = hf.spans[si].next) {
                bool walkable = hf.spans[si].area != kNullArea;
                if (!walkable && prevWalkable && prev != -1) {
                    if (std::abs(hf.spans[si].smax - hf.spans[prev].smax) <= walkableClimb) {
                        hf.spans[si].area = prevArea;
                    }
                }
                prevWalkable = walkable;
                prevArea = hf.spans[si].area;
            }
        }
    }
}

void FilterLedgeSpans(Heightfield& hf, int walkableHeight, int walkableClimb) {
    for (int z = 0; z < hf.h; ++z) {
        for (int x = 0; x < hf.w; ++x) {
            for (int si = hf.columns[x + z * hf.w]; si != -1; si = hf.spans[si].next) {
                if (hf.spans[si].area == kNullArea) {
                    continue;
                }
                const int bot = hf.spans[si].smax;
                const int top = hf.spans[si].next != -1 ? hf.spans[hf.spans[si].next].smin : kMaxHeight;

                int minh = kMaxHeight;
                int asmin = bot;
                int asmax = bot;

                for (int dir = 0; dir < 4; ++dir) {
                    int dx = x + kDirX[dir];
                    int dz = z + kDirZ[dir];
                    if (dx < 0 || dz < 0 || dx >= hf.w || dz >= hf.h) {
                        minh = std::min(minh, -walkableClimb - bot);
                        break;
                    }
                    int nsi = hf.columns[dx + dz * hf.w];
                    int nbot = -walkableClimb;
                    int ntop = nsi != -1 ? hf.spans[nsi].smin : kMaxHeight;
                    if (std::min(top, ntop) - std::max(bot, nbot) > walkableHeight) {
                        minh = std::min(minh, nbot - bot);
                    }
                    for (; nsi != -1; nsi = hf.spans[nsi].next) {
                        nbot = hf.spans[nsi].smax;
                        ntop = hf.spans[nsi].next != -1 ? hf.spans[hf.spans[nsi].next].smin : kMaxHeight;
                        if (std::min(top, ntop) - std::max(bot, nbot) > walkableHeight) {
                            minh = std::min(minh, nbot - bot);
                            if (std::abs(nbot - bot) <= walkableClimb) {
                                if (nbot < asmin) asmin = nbot;
                                if (nbot > asmax) asmax = nbot;
                            }
                        }
                    }
                }

                if (minh < -walkableClimb) {
                    hf.spans[si].area = kNullArea;
                } else if ((asmax - asmin) > walkableClimb) {
                    hf.spans[si].area = kNullArea;
                }
            }
        }
    }
}

void FilterWalkableLowHeightSpans(Heightfield& hf, int walkableHeight) {
    for (int z = 0; z < hf.h; ++z) {
        for (int x = 0; x < hf.w; ++x) {
            for (int si = hf.columns[x + z * hf.w]; si != -1; si = hf.spans[si].next) {
                const int bot = hf.spans[si].smax;
                const int top = hf.spans[si].next != -1 ? hf.spans[hf.spans[si].next].smin : kMaxHeight;
                if ((top - bot) < walkableHeight) {
                    hf.spans[si].area = kNullArea;
                }
            }
        }
    }
}

// ============================================================================
// Compact heightfield
// ============================================================================

struct CompactSpan {
    int y = 0;       // Floor elevation (cell units) = top of the solid span.
    int h = 0;       // Clearance above floor (cell units).
    int area = kNullArea;
    int reg = -1;    // Connected-region id.
    int con[4] = {kNotConnected, kNotConnected, kNotConnected, kNotConnected};
};

struct CompactCell {
    int index = 0;
    int count = 0;
};

struct CompactHeightfield {
    int w = 0;
    int h = 0;
    float cs = 0.0f;
    float ch = 0.0f;
    Vec3 bmin;
    int walkableHeight = 0;
    int walkableClimb = 0;
    std::vector<CompactCell> cells;
    std::vector<CompactSpan> spans;
};

void BuildCompactHeightfield(const Heightfield& hf, int walkableHeight, int walkableClimb,
                             CompactHeightfield& chf) {
    chf.w = hf.w;
    chf.h = hf.h;
    chf.cs = hf.cs;
    chf.ch = hf.ch;
    chf.bmin = hf.bmin;
    chf.walkableHeight = walkableHeight;
    chf.walkableClimb = walkableClimb;
    chf.cells.assign(static_cast<size_t>(hf.w) * hf.h, CompactCell());

    int idx = 0;
    for (int z = 0; z < hf.h; ++z) {
        for (int x = 0; x < hf.w; ++x) {
            CompactCell& c = chf.cells[x + z * hf.w];
            c.index = idx;
            c.count = 0;
            for (int si = hf.columns[x + z * hf.w]; si != -1; si = hf.spans[si].next) {
                const Span& s = hf.spans[si];
                if (s.area == kNullArea) {
                    continue;
                }
                const int bot = s.smax;
                const int top = s.next != -1 ? hf.spans[s.next].smin : kMaxHeight;
                CompactSpan cs;
                cs.y = std::max(0, std::min(bot, 0xffff));
                cs.h = std::max(0, std::min(top - bot, 0xffff));
                cs.area = s.area;
                chf.spans.push_back(cs);
                ++idx;
                ++c.count;
            }
        }
    }

    // Build 4-way connectivity gated by climb and head-room.
    for (int z = 0; z < hf.h; ++z) {
        for (int x = 0; x < hf.w; ++x) {
            const CompactCell& c = chf.cells[x + z * hf.w];
            for (int i = c.index; i < c.index + c.count; ++i) {
                CompactSpan& s = chf.spans[i];
                for (int dir = 0; dir < 4; ++dir) {
                    s.con[dir] = kNotConnected;
                    int dx = x + kDirX[dir];
                    int dz = z + kDirZ[dir];
                    if (dx < 0 || dz < 0 || dx >= hf.w || dz >= hf.h) {
                        continue;
                    }
                    const CompactCell& nc = chf.cells[dx + dz * hf.w];
                    for (int k = nc.index; k < nc.index + nc.count; ++k) {
                        const CompactSpan& ns = chf.spans[k];
                        int bot = std::max(s.y, ns.y);
                        int topc = std::min(s.y + s.h, ns.y + ns.h);
                        if ((topc - bot) >= walkableHeight &&
                            std::abs(ns.y - s.y) <= walkableClimb) {
                            s.con[dir] = k - nc.index;
                            break;
                        }
                    }
                }
            }
        }
    }
}

inline int NeighborSpanIndex(const CompactHeightfield& chf, int x, int z, int dir,
                             const CompactSpan& s) {
    if (s.con[dir] == kNotConnected) {
        return -1;
    }
    int dx = x + kDirX[dir];
    int dz = z + kDirZ[dir];
    return chf.cells[dx + dz * chf.w].index + s.con[dir];
}

void ApplyCarveVolumes(CompactHeightfield& chf, const std::vector<NavMeshCarveVolume>& carves) {
    if (carves.empty()) {
        return;
    }
    for (int z = 0; z < chf.h; ++z) {
        for (int x = 0; x < chf.w; ++x) {
            const CompactCell& c = chf.cells[x + z * chf.w];
            for (int i = c.index; i < c.index + c.count; ++i) {
                CompactSpan& s = chf.spans[i];
                if (s.area == kNullArea) {
                    continue;
                }
                Vec3 wc(chf.bmin.x + (x + 0.5f) * chf.cs,
                        chf.bmin.y + s.y * chf.ch,
                        chf.bmin.z + (z + 0.5f) * chf.cs);
                for (const NavMeshCarveVolume& carve : carves) {
                    if (carve.box.Contains(wc)) {
                        s.area = kNullArea;
                        break;
                    }
                }
            }
        }
    }
}

void ErodeWalkableArea(CompactHeightfield& chf, int radius) {
    const int w = chf.w;
    const int h = chf.h;
    const int nspans = static_cast<int>(chf.spans.size());
    std::vector<unsigned char> dist(nspans, 0xff);

    // Mark boundary cells (non-walkable, or walkable with a missing neighbour).
    for (int z = 0; z < h; ++z) {
        for (int x = 0; x < w; ++x) {
            const CompactCell& c = chf.cells[x + z * w];
            for (int i = c.index; i < c.index + c.count; ++i) {
                const CompactSpan& s = chf.spans[i];
                if (s.area == kNullArea) {
                    dist[i] = 0;
                    continue;
                }
                int nc = 0;
                for (int dir = 0; dir < 4; ++dir) {
                    if (s.con[dir] != kNotConnected) {
                        ++nc;
                    }
                }
                if (nc != 4) {
                    dist[i] = 0;
                }
            }
        }
    }

    auto relax = [&](int i, int ai, int add) {
        int nd = std::min(static_cast<int>(dist[ai]) + add, 255);
        if (nd < dist[i]) {
            dist[i] = static_cast<unsigned char>(nd);
        }
    };

    // Forward pass.
    for (int z = 0; z < h; ++z) {
        for (int x = 0; x < w; ++x) {
            const CompactCell& c = chf.cells[x + z * w];
            for (int i = c.index; i < c.index + c.count; ++i) {
                const CompactSpan& s = chf.spans[i];
                if (s.con[0] != kNotConnected) {
                    int ai = NeighborSpanIndex(chf, x, z, 0, s);
                    relax(i, ai, 2);
                    const CompactSpan& as = chf.spans[ai];
                    if (as.con[3] != kNotConnected) {
                        int aai = NeighborSpanIndex(chf, x + kDirX[0], z + kDirZ[0], 3, as);
                        relax(i, aai, 3);
                    }
                }
                if (s.con[3] != kNotConnected) {
                    int ai = NeighborSpanIndex(chf, x, z, 3, s);
                    relax(i, ai, 2);
                    const CompactSpan& as = chf.spans[ai];
                    if (as.con[2] != kNotConnected) {
                        int aai = NeighborSpanIndex(chf, x + kDirX[3], z + kDirZ[3], 2, as);
                        relax(i, aai, 3);
                    }
                }
            }
        }
    }

    // Backward pass.
    for (int z = h - 1; z >= 0; --z) {
        for (int x = w - 1; x >= 0; --x) {
            const CompactCell& c = chf.cells[x + z * w];
            for (int i = c.index; i < c.index + c.count; ++i) {
                const CompactSpan& s = chf.spans[i];
                if (s.con[2] != kNotConnected) {
                    int ai = NeighborSpanIndex(chf, x, z, 2, s);
                    relax(i, ai, 2);
                    const CompactSpan& as = chf.spans[ai];
                    if (as.con[1] != kNotConnected) {
                        int aai = NeighborSpanIndex(chf, x + kDirX[2], z + kDirZ[2], 1, as);
                        relax(i, aai, 3);
                    }
                }
                if (s.con[1] != kNotConnected) {
                    int ai = NeighborSpanIndex(chf, x, z, 1, s);
                    relax(i, ai, 2);
                    const CompactSpan& as = chf.spans[ai];
                    if (as.con[0] != kNotConnected) {
                        int aai = NeighborSpanIndex(chf, x + kDirX[1], z + kDirZ[1], 0, as);
                        relax(i, aai, 3);
                    }
                }
            }
        }
    }

    const int thr = radius * 2;
    for (int i = 0; i < nspans; ++i) {
        if (dist[i] < thr) {
            chf.spans[i].area = kNullArea;
        }
    }
}

// Flood-fill walkable spans into connected regions; returns region span counts.
void BuildRegions(CompactHeightfield& chf, std::vector<int>& regionCounts) {
    const int w = chf.w;
    int nextRegion = 0;
    std::vector<int> stack;
    for (int z = 0; z < chf.h; ++z) {
        for (int x = 0; x < w; ++x) {
            const CompactCell& c = chf.cells[x + z * w];
            for (int i = c.index; i < c.index + c.count; ++i) {
                if (chf.spans[i].area == kNullArea || chf.spans[i].reg != -1) {
                    continue;
                }
                int region = nextRegion++;
                int count = 0;
                stack.clear();
                chf.spans[i].reg = region;
                // Encode (x,z,spanIndex) on the stack.
                stack.push_back(x);
                stack.push_back(z);
                stack.push_back(i);
                while (!stack.empty()) {
                    int si = stack.back(); stack.pop_back();
                    int sz = stack.back(); stack.pop_back();
                    int sx = stack.back(); stack.pop_back();
                    ++count;
                    const CompactSpan& s = chf.spans[si];
                    for (int dir = 0; dir < 4; ++dir) {
                        if (s.con[dir] == kNotConnected) {
                            continue;
                        }
                        int nx = sx + kDirX[dir];
                        int nz = sz + kDirZ[dir];
                        int ni = chf.cells[nx + nz * w].index + s.con[dir];
                        CompactSpan& ns = chf.spans[ni];
                        if (ns.area == kNullArea || ns.reg != -1) {
                            continue;
                        }
                        ns.reg = region;
                        stack.push_back(nx);
                        stack.push_back(nz);
                        stack.push_back(ni);
                    }
                }
                regionCounts.push_back(count);
            }
        }
    }
}

// ============================================================================
// Surface mesh extraction (greedy rectangles, smooth across slopes)
// ============================================================================

void ExtractRegionMesh(const CompactHeightfield& chf, int region,
                       std::vector<NavMeshTriangle>& out) {
    const int w = chf.w;
    const int h = chf.h;

    // Region bounding box in cells.
    int minX = w, minZ = h, maxX = -1, maxZ = -1;
    for (int z = 0; z < h; ++z) {
        for (int x = 0; x < w; ++x) {
            const CompactCell& c = chf.cells[x + z * w];
            for (int i = c.index; i < c.index + c.count; ++i) {
                if (chf.spans[i].reg == region) {
                    minX = std::min(minX, x);
                    maxX = std::max(maxX, x);
                    minZ = std::min(minZ, z);
                    maxZ = std::max(maxZ, z);
                }
            }
        }
    }
    if (maxX < minX || maxZ < minZ) {
        return;
    }

    const int rw = maxX - minX + 1;
    const int rh = maxZ - minZ + 1;
    std::vector<int> floorY(static_cast<size_t>(rw) * rh, kMaxHeight); // -1 sentinel via kMaxHeight
    std::vector<unsigned char> occupied(static_cast<size_t>(rw) * rh, 0);

    for (int z = minZ; z <= maxZ; ++z) {
        for (int x = minX; x <= maxX; ++x) {
            const CompactCell& c = chf.cells[x + z * w];
            for (int i = c.index; i < c.index + c.count; ++i) {
                if (chf.spans[i].reg == region) {
                    int lx = x - minX;
                    int lz = z - minZ;
                    floorY[lx + lz * rw] = chf.spans[i].y;
                    occupied[lx + lz * rw] = 1;
                    break;
                }
            }
        }
    }

    // Continuous corner-height grid: average world Y of the occupied cells that
    // touch each corner, so adjacent quads (flats and ramps) stay watertight.
    const int cw = rw + 1;
    const int chh = rh + 1;
    std::vector<float> cornerSum(static_cast<size_t>(cw) * chh, 0.0f);
    std::vector<int> cornerCount(static_cast<size_t>(cw) * chh, 0);
    for (int lz = 0; lz < rh; ++lz) {
        for (int lx = 0; lx < rw; ++lx) {
            if (!occupied[lx + lz * rw]) {
                continue;
            }
            float wy = chf.bmin.y + floorY[lx + lz * rw] * chf.ch;
            const int corners[4][2] = {{lx, lz}, {lx + 1, lz}, {lx + 1, lz + 1}, {lx, lz + 1}};
            for (auto& cn : corners) {
                int ci = cn[0] + cn[1] * cw;
                cornerSum[ci] += wy;
                cornerCount[ci] += 1;
            }
        }
    }
    auto cornerHeight = [&](int cx, int cz) -> float {
        int ci = cx + cz * cw;
        return cornerCount[ci] > 0 ? cornerSum[ci] / static_cast<float>(cornerCount[ci]) : 0.0f;
    };

    auto worldX = [&](int lx) { return chf.bmin.x + (minX + lx) * chf.cs; };
    auto worldZ = [&](int lz) { return chf.bmin.z + (minZ + lz) * chf.cs; };

    std::vector<unsigned char> visited(static_cast<size_t>(rw) * rh, 0);
    for (int lz = 0; lz < rh; ++lz) {
        for (int lx = 0; lx < rw; ++lx) {
            if (!occupied[lx + lz * rw] || visited[lx + lz * rw]) {
                continue;
            }
            int key = floorY[lx + lz * rw];

            // Greedily grow a rectangle of equal-floor, unvisited cells.
            int rectW = 1;
            while (lx + rectW < rw) {
                int idx = (lx + rectW) + lz * rw;
                if (!occupied[idx] || visited[idx] || floorY[idx] != key) {
                    break;
                }
                ++rectW;
            }
            int rectH = 1;
            while (lz + rectH < rh) {
                bool rowOk = true;
                for (int k = 0; k < rectW; ++k) {
                    int idx = (lx + k) + (lz + rectH) * rw;
                    if (!occupied[idx] || visited[idx] || floorY[idx] != key) {
                        rowOk = false;
                        break;
                    }
                }
                if (!rowOk) {
                    break;
                }
                ++rectH;
            }
            for (int dz = 0; dz < rectH; ++dz) {
                for (int dx = 0; dx < rectW; ++dx) {
                    visited[(lx + dx) + (lz + dz) * rw] = 1;
                }
            }

            float x0 = worldX(lx);
            float x1 = worldX(lx + rectW);
            float z0 = worldZ(lz);
            float z1 = worldZ(lz + rectH);
            Vec3 c00(x0, cornerHeight(lx, lz), z0);
            Vec3 c10(x1, cornerHeight(lx + rectW, lz), z0);
            Vec3 c11(x1, cornerHeight(lx + rectW, lz + rectH), z1);
            Vec3 c01(x0, cornerHeight(lx, lz + rectH), z1);

            out.push_back({c00, c10, c11});
            out.push_back({c00, c11, c01});
        }
    }
}

} // namespace

// ============================================================================
// Public API
// ============================================================================

AABB NavMeshBaker::ComputeBounds(const std::vector<NavMeshTriangle>& triangles) {
    if (triangles.empty()) {
        return AABB();
    }
    Vec3 lo(std::numeric_limits<float>::max());
    Vec3 hi(-std::numeric_limits<float>::max());
    auto acc = [&](const Vec3& v) {
        lo.x = std::min(lo.x, v.x); lo.y = std::min(lo.y, v.y); lo.z = std::min(lo.z, v.z);
        hi.x = std::max(hi.x, v.x); hi.y = std::max(hi.y, v.y); hi.z = std::max(hi.z, v.z);
    };
    for (const NavMeshTriangle& t : triangles) {
        acc(t.a); acc(t.b); acc(t.c);
    }
    return AABB(lo, hi);
}

bool NavMeshBaker::Bake(const NavMeshBakeConfig& config,
                        const std::vector<NavMeshTriangle>& inputTriangles,
                        const std::vector<NavMeshCarveVolume>& carveVolumes,
                        std::vector<NavMeshTriangle>& outNavTriangles) {
    outNavTriangles.clear();
    if (inputTriangles.empty()) {
        return false;
    }
    if (config.cellSize <= 0.0f || config.cellHeight <= 0.0f) {
        return false;
    }

    AABB bounds = config.hasBounds ? AABB(config.boundsMin, config.boundsMax)
                                   : ComputeBounds(inputTriangles);

    // Pad the bake volume so erosion has room and border voxels exist for the
    // ledge filter (avoids clipping the navmesh at the geometry edges).
    const int radiusInCells = std::max(0, static_cast<int>(std::ceil(config.agentRadius / config.cellSize)));
    const float padXZ = (radiusInCells + 2) * config.cellSize;
    const float padY = config.cellHeight * 2.0f;
    bounds.min -= Vec3(padXZ, padY, padXZ);
    bounds.max += Vec3(padXZ, padY, padXZ);

    Heightfield hf;
    hf.cs = config.cellSize;
    hf.ch = config.cellHeight;
    hf.bmin = bounds.min;
    hf.bmax = bounds.max;
    hf.w = static_cast<int>((bounds.max.x - bounds.min.x) / config.cellSize + 0.5f);
    hf.h = static_cast<int>((bounds.max.z - bounds.min.z) / config.cellSize + 0.5f);
    if (hf.w < 1 || hf.h < 1) {
        return false;
    }
    // Guard against pathological grid sizes.
    if (static_cast<long long>(hf.w) * hf.h > 8'000'000LL) {
        return false;
    }
    hf.columns.assign(static_cast<size_t>(hf.w) * hf.h, -1);

    const int walkableHeight = std::max(1, static_cast<int>(std::ceil(config.agentHeight / config.cellHeight)));
    const int walkableClimb = std::max(0, static_cast<int>(std::floor(config.agentMaxClimb / config.cellHeight)));
    const float walkableThr = std::cos(config.maxSlopeDegrees * 3.14159265358979323846f / 180.0f);

    // 1. Rasterize: voxelize each triangle, tagging walkable spans by slope.
    for (const NavMeshTriangle& t : inputTriangles) {
        Vec3 e0 = t.b - t.a;
        Vec3 e1 = t.c - t.a;
        Vec3 n = e0.Cross(e1).Normalized();
        int area = (n.y > walkableThr) ? kWalkableArea : kNullArea;
        RasterizeTriangle(hf, t.a, t.b, t.c, area, walkableClimb);
    }

    // 2. Filter the solid heightfield for the agent.
    FilterLowHangingWalkableObstacles(hf, walkableClimb);
    FilterLedgeSpans(hf, walkableHeight, walkableClimb);
    FilterWalkableLowHeightSpans(hf, walkableHeight);

    // 3. Compact the top walkable surfaces with connectivity.
    CompactHeightfield chf;
    BuildCompactHeightfield(hf, walkableHeight, walkableClimb, chf);
    if (chf.spans.empty()) {
        return false;
    }

    // 4. Subtract carve volumes, then erode by the agent radius.
    ApplyCarveVolumes(chf, carveVolumes);
    if (radiusInCells > 0) {
        ErodeWalkableArea(chf, radiusInCells);
    }

    // 5. Region flood-fill, island culling, and surface triangulation.
    std::vector<int> regionCounts;
    BuildRegions(chf, regionCounts);

    const int minRegionArea = std::max(0, config.minRegionArea);
    bool any = false;
    for (int region = 0; region < static_cast<int>(regionCounts.size()); ++region) {
        if (regionCounts[region] < minRegionArea) {
            continue;
        }
        ExtractRegionMesh(chf, region, outNavTriangles);
        any = true;
    }

    return any && !outNavTriangles.empty();
}

} // namespace navigation
} // namespace lupine
