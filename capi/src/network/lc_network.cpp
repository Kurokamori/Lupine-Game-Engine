#include "network/lc_network.h"
#include "core/lc_core.h"
#include "../core/lc_internal.h"

#include <lupine/network/NetworkManager.hpp>
#include <lupine/core/SignalObject.hpp>
#include <lupine/core/SignalDispatcher.hpp>

#include <nlohmann/json.hpp>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

lupine::network::NetworkConfig ToConfig(const LCNetConfig* config) {
    lupine::network::NetworkConfig out;
    if (config == nullptr) {
        return out;
    }
    switch (config->transport) {
        case LC_NET_TRANSPORT_LOOPBACK: out.transport = lupine::network::TransportKind::Loopback; break;
        case LC_NET_TRANSPORT_WEBSOCKET: out.transport = lupine::network::TransportKind::WebSocket; break;
        default: out.transport = lupine::network::TransportKind::ENet; break;
    }
    if (config->address != nullptr) {
        out.address = config->address;
    }
    out.port = config->port;
    if (config->max_peers > 0) {
        out.maxPeers = config->max_peers;
    }
    out.protocolVersion = config->protocol_version;
    if (config->game_id != nullptr) {
        out.gameId = config->game_id;
    }
    if (config->tick_rate > 0.0f) {
        out.tickRate = config->tick_rate;
    }
    if (config->interp_delay_ms > 0.0f) {
        out.interpDelayMs = config->interp_delay_ms;
    }
    if (config->keyframe_interval_ms > 0.0f) {
        out.keyframeIntervalMs = config->keyframe_interval_ms;
    }
    if (config->ping_interval_seconds > 0.0f) {
        out.pingIntervalSeconds = config->ping_interval_seconds;
    }
    if (config->interest_radius > 0.0f) {
        out.interestRadius = config->interest_radius;
    }
    out.enableLanDiscovery = config->enable_lan_discovery;
    if (config->discovery_port != 0) {
        out.discoveryPort = config->discovery_port;
    }
    if (config->server_name != nullptr) {
        out.serverName = config->server_name;
    }
    out.enablePrediction = config->enable_prediction;
    if (config->input_redundancy > 0) {
        out.inputRedundancy = config->input_redundancy;
    }
    out.autoReconnect = config->auto_reconnect;
    if (config->reconnect_attempts > 0) {
        out.reconnectAttempts = config->reconnect_attempts;
    }
    if (config->reconnect_delay_seconds > 0.0f) {
        out.reconnectDelaySeconds = config->reconnect_delay_seconds;
    }
    out.resumeTimeoutSeconds = config->resume_timeout_seconds;
    out.maxMessagesPerSecond = config->max_messages_per_second;
    out.maxBytesPerSecond = config->max_bytes_per_second;
    return out;
}

// Native slot that serialises emitted args to a JSON array string for a callback.
lupine::core::SignalSlot MakeNetSlot(LCNetEventCallback callback, void* user_data) {
    return [callback, user_data](const lupine::core::SignalArgs& args) {
        if (!callback) {
            return;
        }
        nlohmann::json arr = nlohmann::json::array();
        for (const nlohmann::json& a : args) {
            arr.push_back(a);
        }
        const std::string serialized = arr.dump();
        callback(serialized.c_str(), user_data);
    };
}

LCResult AllocJson(const nlohmann::json& value, char** out_json) {
    if (!out_json) {
        SetError(LC_ERROR_NULL_POINTER, "out_json is null");
        return LC_ERROR_NULL_POINTER;
    }
    const std::string serialized = value.dump();
    char* buffer = static_cast<char*>(std::malloc(serialized.size() + 1));
    if (!buffer) {
        SetError(LC_ERROR_OUT_OF_MEMORY, "allocation failed");
        return LC_ERROR_OUT_OF_MEMORY;
    }
    std::memcpy(buffer, serialized.c_str(), serialized.size() + 1);
    *out_json = buffer;
    return LC_SUCCESS;
}

} // namespace

LC_API LCResult lc_net_initialize(void) {
    lupine::network::NetworkManager::GetInstance().Initialize();
    return LC_SUCCESS;
}

LC_API LCResult lc_net_config_default(LCNetConfig* out_config) {
    if (!out_config) {
        SetError(LC_ERROR_NULL_POINTER, "out_config is null");
        return LC_ERROR_NULL_POINTER;
    }
    out_config->transport = LC_NET_TRANSPORT_ENET;
    out_config->address = "127.0.0.1";
    out_config->port = 7777;
    out_config->max_peers = 32;
    out_config->protocol_version = 1;
    out_config->game_id = "lupine-game";
    out_config->tick_rate = 30.0f;
    out_config->interp_delay_ms = 100.0f;
    out_config->keyframe_interval_ms = 1000.0f;
    out_config->ping_interval_seconds = 1.0f;
    out_config->interest_radius = 0.0f;
    out_config->enable_lan_discovery = false;
    out_config->discovery_port = 7779;
    out_config->server_name = "Lupine Server";
    out_config->enable_prediction = true;
    out_config->input_redundancy = 3;
    out_config->auto_reconnect = false;
    out_config->reconnect_attempts = 3;
    out_config->reconnect_delay_seconds = 2.0f;
    out_config->resume_timeout_seconds = 0.0f;
    out_config->max_messages_per_second = 0;
    out_config->max_bytes_per_second = 0;
    return LC_SUCCESS;
}

