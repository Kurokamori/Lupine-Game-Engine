#pragma once

#include "lupine/rendering/Font.hpp"
#include <cstdint>
#include <vector>
#include <unordered_map>

namespace lupine {

/**
 * Global font oversampling factor: the number of device (framebuffer) pixels that
 * cover one logical (design-canvas) pixel for on-screen UI text.
 *
 * UI and text are laid out in a fixed logical canvas (the project's design
 * resolution) and then mapped to the actual window through the viewport. When the
 * window is smaller than the design resolution a logical pixel covers fewer than
 * one device pixel, so a glyph atlas baked at the logical font size is minified at
 * draw time and looks fuzzy. Baking the atlas at fontSize * oversample instead
 * gives each on-screen pixel roughly one texel, restoring crisp text at any
 * window resolution.
 *
 * The factor is transparent to layout: backends record the oversampled pixel
 * height in FontAtlas::fontSize, and the existing layout scale
 * (params.fontSize / atlas.fontSize) cancels it out, so glyphs keep their logical
 * on-screen size while sampling a higher- (or lower-) density texture.
 *
 * Default is 1.0 (legacy behavior). The runtime updates it each frame from the
 * active viewport / design-resolution ratio.
 */
void SetFontOversample(float factor);

/** Current oversampling factor (>= kMinFontOversample, <= kMaxFontOversample). */
float GetFontOversample();

/**
 * Monotonic counter bumped whenever SetFontOversample changes the factor beyond a
 * small epsilon. Text components compare it against a cached value to know when a
 * resolution change has re-baked the font atlases and their cached glyph meshes
 * must be regenerated.
 */
uint64_t GetFontOversampleGeneration();

/** Clamps + quantizes a raw device/design scale into a stable oversampling factor. */
float QuantizeFontOversample(float rawScale);

constexpr float kMinFontOversample = 0.25f;
constexpr float kMaxFontOversample = 4.0f;

/**
 * CPU-side baked atlas: a single-channel (R8) coverage bitmap plus glyph metrics.
 * It owns no GPU resource, so it is backend agnostic; each GfxDevice uploads the
 * bitmap into its own texture format and assembles a FontAtlas from the rest.
 */
struct BakedFontAtlas {
    bool success = false;
    std::vector<unsigned char> bitmap;          // atlasWidth * atlasHeight, R8 coverage
    uint32_t atlasWidth = 0;
    uint32_t atlasHeight = 0;
    float fontSize = 0.0f;                       // actual baked pixel height (logical * oversample)
    float lineHeight = 0.0f;                     // baseline-to-baseline distance
    float ascent = 0.0f;                         // baseline to em-box top (positive)
    float descent = 0.0f;                        // baseline to em-box bottom (positive)
    std::unordered_map<uint32_t, Glyph> glyphs;

    // Non-zero kerning pairs over the baked range, keyed (left << 32) | right, in the same
    // oversampled pixel units as the glyph metrics. Backends copy this straight into
    // FontAtlas::kerning alongside the glyphs.
    std::unordered_map<uint64_t, float> kerning;
};

/**
 * Rasterizes the glyph ranges in @p desc at desc.fontSize * @p oversample, growing
 * the atlas dimensions to fit the larger glyphs (and clamping to a sane maximum).
 * The returned fontSize is the oversampled pixel height; glyph metrics (size,
 * bearing, advance, line height) are likewise in oversampled pixels so the layout
 * scale renders them at their logical size.
 *
 * Backends share this so the stb_truetype baking, atlas sizing, oversampling and
 * UV/metric computation stay identical across every graphics API.
 */
BakedFontAtlas BakeFontAtlas(const FontDesc& desc, float oversample);

} // namespace lupine
