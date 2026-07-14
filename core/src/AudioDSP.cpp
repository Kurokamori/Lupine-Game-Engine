#include "lupine/audio/AudioDSP.hpp"

#include <algorithm>
#include <cmath>

namespace lupine {
namespace audio {

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kMinDb = -120.0f;

float DbToLinear(float db) {
    return std::pow(10.0f, db / 20.0f);
}

float ClampParam(float value, float lo, float hi) {
    return std::max(lo, std::min(hi, value));
}

// ============================================================================
// Biquad - second order IIR filter covering low/high/band-pass, peak and shelf.
// ============================================================================

enum class BiquadMode { LowPass, HighPass, BandPass, Peak, LowShelf, HighShelf };

class BiquadProcessor : public AudioEffectProcessor {
public:
    explicit BiquadProcessor(BiquadMode mode) : m_Mode(mode) {}

    void Prepare(uint32_t channels, uint32_t sampleRate) override {
        m_SampleRate = sampleRate;
        m_State.assign(channels, State{});
        Recompute();
    }

    void Process(float* frames, uint32_t frameCount, uint32_t channels) override {
        if (m_State.size() < channels) {
            m_State.resize(channels);
        }
        for (uint32_t i = 0; i < frameCount; ++i) {
            for (uint32_t c = 0; c < channels; ++c) {
                float x = frames[i * channels + c];
                State& s = m_State[c];
                float y = m_B0 * x + m_B1 * s.x1 + m_B2 * s.x2 - m_A1 * s.y1 - m_A2 * s.y2;
                s.x2 = s.x1;
                s.x1 = x;
                s.y2 = s.y1;
                s.y1 = y;
                frames[i * channels + c] = y;
            }
        }
    }

    void SetParameter(const std::string& name, float value) override {
        if (name == "cutoff_hz" || name == "frequency_hz") {
            m_Frequency = value;
        } else if (name == "resonance") {
            m_Q = value;
        } else if (name == "gain_db") {
            m_GainDb = value;
        }
        Recompute();
    }

private:
    struct State {
        float x1{0.0f}, x2{0.0f}, y1{0.0f}, y2{0.0f};
    };

    void Recompute() {
        float freq = ClampParam(m_Frequency, 10.0f, m_SampleRate * 0.45f);
        float q = std::max(0.0001f, m_Q);
        float omega = 2.0f * kPi * freq / static_cast<float>(m_SampleRate);
        float sinw = std::sin(omega);
        float cosw = std::cos(omega);
        float alpha = sinw / (2.0f * q);
        float A = std::pow(10.0f, m_GainDb / 40.0f);
        float sqrtA = std::sqrt(A);

        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a0 = 1.0f, a1 = 0.0f, a2 = 0.0f;
        switch (m_Mode) {
            case BiquadMode::LowPass:
                b0 = (1.0f - cosw) * 0.5f; b1 = 1.0f - cosw; b2 = (1.0f - cosw) * 0.5f;
                a0 = 1.0f + alpha; a1 = -2.0f * cosw; a2 = 1.0f - alpha;
                break;
            case BiquadMode::HighPass:
                b0 = (1.0f + cosw) * 0.5f; b1 = -(1.0f + cosw); b2 = (1.0f + cosw) * 0.5f;
                a0 = 1.0f + alpha; a1 = -2.0f * cosw; a2 = 1.0f - alpha;
                break;
            case BiquadMode::BandPass:
                b0 = alpha; b1 = 0.0f; b2 = -alpha;
                a0 = 1.0f + alpha; a1 = -2.0f * cosw; a2 = 1.0f - alpha;
                break;
            case BiquadMode::Peak:
                b0 = 1.0f + alpha * A; b1 = -2.0f * cosw; b2 = 1.0f - alpha * A;
                a0 = 1.0f + alpha / A; a1 = -2.0f * cosw; a2 = 1.0f - alpha / A;
                break;
            case BiquadMode::LowShelf:
                b0 = A * ((A + 1.0f) - (A - 1.0f) * cosw + 2.0f * sqrtA * alpha);
                b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosw);
                b2 = A * ((A + 1.0f) - (A - 1.0f) * cosw - 2.0f * sqrtA * alpha);
                a0 = (A + 1.0f) + (A - 1.0f) * cosw + 2.0f * sqrtA * alpha;
                a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosw);
                a2 = (A + 1.0f) + (A - 1.0f) * cosw - 2.0f * sqrtA * alpha;
                break;
            case BiquadMode::HighShelf:
                b0 = A * ((A + 1.0f) + (A - 1.0f) * cosw + 2.0f * sqrtA * alpha);
                b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosw);
                b2 = A * ((A + 1.0f) + (A - 1.0f) * cosw - 2.0f * sqrtA * alpha);
                a0 = (A + 1.0f) - (A - 1.0f) * cosw + 2.0f * sqrtA * alpha;
                a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosw);
                a2 = (A + 1.0f) - (A - 1.0f) * cosw - 2.0f * sqrtA * alpha;
                break;
        }

        float inv = (std::fabs(a0) > 1e-9f) ? (1.0f / a0) : 1.0f;
        m_B0 = b0 * inv; m_B1 = b1 * inv; m_B2 = b2 * inv;
        m_A1 = a1 * inv; m_A2 = a2 * inv;
    }

    BiquadMode m_Mode;
    uint32_t m_SampleRate{48000};
    float m_Frequency{1000.0f};
    float m_Q{0.707f};
    float m_GainDb{0.0f};
    float m_B0{1.0f}, m_B1{0.0f}, m_B2{0.0f}, m_A1{0.0f}, m_A2{0.0f};
    std::vector<State> m_State;
};

