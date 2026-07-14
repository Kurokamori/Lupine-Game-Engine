#include <algorithm>
#include <fstream>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include "lupine/asset/FontAsset.hpp"
#include "lupine/rendering/TextLayout.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/platform/Platform.hpp"
#include "lupine/platform/PackFile.hpp"

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
    // Store the path (will be converted to res:// format if possible)
    SetPath(filepath);
    m_FontSize = fontSize;
    m_AtlasWidth = atlasWidth;
    m_AtlasHeight = atlasHeight;

    // Resolve the path using the asset system (handles UUID fallback)
    std::string resolvedPath = ResolveAssetPath(filepath);

    std::vector<uint8_t> fontData;

    // Check if running from pack file first
    auto& packFS = platform::PackFileSystem::Instance();
    std::string packPath = packFS.resolveAsset(filepath);

    if (packFS.isPackMode() && packFS.exists(packPath)) {
        fontData = packFS.readAsset(filepath);
        if (fontData.empty()) {
            LOG_ERROR(LogCategory::Core, "FontAsset: Failed to read from pack: {}", filepath);
            return false;
        }
    } else {
        // Use resolved physical path
        auto result = platform::FileSystem::ReadBinaryFile(resolvedPath);
        if (!result.success) {
            LOG_ERROR(LogCategory::Core, "FontAsset: Failed to read file: {} (resolved: {})",
                      filepath, resolvedPath);
            return false;
        }
        fontData = std::move(result.data);
    }

    GenerateAtlas(fontData, fontSize, atlasWidth, atlasHeight);

    SetLoaded(true);
    SetTrackedBytes(m_AtlasData.size());

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

        uint32_t glyphWidth = (x1 > x0) ? static_cast<uint32_t>(x1 - x0) : 0u;
        uint32_t glyphHeight = (y1 > y0) ? static_cast<uint32_t>(y1 - y0) : 0u;

        // A single glyph wider/taller than the whole atlas can never be packed; skip
        // it rather than let the row-relative write index below spill past the end of
        // m_AtlasData (a heap overflow). Cannot happen for the atlas sizing the UI
        // uses today, but the write must be safe regardless of font/atlas inputs.
        if (glyphWidth >= atlasWidth || glyphHeight >= atlasHeight) {
            continue;
        }

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
                    if (atlasIdx >= m_AtlasData.size()) {
                        continue;
                    }
                    uint32_t glyphIdx = gy * glyphWidth + gx;
                    m_AtlasData[atlasIdx] = glyphBitmap[glyphIdx];
                }
            }
        }

        Glyph glyph;
        glyph.codepoint = codepoint;
        glyph.advance = advance * scale;
        glyph.bearingX = lsb * scale;
        glyph.bearingY = static_cast<float>(-y0);
        glyph.width = static_cast<float>(glyphWidth);
        glyph.height = static_cast<float>(glyphHeight);
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

    // Kerning pairs over the same range. m_KerningTable was declared, cleared on reset, and
    // read by GetKerning() -- but nothing ever WROTE it, so every lookup returned 0 and the
    // font's kern table was silently ignored. It matters that this matches what FontBaker
    // bakes for the GPU atlas: the measure path reads these metrics and the draw path reads
    // the baked atlas, and if only one of them kerned, a Label would measure at one width and
    // then draw at another.
    for (uint32_t left = 32; left <= 126; ++left) {
        for (uint32_t right = 32; right <= 126; ++right) {
            const int raw = stbtt_GetCodepointKernAdvance(
                &fontInfo, static_cast<int>(left), static_cast<int>(right));
            if (raw == 0) {
                continue;
            }
            const uint64_t key = (static_cast<uint64_t>(left) << 32) | static_cast<uint64_t>(right);
            m_KerningTable[key] = static_cast<float>(raw) * scale;
        }
    }

    BuildMetricsAtlas();
}

void FontAsset::BuildMetricsAtlas() {
    m_MetricsAtlas = FontAtlas();
    m_MetricsAtlas.atlasWidth = m_AtlasWidth;
    m_MetricsAtlas.atlasHeight = m_AtlasHeight;
    m_MetricsAtlas.fontSize = m_FontSize;
    m_MetricsAtlas.lineHeight = m_LineHeight;
    m_MetricsAtlas.ascent = m_Ascent;
    // stb reports the descent as a negative offset from the baseline; FontAtlas
    // stores both vertical metrics as positive distances.
    m_MetricsAtlas.descent = -m_Descent;

    // Kerning, so TextLayout::Measure against this atlas produces the same advances as
    // TextLayout::Layout against the backend's baked atlas. Same key layout on both sides.
    m_MetricsAtlas.kerning.clear();
    for (const std::pair<const uint64_t, float>& entry : m_KerningTable) {
        m_MetricsAtlas.kerning[entry.first] = entry.second;
    }

    for (const auto& entry : m_Glyphs) {
        const Glyph& src = entry.second;

        lupine::Glyph dst;
        dst.codepoint = src.codepoint;
        dst.uvMin = math::Vec2(src.atlasX, src.atlasY);
        dst.uvMax = math::Vec2(src.atlasX + src.atlasWidth, src.atlasY + src.atlasHeight);
        dst.size = math::Vec2(src.width, src.height);
        // asset::Glyph::bearingY is the positive, upward baseline-to-top distance;
        // lupine::Glyph::bearing.y is the downward (negative) stb `yoff`.
        dst.bearing = math::Vec2(src.bearingX, -src.bearingY);
        dst.advance = src.advance;

        m_MetricsAtlas.glyphs[src.codepoint] = dst;
    }
}

math::Vec2 FontAsset::MeasureText(const std::string& text, float fontSize) const {
    if (text.empty() || m_FontSize == 0.0f) {
        return math::Vec2(0.0f, 0.0f);
    }

    // Delegate to the shared layout engine. The previous hand-rolled loop accumulated
    // every line's width into one running total (never resetting on '\n'), so a
    // two-line string measured as wide as both lines concatenated — which made
    // Button's FitToText roughly twice as wide as it should be. It also walked raw
    // bytes, turning any non-ASCII UTF-8 sequence into garbage codepoints.
    TextLayoutParams params;
    params.fontSize = fontSize;
    params.multiline = true;

    return TextLayout::Measure(text, m_MetricsAtlas, params);
}

}
}

