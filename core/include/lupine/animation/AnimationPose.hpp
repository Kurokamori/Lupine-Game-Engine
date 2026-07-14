#pragma once

#include "lupine/animation/AnimationTypes.hpp"
#include "lupine/math/Math.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace lupine {
namespace core { class Node; }
namespace animation {

/**
 * A single resolved animated value (absolute), ready to be written to a node.
 */
struct TargetValue {
    std::string node;
    std::string nodeUuid;
    std::string channel;
    TrackValueType valueType = TrackValueType::Float;
    bool useSlerp = false;
    nlohmann::json value;
};

// Finalized pose: target key ("node\x1Fchannel") -> resolved value.
using TargetValueMap = std::unordered_map<std::string, TargetValue>;

/**
 * AnimationPose - weighted blend accumulator.
 *
 * Blend-tree nodes and clip sampling contribute weighted values per target; the
 * pose combines them by value type:
 *   - continuous (Float/Vec/Color/Int)  : weighted average (sum of v*w over sum of w)
 *   - rotation   (Quat / rotation_3d)    : incremental weighted slerp
 *   - discrete   (Bool)                  : the contribution with the highest weight
 * Finalize() resolves these into absolute TargetValues; layer compositing and
 * ApplyTargetValues then combine layers and write them to the scene.
 */
class AnimationPose {
public:
    void AddValue(const std::string& node, const std::string& nodeUuid,
                  const std::string& channel,
                  TrackValueType type, bool useSlerp,
                  const nlohmann::json& value, float weight);

    bool Empty() const { return m_Targets.empty(); }
    void Clear() { m_Targets.clear(); }

    TargetValueMap Finalize() const;

private:
    struct Accumulator {
        bool inited = false;
        std::string node;
        std::string nodeUuid;
        std::string channel;
        TrackValueType type = TrackValueType::Float;
        bool useSlerp = false;

        // Continuous channels.
        std::vector<double> sum;
        double weight = 0.0;

        // Rotation channels.
        math::Quat quat;
        double quatWeight = 0.0;
        bool quatInit = false;

        // Discrete channels.
        nlohmann::json discreteValue;
        double discreteWeight = -1.0;
    };

    std::unordered_map<std::string, Accumulator> m_Targets;
};

// ---------------------------------------------------------------------------
// Compositing + application helpers
// ---------------------------------------------------------------------------

// Interpolate a channel value a -> b by t. Honours slerp for rotation channels
// and step semantics for discrete (Bool) channels.
nlohmann::json BlendChannelValues(TrackValueType type, bool useSlerp,
                                  const nlohmann::json& a, const nlohmann::json& b, float t);

// Additive composite: base + (current - reference) * scale, component-wise.
// (Generic property-level additive; quaternion-correct additive is part of the
// deferred skeletal phase.)
nlohmann::json AddChannelDelta(const nlohmann::json& base, const nlohmann::json& current,
                               const nlohmann::json& reference, float scale);

// Resolve each target relative to `root` (NodeRef path rules; "" / "." == root)
// and write its value through the channel system.
void ApplyTargetValues(core::Node* root, const TargetValueMap& values);

} // namespace animation
} // namespace lupine
