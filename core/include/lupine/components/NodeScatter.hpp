#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/components/CollisionMesh3DComponent.hpp"
#include <nlohmann/json.hpp>
#include <random>
#include <vector>
#include <memory>

namespace lupine {

// Forward declarations
namespace core {
    class Node;
    class Node3D;
    class Scene;
}

namespace components {

using namespace math;

/**
 * Scatter mode for NodeScatter
 */
enum class NodeScatterMode {
    Simple = 0,     // Simple area-based scattering (like ScatterMultiMesh)
    Collision = 1   // Collision-based scattering (like CollisionScatterMultiMesh)
};

/**
 * Collision optimization mode for scattered instances
 */
enum class ScatterCollisionMode {
    Full = 0,           // Use original collision (can be slow with trimesh)
    SimplifiedBox = 1,  // Replace collision with bounding box
    SimplifiedSphere = 2, // Replace collision with bounding sphere
    None = 3            // Disable collision on scattered instances
};

/**
 * Distribution mode for node scatter placement
 */
enum class NodeScatterDistribution {
    Random = 0,     // Random placement within volume
    Grid = 1        // Grid-based placement
};

/**
 * NodeScatter Component
 *
 * Scatters instances of a node tree (template) across an area or on collision surfaces.
 * Unlike MultiMesh-based scattering, this creates actual node instances that can have
 * scripts, collision, audio, and other components.
 *
 * Usage:
 * 1. Add NodeScatter component to a Node3D
 * 2. Add a child Node3D as the template (first Node3D child is used)
 * 3. Configure scatter settings
 * 4. The template and its children will be cloned and scattered
 *
 * Features:
 * - Simple (area) or Collision-based scattering
 * - Random or Grid distribution
 * - Scale/rotation variation
 * - Collision layer filtering (in Collision mode)
 * - Slope filtering (in Collision mode)
 * - Align to surface normals
 * - Random seed for reproducibility
 * - Debug visualization
 */
class NodeScatter : public core::Component {
public:
    NodeScatter();
    explicit NodeScatter(const std::string& name);
    virtual ~NodeScatter();

    // ISerializable interface
    std::string GetTypeName() const override { return "NodeScatter"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnRender() override;
    void OnUpdate(float deltaTime) override;

    // Property change notification
    void OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) override;

    // Scatter mode
    NodeScatterMode GetScatterMode() const;
    void SetScatterMode(NodeScatterMode mode);

    // Collision layer settings (for Collision mode)
    uint32_t GetScatterLayers() const;
    void SetScatterLayers(uint32_t layers);
    uint32_t GetCutoutLayers() const;
    void SetCutoutLayers(uint32_t layers);

    // Distribution settings
    NodeScatterDistribution GetDistributionMode() const;
    void SetDistributionMode(NodeScatterDistribution mode);
    int GetInstanceCount() const;
    void SetInstanceCount(int count);

    // Grid settings
    float GetGridSpacingX() const;
    void SetGridSpacingX(float spacing);
    float GetGridSpacingZ() const;
    void SetGridSpacingZ(float spacing);
    float GetGridJitter() const;
    void SetGridJitter(float jitter);

    // Area settings
    Vec3 GetScatterAreaSize() const;
    void SetScatterAreaSize(const Vec3& size);
    Vec3 GetScatterAreaOffset() const;
    void SetScatterAreaOffset(const Vec3& offset);

    // Surface settings (for Collision mode)
    bool GetAlignToNormal() const;
    void SetAlignToNormal(bool align);
    float GetNormalAlignmentStrength() const;
    void SetNormalAlignmentStrength(float strength);
    float GetSurfaceOffset() const;
    void SetSurfaceOffset(float offset);

    // Slope filtering (for Collision mode)
    float GetMinSlope() const;
    void SetMinSlope(float slope);
    float GetMaxSlope() const;
    void SetMaxSlope(float slope);

    // Scale variation
    Vec2 GetScaleRange() const;
    void SetScaleRange(const Vec2& range);
    bool GetUniformScale() const;
    void SetUniformScale(bool uniform);

    // Rotation variation
    Vec3 GetRotationVariation() const;
    void SetRotationVariation(const Vec3& variation);
    bool GetRandomYRotation() const;
    void SetRandomYRotation(bool random);

    // Randomization
    int GetRandomSeed() const;
    void SetRandomSeed(int seed);
    bool GetUseRandomSeed() const;
    void SetUseRandomSeed(bool use);

    // Debug visualization
    bool GetShowDebugShape() const;
    void SetShowDebugShape(bool show);
    Color GetDebugColor() const;
    void SetDebugColor(const Color& color);

