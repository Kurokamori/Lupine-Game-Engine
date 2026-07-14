#include "lupine/components/Button3D.hpp"
#include "lupine/localization/LocalizationManager.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/Mesh.hpp"
#include "lupine/rendering/Font.hpp"
#include "lupine/rendering/DrawCommand.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/rendering/gfx/GfxDescriptors.hpp"
#include "lupine/input/InputManager.hpp"
#include "lupine/asset/AssetDatabase.hpp"
#include "lupine/logger/Logger.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

using core::Node2D;
using core::Node3D;

Button3D::Button3D()
    : Component("Button3D")
    , m_BaseStyleBox(nullptr)
    , m_CornerRadius(0.0f, true)
    , m_BorderWidth(0.0f, true)
    , m_MeshNeedsRegeneration(true)
{

    m_BaseStyleBox = std::make_shared<StyleBoxFlat>();

    m_StateStyles[static_cast<int>(Button3DState::Normal)].modulationColor = Color::White();
    m_StateStyles[static_cast<int>(Button3DState::Hover)].modulationColor = Color(1.2f, 1.2f, 1.2f, 1.0f);
    m_StateStyles[static_cast<int>(Button3DState::Pressed)].modulationColor = Color(0.8f, 0.8f, 0.8f, 1.0f);
    m_StateStyles[static_cast<int>(Button3DState::Disabled)].modulationColor = Color(0.5f, 0.5f, 0.5f, 0.5f);
}

Button3D::Button3D(const std::string& name)
    : Component(name)
    , m_BaseStyleBox(nullptr)
    , m_CornerRadius(0.0f, true)
    , m_BorderWidth(0.0f, true)
    , m_MeshNeedsRegeneration(true)
{

    m_BaseStyleBox = std::make_shared<StyleBoxFlat>();

    m_StateStyles[static_cast<int>(Button3DState::Normal)].modulationColor = Color::White();
    m_StateStyles[static_cast<int>(Button3DState::Hover)].modulationColor = Color(1.2f, 1.2f, 1.2f, 1.0f);
    m_StateStyles[static_cast<int>(Button3DState::Pressed)].modulationColor = Color(0.8f, 0.8f, 0.8f, 1.0f);
    m_StateStyles[static_cast<int>(Button3DState::Disabled)].modulationColor = Color(0.5f, 0.5f, 0.5f, 0.5f);
}

Button3D::~Button3D() {

}

