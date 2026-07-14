#include "lupine/animation/AnimationInterp.hpp"
#include <algorithm>
#include <cmath>

namespace lupine {
namespace animation {

namespace {

constexpr float kPi = 3.14159265358979323846f;

// Standard (Penner-style) easing equations operating on a normalized t in [0,1].
float EaseInBack(float t) {
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    return c3 * t * t * t - c1 * t * t;
}
float EaseOutBack(float t) {
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    float p = t - 1.0f;
    return 1.0f + c3 * p * p * p + c1 * p * p;
}
float EaseInOutBack(float t) {
    const float c1 = 1.70158f;
    const float c2 = c1 * 1.525f;
    if (t < 0.5f) {
        return (std::pow(2.0f * t, 2.0f) * ((c2 + 1.0f) * 2.0f * t - c2)) / 2.0f;
    }
    float p = 2.0f * t - 2.0f;
    return (std::pow(p, 2.0f) * ((c2 + 1.0f) * p + c2) + 2.0f) / 2.0f;
}
float EaseOutBounce(float t) {
    const float n1 = 7.5625f;
    const float d1 = 2.75f;
    if (t < 1.0f / d1) {
        return n1 * t * t;
    } else if (t < 2.0f / d1) {
        t -= 1.5f / d1;
        return n1 * t * t + 0.75f;
    } else if (t < 2.5f / d1) {
        t -= 2.25f / d1;
        return n1 * t * t + 0.9375f;
    }
    t -= 2.625f / d1;
    return n1 * t * t + 0.984375f;
}
float EaseInBounce(float t) {
    return 1.0f - EaseOutBounce(1.0f - t);
}
float EaseInOutBounce(float t) {
    return t < 0.5f
        ? (1.0f - EaseOutBounce(1.0f - 2.0f * t)) / 2.0f
        : (1.0f + EaseOutBounce(2.0f * t - 1.0f)) / 2.0f;
}
float EaseInElastic(float t) {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    const float c4 = (2.0f * kPi) / 3.0f;
    return -std::pow(2.0f, 10.0f * t - 10.0f) * std::sin((t * 10.0f - 10.75f) * c4);
}
float EaseOutElastic(float t) {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    const float c4 = (2.0f * kPi) / 3.0f;
    return std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * c4) + 1.0f;
}
float EaseInOutElastic(float t) {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    const float c5 = (2.0f * kPi) / 4.5f;
    if (t < 0.5f) {
        return -(std::pow(2.0f, 20.0f * t - 10.0f) * std::sin((20.0f * t - 11.125f) * c5)) / 2.0f;
    }
    return (std::pow(2.0f, -20.0f * t + 10.0f) * std::sin((20.0f * t - 11.125f) * c5)) / 2.0f + 1.0f;
}

} // namespace

float ApplyEasing(const std::string& name, float t) {
    t = std::clamp(t, 0.0f, 1.0f);

    if (name.empty() || name == "linear") return t;

    if (name == "sine_in")     return 1.0f - std::cos((t * kPi) / 2.0f);
    if (name == "sine_out")    return std::sin((t * kPi) / 2.0f);
    if (name == "sine_in_out") return -(std::cos(kPi * t) - 1.0f) / 2.0f;

    if (name == "quad_in")     return t * t;
    if (name == "quad_out")    return 1.0f - (1.0f - t) * (1.0f - t);
    if (name == "quad_in_out") return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;

    if (name == "cubic_in")     return t * t * t;
    if (name == "cubic_out")    return 1.0f - std::pow(1.0f - t, 3.0f);
    if (name == "cubic_in_out") return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;

    if (name == "quart_in")     return t * t * t * t;
    if (name == "quart_out")    return 1.0f - std::pow(1.0f - t, 4.0f);
    if (name == "quart_in_out") return t < 0.5f ? 8.0f * t * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 4.0f) / 2.0f;

