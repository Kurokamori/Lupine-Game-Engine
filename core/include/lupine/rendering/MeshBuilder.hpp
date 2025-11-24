#pragma once

#include "Mesh.hpp"
#include "lupine/math/Vec2.hpp"
#include "lupine/math/Vec3.hpp"
#include "lupine/math/Vec4.hpp"

namespace lupine {

// Import math types into lupine namespace for rendering
using math::Vec2;
using math::Vec3;
using math::Vec4;

/**
 * Utility class for building meshes procedurally.
 */
class MeshBuilder {
public:
    MeshBuilder();

    // Add vertices
    void addVertex(const Vec3& position, const Vec3& normal = Vec3(0, 1, 0),
                   const Vec2& texCoord = Vec2(0, 0), const Vec4& color = Vec4(1, 1, 1, 1));

    // Add indices
    void addIndex(uint32_t index);
    void addTriangle(uint32_t i0, uint32_t i1, uint32_t i2);
    void addQuad(uint32_t i0, uint32_t i1, uint32_t i2, uint32_t i3);

    // Build the final mesh
    MeshData build();

    // Clear all data
    void clear();

    // ===== Primitive Generators =====

    /**
     * Generate a quad (2 triangles).
     * Centered at origin, facing +Z.
     */
    static MeshData createQuad(float width = 1.0f, float height = 1.0f);

    /**
     * Generate a cube.
     */
    static MeshData createCube(float size = 1.0f);

    /**
     * Generate a sphere.
     */
    static MeshData createSphere(float radius = 0.5f, uint32_t segments = 32, uint32_t rings = 16);

    /**
     * Generate a cylinder.
     */
    static MeshData createCylinder(float radius = 0.5f, float height = 1.0f, uint32_t segments = 32);

    /**
     * Generate a plane (grid).
     */
    static MeshData createPlane(float width = 1.0f, float depth = 1.0f, uint32_t widthSegments = 1, uint32_t depthSegments = 1);

    /**
     * Generate a circle (2D).
     */
    static MeshData createCircle(float radius = 0.5f, uint32_t segments = 32);

    /**
     * Generate a cone.
     */
    static MeshData createCone(float radius = 0.5f, float height = 1.0f, uint32_t segments = 32);

    /**
     * Generate a pyramid (4-sided).
     */
    static MeshData createPyramid(float baseSize = 1.0f, float height = 1.0f);

    /**
     * Generate a torus (donut shape).
     */
    static MeshData createTorus(float majorRadius = 0.5f, float minorRadius = 0.2f, uint32_t majorSegments = 32, uint32_t minorSegments = 16);

    /**
     * Generate a capsule (cylinder with hemisphere caps).
     */
    static MeshData createCapsule(float radius = 0.5f, float height = 1.0f, uint32_t segments = 32, uint32_t rings = 8);

private:
    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;
};

} // namespace lupine