// ============================================================================
// Gain
// ============================================================================

class GainProcessor : public AudioEffectProcessor {
public:
    void Prepare(uint32_t, uint32_t) override {}
    void Process(float* frames, uint32_t frameCount, uint32_t channels) override {
        for (uint32_t i = 0; i < frameCount * channels; ++i) {
            frames[i] *= m_Gain;
        }
    }
    void SetParameter(const std::string& name, float value) override {
        if (name == "gain_db") {
            m_Gain = DbToLinear(value);
        }
    }
private:
    float m_Gain{1.0f};
};

// ============================================================================
// Delay (per-channel feedback delay line)
// ============================================================================

class DelayProcessor : public AudioEffectProcessor {
public:
    void Prepare(uint32_t channels, uint32_t sampleRate) override {
        m_SampleRate = sampleRate;
        m_Size = std::max<uint32_t>(1, sampleRate * 2); // up to 2 seconds
        m_Buffers.assign(channels, std::vector<float>(m_Size, 0.0f));
        m_WriteIndex.assign(channels, 0);
    }
    void Process(float* frames, uint32_t frameCount, uint32_t channels) override {
        if (m_Buffers.size() < channels) {
            Prepare(channels, m_SampleRate);
        }
        uint32_t delaySamples = std::min(m_Size - 1,
            static_cast<uint32_t>(m_TimeMs * 0.001f * static_cast<float>(m_SampleRate)));
        for (uint32_t i = 0; i < frameCount; ++i) {
            for (uint32_t c = 0; c < channels; ++c) {
                std::vector<float>& buf = m_Buffers[c];
                uint32_t& w = m_WriteIndex[c];
                uint32_t readIndex = (w + m_Size - delaySamples) % m_Size;
                float delayed = buf[readIndex];
                float in = frames[i * channels + c];
                buf[w] = in + delayed * m_Feedback;
                w = (w + 1) % m_Size;
                frames[i * channels + c] = in * m_Dry + delayed * m_Wet;
            }
        }
    }
    void SetParameter(const std::string& name, float value) override {
        if (name == "time_ms") m_TimeMs = std::max(0.0f, value);
        else if (name == "feedback") m_Feedback = ClampParam(value, 0.0f, 0.99f);
        else if (name == "wet") m_Wet = value;
        else if (name == "dry") m_Dry = value;
    }
private:
    uint32_t m_SampleRate{48000};
    uint32_t m_Size{1};
    float m_TimeMs{300.0f};
    float m_Feedback{0.4f};
    float m_Wet{0.5f};
    float m_Dry{1.0f};
    std::vector<std::vector<float>> m_Buffers;
    std::vector<uint32_t> m_WriteIndex;
};

