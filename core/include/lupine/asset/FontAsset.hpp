#pragma once

#include "Asset.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/rendering/Font.hpp"
#include <vector>
#include <map>
#include <cstdint>

namespace lupine {
namespace asset {

/**
 * Glyph information
 */
struct Glyph {
    uint32_t codepoint;
    
    // Glyph metrics
    float advance;
    float bearingX;
    float bearingY;
    float width;
    float height;
    
    // Atlas coordinates (normalized 0-1)
    float atlasX;
    float atlasY;
    float atlasWidth;
    float atlasHeight;
    
    // Pixel coordinates in atlas
    uint32_t pixelX;
    uint32_t pixelY;
    uint32_t pixelWidth;
    uint32_t pixelHeight;
};

/**
 * Font asset class
 * Supports TTF and OTF formats
 */
class FontAsset : public Asset {
public:
    FontAsset();
    explicit FontAsset(const core::UUID& uuid);
    virtual ~FontAsset();

    AssetType GetType() const override { return AssetType::Font; }

    // Load from file
    bool LoadFromFile(const std::string& filepath, float fontSize = 48.0f, uint32_t atlasWidth = 512, uint32_t atlasHeight = 512);
    
    // Font properties
    float GetFontSize() const { return m_FontSize; }
    float GetLineHeight() const { return m_LineHeight; }
    float GetAscent() const { return m_Ascent; }
    float GetDescent() const { return m_Descent; }
    
    // Glyph access
    const Glyph* GetGlyph(uint32_t codepoint) const;
    bool HasGlyph(uint32_t codepoint) const;
    const std::map<uint32_t, Glyph>& GetAllGlyphs() const { return m_Glyphs; }
    
    // Atlas access
    uint32_t GetAtlasWidth() const { return m_AtlasWidth; }
    uint32_t GetAtlasHeight() const { return m_AtlasHeight; }
    const uint8_t* GetAtlasData() const { return m_AtlasData.data(); }
    size_t GetAtlasDataSize() const { return m_AtlasData.size(); }
    
    // Kerning
    float GetKerning(uint32_t first, uint32_t second) const;

    // Text measurement.
    //
    // NOTE: this is a plain single-pass advance sum. It ignores word wrap, line
    // spacing and alignment, and it does not reset the width per line. Prefer
    // TextLayout::Measure(text, GetMetricsAtlas(), params) for anything that has to
    // agree with what gets drawn.
    math::Vec2 MeasureText(const std::string& text, float fontSize) const;

    /**
     * The font's metrics expressed as a renderer-side FontAtlas (no texture).
     *
     * This lets CPU-side code — measurement, auto-sizing, bounds — run the very same
     * TextLayout used to position the glyphs, without needing a GfxDevice to bake a
     * GPU atlas first. Glyph bearings are converted into the renderer's convention
     * (see lupine::Glyph: bearing.y is the downward, negative stb `yoff`).
     */
    const FontAtlas& GetMetricsAtlas() const { return m_MetricsAtlas; }

private:
    void GenerateAtlas(const std::vector<uint8_t>& fontData, float fontSize, uint32_t atlasWidth, uint32_t atlasHeight);
    void BuildMetricsAtlas();

    FontAtlas m_MetricsAtlas;

    float m_FontSize{48.0f};
    float m_LineHeight{0.0f};
    float m_Ascent{0.0f};
    float m_Descent{0.0f};
    
    uint32_t m_AtlasWidth{512};
    uint32_t m_AtlasHeight{512};
    std::vector<uint8_t> m_AtlasData;
    
    std::map<uint32_t, Glyph> m_Glyphs;
    std::map<uint64_t, float> m_KerningTable; // (first << 32 | second) -> kerning
};

} // namespace asset
} // namespace lupine

