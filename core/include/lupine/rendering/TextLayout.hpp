#pragma once

#include "lupine/math/Vec2.hpp"
#include "lupine/math/Color.hpp"
#include "lupine/rendering/Font.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace lupine {

/**
 * Horizontal text alignment within the layout box.
 *  - Left/Center/Right: align each line to the box.
 *  - Fill: justify (distribute inter-word space) every line except the last.
 */
enum class TextHAlign {
    Left = 0,
    Center = 1,
    Right = 2,
    Fill = 3
};

/**
 * Vertical text alignment within the layout box.
 */
enum class TextVAlign {
    Top = 0,
    Center = 1,
    Bottom = 2
};

/**
 * How a line too long for the box is broken. Mirrors Godot's autowrap_mode.
 *
 *  - Off       : never wrap; the line overflows (and may be trimmed -- see TextOverrun).
 *  - Arbitrary : break between any two characters, mid-word if need be.
 *  - Word      : break only at word boundaries. A single word wider than the box OVERFLOWS
 *                rather than being split.
 *  - WordSmart : break at word boundaries, but split a word that is itself wider than the
 *                box. This is the behaviour the old `wordWrap = true` bool produced.
 */
enum class TextAutowrapMode {
    Off = 0,
    Arbitrary = 1,
    Word = 2,
    WordSmart = 3
};

/**
 * What to do with a line that still does not fit after wrapping (or with wrapping off).
 * Mirrors Godot's text_overrun_behavior.
 *
 * Trim* cut the overflowing text; Ellipsis* cut it and append an ellipsis ("…", or "..."
 * when the font has no U+2026 glyph).
 */
enum class TextOverrunBehavior {
    None = 0,            // Let it overflow the box
    TrimChar = 1,        // Cut at the last character that fits
    TrimWord = 2,        // Cut at the last whole word that fits
    EllipsisChar = 3,    // Cut at a character, append an ellipsis
    EllipsisWord = 4     // Cut at a word, append an ellipsis
};

/**
 * Parameters controlling a text layout pass.
 */
struct TextLayoutParams {
    float fontSize = 16.0f;                 // Render size; scale = fontSize / atlas.fontSize
    math::Color color = math::Color::White();

    TextHAlign hAlign = TextHAlign::Left;
    TextVAlign vAlign = TextVAlign::Top;

    bool multiline = true;                  // Honor '\n'; if false, newlines become spaces

    // Line breaking. `wordWrap` is the original boolean and is still honored: when it is
    // true and autowrapMode is left Off, the mode is taken to be WordSmart, which is exactly
    // what the bool used to mean. Set autowrapMode explicitly for the other modes.
    bool wordWrap = false;
    TextAutowrapMode autowrapMode = TextAutowrapMode::Off;

    // What to do with text that still does not fit (ellipsis / trim). Applied per line
    // against boxWidth, and -- when boxHeight is set and maxLines is not -- to the lines that
    // fall past the bottom of the box.
    TextOverrunBehavior overrunBehavior = TextOverrunBehavior::None;

    // Discard glyphs that fall outside the layout box instead of letting them spill.
    // Independent of overrunBehavior: clipping cuts the glyph, overrun rewrites the text.
    bool clipText = false;

    // Hard cap on the number of laid-out lines (0 = unlimited). Lines past the cap are
    // dropped, and the last surviving line takes the overrun treatment.
    int maxLines = 0;

    // Width of a tab stop, in spaces. Tabs used to be a hardcoded 4-space advance that no
    // caller could change, and RichTextLabel disagreed with TextLayout about the number.
    int tabSize = 4;

    // Box used for wrapping (wordWrap) and alignment. A boxWidth <= 0 disables wrap
    // and uses the natural (widest-line) width for horizontal alignment, which makes
    // hAlign a no-op: there is no slack to distribute. Likewise a boxHeight <= 0 uses
    // the natural content height, making vAlign a no-op. Pass the control's RESOLVED
    // rect size here, not its authored width/height properties, or alignment will
    // silently do nothing for anchor- or container-sized controls.
    float boxWidth = 0.0f;
    float boxHeight = 0.0f;

    float lineSpacing = 1.0f;               // Multiplier applied to the line advance

    // Outline drawn behind the glyphs. outlineWidth is in render pixels; <= 0 = none.
    float outlineWidth = 0.0f;
    math::Color outlineColor = math::Color(0.0f, 0.0f, 0.0f, 1.0f);

    // Drop shadow: a copy of the glyphs stamped behind them at `shadowOffset` (in render
    // pixels, +y = DOWN the screen, as an author expects). Disabled when the colour is fully
    // transparent. StyleBoxFlat has a box shadow; text had none.
    math::Vec2 shadowOffset = math::Vec2(0.0f, 0.0f);
    math::Color shadowColor = math::Color(0.0f, 0.0f, 0.0f, 0.0f);

    /** The wrap mode actually in force, folding the legacy `wordWrap` bool in. */
    TextAutowrapMode EffectiveAutowrap() const {
        if (autowrapMode != TextAutowrapMode::Off) {
            return autowrapMode;
        }
        return wordWrap ? TextAutowrapMode::WordSmart : TextAutowrapMode::Off;
    }
};