// ============================================================================
// Reverb (Freeverb: 8 comb + 4 allpass per channel)
// ============================================================================

class ReverbProcessor : public AudioEffectProcessor {
public:
    void Prepare(uint32_t channels, uint32_t sampleRate) override {
        m_SampleRate = sampleRate;
        float scale = static_cast<float>(sampleRate) / 44100.0f;
        static const int kComb[kNumCombs] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
        static const int kAllpass[kNumAllpass] = {556, 441, 341, 225};
        const int stereoSpread = 23;

        m_Channels.assign(channels, ChannelState{});
        for (uint32_t c = 0; c < channels; ++c) {
            ChannelState& ch = m_Channels[c];
            for (int i = 0; i < kNumCombs; ++i) {
                int len = static_cast<int>((kComb[i] + (c % 2 == 1 ? stereoSpread : 0)) * scale);
                ch.combs[i].buffer.assign(std::max(1, len), 0.0f);
                ch.combs[i].index = 0;
                ch.combs[i].filterStore = 0.0f;
            }
            for (int i = 0; i < kNumAllpass; ++i) {
                int len = static_cast<int>((kAllpass[i] + (c % 2 == 1 ? stereoSpread : 0)) * scale);
                ch.allpass[i].buffer.assign(std::max(1, len), 0.0f);
                ch.allpass[i].index = 0;
            }
        }
        UpdateInternal();
    }

    void Process(float* frames, uint32_t frameCount, uint32_t channels) override {
        if (m_Channels.size() < channels) {
            Prepare(channels, m_SampleRate);
        }
        const float fixedGain = 0.015f;
        for (uint32_t i = 0; i < frameCount; ++i) {
            for (uint32_t c = 0; c < channels; ++c) {
                ChannelState& ch = m_Channels[c];
                float input = frames[i * channels + c];
                float in = input * fixedGain;
                float out = 0.0f;
                for (int k = 0; k < kNumCombs; ++k) {
                    out += ProcessComb(ch.combs[k], in);
                }
                for (int k = 0; k < kNumAllpass; ++k) {
                    out = ProcessAllpass(ch.allpass[k], out);
                }
                frames[i * channels + c] = input * m_Dry + out * m_Wet;
            }
        }
    }

    void SetParameter(const std::string& name, float value) override {
        if (name == "room_size") m_RoomSize = ClampParam(value, 0.0f, 1.0f);
        else if (name == "damping") m_Damping = ClampParam(value, 0.0f, 1.0f);
        else if (name == "wet") m_Wet = value;
        else if (name == "dry") m_Dry = value;
        else if (name == "width") m_Width = ClampParam(value, 0.0f, 1.0f);
        UpdateInternal();
    }

private:
    static constexpr int kNumCombs = 8;
    static constexpr int kNumAllpass = 4;

    struct Comb {
        std::vector<float> buffer;
        size_t index{0};
        float filterStore{0.0f};
    };
    struct Allpass {
        std::vector<float> buffer;
        size_t index{0};
    };
    struct ChannelState {
        Comb combs[kNumCombs];
        Allpass allpass[kNumAllpass];
    };

    void UpdateInternal() {
        m_Feedback = m_RoomSize * 0.28f + 0.7f;
        m_Damp = m_Damping * 0.4f;
    }

    float ProcessComb(Comb& comb, float input) {
        float output = comb.buffer[comb.index];
        comb.filterStore = output * (1.0f - m_Damp) + comb.filterStore * m_Damp;
        comb.buffer[comb.index] = input + comb.filterStore * m_Feedback;
        comb.index = (comb.index + 1) % comb.buffer.size();
        return output;
    }

