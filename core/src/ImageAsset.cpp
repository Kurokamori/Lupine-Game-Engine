#include "lupine/asset/ImageAsset.hpp"
#include "lupine/platform/PackFile.hpp"
#include "lupine/logger/Logger.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace lupine {
namespace asset {

namespace {

// Upper bound on a single decoded mip level. Anything larger is treated as a corrupt or
// hostile dimension pair rather than a legitimate texture; 2 GiB also keeps every byte
// offset inside the level addressable by a 32-bit index.
constexpr uint64_t kMaxImageLevelBytes = 1ull << 31;

// Computes width * height * channels in 64-bit and rejects any combination that would
// overflow (the naive 32-bit product silently wraps and yields an undersized buffer).
bool ComputeLevelSize(uint32_t width, uint32_t height, uint32_t channels, size_t& outSize) {
    outSize = 0;

    if (width == 0 || height == 0 || channels == 0) {
        return false;
    }

    const uint64_t total = static_cast<uint64_t>(width) *
                           static_cast<uint64_t>(height) *
                           static_cast<uint64_t>(channels);

    if (total > kMaxImageLevelBytes ||
        total > static_cast<uint64_t>(std::numeric_limits<size_t>::max())) {
        return false;
    }

    outSize = static_cast<size_t>(total);
    return true;
}

} // namespace

ImageAsset::ImageAsset()
    : Asset() {
}

ImageAsset::ImageAsset(const core::UUID& uuid)
    : Asset(uuid) {
}

ImageAsset::~ImageAsset() {
    m_MipLevels.clear();
}

void ImageAsset::RefreshTrackedBytes() {
    size_t total = 0;
    for (const MipLevel& mip : m_MipLevels) {
        total += mip.data.size();
    }
    SetTrackedBytes(total);
}

bool ImageAsset::LoadFromFile(const std::string& filepath, bool generateMips, ImageColorSpace colorSpace) {
    // Normalize the filepath - fix malformed res:/ paths and doubled res:// prefixes
    std::string normalizedPath = filepath;

    // Replace backslashes with forward slashes
    std::replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');

    // Fix doubled res:// paths (e.g., "res://assets/glTF/res:/assets/glTF/Grass.png")
    // Find the LAST occurrence of "res:/" and use that as the canonical path
    size_t lastResPos = normalizedPath.rfind("res:/");
    if (lastResPos != std::string::npos && lastResPos > 0) {
        // There's a res:/ that's not at the start - use only that part
        size_t startPos = lastResPos + 5;
        // Skip additional slashes
        while (startPos < normalizedPath.size() && normalizedPath[startPos] == '/') {
            startPos++;
        }
        if (startPos < normalizedPath.size()) {
            normalizedPath = "res://" + normalizedPath.substr(startPos);
        }
    } else if (lastResPos == 0) {
        // Single res:/ at start - normalize to res://
        size_t startPos = 5;
        while (startPos < normalizedPath.size() && normalizedPath[startPos] == '/') {
            startPos++;
        }
        if (startPos < normalizedPath.size()) {
            normalizedPath = "res://" + normalizedPath.substr(startPos);
        }
    }

    // Store the path (will be converted to res:// format if possible)
    SetPath(normalizedPath);
    m_ColorSpace = colorSpace;

    int width = 0, height = 0, channels = 0;
    stbi_set_flip_vertically_on_load(true);

    uint8_t* data = nullptr;

    // Resolve the path using the asset system (handles UUID fallback)
    std::string resolvedPath = ResolveAssetPath(normalizedPath);

    // Check if running from pack file first
    auto& packFS = platform::PackFileSystem::Instance();
    bool packMode = packFS.isPackMode();

    // For pack mode, use the resolved asset path
    std::string packPath = packMode ? packFS.resolveAsset(normalizedPath) : normalizedPath;
    bool existsInPack = packMode && packFS.exists(packPath);

    if (packMode && existsInPack) {
        // Load from pack file using normalized path
        std::vector<uint8_t> fileData = packFS.readAsset(normalizedPath);
        
        if (!fileData.empty()) {
            // Debug: log first few bytes to verify file signature
            if (fileData.size() >= 8) {
                
            }
            data = stbi_load_from_memory(fileData.data(), static_cast<int>(fileData.size()),
                                         &width, &height, &channels, 4);
        }
    } else {
        // Fall back to filesystem using resolved physical path
        
        data = stbi_load(resolvedPath.c_str(), &width, &height, &channels, 4);
    }

    if (!data) {
        const char* failureReason = stbi_failure_reason();
        LOG_ERROR(LogCategory::Core, "ImageAsset: stbi_load FAILED for {} (normalized: {}, resolved: {}). Reason: {}",
                  filepath, normalizedPath, resolvedPath, failureReason ? failureReason : "unknown");
        return false;
    }

    m_Width = static_cast<uint32_t>(width);
    m_Height = static_cast<uint32_t>(height);
    // Always use 4 channels since we forced RGBA loading
    m_Channels = 4;

    DeterminFormat();

    size_t baseSize = 0;
    if (!ComputeLevelSize(m_Width, m_Height, m_Channels, baseSize)) {
        LOG_ERROR(LogCategory::Core, "ImageAsset: rejected image {} with unusable dimensions {}x{}x{}",
                  filepath, m_Width, m_Height, m_Channels);
        stbi_image_free(data);
        return false;
    }

    MipLevel baseMip;
    baseMip.width = m_Width;
    baseMip.height = m_Height;
    baseMip.data.resize(baseSize);
    std::memcpy(baseMip.data.data(), data, baseSize);

    m_MipLevels.push_back(std::move(baseMip));

    stbi_image_free(data);

    if (generateMips && m_Width > 1 && m_Height > 1) {
        GenerateMipmaps();
    }

    SetLoaded(true);
    RefreshTrackedBytes();

    return true;
}

bool ImageAsset::LoadFromMemory(const uint8_t* data, size_t dataSize, bool generateMips, ImageColorSpace colorSpace) {

    m_ColorSpace = colorSpace;

    int width = 0, height = 0, channels = 0;
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

    size_t baseSize = 0;
    if (!ComputeLevelSize(m_Width, m_Height, m_Channels, baseSize)) {
        LOG_ERROR(LogCategory::Core, "ImageAsset: rejected in-memory image with unusable dimensions {}x{}x{}",
                  m_Width, m_Height, m_Channels);
        stbi_image_free(imageData);
        return false;
    }

    MipLevel baseMip;
    baseMip.width = m_Width;
    baseMip.height = m_Height;
    baseMip.data.resize(baseSize);
    std::memcpy(baseMip.data.data(), imageData, baseSize);

    m_MipLevels.push_back(std::move(baseMip));

    stbi_image_free(imageData);

    if (generateMips && m_Width > 1 && m_Height > 1) {
        GenerateMipmaps();
    }

    SetLoaded(true);
    RefreshTrackedBytes();

    return true;
}

bool ImageAsset::LoadFromRawData(const uint8_t* data, uint32_t width, uint32_t height, uint32_t channels, bool generateMips, ImageColorSpace colorSpace) {
    if (!data) {
        return false;
    }

    size_t baseSize = 0;
    if (!ComputeLevelSize(width, height, channels, baseSize)) {
        LOG_ERROR(LogCategory::Core, "ImageAsset: rejected raw image with unusable dimensions {}x{}x{}",
                  width, height, channels);
        return false;
    }

    m_Width = width;
    m_Height = height;
    m_Channels = channels;
    m_ColorSpace = colorSpace;

    DeterminFormat();

    MipLevel baseMip;
    baseMip.width = m_Width;
    baseMip.height = m_Height;
    baseMip.data.resize(baseSize);
    std::memcpy(baseMip.data.data(), data, baseSize);

    m_MipLevels.push_back(std::move(baseMip));

    if (generateMips && m_Width > 1 && m_Height > 1) {
        GenerateMipmaps();
    }

    SetLoaded(true);
    RefreshTrackedBytes();

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

void ImageAsset::EnsureMipmaps() {
    if (m_MipLevels.size() > 1) {
        return;
    }
    if (m_MipLevels.empty() || (m_Width <= 1 && m_Height <= 1)) {
        return;
    }
    GenerateMipmaps();
    RefreshTrackedBytes();
}

void ImageAsset::DilateAlphaBorders() {
    // Bleed the colour of opaque texels outward into adjacent fully-transparent texels
    // ("fix alpha border" / edge padding). Transparent pixels in source art usually
    // store an arbitrary (often white) RGB; when the texture is bilinear-filtered or
    // downsampled into mips that colour bleeds in and shows up as a bright fringe around
    // sprite edges. Replacing transparent RGB with the nearest opaque colour removes the
    // fringe. Alpha is never modified, so the visible image is unchanged; this only fixes
    // the colour that filtering pulls in at the edges. Applied to the base level before
    // mip generation so both CPU-built mips and GPU-generated (glGenerateMipmap) mips are
    // clean.
    if (m_Channels != 4 || m_MipLevels.empty()) {
        return;
    }

    MipLevel& base = m_MipLevels[0];
    const uint32_t w = base.width;
    const uint32_t h = base.height;
    if (w == 0 || h == 0) {
        return;
    }

    std::vector<uint8_t> filled(static_cast<size_t>(w) * h);
    for (size_t i = 0; i < filled.size(); ++i) {
        filled[i] = (base.data[i * 4 + 3] != 0) ? 1 : 0;
    }

    // A handful of dilation passes covers the edge band the mip chain actually samples;
    // deeper transparent regions collapse to nothing in the small mips anyway.
    const int kMaxPasses = 16;
    for (int pass = 0; pass < kMaxPasses; ++pass) {
        std::vector<uint8_t> nextFilled = filled;
        bool changed = false;

        for (uint32_t y = 0; y < h; ++y) {
            for (uint32_t x = 0; x < w; ++x) {
                const size_t texel = static_cast<size_t>(y) * w + x;
                if (filled[texel]) {
                    continue;
                }

                uint32_t r = 0, g = 0, b = 0, n = 0;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dy == 0) {
                            continue;
                        }
                        const int nx = static_cast<int>(x) + dx;
                        const int ny = static_cast<int>(y) + dy;
                        if (nx < 0 || ny < 0 || nx >= static_cast<int>(w) || ny >= static_cast<int>(h)) {
                            continue;
                        }
                        const size_t nIdx = static_cast<size_t>(ny) * w + static_cast<size_t>(nx);
                        if (filled[nIdx]) {
                            r += base.data[nIdx * 4 + 0];
                            g += base.data[nIdx * 4 + 1];
                            b += base.data[nIdx * 4 + 2];
                            ++n;
                        }
                    }
                }

                if (n > 0) {
                    base.data[texel * 4 + 0] = static_cast<uint8_t>(r / n);
                    base.data[texel * 4 + 1] = static_cast<uint8_t>(g / n);
                    base.data[texel * 4 + 2] = static_cast<uint8_t>(b / n);
                    nextFilled[texel] = 1;
                    changed = true;
                }
            }
        }

