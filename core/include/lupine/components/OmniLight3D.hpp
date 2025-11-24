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
 * OmniLight3D Component (Point Light)
 *
 * Represents an omnidirectional point light source that emits light
 * in all directions from a single point in space.
 *
 * Features:
 * - Color and intensity control
 * - Range/attenuation control
 * - Shadow casting with configurable settings
 * - Negative light mode (subtracts light instead of adding)
 * - Debug visualization in editor
 */
class OmniLight3D : public core::Component {
public:
    OmniLight3D();
    explicit OmniLight3D(const std::string& name);
    virtual ~OmniLight3D();

    // ISerializable interface
    std::string GetTypeName() const override { return "OmniLight3D"; }
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
     * Get/Set light range (distance at which light intensity reaches zero)
     */
    float GetRange() const;
    void SetRange(float range);

    /**
     * Get/Set attenuation exponent (controls falloff curve)
     */
    float GetAttenuation() const;
    void SetAttenuation(float attenuation);

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
     * Get/Set shadow map resolution (per cubemap face)
     */
    int GetShadowResolution() const;
    void SetShadowResolution(int resolution);

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

