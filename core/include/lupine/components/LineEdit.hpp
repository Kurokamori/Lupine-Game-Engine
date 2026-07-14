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
 * LineEdit
 *
 * A single-line editable text field with a caret, selection, clipboard support,
 * placeholder text, password masking, and horizontal scrolling. Text overflowing
 * the field is clipped with the engine scissor mechanism.
 *
 * Signals:
 * - text_changed(text): emitted whenever the contents change
 * - text_submitted(text): emitted when Enter is pressed
 */
class LineEdit : public UIControl, public IRenderableComponent {
public:
    LineEdit();
    explicit LineEdit(const std::string& name);
    virtual ~LineEdit();

    std::string GetTypeName() const override { return "LineEdit"; }
    void DefineProperties() override;
    void DefineSignals() override;

    // Theme: text/placeholder/background/border/selection/caret colours, font
    // size, corner radius + border width.
    const std::vector<ThemeBinding>& GetThemeBindings() const override;

    void OnAwake() override;
    void OnInput(float deltaTime) override;
    void OnUpdate(float deltaTime) override;
    bool OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) override;

    // ===== Text =====
    std::string GetText() const;
    void SetText(const std::string& text);
    std::string GetPlaceholder() const;
    void SetPlaceholder(const std::string& text);

    bool GetEditable() const;
    void SetEditable(bool editable);
    bool GetSecret() const;
    void SetSecret(bool secret);
    int GetMaxLength() const;
    void SetMaxLength(int maxLength);

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

    math::Color GetFontColor() const;
    void SetFontColor(const math::Color& color);
    math::Color GetPlaceholderColor() const;
    void SetPlaceholderColor(const math::Color& color);
    math::Color GetBackgroundColor() const;
    void SetBackgroundColor(const math::Color& color);
    math::Color GetBorderColor() const;
    void SetBorderColor(const math::Color& color);
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

private:
    // Font lifecycle (mirrors Label/Button).
    void EnsureFont(RenderContext& ctx);

    // Codepoint <-> property string synchronization.
    void SyncTextFromProperty();
    void WriteTextToProperty();

    // The string actually rendered (password mask applied).
    std::string DisplayString() const;

    // Caret / selection helpers.
    int SelectionMin() const;
    int SelectionMax() const;
    void DeleteSelection();
    void InsertCodepoint(uint32_t codepoint);
    void InsertString(const std::string& utf8);
    int CaretIndexFromLocalX(float localX, const FontAtlas& atlas) const;
    float PrefixWidth(int index, const FontAtlas& atlas) const;

    math::Vec2 GetCenter() const;
    math::Rect GetInnerRect() const;
    void UpdateScroll(const FontAtlas& atlas);
    void EmitTextChanged();

    // Font
    asset::AssetRef<asset::FontAsset> m_FontAsset;
    FontHandle m_FontHandle;
    std::string m_CurrentFontPath;
    float m_CurrentFontSize{16.0f};

    // Last GPU atlas used for rendering, cached so OnInput (which has no
    // RenderContext) can hit-test the caret against the rendered glyph metrics.
    const FontAtlas* m_LastAtlas{nullptr};

    // Editing state (codepoints).
    std::vector<uint32_t> m_Text;
    std::string m_CachedPropertyText;
    int m_Caret{0};
    int m_SelAnchor{-1};   // -1 = no selection
    float m_ScrollX{0.0f};

    // Caret blink.
    float m_BlinkTime{0.0f};
    bool m_CaretVisible{true};

    bool m_MouseSelecting{false};

    // Cached text mesh (rebuilt only when its inputs change, so the draw item
    // references a live mesh through command execution).
    MeshHandle m_TextMesh;
    std::string m_TextMeshKey;
};

} // namespace components
} // namespace lupine
