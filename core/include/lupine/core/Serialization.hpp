#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <typeindex>
#include <nlohmann/json.hpp>
#include "lupine/core/PropertyDescriptor.hpp"

namespace lupine {
namespace core {

// Forward declarations
class ISerializable;
class Property;

/**
 * Property types supported by the serialization system
 */
enum class PropertyType {
    Bool,
    Int,
    Float,
    Double,
    String,
    Vec2,
    Vec3,
    Vec4,
    Color,
    Object,      // Reference to another serializable object
    Array,       // Array of values
    Custom       // Custom serialization
};

/**
 * Base class for property metadata
 * Properties are values that can be exported/serialized
 */
class Property {
public:
    Property(const std::string& name, PropertyType type)
        : m_Name(name), m_Type(type) {}
    
    virtual ~Property() = default;

    const std::string& GetName() const { return m_Name; }
    PropertyType GetType() const { return m_Type; }

    // Serialize the property to JSON
    virtual nlohmann::json Serialize() const = 0;
    
    // Deserialize the property from JSON
    virtual void Deserialize(const nlohmann::json& json) = 0;
    
    // Get property descriptor (metadata)
    virtual PropertyDescriptor GetDescriptor() const { return PropertyDescriptor(); }

protected:
    std::string m_Name;
    PropertyType m_Type;
};

/**
 * Typed property implementation
 */
template<typename T>
class TypedProperty : public Property {
public:
    using GetterFunc = std::function<T()>;
    using SetterFunc = std::function<void(const T&)>;

    TypedProperty(const std::string& name, PropertyType type, GetterFunc getter, SetterFunc setter)
        : Property(name, type), m_Getter(getter), m_Setter(setter) {}
    
    TypedProperty(const std::string& name, PropertyType type, GetterFunc getter, SetterFunc setter, const PropertyDescriptor& desc)
        : Property(name, type), m_Getter(getter), m_Setter(setter), m_Descriptor(desc) {}

    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& json) override;
    PropertyDescriptor GetDescriptor() const override { return m_Descriptor; }
    
    void SetDescriptor(const PropertyDescriptor& desc) { m_Descriptor = desc; }

private:
    GetterFunc m_Getter;
    SetterFunc m_Setter;
    PropertyDescriptor m_Descriptor;
};

/**
 * Interface for serializable objects
 */
class ISerializable {
public:
    virtual ~ISerializable() = default;

    // Get the type name of this object
    virtual std::string GetTypeName() const = 0;

    // Serialize this object to JSON
    virtual nlohmann::json Serialize() const;
    
    // Serialize with property metadata for editor use
    virtual nlohmann::json SerializeWithMetadata() const;

    // Deserialize this object from JSON
    virtual void Deserialize(const nlohmann::json& json);

    // Register properties for serialization
    virtual void RegisterProperties() = 0;

protected:
    // Helper methods to register properties
    template<typename T>
    void RegisterProperty(const std::string& name, PropertyType type,
                         std::function<T()> getter,
                         std::function<void(const T&)> setter);
    
    // Register property with metadata
    template<typename T>
    void RegisterPropertyWithMetadata(const std::string& name, PropertyType type,
                                     std::function<T()> getter,
                                     std::function<void(const T&)> setter,
                                     const PropertyDescriptor& descriptor);

    std::vector<std::shared_ptr<Property>> m_Properties;
};

/**
 * Serialization utility class
 */
class Serializer {
public:
    // Serialize an object to JSON string
    static std::string SerializeToString(const ISerializable& object, int indent = -1);

    // Deserialize an object from JSON string
    static bool DeserializeFromString(ISerializable& object, const std::string& jsonStr);

    // Serialize an object to JSON
    static nlohmann::json SerializeToJson(const ISerializable& object);

    // Deserialize an object from JSON
    static bool DeserializeFromJson(ISerializable& object, const nlohmann::json& json);

    // Save serialized object to file
    static bool SaveToFile(const ISerializable& object, const std::string& filepath, int indent = 2);

    // Load and deserialize object from file
    static bool LoadFromFile(ISerializable& object, const std::string& filepath);
};

/**
 * Type registry for creating objects by type name
 */
class TypeRegistry {
public:
    using FactoryFunc = std::function<std::shared_ptr<ISerializable>()>;

    static TypeRegistry& GetInstance();

    // Register a type with a factory function
    void RegisterType(const std::string& typeName, FactoryFunc factory);

    // Create an instance of a registered type
    std::shared_ptr<ISerializable> CreateInstance(const std::string& typeName);

    // Check if a type is registered
    bool IsTypeRegistered(const std::string& typeName) const;

    // Enumerate all registered type names (for reflection / language bindings)
    std::vector<std::string> GetRegisteredTypeNames() const;

private:
    TypeRegistry() = default;
    std::unordered_map<std::string, FactoryFunc> m_Factories;
};

} // namespace core
} // namespace lupine

