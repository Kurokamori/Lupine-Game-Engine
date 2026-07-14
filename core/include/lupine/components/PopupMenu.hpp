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
 * PopupMenu
 *
 * A floating vertical menu of selectable items. It renders nothing while closed;
 * call Popup()/PopupAt() to open it at the node position or an explicit canvas
 * position. Clicking an item emits item_selected and closes the menu; clicking
 * outside (or Escape) closes it.
 *
 * For a popup to draw above other UI, place its node on a high Z-index.
 *
 * Signals:
 * - item_selected(index)
 * - popup_closed()
 */
class PopupMenu : public UIControl, public IRenderableComponent {
public:
    PopupMenu();
    explicit PopupMenu(const std::string& name);
    virtual ~PopupMenu();

    std::string GetTypeName() const override { return "PopupMenu"; }
    void DefineProperties() override;
    void DefineSignals() override;

    // Theme: font/background/border/hover colours + font size.
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

    // ===== Open / close =====
    void Popup();
    void PopupAt(const math::Vec2& position);
    void Close();
    bool IsOpen() const { return m_Open; }

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
    math::Color GetHoverColor() const;
    void SetHoverColor(const math::Color& color);

    // ===== IRenderableComponent =====
    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override { return GetUISpatialType(); }

private:
    void EnsureFont(RenderContext& ctx);
    std::vector<std::string> ParseItems() const;
    void WriteItems(const std::vector<std::string>& items);

    math::Vec2 GetCenter() const;
    math::Rect ComputeMenuRect() const;
    int ItemIndexFromMouse(const math::Vec2& mouse) const;

    asset::AssetRef<asset::FontAsset> m_FontAsset;
    FontHandle m_FontHandle;
    std::string m_CurrentFontPath;
    float m_CurrentFontSize{16.0f};

    MeshHandle m_TextMesh;
    std::string m_TextMeshKey;

    bool m_Open{false};
    math::Vec2 m_PopupPos{0.0f, 0.0f};
    float m_PopupWidth{0.0f};
    int m_HoverIndex{-1};
};

} // namespace components
} // namespace lupine
