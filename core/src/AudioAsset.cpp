#include "lupine/asset/AudioAsset.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/platform/FileSystem.hpp"
#include <cstring>
#include <algorithm>

#include <fstream>

#include <miniaudio.h>

namespace lupine {
namespace asset {

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

    SetPath(filepath);
    m_LoadMode = loadMode;

    std::string ext = GetFileExtension(filepath);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    bool success = false;
    if (ext == ".wav") {
        success = LoadWAV(filepath);
    } else if (ext == ".ogg") {
        success = LoadOGG(filepath);
    } else if (ext == ".mp3") {
        success = LoadMP3(filepath);
    } else if (ext == ".flac") {

        success = LoadMP3(filepath);
    } else {

        return false;
    }

    if (success) {
        SetLoaded(true);

    } else {

    }

    return success;
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

    FILE* file = static_cast<FILE*>(m_StreamHandle);

    fseek(file, static_cast<long>(m_StreamDataOffset + offset), SEEK_SET);

    size_t remainingData = m_StreamDataSize - offset;
    size_t toRead = std::min(bufferSize, remainingData);

    if (toRead == 0) {
        return 0;
    }

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

    FILE* file = fopen(filepath.c_str(), "rb");
    if (!file) {

        return false;
    }

    char riff[4];
    uint32_t fileSize;
    char wave[4];

    fread(riff, 1, 4, file);
    fread(&fileSize, 4, 1, file);
    fread(wave, 1, 4, file);

    if (strncmp(riff, "RIFF", 4) != 0 || strncmp(wave, "WAVE", 4) != 0) {

        fclose(file);
        return false;
    }

    char chunkId[4];
    uint32_t chunkSize;
    bool foundFmt = false;

    while (fread(chunkId, 1, 4, file) == 4) {
        fread(&chunkSize, 4, 1, file);

        if (strncmp(chunkId, "fmt ", 4) == 0) {
            foundFmt = true;

            uint16_t audioFormat;
            uint16_t numChannels;
            uint32_t sampleRate;
            uint32_t byteRate;
            uint16_t blockAlign;
            uint16_t bitsPerSample;

            fread(&audioFormat, 2, 1, file);
            fread(&numChannels, 2, 1, file);
            fread(&sampleRate, 4, 1, file);
            fread(&byteRate, 4, 1, file);
            fread(&blockAlign, 2, 1, file);
            fread(&bitsPerSample, 2, 1, file);

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

                fclose(file);
                return false;
            }

            if (chunkSize > 16) {
                fseek(file, chunkSize - 16, SEEK_CUR);
            }

            break;
        } else {

            fseek(file, chunkSize, SEEK_CUR);
        }
    }

    if (!foundFmt) {

        fclose(file);
        return false;
    }

    fseek(file, 12, SEEK_SET);
    bool foundData = false;

    while (fread(chunkId, 1, 4, file) == 4) {
        fread(&chunkSize, 4, 1, file);

        if (strncmp(chunkId, "data", 4) == 0) {
            foundData = true;
            m_StreamDataOffset = ftell(file);
            m_StreamDataSize = chunkSize;

            uint32_t bytesPerSample = (m_AudioData.bitsPerSample / 8) * m_AudioData.channels;
            if (bytesPerSample > 0 && m_AudioData.sampleRate > 0) {
                m_AudioData.duration = static_cast<float>(chunkSize) /
                                      (bytesPerSample * m_AudioData.sampleRate);
            }

            if (m_LoadMode == AudioLoadMode::Preload) {

                m_AudioData.data.resize(chunkSize);
                fread(m_AudioData.data.data(), 1, chunkSize, file);
                fclose(file);
            } else {

                m_StreamHandle = file;
            }

            return true;
        } else {

            fseek(file, chunkSize, SEEK_CUR);
        }
    }

    if (!foundData) {

        fclose(file);
        return false;
    }

    fclose(file);
    return false;
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

}
}

