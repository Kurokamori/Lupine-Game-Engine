#include "lupine/rendering/FontBaker.hpp"

#include "lupine/platform/FileSystem.hpp"
#include "lupine/platform/PackFile.hpp"
#include "lupine/asset/Asset.hpp"

#include <stb_truetype.h>

#include <algorithm>
#include <atomic>
#include <cmath>

namespace lupine {

namespace {

std::atomic<float> g_oversample{1.0f};
std::atomic<uint64_t> g_generation{0};

constexpr uint32_t kMinAtlasDim = 128;
constexpr uint32_t kMaxAtlasDim = 4096;
constexpr float kOversampleStep = 0.25f;
constexpr float kOversampleEpsilon = 0.01f;

uint32_t RoundUpPow2(uint32_t v) {
    if (v <= 1) {
        return 1;
    }
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    return v + 1;
}

uint32_t ScaleAtlasDim(uint32_t base, float oversample) {
    float scaled = static_cast<float>(base) * oversample;
    uint32_t dim = RoundUpPow2(static_cast<uint32_t>(std::ceil(scaled)));
    dim = std::clamp(dim, kMinAtlasDim, kMaxAtlasDim);
    return dim;
}

std::vector<unsigned char> LoadFontFile(const std::string& fontPath) {
    auto& packFS = platform::PackFileSystem::Instance();
    if (packFS.isPackMode() && packFS.exists(fontPath)) {
        return packFS.readFile(fontPath);
    }
    std::string resolvedPath = asset::Asset::ResolveAssetPath(fontPath);
    auto result = platform::FileSystem::ReadBinaryFile(resolvedPath);
    if (result.success) {
        return std::move(result.data);
    }
    return {};
}

} // namespace

void SetFontOversample(float factor) {
    factor = std::clamp(factor, kMinFontOversample, kMaxFontOversample);
    float previous = g_oversample.load(std::memory_order_relaxed);
    if (std::abs(factor - previous) > kOversampleEpsilon) {
        g_oversample.store(factor, std::memory_order_relaxed);
        g_generation.fetch_add(1, std::memory_order_relaxed);
    }
}

float GetFontOversample() {
    return g_oversample.load(std::memory_order_relaxed);
}

uint64_t GetFontOversampleGeneration() {
    return g_generation.load(std::memory_order_relaxed);
}

float QuantizeFontOversample(float rawScale) {
    if (!(rawScale > 0.0f)) {
        return 1.0f;
    }
    // Round up to the next quantum so glyphs are never baked smaller than the
    // on-screen footprint (which would reintroduce minification blur), while the
    // quantization keeps tiny window-size jitter from constantly re-baking.
    float quantized = std::ceil(rawScale / kOversampleStep) * kOversampleStep;
    return std::clamp(quantized, kMinFontOversample, kMaxFontOversample);
}

BakedFontAtlas BakeFontAtlas(const FontDesc& desc, float oversample) {
    BakedFontAtlas baked;

    if (desc.charRanges.empty()) {
        return baked;
    }

    oversample = std::clamp(oversample, kMinFontOversample, kMaxFontOversample);
    const float pixelHeight = std::max(1.0f, desc.fontSize * oversample);

    std::vector<unsigned char> fontBuffer = LoadFontFile(desc.fontPath);
    if (fontBuffer.empty()) {
        return baked;
    }

    stbtt_fontinfo fontInfo;
    if (!stbtt_InitFont(&fontInfo, fontBuffer.data(), 0)) {
        return baked;
    }

    const auto& charRange = desc.charRanges[0];
    const uint32_t numChars = charRange.last - charRange.first + 1;
    std::vector<stbtt_bakedchar> bakedChars(numChars);

    uint32_t atlasWidth = ScaleAtlasDim(desc.atlasWidth, oversample);
    uint32_t atlasHeight = ScaleAtlasDim(desc.atlasHeight, oversample);

    std::vector<unsigned char> atlasBitmap;
    int bakeResult = 0;

    // stbtt_BakeFontBitmap returns a non-positive value when the requested glyphs
    // do not fit. Grow the atlas (square, power-of-two) and retry so larger
    // oversampling factors or extended ranges still succeed instead of silently
    // dropping the font.
    while (true) {
        atlasBitmap.assign(static_cast<size_t>(atlasWidth) * atlasHeight, 0);
        bakeResult = stbtt_BakeFontBitmap(
            fontBuffer.data(), 0,
            pixelHeight,
            atlasBitmap.data(), static_cast<int>(atlasWidth), static_cast<int>(atlasHeight),
            charRange.first, static_cast<int>(numChars),
            bakedChars.data());

        if (bakeResult > 0) {
            break;
        }

        const uint32_t nextWidth = std::min(atlasWidth * 2u, kMaxAtlasDim);
        const uint32_t nextHeight = std::min(atlasHeight * 2u, kMaxAtlasDim);
        if (nextWidth == atlasWidth && nextHeight == atlasHeight) {
            return baked;
        }
        atlasWidth = nextWidth;
        atlasHeight = nextHeight;
    }

    baked.bitmap = std::move(atlasBitmap);
    baked.atlasWidth = atlasWidth;
    baked.atlasHeight = atlasHeight;
    baked.fontSize = pixelHeight;

    int ascent = 0;
    int descent = 0;
    int lineGap = 0;
    const float metricScale = stbtt_ScaleForPixelHeight(&fontInfo, pixelHeight);
    stbtt_GetFontVMetrics(&fontInfo, &ascent, &descent, &lineGap);
    baked.lineHeight = static_cast<float>(ascent - descent + lineGap) * metricScale;
    // stb reports descent as a negative offset from the baseline; the atlas stores
    // both metrics as positive distances (see FontAtlas).
    baked.ascent = static_cast<float>(ascent) * metricScale;
    baked.descent = static_cast<float>(-descent) * metricScale;

    baked.glyphs.reserve(numChars);
    for (uint32_t i = 0; i < numChars; ++i) {
        const stbtt_bakedchar& bc = bakedChars[i];

        Glyph glyph;
        glyph.codepoint = charRange.first + i;
        glyph.uvMin = Vec2(bc.x0 / static_cast<float>(atlasWidth),
                           bc.y0 / static_cast<float>(atlasHeight));
        glyph.uvMax = Vec2(bc.x1 / static_cast<float>(atlasWidth),
                           bc.y1 / static_cast<float>(atlasHeight));
        glyph.size = Vec2(static_cast<float>(bc.x1 - bc.x0),
                          static_cast<float>(bc.y1 - bc.y0));
        glyph.bearing = Vec2(bc.xoff, bc.yoff);
        glyph.advance = bc.xadvance;

        baked.glyphs[glyph.codepoint] = glyph;
    }

    // Kerning pairs over the baked range. Layout was advance-only, so a font's kern table was
    // ignored entirely and pairs like "AV", "To" and "Ye" were spaced as if the glyphs were
    // plain rectangles.
    //
    // Only non-zero pairs are stored, which for a typical text font is a small fraction of
    // the range. The pair scan is O(n^2) in the range size, so it is skipped for very large
    // ranges (a CJK block would be millions of probes for a table almost entirely of zeroes,
    // and those scripts are not kerned this way anyway).
    constexpr uint32_t kMaxKerningRange = 512;
    if (numChars <= kMaxKerningRange) {
        for (uint32_t li = 0; li < numChars; ++li) {
            const uint32_t left = charRange.first + li;
            for (uint32_t ri = 0; ri < numChars; ++ri) {
                const uint32_t right = charRange.first + ri;
                const int raw = stbtt_GetCodepointKernAdvance(
                    &fontInfo, static_cast<int>(left), static_cast<int>(right));
                if (raw == 0) {
                    continue;
                }
                // Same (oversampled) pixel units as Glyph::advance, so TextLayout's
                // fontSize/atlas.fontSize scale applies to both alike.
                baked.kerning[FontAtlas::kernKey(left, right)] =
                    static_cast<float>(raw) * metricScale;
            }
        }
    }

    baked.success = true;
    return baked;
}

} // namespace lupine
