#include "lupine/components/TextEdit.hpp"
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

std::string EncodeUTF8(const std::vector<uint32_t>& cps, int start, int end) {
    std::string out;
    for (int i = start; i < end && i < static_cast<int>(cps.size()); ++i) {
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

TextEdit::TextEdit()
    : UIControl("TextEdit")
{
}

TextEdit::TextEdit(const std::string& name)
    : UIControl(name)
{
}

TextEdit::~TextEdit() {
}

void TextEdit::DefineProperties() {
    DefineUIControlProperties(280.0f, 160.0f, "useUISpace", "Size");

    DefineProperty(PROPERTY_DEFAULT_GROUP(text, String, std::string(""), "Text"));
    DefineProperty(PROPERTY_FILE_GROUP(fontPath, std::string(""), "*.ttf,*.otf", "Text"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(fontSize, 16.0f, 1.0f, 256.0f, 1.0f, "Text"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(editable, Bool, true, "Text"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(lineSpacing, 1.0f, 0.1f, 5.0f, 0.05f, "Text"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(padding, 6.0f, 0.0f, 64.0f, 1.0f, "Text"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(fontColor, Color, Color::White(), "Appearance"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(backgroundColor, Color, Color(0.1f, 0.1f, 0.1f, 1.0f), "Appearance"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderColor, Color, Color(0.4f, 0.4f, 0.4f, 1.0f), "Appearance"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(borderWidth, 1.0f, 0.0f, 16.0f, 1.0f, "Appearance"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(cornerRadius, 4.0f, 0.0f, 64.0f, 1.0f, "Appearance"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(selectionColor, Color, Color(0.2f, 0.4f, 0.8f, 0.6f), "Appearance"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(caretColor, Color, Color::White(), "Appearance"));
}

void TextEdit::DefineSignals() {
    RegisterSignal({"text_changed",
                    {{"text", core::PropertyValueType::String}},
                    "Emitted whenever the text changes."});
}

void TextEdit::OnAwake() {
    SyncTextFromProperty();
    m_Caret = static_cast<int>(m_Text.size());
}

// ========================================
// Property sync
// ========================================

void TextEdit::SyncTextFromProperty() {
    std::string propText = GetPropertyValue<std::string>("text");
    m_Text = DecodeUTF8(propText);
    m_CachedPropertyText = propText;
    m_Caret = std::clamp(m_Caret, 0, static_cast<int>(m_Text.size()));
    m_SelAnchor = -1;
}

void TextEdit::WriteTextToProperty() {
    std::string encoded = EncodeUTF8(m_Text, 0, static_cast<int>(m_Text.size()));
    m_CachedPropertyText = encoded;
    SetPropertyValue<std::string>("text", encoded);
}

std::string TextEdit::GetText() const { return GetPropertyValue<std::string>("text"); }

void TextEdit::SetText(const std::string& text) {
    SetPropertyValue<std::string>("text", text);
    SyncTextFromProperty();
    m_Caret = static_cast<int>(m_Text.size());
}

bool TextEdit::GetEditable() const { return GetPropertyValue<bool>("editable"); }
void TextEdit::SetEditable(bool editable) { SetPropertyValue<bool>("editable", editable); }

// ========================================
// Caret / selection
// ========================================

void TextEdit::SetCaretPosition(int position) {
    m_Caret = std::clamp(position, 0, static_cast<int>(m_Text.size()));
}

void TextEdit::SelectAll() {
    m_SelAnchor = 0;
    m_Caret = static_cast<int>(m_Text.size());
}

void TextEdit::Deselect() { m_SelAnchor = -1; }

bool TextEdit::HasSelection() const {
    return m_SelAnchor >= 0 && m_SelAnchor != m_Caret;
}

int TextEdit::SelectionMin() const {
    return HasSelection() ? std::min(m_SelAnchor, m_Caret) : m_Caret;
}

int TextEdit::SelectionMax() const {
    return HasSelection() ? std::max(m_SelAnchor, m_Caret) : m_Caret;
}

std::string TextEdit::GetSelectedText() const {
    if (!HasSelection()) return std::string();
    return EncodeUTF8(m_Text, SelectionMin(), SelectionMax());
}

void TextEdit::DeleteSelection() {
    if (!HasSelection()) return;
    const int lo = SelectionMin();
    const int hi = SelectionMax();
    m_Text.erase(m_Text.begin() + lo, m_Text.begin() + hi);
    m_Caret = lo;
    m_SelAnchor = -1;
}

void TextEdit::InsertCodepoint(uint32_t codepoint) {
    if (HasSelection()) {
        DeleteSelection();
    }
    m_Text.insert(m_Text.begin() + m_Caret, codepoint);
    ++m_Caret;
    m_SelAnchor = -1;
}

void TextEdit::InsertString(const std::string& utf8) {
    std::vector<uint32_t> cps = DecodeUTF8(utf8);
    if (HasSelection()) {
        DeleteSelection();
    }
    for (uint32_t cp : cps) {
        if (cp == '\r') continue;
        m_Text.insert(m_Text.begin() + m_Caret, cp);
        ++m_Caret;
    }
    m_SelAnchor = -1;
}

// ========================================
// Line model
// ========================================

std::vector<TextEdit::LineSpan> TextEdit::ComputeLines() const {
    std::vector<LineSpan> lines;
    int start = 0;
    const int n = static_cast<int>(m_Text.size());
    for (int i = 0; i < n; ++i) {
        if (m_Text[i] == '\n') {
            lines.push_back({start, i});
            start = i + 1;
        }
    }
    lines.push_back({start, n});
    return lines;
}

int TextEdit::LineIndexOf(int caret, const std::vector<LineSpan>& lines) const {
    for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
        if (caret >= lines[i].start && caret <= lines[i].end) {
            return i;
        }
    }
    return std::max(0, static_cast<int>(lines.size()) - 1);
}

std::string TextEdit::LineString(const LineSpan& span) const {
    return EncodeUTF8(m_Text, span.start, span.end);
}

float TextEdit::CaretXForLine(const LineSpan& span, int caret, const FontAtlas& atlas) const {
    const int col = std::clamp(caret - span.start, 0, span.end - span.start);
    std::string line = LineString(span);
    return TextLayout::MeasurePrefixWidth(line, static_cast<size_t>(col), atlas, GetFontSize());
}

float TextEdit::LineAdvance(const FontAtlas& atlas) const {
    const float scale = (atlas.fontSize > 0.0f) ? (GetFontSize() / atlas.fontSize) : 1.0f;
    const float base = (atlas.lineHeight > 0.0f) ? (atlas.lineHeight * scale) : (GetFontSize() * 1.2f);
    return base * GetLineSpacing();
}

int TextEdit::CaretFromMouse(const Vec2& mouse, const FontAtlas& atlas) {
    std::vector<LineSpan> lines = ComputeLines();
    Rect inner = GetInnerRect();
    const float lineAdv = LineAdvance(atlas);
    // Y-up canvas: line 0 sits at the inner-rect top and lines stack downward.
    const float topAnchor = (inner.position.y + inner.size.y) + m_ScrollY;
    const float boxLeftX = inner.position.x - m_ScrollX;

    int lineIdx = static_cast<int>(std::floor((topAnchor - mouse.y) / std::max(1.0f, lineAdv)));
    lineIdx = std::clamp(lineIdx, 0, static_cast<int>(lines.size()) - 1);

    const LineSpan& span = lines[lineIdx];
    std::string line = LineString(span);
    const float localX = mouse.x - boxLeftX;
    const float fontSize = GetFontSize();

    int bestCol = 0;
    float bestDist = std::abs(0.0f - localX);
    const int lineLen = span.end - span.start;
    for (int c = 1; c <= lineLen; ++c) {
        float w = TextLayout::MeasurePrefixWidth(line, static_cast<size_t>(c), atlas, fontSize);
        float d = std::abs(w - localX);
        if (d < bestDist) {
            bestDist = d;
            bestCol = c;
        }
    }
    return span.start + bestCol;
}

void TextEdit::EnsureCaretVisible(const FontAtlas& atlas) {
    std::vector<LineSpan> lines = ComputeLines();
    Rect inner = GetInnerRect();
    const float lineAdv = LineAdvance(atlas);

    const int lineIdx = LineIndexOf(m_Caret, lines);
    const float caretLocalY = static_cast<float>(lineIdx) * lineAdv;
    const float caretLocalX = CaretXForLine(lines[lineIdx], m_Caret, atlas);

    if (caretLocalY < m_ScrollY) {
        m_ScrollY = caretLocalY;
    } else if (caretLocalY + lineAdv > m_ScrollY + inner.size.y) {
        m_ScrollY = caretLocalY + lineAdv - inner.size.y;
    }
    if (caretLocalX < m_ScrollX) {
        m_ScrollX = caretLocalX;
    } else if (caretLocalX > m_ScrollX + inner.size.x) {
        m_ScrollX = caretLocalX - inner.size.x;
    }

    const float totalH = static_cast<float>(lines.size()) * lineAdv;
    m_ScrollY = std::clamp(m_ScrollY, 0.0f, std::max(0.0f, totalH - inner.size.y));
    m_ScrollX = std::max(0.0f, m_ScrollX);
}

// ========================================
// Font / appearance accessors
// ========================================

std::string TextEdit::GetFontPath() const { return ResolveThemedFontPath("fontPath", "font"); }
void TextEdit::SetFontPath(const std::string& path) { SetThemedProperty<std::string>("fontPath", path); }
float TextEdit::GetFontSize() const { return ResolveThemedFontSize("fontSize", "font_size", "font"); }
void TextEdit::SetFontSize(float size) { SetThemedProperty<float>("fontSize", size); }
float TextEdit::GetLineSpacing() const { return GetPropertyValue<float>("lineSpacing"); }
void TextEdit::SetLineSpacing(float spacing) { SetPropertyValue<float>("lineSpacing", spacing); }

const std::vector<UIControl::ThemeBinding>& TextEdit::GetThemeBindings() const {
    static const std::vector<ThemeBinding> kBindings = {
        { "fontColor",       "font_color",     ThemeBinding::Kind::Color },
        { "backgroundColor", "background",      ThemeBinding::Kind::Color },
        { "borderColor",     "border_color",    ThemeBinding::Kind::Color },
        { "selectionColor",  "selection_color", ThemeBinding::Kind::Color },
        { "caretColor",      "caret_color",     ThemeBinding::Kind::Color },
        { "fontPath",        "font",            ThemeBinding::Kind::Font },
        { "fontSize",        "font_size",       ThemeBinding::Kind::Constant },
        { "cornerRadius",    "corner_radius",   ThemeBinding::Kind::Constant },
        { "borderWidth",     "border_width",    ThemeBinding::Kind::Constant }
    };
    return kBindings;
}

Color TextEdit::GetFontColor() const {
    return ResolveThemedColor("fontColor", "font_color");
}
void TextEdit::SetFontColor(const Color& color) { SetThemedProperty<Color>("fontColor", color); }
Color TextEdit::GetBackgroundColor() const {
    return ResolveThemedColor("backgroundColor", "background");
}
void TextEdit::SetBackgroundColor(const Color& color) { SetThemedProperty<Color>("backgroundColor", color); }
Color TextEdit::GetSelectionColor() const {
    return ResolveThemedColor("selectionColor", "selection_color");
}
void TextEdit::SetSelectionColor(const Color& color) { SetThemedProperty<Color>("selectionColor", color); }
Color TextEdit::GetCaretColor() const {
    return ResolveThemedColor("caretColor", "caret_color");
}
void TextEdit::SetCaretColor(const Color& color) { SetThemedProperty<Color>("caretColor", color); }

// ========================================
// Geometry
// ========================================

Vec2 TextEdit::GetCenter() const {
    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    return node2D ? node2D->GetGlobalPosition() : Vec2(0.0f, 0.0f);
}

Rect TextEdit::GetInnerRect() const {
    const Vec2 center = GetCenter();
    const Vec2 size = GetBoundsSize();
    const Rect box(center.x - size.x * 0.5f, center.y - size.y * 0.5f, size.x, size.y);

    // As LineEdit: the scalar `padding` still applies, and the themed StyleBox's per-side
    // content margins now apply too.
    return GetContentInsetRect(box);
}

// ========================================
// Font lifecycle
// ========================================

void TextEdit::EnsureFont(RenderContext& ctx) {
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

bool TextEdit::OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) {
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

void TextEdit::OnUpdate(float deltaTime) {
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

void TextEdit::EmitTextChanged() {
    WriteTextToProperty();
    Emit("text_changed", { GetText() });
    m_CaretVisible = true;
    m_BlinkTime = 0.0f;
}

void TextEdit::OnInput(float deltaTime) {
    (void)deltaTime;
    if (!IsEnabled()) {
        return;
    }

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

    if (leftJustPressed) {
        if (overControl) {
            GrabFocus();
            if (m_LastAtlas) {
                m_Caret = CaretFromMouse(mouse, *m_LastAtlas);
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
    if (m_MouseSelecting && leftDown && m_LastAtlas) {
        int idx = CaretFromMouse(mouse, *m_LastAtlas);
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

    // Wheel scrolling (works even when read-only).
    if (overControl && m_LastAtlas) {
        glm::vec2 wheel = inputMgr.GetMouseScrollDelta();
        if (wheel.y != 0.0f) {
            m_ScrollY -= wheel.y * LineAdvance(*m_LastAtlas) * 3.0f;
            std::vector<LineSpan> lines = ComputeLines();
            const float totalH = static_cast<float>(lines.size()) * LineAdvance(*m_LastAtlas);
            m_ScrollY = std::clamp(m_ScrollY, 0.0f, std::max(0.0f, totalH - GetInnerRect().size.y));
        }
    }

    if (!GetEditable()) {
        return;
    }

    bool changed = false;
    const bool ctrl = inputMgr.IsActionModifierPressed();
    const bool shift = inputMgr.IsShiftPressed();

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

    if (inputMgr.IsKeyJustPressed(input::KeyCode::Backspace)) {
        if (HasSelection()) DeleteSelection();
        else if (m_Caret > 0) { m_Text.erase(m_Text.begin() + (m_Caret - 1)); --m_Caret; }
        changed = true;
    }
    if (inputMgr.IsKeyJustPressed(input::KeyCode::Delete)) {
        if (HasSelection()) DeleteSelection();
        else if (m_Caret < static_cast<int>(m_Text.size())) m_Text.erase(m_Text.begin() + m_Caret);
        changed = true;
    }
    if (inputMgr.IsKeyJustPressed(input::KeyCode::Enter)) {
        InsertCodepoint('\n');
        changed = true;
    }
    if (inputMgr.IsKeyJustPressed(input::KeyCode::Tab)) {
        InsertCodepoint('\t');
        changed = true;
    }

    auto moveCaret = [&](int newCaret) {
        newCaret = std::clamp(newCaret, 0, static_cast<int>(m_Text.size()));
        if (shift) { if (m_SelAnchor < 0) m_SelAnchor = m_Caret; }
        else { m_SelAnchor = -1; }
        m_Caret = newCaret;
        m_CaretVisible = true;
        m_BlinkTime = 0.0f;
    };

    std::vector<LineSpan> lines = ComputeLines();
    const int curLineIdx = LineIndexOf(m_Caret, lines);
    const int curCol = m_Caret - lines[curLineIdx].start;

    if (inputMgr.IsKeyJustPressed(input::KeyCode::Left)) {
        if (!shift && HasSelection()) moveCaret(SelectionMin());
        else moveCaret(m_Caret - 1);
    }
    if (inputMgr.IsKeyJustPressed(input::KeyCode::Right)) {
        if (!shift && HasSelection()) moveCaret(SelectionMax());
        else moveCaret(m_Caret + 1);
    }
    if (inputMgr.IsKeyJustPressed(input::KeyCode::Up)) {
        if (curLineIdx > 0) {
            const LineSpan& t = lines[curLineIdx - 1];
            moveCaret(t.start + std::min(curCol, t.end - t.start));
        } else {
            moveCaret(0);
        }
    }
    if (inputMgr.IsKeyJustPressed(input::KeyCode::Down)) {
        if (curLineIdx + 1 < static_cast<int>(lines.size())) {
            const LineSpan& t = lines[curLineIdx + 1];
            moveCaret(t.start + std::min(curCol, t.end - t.start));
        } else {
            moveCaret(static_cast<int>(m_Text.size()));
        }
    }
    if (inputMgr.IsKeyJustPressed(input::KeyCode::Home)) {
        moveCaret(lines[curLineIdx].start);
    }
    if (inputMgr.IsKeyJustPressed(input::KeyCode::End)) {
        moveCaret(lines[curLineIdx].end);
    }
    if (inputMgr.IsKeyJustPressed(input::KeyCode::PageUp)) {
        int visibleLines = 1;
        if (m_LastAtlas) visibleLines = std::max(1, static_cast<int>(GetInnerRect().size.y / LineAdvance(*m_LastAtlas)));
        int target = std::max(0, curLineIdx - visibleLines);
        moveCaret(lines[target].start + std::min(curCol, lines[target].end - lines[target].start));
    }
    if (inputMgr.IsKeyJustPressed(input::KeyCode::PageDown)) {
        int visibleLines = 1;
        if (m_LastAtlas) visibleLines = std::max(1, static_cast<int>(GetInnerRect().size.y / LineAdvance(*m_LastAtlas)));
        int target = std::min(static_cast<int>(lines.size()) - 1, curLineIdx + visibleLines);
        moveCaret(lines[target].start + std::min(curCol, lines[target].end - lines[target].start));
    }

    const std::vector<uint32_t>& typed = inputMgr.GetTextInput();
    for (uint32_t cp : typed) {
        if (cp < 0x20 || cp == 0x7F) continue;
        InsertCodepoint(cp);
        changed = true;
    }

    if (changed) {
        EmitTextChanged();
        if (m_LastAtlas) EnsureCaretVisible(*m_LastAtlas);
    }
}

// ========================================
// Rendering
// ========================================

void TextEdit::buildDrawCommands(RenderContext& ctx) {
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
    Color borderColor = ResolveThemedColor("borderColor", "border_color");

    if (borderWidth > 0.0f) {
        Vec2 bTopLeft = topLeft - Vec2(borderWidth, borderWidth);
        Vec2 bSize = size + Vec2(borderWidth * 2.0f, borderWidth * 2.0f);
        ctx.drawRoundedRect(bTopLeft, bSize, cornerRadius + borderWidth, borderColor, 0);
    }
    ctx.drawRoundedRect(topLeft, size, cornerRadius, GetBackgroundColor(), 0);

    const FontAtlas* atlas = m_FontHandle.isValid() ? ctx.getDevice()->getFontAtlas(m_FontHandle) : nullptr;
    if (!atlas) {
        return;
    }
    m_LastAtlas = atlas;

    Rect inner = GetInnerRect();
    const float fontSize = GetFontSize();
    const float lineAdv = LineAdvance(*atlas);
    const float boxLeftX = inner.position.x - m_ScrollX;
    // Y-up canvas: anchor line 0 at the inner-rect top and stack lines downward. Each
    // logical line is laid out and placed individually so that per-line selection and
    // caret geometry share exactly the same band arithmetic as the glyphs.
    // lineBandMinY is the min-Y (bottom) corner of a line's row band, matching the
    // rect-origin convention used for selection/caret; lineBandTopY is its max-Y
    // corner, which is the origin TextLayout expects.
    const float topAnchor = (inner.position.y + inner.size.y) + m_ScrollY;
    auto lineBandTopY = [&](int li) -> float {
        return topAnchor - static_cast<float>(li) * lineAdv;
    };
    auto lineBandMinY = [&](int li) -> float {
        return topAnchor - static_cast<float>(li + 1) * lineAdv;
    };

    std::string displayStr = EncodeUTF8(m_Text, 0, static_cast<int>(m_Text.size()));
    Color textColor = GetFontColor();

    ctx.pushClipRect(inner.position, inner.size);

    std::vector<LineSpan> lines = ComputeLines();

    // Selection highlight (per line).
    if (HasSelection()) {
        const int selLo = SelectionMin();
        const int selHi = SelectionMax();
        const int loLine = LineIndexOf(selLo, lines);
        const int hiLine = LineIndexOf(selHi, lines);
        for (int li = loLine; li <= hiLine; ++li) {
            const LineSpan& span = lines[li];
            const int a = std::max(selLo, span.start);
            const int b = std::min(selHi, span.end);
            std::string line = LineString(span);
            float x0 = TextLayout::MeasurePrefixWidth(line, static_cast<size_t>(a - span.start), *atlas, fontSize);
            float x1 = TextLayout::MeasurePrefixWidth(line, static_cast<size_t>(b - span.start), *atlas, fontSize);
            // Lines that continue into the next selected line get a small trailing
            // marker so the newline reads as selected.
            if (li < hiLine) x1 += fontSize * 0.3f;
            if (x1 > x0) {
                Vec2 selTopLeft = Vec2(boxLeftX + x0, lineBandMinY(li));
                Vec2 selSize = Vec2(x1 - x0, lineAdv);
                ctx.drawRoundedRect(selTopLeft, selSize, 0.0f, GetSelectionColor(), 0);
            }
        }
    }

    // Text mesh (cached), built per logical line so lines read top-to-bottom.
    {
        char keyBuf[160];
        std::snprintf(keyBuf, sizeof(keyBuf), "%.2f|%.2f|%.2f|%.3f|%.3f|%.3f|%.3f",
                      boxLeftX, topAnchor, fontSize, textColor.r, textColor.g, textColor.b, textColor.a);
        std::string key = displayStr + "\x1f" + keyBuf;

        if (key != m_TextMeshKey) {
            if (m_TextMesh.isValid()) {
                ctx.getDevice()->destroyMesh(m_TextMesh);
                m_TextMesh = MeshHandle();
            }
            MeshData md;
            uint32_t base = 0;
            for (int li = 0; li < static_cast<int>(lines.size()); ++li) {
                std::string line = LineString(lines[li]);
                if (line.empty()) continue;

                TextLayoutParams params;
                params.fontSize = fontSize;
                params.color = textColor;
                params.hAlign = TextHAlign::Left;
                // Center the line's em box within its row band, so line spacing > 1
                // distributes evenly instead of hanging the glyphs off the band edge.
                params.vAlign = TextVAlign::Center;
                params.boxHeight = lineAdv;
                params.multiline = false;
                params.wordWrap = false;
                TextLayoutResult layout = TextLayout::Layout(line, *atlas, params);

                const float originY = lineBandTopY(li);
                for (const TextGlyphQuad& q : layout.quads) {
                    for (int k = 0; k < 4; ++k) {
                        Vertex vtx;
                        vtx.position = Vec3(boxLeftX + q.pos[k].x, originY + q.pos[k].y, 0.0f);
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

    // Caret.
    if (HasFocus() && m_CaretVisible && GetEditable()) {
        const int li = LineIndexOf(m_Caret, lines);
        const float caretX = CaretXForLine(lines[li], m_Caret, *atlas);
        Vec2 caretTopLeft = Vec2(boxLeftX + caretX, lineBandMinY(li));
        ctx.drawRoundedRect(caretTopLeft, Vec2(2.0f, lineAdv), 0.0f, GetCaretColor(), 0);
    }

    ctx.popClipRect();
}

AABB TextEdit::getWorldBounds() const {
    Vec2 center = GetCenter();
    Vec2 half = GetBoundsSize() * 0.5f;
    return AABB(Vec3(center.x - half.x, center.y - half.y, -0.1f),
                Vec3(center.x + half.x, center.y + half.y, 0.1f));
}

RenderLayer TextEdit::getRenderLayer() const {
    return RenderLayer::Transparent;
}

Vec2 TextEdit::GetContentMinSize() const {
    const float pad = GetPropertyValue<float>("padding");
    return Vec2(GetFontSize() * 6.0f + pad * 2.0f, GetFontSize() * 3.0f + pad * 2.0f);
}

} // namespace components
} // namespace lupine
