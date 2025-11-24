#include "lupine/rendering/Mesh.hpp"
#include <limits>

namespace lupine {

void MeshData::calculateBounds() {
    if (vertices.empty()) {
        bounds = AABB();
        return;
    }

    Vec3 min(std::numeric_limits<float>::max());
    Vec3 max(std::numeric_limits<float>::lowest());

    for (const Vertex& v : vertices) {
        min.x = std::min(min.x, v.position.x);
        min.y = std::min(min.y, v.position.y);
        min.z = std::min(min.z, v.position.z);

        max.x = std::max(max.x, v.position.x);
        max.y = std::max(max.y, v.position.y);
        max.z = std::max(max.z, v.position.z);
    }

    bounds = AABB(min, max);
}

void MeshData::addSubmesh(uint32_t indexCount, uint32_t vertexCount, MaterialHandle material) {
    Submesh submesh;

    if (!submeshes.empty()) {
        const Submesh& lastSubmesh = submeshes.back();
        submesh.indexOffset = lastSubmesh.indexOffset + lastSubmesh.indexCount;
        submesh.vertexOffset = lastSubmesh.vertexOffset + lastSubmesh.vertexCount;
    } else {
        submesh.indexOffset = 0;
        submesh.vertexOffset = 0;
    }

    submesh.indexCount = indexCount;
    submesh.vertexCount = vertexCount;
    submesh.material = material;

    submesh.bounds = bounds;

    submeshes.push_back(submesh);
}

}
