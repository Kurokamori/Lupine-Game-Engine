#pragma once

#include "lupine/components/Container.hpp"
#include "lupine/rendering/ResourceHandles.hpp"
#include "lupine/rendering/TextLayout.hpp"
#include "lupine/asset/FontAsset.hpp"
#include "lupine/math/Math.hpp"
#include <string>

namespace lupine {
namespace components {

/**
 * TabContainer
 *
 * A container that shows one direct-child "page" at a time, with a clickable tab
 * bar across the top. Each direct child node is a tab; its node name is the tab
 * title. The active page is resized to fill the content area below the tab bar and
 * the other pages are hidden.
 *
 * Signals:
 * - tab_changed(index): emitted when the active tab changes
 */
class TabContainer : public Container {
public:
    TabContainer();
    explicit TabContainer(const std::string& name);
    virtual ~TabContainer();

    std::string GetTypeName() const override { return "TabContainer"; }
    void DefineProperties() override;
    void DefineSignals() override;
    const std::vector<ThemeBinding>& GetThemeBindings() const override;

    void OnAwake() override;
    void OnInput(float deltaTime) override;
    bool OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) override;

    int GetCurrentTab() const;
    void SetCurrentTab(int index);
    int GetTabCount() const;
    std::string GetTabTitle(int index) const;

    float GetTabHeight() const;
    void SetTabHeight(float height);
    std::string GetFontPath() const;
    void SetFontPath(const std::string& path);
    float GetFontSize() const;
    void SetFontSize(float size);

    math::Color GetFontColor() const;
    void SetFontColor(const math::Color& color);
    math::Color GetTabColor() const;
    void SetTabColor(const math::Color& color);
    math::Color GetActiveTabColor() const;
    void SetActiveTabColor(const math::Color& color);
    math::Color GetTabBarColor() const;
    void SetTabBarColor(const math::Color& color);

    // Pages are clipped to the content area below the tab bar.
    bool ClipsDescendants() const override { return true; }
    math::Rect GetClipRect() const override;

    void buildDrawCommands(RenderContext& ctx) override;

protected:
    void CalculateLayout() override;

    /**
     * Reserve the tab bar. A TabContainer never overrode this and so reported (0,0) to a
     * parent container: nested in a VerticalContainer with verticalSizeMode = Minimum it did
     * not even reserve its own tabHeight, and the bar drew over the next sibling.
     */
    math::Vec2 GetMinimumSize() const override;

private:
    void EnsureFont(RenderContext& ctx);

    /**
     * This container's own rect (unpadded), in global coordinates. The bar and the content
     * area are both cut out of it.
     */
    math::Rect GetOwnRect() const;

    math::Rect GetTabBarRect() const;
    math::Rect GetContentArea() const;
    math::Rect GetTabRect(int index, const FontAtlas& atlas) const;

    std::vector<core::Node*> GetTabChildren() const;

    asset::AssetRef<asset::FontAsset> m_FontAsset;
    FontHandle m_FontHandle;
    std::string m_CurrentFontPath;
    float m_CurrentFontSize{16.0f};
    const FontAtlas* m_LastAtlas{nullptr};

    MeshHandle m_TabTextMesh;
    std::string m_TabTextKey;
};

} // namespace components
} // namespace lupine
