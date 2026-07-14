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
#include "lupine/asset/ImageAsset.hpp"
#include <string>
#include <memory>
#include <functional>

namespace lupine {
namespace components {

// Forward declaration
class CheckList;

/**
 * Checkbox mouse states
 */
enum class CheckboxState {
    Normal = 0,
    Hover = 1,
    Pressed = 2,
    Checked = 3,
    CheckedHover = 4,
    Disabled = 5,
    COUNT = 6
};

/**
 * Per-state style data
 */
struct CheckboxStateStyle {
    math::Color modulationColor{1.0f, 1.0f, 1.0f, 1.0f};
};

/**
 * Checkbox Component
 *
 * A UI checkbox component with:
 * - Box indicator with customizable size and colors
 * - Checked/unchecked state with visual indicator
 * - Text label with font properties
 * - Mouse state management (Normal, Hover, Pressed, Disabled)
 * - Group management via CheckList component
 * - Per-state color modulation
 * - Check/uncheck callback support
 * - Audio feedback on state changes
 * - Optional texture support for checked/unchecked states
 *
 * Checkboxes can be used independently or in groups managed by CheckList.
 */
class Checkbox : public UIControl, public IRenderableComponent {
public:
    Checkbox();
    explicit Checkbox(const std::string& name);
    virtual ~Checkbox();

    // ISerializable interface
    std::string GetTypeName() const override { return "Checkbox"; }
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

    float GetBoxSize() const;
    void SetBoxSize(float size);

    // Content minimum size = box + label text (used by containers/anchors).
    math::Vec2 GetContentMinSize() const override;

    // ===== Layer Properties =====
    
    int GetLayer() const;
    void SetLayer(int layer);

    int GetSortingOrder() const;
    void SetSortingOrder(int order);

    // ===== UI Space =====
    // GetUseUISpace/SetUseUISpace are provided by the UIControl base class.

    // ===== Checkbox State =====
    
    CheckboxState GetCurrentState() const { return m_CurrentState; }
    void SetEnabled(bool enabled);
    bool IsButtonEnabled() const { return m_ButtonEnabled; }

    bool IsChecked() const { return m_IsChecked; }
    void SetChecked(bool checked);
    void Toggle();

    // ===== Group Management =====
    
    std::string GetGroupName() const;
    void SetGroupName(const std::string& groupName);

    int GetCheckboxValue() const;
    void SetCheckboxValue(int value);

    // ===== Box Properties =====

    math::Color GetBorderColor() const;
    void SetBorderColor(const math::Color& color);

    math::Color GetBackgroundColor() const;
    void SetBackgroundColor(const math::Color& color);

    math::Color GetCheckmarkColor() const;
    void SetCheckmarkColor(const math::Color& color);

    float GetCornerRadius() const;
    void SetCornerRadius(float radius);

    float GetBorderWidth() const;
    void SetBorderWidth(float width);

    // ===== Texture Properties =====

    std::string GetUncheckedTexturePath() const;
    void SetUncheckedTexturePath(const std::string& path);

    std::string GetCheckedTexturePath() const;
    void SetCheckedTexturePath(const std::string& path);

    bool GetUseTextures() const;
    void SetUseTextures(bool use);

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

    const math::Color& GetStateModulation(CheckboxState state) const;
    void SetStateModulation(CheckboxState state, const math::Color& color);

    // ===== Audio =====

    std::string GetCheckSoundPath() const;
    void SetCheckSoundPath(const std::string& path);

    std::string GetUncheckSoundPath() const;
    void SetUncheckSoundPath(const std::string& path);

    std::string GetHoverSoundPath() const;
    void SetHoverSoundPath(const std::string& path);

    std::string GetPressSoundPath() const;
    void SetPressSoundPath(const std::string& path);

    // ===== Callbacks =====

    using CheckCallback = std::function<void(bool)>;
    void SetOnCheckedCallback(CheckCallback callback);

    // ===== IRenderableComponent Implementation =====

    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override;

    math::OBB getOrientedBounds() const override;
    bool IntersectRay(const math::Ray& ray, float& outDistance) const override;

    // ===== Internal Methods (for CheckList) =====

    void NotifyCheckList();
    void SetCheckList(CheckList* checkList);

    // Calculate total size (box + text) - public for CheckList layout
    math::Vec2 CalculateTotalSize() const;

private:
    // Sync internal state from properties
    void SyncFromProperties();

    // Load per-state modulation colors from theme/instance overrides
    void LoadThemedStateData();

    // Update mouse interaction
    void UpdateMouseInteraction(float deltaTime);

    // Check if mouse is over checkbox
    bool IsMouseOver(const math::Vec2& mousePos) const;

    // Transition to new state
    void TransitionToState(CheckboxState newState);

    // Play sounds
    void PlayCheckSound();
    void PlayUncheckSound();
    void PlayHoverSound();
    void PlayPressSound();

    // Render the checkbox box
    void RenderBox(RenderContext& ctx, const math::Vec2& position, float rotation);

    // Load + GPU-upload the box textures on demand, reloading whichever path changed.
    // No-op when useTextures is disabled.
    void EnsureBoxTextures(RenderContext& ctx);

    // Regenerate the text mesh
    void RegenerateTextMesh(RenderContext& ctx, const math::Vec2& position, float rotation);

    // Render the button text
    void RenderText(RenderContext& ctx, const math::Vec2& position, float rotation);

    // Calculate text size
    math::Vec2 CalculateTextSize() const;

    // Get effective color for current state
    math::Color GetEffectiveColor(const math::Color& baseColor) const;

    // Safe property accessor for colors
    math::Color GetSafeColorProperty(const std::string& propName, const math::Color& defaultColor) const;

    // Button state
    bool m_ButtonEnabled{true};
    bool m_IsChecked{false};
    CheckboxState m_CurrentState{CheckboxState::Normal};
    CheckboxState m_PreviousState{CheckboxState::Normal};

    // Group management
    std::string m_GroupName;
    int m_CheckboxValue{0};
    CheckList* m_CheckList{nullptr};

    // Per-state data
    CheckboxStateStyle m_StateStyles[static_cast<int>(CheckboxState::COUNT)];

    // Audio
    std::string m_CheckSoundPath;
    std::string m_UncheckSoundPath;
    std::string m_HoverSoundPath;
    std::string m_PressSoundPath;
    asset::AssetRef<asset::AudioAsset> m_CheckSoundAsset;
    asset::AssetRef<asset::AudioAsset> m_UncheckSoundAsset;
    asset::AssetRef<asset::AudioAsset> m_HoverSoundAsset;
    asset::AssetRef<asset::AudioAsset> m_PressSoundAsset;

    // Textures
    asset::AssetRef<asset::ImageAsset> m_UncheckedTextureAsset;
    asset::AssetRef<asset::ImageAsset> m_CheckedTextureAsset;
    TextureHandle m_UncheckedTextureHandle;
    TextureHandle m_CheckedTextureHandle;
    std::string m_CurrentUncheckedTexturePath;
    std::string m_CurrentCheckedTexturePath;

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

    // Callback
    CheckCallback m_OnCheckedCallback;
};

} // namespace components
} // namespace lupine