/**
 * A single laid-out glyph quad in text-block-local coordinates.
 *
 * LOCAL SPACE CONTRACT: the origin (0,0) is the TOP-LEFT corner of the layout box,
 * and the space is Y-up (+X right, +Y screen-up) to match the canvas the UI renders
 * onto. Content therefore descends into NEGATIVE y: the box occupies
 * x in [0, boxWidth] and y in [-boxHeight, 0], successive lines step in -Y, and the
 * first line's baseline sits at y = -(ascent) when vAlign is Top.
 *
 * Consumers place the block by adding the world-space position of the box's TOP-LEFT
 * corner to `pos` (no Y flip, no ascent adjustment):
 *
 *     worldY = boxTopLeft.y + q.pos[k].y;
 *
 * Remember that on the Y-up canvas a math::Rect's `position` is its MIN corner
 * (bottom-left), so the box top-left is `rect.position + Vec2(0, rect.size.y)`.
 *
 * Vertex/UV order matches the engine's existing glyph quads:
 *   v[0] = bottom-left  (uvMin.x, uvMax.y)
 *   v[1] = bottom-right (uvMax.x, uvMax.y)
 *   v[2] = top-right    (uvMax.x, uvMin.y)
 *   v[3] = top-left     (uvMin.x, uvMin.y)
 * with triangles (0,1,2) and (0,2,3). Consumers transform pos into their own world
 * space (translation + rotation) and emit vertices using `color`.
 */
struct TextGlyphQuad {
    math::Vec2 pos[4];
    math::Vec2 uv[4];
    math::Color color;
    bool isOutline = false;
};

/**
 * One line of text after line-breaking.
 */
struct TextLine {
    size_t start = 0;   // First codepoint index of the line
    size_t end = 0;     // One past the last codepoint index (exclusive)
    float width = 0.0f; // Measured advance width in render units (trailing spaces trimmed)
};

/**
 * Result of laying out a string. All Y values are in the layout-local space
 * documented on TextGlyphQuad (origin = box top-left, Y-up, content descends).
 */
struct TextLayoutResult {
    // Outline quads first (drawn behind), then the main glyph quads.
    std::vector<TextGlyphQuad> quads;
    math::Vec2 size = math::Vec2(0.0f, 0.0f); // Measured content size (width, height)

    float ascent = 0.0f;        // Font ascent (baseline to em-box top), positive
    float descent = 0.0f;       // Font descent (baseline to em-box bottom), positive
    float lineHeight = 0.0f;    // Height of one line's em box
    float lineAdvance = 0.0f;   // Baseline-to-baseline step (lineHeight * lineSpacing)
    int lineCount = 0;

    // Local Y of the top of the laid-out text block. This is 0 for vAlign Top and
    // becomes negative as vertical alignment pushes the block down the box.
    float contentTopY = 0.0f;

    // Local Y of the first line's baseline (negative; == contentTopY - ascent).
    float firstBaselineY = 0.0f;

    /** Local Y of the top edge of line `index`'s em box. */
    float GetLineTopY(int index) const {
        return contentTopY - static_cast<float>(index) * lineAdvance;
    }

    /** Local Y of the bottom edge (min-Y corner) of line `index`'s em box. */
    float GetLineBottomY(int index) const {
        return GetLineTopY(index) - lineHeight;
    }

    /** Local Y of line `index`'s baseline. */
    float GetLineBaselineY(int index) const {
        return firstBaselineY - static_cast<float>(index) * lineAdvance;
    }
};

/**
 * Backend-independent text layout / shaping (single font atlas, left-to-right).
 *
 * Produces glyph quads with alignment, multiline, optional word-wrap, and an
 * optional outline. Shared by Label, Button, LineEdit, TextEdit, RichTextLabel and
 * other text-bearing components so the glyph math lives in one place.
 */
class TextLayout {
public:
    /**
     * Break text into lines, honoring explicit newlines (when params.multiline) and
     * optional word wrap to params.boxWidth.
     */
    static std::vector<TextLine> BreakLines(const std::string& text,
                                            const FontAtlas& atlas,
                                            const TextLayoutParams& params);

    /**
     * Measure the content size (widest line width, total height) of `text`.
     */
    static math::Vec2 Measure(const std::string& text,
                              const FontAtlas& atlas,
                              const TextLayoutParams& params);

    /**
     * Full layout: returns positioned glyph quads (with outline + alignment) plus
     * measured metrics. Consumers transform quad positions into world space.
     */
    static TextLayoutResult Layout(const std::string& text,
                                   const FontAtlas& atlas,
                                   const TextLayoutParams& params);

    /**
     * Advance width of a single line of text (no wrapping), measured from the atlas.
     * Useful for caret positioning in text fields.
     */
    static float MeasureLineWidth(const std::string& text,
                                  const FontAtlas& atlas,
                                  float fontSize);

    /**
     * X advance from the start of `text` up to codepoint index `count` (used to place
     * a caret). `count` is a count of decoded codepoints, not bytes.
     */
    static float MeasurePrefixWidth(const std::string& text,
                                    size_t count,
                                    const FontAtlas& atlas,
                                    float fontSize);
};

} // namespace lupine
