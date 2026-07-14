/**
 * @file lc_reflection.h
 * @brief Lupine Engine C API - Generic reflection / object-model bridge
 *
 * This header exposes the engine's dynamic type, property and method system to C
 * so that a host language binding (C#, Rust, Swift, Go, JavaScript, ...) can drive
 * the engine generically rather than through per-component typed accessors:
 *
 *   - Instantiate any registered node/component type by string name (type registry).
 *   - Enumerate, read and write a component's or node's properties by name.
 *   - Introspect a component's property descriptors (name, type, default, hints).
 *   - Invoke a component's named control methods (CallMethod dispatch).
 *   - Serialize/deserialize a whole node or component to/from JSON.
 *
 * Dynamic values cross the boundary as UTF-8 JSON strings, matching the engine's
 * internal marshalling (ComponentRef::Get/Set, Component::CallMethod). Scalars
 * marshal as JSON scalars; Vec2/Vec3/Vec4 as {"x","y",...}; Color as
 * {"r","g","b","a"}; method arguments as a JSON array.
 *
 * Functions whose result length is unbounded write a newly allocated, NUL-terminated
 * string to an `out_json` parameter. The caller OWNS that buffer and MUST release it
 * with lc_free() (see lc_core.h). On failure `*out_json` is left unchanged.
 */

#ifndef LUPINE_CAPI_REFLECTION_H
#define LUPINE_CAPI_REFLECTION_H

#include <core/lc_core.h>
#include <core/lc_node.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Property value types (mirrors lupine::core::PropertyValueType)
 * ============================================================================ */

typedef enum LCPropertyValueType {
    LC_PROPERTY_TYPE_INT          = 0,
    LC_PROPERTY_TYPE_FLOAT        = 1,
    LC_PROPERTY_TYPE_STRING       = 2,
    LC_PROPERTY_TYPE_BOOL         = 3,
    LC_PROPERTY_TYPE_VEC2         = 4,
    LC_PROPERTY_TYPE_VEC3         = 5,
    LC_PROPERTY_TYPE_VEC4         = 6,
    LC_PROPERTY_TYPE_COLOR        = 7,
    LC_PROPERTY_TYPE_NODE_PATH    = 8,
    LC_PROPERTY_TYPE_SCENE_PATH   = 9,
    LC_PROPERTY_TYPE_ENUM         = 10,
    LC_PROPERTY_TYPE_STRING_ARRAY = 11,
    LC_PROPERTY_TYPE_DOUBLE       = 12,
    LC_PROPERTY_TYPE_QUAT         = 13,
    LC_PROPERTY_TYPE_RECT         = 14,
    LC_PROPERTY_TYPE_RESOURCE     = 15,
    LC_PROPERTY_TYPE_INT_ARRAY    = 16,
    LC_PROPERTY_TYPE_FLOAT_ARRAY  = 17,
    LC_PROPERTY_TYPE_ARRAY        = 18,
    LC_PROPERTY_TYPE_DICTIONARY   = 19
} LCPropertyValueType;

/**
 * @brief Editor hint applied to a property (mirrors core PropertyHintType).
 *
 * Carried in the serialized property descriptor JSON under "hint".{ "type", "hint_string" }.
 */
typedef enum LCPropertyHintType {
    LC_PROPERTY_HINT_NONE           = 0,
    LC_PROPERTY_HINT_RANGE          = 1,   /* hint_string = "min,max,step" */
    LC_PROPERTY_HINT_ENUM           = 2,   /* hint_string = "A,B,C" */
    LC_PROPERTY_HINT_FILE           = 3,   /* hint_string = file filter, e.g. "*.png" */
    LC_PROPERTY_HINT_MULTILINE_TEXT = 4,
    LC_PROPERTY_HINT_EXP_RANGE      = 5,
    LC_PROPERTY_HINT_LENGTH         = 6,
    LC_PROPERTY_HINT_COLOR_NO_ALPHA = 7,
    LC_PROPERTY_HINT_DIR            = 8,
    LC_PROPERTY_HINT_LAYERS_2D      = 9,
    LC_PROPERTY_HINT_LAYERS_3D      = 10,
    LC_PROPERTY_HINT_NODE_TYPE      = 11,  /* hint_string = node class to filter to */
    LC_PROPERTY_HINT_ARCHETYPE_TYPE = 12,  /* hint_string = archetype class to filter to */
    LC_PROPERTY_HINT_SCRIPT_CLASS   = 13,  /* hint_string = script/component class */
    LC_PROPERTY_HINT_FLAGS          = 14,  /* hint_string = comma-separated flag names */
    LC_PROPERTY_HINT_EXP_EASING     = 15
} LCPropertyHintType;

/**
 * @brief Property usage flags bitmask (mirrors core PropertyUsageFlags).
 *
 * Carried in the serialized property descriptor JSON under the integer "usage" key.
 */
