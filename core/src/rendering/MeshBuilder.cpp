#include "lupine/rendering/MeshBuilder.hpp"
#include "lupine/math/MathCommon.hpp"
#include <cmath>
#include <algorithm>

namespace lupine {

namespace {

inline void FlipNormalsAndWinding(MeshData& mesh) {
    for (auto& v : mesh.vertices) {
        v.normal = -v.normal;
    }
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        std::swap(mesh.indices[i + 1], mesh.indices[i + 2]);
    }
}

}

MeshBuilder::MeshBuilder() {
}

void MeshBuilder::addVertex(const Vec3& position, const Vec3& normal,
                            const Vec2& texCoord, const Vec4& color) {
    Vertex v;
    v.position = position;
    v.normal = normal;
    v.texCoord = texCoord;
    v.color = color;
    m_vertices.push_back(v);
}

void MeshBuilder::addIndex(uint32_t index) {
    m_indices.push_back(index);
}

void MeshBuilder::addTriangle(uint32_t i0, uint32_t i1, uint32_t i2) {
    m_indices.push_back(i0);
    m_indices.push_back(i1);
    m_indices.push_back(i2);
}

void MeshBuilder::addQuad(uint32_t i0, uint32_t i1, uint32_t i2, uint32_t i3) {

    m_indices.push_back(i0);
    m_indices.push_back(i1);
    m_indices.push_back(i2);

    m_indices.push_back(i0);
    m_indices.push_back(i2);
    m_indices.push_back(i3);
}

MeshData MeshBuilder::build() {
    MeshData meshData;
    meshData.vertices = m_vertices;
    meshData.indices = m_indices;
    meshData.calculateBounds();

    if (!m_indices.empty()) {
        meshData.addSubmesh(
            static_cast<uint32_t>(m_indices.size()),
            static_cast<uint32_t>(m_vertices.size())
        );
    }

    return meshData;
}

void MeshBuilder::clear() {
    m_vertices.clear();
    m_indices.clear();
}

MeshData MeshBuilder::createQuad(float width, float height) {
    MeshBuilder builder;

    float halfW = width * 0.5f;
    float halfH = height * 0.5f;

    builder.addVertex(Vec3(-halfW, -halfH, 0.0f), Vec3(0, 0, -1), Vec2(0, 0));
    builder.addVertex(Vec3(-halfW,  halfH, 0.0f), Vec3(0, 0, -1), Vec2(0, 1));
    builder.addVertex(Vec3( halfW,  halfH, 0.0f), Vec3(0, 0, -1), Vec2(1, 1));
    builder.addVertex(Vec3( halfW, -halfH, 0.0f), Vec3(0, 0, -1), Vec2(1, 0));

    builder.addQuad(0, 1, 2, 3);

    MeshData mesh = builder.build();
    FlipNormalsAndWinding(mesh);
    return mesh;
}

