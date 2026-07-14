#pragma once

#include <vector>
#include <string>
#include <algorithm>

namespace lupine {
namespace math {

/**
 * Curve - a 1D animation curve sampled over a normalized domain.
 *
 * Points are kept sorted by position (in [0,1]). Sampling interpolates linearly
 * between adjacent points and clamps outside the first/last point. A general,
 * serializable engine data type usable for scale/alpha/velocity over time, etc.
 *
 * Linear interpolation only for now; the point struct leaves room for per-point
 * tangents to be added later without changing the on-disk format.
 */
class Curve {
public:
    struct Point {
        float position = 0.0f;
        float value = 0.0f;

        Point() = default;
        Point(float pos, float val) : position(pos), value(val) {}
    };

    Curve() = default;

    /**
     * Sample the curve at t (clamped to [0,1]). Returns the supplied default for
     * an empty curve, and the single point's value when only one point exists.
     */
    float Sample(float t, float emptyDefault = 1.0f) const {
        if (m_Points.empty()) {
            return emptyDefault;
        }
        if (m_Points.size() == 1) {
            return m_Points.front().value;
        }

        if (t <= m_Points.front().position) {
            return m_Points.front().value;
        }
        if (t >= m_Points.back().position) {
            return m_Points.back().value;
        }

        for (size_t i = 1; i < m_Points.size(); ++i) {
            const Point& hi = m_Points[i];
            if (t <= hi.position) {
                const Point& lo = m_Points[i - 1];
                float span = hi.position - lo.position;
                float localT = (span > 1e-6f) ? (t - lo.position) / span : 0.0f;
                return lo.value + (hi.value - lo.value) * localT;
            }
        }
        return m_Points.back().value;
    }

    /**
     * Insert a point, keeping the point list sorted by position.
     */
    void AddPoint(float position, float value) {
        Point point(position, value);
        auto it = std::lower_bound(m_Points.begin(), m_Points.end(), point,
            [](const Point& a, const Point& b) { return a.position < b.position; });
        m_Points.insert(it, point);
    }

    void Clear() { m_Points.clear(); }

    const std::vector<Point>& GetPoints() const { return m_Points; }
    size_t GetPointCount() const { return m_Points.size(); }
    bool IsEmpty() const { return m_Points.empty(); }

    /**
     * A curve is "usable" only when it has at least one point; callers fall back
     * to simpler scalar logic otherwise.
     */
    bool IsValid() const { return !m_Points.empty(); }

    // ===== Serialization =====
    std::string ToJsonString() const;
    static Curve FromJsonString(const std::string& json);

    /**
     * A flat curve holding a constant value across the whole domain.
     */
    static Curve Constant(float value) {
        Curve c;
        c.AddPoint(0.0f, value);
        c.AddPoint(1.0f, value);
        return c;
    }

private:
    std::vector<Point> m_Points;
};

} // namespace math
} // namespace lupine
