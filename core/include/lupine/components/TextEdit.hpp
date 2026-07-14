#pragma once

#include "lupine/components/UIControl.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/ResourceHandles.hpp"
#include "lupine/rendering/TextLayout.hpp"
#include "lupine/asset/FontAsset.hpp"
#include "lupine/math/Math.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace lupine {
namespace components {

/**
 * TextEdit
 *
 * A multi-line editable text area with a caret, multi-line selection, clipboard
 * support, and 2D scrolling. Lines are logical (split on '\n'); long lines scroll
 * horizontally. Content is clipped to the field with the engine scissor mechanism.
 *
 * Signals:
 * - text_changed(text): emitted whenever the contents change
 */
class TextEdit : public UIControl, public IRenderableComponent {
public:
    TextEdit();
    explicit TextEdit(const std::string& name);
    virtual ~TextEdit();

    std::string GetTypeName() const override { return "TextEdit"; }
    void DefineProperties() override;
    void DefineSignals() override;

    // Theme: text/background/border/selection/caret colours, font size, corner
    // radius + border width.
    const std::vector<ThemeBinding>& GetThemeBindings() const override;

    void OnAwake() override;
    void OnInput(float deltaTime) override;
    void OnUpdate(float deltaTime) override;
    bool OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) override;

    // ===== Text =====
    std::string GetText() const;
    void SetText(const std::string& text);
    bool GetEditable() const;
    void SetEditable(bool editable);

    // ===== Caret / selection =====
    int GetCaretPosition() const { return m_Caret; }
    void SetCaretPosition(int position);
    void SelectAll();
    void Deselect();
    bool HasSelection() const;
    std::string GetSelectedText() const;

    // ===== Font / appearance =====
    std::string GetFontPath() const;
    void SetFontPath(const std::string& path);
    float GetFontSize() const;
    void SetFontSize(float size);
    float GetLineSpacing() const;
    void SetLineSpacing(float spacing);

    math::Color GetFontColor() const;
    void SetFontColor(const math::Color& color);
    math::Color GetBackgroundColor() const;
    void SetBackgroundColor(const math::Color& color);
    math::Color GetSelectionColor() const;
    void SetSelectionColor(const math::Color& color);
    math::Color GetCaretColor() const;
    void SetCaretColor(const math::Color& color);

    // ===== IRenderableComponent =====
    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override { return GetUISpatialType(); }
    math::Vec2 GetContentMinSize() const override;

    // Always clip descendants too (in case content is nested), but the field draws
    // its own clipped text directly.
    bool ClipsDescendants() const override { return true; }

private:
    void EnsureFont(RenderContext& ctx);

    void SyncTextFromProperty();
    void WriteTextToProperty();

    // Line model (logical lines split on '\n'), recomputed from m_Text.
    struct LineSpan { int start; int end; }; // codepoint indices [start, end)
    std::vector<LineSpan> ComputeLines() const;
    int LineIndexOf(int caret, const std::vector<LineSpan>& lines) const;

    int SelectionMin() const;
    int SelectionMax() const;
    void DeleteSelection();
    void InsertCodepoint(uint32_t codepoint);
    void InsertString(const std::string& utf8);

    std::string LineString(const LineSpan& span) const;
    float CaretXForLine(const LineSpan& span, int caret, const FontAtlas& atlas) const;
    int CaretFromMouse(const math::Vec2& mouse, const FontAtlas& atlas);
    void EnsureCaretVisible(const FontAtlas& atlas);

    math::Vec2 GetCenter() const;
    math::Rect GetInnerRect() const;
    float LineAdvance(const FontAtlas& atlas) const;
    void EmitTextChanged();

    // Font
    asset::AssetRef<asset::FontAsset> m_FontAsset;
    FontHandle m_FontHandle;
    std::string m_CurrentFontPath;
    float m_CurrentFontSize{16.0f};
    const FontAtlas* m_LastAtlas{nullptr};

    // Cached text mesh.
    MeshHandle m_TextMesh;
    std::string m_TextMeshKey;

    // Editing state (codepoints, including '\n').
    std::vector<uint32_t> m_Text;
    std::string m_CachedPropertyText;
    int m_Caret{0};
    int m_SelAnchor{-1};
    float m_ScrollX{0.0f};
    float m_ScrollY{0.0f};

    // Caret blink.
    float m_BlinkTime{0.0f};
    bool m_CaretVisible{true};
    bool m_MouseSelecting{false};
};

} // namespace components
} // namespace lupine
