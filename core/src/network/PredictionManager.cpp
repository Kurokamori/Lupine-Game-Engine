#include "lupine/network/PredictionManager.hpp"
#include "lupine/network/NetworkManager.hpp"
#include "lupine/network/ReplicationManager.hpp"
#include "lupine/components/NetworkController.hpp"
#include "lupine/core/Node.hpp"

#include <algorithm>

namespace lupine {
namespace network {

PredictionManager::PredictionManager(NetworkManager& owner) : m_Net(owner) {}

void PredictionManager::Initialize() {
    m_Net.RegisterMessageHandler(MsgType::InputCommand,
        [this](PeerId from, ByteReader& reader) { OnInputCommandMessage(from, reader); });
}

void PredictionManager::OnSessionEnded() {
    for (components::NetworkController* controller : m_Controllers) {
        if (controller != nullptr) {
            controller->ResetSessionState();
        }
    }
}

void PredictionManager::Register(components::NetworkController* controller) {
    if (controller == nullptr) {
        return;
    }
    if (std::find(m_Controllers.begin(), m_Controllers.end(), controller) == m_Controllers.end()) {
        m_Controllers.push_back(controller);
    }
}

void PredictionManager::Unregister(components::NetworkController* controller) {
    m_Controllers.erase(std::remove(m_Controllers.begin(), m_Controllers.end(), controller),
                        m_Controllers.end());
}

void PredictionManager::Tick(float fixedDeltaTime) {
    if (!m_Net.IsActive()) {
        return;
    }
    const bool server = m_Net.IsServer();
    for (components::NetworkController* controller : m_Controllers) {
        if (controller == nullptr) {
            continue;
        }
        if (server) {
            controller->ServerTick(fixedDeltaTime);
        } else if (controller->IsPredictingLocally()) {
            controller->ClientReconcile(fixedDeltaTime);
            controller->ClientSampleAndSend(fixedDeltaTime);
        }
    }
}

void PredictionManager::OnInputCommandMessage(PeerId from, ByteReader& reader) {
    if (!m_Net.IsServer()) {
        return;  // Only the authority consumes input commands.
    }
    const NetworkId networkId = reader.ReadU32();
    if (!reader.Ok()) {
        return;
    }
    core::Node* node = m_Net.Replication().ResolveNode(networkId);
    if (node == nullptr) {
        return;
    }
    std::shared_ptr<core::Component> comp = node->GetComponent("NetworkController");
    if (auto* controller = dynamic_cast<components::NetworkController*>(comp.get())) {
        controller->ReceiveInput(from, reader);
    }
}

} // namespace network
} // namespace lupine
