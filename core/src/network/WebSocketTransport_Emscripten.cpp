#include "lupine/network/WebSocketTransport.hpp"
#include "lupine/logger/Logger.hpp"

#ifdef __EMSCRIPTEN__

#include <emscripten/websocket.h>

#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace lupine {
namespace network {

// Browser WebSocket client. The browser event loop is single-threaded, so the
// callbacks run on the main thread and no locking is needed. Browsers cannot
// accept inbound connections, so this is client-only (StartServer returns false).
// The browser WebSocket callbacks must be plain C-style function pointers, so
// they live as static members of Impl. Being members gives them access to the
// private nested Impl type that the void* userData is cast back to.
struct WebSocketTransport::Impl {
    std::deque<TransportEvent> inbound;
    EMSCRIPTEN_WEBSOCKET_T socket = 0;
    bool open = false;

    static EM_BOOL OnOpen(int eventType, const EmscriptenWebSocketOpenEvent* e, void* userData);
    static EM_BOOL OnMessage(int eventType, const EmscriptenWebSocketMessageEvent* e, void* userData);
    static EM_BOOL OnClose(int eventType, const EmscriptenWebSocketCloseEvent* e, void* userData);
    static EM_BOOL OnError(int eventType, const EmscriptenWebSocketErrorEvent* e, void* userData);
};

EM_BOOL WebSocketTransport::Impl::OnOpen(int /*eventType*/, const EmscriptenWebSocketOpenEvent* /*e*/, void* userData) {
    auto* impl = static_cast<Impl*>(userData);
    impl->open = true;
    TransportEvent event;
    event.type = TransportEvent::Type::PeerConnected;
    event.peer = kServerPeerId;
    impl->inbound.push_back(std::move(event));
    return EM_TRUE;
}

EM_BOOL WebSocketTransport::Impl::OnMessage(int /*eventType*/, const EmscriptenWebSocketMessageEvent* e, void* userData) {
    auto* impl = static_cast<Impl*>(userData);
    if (e->numBytes > 0 && e->data != nullptr) {
        TransportEvent event;
        event.type = TransportEvent::Type::DataReceived;
        event.peer = kServerPeerId;
        event.data.assign(e->data, e->data + e->numBytes);
        impl->inbound.push_back(std::move(event));
    }
    return EM_TRUE;
}

EM_BOOL WebSocketTransport::Impl::OnClose(int /*eventType*/, const EmscriptenWebSocketCloseEvent* /*e*/, void* userData) {
    auto* impl = static_cast<Impl*>(userData);
    impl->open = false;
    TransportEvent event;
    event.type = TransportEvent::Type::PeerDisconnected;
    event.peer = kServerPeerId;
    impl->inbound.push_back(std::move(event));
    return EM_TRUE;
}

EM_BOOL WebSocketTransport::Impl::OnError(int /*eventType*/, const EmscriptenWebSocketErrorEvent* /*e*/, void* userData) {
    auto* impl = static_cast<Impl*>(userData);
    if (!impl->open) {
        TransportEvent event;
        event.type = TransportEvent::Type::PeerDisconnected;
        event.peer = kServerPeerId;
        impl->inbound.push_back(std::move(event));
    }
    return EM_TRUE;
}

WebSocketTransport::WebSocketTransport() : m_Impl(std::make_unique<Impl>()) {}

WebSocketTransport::~WebSocketTransport() {
    Shutdown();
}

bool WebSocketTransport::StartServer(const NetworkConfig& /*config*/) {
    LOG_NETWORK_WARN("WebSocket: browsers cannot host; use a native/relay server");
    return false;
}

bool WebSocketTransport::Connect(const NetworkConfig& config) {
    if (!emscripten_websocket_is_supported()) {
        LOG_NETWORK_ERROR("WebSocket: not supported in this browser");
        return false;
    }
    if (m_Impl->socket != 0) {
        return false;
    }
    const std::string url = "ws://" + config.address + ":" + std::to_string(config.port);

    EmscriptenWebSocketCreateAttributes attr;
    emscripten_websocket_init_create_attributes(&attr);
    attr.url = url.c_str();
    attr.protocols = nullptr;
    attr.createOnMainThread = EM_TRUE;

    const EMSCRIPTEN_WEBSOCKET_T socket = emscripten_websocket_new(&attr);
    if (socket <= 0) {
        LOG_NETWORK_ERROR("WebSocket: failed to create socket for {}", url);
        return false;
    }
    m_Impl->socket = socket;

    emscripten_websocket_set_onopen_callback(socket, m_Impl.get(), Impl::OnOpen);
    emscripten_websocket_set_onmessage_callback(socket, m_Impl.get(), Impl::OnMessage);
    emscripten_websocket_set_onclose_callback(socket, m_Impl.get(), Impl::OnClose);
    emscripten_websocket_set_onerror_callback(socket, m_Impl.get(), Impl::OnError);
    LOG_NETWORK_INFO("WebSocket: connecting to {}", url);
    return true;
}

void WebSocketTransport::DisconnectPeer(PeerId /*transportPeer*/) {
    if (m_Impl->socket != 0) {
        emscripten_websocket_close(m_Impl->socket, 1000, "client disconnect");
    }
}

void WebSocketTransport::Shutdown() {
    if (m_Impl->socket != 0) {
        emscripten_websocket_close(m_Impl->socket, 1000, "shutdown");
        emscripten_websocket_delete(m_Impl->socket);
        m_Impl->socket = 0;
    }
    m_Impl->open = false;
    m_Impl->inbound.clear();
}

void WebSocketTransport::Service() {
    // The browser event loop delivers callbacks; nothing to pump here.
}

bool WebSocketTransport::PollEvent(TransportEvent& out) {
    if (m_Impl->inbound.empty()) {
        return false;
    }
    out = std::move(m_Impl->inbound.front());
    m_Impl->inbound.pop_front();
    return true;
}

void WebSocketTransport::Send(PeerId /*transportPeer*/, Channel /*channel*/,
                             const uint8_t* data, size_t size) {
    if (m_Impl->socket != 0 && m_Impl->open && data != nullptr) {
        emscripten_websocket_send_binary(m_Impl->socket,
                                         const_cast<void*>(static_cast<const void*>(data)),
                                         static_cast<uint32_t>(size));
    }
}

TransportCapabilities WebSocketTransport::GetCapabilities() const {
    TransportCapabilities caps;
    caps.canListen = false;
    caps.hasUnreliable = false;
    caps.isInProcess = false;
    caps.name = "websocket-emscripten";
    return caps;
}

} // namespace network
} // namespace lupine

#endif  // __EMSCRIPTEN__
