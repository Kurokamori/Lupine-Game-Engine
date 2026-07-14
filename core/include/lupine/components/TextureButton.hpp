#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include "lupine/components/UIControl.hpp"
#include "lupine/components/UIImageDraw.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/rendering/ResourceHandles.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/asset/ImageAsset.hpp"
#include "lupine/asset/FontAsset.hpp"
#include "lupine/audio/AudioManager.hpp"
#include "lupine/asset/AudioAsset.hpp"
#include <string>
#include <memory>
#include <unordered_map>
#include <functional>

namespace lupine {
namespace components {

/**
 * TextureButton mouse states
 */
enum class TextureButtonState {
    Normal = 0,
    Hover = 1,
    Pressed = 2,
    Disabled = 3,
    COUNT = 4
};

/**
 * TextureButton style mode
 */
enum class TextureButtonStyleMode {
    Automatic = 0,  // Single texture with per-state color modulation
    Manual = 1      // Full texture per state
};

/**
 * Stretch mode for texture rendering
 */
enum class TextureButtonStretchMode {
    Stretch = 0,        // Stretch texture to fill button
    KeepCentered = 1,   // Keep texture at original size, centered
    NineSlice = 2       // Nine-slice scaling
};

/**
 * Click mask mode for determining clickable region
 */
enum class ClickMaskMode {
    Rect = 0,           // Rectangular click area
    AlphaThreshold = 1  // Use texture alpha for click detection
};

/**
 * Mouse button for interaction
 */
enum class MouseButtonType {
    Left = 0,
    Right = 1,
    Middle = 2
};

/**
 * Per-state texture data for manual mode
 */
struct TextureButtonStateTexture {
    std::string texturePath;
    asset::AssetRef<asset::ImageAsset> textureAsset;
    TextureHandle textureHandle;
    math::Color modulationColor{1.0f, 1.0f, 1.0f, 1.0f};
};

/**
 * Per-state sound data
 */
struct TextureButtonStateSound {
    std::string audioPath;
    asset::AssetRef<asset::AudioAsset> audioAsset;
    std::string busName{"SFX"};
    float volume{1.0f};
};

/**
 * Per-state tween data
 */
struct TextureButtonStateTween {
    bool enabled{false};
    math::Vec2 scaleOffset{0.0f, 0.0f};      // Scale change
    float rotationOffset{0.0f};               // Rotation change (degrees)
    math::Vec2 positionOffset{0.0f, 0.0f};   // Position offset
    float duration{0.2f};                     // Tween duration
};

/**
 * TextureButton Component
 *
 * A comprehensive UI button that uses textures for its various states.
 * Features:
 * - Texture-based button states (Normal, Hover, Pressed, Disabled)
 * - Two style modes: Automatic (single texture + modulation) and Manual (per-state textures)
 * - Alpha masking for precise click detection
 * - Stretch modes (Stretch, Keep Centered, Nine Slice)
 * - Toggle button support
 * - Per-state sounds and tweens
 * - Text overlay with font support
 * - Mouse button selection
 *
 * Events: on_clicked(), on_pressed(), on_released(), on_toggled(is_checked:bool)
 */
class TextureButton : public UIControl, public IRenderableComponent {
public:
    TextureButton();
    explicit TextureButton(const std::string& name);
    virtual ~TextureButton();

    // ISerializable interface
    std::string GetTypeName() const override { return "TextureButton"; }
    void DefineProperties() override;
    void DefineSignals() override;

    // Theme: text font colour + font size.
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

    // Asset hot-reload support
    bool OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) override;

    // ===== Size Properties =====
    // Width/height/size are provided by the UIControl base class.

    math::Vec2 CalculateButtonSize() const;
    math::Vec2 CalculateTextSize() const;

    // ===== Layer Properties =====
    
    int GetLayer() const;
    void SetLayer(int layer);

    int GetSortingOrder() const;
    void SetSortingOrder(int order);

    // ===== UI Space =====
    // GetUseUISpace/SetUseUISpace are provided by the UIControl base class.

    // ===== Button State =====

