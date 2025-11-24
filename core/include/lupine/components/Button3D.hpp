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
enum class Button3DState {
    Normal = 0,
    Hover = 1,
    Pressed = 2,
    Disabled = 3,
    COUNT = 4
};

/**
 * Button style mode
 */
enum class Button3DStyleMode {
    Automatic = 0,  // Single style with per-state color modulation
    Manual = 1      // Full StyleBox per state
};

/**
 * Button scale mode
 */
enum class Button3DScaleMode {
    Fixed = 0,          // Fixed size
    FitToText = 1,      // Scale to fit text with padding
    FitToTextWidth = 2  // Scale width only to fit text
};

/**
 * Billboard mode for 3D buttons
 */
enum class Button3DBillboardMode {
    Disabled = 0,  // No billboarding
    Enabled = 1,   // Full billboard (face camera)
    YAxisOnly = 2  // Y-axis billboard (rotate around Y to face camera)
};

/**
 * Per-state style data for automatic mode
 */
struct Button3DStateStyle {
    math::Color modulationColor{1.0f, 1.0f, 1.0f, 1.0f};  // Color modulation
    std::shared_ptr<StyleBox> styleBox{nullptr};            // For manual mode
};

/**
 * Per-state sound data
 */
struct Button3DStateSound {
    std::string audioPath;
    asset::AssetRef<asset::AudioAsset> audioAsset;
    std::string busName{"SFX"};
    float volume{1.0f};
};

/**
 * Per-state tween data
 */
struct Button3DStateTween {
    bool enabled{false};
    math::Vec2 scaleOffset{0.0f, 0.0f};      // Scale change
    float rotationOffset{0.0f};               // Rotation change (degrees)
    math::Vec2 positionOffset{0.0f, 0.0f};   // Position offset
    float duration{0.2f};                     // Tween duration
};

/**
 * Button3D Component
 *
 * A comprehensive 3D UI button with:
 * - Panel-like backgrounds with full StyleBox support
 * - Text labels with font properties
 * - Mouse state management (Normal, Hover, Pressed, Disabled) using ray casting
 * - Two style modes: Automatic (color modulation) and Manual (full StyleBox per state)
 * - Per-state sounds
 * - Per-state tweens (scale, rotation, position)
 * - Per-state callbacks
 * - Scale modes (fixed, fit to text)
 * - Billboard mode support (face camera, Y-axis only, or disabled)
 * - Shadow casting and receiving
 * - Double-sided rendering option
 */
class Button3D : public core::Component, public IRenderableComponent {
public:
    Button3D();
    explicit Button3D(const std::string& name);
    virtual ~Button3D();

    // ISerializable interface
    std::string GetTypeName() const override { return "Button3D"; }
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

    // ===== 3D Rendering Properties =====
    
    // Billboard mode
    Button3DBillboardMode GetBillboardMode() const;
    void SetBillboardMode(Button3DBillboardMode mode);
    
    // Double-sided rendering
    bool GetDoubleSided() const;
    void SetDoubleSided(bool doubleSided);
    
    // Shadow properties
    bool GetCastShadow() const;
    void SetCastShadow(bool castShadow);
    
    bool GetReceiveShadow() const;
    void SetReceiveShadow(bool receiveShadow);

    // ===== Layer Properties =====
    
    int GetLayer() const;
    void SetLayer(int layer);

    int GetSortingOrder() const;
    void SetSortingOrder(int order);

    // ===== Button State =====

    Button3DState GetCurrentState() const { return m_CurrentState; }
    void SetEnabled(bool enabled);
    bool IsButtonEnabled() const { return m_ButtonEnabled; }

    // ===== Style Mode =====

    Button3DStyleMode GetStyleMode() const { return m_StyleMode; }
    void SetStyleMode(Button3DStyleMode mode);

    // ===== Scale Mode =====

    Button3DScaleMode GetScaleMode() const { return m_ScaleMode; }
    void SetScaleMode(Button3DScaleMode mode);

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

    const math::Color& GetStateModulation(Button3DState state) const;
    void SetStateModulation(Button3DState state, const math::Color& color);

    // ===== Per-State StyleBox (Manual Mode) =====

    std::shared_ptr<StyleBox> GetStateStyleBox(Button3DState state) const;
    void SetStateStyleBox(Button3DState state, std::shared_ptr<StyleBox> styleBox);

    // ===== Per-State Sounds =====

