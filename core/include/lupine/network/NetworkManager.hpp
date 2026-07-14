#pragma once

#include "lupine/network/NetworkTypes.hpp"
#include "lupine/network/NetworkMessage.hpp"
#include "lupine/network/NetworkSerializer.hpp"
#include "lupine/network/INetworkTransport.hpp"
#include "lupine/network/LanDiscovery.hpp"
#include "lupine/core/SignalObject.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace lupine {
namespace network {

class RpcManager;
class ReplicationManager;
class PredictionManager;

/**
 * Central networking subsystem: owns the active transport(s), tracks peers and
 * the local identity, drives the connection handshake, and routes messages.
 *
 * Designed so that non-networked games pay nothing: while the session mode is
 * Offline, Poll()/Flush() return on their first line and no component, script,
 * or C-API networking call does any work. Networking is therefore entirely
 * opt-in - a game becomes networked only once it explicitly starts or joins a
 * session.
 *
 * The RPC and replication layers attach by registering handlers for the gameplay
 * message kinds (RegisterMessageHandler) rather than being baked in here, so the
 * manager has no compile-time dependency on them.
 *
 * Like the engine's other managers (AudioManager, LocalizationManager) this is a
 * process-default singleton via GetInstance(); the constructor is nonetheless
 * public so tests can stand up independent Host/Client instances in one process
 * over the loopback transport.
 */
class NetworkManager {
public:
    // Handler invoked for an incoming gameplay message. `reader` is positioned at
    // the message payload (the channel + type header has already been consumed).
    using MessageHandler = std::function<void(PeerId from, ByteReader& reader)>;

    NetworkManager();
    ~NetworkManager();

    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;

    // Process-default instance used by the engine, scripting, and C-API.
    static NetworkManager& GetInstance();

    void Initialize();
    void Shutdown();
    bool IsInitialized() const { return m_Initialized; }

    // ------------------------------------------------------------------
    // Session control
    // ------------------------------------------------------------------

    // Start an authoritative server with no local player (dedicated). Returns
    // false if the transport could not start listening.
    bool StartServer(const NetworkConfig& config);

    // Start an authoritative server that is also a local player (listen server).
    bool StartHost(const NetworkConfig& config);

    // Connect to a remote authoritative server. Returns false only if the attempt
    // could not be started; success/failure otherwise arrives as the
    // connected_to_server / connection_failed signal.
    bool Connect(const NetworkConfig& config);

    // Start a peer-to-peer session: listen for inbound peers and dial each of
    // config.p2pPeers. Authority is per-object (ownership), with no central server.
    bool StartP2P(const NetworkConfig& config);

    // Leave the session and return to Offline. Safe to call when already offline.
    void Disconnect();

    // ------------------------------------------------------------------
    // State queries
    // ------------------------------------------------------------------

    bool IsActive() const { return m_Mode != SessionMode::Offline; }
    SessionMode GetMode() const { return m_Mode; }
    const NetworkConfig& GetConfig() const { return m_Config; }

    // Project-derived default session config. Populated on project load from the
    // project's networking settings (SceneManager::ApplyProjectSettings) and used
    // as the base by the scripting / C-API session-start helpers, so a game
    // inherits the project's transport, rates, and discovery options without
    // restating them. Untouched while a session is active.
    void SetDefaultConfig(const NetworkConfig& config) { m_DefaultConfig = config; }
    const NetworkConfig& GetDefaultConfig() const { return m_DefaultConfig; }

    PeerId GetLocalPeerId() const { return m_LocalPeer; }

    // True on the authoritative side (Server or Host). In P2P this is false;
    // authority is decided per object via ownership.
    bool IsServer() const { return m_Mode == SessionMode::Server || m_Mode == SessionMode::Host; }
    bool IsClient() const { return m_Mode == SessionMode::Client; }

