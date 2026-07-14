#ifndef LUPINE_C_REPLICATION_H
#define LUPINE_C_REPLICATION_H

/**
 * @file lc_replication.h
 * @brief C API for object replication: NetworkId / authority queries and
 *        spawn/despawn (network::ReplicationManager + NetworkObject/NetworkSpawner).
 */

#include "core/lc_core.h"
#include "core/lc_node.h"

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Get a node's network-stable id (0 if it has no NetworkObject / unassigned). */
LC_API LCResult lc_net_object_id(LCNodeHandle node, uint32_t* out_id);

/** Set / get the peer that has authority over a node's NetworkObject. */
LC_API LCResult lc_net_set_authority(LCNodeHandle node, uint32_t peer_id);
LC_API uint32_t lc_net_get_authority(LCNodeHandle node);

/** True if the local instance is authority for the node (true offline). */
LC_API bool lc_net_is_authority(LCNodeHandle node);

/**
 * Authority: spawn a whitelisted scene as a networked child of a node carrying a
 * NetworkSpawner. @p out_id receives the new NetworkId (may be NULL).
 */
LC_API LCResult lc_net_spawn(LCNodeHandle spawner, const char* scene_path,
                             uint32_t owner_peer, uint32_t* out_id);

/** Authority: despawn a previously spawned object by its NetworkId. */
LC_API LCResult lc_net_despawn(uint32_t network_id);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_C_REPLICATION_H */
