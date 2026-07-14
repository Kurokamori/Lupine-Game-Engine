#ifndef LUPINE_C_NETWORK_H
#define LUPINE_C_NETWORK_H

/**
 * @file lc_network.h
 * @brief C API for the Lupine networking/multiplayer session layer.
 *
 * Mirrors network::NetworkManager. Session control, peer queries, and session
 * signals (peer_connected / peer_disconnected / connected_to_server /
 * connection_failed / server_disconnected) delivered to native callbacks.
 *
 * All functions are safe to call when networking is compiled out or no session
 * is active - they simply report the inactive state. A foreign host with no
 * engine main loop should drive lc_net_poll()/lc_net_flush() each frame.
 */

#include "core/lc_core.h"

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Transport selection for LCNetConfig::transport. */
#define LC_NET_TRANSPORT_LOOPBACK 0
#define LC_NET_TRANSPORT_ENET     1
#define LC_NET_TRANSPORT_WEBSOCKET 2

/** Session mode reported by lc_net_get_mode (matches network::SessionMode). */
#define LC_NET_MODE_OFFLINE 0
#define LC_NET_MODE_SERVER  1
#define LC_NET_MODE_HOST    2
#define LC_NET_MODE_CLIENT  3
#define LC_NET_MODE_P2P     4

/** Session configuration. Fill via lc_net_config_default then override. */
typedef struct LCNetConfig {
    int transport;             /**< LC_NET_TRANSPORT_* */
    const char* address;       /**< Client: server address. Server: bind ("" = any). */
    uint16_t port;
    uint32_t max_peers;
    uint32_t protocol_version; /**< Per-game protocol gate. */
    const char* game_id;       /**< Per-game id gate. */
    float tick_rate;           /**< Snapshot send rate (Hz). */
    float interp_delay_ms;     /**< Default client interpolation delay. */
    float keyframe_interval_ms;/**< Reliable full-state keyframe period (0 = off). */
    float ping_interval_seconds;/**< RTT probe cadence (0 = off). */
    float interest_radius;     /**< Area-of-interest cull radius (0 = no culling). */
    bool enable_lan_discovery; /**< Listening sessions advertise on the LAN. */
    uint16_t discovery_port;   /**< UDP port for LAN discovery. */
    const char* server_name;   /**< Label shown in a LAN server browser. */

    /* Client-side prediction. */
    bool enable_prediction;    /**< Master toggle for NetworkController prediction. */
    uint32_t input_redundancy; /**< Past input commands repeated per packet (>=1). */

    /* Reconnection / session resume. */
    bool auto_reconnect;          /**< Client retries on an unexpected drop. */
    uint32_t reconnect_attempts;  /**< Max reconnect tries. */
    float reconnect_delay_seconds;/**< Delay between reconnect tries. */
    float resume_timeout_seconds; /**< Server: how long a dropped slot is held (0=off). */

    /* Anti-flood guards (server). 0 disables a guard. */
    uint32_t max_messages_per_second; /**< Inbound msgs/s per peer before a kick. */
    uint32_t max_bytes_per_second;    /**< Inbound bytes/s per peer before a kick. */
} LCNetConfig;

/** Aggregate, session-wide networking diagnostics (see lc_net_get_stats). */
typedef struct LCNetStats {
    uint32_t peer_count;
    float bytes_in_per_sec;
    float bytes_out_per_sec;
    uint64_t total_bytes_in;
    uint64_t total_bytes_out;
    float average_rtt_ms;
    float average_packet_loss_percent;
    uint64_t snapshots_sent;
    uint64_t snapshots_received;
    uint64_t messages_sent;
    uint64_t messages_received;
} LCNetStats;

/** Native session-signal callback. @p args_json is a JSON array string. */
typedef void (*LCNetEventCallback)(const char* args_json, void* user_data);

/** Initialize the networking manager (registers session signals). Idempotent. */
LC_API LCResult lc_net_initialize(void);

