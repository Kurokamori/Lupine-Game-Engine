#include "lupine/components/ToggleButton.hpp"
#include "lupine/components/StyleBoxRenderer.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/ui/ThemeManager.hpp"
#include "lupine/ui/Theme.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/Mesh.hpp"
#include "lupine/rendering/Font.hpp"
#include "lupine/rendering/DrawCommand.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/rendering/gfx/GfxDescriptors.hpp"
#include "lupine/input/InputManager.hpp"
#include "lupine/rendering/ViewportUtils.hpp"
#include "lupine/asset/AssetDatabase.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/math/OBB.hpp"

namespace lupine {
namespace components {

using namespace core;
using namespace math;

namespace {
// Maps an interaction state to its themed-stylebox entry name. Reuses Godot's
// standard Button stylebox vocabulary (normal/hover/pressed/hover_pressed/disabled)
// so a ported Godot Button theme styles toggles too: toggled-on shows "pressed",
// toggled+hover shows "hover_pressed".
const char* ToggleButtonStateThemeEntry(ToggleButtonState state) {
    switch (state) {
        case ToggleButtonState::Normal:         return "normal";
        case ToggleButtonState::Hover:          return "hover";
        case ToggleButtonState::Pressed:        return "pressed";
        case ToggleButtonState::Toggled:        return "pressed";
        case ToggleButtonState::ToggledHover:   return "hover_pressed";
        case ToggleButtonState::ToggledPressed: return "pressed";
        case ToggleButtonState::Disabled:       return "disabled";
        default:                                return "normal";
    }
}
} // namespace

ToggleButton::ToggleButton()
    : UIControl("ToggleButton")
    , m_BaseStyleBox(nullptr)
    , m_CornerRadius(0.0f, true)
    , m_BorderWidth(0.0f, true)
    , m_MeshNeedsRegeneration(true)
{
    m_BaseStyleBox = std::make_shared<StyleBoxFlat>();

    // Initialize default state modulation colors
    m_StateStyles[static_cast<int>(ToggleButtonState::Normal)].modulationColor = Color::White();
    m_StateStyles[static_cast<int>(ToggleButtonState::Hover)].modulationColor = Color(1.2f, 1.2f, 1.2f, 1.0f);
    m_StateStyles[static_cast<int>(ToggleButtonState::Pressed)].modulationColor = Color(0.8f, 0.8f, 0.8f, 1.0f);
    m_StateStyles[static_cast<int>(ToggleButtonState::Toggled)].modulationColor = Color(1.3f, 1.3f, 1.3f, 1.0f);
    m_StateStyles[static_cast<int>(ToggleButtonState::ToggledHover)].modulationColor = Color(1.5f, 1.5f, 1.5f, 1.0f);
    m_StateStyles[static_cast<int>(ToggleButtonState::ToggledPressed)].modulationColor = Color(1.1f, 1.1f, 1.1f, 1.0f);
    m_StateStyles[static_cast<int>(ToggleButtonState::Disabled)].modulationColor = Color(0.5f, 0.5f, 0.5f, 0.5f);
}

ToggleButton::ToggleButton(const std::string& name)
    : UIControl(name)
    , m_BaseStyleBox(nullptr)
    , m_CornerRadius(0.0f, true)
    , m_BorderWidth(0.0f, true)
    , m_MeshNeedsRegeneration(true)
{
    m_BaseStyleBox = std::make_shared<StyleBoxFlat>();

    // Initialize default state modulation colors
    m_StateStyles[static_cast<int>(ToggleButtonState::Normal)].modulationColor = Color::White();
    m_StateStyles[static_cast<int>(ToggleButtonState::Hover)].modulationColor = Color(1.2f, 1.2f, 1.2f, 1.0f);
    m_StateStyles[static_cast<int>(ToggleButtonState::Pressed)].modulationColor = Color(0.8f, 0.8f, 0.8f, 1.0f);
    m_StateStyles[static_cast<int>(ToggleButtonState::Toggled)].modulationColor = Color(1.3f, 1.3f, 1.3f, 1.0f);
    m_StateStyles[static_cast<int>(ToggleButtonState::ToggledHover)].modulationColor = Color(1.5f, 1.5f, 1.5f, 1.0f);
    m_StateStyles[static_cast<int>(ToggleButtonState::ToggledPressed)].modulationColor = Color(1.1f, 1.1f, 1.1f, 1.0f);
    m_StateStyles[static_cast<int>(ToggleButtonState::Disabled)].modulationColor = Color(0.5f, 0.5f, 0.5f, 0.5f);
}

ToggleButton::~ToggleButton() {
}

void ToggleButton::DefineProperties() {
    DefineUIControlProperties(120.0f, 40.0f, "useUISpace", "Size");

    // Rendering
    DefineProperty(PROPERTY_INT_RANGE_GROUP(layer, 0, -100, 100, 1, "Rendering"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(sortingOrder, 0, -1000, 1000, 1, "Rendering"));

    // Button
    DefineProperty(PROPERTY_DEFAULT_GROUP(buttonEnabled, Bool, true, "Button"));
    DefineProperty(PROPERTY_ENUM_GROUP(styleMode, 0, "Button", Automatic, Manual));
    DefineProperty(PROPERTY_DEFAULT_GROUP(isToggled, Bool, false, "Button"));

    // Text
    DefineProperty(PROPERTY_DEFAULT_GROUP(text, String, std::string("Toggle"), "Text"));
    DefineProperty(PROPERTY_FILE_GROUP(fontPath, std::string(""), "*.ttf,*.otf", "Text"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(fontSize, 16.0f, 1.0f, 256.0f, 1.0f, "Text"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(fontColor, Color, Color::White(), "Text"));

    // Background
    DefineProperty(PROPERTY_DEFAULT_GROUP(backgroundColor, Color, Color(0.2f, 0.2f, 0.2f, 1.0f), "Background"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(opacity, 1.0f, 0.0f, 1.0f, 0.01f, "Background"));

    // Border
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderEnabled, Bool, true, "Border"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderWidthLinked, Bool, true, "Border"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderWidth, Vec4, Vec4(2.0f, 2.0f, 2.0f, 2.0f), "Border"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderColor, Color, Color(0.5f, 0.5f, 0.5f, 1.0f), "Border"));

    // Corner Radius
    DefineProperty(PROPERTY_DEFAULT_GROUP(cornerRadiusLinked, Bool, true, "CornerRadius"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(cornerRadius, Vec4, Vec4(5.0f, 5.0f, 5.0f, 5.0f), "CornerRadius"));

    // State - Normal
    DefineProperty(PROPERTY_DEFAULT_GROUP(normalModulation, Color, Color::White(), "State_Normal"));
    DefineProperty(PROPERTY_FILE_GROUP(normalSoundPath, std::string(""), "*.wav,*.ogg,*.mp3", "State_Normal"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(normalTweenEnabled, Bool, false, "State_Normal"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(normalTweenScale, Vec2, Vec2(0.0f, 0.0f), "State_Normal"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(normalTweenRotation, 0.0f, -360.0f, 360.0f, 1.0f, "State_Normal"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(normalTweenPosition, Vec2, Vec2(0.0f, 0.0f), "State_Normal"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(normalTweenDuration, 0.2f, 0.0f, 5.0f, 0.01f, "State_Normal"));

    // State - Hover
    DefineProperty(PROPERTY_DEFAULT_GROUP(hoverModulation, Color, Color(1.2f, 1.2f, 1.2f, 1.0f), "State_Hover"));
    DefineProperty(PROPERTY_FILE_GROUP(hoverSoundPath, std::string(""), "*.wav,*.ogg,*.mp3", "State_Hover"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(hoverTweenEnabled, Bool, true, "State_Hover"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(hoverTweenScale, Vec2, Vec2(0.05f, 0.05f), "State_Hover"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(hoverTweenRotation, 0.0f, -360.0f, 360.0f, 1.0f, "State_Hover"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(hoverTweenPosition, Vec2, Vec2(0.0f, 0.0f), "State_Hover"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(hoverTweenDuration, 0.2f, 0.0f, 5.0f, 0.01f, "State_Hover"));

    // State - Pressed
    DefineProperty(PROPERTY_DEFAULT_GROUP(pressedModulation, Color, Color(0.8f, 0.8f, 0.8f, 1.0f), "State_Pressed"));
    DefineProperty(PROPERTY_FILE_GROUP(pressedSoundPath, std::string(""), "*.wav,*.ogg,*.mp3", "State_Pressed"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(pressedTweenEnabled, Bool, true, "State_Pressed"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(pressedTweenScale, Vec2, Vec2(-0.05f, -0.05f), "State_Pressed"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(pressedTweenRotation, 0.0f, -360.0f, 360.0f, 1.0f, "State_Pressed"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(pressedTweenPosition, Vec2, Vec2(0.0f, 2.0f), "State_Pressed"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(pressedTweenDuration, 0.1f, 0.0f, 5.0f, 0.01f, "State_Pressed"));

    // State - Toggled
    DefineProperty(PROPERTY_DEFAULT_GROUP(toggledModulation, Color, Color(1.3f, 1.3f, 1.3f, 1.0f), "State_Toggled"));
    DefineProperty(PROPERTY_FILE_GROUP(toggledSoundPath, std::string(""), "*.wav,*.ogg,*.mp3", "State_Toggled"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(toggledTweenEnabled, Bool, false, "State_Toggled"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(toggledTweenScale, Vec2, Vec2(0.0f, 0.0f), "State_Toggled"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(toggledTweenRotation, 0.0f, -360.0f, 360.0f, 1.0f, "State_Toggled"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(toggledTweenPosition, Vec2, Vec2(0.0f, 0.0f), "State_Toggled"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(toggledTweenDuration, 0.2f, 0.0f, 5.0f, 0.01f, "State_Toggled"));

    // State - ToggledHover
    DefineProperty(PROPERTY_DEFAULT_GROUP(toggledHoverModulation, Color, Color(1.5f, 1.5f, 1.5f, 1.0f), "State_ToggledHover"));
    DefineProperty(PROPERTY_FILE_GROUP(toggledHoverSoundPath, std::string(""), "*.wav,*.ogg,*.mp3", "State_ToggledHover"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(toggledHoverTweenEnabled, Bool, true, "State_ToggledHover"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(toggledHoverTweenScale, Vec2, Vec2(0.05f, 0.05f), "State_ToggledHover"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(toggledHoverTweenRotation, 0.0f, -360.0f, 360.0f, 1.0f, "State_ToggledHover"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(toggledHoverTweenPosition, Vec2, Vec2(0.0f, 0.0f), "State_ToggledHover"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(toggledHoverTweenDuration, 0.2f, 0.0f, 5.0f, 0.01f, "State_ToggledHover"));

    // State - ToggledPressed
    DefineProperty(PROPERTY_DEFAULT_GROUP(toggledPressedModulation, Color, Color(1.1f, 1.1f, 1.1f, 1.0f), "State_ToggledPressed"));
    DefineProperty(PROPERTY_FILE_GROUP(toggledPressedSoundPath, std::string(""), "*.wav,*.ogg,*.mp3", "State_ToggledPressed"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(toggledPressedTweenEnabled, Bool, true, "State_ToggledPressed"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(toggledPressedTweenScale, Vec2, Vec2(-0.05f, -0.05f), "State_ToggledPressed"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(toggledPressedTweenRotation, 0.0f, -360.0f, 360.0f, 1.0f, "State_ToggledPressed"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(toggledPressedTweenPosition, Vec2, Vec2(0.0f, 2.0f), "State_ToggledPressed"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(toggledPressedTweenDuration, 0.1f, 0.0f, 5.0f, 0.01f, "State_ToggledPressed"));

    // State - Disabled
    DefineProperty(PROPERTY_DEFAULT_GROUP(disabledModulation, Color, Color(0.5f, 0.5f, 0.5f, 0.5f), "State_Disabled"));
    DefineProperty(PROPERTY_FILE_GROUP(disabledSoundPath, std::string(""), "*.wav,*.ogg,*.mp3", "State_Disabled"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(disabledTweenEnabled, Bool, false, "State_Disabled"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(disabledTweenScale, Vec2, Vec2(0.0f, 0.0f), "State_Disabled"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(disabledTweenRotation, 0.0f, -360.0f, 360.0f, 1.0f, "State_Disabled"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(disabledTweenPosition, Vec2, Vec2(0.0f, 0.0f), "State_Disabled"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(disabledTweenDuration, 0.2f, 0.0f, 5.0f, 0.01f, "State_Disabled"));
}

void ToggleButton::OnAwake() {
    m_ButtonEnabled = GetPropertyValue<bool>("buttonEnabled");
    m_IsToggled = GetPropertyValue<bool>("isToggled");

    // Load corner radius
    Vec4 cornerRadiusVec = GetPropertyValue<Vec4>("cornerRadius");
    bool cornerRadiusLinked = GetPropertyValue<bool>("cornerRadiusLinked");
    m_CornerRadius.FromVec4(cornerRadiusVec);
    m_CornerRadius.SetLinked(cornerRadiusLinked);

    // Load border width
    Vec4 borderWidthVec = GetPropertyValue<Vec4>("borderWidth");
    bool borderWidthLinked = GetPropertyValue<bool>("borderWidthLinked");
    m_BorderWidth.FromVec4(borderWidthVec);
    m_BorderWidth.SetLinked(borderWidthLinked);

    LoadThemedStateData();

    // Load state sounds (just paths, assets loaded on-demand)
    auto loadSound = [this](ToggleButtonState state, const std::string& prefix) {
        std::string path = GetPropertyValue<std::string>(prefix + "SoundPath");
        if (!path.empty()) {
            SetStateSoundPath(state, path);
        }
    };

    loadSound(ToggleButtonState::Normal, "normal");
    loadSound(ToggleButtonState::Hover, "hover");
    loadSound(ToggleButtonState::Pressed, "pressed");
    loadSound(ToggleButtonState::Toggled, "toggled");
    loadSound(ToggleButtonState::ToggledHover, "toggledHover");
    loadSound(ToggleButtonState::ToggledPressed, "toggledPressed");
    loadSound(ToggleButtonState::Disabled, "disabled");

    std::string fontPath = GetFontPath();
    if (!fontPath.empty()) {
        m_CurrentFontPath = fontPath;
    }
}

void ToggleButton::OnReady() {
    m_MeshNeedsRegeneration = true;
}

void ToggleButton::OnInput(float deltaTime) {
    if (!IsEnabled()) {
        return;
    }

    UpdateMouseInteraction(deltaTime);
}

void ToggleButton::OnUpdate(float deltaTime) {
    UIControl::OnUpdate(deltaTime);

    if (!IsEnabled()) {
        return;
    }

    SyncFromProperties();

    // Update tween animation
    if (m_IsTweening && m_TweenProgress < 1.0f) {
        const ToggleButtonStateTween& targetTween = m_StateTweens[static_cast<int>(m_CurrentState)];
        if (targetTween.duration > 0.0f) {
            m_TweenProgress += deltaTime / targetTween.duration;

            if (m_TweenProgress >= 1.0f) {
                m_TweenProgress = 1.0f;
                m_IsTweening = false;
            }

            float t = m_TweenProgress;
            m_CurrentScaleOffset = m_TweenStartScaleOffset + (targetTween.scaleOffset - m_TweenStartScaleOffset) * t;
            m_CurrentRotationOffset = m_TweenStartRotationOffset + (targetTween.rotationOffset - m_TweenStartRotationOffset) * t;
            m_CurrentPositionOffset = m_TweenStartPositionOffset + (targetTween.positionOffset - m_TweenStartPositionOffset) * t;
        }
    }
}

void ToggleButton::OnRender() {
}

bool ToggleButton::OnGizmoScale(float scaleDelta, int axis, bool is3D) {
    // Resizing is delegated to the base, which writes whichever property actually drives
    // the axis: width/height when point-anchored, the offsets when anchor-stretched (where
    // width/height are never read at all), and nothing when a container owns the rect.
    const bool handled = UIControl::OnGizmoScale(scaleDelta, axis, is3D);
    if (handled) {
        m_MeshNeedsRegeneration = true;
    }
    return handled;
}

// ===== Property Accessors =====

float ToggleButton::GetWidth() const {
    return GetPropertyValue<float>("width");
}

void ToggleButton::SetWidth(float width) {
    SetPropertyValue<float>("width", width);
    m_MeshNeedsRegeneration = true;
}

float ToggleButton::GetHeight() const {
    return GetPropertyValue<float>("height");
}

void ToggleButton::SetHeight(float height) {
    SetPropertyValue<float>("height", height);
    m_MeshNeedsRegeneration = true;
}

int ToggleButton::GetLayer() const {
    return GetPropertyValue<int>("layer");
}

void ToggleButton::SetLayer(int layer) {
    SetPropertyValue<int>("layer", layer);
}

int ToggleButton::GetSortingOrder() const {
    return GetPropertyValue<int>("sortingOrder");
}

void ToggleButton::SetSortingOrder(int order) {
    SetPropertyValue<int>("sortingOrder", order);
}

void ToggleButton::SetEnabled(bool enabled) {
    m_ButtonEnabled = enabled;
    SetPropertyValue<bool>("buttonEnabled", enabled);
}

bool ToggleButton::IsToggled() const {
    return m_IsToggled;
}

void ToggleButton::SetToggled(bool toggled) {
    if (m_IsToggled != toggled) {
        m_IsToggled = toggled;
        SetPropertyValue<bool>("isToggled", toggled);

        if (m_OnToggledCallback) {
            m_OnToggledCallback(toggled);
        }
        Emit("toggled", { toggled });
    }
}

void ToggleButton::DefineSignals() {
    RegisterSignal({"toggled",
                    {{"toggled", core::PropertyValueType::Bool}},
                    "Emitted when the toggle state changes."});
}

std::string ToggleButton::GetText() const {
    return GetPropertyValue<std::string>("text");
}

void ToggleButton::SetText(const std::string& text) {
    SetPropertyValue<std::string>("text", text);
    m_TextMeshNeedsRegeneration = true;
}

std::string ToggleButton::GetFontPath() const {
    return ResolveThemedFontPath("fontPath", "font");
}

void ToggleButton::SetFontPath(const std::string& path) {
    SetThemedProperty<std::string>("fontPath", path);
    m_CurrentFontPath = path;
    m_TextMeshNeedsRegeneration = true;
}

float ToggleButton::GetFontSize() const {
    return ResolveThemedFontSize("fontSize", "font_size", "font");
}

void ToggleButton::SetFontSize(float size) {
    SetThemedProperty<float>("fontSize", size);
    m_CurrentFontSize = size;
    m_TextMeshNeedsRegeneration = true;
}

const std::vector<UIControl::ThemeBinding>& ToggleButton::GetThemeBindings() const {
    static const std::vector<ThemeBinding> kBindings = []() {
        std::vector<ThemeBinding> b = {
            { "backgroundColor", "background",    ThemeBinding::Kind::Color },
            { "borderColor",     "border_color",  ThemeBinding::Kind::Color },
            { "fontColor",       "font_color",    ThemeBinding::Kind::Color },
            { "fontPath",        "font",          ThemeBinding::Kind::Font },
            { "fontSize",        "font_size",     ThemeBinding::Kind::Constant },
            { "cornerRadius",    "corner_radius", ThemeBinding::Kind::Constant }
        };
        const char* states[] = { "normal", "hover", "pressed", "toggled", "toggledHover", "toggledPressed", "disabled" };
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

Color ToggleButton::GetFontColor() const {
    return ResolveThemedColor("fontColor", "font_color");
}

void ToggleButton::SetFontColor(const Color& color) {
    SetThemedProperty<Color>("fontColor", color);
    m_TextMeshNeedsRegeneration = true;
}

Color ToggleButton::GetBackgroundColor() const {
    return ResolveThemedColor("backgroundColor", "background");
}

void ToggleButton::SetBackgroundColor(const Color& color) {
    SetThemedProperty<Color>("backgroundColor", color);
}

float ToggleButton::GetOpacity() const {
    return GetPropertyValue<float>("opacity");
}

void ToggleButton::SetOpacity(float opacity) {
    SetPropertyValue<float>("opacity", opacity);
}

bool ToggleButton::GetBorderEnabled() const {
    return GetPropertyValue<bool>("borderEnabled");
}

void ToggleButton::SetBorderEnabled(bool enabled) {
    SetPropertyValue<bool>("borderEnabled", enabled);
}

Color ToggleButton::GetBorderColor() const {
    return ResolveThemedColor("borderColor", "border_color");
}

void ToggleButton::SetBorderColor(const Color& color) {
    SetThemedProperty<Color>("borderColor", color);
}

void ToggleButton::SetOnToggledCallback(ToggleCallback callback) {
    m_OnToggledCallback = callback;
}

// ===== Style Mode =====

ToggleButtonStyleMode ToggleButton::GetStyleMode() const {
    return m_StyleMode;
}

void ToggleButton::SetStyleMode(ToggleButtonStyleMode mode) {
    m_StyleMode = mode;
    SetPropertyValue<int>("styleMode", static_cast<int>(mode));
    m_MeshNeedsRegeneration = true;
}

// ===== Per-State Styles =====

const ToggleButtonStateStyle& ToggleButton::GetStateStyle(ToggleButtonState state) const {
    return m_StateStyles[static_cast<int>(state)];
}

void ToggleButton::SetStateStyle(ToggleButtonState state, const ToggleButtonStateStyle& style) {
    m_StateStyles[static_cast<int>(state)] = style;
    m_MeshNeedsRegeneration = true;
}

const Color& ToggleButton::GetStateModulation(ToggleButtonState state) const {
    return m_StateStyles[static_cast<int>(state)].modulationColor;
}

void ToggleButton::SetStateModulation(ToggleButtonState state, const Color& color) {
    m_StateStyles[static_cast<int>(state)].modulationColor = color;

    std::string propName;
    switch (state) {
        case ToggleButtonState::Normal: propName = "normalModulation"; break;
        case ToggleButtonState::Hover: propName = "hoverModulation"; break;
        case ToggleButtonState::Pressed: propName = "pressedModulation"; break;
        case ToggleButtonState::Toggled: propName = "toggledModulation"; break;
        case ToggleButtonState::ToggledHover: propName = "toggledHoverModulation"; break;
        case ToggleButtonState::ToggledPressed: propName = "toggledPressedModulation"; break;
        case ToggleButtonState::Disabled: propName = "disabledModulation"; break;
        default: return;
    }
    SetPropertyValue<Color>(propName, color);
    m_MeshNeedsRegeneration = true;
}

// ===== Per-State Sounds =====

const ToggleButtonStateSound& ToggleButton::GetStateSound(ToggleButtonState state) const {
    return m_StateSounds[static_cast<int>(state)];
}

void ToggleButton::SetStateSound(ToggleButtonState state, const ToggleButtonStateSound& sound) {
    m_StateSounds[static_cast<int>(state)] = sound;
}

void ToggleButton::SetStateSoundPath(ToggleButtonState state, const std::string& path) {
    int idx = static_cast<int>(state);
    m_StateSounds[idx].audioPath = path;

    std::string propName;
    switch (state) {
        case ToggleButtonState::Normal: propName = "normalSoundPath"; break;
        case ToggleButtonState::Hover: propName = "hoverSoundPath"; break;
        case ToggleButtonState::Pressed: propName = "pressedSoundPath"; break;
        case ToggleButtonState::Toggled: propName = "toggledSoundPath"; break;
        case ToggleButtonState::ToggledHover: propName = "toggledHoverSoundPath"; break;
        case ToggleButtonState::ToggledPressed: propName = "toggledPressedSoundPath"; break;
        case ToggleButtonState::Disabled: propName = "disabledSoundPath"; break;
        default: return;
    }
    SetPropertyValue<std::string>(propName, path);
}

// ===== Per-State Tweens =====

const ToggleButtonStateTween& ToggleButton::GetStateTween(ToggleButtonState state) const {
    return m_StateTweens[static_cast<int>(state)];
}

void ToggleButton::SetStateTween(ToggleButtonState state, const ToggleButtonStateTween& tween) {
    m_StateTweens[static_cast<int>(state)] = tween;
}

// ===== Per-State Callbacks =====

void ToggleButton::SetStateCallback(ToggleButtonState state, StateCallback callback) {
    m_StateCallbacks[state] = callback;
}

void ToggleButton::ClearStateCallback(ToggleButtonState state) {
    m_StateCallbacks.erase(state);
}

// ===== Private Methods =====

void ToggleButton::UpdateMouseInteraction(float) {
    if (!m_ButtonEnabled) {
        if (m_CurrentState != ToggleButtonState::Disabled) {
            TransitionToState(ToggleButtonState::Disabled);
        }
        return;
    }

    input::InputManager& inputMgr = input::InputManager::Get();

    // Mouse in the centered canvas space the control rects live in, so hit-testing
    // matches rendering on every backend (shared helper).
    Vec2 mousePos = GetCanvasMousePosition();

    bool isOver = IsMouseOver(mousePos) && IsTopPointerTarget(mousePos);
    bool leftPressed = inputMgr.IsMouseButtonPressed(input::MouseButton::Left);

    // Track when press starts inside button
    if (leftPressed && isOver && !m_PressedInside) {
        // Only set if we're transitioning into pressed state
        if (m_CurrentState != ToggleButtonState::Pressed &&
            m_CurrentState != ToggleButtonState::ToggledPressed) {
            m_PressedInside = true;
        }
    }

    // Check if we were in a pressed state and now releasing while over
    bool wasInPressedState = (m_CurrentState == ToggleButtonState::Pressed ||
                              m_CurrentState == ToggleButtonState::ToggledPressed);

    // Toggle when releasing from pressed state while still over the button
    if (wasInPressedState && !leftPressed && isOver && m_PressedInside) {
        SetToggled(!m_IsToggled);
        m_PressedInside = false;
    }

    // Reset pressed tracking if mouse released or moved off
    if (!leftPressed || !isOver) {
        m_PressedInside = false;
    }

    // Determine new state based on mouse interaction and toggle state
    ToggleButtonState newState = m_CurrentState;

    if (m_IsToggled) {
        // Toggled states
        if (isOver && leftPressed) {
            newState = ToggleButtonState::ToggledPressed;
        } else if (isOver) {
            newState = ToggleButtonState::ToggledHover;
        } else {
            newState = ToggleButtonState::Toggled;
        }
    } else {
        // Normal states
        if (isOver && leftPressed) {
            newState = ToggleButtonState::Pressed;
        } else if (isOver) {
            newState = ToggleButtonState::Hover;
        } else {
            newState = ToggleButtonState::Normal;
        }
    }

    if (newState != m_CurrentState) {
        TransitionToState(newState);
    }
}

bool ToggleButton::IsMouseOver(const Vec2& mousePos) const {
    if (!GetOwner()) {
        return false;
    }

    core::Node2D* node2D = dynamic_cast<core::Node2D*>(GetOwner());
    if (!node2D) {
        return false;
    }

    Vec2 position = node2D->GetGlobalPosition();
    Vec2 size = GetBoundsSize();
    float rotation = node2D->GetGlobalRotation();

    // Transform mouse position to local button space
    Vec2 localMousePos = mousePos - position;

    // Apply inverse rotation
    if (std::abs(rotation) > 0.001f) {
        float cosR = std::cos(-rotation * DEG_TO_RAD);
        float sinR = std::sin(-rotation * DEG_TO_RAD);
        Vec2 rotated;
        rotated.x = localMousePos.x * cosR - localMousePos.y * sinR;
        rotated.y = localMousePos.x * sinR + localMousePos.y * cosR;
        localMousePos = rotated;
    }

    // Check if within bounds
    Vec2 halfSize = size * 0.5f;
    if (localMousePos.x < -halfSize.x || localMousePos.x > halfSize.x ||
        localMousePos.y < -halfSize.y || localMousePos.y > halfSize.y) {
        return false;
    }

    return true;
}

void ToggleButton::TransitionToState(ToggleButtonState newState) {
    if (newState == m_CurrentState) {
        return;
    }

    m_PreviousState = m_CurrentState;
    m_CurrentState = newState;

    // Play state sound
    PlayStateSound(newState);

    // Invoke state callback
    InvokeStateCallback(newState);

    // Start tween animation
    const ToggleButtonStateTween& tween = m_StateTweens[static_cast<int>(newState)];
    if (tween.enabled) {
        m_TweenStartScaleOffset = m_CurrentScaleOffset;
        m_TweenStartRotationOffset = m_CurrentRotationOffset;
        m_TweenStartPositionOffset = m_CurrentPositionOffset;
        m_TweenProgress = 0.0f;
        m_IsTweening = true;
    } else {
        // Snap to target immediately
        m_CurrentScaleOffset = tween.scaleOffset;
        m_CurrentRotationOffset = tween.rotationOffset;
        m_CurrentPositionOffset = tween.positionOffset;
        m_IsTweening = false;
    }

    m_MeshNeedsRegeneration = true;
    m_TextMeshNeedsRegeneration = true;
}

void ToggleButton::PlayStateSound(ToggleButtonState state) {
    const ToggleButtonStateSound& sound = m_StateSounds[static_cast<int>(state)];

    if (sound.audioPath.empty()) {
        return;
    }

    // Load audio asset if not already loaded
    if (!sound.audioAsset) {
        auto& mutableSound = const_cast<ToggleButtonStateSound&>(sound);
        mutableSound.audioAsset = asset::AssetRef<asset::AudioAsset>(new asset::AudioAsset());
        if (!mutableSound.audioAsset->LoadFromFile(sound.audioPath)) {
            
            return;
        }
    }

    // Play the sound
    audio::AudioManager& audioMgr = audio::AudioManager::GetInstance();
    if (sound.audioAsset && sound.audioAsset->IsLoaded()) {
        audioMgr.Play(sound.audioAsset, sound.busName, audio::PlaybackMode::OneShot, sound.volume);
    }
}

void ToggleButton::InvokeStateCallback(ToggleButtonState state) {
    auto it = m_StateCallbacks.find(state);
    if (it != m_StateCallbacks.end() && it->second) {
        it->second();
    }
}

void ToggleButton::SyncFromProperties() {
    // Sync corner radius
    Vec4 cornerRadiusVec = GetPropertyValue<Vec4>("cornerRadius");
    bool cornerRadiusLinked = GetPropertyValue<bool>("cornerRadiusLinked");
    m_CornerRadius.FromVec4(cornerRadiusVec);
    m_CornerRadius.SetLinked(cornerRadiusLinked);

    // Sync border width
    Vec4 borderWidthVec = GetPropertyValue<Vec4>("borderWidth");
    bool borderWidthLinked = GetPropertyValue<bool>("borderWidthLinked");
    m_BorderWidth.FromVec4(borderWidthVec);
    m_BorderWidth.SetLinked(borderWidthLinked);

    LoadThemedStateData();

    // Sync button enabled state
    m_ButtonEnabled = GetPropertyValue<bool>("buttonEnabled");
    m_StyleMode = static_cast<ToggleButtonStyleMode>(GetPropertyValue<int>("styleMode"));
}

void ToggleButton::LoadThemedStateData() {
    struct StateMap { ToggleButtonState state; const char* prefix; };
    static const StateMap kStates[] = {
        { ToggleButtonState::Normal,         "normal" },
        { ToggleButtonState::Hover,          "hover" },
        { ToggleButtonState::Pressed,        "pressed" },
        { ToggleButtonState::Toggled,        "toggled" },
        { ToggleButtonState::ToggledHover,   "toggledHover" },
        { ToggleButtonState::ToggledPressed, "toggledPressed" },
        { ToggleButtonState::Disabled,       "disabled" },
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

void ToggleButton::RenderBackground(RenderContext& ctx, const Vec2& position, const Vec2& size, float rotation) {
    // Apply state modulation
    Color bgColor = GetBackgroundColor();
    float opacity = GetOpacity();
    bgColor.a *= opacity;

    // Apply state modulation color
    const Color& stateModulation = m_StateStyles[static_cast<int>(m_CurrentState)].modulationColor;
    bgColor.r *= stateModulation.r;
    bgColor.g *= stateModulation.g;
    bgColor.b *= stateModulation.b;
    bgColor.a *= stateModulation.a;

    // Apply tween transformations
    Vec2 tweenedPosition = position + m_CurrentPositionOffset;
    Vec2 tweenedSize = size + m_CurrentScaleOffset;
    float tweenedRotation = rotation + m_CurrentRotationOffset;

    // A themed StyleBox entry for the current state, when the effective theme defines
    // one, fully replaces the flat property-driven background+border for that state,
    // tinted by the state modulation colour and the control's opacity.
    if (std::shared_ptr<StyleBox> themedBox = ResolveThemedStyleBox(ToggleButtonStateThemeEntry(m_CurrentState))) {
        Color modulate = stateModulation;
        modulate.a *= opacity;
        DrawStyleBox(ctx, themedBox.get(), tweenedPosition, tweenedSize, tweenedRotation, modulate);
        return;
    }

    Vec4 cornerRadiusVec = m_CornerRadius.AsVec4();

    // Draw border FIRST (outer), then background (inner) - like Button
    if (GetBorderEnabled()) {
        float borderTop = m_BorderWidth.Get(0);
        float borderRight = m_BorderWidth.Get(1);
        float borderBottom = m_BorderWidth.Get(2);
        float borderLeft = m_BorderWidth.Get(3);

        if (borderTop > 0.0f || borderRight > 0.0f || borderBottom > 0.0f || borderLeft > 0.0f) {
            Color borderColor = GetBorderColor();
            // Apply state modulation to border as well
            borderColor.r *= stateModulation.r;
            borderColor.g *= stateModulation.g;
            borderColor.b *= stateModulation.b;
            borderColor.a *= stateModulation.a;

            Vec4 innerRadius = cornerRadiusVec;
            Vec2 outerSize = Vec2(tweenedSize.x + borderLeft + borderRight, tweenedSize.y + borderTop + borderBottom);

            Vec4 outerRadius = Vec4(
                innerRadius.x + std::max(borderTop, borderLeft),
                innerRadius.y + std::max(borderTop, borderRight),
                innerRadius.z + std::max(borderBottom, borderRight),
                innerRadius.w + std::max(borderBottom, borderLeft)
            );

            Vec4 borderWidthVec = Vec4(borderTop, borderRight, borderBottom, borderLeft);

            if (std::abs(tweenedRotation) > 0.0001f) {
                // Rotated: position is center-pivot, use as-is
                ctx.drawRoundedRectBorder(
                    tweenedPosition,
                    outerSize,
                    outerRadius,
                    borderWidthVec,
                    borderColor,
                    tweenedRotation
                );
            } else {
                // Non-rotated: convert center-pivot to top-left
                Vec2 topLeft = Vec2(tweenedPosition.x - tweenedSize.x * 0.5f, tweenedPosition.y - tweenedSize.y * 0.5f);
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

    // Draw background (inner)
    if (std::abs(tweenedRotation) > 0.0001f) {
        // Rotated: position is center-pivot, use as-is
        ctx.drawRoundedRect(
            tweenedPosition,
            tweenedSize,
            cornerRadiusVec,
            bgColor,
            tweenedRotation,
            0
        );
    } else {
        // Non-rotated: convert center-pivot to top-left
        Vec2 topLeft = Vec2(tweenedPosition.x - tweenedSize.x * 0.5f, tweenedPosition.y - tweenedSize.y * 0.5f);
        ctx.drawRoundedRect(
            topLeft,
            tweenedSize,
            cornerRadiusVec,
            bgColor,
            0
        );
    }
}

Vec2 ToggleButton::CalculateTextSize() const {
    std::string text = GetText();
    float fontSize = GetFontSize();

    // Measure precisely against the loaded font atlas (matches Button) so the label
    // centers correctly; fall back to a rough estimate before the atlas has loaded.
    if (m_FontAsset.IsValid() && m_FontAsset->IsLoaded()) {
        return m_FontAsset->MeasureText(text, fontSize);
    }

    return Vec2(static_cast<float>(text.length()) * fontSize * 0.6f, fontSize);
}

Color ToggleButton::GetEffectiveFontColor() const {
    Color color = GetFontColor();
    color.a *= GetOpacity();

    const Color& stateModulation = m_StateStyles[static_cast<int>(m_CurrentState)].modulationColor;
    color.r *= stateModulation.r;
    color.g *= stateModulation.g;
    color.b *= stateModulation.b;
    color.a *= stateModulation.a;
    return color;
}

void ToggleButton::RegenerateTextMesh(RenderContext& ctx, const Vec2& position, const Vec2& size) {
    (void)size;
    std::string text = GetText();
    float fontSize = GetFontSize();

    if (text.empty()) {
        if (m_TextMesh.isValid()) {
            ctx.getDevice()->destroyMesh(m_TextMesh);
            m_TextMesh = MeshHandle();
        }
        m_CachedText = "";
        m_CachedFontSize = 0.0f;
        m_CachedPosition = position;
        return;
    }

    const FontAtlas* fontAtlas = ctx.getDevice()->getFontAtlas(m_FontHandle);
    if (!fontAtlas) {
        return;
    }

    Vec2 textSize = CalculateTextSize();
    Vec2 textPos;
    // Position is center-pivot, so center the text around it.
    textPos.x = position.x - textSize.x * 0.5f;

    if (m_FontAsset.IsValid() && m_FontAsset->IsLoaded()) {
        float scaleFactor = fontSize / m_FontAsset->GetFontSize();
        float ascent = m_FontAsset->GetAscent() * scaleFactor;
        float descent = m_FontAsset->GetDescent() * scaleFactor;
        textPos.y = position.y - (ascent + descent) * 0.5f;
    } else {
        textPos.y = position.y;
    }

    float scale = fontSize / fontAtlas->fontSize;
    MeshData textMeshData;
    Vec2 cursor = textPos;
    uint32_t vertexOffset = 0;

    const Color fontColor = GetEffectiveFontColor();

    for (size_t i = 0; i < text.length(); ++i) {
        char c = text[i];
        uint32_t codepoint = static_cast<uint32_t>(c);

        if (c == '\n') {
            cursor.x = textPos.x;
            cursor.y += fontAtlas->lineHeight * scale;
            continue;
        }

        const Glyph* glyph = fontAtlas->getGlyph(codepoint);
        if (!glyph) {
            if (c == ' ') {
                cursor.x += fontAtlas->fontSize * 0.25f * scale;
            }
            continue;
        }

        Vec2 glyphPos;
        glyphPos.x = cursor.x + glyph->bearing.x * scale;
        glyphPos.y = cursor.y - glyph->bearing.y * scale;
        Vec2 glyphSize = glyph->size * scale;

        float textZ = 0.1f;

        Vertex v0, v1, v2, v3;
        v0.position = Vec3(glyphPos.x, glyphPos.y - glyphSize.y, textZ);
        v0.normal = Vec3(0, 0, 1);
        v0.texCoord = Vec2(glyph->uvMin.x, glyph->uvMax.y);
        v0.color = Vec4(fontColor.r, fontColor.g, fontColor.b, fontColor.a);

        v1.position = Vec3(glyphPos.x + glyphSize.x, glyphPos.y - glyphSize.y, textZ);
        v1.normal = Vec3(0, 0, 1);
        v1.texCoord = Vec2(glyph->uvMax.x, glyph->uvMax.y);
        v1.color = Vec4(fontColor.r, fontColor.g, fontColor.b, fontColor.a);

        v2.position = Vec3(glyphPos.x + glyphSize.x, glyphPos.y, textZ);
        v2.normal = Vec3(0, 0, 1);
        v2.texCoord = Vec2(glyph->uvMax.x, glyph->uvMin.y);
        v2.color = Vec4(fontColor.r, fontColor.g, fontColor.b, fontColor.a);

        v3.position = Vec3(glyphPos.x, glyphPos.y, textZ);
        v3.normal = Vec3(0, 0, 1);
        v3.texCoord = Vec2(glyph->uvMin.x, glyph->uvMin.y);
        v3.color = Vec4(fontColor.r, fontColor.g, fontColor.b, fontColor.a);

        textMeshData.vertices.push_back(v0);
        textMeshData.vertices.push_back(v1);
        textMeshData.vertices.push_back(v2);
        textMeshData.vertices.push_back(v3);

        textMeshData.indices.push_back(vertexOffset + 0);
        textMeshData.indices.push_back(vertexOffset + 1);
        textMeshData.indices.push_back(vertexOffset + 2);
        textMeshData.indices.push_back(vertexOffset + 0);
        textMeshData.indices.push_back(vertexOffset + 2);
        textMeshData.indices.push_back(vertexOffset + 3);

        vertexOffset += 4;
        cursor.x += glyph->advance * scale;
    }

    if (m_TextMesh.isValid()) {
        ctx.getDevice()->destroyMesh(m_TextMesh);
    }

    m_TextMesh = ctx.getDevice()->createMesh(textMeshData);
    m_CachedText = text;
    m_CachedFontSize = fontSize;
    m_CachedPosition = position;
    m_CachedFontColor = fontColor;
    m_TextMeshNeedsRegeneration = false;
}

void ToggleButton::RenderText(RenderContext& ctx, const Vec2& position, const Vec2& size) {
    std::string text = GetText();
    if (text.empty()) {
        return;
    }

    if (!m_FontHandle.isValid()) {
        return;
    }

    float fontSize = GetFontSize();
    Color fontColor = GetEffectiveFontColor();

    // Rebuild the glyph mesh whenever anything baked into it changes (text, size,
    // placement or the state-modulated colour), since the mesh caches vertex colours.
    if (m_TextMeshNeedsRegeneration || text != m_CachedText ||
        std::abs(fontSize - m_CachedFontSize) > 0.01f ||
        position != m_CachedPosition || fontColor != m_CachedFontColor ||
        !m_TextMesh.isValid()) {
        RegenerateTextMesh(ctx, position, size);
    }

    if (!m_TextMesh.isValid()) {
        return;
    }

    const FontAtlas* fontAtlas = ctx.getDevice()->getFontAtlas(m_FontHandle);
    if (!fontAtlas) {
        return;
    }

    MaterialPropertyBlock overrides;
    overrides.setTexture("u_FontAtlas", fontAtlas->texture);
    overrides.setColor("u_TextColor", fontColor);

    Mat4 transform = Mat4::Identity();
    ctx.drawMesh(m_TextMesh, ctx.getDefaultTextMaterial(), transform, overrides);
}

// ===== IRenderableComponent Implementation =====

void ToggleButton::buildDrawCommands(RenderContext& ctx) {
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


    // A theme may supply a uniform corner radius (applied only when defined and the
    // property is not a local override). Reload from the property first so the themed
    // value stays transient and never overwrites the stored cornerRadius.
    Vec4 cornerRadiusVec = GetPropertyValue<Vec4>("cornerRadius");
    bool cornerRadiusLinked = GetPropertyValue<bool>("cornerRadiusLinked");
    m_CornerRadius.FromVec4(cornerRadiusVec);
    m_CornerRadius.SetLinked(cornerRadiusLinked);
    if (!IsThemeOverridden("cornerRadius")) {
        const ui::ThemeAsset* theme = GetEffectiveTheme();
        ui::ThemeManager& tm = ui::ThemeManager::GetInstance();
        if (tm.HasConstant(theme, GetThemeTypeName(), GetThemeTypeVariation(), "corner_radius")) {
            float r = tm.ResolveConstant(theme, GetThemeTypeName(), GetThemeTypeVariation(), "corner_radius", 0.0f);
            m_CornerRadius.FromVec4(Vec4(r, r, r, r));
            m_CornerRadius.SetLinked(true);
        }
    }

    // Ensure the font atlas is loaded, falling back to the project's default font when
    // none is set so a toggle button with no explicit font still renders its text
    // (matches Button / Label).
    float currentFontSize = GetFontSize();
    std::string fontPath = GetFontPath();
    if (fontPath.empty()) {
        asset::AssetDatabase& assetDb = asset::AssetDatabase::GetInstance();
        if (assetDb.IsInitialized()) {
            fontPath = assetDb.GetDefaultFontPath();
        }
    }

    if (fontPath != m_CurrentFontPath || currentFontSize != m_CurrentFontSize) {
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
                // The label is measured with this font, which loads lazily on the first
                // draw -- after any parent container measured this control without it.
                NotifyContentSizeChanged();
            }
        }

        m_TextMeshNeedsRegeneration = true;
    }

    if (m_FontAsset.IsValid() && m_FontAsset->IsLoaded() && !m_FontHandle.isValid()) {
        FontDesc fontDesc;
        // GetPhysicalPath() resolves a res:// path to a filesystem path for loading.
        fontDesc.fontPath = m_FontAsset->GetPhysicalPath();
        fontDesc.fontSize = currentFontSize;
        fontDesc.atlasWidth = m_FontAsset->GetAtlasWidth();
        fontDesc.atlasHeight = m_FontAsset->GetAtlasHeight();

        IGfxDevice* device = ctx.getDevice();
        if (device) {
            m_FontHandle = device->createFontAtlas(fontDesc);
        }
    }

    Vec2 position = GetResolvedRect().GetCenter();
    Vec2 size = GetBoundsSize();
    float rotation = node2D->GetGlobalRotation();

    RenderBackground(ctx, position, size, rotation);
    RenderText(ctx, position, size);
}

AABB ToggleButton::getWorldBounds() const {
    if (!GetOwner()) {
        return AABB();
    }

    core::Node2D* node2D = dynamic_cast<core::Node2D*>(GetOwner());
    if (!node2D) {
        return AABB();
    }

    Vec2 position = node2D->GetGlobalPosition();
    Vec2 size = GetBoundsSize();
    Vec2 halfSize = size * 0.5f;

    return AABB(
        Vec3(position.x - halfSize.x, position.y - halfSize.y, -0.1f),
        Vec3(position.x + halfSize.x, position.y + halfSize.y, 0.1f)
    );
}

RenderLayer ToggleButton::getRenderLayer() const {
    int layer = GetLayer();
    int sortingOrder = GetSortingOrder();
    return static_cast<RenderLayer>(layer * 1000 + sortingOrder);
}

SpatialType ToggleButton::getSpatialType() const {
    return GetUISpatialType();
}

Vec2 ToggleButton::GetContentMinSize() const {
    // A toggle button's minimum content is its estimated label size.
    return CalculateTextSize();
}

OBB ToggleButton::getOrientedBounds() const {
    if (!GetOwner()) {
        return OBB();
    }

    core::Node2D* node2D = dynamic_cast<core::Node2D*>(GetOwner());
    if (!node2D) {
        return OBB();
    }

    Vec2 position = node2D->GetGlobalPosition();
    Vec2 size = GetBoundsSize();
    float rotation = node2D->GetGlobalRotation();

    Vec3 center(position.x, position.y, 0.0f);
    Vec3 halfExtents(size.x * 0.5f, size.y * 0.5f, 0.1f);
    Quat quatRotation = Quat::FromAxisAngle(Vec3::UnitZ(), rotation);

    return OBB(center, halfExtents, quatRotation);
}

bool ToggleButton::IntersectRay(const Ray& ray, float& outDistance) const {
    OBB obb = getOrientedBounds();

    Vec3 localRayOrigin = obb.rotation.Inverse() * (ray.origin - obb.center);
    Vec3 localRayDir = obb.rotation.Inverse() * ray.direction;

    AABB localAABB(
        Vec3(-obb.extents.x, -obb.extents.y, -obb.extents.z),
        Vec3(obb.extents.x, obb.extents.y, obb.extents.z)
    );

    Ray localRay(localRayOrigin, localRayDir);
    return localRay.IntersectAABB(localAABB, outDistance);
}

bool ToggleButton::OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) {
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
        m_TextMeshNeedsRegeneration = true;
        return true;
    }

    return false;
}

} // namespace components
} // namespace lupine

