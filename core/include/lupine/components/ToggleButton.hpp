#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include "lupine/core/LinkedProperty.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/rendering/ResourceHandles.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/components/StyleBox.hpp"
#include "lupine/components/UIControl.hpp"
#include "lupine/audio/AudioManager.hpp"
#include "lupine/asset/AudioAsset.hpp"
#include "lupine/asset/FontAsset.hpp"
#include <string>
#include <memory>
#include <unordered_map>
#include <functional>

namespace lupine {
namespace components {

/**
 * ToggleButton mouse states
 */
enum class ToggleButtonState {
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
 * ToggleButton style mode
 */
enum class ToggleButtonStyleMode {
    Automatic = 0,  // Single style with per-state color modulation
    Manual = 1      // Full StyleBox per state
};

/**
 * Per-state style data
 */
struct ToggleButtonStateStyle {
    math::Color modulationColor{1.0f, 1.0f, 1.0f, 1.0f};  // Color modulation
    std::shared_ptr<StyleBox> styleBox{nullptr};            // For manual mode
};

/**
 * Per-state sound data
 */
struct ToggleButtonStateSound {
    std::string audioPath;
    asset::AssetRef<asset::AudioAsset> audioAsset;
    std::string busName{"SFX"};
    float volume{1.0f};
};

/**
 * Per-state tween data
 */
struct ToggleButtonStateTween {
    bool enabled{false};
    math::Vec2 scaleOffset{0.0f, 0.0f};      // Scale change
    float rotationOffset{0.0f};               // Rotation change (degrees)
    math::Vec2 positionOffset{0.0f, 0.0f};   // Position offset
    float duration{0.2f};                     // Tween duration
};

/**
 * ToggleButton Component
 *
 * A comprehensive UI toggle button with:
 * - Toggle state (on/off, yes/no, checked/unchecked)
 * - Panel-like backgrounds with full StyleBox support
 * - Text labels with font properties
 * - Mouse state management (Normal, Hover, Pressed, Toggled, ToggledHover, ToggledPressed, Disabled)
 * - Two style modes: Automatic (color modulation) and Manual (full StyleBox per state)
 * - Per-state sounds
 * - Per-state tweens (scale, rotation, position)
 * - Per-state callbacks
 * - Toggle callback when state changes
 */
class ToggleButton : public UIControl, public IRenderableComponent {
public:
    ToggleButton();
    explicit ToggleButton(const std::string& name);
    virtual ~ToggleButton();

    // ISerializable interface
    std::string GetTypeName() const override { return "ToggleButton"; }
    void DefineProperties() override;
    const std::vector<ThemeBinding>& GetThemeBindings() const override;
    void DefineSignals() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnInput(float deltaTime) override;
    void OnUpdate(float deltaTime) override;
    void OnRender() override;

    // Editor gizmo hooks
    // Pointer arbitration: opaque to the cursor, hit-tests with its own bounds so
    // overlapping controls don't all react to one click.
    bool ConsumesPointerInput() const override { return true; }
    bool ContainsCanvasPoint(const math::Vec2& point) const override { return IsMouseOver(point); }

    bool OnGizmoScale(float scaleDelta, int axis, bool is3D) override;

    // Asset hot-reload support
    bool OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) override;

    // ===== Size Properties =====
    // The width/height properties are registered by the UIControl base; the
    // setters keep the toggle-button mesh-regeneration side effect.
    float GetWidth() const override;
    void SetWidth(float width);

    float GetHeight() const override;
    void SetHeight(float height);

    // Content minimum size = estimated text size (used by containers/anchors).
    math::Vec2 GetContentMinSize() const override;

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

    // ===== Style Mode =====

    ToggleButtonStyleMode GetStyleMode() const;
    void SetStyleMode(ToggleButtonStyleMode mode);

    // ===== Toggle State =====

    bool IsToggled() const;
    void SetToggled(bool toggled);

    // ===== Per-State Styles =====

    const ToggleButtonStateStyle& GetStateStyle(ToggleButtonState state) const;
    void SetStateStyle(ToggleButtonState state, const ToggleButtonStateStyle& style);

    const math::Color& GetStateModulation(ToggleButtonState state) const;
    void SetStateModulation(ToggleButtonState state, const math::Color& color);

    // ===== Per-State Sounds =====

