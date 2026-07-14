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
 * Tree
 *
 * A scrollable hierarchical list with expand/collapse and indentation. The tree is
 * stored as a tab-indented string: each line is one item, and the number of
 * leading tab characters is its depth. An item is a branch if the next item is
 * deeper; branches show an expand/collapse arrow. Expand state is runtime.
 *
 * Signals:
 * - item_selected(index)   (index = absolute item index)
 * - item_activated(index)  (double-click)
 */
class Tree : public UIControl, public IRenderableComponent {
public:
    Tree();
    explicit Tree(const std::string& name);
    virtual ~Tree();

    std::string GetTypeName() const override { return "Tree"; }
    void DefineProperties() override;
    void DefineSignals() override;

    // Theme: font/background/border/selection/hover colours, font size + border width.
    const std::vector<ThemeBinding>& GetThemeBindings() const override;

    void OnInput(float deltaTime) override;
    void OnUpdate(float deltaTime) override;
    bool OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) override;

    // ===== Items =====
    int GetItemCount() const;
    std::string GetItemText(int index) const;
    int GetItemDepth(int index) const;
    void AddItem(const std::string& text, int depth);
    void ClearItems();

    int GetSelectedIndex() const;
    void SetSelectedIndex(int index);

    bool IsExpanded(int index) const;
    void SetExpanded(int index, bool expanded);
    void ToggleExpand(int index);

    // ===== Appearance =====
    float GetItemHeight() const;
    void SetItemHeight(float height);
    float GetIndentWidth() const;
    void SetIndentWidth(float width);
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

    // ===== IRenderableComponent =====
    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override { return GetUISpatialType(); }
    math::Vec2 GetContentMinSize() const override;

private:
    struct Item { int depth; std::string text; };

    void EnsureFont(RenderContext& ctx);
    std::vector<Item> ParseItems() const;
    void WriteItems(const std::vector<Item>& items);
    bool IsBranch(int index, const std::vector<Item>& items) const;
    std::vector<int> ComputeVisibleRows(const std::vector<Item>& items) const;
    void SyncCollapsedSize(int count) const;

    math::Vec2 GetCenter() const;
    math::Rect GetInnerRect() const;
    void ClampScroll(int visibleCount);

    asset::AssetRef<asset::FontAsset> m_FontAsset;
    FontHandle m_FontHandle;
    std::string m_CurrentFontPath;
    float m_CurrentFontSize{16.0f};
    const FontAtlas* m_LastAtlas{nullptr};

    MeshHandle m_TextMesh;
    std::string m_TextMeshKey;

    // Collapsed state by absolute item index (1 = collapsed). Mutable so it can be
    // lazily resized from const accessors.
    mutable std::vector<uint8_t> m_Collapsed;

    float m_ScrollY{0.0f};
    int m_HoverRow{-1};

    float m_Time{0.0f};
    float m_LastClickTime{-10.0f};
    int m_LastClickItem{-1};
};

} // namespace components
} // namespace lupine
