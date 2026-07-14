#include "lupine/animation/BlendTree.hpp"
#include "lupine/animation/ClipSampler.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace lupine {
namespace animation {

namespace {

constexpr float kEpsilon = 1e-5f;

float ParamFloat(const BlendContext& ctx, const std::string& name) {
    return ctx.params ? ctx.params->GetFloat(name) : 0.0f;
}

bool ParamGate(const BlendContext& ctx, const std::string& name) {
    if (!ctx.params) return false;
    return ctx.params->GetBool(name) || ctx.params->IsTriggerSet(name);
}

const AnimationClip* ResolveClip(const BlendContext& ctx, const BlendNode& node) {
    if (!ctx.resolveClip) return nullptr;
    return ctx.resolveClip(node.clip, node.clipPath);
}

// Cartesian gradient-band weighting (Johansen). Smooth across an arbitrary 2D
// scatter of sample points; degrades to nearest-point when the band collapses.
std::vector<float> GradientBand2D(const BlendNode& node, float px, float py) {
    const size_t n = node.children.size();
    std::vector<float> weights(n, 0.0f);
    if (n == 0) return weights;
    if (n == 1) { weights[0] = 1.0f; return weights; }

    float total = 0.0f;
    for (size_t i = 0; i < n; ++i) {
        float pix = node.children[i].posX;
        float piy = node.children[i].posY;
        float hi = std::numeric_limits<float>::max();
        for (size_t j = 0; j < n; ++j) {
            if (j == i) continue;
            float ijx = node.children[j].posX - pix;
            float ijy = node.children[j].posY - piy;
            float ipx = px - pix;
            float ipy = py - piy;
            float denom = ijx * ijx + ijy * ijy;
            float val = denom > kEpsilon ? 1.0f - (ipx * ijx + ipy * ijy) / denom : 1.0f;
            hi = std::min(hi, val);
        }
        weights[i] = std::max(0.0f, hi);
        total += weights[i];
    }

    if (total <= kEpsilon) {
        // Fall back to the nearest point.
        size_t nearest = 0;
        float best = std::numeric_limits<float>::max();
        for (size_t i = 0; i < n; ++i) {
            float dx = px - node.children[i].posX;
            float dy = py - node.children[i].posY;
            float d = dx * dx + dy * dy;
            if (d < best) { best = d; nearest = i; }
        }
        std::fill(weights.begin(), weights.end(), 0.0f);
        weights[nearest] = 1.0f;
        return weights;
    }

    for (float& w : weights) w /= total;
    return weights;
}

std::vector<float> Weights1D(const BlendNode& node, float p) {
    const size_t n = node.children.size();
    std::vector<float> weights(n, 0.0f);
    if (n == 0) return weights;
    if (n == 1) { weights[0] = 1.0f; return weights; }

    std::vector<size_t> order(n);
    for (size_t i = 0; i < n; ++i) order[i] = i;
    std::stable_sort(order.begin(), order.end(),
        [&](size_t a, size_t b) { return node.children[a].threshold < node.children[b].threshold; });

    float firstThr = node.children[order.front()].threshold;
    float lastThr = node.children[order.back()].threshold;
    if (p <= firstThr) { weights[order.front()] = 1.0f; return weights; }
    if (p >= lastThr) { weights[order.back()] = 1.0f; return weights; }

    for (size_t k = 0; k + 1 < n; ++k) {
        float t0 = node.children[order[k]].threshold;
        float t1 = node.children[order[k + 1]].threshold;
        if (p >= t0 && p <= t1) {
            float span = t1 - t0;
            float t = span > kEpsilon ? (p - t0) / span : 0.0f;
            weights[order[k]] = 1.0f - t;
            weights[order[k + 1]] = t;
            return weights;
        }
    }
    return weights;
}

} // namespace

std::vector<float> BlendTree::ChildWeights(const BlendNode& node, const BlendContext& ctx) {
    const size_t n = node.children.size();
    switch (node.type) {
        case BlendNodeType::Blend1D:
            return Weights1D(node, ParamFloat(ctx, node.parameterX));
        case BlendNodeType::Blend2D:
            return GradientBand2D(node, ParamFloat(ctx, node.parameterX), ParamFloat(ctx, node.parameterY));
        case BlendNodeType::Direct: {
            std::vector<float> weights(n, 0.0f);
            float total = 0.0f;
            for (size_t i = 0; i < n; ++i) {
                weights[i] = std::max(0.0f, ParamFloat(ctx, node.children[i].parameter));
                total += weights[i];
            }
            if (total > kEpsilon) {
                for (float& w : weights) w /= total;
            }
            return weights;
        }
        case BlendNodeType::Blend2: {
            std::vector<float> weights(n, 0.0f);
            if (n >= 1) {
                float p = std::clamp(ParamFloat(ctx, node.parameterX), 0.0f, 1.0f);
                weights[0] = 1.0f - p;
                if (n >= 2) weights[1] = p;
            }
            return weights;
        }
        default:
            return std::vector<float>(n, 0.0f);
    }
}

TargetValueMap BlendTree::BlendMaps(const std::vector<std::pair<TargetValueMap, float>>& maps) {
    AnimationPose pose;
    for (const auto& entry : maps) {
        const float w = entry.second;
        if (w <= 0.0f) continue;
        for (const auto& kv : entry.first) {
            const TargetValue& tv = kv.second;
            pose.AddValue(tv.node, tv.nodeUuid, tv.channel, tv.valueType, tv.useSlerp, tv.value, w);
        }
    }
    return pose.Finalize();
}

TargetValueMap BlendTree::Evaluate(const BlendNode& node, const BlendContext& ctx, float phase) {
    switch (node.type) {
        case BlendNodeType::Clip: {
            const AnimationClip* clip = ResolveClip(ctx, node);
            if (!clip) return {};
            AnimationPose pose;
            ClipSampler::Sample(*clip, std::clamp(phase, 0.0f, 1.0f) * clip->EffectiveLength(), 1.0f, pose);
            return pose.Finalize();
        }
        case BlendNodeType::Blend1D:
        case BlendNodeType::Blend2D:
        case BlendNodeType::Direct:
        case BlendNodeType::Blend2: {
            std::vector<float> weights = ChildWeights(node, ctx);
            std::vector<std::pair<TargetValueMap, float>> maps;
            for (size_t i = 0; i < node.children.size(); ++i) {
                if (weights[i] <= kEpsilon || !node.children[i].node) continue;
                maps.emplace_back(Evaluate(*node.children[i].node, ctx, phase), weights[i]);
            }
            return BlendMaps(maps);
        }
        case BlendNodeType::Add2: {
            if (node.children.size() < 2 || !node.children[0].node || !node.children[1].node) {
                return node.children.empty() || !node.children[0].node
                    ? TargetValueMap{} : Evaluate(*node.children[0].node, ctx, phase);
            }
            TargetValueMap base = Evaluate(*node.children[0].node, ctx, phase);
            TargetValueMap add = Evaluate(*node.children[1].node, ctx, phase);
            TargetValueMap ref = Evaluate(*node.children[1].node, ctx, 0.0f);
            float scale = ParamFloat(ctx, node.parameterX);
            TargetValueMap result = base;
            for (const auto& kv : add) {
                const std::string& key = kv.first;
                const TargetValue& addVal = kv.second;
                auto rit = ref.find(key);
                nlohmann::json refValue = rit != ref.end() ? rit->second.value : addVal.value;
                auto bit = base.find(key);
                nlohmann::json baseValue = bit != base.end() ? bit->second.value : refValue;
                TargetValue out = addVal;
                out.value = AddChannelDelta(baseValue, addVal.value, refValue, scale);
                result[key] = out;
            }
            return result;
        }
        case BlendNodeType::OneShot: {
            if (node.children.empty() || !node.children[0].node) return {};
            TargetValueMap base = Evaluate(*node.children[0].node, ctx, phase);
            if (node.children.size() < 2 || !node.children[1].node) return base;
            float gate = ParamGate(ctx, node.parameter) ? 1.0f : 0.0f;
            if (gate <= kEpsilon) return base;
            TargetValueMap shot = Evaluate(*node.children[1].node, ctx, phase);
            return BlendMaps({ {base, 1.0f - gate}, {shot, gate} });
        }
        case BlendNodeType::TimeScale: {
            if (node.children.empty() || !node.children[0].node) return {};
            return Evaluate(*node.children[0].node, ctx, phase);
        }
    }
    return {};
}

float BlendTree::EffectiveDuration(const BlendNode& node, const BlendContext& ctx) {
    switch (node.type) {
        case BlendNodeType::Clip: {
            const AnimationClip* clip = ResolveClip(ctx, node);
            if (!clip) return 0.0f;
            float speed = std::fabs(node.speed) > kEpsilon ? std::fabs(node.speed) : 1.0f;
            return clip->EffectiveLength() / speed;
        }
        case BlendNodeType::Blend1D:
        case BlendNodeType::Blend2D:
        case BlendNodeType::Direct:
        case BlendNodeType::Blend2: {
            std::vector<float> weights = ChildWeights(node, ctx);
            float dur = 0.0f;
            float totalW = 0.0f;
            float anyDur = 0.0f;
            int anyCount = 0;
            for (size_t i = 0; i < node.children.size(); ++i) {
                if (!node.children[i].node) continue;
                float cd = EffectiveDuration(*node.children[i].node, ctx);
                anyDur += cd;
                ++anyCount;
                if (weights[i] > kEpsilon) {
                    dur += cd * weights[i];
                    totalW += weights[i];
                }
            }
            if (totalW > kEpsilon) return dur / totalW;
            return anyCount > 0 ? anyDur / static_cast<float>(anyCount) : 0.0f;
        }
        case BlendNodeType::Add2:
        case BlendNodeType::OneShot: {
            if (!node.children.empty() && node.children[0].node) {
                return EffectiveDuration(*node.children[0].node, ctx);
            }
            return 0.0f;
        }
        case BlendNodeType::TimeScale: {
            if (node.children.empty() || !node.children[0].node) return 0.0f;
            float base = EffectiveDuration(*node.children[0].node, ctx);
            float scale = node.parameterX.empty() ? 1.0f : ParamFloat(ctx, node.parameterX);
            scale = std::fabs(scale) > kEpsilon ? std::fabs(scale) : 1.0f;
            return base / scale;
        }
    }
    return 0.0f;
}

} // namespace animation
} // namespace lupine
