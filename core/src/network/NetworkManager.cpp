#include "lupine/network/NetworkManager.hpp"
#include "lupine/network/NetworkProtocol.hpp"
#include "lupine/network/LoopbackTransport.hpp"
#include "lupine/network/ENetTransport.hpp"
#include "lupine/network/WebSocketTransport.hpp"
#include "lupine/network/RpcManager.hpp"
#include "lupine/network/ReplicationManager.hpp"
#include "lupine/network/PredictionManager.hpp"
#include "lupine/logger/Logger.hpp"

#include <chrono>
#include <random>
#include <set>
#include <utility>

namespace lupine {
namespace network {

namespace {

// Monotonic millisecond clock for round-trip timing.
double NowMs() {
    using namespace std::chrono;
    return static_cast<double>(
        duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count()) / 1000.0;
}

// Self-assigned peer id for P2P sessions (no central authority to assign one).
PeerId GenerateP2PId() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(kServerPeerId + 1, 0xFFFFFFFEu);
    return dist(gen);
}

// Random non-zero resume token. A reconnecting client must echo the token it was
// given to reclaim its id, so a third party cannot hijack a held slot.
uint32_t GenerateResumeToken() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(1u, 0xFFFFFFFFu);
    return dist(gen);
}

// Split a "host:port" entry. Falls back to defaultPort when no port is present.
void ParseEndpoint(const std::string& endpoint, uint16_t defaultPort,
                   std::string& outHost, uint16_t& outPort) {
    const std::size_t colon = endpoint.find_last_of(':');
    if (colon == std::string::npos) {
        outHost = endpoint;
        outPort = defaultPort;
        return;
    }
    outHost = endpoint.substr(0, colon);
    const std::string portStr = endpoint.substr(colon + 1);
    int parsed = 0;
    try {
        parsed = std::stoi(portStr);
    } catch (...) {
        parsed = defaultPort;
    }
    outPort = static_cast<uint16_t>(parsed);
}

} // namespace

NetworkManager::NetworkManager()
    : m_Rpc(std::make_unique<RpcManager>(*this)),
      m_Replication(std::make_unique<ReplicationManager>(*this)),
      m_Prediction(std::make_unique<PredictionManager>(*this)) {}

NetworkManager::~NetworkManager() {
    Shutdown();
}

RpcManager& NetworkManager::Rpc() {
    return *m_Rpc;
}

ReplicationManager& NetworkManager::Replication() {
    return *m_Replication;
}

PredictionManager& NetworkManager::Prediction() {
    return *m_Prediction;
}

NetworkManager& NetworkManager::GetInstance() {
    static NetworkManager instance;
    return instance;
}

void NetworkManager::Initialize() {
    if (m_Initialized) {
        return;
    }
    m_Events.AddUserSignal("peer_connected", {{"peer", core::PropertyValueType::Int}});
    m_Events.AddUserSignal("peer_disconnected", {{"peer", core::PropertyValueType::Int}});
    m_Events.AddUserSignal("connected_to_server", {});
    m_Events.AddUserSignal("connection_failed", {{"reason", core::PropertyValueType::String}});
    m_Events.AddUserSignal("server_disconnected", {});
    m_Events.AddUserSignal("reconnecting", {{"attempt", core::PropertyValueType::Int}});
    m_Events.AddUserSignal("reconnected", {});

    // Sub-managers register their message handlers on this manager.
    m_Rpc->Initialize();
    m_Replication->Initialize();
    m_Prediction->Initialize();

    m_Initialized = true;
}

void NetworkManager::Shutdown() {
    Disconnect();
    m_Discovery.reset();  // Tears down advertise + discovery sockets.
    m_Handlers.clear();
    m_Initialized = false;
}

// ============================================================================
// Session control
// ============================================================================

std::unique_ptr<INetworkTransport> NetworkManager::CreateTransport(TransportKind kind) {
    switch (kind) {
        case TransportKind::Loopback:
            return std::make_unique<LoopbackTransport>();
        case TransportKind::ENet:
            return std::make_unique<ENetTransport>();
        case TransportKind::WebSocket:
            return std::make_unique<WebSocketTransport>();
    }
    return nullptr;
}

bool NetworkManager::BeginSession(SessionMode mode, const NetworkConfig& config,
                                  bool listen, bool dial) {
    if (IsActive()) {
        Disconnect();
    }

    std::unique_ptr<INetworkTransport> transport = CreateTransport(config.transport);
    if (!transport) {
        EmitSignal("connection_failed", {std::string("transport unavailable")});
        return false;
    }

    if (listen && !transport->StartServer(config)) {
        if (!dial) {
            LOG_NETWORK_ERROR("Failed to start listening on port {}", config.port);
            return false;
        }
        LOG_NETWORK_WARN("P2P: could not bind a listener on port {} (dial-only)", config.port);
    }

    if (dial && mode == SessionMode::Client) {
        if (!transport->Connect(config)) {
            EmitSignal("connection_failed", {std::string("could not start connection")});
            return false;
        }
    }

    m_Config = config;
    m_Mode = mode;
    m_Transports.push_back(std::move(transport));

    // Cross-play: a native listening session can also open a WebSocket listener so
    // browser clients can join the same session alongside ENet clients.
    if (listen && config.alsoListenWebSocket && config.transport != TransportKind::WebSocket) {
        std::unique_ptr<INetworkTransport> wsTransport = CreateTransport(TransportKind::WebSocket);
        if (wsTransport) {
            NetworkConfig wsConfig = config;
            wsConfig.transport = TransportKind::WebSocket;
            wsConfig.port = config.webSocketPort;
            if (wsTransport->StartServer(wsConfig)) {
                m_Transports.push_back(std::move(wsTransport));
                LOG_NETWORK_INFO("Also listening for WebSocket clients on port {}", config.webSocketPort);
            }
        }
    }

    if (mode == SessionMode::Server || mode == SessionMode::Host) {
        m_LocalPeer = kServerPeerId;
        m_NextAssignedPeer = kServerPeerId + 1;
    } else if (mode == SessionMode::P2P) {
        m_LocalPeer = GenerateP2PId();
    } else {
        m_LocalPeer = kInvalidPeerId;  // Client: assigned by the server.
        m_HandshakeComplete = false;
        m_ConnectTimer = 0.0f;
        m_ServerTransportIndex = 0;
        m_ServerTransportPeer = kInvalidPeerId;
    }

    // P2P dials each configured peer on the same (listening) transport.
    if (dial && mode == SessionMode::P2P) {
        for (const std::string& endpoint : config.p2pPeers) {
            NetworkConfig dialConfig = config;
            ParseEndpoint(endpoint, config.port, dialConfig.address, dialConfig.port);
            m_Transports[0]->Connect(dialConfig);
        }
    }

    // A listening session opts into LAN discovery so clients can find it.
    if (listen && config.enableLanDiscovery) {
        StartLanAdvertising();
    }

    return true;
}

