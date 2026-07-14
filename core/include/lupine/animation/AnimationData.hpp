#pragma once

#include "lupine/animation/AnimationTypes.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace lupine {
namespace animation {

/**
 * Animation data model.
 *
 * Plain data structures describing keyframe clips and the blend-tree/state-machine
 * graph, with JSON (de)serialization matching the .animclip / .animgraph formats.
 * The runtime (ClipSampler, BlendTree, AnimationStateMachine) consumes these; the
 * components and assets own them. Values cross as nlohmann::json (a number for
 * scalar channels, an array for vector channels) exactly like the Tween channel
 * model, so the same NodeRef channel read/write path applies them.
 */

// ---------------------------------------------------------------------------
// Keyframe
// ---------------------------------------------------------------------------

struct Keyframe {
    float time = 0.0f;

    // Value tracks: a number (scalar) or array (vector). Method tracks ignore it.
    nlohmann::json value;

    // Per-component cubic Bezier tangents, only used when the track interpolation
    // is Cubic. Each is sized to the value's component count (slope, value units
    // per second). Empty => treated as a flat (linear-ish) tangent.
    std::vector<float> inTangent;
    std::vector<float> outTangent;

    // Method tracks: the method to invoke on the target node and its JSON args.
    std::string method;
    nlohmann::json args = nlohmann::json::array();

    nlohmann::json Serialize(TrackType trackType) const;
    static Keyframe Deserialize(const nlohmann::json& j, TrackType trackType);
};

// ---------------------------------------------------------------------------
// Track
// ---------------------------------------------------------------------------

struct Track {
    TrackType type = TrackType::Value;

    // Target node, resolved relative to the player's root node. "" or "." targets
    // the root itself; "%Unique" and "Child/Sub" follow NodeRef::FindNode rules.
    std::string node;

    // Stable scene-wide identity of the target node (hex UUID string). Authored by
    // the editor so a track survives the node being moved/renamed in the scene tree:
    // resolution prefers this UUID and only falls back to `node` when it cannot be
    // found (empty, or a different prefab instance). Empty for legacy/portable clips.
    std::string nodeUuid;

    // Channel name: transform channels ("position_2d", "rotation_3d", ...) or any
    // component/node property name, matching the Tween channel vocabulary.
    std::string channel;

    TrackValueType valueType = TrackValueType::Float;
    InterpolationMode interp = InterpolationMode::Linear;
    bool useSlerp = false;

    std::vector<Keyframe> keys;

    nlohmann::json Serialize() const;
    static Track Deserialize(const nlohmann::json& j);

    // Sort keys ascending by time (callers should keep tracks sorted).
    void SortKeys();

    // Stable per-target identity used as a pose accumulator key and mask token.
    std::string TargetKey() const;
};

// ---------------------------------------------------------------------------
// AnimationClip
// ---------------------------------------------------------------------------

struct AnimationClip {
    std::string name;
    float length = 1.0f;     // Seconds. 0 => derived from the last keyframe.
    float fps = 30.0f;       // Editor sampling rate (display only).
    LoopMode loopMode = LoopMode::None;
    std::vector<Track> tracks;

    nlohmann::json Serialize() const;
    static AnimationClip Deserialize(const nlohmann::json& j);

    // Largest keyframe time across all tracks (0 if empty).
    float ComputeLength() const;

    // length if > 0, otherwise ComputeLength().
    float EffectiveLength() const;
};

// ---------------------------------------------------------------------------
// Blend tree
// ---------------------------------------------------------------------------

struct BlendNode;

struct BlendChild {
    std::shared_ptr<BlendNode> node;

    float threshold = 0.0f;       // Blend1D position along the parameter axis.
    float posX = 0.0f;            // Blend2D position (X).
    float posY = 0.0f;            // Blend2D position (Y).
    std::string parameter;        // Direct blend: per-child weight parameter.
};

struct BlendNode {
    BlendNodeType type = BlendNodeType::Clip;

    // Clip leaf.
    std::string clip;             // Clip name resolved from the linked player.
    std::string clipPath;         // Optional direct res:// .animclip path.
    float speed = 1.0f;

    // Blend parameters. Blend1D/Blend2/Add2/TimeScale use parameterX; Blend2D uses
    // parameterX (X axis) and parameterY (Y axis); OneShot uses parameter (trigger).
    std::string parameterX;
    std::string parameterY;
    std::string parameter;

    Blend2DMode blend2dMode = Blend2DMode::FreeformDirectional;

    // OneShot fade times (seconds).
    float fadeIn = 0.2f;
    float fadeOut = 0.2f;

    std::vector<BlendChild> children;

