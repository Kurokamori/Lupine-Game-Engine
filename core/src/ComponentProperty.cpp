#include "lupine/core/ComponentProperty.hpp"
#include "lupine/logger/Logger.hpp"

namespace lupine {
namespace core {

void ComponentProperty::InitializeFromDefault() {
    m_CurrentValue = m_Descriptor.GetDefaultAsJson();

    if (m_CurrentValue.is_null()) {
        switch (m_Descriptor.type) {
            case core::PropertyValueType::Int:
            case core::PropertyValueType::Enum:
                m_CurrentValue = 0;
                break;
            case core::PropertyValueType::Float:
                m_CurrentValue = 0.0f;
                break;
            case core::PropertyValueType::String:
            case core::PropertyValueType::NodePath:
            case core::PropertyValueType::ScenePath:
            case core::PropertyValueType::Resource:
                m_CurrentValue = "";
                break;
            case core::PropertyValueType::Bool:
                m_CurrentValue = false;
                break;
            case core::PropertyValueType::Double:
                m_CurrentValue = 0.0;
                break;
            case core::PropertyValueType::Vec2:
                m_CurrentValue = nlohmann::json{{"x", 0.0f}, {"y", 0.0f}};
                break;
            case core::PropertyValueType::Vec3:
                m_CurrentValue = nlohmann::json{{"x", 0.0f}, {"y", 0.0f}, {"z", 0.0f}};
                break;
            case core::PropertyValueType::Vec4:
                m_CurrentValue = nlohmann::json{{"x", 0.0f}, {"y", 0.0f}, {"z", 0.0f}, {"w", 0.0f}};
                break;
            case core::PropertyValueType::Quat:
                m_CurrentValue = nlohmann::json{{"w", 1.0f}, {"x", 0.0f}, {"y", 0.0f}, {"z", 0.0f}};
                break;
            case core::PropertyValueType::Rect:
                m_CurrentValue = nlohmann::json{{"x", 0.0f}, {"y", 0.0f}, {"w", 0.0f}, {"h", 0.0f}};
                break;
            case core::PropertyValueType::Color:
                m_CurrentValue = nlohmann::json{{"r", 1.0f}, {"g", 1.0f}, {"b", 1.0f}, {"a", 1.0f}};
                break;
            case core::PropertyValueType::StringArray:
            case core::PropertyValueType::IntArray:
            case core::PropertyValueType::FloatArray:
            case core::PropertyValueType::Array:
                m_CurrentValue = nlohmann::json::array();
                break;
            case core::PropertyValueType::Dictionary:
                m_CurrentValue = nlohmann::json::object();
                break;
        }
    }
}

nlohmann::json ComponentProperty::Serialize() const {
    nlohmann::json json;
    json["descriptor"] = m_Descriptor.Serialize();
    json["value"] = m_CurrentValue;
    return json;
}

void ComponentProperty::Deserialize(const nlohmann::json& json) {
    if (json.contains("descriptor")) {
        m_Descriptor = core::PropertyDescriptor::Deserialize(json["descriptor"]);
    }

    if (json.contains("value")) {
        m_CurrentValue = json["value"];
    } else {
        InitializeFromDefault();
    }
}

void ComponentPropertyRegistry::DefineProperty(const core::PropertyDescriptor& desc) {
    m_Properties[desc.name] = ComponentProperty(desc);
}

ComponentProperty* ComponentPropertyRegistry::GetProperty(const std::string& name) {
    auto it = m_Properties.find(name);
    if (it != m_Properties.end()) {
        return &it->second;
    }
    return nullptr;
}

const ComponentProperty* ComponentPropertyRegistry::GetProperty(const std::string& name) const {
    auto it = m_Properties.find(name);
    if (it != m_Properties.end()) {
        return &it->second;
    }
    return nullptr;
}

nlohmann::json ComponentPropertyRegistry::SerializeProperties() const {
    nlohmann::json json = nlohmann::json::object();

    for (const auto& [name, prop] : m_Properties) {
        json[name] = prop.GetValueAsJson();
    }

    return json;
}

void ComponentPropertyRegistry::DeserializeProperties(const nlohmann::json& json) {
    if (!json.is_object()) {
        return;
    }

    for (auto& [name, prop] : m_Properties) {
        if (json.contains(name)) {
            prop.SetValueFromJson(json[name]);
        }
    }
}

std::vector<core::PropertyDescriptor> ComponentPropertyRegistry::GetPropertyDescriptors() const {
    std::vector<core::PropertyDescriptor> descriptors;
    descriptors.reserve(m_Properties.size());

    for (const auto& [name, prop] : m_Properties) {
        descriptors.push_back(prop.GetDescriptor());
    }

    return descriptors;
}

}
}
