#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace lupine {
namespace network {

/**
 * A server seen on (or advertised to) the local network. On the discovering side
 * `address` and `ageSeconds` are filled from the responding datagram; the rest is
 * carried in the advertisement.
 */
struct LanServerInfo {
    std::string gameId;             // Per-project id; only matching servers are surfaced.
    std::string name = "Lupine Server";  // Human-readable label for a server browser.
    std::string address;            // Filled from the responder's IP (discovering side).
    uint16_t gamePort = 0;          // Port a client should Connect() to.
    uint32_t playerCount = 0;
    uint32_t maxPlayers = 0;
    uint32_t protocolVersion = 0;
    float ageSeconds = 0.0f;        // Seconds since last heard (discovering side).
};

/**
 * UDP-broadcast LAN discovery, independent of the gameplay transport.
 *
 * A host advertises by answering discovery probes on a well-known UDP port with a
 * datagram describing itself (game id, name, gameplay port, player counts). A
 * client discovers by broadcasting probes on that port and collecting the
 * replies, so it can present a server browser and Connect() without the user
 * typing an address. Both roles are non-blocking and pumped once per frame from
 * Poll(); discovery works while the manager is otherwise Offline.
 *
 * Uses only OS UDP sockets (no third-party dependency), so it is available even
 * in a build without the ENet / WebSocket transports.
 */
class LanDiscovery {
public:
    LanDiscovery();
    ~LanDiscovery();

    LanDiscovery(const LanDiscovery&) = delete;
    LanDiscovery& operator=(const LanDiscovery&) = delete;

    // ------------------------------------------------------------------
    // Advertiser (server / host)
    // ------------------------------------------------------------------

    // Begin answering probes on `discoveryPort` with `info`. Returns false if the
    // socket could not be created or bound.
    bool StartAdvertising(uint16_t discoveryPort, const LanServerInfo& info);

    // Update the advertised payload (e.g. current player count) without rebinding.
    void UpdateAdvertisement(const LanServerInfo& info);

    void StopAdvertising();
    bool IsAdvertising() const;

    // ------------------------------------------------------------------
    // Discoverer (client / server browser)
    // ------------------------------------------------------------------

    // Begin discovering servers on `discoveryPort`. Only servers whose advertised
    // game id equals `gameId` are surfaced (empty matches any). Returns false if
    // the broadcast socket could not be created.
    bool StartDiscovery(uint16_t discoveryPort, const std::string& gameId);

    // Broadcast a probe immediately. Poll() also re-probes periodically.
    void SendProbe();

    void StopDiscovery();
    bool IsDiscovering() const;

    // ------------------------------------------------------------------
    // Per-frame driving
    // ------------------------------------------------------------------

    // Service both roles: answer inbound probes, collect responses, age out stale
    // entries, and re-probe on a fixed cadence. No-op when neither role is active.
    void Poll(float deltaTime);

    // Servers heard recently, most-recently-seen first.
    std::vector<LanServerInfo> GetServers() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

} // namespace network
} // namespace lupine