    nlohmann::json Serialize() const;
    static std::shared_ptr<BlendNode> Deserialize(const nlohmann::json& j);
};

// ---------------------------------------------------------------------------
// State machine
// ---------------------------------------------------------------------------

struct TransitionCondition {
    std::string parameter;
    ConditionOp op = ConditionOp::Greater;
    float value = 0.0f;           // Comparison threshold (ignored for IsTrue/IsFalse).

    nlohmann::json Serialize() const;
    static TransitionCondition Deserialize(const nlohmann::json& j);
};

struct StateTransition {
    std::string from;             // "" or "Any" => evaluated from any state.
    std::string to;
    bool hasExitTime = false;
    float exitTime = 1.0f;        // Normalized [0,1] phase the source must reach.
    float duration = 0.2f;        // Cross-fade seconds.
    int priority = 0;             // Higher priority transitions are tested first.
    std::vector<TransitionCondition> conditions;

    bool IsAnyState() const;

    nlohmann::json Serialize() const;
    static StateTransition Deserialize(const nlohmann::json& j);
};

struct AnimationState {
    std::string name;
    bool isBlendTree = false;

    std::string clip;             // Clip-state: clip name from the linked player.
    std::shared_ptr<BlendNode> blendTree;  // Blend-tree state root node.

    float speed = 1.0f;
    float posX = 0.0f;            // Editor canvas layout.
    float posY = 0.0f;

    nlohmann::json Serialize() const;
    static AnimationState Deserialize(const nlohmann::json& j);
};

struct StateMachineData {
    std::string entry;            // Name of the default state.
    std::vector<AnimationState> states;
    std::vector<StateTransition> transitions;

    const AnimationState* FindState(const std::string& name) const;

    nlohmann::json Serialize() const;
    static StateMachineData Deserialize(const nlohmann::json& j);
};

// ---------------------------------------------------------------------------
// Layers + graph
// ---------------------------------------------------------------------------

struct LayerMask {
    MaskMode mode = MaskMode::Include;
    // Target tokens: exact "node\x1Fchannel" keys or bare node paths (matching any
    // channel on that node). Empty include-list => affect everything.
    std::vector<std::string> targets;

    // True when a layer with this mask should write the given target.
    bool Allows(const std::string& node, const std::string& channel) const;

    nlohmann::json Serialize() const;
    static LayerMask Deserialize(const nlohmann::json& j);
};

struct AnimationLayer {
    std::string name = "Layer";
    float weight = 1.0f;
    LayerBlendMode blendMode = LayerBlendMode::Override;
    LayerMask mask;
    StateMachineData stateMachine;

    nlohmann::json Serialize() const;
    static AnimationLayer Deserialize(const nlohmann::json& j);
};

struct AnimationParameter {
    std::string name;
    ParamType type = ParamType::Float;
    float defFloat = 0.0f;
    int defInt = 0;
    bool defBool = false;

    nlohmann::json Serialize() const;
    static AnimationParameter Deserialize(const nlohmann::json& j);
};

struct AnimationGraph {
    int version = 1;
    std::vector<AnimationParameter> parameters;
    std::vector<AnimationLayer> layers;

    const AnimationParameter* FindParameter(const std::string& name) const;

    nlohmann::json Serialize() const;
    static AnimationGraph Deserialize(const nlohmann::json& j);
};

// ---------------------------------------------------------------------------
// Runtime parameter store
// ---------------------------------------------------------------------------

/**
 * Live parameter values for an AnimationTree. Seeded from a graph's declared
 * parameters and mutated at runtime by scripts / the editor. Triggers latch true
 * until consumed by a satisfied transition. Persisted with the component so a
 * scene round-trips its current parameter state.
 */
class AnimationParameters {
public:
    void InitFromGraph(const AnimationGraph& graph);

    void SetFloat(const std::string& name, float value);
    void SetInt(const std::string& name, int value);
    void SetBool(const std::string& name, bool value);
    void SetTrigger(const std::string& name);
    void ResetTrigger(const std::string& name);

    float GetFloat(const std::string& name) const;
    int GetInt(const std::string& name) const;
    bool GetBool(const std::string& name) const;
    bool IsTriggerSet(const std::string& name) const;

    bool Has(const std::string& name) const;

    // Evaluate a single condition against the current values. Consuming triggers is
    // deferred to ConsumeTriggersFor so a transition only fires when ALL conditions
    // pass.
    bool EvaluateCondition(const TransitionCondition& cond) const;

    // Reset every trigger a transition's conditions reference (called when it fires).
    void ConsumeTriggersFor(const std::vector<TransitionCondition>& conditions);

    nlohmann::json Serialize() const;
    void Deserialize(const nlohmann::json& j);

private:
    std::unordered_map<std::string, float> m_Floats;
    std::unordered_map<std::string, int> m_Ints;
    std::unordered_map<std::string, bool> m_Bools;
    std::unordered_map<std::string, bool> m_Triggers;
};

} // namespace animation
} // namespace lupine