LC_API LCResult lc_net_start_server(const LCNetConfig* config, bool* out_ok) {
    const bool ok = lupine::network::NetworkManager::GetInstance().StartServer(ToConfig(config));
    if (out_ok) {
        *out_ok = ok;
    }
    return ok ? LC_SUCCESS : LC_ERROR_OPERATION_FAILED;
}

LC_API LCResult lc_net_start_host(const LCNetConfig* config, bool* out_ok) {
    const bool ok = lupine::network::NetworkManager::GetInstance().StartHost(ToConfig(config));
    if (out_ok) {
        *out_ok = ok;
    }
    return ok ? LC_SUCCESS : LC_ERROR_OPERATION_FAILED;
}

LC_API LCResult lc_net_connect(const LCNetConfig* config, bool* out_ok) {
    const bool ok = lupine::network::NetworkManager::GetInstance().Connect(ToConfig(config));
    if (out_ok) {
        *out_ok = ok;
    }
    return ok ? LC_SUCCESS : LC_ERROR_OPERATION_FAILED;
}

LC_API LCResult lc_net_disconnect(void) {
    lupine::network::NetworkManager::GetInstance().Disconnect();
    return LC_SUCCESS;
}

LC_API bool lc_net_is_active(void) {
    return lupine::network::NetworkManager::GetInstance().IsActive();
}

LC_API bool lc_net_is_server(void) {
    return lupine::network::NetworkManager::GetInstance().IsServer();
}

LC_API bool lc_net_is_client(void) {
    return lupine::network::NetworkManager::GetInstance().IsClient();
}

LC_API int lc_net_get_mode(void) {
    return static_cast<int>(lupine::network::NetworkManager::GetInstance().GetMode());
}

LC_API uint32_t lc_net_local_peer_id(void) {
    return lupine::network::NetworkManager::GetInstance().GetLocalPeerId();
}

LC_API uint32_t lc_net_peer_count(void) {
    return static_cast<uint32_t>(lupine::network::NetworkManager::GetInstance().GetPeerCount());
}

LC_API LCResult lc_net_get_peers(char** out_json) {
    nlohmann::json arr = nlohmann::json::array();
    for (const lupine::network::PeerInfo& peer :
         lupine::network::NetworkManager::GetInstance().GetPeers()) {
        arr.push_back(peer.id);
    }
    return AllocJson(arr, out_json);
}

LC_API float lc_net_peer_rtt(uint32_t peer_id) {
    for (const lupine::network::PeerInfo& peer :
         lupine::network::NetworkManager::GetInstance().GetPeers()) {
        if (peer.id == peer_id) {
            return peer.roundTripMs;
        }
    }
    return 0.0f;
}

LC_API LCResult lc_net_get_stats(LCNetStats* out_stats) {
    if (!out_stats) {
        SetError(LC_ERROR_NULL_POINTER, "out_stats is null");
        return LC_ERROR_NULL_POINTER;
    }
    const lupine::network::NetworkStats s =
        lupine::network::NetworkManager::GetInstance().GetStats();
    out_stats->peer_count = s.peerCount;
    out_stats->bytes_in_per_sec = s.bytesInPerSec;
    out_stats->bytes_out_per_sec = s.bytesOutPerSec;
    out_stats->total_bytes_in = s.totalBytesIn;
    out_stats->total_bytes_out = s.totalBytesOut;
    out_stats->average_rtt_ms = s.averageRttMs;
    out_stats->average_packet_loss_percent = s.averagePacketLossPercent;
    out_stats->snapshots_sent = s.snapshotsSent;
    out_stats->snapshots_received = s.snapshotsReceived;
    out_stats->messages_sent = s.messagesSent;
    out_stats->messages_received = s.messagesReceived;
    return LC_SUCCESS;
}

LC_API float lc_net_peer_loss(uint32_t peer_id) {
    lupine::network::PeerInfo info;
    if (lupine::network::NetworkManager::GetInstance().GetPeerInfo(peer_id, info)) {
        return info.packetLossPercent;
    }
    return 0.0f;
}

LC_API float lc_net_peer_jitter(uint32_t peer_id) {
    lupine::network::PeerInfo info;
    if (lupine::network::NetworkManager::GetInstance().GetPeerInfo(peer_id, info)) {
        return info.jitterMs;
    }
    return 0.0f;
}

