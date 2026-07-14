#include "lupine/components/Dropdown.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/Mesh.hpp"
#include "lupine/rendering/Font.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/asset/AssetDatabase.hpp"
#include "lupine/input/InputManager.hpp"
#include "lupine/input/InputCodes.hpp"
#include "lupine/logger/Logger.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

namespace {

constexpr float kArrowAreaWidth = 22.0f;

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

// One string laid out in a band: `topLeft` is the band's TOP-left corner (max-Y on
// the Y-up canvas) and `bandHeight` the height the text is vertically centered in.
// A bandHeight <= 0 places the text top-aligned at topLeft.
struct TextRun {
    std::string text;
    Vec2 topLeft;
    float bandHeight = 0.0f;
};

// Build a text mesh for a set of strings placed in their bands.
MeshHandle BuildTextMesh(RenderContext& ctx, const FontAtlas& atlas, float fontSize,
                         const Color& color,
                         const std::vector<TextRun>& runs) {
    MeshData md;
    uint32_t base = 0;
    for (const TextRun& run : runs) {
        TextLayoutParams params;
        params.fontSize = fontSize;
        params.color = color;
        params.hAlign = TextHAlign::Left;
        params.vAlign = (run.bandHeight > 0.0f) ? TextVAlign::Center : TextVAlign::Top;
        params.boxHeight = std::max(0.0f, run.bandHeight);
        params.multiline = false;
        TextLayoutResult layout = TextLayout::Layout(run.text, atlas, params);
        for (const TextGlyphQuad& q : layout.quads) {
            for (int k = 0; k < 4; ++k) {
                Vertex vtx;
                vtx.position = Vec3(run.topLeft.x + q.pos[k].x, run.topLeft.y + q.pos[k].y, 0.0f);
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
    if (md.vertices.empty()) {
        return MeshHandle();
    }
    md.calculateBounds();
    return ctx.getDevice()->createMesh(md);
}

} // anonymous namespace

Dropdown::Dropdown()
    : UIControl("Dropdown")
{
}

Dropdown::Dropdown(const std::string& name)
    : UIControl(name)
{
}

Dropdown::~Dropdown() {
}

void Dropdown::DefineProperties() {
    DefineUIControlProperties(160.0f, 32.0f, "useUISpace", "Size");

    DefineProperty(PROPERTY_DEFAULT_GROUP(items, String, std::string(""), "Dropdown"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(selectedIndex, -1, -1, 1000000, 1, "Dropdown"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(placeholder, String, std::string("Select..."), "Dropdown"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(itemHeight, 26.0f, 4.0f, 256.0f, 1.0f, "Dropdown"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(padding, 8.0f, 0.0f, 64.0f, 1.0f, "Dropdown"));

    DefineProperty(PROPERTY_FILE_GROUP(fontPath, std::string(""), "*.ttf,*.otf", "Text"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(fontSize, 16.0f, 1.0f, 128.0f, 1.0f, "Text"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(fontColor, Color, Color::White(), "Appearance"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(backgroundColor, Color, Color(0.2f, 0.2f, 0.22f, 1.0f), "Appearance"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderColor, Color, Color(0.4f, 0.4f, 0.4f, 1.0f), "Appearance"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(borderWidth, 1.0f, 0.0f, 16.0f, 1.0f, "Appearance"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(cornerRadius, 4.0f, 0.0f, 64.0f, 1.0f, "Appearance"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(listColor, Color, Color(0.15f, 0.15f, 0.17f, 0.98f), "Appearance"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(hoverColor, Color, Color(0.3f, 0.4f, 0.6f, 1.0f), "Appearance"));
}

void Dropdown::DefineSignals() {
    RegisterSignal({"item_selected", {{"index", core::PropertyValueType::Int}}, "Emitted when an option is chosen."});
}

// ========================================
// Items
// ========================================

std::vector<std::string> Dropdown::ParseItems() const { return SplitLines(GetPropertyValue<std::string>("items")); }
void Dropdown::WriteItems(const std::vector<std::string>& items) { SetPropertyValue<std::string>("items", JoinLines(items)); }

int Dropdown::GetItemCount() const { return static_cast<int>(ParseItems().size()); }
std::string Dropdown::GetItem(int index) const {
    std::vector<std::string> items = ParseItems();
    return (index >= 0 && index < static_cast<int>(items.size())) ? items[index] : std::string();
}
void Dropdown::AddItem(const std::string& text) { auto items = ParseItems(); items.push_back(text); WriteItems(items); }
void Dropdown::RemoveItem(int index) {
    auto items = ParseItems();
    if (index >= 0 && index < static_cast<int>(items.size())) {
        items.erase(items.begin() + index);
        WriteItems(items);
        int sel = GetSelectedIndex();
        if (sel == index) SetSelectedIndex(-1);
        else if (sel > index) SetSelectedIndex(sel - 1);
    }
}
void Dropdown::ClearItems() { WriteItems({}); SetSelectedIndex(-1); }
void Dropdown::SetItems(const std::vector<std::string>& items) { WriteItems(items); SetSelectedIndex(-1); }

int Dropdown::GetSelectedIndex() const { return GetPropertyValue<int>("selectedIndex"); }
void Dropdown::SetSelectedIndex(int index) {
    const int count = GetItemCount();
    int clamped = (index < 0 || count == 0) ? -1 : std::clamp(index, 0, count - 1);
    SetPropertyValue<int>("selectedIndex", clamped);
}

std::string Dropdown::GetSelectedText() const {
    const int sel = GetSelectedIndex();
    if (sel >= 0 && sel < GetItemCount()) {
        return GetItem(sel);
    }
    return GetPlaceholder();
}

// ========================================
// Appearance accessors
// ========================================

std::string Dropdown::GetPlaceholder() const { return GetPropertyValue<std::string>("placeholder"); }
void Dropdown::SetPlaceholder(const std::string& text) { SetPropertyValue<std::string>("placeholder", text); }
float Dropdown::GetItemHeight() const { return GetPropertyValue<float>("itemHeight"); }
void Dropdown::SetItemHeight(float height) { SetPropertyValue<float>("itemHeight", height); }
std::string Dropdown::GetFontPath() const { return ResolveThemedFontPath("fontPath", "font"); }
void Dropdown::SetFontPath(const std::string& path) { SetThemedProperty<std::string>("fontPath", path); }
float Dropdown::GetFontSize() const { return ResolveThemedFontSize("fontSize", "font_size", "font"); }
void Dropdown::SetFontSize(float size) { SetThemedProperty<float>("fontSize", size); }

const std::vector<UIControl::ThemeBinding>& Dropdown::GetThemeBindings() const {
    static const std::vector<ThemeBinding> kBindings = {
        { "fontColor",       "font_color",       ThemeBinding::Kind::Color },
        { "backgroundColor", "background",       ThemeBinding::Kind::Color },
        { "borderColor",     "border_color",     ThemeBinding::Kind::Color },
        { "listColor",       "popup_background", ThemeBinding::Kind::Color },
        { "hoverColor",      "hover_color",      ThemeBinding::Kind::Color },
        { "fontPath",        "font",             ThemeBinding::Kind::Font },
        { "fontSize",        "font_size",        ThemeBinding::Kind::Constant },
        { "cornerRadius",    "corner_radius",    ThemeBinding::Kind::Constant },
        { "borderWidth",     "border_width",     ThemeBinding::Kind::Constant }
    };
    return kBindings;
}

Color Dropdown::GetFontColor() const {
    return ResolveThemedColor("fontColor", "font_color");
}
void Dropdown::SetFontColor(const Color& color) { SetThemedProperty<Color>("fontColor", color); }
Color Dropdown::GetBackgroundColor() const {
    return ResolveThemedColor("backgroundColor", "background");
}
void Dropdown::SetBackgroundColor(const Color& color) { SetThemedProperty<Color>("backgroundColor", color); }
Color Dropdown::GetListColor() const {
    return ResolveThemedColor("listColor", "popup_background");
}
void Dropdown::SetListColor(const Color& color) { SetThemedProperty<Color>("listColor", color); }
Color Dropdown::GetHoverColor() const {
    return ResolveThemedColor("hoverColor", "hover_color");
}
void Dropdown::SetHoverColor(const Color& color) { SetThemedProperty<Color>("hoverColor", color); }

// ========================================
// Geometry
// ========================================

Vec2 Dropdown::GetCenter() const {
    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    return node2D ? node2D->GetGlobalPosition() : Vec2(0.0f, 0.0f);
}

Rect Dropdown::GetButtonRect() const {
    Vec2 center = GetCenter();
    Vec2 size = GetBoundsSize();
    return Rect(Vec2(center.x - size.x * 0.5f, center.y - size.y * 0.5f), size);
}

Rect Dropdown::GetListRect() const {
    Rect button = GetButtonRect();
    const int count = GetItemCount();
    const float h = static_cast<float>(count) * GetItemHeight();
    // Y-up canvas: the options drop DOWN from the button, i.e. they occupy the band
    // just below the button's bottom edge (button.position.y is the minimum-Y corner).
    return Rect(Vec2(button.position.x, button.position.y - h), Vec2(button.size.x, h));
}

int Dropdown::ItemIndexFromMouse(const Vec2& mouse) const {
    Rect list = GetListRect();
    if (mouse.x < list.position.x || mouse.x > list.position.x + list.size.x ||
        mouse.y < list.position.y || mouse.y > list.position.y + list.size.y) {
        return -1;
    }
    const float itemH = std::max(1.0f, GetItemHeight());
    const int count = GetItemCount();
    // Y-up: item 0 is at the TOP of the list (highest Y, nearest the button), so invert
    // the row measured from the list's bottom edge.
    int rowFromBottom = static_cast<int>(std::floor((mouse.y - list.position.y) / itemH));
    int idx = count - 1 - rowFromBottom;
    if (idx < 0 || idx >= count) return -1;
    return idx;
}

// ========================================
// Font lifecycle
// ========================================

void Dropdown::EnsureFont(RenderContext& ctx) {
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

bool Dropdown::OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) {
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

void Dropdown::OnInput(float deltaTime) {
    (void)deltaTime;
    if (!IsEnabled()) {
        return;
    }
    input::InputManager& inputMgr = input::InputManager::Get();
    Vec2 mouse = GetCanvasMousePosition();
    Rect button = GetButtonRect();

    if (inputMgr.IsMouseButtonPressed(input::MouseButton::Left) ||
        inputMgr.IsMouseButtonJustPressed(input::MouseButton::Left)) {
        LOG_INPUT_INFO("[diag] Dropdown click mouse=({},{}) btnPos=({},{}) size=({},{}) inBtn={} just={} open={}",
                       mouse.x, mouse.y, button.position.x, button.position.y, button.size.x, button.size.y,
                       button.Contains(mouse), inputMgr.IsMouseButtonJustPressed(input::MouseButton::Left), m_Open);
    }

    if (m_Open) {
        m_HoverIndex = ItemIndexFromMouse(mouse);
    } else {
        m_HoverIndex = -1;
    }

    if (inputMgr.IsKeyJustPressed(input::KeyCode::Escape) && m_Open) {
        m_Open = false;
        if (HasFocus()) ReleaseFocus();
        return;
    }

    if (inputMgr.IsMouseButtonJustPressed(input::MouseButton::Left)) {
        if (button.Contains(mouse)) {
            m_Open = !m_Open;
            if (m_Open) GrabFocus();
            else if (HasFocus()) ReleaseFocus();
        } else if (m_Open) {
            int idx = ItemIndexFromMouse(mouse);
            if (idx >= 0) {
                SetSelectedIndex(idx);
                Emit("item_selected", { idx });
            }
            m_Open = false;
            if (HasFocus()) ReleaseFocus();
        }
    }
}

// ========================================
// Rendering
// ========================================

void Dropdown::buildDrawCommands(RenderContext& ctx) {
    if (!m_Owner) {
        return;
    }

    // The cached option-list text mesh bakes in the font colour, but its cache key
    // does not; rebuild it when the theme/palette changes.
    if (ConsumeThemeVersionChanged()) {
        m_ListTextKey.clear();
    }

    // A resolution change re-bakes the font atlas at a new oversampling density,
    // which repacks glyph UVs; regenerate the cached text mesh so it matches.
    if (ConsumeFontOversampleChanged()) {
        m_ButtonTextKey.clear();
        m_ListTextKey.clear();
    }

    if (!dynamic_cast<Node2D*>(m_Owner)) {
        return;
    }

    EnsureFont(ctx);

    Rect button = GetButtonRect();
    const float cornerRadius = ResolveThemedConstant("cornerRadius", "corner_radius");
    const float borderWidth = ResolveThemedConstant("borderWidth", "border_width");
    Color borderColor = ResolveThemedColor("borderColor", "border_color");

    if (borderWidth > 0.0f) {
        ctx.drawRoundedRect(button.position - Vec2(borderWidth, borderWidth),
                            button.size + Vec2(borderWidth * 2.0f, borderWidth * 2.0f),
                            cornerRadius + borderWidth, borderColor, 0);
    }
    ctx.drawRoundedRect(button.position, button.size, cornerRadius, GetBackgroundColor(), 0);

    // Down arrow (stacked tapering bars -> points toward the list).
    {
        Color arrowColor = GetFontColor();
        const float ax = button.position.x + button.size.x - kArrowAreaWidth * 0.5f;
        const float ay = button.position.y + button.size.y * 0.5f;
        const float w = 10.0f;
        const float h = 6.0f;
        const int rows = 3;
        for (int r = 0; r < rows; ++r) {
            float barW = w * static_cast<float>(r + 1) / static_cast<float>(rows);
            float barH = h / static_cast<float>(rows);
            float y = ay - h * 0.5f + static_cast<float>(r) * barH;
            ctx.drawRoundedRect(Vec2(ax - barW * 0.5f, y), Vec2(barW, barH), 0.0f, arrowColor, 0);
        }
    }

    const FontAtlas* atlas = m_FontHandle.isValid() ? ctx.getDevice()->getFontAtlas(m_FontHandle) : nullptr;
    if (!atlas) {
        return;
    }
    m_LastAtlas = atlas;

    const float fontSize = GetFontSize();
    const float pad = GetPropertyValue<float>("padding");
    Color fontColor = GetFontColor();

    // Button label (selected text or placeholder), clipped to the text area.
    {
        std::string label = GetSelectedText();
        const float boxLeftX = button.position.x + pad;
        // The button rect's top edge; BuildTextMesh centers the label within its height.
        const float boxTopY = button.position.y + button.size.y;

        char keyBuf[128];
        std::snprintf(keyBuf, sizeof(keyBuf), "%.1f|%.1f|%.1f|%.1f|%.3f|%.3f|%.3f",
                      boxLeftX, boxTopY, button.size.y, fontSize, fontColor.r, fontColor.g, fontColor.b);
        std::string key = label + "\x1f" + keyBuf;
        if (key != m_ButtonTextKey) {
            if (m_ButtonTextMesh.isValid()) { ctx.getDevice()->destroyMesh(m_ButtonTextMesh); m_ButtonTextMesh = MeshHandle(); }
            m_ButtonTextMesh = BuildTextMesh(ctx, *atlas, fontSize, fontColor,
                                             {{label, Vec2(boxLeftX, boxTopY), button.size.y}});
            m_ButtonTextKey = key;
        }
        Rect textArea(button.position, Vec2(std::max(0.0f, button.size.x - kArrowAreaWidth), button.size.y));
        ctx.pushClipRect(textArea.position, textArea.size);
        if (m_ButtonTextMesh.isValid()) {
            MaterialPropertyBlock overrides;
            overrides.setTexture("u_FontAtlas", atlas->texture);
            ctx.drawMesh(m_ButtonTextMesh, ctx.getDefaultTextMaterial(), Mat4::Identity(), overrides);
        }
        ctx.popClipRect();
    }

    // Option list (overlay) when open.
    if (m_Open) {
        std::vector<std::string> items = ParseItems();
        const int count = static_cast<int>(items.size());
        const float itemH = GetItemHeight();
        Rect list = GetListRect();
        const int selected = GetSelectedIndex();

        // Draw the open list above surrounding content: the render traversal records a
        // node's own draws before its siblings, so raise the Z-index for the overlay so
        // it is not occluded by content drawn afterward.
        const int savedListZIndex = ctx.getZIndex();
        ctx.setZIndex(savedListZIndex + 4096);

        ctx.drawRoundedRect(list.position - Vec2(1.0f, 0.0f), list.size + Vec2(2.0f, 1.0f), 0.0f, borderColor, 0);
        ctx.drawRoundedRect(list.position, list.size, 0.0f, GetListColor(), 0);

        ctx.pushClipRect(list.position, list.size);

        const float boxLeftX = list.position.x + pad;

        for (int i = 0; i < count; ++i) {
            const float rowTop = list.position.y + static_cast<float>(count - 1 - i) * itemH;
            if (i == selected) {
                ctx.drawRoundedRect(Vec2(list.position.x, rowTop), Vec2(list.size.x, itemH), 0.0f,
                                    Color(GetHoverColor().r * 0.7f, GetHoverColor().g * 0.7f, GetHoverColor().b * 0.7f, GetHoverColor().a), 0);
            } else if (i == m_HoverIndex) {
                ctx.drawRoundedRect(Vec2(list.position.x, rowTop), Vec2(list.size.x, itemH), 0.0f, GetHoverColor(), 0);
            }
        }

        char keyBuf[128];
        std::snprintf(keyBuf, sizeof(keyBuf), "%.1f|%.1f|%.1f|%.1f", boxLeftX, list.position.y, fontSize, itemH);
        std::string key = GetPropertyValue<std::string>("items") + "\x1f" + keyBuf;
        if (key != m_ListTextKey) {
            if (m_ListTextMesh.isValid()) { ctx.getDevice()->destroyMesh(m_ListTextMesh); m_ListTextMesh = MeshHandle(); }
            std::vector<TextRun> runs;
            for (int i = 0; i < count; ++i) {
                // rowTop is the row band's min-Y corner; the text is centered in the band.
                const float rowTop = list.position.y + static_cast<float>(count - 1 - i) * itemH;
                runs.push_back({items[i], Vec2(boxLeftX, rowTop + itemH), itemH});
            }
            m_ListTextMesh = BuildTextMesh(ctx, *atlas, fontSize, fontColor, runs);
            m_ListTextKey = key;
        }
        if (m_ListTextMesh.isValid()) {
            MaterialPropertyBlock overrides;
            overrides.setTexture("u_FontAtlas", atlas->texture);
            ctx.drawMesh(m_ListTextMesh, ctx.getDefaultTextMaterial(), Mat4::Identity(), overrides);
        }

        ctx.popClipRect();
        ctx.setZIndex(savedListZIndex);
    }
}

AABB Dropdown::getWorldBounds() const {
    Rect button = GetButtonRect();
    float minY = button.position.y;
    float maxY = button.position.y + button.size.y;
    if (m_Open) {
        // The open list drops below the button (toward -Y) on the Y-up canvas.
        minY -= static_cast<float>(GetItemCount()) * GetItemHeight();
    }
    return AABB(Vec3(button.position.x, minY, -0.1f),
                Vec3(button.position.x + button.size.x, maxY, 0.1f));
}

RenderLayer Dropdown::getRenderLayer() const {
    return RenderLayer::Transparent;
}

Vec2 Dropdown::GetContentMinSize() const {
    const float pad = GetPropertyValue<float>("padding");
    return Vec2(GetFontSize() * 4.0f + pad * 2.0f + kArrowAreaWidth, GetFontSize() * 1.4f + pad * 2.0f);
}

} // namespace components
} // namespace lupine
