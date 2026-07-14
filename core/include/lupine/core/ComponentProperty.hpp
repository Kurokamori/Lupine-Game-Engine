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

    // Remove a property by name
    void RemoveProperty(const std::string& name) {
        m_Properties.erase(name);
    }

    // Clear all properties
    void Clear() {
        m_Properties.clear();
    }

    // Clear all properties except specified ones
    void ClearExcept(const std::vector<std::string>& keepProperties) {
        std::unordered_map<std::string, ComponentProperty> kept;
        for (const auto& name : keepProperties) {
            auto it = m_Properties.find(name);
            if (it != m_Properties.end()) {
                kept[name] = it->second;
            }
        }
        m_Properties = std::move(kept);
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

// Read a numeric field from a json object without ever throwing: a missing key,
// a non-object container, or a non-numeric value all fall back to `fallback`.
// Property json can legitimately be null (never assigned) or the wrong shape
// (set from script with a malformed table); GetValue must degrade gracefully
// rather than raise an nlohmann type_error that propagates out of the render /
// process walk and aborts the process (std::terminate on an unhandled throw).
inline float ComponentPropertyJsonNumber(const nlohmann::json& j, const char* key, float fallback) {
    auto it = j.find(key);
    if (it != j.end() && it->is_number()) {
        return it->get<float>();
    }
    return fallback;
}

// Template implementations
template<typename T>
T ComponentProperty::GetValue() const {
    if constexpr (std::is_same_v<T, int>) {
        return m_CurrentValue.is_number() ? m_CurrentValue.get<int>() : 0;
    } else if constexpr (std::is_same_v<T, float>) {
        return m_CurrentValue.is_number() ? m_CurrentValue.get<float>() : 0.0f;
    } else if constexpr (std::is_same_v<T, std::string>) {
        return m_CurrentValue.is_string() ? m_CurrentValue.get<std::string>() : std::string();
    } else if constexpr (std::is_same_v<T, bool>) {
        return m_CurrentValue.is_boolean() ? m_CurrentValue.get<bool>() : false;
    } else if constexpr (std::is_same_v<T, math::Vec2>) {
        math::Vec2 v;
        v.x = ComponentPropertyJsonNumber(m_CurrentValue, "x", 0.0f);
        v.y = ComponentPropertyJsonNumber(m_CurrentValue, "y", 0.0f);
        return v;
    } else if constexpr (std::is_same_v<T, math::Vec3>) {
        math::Vec3 v;
        v.x = ComponentPropertyJsonNumber(m_CurrentValue, "x", 0.0f);
        v.y = ComponentPropertyJsonNumber(m_CurrentValue, "y", 0.0f);
        v.z = ComponentPropertyJsonNumber(m_CurrentValue, "z", 0.0f);
        return v;
    } else if constexpr (std::is_same_v<T, math::Vec4>) {
        math::Vec4 v;
        v.x = ComponentPropertyJsonNumber(m_CurrentValue, "x", 0.0f);
        v.y = ComponentPropertyJsonNumber(m_CurrentValue, "y", 0.0f);
        v.z = ComponentPropertyJsonNumber(m_CurrentValue, "z", 0.0f);
        v.w = ComponentPropertyJsonNumber(m_CurrentValue, "w", 0.0f);
        return v;
    } else if constexpr (std::is_same_v<T, math::Color>) {
        // Missing channels fall back to opaque white (the conventional "no tint"),
        // so a null/partial color never crashes and renders sensibly.
        math::Color c;
        c.r = ComponentPropertyJsonNumber(m_CurrentValue, "r", 1.0f);
        c.g = ComponentPropertyJsonNumber(m_CurrentValue, "g", 1.0f);
        c.b = ComponentPropertyJsonNumber(m_CurrentValue, "b", 1.0f);
        c.a = ComponentPropertyJsonNumber(m_CurrentValue, "a", 1.0f);
        return c;
    } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
        std::vector<std::string> arr;
        if (m_CurrentValue.is_array()) {
            for (const auto& item : m_CurrentValue) {
                if (item.is_string()) {
                    arr.push_back(item.get<std::string>());
                }
            }
        }
        return arr;
    } else {
        // Only instantiated for types the chain above does not handle; keeping it in
        // the else branch stops it being compiled (and reported unreachable) for the
        // handled ones.
        return T{};
    }
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
    } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& str : value) {
            arr.push_back(str);
        }
        m_CurrentValue = arr;
    }
}

} // namespace core
} // namespace lupine