void Button3D::DefineProperties() {

    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(width, 5.0f, 0.0f, 10000.0f, 0.1f, "Size"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(height, 2.0f, 0.0f, 10000.0f, 0.1f, "Size"));

    DefineProperty(PROPERTY_ENUM_GROUP(billboardMode, 1, "Transform", Disabled, Enabled, YAxisOnly));

    DefineProperty(PROPERTY_DEFAULT_GROUP(doubleSided, Bool, true, "Rendering"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(castShadow, Bool, true, "Rendering"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(receiveShadow, Bool, true, "Rendering"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(layer, 0, -100, 100, 1, "Rendering"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(sortingOrder, 0, -1000, 1000, 1, "Rendering"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(buttonEnabled, Bool, true, "Button"));
    DefineProperty(PROPERTY_ENUM_GROUP(styleMode, 0, "Button", Automatic, Manual));
    DefineProperty(PROPERTY_ENUM_GROUP(scaleMode, 0, "Button", Fixed, FitToText, FitToTextWidth));

    DefineProperty(PROPERTY_DEFAULT_GROUP(textPadding, Vec2, Vec2(0.5f, 0.25f), "Button"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(backgroundColor, Color, Color(0.3f, 0.3f, 0.3f, 1.0f), "Background"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(opacity, 1.0f, 0.0f, 1.0f, 0.01f, "Background"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(borderEnabled, Bool, true, "Border"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderWidthLinked, Bool, true, "Border"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderWidth, Vec4, Vec4(0.1f, 0.1f, 0.1f, 0.1f), "Border"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderColor, Color, Color(0.5f, 0.5f, 0.5f, 1.0f), "Border"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(cornerRadiusLinked, Bool, true, "CornerRadius"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(cornerRadius, Vec4, Vec4(0.2f, 0.2f, 0.2f, 0.2f), "CornerRadius"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(text, String, std::string("Button"), "Text"));

    // localizationKey overrides "text" and resolves through the LocalizationManager.
    DefineProperty(PROPERTY_DEFAULT_GROUP(localizationKey, String, std::string(""), "Localization"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(localizationTable, String, std::string(""), "Localization"));
    DefineProperty(PROPERTY_FILE_GROUP(fontPath, std::string(""), "*.ttf,*.otf", "Text"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(fontSize, 16.0f, 1.0f, 256.0f, 1.0f, "Text"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(fontColor, Color, Color::White(), "Text"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(wordWrap, Bool, false, "Text"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(normalModulation, Color, Color::White(), "State_Normal"));
    DefineProperty(PROPERTY_FILE_GROUP(normalSoundPath, std::string(""), "*.wav,*.ogg,*.mp3", "State_Normal"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(normalTweenEnabled, Bool, false, "State_Normal"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(normalTweenScale, Vec2, Vec2(0.0f, 0.0f), "State_Normal"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(normalTweenRotation, 0.0f, -360.0f, 360.0f, 1.0f, "State_Normal"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(normalTweenPosition, Vec2, Vec2(0.0f, 0.0f), "State_Normal"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(normalTweenDuration, 0.2f, 0.0f, 5.0f, 0.01f, "State_Normal"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(hoverModulation, Color, Color(1.2f, 1.2f, 1.2f, 1.0f), "State_Hover"));
    DefineProperty(PROPERTY_FILE_GROUP(hoverSoundPath, std::string(""), "*.wav,*.ogg,*.mp3", "State_Hover"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(hoverTweenEnabled, Bool, true, "State_Hover"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(hoverTweenScale, Vec2, Vec2(0.05f, 0.05f), "State_Hover"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(hoverTweenRotation, 0.0f, -360.0f, 360.0f, 1.0f, "State_Hover"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(hoverTweenPosition, Vec2, Vec2(0.0f, 0.0f), "State_Hover"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(hoverTweenDuration, 0.2f, 0.0f, 5.0f, 0.01f, "State_Hover"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(pressedModulation, Color, Color(0.8f, 0.8f, 0.8f, 1.0f), "State_Pressed"));
    DefineProperty(PROPERTY_FILE_GROUP(pressedSoundPath, std::string(""), "*.wav,*.ogg,*.mp3", "State_Pressed"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(pressedTweenEnabled, Bool, true, "State_Pressed"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(pressedTweenScale, Vec2, Vec2(-0.05f, -0.05f), "State_Pressed"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(pressedTweenRotation, 0.0f, -360.0f, 360.0f, 1.0f, "State_Pressed"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(pressedTweenPosition, Vec2, Vec2(0.0f, 0.1f), "State_Pressed"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(pressedTweenDuration, 0.1f, 0.0f, 5.0f, 0.01f, "State_Pressed"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(disabledModulation, Color, Color(0.5f, 0.5f, 0.5f, 0.5f), "State_Disabled"));
    DefineProperty(PROPERTY_FILE_GROUP(disabledSoundPath, std::string(""), "*.wav,*.ogg,*.mp3", "State_Disabled"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(disabledTweenEnabled, Bool, false, "State_Disabled"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(disabledTweenScale, Vec2, Vec2(0.0f, 0.0f), "State_Disabled"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(disabledTweenRotation, 0.0f, -360.0f, 360.0f, 1.0f, "State_Disabled"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(disabledTweenPosition, Vec2, Vec2(0.0f, 0.0f), "State_Disabled"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(disabledTweenDuration, 0.2f, 0.0f, 5.0f, 0.01f, "State_Disabled"));
}

void Button3D::OnAwake() {
    SyncFromProperties();
}

void Button3D::OnReady() {

    if (!m_CurrentFontPath.empty()) {

    }
}

void Button3D::OnInput(float) {
    if (!m_ButtonEnabled) {
        if (m_CurrentState != Button3DState::Disabled) {
            TransitionToState(Button3DState::Disabled);
        }
        return;
    }

}

void Button3D::OnUpdate(float deltaTime) {

    if (m_IsTweening) {
        m_TweenProgress += deltaTime;

        const Button3DStateTween& tween = m_StateTweens[static_cast<int>(m_CurrentState)];

        if (m_TweenProgress >= tween.duration) {

            m_TweenProgress = tween.duration;
            m_IsTweening = false;
        }

        float t = tween.duration > 0.0f ? (m_TweenProgress / tween.duration) : 1.0f;
        t = glm::clamp(t, 0.0f, 1.0f);

        m_CurrentScaleOffset = m_TweenStartScaleOffset + (tween.scaleOffset - m_TweenStartScaleOffset) * t;
        m_CurrentRotationOffset = m_TweenStartRotationOffset + (tween.rotationOffset - m_TweenStartRotationOffset) * t;
        m_CurrentPositionOffset = m_TweenStartPositionOffset + (tween.positionOffset - m_TweenStartPositionOffset) * t;
    }
}

void Button3D::OnRender() {

}

bool Button3D::OnGizmoScale(float scaleDelta, int axis, bool is3D) {
    if (!is3D) {
        return false;
    }

    float currentWidth = GetWidth();
    float currentHeight = GetHeight();

    if (axis == 0) {
        SetWidth(currentWidth + scaleDelta);
    } else if (axis == 1) {
        SetHeight(currentHeight + scaleDelta);
    } else if (axis == -1) {
        SetWidth(currentWidth + scaleDelta);
        SetHeight(currentHeight + scaleDelta);
    }

    return true;
}

float Button3D::GetWidth() const {
    return GetPropertyValue<float>("width");
}

void Button3D::SetWidth(float width) {
    SetPropertyValue("width", width);
    m_MeshNeedsRegeneration = true;
}

float Button3D::GetHeight() const {
    return GetPropertyValue<float>("height");
}

void Button3D::SetHeight(float height) {
    SetPropertyValue("height", height);
    m_MeshNeedsRegeneration = true;
}

Button3DBillboardMode Button3D::GetBillboardMode() const {
    return static_cast<Button3DBillboardMode>(GetPropertyValue<int>("billboardMode"));
}

void Button3D::SetBillboardMode(Button3DBillboardMode mode) {
    SetPropertyValue("billboardMode", static_cast<int>(mode));
}

bool Button3D::GetDoubleSided() const {
    return GetPropertyValue<bool>("doubleSided");
}

void Button3D::SetDoubleSided(bool doubleSided) {
    SetPropertyValue("doubleSided", doubleSided);
}

bool Button3D::GetCastShadow() const {
    return GetPropertyValue<bool>("castShadow");
}

void Button3D::SetCastShadow(bool castShadow) {
    SetPropertyValue("castShadow", castShadow);
}

bool Button3D::GetReceiveShadow() const {
    return GetPropertyValue<bool>("receiveShadow");
}

void Button3D::SetReceiveShadow(bool receiveShadow) {
    SetPropertyValue("receiveShadow", receiveShadow);
}

int Button3D::GetLayer() const {
    return GetPropertyValue<int>("layer");
}

void Button3D::SetLayer(int layer) {
    SetPropertyValue("layer", layer);
}

int Button3D::GetSortingOrder() const {
    return GetPropertyValue<int>("sortingOrder");
}

void Button3D::SetSortingOrder(int order) {
    SetPropertyValue("sortingOrder", order);
}

void Button3D::SetEnabled(bool enabled) {
    m_ButtonEnabled = enabled;
    SetPropertyValue("buttonEnabled", enabled);

    if (!enabled && m_CurrentState != Button3DState::Disabled) {
        TransitionToState(Button3DState::Disabled);
    } else if (enabled && m_CurrentState == Button3DState::Disabled) {
        TransitionToState(Button3DState::Normal);
    }
}

void Button3D::SetStyleMode(Button3DStyleMode mode) {
    m_StyleMode = mode;
    SetPropertyValue("styleMode", static_cast<int>(mode));
    m_MeshNeedsRegeneration = true;
}

void Button3D::SetScaleMode(Button3DScaleMode mode) {
    m_ScaleMode = mode;
    SetPropertyValue("scaleMode", static_cast<int>(mode));
    m_MeshNeedsRegeneration = true;
}

void Button3D::SetTextPadding(const Vec2& padding) {
    m_TextPadding = padding;
    SetPropertyValue("textPadding", padding);
    m_MeshNeedsRegeneration = true;
}

void Button3D::SetBaseStyleBox(std::shared_ptr<StyleBox> styleBox) {
    m_BaseStyleBox = styleBox;
    m_MeshNeedsRegeneration = true;
}

Color Button3D::GetBackgroundColor() const {
    if (m_BaseStyleBox) {
        std::shared_ptr<StyleBoxFlat> flatStyle = std::dynamic_pointer_cast<StyleBoxFlat>(m_BaseStyleBox);
        if (flatStyle) {
            return flatStyle->GetBackgroundColor();
        }
    }
    return Color(0.3f, 0.3f, 0.3f, 1.0f);
}

void Button3D::SetBackgroundColor(const Color& color) {
    if (!m_BaseStyleBox) {
        m_BaseStyleBox = std::make_shared<StyleBoxFlat>();
    }
    auto flatStyle = std::dynamic_pointer_cast<StyleBoxFlat>(m_BaseStyleBox);
    if (flatStyle) {
        flatStyle->SetBackgroundColor(color);
        SetPropertyValue("backgroundColor", color);
        m_MeshNeedsRegeneration = true;
    }
}

float Button3D::GetOpacity() const {
    return GetPropertyValue<float>("opacity");
}

void Button3D::SetOpacity(float opacity) {
    SetPropertyValue("opacity", opacity);
    m_MeshNeedsRegeneration = true;
}

bool Button3D::GetBorderWidthLinked() const {
    return m_BorderWidth.IsLinked();
}

void Button3D::SetBorderWidthLinked(bool linked) {
    m_BorderWidth.SetLinked(linked);
    SetPropertyValue("borderWidthLinked", linked);
}

float Button3D::GetBorderWidthLeft() const {
    return m_BorderWidth.Get(0);
}

void Button3D::SetBorderWidthLeft(float width) {
    m_BorderWidth.Set(0, width);
    if (m_BaseStyleBox) {
        auto flatStyle = std::dynamic_pointer_cast<StyleBoxFlat>(m_BaseStyleBox);
        if (flatStyle) {
            flatStyle->SetBorderWidthLeft(width);
        }
    }
    m_MeshNeedsRegeneration = true;
}

float Button3D::GetBorderWidthRight() const {
    return m_BorderWidth.Get(1);
}

void Button3D::SetBorderWidthRight(float width) {
    m_BorderWidth.Set(1, width);
    if (m_BaseStyleBox) {
        auto flatStyle = std::dynamic_pointer_cast<StyleBoxFlat>(m_BaseStyleBox);
        if (flatStyle) {
            flatStyle->SetBorderWidthRight(width);
        }
    }
    m_MeshNeedsRegeneration = true;
}

float Button3D::GetBorderWidthTop() const {
    return m_BorderWidth.Get(2);
}

void Button3D::SetBorderWidthTop(float width) {
    m_BorderWidth.Set(2, width);
    if (m_BaseStyleBox) {
        auto flatStyle = std::dynamic_pointer_cast<StyleBoxFlat>(m_BaseStyleBox);
        if (flatStyle) {
            flatStyle->SetBorderWidthTop(width);
        }
    }
    m_MeshNeedsRegeneration = true;
}

float Button3D::GetBorderWidthBottom() const {
    return m_BorderWidth.Get(3);
}

void Button3D::SetBorderWidthBottom(float width) {
    m_BorderWidth.Set(3, width);
    if (m_BaseStyleBox) {
        auto flatStyle = std::dynamic_pointer_cast<StyleBoxFlat>(m_BaseStyleBox);
        if (flatStyle) {
            flatStyle->SetBorderWidthBottom(width);
        }
    }
    m_MeshNeedsRegeneration = true;
}

bool Button3D::GetBorderEnabled() const {
    return GetPropertyValue<bool>("borderEnabled");
}

void Button3D::SetBorderEnabled(bool enabled) {
    SetPropertyValue("borderEnabled", enabled);

    m_MeshNeedsRegeneration = true;
}

Color Button3D::GetBorderColor() const {
    if (m_BaseStyleBox) {
        std::shared_ptr<StyleBoxFlat> flatStyle = std::dynamic_pointer_cast<StyleBoxFlat>(m_BaseStyleBox);
        if (flatStyle) {
            return flatStyle->GetBorderColorLeft();
        }
    }
    return Color(0.5f, 0.5f, 0.5f, 1.0f);
}

void Button3D::SetBorderColor(const Color& color) {
    if (!m_BaseStyleBox) {
        m_BaseStyleBox = std::make_shared<StyleBoxFlat>();
    }
    auto flatStyle = std::dynamic_pointer_cast<StyleBoxFlat>(m_BaseStyleBox);
    if (flatStyle) {
        flatStyle->SetBorderColorAll(color);
        SetPropertyValue("borderColor", color);
        m_MeshNeedsRegeneration = true;
    }
}

bool Button3D::GetCornerRadiusLinked() const {
    return m_CornerRadius.IsLinked();
}

void Button3D::SetCornerRadiusLinked(bool linked) {
    m_CornerRadius.SetLinked(linked);
    SetPropertyValue("cornerRadiusLinked", linked);
}

float Button3D::GetCornerRadiusTopLeft() const {
    return m_CornerRadius.Get(0);
}

void Button3D::SetCornerRadiusTopLeft(float radius) {
    m_CornerRadius.Set(0, radius);
    if (m_BaseStyleBox) {
        auto flatStyle = std::dynamic_pointer_cast<StyleBoxFlat>(m_BaseStyleBox);
        if (flatStyle) {
            flatStyle->SetCornerRadiusTopLeft(radius);
        }
    }
    m_MeshNeedsRegeneration = true;
}

float Button3D::GetCornerRadiusTopRight() const {
    return m_CornerRadius.Get(1);
}

void Button3D::SetCornerRadiusTopRight(float radius) {
    m_CornerRadius.Set(1, radius);
    if (m_BaseStyleBox) {
        auto flatStyle = std::dynamic_pointer_cast<StyleBoxFlat>(m_BaseStyleBox);
        if (flatStyle) {
            flatStyle->SetCornerRadiusTopRight(radius);
        }
    }
    m_MeshNeedsRegeneration = true;
}

float Button3D::GetCornerRadiusBottomLeft() const {
    return m_CornerRadius.Get(2);
}

void Button3D::SetCornerRadiusBottomLeft(float radius) {
    m_CornerRadius.Set(2, radius);
    if (m_BaseStyleBox) {
        auto flatStyle = std::dynamic_pointer_cast<StyleBoxFlat>(m_BaseStyleBox);
        if (flatStyle) {
            flatStyle->SetCornerRadiusBottomLeft(radius);
        }
    }
    m_MeshNeedsRegeneration = true;
}

float Button3D::GetCornerRadiusBottomRight() const {
    return m_CornerRadius.Get(3);
}

void Button3D::SetCornerRadiusBottomRight(float radius) {
    m_CornerRadius.Set(3, radius);
    if (m_BaseStyleBox) {
        auto flatStyle = std::dynamic_pointer_cast<StyleBoxFlat>(m_BaseStyleBox);
        if (flatStyle) {
            flatStyle->SetCornerRadiusBottomRight(radius);
        }
    }
    m_MeshNeedsRegeneration = true;
}

std::string Button3D::GetText() const {
    std::string locKey = GetPropertyValue<std::string>("localizationKey");
    if (!locKey.empty()) {
        return localization::LocalizationManager::GetInstance().Translate(
            locKey, GetPropertyValue<std::string>("localizationTable"));
    }
    return GetPropertyValue<std::string>("text");
}

void Button3D::SetText(const std::string& text) {
    SetPropertyValue("text", text);
    m_TextMeshNeedsRegeneration = true;
}

std::string Button3D::GetFontPath() const {
    return GetPropertyValue<std::string>("fontPath");
}

void Button3D::SetFontPath(const std::string& path) {
    SetPropertyValue("fontPath", path);
    m_CurrentFontPath = path;
    m_TextMeshNeedsRegeneration = true;
}

float Button3D::GetFontSize() const {
    return GetPropertyValue<float>("fontSize");
}

void Button3D::SetFontSize(float size) {
    SetPropertyValue("fontSize", size);
    m_CurrentFontSize = size;
    m_TextMeshNeedsRegeneration = true;
}

Color Button3D::GetFontColor() const {
    return GetPropertyValue<Color>("fontColor");
}

void Button3D::SetFontColor(const Color& color) {
    SetPropertyValue("fontColor", color);
    m_TextMeshNeedsRegeneration = true;
}

bool Button3D::GetWordWrap() const {
    return GetPropertyValue<bool>("wordWrap");
}

void Button3D::SetWordWrap(bool wrap) {
    SetPropertyValue("wordWrap", wrap);
    m_TextMeshNeedsRegeneration = true;
}

const Color& Button3D::GetStateModulation(Button3DState state) const {
    return m_StateStyles[static_cast<int>(state)].modulationColor;
}

void Button3D::SetStateModulation(Button3DState state, const Color& color) {
    m_StateStyles[static_cast<int>(state)].modulationColor = color;

    switch (state) {
        case Button3DState::Normal:
            SetPropertyValue("normalModulation", color);
            break;
        case Button3DState::Hover:
            SetPropertyValue("hoverModulation", color);
            break;
        case Button3DState::Pressed:
            SetPropertyValue("pressedModulation", color);
            break;
        case Button3DState::Disabled:
            SetPropertyValue("disabledModulation", color);
            break;
        default:
            break;
    }
}

std::shared_ptr<StyleBox> Button3D::GetStateStyleBox(Button3DState state) const {
    return m_StateStyles[static_cast<int>(state)].styleBox;
}

void Button3D::SetStateStyleBox(Button3DState state, std::shared_ptr<StyleBox> styleBox) {
    m_StateStyles[static_cast<int>(state)].styleBox = styleBox;
    m_MeshNeedsRegeneration = true;
}

const Button3DStateSound& Button3D::GetStateSound(Button3DState state) const {
    return m_StateSounds[static_cast<int>(state)];
}

void Button3D::SetStateSound(Button3DState state, const Button3DStateSound& sound) {
    m_StateSounds[static_cast<int>(state)] = sound;
}

void Button3D::SetStateSoundPath(Button3DState state, const std::string& path) {
    m_StateSounds[static_cast<int>(state)].audioPath = path;

    switch (state) {
        case Button3DState::Normal:
            SetPropertyValue("normalSoundPath", path);
            break;
        case Button3DState::Hover:
            SetPropertyValue("hoverSoundPath", path);
            break;
        case Button3DState::Pressed:
            SetPropertyValue("pressedSoundPath", path);
            break;
        case Button3DState::Disabled:
            SetPropertyValue("disabledSoundPath", path);
            break;
        default:
            break;
    }
}

const Button3DStateTween& Button3D::GetStateTween(Button3DState state) const {
    return m_StateTweens[static_cast<int>(state)];
}

void Button3D::SetStateTween(Button3DState state, const Button3DStateTween& tween) {
    m_StateTweens[static_cast<int>(state)] = tween;
}

void Button3D::SetStateCallback(Button3DState state, StateCallback callback) {
    m_StateCallbacks[state] = callback;
}

void Button3D::ClearStateCallback(Button3DState state) {
    m_StateCallbacks.erase(state);
}

void Button3D::PreviewTween(Button3DState state) {

    TransitionToState(state);
}

void Button3D::SyncFromProperties() {

    m_StyleMode = static_cast<Button3DStyleMode>(GetPropertyValue<int>("styleMode"));
    m_ScaleMode = static_cast<Button3DScaleMode>(GetPropertyValue<int>("scaleMode"));
    m_TextPadding = GetPropertyValue<Vec2>("textPadding");

    m_ButtonEnabled = GetPropertyValue<bool>("buttonEnabled");

    m_StateStyles[static_cast<int>(Button3DState::Normal)].modulationColor = GetPropertyValue<Color>("normalModulation");
    m_StateStyles[static_cast<int>(Button3DState::Hover)].modulationColor = GetPropertyValue<Color>("hoverModulation");
    m_StateStyles[static_cast<int>(Button3DState::Pressed)].modulationColor = GetPropertyValue<Color>("pressedModulation");
    m_StateStyles[static_cast<int>(Button3DState::Disabled)].modulationColor = GetPropertyValue<Color>("disabledModulation");

    m_StateSounds[static_cast<int>(Button3DState::Normal)].audioPath = GetPropertyValue<std::string>("normalSoundPath");
    m_StateSounds[static_cast<int>(Button3DState::Hover)].audioPath = GetPropertyValue<std::string>("hoverSoundPath");
    m_StateSounds[static_cast<int>(Button3DState::Pressed)].audioPath = GetPropertyValue<std::string>("pressedSoundPath");
    m_StateSounds[static_cast<int>(Button3DState::Disabled)].audioPath = GetPropertyValue<std::string>("disabledSoundPath");

    auto syncTween = [this](Button3DState state, const std::string& prefix) {
        Button3DStateTween& tween = m_StateTweens[static_cast<int>(state)];
        tween.enabled = GetPropertyValue<bool>(prefix + "TweenEnabled");
        tween.scaleOffset = GetPropertyValue<Vec2>(prefix + "TweenScale");
        tween.rotationOffset = GetPropertyValue<float>(prefix + "TweenRotation");
        tween.positionOffset = GetPropertyValue<Vec2>(prefix + "TweenPosition");
        tween.duration = GetPropertyValue<float>(prefix + "TweenDuration");
    };

    syncTween(Button3DState::Normal, "normal");
    syncTween(Button3DState::Hover, "hover");
    syncTween(Button3DState::Pressed, "pressed");
    syncTween(Button3DState::Disabled, "disabled");

    m_CurrentFontPath = GetPropertyValue<std::string>("fontPath");
    m_CurrentFontSize = GetPropertyValue<float>("fontSize");

    if (m_BaseStyleBox) {
        auto flatStyle = std::dynamic_pointer_cast<StyleBoxFlat>(m_BaseStyleBox);
        if (flatStyle) {
            flatStyle->SetBackgroundColor(GetPropertyValue<Color>("backgroundColor"));
            flatStyle->SetBorderColorAll(GetPropertyValue<Color>("borderColor"));

            Vec4 borderWidth = GetPropertyValue<Vec4>("borderWidth");
            flatStyle->SetBorderWidthLeft(borderWidth.x);
            flatStyle->SetBorderWidthRight(borderWidth.y);
            flatStyle->SetBorderWidthTop(borderWidth.z);
            flatStyle->SetBorderWidthBottom(borderWidth.w);

            Vec4 cornerRadius = GetPropertyValue<Vec4>("cornerRadius");
            flatStyle->SetCornerRadiusTopLeft(cornerRadius.x);
            flatStyle->SetCornerRadiusTopRight(cornerRadius.y);
            flatStyle->SetCornerRadiusBottomLeft(cornerRadius.z);
            flatStyle->SetCornerRadiusBottomRight(cornerRadius.w);
        }
    }
}

void Button3D::UpdateMouseInteraction(float, RenderContext& ctx) {
    if (!m_ButtonEnabled) {
        return;
    }

    input::InputManager& inputMgr = input::InputManager::Get();

    RenderCamera* camera = ctx.getCamera();
    if (!camera || camera->getType() != CameraType::Camera3D) {
        return;
    }

    RenderView* view = ctx.getView();
    if (!view) {
        return;
    }

    float aspectRatio = view->getAspectRatio();
    Mat4 viewMatrix = camera->getViewMatrix();
    Mat4 projMatrix = camera->getProjectionMatrix(aspectRatio);

    glm::vec2 rawMousePos = inputMgr.GetMousePosition();
    glm::ivec2 windowSize = inputMgr.GetWindowSize();

    float ndcX = (2.0f * rawMousePos.x) / windowSize.x - 1.0f;
    float ndcY = 1.0f - (2.0f * rawMousePos.y) / windowSize.y;

    Mat4 invViewProj = (projMatrix * viewMatrix).Inverse();

    Vec4 rayClipNear = Vec4(ndcX, ndcY, -1.0f, 1.0f);
    Vec4 rayWorldNear = invViewProj * rayClipNear;
    Vec3 nearPoint = Vec3(rayWorldNear.x / rayWorldNear.w,
                         rayWorldNear.y / rayWorldNear.w,
                         rayWorldNear.z / rayWorldNear.w);

    Vec4 rayClipFar = Vec4(ndcX, ndcY, 1.0f, 1.0f);
    Vec4 rayWorldFar = invViewProj * rayClipFar;
    Vec3 farPoint = Vec3(rayWorldFar.x / rayWorldFar.w,
                        rayWorldFar.y / rayWorldFar.w,
                        rayWorldFar.z / rayWorldFar.w);

    Vec3 rayDir = (farPoint - nearPoint).Normalized();
    Ray ray(nearPoint, rayDir);

    float distance = 0.0f;
    bool isIntersecting = IsRayIntersecting(ray, distance);

    bool leftPressed = inputMgr.IsMouseButtonPressed(input::MouseButton::Left);

    Button3DState newState = m_CurrentState;

    if (isIntersecting) {
        if (leftPressed) {
            newState = Button3DState::Pressed;
        } else {
            newState = Button3DState::Hover;
        }
    } else {
        if (leftPressed && m_CurrentState == Button3DState::Pressed) {

            newState = Button3DState::Pressed;
        } else {
            newState = Button3DState::Normal;
        }
    }

    if (newState != m_CurrentState) {
        TransitionToState(newState);
    }

    m_WasIntersecting = isIntersecting;
}

bool Button3D::IsRayIntersecting(const Ray& ray, float& outDistance) const {

    return IntersectRay(ray, outDistance);
}

void Button3D::TransitionToState(Button3DState newState) {
    if (newState == m_CurrentState) {
        return;
    }

    m_PreviousState = m_CurrentState;
    m_CurrentState = newState;

    PlayStateSound(newState);

    const Button3DStateTween& tween = m_StateTweens[static_cast<int>(newState)];
    if (tween.enabled) {
        ApplyStateTween(newState, true);
    }

    InvokeStateCallback(newState);

    Emit("state_changed", { static_cast<int>(newState) });
    if (newState == Button3DState::Pressed) {
        Emit("pressed");
    } else if (m_PreviousState == Button3DState::Pressed) {
        Emit("released");
    }
    if (newState == Button3DState::Hover) {
        Emit("hovered");
    }
}

void Button3D::DefineSignals() {
    RegisterSignal({"pressed", {}, "Emitted when the 3D button is pressed down."});
    RegisterSignal({"released", {}, "Emitted when a press is released."});
    RegisterSignal({"hovered", {}, "Emitted when the pointer enters the button."});
    RegisterSignal({"state_changed",
                    {{"state", core::PropertyValueType::Int}},
                    "Emitted on any 3D button state change."});
}

void Button3D::PlayStateSound(Button3DState state) {
    const Button3DStateSound& sound = m_StateSounds[static_cast<int>(state)];

    if (sound.audioPath.empty()) {
        return;
    }

    if (!sound.audioAsset.IsValid()) {
        return;
    }

    audio::AudioManager& audioMgr = audio::AudioManager::GetInstance();

    if (sound.audioAsset->IsLoaded()) {
        audioMgr.Play(sound.audioAsset, sound.busName, audio::PlaybackMode::OneShot, sound.volume);
    }
}

void Button3D::ApplyStateTween(Button3DState state, bool) {
    const Button3DStateTween& tween = m_StateTweens[static_cast<int>(state)];

    if (!tween.enabled) {
        return;
    }

    m_TweenStartScaleOffset = m_CurrentScaleOffset;
    m_TweenStartRotationOffset = m_CurrentRotationOffset;
    m_TweenStartPositionOffset = m_CurrentPositionOffset;
    m_TweenProgress = 0.0f;
    m_IsTweening = true;
}

void Button3D::InvokeStateCallback(Button3DState state) {
    auto it = m_StateCallbacks.find(state);
    if (it != m_StateCallbacks.end() && it->second) {
        it->second();
    }
}

Vec2 Button3D::CalculateButtonSize() const {
    Vec2 size(GetWidth(), GetHeight());

    if (m_ScaleMode == Button3DScaleMode::Fixed) {
        return size;
    }

    Vec2 textSize = CalculateTextSize();

    if (m_ScaleMode == Button3DScaleMode::FitToText) {
        size.x = textSize.x + m_TextPadding.x * 2.0f;
        size.y = textSize.y + m_TextPadding.y * 2.0f;
    } else if (m_ScaleMode == Button3DScaleMode::FitToTextWidth) {
        size.x = textSize.x + m_TextPadding.x * 2.0f;
    }

    return size;
}

Vec2 Button3D::CalculateTextSize() const {

    if (!m_FontAsset.IsValid() || !m_FontAsset->IsLoaded()) {

        std::string text = GetText();
        float fontSize = GetFontSize();
        float width = text.length() * fontSize * 0.6f * 0.01f;
        float height = fontSize * 0.01f;
        return Vec2(width, height);
    }

    std::string text = GetText();
    float fontSize = GetFontSize();

    Vec2 pixelSize = m_FontAsset->MeasureText(text, fontSize);

    return pixelSize * 0.01f;
}

bool Button3D::LoadFont(const std::string& filepath) {
    if (filepath.empty()) {
        return false;
    }

    m_FontAsset = asset::AssetRef<asset::FontAsset>(new asset::FontAsset());

    float fontSize = GetFontSize();

    uint32_t minAtlasSize = static_cast<uint32_t>(fontSize * 20.0f);
    uint32_t atlasSize = 1024;
    while (atlasSize < minAtlasSize && atlasSize < 4096) {
        atlasSize *= 2;
    }

    bool loaded = m_FontAsset->LoadFromFile(filepath, fontSize, atlasSize, atlasSize);

    if (!loaded) {
        m_FontAsset.Reset();
        return false;
    }

    m_MeshNeedsRegeneration = true;

    SetFontPath(filepath);

    return true;
}

std::shared_ptr<StyleBoxFlat> Button3D::GetEffectiveStyle() const {
    if (m_StyleMode == Button3DStyleMode::Manual) {

        auto stateStyle = m_StateStyles[static_cast<int>(m_CurrentState)].styleBox;
        if (stateStyle) {
            return std::dynamic_pointer_cast<StyleBoxFlat>(stateStyle);
        }
    }

    return std::dynamic_pointer_cast<StyleBoxFlat>(m_BaseStyleBox);
}

Color Button3D::GetEffectiveColor(const Color& baseColor) const {
    if (m_StyleMode == Button3DStyleMode::Automatic) {

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

Mat4 Button3D::CalculateBillboardTransform(const Mat4& viewMatrix) const {
    Button3DBillboardMode billboardMode = GetBillboardMode();

    if (billboardMode == Button3DBillboardMode::Disabled) {
        return Mat4::Identity();
    }

    Node* owner = GetOwner();
    if (!owner) {
        return Mat4::Identity();
    }

    Node3D* node3D = dynamic_cast<Node3D*>(owner);
    if (!node3D) {
        return Mat4::Identity();
    }

    Vec3 position = node3D->GetGlobalPosition();
    Vec3 scale = node3D->GetGlobalScale();

    glm::mat4 glmView = viewMatrix.ToGLM();
    Vec3 right(glmView[0][0], glmView[1][0], glmView[2][0]);
    Vec3 up(glmView[0][1], glmView[1][1], glmView[2][1]);
    Vec3 forward = Cross(right, up).Normalized();

    if (billboardMode == Button3DBillboardMode::Enabled) {

        right = right * scale.x;
        up = up * scale.y;
        forward = forward * scale.z;

        glm::mat4 billboard(1.0f);
        billboard[0] = glm::vec4(right.x, right.y, right.z, 0.0f);
        billboard[1] = glm::vec4(up.x, up.y, up.z, 0.0f);
        billboard[2] = glm::vec4(forward.x, forward.y, forward.z, 0.0f);
        billboard[3] = glm::vec4(position.x, position.y, position.z, 1.0f);

        return Mat4(billboard);
    } else if (billboardMode == Button3DBillboardMode::YAxisOnly) {

        glm::mat4 invView = glm::inverse(glmView);
        Vec3 cameraPos(invView[3][0], invView[3][1], invView[3][2]);

        Vec3 toCamera = (cameraPos - position);
        toCamera.y = 0.0f;
        toCamera = toCamera.Normalized();

        Vec3 upVec(0, 1, 0);
        Vec3 rightVec = Cross(upVec, toCamera).Normalized();
        Vec3 forwardVec = toCamera;

        rightVec = rightVec * scale.x;
        upVec = upVec * scale.y;
        forwardVec = forwardVec * scale.z;

        glm::mat4 billboard(1.0f);
        billboard[0] = glm::vec4(rightVec.x, rightVec.y, rightVec.z, 0.0f);
        billboard[1] = glm::vec4(upVec.x, upVec.y, upVec.z, 0.0f);
        billboard[2] = glm::vec4(forwardVec.x, forwardVec.y, forwardVec.z, 0.0f);
        billboard[3] = glm::vec4(position.x, position.y, position.z, 1.0f);

        return Mat4(billboard);
    }

    return Mat4::Identity();
}

void Button3D::buildDrawCommands(RenderContext& ctx) {
    if (!IsEnabled()) {
        return;
    }

    UpdateMouseInteraction(0.0f, ctx);

    Vec4 cornerRadiusVec = GetPropertyValue<Vec4>("cornerRadius");
    bool cornerRadiusLinked = GetPropertyValue<bool>("cornerRadiusLinked");
    m_CornerRadius.FromVec4(cornerRadiusVec);
    m_CornerRadius.SetLinked(cornerRadiusLinked);

    Vec4 borderWidthVec = GetPropertyValue<Vec4>("borderWidth");
    bool borderWidthLinked = GetPropertyValue<bool>("borderWidthLinked");
    m_BorderWidth.FromVec4(borderWidthVec);
    m_BorderWidth.SetLinked(borderWidthLinked);

    Node* owner = GetOwner();
    if (!owner) {
        return;
    }

    Node3D* node3D = dynamic_cast<Node3D*>(owner);
    if (!node3D) {
        return;
    }

    Vec2 buttonSize = CalculateButtonSize();

    Button3DBillboardMode billboardMode = GetBillboardMode();

    Mat4 transform;

    if (billboardMode != Button3DBillboardMode::Disabled) {

        transform = CalculateBillboardTransform(ctx.getViewMatrix());
    } else {

        transform = node3D->GetGlobalTransformMatrix();
    }

    Vec2 tweenScale = Vec2(1.0f, 1.0f) + m_CurrentScaleOffset;
    Vec3 tweenPosition = Vec3(m_CurrentPositionOffset.x, m_CurrentPositionOffset.y, 0.0f);
    float tweenRotation = m_CurrentRotationOffset;

    Mat4 tweenTransform = Mat4::Translate(tweenPosition) *
                          Mat4::Rotate(tweenRotation, Vec3(0.0f, 0.0f, 1.0f)) *
                          Mat4::Scale(Vec3(tweenScale.x, tweenScale.y, 1.0f));

    transform = transform * tweenTransform;

    bool doubleSided = GetDoubleSided();

    if (doubleSided) {

        RenderBorder(ctx, transform, buttonSize, true);
        RenderFill(ctx, transform, buttonSize, true);

        RenderFill(ctx, transform, buttonSize, false);
        RenderBorder(ctx, transform, buttonSize, false);
    } else {

        RenderBorder(ctx, transform, buttonSize, false);
        RenderFill(ctx, transform, buttonSize, false);
    }

    RenderText(ctx, transform, buttonSize);
}

void Button3D::RenderFill(RenderContext& ctx, const Mat4& transform, const Vec2& size, bool isBackFace) {

    auto style = GetEffectiveStyle();
    if (!style) {
        return;
    }

    Color fillColor = GetEffectiveColor(style->GetBackgroundColor());
    fillColor.a *= GetOpacity();

    if (fillColor.a <= 0.0f) return;

    MeshHandle quadMesh = ctx.getOrCreateQuadMesh();
    if (!quadMesh.isValid()) {
        return;
    }

    float zOffset = isBackFace ? -0.001f : 0.001f;

    Mat4 scale = Mat4::Scale(Vec3(size.x, size.y, 1.0f));
    Mat4 finalTransform = transform * scale * Mat4::Translate(Vec3(0, 0, zOffset));

    Vec4 cornerRadius = m_CornerRadius.AsVec4();
    bool hasRoundedCorners = (cornerRadius.x > 0.0f || cornerRadius.y > 0.0f ||
                              cornerRadius.z > 0.0f || cornerRadius.w > 0.0f);

    const float pixelScale = 20.0f;
    Vec2 scaledSize = size * pixelScale;
    Vec4 scaledCornerRadius = cornerRadius * pixelScale;

    MaterialPropertyBlock overrides;
    overrides.setColor("u_Color", fillColor);
    overrides.setVec2("u_Size", scaledSize);
    overrides.setVec4("u_CornerRadius", scaledCornerRadius);
    overrides.setBool("u_UseTexture", false);

    MaterialHandle material;
    if (hasRoundedCorners) {
        material = ctx.getRoundedRect3DMaterial();
    } else {
        material = GetDoubleSided()
            ? ctx.getDefaultColoredDoubleSidedMaterial()
            : ctx.getDefaultColoredMaterial();
        overrides.setColor("u_TintColor", fillColor);
    }

    ctx.drawMesh(quadMesh, material, finalTransform, overrides, 0, GetCastShadow(), GetReceiveShadow());
}

void Button3D::RenderBorder(RenderContext& ctx, const Mat4& transform, const Vec2& size, bool isBackFace) {
    if (!GetBorderEnabled()) {
        return;
    }

    auto style = GetEffectiveStyle();
    if (!style) {
        return;
    }

    Color borderColor = GetEffectiveColor(style->GetBorderColorLeft());
    borderColor.a *= GetOpacity();

    if (borderColor.a <= 0.0f) return;

    Vec4 borderWidth = m_BorderWidth.AsVec4();
    if (borderWidth.x <= 0.0f && borderWidth.y <= 0.0f &&
        borderWidth.z <= 0.0f && borderWidth.w <= 0.0f) {
        return;
    }

    MeshHandle quadMesh = ctx.getOrCreateQuadMesh();
    if (!quadMesh.isValid()) {
        return;
    }

    float zOffset = isBackFace ? -0.002f : 0.0f;

    Mat4 scale = Mat4::Scale(Vec3(size.x, size.y, 1.0f));
    Mat4 finalTransform = transform * scale * Mat4::Translate(Vec3(0, 0, zOffset));

    Vec4 cornerRadius = m_CornerRadius.AsVec4();

    const float pixelScale = 20.0f;
    Vec2 scaledSize = size * pixelScale;
    Vec4 scaledCornerRadius = cornerRadius * pixelScale;
    Vec4 scaledBorderWidth = borderWidth * pixelScale;

    MaterialPropertyBlock overrides;
    overrides.setColor("u_BorderColor", borderColor);
    overrides.setVec2("u_Size", scaledSize);
    overrides.setVec4("u_CornerRadius", scaledCornerRadius);
    overrides.setVec4("u_BorderWidth", scaledBorderWidth);
    overrides.setBool("u_DrawBorder", true);

    MaterialHandle material = ctx.getRoundedRect3DMaterial();

    ctx.drawMesh(quadMesh, material, finalTransform, overrides, 0, GetCastShadow(), GetReceiveShadow());
}

void Button3D::RenderText(RenderContext& ctx, const Mat4& transform, const Vec2&) {
    std::string text = GetText();
    if (text.empty()) {
        return;
    }

    // A resolution change re-bakes the font atlas at a new oversampling density,
    // which repacks glyph UVs; regenerate the cached text mesh so it matches.
    if (ConsumeFontOversampleChanged()) {
        m_MeshNeedsRegeneration = true;
    }

    std::string fontPath = GetFontPath();
    float currentFontSize = GetFontSize();
    bool fontPathChanged = (fontPath != m_CurrentFontPath);
    bool fontSizeChanged = (currentFontSize != m_CurrentFontSize);

    if (fontPathChanged || fontSizeChanged) {
        m_CurrentFontPath = fontPath;
        m_CurrentFontSize = currentFontSize;

        if (m_FontHandle.isValid()) {
            m_FontHandle = FontHandle();
        }

        if (m_TextMesh.isValid() && ctx.getDevice()) {
            ctx.getDevice()->destroyMesh(m_TextMesh);
            m_TextMesh = MeshHandle();
        }

        if (!fontPath.empty()) {
            LoadFont(fontPath);
        } else {
            m_FontAsset.Reset();
        }
        m_MeshNeedsRegeneration = true;
    }

    if (m_FontAsset.IsValid() && m_FontAsset->IsLoaded() && !m_FontHandle.isValid()) {
        FontDesc fontDesc;
        // Use GetPhysicalPath() to resolve res:// path to filesystem path for file loading
        fontDesc.fontPath = m_FontAsset->GetPhysicalPath();
        fontDesc.fontSize = GetFontSize();
        fontDesc.atlasWidth = m_FontAsset->GetAtlasWidth();
        fontDesc.atlasHeight = m_FontAsset->GetAtlasHeight();

        IGfxDevice* device = ctx.getDevice();
        if (device) {
            m_FontHandle = device->createFontAtlas(fontDesc);
        }
        m_MeshNeedsRegeneration = true;
    }

    if (!m_FontHandle.isValid()) {
        return;
    }

    if (text != m_CachedText || GetFontSize() != m_CachedFontSize) {
        m_MeshNeedsRegeneration = true;
    }

    if (m_MeshNeedsRegeneration || !m_TextMesh.isValid()) {
        RegenerateTextMesh(ctx);
    }

    if (!m_TextMesh.isValid()) {
        return;
    }

    const FontAtlas* fontAtlas = ctx.getDevice()->getFontAtlas(m_FontHandle);
    if (!fontAtlas) {
        return;
    }

    Vec2 textSize = CalculateTextSize();

    Vec2 textOffset = Vec2(0.0f, 0.0f);

    float zOffset = 0.002f;

    Mat4 textScale = Mat4::Scale(Vec3(textSize.x, textSize.y, 1.0f));
    Mat4 textTranslate = Mat4::Translate(Vec3(textOffset.x, textOffset.y, zOffset));
    Mat4 finalTransform = transform * textTranslate * textScale;

    Color fontColor = GetEffectiveColor(GetFontColor());
    fontColor.a *= GetOpacity();

    MaterialPropertyBlock overrides;
    overrides.setColor("u_TintColor", fontColor);
    overrides.setTexture("u_Texture", fontAtlas->texture);
    overrides.setBool("u_UseTexture", true);

    MaterialHandle material = GetDoubleSided()
        ? ctx.getDefaultTexturedDoubleSidedMaterial()
        : ctx.getDefaultTexturedMaterial();

    ctx.drawMesh(m_TextMesh, material, finalTransform, overrides, 0, GetCastShadow(), GetReceiveShadow());
}

void Button3D::RegenerateTextMesh(RenderContext& ctx) {
    if (!m_FontAsset.IsValid() || !m_FontAsset->IsLoaded()) {
        m_MeshNeedsRegeneration = false;
        return;
    }

    IGfxDevice* device = ctx.getDevice();
    if (!device) {
        return;
    }

    if (m_TextMesh.isValid()) {
        device->destroyMesh(m_TextMesh);
        m_TextMesh = MeshHandle();
    }

    std::string text = GetText();
    float fontSize = GetFontSize();

    MeshData meshData;

    float x = 0.0f;
    float y = 0.0f;
    float scale = 1.0f / fontSize;

    float totalWidth = 0.0f;
    for (char c : text) {
        const asset::Glyph* glyph = m_FontAsset->GetGlyph(static_cast<uint32_t>(c));
        if (!glyph) continue;
        totalWidth += glyph->advance * scale;
    }

    x = -totalWidth * 0.5f;

    for (char c : text) {
        const asset::Glyph* glyph = m_FontAsset->GetGlyph(static_cast<uint32_t>(c));
        if (!glyph) {

            if (c == ' ') {
                x += fontSize * 0.25f * scale;
            }
            continue;
        }

        float xpos = x + glyph->bearingX * scale;
        float ypos = y - glyph->bearingY * scale;
        float w = glyph->width * scale;
        float h = glyph->height * scale;

        uint32_t baseIndex = static_cast<uint32_t>(meshData.vertices.size());

        float uvMinX = glyph->atlasX;
        float uvMinY = glyph->atlasY;
        float uvMaxX = glyph->atlasX + glyph->atlasWidth;
        float uvMaxY = glyph->atlasY + glyph->atlasHeight;

        Vertex v0, v1, v2, v3;
        v0.position = Vec3(xpos, ypos - h, 0.0f);
        v0.normal = Vec3(0, 0, 1);
        v0.texCoord = Vec2(uvMinX, uvMaxY);
        v0.color = Vec4(1.0f, 1.0f, 1.0f, 1.0f);

        v1.position = Vec3(xpos + w, ypos - h, 0.0f);
        v1.normal = Vec3(0, 0, 1);
        v1.texCoord = Vec2(uvMaxX, uvMaxY);
        v1.color = Vec4(1.0f, 1.0f, 1.0f, 1.0f);

        v2.position = Vec3(xpos + w, ypos, 0.0f);
        v2.normal = Vec3(0, 0, 1);
        v2.texCoord = Vec2(uvMaxX, uvMinY);
        v2.color = Vec4(1.0f, 1.0f, 1.0f, 1.0f);

        v3.position = Vec3(xpos, ypos, 0.0f);
        v3.normal = Vec3(0, 0, 1);
        v3.texCoord = Vec2(uvMinX, uvMinY);
        v3.color = Vec4(1.0f, 1.0f, 1.0f, 1.0f);

        meshData.vertices.push_back(v0);
        meshData.vertices.push_back(v1);
        meshData.vertices.push_back(v2);
        meshData.vertices.push_back(v3);

        meshData.indices.push_back(baseIndex + 0);
        meshData.indices.push_back(baseIndex + 1);
        meshData.indices.push_back(baseIndex + 2);
        meshData.indices.push_back(baseIndex + 0);
        meshData.indices.push_back(baseIndex + 2);
        meshData.indices.push_back(baseIndex + 3);

        x += glyph->advance * scale;
    }

    if (!meshData.vertices.empty() && !meshData.indices.empty()) {
        meshData.calculateBounds();
        m_TextMesh = device->createMesh(meshData);
    }

    m_CachedText = text;
    m_CachedFontSize = fontSize;
    m_MeshNeedsRegeneration = false;
}

AABB Button3D::getWorldBounds() const {
    Node* owner = GetOwner();
    if (!owner) {
        return AABB();
    }

    Node3D* node3D = dynamic_cast<Node3D*>(owner);
    if (!node3D) {
        return AABB();
    }

    Vec3 position = node3D->GetGlobalPosition();
    Vec2 size = CalculateButtonSize();

    Vec3 halfSize = Vec3(size.x * 0.5f, size.y * 0.5f, 0.1f);

    return AABB(position - halfSize, position + halfSize);
}

math::OBB Button3D::getOrientedBounds() const {
    Node* owner = GetOwner();
    if (!owner) {
        return math::OBB();
    }

    Node3D* node3D = dynamic_cast<Node3D*>(owner);
    if (!node3D) {
        return math::OBB();
    }

    Vec3 position = node3D->GetGlobalPosition();
    Quat rotation = node3D->GetGlobalRotation();
    Vec2 size = CalculateButtonSize();
    Vec3 globalScale = node3D->GetGlobalScale();

    Vec2 tweenScale = Vec2(1.0f, 1.0f) + m_CurrentScaleOffset;
    Vec3 tweenPosition = Vec3(m_CurrentPositionOffset.x, m_CurrentPositionOffset.y, 0.0f);

    Button3DBillboardMode billboardMode = GetBillboardMode();
    if (billboardMode != Button3DBillboardMode::Disabled) {
        return math::OBB::FromAABB(getWorldBounds());
    }

    size.x *= globalScale.x;
    size.y *= globalScale.y;

    Vec3 center = position + tweenPosition;
    Vec3 extents = Vec3(size.x * 0.5f * tweenScale.x, size.y * 0.5f * tweenScale.y, 0.1f);

    return math::OBB(center, extents, rotation);
}

bool Button3D::IntersectRay(const Ray& ray, float& outDistance) const {
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

RenderLayer Button3D::getRenderLayer() const {

    float opacity = GetOpacity();
    if (opacity < 1.0f) {
        return RenderLayer::Transparent;
    }
    return RenderLayer::Opaque;
}

bool Button3D::OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) {
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

}
}