    const Button3DStateSound& GetStateSound(Button3DState state) const;
    void SetStateSound(Button3DState state, const Button3DStateSound& sound);
    void SetStateSoundPath(Button3DState state, const std::string& path);

    // ===== Per-State Tweens =====

    const Button3DStateTween& GetStateTween(Button3DState state) const;
    void SetStateTween(Button3DState state, const Button3DStateTween& tween);

    // ===== Per-State Callbacks =====

    using StateCallback = std::function<void()>;
    void SetStateCallback(Button3DState state, StateCallback callback);
    void ClearStateCallback(Button3DState state);

    // ===== Editor Methods =====

    // Preview tween for a specific state (for editor)
    void PreviewTween(Button3DState state);

    // ===== IRenderableComponent Implementation =====

    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override { return SpatialType::World3D; }

    // Override to provide oriented bounding box that rotates with button
    math::OBB getOrientedBounds() const override;

    // Override for precise ray intersection on rotated buttons
    bool IntersectRay(const math::Ray& ray, float& outDistance) const override;

private:
    // Sync internal state from properties
    void SyncFromProperties();

    // Update mouse interaction using ray casting
    void UpdateMouseInteraction(float deltaTime, RenderContext& ctx);

    // Check if ray intersects button (for 3D mouse picking)
    bool IsRayIntersecting(const math::Ray& ray, float& outDistance) const;

    // Transition to new state
    void TransitionToState(Button3DState newState);

    // Play state sound
    void PlayStateSound(Button3DState state);

    // Apply state tween
    void ApplyStateTween(Button3DState state, bool entering);

    // Invoke state callback
    void InvokeStateCallback(Button3DState state);

    // Calculate button size based on scale mode
    math::Vec2 CalculateButtonSize() const;

    // Calculate text size
    math::Vec2 CalculateTextSize() const;

    // Load font from path
    bool LoadFont(const std::string& path);

    // Render the button fill in 3D space
    void RenderFill(RenderContext& ctx, const math::Mat4& transform, const math::Vec2& size, bool isBackFace);

    // Render the button border in 3D space
    void RenderBorder(RenderContext& ctx, const math::Mat4& transform, const math::Vec2& size, bool isBackFace);

    // Regenerate the text mesh
    void RegenerateTextMesh(RenderContext& ctx);

    // Render the button text in 3D space
    void RenderText(RenderContext& ctx, const math::Mat4& transform, const math::Vec2& size);

    // Get effective style for current state
    std::shared_ptr<StyleBoxFlat> GetEffectiveStyle() const;

    // Get effective color for current state
    math::Color GetEffectiveColor(const math::Color& baseColor) const;

    // Calculate billboard transform matrix
    math::Mat4 CalculateBillboardTransform(const math::Mat4& viewMatrix) const;

    // Current state
    Button3DState m_CurrentState{Button3DState::Normal};
    Button3DState m_PreviousState{Button3DState::Normal};
    bool m_ButtonEnabled{true};

    // Style mode
    Button3DStyleMode m_StyleMode{Button3DStyleMode::Automatic};

    // Scale mode
    Button3DScaleMode m_ScaleMode{Button3DScaleMode::Fixed};
    math::Vec2 m_TextPadding{0.5f, 0.25f};  // 3D world space padding (smaller than 2D)

    // Base style (for automatic mode)
    std::shared_ptr<StyleBox> m_BaseStyleBox;

    // Linked properties for editor control
    core::LinkedProperty4 m_CornerRadius;
    core::LinkedProperty4 m_BorderWidth;

    // Per-state data
    Button3DStateStyle m_StateStyles[static_cast<int>(Button3DState::COUNT)];
    Button3DStateSound m_StateSounds[static_cast<int>(Button3DState::COUNT)];
    Button3DStateTween m_StateTweens[static_cast<int>(Button3DState::COUNT)];
    std::unordered_map<Button3DState, StateCallback> m_StateCallbacks;

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
    math::Vec3 m_CachedPosition{0.0f, 0.0f, 0.0f};  // Track 3D position
    math::Color m_CachedFontColor{1.0f, 1.0f, 1.0f, 1.0f};

    // Flags
    bool m_MeshNeedsRegeneration{true};
    bool m_TextMeshNeedsRegeneration{true};
    bool m_WasIntersecting{false};

    // Cached ray intersection for this frame
    bool m_HasCachedIntersection{false};
    float m_CachedIntersectionDistance{0.0f};
};

} // namespace components
} // namespace lupine

