#pragma once

#include "lupine/components/UIControl.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/math/Math.hpp"
#include <string>

namespace lupine {
namespace components {

/**
 * Slider
 *
 * A draggable value selector along a horizontal or vertical track.
 *
 * Features:
 * - Min/Max/Value with optional step snapping
 * - Horizontal or vertical orientation
 * - Click-to-set and drag, plus mouse-wheel adjustment
 * - Customizable track, fill and handle colors / sizes
 * - Emits "value_changed" on change
 */
class Slider : public UIControl, public IRenderableComponent {
public:
    enum class Orientation {
        Horizontal = 0,
        Vertical = 1
    };

    Slider();
    explicit Slider(const std::string& name);
    virtual ~Slider();

    std::string GetTypeName() const override { return "Slider"; }
    void DefineProperties() override;
    void DefineSignals() override;

    // Theme: track/fill/grabber colours, track thickness + grabber radius.
    const std::vector<ThemeBinding>& GetThemeBindings() const override;

    void OnAwake() override;
    void OnInput(float deltaTime) override;

    // ===== Value =====
    float GetMinValue() const;
    void SetMinValue(float value);
    float GetMaxValue() const;
    void SetMaxValue(float value);
    float GetValue() const;
    void SetValue(float value);
    float GetStep() const;
    void SetStep(float step);
    float GetRatio() const;

    Orientation GetOrientation() const;
    void SetOrientation(Orientation orientation);

    bool GetEditable() const;
    void SetEditable(bool editable);

    // ===== Appearance =====
    math::Color GetTrackColor() const;
    void SetTrackColor(const math::Color& color);
    math::Color GetFillColor() const;
    void SetFillColor(const math::Color& color);
    math::Color GetHandleColor() const;
    void SetHandleColor(const math::Color& color);
    float GetTrackThickness() const;
    void SetTrackThickness(float thickness);
    float GetHandleRadius() const;
    void SetHandleRadius(float radius);

    // ===== IRenderableComponent =====
    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override { return GetUISpatialType(); }
    math::Vec2 GetContentMinSize() const override;

private:
    float ClampValue(float value) const;
    float ApplyStep(float value) const;
    float ValueFromMouse(const math::Vec2& mouse) const;

    // Geometry in node space (center origin), computed from node position + size.
    math::Vec2 GetCenter() const;
    void GetTrackExtent(math::Vec2& outStart, math::Vec2& outEnd, float& outLength) const;
    math::Vec2 GetHandleCenter() const;

    bool m_Dragging{false};
};

} // namespace components
} // namespace lupine
