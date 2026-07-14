#ifndef LUPINE_C_SIGNAL_H
#define LUPINE_C_SIGNAL_H

/**
 * @file lc_signal.h
 * @brief C API for the Lupine signal/event system.
 *
 * Mirrors the engine's SignalObject + EventBus. Signal arguments cross the C
 * boundary as a JSON array string (e.g. "[10, \"hello\", true]"); this matches
 * how the engine marshals signal payloads internally. Connection targets are
 * nodes whose attached script(s) provide the handler method, or native C
 * callbacks registered with user data.
 *
 * Connection flags (bitmask):
 *   LC_CONNECT_DEFERRED - invoke the handler at the end of the frame
 *   LC_CONNECT_ONESHOT  - auto-disconnect after the first invocation
 */

#include "core/lc_core.h"
#include "core/lc_node.h"

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Connection flag bits (combine with bitwise OR). */
#define LC_CONNECT_NONE     0u
#define LC_CONNECT_DEFERRED 1u
#define LC_CONNECT_ONESHOT  2u

/**
 * Native signal handler callback.
 * @param args_json The emitted arguments as a JSON array string.
 * @param user_data The opaque pointer supplied at connect time.
 */
typedef void (*LCSignalCallback)(const char* args_json, void* user_data);

/* ======================================================================== */
/* Node signals                                                             */
/* ======================================================================== */

/**
 * Declare a user signal on a node (so it can be connected/emitted by name).
 */
LC_API LCResult lc_node_add_user_signal(LCNodeHandle node, const char* signal);

/**
 * Emit a signal on a node. @p args_json may be NULL or "[]" for no arguments.
 */
LC_API LCResult lc_node_emit(LCNodeHandle node, const char* signal, const char* args_json);

/**
 * Connect a node's signal to a method handler on a target node.
 * @param out_connection_id Receives the connection id (may be NULL).
 */
LC_API LCResult lc_node_connect(LCNodeHandle node, const char* signal,
                                LCNodeHandle target, const char* method,
                                uint32_t flags, uint64_t* out_connection_id);

/**
 * Connect a node's signal to a native C callback.
 * @param out_connection_id Receives the connection id (may be NULL).
 */
LC_API LCResult lc_node_connect_callback(LCNodeHandle node, const char* signal,
                                         LCSignalCallback callback, void* user_data,
                                         uint32_t flags, uint64_t* out_connection_id);

/** Disconnect a node connection by id. */
LC_API LCResult lc_node_disconnect(LCNodeHandle node, const char* signal, uint64_t connection_id);

/** Query whether a node signal has at least one live connection. */
LC_API LCResult lc_node_is_connected(LCNodeHandle node, const char* signal, bool* out_connected);

/**
 * Queue a method call on a node for the next deferred flush (end of frame).
 * @param args_json May be NULL or "[]" for no arguments.
 */
LC_API LCResult lc_node_call_deferred(LCNodeHandle node, const char* method, const char* args_json);

/* ======================================================================== */
/* Component signals                                                        */
/* ======================================================================== */

LC_API LCResult lc_component_add_user_signal(LCComponentHandle component, const char* signal);
LC_API LCResult lc_component_emit(LCComponentHandle component, const char* signal, const char* args_json);
LC_API LCResult lc_component_connect(LCComponentHandle component, const char* signal,
                                     LCNodeHandle target, const char* method,
                                     uint32_t flags, uint64_t* out_connection_id);
LC_API LCResult lc_component_connect_callback(LCComponentHandle component, const char* signal,
                                              LCSignalCallback callback, void* user_data,
                                              uint32_t flags, uint64_t* out_connection_id);
LC_API LCResult lc_component_disconnect(LCComponentHandle component, const char* signal, uint64_t connection_id);
LC_API LCResult lc_component_is_connected(LCComponentHandle component, const char* signal, bool* out_connected);

/* ======================================================================== */
/* Global event bus                                                         */
/* ======================================================================== */

/** Emit a global event. @p args_json may be NULL or "[]" for no arguments. */
LC_API LCResult lc_event_emit(const char* event, const char* args_json);

/** Subscribe a target node's method handler to a global event. */
LC_API LCResult lc_event_subscribe(const char* event, LCNodeHandle target, const char* method,
                                   uint32_t flags, uint64_t* out_subscription_id);

/** Subscribe a native C callback to a global event. */
LC_API LCResult lc_event_subscribe_callback(const char* event, LCSignalCallback callback, void* user_data,
                                            uint32_t flags, uint64_t* out_subscription_id);

/** Remove a global event subscription by id. */
LC_API LCResult lc_event_unsubscribe(const char* event, uint64_t subscription_id);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_C_SIGNAL_H */