MeshData MeshBuilder::createCube(float size) {
    MeshBuilder builder;

    float half = size * 0.5f;

    builder.addVertex(Vec3(-half, -half,  half), Vec3(0, 0, -1), Vec2(0, 0));
    builder.addVertex(Vec3(-half,  half,  half), Vec3(0, 0, -1), Vec2(0, 1));
    builder.addVertex(Vec3( half,  half,  half), Vec3(0, 0, -1), Vec2(1, 1));
    builder.addVertex(Vec3( half, -half,  half), Vec3(0, 0, -1), Vec2(1, 0));

    builder.addVertex(Vec3( half, -half, -half), Vec3(0, 0, 1), Vec2(0, 0));
    builder.addVertex(Vec3( half,  half, -half), Vec3(0, 0, 1), Vec2(0, 1));
    builder.addVertex(Vec3(-half,  half, -half), Vec3(0, 0, 1), Vec2(1, 1));
    builder.addVertex(Vec3(-half, -half, -half), Vec3(0, 0, 1), Vec2(1, 0));

    builder.addVertex(Vec3(-half,  half,  half), Vec3(0, -1, 0), Vec2(0, 0));
    builder.addVertex(Vec3(-half,  half, -half), Vec3(0, -1, 0), Vec2(0, 1));
    builder.addVertex(Vec3( half,  half, -half), Vec3(0, -1, 0), Vec2(1, 1));
    builder.addVertex(Vec3( half,  half,  half), Vec3(0, -1, 0), Vec2(1, 0));

    builder.addVertex(Vec3(-half, -half, -half), Vec3(0, 1, 0), Vec2(0, 0));
    builder.addVertex(Vec3(-half, -half,  half), Vec3(0, 1, 0), Vec2(0, 1));
    builder.addVertex(Vec3( half, -half,  half), Vec3(0, 1, 0), Vec2(1, 1));
    builder.addVertex(Vec3( half, -half, -half), Vec3(0, 1, 0), Vec2(1, 0));

    builder.addVertex(Vec3( half, -half,  half), Vec3(-1, 0, 0), Vec2(0, 0));
    builder.addVertex(Vec3( half,  half,  half), Vec3(-1, 0, 0), Vec2(0, 1));
    builder.addVertex(Vec3( half,  half, -half), Vec3(-1, 0, 0), Vec2(1, 1));
    builder.addVertex(Vec3( half, -half, -half), Vec3(-1, 0, 0), Vec2(1, 0));

    builder.addVertex(Vec3(-half, -half, -half), Vec3(1, 0, 0), Vec2(0, 0));
    builder.addVertex(Vec3(-half,  half, -half), Vec3(1, 0, 0), Vec2(0, 1));
    builder.addVertex(Vec3(-half,  half,  half), Vec3(1, 0, 0), Vec2(1, 1));
    builder.addVertex(Vec3(-half, -half,  half), Vec3(1, 0, 0), Vec2(1, 0));

    for (uint32_t face = 0; face < 6; ++face) {
        uint32_t base = face * 4;
        builder.addQuad(base, base + 1, base + 2, base + 3);
    }

    MeshData mesh = builder.build();
    FlipNormalsAndWinding(mesh);
    return mesh;
}

MeshData MeshBuilder::createSphere(float radius, uint32_t segments, uint32_t rings) {
    MeshBuilder builder;

    float const R = 1.0f / (float)(rings - 1);
    float const S = 1.0f / (float)(segments - 1);

    for (uint32_t r = 0; r < rings; ++r) {
        for (uint32_t s = 0; s < segments; ++s) {
            float const y = std::sin(-math::HALF_PI + math::PI * r * R);
            float const x = std::cos(2.0f * math::PI * s * S) * std::sin(math::PI * r * R);
            float const z = std::sin(2.0f * math::PI * s * S) * std::sin(math::PI * r * R);

            Vec3 position(x * radius, y * radius, z * radius);
            Vec3 normal = -position.Normalized();
            Vec2 texCoord(s * S, r * R);

            builder.addVertex(position, normal, texCoord);
        }
    }

    for (uint32_t r = 0; r < rings - 1; ++r) {
        for (uint32_t s = 0; s < segments - 1; ++s) {
            uint32_t curRow = r * segments;
            uint32_t nextRow = (r + 1) * segments;

            builder.addQuad(
                curRow + s,
                curRow + (s + 1),
                nextRow + (s + 1),
                nextRow + s
            );
        }
    }

    MeshData mesh = builder.build();
    FlipNormalsAndWinding(mesh);
    return mesh;
}

MeshData MeshBuilder::createCylinder(float radius, float height, uint32_t segments) {
    MeshBuilder builder;

    float halfH = height * 0.5f;

    uint32_t topCenter = 0;
    uint32_t bottomCenter = 1;

    builder.addVertex(Vec3(0, halfH, 0), Vec3(0, -1, 0), Vec2(0.5f, 0.5f));
    builder.addVertex(Vec3(0, -halfH, 0), Vec3(0, 1, 0), Vec2(0.5f, 0.5f));

    for (uint32_t i = 0; i <= segments; ++i) {
        float angle = (float)i / (float)segments * 2.0f * math::PI;
        float x = std::cos(angle) * radius;
        float z = std::sin(angle) * radius;

        Vec3 normal(-x, 0, -z);
        normal.Normalize();

        builder.addVertex(Vec3(x, halfH, z), normal, Vec2((float)i / segments, 1));

        builder.addVertex(Vec3(x, -halfH, z), normal, Vec2((float)i / segments, 0));
    }

    uint32_t baseIndex = 2;

    for (uint32_t i = 0; i < segments; ++i) {
        uint32_t topCur = baseIndex + i * 2;
        uint32_t bottomCur = topCur + 1;
        uint32_t topNext = baseIndex + ((i + 1) % (segments + 1)) * 2;
        uint32_t bottomNext = topNext + 1;

        builder.addQuad(topCur, bottomCur, bottomNext, topNext);
    }

    for (uint32_t i = 0; i < segments; ++i) {
        uint32_t cur = baseIndex + i * 2;
        uint32_t next = baseIndex + ((i + 1) % (segments + 1)) * 2;
        builder.addTriangle(topCenter, cur, next);
    }

    for (uint32_t i = 0; i < segments; ++i) {
        uint32_t cur = baseIndex + i * 2 + 1;
        uint32_t next = baseIndex + ((i + 1) % (segments + 1)) * 2 + 1;
        builder.addTriangle(bottomCenter, next, cur);
    }

    MeshData mesh = builder.build();
    FlipNormalsAndWinding(mesh);
    return mesh;
}

