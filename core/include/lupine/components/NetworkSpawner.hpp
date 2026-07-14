#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/network/NetworkTypes.hpp"
#include <string>
#include <vector>

namespace lupine {
namespace components {

/**
 * NetworkSpawner - replicates the creation/destruction of dynamic objects.
 *
 * Holds a whitelist of spawnable scene paths. On the authority, scripts call
 * spawn(scenePath[, ownerPeer]) to instantiate one as a networked child of this
 * spawner's node; the ReplicationManager assigns a NetworkId and broadcasts a
 * Spawn message so every peer creates the identical object. despawn(networkId)
 * mirrors the teardown. The whitelist is what lets clients safely reconstruct a
 * spawn from a compact index rather than trusting an arbitrary path.
 *
 * The spawner's own node must carry a NetworkObject so it has a stable NetworkId
 * that spawn messages can reference.
 */
class NetworkSpawner : public core::Component {
public:
    NetworkSpawner();
    explicit NetworkSpawner(const std::string& name);

    std::string GetTypeName() const override { return "NetworkSpawner"; }
    void DefineProperties() override;

    void OnReady() override;

    nlohmann::json CallMethod(const std::string& method, const nlohmann::json& args) override;

    // Authority: instantiate `scenePath` (which must be in the whitelist) as a
    // networked child. Returns the new NetworkId, or 0 on failure.
    network::NetworkId Spawn(const std::string& scenePath,
                             network::PeerId ownerPeer = network::kServerPeerId);
    void Despawn(network::NetworkId id);

private:
    std::vector<std::string> GetSpawnables() const;
    int IndexOfSpawnable(const std::string& scenePath) const;
};

} // namespace components
} // namespace lupine