    // True if this instance is the authority for an object owned by `ownerPeer`.
    // Server/Host is authority over everything; in P2P/client a peer is authority
    // only over objects it owns.
    bool HasAuthorityOver(PeerId ownerPeer) const;

    std::vector<PeerInfo> GetPeers() const;     // Remote peers (excludes self).
    size_t GetPeerCount() const { return m_Peers.size(); }
    bool HasPeer(PeerId peer) const { return m_Peers.count(peer) != 0; }

    // Forcibly disconnect a peer (server/host only). The other peers are notified
    // via the normal peer_disconnected path. Returns false if not the authority
    // or the peer is unknown.
    bool KickPeer(PeerId peer);

    // ------------------------------------------------------------------
    // Per-frame driving (called by SceneManager)
    // ------------------------------------------------------------------

    // Receive pass: pump transports, decode frames, route them, queue events.
    void Poll(float deltaTime);

    // Send pass: flush queued outbound traffic.
    void Flush();

    // Fixed-tick replication: the authority gathers changed state and sends a
    // snapshot (rate-limited to the configured tick rate). No-op when offline.
    // Called from SceneManager::PhysicsUpdate after physics has solved.
    void GenerateSnapshot(float fixedDeltaTime);

    // Raw payload (bytes after the channel+type header) of the message currently
    // being dispatched to a registered handler. Lets a handler relay the exact
    // bytes it received without re-encoding. Empty outside of handler dispatch.
    const std::vector<uint8_t>& GetCurrentPayload() const { return m_CurrentPayload; }

    // ------------------------------------------------------------------
    // Signals (peer_connected, peer_disconnected, connected_to_server,
    // connection_failed, server_disconnected). Scripts / C-API connect here.
    // ------------------------------------------------------------------
    core::SignalObject& Events() { return m_Events; }

    // ------------------------------------------------------------------
    // Messaging (used by the RPC / replication layers and custom messages)
    // ------------------------------------------------------------------

    // Send an already-encoded payload to a single peer. (Named SendTo rather
    // than SendMessage to avoid the Windows <winuser.h> SendMessage macro.)
    void SendTo(PeerId to, Channel channel, MsgType type,
                const std::vector<uint8_t>& payload);

    // Send to every connected (handshaken) peer except `except`.
    void BroadcastMessage(Channel channel, MsgType type,
                          const std::vector<uint8_t>& payload,
                          PeerId except = kInvalidPeerId);

    // Register the handler for a gameplay message kind. Replaces any existing
    // handler for that kind. Built-in session kinds cannot be overridden.
    void RegisterMessageHandler(MsgType type, MessageHandler handler);

    // ------------------------------------------------------------------
    // LAN discovery (UDP broadcast, independent of the gameplay transport).
    // A listening session started with NetworkConfig::enableLanDiscovery
    // advertises automatically; these methods also allow manual control and
    // let a not-yet-connected client browse for servers on the local subnet.
    // ------------------------------------------------------------------

    // Begin advertising this (already started) server/host on the local subnet.
    bool StartLanAdvertising();

    // Stop answering discovery probes (does not affect an active discovery scan).
    void StopLanAdvertising();
    bool IsLanAdvertising() const;

    // Begin / stop scanning the local subnet for servers of `gameId`. Works while
    // Offline so a client can populate a server browser before connecting.
    bool StartLanDiscovery(const std::string& gameId, uint16_t discoveryPort);
    void StopLanDiscovery();
    bool IsLanDiscovering() const;

    // Servers heard recently, most-recently-seen first.
    std::vector<LanServerInfo> GetDiscoveredServers() const;

    // ------------------------------------------------------------------
    // Area-of-interest. Report the local player's focus position so a server
    // running with NetworkConfig::interestRadius can cull replication to what is
    // near this client. No-op unless a session is active.
    // ------------------------------------------------------------------
    void SetInterestPosition2D(float x, float y);
    void SetInterestPosition3D(float x, float y, float z);

