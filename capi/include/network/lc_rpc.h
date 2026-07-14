#ifndef LUPINE_C_RPC_H
#define LUPINE_C_RPC_H

/**
 * @file lc_rpc.h
 * @brief C API for remote procedure calls (network::RpcManager).
 *
 * Invoke a method on a node across the network. The node must carry a
 * NetworkObject. Arguments cross as a JSON array string, matching the signal
 * convention. Offline, an RPC degrades to a local call so the behaviour is
 * identical in single-player.
 */

#include "core/lc_core.h"
#include "core/lc_node.h"

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** RPC mode for lc_net_register_component_rpc. */
#define LC_RPC_MODE_AUTHORITY 0
#define LC_RPC_MODE_ANY_PEER  1

/** RPC transfer mode. */
#define LC_RPC_TRANSFER_RELIABLE   0
#define LC_RPC_TRANSFER_UNRELIABLE 1

/** Call a script-function RPC on a node per its declared config (broadcast). */
LC_API LCResult lc_net_rpc(LCNodeHandle node, const char* method, const char* args_json);

/** Call a script-function RPC on a single peer. */
LC_API LCResult lc_net_rpc_id(LCNodeHandle node, uint32_t peer_id,
                              const char* method, const char* args_json);

/** Call a script-function RPC over the unreliable channel. */
LC_API LCResult lc_net_rpc_unreliable(LCNodeHandle node, const char* method, const char* args_json);

/** Register an RPC config for a component type's method (component-method RPCs). */
LC_API LCResult lc_net_register_component_rpc(const char* component_type, const char* method,
                                              int mode, int transfer, bool call_local);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_C_RPC_H */
