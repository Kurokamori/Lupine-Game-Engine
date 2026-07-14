#include "lupine/components/TextureButton.hpp"
#include "lupine/ui/Theme.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/Mesh.hpp"
#include "lupine/rendering/Font.hpp"
#include "lupine/rendering/DrawCommand.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/rendering/TextureUpload.hpp"
#include "lupine/rendering/gfx/GfxDescriptors.hpp"
#include "lupine/input/InputManager.hpp"
#include "lupine/localization/LocalizationManager.hpp"
#include "lupine/rendering/ViewportUtils.hpp"
#include "lupine/asset/AssetDatabase.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/math/OBB.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/audio/AudioManager.hpp"

namespace lupine {
namespace components {

using namespace core;
using namespace math;

using core::Node2D;
using core::Node3D;

TextureButton::TextureButton()
    : UIControl("TextureButton")
    , m_MeshNeedsRegeneration(true)
{
    // Initialize default state modulations
    m_StateTextures[static_cast<int>(TextureButtonState::Normal)].modulationColor = Color::White();
    m_StateTextures[static_cast<int>(TextureButtonState::Hover)].modulationColor = Color(1.2f, 1.2f, 1.2f, 1.0f);
    m_StateTextures[static_cast<int>(TextureButtonState::Pressed)].modulationColor = Color(0.8f, 0.8f, 0.8f, 1.0f);
    m_StateTextures[static_cast<int>(TextureButtonState::Disabled)].modulationColor = Color(0.5f, 0.5f, 0.5f, 0.5f);
}

TextureButton::TextureButton(const std::string& name)
    : UIControl(name)
    , m_MeshNeedsRegeneration(true)
{
    // Initialize default state modulations
    m_StateTextures[static_cast<int>(TextureButtonState::Normal)].modulationColor = Color::White();
    m_StateTextures[static_cast<int>(TextureButtonState::Hover)].modulationColor = Color(1.2f, 1.2f, 1.2f, 1.0f);
    m_StateTextures[static_cast<int>(TextureButtonState::Pressed)].modulationColor = Color(0.8f, 0.8f, 0.8f, 1.0f);
    m_StateTextures[static_cast<int>(TextureButtonState::Disabled)].modulationColor = Color(0.5f, 0.5f, 0.5f, 0.5f);
}

TextureButton::~TextureButton() {
}

void TextureButton::DefineProperties() {
    DefineUIControlProperties(120.0f, 40.0f, "useUISpace", "Size");

    // Rendering
    DefineProperty(PROPERTY_INT_RANGE_GROUP(layer, 0, -100, 100, 1, "Rendering"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(sortingOrder, 0, -1000, 1000, 1, "Rendering"));

    // Button
    DefineProperty(PROPERTY_DEFAULT_GROUP(buttonEnabled, Bool, true, "Button"));
    DefineProperty(PROPERTY_ENUM_GROUP(styleMode, 0, "Button", Automatic, Manual));
    DefineProperty(PROPERTY_DEFAULT_GROUP(useStretch, Bool, true, "Button"));
    DefineProperty(PROPERTY_ENUM_GROUP(stretchMode, 0, "Button", Stretch, KeepCentered, NineSlice));

    // Nine-slice (used when stretchMode == NineSlice). Margins are in source texture
    // pixels (left, top, right, bottom); per-axis stretch/tile; optional center fill.
    DefineProperty(PROPERTY_DEFAULT_GROUP(nineSliceMargins, Vec4, Vec4(8.0f, 8.0f, 8.0f, 8.0f), "NineSlice"));
    DefineProperty(PROPERTY_ENUM_GROUP(nineSliceAxisH, 0, "NineSlice", Stretch, Tile));
    DefineProperty(PROPERTY_ENUM_GROUP(nineSliceAxisV, 0, "NineSlice", Stretch, Tile));
    DefineProperty(PROPERTY_DEFAULT_GROUP(nineSliceDrawCenter, Bool, true, "NineSlice"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(isToggle, Bool, false, "Button"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(isChecked, Bool, false, "Button"));
    DefineProperty(PROPERTY_ENUM_GROUP(clickMaskMode, 0, "Button", Rect, AlphaThreshold));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(alphaThreshold, 0.1f, 0.0f, 1.0f, 0.01f, "Button"));
    DefineProperty(PROPERTY_ENUM_GROUP(mouseButton, 0, "Button", Left, Right, Middle));

    // Texture (Automatic Mode)
    DefineProperty(PROPERTY_FILE_GROUP(texturePath, std::string(""), "*.png,*.jpg,*.jpeg,*.bmp,*.tga", "Texture"));

    // Normal State
    DefineProperty(PROPERTY_FILE_GROUP(normalTexturePath, std::string(""), "*.png,*.jpg,*.jpeg,*.bmp,*.tga", "State_Normal"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(normalModulation, Color, Color::White(), "State_Normal"));
    DefineProperty(PROPERTY_FILE_GROUP(normalSoundPath, std::string(""), "*.wav,*.ogg,*.mp3", "State_Normal"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(normalTweenEnabled, Bool, false, "State_Normal"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(normalTweenScale, Vec2, Vec2(0.0f, 0.0f), "State_Normal"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(normalTweenRotation, 0.0f, -360.0f, 360.0f, 1.0f, "State_Normal"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(normalTweenPosition, Vec2, Vec2(0.0f, 0.0f), "State_Normal"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(normalTweenDuration, 0.2f, 0.0f, 5.0f, 0.01f, "State_Normal"));

    // Hover State
    DefineProperty(PROPERTY_FILE_GROUP(hoverTexturePath, std::string(""), "*.png,*.jpg,*.jpeg,*.bmp,*.tga", "State_Hover"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(hoverModulation, Color, Color(1.2f, 1.2f, 1.2f, 1.0f), "State_Hover"));
    DefineProperty(PROPERTY_FILE_GROUP(hoverSoundPath, std::string(""), "*.wav,*.ogg,*.mp3", "State_Hover"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(hoverTweenEnabled, Bool, true, "State_Hover"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(hoverTweenScale, Vec2, Vec2(0.05f, 0.05f), "State_Hover"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(hoverTweenRotation, 0.0f, -360.0f, 360.0f, 1.0f, "State_Hover"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(hoverTweenPosition, Vec2, Vec2(0.0f, 0.0f), "State_Hover"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(hoverTweenDuration, 0.2f, 0.0f, 5.0f, 0.01f, "State_Hover"));

    // Pressed State
    DefineProperty(PROPERTY_FILE_GROUP(pressedTexturePath, std::string(""), "*.png,*.jpg,*.jpeg,*.bmp,*.tga", "State_Pressed"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(pressedModulation, Color, Color(0.8f, 0.8f, 0.8f, 1.0f), "State_Pressed"));
    DefineProperty(PROPERTY_FILE_GROUP(pressedSoundPath, std::string(""), "*.wav,*.ogg,*.mp3", "State_Pressed"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(pressedTweenEnabled, Bool, true, "State_Pressed"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(pressedTweenScale, Vec2, Vec2(-0.05f, -0.05f), "State_Pressed"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(pressedTweenRotation, 0.0f, -360.0f, 360.0f, 1.0f, "State_Pressed"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(pressedTweenPosition, Vec2, Vec2(0.0f, 2.0f), "State_Pressed"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(pressedTweenDuration, 0.1f, 0.0f, 5.0f, 0.01f, "State_Pressed"));

    // Disabled State
    DefineProperty(PROPERTY_FILE_GROUP(disabledTexturePath, std::string(""), "*.png,*.jpg,*.jpeg,*.bmp,*.tga", "State_Disabled"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(disabledModulation, Color, Color(0.5f, 0.5f, 0.5f, 0.5f), "State_Disabled"));
    DefineProperty(PROPERTY_FILE_GROUP(disabledSoundPath, std::string(""), "*.wav,*.ogg,*.mp3", "State_Disabled"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(disabledTweenEnabled, Bool, false, "State_Disabled"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(disabledTweenScale, Vec2, Vec2(0.0f, 0.0f), "State_Disabled"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(disabledTweenRotation, 0.0f, -360.0f, 360.0f, 1.0f, "State_Disabled"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(disabledTweenPosition, Vec2, Vec2(0.0f, 0.0f), "State_Disabled"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(disabledTweenDuration, 0.2f, 0.0f, 5.0f, 0.01f, "State_Disabled"));

    // Text
    DefineProperty(PROPERTY_DEFAULT_GROUP(text, String, std::string(""), "Text"));

    // localizationKey overrides "text" and resolves through the LocalizationManager.
    DefineProperty(PROPERTY_DEFAULT_GROUP(localizationKey, String, std::string(""), "Localization"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(localizationTable, String, std::string(""), "Localization"));
    DefineProperty(PROPERTY_FILE_GROUP(fontPath, std::string(""), "*.ttf,*.otf", "Text"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(fontSize, 16.0f, 1.0f, 256.0f, 1.0f, "Text"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(fontColor, Color, Color::White(), "Text"));
}

void TextureButton::OnAwake() {
    LoadThemedStateData();

    // Load sounds
    auto loadSound = [this](TextureButtonState state, const std::string& prefix) {
        std::string path = GetPropertyValue<std::string>(prefix + "SoundPath");
        if (!path.empty()) {
            SetStateSoundPath(state, path);
        }
    };

    loadSound(TextureButtonState::Normal, "normal");
    loadSound(TextureButtonState::Hover, "hover");
    loadSound(TextureButtonState::Pressed, "pressed");
    loadSound(TextureButtonState::Disabled, "disabled");

    // Load font
    std::string fontPath = GetFontPath();
    if (!fontPath.empty()) {
        m_CurrentFontPath = fontPath;
    }

    m_ButtonEnabled = GetPropertyValue<bool>("buttonEnabled");
    m_CurrentState = m_ButtonEnabled ? TextureButtonState::Normal : TextureButtonState::Disabled;
    m_StyleMode = static_cast<TextureButtonStyleMode>(GetPropertyValue<int>("styleMode"));
}

void TextureButton::OnReady() {
    m_MeshNeedsRegeneration = true;
}

void TextureButton::OnInput(float deltaTime) {
    if (!IsEnabled()) {
        return;
    }

    UpdateMouseInteraction(deltaTime);
}

void TextureButton::OnUpdate(float deltaTime) {
    UIControl::OnUpdate(deltaTime);

    if (!IsEnabled()) {
        return;
    }

    SyncFromProperties();

    // Update tween animation
    if (m_IsTweening && m_TweenProgress < 1.0f) {
        const TextureButtonStateTween& targetTween = m_StateTweens[static_cast<int>(m_CurrentState)];
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

bool TextureButton::OnGizmoScale(float scaleDelta, int axis, bool is3D) {
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

int TextureButton::GetLayer() const {
    return GetPropertyValue<int>("layer");
}

void TextureButton::SetLayer(int layer) {
    SetPropertyValue<int>("layer", layer);
}

int TextureButton::GetSortingOrder() const {
    return GetPropertyValue<int>("sortingOrder");
}

void TextureButton::SetSortingOrder(int order) {
    SetPropertyValue<int>("sortingOrder", order);
}

void TextureButton::SetEnabled(bool enabled) {
    m_ButtonEnabled = enabled;
    SetPropertyValue<bool>("buttonEnabled", enabled);

    if (!enabled && m_CurrentState != TextureButtonState::Disabled) {
        TransitionToState(TextureButtonState::Disabled);
    } else if (enabled && m_CurrentState == TextureButtonState::Disabled) {
        TransitionToState(TextureButtonState::Normal);
    }
}

void TextureButton::SetStyleMode(TextureButtonStyleMode mode) {
    m_StyleMode = mode;
    SetPropertyValue<int>("styleMode", static_cast<int>(mode));
    m_MeshNeedsRegeneration = true;
}

TextureButtonStretchMode TextureButton::GetStretchMode() const {
    return static_cast<TextureButtonStretchMode>(GetPropertyValue<int>("stretchMode"));
}

void TextureButton::SetStretchMode(TextureButtonStretchMode mode) {
    SetPropertyValue<int>("stretchMode", static_cast<int>(mode));
    m_MeshNeedsRegeneration = true;
}

bool TextureButton::GetUseStretch() const {
    return GetPropertyValue<bool>("useStretch");
}

void TextureButton::SetUseStretch(bool useStretch) {
    SetPropertyValue<bool>("useStretch", useStretch);
    m_MeshNeedsRegeneration = true;
}

Vec4 TextureButton::GetNineSliceMargins() const {
    return GetPropertyValue<Vec4>("nineSliceMargins");
}

void TextureButton::SetNineSliceMargins(const Vec4& margins) {
    SetPropertyValue<Vec4>("nineSliceMargins", margins);
}

UINineSliceAxisMode TextureButton::GetNineSliceAxisHorizontal() const {
    return UINineSliceAxisModeFromInt(GetPropertyValue<int>("nineSliceAxisH"));
}

void TextureButton::SetNineSliceAxisHorizontal(UINineSliceAxisMode mode) {
    SetPropertyValue<int>("nineSliceAxisH", static_cast<int>(mode));
}

UINineSliceAxisMode TextureButton::GetNineSliceAxisVertical() const {
    return UINineSliceAxisModeFromInt(GetPropertyValue<int>("nineSliceAxisV"));
}

void TextureButton::SetNineSliceAxisVertical(UINineSliceAxisMode mode) {
    SetPropertyValue<int>("nineSliceAxisV", static_cast<int>(mode));
}

bool TextureButton::GetNineSliceDrawCenter() const {
    return GetPropertyValue<bool>("nineSliceDrawCenter");
}

void TextureButton::SetNineSliceDrawCenter(bool drawCenter) {
    SetPropertyValue<bool>("nineSliceDrawCenter", drawCenter);
}

bool TextureButton::GetIsToggle() const {
    return GetPropertyValue<bool>("isToggle");
}

void TextureButton::SetIsToggle(bool isToggle) {
    SetPropertyValue<bool>("isToggle", isToggle);
}

bool TextureButton::GetIsChecked() const {
    return GetPropertyValue<bool>("isChecked");
}

void TextureButton::SetIsChecked(bool isChecked) {
    SetPropertyValue<bool>("isChecked", isChecked);
    if (m_OnToggledCallback) {
        m_OnToggledCallback(isChecked);
    }
    Emit("toggled", { isChecked });
}

void TextureButton::DefineSignals() {
    RegisterSignal({"pressed", {}, "Emitted when the texture button is pressed."});
    RegisterSignal({"toggled",
                    {{"checked", core::PropertyValueType::Bool}},
                    "Emitted when the checked state changes (toggle mode)."});
}

ClickMaskMode TextureButton::GetClickMaskMode() const {
    return static_cast<ClickMaskMode>(GetPropertyValue<int>("clickMaskMode"));
}

void TextureButton::SetClickMaskMode(ClickMaskMode mode) {
    SetPropertyValue<int>("clickMaskMode", static_cast<int>(mode));
}

float TextureButton::GetAlphaThreshold() const {
    return GetPropertyValue<float>("alphaThreshold");
}

void TextureButton::SetAlphaThreshold(float threshold) {
    SetPropertyValue<float>("alphaThreshold", threshold);
}

MouseButtonType TextureButton::GetMouseButton() const {
    return static_cast<MouseButtonType>(GetPropertyValue<int>("mouseButton"));
}

void TextureButton::SetMouseButton(MouseButtonType button) {
    SetPropertyValue<int>("mouseButton", static_cast<int>(button));
}

std::string TextureButton::GetTexturePath() const {
    return GetPropertyValue<std::string>("texturePath");
}

void TextureButton::SetTexturePath(const std::string& path) {
    // Convert to res:// path if possible
    std::string resPath = path;
    if (!path.empty() && !(path.size() >= 6 && path.substr(0, 6) == "res://")) {
        auto& assetDb = asset::AssetDatabase::GetInstance();
        if (assetDb.IsInitialized()) {
            std::string converted = assetDb.ToResourcePath(path);
            if (!converted.empty()) {
                resPath = converted;
            }
        }
    }
    SetThemedProperty<std::string>("texturePath", resPath);
    m_AutomaticTexturePath = resPath;
    m_MeshNeedsRegeneration = true;
}

std::string TextureButton::GetStateTexturePath(TextureButtonState state) const {
    std::string propName;
    switch (state) {
        case TextureButtonState::Normal: propName = "normalTexturePath"; break;
        case TextureButtonState::Hover: propName = "hoverTexturePath"; break;
        case TextureButtonState::Pressed: propName = "pressedTexturePath"; break;
        case TextureButtonState::Disabled: propName = "disabledTexturePath"; break;
        default: return "";
    }
    return GetPropertyValue<std::string>(propName);
}

void TextureButton::SetStateTexturePath(TextureButtonState state, const std::string& path) {
    int idx = static_cast<int>(state);
    m_StateTextures[idx].texturePath = path;

    std::string propName;
    switch (state) {
        case TextureButtonState::Normal: propName = "normalTexturePath"; break;
        case TextureButtonState::Hover: propName = "hoverTexturePath"; break;
        case TextureButtonState::Pressed: propName = "pressedTexturePath"; break;
        case TextureButtonState::Disabled: propName = "disabledTexturePath"; break;
        default: return;
    }
    SetThemedProperty<std::string>(propName, path);
    m_MeshNeedsRegeneration = true;
}

const Color& TextureButton::GetStateModulation(TextureButtonState state) const {
    return m_StateTextures[static_cast<int>(state)].modulationColor;
}

void TextureButton::SetStateModulation(TextureButtonState state, const Color& color) {
    m_StateTextures[static_cast<int>(state)].modulationColor = color;

    std::string propName;
    switch (state) {
        case TextureButtonState::Normal: propName = "normalModulation"; break;
        case TextureButtonState::Hover: propName = "hoverModulation"; break;
        case TextureButtonState::Pressed: propName = "pressedModulation"; break;
        case TextureButtonState::Disabled: propName = "disabledModulation"; break;
        default: return;
    }
    SetPropertyValue<Color>(propName, color);
    m_MeshNeedsRegeneration = true;
}

const TextureButtonStateSound& TextureButton::GetStateSound(TextureButtonState state) const {
    return m_StateSounds[static_cast<int>(state)];
}

void TextureButton::SetStateSound(TextureButtonState state, const TextureButtonStateSound& sound) {
    m_StateSounds[static_cast<int>(state)] = sound;
}

void TextureButton::SetStateSoundPath(TextureButtonState state, const std::string& path) {
    int idx = static_cast<int>(state);
    m_StateSounds[idx].audioPath = path;

    std::string propName;
    switch (state) {
        case TextureButtonState::Normal: propName = "normalSoundPath"; break;
        case TextureButtonState::Hover: propName = "hoverSoundPath"; break;
        case TextureButtonState::Pressed: propName = "pressedSoundPath"; break;
        case TextureButtonState::Disabled: propName = "disabledSoundPath"; break;
        default: return;
    }
    SetPropertyValue<std::string>(propName, path);
}

const TextureButtonStateTween& TextureButton::GetStateTween(TextureButtonState state) const {
    return m_StateTweens[static_cast<int>(state)];
}

void TextureButton::SetStateTween(TextureButtonState state, const TextureButtonStateTween& tween) {
    m_StateTweens[static_cast<int>(state)] = tween;
}

const std::vector<UIControl::ThemeBinding>& TextureButton::GetThemeBindings() const {
    static const std::vector<ThemeBinding> kBindings = []() {
        std::vector<ThemeBinding> b = {
            { "fontColor",   "font_color", ThemeBinding::Kind::Color },
            { "fontPath",    "font",       ThemeBinding::Kind::Font },
            { "fontSize",    "font_size",  ThemeBinding::Kind::Constant },
            { "texturePath", "texture",    ThemeBinding::Kind::Image }
        };
        // Per-state modulation + tween: the entry name matches the property name.
        const char* states[] = { "normal", "hover", "pressed", "disabled" };
        for (const char* s : states) {
            std::string p(s);
            b.push_back({ p + "TexturePath",   p + "_texture",      ThemeBinding::Kind::Image });
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

std::string TextureButton::GetText() const {
    std::string locKey = GetPropertyValue<std::string>("localizationKey");
    if (!locKey.empty()) {
        return localization::LocalizationManager::GetInstance().Translate(
            locKey, GetPropertyValue<std::string>("localizationTable"));
    }
    return GetPropertyValue<std::string>("text");
}

void TextureButton::SetText(const std::string& text) {
    SetPropertyValue<std::string>("text", text);
    m_TextMeshNeedsRegeneration = true;
}

std::string TextureButton::GetFontPath() const {
    return ResolveThemedFontPath("fontPath", "font");
}

void TextureButton::SetFontPath(const std::string& path) {
    SetThemedProperty<std::string>("fontPath", path);
    m_CurrentFontPath = path;
    m_TextMeshNeedsRegeneration = true;
}

float TextureButton::GetFontSize() const {
    return ResolveThemedFontSize("fontSize", "font_size", "font");
}

void TextureButton::SetFontSize(float size) {
    SetThemedProperty<float>("fontSize", size);
    m_CurrentFontSize = size;
    m_TextMeshNeedsRegeneration = true;
}

Color TextureButton::GetFontColor() const {
    return ResolveThemedColor("fontColor", "font_color");
}

void TextureButton::SetFontColor(const Color& color) {
    SetThemedProperty<Color>("fontColor", color);
    m_TextMeshNeedsRegeneration = true;
}

// ===== Callbacks =====

void TextureButton::SetStateCallback(TextureButtonState state, StateCallback callback) {
    m_StateCallbacks[state] = callback;
}

void TextureButton::ClearStateCallback(TextureButtonState state) {
    m_StateCallbacks.erase(state);
}

void TextureButton::SetOnClickedCallback(StateCallback callback) {
    m_OnClickedCallback = callback;
}

void TextureButton::SetOnPressedCallback(StateCallback callback) {
    m_OnPressedCallback = callback;
}

void TextureButton::SetOnReleasedCallback(StateCallback callback) {
    m_OnReleasedCallback = callback;
}

void TextureButton::SetOnToggledCallback(ToggleCallback callback) {
    m_OnToggledCallback = callback;
}

// ===== Private Methods =====

void TextureButton::SyncFromProperties() {
    // Sync style mode
    TextureButtonStyleMode newStyleMode = static_cast<TextureButtonStyleMode>(GetPropertyValue<int>("styleMode"));
    if (newStyleMode != m_StyleMode) {
        m_StyleMode = newStyleMode;
        m_MeshNeedsRegeneration = true;
    }

    // Sync automatic texture path (themed: a theme may supply the texture)
    if (m_StyleMode == TextureButtonStyleMode::Automatic) {
        std::string newTexturePath = ResolveThemedImage("texturePath", "texture");
        if (newTexturePath != m_CurrentAutomaticTexturePath) {
            m_AutomaticTexturePath = newTexturePath;
            m_CurrentAutomaticTexturePath = newTexturePath;

            // Clear old texture to force reload
            m_AutomaticTextureAsset.Reset();
            m_AutomaticTextureHandle = TextureHandle();

            m_MeshNeedsRegeneration = true;
        }
    }

    // Sync manual texture paths (themed: a theme may supply per-state textures)
    if (m_StyleMode == TextureButtonStyleMode::Manual) {
        auto syncStatePath = [this](TextureButtonState state, const std::string& propName,
                                    const std::string& entry) {
            std::string newPath = ResolveThemedImage(propName, entry);
            int idx = static_cast<int>(state);
            if (newPath != m_StateTextures[idx].texturePath) {
                m_StateTextures[idx].texturePath = newPath;

                // Clear old texture to force reload
                m_StateTextures[idx].textureAsset.Reset();
                m_StateTextures[idx].textureHandle = TextureHandle();

                m_MeshNeedsRegeneration = true;
            }
        };

        syncStatePath(TextureButtonState::Normal, "normalTexturePath", "normal_texture");
        syncStatePath(TextureButtonState::Hover, "hoverTexturePath", "hover_texture");
        syncStatePath(TextureButtonState::Pressed, "pressedTexturePath", "pressed_texture");
        syncStatePath(TextureButtonState::Disabled, "disabledTexturePath", "disabled_texture");
    }

    // Load per-state modulation + tween data from theme or per-instance overrides
    LoadThemedStateData();

    // Sync font
    std::string newFontPath = ResolveThemedFontPath("fontPath", "font");
    if (newFontPath != m_CurrentFontPath) {
        m_CurrentFontPath = newFontPath;

        // Clear old font to force reload
        m_FontAsset.Reset();
        m_FontHandle = FontHandle();

        m_TextMeshNeedsRegeneration = true;
    }

    float newFontSize = ResolveThemedFontSize("fontSize", "font_size", "font");
    if (newFontSize != m_CurrentFontSize) {
        m_CurrentFontSize = newFontSize;

        // Clear old font to force reload with new size
        m_FontAsset.Reset();
        m_FontHandle = FontHandle();

        m_TextMeshNeedsRegeneration = true;
    }

    // Sync font color
    math::Color newFontColor = GetPropertyValue<math::Color>("fontColor");
    if (newFontColor != m_CachedFontColor) {
        m_TextMeshNeedsRegeneration = true;
    }

    // Sync text (resolved through localization so locale changes live-update)
    std::string newText = GetText();
    if (newText != m_CachedText) {
        m_TextMeshNeedsRegeneration = true;
    }

    // Sync sound paths
    auto syncSound = [this](TextureButtonState state, const std::string& propName) {
        std::string newPath = GetPropertyValue<std::string>(propName);
        int idx = static_cast<int>(state);
        if (newPath != m_StateSounds[idx].audioPath) {
            m_StateSounds[idx].audioPath = newPath;
            // Clear old audio asset to force reload
            m_StateSounds[idx].audioAsset.Reset();
        }
    };

    syncSound(TextureButtonState::Normal, "normalSoundPath");
    syncSound(TextureButtonState::Hover, "hoverSoundPath");
    syncSound(TextureButtonState::Pressed, "pressedSoundPath");
    syncSound(TextureButtonState::Disabled, "disabledSoundPath");
}

void TextureButton::LoadThemedStateData() {
    struct StateMap { TextureButtonState state; const char* prefix; };
    static const StateMap kStates[] = {
        { TextureButtonState::Normal,   "normal" },
        { TextureButtonState::Hover,    "hover" },
        { TextureButtonState::Pressed,  "pressed" },
        { TextureButtonState::Disabled, "disabled" },
    };
    for (const StateMap& sm : kStates) {
        int idx = static_cast<int>(sm.state);
        std::string p(sm.prefix);
        m_StateTextures[idx].modulationColor = ResolveThemedColor(p + "Modulation", p + "Modulation");
        m_StateTweens[idx].enabled        = ResolveThemedBool(p + "TweenEnabled", p + "TweenEnabled");
        m_StateTweens[idx].scaleOffset    = ResolveThemedVec2(p + "TweenScale", p + "TweenScale");
        m_StateTweens[idx].rotationOffset = ResolveThemedConstant(p + "TweenRotation", p + "TweenRotation");
        m_StateTweens[idx].positionOffset = ResolveThemedVec2(p + "TweenPosition", p + "TweenPosition");
        m_StateTweens[idx].duration       = ResolveThemedConstant(p + "TweenDuration", p + "TweenDuration");
    }
}

void TextureButton::UpdateMouseInteraction(float) {
    if (!m_ButtonEnabled) {
        m_PressArmed = false;
        return;
    }

    input::InputManager& inputMgr = input::InputManager::Get();

    // Mouse in the centered canvas space the control rects live in, so hit-testing
    // matches rendering on every backend (shared helper).
    Vec2 mousePos = GetCanvasMousePosition();

    bool isOver = IsMouseOver(mousePos) && IsTopPointerTarget(mousePos);

    bool leftPressed = inputMgr.IsMouseButtonPressed(input::MouseButton::Left);
    bool leftJustPressed = inputMgr.IsMouseButtonJustPressed(input::MouseButton::Left);

    // A press belongs to the control the button-down landed on, and pointer arbitration
    // is redone every frame. Without this ownership, a button that becomes visible during
    // another button's `pressed` callback wins arbitration on the next frame, still sees
    // the mouse held, and fires from a click the player never aimed at it.
    if (!leftPressed) {
        m_PressArmed = false;
    } else if (leftJustPressed && isOver) {
        m_PressArmed = true;
    }

    bool pressHeld = leftPressed && m_PressArmed;

    TextureButtonState newState = m_CurrentState;

    if (isOver) {
        if (pressHeld) {
            newState = TextureButtonState::Pressed;
        } else {
            newState = TextureButtonState::Hover;
        }
    } else if (pressHeld && m_CurrentState == TextureButtonState::Pressed) {
        newState = TextureButtonState::Pressed;
    } else if (HasFocus()) {
        // Keyboard / gamepad focus lights the button with its Hover (focus) visual
        // so directional navigation shows the same highlight a mouse hover would.
        newState = TextureButtonState::Hover;
    } else {
        newState = TextureButtonState::Normal;
    }

    if (newState != m_CurrentState) {

        TransitionToState(newState);
    }

    m_MouseWasOver = isOver;
}

bool TextureButton::IsMouseOver(const Vec2& mousePos) const {
    if (!GetOwner()) {
        return false;
    }

    core::Node2D* node2D = dynamic_cast<core::Node2D*>(GetOwner());
    if (!node2D) {
        return false;
    }

    Vec2 position = node2D->GetGlobalPosition();
    // Hit-test against the resolved layout size (the rect the button actually
    // renders into), not the raw width/height request. In anchor-stretch layouts
    // those differ, so width/height would shrink the clickable area to a small box
    // at the button's center while the texture draws across the full resolved rect.
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

    // Alpha threshold check
    if (GetClickMaskMode() == ClickMaskMode::AlphaThreshold) {
        // Convert local position to UV coordinates (0-1 range)
        Vec2 uv;
        uv.x = (localMousePos.x + halfSize.x) / size.x;
        uv.y = (localMousePos.y + halfSize.y) / size.y;

        return CheckAlphaAtUV(uv);
    }

    return true;
}

bool TextureButton::CheckAlphaAtUV(const Vec2& uv) const {
    // Get the appropriate texture asset
    asset::AssetRef<asset::ImageAsset> textureAsset;

    if (m_StyleMode == TextureButtonStyleMode::Automatic) {
        textureAsset = m_AutomaticTextureAsset;
    } else {
        textureAsset = m_StateTextures[static_cast<int>(m_CurrentState)].textureAsset;
    }

    if (!textureAsset || !textureAsset->GetData()) {
        return true; // No texture, allow click
    }

    // Get pixel coordinates
    int width = textureAsset->GetWidth();
    int height = textureAsset->GetHeight();
    int x = static_cast<int>(uv.x * width);
    int y = static_cast<int>(uv.y * height);

    // Clamp to texture bounds
    x = std::max(0, std::min(x, width - 1));
    y = std::max(0, std::min(y, height - 1));

    // Get pixel data (assuming RGBA format)
    const unsigned char* data = textureAsset->GetData();
    int channels = 4; // RGBA
    int index = (y * width + x) * channels;

    // Get alpha value
    float alpha = data[index + 3] / 255.0f;

    // Compare with threshold
    return alpha >= GetAlphaThreshold();
}

void TextureButton::TransitionToState(TextureButtonState newState) {
    if (newState == m_CurrentState) {
        return;
    }

    m_PreviousState = m_CurrentState;
    m_CurrentState = newState;

    // Play state sound
    PlayStateSound(newState);

    // Invoke state callback
    InvokeStateCallback(newState);

    if (newState == TextureButtonState::Pressed) {
        Emit("pressed");
    }

    // Start tween animation
    const TextureButtonStateTween& tween = m_StateTweens[static_cast<int>(newState)];
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
}

void TextureButton::PlayStateSound(TextureButtonState state) {
    const TextureButtonStateSound& sound = m_StateSounds[static_cast<int>(state)];
    if (sound.audioPath.empty()) {
        return;
    }

    // Load audio asset if not already loaded
    if (!sound.audioAsset) {
        auto& mutableSound = const_cast<TextureButtonStateSound&>(sound);
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

void TextureButton::InvokeStateCallback(TextureButtonState state) {
    auto it = m_StateCallbacks.find(state);
    if (it != m_StateCallbacks.end() && it->second) {
        it->second();
    }
}

void TextureButton::LoadStateTexture(TextureButtonState state, const std::string& path) {
    int idx = static_cast<int>(state);

    if (path.empty()) {
        m_StateTextures[idx].textureAsset.Reset();
        m_StateTextures[idx].textureHandle = TextureHandle();
        return;
    }

    m_StateTextures[idx].textureAsset = asset::AssetRef<asset::ImageAsset>(new asset::ImageAsset());
    bool loaded = m_StateTextures[idx].textureAsset->LoadFromFile(path, true, asset::ImageColorSpace::sRGB);

    if (!loaded) {
        
        m_StateTextures[idx].textureAsset.Reset();
    }
}

void TextureButton::UploadTextureToGPU(TextureButtonState state, RenderContext& ctx) {
    int idx = static_cast<int>(state);

    if (!m_StateTextures[idx].textureAsset || !m_StateTextures[idx].textureAsset->GetData()) {
        return;
    }

    if (m_StateTextures[idx].textureHandle.isValid()) {
        return; // Already uploaded
    }

    auto device = ctx.getDevice();
    if (!device) {
        return;
    }

    m_StateTextures[idx].textureHandle = lupine::CreateTexture2DFromImage(device, *m_StateTextures[idx].textureAsset, TextureFormat::RGBA8_UNORM);
}

TextureHandle TextureButton::GetEffectiveTexture() const {
    if (m_StyleMode == TextureButtonStyleMode::Automatic) {
        return m_AutomaticTextureHandle;
    } else {
        return m_StateTextures[static_cast<int>(m_CurrentState)].textureHandle;
    }
}

Color TextureButton::GetEffectiveColor() const {
    return m_StateTextures[static_cast<int>(m_CurrentState)].modulationColor;
}

void TextureButton::RenderTexture(RenderContext& ctx, const Vec2& position, const Vec2& size, float rotation) {
    // Load textures if needed
    if (m_StyleMode == TextureButtonStyleMode::Automatic) {
        if (!m_AutomaticTextureAsset && !m_AutomaticTexturePath.empty()) {
            m_AutomaticTextureAsset = asset::AssetRef<asset::ImageAsset>(new asset::ImageAsset());
            bool loaded = m_AutomaticTextureAsset->LoadFromFile(m_AutomaticTexturePath, true, asset::ImageColorSpace::sRGB);
            if (!loaded) {
                
                m_AutomaticTextureAsset.Reset();
            }
        }

        // Upload to GPU if needed
        if (m_AutomaticTextureAsset && !m_AutomaticTextureHandle.isValid()) {
            auto device = ctx.getDevice();
            if (device) {
                m_AutomaticTextureHandle = lupine::CreateTexture2DFromImage(device, *m_AutomaticTextureAsset, TextureFormat::RGBA8_UNORM);
            }
        }
    } else {
        // Manual mode - load current state texture
        int idx = static_cast<int>(m_CurrentState);
        if (!m_StateTextures[idx].textureAsset && !m_StateTextures[idx].texturePath.empty()) {
            LoadStateTexture(m_CurrentState, m_StateTextures[idx].texturePath);
        }

        // Upload to GPU if needed
        if (m_StateTextures[idx].textureAsset && !m_StateTextures[idx].textureHandle.isValid()) {
            UploadTextureToGPU(m_CurrentState, ctx);
        }
    }

    TextureHandle texture = GetEffectiveTexture();
    if (!texture.isValid()) {
        return; // No texture to render
    }

    Color effectiveColor = GetEffectiveColor();

    // Apply tween offsets
    Vec2 tweenedPosition = position + m_CurrentPositionOffset;
    Vec2 tweenedScale = Vec2(1.0f, 1.0f) + m_CurrentScaleOffset;
    Vec2 tweenedSize = size * tweenedScale;
    float tweenedRotation = rotation + m_CurrentRotationOffset;

    TextureButtonStretchMode stretchMode = GetStretchMode();
    bool useStretch = GetUseStretch();

    if (!useStretch) {
        stretchMode = TextureButtonStretchMode::KeepCentered;
    }

    // Nine-slice configuration starts from this control's own properties. A themed
    // image entry for the image being drawn (the "texture" entry in Automatic mode,
    // or the current state's "<state>_texture" entry in Manual mode) may also dictate
    // the stretch / nine-slice fit, in which case it overrides these — so a theme can
    // set its texture-button images to nine-slice and supply the margins.
    Vec4 nsMargins = GetNineSliceMargins();
    UINineSliceAxisMode nsAxisH = GetNineSliceAxisHorizontal();
    UINineSliceAxisMode nsAxisV = GetNineSliceAxisVertical();
    bool nsDrawCenter = GetNineSliceDrawCenter();
    {
        std::string pathProp;
        std::string imageEntry;
        if (m_StyleMode == TextureButtonStyleMode::Automatic) {
            pathProp = "texturePath";
            imageEntry = "texture";
        } else {
            const char* stateName = "normal";
            switch (m_CurrentState) {
                case TextureButtonState::Hover:    stateName = "hover";    break;
                case TextureButtonState::Pressed:  stateName = "pressed";  break;
                case TextureButtonState::Disabled: stateName = "disabled"; break;
                default:                           stateName = "normal";   break;
            }
            pathProp = std::string(stateName) + "TexturePath";
            imageEntry = std::string(stateName) + "_texture";
        }
        ui::ThemeImage themedImage;
        if (ResolveThemedImageEx(pathProp, imageEntry, themedImage) && themedImage.hasStretch) {
            stretchMode = static_cast<TextureButtonStretchMode>(themedImage.stretchMode);
            if (themedImage.stretchMode == 2) {
                nsMargins = Vec4(themedImage.marginLeft, themedImage.marginTop,
                                 themedImage.marginRight, themedImage.marginBottom);
                nsAxisH = UINineSliceAxisModeFromInt(themedImage.axisH);
                nsAxisV = UINineSliceAxisModeFromInt(themedImage.axisV);
                nsDrawCenter = themedImage.drawCenter;
            }
        }
    }

    // Get texture dimensions
    int textureWidth = 0;
    int textureHeight = 0;
    if (m_StyleMode == TextureButtonStyleMode::Automatic && m_AutomaticTextureAsset) {
        textureWidth = m_AutomaticTextureAsset->GetWidth();
        textureHeight = m_AutomaticTextureAsset->GetHeight();
    } else if (m_StyleMode == TextureButtonStyleMode::Manual) {
        int idx = static_cast<int>(m_CurrentState);
        if (m_StateTextures[idx].textureAsset) {
            textureWidth = m_StateTextures[idx].textureAsset->GetWidth();
            textureHeight = m_StateTextures[idx].textureAsset->GetHeight();
        }
    }

    switch (stretchMode) {
        case TextureButtonStretchMode::Stretch: {
            // Stretch texture to fill button - use full UV range
            ctx.drawRoundedRect(
                tweenedPosition,
                tweenedSize,
                Vec4(0.0f, 0.0f, 0.0f, 0.0f),
                effectiveColor,
                texture,
                tweenedRotation,
                Vec2(0.0f, 0.0f),
                Vec2(1.0f, 1.0f),
                0
            );
            break;
        }

        case TextureButtonStretchMode::KeepCentered: {
            // Keep texture at original size, centered - calculate UV to show only portion that fits
            if (textureWidth > 0 && textureHeight > 0) {
                // Calculate how much of the texture to show based on button size
                float uScale = tweenedSize.x / static_cast<float>(textureWidth);
                float vScale = tweenedSize.y / static_cast<float>(textureHeight);

                // Center the texture by adjusting UV coordinates
                float uMin = (1.0f - uScale) * 0.5f;
                float vMin = (1.0f - vScale) * 0.5f;
                float uMax = uMin + uScale;
                float vMax = vMin + vScale;

                ctx.drawRoundedRect(
                    tweenedPosition,
                    tweenedSize,
                    Vec4(0.0f, 0.0f, 0.0f, 0.0f),
                    effectiveColor,
                    texture,
                    tweenedRotation,
                    Vec2(uMin, vMin),
                    Vec2(uMax, vMax),
                    0
                );
            }
            break;
        }

        case TextureButtonStretchMode::NineSlice: {
            // Nine-slice scaling via the shared painter, honoring the configurable
            // per-side margins (source texture pixels), per-axis stretch/tile, and
            // optional center fill. The whole rect (corners included) follows the
            // interaction tween, matching every other nine-slice control.
            if (textureWidth > 0 && textureHeight > 0) {
                UINineSlice nineSlice;
                nineSlice.marginLeft = nsMargins.x;
                nineSlice.marginTop = nsMargins.y;
                nineSlice.marginRight = nsMargins.z;
                nineSlice.marginBottom = nsMargins.w;
                nineSlice.axisHorizontal = nsAxisH;
                nineSlice.axisVertical = nsAxisV;
                nineSlice.drawCenter = nsDrawCenter;

                DrawUIImage(ctx, tweenedPosition, tweenedSize, tweenedRotation, texture,
                            textureWidth, textureHeight, effectiveColor,
                            Vec4(0.0f, 0.0f, 0.0f, 0.0f), UIImageStretchMode::NineSlice, nineSlice);
            }
            break;
        }
    }
}

Vec2 TextureButton::CalculateButtonSize() const {
    Vec2 baseSize(GetPropertyValue<float>("width"), GetPropertyValue<float>("height"));
    return baseSize;
}

void TextureButton::RegenerateTextMesh(RenderContext& ctx, const Vec2& position, float rotation) {
    std::string text = GetText();
    if (text.empty()) {
        if (m_TextMesh.isValid()) {
            ctx.getDevice()->destroyMesh(m_TextMesh);
            m_TextMesh = MeshHandle();
        }
        m_CachedText = "";
        m_CachedFontSize = 0.0f;
        m_TextMeshNeedsRegeneration = false;
        return;
    }

    // Load font if needed
    if (!m_FontAsset && !m_CurrentFontPath.empty()) {
        m_FontAsset = asset::AssetRef<asset::FontAsset>(new asset::FontAsset());
        bool loaded = m_FontAsset->LoadFromFile(m_CurrentFontPath);
        if (!loaded) {
            
            m_FontAsset.Reset();
            m_TextMeshNeedsRegeneration = false;
            return;
        }
    }

    if (!m_FontAsset) {
        m_TextMeshNeedsRegeneration = false;
        return;
    }

    const FontAtlas* fontAtlas = ctx.getDevice()->getFontAtlas(m_FontHandle);
    if (!fontAtlas) {
        return;
    }

    float fontSize = GetFontSize();
    Vec2 textSize = CalculateTextSize();

    // Calculate text position centered on button position
    Vec2 textPos = position;
    textPos.x -= textSize.x * 0.5f;

    if (m_FontAsset.IsValid() && m_FontAsset->IsLoaded()) {
        float scaleFactor = fontSize / m_FontAsset->GetFontSize();
        float ascent = m_FontAsset->GetAscent() * scaleFactor;
        float descent = m_FontAsset->GetDescent() * scaleFactor;
        textPos.y -= (ascent + descent) * 0.5f;
    }

    float scale = fontSize / fontAtlas->fontSize;
    MeshData textMeshData;
    Vec2 cursor = textPos;
    uint32_t vertexOffset = 0;

    // Use font color
    Color fontColor = GetFontColor();

    // Lambda to rotate vertex around button position (like Label.cpp does)
    // Note: rotation is in radians from GetGlobalRotation()
    auto rotateVertex = [&](float x, float y) -> Vec3 {
        if (std::abs(rotation) > 0.0001f) {
            float relX = x - position.x;
            float relY = y - position.y;
            float cosR = std::cos(rotation);
            float sinR = std::sin(rotation);
            float rotX = relX * cosR - relY * sinR;
            float rotY = relX * sinR + relY * cosR;
            return Vec3(rotX + position.x, rotY + position.y, 0.1f);
        }
        return Vec3(x, y, 0.1f);
    };

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

        float glyphX = cursor.x + glyph->bearing.x * scale;
        float glyphY = cursor.y - glyph->bearing.y * scale;
        float glyphW = glyph->size.x * scale;
        float glyphH = glyph->size.y * scale;

        Vertex v0, v1, v2, v3;
        v0.position = rotateVertex(glyphX, glyphY - glyphH);
        v0.normal = Vec3(0, 0, 1);
        v0.texCoord = Vec2(glyph->uvMin.x, glyph->uvMax.y);
        v0.color = Vec4(fontColor.r, fontColor.g, fontColor.b, fontColor.a);

        v1.position = rotateVertex(glyphX + glyphW, glyphY - glyphH);
        v1.normal = Vec3(0, 0, 1);
        v1.texCoord = Vec2(glyph->uvMax.x, glyph->uvMax.y);
        v1.color = Vec4(fontColor.r, fontColor.g, fontColor.b, fontColor.a);

        v2.position = rotateVertex(glyphX + glyphW, glyphY);
        v2.normal = Vec3(0, 0, 1);
        v2.texCoord = Vec2(glyph->uvMax.x, glyph->uvMin.y);
        v2.color = Vec4(fontColor.r, fontColor.g, fontColor.b, fontColor.a);

        v3.position = rotateVertex(glyphX, glyphY);
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

    if (textMeshData.vertices.empty()) {
        m_TextMeshNeedsRegeneration = false;
        return;
    }

    if (m_TextMesh.isValid()) {
        ctx.getDevice()->destroyMesh(m_TextMesh);
    }

    m_TextMesh = ctx.getDevice()->createMesh(textMeshData);
    m_CachedText = text;
    m_CachedFontSize = fontSize;
    m_CachedFontColor = GetFontColor();
    m_CachedPosition = position;
    m_CachedRotation = rotation;
    m_TextMeshNeedsRegeneration = false;
}

Vec2 TextureButton::CalculateTextSize() const {
    if (!m_FontAsset.IsValid() || !m_FontAsset->IsLoaded()) {
        return Vec2(0.0f, 0.0f);
    }

    std::string text = GetText();
    float fontSize = GetFontSize();

    return m_FontAsset->MeasureText(text, fontSize);
}

void TextureButton::RenderText(RenderContext& ctx, const Vec2& position, const Vec2&, float baseRotation) {
    std::string text = GetText();
    if (text.empty()) {
        return;
    }

    if (!m_FontHandle.isValid()) {
        return;
    }

    float fontSize = GetFontSize();
    Color fontColor = GetFontColor();

    // Regenerate mesh if content/style changed or position/rotation changed
    if (text != m_CachedText ||
        fontSize != m_CachedFontSize ||
        fontColor != m_CachedFontColor ||
        position != m_CachedPosition ||
        std::abs(baseRotation - m_CachedRotation) > 0.001f ||
        m_TextMeshNeedsRegeneration ||
        !m_TextMesh.isValid()) {
        RegenerateTextMesh(ctx, position, baseRotation);
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

    // Mesh is already at final position, use identity transform (like Label.cpp)
    Mat4 transform = Mat4::Identity();

    ctx.drawMesh(m_TextMesh, ctx.getDefaultTextMaterial(), transform, overrides);
}

// ===== IRenderableComponent Implementation =====

void TextureButton::buildDrawCommands(RenderContext& ctx) {
    if (!IsEnabled() || !GetOwner()) {
        return;
    }

    Node2D* node2D = dynamic_cast<Node2D*>(GetOwner());
    if (!node2D) {
        return;
    }

    // A resolution change re-bakes the font atlas at a new oversampling density,
    // which repacks glyph UVs; regenerate the cached text mesh so it matches.
    if (ConsumeFontOversampleChanged()) {
        m_TextMeshNeedsRegeneration = true;
    }

    // Load and setup font if needed
    std::string fontPath = GetFontPath();
    float fontSize = GetFontSize();
    bool fontPathChanged = (fontPath != m_CurrentFontPath);
    bool fontSizeChanged = (fontSize != m_CurrentFontSize);
    bool needsFontLoad = !fontPath.empty() && (!m_FontAsset.IsValid() || !m_FontAsset->IsLoaded());

    if (fontPathChanged || fontSizeChanged || needsFontLoad) {
        m_CurrentFontPath = fontPath;
        m_CurrentFontSize = fontSize;

        if (m_FontHandle.isValid()) {
            m_FontHandle = FontHandle();
        }

        m_FontAsset.Reset();

        if (!fontPath.empty()) {
            m_FontAsset = asset::AssetRef<asset::FontAsset>(new asset::FontAsset());

            uint32_t minAtlasSize = static_cast<uint32_t>(fontSize * 10.0f);
            uint32_t atlasSize = 512;
            while (atlasSize < minAtlasSize && atlasSize < 4096) {
                atlasSize *= 2;
            }

            bool loaded = m_FontAsset->LoadFromFile(fontPath, fontSize, atlasSize, atlasSize);
            if (!loaded) {
                
                m_FontAsset.Reset();
            } else {
                
            }
        }
    }

    if (m_FontAsset.IsValid() && m_FontAsset->IsLoaded() && !m_FontHandle.isValid()) {
        FontDesc fontDesc;
        // Use GetPhysicalPath() to resolve res:// path to filesystem path for file loading
        fontDesc.fontPath = m_FontAsset->GetPhysicalPath();
        fontDesc.fontSize = fontSize;
        fontDesc.atlasWidth = m_FontAsset->GetAtlasWidth();
        fontDesc.atlasHeight = m_FontAsset->GetAtlasHeight();

        IGfxDevice* device = ctx.getDevice();
        if (device) {
            m_FontHandle = device->createFontAtlas(fontDesc);
            if (!m_FontHandle.isValid()) {

            } else {

            }
        }
    }

    const math::Rect __rect = GetResolvedRect();
    Vec2 position = __rect.GetCenter();
    Vec2 size = __rect.size;
    float rotation = node2D->GetGlobalRotation();

    // RenderTexture applies the tween offsets itself; RenderText does not, so it gets
    // the already-offset position and the untweened rotation.
    Vec2 tweenedPosition = position + m_CurrentPositionOffset;

    // Render texture
    RenderTexture(ctx, position, size, rotation);

    // Render text overlay with tweened position and base rotation
    RenderText(ctx, tweenedPosition, size, rotation);
}

AABB TextureButton::getWorldBounds() const {

    Node2D* node2D = dynamic_cast<Node2D*>(GetOwner());

    if (!node2D) {
        return AABB();
    }

    Vec2 position = node2D->GetGlobalPosition();
    Vec2 size = GetBoundsSize();
    Vec2 halfSize = size * 0.5f;

    AABB bounds;
    bounds.min = Vec3(position.x - halfSize.x, position.y - halfSize.y, 0.0f);
    bounds.max = Vec3(position.x + halfSize.x, position.y + halfSize.y, 0.0f);

    return bounds;
}

RenderLayer TextureButton::getRenderLayer() const {
    int layer = GetLayer();
    int sortingOrder = GetSortingOrder();

    return static_cast<RenderLayer>(layer * 1000 + sortingOrder);
}

SpatialType TextureButton::getSpatialType() const {
    return GetUISpatialType();
}

OBB TextureButton::getOrientedBounds() const {
    if (!GetOwner()) {
        return math::OBB();
    }

    Node2D* node2D = dynamic_cast<Node2D*>(GetOwner());
    if (!node2D) {
        return math::OBB();
    }

    Vec2 position = node2D->GetGlobalPosition();
    Vec2 size = GetBoundsSize();
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

bool TextureButton::IntersectRay(const Ray& ray, float& outDistance) const {
    math::OBB obb = getOrientedBounds();

    Vec3 localRayOrigin = obb.rotation.Inverse() * (ray.origin - obb.center);
    Vec3 localRayDir = obb.rotation.Inverse() * ray.direction;

    AABB localAABB(
        Vec3(-obb.extents.x, -obb.extents.y, -obb.extents.z),
        Vec3(obb.extents.x, obb.extents.y, obb.extents.z)
    );

    math::Ray localRay(localRayOrigin, localRayDir);
    return localAABB.IntersectRay(localRay, outDistance);
}

bool TextureButton::OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) {
    auto& assetDb = asset::AssetDatabase::GetInstance();
    bool anyChanged = false;

    auto matchesPath = [&](const std::string& currentPath) -> bool {
        if (currentPath.empty()) return false;
        std::string resolved;
        if (assetDb.IsInitialized()) {
            resolved = assetDb.ResolveAsset(currentPath);
        }
        return (currentPath == changedPath) ||
               (!resolved.empty() && !resolvedChangedPath.empty() && resolved == resolvedChangedPath);
    };

    // Check automatic texture
    if (matchesPath(m_AutomaticTexturePath)) {
        
        m_AutomaticTextureHandle = TextureHandle();
        m_AutomaticTextureAsset.Reset();
        m_CurrentAutomaticTexturePath.clear();
        anyChanged = true;
    }

    // Check per-state textures
    for (int i = 0; i < static_cast<int>(TextureButtonState::COUNT); ++i) {
        auto& stateTexture = m_StateTextures[i];
        if (matchesPath(stateTexture.texturePath)) {
            
            stateTexture.textureHandle = TextureHandle();
            stateTexture.textureAsset.Reset();
            anyChanged = true;
        }
    }

    // Check font
    std::string fontPath = GetFontPath();
    if (matchesPath(fontPath)) {
        
        m_FontHandle = FontHandle();
        m_FontAsset.Reset();
        m_CurrentFontPath.clear();
        m_MeshNeedsRegeneration = true;
        anyChanged = true;
    }

    return anyChanged;
}

} // namespace components
} // namespace lupine

