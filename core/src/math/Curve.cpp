#include "lupine/math/Curve.hpp"
#include <nlohmann/json.hpp>

namespace lupine {
namespace math {

std::string Curve::ToJsonString() const {
    nlohmann::json root;
    nlohmann::json points = nlohmann::json::array();
    for (const Point& p : m_Points) {
        points.push_back({
            {"pos", p.position},
            {"value", p.value},
        });
    }
    root["points"] = points;
    return root.dump();
}

Curve Curve::FromJsonString(const std::string& json) {
    Curve curve;
    if (json.empty()) {
        return curve;
    }

    try {
        nlohmann::json root = nlohmann::json::parse(json);
        if (!root.contains("points") || !root["points"].is_array()) {
            return curve;
        }
        for (const nlohmann::json& point : root["points"]) {
            float pos = point.value("pos", 0.0f);
            float value = point.value("value", 0.0f);
            curve.AddPoint(pos, value);
        }
    } catch (...) {
        curve.Clear();
    }

    return curve;
}

} // namespace math
} // namespace lupine
