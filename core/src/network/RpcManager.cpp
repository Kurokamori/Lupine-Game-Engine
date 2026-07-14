#include "lupine/network/RpcManager.hpp"
#include "lupine/network/NetworkManager.hpp"
#include "lupine/network/ReplicationManager.hpp"
#include "lupine/components/NetworkObject.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/ScriptComponent.hpp"
#include "lupine/logger/Logger.hpp"

namespace lupine {
namespace network {

namespace {

components::NetworkObject* FindNetworkObject(core::Node* node) {
    if (node == nullptr) {
        return nullptr;
    }
    std::shared_ptr<core::Component> comp = node->GetComponent("NetworkObject");
    return dynamic_cast<components::NetworkObject*>(comp.get());
}

Channel ChannelFor(TransferMode transfer) {
    return (transfer == TransferMode::Reliable) ? Channel::ReliableOrdered
                                                : Channel::UnreliableSequenced;
}

} // namespace

RpcManager::RpcManager(NetworkManager& owner) : m_Net(owner) {}

void RpcManager::Initialize() {
    m_Net.RegisterMessageHandler(MsgType::Rpc,
        [this](PeerId from, ByteReader& reader) { OnRpcMessage(from, reader); });
}

void RpcManager::RegisterComponentRpc(const std::string& componentType, const std::string& method,
                                      const RpcConfig& config) {
    m_ComponentConfigs[componentType + "::" + method] = config;
}

const RpcConfig* RpcManager::GetComponentRpc(const std::string& componentType,
                                             const std::string& method) const {
    auto it = m_ComponentConfigs.find(componentType + "::" + method);
    return (it != m_ComponentConfigs.end()) ? &it->second : nullptr;
}

RpcConfig RpcManager::ResolveScriptConfig(core::Node* node, const std::string& method) const {
    if (node != nullptr) {
        for (const std::shared_ptr<core::Component>& comp : node->GetComponents()) {
            if (auto* script = dynamic_cast<core::ScriptComponent*>(comp.get())) {
                if (const RpcConfig* config = script->GetRpcConfig(method)) {
                    return *config;
                }
            }
        }
    }
    // Undeclared RPCs default to the most forgiving config so a call still works;
    // declaring `--@rpc` is recommended to tighten authority/reliability.
    RpcConfig fallback;
    fallback.mode = RpcMode::AnyPeer;
    fallback.transfer = TransferMode::Reliable;
    fallback.callLocal = false;
    return fallback;
}

void RpcManager::SendNodeRpc(core::Node* node, const std::string& method,
                             const nlohmann::json& args, PeerId targetPeer,
                             bool overrideTransfer, TransferMode transfer) {
    if (node == nullptr) {
        return;
    }
    RpcConfig config = ResolveScriptConfig(node, method);
    if (overrideTransfer) {
        config.transfer = transfer;
    }

    if (!m_Net.IsActive()) {
        // Single-player: run the RPC locally so annotated functions still fire.
        DispatchLocal(node, MethodKind::Script, std::string(), method, args, m_Net.GetLocalPeerId());
        return;
    }
    SendResolved(node, MethodKind::Script, std::string(), method, args, targetPeer, config);
}

void RpcManager::SendComponentRpc(core::Node* node, const std::string& componentType,
                                  const std::string& method, const nlohmann::json& args,
                                  PeerId targetPeer, bool overrideTransfer, TransferMode transfer) {
    if (node == nullptr) {
        return;
    }
    RpcConfig config;
    if (const RpcConfig* registered = GetComponentRpc(componentType, method)) {
        config = *registered;
    } else {
        config.mode = RpcMode::AnyPeer;
    }
    if (overrideTransfer) {
        config.transfer = transfer;
    }

    if (!m_Net.IsActive()) {
        DispatchLocal(node, MethodKind::Component, componentType, method, args, m_Net.GetLocalPeerId());
        return;
    }
    SendResolved(node, MethodKind::Component, componentType, method, args, targetPeer, config);
}

void RpcManager::SendResolved(core::Node* node, MethodKind kind, const std::string& componentType,
                              const std::string& method, const nlohmann::json& args,
                              PeerId targetPeer, const RpcConfig& config) {
    const NetworkId networkId = m_Net.Replication().NetworkIdOf(node);
    if (networkId == kInvalidNetworkId) {
        LOG_NETWORK_WARN("RPC '{}' on a node with no NetworkObject/NetworkId - ignored", method);
        return;
    }

    components::NetworkObject* netObject = FindNetworkObject(node);
    if (config.mode == RpcMode::Authority && netObject != nullptr && !netObject->IsAuthority()) {
        LOG_NETWORK_WARN("RPC '{}' is authority-only but the local peer is not the authority", method);
        return;
    }

    const PeerId localPeer = m_Net.GetLocalPeerId();
    const Channel channel = ChannelFor(config.transfer);

    const bool runLocal = config.callLocal || targetPeer == localPeer;
    if (runLocal) {
        DispatchLocal(node, kind, componentType, method, args, localPeer);
    }
    if (targetPeer == localPeer) {
        return;  // Destination was purely local.
    }

    const std::vector<uint8_t> payload =
        Encode(networkId, localPeer, targetPeer, kind, config.transfer, componentType, method, args);

    if (m_Net.IsServer() || m_Net.GetMode() == SessionMode::P2P) {
        if (targetPeer == kInvalidPeerId) {
            m_Net.BroadcastMessage(channel, MsgType::Rpc, payload);
        } else {
            m_Net.SendTo(targetPeer, channel, MsgType::Rpc, payload);
        }
    } else {
        // Client: route through the authoritative server, which validates and relays.
        m_Net.SendTo(kServerPeerId, channel, MsgType::Rpc, payload);
    }
}

void RpcManager::OnRpcMessage(PeerId from, ByteReader& reader) {
    (void)from;
    const NetworkId networkId = reader.ReadU32();
    const PeerId sender = reader.ReadU32();
    const PeerId targetPeer = reader.ReadU32();
    const MethodKind kind = static_cast<MethodKind>(reader.ReadU8());
    const TransferMode transfer = static_cast<TransferMode>(reader.ReadU8());
    const std::string componentType = reader.ReadString();
    const std::string method = reader.ReadString();
    const nlohmann::json args = reader.ReadJson();
    if (!reader.Ok()) {
        LOG_NETWORK_WARN("Dropped malformed RPC frame");
        return;
    }

    core::Node* node = m_Net.Replication().ResolveNode(networkId);
    if (node == nullptr) {
        LOG_NETWORK_DEBUG("RPC for unknown NetworkId {}", networkId);
        return;
    }

    const Channel channel = ChannelFor(transfer);

    if (m_Net.IsServer()) {
        // Authority validation: an authority-only RPC must come from the object's
        // owner. This is the server's anti-cheat gate.
        RpcConfig config = (kind == MethodKind::Script)
                               ? ResolveScriptConfig(node, method)
                               : RpcConfig{};
        if (kind == MethodKind::Component) {
            if (const RpcConfig* registered = GetComponentRpc(componentType, method)) {
                config = *registered;
            } else {
                config.mode = RpcMode::AnyPeer;
            }
        }
        if (config.mode == RpcMode::Authority) {
            components::NetworkObject* netObject = FindNetworkObject(node);
            if (netObject == nullptr || sender != netObject->GetOwnerPeerId()) {
                LOG_NETWORK_WARN("Rejected authority RPC '{}' from non-owner peer {}", method, sender);
                return;
            }
        }

        if (targetPeer == kInvalidPeerId) {
            DispatchLocal(node, kind, componentType, method, args, sender);
            const std::vector<uint8_t> payload =
                Encode(networkId, sender, targetPeer, kind, transfer, componentType, method, args);
            m_Net.BroadcastMessage(channel, MsgType::Rpc, payload, /*except*/ sender);
        } else if (targetPeer == m_Net.GetLocalPeerId()) {
            DispatchLocal(node, kind, componentType, method, args, sender);
        } else {
            const std::vector<uint8_t> payload =
                Encode(networkId, sender, targetPeer, kind, transfer, componentType, method, args);
            m_Net.SendTo(targetPeer, channel, MsgType::Rpc, payload);
        }
    } else {
        // Client / P2P peer: the message reached its destination; run it.
        DispatchLocal(node, kind, componentType, method, args, sender);
    }
}

void RpcManager::DispatchLocal(core::Node* node, MethodKind kind, const std::string& componentType,
                               const std::string& method, const nlohmann::json& args, PeerId sender) {
    if (node == nullptr) {
        return;
    }
    const PeerId previousSender = m_CurrentSender;
    m_CurrentSender = sender;

    if (kind == MethodKind::Script) {
        for (const std::shared_ptr<core::Component>& comp : node->GetComponents()) {
            if (auto* script = dynamic_cast<core::ScriptComponent*>(comp.get())) {
                if (scripting::IScriptEnvironment* env = script->GetScriptEnvironment()) {
                    env->CallFunctionArgs(method, args);
                }
            }
        }
    } else {
        std::shared_ptr<core::Component> comp = node->GetComponent(componentType);
        if (comp) {
            comp->CallMethod(method, args);
        }
    }

    m_CurrentSender = previousSender;
}

std::vector<uint8_t> RpcManager::Encode(NetworkId networkId, PeerId sender, PeerId targetPeer,
                                        MethodKind kind, TransferMode transfer,
                                        const std::string& componentType, const std::string& method,
                                        const nlohmann::json& args) const {
    ByteWriter writer;
    writer.WriteU32(networkId);
    writer.WriteU32(sender);
    writer.WriteU32(targetPeer);
    writer.WriteU8(static_cast<uint8_t>(kind));
    writer.WriteU8(static_cast<uint8_t>(transfer));
    writer.WriteString(componentType);
    writer.WriteString(method);
    writer.WriteJson(args);
    return writer.Take();
}

} // namespace network
} // namespace lupine
