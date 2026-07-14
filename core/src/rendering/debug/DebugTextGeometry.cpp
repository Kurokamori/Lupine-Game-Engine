#include "lupine/rendering/debug/DebugTextGeometry.hpp"
#include "lupine/rendering/debug/DebugRenderer.hpp"
#include "lupine/asset/AssetDatabase.hpp"
#include "lupine/asset/Asset.hpp"
#include "lupine/platform/FileSystem.hpp"
#include "lupine/logger/Logger.hpp"

#include <stb_truetype.h>

#include <unordered_map>
#include <vector>

namespace lupine {
namespace debug {

namespace {

// A glyph's outline in normalized units (font units * baseScale, so that the font's
// ascent maps to ~1.0). Callers scale by the requested em size.
struct CachedGlyph {
    std::vector<std::vector<Vec2>> contours;  // closed polylines
    float advance = 0.0f;
};

struct DebugFont {
    bool loadAttempted = false;
    bool loaded = false;
    std::vector<unsigned char> bytes;  // must outlive fontInfo (it points into this)
    stbtt_fontinfo info{};
    float baseScale = 0.0f;     // font units -> normalized (ascent == 1.0)
    float lineAdvance = 1.25f;  // normalized vertical advance between lines
    std::unordered_map<int, CachedGlyph> glyphs;
};

DebugFont& Font() {
    static DebugFont font;
    return font;
}

// Flatten a quadratic bezier (p0->p2, control p1) into `dst`, excluding p0 (assumed
// already present) and including p2.
void FlattenQuadratic(const Vec2& p0, const Vec2& p1, const Vec2& p2, std::vector<Vec2>& dst) {
    constexpr int kSegments = 8;
    for (int i = 1; i <= kSegments; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(kSegments);
        float u = 1.0f - t;
        float x = u * u * p0.x + 2.0f * u * t * p1.x + t * t * p2.x;
        float y = u * u * p0.y + 2.0f * u * t * p1.y + t * t * p2.y;
        dst.push_back(Vec2(x, y));
    }
}

// Flatten a cubic bezier (p0->p3, controls p1,p2) into `dst`, excluding p0.
void FlattenCubic(const Vec2& p0, const Vec2& p1, const Vec2& p2, const Vec2& p3, std::vector<Vec2>& dst) {
    constexpr int kSegments = 12;
    for (int i = 1; i <= kSegments; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(kSegments);
        float u = 1.0f - t;
        float x = u * u * u * p0.x + 3.0f * u * u * t * p1.x + 3.0f * u * t * t * p2.x + t * t * t * p3.x;
        float y = u * u * u * p0.y + 3.0f * u * u * t * p1.y + 3.0f * u * t * t * p2.y + t * t * t * p3.y;
        dst.push_back(Vec2(x, y));
    }
}

void EnsureFontLoaded() {
    DebugFont& font = Font();
    if (font.loadAttempted) {
        return;
    }
    font.loadAttempted = true;

    std::string fontPath;
    asset::AssetDatabase& assetDb = asset::AssetDatabase::GetInstance();
    if (assetDb.IsInitialized()) {
        fontPath = assetDb.GetDefaultFontPath();
    }
    if (fontPath.empty()) {
        fontPath = "res://assets/fonts/DefaultFont.ttf";
    }

    std::string resolved = asset::Asset::ResolveAssetPath(fontPath);
    auto result = platform::FileSystem::ReadBinaryFile(resolved);
    if (!result.success || result.data.empty()) {
        LOG_WARN(LogCategory::Render,
                 "[DebugText] Could not load debug font '{}' (resolved '{}'); "
                 "debug text will fall back to boxes.", fontPath, resolved);
        return;
    }

    font.bytes.assign(result.data.begin(), result.data.end());
    if (!stbtt_InitFont(&font.info, font.bytes.data(), stbtt_GetFontOffsetForIndex(font.bytes.data(), 0))) {
        LOG_WARN(LogCategory::Render, "[DebugText] stbtt_InitFont failed for '{}'.", resolved);
        font.bytes.clear();
        return;
    }

    int ascent = 0, descent = 0, lineGap = 0;
    stbtt_GetFontVMetrics(&font.info, &ascent, &descent, &lineGap);
    if (ascent <= 0) {
        LOG_WARN(LogCategory::Render, "[DebugText] Font '{}' has non-positive ascent.", resolved);
        font.bytes.clear();
        return;
    }

    font.baseScale = 1.0f / static_cast<float>(ascent);
    font.lineAdvance = static_cast<float>(ascent - descent + lineGap) * font.baseScale;
    font.loaded = true;
}

// Build (and cache) the normalized outline for one codepoint.
const CachedGlyph& GetGlyph(int codepoint) {
    DebugFont& font = Font();
    auto it = font.glyphs.find(codepoint);
    if (it != font.glyphs.end()) {
        return it->second;
    }

    CachedGlyph glyph;

    int advanceWidth = 0, leftBearing = 0;
    stbtt_GetCodepointHMetrics(&font.info, codepoint, &advanceWidth, &leftBearing);
    glyph.advance = static_cast<float>(advanceWidth) * font.baseScale;

    stbtt_vertex* verts = nullptr;
    int vertCount = stbtt_GetCodepointShape(&font.info, codepoint, &verts);
    if (verts && vertCount > 0) {
        std::vector<Vec2> contour;
        Vec2 pen(0.0f, 0.0f);

        auto scale = [&](float v) { return v * font.baseScale; };

        for (int i = 0; i < vertCount; ++i) {
            const stbtt_vertex& v = verts[i];
            Vec2 to(scale(static_cast<float>(v.x)), scale(static_cast<float>(v.y)));

            switch (v.type) {
                case STBTT_vmove:
                    if (contour.size() >= 2) {
                        contour.push_back(contour.front());  // close previous contour
                        glyph.contours.push_back(std::move(contour));
                    }
                    contour.clear();
                    contour.push_back(to);
                    pen = to;
                    break;
                case STBTT_vline:
                    contour.push_back(to);
                    pen = to;
                    break;
                case STBTT_vcurve: {
                    Vec2 ctrl(scale(static_cast<float>(v.cx)), scale(static_cast<float>(v.cy)));
                    FlattenQuadratic(pen, ctrl, to, contour);
                    pen = to;
                    break;
                }
                case STBTT_vcubic: {
                    Vec2 c0(scale(static_cast<float>(v.cx)), scale(static_cast<float>(v.cy)));
                    Vec2 c1(scale(static_cast<float>(v.cx1)), scale(static_cast<float>(v.cy1)));
                    FlattenCubic(pen, c0, c1, to, contour);
                    pen = to;
                    break;
                }
                default:
                    break;
            }
        }

        if (contour.size() >= 2) {
            contour.push_back(contour.front());
            glyph.contours.push_back(std::move(contour));
        }
    }

    if (verts) {
        stbtt_FreeShape(&font.info, verts);
    }

    auto inserted = font.glyphs.emplace(codepoint, std::move(glyph));
    return inserted.first->second;
}

} // namespace

bool IsDebugFontAvailable() {
    EnsureFontLoaded();
    return Font().loaded;
}

std::vector<DebugTextSegment> BuildTextSegments(const std::string& text, float size) {
    std::vector<DebugTextSegment> segments;

    EnsureFontLoaded();
    DebugFont& font = Font();
    if (!font.loaded) {
        return segments;
    }

    float cursorX = 0.0f;
    float cursorY = 0.0f;

    for (char ch : text) {
        if (ch == '\n') {
            cursorX = 0.0f;
            cursorY -= font.lineAdvance * size;
            continue;
        }
        if (ch == '\r') {
            continue;
        }

        int codepoint = static_cast<unsigned char>(ch);
        const CachedGlyph& glyph = GetGlyph(codepoint);

        for (const std::vector<Vec2>& contour : glyph.contours) {
            for (size_t i = 1; i < contour.size(); ++i) {
                DebugTextSegment seg;
                seg.a = Vec2(cursorX + contour[i - 1].x * size, cursorY + contour[i - 1].y * size);
                seg.b = Vec2(cursorX + contour[i].x * size, cursorY + contour[i].y * size);
                segments.push_back(seg);
            }
        }

        cursorX += glyph.advance * size;
    }

    return segments;
}

void EmitDebugText(DebugRenderer& renderer, const Vec3& position, const std::string& text,
                   const Color& color, float size, float duration) {
    std::vector<DebugTextSegment> segments = BuildTextSegments(text, size);

    if (!segments.empty()) {
        for (const DebugTextSegment& seg : segments) {
            Vec3 a(position.x + seg.a.x, position.y + seg.a.y, position.z);
            Vec3 b(position.x + seg.b.x, position.y + seg.b.y, position.z);
            renderer.drawLine(a, b, color, duration, true);
        }
        return;
    }

    // Fallback (no font available): one box per character, matching the previous
    // placeholder behavior but using only the shared line path.
    float charWidth = size * 0.6f;
    float charHeight = size;
    float cursorX = position.x;

    for (size_t i = 0; i < text.length(); ++i) {
        if (text[i] == '\n' || text[i] == '\r') {
            cursorX = position.x;
            continue;
        }
        float x0 = cursorX + charWidth * 0.1f;
        float x1 = cursorX + charWidth * 0.9f;
        float y0 = position.y + charHeight * 0.1f;
        float y1 = position.y + charHeight * 0.9f;
        float z = position.z;

        Vec3 bl(x0, y0, z), br(x1, y0, z), tr(x1, y1, z), tl(x0, y1, z);
        renderer.drawLine(bl, br, color, duration, true);
        renderer.drawLine(br, tr, color, duration, true);
        renderer.drawLine(tr, tl, color, duration, true);
        renderer.drawLine(tl, bl, color, duration, true);

        cursorX += charWidth;
    }
}

} // namespace debug
} // namespace lupine
