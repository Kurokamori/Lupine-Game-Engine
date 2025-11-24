#pragma once

#include "lupine/asset/AudioAsset.hpp"
#include "lupine/core/UUID.hpp"
#include "lupine/core/Serialization.hpp"
#include "lupine/math/Vec3.hpp"
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>
#include <atomic>
#include <mutex>

// Forward declare miniaudio types
// Note: These are forward declarations only; full definitions come from miniaudio.h
struct ma_engine;
struct ma_sound;

namespace lupine {
namespace audio {

/**
 * Audio playback mode
 */
enum class PlaybackMode {
    OneShot,    // Play once and stop
    Loop,       // Loop continuously
    Scheduled   // Play at a scheduled time
};

/**
 * Audio playback state
 */
enum class PlaybackState {
    Stopped,
    Playing,
    Paused
};

/**
 * Audio source instance
 * Represents a single playing audio source
 */
struct AudioSource {
    core::UUID uuid;                        // Unique ID for this source
    core::UUID audioAssetUUID;              // Reference to the audio asset
    asset::AssetRef<asset::AudioAsset> asset; // The audio asset being played

    PlaybackState state{PlaybackState::Stopped};
    PlaybackMode mode{PlaybackMode::OneShot};

    float volume{1.0f};                     // Source volume (0.0 - 1.0)
    float pitch{1.0f};                      // Pitch multiplier (0.5 - 2.0)
    float pan{0.0f};                        // Stereo pan (-1.0 = left, 0.0 = center, 1.0 = right)

    // 3D spatial parameters
    bool is3D{false};                       // Is this a 3D positioned sound?
    math::Vec3 position{0.0f, 0.0f, 0.0f}; // 3D position
    float minDistance{1.0f};                // Distance at which sound starts to attenuate
    float maxDistance{100.0f};              // Distance at which sound is inaudible
    float rolloffFactor{1.0f};              // How quickly sound attenuates with distance

    // Playback tracking
    size_t currentSample{0};                // Current playback position in samples
    double scheduledTime{0.0};              // Scheduled playback time (for scheduled mode)

    // Bus assignment
    std::string busName{"Master"};          // Which bus this source is assigned to

    // miniaudio backend
    ma_sound* maSound{nullptr};             // Pointer to miniaudio sound object
    void* maAudioBuffer{nullptr};            // Pointer to audio buffer (for preloaded data) - opaque pointer to avoid forward declaration issues
};

/**
 * Audio bus
 * Groups audio sources and applies collective processing
 */
struct AudioBus {
    std::string name;
    float volume{1.0f};                     // Bus volume (0.0 - 1.0)
    bool muted{false};                      // Is this bus muted?
    bool solo{false};                       // Is this bus soloed?

    // Parent bus for hierarchical mixing
    std::string parentBus;

    // miniaudio backend
    // In miniaudio, ma_sound_group is typedef'd to ma_sound
    ma_sound* maSoundGroup{nullptr};  // Pointer to miniaudio sound group

    // Get effective volume (considering parent buses)
    float GetEffectiveVolume() const { return muted ? 0.0f : volume; }
};

/**
 * Audio listener
 * Represents the "ears" in the 3D audio scene
 */
struct AudioListener {
    math::Vec3 position{0.0f, 0.0f, 0.0f};
    math::Vec3 forward{0.0f, 0.0f, -1.0f};
    math::Vec3 up{0.0f, 1.0f, 0.0f};
    math::Vec3 velocity{0.0f, 0.0f, 0.0f};  // For doppler effect
};

/**
 * Audio Manager
 * Backend-agnostic audio management system
 * Handles audio asset playback, mixing, buses, and 3D spatial audio
 */
class AudioManager : public core::ISerializable {
public:
    AudioManager();
    virtual ~AudioManager();
    
    // Singleton access
    static AudioManager& GetInstance();
    
    // Initialization
    void Initialize();
    void Shutdown();
    bool IsInitialized() const { return m_Initialized; }
    
    // ========================================
    // Playback Control
    // ========================================
    
    // Play an audio asset
    core::UUID Play(const asset::AssetRef<asset::AudioAsset>& audioAsset, 
                    const std::string& busName = "Master",
                    PlaybackMode mode = PlaybackMode::OneShot,
                    float volume = 1.0f);
    
