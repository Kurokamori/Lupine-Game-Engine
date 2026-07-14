#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include "lupine/core/LinkedProperty.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/rendering/ResourceHandles.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/TextLayout.hpp"
#include "lupine/components/StyleBox.hpp"
#include "lupine/components/UIControl.hpp"
#include "lupine/components/UIImageDraw.hpp"
#include "lupine/audio/AudioManager.hpp"
#include "lupine/asset/AudioAsset.hpp"
#include "lupine/asset/FontAsset.hpp"
#include "lupine/asset/ImageAsset.hpp"
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
class Button : public UIControl, public IRenderableComponent {
public:
    Button();
    explicit Button(const std::string& name);
    virtual ~Button();

    // ISerializable interface
    std::string GetTypeName() const override { return "Button"; }
    void DefineProperties() override;
    void DefineSignals() override;

    // Theme: background/border/font colours, font size + uniform corner radius.
    const std::vector<ThemeBinding>& GetThemeBindings() const override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnInput(float deltaTime) override;
    void OnUpdate(float deltaTime) override;
    void OnRender() override;

    // Pointer arbitration: a button is opaque to the cursor and hit-tests with its
    // own center-pivot bounds, so overlapping buttons don't all react to one click.
    bool ConsumesPointerInput() const override { return true; }
    bool ContainsCanvasPoint(const math::Vec2& point) const override { return IsMouseOver(point); }

    // Editor gizmo hooks
    bool OnGizmoScale(float scaleDelta, int axis, bool is3D) override;

    // Asset hot-reload support
    bool OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) override;

    // ===== Size Properties =====
    // Width/height keep button-specific scale-mode logic (FitToText). The
    // width/height properties themselves are registered by the UIControl base.
    float GetWidth() const override;
    void SetWidth(float width);

    float GetHeight() const override;
    void SetHeight(float height);

    // Content minimum size = measured text + text padding (used by containers/anchors).
    math::Vec2 GetContentMinSize() const override;

    // ===== Layer Properties =====
    
    int GetLayer() const;
    void SetLayer(int layer);

    int GetSortingOrder() const;
    void SetSortingOrder(int order);

    // ===== UI Space =====
    // GetUseUISpace/SetUseUISpace are provided by the UIControl base class.

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
    math::Color GetBackgroundColor() const;
    void SetBackgroundColor(const math::Color& color);

    float GetOpacity() const;
    void SetOpacity(float opacity);

    // Background image (themeable). Drawn over the flat background fill, tinted by the
    // current state modulation + opacity. Empty path = flat-color background only.
    std::string GetBackgroundImagePath() const;
    void SetBackgroundImagePath(const std::string& path);

    UIImageStretchMode GetBackgroundImageStretchMode() const;
    void SetBackgroundImageStretchMode(UIImageStretchMode mode);

    // Nine-slice config for the background image (used when stretch mode == NineSlice).
    // Margins are in SOURCE texture pixels: x=left, y=top, z=right, w=bottom.
    math::Vec4 GetNineSliceMargins() const;
    void SetNineSliceMargins(const math::Vec4& margins);

    UINineSliceAxisMode GetNineSliceAxisHorizontal() const;
    void SetNineSliceAxisHorizontal(UINineSliceAxisMode mode);

    UINineSliceAxisMode GetNineSliceAxisVertical() const;
    void SetNineSliceAxisVertical(UINineSliceAxisMode mode);

    bool GetNineSliceDrawCenter() const;
    void SetNineSliceDrawCenter(bool drawCenter);

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
    math::Color GetBorderColor() const;
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

    math::Color GetFontColor() const;
    void SetFontColor(const math::Color& color);

    bool GetWordWrap() const;
    void SetWordWrap(bool wrap);

    // Text alignment inside the button's content box. A Button previously had none: the text
    // was hard-centered by a hand-rolled glyph loop and there was no way to left-align it.
    TextHAlign GetTextHAlign() const;
    void SetTextHAlign(TextHAlign align);

    TextVAlign GetTextVAlign() const;
    void SetTextVAlign(TextVAlign align);

    // Extra pixels between wrapped/multiline rows. Present on Label/RichTextLabel/TextEdit,
    // and missing here until now.
    float GetLineSpacing() const;
    void SetLineSpacing(float spacing);

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

    // Resolve per-state modulation + tween from the theme (falling back to this
    // control's own property values / overrides). Called from OnAwake and each
    // SyncFromProperties so theme edits apply live.
    void LoadThemedStateData();

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

    // Effective on-screen size: in Fixed scale mode this is the resolved (anchor-driven)
    // rect, so the button stretches with anchors like any UIControl; in FitToText /
    // FitToTextWidth the text-fit size wins on the fitted axis ("fit to text" overrides
    // anchoring). Used for rendering, world bounds, and mouse hit-testing.
    math::Vec2 GetEffectiveSize() const;

    // Calculate text size (through TextLayout, so wrap/line-spacing/multiline all count)
    math::Vec2 CalculateTextSize() const;

    // The box the label is laid out inside: the button's rect (centered on `center`) inset by
    // the content margins -- textPadding and/or the themed StyleBox's content margins.
    math::Rect GetTextBoxRect(const math::Vec2& center) const;

    // Shared TextLayout parameters for both the measure and the draw path, so the two can
    // never disagree. `boxSize` is passed in rather than read, so measuring cannot recurse
    // into the sizing it feeds.
    TextLayoutParams BuildLayoutParams(const math::Vec2& boxSize) const;

    // Render the button background
    void RenderBackground(RenderContext& ctx, const math::Vec2& position, const math::Vec2& size, float rotation);

    // Render the optional background image (lazily loads/uploads the texture). Drawn
    // over the flat fill, tinted by the supplied color (state modulation * opacity).
    void RenderBackgroundImage(RenderContext& ctx, const math::Vec2& position, const math::Vec2& size,
                               float rotation, const math::Color& tint);

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

    // Background image rendering
    asset::AssetRef<asset::ImageAsset> m_BackgroundImageAsset;
    TextureHandle m_BackgroundImageHandle;
    std::string m_CurrentBackgroundImagePath;

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