bool NetworkManager::StartServer(const NetworkConfig& config) {
    const bool ok = BeginSession(SessionMode::Server, config, /*listen*/ true, /*dial*/ false);
    if (ok) {
        LOG_NETWORK_INFO("Server started on port {}", config.port);
    }
    return ok;
}

bool NetworkManager::StartHost(const NetworkConfig& config) {
    const bool ok = BeginSession(SessionMode::Host, config, /*listen*/ true, /*dial*/ false);
    if (ok) {
        LOG_NETWORK_INFO("Host started on port {} (local peer {})", config.port, m_LocalPeer);
    }
    return ok;
}

bool NetworkManager::Connect(const NetworkConfig& config) {
    const bool ok = BeginSession(SessionMode::Client, config, /*listen*/ false, /*dial*/ true);
    if (ok) {
        LOG_NETWORK_INFO("Connecting to {}:{}", config.address, config.port);
    }
    return ok;
}

bool NetworkManager::StartP2P(const NetworkConfig& config) {
    const bool ok = BeginSession(SessionMode::P2P, config, /*listen*/ true, /*dial*/ true);
    if (ok) {
        LOG_NETWORK_INFO("P2P session started on port {} (local peer {})", config.port, m_LocalPeer);
    }
    return ok;
}

void NetworkManager::Disconnect() {
    if (m_Mode == SessionMode::Offline) {
        return;
    }
    for (std::unique_ptr<INetworkTransport>& transport : m_Transports) {
        if (transport) {
            transport->Shutdown();
        }
    }
    ResetToOffline();
    LOG_NETWORK_INFO("Networking session ended");
}

void NetworkManager::ResetToOffline() {
    if (m_Replication) {
        m_Replication->OnSessionEnded();
    }
    if (m_Prediction) {
        m_Prediction->OnSessionEnded();
    }
    StopLanAdvertising();  // A browsing (discovery) scan, if any, is left running.
    m_Transports.clear();
    m_Peers.clear();
    m_TransportToGlobal.clear();
    m_Mode = SessionMode::Offline;
    m_LocalPeer = kInvalidPeerId;
    m_NextAssignedPeer = kServerPeerId + 1;
    m_HandshakeComplete = false;
    m_ConnectTimer = 0.0f;
    m_ServerTransportIndex = 0;
    m_ServerTransportPeer = kInvalidPeerId;
    m_PingAccum = 0.0f;
    m_PingSent.clear();
    m_StatsAccum = 0.0f;
    m_TotalBytesIn = 0;
    m_TotalBytesOut = 0;
    m_MessagesIn = 0;
    m_MessagesOut = 0;
    m_SnapshotsSent = 0;
    m_SnapshotsReceived = 0;
    m_Reconnecting = false;
    m_AttemptingResume = false;
    m_ReconnectTimer = 0.0f;
    m_ReconnectAttemptsLeft = 0;
    m_ResumeToken = 0;
    m_ResumePeerId = kInvalidPeerId;
}

// ============================================================================
// Per-frame driving
// ============================================================================

void NetworkManager::Poll(float deltaTime) {
    // LAN discovery runs independently of the gameplay session so a client can
    // browse for servers while still Offline; pump it before the offline guard.
    if (m_Discovery) {
        if (m_Discovery->IsAdvertising()) {
            m_Discovery->UpdateAdvertisement(BuildAdvertisement());
        }
        m_Discovery->Poll(deltaTime);
    }

    if (m_Mode == SessionMode::Offline) {
        return;
    }

    // The reconnect loop owns connection timing while reconnecting; the normal
    // first-connect timeout only applies to the very first attempt (an initial
    // connect failure is reported as connection_failed, not auto-reconnected).
    if (m_Mode == SessionMode::Client && !m_HandshakeComplete && !m_Reconnecting) {
        m_ConnectTimer += deltaTime;
        if (m_ConnectTimer > m_Config.connectTimeoutSeconds) {
            LOG_NETWORK_WARN("Connection attempt timed out after {:.1f}s", m_ConnectTimer);
            EmitSignal("connection_failed", {std::string("timeout")});
            Disconnect();
            return;
        }
    }

    UpdateReconnect(deltaTime);
    if (m_Mode == SessionMode::Offline) {
        return;  // Reconnect gave up.
    }

    for (size_t i = 0; i < m_Transports.size(); ++i) {
        m_Transports[i]->Service();
        TransportEvent event;
        while (m_Transports[i]->PollEvent(event)) {
            HandleTransportEvent(i, event);
            // A disconnect handled mid-drain can drop the session offline or enter
            // the reconnect loop (which rebuilds the transport list); in either
            // case stop touching the now-stale transports.
            if (m_Mode == SessionMode::Offline || i >= m_Transports.size()) {
                return;
            }
        }
    }

    UpdatePings(deltaTime);
    UpdateStats(deltaTime);
    UpdateResumeRetention();
}

void NetworkManager::Flush() {
    if (m_Mode == SessionMode::Offline) {
        return;
    }
    for (std::unique_ptr<INetworkTransport>& transport : m_Transports) {
        transport->Service();
    }
}

