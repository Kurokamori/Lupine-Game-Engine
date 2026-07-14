#include "lupine/asset/AudioAsset.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/platform/FileSystem.hpp"
#include "lupine/platform/PackFile.hpp"
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <exception>

#include <fstream>

#include <miniaudio.h>

namespace lupine {
namespace asset {

namespace {

// "RIFF" + size + "WAVE"
constexpr uint64_t kRiffHeaderBytes = 12;

// 4-byte chunk id + 4-byte chunk size
constexpr uint64_t kChunkHeaderBytes = 8;

// The 16-byte PCM fmt body (format, channels, sample rate, byte rate, block align, bits).
constexpr uint64_t kMinFmtChunkBytes = 16;

// RIFF header + a minimal fmt chunk + an empty data chunk header.
constexpr long kMinWavFileBytes = 44;

// Upper bound on a decoded WAV data chunk. Sizes above this are rejected outright rather
// than passed to resize(), which would otherwise throw length_error/bad_alloc.
constexpr uint64_t kMaxWavDataBytes = 1ull << 31;

// MSVC deprecates fopen and offers only the non-portable fopen_s, so the C stdio
// reader below goes through this wrapper. Returns nullptr when the file cannot open.
FILE* OpenBinaryFileForRead(const std::string& filepath) {
#if defined(_MSC_VER)
    FILE* file = nullptr;
    if (fopen_s(&file, filepath.c_str(), "rb") != 0) {
        return nullptr;
    }
    return file;
#else
    return std::fopen(filepath.c_str(), "rb");
#endif
}

// Reads exactly `size` bytes or fails; the raw fread() return value must never be ignored
// when parsing an untrusted file, or a truncated read leaves the destination uninitialized.
bool ReadExact(FILE* file, void* dest, size_t size) {
    return std::fread(dest, 1, size, file) == size;
}

// Returns the total length of an open binary file, or 0 if it cannot be determined.
long GetFileLength(FILE* file) {
    const long current = std::ftell(file);
    if (current < 0 || std::fseek(file, 0, SEEK_END) != 0) {
        return 0;
    }

    const long length = std::ftell(file);
    if (length < 0 || std::fseek(file, current, SEEK_SET) != 0) {
        return 0;
    }

    return length;
}

// RIFF chunks are word-aligned: a chunk with an odd size is followed by one pad byte that
// is not counted in chunkSize. Advancing by chunkSize alone desynchronizes the parser.
uint64_t PaddedChunkSize(uint32_t chunkSize) {
    return static_cast<uint64_t>(chunkSize) + (chunkSize & 1u);
}

} // namespace

AudioAsset::AudioAsset()
    : Asset() {
}

AudioAsset::AudioAsset(const core::UUID& uuid)
    : Asset(uuid) {
}

AudioAsset::~AudioAsset() {

    if (m_StreamHandle) {
        fclose(static_cast<FILE*>(m_StreamHandle));
        m_StreamHandle = nullptr;
    }
}

bool AudioAsset::LoadFromFile(const std::string& filepath, AudioLoadMode loadMode) {
    // A reload of a streaming clip must not orphan the handle opened by the previous load.
    if (m_StreamHandle) {
        fclose(static_cast<FILE*>(m_StreamHandle));
        m_StreamHandle = nullptr;
    }
    m_StreamDataOffset = 0;
    m_StreamDataSize = 0;
    m_AudioData.data.clear();

    // Store the path (will be converted to res:// format if possible)
    SetPath(filepath);
    m_LoadMode = loadMode;

    // Resolve the path using the asset system (handles UUID fallback)
    std::string resolvedPath = ResolveAssetPath(filepath);

    std::string ext = GetFileExtension(filepath);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    // Check if running from pack file first
    std::vector<uint8_t> packData;
    auto& packFS = platform::PackFileSystem::Instance();
    std::string packPath = packFS.resolveAsset(filepath);
    bool fromPack = packFS.isPackMode() && packFS.exists(packPath);

    if (fromPack) {
        packData = packFS.readAsset(filepath);
        if (packData.empty()) {
            LOG_ERROR(LogCategory::Core, "AudioAsset: Failed to read from pack: {}", filepath);
            return false;
        }
    }

    bool success = false;
    try {
        if (ext == ".wav") {
            if (fromPack) {
                success = LoadWAVFromMemory(packData.data(), packData.size());
            } else {
                success = LoadWAV(resolvedPath);
            }
        } else if (ext == ".ogg" || ext == ".mp3" || ext == ".flac") {
            if (fromPack) {
                success = LoadMP3FromMemory(packData.data(), packData.size());
            } else {
                success = LoadMP3(resolvedPath);
            }
        } else {
            LOG_ERROR(LogCategory::Core, "AudioAsset: Unsupported format: {}", ext);
            return false;
        }
    } catch (const std::exception& e) {
        // Decoders allocate buffers sized from file-supplied values; a malformed clip must
        // fail this asset load, not tear down the process.
        LOG_ERROR(LogCategory::Core, "AudioAsset: exception while decoding '{}': {}", filepath, e.what());
        m_AudioData.data.clear();
        return false;
    }

    if (success) {
        SetLoaded(true);
        // Report the in-memory footprint: the decoded PCM buffer for preloaded
        // clips, ~0 for streaming clips (which read from disk on demand).
        SetTrackedBytes(m_AudioData.data.size());
    } else {
        LOG_ERROR(LogCategory::Core, "AudioAsset: Failed to load: {}", filepath);
    }

    return success;
}

bool AudioAsset::LoadFromPCM(const uint8_t* pcm, size_t sizeBytes, uint32_t sampleRate,
                             uint32_t channels, uint32_t bitsPerSample) {
    if (!pcm || sizeBytes == 0 || sampleRate == 0 ||
        (channels != 1 && channels != 2) || bitsPerSample != 16) {
        return false;
    }

    m_LoadMode = AudioLoadMode::Preload;
    m_AudioData.sampleRate = sampleRate;
    m_AudioData.channels = channels;
    m_AudioData.bitsPerSample = bitsPerSample;
    m_AudioData.format = (channels == 1) ? AudioFormat::Mono16 : AudioFormat::Stereo16;
    m_AudioData.data.assign(pcm, pcm + sizeBytes);

    uint32_t bytesPerFrame = (bitsPerSample / 8) * channels;
    m_AudioData.duration = (bytesPerFrame > 0)
        ? static_cast<float>(sizeBytes / bytesPerFrame) / static_cast<float>(sampleRate)
        : 0.0f;

    SetLoaded(true);
    SetTrackedBytes(m_AudioData.data.size());
    return true;
}

const uint8_t* AudioAsset::GetData() const {
    if (m_AudioData.data.empty()) {
        return nullptr;
    }
    return m_AudioData.data.data();
}

size_t AudioAsset::GetDataSize() const {
    return m_AudioData.data.size();
}

size_t AudioAsset::ReadStreamChunk(uint8_t* buffer, size_t bufferSize, size_t offset) {
    if (!IsStreaming() || !m_StreamHandle) {
        return 0;
    }

    if (!buffer || bufferSize == 0) {
        return 0;
    }

    // At or past end-of-clip. Without this guard the subtraction below underflows and the
    // read runs off the end of the data chunk into whatever trailing chunks follow it.
    if (offset >= m_StreamDataSize) {
        return 0;
    }

    FILE* file = static_cast<FILE*>(m_StreamHandle);

    if (fseek(file, static_cast<long>(m_StreamDataOffset + offset), SEEK_SET) != 0) {
        return 0;
    }

    size_t remainingData = m_StreamDataSize - offset;
    size_t toRead = std::min(bufferSize, remainingData);

    size_t bytesRead = fread(buffer, 1, toRead, file);
    return bytesRead;
}

std::string AudioAsset::GetFileExtension(const std::string& filepath) const {
    size_t dotPos = filepath.find_last_of('.');
    if (dotPos == std::string::npos) {
        return "";
    }
    return filepath.substr(dotPos);
}

bool AudioAsset::LoadWAV(const std::string& filepath) {

    FILE* file = OpenBinaryFileForRead(filepath);
    if (!file) {
        LOG_ERROR(LogCategory::Asset, "AudioAsset: failed to open WAV file '{}'", filepath);
        return false;
    }

    const long fileLengthSigned = GetFileLength(file);
    if (fileLengthSigned < kMinWavFileBytes) {
        LOG_ERROR(LogCategory::Asset, "AudioAsset: WAV file '{}' is too small to be valid", filepath);
        fclose(file);
        return false;
    }
    const uint64_t fileLength = static_cast<uint64_t>(fileLengthSigned);

    char riff[4];
    uint32_t riffSize;
    char wave[4];

    if (!ReadExact(file, riff, 4) || !ReadExact(file, &riffSize, 4) || !ReadExact(file, wave, 4)) {
        LOG_ERROR(LogCategory::Asset, "AudioAsset: truncated RIFF header in '{}'", filepath);
        fclose(file);
        return false;
    }

    if (strncmp(riff, "RIFF", 4) != 0 || strncmp(wave, "WAVE", 4) != 0) {
        LOG_ERROR(LogCategory::Asset, "AudioAsset: '{}' is not a RIFF/WAVE file", filepath);
        fclose(file);
        return false;
    }

    bool foundFmt = false;
    bool foundData = false;
    uint64_t dataOffset = 0;
    uint64_t dataSize = 0;

    uint64_t cursor = kRiffHeaderBytes;
    while (cursor + kChunkHeaderBytes <= fileLength) {
        if (fseek(file, static_cast<long>(cursor), SEEK_SET) != 0) {
            break;
        }

        char chunkId[4];
        uint32_t chunkSize;
        if (!ReadExact(file, chunkId, 4) || !ReadExact(file, &chunkSize, 4)) {
            break;
        }

        const uint64_t chunkStart = cursor + kChunkHeaderBytes;

        // The declared chunk size is untrusted: a crafted file can claim a chunk far larger
        // than the file itself. Clamp what we are willing to read to what actually exists.
        const uint64_t chunkBytes = std::min<uint64_t>(chunkSize, fileLength - chunkStart);

        if (!foundFmt && strncmp(chunkId, "fmt ", 4) == 0) {
            if (chunkBytes < kMinFmtChunkBytes) {
                LOG_ERROR(LogCategory::Asset, "AudioAsset: truncated fmt chunk in '{}'", filepath);
                fclose(file);
                return false;
            }

            uint16_t audioFormat;
            uint16_t numChannels;
            uint32_t sampleRate;
            uint32_t byteRate;
            uint16_t blockAlign;
            uint16_t bitsPerSample;

            if (!ReadExact(file, &audioFormat, 2) ||
                !ReadExact(file, &numChannels, 2) ||
                !ReadExact(file, &sampleRate, 4) ||
                !ReadExact(file, &byteRate, 4) ||
                !ReadExact(file, &blockAlign, 2) ||
                !ReadExact(file, &bitsPerSample, 2)) {
                LOG_ERROR(LogCategory::Asset, "AudioAsset: truncated fmt chunk in '{}'", filepath);
                fclose(file);
                return false;
            }

            m_AudioData.sampleRate = sampleRate;
            m_AudioData.channels = numChannels;
            m_AudioData.bitsPerSample = bitsPerSample;

            if (numChannels == 1 && bitsPerSample == 8) {
                m_AudioData.format = AudioFormat::Mono8;
            } else if (numChannels == 1 && bitsPerSample == 16) {
                m_AudioData.format = AudioFormat::Mono16;
            } else if (numChannels == 2 && bitsPerSample == 8) {
                m_AudioData.format = AudioFormat::Stereo8;
            } else if (numChannels == 2 && bitsPerSample == 16) {
                m_AudioData.format = AudioFormat::Stereo16;
            } else {
                LOG_ERROR(LogCategory::Asset,
                          "AudioAsset: unsupported WAV format in '{}' ({} channels, {} bits)",
                          filepath, numChannels, bitsPerSample);
                fclose(file);
                return false;
            }

            foundFmt = true;
        } else if (!foundData && strncmp(chunkId, "data", 4) == 0) {
            dataOffset = chunkStart;
            dataSize = chunkBytes;
            foundData = true;
        }

        if (foundFmt && foundData) {
            break;
        }

        // Advance by the DECLARED size (plus the RIFF word-alignment pad byte), not the
        // clamped size, so a chunk that overruns the file terminates the loop below.
        cursor = chunkStart + PaddedChunkSize(chunkSize);
    }

    if (!foundFmt || !foundData) {
        LOG_ERROR(LogCategory::Asset, "AudioAsset: '{}' is missing a {} chunk",
                  filepath, foundFmt ? "data" : "fmt ");
        fclose(file);
        return false;
    }

    if (dataSize > kMaxWavDataBytes) {
        LOG_ERROR(LogCategory::Asset, "AudioAsset: WAV data chunk in '{}' is unreasonably large ({} bytes)",
                  filepath, dataSize);
        fclose(file);
        return false;
    }

    m_StreamDataOffset = static_cast<size_t>(dataOffset);
    m_StreamDataSize = static_cast<size_t>(dataSize);

    const uint32_t bytesPerSample = (m_AudioData.bitsPerSample / 8) * m_AudioData.channels;
    if (bytesPerSample > 0 && m_AudioData.sampleRate > 0) {
        m_AudioData.duration = static_cast<float>(dataSize) /
                               static_cast<float>(bytesPerSample * m_AudioData.sampleRate);
    } else {
        m_AudioData.duration = 0.0f;
    }

    if (m_LoadMode == AudioLoadMode::Preload) {
        const size_t preloadSize = static_cast<size_t>(dataSize);

        if (fseek(file, static_cast<long>(dataOffset), SEEK_SET) != 0) {
            LOG_ERROR(LogCategory::Asset, "AudioAsset: failed to seek to WAV data in '{}'", filepath);
            fclose(file);
            return false;
        }

        m_AudioData.data.resize(preloadSize);

        if (preloadSize > 0 && !ReadExact(file, m_AudioData.data.data(), preloadSize)) {
            LOG_ERROR(LogCategory::Asset, "AudioAsset: truncated WAV data chunk in '{}'", filepath);
            m_AudioData.data.clear();
            fclose(file);
            return false;
        }

        fclose(file);
    } else {
        m_StreamHandle = file;
    }

    return true;
}

bool AudioAsset::LoadOGG(const std::string& filepath) {

    return LoadMP3(filepath);
}

bool AudioAsset::LoadMP3(const std::string& filepath) {

    ma_decoder decoder;
    ma_decoder_config config = ma_decoder_config_init(ma_format_s16, 0, 0);

    ma_result result = ma_decoder_init_file(filepath.c_str(), &config, &decoder);
    if (result != MA_SUCCESS) {

        return false;
    }

    m_AudioData.sampleRate = decoder.outputSampleRate;
    m_AudioData.channels = decoder.outputChannels;
    m_AudioData.bitsPerSample = 16;

    if (decoder.outputChannels == 1) {
        m_AudioData.format = AudioFormat::Mono16;
    } else if (decoder.outputChannels == 2) {
        m_AudioData.format = AudioFormat::Stereo16;
    } else {

        ma_decoder_uninit(&decoder);
        return false;
    }

    ma_uint64 totalFrames;
    result = ma_decoder_get_length_in_pcm_frames(&decoder, &totalFrames);
    if (result != MA_SUCCESS) {

        totalFrames = 0;
    }

    if (m_LoadMode == AudioLoadMode::Preload) {

        if (totalFrames > 0) {

            ma_uint64 totalSamples = totalFrames * decoder.outputChannels;
            size_t dataSize = totalSamples * sizeof(int16_t);
            m_AudioData.data.resize(dataSize);

            ma_uint64 framesRead = 0;
            ma_result readResult = ma_decoder_read_pcm_frames(&decoder, m_AudioData.data.data(), totalFrames, &framesRead);
            if (readResult != MA_SUCCESS || framesRead != totalFrames) {

            }

            if (m_AudioData.sampleRate > 0) {
                m_AudioData.duration = static_cast<float>(framesRead) / m_AudioData.sampleRate;
            }
        } else {

            std::vector<int16_t> tempBuffer;
            const ma_uint64 framesToRead = 4096;
            int16_t* chunkBuffer = new int16_t[framesToRead * decoder.outputChannels];

            ma_uint64 totalFramesRead = 0;
            while (true) {
                ma_uint64 framesRead = 0;
                ma_result readResult = ma_decoder_read_pcm_frames(&decoder, chunkBuffer, framesToRead, &framesRead);
                if (readResult != MA_SUCCESS || framesRead == 0) {
                    break;
                }

                size_t samplesRead = framesRead * decoder.outputChannels;
                tempBuffer.insert(tempBuffer.end(), chunkBuffer, chunkBuffer + samplesRead);
                totalFramesRead += framesRead;

                if (framesRead < framesToRead) {
                    break;
                }
            }

            delete[] chunkBuffer;

            m_AudioData.data.resize(tempBuffer.size() * sizeof(int16_t));
            memcpy(m_AudioData.data.data(), tempBuffer.data(), m_AudioData.data.size());

            if (m_AudioData.sampleRate > 0) {
                m_AudioData.duration = static_cast<float>(totalFramesRead) / m_AudioData.sampleRate;
            }
        }

        ma_decoder_uninit(&decoder);
    } else {

        if (totalFrames > 0 && m_AudioData.sampleRate > 0) {
            m_AudioData.duration = static_cast<float>(totalFrames) / m_AudioData.sampleRate;
        } else {
            m_AudioData.duration = 0.0f;
        }

        ma_decoder_uninit(&decoder);

    }

    return true;
}

bool AudioAsset::LoadWAVFromMemory(const uint8_t* data, size_t dataSize) {
    if (!data || dataSize < static_cast<size_t>(kMinWavFileBytes)) {
        return false;
    }

    const uint64_t bufferLength = static_cast<uint64_t>(dataSize);

    if (memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WAVE", 4) != 0) {
        return false;
    }

    bool foundFmt = false;
    bool foundData = false;
    uint64_t dataChunkOffset = 0;
    uint64_t dataChunkSize = 0;

    // Cursor arithmetic is done on 64-bit offsets rather than pointers: an untrusted
    // 32-bit chunkSize added to a pointer can wrap and defeat an "end" comparison.
    uint64_t cursor = kRiffHeaderBytes;
    while (cursor + kChunkHeaderBytes <= bufferLength) {
        char chunkId[4];
        memcpy(chunkId, data + cursor, 4);

        uint32_t chunkSize;
        memcpy(&chunkSize, data + cursor + 4, 4);

        const uint64_t chunkStart = cursor + kChunkHeaderBytes;
        const uint64_t chunkBytes = std::min<uint64_t>(chunkSize, bufferLength - chunkStart);

        if (!foundFmt && strncmp(chunkId, "fmt ", 4) == 0) {
            if (chunkBytes < kMinFmtChunkBytes) {
                return false;
            }

            const uint8_t* fmt = data + chunkStart;

            uint16_t audioFormat, numChannels, blockAlign, bitsPerSample;
            uint32_t sampleRate, byteRate;

            memcpy(&audioFormat, fmt + 0, 2);
            memcpy(&numChannels, fmt + 2, 2);
            memcpy(&sampleRate, fmt + 4, 4);
            memcpy(&byteRate, fmt + 8, 4);
            memcpy(&blockAlign, fmt + 12, 2);
            memcpy(&bitsPerSample, fmt + 14, 2);

            m_AudioData.sampleRate = sampleRate;
            m_AudioData.channels = numChannels;
            m_AudioData.bitsPerSample = bitsPerSample;

            if (numChannels == 1 && bitsPerSample == 8) {
                m_AudioData.format = AudioFormat::Mono8;
            } else if (numChannels == 1 && bitsPerSample == 16) {
                m_AudioData.format = AudioFormat::Mono16;
            } else if (numChannels == 2 && bitsPerSample == 8) {
                m_AudioData.format = AudioFormat::Stereo8;
            } else if (numChannels == 2 && bitsPerSample == 16) {
                m_AudioData.format = AudioFormat::Stereo16;
            } else {
                return false;
            }

            foundFmt = true;
        } else if (!foundData && strncmp(chunkId, "data", 4) == 0) {
            dataChunkOffset = chunkStart;
            dataChunkSize = chunkBytes;
            foundData = true;
        }

        if (foundFmt && foundData) {
            break;
        }

        cursor = chunkStart + PaddedChunkSize(chunkSize);
    }

    if (!foundFmt || !foundData) {
        return false;
    }

    const uint32_t bytesPerSample = (m_AudioData.bitsPerSample / 8) * m_AudioData.channels;
    if (bytesPerSample > 0 && m_AudioData.sampleRate > 0) {
        m_AudioData.duration = static_cast<float>(dataChunkSize) /
                               static_cast<float>(bytesPerSample * m_AudioData.sampleRate);
    } else {
        m_AudioData.duration = 0.0f;
    }

    // Always preload when loading from memory
    const size_t preloadSize = static_cast<size_t>(dataChunkSize);
    m_AudioData.data.resize(preloadSize);
    if (preloadSize > 0) {
        memcpy(m_AudioData.data.data(), data + dataChunkOffset, preloadSize);
    }

    return true;
}

bool AudioAsset::LoadMP3FromMemory(const uint8_t* data, size_t dataSize) {
    ma_decoder decoder;
    ma_decoder_config config = ma_decoder_config_init(ma_format_s16, 0, 0);

    ma_result result = ma_decoder_init_memory(data, dataSize, &config, &decoder);
    if (result != MA_SUCCESS) {
        return false;
    }

    m_AudioData.sampleRate = decoder.outputSampleRate;
    m_AudioData.channels = decoder.outputChannels;
    m_AudioData.bitsPerSample = 16;

    if (decoder.outputChannels == 1) {
        m_AudioData.format = AudioFormat::Mono16;
    } else if (decoder.outputChannels == 2) {
        m_AudioData.format = AudioFormat::Stereo16;
    } else {
        ma_decoder_uninit(&decoder);
        return false;
    }

    ma_uint64 totalFrames;
    result = ma_decoder_get_length_in_pcm_frames(&decoder, &totalFrames);
    if (result != MA_SUCCESS) {
        totalFrames = 0;
    }

    // Always preload when loading from memory
    if (totalFrames > 0) {
        ma_uint64 totalSamples = totalFrames * decoder.outputChannels;
        size_t audioDataSize = totalSamples * sizeof(int16_t);
        m_AudioData.data.resize(audioDataSize);

        ma_uint64 framesRead = 0;
        ma_decoder_read_pcm_frames(&decoder, m_AudioData.data.data(), totalFrames, &framesRead);

        if (m_AudioData.sampleRate > 0) {
            m_AudioData.duration = static_cast<float>(framesRead) / m_AudioData.sampleRate;
        }
    } else {
        std::vector<int16_t> tempBuffer;
        const ma_uint64 framesToRead = 4096;
        int16_t* chunkBuffer = new int16_t[framesToRead * decoder.outputChannels];

        ma_uint64 totalFramesRead = 0;
        while (true) {
            ma_uint64 framesRead = 0;
            ma_result readResult = ma_decoder_read_pcm_frames(&decoder, chunkBuffer, framesToRead, &framesRead);
            if (readResult != MA_SUCCESS || framesRead == 0) {
                break;
            }

            size_t samplesRead = framesRead * decoder.outputChannels;
            tempBuffer.insert(tempBuffer.end(), chunkBuffer, chunkBuffer + samplesRead);
            totalFramesRead += framesRead;

            if (framesRead < framesToRead) {
                break;
            }
        }

        delete[] chunkBuffer;

        m_AudioData.data.resize(tempBuffer.size() * sizeof(int16_t));
        memcpy(m_AudioData.data.data(), tempBuffer.data(), m_AudioData.data.size());

        if (m_AudioData.sampleRate > 0) {
            m_AudioData.duration = static_cast<float>(totalFramesRead) / m_AudioData.sampleRate;
        }
    }

    ma_decoder_uninit(&decoder);
    return true;
}

}
}