typedef enum LCPropertyUsageFlags {
    LC_PROPERTY_USAGE_NONE         = 0,
    LC_PROPERTY_USAGE_READONLY     = 1 << 0,
    LC_PROPERTY_USAGE_HIDDEN       = 1 << 1,
    LC_PROPERTY_USAGE_NO_SERIALIZE = 1 << 2,
    LC_PROPERTY_USAGE_REQUIRED     = 1 << 3,
    LC_PROPERTY_USAGE_UNIQUE       = 1 << 4,
    LC_PROPERTY_USAGE_EXPERIMENTAL = 1 << 5,
    LC_PROPERTY_USAGE_ADVANCED     = 1 << 6
} LCPropertyUsageFlags;

/* ============================================================================
 * Type registry (dynamic instantiation by name)
 * ============================================================================ */

/**
 * @brief Check whether a type name is registered with the engine type registry.
 * @return true if a factory exists for type_name.
 * @threadsafety Not thread-safe with concurrent type registration.
 */
LC_API bool lc_type_is_registered(const char* type_name);

/**
 * @brief Get the number of registered types.
 */
LC_API LCResult lc_type_get_count(int* out_count);

/**
 * @brief Get the name of the registered type at an index in [0, count).
 *
 * Enumeration order is unspecified but stable between calls while no types are
 * registered or removed.
 */
LC_API LCResult lc_type_get_name_at(int index, char* buffer, uint32_t max_length);

/**
 * @brief Instantiate a registered type as a Node and return a node handle.
 *
 * Fails with LC_ERROR_NOT_FOUND if the type is not registered, or
 * LC_ERROR_NOT_SUPPORTED if the registered type is not a Node subtype.
 */
LC_API LCResult lc_type_create_node(const char* type_name, LCNodeHandle* out_node);

/**
 * @brief Instantiate a registered type as a Component and return a (detached)
 *        component handle. Attach it to a node with lc_node_add_component, or use
 *        lc_node_add_component_by_type to create and attach in one call.
 *
 * Fails with LC_ERROR_NOT_FOUND if not registered, or LC_ERROR_NOT_SUPPORTED if
 * the registered type is not a Component subtype.
 */
LC_API LCResult lc_type_create_component(const char* type_name, LCComponentHandle* out_component);

/**
 * @brief Create a registered node type, name it, and attach it under a parent in
 *        one call.
 *
 * Instantiates @p type_name as a Node, applies @p name, registers its properties,
 * and attaches it to @p parent_or_null (or the runtime SceneManager's current
 * scene root when NULL). C-API equivalent of the scripting `create_node()`.
 *
 * @param type_name Registered node type name to instantiate.
 * @param name Name to assign to the new node (may be NULL for the type default).
 * @param parent_or_null Parent node handle, or NULL to attach to the current scene root.
 * @param out_node Output handle to the created node.
 * @return LC_SUCCESS on success, LC_ERROR_NULL_POINTER if type_name/out_node is
 *         NULL, LC_ERROR_NOT_FOUND if the type is not a registered Node,
 *         LC_ERROR_OPERATION_FAILED if no attach parent could be resolved.
 */
LC_API LCResult lc_create_node_child(const char* type_name, const char* name,
                                     LCNodeHandle parent_or_null, LCNodeHandle* out_node);

/* ============================================================================
 * Node reflection
 * ============================================================================ */

/**
 * @brief Get a node's runtime type name (e.g. "Node", "Node2D", "Node3D").
 */
LC_API LCResult lc_node_get_type_name(LCNodeHandle node, char* buffer, uint32_t max_length);

/**
 * @brief Get the number of components attached to a node.
 */
LC_API LCResult lc_node_get_component_count(LCNodeHandle node, int* out_count);

/**
 * @brief Get a handle to the component at an index in [0, count).
 */
LC_API LCResult lc_node_get_component_at(LCNodeHandle node, int index, LCComponentHandle* out_component);

/**
 * @brief Get a handle to the first component on the node whose type name matches.
 * @return LC_ERROR_NOT_FOUND if no component of that type is attached.
 */
LC_API LCResult lc_node_get_component_by_type(LCNodeHandle node, const char* type_name,
                                              LCComponentHandle* out_component);

/**
 * @brief Check whether a node has a component of the given type name.
 */
LC_API bool lc_node_has_component(LCNodeHandle node, const char* type_name);

/**
 * @brief Create a component of the given registered type and attach it to the node.
 *        The component's properties are registered before attachment.
 * @return LC_ERROR_NOT_FOUND / LC_ERROR_NOT_SUPPORTED as lc_type_create_component.
 */
LC_API LCResult lc_node_add_component_by_type(LCNodeHandle node, const char* type_name,
                                              LCComponentHandle* out_component);

/**
 * @brief Detach a component from a node. The component handle is not destroyed by
 *        this call; release it with lc_component_destroy if no longer needed.
 */
LC_API LCResult lc_node_remove_component(LCNodeHandle node, LCComponentHandle component);

/**
 * @brief Check whether a node exposes a property by name (component custom
 *        properties first, then node-level declared properties).
 */
LC_API bool lc_node_has_property(LCNodeHandle node, const char* name);

/**
 * @brief Read a node property by name as a JSON value string.
 *
 * Searches the node's components' custom properties, then node-level declared
 * properties. Writes a newly allocated JSON string to *out_json (free with
 * lc_free). @return LC_ERROR_NOT_FOUND if the property does not exist.
 */
