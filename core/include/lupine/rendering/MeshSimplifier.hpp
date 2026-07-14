#pragma once

#include "lupine/rendering/Mesh.hpp"

namespace lupine {

/**
 * MeshSimplifier
 *
 * Quadric-error-metric (QEM) edge-collapse mesh decimation used to generate
 * lower-detail LOD meshes automatically when an author does not supply hand-made
 * LOD meshes. The algorithm:
 *   1. Welds vertices that share a position (so seams collapse as one surface).
 *   2. Accumulates a fundamental error quadric per vertex from its incident
 *      face planes (area-weighted), with extra perpendicular planes added along
 *      open boundary edges so silhouettes are preserved.
 *   3. Repeatedly collapses the lowest-error edge (choosing the cheaper of its
 *      endpoints or midpoint as the survivor) until the target triangle ratio is
 *      reached, skipping any collapse that would flip a face normal.
 *
 * It is intended to run once at load time (not per frame). Vertex attributes
 * (UV, color, tangent) are carried from the surviving welded vertex and normals
 * are recomputed from the decimated geometry.
 */
class MeshSimplifier {
public:
    /**
     * Decimate a mesh to approximately targetRatio of its original triangle
     * count. targetRatio is clamped to (0, 1]; a value >= 1 (or a mesh too small
     * to simplify) returns a copy of the input unchanged.
     */
    static MeshData Simplify(const MeshData& input, float targetRatio);
};

} // namespace lupine
