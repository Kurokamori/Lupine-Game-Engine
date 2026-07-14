#include "lupine/network/LanDiscovery.hpp"
#include "lupine/network/NetworkSerializer.hpp"
#include "lupine/logger/Logger.hpp"

#include <algorithm>
#include <cstring>

#if defined(_WIN32)
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  pragma comment(lib, "ws2_32.lib")
using socket_t = SOCKET;
static constexpr socket_t kInvalidSocket = INVALID_SOCKET;
#else
#  include <arpa/inet.h>
#  include <fcntl.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <unistd.h>
using socket_t = int;
static constexpr socket_t kInvalidSocket = -1;
#endif

namespace lupine {
namespace network {

namespace {

// Datagram framing: magic + version, then a one-byte kind.
constexpr uint8_t kMagic0 = 'L';
constexpr uint8_t kMagic1 = 'U';
constexpr uint8_t kMagic2 = 'P';
constexpr uint8_t kMagic3 = 'N';
constexpr uint8_t kDiscoveryVersion = 1;

enum class DiscoveryKind : uint8_t {
    Probe = 0,     // discoverer -> network: "who is hosting <gameId>?"
    Response = 1   // advertiser -> discoverer: server description
};

// How often the discoverer re-broadcasts a probe, and how long a server entry
// survives without being heard again.
constexpr float kProbeIntervalSeconds = 1.0f;
constexpr float kServerTimeoutSeconds = 5.0f;

void WriteHeader(ByteWriter& writer, DiscoveryKind kind) {
    writer.WriteU8(kMagic0);
    writer.WriteU8(kMagic1);
    writer.WriteU8(kMagic2);
    writer.WriteU8(kMagic3);
    writer.WriteU8(kDiscoveryVersion);
    writer.WriteU8(static_cast<uint8_t>(kind));
}

bool ReadHeader(ByteReader& reader, DiscoveryKind& outKind) {
    const uint8_t m0 = reader.ReadU8();
    const uint8_t m1 = reader.ReadU8();
    const uint8_t m2 = reader.ReadU8();
    const uint8_t m3 = reader.ReadU8();
    const uint8_t version = reader.ReadU8();
    const uint8_t kind = reader.ReadU8();
    if (!reader.Ok() || m0 != kMagic0 || m1 != kMagic1 || m2 != kMagic2 || m3 != kMagic3) {
        return false;
    }
    if (version != kDiscoveryVersion || kind > static_cast<uint8_t>(DiscoveryKind::Response)) {
        return false;
    }
    outKind = static_cast<DiscoveryKind>(kind);
    return true;
}

void EncodeResponse(ByteWriter& writer, const LanServerInfo& info) {
    WriteHeader(writer, DiscoveryKind::Response);
    writer.WriteString(info.gameId);
    writer.WriteString(info.name);
    writer.WriteU16(info.gamePort);
    writer.WriteU32(info.playerCount);
    writer.WriteU32(info.maxPlayers);
    writer.WriteU32(info.protocolVersion);
}

bool SetNonBlocking(socket_t sock) {
#if defined(_WIN32)
    u_long mode = 1;
    return ioctlsocket(sock, FIONBIO, &mode) == 0;
#else
    const int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    return fcntl(sock, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

void CloseSocket(socket_t sock) {
    if (sock == kInvalidSocket) {
        return;
    }
#if defined(_WIN32)
    closesocket(sock);
#else
    ::close(sock);
#endif
}

// Process-wide Winsock init refcount (the engine is single-threaded for sockets).
#if defined(_WIN32)
int g_WinsockRefs = 0;

bool WinsockRetain() {
    if (g_WinsockRefs == 0) {
        WSADATA data;
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
            return false;
        }
    }
    ++g_WinsockRefs;
    return true;
}

void WinsockRelease() {
    if (g_WinsockRefs > 0 && --g_WinsockRefs == 0) {
        WSACleanup();
    }
}
#else
bool WinsockRetain() { return true; }
void WinsockRelease() {}
#endif

}  // namespace

struct LanDiscovery::Impl {
    // Advertiser state.
    socket_t advertiseSocket = kInvalidSocket;
    LanServerInfo advertisement;

    // Discoverer state.
    socket_t discoverSocket = kInvalidSocket;
    uint16_t discoverPort = 0;
    std::string discoverGameId;
    float probeTimer = 0.0f;
    std::vector<LanServerInfo> servers;

    bool winsockHeld = false;

