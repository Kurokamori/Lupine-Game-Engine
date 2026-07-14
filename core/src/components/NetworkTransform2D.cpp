#include "lupine/components/NetworkTransform2D.hpp"
#include "lupine/components/NetworkObject.hpp"
#include "lupine/network/NetworkSerializer.hpp"
#include "lupine/network/NetworkManager.hpp"
#include "lupine/core/Node.hpp"
#include <cmath>

namespace lupine {
namespace components {

namespace {
constexpr float kChangeEpsilon = 0.0001f;

// Hard cap on buffered samples so a stalled receiver cannot grow unbounded.
constexpr size_t kMaxBufferedSamples = 64;

// If the playback clock drifts further than this behind the ideal render time
// (a network hiccup, or first samples after a stall), snap it back instead of
// crawling to catch up.
constexpr double kMaxResyncDriftMs = 500.0;

float NtDistance(const math::Vec2& a, const math::Vec2& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

math::Vec2 NtLerp(const math::Vec2& a, const math::Vec2& b, float t) {
    return math::Vec2{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}

// True when a sibling NetworkController is locally predicting this object, in
// which case the controller owns the node transform and NetworkTransform stands
// aside (queried by name to avoid a hard dependency on the controller type).
bool ControllerPredictsLocally(core::Node* owner) {
    if (owner == nullptr) {
        return false;
    }
    std::shared_ptr<core::Component> ctrl = owner->GetComponent("NetworkController");
    if (!ctrl) {
        return false;
    }
    const nlohmann::json result = ctrl->CallMethod("is_predicting_locally", nlohmann::json::array());
    return result.is_boolean() && result.get<bool>();
}
}  // namespace

NetworkTransform2D::NetworkTransform2D() : core::Component("NetworkTransform2D") {
    DefineProperties();
}

NetworkTransform2D::NetworkTransform2D(const std::string& name) : core::Component(name) {
    DefineProperties();
}

void NetworkTransform2D::DefineProperties() {
    if (m_PropertiesDefined) {
        return;
    }
    DefineProperty(PROPERTY_DEFAULT(syncPosition, Bool, true));
    DefineProperty(PROPERTY_DEFAULT(syncRotation, Bool, true));
    DefineProperty(PROPERTY_DEFAULT(syncScale, Bool, false));
    DefineProperty(PROPERTY_DEFAULT(snapThreshold, Float, 0.0f));
    // When on, position/scale ride the wire as 16-bit half floats and rotation as
    // a 16-bit quantized angle - roughly halving the per-update size at the cost
    // of precision (good for sprites within a few thousand units). The receiver
    // reads the format from a flag in the update, so a sender can toggle it freely.
    DefineProperty(PROPERTY_DEFAULT(compress, Bool, false));
    m_PropertiesDefined = true;
}

bool NetworkTransform2D::IsLocalAuthority() const {
    core::Node* owner = GetOwner();
    if (owner == nullptr) {
        return true;
    }
    auto* netObject = dynamic_cast<NetworkObject*>(owner->GetComponent("NetworkObject").get());
    return netObject == nullptr ? true : netObject->IsAuthority();
}

bool NetworkTransform2D::NetGather(network::ByteWriter& writer, bool force) {
    auto* node = dynamic_cast<core::Node2D*>(GetOwner());
    if (node == nullptr) {
        return false;
    }
    const bool syncPos = GetPropertyValue<bool>("syncPosition");
    const bool syncRot = GetPropertyValue<bool>("syncRotation");
    const bool syncScl = GetPropertyValue<bool>("syncScale");

    const math::Vec2 pos = node->GetPosition();
    const float rot = node->GetRotation();
    const math::Vec2 scl = node->GetScale();

    // A forced (keyframe) gather always writes full state but leaves the change-
    // detection baseline untouched, so it has no effect on the delta stream sent
    // to other peers (a keyframe is pure reliable redundancy).
    if (!force) {
        const bool changed = !m_HasLast ||
            (syncPos && NtDistance(pos, m_LastPosition) > kChangeEpsilon) ||
            (syncRot && std::fabs(rot - m_LastRotation) > kChangeEpsilon) ||
            (syncScl && NtDistance(scl, m_LastScale) > kChangeEpsilon);
        if (!changed) {
            return false;
        }
        m_LastPosition = pos;
        m_LastRotation = rot;
        m_LastScale = scl;
        m_HasLast = true;
    }

    const bool compress = GetPropertyValue<bool>("compress");
    const uint8_t flags = (syncPos ? 1u : 0u) | (syncRot ? 2u : 0u) | (syncScl ? 4u : 0u) |
                          (compress ? 8u : 0u);
    writer.WriteU8(flags);
    if (syncPos) {
        if (compress) {
            writer.WriteHalf(pos.x);
            writer.WriteHalf(pos.y);
        } else {
            writer.WriteFloat(pos.x);
            writer.WriteFloat(pos.y);
        }
    }
    if (syncRot) {
        if (compress) {
            writer.WriteQuantizedAngle(rot, 16);
        } else {
            writer.WriteFloat(rot);
        }
    }
    if (syncScl) {
        if (compress) {
            writer.WriteHalf(scl.x);
            writer.WriteHalf(scl.y);
        } else {
            writer.WriteFloat(scl.x);
            writer.WriteFloat(scl.y);
        }
    }
    return true;
}

void NetworkTransform2D::NetApply(network::ByteReader& reader, uint32_t serverTimeMs) {
    // A locally-predicted object reconciles via its NetworkController, which
    // carries the authoritative transform; do not also buffer it for interpolation.
    if (ControllerPredictsLocally(GetOwner())) {
        return;
    }
    auto* node = dynamic_cast<core::Node2D*>(GetOwner());
    Sample sample;
    sample.timeMs = serverTimeMs;
    if (node != nullptr) {
        sample.position = node->GetPosition();
        sample.rotation = node->GetRotation();
        sample.scale = node->GetScale();
    }

    const uint8_t flags = reader.ReadU8();
    const bool compressed = (flags & 8u) != 0u;
    if (flags & 1u) {
        sample.position.x = compressed ? reader.ReadHalf() : reader.ReadFloat();
        sample.position.y = compressed ? reader.ReadHalf() : reader.ReadFloat();
    }
    if (flags & 2u) {
        sample.rotation = compressed ? reader.ReadQuantizedAngle(16) : reader.ReadFloat();
    }
    if (flags & 4u) {
        sample.scale.x = compressed ? reader.ReadHalf() : reader.ReadFloat();
        sample.scale.y = compressed ? reader.ReadHalf() : reader.ReadFloat();
    }
    if (!reader.Ok()) {
        return;
    }

    // Insert in timestamp order. Sequenced delivery normally appends, but a
    // reliable keyframe can arrive interleaved with the unreliable delta stream;
    // drop a sample older than the newest and replace one with an equal stamp.
    if (!m_Buffer.empty() && sample.timeMs < m_Buffer.back().timeMs) {
        return;
    }
    if (!m_Buffer.empty() && sample.timeMs == m_Buffer.back().timeMs) {
        m_Buffer.back() = sample;
    } else {
        m_Buffer.push_back(sample);
    }
    while (m_Buffer.size() > kMaxBufferedSamples) {
        m_Buffer.pop_front();
    }
}

void NetworkTransform2D::OnUpdate(float deltaTime) {
    if (!network::NetworkManager::GetInstance().IsActive()) {
        return;  // Single-player: transform is driven by gameplay directly.
    }
    if (IsLocalAuthority() || m_Buffer.empty()) {
        return;  // Authority drives its own transform; nothing to interpolate.
    }
    if (ControllerPredictsLocally(GetOwner())) {
        return;  // The NetworkController predicts/reconciles this object's motion.
    }
    auto* node = dynamic_cast<core::Node2D*>(GetOwner());
    if (node == nullptr) {
        return;
    }

    const double interpDelayMs =
        static_cast<double>(network::NetworkManager::GetInstance().GetConfig().interpDelayMs);
    const double latestMs = static_cast<double>(m_Buffer.back().timeMs);
    const double targetMs = latestMs - interpDelayMs;

    if (!m_Interpolating) {
        m_RenderClockMs = targetMs;
        m_Interpolating = true;
    } else {
        m_RenderClockMs += static_cast<double>(deltaTime) * 1000.0;
        if (m_RenderClockMs > latestMs) {
            m_RenderClockMs = latestMs;  // Buffer underran: hold at the newest sample.
        }
        if (m_RenderClockMs < targetMs - kMaxResyncDriftMs) {
            m_RenderClockMs = targetMs;  // Fell too far behind: resync to the delay.
        }
    }

    // Locate the two samples bracketing the render clock, then interpolate.
    Sample a = m_Buffer.front();
    Sample b = m_Buffer.back();
    if (m_RenderClockMs <= static_cast<double>(m_Buffer.front().timeMs)) {
        a = b = m_Buffer.front();
    } else if (m_RenderClockMs >= latestMs) {
        a = b = m_Buffer.back();
    } else {
        for (size_t i = 0; i + 1 < m_Buffer.size(); ++i) {
            if (static_cast<double>(m_Buffer[i].timeMs) <= m_RenderClockMs &&
                m_RenderClockMs <= static_cast<double>(m_Buffer[i + 1].timeMs)) {
                a = m_Buffer[i];
                b = m_Buffer[i + 1];
                break;
            }
        }
    }

    float t = 0.0f;
    if (b.timeMs > a.timeMs) {
        t = static_cast<float>((m_RenderClockMs - static_cast<double>(a.timeMs)) /
                               static_cast<double>(b.timeMs - a.timeMs));
        if (t < 0.0f) {
            t = 0.0f;
        } else if (t > 1.0f) {
            t = 1.0f;
        }
    }

    const float snapThreshold = GetPropertyValue<float>("snapThreshold");
    if (snapThreshold > 0.0f && NtDistance(a.position, b.position) > snapThreshold) {
        t = 1.0f;  // Teleport across large jumps instead of sliding.
    }

    if (GetPropertyValue<bool>("syncPosition")) {
        node->SetPosition(NtLerp(a.position, b.position, t));
    }
    if (GetPropertyValue<bool>("syncRotation")) {
        node->SetRotation(a.rotation + (b.rotation - a.rotation) * t);
    }
    if (GetPropertyValue<bool>("syncScale")) {
        node->SetScale(NtLerp(a.scale, b.scale, t));
    }

    // Drop samples fully behind the render clock, keeping the one bracketing it.
    while (m_Buffer.size() > 2 &&
           static_cast<double>(m_Buffer[1].timeMs) <= m_RenderClockMs) {
        m_Buffer.pop_front();
    }
}

} // namespace components
} // namespace lupine