LC_API LCResult lc_node_get_property_json(LCNodeHandle node, const char* name, char** out_json);

/**
 * @brief Write a node property by name from a JSON value string.
 *
 * Targets a matching component custom property if present, otherwise applies the
 * value to a node-level declared property without disturbing other state.
 * @return LC_ERROR_NOT_FOUND if no such property exists,
 *         LC_ERROR_INVALID_PARAMETER if value_json is not valid JSON.
 */
LC_API LCResult lc_node_set_property_json(LCNodeHandle node, const char* name, const char* value_json);

/**
 * @brief Serialize an entire node (type + properties) to a JSON string.
 *        Writes a newly allocated string to *out_json (free with lc_free).
 */
LC_API LCResult lc_node_serialize_json(LCNodeHandle node, char** out_json);

/**
 * @brief Apply a serialized JSON object to a node (inverse of lc_node_serialize_json).
 * @return LC_ERROR_INVALID_PARAMETER if json is not valid JSON.
 */
LC_API LCResult lc_node_deserialize_json(LCNodeHandle node, const char* json);

/* ============================================================================
 * Component reflection
 * ============================================================================ */

/**
 * @brief Get a component's runtime type name (e.g. "DirectionalLight3D").
 */
LC_API LCResult lc_component_get_type_name(LCComponentHandle component, char* buffer, uint32_t max_length);

/**
 * @brief Get a component's instance name.
 */
LC_API LCResult lc_component_get_name(LCComponentHandle component, char* buffer, uint32_t max_length);

/**
 * @brief Query / set whether a component is enabled.
 */
LC_API bool lc_component_is_enabled(LCComponentHandle component);
LC_API LCResult lc_component_set_enabled(LCComponentHandle component, bool enabled);

/**
 * @brief Check whether a component exposes a property by name (custom registry
 *        first, then declared/serialized properties).
 */
LC_API bool lc_component_has_property(LCComponentHandle component, const char* name);

/**
 * @brief Read a component property by name as a JSON value string.
 *        Writes a newly allocated string to *out_json (free with lc_free).
 * @return LC_ERROR_NOT_FOUND if the property does not exist.
 */
LC_API LCResult lc_component_get_property_json(LCComponentHandle component, const char* name, char** out_json);

/**
 * @brief Write a component property by name from a JSON value string.
 *
 * Targets the component's custom property registry when the property is a custom
 * (script-/archetype-exported) property; otherwise applies the value to a declared
 * property without disturbing other state. For built-in typed properties the
 * dedicated typed setters (e.g. lc_directional_light3d_set_intensity) remain
 * available and are generally more convenient.
 * @return LC_ERROR_NOT_FOUND if no such property exists,
 *         LC_ERROR_INVALID_PARAMETER if value_json is not valid JSON.
 */
LC_API LCResult lc_component_set_property_json(LCComponentHandle component, const char* name, const char* value_json);

/**
 * @brief Get the declared value type of a (custom) component property.
 * @return LC_ERROR_NOT_FOUND if the property is not a custom-registry property.
 */
LC_API LCResult lc_component_get_property_type(LCComponentHandle component, const char* name,
                                               LCPropertyValueType* out_type);

/**
 * @brief Get the number of reflected property descriptors for a component.
 */
LC_API LCResult lc_component_get_property_count(LCComponentHandle component, int* out_count);

/**
 * @brief Get the name of the reflected property descriptor at an index in [0, count).
 */
LC_API LCResult lc_component_get_property_name_at(LCComponentHandle component, int index,
                                                  char* buffer, uint32_t max_length);

/**
 * @brief Get all of a component's property descriptors as a JSON array string.
 *
 * Each element describes one property (name, type, default value, hint, group,
 * description). Writes a newly allocated string to *out_json (free with lc_free).
 */
LC_API LCResult lc_component_get_property_descriptors_json(LCComponentHandle component, char** out_json);

/**
 * @brief Invoke a component's named control method (generic CallMethod dispatch,
 *        e.g. AnimationPlayer "play", AnimationTree "set_float").
 *
 * @param args_json A JSON array of arguments, or NULL / "null" for none.
 * @param out_result_json Receives a newly allocated JSON result string (free with
 *        lc_free). "null" is written when the method has no return value. May be
 *        NULL to discard the result.
 */
LC_API LCResult lc_component_call_method_json(LCComponentHandle component, const char* method,
                                              const char* args_json, char** out_result_json);

/**
 * @brief Serialize an entire component (type + properties) to a JSON string.
 *        Writes a newly allocated string to *out_json (free with lc_free).
 */
LC_API LCResult lc_component_serialize_json(LCComponentHandle component, char** out_json);

/**
 * @brief Apply a serialized JSON object to a component (inverse of
 *        lc_component_serialize_json). This is the generic path for writing
 *        built-in declared properties in bulk.
 * @return LC_ERROR_INVALID_PARAMETER if json is not valid JSON.
 */
LC_API LCResult lc_component_deserialize_json(LCComponentHandle component, const char* json);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_REFLECTION_H */
