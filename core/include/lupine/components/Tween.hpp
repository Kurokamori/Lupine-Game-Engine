#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace lupine {
namespace components {

/**
 * Tween Component
 *
 * A non-rendering utility component that interpolates a single value channel on
 * its owner node over a duration with an easing curve, then emits a "finished"
 * signal. It is the runtime backing for the scripting `create_tween*` API and can
 * also be authored in the editor.
 *
 * Channels (the `channel` string):
 *   - Transform: "position_2d", "rotation_2d", "scale_2d", "global_position_2d",
 *     "position_3d", "rotation_3d", "scale_3d", "global_position_3d". These go
 *     through the engine's script transform path (degrees + euler), so values
 *     match what scripts read/write.
 *   - Any other string targets the owner's property bag (a component-declared or
 *     node-level property), looked up the same way as the node object model.
 *
 * Values cross as JSON: a number for scalar channels, or an array ([x,y],
 * [x,y,z], [x,y,z,w], [r,g,b,a]) for vector channels. The start value is captured
 * from the live value at Start() unless SetFrom() supplied one explicitly, so
 * tweens run relative to wherever the target currently is.
 *
 * Property-bag channels store their vectors and colors as *keyed objects*
 * ({"x","y"}, {"r","g","b","a"}, ...) rather than arrays, so the component
 * canonicalizes both endpoints to arrays before interpolating and rebuilds the
 * property's original key shape on write. Callers may therefore pass a target as
 * an array or as a partial object ({"a": 0} fades only alpha, keeping the other
 * channels at their captured start value).
 */
class Tween : public core::Component {
public:
    Tween();
    explicit Tween(const std::string& name);
    virtual ~Tween();

    std::string GetTypeName() const override { return "Tween"; }
    void DefineProperties() override;
    void DefineSignals() override;

    void OnAwake() override;
    void OnUpdate(float deltaTime) override;

    // ===== Configuration (scripting API) =====
    void Configure(const std::string& channel, const nlohmann::json& toValue,
                   float duration, const std::string& easing);
    void SetChannel(const std::string& channel);
    const std::string& GetChannel() const { return m_Channel; }
    void SetToValue(const nlohmann::json& value);
    const nlohmann::json& GetToValue() const { return m_To; }
    void SetFrom(const nlohmann::json& value);  // explicit start; clears auto-capture

    // ===== Control =====
    void Start();
    void Stop();
    void Reset();
    void Restart();
    bool IsFinished() const;
    float GetProgress() const;  // 0..1 (linear, unclamped to duration)

    // ===== Property accessors (registry-backed, serialized) =====
    float GetDuration() const;
    void SetDuration(float duration);
    float GetElapsed() const;
    void SetElapsed(float elapsed);
    std::string GetEasing() const;
    void SetEasing(const std::string& easing);
    bool GetLoop() const;
    void SetLoop(bool loop);
    bool GetAutoRemove() const;
    void SetAutoRemove(bool autoRemove);
    bool IsRunning() const;
    void SetRunning(bool running);

private:
    void CaptureFromIfNeeded();
    void ApplyEased(float t01);
    void Finish();

    nlohmann::json ReadChannelValue() const;
    void WriteChannelValue(const nlohmann::json& value) const;

    /**
     * Inspect the live property value and record the key order to interpolate it
     * under, so an object-valued property (Color/Vec2/Vec3/Vec4/Quat/Rect) can be
     * lerped as an array and written back in its original shape. Leaves the key
     * list empty for transform channels and for scalar/array-valued properties,
     * which need no conversion.
     */
    void ResolveChannelShape();

    /** The canonical key order for a keyed vector/color object, or nullptr if it isn't one. */
    static const std::vector<std::string>* ObjectKeyOrder(const nlohmann::json& value);

    /**
     * Object -> array in `keys` order. Numbers and arrays pass through unchanged.
     * Keys absent from `value` take their value from `fallback` (itself a canonical
     * array) when supplied, so a partial target object leaves the other channels alone.
     */
    static nlohmann::json ToCanonicalArray(const nlohmann::json& value,
                                           const std::vector<std::string>& keys,
                                           const nlohmann::json& fallback);

    /** Array -> object keyed by `keys`. Passes through when there is no key shape. */
    static nlohmann::json FromCanonicalArray(const nlohmann::json& value,
                                             const std::vector<std::string>& keys);

    std::string m_Channel;
    nlohmann::json m_From;
    nlohmann::json m_To;
    std::vector<std::string> m_PropertyKeys;
    bool m_HasExplicitFrom = false;
    bool m_Captured = false;
    bool m_HasTriggered = false;
};

} // namespace components
} // namespace lupine