void NetworkManager::GenerateSnapshot(float fixedDeltaTime) {
    if (m_Mode == SessionMode::Offline) {
        return;
    }
    // Prediction runs first so the snapshot reflects this tick's freshly-simulated
    // authoritative state (and the last-processed input sequence per controller).
    m_Prediction->Tick(fixedDeltaTime);
    m_Replication->GenerateSnapshot(fixedDeltaTime);
}

// ============================================================================
// Event routing
// ============================================================================

void NetworkManager::HandleTransportEvent(size_t transportIndex, const TransportEvent& event) {
    switch (event.type) {
        case TransportEvent::Type::PeerConnected:
            HandleConnected(transportIndex, event.peer);
            break;
        case TransportEvent::Type::PeerDisconnected:
            HandleDisconnected(transportIndex, event.peer);
            break;
        case TransportEvent::Type::DataReceived:
            HandleData(transportIndex, event.peer, event.channel, event.data);
            break;
        case TransportEvent::Type::None:
            break;
    }
}

void NetworkManager::HandleConnected(size_t transportIndex, PeerId transportPeer) {
    switch (m_Mode) {
        case SessionMode::Client:
            m_ServerTransportIndex = transportIndex;
            m_ServerTransportPeer = transportPeer;
            m_TransportToGlobal[{transportIndex, transportPeer}] = kServerPeerId;
            // A reconnect that holds a resume token tries to reclaim its old id;
            // a fresh connect (or token-less reconnect) does a normal handshake.
            if (m_Reconnecting && m_AttemptingResume && m_ResumeToken != 0) {
                SendResume(transportIndex, transportPeer);
            } else {
                SendHandshake(transportIndex, transportPeer);
            }
            break;
        case SessionMode::P2P:
            SendHandshake(transportIndex, transportPeer);
            break;
        case SessionMode::Server:
        case SessionMode::Host:
            // Passive: wait for the joiner's handshake before assigning an id.
            break;
        case SessionMode::Offline:
            break;
    }
}

void NetworkManager::HandleDisconnected(size_t transportIndex, PeerId transportPeer) {
    const PeerId id = GlobalIdFor(transportIndex, transportPeer);

    if (m_Mode == SessionMode::Client) {
        const bool isServerConn = transportIndex == m_ServerTransportIndex &&
                                  transportPeer == m_ServerTransportPeer;
        if (isServerConn) {
            if (m_Reconnecting) {
                // A reconnect attempt's socket dropped; discard it so the loop
                // schedules the next attempt rather than failing outright.
                for (std::unique_ptr<INetworkTransport>& t : m_Transports) {
                    if (t) {
                        t->Shutdown();
                    }
                }
                m_Transports.clear();
                m_TransportToGlobal.clear();
                return;
            }
            if (m_HandshakeComplete && m_Config.autoReconnect && BeginReconnect()) {
                return;  // Entering the reconnect loop; session state is retained.
            }
            if (!m_HandshakeComplete) {
                EmitSignal("connection_failed", {std::string("disconnected during handshake")});
            } else {
                EmitSignal("server_disconnected", {});
            }
            Disconnect();
            return;
        }
        if (id != kInvalidPeerId) {
            RemovePeerByGlobalId(id);
            EmitSignal("peer_disconnected", {static_cast<int64_t>(id)});
        }
        return;
    }

    if (id == kInvalidPeerId) {
        return;
    }

    // Server/Host: optionally retain the dropped peer's slot so it can resume in
    // place. Others are not told it left and its objects are kept until the
    // resume window lapses (UpdateResumeRetention finalizes the removal then).
    if (IsServer() && m_Config.resumeTimeoutSeconds > 0.0f) {
        auto it = m_Peers.find(id);
        if (it != m_Peers.end() && it->second.handshaken && it->second.resumeToken != 0) {
            it->second.handshaken = false;
            it->second.awaitingResume = true;
            it->second.resumeDeadlineMs = NowMs() + m_Config.resumeTimeoutSeconds * 1000.0;
            it->second.info.state = PeerState::Disconnected;
            m_TransportToGlobal.erase({transportIndex, transportPeer});
            LOG_NETWORK_INFO("Peer {} dropped; holding slot {:.1f}s for resume", id,
                             m_Config.resumeTimeoutSeconds);
            return;
        }
    }

    m_TransportToGlobal.erase({transportIndex, transportPeer});
    FinalizePeerRemoval(id, /*notifyOthers*/ true);
}

void NetworkManager::FinalizePeerRemoval(PeerId globalId, bool notifyOthers) {
    if (notifyOthers && IsServer()) {
        ByteWriter writer;
        protocol::EncodePeerEvent(writer, PeerEventMsg{globalId});
        BroadcastMessage(Channel::ReliableOrdered, MsgType::PeerLeft, writer.Data(), globalId);
    }
    RemovePeerByGlobalId(globalId);
    EmitSignal("peer_disconnected", {static_cast<int64_t>(globalId)});
}

