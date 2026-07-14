#include "lupine/components/NetworkSpawner.hpp"
#include "lupine/network/NetworkManager.hpp"
#include "lupine/network/ReplicationManager.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/logger/Logger.hpp"

namespace lupine {
namespace components {

NetworkSpawner::NetworkSpawner() : core::Component("NetworkSpawner") {
    DefineProperties();
}

NetworkSpawner::NetworkSpawner(const std::string& name) : core::Component(name) {
    DefineProperties();
}

void NetworkSpawner::DefineProperties() {
    if (m_PropertiesDefined) {
        return;
    }
    DefineProperty(core::PropertyDescriptor("spawnableScenes", core::PropertyValueType::StringArray,
                                            core::PropertyDefaultValue(std::vector<std::string>{})));
    DefineProperty(PROPERTY_DEFAULT(autoSpawnOnReady, Bool, false));
    m_PropertiesDefined = true;
}

std::vector<std::string> NetworkSpawner::GetSpawnables() const {
    return const_cast<NetworkSpawner*>(this)->GetPropertyValue<std::vector<std::string>>(
        "spawnableScenes");
}

int NetworkSpawner::IndexOfSpawnable(const std::string& scenePath) const {
    const std::vector<std::string> spawnables = GetSpawnables();
    for (std::size_t i = 0; i < spawnables.size(); ++i) {
        if (spawnables[i] == scenePath) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

network::NetworkId NetworkSpawner::Spawn(const std::string& scenePath, network::PeerId ownerPeer) {
    core::Node* owner = GetOwner();
    if (owner == nullptr) {
        return network::kInvalidNetworkId;
    }
    const int index = IndexOfSpawnable(scenePath);
    if (index < 0) {
        LOG_NETWORK_WARN("NetworkSpawner: '{}' is not in the spawnable whitelist", scenePath);
        return network::kInvalidNetworkId;
    }
    return network::NetworkManager::GetInstance().Replication().SpawnScene(owner, index, scenePath, ownerPeer);
}

void NetworkSpawner::Despawn(network::NetworkId id) {
    network::NetworkManager::GetInstance().Replication().DespawnObject(id);
}

void NetworkSpawner::OnReady() {
    if (!GetPropertyValue<bool>("autoSpawnOnReady")) {
        return;
    }
    network::NetworkManager& net = network::NetworkManager::GetInstance();
    const bool canSpawn = !net.IsActive() || net.IsServer() ||
                          net.GetMode() == network::SessionMode::P2P;
    if (!canSpawn) {
        return;  // Clients receive spawns from the authority.
    }
    const network::PeerId owner = net.GetLocalPeerId() != network::kInvalidPeerId
                                      ? net.GetLocalPeerId()
                                      : network::kServerPeerId;
    for (const std::string& path : GetSpawnables()) {
        Spawn(path, owner);
    }
}

nlohmann::json NetworkSpawner::CallMethod(const std::string& method, const nlohmann::json& args) {
    if (method == "spawn") {
        if (!args.is_array() || args.empty() || !args[0].is_string()) {
            return nlohmann::json(static_cast<int64_t>(network::kInvalidNetworkId));
        }
        network::PeerId owner = network::kServerPeerId;
        if (args.size() > 1 && args[1].is_number()) {
            owner = static_cast<network::PeerId>(args[1].get<int64_t>());
        }
        const network::NetworkId id = Spawn(args[0].get<std::string>(), owner);
        return nlohmann::json(static_cast<int64_t>(id));
    }
    if (method == "despawn") {
        if (args.is_array() && !args.empty() && args[0].is_number()) {
            Despawn(static_cast<network::NetworkId>(args[0].get<int64_t>()));
        }
        return nlohmann::json(nullptr);
    }
    if (method == "__resolve_spawnable") {
        if (args.is_array() && !args.empty() && args[0].is_number()) {
            const int index = args[0].get<int>();
            const std::vector<std::string> spawnables = GetSpawnables();
            if (index >= 0 && index < static_cast<int>(spawnables.size())) {
                return nlohmann::json(spawnables[index]);
            }
        }
        return nlohmann::json(nullptr);
    }
    if (method == "get_spawnable_count") {
        return nlohmann::json(static_cast<int64_t>(GetSpawnables().size()));
    }
    return nlohmann::json(nullptr);
}

} // namespace components
} // namespace lupine
