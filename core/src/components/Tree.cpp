#include "lupine/components/Tree.hpp"
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
#include <climits>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

namespace {

constexpr float kArrowWidth = 14.0f;

uint32_t NextAtlasSize(float fontSize) {
    uint32_t minAtlasSize = static_cast<uint32_t>(fontSize * 10.0f);
    uint32_t atlasSize = 512;
    while (atlasSize < minAtlasSize && atlasSize < 4096) atlasSize *= 2;
    return atlasSize;
}

} // anonymous namespace

Tree::Tree()
    : UIControl("Tree")
{
}

Tree::Tree(const std::string& name)
    : UIControl(name)
{
}

Tree::~Tree() {
}

void Tree::DefineProperties() {
    DefineUIControlProperties(220.0f, 240.0f, "useUISpace", "Size");

    DefineProperty(PROPERTY_DEFAULT_GROUP(items, String, std::string(""), "Tree"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(selectedIndex, -1, -1, 1000000, 1, "Tree"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(itemHeight, 24.0f, 4.0f, 256.0f, 1.0f, "Tree"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(indentWidth, 18.0f, 0.0f, 128.0f, 1.0f, "Tree"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(scrollSpeed, 30.0f, 1.0f, 500.0f, 1.0f, "Tree"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(padding, 4.0f, 0.0f, 64.0f, 1.0f, "Tree"));

    DefineProperty(PROPERTY_FILE_GROUP(fontPath, std::string(""), "*.ttf,*.otf", "Text"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(fontSize, 16.0f, 1.0f, 128.0f, 1.0f, "Text"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(fontColor, Color, Color::White(), "Appearance"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(backgroundColor, Color, Color(0.12f, 0.12f, 0.12f, 1.0f), "Appearance"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderColor, Color, Color(0.4f, 0.4f, 0.4f, 1.0f), "Appearance"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(borderWidth, 1.0f, 0.0f, 16.0f, 1.0f, "Appearance"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(selectionColor, Color, Color(0.2f, 0.4f, 0.8f, 0.8f), "Appearance"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(hoverColor, Color, Color(1.0f, 1.0f, 1.0f, 0.1f), "Appearance"));
}

void Tree::DefineSignals() {
    RegisterSignal({"item_selected", {{"index", core::PropertyValueType::Int}}, "Emitted when a tree item is selected."});
    RegisterSignal({"item_activated", {{"index", core::PropertyValueType::Int}}, "Emitted when a tree item is double-clicked."});
}

// ========================================
// Items
// ========================================

std::vector<Tree::Item> Tree::ParseItems() const {
    std::vector<Item> items;
    const std::string s = GetPropertyValue<std::string>("items");
    if (s.empty()) return items;

    std::string line;
    auto pushLine = [&]() {
        int depth = 0;
        size_t p = 0;
        while (p < line.size() && line[p] == '\t') { ++depth; ++p; }
        items.push_back({depth, line.substr(p)});
    };
    for (char c : s) {
        if (c == '\n') { pushLine(); line.clear(); }
        else if (c != '\r') line.push_back(c);
    }
    // Trailing line (unless the string ended with a newline -> no empty trailing item).
    if (!(s.back() == '\n')) {
        pushLine();
    } else if (!line.empty()) {
        pushLine();
    }
    return items;
}

void Tree::WriteItems(const std::vector<Item>& items) {
    std::string s;
    for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0) s.push_back('\n');
        for (int d = 0; d < items[i].depth; ++d) s.push_back('\t');
        s += items[i].text;
    }
    SetPropertyValue<std::string>("items", s);
}

int Tree::GetItemCount() const { return static_cast<int>(ParseItems().size()); }

std::string Tree::GetItemText(int index) const {
    std::vector<Item> items = ParseItems();
    return (index >= 0 && index < static_cast<int>(items.size())) ? items[index].text : std::string();
}

int Tree::GetItemDepth(int index) const {
    std::vector<Item> items = ParseItems();
    return (index >= 0 && index < static_cast<int>(items.size())) ? items[index].depth : 0;
}

void Tree::AddItem(const std::string& text, int depth) {
    std::vector<Item> items = ParseItems();
    items.push_back({std::max(0, depth), text});
    WriteItems(items);
}

void Tree::ClearItems() {
    WriteItems({});
    m_Collapsed.clear();
    SetSelectedIndex(-1);
}

int Tree::GetSelectedIndex() const { return GetPropertyValue<int>("selectedIndex"); }

void Tree::SetSelectedIndex(int index) {
    const int count = GetItemCount();
    int clamped = (index < 0 || count == 0) ? -1 : std::clamp(index, 0, count - 1);
    SetPropertyValue<int>("selectedIndex", clamped);
}

void Tree::SyncCollapsedSize(int count) const {
    if (static_cast<int>(m_Collapsed.size()) != count) {
        m_Collapsed.resize(count, 0);
    }
}

bool Tree::IsExpanded(int index) const {
    SyncCollapsedSize(GetItemCount());
    if (index < 0 || index >= static_cast<int>(m_Collapsed.size())) return true;
    return m_Collapsed[index] == 0;
}

void Tree::SetExpanded(int index, bool expanded) {
    SyncCollapsedSize(GetItemCount());
    if (index >= 0 && index < static_cast<int>(m_Collapsed.size())) {
        m_Collapsed[index] = expanded ? 0 : 1;
    }
}

void Tree::ToggleExpand(int index) {
    SetExpanded(index, !IsExpanded(index));
}

bool Tree::IsBranch(int index, const std::vector<Item>& items) const {
    return (index + 1 < static_cast<int>(items.size())) && (items[index + 1].depth > items[index].depth);
}

std::vector<int> Tree::ComputeVisibleRows(const std::vector<Item>& items) const {
    SyncCollapsedSize(static_cast<int>(items.size()));
    std::vector<int> vis;
    int hideDepth = INT_MAX;
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        const int d = items[i].depth;
        if (d > hideDepth) {
            continue;
        }
        hideDepth = INT_MAX;
        vis.push_back(i);
        if (IsBranch(i, items) && m_Collapsed[i]) {
            hideDepth = d;
        }
    }
    return vis;
}

// ========================================
// Appearance accessors
// ========================================

float Tree::GetItemHeight() const { return GetPropertyValue<float>("itemHeight"); }
void Tree::SetItemHeight(float height) { SetPropertyValue<float>("itemHeight", height); }
float Tree::GetIndentWidth() const { return GetPropertyValue<float>("indentWidth"); }
void Tree::SetIndentWidth(float width) { SetPropertyValue<float>("indentWidth", width); }
std::string Tree::GetFontPath() const { return ResolveThemedFontPath("fontPath", "font"); }
void Tree::SetFontPath(const std::string& path) { SetThemedProperty<std::string>("fontPath", path); }
float Tree::GetFontSize() const { return ResolveThemedFontSize("fontSize", "font_size", "font"); }
void Tree::SetFontSize(float size) { SetThemedProperty<float>("fontSize", size); }

const std::vector<UIControl::ThemeBinding>& Tree::GetThemeBindings() const {
    static const std::vector<ThemeBinding> kBindings = {
        { "fontColor",       "font_color",      ThemeBinding::Kind::Color },
        { "backgroundColor", "background",      ThemeBinding::Kind::Color },
        { "borderColor",     "border_color",    ThemeBinding::Kind::Color },
        { "selectionColor",  "selection_color", ThemeBinding::Kind::Color },
        { "hoverColor",      "hover_color",     ThemeBinding::Kind::Color },
        { "fontPath",        "font",            ThemeBinding::Kind::Font },
        { "fontSize",        "font_size",       ThemeBinding::Kind::Constant },
        { "borderWidth",     "border_width",    ThemeBinding::Kind::Constant }
    };
    return kBindings;
}

Color Tree::GetFontColor() const {
    return ResolveThemedColor("fontColor", "font_color");
}
void Tree::SetFontColor(const Color& color) { SetThemedProperty<Color>("fontColor", color); }
Color Tree::GetBackgroundColor() const {
    return ResolveThemedColor("backgroundColor", "background");
}
void Tree::SetBackgroundColor(const Color& color) { SetThemedProperty<Color>("backgroundColor", color); }
Color Tree::GetSelectionColor() const {
    return ResolveThemedColor("selectionColor", "selection_color");
}
void Tree::SetSelectionColor(const Color& color) { SetThemedProperty<Color>("selectionColor", color); }

// ========================================
// Geometry
// ========================================

Vec2 Tree::GetCenter() const {
    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    return node2D ? node2D->GetGlobalPosition() : Vec2(0.0f, 0.0f);
}

Rect Tree::GetInnerRect() const {
    Vec2 center = GetCenter();
    Vec2 size = GetSize();
    const float pad = GetPropertyValue<float>("padding");
    Vec2 topLeft = Vec2(center.x - size.x * 0.5f + pad, center.y - size.y * 0.5f + pad);
    Vec2 innerSize = Vec2(std::max(0.0f, size.x - pad * 2.0f), std::max(0.0f, size.y - pad * 2.0f));
    return Rect(topLeft, innerSize);
}

void Tree::ClampScroll(int visibleCount) {
    Rect inner = GetInnerRect();
    const float total = static_cast<float>(visibleCount) * GetItemHeight();
    m_ScrollY = std::clamp(m_ScrollY, 0.0f, std::max(0.0f, total - inner.size.y));
}

// ========================================
// Font lifecycle
// ========================================

void Tree::EnsureFont(RenderContext& ctx) {
    std::string fontPath = GetFontPath();
    if (fontPath.empty()) {
        auto& assetDb = asset::AssetDatabase::GetInstance();
        if (assetDb.IsInitialized()) fontPath = assetDb.GetDefaultFontPath();
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
            if (!m_FontAsset->LoadFromFile(fontPath, fontSize, atlasSize, atlasSize)) m_FontAsset.Reset();
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

bool Tree::OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) {
    std::string fontPath = GetFontPath();
    if (fontPath.empty()) return false;
    auto& assetDb = asset::AssetDatabase::GetInstance();
    std::string resolvedFontPath;
    if (assetDb.IsInitialized()) resolvedFontPath = assetDb.ResolveAsset(fontPath);
    bool matches = (fontPath == changedPath) ||
                   (!resolvedFontPath.empty() && !resolvedChangedPath.empty() && resolvedFontPath == resolvedChangedPath);
    if (matches) {
        m_FontHandle = FontHandle();
        m_FontAsset.Reset();
        m_CurrentFontPath.clear();
        return true;
    }
    return false;
}

// ========================================
// Update / input
// ========================================

void Tree::OnUpdate(float deltaTime) {
    UIControl::OnUpdate(deltaTime);
    m_Time += deltaTime;
}

void Tree::OnInput(float deltaTime) {
    (void)deltaTime;
    if (!IsEnabled()) {
        return;
    }
    input::InputManager& inputMgr = input::InputManager::Get();
    Vec2 mouse = GetCanvasMousePosition();
    Vec2 center = GetCenter();
    Vec2 half = GetSize() * 0.5f;
    bool overControl = mouse.x >= center.x - half.x && mouse.x <= center.x + half.x &&
                       mouse.y >= center.y - half.y && mouse.y <= center.y + half.y;

    std::vector<Item> items = ParseItems();
    std::vector<int> visible = ComputeVisibleRows(items);
    Rect inner = GetInnerRect();
    const float itemH = std::max(1.0f, GetItemHeight());
    const float indent = GetIndentWidth();
    const float boxTopY = inner.position.y + inner.size.y + m_ScrollY;

    int hoverRow = -1;
    if (overControl && mouse.x >= inner.position.x && mouse.x <= inner.position.x + inner.size.x) {
        int row = static_cast<int>(std::floor((boxTopY - mouse.y) / itemH));
        if (row >= 0 && row < static_cast<int>(visible.size())) hoverRow = row;
    }
    m_HoverRow = hoverRow;

    if (overControl) {
        glm::vec2 wheel = inputMgr.GetMouseScrollDelta();
        if (wheel.y != 0.0f) {
            m_ScrollY -= wheel.y * GetPropertyValue<float>("scrollSpeed");
            ClampScroll(static_cast<int>(visible.size()));
        }
    }

    if (inputMgr.IsMouseButtonJustPressed(input::MouseButton::Left) && hoverRow >= 0) {
        const int itemIdx = visible[hoverRow];
        const int depth = items[itemIdx].depth;
        const float arrowX0 = inner.position.x + static_cast<float>(depth) * indent;
        const bool branch = IsBranch(itemIdx, items);
        const bool onArrow = branch && mouse.x >= arrowX0 && mouse.x <= arrowX0 + kArrowWidth;

        if (onArrow) {
            ToggleExpand(itemIdx);
        } else {
            SetSelectedIndex(itemIdx);
            Emit("item_selected", { itemIdx });
            if (itemIdx == m_LastClickItem && (m_Time - m_LastClickTime) < 0.35f) {
                if (branch) ToggleExpand(itemIdx);
                Emit("item_activated", { itemIdx });
            }
            m_LastClickItem = itemIdx;
            m_LastClickTime = m_Time;
        }
    }
}

// ========================================
// Rendering
// ========================================

void Tree::buildDrawCommands(RenderContext& ctx) {
    if (!m_Owner) {
        return;
    }

    // The cached item-text mesh bakes in the font colour, but its cache key does
    // not; rebuild it when the theme/palette changes.
    if (ConsumeThemeVersionChanged()) {
        m_TextMeshKey.clear();
    }

    // A resolution change re-bakes the font atlas at a new oversampling density,
    // which repacks glyph UVs; regenerate the cached text mesh so it matches.
    if (ConsumeFontOversampleChanged()) {
        m_TextMeshKey.clear();
    }

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    if (!node2D) {
        return;
    }

    EnsureFont(ctx);

    Vec2 center = GetCenter();
    Vec2 size = GetSize();
    Vec2 topLeft = Vec2(center.x - size.x * 0.5f, center.y - size.y * 0.5f);

    const float borderWidth = ResolveThemedConstant("borderWidth", "border_width");
    if (borderWidth > 0.0f) {
        ctx.drawRoundedRect(topLeft - Vec2(borderWidth, borderWidth),
                            size + Vec2(borderWidth * 2.0f, borderWidth * 2.0f), 0.0f,
                            ResolveThemedColor("borderColor", "border_color"), 0);
    }
    ctx.drawRoundedRect(topLeft, size, 0.0f, GetBackgroundColor(), 0);

    const FontAtlas* atlas = m_FontHandle.isValid() ? ctx.getDevice()->getFontAtlas(m_FontHandle) : nullptr;
    if (!atlas) {
        return;
    }
    m_LastAtlas = atlas;

    std::vector<Item> items = ParseItems();
    std::vector<int> visible = ComputeVisibleRows(items);
    ClampScroll(static_cast<int>(visible.size()));

    Rect inner = GetInnerRect();
    const float fontSize = GetFontSize();
    const float itemH = GetItemHeight();
    const float indent = GetIndentWidth();
    const float boxTopY = inner.position.y + inner.size.y + m_ScrollY;
    const int selected = GetSelectedIndex();

    ctx.pushClipRect(inner.position, inner.size);

    Color arrowColor = GetFontColor();

    // Row highlights + expand/collapse arrows.
    for (int r = 0; r < static_cast<int>(visible.size()); ++r) {
        const int itemIdx = visible[r];
        const float rowTop = boxTopY - static_cast<float>(r + 1) * itemH;
        if (rowTop + itemH < inner.position.y || rowTop > inner.position.y + inner.size.y) {
            continue;
        }
        if (itemIdx == selected) {
            ctx.drawRoundedRect(Vec2(inner.position.x, rowTop), Vec2(inner.size.x, itemH), 0.0f, GetSelectionColor(), 0);
        } else if (r == m_HoverRow) {
            ctx.drawRoundedRect(Vec2(inner.position.x, rowTop), Vec2(inner.size.x, itemH), 0.0f, ResolveThemedColor("hoverColor", "hover_color"), 0);
        }

        if (IsBranch(itemIdx, items)) {
            const float ax = inner.position.x + static_cast<float>(items[itemIdx].depth) * indent + kArrowWidth * 0.5f;
            const float ay = rowTop + itemH * 0.5f;
            const bool expanded = (m_Collapsed.empty() || m_Collapsed[itemIdx] == 0);
            if (expanded) {
                // Down-pointing: stacked tapering bars.
                const float w = 8.0f, h = 5.0f;
                for (int k = 0; k < 3; ++k) {
                    float t = static_cast<float>(k) / 3.0f;
                    float bw = w * (1.0f - t);
                    float bh = h / 3.0f;
                    ctx.drawRoundedRect(Vec2(ax - bw * 0.5f, ay - h * 0.5f + k * bh), Vec2(bw, bh), 0.0f, arrowColor, 0);
                }
            } else {
                // Right-pointing: side-by-side tapering bars.
                const float w = 5.0f, h = 8.0f;
                for (int k = 0; k < 3; ++k) {
                    float t = static_cast<float>(k) / 3.0f;
                    float bh = h * (1.0f - t);
                    float bw = w / 3.0f;
                    ctx.drawRoundedRect(Vec2(ax - w * 0.5f + k * bw, ay - bh * 0.5f), Vec2(bw, bh), 0.0f, arrowColor, 0);
                }
            }
        }
    }

    // Combined item-text mesh (cached). Key captures the visible set + scroll + pos.
    {
        std::string visKey;
        visKey.reserve(visible.size() * 4);
        for (int idx : visible) { visKey += std::to_string(idx); visKey.push_back(','); }

        char keyBuf[96];
        std::snprintf(keyBuf, sizeof(keyBuf), "%.1f|%.1f|%.1f|%.1f|%.1f", inner.position.x, boxTopY, fontSize, itemH, indent);
        std::string key = GetPropertyValue<std::string>("items") + "\x1f" + visKey + "\x1f" + keyBuf;

        if (key != m_TextMeshKey) {
            if (m_TextMesh.isValid()) {
                ctx.getDevice()->destroyMesh(m_TextMesh);
                m_TextMesh = MeshHandle();
            }
            MeshData md;
            uint32_t base = 0;
            Color fontColor = GetFontColor();
            for (int r = 0; r < static_cast<int>(visible.size()); ++r) {
                const int itemIdx = visible[r];
                const std::string& itemText = items[itemIdx].text;
                const float textX = inner.position.x + static_cast<float>(items[itemIdx].depth) * indent + kArrowWidth + 2.0f;

                TextLayoutParams params;
                params.fontSize = fontSize;
                params.color = fontColor;
                params.hAlign = TextHAlign::Left;
                // Center each row's text within its band.
                params.vAlign = TextVAlign::Center;
                params.boxHeight = itemH;
                params.multiline = false;
                TextLayoutResult layout = TextLayout::Layout(itemText, *atlas, params);
                // The layout origin is the row band's TOP edge on the Y-up canvas.
                const float rowTopY = boxTopY - static_cast<float>(r) * itemH;

                for (const TextGlyphQuad& q : layout.quads) {
                    for (int k = 0; k < 4; ++k) {
                        Vertex vtx;
                        vtx.position = Vec3(textX + q.pos[k].x, rowTopY + q.pos[k].y, 0.0f);
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
                m_TextMesh = ctx.getDevice()->createMesh(md);
            }
            m_TextMeshKey = key;
        }
        if (m_TextMesh.isValid()) {
            MaterialPropertyBlock overrides;
            overrides.setTexture("u_FontAtlas", atlas->texture);
            ctx.drawMesh(m_TextMesh, ctx.getDefaultTextMaterial(), Mat4::Identity(), overrides);
        }
    }

    ctx.popClipRect();
}

AABB Tree::getWorldBounds() const {
    Vec2 center = GetCenter();
    Vec2 half = GetSize() * 0.5f;
    return AABB(Vec3(center.x - half.x, center.y - half.y, -0.1f),
                Vec3(center.x + half.x, center.y + half.y, 0.1f));
}

RenderLayer Tree::getRenderLayer() const {
    return RenderLayer::Transparent;
}

Vec2 Tree::GetContentMinSize() const {
    const float pad = GetPropertyValue<float>("padding");
    return Vec2(GetFontSize() * 5.0f + pad * 2.0f, GetItemHeight() * 4.0f + pad * 2.0f);
}

} // namespace components
} // namespace lupine
