#include "lupine/network/ReplicationManager.hpp"
#include "lupine/network/NetworkManager.hpp"
#include "lupine/network/INetworkSerializable.hpp"
#include "lupine/components/NetworkObject.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/SceneInstance.hpp"
#include "lupine/asset/Assets.hpp"
#include "lupine/logger/Logger.hpp"

#include <chrono>
#include <memory>
#include <vector>

namespace lupine {
namespace network {

namespace {
// Receiver ack cadence and the sequence gap that triggers a recovery keyframe.
constexpr float kAckIntervalMs = 500.0f;
constexpr uint32_t kLossRecoveryGap = 10;
constexpr double kRecoveryThrottleMs = 500.0;

double NowMs() {
    using namespace std::chrono;
    return static_cast<double>(
        duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count()) / 1000.0;
}
}  // namespace

ReplicationManager::ReplicationManager(NetworkManager& owner) : m_Net(owner) {}

void ReplicationManager::Initialize() {
    m_Net.RegisterMessageHandler(MsgType::Spawn,
        [this](PeerId from, ByteReader& reader) { OnSpawnMessage(from, reader); });
    m_Net.RegisterMessageHandler(MsgType::Despawn,
        [this](PeerId from, ByteReader& reader) { OnDespawnMessage(from, reader); });
    m_Net.RegisterMessageHandler(MsgType::Snapshot,
        [this](PeerId from, ByteReader& reader) { OnSnapshotMessage(from, reader); });
    m_Net.RegisterMessageHandler(MsgType::InterestUpdate,
        [this](PeerId from, ByteReader& reader) { OnInterestUpdateMessage(from, reader); });
    m_Net.RegisterMessageHandler(MsgType::SnapshotAck,
        [this](PeerId from, ByteReader& reader) { OnSnapshotAckMessage(from, reader); });
    m_NextSpawnedId = kFirstSpawnedNetworkId;
    m_NextSceneId = 1;
}

void ReplicationManager::OnSessionEnded() {
    // Reset every registered object's id so the next session can reassign
    // scene ids deterministically, then clear the registry and allocators.
    std::vector<core::Node*> nodes;
    nodes.reserve(m_ById.size());
    for (const auto& entry : m_ById) {
        nodes.push_back(entry.second);
    }
    m_ById.clear();
    m_IdByNode.clear();
    m_SpawnRecords.clear();
    m_PeerFocus.clear();

    for (core::Node* node : nodes) {
        if (node == nullptr) {
            continue;
        }
        std::shared_ptr<core::Component> comp = node->GetComponent("NetworkObject");
        if (auto* netObject = dynamic_cast<components::NetworkObject*>(comp.get())) {
            netObject->SetNetworkId(kInvalidNetworkId);
        }
    }

    m_NextSpawnedId = kFirstSpawnedNetworkId;
    m_NextSceneId = 1;
    m_SnapshotClockMs = 0.0f;
    m_SendAccum = 0.0f;
    m_KeyframeAccumMs = 0.0f;
    m_SnapshotSeq = 0;
    m_LastReceivedSeq = 0;
    m_AckAccumMs = 0.0f;
    m_PeerLastRecoverMs.clear();
}

void ReplicationManager::RegisterObject(NetworkId id, core::Node* node) {
    if (id == kInvalidNetworkId || node == nullptr) {
        return;
    }
    m_ById[id] = node;
    m_IdByNode[node] = id;
}

void ReplicationManager::UnregisterObject(NetworkId id) {
    auto it = m_ById.find(id);
    if (it == m_ById.end()) {
        return;
    }
    m_IdByNode.erase(it->second);
    m_ById.erase(it);
}

core::Node* ReplicationManager::ResolveNode(NetworkId id) const {
    auto it = m_ById.find(id);
    return (it != m_ById.end()) ? it->second : nullptr;
}

NetworkId ReplicationManager::NetworkIdOf(const core::Node* node) const {
    auto it = m_IdByNode.find(node);
    return (it != m_IdByNode.end()) ? it->second : kInvalidNetworkId;
}

bool ReplicationManager::IsRegistered(NetworkId id) const {
    return m_ById.count(id) != 0;
}

NetworkId ReplicationManager::AllocateSpawnedId() {
    return m_NextSpawnedId++;
}

void ReplicationManager::AssignSceneNetworkIds(core::Node* root) {
    if (root == nullptr) {
        return;
    }
    // Restart scene-id numbering so the same scene always yields the same ids on
    // every peer, independent of how many scenes were loaded before it. Spawned
    // objects use the separate high id range, so they never collide.
    m_NextSceneId = 1;
    AssignSceneNetworkIdsRecursive(root);
}

void ReplicationManager::AssignSceneNetworkIdsRecursive(core::Node* node) {
    if (node == nullptr) {
        return;
    }
    std::shared_ptr<core::Component> comp = node->GetComponent("NetworkObject");
    if (auto* netObject = dynamic_cast<components::NetworkObject*>(comp.get())) {
        if (netObject->GetNetworkId() == kInvalidNetworkId) {
            netObject->SetNetworkId(m_NextSceneId++);
        }
    }
    for (const std::shared_ptr<core::Node>& child : node->GetChildren()) {
        AssignSceneNetworkIdsRecursive(child.get());
    }
}

// ============================================================================
// Replicated-component enumeration + scene instantiation
// ============================================================================

std::vector<INetworkSerializable*> ReplicationManager::GatherSyncs(core::Node* node) const {
    std::vector<INetworkSerializable*> syncs;
    if (node == nullptr) {
        return syncs;
    }
    for (const std::shared_ptr<core::Component>& comp : node->GetComponents()) {
        if (auto* sync = dynamic_cast<INetworkSerializable*>(comp.get())) {
            syncs.push_back(sync);
        }
    }
    return syncs;
}

core::Node* ReplicationManager::InstantiateNetworkedScene(core::Node* parent,
                                                          const std::string& scenePath,
                                                          NetworkId id, PeerId ownerPeer) {
    if (parent == nullptr) {
        return nullptr;
    }
    std::string resolved = asset::Asset::ResolveAssetPath(scenePath);
    if (resolved.empty()) {
        resolved = scenePath;
    }

    auto instance = std::make_shared<core::SceneInstance>();
    instance->RegisterProperties();
    parent->AddChild(instance);
    if (!instance->SetSceneReference(resolved)) {
        LOG_NETWORK_ERROR("Spawn: failed to instantiate scene '{}'", scenePath);
        parent->RemoveChild(instance);
        return nullptr;
    }

    auto netObject = std::make_shared<components::NetworkObject>();
    netObject->SetOwnerPeerId(ownerPeer);
    netObject->SetSpawnScene(scenePath);
    instance->AddComponent(netObject);
    netObject->SetNetworkId(id);  // Registers with this manager.
    return instance.get();
}

// ============================================================================
// Spawn / despawn
// ============================================================================

NetworkId ReplicationManager::SpawnScene(core::Node* spawner, int spawnableIndex,
                                         const std::string& scenePath, PeerId ownerPeer) {
    if (spawner == nullptr) {
        return kInvalidNetworkId;
    }
    if (m_Net.IsActive() && !m_Net.IsServer() && m_Net.GetMode() != SessionMode::P2P) {
        LOG_NETWORK_WARN("Spawn requested on a client; only the authority may spawn");
        return kInvalidNetworkId;
    }

    const NetworkId newId = AllocateSpawnedId();
    core::Node* spawned = InstantiateNetworkedScene(spawner, scenePath, newId, ownerPeer);
    if (spawned == nullptr) {
        return kInvalidNetworkId;
    }

    // Record what is needed to replay this spawn to a peer that joins later.
    SpawnRecord record;
    record.spawnerId = NetworkIdOf(spawner);
    record.spawnableIndex = static_cast<uint32_t>(spawnableIndex);
    record.ownerPeer = ownerPeer;
    m_SpawnRecords[newId] = record;

    if (m_Net.IsActive()) {
        m_Net.BroadcastMessage(Channel::ReliableOrdered, MsgType::Spawn, EncodeSpawn(newId));
    }
    return newId;
}

std::vector<uint8_t> ReplicationManager::EncodeSpawn(NetworkId id) const {
    ByteWriter writer;
    auto it = m_SpawnRecords.find(id);
    const SpawnRecord record = (it != m_SpawnRecords.end()) ? it->second : SpawnRecord{};
    writer.WriteU32(record.spawnerId);
    writer.WriteU32(record.spawnableIndex);
    writer.WriteU32(id);
    writer.WriteU32(record.ownerPeer);
    return writer.Data();
}

void ReplicationManager::DespawnObject(NetworkId id) {
    core::Node* node = ResolveNode(id);
    if (node == nullptr) {
        return;
    }
    if (m_Net.IsActive()) {
        ByteWriter writer;
        writer.WriteU32(id);
        m_Net.BroadcastMessage(Channel::ReliableOrdered, MsgType::Despawn, writer.Data());
    }
    m_SpawnRecords.erase(id);
    UnregisterObject(id);
    if (core::Node* parent = node->GetParent()) {
        parent->RemoveChild(node->shared_from_this());
    }
}

void ReplicationManager::OnSpawnMessage(PeerId from, ByteReader& reader) {
    (void)from;
    const NetworkId spawnerId = reader.ReadU32();
    const uint32_t spawnableIndex = reader.ReadU32();
    const NetworkId newId = reader.ReadU32();
    const PeerId ownerPeer = reader.ReadU32();
    if (!reader.Ok()) {
        return;
    }

    if (IsRegistered(newId)) {
        return;  // Already have it (e.g. local authority that just spawned).
    }
    core::Node* spawner = ResolveNode(spawnerId);
    if (spawner == nullptr) {
        LOG_NETWORK_DEBUG("Spawn for unknown spawner NetworkId {}", spawnerId);
        return;
    }
    auto* spawnerComp = dynamic_cast<components::NetworkObject*>(
        spawner->GetComponent("NetworkObject").get());
    (void)spawnerComp;

    std::shared_ptr<core::Component> spawnerNetSpawner = spawner->GetComponent("NetworkSpawner");
    std::string scenePath;
    if (spawnerNetSpawner) {
        nlohmann::json result = spawnerNetSpawner->CallMethod(
            "__resolve_spawnable", nlohmann::json::array({static_cast<int>(spawnableIndex)}));
        if (result.is_string()) {
            scenePath = result.get<std::string>();
        }
    }
    if (scenePath.empty()) {
        LOG_NETWORK_WARN("Spawn: spawner has no spawnable at index {}", spawnableIndex);
        return;
    }
    InstantiateNetworkedScene(spawner, scenePath, newId, ownerPeer);
}

void ReplicationManager::OnDespawnMessage(PeerId from, ByteReader& reader) {
    (void)from;
    const NetworkId id = reader.ReadU32();
    if (!reader.Ok()) {
        return;
    }
    core::Node* node = ResolveNode(id);
    if (node == nullptr) {
        return;
    }
    m_SpawnRecords.erase(id);
    UnregisterObject(id);
    if (core::Node* parent = node->GetParent()) {
        parent->RemoveChild(node->shared_from_this());
    }
}

// ============================================================================
// State snapshots
// ============================================================================

uint32_t ReplicationManager::BuildStateSnapshot(bool force, uint32_t seq, ByteWriter& out,
                                                PeerId forPeer) {
    ByteWriter objects;
    uint32_t objectCount = 0;
    for (const auto& entry : m_ById) {
        core::Node* node = entry.second;
        if (node == nullptr) {
            continue;
        }
        auto* netObject = dynamic_cast<components::NetworkObject*>(
            node->GetComponent("NetworkObject").get());
        const PeerId owner = netObject ? netObject->GetOwnerPeerId() : kServerPeerId;
        // Only the peer that actually simulates an object gathers its state. A
        // server is the relay/validation authority over everything, but it does
        // NOT simulate client-owned objects (those arrive via the client and are
        // relayed), so gathering them here would broadcast stale server-side
        // copies - harmless for deltas (change-gated) but actively wrong for a
        // forced keyframe. Gathering strictly by ownership avoids that.
        if (owner != m_Net.GetLocalPeerId()) {
            continue;
        }
        // Area-of-interest: skip objects not relevant to the target peer.
        if (forPeer != kInvalidPeerId && !IsRelevantTo(node, forPeer)) {
            continue;
        }

        std::vector<INetworkSerializable*> syncs = GatherSyncs(node);
        ByteWriter syncBuf;
        uint32_t syncCount = 0;
        for (std::size_t i = 0; i < syncs.size(); ++i) {
            ByteWriter blob;
            if (syncs[i]->NetGather(blob, force)) {
                syncBuf.WriteU32(static_cast<uint32_t>(i));
                syncBuf.WriteBytes(blob.Data().data(), blob.Size());
                ++syncCount;
            }
        }
        if (syncCount > 0) {
            objects.WriteU32(entry.first);
            objects.WriteU32(syncCount);
            objects.WriteRaw(syncBuf.Data().data(), syncBuf.Size());
            ++objectCount;
        }
    }

    out.WriteU32(seq);
    out.WriteU32(static_cast<uint32_t>(m_SnapshotClockMs));
    out.WriteU32(objectCount);
    out.WriteRaw(objects.Data().data(), objects.Size());
    return objectCount;
}

void ReplicationManager::GenerateSnapshot(float fixedDeltaTime) {
    if (!m_Net.IsActive()) {
        return;
    }
    m_SnapshotClockMs += fixedDeltaTime * 1000.0f;

    // Receiver side: periodically ack the latest snapshot sequence so the server
    // can detect a run of losses and push a recovery keyframe sooner than the
    // periodic-keyframe cadence would.
    if (m_Net.GetMode() == SessionMode::Client) {
        m_AckAccumMs += fixedDeltaTime * 1000.0f;
        if (m_AckAccumMs >= kAckIntervalMs && m_LastReceivedSeq != 0) {
            m_AckAccumMs = 0.0f;
            ByteWriter ack;
            ack.WriteU32(m_LastReceivedSeq);
            m_Net.SendTo(kServerPeerId, Channel::UnreliableSequenced, MsgType::SnapshotAck,
                         ack.Data());
        }
    }

    const NetworkConfig& config = m_Net.GetConfig();
    const float tickRate = config.tickRate > 0.0f ? config.tickRate : 30.0f;
    const float interval = 1.0f / tickRate;
    m_KeyframeAccumMs += fixedDeltaTime * 1000.0f;

    m_SendAccum += fixedDeltaTime;
    if (m_SendAccum < interval) {
        return;
    }
    m_SendAccum = 0.0f;

    const bool authorityBroadcasts = m_Net.IsServer() || m_Net.GetMode() == SessionMode::P2P;
    const bool keyframeDue =
        config.keyframeIntervalMs > 0.0f && m_KeyframeAccumMs >= config.keyframeIntervalMs;
    if (keyframeDue) {
        m_KeyframeAccumMs = 0.0f;
    }

    // Area-of-interest: a server with a positive interestRadius sends each client
    // a tailored full-state snapshot of only the objects relevant to that client,
    // instead of broadcasting every object to everyone. Full state (force) is used
    // per peer because per-peer change deltas would need per-peer baselines; the
    // cull keeps each message small, and full state every tick is self-healing
    // against packet loss (so no separate keyframe is needed under AOI).
    const uint32_t seq = ++m_SnapshotSeq;

    if (m_Net.IsServer() && config.interestRadius > 0.0f) {
        for (const PeerInfo& peer : m_Net.GetPeers()) {
            ByteWriter snapshot;
            if (BuildStateSnapshot(/*force*/ true, seq, snapshot, peer.id) > 0) {
                m_Net.SendTo(peer.id, Channel::UnreliableSequenced, MsgType::Snapshot,
                             snapshot.Data());
                m_Net.NotifySnapshotSent();
            }
        }
        return;
    }

    // Per-tick delta over the unreliable channel (low latency, may be dropped).
    ByteWriter delta;
    if (BuildStateSnapshot(/*force*/ false, seq, delta) > 0) {
        if (authorityBroadcasts) {
            m_Net.BroadcastMessage(Channel::UnreliableSequenced, MsgType::Snapshot, delta.Data());
        } else {
            // Client authority over its own objects: send to the server, which relays.
            m_Net.SendTo(kServerPeerId, Channel::UnreliableSequenced, MsgType::Snapshot,
                         delta.Data());
        }
        m_Net.NotifySnapshotSent();
    }

    // Periodic full-state keyframe over the reliable channel. This re-establishes
    // any state stranded by a dropped unreliable delta, so a value that changed
    // once and then stopped can never stay stale on a receiver.
    if (keyframeDue) {
        ByteWriter keyframe;
        if (BuildStateSnapshot(/*force*/ true, ++m_SnapshotSeq, keyframe) > 0) {
            if (authorityBroadcasts) {
                m_Net.BroadcastMessage(Channel::ReliableOrdered, MsgType::Snapshot,
                                       keyframe.Data());
            } else {
                m_Net.SendTo(kServerPeerId, Channel::ReliableOrdered, MsgType::Snapshot,
                             keyframe.Data());
            }
            m_Net.NotifySnapshotSent();
        }
    }
}

void ReplicationManager::SendInitialStateTo(PeerId peer) {
    if (!m_Net.IsActive() || peer == kInvalidPeerId) {
        return;
    }

    // Replay every runtime spawn so the joiner instantiates the same objects the
    // others already have (scene-placed objects exist on the joiner already via
    // the deterministic scene-id walk, so only spawned objects need replaying).
    for (const auto& entry : m_SpawnRecords) {
        m_Net.SendTo(peer, Channel::ReliableOrdered, MsgType::Spawn, EncodeSpawn(entry.first));
    }

    // Then a reliable full-state keyframe so the joiner's transforms / properties
    // match the authority immediately rather than only after the next change. The
    // full world is sent (no AOI cull) because the joiner has not yet reported an
    // interest position; subsequent ticks cull per its area of interest.
    ByteWriter keyframe;
    if (BuildStateSnapshot(/*force*/ true, ++m_SnapshotSeq, keyframe) > 0) {
        m_Net.SendTo(peer, Channel::ReliableOrdered, MsgType::Snapshot, keyframe.Data());
        m_Net.NotifySnapshotSent();
    }
}

// ============================================================================
// Area-of-interest
// ============================================================================

void ReplicationManager::SetLocalInterest(bool is3D, const math::Vec3& position) {
    if (!m_Net.IsActive()) {
        return;
    }
    if (m_Net.IsServer()) {
        // The host's local player: store directly under the local peer id.
        Focus focus;
        focus.valid = true;
        focus.is3D = is3D;
        focus.position = position;
        m_PeerFocus[m_Net.GetLocalPeerId()] = focus;
        return;
    }
    if (m_Net.GetMode() == SessionMode::Client) {
        ByteWriter writer;
        writer.WriteU8(is3D ? 1u : 0u);
        writer.WriteFloat(position.x);
        writer.WriteFloat(position.y);
        writer.WriteFloat(position.z);
        m_Net.SendTo(kServerPeerId, Channel::UnreliableSequenced, MsgType::InterestUpdate,
                     writer.Data());
    }
}

void ReplicationManager::OnSnapshotAckMessage(PeerId from, ByteReader& reader) {
    const uint32_t ackedSeq = reader.ReadU32();
    if (!reader.Ok() || !m_Net.IsServer()) {
        return;
    }
    // A large gap between what we have sent and what this peer has received means a
    // run of unreliable snapshots was dropped; push a recovery keyframe (throttled
    // so a persistently lossy peer is not flooded).
    if (m_SnapshotSeq > ackedSeq + kLossRecoveryGap) {
        const double now = NowMs();
        auto it = m_PeerLastRecoverMs.find(from);
        if (it == m_PeerLastRecoverMs.end() || (now - it->second) > kRecoveryThrottleMs) {
            m_PeerLastRecoverMs[from] = now;
            SendInitialStateTo(from);
        }
    }
}

void ReplicationManager::OnInterestUpdateMessage(PeerId from, ByteReader& reader) {
    Focus focus;
    focus.is3D = reader.ReadU8() != 0u;
    focus.position.x = reader.ReadFloat();
    focus.position.y = reader.ReadFloat();
    focus.position.z = reader.ReadFloat();
    if (!reader.Ok()) {
        return;
    }
    focus.valid = true;
    m_PeerFocus[from] = focus;
}

bool ReplicationManager::GetNodeInterestPos(core::Node* node, bool& outIs3D,
                                            math::Vec3& outPos) const {
    if (auto* n3 = dynamic_cast<core::Node3D*>(node)) {
        outIs3D = true;
        outPos = n3->GetPosition();
        return true;
    }
    if (auto* n2 = dynamic_cast<core::Node2D*>(node)) {
        const math::Vec2 p = n2->GetPosition();
        outIs3D = false;
        outPos = math::Vec3{p.x, p.y, 0.0f};
        return true;
    }
    return false;
}

bool ReplicationManager::IsRelevantTo(core::Node* node, PeerId peer) const {
    auto* netObject = dynamic_cast<components::NetworkObject*>(
        node->GetComponent("NetworkObject").get());
    if (netObject != nullptr && netObject->GetPropertyValue<bool>("alwaysRelevant")) {
        return true;
    }

    auto fit = m_PeerFocus.find(peer);
    if (fit == m_PeerFocus.end() || !fit->second.valid) {
        return true;  // The peer has not reported an interest position: send all.
    }

    bool is3D = false;
    math::Vec3 pos{0.0f, 0.0f, 0.0f};
    if (!GetNodeInterestPos(node, is3D, pos)) {
        return true;  // Non-spatial objects are always relevant.
    }
    const Focus& focus = fit->second;
    if (focus.is3D != is3D) {
        return true;  // Different spatial dimension: do not cull across it.
    }

    const float dx = pos.x - focus.position.x;
    const float dy = pos.y - focus.position.y;
    const float dz = is3D ? (pos.z - focus.position.z) : 0.0f;
    const float radius = m_Net.GetConfig().interestRadius;
    return (dx * dx + dy * dy + dz * dz) <= (radius * radius);
}

void ReplicationManager::OnSnapshotMessage(PeerId from, ByteReader& reader) {
    const uint32_t seq = reader.ReadU32();
    const uint32_t serverTimeMs = reader.ReadU32();
    const uint32_t objectCount = reader.ReadU32();
    if (!reader.Ok() || objectCount > 0xFFFFu) {
        return;
    }
    m_Net.NotifySnapshotReceived(from, seq);
    if (seq > m_LastReceivedSeq) {
        m_LastReceivedSeq = seq;
    }

    for (uint32_t o = 0; o < objectCount && reader.Ok(); ++o) {
        const NetworkId id = reader.ReadU32();
        const uint32_t syncCount = reader.ReadU32();
        if (!reader.Ok() || syncCount > 0xFFFFu) {
            return;
        }
        core::Node* node = ResolveNode(id);
        std::vector<INetworkSerializable*> syncs = node ? GatherSyncs(node)
                                                        : std::vector<INetworkSerializable*>();
        for (uint32_t s = 0; s < syncCount && reader.Ok(); ++s) {
            const uint32_t index = reader.ReadU32();
            const uint64_t len = reader.ReadU64();
            if (!reader.Ok() || len > static_cast<uint64_t>(reader.Remaining())) {
                return;
            }
            std::vector<uint8_t> blob = reader.ReadBytes(static_cast<size_t>(len));
            if (!reader.Ok()) {
                return;
            }
            if (index < syncs.size()) {
                ByteReader blobReader(blob);
                syncs[index]->NetApply(blobReader, serverTimeMs);
            }
        }
    }
    if (!reader.Ok()) {
        return;
    }

    // The server relays a client-authored snapshot to the other peers verbatim.
    if (m_Net.IsServer()) {
        const std::vector<uint8_t>& payload = m_Net.GetCurrentPayload();
        if (!payload.empty()) {
            m_Net.BroadcastMessage(Channel::UnreliableSequenced, MsgType::Snapshot, payload, from);
        }
    }
}

} // namespace network
} // namespace lupine
