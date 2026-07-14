#include "lupine/components/Slider.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/input/InputManager.hpp"
#include "lupine/logger/Logger.hpp"
#include <algorithm>
#include <cmath>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

Slider::Slider()
    : UIControl("Slider")
{
}

Slider::Slider(const std::string& name)
    : UIControl(name)
{
}

Slider::~Slider() {
}

void Slider::DefineProperties() {
    DefineUIControlProperties(200.0f, 20.0f, "useUISpace", "Size");

    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(minValue, 0.0f, -1000000.0f, 1000000.0f, 0.01f, "Slider"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(maxValue, 100.0f, -1000000.0f, 1000000.0f, 0.01f, "Slider"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(value, 0.0f, -1000000.0f, 1000000.0f, 0.01f, "Slider"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(step, 0.0f, 0.0f, 1000000.0f, 0.01f, "Slider"));
    DefineProperty(PROPERTY_ENUM_GROUP(orientation, 0, "Slider", Horizontal, Vertical));
    DefineProperty(PROPERTY_DEFAULT_GROUP(editable, Bool, true, "Slider"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(trackColor, Color, Color(0.25f, 0.25f, 0.25f, 1.0f), "Appearance"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(fillColor, Color, Color(0.3f, 0.6f, 1.0f, 1.0f), "Appearance"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(handleColor, Color, Color(0.9f, 0.9f, 0.9f, 1.0f), "Appearance"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(trackThickness, 6.0f, 1.0f, 128.0f, 1.0f, "Appearance"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(handleRadius, 9.0f, 1.0f, 128.0f, 1.0f, "Appearance"));
}

void Slider::DefineSignals() {
    RegisterSignal({"value_changed",
                    {{"value", core::PropertyValueType::Float}},
                    "Emitted when the slider value changes."});
}

void Slider::OnAwake() {
    SetValue(GetValue());
}

// ========================================
// Value
// ========================================

float Slider::GetMinValue() const { return GetPropertyValue<float>("minValue"); }
void Slider::SetMinValue(float value) { SetPropertyValue<float>("minValue", value); SetValue(GetValue()); }
float Slider::GetMaxValue() const { return GetPropertyValue<float>("maxValue"); }
void Slider::SetMaxValue(float value) { SetPropertyValue<float>("maxValue", value); SetValue(GetValue()); }

float Slider::GetValue() const { return GetPropertyValue<float>("value"); }
void Slider::SetValue(float value) {
    SetPropertyValue<float>("value", ClampValue(ApplyStep(value)));
}

float Slider::GetStep() const { return GetPropertyValue<float>("step"); }
void Slider::SetStep(float step) { SetPropertyValue<float>("step", step); }

float Slider::GetRatio() const {
    const float range = GetMaxValue() - GetMinValue();
    if (std::abs(range) < 1e-6f) return 0.0f;
    return std::clamp((GetValue() - GetMinValue()) / range, 0.0f, 1.0f);
}

Slider::Orientation Slider::GetOrientation() const {
    return static_cast<Orientation>(GetPropertyValue<int>("orientation"));
}
void Slider::SetOrientation(Orientation orientation) {
    SetPropertyValue<int>("orientation", static_cast<int>(orientation));
}

bool Slider::GetEditable() const { return GetPropertyValue<bool>("editable"); }
void Slider::SetEditable(bool editable) { SetPropertyValue<bool>("editable", editable); }

float Slider::ClampValue(float value) const {
    const float lo = std::min(GetMinValue(), GetMaxValue());
    const float hi = std::max(GetMinValue(), GetMaxValue());
    return std::clamp(value, lo, hi);
}

float Slider::ApplyStep(float value) const {
    const float step = GetStep();
    if (step <= 0.0f) return value;
    const float base = GetMinValue();
    return base + std::round((value - base) / step) * step;
}

// ========================================
// Appearance accessors
// ========================================

const std::vector<UIControl::ThemeBinding>& Slider::GetThemeBindings() const {
    static const std::vector<ThemeBinding> kBindings = {
        { "trackColor",     "track_color",     ThemeBinding::Kind::Color },
        { "fillColor",      "fill_color",      ThemeBinding::Kind::Color },
        { "handleColor",    "grabber_color",   ThemeBinding::Kind::Color },
        { "trackThickness", "track_thickness", ThemeBinding::Kind::Constant },
        { "handleRadius",   "grabber_radius",  ThemeBinding::Kind::Constant }
    };
    return kBindings;
}

Color Slider::GetTrackColor() const {
    return ResolveThemedColor("trackColor", "track_color");
}
void Slider::SetTrackColor(const Color& color) { SetThemedProperty<Color>("trackColor", color); }
Color Slider::GetFillColor() const {
    return ResolveThemedColor("fillColor", "fill_color");
}
void Slider::SetFillColor(const Color& color) { SetThemedProperty<Color>("fillColor", color); }
Color Slider::GetHandleColor() const {
    return ResolveThemedColor("handleColor", "grabber_color");
}
void Slider::SetHandleColor(const Color& color) { SetThemedProperty<Color>("handleColor", color); }
float Slider::GetTrackThickness() const { return ResolveThemedConstant("trackThickness", "track_thickness"); }
void Slider::SetTrackThickness(float thickness) { SetThemedProperty<float>("trackThickness", thickness); }
float Slider::GetHandleRadius() const { return ResolveThemedConstant("handleRadius", "grabber_radius"); }
void Slider::SetHandleRadius(float radius) { SetThemedProperty<float>("handleRadius", radius); }

// ========================================
// Geometry
// ========================================

Vec2 Slider::GetCenter() const {
    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    return node2D ? node2D->GetGlobalPosition() : Vec2(0.0f, 0.0f);
}

void Slider::GetTrackExtent(Vec2& outStart, Vec2& outEnd, float& outLength) const {
    Vec2 center = GetCenter();
    Vec2 size = GetBoundsSize();
    const float handleR = GetHandleRadius();

    if (GetOrientation() == Orientation::Vertical) {
        const float half = std::max(0.0f, size.y * 0.5f - handleR);
        outStart = Vec2(center.x, center.y - half); // min (bottom)
        outEnd = Vec2(center.x, center.y + half);   // max (top)
        outLength = half * 2.0f;
    } else {
        const float half = std::max(0.0f, size.x * 0.5f - handleR);
        outStart = Vec2(center.x - half, center.y); // min (left)
        outEnd = Vec2(center.x + half, center.y);   // max (right)
        outLength = half * 2.0f;
    }
}

Vec2 Slider::GetHandleCenter() const {
    Vec2 start, end;
    float length;
    GetTrackExtent(start, end, length);
    const float t = GetRatio();
    return Vec2(start.x + (end.x - start.x) * t, start.y + (end.y - start.y) * t);
}

float Slider::ValueFromMouse(const Vec2& mouse) const {
    Vec2 start, end;
    float length;
    GetTrackExtent(start, end, length);
    if (length <= 0.0f) return GetValue();

    float t;
    if (GetOrientation() == Orientation::Vertical) {
        t = (mouse.y - start.y) / length;
    } else {
        t = (mouse.x - start.x) / length;
    }
    t = std::clamp(t, 0.0f, 1.0f);
    return GetMinValue() + t * (GetMaxValue() - GetMinValue());
}

// ========================================
// Input
// ========================================

void Slider::OnInput(float deltaTime) {
    (void)deltaTime;

    if (!IsEnabled() || !GetEditable()) {
        return;
    }

    input::InputManager& inputMgr = input::InputManager::Get();
    Vec2 mouse = GetCanvasMousePosition();
    Vec2 center = GetCenter();
    Vec2 half = GetBoundsSize() * 0.5f;
    const float handleR = GetHandleRadius();
    // Expand the hit area by the handle radius so the ends remain grabbable.
    bool overControl = mouse.x >= center.x - half.x - handleR && mouse.x <= center.x + half.x + handleR &&
                       mouse.y >= center.y - half.y - handleR && mouse.y <= center.y + half.y + handleR;

    const bool leftDown = inputMgr.IsMouseButtonPressed(input::MouseButton::Left);
    const bool leftJustPressed = inputMgr.IsMouseButtonJustPressed(input::MouseButton::Left);

    if (leftJustPressed) {
        LOG_INPUT_INFO("[diag] Slider press mouse=({},{}) center=({},{}) size=({},{}) over={}",
                       mouse.x, mouse.y, center.x, center.y, GetBoundsSize().x, GetBoundsSize().y, overControl);
    }

    if (leftJustPressed && overControl) {
        m_Dragging = true;
    }

    if (m_Dragging) {
        if (!leftDown) {
            m_Dragging = false;
        } else {
            float newValue = ApplyStep(ValueFromMouse(mouse));
            if (newValue != GetValue()) {
                SetValue(newValue);
                Emit("value_changed", { GetValue() });
            }
        }
        return;
    }

    if (overControl) {
        glm::vec2 wheel = inputMgr.GetMouseScrollDelta();
        if (wheel.y != 0.0f) {
            const float step = (GetStep() > 0.0f) ? GetStep() : (GetMaxValue() - GetMinValue()) * 0.05f;
            float newValue = GetValue() + wheel.y * step;
            if (newValue != GetValue()) {
                SetValue(newValue);
                Emit("value_changed", { GetValue() });
            }
        }
    }
}

// ========================================
// Rendering
// ========================================

void Slider::buildDrawCommands(RenderContext& ctx) {
    if (!m_Owner) {
        return;
    }

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    if (!node2D) {
        return;
    }

    Vec2 start, end;
    float length;
    GetTrackExtent(start, end, length);
    Vec2 handle = GetHandleCenter();

    const float thickness = GetTrackThickness();
    const float handleR = GetHandleRadius();
    const bool vertical = (GetOrientation() == Orientation::Vertical);

    auto drawBar = [&](const Vec2& a, const Vec2& b, const Color& color) {
        Vec2 mid = (a + b) * 0.5f;
        Vec2 size = vertical ? Vec2(thickness, std::abs(b.y - a.y))
                             : Vec2(std::abs(b.x - a.x), thickness);
        Vec2 topLeft = mid - size * 0.5f;
        ctx.drawRoundedRect(topLeft, size, thickness * 0.5f, color, 0);
    };

    // Track (full extent), then fill from start to the handle.
    drawBar(start, end, GetTrackColor());
    drawBar(start, handle, GetFillColor());

    // Handle (rounded square -> circle).
    Vec2 handleSize(handleR * 2.0f, handleR * 2.0f);
    ctx.drawRoundedRect(handle - handleSize * 0.5f, handleSize, handleR, GetHandleColor(), 0);
}

AABB Slider::getWorldBounds() const {
    Vec2 center = GetCenter();
    Vec2 half = GetBoundsSize() * 0.5f;
    const float handleR = GetHandleRadius();
    half.x += handleR;
    half.y += handleR;
    return AABB(Vec3(center.x - half.x, center.y - half.y, -0.1f),
                Vec3(center.x + half.x, center.y + half.y, 0.1f));
}

RenderLayer Slider::getRenderLayer() const {
    return RenderLayer::Transparent;
}

Vec2 Slider::GetContentMinSize() const {
    const float handleR = GetHandleRadius();
    const float cross = std::max(GetTrackThickness(), handleR * 2.0f);
    if (GetOrientation() == Orientation::Vertical) {
        return Vec2(cross, handleR * 4.0f);
    }
    return Vec2(handleR * 4.0f, cross);
}

} // namespace components
} // namespace lupine
