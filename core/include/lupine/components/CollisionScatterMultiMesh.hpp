#pragma once

#include "lupine/components/MultiMeshGeneric.hpp"
#include "lupine/components/CollisionMesh3DComponent.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/physics3d/Physics3DWorld.hpp"
#include <nlohmann/json.hpp>
#include <random>
#include <vector>

namespace lupine {

// Forward declarations
namespace core {
    class Node;
}

namespace components {

using namespace math;

/**
 * Distribution mode for scatter placement
 */
enum class ScatterDistributionMode {
    Random = 0,     // Random placement within volume
    Grid = 1,       // Grid-based placement
    Outline = 2     // Scatter only on edges/outline
};

/**
 * Scatter placement mode
 */
enum class ScatterPlacementMode {
    Surface = 0,    // Scatter on collision surface with normal alignment
    Volume = 1      // Scatter within collision volume
};

/**
 * CollisionScatterMultiMesh Component
 *
 * Scatters instances of a mesh based on collision shapes and layers.
 *
 * Features:
 * - Scatter on specific collision layers
 * - Exclusion/cutout collision layers
 * - Surface or volume scattering
 * - Align to surface normals
 * - Grid, random, or outline distribution
 * - Scale/rotation/color variation
 * - Slope filtering for surfaces
 * - Density control
 * - Random seed for reproducibility
 * - Debug visualization
 */
class CollisionScatterMultiMesh : public MultiMeshGeneric {
public:
    CollisionScatterMultiMesh();
    explicit CollisionScatterMultiMesh(const std::string& name);
    virtual ~CollisionScatterMultiMesh();

    // ISerializable interface
    std::string GetTypeName() const override { return "CollisionScatterMultiMesh"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnRender() override;

    // Property change notification
    void OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) override;

    // IRenderableComponent override
    void buildDrawCommands(RenderContext& ctx) override;

    // Collision layer settings
    uint32_t GetScatterLayers() const;
    void SetScatterLayers(uint32_t layers);
    uint32_t GetCutoutLayers() const;
    void SetCutoutLayers(uint32_t layers);

    // Distribution settings
    ScatterDistributionMode GetDistributionMode() const;
    void SetDistributionMode(ScatterDistributionMode mode);
    ScatterPlacementMode GetPlacementMode() const;
    void SetPlacementMode(ScatterPlacementMode mode);

    // Density and count
    float GetDensity() const;
    void SetDensity(float density);
    int GetMaxInstances() const;
    void SetMaxInstances(int max);

    // Grid settings
    float GetGridSpacingX() const;
    void SetGridSpacingX(float spacing);
    float GetGridSpacingZ() const;
    void SetGridSpacingZ(float spacing);
    float GetGridJitter() const;
    void SetGridJitter(float jitter);

    // Surface settings
    bool GetAlignToNormal() const;
    void SetAlignToNormal(bool align);
    float GetNormalAlignmentStrength() const;
    void SetNormalAlignmentStrength(float strength);
    float GetSurfaceOffset() const;
    void SetSurfaceOffset(float offset);

    // Slope filtering
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

    // Color variation
    bool GetColorVariationEnabled() const;
    void SetColorVariationEnabled(bool enabled);
    Color GetBaseColor() const;
    void SetBaseColor(const Color& color);
    float GetColorVariation() const;
    void SetColorVariation(float variation);

    // Randomization
    int GetRandomSeed() const;
    void SetRandomSeed(int seed);
    bool GetUseRandomSeed() const;
    void SetUseRandomSeed(bool use);

    // Scan area
    Vec3 GetScanAreaSize() const;
    void SetScanAreaSize(const Vec3& size);
    Vec3 GetScanAreaOffset() const;
    void SetScanAreaOffset(const Vec3& offset);

    // Debug visualization
    bool GetShowDebugShape() const;
    void SetShowDebugShape(bool show);
    Color GetDebugColor() const;
    void SetDebugColor(const Color& color);

    // Regeneration
    void RegenerateScatter();
    bool NeedsRegeneration() const { return m_NeedsRegeneration; }

private:
    // Scatter state
    bool m_NeedsRegeneration;
    std::mt19937 m_RandomGenerator;

    // Tracking for change detection
    Vec3 m_LastNodePosition;
    Quat m_LastNodeRotation;
    Vec3 m_LastNodeScale;
    std::string m_LastMeshPath;
    bool m_TrackingInitialized;