MeshData MeshBuilder::createPlane(float width, float depth, uint32_t widthSegments, uint32_t depthSegments) {
    MeshBuilder builder;

    float halfW = width * 0.5f;
    float halfD = depth * 0.5f;

    float segW = width / widthSegments;
    float segD = depth / depthSegments;

    for (uint32_t z = 0; z <= depthSegments; ++z) {
        for (uint32_t x = 0; x <= widthSegments; ++x) {
            float px = -halfW + x * segW;
            float pz = -halfD + z * segD;

            Vec3 position(px, 0, pz);
            Vec3 normal(0, -1, 0);
            Vec2 texCoord((float)x / widthSegments, (float)z / depthSegments);

            builder.addVertex(position, normal, texCoord);
        }
    }

    for (uint32_t z = 0; z < depthSegments; ++z) {
        for (uint32_t x = 0; x < widthSegments; ++x) {
            uint32_t i0 = z * (widthSegments + 1) + x;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = (z + 1) * (widthSegments + 1) + x + 1;
            uint32_t i3 = i2 - 1;

            builder.addQuad(i0, i3, i2, i1);
        }
    }

    MeshData mesh = builder.build();
    FlipNormalsAndWinding(mesh);
    return mesh;
}

MeshData MeshBuilder::createCircle(float radius, uint32_t segments) {
    MeshBuilder builder;

    builder.addVertex(Vec3(0, 0, 0), Vec3(0, 0, 1), Vec2(0.5f, 0.5f));

    for (uint32_t i = 0; i <= segments; ++i) {
        float angle = (float)i / (float)segments * 2.0f * math::PI;
        float x = std::cos(angle) * radius;
        float y = std::sin(angle) * radius;

        Vec3 position(x, y, 0);
        Vec3 normal(0, 0, 1);
        Vec2 texCoord(0.5f + x / (2.0f * radius), 0.5f + y / (2.0f * radius));

        builder.addVertex(position, normal, texCoord);
    }

    for (uint32_t i = 1; i <= segments; ++i) {
        builder.addTriangle(0, i, i + 1);
    }

    return builder.build();
}

MeshData MeshBuilder::createCone(float radius, float height, uint32_t segments) {
    MeshBuilder builder;

    float halfH = height * 0.5f;

    uint32_t apexIndex = 0;
    builder.addVertex(Vec3(0, halfH, 0), Vec3(0, -1, 0), Vec2(0.5f, 1.0f));

    uint32_t bottomCenter = 1;
    builder.addVertex(Vec3(0, -halfH, 0), Vec3(0, 1, 0), Vec2(0.5f, 0.5f));

    for (uint32_t i = 0; i <= segments; ++i) {
        float angle = (float)i / (float)segments * 2.0f * math::PI;
        float x = std::cos(angle) * radius;
        float z = std::sin(angle) * radius;

        Vec3 toApex = Vec3(0, halfH, 0) - Vec3(x, -halfH, z);
        Vec3 tangent = Vec3(-z, 0, x);
        Vec3 normal = -tangent.Cross(toApex).Normalized();

        builder.addVertex(Vec3(x, -halfH, z), normal, Vec2((float)i / segments, 0));
    }

    uint32_t baseRingStart = 2;

    for (uint32_t i = 0; i < segments; ++i) {
        uint32_t cur = baseRingStart + i;
        uint32_t next = baseRingStart + i + 1;
        builder.addTriangle(apexIndex, cur, next);
    }

    for (uint32_t i = 0; i < segments; ++i) {
        uint32_t cur = baseRingStart + i;
        uint32_t next = baseRingStart + i + 1;
        builder.addTriangle(bottomCenter, next, cur);
    }

    MeshData mesh = builder.build();
    FlipNormalsAndWinding(mesh);
    return mesh;
}

