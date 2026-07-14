#include "lupine/network/LoopbackTransport.hpp"
#include <unordered_map>

namespace lupine {
namespace network {

namespace {

// Process-global registry of loopback servers by port. Loopback "ports" form a
// per-process namespace so a Host and a Client in the same process can find each
// other. Main-thread only (matches the rest of the engine), so no locking.
std::unordered_map<uint16_t, LoopbackTransport*>& Servers() {
    static std::unordered_map<uint16_t, LoopbackTransport*> servers;
    return servers;
}

} // namespace

LoopbackTransport::~LoopbackTransport() {
    Shutdown();
}

bool LoopbackTransport::StartServer(const NetworkConfig& config) {
    if (Servers().count(config.port) != 0) {
        return false;  // Port already taken in this process.
    }
    Servers()[config.port] = this;
    m_ServerPort = config.port;
    m_IsServer = true;
    return true;
}

bool LoopbackTransport::Connect(const NetworkConfig& config) {
    auto it = Servers().find(config.port);
    if (it == Servers().end()) {
        return false;  // No in-process server on that port -> connect fails.
    }
    LoopbackTransport* server = it->second;

    const PeerId clientLocalForServer = m_NextLocalId++;
    const PeerId serverLocalForClient = server->m_NextLocalId++;

    m_Links[clientLocalForServer] = Link{server, serverLocalForClient};
    server->m_Links[serverLocalForClient] = Link{this, clientLocalForServer};

    // Both ends observe the new connection.
    DeliverConnected(clientLocalForServer);
    server->DeliverConnected(serverLocalForClient);
    return true;
}

void LoopbackTransport::DeliverConnected(PeerId localId) {
    TransportEvent event;
    event.type = TransportEvent::Type::PeerConnected;
    event.peer = localId;
    Enqueue(std::move(event));
}

void LoopbackTransport::Enqueue(TransportEvent&& event) {
    m_Inbound.push_back(std::move(event));
}

void LoopbackTransport::DisconnectPeer(PeerId transportPeer) {
    auto it = m_Links.find(transportPeer);
    if (it == m_Links.end()) {
        return;
    }
    LoopbackTransport* other = it->second.other;
    const PeerId remoteLocalId = it->second.remoteLocalId;
    m_Links.erase(it);

    if (other != nullptr) {
        auto otherIt = other->m_Links.find(remoteLocalId);
        if (otherIt != other->m_Links.end()) {
            other->m_Links.erase(otherIt);
        }
        TransportEvent event;
        event.type = TransportEvent::Type::PeerDisconnected;
        event.peer = remoteLocalId;
        other->Enqueue(std::move(event));
    }
}

void LoopbackTransport::Shutdown() {
    // Drop every link, notifying the far end.
    while (!m_Links.empty()) {
        DisconnectPeer(m_Links.begin()->first);
    }
    if (m_IsServer) {
        auto it = Servers().find(m_ServerPort);
        if (it != Servers().end() && it->second == this) {
            Servers().erase(it);
        }
        m_IsServer = false;
    }
    m_Inbound.clear();
    m_NextLocalId = 1;
}

void LoopbackTransport::Service() {
    // Delivery is immediate (Send pushes straight into the peer's queue).
}

bool LoopbackTransport::PollEvent(TransportEvent& out) {
    if (m_Inbound.empty()) {
        return false;
    }
    out = std::move(m_Inbound.front());
    m_Inbound.pop_front();
    return true;
}

void LoopbackTransport::Send(PeerId transportPeer, Channel channel,
                             const uint8_t* data, size_t size) {
    auto it = m_Links.find(transportPeer);
    if (it == m_Links.end() || it->second.other == nullptr) {
        return;
    }
    TransportEvent event;
    event.type = TransportEvent::Type::DataReceived;
    event.peer = it->second.remoteLocalId;
    event.channel = channel;
    if (data != nullptr && size > 0) {
        event.data.assign(data, data + size);
    }
    it->second.other->Enqueue(std::move(event));
}

TransportCapabilities LoopbackTransport::GetCapabilities() const {
    TransportCapabilities caps;
    caps.canListen = true;
    caps.hasUnreliable = true;
    caps.isInProcess = true;
    caps.name = "loopback";
    return caps;
}

} // namespace network
} // namespace lupine
