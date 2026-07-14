#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/animation/AnimationData.hpp"
#include "lupine/animation/AnimationPose.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <utility>
#include <unordered_map>
#include <deque>

namespace lupine {
namespace components {

/**
 * AnimationPlayer
 *
 * Plays keyframe animation clips, driving any node/component property or transform
 * channel relative to a configurable root node. Holds a clip library (named
 * .animclip references plus optional inline clips), supports cross-fade blending
 * between clips, a playback queue, looping/ping-pong, and method/event tracks.
 *
 * The runtime backing for direct keyframe playback; an
 * AnimationTree can also sample this player's clips by name to feed its blend
 * trees, in which case the player yields pose control to the tree.
 */
class AnimationPlayer : public core::Component {
public:
    AnimationPlayer();
    explicit AnimationPlayer(const std::string& name);
    ~AnimationPlayer() override;

    std::string GetTypeName() const override { return "AnimationPlayer"; }
    void DefineProperties() override;
    void DefineSignals() override;

    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& json) override;

    void OnAwake() override;
    void OnReady() override;
    void OnUpdate(float deltaTime) override;

    nlohmann::json CallMethod(const std::string& method, const nlohmann::json& args) override;

    // ===== Clip library =====
    void AddClip(const std::string& name, const std::string& path);
    void AddInlineClip(const animation::AnimationClip& clip);
    void RemoveClip(const std::string& name);
    bool HasClip(const std::string& name);
    std::vector<std::string> GetAnimationList();
    const animation::AnimationClip* GetClip(const std::string& name);
    void ReloadClips();
    const std::vector<std::pair<std::string, std::string>>& GetClipLibrary() const { return m_ClipLibrary; }

    // ===== Playback =====
    void Play(const std::string& name, float blend = -1.0f);
    void PlayBackwards(const std::string& name, float blend = -1.0f);
    void Stop(bool reset = true);
    void Pause();
    void Resume();
    void Seek(float time, bool update = true);
    void Queue(const std::string& name);
    void ClearQueue();
    bool IsPlaying() const { return m_Playing; }
    std::string GetCurrentAnimation() const { return m_CurrentClip; }
    float GetCurrentTime() const { return m_Time; }
    float GetCurrentLength();

    // Editor preview: apply a clip's pose at an absolute time to the scene.
    void SampleAt(const std::string& name, float time);

    // ===== Properties =====
    std::string GetRootNodePath() const;
    void SetRootNodePath(const std::string& path);
    std::string GetAutoPlay() const;
    void SetAutoPlay(const std::string& name);
    float GetSpeed() const;
    void SetSpeed(float speed);
    float GetDefaultBlendTime() const;
    void SetDefaultBlendTime(float t);

    // When an AnimationTree owns this player it suppresses self-driven playback.
    void SetControlledByTree(bool controlled) { m_ControlledByTree = controlled; }
    bool IsControlledByTree() const { return m_ControlledByTree; }

private:
    void EnsureClipsLoaded();
    core::Node* ResolveRoot() const;
    void ApplyPose(const animation::TargetValueMap& map) const;
    void FireMethodCalls(const animation::AnimationClip& clip, float fromTime, float toTime);
    void HandleTimeBounds(float length, animation::LoopMode loopMode, bool& looped, bool& finished);
    void AdvanceQueue();

    std::vector<std::pair<std::string, std::string>> m_ClipLibrary;
    std::vector<animation::AnimationClip> m_InlineClips;
    std::unordered_map<std::string, animation::AnimationClip> m_LoadedClips;
    bool m_ClipsLoaded = false;

    std::string m_CurrentClip;
    float m_Time = 0.0f;
    float m_PrevTime = 0.0f;
    bool m_Playing = false;
    bool m_Backwards = false;

    bool m_FadeActive = false;
    std::string m_FadeFromClip;
    float m_FadeFromTime = 0.0f;
    float m_FadeDuration = 0.0f;
    float m_FadeTime = 0.0f;

    std::deque<std::string> m_Queue;
    bool m_ControlledByTree = false;
};

} // namespace components
} // namespace lupine
