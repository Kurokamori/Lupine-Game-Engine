#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include "lupine/components/UIControl.hpp"
#include "lupine/components/UIImageDraw.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/rendering/ResourceHandles.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/asset/ImageAsset.hpp"
#include "lupine/audio/AudioManager.hpp"
#include "lupine/asset/AudioAsset.hpp"
#include <string>
#include <functional>
#include <unordered_map>

namespace lupine {
namespace components {

/**
 * TextureToggleButton mouse states
 */
enum class TextureToggleButtonState {
    Normal = 0,
    Hover = 1,
    Pressed = 2,
    Toggled = 3,
    ToggledHover = 4,
    ToggledPressed = 5,
    Disabled = 6,
    COUNT = 7
};

/**
 * Per-state texture data
 */
struct TextureToggleButtonStateTexture {
    std::string texturePath;
    asset::AssetRef<asset::ImageAsset> textureAsset;
    TextureHandle textureHandle;
    math::Color modulationColor{1.0f, 1.0f, 1.0f, 1.0f};
};

/**
 * Per-state sound data
 */
struct TextureToggleButtonStateSound {
    std::string audioPath;
    asset::AssetRef<asset::AudioAsset> audioAsset;
    std::string busName{"SFX"};
    float volume{1.0f};
};

/**
 * Per-state tween data
 */
struct TextureToggleButtonStateTween {
    bool enabled{false};
    math::Vec2 scaleOffset{0.0f, 0.0f};      // Scale change
    float rotationOffset{0.0f};               // Rotation change (degrees)
    math::Vec2 positionOffset{0.0f, 0.0f};   // Position offset
    float duration{0.2f};                     // Tween duration
};

/**
 * TextureToggleButton Component
 *
 * A comprehensive UI toggle button that uses textures for its various states.
 * Features:
 * - Texture-based button states (Normal, Hover, Pressed, Toggled, ToggledHover, ToggledPressed, Disabled)
 * - Toggle state (on/off)
 * - Per-state textures and modulation colors
 * - Per-state sounds and tweens
 * - Nine-slice support
 * - Layer ordering
 * - UI space selection
 * - Toggle callback
 *
 * Events: on_toggled(is_checked:bool)
 */
class TextureToggleButton : public UIControl, public IRenderableComponent {
public:
    TextureToggleButton();
    explicit TextureToggleButton(const std::string& name);
    virtual ~TextureToggleButton();

    // ISerializable interface
    std::string GetTypeName() const override { return "TextureToggleButton"; }
    void DefineProperties() override;
    const std::vector<ThemeBinding>& GetThemeBindings() const override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnInput(float deltaTime) override;
    void OnUpdate(float deltaTime) override;

    // Editor gizmo hooks
    // Pointer arbitration: opaque to the cursor, hit-tests with its own bounds so
    // overlapping controls don't all react to one click.
    bool ConsumesPointerInput() const override { return true; }
    bool ContainsCanvasPoint(const math::Vec2& point) const override { return IsMouseOver(point); }

    bool OnGizmoScale(float scaleDelta, int axis, bool is3D) override;

    // ===== Size Properties =====
    // Width/height/size are provided by the UIControl base class.

    // ===== Layer Properties =====
    
    int GetLayer() const;
    void SetLayer(int layer);

    int GetSortingOrder() const;
    void SetSortingOrder(int order);

    // ===== UI Space =====
    // GetUseUISpace/SetUseUISpace are provided by the UIControl base class.

    // ===== Button State =====

    void SetEnabled(bool enabled);
    bool IsButtonEnabled() const { return m_ButtonEnabled; }

    // ===== Toggle State =====

    bool IsToggled() const;
    void SetToggled(bool toggled);

    // ===== Per-State Textures =====

    const TextureToggleButtonStateTexture& GetStateTexture(TextureToggleButtonState state) const;
    void SetStateTexture(TextureToggleButtonState state, const TextureToggleButtonStateTexture& texture);
    void SetStateTexturePath(TextureToggleButtonState state, const std::string& path);

    const math::Color& GetStateModulation(TextureToggleButtonState state) const;
    void SetStateModulation(TextureToggleButtonState state, const math::Color& color);