/** Populate @p out_config with sensible defaults (ENet, port 7777, 32 peers). */
LC_API LCResult lc_net_config_default(LCNetConfig* out_config);

LC_API LCResult lc_net_start_server(const LCNetConfig* config, bool* out_ok);
LC_API LCResult lc_net_start_host(const LCNetConfig* config, bool* out_ok);
LC_API LCResult lc_net_connect(const LCNetConfig* config, bool* out_ok);
LC_API LCResult lc_net_disconnect(void);

LC_API bool lc_net_is_active(void);
LC_API bool lc_net_is_server(void);
LC_API bool lc_net_is_client(void);
LC_API int lc_net_get_mode(void);
LC_API uint32_t lc_net_local_peer_id(void);
LC_API uint32_t lc_net_peer_count(void);

/** Get the connected peer ids as a JSON array string. Caller frees with lc_free. */
LC_API LCResult lc_net_get_peers(char** out_json);

/** Smoothed round-trip time to a peer in milliseconds (0 if unknown). */
LC_API float lc_net_peer_rtt(uint32_t peer_id);

/** Aggregate session diagnostics (bandwidth, loss, snapshot counts). */
LC_API LCResult lc_net_get_stats(LCNetStats* out_stats);

/** Per-peer estimated packet loss (percent) and snapshot jitter (ms). 0 if unknown. */
LC_API float lc_net_peer_loss(uint32_t peer_id);
LC_API float lc_net_peer_jitter(uint32_t peer_id);
LC_API float lc_net_peer_bytes_in_per_sec(uint32_t peer_id);
LC_API float lc_net_peer_bytes_out_per_sec(uint32_t peer_id);

/** Forcibly disconnect a peer (server/host only). Returns false if not allowed. */
LC_API LCResult lc_net_kick_peer(uint32_t peer_id, bool* out_ok);

/** Report the local player's area-of-interest position (2D / 3D). */
LC_API LCResult lc_net_set_interest_2d(float x, float y);
LC_API LCResult lc_net_set_interest_3d(float x, float y, float z);

/* ----------------------------------------------------------------------------
 * LAN discovery (UDP broadcast). A listening session started with
 * enable_lan_discovery advertises automatically; a client can browse for
 * servers while still offline.
 * ------------------------------------------------------------------------- */

/** Begin advertising the active server/host on the LAN. */
LC_API LCResult lc_net_start_lan_advertising(bool* out_ok);
/** Stop advertising (does not affect an active discovery scan). */
LC_API LCResult lc_net_stop_lan_advertising(void);
LC_API bool lc_net_is_lan_advertising(void);

/** Begin scanning the LAN for servers of @p game_id on @p discovery_port. */
LC_API LCResult lc_net_start_lan_discovery(const char* game_id, uint16_t discovery_port,
                                           bool* out_ok);
LC_API LCResult lc_net_stop_lan_discovery(void);
LC_API bool lc_net_is_lan_discovering(void);

/**
 * Discovered servers as a JSON array string (objects with game_id, name,
 * address, port, player_count, max_players, protocol_version, age_seconds).
 * Caller frees with lc_free.
 */
LC_API LCResult lc_net_get_discovered_servers(char** out_json);

/** Connect a native callback to a session signal. @p out_id may be NULL. */
LC_API LCResult lc_net_on_event(const char* event, LCNetEventCallback callback,
                                void* user_data, uint64_t* out_id);
/** Disconnect a session-signal callback by id. */
LC_API LCResult lc_net_off_event(const char* event, uint64_t id);

/**
 * Drive the receive pass + deliver queued events (for hosts without an engine
 * main loop). The engine runtime already drives this from SceneManager::Update.
 */
LC_API LCResult lc_net_poll(float delta_seconds);
/** Drive the send pass (flush outgoing traffic). */
LC_API LCResult lc_net_flush(void);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_C_NETWORK_H */