void NetworkManager::HandleData(size_t transportIndex, PeerId transportPeer,
                                Channel channel, const std::vector<uint8_t>& data) {
    (void)channel;
    ByteReader reader(data);
    Channel frameChannel = Channel::ReliableOrdered;
    MsgType type = MsgType::Custom;
    if (!protocol::ReadFrameHeader(reader, frameChannel, type)) {
        LOG_NETWORK_WARN("Dropped malformed frame from transport peer {}", transportPeer);
        return;
    }

    const PeerId from = GlobalIdFor(transportIndex, transportPeer);
    RecordBytesIn(from, data.size());

    switch (type) {
        case MsgType::Handshake: {
            HandshakeMsg msg;
            if (protocol::DecodeHandshake(reader, msg)) {
                OnHandshake(transportIndex, transportPeer, msg);
            }
            break;
        }
        case MsgType::Welcome: {
            WelcomeMsg msg;
            if (protocol::DecodeWelcome(reader, msg)) {
                OnWelcome(msg);
            }
            break;
        }
        case MsgType::Resume: {
            ResumeMsg msg;
            if (protocol::DecodeResume(reader, msg)) {
                OnResume(transportIndex, transportPeer, msg);
            }
            break;
        }
        case MsgType::PeerJoined: {
            PeerEventMsg msg;
            if (protocol::DecodePeerEvent(reader, msg) && msg.peerId != m_LocalPeer) {
                PeerLink link;
                link.id = msg.peerId;
                link.transportIndex = m_ServerTransportIndex;
                link.transportPeer = m_ServerTransportPeer;
                link.handshaken = true;
                link.info.id = msg.peerId;
                link.info.state = PeerState::Connected;
                m_Peers[msg.peerId] = link;
                EmitSignal("peer_connected", {static_cast<int64_t>(msg.peerId)});
            }
            break;
        }
        case MsgType::PeerLeft: {
            PeerEventMsg msg;
            if (protocol::DecodePeerEvent(reader, msg)) {
                RemovePeerByGlobalId(msg.peerId);
                EmitSignal("peer_disconnected", {static_cast<int64_t>(msg.peerId)});
            }
            break;
        }
        case MsgType::Ping: {
            PingMsg msg;
            if (protocol::DecodePing(reader, msg)) {
                ByteWriter writer;
                protocol::EncodePong(writer, PongMsg{msg.token});
                SendToTransportPeer(transportIndex, transportPeer,
                                    Channel::UnreliableSequenced, MsgType::Pong, writer.Data());
            }
            break;
        }
        case MsgType::Pong: {
            PongMsg msg;
            if (protocol::DecodePong(reader, msg)) {
                OnPong(from, msg.token);
            }
            break;
        }
        default: {
            auto it = m_Handlers.find(static_cast<uint8_t>(type));
            if (it != m_Handlers.end() && it->second) {
                // Expose the raw payload (bytes after the 2-byte header) so the
                // handler can relay it verbatim without re-encoding.
                m_CurrentPayload.assign(data.begin() + 2, data.end());
                it->second(from, reader);
                m_CurrentPayload.clear();
            } else {
                LOG_NETWORK_DEBUG("No handler for message type {}", static_cast<int>(type));
            }
            break;
        }
    }
}

void NetworkManager::OnHandshake(size_t transportIndex, PeerId transportPeer,
                                 const HandshakeMsg& msg) {
    const bool versionOk = msg.engineProtocol == kEngineProtocolVersion &&
                           msg.gameProtocol == m_Config.protocolVersion &&
                           msg.gameId == m_Config.gameId;
    if (!versionOk) {
        LOG_NETWORK_WARN("Rejecting handshake (protocol/game-id mismatch)");
        m_Transports[transportIndex]->DisconnectPeer(transportPeer);
        return;
    }

    if (m_Mode == SessionMode::Server || m_Mode == SessionMode::Host) {
        // Enforce the peer cap: reject the join when the session is already full
        // so a server cannot be overrun past its configured capacity.
        if (m_Config.maxPeers > 0 && m_Peers.size() >= m_Config.maxPeers) {
            LOG_NETWORK_WARN("Rejecting join: session is full ({} peers)", m_Peers.size());
            m_Transports[transportIndex]->DisconnectPeer(transportPeer);
            return;
        }

        const PeerId id = m_NextAssignedPeer++;
        AddPeer(transportIndex, transportPeer, id);

        // Hand out a resume token so the peer can reclaim this id after a drop.
        const uint32_t token = GenerateResumeToken();
        m_Peers[id].resumeToken = token;

        WelcomeMsg welcome;
        welcome.assignedPeerId = id;
        welcome.serverPeerId = kServerPeerId;
        welcome.resumeToken = token;
        for (const auto& entry : m_Peers) {
            if (entry.first != id) {
                welcome.existingPeers.push_back(entry.first);
            }
        }
        ByteWriter welcomeWriter;
        protocol::EncodeWelcome(welcomeWriter, welcome);
        SendToTransportPeer(transportIndex, transportPeer,
                            Channel::ReliableOrdered, MsgType::Welcome, welcomeWriter.Data());

        ByteWriter joinWriter;
        protocol::EncodePeerEvent(joinWriter, PeerEventMsg{id});
        BroadcastMessage(Channel::ReliableOrdered, MsgType::PeerJoined, joinWriter.Data(), id);

        EmitSignal("peer_connected", {static_cast<int64_t>(id)});
        LOG_NETWORK_INFO("Peer {} joined", id);

        // Bring the joiner up to date: replay spawns + send a full keyframe so it
        // sees the current world immediately instead of only after the next change.
        if (m_Replication) {
            m_Replication->SendInitialStateTo(id);
        }
    } else if (m_Mode == SessionMode::P2P) {
        if (msg.selfId == kInvalidPeerId) {
            m_Transports[transportIndex]->DisconnectPeer(transportPeer);
            return;
        }
        AddPeer(transportIndex, transportPeer, msg.selfId);
        EmitSignal("peer_connected", {static_cast<int64_t>(msg.selfId)});
        LOG_NETWORK_INFO("P2P peer {} connected", msg.selfId);

        // Share the objects this peer owns (its own spawns + current state).
        if (m_Replication) {
            m_Replication->SendInitialStateTo(msg.selfId);
        }
    }
}

