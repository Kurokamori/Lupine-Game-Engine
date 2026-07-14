#include "lupine/rendering/Font.hpp"
#include <algorithm>

namespace lupine {

const Glyph* FontAtlas::getGlyph(uint32_t codepoint) const {
    auto it = glyphs.find(codepoint);
    if (it != glyphs.end()) {
        return &it->second;
    }
    return nullptr;
}

Vec2 FontAtlas::measureText(const std::string& text, float scale) const {
    const float lineH = getLineHeight() * scale;

    float width = 0.0f;
    float lineWidth = 0.0f;
    float height = lineH;

    for (char c : text) {
        if (c == '\n') {
            // Each line is measured on its own; the widest one wins. Accumulating
            // every line into a single running total (as this used to) reports a
            // wildly oversized width for multi-line strings.
            width = std::max(width, lineWidth);
            lineWidth = 0.0f;
            height += lineH;
            continue;
        }

        const Glyph* glyph = getGlyph(static_cast<uint32_t>(c));
        if (glyph) {
            lineWidth += glyph->advance * scale;
        } else if (c == ' ') {
            // Whitespace usually has no rasterized glyph; without this a measured
            // string is short by one space-width per space, so centered text drifts.
            lineWidth += fontSize * 0.25f * scale;
        } else if (c == '\t') {
            lineWidth += fontSize * 0.25f * 4.0f * scale;
        }
    }

    width = std::max(width, lineWidth);
    return Vec2(width, height);
}

}
