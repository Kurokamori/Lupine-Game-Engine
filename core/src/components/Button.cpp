#include "lupine/components/Button.hpp"
#include "lupine/components/StyleBoxRenderer.hpp"
#include "lupine/localization/LocalizationManager.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/ui/ThemeManager.hpp"
#include "lupine/ui/Theme.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/Mesh.hpp"
#include "lupine/rendering/Font.hpp"
#include "lupine/rendering/DrawCommand.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/rendering/TextureUpload.hpp"
#include "lupine/rendering/gfx/GfxDescriptors.hpp"
#include "lupine/input/InputManager.hpp"
#include "lupine/rendering/ViewportUtils.hpp"
#include "lupine/asset/AssetDatabase.hpp"
#include "lupine/logger/Logger.hpp"

namespace lupine {
namespace components {

using namespace core;
using namespace math;

using core::Node2D;
using core::Node3D;

namespace {
// Maps an interaction state to its themed-stylebox entry name (Godot Button parity).
const char* ButtonStateThemeEntry(ButtonState state) {
    switch (state) {
        case ButtonState::Normal:   return "normal";
        case ButtonState::Hover:    return "hover";
        case ButtonState::Pressed:  return "pressed";
        case ButtonState::Disabled: return "disabled";
        default:                    return "normal";
    }
}
} // namespace

Button::Button()
    : UIControl("Button")
    , m_BaseStyleBox(nullptr)
    , m_CornerRadius(0.0f, true)
    , m_BorderWidth(0.0f, true)
    , m_MeshNeedsRegeneration(true)
{

    m_BaseStyleBox = std::make_shared<StyleBoxFlat>();

    m_StateStyles[static_cast<int>(ButtonState::Normal)].modulationColor = Color::White();
    m_StateStyles[static_cast<int>(ButtonState::Hover)].modulationColor = Color(1.2f, 1.2f, 1.2f, 1.0f);
    m_StateStyles[static_cast<int>(ButtonState::Pressed)].modulationColor = Color(0.8f, 0.8f, 0.8f, 1.0f);
    m_StateStyles[static_cast<int>(ButtonState::Disabled)].modulationColor = Color(0.5f, 0.5f, 0.5f, 0.5f);
}

Button::Button(const std::string& name)
    : UIControl(name)
    , m_BaseStyleBox(nullptr)
    , m_CornerRadius(0.0f, true)
    , m_BorderWidth(0.0f, true)
    , m_MeshNeedsRegeneration(true)
{

    m_BaseStyleBox = std::make_shared<StyleBoxFlat>();

    m_StateStyles[static_cast<int>(ButtonState::Normal)].modulationColor = Color::White();
    m_StateStyles[static_cast<int>(ButtonState::Hover)].modulationColor = Color(1.2f, 1.2f, 1.2f, 1.0f);
    m_StateStyles[static_cast<int>(ButtonState::Pressed)].modulationColor = Color(0.8f, 0.8f, 0.8f, 1.0f);
    m_StateStyles[static_cast<int>(ButtonState::Disabled)].modulationColor = Color(0.5f, 0.5f, 0.5f, 0.5f);
}

Button::~Button() {

}

void Button::DefineProperties() {

    DefineUIControlProperties(120.0f, 40.0f, "useUISpace", "Size");

    DefineProperty(PROPERTY_INT_RANGE_GROUP(layer, 0, -100, 100, 1, "Rendering"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(sortingOrder, 0, -1000, 1000, 1, "Rendering"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(buttonEnabled, Bool, true, "Button"));
    DefineProperty(PROPERTY_ENUM_GROUP(styleMode, 0, "Button", Automatic, Manual));
    DefineProperty(PROPERTY_ENUM_GROUP(scaleMode, 0, "Button", Fixed, FitToText, FitToTextWidth));

    DefineProperty(PROPERTY_DEFAULT_GROUP(textPadding, Vec2, Vec2(10.0f, 5.0f), "Button"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(backgroundColor, Color, Color(0.3f, 0.3f, 0.3f, 1.0f), "Background"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(opacity, 1.0f, 0.0f, 1.0f, 0.01f, "Background"));
    DefineProperty(PROPERTY_FILE_GROUP(backgroundImagePath, std::string(""), "*.png,*.jpg,*.jpeg,*.bmp,*.tga", "Background"));
    DefineProperty(PROPERTY_ENUM_GROUP(backgroundImageStretchMode, 0, "Background", Stretch, KeepCentered, NineSlice));

    DefineProperty(PROPERTY_DEFAULT_GROUP(nineSliceMargins, Vec4, Vec4(8.0f, 8.0f, 8.0f, 8.0f), "NineSlice"));
    DefineProperty(PROPERTY_ENUM_GROUP(nineSliceAxisH, 0, "NineSlice", Stretch, Tile));
    DefineProperty(PROPERTY_ENUM_GROUP(nineSliceAxisV, 0, "NineSlice", Stretch, Tile));
    DefineProperty(PROPERTY_DEFAULT_GROUP(nineSliceDrawCenter, Bool, true, "NineSlice"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(borderEnabled, Bool, true, "Border"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderWidthLinked, Bool, true, "Border"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderWidth, Vec4, Vec4(2.0f, 2.0f, 2.0f, 2.0f), "Border"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderColor, Color, Color(0.5f, 0.5f, 0.5f, 1.0f), "Border"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(cornerRadiusLinked, Bool, true, "CornerRadius"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(cornerRadius, Vec4, Vec4(4.0f, 4.0f, 4.0f, 4.0f), "CornerRadius"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(text, String, std::string("Button"), "Text"));

    // localizationKey overrides "text" and resolves through the LocalizationManager.
    DefineProperty(PROPERTY_DEFAULT_GROUP(localizationKey, String, std::string(""), "Localization"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(localizationTable, String, std::string(""), "Localization"));
    DefineProperty(PROPERTY_FILE_GROUP(fontPath, std::string(""), "*.ttf,*.otf", "Text"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(fontSize, 16.0f, 1.0f, 256.0f, 1.0f, "Text"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(fontColor, Color, Color::White(), "Text"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(wordWrap, Bool, false, "Text"));
    // Alignment and line spacing. A Button had NO alignment properties at all -- its text was
    // hard-centered on the button's center by a hand-rolled glyph loop, so a left-aligned
    // menu-row button was simply not expressible.
    DefineProperty(PROPERTY_ENUM_GROUP(textHAlign, 1, "Text", Left, Center, Right, Fill));
    DefineProperty(PROPERTY_ENUM_GROUP(textVAlign, 1, "Text", Top, Center, Bottom));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(lineSpacing, 1.0f, 0.1f, 5.0f, 0.05f, "Text"));
    // Line-breaking and overflow, as on Label. `wordWrap` was already advertised here but was
    // never read by the renderer at all -- long text just overflowed the button unclipped.
    DefineProperty(PROPERTY_ENUM_GROUP(autowrapMode, 0, "Text", Off, Arbitrary, Word, WordSmart));
    DefineProperty(PROPERTY_ENUM_GROUP(overrunBehavior, 0, "Text",
        None, TrimChar, TrimWord, EllipsisChar, EllipsisWord));
    DefineProperty(PROPERTY_DEFAULT_GROUP(clipText, Bool, false, "Text"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(tabSize, 4, 1, 16, 1, "Text"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(shadowOffset, Vec2, Vec2(0.0f, 0.0f), "Shadow"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(shadowColor, Color, Color(0.0f, 0.0f, 0.0f, 0.0f), "Shadow"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(normalModulation, Color, Color::White(), "StateModulation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(hoverModulation, Color, Color(1.2f, 1.2f, 1.2f, 1.0f), "StateModulation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(pressedModulation, Color, Color(0.8f, 0.8f, 0.8f, 1.0f), "StateModulation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(disabledModulation, Color, Color(0.5f, 0.5f, 0.5f, 0.5f), "StateModulation"));

    DefineProperty(PROPERTY_FILE_GROUP(normalSoundPath, std::string(""), "*.wav,*.ogg,*.mp3", "StateSounds"));
    DefineProperty(PROPERTY_FILE_GROUP(hoverSoundPath, std::string(""), "*.wav,*.ogg,*.mp3", "StateSounds"));
    DefineProperty(PROPERTY_FILE_GROUP(pressedSoundPath, std::string(""), "*.wav,*.ogg,*.mp3", "StateSounds"));
    DefineProperty(PROPERTY_FILE_GROUP(disabledSoundPath, std::string(""), "*.wav,*.ogg,*.mp3", "StateSounds"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(normalTweenEnabled, Bool, false, "StateTweens_Normal"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(normalTweenScale, Vec2, Vec2(0.0f, 0.0f), "StateTweens_Normal"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(normalTweenRotation, 0.0f, -360.0f, 360.0f, 1.0f, "StateTweens_Normal"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(normalTweenPosition, Vec2, Vec2(0.0f, 0.0f), "StateTweens_Normal"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(normalTweenDuration, 0.2f, 0.0f, 5.0f, 0.01f, "StateTweens_Normal"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(hoverTweenEnabled, Bool, true, "StateTweens_Hover"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(hoverTweenScale, Vec2, Vec2(0.05f, 0.05f), "StateTweens_Hover"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(hoverTweenRotation, 0.0f, -360.0f, 360.0f, 1.0f, "StateTweens_Hover"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(hoverTweenPosition, Vec2, Vec2(0.0f, 0.0f), "StateTweens_Hover"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(hoverTweenDuration, 0.2f, 0.0f, 5.0f, 0.01f, "StateTweens_Hover"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(pressedTweenEnabled, Bool, true, "StateTweens_Pressed"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(pressedTweenScale, Vec2, Vec2(-0.05f, -0.05f), "StateTweens_Pressed"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(pressedTweenRotation, 0.0f, -360.0f, 360.0f, 1.0f, "StateTweens_Pressed"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(pressedTweenPosition, Vec2, Vec2(0.0f, 2.0f), "StateTweens_Pressed"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(pressedTweenDuration, 0.1f, 0.0f, 5.0f, 0.01f, "StateTweens_Pressed"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(disabledTweenEnabled, Bool, false, "StateTweens_Disabled"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(disabledTweenScale, Vec2, Vec2(0.0f, 0.0f), "StateTweens_Disabled"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(disabledTweenRotation, 0.0f, -360.0f, 360.0f, 1.0f, "StateTweens_Disabled"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(disabledTweenPosition, Vec2, Vec2(0.0f, 0.0f), "StateTweens_Disabled"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(disabledTweenDuration, 0.2f, 0.0f, 5.0f, 0.01f, "StateTweens_Disabled"));
}

void Button::OnAwake() {

    Vec4 cornerRadiusVec = GetPropertyValue<Vec4>("cornerRadius");
    bool cornerRadiusLinked = GetPropertyValue<bool>("cornerRadiusLinked");
    m_CornerRadius.FromVec4(cornerRadiusVec);
    m_CornerRadius.SetLinked(cornerRadiusLinked);

    Vec4 borderWidthVec = GetPropertyValue<Vec4>("borderWidth");
    bool borderWidthLinked = GetPropertyValue<bool>("borderWidthLinked");
    m_BorderWidth.FromVec4(borderWidthVec);
    m_BorderWidth.SetLinked(borderWidthLinked);

    LoadThemedStateData();

    auto loadSound = [this](ButtonState state, const std::string& prefix) {
        std::string path = GetPropertyValue<std::string>(prefix + "SoundPath");
        if (!path.empty()) {
            SetStateSoundPath(state, path);
        }
    };

    loadSound(ButtonState::Normal, "normal");
    loadSound(ButtonState::Hover, "hover");
    loadSound(ButtonState::Pressed, "pressed");
    loadSound(ButtonState::Disabled, "disabled");

    std::string fontPath = GetFontPath();
    if (!fontPath.empty()) {

        m_CurrentFontPath = fontPath;
    }

    m_ButtonEnabled = GetPropertyValue<bool>("buttonEnabled");
    m_CurrentState = m_ButtonEnabled ? ButtonState::Normal : ButtonState::Disabled;
    m_StyleMode = static_cast<ButtonStyleMode>(GetPropertyValue<int>("styleMode"));
    m_ScaleMode = static_cast<ButtonScaleMode>(GetPropertyValue<int>("scaleMode"));
}

void Button::OnReady() {
    m_MeshNeedsRegeneration = true;
}

void Button::OnInput(float deltaTime) {

    if (!IsEnabled()) {
        return;
    }

    UpdateMouseInteraction(deltaTime);
}

void Button::OnUpdate(float deltaTime) {
    UIControl::OnUpdate(deltaTime);

    if (!IsEnabled()) {
        return;
    }

    SyncFromProperties();

    if (m_IsTweening && m_TweenProgress < 1.0f) {
        const ButtonStateTween& targetTween = m_StateTweens[static_cast<int>(m_CurrentState)];
        m_TweenProgress += deltaTime / targetTween.duration;

        if (m_TweenProgress >= 1.0f) {
            m_TweenProgress = 1.0f;
            m_IsTweening = false;
        }

        float t = m_TweenProgress;
        m_CurrentScaleOffset = m_TweenStartScaleOffset.Lerp(targetTween.scaleOffset, t);
        m_CurrentRotationOffset = Lerp(m_TweenStartRotationOffset, targetTween.rotationOffset, t);
        m_CurrentPositionOffset = m_TweenStartPositionOffset.Lerp(targetTween.positionOffset, t);
    }
}

void Button::OnRender() {

}

bool Button::OnGizmoScale(float scaleDelta, int axis, bool is3D) {
    // A FitToText button derives its size from its label, so a manual resize has nothing to
    // write to and is refused rather than accepted and discarded.
    if (m_ScaleMode != ButtonScaleMode::Fixed) {
        return false;
    }

    // The base writes whichever property actually drives the axis: width/height when
    // point-anchored, the offsets when anchor-stretched, and nothing when a parent container
    // owns the rect.
    const bool handled = UIControl::OnGizmoScale(scaleDelta, axis, is3D);
    if (handled) {
        m_MeshNeedsRegeneration = true;
    }
    return handled;
}

float Button::GetWidth() const {
    if (m_ScaleMode != ButtonScaleMode::Fixed) {
        return CalculateButtonSize().x;
    }
    return GetPropertyValue<float>("width");
}

void Button::SetWidth(float width) {
    SetPropertyValue<float>("width", width);
    m_MeshNeedsRegeneration = true;
}

float Button::GetHeight() const {
    if (m_ScaleMode == ButtonScaleMode::FitToText) {
        return CalculateButtonSize().y;
    }
    return GetPropertyValue<float>("height");
}

void Button::SetHeight(float height) {
    SetPropertyValue<float>("height", height);
    m_MeshNeedsRegeneration = true;
}

int Button::GetLayer() const {
    return GetPropertyValue<int>("layer");
}

void Button::SetLayer(int layer) {
    SetPropertyValue<int>("layer", layer);
}

int Button::GetSortingOrder() const {
    return GetPropertyValue<int>("sortingOrder");
}

void Button::SetSortingOrder(int order) {
    SetPropertyValue<int>("sortingOrder", order);
}

void Button::SetEnabled(bool enabled) {
    m_ButtonEnabled = enabled;
    SetPropertyValue<bool>("buttonEnabled", enabled);

    if (!enabled && m_CurrentState != ButtonState::Disabled) {
        TransitionToState(ButtonState::Disabled);
    } else if (enabled && m_CurrentState == ButtonState::Disabled) {
        TransitionToState(ButtonState::Normal);
    }
}

void Button::SetStyleMode(ButtonStyleMode mode) {
    m_StyleMode = mode;
    SetPropertyValue<int>("styleMode", static_cast<int>(mode));
    m_MeshNeedsRegeneration = true;
}

void Button::SetScaleMode(ButtonScaleMode mode) {
    m_ScaleMode = mode;
    SetPropertyValue<int>("scaleMode", static_cast<int>(mode));
    m_MeshNeedsRegeneration = true;
}

void Button::SetTextPadding(const Vec2& padding) {
    m_TextPadding = padding;
    SetPropertyValue<Vec2>("textPadding", padding);
    m_MeshNeedsRegeneration = true;
}

void Button::SetBaseStyleBox(std::shared_ptr<StyleBox> styleBox) {
    m_BaseStyleBox = styleBox;
    m_MeshNeedsRegeneration = true;
}

const std::vector<UIControl::ThemeBinding>& Button::GetThemeBindings() const {
    static const std::vector<ThemeBinding> kBindings = []() {
        std::vector<ThemeBinding> b = {
            { "backgroundColor",     "background",       ThemeBinding::Kind::Color },
            { "backgroundImagePath", "background_image", ThemeBinding::Kind::Image },
            { "borderColor",     "border_color",  ThemeBinding::Kind::Color },
            { "fontColor",       "font_color",    ThemeBinding::Kind::Color },
            { "fontPath",        "font",          ThemeBinding::Kind::Font },
            { "fontSize",        "font_size",     ThemeBinding::Kind::Constant },
            { "cornerRadius",    "corner_radius", ThemeBinding::Kind::Constant },
        };
        // Per-state modulation + tween: the entry name matches the property name.
        const char* states[] = { "normal", "hover", "pressed", "disabled" };
        for (const char* s : states) {
            std::string p(s);
            b.push_back({ p + "Modulation",    p + "Modulation",    ThemeBinding::Kind::Color });
            b.push_back({ p + "TweenEnabled",  p + "TweenEnabled",  ThemeBinding::Kind::Bool });
            b.push_back({ p + "TweenScale",    p + "TweenScale",    ThemeBinding::Kind::Vec2 });
            b.push_back({ p + "TweenRotation", p + "TweenRotation", ThemeBinding::Kind::Constant });
            b.push_back({ p + "TweenPosition", p + "TweenPosition", ThemeBinding::Kind::Vec2 });
            b.push_back({ p + "TweenDuration", p + "TweenDuration", ThemeBinding::Kind::Constant });
        }
        return b;
    }();
    return kBindings;
}

Color Button::GetBackgroundColor() const {
    return ResolveThemedColor("backgroundColor", "background");
}

void Button::SetBackgroundColor(const Color& color) {
    SetThemedProperty<Color>("backgroundColor", color);
    if (auto flatStyle = std::dynamic_pointer_cast<StyleBoxFlat>(m_BaseStyleBox)) {
        flatStyle->SetBackgroundColor(color);
    }
    m_MeshNeedsRegeneration = true;
}

float Button::GetOpacity() const {
    return GetPropertyValue<float>("opacity");
}

void Button::SetOpacity(float opacity) {
    SetPropertyValue<float>("opacity", opacity);
    if (auto flatStyle = std::dynamic_pointer_cast<StyleBoxFlat>(m_BaseStyleBox)) {
        flatStyle->SetOpacity(opacity);
    }
    m_MeshNeedsRegeneration = true;
}

std::string Button::GetBackgroundImagePath() const {
    return ResolveThemedImage("backgroundImagePath", "background_image");
}

void Button::SetBackgroundImagePath(const std::string& path) {
    std::string resPath = path;
    if (!path.empty() && !(path.size() >= 6 && path.substr(0, 6) == "res://")) {
        asset::AssetDatabase& assetDb = asset::AssetDatabase::GetInstance();
        if (assetDb.IsInitialized()) {
            std::string converted = assetDb.ToResourcePath(path);
            if (!converted.empty()) {
                resPath = converted;
            }
        }
    }
    SetThemedProperty<std::string>("backgroundImagePath", resPath);
    m_MeshNeedsRegeneration = true;
}

UIImageStretchMode Button::GetBackgroundImageStretchMode() const {
    return UIImageStretchModeFromInt(GetPropertyValue<int>("backgroundImageStretchMode"));
}

void Button::SetBackgroundImageStretchMode(UIImageStretchMode mode) {
    SetPropertyValue<int>("backgroundImageStretchMode", static_cast<int>(mode));
    m_MeshNeedsRegeneration = true;
}

Vec4 Button::GetNineSliceMargins() const {
    return GetPropertyValue<Vec4>("nineSliceMargins");
}

void Button::SetNineSliceMargins(const Vec4& margins) {
    SetPropertyValue<Vec4>("nineSliceMargins", margins);
    m_MeshNeedsRegeneration = true;
}

UINineSliceAxisMode Button::GetNineSliceAxisHorizontal() const {
    return UINineSliceAxisModeFromInt(GetPropertyValue<int>("nineSliceAxisH"));
}

void Button::SetNineSliceAxisHorizontal(UINineSliceAxisMode mode) {
    SetPropertyValue<int>("nineSliceAxisH", static_cast<int>(mode));
    m_MeshNeedsRegeneration = true;
}

UINineSliceAxisMode Button::GetNineSliceAxisVertical() const {
    return UINineSliceAxisModeFromInt(GetPropertyValue<int>("nineSliceAxisV"));
}

void Button::SetNineSliceAxisVertical(UINineSliceAxisMode mode) {
    SetPropertyValue<int>("nineSliceAxisV", static_cast<int>(mode));
    m_MeshNeedsRegeneration = true;
}

bool Button::GetNineSliceDrawCenter() const {
    return GetPropertyValue<bool>("nineSliceDrawCenter");
}

void Button::SetNineSliceDrawCenter(bool drawCenter) {
    SetPropertyValue<bool>("nineSliceDrawCenter", drawCenter);
    m_MeshNeedsRegeneration = true;
}

bool Button::GetBorderWidthLinked() const {
    return m_BorderWidth.IsLinked();
}

void Button::SetBorderWidthLinked(bool linked) {
    m_BorderWidth.SetLinked(linked);
    SetPropertyValue<bool>("borderWidthLinked", linked);
}

float Button::GetBorderWidthLeft() const {
    return m_BorderWidth.Get(3);
}

void Button::SetBorderWidthLeft(float width) {
    m_BorderWidth.Set(3, width);
    SetPropertyValue<Vec4>("borderWidth", m_BorderWidth.AsVec4());
    m_MeshNeedsRegeneration = true;
}

float Button::GetBorderWidthRight() const {
    return m_BorderWidth.Get(1);
}

void Button::SetBorderWidthRight(float width) {
    m_BorderWidth.Set(1, width);
    SetPropertyValue<Vec4>("borderWidth", m_BorderWidth.AsVec4());
    m_MeshNeedsRegeneration = true;
}

float Button::GetBorderWidthTop() const {
    return m_BorderWidth.Get(0);
}

void Button::SetBorderWidthTop(float width) {
    m_BorderWidth.Set(0, width);
    SetPropertyValue<Vec4>("borderWidth", m_BorderWidth.AsVec4());
    m_MeshNeedsRegeneration = true;
}

float Button::GetBorderWidthBottom() const {
    return m_BorderWidth.Get(2);
}

void Button::SetBorderWidthBottom(float width) {
    m_BorderWidth.Set(2, width);
    SetPropertyValue<Vec4>("borderWidth", m_BorderWidth.AsVec4());
    m_MeshNeedsRegeneration = true;
}

bool Button::GetBorderEnabled() const {
    return GetPropertyValue<bool>("borderEnabled");
}

void Button::SetBorderEnabled(bool enabled) {
    SetPropertyValue<bool>("borderEnabled", enabled);
    m_MeshNeedsRegeneration = true;
}

Color Button::GetBorderColor() const {
    return ResolveThemedColor("borderColor", "border_color");
}

void Button::SetBorderColor(const Color& color) {
    SetThemedProperty<Color>("borderColor", color);
    m_MeshNeedsRegeneration = true;
}

bool Button::GetCornerRadiusLinked() const {
    return m_CornerRadius.IsLinked();
}

void Button::SetCornerRadiusLinked(bool linked) {
    m_CornerRadius.SetLinked(linked);
    SetPropertyValue<bool>("cornerRadiusLinked", linked);
}

float Button::GetCornerRadiusTopLeft() const {
    return m_CornerRadius.Get(0);
}

void Button::SetCornerRadiusTopLeft(float radius) {
    m_CornerRadius.Set(0, radius);
    SetThemedProperty<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
    m_MeshNeedsRegeneration = true;
}

float Button::GetCornerRadiusTopRight() const {
    return m_CornerRadius.Get(1);
}

void Button::SetCornerRadiusTopRight(float radius) {
    m_CornerRadius.Set(1, radius);
    SetThemedProperty<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
    m_MeshNeedsRegeneration = true;
}

float Button::GetCornerRadiusBottomLeft() const {
    return m_CornerRadius.Get(3);
}

void Button::SetCornerRadiusBottomLeft(float radius) {
    m_CornerRadius.Set(3, radius);
    SetThemedProperty<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
    m_MeshNeedsRegeneration = true;
}

float Button::GetCornerRadiusBottomRight() const {
    return m_CornerRadius.Get(2);
}

void Button::SetCornerRadiusBottomRight(float radius) {
    m_CornerRadius.Set(2, radius);
    SetThemedProperty<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
    m_MeshNeedsRegeneration = true;
}

std::string Button::GetText() const {
    std::string locKey = GetPropertyValue<std::string>("localizationKey");
    if (!locKey.empty()) {
        return localization::LocalizationManager::GetInstance().Translate(
            locKey, GetPropertyValue<std::string>("localizationTable"));
    }
    return GetPropertyValue<std::string>("text");
}

void Button::SetText(const std::string& text) {
    SetPropertyValue<std::string>("text", text);
    m_MeshNeedsRegeneration = true;
    m_TextMeshNeedsRegeneration = true;
}

std::string Button::GetFontPath() const {
    return ResolveThemedFontPath("fontPath", "font");
}

void Button::SetFontPath(const std::string& path) {
    SetThemedProperty<std::string>("fontPath", path);
    m_CurrentFontPath = path;
    m_MeshNeedsRegeneration = true;
}

float Button::GetFontSize() const {
    return ResolveThemedFontSize("fontSize", "font_size", "font");
}

void Button::SetFontSize(float size) {
    SetThemedProperty<float>("fontSize", size);
    m_CurrentFontSize = size;
    m_MeshNeedsRegeneration = true;
    m_TextMeshNeedsRegeneration = true;
}

Color Button::GetFontColor() const {
    return ResolveThemedColor("fontColor", "font_color");
}

void Button::SetFontColor(const Color& color) {
    SetThemedProperty<Color>("fontColor", color);
    m_MeshNeedsRegeneration = true;
    m_TextMeshNeedsRegeneration = true;
}

bool Button::GetWordWrap() const {
    return GetPropertyValue<bool>("wordWrap");
}

void Button::SetWordWrap(bool wrap) {
    SetPropertyValue<bool>("wordWrap", wrap);
    m_MeshNeedsRegeneration = true;
}

const Color& Button::GetStateModulation(ButtonState state) const {
    return m_StateStyles[static_cast<int>(state)].modulationColor;
}

void Button::SetStateModulation(ButtonState state, const Color& color) {
    m_StateStyles[static_cast<int>(state)].modulationColor = color;

    std::string propName;
    switch (state) {
        case ButtonState::Normal: propName = "normalModulation"; break;
        case ButtonState::Hover: propName = "hoverModulation"; break;
        case ButtonState::Pressed: propName = "pressedModulation"; break;
        case ButtonState::Disabled: propName = "disabledModulation"; break;
        default: return;
    }
    SetPropertyValue<Color>(propName, color);
    m_MeshNeedsRegeneration = true;
}

std::shared_ptr<StyleBox> Button::GetStateStyleBox(ButtonState state) const {
    return m_StateStyles[static_cast<int>(state)].styleBox;
}

void Button::SetStateStyleBox(ButtonState state, std::shared_ptr<StyleBox> styleBox) {
    m_StateStyles[static_cast<int>(state)].styleBox = styleBox;
    m_MeshNeedsRegeneration = true;
}

const ButtonStateSound& Button::GetStateSound(ButtonState state) const {
    return m_StateSounds[static_cast<int>(state)];
}

void Button::SetStateSound(ButtonState state, const ButtonStateSound& sound) {
    m_StateSounds[static_cast<int>(state)] = sound;
}

void Button::SetStateSoundPath(ButtonState state, const std::string& path) {
    int idx = static_cast<int>(state);
    m_StateSounds[idx].audioPath = path;

    if (!path.empty()) {

    }

    std::string propName;
    switch (state) {
        case ButtonState::Normal: propName = "normalSoundPath"; break;
        case ButtonState::Hover: propName = "hoverSoundPath"; break;
        case ButtonState::Pressed: propName = "pressedSoundPath"; break;
        case ButtonState::Disabled: propName = "disabledSoundPath"; break;
        default: return;
    }
    SetPropertyValue<std::string>(propName, path);
}

const ButtonStateTween& Button::GetStateTween(ButtonState state) const {
    return m_StateTweens[static_cast<int>(state)];
}

void Button::SetStateTween(ButtonState state, const ButtonStateTween& tween) {
    m_StateTweens[static_cast<int>(state)] = tween;
}

void Button::SetStateCallback(ButtonState state, StateCallback callback) {
    m_StateCallbacks[state] = callback;
}

void Button::ClearStateCallback(ButtonState state) {
    m_StateCallbacks.erase(state);
}

void Button::SyncFromProperties() {

    Vec4 cornerRadiusVec = GetPropertyValue<Vec4>("cornerRadius");
    bool cornerRadiusLinked = GetPropertyValue<bool>("cornerRadiusLinked");
    m_CornerRadius.FromVec4(cornerRadiusVec);
    m_CornerRadius.SetLinked(cornerRadiusLinked);

    Vec4 borderWidthVec = GetPropertyValue<Vec4>("borderWidth");
    bool borderWidthLinked = GetPropertyValue<bool>("borderWidthLinked");
    m_BorderWidth.FromVec4(borderWidthVec);
    m_BorderWidth.SetLinked(borderWidthLinked);

    LoadThemedStateData();

    m_ButtonEnabled = GetPropertyValue<bool>("buttonEnabled");
    m_StyleMode = static_cast<ButtonStyleMode>(GetPropertyValue<int>("styleMode"));
    m_ScaleMode = static_cast<ButtonScaleMode>(GetPropertyValue<int>("scaleMode"));
}

void Button::LoadThemedStateData() {
    struct StateMap { ButtonState state; const char* prefix; };
    static const StateMap kStates[] = {
        { ButtonState::Normal,   "normal" },
        { ButtonState::Hover,    "hover" },
        { ButtonState::Pressed,  "pressed" },
        { ButtonState::Disabled, "disabled" },
    };
    for (const StateMap& sm : kStates) {
        int idx = static_cast<int>(sm.state);
        std::string p(sm.prefix);
        m_StateStyles[idx].modulationColor = ResolveThemedColor(p + "Modulation", p + "Modulation");
        m_StateTweens[idx].enabled        = ResolveThemedBool(p + "TweenEnabled", p + "TweenEnabled");
        m_StateTweens[idx].scaleOffset    = ResolveThemedVec2(p + "TweenScale", p + "TweenScale");
        m_StateTweens[idx].rotationOffset = ResolveThemedConstant(p + "TweenRotation", p + "TweenRotation");
        m_StateTweens[idx].positionOffset = ResolveThemedVec2(p + "TweenPosition", p + "TweenPosition");
        m_StateTweens[idx].duration       = ResolveThemedConstant(p + "TweenDuration", p + "TweenDuration");
    }
}

void Button::UpdateMouseInteraction(float) {

    if (!m_ButtonEnabled) {
        if (m_CurrentState != ButtonState::Disabled) {
            TransitionToState(ButtonState::Disabled);
        }
        return;
    }

    input::InputManager& inputMgr = input::InputManager::Get();

    // Mouse in the centered canvas space the control rects live in, so hit-testing
    // matches rendering on every backend (shared helper).
    Vec2 mousePos = GetCanvasMousePosition();

    // Only the front-most button under the cursor handles the click; a button behind
    // another sees isOver=false so a single click never passes through to it.
    bool isOver = IsMouseOver(mousePos) && IsTopPointerTarget(mousePos);

    bool leftPressed = inputMgr.IsMouseButtonPressed(input::MouseButton::Left);

    ButtonState newState = m_CurrentState;

    if (isOver) {
        if (leftPressed) {
            newState = ButtonState::Pressed;
        } else {
            newState = ButtonState::Hover;
        }
    } else if (leftPressed && m_CurrentState == ButtonState::Pressed) {
        newState = ButtonState::Pressed;
    } else if (HasFocus()) {
        // Keyboard / gamepad focus lights the button with its Hover (focus) visual
        // so directional navigation shows the same highlight a mouse hover would.
        newState = ButtonState::Hover;
    } else {
        newState = ButtonState::Normal;
    }

    if (newState != m_CurrentState) {

        TransitionToState(newState);
    }

    m_MouseWasOver = isOver;
}

bool Button::IsMouseOver(const Vec2& mousePos) const {
    if (!GetOwner()) {
        return false;
    }

    core::Node2D* node2D = dynamic_cast<core::Node2D*>(GetOwner());
    if (!node2D) {
        return false;
    }

    Vec2 position = node2D->GetGlobalPosition();
    Vec2 size = GetEffectiveSize();

    position += m_CurrentPositionOffset;

    // Position is center-pivot, so calculate bounds from center
    Vec2 halfSize = size * 0.5f;
    return mousePos.x >= position.x - halfSize.x && mousePos.x <= position.x + halfSize.x &&
           mousePos.y >= position.y - halfSize.y && mousePos.y <= position.y + halfSize.y;
}

void Button::TransitionToState(ButtonState newState) {
    if (newState == m_CurrentState) {
        return;
    }

    m_PreviousState = m_CurrentState;
    m_CurrentState = newState;

    PlayStateSound(newState);

    const ButtonStateTween& tween = m_StateTweens[static_cast<int>(newState)];
    if (tween.enabled) {
        m_TweenStartScaleOffset = m_CurrentScaleOffset;
        m_TweenStartRotationOffset = m_CurrentRotationOffset;
        m_TweenStartPositionOffset = m_CurrentPositionOffset;

        m_IsTweening = true;
        m_TweenProgress = 0.0f;
    } else {
        // Snap to target immediately
        m_CurrentScaleOffset = tween.scaleOffset;
        m_CurrentRotationOffset = tween.rotationOffset;
        m_CurrentPositionOffset = tween.positionOffset;
        m_IsTweening = false;
    }

    InvokeStateCallback(newState);

    // Emit named signals alongside the legacy state callbacks so the transition
    // is observable from scripts/editor connections.
    Emit("state_changed", { static_cast<int>(newState) });
    if (newState == ButtonState::Pressed) {
        Emit("pressed");
    } else if (m_PreviousState == ButtonState::Pressed) {
        Emit("released");
    }
    if (newState == ButtonState::Hover) {
        Emit("hovered");
    }

    m_MeshNeedsRegeneration = true;
    m_TextMeshNeedsRegeneration = true;
}

void Button::DefineSignals() {
    RegisterSignal({"pressed", {}, "Emitted when the button is pressed down."});
    RegisterSignal({"released", {}, "Emitted when a press is released."});
    RegisterSignal({"hovered", {}, "Emitted when the pointer enters the button."});
    RegisterSignal({"state_changed",
                    {{"state", core::PropertyValueType::Int}},
                    "Emitted on any button state change (Normal/Hover/Pressed/Disabled)."});
}

void Button::PlayStateSound(ButtonState state) {
    const ButtonStateSound& sound = m_StateSounds[static_cast<int>(state)];

    if (sound.audioPath.empty()) {
        return;
    }

    audio::AudioManager& audioMgr = audio::AudioManager::GetInstance();

    if (sound.audioAsset && sound.audioAsset->IsLoaded()) {
        audioMgr.Play(sound.audioAsset, sound.busName, audio::PlaybackMode::OneShot, sound.volume);
    }
}

void Button::ApplyStateTween(ButtonState state, bool entering) {
    const ButtonStateTween& tween = m_StateTweens[static_cast<int>(state)];

    if (!tween.enabled) {
        return;
    }

    if (entering) {
        m_IsTweening = true;
        m_TweenProgress = 0.0f;
    } else {

        m_IsTweening = true;
        m_TweenProgress = 0.0f;
    }
}

void Button::InvokeStateCallback(ButtonState state) {
    auto it = m_StateCallbacks.find(state);
    if (it != m_StateCallbacks.end() && it->second) {
        it->second();
    }
}

void Button::PreviewTween(ButtonState state) {
    const ButtonStateTween& tween = m_StateTweens[static_cast<int>(state)];

    if (!tween.enabled) {

        m_CurrentScaleOffset = Vec2(0.0f, 0.0f);
        m_CurrentRotationOffset = 0.0f;
        m_CurrentPositionOffset = Vec2(0.0f, 0.0f);
        return;
    }

    m_CurrentScaleOffset = tween.scaleOffset;
    m_CurrentRotationOffset = tween.rotationOffset;
    m_CurrentPositionOffset = tween.positionOffset;
}

Vec2 Button::CalculateButtonSize() const {
    Vec2 baseSize(GetPropertyValue<float>("width"), GetPropertyValue<float>("height"));

    if (m_ScaleMode == ButtonScaleMode::Fixed) {
        return baseSize;
    }

    Vec2 textSize = CalculateTextSize();
    Vec2 padding = m_TextPadding * 2.0f;

    if (m_ScaleMode == ButtonScaleMode::FitToText) {
        return textSize + padding;
    } else if (m_ScaleMode == ButtonScaleMode::FitToTextWidth) {
        return Vec2(textSize.x + padding.x, baseSize.y);
    }

    return baseSize;
}

Vec2 Button::GetEffectiveSize() const {
    Vec2 resolved = GetResolvedRect().size;
    bool resolvedValid = (resolved.x > 0.0f || resolved.y > 0.0f);

    switch (m_ScaleMode) {
        case ButtonScaleMode::Fixed:
            // Follow the resolved (anchor-driven) rect so the button stretches with anchors.
            return resolvedValid ? resolved : CalculateButtonSize();
        case ButtonScaleMode::FitToTextWidth: {
            // Width is content-driven; height follows the resolved rect (vertical anchors stretch).
            Vec2 textFit = CalculateButtonSize();
            return resolvedValid ? Vec2(textFit.x, resolved.y) : textFit;
        }
        case ButtonScaleMode::FitToText:
        default:
            // Content-driven on both axes: "fit to text" overrides anchor stretching.
            return CalculateButtonSize();
    }
}

Vec2 Button::CalculateTextSize() const {
    if (!m_FontAsset.IsValid() || !m_FontAsset->IsLoaded()) {
        return Vec2(0.0f, 0.0f);
    }

    // Measured through the same layout engine that positions the glyphs, so word wrap, line
    // spacing and multiline are all accounted for exactly as drawn.
    //
    // Only the wrap width comes from the resolved rect (minus the content margins): reading
    // the box HEIGHT here would recurse, since GetBoundsSize() -> GetMinSize() ->
    // GetContentMinSize() lands back in this function.
    const Vec4 margins = GetContentMargins();
    const float available = GetResolvedRect().size.x - (margins.w + margins.y);

    const Vec2 measureBox(std::max(0.0f, available), 0.0f);
    TextLayoutParams params = BuildLayoutParams(measureBox);

    return TextLayout::Measure(GetText(), m_FontAsset->GetMetricsAtlas(), params);
}

TextHAlign Button::GetTextHAlign() const {
    return static_cast<TextHAlign>(GetPropertyValue<int>("textHAlign"));
}

void Button::SetTextHAlign(TextHAlign align) {
    SetPropertyValue<int>("textHAlign", static_cast<int>(align));
    m_TextMeshNeedsRegeneration = true;
}

TextVAlign Button::GetTextVAlign() const {
    return static_cast<TextVAlign>(GetPropertyValue<int>("textVAlign"));
}

void Button::SetTextVAlign(TextVAlign align) {
    SetPropertyValue<int>("textVAlign", static_cast<int>(align));
    m_TextMeshNeedsRegeneration = true;
}

float Button::GetLineSpacing() const {
    return GetPropertyValue<float>("lineSpacing");
}

void Button::SetLineSpacing(float spacing) {
    SetPropertyValue<float>("lineSpacing", spacing);
    m_TextMeshNeedsRegeneration = true;
}

Vec2 Button::GetContentMinSize() const {
    // A button's minimum content is its label plus the content margins on all four sides --
    // which is textPadding, the themed StyleBox's content margins, or whichever of the two is
    // larger per side. Reading textPadding directly (as this used to) ignored a theme that
    // asked for room around the label.
    Vec2 textSize = CalculateTextSize();
    const Vec4 margins = GetContentMargins();

    textSize.x += margins.w + margins.y;   // left + right
    textSize.y += margins.x + margins.z;   // top + bottom

    return textSize;
}

std::shared_ptr<StyleBoxFlat> Button::GetEffectiveStyle() const {
    if (m_StyleMode == ButtonStyleMode::Manual) {

        auto stateStyle = m_StateStyles[static_cast<int>(m_CurrentState)].styleBox;
        if (stateStyle) {
            return std::dynamic_pointer_cast<StyleBoxFlat>(stateStyle);
        }
    }

    return std::dynamic_pointer_cast<StyleBoxFlat>(m_BaseStyleBox);
}

Color Button::GetEffectiveColor(const Color& baseColor) const {
    if (m_StyleMode == ButtonStyleMode::Automatic) {

        const Color& modulation = m_StateStyles[static_cast<int>(m_CurrentState)].modulationColor;
        return Color(
            baseColor.r * modulation.r,
            baseColor.g * modulation.g,
            baseColor.b * modulation.b,
            baseColor.a * modulation.a
        );
    }

    return baseColor;
}

void Button::buildDrawCommands(RenderContext& ctx) {
    if (!GetOwner()) {
        return;
    }

    core::Node2D* node2D = dynamic_cast<core::Node2D*>(GetOwner());
    if (!node2D) {
        return;
    }

    // A resolution change re-bakes the font atlas at a new oversampling density,
    // which repacks glyph UVs; regenerate the cached text mesh so it matches.
    if (ConsumeFontOversampleChanged()) {
        m_TextMeshNeedsRegeneration = true;
    }

    Vec4 cornerRadiusVec = GetPropertyValue<Vec4>("cornerRadius");
    bool cornerRadiusLinked = GetPropertyValue<bool>("cornerRadiusLinked");
    if (cornerRadiusVec != m_CornerRadius.AsVec4() || cornerRadiusLinked != m_CornerRadius.IsLinked()) {
        m_MeshNeedsRegeneration = true;
    }
    m_CornerRadius.FromVec4(cornerRadiusVec);
    m_CornerRadius.SetLinked(cornerRadiusLinked);

    // A theme may supply a uniform corner radius (applied only when defined and the
    // property is not a local override).
    if (!IsThemeOverridden("cornerRadius")) {
        const ui::ThemeAsset* theme = GetEffectiveTheme();
        ui::ThemeManager& tm = ui::ThemeManager::GetInstance();
        if (tm.HasConstant(theme, GetThemeTypeName(), GetThemeTypeVariation(), "corner_radius")) {
            float r = tm.ResolveConstant(theme, GetThemeTypeName(), GetThemeTypeVariation(), "corner_radius", 0.0f);
            m_CornerRadius.FromVec4(Vec4(r, r, r, r));
            m_CornerRadius.SetLinked(true);
        }
    }

    Vec4 borderWidthVec = GetPropertyValue<Vec4>("borderWidth");
    bool borderWidthLinked = GetPropertyValue<bool>("borderWidthLinked");
    if (borderWidthVec != m_BorderWidth.AsVec4() || borderWidthLinked != m_BorderWidth.IsLinked()) {
        m_MeshNeedsRegeneration = true;
    }
    m_BorderWidth.FromVec4(borderWidthVec);
    m_BorderWidth.SetLinked(borderWidthLinked);

    std::string currentText = GetText();
    float currentFontSize = GetFontSize();
    Vec2 currentTextPadding = GetPropertyValue<Vec2>("textPadding");
    Color currentFontColor = GetEffectiveColor(GetFontColor());
    Vec2 currentPosition = node2D->GetGlobalPosition();

    if (currentText != m_CachedText || currentFontSize != m_CachedFontSize) {
        m_TextMeshNeedsRegeneration = true;
    }
    if (currentTextPadding != m_TextPadding) {
        m_TextPadding = currentTextPadding;
        m_TextMeshNeedsRegeneration = true;
    }
    if (currentFontColor != m_CachedFontColor) {
        m_CachedFontColor = currentFontColor;
        m_TextMeshNeedsRegeneration = true;
    }
    if (currentPosition != m_CachedPosition) {
        m_TextMeshNeedsRegeneration = true;
    }

    std::string fontPath = GetFontPath();

    // Fall back to the project's default font when none is set, matching Label so a
    // button with no explicit font still renders its text.
    if (fontPath.empty()) {
        asset::AssetDatabase& assetDb = asset::AssetDatabase::GetInstance();
        if (assetDb.IsInitialized()) {
            fontPath = assetDb.GetDefaultFontPath();
        }
    }

    bool fontPathChanged = (fontPath != m_CurrentFontPath);
    bool fontSizeChanged = (currentFontSize != m_CurrentFontSize);

    if (fontPathChanged || fontSizeChanged) {

        m_CurrentFontPath = fontPath;
        m_CurrentFontSize = currentFontSize;

        if (m_FontHandle.isValid()) {
            m_FontHandle = FontHandle();
        }

        m_FontAsset.Reset();

        if (!fontPath.empty()) {

            m_FontAsset = asset::AssetRef<asset::FontAsset>(new asset::FontAsset());

            uint32_t minAtlasSize = static_cast<uint32_t>(currentFontSize * 10.0f);
            uint32_t atlasSize = 512;
            while (atlasSize < minAtlasSize && atlasSize < 4096) {
                atlasSize *= 2;
            }

            bool loaded = m_FontAsset->LoadFromFile(fontPath, currentFontSize, atlasSize, atlasSize);
            if (!loaded) {
                m_FontAsset.Reset();
            } else {
                // The label is measured with this font, and the font arrives lazily -- on
                // the first draw, i.e. after any parent container has already measured and
                // arranged this button against a fontless estimate. Tell it to re-measure.
                NotifyContentSizeChanged();
            }
        }
    }

    if (m_FontAsset.IsValid() && m_FontAsset->IsLoaded() && !m_FontHandle.isValid()) {

        FontDesc fontDesc;
        // Use GetPhysicalPath() to resolve res:// path to filesystem path for file loading
        fontDesc.fontPath = m_FontAsset->GetPhysicalPath();
        fontDesc.fontSize = currentFontSize;
        fontDesc.atlasWidth = m_FontAsset->GetAtlasWidth();
        fontDesc.atlasHeight = m_FontAsset->GetAtlasHeight();

        IGfxDevice* device = ctx.getDevice();
        if (device) {
            m_FontHandle = device->createFontAtlas(fontDesc);
            if (m_FontHandle.isValid()) {

            } else {

            }
        }
    }

    Vec2 position = GetResolvedRect().GetCenter();
    Vec2 size = GetEffectiveSize();

    position += m_CurrentPositionOffset;
    Vec2 scale = Vec2(1.0f, 1.0f) + m_CurrentScaleOffset;
    float rotation = node2D->GetGlobalRotation() + m_CurrentRotationOffset;

    RenderBackground(ctx, position, size, rotation);

    RenderText(ctx, position, size);
}

void Button::RenderBackground(RenderContext& ctx, const Vec2& position, const Vec2& size, float rotation) {
    auto effectiveStyle = GetEffectiveStyle();
    if (!effectiveStyle) {
        return;
    }

    // A themed StyleBox entry for the current interaction state (Godot Button parity:
    // "normal"/"hover"/"pressed"/"disabled"), when the effective theme defines one,
    // fully replaces the flat property-driven background+border for that state. It is
    // tinted by the state's modulation colour and the control's opacity, and the
    // background image still paints over it.
    if (std::shared_ptr<StyleBox> themedBox = ResolveThemedStyleBox(ButtonStateThemeEntry(m_CurrentState))) {
        Color modulate = GetEffectiveColor(Color::White());
        modulate.a *= GetOpacity();
        DrawStyleBox(ctx, themedBox.get(), position, size, rotation, modulate);
        RenderBackgroundImage(ctx, position, size, rotation, modulate);
        return;
    }

    Color bgColor = GetEffectiveColor(GetBackgroundColor());
    bgColor.a *= GetOpacity();

    Vec4 cornerRadiusVec = m_CornerRadius.AsVec4();

    if (GetBorderEnabled()) {

        float borderTop = m_BorderWidth.Get(0);
        float borderRight = m_BorderWidth.Get(1);
        float borderBottom = m_BorderWidth.Get(2);
        float borderLeft = m_BorderWidth.Get(3);

        if (borderTop > 0.0f || borderRight > 0.0f || borderBottom > 0.0f || borderLeft > 0.0f) {

            Color borderColor = GetEffectiveColor(GetBorderColor());

            Vec4 innerRadius = cornerRadiusVec;

            Vec2 outerSize = Vec2(size.x + borderLeft + borderRight, size.y + borderTop + borderBottom);

            Vec4 outerRadius = Vec4(
                innerRadius.x + std::max(borderTop, borderLeft),
                innerRadius.y + std::max(borderTop, borderRight),
                innerRadius.z + std::max(borderBottom, borderRight),
                innerRadius.w + std::max(borderBottom, borderLeft)
            );

            Vec4 borderWidthVec = Vec4(borderTop, borderRight, borderBottom, borderLeft);

            if (std::abs(rotation) > 0.0001f) {
                // Rotated: position is center-pivot, use as-is
                ctx.drawRoundedRectBorder(
                    position,
                    outerSize,
                    outerRadius,
                    borderWidthVec,
                    borderColor,
                    rotation
                );
            } else {
                // Non-rotated: convert center-pivot to top-left
                Vec2 topLeft = Vec2(position.x - size.x * 0.5f, position.y - size.y * 0.5f);
                Vec2 outerPos = Vec2(topLeft.x - borderLeft, topLeft.y - borderTop);
                ctx.drawRoundedRectBorder(
                    outerPos,
                    outerSize,
                    outerRadius,
                    borderWidthVec,
                    borderColor
                );
            }
        }
    }

    if (std::abs(rotation) > 0.0001f) {
        ctx.drawRoundedRect(
            position,
            size,
            cornerRadiusVec,
            bgColor,
            rotation,
            0
        );
    } else {
        // Position is center-pivot, convert to top-left for non-rotated rendering
        Vec2 topLeft = Vec2(position.x - size.x * 0.5f, position.y - size.y * 0.5f);
        ctx.drawRoundedRect(
            topLeft,
            size,
            cornerRadiusVec,
            bgColor,
            0
        );
    }

    // Optional background image, painted over the flat fill and tinted by the current
    // state modulation + opacity (so hover/pressed brightness affects the image too).
    Color imageTint = GetEffectiveColor(Color::White());
    imageTint.a *= GetOpacity();
    RenderBackgroundImage(ctx, position, size, rotation, imageTint);
}

void Button::RenderBackgroundImage(RenderContext& ctx, const Vec2& position, const Vec2& size,
                                   float rotation, const Color& tint) {
    std::string path = ResolveThemedImage("backgroundImagePath", "background_image");

    if (path != m_CurrentBackgroundImagePath) {
        if (m_BackgroundImageHandle.isValid()) {
            if (IGfxDevice* device = ctx.getDevice()) {
                device->destroyTexture(m_BackgroundImageHandle);
            }
            m_BackgroundImageHandle = TextureHandle();
        }
        m_BackgroundImageAsset.Reset();
        m_CurrentBackgroundImagePath = path;

        if (!path.empty()) {
            m_BackgroundImageAsset = asset::AssetRef<asset::ImageAsset>(new asset::ImageAsset());
            if (!m_BackgroundImageAsset->LoadFromFile(path, true, asset::ImageColorSpace::sRGB)) {
                m_BackgroundImageAsset.Reset();
            }
        }
    }

    if (!m_BackgroundImageHandle.isValid() && m_BackgroundImageAsset.IsValid() &&
        m_BackgroundImageAsset->IsLoaded()) {
        if (m_BackgroundImageAsset->GetWidth() > 0 && m_BackgroundImageAsset->GetHeight() > 0 &&
            m_BackgroundImageAsset->GetData() != nullptr) {
            if (IGfxDevice* device = ctx.getDevice()) {
                m_BackgroundImageHandle = lupine::CreateTexture2DFromImage(device, *m_BackgroundImageAsset, TextureFormat::RGBA8_UNORM);
            }
        }
    }

    if (!m_BackgroundImageHandle.isValid() || !m_BackgroundImageAsset.IsValid()) {
        return;
    }

    UIImageStretchMode stretchMode = GetBackgroundImageStretchMode();
    Vec4 margins = GetNineSliceMargins();
    UINineSlice nineSlice;
    nineSlice.marginLeft = margins.x;
    nineSlice.marginTop = margins.y;
    nineSlice.marginRight = margins.z;
    nineSlice.marginBottom = margins.w;
    nineSlice.axisHorizontal = GetNineSliceAxisHorizontal();
    nineSlice.axisVertical = GetNineSliceAxisVertical();
    nineSlice.drawCenter = GetNineSliceDrawCenter();

    // A themed "background_image" entry may also dictate the fit (stretch / nine-slice),
    // overriding this control's own stretch + nine-slice properties.
    ui::ThemeImage themedImage;
    if (ResolveThemedImageEx("backgroundImagePath", "background_image", themedImage) && themedImage.hasStretch) {
        stretchMode = UIImageStretchModeFromInt(themedImage.stretchMode);
        if (themedImage.stretchMode == 2) {
            nineSlice.marginLeft = themedImage.marginLeft;
            nineSlice.marginTop = themedImage.marginTop;
            nineSlice.marginRight = themedImage.marginRight;
            nineSlice.marginBottom = themedImage.marginBottom;
            nineSlice.axisHorizontal = UINineSliceAxisModeFromInt(themedImage.axisH);
            nineSlice.axisVertical = UINineSliceAxisModeFromInt(themedImage.axisV);
            nineSlice.drawCenter = themedImage.drawCenter;
        }
    }

    DrawUIImage(ctx, position, size, rotation, m_BackgroundImageHandle,
                m_BackgroundImageAsset->GetWidth(), m_BackgroundImageAsset->GetHeight(),
                tint, m_CornerRadius.AsVec4(), stretchMode, nineSlice);
}

TextLayoutParams Button::BuildLayoutParams(const Vec2& boxSize) const {
    TextLayoutParams params;
    params.fontSize = GetFontSize();
    params.color = GetEffectiveColor(GetFontColor());
    params.hAlign = GetTextHAlign();
    params.vAlign = GetTextVAlign();
    params.multiline = true;
    params.wordWrap = GetWordWrap();
    params.autowrapMode = static_cast<TextAutowrapMode>(GetPropertyValue<int>("autowrapMode"));
    params.overrunBehavior = static_cast<TextOverrunBehavior>(GetPropertyValue<int>("overrunBehavior"));
    params.clipText = GetPropertyValue<bool>("clipText");
    params.tabSize = GetPropertyValue<int>("tabSize");
    params.lineSpacing = GetLineSpacing();
    params.shadowOffset = GetPropertyValue<Vec2>("shadowOffset");
    params.shadowColor = GetPropertyValue<Color>("shadowColor");
    params.boxWidth = (boxSize.x > 0.0f) ? boxSize.x : 0.0f;
    params.boxHeight = (boxSize.y > 0.0f) ? boxSize.y : 0.0f;
    return params;
}

math::Rect Button::GetTextBoxRect(const Vec2& center) const {
    // The box the label is laid out inside: the button's on-screen rect, inset by the content
    // margins. textPadding used to be read ONLY when auto-sizing -- it never moved the drawn
    // text -- and the themed StyleBox's content margins were not read by anything at all.
    //
    // The center is passed in rather than read off the node, because the draw pass offsets it
    // by the active state tween's position offset; the label has to travel with the button.
    //
    // GetEffectiveSize(), not GetBoundsSize(): in FitToText mode the fitted size is what the
    // button actually draws at.
    const Vec2 size = GetEffectiveSize();
    const math::Rect box(center.x - size.x * 0.5f, center.y - size.y * 0.5f, size.x, size.y);

    return GetContentInsetRect(box);
}

void Button::RegenerateTextMesh(RenderContext& ctx, const Vec2& position, const Vec2&) {
    const std::string text = GetText();

    if (text.empty()) {
        if (m_TextMesh.isValid()) {
            ctx.getDevice()->destroyMesh(m_TextMesh);
            m_TextMesh = MeshHandle();
        }
        m_CachedText = "";
        m_CachedFontSize = 0.0f;
        m_CachedPosition = position;
        m_TextMeshNeedsRegeneration = false;
        return;
    }

    const FontAtlas* fontAtlas = ctx.getDevice()->getFontAtlas(m_FontHandle);
    if (!fontAtlas) {
        return;
    }

    // Laid out through TextLayout, exactly as Label is. The old implementation was a
    // hand-rolled glyph loop that hard-CENTERED the text on the button's center: there was no
    // horizontal or vertical alignment at all (you could not left-align a menu-row button),
    // the wordWrap property it advertised was never read by the renderer (long text simply
    // overflowed), lineSpacing did not exist, and the loop was byte-wise so non-ASCII UTF-8
    // produced garbage codepoints.
    const math::Rect box = GetTextBoxRect(position);
    const Vec2 boxTopLeft = box.GetTopLeft();

    TextLayoutParams params = BuildLayoutParams(box.size);
    TextLayoutResult layout = TextLayout::Layout(text, *fontAtlas, params);

    if (m_TextMesh.isValid()) {
        ctx.getDevice()->destroyMesh(m_TextMesh);
        m_TextMesh = MeshHandle();
    }

    if (layout.quads.empty()) {
        m_CachedText = text;
        m_CachedFontSize = params.fontSize;
        m_CachedPosition = position;
        m_TextMeshNeedsRegeneration = false;
        return;
    }

    // TextLayout's local space has its origin at the box's TOP-left and descends into
    // negative Y, so the glyphs land inside the box by adding that corner and nothing else.
    const float textZ = 0.1f;

    MeshData textMeshData;
    textMeshData.vertices.reserve(layout.quads.size() * 4);
    textMeshData.indices.reserve(layout.quads.size() * 6);

    uint32_t vertexOffset = 0;
    for (const TextGlyphQuad& q : layout.quads) {
        for (int k = 0; k < 4; ++k) {
            Vertex vtx;
            vtx.position = Vec3(boxTopLeft.x + q.pos[k].x, boxTopLeft.y + q.pos[k].y, textZ);
            vtx.normal = Vec3(0.0f, 0.0f, 1.0f);
            vtx.texCoord = q.uv[k];
            vtx.color = Vec4(q.color.r, q.color.g, q.color.b, q.color.a);
            textMeshData.vertices.push_back(vtx);
        }
        textMeshData.indices.push_back(vertexOffset + 0);
        textMeshData.indices.push_back(vertexOffset + 1);
        textMeshData.indices.push_back(vertexOffset + 2);
        textMeshData.indices.push_back(vertexOffset + 0);
        textMeshData.indices.push_back(vertexOffset + 2);
        textMeshData.indices.push_back(vertexOffset + 3);
        vertexOffset += 4;
    }

    textMeshData.calculateBounds();

    m_TextMesh = ctx.getDevice()->createMesh(textMeshData);
    m_CachedText = text;
    m_CachedFontSize = params.fontSize;
    m_CachedPosition = position;
    m_TextMeshNeedsRegeneration = false;

}

void Button::RenderText(RenderContext& ctx, const Vec2& position, const Vec2& size) {
    std::string text = GetText();
    if (text.empty()) {
        return;
    }

    if (!m_FontHandle.isValid()) {

        return;
    }

    float fontSize = GetFontSize();
    if (text != m_CachedText || fontSize != m_CachedFontSize || position != m_CachedPosition || m_TextMeshNeedsRegeneration || !m_TextMesh.isValid()) {
        RegenerateTextMesh(ctx, position, size);
    }

    if (!m_TextMesh.isValid()) {
        return;
    }

    const FontAtlas* fontAtlas = ctx.getDevice()->getFontAtlas(m_FontHandle);
    if (!fontAtlas) {
        return;
    }

    Color fontColor = GetEffectiveColor(GetFontColor());

    MaterialPropertyBlock overrides;
    overrides.setTexture("u_FontAtlas", fontAtlas->texture);
    overrides.setColor("u_TextColor", fontColor);

    Mat4 transform = Mat4::Identity();
    ctx.drawMesh(m_TextMesh, ctx.getDefaultTextMaterial(), transform, overrides);

}

AABB Button::getWorldBounds() const {
    if (!GetOwner()) {
        return AABB();
    }

    core::Node2D* node2D = dynamic_cast<core::Node2D*>(GetOwner());
    if (!node2D) {
        return AABB();
    }

    Vec2 position = node2D->GetGlobalPosition();
    Vec2 globalScale = node2D->GetGlobalScale();
    Vec2 size = GetEffectiveSize();

    size.x *= globalScale.x;
    size.y *= globalScale.y;

    position += m_CurrentPositionOffset;

    float rotation = node2D->GetGlobalRotation();
    if (std::abs(rotation) > 0.0001f) {
        float cosR = std::cos(rotation);
        float sinR = std::sin(rotation);
        Vec2 halfSize = size * 0.5f;

        Vec2 localCorners[4] = {
            Vec2(-halfSize.x, -halfSize.y),
            Vec2( halfSize.x, -halfSize.y),
            Vec2( halfSize.x,  halfSize.y),
            Vec2(-halfSize.x,  halfSize.y)
        };

        Vec2 min(FLT_MAX, FLT_MAX);
        Vec2 max(-FLT_MAX, -FLT_MAX);

        for (int i = 0; i < 4; ++i) {

            Vec2 rotated(
                localCorners[i].x * cosR - localCorners[i].y * sinR,
                localCorners[i].x * sinR + localCorners[i].y * cosR
            );

            Vec2 worldCorner = position + rotated;

            min.x = std::min(min.x, worldCorner.x);
            min.y = std::min(min.y, worldCorner.y);
            max.x = std::max(max.x, worldCorner.x);
            max.y = std::max(max.y, worldCorner.y);
        }

        return AABB(Vec3(min.x, min.y, -0.1f), Vec3(max.x, max.y, 0.1f));
    }

    return AABB(
        Vec3(position.x - size.x * 0.5f, position.y - size.y * 0.5f, -0.1f),
        Vec3(position.x + size.x * 0.5f, position.y + size.y * 0.5f, 0.1f)
    );
}

math::OBB Button::getOrientedBounds() const {
    if (!GetOwner()) {
        return math::OBB();
    }

    core::Node2D* node2D = dynamic_cast<core::Node2D*>(GetOwner());
    if (!node2D) {
        return math::OBB();
    }

    Vec2 position = node2D->GetGlobalPosition();
    Vec2 size = GetEffectiveSize();
    Vec2 globalScale = node2D->GetGlobalScale();
    float rotation = node2D->GetGlobalRotation();

    size.x *= globalScale.x;
    size.y *= globalScale.y;

    position += m_CurrentPositionOffset;

    Vec3 center = Vec3(position.x, position.y, 0.0f);
    Vec3 extents = Vec3(size.x * 0.5f, size.y * 0.5f, 0.1f);
    Quat quatRotation = Quat::FromAxisAngle(Vec3::UnitZ(), rotation + m_CurrentRotationOffset);

    return math::OBB(center, extents, quatRotation);
}

bool Button::IntersectRay(const math::Ray& ray, float& outDistance) const {
    math::OBB obb = getOrientedBounds();

    Vec3 localRayOrigin = obb.rotation.Inverse() * (ray.origin - obb.center);
    Vec3 localRayDir = obb.rotation.Inverse() * ray.direction;

    AABB localAABB(
        Vec3(-obb.extents.x, -obb.extents.y, -obb.extents.z),
        Vec3(obb.extents.x, obb.extents.y, obb.extents.z)
    );

    math::Ray localRay(localRayOrigin, localRayDir);
    return localRay.IntersectAABB(localAABB, outDistance);
}

RenderLayer Button::getRenderLayer() const {
    int layer = GetLayer();
    int sortingOrder = GetSortingOrder();

    return static_cast<RenderLayer>(layer * 1000 + sortingOrder);
}

SpatialType Button::getSpatialType() const {
    return GetUISpatialType();
}

bool Button::OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) {
    asset::AssetDatabase& assetDb = asset::AssetDatabase::GetInstance();

    auto matchesPath = [&](const std::string& assetPath) -> bool {
        if (assetPath.empty()) {
            return false;
        }
        std::string resolved;
        if (assetDb.IsInitialized()) {
            resolved = assetDb.ResolveAsset(assetPath);
        }
        return (assetPath == changedPath) ||
               (!resolved.empty() && !resolvedChangedPath.empty() && resolved == resolvedChangedPath);
    };

    bool handled = false;

    if (matchesPath(GetFontPath())) {
        m_FontHandle = FontHandle();
        m_FontAsset.Reset();
        m_CurrentFontPath.clear();
        m_TextMeshNeedsRegeneration = true;
        handled = true;
    }

    if (matchesPath(GetBackgroundImagePath())) {
        m_BackgroundImageHandle = TextureHandle();
        m_BackgroundImageAsset.Reset();
        m_CurrentBackgroundImagePath.clear();
        m_MeshNeedsRegeneration = true;
        handled = true;
    }

    return handled;
}

}
}

