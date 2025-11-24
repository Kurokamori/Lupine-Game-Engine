#pragma once

#include "lupine/core/PropertyDescriptor.hpp"
#include <string>
#include <unordered_map>
#include <memory>
#include <functional>
#include <any>

namespace lupine {
namespace core {

/**
 * Component property - combines a descriptor with actual runtime value storage
 * This allows components to define properties declaratively while maintaining
 * type-safe getter/setter functionality
 */
class ComponentProperty {
public:
    ComponentProperty() = default;
    
    ComponentProperty(const PropertyDescriptor& desc)
        : m_Descriptor(desc) {
        InitializeFromDefault();
    }

    const PropertyDescriptor& GetDescriptor() const { return m_Descriptor; }
    const std::string& GetName() const { return m_Descriptor.name; }
    PropertyValueType GetType() const { return m_Descriptor.type; }

    // Get value as JSON
    nlohmann::json GetValueAsJson() const { return m_CurrentValue; }
    
    // Set value from JSON
    void SetValueFromJson(const nlohmann::json& json) { m_CurrentValue = json; }

    // Get value by type (with type checking)
    template<typename T>
    T GetValue() const;

    // Set value by type (with type checking)
    template<typename T>
    void SetValue(const T& value);

    // Reset to default value
    void ResetToDefault() { InitializeFromDefault(); }

    // Serialize the property (descriptor + current value)
    nlohmann::json Serialize() const;

    // Deserialize the property
    void Deserialize(const nlohmann::json& json);

private:
    PropertyDescriptor m_Descriptor;
    nlohmann::json m_CurrentValue;  // Store current value as JSON for flexibility

    void InitializeFromDefault();
};

/**
 * Property registry for components
 * Manages the collection of properties for a component instance
 */
class ComponentPropertyRegistry {
public:
    ComponentPropertyRegistry() = default;

    // Define a property from a descriptor
    void DefineProperty(const PropertyDescriptor& desc);

    // Get property by name
    ComponentProperty* GetProperty(const std::string& name);
    const ComponentProperty* GetProperty(const std::string& name) const;

    // Get all properties
    const std::unordered_map<std::string, ComponentProperty>& GetProperties() const {
        return m_Properties;
    }

    // Check if property exists
    bool HasProperty(const std::string& name) const {
        return m_Properties.find(name) != m_Properties.end();
    }

    // Serialize all properties
    nlohmann::json SerializeProperties() const;

    // Deserialize all properties
    void DeserializeProperties(const nlohmann::json& json);

    // Get property descriptors (for editor/reflection)
    std::vector<core::PropertyDescriptor> GetPropertyDescriptors() const;

private:
    std::unordered_map<std::string, ComponentProperty> m_Properties;
};

// Template implementations
template<typename T>
T ComponentProperty::GetValue() const {
    if constexpr (std::is_same_v<T, int>) {
        return m_CurrentValue.get<int>();
    } else if constexpr (std::is_same_v<T, float>) {
        return m_CurrentValue.get<float>();
    } else if constexpr (std::is_same_v<T, std::string>) {
        return m_CurrentValue.get<std::string>();
    } else if constexpr (std::is_same_v<T, bool>) {
        return m_CurrentValue.get<bool>();
    } else if constexpr (std::is_same_v<T, math::Vec2>) {
        math::Vec2 v;
        v.x = m_CurrentValue["x"].get<float>();
        v.y = m_CurrentValue["y"].get<float>();
        return v;
    } else if constexpr (std::is_same_v<T, math::Vec3>) {
        math::Vec3 v;
        v.x = m_CurrentValue["x"].get<float>();
        v.y = m_CurrentValue["y"].get<float>();
        v.z = m_CurrentValue["z"].get<float>();
        return v;
    } else if constexpr (std::is_same_v<T, math::Vec4>) {
        math::Vec4 v;
        v.x = m_CurrentValue["x"].get<float>();
        v.y = m_CurrentValue["y"].get<float>();
        v.z = m_CurrentValue["z"].get<float>();
        v.w = m_CurrentValue["w"].get<float>();
        return v;
    } else if constexpr (std::is_same_v<T, math::Color>) {
        math::Color c;
        c.r = m_CurrentValue["r"].get<float>();
        c.g = m_CurrentValue["g"].get<float>();
        c.b = m_CurrentValue["b"].get<float>();
        c.a = m_CurrentValue["a"].get<float>();
        return c;
    }
    return T{};
}

template<typename T>
void ComponentProperty::SetValue(const T& value) {
    if constexpr (std::is_same_v<T, int> || std::is_same_v<T, float> || 
                  std::is_same_v<T, std::string> || std::is_same_v<T, bool>) {
        m_CurrentValue = value;
    } else if constexpr (std::is_same_v<T, math::Vec2>) {
        m_CurrentValue = nlohmann::json{{"x", value.x}, {"y", value.y}};
    } else if constexpr (std::is_same_v<T, math::Vec3>) {
        m_CurrentValue = nlohmann::json{{"x", value.x}, {"y", value.y}, {"z", value.z}};
    } else if constexpr (std::is_same_v<T, math::Vec4>) {
        m_CurrentValue = nlohmann::json{{"x", value.x}, {"y", value.y}, {"z", value.z}, {"w", value.w}};
    } else if constexpr (std::is_same_v<T, math::Color>) {
        m_CurrentValue = nlohmann::json{{"r", value.r}, {"g", value.g}, {"b", value.b}, {"a", value.a}};
    }
}

} // namespace core
} // namespace lupine
