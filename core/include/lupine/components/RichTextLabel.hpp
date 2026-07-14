#pragma once

#include "lupine/components/UIControl.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/ResourceHandles.hpp"
#include "lupine/rendering/TextLayout.hpp"
#include "lupine/asset/FontAsset.hpp"
#include "lupine/math/Math.hpp"
#include <string>

namespace lupine {
namespace components {

/**
 * RichTextLabel
 *
 * Renders text with inline BBCode-style markup:
 *   [b]bold[/b]  [i]italic[/i]  [color=#rrggbb]colored[/color]  [color=red]named[/color]
 *   [size=24]bigger[/size]
 * Tags nest; unknown tags are ignored. Supports word-wrap to the control width,
 * multiple line, line spacing, and horizontal alignment. Bold is faux-bold
 * (double-struck) and italic is a shear, since a single font atlas is used.
 */
class RichTextLabel : public UIControl, public IRenderableComponent {
public:
    RichTextLabel();
    explicit RichTextLabel(const std::string& name);
    virtual ~RichTextLabel();

    std::string GetTypeName() const override { return "RichTextLabel"; }
    void DefineProperties() override;

    // Theme: default text colour + font size (per-span BBCode colours are not themed).
    const std::vector<ThemeBinding>& GetThemeBindings() const override;

    void OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) override;
    bool OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) override;

    std::string GetText() const;
    void SetText(const std::string& text);
    std::string GetFontPath() const;
    void SetFontPath(const std::string& path);
    float GetFontSize() const;
    void SetFontSize(float size);
    math::Color GetColor() const;
    void SetColor(const math::Color& color);
    bool GetWordWrap() const;
    void SetWordWrap(bool wrap);
    TextHAlign GetHorizontalAlign() const;
    void SetHorizontalAlign(TextHAlign align);
    float GetLineSpacing() const;
    void SetLineSpacing(float spacing);

    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override { return GetUISpatialType(); }
    math::Vec2 GetContentMinSize() const override;

private:
    void EnsureFont(RenderContext& ctx);
    math::Vec2 GetCenter() const;

    /**
     * The rectangle the text is laid out in, in world space, and the single source of
     * truth for both the renderer and the reported bounds. Rect::position is the MIN
     * corner on the Y-up canvas; the text descends from the box's TOP-left
     * (GetTopLeft()).
     */
    math::Rect GetTextBoxRect() const;

    asset::AssetRef<asset::FontAsset> m_FontAsset;
    FontHandle m_FontHandle;
    std::string m_CurrentFontPath;
    float m_CurrentFontSize{16.0f};

    MeshHandle m_TextMesh;
    std::string m_TextMeshKey;
    mutable math::Vec2 m_CachedSize{0.0f, 0.0f};
    bool m_Dirty{true};
};

} // namespace components
} // namespace lupine