        filled.swap(nextFilled);
        if (!changed) {
            break;
        }
    }
}

void ImageAsset::GenerateMipmaps() {
    // Pad transparent borders with edge colour first so neither the CPU box filter below
    // nor a GPU mip generator bleeds a bright fringe into minified sprite edges.
    DilateAlphaBorders();

    uint32_t mipWidth = m_Width;
    uint32_t mipHeight = m_Height;

    while (mipWidth > 1 || mipHeight > 1) {
        uint32_t nextWidth = std::max(1u, mipWidth / 2);
        uint32_t nextHeight = std::max(1u, mipHeight / 2);

        size_t mipSize = 0;
        if (!ComputeLevelSize(nextWidth, nextHeight, m_Channels, mipSize)) {
            break;
        }

        MipLevel mip;
        mip.width = nextWidth;
        mip.height = nextHeight;
        mip.data.resize(mipSize);

        const MipLevel& prevMip = m_MipLevels.back();

        // For RGBA images, downsample the colour channels with an ALPHA-WEIGHTED
        // average instead of a plain box filter. Fully/partly transparent texels in
        // source art usually still store an arbitrary (often white) RGB; a plain
        // average bleeds that colour into the smaller mip levels, producing a bright
        // halo around sprite edges as they minify. Weighting RGB by alpha makes
        // transparent texels contribute no colour, so edges keep the colour of their
        // opaque neighbours. Alpha itself is averaged normally. If a 2x2 block is
        // fully transparent (zero total alpha) we fall back to a plain RGB average so
        // the (invisible) colour is still defined and deterministic.
        const bool alphaWeighted = (m_Channels == 4);

        for (uint32_t y = 0; y < nextHeight; ++y) {
            for (uint32_t x = 0; x < nextWidth; ++x) {
                uint32_t srcX = x * 2;
                uint32_t srcY = y * 2;

                if (alphaWeighted) {
                    uint32_t rgbWeighted[3] = {0, 0, 0};
                    uint32_t rgbPlain[3] = {0, 0, 0};
                    uint32_t alphaSum = 0;
                    uint32_t alphaWeight = 0;
                    uint32_t count = 0;

                    for (uint32_t dy = 0; dy < 2 && (srcY + dy) < mipHeight; ++dy) {
                        for (uint32_t dx = 0; dx < 2 && (srcX + dx) < mipWidth; ++dx) {
                            size_t base = (static_cast<size_t>(srcY + dy) * mipWidth + (srcX + dx)) * 4;
                            uint32_t a = prevMip.data[base + 3];
                            for (uint32_t c = 0; c < 3; ++c) {
                                uint32_t v = prevMip.data[base + c];
                                rgbWeighted[c] += v * a;
                                rgbPlain[c] += v;
                            }
                            alphaSum += a;
                            alphaWeight += a;
                            ++count;
                        }
                    }

                    size_t dstBase = (static_cast<size_t>(y) * nextWidth + x) * 4;
                    for (uint32_t c = 0; c < 3; ++c) {
                        uint32_t value = (alphaWeight > 0)
                            ? (rgbWeighted[c] / alphaWeight)
                            : (rgbPlain[c] / count);
                        mip.data[dstBase + c] = static_cast<uint8_t>(value);
                    }
                    mip.data[dstBase + 3] = static_cast<uint8_t>(alphaSum / count);
                } else {
                    for (uint32_t c = 0; c < m_Channels; ++c) {
                        uint32_t sum = 0;
                        uint32_t count = 0;

                        for (uint32_t dy = 0; dy < 2 && (srcY + dy) < mipHeight; ++dy) {
                            for (uint32_t dx = 0; dx < 2 && (srcX + dx) < mipWidth; ++dx) {
                                size_t srcIdx = (static_cast<size_t>(srcY + dy) * mipWidth + (srcX + dx)) * m_Channels + c;
                                sum += prevMip.data[srcIdx];
                                count++;
                            }
                        }

                        size_t dstIdx = (static_cast<size_t>(y) * nextWidth + x) * m_Channels + c;
                        mip.data[dstIdx] = static_cast<uint8_t>(sum / count);
                    }
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

