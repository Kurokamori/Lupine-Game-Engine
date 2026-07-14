#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace lupine {
namespace network {

/**
 * Stable identity of a connected participant in a networking session.
 *
 * In client-server / host sessions the server owns the id space: the server (or
 * host) is always peer 1, and joining clients are assigned 2, 3, ... A value of
 * 0 is never a real peer; it is used as an "invalid / none" sentinel and as the
 * "broadcast to everyone" sentinel in send helpers. In peer-to-peer sessions
 * each instance self-assigns a random non-zero id announced during the
 * handshake.
 */
using PeerId = uint32_t;

/**
 * Network-stable identity of a replicated Node, distinct from its 128-bit
 * per-instance UUID (which is never synchronized between machines). NetworkIds
 * are 32-bit so they are cheap on the wire. Two non-overlapping ranges are used
 * (see ReplicationManager): scene-placed nodes get low ids assigned by a
 * deterministic scene walk that every peer reproduces identically, and
 * runtime-spawned nodes get high ids allocated by the authority. 0 means none.
 */
using NetworkId = uint32_t;

// The peer id reserved for the authoritative server / host.
constexpr PeerId kServerPeerId = 1;

// "No peer" / "broadcast to all" sentinel for the send helpers.
constexpr PeerId kInvalidPeerId = 0;

// "No object" sentinel.
constexpr NetworkId kInvalidNetworkId = 0;

// First NetworkId handed out for runtime-spawned objects. Ids below this belong
// to scene-placed nodes (assigned deterministically); ids at or above it are
// allocated by the authority for objects created at runtime, so the two schemes
// never collide.
constexpr NetworkId kFirstSpawnedNetworkId = 0x40000000u;

// Engine-level wire protocol version. Bumped when the framing/message layout
// changes incompatibly. Games may additionally set a per-project protocol
// version + game id in NetworkConfig to reject mismatched builds.
// v2: added the InterestUpdate message kind (area-of-interest replication).
// v3: added InputCommand / SnapshotAck / Resume kinds (client prediction,
//     snapshot sequencing + acks, reconnection/resume) and a per-snapshot
//     sequence number in the snapshot frame header.
constexpr uint32_t kEngineProtocolVersion = 3;

/**
 * The role this instance plays in the current session.
 */
enum class SessionMode : uint8_t {
    Offline = 0,  // No session: the whole subsystem is a no-op (zero per-frame cost).
    Server,       // Authoritative server with no local player (dedicated).
    Host,         // Authoritative server that is also a local player (listen server).
    Client,       // Connected to a remote authoritative server.
    P2P           // Peer-to-peer mesh: no central authority, ownership is per-object.
};

/**
 * Logical delivery channel for a message. The transport maps these onto its own
 * facilities (ENet uses two real channels; TCP/WebSocket carries both over one
 * ordered stream but preserves the logical channel in the frame header).
 */
enum class Channel : uint8_t {
    ReliableOrdered = 0,     // Handshake, RPC(reliable), spawn/despawn, ownership.
    UnreliableSequenced = 1  // Snapshots, network-transform updates, ping.
};

constexpr uint8_t kChannelCount = 2;

/**
 * Who is permitted to invoke an RPC.
 */
enum class RpcMode : uint8_t {
    Authority = 0,  // Only the node's authority may call it; runs on the others.
    AnyPeer         // Any peer may call it (server still validates / relays).
};

/**
 * Delivery guarantee requested for an RPC.
 */
enum class TransferMode : uint8_t {
    Reliable = 0,
    Unreliable
};

/**
 * Per-method RPC configuration, declared in a script via `--@rpc` (or registered
 * for a component type via the C-API / native API). 
 */
struct RpcConfig {
    RpcMode mode = RpcMode::Authority;        // who may invoke it
    TransferMode transfer = TransferMode::Reliable;
    bool callLocal = false;                   // also run on the calling peer
};

/**
 * Which concrete transport backs the session.
 */
enum class TransportKind : uint8_t {
    Loopback = 0,  // In-process; single-player / automated tests.
    ENet,          // Native reliable-UDP (desktop/mobile); client-server and P2P.
    WebSocket      // TCP WebSocket; native server + browser/native clients.
};

/**
 * Connection state of a single peer.
 */
enum class PeerState : uint8_t {
    Connecting = 0,
    Connected,
    Disconnecting,
    Disconnected
};

/**
 * Session configuration passed to the Start* / Connect entry points.
 */
struct NetworkConfig {
    TransportKind transport = TransportKind::ENet;

    // Client: address of the server to reach. Server/Host: bind address
    // ("" or "0.0.0.0" binds all interfaces).
    std::string address = "127.0.0.1";
    uint16_t port = 7777;

    uint32_t maxPeers = 32;

    // Per-project protocol gate. A peer whose (protocolVersion, gameId) does not
    // match the server's is rejected with connection_failed, so incompatible
    // builds cannot corrupt one another.
    uint32_t protocolVersion = 1;
    std::string gameId = "lupine-game";

    float tickRate = 30.0f;        // Replication snapshot send rate (Hz).
    float interpDelayMs = 100.0f;  // Default client interpolation buffer delay.
    uint32_t snapshotKbCap = 256;  // Hard cap on a single received frame (KiB).

