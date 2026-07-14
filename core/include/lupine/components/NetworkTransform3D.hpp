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
 * NetworkTransform3D - 3D analogue of NetworkTransform2D. Replicates a Node3D's
 * position/rotation(quaternion)/scale from its authority and plays the buffered
 * samples back behind a fixed interpolation delay (NetworkConfig::interpDelayMs),
 * slerping rotation along the shortest path. Inert when no session is active.
 */
class NetworkTransform3D : public core::Component, public network::INetworkSerializable {
public:
    NetworkTransform3D();
    explicit NetworkTransform3D(const std::string& name);

    std::string GetTypeName() const override { return "NetworkTransform3D"; }
    void DefineProperties() override;

    void OnUpdate(float deltaTime) override;

    bool NetGather(network::ByteWriter& writer, bool force) override;
    void NetApply(network::ByteReader& reader, uint32_t serverTimeMs) override;

private:
    struct Sample {
        uint32_t timeMs = 0;
        math::Vec3 position{0.0f, 0.0f, 0.0f};
        math::Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        math::Vec3 scale{1.0f, 1.0f, 1.0f};
    };

    bool IsLocalAuthority() const;

    math::Vec3 m_LastPosition{0.0f, 0.0f, 0.0f};
    math::Quat m_LastRotation{1.0f, 0.0f, 0.0f, 0.0f};
    math::Vec3 m_LastScale{1.0f, 1.0f, 1.0f};
    bool m_HasLast = false;

    // Receiver-side timestamped sample buffer + interpolation-delay playback clock.
    std::deque<Sample> m_Buffer;
    double m_RenderClockMs = 0.0;
    bool m_Interpolating = false;
};

} // namespace components
} // namespace lupine
