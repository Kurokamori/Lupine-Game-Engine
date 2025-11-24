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
 * Glyph information for a single character
 */
struct Glyph {
    uint32_t codepoint;           // Unicode codepoint
    Vec2 uvMin;                   // UV coordinates in atlas (top-left)
    Vec2 uvMax;                   // UV coordinates in atlas (bottom-right)
    Vec2 size;                    // Size in pixels
    Vec2 bearing;                 // Offset from baseline to top-left of glyph
    float advance;                // Horizontal advance to next glyph
};

/**
 * Font atlas - contains pre-rendered glyphs in a texture
 */
struct FontAtlas {
    TextureHandle texture;        // Atlas texture
    uint32_t atlasWidth = 0;      // Atlas dimensions
    uint32_t atlasHeight = 0;
    float fontSize = 16.0f;       // Base font size
    float lineHeight = 0.0f;      // Line spacing

    // Glyph lookup table
    std::unordered_map<uint32_t, Glyph> glyphs;

    // Get glyph for a character (returns nullptr if not found)
    const Glyph* getGlyph(uint32_t codepoint) const;

    // Calculate text bounds
    Vec2 measureText(const std::string& text, float scale = 1.0f) const;
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