    // Period between reliable full-state keyframe snapshots. The per-tick delta
    // stream is unreliable, so a dropped delta for a value that then stops
    // changing would otherwise be lost forever; a periodic reliable keyframe
    // re-establishes the full state and also brings late joiners up to date.
    // 0 disables keyframes (delta-only).
    float keyframeIntervalMs = 1000.0f;

    // Interval between RTT probes the manager sends to each peer (seconds). The
    // measured round-trip is surfaced on PeerInfo::roundTripMs. 0 disables probing.
    float pingIntervalSeconds = 1.0f;

    // Area-of-interest radius for server replication. When > 0 the server sends
    // each client only the server-owned objects within this distance of that
    // client's reported interest position (plus objects marked alwaysRelevant),
    // instead of broadcasting every object to everyone. 0 disables culling (every
    // server-owned object is replicated to every peer, the default behaviour).
    float interestRadius = 0.0f;

    // Connect attempt timeout in seconds (client). If no handshake completes in
    // this window the attempt fails with connection_failed.
    float connectTimeoutSeconds = 10.0f;

    // When hosting natively, also open a WebSocket listener so browser clients
    // can join the same session (cross-play). Requires the WebSocket transport
    // to be compiled in.
    bool alsoListenWebSocket = false;
    uint16_t webSocketPort = 7778;

    // Addresses to dial when starting a P2P session (each "host:port"). The local
    // instance also listens on `port` so other peers can dial back.
    std::vector<std::string> p2pPeers;

    // LAN discovery. When a server/host starts with enableLanDiscovery, it answers
    // UDP discovery probes on discoveryPort so clients on the same subnet can find
    // it without knowing its address. serverName is the human-readable label shown
    // in a server browser. Discovery is independent of the gameplay transport.
    bool enableLanDiscovery = false;
    uint16_t discoveryPort = 7779;
    std::string serverName = "Lupine Server";

    // ------------------------------------------------------------------
    // Client-side prediction.
    // ------------------------------------------------------------------

    // Master toggle. When on, an object carrying a NetworkController whose
    // controller peer is the local client predicts its motion from local input
    // each tick and reconciles against the authority's snapshots. When off the
    // controller still ships input to the authority but applies no prediction
    // (the object follows interpolated authority state like any other).
    bool enablePrediction = true;

    // How many of the most recent input commands a client repeats in every
    // InputCommand packet. The command stream rides the unreliable channel, so
    // resending the last few commands lets the server recover a dropped packet's
    // input from a later one without a reliable round trip. 1 = no redundancy.
    uint32_t inputRedundancy = 3;

    // ------------------------------------------------------------------
    // Reconnection / session resume (client).
    // ------------------------------------------------------------------

    // When a client loses an established connection unexpectedly, automatically
    // attempt to reconnect (up to reconnectAttempts, reconnectDelaySeconds apart)
    // and resume the prior session, keeping the same peer id and replicated
    // objects if the server still holds the slot.
    bool autoReconnect = false;
    uint32_t reconnectAttempts = 3;
    float reconnectDelaySeconds = 2.0f;

    // Server: how long a disconnected peer's id + owned objects are retained so a
    // reconnecting client can resume rather than rejoin fresh. 0 disables resume
    // (a dropped peer is removed immediately, the default).
    float resumeTimeoutSeconds = 0.0f;

    // ------------------------------------------------------------------
    // Anti-flood guards (server). A peer exceeding either inbound budget over a
    // one-second window is force-disconnected. 0 disables that guard.
    // ------------------------------------------------------------------
    uint32_t maxMessagesPerSecond = 0;
    uint32_t maxBytesPerSecond = 0;
};

/**
 * Lightweight, read-only view of a connected peer for the public API / scripts.
 * The trailing fields are live diagnostics, recomputed by the manager each second.
 */
struct PeerInfo {
    PeerId id = kInvalidPeerId;
    std::string address;
    float roundTripMs = 0.0f;
    PeerState state = PeerState::Disconnected;
    bool isLocal = false;

    // Diagnostics (see NetworkManager stats). Rates are over a one-second window.
    float bytesInPerSec = 0.0f;
    float bytesOutPerSec = 0.0f;
    float packetLossPercent = 0.0f;  // Estimated from gaps in received snapshot seq.
    float jitterMs = 0.0f;           // Mean deviation of snapshot inter-arrival time.
    uint64_t totalBytesIn = 0;
    uint64_t totalBytesOut = 0;
};

/**
 * Aggregate, session-wide networking diagnostics. Snapshot of the live counters
 * the manager maintains; safe to poll every frame for a debug overlay / HUD.
 */
struct NetworkStats {
    uint32_t peerCount = 0;
    float bytesInPerSec = 0.0f;          // Summed across peers.
    float bytesOutPerSec = 0.0f;
    uint64_t totalBytesIn = 0;
    uint64_t totalBytesOut = 0;
    float averageRttMs = 0.0f;
    float averagePacketLossPercent = 0.0f;
    uint64_t snapshotsSent = 0;
    uint64_t snapshotsReceived = 0;
    uint64_t messagesSent = 0;
    uint64_t messagesReceived = 0;
};

} // namespace network
} // namespace lupine
