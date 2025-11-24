#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/asset/AudioAsset.hpp"
#include "lupine/core/UUID.hpp"
#include <string>

namespace lupine {
namespace components {

/**
 * AudioPlayer Component
 * Plays audio files with support for autoplay, looping, and bus assignment
 *
 * Features:
 * - Autoplay on scene start
 * - Loop or one-shot playback
 * - Volume, pitch, and pan control
 * - Audio bus assignment for mixing
 * - 2D/3D audio support
 */
class AudioPlayer : public core::Component {
public:
    AudioPlayer();
    virtual ~AudioPlayer();

    // Component interface
    std::string GetTypeName() const override { return "AudioPlayer"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnDestroy() override;
    void OnUpdate(float deltaTime) override;

    // Playback control
    void Play();
    void Stop();
    void Pause();
    void Resume();
    bool IsPlaying() const;

    // Audio asset
    void SetAudioAsset(const asset::AssetRef<asset::AudioAsset>& audioAsset);
    const asset::AssetRef<asset::AudioAsset>& GetAudioAsset() const { return m_AudioAsset; }

    // Properties
    void SetAutoplay(bool autoplay) { m_Autoplay = autoplay; }
    bool GetAutoplay() const { return m_Autoplay; }

    void SetLoop(bool loop);
    bool GetLoop() const { return m_Loop; }

    void SetVolume(float volume);
    float GetVolume() const { return m_Volume; }

    void SetPitch(float pitch);
    float GetPitch() const { return m_Pitch; }

    void SetPan(float pan);
    float GetPan() const { return m_Pan; }

    void SetBus(const std::string& busName);
    const std::string& GetBus() const { return m_BusName; }

    void SetIs3D(bool is3D) { m_Is3D = is3D; }
    bool GetIs3D() const { return m_Is3D; }

    // For 3D audio
    void SetMinDistance(float distance);
    float GetMinDistance() const { return m_MinDistance; }

    void SetMaxDistance(float distance);
    float GetMaxDistance() const { return m_MaxDistance; }

    void SetRolloffFactor(float factor);
    float GetRolloffFactor() const { return m_RolloffFactor; }

private:
    // Audio asset
    asset::AssetRef<asset::AudioAsset> m_AudioAsset;

    // Playback settings
    bool m_Autoplay{false};
    bool m_Loop{false};
    float m_Volume{1.0f};
    float m_Pitch{1.0f};
    float m_Pan{0.0f};
    std::string m_BusName{"Master"};

    // 3D audio settings
    bool m_Is3D{false};
    float m_MinDistance{1.0f};
    float m_MaxDistance{100.0f};
    float m_RolloffFactor{1.0f};

    // Runtime state
    core::UUID m_PlayingSourceUUID;  // UUID of currently playing source
    bool m_WasPlaying{false};        // Track playing state for pause/resume
};

} // namespace components
} // namespace lupine
