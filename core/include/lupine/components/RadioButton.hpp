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
#include <functional>

namespace lupine {
namespace components {

// Forward declaration
class RadioList;

/**
 * RadioButton mouse states
 */
enum class RadioButtonState {
    Normal = 0,
    Hover = 1,
    Pressed = 2,
    Selected = 3,
    SelectedHover = 4,
    Disabled = 5,
    COUNT = 6
};

/**
 * Per-state style data
 */
struct RadioButtonStateStyle {
    math::Color modulationColor{1.0f, 1.0f, 1.0f, 1.0f};
};

/**
 * RadioButton Component
 *
 * A UI radio button component with:
 * - Circular indicator with customizable size and colors
 * - Text label with font properties
 * - Mouse state management (Normal, Hover, Pressed, Selected, SelectedHover, Disabled)
 * - Group management via RadioList component
 * - Per-state color modulation
 * - Selection callback support
 * - Audio feedback on state changes
 *
 * Radio buttons work in groups where only one can be selected at a time.
 * Use RadioList component to manage groups of radio buttons.
 */
class RadioButton : public UIControl, public IRenderableComponent {
public:
    RadioButton();
    explicit RadioButton(const std::string& name);
    virtual ~RadioButton();

    // ISerializable interface
    std::string GetTypeName() const override { return "RadioButton"; }
    void DefineProperties() override;
    const std::vector<ThemeBinding>& GetThemeBindings() const override;
    void DefineSignals() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnInput(float deltaTime) override;
    void OnUpdate(float deltaTime) override;

    // Property change notification
    void OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) override;

    // Editor gizmo hooks
    // Pointer arbitration: opaque to the cursor, hit-tests with its own bounds so
    // overlapping controls don't all react to one click.
    bool ConsumesPointerInput() const override { return true; }
    bool ContainsCanvasPoint(const math::Vec2& point) const override { return IsMouseOver(point); }

    bool OnGizmoScale(float scaleDelta, int axis, bool is3D) override;

    // Asset hot-reload support
    bool OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) override;

    // ===== Size Properties =====

    float GetIndicatorSize() const;
    void SetIndicatorSize(float size);

    // Content minimum size = indicator + label text (used by containers/anchors).
    math::Vec2 GetContentMinSize() const override;

    // ===== Layer Properties =====
    
    int GetLayer() const;
    void SetLayer(int layer);

    int GetSortingOrder() const;
    void SetSortingOrder(int order);

    // ===== UI Space =====
    // GetUseUISpace/SetUseUISpace are provided by the UIControl base class.

    // ===== Radio Button State =====
    
    RadioButtonState GetCurrentState() const { return m_CurrentState; }
    void SetEnabled(bool enabled);
    bool IsButtonEnabled() const { return m_ButtonEnabled; }

    bool IsSelected() const { return m_IsSelected; }
    void SetSelected(bool selected);

    // ===== Group Management =====
    
    std::string GetGroupName() const;
    void SetGroupName(const std::string& groupName);

    int GetRadioValue() const;
    void SetRadioValue(int value);

    // ===== Indicator Properties =====

    math::Color GetOuterCircleColor() const;
    void SetOuterCircleColor(const math::Color& color);

    math::Color GetBackgroundColor() const;
    void SetBackgroundColor(const math::Color& color);

    math::Color GetInnerCircleColor() const;
    void SetInnerCircleColor(const math::Color& color);

    float GetInnerCircleScale() const;
    void SetInnerCircleScale(float scale);

    float GetBorderWidth() const;
    void SetBorderWidth(float width);

    // ===== Text Properties =====

    std::string GetText() const;
    void SetText(const std::string& text);

    std::string GetFontPath() const;
    void SetFontPath(const std::string& path);

    float GetFontSize() const;
    void SetFontSize(float size);

    math::Color GetFontColor() const;
    void SetFontColor(const math::Color& color);

    float GetTextOffset() const;
    void SetTextOffset(float offset);

    // ===== Per-State Modulation =====

    const math::Color& GetStateModulation(RadioButtonState state) const;
    void SetStateModulation(RadioButtonState state, const math::Color& color);

    // ===== Audio =====

    std::string GetSelectSoundPath() const;
    void SetSelectSoundPath(const std::string& path);

    // ===== Selection Callback =====

    using SelectionCallback = std::function<void(int)>;
    void SetOnSelectedCallback(SelectionCallback callback);

    // ===== IRenderableComponent Implementation =====

    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override;

    math::OBB getOrientedBounds() const override;
    bool IntersectRay(const math::Ray& ray, float& outDistance) const override;

    // ===== Internal Methods (for RadioList) =====

    void NotifyRadioList();
    void SetRadioList(RadioList* radioList);

    // Calculate total size (indicator + text) - public for RadioList layout
    math::Vec2 CalculateTotalSize() const;

private:
    // Sync internal state from properties
    void SyncFromProperties();

    // Load per-state modulation data, honoring theme bindings and per-instance overrides
    void LoadThemedStateData();

    // Update mouse interaction
    void UpdateMouseInteraction(float deltaTime);

    // Check if mouse is over button
    bool IsMouseOver(const math::Vec2& mousePos) const;

    // Transition to new state
    void TransitionToState(RadioButtonState newState);

    // Play selection sound
    void PlaySelectionSound();

    // Deselect other radio buttons with the same group name (fallback when no RadioList)
    void DeselectOtherRadioButtons();

    // Render the radio button indicator (circle)
    void RenderIndicator(RenderContext& ctx, const math::Vec2& position, float rotation);

    // Regenerate the text mesh
    void RegenerateTextMesh(RenderContext& ctx, const math::Vec2& position, float rotation);

    // Render the button text
    void RenderText(RenderContext& ctx, const math::Vec2& position, float rotation);

    // Calculate text size
    math::Vec2 CalculateTextSize() const;

    // Get effective color for current state
    math::Color GetEffectiveColor(const math::Color& baseColor) const;

    // Safe property accessor for colors (handles null/uninitialized properties)
    math::Color GetSafeColorProperty(const std::string& propName, const math::Color& defaultColor) const;

    // Button state
    bool m_ButtonEnabled{true};
    bool m_IsSelected{false};
    RadioButtonState m_CurrentState{RadioButtonState::Normal};
    RadioButtonState m_PreviousState{RadioButtonState::Normal};

    // Group management
    std::string m_GroupName;
    int m_RadioValue{0};
    RadioList* m_RadioList{nullptr};

    // Per-state data
    RadioButtonStateStyle m_StateStyles[static_cast<int>(RadioButtonState::COUNT)];

    // Audio
    std::string m_SelectSoundPath;
    asset::AssetRef<asset::AudioAsset> m_SelectSoundAsset;

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
    float m_CachedRotation{0.0f};
    math::Color m_CachedFontColor{1.0f, 1.0f, 1.0f, 1.0f};

    // Flags
    bool m_TextMeshNeedsRegeneration{true};
    bool m_MouseWasOver{false};
    bool m_PressedInside{false};

    // Selection callback
    SelectionCallback m_OnSelectedCallback;
};

} // namespace components
} // namespace lupine

