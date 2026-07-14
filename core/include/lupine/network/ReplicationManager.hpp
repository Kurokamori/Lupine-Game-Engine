#pragma once

#include "lupine/network/NetworkTypes.hpp"
#include "lupine/network/NetworkSerializer.hpp"
#include "lupine/math/Vec3.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace lupine {
namespace core {
class Node;
}

namespace network {

class NetworkManager;
class INetworkSerializable;

/**
 * Owns the mapping between a network-stable NetworkId and the live Node it names,
 * and allocates NetworkIds. This registry is what lets an RPC or a state update
 * that arrives as a bare 32-bit id be routed to the correct node on every peer.
 *
 * NetworkId allocation uses two non-overlapping ranges so the two assignment
 * schemes never collide:
 *  - Scene-placed nodes get low ids by a deterministic depth-first walk of the
 *    loaded scene (AssignSceneNetworkIds). Because every peer loads the identical
 *    scene file, each reproduces the same id for the same node with no sync.
 *  - Runtime-spawned nodes get high ids (>= kFirstSpawnedNetworkId) from an
 *    authority-only counter, carried to clients in the spawn message.
 *
 * In the first networking phase this provides the registry + allocation; spawn /
 * despawn replication and state snapshots are layered on in a later phase.
 */
class ReplicationManager {
public:
    explicit ReplicationManager(NetworkManager& owner);

    // Register the message handlers this manager owns and clear per-session state.
    void Initialize();

    // Reset all per-session state (called when a session ends). The registry of
    // live objects is left intact - it tracks node lifetimes, not session state -
    // but id allocation restarts so a fresh session reassigns deterministically.
    void OnSessionEnded();

    // ------------------------------------------------------------------
    // NetworkId <-> Node registry
    // ------------------------------------------------------------------
    void RegisterObject(NetworkId id, core::Node* node);
    void UnregisterObject(NetworkId id);
    core::Node* ResolveNode(NetworkId id) const;
    NetworkId NetworkIdOf(const core::Node* node) const;
    bool IsRegistered(NetworkId id) const;
    size_t GetObjectCount() const { return m_ById.size(); }

    // ------------------------------------------------------------------
    // NetworkId allocation
    // ------------------------------------------------------------------

    // Allocate the next runtime-spawn id (authority only).
    NetworkId AllocateSpawnedId();

    // Walk `root` depth-first and assign a deterministic scene-placed NetworkId to
    // every node carrying a NetworkObject that does not already have one, then
    // register it. Must be run with an identical scene tree on every peer.
    void AssignSceneNetworkIds(core::Node* root);

    // ------------------------------------------------------------------
    // Spawn / despawn replication
    // ------------------------------------------------------------------

    // Authority: instantiate `scenePath` as a networked child of `spawner`,
    // assign it a spawned NetworkId, register it, and broadcast a Spawn message so
    // every peer creates the same object. spawnableIndex identifies the entry in
    // the spawner's whitelist (echoed to clients). Returns the new NetworkId, or 0.
    NetworkId SpawnScene(core::Node* spawner, int spawnableIndex,
                         const std::string& scenePath, PeerId ownerPeer);

    // Authority: remove a spawned object and broadcast a Despawn message.
    void DespawnObject(NetworkId id);

    // ------------------------------------------------------------------
    // Per-tick state replication
    // ------------------------------------------------------------------

    // Per-tick delta snapshot (unreliable) plus a periodic full-state keyframe
    // (reliable). The keyframe guarantees a receiver that dropped an unreliable
    // delta is resynchronised within one keyframe interval.
    void GenerateSnapshot(float fixedDeltaTime);

    // Bring a freshly joined peer fully up to date: replay the spawn of every
    // runtime-spawned object, then send a reliable full-state keyframe of all
    // authority-owned objects to that peer only. Called by NetworkManager once a
    // joiner's handshake completes (authority side).
    void SendInitialStateTo(PeerId peer);

    // ------------------------------------------------------------------
    // Area-of-interest
    // ------------------------------------------------------------------

