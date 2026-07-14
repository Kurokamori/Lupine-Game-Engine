#include "lupine/components/TextureToggleButton.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/rendering/TextureUpload.hpp"
#include "lupine/rendering/gfx/GfxDescriptors.hpp"
#include "lupine/input/InputManager.hpp"
#include "lupine/rendering/ViewportUtils.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/math/OBB.hpp"

namespace lupine {
namespace components {

using namespace core;
using namespace math;

TextureToggleButton::TextureToggleButton()
    : UIControl("TextureToggleButton")
{
    // Initialize default state modulation colors
    m_StateTextures[static_cast<int>(TextureToggleButtonState::Normal)].modulationColor = Color::White();
    m_StateTextures[static_cast<int>(TextureToggleButtonState::Hover)].modulationColor = Color(1.2f, 1.2f, 1.2f, 1.0f);
    m_StateTextures[static_cast<int>(TextureToggleButtonState::Pressed)].modulationColor = Color(0.8f, 0.8f, 0.8f, 1.0f);
    m_StateTextures[static_cast<int>(TextureToggleButtonState::Toggled)].modulationColor = Color(1.3f, 1.3f, 1.3f, 1.0f);
    m_StateTextures[static_cast<int>(TextureToggleButtonState::ToggledHover)].modulationColor = Color(1.5f, 1.5f, 1.5f, 1.0f);
    m_StateTextures[static_cast<int>(TextureToggleButtonState::ToggledPressed)].modulationColor = Color(1.1f, 1.1f, 1.1f, 1.0f);
    m_StateTextures[static_cast<int>(TextureToggleButtonState::Disabled)].modulationColor = Color(0.5f, 0.5f, 0.5f, 0.5f);
}

TextureToggleButton::TextureToggleButton(const std::string& name)
    : UIControl(name)
{
    // Initialize default state modulation colors
    m_StateTextures[static_cast<int>(TextureToggleButtonState::Normal)].modulationColor = Color::White();
    m_StateTextures[static_cast<int>(TextureToggleButtonState::Hover)].modulationColor = Color(1.2f, 1.2f, 1.2f, 1.0f);
    m_StateTextures[static_cast<int>(TextureToggleButtonState::Pressed)].modulationColor = Color(0.8f, 0.8f, 0.8f, 1.0f);
    m_StateTextures[static_cast<int>(TextureToggleButtonState::Toggled)].modulationColor = Color(1.3f, 1.3f, 1.3f, 1.0f);
    m_StateTextures[static_cast<int>(TextureToggleButtonState::ToggledHover)].modulationColor = Color(1.5f, 1.5f, 1.5f, 1.0f);
    m_StateTextures[static_cast<int>(TextureToggleButtonState::ToggledPressed)].modulationColor = Color(1.1f, 1.1f, 1.1f, 1.0f);
    m_StateTextures[static_cast<int>(TextureToggleButtonState::Disabled)].modulationColor = Color(0.5f, 0.5f, 0.5f, 0.5f);
}

TextureToggleButton::~TextureToggleButton() {
}

void TextureToggleButton::DefineProperties() {
    DefineUIControlProperties(100.0f, 100.0f, "useUISpace", "Size");

    // Rendering
    DefineProperty(PROPERTY_INT_RANGE_GROUP(layer, 0, -100, 100, 1, "Rendering"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(sortingOrder, 0, -1000, 1000, 1, "Rendering"));

    // Button
    DefineProperty(PROPERTY_DEFAULT_GROUP(buttonEnabled, Bool, true, "Button"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(isToggled, Bool, false, "Button"));

    // Nine-Slice
    DefineProperty(PROPERTY_DEFAULT_GROUP(useNineSlice, Bool, false, "Nine-Slice"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(marginLeft, 10.0f, 0.0f, 1000.0f, 1.0f, "Nine-Slice"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(marginRight, 10.0f, 0.0f, 1000.0f, 1.0f, "Nine-Slice"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(marginTop, 10.0f, 0.0f, 1000.0f, 1.0f, "Nine-Slice"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(marginBottom, 10.0f, 0.0f, 1000.0f, 1.0f, "Nine-Slice"));
    DefineProperty(PROPERTY_ENUM_GROUP(nineSliceAxisH, 0, "Nine-Slice", Stretch, Tile));
    DefineProperty(PROPERTY_ENUM_GROUP(nineSliceAxisV, 0, "Nine-Slice", Stretch, Tile));
    DefineProperty(PROPERTY_DEFAULT_GROUP(nineSliceDrawCenter, Bool, true, "Nine-Slice"));

    // State Textures - Normal
    DefineProperty(PROPERTY_FILE_GROUP(normalTexturePath, std::string(""), "*.png,*.jpg,*.jpeg,*.bmp,*.tga", "State_Normal"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(normalModulation, Color, Color::White(), "State_Normal"));
    DefineProperty(PROPERTY_FILE_GROUP(normalSoundPath, std::string(""), "*.wav,*.ogg,*.mp3", "State_Normal"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(normalTweenEnabled, Bool, false, "State_Normal"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(normalTweenScale, Vec2, Vec2(0.0f, 0.0f), "State_Normal"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(normalTweenRotation, 0.0f, -360.0f, 360.0f, 1.0f, "State_Normal"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(normalTweenPosition, Vec2, Vec2(0.0f, 0.0f), "State_Normal"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(normalTweenDuration, 0.2f, 0.0f, 5.0f, 0.01f, "State_Normal"));

    // State Textures - Hover
    DefineProperty(PROPERTY_FILE_GROUP(hoverTexturePath, std::string(""), "*.png,*.jpg,*.jpeg,*.bmp,*.tga", "State_Hover"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(hoverModulation, Color, Color(1.2f, 1.2f, 1.2f, 1.0f), "State_Hover"));
    DefineProperty(PROPERTY_FILE_GROUP(hoverSoundPath, std::string(""), "*.wav,*.ogg,*.mp3", "State_Hover"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(hoverTweenEnabled, Bool, true, "State_Hover"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(hoverTweenScale, Vec2, Vec2(0.05f, 0.05f), "State_Hover"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(hoverTweenRotation, 0.0f, -360.0f, 360.0f, 1.0f, "State_Hover"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(hoverTweenPosition, Vec2, Vec2(0.0f, 0.0f), "State_Hover"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(hoverTweenDuration, 0.2f, 0.0f, 5.0f, 0.01f, "State_Hover"));

    // State Textures - Pressed
    DefineProperty(PROPERTY_FILE_GROUP(pressedTexturePath, std::string(""), "*.png,*.jpg,*.jpeg,*.bmp,*.tga", "State_Pressed"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(pressedModulation, Color, Color(0.8f, 0.8f, 0.8f, 1.0f), "State_Pressed"));
    DefineProperty(PROPERTY_FILE_GROUP(pressedSoundPath, std::string(""), "*.wav,*.ogg,*.mp3", "State_Pressed"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(pressedTweenEnabled, Bool, true, "State_Pressed"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(pressedTweenScale, Vec2, Vec2(-0.05f, -0.05f), "State_Pressed"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(pressedTweenRotation, 0.0f, -360.0f, 360.0f, 1.0f, "State_Pressed"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(pressedTweenPosition, Vec2, Vec2(0.0f, 2.0f), "State_Pressed"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(pressedTweenDuration, 0.1f, 0.0f, 5.0f, 0.01f, "State_Pressed"));

    // State Textures - Toggled
    DefineProperty(PROPERTY_FILE_GROUP(toggledTexturePath, std::string(""), "*.png,*.jpg,*.jpeg,*.bmp,*.tga", "State_Toggled"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(toggledModulation, Color, Color(1.3f, 1.3f, 1.3f, 1.0f), "State_Toggled"));
    DefineProperty(PROPERTY_FILE_GROUP(toggledSoundPath, std::string(""), "*.wav,*.ogg,*.mp3", "State_Toggled"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(toggledTweenEnabled, Bool, false, "State_Toggled"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(toggledTweenScale, Vec2, Vec2(0.0f, 0.0f), "State_Toggled"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(toggledTweenRotation, 0.0f, -360.0f, 360.0f, 1.0f, "State_Toggled"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(toggledTweenPosition, Vec2, Vec2(0.0f, 0.0f), "State_Toggled"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(toggledTweenDuration, 0.2f, 0.0f, 5.0f, 0.01f, "State_Toggled"));

    // State Textures - ToggledHover
    DefineProperty(PROPERTY_FILE_GROUP(toggledHoverTexturePath, std::string(""), "*.png,*.jpg,*.jpeg,*.bmp,*.tga", "State_ToggledHover"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(toggledHoverModulation, Color, Color(1.5f, 1.5f, 1.5f, 1.0f), "State_ToggledHover"));
    DefineProperty(PROPERTY_FILE_GROUP(toggledHoverSoundPath, std::string(""), "*.wav,*.ogg,*.mp3", "State_ToggledHover"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(toggledHoverTweenEnabled, Bool, true, "State_ToggledHover"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(toggledHoverTweenScale, Vec2, Vec2(0.05f, 0.05f), "State_ToggledHover"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(toggledHoverTweenRotation, 0.0f, -360.0f, 360.0f, 1.0f, "State_ToggledHover"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(toggledHoverTweenPosition, Vec2, Vec2(0.0f, 0.0f), "State_ToggledHover"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(toggledHoverTweenDuration, 0.2f, 0.0f, 5.0f, 0.01f, "State_ToggledHover"));

    // State Textures - ToggledPressed
    DefineProperty(PROPERTY_FILE_GROUP(toggledPressedTexturePath, std::string(""), "*.png,*.jpg,*.jpeg,*.bmp,*.tga", "State_ToggledPressed"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(toggledPressedModulation, Color, Color(1.1f, 1.1f, 1.1f, 1.0f), "State_ToggledPressed"));
    DefineProperty(PROPERTY_FILE_GROUP(toggledPressedSoundPath, std::string(""), "*.wav,*.ogg,*.mp3", "State_ToggledPressed"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(toggledPressedTweenEnabled, Bool, true, "State_ToggledPressed"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(toggledPressedTweenScale, Vec2, Vec2(-0.05f, -0.05f), "State_ToggledPressed"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(toggledPressedTweenRotation, 0.0f, -360.0f, 360.0f, 1.0f, "State_ToggledPressed"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(toggledPressedTweenPosition, Vec2, Vec2(0.0f, 2.0f), "State_ToggledPressed"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(toggledPressedTweenDuration, 0.1f, 0.0f, 5.0f, 0.01f, "State_ToggledPressed"));

    // State Textures - Disabled
    DefineProperty(PROPERTY_FILE_GROUP(disabledTexturePath, std::string(""), "*.png,*.jpg,*.jpeg,*.bmp,*.tga", "State_Disabled"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(disabledModulation, Color, Color(0.5f, 0.5f, 0.5f, 0.5f), "State_Disabled"));
    DefineProperty(PROPERTY_FILE_GROUP(disabledSoundPath, std::string(""), "*.wav,*.ogg,*.mp3", "State_Disabled"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(disabledTweenEnabled, Bool, false, "State_Disabled"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(disabledTweenScale, Vec2, Vec2(0.0f, 0.0f), "State_Disabled"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(disabledTweenRotation, 0.0f, -360.0f, 360.0f, 1.0f, "State_Disabled"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(disabledTweenPosition, Vec2, Vec2(0.0f, 0.0f), "State_Disabled"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(disabledTweenDuration, 0.2f, 0.0f, 5.0f, 0.01f, "State_Disabled"));
}

void TextureToggleButton::OnAwake() {
    m_ButtonEnabled = GetPropertyValue<bool>("buttonEnabled");
    m_IsToggled = GetPropertyValue<bool>("isToggled");

    // Load themed per-state modulation + tween data
    LoadThemedStateData();

    // Load state sounds (just paths, assets loaded on-demand)
    auto loadSound = [this](TextureToggleButtonState state, const std::string& prefix) {
        std::string path = GetPropertyValue<std::string>(prefix + "SoundPath");
        if (!path.empty()) {
            SetStateSoundPath(state, path);
        }
    };

    loadSound(TextureToggleButtonState::Normal, "normal");
    loadSound(TextureToggleButtonState::Hover, "hover");
    loadSound(TextureToggleButtonState::Pressed, "pressed");
    loadSound(TextureToggleButtonState::Toggled, "toggled");
    loadSound(TextureToggleButtonState::ToggledHover, "toggledHover");
    loadSound(TextureToggleButtonState::ToggledPressed, "toggledPressed");
    loadSound(TextureToggleButtonState::Disabled, "disabled");

    // Load state textures (just paths, assets loaded on-demand)
    auto loadTexture = [this](TextureToggleButtonState state, const std::string& prefix) {
        std::string path = GetPropertyValue<std::string>(prefix + "TexturePath");
        if (!path.empty()) {
            SetStateTexturePath(state, path);
        }
    };

    loadTexture(TextureToggleButtonState::Normal, "normal");
    loadTexture(TextureToggleButtonState::Hover, "hover");
    loadTexture(TextureToggleButtonState::Pressed, "pressed");
    loadTexture(TextureToggleButtonState::Toggled, "toggled");
    loadTexture(TextureToggleButtonState::ToggledHover, "toggledHover");
    loadTexture(TextureToggleButtonState::ToggledPressed, "toggledPressed");
    loadTexture(TextureToggleButtonState::Disabled, "disabled");
}

void TextureToggleButton::OnReady() {
}

void TextureToggleButton::OnInput(float deltaTime) {
    if (!IsEnabled()) {
        return;
    }

    UpdateMouseInteraction(deltaTime);
}

void TextureToggleButton::OnUpdate(float deltaTime) {
    UIControl::OnUpdate(deltaTime);

    if (!IsEnabled()) {
        return;
    }

    SyncFromProperties();

    // Update tween animation
    if (m_IsTweening && m_TweenProgress < 1.0f) {
        const TextureToggleButtonStateTween& targetTween = m_StateTweens[static_cast<int>(m_CurrentState)];
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

bool TextureToggleButton::OnGizmoScale(float scaleDelta, int axis, bool is3D) {
    // Resizing is delegated to the base, which writes whichever property actually drives
    // the axis: width/height when point-anchored, the offsets when anchor-stretched (where
    // width/height are never read at all), and nothing when a container owns the rect.
    const bool handled = UIControl::OnGizmoScale(scaleDelta, axis, is3D);
    return handled;
}

// ===== Property Accessors =====

int TextureToggleButton::GetLayer() const {
    return GetPropertyValue<int>("layer");
}

void TextureToggleButton::SetLayer(int layer) {
    SetPropertyValue<int>("layer", layer);
}

int TextureToggleButton::GetSortingOrder() const {
    return GetPropertyValue<int>("sortingOrder");
}

void TextureToggleButton::SetSortingOrder(int order) {
    SetPropertyValue<int>("sortingOrder", order);
}

void TextureToggleButton::SetEnabled(bool enabled) {
    m_ButtonEnabled = enabled;
    SetPropertyValue<bool>("buttonEnabled", enabled);
}

bool TextureToggleButton::IsToggled() const {
    return m_IsToggled;
}

void TextureToggleButton::SetToggled(bool toggled) {
    if (m_IsToggled != toggled) {
        m_IsToggled = toggled;
        SetPropertyValue<bool>("isToggled", toggled);

        if (m_OnToggledCallback) {
            m_OnToggledCallback(toggled);
        }
    }
}

bool TextureToggleButton::GetUseNineSlice() const {
    return GetPropertyValue<bool>("useNineSlice");
}

void TextureToggleButton::SetUseNineSlice(bool useNineSlice) {
    SetPropertyValue<bool>("useNineSlice", useNineSlice);
}

float TextureToggleButton::GetMarginLeft() const {
    return GetPropertyValue<float>("marginLeft");
}

void TextureToggleButton::SetMarginLeft(float margin) {
    SetPropertyValue<float>("marginLeft", margin);
}

float TextureToggleButton::GetMarginRight() const {
    return GetPropertyValue<float>("marginRight");
}

void TextureToggleButton::SetMarginRight(float margin) {
    SetPropertyValue<float>("marginRight", margin);
}

float TextureToggleButton::GetMarginTop() const {
    return GetPropertyValue<float>("marginTop");
}

void TextureToggleButton::SetMarginTop(float margin) {
    SetPropertyValue<float>("marginTop", margin);
}

float TextureToggleButton::GetMarginBottom() const {
    return GetPropertyValue<float>("marginBottom");
}

void TextureToggleButton::SetMarginBottom(float margin) {
    SetPropertyValue<float>("marginBottom", margin);
}

UINineSliceAxisMode TextureToggleButton::GetNineSliceAxisHorizontal() const {
    return UINineSliceAxisModeFromInt(GetPropertyValue<int>("nineSliceAxisH"));
}

void TextureToggleButton::SetNineSliceAxisHorizontal(UINineSliceAxisMode mode) {
    SetPropertyValue<int>("nineSliceAxisH", static_cast<int>(mode));
}

UINineSliceAxisMode TextureToggleButton::GetNineSliceAxisVertical() const {
    return UINineSliceAxisModeFromInt(GetPropertyValue<int>("nineSliceAxisV"));
}

void TextureToggleButton::SetNineSliceAxisVertical(UINineSliceAxisMode mode) {
    SetPropertyValue<int>("nineSliceAxisV", static_cast<int>(mode));
}

bool TextureToggleButton::GetNineSliceDrawCenter() const {
    return GetPropertyValue<bool>("nineSliceDrawCenter");
}

void TextureToggleButton::SetNineSliceDrawCenter(bool drawCenter) {
    SetPropertyValue<bool>("nineSliceDrawCenter", drawCenter);
}

void TextureToggleButton::SetOnToggledCallback(ToggleCallback callback) {
    m_OnToggledCallback = callback;
}

// ===== Per-State Textures =====

const TextureToggleButtonStateTexture& TextureToggleButton::GetStateTexture(TextureToggleButtonState state) const {
    return m_StateTextures[static_cast<int>(state)];
}

void TextureToggleButton::SetStateTexture(TextureToggleButtonState state, const TextureToggleButtonStateTexture& texture) {
    m_StateTextures[static_cast<int>(state)] = texture;
}

void TextureToggleButton::SetStateTexturePath(TextureToggleButtonState state, const std::string& path) {
    int idx = static_cast<int>(state);
    m_StateTextures[idx].texturePath = path;

    std::string propName;
    switch (state) {
        case TextureToggleButtonState::Normal: propName = "normalTexturePath"; break;
        case TextureToggleButtonState::Hover: propName = "hoverTexturePath"; break;
        case TextureToggleButtonState::Pressed: propName = "pressedTexturePath"; break;
        case TextureToggleButtonState::Toggled: propName = "toggledTexturePath"; break;
        case TextureToggleButtonState::ToggledHover: propName = "toggledHoverTexturePath"; break;
        case TextureToggleButtonState::ToggledPressed: propName = "toggledPressedTexturePath"; break;
        case TextureToggleButtonState::Disabled: propName = "disabledTexturePath"; break;
        default: return;
    }
    SetPropertyValue<std::string>(propName, path);
}

const Color& TextureToggleButton::GetStateModulation(TextureToggleButtonState state) const {
    return m_StateTextures[static_cast<int>(state)].modulationColor;
}

void TextureToggleButton::SetStateModulation(TextureToggleButtonState state, const Color& color) {
    m_StateTextures[static_cast<int>(state)].modulationColor = color;

    std::string propName;
    switch (state) {
        case TextureToggleButtonState::Normal: propName = "normalModulation"; break;
        case TextureToggleButtonState::Hover: propName = "hoverModulation"; break;
        case TextureToggleButtonState::Pressed: propName = "pressedModulation"; break;
        case TextureToggleButtonState::Toggled: propName = "toggledModulation"; break;
        case TextureToggleButtonState::ToggledHover: propName = "toggledHoverModulation"; break;
        case TextureToggleButtonState::ToggledPressed: propName = "toggledPressedModulation"; break;
        case TextureToggleButtonState::Disabled: propName = "disabledModulation"; break;
        default: return;
    }
    SetPropertyValue<Color>(propName, color);
}

// ===== Per-State Sounds =====

const TextureToggleButtonStateSound& TextureToggleButton::GetStateSound(TextureToggleButtonState state) const {
    return m_StateSounds[static_cast<int>(state)];
}

void TextureToggleButton::SetStateSound(TextureToggleButtonState state, const TextureToggleButtonStateSound& sound) {
    m_StateSounds[static_cast<int>(state)] = sound;
}

void TextureToggleButton::SetStateSoundPath(TextureToggleButtonState state, const std::string& path) {
    int idx = static_cast<int>(state);
    m_StateSounds[idx].audioPath = path;

    std::string propName;
    switch (state) {
        case TextureToggleButtonState::Normal: propName = "normalSoundPath"; break;
        case TextureToggleButtonState::Hover: propName = "hoverSoundPath"; break;
        case TextureToggleButtonState::Pressed: propName = "pressedSoundPath"; break;
        case TextureToggleButtonState::Toggled: propName = "toggledSoundPath"; break;
        case TextureToggleButtonState::ToggledHover: propName = "toggledHoverSoundPath"; break;
        case TextureToggleButtonState::ToggledPressed: propName = "toggledPressedSoundPath"; break;
        case TextureToggleButtonState::Disabled: propName = "disabledSoundPath"; break;
        default: return;
    }
    SetPropertyValue<std::string>(propName, path);
}

// ===== Per-State Tweens =====

const TextureToggleButtonStateTween& TextureToggleButton::GetStateTween(TextureToggleButtonState state) const {
    return m_StateTweens[static_cast<int>(state)];
}

void TextureToggleButton::SetStateTween(TextureToggleButtonState state, const TextureToggleButtonStateTween& tween) {
    m_StateTweens[static_cast<int>(state)] = tween;
}

// ===== Private Methods =====

void TextureToggleButton::UploadTextureToGPU(RenderContext& ctx, asset::AssetRef<asset::ImageAsset>& asset, TextureHandle& handle) {
    if (!asset || !asset->GetData()) {
        return;
    }

    if (handle.isValid()) {
        return; // Already uploaded
    }

    auto device = ctx.getDevice();
    if (!device) {
        return;
    }

    handle = lupine::CreateTexture2DFromImage(device, *asset, TextureFormat::RGBA8_UNORM);
}

void TextureToggleButton::UpdateMouseInteraction(float) {
    if (!m_ButtonEnabled) {
        if (m_CurrentState != TextureToggleButtonState::Disabled) {
            TransitionToState(TextureToggleButtonState::Disabled);
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
        if (m_CurrentState != TextureToggleButtonState::Pressed &&
            m_CurrentState != TextureToggleButtonState::ToggledPressed) {
            m_PressedInside = true;
        }
    }

    // Check if we were in a pressed state and now releasing while over
    bool wasInPressedState = (m_CurrentState == TextureToggleButtonState::Pressed ||
                              m_CurrentState == TextureToggleButtonState::ToggledPressed);

    // Toggle when releasing from pressed state while still over the button
    if (wasInPressedState && !leftPressed && isOver && m_PressedInside) {
        SetToggled(!m_IsToggled);
        m_PressedInside = false;
    }

    // Reset pressed tracking if mouse released or moved off
    if (!leftPressed || !isOver) {
        m_PressedInside = false;
    }

    // Determine new state based on toggle state and mouse interaction
    TextureToggleButtonState newState = m_CurrentState;

    if (m_IsToggled) {
        // Toggled states
        if (isOver && leftPressed) {
            newState = TextureToggleButtonState::ToggledPressed;
        } else if (isOver) {
            newState = TextureToggleButtonState::ToggledHover;
        } else {
            newState = TextureToggleButtonState::Toggled;
        }
    } else {
        // Normal states
        if (isOver && leftPressed) {
            newState = TextureToggleButtonState::Pressed;
        } else if (isOver) {
            newState = TextureToggleButtonState::Hover;
        } else {
            newState = TextureToggleButtonState::Normal;
        }
    }

    // Transition to new state if changed
    if (newState != m_CurrentState) {
        TransitionToState(newState);
    }
}

void TextureToggleButton::TransitionToState(TextureToggleButtonState newState) {
    if (m_CurrentState == newState) {
        return;
    }

    m_PreviousState = m_CurrentState;
    m_CurrentState = newState;

    // Play state sound
    PlayStateSound(newState);

    // Start tween animation
    const TextureToggleButtonStateTween& tween = m_StateTweens[static_cast<int>(newState)];
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

void TextureToggleButton::PlayStateSound(TextureToggleButtonState state) {
    const auto& sound = m_StateSounds[static_cast<int>(state)];

    if (sound.audioPath.empty()) {
        return;
    }

    // Load audio asset if not already loaded
    if (!sound.audioAsset) {
        auto& mutableSound = const_cast<TextureToggleButtonStateSound&>(sound);
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

void TextureToggleButton::SyncFromProperties() {
    // Sync themed per-state modulation + tween data
    LoadThemedStateData();

    // Sync button enabled state
    m_ButtonEnabled = GetPropertyValue<bool>("buttonEnabled");
}

void TextureToggleButton::LoadThemedStateData() {
    struct StateMap { TextureToggleButtonState state; const char* prefix; };
    static const StateMap kStates[] = {
        { TextureToggleButtonState::Normal,         "normal" },
        { TextureToggleButtonState::Hover,          "hover" },
        { TextureToggleButtonState::Pressed,        "pressed" },
        { TextureToggleButtonState::Toggled,        "toggled" },
        { TextureToggleButtonState::ToggledHover,   "toggledHover" },
        { TextureToggleButtonState::ToggledPressed, "toggledPressed" },
        { TextureToggleButtonState::Disabled,       "disabled" },
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

const std::vector<UIControl::ThemeBinding>& TextureToggleButton::GetThemeBindings() const {
    static const std::vector<ThemeBinding> kBindings = []() {
        std::vector<ThemeBinding> b;
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

bool TextureToggleButton::IsMouseOver(const Vec2& mousePos) const {
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

    return true;
}

void TextureToggleButton::RenderNineSlice(RenderContext& ctx, const Vec2& position, const Vec2& size,
                                          float rotation, TextureHandle texture, int textureWidth, int textureHeight,
                                          const Color& modulate) {
    if (!texture.isValid() || textureWidth == 0 || textureHeight == 0) {
        return;
    }

    UINineSlice nineSlice;
    nineSlice.marginLeft = GetMarginLeft();
    nineSlice.marginTop = GetMarginTop();
    nineSlice.marginRight = GetMarginRight();
    nineSlice.marginBottom = GetMarginBottom();
    nineSlice.axisHorizontal = GetNineSliceAxisHorizontal();
    nineSlice.axisVertical = GetNineSliceAxisVertical();
    nineSlice.drawCenter = GetNineSliceDrawCenter();

    DrawUIImage(ctx, position, size, rotation, texture, textureWidth, textureHeight,
                modulate, Vec4(0.0f, 0.0f, 0.0f, 0.0f), UIImageStretchMode::NineSlice, nineSlice);
}

void TextureToggleButton::RenderButton(RenderContext& ctx, const Vec2& position, const Vec2& size, float rotation) {
    // Get current state texture and modulation
    const auto& stateTexture = m_StateTextures[static_cast<int>(m_CurrentState)];

    // Apply tween transformations
    Vec2 tweenedPosition = position + m_CurrentPositionOffset;
    Vec2 tweenedSize = size + m_CurrentScaleOffset;
    float tweenedRotation = rotation + m_CurrentRotationOffset;

    // Use state texture if available, otherwise fall back to old toggle textures
    TextureHandle currentTexture;
    asset::AssetRef<asset::ImageAsset> currentAsset;

    if (!stateTexture.texturePath.empty() && stateTexture.textureAsset) {
        currentAsset = stateTexture.textureAsset;
        // Upload texture to GPU if needed
        if (!stateTexture.textureHandle.isValid() && currentAsset.IsValid() && currentAsset->IsLoaded()) {
            auto device = ctx.getDevice();
            if (device) {
                const_cast<TextureToggleButtonStateTexture&>(stateTexture).textureHandle = lupine::CreateTexture2DFromImage(device, *currentAsset, TextureFormat::RGBA8_UNORM);
            }
        }
        currentTexture = stateTexture.textureHandle;
    }

    if (!currentTexture.isValid()) {
        return; // No texture to render
    }

    // Apply state modulation color
    Color modulate = stateTexture.modulationColor;

    // Render with nine-slice if enabled
    if (GetUseNineSlice() && currentAsset.IsValid()) {
        int textureWidth = currentAsset->GetWidth();
        int textureHeight = currentAsset->GetHeight();
        RenderNineSlice(ctx, tweenedPosition, tweenedSize, tweenedRotation, currentTexture, textureWidth, textureHeight, modulate);
    } else {
        // Simple stretch rendering
        if (std::abs(tweenedRotation) > 0.0001f) {
            // Rotated: position is center-pivot, use as-is
            ctx.drawRoundedRect(
                tweenedPosition, tweenedSize,
                Vec4(0.0f, 0.0f, 0.0f, 0.0f),
                modulate, currentTexture, tweenedRotation,
                Vec2(0.0f, 0.0f), Vec2(1.0f, 1.0f), 0
            );
        } else {
            // Non-rotated: convert center-pivot to top-left
            Vec2 topLeft = Vec2(tweenedPosition.x - tweenedSize.x * 0.5f, tweenedPosition.y - tweenedSize.y * 0.5f);
            ctx.drawRoundedRect(
                topLeft, tweenedSize,
                Vec4(0.0f, 0.0f, 0.0f, 0.0f),
                modulate, currentTexture, 0.0f,
                Vec2(0.0f, 0.0f), Vec2(1.0f, 1.0f), 0
            );
        }
    }
}

// ===== IRenderableComponent Implementation =====

void TextureToggleButton::buildDrawCommands(RenderContext& ctx) {
    if (!GetOwner()) {
        return;
    }

    core::Node2D* node2D = dynamic_cast<core::Node2D*>(GetOwner());
    if (!node2D) {
        return;
    }

    const math::Rect __rect = GetResolvedRect();
    Vec2 position = __rect.GetCenter();
    Vec2 size = __rect.size;
    float rotation = node2D->GetGlobalRotation();

    RenderButton(ctx, position, size, rotation);
}

AABB TextureToggleButton::getWorldBounds() const {
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

RenderLayer TextureToggleButton::getRenderLayer() const {
    int layer = GetLayer();
    int sortingOrder = GetSortingOrder();
    return static_cast<RenderLayer>(layer * 1000 + sortingOrder);
}

SpatialType TextureToggleButton::getSpatialType() const {
    return GetUISpatialType();
}

OBB TextureToggleButton::getOrientedBounds() const {
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

bool TextureToggleButton::IntersectRay(const Ray& ray, float& outDistance) const {
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

} // namespace components
} // namespace lupine