    float ProcessAllpass(Allpass& ap, float input) {
        float bufout = ap.buffer[ap.index];
        float output = -input + bufout;
        ap.buffer[ap.index] = input + bufout * 0.5f;
        ap.index = (ap.index + 1) % ap.buffer.size();
        return output;
    }

    uint32_t m_SampleRate{48000};
    float m_RoomSize{0.5f};
    float m_Damping{0.5f};
    float m_Wet{0.33f};
    float m_Dry{1.0f};
    float m_Width{1.0f};
    float m_Feedback{0.84f};
    float m_Damp{0.2f};
    std::vector<ChannelState> m_Channels;
};

// ============================================================================
// Compressor (peak detector, shared gain across channels)
// ============================================================================

class CompressorProcessor : public AudioEffectProcessor {
public:
    void Prepare(uint32_t, uint32_t sampleRate) override {
        m_SampleRate = sampleRate;
        m_Envelope = 0.0f;
        UpdateCoefficients();
    }
    void Process(float* frames, uint32_t frameCount, uint32_t channels) override {
        float makeup = DbToLinear(m_MakeupDb);
        for (uint32_t i = 0; i < frameCount; ++i) {
            float peak = 0.0f;
            for (uint32_t c = 0; c < channels; ++c) {
                peak = std::max(peak, std::fabs(frames[i * channels + c]));
            }
            float coef = (peak > m_Envelope) ? m_AttackCoef : m_ReleaseCoef;
            m_Envelope = coef * (m_Envelope - peak) + peak;

            float envDb = (m_Envelope > 1e-6f) ? (20.0f * std::log10(m_Envelope)) : kMinDb;
            float gainDb = 0.0f;
            if (envDb > m_ThresholdDb && m_Ratio > 1.0f) {
                gainDb = (m_ThresholdDb - envDb) * (1.0f - 1.0f / m_Ratio);
            }
            float gain = DbToLinear(gainDb) * makeup;
            for (uint32_t c = 0; c < channels; ++c) {
                frames[i * channels + c] *= gain;
            }
        }
    }
    void SetParameter(const std::string& name, float value) override {
        if (name == "threshold_db") m_ThresholdDb = value;
        else if (name == "ratio") m_Ratio = std::max(1.0f, value);
        else if (name == "attack_ms") { m_AttackMs = std::max(0.01f, value); UpdateCoefficients(); }
        else if (name == "release_ms") { m_ReleaseMs = std::max(0.01f, value); UpdateCoefficients(); }
        else if (name == "makeup_db") m_MakeupDb = value;
    }
private:
    void UpdateCoefficients() {
        m_AttackCoef = std::exp(-1.0f / (m_AttackMs * 0.001f * static_cast<float>(m_SampleRate)));
        m_ReleaseCoef = std::exp(-1.0f / (m_ReleaseMs * 0.001f * static_cast<float>(m_SampleRate)));
    }
    uint32_t m_SampleRate{48000};
    float m_ThresholdDb{-12.0f};
    float m_Ratio{4.0f};
    float m_AttackMs{10.0f};
    float m_ReleaseMs{100.0f};
    float m_MakeupDb{0.0f};
    float m_AttackCoef{0.0f};
    float m_ReleaseCoef{0.0f};
    float m_Envelope{0.0f};
};

// ============================================================================
// Distortion (soft-clip)
// ============================================================================

class DistortionProcessor : public AudioEffectProcessor {
public:
    void Prepare(uint32_t, uint32_t) override {}
    void Process(float* frames, uint32_t frameCount, uint32_t channels) override {
        for (uint32_t i = 0; i < frameCount * channels; ++i) {
            float x = frames[i];
            float shaped = std::tanh(m_Drive * x);
            frames[i] = x * m_Dry + shaped * m_Wet;
        }
    }
    void SetParameter(const std::string& name, float value) override {
        if (name == "drive_db") m_Drive = DbToLinear(std::max(0.0f, value));
        else if (name == "wet") m_Wet = value;
        else if (name == "dry") m_Dry = value;
    }
private:
    float m_Drive{4.0f};
    float m_Wet{0.5f};
    float m_Dry{0.5f};
};

