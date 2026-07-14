#include "lupine/network/ENetTransport.hpp"
#include "lupine/logger/Logger.hpp"

#ifdef LUPINE_HAS_ENET

#include <enet/enet.h>
#include <cstring>

namespace lupine {
namespace network {

namespace {

// ENet requires a single global initialize/deinitialize, reference-counted here
// so multiple transports (e.g. a host running ENet + a separate WebSocket
// listener, or sequential sessions) share one initialization.
int g_ENetRefCount = 0;

bool AcquireENet() {
    if (g_ENetRefCount == 0) {
        if (enet_initialize() != 0) {
            LOG_NETWORK_ERROR("Failed to initialize ENet");
            return false;
        }
    }
    ++g_ENetRefCount;
    return true;
}

void ReleaseENet() {
    if (g_ENetRefCount > 0) {
        --g_ENetRefCount;
        if (g_ENetRefCount == 0) {
            enet_deinitialize();
        }
    }
}

} // namespace

ENetTransport::ENetTransport() = default;

ENetTransport::~ENetTransport() {
    Shutdown();
}

bool ENetTransport::StartServer(const NetworkConfig& config) {
    if (m_Host != nullptr) {
        return false;
    }
    if (!AcquireENet()) {
        return false;
    }

    ENetAddress address;
    if (config.address.empty() || config.address == "0.0.0.0") {
        address.host = ENET_HOST_ANY;
    } else if (enet_address_set_host(&address, config.address.c_str()) != 0) {
        address.host = ENET_HOST_ANY;
    }
    address.port = config.port;

    m_Host = enet_host_create(&address,
                              config.maxPeers,
                              kChannelCount,
                              0, 0);
    if (m_Host == nullptr) {
        LOG_NETWORK_ERROR("ENet: failed to create server host on port {}", config.port);
        ReleaseENet();
        return false;
    }
    m_MaxFrameBytes = config.snapshotKbCap * 1024u;
    LOG_NETWORK_INFO("ENet: server listening on port {}", config.port);
    return true;
}

bool ENetTransport::Connect(const NetworkConfig& config) {
    if (m_Host != nullptr) {
        return false;
    }
    if (!AcquireENet()) {
        return false;
    }

    m_Host = enet_host_create(nullptr, 1, kChannelCount, 0, 0);
    if (m_Host == nullptr) {
        LOG_NETWORK_ERROR("ENet: failed to create client host");
        ReleaseENet();
        return false;
    }
    m_MaxFrameBytes = config.snapshotKbCap * 1024u;

    ENetAddress address;
    if (enet_address_set_host(&address, config.address.c_str()) != 0) {
        LOG_NETWORK_ERROR("ENet: could not resolve address '{}'", config.address);
        enet_host_destroy(m_Host);
        m_Host = nullptr;
        ReleaseENet();
        return false;
    }
    address.port = config.port;

    ENetPeer* peer = enet_host_connect(m_Host, &address, kChannelCount, 0);
    if (peer == nullptr) {
        LOG_NETWORK_ERROR("ENet: no available peers to initiate a connection");
        enet_host_destroy(m_Host);
        m_Host = nullptr;
        ReleaseENet();
        return false;
    }
    // Reserve the local id now; the CONNECT event confirms the link.
    LocalIdForPeer(peer, true);
    LOG_NETWORK_INFO("ENet: connecting to {}:{}", config.address, config.port);
    return true;
}

PeerId ENetTransport::LocalIdForPeer(_ENetPeer* peer, bool createIfMissing) {
    ENetPeer* enetPeer = reinterpret_cast<ENetPeer*>(peer);
    if (enetPeer->data != nullptr) {
        return static_cast<PeerId>(reinterpret_cast<uintptr_t>(enetPeer->data));
    }
    if (!createIfMissing) {
        return kInvalidPeerId;
    }
    const PeerId id = m_NextLocalId++;
    enetPeer->data = reinterpret_cast<void*>(static_cast<uintptr_t>(id));
    m_PeerById[id] = peer;
    return id;
}

void ENetTransport::DisconnectPeer(PeerId transportPeer) {
    auto it = m_PeerById.find(transportPeer);
    if (it == m_PeerById.end()) {
        return;
    }
    enet_peer_disconnect(reinterpret_cast<ENetPeer*>(it->second), 0);
}

void ENetTransport::Shutdown() {
    if (m_Host == nullptr) {
        return;
    }
    // Best-effort graceful disconnect of all peers, then force-destroy.
    for (auto& entry : m_PeerById) {
        enet_peer_disconnect_now(reinterpret_cast<ENetPeer*>(entry.second), 0);
    }
    enet_host_destroy(m_Host);
    m_Host = nullptr;
    m_PeerById.clear();
    m_Inbound.clear();
    m_NextLocalId = 1;
    ReleaseENet();
}

void ENetTransport::DrainServiceLoop() {
    ENetEvent event;
    while (m_Host != nullptr && enet_host_service(m_Host, &event, 0) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT: {
                const PeerId id = LocalIdForPeer(event.peer, true);
                TransportEvent te;
                te.type = TransportEvent::Type::PeerConnected;
                te.peer = id;
                m_Inbound.push_back(std::move(te));
                break;
            }
            case ENET_EVENT_TYPE_RECEIVE: {
                const PeerId id = LocalIdForPeer(event.peer, false);
                if (id != kInvalidPeerId && event.packet != nullptr &&
                    event.packet->dataLength <= m_MaxFrameBytes) {
                    TransportEvent te;
                    te.type = TransportEvent::Type::DataReceived;
                    te.peer = id;
                    te.channel = (event.channelID == static_cast<uint8_t>(Channel::UnreliableSequenced))
                                     ? Channel::UnreliableSequenced
                                     : Channel::ReliableOrdered;
                    te.data.assign(event.packet->data,
                                   event.packet->data + event.packet->dataLength);
                    m_Inbound.push_back(std::move(te));
                }
                if (event.packet != nullptr) {
                    enet_packet_destroy(event.packet);
                }
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT: {
                const PeerId id = LocalIdForPeer(event.peer, false);
                if (id != kInvalidPeerId) {
                    TransportEvent te;
                    te.type = TransportEvent::Type::PeerDisconnected;
                    te.peer = id;
                    m_Inbound.push_back(std::move(te));
                    m_PeerById.erase(id);
                }
                if (event.peer != nullptr) {
                    event.peer->data = nullptr;
                }
                break;
            }
            default:
                break;
        }
    }
}

void ENetTransport::Service() {
    DrainServiceLoop();
}

bool ENetTransport::PollEvent(TransportEvent& out) {
    if (m_Inbound.empty()) {
        return false;
    }
    out = std::move(m_Inbound.front());
    m_Inbound.pop_front();
    return true;
}

void ENetTransport::Send(PeerId transportPeer, Channel channel,
                         const uint8_t* data, size_t size) {
    auto it = m_PeerById.find(transportPeer);
    if (it == m_PeerById.end() || m_Host == nullptr) {
        return;
    }
    const enet_uint32 flags = (channel == Channel::ReliableOrdered)
                                  ? ENET_PACKET_FLAG_RELIABLE
                                  : 0u;
    ENetPacket* packet = enet_packet_create(data, size, flags);
    if (packet == nullptr) {
        return;
    }
    enet_peer_send(reinterpret_cast<ENetPeer*>(it->second),
                   static_cast<enet_uint8>(channel), packet);
}

TransportCapabilities ENetTransport::GetCapabilities() const {
    TransportCapabilities caps;
    caps.canListen = true;
    caps.hasUnreliable = true;
    caps.isInProcess = false;
    caps.name = "enet";
    return caps;
}

} // namespace network
} // namespace lupine

#else  // LUPINE_HAS_ENET

namespace lupine {
namespace network {

ENetTransport::ENetTransport() = default;
ENetTransport::~ENetTransport() = default;

bool ENetTransport::StartServer(const NetworkConfig&) { return false; }
bool ENetTransport::Connect(const NetworkConfig&) { return false; }
void ENetTransport::DisconnectPeer(PeerId) {}
void ENetTransport::Shutdown() {}
void ENetTransport::Service() {}
bool ENetTransport::PollEvent(TransportEvent&) { return false; }
void ENetTransport::Send(PeerId, Channel, const uint8_t*, size_t) {}

TransportCapabilities ENetTransport::GetCapabilities() const {
    TransportCapabilities caps;
    caps.canListen = false;
    caps.hasUnreliable = false;
    caps.isInProcess = false;
    caps.name = "enet-disabled";
    return caps;
}

} // namespace network
} // namespace lupine

#endif  // LUPINE_HAS_ENET