void NetworkManager::OnWelcome(const WelcomeMsg& msg) {
    const PeerId reclaimed = m_ResumePeerId;
    const bool wasReconnecting = m_Reconnecting;

    m_LocalPeer = msg.assignedPeerId;
    m_HandshakeComplete = true;
    m_ResumeToken = msg.resumeToken;
    m_ResumePeerId = msg.assignedPeerId;
    m_Reconnecting = false;
    m_AttemptingResume = false;
    m_ReconnectTimer = 0.0f;

    auto addLogicalPeer = [this](PeerId id) {
        if (id == m_LocalPeer || id == kInvalidPeerId) {
            return;
        }
        PeerLink link;
        link.id = id;
        link.transportIndex = m_ServerTransportIndex;
        link.transportPeer = m_ServerTransportPeer;
        link.handshaken = true;
        link.info.id = id;
        link.info.state = PeerState::Connected;
        m_Peers[id] = link;
    };

    // A resume keeps the same id: the other peers never saw this client leave, so
    // do not re-announce them - just rebuild the local table and signal reconnected.
    const bool resumed =
        wasReconnecting && reclaimed != kInvalidPeerId && msg.assignedPeerId == reclaimed;

    addLogicalPeer(msg.serverPeerId);
    for (const PeerId peer : msg.existingPeers) {
        addLogicalPeer(peer);
    }

    if (resumed) {
        EmitSignal("reconnected", {});
        LOG_NETWORK_INFO("Resumed session as peer {}", m_LocalPeer);
        return;
    }

    EmitSignal("connected_to_server", {});
    EmitSignal("peer_connected", {static_cast<int64_t>(msg.serverPeerId)});
    for (const PeerId peer : msg.existingPeers) {
        EmitSignal("peer_connected", {static_cast<int64_t>(peer)});
    }
    LOG_NETWORK_INFO("Connected to server as peer {}", m_LocalPeer);
}

void NetworkManager::OnResume(size_t transportIndex, PeerId transportPeer, const ResumeMsg& msg) {
    if (!IsServer()) {
        return;
    }
    const bool versionOk = msg.engineProtocol == kEngineProtocolVersion &&
                           msg.gameProtocol == m_Config.protocolVersion &&
                           msg.gameId == m_Config.gameId;
    if (!versionOk) {
        m_Transports[transportIndex]->DisconnectPeer(transportPeer);
        return;
    }

    auto it = m_Peers.find(msg.peerId);
    const double now = NowMs();
    if (it != m_Peers.end() && it->second.awaitingResume && msg.resumeToken != 0 &&
        it->second.resumeToken == msg.resumeToken && now <= it->second.resumeDeadlineMs) {
        // Rebind the held slot to the reconnecting socket and resync world state.
        it->second.transportIndex = transportIndex;
        it->second.transportPeer = transportPeer;
        it->second.handshaken = true;
        it->second.awaitingResume = false;
        it->second.info.state = PeerState::Connected;
        m_TransportToGlobal[{transportIndex, transportPeer}] = msg.peerId;

        WelcomeMsg welcome;
        welcome.assignedPeerId = msg.peerId;
        welcome.serverPeerId = kServerPeerId;
        welcome.resumeToken = it->second.resumeToken;
        for (const auto& entry : m_Peers) {
            if (entry.first != msg.peerId && entry.second.handshaken) {
                welcome.existingPeers.push_back(entry.first);
            }
        }
        ByteWriter welcomeWriter;
        protocol::EncodeWelcome(welcomeWriter, welcome);
        SendToTransportPeer(transportIndex, transportPeer,
                            Channel::ReliableOrdered, MsgType::Welcome, welcomeWriter.Data());

        if (m_Replication) {
            m_Replication->SendInitialStateTo(msg.peerId);
        }
        LOG_NETWORK_INFO("Peer {} resumed its session", msg.peerId);
        return;
    }

    // Resume is no longer possible (slot expired / unknown id / bad token): admit
    // the connection as a brand-new joiner instead.
    HandshakeMsg handshake;
    handshake.engineProtocol = msg.engineProtocol;
    handshake.gameProtocol = msg.gameProtocol;
    handshake.gameId = msg.gameId;
    handshake.selfId = kInvalidPeerId;
    OnHandshake(transportIndex, transportPeer, handshake);
}

void NetworkManager::SendResume(size_t transportIndex, PeerId transportPeer) {
    ResumeMsg msg;
    msg.engineProtocol = kEngineProtocolVersion;
    msg.gameProtocol = m_Config.protocolVersion;
    msg.gameId = m_Config.gameId;
    msg.peerId = m_ResumePeerId;
    msg.resumeToken = m_ResumeToken;

    ByteWriter writer;
    protocol::EncodeResume(writer, msg);
    SendToTransportPeer(transportIndex, transportPeer,
                        Channel::ReliableOrdered, MsgType::Resume, writer.Data());
}

bool NetworkManager::BeginReconnect() {
    if (m_Mode != SessionMode::Client || !m_Config.autoReconnect ||
        m_Config.reconnectAttempts == 0) {
        return false;
    }
    // Drop the dead socket but keep replicated objects + the resume token so the
    // session can be reclaimed; the local peer view is rebuilt from the next Welcome.
    for (std::unique_ptr<INetworkTransport>& transport : m_Transports) {
        if (transport) {
            transport->Shutdown();
        }
    }
    m_Transports.clear();
    m_TransportToGlobal.clear();
    m_Peers.clear();
    m_HandshakeComplete = false;
    m_Reconnecting = true;
    m_AttemptingResume = m_ResumeToken != 0;
    m_ReconnectAttemptsLeft = m_Config.reconnectAttempts;
    m_ReconnectTimer = 0.0f;
    m_ServerTransportIndex = 0;
    m_ServerTransportPeer = kInvalidPeerId;
    LOG_NETWORK_INFO("Connection lost; auto-reconnecting (up to {} attempts)",
                     m_Config.reconnectAttempts);
    return true;
}