    // Set the local instance's interest position. On a client this is sent to the
    // server so it can cull replication to what is near the client; on a host it
    // sets the host player's own interest. `is3D` selects 2D vs 3D distance.
    void SetLocalInterest(bool is3D, const math::Vec3& position);

private:
    void AssignSceneNetworkIdsRecursive(core::Node* node);

    void OnSpawnMessage(PeerId from, ByteReader& reader);
    void OnDespawnMessage(PeerId from, ByteReader& reader);
    void OnSnapshotMessage(PeerId from, ByteReader& reader);
    void OnInterestUpdateMessage(PeerId from, ByteReader& reader);
    void OnSnapshotAckMessage(PeerId from, ByteReader& reader);

    // Build a state snapshot frame ([seq][tickMs][objCount][objects...]) into
    // `out`. `seq` is the sender's monotonic snapshot sequence (lets receivers
    // estimate packet loss). When `force` is true every owned object writes its
    // full state (keyframe / initial sync); otherwise only objects with changed
    // components are included (delta). When `forPeer` is a real peer id, objects
    // not relevant to that peer's area of interest are skipped. Returns the number
    // of objects written.
    uint32_t BuildStateSnapshot(bool force, uint32_t seq, ByteWriter& out,
                                PeerId forPeer = kInvalidPeerId);

    // True if `node` should be replicated to `peer` under area-of-interest rules.
    bool IsRelevantTo(core::Node* node, PeerId peer) const;

    // Extract a node's interest position. Returns false for non-spatial nodes
    // (which are treated as always relevant).
    bool GetNodeInterestPos(core::Node* node, bool& outIs3D, math::Vec3& outPos) const;

    // Encode a Spawn message for an already-spawned object from its record.
    std::vector<uint8_t> EncodeSpawn(NetworkId id) const;

    // Enumerate the replicated (INetworkSerializable) components of a node in the
    // stable component-list order shared by all peers.
    std::vector<INetworkSerializable*> GatherSyncs(core::Node* node) const;

    // Create a SceneInstance child of `parent` from `scenePath`, attach a
    // NetworkObject with `id`/`ownerPeer`, and register it. Used on both the
    // authority (after id allocation) and clients (id from the Spawn message).
    core::Node* InstantiateNetworkedScene(core::Node* parent, const std::string& scenePath,
                                          NetworkId id, PeerId ownerPeer);

    // What the authority needs to replay a runtime spawn to a late joiner.
    struct SpawnRecord {
        NetworkId spawnerId = kInvalidNetworkId;
        uint32_t spawnableIndex = 0;
        PeerId ownerPeer = kServerPeerId;
    };

    // A peer's reported area-of-interest centre.
    struct Focus {
        bool valid = false;
        bool is3D = false;
        math::Vec3 position{0.0f, 0.0f, 0.0f};
    };

    NetworkManager& m_Net;
    std::unordered_map<NetworkId, core::Node*> m_ById;
    std::unordered_map<const core::Node*, NetworkId> m_IdByNode;
    std::unordered_map<NetworkId, SpawnRecord> m_SpawnRecords;
    std::unordered_map<PeerId, Focus> m_PeerFocus;
    NetworkId m_NextSpawnedId = kFirstSpawnedNetworkId;
    NetworkId m_NextSceneId = 1;

    float m_SnapshotClockMs = 0.0f;  // Authority's monotonic snapshot timestamp.
    float m_SendAccum = 0.0f;        // Accumulator for tick-rate limiting.
    float m_KeyframeAccumMs = 0.0f;  // Accumulator for the reliable keyframe cadence.
    uint32_t m_SnapshotSeq = 0;      // Monotonic per-tick snapshot sequence number.

    // Loss-recovery acking. A receiver tracks the highest snapshot sequence it has
    // seen and periodically acks it; the authority, on seeing a large gap behind
    // its own sequence, pushes that peer a recovery keyframe.
    uint32_t m_LastReceivedSeq = 0;  // Highest snapshot seq received (receiver side).
    float m_AckAccumMs = 0.0f;       // Receiver ack-send cadence accumulator.
    std::unordered_map<PeerId, double> m_PeerLastRecoverMs;  // Per-peer recovery throttle.
};

} // namespace network
} // namespace lupine
