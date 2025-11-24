#include "lupine/components/Button.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/Mesh.hpp"
#include "lupine/rendering/Font.hpp"
#include "lupine/rendering/DrawCommand.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/rendering/gfx/GfxDescriptors.hpp"
#include "lupine/input/InputManager.hpp"
#include "lupine/rendering/ViewportUtils.hpp"
#include "lupine/logger/Logger.hpp"

namespace lupine {
namespace components {

using namespace core;
using namespace math;

using core::Node2D;
using core::Node3D;

Button::Button()
    : Component("Button")
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
    : Component(name)
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

    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(width, 120.0f, 0.0f, 10000.0f, 1.0f, "Size"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(height, 40.0f, 0.0f, 10000.0f, 1.0f, "Size"));

    DefineProperty(PROPERTY_INT_RANGE_GROUP(layer, 0, -100, 100, 1, "Rendering"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(sortingOrder, 0, -1000, 1000, 1, "Rendering"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(useUISpace, Bool, true, "Rendering"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(buttonEnabled, Bool, true, "Button"));
    DefineProperty(PROPERTY_ENUM_GROUP(styleMode, 0, "Button", Automatic, Manual));
    DefineProperty(PROPERTY_ENUM_GROUP(scaleMode, 0, "Button", Fixed, FitToText, FitToTextWidth));

    DefineProperty(PROPERTY_DEFAULT_GROUP(textPadding, Vec2, Vec2(10.0f, 5.0f), "Button"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(backgroundColor, Color, Color(0.3f, 0.3f, 0.3f, 1.0f), "Background"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(opacity, 1.0f, 0.0f, 1.0f, 0.01f, "Background"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(borderEnabled, Bool, true, "Border"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderWidthLinked, Bool, true, "Border"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderWidth, Vec4, Vec4(2.0f, 2.0f, 2.0f, 2.0f), "Border"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderColor, Color, Color(0.5f, 0.5f, 0.5f, 1.0f), "Border"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(cornerRadiusLinked, Bool, true, "CornerRadius"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(cornerRadius, Vec4, Vec4(4.0f, 4.0f, 4.0f, 4.0f), "CornerRadius"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(text, String, std::string("Button"), "Text"));
    DefineProperty(PROPERTY_FILE_GROUP(fontPath, std::string(""), "*.ttf,*.otf", "Text"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(fontSize, 16.0f, 1.0f, 256.0f, 1.0f, "Text"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(fontColor, Color, Color::White(), "Text"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(wordWrap, Bool, false, "Text"));

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

    m_StateStyles[static_cast<int>(ButtonState::Normal)].modulationColor = GetPropertyValue<Color>("normalModulation");
    m_StateStyles[static_cast<int>(ButtonState::Hover)].modulationColor = GetPropertyValue<Color>("hoverModulation");
    m_StateStyles[static_cast<int>(ButtonState::Pressed)].modulationColor = GetPropertyValue<Color>("pressedModulation");
    m_StateStyles[static_cast<int>(ButtonState::Disabled)].modulationColor = GetPropertyValue<Color>("disabledModulation");

    auto loadTween = [this](ButtonState state, const std::string& prefix) {
        int idx = static_cast<int>(state);
        m_StateTweens[idx].enabled = GetPropertyValue<bool>(prefix + "TweenEnabled");
        m_StateTweens[idx].scaleOffset = GetPropertyValue<Vec2>(prefix + "TweenScale");
        m_StateTweens[idx].rotationOffset = GetPropertyValue<float>(prefix + "TweenRotation");
        m_StateTweens[idx].positionOffset = GetPropertyValue<Vec2>(prefix + "TweenPosition");
        m_StateTweens[idx].duration = GetPropertyValue<float>(prefix + "TweenDuration");
    };

    loadTween(ButtonState::Normal, "normal");
    loadTween(ButtonState::Hover, "hover");
    loadTween(ButtonState::Pressed, "pressed");
    loadTween(ButtonState::Disabled, "disabled");

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

    if (!is3D && m_ScaleMode == ButtonScaleMode::Fixed) {
        float currentWidth = GetWidth();
        float currentHeight = GetHeight();

        if (axis == 0) {

            SetWidth(std::max(0.1f, currentWidth + scaleDelta * currentWidth));
        } else if (axis == 1) {

            SetHeight(std::max(0.1f, currentHeight + scaleDelta * currentHeight));
        } else if (axis == -1) {

            SetWidth(std::max(0.1f, currentWidth + scaleDelta * currentWidth));
            SetHeight(std::max(0.1f, currentHeight + scaleDelta * currentHeight));
        }

        m_MeshNeedsRegeneration = true;

        return true;
    }

    return false;
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

bool Button::GetUseUISpace() const {
    return GetPropertyValue<bool>("useUISpace");
}

void Button::SetUseUISpace(bool useUISpace) {
    SetPropertyValue<bool>("useUISpace", useUISpace);
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

const Color& Button::GetBackgroundColor() const {
    static Color cachedColor;
    const ComponentProperty* prop = m_CustomProperties.GetProperty("backgroundColor");
    if (prop) {
        cachedColor = prop->GetValue<Color>();
        return cachedColor;
    }
    return Color::White();
}

void Button::SetBackgroundColor(const Color& color) {
    SetPropertyValue<Color>("backgroundColor", color);
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

const Color& Button::GetBorderColor() const {
    static Color cachedColor;
    static Color defaultColor(0.5f, 0.5f, 0.5f, 1.0f);
    const ComponentProperty* prop = m_CustomProperties.GetProperty("borderColor");
    if (prop && !prop->GetValueAsJson().is_null()) {
        cachedColor = prop->GetValue<Color>();
        return cachedColor;
    }
    return defaultColor;
}

void Button::SetBorderColor(const Color& color) {
    SetPropertyValue<Color>("borderColor", color);
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
    SetPropertyValue<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
    m_MeshNeedsRegeneration = true;
}

float Button::GetCornerRadiusTopRight() const {
    return m_CornerRadius.Get(1);
}

void Button::SetCornerRadiusTopRight(float radius) {
    m_CornerRadius.Set(1, radius);
    SetPropertyValue<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
    m_MeshNeedsRegeneration = true;
}

float Button::GetCornerRadiusBottomLeft() const {
    return m_CornerRadius.Get(3);
}

void Button::SetCornerRadiusBottomLeft(float radius) {
    m_CornerRadius.Set(3, radius);
    SetPropertyValue<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
    m_MeshNeedsRegeneration = true;
}

float Button::GetCornerRadiusBottomRight() const {
    return m_CornerRadius.Get(2);
}

void Button::SetCornerRadiusBottomRight(float radius) {
    m_CornerRadius.Set(2, radius);
    SetPropertyValue<Vec4>("cornerRadius", m_CornerRadius.AsVec4());
    m_MeshNeedsRegeneration = true;
}

std::string Button::GetText() const {
    return GetPropertyValue<std::string>("text");
}

void Button::SetText(const std::string& text) {
    SetPropertyValue<std::string>("text", text);
    m_MeshNeedsRegeneration = true;
    m_TextMeshNeedsRegeneration = true;
}

std::string Button::GetFontPath() const {
    return GetPropertyValue<std::string>("fontPath");
}

void Button::SetFontPath(const std::string& path) {
    SetPropertyValue<std::string>("fontPath", path);
    m_CurrentFontPath = path;
    m_MeshNeedsRegeneration = true;
}

float Button::GetFontSize() const {
    return GetPropertyValue<float>("fontSize");
}

void Button::SetFontSize(float size) {
    SetPropertyValue<float>("fontSize", size);
    m_CurrentFontSize = size;
    m_MeshNeedsRegeneration = true;
    m_TextMeshNeedsRegeneration = true;
}

const Color& Button::GetFontColor() const {
    static Color cachedColor;
    const ComponentProperty* prop = m_CustomProperties.GetProperty("fontColor");
    if (prop) {
        cachedColor = prop->GetValue<Color>();
        return cachedColor;
    }
    return Color::White();
}

void Button::SetFontColor(const Color& color) {
    SetPropertyValue<Color>("fontColor", color);
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

    m_StateStyles[static_cast<int>(ButtonState::Normal)].modulationColor = GetPropertyValue<Color>("normalModulation");
    m_StateStyles[static_cast<int>(ButtonState::Hover)].modulationColor = GetPropertyValue<Color>("hoverModulation");
    m_StateStyles[static_cast<int>(ButtonState::Pressed)].modulationColor = GetPropertyValue<Color>("pressedModulation");
    m_StateStyles[static_cast<int>(ButtonState::Disabled)].modulationColor = GetPropertyValue<Color>("disabledModulation");

    m_ButtonEnabled = GetPropertyValue<bool>("buttonEnabled");
    m_StyleMode = static_cast<ButtonStyleMode>(GetPropertyValue<int>("styleMode"));
    m_ScaleMode = static_cast<ButtonScaleMode>(GetPropertyValue<int>("scaleMode"));
}

void Button::UpdateMouseInteraction(float deltaTime) {

    if (!m_ButtonEnabled) {
        if (m_CurrentState != ButtonState::Disabled) {
            TransitionToState(ButtonState::Disabled);
        }
        return;
    }

    input::InputManager& inputMgr = input::InputManager::Get();

    SpatialType spatialType = getSpatialType();

    glm::vec2 rawMousePos = inputMgr.GetMousePosition();

    Viewport viewport = GetCurrentViewport();
    Vec2 logicalSize = GetLogicalCanvasSize();
    glm::ivec2 windowSize = inputMgr.GetWindowSize();

    glm::vec2 mouseWindowGL;
    mouseWindowGL.x = rawMousePos.x;
    mouseWindowGL.y = static_cast<float>(windowSize.y) - rawMousePos.y;

    glm::vec2 viewportRelativePos;
    viewportRelativePos.x = mouseWindowGL.x - viewport.x;
    viewportRelativePos.y = mouseWindowGL.y - viewport.y;

    glm::vec2 normalizedPos;
    normalizedPos.x = viewportRelativePos.x / viewport.width;
    normalizedPos.y = viewportRelativePos.y / viewport.height;

    glm::vec2 mousePosGame;
    mousePosGame.x = normalizedPos.x * logicalSize.x;
    mousePosGame.y = normalizedPos.y * logicalSize.y;

    Vec2 mousePos(mousePosGame.x, mousePosGame.y);

    bool isOver = IsMouseOver(mousePos);

    bool leftPressed = inputMgr.IsMouseButtonPressed(input::MouseButton::Left);
    bool leftJustPressed = inputMgr.IsMouseButtonJustPressed(input::MouseButton::Left);
    bool leftJustReleased = inputMgr.IsMouseButtonJustReleased(input::MouseButton::Left);

    Vec2 position = Vec2(0, 0);
    Vec2 size = CalculateButtonSize();
    if (GetOwner()) {
        core::Node2D* node2D = dynamic_cast<core::Node2D*>(GetOwner());
        if (node2D) {
            position = node2D->GetGlobalPosition();
        }
    }

    Vec2 buttonCenter = Vec2(position.x + size.x * 0.5f, position.y + size.y * 0.5f);
    Vec2 toCenter = buttonCenter - mousePos;
    float dist = sqrtf(toCenter.x * toCenter.x + toCenter.y * toCenter.y);

    ButtonState newState = m_CurrentState;

    if (isOver) {
        if (leftPressed) {
            newState = ButtonState::Pressed;
        } else {
            newState = ButtonState::Hover;
        }
    } else {
        if (leftPressed && m_CurrentState == ButtonState::Pressed) {

            newState = ButtonState::Pressed;
        } else {
            newState = ButtonState::Normal;
        }
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
    Vec2 size = CalculateButtonSize();

    position += m_CurrentPositionOffset;

    return mousePos.x >= position.x && mousePos.x <= position.x + size.x &&
           mousePos.y >= position.y && mousePos.y <= position.y + size.y;
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
    }

    InvokeStateCallback(newState);

    m_MeshNeedsRegeneration = true;
    m_TextMeshNeedsRegeneration = true;
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

Vec2 Button::CalculateTextSize() const {
    if (!m_FontAsset.IsValid() || !m_FontAsset->IsLoaded()) {
        return Vec2(0.0f, 0.0f);
    }

    std::string text = GetText();
    float fontSize = GetFontSize();

    return m_FontAsset->MeasureText(text, fontSize);
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

    Vec4 cornerRadiusVec = GetPropertyValue<Vec4>("cornerRadius");
    bool cornerRadiusLinked = GetPropertyValue<bool>("cornerRadiusLinked");
    if (cornerRadiusVec != m_CornerRadius.AsVec4() || cornerRadiusLinked != m_CornerRadius.IsLinked()) {
        m_MeshNeedsRegeneration = true;
    }
    m_CornerRadius.FromVec4(cornerRadiusVec);
    m_CornerRadius.SetLinked(cornerRadiusLinked);

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

    SpatialType spatialType = getSpatialType();
    const char* spatialTypeStr = (spatialType == SpatialType::Canvas) ? "Canvas" :
                                 (spatialType == SpatialType::World2D) ? "World2D" : "World3D";

    std::string fontPath = GetFontPath();
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

            }
        }
    }

    if (m_FontAsset.IsValid() && m_FontAsset->IsLoaded() && !m_FontHandle.isValid()) {

        FontDesc fontDesc;
        fontDesc.fontPath = m_FontAsset->GetPath();
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

    Vec2 position = node2D->GetGlobalPosition();
    Vec2 size = CalculateButtonSize();

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
            Vec2 outerPosition = Vec2(position.x, position.y);

            Vec4 outerRadius = Vec4(
                innerRadius.x + std::max(borderTop, borderLeft),
                innerRadius.y + std::max(borderTop, borderRight),
                innerRadius.z + std::max(borderBottom, borderRight),
                innerRadius.w + std::max(borderBottom, borderLeft)
            );

            Vec4 borderWidthVec = Vec4(borderTop, borderRight, borderBottom, borderLeft);

            ctx.drawRoundedRectBorder(
                outerPosition,
                outerSize,
                outerRadius,
                borderWidthVec,
                borderColor,
                rotation
            );
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
        ctx.drawRoundedRect(
            position,
            size,
            cornerRadiusVec,
            bgColor,
            0
        );
    }
}

void Button::RegenerateTextMesh(RenderContext& ctx, const Vec2& position, const Vec2& size) {
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
    textPos.x = position.x + (size.x - textSize.x) * 0.5f;

    if (m_FontAsset.IsValid() && m_FontAsset->IsLoaded()) {
        float scaleFactor = fontSize / m_FontAsset->GetFontSize();
        float ascent = m_FontAsset->GetAscent() * scaleFactor;
        float descent = m_FontAsset->GetDescent() * scaleFactor;

        float buttonCenterY = position.y + size.y * 0.5f;
        textPos.y = buttonCenterY - (ascent + descent) * 0.5f;
    } else {
        textPos.y = position.y + size.y * 0.5f;
    }

    float scale = fontSize / fontAtlas->fontSize;
    MeshData textMeshData;
    Vec2 cursor = textPos;
    uint32_t vertexOffset = 0;

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
        Color fontColor = GetEffectiveColor(GetFontColor());

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
    Vec2 size = CalculateButtonSize();

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
    Vec2 size = CalculateButtonSize();
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

    if (GetUseUISpace()) {
        return SpatialType::Canvas;
    } else {
        return SpatialType::World2D;
    }
}

}
}

