#pragma once

#include "lupine/math/Color.hpp"
#include <vector>
#include <string>
#include <algorithm>

namespace lupine {
namespace math {

/**
 * Gradient - a multi-stop color ramp.
 *
 * Stops are kept sorted by position (in [0,1]). Sampling interpolates linearly
 * between adjacent stops and clamps outside the first/last stop. A general,
 * serializable engine data type usable by particles, UI, materials, etc.
 */
class Gradient {
public:
    struct Stop {
        float position = 0.0f;
        Color color = Color::White();

        Stop() = default;
        Stop(float pos, const Color& col) : position(pos), color(col) {}
    };

    Gradient() = default;

    /**
     * Sample the gradient at t (clamped to [0,1]). Returns white for an empty
     * gradient and the single stop's color when only one stop exists.
     */
    Color Sample(float t) const {
        if (m_Stops.empty()) {
            return Color::White();
        }
        if (m_Stops.size() == 1) {
            return m_Stops.front().color;
        }

        if (t <= m_Stops.front().position) {
            return m_Stops.front().color;
        }
        if (t >= m_Stops.back().position) {
            return m_Stops.back().color;
        }

        for (size_t i = 1; i < m_Stops.size(); ++i) {
            const Stop& hi = m_Stops[i];
            if (t <= hi.position) {
                const Stop& lo = m_Stops[i - 1];
                float span = hi.position - lo.position;
                float localT = (span > 1e-6f) ? (t - lo.position) / span : 0.0f;
                return lo.color.Lerp(hi.color, localT);
            }
        }
        return m_Stops.back().color;
    }

    /**
     * Insert a stop, keeping the stop list sorted by position.
     */
    void AddStop(float position, const Color& color) {
        Stop stop(position, color);
        auto it = std::lower_bound(m_Stops.begin(), m_Stops.end(), stop,
            [](const Stop& a, const Stop& b) { return a.position < b.position; });
        m_Stops.insert(it, stop);
    }

    void Clear() { m_Stops.clear(); }

    const std::vector<Stop>& GetStops() const { return m_Stops; }
    size_t GetStopCount() const { return m_Stops.size(); }
    bool IsEmpty() const { return m_Stops.empty(); }

    /**
     * A gradient is "usable" for color-over-life only when it has at least two
     * stops; callers fall back to simpler color logic otherwise.
     */
    bool IsValid() const { return m_Stops.size() >= 2; }

    // ===== Serialization =====
    std::string ToJsonString() const;
    static Gradient FromJsonString(const std::string& json);

    /**
     * Default white -> transparent ramp (reproduces the classic particle fade).
     */
    static Gradient WhiteToTransparent() {
        Gradient g;
        g.AddStop(0.0f, Color(1.0f, 1.0f, 1.0f, 1.0f));
        g.AddStop(1.0f, Color(1.0f, 1.0f, 1.0f, 0.0f));
        return g;
    }

private:
    std::vector<Stop> m_Stops;
};

} // namespace math
} // namespace lupine