    const ToggleButtonStateSound& GetStateSound(ToggleButtonState state) const;
    void SetStateSound(ToggleButtonState state, const ToggleButtonStateSound& sound);
    void SetStateSoundPath(ToggleButtonState state, const std::string& path);

    // ===== Per-State Tweens =====

    const ToggleButtonStateTween& GetStateTween(ToggleButtonState state) const;
    void SetStateTween(ToggleButtonState state, const ToggleButtonStateTween& tween);

    // ===== Per-State Callbacks =====

    using StateCallback = std::function<void()>;
    void SetStateCallback(ToggleButtonState state, StateCallback callback);
    void ClearStateCallback(ToggleButtonState state);

    // ===== Text Properties =====

    std::string GetText() const;
    void SetText(const std::string& text);

    std::string GetFontPath() const;
    void SetFontPath(const std::string& path);

    float GetFontSize() const;
    void SetFontSize(float size);

    math::Color GetFontColor() const;
    void SetFontColor(const math::Color& color);

    // ===== StyleBox Properties =====

    math::Color GetBackgroundColor() const;
    void SetBackgroundColor(const math::Color& color);

    float GetOpacity() const;
    void SetOpacity(float opacity);

    bool GetBorderEnabled() const;
    void SetBorderEnabled(bool enabled);

    math::Color GetBorderColor() const;
    void SetBorderColor(const math::Color& color);

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

    void LoadThemedStateData();

    // Update mouse interaction
    void UpdateMouseInteraction(float deltaTime);

    // Check if mouse is over button
    bool IsMouseOver(const math::Vec2& mousePos) const;

    // Transition to new state
    void TransitionToState(ToggleButtonState newState);

    // Play state sound
    void PlayStateSound(ToggleButtonState state);

    // Invoke state callback
    void InvokeStateCallback(ToggleButtonState state);

    // Render the button background
    void RenderBackground(RenderContext& ctx, const math::Vec2& position, const math::Vec2& size, float rotation);

    // Regenerate the text mesh
    void RegenerateTextMesh(RenderContext& ctx, const math::Vec2& position, const math::Vec2& size);

    // Render the button text
    void RenderText(RenderContext& ctx, const math::Vec2& position, const math::Vec2& size);

    // Calculate text size
    math::Vec2 CalculateTextSize() const;

    // Font color with per-state modulation and opacity applied (matches the background).
    math::Color GetEffectiveFontColor() const;

    // Button state
    bool m_ButtonEnabled{true};
    bool m_IsToggled{false};
    bool m_PressedInside{false};  // Track if press started inside button
    ToggleButtonState m_CurrentState{ToggleButtonState::Normal};
    ToggleButtonState m_PreviousState{ToggleButtonState::Normal};

    // Style mode
    ToggleButtonStyleMode m_StyleMode{ToggleButtonStyleMode::Automatic};

    // Base style (for automatic mode)
    std::shared_ptr<StyleBox> m_BaseStyleBox;
    core::LinkedProperty4 m_CornerRadius;
    core::LinkedProperty4 m_BorderWidth;

    // Per-state data
    ToggleButtonStateStyle m_StateStyles[static_cast<int>(ToggleButtonState::COUNT)];
    ToggleButtonStateSound m_StateSounds[static_cast<int>(ToggleButtonState::COUNT)];
    ToggleButtonStateTween m_StateTweens[static_cast<int>(ToggleButtonState::COUNT)];
    std::unordered_map<ToggleButtonState, StateCallback> m_StateCallbacks;

    // Tween animation state
    bool m_IsTweening{false};
    float m_TweenProgress{0.0f};
    math::Vec2 m_TweenStartScaleOffset{0.0f, 0.0f};
    float m_TweenStartRotationOffset{0.0f};
    math::Vec2 m_TweenStartPositionOffset{0.0f, 0.0f};
    math::Vec2 m_CurrentScaleOffset{0.0f, 0.0f};
    float m_CurrentRotationOffset{0.0f};
    math::Vec2 m_CurrentPositionOffset{0.0f, 0.0f};

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

    // Flags
    bool m_MeshNeedsRegeneration{true};
    bool m_TextMeshNeedsRegeneration{true};

    // Toggle callback
    ToggleCallback m_OnToggledCallback;
};

} // namespace components
} // namespace lupine