    // ------------------------------------------------------------------
    // Diagnostics. Live byte/message/loss/jitter counters, recomputed each
    // second. Cheap to poll every frame for a debug overlay / HUD.
    // ------------------------------------------------------------------
    NetworkStats GetStats() const;
    bool GetPeerInfo(PeerId peer, PeerInfo& out) const;

    // Reported by the replication layer so the manager can count snapshots and,
    // from the per-snapshot sequence number, estimate per-peer packet loss and
    // jitter. `seq` is the sender's monotonic snapshot sequence.
    void NotifySnapshotSent();
    void NotifySnapshotReceived(PeerId from, uint32_t seq);

    // ------------------------------------------------------------------
    // Sub-managers (owned). RPC dispatch and object replication are scoped to
    // this manager instance, so independent Host/Client instances in a test do
    // not share routing/registry state.
    // ------------------------------------------------------------------
    RpcManager& Rpc();
    ReplicationManager& Replication();
    PredictionManager& Prediction();

private:
    // Live diagnostics for one peer. Totals accumulate forever; the *Window
    // counters are drained into per-second rates by UpdateStats() and reset.
    struct PeerStats {
        uint64_t bytesInTotal = 0;
        uint64_t bytesOutTotal = 0;
        uint64_t bytesInWindow = 0;
        uint64_t bytesOutWindow = 0;
        uint64_t messagesInWindow = 0;

        // Snapshot sequence tracking for loss + jitter estimation.
        uint32_t lastSnapshotSeq = 0;
        bool hasSnapshotSeq = false;
        uint32_t snapshotsReceivedWindow = 0;
        uint32_t snapshotsLostWindow = 0;
        double lastSnapshotArrivalMs = 0.0;
        bool hasArrival = false;
        float jitterMs = 0.0f;
    };

    struct PeerLink {
        PeerId id = kInvalidPeerId;
        size_t transportIndex = 0;
        PeerId transportPeer = kInvalidPeerId;
        PeerInfo info;
        bool handshaken = false;
        PeerStats stats;

        // Session resume. resumeToken is the secret this peer must present to
        // reclaim its id. While awaitingResume is set the link is a "ghost": kept
        // after an unexpected drop (others are not told it left, its objects are
        // retained) until resumeDeadlineMs passes or it reconnects.
        uint32_t resumeToken = 0;
        bool awaitingResume = false;
        double resumeDeadlineMs = 0.0;
    };

    using TransportKey = std::pair<size_t, PeerId>;

    std::unique_ptr<INetworkTransport> CreateTransport(TransportKind kind);
    bool BeginSession(SessionMode mode, const NetworkConfig& config, bool listen, bool dial);
    void ResetToOffline();

    void HandleTransportEvent(size_t transportIndex, const TransportEvent& event);
    void HandleConnected(size_t transportIndex, PeerId transportPeer);
    void HandleDisconnected(size_t transportIndex, PeerId transportPeer);
    void HandleData(size_t transportIndex, PeerId transportPeer,
                    Channel channel, const std::vector<uint8_t>& data);

    void OnHandshake(size_t transportIndex, PeerId transportPeer, const HandshakeMsg& msg);
    void OnWelcome(const WelcomeMsg& msg);
    void OnResume(size_t transportIndex, PeerId transportPeer, const ResumeMsg& msg);

    // Client: enter the reconnect loop after an unexpected drop (autoReconnect).
    // Returns true if a reconnect was scheduled (so the caller skips going offline).
    bool BeginReconnect();
    void SendResume(size_t transportIndex, PeerId transportPeer);
    void UpdateReconnect(float deltaTime);

    // Server: drop ghost peers whose resume window has expired (finalize the
    // disconnect the retention deferred).
    void UpdateResumeRetention();
    void FinalizePeerRemoval(PeerId globalId, bool notifyOthers);