    TextureButtonState GetCurrentState() const { return m_CurrentState; }
    void SetEnabled(bool enabled);
    bool IsButtonEnabled() const { return m_ButtonEnabled; }

    // ===== Style Mode =====

    TextureButtonStyleMode GetStyleMode() const { return m_StyleMode; }
    void SetStyleMode(TextureButtonStyleMode mode);

    // ===== Stretch Mode =====

    TextureButtonStretchMode GetStretchMode() const;
    void SetStretchMode(TextureButtonStretchMode mode);

    bool GetUseStretch() const;
    void SetUseStretch(bool useStretch);

    // ===== Nine-slice (used when StretchMode == NineSlice) =====

    // Per-side margins in SOURCE texture pixels: (left, top, right, bottom).
    math::Vec4 GetNineSliceMargins() const;
    void SetNineSliceMargins(const math::Vec4& margins);

    UINineSliceAxisMode GetNineSliceAxisHorizontal() const;
    void SetNineSliceAxisHorizontal(UINineSliceAxisMode mode);

    UINineSliceAxisMode GetNineSliceAxisVertical() const;
    void SetNineSliceAxisVertical(UINineSliceAxisMode mode);

    bool GetNineSliceDrawCenter() const;
    void SetNineSliceDrawCenter(bool drawCenter);

    // ===== Toggle Mode =====

    bool GetIsToggle() const;
    void SetIsToggle(bool isToggle);

    bool GetIsChecked() const;
    void SetIsChecked(bool isChecked);

    // ===== Click Mask =====

    ClickMaskMode GetClickMaskMode() const;
    void SetClickMaskMode(ClickMaskMode mode);

    float GetAlphaThreshold() const;
    void SetAlphaThreshold(float threshold);

    // ===== Mouse Button =====

    MouseButtonType GetMouseButton() const;
    void SetMouseButton(MouseButtonType button);

    // ===== Texture Properties (Automatic Mode) =====

    std::string GetTexturePath() const;
    void SetTexturePath(const std::string& path);

    // ===== Per-State Textures (Manual Mode) =====

    std::string GetStateTexturePath(TextureButtonState state) const;
    void SetStateTexturePath(TextureButtonState state, const std::string& path);

    // ===== Per-State Modulation =====

    const math::Color& GetStateModulation(TextureButtonState state) const;
    void SetStateModulation(TextureButtonState state, const math::Color& color);

    // ===== Per-State Sounds =====

    const TextureButtonStateSound& GetStateSound(TextureButtonState state) const;
    void SetStateSound(TextureButtonState state, const TextureButtonStateSound& sound);
    void SetStateSoundPath(TextureButtonState state, const std::string& path);

    // ===== Per-State Tweens =====

    const TextureButtonStateTween& GetStateTween(TextureButtonState state) const;
    void SetStateTween(TextureButtonState state, const TextureButtonStateTween& tween);

    // ===== Text Properties =====

    std::string GetText() const;
    void SetText(const std::string& text);

    std::string GetFontPath() const;
    void SetFontPath(const std::string& path);

    float GetFontSize() const;
    void SetFontSize(float size);

    math::Color GetFontColor() const;
    void SetFontColor(const math::Color& color);

    // ===== Per-State Callbacks =====

    using StateCallback = std::function<void()>;
    using ToggleCallback = std::function<void(bool)>;

    void SetStateCallback(TextureButtonState state, StateCallback callback);
    void ClearStateCallback(TextureButtonState state);

    void SetOnClickedCallback(StateCallback callback);
    void SetOnPressedCallback(StateCallback callback);
    void SetOnReleasedCallback(StateCallback callback);
    void SetOnToggledCallback(ToggleCallback callback);

    // ===== IRenderableComponent Implementation =====

    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override;

    // Override to provide oriented bounding box that rotates with button
    math::OBB getOrientedBounds() const override;

    // Override for precise ray intersection on rotated buttons
    bool IntersectRay(const math::Ray& ray, float& outDistance) const override;

private:
    // Sync internal state from properties
    void SyncFromProperties();

    // Load per-state modulation + tween data from the active theme (or per-instance overrides)
    void LoadThemedStateData();

