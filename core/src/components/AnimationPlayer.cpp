#include "lupine/components/AnimationPlayer.hpp"
#include "lupine/animation/ClipSampler.hpp"
#include "lupine/animation/AnimationChannel.hpp"
#include "lupine/asset/AnimationClipAsset.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/scripting/NodeRef.hpp"
#include <algorithm>

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

AnimationPlayer::AnimationPlayer()
    : Component("AnimationPlayer") {
}

AnimationPlayer::AnimationPlayer(const std::string& name)
    : Component(name) {
}

AnimationPlayer::~AnimationPlayer() {
}

void AnimationPlayer::DefineProperties() {
    DefineProperty(PROPERTY_DEFAULT_GROUP(rootNode, NodePath, std::string(""), "Animation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(autoPlay, String, std::string(""), "Animation"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(speed, 1.0f, -8.0f, 8.0f, 0.05f, "Animation"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(defaultBlendTime, 0.0f, 0.0f, 10.0f, 0.05f, "Animation"));
}

void AnimationPlayer::DefineSignals() {
    RegisterSignal({"animation_started", {{"name", core::PropertyValueType::String}}, "Emitted when an animation starts."});
    RegisterSignal({"animation_finished", {{"name", core::PropertyValueType::String}}, "Emitted when a non-looping animation completes."});
    RegisterSignal({"animation_changed", {{"old", core::PropertyValueType::String}, {"new", core::PropertyValueType::String}}, "Emitted when the current animation changes."});
    RegisterSignal({"animation_looped", {{"name", core::PropertyValueType::String}}, "Emitted when a looping animation wraps."});
    RegisterSignal({"method_track", {}, "Emitted when a method track key is crossed (node, method, args)."});
}

nlohmann::json AnimationPlayer::Serialize() const {
    nlohmann::json j = Component::Serialize();
    nlohmann::json clips = nlohmann::json::array();
    for (const auto& entry : m_ClipLibrary) {
        clips.push_back({ {"name", entry.first}, {"path", entry.second} });
    }
    j["clips"] = clips;
    nlohmann::json inlineClips = nlohmann::json::array();
    for (const AnimationClip& clip : m_InlineClips) {
        inlineClips.push_back(clip.Serialize());
    }
    j["inlineClips"] = inlineClips;
    return j;
}

void AnimationPlayer::Deserialize(const nlohmann::json& json) {
    Component::Deserialize(json);
    m_ClipLibrary.clear();
    m_InlineClips.clear();
    if (json.contains("clips") && json["clips"].is_array()) {
        for (const nlohmann::json& c : json["clips"]) {
            m_ClipLibrary.emplace_back(c.value("name", std::string()), c.value("path", std::string()));
        }
    }
    if (json.contains("inlineClips") && json["inlineClips"].is_array()) {
        for (const nlohmann::json& c : json["inlineClips"]) {
            m_InlineClips.push_back(AnimationClip::Deserialize(c));
        }
    }
    m_ClipsLoaded = false;
}

void AnimationPlayer::OnAwake() {
    EnsureClipsLoaded();
}

void AnimationPlayer::OnReady() {
    std::string autoPlay = GetAutoPlay();
    if (!autoPlay.empty() && !m_ControlledByTree) {
        Play(autoPlay);
    }
}

void AnimationPlayer::OnUpdate(float deltaTime) {
    if (m_ControlledByTree) return;
    EnsureClipsLoaded();
    if (!m_Playing || m_CurrentClip.empty()) return;

    const AnimationClip* clip = GetClip(m_CurrentClip);
    if (!clip) {
        m_Playing = false;
        return;
    }

    const float len = clip->EffectiveLength();
    const float speed = GetSpeed();
    const float dir = m_Backwards ? -1.0f : 1.0f;

    m_PrevTime = m_Time;
    m_Time += deltaTime * speed * dir;

    bool looped = false;
    bool finished = false;
    HandleTimeBounds(len, clip->loopMode, looped, finished);

    AnimationPose pose;
    if (m_FadeActive) {
        const AnimationClip* fromClip = GetClip(m_FadeFromClip);
        m_FadeFromTime += deltaTime * speed;
        m_FadeTime += deltaTime;
        float t = m_FadeDuration > 0.0f ? std::clamp(m_FadeTime / m_FadeDuration, 0.0f, 1.0f) : 1.0f;
        if (fromClip) {
            float flen = fromClip->EffectiveLength();
            float ft = flen > 0.0f ? std::clamp(m_FadeFromTime, 0.0f, flen) : 0.0f;
            ClipSampler::Sample(*fromClip, ft, 1.0f - t, pose);
        }
        ClipSampler::Sample(*clip, m_Time, t, pose);
        if (m_FadeTime >= m_FadeDuration) {
            m_FadeActive = false;
        }
    } else {
        ClipSampler::Sample(*clip, m_Time, 1.0f, pose);
    }
    ApplyPose(pose.Finalize());

    // Method tracks fire only on forward playback.
    if (!m_Backwards && speed >= 0.0f) {
        if (looped) {
            FireMethodCalls(*clip, m_PrevTime, len);
            FireMethodCalls(*clip, -1.0f, m_Time);
        } else {
            FireMethodCalls(*clip, m_PrevTime, m_Time);
        }
    }

    if (finished) {
        m_Playing = false;
        Emit("animation_finished", { m_CurrentClip });
        AdvanceQueue();
    } else if (looped) {
        Emit("animation_looped", { m_CurrentClip });
    }
}

nlohmann::json AnimationPlayer::CallMethod(const std::string& method, const nlohmann::json& args) {
    if (method == "play") {
        Play(ArgStr(args, 0), args.is_array() && args.size() > 1 ? ArgFloat(args, 1, -1.0f) : -1.0f);
    } else if (method == "play_backwards") {
        PlayBackwards(ArgStr(args, 0), args.is_array() && args.size() > 1 ? ArgFloat(args, 1, -1.0f) : -1.0f);
    } else if (method == "stop") {
        Stop(ArgBool(args, 0, true));
    } else if (method == "pause") {
        Pause();
    } else if (method == "resume") {
        Resume();
    } else if (method == "seek") {
        Seek(ArgFloat(args, 0), ArgBool(args, 1, true));
    } else if (method == "queue") {
        Queue(ArgStr(args, 0));
    } else if (method == "clear_queue") {
        ClearQueue();
    } else if (method == "is_playing") {
        return m_Playing;
    } else if (method == "current_animation") {
        return m_CurrentClip;
    } else if (method == "current_time") {
        return m_Time;
    } else if (method == "has_animation") {
        return HasClip(ArgStr(args, 0));
    } else if (method == "get_animation_list") {
        return GetAnimationList();
    } else if (method == "set_speed") {
        SetSpeed(ArgFloat(args, 0, 1.0f));
    } else if (method == "get_speed") {
        return GetSpeed();
    } else if (method == "sample_at") {
        SampleAt(ArgStr(args, 0), ArgFloat(args, 1));
    } else if (method == "add_clip") {
        AddClip(ArgStr(args, 0), ArgStr(args, 1));
    } else if (method == "remove_clip") {
        RemoveClip(ArgStr(args, 0));
    } else if (method == "reload_clips") {
        ReloadClips();
    } else if (method == "get_clip_library") {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& entry : m_ClipLibrary) {
            arr.push_back({ {"name", entry.first}, {"path", entry.second} });
        }
        for (const AnimationClip& clip : m_InlineClips) {
            arr.push_back({ {"name", clip.name}, {"path", ""} });
        }
        return arr;
    }
    return nlohmann::json();
}

// ===== Clip library =====

void AnimationPlayer::AddClip(const std::string& name, const std::string& path) {
    for (auto& entry : m_ClipLibrary) {
        if (entry.first == name) {
            entry.second = path;
            m_ClipsLoaded = false;
            return;
        }
    }
    m_ClipLibrary.emplace_back(name, path);
    m_ClipsLoaded = false;
}

void AnimationPlayer::AddInlineClip(const AnimationClip& clip) {
    for (AnimationClip& c : m_InlineClips) {
        if (c.name == clip.name) {
            c = clip;
            m_ClipsLoaded = false;
            return;
        }
    }
    m_InlineClips.push_back(clip);
    m_ClipsLoaded = false;
}

void AnimationPlayer::RemoveClip(const std::string& name) {
    m_ClipLibrary.erase(
        std::remove_if(m_ClipLibrary.begin(), m_ClipLibrary.end(),
            [&](const std::pair<std::string, std::string>& e) { return e.first == name; }),
        m_ClipLibrary.end());
    m_InlineClips.erase(
        std::remove_if(m_InlineClips.begin(), m_InlineClips.end(),
            [&](const AnimationClip& c) { return c.name == name; }),
        m_InlineClips.end());
    m_ClipsLoaded = false;
}

bool AnimationPlayer::HasClip(const std::string& name) {
    EnsureClipsLoaded();
    if (m_LoadedClips.find(name) != m_LoadedClips.end()) return true;
    for (const auto& entry : m_ClipLibrary) {
        if (entry.first == name) return true;
    }
    for (const AnimationClip& clip : m_InlineClips) {
        if (clip.name == name) return true;
    }
    return false;
}

std::vector<std::string> AnimationPlayer::GetAnimationList() {
    EnsureClipsLoaded();
    std::vector<std::string> names;
    names.reserve(m_LoadedClips.size());
    for (const auto& entry : m_LoadedClips) {
        names.push_back(entry.first);
    }
    return names;
}

const AnimationClip* AnimationPlayer::GetClip(const std::string& name) {
    EnsureClipsLoaded();
    auto it = m_LoadedClips.find(name);
    return it != m_LoadedClips.end() ? &it->second : nullptr;
}

void AnimationPlayer::ReloadClips() {
    m_ClipsLoaded = false;
    EnsureClipsLoaded();
}

void AnimationPlayer::EnsureClipsLoaded() {
    if (m_ClipsLoaded) return;
    m_LoadedClips.clear();
    for (const auto& entry : m_ClipLibrary) {
        if (entry.second.empty()) continue;
        asset::AnimationClipAsset clipAsset;
        if (clipAsset.LoadFromFile(entry.second)) {
            m_LoadedClips[entry.first] = clipAsset.GetClip();
        }
    }
    for (const AnimationClip& clip : m_InlineClips) {
        m_LoadedClips[clip.name] = clip;
    }
    m_ClipsLoaded = true;
}

// ===== Playback =====

void AnimationPlayer::Play(const std::string& name, float blend) {
    EnsureClipsLoaded();
    if (!HasClip(name)) return;

    float blendTime = blend >= 0.0f ? blend : GetDefaultBlendTime();
    std::string previous = m_CurrentClip;

    if (blendTime > 0.0f && !m_CurrentClip.empty() && m_CurrentClip != name && m_Playing) {
        m_FadeActive = true;
        m_FadeFromClip = m_CurrentClip;
        m_FadeFromTime = m_Time;
        m_FadeDuration = blendTime;
        m_FadeTime = 0.0f;
    } else {
        m_FadeActive = false;
    }

    m_CurrentClip = name;
    m_Time = 0.0f;
    m_PrevTime = 0.0f;
    m_Backwards = false;
    m_Playing = true;

    if (previous != name) {
        Emit("animation_changed", { previous, name });
    }
    Emit("animation_started", { name });
}

void AnimationPlayer::PlayBackwards(const std::string& name, float blend) {
    Play(name, blend);
    m_Backwards = true;
    m_Time = GetCurrentLength();
    m_PrevTime = m_Time;
}

void AnimationPlayer::Stop(bool reset) {
    m_Playing = false;
    m_FadeActive = false;
    if (reset) {
        m_Time = 0.0f;
        m_PrevTime = 0.0f;
    }
}

void AnimationPlayer::Pause() {
    m_Playing = false;
}

void AnimationPlayer::Resume() {
    if (!m_CurrentClip.empty()) {
        m_Playing = true;
    }
}

void AnimationPlayer::Seek(float time, bool update) {
    float len = GetCurrentLength();
    m_Time = len > 0.0f ? std::clamp(time, 0.0f, len) : 0.0f;
    m_PrevTime = m_Time;
    if (update && !m_CurrentClip.empty()) {
        const AnimationClip* clip = GetClip(m_CurrentClip);
        if (clip) {
            AnimationPose pose;
            ClipSampler::Sample(*clip, m_Time, 1.0f, pose);
            ApplyPose(pose.Finalize());
        }
    }
}

void AnimationPlayer::Queue(const std::string& name) {
    m_Queue.push_back(name);
}

void AnimationPlayer::ClearQueue() {
    m_Queue.clear();
}

float AnimationPlayer::GetCurrentLength() {
    const AnimationClip* clip = GetClip(m_CurrentClip);
    return clip ? clip->EffectiveLength() : 0.0f;
}

void AnimationPlayer::SampleAt(const std::string& name, float time) {
    EnsureClipsLoaded();
    const AnimationClip* clip = GetClip(name);
    if (!clip) return;
    float len = clip->EffectiveLength();
    float t = len > 0.0f ? std::clamp(time, 0.0f, len) : 0.0f;
    AnimationPose pose;
    ClipSampler::Sample(*clip, t, 1.0f, pose);
    ApplyPose(pose.Finalize());
}

void AnimationPlayer::AdvanceQueue() {
    if (!m_Queue.empty()) {
        std::string next = m_Queue.front();
        m_Queue.pop_front();
        Play(next);
    }
}

// ===== Helpers =====

void AnimationPlayer::HandleTimeBounds(float len, LoopMode mode, bool& looped, bool& finished) {
    if (len <= 0.0f) {
        m_Time = 0.0f;
        return;
    }
    if (m_Time > len) {
        switch (mode) {
            case LoopMode::Loop:
                while (m_Time > len) m_Time -= len;
                looped = true;
                break;
            case LoopMode::PingPong:
                m_Backwards = true;
                m_Time = len - (m_Time - len);
                if (m_Time < 0.0f) m_Time = 0.0f;
                looped = true;
                break;
            case LoopMode::None:
                m_Time = len;
                finished = true;
                break;
        }
    } else if (m_Time < 0.0f) {
        switch (mode) {
            case LoopMode::Loop:
                while (m_Time < 0.0f) m_Time += len;
                looped = true;
                break;
            case LoopMode::PingPong:
                m_Backwards = false;
                m_Time = -m_Time;
                if (m_Time > len) m_Time = len;
                looped = true;
                break;
            case LoopMode::None:
                m_Time = 0.0f;
                finished = true;
                break;
        }
    }
}

core::Node* AnimationPlayer::ResolveRoot() const {
    core::Node* owner = GetOwner();
    if (!owner) return nullptr;
    std::string rootPath = GetRootNodePath();
    if (rootPath.empty() || rootPath == ".") {
        return owner;
    }
    scripting::NodeRef ref = scripting::NodeRef::FromRaw(owner, nullptr).FindNode(rootPath);
    return ref.IsValid() ? ref.Lock().get() : owner;
}

void AnimationPlayer::ApplyPose(const TargetValueMap& map) const {
    animation::ApplyTargetValues(ResolveRoot(), map);
}

void AnimationPlayer::FireMethodCalls(const AnimationClip& clip, float fromTime, float toTime) {
    std::vector<MethodCall> calls;
    ClipSampler::CollectMethodCalls(clip, fromTime, toTime, calls);
    if (calls.empty()) return;

    core::Node* root = ResolveRoot();

    for (const MethodCall& call : calls) {
        Emit("method_track", { call.node, call.method, call.args });

        scripting::NodeRef tr = animation::ResolveTargetNode(root, call.node, call.nodeUuid);
        core::Node* target = tr.IsValid() ? tr.Lock().get() : nullptr;
        if (target && !call.method.empty()) {
            target->InvokeSignalMethod(call.method, ToSignalArgs(call.args));
        }
    }
}

// ===== Property accessors =====

std::string AnimationPlayer::GetRootNodePath() const { return GetPropertyValue<std::string>("rootNode"); }
void AnimationPlayer::SetRootNodePath(const std::string& path) { SetPropertyValue<std::string>("rootNode", path); }
std::string AnimationPlayer::GetAutoPlay() const { return GetPropertyValue<std::string>("autoPlay"); }
void AnimationPlayer::SetAutoPlay(const std::string& name) { SetPropertyValue<std::string>("autoPlay", name); }
float AnimationPlayer::GetSpeed() const { return GetPropertyValue<float>("speed"); }
void AnimationPlayer::SetSpeed(float speed) { SetPropertyValue<float>("speed", speed); }
float AnimationPlayer::GetDefaultBlendTime() const { return GetPropertyValue<float>("defaultBlendTime"); }
void AnimationPlayer::SetDefaultBlendTime(float t) { SetPropertyValue<float>("defaultBlendTime", t); }

} // namespace components
} // namespace lupine