// ============================================================================
// Chorus (modulated delay line)
// ============================================================================

class ChorusProcessor : public AudioEffectProcessor {
public:
    void Prepare(uint32_t channels, uint32_t sampleRate) override {
        m_SampleRate = sampleRate;
        m_Size = std::max<uint32_t>(2, sampleRate / 10); // 100 ms
        m_Buffers.assign(channels, std::vector<float>(m_Size, 0.0f));
        m_WriteIndex.assign(channels, 0);
        m_Phase.assign(channels, 0.0f);
        for (uint32_t c = 0; c < channels; ++c) {
            m_Phase[c] = static_cast<float>(c) * 0.25f; // decorrelate channels
        }
    }
    void Process(float* frames, uint32_t frameCount, uint32_t channels) override {
        if (m_Buffers.size() < channels) {
            Prepare(channels, m_SampleRate);
        }
        float baseDelay = 0.015f * static_cast<float>(m_SampleRate);  // 15 ms
        float depth = m_DepthMs * 0.001f * static_cast<float>(m_SampleRate);
        float phaseInc = 2.0f * kPi * m_RateHz / static_cast<float>(m_SampleRate);
        for (uint32_t i = 0; i < frameCount; ++i) {
            for (uint32_t c = 0; c < channels; ++c) {
                std::vector<float>& buf = m_Buffers[c];
                uint32_t& w = m_WriteIndex[c];
                float in = frames[i * channels + c];
                buf[w] = in;

                float delaySamples = baseDelay + depth * (0.5f + 0.5f * std::sin(m_Phase[c]));
                delaySamples = std::min(delaySamples, static_cast<float>(m_Size - 2));
                float readPos = static_cast<float>(w) - delaySamples;
                while (readPos < 0.0f) readPos += static_cast<float>(m_Size);
                uint32_t i0 = static_cast<uint32_t>(readPos) % m_Size;
                uint32_t i1 = (i0 + 1) % m_Size;
                float frac = readPos - std::floor(readPos);
                float delayed = buf[i0] * (1.0f - frac) + buf[i1] * frac;

                frames[i * channels + c] = in * (1.0f - m_Mix) + delayed * m_Mix;
                w = (w + 1) % m_Size;
                m_Phase[c] += phaseInc;
                if (m_Phase[c] > 2.0f * kPi) m_Phase[c] -= 2.0f * kPi;
            }
        }
    }
    void SetParameter(const std::string& name, float value) override {
        if (name == "rate_hz") m_RateHz = std::max(0.0f, value);
        else if (name == "depth_ms") m_DepthMs = std::max(0.0f, value);
        else if (name == "mix") m_Mix = ClampParam(value, 0.0f, 1.0f);
    }
private:
    uint32_t m_SampleRate{48000};
    uint32_t m_Size{4800};
    float m_RateHz{1.0f};
    float m_DepthMs{5.0f};
    float m_Mix{0.5f};
    std::vector<std::vector<float>> m_Buffers;
    std::vector<uint32_t> m_WriteIndex;
    std::vector<float> m_Phase;
};

} // namespace

// ============================================================================
// Type metadata
// ============================================================================

std::string AudioEffectTypeToString(AudioEffectType type) {
    switch (type) {
        case AudioEffectType::Gain: return "Gain";
        case AudioEffectType::LowPass: return "LowPass";
        case AudioEffectType::HighPass: return "HighPass";
        case AudioEffectType::BandPass: return "BandPass";
        case AudioEffectType::Peak: return "Peak";
        case AudioEffectType::LowShelf: return "LowShelf";
        case AudioEffectType::HighShelf: return "HighShelf";
        case AudioEffectType::Delay: return "Delay";
        case AudioEffectType::Reverb: return "Reverb";
        case AudioEffectType::Compressor: return "Compressor";
        case AudioEffectType::Distortion: return "Distortion";
        case AudioEffectType::Chorus: return "Chorus";
    }
    return "Gain";
}

