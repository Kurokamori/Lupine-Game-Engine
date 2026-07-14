#include "lupine/components/SpinBox.hpp"
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
#include <cstdlib>

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

bool IsNumericChar(uint32_t cp) {
    return (cp >= '0' && cp <= '9') || cp == '.' || cp == '-' || cp == '+' || cp == 'e' || cp == 'E';
}

} // anonymous namespace

SpinBox::SpinBox()
    : UIControl("SpinBox")
{
}

SpinBox::SpinBox(const std::string& name)
    : UIControl(name)
{
}

SpinBox::~SpinBox() {
}

void SpinBox::DefineProperties() {
    DefineUIControlProperties(120.0f, 32.0f, "useUISpace", "Size");

    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(value, 0.0f, -1000000.0f, 1000000.0f, 0.01f, "SpinBox"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(minValue, 0.0f, -1000000.0f, 1000000.0f, 0.01f, "SpinBox"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(maxValue, 100.0f, -1000000.0f, 1000000.0f, 0.01f, "SpinBox"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(step, 1.0f, 0.0001f, 1000000.0f, 0.01f, "SpinBox"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(decimals, 0, 0, 8, 1, "SpinBox"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(editable, Bool, true, "SpinBox"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(prefix, String, std::string(""), "SpinBox"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(suffix, String, std::string(""), "SpinBox"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(dragSensitivity, 0.25f, 0.0f, 100.0f, 0.01f, "SpinBox"));

    DefineProperty(PROPERTY_FILE_GROUP(fontPath, std::string(""), "*.ttf,*.otf", "Text"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(fontSize, 16.0f, 1.0f, 256.0f, 1.0f, "Text"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(padding, 6.0f, 0.0f, 64.0f, 1.0f, "Text"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(fontColor, Color, Color::White(), "Appearance"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(backgroundColor, Color, Color(0.12f, 0.12f, 0.12f, 1.0f), "Appearance"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(buttonColor, Color, Color(0.3f, 0.3f, 0.3f, 1.0f), "Appearance"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderColor, Color, Color(0.4f, 0.4f, 0.4f, 1.0f), "Appearance"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(borderWidth, 1.0f, 0.0f, 16.0f, 1.0f, "Appearance"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(cornerRadius, 4.0f, 0.0f, 64.0f, 1.0f, "Appearance"));
}

void SpinBox::DefineSignals() {
    RegisterSignal({"value_changed",
                    {{"value", core::PropertyValueType::Float}},
                    "Emitted when the value changes."});
}

void SpinBox::OnAwake() {
    SetValue(GetValue());
}

// ========================================
// Value
// ========================================

float SpinBox::GetValue() const { return GetPropertyValue<float>("value"); }

void SpinBox::SetValue(float value) {
    SetPropertyValue<float>("value", ClampToRange(value));
}

float SpinBox::GetMinValue() const { return GetPropertyValue<float>("minValue"); }
void SpinBox::SetMinValue(float value) { SetPropertyValue<float>("minValue", value); SetValue(GetValue()); }
float SpinBox::GetMaxValue() const { return GetPropertyValue<float>("maxValue"); }
void SpinBox::SetMaxValue(float value) { SetPropertyValue<float>("maxValue", value); SetValue(GetValue()); }
float SpinBox::GetStep() const { return GetPropertyValue<float>("step"); }
void SpinBox::SetStep(float step) { SetPropertyValue<float>("step", step); }
int SpinBox::GetDecimals() const { return GetPropertyValue<int>("decimals"); }
void SpinBox::SetDecimals(int decimals) { SetPropertyValue<int>("decimals", std::clamp(decimals, 0, 8)); }
bool SpinBox::GetEditable() const { return GetPropertyValue<bool>("editable"); }
void SpinBox::SetEditable(bool editable) { SetPropertyValue<bool>("editable", editable); }

std::string SpinBox::GetPrefix() const { return GetPropertyValue<std::string>("prefix"); }
void SpinBox::SetPrefix(const std::string& prefix) { SetPropertyValue<std::string>("prefix", prefix); }
std::string SpinBox::GetSuffix() const { return GetPropertyValue<std::string>("suffix"); }
void SpinBox::SetSuffix(const std::string& suffix) { SetPropertyValue<std::string>("suffix", suffix); }

float SpinBox::ClampToRange(float value) const {
    const float lo = std::min(GetMinValue(), GetMaxValue());
    const float hi = std::max(GetMinValue(), GetMaxValue());
    return std::clamp(value, lo, hi);
}

void SpinBox::Step(int dir) {
    SetValue(GetValue() + static_cast<float>(dir) * GetStep());
    EmitValueChanged();
}

std::string SpinBox::FormatValue() const {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", GetDecimals(), GetValue());
    return GetPrefix() + std::string(buf) + GetSuffix();
}

// ========================================
// Font / appearance accessors
// ========================================

std::string SpinBox::GetFontPath() const { return ResolveThemedFontPath("fontPath", "font"); }
void SpinBox::SetFontPath(const std::string& path) { SetThemedProperty<std::string>("fontPath", path); }
float SpinBox::GetFontSize() const { return ResolveThemedFontSize("fontSize", "font_size", "font"); }
void SpinBox::SetFontSize(float size) { SetThemedProperty<float>("fontSize", size); }

const std::vector<UIControl::ThemeBinding>& SpinBox::GetThemeBindings() const {
    static const std::vector<ThemeBinding> kBindings = {
        { "fontColor",       "font_color",   ThemeBinding::Kind::Color },
        { "backgroundColor", "background",   ThemeBinding::Kind::Color },
        { "buttonColor",     "button_color", ThemeBinding::Kind::Color },
        { "borderColor",     "border_color", ThemeBinding::Kind::Color },
        { "fontPath",        "font",         ThemeBinding::Kind::Font },
        { "fontSize",        "font_size",    ThemeBinding::Kind::Constant },
        { "cornerRadius",    "corner_radius",ThemeBinding::Kind::Constant },
        { "borderWidth",     "border_width", ThemeBinding::Kind::Constant }
    };
    return kBindings;
}

Color SpinBox::GetFontColor() const {
    return ResolveThemedColor("fontColor", "font_color");
}
void SpinBox::SetFontColor(const Color& color) { SetThemedProperty<Color>("fontColor", color); }
Color SpinBox::GetBackgroundColor() const {
    return ResolveThemedColor("backgroundColor", "background");
}
void SpinBox::SetBackgroundColor(const Color& color) { SetThemedProperty<Color>("backgroundColor", color); }
Color SpinBox::GetButtonColor() const {
    return ResolveThemedColor("buttonColor", "button_color");
}
void SpinBox::SetButtonColor(const Color& color) { SetThemedProperty<Color>("buttonColor", color); }

// ========================================
// Geometry
// ========================================

Vec2 SpinBox::GetCenter() const {
    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    return node2D ? node2D->GetGlobalPosition() : Vec2(0.0f, 0.0f);
}

Rect SpinBox::GetStepperRect() const {
    Vec2 center = GetCenter();
    Vec2 size = GetBoundsSize();
    const float stepperW = std::min(size.x * 0.35f, std::max(16.0f, size.y));
    Vec2 topLeft = Vec2(center.x + size.x * 0.5f - stepperW, center.y - size.y * 0.5f);
    return Rect(topLeft, Vec2(stepperW, size.y));
}

Rect SpinBox::GetFieldRect() const {
    Vec2 center = GetCenter();
    Vec2 size = GetBoundsSize();
    const float stepperW = GetStepperRect().size.x;
    const float pad = GetPropertyValue<float>("padding");
    Vec2 topLeft = Vec2(center.x - size.x * 0.5f + pad, center.y - size.y * 0.5f + pad);
    Vec2 innerSize = Vec2(std::max(0.0f, size.x - stepperW - pad * 2.0f),
                          std::max(0.0f, size.y - pad * 2.0f));
    return Rect(topLeft, innerSize);
}

Rect SpinBox::GetUpButtonRect() const {
    Rect stepper = GetStepperRect();
    // Y-up canvas: stepper.position is the bottom-left (min-Y) corner, so the "up"
    // (increment) button is the TOP half — offset up by half the height.
    return Rect(Vec2(stepper.position.x, stepper.position.y + stepper.size.y * 0.5f),
                Vec2(stepper.size.x, stepper.size.y * 0.5f));
}

Rect SpinBox::GetDownButtonRect() const {
    Rect stepper = GetStepperRect();
    // Y-up canvas: the "down" (decrement) button is the BOTTOM half at the min-Y corner.
    return Rect(stepper.position, Vec2(stepper.size.x, stepper.size.y * 0.5f));
}

// ========================================
// Font lifecycle
// ========================================

void SpinBox::EnsureFont(RenderContext& ctx) {
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

bool SpinBox::OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) {
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
// Editing
// ========================================

void SpinBox::BeginEdit() {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", GetDecimals(), GetValue());
    m_EditBuffer = buf;
    m_Editing = true;
    m_CaretVisible = true;
    m_BlinkTime = 0.0f;
    GrabFocus();
}

void SpinBox::CommitEdit() {
    if (!m_Editing) return;
    const char* s = m_EditBuffer.c_str();
    char* end = nullptr;
    float v = std::strtof(s, &end);
    if (end != s) {
        SetValue(v);
        EmitValueChanged();
    }
    m_Editing = false;
}

void SpinBox::CancelEdit() {
    m_Editing = false;
}

void SpinBox::EmitValueChanged() {
    Emit("value_changed", { GetValue() });
}

// ========================================
// Update / input
// ========================================

void SpinBox::OnUpdate(float deltaTime) {
    UIControl::OnUpdate(deltaTime);
    if (m_Editing && HasFocus()) {
        m_BlinkTime += deltaTime;
        if (m_BlinkTime >= 0.5f) {
            m_BlinkTime = 0.0f;
            m_CaretVisible = !m_CaretVisible;
        }
    } else {
        m_CaretVisible = false;
    }
}

void SpinBox::OnInput(float deltaTime) {
    (void)deltaTime;
    if (!IsEnabled() || !GetEditable()) {
        return;
    }

    input::InputManager& inputMgr = input::InputManager::Get();
    Vec2 mouse = GetCanvasMousePosition();
    Vec2 center = GetCenter();
    Vec2 half = GetBoundsSize() * 0.5f;
    bool overControl = mouse.x >= center.x - half.x && mouse.x <= center.x + half.x &&
                       mouse.y >= center.y - half.y && mouse.y <= center.y + half.y;

    const bool leftJustPressed = inputMgr.IsMouseButtonJustPressed(input::MouseButton::Left);
    const bool leftJustReleased = inputMgr.IsMouseButtonJustReleased(input::MouseButton::Left);
    const bool leftDown = inputMgr.IsMouseButtonPressed(input::MouseButton::Left);

    if (leftJustPressed) {
        if (GetUpButtonRect().Contains(mouse)) {
            Step(+1);
        } else if (GetDownButtonRect().Contains(mouse)) {
            Step(-1);
        } else if (GetFieldRect().Contains(mouse)) {
            m_PressedInField = true;
            m_Dragging = false;
            m_DragStartMouseX = mouse.x;
            m_DragStartValue = GetValue();
            GrabFocus();
        } else if (!overControl) {
            if (m_Editing) CommitEdit();
            if (HasFocus()) ReleaseFocus();
        }
    }

    // Drag-to-adjust (only when not in text edit mode).
    if (m_PressedInField && leftDown && !m_Editing) {
        const float dx = mouse.x - m_DragStartMouseX;
        if (std::abs(dx) > 3.0f) {
            m_Dragging = true;
        }
        if (m_Dragging) {
            const float sensitivity = GetPropertyValue<float>("dragSensitivity");
            float newValue = m_DragStartValue + dx * GetStep() * sensitivity;
            SetValue(newValue);
            EmitValueChanged();
        }
    }

    if (leftJustReleased) {
        if (m_PressedInField && !m_Dragging && !m_Editing) {
            BeginEdit();
        }
        m_PressedInField = false;
        m_Dragging = false;
    }

    // Wheel stepping.
    if (overControl && !m_Dragging) {
        glm::vec2 wheel = inputMgr.GetMouseScrollDelta();
        if (wheel.y > 0.0f) Step(+1);
        else if (wheel.y < 0.0f) Step(-1);
    }

    // Lost focus while editing -> commit.
    if (m_Editing && !HasFocus()) {
        CommitEdit();
    }

    // Inline text editing.
    if (m_Editing && HasFocus()) {
        bool changed = false;
        if (inputMgr.IsKeyJustPressed(input::KeyCode::Enter)) {
            CommitEdit();
            ReleaseFocus();
            return;
        }
        if (inputMgr.IsKeyJustPressed(input::KeyCode::Escape)) {
            CancelEdit();
            ReleaseFocus();
            return;
        }
        if (inputMgr.IsKeyJustPressed(input::KeyCode::Backspace)) {
            if (!m_EditBuffer.empty()) {
                m_EditBuffer.pop_back();
                changed = true;
            }
        }
        const std::vector<uint32_t>& typed = inputMgr.GetTextInput();
        for (uint32_t cp : typed) {
            if (IsNumericChar(cp)) {
                m_EditBuffer.push_back(static_cast<char>(cp));
                changed = true;
            }
        }
        if (changed) {
            m_CaretVisible = true;
            m_BlinkTime = 0.0f;
        }
    }
}

// ========================================
// Rendering
// ========================================

void SpinBox::buildDrawCommands(RenderContext& ctx) {
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

    EnsureFont(ctx);

    Vec2 center = GetCenter();
    Vec2 size = GetBoundsSize();
    Vec2 topLeft = Vec2(center.x - size.x * 0.5f, center.y - size.y * 0.5f);

    const float cornerRadius = ResolveThemedConstant("cornerRadius", "corner_radius");
    const float borderWidth = ResolveThemedConstant("borderWidth", "border_width");
    Color borderColor = ResolveThemedColor("borderColor", "border_color");

    // Border + background.
    if (borderWidth > 0.0f) {
        Vec2 bTopLeft = topLeft - Vec2(borderWidth, borderWidth);
        Vec2 bSize = size + Vec2(borderWidth * 2.0f, borderWidth * 2.0f);
        ctx.drawRoundedRect(bTopLeft, bSize, cornerRadius + borderWidth, borderColor, 0);
    }
    ctx.drawRoundedRect(topLeft, size, cornerRadius, GetBackgroundColor(), 0);

    // Stepper buttons background.
    Rect upBtn = GetUpButtonRect();
    Rect downBtn = GetDownButtonRect();
    Color btnColor = GetButtonColor();
    ctx.drawRoundedRect(upBtn.position, upBtn.size, 0.0f, btnColor, 0);
    ctx.drawRoundedRect(downBtn.position, downBtn.size, 0.0f, btnColor, 0);

    // Plus / minus glyphs (font-free bars).
    Color glyphColor = GetFontColor();
    auto drawSign = [&](const Rect& r, bool plus) {
        Vec2 c = r.GetCenter();
        const float arm = std::min(r.size.x, r.size.y) * 0.28f;
        const float thick = std::max(1.5f, arm * 0.4f);
        ctx.drawRoundedRect(Vec2(c.x - arm, c.y - thick * 0.5f), Vec2(arm * 2.0f, thick), thick * 0.5f, glyphColor, 0);
        if (plus) {
            ctx.drawRoundedRect(Vec2(c.x - thick * 0.5f, c.y - arm), Vec2(thick, arm * 2.0f), thick * 0.5f, glyphColor, 0);
        }
    };
    drawSign(upBtn, true);
    drawSign(downBtn, false);

    const FontAtlas* atlas = m_FontHandle.isValid() ? ctx.getDevice()->getFontAtlas(m_FontHandle) : nullptr;
    if (!atlas) {
        return;
    }

    Rect field = GetFieldRect();
    const float fontSize = GetFontSize();
    std::string displayStr = m_Editing ? m_EditBuffer : FormatValue();
    Color textColor = GetFontColor();

    TextLayoutParams params;
    params.fontSize = fontSize;
    params.color = textColor;
    params.hAlign = TextHAlign::Left;
    // Let the layout center the single line inside the field.
    params.vAlign = TextVAlign::Center;
    params.boxHeight = field.size.y;
    params.multiline = false;
    params.wordWrap = false;

    TextLayoutResult layout = TextLayout::Layout(displayStr, *atlas, params);
    const float lineHeight = layout.lineHeight;
    // TextLayout's origin is the box TOP-left = the field's max-Y corner.
    const float boxTopY = field.position.y + field.size.y;
    const float boxLeftX = field.position.x;
    const float lineBottomY = boxTopY + layout.GetLineBottomY(0);

    ctx.pushClipRect(field.position, field.size);

    if (!layout.quads.empty()) {
        char keyBuf[160];
        std::snprintf(keyBuf, sizeof(keyBuf), "%.2f|%.2f|%.2f|%.3f|%.3f|%.3f|%.3f",
                      boxLeftX, boxTopY, fontSize, textColor.r, textColor.g, textColor.b, textColor.a);
        std::string key = displayStr + "\x1f" + keyBuf;

        if (key != m_TextMeshKey) {
            if (m_TextMesh.isValid()) {
                ctx.getDevice()->destroyMesh(m_TextMesh);
                m_TextMesh = MeshHandle();
            }
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
            m_TextMeshKey = key;
        }

        if (m_TextMesh.isValid()) {
            MaterialPropertyBlock overrides;
            overrides.setTexture("u_FontAtlas", atlas->texture);
            ctx.drawMesh(m_TextMesh, ctx.getDefaultTextMaterial(), Mat4::Identity(), overrides);
        }
    }

    // Caret at end of the edit buffer while editing.
    if (m_Editing && HasFocus() && m_CaretVisible) {
        const float caretX = TextLayout::MeasureLineWidth(displayStr, *atlas, fontSize);
        ctx.drawRoundedRect(Vec2(boxLeftX + caretX, lineBottomY), Vec2(2.0f, lineHeight), 0.0f, GetFontColor(), 0);
    }

    ctx.popClipRect();
}

AABB SpinBox::getWorldBounds() const {
    Vec2 center = GetCenter();
    Vec2 half = GetBoundsSize() * 0.5f;
    return AABB(Vec3(center.x - half.x, center.y - half.y, -0.1f),
                Vec3(center.x + half.x, center.y + half.y, 0.1f));
}

RenderLayer SpinBox::getRenderLayer() const {
    return RenderLayer::Transparent;
}

Vec2 SpinBox::GetContentMinSize() const {
    const float pad = GetPropertyValue<float>("padding");
    return Vec2(GetFontSize() * 4.0f + pad * 2.0f + 24.0f, GetFontSize() * 1.4f + pad * 2.0f);
}

} // namespace components
} // namespace lupine
