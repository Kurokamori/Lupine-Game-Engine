#include "lupine/components/PopupMenu.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/Mesh.hpp"
#include "lupine/rendering/Font.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/asset/AssetDatabase.hpp"
#include "lupine/input/InputManager.hpp"
#include "lupine/input/InputCodes.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

namespace {

constexpr float kMenuPad = 4.0f;

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
    if (s.empty()) return out;
    std::string current;
    for (char c : s) {
        if (c == '\n') { out.push_back(current); current.clear(); }
        else if (c != '\r') current.push_back(c);
    }
    out.push_back(current);
    if (out.back().empty() && s.back() == '\n') out.pop_back();
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

PopupMenu::PopupMenu()
    : UIControl("PopupMenu")
{
}

PopupMenu::PopupMenu(const std::string& name)
    : UIControl(name)
{
}

PopupMenu::~PopupMenu() {
}

void PopupMenu::DefineProperties() {
    DefineUIControlProperties(0.0f, 0.0f, "useUISpace", "Size");

    DefineProperty(PROPERTY_DEFAULT_GROUP(items, String, std::string(""), "Menu"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(itemHeight, 24.0f, 4.0f, 256.0f, 1.0f, "Menu"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(minWidth, 120.0f, 0.0f, 2000.0f, 1.0f, "Menu"));

    DefineProperty(PROPERTY_FILE_GROUP(fontPath, std::string(""), "*.ttf,*.otf", "Text"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(fontSize, 16.0f, 1.0f, 128.0f, 1.0f, "Text"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(fontColor, Color, Color::White(), "Appearance"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(backgroundColor, Color, Color(0.15f, 0.15f, 0.17f, 0.98f), "Appearance"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderColor, Color, Color(0.4f, 0.4f, 0.4f, 1.0f), "Appearance"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(hoverColor, Color, Color(0.3f, 0.4f, 0.6f, 1.0f), "Appearance"));
}

void PopupMenu::DefineSignals() {
    RegisterSignal({"item_selected", {{"index", core::PropertyValueType::Int}}, "Emitted when a menu item is chosen."});
    RegisterSignal({"popup_closed", {}, "Emitted when the menu closes."});
}

// ========================================
// Items
// ========================================

std::vector<std::string> PopupMenu::ParseItems() const { return SplitLines(GetPropertyValue<std::string>("items")); }
void PopupMenu::WriteItems(const std::vector<std::string>& items) { SetPropertyValue<std::string>("items", JoinLines(items)); }

int PopupMenu::GetItemCount() const { return static_cast<int>(ParseItems().size()); }
std::string PopupMenu::GetItem(int index) const {
    std::vector<std::string> items = ParseItems();
    return (index >= 0 && index < static_cast<int>(items.size())) ? items[index] : std::string();
}
void PopupMenu::AddItem(const std::string& text) { auto items = ParseItems(); items.push_back(text); WriteItems(items); }
void PopupMenu::RemoveItem(int index) {
    auto items = ParseItems();
    if (index >= 0 && index < static_cast<int>(items.size())) { items.erase(items.begin() + index); WriteItems(items); }
}
void PopupMenu::ClearItems() { WriteItems({}); }
void PopupMenu::SetItems(const std::vector<std::string>& items) { WriteItems(items); }

// ========================================
// Open / close
// ========================================

void PopupMenu::Popup() {
    PopupAt(GetCenter());
}

void PopupMenu::PopupAt(const Vec2& position) {
    m_PopupPos = position;
    m_Open = true;
    GrabFocus();
}

void PopupMenu::Close() {
    if (m_Open) {
        m_Open = false;
        if (HasFocus()) ReleaseFocus();
        Emit("popup_closed");
    }
}

// ========================================
// Appearance accessors
// ========================================

float PopupMenu::GetItemHeight() const { return GetPropertyValue<float>("itemHeight"); }
void PopupMenu::SetItemHeight(float height) { SetPropertyValue<float>("itemHeight", height); }
std::string PopupMenu::GetFontPath() const { return ResolveThemedFontPath("fontPath", "font"); }
void PopupMenu::SetFontPath(const std::string& path) { SetThemedProperty<std::string>("fontPath", path); }
float PopupMenu::GetFontSize() const { return ResolveThemedFontSize("fontSize", "font_size", "font"); }
void PopupMenu::SetFontSize(float size) { SetThemedProperty<float>("fontSize", size); }

const std::vector<UIControl::ThemeBinding>& PopupMenu::GetThemeBindings() const {
    static const std::vector<ThemeBinding> kBindings = {
        { "fontColor",       "font_color",   ThemeBinding::Kind::Color },
        { "backgroundColor", "background",   ThemeBinding::Kind::Color },
        { "borderColor",     "border_color", ThemeBinding::Kind::Color },
        { "hoverColor",      "hover_color",  ThemeBinding::Kind::Color },
        { "fontPath",        "font",         ThemeBinding::Kind::Font },
        { "fontSize",        "font_size",    ThemeBinding::Kind::Constant }
    };
    return kBindings;
}

Color PopupMenu::GetFontColor() const {
    return ResolveThemedColor("fontColor", "font_color");
}
void PopupMenu::SetFontColor(const Color& color) { SetThemedProperty<Color>("fontColor", color); }
Color PopupMenu::GetBackgroundColor() const {
    return ResolveThemedColor("backgroundColor", "background");
}
void PopupMenu::SetBackgroundColor(const Color& color) { SetThemedProperty<Color>("backgroundColor", color); }
Color PopupMenu::GetHoverColor() const {
    return ResolveThemedColor("hoverColor", "hover_color");
}
void PopupMenu::SetHoverColor(const Color& color) { SetThemedProperty<Color>("hoverColor", color); }

// ========================================
// Geometry
// ========================================

Vec2 PopupMenu::GetCenter() const {
    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    return node2D ? node2D->GetGlobalPosition() : Vec2(0.0f, 0.0f);
}

Rect PopupMenu::ComputeMenuRect() const {
    const int count = GetItemCount();
    const float itemH = GetItemHeight();
    const float width = std::max(m_PopupWidth, GetPropertyValue<float>("minWidth"));
    const float height = static_cast<float>(count) * itemH + kMenuPad * 2.0f;
    return Rect(m_PopupPos, Vec2(width, height));
}

int PopupMenu::ItemIndexFromMouse(const Vec2& mouse) const {
    Rect menu = ComputeMenuRect();
    if (mouse.x < menu.position.x || mouse.x > menu.position.x + menu.size.x ||
        mouse.y < menu.position.y || mouse.y > menu.position.y + menu.size.y) {
        return -1;
    }
    const float itemH = std::max(1.0f, GetItemHeight());
    int idx = static_cast<int>(std::floor(((menu.position.y + menu.size.y - kMenuPad) - mouse.y) / itemH));
    if (idx < 0 || idx >= GetItemCount()) return -1;
    return idx;
}

// ========================================
// Font lifecycle
// ========================================

void PopupMenu::EnsureFont(RenderContext& ctx) {
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

bool PopupMenu::OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) {
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
// Input
// ========================================

void PopupMenu::OnInput(float deltaTime) {
    (void)deltaTime;
    if (!IsEnabled() || !m_Open) {
        return;
    }
    input::InputManager& inputMgr = input::InputManager::Get();
    Vec2 mouse = GetCanvasMousePosition();

    m_HoverIndex = ItemIndexFromMouse(mouse);

    if (inputMgr.IsKeyJustPressed(input::KeyCode::Escape)) {
        Close();
        return;
    }

    if (inputMgr.IsMouseButtonJustPressed(input::MouseButton::Left)) {
        Rect menu = ComputeMenuRect();
        bool inside = menu.Contains(mouse);
        if (inside) {
            int idx = ItemIndexFromMouse(mouse);
            if (idx >= 0) {
                Emit("item_selected", { idx });
                Close();
            }
        } else {
            Close();
        }
    }
}

// ========================================
// Rendering
// ========================================

void PopupMenu::buildDrawCommands(RenderContext& ctx) {
    if (!m_Owner || !m_Open) {
        return;
    }

    // The cached item-text mesh bakes in the font colour, but its cache key does
    // not; rebuild it when the theme/palette changes. Consumed only while open so a
    // theme change made while the menu is closed is still applied on the next popup.
    if (ConsumeThemeVersionChanged()) {
        m_TextMeshKey.clear();
    }

    // A resolution change re-bakes the font atlas at a new oversampling density,
    // which repacks glyph UVs; regenerate the cached text mesh so it matches.
    if (ConsumeFontOversampleChanged()) {
        m_TextMeshKey.clear();
    }

    EnsureFont(ctx);
    const FontAtlas* atlas = m_FontHandle.isValid() ? ctx.getDevice()->getFontAtlas(m_FontHandle) : nullptr;
    if (!atlas) {
        return;
    }

    const float fontSize = GetFontSize();
    std::vector<std::string> items = ParseItems();
    const int count = static_cast<int>(items.size());

    // Resolve menu width from the widest item.
    float maxW = 0.0f;
    for (const std::string& it : items) {
        maxW = std::max(maxW, TextLayout::MeasureLineWidth(it, *atlas, fontSize));
    }
    m_PopupWidth = std::max(GetPropertyValue<float>("minWidth"), maxW + 24.0f);

    Rect menu = ComputeMenuRect();
    const float itemH = GetItemHeight();

    // Background + border.
    ctx.drawRoundedRect(menu.position - Vec2(1.0f, 1.0f), menu.size + Vec2(2.0f, 2.0f), 4.0f,
                        ResolveThemedColor("borderColor", "border_color"), 0);
    ctx.drawRoundedRect(menu.position, menu.size, 4.0f, GetBackgroundColor(), 0);

    ctx.pushClipRect(menu.position, menu.size);

    const float boxLeftX = menu.position.x + 8.0f;
    // Y-up canvas: anchor the first item at the menu top and stack downward.
    const float boxTopY = menu.position.y + menu.size.y - kMenuPad;

    // Hover highlight.
    if (m_HoverIndex >= 0 && m_HoverIndex < count) {
        const float rowTop = boxTopY - static_cast<float>(m_HoverIndex + 1) * itemH;
        ctx.drawRoundedRect(Vec2(menu.position.x, rowTop), Vec2(menu.size.x, itemH), 0.0f, GetHoverColor(), 0);
    }

    // Item text (cached combined mesh).
    {
        char keyBuf[128];
        std::snprintf(keyBuf, sizeof(keyBuf), "%.1f|%.1f|%.1f|%.1f", boxLeftX, boxTopY, fontSize, itemH);
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
                // The layout origin is the row band's TOP edge on the Y-up canvas.
                const float rowTopY = boxTopY - static_cast<float>(i) * itemH;
                for (const TextGlyphQuad& q : layout.quads) {
                    for (int k = 0; k < 4; ++k) {
                        Vertex vtx;
                        vtx.position = Vec3(boxLeftX + q.pos[k].x, rowTopY + q.pos[k].y, 0.0f);
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

AABB PopupMenu::getWorldBounds() const {
    if (!m_Open) {
        Vec2 c = GetCenter();
        return AABB(Vec3(c.x, c.y, -0.1f), Vec3(c.x, c.y, 0.1f));
    }
    Rect menu = ComputeMenuRect();
    return AABB(Vec3(menu.position.x, menu.position.y, -0.1f),
                Vec3(menu.position.x + menu.size.x, menu.position.y + menu.size.y, 0.1f));
}

RenderLayer PopupMenu::getRenderLayer() const {
    return RenderLayer::Transparent;
}

} // namespace components
} // namespace lupine
