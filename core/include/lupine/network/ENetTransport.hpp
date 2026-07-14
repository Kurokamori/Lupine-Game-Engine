#pragma once

#include "lupine/network/INetworkTransport.hpp"
#include <cstdint>
#include <deque>
#include <unordered_map>

// Forward declarations of the ENet C structs so this header never pulls in
// <enet/enet.h> (kept entirely inside the .cpp). Includers of the networking
// stack therefore do not need ENet on their include path.
struct _ENetHost;
struct _ENetPeer;

namespace lupine {
namespace network {

/**
 * Native reliable-UDP transport built on ENet. Backs client-server, host
 * (listen-server), dedicated-server, and peer-to-peer sessions on desktop and
 * mobile. ENet channel 0 carries reliable-ordered traffic, channel 1 carries
 * unreliable-sequenced traffic. When the networking feature is compiled out the
 * methods degrade to no-ops returning false, so the surrounding code still
 * links.
 */
class ENetTransport : public INetworkTransport {
public:
    ENetTransport();
    ~ENetTransport() override;

    bool StartServer(const NetworkConfig& config) override;
    bool Connect(const NetworkConfig& config) override;
    void DisconnectPeer(PeerId transportPeer) override;
    void Shutdown() override;
    void Service() override;
    bool PollEvent(TransportEvent& out) override;
    void Send(PeerId transportPeer, Channel channel,
              const uint8_t* data, size_t size) override;
    TransportCapabilities GetCapabilities() const override;

private:
#ifdef LUPINE_HAS_ENET
    // Resolve our stable local id for an ENet peer, assigning one on first sight.
    PeerId LocalIdForPeer(_ENetPeer* peer, bool createIfMissing);
    void DrainServiceLoop();

    _ENetHost* m_Host = nullptr;
    std::unordered_map<PeerId, _ENetPeer*> m_PeerById;
    std::deque<TransportEvent> m_Inbound;
    PeerId m_NextLocalId = 1;
    uint32_t m_MaxFrameBytes = 256u * 1024u;
#endif
};

} // namespace network
} // namespace lupine
