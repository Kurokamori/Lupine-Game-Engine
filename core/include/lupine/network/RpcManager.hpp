#pragma once

#include "lupine/network/NetworkTypes.hpp"
#include "lupine/network/NetworkSerializer.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>

namespace lupine {
namespace core {
class Node;
}

namespace network {

class NetworkManager;

/**
 * Routes remote procedure calls between peers.
 *
 * A call targets a node (resolved across machines by its NetworkId) and names
 * either a script function (the default - dispatched via the node's script
 * components) or a component method (dispatched via Component::CallMethod). Each
 * RPC's behaviour comes from its RpcConfig: who may call it (authority vs any
 * peer), the delivery guarantee (reliable/unreliable), and whether it also runs
 * on the caller (call_local). Script configs are parsed from `--@rpc` annotations
 * and read off the node's ScriptComponent; component configs are registered here.
 *
 * Routing follows the authoritative model: clients send to the server, which
 * validates authority and relays to the other peers; the server/host broadcasts
 * directly; P2P peers send directly to one another. Offline, an RPC simply runs
 * locally so rpc-annotated functions still work in single-player.
 */
class RpcManager {
public:
    explicit RpcManager(NetworkManager& owner);

    // Register the MsgType::Rpc handler on the owning manager.
    void Initialize();

    // Register an RPC configuration for a component type's method (used by native
    // components and the C-API; script RPCs are configured via `--@rpc`).
    void RegisterComponentRpc(const std::string& componentType, const std::string& method,
                              const RpcConfig& config);
    const RpcConfig* GetComponentRpc(const std::string& componentType,
                                     const std::string& method) const;

    // Invoke a script-function RPC on a node. targetPeer == kInvalidPeerId means
    // "broadcast per the RPC's config"; a specific id targets one peer.
    void SendNodeRpc(core::Node* node, const std::string& method, const nlohmann::json& args,
                     PeerId targetPeer = kInvalidPeerId,
                     bool overrideTransfer = false,
                     TransferMode transfer = TransferMode::Reliable);

    // Invoke a component-method RPC on a node.
    void SendComponentRpc(core::Node* node, const std::string& componentType,
                          const std::string& method, const nlohmann::json& args,
                          PeerId targetPeer = kInvalidPeerId,
                          bool overrideTransfer = false,
                          TransferMode transfer = TransferMode::Reliable);

    // Peer id of the sender of the RPC currently being dispatched (Godot's
    // get_rpc_sender_id); kInvalidPeerId outside of dispatch.
    PeerId GetCurrentSender() const { return m_CurrentSender; }

private:
    enum class MethodKind : uint8_t { Script = 0, Component = 1 };

    void OnRpcMessage(PeerId from, ByteReader& reader);

    RpcConfig ResolveScriptConfig(core::Node* node, const std::string& method) const;

    void SendResolved(core::Node* node, MethodKind kind, const std::string& componentType,
                      const std::string& method, const nlohmann::json& args,
                      PeerId targetPeer, const RpcConfig& config);

    void DispatchLocal(core::Node* node, MethodKind kind, const std::string& componentType,
                       const std::string& method, const nlohmann::json& args, PeerId sender);

    std::vector<uint8_t> Encode(NetworkId networkId, PeerId sender, PeerId targetPeer,
                                MethodKind kind, TransferMode transfer,
                                const std::string& componentType, const std::string& method,
                                const nlohmann::json& args) const;

    NetworkManager& m_Net;
    std::unordered_map<std::string, RpcConfig> m_ComponentConfigs;  // "Type::method" -> config
    PeerId m_CurrentSender = kInvalidPeerId;
};

} // namespace network
} // namespace lupine
