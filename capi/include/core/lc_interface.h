#ifndef LUPINE_C_INTERFACE_H
#define LUPINE_C_INTERFACE_H

/**
 * @file lc_interface.h
 * @brief C API for the Lupine interface-types system.
 *
 * Interfaces are named capability contracts (required methods + required
 * signals). Scripts, archetypes, and native components declare that they
 * implement an interface; the engine indexes those declarations so a host can
 * query "every Damageable in the scene", verify a node against a contract, and
 * pair the result with the signal API (see lc_signal.h).
 *
 * Definitions and node/archetype interface lists cross the boundary as JSON
 * (the engine's universal currency). A serialized interface definition matches
 * the .interface asset schema, e.g.:
 *   { "interface_name": "Damageable", "description": "...",
 *     "extends": ["Entity"], "methods": [{"name":"take_damage","params":[...]}],
 *     "signals": [{"name":"died","args":[]}], "tags": ["combat"] }
 *
 * Functions that emit a newly-allocated JSON/string to an out parameter transfer
 * ownership to the caller, who MUST release it with lc_free() (see lc_core.h).
 */

#include "core/lc_core.h"
#include "core/lc_node.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================== */
/* Interface registry / definitions                                         */
/* ======================================================================== */

/** Whether an interface name is registered. */
LC_API LCResult lc_interface_exists(const char* interface_name, bool* out_exists);

/** Number of registered interfaces. */
LC_API LCResult lc_interface_get_count(int* out_count);

/** Name of the interface at @p index ([0, count)). Writes into @p buffer. */
LC_API LCResult lc_interface_get_name_at(int index, char* buffer, uint32_t max_length);

/**
 * The serialized definition of an interface (the .interface schema as JSON).
 * Writes a newly allocated string to *out_json (free with lc_free).
 * @return LC_ERROR_NOT_FOUND if the interface is unknown.
 */
LC_API LCResult lc_interface_get_definition_json(const char* interface_name, char** out_json);

/**
 * Register (or replace) an interface at runtime from a JSON definition object
 * (same shape as a .interface file). The interface persists across scene
 * switches. @return LC_ERROR_INVALID_PARAMETER if the JSON is not a valid
 * definition (e.g. missing "interface_name").
 */
LC_API LCResult lc_interface_register_json(const char* definition_json);

/**
 * Verify a candidate against an interface's effective contract given the method
 * names and signal names it exposes (both JSON arrays of strings, may be NULL
 * for none). Writes a result object to *out_json (free with lc_free):
 *   { "interface","exists","conforms","missing_methods","missing_signals" }
 */
LC_API LCResult lc_interface_verify_members_json(const char* interface_name,
                                                 const char* present_methods_json,
                                                 const char* present_signals_json,
                                                 char** out_json);

/* ======================================================================== */
/* Native component-type conformance                                        */
/* ======================================================================== */

/**
 * Declare from C that a native component type implements an interface (for
 * native extensions). Equivalent to the REGISTER_COMPONENT_INTERFACES macro.
 */
LC_API LCResult lc_interface_register_type_conformance(const char* type_name,
                                                       const char* interface_name);

/** Whether a native component type implements an interface (inheritance aware). */
LC_API LCResult lc_interface_type_implements(const char* type_name,
                                             const char* interface_name,
                                             bool* out_implements);

/**
 * The component type names that implement an interface, as a JSON array of
 * strings. Writes a newly allocated string to *out_json (free with lc_free).
 */
LC_API LCResult lc_interface_get_types_implementing_json(const char* interface_name,
                                                         char** out_json);

/* ======================================================================== */
/* Node conformance                                                         */
/* ======================================================================== */

/** Whether a node implements an interface (any component declares it). */
LC_API LCResult lc_node_implements_interface(LCNodeHandle node, const char* interface_name,
                                             bool* out_implements);

/**
 * The interfaces a node implements (including inherited base interfaces), as a
 * JSON array of strings. Writes a newly allocated string to *out_json.
 */
LC_API LCResult lc_node_get_interfaces_json(LCNodeHandle node, char** out_json);

/**
 * Verify a node against an interface's method+signal contract. Writes a result
 * object to *out_json (free with lc_free):
 *   { "interface","exists","conforms","missing_methods","missing_signals" }
 */
LC_API LCResult lc_node_verify_interface_json(LCNodeHandle node, const char* interface_name,
                                              char** out_json);

/* ======================================================================== */
/* Component conformance                                                     */
/* ======================================================================== */

/** Whether a component implements an interface (inheritance aware). */
LC_API LCResult lc_component_implements_interface(LCComponentHandle component,
                                                  const char* interface_name,
                                                  bool* out_implements);

/** The interfaces a component declares, as a JSON array of strings. */
LC_API LCResult lc_component_get_interfaces_json(LCComponentHandle component, char** out_json);

/* ======================================================================== */
/* Scene queries                                                            */
/* ======================================================================== */

/**
 * Collect every node in the current scene that implements an interface.
 * @param out_nodes   Receives up to @p max_count handles (may be NULL to count).
 * @param out_count   Receives the total number of matches (required).
 * Each returned handle is owned by the caller and must be released with the node
 * handle API as usual.
 */
LC_API LCResult lc_scene_find_nodes_by_interface(const char* interface_name,
                                                 LCNodeHandle* out_nodes, size_t max_count,
                                                 size_t* out_count);

/** First node in the current scene implementing an interface (or NULL handle). */
LC_API LCResult lc_scene_get_first_node_by_interface(const char* interface_name,
                                                     LCNodeHandle* out_found);

/* ======================================================================== */
/* Archetype conformance                                                    */
/* ======================================================================== */

/** Whether an archetype class implements an interface (inheritance aware). */
LC_API LCResult lc_archetype_implements_interface(const char* class_name,
                                                  const char* interface_name,
                                                  bool* out_implements);

/** The interfaces an archetype class implements, as a JSON array of strings. */
LC_API LCResult lc_archetype_get_interfaces_json(const char* class_name, char** out_json);

/** The archetype class names implementing an interface, as a JSON array. */
LC_API LCResult lc_archetype_get_implementing_json(const char* interface_name, char** out_json);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_C_INTERFACE_H */
