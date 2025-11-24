#pragma once

#include <string>
#include <vector>
#include <variant>
#include <optional>
#include <map>
#include <nlohmann/json.hpp>
#include "lupine/math/Math.hpp"

namespace lupine {
namespace core {

/**
 * Property value types supported by the component system
 */
enum class PropertyValueType {
    Int,
    Float,
    String,
    Bool,
    Vec2,
    Vec3,
    Vec4,
    Color,
    NodePath,     // Reference to a node in the scene tree
    ScenePath,    // Reference to a scene file
    Enum          // Enumeration with specific allowed values
};

/**
 * Property hint types that provide additional metadata for the editor
 */
enum class PropertyHintType {
    None,
    Range,           // Min and max values (for Int/Float)
    Enum,            // List of allowed values (for Enum type)
    File,            // File path filter (for String)
    MultilineText,   // Multi-line text editor (for String)
    ExpRange,        // Exponential range (for Float)
    Length,          // String length constraint
    ColorNoAlpha,    // Color without alpha channel
    Dir,             // Directory path (for String)
    Layers2D,        // 2D physics/render layers
    Layers3D         // 3D physics/render layers
};

/**
 * Property hint data - stores additional metadata about a property
 */
struct PropertyHint {
    PropertyHintType type = PropertyHintType::None;
    std::string hintString;  // Comma-separated values for Enum, "min,max,step" for Range, etc.

    PropertyHint() = default;
    PropertyHint(PropertyHintType t, const std::string& hint = "")
        : type(t), hintString(hint) {}

    // Parse hint string based on type
    std::vector<std::string> GetEnumValues() const;
    void GetRangeValues(float& min, float& max, float& step) const;
    void GetRangeValues(int& min, int& max, int& step) const;

    nlohmann::json Serialize() const;
    static PropertyHint Deserialize(const nlohmann::json& json);
};

/**
 * Default value storage using std::variant
 */
using PropertyDefaultValue = std::variant<
    std::monostate,      // No default value
    int,
    float,
    std::string,
    bool,
    math::Vec2,
    math::Vec3,
    math::Vec4,
    math::Color
>;

/**
 * Property descriptor - describes a property with its type, default value, and hints
 * This is the declarative metadata for properties that can be defined by users
 */
struct PropertyDescriptor {
    std::string name;
    PropertyValueType type;
    PropertyDefaultValue defaultValue;
    PropertyHint hint;
    std::string description;  // Optional description for documentation/tooltips
    std::string group;        // Optional group name for organizing properties in the editor

    PropertyDescriptor() = default;

    PropertyDescriptor(const std::string& propName, PropertyValueType propType)
        : name(propName), type(propType), defaultValue(std::monostate{}), hint(), description() {}

    PropertyDescriptor(const std::string& propName, PropertyValueType propType, 
                       PropertyDefaultValue propDefault)
        : name(propName), type(propType), defaultValue(propDefault), hint(), description() {}

    PropertyDescriptor(const std::string& propName, PropertyValueType propType,
                       PropertyDefaultValue propDefault, PropertyHint propHint)
        : name(propName), type(propType), defaultValue(propDefault), hint(propHint), description(), group() {}

    PropertyDescriptor(const std::string& propName, PropertyValueType propType,
                       PropertyDefaultValue propDefault, PropertyHint propHint,
                       const std::string& propGroup)
        : name(propName), type(propType), defaultValue(propDefault), hint(propHint), description(), group(propGroup) {}

    // Serialize descriptor to JSON
    nlohmann::json Serialize() const;

    // Deserialize descriptor from JSON
    static PropertyDescriptor Deserialize(const nlohmann::json& json);

    // Get default value as JSON
    nlohmann::json GetDefaultAsJson() const;
};

/**
 * Helper macros for defining properties with type safety
 */

// Define a simple property (type only)
#define PROPERTY(name, type) \
    lupine::core::PropertyDescriptor(#name, lupine::core::PropertyValueType::type)

// Define a property with default value
#define PROPERTY_DEFAULT(name, type, defaultVal) \
    lupine::core::PropertyDescriptor(#name, lupine::core::PropertyValueType::type, defaultVal)

// Define a property with default and hint
#define PROPERTY_HINT(name, type, defaultVal, hintType, hintStr) \
    lupine::core::PropertyDescriptor(#name, lupine::core::PropertyValueType::type, defaultVal, \
        lupine::core::PropertyHint(lupine::core::PropertyHintType::hintType, hintStr))

// Define a property with default, hint, and group
#define PROPERTY_GROUP(name, type, defaultVal, hintType, hintStr, groupName) \
    lupine::core::PropertyDescriptor(#name, lupine::core::PropertyValueType::type, defaultVal, \
        lupine::core::PropertyHint(lupine::core::PropertyHintType::hintType, hintStr), groupName)

// Specialized macros for common patterns

// Int property with range
#define PROPERTY_INT_RANGE(name, defaultVal, min, max, step) \
    PROPERTY_HINT(name, Int, defaultVal, Range, std::to_string(min) + "," + std::to_string(max) + "," + std::to_string(step))

// Float property with range
#define PROPERTY_FLOAT_RANGE(name, defaultVal, min, max, step) \
    PROPERTY_HINT(name, Float, defaultVal, Range, std::to_string(min) + "," + std::to_string(max) + "," + std::to_string(step))

// Enum property
#define PROPERTY_ENUM(name, defaultVal, ...) \
    PROPERTY_HINT(name, Enum, defaultVal, Enum, std::string(#__VA_ARGS__))

// File path property
#define PROPERTY_FILE(name, defaultVal, filter) \
    PROPERTY_HINT(name, String, defaultVal, File, filter)

// File path property with group
#define PROPERTY_FILE_GROUP(name, defaultVal, filter, groupName) \
    PROPERTY_GROUP(name, String, defaultVal, File, filter, groupName)

// Default property with group
#define PROPERTY_DEFAULT_GROUP(name, type, defaultVal, groupName) \
    lupine::core::PropertyDescriptor(#name, lupine::core::PropertyValueType::type, defaultVal, \
        lupine::core::PropertyHint(), groupName)

// Float property with range and group
#define PROPERTY_FLOAT_RANGE_GROUP(name, defaultVal, min, max, step, groupName) \
    lupine::core::PropertyDescriptor(#name, lupine::core::PropertyValueType::Float, defaultVal, \
        lupine::core::PropertyHint(lupine::core::PropertyHintType::Range, \
        std::to_string(min) + "," + std::to_string(max) + "," + std::to_string(step)), groupName)

// Int property with range and group
#define PROPERTY_INT_RANGE_GROUP(name, defaultVal, min, max, step, groupName) \
    lupine::core::PropertyDescriptor(#name, lupine::core::PropertyValueType::Int, defaultVal, \
        lupine::core::PropertyHint(lupine::core::PropertyHintType::Range, \
        std::to_string(min) + "," + std::to_string(max) + "," + std::to_string(step)), groupName)

// ENUM property with group
#define PROPERTY_ENUM_GROUP(name, defaultVal, groupName, ...) \
    lupine::core::PropertyDescriptor(#name, lupine::core::PropertyValueType::Enum, defaultVal, \
        lupine::core::PropertyHint(lupine::core::PropertyHintType::Enum, std::string(#__VA_ARGS__)), groupName)

} // namespace core
} // namespace lupine
