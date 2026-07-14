#include "lupine/math/Gradient.hpp"
#include <nlohmann/json.hpp>

namespace lupine {
namespace math {

std::string Gradient::ToJsonString() const {
    nlohmann::json root;
    nlohmann::json stops = nlohmann::json::array();
    for (const Stop& s : m_Stops) {
        nlohmann::json stop;
        stop["pos"] = s.position;
        stop["color"] = {
            {"r", s.color.r},
            {"g", s.color.g},
            {"b", s.color.b},
            {"a", s.color.a},
        };
        stops.push_back(stop);
    }
    root["stops"] = stops;
    return root.dump();
}

Gradient Gradient::FromJsonString(const std::string& json) {
    Gradient gradient;
    if (json.empty()) {
        return gradient;
    }

    try {
        nlohmann::json root = nlohmann::json::parse(json);
        if (!root.contains("stops") || !root["stops"].is_array()) {
            return gradient;
        }
        for (const nlohmann::json& stop : root["stops"]) {
            float pos = stop.value("pos", 0.0f);
            Color color = Color::White();
            if (stop.contains("color") && stop["color"].is_object()) {
                const nlohmann::json& c = stop["color"];
                color.r = c.value("r", 1.0f);
                color.g = c.value("g", 1.0f);
                color.b = c.value("b", 1.0f);
                color.a = c.value("a", 1.0f);
            }
            gradient.AddStop(pos, color);
        }
    } catch (...) {
        gradient.Clear();
    }

    return gradient;
}

} // namespace math
} // namespace lupine
