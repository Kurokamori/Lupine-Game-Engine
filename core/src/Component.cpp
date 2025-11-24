#include "lupine/core/Component.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/PropertyDescriptor.hpp"
#include "lupine/logger/Logger.hpp"

namespace lupine {
namespace core {

Component::Component()
    : m_UUID(), m_Name("Component"), m_Owner(nullptr), m_Enabled(true), m_PropertiesDefined(false) {
}

Component::Component(const std::string& name)
    : m_UUID(), m_Name(name), m_Owner(nullptr), m_Enabled(true), m_PropertiesDefined(false) {
}

Component::~Component() {
}

void Component::RegisterProperties() {

    m_Properties.clear();

    RegisterProperty<std::string>("name", core::PropertyType::String,
        [this]() { return m_Name; },
        [this](const std::string& value) { m_Name = value; });

    RegisterProperty<bool>("enabled", core::PropertyType::Bool,
        [this]() { return m_Enabled; },
        [this](const bool& value) { m_Enabled = value; });

    if (!m_PropertiesDefined) {
        DefineProperties();
        m_PropertiesDefined = true;
    }

    auto descriptors = m_CustomProperties.GetPropertyDescriptors();
    for (const auto& desc : descriptors) {

        switch (desc.type) {
            case PropertyValueType::Bool: {
                auto prop = m_CustomProperties.GetProperty(desc.name);
                if (prop) {
                    PropertyDescriptor metadata = desc;
                    RegisterPropertyWithMetadata<bool>(desc.name, PropertyType::Bool,
                        [this, name = desc.name]() {
                            auto p = m_CustomProperties.GetProperty(name);
                            return p ? p->GetValue<bool>() : false;
                        },
                        [this, name = desc.name](const bool& value) {
                            auto p = m_CustomProperties.GetProperty(name);
                            if (p) p->SetValue(value);
                        },
                        metadata);
                }
                break;
            }
            case PropertyValueType::Int: {
                auto prop = m_CustomProperties.GetProperty(desc.name);
                if (prop) {
                    PropertyDescriptor metadata = desc;
                    RegisterPropertyWithMetadata<int>(desc.name, PropertyType::Int,
                        [this, name = desc.name]() {
                            auto p = m_CustomProperties.GetProperty(name);
                            return p ? p->GetValue<int>() : 0;
                        },
                        [this, name = desc.name](const int& value) {
                            auto p = m_CustomProperties.GetProperty(name);
                            if (p) p->SetValue(value);
                        },
                        metadata);
                }
                break;
            }
            case PropertyValueType::Float: {
                auto prop = m_CustomProperties.GetProperty(desc.name);
                if (prop) {
                    PropertyDescriptor metadata = desc;
                    RegisterPropertyWithMetadata<float>(desc.name, PropertyType::Float,
                        [this, name = desc.name]() {
                            auto p = m_CustomProperties.GetProperty(name);
                            return p ? p->GetValue<float>() : 0.0f;
                        },
                        [this, name = desc.name](const float& value) {
                            auto p = m_CustomProperties.GetProperty(name);
                            if (p) p->SetValue(value);
                        },
                        metadata);
                }
                break;
            }
            case PropertyValueType::String: {
                auto prop = m_CustomProperties.GetProperty(desc.name);
                if (prop) {
                    PropertyDescriptor metadata = desc;
                    RegisterPropertyWithMetadata<std::string>(desc.name, PropertyType::String,
                        [this, name = desc.name]() {
                            auto p = m_CustomProperties.GetProperty(name);
                            return p ? p->GetValue<std::string>() : std::string("");
                        },
                        [this, name = desc.name](const std::string& value) {
                            auto p = m_CustomProperties.GetProperty(name);
                            if (p) p->SetValue(value);
                        },
                        metadata);
                }
                break;
            }
            case PropertyValueType::Vec2: {
                auto prop = m_CustomProperties.GetProperty(desc.name);
                if (prop) {
                    PropertyDescriptor metadata = desc;
                    RegisterPropertyWithMetadata<math::Vec2>(desc.name, PropertyType::Vec2,
                        [this, name = desc.name]() {
                            auto p = m_CustomProperties.GetProperty(name);
                            return p ? p->GetValue<math::Vec2>() : math::Vec2();
                        },
                        [this, name = desc.name](const math::Vec2& value) {
                            auto p = m_CustomProperties.GetProperty(name);
                            if (p) p->SetValue(value);
                        },
                        metadata);
                }
                break;
            }
            case PropertyValueType::Vec3: {
                auto prop = m_CustomProperties.GetProperty(desc.name);
                if (prop) {
                    PropertyDescriptor metadata = desc;
                    RegisterPropertyWithMetadata<math::Vec3>(desc.name, PropertyType::Vec3,
                        [this, name = desc.name]() {
                            auto p = m_CustomProperties.GetProperty(name);
                            return p ? p->GetValue<math::Vec3>() : math::Vec3();
                        },
                        [this, name = desc.name](const math::Vec3& value) {
                            auto p = m_CustomProperties.GetProperty(name);
                            if (p) p->SetValue(value);
                        },
                        metadata);
                }
                break;
            }
            case PropertyValueType::Vec4: {
                auto prop = m_CustomProperties.GetProperty(desc.name);
                if (prop) {
                    PropertyDescriptor metadata = desc;
                    RegisterPropertyWithMetadata<math::Vec4>(desc.name, PropertyType::Vec4,
                        [this, name = desc.name]() {
                            auto p = m_CustomProperties.GetProperty(name);
                            return p ? p->GetValue<math::Vec4>() : math::Vec4();
                        },
                        [this, name = desc.name](const math::Vec4& value) {
                            auto p = m_CustomProperties.GetProperty(name);
                            if (p) p->SetValue(value);
                        },
                        metadata);
                }
                break;
            }
            case PropertyValueType::Color: {
                auto prop = m_CustomProperties.GetProperty(desc.name);
                if (prop) {
                    PropertyDescriptor metadata = desc;
                    RegisterPropertyWithMetadata<math::Color>(desc.name, PropertyType::Color,
                        [this, name = desc.name]() {
                            auto p = m_CustomProperties.GetProperty(name);
                            return p ? p->GetValue<math::Color>() : math::Color();
                        },
                        [this, name = desc.name](const math::Color& value) {
                            auto p = m_CustomProperties.GetProperty(name);
                            if (p) p->SetValue(value);
                        },
                        metadata);
                }
                break;
            }
            case PropertyValueType::Enum: {
                auto prop = m_CustomProperties.GetProperty(desc.name);
                if (prop) {
                    PropertyDescriptor metadata = desc;
                    RegisterPropertyWithMetadata<int>(desc.name, PropertyType::Int,
                        [this, name = desc.name]() {
                            auto p = m_CustomProperties.GetProperty(name);
                            return p ? p->GetValue<int>() : 0;
                        },
                        [this, name = desc.name](const int& value) {
                            auto p = m_CustomProperties.GetProperty(name);
                            if (p) p->SetValue(value);
                        },
                        metadata);
                }
                break;
            }
            default:

                break;
        }
    }
}

nlohmann::json Component::Serialize() const {
    nlohmann::json json;
    json["type"] = GetTypeName();

    if (!m_CustomProperties.GetProperties().empty()) {
        json["properties"] = m_CustomProperties.SerializeProperties();
    } else {

        json = core::ISerializable::Serialize();
    }

    return json;
}

void Component::Deserialize(const nlohmann::json& json) {

    if (!m_PropertiesDefined) {
        DefineProperties();
        m_PropertiesDefined = true;
    }

    if (!m_CustomProperties.GetProperties().empty()) {

        if (json.contains("properties") && json["properties"].is_object()) {
            m_CustomProperties.DeserializeProperties(json["properties"]);
        }

        else if (json.contains("custom_properties")) {
            m_CustomProperties.DeserializeProperties(json["custom_properties"]);
        }

        if (json.contains("properties")) {
            const auto& props = json["properties"];
            if (props.contains("enabled") && props["enabled"].is_boolean()) {
                m_Enabled = props["enabled"].get<bool>();
            }
        }
    } else {

        core::ISerializable::Deserialize(json);
    }
}

std::vector<core::PropertyDescriptor> Component::GetPropertyDescriptors() const {
    return m_CustomProperties.GetPropertyDescriptors();
}

}
}

