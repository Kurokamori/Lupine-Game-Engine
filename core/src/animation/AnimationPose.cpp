#include "lupine/animation/AnimationPose.hpp"
#include "lupine/animation/AnimationInterp.hpp"
#include "lupine/animation/AnimationChannel.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/scripting/NodeRef.hpp"
#include <cmath>

namespace lupine {
namespace animation {

namespace {

constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
constexpr float kRadToDeg = 180.0f / 3.14159265358979323846f;

math::Quat EulerDegToQuat(const nlohmann::json& value) {
    math::Vec3 eulerRad(
        JsonComponent(value, 0) * kDegToRad,
        JsonComponent(value, 1) * kDegToRad,
        JsonComponent(value, 2) * kDegToRad);
    return math::Quat::FromEuler(eulerRad);
}

nlohmann::json QuatToEulerDeg(const math::Quat& q) {
    math::Vec3 euler = q.Normalized().ToEuler();
    return nlohmann::json::array({euler.x * kRadToDeg, euler.y * kRadToDeg, euler.z * kRadToDeg});
}

math::Quat NegateQuat(const math::Quat& q) {
    return math::Quat(-q.w(), -q.x(), -q.y(), -q.z());
}

} // namespace

void AnimationPose::AddValue(const std::string& node, const std::string& nodeUuid,
                             const std::string& channel,
                             TrackValueType type, bool useSlerp,
                             const nlohmann::json& value, float weight) {
    if (weight <= 0.0f || value.is_null()) {
        return;
    }

    const std::string key = node + std::string("\x1F") + channel;
    Accumulator& acc = m_Targets[key];
    if (!acc.inited) {
        acc.inited = true;
        acc.node = node;
        acc.nodeUuid = nodeUuid;
        acc.channel = channel;
        acc.type = type;
        acc.useSlerp = useSlerp;
    }

    if (acc.type == TrackValueType::Quat) {
        math::Quat q = EulerDegToQuat(value);
        if (!acc.quatInit) {
            acc.quat = q;
            acc.quatWeight = weight;
            acc.quatInit = true;
        } else {
            if (acc.quat.Dot(q) < 0.0f) {
                q = NegateQuat(q);
            }
            float t = weight / static_cast<float>(acc.quatWeight + weight);
            acc.quat = acc.quat.Slerp(q, t);
            acc.quatWeight += weight;
        }
        return;
    }

    if (acc.type == TrackValueType::Bool || acc.type == TrackValueType::String) {
        if (static_cast<double>(weight) > acc.discreteWeight) {
            acc.discreteValue = value;
            acc.discreteWeight = static_cast<double>(weight);
        }
        return;
    }

    const int n = ValueComponentCount(acc.type);
    if (static_cast<int>(acc.sum.size()) < n) {
        acc.sum.resize(n, 0.0);
    }
    for (int i = 0; i < n; ++i) {
        acc.sum[i] += static_cast<double>(JsonComponent(value, static_cast<size_t>(i))) * static_cast<double>(weight);
    }
    acc.weight += static_cast<double>(weight);
}

TargetValueMap AnimationPose::Finalize() const {
    TargetValueMap out;
    for (const auto& entry : m_Targets) {
        const Accumulator& acc = entry.second;
        if (!acc.inited) continue;

        TargetValue tv;
        tv.node = acc.node;
        tv.nodeUuid = acc.nodeUuid;
        tv.channel = acc.channel;
        tv.valueType = acc.type;
        tv.useSlerp = acc.useSlerp;

        if (acc.type == TrackValueType::Quat) {
            if (!acc.quatInit) continue;
            tv.value = QuatToEulerDeg(acc.quat);
        } else if (acc.type == TrackValueType::Bool || acc.type == TrackValueType::String) {
            if (acc.discreteWeight < 0.0) continue;
            tv.value = acc.discreteValue;
        } else {
            if (acc.weight <= 0.0) continue;
            const int n = ValueComponentCount(acc.type);
            if (n <= 1) {
                double v = acc.sum.empty() ? 0.0 : acc.sum[0] / acc.weight;
                if (acc.type == TrackValueType::Int) {
                    tv.value = static_cast<long long>(std::llround(v));
                } else {
                    tv.value = v;
                }
            } else {
                nlohmann::json arr = nlohmann::json::array();
                for (int i = 0; i < n; ++i) {
                    arr.push_back(acc.sum[i] / acc.weight);
                }
                tv.value = arr;
            }
        }

        out[entry.first] = tv;
    }
    return out;
}

// ---------------------------------------------------------------------------

nlohmann::json BlendChannelValues(TrackValueType type, bool useSlerp,
                                  const nlohmann::json& a, const nlohmann::json& b, float t) {
    if (type == TrackValueType::Bool || type == TrackValueType::String) {
        return t >= 0.5f ? b : a;
    }
    if (useSlerp || type == TrackValueType::Quat) {
        math::Quat qa = EulerDegToQuat(a);
        math::Quat qb = EulerDegToQuat(b);
        if (qa.Dot(qb) < 0.0f) {
            qb = NegateQuat(qb);
        }
        return QuatToEulerDeg(qa.Slerp(qb, t));
    }
    nlohmann::json result = LerpJson(a, b, t);
    if (type == TrackValueType::Int && result.is_number()) {
        result = static_cast<long long>(std::llround(result.get<double>()));
    }
    return result;
}

nlohmann::json AddChannelDelta(const nlohmann::json& base, const nlohmann::json& current,
                               const nlohmann::json& reference, float scale) {
    if (base.is_array()) {
        nlohmann::json out = nlohmann::json::array();
        size_t n = base.size();
        for (size_t i = 0; i < n; ++i) {
            double b = JsonComponent(base, i);
            double c = JsonComponent(current, i);
            double r = JsonComponent(reference, i);
            out.push_back(b + (c - r) * static_cast<double>(scale));
        }
        return out;
    }
    if (base.is_number()) {
        double b = base.get<double>();
        double c = current.is_number() ? current.get<double>() : 0.0;
        double r = reference.is_number() ? reference.get<double>() : 0.0;
        return b + (c - r) * static_cast<double>(scale);
    }
    return base;
}

void ApplyTargetValues(core::Node* root, const TargetValueMap& values) {
    if (!root) return;

    for (const auto& entry : values) {
        const TargetValue& tv = entry.second;
        scripting::NodeRef target = ResolveTargetNode(root, tv.node, tv.nodeUuid);
        if (target.IsValid()) {
            WriteChannel(target, tv.channel, tv.value, tv.valueType);
        }
    }
}

} // namespace animation
} // namespace lupine
