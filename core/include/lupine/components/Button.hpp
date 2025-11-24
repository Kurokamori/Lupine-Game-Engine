#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include "lupine/core/LinkedProperty.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/rendering/ResourceHandles.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/components/StyleBox.hpp"
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
 * Button mouse states
 */
enum class ButtonState {
    Normal = 0,
    Hover = 1,
    Pressed = 2,
    Disabled = 3,
    COUNT = 4
};

/**
 * Button style mode
 */
enum class ButtonStyleMode {
    Automatic = 0,  // Single style with per-state color modulation
    Manual = 1      // Full StyleBox per state
};

/**
 * Button scale mode
 */
enum class ButtonScaleMode {
    Fixed = 0,          // Fixed size
    FitToText = 1,      // Scale to fit text with padding
    FitToTextWidth = 2  // Scale width only to fit text
};

/**
 * Per-state style data for automatic mode
 */
struct ButtonStateStyle {
    math::Color modulationColor{1.0f, 1.0f, 1.0f, 1.0f};  // Color modulation
    std::shared_ptr<StyleBox> styleBox{nullptr};            // For manual mode
};

/**
 * Per-state sound data
 */
struct ButtonStateSound {
    std::string audioPath;
    asset::AssetRef<asset::AudioAsset> audioAsset;
    std::string busName{"SFX"};
    float volume{1.0f};
};

/**
 * Per-state tween data
 */
struct ButtonStateTween {
    bool enabled{false};
    math::Vec2 scaleOffset{0.0f, 0.0f};      // Scale change
    float rotationOffset{0.0f};               // Rotation change (degrees)
    math::Vec2 positionOffset{0.0f, 0.0f};   // Position offset
    float duration{0.2f};                     // Tween duration
};

/**
 * Button Component
 *
 * A comprehensive UI button with:
 * - Panel-like backgrounds with full StyleBox support
 * - Text labels with font properties
 * - Mouse state management (Normal, Hover, Pressed, Disabled)
 * - Two style modes: Automatic (color modulation) and Manual (full StyleBox per state)
 * - Per-state sounds
 * - Per-state tweens (scale, rotation, position)
 * - Per-state callbacks
 * - Scale modes (fixed, fit to text)
 * - Text formatting (word wrap, bold, italic)
 */
class Button : public core::Component, public IRenderableComponent {
public:
    Button();
    explicit Button(const std::string& name);
    virtual ~Button();

