#include "lupine/components/NetworkTransform3D.hpp"
#include "lupine/components/NetworkObject.hpp"
#include "lupine/network/NetworkSerializer.hpp"
#include "lupine/network/NetworkManager.hpp"
#include "lupine/core/Node.hpp"
#include <cmath>

namespace lupine {
namespace components {

namespace {
constexpr float kChangeEpsilon = 0.0001f;
constexpr size_t kMaxBufferedSamples = 64;
constexpr double kMaxResyncDriftMs = 500.0;

float Nt3Distance(const math::Vec3& a, const math::Vec3& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

math::Vec3 Nt3Lerp(const math::Vec3& a, const math::Vec3& b, float t) {
    return math::Vec3{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
}

// Angular "distance" in [0,1]: 0 = identical orientation, 1 = opposite.
float Nt3QuatDistance(const math::Quat& a, const math::Quat& b) {
    return 1.0f - std::fabs(a.Dot(b));
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

NetworkTransform3D::NetworkTransform3D() : core::Component("NetworkTransform3D") {
    DefineProperties();
}

NetworkTransform3D::NetworkTransform3D(const std::string& name) : core::Component(name) {
    DefineProperties();
}

void NetworkTransform3D::DefineProperties() {
    if (m_PropertiesDefined) {
        return;
    }
    DefineProperty(PROPERTY_DEFAULT(syncPosition, Bool, true));
    DefineProperty(PROPERTY_DEFAULT(syncRotation, Bool, true));
    DefineProperty(PROPERTY_DEFAULT(syncScale, Bool, false));
    DefineProperty(PROPERTY_DEFAULT(snapThreshold, Float, 0.0f));
    // When on, position/scale ride as 16-bit half floats and the rotation
    // quaternion as a 4-byte smallest-three encoding (vs 16 bytes), roughly
    // halving each update. The receiver reads the format from a flag in the
    // update, so a sender can toggle it freely.
    DefineProperty(PROPERTY_DEFAULT(compress, Bool, false));
    m_PropertiesDefined = true;
}

bool NetworkTransform3D::IsLocalAuthority() const {
    core::Node* owner = GetOwner();
    if (owner == nullptr) {
        return true;
    }
    auto* netObject = dynamic_cast<NetworkObject*>(owner->GetComponent("NetworkObject").get());
    return netObject == nullptr ? true : netObject->IsAuthority();
}

bool NetworkTransform3D::NetGather(network::ByteWriter& writer, bool force) {
    auto* node = dynamic_cast<core::Node3D*>(GetOwner());
    if (node == nullptr) {
        return false;
    }
    const bool syncPos = GetPropertyValue<bool>("syncPosition");
    const bool syncRot = GetPropertyValue<bool>("syncRotation");
    const bool syncScl = GetPropertyValue<bool>("syncScale");

    const math::Vec3 pos = node->GetPosition();
    const math::Quat rot = node->GetRotation();
    const math::Vec3 scl = node->GetScale();

    // A forced (keyframe) gather always writes full state but leaves the change-
    // detection baseline untouched (pure reliable redundancy, no effect on the
    // delta stream to other peers).
    if (!force) {
        const bool changed = !m_HasLast ||
            (syncPos && Nt3Distance(pos, m_LastPosition) > kChangeEpsilon) ||
            (syncRot && Nt3QuatDistance(rot, m_LastRotation) > kChangeEpsilon) ||
            (syncScl && Nt3Distance(scl, m_LastScale) > kChangeEpsilon);
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
            writer.WriteHalf(pos.z);
        } else {
            writer.WriteFloat(pos.x);
            writer.WriteFloat(pos.y);
            writer.WriteFloat(pos.z);
        }
    }
    if (syncRot) {
        if (compress) {
            writer.WriteUnitQuat(rot.w(), rot.x(), rot.y(), rot.z());
        } else {
            writer.WriteFloat(rot.w());
            writer.WriteFloat(rot.x());
            writer.WriteFloat(rot.y());
            writer.WriteFloat(rot.z());
        }
    }
    if (syncScl) {
        if (compress) {
            writer.WriteHalf(scl.x);
            writer.WriteHalf(scl.y);
            writer.WriteHalf(scl.z);
        } else {
            writer.WriteFloat(scl.x);
            writer.WriteFloat(scl.y);
            writer.WriteFloat(scl.z);
        }
    }
    return true;
}

void NetworkTransform3D::NetApply(network::ByteReader& reader, uint32_t serverTimeMs) {
    // A locally-predicted object reconciles via its NetworkController, which
    // carries the authoritative transform; do not also buffer it for interpolation.
    if (ControllerPredictsLocally(GetOwner())) {
        return;
    }
    auto* node = dynamic_cast<core::Node3D*>(GetOwner());
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
        sample.position.z = compressed ? reader.ReadHalf() : reader.ReadFloat();
    }
    if (flags & 2u) {
        float w = 1.0f, x = 0.0f, y = 0.0f, z = 0.0f;
        if (compressed) {
            reader.ReadUnitQuat(w, x, y, z);
        } else {
            w = reader.ReadFloat();
            x = reader.ReadFloat();
            y = reader.ReadFloat();
            z = reader.ReadFloat();
        }
        sample.rotation = math::Quat(w, x, y, z);
    }
    if (flags & 4u) {
        sample.scale.x = compressed ? reader.ReadHalf() : reader.ReadFloat();
        sample.scale.y = compressed ? reader.ReadHalf() : reader.ReadFloat();
        sample.scale.z = compressed ? reader.ReadHalf() : reader.ReadFloat();
    }
    if (!reader.Ok()) {
        return;
    }

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

void NetworkTransform3D::OnUpdate(float deltaTime) {
    if (!network::NetworkManager::GetInstance().IsActive()) {
        return;
    }
    if (IsLocalAuthority() || m_Buffer.empty()) {
        return;
    }
    if (ControllerPredictsLocally(GetOwner())) {
        return;  // The NetworkController predicts/reconciles this object's motion.
    }
    auto* node = dynamic_cast<core::Node3D*>(GetOwner());
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
            m_RenderClockMs = latestMs;
        }
        if (m_RenderClockMs < targetMs - kMaxResyncDriftMs) {
            m_RenderClockMs = targetMs;
        }
    }

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
    if (snapThreshold > 0.0f && Nt3Distance(a.position, b.position) > snapThreshold) {
        t = 1.0f;
    }

    if (GetPropertyValue<bool>("syncPosition")) {
        node->SetPosition(Nt3Lerp(a.position, b.position, t));
    }
    if (GetPropertyValue<bool>("syncRotation")) {
        node->SetRotation(math::Slerp(a.rotation, b.rotation, t));
    }
    if (GetPropertyValue<bool>("syncScale")) {
        node->SetScale(Nt3Lerp(a.scale, b.scale, t));
    }

    while (m_Buffer.size() > 2 &&
           static_cast<double>(m_Buffer[1].timeMs) <= m_RenderClockMs) {
        m_Buffer.pop_front();
    }
}

} // namespace components
} // namespace lupine
