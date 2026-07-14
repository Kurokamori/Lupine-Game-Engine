#pragma once

#include "Asset.hpp"
#include <vector>
#include <cstdint>

namespace lupine {
namespace asset {

/**
 * Audio format enumeration
 */
enum class AudioFormat {
    Unknown,
    Mono8,      // 8-bit mono
    Mono16,     // 16-bit mono
    Stereo8,    // 8-bit stereo
    Stereo16    // 16-bit stereo
};

/**
 * Audio loading mode
 */
enum class AudioLoadMode {
    Preload,    // Load entire audio file into memory (for sound effects)
    Streaming   // Stream audio from disk (for music/long audio)
};

/**
 * Audio data structure
 */
struct AudioData {
    std::vector<uint8_t> data;      // Raw audio data
    uint32_t sampleRate{0};         // Sample rate (e.g., 44100, 48000)
    uint32_t channels{0};           // Number of channels (1=mono, 2=stereo)
    uint32_t bitsPerSample{0};      // Bits per sample (8, 16, 24, 32)
    AudioFormat format{AudioFormat::Unknown};
    float duration{0.0f};           // Duration in seconds
};

/**
 * Audio asset class
 * Supports WAV, MP3, OGG, FLAC formats via miniaudio
 * Can be loaded as preloaded (sound effects) or streaming (music)
 */
class AudioAsset : public Asset {
public:
    AudioAsset();
    explicit AudioAsset(const core::UUID& uuid);
    virtual ~AudioAsset();

    AssetType GetType() const override { return AssetType::Audio; }

    // Load from file
    bool LoadFromFile(const std::string& filepath, AudioLoadMode loadMode = AudioLoadMode::Preload);

    // Build a preloaded asset directly from in-memory interleaved PCM samples.
    // Used to play the decoded audio track of a video through AudioManager.
    // `channels` must be 1 or 2 and `bitsPerSample` must be 16 (the format the
    // audio backend plays preloaded buffers as). Returns false on invalid input.
    bool LoadFromPCM(const uint8_t* pcm, size_t sizeBytes, uint32_t sampleRate,
                     uint32_t channels, uint32_t bitsPerSample = 16);

    // Audio properties
    uint32_t GetSampleRate() const { return m_AudioData.sampleRate; }
    uint32_t GetChannels() const { return m_AudioData.channels; }
    uint32_t GetBitsPerSample() const { return m_AudioData.bitsPerSample; }
    AudioFormat GetFormat() const { return m_AudioData.format; }
    float GetDuration() const { return m_AudioData.duration; }
    AudioLoadMode GetLoadMode() const { return m_LoadMode; }
    
    // Data access
    const uint8_t* GetData() const;
    size_t GetDataSize() const;
    const AudioData& GetAudioData() const { return m_AudioData; }
    
    // Streaming support
    bool IsStreaming() const { return m_LoadMode == AudioLoadMode::Streaming; }
    
    // For streaming: read a chunk of data
    // Returns number of bytes read, 0 if end of stream
    size_t ReadStreamChunk(uint8_t* buffer, size_t bufferSize, size_t offset);

private:
    AudioData m_AudioData;
    AudioLoadMode m_LoadMode{AudioLoadMode::Preload};
    
    // File handle for streaming (platform-specific, will use FILE* for now)
    void* m_StreamHandle{nullptr};
    size_t m_StreamDataOffset{0};  // Offset in file where audio data starts
    size_t m_StreamDataSize{0};    // Size of audio data in file
    
    // Helper methods for different formats
    bool LoadWAV(const std::string& filepath);
    bool LoadOGG(const std::string& filepath);
    bool LoadMP3(const std::string& filepath);
    // Memory-based loading for pack file support
    bool LoadWAVFromMemory(const uint8_t* data, size_t dataSize);
    bool LoadMP3FromMemory(const uint8_t* data, size_t dataSize);
    // Note: FLAC uses LoadMP3 which handles all formats via miniaudio decoder

    // Determine format from file extension
    std::string GetFileExtension(const std::string& filepath) const;
};

} // namespace asset
} // namespace lupine

