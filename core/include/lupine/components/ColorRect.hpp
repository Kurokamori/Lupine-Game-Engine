#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include "lupine/core/LinkedProperty.hpp"
#include "lupine/components/UIControl.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/rendering/ResourceHandles.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/components/CustomShaderParams.hpp"
#include <string>

namespace lupine {
namespace components {

// Forward declare BlendMode from Image2D.hpp to avoid circular dependency
enum class BlendMode;

/**
 * ColorRect Component
 *
 * A UI component that renders a colored rectangle with customizable properties.
 * This is the foundational UI element that supports:
 * - Solid color fill
 * - Rounded corners (individually controllable or linked)
 * - Border rendering with customizable width (per-edge or linked)
 * - Flexible sizing
 * - Layer ordering
 * - UI space selection (Camera2D or CameraUI)
 *
 * Features:
 * - Color with alpha support
 * - Enable/disable rendering
 * - Layer and sorting order for draw order control
 * - Width and height properties
 * - Corner radius with linked/unlinked editing
 * - Border with customizable color and per-edge width control
 * - UI space toggle (belongs to Camera2D or CameraUI render pass)
 */
class ColorRect : public UIControl, public IRenderableComponent {
public:
    ColorRect();
    explicit ColorRect(const std::string& name);
    virtual ~ColorRect();

    // ISerializable interface
    std::string GetTypeName() const override { return "ColorRect"; }
    void DefineProperties() override;
    void DefineSignals() override;

    // Theme: fill/border colours + uniform corner radius.
    const std::vector<ThemeBinding>& GetThemeBindings() const override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnInput(float deltaTime) override;
    void OnRender() override;

    // Editor gizmo hooks
    bool OnGizmoScale(float scaleDelta, int axis, bool is3D) override;

    // ===== Core Properties =====
    
    /**
     * Get/Set the fill color of the rectangle
     */
    math::Color GetColor() const;
    void SetColor(const math::Color& color);
    
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

    // ===== Size Properties =====
    // Width/height/size are provided by the UIControl base class.

    // ===== Corner Radius Properties =====
    
    /**
     * Get the corner radius linked property
     * Order: [top-left, top-right, bottom-right, bottom-left]
     */
    const core::LinkedProperty4& GetCornerRadius() const;
    void SetCornerRadius(const core::LinkedProperty4& radius);
    
    /**
     * Set all corner radii to the same value
     */
    void SetCornerRadiusAll(float radius);
    
    /**
     * Set individual corner radius
     * Index: 0=top-left, 1=top-right, 2=bottom-right, 3=bottom-left
     */
    void SetCornerRadiusIndividual(size_t index, float radius);
    
    /**
     * Get/Set corner radius linked state
     */
    bool IsCornerRadiusLinked() const;
    void SetCornerRadiusLinked(bool linked);

    // ===== Border Properties =====
    
    /**
     * Get/Set border enabled state
     */
    bool GetBorderEnabled() const;
    void SetBorderEnabled(bool enabled);
    
    /**
     * Get/Set border color
     */
    math::Color GetBorderColor() const;
    void SetBorderColor(const math::Color& color);
    
    /**
     * Get the border width linked property
     * Order: [top, right, bottom, left]
     */
    const core::LinkedProperty4& GetBorderWidth() const;
    void SetBorderWidth(const core::LinkedProperty4& width);
    
    /**
     * Set all border widths to the same value
     */
    void SetBorderWidthAll(float width);
    
    /**
     * Set individual border width
     * Index: 0=top, 1=right, 2=bottom, 3=left
     */
    void SetBorderWidthIndividual(size_t index, float width);
    
    /**
     * Get/Set border width linked state
     */
    bool IsBorderWidthLinked() const;
    void SetBorderWidthLinked(bool linked);

    // ===== UI Space Properties =====
    // GetUISpace/SetUISpace are provided by the UIControl base class.

    // ===== Mouse Handling =====
    // GetMouseFilter/SetMouseFilter, the hover/press queries and the mouse signals are
    // provided by the UIControl base class. A ColorRect defaults to MouseFilter::Ignore,
    // so it stays decorative (and click-transparent) unless the filter is changed.

    // ===== Blend Mode =====
    
    /**
     * Get/Set blend mode
     */
    BlendMode GetBlendMode() const;
    void SetBlendMode(BlendMode mode);
    
    /**
     * Helper: Convert enum to int for property system
     */
    int BlendModeToInt(BlendMode mode) const;
    BlendMode IntToBlendMode(int value) const;

    // ===== Material Override =====
    
    /**
     * Get/Set material override path
     * Empty string means use default material
     */
    const std::string& GetMaterialOverride() const;
    void SetMaterialOverride(const std::string& materialPath);

    // ===== Custom Shader (.lsh) =====

    /**
     * Get/Set the attached Lupine Shader (.lsh) path.
     * Empty string means use the built-in rounded-rect shader.
     * When set, the fill is rendered with the custom shader and its exported parameters.
     */
    const std::string& GetShader() const;
    void SetShader(const std::string& shaderPath);

    /**
     * Get/Set the serialized exported-shader-parameter values (JSON object).
     * Maps uniform name -> typed value, e.g.
     *   {"u_Intensity": {"type":"float","value":1.0},
     *    "u_GlowColor": {"type":"color","value":[1,0,0,1]}}
     * Managed by the editor's shader-parameter inspector; consumed at render time.
     */
    const std::string& GetShaderParameters() const;
    void SetShaderParameters(const std::string& parametersJson);

    // ===== IRenderableComponent Interface =====

    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override;

private:
    // Internal properties stored separately from property system
    // for performance-critical rendering
    core::LinkedProperty4 m_CornerRadius;
    core::LinkedProperty4 m_BorderWidth;
    
    /**
     * Helper: Convert LinkedProperty to Vec4 for serialization
     */
    math::Vec4 CornerRadiusToVec4() const;
    math::Vec4 BorderWidthToVec4() const;
    
    /**
     * Helper: Convert Vec4 to LinkedProperty for deserialization
     */
    void Vec4ToCornerRadius(const math::Vec4& vec);
    void Vec4ToBorderWidth(const math::Vec4& vec);
    
    /**
     * Render the filled rectangle
     */
    void RenderFill(RenderContext& ctx, const math::Vec2& center, const math::Vec2& size, float rotation);

    /**
     * Render the filled rectangle using the attached custom shader.
     * Returns false if the shader could not be resolved/compiled (caller falls back to RenderFill).
     */
    bool RenderFillCustomShader(RenderContext& ctx, const math::Vec2& center, const math::Vec2& size, float rotation);

    /**
     * Render the border
     */
    void RenderBorder(RenderContext& ctx, const math::Vec2& center, const math::Vec2& size, float rotation);

    // Custom shader parameter parsing + texture-parameter cache (shared helper).
    CustomShaderParams m_ShaderParams;
};

} // namespace components
} // namespace lupine
