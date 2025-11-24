#include "lupine/asset/ImageAsset.hpp"
#include "lupine/logger/Logger.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <algorithm>
#include <cmath>

namespace lupine {
namespace asset {

ImageAsset::ImageAsset()
    : Asset() {
}

ImageAsset::ImageAsset(const core::UUID& uuid)
    : Asset(uuid) {
}

ImageAsset::~ImageAsset() {
    m_MipLevels.clear();
}

bool ImageAsset::LoadFromFile(const std::string& filepath, bool generateMips, ImageColorSpace colorSpace) {

    SetPath(filepath);
    m_ColorSpace = colorSpace;

    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);

    // Force load as RGBA (4 channels) to ensure consistency with GPU texture formats
    uint8_t* data = stbi_load(filepath.c_str(), &width, &height, &channels, 4);

    if (!data) {

        return false;
    }

    m_Width = static_cast<uint32_t>(width);
    m_Height = static_cast<uint32_t>(height);
    // Always use 4 channels since we forced RGBA loading
    m_Channels = 4;

    DeterminFormat();

    MipLevel baseMip;
    baseMip.width = m_Width;
    baseMip.height = m_Height;
    baseMip.data.resize(m_Width * m_Height * m_Channels);
    std::memcpy(baseMip.data.data(), data, baseMip.data.size());

    m_MipLevels.push_back(std::move(baseMip));

    stbi_image_free(data);

    if (generateMips && m_Width > 1 && m_Height > 1) {
        GenerateMipmaps();
    }

    SetLoaded(true);

    return true;
}

bool ImageAsset::LoadFromMemory(const uint8_t* data, size_t dataSize, bool generateMips, ImageColorSpace colorSpace) {

    m_ColorSpace = colorSpace;

    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);

    // Force load as RGBA (4 channels) to ensure consistency with GPU texture formats
    uint8_t* imageData = stbi_load_from_memory(data, static_cast<int>(dataSize), &width, &height, &channels, 4);

    if (!imageData) {

        return false;
    }

    m_Width = static_cast<uint32_t>(width);
    m_Height = static_cast<uint32_t>(height);
    // Always use 4 channels since we forced RGBA loading
    m_Channels = 4;

    DeterminFormat();

    MipLevel baseMip;
    baseMip.width = m_Width;
    baseMip.height = m_Height;
    baseMip.data.resize(m_Width * m_Height * m_Channels);
    std::memcpy(baseMip.data.data(), imageData, baseMip.data.size());

    m_MipLevels.push_back(std::move(baseMip));

    stbi_image_free(imageData);

    if (generateMips && m_Width > 1 && m_Height > 1) {
        GenerateMipmaps();
    }

    SetLoaded(true);

    return true;
}

bool ImageAsset::LoadFromRawData(const uint8_t* data, uint32_t width, uint32_t height, uint32_t channels, bool generateMips, ImageColorSpace colorSpace) {

    m_Width = width;
    m_Height = height;
    m_Channels = channels;
    m_ColorSpace = colorSpace;

    DeterminFormat();

    MipLevel baseMip;
    baseMip.width = m_Width;
    baseMip.height = m_Height;
    baseMip.data.resize(m_Width * m_Height * m_Channels);
    std::memcpy(baseMip.data.data(), data, baseMip.data.size());

    m_MipLevels.push_back(std::move(baseMip));

    if (generateMips && m_Width > 1 && m_Height > 1) {
        GenerateMipmaps();
    }

    SetLoaded(true);

    return true;
}

const MipLevel& ImageAsset::GetMipLevel(uint32_t level) const {
    if (level >= m_MipLevels.size()) {

        return m_MipLevels[0];
    }
    return m_MipLevels[level];
}

const uint8_t* ImageAsset::GetData() const {
    if (m_MipLevels.empty()) {
        return nullptr;
    }
    return m_MipLevels[0].data.data();
}

size_t ImageAsset::GetDataSize() const {
    if (m_MipLevels.empty()) {
        return 0;
    }
    return m_MipLevels[0].data.size();
}

void ImageAsset::GenerateMipmaps() {
    uint32_t mipWidth = m_Width;
    uint32_t mipHeight = m_Height;

    while (mipWidth > 1 || mipHeight > 1) {
        uint32_t nextWidth = std::max(1u, mipWidth / 2);
        uint32_t nextHeight = std::max(1u, mipHeight / 2);

        MipLevel mip;
        mip.width = nextWidth;
        mip.height = nextHeight;
        mip.data.resize(nextWidth * nextHeight * m_Channels);

        const MipLevel& prevMip = m_MipLevels.back();

        for (uint32_t y = 0; y < nextHeight; ++y) {
            for (uint32_t x = 0; x < nextWidth; ++x) {
                uint32_t srcX = x * 2;
                uint32_t srcY = y * 2;

                for (uint32_t c = 0; c < m_Channels; ++c) {
                    uint32_t sum = 0;
                    uint32_t count = 0;

                    for (uint32_t dy = 0; dy < 2 && (srcY + dy) < mipHeight; ++dy) {
                        for (uint32_t dx = 0; dx < 2 && (srcX + dx) < mipWidth; ++dx) {
                            uint32_t srcIdx = ((srcY + dy) * mipWidth + (srcX + dx)) * m_Channels + c;
                            sum += prevMip.data[srcIdx];
                            count++;
                        }
                    }

                    uint32_t dstIdx = (y * nextWidth + x) * m_Channels + c;
                    mip.data[dstIdx] = static_cast<uint8_t>(sum / count);
                }
            }
        }

        m_MipLevels.push_back(std::move(mip));

        mipWidth = nextWidth;
        mipHeight = nextHeight;
    }
}

void ImageAsset::DeterminFormat() {
    switch (m_Channels) {
        case 1: m_Format = ImageFormat::R8; break;
        case 2: m_Format = ImageFormat::RG8; break;
        case 3: m_Format = ImageFormat::RGB8; break;
        case 4: m_Format = ImageFormat::RGBA8; break;
        default: m_Format = ImageFormat::Unknown; break;
    }
}

}
}

