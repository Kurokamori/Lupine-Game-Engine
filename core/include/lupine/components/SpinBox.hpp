#pragma once

#include "lupine/components/UIControl.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/ResourceHandles.hpp"
#include "lupine/rendering/TextLayout.hpp"
#include "lupine/asset/FontAsset.hpp"
#include "lupine/math/Math.hpp"
#include <string>

namespace lupine {
namespace components {

/**
 * SpinBox
 *
 * A numeric input field with +/- stepper buttons, drag-to-adjust, mouse-wheel
 * stepping, and inline numeric text entry. Value is clamped to [min, max] and
 * formatted with a configurable number of decimals, with optional prefix/suffix.
 *
 * Signals:
 * - value_changed(value): emitted whenever the value changes
 */
class SpinBox : public UIControl, public IRenderableComponent {
public:
    SpinBox();
    explicit SpinBox(const std::string& name);
    virtual ~SpinBox();

    std::string GetTypeName() const override { return "SpinBox"; }
    void DefineProperties() override;
    void DefineSignals() override;

    // Theme: font/background/button/border colours, font size, corner radius +
    // border width.
    const std::vector<ThemeBinding>& GetThemeBindings() const override;

    void OnAwake() override;
    void OnInput(float deltaTime) override;
    void OnUpdate(float deltaTime) override;
    bool OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) override;

    // ===== Value =====
    float GetValue() const;
    void SetValue(float value);
    float GetMinValue() const;
    void SetMinValue(float value);
    float GetMaxValue() const;
    void SetMaxValue(float value);
    float GetStep() const;
    void SetStep(float step);
    int GetDecimals() const;
    void SetDecimals(int decimals);
    bool GetEditable() const;
    void SetEditable(bool editable);

    std::string GetPrefix() const;
    void SetPrefix(const std::string& prefix);
    std::string GetSuffix() const;
    void SetSuffix(const std::string& suffix);

    // ===== Font / appearance =====
    std::string GetFontPath() const;
    void SetFontPath(const std::string& path);
    float GetFontSize() const;
    void SetFontSize(float size);

    math::Color GetFontColor() const;
    void SetFontColor(const math::Color& color);
    math::Color GetBackgroundColor() const;
    void SetBackgroundColor(const math::Color& color);
    math::Color GetButtonColor() const;
    void SetButtonColor(const math::Color& color);

    // ===== IRenderableComponent =====
    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override { return GetUISpatialType(); }
    math::Vec2 GetContentMinSize() const override;

private:
    void EnsureFont(RenderContext& ctx);
    std::string FormatValue() const;
    float ClampToRange(float value) const;
    void Step(int dir);
    void BeginEdit();
    void CommitEdit();
    void CancelEdit();

    math::Vec2 GetCenter() const;
    math::Rect GetFieldRect() const;     // the text area (excludes stepper)
    math::Rect GetStepperRect() const;   // the +/- column on the right
    math::Rect GetUpButtonRect() const;
    math::Rect GetDownButtonRect() const;
    void EmitValueChanged();

    // Font
    asset::AssetRef<asset::FontAsset> m_FontAsset;
    FontHandle m_FontHandle;
    std::string m_CurrentFontPath;
    float m_CurrentFontSize{16.0f};

    // Cached text mesh.
    MeshHandle m_TextMesh;
    std::string m_TextMeshKey;

    // Inline editing.
    bool m_Editing{false};
    std::string m_EditBuffer;

    // Drag-to-adjust.
    bool m_Dragging{false};
    bool m_PressedInField{false};
    float m_DragStartMouseX{0.0f};
    float m_DragStartValue{0.0f};
    float m_DragAccum{0.0f};

    // Caret blink (edit mode).
    float m_BlinkTime{0.0f};
    bool m_CaretVisible{true};
};

} // namespace components
} // namespace lupine
