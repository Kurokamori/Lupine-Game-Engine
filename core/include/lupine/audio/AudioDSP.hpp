#pragma once

#include <nlohmann/json.hpp>
#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace lupine {
namespace audio {

/**
 * Bus DSP effect types. Each maps to a concrete processor in AudioDSP.cpp and a
 * small set of named float parameters (see DefaultParameters()).
 */
enum class AudioEffectType {
    Gain,        // gain_db
    LowPass,     // cutoff_hz, resonance
    HighPass,    // cutoff_hz, resonance
    BandPass,    // cutoff_hz, resonance
    Peak,        // frequency_hz, resonance, gain_db
    LowShelf,    // frequency_hz, gain_db
    HighShelf,   // frequency_hz, gain_db
    Delay,       // time_ms, feedback, wet, dry
    Reverb,      // room_size, damping, wet, dry, width
    Compressor,  // threshold_db, ratio, attack_ms, release_ms, makeup_db
    Distortion,  // drive_db, wet, dry
    Chorus       // rate_hz, depth_ms, mix
};

/** Parse/format effect type <-> string (used by serialization and the API). */
std::string AudioEffectTypeToString(AudioEffectType type);
bool AudioEffectTypeFromString(const std::string& name, AudioEffectType& outType);

/** The default parameter set (name -> value) for a given effect type. */
std::map<std::string, float> DefaultEffectParameters(AudioEffectType type);

/**
 * Serializable description of one effect on a bus: its type, enabled flag and
 * named parameter values. This is the data model the editor/scripting/C-API and
 * scene files manipulate; the live DSP processors are rebuilt from it.
 */
struct AudioEffectDesc {
    AudioEffectType type{AudioEffectType::Gain};
    bool enabled{true};
    std::map<std::string, float> parameters;

    AudioEffectDesc() = default;
    explicit AudioEffectDesc(AudioEffectType t)
        : type(t), parameters(DefaultEffectParameters(t)) {}

    float GetParameter(const std::string& name, float fallback = 0.0f) const;
    void SetParameter(const std::string& name, float value);

    nlohmann::json Serialize() const;
    static AudioEffectDesc Deserialize(const nlohmann::json& json);
};

/**
 * Base class for a real-time DSP processor that operates on interleaved float
 * frames. Implementations are created from an AudioEffectDesc and live on the
 * audio thread.
 */
class AudioEffectProcessor {
public:
    virtual ~AudioEffectProcessor() = default;

    /** Allocate/clear state for the given stream format. */
    virtual void Prepare(uint32_t channels, uint32_t sampleRate) = 0;

    /** Process `frameCount` interleaved frames in place. */
    virtual void Process(float* frames, uint32_t frameCount, uint32_t channels) = 0;

    /** Update a single named parameter (recomputes coefficients as needed). */
    virtual void SetParameter(const std::string& name, float value) = 0;

    /** Construct the processor matching an effect type. */
    static std::unique_ptr<AudioEffectProcessor> Create(AudioEffectType type);
};

/**
 * An ordered chain of effect processors applied to one bus's audio. The chain is
 * built from a list of AudioEffectDesc and processed on the audio thread; it is
 * guarded by a mutex so the main thread can rebuild it (add/remove/reorder)
 * safely. Per-parameter tweaks are applied without rebuilding.
 */
class BusEffectChain {
public:
    /** Rebuild the live processors from descriptors (main thread). */
    void Rebuild(const std::vector<AudioEffectDesc>& effects,
                 uint32_t channels, uint32_t sampleRate);

    /** Push a single parameter change to a live processor (main thread). */
    void UpdateParameter(size_t index, const std::string& name, float value);

    /** Enable/disable a processor without rebuilding (main thread). */
    void SetEnabled(size_t index, bool enabled);

    /** Process interleaved frames in place (audio thread). */
    void Process(float* frames, uint32_t frameCount, uint32_t channels);

    bool IsEmpty() const;

private:
    struct Slot {
        std::unique_ptr<AudioEffectProcessor> processor;
        bool enabled{true};
    };

    mutable std::mutex m_Mutex;
    std::vector<Slot> m_Slots;
    // Lets the audio thread skip locking entirely when the bus has no effects.
    std::atomic<size_t> m_SlotCount{0};
    uint32_t m_Channels{2};
    uint32_t m_SampleRate{48000};
};

} // namespace audio
} // namespace lupine
