#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/rendering/ResourceHandles.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include <string>

namespace lupine {
namespace components {

// Forward declare BlendMode from Image2D.hpp to avoid circular dependency
enum class BlendMode;

/**
 * Light2DBlendMode - Blend modes specifically for 2D lighting
 */
enum class Light2DBlendMode {
    SoftLight,  // Soft light blending - gentle lighting effect
    Overlay,    // Overlay blending - stronger contrast lighting
    Additive    // Additive blending - bright light accumulation
};

/**
 * Light2D Component
 *
 * A 2D light component that creates a point light effect in 2D space.
 * Uses radial gradient falloff and blend modes to simulate lighting.
 *
 * Features:
 * - Customizable light color with HDR intensity
 * - Adjustable range/radius for light falloff
 * - Intensity multiplier for brightness control
 * - Blend mode selection (Soft Light, Overlay, Additive)
 * - Falloff curve control for light attenuation
 * - Optional texture for light cookie/pattern
 * - Layer ordering for stacking multiple lights
 */
class Light2D : public core::Component, public IRenderableComponent {
public:
    Light2D();
    explicit Light2D(const std::string& name);
    virtual ~Light2D();

    // ISerializable interface
    std::string GetTypeName() const override { return "Light2D"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnRender() override;

    // Scripting control entry point
    nlohmann::json CallMethod(const std::string& method, const nlohmann::json& args) override;

    // Editor gizmo hooks
    bool OnGizmoScale(float scaleDelta, int axis, bool is3D) override;

    // ===== Light Properties =====
    
    /**
     * Get/Set the light color
     */
    math::Color GetColor() const;
    void SetColor(const math::Color& color);
    
    /**
     * Get/Set the light range (radius in world units)
     */
    float GetRange() const;
    void SetRange(float range);
    
    /**
     * Get/Set the light intensity (brightness multiplier)
     */
    float GetIntensity() const;
    void SetIntensity(float intensity);
    
    /**
     * Get/Set the falloff exponent (controls light attenuation curve)
     * 1.0 = linear, 2.0 = quadratic (realistic), 0.5 = slow falloff
     */
    float GetFalloff() const;
    void SetFalloff(float falloff);
    
    /**
     * Get/Set the blend mode for light rendering
     */
    Light2DBlendMode GetBlendMode() const;
    void SetBlendMode(Light2DBlendMode mode);
    
    /**
     * Get/Set whether light is enabled
     */
    bool GetLightEnabled() const;
    void SetLightEnabled(bool enabled);

    // ===== Shadow Properties =====

    /**
     * Get/Set whether this light casts shadows from LightOccluder2D components.
     * When enabled, the light is rendered as a visibility-clipped mesh so areas
     * blocked by occluders receive no light. Opt-in (default off) so existing
     * lights keep their gradient-based appearance.
     */
    bool GetShadowEnabled() const;
    void SetShadowEnabled(bool enabled);

    /**
     * Get/Set the angular resolution (number of perimeter samples) used to build
     * the round, unobstructed portion of the light. Higher = smoother circle and
     * crisper shadows at a small cost. Clamped to a sane range internally.
     */
    int GetShadowSampleCount() const;
    void SetShadowSampleCount(int count);

    // ===== Layering Properties =====
    
    /**
     * Get/Set render layer
     */
    int GetLayer() const;
    void SetLayer(int layer);
    
    /**
     * Get/Set sorting order within layer
     */
    int GetSortingOrder() const;
    void SetSortingOrder(int order);

    // ===== IRenderableComponent Interface =====

    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override;

private:
    /**
     * Helper: Convert enum to int for property system
     */
    int BlendModeToInt(Light2DBlendMode mode) const;
    Light2DBlendMode IntToBlendMode(int value) const;

    /**
     * Render the light effect (unshadowed radial gradient path)
     */
    void RenderLight(RenderContext& ctx, const math::Vec2& center, float range, float rotation);

    /**
     * Render the light as a visibility-clipped additive mesh so LightOccluder2D
     * components cast hard shadows. Used when shadows are enabled.
     */
    void RenderShadowedLight(RenderContext& ctx, const math::Vec2& center, float range);
};

} // namespace components
} // namespace lupine

