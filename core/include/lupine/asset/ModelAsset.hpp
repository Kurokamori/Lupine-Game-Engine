#pragma once

#include "Asset.hpp"
#include "lupine/math/Math.hpp"
#include <vector>
#include <string>
#include <map>

namespace lupine {
namespace asset {

/**
 * Vertex structure for mesh data
 */
struct Vertex {
    math::Vec3 position;
    math::Vec3 normal;
    math::Vec2 texCoord;
    math::Vec4 color{1.0f, 1.0f, 1.0f, 1.0f};  // Vertex color (default white)
    math::Vec3 tangent;
    math::Vec3 bitangent;

    int boneIDs[4] = {0, 0, 0, 0};
    float boneWeights[4] = {0.0f, 0.0f, 0.0f, 0.0f};
};

/**
 * Embedded texture data
 */
struct EmbeddedTexture {
    std::string name;
    uint32_t width{0};
    uint32_t height{0};
    std::vector<uint8_t> data;
    bool compressed{false}; // If true, data is compressed (e.g., PNG/JPG), otherwise raw RGBA
};

/**
 * Material properties
 */
struct Material {
    std::string name;

    math::Vec3 albedo{1.0f, 1.0f, 1.0f};
    float metallic{0.0f};
    float roughness{0.5f};
    float ao{1.0f};

    std::string albedoMap;
    std::string normalMap;
    std::string metallicMap;
    std::string roughnessMap;
    std::string aoMap;
    std::string emissiveMap;

    math::Vec3 emissive{0.0f, 0.0f, 0.0f};
    float opacity{1.0f};
};

/**
 * Mesh data
 */
struct Mesh {
    std::string name;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    uint32_t materialIndex{0};
    
    // Bounding box
    math::Vec3 boundsMin;
    math::Vec3 boundsMax;
};

/**
 * Bone structure for skeletal animation
 */
struct Bone {
    std::string name;
    int parentIndex{-1};
    math::Mat4 offsetMatrix;
    math::Mat4 localTransform;
};

/**
 * Animation keyframe types
 */
struct PositionKey {
    float time;
    math::Vec3 value;
};

struct RotationKey {
    float time;
    math::Quat value;
};

struct ScaleKey {
    float time;
    math::Vec3 value;
};

/**
 * Animation channel (per bone)
 */
struct AnimationChannel {
    std::string boneName;
    std::vector<PositionKey> positionKeys;
    std::vector<RotationKey> rotationKeys;
    std::vector<ScaleKey> scaleKeys;
};

/**
 * Animation clip
 */
struct AnimationClip {
    std::string name;
    float duration{0.0f};
    float ticksPerSecond{25.0f};
    std::vector<AnimationChannel> channels;
};

/**
 * Skeleton structure
 */
struct Skeleton {
    std::vector<Bone> bones;
    std::map<std::string, uint32_t> boneMap; // name -> index
    math::Mat4 globalInverseTransform;
};

/**
 * Model asset class
 * Supports GLTF, FBX, OBJ formats via Assimp
 */
class ModelAsset : public Asset {
public:
    ModelAsset();
    explicit ModelAsset(const core::UUID& uuid);
    virtual ~ModelAsset();

    AssetType GetType() const override { return AssetType::Model; }

    // Load from file
    bool LoadFromFile(const std::string& filepath, bool optimizeMesh = true);
    
    // Mesh access
    const std::vector<Mesh>& GetMeshes() const { return m_Meshes; }
    uint32_t GetMeshCount() const { return static_cast<uint32_t>(m_Meshes.size()); }
    
    // Material access
    const std::vector<Material>& GetMaterials() const { return m_Materials; }
    uint32_t GetMaterialCount() const { return static_cast<uint32_t>(m_Materials.size()); }
    
    // Skeleton access
    bool HasSkeleton() const { return m_HasSkeleton; }
    const Skeleton& GetSkeleton() const { return m_Skeleton; }
    
    // Animation access
    bool HasAnimations() const { return !m_Animations.empty(); }
    const std::vector<AnimationClip>& GetAnimations() const { return m_Animations; }
    uint32_t GetAnimationCount() const { return static_cast<uint32_t>(m_Animations.size()); }

    // Embedded texture access
    bool HasEmbeddedTextures() const { return !m_EmbeddedTextures.empty(); }
    const std::vector<EmbeddedTexture>& GetEmbeddedTextures() const { return m_EmbeddedTextures; }
    const EmbeddedTexture* GetEmbeddedTexture(const std::string& name) const;

private:
    std::vector<Mesh> m_Meshes;
    std::vector<Material> m_Materials;
    std::vector<EmbeddedTexture> m_EmbeddedTextures;
    Skeleton m_Skeleton;
    std::vector<AnimationClip> m_Animations;
    bool m_HasSkeleton{false};

    void RefreshTrackedBytes();   // sum mesh vertex/index data and report to the profiler

    // Helper methods using void* to avoid exposing Assimp types in the public API
    void ProcessNode(void* node, const void* scene);
    void ProcessMesh(void* aiMesh, const void* scene);
    void ProcessBones(void* aiMesh, Mesh& mesh);
    void ProcessSkeleton(const void* scene);
    void ProcessAnimations(const void* scene);
    void ProcessEmbeddedTextures(const void* scene);
    void BuildBoneHierarchy(void* node, int parentIndex);
};

} // namespace asset
} // namespace lupine

