#pragma once

#include "ResourceHandles.hpp"
#include "lupine/math/Vec2.hpp"
#include "lupine/math/Vec3.hpp"
#include "lupine/math/Vec4.hpp"
#include "lupine/math/AABB.hpp"
#include <vector>
#include <string>

namespace lupine {

// Import math types into lupine namespace for rendering
using math::Vec2;
using math::Vec3;
using math::Vec4;
using math::AABB;

/**
 * Vertex attribute data
 */
struct alignas(4) Vertex {
    Vec3 position;    // offset 0, size 12
    Vec3 normal;      // offset 12, size 12
    Vec2 texCoord;    // offset 24, size 8
    Vec4 color = Vec4(1.0f, 1.0f, 1.0f, 1.0f);  // offset 32, size 16
    Vec3 tangent = Vec3(0.0f, 0.0f, 0.0f);      // offset 48, size 12

    // Skinning data (up to 4 bone influences per vertex)
    // Using Vec4 for alignment - x,y,z,w map to bone indices 0,1,2,3
    Vec4 boneIDs = Vec4(0.0f, 0.0f, 0.0f, 0.0f);          // offset 60, size 16 (0 not -1: DX12 root CBVs page-fault on index -1)
    Vec4 boneWeights = Vec4(0.0f, 0.0f, 0.0f, 0.0f);     // offset 76, size 16
    // Total: 92 bytes with 4-byte alignment
};

/**
 * Submesh - a drawable portion of a mesh
 */
struct Submesh {
    uint32_t indexOffset = 0;
    uint32_t indexCount = 0;
    uint32_t vertexOffset = 0;
    uint32_t vertexCount = 0;
    MaterialHandle material;
    AABB bounds;
};

/**
 * Mesh data structure
 */
struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Submesh> submeshes;
    AABB bounds;

    // Calculate bounding box from vertices
    void calculateBounds();

    // Add a submesh
    void addSubmesh(uint32_t indexCount, uint32_t vertexCount, MaterialHandle material = MaterialHandle());
};

/**
 * GPU mesh resource
 */
struct GPUMesh {
    BufferHandle vertexBuffer;
    BufferHandle indexBuffer;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    std::vector<Submesh> submeshes;
    AABB bounds;
};

} // namespace lupine