    // ISerializable interface
    std::string GetTypeName() const override { return "Button"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnInput(float deltaTime) override;
    void OnUpdate(float deltaTime) override;
    void OnRender() override;

    // Editor gizmo hooks
    bool OnGizmoScale(float scaleDelta, int axis, bool is3D) override;

    // ===== Size Properties =====
    
    float GetWidth() const;
    void SetWidth(float width);

    float GetHeight() const;
    void SetHeight(float height);

    // ===== Layer Properties =====
    
    int GetLayer() const;
    void SetLayer(int layer);

    int GetSortingOrder() const;
    void SetSortingOrder(int order);

    // ===== UI Space =====
    
    bool GetUseUISpace() const;
    void SetUseUISpace(bool useUISpace);

    // ===== Button State =====
    
    ButtonState GetCurrentState() const { return m_CurrentState; }
    void SetEnabled(bool enabled);
    bool IsButtonEnabled() const { return m_ButtonEnabled; }

    // ===== Style Mode =====
    
    ButtonStyleMode GetStyleMode() const { return m_StyleMode; }
    void SetStyleMode(ButtonStyleMode mode);

    // ===== Scale Mode =====
    
    ButtonScaleMode GetScaleMode() const { return m_ScaleMode; }
    void SetScaleMode(ButtonScaleMode mode);

    math::Vec2 GetTextPadding() const { return m_TextPadding; }
    void SetTextPadding(const math::Vec2& padding);

    // ===== Base Style (for Automatic mode) =====
    
    std::shared_ptr<StyleBox> GetBaseStyleBox() const { return m_BaseStyleBox; }
    void SetBaseStyleBox(std::shared_ptr<StyleBox> styleBox);

    // Background
    const math::Color& GetBackgroundColor() const;
    void SetBackgroundColor(const math::Color& color);

    float GetOpacity() const;
    void SetOpacity(float opacity);

    // Border width
    bool GetBorderWidthLinked() const;
    void SetBorderWidthLinked(bool linked);
    float GetBorderWidthLeft() const;
    void SetBorderWidthLeft(float width);
    float GetBorderWidthRight() const;
    void SetBorderWidthRight(float width);
    float GetBorderWidthTop() const;
    void SetBorderWidthTop(float width);
    float GetBorderWidthBottom() const;
    void SetBorderWidthBottom(float width);
    bool GetBorderEnabled() const;
    void SetBorderEnabled(bool enabled);

    // Border color
    const math::Color& GetBorderColor() const;
    void SetBorderColor(const math::Color& color);

    // Corner radius
    bool GetCornerRadiusLinked() const;
    void SetCornerRadiusLinked(bool linked);
    float GetCornerRadiusTopLeft() const;
    void SetCornerRadiusTopLeft(float radius);
    float GetCornerRadiusTopRight() const;
    void SetCornerRadiusTopRight(float radius);
    float GetCornerRadiusBottomLeft() const;
    void SetCornerRadiusBottomLeft(float radius);
    float GetCornerRadiusBottomRight() const;
    void SetCornerRadiusBottomRight(float radius);

    // ===== Text Properties =====

    std::string GetText() const;
    void SetText(const std::string& text);

    std::string GetFontPath() const;
    void SetFontPath(const std::string& path);

    float GetFontSize() const;
    void SetFontSize(float size);

    const math::Color& GetFontColor() const;
    void SetFontColor(const math::Color& color);

    bool GetWordWrap() const;
    void SetWordWrap(bool wrap);

    // ===== Per-State Modulation (Automatic Mode) =====

    const math::Color& GetStateModulation(ButtonState state) const;
    void SetStateModulation(ButtonState state, const math::Color& color);

    // ===== Per-State StyleBox (Manual Mode) =====

    std::shared_ptr<StyleBox> GetStateStyleBox(ButtonState state) const;
    void SetStateStyleBox(ButtonState state, std::shared_ptr<StyleBox> styleBox);

    // ===== Per-State Sounds =====

    const ButtonStateSound& GetStateSound(ButtonState state) const;
    void SetStateSound(ButtonState state, const ButtonStateSound& sound);
    void SetStateSoundPath(ButtonState state, const std::string& path);

    // ===== Per-State Tweens =====

    const ButtonStateTween& GetStateTween(ButtonState state) const;
    void SetStateTween(ButtonState state, const ButtonStateTween& tween);

    // ===== Per-State Callbacks =====

    using StateCallback = std::function<void()>;
    void SetStateCallback(ButtonState state, StateCallback callback);
    void ClearStateCallback(ButtonState state);

    // ===== Editor Methods =====

    // Preview tween for a specific state (for editor)
    void PreviewTween(ButtonState state);

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

    // Update mouse interaction
    void UpdateMouseInteraction(float deltaTime);

    // Check if mouse is over button
    bool IsMouseOver(const math::Vec2& mousePos) const;

    // Transition to new state
    void TransitionToState(ButtonState newState);

    // Play state sound
    void PlayStateSound(ButtonState state);

    // Apply state tween
    void ApplyStateTween(ButtonState state, bool entering);

    // Invoke state callback
    void InvokeStateCallback(ButtonState state);

    // Calculate button size based on scale mode
    math::Vec2 CalculateButtonSize() const;

    // Calculate text size
    math::Vec2 CalculateTextSize() const;

    // Render the button background
    void RenderBackground(RenderContext& ctx, const math::Vec2& position, const math::Vec2& size, float rotation);

    // Regenerate the text mesh
    void RegenerateTextMesh(RenderContext& ctx, const math::Vec2& position, const math::Vec2& size);

    // Render the button text
    void RenderText(RenderContext& ctx, const math::Vec2& position, const math::Vec2& size);

    // Get effective style for current state
    std::shared_ptr<StyleBoxFlat> GetEffectiveStyle() const;

    // Get effective color for current state
    math::Color GetEffectiveColor(const math::Color& baseColor) const;

    // Current state
    ButtonState m_CurrentState{ButtonState::Normal};
    ButtonState m_PreviousState{ButtonState::Normal};
    bool m_ButtonEnabled{true};

    // Style mode
    ButtonStyleMode m_StyleMode{ButtonStyleMode::Automatic};

    // Scale mode
    ButtonScaleMode m_ScaleMode{ButtonScaleMode::Fixed};
    math::Vec2 m_TextPadding{10.0f, 5.0f};

    // Base style (for automatic mode)
    std::shared_ptr<StyleBox> m_BaseStyleBox;

    // Linked properties for editor control
    core::LinkedProperty4 m_CornerRadius;
    core::LinkedProperty4 m_BorderWidth;

    // Per-state data
    ButtonStateStyle m_StateStyles[static_cast<int>(ButtonState::COUNT)];
    ButtonStateSound m_StateSounds[static_cast<int>(ButtonState::COUNT)];
    ButtonStateTween m_StateTweens[static_cast<int>(ButtonState::COUNT)];
    std::unordered_map<ButtonState, StateCallback> m_StateCallbacks;

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

    // Cached text mesh (like Label does)
    MeshHandle m_TextMesh;
    std::string m_CachedText;
    float m_CachedFontSize{0.0f};
    math::Vec2 m_CachedPosition{0.0f, 0.0f};  // Track position to regenerate when node moves
    math::Color m_CachedFontColor{1.0f, 1.0f, 1.0f, 1.0f};

    // Flags
    bool m_MeshNeedsRegeneration{true};
    bool m_TextMeshNeedsRegeneration{true};
    bool m_MouseWasOver{false};
};

} // namespace components
} // namespace lupine