    ~Impl() {
        CloseSocket(advertiseSocket);
        CloseSocket(discoverSocket);
        if (winsockHeld) {
            WinsockRelease();
        }
    }

    bool EnsureWinsock() {
        if (!winsockHeld) {
            winsockHeld = WinsockRetain();
        }
        return winsockHeld;
    }
};

LanDiscovery::LanDiscovery() : m_Impl(std::make_unique<Impl>()) {}

LanDiscovery::~LanDiscovery() = default;

// ============================================================================
// Advertiser
// ============================================================================

bool LanDiscovery::StartAdvertising(uint16_t discoveryPort, const LanServerInfo& info) {
    StopAdvertising();
    if (!m_Impl->EnsureWinsock()) {
        LOG_NETWORK_ERROR("LAN discovery: socket subsystem init failed");
        return false;
    }

    socket_t sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == kInvalidSocket) {
        LOG_NETWORK_ERROR("LAN discovery: could not create advertise socket");
        return false;
    }

    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(discoveryPort);
    if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        LOG_NETWORK_ERROR("LAN discovery: could not bind advertise socket on port {}", discoveryPort);
        CloseSocket(sock);
        return false;
    }
    if (!SetNonBlocking(sock)) {
        CloseSocket(sock);
        return false;
    }

    m_Impl->advertiseSocket = sock;
    m_Impl->advertisement = info;
    LOG_NETWORK_INFO("LAN discovery: advertising '{}' on port {}", info.name, discoveryPort);
    return true;
}

void LanDiscovery::UpdateAdvertisement(const LanServerInfo& info) {
    m_Impl->advertisement = info;
}

void LanDiscovery::StopAdvertising() {
    if (m_Impl->advertiseSocket != kInvalidSocket) {
        CloseSocket(m_Impl->advertiseSocket);
        m_Impl->advertiseSocket = kInvalidSocket;
    }
}

bool LanDiscovery::IsAdvertising() const {
    return m_Impl->advertiseSocket != kInvalidSocket;
}

// ============================================================================
// Discoverer
// ============================================================================

bool LanDiscovery::StartDiscovery(uint16_t discoveryPort, const std::string& gameId) {
    StopDiscovery();
    if (!m_Impl->EnsureWinsock()) {
        LOG_NETWORK_ERROR("LAN discovery: socket subsystem init failed");
        return false;
    }

    socket_t sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == kInvalidSocket) {
        LOG_NETWORK_ERROR("LAN discovery: could not create discover socket");
        return false;
    }

    int broadcast = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&broadcast),
                   sizeof(broadcast)) != 0) {
        LOG_NETWORK_ERROR("LAN discovery: could not enable broadcast");
        CloseSocket(sock);
        return false;
    }

    // Bind to an ephemeral local port so replies can be received.
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(0);
    if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        CloseSocket(sock);
        return false;
    }
    if (!SetNonBlocking(sock)) {
        CloseSocket(sock);
        return false;
    }

    m_Impl->discoverSocket = sock;
    m_Impl->discoverPort = discoveryPort;
    m_Impl->discoverGameId = gameId;
    m_Impl->probeTimer = 0.0f;
    m_Impl->servers.clear();
    SendProbe();
    LOG_NETWORK_INFO("LAN discovery: searching for '{}' on port {}", gameId, discoveryPort);
    return true;
}

void LanDiscovery::SendProbe() {
    if (m_Impl->discoverSocket == kInvalidSocket) {
        return;
    }
    ByteWriter writer;
    WriteHeader(writer, DiscoveryKind::Probe);
    writer.WriteString(m_Impl->discoverGameId);

    sockaddr_in dest;
    std::memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    dest.sin_port = htons(m_Impl->discoverPort);
    sendto(m_Impl->discoverSocket, reinterpret_cast<const char*>(writer.Data().data()),
           static_cast<int>(writer.Size()), 0, reinterpret_cast<sockaddr*>(&dest), sizeof(dest));
}

void LanDiscovery::StopDiscovery() {
    if (m_Impl->discoverSocket != kInvalidSocket) {
        CloseSocket(m_Impl->discoverSocket);
        m_Impl->discoverSocket = kInvalidSocket;
    }
    m_Impl->servers.clear();
}

bool LanDiscovery::IsDiscovering() const {
    return m_Impl->discoverSocket != kInvalidSocket;
}

// ============================================================================
// Per-frame driving
// ============================================================================