bool AudioEffectTypeFromString(const std::string& name, AudioEffectType& outType) {
    static const std::map<std::string, AudioEffectType> table = {
        {"Gain", AudioEffectType::Gain},
        {"LowPass", AudioEffectType::LowPass},
        {"HighPass", AudioEffectType::HighPass},
        {"BandPass", AudioEffectType::BandPass},
        {"Peak", AudioEffectType::Peak},
        {"LowShelf", AudioEffectType::LowShelf},
        {"HighShelf", AudioEffectType::HighShelf},
        {"Delay", AudioEffectType::Delay},
        {"Reverb", AudioEffectType::Reverb},
        {"Compressor", AudioEffectType::Compressor},
        {"Distortion", AudioEffectType::Distortion},
        {"Chorus", AudioEffectType::Chorus},
    };
    auto it = table.find(name);
    if (it == table.end()) {
        return false;
    }
    outType = it->second;
    return true;
}

std::map<std::string, float> DefaultEffectParameters(AudioEffectType type) {
    switch (type) {
        case AudioEffectType::Gain:
            return {{"gain_db", 0.0f}};
        case AudioEffectType::LowPass:
            return {{"cutoff_hz", 8000.0f}, {"resonance", 0.707f}};
        case AudioEffectType::HighPass:
            return {{"cutoff_hz", 120.0f}, {"resonance", 0.707f}};
        case AudioEffectType::BandPass:
            return {{"cutoff_hz", 1000.0f}, {"resonance", 1.0f}};
        case AudioEffectType::Peak:
            return {{"frequency_hz", 1000.0f}, {"resonance", 1.0f}, {"gain_db", 0.0f}};
        case AudioEffectType::LowShelf:
            return {{"frequency_hz", 200.0f}, {"gain_db", 0.0f}, {"resonance", 0.707f}};
        case AudioEffectType::HighShelf:
            return {{"frequency_hz", 4000.0f}, {"gain_db", 0.0f}, {"resonance", 0.707f}};
        case AudioEffectType::Delay:
            return {{"time_ms", 300.0f}, {"feedback", 0.4f}, {"wet", 0.4f}, {"dry", 1.0f}};
        case AudioEffectType::Reverb:
            return {{"room_size", 0.5f}, {"damping", 0.5f}, {"wet", 0.33f}, {"dry", 1.0f}, {"width", 1.0f}};
        case AudioEffectType::Compressor:
            return {{"threshold_db", -12.0f}, {"ratio", 4.0f}, {"attack_ms", 10.0f},
                    {"release_ms", 100.0f}, {"makeup_db", 0.0f}};
        case AudioEffectType::Distortion:
            return {{"drive_db", 12.0f}, {"wet", 0.5f}, {"dry", 0.5f}};
        case AudioEffectType::Chorus:
            return {{"rate_hz", 1.0f}, {"depth_ms", 5.0f}, {"mix", 0.5f}};
    }
    return {};
}

// ============================================================================
// AudioEffectDesc
// ============================================================================

float AudioEffectDesc::GetParameter(const std::string& name, float fallback) const {
    auto it = parameters.find(name);
    return (it != parameters.end()) ? it->second : fallback;
}

void AudioEffectDesc::SetParameter(const std::string& name, float value) {
    parameters[name] = value;
}

nlohmann::json AudioEffectDesc::Serialize() const {
    nlohmann::json json;
    json["type"] = AudioEffectTypeToString(type);
    json["enabled"] = enabled;
    nlohmann::json params = nlohmann::json::object();
    for (const auto& entry : parameters) {
        params[entry.first] = entry.second;
    }
    json["parameters"] = params;
    return json;
}

