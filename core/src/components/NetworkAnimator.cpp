#include "lupine/components/NetworkAnimator.hpp"
#include "lupine/components/NetworkObject.hpp"
#include "lupine/network/NetworkSerializer.hpp"
#include "lupine/network/NetworkManager.hpp"
#include "lupine/core/Node.hpp"
#include <cmath>

namespace lupine {
namespace components {

namespace {
nlohmann::json Arg0() { return nlohmann::json::array(); }
nlohmann::json Arg1(const nlohmann::json& a) { return nlohmann::json::array({a}); }
nlohmann::json Arg2(const nlohmann::json& a, const nlohmann::json& b) {
    return nlohmann::json::array({a, b});
}
}  // namespace

NetworkAnimator::NetworkAnimator() : core::Component("NetworkAnimator") {
    DefineProperties();
}

NetworkAnimator::NetworkAnimator(const std::string& name) : core::Component(name) {
    DefineProperties();
}

void NetworkAnimator::DefineProperties() {
    if (m_PropertiesDefined) {
        return;
    }
    DefineProperty(PROPERTY_DEFAULT(syncAnimationPlayer, Bool, true));
    DefineProperty(PROPERTY_DEFAULT(syncAnimationTree, Bool, false));
    DefineProperty(PROPERTY_DEFAULT(syncStateMachine, Bool, false));
    DefineProperty(PROPERTY_DEFAULT(stateLayers, Int, 1));
    DefineProperty(PROPERTY_DEFAULT(timeResyncThreshold, Float, 0.15f));
    DefineProperty(PROPERTY_DEFAULT(targetPath, String, std::string("")));
    // AnimationTree blend parameters to keep in sync, each "type:name" with type
    // one of float / int / bool (e.g. "float:Speed", "bool:Crouching").
    DefineProperty(core::PropertyDescriptor("treeParameters", core::PropertyValueType::StringArray,
                                            core::PropertyDefaultValue(std::vector<std::string>{})));
    m_PropertiesDefined = true;
}

bool NetworkAnimator::IsLocalAuthority() const {
    core::Node* owner = GetOwner();
    if (owner == nullptr) {
        return true;
    }
    auto* netObject = dynamic_cast<NetworkObject*>(owner->GetComponent("NetworkObject").get());
    return netObject == nullptr ? true : netObject->IsAuthority();
}

core::Node* NetworkAnimator::ResolveTarget() const {
    core::Node* owner = GetOwner();
    if (owner == nullptr) {
        return nullptr;
    }
    const std::string path =
        const_cast<NetworkAnimator*>(this)->GetPropertyValue<std::string>("targetPath");
    if (path.empty()) {
        return owner;
    }
    return owner->FindNode(path).get();
}

core::Component* NetworkAnimator::GetAnimationPlayer() const {
    core::Node* target = ResolveTarget();
    return target ? target->GetComponent("AnimationPlayer").get() : nullptr;
}

core::Component* NetworkAnimator::GetAnimationTree() const {
    core::Node* target = ResolveTarget();
    return target ? target->GetComponent("AnimationTree").get() : nullptr;
}

std::vector<NetworkAnimator::TreeParam> NetworkAnimator::GetTreeParams() const {
    std::vector<TreeParam> result;
    const std::vector<std::string> entries =
        const_cast<NetworkAnimator*>(this)->GetPropertyValue<std::vector<std::string>>(
            "treeParameters");
    result.reserve(entries.size());
    for (const std::string& entry : entries) {
        TreeParam p;
        const std::size_t colon = entry.find(':');
        std::string typeName;
        if (colon != std::string::npos) {
            typeName = entry.substr(0, colon);
            p.name = entry.substr(colon + 1);
        } else {
            p.name = entry;  // Untyped entries default to float.
        }
        if (typeName == "int") {
            p.type = 1;
        } else if (typeName == "bool") {
            p.type = 2;
        } else {
            p.type = 0;
        }
        result.push_back(p);
    }
    return result;
}

bool NetworkAnimator::NetGather(network::ByteWriter& writer, bool force) {
    core::Node* target = ResolveTarget();
    if (target == nullptr) {
        return false;
    }

    uint8_t sections = 0;
    network::ByteWriter playerBuf;
    network::ByteWriter treeBuf;
    network::ByteWriter stateBuf;

    if (GetPropertyValue<bool>("syncAnimationPlayer")) {
        if (core::Component* player = target->GetComponent("AnimationPlayer").get()) {
            const nlohmann::json animJson = player->CallMethod("current_animation", Arg0());
            const std::string anim = animJson.is_string() ? animJson.get<std::string>() : std::string();
            const nlohmann::json playingJson = player->CallMethod("is_playing", Arg0());
            const bool playing = playingJson.is_boolean() ? playingJson.get<bool>() : false;
            const nlohmann::json speedJson = player->CallMethod("get_speed", Arg0());
            const float speed = speedJson.is_number() ? speedJson.get<float>() : 1.0f;
            const nlohmann::json timeJson = player->CallMethod("current_time", Arg0());
            const float time = timeJson.is_number() ? timeJson.get<float>() : 0.0f;

            const bool changed = !m_HasLastPlayer || anim != m_LastAnim || playing != m_LastPlaying ||
                                 std::fabs(speed - m_LastSpeed) > 1e-4f;
            if (force || changed) {
                sections |= SectionPlayer;
                playerBuf.WriteString(anim);
                playerBuf.WriteFloat(time);
                playerBuf.WriteFloat(speed);
                playerBuf.WriteBool(playing);
                if (!force) {
                    m_LastAnim = anim;
                    m_LastSpeed = speed;
                    m_LastPlaying = playing;
                    m_HasLastPlayer = true;
                }
            }
        }
    }

    if (GetPropertyValue<bool>("syncAnimationTree")) {
        if (core::Component* tree = target->GetComponent("AnimationTree").get()) {
            const std::vector<TreeParam> params = GetTreeParams();
            network::ByteWriter changed;
            uint32_t changedCount = 0;
            for (uint32_t i = 0; i < params.size(); ++i) {
                const TreeParam& p = params[i];
                nlohmann::json value;
                if (p.type == 1) {
                    value = tree->CallMethod("get_int", Arg1(p.name));
                } else if (p.type == 2) {
                    value = tree->CallMethod("get_bool", Arg1(p.name));
                } else {
                    value = tree->CallMethod("get_float", Arg1(p.name));
                }
                auto it = m_LastTreeValues.find(i);
                const bool include = force || it == m_LastTreeValues.end() || it->second != value;
                if (!include) {
                    continue;
                }
                changed.WriteU32(i);
                changed.WriteU8(p.type);
                if (p.type == 1) {
                    changed.WriteI32(value.is_number() ? value.get<int32_t>() : 0);
                } else if (p.type == 2) {
                    changed.WriteBool(value.is_boolean() ? value.get<bool>() : false);
                } else {
                    changed.WriteFloat(value.is_number() ? value.get<float>() : 0.0f);
                }
                if (!force) {
                    m_LastTreeValues[i] = value;
                }
                ++changedCount;
            }
            if (changedCount > 0) {
                sections |= SectionTreeParams;
                treeBuf.WriteU32(changedCount);
                treeBuf.WriteRaw(changed.Data().data(), changed.Size());
            }

            if (GetPropertyValue<bool>("syncStateMachine")) {
                int layers = GetPropertyValue<int>("stateLayers");
                if (layers < 1) {
                    layers = 1;
                }
                if (m_LastStates.size() != static_cast<size_t>(layers)) {
                    m_LastStates.assign(static_cast<size_t>(layers), std::string());
                }
                network::ByteWriter sbuf;
                bool stateChanged = force;
                for (int l = 0; l < layers; ++l) {
                    const nlohmann::json stJson = tree->CallMethod("get_current_state", Arg1(l));
                    const std::string state = stJson.is_string() ? stJson.get<std::string>() : std::string();
                    if (state != m_LastStates[static_cast<size_t>(l)]) {
                        stateChanged = true;
                    }
                    sbuf.WriteString(state);
                    if (!force) {
                        m_LastStates[static_cast<size_t>(l)] = state;
                    }
                }
                if (stateChanged) {
                    sections |= SectionTreeState;
                    stateBuf.WriteU8(static_cast<uint8_t>(layers));
                    stateBuf.WriteRaw(sbuf.Data().data(), sbuf.Size());
                }
            }
        }
    }

    if (sections == 0) {
        return false;
    }
    writer.WriteU8(sections);
    if (sections & SectionPlayer) {
        writer.WriteRaw(playerBuf.Data().data(), playerBuf.Size());
    }
    if (sections & SectionTreeParams) {
        writer.WriteRaw(treeBuf.Data().data(), treeBuf.Size());
    }
    if (sections & SectionTreeState) {
        writer.WriteRaw(stateBuf.Data().data(), stateBuf.Size());
    }
    return true;
}

void NetworkAnimator::NetApply(network::ByteReader& reader, uint32_t serverTimeMs) {
    (void)serverTimeMs;
    core::Node* target = ResolveTarget();
    const uint8_t sections = reader.ReadU8();
    if (!reader.Ok()) {
        return;
    }

    if (sections & SectionPlayer) {
        const std::string anim = reader.ReadString();
        const float time = reader.ReadFloat();
        const float speed = reader.ReadFloat();
        const bool playing = reader.ReadBool();
        if (!reader.Ok()) {
            return;
        }
        core::Component* player = target ? target->GetComponent("AnimationPlayer").get() : nullptr;
        if (player != nullptr && !anim.empty()) {
            const nlohmann::json curJson = player->CallMethod("current_animation", Arg0());
            const std::string cur = curJson.is_string() ? curJson.get<std::string>() : std::string();
            const nlohmann::json playingJson = player->CallMethod("is_playing", Arg0());
            const bool isPlaying = playingJson.is_boolean() ? playingJson.get<bool>() : false;

            if (cur != anim) {
                player->CallMethod("play", Arg1(anim));
                player->CallMethod("seek", Arg1(time));
            } else {
                const nlohmann::json timeJson = player->CallMethod("current_time", Arg0());
                const float curTime = timeJson.is_number() ? timeJson.get<float>() : 0.0f;
                const float threshold = GetPropertyValue<float>("timeResyncThreshold");
                if (std::fabs(curTime - time) > threshold) {
                    player->CallMethod("seek", Arg1(time));
                }
            }
            player->CallMethod("set_speed", Arg1(speed));
            if (playing) {
                if (cur == anim && !isPlaying) {
                    player->CallMethod("resume", Arg0());
                }
            } else {
                player->CallMethod("pause", Arg0());
            }
        }
    }

    if (sections & SectionTreeParams) {
        const uint32_t count = reader.ReadU32();
        if (!reader.Ok() || count > 0xFFFFu) {
            return;
        }
        const std::vector<TreeParam> params = GetTreeParams();
        core::Component* tree = target ? target->GetComponent("AnimationTree").get() : nullptr;
        for (uint32_t c = 0; c < count && reader.Ok(); ++c) {
            const uint32_t index = reader.ReadU32();
            const uint8_t type = reader.ReadU8();
            nlohmann::json value;
            if (type == 1) {
                value = static_cast<int64_t>(reader.ReadI32());
            } else if (type == 2) {
                value = reader.ReadBool();
            } else {
                value = reader.ReadFloat();
            }
            if (!reader.Ok() || tree == nullptr || index >= params.size()) {
                continue;
            }
            const std::string& name = params[index].name;
            if (type == 1) {
                tree->CallMethod("set_int", Arg2(name, value));
            } else if (type == 2) {
                tree->CallMethod("set_bool", Arg2(name, value));
            } else {
                tree->CallMethod("set_float", Arg2(name, value));
            }
        }
    }

    if (sections & SectionTreeState) {
        const uint8_t layers = reader.ReadU8();
        if (!reader.Ok()) {
            return;
        }
        core::Component* tree = target ? target->GetComponent("AnimationTree").get() : nullptr;
        for (uint8_t l = 0; l < layers && reader.Ok(); ++l) {
            const std::string state = reader.ReadString();
            if (tree != nullptr && !state.empty()) {
                tree->CallMethod("travel", Arg2(state, static_cast<int>(l)));
            }
        }
    }
}

} // namespace components
} // namespace lupine