void NetworkManager::UpdateReconnect(float deltaTime) {
    if (!m_Reconnecting) {
        return;
    }
    m_ReconnectTimer += deltaTime;

    if (m_Transports.empty()) {
        // Between attempts: wait the configured delay, then start the next attempt.
        if (m_ReconnectTimer < m_Config.reconnectDelaySeconds) {
            return;
        }
        m_ReconnectTimer = 0.0f;
        if (m_ReconnectAttemptsLeft == 0) {
            m_Reconnecting = false;
            EmitSignal("server_disconnected", {});
            Disconnect();
            return;
        }
        const uint32_t attemptNumber = m_Config.reconnectAttempts - m_ReconnectAttemptsLeft + 1;
        --m_ReconnectAttemptsLeft;
        EmitSignal("reconnecting", {static_cast<int64_t>(attemptNumber)});

        std::unique_ptr<INetworkTransport> transport = CreateTransport(m_Config.transport);
        if (transport && transport->Connect(m_Config)) {
            m_Transports.push_back(std::move(transport));
            m_ServerTransportIndex = 0;
            m_ServerTransportPeer = kInvalidPeerId;
        }
        // If the attempt could not even start, the next tick waits the delay again.
        return;
    }

    // An attempt is in flight: bound how long we wait for it to complete.
    if (m_ReconnectTimer >= m_Config.connectTimeoutSeconds) {
        for (std::unique_ptr<INetworkTransport>& transport : m_Transports) {
            if (transport) {
                transport->Shutdown();
            }
        }
        m_Transports.clear();
        m_TransportToGlobal.clear();
        m_ReconnectTimer = 0.0f;
        if (m_ReconnectAttemptsLeft == 0) {
            m_Reconnecting = false;
            EmitSignal("server_disconnected", {});
            Disconnect();
        }
    }
}

void NetworkManager::UpdateResumeRetention() {
    if (!IsServer() || m_Config.resumeTimeoutSeconds <= 0.0f) {
        return;
    }
    const double now = NowMs();
    std::vector<PeerId> expired;
    for (const auto& entry : m_Peers) {
        if (entry.second.awaitingResume && now > entry.second.resumeDeadlineMs) {
            expired.push_back(entry.first);
        }
    }
    for (const PeerId id : expired) {
        LOG_NETWORK_INFO("Resume window for peer {} expired; finalizing disconnect", id);
        FinalizePeerRemoval(id, /*notifyOthers*/ true);
    }
}

// ============================================================================
// Round-trip timing
// ============================================================================

void NetworkManager::UpdatePings(float deltaTime) {
    const float interval = m_Config.pingIntervalSeconds;
    if (interval <= 0.0f || m_Peers.empty()) {
        return;
    }

    // Drop probes that never got a Pong (the probe rides the unreliable channel)
    // so the outstanding-token map cannot grow without bound.
    const double now = NowMs();
    for (auto it = m_PingSent.begin(); it != m_PingSent.end();) {
        if (now - it->second > 10000.0) {
            it = m_PingSent.erase(it);
        } else {
            ++it;
        }
    }

    m_PingAccum += deltaTime;
    if (m_PingAccum < interval) {
        return;
    }
    m_PingAccum = 0.0f;

    // One probe per unique transport endpoint - relayed logical peers share a
    // socket, so the round trip is a property of the endpoint, not the peer id.
    std::set<TransportKey> probed;
    for (const auto& entry : m_Peers) {
        const PeerLink& link = entry.second;
        if (!link.handshaken) {
            continue;
        }
        const TransportKey key{link.transportIndex, link.transportPeer};
        if (!probed.insert(key).second) {
            continue;
        }
        const uint32_t token = m_NextPingToken++;
        m_PingSent[token] = NowMs();
        ByteWriter writer;
        protocol::EncodePing(writer, PingMsg{token});
        SendToTransportPeer(link.transportIndex, link.transportPeer,
                            Channel::UnreliableSequenced, MsgType::Ping, writer.Data());
    }
}

void NetworkManager::OnPong(PeerId from, uint32_t token) {
    auto it = m_PingSent.find(token);
    if (it == m_PingSent.end()) {
        return;
    }
    const double rtt = NowMs() - it->second;
    m_PingSent.erase(it);

    auto peerIt = m_Peers.find(from);
    if (peerIt == m_Peers.end()) {
        return;
    }
    float& stored = peerIt->second.info.roundTripMs;
    // Exponential moving average to smooth out per-sample jitter.
    stored = (stored <= 0.0f) ? static_cast<float>(rtt)
                              : stored * 0.8f + static_cast<float>(rtt) * 0.2f;
}

// ============================================================================
// Messaging
// ============================================================================

void NetworkManager::SendHandshake(size_t transportIndex, PeerId transportPeer) {
    HandshakeMsg msg;
    msg.engineProtocol = kEngineProtocolVersion;
    msg.gameProtocol = m_Config.protocolVersion;
    msg.gameId = m_Config.gameId;
    msg.selfId = (m_Mode == SessionMode::P2P) ? m_LocalPeer : kInvalidPeerId;

    ByteWriter writer;
    protocol::EncodeHandshake(writer, msg);
    SendToTransportPeer(transportIndex, transportPeer,
                        Channel::ReliableOrdered, MsgType::Handshake, writer.Data());
}

void NetworkManager::SendToTransportPeer(size_t transportIndex, PeerId transportPeer,
                                         Channel channel, MsgType type,
                                         const std::vector<uint8_t>& payload) {
    if (transportIndex >= m_Transports.size()) {
        return;
    }
    const std::vector<uint8_t> frame = protocol::MakeFrame(channel, type, payload);
    m_Transports[transportIndex]->Send(transportPeer, channel, frame.data(), frame.size());
    RecordBytesOut(GlobalIdFor(transportIndex, transportPeer), frame.size());
}

void NetworkManager::SendTo(PeerId to, Channel channel, MsgType type,
                            const std::vector<uint8_t>& payload) {
    auto it = m_Peers.find(to);
    if (it == m_Peers.end()) {
        return;
    }
    SendToTransportPeer(it->second.transportIndex, it->second.transportPeer,
                        channel, type, payload);
}

void NetworkManager::BroadcastMessage(Channel channel, MsgType type,
                                      const std::vector<uint8_t>& payload, PeerId except) {
    const std::vector<uint8_t> frame = protocol::MakeFrame(channel, type, payload);
    // Dedupe by transport endpoint so a relayed (logical) peer table never sends
    // the same frame to the same socket twice.
    std::set<TransportKey> sent;
    for (const auto& entry : m_Peers) {
        const PeerLink& link = entry.second;
        if (entry.first == except || !link.handshaken) {
            continue;
        }
        const TransportKey key{link.transportIndex, link.transportPeer};
        if (!sent.insert(key).second) {
            continue;
        }
        if (link.transportIndex < m_Transports.size()) {
            m_Transports[link.transportIndex]->Send(link.transportPeer, channel,
                                                    frame.data(), frame.size());
            RecordBytesOut(entry.first, frame.size());
        }
    }
}

