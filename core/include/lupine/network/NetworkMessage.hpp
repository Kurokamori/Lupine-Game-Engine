#pragma once

#include "lupine/network/NetworkTypes.hpp"
#include <cstdint>
#include <vector>

namespace lupine {
namespace network {

/**
 * Wire message kinds. The first byte of every message frame's body (after the
 * channel byte) is one of these. Session-management kinds (Handshake..Pong) are
 * handled internally by NetworkManager; the gameplay kinds (Rpc..Custom) are
 * routed to handlers registered by the RPC and replication layers, so those
 * layers can be added without modifying the manager.
 */
enum class MsgType : uint8_t {
    Handshake = 0,     // joiner -> authority: version/game-id (+ self id for P2P)
    Welcome,           // authority -> joiner: assigned peer id + existing peers
    PeerJoined,        // authority -> all: a peer joined
    PeerLeft,          // authority -> all: a peer left
    Ping,              // rtt probe
    Pong,              // rtt reply
    Rpc,               // remote procedure call (handled by RpcManager)
    Spawn,             // replicated object spawn (handled by ReplicationManager)
    Despawn,           // replicated object despawn
    Snapshot,          // replicated state delta
    OwnershipChange,   // authority transfer for an object
    InterestUpdate,    // client -> server: this peer's area-of-interest position
    InputCommand,      // controller -> authority: sequenced input for prediction
    SnapshotAck,       // receiver -> authority: highest snapshot sequence received
    Resume,            // reconnecting client -> server: prior peer id + resume token
    Custom             // application-defined raw message
};

/**
 * joiner -> authority. Validated against the local NetworkConfig; a mismatch
 * rejects the connection with a connection_failed event.
 */
struct HandshakeMsg {
    uint32_t engineProtocol = kEngineProtocolVersion;
    uint32_t gameProtocol = 0;
    std::string gameId;
    PeerId selfId = kInvalidPeerId;  // Non-zero only in P2P (sender's self-assigned id).
};

/**
 * authority -> joiner. Tells the joiner the id it has been assigned and lists
 * the peers already in the session so it can populate its peer table. The
 * resumeToken is an opaque secret the client stores and presents in a Resume
 * message to reclaim this same id after an unexpected disconnect.
 */
struct WelcomeMsg {
    PeerId assignedPeerId = kInvalidPeerId;
    PeerId serverPeerId = kServerPeerId;
    uint32_t resumeToken = 0;
    std::vector<PeerId> existingPeers;
};

/**
 * reconnecting client -> authority. Presents the peer id + token from the prior
 * session's Welcome to resume in place (same id, retained owned objects) if the
 * server still holds the slot; otherwise the server falls back to a fresh join.
 */
struct ResumeMsg {
    uint32_t engineProtocol = kEngineProtocolVersion;
    uint32_t gameProtocol = 0;
    std::string gameId;
    PeerId peerId = kInvalidPeerId;
    uint32_t resumeToken = 0;
};

struct PeerEventMsg {
    PeerId peerId = kInvalidPeerId;
};

struct PingMsg {
    uint32_t token = 0;
};

struct PongMsg {
    uint32_t token = 0;
};

} // namespace network
} // namespace lupine
