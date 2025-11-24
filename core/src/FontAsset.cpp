#include <algorithm>
#include <fstream>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include "lupine/asset/FontAsset.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/platform/Platform.hpp"

namespace lupine {
namespace asset {

FontAsset::FontAsset()
    : Asset() {
}

FontAsset::FontAsset(const core::UUID& uuid)
    : Asset(uuid) {
}

FontAsset::~FontAsset() {
    m_AtlasData.clear();
    m_Glyphs.clear();
    m_KerningTable.clear();
}

bool FontAsset::LoadFromFile(const std::string& filepath, float fontSize, uint32_t atlasWidth, uint32_t atlasHeight) {

    SetPath(filepath);
    m_FontSize = fontSize;
    m_AtlasWidth = atlasWidth;
    m_AtlasHeight = atlasHeight;

    auto result = platform::FileSystem::ReadBinaryFile(filepath);
    if (!result.success) {

        return false;
    }

    std::vector<uint8_t> fontData = result.data;

    GenerateAtlas(fontData, fontSize, atlasWidth, atlasHeight);

    SetLoaded(true);

    return true;
}

const Glyph* FontAsset::GetGlyph(uint32_t codepoint) const {
    auto it = m_Glyphs.find(codepoint);
    if (it != m_Glyphs.end()) {
        return &it->second;
    }
    return nullptr;
}

bool FontAsset::HasGlyph(uint32_t codepoint) const {
    return m_Glyphs.find(codepoint) != m_Glyphs.end();
}

float FontAsset::GetKerning(uint32_t first, uint32_t second) const {
    uint64_t key = (static_cast<uint64_t>(first) << 32) | static_cast<uint64_t>(second);
    auto it = m_KerningTable.find(key);
    if (it != m_KerningTable.end()) {
        return it->second;
    }
    return 0.0f;
}

void FontAsset::GenerateAtlas(const std::vector<uint8_t>& fontData, float fontSize, uint32_t atlasWidth, uint32_t atlasHeight) {
    stbtt_fontinfo fontInfo;
    if (!stbtt_InitFont(&fontInfo, fontData.data(), 0)) {

        return;
    }

    float scale = stbtt_ScaleForPixelHeight(&fontInfo, fontSize);

    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&fontInfo, &ascent, &descent, &lineGap);

    m_Ascent = ascent * scale;
    m_Descent = descent * scale;
    m_LineHeight = (ascent - descent + lineGap) * scale;

    m_AtlasData.resize(atlasWidth * atlasHeight, 0);

    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t rowHeight = 0;

    for (uint32_t codepoint = 32; codepoint <= 126; ++codepoint) {
        int glyphIndex = stbtt_FindGlyphIndex(&fontInfo, codepoint);
        if (glyphIndex == 0) continue;

        int advance, lsb;
        stbtt_GetGlyphHMetrics(&fontInfo, glyphIndex, &advance, &lsb);

        int x0, y0, x1, y1;
        stbtt_GetGlyphBitmapBox(&fontInfo, glyphIndex, scale, scale, &x0, &y0, &x1, &y1);

        uint32_t glyphWidth = x1 - x0;
        uint32_t glyphHeight = y1 - y0;

        if (x + glyphWidth >= atlasWidth) {
            x = 0;
            y += rowHeight;
            rowHeight = 0;
        }

        if (y + glyphHeight >= atlasHeight) {

            break;
        }

        if (glyphWidth > 0 && glyphHeight > 0) {
            std::vector<uint8_t> glyphBitmap(glyphWidth * glyphHeight);
            stbtt_MakeGlyphBitmap(&fontInfo, glyphBitmap.data(), glyphWidth, glyphHeight, glyphWidth, scale, scale, glyphIndex);

            for (uint32_t gy = 0; gy < glyphHeight; ++gy) {
                for (uint32_t gx = 0; gx < glyphWidth; ++gx) {
                    uint32_t atlasIdx = (y + gy) * atlasWidth + (x + gx);
                    uint32_t glyphIdx = gy * glyphWidth + gx;
                    m_AtlasData[atlasIdx] = glyphBitmap[glyphIdx];
                }
            }
        }

        Glyph glyph;
        glyph.codepoint = codepoint;
        glyph.advance = advance * scale;
        glyph.bearingX = lsb * scale;
        glyph.bearingY = -y0;
        glyph.width = glyphWidth;
        glyph.height = glyphHeight;
        glyph.pixelX = x;
        glyph.pixelY = y;
        glyph.pixelWidth = glyphWidth;
        glyph.pixelHeight = glyphHeight;
        glyph.atlasX = static_cast<float>(x) / atlasWidth;
        glyph.atlasY = static_cast<float>(y) / atlasHeight;
        glyph.atlasWidth = static_cast<float>(glyphWidth) / atlasWidth;
        glyph.atlasHeight = static_cast<float>(glyphHeight) / atlasHeight;

        m_Glyphs[codepoint] = glyph;

        x += glyphWidth + 1;
        rowHeight = std::max(rowHeight, glyphHeight + 1);
    }
}

math::Vec2 FontAsset::MeasureText(const std::string& text, float fontSize) const {
    if (text.empty() || m_FontSize == 0.0f) {
        return math::Vec2(0.0f, 0.0f);
    }

    float scale = fontSize / m_FontSize;
    float width = 0.0f;
    float height = m_LineHeight * scale;

    for (size_t i = 0; i < text.length(); ++i) {
        char c = text[i];
        uint32_t codepoint = static_cast<uint32_t>(c);

        if (c == '\n') {
            height += m_LineHeight * scale;
            continue;
        }

        const Glyph* glyph = GetGlyph(codepoint);
        if (glyph) {
            width += glyph->advance * scale;
        } else if (c == ' ') {

            width += m_FontSize * 0.25f * scale;
        }
    }

    return math::Vec2(width, height);
}

}
}