void LanDiscovery::Poll(float deltaTime) {
    uint8_t buffer[1024];

    // Advertiser: reply to each inbound probe whose game id matches.
    if (m_Impl->advertiseSocket != kInvalidSocket) {
        for (;;) {
            sockaddr_in from;
#if defined(_WIN32)
            int fromLen = sizeof(from);
#else
            socklen_t fromLen = sizeof(from);
#endif
            const int received = recvfrom(m_Impl->advertiseSocket,
                                          reinterpret_cast<char*>(buffer), sizeof(buffer), 0,
                                          reinterpret_cast<sockaddr*>(&from), &fromLen);
            if (received <= 0) {
                break;
            }
            ByteReader reader(buffer, static_cast<size_t>(received));
            DiscoveryKind kind = DiscoveryKind::Probe;
            if (!ReadHeader(reader, kind) || kind != DiscoveryKind::Probe) {
                continue;
            }
            const std::string wanted = reader.ReadString();
            if (!reader.Ok()) {
                continue;
            }
            if (!wanted.empty() && wanted != m_Impl->advertisement.gameId) {
                continue;  // A probe for a different game.
            }

            ByteWriter response;
            EncodeResponse(response, m_Impl->advertisement);
            sendto(m_Impl->advertiseSocket, reinterpret_cast<const char*>(response.Data().data()),
                   static_cast<int>(response.Size()), 0, reinterpret_cast<sockaddr*>(&from), fromLen);
        }
    }

    // Discoverer: collect responses, age out stale entries, re-probe on cadence.
    if (m_Impl->discoverSocket != kInvalidSocket) {
        for (;;) {
            sockaddr_in from;
#if defined(_WIN32)
            int fromLen = sizeof(from);
#else
            socklen_t fromLen = sizeof(from);
#endif
            const int received = recvfrom(m_Impl->discoverSocket,
                                          reinterpret_cast<char*>(buffer), sizeof(buffer), 0,
                                          reinterpret_cast<sockaddr*>(&from), &fromLen);
            if (received <= 0) {
                break;
            }
            ByteReader reader(buffer, static_cast<size_t>(received));
            DiscoveryKind kind = DiscoveryKind::Probe;
            if (!ReadHeader(reader, kind) || kind != DiscoveryKind::Response) {
                continue;
            }
            LanServerInfo info;
            info.gameId = reader.ReadString();
            info.name = reader.ReadString();
            info.gamePort = reader.ReadU16();
            info.playerCount = reader.ReadU32();
            info.maxPlayers = reader.ReadU32();
            info.protocolVersion = reader.ReadU32();
            if (!reader.Ok()) {
                continue;
            }
            if (!m_Impl->discoverGameId.empty() && info.gameId != m_Impl->discoverGameId) {
                continue;
            }

            char ipText[INET_ADDRSTRLEN] = {0};
            inet_ntop(AF_INET, &from.sin_addr, ipText, sizeof(ipText));
            info.address = ipText;
            info.ageSeconds = 0.0f;

            // Upsert keyed by responding address + gameplay port.
            auto it = std::find_if(m_Impl->servers.begin(), m_Impl->servers.end(),
                                   [&](const LanServerInfo& s) {
                                       return s.address == info.address && s.gamePort == info.gamePort;
                                   });
            if (it != m_Impl->servers.end()) {
                *it = info;
            } else {
                m_Impl->servers.push_back(info);
            }
        }

        // Age entries and drop ones not heard from within the timeout.
        for (LanServerInfo& server : m_Impl->servers) {
            server.ageSeconds += deltaTime;
        }
        m_Impl->servers.erase(
            std::remove_if(m_Impl->servers.begin(), m_Impl->servers.end(),
                           [](const LanServerInfo& s) { return s.ageSeconds > kServerTimeoutSeconds; }),
            m_Impl->servers.end());

        m_Impl->probeTimer += deltaTime;
        if (m_Impl->probeTimer >= kProbeIntervalSeconds) {
            m_Impl->probeTimer = 0.0f;
            SendProbe();
        }
    }
}

std::vector<LanServerInfo> LanDiscovery::GetServers() const {
    std::vector<LanServerInfo> sorted = m_Impl->servers;
    std::sort(sorted.begin(), sorted.end(),
              [](const LanServerInfo& a, const LanServerInfo& b) { return a.ageSeconds < b.ageSeconds; });
    return sorted;
}

} // namespace network
} // namespace lupine
