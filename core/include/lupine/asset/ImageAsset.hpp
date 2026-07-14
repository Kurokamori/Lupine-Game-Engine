#pragma once

#include "Asset.hpp"
#include <vector>
#include <cstdint>

namespace lupine {
namespace asset {

/**
 * Image format enumeration
 */
enum class ImageFormat {
    Unknown,
    R8,
    RG8,
    RGB8,
    RGBA8,
    R16F,
    RG16F,
    RGB16F,
    RGBA16F,
    R32F,
    RG32F,
    RGB32F,
    RGBA32F
};

/**
 * Image color space
 */
enum class ImageColorSpace {
    Linear,
    sRGB
};

/**
 * Mipmap level data
 */
struct MipLevel {
    uint32_t width;
    uint32_t height;
    std::vector<uint8_t> data;
};

/**
 * Image asset class
 * Supports PNG, JPG, BMP, TGA, HDR formats
 */
class ImageAsset : public Asset {
public:
    ImageAsset();
    explicit ImageAsset(const core::UUID& uuid);
    virtual ~ImageAsset();

    AssetType GetType() const override { return AssetType::Image; }

    // Load from file
    bool LoadFromFile(const std::string& filepath, bool generateMips = true, ImageColorSpace colorSpace = ImageColorSpace::sRGB);

    // Load from memory (compressed data like PNG, JPG)
    bool LoadFromMemory(const uint8_t* data, size_t dataSize, bool generateMips = true, ImageColorSpace colorSpace = ImageColorSpace::sRGB);

    // Load from raw RGBA data
    bool LoadFromRawData(const uint8_t* data, uint32_t width, uint32_t height, uint32_t channels, bool generateMips = true, ImageColorSpace colorSpace = ImageColorSpace::sRGB);

    // Image properties
    uint32_t GetWidth() const { return m_Width; }
    uint32_t GetHeight() const { return m_Height; }
    uint32_t GetChannels() const { return m_Channels; }
    ImageFormat GetFormat() const { return m_Format; }
    ImageColorSpace GetColorSpace() const { return m_ColorSpace; }
    
    // Mipmap access
    uint32_t GetMipLevels() const { return static_cast<uint32_t>(m_MipLevels.size()); }
    const MipLevel& GetMipLevel(uint32_t level) const;
    const std::vector<MipLevel>& GetAllMipLevels() const { return m_MipLevels; }

    // Generate the full mipmap chain in place if it has not been generated yet
    // (i.e. the image was loaded without mips). No-op when a chain already exists
    // or the base level is 1x1. Used by the renderer to give 2D content textures a
    // mip chain for smooth minification when the window is below the design size.
    void EnsureMipmaps();
    
    // Raw data access (base level)
    const uint8_t* GetData() const;
    size_t GetDataSize() const;
    
    // Has alpha channel
    bool HasAlpha() const { return m_Channels == 4; }
    
    // Compression support (for future use)
    bool IsCompressed() const { return m_Compressed; }
    
private:
    void GenerateMipmaps();
    void DilateAlphaBorders();
    void DeterminFormat();
    void RefreshTrackedBytes();   // sum all mip levels and report to the profiler
    
    uint32_t m_Width{0};
    uint32_t m_Height{0};
    uint32_t m_Channels{0};
    ImageFormat m_Format{ImageFormat::Unknown};
    ImageColorSpace m_ColorSpace{ImageColorSpace::sRGB};
    bool m_Compressed{false};
    
    std::vector<MipLevel> m_MipLevels;
};

} // namespace asset
} // namespace lupine

