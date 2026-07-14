#include "lupine/network/WebSocketTransport.hpp"
#include "lupine/logger/Logger.hpp"

#if defined(LUPINE_HAS_WEBSOCKET) && !defined(__EMSCRIPTEN__)

#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketServer.h>
#include <ixwebsocket/IXNetSystem.h>

#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace lupine {
namespace network {

namespace {
// ixwebsocket needs Winsock init once on Windows; reference-counted.
int g_NetSystemRefs = 0;
std::mutex g_NetSystemMutex;

void AcquireNetSystem() {
    std::lock_guard<std::mutex> lock(g_NetSystemMutex);
    if (g_NetSystemRefs == 0) {
        ix::initNetSystem();
    }
    ++g_NetSystemRefs;
}

void ReleaseNetSystem() {
    std::lock_guard<std::mutex> lock(g_NetSystemMutex);
    if (g_NetSystemRefs > 0) {
        --g_NetSystemRefs;
        if (g_NetSystemRefs == 0) {
            ix::uninitNetSystem();
        }
    }
}
}  // namespace

struct WebSocketTransport::Impl {
    std::mutex mutex;                          // Guards inbound + connections (ixwebsocket callbacks are off-thread).
    std::deque<TransportEvent> inbound;
    std::unordered_map<PeerId, std::shared_ptr<ix::WebSocket>> connections;  // server side
    std::unique_ptr<ix::WebSocketServer> server;
    std::unique_ptr<ix::WebSocket> client;
    PeerId nextLocalId = 1;
    bool isServer = false;
    bool acquired = false;
};

WebSocketTransport::WebSocketTransport() : m_Impl(std::make_unique<Impl>()) {}

WebSocketTransport::~WebSocketTransport() {
    Shutdown();
}

bool WebSocketTransport::StartServer(const NetworkConfig& config) {
    if (m_Impl->server || m_Impl->client) {
        return false;
    }
    AcquireNetSystem();
    m_Impl->acquired = true;
    m_Impl->isServer = true;

    const std::string host = (config.address.empty() || config.address == "0.0.0.0")
                                 ? std::string("0.0.0.0")
                                 : config.address;
    m_Impl->server = std::make_unique<ix::WebSocketServer>(config.port, host);

    Impl* impl = m_Impl.get();
    m_Impl->server->setOnConnectionCallback(
        [impl](std::weak_ptr<ix::WebSocket> webSocketWeak,
               std::shared_ptr<ix::ConnectionState> /*state*/) {
            std::shared_ptr<ix::WebSocket> webSocket = webSocketWeak.lock();
            if (!webSocket) {
                return;
            }
            auto idSlot = std::make_shared<PeerId>(kInvalidPeerId);
            webSocket->setOnMessageCallback(
                [impl, webSocket, idSlot](const ix::WebSocketMessagePtr& msg) {
                    std::lock_guard<std::mutex> lock(impl->mutex);
                    if (msg->type == ix::WebSocketMessageType::Open) {
                        const PeerId id = impl->nextLocalId++;
                        *idSlot = id;
                        impl->connections[id] = webSocket;
                        TransportEvent event;
                        event.type = TransportEvent::Type::PeerConnected;
                        event.peer = id;
                        impl->inbound.push_back(std::move(event));
                    } else if (msg->type == ix::WebSocketMessageType::Message) {
                        if (*idSlot != kInvalidPeerId) {
                            TransportEvent event;
                            event.type = TransportEvent::Type::DataReceived;
                            event.peer = *idSlot;
                            event.data.assign(msg->str.begin(), msg->str.end());
                            impl->inbound.push_back(std::move(event));
                        }
                    } else if (msg->type == ix::WebSocketMessageType::Close) {
                        if (*idSlot != kInvalidPeerId) {
                            TransportEvent event;
                            event.type = TransportEvent::Type::PeerDisconnected;
                            event.peer = *idSlot;
                            impl->inbound.push_back(std::move(event));
                            impl->connections.erase(*idSlot);
                        }
                    }
                });
        });

    std::pair<bool, std::string> result = m_Impl->server->listen();
    if (!result.first) {
        LOG_NETWORK_ERROR("WebSocket: failed to listen on port {}: {}", config.port, result.second);
        m_Impl->server.reset();
        ReleaseNetSystem();
        m_Impl->acquired = false;
        return false;
    }
    m_Impl->server->start();
    LOG_NETWORK_INFO("WebSocket: server listening on port {}", config.port);
    return true;
}

bool WebSocketTransport::Connect(const NetworkConfig& config) {
    if (m_Impl->client) {
        return false;
    }
    AcquireNetSystem();
    m_Impl->acquired = true;
    m_Impl->isServer = false;

    m_Impl->client = std::make_unique<ix::WebSocket>();
    const std::string url = "ws://" + config.address + ":" + std::to_string(config.port);
    m_Impl->client->setUrl(url);

    Impl* impl = m_Impl.get();
    m_Impl->client->setOnMessageCallback([impl](const ix::WebSocketMessagePtr& msg) {
        std::lock_guard<std::mutex> lock(impl->mutex);
        if (msg->type == ix::WebSocketMessageType::Open) {
            TransportEvent event;
            event.type = TransportEvent::Type::PeerConnected;
            event.peer = kServerPeerId;  // The single connection is to the server.
            impl->inbound.push_back(std::move(event));
        } else if (msg->type == ix::WebSocketMessageType::Message) {
            TransportEvent event;
            event.type = TransportEvent::Type::DataReceived;
            event.peer = kServerPeerId;
            event.data.assign(msg->str.begin(), msg->str.end());
            impl->inbound.push_back(std::move(event));
        } else if (msg->type == ix::WebSocketMessageType::Close ||
                   msg->type == ix::WebSocketMessageType::Error) {
            TransportEvent event;
            event.type = TransportEvent::Type::PeerDisconnected;
            event.peer = kServerPeerId;
            impl->inbound.push_back(std::move(event));
        }
    });
    m_Impl->client->start();
    LOG_NETWORK_INFO("WebSocket: connecting to {}", url);
    return true;
}

void WebSocketTransport::DisconnectPeer(PeerId transportPeer) {
    std::lock_guard<std::mutex> lock(m_Impl->mutex);
    if (m_Impl->isServer) {
        auto it = m_Impl->connections.find(transportPeer);
        if (it != m_Impl->connections.end() && it->second) {
            it->second->close();
        }
    } else if (m_Impl->client) {
        m_Impl->client->close();
    }
}

void WebSocketTransport::Shutdown() {
    if (m_Impl->server) {
        m_Impl->server->stop();
        m_Impl->server.reset();
    }
    if (m_Impl->client) {
        m_Impl->client->stop();
        m_Impl->client.reset();
    }
    {
        std::lock_guard<std::mutex> lock(m_Impl->mutex);
        m_Impl->connections.clear();
        m_Impl->inbound.clear();
    }
    if (m_Impl->acquired) {
        ReleaseNetSystem();
        m_Impl->acquired = false;
    }
    m_Impl->nextLocalId = 1;
}

void WebSocketTransport::Service() {
    // ixwebsocket runs its own background threads; nothing to pump here.
}

bool WebSocketTransport::PollEvent(TransportEvent& out) {
    std::lock_guard<std::mutex> lock(m_Impl->mutex);
    if (m_Impl->inbound.empty()) {
        return false;
    }
    out = std::move(m_Impl->inbound.front());
    m_Impl->inbound.pop_front();
    return true;
}

void WebSocketTransport::Send(PeerId transportPeer, Channel channel,
                             const uint8_t* data, size_t size) {
    (void)channel;  // Single TCP stream; logical channel is in the frame header.
    std::string payload(reinterpret_cast<const char*>(data), size);
    std::lock_guard<std::mutex> lock(m_Impl->mutex);
    if (m_Impl->isServer) {
        auto it = m_Impl->connections.find(transportPeer);
        if (it != m_Impl->connections.end() && it->second) {
            it->second->sendBinary(payload);
        }
    } else if (m_Impl->client) {
        m_Impl->client->sendBinary(payload);
    }
}

TransportCapabilities WebSocketTransport::GetCapabilities() const {
    TransportCapabilities caps;
    caps.canListen = true;
    caps.hasUnreliable = false;  // TCP: everything is reliable-ordered.
    caps.isInProcess = false;
    caps.name = "websocket-native";
    return caps;
}

} // namespace network
} // namespace lupine

