#include "network/lc_replication.h"
#include "core/lc_core.h"
#include "../core/lc_internal.h"

#include <lupine/network/NetworkManager.hpp>
#include <lupine/network/ReplicationManager.hpp>
#include <lupine/core/Node.hpp>
#include <lupine/core/Component.hpp>

#include <nlohmann/json.hpp>

namespace {

// Invoke a method on a named component of a node via the generic CallMethod
// dispatch, returning its JSON result (null if the component/method is absent).
nlohmann::json CallOnComponent(const std::shared_ptr<lupine::core::Node>& node,
                               const char* componentType, const std::string& method,
                               const nlohmann::json& args) {
    if (!node) {
        return nlohmann::json(nullptr);
    }
    std::shared_ptr<lupine::core::Component> comp = node->GetComponent(componentType);
    if (!comp) {
        return nlohmann::json(nullptr);
    }
    return comp->CallMethod(method, args);
}

} // namespace

LC_API LCResult lc_net_object_id(LCNodeHandle node, uint32_t* out_id) {
    if (!out_id) {
        SetError(LC_ERROR_NULL_POINTER, "out_id is null");
        return LC_ERROR_NULL_POINTER;
    }
    auto n = GetNode(node);
    if (!n) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    const nlohmann::json result =
        CallOnComponent(n, "NetworkObject", "get_network_id", nlohmann::json::array());
    *out_id = result.is_number() ? result.get<uint32_t>() : 0u;
    return LC_SUCCESS;
}

LC_API LCResult lc_net_set_authority(LCNodeHandle node, uint32_t peer_id) {
    auto n = GetNode(node);
    if (!n) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    CallOnComponent(n, "NetworkObject", "set_authority",
                    nlohmann::json::array({static_cast<int64_t>(peer_id)}));
    return LC_SUCCESS;
}

LC_API uint32_t lc_net_get_authority(LCNodeHandle node) {
    auto n = GetNode(node);
    if (!n) {
        return 0u;
    }
    const nlohmann::json result =
        CallOnComponent(n, "NetworkObject", "get_owner", nlohmann::json::array());
    return result.is_number() ? result.get<uint32_t>() : 0u;
}

LC_API bool lc_net_is_authority(LCNodeHandle node) {
    auto n = GetNode(node);
    if (!n) {
        return false;
    }
    const nlohmann::json result =
        CallOnComponent(n, "NetworkObject", "is_authority", nlohmann::json::array());
    return result.is_boolean() ? result.get<bool>() : false;
}

LC_API LCResult lc_net_spawn(LCNodeHandle spawner, const char* scene_path,
                             uint32_t owner_peer, uint32_t* out_id) {
    if (!scene_path) {
        SetError(LC_ERROR_NULL_POINTER, "scene_path is null");
        return LC_ERROR_NULL_POINTER;
    }
    auto n = GetNode(spawner);
    if (!n) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    const nlohmann::json result = CallOnComponent(
        n, "NetworkSpawner", "spawn",
        nlohmann::json::array({std::string(scene_path), static_cast<int64_t>(owner_peer)}));
    const uint32_t id = result.is_number() ? result.get<uint32_t>() : 0u;
    if (out_id) {
        *out_id = id;
    }
    return id != 0u ? LC_SUCCESS : LC_ERROR_OPERATION_FAILED;
}

LC_API LCResult lc_net_despawn(uint32_t network_id) {
    lupine::network::NetworkManager::GetInstance().Replication().DespawnObject(network_id);
    return LC_SUCCESS;
}
