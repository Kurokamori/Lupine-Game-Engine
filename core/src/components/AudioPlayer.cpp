#include "lupine/components/AudioPlayer.hpp"
#include "lupine/audio/AudioManager.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/math/MathCommon.hpp"

namespace lupine {
namespace components {

AudioPlayer::AudioPlayer() {
}

AudioPlayer::~AudioPlayer() {

    Stop();
}

void AudioPlayer::DefineProperties() {

    DefineProperty(PROPERTY_FILE_GROUP(audioAsset, std::string(""), "*.wav,*.mp3,*.ogg,*.flac", "Audio"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(autoplay, Bool, false, "Playback"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(loop, Bool, false, "Playback"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(volume, 1.0f, 0.0f, 1.0f, 0.01f, "Playback"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(pitch, 1.0f, 0.5f, 2.0f, 0.01f, "Playback"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(pan, 0.0f, -1.0f, 1.0f, 0.01f, "Playback"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(bus, String, std::string("Master"), "Playback"));

    DefineProperty(PROPERTY_DEFAULT_GROUP(is3D, Bool, false, "3D Audio"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(minDistance, 1.0f, 0.1f, 100.0f, 0.1f, "3D Audio"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(maxDistance, 100.0f, 1.0f, 1000.0f, 1.0f, "3D Audio"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(rolloffFactor, 1.0f, 0.0f, 10.0f, 0.1f, "3D Audio"));
}

void AudioPlayer::OnAwake() {

    m_Autoplay = GetPropertyValue<bool>("autoplay");
    m_Loop = GetPropertyValue<bool>("loop");
    m_Volume = GetPropertyValue<float>("volume");
    m_Pitch = GetPropertyValue<float>("pitch");
    m_Pan = GetPropertyValue<float>("pan");
    m_BusName = GetPropertyValue<std::string>("bus");
    m_Is3D = GetPropertyValue<bool>("is3D");
    m_MinDistance = GetPropertyValue<float>("minDistance");
    m_MaxDistance = GetPropertyValue<float>("maxDistance");
    m_RolloffFactor = GetPropertyValue<float>("rolloffFactor");

    std::string assetPath = GetPropertyValue<std::string>("audioAsset");
    if (!assetPath.empty()) {

        auto* audioAsset = new asset::AudioAsset();
        if (audioAsset->LoadFromFile(assetPath, asset::AudioLoadMode::Preload)) {
            m_AudioAsset = asset::AssetRef<asset::AudioAsset>(audioAsset);

        } else {

            audioAsset->Release();
        }
    }
}

void AudioPlayer::OnReady() {

    if (m_Autoplay && m_AudioAsset) {
        auto* node = GetOwner();
        if (node) {
            core::Scene* scene = node->GetScene();

            if (scene && !scene->IsInEditor()) {
                Play();
            }
        }
    }
}

void AudioPlayer::OnDestroy() {

    Stop();
}

void AudioPlayer::OnUpdate(float deltaTime) {

    if (m_Is3D && m_PlayingSourceUUID.IsValid()) {
        auto* node = GetOwner();
        if (node) {

            math::Vec3 position(0.0f, 0.0f, 0.0f);

            if (auto* node3d = dynamic_cast<core::Node3D*>(node)) {
                position = node3d->GetGlobalPosition();
            }

            else if (auto* node2d = dynamic_cast<core::Node2D*>(node)) {
                math::Vec2 pos2d = node2d->GetGlobalPosition();
                position = math::Vec3(pos2d.x, pos2d.y, 0.0f);
            }

            audio::AudioManager::GetInstance().SetSourcePosition(m_PlayingSourceUUID, position);
        }
    }
}

void AudioPlayer::Play() {
    if (!m_AudioAsset || !m_AudioAsset->IsLoaded()) {

        return;
    }

    Stop();

    audio::AudioManager& audioMgr = audio::AudioManager::GetInstance();

    audio::PlaybackMode mode = m_Loop ? audio::PlaybackMode::Loop : audio::PlaybackMode::OneShot;

    if (m_Is3D) {
        auto* node = GetOwner();
        math::Vec3 position(0.0f, 0.0f, 0.0f);

        if (node) {

            if (auto* node3d = dynamic_cast<core::Node3D*>(node)) {
                position = node3d->GetGlobalPosition();
            }

            else if (auto* node2d = dynamic_cast<core::Node2D*>(node)) {
                math::Vec2 pos2d = node2d->GetGlobalPosition();
                position = math::Vec3(pos2d.x, pos2d.y, 0.0f);
            }
        }

        m_PlayingSourceUUID = audioMgr.Play3D(
            m_AudioAsset,
            position,
            m_BusName,
            mode,
            m_Volume
        );

        if (m_PlayingSourceUUID.IsValid()) {
            auto* source = audioMgr.GetSource(m_PlayingSourceUUID);
            if (source) {
                source->minDistance = m_MinDistance;
                source->maxDistance = m_MaxDistance;
                source->rolloffFactor = m_RolloffFactor;
            }
        }
    } else {
        m_PlayingSourceUUID = audioMgr.Play(
            m_AudioAsset,
            m_BusName,
            mode,
            m_Volume
        );

        if (m_PlayingSourceUUID.IsValid()) {
            audioMgr.SetSourcePitch(m_PlayingSourceUUID, m_Pitch);
            audioMgr.SetSourcePan(m_PlayingSourceUUID, m_Pan);
        }
    }

    m_WasPlaying = m_PlayingSourceUUID.IsValid();
}

void AudioPlayer::Stop() {
    if (m_PlayingSourceUUID.IsValid()) {
        audio::AudioManager::GetInstance().Stop(m_PlayingSourceUUID);
        m_PlayingSourceUUID = core::UUID(0);
        m_WasPlaying = false;
    }
}

void AudioPlayer::Pause() {
    if (m_PlayingSourceUUID.IsValid()) {
        audio::AudioManager::GetInstance().Pause(m_PlayingSourceUUID);
        m_WasPlaying = true;
    }
}

void AudioPlayer::Resume() {
    if (m_PlayingSourceUUID.IsValid() && m_WasPlaying) {
        audio::AudioManager::GetInstance().Resume(m_PlayingSourceUUID);
    }
}

bool AudioPlayer::IsPlaying() const {
    if (!m_PlayingSourceUUID.IsValid()) {
        return false;
    }

    const audio::AudioSource* source = audio::AudioManager::GetInstance().GetSource(m_PlayingSourceUUID);
    return source && source->state == audio::PlaybackState::Playing;
}

void AudioPlayer::SetAudioAsset(const asset::AssetRef<asset::AudioAsset>& audioAsset) {
    m_AudioAsset = audioAsset;

    if (audioAsset) {
        SetPropertyValue("audioAsset", audioAsset->GetPath());
    }
}

void AudioPlayer::SetLoop(bool loop) {
    m_Loop = loop;
    SetPropertyValue("loop", loop);

    if (IsPlaying()) {
        Play();
    }
}

void AudioPlayer::SetVolume(float volume) {
    m_Volume = math::Clamp(volume, 0.0f, 1.0f);
    SetPropertyValue("volume", m_Volume);

    if (m_PlayingSourceUUID.IsValid()) {
        audio::AudioManager::GetInstance().SetSourceVolume(m_PlayingSourceUUID, m_Volume);
    }
}

void AudioPlayer::SetPitch(float pitch) {
    m_Pitch = math::Clamp(pitch, 0.5f, 2.0f);
    SetPropertyValue("pitch", m_Pitch);

    if (m_PlayingSourceUUID.IsValid() && !m_Is3D) {
        audio::AudioManager::GetInstance().SetSourcePitch(m_PlayingSourceUUID, m_Pitch);
    }
}

void AudioPlayer::SetPan(float pan) {
    m_Pan = math::Clamp(pan, -1.0f, 1.0f);
    SetPropertyValue("pan", m_Pan);

    if (m_PlayingSourceUUID.IsValid() && !m_Is3D) {
        audio::AudioManager::GetInstance().SetSourcePan(m_PlayingSourceUUID, m_Pan);
    }
}

void AudioPlayer::SetBus(const std::string& busName) {
    m_BusName = busName;
    SetPropertyValue("bus", busName);

    if (m_PlayingSourceUUID.IsValid()) {
        audio::AudioManager::GetInstance().SetSourceBus(m_PlayingSourceUUID, busName);
    }
}

void AudioPlayer::SetMinDistance(float distance) {
    m_MinDistance = math::Max(distance, 0.1f);
    SetPropertyValue("minDistance", m_MinDistance);

    if (m_PlayingSourceUUID.IsValid() && m_Is3D) {
        auto* source = audio::AudioManager::GetInstance().GetSource(m_PlayingSourceUUID);
        if (source) {
            source->minDistance = m_MinDistance;
        }
    }
}

void AudioPlayer::SetMaxDistance(float distance) {
    m_MaxDistance = math::Max(distance, 1.0f);
    SetPropertyValue("maxDistance", m_MaxDistance);

    if (m_PlayingSourceUUID.IsValid() && m_Is3D) {
        auto* source = audio::AudioManager::GetInstance().GetSource(m_PlayingSourceUUID);
        if (source) {
            source->maxDistance = m_MaxDistance;
        }
    }
}

void AudioPlayer::SetRolloffFactor(float factor) {
    m_RolloffFactor = math::Clamp(factor, 0.0f, 10.0f);
    SetPropertyValue("rolloffFactor", m_RolloffFactor);

    if (m_PlayingSourceUUID.IsValid() && m_Is3D) {
        auto* source = audio::AudioManager::GetInstance().GetSource(m_PlayingSourceUUID);
        if (source) {
            source->rolloffFactor = m_RolloffFactor;
        }
    }
}

}
}
