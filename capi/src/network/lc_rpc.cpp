#include "network/lc_rpc.h"
#include "core/lc_core.h"
#include "../core/lc_internal.h"

#include <lupine/network/NetworkManager.hpp>
#include <lupine/network/RpcManager.hpp>
#include <lupine/core/Node.hpp>

#include <nlohmann/json.hpp>

namespace {

nlohmann::json ToArgs(const char* args_json) {
    nlohmann::json args = nlohmann::json::array();
    if (!args_json || args_json[0] == '\0') {
        return args;
    }
    nlohmann::json parsed = nlohmann::json::parse(args_json, nullptr, /*allow_exceptions=*/false);
    if (parsed.is_array()) {
        return parsed;
    }
    if (!parsed.is_discarded() && !parsed.is_null()) {
        args.push_back(parsed);
    }
    return args;
}

} // namespace

LC_API LCResult lc_net_rpc(LCNodeHandle node, const char* method, const char* args_json) {
    if (!method) {
        SetError(LC_ERROR_NULL_POINTER, "method is null");
        return LC_ERROR_NULL_POINTER;
    }
    auto n = GetNode(node);
    if (!n) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    lupine::network::NetworkManager::GetInstance().Rpc().SendNodeRpc(n.get(), method, ToArgs(args_json));
    return LC_SUCCESS;
}

LC_API LCResult lc_net_rpc_id(LCNodeHandle node, uint32_t peer_id,
                              const char* method, const char* args_json) {
    if (!method) {
        SetError(LC_ERROR_NULL_POINTER, "method is null");
        return LC_ERROR_NULL_POINTER;
    }
    auto n = GetNode(node);
    if (!n) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    lupine::network::NetworkManager::GetInstance().Rpc().SendNodeRpc(
        n.get(), method, ToArgs(args_json), peer_id);
    return LC_SUCCESS;
}

LC_API LCResult lc_net_rpc_unreliable(LCNodeHandle node, const char* method, const char* args_json) {
    if (!method) {
        SetError(LC_ERROR_NULL_POINTER, "method is null");
        return LC_ERROR_NULL_POINTER;
    }
    auto n = GetNode(node);
    if (!n) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid node handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    lupine::network::NetworkManager::GetInstance().Rpc().SendNodeRpc(
        n.get(), method, ToArgs(args_json), lupine::network::kInvalidPeerId,
        /*overrideTransfer*/ true, lupine::network::TransferMode::Unreliable);
    return LC_SUCCESS;
}

LC_API LCResult lc_net_register_component_rpc(const char* component_type, const char* method,
                                              int mode, int transfer, bool call_local) {
    if (!component_type || !method) {
        SetError(LC_ERROR_NULL_POINTER, "component_type or method is null");
        return LC_ERROR_NULL_POINTER;
    }
    lupine::network::RpcConfig config;
    config.mode = (mode == LC_RPC_MODE_ANY_PEER) ? lupine::network::RpcMode::AnyPeer
                                                 : lupine::network::RpcMode::Authority;
    config.transfer = (transfer == LC_RPC_TRANSFER_UNRELIABLE)
                          ? lupine::network::TransferMode::Unreliable
                          : lupine::network::TransferMode::Reliable;
    config.callLocal = call_local;
    lupine::network::NetworkManager::GetInstance().Rpc().RegisterComponentRpc(
        component_type, method, config);
    return LC_SUCCESS;
}
