#pragma once

#include "lupine/animation/AnimationData.hpp"
#include "lupine/animation/AnimationPose.hpp"
#include <functional>
#include <vector>
#include <utility>

namespace lupine {
namespace animation {

/**
 * Context for evaluating a blend tree: the live parameter values plus a resolver
 * that turns a clip reference (name from the linked AnimationPlayer, or a direct
 * res:// path) into an AnimationClip to sample.
 */
struct BlendContext {
    const AnimationParameters* params = nullptr;
    std::function<const AnimationClip*(const std::string& clip, const std::string& path)> resolveClip;
};

/**
 * BlendTree - evaluates a blend-node tree into a resolved pose.
 *
 * Leaf clips are sampled at a shared normalized phase so blended motions stay in
 * sync (Unity behavior). Child maps are combined through the AnimationPose
 * accumulator, so continuous channels average, rotations slerp, and discrete
 * channels take the highest weight. Supports Blend1D, Blend2D (2D gradient-band
 * weighting), Direct, Blend2, Add2 (additive), OneShot, and TimeScale.
 *
 * Note: the four Blend2D modes are accepted and stored; the v1 runtime evaluates
 * all of them with the Cartesian gradient-band metric (the polar/directional
 * metric is a documented refinement). Blends remain smooth in every mode.
 */
class BlendTree {
public:
    // Resolved pose for `node` at normalized phase [0,1] (as if at full weight).
    static TargetValueMap Evaluate(const BlendNode& node, const BlendContext& ctx, float phase);

    // Weighted (blended) clip duration in seconds, used to advance the phase so a
    // blend plays at the blended speed.
    static float EffectiveDuration(const BlendNode& node, const BlendContext& ctx);

    // Blend several resolved maps by weight (weighted-average / slerp / discrete).
    static TargetValueMap BlendMaps(const std::vector<std::pair<TargetValueMap, float>>& maps);

private:
    // Normalized child weights (sum to 1) for Blend1D/Blend2D/Direct/Blend2.
    static std::vector<float> ChildWeights(const BlendNode& node, const BlendContext& ctx);
};

} // namespace animation
} // namespace lupine