MeshData MeshBuilder::createPyramid(float baseSize, float height) {
    MeshBuilder builder;

    float half = baseSize * 0.5f;
    float halfH = height * 0.5f;

    builder.addVertex(Vec3(0, halfH, 0), Vec3(0, -1, 0), Vec2(0.5f, 1.0f));

    builder.addVertex(Vec3(-half, -halfH, -half), Vec3(0, 1, 0), Vec2(0, 0));
    builder.addVertex(Vec3( half, -halfH, -half), Vec3(0, 1, 0), Vec2(1, 0));
    builder.addVertex(Vec3( half, -halfH,  half), Vec3(0, 1, 0), Vec2(1, 1));
    builder.addVertex(Vec3(-half, -halfH,  half), Vec3(0, 1, 0), Vec2(0, 1));

    Vec3 frontNormal = -Vec3(0, 0.5f, 0.5f).Normalized();
    builder.addVertex(Vec3(0, halfH, 0), frontNormal, Vec2(0.5f, 1.0f));
    builder.addVertex(Vec3(-half, -halfH, half), frontNormal, Vec2(0, 0));
    builder.addVertex(Vec3( half, -halfH, half), frontNormal, Vec2(1, 0));

    Vec3 rightNormal = -Vec3(0.5f, 0.5f, 0).Normalized();
    builder.addVertex(Vec3(0, halfH, 0), rightNormal, Vec2(0.5f, 1.0f));
    builder.addVertex(Vec3(half, -halfH,  half), rightNormal, Vec2(0, 0));
    builder.addVertex(Vec3(half, -halfH, -half), rightNormal, Vec2(1, 0));

    Vec3 backNormal = -Vec3(0, 0.5f, -0.5f).Normalized();
    builder.addVertex(Vec3(0, halfH, 0), backNormal, Vec2(0.5f, 1.0f));
    builder.addVertex(Vec3( half, -halfH, -half), backNormal, Vec2(0, 0));
    builder.addVertex(Vec3(-half, -halfH, -half), backNormal, Vec2(1, 0));

    Vec3 leftNormal = -Vec3(-0.5f, 0.5f, 0).Normalized();
    builder.addVertex(Vec3(0, halfH, 0), leftNormal, Vec2(0.5f, 1.0f));
    builder.addVertex(Vec3(-half, -halfH, -half), leftNormal, Vec2(0, 0));
    builder.addVertex(Vec3(-half, -halfH,  half), leftNormal, Vec2(1, 0));

    builder.addQuad(1, 4, 3, 2);

    builder.addTriangle(5, 7, 6);
    builder.addTriangle(8, 10, 9);
    builder.addTriangle(11, 13, 12);
    builder.addTriangle(14, 16, 15);

    MeshData mesh = builder.build();
    FlipNormalsAndWinding(mesh);
    return mesh;
}

MeshData MeshBuilder::createTorus(float majorRadius, float minorRadius, uint32_t majorSegments, uint32_t minorSegments) {
    MeshBuilder builder;

    for (uint32_t i = 0; i <= majorSegments; ++i) {
        float theta = (float)i / (float)majorSegments * 2.0f * math::PI;
        float cosTheta = std::cos(theta);
        float sinTheta = std::sin(theta);

        for (uint32_t j = 0; j <= minorSegments; ++j) {
            float phi = (float)j / (float)minorSegments * 2.0f * math::PI;
            float cosPhi = std::cos(phi);
            float sinPhi = std::sin(phi);

            float x = (majorRadius + minorRadius * cosPhi) * cosTheta;
            float y = minorRadius * sinPhi;
            float z = (majorRadius + minorRadius * cosPhi) * sinTheta;

            Vec3 position(x, y, z);

            Vec3 center(majorRadius * cosTheta, 0, majorRadius * sinTheta);
            Vec3 normal = (position - center).Normalized();

            Vec2 texCoord((float)i / majorSegments, (float)j / minorSegments);

            builder.addVertex(position, normal, texCoord);
        }
    }

    for (uint32_t i = 0; i < majorSegments; ++i) {
        for (uint32_t j = 0; j < minorSegments; ++j) {
            uint32_t stride = minorSegments + 1;
            uint32_t i0 = i * stride + j;
            uint32_t i1 = (i + 1) * stride + j;
            uint32_t i2 = (i + 1) * stride + (j + 1);
            uint32_t i3 = i * stride + (j + 1);

            builder.addQuad(i0, i3, i2, i1);
        }
    }

    return builder.build();
}

