#include "lupine/core/PropertyDescriptor.hpp"
#include "lupine/logger/Logger.hpp"
#include <sstream>

namespace lupine {
namespace core {

std::vector<std::string> PropertyHint::GetEnumValues() const {
    std::vector<std::string> values;
    std::stringstream ss(hintString);
    std::string item;

    while (std::getline(ss, item, ',')) {

        size_t start = item.find_first_not_of(" \t\n\r");
        size_t end = item.find_last_not_of(" \t\n\r");
        if (start != std::string::npos && end != std::string::npos) {
            values.push_back(item.substr(start, end - start + 1));
        }
    }

    return values;
}

void PropertyHint::GetRangeValues(float& min, float& max, float& step) const {
    std::stringstream ss(hintString);
    std::string item;
    std::vector<float> values;

    while (std::getline(ss, item, ',')) {
        try {
            values.push_back(std::stof(item));
        } catch (...) {

        }
    }

    if (values.size() >= 1) min = values[0];
    if (values.size() >= 2) max = values[1];
    if (values.size() >= 3) step = values[2];
}

void PropertyHint::GetRangeValues(int& min, int& max, int& step) const {
    std::stringstream ss(hintString);
    std::string item;
    std::vector<int> values;

    while (std::getline(ss, item, ',')) {
        try {
            values.push_back(std::stoi(item));
        } catch (...) {

        }
    }

    if (values.size() >= 1) min = values[0];
    if (values.size() >= 2) max = values[1];
    if (values.size() >= 3) step = values[2];
}

nlohmann::json PropertyHint::Serialize() const {
    nlohmann::json json;
    json["type"] = static_cast<int>(type);
    json["hint_string"] = hintString;
    return json;
}

PropertyHint PropertyHint::Deserialize(const nlohmann::json& json) {
    PropertyHint hint;
    if (json.contains("type")) {
        hint.type = static_cast<PropertyHintType>(json["type"].get<int>());
    }
    if (json.contains("hint_string")) {
        hint.hintString = json["hint_string"].get<std::string>();
    }
    return hint;
}

nlohmann::json PropertyDescriptor::Serialize() const {
    nlohmann::json json;
    json["name"] = name;
    json["type"] = static_cast<int>(type);
    json["default"] = GetDefaultAsJson();
    json["hint"] = hint.Serialize();
    json["description"] = description;
    json["group"] = group;
    return json;
}

PropertyDescriptor PropertyDescriptor::Deserialize(const nlohmann::json& json) {
    PropertyDescriptor desc;

    if (json.contains("name")) {
        desc.name = json["name"].get<std::string>();
    }

    if (json.contains("type")) {
        desc.type = static_cast<PropertyValueType>(json["type"].get<int>());
    }

    if (json.contains("hint")) {
        desc.hint = PropertyHint::Deserialize(json["hint"]);
    }

    if (json.contains("description")) {
        desc.description = json["description"].get<std::string>();
    }

    if (json.contains("group")) {
        desc.group = json["group"].get<std::string>();
    }

    if (json.contains("default") && !json["default"].is_null()) {
        const auto& defJson = json["default"];

        switch (desc.type) {
            case PropertyValueType::Int:
                if (defJson.is_number_integer()) {
                    desc.defaultValue = defJson.get<int>();
                }
                break;

            case PropertyValueType::Float:
                if (defJson.is_number()) {
                    desc.defaultValue = defJson.get<float>();
                }
                break;

            case PropertyValueType::String:
            case PropertyValueType::NodePath:
            case PropertyValueType::ScenePath:
                if (defJson.is_string()) {
                    desc.defaultValue = defJson.get<std::string>();
                }
                break;

            case PropertyValueType::Bool:
                if (defJson.is_boolean()) {
                    desc.defaultValue = defJson.get<bool>();
                }
                break;

            case PropertyValueType::Vec2:
                if (defJson.is_object() && defJson.contains("x") && defJson.contains("y")) {
                    math::Vec2 v;
                    v.x = defJson["x"].get<float>();
                    v.y = defJson["y"].get<float>();
                    desc.defaultValue = v;
                }
                break;

            case PropertyValueType::Vec3:
                if (defJson.is_object() && defJson.contains("x") &&
                    defJson.contains("y") && defJson.contains("z")) {
                    math::Vec3 v;
                    v.x = defJson["x"].get<float>();
                    v.y = defJson["y"].get<float>();
                    v.z = defJson["z"].get<float>();
                    desc.defaultValue = v;
                }
                break;

            case PropertyValueType::Vec4:
                if (defJson.is_object() && defJson.contains("x") && defJson.contains("y") &&
                    defJson.contains("z") && defJson.contains("w")) {
                    math::Vec4 v;
                    v.x = defJson["x"].get<float>();
                    v.y = defJson["y"].get<float>();
                    v.z = defJson["z"].get<float>();
                    v.w = defJson["w"].get<float>();
                    desc.defaultValue = v;
                }
                break;

            case PropertyValueType::Color:
                if (defJson.is_object() && defJson.contains("r") && defJson.contains("g") &&
                    defJson.contains("b") && defJson.contains("a")) {
                    math::Color c;
                    c.r = defJson["r"].get<float>();
                    c.g = defJson["g"].get<float>();
                    c.b = defJson["b"].get<float>();
                    c.a = defJson["a"].get<float>();
                    desc.defaultValue = c;
                }
                break;

            case PropertyValueType::Enum:
                if (defJson.is_string()) {
                    desc.defaultValue = defJson.get<std::string>();
                } else if (defJson.is_number_integer()) {
                    desc.defaultValue = defJson.get<int>();
                }
                break;
        }
    }

    return desc;
}

nlohmann::json PropertyDescriptor::GetDefaultAsJson() const {
    return std::visit([](auto&& arg) -> nlohmann::json {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, std::monostate>) {
            return nullptr;
        }
        else if constexpr (std::is_same_v<T, math::Vec2>) {
            return nlohmann::json{{"x", arg.x}, {"y", arg.y}};
        }
        else if constexpr (std::is_same_v<T, math::Vec3>) {
            return nlohmann::json{{"x", arg.x}, {"y", arg.y}, {"z", arg.z}};
        }
        else if constexpr (std::is_same_v<T, math::Vec4>) {
            return nlohmann::json{{"x", arg.x}, {"y", arg.y}, {"z", arg.z}, {"w", arg.w}};
        }
        else if constexpr (std::is_same_v<T, math::Color>) {
            return nlohmann::json{{"r", arg.r}, {"g", arg.g}, {"b", arg.b}, {"a", arg.a}};
        }
        else {
            return arg;
        }
    }, defaultValue);
}

}
}