    // Cached scatter points (stored in LOCAL space relative to node)
    struct ScatterPoint {
        Vec3 position;      // Local space position
        Vec3 worldPosition; // World space position (for raycasting)
        Vec3 normal;        // World space normal
        bool valid;
    };
    std::vector<ScatterPoint> m_CachedScatterPoints;

    /**
     * Generate scatter points based on current settings
     */
    void GenerateScatterPoints();

    /**
     * Generate random scatter points
     */
    void GenerateRandomPoints(std::vector<ScatterPoint>& points);

    /**
     * Generate grid-based scatter points
     */
    void GenerateGridPoints(std::vector<ScatterPoint>& points);

    /**
     * Generate outline scatter points
     */
    void GenerateOutlinePoints(std::vector<ScatterPoint>& points);

    /**
     * Raycast to find surface point
     * Returns true if hit a scatter layer, false if hit cutout or no hit
     */
    bool RaycastToSurface(const Vec3& origin, const Vec3& direction, float maxDist,
                          Vec3& outPoint, Vec3& outNormal);

    /**
     * Check if a collision body matches scatter layers
     */
    bool IsScatterLayer(uint32_t bodyLayers) const;

    /**
     * Check if a collision body matches cutout layers
     */
    bool IsCutoutLayer(uint32_t bodyLayers) const;

    /**
     * Check if surface slope is within acceptable range
     */
    bool IsSlopeAcceptable(const Vec3& normal) const;

    /**
     * Generate random scale
     */
    Vec3 GenerateRandomScale();

    /**
     * Generate random rotation (optionally aligned to normal)
     */
    Vec3 GenerateRandomRotation(const Vec3& surfaceNormal);

    /**
     * Generate random color
     */
    Color GenerateRandomColor();

    /**
     * Build instance transforms from scatter points
     */
    void BuildInstancesFromPoints();

    /**
     * Draw debug visualization
     */
    void DrawDebugShape();

    /**
     * Check if scatter parameters have changed
     */
    void CheckForChanges();

public:
    // ===== Direct Collision Shape Raycasting =====

    /**
     * Information about a collision shape in the scene
     */
    struct CollisionShapeInfo {
        CollisionMesh3DComponent* component;
        core::Node* node;
        Mat4 worldTransform;
        uint32_t layers;
    };

private:

    /**
     * Cached collision shapes from scene
     */
    std::vector<CollisionShapeInfo> m_CachedCollisionShapes;

    /**
     * Find all collision shapes in the scene
     */
    void FindCollisionShapes();

    /**
     * Raycast against cached collision shapes
     * Returns true if hit, fills outPoint and outNormal
     */
    bool RaycastAgainstShapes(const Vec3& origin, const Vec3& direction, float maxDist,
                               Vec3& outPoint, Vec3& outNormal, uint32_t& outLayers);

    /**
     * Ray-Box intersection test
     * Returns distance to hit or -1 if no hit
     */
    float RayBoxIntersect(const Vec3& rayOrigin, const Vec3& rayDir,
                          const Vec3& boxCenter, const Vec3& boxSize,
                          const Mat4& boxTransform, Vec3& outNormal);

    /**
     * Ray-Sphere intersection test
     */
    float RaySphereIntersect(const Vec3& rayOrigin, const Vec3& rayDir,
                             const Vec3& sphereCenter, float radius, Vec3& outNormal);

    /**
     * Ray-Capsule intersection test
     */
    float RayCapsuleIntersect(const Vec3& rayOrigin, const Vec3& rayDir,
                              const Vec3& capsuleCenter, float radius, float height,
                              const Mat4& transform, Vec3& outNormal);

    /**
     * Ray-Cylinder intersection test
     */
    float RayCylinderIntersect(const Vec3& rayOrigin, const Vec3& rayDir,
                               const Vec3& cylinderCenter, float radius, float height,
                               const Mat4& transform, Vec3& outNormal);

    /**
     * Ray-Plane intersection test
     */
    float RayPlaneIntersect(const Vec3& rayOrigin, const Vec3& rayDir,
                            const Vec3& planeCenter, const Vec3& planeNormal,
                            float planeWidth, float planeLength,
                            const Mat4& transform, Vec3& outNormal);
};

} // namespace components
} // namespace lupine