void NetworkManager::RegisterMessageHandler(MsgType type, MessageHandler handler) {
    m_Handlers[static_cast<uint8_t>(type)] = std::move(handler);
}

// ============================================================================
// Peer table helpers
// ============================================================================

PeerId NetworkManager::AddPeer(size_t transportIndex, PeerId transportPeer, PeerId globalId) {
    PeerLink link;
    link.id = globalId;
    link.transportIndex = transportIndex;
    link.transportPeer = transportPeer;
    link.handshaken = true;
    link.info.id = globalId;
    link.info.state = PeerState::Connected;
    m_Peers[globalId] = link;
    m_TransportToGlobal[{transportIndex, transportPeer}] = globalId;
    return globalId;
}

void NetworkManager::RemovePeerByGlobalId(PeerId globalId) {
    m_Peers.erase(globalId);
}

PeerId NetworkManager::GlobalIdFor(size_t transportIndex, PeerId transportPeer) const {
    auto it = m_TransportToGlobal.find({transportIndex, transportPeer});
    return (it != m_TransportToGlobal.end()) ? it->second : kInvalidPeerId;
}

// ============================================================================
// Queries
// ============================================================================

bool NetworkManager::HasAuthorityOver(PeerId ownerPeer) const {
    if (m_Mode == SessionMode::Server || m_Mode == SessionMode::Host) {
        return true;
    }
    return ownerPeer == m_LocalPeer;
}

bool NetworkManager::KickPeer(PeerId peer) {
    if (!IsServer()) {
        return false;
    }
    auto it = m_Peers.find(peer);
    if (it == m_Peers.end()) {
        return false;
    }
    const size_t idx = it->second.transportIndex;
    const PeerId tpeer = it->second.transportPeer;
    if (idx < m_Transports.size()) {
        m_Transports[idx]->DisconnectPeer(tpeer);
    }

    // Proactively notify the rest and drop the peer; the transport's own
    // disconnect event (if it surfaces one) then resolves to a harmless no-op.
    ByteWriter writer;
    protocol::EncodePeerEvent(writer, PeerEventMsg{peer});
    BroadcastMessage(Channel::ReliableOrdered, MsgType::PeerLeft, writer.Data(), peer);
    RemovePeerByGlobalId(peer);
    m_TransportToGlobal.erase({idx, tpeer});
    EmitSignal("peer_disconnected", {static_cast<int64_t>(peer)});
    LOG_NETWORK_INFO("Kicked peer {}", peer);
    return true;
}

std::vector<PeerInfo> NetworkManager::GetPeers() const {
    std::vector<PeerInfo> peers;
    peers.reserve(m_Peers.size());
    for (const auto& entry : m_Peers) {
        peers.push_back(entry.second.info);
    }
    return peers;
}

void NetworkManager::EmitSignal(const std::string& signal, const core::SignalArgs& args) {
    m_Events.EmitDeferred(signal, args);
}

// ============================================================================
// Diagnostics
// ============================================================================

void NetworkManager::RecordBytesOut(PeerId globalPeer, size_t bytes) {
    m_TotalBytesOut += bytes;
    ++m_MessagesOut;
    auto it = m_Peers.find(globalPeer);
    if (it != m_Peers.end()) {
        it->second.stats.bytesOutTotal += bytes;
        it->second.stats.bytesOutWindow += bytes;
        it->second.info.totalBytesOut += bytes;
    }
}

void NetworkManager::RecordBytesIn(PeerId globalPeer, size_t bytes) {
    m_TotalBytesIn += bytes;
    ++m_MessagesIn;
    auto it = m_Peers.find(globalPeer);
    if (it != m_Peers.end()) {
        it->second.stats.bytesInTotal += bytes;
        it->second.stats.bytesInWindow += bytes;
        it->second.stats.messagesInWindow += 1;
        it->second.info.totalBytesIn += bytes;
    }
}

void NetworkManager::UpdateStats(float deltaTime) {
    m_StatsAccum += deltaTime;
    if (m_StatsAccum < 1.0f) {
        return;
    }
    const float elapsed = m_StatsAccum;
    m_StatsAccum = 0.0f;

    std::vector<PeerId> toKick;
    const bool guard = IsServer() &&
                       (m_Config.maxMessagesPerSecond > 0 || m_Config.maxBytesPerSecond > 0);

    for (auto& entry : m_Peers) {
        PeerLink& link = entry.second;
        PeerStats& s = link.stats;

        link.info.bytesInPerSec = static_cast<float>(s.bytesInWindow) / elapsed;
        link.info.bytesOutPerSec = static_cast<float>(s.bytesOutWindow) / elapsed;
        link.info.jitterMs = s.jitterMs;

        const uint32_t received = s.snapshotsReceivedWindow;
        const uint32_t lost = s.snapshotsLostWindow;
        const uint32_t total = received + lost;
        link.info.packetLossPercent =
            (total > 0) ? (static_cast<float>(lost) / static_cast<float>(total) * 100.0f) : 0.0f;

        if (guard) {
            const bool msgOver = m_Config.maxMessagesPerSecond > 0 &&
                                 s.messagesInWindow >
                                     static_cast<uint64_t>(m_Config.maxMessagesPerSecond);
            const bool byteOver = m_Config.maxBytesPerSecond > 0 &&
                                  s.bytesInWindow >
                                      static_cast<uint64_t>(m_Config.maxBytesPerSecond);
            if ((msgOver || byteOver) && entry.first != m_LocalPeer) {
                LOG_NETWORK_WARN("Peer {} exceeded inbound budget ({} msg / {} bytes per s); kicking",
                                 entry.first, s.messagesInWindow, s.bytesInWindow);
                toKick.push_back(entry.first);
            }
        }

        s.bytesInWindow = 0;
        s.bytesOutWindow = 0;
        s.messagesInWindow = 0;
        s.snapshotsReceivedWindow = 0;
        s.snapshotsLostWindow = 0;
    }

    for (const PeerId peer : toKick) {
        KickPeer(peer);
    }
}

