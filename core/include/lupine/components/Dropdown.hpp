#pragma once

#include "lupine/components/UIControl.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/ResourceHandles.hpp"
#include "lupine/rendering/TextLayout.hpp"
#include "lupine/asset/FontAsset.hpp"
#include "lupine/math/Math.hpp"
#include <string>
#include <vector>

namespace lupine {
namespace components {

/**
 * Dropdown (OptionButton)
 *
 * A button that displays the currently selected option and, when clicked, opens an
 * inline list of options below it. Selecting an option closes the list and updates
 * the selection. Items are stored as a newline-separated string.
 *
 * Signals:
 * - item_selected(index)
 */
class Dropdown : public UIControl, public IRenderableComponent {
public:
    Dropdown();
    explicit Dropdown(const std::string& name);
    virtual ~Dropdown();

    std::string GetTypeName() const override { return "Dropdown"; }
    void DefineProperties() override;
    void DefineSignals() override;

    // Theme: font/background/list/hover colours, font size, corner radius + border.
    const std::vector<ThemeBinding>& GetThemeBindings() const override;

    void OnInput(float deltaTime) override;
    bool OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) override;

    // ===== Items =====
    int GetItemCount() const;
    std::string GetItem(int index) const;
    void AddItem(const std::string& text);
    void RemoveItem(int index);
    void ClearItems();
    void SetItems(const std::vector<std::string>& items);

    int GetSelectedIndex() const;
    void SetSelectedIndex(int index);
    std::string GetSelectedText() const;

    bool IsOpen() const { return m_Open; }

    // ===== Appearance =====
    std::string GetPlaceholder() const;
    void SetPlaceholder(const std::string& text);
    float GetItemHeight() const;
    void SetItemHeight(float height);
    std::string GetFontPath() const;
    void SetFontPath(const std::string& path);
    float GetFontSize() const;
    void SetFontSize(float size);

    math::Color GetFontColor() const;
    void SetFontColor(const math::Color& color);
    math::Color GetBackgroundColor() const;
    void SetBackgroundColor(const math::Color& color);
    math::Color GetListColor() const;
    void SetListColor(const math::Color& color);
    math::Color GetHoverColor() const;
    void SetHoverColor(const math::Color& color);

    // ===== IRenderableComponent =====
    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override { return GetUISpatialType(); }
    math::Vec2 GetContentMinSize() const override;

private:
    void EnsureFont(RenderContext& ctx);
    std::vector<std::string> ParseItems() const;
    void WriteItems(const std::vector<std::string>& items);

    math::Vec2 GetCenter() const;
    math::Rect GetButtonRect() const;
    math::Rect GetListRect() const;
    int ItemIndexFromMouse(const math::Vec2& mouse) const;

    // Build/refresh a cached text mesh from a key; returns the handle to draw.
    FontHandle m_FontHandle;
    asset::AssetRef<asset::FontAsset> m_FontAsset;
    std::string m_CurrentFontPath;
    float m_CurrentFontSize{16.0f};
    const FontAtlas* m_LastAtlas{nullptr};

    MeshHandle m_ButtonTextMesh;
    std::string m_ButtonTextKey;
    MeshHandle m_ListTextMesh;
    std::string m_ListTextKey;

    bool m_Open{false};
    int m_HoverIndex{-1};
};

} // namespace components
} // namespace lupine
