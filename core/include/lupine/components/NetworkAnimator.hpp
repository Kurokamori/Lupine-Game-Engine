#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/network/INetworkSerializable.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>

namespace lupine {
namespace core {
class Node;
}
namespace components {

/**
 * NetworkAnimator - replicates animation state from an object's authority to the
 * other peers.
 *
 * It drives an AnimationPlayer (current clip, playback time, speed, play/pause)
 * and/or an AnimationTree (a declared list of blend parameters plus, optionally,
 * the per-layer state-machine state) sitting on the same node or a referenced
 * child. Clip / parameter changes ride a delta on change; a periodic keyframe
 * (forced gather) resynchronises playback time so a dropped delta cannot strand a
 * receiver on the wrong clip. Receivers play the clip locally so it advances on
 * its own between updates - only changes and drift corrections cross the wire.
 *
 * Inert when no session is active or on the object's authority (which drives the
 * animation directly through gameplay).
 */
class NetworkAnimator : public core::Component, public network::INetworkSerializable {
public:
    NetworkAnimator();
    explicit NetworkAnimator(const std::string& name);

    std::string GetTypeName() const override { return "NetworkAnimator"; }
    void DefineProperties() override;

    bool NetGather(network::ByteWriter& writer, bool force) override;
    void NetApply(network::ByteReader& reader, uint32_t serverTimeMs) override;

private:
    enum Section : uint8_t {
        SectionPlayer = 1u,
        SectionTreeParams = 2u,
        SectionTreeState = 4u
    };

    bool IsLocalAuthority() const;
    core::Node* ResolveTarget() const;
    core::Component* GetAnimationPlayer() const;
    core::Component* GetAnimationTree() const;

    // Parsed `treeParameters` entries, each "type:name" with type in {float,int,bool}.
    struct TreeParam {
        uint8_t type = 0;  // 0 float, 1 int, 2 bool
        std::string name;
    };
    std::vector<TreeParam> GetTreeParams() const;

    // Authority-side change-detection baselines.
    std::string m_LastAnim;
    float m_LastSpeed = 1.0f;
    bool m_LastPlaying = false;
    bool m_HasLastPlayer = false;
    std::unordered_map<uint32_t, nlohmann::json> m_LastTreeValues;  // param index -> value
    std::vector<std::string> m_LastStates;
};

} // namespace components
} // namespace lupine