    if (name == "quint_in")     return t * t * t * t * t;
    if (name == "quint_out")    return 1.0f - std::pow(1.0f - t, 5.0f);
    if (name == "quint_in_out") return t < 0.5f ? 16.0f * t * t * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 5.0f) / 2.0f;

    if (name == "expo_in")     return t <= 0.0f ? 0.0f : std::pow(2.0f, 10.0f * t - 10.0f);
    if (name == "expo_out")    return t >= 1.0f ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t);
    if (name == "expo_in_out") {
        if (t <= 0.0f) return 0.0f;
        if (t >= 1.0f) return 1.0f;
        return t < 0.5f ? std::pow(2.0f, 20.0f * t - 10.0f) / 2.0f
                        : (2.0f - std::pow(2.0f, -20.0f * t + 10.0f)) / 2.0f;
    }

    if (name == "circ_in")     return 1.0f - std::sqrt(1.0f - std::pow(t, 2.0f));
    if (name == "circ_out")    return std::sqrt(1.0f - std::pow(t - 1.0f, 2.0f));
    if (name == "circ_in_out") {
        return t < 0.5f
            ? (1.0f - std::sqrt(1.0f - std::pow(2.0f * t, 2.0f))) / 2.0f
            : (std::sqrt(1.0f - std::pow(-2.0f * t + 2.0f, 2.0f)) + 1.0f) / 2.0f;
    }

    if (name == "back_in")     return EaseInBack(t);
    if (name == "back_out")    return EaseOutBack(t);
    if (name == "back_in_out") return EaseInOutBack(t);

    if (name == "elastic_in")     return EaseInElastic(t);
    if (name == "elastic_out")    return EaseOutElastic(t);
    if (name == "elastic_in_out") return EaseInOutElastic(t);

    if (name == "bounce_in")     return EaseInBounce(t);
    if (name == "bounce_out")    return EaseOutBounce(t);
    if (name == "bounce_in_out") return EaseInOutBounce(t);

    // Friendly aliases.
    if (name == "ease_in")     return t * t;
    if (name == "ease_out")    return 1.0f - (1.0f - t) * (1.0f - t);
    if (name == "ease_in_out") return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;

    return t;
}

nlohmann::json LerpJson(const nlohmann::json& a, const nlohmann::json& b, float t) {
    if (a.is_number() && b.is_number()) {
        double av = a.get<double>();
        double bv = b.get<double>();
        return av + (bv - av) * static_cast<double>(t);
    }
    if (a.is_array() && b.is_array()) {
        nlohmann::json out = nlohmann::json::array();
        size_t n = std::min(a.size(), b.size());
        for (size_t i = 0; i < n; ++i) {
            if (a[i].is_number() && b[i].is_number()) {
                double av = a[i].get<double>();
                double bv = b[i].get<double>();
                out.push_back(av + (bv - av) * static_cast<double>(t));
            } else {
                out.push_back(t >= 1.0f ? b[i] : a[i]);
            }
        }
        return out;
    }
    return (t >= 1.0f) ? b : a;
}

float JsonComponent(const nlohmann::json& v, std::size_t index) {
    if (v.is_array()) {
        return index < v.size() && v[index].is_number() ? v[index].get<float>() : 0.0f;
    }
    if (v.is_number()) {
        return index == 0 ? v.get<float>() : 0.0f;
    }
    return 0.0f;
}

float CubicHermite(float v0, float v1, float outTan0, float inTan1,
                   float segDuration, float localT) {
    float u = std::clamp(localT, 0.0f, 1.0f);
    float u2 = u * u;
    float u3 = u2 * u;
    float h00 = 2.0f * u3 - 3.0f * u2 + 1.0f;
    float h10 = u3 - 2.0f * u2 + u;
    float h01 = -2.0f * u3 + 3.0f * u2;
    float h11 = u3 - u2;
    return h00 * v0
         + h10 * (outTan0 * segDuration)
         + h01 * v1
         + h11 * (inTan1 * segDuration);
}

} // namespace animation
} // namespace lupine