    // Update mouse interaction
    void UpdateMouseInteraction(float deltaTime);

    // Check if mouse is over button (with alpha masking support)
    bool IsMouseOver(const math::Vec2& mousePos) const;

    // Check alpha at specific UV coordinate
    bool CheckAlphaAtUV(const math::Vec2& uv) const;

    // Transition to new state
    void TransitionToState(TextureButtonState newState);

    // Play state sound
    void PlayStateSound(TextureButtonState state);

    // Invoke state callback
    void InvokeStateCallback(TextureButtonState state);

    // Load texture for a specific state
    void LoadStateTexture(TextureButtonState state, const std::string& path);

    // Upload texture to GPU
    void UploadTextureToGPU(TextureButtonState state, RenderContext& ctx);

    // Render the button texture
    void RenderTexture(RenderContext& ctx, const math::Vec2& position, const math::Vec2& size, float rotation);

    // Regenerate the text mesh at the given position with rotation
    void RegenerateTextMesh(RenderContext& ctx, const math::Vec2& position, float rotation);

    // Render the button text
    void RenderText(RenderContext& ctx, const math::Vec2& position, const math::Vec2& size, float baseRotation);

    // Get effective texture for current state
    TextureHandle GetEffectiveTexture() const;

    // Get effective color for current state
    math::Color GetEffectiveColor() const;

    // Current state
    TextureButtonState m_CurrentState{TextureButtonState::Normal};
    TextureButtonState m_PreviousState{TextureButtonState::Normal};
    bool m_ButtonEnabled{true};

    // Style mode
    TextureButtonStyleMode m_StyleMode{TextureButtonStyleMode::Automatic};

    // Automatic mode texture (shared across all states)
    std::string m_AutomaticTexturePath;
    asset::AssetRef<asset::ImageAsset> m_AutomaticTextureAsset;
    TextureHandle m_AutomaticTextureHandle;
    std::string m_CurrentAutomaticTexturePath;

    // Per-state data
    TextureButtonStateTexture m_StateTextures[static_cast<int>(TextureButtonState::COUNT)];
    TextureButtonStateSound m_StateSounds[static_cast<int>(TextureButtonState::COUNT)];
    TextureButtonStateTween m_StateTweens[static_cast<int>(TextureButtonState::COUNT)];
    std::unordered_map<TextureButtonState, StateCallback> m_StateCallbacks;

    // Event callbacks
    StateCallback m_OnClickedCallback;
    StateCallback m_OnPressedCallback;
    StateCallback m_OnReleasedCallback;
    ToggleCallback m_OnToggledCallback;

    // Tween state
    math::Vec2 m_CurrentScaleOffset{0.0f, 0.0f};
    float m_CurrentRotationOffset{0.0f};
    math::Vec2 m_CurrentPositionOffset{0.0f, 0.0f};
    math::Vec2 m_TweenStartScaleOffset{0.0f, 0.0f};
    float m_TweenStartRotationOffset{0.0f};
    math::Vec2 m_TweenStartPositionOffset{0.0f, 0.0f};
    float m_TweenProgress{0.0f};
    bool m_IsTweening{false};

    // Font rendering
    asset::AssetRef<asset::FontAsset> m_FontAsset;
    FontHandle m_FontHandle;
    std::string m_CurrentFontPath;
    float m_CurrentFontSize{16.0f};

    // Cached text mesh
    MeshHandle m_TextMesh;
    std::string m_CachedText;
    float m_CachedFontSize{0.0f};
    math::Vec2 m_CachedPosition{0.0f, 0.0f};
    math::Color m_CachedFontColor{1.0f, 1.0f, 1.0f, 1.0f};
    float m_CachedRotation{0.0f};

    // Flags
    bool m_MeshNeedsRegeneration{true};
    bool m_TextMeshNeedsRegeneration{true};
    bool m_MouseWasOver{false};

    // True while the button holds the press that began on it. A press belongs to the
    // control the button-down landed on, so a button revealed under an already-held
    // mouse cannot claim that same click.
    bool m_PressArmed{false};
    bool m_WasPressed{false};
};

} // namespace components
} // namespace lupine

