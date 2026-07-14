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
 * ItemList
 *
 * A scrollable, single-selection list of text items. Items are stored as a
 * newline-separated string (the engine property system has no array type), and
 * managed through AddItem/RemoveItem/etc. Content scrolls vertically and is
 * clipped to the list rect with the engine scissor mechanism.
 *
 * Signals:
 * - item_selected(index): emitted when an item is clicked/selected
 * - item_activated(index): emitted when an item is double-clicked
 */
class ItemList : public UIControl, public IRenderableComponent {
public:
    ItemList();
    explicit ItemList(const std::string& name);
    virtual ~ItemList();

    std::string GetTypeName() const override { return "ItemList"; }
    void DefineProperties() override;
    void DefineSignals() override;

    // Theme: font/background/border/selection/hover colours, font size + border width.
    const std::vector<ThemeBinding>& GetThemeBindings() const override;

    void OnAwake() override;
    void OnInput(float deltaTime) override;
    void OnUpdate(float deltaTime) override;
    bool OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) override;

    // ===== Items =====
    int GetItemCount() const;
    std::string GetItem(int index) const;
    void AddItem(const std::string& text);
    void RemoveItem(int index);
    void ClearItems();
    void SetItems(const std::vector<std::string>& items);
    std::vector<std::string> GetItems() const;

    int GetSelectedIndex() const;
    void SetSelectedIndex(int index);

    // ===== Appearance =====
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
    math::Color GetSelectionColor() const;
    void SetSelectionColor(const math::Color& color);
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
    math::Rect GetInnerRect() const;
    int ItemIndexFromMouse(const math::Vec2& mouse) const;
    void ClampScroll();

    // Font
    asset::AssetRef<asset::FontAsset> m_FontAsset;
    FontHandle m_FontHandle;
    std::string m_CurrentFontPath;
    float m_CurrentFontSize{16.0f};
    const FontAtlas* m_LastAtlas{nullptr};

    // Cached text mesh.
    MeshHandle m_TextMesh;
    std::string m_TextMeshKey;

    float m_ScrollY{0.0f};
    int m_HoverIndex{-1};

    // Double-click detection.
    float m_Time{0.0f};
    float m_LastClickTime{-10.0f};
    int m_LastClickIndex{-1};
};

} // namespace components
} // namespace lupine
