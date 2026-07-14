#pragma once

#include "ResourceHandles.hpp"
#include "lupine/math/Vec2.hpp"
#include <unordered_map>
#include <string>
#include <vector>

namespace lupine {

// Import math types into lupine namespace for rendering
using math::Vec2;

/**
 * Glyph information for a single character.
 *
 * IMPORTANT - bearing.y sign convention: this is stb_truetype's `bakedchar.yoff`,
 * i.e. the offset from the baseline to the glyph's top edge measured DOWNWARD.
 * It is therefore NEGATIVE for the usual glyph that sits above the baseline.
 * On the engine's Y-up canvas the glyph's visual top is `baselineY - bearing.y`
 * (a subtraction, because the value is negative) and the quad hangs down from
 * there by `size.y`.
 *
 * Note this is the opposite sign to asset::Glyph::bearingY, which stores the
 * classic FreeType-style positive "baseline to top, upward" bearing. Do not mix
 * the two types' metrics.
 */
struct Glyph {
    uint32_t codepoint;           // Unicode codepoint
    Vec2 uvMin;                   // UV coordinates in atlas (top-left)
    Vec2 uvMax;                   // UV coordinates in atlas (bottom-right)
    Vec2 size;                    // Size in pixels
    Vec2 bearing;                 // x: left side bearing; y: stb yoff (see above, negative)
    float advance;                // Horizontal advance to next glyph
};

/**
 * Font atlas - contains pre-rendered glyphs in a texture.
 *
 * ascent/descent/lineHeight are whole-font vertical metrics (from the font's
 * head/hhea tables, not from the glyphs that happen to be in any one string), in
 * the same pixel units as `fontSize`. Both ascent and descent are POSITIVE
 * distances from the baseline (up and down respectively). Text layout must use
 * these rather than deriving metrics from individual glyph bearings, otherwise a
 * string's vertical placement changes with the characters it contains.
 */
struct FontAtlas {
    TextureHandle texture;        // Atlas texture
    uint32_t atlasWidth = 0;      // Atlas dimensions
    uint32_t atlasHeight = 0;
    float fontSize = 16.0f;       // Base font size (baked pixel height)
    float lineHeight = 0.0f;      // Baseline-to-baseline distance
    float ascent = 0.0f;          // Baseline to top of the em box (positive)
    float descent = 0.0f;         // Baseline to bottom of the em box (positive)

    // Glyph lookup table
    std::unordered_map<uint32_t, Glyph> glyphs;

    // Kerning pairs, keyed (left << 32) | right, in the same (oversampled) pixel units as
    // Glyph::advance. Only non-zero pairs are stored, so most fonts contribute a small table
    // and a font with no kern table contributes none. Text layout was advance-only before
    // this: "AV" and "To" were spaced as though the glyphs were rectangles.
    std::unordered_map<uint64_t, float> kerning;

    static uint64_t kernKey(uint32_t left, uint32_t right) {
        return (static_cast<uint64_t>(left) << 32) | static_cast<uint64_t>(right);
    }

    /** Kerning adjustment between two adjacent codepoints; 0 when the pair is not kerned. */
    float getKerning(uint32_t left, uint32_t right) const {
        auto it = kerning.find(kernKey(left, right));
        return (it != kerning.end()) ? it->second : 0.0f;
    }

    // Get glyph for a character (returns nullptr if not found)
    const Glyph* getGlyph(uint32_t codepoint) const;

    // Calculate text bounds
    Vec2 measureText(const std::string& text, float scale = 1.0f) const;

    // Vertical metrics with fallbacks, so a hand-built atlas that only sets
    // fontSize still lays out sanely. Always positive.
    float getAscent() const {
        return (ascent > 0.0f) ? ascent : (fontSize * 0.8f);
    }
    float getDescent() const {
        return (descent > 0.0f) ? descent : (fontSize * 0.2f);
    }
    float getLineHeight() const {
        return (lineHeight > 0.0f) ? lineHeight : (getAscent() + getDescent());
    }
};

/**
 * Font loading descriptor
 */
struct FontDesc {
    std::string fontPath;         // Path to font file (TTF, OTF)
    float fontSize = 16.0f;       // Size to render glyphs
    uint32_t atlasWidth = 512;    // Atlas texture dimensions
    uint32_t atlasHeight = 512;

    // Character ranges to include (e.g., ASCII, extended ASCII, Unicode ranges)
    struct CharRange {
        uint32_t first;
        uint32_t last;
    };
    std::vector<CharRange> charRanges = {{32, 126}}; // Default: ASCII printable
};

} // namespace lupine
