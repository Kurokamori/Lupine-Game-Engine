#include "lupine/animation/AnimationPreview.hpp"
#include "lupine/animation/AnimationData.hpp"
#include "lupine/animation/ClipSampler.hpp"
#include "lupine/animation/AnimationPose.hpp"
#include "lupine/animation/AnimationChannel.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/scripting/NodeRef.hpp"
#include <algorithm>

namespace lupine {
namespace animation {

nlohmann::json PreviewClip(core::Node* root, const nlohmann::json& clipJson, float time) {
    AnimationClip clip = AnimationClip::Deserialize(clipJson);
    float len = clip.EffectiveLength();
    float t = len > 0.0f ? std::clamp(time, 0.0f, len) : 0.0f;

    AnimationPose pose;
    ClipSampler::Sample(clip, t, 1.0f, pose);
    TargetValueMap map = pose.Finalize();
    ApplyTargetValues(root, map);

    nlohmann::json targets = nlohmann::json::array();
    for (const auto& kv : map) {
        targets.push_back({ {"node", kv.second.node},
                            {"nodeUuid", kv.second.nodeUuid},
                            {"channel", kv.second.channel},
                            {"valueType", ToString(kv.second.valueType)} });
    }
    return targets;
}

nlohmann::json CapturePose(core::Node* root, const nlohmann::json& targetsJson) {
    nlohmann::json out = nlohmann::json::array();
    if (!root || !targetsJson.is_array()) return out;

    for (const auto& tj : targetsJson) {
        std::string node = tj.value("node", std::string());
        std::string nodeUuid = tj.value("nodeUuid", std::string());
        std::string channel = tj.value("channel", std::string());
        TrackValueType type = TrackValueTypeFromString(tj.value("valueType", std::string("float")));

        scripting::NodeRef target = ResolveTargetNode(root, node, nodeUuid);
        if (!target.IsValid()) continue;

        nlohmann::json value = ReadChannel(target, channel, type);
        out.push_back({ {"node", node}, {"nodeUuid", nodeUuid}, {"channel", channel},
                        {"valueType", ToString(type)}, {"value", value} });
    }
    return out;
}

void RestorePose(core::Node* root, const nlohmann::json& capturedJson) {
    if (!root || !capturedJson.is_array()) return;

    for (const auto& cj : capturedJson) {
        if (!cj.contains("value")) continue;
        std::string node = cj.value("node", std::string());
        std::string nodeUuid = cj.value("nodeUuid", std::string());
        std::string channel = cj.value("channel", std::string());
        TrackValueType type = TrackValueTypeFromString(cj.value("valueType", std::string("float")));

        scripting::NodeRef target = ResolveTargetNode(root, node, nodeUuid);
        if (!target.IsValid()) continue;

        WriteChannel(target, channel, cj["value"], type);
    }
}

} // namespace animation
} // namespace lupine