#else  // WebSocket not available in this native build

namespace lupine {
namespace network {

struct WebSocketTransport::Impl {};

WebSocketTransport::WebSocketTransport() : m_Impl(nullptr) {}
WebSocketTransport::~WebSocketTransport() = default;

bool WebSocketTransport::StartServer(const NetworkConfig&) {
    LOG_NETWORK_WARN("WebSocket transport not built (install ixwebsocket to enable)");
    return false;
}
bool WebSocketTransport::Connect(const NetworkConfig&) {
    LOG_NETWORK_WARN("WebSocket transport not built (install ixwebsocket to enable)");
    return false;
}
void WebSocketTransport::DisconnectPeer(PeerId) {}
void WebSocketTransport::Shutdown() {}
void WebSocketTransport::Service() {}
bool WebSocketTransport::PollEvent(TransportEvent&) { return false; }
void WebSocketTransport::Send(PeerId, Channel, const uint8_t*, size_t) {}

TransportCapabilities WebSocketTransport::GetCapabilities() const {
    TransportCapabilities caps;
    caps.canListen = false;
    caps.hasUnreliable = false;
    caps.isInProcess = false;
    caps.name = "websocket-disabled";
    return caps;
}

} // namespace network
} // namespace lupine

#endif  // LUPINE_HAS_WEBSOCKET && !__EMSCRIPTEN__
