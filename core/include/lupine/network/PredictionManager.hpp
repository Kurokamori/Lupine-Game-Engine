#pragma once

#include "lupine/network/NetworkTypes.hpp"
#include "lupine/network/NetworkSerializer.hpp"
#include <vector>

namespace lupine {
namespace components {
class NetworkController;
}
namespace network {

class NetworkManager;

/**
 * Drives client-side prediction and server reconciliation for every
 * NetworkController in the scene.
 *
 * Controllers register themselves as they enter the tree. Once per fixed tick
 * (from NetworkManager::GenerateSnapshot, before the replication snapshot is
 * gathered) the manager lets each controller do its role-appropriate work: the
 * server applies queued input and advances authoritative state; a controlling
 * client reconciles against the latest authoritative snapshot and then samples,
 * predicts, and sends its next input command. Inbound InputCommand frames are
 * routed to the addressed controller here.
 *
 * Scoped to its owning NetworkManager instance, like the RPC and replication
 * managers, so independent instances in a test never share routing state.
 */
class PredictionManager {
public:
    explicit PredictionManager(NetworkManager& owner);

    // Register the InputCommand message handler on the owning manager.
    void Initialize();

    // Reset every registered controller's per-session prediction state.
    void OnSessionEnded();

    void Register(components::NetworkController* controller);
    void Unregister(components::NetworkController* controller);

    // Per fixed tick. No-op when offline.
    void Tick(float fixedDeltaTime);

private:
    void OnInputCommandMessage(PeerId from, ByteReader& reader);

    NetworkManager& m_Net;
    std::vector<components::NetworkController*> m_Controllers;
};

} // namespace network
} // namespace lupine