    // Performance / Optimization
    int GetInstancesPerFrame() const;
    void SetInstancesPerFrame(int count);
    ScatterCollisionMode GetCollisionOptimization() const;
    void SetCollisionOptimization(ScatterCollisionMode mode);
    float GetCollisionDistance() const;
    void SetCollisionDistance(float distance);
    bool GetProgressiveLoading() const;
    void SetProgressiveLoading(bool enabled);

    // Regeneration
    void RegenerateScatter();
    bool NeedsRegeneration() const { return m_NeedsRegeneration; }

    // Get scattered instances
    const std::vector<std::shared_ptr<core::Node3D>>& GetScatteredInstances() const { return m_ScatteredInstances; }

    // ===== Collision shape info (public for helper function access) =====
    struct CollisionShapeInfo {
        CollisionMesh3DComponent* component;
        core::Node* node;
        Mat4 worldTransform;
        uint32_t layers;
    };

private:
    // Scatter point data (defined early so it can be used below)
    struct ScatterPoint {
        Vec3 position;      // World space position
        Vec3 normal;        // World space normal (for collision mode)
        bool valid;
    };

    // Scatter state
    bool m_NeedsRegeneration;
    bool m_IsInitialized;
    std::mt19937 m_RandomGenerator;

    // Progressive loading state
    bool m_IsProgressivelyLoading;
    int m_ProgressiveLoadIndex;
    std::vector<ScatterPoint> m_PendingScatterPoints;

    // Template node (first Node3D child)
    std::weak_ptr<core::Node3D> m_TemplateNode;

    // Scattered node instances
    std::vector<std::shared_ptr<core::Node3D>> m_ScatteredInstances;

    // Tracking for change detection
    Vec3 m_LastNodePosition;
    Quat m_LastNodeRotation;
    Vec3 m_LastNodeScale;
    bool m_TrackingInitialized;

    // Cached scatter points
    std::vector<ScatterPoint> m_CachedScatterPoints;

    // ===== Collision shape raycasting (for Collision mode) =====
    std::vector<CollisionShapeInfo> m_CachedCollisionShapes;

    /**
     * Find the template node (first Node3D child)
     */
    std::shared_ptr<core::Node3D> FindTemplateNode();

    /**
     * Generate scatter points based on current settings
     */
    void GenerateScatterPoints();

    /**
     * Generate simple (area-based) scatter points
     */
    void GenerateSimpleRandomPoints(std::vector<ScatterPoint>& points);
    void GenerateSimpleGridPoints(std::vector<ScatterPoint>& points);

    /**
     * Generate collision-based scatter points
     */
    void GenerateCollisionRandomPoints(std::vector<ScatterPoint>& points);
    void GenerateCollisionGridPoints(std::vector<ScatterPoint>& points);

    /**
     * Find all collision shapes in the scene
     */
    void FindCollisionShapes();

    /**
     * Raycast against cached collision shapes
     */
    bool RaycastAgainstShapes(const Vec3& origin, const Vec3& direction, float maxDist,
                               Vec3& outPoint, Vec3& outNormal, uint32_t& outLayers);

    /**
     * Ray-shape intersection tests
     */
    float RayBoxIntersect(const Vec3& rayOrigin, const Vec3& rayDir,
                          const Vec3& boxCenter, const Vec3& boxSize,
                          const Mat4& boxTransform, Vec3& outNormal);
    float RaySphereIntersect(const Vec3& rayOrigin, const Vec3& rayDir,
                             const Vec3& sphereCenter, float radius, Vec3& outNormal);

    /**
     * Check if surface slope is within acceptable range
     */
    bool IsSlopeAcceptable(const Vec3& normal) const;

    /**
     * Create scattered node instances from points
     */
    void CreateScatteredInstances();

    /**
     * Create a batch of instances for progressive loading
     * Returns true if there are more to create
     */
    bool CreateInstanceBatch(int batchSize);

    /**
     * Clone a node and all its children
     */
    std::shared_ptr<core::Node3D> CloneNodeTree(std::shared_ptr<core::Node3D> source);

    /**
     * Optimize collision on a cloned instance
     * Replaces trimesh with simplified shapes or removes collision
     */
    void OptimizeInstanceCollision(std::shared_ptr<core::Node3D> instance);

    /**
     * Clear all scattered instances
     */
    void ClearScatteredInstances();

    /**
     * Generate random scale
     */
    Vec3 GenerateRandomScale();

    /**
     * Generate random rotation
     */
    Vec3 GenerateRandomRotation(const Vec3& surfaceNormal);

    /**
     * Draw debug visualization
     */
    void DrawDebugShape();

    /**
     * Check if scatter parameters have changed
     */
    void CheckForChanges();
};

} // namespace components
} // namespace lupine