    // Play an audio asset at a 3D position
    core::UUID Play3D(const asset::AssetRef<asset::AudioAsset>& audioAsset,
                      const math::Vec3& position,
                      const std::string& busName = "Master",
                      PlaybackMode mode = PlaybackMode::OneShot,
                      float volume = 1.0f);
    
    // Playback control by source UUID
    void Stop(const core::UUID& sourceUUID);
    void Pause(const core::UUID& sourceUUID);
    void Resume(const core::UUID& sourceUUID);
    
    // Stop all sources
    void StopAll();
    void PauseAll();
    void ResumeAll();
    
    // ========================================
    // Source Control
    // ========================================
    
    // Get a source by UUID
    AudioSource* GetSource(const core::UUID& sourceUUID);
    const AudioSource* GetSource(const core::UUID& sourceUUID) const;
    
    // Set source properties
    void SetSourceVolume(const core::UUID& sourceUUID, float volume);
    void SetSourcePitch(const core::UUID& sourceUUID, float pitch);
    void SetSourcePan(const core::UUID& sourceUUID, float pan);
    void SetSourcePosition(const core::UUID& sourceUUID, const math::Vec3& position);
    void SetSourceBus(const core::UUID& sourceUUID, const std::string& busName);

    // ========================================
    // Bus Management
    // ========================================

    // Create/destroy buses
    void CreateBus(const std::string& name, const std::string& parentBus = "");
    void DestroyBus(const std::string& name);
    bool HasBus(const std::string& name) const;

    // Get bus
    AudioBus* GetBus(const std::string& name);
    const AudioBus* GetBus(const std::string& name) const;

    // Bus control
    void SetBusVolume(const std::string& name, float volume);
    void SetBusMuted(const std::string& name, bool muted);
    void SetBusSolo(const std::string& name, bool solo);
    float GetBusVolume(const std::string& name) const;
    bool IsBusMuted(const std::string& name) const;
    bool IsBusSolo(const std::string& name) const;

    // Get all buses
    const std::unordered_map<std::string, AudioBus>& GetBuses() const { return m_Buses; }

    // ========================================
    // 3D Audio / Listener
    // ========================================

    // Set listener properties
    void SetListenerPosition(const math::Vec3& position);
    void SetListenerOrientation(const math::Vec3& forward, const math::Vec3& up);
    void SetListenerVelocity(const math::Vec3& velocity);

    const AudioListener& GetListener() const { return m_Listener; }

    // ========================================
    // Master Controls
    // ========================================

    void SetMasterVolume(float volume);
    float GetMasterVolume() const { return m_MasterVolume; }

    void SetMasterMuted(bool muted);
    bool IsMasterMuted() const { return m_MasterMuted; }

    // ========================================
    // Update
    // ========================================

    // Update audio system (call once per frame)
    // deltaTime in seconds
    void Update(float deltaTime);

    // ========================================
    // Serialization
    // ========================================

    std::string GetTypeName() const override { return "AudioManager"; }
    void RegisterProperties() override;
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& json) override;

private:
    bool m_Initialized{false};

    // Audio sources
    std::unordered_map<core::UUID, AudioSource> m_Sources;
    std::atomic<uint32_t> m_NextSourceId{0};
    std::mutex m_SourcesMutex;  // Thread-safe access to sources

    // Audio buses
    std::unordered_map<std::string, AudioBus> m_Buses;

    // 3D audio listener
    AudioListener m_Listener;

    // Master controls
    float m_MasterVolume{1.0f};
    bool m_MasterMuted{false};

    // Time tracking
    double m_CurrentTime{0.0};

    // miniaudio backend
    ma_engine* m_Engine{nullptr};

    // Helper methods
    void RemoveFinishedSources();
    float CalculateSourceVolume(const AudioSource& source) const;
    float Calculate3DAttenuation(const AudioSource& source) const;
    void ApplyPanning(const AudioSource& source, float& leftGain, float& rightGain) const;

    // miniaudio helpers
    void UpdateSourceBackend(AudioSource& source);
    void CreateSoundGroup(AudioBus& bus);
    void DestroySoundGroup(AudioBus& bus);

    // Singleton
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;
};

} // namespace audio
} // namespace lupine

