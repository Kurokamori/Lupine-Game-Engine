#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/network/NetworkTypes.hpp"
#include <string>

namespace lupine {
namespace components {

/**
 * NetworkObject (a.k.a. network identity).
 *
 * Marks a Node as a participant in network replication. It carries the node's
 * network-stable NetworkId and the peer that has authority over it, and
 * registers/unregisters itself with the ReplicationManager as it enters/leaves
 * the scene tree. RPCs and state updates target a node by the NetworkId held
 * here, so any node that wants to send/receive RPCs or be replicated needs one.
 *
 * The runtime fields (network id, owner peer) are assigned at runtime - by the
 * deterministic scene-id walk for scene-placed nodes, or by the spawner for
 * runtime-spawned nodes - and are deliberately NOT serialized into the scene.
 * Only the authoring options below persist.
 */
class NetworkObject : public core::Component {
public:
    NetworkObject();
    explicit NetworkObject(const std::string& name);
    ~NetworkObject() override;

    std::string GetTypeName() const override { return "NetworkObject"; }
    void DefineProperties() override;
    void DefineSignals() override;

    void OnEnterTree() override;
    void OnExitTree() override;

    nlohmann::json CallMethod(const std::string& method, const nlohmann::json& args) override;

    // Network-stable id. 0 until assigned. Setting it (re)registers with the
    // ReplicationManager when the node is in the tree.
    network::NetworkId GetNetworkId() const { return m_NetworkId; }
    void SetNetworkId(network::NetworkId id);

    // The peer that owns / has authority over this object. Defaults to the server.
    network::PeerId GetOwnerPeerId() const { return m_OwnerPeer; }
    void SetOwnerPeerId(network::PeerId peer);

    // True when the local instance is the authority for this object. Offline
    // (single-player) this is always true, so authority-gated game logic runs
    // unchanged when networking is not in use.
    bool IsAuthority() const;
    bool IsMine() const { return IsAuthority(); }

    // Scene path this object was spawned from (empty for scene-placed objects).
    const std::string& GetSpawnScene() const { return m_SpawnScene; }
    void SetSpawnScene(const std::string& scenePath) { m_SpawnScene = scenePath; }

private:
    void RegisterIfPossible();
    void UnregisterIfNeeded();

    network::NetworkId m_NetworkId = network::kInvalidNetworkId;
    network::PeerId m_OwnerPeer = network::kServerPeerId;
    std::string m_SpawnScene;
    bool m_Registered = false;
};

} // namespace components
} // namespace lupine