MeshData MeshBuilder::createCapsule(float radius, float height, uint32_t segments, uint32_t rings) {
    MeshBuilder builder;

    float cylinderHeight = height - 2.0f * radius;
    float halfCylinderH = cylinderHeight * 0.5f;

    for (uint32_t r = 0; r <= rings / 2; ++r) {
        for (uint32_t s = 0; s < segments; ++s) {
            float theta = (float)r / (float)(rings / 2) * math::HALF_PI;
            float phi = (float)s / (float)segments * 2.0f * math::PI;

            float sinTheta = std::sin(theta);
            float cosTheta = std::cos(theta);
            float sinPhi = std::sin(phi);
            float cosPhi = std::cos(phi);

            float x = radius * sinTheta * cosPhi;
            float y = halfCylinderH + radius * cosTheta;
            float z = radius * sinTheta * sinPhi;

            Vec3 position(x, y, z);
            Vec3 normal = -Vec3(x, y - halfCylinderH, z).Normalized();
            Vec2 texCoord((float)s / segments, (float)r / (rings / 2) * 0.25f + 0.75f);

            builder.addVertex(position, normal, texCoord);
        }
    }

    uint32_t topHemisphereVertexCount = (rings / 2 + 1) * segments;

    for (uint32_t i = 0; i <= 1; ++i) {
        float y = i == 0 ? halfCylinderH : -halfCylinderH;
        for (uint32_t s = 0; s < segments; ++s) {
            float angle = (float)s / (float)segments * 2.0f * math::PI;
            float x = std::cos(angle) * radius;
            float z = std::sin(angle) * radius;

            Vec3 position(x, y, z);
            Vec3 normal(-x, 0, -z);
            normal.Normalize();
            Vec2 texCoord((float)s / segments, 0.5f + (float)i * 0.25f);

            builder.addVertex(position, normal, texCoord);
        }
    }

    uint32_t cylinderVertexStart = topHemisphereVertexCount;

    for (uint32_t r = 0; r <= rings / 2; ++r) {
        for (uint32_t s = 0; s < segments; ++s) {
            float theta = math::HALF_PI + (float)r / (float)(rings / 2) * math::HALF_PI;
            float phi = (float)s / (float)segments * 2.0f * math::PI;

            float sinTheta = std::sin(theta);
            float cosTheta = std::cos(theta);
            float sinPhi = std::sin(phi);
            float cosPhi = std::cos(phi);

            float x = radius * sinTheta * cosPhi;
            float y = -halfCylinderH + radius * cosTheta;
            float z = radius * sinTheta * sinPhi;

            Vec3 position(x, y, z);
            Vec3 normal = -Vec3(x, y + halfCylinderH, z).Normalized();
            Vec2 texCoord((float)s / segments, (float)r / (rings / 2) * 0.25f);

            builder.addVertex(position, normal, texCoord);
        }
    }

    for (uint32_t r = 0; r < rings / 2; ++r) {
        for (uint32_t s = 0; s < segments; ++s) {
            uint32_t cur = r * segments + s;
            uint32_t next = r * segments + (s + 1) % segments;
            uint32_t curNext = (r + 1) * segments + s;
            uint32_t nextNext = (r + 1) * segments + (s + 1) % segments;

            builder.addQuad(cur, curNext, nextNext, next);
        }
    }

    for (uint32_t s = 0; s < segments; ++s) {
        uint32_t cur = cylinderVertexStart + s;
        uint32_t next = cylinderVertexStart + (s + 1) % segments;
        uint32_t curNext = cylinderVertexStart + segments + s;
        uint32_t nextNext = cylinderVertexStart + segments + (s + 1) % segments;

        builder.addQuad(cur, curNext, nextNext, next);
    }

    uint32_t bottomHemisphereStart = cylinderVertexStart + 2 * segments;
    for (uint32_t r = 0; r < rings / 2; ++r) {
        for (uint32_t s = 0; s < segments; ++s) {
            uint32_t cur = bottomHemisphereStart + r * segments + s;
            uint32_t next = bottomHemisphereStart + r * segments + (s + 1) % segments;
            uint32_t curNext = bottomHemisphereStart + (r + 1) * segments + s;
            uint32_t nextNext = bottomHemisphereStart + (r + 1) * segments + (s + 1) % segments;

            builder.addQuad(cur, curNext, nextNext, next);
        }
    }

    MeshData mesh = builder.build();
    FlipNormalsAndWinding(mesh);
    return mesh;
}

}
