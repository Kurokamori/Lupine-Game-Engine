#include "lupine/components/NetworkController.hpp"
#include "lupine/components/NetworkObject.hpp"
#include "lupine/network/NetworkManager.hpp"
#include "lupine/network/PredictionManager.hpp"
#include "lupine/network/ReplicationManager.hpp"
#include "lupine/network/NetworkSerializer.hpp"
#include "lupine/core/ScriptComponent.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/input/InputManager.hpp"

namespace lupine {
namespace components {

namespace {
// Upper bound on commands the server applies in a single tick, so a backlog
// (e.g. a client that stalled then flushed) cannot spike one tick's work.
constexpr int kMaxServerCommandsPerTick = 16;
constexpr size_t kMaxPendingCommands = 128;
constexpr size_t kMaxServerQueued = 256;
}  // namespace

NetworkController::NetworkController() : core::Component("NetworkController") {
    DefineProperties();
}

NetworkController::NetworkController(const std::string& name) : core::Component(name) {
    DefineProperties();
}

NetworkController::~NetworkController() = default;

void NetworkController::DefineProperties() {
    if (m_PropertiesDefined) {
        return;
    }
    // Input schema. When autoSampleInput is on, axis names are read from the
    // InputManager via GetAxisValue and button names via IsActionPressed; the
    // names therefore double as input-map action/axis names.
    DefineProperty(core::PropertyDescriptor("axes", core::PropertyValueType::StringArray,
                                            core::PropertyDefaultValue(std::vector<std::string>{})));
    DefineProperty(core::PropertyDescriptor("buttons", core::PropertyValueType::StringArray,
                                            core::PropertyDefaultValue(std::vector<std::string>{})));
    DefineProperty(PROPERTY_DEFAULT(autoSampleInput, Bool, true));
    m_PropertiesDefined = true;
}

void NetworkController::OnEnterTree() {
    if (!m_Registered) {
        network::NetworkManager::GetInstance().Prediction().Register(this);
        m_Registered = true;
    }
}

void NetworkController::OnExitTree() {
    if (m_Registered) {
        network::NetworkManager::GetInstance().Prediction().Unregister(this);
        m_Registered = false;
    }
}

std::vector<std::string> NetworkController::GetAxisNames() const {
    return const_cast<NetworkController*>(this)->GetPropertyValue<std::vector<std::string>>("axes");
}

std::vector<std::string> NetworkController::GetButtonNames() const {
    return const_cast<NetworkController*>(this)->GetPropertyValue<std::vector<std::string>>("buttons");
}

NetworkObject* NetworkController::GetNetworkObject() const {
    core::Node* owner = GetOwner();
    if (owner == nullptr) {
        return nullptr;
    }
    return dynamic_cast<NetworkObject*>(owner->GetComponent("NetworkObject").get());
}

bool NetworkController::OwnerIsServerAuthoritative() const {
    NetworkObject* netObject = GetNetworkObject();
    return netObject != nullptr && netObject->GetPropertyValue<bool>("serverAuthoritative");
}

bool NetworkController::NodeIs3D() const {
    return dynamic_cast<core::Node3D*>(GetOwner()) != nullptr;
}

bool NetworkController::IsPredictingLocally() const {
    network::NetworkManager& net = network::NetworkManager::GetInstance();
    if (!net.IsActive() || !net.IsClient()) {
        return false;
    }
    if (m_ControllerPeer != net.GetLocalPeerId()) {
        return false;
    }
    if (!net.GetConfig().enablePrediction) {
        return false;
    }
    return OwnerIsServerAuthoritative();
}

void NetworkController::SampleInput() {
    if (!GetPropertyValue<bool>("autoSampleInput")) {
        return;  // The game sets input explicitly via set_axis / set_button.
    }
    input::InputManager& im = input::InputManager::Get();
    for (const std::string& name : GetAxisNames()) {
        m_AxisValues[name] = im.GetAxisValue(name);
    }
    for (const std::string& name : GetButtonNames()) {
        m_ButtonValues[name] = im.IsActionPressed(name);
    }
}

NetworkController::InputCommand NetworkController::BuildCommandFromCurrent(uint32_t seq) const {
    InputCommand cmd;
    cmd.seq = seq;
    for (const std::string& name : GetAxisNames()) {
        auto it = m_AxisValues.find(name);
        cmd.axes.push_back(it != m_AxisValues.end() ? it->second : 0.0f);
    }
    for (const std::string& name : GetButtonNames()) {
        auto it = m_ButtonValues.find(name);
        cmd.buttons.push_back((it != m_ButtonValues.end() && it->second) ? 1u : 0u);
    }
    return cmd;
}

nlohmann::json NetworkController::BuildInputJson(const InputCommand& cmd) const {
    nlohmann::json input = nlohmann::json::object();
    const std::vector<std::string> axisNames = GetAxisNames();
    for (size_t i = 0; i < axisNames.size(); ++i) {
        input[axisNames[i]] = (i < cmd.axes.size()) ? cmd.axes[i] : 0.0f;
    }
    const std::vector<std::string> buttonNames = GetButtonNames();
    for (size_t i = 0; i < buttonNames.size(); ++i) {
        input[buttonNames[i]] = (i < cmd.buttons.size()) && cmd.buttons[i] != 0u;
    }
    return input;
}

void NetworkController::Simulate(const InputCommand& cmd, float dt) {
    core::Node* node = GetOwner();
    if (node == nullptr) {
        return;
    }
    const nlohmann::json args = nlohmann::json::array({BuildInputJson(cmd), dt});
    for (const std::shared_ptr<core::Component>& comp : node->GetComponents()) {
        if (auto* script = dynamic_cast<core::ScriptComponent*>(comp.get())) {
            if (scripting::IScriptEnvironment* env = script->GetScriptEnvironment()) {
                if (env->HasFunction("on_network_simulate")) {
                    env->CallFunctionArgs("on_network_simulate", args);
                }
            }
        }
    }
}

void NetworkController::WriteCommand(network::ByteWriter& writer, const InputCommand& cmd) const {
    const std::vector<std::string> axisNames = GetAxisNames();
    for (size_t i = 0; i < axisNames.size(); ++i) {
        writer.WriteHalf(i < cmd.axes.size() ? cmd.axes[i] : 0.0f);
    }
    const std::vector<std::string> buttonNames = GetButtonNames();
    for (size_t b = 0; b < buttonNames.size(); b += 8) {
        uint8_t bits = 0;
        for (size_t k = 0; k < 8 && (b + k) < buttonNames.size(); ++k) {
            if ((b + k) < cmd.buttons.size() && cmd.buttons[b + k] != 0u) {
                bits |= static_cast<uint8_t>(1u << k);
            }
        }
        writer.WriteU8(bits);
    }
}

bool NetworkController::ReadCommand(network::ByteReader& reader, uint32_t seq,
                                    InputCommand& out) const {
    out.seq = seq;
    const std::vector<std::string> axisNames = GetAxisNames();
    out.axes.resize(axisNames.size());
    for (size_t i = 0; i < axisNames.size(); ++i) {
        out.axes[i] = reader.ReadHalf();
    }
    const std::vector<std::string> buttonNames = GetButtonNames();
    out.buttons.assign(buttonNames.size(), 0u);
    for (size_t b = 0; b < buttonNames.size(); b += 8) {
        const uint8_t bits = reader.ReadU8();
        for (size_t k = 0; k < 8 && (b + k) < buttonNames.size(); ++k) {
            out.buttons[b + k] = ((bits >> k) & 1u) ? 1u : 0u;
        }
    }
    return reader.Ok();
}

void NetworkController::SendInputBatch() {
    if (m_PendingCommands.empty()) {
        return;
    }
    network::NetworkManager& net = network::NetworkManager::GetInstance();
    const network::NetworkId id = net.Replication().NetworkIdOf(GetOwner());
    if (id == network::kInvalidNetworkId) {
        return;
    }
    uint32_t redundancy = net.GetConfig().inputRedundancy;
    if (redundancy < 1u) {
        redundancy = 1u;
    }
    const size_t count = std::min(static_cast<size_t>(redundancy), m_PendingCommands.size());
    const size_t start = m_PendingCommands.size() - count;

    network::ByteWriter writer;
    writer.WriteU32(id);
    writer.WriteU32(m_PendingCommands[start].seq);  // base sequence
    writer.WriteU8(static_cast<uint8_t>(count));
    for (size_t i = start; i < m_PendingCommands.size(); ++i) {
        WriteCommand(writer, m_PendingCommands[i]);
    }
    net.SendTo(network::kServerPeerId, network::Channel::UnreliableSequenced,
               network::MsgType::InputCommand, writer.Data());
}

void NetworkController::ReceiveInput(network::PeerId from, network::ByteReader& reader) {
    // Anti-cheat: only the assigned controller may drive this object.
    if (from != m_ControllerPeer) {
        return;
    }
    const uint32_t baseSeq = reader.ReadU32();
    const uint8_t count = reader.ReadU8();
    if (!reader.Ok() || count == 0 || count > 64u) {
        return;
    }
    for (uint8_t i = 0; i < count; ++i) {
        InputCommand cmd;
        if (!ReadCommand(reader, baseSeq + i, cmd)) {
            return;
        }
        if (cmd.seq > m_LastProcessedSeq && m_ServerCommands.find(cmd.seq) == m_ServerCommands.end()) {
            m_ServerCommands[cmd.seq] = cmd;
        }
    }
    while (m_ServerCommands.size() > kMaxServerQueued) {
        m_ServerCommands.erase(m_ServerCommands.begin());
    }
}

void NetworkController::ServerTick(float fixedDeltaTime) {
    network::NetworkManager& net = network::NetworkManager::GetInstance();

    // The host's own controlled object is authoritative locally: sample and apply
    // input directly with no prediction or network round trip.
    if (m_ControllerPeer == net.GetLocalPeerId() && m_ControllerPeer != network::kInvalidPeerId) {
        SampleInput();
        const InputCommand cmd = BuildCommandFromCurrent(++m_InputSeq);
        Simulate(cmd, fixedDeltaTime);
        m_LastProcessedSeq = cmd.seq;
        return;
    }

    // Remote-controlled: apply queued commands in sequence order.
    int processed = 0;
    for (auto it = m_ServerCommands.begin();
         it != m_ServerCommands.end() && processed < kMaxServerCommandsPerTick;) {
        if (it->first <= m_LastProcessedSeq) {
            it = m_ServerCommands.erase(it);
            continue;
        }
        Simulate(it->second, fixedDeltaTime);
        m_LastProcessedSeq = it->first;
        it = m_ServerCommands.erase(it);
        ++processed;
    }
}

void NetworkController::ApplyAuthoritativeTransform() {
    core::Node* node = GetOwner();
    if (node == nullptr) {
        return;
    }
    if (m_AuthIs3D) {
        if (auto* n3 = dynamic_cast<core::Node3D*>(node)) {
            n3->SetPosition(m_AuthPosition);
            n3->SetRotation(m_AuthRotation);
        }
    } else {
        if (auto* n2 = dynamic_cast<core::Node2D*>(node)) {
            n2->SetPosition(math::Vec2{m_AuthPosition.x, m_AuthPosition.y});
            n2->SetRotation(m_AuthRotation2D);
        }
    }
}

void NetworkController::ClientReconcile(float fixedDeltaTime) {
    if (!m_HasNewAuth) {
        return;
    }
    m_HasNewAuth = false;

    // Rewind to the authoritative state, drop commands the server has confirmed,
    // and replay the still-unacknowledged ones to re-predict to "now".
    ApplyAuthoritativeTransform();
    while (!m_PendingCommands.empty() && m_PendingCommands.front().seq <= m_LastAckedSeq) {
        m_PendingCommands.pop_front();
    }
    for (const InputCommand& cmd : m_PendingCommands) {
        Simulate(cmd, fixedDeltaTime);
    }
}

void NetworkController::ClientSampleAndSend(float fixedDeltaTime) {
    SampleInput();
    const InputCommand cmd = BuildCommandFromCurrent(++m_InputSeq);
    m_PendingCommands.push_back(cmd);
    Simulate(cmd, fixedDeltaTime);
    while (m_PendingCommands.size() > kMaxPendingCommands) {
        m_PendingCommands.pop_front();
    }
    SendInputBatch();
}

bool NetworkController::NetGather(network::ByteWriter& writer, bool force) {
    core::Node* node = GetOwner();
    if (node == nullptr) {
        return false;
    }
    const bool is3D = NodeIs3D();
    math::Vec3 pos{0.0f, 0.0f, 0.0f};
    math::Quat quat;
    float rot2d = 0.0f;
    if (is3D) {
        auto* n3 = dynamic_cast<core::Node3D*>(node);
        pos = n3->GetPosition();
        quat = n3->GetRotation();
    } else if (auto* n2 = dynamic_cast<core::Node2D*>(node)) {
        const math::Vec2 p = n2->GetPosition();
        pos = math::Vec3{p.x, p.y, 0.0f};
        rot2d = n2->GetRotation();
    }

    const bool changed = !m_HasSentState || m_LastProcessedSeq != m_LastSentSeq ||
                         m_ControllerPeer != m_LastSentController;
    if (!force && !changed) {
        return false;
    }
    if (!force) {
        m_LastSentSeq = m_LastProcessedSeq;
        m_LastSentController = m_ControllerPeer;
        m_HasSentState = true;
    }

    writer.WriteU32(m_ControllerPeer);
    writer.WriteU32(m_LastProcessedSeq);
    writer.WriteU8(is3D ? 3u : 2u);
    if (is3D) {
        writer.WriteFloat(pos.x);
        writer.WriteFloat(pos.y);
        writer.WriteFloat(pos.z);
        writer.WriteFloat(quat.w());
        writer.WriteFloat(quat.x());
        writer.WriteFloat(quat.y());
        writer.WriteFloat(quat.z());
    } else {
        writer.WriteFloat(pos.x);
        writer.WriteFloat(pos.y);
        writer.WriteFloat(rot2d);
    }
    return true;
}

void NetworkController::NetApply(network::ByteReader& reader, uint32_t serverTimeMs) {
    (void)serverTimeMs;
    const network::PeerId controllerPeer = reader.ReadU32();
    const uint32_t seq = reader.ReadU32();
    const uint8_t dims = reader.ReadU8();
    const bool is3D = (dims == 3u);
    math::Vec3 pos{0.0f, 0.0f, 0.0f};
    math::Quat quat;
    float rot2d = 0.0f;
    if (is3D) {
        pos.x = reader.ReadFloat();
        pos.y = reader.ReadFloat();
        pos.z = reader.ReadFloat();
        const float w = reader.ReadFloat();
        const float x = reader.ReadFloat();
        const float y = reader.ReadFloat();
        const float z = reader.ReadFloat();
        quat = math::Quat(w, x, y, z);
    } else {
        pos.x = reader.ReadFloat();
        pos.y = reader.ReadFloat();
        rot2d = reader.ReadFloat();
    }
    if (!reader.Ok()) {
        return;
    }

    m_ControllerPeer = controllerPeer;
    m_AuthIs3D = is3D;
    m_AuthPosition = pos;
    m_AuthRotation = quat;
    m_AuthRotation2D = rot2d;
    if (seq >= m_LastAckedSeq) {
        m_LastAckedSeq = seq;
    }
    m_HasNewAuth = true;
}

void NetworkController::ResetSessionState() {
    m_InputSeq = 0;
    m_PendingCommands.clear();
    m_LastAckedSeq = 0;
    m_HasNewAuth = false;
    m_ServerCommands.clear();
    m_LastProcessedSeq = 0;
    m_LastSentSeq = 0;
    m_LastSentController = network::kInvalidPeerId;
    m_HasSentState = false;
    m_ControllerPeer = network::kInvalidPeerId;
}

nlohmann::json NetworkController::CallMethod(const std::string& method, const nlohmann::json& args) {
    if (method == "set_controller") {
        if (args.is_array() && !args.empty() && args[0].is_number()) {
            m_ControllerPeer = args[0].get<network::PeerId>();
        }
        return nlohmann::json(nullptr);
    }
    if (method == "get_controller") {
        return nlohmann::json(m_ControllerPeer);
    }
    if (method == "set_axis") {
        if (args.is_array() && args.size() >= 2 && args[0].is_string() && args[1].is_number()) {
            m_AxisValues[args[0].get<std::string>()] = args[1].get<float>();
        }
        return nlohmann::json(nullptr);
    }
    if (method == "get_axis") {
        if (args.is_array() && !args.empty() && args[0].is_string()) {
            auto it = m_AxisValues.find(args[0].get<std::string>());
            return nlohmann::json(it != m_AxisValues.end() ? it->second : 0.0f);
        }
        return nlohmann::json(0.0f);
    }
    if (method == "set_button") {
        if (args.is_array() && args.size() >= 2 && args[0].is_string()) {
            m_ButtonValues[args[0].get<std::string>()] = args[1].get<bool>();
        }
        return nlohmann::json(nullptr);
    }
    if (method == "get_button") {
        if (args.is_array() && !args.empty() && args[0].is_string()) {
            auto it = m_ButtonValues.find(args[0].get<std::string>());
            return nlohmann::json(it != m_ButtonValues.end() && it->second);
        }
        return nlohmann::json(false);
    }
    if (method == "is_predicting_locally") {
        return nlohmann::json(IsPredictingLocally());
    }
    if (method == "get_pending_input_count") {
        return nlohmann::json(static_cast<int>(m_PendingCommands.size()));
    }
    return nlohmann::json(nullptr);
}

} // namespace components
} // namespace lupine