LC_API float lc_net_peer_bytes_in_per_sec(uint32_t peer_id) {
    lupine::network::PeerInfo info;
    if (lupine::network::NetworkManager::GetInstance().GetPeerInfo(peer_id, info)) {
        return info.bytesInPerSec;
    }
    return 0.0f;
}

LC_API float lc_net_peer_bytes_out_per_sec(uint32_t peer_id) {
    lupine::network::PeerInfo info;
    if (lupine::network::NetworkManager::GetInstance().GetPeerInfo(peer_id, info)) {
        return info.bytesOutPerSec;
    }
    return 0.0f;
}

LC_API LCResult lc_net_kick_peer(uint32_t peer_id, bool* out_ok) {
    const bool ok = lupine::network::NetworkManager::GetInstance().KickPeer(peer_id);
    if (out_ok) {
        *out_ok = ok;
    }
    return ok ? LC_SUCCESS : LC_ERROR_OPERATION_FAILED;
}

LC_API LCResult lc_net_set_interest_2d(float x, float y) {
    lupine::network::NetworkManager::GetInstance().SetInterestPosition2D(x, y);
    return LC_SUCCESS;
}

LC_API LCResult lc_net_set_interest_3d(float x, float y, float z) {
    lupine::network::NetworkManager::GetInstance().SetInterestPosition3D(x, y, z);
    return LC_SUCCESS;
}

LC_API LCResult lc_net_start_lan_advertising(bool* out_ok) {
    const bool ok = lupine::network::NetworkManager::GetInstance().StartLanAdvertising();
    if (out_ok) {
        *out_ok = ok;
    }
    return ok ? LC_SUCCESS : LC_ERROR_OPERATION_FAILED;
}

LC_API LCResult lc_net_stop_lan_advertising(void) {
    lupine::network::NetworkManager::GetInstance().StopLanAdvertising();
    return LC_SUCCESS;
}

LC_API bool lc_net_is_lan_advertising(void) {
    return lupine::network::NetworkManager::GetInstance().IsLanAdvertising();
}

LC_API LCResult lc_net_start_lan_discovery(const char* game_id, uint16_t discovery_port,
                                           bool* out_ok) {
    const bool ok = lupine::network::NetworkManager::GetInstance().StartLanDiscovery(
        game_id ? game_id : "", discovery_port);
    if (out_ok) {
        *out_ok = ok;
    }
    return ok ? LC_SUCCESS : LC_ERROR_OPERATION_FAILED;
}

LC_API LCResult lc_net_stop_lan_discovery(void) {
    lupine::network::NetworkManager::GetInstance().StopLanDiscovery();
    return LC_SUCCESS;
}

LC_API bool lc_net_is_lan_discovering(void) {
    return lupine::network::NetworkManager::GetInstance().IsLanDiscovering();
}

LC_API LCResult lc_net_get_discovered_servers(char** out_json) {
    nlohmann::json arr = nlohmann::json::array();
    for (const lupine::network::LanServerInfo& server :
         lupine::network::NetworkManager::GetInstance().GetDiscoveredServers()) {
        nlohmann::json entry;
        entry["game_id"] = server.gameId;
        entry["name"] = server.name;
        entry["address"] = server.address;
        entry["port"] = server.gamePort;
        entry["player_count"] = server.playerCount;
        entry["max_players"] = server.maxPlayers;
        entry["protocol_version"] = server.protocolVersion;
        entry["age_seconds"] = server.ageSeconds;
        arr.push_back(entry);
    }
    return AllocJson(arr, out_json);
}

LC_API LCResult lc_net_on_event(const char* event, LCNetEventCallback callback,
                                void* user_data, uint64_t* out_id) {
    if (!event || !callback) {
        SetError(LC_ERROR_NULL_POINTER, "event name or callback is null");
        return LC_ERROR_NULL_POINTER;
    }
    const uint64_t id = lupine::network::NetworkManager::GetInstance().Events().Connect(
        event, MakeNetSlot(callback, user_data));
    if (out_id) {
        *out_id = id;
    }
    return LC_SUCCESS;
}

LC_API LCResult lc_net_off_event(const char* event, uint64_t id) {
    if (!event) {
        SetError(LC_ERROR_NULL_POINTER, "event name is null");
        return LC_ERROR_NULL_POINTER;
    }
    lupine::network::NetworkManager::GetInstance().Events().Disconnect(event, id);
    return LC_SUCCESS;
}

LC_API LCResult lc_net_poll(float delta_seconds) {
    lupine::network::NetworkManager::GetInstance().Poll(delta_seconds);
    // Deliver any session-signal callbacks queued during the receive pass.
    lupine::core::SignalDispatcher::Get().Flush();
    return LC_SUCCESS;
}

LC_API LCResult lc_net_flush(void) {
    lupine::network::NetworkManager::GetInstance().Flush();
    return LC_SUCCESS;
}
