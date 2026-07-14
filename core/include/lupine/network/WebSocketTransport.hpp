#pragma once

#include "lupine/network/INetworkTransport.hpp"
#include <memory>

namespace lupine {
namespace network {

/**
 * WebSocket transport. Two platform implementations sit behind one PIMPL:
 *
 *  - Native (ixwebsocket): a WebSocketTransport can both listen (ix::WebSocketServer)
 *    and connect (ix::WebSocket), so a native host can accept browser clients for
 *    cross-play. Requires the ixwebsocket package; when it is absent the methods
 *    degrade to no-ops (LUPINE_HAS_WEBSOCKET undefined).
 *  - Emscripten (browser): uses the browser's WebSocket API. Browsers can only
 *    open outbound connections, so StartServer() returns false - the web build is
 *    a WebSocket CLIENT that joins a native (or relay) host.
 *
 * WebSocket is a single reliable, ordered TCP stream, so both logical channels
 * share it; the logical channel is preserved in the NetworkManager frame header,
 * which the receiver reads, so snapshots and RPCs remain distinguishable.
 */
class WebSocketTransport : public INetworkTransport {
public:
    WebSocketTransport();
    ~WebSocketTransport() override;

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
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

} // namespace network
} // namespace lupine
