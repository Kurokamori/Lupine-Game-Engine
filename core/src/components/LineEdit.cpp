#include "lupine/components/LineEdit.hpp"
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

void AppendUTF8(std::string& s, uint32_t cp) {
    if (cp < 0x80) {
        s.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        s.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        s.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        s.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        s.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        s.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        s.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        s.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        s.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        s.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

std::vector<uint32_t> DecodeUTF8(const std::string& str) {
    std::vector<uint32_t> out;
    out.reserve(str.size());
    const unsigned char* b = reinterpret_cast<const unsigned char*>(str.data());
    const size_t n = str.size();
    size_t i = 0;
    while (i < n) {
        unsigned char lead = b[i];
        uint32_t cp = 0;
        size_t extra = 0;
        if (lead < 0x80) { cp = lead; extra = 0; }
        else if ((lead & 0xE0) == 0xC0) { cp = lead & 0x1F; extra = 1; }
        else if ((lead & 0xF0) == 0xE0) { cp = lead & 0x0F; extra = 2; }
        else if ((lead & 0xF8) == 0xF0) { cp = lead & 0x07; extra = 3; }
        else { ++i; continue; }
        ++i;
        bool ok = true;
        for (size_t k = 0; k < extra; ++k) {
            if (i >= n || (b[i] & 0xC0) != 0x80) { ok = false; break; }
            cp = (cp << 6) | (b[i] & 0x3F);
            ++i;
        }
        if (ok) out.push_back(cp);
    }
    return out;
}

std::string EncodeUTF8(const std::vector<uint32_t>& cps, size_t start, size_t end) {
    std::string out;
    for (size_t i = start; i < end && i < cps.size(); ++i) {
        AppendUTF8(out, cps[i]);
    }
    return out;
}

uint32_t NextAtlasSize(float fontSize) {
    uint32_t minAtlasSize = static_cast<uint32_t>(fontSize * 10.0f);
    uint32_t atlasSize = 512;
    while (atlasSize < minAtlasSize && atlasSize < 4096) {
        atlasSize *= 2;
    }
    return atlasSize;
}

} // anonymous namespace

LineEdit::LineEdit()
    : UIControl("LineEdit")
{
}

LineEdit::LineEdit(const std::string& name)
    : UIControl(name)
{
}

LineEdit::~LineEdit() {
}

void LineEdit::DefineProperties() {
    DefineUIControlProperties(200.0f, 32.0f, "useUISpace", "Size");

    DefineProperty(PROPERTY_DEFAULT_GROUP(text, String, std::string(""), "Text"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(placeholder, String, std::string(""), "Text"));
    DefineProperty(PROPERTY_FILE_GROUP(fontPath, std::string(""), "*.ttf,*.otf", "Text"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(fontSize, 16.0f, 1.0f, 256.0f, 1.0f, "Text"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(editable, Bool, true, "Text"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(secret, Bool, false, "Text"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(maxLength, 0, 0, 100000, 1, "Text"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(padding, 6.0f, 0.0f, 64.0f, 1.0f, "Text"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(fontColor, Color, Color::White(), "Appearance"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(placeholderColor, Color, Color(0.6f, 0.6f, 0.6f, 1.0f), "Appearance"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(backgroundColor, Color, Color(0.12f, 0.12f, 0.12f, 1.0f), "Appearance"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderColor, Color, Color(0.4f, 0.4f, 0.4f, 1.0f), "Appearance"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(borderWidth, 1.0f, 0.0f, 16.0f, 1.0f, "Appearance"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(cornerRadius, 4.0f, 0.0f, 64.0f, 1.0f, "Appearance"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(selectionColor, Color, Color(0.2f, 0.4f, 0.8f, 0.6f), "Appearance"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(caretColor, Color, Color::White(), "Appearance"));
}

void LineEdit::DefineSignals() {
    RegisterSignal({"text_changed",
                    {{"text", core::PropertyValueType::String}},
                    "Emitted whenever the text changes."});
    RegisterSignal({"text_submitted",
                    {{"text", core::PropertyValueType::String}},
                    "Emitted when Enter is pressed."});
}

void LineEdit::OnAwake() {
    SyncTextFromProperty();
    m_Caret = static_cast<int>(m_Text.size());
}

// ========================================
// Text / property sync
// ========================================

void LineEdit::SyncTextFromProperty() {
    std::string propText = GetPropertyValue<std::string>("text");
    m_Text = DecodeUTF8(propText);
    m_CachedPropertyText = propText;
    m_Caret = std::clamp(m_Caret, 0, static_cast<int>(m_Text.size()));
    m_SelAnchor = -1;
}

void LineEdit::WriteTextToProperty() {
    std::string encoded = EncodeUTF8(m_Text, 0, m_Text.size());
    m_CachedPropertyText = encoded;
    SetPropertyValue<std::string>("text", encoded);
}

std::string LineEdit::DisplayString() const {
    if (GetSecret()) {
        return std::string(m_Text.size(), '*');
    }
    return EncodeUTF8(m_Text, 0, m_Text.size());
}

std::string LineEdit::GetText() const {
    return GetPropertyValue<std::string>("text");
}

void LineEdit::SetText(const std::string& text) {
    SetPropertyValue<std::string>("text", text);
    SyncTextFromProperty();
    m_Caret = static_cast<int>(m_Text.size());
}

std::string LineEdit::GetPlaceholder() const { return GetPropertyValue<std::string>("placeholder"); }
void LineEdit::SetPlaceholder(const std::string& text) { SetPropertyValue<std::string>("placeholder", text); }
bool LineEdit::GetEditable() const { return GetPropertyValue<bool>("editable"); }
void LineEdit::SetEditable(bool editable) { SetPropertyValue<bool>("editable", editable); }
bool LineEdit::GetSecret() const { return GetPropertyValue<bool>("secret"); }
void LineEdit::SetSecret(bool secret) { SetPropertyValue<bool>("secret", secret); }
int LineEdit::GetMaxLength() const { return GetPropertyValue<int>("maxLength"); }
void LineEdit::SetMaxLength(int maxLength) { SetPropertyValue<int>("maxLength", maxLength); }

// ========================================
// Caret / selection
// ========================================

void LineEdit::SetCaretPosition(int position) {
    m_Caret = std::clamp(position, 0, static_cast<int>(m_Text.size()));
}

void LineEdit::SelectAll() {
    m_SelAnchor = 0;
    m_Caret = static_cast<int>(m_Text.size());
}

void LineEdit::Deselect() {
    m_SelAnchor = -1;
}

bool LineEdit::HasSelection() const {
    return m_SelAnchor >= 0 && m_SelAnchor != m_Caret;
}

int LineEdit::SelectionMin() const {
    return HasSelection() ? std::min(m_SelAnchor, m_Caret) : m_Caret;
}

int LineEdit::SelectionMax() const {
    return HasSelection() ? std::max(m_SelAnchor, m_Caret) : m_Caret;
}

std::string LineEdit::GetSelectedText() const {
    if (!HasSelection()) return std::string();
    return EncodeUTF8(m_Text, static_cast<size_t>(SelectionMin()), static_cast<size_t>(SelectionMax()));
}

void LineEdit::DeleteSelection() {
    if (!HasSelection()) return;
    const int lo = SelectionMin();
    const int hi = SelectionMax();
    m_Text.erase(m_Text.begin() + lo, m_Text.begin() + hi);
    m_Caret = lo;
    m_SelAnchor = -1;
}

void LineEdit::InsertCodepoint(uint32_t codepoint) {
    if (HasSelection()) {
        DeleteSelection();
    }
    const int maxLen = GetMaxLength();
    if (maxLen > 0 && static_cast<int>(m_Text.size()) >= maxLen) {
        return;
    }
    m_Text.insert(m_Text.begin() + m_Caret, codepoint);
    ++m_Caret;
    m_SelAnchor = -1;
}

void LineEdit::InsertString(const std::string& utf8) {
    std::vector<uint32_t> cps = DecodeUTF8(utf8);
    for (uint32_t cp : cps) {
        if (cp == '\n' || cp == '\r' || cp == '\t') continue;
        InsertCodepoint(cp);
    }
}

// ========================================
// Font / appearance accessors
// ========================================

std::string LineEdit::GetFontPath() const { return ResolveThemedFontPath("fontPath", "font"); }
void LineEdit::SetFontPath(const std::string& path) { SetThemedProperty<std::string>("fontPath", path); }
float LineEdit::GetFontSize() const { return ResolveThemedFontSize("fontSize", "font_size", "font"); }
void LineEdit::SetFontSize(float size) { SetThemedProperty<float>("fontSize", size); }

const std::vector<UIControl::ThemeBinding>& LineEdit::GetThemeBindings() const {
    static const std::vector<ThemeBinding> kBindings = {
        { "fontColor",        "font_color",        ThemeBinding::Kind::Color },
        { "placeholderColor", "placeholder_color", ThemeBinding::Kind::Color },
        { "backgroundColor",  "background",        ThemeBinding::Kind::Color },
        { "borderColor",      "border_color",      ThemeBinding::Kind::Color },
        { "selectionColor",   "selection_color",   ThemeBinding::Kind::Color },
        { "caretColor",       "caret_color",       ThemeBinding::Kind::Color },
        { "fontPath",         "font",              ThemeBinding::Kind::Font },
        { "fontSize",         "font_size",         ThemeBinding::Kind::Constant },
        { "cornerRadius",     "corner_radius",     ThemeBinding::Kind::Constant },
        { "borderWidth",      "border_width",      ThemeBinding::Kind::Constant }
    };
    return kBindings;
}

Color LineEdit::GetFontColor() const {
    return ResolveThemedColor("fontColor", "font_color");
}
void LineEdit::SetFontColor(const Color& color) { SetThemedProperty<Color>("fontColor", color); }
Color LineEdit::GetPlaceholderColor() const {
    return ResolveThemedColor("placeholderColor", "placeholder_color");
}
void LineEdit::SetPlaceholderColor(const Color& color) { SetThemedProperty<Color>("placeholderColor", color); }
Color LineEdit::GetBackgroundColor() const {
    return ResolveThemedColor("backgroundColor", "background");
}
void LineEdit::SetBackgroundColor(const Color& color) { SetThemedProperty<Color>("backgroundColor", color); }
Color LineEdit::GetBorderColor() const {
    return ResolveThemedColor("borderColor", "border_color");
}
void LineEdit::SetBorderColor(const Color& color) { SetThemedProperty<Color>("borderColor", color); }
Color LineEdit::GetSelectionColor() const {
    return ResolveThemedColor("selectionColor", "selection_color");
}
void LineEdit::SetSelectionColor(const Color& color) { SetThemedProperty<Color>("selectionColor", color); }
Color LineEdit::GetCaretColor() const {
    return ResolveThemedColor("caretColor", "caret_color");
}
void LineEdit::SetCaretColor(const Color& color) { SetThemedProperty<Color>("caretColor", color); }

// ========================================
// Geometry / measurement
// ========================================

Vec2 LineEdit::GetCenter() const {
    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    return node2D ? node2D->GetGlobalPosition() : Vec2(0.0f, 0.0f);
}

Rect LineEdit::GetInnerRect() const {
    const Vec2 center = GetCenter();
    const Vec2 size = GetBoundsSize();
    const Rect box(center.x - size.x * 0.5f, center.y - size.y * 0.5f, size.x, size.y);

    // The scalar `padding` property still works exactly as before -- GetContentMargins folds
    // it in -- but the themed StyleBox's per-side content margins are now honored too, so a
    // theme can inset the text without every control needing its own padding field.
    return GetContentInsetRect(box);
}

float LineEdit::PrefixWidth(int index, const FontAtlas& atlas) const {
    return TextLayout::MeasurePrefixWidth(DisplayString(), static_cast<size_t>(std::max(0, index)),
                                          atlas, GetFontSize());
}

int LineEdit::CaretIndexFromLocalX(float localX, const FontAtlas& atlas) const {
    const std::string disp = DisplayString();
    const float fontSize = GetFontSize();
    int best = 0;
    float bestDist = std::abs(0.0f - localX);
    for (int i = 1; i <= static_cast<int>(m_Text.size()); ++i) {
        float w = TextLayout::MeasurePrefixWidth(disp, static_cast<size_t>(i), atlas, fontSize);
        float d = std::abs(w - localX);
        if (d < bestDist) {
            bestDist = d;
            best = i;
        }
    }
    return best;
}

void LineEdit::UpdateScroll(const FontAtlas& atlas) {
    Rect inner = GetInnerRect();
    const float caretX = PrefixWidth(m_Caret, atlas);
    const float totalW = PrefixWidth(static_cast<int>(m_Text.size()), atlas);

    if (totalW <= inner.size.x) {
        m_ScrollX = 0.0f;
        return;
    }
    if (caretX - m_ScrollX < 0.0f) {
        m_ScrollX = caretX;
    } else if (caretX - m_ScrollX > inner.size.x) {
        m_ScrollX = caretX - inner.size.x;
    }
    m_ScrollX = std::clamp(m_ScrollX, 0.0f, std::max(0.0f, totalW - inner.size.x));
}

// ========================================
// Font lifecycle
// ========================================

void LineEdit::EnsureFont(RenderContext& ctx) {
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

bool LineEdit::OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) {
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

void LineEdit::OnUpdate(float deltaTime) {
    UIControl::OnUpdate(deltaTime);

    if (HasFocus()) {
        m_BlinkTime += deltaTime;
        if (m_BlinkTime >= 0.5f) {
            m_BlinkTime = 0.0f;
            m_CaretVisible = !m_CaretVisible;
        }
    } else {
        m_CaretVisible = false;
    }
}

void LineEdit::EmitTextChanged() {
    WriteTextToProperty();
    Emit("text_changed", { GetText() });
    m_CaretVisible = true;
    m_BlinkTime = 0.0f;
}

void LineEdit::OnInput(float deltaTime) {
    (void)deltaTime;
    if (!IsEnabled()) {
        return;
    }

    // Re-sync if the text property was changed externally (editor/script).
    if (GetPropertyValue<std::string>("text") != m_CachedPropertyText) {
        SyncTextFromProperty();
    }

    input::InputManager& inputMgr = input::InputManager::Get();
    Vec2 mouse = GetCanvasMousePosition();
    Vec2 center = GetCenter();
    Vec2 half = GetBoundsSize() * 0.5f;
    bool overControl = mouse.x >= center.x - half.x && mouse.x <= center.x + half.x &&
                       mouse.y >= center.y - half.y && mouse.y <= center.y + half.y;

    const bool leftJustPressed = inputMgr.IsMouseButtonJustPressed(input::MouseButton::Left);
    const bool leftDown = inputMgr.IsMouseButtonPressed(input::MouseButton::Left);

    // Focus handling + click-to-place caret.
    if (leftJustPressed) {
        if (overControl) {
            GrabFocus();
            if (m_LastAtlas) {
                Rect inner = GetInnerRect();
                const float localX = (mouse.x - inner.position.x) + m_ScrollX;
                m_Caret = CaretIndexFromLocalX(localX, *m_LastAtlas);
            }
            m_SelAnchor = -1;
            m_MouseSelecting = true;
            m_CaretVisible = true;
            m_BlinkTime = 0.0f;
        } else if (HasFocus()) {
            ReleaseFocus();
            m_MouseSelecting = false;
        }
    }

    if (!leftDown) {
        m_MouseSelecting = false;
    }

    // Drag to extend the selection.
    if (m_MouseSelecting && leftDown && m_LastAtlas) {
        Rect inner = GetInnerRect();
        const float localX = (mouse.x - inner.position.x) + m_ScrollX;
        const int idx = CaretIndexFromLocalX(localX, *m_LastAtlas);
        if (idx != m_Caret) {
            if (m_SelAnchor < 0) m_SelAnchor = m_Caret;
            m_Caret = idx;
            m_CaretVisible = true;
            m_BlinkTime = 0.0f;
        }
    }

    if (!HasFocus()) {
        return;
    }

    if (!GetEditable()) {
        return;
    }

    bool changed = false;

    const bool ctrl = inputMgr.IsActionModifierPressed();
    const bool shift = inputMgr.IsShiftPressed();

    // Clipboard / select-all shortcuts.
    if (ctrl && inputMgr.IsKeyJustPressed(input::KeyCode::A)) {
        SelectAll();
    } else if (ctrl && inputMgr.IsKeyJustPressed(input::KeyCode::C)) {
        if (HasSelection()) inputMgr.SetClipboardText(GetSelectedText());
    } else if (ctrl && inputMgr.IsKeyJustPressed(input::KeyCode::X)) {
        if (HasSelection()) {
            inputMgr.SetClipboardText(GetSelectedText());
            DeleteSelection();
            changed = true;
        }
    } else if (ctrl && inputMgr.IsKeyJustPressed(input::KeyCode::V)) {
        std::string clip = inputMgr.GetClipboardText();
        if (!clip.empty()) {
            InsertString(clip);
            changed = true;
        }
    }

    // Editing keys.
    if (inputMgr.IsKeyJustPressed(input::KeyCode::Backspace)) {
        if (HasSelection()) {
            DeleteSelection();
        } else if (m_Caret > 0) {
            m_Text.erase(m_Text.begin() + (m_Caret - 1));
            --m_Caret;
        }
        changed = true;
    }
    if (inputMgr.IsKeyJustPressed(input::KeyCode::Delete)) {
        if (HasSelection()) {
            DeleteSelection();
        } else if (m_Caret < static_cast<int>(m_Text.size())) {
            m_Text.erase(m_Text.begin() + m_Caret);
        }
        changed = true;
    }

    // Caret movement.
    auto moveCaret = [&](int newCaret) {
        newCaret = std::clamp(newCaret, 0, static_cast<int>(m_Text.size()));
        if (shift) {
            if (m_SelAnchor < 0) m_SelAnchor = m_Caret;
        } else {
            m_SelAnchor = -1;
        }
        m_Caret = newCaret;
        m_CaretVisible = true;
        m_BlinkTime = 0.0f;
    };

    if (inputMgr.IsKeyJustPressed(input::KeyCode::Left)) {
        if (!shift && HasSelection()) moveCaret(SelectionMin());
        else moveCaret(m_Caret - 1);
    }
    if (inputMgr.IsKeyJustPressed(input::KeyCode::Right)) {
        if (!shift && HasSelection()) moveCaret(SelectionMax());
        else moveCaret(m_Caret + 1);
    }
    if (inputMgr.IsKeyJustPressed(input::KeyCode::Home)) {
        moveCaret(0);
    }
    if (inputMgr.IsKeyJustPressed(input::KeyCode::End)) {
        moveCaret(static_cast<int>(m_Text.size()));
    }
    if (inputMgr.IsKeyJustPressed(input::KeyCode::Enter)) {
        WriteTextToProperty();
        Emit("text_submitted", { GetText() });
    }

    // Typed characters.
    const std::vector<uint32_t>& typed = inputMgr.GetTextInput();
    for (uint32_t cp : typed) {
        if (cp < 0x20 || cp == 0x7F) continue;
        InsertCodepoint(cp);
        changed = true;
    }

    if (changed) {
        EmitTextChanged();
    }
}

// ========================================
// Rendering
// ========================================

void LineEdit::buildDrawCommands(RenderContext& ctx) {
    if (!m_Owner) {
        return;
    }
    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    if (!node2D) {
        return;
    }

    // A resolution change re-bakes the font atlas at a new oversampling density,
    // which repacks glyph UVs; regenerate the cached text mesh so it matches.
    if (ConsumeFontOversampleChanged()) {
        m_TextMeshKey.clear();
    }

    if (GetPropertyValue<std::string>("text") != m_CachedPropertyText) {
        SyncTextFromProperty();
    }

    EnsureFont(ctx);

    Vec2 center = GetCenter();
    Vec2 size = GetBoundsSize();
    Vec2 topLeft = Vec2(center.x - size.x * 0.5f, center.y - size.y * 0.5f);

    const float cornerRadius = ResolveThemedConstant("cornerRadius", "corner_radius");
    const float borderWidth = ResolveThemedConstant("borderWidth", "border_width");

    // Background + border (unclipped).
    if (borderWidth > 0.0f) {
        Vec2 borderTopLeft = topLeft - Vec2(borderWidth, borderWidth);
        Vec2 borderSize = size + Vec2(borderWidth * 2.0f, borderWidth * 2.0f);
        ctx.drawRoundedRect(borderTopLeft, borderSize, cornerRadius + borderWidth, GetBorderColor(), 0);
    }
    ctx.drawRoundedRect(topLeft, size, cornerRadius, GetBackgroundColor(), 0);

    const FontAtlas* atlas = m_FontHandle.isValid() ? ctx.getDevice()->getFontAtlas(m_FontHandle) : nullptr;
    if (!atlas) {
        return;
    }
    m_LastAtlas = atlas;

    Rect inner = GetInnerRect();
    const float fontSize = GetFontSize();

    const bool showPlaceholder = m_Text.empty();
    std::string displayStr = showPlaceholder ? GetPlaceholder() : DisplayString();
    Color textColor = showPlaceholder ? GetPlaceholderColor() : GetFontColor();

    UpdateScroll(*atlas);

    TextLayoutParams params;
    params.fontSize = fontSize;
    params.color = textColor;
    params.hAlign = TextHAlign::Left;
    // Let the layout center the single line inside the inner rect.
    params.vAlign = TextVAlign::Center;
    params.boxHeight = inner.size.y;
    params.multiline = false;
    params.wordWrap = false;

    TextLayoutResult layout = TextLayout::Layout(displayStr, *atlas, params);
    const float lineHeight = layout.lineHeight;

    // TextLayout's origin is the box TOP-left; on the Y-up canvas that is the inner
    // rect's max-Y corner. Selection/caret rects are placed from their min-Y corner,
    // which the layout reports as the line's bottom edge.
    const float boxTopY = inner.position.y + inner.size.y;
    const float boxLeftX = inner.position.x - m_ScrollX;
    const float lineBottomY = boxTopY + layout.GetLineBottomY(0);

    // Clip content to the inner rect.
    ctx.pushClipRect(inner.position, inner.size);

    // Selection highlight.
    if (HasSelection() && !showPlaceholder) {
        const float selX0 = PrefixWidth(SelectionMin(), *atlas);
        const float selX1 = PrefixWidth(SelectionMax(), *atlas);
        Vec2 selTopLeft = Vec2(boxLeftX + selX0, lineBottomY);
        Vec2 selSize = Vec2(std::max(0.0f, selX1 - selX0), lineHeight);
        if (selSize.x > 0.0f) {
            ctx.drawRoundedRect(selTopLeft, selSize, 0.0f, GetSelectionColor(), 0);
        }
    }

    // Text mesh, cached so the draw item references a live mesh at execution time.
    // The key captures everything that affects glyph positions/colors.
    {
        char keyBuf[160];
        std::snprintf(keyBuf, sizeof(keyBuf), "%.2f|%.2f|%.2f|%.3f|%.3f|%.3f|%.3f",
                      boxLeftX, boxTopY, fontSize,
                      textColor.r, textColor.g, textColor.b, textColor.a);
        std::string key = displayStr + "\x1f" + keyBuf;

        if (key != m_TextMeshKey) {
            if (m_TextMesh.isValid()) {
                ctx.getDevice()->destroyMesh(m_TextMesh);
                m_TextMesh = MeshHandle();
            }
            if (!layout.quads.empty()) {
                MeshData md;
                md.vertices.reserve(layout.quads.size() * 4);
                md.indices.reserve(layout.quads.size() * 6);
                uint32_t base = 0;
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

    // Caret.
    if (HasFocus() && m_CaretVisible && GetEditable()) {
        const float caretX = PrefixWidth(m_Caret, *atlas);
        Vec2 caretTopLeft = Vec2(boxLeftX + caretX, lineBottomY);
        ctx.drawRoundedRect(caretTopLeft, Vec2(2.0f, lineHeight), 0.0f, GetCaretColor(), 0);
    }

    ctx.popClipRect();
}

AABB LineEdit::getWorldBounds() const {
    Vec2 center = GetCenter();
    Vec2 half = GetBoundsSize() * 0.5f;
    return AABB(Vec3(center.x - half.x, center.y - half.y, -0.1f),
                Vec3(center.x + half.x, center.y + half.y, 0.1f));
}

RenderLayer LineEdit::getRenderLayer() const {
    return RenderLayer::Transparent;
}

Vec2 LineEdit::GetContentMinSize() const {
    const float pad = GetPropertyValue<float>("padding");
    return Vec2(GetFontSize() * 4.0f + pad * 2.0f, GetFontSize() * 1.4f + pad * 2.0f);
}

} // namespace components
} // namespace lupine
