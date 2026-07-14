#include "lupine/components/ItemList.hpp"
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

uint32_t NextAtlasSize(float fontSize) {
    uint32_t minAtlasSize = static_cast<uint32_t>(fontSize * 10.0f);
    uint32_t atlasSize = 512;
    while (atlasSize < minAtlasSize && atlasSize < 4096) {
        atlasSize *= 2;
    }
    return atlasSize;
}

std::vector<std::string> SplitLines(const std::string& s) {
    std::vector<std::string> out;
    if (s.empty()) {
        return out;
    }
    std::string current;
    for (char c : s) {
        if (c == '\n') {
            out.push_back(current);
            current.clear();
        } else if (c != '\r') {
            current.push_back(c);
        }
    }
    out.push_back(current);
    // A single trailing newline terminates the list rather than adding an empty item.
    if (out.back().empty() && s.back() == '\n') {
        out.pop_back();
    }
    return out;
}

std::string JoinLines(const std::vector<std::string>& items) {
    std::string out;
    for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0) out.push_back('\n');
        out += items[i];
    }
    return out;
}

} // anonymous namespace

ItemList::ItemList()
    : UIControl("ItemList")
{
}

ItemList::ItemList(const std::string& name)
    : UIControl(name)
{
}

ItemList::~ItemList() {
}

