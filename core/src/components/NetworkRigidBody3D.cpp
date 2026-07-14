#include "lupine/components/NetworkRigidBody3D.hpp"
#include "lupine/components/NetworkObject.hpp"
#include "lupine/components/RigidBody3DComponent.hpp"
#include "lupine/network/NetworkSerializer.hpp"
#include "lupine/network/NetworkManager.hpp"
#include "lupine/physics3d/RigidBody3D.hpp"
#include "lupine/physics3d/Physics3DWorld.hpp"
#include "lupine/core/Node.hpp"
#include <cmath>

namespace lupine {
namespace components {

namespace {
constexpr float kChangeEpsilon = 0.0001f;
constexpr size_t kMaxBufferedSamples = 64;
constexpr double kMaxResyncDriftMs = 500.0;

float Nrb3Distance(const math::Vec3& a, const math::Vec3& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

math::Vec3 Nrb3Lerp(const math::Vec3& a, const math::Vec3& b, float t) {
    return math::Vec3{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
}

float Nrb3QuatDistance(const math::Quat& a, const math::Quat& b) {
    return 1.0f - std::fabs(a.Dot(b));
}
}  // namespace

NetworkRigidBody3D::NetworkRigidBody3D() : core::Component("NetworkRigidBody3D") {
    DefineProperties();
}

NetworkRigidBody3D::NetworkRigidBody3D(const std::string& name) : core::Component(name) {
    DefineProperties();
}

void NetworkRigidBody3D::DefineProperties() {
    if (m_PropertiesDefined) {
        return;
    }
    DefineProperty(PROPERTY_DEFAULT(syncVelocity, Bool, true));
    DefineProperty(PROPERTY_DEFAULT(snapThreshold, Float, 0.0f));
    DefineProperty(PROPERTY_DEFAULT(compress, Bool, false));
    DefineProperty(PROPERTY_DEFAULT(makeKinematicOnRemote, Bool, true));
    m_PropertiesDefined = true;
}

bool NetworkRigidBody3D::IsLocalAuthority() const {
    core::Node* owner = GetOwner();
    if (owner == nullptr) {
        return true;
    }
    auto* netObject = dynamic_cast<NetworkObject*>(owner->GetComponent("NetworkObject").get());
    return netObject == nullptr ? true : netObject->IsAuthority();
}

RigidBody3DComponent* NetworkRigidBody3D::GetBody() const {
    core::Node* owner = GetOwner();
    if (owner == nullptr) {
        return nullptr;
    }
    return dynamic_cast<RigidBody3DComponent*>(owner->GetComponent("RigidBody3DComponent").get());
}

void NetworkRigidBody3D::EnsureKinematic() {
    if (m_KinematicApplied || !GetPropertyValue<bool>("makeKinematicOnRemote")) {
        return;
    }
    RigidBody3DComponent* rb = GetBody();
    if (rb == nullptr) {
        return;
    }
    physics3d::RigidBody3D* body = rb->GetPhysicsBody();
    if (body == nullptr) {
        return;
    }
    m_OriginalBodyType = static_cast<int>(body->GetBodyType());
    body->SetBodyType(physics3d::BodyType::Kinematic);
    m_KinematicApplied = true;
}

void NetworkRigidBody3D::RestoreBodyType() {
    if (!m_KinematicApplied) {
        return;
    }
    RigidBody3DComponent* rb = GetBody();
    if (rb != nullptr) {
        if (physics3d::RigidBody3D* body = rb->GetPhysicsBody()) {
            body->SetBodyType(static_cast<physics3d::BodyType>(m_OriginalBodyType));
        }
    }
    m_KinematicApplied = false;
    m_OriginalBodyType = -1;
}

bool NetworkRigidBody3D::NetGather(network::ByteWriter& writer, bool force) {
    auto* node = dynamic_cast<core::Node3D*>(GetOwner());
    RigidBody3DComponent* rb = GetBody();
    if (node == nullptr || rb == nullptr) {
        return false;
    }
    const bool syncVel = GetPropertyValue<bool>("syncVelocity");

    const math::Vec3 pos = node->GetPosition();
    const math::Quat rot = node->GetRotation();
    const math::Vec3 linVel = rb->GetLinearVelocity();
    const math::Vec3 angVel = rb->GetAngularVelocity();

    if (!force) {
        const bool changed = !m_HasLast ||
            Nrb3Distance(pos, m_LastPosition) > kChangeEpsilon ||
            Nrb3QuatDistance(rot, m_LastRotation) > kChangeEpsilon ||
            (syncVel && (Nrb3Distance(linVel, m_LastLinearVelocity) > kChangeEpsilon ||
                         Nrb3Distance(angVel, m_LastAngularVelocity) > kChangeEpsilon));
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
        writer.WriteHalf(pos.z);
        writer.WriteUnitQuat(rot.w(), rot.x(), rot.y(), rot.z());
    } else {
        writer.WriteFloat(pos.x);
        writer.WriteFloat(pos.y);
        writer.WriteFloat(pos.z);
        writer.WriteFloat(rot.w());
        writer.WriteFloat(rot.x());
        writer.WriteFloat(rot.y());
        writer.WriteFloat(rot.z());
    }
    if (syncVel) {
        if (compress) {
            writer.WriteHalf(linVel.x);
            writer.WriteHalf(linVel.y);
            writer.WriteHalf(linVel.z);
            writer.WriteHalf(angVel.x);
            writer.WriteHalf(angVel.y);
            writer.WriteHalf(angVel.z);
        } else {
            writer.WriteFloat(linVel.x);
            writer.WriteFloat(linVel.y);
            writer.WriteFloat(linVel.z);
            writer.WriteFloat(angVel.x);
            writer.WriteFloat(angVel.y);
            writer.WriteFloat(angVel.z);
        }
    }
    return true;
}

void NetworkRigidBody3D::NetApply(network::ByteReader& reader, uint32_t serverTimeMs) {
    Sample sample;
    sample.timeMs = serverTimeMs;

    const uint8_t flags = reader.ReadU8();
    const bool compressed = (flags & 1u) != 0u;
    const bool hasVel = (flags & 2u) != 0u;
    if (compressed) {
        sample.position.x = reader.ReadHalf();
        sample.position.y = reader.ReadHalf();
        sample.position.z = reader.ReadHalf();
        float w = 1.0f, x = 0.0f, y = 0.0f, z = 0.0f;
        reader.ReadUnitQuat(w, x, y, z);
        sample.rotation = math::Quat(w, x, y, z);
    } else {
        sample.position.x = reader.ReadFloat();
        sample.position.y = reader.ReadFloat();
        sample.position.z = reader.ReadFloat();
        const float w = reader.ReadFloat();
        const float x = reader.ReadFloat();
        const float y = reader.ReadFloat();
        const float z = reader.ReadFloat();
        sample.rotation = math::Quat(w, x, y, z);
    }
    if (hasVel) {
        if (compressed) {
            sample.linearVelocity.x = reader.ReadHalf();
            sample.linearVelocity.y = reader.ReadHalf();
            sample.linearVelocity.z = reader.ReadHalf();
            sample.angularVelocity.x = reader.ReadHalf();
            sample.angularVelocity.y = reader.ReadHalf();
            sample.angularVelocity.z = reader.ReadHalf();
        } else {
            sample.linearVelocity.x = reader.ReadFloat();
            sample.linearVelocity.y = reader.ReadFloat();
            sample.linearVelocity.z = reader.ReadFloat();
            sample.angularVelocity.x = reader.ReadFloat();
            sample.angularVelocity.y = reader.ReadFloat();
            sample.angularVelocity.z = reader.ReadFloat();
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

void NetworkRigidBody3D::OnUpdate(float deltaTime) {
    if (!network::NetworkManager::GetInstance().IsActive()) {
        return;
    }
    if (IsLocalAuthority()) {
        RestoreBodyType();
        m_Buffer.clear();
        m_Interpolating = false;
        return;
    }
    if (m_Buffer.empty()) {
        return;
    }

    RigidBody3DComponent* rb = GetBody();
    if (rb == nullptr) {
        return;
    }
    EnsureKinematic();
    physics3d::RigidBody3D* body = rb->GetPhysicsBody();
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
    if (snapThreshold > 0.0f && Nrb3Distance(a.position, b.position) > snapThreshold) {
        t = 1.0f;
    }

    const math::Vec3 pos = Nrb3Lerp(a.position, b.position, t);
    const math::Quat rot = math::Slerp(a.rotation, b.rotation, t);
    body->SetTransform(pos, rot);

    if (GetPropertyValue<bool>("syncVelocity")) {
        body->SetLinearVelocity(Nrb3Lerp(a.linearVelocity, b.linearVelocity, t));
        body->SetAngularVelocity(Nrb3Lerp(a.angularVelocity, b.angularVelocity, t));
    }

    while (m_Buffer.size() > 2 &&
           static_cast<double>(m_Buffer[1].timeMs) <= m_RenderClockMs) {
        m_Buffer.pop_front();
    }
}

} // namespace components
} // namespace lupine
