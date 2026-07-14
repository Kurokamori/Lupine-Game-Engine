#include "lupine/audio/AudioManager.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/math/MathCommon.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

namespace lupine {
namespace audio {

/**
 * Per-bus DSP node: a miniaudio custom node (one input bus, one output bus)
 * that runs a BusEffectChain over the bus's mixed audio. The ma_node_base must
 * be the first member so the node pointer can be cast back to BusEffectNode.
 */
constexpr ma_uint32 kMaxMeterChannels = 8;

struct BusEffectNode {
    ma_node_base base;
    BusEffectChain chain;
    ma_uint32 channels{2};
    // Post-fader / post-effect peak per channel, accumulated as a max on the
    // audio thread and reset to 0 when the meter reads it (so no transient
    // between UI polls is missed).
    std::atomic<float> levels[kMaxMeterChannels];

    BusEffectNode() {
        for (ma_uint32 i = 0; i < kMaxMeterChannels; ++i) {
            levels[i].store(0.0f, std::memory_order_relaxed);
        }
    }
};

namespace {

void BusEffectNode_Process(ma_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn,
                           float** ppFramesOut, ma_uint32* pFrameCountOut) {
    // For a 1-in/1-out effect node miniaudio guarantees the input and output
    // frame counts match, so we process the requested output count in place.
    (void)pFrameCountIn;
    BusEffectNode* node = reinterpret_cast<BusEffectNode*>(pNode);
    ma_uint32 frameCount = *pFrameCountOut;
    ma_uint32 channels = node->channels;
    const float* in = ppFramesIn[0];
    float* out = ppFramesOut[0];
    if (in != nullptr && out != nullptr) {
        std::memcpy(out, in, static_cast<size_t>(frameCount) * channels * sizeof(float));
        node->chain.Process(out, frameCount, channels);

        // Metering: a decaying peak per channel (instant attack, gradual release)
        // stored atomically so any number of readers can sample it without a
        // destructive read. ~0.85/block gives a roughly 100 ms fall at typical
        // device block sizes; the UI adds its own peak-hold on top.
        ma_uint32 meterChannels = (channels < kMaxMeterChannels) ? channels : kMaxMeterChannels;
        for (ma_uint32 c = 0; c < meterChannels; ++c) {
            float peak = 0.0f;
            for (ma_uint32 i = 0; i < frameCount; ++i) {
                float v = std::fabs(out[i * channels + c]);
                if (v > peak) {
                    peak = v;
                }
            }
            float decayed = node->levels[c].load(std::memory_order_relaxed) * 0.85f;
            node->levels[c].store(peak > decayed ? peak : decayed, std::memory_order_relaxed);
        }
    }
}

ma_node_vtable g_busEffectVtable = {
    BusEffectNode_Process,
    nullptr, // onGetRequiredInputFrameCount
    1,       // inputBusCount
    1,       // outputBusCount
    0        // flags
};

} // namespace

AudioManager::AudioManager() {
}

AudioManager::~AudioManager() {
    Shutdown();
}

AudioManager& AudioManager::GetInstance() {
    static AudioManager instance;
    return instance;
}

void AudioManager::Initialize() {
    if (m_Initialized) {

        return;
    }

    m_Engine = new ma_engine();

    ma_result result = ma_engine_init(nullptr, m_Engine);
    if (result != MA_SUCCESS) {

        delete m_Engine;
        m_Engine = nullptr;
        return;
    }

    {
        std::lock_guard<std::recursive_mutex> busLock(m_BusMutex);
        AudioBus masterBus;
        masterBus.name = "Master";
        masterBus.volume = 1.0f;
        masterBus.muted = false;
        masterBus.solo = false;
        m_Buses["Master"] = masterBus;
        CreateSoundGroup(m_Buses["Master"]);

        CreateBus("SFX", "Master");
        CreateBus("Music", "Master");
        CreateBus("Ambient", "Master");
    }

    ma_engine_listener_set_position(m_Engine, 0, 0.0f, 0.0f, 0.0f);
    ma_engine_listener_set_direction(m_Engine, 0, 0.0f, 0.0f, -1.0f);
    ma_engine_listener_set_world_up(m_Engine, 0, 0.0f, 1.0f, 0.0f);

    m_Initialized = true;

}

void AudioManager::Shutdown() {
    if (!m_Initialized) {
        return;
    }

    std::lock_guard<std::recursive_mutex> busLock(m_BusMutex);

    {
        std::lock_guard<std::mutex> lock(m_SourcesMutex);
        for (auto& pair : m_Sources) {
            AudioSource& source = pair.second;
            if (source.maSound) {
                ma_sound_uninit(source.maSound);
                delete source.maSound;
                source.maSound = nullptr;
            }
            if (source.maAudioBuffer) {
                ma_audio_buffer* buffer = static_cast<ma_audio_buffer*>(source.maAudioBuffer);
                ma_audio_buffer_uninit(buffer);
                delete buffer;
                source.maAudioBuffer = nullptr;
            }
        }
        m_Sources.clear();
    }

    // Uninitialize effect nodes before their sound groups and the engine.
    for (auto& pair : m_BusEffectNodes) {
        if (pair.second) {
            ma_node_uninit(&pair.second->base, nullptr);
        }
    }
    m_BusEffectNodes.clear();

    for (auto& pair : m_Buses) {
        DestroySoundGroup(pair.second);
    }
    m_Buses.clear();

    if (m_Engine) {
        ma_engine_uninit(m_Engine);
        delete m_Engine;
        m_Engine = nullptr;
    }

    m_Initialized = false;

}

core::UUID AudioManager::Play(const asset::AssetRef<asset::AudioAsset>& audioAsset,
                               const std::string& busName,
                               PlaybackMode mode,
                               float volume,
                               double startDelaySeconds) {
    if (!m_Initialized || !m_Engine) {

        return core::UUID(0);
    }

    if (!audioAsset || !audioAsset->IsLoaded()) {

        return core::UUID(0);
    }

    if (!HasBus(busName)) {

    }

    AudioSource source;
    source.uuid = core::UUID();
    source.audioAssetUUID = audioAsset->GetUUID();
    source.asset = audioAsset;
    source.state = PlaybackState::Playing;
    source.mode = mode;
    source.volume = volume;
    source.busName = HasBus(busName) ? busName : "Master";
    source.is3D = false;

    source.maSound = new ma_sound();

    ma_sound* soundGroup = nullptr;
    {
        std::lock_guard<std::recursive_mutex> busLock(m_BusMutex);
        AudioBus* bus = GetBus(source.busName);
        soundGroup = bus ? bus->maSoundGroup : nullptr;
    }

    ma_result result;
    ma_uint32 flags = 0;

    // Stream from disk for Streaming-mode assets instead of decoding fully into
    // memory (used for music/long clips). Preloaded assets play from their buffer.
    const bool streaming = audioAsset->GetLoadMode() == asset::AudioLoadMode::Streaming;
    if (streaming) {
        flags |= MA_SOUND_FLAG_STREAM;
    }

    if (audioAsset->GetLoadMode() == asset::AudioLoadMode::Preload && audioAsset->GetDataSize() > 0) {

        ma_audio_buffer_config bufferConfig = ma_audio_buffer_config_init(
            ma_format_s16,
            audioAsset->GetChannels(),
            audioAsset->GetDataSize() / sizeof(int16_t) / audioAsset->GetChannels(),
            audioAsset->GetData(),
            nullptr
        );

        ma_audio_buffer* audioBuffer = new ma_audio_buffer();
        result = ma_audio_buffer_init(&bufferConfig, audioBuffer);

        if (result != MA_SUCCESS) {

            delete audioBuffer;
            delete source.maSound;
            source.maSound = nullptr;
            return core::UUID(0);
        }

        result = ma_sound_init_from_data_source(
            m_Engine,
            audioBuffer,
            flags,
            soundGroup,
            source.maSound
        );

        if (result != MA_SUCCESS) {

            ma_audio_buffer_uninit(audioBuffer);
            delete audioBuffer;
            delete source.maSound;
            source.maSound = nullptr;
            return core::UUID(0);
        }

        source.maAudioBuffer = static_cast<void*>(audioBuffer);
    } else {

        const std::string& filePath = audioAsset->GetPath();

        result = ma_sound_init_from_file(
            m_Engine,
            filePath.c_str(),
            flags,
            soundGroup,
            nullptr,
            source.maSound
        );

        if (result != MA_SUCCESS) {

            delete source.maSound;
            source.maSound = nullptr;
            return core::UUID(0);
        }

        source.maAudioBuffer = nullptr;
    }

    if (mode == PlaybackMode::Loop) {
        ma_sound_set_looping(source.maSound, MA_TRUE);
    }

    ma_sound_set_volume(source.maSound, volume);
    ma_sound_set_pitch(source.maSound, source.pitch);
    ma_sound_set_pan(source.maSound, source.pan);

    if (startDelaySeconds > 0.0) {
        // Defer playback: leave the sound initialized but unstarted; Update()
        // starts it once the scheduled time elapses.
        source.scheduledTime = m_CurrentTime + startDelaySeconds;
        source.pendingScheduledStart = true;
        source.state = PlaybackState::Stopped;
    } else {
        result = ma_sound_start(source.maSound);
        if (result != MA_SUCCESS) {

            ma_sound_uninit(source.maSound);
            delete source.maSound;
            source.maSound = nullptr;
            return core::UUID(0);
        }
        source.state = PlaybackState::Playing;
    }

    {
        std::lock_guard<std::mutex> lock(m_SourcesMutex);
        m_Sources[source.uuid] = source;
    }

    return source.uuid;
}

core::UUID AudioManager::Play3D(const asset::AssetRef<asset::AudioAsset>& audioAsset,
                                 const math::Vec3& position,
                                 const std::string& busName,
                                 PlaybackMode mode,
                                 float volume,
                                 double startDelaySeconds) {
    if (!m_Initialized || !m_Engine) {

        return core::UUID(0);
    }

    if (!audioAsset || !audioAsset->IsLoaded()) {

        return core::UUID(0);
    }

    if (!HasBus(busName)) {

    }

    AudioSource source;
    source.uuid = core::UUID();
    source.audioAssetUUID = audioAsset->GetUUID();
    source.asset = audioAsset;
    source.state = PlaybackState::Playing;
    source.mode = mode;
    source.volume = volume;
    source.busName = HasBus(busName) ? busName : "Master";
    source.is3D = true;
    source.position = position;

    source.maSound = new ma_sound();

    ma_sound* soundGroup = nullptr;
    {
        std::lock_guard<std::recursive_mutex> busLock(m_BusMutex);
        AudioBus* bus = GetBus(source.busName);
        soundGroup = bus ? bus->maSoundGroup : nullptr;
    }

    ma_result result;
    ma_uint32 flags = 0;

    // Stream from disk for Streaming-mode assets instead of decoding fully into
    // memory (used for music/long clips). Preloaded assets play from their buffer.
    const bool streaming = audioAsset->GetLoadMode() == asset::AudioLoadMode::Streaming;
    if (streaming) {
        flags |= MA_SOUND_FLAG_STREAM;
    }

    if (audioAsset->GetLoadMode() == asset::AudioLoadMode::Preload && audioAsset->GetDataSize() > 0) {

        ma_audio_buffer_config bufferConfig = ma_audio_buffer_config_init(
            ma_format_s16,
            audioAsset->GetChannels(),
            audioAsset->GetDataSize() / sizeof(int16_t) / audioAsset->GetChannels(),
            audioAsset->GetData(),
            nullptr
        );

        ma_audio_buffer* audioBuffer = new ma_audio_buffer();
        result = ma_audio_buffer_init(&bufferConfig, audioBuffer);

        if (result != MA_SUCCESS) {

            delete audioBuffer;
            delete source.maSound;
            source.maSound = nullptr;
            return core::UUID(0);
        }

        result = ma_sound_init_from_data_source(
            m_Engine,
            audioBuffer,
            flags,
            soundGroup,
            source.maSound
        );

        if (result != MA_SUCCESS) {

            ma_audio_buffer_uninit(audioBuffer);
            delete audioBuffer;
            delete source.maSound;
            source.maSound = nullptr;
            return core::UUID(0);
        }

        source.maAudioBuffer = static_cast<void*>(audioBuffer);
    } else {

        const std::string& filePath = audioAsset->GetPath();

        result = ma_sound_init_from_file(
            m_Engine,
            filePath.c_str(),
            flags,
            soundGroup,
            nullptr,
            source.maSound
        );

        if (result != MA_SUCCESS) {

            delete source.maSound;
            source.maSound = nullptr;
            return core::UUID(0);
        }

        source.maAudioBuffer = nullptr;
    }

    if (mode == PlaybackMode::Loop) {
        ma_sound_set_looping(source.maSound, MA_TRUE);
    }

    ma_sound_set_spatialization_enabled(source.maSound, MA_TRUE);

    ma_sound_set_position(source.maSound, position.x, position.y, position.z);
    ma_sound_set_volume(source.maSound, volume);

    ma_sound_set_attenuation_model(source.maSound, ma_attenuation_model_linear);
    ma_sound_set_min_distance(source.maSound, source.minDistance);
    ma_sound_set_max_distance(source.maSound, source.maxDistance);
    ma_sound_set_rolloff(source.maSound, source.rolloffFactor);

    if (startDelaySeconds > 0.0) {
        // Defer playback: leave the sound initialized but unstarted; Update()
        // starts it once the scheduled time elapses.
        source.scheduledTime = m_CurrentTime + startDelaySeconds;
        source.pendingScheduledStart = true;
        source.state = PlaybackState::Stopped;
    } else {
        result = ma_sound_start(source.maSound);
        if (result != MA_SUCCESS) {

            ma_sound_uninit(source.maSound);
            delete source.maSound;
            source.maSound = nullptr;
            return core::UUID(0);
        }
        source.state = PlaybackState::Playing;
    }

    {
        std::lock_guard<std::mutex> lock(m_SourcesMutex);
        m_Sources[source.uuid] = source;
    }

    return source.uuid;
}

void AudioManager::Stop(const core::UUID& sourceUUID) {
    std::lock_guard<std::mutex> lock(m_SourcesMutex);
    auto it = m_Sources.find(sourceUUID);
    if (it != m_Sources.end()) {
        AudioSource& source = it->second;

        if (source.maSound) {
            ma_sound_stop(source.maSound);
            ma_sound_uninit(source.maSound);
            delete source.maSound;
            source.maSound = nullptr;
        }

        if (source.maAudioBuffer) {
            ma_audio_buffer* buffer = static_cast<ma_audio_buffer*>(source.maAudioBuffer);
            ma_audio_buffer_uninit(buffer);
            delete buffer;
            source.maAudioBuffer = nullptr;
        }

        source.state = PlaybackState::Stopped;

        m_Sources.erase(it);
    }
}

void AudioManager::Pause(const core::UUID& sourceUUID) {
    std::lock_guard<std::mutex> lock(m_SourcesMutex);
    auto it = m_Sources.find(sourceUUID);
    if (it != m_Sources.end() && it->second.state == PlaybackState::Playing) {
        AudioSource& source = it->second;
        if (source.maSound) {
            ma_sound_stop(source.maSound);
        }
        source.state = PlaybackState::Paused;

    }
}

void AudioManager::Resume(const core::UUID& sourceUUID) {
    std::lock_guard<std::mutex> lock(m_SourcesMutex);
    auto it = m_Sources.find(sourceUUID);
    if (it != m_Sources.end() && it->second.state == PlaybackState::Paused) {
        AudioSource& source = it->second;
        if (source.maSound) {
            ma_sound_start(source.maSound);
        }
        source.state = PlaybackState::Playing;

    }
}

void AudioManager::StopAll() {
    std::lock_guard<std::mutex> lock(m_SourcesMutex);
    for (auto& pair : m_Sources) {
        AudioSource& source = pair.second;
        if (source.maSound) {
            ma_sound_stop(source.maSound);
        }
        source.state = PlaybackState::Stopped;
    }

}

void AudioManager::PauseAll() {
    std::lock_guard<std::mutex> lock(m_SourcesMutex);
    for (auto& pair : m_Sources) {
        AudioSource& source = pair.second;
        if (source.state == PlaybackState::Playing && source.maSound) {
            ma_sound_stop(source.maSound);
            source.state = PlaybackState::Paused;
        }
    }

}

void AudioManager::ResumeAll() {
    std::lock_guard<std::mutex> lock(m_SourcesMutex);
    for (auto& pair : m_Sources) {
        AudioSource& source = pair.second;
        if (source.state == PlaybackState::Paused && source.maSound) {
            ma_sound_start(source.maSound);
            source.state = PlaybackState::Playing;
        }
    }

}

AudioSource* AudioManager::GetSource(const core::UUID& sourceUUID) {
    auto it = m_Sources.find(sourceUUID);
    if (it != m_Sources.end()) {
        return &it->second;
    }
    return nullptr;
}

const AudioSource* AudioManager::GetSource(const core::UUID& sourceUUID) const {
    auto it = m_Sources.find(sourceUUID);
    if (it != m_Sources.end()) {
        return &it->second;
    }
    return nullptr;
}

void AudioManager::SetSourceVolume(const core::UUID& sourceUUID, float volume) {
    std::lock_guard<std::mutex> lock(m_SourcesMutex);
    AudioSource* source = GetSource(sourceUUID);
    if (source) {
        source->volume = math::Clamp(volume, 0.0f, 1.0f);
        if (source->maSound) {
            ma_sound_set_volume(source->maSound, source->volume);
        }
    }
}

void AudioManager::SetSourcePitch(const core::UUID& sourceUUID, float pitch) {
    std::lock_guard<std::mutex> lock(m_SourcesMutex);
    AudioSource* source = GetSource(sourceUUID);
    if (source) {
        source->pitch = math::Clamp(pitch, 0.5f, 2.0f);
        if (source->maSound && !source->is3D) {
            ma_sound_set_pitch(source->maSound, source->pitch);
        }
    }
}

void AudioManager::SetSourcePan(const core::UUID& sourceUUID, float pan) {
    std::lock_guard<std::mutex> lock(m_SourcesMutex);
    AudioSource* source = GetSource(sourceUUID);
    if (source) {
        source->pan = math::Clamp(pan, -1.0f, 1.0f);
        if (source->maSound && !source->is3D) {
            ma_sound_set_pan(source->maSound, source->pan);
        }
    }
}

void AudioManager::SetSourcePosition(const core::UUID& sourceUUID, const math::Vec3& position) {
    std::lock_guard<std::mutex> lock(m_SourcesMutex);
    AudioSource* source = GetSource(sourceUUID);
    if (source) {
        source->position = position;
        if (source->maSound && source->is3D) {
            ma_sound_set_position(source->maSound, position.x, position.y, position.z);
        }
    }
}

void AudioManager::SetSourceBus(const core::UUID& sourceUUID, const std::string& busName) {
    std::lock_guard<std::recursive_mutex> busLock(m_BusMutex);
    std::lock_guard<std::mutex> lock(m_SourcesMutex);
    AudioSource* source = GetSource(sourceUUID);
    if (source && HasBus(busName)) {
        source->busName = busName;
    }
}

void AudioManager::CreateBus(const std::string& name, const std::string& parentBus) {
    std::lock_guard<std::recursive_mutex> busLock(m_BusMutex);
    if (HasBus(name)) {

        return;
    }

    AudioBus bus;
    bus.name = name;
    bus.parentBus = parentBus;
    bus.volume = 1.0f;
    bus.muted = false;
    bus.solo = false;

    m_Buses[name] = bus;

    CreateSoundGroup(m_Buses[name]);

}

void AudioManager::DestroyBus(const std::string& name) {
    if (name == "Master") {

        return;
    }

    std::lock_guard<std::recursive_mutex> busLock(m_BusMutex);

    std::unordered_map<std::string, AudioBus>::iterator it = m_Buses.find(name);
    if (it != m_Buses.end()) {

        {
            std::lock_guard<std::mutex> lock(m_SourcesMutex);
            for (auto& sourcePair : m_Sources) {
                if (sourcePair.second.busName == name) {
                    sourcePair.second.busName = "Master";
                }
            }
        }

        DestroyBusEffectNode(name);
        DestroySoundGroup(it->second);

        m_Buses.erase(it);

    }
}

bool AudioManager::HasBus(const std::string& name) const {
    std::lock_guard<std::recursive_mutex> busLock(m_BusMutex);
    return m_Buses.find(name) != m_Buses.end();
}

std::unordered_map<std::string, AudioBus> AudioManager::GetBuses() const {
    std::lock_guard<std::recursive_mutex> busLock(m_BusMutex);
    return m_Buses;
}

AudioBus* AudioManager::GetBus(const std::string& name) {
    std::lock_guard<std::recursive_mutex> busLock(m_BusMutex);
    std::unordered_map<std::string, AudioBus>::iterator it = m_Buses.find(name);
    if (it != m_Buses.end()) {
        return &it->second;
    }
    return nullptr;
}

const AudioBus* AudioManager::GetBus(const std::string& name) const {
    std::lock_guard<std::recursive_mutex> busLock(m_BusMutex);
    std::unordered_map<std::string, AudioBus>::const_iterator it = m_Buses.find(name);
    if (it != m_Buses.end()) {
        return &it->second;
    }
    return nullptr;
}

void AudioManager::SetBusVolume(const std::string& name, float volume) {
    std::lock_guard<std::recursive_mutex> busLock(m_BusMutex);
    AudioBus* bus = GetBus(name);
    if (bus) {
        bus->volume = math::Clamp(volume, 0.0f, 1.0f);
        if (bus->maSoundGroup) {
            ma_sound_group_set_volume(bus->maSoundGroup, bus->volume);
        }
    }
}

void AudioManager::SetBusMuted(const std::string& name, bool muted) {
    std::lock_guard<std::recursive_mutex> busLock(m_BusMutex);
    AudioBus* bus = GetBus(name);
    if (bus) {
        bus->muted = muted;

        if (bus->maSoundGroup) {
            ma_sound_group_set_volume(bus->maSoundGroup, muted ? 0.0f : bus->volume);
        }
    }
}

void AudioManager::SetBusSolo(const std::string& name, bool solo) {
    std::lock_guard<std::recursive_mutex> busLock(m_BusMutex);
    AudioBus* bus = GetBus(name);
    if (bus) {
        bus->solo = solo;

        if (solo) {
            for (auto& pair : m_Buses) {
                if (pair.first != name && pair.second.maSoundGroup) {
                    ma_sound_group_set_volume(pair.second.maSoundGroup, 0.0f);
                }
            }
        } else {

            for (auto& pair : m_Buses) {
                if (pair.second.maSoundGroup) {
                    ma_sound_group_set_volume(pair.second.maSoundGroup,
                                             pair.second.muted ? 0.0f : pair.second.volume);
                }
            }
        }
    }
}

float AudioManager::GetBusVolume(const std::string& name) const {
    std::lock_guard<std::recursive_mutex> busLock(m_BusMutex);
    const AudioBus* bus = GetBus(name);
    return bus ? bus->volume : 0.0f;
}

bool AudioManager::IsBusMuted(const std::string& name) const {
    std::lock_guard<std::recursive_mutex> busLock(m_BusMutex);
    const AudioBus* bus = GetBus(name);
    return bus ? bus->muted : false;
}

bool AudioManager::IsBusSolo(const std::string& name) const {
    std::lock_guard<std::recursive_mutex> busLock(m_BusMutex);
    const AudioBus* bus = GetBus(name);
    return bus ? bus->solo : false;
}

void AudioManager::SetListenerPosition(const math::Vec3& position) {
    m_Listener.position = position;
    if (m_Engine) {
        ma_engine_listener_set_position(m_Engine, 0, position.x, position.y, position.z);
    }
}

void AudioManager::SetListenerOrientation(const math::Vec3& forward, const math::Vec3& up) {
    m_Listener.forward = forward;
    m_Listener.up = up;
    if (m_Engine) {
        ma_engine_listener_set_direction(m_Engine, 0, forward.x, forward.y, forward.z);
        ma_engine_listener_set_world_up(m_Engine, 0, up.x, up.y, up.z);
    }
}

void AudioManager::SetListenerVelocity(const math::Vec3& velocity) {
    m_Listener.velocity = velocity;
    if (m_Engine) {
        ma_engine_listener_set_velocity(m_Engine, 0, velocity.x, velocity.y, velocity.z);
    }
}

void AudioManager::SetMasterVolume(float volume) {
    m_MasterVolume = math::Clamp(volume, 0.0f, 1.0f);
    if (m_Engine) {
        ma_engine_set_volume(m_Engine, m_MasterVolume);
    }
}

void AudioManager::SetMasterMuted(bool muted) {
    m_MasterMuted = muted;
    if (m_Engine) {
        ma_engine_set_volume(m_Engine, muted ? 0.0f : m_MasterVolume);
    }
}

void AudioManager::Update(float deltaTime) {
    if (!m_Initialized || !m_Engine) {
        return;
    }

    m_CurrentTime += deltaTime;

    StartScheduledSources();
    RemoveFinishedSources();
}

void AudioManager::StartScheduledSources() {
    std::lock_guard<std::mutex> lock(m_SourcesMutex);
    for (auto& [uuid, source] : m_Sources) {
        if (source.pendingScheduledStart && m_CurrentTime >= source.scheduledTime) {
            if (source.maSound) {
                ma_sound_start(source.maSound);
                source.state = PlaybackState::Playing;
            }
            source.pendingScheduledStart = false;
        }
    }
}

void AudioManager::RemoveFinishedSources() {
    // Detect completed one-shots and erase them under the lock, then fire the
    // finished callback after releasing it (so callbacks may re-enter the manager
    // without deadlocking).
    std::vector<core::UUID> finishedUUIDs;
    {
        std::lock_guard<std::mutex> lock(m_SourcesMutex);
        auto it = m_Sources.begin();
        while (it != m_Sources.end()) {
            AudioSource& source = it->second;

            // Only an actively-playing one-shot can finish. Gating on Playing
            // leaves paused sources (also reported as "not playing" by miniaudio)
            // and not-yet-started scheduled sources untouched.
            if (source.maSound && source.state == PlaybackState::Playing &&
                !source.pendingScheduledStart && source.mode == PlaybackMode::OneShot) {
                if (!ma_sound_is_playing(source.maSound)) {
                    ma_sound_uninit(source.maSound);
                    delete source.maSound;
                    source.maSound = nullptr;

                    if (source.maAudioBuffer) {
                        ma_audio_buffer* buffer = static_cast<ma_audio_buffer*>(source.maAudioBuffer);
                        ma_audio_buffer_uninit(buffer);
                        delete buffer;
                        source.maAudioBuffer = nullptr;
                    }

                    source.state = PlaybackState::Stopped;
                    source.finished = true;
                }
            }

            if (source.finished && source.mode == PlaybackMode::OneShot) {
                finishedUUIDs.push_back(source.uuid);
                it = m_Sources.erase(it);
            } else {
                ++it;
            }
        }
    }

    if (m_SourceFinishedCallback) {
        for (const core::UUID& uuid : finishedUUIDs) {
            m_SourceFinishedCallback(uuid);
        }
    }
}

bool AudioManager::IsSourcePlaying(const core::UUID& sourceUUID) const {
    std::lock_guard<std::mutex> lock(m_SourcesMutex);
    auto it = m_Sources.find(sourceUUID);
    return it != m_Sources.end() && it->second.state == PlaybackState::Playing;
}

bool AudioManager::IsSourceFinished(const core::UUID& sourceUUID) const {
    std::lock_guard<std::mutex> lock(m_SourcesMutex);
    auto it = m_Sources.find(sourceUUID);
    return it != m_Sources.end() && it->second.finished;
}

void AudioManager::SetSourceFinishedCallback(SourceFinishedCallback callback) {
    m_SourceFinishedCallback = std::move(callback);
}

float AudioManager::CalculateSourceVolume(const AudioSource& source) const {
    float volume = source.volume;

    std::string currentBusName = source.busName;
    while (!currentBusName.empty()) {
        const AudioBus* bus = GetBus(currentBusName);
        if (!bus) break;

        if (bus->muted) {
            return 0.0f;
        }

        volume *= bus->volume;
        currentBusName = bus->parentBus;
    }

    if (m_MasterMuted) {
        return 0.0f;
    }
    volume *= m_MasterVolume;

    if (source.is3D) {
        volume *= Calculate3DAttenuation(source);
    }

    return math::Clamp(volume, 0.0f, 1.0f);
}

float AudioManager::Calculate3DAttenuation(const AudioSource& source) const {

    math::Vec3 toSource = source.position - m_Listener.position;
    float distance = math::Length(toSource);

    if (distance <= source.minDistance) {
        return 1.0f;
    }

    if (distance >= source.maxDistance) {
        return 0.0f;
    }

    float attenuation = 1.0f - ((distance - source.minDistance) /
                                (source.maxDistance - source.minDistance));
    attenuation = math::Pow(attenuation, source.rolloffFactor);

    return math::Clamp(attenuation, 0.0f, 1.0f);
}

void AudioManager::ApplyPanning(const AudioSource& source, float& leftGain, float& rightGain) const {
    if (source.is3D) {

        math::Vec3 toSource = source.position - m_Listener.position;
        toSource = math::Normalize(toSource);

        math::Vec3 right = math::Cross(m_Listener.forward, m_Listener.up);
        right = math::Normalize(right);

        float pan = math::Dot(toSource, right);

        leftGain = math::Clamp(1.0f - pan, 0.0f, 1.0f);
        rightGain = math::Clamp(1.0f + pan, 0.0f, 1.0f);
    } else {

        float pan = source.pan;
        leftGain = math::Clamp(1.0f - pan, 0.0f, 1.0f);
        rightGain = math::Clamp(1.0f + pan, 0.0f, 1.0f);
    }
}

void AudioManager::UpdateSourceBackend(AudioSource& source) {
    if (!source.maSound) return;

    ma_sound_set_volume(source.maSound, source.volume);

    if (!source.is3D) {
        ma_sound_set_pitch(source.maSound, source.pitch);
        ma_sound_set_pan(source.maSound, source.pan);
    } else {
        ma_sound_set_position(source.maSound, source.position.x, source.position.y, source.position.z);
        ma_sound_set_min_distance(source.maSound, source.minDistance);
        ma_sound_set_max_distance(source.maSound, source.maxDistance);
        ma_sound_set_rolloff(source.maSound, source.rolloffFactor);
    }
}

void AudioManager::CreateSoundGroup(AudioBus& bus) {
    std::lock_guard<std::recursive_mutex> busLock(m_BusMutex);
    if (!m_Engine) return;

    bus.maSoundGroup = new ma_sound();

    ma_sound* parentGroup = nullptr;
    if (!bus.parentBus.empty()) {
        AudioBus* parent = GetBus(bus.parentBus);
        if (parent) {
            parentGroup = parent->maSoundGroup;
        }
    }

    ma_result result = ma_sound_group_init(m_Engine, 0, parentGroup, bus.maSoundGroup);
    if (result != MA_SUCCESS) {

        delete bus.maSoundGroup;
        bus.maSoundGroup = nullptr;
        return;
    }

    ma_sound_group_set_volume(bus.maSoundGroup, bus.volume);

    CreateBusEffectNode(bus);
}

void AudioManager::DestroySoundGroup(AudioBus& bus) {
    std::lock_guard<std::recursive_mutex> busLock(m_BusMutex);
    if (bus.maSoundGroup) {
        ma_sound_group_uninit(bus.maSoundGroup);
        delete bus.maSoundGroup;
        bus.maSoundGroup = nullptr;
    }
}

void AudioManager::RegisterProperties() {

    RegisterProperty<float>("masterVolume", core::PropertyType::Float,
        [this]() { return m_MasterVolume; },
        [this](const float& value) { SetMasterVolume(value); });

    RegisterProperty<bool>("masterMuted", core::PropertyType::Bool,
        [this]() { return m_MasterMuted; },
        [this](const bool& value) { SetMasterMuted(value); });
}

nlohmann::json AudioManager::Serialize() const {
    nlohmann::json json = core::ISerializable::Serialize();

    std::lock_guard<std::recursive_mutex> busLock(m_BusMutex);

    nlohmann::json busesJson = nlohmann::json::array();
    for (const auto& pair : m_Buses) {
        const AudioBus& bus = pair.second;
        nlohmann::json busJson;
        busJson["name"] = bus.name;
        busJson["volume"] = bus.volume;
        busJson["muted"] = bus.muted;
        busJson["solo"] = bus.solo;
        busJson["parentBus"] = bus.parentBus;
        nlohmann::json effectsJson = nlohmann::json::array();
        for (const AudioEffectDesc& effect : bus.effects) {
            effectsJson.push_back(effect.Serialize());
        }
        busJson["effects"] = effectsJson;
        busesJson.push_back(busJson);
    }
    json["buses"] = busesJson;

    return json;
}

void AudioManager::Deserialize(const nlohmann::json& json) {
    RegisterProperties();
    core::ISerializable::Deserialize(json);

    if (json.contains("buses") && json["buses"].is_array()) {

        std::lock_guard<std::recursive_mutex> busLock(m_BusMutex);

        std::unordered_map<std::string, AudioBus>::iterator it = m_Buses.begin();
        while (it != m_Buses.end()) {
            if (it->first == "Master") {
                ++it;
            } else {
                DestroyBusEffectNode(it->first);
                DestroySoundGroup(it->second);
                it = m_Buses.erase(it);
            }
        }

        for (const auto& busJson : json["buses"]) {
            if (!busJson.contains("name")) continue;

            std::string name = busJson["name"].get<std::string>();

            if (name == "Master") {

                AudioBus* masterBus = GetBus("Master");
                if (masterBus && busJson.contains("volume")) {
                    masterBus->volume = busJson["volume"].get<float>();
                    if (masterBus->maSoundGroup) {
                        ma_sound_group_set_volume(masterBus->maSoundGroup, masterBus->volume);
                    }
                }
                if (masterBus && busJson.contains("muted")) {
                    masterBus->muted = busJson["muted"].get<bool>();
                }
                if (masterBus && busJson.contains("effects") && busJson["effects"].is_array()) {
                    masterBus->effects.clear();
                    for (const auto& effectJson : busJson["effects"]) {
                        masterBus->effects.push_back(AudioEffectDesc::Deserialize(effectJson));
                    }
                    RebuildBusChain("Master");
                }
                continue;
            }

            AudioBus bus;
            bus.name = name;
            bus.volume = busJson.value("volume", 1.0f);
            bus.muted = busJson.value("muted", false);
            bus.solo = busJson.value("solo", false);
            bus.parentBus = busJson.value("parentBus", "");
            if (busJson.contains("effects") && busJson["effects"].is_array()) {
                for (const auto& effectJson : busJson["effects"]) {
                    bus.effects.push_back(AudioEffectDesc::Deserialize(effectJson));
                }
            }

            m_Buses[name] = bus;
            CreateSoundGroup(m_Buses[name]);
        }
    }
}

// ============================================================================
// Bus DSP effect helpers + API
// ============================================================================

void AudioManager::CreateBusEffectNode(AudioBus& bus) {
    std::lock_guard<std::recursive_mutex> busLock(m_BusMutex);
    if (!m_Engine || !bus.maSoundGroup) {
        return;
    }
    if (m_BusEffectNodes.find(bus.name) != m_BusEffectNodes.end()) {
        return;
    }

    std::unique_ptr<BusEffectNode> node = std::make_unique<BusEffectNode>();
    node->channels = ma_engine_get_channels(m_Engine);

    ma_node_config cfg = ma_node_config_init();
    cfg.vtable = &g_busEffectVtable;
    cfg.pInputChannels = &node->channels;
    cfg.pOutputChannels = &node->channels;

    ma_node_graph* graph = ma_engine_get_node_graph(m_Engine);
    if (ma_node_init(graph, &cfg, nullptr, &node->base) != MA_SUCCESS) {
        return;
    }

    // Re-route this bus through the effect node: group -> effect -> parent.
    ma_node* parentNode = nullptr;
    if (!bus.parentBus.empty()) {
        AudioBus* parent = GetBus(bus.parentBus);
        if (parent && parent->maSoundGroup) {
            parentNode = parent->maSoundGroup;
        }
    }
    if (!parentNode) {
        parentNode = ma_node_graph_get_endpoint(graph);
    }
    ma_node_attach_output_bus(bus.maSoundGroup, 0, &node->base, 0);
    ma_node_attach_output_bus(&node->base, 0, parentNode, 0);

    node->chain.Rebuild(bus.effects, node->channels, ma_engine_get_sample_rate(m_Engine));
    m_BusEffectNodes[bus.name] = std::move(node);
}

void AudioManager::DestroyBusEffectNode(const std::string& busName) {
    std::lock_guard<std::recursive_mutex> busLock(m_BusMutex);
    std::unordered_map<std::string, std::unique_ptr<BusEffectNode>>::iterator it =
        m_BusEffectNodes.find(busName);
    if (it == m_BusEffectNodes.end()) {
        return;
    }
    // ma_node_uninit detaches the node from the graph and fences against an
    // in-flight BusEffectNode_Process, so the node is unreachable from the audio
    // thread by the time the map entry (and the node itself) is destroyed.
    if (it->second) {
        ma_node_uninit(&it->second->base, nullptr);
    }
    m_BusEffectNodes.erase(it);
}

void AudioManager::RebuildBusChain(const std::string& busName) {
    std::lock_guard<std::recursive_mutex> busLock(m_BusMutex);
    std::unordered_map<std::string, AudioBus>::iterator busIt = m_Buses.find(busName);
    if (busIt == m_Buses.end()) {
        return;
    }
    std::unordered_map<std::string, std::unique_ptr<BusEffectNode>>::iterator nodeIt =
        m_BusEffectNodes.find(busName);
    if (nodeIt == m_BusEffectNodes.end() || !nodeIt->second) {
        return;
    }
    nodeIt->second->chain.Rebuild(busIt->second.effects, nodeIt->second->channels,
                                  m_Engine ? ma_engine_get_sample_rate(m_Engine) : 48000);
}

int AudioManager::AddBusEffect(const std::string& busName, AudioEffectType type) {
    std::lock_guard<std::recursive_mutex> busLock(m_BusMutex);
    std::unordered_map<std::string, AudioBus>::iterator it = m_Buses.find(busName);
    if (it == m_Buses.end()) {
        return -1;
    }
    it->second.effects.emplace_back(type);
    RebuildBusChain(busName);
    return static_cast<int>(it->second.effects.size()) - 1;
}

void AudioManager::RemoveBusEffect(const std::string& busName, int index) {
    std::lock_guard<std::recursive_mutex> busLock(m_BusMutex);
    std::unordered_map<std::string, AudioBus>::iterator it = m_Buses.find(busName);
    if (it == m_Buses.end()) {
        return;
    }
    std::vector<AudioEffectDesc>& effects = it->second.effects;
    if (index < 0 || index >= static_cast<int>(effects.size())) {
        return;
    }
    effects.erase(effects.begin() + index);
    RebuildBusChain(busName);
}

void AudioManager::MoveBusEffect(const std::string& busName, int fromIndex, int toIndex) {
    std::lock_guard<std::recursive_mutex> busLock(m_BusMutex);
    std::unordered_map<std::string, AudioBus>::iterator it = m_Buses.find(busName);
    if (it == m_Buses.end()) {
        return;
    }
    std::vector<AudioEffectDesc>& effects = it->second.effects;
    int count = static_cast<int>(effects.size());
    if (fromIndex < 0 || fromIndex >= count || toIndex < 0 || toIndex >= count || fromIndex == toIndex) {
        return;
    }
    AudioEffectDesc desc = effects[fromIndex];
    effects.erase(effects.begin() + fromIndex);
    effects.insert(effects.begin() + toIndex, desc);
    RebuildBusChain(busName);
}

void AudioManager::ClearBusEffects(const std::string& busName) {
    std::lock_guard<std::recursive_mutex> busLock(m_BusMutex);
    std::unordered_map<std::string, AudioBus>::iterator it = m_Buses.find(busName);
    if (it == m_Buses.end()) {
        return;
    }
    it->second.effects.clear();
    RebuildBusChain(busName);
}

void AudioManager::SetBusEffectEnabled(const std::string& busName, int index, bool enabled) {
    std::lock_guard<std::recursive_mutex> busLock(m_BusMutex);
    std::unordered_map<std::string, AudioBus>::iterator it = m_Buses.find(busName);
    if (it == m_Buses.end()) {
        return;
    }
    std::vector<AudioEffectDesc>& effects = it->second.effects;
    if (index < 0 || index >= static_cast<int>(effects.size())) {
        return;
    }
    effects[index].enabled = enabled;
    std::unordered_map<std::string, std::unique_ptr<BusEffectNode>>::iterator nodeIt =
        m_BusEffectNodes.find(busName);
    if (nodeIt != m_BusEffectNodes.end() && nodeIt->second) {
        nodeIt->second->chain.SetEnabled(static_cast<size_t>(index), enabled);
    }
}

void AudioManager::SetBusEffectParameter(const std::string& busName, int index,
                                         const std::string& parameter, float value) {
    std::lock_guard<std::recursive_mutex> busLock(m_BusMutex);
    std::unordered_map<std::string, AudioBus>::iterator it = m_Buses.find(busName);
    if (it == m_Buses.end()) {
        return;
    }
    std::vector<AudioEffectDesc>& effects = it->second.effects;
    if (index < 0 || index >= static_cast<int>(effects.size())) {
        return;
    }
    effects[index].SetParameter(parameter, value);
    std::unordered_map<std::string, std::unique_ptr<BusEffectNode>>::iterator nodeIt =
        m_BusEffectNodes.find(busName);
    if (nodeIt != m_BusEffectNodes.end() && nodeIt->second) {
        nodeIt->second->chain.UpdateParameter(static_cast<size_t>(index), parameter, value);
    }
}

int AudioManager::GetBusEffectCount(const std::string& busName) const {
    std::lock_guard<std::recursive_mutex> busLock(m_BusMutex);
    std::unordered_map<std::string, AudioBus>::const_iterator it = m_Buses.find(busName);
    return (it != m_Buses.end()) ? static_cast<int>(it->second.effects.size()) : 0;
}

bool AudioManager::GetBusEffectType(const std::string& busName, int index, AudioEffectType& outType) const {
    std::lock_guard<std::recursive_mutex> busLock(m_BusMutex);
    std::unordered_map<std::string, AudioBus>::const_iterator it = m_Buses.find(busName);
    if (it == m_Buses.end() || index < 0 || index >= static_cast<int>(it->second.effects.size())) {
        return false;
    }
    outType = it->second.effects[index].type;
    return true;
}

bool AudioManager::IsBusEffectEnabled(const std::string& busName, int index) const {
    std::lock_guard<std::recursive_mutex> busLock(m_BusMutex);
    std::unordered_map<std::string, AudioBus>::const_iterator it = m_Buses.find(busName);
    if (it == m_Buses.end() || index < 0 || index >= static_cast<int>(it->second.effects.size())) {
        return false;
    }
    return it->second.effects[index].enabled;
}

float AudioManager::GetBusEffectParameter(const std::string& busName, int index,
                                          const std::string& parameter, float fallback) const {
    std::lock_guard<std::recursive_mutex> busLock(m_BusMutex);
    std::unordered_map<std::string, AudioBus>::const_iterator it = m_Buses.find(busName);
    if (it == m_Buses.end() || index < 0 || index >= static_cast<int>(it->second.effects.size())) {
        return fallback;
    }
    return it->second.effects[index].GetParameter(parameter, fallback);
}

std::vector<AudioEffectDesc> AudioManager::GetBusEffects(const std::string& busName) const {
    std::lock_guard<std::recursive_mutex> busLock(m_BusMutex);
    std::unordered_map<std::string, AudioBus>::const_iterator it = m_Buses.find(busName);
    return (it != m_Buses.end()) ? it->second.effects : std::vector<AudioEffectDesc>{};
}

void AudioManager::GetBusLevels(const std::string& busName, float& outLeft, float& outRight) const {
    outLeft = 0.0f;
    outRight = 0.0f;
    std::lock_guard<std::recursive_mutex> busLock(m_BusMutex);
    std::unordered_map<std::string, std::unique_ptr<BusEffectNode>>::const_iterator it =
        m_BusEffectNodes.find(busName);
    if (it == m_BusEffectNodes.end() || !it->second) {
        return;
    }
    const BusEffectNode& node = *it->second;
    outLeft = node.levels[0].load(std::memory_order_relaxed);
    ma_uint32 rightChannel = (node.channels > 1) ? 1 : 0;
    outRight = node.levels[rightChannel].load(std::memory_order_relaxed);
}

float AudioManager::GetBusPeak(const std::string& busName) const {
    float l = 0.0f;
    float r = 0.0f;
    GetBusLevels(busName, l, r);
    return (l > r) ? l : r;
}

}
}