void ItemList::DefineProperties() {
    DefineUIControlProperties(200.0f, 200.0f, "useUISpace", "Size");

    DefineProperty(PROPERTY_DEFAULT_GROUP(items, String, std::string(""), "Items"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(selectedIndex, -1, -1, 1000000, 1, "Items"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(itemHeight, 24.0f, 4.0f, 256.0f, 1.0f, "Items"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(scrollSpeed, 30.0f, 1.0f, 500.0f, 1.0f, "Items"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(padding, 4.0f, 0.0f, 64.0f, 1.0f, "Items"));

    DefineProperty(PROPERTY_FILE_GROUP(fontPath, std::string(""), "*.ttf,*.otf", "Text"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(fontSize, 16.0f, 1.0f, 128.0f, 1.0f, "Text"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(fontColor, Color, Color::White(), "Appearance"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(backgroundColor, Color, Color(0.12f, 0.12f, 0.12f, 1.0f), "Appearance"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderColor, Color, Color(0.4f, 0.4f, 0.4f, 1.0f), "Appearance"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(borderWidth, 1.0f, 0.0f, 16.0f, 1.0f, "Appearance"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(selectionColor, Color, Color(0.2f, 0.4f, 0.8f, 0.8f), "Appearance"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(hoverColor, Color, Color(1.0f, 1.0f, 1.0f, 0.1f), "Appearance"));
}

void ItemList::DefineSignals() {
    RegisterSignal({"item_selected",
                    {{"index", core::PropertyValueType::Int}},
                    "Emitted when an item is selected."});
    RegisterSignal({"item_activated",
                    {{"index", core::PropertyValueType::Int}},
                    "Emitted when an item is double-clicked."});
}

void ItemList::OnAwake() {
}

// ========================================
// Items
// ========================================

std::vector<std::string> ItemList::ParseItems() const {
    return SplitLines(GetPropertyValue<std::string>("items"));
}

void ItemList::WriteItems(const std::vector<std::string>& items) {
    SetPropertyValue<std::string>("items", JoinLines(items));
}

int ItemList::GetItemCount() const {
    return static_cast<int>(ParseItems().size());
}

std::string ItemList::GetItem(int index) const {
    std::vector<std::string> items = ParseItems();
    if (index >= 0 && index < static_cast<int>(items.size())) {
        return items[index];
    }
    return std::string();
}

void ItemList::AddItem(const std::string& text) {
    std::vector<std::string> items = ParseItems();
    items.push_back(text);
    WriteItems(items);
}

void ItemList::RemoveItem(int index) {
    std::vector<std::string> items = ParseItems();
    if (index >= 0 && index < static_cast<int>(items.size())) {
        items.erase(items.begin() + index);
        WriteItems(items);
        int sel = GetSelectedIndex();
        if (sel == index) SetSelectedIndex(-1);
        else if (sel > index) SetSelectedIndex(sel - 1);
    }
}

void ItemList::ClearItems() {
    WriteItems({});
    SetSelectedIndex(-1);
}

void ItemList::SetItems(const std::vector<std::string>& items) {
    WriteItems(items);
    SetSelectedIndex(-1);
}

std::vector<std::string> ItemList::GetItems() const {
    return ParseItems();
}

int ItemList::GetSelectedIndex() const {
    return GetPropertyValue<int>("selectedIndex");
}

void ItemList::SetSelectedIndex(int index) {
    const int count = GetItemCount();
    int clamped = (index < 0 || count == 0) ? -1 : std::clamp(index, 0, count - 1);
    SetPropertyValue<int>("selectedIndex", clamped);
}

// ========================================
// Appearance accessors
// ========================================

float ItemList::GetItemHeight() const { return GetPropertyValue<float>("itemHeight"); }
void ItemList::SetItemHeight(float height) { SetPropertyValue<float>("itemHeight", height); }
std::string ItemList::GetFontPath() const { return ResolveThemedFontPath("fontPath", "font"); }
void ItemList::SetFontPath(const std::string& path) { SetThemedProperty<std::string>("fontPath", path); }
float ItemList::GetFontSize() const { return ResolveThemedFontSize("fontSize", "font_size", "font"); }
void ItemList::SetFontSize(float size) { SetThemedProperty<float>("fontSize", size); }

const std::vector<UIControl::ThemeBinding>& ItemList::GetThemeBindings() const {
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

Color ItemList::GetFontColor() const {
    return ResolveThemedColor("fontColor", "font_color");
}
void ItemList::SetFontColor(const Color& color) { SetThemedProperty<Color>("fontColor", color); }
Color ItemList::GetBackgroundColor() const {
    return ResolveThemedColor("backgroundColor", "background");
}
void ItemList::SetBackgroundColor(const Color& color) { SetThemedProperty<Color>("backgroundColor", color); }
Color ItemList::GetSelectionColor() const {
    return ResolveThemedColor("selectionColor", "selection_color");
}
void ItemList::SetSelectionColor(const Color& color) { SetThemedProperty<Color>("selectionColor", color); }
Color ItemList::GetHoverColor() const {
    return ResolveThemedColor("hoverColor", "hover_color");
}
void ItemList::SetHoverColor(const Color& color) { SetThemedProperty<Color>("hoverColor", color); }

// ========================================
// Geometry
// ========================================

Vec2 ItemList::GetCenter() const {
    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    return node2D ? node2D->GetGlobalPosition() : Vec2(0.0f, 0.0f);
}

Rect ItemList::GetInnerRect() const {
    Vec2 center = GetCenter();
    Vec2 size = GetSize();
    const float pad = GetPropertyValue<float>("padding");
    Vec2 topLeft = Vec2(center.x - size.x * 0.5f + pad, center.y - size.y * 0.5f + pad);
    Vec2 innerSize = Vec2(std::max(0.0f, size.x - pad * 2.0f), std::max(0.0f, size.y - pad * 2.0f));
    return Rect(topLeft, innerSize);
}

int ItemList::ItemIndexFromMouse(const Vec2& mouse) const {
    Rect inner = GetInnerRect();
    if (mouse.x < inner.position.x || mouse.x > inner.position.x + inner.size.x ||
        mouse.y < inner.position.y || mouse.y > inner.position.y + inner.size.y) {
        return -1;
    }
    const float itemH = std::max(1.0f, GetItemHeight());
    // Y-up canvas: rows stack downward from the inner-rect top.
    const float localY = ((inner.position.y + inner.size.y + m_ScrollY) - mouse.y);
    int idx = static_cast<int>(std::floor(localY / itemH));
    if (idx < 0 || idx >= GetItemCount()) return -1;
    return idx;
}

void ItemList::ClampScroll() {
    Rect inner = GetInnerRect();
    const float total = static_cast<float>(GetItemCount()) * GetItemHeight();
    m_ScrollY = std::clamp(m_ScrollY, 0.0f, std::max(0.0f, total - inner.size.y));
}

// ========================================
// Font lifecycle
// ========================================

void ItemList::EnsureFont(RenderContext& ctx) {
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

bool ItemList::OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) {
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
// Update / input
// ========================================

void ItemList::OnUpdate(float deltaTime) {
    UIControl::OnUpdate(deltaTime);
    m_Time += deltaTime;
}

void ItemList::OnInput(float deltaTime) {
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

    m_HoverIndex = overControl ? ItemIndexFromMouse(mouse) : -1;

    if (overControl) {
        glm::vec2 wheel = inputMgr.GetMouseScrollDelta();
        if (wheel.y != 0.0f) {
            m_ScrollY -= wheel.y * GetPropertyValue<float>("scrollSpeed");
            ClampScroll();
        }
    }

    if (inputMgr.IsMouseButtonJustPressed(input::MouseButton::Left) && overControl) {
        int idx = ItemIndexFromMouse(mouse);
        if (idx >= 0) {
            SetSelectedIndex(idx);
            Emit("item_selected", { idx });

            if (idx == m_LastClickIndex && (m_Time - m_LastClickTime) < 0.35f) {
                Emit("item_activated", { idx });
            }
            m_LastClickIndex = idx;
            m_LastClickTime = m_Time;
        }
    }
}

// ========================================
// Rendering
// ========================================

void ItemList::buildDrawCommands(RenderContext& ctx) {
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
    Color borderColor = ResolveThemedColor("borderColor", "border_color");
    if (borderWidth > 0.0f) {
        Vec2 bTopLeft = topLeft - Vec2(borderWidth, borderWidth);
        Vec2 bSize = size + Vec2(borderWidth * 2.0f, borderWidth * 2.0f);
        ctx.drawRoundedRect(bTopLeft, bSize, 0.0f, borderColor, 0);
    }
    ctx.drawRoundedRect(topLeft, size, 0.0f, GetBackgroundColor(), 0);

    const FontAtlas* atlas = m_FontHandle.isValid() ? ctx.getDevice()->getFontAtlas(m_FontHandle) : nullptr;
    if (!atlas) {
        return;
    }
    m_LastAtlas = atlas;

    ClampScroll();

    Rect inner = GetInnerRect();
    const float fontSize = GetFontSize();
    const float itemH = GetItemHeight();
    const float boxLeftX = inner.position.x;
    // Y-up canvas: anchor the first row at the inner-rect top and stack downward.
    const float boxTopY = inner.position.y + inner.size.y + m_ScrollY;

    std::vector<std::string> items = ParseItems();
    const int count = static_cast<int>(items.size());
    const int selected = GetSelectedIndex();

    ctx.pushClipRect(inner.position, inner.size);

    // Row highlights (selection + hover).
    for (int i = 0; i < count; ++i) {
        const float rowTop = boxTopY - static_cast<float>(i + 1) * itemH;
        if (rowTop + itemH < inner.position.y || rowTop > inner.position.y + inner.size.y) {
            continue; // off-screen
        }
        if (i == selected) {
            ctx.drawRoundedRect(Vec2(inner.position.x, rowTop), Vec2(inner.size.x, itemH), 0.0f, GetSelectionColor(), 0);
        } else if (i == m_HoverIndex) {
            ctx.drawRoundedRect(Vec2(inner.position.x, rowTop), Vec2(inner.size.x, itemH), 0.0f, GetHoverColor(), 0);
        }
    }

    // Combined item-text mesh (cached).
    {
        char keyBuf[128];
        std::snprintf(keyBuf, sizeof(keyBuf), "%.1f|%.1f|%.1f|%.1f|%d",
                      boxLeftX, boxTopY, fontSize, itemH, selected);
        std::string key = GetPropertyValue<std::string>("items") + "\x1f" + keyBuf;

        if (key != m_TextMeshKey) {
            if (m_TextMesh.isValid()) {
                ctx.getDevice()->destroyMesh(m_TextMesh);
                m_TextMesh = MeshHandle();
            }
            MeshData md;
            uint32_t base = 0;
            Color fontColor = GetFontColor();
            for (int i = 0; i < count; ++i) {
                TextLayoutParams params;
                params.fontSize = fontSize;
                params.color = fontColor;
                params.hAlign = TextHAlign::Left;
                // Center each item's text within its row band.
                params.vAlign = TextVAlign::Center;
                params.boxHeight = itemH;
                params.multiline = false;
                TextLayoutResult layout = TextLayout::Layout(items[i], *atlas, params);
                // rowTop is the row band's min-Y corner; the layout origin is its top.
                const float rowTop = boxTopY - static_cast<float>(i + 1) * itemH;
                const float rowTopY = rowTop + itemH;

                for (const TextGlyphQuad& q : layout.quads) {
                    for (int k = 0; k < 4; ++k) {
                        Vertex vtx;
                        vtx.position = Vec3(boxLeftX + 4.0f + q.pos[k].x, rowTopY + q.pos[k].y, 0.0f);
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

AABB ItemList::getWorldBounds() const {
    Vec2 center = GetCenter();
    Vec2 half = GetSize() * 0.5f;
    return AABB(Vec3(center.x - half.x, center.y - half.y, -0.1f),
                Vec3(center.x + half.x, center.y + half.y, 0.1f));
}

RenderLayer ItemList::getRenderLayer() const {
    return RenderLayer::Transparent;
}

Vec2 ItemList::GetContentMinSize() const {
    const float pad = GetPropertyValue<float>("padding");
    return Vec2(GetFontSize() * 4.0f + pad * 2.0f, GetItemHeight() * 3.0f + pad * 2.0f);
}

} // namespace components
} // namespace lupine
