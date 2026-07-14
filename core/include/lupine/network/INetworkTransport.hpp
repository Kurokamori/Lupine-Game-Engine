#pragma once

#include "lupine/network/NetworkTypes.hpp"
#include <cstdint>
#include <vector>

namespace lupine {
namespace network {

/**
 * A single thing that happened on a transport, drained by NetworkManager once
 * per frame. `peer` is the transport-LOCAL peer id (the transport's own handle
 * for a connection); NetworkManager maps it to a global PeerId.
 */
struct TransportEvent {
    enum class Type : uint8_t {
        None = 0,
        PeerConnected,     // A connection was established (in/out-bound).
        PeerDisconnected,  // A connection was lost / closed.
        DataReceived       // A complete message frame arrived in `data`.
    };

    Type type = Type::None;
    PeerId peer = kInvalidPeerId;  // Transport-local connection id.
    Channel channel = Channel::ReliableOrdered;
    std::vector<uint8_t> data;     // Valid for DataReceived.
};

/**
 * What a transport can do, so NetworkManager can degrade gracefully (e.g. a
 * browser WebSocket client cannot listen for inbound connections).
 */
struct TransportCapabilities {
    bool canListen = true;       // Can accept inbound connections (be a server).
    bool hasUnreliable = true;   // Has a real unreliable channel (else everything is reliable).
    bool isInProcess = false;    // No real sockets (loopback) - never fails to "connect".
    std::string name = "transport";
};

/**
 * Abstract message transport. Concrete backends (Loopback, ENet, WebSocket) hide
 * all socket/library specifics behind this interface so the rest of the
 * networking stack is backend-agnostic and new transports can be added without
 * touching NetworkManager. Each Send/received frame is a single discrete,
 * length-delimited message (the backend owns its own framing), so higher layers
 * never need to re-frame a byte stream.
 *
 * Not thread-safe: all calls happen on the main game thread, like the rest of
 * the engine's per-frame subsystems.
 */
class INetworkTransport {
public:
    virtual ~INetworkTransport() = default;

    // Begin listening for inbound connections. Returns false if this transport
    // cannot listen (see TransportCapabilities::canListen) or binding failed.
    virtual bool StartServer(const NetworkConfig& config) = 0;

    // Begin an outbound connection attempt. Success/failure is reported later as
    // a PeerConnected / PeerDisconnected event. Returns false only if the attempt
    // could not be started at all.
    virtual bool Connect(const NetworkConfig& config) = 0;

    // Forcibly drop a single connection by its transport-local id.
    virtual void DisconnectPeer(PeerId transportPeer) = 0;

    // Tear the whole transport down, dropping every connection.
    virtual void Shutdown() = 0;

    // Pump the underlying socket library: flush queued outbound packets and read
    // inbound ones into the event queue. Called by NetworkManager on both the
    // receive (early) and send (late) passes of a frame.
    virtual void Service() = 0;

    // Pop the next pending event. Returns false (leaving `out` untouched) once
    // the queue is drained for this Service() pass.
    virtual bool PollEvent(TransportEvent& out) = 0;

    // Send one complete message frame to a single peer on the given channel.
    virtual void Send(PeerId transportPeer, Channel channel,
                      const uint8_t* data, size_t size) = 0;

    virtual TransportCapabilities GetCapabilities() const = 0;
};

} // namespace network
} // namespace lupine
