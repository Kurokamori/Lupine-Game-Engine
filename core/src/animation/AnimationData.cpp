#include "lupine/animation/AnimationData.hpp"
#include <algorithm>
#include <cmath>

namespace lupine {
namespace animation {

// ===========================================================================
// Enum string conversions
// ===========================================================================

const char* ToString(TrackType v) {
    switch (v) {
        case TrackType::Value: return "value";
        case TrackType::Method: return "method";
    }
    return "value";
}

const char* ToString(InterpolationMode v) {
    switch (v) {
        case InterpolationMode::Constant: return "constant";
        case InterpolationMode::Linear: return "linear";
        case InterpolationMode::Cubic: return "cubic";
    }
    return "linear";
}

const char* ToString(LoopMode v) {
    switch (v) {
        case LoopMode::None: return "none";
        case LoopMode::Loop: return "loop";
        case LoopMode::PingPong: return "pingpong";
    }
    return "none";
}

const char* ToString(TrackValueType v) {
    switch (v) {
        case TrackValueType::Float: return "float";
        case TrackValueType::Vec2: return "vec2";
        case TrackValueType::Vec3: return "vec3";
        case TrackValueType::Vec4: return "vec4";
        case TrackValueType::Color: return "color";
        case TrackValueType::Bool: return "bool";
        case TrackValueType::Int: return "int";
        case TrackValueType::Quat: return "quat";
        case TrackValueType::String: return "string";
    }
    return "float";
}

const char* ToString(BlendNodeType v) {
    switch (v) {
        case BlendNodeType::Clip: return "clip";
        case BlendNodeType::Blend1D: return "blend1d";
        case BlendNodeType::Blend2D: return "blend2d";
        case BlendNodeType::Direct: return "direct";
        case BlendNodeType::Blend2: return "blend2";
        case BlendNodeType::Add2: return "add2";
        case BlendNodeType::OneShot: return "oneshot";
        case BlendNodeType::TimeScale: return "timescale";
    }
    return "clip";
}

const char* ToString(Blend2DMode v) {
    switch (v) {
        case Blend2DMode::SimpleDirectional: return "simple_directional";
        case Blend2DMode::FreeformDirectional: return "freeform_directional";
        case Blend2DMode::FreeformCartesian: return "freeform_cartesian";
        case Blend2DMode::Direct: return "direct";
    }
    return "freeform_directional";
}

const char* ToString(LayerBlendMode v) {
    switch (v) {
        case LayerBlendMode::Override: return "override";
        case LayerBlendMode::Additive: return "additive";
    }
    return "override";
}

const char* ToString(MaskMode v) {
    switch (v) {
        case MaskMode::Include: return "include";
        case MaskMode::Exclude: return "exclude";
    }
    return "include";
}

const char* ToString(ParamType v) {
    switch (v) {
        case ParamType::Float: return "float";
        case ParamType::Int: return "int";
        case ParamType::Bool: return "bool";
        case ParamType::Trigger: return "trigger";
    }
    return "float";
}

const char* ToString(ConditionOp v) {
    switch (v) {
        case ConditionOp::Greater: return "greater";
        case ConditionOp::Less: return "less";
        case ConditionOp::Equal: return "equal";
        case ConditionOp::NotEqual: return "not_equal";
        case ConditionOp::IsTrue: return "is_true";
        case ConditionOp::IsFalse: return "is_false";
    }
    return "greater";
}

TrackType TrackTypeFromString(const std::string& s) {
    if (s == "method") return TrackType::Method;
    return TrackType::Value;
}

InterpolationMode InterpolationModeFromString(const std::string& s) {
    if (s == "constant") return InterpolationMode::Constant;
    if (s == "cubic") return InterpolationMode::Cubic;
    return InterpolationMode::Linear;
}

LoopMode LoopModeFromString(const std::string& s) {
    if (s == "loop") return LoopMode::Loop;
    if (s == "pingpong") return LoopMode::PingPong;
    return LoopMode::None;
}

TrackValueType TrackValueTypeFromString(const std::string& s) {
    if (s == "vec2") return TrackValueType::Vec2;
    if (s == "vec3") return TrackValueType::Vec3;
    if (s == "vec4") return TrackValueType::Vec4;
    if (s == "color") return TrackValueType::Color;
    if (s == "bool") return TrackValueType::Bool;
    if (s == "int") return TrackValueType::Int;
    if (s == "quat") return TrackValueType::Quat;
    if (s == "string") return TrackValueType::String;
    return TrackValueType::Float;
}

BlendNodeType BlendNodeTypeFromString(const std::string& s) {
    if (s == "blend1d") return BlendNodeType::Blend1D;
    if (s == "blend2d") return BlendNodeType::Blend2D;
    if (s == "direct") return BlendNodeType::Direct;
    if (s == "blend2") return BlendNodeType::Blend2;
    if (s == "add2") return BlendNodeType::Add2;
    if (s == "oneshot") return BlendNodeType::OneShot;
    if (s == "timescale") return BlendNodeType::TimeScale;
    return BlendNodeType::Clip;
}

Blend2DMode Blend2DModeFromString(const std::string& s) {
    if (s == "simple_directional") return Blend2DMode::SimpleDirectional;
    if (s == "freeform_cartesian") return Blend2DMode::FreeformCartesian;
    if (s == "direct") return Blend2DMode::Direct;
    return Blend2DMode::FreeformDirectional;
}

LayerBlendMode LayerBlendModeFromString(const std::string& s) {
    if (s == "additive") return LayerBlendMode::Additive;
    return LayerBlendMode::Override;
}

MaskMode MaskModeFromString(const std::string& s) {
    if (s == "exclude") return MaskMode::Exclude;
    return MaskMode::Include;
}

ParamType ParamTypeFromString(const std::string& s) {
    if (s == "int") return ParamType::Int;
    if (s == "bool") return ParamType::Bool;
    if (s == "trigger") return ParamType::Trigger;
    return ParamType::Float;
}

ConditionOp ConditionOpFromString(const std::string& s) {
    if (s == "less") return ConditionOp::Less;
    if (s == "equal") return ConditionOp::Equal;
    if (s == "not_equal") return ConditionOp::NotEqual;
    if (s == "is_true") return ConditionOp::IsTrue;
    if (s == "is_false") return ConditionOp::IsFalse;
    return ConditionOp::Greater;
}

int ValueComponentCount(TrackValueType type) {
    switch (type) {
        case TrackValueType::Float:
        case TrackValueType::Bool:
        case TrackValueType::Int:
        case TrackValueType::String: return 1;
        case TrackValueType::Vec2:  return 2;
        case TrackValueType::Vec3:  return 3;
        case TrackValueType::Quat:  return 3;  // Stored/applied as euler degrees.
        case TrackValueType::Vec4:
        case TrackValueType::Color: return 4;
    }
    return 1;
}

TrackValueType InferValueTypeFromChannel(const std::string& channel) {
    if (channel == "position_2d" || channel == "scale_2d" || channel == "global_position_2d") {
        return TrackValueType::Vec2;
    }
    if (channel == "rotation_2d") {
        return TrackValueType::Float;
    }
    if (channel == "position_3d" || channel == "scale_3d" || channel == "global_position_3d") {
        return TrackValueType::Vec3;
    }
    if (channel == "rotation_3d") {
        return TrackValueType::Quat;
    }
    if (channel == "modulate") {
        return TrackValueType::Color;
    }
    return TrackValueType::Float;
}

bool ChannelUsesSlerp(const std::string& channel) {
    return channel == "rotation_3d";
}

// ===========================================================================
// Keyframe
// ===========================================================================

nlohmann::json Keyframe::Serialize(TrackType trackType) const {
    nlohmann::json j;
    j["t"] = time;
    if (trackType == TrackType::Method) {
        j["method"] = method;
        j["args"] = args;
        return j;
    }
    j["v"] = value;
    if (!inTangent.empty()) j["in"] = inTangent;
    if (!outTangent.empty()) j["out"] = outTangent;
    return j;
}

Keyframe Keyframe::Deserialize(const nlohmann::json& j, TrackType trackType) {
    Keyframe k;
    k.time = j.value("t", 0.0f);
    if (trackType == TrackType::Method) {
        k.method = j.value("method", std::string());
        if (j.contains("args")) k.args = j["args"];
        return k;
    }
    if (j.contains("v")) k.value = j["v"];
    if (j.contains("in") && j["in"].is_array()) k.inTangent = j["in"].get<std::vector<float>>();
    if (j.contains("out") && j["out"].is_array()) k.outTangent = j["out"].get<std::vector<float>>();
    return k;
}

// ===========================================================================
// Track
// ===========================================================================

nlohmann::json Track::Serialize() const {
    nlohmann::json j;
    j["type"] = ToString(type);
    j["node"] = node;
    if (!nodeUuid.empty()) j["nodeUuid"] = nodeUuid;
    j["channel"] = channel;
    j["valueType"] = ToString(valueType);
    j["interp"] = ToString(interp);
    j["useSlerp"] = useSlerp;
    nlohmann::json keysJson = nlohmann::json::array();
    for (const Keyframe& k : keys) {
        keysJson.push_back(k.Serialize(type));
    }
    j["keys"] = keysJson;
    return j;
}

Track Track::Deserialize(const nlohmann::json& j) {
    Track t;
    t.type = TrackTypeFromString(j.value("type", std::string("value")));
    t.node = j.value("node", std::string());
    t.nodeUuid = j.value("nodeUuid", std::string());
    t.channel = j.value("channel", std::string());
    if (j.contains("valueType")) {
        t.valueType = TrackValueTypeFromString(j["valueType"].get<std::string>());
    } else {
        t.valueType = InferValueTypeFromChannel(t.channel);
    }
    t.interp = InterpolationModeFromString(j.value("interp", std::string("linear")));
    t.useSlerp = j.value("useSlerp", ChannelUsesSlerp(t.channel));
    if (j.contains("keys") && j["keys"].is_array()) {
        for (const nlohmann::json& kj : j["keys"]) {
            t.keys.push_back(Keyframe::Deserialize(kj, t.type));
        }
    }
    t.SortKeys();
    return t;
}

void Track::SortKeys() {
    std::stable_sort(keys.begin(), keys.end(),
        [](const Keyframe& a, const Keyframe& b) { return a.time < b.time; });
}

std::string Track::TargetKey() const {
    return node + std::string("\x1F") + channel;
}

// ===========================================================================
// AnimationClip
// ===========================================================================

nlohmann::json AnimationClip::Serialize() const {
    nlohmann::json j;
    j["version"] = 1;
    j["name"] = name;
    j["length"] = length;
    j["fps"] = fps;
    j["loop"] = ToString(loopMode);
    nlohmann::json tracksJson = nlohmann::json::array();
    for (const Track& tr : tracks) {
        tracksJson.push_back(tr.Serialize());
    }
    j["tracks"] = tracksJson;
    return j;
}

AnimationClip AnimationClip::Deserialize(const nlohmann::json& j) {
    AnimationClip c;
    c.name = j.value("name", std::string());
    c.length = j.value("length", 0.0f);
    c.fps = j.value("fps", 30.0f);
    c.loopMode = LoopModeFromString(j.value("loop", std::string("none")));
    if (j.contains("tracks") && j["tracks"].is_array()) {
        for (const nlohmann::json& tj : j["tracks"]) {
            c.tracks.push_back(Track::Deserialize(tj));
        }
    }
    if (c.length <= 0.0f) {
        c.length = c.ComputeLength();
    }
    return c;
}

float AnimationClip::ComputeLength() const {
    float maxTime = 0.0f;
    for (const Track& tr : tracks) {
        if (!tr.keys.empty()) {
            maxTime = std::max(maxTime, tr.keys.back().time);
        }
    }
    return maxTime;
}

float AnimationClip::EffectiveLength() const {
    return length > 0.0f ? length : ComputeLength();
}

// ===========================================================================
// Blend tree
// ===========================================================================

nlohmann::json BlendNode::Serialize() const {
    nlohmann::json j;
    j["type"] = ToString(type);
    switch (type) {
        case BlendNodeType::Clip:
            j["clip"] = clip;
            if (!clipPath.empty()) j["clipPath"] = clipPath;
            j["speed"] = speed;
            break;
        case BlendNodeType::Blend1D:
            j["parameter"] = parameterX;
            break;
        case BlendNodeType::Blend2D:
            j["parameterX"] = parameterX;
            j["parameterY"] = parameterY;
            j["mode"] = ToString(blend2dMode);
            break;
        case BlendNodeType::Blend2:
        case BlendNodeType::Add2:
        case BlendNodeType::TimeScale:
            j["parameter"] = parameterX;
            break;
        case BlendNodeType::OneShot:
            j["parameter"] = parameter;
            j["fadeIn"] = fadeIn;
            j["fadeOut"] = fadeOut;
            break;
        case BlendNodeType::Direct:
            break;
    }
    if (!children.empty()) {
        nlohmann::json childrenJson = nlohmann::json::array();
        for (const BlendChild& ch : children) {
            nlohmann::json cj;
            cj["node"] = ch.node ? ch.node->Serialize() : nlohmann::json();
            switch (type) {
                case BlendNodeType::Blend1D: cj["threshold"] = ch.threshold; break;
                case BlendNodeType::Blend2D: cj["posX"] = ch.posX; cj["posY"] = ch.posY; break;
                case BlendNodeType::Direct: cj["parameter"] = ch.parameter; break;
                default: break;
            }
            childrenJson.push_back(cj);
        }
        j["children"] = childrenJson;
    }
    return j;
}

std::shared_ptr<BlendNode> BlendNode::Deserialize(const nlohmann::json& j) {
    if (!j.is_object()) return nullptr;
    auto node = std::make_shared<BlendNode>();
    node->type = BlendNodeTypeFromString(j.value("type", std::string("clip")));
    node->clip = j.value("clip", std::string());
    node->clipPath = j.value("clipPath", std::string());
    node->speed = j.value("speed", 1.0f);
    node->blend2dMode = Blend2DModeFromString(j.value("mode", std::string("freeform_directional")));
    node->fadeIn = j.value("fadeIn", 0.2f);
    node->fadeOut = j.value("fadeOut", 0.2f);

    switch (node->type) {
        case BlendNodeType::Blend1D:
        case BlendNodeType::Blend2:
        case BlendNodeType::Add2:
        case BlendNodeType::TimeScale:
            node->parameterX = j.value("parameter", std::string());
            break;
        case BlendNodeType::Blend2D:
            node->parameterX = j.value("parameterX", std::string());
            node->parameterY = j.value("parameterY", std::string());
            break;
        case BlendNodeType::OneShot:
            node->parameter = j.value("parameter", std::string());
            break;
        default:
            break;
    }

    if (j.contains("children") && j["children"].is_array()) {
        for (const nlohmann::json& cj : j["children"]) {
            BlendChild ch;
            if (cj.contains("node")) ch.node = BlendNode::Deserialize(cj["node"]);
            ch.threshold = cj.value("threshold", 0.0f);
            if (cj.contains("pos") && cj["pos"].is_array() && cj["pos"].size() >= 2) {
                ch.posX = cj["pos"][0].get<float>();
                ch.posY = cj["pos"][1].get<float>();
            }
            ch.posX = cj.value("posX", ch.posX);
            ch.posY = cj.value("posY", ch.posY);
            ch.parameter = cj.value("parameter", std::string());
            node->children.push_back(ch);
        }
    }
    return node;
}

// ===========================================================================
// State machine
// ===========================================================================

nlohmann::json TransitionCondition::Serialize() const {
    nlohmann::json j;
    j["param"] = parameter;
    j["op"] = ToString(op);
    j["value"] = value;
    return j;
}

TransitionCondition TransitionCondition::Deserialize(const nlohmann::json& j) {
    TransitionCondition c;
    c.parameter = j.value("param", std::string());
    c.op = ConditionOpFromString(j.value("op", std::string("greater")));
    c.value = j.value("value", 0.0f);
    return c;
}

bool StateTransition::IsAnyState() const {
    return from.empty() || from == "Any";
}

nlohmann::json StateTransition::Serialize() const {
    nlohmann::json j;
    j["from"] = from;
    j["to"] = to;
    j["hasExitTime"] = hasExitTime;
    j["exitTime"] = exitTime;
    j["duration"] = duration;
    j["priority"] = priority;
    nlohmann::json condsJson = nlohmann::json::array();
    for (const TransitionCondition& cond : conditions) {
        condsJson.push_back(cond.Serialize());
    }
    j["conditions"] = condsJson;
    return j;
}

StateTransition StateTransition::Deserialize(const nlohmann::json& j) {
    StateTransition t;
    t.from = j.value("from", std::string());
    t.to = j.value("to", std::string());
    t.hasExitTime = j.value("hasExitTime", false);
    t.exitTime = j.value("exitTime", 1.0f);
    t.duration = j.value("duration", 0.2f);
    t.priority = j.value("priority", 0);
    if (j.contains("conditions") && j["conditions"].is_array()) {
        for (const nlohmann::json& cj : j["conditions"]) {
            t.conditions.push_back(TransitionCondition::Deserialize(cj));
        }
    }
    return t;
}

nlohmann::json AnimationState::Serialize() const {
    nlohmann::json j;
    j["name"] = name;
    j["type"] = isBlendTree ? "blendtree" : "clip";
    j["speed"] = speed;
    j["pos"] = nlohmann::json::array({posX, posY});
    if (isBlendTree) {
        if (blendTree) j["blendTree"] = blendTree->Serialize();
    } else {
        j["clip"] = clip;
    }
    return j;
}

AnimationState AnimationState::Deserialize(const nlohmann::json& j) {
    AnimationState s;
    s.name = j.value("name", std::string());
    s.isBlendTree = (j.value("type", std::string("clip")) == "blendtree");
    s.clip = j.value("clip", std::string());
    s.speed = j.value("speed", 1.0f);
    if (j.contains("pos") && j["pos"].is_array() && j["pos"].size() >= 2) {
        s.posX = j["pos"][0].get<float>();
        s.posY = j["pos"][1].get<float>();
    }
    if (s.isBlendTree && j.contains("blendTree")) {
        s.blendTree = BlendNode::Deserialize(j["blendTree"]);
    }
    return s;
}

const AnimationState* StateMachineData::FindState(const std::string& name) const {
    for (const AnimationState& s : states) {
        if (s.name == name) return &s;
    }
    return nullptr;
}

nlohmann::json StateMachineData::Serialize() const {
    nlohmann::json j;
    j["entry"] = entry;
    nlohmann::json statesJson = nlohmann::json::array();
    for (const AnimationState& s : states) statesJson.push_back(s.Serialize());
    j["states"] = statesJson;
    nlohmann::json transJson = nlohmann::json::array();
    for (const StateTransition& t : transitions) transJson.push_back(t.Serialize());
    j["transitions"] = transJson;
    return j;
}

StateMachineData StateMachineData::Deserialize(const nlohmann::json& j) {
    StateMachineData sm;
    sm.entry = j.value("entry", std::string());
    if (j.contains("states") && j["states"].is_array()) {
        for (const nlohmann::json& sj : j["states"]) {
            sm.states.push_back(AnimationState::Deserialize(sj));
        }
    }
    if (j.contains("transitions") && j["transitions"].is_array()) {
        for (const nlohmann::json& tj : j["transitions"]) {
            sm.transitions.push_back(StateTransition::Deserialize(tj));
        }
    }
    if (sm.entry.empty() && !sm.states.empty()) {
        sm.entry = sm.states.front().name;
    }
    return sm;
}

// ===========================================================================
// Layers + graph
// ===========================================================================

bool LayerMask::Allows(const std::string& node, const std::string& channel) const {
    if (targets.empty()) {
        return mode == MaskMode::Include;
    }
    const std::string key = node + std::string("\x1F") + channel;
    bool listed = false;
    for (const std::string& token : targets) {
        if (token == key || token == node) {
            listed = true;
            break;
        }
    }
    return mode == MaskMode::Include ? listed : !listed;
}

nlohmann::json LayerMask::Serialize() const {
    nlohmann::json j;
    j["mode"] = ToString(mode);
    j["targets"] = targets;
    return j;
}

LayerMask LayerMask::Deserialize(const nlohmann::json& j) {
    LayerMask m;
    m.mode = MaskModeFromString(j.value("mode", std::string("include")));
    if (j.contains("targets") && j["targets"].is_array()) {
        m.targets = j["targets"].get<std::vector<std::string>>();
    }
    return m;
}

nlohmann::json AnimationLayer::Serialize() const {
    nlohmann::json j;
    j["name"] = name;
    j["weight"] = weight;
    j["blendMode"] = ToString(blendMode);
    j["mask"] = mask.Serialize();
    j["stateMachine"] = stateMachine.Serialize();
    return j;
}

AnimationLayer AnimationLayer::Deserialize(const nlohmann::json& j) {
    AnimationLayer l;
    l.name = j.value("name", std::string("Layer"));
    l.weight = j.value("weight", 1.0f);
    l.blendMode = LayerBlendModeFromString(j.value("blendMode", std::string("override")));
    if (j.contains("mask")) l.mask = LayerMask::Deserialize(j["mask"]);
    if (j.contains("stateMachine")) l.stateMachine = StateMachineData::Deserialize(j["stateMachine"]);
    return l;
}

nlohmann::json AnimationParameter::Serialize() const {
    nlohmann::json j;
    j["name"] = name;
    j["type"] = ToString(type);
    switch (type) {
        case ParamType::Float:   j["default"] = defFloat; break;
        case ParamType::Int:     j["default"] = defInt; break;
        case ParamType::Bool:
        case ParamType::Trigger: j["default"] = defBool; break;
    }
    return j;
}

AnimationParameter AnimationParameter::Deserialize(const nlohmann::json& j) {
    AnimationParameter p;
    p.name = j.value("name", std::string());
    p.type = ParamTypeFromString(j.value("type", std::string("float")));
    if (j.contains("default")) {
        const nlohmann::json& d = j["default"];
        switch (p.type) {
            case ParamType::Float:   if (d.is_number()) p.defFloat = d.get<float>(); break;
            case ParamType::Int:     if (d.is_number()) p.defInt = d.get<int>(); break;
            case ParamType::Bool:
            case ParamType::Trigger: if (d.is_boolean()) p.defBool = d.get<bool>(); break;
        }
    }
    return p;
}

const AnimationParameter* AnimationGraph::FindParameter(const std::string& name) const {
    for (const AnimationParameter& p : parameters) {
        if (p.name == name) return &p;
    }
    return nullptr;
}

nlohmann::json AnimationGraph::Serialize() const {
    nlohmann::json j;
    j["version"] = version;
    nlohmann::json paramsJson = nlohmann::json::array();
    for (const AnimationParameter& p : parameters) paramsJson.push_back(p.Serialize());
    j["parameters"] = paramsJson;
    nlohmann::json layersJson = nlohmann::json::array();
    for (const AnimationLayer& l : layers) layersJson.push_back(l.Serialize());
    j["layers"] = layersJson;
    return j;
}

AnimationGraph AnimationGraph::Deserialize(const nlohmann::json& j) {
    AnimationGraph g;
    g.version = j.value("version", 1);
    if (j.contains("parameters") && j["parameters"].is_array()) {
        for (const nlohmann::json& pj : j["parameters"]) {
            g.parameters.push_back(AnimationParameter::Deserialize(pj));
        }
    }
    if (j.contains("layers") && j["layers"].is_array()) {
        for (const nlohmann::json& lj : j["layers"]) {
            g.layers.push_back(AnimationLayer::Deserialize(lj));
        }
    }
    return g;
}

// ===========================================================================
// AnimationParameters (runtime store)
// ===========================================================================

void AnimationParameters::InitFromGraph(const AnimationGraph& graph) {
    for (const AnimationParameter& p : graph.parameters) {
        switch (p.type) {
            case ParamType::Float:   if (!m_Floats.count(p.name)) m_Floats[p.name] = p.defFloat; break;
            case ParamType::Int:     if (!m_Ints.count(p.name)) m_Ints[p.name] = p.defInt; break;
            case ParamType::Bool:    if (!m_Bools.count(p.name)) m_Bools[p.name] = p.defBool; break;
            case ParamType::Trigger: if (!m_Triggers.count(p.name)) m_Triggers[p.name] = p.defBool; break;
        }
    }
}

void AnimationParameters::SetFloat(const std::string& name, float value) { m_Floats[name] = value; }
void AnimationParameters::SetInt(const std::string& name, int value) { m_Ints[name] = value; }
void AnimationParameters::SetBool(const std::string& name, bool value) { m_Bools[name] = value; }
void AnimationParameters::SetTrigger(const std::string& name) { m_Triggers[name] = true; }
void AnimationParameters::ResetTrigger(const std::string& name) { m_Triggers[name] = false; }

float AnimationParameters::GetFloat(const std::string& name) const {
    auto fit = m_Floats.find(name);
    if (fit != m_Floats.end()) return fit->second;
    auto iit = m_Ints.find(name);
    if (iit != m_Ints.end()) return static_cast<float>(iit->second);
    auto bit = m_Bools.find(name);
    if (bit != m_Bools.end()) return bit->second ? 1.0f : 0.0f;
    auto tit = m_Triggers.find(name);
    if (tit != m_Triggers.end()) return tit->second ? 1.0f : 0.0f;
    return 0.0f;
}

int AnimationParameters::GetInt(const std::string& name) const {
    auto iit = m_Ints.find(name);
    if (iit != m_Ints.end()) return iit->second;
    return static_cast<int>(std::lround(GetFloat(name)));
}

bool AnimationParameters::GetBool(const std::string& name) const {
    auto bit = m_Bools.find(name);
    if (bit != m_Bools.end()) return bit->second;
    auto tit = m_Triggers.find(name);
    if (tit != m_Triggers.end()) return tit->second;
    return false;
}

bool AnimationParameters::IsTriggerSet(const std::string& name) const {
    auto tit = m_Triggers.find(name);
    return tit != m_Triggers.end() && tit->second;
}

bool AnimationParameters::Has(const std::string& name) const {
    return m_Floats.count(name) || m_Ints.count(name) || m_Bools.count(name) || m_Triggers.count(name);
}

bool AnimationParameters::EvaluateCondition(const TransitionCondition& cond) const {
    switch (cond.op) {
        case ConditionOp::Greater:  return GetFloat(cond.parameter) > cond.value;
        case ConditionOp::Less:     return GetFloat(cond.parameter) < cond.value;
        case ConditionOp::Equal:    return std::fabs(GetFloat(cond.parameter) - cond.value) < 0.0001f;
        case ConditionOp::NotEqual: return std::fabs(GetFloat(cond.parameter) - cond.value) >= 0.0001f;
        case ConditionOp::IsTrue:   return GetBool(cond.parameter);
        case ConditionOp::IsFalse:  return !GetBool(cond.parameter);
    }
    return false;
}

void AnimationParameters::ConsumeTriggersFor(const std::vector<TransitionCondition>& conditions) {
    for (const TransitionCondition& cond : conditions) {
        auto tit = m_Triggers.find(cond.parameter);
        if (tit != m_Triggers.end()) {
            tit->second = false;
        }
    }
}

nlohmann::json AnimationParameters::Serialize() const {
    nlohmann::json j;
    j["floats"] = m_Floats;
    j["ints"] = m_Ints;
    j["bools"] = m_Bools;
    j["triggers"] = m_Triggers;
    return j;
}

void AnimationParameters::Deserialize(const nlohmann::json& j) {
    if (j.contains("floats")) m_Floats = j["floats"].get<std::unordered_map<std::string, float>>();
    if (j.contains("ints")) m_Ints = j["ints"].get<std::unordered_map<std::string, int>>();
    if (j.contains("bools")) m_Bools = j["bools"].get<std::unordered_map<std::string, bool>>();
    if (j.contains("triggers")) m_Triggers = j["triggers"].get<std::unordered_map<std::string, bool>>();
}

} // namespace animation
} // namespace lupine
