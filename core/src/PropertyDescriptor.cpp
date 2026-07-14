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

    // Extended editor metadata - emitted only when set so existing files and the
    // common case stay free of empty keys.
    if (usageFlags != 0) {
        json["usage"] = usageFlags;
    }
    if (!headerText.empty()) {
        json["header"] = headerText;
    }
    if (!suffix.empty()) {
        json["suffix"] = suffix;
    }
    if (!customWidget.empty()) {
        json["custom_widget"] = customWidget;
    }
    if (!customWidgetConfig.empty()) {
        json["custom_widget_config"] = customWidgetConfig;
    }
    if (!objectTypeName.empty()) {
        json["object_type"] = objectTypeName;
    }
    if (!objectSchema.empty()) {
        nlohmann::json schema = nlohmann::json::array();
        for (const PropertyDescriptor& field : objectSchema) {
            schema.push_back(field.Serialize());
        }
        json["object_schema"] = schema;
    }
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

    // Extended editor metadata (all optional)
    if (json.contains("usage") && json["usage"].is_number_integer()) {
        desc.usageFlags = json["usage"].get<uint32_t>();
    }
    if (json.contains("header") && json["header"].is_string()) {
        desc.headerText = json["header"].get<std::string>();
    }
    if (json.contains("suffix") && json["suffix"].is_string()) {
        desc.suffix = json["suffix"].get<std::string>();
    }
    if (json.contains("custom_widget") && json["custom_widget"].is_string()) {
        desc.customWidget = json["custom_widget"].get<std::string>();
    }
    if (json.contains("custom_widget_config") && json["custom_widget_config"].is_string()) {
        desc.customWidgetConfig = json["custom_widget_config"].get<std::string>();
    }
    if (json.contains("object_type") && json["object_type"].is_string()) {
        desc.objectTypeName = json["object_type"].get<std::string>();
    }
    if (json.contains("object_schema") && json["object_schema"].is_array()) {
        for (const nlohmann::json& fieldJson : json["object_schema"]) {
            desc.objectSchema.push_back(PropertyDescriptor::Deserialize(fieldJson));
        }
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

            case PropertyValueType::StringArray:
                if (defJson.is_array()) {
                    std::vector<std::string> arr;
                    for (const auto& item : defJson) {
                        if (item.is_string()) {
                            arr.push_back(item.get<std::string>());
                        }
                    }
                    desc.defaultValue = arr;
                }
                break;

            case PropertyValueType::Double:
                if (defJson.is_number()) {
                    desc.defaultValue = defJson.get<double>();
                }
                break;

            case PropertyValueType::Quat:
                if (defJson.is_object() && defJson.contains("w") && defJson.contains("x") &&
                    defJson.contains("y") && defJson.contains("z")) {
                    desc.defaultValue = math::Quat(
                        defJson["w"].get<float>(), defJson["x"].get<float>(),
                        defJson["y"].get<float>(), defJson["z"].get<float>());
                }
                break;

            case PropertyValueType::Rect:
                if (defJson.is_object() && defJson.contains("x") && defJson.contains("y") &&
                    defJson.contains("w") && defJson.contains("h")) {
                    desc.defaultValue = math::Rect(
                        defJson["x"].get<float>(), defJson["y"].get<float>(),
                        defJson["w"].get<float>(), defJson["h"].get<float>());
                }
                break;

            case PropertyValueType::Resource:
                if (defJson.is_string()) {
                    desc.defaultValue = defJson.get<std::string>();
                }
                break;

            case PropertyValueType::IntArray:
                if (defJson.is_array()) {
                    nlohmann::json arr = nlohmann::json::array();
                    for (const auto& item : defJson) {
                        if (item.is_number_integer()) {
                            arr.push_back(item.get<int>());
                        } else if (item.is_number()) {
                            arr.push_back(static_cast<int>(item.get<double>()));
                        }
                    }
                    desc.defaultValue = PropertyJsonDefault(arr);
                }
                break;

            case PropertyValueType::FloatArray:
                if (defJson.is_array()) {
                    nlohmann::json arr = nlohmann::json::array();
                    for (const auto& item : defJson) {
                        if (item.is_number()) {
                            arr.push_back(item.get<float>());
                        }
                    }
                    desc.defaultValue = PropertyJsonDefault(arr);
                }
                break;

            case PropertyValueType::Array:
                if (defJson.is_array()) {
                    desc.defaultValue = PropertyJsonDefault(defJson);
                }
                break;

            case PropertyValueType::Dictionary:
                if (defJson.is_object()) {
                    desc.defaultValue = PropertyJsonDefault(defJson);
                }
                break;
        }
    }

    return desc;
}

nlohmann::json PropertyDescriptor::GetDefaultAsJson() const {
    nlohmann::json result = std::visit([](auto&& arg) -> nlohmann::json {
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
        else if constexpr (std::is_same_v<T, math::Quat>) {
            return nlohmann::json{{"w", arg.w()}, {"x", arg.x()}, {"y", arg.y()}, {"z", arg.z()}};
        }
        else if constexpr (std::is_same_v<T, math::Rect>) {
            return nlohmann::json{{"x", arg.position.x}, {"y", arg.position.y},
                                  {"w", arg.size.x}, {"h", arg.size.y}};
        }
        else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& str : arg) {
                arr.push_back(str);
            }
            return arr;
        }
        else if constexpr (std::is_same_v<T, PropertyJsonDefault>) {
            return arg.value;
        }
        else {
            return arg;
        }
    }, defaultValue);

    // When no explicit default was authored, give container types a sensible
    // empty value so instances and the inspector see a list/map, not null.
    if (result.is_null()) {
        switch (type) {
            case PropertyValueType::StringArray:
            case PropertyValueType::IntArray:
            case PropertyValueType::FloatArray:
            case PropertyValueType::Array:
                result = nlohmann::json::array();
                break;
            case PropertyValueType::Dictionary:
                result = nlohmann::json::object();
                break;
            default:
                break;
        }
    }

    return result;
}

}
}
