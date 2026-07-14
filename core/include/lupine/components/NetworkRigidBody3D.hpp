#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/network/INetworkSerializable.hpp"
#include "lupine/math/Vec3.hpp"
#include "lupine/math/Quat.hpp"
#include <cstdint>
#include <deque>
#include <string>

namespace lupine {
namespace components {

/**
 * NetworkRigidBody3D - replicates a 3D physics body (a sibling RigidBody3DComponent)
 * from its authority to the other peers.
 *
 * The authority's body simulates normally; its position, rotation, and linear /
 * angular velocity are gathered and sent. On every other peer the body is switched
 * to a kinematic proxy (so the local solver never fights replication) and driven
 * from the received stream: position/rotation are interpolated behind the usual
 * delay for smoothness, and the replicated velocity is applied so the proxy still
 * pushes the receiver's own dynamic bodies realistically. The kinematic switch is
 * reverted if the object's authority later returns to this peer.
 *
 * Inert when no session is active (the body is locally simulated as normal).
 */
class NetworkRigidBody3D : public core::Component, public network::INetworkSerializable {
public:
    NetworkRigidBody3D();
    explicit NetworkRigidBody3D(const std::string& name);

    std::string GetTypeName() const override { return "NetworkRigidBody3D"; }
    void DefineProperties() override;

    void OnUpdate(float deltaTime) override;

    bool NetGather(network::ByteWriter& writer, bool force) override;
    void NetApply(network::ByteReader& reader, uint32_t serverTimeMs) override;

private:
    struct Sample {
        uint32_t timeMs = 0;
        math::Vec3 position{0.0f, 0.0f, 0.0f};
        math::Quat rotation;
        math::Vec3 linearVelocity{0.0f, 0.0f, 0.0f};
        math::Vec3 angularVelocity{0.0f, 0.0f, 0.0f};
    };

    bool IsLocalAuthority() const;
    class RigidBody3DComponent* GetBody() const;
    void EnsureKinematic();
    void RestoreBodyType();

    // Authority-side change-detection baselines.
    math::Vec3 m_LastPosition{0.0f, 0.0f, 0.0f};
    math::Quat m_LastRotation;
    math::Vec3 m_LastLinearVelocity{0.0f, 0.0f, 0.0f};
    math::Vec3 m_LastAngularVelocity{0.0f, 0.0f, 0.0f};
    bool m_HasLast = false;

    // Receiver-side interpolation buffer + clock.
    std::deque<Sample> m_Buffer;
    double m_RenderClockMs = 0.0;
    bool m_Interpolating = false;

    bool m_KinematicApplied = false;
    int m_OriginalBodyType = -1;
};

} // namespace components
} // namespace lupine
