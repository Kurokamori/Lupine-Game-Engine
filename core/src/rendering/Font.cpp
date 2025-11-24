#include "lupine/rendering/Font.hpp"

namespace lupine {

const Glyph* FontAtlas::getGlyph(uint32_t codepoint) const {
    auto it = glyphs.find(codepoint);
    if (it != glyphs.end()) {
        return &it->second;
    }
    return nullptr;
}

Vec2 FontAtlas::measureText(const std::string& text, float scale) const {
    float width = 0.0f;
    float height = lineHeight * scale;

    for (char c : text) {
        const Glyph* glyph = getGlyph(static_cast<uint32_t>(c));
        if (glyph) {
            width += glyph->advance * scale;
        }
    }

    return Vec2(width, height);
}

}
