/**
 * @file AnimationComponentTests.cpp
 * @brief Headless behavioral tests for the animation/tween COMPONENTS:
 *        AnimationPlayer, AnimationTree, and TweenSequence.
 *
 * These drive the CPU-side playback/state/config layer of each component without a
 * renderer or window. Clips are supplied inline (no asset files), pose application
 * targets a plain Node2D via the same channel vocabulary the Tween tests use, and
 * time is advanced by explicit OnUpdate() ticks so playback positions and state
 * transitions are fully deterministic.
 *
 * The plain interpolation/easing math is covered by AnimationInterpTests.cpp and the
 * single-channel Tween component by TweenTests.cpp; this suite does not duplicate
 * those and focuses on the component-level public API surface.
 */

#include "lupine/engine/Engine.hpp"
#include "lupine/components/AnimationPlayer.hpp"
#include "lupine/components/AnimationTree.hpp"
#include "lupine/components/TweenSequence.hpp"
#include "lupine/components/Tween.hpp"
#include "lupine/animation/AnimationData.hpp"
#include "lupine/animation/AnimationTypes.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/math/Math.hpp"
#include "TestFramework.hpp"
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <cmath>

using namespace lupine;
using namespace lupine::core;
using namespace lupine::components;

namespace {

bool FloatEq(float a, float b, float tol = 1e-3f) {
    return std::fabs(a - b) < tol;
}

animation::AnimationClip MakePositionClip(const std::string& name, float length,
                                          animation::LoopMode loopMode,
                                          float endX) {
    animation::AnimationClip clip;
    clip.name = name;
    clip.length = length;
    clip.fps = 30.0f;
    clip.loopMode = loopMode;

    animation::Track track;
    track.type = animation::TrackType::Value;
    track.node = "";
    track.channel = "position_2d";
    track.valueType = animation::TrackValueType::Vec2;
    track.interp = animation::InterpolationMode::Linear;

    animation::Keyframe k0;
    k0.time = 0.0f;
    k0.value = nlohmann::json::array({ 0.0, 0.0 });

    animation::Keyframe k1;
    k1.time = length;
    k1.value = nlohmann::json::array({ static_cast<double>(endX), 0.0 });

    track.keys.push_back(k0);
    track.keys.push_back(k1);
    clip.tracks.push_back(track);
    return clip;
}

std::shared_ptr<AnimationPlayer> MakePlayerOn(const std::shared_ptr<Node2D>& node) {
    std::shared_ptr<AnimationPlayer> player = std::make_shared<AnimationPlayer>();
    player->RegisterProperties();
    node->AddComponent(player);
    return player;
}

// ===========================================================================
// AnimationPlayer
// ===========================================================================

bool TestPlayerClipLibrary() {
    TEST_SECTION("AnimationPlayer: Clip Library");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("P");
    std::shared_ptr<AnimationPlayer> player = MakePlayerOn(node);

    TEST_ASSERT(!player->HasClip("walk"), "No clip present before any are added");

    player->AddClip("walk", "res://anims/walk.animclip");
    TEST_ASSERT(player->GetClipLibrary().size() == 1, "AddClip records a library entry");
    TEST_ASSERT(player->GetClipLibrary()[0].first == "walk", "Library entry name preserved");
    TEST_ASSERT(player->GetClipLibrary()[0].second == "res://anims/walk.animclip",
                "Library entry path preserved");

    player->AddClip("walk", "res://anims/walk2.animclip");
    TEST_ASSERT(player->GetClipLibrary().size() == 1, "Re-adding a name updates in place, not appends");
    TEST_ASSERT(player->GetClipLibrary()[0].second == "res://anims/walk2.animclip",
                "Re-adding a name overwrites its path");

    player->AddInlineClip(MakePositionClip("idle", 1.0f, animation::LoopMode::None, 100.0f));
    TEST_ASSERT(player->HasClip("idle"), "Inline clip is reported by HasClip");
    const animation::AnimationClip* idle = player->GetClip("idle");
    TEST_ASSERT(idle != nullptr && idle->name == "idle", "GetClip returns the inline clip");
    TEST_ASSERT(FloatEq(idle->EffectiveLength(), 1.0f), "Inline clip reports its length");

    std::vector<std::string> list = player->GetAnimationList();
    bool hasIdle = false;
    for (const std::string& n : list) {
        if (n == "idle") hasIdle = true;
    }
    TEST_ASSERT(hasIdle, "GetAnimationList includes the loaded inline clip");

    player->AddInlineClip(MakePositionClip("idle", 2.0f, animation::LoopMode::None, 50.0f));
    const animation::AnimationClip* updated = player->GetClip("idle");
    TEST_ASSERT(updated != nullptr && FloatEq(updated->EffectiveLength(), 2.0f),
                "Re-adding an inline clip by name replaces it");

    player->RemoveClip("idle");
    TEST_ASSERT(!player->HasClip("idle"), "RemoveClip removes the inline clip");
    player->RemoveClip("walk");
    TEST_ASSERT(player->GetClipLibrary().empty(), "RemoveClip removes the library entry");

    return true;
}

bool TestPlayerPlaybackState() {
    TEST_SECTION("AnimationPlayer: Playback State Transitions");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("P");
    std::shared_ptr<AnimationPlayer> player = MakePlayerOn(node);
    player->AddInlineClip(MakePositionClip("run", 1.0f, animation::LoopMode::None, 100.0f));

    TEST_ASSERT(!player->IsPlaying(), "Player is not playing before Play");
    TEST_ASSERT(player->GetCurrentAnimation().empty(), "No current animation before Play");

    player->Play("run");
    TEST_ASSERT(player->IsPlaying(), "Player is playing after Play");
    TEST_ASSERT(player->GetCurrentAnimation() == "run", "Current animation set by Play");
    TEST_ASSERT(FloatEq(player->GetCurrentTime(), 0.0f), "Play resets time to 0");
    TEST_ASSERT(FloatEq(player->GetCurrentLength(), 1.0f), "Current length reflects the playing clip");

    player->Play("missing");
    TEST_ASSERT(player->GetCurrentAnimation() == "run",
                "Play on an unknown clip is ignored and leaves the current clip");

    player->Pause();
    TEST_ASSERT(!player->IsPlaying(), "Pause halts playback");

    player->Resume();
    TEST_ASSERT(player->IsPlaying(), "Resume continues playback of the current clip");

    player->Stop(false);
    TEST_ASSERT(!player->IsPlaying(), "Stop halts playback");

    return true;
}

bool TestPlayerAdvance() {
    TEST_SECTION("AnimationPlayer: Time Advance & Pose");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("P");
    std::shared_ptr<AnimationPlayer> player = MakePlayerOn(node);
    player->AddInlineClip(MakePositionClip("run", 1.0f, animation::LoopMode::None, 100.0f));

    player->Play("run");
    player->OnUpdate(0.5f);
    TEST_ASSERT(FloatEq(player->GetCurrentTime(), 0.5f), "Time advances by deltaTime at speed 1");
    TEST_ASSERT(FloatEq(node->GetPosition().x, 50.0f), "Pose interpolated to midpoint at t=0.5");
    TEST_ASSERT(player->IsPlaying(), "Player still playing mid-clip");

    player->OnUpdate(0.6f);
    TEST_ASSERT(FloatEq(node->GetPosition().x, 100.0f), "Pose reaches end value when clip completes");
    TEST_ASSERT(!player->IsPlaying(), "Non-looping clip stops playing once finished");
    TEST_ASSERT(FloatEq(player->GetCurrentTime(), 1.0f), "Finished non-looping clip clamps time to length");

    return true;
}

bool TestPlayerSpeed() {
    TEST_SECTION("AnimationPlayer: Speed");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("P");
    std::shared_ptr<AnimationPlayer> player = MakePlayerOn(node);
    player->AddInlineClip(MakePositionClip("run", 1.0f, animation::LoopMode::None, 100.0f));

    TEST_ASSERT(FloatEq(player->GetSpeed(), 1.0f), "Default speed is 1.0");
    player->SetSpeed(2.0f);
    TEST_ASSERT(FloatEq(player->GetSpeed(), 2.0f), "Speed round-trips");

    player->Play("run");
    player->OnUpdate(0.25f);
    TEST_ASSERT(FloatEq(player->GetCurrentTime(), 0.5f), "Speed 2.0 doubles the advance per tick");
    TEST_ASSERT(FloatEq(node->GetPosition().x, 50.0f), "Pose follows the sped-up time");

    return true;
}

bool TestPlayerLoopModes() {
    TEST_SECTION("AnimationPlayer: Loop Modes");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("P");
    std::shared_ptr<AnimationPlayer> player = MakePlayerOn(node);
    player->AddInlineClip(MakePositionClip("loop", 1.0f, animation::LoopMode::Loop, 100.0f));
    player->AddInlineClip(MakePositionClip("ping", 1.0f, animation::LoopMode::PingPong, 100.0f));

    player->Play("loop");
    player->OnUpdate(1.25f);
    TEST_ASSERT(player->IsPlaying(), "Looping clip keeps playing past its length");
    TEST_ASSERT(FloatEq(player->GetCurrentTime(), 0.25f), "Looping clip wraps time back into range");

    player->Play("ping");
    player->OnUpdate(1.25f);
    TEST_ASSERT(player->IsPlaying(), "PingPong clip keeps playing past its length");
    TEST_ASSERT(FloatEq(player->GetCurrentTime(), 0.75f),
                "PingPong reflects time about the end (1.25 -> 0.75)");

    return true;
}

bool TestPlayerSeek() {
    TEST_SECTION("AnimationPlayer: Seek");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("P");
    std::shared_ptr<AnimationPlayer> player = MakePlayerOn(node);
    player->AddInlineClip(MakePositionClip("run", 1.0f, animation::LoopMode::None, 100.0f));

    player->Play("run");
    player->Seek(0.5f, true);
    TEST_ASSERT(FloatEq(player->GetCurrentTime(), 0.5f), "Seek sets the playback time");
    TEST_ASSERT(FloatEq(node->GetPosition().x, 50.0f), "Seek with update applies the pose");

    player->Seek(5.0f, false);
    TEST_ASSERT(FloatEq(player->GetCurrentTime(), 1.0f), "Seek clamps time to clip length");

    player->Seek(-5.0f, false);
    TEST_ASSERT(FloatEq(player->GetCurrentTime(), 0.0f), "Seek clamps negative time to 0");

    return true;
}

bool TestPlayerStopReset() {
    TEST_SECTION("AnimationPlayer: Stop & Reset Semantics");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("P");
    std::shared_ptr<AnimationPlayer> player = MakePlayerOn(node);
    player->AddInlineClip(MakePositionClip("run", 1.0f, animation::LoopMode::None, 100.0f));

    player->Play("run");
    player->OnUpdate(0.4f);
    player->Stop(false);
    TEST_ASSERT(!player->IsPlaying(), "Stop(false) halts playback");
    TEST_ASSERT(FloatEq(player->GetCurrentTime(), 0.4f), "Stop(false) preserves the current time");

    player->Play("run");
    player->OnUpdate(0.4f);
    player->Stop(true);
    TEST_ASSERT(!player->IsPlaying(), "Stop(true) halts playback");
    TEST_ASSERT(FloatEq(player->GetCurrentTime(), 0.0f), "Stop(true) resets the time to 0");

    return true;
}

bool TestPlayerQueue() {
    TEST_SECTION("AnimationPlayer: Playback Queue");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("P");
    std::shared_ptr<AnimationPlayer> player = MakePlayerOn(node);
    player->AddInlineClip(MakePositionClip("a", 1.0f, animation::LoopMode::None, 100.0f));
    player->AddInlineClip(MakePositionClip("b", 1.0f, animation::LoopMode::None, 100.0f));

    player->Play("a");
    player->Queue("b");
    player->OnUpdate(1.25f);
    TEST_ASSERT(player->GetCurrentAnimation() == "b",
                "Finishing a clip auto-advances to the queued clip");
    TEST_ASSERT(player->IsPlaying(), "Queued clip begins playing on advance");

    player->Stop(true);
    player->Play("a");
    player->Queue("b");
    player->ClearQueue();
    player->OnUpdate(1.25f);
    TEST_ASSERT(player->GetCurrentAnimation() == "a",
                "ClearQueue prevents auto-advance; current clip stays put");
    TEST_ASSERT(!player->IsPlaying(), "With an empty queue the finished clip just stops");

    return true;
}

bool TestPlayerBackwards() {
    TEST_SECTION("AnimationPlayer: Play Backwards");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("P");
    std::shared_ptr<AnimationPlayer> player = MakePlayerOn(node);
    player->AddInlineClip(MakePositionClip("run", 1.0f, animation::LoopMode::None, 100.0f));

    player->PlayBackwards("run");
    TEST_ASSERT(player->IsPlaying(), "PlayBackwards starts playback");
    TEST_ASSERT(FloatEq(player->GetCurrentTime(), 1.0f), "PlayBackwards starts at the clip end");

    player->OnUpdate(0.25f);
    TEST_ASSERT(FloatEq(player->GetCurrentTime(), 0.75f), "Backward playback decreases time");
    TEST_ASSERT(FloatEq(node->GetPosition().x, 75.0f), "Backward pose follows decreasing time");

    return true;
}

bool TestPlayerProperties() {
    TEST_SECTION("AnimationPlayer: Properties");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("P");
    std::shared_ptr<AnimationPlayer> player = MakePlayerOn(node);

    player->SetRootNodePath("Child/Sub");
    TEST_ASSERT(player->GetRootNodePath() == "Child/Sub", "Root node path round-trips");

    player->SetAutoPlay("intro");
    TEST_ASSERT(player->GetAutoPlay() == "intro", "Auto-play name round-trips");

    player->SetDefaultBlendTime(0.3f);
    TEST_ASSERT(FloatEq(player->GetDefaultBlendTime(), 0.3f), "Default blend time round-trips");

    TEST_ASSERT(!player->IsControlledByTree(), "Player is not tree-controlled by default");
    player->SetControlledByTree(true);
    TEST_ASSERT(player->IsControlledByTree(), "Tree-controlled flag round-trips");
    player->SetControlledByTree(false);

    return true;
}

bool TestPlayerControlledByTree() {
    TEST_SECTION("AnimationPlayer: Tree Control Suppresses Self-Playback");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("P");
    std::shared_ptr<AnimationPlayer> player = MakePlayerOn(node);
    player->AddInlineClip(MakePositionClip("run", 1.0f, animation::LoopMode::None, 100.0f));

    player->Play("run");
    player->SetControlledByTree(true);
    player->OnUpdate(0.5f);
    TEST_ASSERT(FloatEq(player->GetCurrentTime(), 0.0f),
                "A tree-controlled player does not self-advance on OnUpdate");
    TEST_ASSERT(FloatEq(node->GetPosition().x, 0.0f),
                "A tree-controlled player does not apply its own pose");

    return true;
}

bool TestPlayerSerialization() {
    TEST_SECTION("AnimationPlayer: Serialization Round-Trip");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("P");
    std::shared_ptr<AnimationPlayer> player = MakePlayerOn(node);
    player->AddClip("walk", "res://anims/walk.animclip");
    player->AddInlineClip(MakePositionClip("idle", 1.5f, animation::LoopMode::Loop, 30.0f));

    nlohmann::json json = player->Serialize();
    TEST_ASSERT(json.contains("clips") && json["clips"].is_array(),
                "Serialized player contains a clips array");
    TEST_ASSERT(json.contains("inlineClips") && json["inlineClips"].is_array(),
                "Serialized player contains an inlineClips array");

    std::shared_ptr<Node2D> node2 = std::make_shared<Node2D>("P2");
    std::shared_ptr<AnimationPlayer> restored = MakePlayerOn(node2);
    restored->Deserialize(json);
    TEST_ASSERT(restored->GetClipLibrary().size() == 1, "Deserialize restores the library entry");
    TEST_ASSERT(restored->GetClipLibrary()[0].first == "walk", "Deserialized library name preserved");
    TEST_ASSERT(restored->HasClip("idle"), "Deserialize restores the inline clip");
    const animation::AnimationClip* idle = restored->GetClip("idle");
    TEST_ASSERT(idle != nullptr && FloatEq(idle->EffectiveLength(), 1.5f),
                "Deserialized inline clip length preserved");

    return true;
}

// ===========================================================================
// AnimationTree
// ===========================================================================

std::shared_ptr<AnimationTree> MakeTreeOn(const std::shared_ptr<Node2D>& node) {
    std::shared_ptr<AnimationTree> tree = std::make_shared<AnimationTree>();
    tree->RegisterProperties();
    node->AddComponent(tree);
    return tree;
}

bool TestTreeParameters() {
    TEST_SECTION("AnimationTree: Parameters");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("T");
    std::shared_ptr<AnimationTree> tree = MakeTreeOn(node);

    tree->SetFloat("speed", 2.5f);
    TEST_ASSERT(FloatEq(tree->GetFloat("speed"), 2.5f), "Float parameter round-trips");

    tree->SetInt("phase", 3);
    TEST_ASSERT(tree->GetInt("phase") == 3, "Int parameter round-trips");

    tree->SetBool("grounded", true);
    TEST_ASSERT(tree->GetBool("grounded"), "Bool parameter round-trips");
    tree->SetBool("grounded", false);
    TEST_ASSERT(!tree->GetBool("grounded"), "Bool parameter can be cleared");

    TEST_ASSERT(FloatEq(tree->GetFloat("undeclared"), 0.0f),
                "Unset float parameter reads as 0");
    TEST_ASSERT(tree->GetInt("undeclared") == 0, "Unset int parameter reads as 0");
    TEST_ASSERT(!tree->GetBool("undeclared"), "Unset bool parameter reads as false");

    return true;
}

bool TestTreeTriggers() {
    TEST_SECTION("AnimationTree: Triggers & Parameter Store");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("T");
    std::shared_ptr<AnimationTree> tree = MakeTreeOn(node);

    animation::AnimationParameters& params = tree->GetParameters();
    TEST_ASSERT(!params.Has("jump"), "Trigger absent before being set");

    tree->SetTrigger("jump");
    TEST_ASSERT(params.IsTriggerSet("jump"), "SetTrigger latches the trigger true");
    TEST_ASSERT(params.Has("jump"), "Trigger is now present in the store");
    TEST_ASSERT(tree->GetBool("jump"), "A set trigger reads as a true bool");

    params.ResetTrigger("jump");
    TEST_ASSERT(!params.IsTriggerSet("jump"), "ResetTrigger clears the trigger");

    return true;
}

bool TestTreeParameterTypeCoercion() {
    TEST_SECTION("AnimationTree: Parameter Type Coercion");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("T");
    std::shared_ptr<AnimationTree> tree = MakeTreeOn(node);

    tree->SetInt("count", 7);
    TEST_ASSERT(FloatEq(tree->GetFloat("count"), 7.0f), "Int reads back as float");

    tree->SetBool("flag", true);
    TEST_ASSERT(FloatEq(tree->GetFloat("flag"), 1.0f), "True bool reads back as 1.0 float");

    tree->SetFloat("approx", 2.6f);
    TEST_ASSERT(tree->GetInt("approx") == 3, "Float reads back as a rounded int");

    return true;
}

bool TestTreeStateControl() {
    TEST_SECTION("AnimationTree: State Control Without Graph");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("T");
    std::shared_ptr<AnimationTree> tree = MakeTreeOn(node);

    TEST_ASSERT(tree->GetCurrentState(0).empty(),
                "With no graph there is no current state on layer 0");
    TEST_ASSERT(tree->GetCurrentState(5).empty(),
                "An out-of-range layer yields an empty current state");

    tree->Travel("Run", 0, 0.2f);
    TEST_ASSERT(tree->GetCurrentState(0).empty(),
                "Travel on an empty graph is a safe no-op");

    TEST_ASSERT(tree->GetGraph().layers.empty(), "Empty graph has no layers");
    TEST_ASSERT(tree->GetGraph().parameters.empty(), "Empty graph has no declared parameters");

    return true;
}

bool TestTreeProperties() {
    TEST_SECTION("AnimationTree: Properties");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("T");
    std::shared_ptr<AnimationTree> tree = MakeTreeOn(node);

    TEST_ASSERT(tree->GetActive(), "AnimationTree is active by default");
    tree->SetActive(false);
    TEST_ASSERT(!tree->GetActive(), "Active flag round-trips");
    tree->SetActive(true);

    tree->SetGraphPath("res://anims/player.animgraph");
    TEST_ASSERT(tree->GetGraphPath() == "res://anims/player.animgraph", "Graph path round-trips");

    tree->SetAnimationPlayerPath("../Player");
    TEST_ASSERT(tree->GetAnimationPlayerPath() == "../Player", "Animation player path round-trips");

    tree->SetRootNodePath("Skeleton");
    TEST_ASSERT(tree->GetRootNodePath() == "Skeleton", "Root node path round-trips");

    return true;
}

bool TestTreeInactiveUpdate() {
    TEST_SECTION("AnimationTree: Inactive Update Is A No-Op");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("T");
    std::shared_ptr<AnimationTree> tree = MakeTreeOn(node);

    tree->SetActive(false);
    tree->OnUpdate(0.5f);
    TEST_ASSERT(FloatEq(node->GetPosition().x, 0.0f),
                "An inactive tree writes nothing to the scene");

    tree->SetActive(true);
    tree->OnUpdate(0.5f);
    TEST_ASSERT(FloatEq(node->GetPosition().x, 0.0f),
                "An active tree with no layers also writes nothing");

    return true;
}

bool TestTreeCallMethod() {
    TEST_SECTION("AnimationTree: CallMethod Dispatch");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("T");
    std::shared_ptr<AnimationTree> tree = MakeTreeOn(node);

    tree->CallMethod("set_float", nlohmann::json::array({ "blend", 0.8 }));
    nlohmann::json got = tree->CallMethod("get_float", nlohmann::json::array({ "blend" }));
    TEST_ASSERT(got.is_number() && FloatEq(static_cast<float>(got.get<double>()), 0.8f),
                "set_float / get_float via CallMethod round-trip");

    tree->CallMethod("set_int", nlohmann::json::array({ "n", 4 }));
    nlohmann::json gotInt = tree->CallMethod("get_int", nlohmann::json::array({ "n" }));
    TEST_ASSERT(gotInt.is_number() && gotInt.get<int>() == 4, "set_int / get_int via CallMethod round-trip");

    tree->CallMethod("set_bool", nlohmann::json::array({ "b", true }));
    nlohmann::json gotBool = tree->CallMethod("get_bool", nlohmann::json::array({ "b" }));
    TEST_ASSERT(gotBool.is_boolean() && gotBool.get<bool>(), "set_bool / get_bool via CallMethod round-trip");

    tree->CallMethod("set_active", nlohmann::json::array({ false }));
    nlohmann::json active = tree->CallMethod("is_active", nlohmann::json::array());
    TEST_ASSERT(active.is_boolean() && !active.get<bool>(), "set_active / is_active via CallMethod round-trip");
    tree->SetActive(true);

    nlohmann::json state = tree->CallMethod("get_current_state", nlohmann::json::array({ 0 }));
    TEST_ASSERT(state.is_string() && state.get<std::string>().empty(),
                "get_current_state via CallMethod returns the empty state with no graph");

    return true;
}

bool TestTreeSerialization() {
    TEST_SECTION("AnimationTree: Serialization Round-Trip");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("T");
    std::shared_ptr<AnimationTree> tree = MakeTreeOn(node);
    tree->SetFloat("speed", 1.5f);
    tree->SetInt("phase", 2);
    tree->SetBool("grounded", true);

    nlohmann::json json = tree->Serialize();
    TEST_ASSERT(json.contains("parameters"), "Serialized tree contains a parameters object");

    std::shared_ptr<Node2D> node2 = std::make_shared<Node2D>("T2");
    std::shared_ptr<AnimationTree> restored = MakeTreeOn(node2);
    restored->Deserialize(json);
    TEST_ASSERT(FloatEq(restored->GetFloat("speed"), 1.5f), "Deserialized float parameter preserved");
    TEST_ASSERT(restored->GetInt("phase") == 2, "Deserialized int parameter preserved");
    TEST_ASSERT(restored->GetBool("grounded"), "Deserialized bool parameter preserved");

    return true;
}

// ===========================================================================
// TweenSequence
// ===========================================================================

std::shared_ptr<TweenSequence> MakeSequenceOn(const std::shared_ptr<Node2D>& node) {
    std::shared_ptr<TweenSequence> seq = std::make_shared<TweenSequence>();
    seq->RegisterProperties();
    node->AddComponent(seq);
    return seq;
}

bool TestSequenceBuilder() {
    TEST_SECTION("TweenSequence: Builder & Step Count");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("S");
    std::shared_ptr<TweenSequence> seq = MakeSequenceOn(node);

    TEST_ASSERT(seq->GetStepCount() == 0, "New sequence has no steps");

    seq->AppendInterval(1.0f, false);
    TEST_ASSERT(seq->GetStepCount() == 1, "AppendInterval adds a step");

    seq->AppendTween(nullptr, "position_2d", nlohmann::json::array({ 50.0, 0.0 }), 1.0f, "linear", false);
    TEST_ASSERT(seq->GetStepCount() == 2, "AppendTween adds a step");

    seq->AppendCallback(nullptr, "on_done", false);
    TEST_ASSERT(seq->GetStepCount() == 3, "AppendCallback adds a step");

    seq->AppendTween(nullptr, "rotation_2d", 90.0, 0.5f, "quad_in", true);
    TEST_ASSERT(seq->GetStepCount() == 4, "Parallel AppendTween still adds a step");

    return true;
}

bool TestSequenceConfig() {
    TEST_SECTION("TweenSequence: Config");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("S");
    std::shared_ptr<TweenSequence> seq = MakeSequenceOn(node);

    TEST_ASSERT(seq->GetLoops() == 1, "Default loop count is 1");
    seq->SetLoops(3);
    TEST_ASSERT(seq->GetLoops() == 3, "Loop count round-trips");
    seq->SetLoops(-1);
    TEST_ASSERT(seq->GetLoops() == -1, "Infinite loop count (-1) round-trips");

    TEST_ASSERT(!seq->GetAutoRemove(), "Auto-remove defaults to false");
    seq->SetAutoRemove(true);
    TEST_ASSERT(seq->GetAutoRemove(), "Auto-remove round-trips");
    seq->SetAutoRemove(false);

    TEST_ASSERT(!seq->IsRunning(), "Sequence is not running before Play");
    TEST_ASSERT(!seq->IsFinished(), "Sequence is not finished before Play");

    return true;
}

bool TestSequenceEmptyPlay() {
    TEST_SECTION("TweenSequence: Empty Sequence Finishes Immediately");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("S");
    std::shared_ptr<TweenSequence> seq = MakeSequenceOn(node);

    seq->Play();
    TEST_ASSERT(!seq->IsRunning(), "An empty sequence is not left running");
    TEST_ASSERT(seq->IsFinished(), "An empty sequence finishes immediately on Play");

    return true;
}

bool TestSequenceIntervalProgression() {
    TEST_SECTION("TweenSequence: Sequential Interval Phases");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("S");
    std::shared_ptr<TweenSequence> seq = MakeSequenceOn(node);
    seq->AppendInterval(1.0f, false);
    seq->AppendInterval(1.0f, false);

    seq->Play();
    TEST_ASSERT(seq->IsRunning(), "Sequence is running after Play with steps");
    TEST_ASSERT(!seq->IsFinished(), "Sequence is not finished while phases remain");

    seq->OnUpdate(0.5f);
    TEST_ASSERT(seq->IsRunning(), "First interval phase still active mid-interval");

    seq->OnUpdate(0.6f);
    TEST_ASSERT(seq->IsRunning(), "Sequence advances into the second phase, still running");
    TEST_ASSERT(!seq->IsFinished(), "Sequence not finished after only the first phase");

    seq->OnUpdate(1.1f);
    TEST_ASSERT(!seq->IsRunning(), "Sequence stops running once all phases complete");
    TEST_ASSERT(seq->IsFinished(), "Sequence reports finished after the final phase");

    return true;
}

bool TestSequenceParallelPhase() {
    TEST_SECTION("TweenSequence: Parallel Steps Share A Phase");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("S");
    std::shared_ptr<TweenSequence> seq = MakeSequenceOn(node);
    seq->AppendInterval(1.0f, false);
    seq->AppendInterval(2.0f, true);

    seq->Play();
    seq->OnUpdate(1.5f);
    TEST_ASSERT(seq->IsRunning(),
                "Phase stays active while a parallel interval is still counting down");

    seq->OnUpdate(0.6f);
    TEST_ASSERT(seq->IsFinished(),
                "Phase completes only when every parallel step in it completes");

    return true;
}

bool TestSequenceLooping() {
    TEST_SECTION("TweenSequence: Finite Looping");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("S");
    std::shared_ptr<TweenSequence> seq = MakeSequenceOn(node);
    seq->SetLoops(2);
    seq->AppendInterval(1.0f, false);

    seq->Play();
    seq->OnUpdate(1.1f);
    TEST_ASSERT(seq->IsRunning(), "Sequence restarts for the second loop instead of finishing");
    TEST_ASSERT(!seq->IsFinished(), "Sequence not finished during its first-to-second loop");

    seq->OnUpdate(1.1f);
    TEST_ASSERT(!seq->IsRunning(), "Sequence stops after the configured number of loops");
    TEST_ASSERT(seq->IsFinished(), "Sequence finishes after the last loop");

    return true;
}

bool TestSequenceStopReset() {
    TEST_SECTION("TweenSequence: Stop / Reset / Restart");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("S");
    std::shared_ptr<TweenSequence> seq = MakeSequenceOn(node);
    seq->AppendInterval(1.0f, false);
    seq->AppendInterval(1.0f, false);

    seq->Play();
    seq->OnUpdate(0.5f);
    seq->Stop();
    TEST_ASSERT(!seq->IsRunning(), "Stop halts the sequence");

    seq->OnUpdate(2.0f);
    TEST_ASSERT(!seq->IsRunning(), "A stopped sequence does not advance on update");

    seq->Reset();
    TEST_ASSERT(!seq->IsRunning() && !seq->IsFinished(),
                "Reset returns the sequence to an idle, unfinished state");

    seq->Restart();
    TEST_ASSERT(seq->IsRunning(), "Restart begins the sequence again");
    TEST_ASSERT(!seq->IsFinished(), "Restart clears the finished flag");

    return true;
}

bool TestSequenceTweenStep() {
    TEST_SECTION("TweenSequence: Tween Step Drives The Target");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("S");
    std::shared_ptr<TweenSequence> seq = MakeSequenceOn(node);
    seq->AppendTween(nullptr, "position_2d", nlohmann::json::array({ 50.0, 0.0 }), 1.0f, "linear", false);

    seq->Play();
    TEST_ASSERT(seq->IsRunning(), "Sequence is running after starting a tween phase");

    std::shared_ptr<Tween> child = node->GetComponent<Tween>();
    TEST_ASSERT(child != nullptr, "Tween step spawns a child Tween component on the target");

    child->OnUpdate(0.5f);
    TEST_ASSERT(FloatEq(node->GetPosition().x, 25.0f),
                "Ticking the spawned tween interpolates the target at the midpoint");
    seq->OnUpdate(0.0f);
    TEST_ASSERT(seq->IsRunning(), "Sequence still running while the spawned tween is unfinished");

    child->OnUpdate(0.6f);
    TEST_ASSERT(FloatEq(node->GetPosition().x, 50.0f),
                "Completing the spawned tween reaches the target value");
    seq->OnUpdate(0.0f);
    TEST_ASSERT(seq->IsFinished(),
                "Sequence finishes once its single tween phase completes");

    return true;
}

} // namespace

void RunAnimationComponentTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "ANIMATION COMPONENT TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    lupine_test::SetCurrentSuite("AnimationComponents");

    engine::InitializeEngine();

    bool allPassed = true;

    allPassed &= TestPlayerClipLibrary();
    allPassed &= TestPlayerPlaybackState();
    allPassed &= TestPlayerAdvance();
    allPassed &= TestPlayerSpeed();
    allPassed &= TestPlayerLoopModes();
    allPassed &= TestPlayerSeek();
    allPassed &= TestPlayerStopReset();
    allPassed &= TestPlayerQueue();
    allPassed &= TestPlayerBackwards();
    allPassed &= TestPlayerProperties();
    allPassed &= TestPlayerControlledByTree();
    allPassed &= TestPlayerSerialization();

    allPassed &= TestTreeParameters();
    allPassed &= TestTreeTriggers();
    allPassed &= TestTreeParameterTypeCoercion();
    allPassed &= TestTreeStateControl();
    allPassed &= TestTreeProperties();
    allPassed &= TestTreeInactiveUpdate();
    allPassed &= TestTreeCallMethod();
    allPassed &= TestTreeSerialization();

    allPassed &= TestSequenceBuilder();
    allPassed &= TestSequenceConfig();
    allPassed &= TestSequenceEmptyPlay();
    allPassed &= TestSequenceIntervalProgression();
    allPassed &= TestSequenceParallelPhase();
    allPassed &= TestSequenceLooping();
    allPassed &= TestSequenceStopReset();
    allPassed &= TestSequenceTweenStep();

    std::cout << "\n========================================" << std::endl;
    if (allPassed) {
        std::cout << "ALL ANIMATION COMPONENT TESTS PASSED!" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================\n" << std::endl;
}
