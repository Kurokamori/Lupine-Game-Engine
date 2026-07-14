#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/LinkedProperty.hpp"
#include "lupine/components/UIControl.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/ResourceHandles.hpp"
#include "lupine/asset/Asset.hpp"
#include "lupine/asset/ImageAsset.hpp"
#include "lupine/asset/FontAsset.hpp"
#include <string>

namespace lupine {
namespace components {

/**
 * ProgressBar Component
 *
 * A progress bar that displays an overall value between the min value and the max value.
 * Think Health Bars, loading bars, etc.
 *
 * Features:
 * - Min/Max/Current value with step support
 * - Smooth interpolation option
 * - Multiple orientations (Horizontal, Vertical)
 * - Multiple fill directions (Left to Right, Right to Left, Up to Down, Down to Up)
 * - Background, Fill, and Border textures/styleboxes
 * - Optional value display with customizable font
 */
class ProgressBar : public UIControl, public IRenderableComponent {
public:
    ProgressBar();
    explicit ProgressBar(const std::string& name);
    virtual ~ProgressBar();

    // ISerializable interface
    std::string GetTypeName() const override { return "ProgressBar"; }
    void DefineProperties() override;

    // Theme: background/fill/border/value colours + value font size + corner radius.
    const std::vector<ThemeBinding>& GetThemeBindings() const override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnUpdate(float deltaTime) override;

    // Editor gizmo hooks
    bool OnGizmoScale(float scaleDelta, int axis, bool is3D) override;

    // Asset hot-reload support
    bool OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) override;

    // ===== Value Properties =====
    
    float GetMinValue() const;
    void SetMinValue(float value);

    float GetMaxValue() const;
    void SetMaxValue(float value);

    float GetValue() const;
    void SetValue(float value);

    float GetStep() const;
    void SetStep(float step);

    // ===== Smooth Properties =====
    
    bool GetSmooth() const;
    void SetSmooth(bool smooth);

    float GetSmoothSpeed() const;
    void SetSmoothSpeed(float speed);

    // ===== Size Properties =====
    // Width/height/size are provided by the UIControl base class.

    // ===== Orientation & Fill Direction =====
    
    int GetOrientation() const;  // 0=Horizontal, 1=Vertical
    void SetOrientation(int orientation);

    int GetFillDirection() const;  // 0=LeftToRight, 1=RightToLeft, 2=UpToDown, 3=DownToUp
    void SetFillDirection(int direction);

    // ===== Background Texture =====
    
    const std::string& GetBackgroundTexturePath() const;
    void SetBackgroundTexturePath(const std::string& path);

    math::Color GetBackgroundColor() const;
    void SetBackgroundColor(const math::Color& color);

    // ===== Fill Texture =====
    
    const std::string& GetFillTexturePath() const;
    void SetFillTexturePath(const std::string& path);

    math::Color GetFillColor() const;
    void SetFillColor(const math::Color& color);

    // ===== Border Texture =====
    
    const std::string& GetBorderTexturePath() const;
    void SetBorderTexturePath(const std::string& path);

    math::Color GetBorderColor() const;
    void SetBorderColor(const math::Color& color);

    // ===== Value Display =====
    
    bool GetShowValue() const;
    void SetShowValue(bool show);

    const std::string& GetValueFontPath() const;
    void SetValueFontPath(const std::string& path);

    float GetValueFontSize() const;
    void SetValueFontSize(float size);

    math::Color GetValueColor() const;
    void SetValueColor(const math::Color& color);

    // ===== Corner Radius Properties =====

    const core::LinkedProperty4& GetCornerRadius() const;
    void SetCornerRadius(const core::LinkedProperty4& radius);
    void SetCornerRadiusAll(float radius);
    void SetCornerRadiusIndividual(size_t index, float radius);
    bool IsCornerRadiusLinked() const;
    void SetCornerRadiusLinked(bool linked);

    // ===== Border Width Properties =====

    const core::LinkedProperty4& GetBorderWidth() const;
    void SetBorderWidth(const core::LinkedProperty4& width);
    void SetBorderWidthAll(float width);
    void SetBorderWidthIndividual(size_t index, float width);
    bool IsBorderWidthLinked() const;
    void SetBorderWidthLinked(bool linked);

    // ===== IRenderableComponent Interface =====

    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override;

private:
    // Helper methods
    void LoadBackgroundTexture(const std::string& path);
    void LoadFillTexture(const std::string& path);
    void LoadBorderTexture(const std::string& path);
    void LoadFont(const std::string& path);
    
    void RenderBackground(RenderContext& ctx, const math::Vec2& position, const math::Vec2& size, float rotation);
    void RenderFill(RenderContext& ctx, const math::Vec2& position, const math::Vec2& size, float rotation);
    void RenderBorder(RenderContext& ctx, const math::Vec2& position, const math::Vec2& size, float rotation);
    void RenderValue(RenderContext& ctx, const math::Vec2& position, const math::Vec2& size);
    
    float CalculateFillRatio() const;
    void RegenerateTextMesh(RenderContext& ctx, const math::Vec2& position, const math::Vec2& size);

    // Texture assets and handles
    asset::AssetRef<asset::ImageAsset> m_BackgroundTextureAsset;
    TextureHandle m_BackgroundTextureHandle;
    std::string m_CurrentBackgroundTexturePath;

    asset::AssetRef<asset::ImageAsset> m_FillTextureAsset;
    TextureHandle m_FillTextureHandle;
    std::string m_CurrentFillTexturePath;

    asset::AssetRef<asset::ImageAsset> m_BorderTextureAsset;
    TextureHandle m_BorderTextureHandle;
    std::string m_CurrentBorderTexturePath;

    // Font assets and handles
    asset::AssetRef<asset::FontAsset> m_FontAsset;
    FontHandle m_FontHandle;
    std::string m_CurrentFontPath;
    float m_CurrentFontSize;

    // Text mesh for value display
    MeshHandle m_TextMesh;
    bool m_TextMeshNeedsRegeneration;
    std::string m_CachedValueText;

    // Smooth interpolation
    float m_DisplayValue;  // The smoothly interpolated display value

    // Style properties
    core::LinkedProperty4 m_CornerRadius;
    core::LinkedProperty4 m_BorderWidth;
};

} // namespace components
} // namespace lupine

