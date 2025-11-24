#pragma once

#include "lupine/math/Vec3.hpp"
#include "lupine/math/Color.hpp"
#include "lupine/rendering/Rendering.hpp"
#include <memory>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace lupine {
namespace engine {

/**
 * A single voxel in the builder
 */
struct Voxel {
    int32_t x, y, z;
    math::Color color;

    Voxel() : x(0), y(0), z(0), color(math::Color::White()) {}
    Voxel(int32_t x, int32_t y, int32_t z, const math::Color& color = math::Color::White())
        : x(x), y(y), z(z), color(color) {}

    bool operator==(const Voxel& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

/**
 * Hash function for voxel positions
 */
struct VoxelPosHash {
    std::size_t operator()(const std::tuple<int32_t, int32_t, int32_t>& pos) const {
        auto h1 = std::hash<int32_t>{}(std::get<0>(pos));
        auto h2 = std::hash<int32_t>{}(std::get<1>(pos));
        auto h3 = std::hash<int32_t>{}(std::get<2>(pos));
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

/**
 * VoxelBuilder - A standalone voxel modeling system for the editor
 *
 * Manages its own rendering without using scenes/nodes.
 * Renders a combined mesh of all voxels efficiently.
 */
class VoxelBuilder {
public:
    VoxelBuilder();
    ~VoxelBuilder();

    // ========================================================================
    // Voxel Operations
    // ========================================================================

    /**
     * Place a voxel at the given position
     */
    void PlaceVoxel(int32_t x, int32_t y, int32_t z, const math::Color& color);

    /**
     * Erase a voxel at the given position
     */
    void EraseVoxel(int32_t x, int32_t y, int32_t z);

    /**
     * Check if a voxel exists at the given position
     */
    bool HasVoxel(int32_t x, int32_t y, int32_t z) const;

    /**
     * Get voxel at position (returns nullptr if not found)
     */
    const Voxel* GetVoxel(int32_t x, int32_t y, int32_t z) const;

    /**
     * Clear all voxels
     */
    void Clear();

    /**
     * Get all voxels
     */
    const std::unordered_map<std::tuple<int32_t, int32_t, int32_t>, Voxel, VoxelPosHash>& GetVoxels() const {
        return m_Voxels;
    }

    /**
     * Get voxel count
     */
    size_t GetVoxelCount() const { return m_Voxels.size(); }

    // ========================================================================
    // Rendering
    // ========================================================================

    /**
     * Initialize rendering resources
     * Must be called before Render()
     */
    bool InitializeRendering(RenderWorld* renderWorld);

    /**
     * Shutdown rendering resources
     */
    void ShutdownRendering();

    /**
     * Get the combined mesh handle for rendering
     * Regenerates mesh if voxels have changed
     * @return MeshHandle for the combined voxel mesh (may be invalid if no voxels)
     */
    MeshHandle GetMesh();

    /**
     * Force mesh regeneration on next GetMesh() call
     */
    void MarkMeshDirty() { m_MeshDirty = true; }

    /**
     * Get the transform matrix for rendering (identity since voxels have absolute positions)
     */
    math::Mat4 GetTransform() const { return math::Mat4::Identity(); }

    /**
     * Get the PBR material handle for rendering voxels
     * @return MaterialHandle for the voxel PBR material (may be invalid if not initialized)
     */
    MaterialHandle GetMaterial() const { return m_Material; }

    // ========================================================================
    // Serialization
    // ========================================================================

    /**
     * Serialize to JSON string
     */
    std::string ToJSON() const;

    /**
     * Deserialize from JSON string
     */
    bool FromJSON(const std::string& json);

    /**
     * Export to OBJ format
     * @param mergeFaces If true, merge internal faces (only export visible external faces)
     * @param textureAtlas If true, use texture atlas for colors; if false, use vertex colors
     */
    std::string ExportToOBJ(bool mergeFaces = false, bool textureAtlas = false) const;

    /**
     * Export to glTF format
     * @param mergeFaces If true, merge internal faces (only export visible external faces)
     * @param textureAtlas If true, use texture atlas for colors; if false, use vertex colors
     */
    std::string ExportToGLTF(bool mergeFaces = false, bool textureAtlas = false) const;

private:
    // Voxel storage
    std::unordered_map<std::tuple<int32_t, int32_t, int32_t>, Voxel, VoxelPosHash> m_Voxels;

    // Rendering state
    RenderWorld* m_RenderWorld;
    MeshHandle m_CombinedMesh;
    MaterialHandle m_Material;
    bool m_MeshDirty;
    bool m_RenderingInitialized;

    // Helper methods
    void RegenerateMesh();
    void GenerateCubeMesh(const Voxel& voxel, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices) const;
};

} // namespace engine
} // namespace lupine