void NetworkManager::NotifySnapshotSent() {
    ++m_SnapshotsSent;
}

void NetworkManager::NotifySnapshotReceived(PeerId from, uint32_t seq) {
    ++m_SnapshotsReceived;
    auto it = m_Peers.find(from);
    if (it == m_Peers.end()) {
        return;
    }
    PeerStats& s = it->second.stats;
    ++s.snapshotsReceivedWindow;

    // Loss: a sequence jump beyond the next expected value means intermediate
    // snapshots were dropped (the stream rides the unreliable channel).
    if (s.hasSnapshotSeq && seq > s.lastSnapshotSeq + 1) {
        s.snapshotsLostWindow += (seq - s.lastSnapshotSeq - 1);
    }
    if (!s.hasSnapshotSeq || seq > s.lastSnapshotSeq) {
        s.lastSnapshotSeq = seq;
        s.hasSnapshotSeq = true;
    }

    // Jitter: RFC3550-style smoothed mean deviation of inter-arrival time from the
    // nominal tick interval.
    const double now = NowMs();
    if (s.hasArrival) {
        const float tickMs = (m_Config.tickRate > 0.0f) ? (1000.0f / m_Config.tickRate) : 33.3f;
        const double interArrival = now - s.lastSnapshotArrivalMs;
        const float d = static_cast<float>(interArrival) - tickMs;
        const float absd = (d < 0.0f) ? -d : d;
        s.jitterMs += (absd - s.jitterMs) / 16.0f;
    }
    s.lastSnapshotArrivalMs = now;
    s.hasArrival = true;
}

NetworkStats NetworkManager::GetStats() const {
    NetworkStats stats;
    stats.peerCount = static_cast<uint32_t>(m_Peers.size());
    stats.totalBytesIn = m_TotalBytesIn;
    stats.totalBytesOut = m_TotalBytesOut;
    stats.messagesSent = m_MessagesOut;
    stats.messagesReceived = m_MessagesIn;
    stats.snapshotsSent = m_SnapshotsSent;
    stats.snapshotsReceived = m_SnapshotsReceived;

    float rttSum = 0.0f;
    float lossSum = 0.0f;
    uint32_t rttCount = 0;
    for (const auto& entry : m_Peers) {
        const PeerInfo& info = entry.second.info;
        stats.bytesInPerSec += info.bytesInPerSec;
        stats.bytesOutPerSec += info.bytesOutPerSec;
        if (info.roundTripMs > 0.0f) {
            rttSum += info.roundTripMs;
            ++rttCount;
        }
        lossSum += info.packetLossPercent;
    }
    if (rttCount > 0) {
        stats.averageRttMs = rttSum / static_cast<float>(rttCount);
    }
    if (!m_Peers.empty()) {
        stats.averagePacketLossPercent = lossSum / static_cast<float>(m_Peers.size());
    }
    return stats;
}

bool NetworkManager::GetPeerInfo(PeerId peer, PeerInfo& out) const {
    auto it = m_Peers.find(peer);
    if (it == m_Peers.end()) {
        return false;
    }
    out = it->second.info;
    return true;
}

// ============================================================================
// LAN discovery
// ============================================================================

LanServerInfo NetworkManager::BuildAdvertisement() const {
    LanServerInfo info;
    info.gameId = m_Config.gameId;
    info.name = m_Config.serverName;
    info.gamePort = m_Config.port;
    info.maxPlayers = m_Config.maxPeers;
    info.protocolVersion = m_Config.protocolVersion;
    // Connected players = remote peers plus the local player when hosting.
    info.playerCount = static_cast<uint32_t>(m_Peers.size()) + (m_Mode == SessionMode::Host ? 1u : 0u);
    return info;
}

bool NetworkManager::StartLanAdvertising() {
    if (!IsServer() && m_Mode != SessionMode::P2P) {
        LOG_NETWORK_WARN("LAN advertising requires an active server/host/P2P session");
        return false;
    }
    if (!m_Discovery) {
        m_Discovery = std::make_unique<LanDiscovery>();
    }
    return m_Discovery->StartAdvertising(m_Config.discoveryPort, BuildAdvertisement());
}

void NetworkManager::StopLanAdvertising() {
    if (m_Discovery) {
        m_Discovery->StopAdvertising();
    }
}

bool NetworkManager::IsLanAdvertising() const {
    return m_Discovery && m_Discovery->IsAdvertising();
}

bool NetworkManager::StartLanDiscovery(const std::string& gameId, uint16_t discoveryPort) {
    if (!m_Discovery) {
        m_Discovery = std::make_unique<LanDiscovery>();
    }
    return m_Discovery->StartDiscovery(discoveryPort, gameId);
}

void NetworkManager::StopLanDiscovery() {
    if (m_Discovery) {
        m_Discovery->StopDiscovery();
    }
}

bool NetworkManager::IsLanDiscovering() const {
    return m_Discovery && m_Discovery->IsDiscovering();
}

std::vector<LanServerInfo> NetworkManager::GetDiscoveredServers() const {
    if (!m_Discovery) {
        return {};
    }
    return m_Discovery->GetServers();
}

// ============================================================================
// Area-of-interest
// ============================================================================

void NetworkManager::SetInterestPosition2D(float x, float y) {
    if (m_Replication) {
        m_Replication->SetLocalInterest(false, math::Vec3{x, y, 0.0f});
    }
}

void NetworkManager::SetInterestPosition3D(float x, float y, float z) {
    if (m_Replication) {
        m_Replication->SetLocalInterest(true, math::Vec3{x, y, z});
    }
}

} // namespace network
} // namespace lupine
