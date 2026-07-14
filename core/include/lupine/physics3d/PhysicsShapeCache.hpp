#pragma once

#include "lupine/math/Vec3.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

// Forward declare Bullet types
class btBvhTriangleMeshShape;
class btTriangleMesh;

namespace lupine {
namespace physics3d {

/**
 * Cached mesh data for physics collision shapes.
 * Stores both the raw vertex/index data and the built BVH shape.
 */
struct CachedMeshShape {
    std::vector<math::Vec3> vertices;
    std::vector<uint32_t> indices;
    btTriangleMesh* triangleMesh = nullptr;
    btBvhTriangleMeshShape* bvhShape = nullptr;
    size_t refCount = 0;

    ~CachedMeshShape();
};

/**
 * Global cache for physics collision shapes.
 *
 * This cache significantly improves scene loading times when multiple objects
 * use the same mesh for collision. The expensive BVH (Bounding Volume Hierarchy)
 * is built only once per unique mesh, and shared across all instances.
 *
 * For scaled instances, use btScaledBvhTriangleMeshShape which wraps the cached
 * BVH shape without rebuilding.
 */
class PhysicsShapeCache {
public:
    static PhysicsShapeCache& Instance();

    /**
     * Get or create a cached BVH shape for a mesh path.
     * If the mesh is already cached, returns the existing shape.
     * If not cached, loads the mesh and builds the BVH, then caches it.
     *
     * @param meshPath Path to the mesh file
     * @return Pointer to the cached BVH shape, or nullptr if loading failed
     */
    btBvhTriangleMeshShape* GetOrCreateShape(const std::string& meshPath);

    /**
     * Get cached vertex data for a mesh, loading if necessary.
     *
     * @param meshPath Path to the mesh file
     * @param outVertices Output vector for vertices
     * @param outIndices Output vector for indices
     * @return true if data was retrieved successfully
     */
    bool GetMeshData(const std::string& meshPath,
                     std::vector<math::Vec3>& outVertices,
                     std::vector<uint32_t>& outIndices);

    /**
     * Register that a shape is being used.
     * Call this when creating a collider that uses a cached shape.
     */
    void AddRef(const std::string& meshPath);

    /**
     * Release a reference to a shape.
     * Call this when destroying a collider that used a cached shape.
     */
    void Release(const std::string& meshPath);

    /**
     * Clear all cached shapes.
     * This should be called when changing scenes or shutting down.
     */
    void Clear();

    /**
     * Get the number of cached shapes.
     */
    size_t GetCacheSize() const;

    /**
     * Preload a mesh into the cache.
     * Useful for preloading common meshes during loading screens.
     */
    void Preload(const std::string& meshPath);

private:
    PhysicsShapeCache() = default;
    ~PhysicsShapeCache();

    PhysicsShapeCache(const PhysicsShapeCache&) = delete;
    PhysicsShapeCache& operator=(const PhysicsShapeCache&) = delete;

    bool LoadMeshFromFile(const std::string& meshPath, CachedMeshShape& outData);
    btBvhTriangleMeshShape* BuildBvhShape(CachedMeshShape& data);

    std::unordered_map<std::string, std::unique_ptr<CachedMeshShape>> m_Cache;
    mutable std::mutex m_Mutex;
};

} // namespace physics3d
} // namespace lupine
