#pragma once

#include <string>

namespace lupine {
namespace animation {

/**
 * Animation type system.
 *
 * Shared enumerations for the keyframe animation + blend-tree runtime. These are
 * serialized to/from .animclip and .animgraph files as stable lowercase strings
 * (see AnimationData.cpp), so renaming an enumerator changes the on-disk format.
 */

// A track either drives a value channel or fires a method call.
enum class TrackType {
    Value,
    Method
};

// How values between two keyframes are interpolated.
enum class InterpolationMode {
    Constant,   // Step: hold the previous key's value until the next key.
    Linear,     // Straight-line interpolation between keys.
    Cubic       // Per-component cubic Bezier using each key's in/out tangents.
};

// Clip-level loop behaviour.
enum class LoopMode {
    None,       // Play once and hold the final value.
    Loop,       // Wrap time back to the start.
    PingPong    // Alternate forward and backward each cycle.
};

// The value shape a value-track produces, controlling how it is interpolated and
// blended (e.g. Quat blends via slerp, Bool/Int are discrete).
enum class TrackValueType {
    Float,
    Vec2,
    Vec3,
    Vec4,
    Color,
    Bool,
    Int,
    Quat,
    String   // Discrete (max-weight) - strings, enums, node/scene paths.
};

// Blend-tree node kinds.
enum class BlendNodeType {
    Clip,       // Leaf: plays a single clip.
    Blend1D,    // Blends children along one parameter via thresholds.
    Blend2D,    // Blends children across a 2D parameter space.
    Direct,     // Each child weighted by its own parameter.
    Blend2,     // Lerp between two children by a parameter.
    Add2,       // Additive: base + child*parameter.
    OneShot,    // Plays an additive child once when triggered.
    TimeScale   // Scales the playback speed of a single child.
};

// 2D blend weighting algorithm (Unity's four 2D blend types).
enum class Blend2DMode {
    SimpleDirectional,
    FreeformDirectional,
    FreeformCartesian,
    Direct
};

// How a layer composites onto the layers beneath it.
enum class LayerBlendMode {
    Override,   // Lerp toward the layer's pose by the layer weight.
    Additive    // Add the layer's delta (vs. its reference frame) by the weight.
};

// Whether a layer mask lists the targets it affects or the targets it excludes.
enum class MaskMode {
    Include,
    Exclude
};

// Graph parameter value kinds.
enum class ParamType {
    Float,
    Int,
    Bool,
    Trigger     // A bool that auto-resets to false once it satisfies a transition.
};

// Transition condition comparison operators.
enum class ConditionOp {
    Greater,
    Less,
    Equal,
    NotEqual,
    IsTrue,     // Bool/Trigger parameter is true.
    IsFalse     // Bool parameter is false.
};

// ---------------------------------------------------------------------------
// String conversions (stable on-disk tokens). Defined in AnimationData.cpp.
// Parsing falls back to the first enumerator for unknown tokens.
// ---------------------------------------------------------------------------

const char* ToString(TrackType v);
const char* ToString(InterpolationMode v);
const char* ToString(LoopMode v);
const char* ToString(TrackValueType v);
const char* ToString(BlendNodeType v);
const char* ToString(Blend2DMode v);
const char* ToString(LayerBlendMode v);
const char* ToString(MaskMode v);
const char* ToString(ParamType v);
const char* ToString(ConditionOp v);

TrackType        TrackTypeFromString(const std::string& s);
InterpolationMode InterpolationModeFromString(const std::string& s);
LoopMode         LoopModeFromString(const std::string& s);
TrackValueType   TrackValueTypeFromString(const std::string& s);
BlendNodeType    BlendNodeTypeFromString(const std::string& s);
Blend2DMode      Blend2DModeFromString(const std::string& s);
LayerBlendMode   LayerBlendModeFromString(const std::string& s);
MaskMode         MaskModeFromString(const std::string& s);
ParamType        ParamTypeFromString(const std::string& s);
ConditionOp      ConditionOpFromString(const std::string& s);

// Number of components a value type carries (Float/Bool/Int -> 1, Vec2 -> 2, ...).
int ValueComponentCount(TrackValueType type);

// Infer a reasonable value type from a channel name (e.g. "position_3d" -> Vec3,
// "rotation_3d" -> Quat, "modulate" -> Color). Used when a track omits valueType.
TrackValueType InferValueTypeFromChannel(const std::string& channel);

// True when a channel should blend rotations via slerp ("rotation_3d").
bool ChannelUsesSlerp(const std::string& channel);

} // namespace animation
} // namespace lupine
