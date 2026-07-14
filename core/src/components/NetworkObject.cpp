#include "lupine/components/NetworkObject.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/network/NetworkManager.hpp"
#include "lupine/network/ReplicationManager.hpp"

namespace lupine {
namespace components {

NetworkObject::NetworkObject() : core::Component("NetworkObject") {
    DefineProperties();
}

NetworkObject::NetworkObject(const std::string& name) : core::Component(name) {
    DefineProperties();
}

NetworkObject::~NetworkObject() {
    UnregisterIfNeeded();
}

void NetworkObject::DefineProperties() {
    if (m_PropertiesDefined) {
        return;
    }
    DefineProperty(PROPERTY_DEFAULT(serverAuthoritative, Bool, true));
    DefineProperty(PROPERTY_DEFAULT(persistAcrossScenes, Bool, false));
    // When area-of-interest culling is enabled (NetworkConfig::interestRadius),
    // an object marked alwaysRelevant is replicated to every peer regardless of
    // distance (e.g. global/world-state objects). Distance-cullable objects leave
    // this off.
    DefineProperty(PROPERTY_DEFAULT(alwaysRelevant, Bool, false));
    m_PropertiesDefined = true;
}

void NetworkObject::DefineSignals() {
    RegisterSignal({"authority_changed", {{"peer", core::PropertyValueType::Int}}, "Authority of this object changed"});
}

void NetworkObject::OnEnterTree() {
    RegisterIfPossible();
}

void NetworkObject::OnExitTree() {
    UnregisterIfNeeded();
}

void NetworkObject::SetNetworkId(network::NetworkId id) {
    if (id == m_NetworkId) {
        return;
    }
    UnregisterIfNeeded();
    m_NetworkId = id;
    RegisterIfPossible();
}

void NetworkObject::SetOwnerPeerId(network::PeerId peer) {
    if (peer == m_OwnerPeer) {
        return;
    }
    m_OwnerPeer = peer;
    Emit("authority_changed", {static_cast<int64_t>(peer)});
}

bool NetworkObject::IsAuthority() const {
    network::NetworkManager& net = network::NetworkManager::GetInstance();
    if (!net.IsActive()) {
        // Single-player: the local instance owns everything, so authority-gated
        // game logic still runs when networking is not in use.
        return true;
    }
    return net.HasAuthorityOver(m_OwnerPeer);
}

void NetworkObject::RegisterIfPossible() {
    if (m_Registered || m_NetworkId == network::kInvalidNetworkId) {
        return;
    }
    core::Node* owner = GetOwner();
    if (owner == nullptr) {
        return;
    }
    network::NetworkManager::GetInstance().Replication().RegisterObject(m_NetworkId, owner);
    m_Registered = true;
}

void NetworkObject::UnregisterIfNeeded() {
    if (!m_Registered) {
        return;
    }
    network::NetworkManager::GetInstance().Replication().UnregisterObject(m_NetworkId);
    m_Registered = false;
}

nlohmann::json NetworkObject::CallMethod(const std::string& method, const nlohmann::json& args) {
    if (method == "get_network_id") {
        return nlohmann::json(static_cast<int64_t>(m_NetworkId));
    }
    if (method == "get_owner" || method == "get_authority") {
        return nlohmann::json(static_cast<int64_t>(m_OwnerPeer));
    }
    if (method == "is_authority" || method == "is_mine") {
        return nlohmann::json(IsAuthority());
    }
    if (method == "set_authority" || method == "set_owner") {
        if (args.is_array() && !args.empty() && args[0].is_number()) {
            SetOwnerPeerId(static_cast<network::PeerId>(args[0].get<int64_t>()));
        }
        return nlohmann::json(nullptr);
    }
    if (method == "set_network_id") {
        if (args.is_array() && !args.empty() && args[0].is_number()) {
            SetNetworkId(static_cast<network::NetworkId>(args[0].get<int64_t>()));
        }
        return nlohmann::json(nullptr);
    }
    return nlohmann::json(nullptr);
}

} // namespace components
} // namespace lupine
