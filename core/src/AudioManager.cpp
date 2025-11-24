#include "lupine/audio/AudioManager.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/math/MathCommon.hpp"
#include <algorithm>

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

namespace lupine {
namespace audio {

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

    ma_engine_listener_set_position(m_Engine, 0, 0.0f, 0.0f, 0.0f);
    ma_engine_listener_set_direction(m_Engine, 0, 0.0f, 0.0f, -1.0f);
    ma_engine_listener_set_world_up(m_Engine, 0, 0.0f, 1.0f, 0.0f);

    m_Initialized = true;

}

void AudioManager::Shutdown() {
    if (!m_Initialized) {
        return;
    }

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
                               float volume) {
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

    AudioBus* bus = GetBus(source.busName);
    ma_sound* soundGroup = bus ? bus->maSoundGroup : nullptr;

    ma_result result;
    ma_uint32 flags = 0;

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

    result = ma_sound_start(source.maSound);
    if (result != MA_SUCCESS) {

        ma_sound_uninit(source.maSound);
        delete source.maSound;
        source.maSound = nullptr;
        return core::UUID(0);
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
                                 float volume) {
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

    AudioBus* bus = GetBus(source.busName);
    ma_sound* soundGroup = bus ? bus->maSoundGroup : nullptr;

    ma_result result;
    ma_uint32 flags = 0;

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

    result = ma_sound_start(source.maSound);
    if (result != MA_SUCCESS) {

        ma_sound_uninit(source.maSound);
        delete source.maSound;
        source.maSound = nullptr;
        return core::UUID(0);
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
    std::lock_guard<std::mutex> lock(m_SourcesMutex);
    AudioSource* source = GetSource(sourceUUID);
    if (source && HasBus(busName)) {
        source->busName = busName;

        if (source->maSound) {
            AudioBus* bus = GetBus(busName);
            if (bus && bus->maSoundGroup) {

            }
        }
    }
}

void AudioManager::CreateBus(const std::string& name, const std::string& parentBus) {
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

    auto it = m_Buses.find(name);
    if (it != m_Buses.end()) {

        {
            std::lock_guard<std::mutex> lock(m_SourcesMutex);
            for (auto& sourcePair : m_Sources) {
                if (sourcePair.second.busName == name) {
                    sourcePair.second.busName = "Master";
                }
            }
        }

        DestroySoundGroup(it->second);

        m_Buses.erase(it);

    }
}

bool AudioManager::HasBus(const std::string& name) const {
    return m_Buses.find(name) != m_Buses.end();
}

AudioBus* AudioManager::GetBus(const std::string& name) {
    auto it = m_Buses.find(name);
    if (it != m_Buses.end()) {
        return &it->second;
    }
    return nullptr;
}

const AudioBus* AudioManager::GetBus(const std::string& name) const {
    auto it = m_Buses.find(name);
    if (it != m_Buses.end()) {
        return &it->second;
    }
    return nullptr;
}

void AudioManager::SetBusVolume(const std::string& name, float volume) {
    AudioBus* bus = GetBus(name);
    if (bus) {
        bus->volume = math::Clamp(volume, 0.0f, 1.0f);
        if (bus->maSoundGroup) {
            ma_sound_group_set_volume(bus->maSoundGroup, bus->volume);
        }
    }
}

void AudioManager::SetBusMuted(const std::string& name, bool muted) {
    AudioBus* bus = GetBus(name);
    if (bus) {
        bus->muted = muted;

        if (bus->maSoundGroup) {
            ma_sound_group_set_volume(bus->maSoundGroup, muted ? 0.0f : bus->volume);
        }
    }
}

void AudioManager::SetBusSolo(const std::string& name, bool solo) {
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
    const AudioBus* bus = GetBus(name);
    return bus ? bus->volume : 0.0f;
}

bool AudioManager::IsBusMuted(const std::string& name) const {
    const AudioBus* bus = GetBus(name);
    return bus ? bus->muted : false;
}

bool AudioManager::IsBusSolo(const std::string& name) const {
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

    RemoveFinishedSources();
}

void AudioManager::RemoveFinishedSources() {
    std::lock_guard<std::mutex> lock(m_SourcesMutex);
    auto it = m_Sources.begin();
    while (it != m_Sources.end()) {
        AudioSource& source = it->second;

        if (source.maSound) {
            ma_bool32 isPlaying = ma_sound_is_playing(source.maSound);
            if (!isPlaying && source.mode == PlaybackMode::OneShot) {

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
            }
        }

        if (source.state == PlaybackState::Stopped && source.mode == PlaybackMode::OneShot) {
            it = m_Sources.erase(it);
        } else {
            ++it;
        }
    }
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

}

void AudioManager::DestroySoundGroup(AudioBus& bus) {
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

    nlohmann::json busesJson = nlohmann::json::array();
    for (const auto& pair : m_Buses) {
        const AudioBus& bus = pair.second;
        nlohmann::json busJson;
        busJson["name"] = bus.name;
        busJson["volume"] = bus.volume;
        busJson["muted"] = bus.muted;
        busJson["solo"] = bus.solo;
        busJson["parentBus"] = bus.parentBus;
        busesJson.push_back(busJson);
    }
    json["buses"] = busesJson;

    return json;
}

void AudioManager::Deserialize(const nlohmann::json& json) {
    RegisterProperties();
    core::ISerializable::Deserialize(json);

    if (json.contains("buses") && json["buses"].is_array()) {

        auto it = m_Buses.begin();
        while (it != m_Buses.end()) {
            if (it->first == "Master") {
                ++it;
            } else {
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
                continue;
            }

            AudioBus bus;
            bus.name = name;
            bus.volume = busJson.value("volume", 1.0f);
            bus.muted = busJson.value("muted", false);
            bus.solo = busJson.value("solo", false);
            bus.parentBus = busJson.value("parentBus", "");

            m_Buses[name] = bus;
            CreateSoundGroup(m_Buses[name]);
        }
    }
}

}
}
