#pragma once

#include "lupine/components/MultiMeshGeneric.hpp"
#include "lupine/math/Math.hpp"
#include <nlohmann/json.hpp>
#include <random>

namespace lupine {
namespace components {

using namespace math;

/**
 * Scatter shape type for mesh distribution
 */
enum class ScatterShapeType {
    Cube = 0,
    Sphere = 1
};

/**
 * Clumping behavior mode
 */
enum class ClumpingMode {
    None = 0,       // Uniform distribution
    Clustered = 1,  // Instances cluster around random points
    Centered = 2    // Instances cluster toward center
};

/**
 * ScatterMultiMesh Component
 *
 * Automatically scatters instances of a mesh within a defined volume (cube or sphere).
 * Inherits from MultiMeshGeneric for GPU-instanced rendering.
 *
 * Features:
 * - Cube or Sphere scatter shapes
 * - Configurable dimensions (width/height/depth or radius)
 * - Clumping behavior for natural-looking distributions
 * - Scale variation per instance
 * - Rotation variation per instance
 * - Color variation per instance
 * - Random seed for reproducible results
 * - Editor preview modes
 * - Material override support (inherited)
 */
class ScatterMultiMesh : public MultiMeshGeneric {
public:
    ScatterMultiMesh();
    explicit ScatterMultiMesh(const std::string& name);
    virtual ~ScatterMultiMesh();

    // ISerializable interface
    std::string GetTypeName() const override { return "ScatterMultiMesh"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnRender() override;

    // Property change notification
    void OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) override;

    // IRenderableComponent override - handles regeneration before rendering
    void buildDrawCommands(RenderContext& ctx) override;

    // Debug visualization settings
    bool GetShowDebugShape() const;
    void SetShowDebugShape(bool show);
    Color GetDebugColor() const;
    void SetDebugColor(const Color& color);

    // Scatter shape settings
    ScatterShapeType GetScatterShape() const;
    void SetScatterShape(ScatterShapeType shape);

    // Cube dimensions (used when shape is Cube)
    float GetWidth() const;
    void SetWidth(float width);
    float GetHeight() const;
    void SetHeight(float height);
    float GetDepth() const;
    void SetDepth(float depth);
    Vec3 GetCubeSize() const;
    void SetCubeSize(const Vec3& size);

    // Sphere radius (used when shape is Sphere)
    float GetRadius() const;
    void SetRadius(float radius);

    // Scatter count
    int GetScatterCount() const;
    void SetScatterCount(int count);

    // Clumping settings
    ClumpingMode GetClumpingMode() const;
    void SetClumpingMode(ClumpingMode mode);
    float GetClumpFactor() const;
    void SetClumpFactor(float factor);
    int GetClumpCount() const;
    void SetClumpCount(int count);
    float GetClumpRadius() const;
    void SetClumpRadius(float radius);

    // Variation settings
    Vec2 GetScaleRange() const;
    void SetScaleRange(const Vec2& range);
    bool GetUniformScale() const;
    void SetUniformScale(bool uniform);
    Vec3 GetRotationVariation() const;
    void SetRotationVariation(const Vec3& variation);
    bool GetAlignToSurface() const;
    void SetAlignToSurface(bool align);

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

    // Regeneration
    void RegenerateScatter();
    bool NeedsRegeneration() const { return m_NeedsRegeneration; }

private:
    // Scatter state
    bool m_NeedsRegeneration;
    int m_LastScatterCount;
    int m_LastSeed;
    ScatterShapeType m_LastShape;
    Vec3 m_LastCubeSize;
    float m_LastRadius;

    // Random number generator
    std::mt19937 m_RandomGenerator;

    /**
     * Generate a random point within the current scatter shape
     */
    Vec3 GenerateRandomPoint();

    /**
     * Generate a random point within cube bounds
     */
    Vec3 GeneratePointInCube(float width, float height, float depth);

    /**
     * Generate a random point within sphere bounds
     */
    Vec3 GeneratePointInSphere(float radius);

    /**
     * Apply clumping to a point
     */
    Vec3 ApplyClumping(const Vec3& point, const std::vector<Vec3>& clumpCenters);

    /**
     * Generate random scale based on variation settings
     */
    Vec3 GenerateRandomScale();

    /**
     * Generate random rotation based on variation settings
     */
    Vec3 GenerateRandomRotation();

    /**
     * Generate random color based on variation settings
     */
    Color GenerateRandomColor();

    /**
     * Generate clump centers for clustered distribution
     */
    std::vector<Vec3> GenerateClumpCenters();

    /**
     * Check if parameters have changed requiring regeneration
     */
    void CheckForChanges();

    /**
     * Draw debug visualization of the scatter shape
     */
    void DrawDebugShape();
};

} // namespace components
} // namespace lupine
