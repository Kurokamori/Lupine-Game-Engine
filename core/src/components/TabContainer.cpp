#include "lupine/components/TabContainer.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/Mesh.hpp"
#include "lupine/rendering/Font.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/asset/AssetDatabase.hpp"
#include "lupine/input/InputManager.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

namespace {
constexpr float kTabPadding = 12.0f;

uint32_t NextAtlasSize(float fontSize) {
    uint32_t minAtlasSize = static_cast<uint32_t>(fontSize * 10.0f);
    uint32_t atlasSize = 512;
    while (atlasSize < minAtlasSize && atlasSize < 4096) {
        atlasSize *= 2;
    }
    return atlasSize;
}
} // anonymous namespace

TabContainer::TabContainer()
    : Container("TabContainer")
{
}

TabContainer::TabContainer(const std::string& name)
    : Container(name)
{
}

TabContainer::~TabContainer() {
}

void TabContainer::DefineProperties() {
    Container::DefineProperties();

    DefineProperty(PROPERTY_INT_RANGE_GROUP(currentTab, 0, 0, 1000, 1, "Tabs"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(tabHeight, 28.0f, 8.0f, 128.0f, 1.0f, "Tabs"));
    DefineProperty(PROPERTY_FILE_GROUP(fontPath, std::string(""), "*.ttf,*.otf", "Tabs"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(fontSize, 16.0f, 1.0f, 128.0f, 1.0f, "Tabs"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(fontColor, Color, Color::White(), "Tabs"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(tabColor, Color, Color(0.2f, 0.2f, 0.2f, 1.0f), "Tabs"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(activeTabColor, Color, Color(0.35f, 0.35f, 0.4f, 1.0f), "Tabs"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(tabBarColor, Color, Color(0.12f, 0.12f, 0.12f, 1.0f), "Tabs"));
}

void TabContainer::DefineSignals() {
    RegisterSignal({"tab_changed",
                    {{"index", core::PropertyValueType::Int}},
                    "Emitted when the active tab changes."});
}

void TabContainer::OnAwake() {
    Container::OnAwake();
    InvalidateLayout();
}

// ========================================
// Accessors
// ========================================

int TabContainer::GetCurrentTab() const { return GetPropertyValue<int>("currentTab"); }

void TabContainer::SetCurrentTab(int index) {
    const int count = GetTabCount();
    int clamped = (count > 0) ? std::clamp(index, 0, count - 1) : 0;
    if (clamped != GetCurrentTab()) {
        SetPropertyValue<int>("currentTab", clamped);
        InvalidateLayout();
        Emit("tab_changed", { clamped });
    }
}

int TabContainer::GetTabCount() const {
    return static_cast<int>(GetTabChildren().size());
}

std::string TabContainer::GetTabTitle(int index) const {
    std::vector<Node*> tabs = GetTabChildren();
    if (index >= 0 && index < static_cast<int>(tabs.size()) && tabs[index]) {
        return tabs[index]->GetName();
    }
    return std::string();
}

float TabContainer::GetTabHeight() const { return GetPropertyValue<float>("tabHeight"); }
void TabContainer::SetTabHeight(float height) { SetPropertyValue<float>("tabHeight", height); InvalidateLayout(); }
std::string TabContainer::GetFontPath() const { return ResolveThemedFontPath("fontPath", "font"); }
void TabContainer::SetFontPath(const std::string& path) { SetThemedProperty<std::string>("fontPath", path); }
float TabContainer::GetFontSize() const { return ResolveThemedFontSize("fontSize", "font_size", "font"); }
void TabContainer::SetFontSize(float size) { SetThemedProperty<float>("fontSize", size); }

const std::vector<UIControl::ThemeBinding>& TabContainer::GetThemeBindings() const {
    // Includes the inherited Container bindings (so background/border/corner-radius
    // stay themeable for TabContainer) plus the tab-bar-specific entries.
    static const std::vector<ThemeBinding> kBindings = {
        { "backgroundColor", "background",         ThemeBinding::Kind::Color },
        { "borderColor",     "border_color",       ThemeBinding::Kind::Color },
        { "cornerRadius",    "corner_radius",      ThemeBinding::Kind::Constant },
        { "fontColor",       "font_color",         ThemeBinding::Kind::Color },
        { "tabColor",        "tab_color",          ThemeBinding::Kind::Color },
        { "activeTabColor",  "tab_selected_color", ThemeBinding::Kind::Color },
        { "tabBarColor",     "tab_bar_color",      ThemeBinding::Kind::Color },
        { "fontPath",        "font",               ThemeBinding::Kind::Font },
        { "fontSize",        "font_size",          ThemeBinding::Kind::Constant }
    };
    return kBindings;
}

Color TabContainer::GetFontColor() const {
    return ResolveThemedColor("fontColor", "font_color");
}
void TabContainer::SetFontColor(const Color& color) { SetThemedProperty<Color>("fontColor", color); }
Color TabContainer::GetTabColor() const {
    return ResolveThemedColor("tabColor", "tab_color");
}
void TabContainer::SetTabColor(const Color& color) { SetThemedProperty<Color>("tabColor", color); }
Color TabContainer::GetActiveTabColor() const {
    return ResolveThemedColor("activeTabColor", "tab_selected_color");
}
void TabContainer::SetActiveTabColor(const Color& color) { SetThemedProperty<Color>("activeTabColor", color); }
Color TabContainer::GetTabBarColor() const {
    return ResolveThemedColor("tabBarColor", "tab_bar_color");
}
void TabContainer::SetTabBarColor(const Color& color) { SetThemedProperty<Color>("tabBarColor", color); }

// ========================================
// Geometry
// ========================================

std::vector<Node*> TabContainer::GetTabChildren() const {
    return GetChildren();
}

Rect TabContainer::GetOwnRect() const {
    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    const Vec2 center = node2D ? node2D->GetGlobalPosition() : Vec2(0.0f, 0.0f);

    // The resolved (arranged) size, so the bar and pages track where the container is
    // actually drawn when it is anchored or driven by a parent container. Each axis falls
    // back to the requested size independently, exactly as Container::GetContentRect does.
    Vec2 size = GetResolvedRect().size;
    if (size.x <= 0.0f || size.y <= 0.0f) {
        const Vec2 requested = GetSize();
        if (size.x <= 0.0f) { size.x = requested.x; }
        if (size.y <= 0.0f) { size.y = requested.y; }
    }

    return Rect(center.x - size.x * 0.5f, center.y - size.y * 0.5f, size.x, size.y);
}

Rect TabContainer::GetTabBarRect() const {
    const Rect own = GetOwnRect();
    const float tabHeight = GetTabHeight();

    // The bar spans the TOP tabHeight pixels. It used to be built as
    // Rect(center - size*0.5, {width, tabHeight}) -- and the minimum corner of a Y-up rect
    // is the BOTTOM-left, so the bar was laid along the bottom edge of the container while
    // everything else assumed it was at the top.
    return Rect::FromEdges(
        own.GetLeftEdge(),
        own.GetTopEdge(),
        own.GetRightEdge(),
        own.GetTopEdge() - tabHeight
    );
}

Rect TabContainer::GetContentArea() const {
    const Rect own = GetOwnRect();
    const float tabHeight = GetTabHeight();

    // Everything below the bar, inset by the padding. FromEdges clamps to zero extent rather
    // than going negative when the bar plus the padding exceeds the container's height.
    return Rect::FromEdges(
        own.GetLeftEdge() + GetPaddingLeft(),
        own.GetTopEdge() - tabHeight - GetPaddingTop(),
        own.GetRightEdge() - GetPaddingRight(),
        own.GetBottomEdge() + GetPaddingBottom()
    );
}

Rect TabContainer::GetClipRect() const {
    return GetContentArea();
}

Rect TabContainer::GetTabRect(int index, const FontAtlas& atlas) const {
    Rect tabBar = GetTabBarRect();
    const float fontSize = GetFontSize();
    std::vector<Node*> tabs = GetTabChildren();

    float x = tabBar.position.x;
    for (int i = 0; i < static_cast<int>(tabs.size()); ++i) {
        std::string title = tabs[i] ? tabs[i]->GetName() : std::string();
        const float w = TextLayout::MeasureLineWidth(title, atlas, fontSize) + kTabPadding * 2.0f;
        if (i == index) {
            return Rect(Vec2(x, tabBar.position.y), Vec2(w, tabBar.size.y));
        }
        x += w;
    }
    return Rect(Vec2(x, tabBar.position.y), Vec2(0.0f, tabBar.size.y));
}

// ========================================
// Layout
// ========================================

void TabContainer::CalculateLayout() {
    std::vector<Node*> tabs = GetTabChildren();
    if (tabs.empty()) {
        return;
    }

    const int count = static_cast<int>(tabs.size());
    const int current = std::clamp(GetCurrentTab(), 0, count - 1);

    const Rect contentArea = GetContentArea();
    for (int i = 0; i < count; ++i) {
        Node* child = tabs[i];
        if (!child) continue;

        const bool active = (i == current);
        child->SetVisible(active);
        if (active) {
            SetChildRect(child, contentArea);
        }
    }

    // We just changed which children are visible. Tell the base that this is OUR doing, so
    // it does not read it back next frame as an external edit and re-invalidate -- which
    // cost a guaranteed redundant re-layout after every tab switch.
    SyncChildListCache();
}

Vec2 TabContainer::GetMinimumSize() const {
    const Vec2 floorSize = Container::GetMinimumSize();

    // The tab bar is reserved unconditionally; the pages need room for the largest of them,
    // since any of them can become the active one.
    Vec2 page(0.0f, 0.0f);
    for (Node* child : GetTabChildren()) {
        const Vec2 childSize = GetChildSize(child);
        page.x = std::max(page.x, childSize.x);
        page.y = std::max(page.y, childSize.y);
    }

    const float minWidth = page.x + GetPaddingLeft() + GetPaddingRight();
    const float minHeight = page.y + GetTabHeight() + GetPaddingTop() + GetPaddingBottom();

    return Vec2(std::max(floorSize.x, minWidth), std::max(floorSize.y, minHeight));
}

// ========================================
// Font lifecycle
// ========================================

void TabContainer::EnsureFont(RenderContext& ctx) {
    std::string fontPath = GetFontPath();
    if (fontPath.empty()) {
        auto& assetDb = asset::AssetDatabase::GetInstance();
        if (assetDb.IsInitialized()) {
            fontPath = assetDb.GetDefaultFontPath();
        }
    }
    const float fontSize = GetFontSize();
    if (fontPath != m_CurrentFontPath || fontSize != m_CurrentFontSize) {
        m_CurrentFontPath = fontPath;
        m_CurrentFontSize = fontSize;
        m_FontHandle = FontHandle();
        m_FontAsset.Reset();
        if (!fontPath.empty()) {
            m_FontAsset = asset::AssetRef<asset::FontAsset>(new asset::FontAsset());
            const uint32_t atlasSize = NextAtlasSize(fontSize);
            if (!m_FontAsset->LoadFromFile(fontPath, fontSize, atlasSize, atlasSize)) {
                m_FontAsset.Reset();
            }
        }
    }
    if (m_FontAsset.IsValid() && m_FontAsset->IsLoaded() && !m_FontHandle.isValid()) {
        FontDesc fontDesc;
        fontDesc.fontPath = m_FontAsset->GetPhysicalPath();
        fontDesc.fontSize = fontSize;
        fontDesc.atlasWidth = m_FontAsset->GetAtlasWidth();
        fontDesc.atlasHeight = m_FontAsset->GetAtlasHeight();
        if (IGfxDevice* device = ctx.getDevice()) {
            m_FontHandle = device->createFontAtlas(fontDesc);
        }
    }
}

bool TabContainer::OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) {
    std::string fontPath = GetFontPath();
    if (fontPath.empty()) return false;
    auto& assetDb = asset::AssetDatabase::GetInstance();
    std::string resolvedFontPath;
    if (assetDb.IsInitialized()) {
        resolvedFontPath = assetDb.ResolveAsset(fontPath);
    }
    bool matches = (fontPath == changedPath) ||
                   (!resolvedFontPath.empty() && !resolvedChangedPath.empty() &&
                    resolvedFontPath == resolvedChangedPath);
    if (matches) {
        m_FontHandle = FontHandle();
        m_FontAsset.Reset();
        m_CurrentFontPath.clear();
        return true;
    }
    return false;
}

// ========================================
// Input
// ========================================

void TabContainer::OnInput(float deltaTime) {
    (void)deltaTime;
    if (!IsEnabled() || !m_LastAtlas) {
        return;
    }
    input::InputManager& inputMgr = input::InputManager::Get();
    if (!inputMgr.IsMouseButtonJustPressed(input::MouseButton::Left)) {
        return;
    }
    Vec2 mouse = GetCanvasMousePosition();
    const int count = GetTabCount();
    for (int i = 0; i < count; ++i) {
        if (GetTabRect(i, *m_LastAtlas).Contains(mouse)) {
            SetCurrentTab(i);
            break;
        }
    }
}

// ========================================
// Rendering
// ========================================

void TabContainer::buildDrawCommands(RenderContext& ctx) {
    // Base container resolves layout (hides/shows pages) and draws bg/border.
    Container::buildDrawCommands(ctx);

    // The cached tab-label mesh bakes the font colour into its vertices and its key
    // does not include the colour; force a rebuild when the theme/palette changes.
    if (ConsumeThemeVersionChanged()) {
        m_TabTextKey.clear();
    }

    // A resolution change re-bakes the font atlas at a new oversampling density,
    // which repacks glyph UVs; regenerate the cached text mesh so it matches.
    if (ConsumeFontOversampleChanged()) {
        m_TabTextKey.clear();
    }

    EnsureFont(ctx);
    const FontAtlas* atlas = m_FontHandle.isValid() ? ctx.getDevice()->getFontAtlas(m_FontHandle) : nullptr;
    if (!atlas) {
        return;
    }
    m_LastAtlas = atlas;

    Rect tabBar = GetTabBarRect();
    const int count = GetTabCount();
    const int current = std::clamp(GetCurrentTab(), 0, std::max(0, count - 1));
    const float fontSize = GetFontSize();

    // Tab bar background + per-tab buttons.
    ctx.drawRoundedRect(tabBar.position, tabBar.size, 0.0f, GetTabBarColor(), 0);

    std::vector<Node*> tabs = GetTabChildren();
    Color tabColor = GetTabColor();
    Color activeColor = GetActiveTabColor();

    ctx.pushClipRect(tabBar.position, tabBar.size);

    std::string key;
    for (int i = 0; i < count; ++i) {
        Rect tabRect = GetTabRect(i, *atlas);
        ctx.drawRoundedRect(tabRect.position, tabRect.size, 0.0f, (i == current) ? activeColor : tabColor, 0);
        key += (tabs[i] ? tabs[i]->GetName() : std::string());
        key += "\x1f";
    }

    // Combined tab-label text mesh (cached).
    {
        char keyBuf[96];
        std::snprintf(keyBuf, sizeof(keyBuf), "%.1f|%.1f|%.1f|%d", tabBar.position.x, tabBar.position.y, fontSize, current);
        key += keyBuf;

        if (key != m_TabTextKey) {
            if (m_TabTextMesh.isValid()) {
                ctx.getDevice()->destroyMesh(m_TabTextMesh);
                m_TabTextMesh = MeshHandle();
            }
            MeshData md;
            uint32_t base = 0;
            Color fontColor = GetFontColor();
            for (int i = 0; i < count; ++i) {
                std::string title = tabs[i] ? tabs[i]->GetName() : std::string();
                Rect tabRect = GetTabRect(i, *atlas);

                TextLayoutParams params;
                params.fontSize = fontSize;
                params.color = fontColor;
                params.hAlign = TextHAlign::Left;
                // Center the tab title within the tab's height.
                params.vAlign = TextVAlign::Center;
                params.boxHeight = tabRect.size.y;
                params.multiline = false;
                TextLayoutResult layout = TextLayout::Layout(title, *atlas, params);
                const float boxLeftX = tabRect.position.x + kTabPadding;
                // The layout origin is the tab's TOP edge on the Y-up canvas.
                const float boxTopY = tabRect.position.y + tabRect.size.y;

                for (const TextGlyphQuad& q : layout.quads) {
                    for (int k = 0; k < 4; ++k) {
                        Vertex vtx;
                        vtx.position = Vec3(boxLeftX + q.pos[k].x, boxTopY + q.pos[k].y, 0.0f);
                        vtx.normal = Vec3(0.0f, 0.0f, 1.0f);
                        vtx.texCoord = q.uv[k];
                        vtx.color = Vec4(q.color.r, q.color.g, q.color.b, q.color.a);
                        md.vertices.push_back(vtx);
                    }
                    md.indices.push_back(base + 0);
                    md.indices.push_back(base + 1);
                    md.indices.push_back(base + 2);
                    md.indices.push_back(base + 0);
                    md.indices.push_back(base + 2);
                    md.indices.push_back(base + 3);
                    base += 4;
                }
            }
            if (!md.vertices.empty()) {
                md.calculateBounds();
                m_TabTextMesh = ctx.getDevice()->createMesh(md);
            }
            m_TabTextKey = key;
        }

        if (m_TabTextMesh.isValid()) {
            MaterialPropertyBlock overrides;
            overrides.setTexture("u_FontAtlas", atlas->texture);
            ctx.drawMesh(m_TabTextMesh, ctx.getDefaultTextMaterial(), Mat4::Identity(), overrides);
        }
    }

    ctx.popClipRect();
}

} // namespace components
} // namespace lupine
