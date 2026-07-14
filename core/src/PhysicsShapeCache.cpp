#include "lupine/physics3d/PhysicsShapeCache.hpp"
#include "lupine/asset/ModelAsset.hpp"
#include "lupine/logger/Logger.hpp"
#include <btBulletDynamicsCommon.h>
#include <BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>

namespace lupine {
namespace physics3d {

CachedMeshShape::~CachedMeshShape() {
    if (bvhShape) {
        delete bvhShape;
        bvhShape = nullptr;
    }
    if (triangleMesh) {
        delete triangleMesh;
        triangleMesh = nullptr;
    }
}

PhysicsShapeCache& PhysicsShapeCache::Instance() {
    static PhysicsShapeCache instance;
    return instance;
}

PhysicsShapeCache::~PhysicsShapeCache() {
    Clear();
}

btBvhTriangleMeshShape* PhysicsShapeCache::GetOrCreateShape(const std::string& meshPath) {
    if (meshPath.empty()) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(m_Mutex);

    // Check if already cached
    auto it = m_Cache.find(meshPath);
    if (it != m_Cache.end() && it->second->bvhShape) {
        return it->second->bvhShape;
    }

    // Not cached - create new entry or load data into existing entry
    CachedMeshShape* cached = nullptr;
    if (it != m_Cache.end()) {
        cached = it->second.get();
    } else {
        auto newEntry = std::make_unique<CachedMeshShape>();
        cached = newEntry.get();
        m_Cache[meshPath] = std::move(newEntry);
    }

    // Load mesh data if not already loaded
    if (cached->vertices.empty()) {
        if (!LoadMeshFromFile(meshPath, *cached)) {
            
            return nullptr;
        }
    }

    // Build the BVH shape
    btBvhTriangleMeshShape* shape = BuildBvhShape(*cached);

    return shape;
}

bool PhysicsShapeCache::GetMeshData(const std::string& meshPath,
                                    std::vector<math::Vec3>& outVertices,
                                    std::vector<uint32_t>& outIndices) {
    if (meshPath.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_Mutex);

    // Check if already cached
    auto it = m_Cache.find(meshPath);
    if (it != m_Cache.end() && !it->second->vertices.empty()) {
        outVertices = it->second->vertices;
        outIndices = it->second->indices;
        return true;
    }

    // Not cached - load it
    CachedMeshShape* cached = nullptr;
    if (it != m_Cache.end()) {
        cached = it->second.get();
    } else {
        auto newEntry = std::make_unique<CachedMeshShape>();
        cached = newEntry.get();
        m_Cache[meshPath] = std::move(newEntry);
    }

    if (!LoadMeshFromFile(meshPath, *cached)) {
        return false;
    }

    outVertices = cached->vertices;
    outIndices = cached->indices;
    return true;
}

void PhysicsShapeCache::AddRef(const std::string& meshPath) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    auto it = m_Cache.find(meshPath);
    if (it != m_Cache.end()) {
        it->second->refCount++;
    }
}

void PhysicsShapeCache::Release(const std::string& meshPath) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    auto it = m_Cache.find(meshPath);
    if (it != m_Cache.end() && it->second->refCount > 0) {
        it->second->refCount--;
        // Don't auto-delete - keep in cache for potential reuse
        // The cache will be cleared on scene change or explicit Clear() call
    }
}

void PhysicsShapeCache::Clear() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Cache.clear();
}

size_t PhysicsShapeCache::GetCacheSize() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_Cache.size();
}

void PhysicsShapeCache::Preload(const std::string& meshPath) {
    // Just call GetOrCreateShape - it handles caching
    GetOrCreateShape(meshPath);
}

bool PhysicsShapeCache::LoadMeshFromFile(const std::string& meshPath, CachedMeshShape& outData) {
    asset::AssetRef<asset::ModelAsset> modelAsset(new asset::ModelAsset());
    bool loaded = modelAsset->LoadFromFile(meshPath, true);

    if (!loaded || !modelAsset.IsValid()) {
        return false;
    }

    outData.vertices.clear();
    outData.indices.clear();

    const auto& meshes = modelAsset->GetMeshes();
    for (const auto& mesh : meshes) {
        uint32_t vertexOffset = static_cast<uint32_t>(outData.vertices.size());

        for (const auto& vertex : mesh.vertices) {
            outData.vertices.push_back(vertex.position);
        }

        for (const auto& index : mesh.indices) {
            outData.indices.push_back(vertexOffset + index);
        }
    }

    return !outData.vertices.empty();
}

btBvhTriangleMeshShape* PhysicsShapeCache::BuildBvhShape(CachedMeshShape& data) {
    if (data.vertices.empty()) {
        return nullptr;
    }

    // Clean up any previous shape/mesh
    if (data.bvhShape) {
        delete data.bvhShape;
        data.bvhShape = nullptr;
    }
    if (data.triangleMesh) {
        delete data.triangleMesh;
        data.triangleMesh = nullptr;
    }

    // Build the btTriangleMesh
    data.triangleMesh = new btTriangleMesh();

    if (!data.indices.empty()) {
        // Indexed triangles
        for (size_t i = 0; i + 2 < data.indices.size(); i += 3) {
            uint32_t i0 = data.indices[i];
            uint32_t i1 = data.indices[i + 1];
            uint32_t i2 = data.indices[i + 2];

            if (i0 < data.vertices.size() && i1 < data.vertices.size() && i2 < data.vertices.size()) {
                btVector3 v0(data.vertices[i0].x, data.vertices[i0].y, data.vertices[i0].z);
                btVector3 v1(data.vertices[i1].x, data.vertices[i1].y, data.vertices[i1].z);
                btVector3 v2(data.vertices[i2].x, data.vertices[i2].y, data.vertices[i2].z);
                data.triangleMesh->addTriangle(v0, v1, v2);
            }
        }
    } else {
        // Non-indexed - vertices in groups of 3
        for (size_t i = 0; i + 2 < data.vertices.size(); i += 3) {
            btVector3 v0(data.vertices[i].x, data.vertices[i].y, data.vertices[i].z);
            btVector3 v1(data.vertices[i + 1].x, data.vertices[i + 1].y, data.vertices[i + 1].z);
            btVector3 v2(data.vertices[i + 2].x, data.vertices[i + 2].y, data.vertices[i + 2].z);
            data.triangleMesh->addTriangle(v0, v1, v2);
        }
    }

    // Build the BVH - this is the expensive operation that we're caching
    data.bvhShape = new btBvhTriangleMeshShape(data.triangleMesh, true);

    return data.bvhShape;
}

} // namespace physics3d
} // namespace lupine