    // Probe each directly-connected peer's round-trip time on a fixed cadence and
    // record the result on PeerInfo::roundTripMs (smoothed). Driven from Poll().
    void UpdatePings(float deltaTime);
    void OnPong(PeerId from, uint32_t token);

    void SendHandshake(size_t transportIndex, PeerId transportPeer);
    void SendToTransportPeer(size_t transportIndex, PeerId transportPeer,
                             Channel channel, MsgType type,
                             const std::vector<uint8_t>& payload);

    PeerId AddPeer(size_t transportIndex, PeerId transportPeer, PeerId globalId);
    void RemovePeerByGlobalId(PeerId globalId);
    PeerId GlobalIdFor(size_t transportIndex, PeerId transportPeer) const;

    void EmitSignal(const std::string& signal, const core::SignalArgs& args);

    // Build the advertised description from the active config + live player count.
    LanServerInfo BuildAdvertisement() const;

    // Attribute byte/message traffic to a peer (and the session totals).
    void RecordBytesOut(PeerId globalPeer, size_t bytes);
    void RecordBytesIn(PeerId globalPeer, size_t bytes);

    // Once-per-second window: fold byte windows into per-second rates, finalize
    // loss/jitter, and enforce the anti-flood budgets.
    void UpdateStats(float deltaTime);

    SessionMode m_Mode = SessionMode::Offline;
    NetworkConfig m_Config;
    NetworkConfig m_DefaultConfig;  // Project-derived base for session-start helpers.
    std::vector<std::unique_ptr<INetworkTransport>> m_Transports;

    PeerId m_LocalPeer = kInvalidPeerId;
    PeerId m_NextAssignedPeer = kServerPeerId + 1;

    std::unordered_map<PeerId, PeerLink> m_Peers;     // global id -> link
    std::map<TransportKey, PeerId> m_TransportToGlobal;

    std::unordered_map<uint8_t, MessageHandler> m_Handlers;
    std::vector<uint8_t> m_CurrentPayload;

    std::unique_ptr<RpcManager> m_Rpc;
    std::unique_ptr<ReplicationManager> m_Replication;
    std::unique_ptr<PredictionManager> m_Prediction;
    std::unique_ptr<LanDiscovery> m_Discovery;

    core::SignalObject m_Events;

    // Client connect tracking.
    bool m_HandshakeComplete = false;
    float m_ConnectTimer = 0.0f;
    size_t m_ServerTransportIndex = 0;
    PeerId m_ServerTransportPeer = kInvalidPeerId;

    // Session resume (client side). m_ResumeToken / m_ResumePeerId are remembered
    // from the Welcome so a dropped client can reclaim its id; the reconnect loop
    // retries the connection up to m_ReconnectAttemptsLeft times.
    uint32_t m_ResumeToken = 0;
    PeerId m_ResumePeerId = kInvalidPeerId;
    bool m_Reconnecting = false;
    bool m_AttemptingResume = false;
    float m_ReconnectTimer = 0.0f;
    uint32_t m_ReconnectAttemptsLeft = 0;

    // RTT probing. m_PingSent maps an outstanding probe token to the steady-clock
    // millisecond timestamp it was sent at; a matching Pong yields the round trip.
    float m_PingAccum = 0.0f;
    uint32_t m_NextPingToken = 1;
    std::unordered_map<uint32_t, double> m_PingSent;

    // Aggregate diagnostics. The window counters are drained into per-second rates
    // by UpdateStats(); the totals accumulate for the whole session.
    float m_StatsAccum = 0.0f;
    uint64_t m_TotalBytesIn = 0;
    uint64_t m_TotalBytesOut = 0;
    uint64_t m_MessagesIn = 0;
    uint64_t m_MessagesOut = 0;
    uint64_t m_SnapshotsSent = 0;
    uint64_t m_SnapshotsReceived = 0;

    bool m_Initialized = false;
};

} // namespace network
} // namespace lupine
