#pragma once

#include "lupine/animation/AnimationData.hpp"
#include "lupine/animation/AnimationPose.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace lupine {
namespace animation {

/**
 * A method-track invocation discovered while advancing a clip's playhead.
 */
struct MethodCall {
    std::string node;
    std::string nodeUuid;
    std::string method;
    nlohmann::json args;
};

/**
 * ClipSampler - evaluates an AnimationClip's tracks.
 *
 * Sampling reads each value track at an absolute clip time, interpolating per the
 * track's mode (Constant/Linear/Cubic, with rotation channels slerped), and feeds
 * the result into an AnimationPose at a given weight. Method tracks are gathered
 * separately by scanning the playhead window the player advanced this frame.
 */
class ClipSampler {
public:
    static void Sample(const AnimationClip& clip, float time, float weight, AnimationPose& pose);

    // Returns null if the track has no keys.
    static nlohmann::json SampleTrack(const Track& track, float time);

    // Append method-track calls whose key time lies in (fromTime, toTime].
    static void CollectMethodCalls(const AnimationClip& clip, float fromTime, float toTime,
                                   std::vector<MethodCall>& out);
};

} // namespace animation
} // namespace lupine
