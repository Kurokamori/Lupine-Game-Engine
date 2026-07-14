#include "lupine/animation/ClipSampler.hpp"
#include "lupine/animation/AnimationInterp.hpp"

namespace lupine {
namespace animation {

namespace {

// Cubic-Hermite a keyframe segment component by component.
nlohmann::json SampleCubic(const Track& track, const Keyframe& k0, const Keyframe& k1, float localT) {
    const int n = ValueComponentCount(track.valueType);
    const float seg = k1.time - k0.time;

    auto tangent = [](const std::vector<float>& tans, int i) -> float {
        return i < static_cast<int>(tans.size()) ? tans[i] : 0.0f;
    };

    if (n <= 1) {
        float v = CubicHermite(JsonComponent(k0.value, 0), JsonComponent(k1.value, 0),
                               tangent(k0.outTangent, 0), tangent(k1.inTangent, 0), seg, localT);
        return v;
    }

    nlohmann::json arr = nlohmann::json::array();
    for (int i = 0; i < n; ++i) {
        float v = CubicHermite(JsonComponent(k0.value, static_cast<size_t>(i)),
                               JsonComponent(k1.value, static_cast<size_t>(i)),
                               tangent(k0.outTangent, i), tangent(k1.inTangent, i), seg, localT);
        arr.push_back(v);
    }
    return arr;
}

} // namespace

nlohmann::json ClipSampler::SampleTrack(const Track& track, float time) {
    const std::vector<Keyframe>& keys = track.keys;
    if (keys.empty()) {
        return nlohmann::json();
    }
    if (keys.size() == 1 || time <= keys.front().time) {
        return keys.front().value;
    }
    if (time >= keys.back().time) {
        return keys.back().value;
    }

    // Locate the segment [k0, k1] with k0.time <= time < k1.time.
    size_t hi = 0;
    while (hi < keys.size() && keys[hi].time <= time) {
        ++hi;
    }
    if (hi == 0) return keys.front().value;
    if (hi >= keys.size()) return keys.back().value;

    const Keyframe& k0 = keys[hi - 1];
    const Keyframe& k1 = keys[hi];
    const float seg = k1.time - k0.time;
    const float localT = seg > 0.0f ? (time - k0.time) / seg : 0.0f;

    const bool slerp = track.useSlerp || track.valueType == TrackValueType::Quat;

    if (track.interp == InterpolationMode::Constant) {
        return k0.value;
    }
    if (track.interp == InterpolationMode::Cubic && !slerp &&
        track.valueType != TrackValueType::Bool && track.valueType != TrackValueType::String) {
        return SampleCubic(track, k0, k1, localT);
    }
    // Linear (and the slerp / discrete fallbacks for Cubic).
    return BlendChannelValues(track.valueType, slerp, k0.value, k1.value, localT);
}

void ClipSampler::Sample(const AnimationClip& clip, float time, float weight, AnimationPose& pose) {
    if (weight <= 0.0f) {
        return;
    }
    for (const Track& track : clip.tracks) {
        if (track.type != TrackType::Value || track.keys.empty()) {
            continue;
        }
        nlohmann::json value = SampleTrack(track, time);
        if (!value.is_null()) {
            pose.AddValue(track.node, track.nodeUuid, track.channel, track.valueType, track.useSlerp, value, weight);
        }
    }
}

void ClipSampler::CollectMethodCalls(const AnimationClip& clip, float fromTime, float toTime,
                                     std::vector<MethodCall>& out) {
    if (toTime < fromTime) {
        return;
    }
    for (const Track& track : clip.tracks) {
        if (track.type != TrackType::Method) {
            continue;
        }
        for (const Keyframe& key : track.keys) {
            if (key.time > fromTime && key.time <= toTime) {
                MethodCall call;
                call.node = track.node;
                call.nodeUuid = track.nodeUuid;
                call.method = key.method;
                call.args = key.args;
                out.push_back(call);
            }
        }
    }
}

} // namespace animation
} // namespace lupine
