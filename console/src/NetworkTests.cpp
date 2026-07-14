#include "lupine/network/NetworkManager.hpp"
#include "lupine/network/ReplicationManager.hpp"
#include "lupine/network/RpcManager.hpp"
#include "lupine/network/NetworkSerializer.hpp"
#include "lupine/components/NetworkObject.hpp"
#include "lupine/components/NetworkSynchronizer.hpp"
#include "lupine/core/Node.hpp"
#include "TestFramework.hpp"

#include <cmath>
#include <iostream>
#include <memory>

using namespace lupine;
using namespace lupine::network;

namespace {

// Drive two manager instances until their handshake converges (loopback delivers
// synchronously, so a handful of poll rounds is plenty).
void Pump(NetworkManager& a, NetworkManager& b, int rounds) {
    for (int i = 0; i < rounds; ++i) {
        a.Poll(0.016f);
        b.Poll(0.016f);
        a.Flush();
        b.Flush();
    }
}

bool TestSerializer() {
    TEST_SECTION("Binary serializer round-trip");

    ByteWriter writer;
    writer.WriteU32(300u);
    writer.WriteI32(-12345);
    writer.WriteU64(0xFFFFFFFFull);
    writer.WriteString("hello");
    writer.WriteFloat(1.5f);
    writer.WriteBool(true);

    nlohmann::json original = {
        {"hp", 100},
        {"name", "player"},
        {"pos", nlohmann::json::array({1.5, -2.0, 3.25})},
        {"flag", false}
    };
    writer.WriteJson(original);

    ByteReader reader(writer.Data());
    TEST_ASSERT(reader.ReadU32() == 300u, "u32 varint round-trips");
    TEST_ASSERT(reader.ReadI32() == -12345, "i32 zig-zag round-trips");
    TEST_ASSERT(reader.ReadU64() == 0xFFFFFFFFull, "u64 varint round-trips");
    TEST_ASSERT(reader.ReadString() == "hello", "string round-trips");
    TEST_ASSERT(reader.ReadFloat() == 1.5f, "float round-trips");
    TEST_ASSERT(reader.ReadBool() == true, "bool round-trips");

    const nlohmann::json decoded = reader.ReadJson();
    TEST_ASSERT(decoded == original, "JSON value codec round-trips losslessly");
    TEST_ASSERT(reader.Ok(), "reader stayed in a valid state");

    // Underrun safety: reading past the end latches failure, never reads OOB.
    ByteReader shortReader(writer.Data().data(), 2);
    shortReader.ReadString();  // demands more bytes than available
    TEST_ASSERT(!shortReader.Ok(), "reader fails safely on underrun");

    return true;
}

bool TestReplicationRegistry() {
    TEST_SECTION("Replication registry + id allocation");

    NetworkManager net;
    net.Initialize();
    ReplicationManager& rep = net.Replication();

    std::shared_ptr<core::Node> node = std::make_shared<core::Node>("Obj");
    rep.RegisterObject(42u, node.get());
    TEST_ASSERT(rep.ResolveNode(42u) == node.get(), "ResolveNode returns the registered node");
    TEST_ASSERT(rep.NetworkIdOf(node.get()) == 42u, "NetworkIdOf returns the registered id");
    TEST_ASSERT(rep.IsRegistered(42u), "IsRegistered is true after register");

    rep.UnregisterObject(42u);
    TEST_ASSERT(rep.ResolveNode(42u) == nullptr, "ResolveNode is null after unregister");

    const NetworkId a = rep.AllocateSpawnedId();
    const NetworkId b = rep.AllocateSpawnedId();
    TEST_ASSERT(a >= kFirstSpawnedNetworkId, "spawned ids are in the high range");
    TEST_ASSERT(b == a + 1u, "spawned ids increment");

    return true;
}

bool TestOfflineDefaults() {
    TEST_SECTION("Offline (no session) defaults");

    NetworkManager net;
    net.Initialize();
    TEST_ASSERT(!net.IsActive(), "fresh manager is inactive");
    TEST_ASSERT(net.GetMode() == SessionMode::Offline, "fresh manager is Offline");
    TEST_ASSERT(net.HasAuthorityOver(kServerPeerId) == false || net.GetLocalPeerId() == kInvalidPeerId,
                "offline authority resolves without a session");
    // Poll/Flush must be safe no-ops offline.
    net.Poll(0.016f);
    net.Flush();
    net.GenerateSnapshot(0.016f);
    TEST_ASSERT(true, "Poll/Flush/GenerateSnapshot are safe no-ops offline");

    return true;
}

bool TestLoopbackSession() {
    TEST_SECTION("Loopback host + client handshake");

    NetworkManager host;
    NetworkManager client;
    host.Initialize();
    client.Initialize();

    NetworkConfig config;
    config.transport = TransportKind::Loopback;
    config.port = 50051;

    TEST_ASSERT(host.StartHost(config), "host starts");
    TEST_ASSERT(host.IsServer(), "host is server");
    TEST_ASSERT(host.GetLocalPeerId() == kServerPeerId, "host local peer id is 1");

    NetworkConfig clientConfig = config;
    TEST_ASSERT(client.Connect(clientConfig), "client connect initiated");

    Pump(host, client, 12);

    TEST_ASSERT(client.GetLocalPeerId() == kServerPeerId + 1, "client was assigned peer id 2");
    TEST_ASSERT(host.GetPeerCount() == 1, "host sees one connected client");
    TEST_ASSERT(client.GetPeerCount() >= 1, "client knows about the server");
    TEST_ASSERT(host.HasPeer(kServerPeerId + 1), "host has peer 2 in its table");

    host.Disconnect();
    client.Disconnect();
    TEST_ASSERT(!host.IsActive() && !client.IsActive(), "both inactive after disconnect");

    return true;
}

bool TestRpcRoundtrip() {
    TEST_SECTION("RPC round-trip over loopback");

    NetworkManager host;
    NetworkManager client;
    host.Initialize();
    client.Initialize();

    NetworkConfig config;
    config.transport = TransportKind::Loopback;
    config.port = 50052;
    host.StartHost(config);
    NetworkConfig clientConfig = config;
    client.Connect(clientConfig);
    Pump(host, client, 12);

    // Mirror a networked node on both sides under the same NetworkId. (Registered
    // directly so the test is independent of scene-id assignment.)
    const NetworkId nid = 7u;
    std::shared_ptr<core::Node> hostNode = std::make_shared<core::Node>("P");
    std::shared_ptr<core::Node> clientNode = std::make_shared<core::Node>("P");
    auto clientNetObj = std::make_shared<components::NetworkObject>();
    clientNode->AddComponent(clientNetObj);

    host.Replication().RegisterObject(nid, hostNode.get());
    client.Replication().RegisterObject(nid, clientNode.get());

    TEST_ASSERT(clientNetObj->GetOwnerPeerId() == kServerPeerId, "client object owner starts as server");

    // Host invokes a component RPC; the client should run it on its mirror.
    host.Rpc().SendComponentRpc(hostNode.get(), "NetworkObject", "set_authority",
                                nlohmann::json::array({static_cast<int64_t>(42)}));
    Pump(host, client, 4);

    TEST_ASSERT(clientNetObj->GetOwnerPeerId() == 42u, "RPC executed on the client mirror");

    host.Disconnect();
    client.Disconnect();
    return true;
}

bool TestSnapshotReplication() {
    TEST_SECTION("State snapshot replication over loopback");

    NetworkManager host;
    NetworkManager client;
    host.Initialize();
    client.Initialize();

    NetworkConfig config;
    config.transport = TransportKind::Loopback;
    config.port = 50053;
    host.StartHost(config);
    NetworkConfig clientConfig = config;
    client.Connect(clientConfig);
    Pump(host, client, 12);

    const NetworkId nid = 9u;

    // Host (authority) node: NetworkObject + a synchronizer replicating one of its
    // properties. The owner defaults to the server, so the host is authority.
    std::shared_ptr<core::Node> hostNode = std::make_shared<core::Node>("S");
    auto hostNetObj = std::make_shared<components::NetworkObject>();
    auto hostSync = std::make_shared<components::NetworkSynchronizer>();
    hostNode->AddComponent(hostNetObj);
    hostNode->AddComponent(hostSync);
    hostSync->SetPropertyValue<std::vector<std::string>>(
        "replicatedProperties", {"NetworkObject:persistAcrossScenes"});

    // Client mirror with the same component layout.
    std::shared_ptr<core::Node> clientNode = std::make_shared<core::Node>("S");
    auto clientNetObj = std::make_shared<components::NetworkObject>();
    auto clientSync = std::make_shared<components::NetworkSynchronizer>();
    clientNode->AddComponent(clientNetObj);
    clientNode->AddComponent(clientSync);
    clientSync->SetPropertyValue<std::vector<std::string>>(
        "replicatedProperties", {"NetworkObject:persistAcrossScenes"});

    host.Replication().RegisterObject(nid, hostNode.get());
    client.Replication().RegisterObject(nid, clientNode.get());

    // Change the replicated property on the authority, then tick a snapshot.
    hostNetObj->SetPropertyValue<bool>("persistAcrossScenes", true);
    host.GenerateSnapshot(0.1f);  // dt > tick interval -> a snapshot is sent
    Pump(host, client, 4);

    const bool replicated = clientNetObj->GetPropertyValue<bool>("persistAcrossScenes");
    TEST_ASSERT(replicated == true, "replicated property reached the client");

    host.Disconnect();
    client.Disconnect();
    return true;
}

bool TestLateJoinerSync() {
    TEST_SECTION("Late-joiner receives current state on connect");

    NetworkManager host;
    NetworkManager client;
    host.Initialize();
    client.Initialize();

    NetworkConfig config;
    config.transport = TransportKind::Loopback;
    config.port = 50054;
    host.StartHost(config);

    const NetworkId nid = 11u;

    // Authority node whose replicated property is changed BEFORE anyone joins.
    std::shared_ptr<core::Node> hostNode = std::make_shared<core::Node>("L");
    auto hostNetObj = std::make_shared<components::NetworkObject>();
    auto hostSync = std::make_shared<components::NetworkSynchronizer>();
    hostNode->AddComponent(hostNetObj);
    hostNode->AddComponent(hostSync);
    hostSync->SetPropertyValue<std::vector<std::string>>(
        "replicatedProperties", {"NetworkObject:persistAcrossScenes"});
    hostNetObj->SetPropertyValue<bool>("persistAcrossScenes", true);
    host.Replication().RegisterObject(nid, hostNode.get());

    // The client mirror exists before it connects (scene-placed equivalent).
    std::shared_ptr<core::Node> clientNode = std::make_shared<core::Node>("L");
    auto clientNetObj = std::make_shared<components::NetworkObject>();
    auto clientSync = std::make_shared<components::NetworkSynchronizer>();
    clientNode->AddComponent(clientNetObj);
    clientNode->AddComponent(clientSync);
    clientSync->SetPropertyValue<std::vector<std::string>>(
        "replicatedProperties", {"NetworkObject:persistAcrossScenes"});
    client.Connect(config);
    client.Replication().RegisterObject(nid, clientNode.get());

    // No GenerateSnapshot is called: the only path that can carry the value is the
    // initial-state keyframe the host sends when the joiner's handshake completes.
    Pump(host, client, 16);

    TEST_ASSERT(clientNetObj->GetPropertyValue<bool>("persistAcrossScenes") == true,
                "joiner received the pre-existing state via the join keyframe");

    host.Disconnect();
    client.Disconnect();
    return true;
}

bool TestLanDiscoveryApi() {
    TEST_SECTION("LAN discovery advertise/browse lifecycle");

    NetworkManager host;
    host.Initialize();

    NetworkConfig config;
    config.transport = TransportKind::Loopback;
    config.port = 50055;
    config.discoveryPort = 50650;  // Uncommon port to avoid clashes.
    config.serverName = "Console Test Server";
    host.StartHost(config);

    const bool advertising = host.StartLanAdvertising();
    TEST_ASSERT(advertising, "advertising starts (UDP discovery socket bound)");
    TEST_ASSERT(host.IsLanAdvertising(), "IsLanAdvertising is true while advertising");
    host.StopLanAdvertising();
    TEST_ASSERT(!host.IsLanAdvertising(), "IsLanAdvertising is false after stop");

    NetworkManager browser;
    browser.Initialize();
    const bool discovering = browser.StartLanDiscovery("lupine-game", config.discoveryPort);
    TEST_ASSERT(discovering, "discovery scan starts (broadcast socket created)");
    TEST_ASSERT(browser.IsLanDiscovering(), "IsLanDiscovering is true while scanning");
    // Pump a few frames; on a machine with loopback broadcast this may even resolve
    // the host, but we only require the lifecycle to be sound here.
    browser.Poll(0.016f);
    browser.StopLanDiscovery();
    TEST_ASSERT(!browser.IsLanDiscovering(), "IsLanDiscovering is false after stop");

    host.Disconnect();
    return true;
}

bool TestCompression() {
    TEST_SECTION("Lossy compression round-trip");

    ByteWriter writer;
    writer.WriteHalf(123.5f);
    writer.WriteQuantizedAngle(1.2345f, 16);
    writer.WriteQuantizedFloat(0.75f, 0.0f, 1.0f, 12);
    // A normalized 90-degree rotation about Y (w = z = 1/sqrt2 here is arbitrary;
    // WriteUnitQuat normalizes defensively).
    const float inv = 1.0f / std::sqrt(2.0f);
    writer.WriteUnitQuat(inv, 0.0f, inv, 0.0f);

    ByteReader reader(writer.Data());
    const float half = reader.ReadHalf();
    TEST_ASSERT(std::fabs(half - 123.5f) < 0.2f, "half float round-trips within tolerance");
    const float angle = reader.ReadQuantizedAngle(16);
    TEST_ASSERT(std::fabs(angle - 1.2345f) < 0.001f, "quantized angle within tolerance");
    const float qf = reader.ReadQuantizedFloat(0.0f, 1.0f, 12);
    TEST_ASSERT(std::fabs(qf - 0.75f) < 0.001f, "quantized float within tolerance");

    float qw = 0.0f, qx = 0.0f, qy = 0.0f, qz = 0.0f;
    reader.ReadUnitQuat(qw, qx, qy, qz);
    TEST_ASSERT(reader.Ok(), "reader valid after compressed reads");
    // Same rotation up to sign: |dot| with the (normalized) input near 1.
    const float dot = std::fabs(qw * inv + qy * inv);
    TEST_ASSERT(dot > 0.99f, "unit quaternion reconstructs the rotation");

    // Half-float is byte-fixed (2 bytes); the quat is fixed 4 bytes.
    ByteWriter sizeProbe;
    sizeProbe.WriteHalf(1.0f);
    TEST_ASSERT(sizeProbe.Size() == 2, "half float is two bytes on the wire");

    return true;
}

bool TestNetworkStats() {
    TEST_SECTION("Diagnostics counters over loopback");

    NetworkManager host;
    NetworkManager client;
    host.Initialize();
    client.Initialize();

    NetworkConfig config;
    config.transport = TransportKind::Loopback;
    config.port = 50056;
    host.StartHost(config);
    NetworkConfig clientConfig = config;
    client.Connect(clientConfig);
    Pump(host, client, 12);

    // Drive >1s so the per-second stats window finalizes at least once.
    for (int i = 0; i < 140; ++i) {
        host.Poll(0.016f);
        client.Poll(0.016f);
        host.Flush();
        client.Flush();
    }

    const NetworkStats hostStats = host.GetStats();
    TEST_ASSERT(hostStats.peerCount == 1, "host stats report one peer");
    TEST_ASSERT(hostStats.totalBytesOut > 0 || hostStats.totalBytesIn > 0,
                "host stats counted handshake/ping bytes");

    PeerInfo info;
    TEST_ASSERT(host.GetPeerInfo(kServerPeerId + 1, info), "host can query a peer's PeerInfo");

    host.Disconnect();
    client.Disconnect();
    return true;
}

} // namespace

void RunNetworkTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "NETWORK TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    lupine_test::SetCurrentSuite("Networking");
    bool allPassed = true;

    allPassed &= TestSerializer();
    allPassed &= TestCompression();
    allPassed &= TestReplicationRegistry();
    allPassed &= TestOfflineDefaults();
    allPassed &= TestLoopbackSession();
    allPassed &= TestRpcRoundtrip();
    allPassed &= TestSnapshotReplication();
    allPassed &= TestLateJoinerSync();
    allPassed &= TestLanDiscoveryApi();
    allPassed &= TestNetworkStats();

    std::cout << "\n========================================" << std::endl;
    if (allPassed) {
        std::cout << "ALL NETWORK TESTS PASSED!" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================\n" << std::endl;
}