AudioEffectDesc AudioEffectDesc::Deserialize(const nlohmann::json& json) {
    AudioEffectDesc desc;
    AudioEffectType type = AudioEffectType::Gain;
    if (json.contains("type") && json["type"].is_string()) {
        AudioEffectTypeFromString(json["type"].get<std::string>(), type);
    }
    desc.type = type;
    desc.parameters = DefaultEffectParameters(type);
    if (json.contains("enabled") && json["enabled"].is_boolean()) {
        desc.enabled = json["enabled"].get<bool>();
    }
    if (json.contains("parameters") && json["parameters"].is_object()) {
        for (auto it = json["parameters"].begin(); it != json["parameters"].end(); ++it) {
            if (it.value().is_number()) {
                desc.parameters[it.key()] = it.value().get<float>();
            }
        }
    }
    return desc;
}

// ============================================================================
// AudioEffectProcessor factory
// ============================================================================

std::unique_ptr<AudioEffectProcessor> AudioEffectProcessor::Create(AudioEffectType type) {
    switch (type) {
        case AudioEffectType::Gain: return std::make_unique<GainProcessor>();
        case AudioEffectType::LowPass: return std::make_unique<BiquadProcessor>(BiquadMode::LowPass);
        case AudioEffectType::HighPass: return std::make_unique<BiquadProcessor>(BiquadMode::HighPass);
        case AudioEffectType::BandPass: return std::make_unique<BiquadProcessor>(BiquadMode::BandPass);
        case AudioEffectType::Peak: return std::make_unique<BiquadProcessor>(BiquadMode::Peak);
        case AudioEffectType::LowShelf: return std::make_unique<BiquadProcessor>(BiquadMode::LowShelf);
        case AudioEffectType::HighShelf: return std::make_unique<BiquadProcessor>(BiquadMode::HighShelf);
        case AudioEffectType::Delay: return std::make_unique<DelayProcessor>();
        case AudioEffectType::Reverb: return std::make_unique<ReverbProcessor>();
        case AudioEffectType::Compressor: return std::make_unique<CompressorProcessor>();
        case AudioEffectType::Distortion: return std::make_unique<DistortionProcessor>();
        case AudioEffectType::Chorus: return std::make_unique<ChorusProcessor>();
    }
    return std::make_unique<GainProcessor>();
}

// ============================================================================
// BusEffectChain
// ============================================================================

void BusEffectChain::Rebuild(const std::vector<AudioEffectDesc>& effects,
                             uint32_t channels, uint32_t sampleRate) {
    std::vector<Slot> slots;
    slots.reserve(effects.size());
    for (const AudioEffectDesc& desc : effects) {
        Slot slot;
        slot.processor = AudioEffectProcessor::Create(desc.type);
        slot.processor->Prepare(channels, sampleRate);
        for (const auto& param : desc.parameters) {
            slot.processor->SetParameter(param.first, param.second);
        }
        slot.enabled = desc.enabled;
        slots.push_back(std::move(slot));
    }

    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Channels = channels;
    m_SampleRate = sampleRate;
    m_Slots = std::move(slots);
    m_SlotCount.store(m_Slots.size());
}

void BusEffectChain::UpdateParameter(size_t index, const std::string& name, float value) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (index < m_Slots.size() && m_Slots[index].processor) {
        m_Slots[index].processor->SetParameter(name, value);
    }
}

void BusEffectChain::SetEnabled(size_t index, bool enabled) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (index < m_Slots.size()) {
        m_Slots[index].enabled = enabled;
    }
}

void BusEffectChain::Process(float* frames, uint32_t frameCount, uint32_t channels) {
    if (m_SlotCount.load() == 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_Mutex);
    for (Slot& slot : m_Slots) {
        if (slot.enabled && slot.processor) {
            slot.processor->Process(frames, frameCount, channels);
        }
    }
}

bool BusEffectChain::IsEmpty() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_Slots.empty();
}

} // namespace audio
} // namespace lupine
