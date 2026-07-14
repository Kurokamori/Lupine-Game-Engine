#include "lupine/components/AnimationTree.hpp"
#include "lupine/components/AnimationPlayer.hpp"
#include "lupine/animation/BlendTree.hpp"
#include "lupine/animation/AnimationChannel.hpp"
#include "lupine/asset/AnimationGraphAsset.hpp"
#include "lupine/asset/AnimationClipAsset.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/scripting/NodeRef.hpp"
#include <memory>

namespace lupine {
namespace components {

using namespace core;
using namespace animation;

namespace {

std::string ArgStr(const nlohmann::json& args, size_t i, const std::string& def = std::string()) {
    if (args.is_array() && i < args.size() && args[i].is_string()) return args[i].get<std::string>();
    return def;
}
float ArgFloat(const nlohmann::json& args, size_t i, float def = 0.0f) {
    if (args.is_array() && i < args.size() && args[i].is_number()) return args[i].get<float>();
    return def;
}
int ArgInt(const nlohmann::json& args, size_t i, int def = 0) {
    if (args.is_array() && i < args.size() && args[i].is_number()) return args[i].get<int>();
    return def;
}
bool ArgBool(const nlohmann::json& args, size_t i, bool def = false) {
    if (args.is_array() && i < args.size() && args[i].is_boolean()) return args[i].get<bool>();
    return def;
}

SignalArgs ToSignalArgs(const nlohmann::json& args) {
    SignalArgs out;
    if (args.is_array()) {
        for (const nlohmann::json& a : args) out.push_back(a);
    } else if (!args.is_null()) {
        out.push_back(args);
    }
    return out;
}

} // namespace

AnimationTree::AnimationTree()
    : Component("AnimationTree") {
}

AnimationTree::AnimationTree(const std::string& name)
    : Component(name) {
}

AnimationTree::~AnimationTree() {
}

void AnimationTree::DefineProperties() {
    DefineProperty(PROPERTY_FILE_GROUP(graphPath, std::string(""), "*.animgraph", "Animation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(animationPlayer, NodePath, std::string(""), "Animation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(active, Bool, true, "Animation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(rootNode, NodePath, std::string(""), "Animation"));
}

void AnimationTree::DefineSignals() {
    RegisterSignal({"state_changed", {}, "Emitted when a layer changes state (layer, from, to)."});
}

nlohmann::json AnimationTree::Serialize() const {
    nlohmann::json j = Component::Serialize();
    j["parameters"] = m_Params.Serialize();
    return j;
}

void AnimationTree::Deserialize(const nlohmann::json& json) {
    Component::Deserialize(json);
    if (json.contains("parameters")) {
        m_Params.Deserialize(json["parameters"]);
    }
    m_Initialized = false;
    m_PlayerResolved = false;
}

void AnimationTree::OnAwake() {
    EnsureInit();
}

void AnimationTree::OnUpdate(float deltaTime) {
    if (!GetActive()) return;
    EnsureInit();
    if (m_Layers.empty()) return;

    BlendContext ctx;
    ctx.params = &m_Params;
    ctx.resolveClip = [this](const std::string& clip, const std::string& path) -> const AnimationClip* {
        return this->ResolveClip(clip, path);
    };

    TargetValueMap result;
    std::vector<MethodCall> methodCalls;

    for (size_t i = 0; i < m_Layers.size() && i < m_Graph.layers.size(); ++i) {
        StateChange change;
        TargetValueMap layerMap = m_Layers[i].Update(deltaTime, ctx, methodCalls, change);
        if (change.changed) {
            Emit("state_changed", { static_cast<int>(i), change.from, change.to });
        }
        CompositeLayer(result, layerMap, m_Layers[i], m_Graph.layers[i], ctx, m_Graph.layers[i].weight);
    }

    ApplyTargetValues(ResolveRoot(), result);
    FireMethodCalls(methodCalls);
}

void AnimationTree::OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) {
    (void)newValue;
    if (propertyName == "graphPath" || propertyName == "animationPlayer" || propertyName == "rootNode") {
        m_Initialized = false;
        m_PlayerResolved = false;
        m_Player = nullptr;
    }
}

nlohmann::json AnimationTree::CallMethod(const std::string& method, const nlohmann::json& args) {
    if (method == "set_float") {
        SetFloat(ArgStr(args, 0), ArgFloat(args, 1));
    } else if (method == "set_int") {
        SetInt(ArgStr(args, 0), ArgInt(args, 1));
    } else if (method == "set_bool") {
        SetBool(ArgStr(args, 0), ArgBool(args, 1));
    } else if (method == "set_trigger") {
        SetTrigger(ArgStr(args, 0));
    } else if (method == "get_float") {
        return GetFloat(ArgStr(args, 0));
    } else if (method == "get_int") {
        return GetInt(ArgStr(args, 0));
    } else if (method == "get_bool") {
        return GetBool(ArgStr(args, 0));
    } else if (method == "travel") {
        Travel(ArgStr(args, 0), ArgInt(args, 1, 0), args.is_array() && args.size() > 2 ? ArgFloat(args, 2, 0.2f) : 0.2f);
    } else if (method == "get_current_state") {
        return GetCurrentState(ArgInt(args, 0, 0));
    } else if (method == "set_active") {
        SetActive(ArgBool(args, 0, true));
    } else if (method == "is_active") {
        return GetActive();
    } else if (method == "evaluate") {
        // Advance the graph one step and apply its pose (editor preview tick).
        OnUpdate(ArgFloat(args, 0, 0.0f));
    } else if (method == "reload_graph") {
        ReloadGraph();
    } else if (method == "get_parameters") {
        return m_Params.Serialize();
    }
    return nlohmann::json();
}

// ===== Parameters =====

void AnimationTree::SetFloat(const std::string& name, float value) { m_Params.SetFloat(name, value); }
void AnimationTree::SetInt(const std::string& name, int value) { m_Params.SetInt(name, value); }
void AnimationTree::SetBool(const std::string& name, bool value) { m_Params.SetBool(name, value); }
void AnimationTree::SetTrigger(const std::string& name) { m_Params.SetTrigger(name); }
float AnimationTree::GetFloat(const std::string& name) { return m_Params.GetFloat(name); }
int AnimationTree::GetInt(const std::string& name) { return m_Params.GetInt(name); }
bool AnimationTree::GetBool(const std::string& name) { return m_Params.GetBool(name); }

// ===== State machine control =====

void AnimationTree::Travel(const std::string& state, int layer, float duration) {
    EnsureInit();
    if (layer >= 0 && layer < static_cast<int>(m_Layers.size())) {
        m_Layers[static_cast<size_t>(layer)].Travel(state, duration);
    }
}

std::string AnimationTree::GetCurrentState(int layer) {
    EnsureInit();
    if (layer >= 0 && layer < static_cast<int>(m_Layers.size())) {
        return m_Layers[static_cast<size_t>(layer)].CurrentState();
    }
    return std::string();
}

// ===== Internals =====

void AnimationTree::EnsureInit() {
    if (m_Initialized) return;
    LoadGraph();
    m_Params.InitFromGraph(m_Graph);
    m_Layers.clear();
    m_Layers.resize(m_Graph.layers.size());
    for (size_t i = 0; i < m_Graph.layers.size(); ++i) {
        m_Layers[i].Init(m_Graph.layers[i]);
    }
    m_PlayerResolved = false;
    m_Player = nullptr;
    ResolvePlayer();
    m_Initialized = true;
}

void AnimationTree::LoadGraph() {
    m_Graph = AnimationGraph();
    m_PathClips.clear();
    std::string path = GetGraphPath();
    if (path.empty()) return;
    asset::AnimationGraphAsset graphAsset;
    if (graphAsset.LoadFromFile(path)) {
        m_Graph = graphAsset.GetGraph();
    }
}

AnimationPlayer* AnimationTree::ResolvePlayer() {
    if (m_PlayerResolved && m_Player) return m_Player;

    core::Node* owner = GetOwner();
    if (!owner) return nullptr;

    core::Node* node = owner;
    std::string pp = GetAnimationPlayerPath();
    if (!pp.empty()) {
        scripting::NodeRef ref = scripting::NodeRef::FromRaw(owner, nullptr).FindNode(pp);
        node = ref.IsValid() ? ref.Lock().get() : nullptr;
    }

    if (node) {
        std::shared_ptr<AnimationPlayer> comp = node->GetComponent<AnimationPlayer>();
        m_Player = comp.get();
        if (m_Player) {
            m_Player->SetControlledByTree(true);
        }
    }
    m_PlayerResolved = true;
    return m_Player;
}

const AnimationClip* AnimationTree::ResolveClip(const std::string& clip, const std::string& path) {
    if (!path.empty()) {
        auto it = m_PathClips.find(path);
        if (it != m_PathClips.end()) return &it->second;
        asset::AnimationClipAsset clipAsset;
        if (clipAsset.LoadFromFile(path)) {
            m_PathClips[path] = clipAsset.GetClip();
            return &m_PathClips[path];
        }
        return nullptr;
    }
    if (!clip.empty()) {
        AnimationPlayer* player = ResolvePlayer();
        if (player) return player->GetClip(clip);
    }
    return nullptr;
}

core::Node* AnimationTree::ResolveRoot() const {
    core::Node* owner = GetOwner();
    if (!owner) return nullptr;
    std::string rootPath = GetRootNodePath();
    if (rootPath.empty() || rootPath == ".") {
        return owner;
    }
    scripting::NodeRef ref = scripting::NodeRef::FromRaw(owner, nullptr).FindNode(rootPath);
    return ref.IsValid() ? ref.Lock().get() : owner;
}

nlohmann::json AnimationTree::ReadCurrent(const std::string& node, const std::string& nodeUuid,
                                          const std::string& channel,
                                          animation::TrackValueType type) const {
    core::Node* root = ResolveRoot();
    scripting::NodeRef target = animation::ResolveTargetNode(root, node, nodeUuid);
    if (!target.IsValid()) return nlohmann::json();
    return animation::ReadChannel(target, channel, type);
}

void AnimationTree::CompositeLayer(TargetValueMap& result, const TargetValueMap& layerMap,
                                   LayerRuntime& runtime, const AnimationLayer& layerData,
                                   const BlendContext& ctx, float weight) const {
    if (weight <= 0.0f || layerMap.empty()) return;

    const bool additive = layerData.blendMode == LayerBlendMode::Additive;
    TargetValueMap reference;
    if (additive) {
        reference = runtime.EvaluateReference(ctx);
    }

    for (const auto& kv : layerMap) {
        const std::string& key = kv.first;
        const TargetValue& tv = kv.second;
        if (!layerData.mask.Allows(tv.node, tv.channel)) continue;

        nlohmann::json base;
        auto rit = result.find(key);
        if (rit != result.end()) {
            base = rit->second.value;
        } else {
            base = ReadCurrent(tv.node, tv.nodeUuid, tv.channel, tv.valueType);
        }
        if (base.is_null()) base = tv.value;

        TargetValue out = tv;
        if (additive) {
            nlohmann::json refValue = tv.value;
            auto refIt = reference.find(key);
            if (refIt != reference.end()) refValue = refIt->second.value;
            out.value = AddChannelDelta(base, tv.value, refValue, weight);
        } else {
            out.value = (weight >= 1.0f)
                ? tv.value
                : BlendChannelValues(tv.valueType, tv.useSlerp, base, tv.value, weight);
        }
        result[key] = out;
    }
}

void AnimationTree::FireMethodCalls(const std::vector<MethodCall>& calls) const {
    if (calls.empty()) return;
    core::Node* root = ResolveRoot();

    for (const MethodCall& call : calls) {
        scripting::NodeRef tr = animation::ResolveTargetNode(root, call.node, call.nodeUuid);
        core::Node* target = tr.IsValid() ? tr.Lock().get() : nullptr;
        if (target && !call.method.empty()) {
            target->InvokeSignalMethod(call.method, ToSignalArgs(call.args));
        }
    }
}

void AnimationTree::ReloadGraph() {
    m_Initialized = false;
    EnsureInit();
}

// ===== Property accessors =====

std::string AnimationTree::GetGraphPath() const { return GetPropertyValue<std::string>("graphPath"); }
void AnimationTree::SetGraphPath(const std::string& path) { SetPropertyValue<std::string>("graphPath", path); m_Initialized = false; }
std::string AnimationTree::GetAnimationPlayerPath() const { return GetPropertyValue<std::string>("animationPlayer"); }
void AnimationTree::SetAnimationPlayerPath(const std::string& path) { SetPropertyValue<std::string>("animationPlayer", path); m_PlayerResolved = false; m_Player = nullptr; }
bool AnimationTree::GetActive() const { return GetPropertyValue<bool>("active"); }
void AnimationTree::SetActive(bool active) { SetPropertyValue<bool>("active", active); }
std::string AnimationTree::GetRootNodePath() const { return GetPropertyValue<std::string>("rootNode"); }
void AnimationTree::SetRootNodePath(const std::string& path) { SetPropertyValue<std::string>("rootNode", path); }

} // namespace components
} // namespace lupine
