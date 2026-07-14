#pragma once

#include "lupine/network/INetworkTransport.hpp"
#include <cstdint>
#include <deque>
#include <unordered_map>

namespace lupine {
namespace network {

/**
 * In-process transport. No sockets: a "server" registers itself under its port
 * in a process-global table, and a "client" Connect() to that port links the two
 * instances directly, delivering each Send() straight into the other endpoint's
 * inbound event queue. Used for single-player Host sessions (zero network
 * overhead) and for automated tests that drive a Host and a Client - each backed
 * by its own NetworkManager - inside one process.
 */
class LoopbackTransport : public INetworkTransport {
public:
    LoopbackTransport() = default;
    ~LoopbackTransport() override;

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
    // One end of a bidirectional in-process link.
    struct Link {
        LoopbackTransport* other = nullptr;
        PeerId remoteLocalId = kInvalidPeerId;  // our id as seen by `other`
    };

    void DeliverConnected(PeerId localId);
    void Enqueue(TransportEvent&& event);

    std::deque<TransportEvent> m_Inbound;
    std::unordered_map<PeerId, Link> m_Links;  // our-local-peer-id -> link
    PeerId m_NextLocalId = 1;
    uint16_t m_ServerPort = 0;
    bool m_IsServer = false;
};

} // namespace network
} // namespace lupine
