#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <cstddef>

namespace lupine {
namespace animation {

/**
 * Shared interpolation primitives for the animation runtime.
 *
 * These were factored out of components::Tween so the keyframe sampler and the
 * tween share one implementation of the Penner easing curves and JSON value
 * interpolation. components::Tween now calls into this module.
 */

// Resolve a named easing curve (Penner equations). Unknown names => linear.
// Input t is clamped to [0,1].
float ApplyEasing(const std::string& name, float t);

// Component-wise linear interpolation of two JSON values (numbers or arrays).
nlohmann::json LerpJson(const nlohmann::json& a, const nlohmann::json& b, float t);

// Extract a single float component from a JSON value (number or array). Out of
// range / non-numeric => 0.
float JsonComponent(const nlohmann::json& v, std::size_t index);

// Cubic Hermite interpolation of one scalar component across a keyframe segment.
// v0/v1 are the surrounding key values; outTan0/inTan1 are tangents in value units
// per second; segDuration is (t1 - t0) in seconds; localT is normalized [0,1].
float CubicHermite(float v0, float v1, float outTan0, float inTan1,
                   float segDuration, float localT);

} // namespace animation
} // namespace lupine
