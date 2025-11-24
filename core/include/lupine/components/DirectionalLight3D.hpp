#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/rendering/Light.hpp"
#include "lupine/math/Color.hpp"
#include "lupine/math/Vec3.hpp"
#include "lupine/math/AABB.hpp"

namespace lupine {
namespace components {

using namespace core;
using namespace math;

/**
 * DirectionalLight3D Component
 *
 * Represents a directional light source (like the sun) that illuminates
 * the entire scene from a specific direction. Light rays are parallel.
 *
 * Features:
 * - Color and intensity control
 * - Shadow casting with configurable settings
 * - Negative light mode (subtracts light instead of adding)
 * - Debug visualization in editor
 */
class DirectionalLight3D : public core::Component {
public:
    DirectionalLight3D();
    explicit DirectionalLight3D(const std::string& name);
    virtual ~DirectionalLight3D();

    // ISerializable interface
    std::string GetTypeName() const override { return "DirectionalLight3D"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnRender() override;

    // ===== Light Properties =====

    /**
     * Get/Set light color
     */
    Color GetColor() const;
    void SetColor(const Color& color);

    /**
     * Get/Set light intensity (multiplier for color)
     */
    float GetIntensity() const;
    void SetIntensity(float intensity);

    /**
     * Get/Set negative light mode (subtracts light instead of adding)
     */
    bool IsNegative() const;
    void SetNegative(bool negative);

    // ===== Shadow Settings =====

    /**
     * Get/Set shadow casting enabled
     */
    bool CastsShadows() const;
    void SetCastsShadows(bool castShadows);

    /**
     * Get/Set shadow opacity (0.0 = no shadow, 1.0 = full shadow)
     */
    float GetShadowOpacity() const;
    void SetShadowOpacity(float opacity);

    /**
     * Get/Set shadow blur amount
     */
    float GetShadowBlur() const;
    void SetShadowBlur(float blur);

    /**
     * Get/Set shadow bias (prevents shadow acne)
     */
    float GetShadowBias() const;
    void SetShadowBias(float bias);

    /**
     * Get/Set shadow map resolution
     */
    int GetShadowResolution() const;
    void SetShadowResolution(int resolution);

    /**
     * Get/Set number of shadow cascades (for CSM)
     */
    int GetShadowCascades() const;
    void SetShadowCascades(int cascades);

    // ===== Light Data Conversion =====

    /**
     * Convert to engine light descriptor for rendering
     */
    LightDescriptor ToLightDescriptor() const;

private:
    /**
     * Draw debug visualization in editor
     */
    void DrawDebugVisualization();
};

} // namespace components
} // namespace lupine

