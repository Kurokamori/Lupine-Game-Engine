#pragma once

#include <string>
#include <vector>
#include <variant>
#include <optional>
#include <map>
#include <cstdint>
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
    Enum,         // Enumeration with specific allowed values
    StringArray,  // Array of strings
    // --- Extended types (appended; existing integer values above must not change) ---
    Double,       // 64-bit floating point scalar
    Quat,         // Quaternion rotation (w, x, y, z)
    Rect,         // 2D rectangle (x, y, w, h)
    Resource,     // Reference to a resource asset file (e.g. an .ares archetype instance)
    IntArray,     // Array of integers
    FloatArray,   // Array of floats
    Array,        // Generic ordered list of heterogeneous JSON values
    Dictionary    // Generic string-keyed map of heterogeneous JSON values
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
    Layers3D,        // 3D physics/render layers
    // --- Extended hints (appended; existing integer values above must not change) ---
    NodeType,        // NodePath filtered to nodes of/owning a class (hintString = class name)
    ArchetypeType,   // Resource filtered to archetype instances of a class (hintString = archetype class)
    ScriptClass,     // NodePath filtered to nodes owning a script/component class (hintString = class name)
    Flags,           // Bit flags chosen from a named list (hintString = comma-separated flag names)
    ExpEasing        // Easing curve (exponential easing widget)
};

/**
 * Property usage flags - bitmask controlling editor/serialization behaviour.
 * Stored on PropertyDescriptor as a uint32_t so it round-trips as a plain integer.
 */
enum class PropertyUsageFlags : uint32_t {
    None         = 0,
    ReadOnly     = 1u << 0,  // Shown in inspector but not editable
    Hidden       = 1u << 1,  // Hidden from the inspector ([HideInInspector])
    NoSerialize  = 1u << 2,  // Not written to disk (editor/runtime transient)
    Required     = 1u << 3,  // Must have a non-empty value (validated by archetype editor)
    Unique       = 1u << 4,  // Value must be unique among sibling instances (identity field)
    Experimental = 1u << 5,  // Flagged as experimental in the inspector
    Advanced     = 1u << 6   // Collapsed into an "advanced" section by default
};

inline constexpr uint32_t operator|(PropertyUsageFlags a, PropertyUsageFlags b) {
    return static_cast<uint32_t>(a) | static_cast<uint32_t>(b);
}

inline constexpr uint32_t operator|(uint32_t a, PropertyUsageFlags b) {
    return a | static_cast<uint32_t>(b);
}

inline uint32_t& operator|=(uint32_t& a, PropertyUsageFlags b) {
    a |= static_cast<uint32_t>(b);
    return a;
}

inline constexpr bool HasUsageFlag(uint32_t flags, PropertyUsageFlags flag) {
    return (flags & static_cast<uint32_t>(flag)) != 0u;
}

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
 * Wrapper for generic JSON-shaped default values (Array / Dictionary / IntArray /
 * FloatArray). nlohmann::json's converting constructors would otherwise make it a
 * viable overload for the scalar variant alternatives (int/float/double/bool/...)
 * and create ambiguity, so the JSON payload is held behind an explicit wrapper.
 */
struct PropertyJsonDefault {
    nlohmann::json value;

    PropertyJsonDefault() : value(nullptr) {}
    explicit PropertyJsonDefault(nlohmann::json v) : value(std::move(v)) {}
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
    math::Color,
    std::vector<std::string>,  // String array
    double,                    // Double
    math::Quat,                // Quaternion
    math::Rect,                // Rectangle
    PropertyJsonDefault        // Array / Dictionary / IntArray / FloatArray defaults
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

    // --- Extended editor metadata (all optional; absent keys round-trip cleanly) ---
    uint32_t usageFlags = 0;             // Bitmask of PropertyUsageFlags
    std::string headerText;              // Header rendered above this row (distinct from group)
    std::string suffix;                  // Unit suffix shown after the value (e.g. "px", "deg")
    std::string customWidget;            // Custom inspector widget id (empty = default by type)
    std::string customWidgetConfig;      // Opaque JSON/string config passed to the custom widget
    std::string objectTypeName;          // Inline-struct type name (for Dictionary-backed structs)
    std::vector<PropertyDescriptor> objectSchema;  // Inline-struct field schema (recursive)

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

    // Chainable builders for the extended editor metadata. These return *this so
    // they compose with the PROPERTY_* factory macros, e.g.
    //   DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(...).WithSuffix("px"));
    PropertyDescriptor& WithDescription(const std::string& text) { description = text; return *this; }
    PropertyDescriptor& WithUsage(uint32_t flags) { usageFlags |= flags; return *this; }
    PropertyDescriptor& WithUsage(PropertyUsageFlags flag) { usageFlags |= static_cast<uint32_t>(flag); return *this; }
    PropertyDescriptor& WithHeader(const std::string& text) { headerText = text; return *this; }
    PropertyDescriptor& WithSuffix(const std::string& text) { suffix = text; return *this; }
    PropertyDescriptor& WithCustomWidget(const std::string& id, const std::string& config = "") {
        customWidget = id; customWidgetConfig = config; return *this;
    }
    PropertyDescriptor& WithObjectSchema(const std::string& typeName, std::vector<PropertyDescriptor> schema) {
        objectTypeName = typeName; objectSchema = std::move(schema); return *this;
    }

    bool HasUsage(PropertyUsageFlags flag) const { return HasUsageFlag(usageFlags, flag); }

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

// Hidden property (registered + serialized but not shown in the inspector)
#define PROPERTY_HIDDEN(name, type, defaultVal) \
    lupine::core::PropertyDescriptor(#name, lupine::core::PropertyValueType::type, defaultVal) \
        .WithUsage(lupine::core::PropertyUsageFlags::Hidden)

// Read-only property (shown but not editable)
#define PROPERTY_READONLY(name, type, defaultVal, groupName) \
    lupine::core::PropertyDescriptor(#name, lupine::core::PropertyValueType::type, defaultVal, \
        lupine::core::PropertyHint(), groupName).WithUsage(lupine::core::PropertyUsageFlags::ReadOnly)

// Property rendered through a named custom inspector widget
#define PROPERTY_CUSTOM_WIDGET(name, type, defaultVal, widgetId, groupName) \
    lupine::core::PropertyDescriptor(#name, lupine::core::PropertyValueType::type, defaultVal, \
        lupine::core::PropertyHint(), groupName).WithCustomWidget(widgetId)

} // namespace core
} // namespace lupine
