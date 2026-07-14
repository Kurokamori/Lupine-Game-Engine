#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/network/INetworkSerializable.hpp"
#include "lupine/math/Vec2.hpp"
#include <cstdint>
#include <deque>
#include <string>

namespace lupine {
namespace components {

/**
 * NetworkTransform2D - replicates a Node2D's transform from its authority to the
 * other peers, with snapshot interpolation for smooth motion on receivers.
 *
 * The authority gathers position/rotation/scale (only the channels enabled, and
 * only when they actually changed - or unconditionally for a keyframe). Receivers
 * buffer timestamped samples and play them back behind a fixed interpolation
 * delay (NetworkConfig::interpDelayMs), so jitter and small packet-timing
 * variations are absorbed and motion stays smooth. A snap threshold teleports
 * instead of interpolating across large jumps. Entirely inert when no session is
 * active.
 */
class NetworkTransform2D : public core::Component, public network::INetworkSerializable {
public:
    NetworkTransform2D();
    explicit NetworkTransform2D(const std::string& name);

    std::string GetTypeName() const override { return "NetworkTransform2D"; }
    void DefineProperties() override;

    void OnUpdate(float deltaTime) override;

    bool NetGather(network::ByteWriter& writer, bool force) override;
    void NetApply(network::ByteReader& reader, uint32_t serverTimeMs) override;

private:
    struct Sample {
        uint32_t timeMs = 0;
        math::Vec2 position{0.0f, 0.0f};
        float rotation = 0.0f;
        math::Vec2 scale{1.0f, 1.0f};
    };

    bool IsLocalAuthority() const;

    // Authority-side last-sent transform (change detection).
    math::Vec2 m_LastPosition{0.0f, 0.0f};
    float m_LastRotation = 0.0f;
    math::Vec2 m_LastScale{1.0f, 1.0f};
    bool m_HasLast = false;

    // Receiver-side timestamped sample buffer (ascending by timeMs) plus the local
    // playback clock that lags the newest sample by the interpolation delay.
    std::deque<Sample> m_Buffer;
    double m_RenderClockMs = 0.0;
    bool m_Interpolating = false;
};

} // namespace components
} // namespace lupine
