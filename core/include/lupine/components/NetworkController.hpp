#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/network/INetworkSerializable.hpp"
#include "lupine/network/NetworkTypes.hpp"
#include "lupine/math/Vec3.hpp"
#include "lupine/math/Quat.hpp"
#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>

namespace lupine {
namespace network {
class ByteReader;
}
namespace components {

class NetworkObject;

/**
 * NetworkController - client-side prediction & server reconciliation for a
 * locally-driven, server-authoritative object (typically the player).
 *
 * The object carries this alongside a NetworkObject (server-authoritative) and a
 * script component implementing the deterministic step
 *
 *     function on_network_simulate(input, dt)   -- moves `self` from `input`
 *
 * One peer is the object's *controller* (its input source), set on the authority
 * via set_controller(peer). Each fixed tick:
 *   - the controlling client samples its input into a sequenced command, applies
 *     it locally (prediction), and sends it (with redundancy) to the server;
 *   - the server applies each new command in order through on_network_simulate,
 *     advancing the authoritative state, and stamps the last processed input
 *     sequence into its snapshot;
 *   - the controlling client, on receiving that snapshot, rewinds to the
 *     authoritative state and replays its still-unacknowledged commands, so the
 *     predicted position converges on the server's without input lag.
 *
 * The host's own controlled object is simulated directly (it is already the
 * authority - zero latency, no prediction). Inert when no session is active.
 */
class NetworkController : public core::Component, public network::INetworkSerializable {
public:
    NetworkController();
    explicit NetworkController(const std::string& name);
    ~NetworkController() override;

    std::string GetTypeName() const override { return "NetworkController"; }
    void DefineProperties() override;

    void OnEnterTree() override;
    void OnExitTree() override;

    nlohmann::json CallMethod(const std::string& method, const nlohmann::json& args) override;

    bool NetGather(network::ByteWriter& writer, bool force) override;
    void NetApply(network::ByteReader& reader, uint32_t serverTimeMs) override;

    // ------------------------------------------------------------------
    // Driven by the PredictionManager each fixed tick.
    // ------------------------------------------------------------------

    // Server: apply queued input from the controlling client (or, for the host's
    // own object, sample and apply local input directly).
    void ServerTick(float fixedDeltaTime);

    // Controlling client: if a newer authoritative state arrived, rewind to it and
    // replay unacknowledged commands.
    void ClientReconcile(float fixedDeltaTime);

    // Controlling client: sample input, predict locally, and send to the server.
    void ClientSampleAndSend(float fixedDeltaTime);

    // Consume an InputCommand frame (server side). The frame's NetworkId has
    // already been read by the PredictionManager to locate this controller.
    void ReceiveInput(network::PeerId from, network::ByteReader& reader);

    // True when this peer is the controlling client of a server-authoritative
    // object with prediction enabled (the case where prediction runs).
    bool IsPredictingLocally() const;

    network::PeerId GetControllerPeer() const { return m_ControllerPeer; }

    // Reset per-session prediction state (sequences, buffers). Called when a
    // session ends so a later session starts clean.
    void ResetSessionState();

private:
    struct InputCommand {
        uint32_t seq = 0;
        std::vector<float> axes;
        std::vector<uint8_t> buttons;  // one byte per button (0/1)
    };

    std::vector<std::string> GetAxisNames() const;
    std::vector<std::string> GetButtonNames() const;

    components::NetworkObject* GetNetworkObject() const;
    bool OwnerIsServerAuthoritative() const;

    void SampleInput();
    InputCommand BuildCommandFromCurrent(uint32_t seq) const;
    nlohmann::json BuildInputJson(const InputCommand& cmd) const;
    void Simulate(const InputCommand& cmd, float dt);

    void WriteCommand(network::ByteWriter& writer, const InputCommand& cmd) const;
    bool ReadCommand(network::ByteReader& reader, uint32_t seq, InputCommand& out) const;
    void SendInputBatch();

    void ApplyAuthoritativeTransform();
    bool NodeIs3D() const;

    network::PeerId m_ControllerPeer = network::kInvalidPeerId;
    bool m_Registered = false;

    // Current sampled input (game-set via set_axis/set_button or auto-sampled).
    std::unordered_map<std::string, float> m_AxisValues;
    std::unordered_map<std::string, bool> m_ButtonValues;

    // Controlling-client prediction state.
    uint32_t m_InputSeq = 0;
    std::deque<InputCommand> m_PendingCommands;  // unacknowledged, ascending seq
    uint32_t m_LastAckedSeq = 0;

    // Authoritative state most recently received (controlling client).
    bool m_HasNewAuth = false;
    bool m_AuthIs3D = false;
    math::Vec3 m_AuthPosition{0.0f, 0.0f, 0.0f};
    math::Quat m_AuthRotation;
    float m_AuthRotation2D = 0.0f;

    // Server-side command queue + progress.
    std::map<uint32_t, InputCommand> m_ServerCommands;
    uint32_t m_LastProcessedSeq = 0;
    uint32_t m_LastSentSeq = 0;
    network::PeerId m_LastSentController = network::kInvalidPeerId;
    bool m_HasSentState = false;
};

} // namespace components
} // namespace lupine
