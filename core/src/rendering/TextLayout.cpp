#include "lupine/rendering/TextLayout.hpp"
#include <algorithm>
#include <cmath>

namespace lupine {

using math::Vec2;
using math::Color;

namespace {

// Decode a UTF-8 string into Unicode codepoints.
std::vector<uint32_t> DecodeUTF8(const std::string& s) {
    std::vector<uint32_t> out;
    out.reserve(s.size());
    const unsigned char* b = reinterpret_cast<const unsigned char*>(s.data());
    const size_t n = s.size();
    size_t i = 0;
    while (i < n) {
        unsigned char lead = b[i];
        uint32_t cp = 0;
        size_t extra = 0;
        if (lead < 0x80) { cp = lead; extra = 0; }
        else if ((lead & 0xE0) == 0xC0) { cp = lead & 0x1F; extra = 1; }
        else if ((lead & 0xF0) == 0xE0) { cp = lead & 0x0F; extra = 2; }
        else if ((lead & 0xF8) == 0xF0) { cp = lead & 0x07; extra = 3; }
        else { ++i; continue; }
        ++i;
        bool ok = true;
        for (size_t k = 0; k < extra; ++k) {
            if (i >= n || (b[i] & 0xC0) != 0x80) { ok = false; break; }
            cp = (cp << 6) | (b[i] & 0x3F);
            ++i;
        }
        if (ok) out.push_back(cp);
    }
    return out;
}

float GlyphAdvance(uint32_t cp, const Glyph* g, const FontAtlas& atlas, float scale, int tabSize) {
    if (cp == '\t') {
        // A tab is `tabSize` spaces wide. This used to be hardcoded to 4 with no way to
        // change it, and RichTextLabel independently assumed a different number.
        const int stops = (tabSize > 0) ? tabSize : 4;
        return atlas.fontSize * 0.25f * static_cast<float>(stops) * scale;
    }
    if (g) return g->advance * scale;
    if (cp == ' ') return atlas.fontSize * 0.25f * scale;
    return 0.0f;
}

// Per-codepoint working data.
struct LaidCp {
    uint32_t cp = 0;
    const Glyph* glyph = nullptr;
    float advance = 0.0f;
    bool isSpace = false;
    bool isNewline = false;
};

struct ShapedText {
    std::vector<LaidCp> cps;
    float scale = 1.0f;
    float ascent = 0.0f;
    float descent = 0.0f;
    float lineHeightBase = 0.0f;
    float lineAdvance = 0.0f;
};

ShapedText Shape(const std::string& text, const FontAtlas& atlas, const TextLayoutParams& params) {
    ShapedText st;
    st.scale = (atlas.fontSize > 0.0f) ? (params.fontSize / atlas.fontSize) : 1.0f;

    // Vertical metrics come from the FONT, never from the glyphs this particular
    // string happens to contain. Deriving them per-string (e.g. max glyph bearing)
    // makes the baseline jump around depending on whether the text has capitals or
    // descenders, and cannot work at all here because Glyph::bearing.y is stb's
    // negative `yoff` rather than a positive upward bearing.
    st.ascent = atlas.getAscent() * st.scale;
    st.descent = atlas.getDescent() * st.scale;
    st.lineHeightBase = atlas.getLineHeight() * st.scale;

    std::vector<uint32_t> codepoints = DecodeUTF8(text);
    st.cps.reserve(codepoints.size());

    for (uint32_t cp : codepoints) {
        LaidCp lc;
        lc.cp = cp;
        const bool hardNewline = (cp == '\n') && params.multiline;
        lc.isNewline = hardNewline;
        if (cp == '\n' && !params.multiline) {
            // Collapse newlines to spaces in single-line mode.
            lc.isSpace = true;
            lc.advance = atlas.fontSize * 0.25f * st.scale;
        } else if (hardNewline) {
            lc.advance = 0.0f;
        } else {
            lc.glyph = atlas.getGlyph(cp);
            lc.isSpace = (cp == ' ' || cp == '\t');
            lc.advance = GlyphAdvance(cp, lc.glyph, atlas, st.scale, params.tabSize);
        }

        // Kerning: fold the pair adjustment into the PRECEDING glyph's advance, so every
        // consumer of `advance` (line breaking, width measurement, justification, caret
        // placement) picks it up without knowing kerning exists. Layout was advance-only
        // before this, so "AV" and "To" were spaced as if the glyphs were rectangles.
        if (!st.cps.empty() && lc.glyph) {
            LaidCp& prev = st.cps.back();
            if (prev.glyph && !prev.isNewline) {
                prev.advance += atlas.getKerning(prev.cp, cp) * st.scale;
            }
        }

        st.cps.push_back(lc);
    }

    const float spacing = (params.lineSpacing > 0.0f) ? params.lineSpacing : 1.0f;
    st.lineAdvance = st.lineHeightBase * spacing;
    return st;
}

struct CpLine {
    size_t start = 0;  // codepoint index (inclusive)
    size_t end = 0;    // codepoint index (exclusive)
};

std::vector<CpLine> BreakInternal(const ShapedText& st, const TextLayoutParams& params) {
    std::vector<CpLine> lines;
    const std::vector<LaidCp>& v = st.cps;

    const TextAutowrapMode mode = params.EffectiveAutowrap();
    const bool wraps = (mode != TextAutowrapMode::Off) && (params.boxWidth > 0.0f);

    // Arbitrary breaks between any two characters. Word and WordSmart prefer a space; they
    // differ only in what happens to a single word that is itself wider than the box --
    // WordSmart splits it, Word lets it overflow.
    const bool preferWordBoundary = (mode == TextAutowrapMode::Word ||
                                     mode == TextAutowrapMode::WordSmart);
    const bool splitLongWords = (mode == TextAutowrapMode::Arbitrary ||
                                 mode == TextAutowrapMode::WordSmart);

    size_t start = 0;
    float curW = 0.0f;
    long lastSpace = -1;
    float widthAtSpaceInclusive = 0.0f;

    for (size_t i = 0; i < v.size(); ++i) {
        if (v[i].isNewline) {
            lines.push_back({start, i});
            start = i + 1;
            curW = 0.0f;
            lastSpace = -1;
            continue;
        }

        const float adv = v[i].advance;
        if (wraps && i > start && (curW + adv) > params.boxWidth) {
            const bool haveSpace = preferWordBoundary && (lastSpace >= static_cast<long>(start));

            if (haveSpace) {
                lines.push_back({start, static_cast<size_t>(lastSpace)});
                start = static_cast<size_t>(lastSpace) + 1;
                curW = curW - widthAtSpaceInclusive;
                lastSpace = -1;
            } else if (splitLongWords) {
                // No space to break at, and this mode is allowed to cut mid-word.
                lines.push_back({start, i});
                start = i;
                curW = 0.0f;
                lastSpace = -1;
            }
            // Word mode with no space on the line: the word overflows, by design.
        }

        if (v[i].isSpace) {
            lastSpace = static_cast<long>(i);
            widthAtSpaceInclusive = curW + adv;
        }
        curW += adv;
    }

    lines.push_back({start, v.size()});

    // Hard line cap. Applied here, after breaking, so a capped line is still a real line that
    // the overrun pass below can ellipsize.
    if (params.maxLines > 0 && lines.size() > static_cast<size_t>(params.maxLines)) {
        lines.resize(static_cast<size_t>(params.maxLines));
    }

    return lines;
}

/**
 * The ellipsis to append, as codepoints: U+2026 when the font has it, else three periods.
 */
std::vector<uint32_t> EllipsisCodepoints(const FontAtlas& atlas) {
    if (atlas.getGlyph(0x2026)) {
        return { 0x2026 };
    }
    return { '.', '.', '.' };
}

/**
 * Rewrite the lines that overflow `boxWidth` according to the overrun behavior: cut them
 * back to what fits (at a character or a word boundary) and, for the Ellipsis* modes, append
 * an ellipsis. Returns the codepoints to append per line (empty when nothing was appended).
 *
 * Overrun and clipping are different things and both exist: clipping cuts the GLYPH at the
 * box edge, while overrun rewrites the TEXT so it ends in a readable "...".
 */
void ApplyOverrun(const ShapedText& st, const TextLayoutParams& params, const FontAtlas& atlas,
                  std::vector<CpLine>& lines, std::vector<std::vector<uint32_t>>& outSuffix) {
    outSuffix.assign(lines.size(), {});

    if (params.overrunBehavior == TextOverrunBehavior::None || params.boxWidth <= 0.0f) {
        return;
    }

    const bool wantEllipsis = (params.overrunBehavior == TextOverrunBehavior::EllipsisChar ||
                               params.overrunBehavior == TextOverrunBehavior::EllipsisWord);
    const bool atWordBoundary = (params.overrunBehavior == TextOverrunBehavior::TrimWord ||
                                 params.overrunBehavior == TextOverrunBehavior::EllipsisWord);

    const std::vector<uint32_t> ellipsis = wantEllipsis ? EllipsisCodepoints(atlas)
                                                        : std::vector<uint32_t>{};

    float ellipsisWidth = 0.0f;
    for (uint32_t cp : ellipsis) {
        ellipsisWidth += GlyphAdvance(cp, atlas.getGlyph(cp), atlas, st.scale, params.tabSize);
    }

    for (size_t li = 0; li < lines.size(); ++li) {
        CpLine& line = lines[li];

        float width = 0.0f;
        for (size_t i = line.start; i < line.end; ++i) {
            width += st.cps[i].advance;
        }
        if (width <= params.boxWidth) {
            continue;
        }

        // Walk back until the visible text plus the ellipsis fits.
        const float budget = params.boxWidth - ellipsisWidth;
        size_t cut = line.start;
        float acc = 0.0f;
        for (size_t i = line.start; i < line.end; ++i) {
            const float next = acc + st.cps[i].advance;
            if (next > budget) {
                break;
            }
            acc = next;
            cut = i + 1;
        }

        if (atWordBoundary) {
            // Retreat to the last space, so the line does not end mid-word.
            size_t wordCut = cut;
            while (wordCut > line.start && !st.cps[wordCut - 1].isSpace) {
                --wordCut;
            }
            if (wordCut > line.start) {
                cut = wordCut;
            }
        }

        // Trailing spaces before an ellipsis read as a gap.
        while (cut > line.start && st.cps[cut - 1].isSpace) {
            --cut;
        }

        line.end = cut;
        if (wantEllipsis) {
            outSuffix[li] = ellipsis;
        }
    }
}

// Line advance width, with trailing whitespace trimmed (for alignment).
float TrimmedLineWidth(const ShapedText& st, const CpLine& ln) {
    size_t e = ln.end;
    while (e > ln.start && st.cps[e - 1].isSpace) --e;
    float w = 0.0f;
    for (size_t i = ln.start; i < e; ++i) w += st.cps[i].advance;
    return w;
}

void EmitGlyphQuad(std::vector<TextGlyphQuad>& out, float gx, float gy,
                   float gw, float gh, const Glyph* g, const Color& col, bool outline) {
    TextGlyphQuad q;
    q.color = col;
    q.isOutline = outline;
    // Matches the engine's existing glyph quad construction exactly so atlas
    // orientation is preserved.
    q.pos[0] = Vec2(gx, gy - gh);       q.uv[0] = Vec2(g->uvMin.x, g->uvMax.y);
    q.pos[1] = Vec2(gx + gw, gy - gh);  q.uv[1] = Vec2(g->uvMax.x, g->uvMax.y);
    q.pos[2] = Vec2(gx + gw, gy);       q.uv[2] = Vec2(g->uvMax.x, g->uvMin.y);
    q.pos[3] = Vec2(gx, gy);            q.uv[3] = Vec2(g->uvMin.x, g->uvMin.y);
    out.push_back(q);
}

// Emit a solid text outline as a set of offset glyph stamps whose union forms a
// continuous stroke. A naive 8-direction tap at the full radius leaves the offset
// copies disconnected for any width past ~1px, which reads as a handful of thin
// wires tracing the glyph rather than an outline. Instead, stamp concentric rings
// spaced ~1px apart from 1px out to the requested width, with the angular sample
// count of each ring scaled to its circumference so neighbouring stamps overlap.
// The outermost ring is always placed exactly at `width` so the outline thickness
// matches the requested value regardless of how it divides by the ring step.
void EmitOutlineStamps(std::vector<TextGlyphQuad>& out, float gx, float gy,
                       float gw, float gh, const Glyph* g, const Color& col,
                       float width) {
    constexpr float kTwoPi = 6.28318530717958647692f;
    // A 1px ring step with circumference-proportional sampling makes the stamp
    // count grow with width^2 — a 26px outline (the severe damage-number case)
    // emits ~2000 quads PER GLYPH, and Label rebuilds that mesh every frame the
    // number animates. Bound the work: keep the dense 1px rings for thin UI
    // outlines (width <= kMaxRings), but for thick outlines spread a fixed number
    // of rings across the width and cap each ring's sample count. The outermost
    // ring still lands exactly on `width`, so the stroke thickness is unchanged;
    // only the (imperceptible, at these sizes) inner sampling density drops.
    constexpr int kMaxRings = 6;
    constexpr int kMaxSamplesPerRing = 48;
    const float ringStep = std::max(1.0f, width / static_cast<float>(kMaxRings));
    for (float radius = ringStep; ; radius += ringStep) {
        const float r = std::min(radius, width);
        int samples = std::max(8, static_cast<int>(std::ceil(kTwoPi * r)));
        samples = std::min(samples, kMaxSamplesPerRing);
        for (int s = 0; s < samples; ++s) {
            const float a = kTwoPi * (static_cast<float>(s) / static_cast<float>(samples));
            EmitGlyphQuad(out, gx + std::cos(a) * r, gy + std::sin(a) * r,
                          gw, gh, g, col, true);
        }
        if (r >= width) break;
    }
}

} // namespace

std::vector<TextLine> TextLayout::BreakLines(const std::string& text,
                                             const FontAtlas& atlas,
                                             const TextLayoutParams& params) {
    ShapedText st = Shape(text, atlas, params);
    std::vector<CpLine> raw = BreakInternal(st, params);
    std::vector<TextLine> lines;
    lines.reserve(raw.size());
    for (const CpLine& cl : raw) {
        TextLine tl;
        tl.start = cl.start;
        tl.end = cl.end;
        tl.width = TrimmedLineWidth(st, cl);
        lines.push_back(tl);
    }
    return lines;
}

math::Vec2 TextLayout::Measure(const std::string& text,
                               const FontAtlas& atlas,
                               const TextLayoutParams& params) {
    ShapedText st = Shape(text, atlas, params);
    std::vector<CpLine> lines = BreakInternal(st, params);

    // Overrun trims the text, so it changes what the control measures too -- an ellipsized
    // line is exactly as wide as the box, not as wide as the string it came from.
    std::vector<std::vector<uint32_t>> suffix;
    ApplyOverrun(st, params, atlas, lines, suffix);

    float maxW = 0.0f;
    for (size_t li = 0; li < lines.size(); ++li) {
        float width = TrimmedLineWidth(st, lines[li]);
        for (uint32_t cp : suffix[li]) {
            width += GlyphAdvance(cp, atlas.getGlyph(cp), atlas, st.scale, params.tabSize);
        }
        maxW = std::max(maxW, width);
    }

    const int lineCount = static_cast<int>(lines.size());
    const float contentH = (lineCount > 0)
        ? (static_cast<float>(lineCount - 1) * st.lineAdvance + st.lineHeightBase)
        : 0.0f;
    return Vec2(maxW, contentH);
}

TextLayoutResult TextLayout::Layout(const std::string& text,
                                    const FontAtlas& atlas,
                                    const TextLayoutParams& params) {
    TextLayoutResult result;
    ShapedText st = Shape(text, atlas, params);
    std::vector<CpLine> lines = BreakInternal(st, params);

    std::vector<std::vector<uint32_t>> suffix;
    ApplyOverrun(st, params, atlas, lines, suffix);

    std::vector<float> widths(lines.size(), 0.0f);
    float maxW = 0.0f;
    for (size_t li = 0; li < lines.size(); ++li) {
        widths[li] = TrimmedLineWidth(st, lines[li]);
        for (uint32_t cp : suffix[li]) {
            widths[li] += GlyphAdvance(cp, atlas.getGlyph(cp), atlas, st.scale, params.tabSize);
        }
        maxW = std::max(maxW, widths[li]);
    }

    const int lineCount = static_cast<int>(lines.size());
    const float contentH = (lineCount > 0)
        ? (static_cast<float>(lineCount - 1) * st.lineAdvance + st.lineHeightBase)
        : 0.0f;

    const float boxW = (params.boxWidth > 0.0f) ? params.boxWidth : maxW;
    const float boxH = (params.boxHeight > 0.0f) ? params.boxHeight : contentH;

    // Distance the block is pushed DOWN from the box top. Local space is Y-up with
    // the origin at the box top-left, so this is applied as -vOffset.
    float vOffset = 0.0f;
    if (params.vAlign == TextVAlign::Center) vOffset = (boxH - contentH) * 0.5f;
    else if (params.vAlign == TextVAlign::Bottom) vOffset = (boxH - contentH);

    const float contentTopY = -vOffset;
    const float firstBaselineY = contentTopY - st.ascent;

    result.ascent = st.ascent;
    result.descent = st.descent;
    result.lineHeight = st.lineHeightBase;
    result.lineAdvance = st.lineAdvance;
    result.lineCount = lineCount;
    result.size = Vec2(maxW, contentH);
    result.contentTopY = contentTopY;
    result.firstBaselineY = firstBaselineY;

    const float outline = params.outlineWidth;

    // Collect shadow, outline and main quads separately so they render strictly back to
    // front in one mesh (draw order = index order): shadow, then outline, then glyphs.
    std::vector<TextGlyphQuad> shadowQuads;
    std::vector<TextGlyphQuad> outlineQuads;
    std::vector<TextGlyphQuad> mainQuads;

    // The canvas is Y-up, but an author giving a shadow offset means +y = DOWN the screen.
    const bool hasShadow = (params.shadowColor.a > 0.0f) &&
                           (params.shadowOffset.x != 0.0f || params.shadowOffset.y != 0.0f);
    const Vec2 shadowOffset(params.shadowOffset.x, -params.shadowOffset.y);

    for (size_t li = 0; li < lines.size(); ++li) {
        const CpLine& ln = lines[li];
        const float lw = widths[li];
        const bool isLast = (li + 1 == lines.size());

        float startX = 0.0f;
        float extraPerGap = 0.0f;
        switch (params.hAlign) {
            case TextHAlign::Left:   startX = 0.0f; break;
            case TextHAlign::Center: startX = (boxW - lw) * 0.5f; break;
            case TextHAlign::Right:  startX = (boxW - lw); break;
            case TextHAlign::Fill: {
                // Justify only when there is a real box to justify into; with no
                // boxWidth the line is already exactly as wide as its content.
                startX = 0.0f;
                if (!isLast && params.boxWidth > 0.0f) {
                    size_t e = ln.end;
                    while (e > ln.start && st.cps[e - 1].isSpace) --e;
                    int gaps = 0;
                    for (size_t i = ln.start; i < e; ++i) {
                        if (st.cps[i].isSpace) ++gaps;
                    }
                    if (gaps > 0) extraPerGap = (boxW - lw) / static_cast<float>(gaps);
                }
                break;
            }
        }

        // Local space is Y-up with the origin at the box TOP-left (see TextGlyphQuad),
        // so the block descends into negative Y: the first baseline sits one ascent
        // below the box top and successive lines step a further -lineAdvance.
        const float baselineY = firstBaselineY - static_cast<float>(li) * st.lineAdvance;
        float penX = startX;

        // Trim trailing spaces so justified/right alignment is not skewed, but keep
        // the advance contribution so wrap widths line up.
        size_t emitEnd = ln.end;
        while (emitEnd > ln.start && st.cps[emitEnd - 1].isSpace) --emitEnd;

        // clipText discards any glyph that does not lie wholly inside the box. This is a
        // per-glyph cut, unlike overrun, which rewrites the text to end in an ellipsis --
        // Label, Button and RichTextLabel offered neither, so long text simply spilled out
        // of the control.
        const bool clipping = params.clipText && (params.boxWidth > 0.0f || params.boxHeight > 0.0f);
        const float clipRight = (params.boxWidth > 0.0f) ? params.boxWidth : 0.0f;
        const float clipBottom = (params.boxHeight > 0.0f) ? -params.boxHeight : 0.0f;

        auto emit = [&](const Glyph* glyph, float glyphX, float glyphY) {
            const float gw = glyph->size.x * st.scale;
            const float gh = glyph->size.y * st.scale;

            if (clipping) {
                if (params.boxWidth > 0.0f && (glyphX < 0.0f || glyphX + gw > clipRight)) {
                    return;
                }
                if (params.boxHeight > 0.0f && (glyphY > 0.0f || glyphY - gh < clipBottom)) {
                    return;
                }
            }

            if (hasShadow) {
                EmitGlyphQuad(shadowQuads, glyphX + shadowOffset.x, glyphY + shadowOffset.y,
                              gw, gh, glyph, params.shadowColor, false);
            }
            if (outline > 0.0f) {
                EmitOutlineStamps(outlineQuads, glyphX, glyphY, gw, gh,
                                  glyph, params.outlineColor, outline);
            }
            EmitGlyphQuad(mainQuads, glyphX, glyphY, gw, gh, glyph, params.color, false);
        };

        for (size_t i = ln.start; i < ln.end; ++i) {
            const LaidCp& lc = st.cps[i];
            if (lc.glyph && i < emitEnd) {
                emit(lc.glyph,
                     penX + lc.glyph->bearing.x * st.scale,
                     baselineY - lc.glyph->bearing.y * st.scale);
            }

            float adv = lc.advance;
            if (lc.isSpace && params.hAlign == TextHAlign::Fill && !isLast) {
                adv += extraPerGap;
            }
            penX += adv;
        }

        // The ellipsis appended by an Ellipsis* overrun behavior.
        for (uint32_t cp : suffix[li]) {
            const Glyph* glyph = atlas.getGlyph(cp);
            if (glyph) {
                emit(glyph,
                     penX + glyph->bearing.x * st.scale,
                     baselineY - glyph->bearing.y * st.scale);
            }
            penX += GlyphAdvance(cp, glyph, atlas, st.scale, params.tabSize);
        }
    }

    result.quads.reserve(shadowQuads.size() + outlineQuads.size() + mainQuads.size());
    result.quads.insert(result.quads.end(), shadowQuads.begin(), shadowQuads.end());
    result.quads.insert(result.quads.end(), outlineQuads.begin(), outlineQuads.end());
    result.quads.insert(result.quads.end(), mainQuads.begin(), mainQuads.end());
    return result;
}

float TextLayout::MeasureLineWidth(const std::string& text,
                                   const FontAtlas& atlas,
                                   float fontSize) {
    TextLayoutParams params;
    params.fontSize = fontSize;
    params.multiline = false;
    ShapedText st = Shape(text, atlas, params);
    float w = 0.0f;
    for (const LaidCp& lc : st.cps) w += lc.advance;
    return w;
}

float TextLayout::MeasurePrefixWidth(const std::string& text,
                                     size_t count,
                                     const FontAtlas& atlas,
                                     float fontSize) {
    TextLayoutParams params;
    params.fontSize = fontSize;
    params.multiline = false;
    ShapedText st = Shape(text, atlas, params);
    float w = 0.0f;
    const size_t limit = std::min(count, st.cps.size());
    for (size_t i = 0; i < limit; ++i) w += st.cps[i].advance;
    return w;
}

} // namespace lupine