    // ===== Per-State Sounds =====

    const TextureToggleButtonStateSound& GetStateSound(TextureToggleButtonState state) const;
    void SetStateSound(TextureToggleButtonState state, const TextureToggleButtonStateSound& sound);
    void SetStateSoundPath(TextureToggleButtonState state, const std::string& path);

    // ===== Per-State Tweens =====

    const TextureToggleButtonStateTween& GetStateTween(TextureToggleButtonState state) const;
    void SetStateTween(TextureToggleButtonState state, const TextureToggleButtonStateTween& tween);

    // ===== Nine-Slice Properties =====
    
    bool GetUseNineSlice() const;
    void SetUseNineSlice(bool useNineSlice);

    float GetMarginLeft() const;
    void SetMarginLeft(float margin);

    float GetMarginRight() const;
    void SetMarginRight(float margin);

    float GetMarginTop() const;
    void SetMarginTop(float margin);

    float GetMarginBottom() const;
    void SetMarginBottom(float margin);

    UINineSliceAxisMode GetNineSliceAxisHorizontal() const;
    void SetNineSliceAxisHorizontal(UINineSliceAxisMode mode);

    UINineSliceAxisMode GetNineSliceAxisVertical() const;
    void SetNineSliceAxisVertical(UINineSliceAxisMode mode);

    bool GetNineSliceDrawCenter() const;
    void SetNineSliceDrawCenter(bool drawCenter);

    // ===== Toggle Callback =====

    using ToggleCallback = std::function<void(bool)>;
    void SetOnToggledCallback(ToggleCallback callback);

    // ===== IRenderableComponent Implementation =====

    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override;

    math::OBB getOrientedBounds() const override;
    bool IntersectRay(const math::Ray& ray, float& outDistance) const override;

private:
    // Sync internal state from properties
    void SyncFromProperties();

    // Resolve per-state modulation + tween from the theme (with own-override fallback)
    void LoadThemedStateData();

    // Update mouse interaction
    void UpdateMouseInteraction(float deltaTime);

    // Check if mouse is over button
    bool IsMouseOver(const math::Vec2& mousePos) const;

    // Transition to new state
    void TransitionToState(TextureToggleButtonState newState);

    // Play state sound
    void PlayStateSound(TextureToggleButtonState state);

    // Upload textures to GPU
    void UploadTextureToGPU(RenderContext& ctx, asset::AssetRef<asset::ImageAsset>& asset, TextureHandle& handle);

    // Render the button
    void RenderButton(RenderContext& ctx, const math::Vec2& position, const math::Vec2& size, float rotation);

    // Render with nine-slice
    void RenderNineSlice(RenderContext& ctx, const math::Vec2& position, const math::Vec2& size,
                         float rotation, TextureHandle texture, int textureWidth, int textureHeight,
                         const math::Color& modulate);

    // Button state
    bool m_ButtonEnabled{true};
    bool m_IsToggled{false};
    bool m_PressedInside{false};  // Track if press started inside button
    TextureToggleButtonState m_CurrentState{TextureToggleButtonState::Normal};
    TextureToggleButtonState m_PreviousState{TextureToggleButtonState::Normal};

    // Per-state data
    TextureToggleButtonStateTexture m_StateTextures[static_cast<int>(TextureToggleButtonState::COUNT)];
    TextureToggleButtonStateSound m_StateSounds[static_cast<int>(TextureToggleButtonState::COUNT)];
    TextureToggleButtonStateTween m_StateTweens[static_cast<int>(TextureToggleButtonState::COUNT)];

    // Tween animation state
    bool m_IsTweening{false};
    float m_TweenProgress{0.0f};
    math::Vec2 m_TweenStartScaleOffset{0.0f, 0.0f};
    float m_TweenStartRotationOffset{0.0f};
    math::Vec2 m_TweenStartPositionOffset{0.0f, 0.0f};
    math::Vec2 m_CurrentScaleOffset{0.0f, 0.0f};
    float m_CurrentRotationOffset{0.0f};
    math::Vec2 m_CurrentPositionOffset{0.0f, 0.0f};

    // Toggle callback
    ToggleCallback m_OnToggledCallback;
};

} // namespace components
} // namespace lupine

