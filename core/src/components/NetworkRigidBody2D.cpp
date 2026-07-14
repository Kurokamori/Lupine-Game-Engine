#include "lupine/components/NetworkRigidBody2D.hpp"
#include "lupine/components/NetworkObject.hpp"
#include "lupine/components/RigidBody2DComponent.hpp"
#include "lupine/network/NetworkSerializer.hpp"
#include "lupine/network/NetworkManager.hpp"
#include "lupine/physics2d/RigidBody2D.hpp"
#include "lupine/physics2d/Physics2DWorld.hpp"
#include "lupine/core/Node.hpp"
#include <cmath>

namespace lupine {
namespace components {

namespace {
constexpr float kChangeEpsilon = 0.0001f;
constexpr size_t kMaxBufferedSamples = 64;
constexpr double kMaxResyncDriftMs = 500.0;

float NrbDistance(const math::Vec2& a, const math::Vec2& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

math::Vec2 NrbLerp(const math::Vec2& a, const math::Vec2& b, float t) {
    return math::Vec2{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}
}  // namespace

NetworkRigidBody2D::NetworkRigidBody2D() : core::Component("NetworkRigidBody2D") {
    DefineProperties();
}

NetworkRigidBody2D::NetworkRigidBody2D(const std::string& name) : core::Component(name) {
    DefineProperties();
}

void NetworkRigidBody2D::DefineProperties() {
    if (m_PropertiesDefined) {
        return;
    }
    DefineProperty(PROPERTY_DEFAULT(syncVelocity, Bool, true));
    DefineProperty(PROPERTY_DEFAULT(snapThreshold, Float, 0.0f));
    DefineProperty(PROPERTY_DEFAULT(compress, Bool, false));
    // When on (default), the body becomes a kinematic proxy on non-authority peers
    // so the local solver cannot fight the replicated transform. Turn off only if
    // a game wants the remote body to keep simulating locally between updates.
    DefineProperty(PROPERTY_DEFAULT(makeKinematicOnRemote, Bool, true));
    m_PropertiesDefined = true;
}

bool NetworkRigidBody2D::IsLocalAuthority() const {
    core::Node* owner = GetOwner();
    if (owner == nullptr) {
        return true;
    }
    auto* netObject = dynamic_cast<NetworkObject*>(owner->GetComponent("NetworkObject").get());
    return netObject == nullptr ? true : netObject->IsAuthority();
}

RigidBody2DComponent* NetworkRigidBody2D::GetBody() const {
    core::Node* owner = GetOwner();
    if (owner == nullptr) {
        return nullptr;
    }
    return dynamic_cast<RigidBody2DComponent*>(owner->GetComponent("RigidBody2DComponent").get());
}

void NetworkRigidBody2D::EnsureKinematic() {
    if (m_KinematicApplied || !GetPropertyValue<bool>("makeKinematicOnRemote")) {
        return;
    }
    RigidBody2DComponent* rb = GetBody();
    if (rb == nullptr) {
        return;
    }
    physics2d::RigidBody2D* body = rb->GetPhysicsBody();
    if (body == nullptr) {
        return;  // Body not created yet; retry next frame.
    }
    m_OriginalBodyType = static_cast<int>(body->GetBodyType());
    body->SetBodyType(physics2d::BodyType::Kinematic);
    m_KinematicApplied = true;
}

void NetworkRigidBody2D::RestoreBodyType() {
    if (!m_KinematicApplied) {
        return;
    }
    RigidBody2DComponent* rb = GetBody();
    if (rb != nullptr) {
        if (physics2d::RigidBody2D* body = rb->GetPhysicsBody()) {
            body->SetBodyType(static_cast<physics2d::BodyType>(m_OriginalBodyType));
        }
    }
    m_KinematicApplied = false;
    m_OriginalBodyType = -1;
}

bool NetworkRigidBody2D::NetGather(network::ByteWriter& writer, bool force) {
    auto* node = dynamic_cast<core::Node2D*>(GetOwner());
    RigidBody2DComponent* rb = GetBody();
    if (node == nullptr || rb == nullptr) {
        return false;
    }
    const bool syncVel = GetPropertyValue<bool>("syncVelocity");

    const math::Vec2 pos = node->GetPosition();
    const float rot = node->GetRotation();
    const math::Vec2 linVel = rb->GetLinearVelocity();
    const float angVel = rb->GetAngularVelocity();

    if (!force) {
        const bool changed = !m_HasLast ||
            NrbDistance(pos, m_LastPosition) > kChangeEpsilon ||
            std::fabs(rot - m_LastRotation) > kChangeEpsilon ||
            (syncVel && (NrbDistance(linVel, m_LastLinearVelocity) > kChangeEpsilon ||
                         std::fabs(angVel - m_LastAngularVelocity) > kChangeEpsilon));
        if (!changed) {
            return false;
        }
        m_LastPosition = pos;
        m_LastRotation = rot;
        m_LastLinearVelocity = linVel;
        m_LastAngularVelocity = angVel;
        m_HasLast = true;
    }

    const bool compress = GetPropertyValue<bool>("compress");
    const uint8_t flags = (compress ? 1u : 0u) | (syncVel ? 2u : 0u);
    writer.WriteU8(flags);
    if (compress) {
        writer.WriteHalf(pos.x);
        writer.WriteHalf(pos.y);
        writer.WriteQuantizedAngle(rot, 16);
    } else {
        writer.WriteFloat(pos.x);
        writer.WriteFloat(pos.y);
        writer.WriteFloat(rot);
    }
    if (syncVel) {
        if (compress) {
            writer.WriteHalf(linVel.x);
            writer.WriteHalf(linVel.y);
            writer.WriteHalf(angVel);
        } else {
            writer.WriteFloat(linVel.x);
            writer.WriteFloat(linVel.y);
            writer.WriteFloat(angVel);
        }
    }
    return true;
}

void NetworkRigidBody2D::NetApply(network::ByteReader& reader, uint32_t serverTimeMs) {
    Sample sample;
    sample.timeMs = serverTimeMs;

    const uint8_t flags = reader.ReadU8();
    const bool compressed = (flags & 1u) != 0u;
    const bool hasVel = (flags & 2u) != 0u;
    if (compressed) {
        sample.position.x = reader.ReadHalf();
        sample.position.y = reader.ReadHalf();
        sample.rotation = reader.ReadQuantizedAngle(16);
    } else {
        sample.position.x = reader.ReadFloat();
        sample.position.y = reader.ReadFloat();
        sample.rotation = reader.ReadFloat();
    }
    if (hasVel) {
        if (compressed) {
            sample.linearVelocity.x = reader.ReadHalf();
            sample.linearVelocity.y = reader.ReadHalf();
            sample.angularVelocity = reader.ReadHalf();
        } else {
            sample.linearVelocity.x = reader.ReadFloat();
            sample.linearVelocity.y = reader.ReadFloat();
            sample.angularVelocity = reader.ReadFloat();
        }
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

void NetworkRigidBody2D::OnUpdate(float deltaTime) {
    if (!network::NetworkManager::GetInstance().IsActive()) {
        return;
    }
    if (IsLocalAuthority()) {
        RestoreBodyType();  // Authority simulates locally; undo any proxy switch.
        m_Buffer.clear();
        m_Interpolating = false;
        return;
    }
    if (m_Buffer.empty()) {
        return;
    }

    RigidBody2DComponent* rb = GetBody();
    if (rb == nullptr) {
        return;
    }
    EnsureKinematic();
    physics2d::RigidBody2D* body = rb->GetPhysicsBody();
    if (body == nullptr) {
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
    if (snapThreshold > 0.0f && NrbDistance(a.position, b.position) > snapThreshold) {
        t = 1.0f;
    }

    const math::Vec2 pos = NrbLerp(a.position, b.position, t);
    const float rot = a.rotation + (b.rotation - a.rotation) * t;
    body->SetTransform(pos, rot);

    if (GetPropertyValue<bool>("syncVelocity")) {
        body->SetLinearVelocity(NrbLerp(a.linearVelocity, b.linearVelocity, t));
        body->SetAngularVelocity(a.angularVelocity + (b.angularVelocity - a.angularVelocity) * t);
    }

    while (m_Buffer.size() > 2 &&
           static_cast<double>(m_Buffer[1].timeMs) <= m_RenderClockMs) {
        m_Buffer.pop_front();
    }
}

} // namespace components
} // namespace lupine
