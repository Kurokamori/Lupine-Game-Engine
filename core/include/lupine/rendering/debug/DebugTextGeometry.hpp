#pragma once

#include "lupine/math/Math.hpp"
#include <string>
#include <vector>

namespace lupine {

class DebugRenderer;

namespace debug {

using math::Vec2;
using math::Vec3;
using math::Color;

/**
 * A single line segment in local text space.
 *
 * Coordinates are laid out with the origin at the start of the first line's
 * baseline and +Y pointing up; capital letters reach roughly +`size` in Y and the
 * pen advances along +X. Multi-line strings extend downward (-Y).
 */
struct DebugTextSegment {
    Vec2 a;
    Vec2 b;
};

/**
 * Shared, backend-agnostic debug text geometry built from a real font.
 *
 * Every debug-renderer backend is fundamentally a line renderer, so debug text is
 * produced as glyph *outline* line segments (via stb_truetype's vector shapes) and
 * drawn through the existing line path — no per-backend texture atlas, sampler, or
 * textured pipeline is required. The font is the engine's default font, loaded once
 * and cached. If no font can be loaded, the helpers fall back to per-character
 * boxes so callers keep their previous behavior.
 */

/**
 * Build the line-segment geometry for `text` at the given em height `size`
 * (world units). Segments are in local 2D space (see DebugTextSegment). Returns an
 * empty vector when the debug font is unavailable.
 */
std::vector<DebugTextSegment> BuildTextSegments(const std::string& text, float size);

/**
 * True if the vector debug font loaded successfully and BuildTextSegments will
 * produce real glyph geometry (false means callers should expect the box fallback).
 */
bool IsDebugFontAvailable();

/**
 * Shared drawText implementation. Emits `text` at world `position` in the XY plane
 * (+Y up) by calling renderer.drawLine for each segment, falling back to
 * per-character boxes when no font is available. Each backend's drawText is a
 * one-line forward to this function.
 */
void EmitDebugText(DebugRenderer& renderer, const Vec3& position, const std::string& text,
                   const Color& color, float size, float duration);

} // namespace debug
} // namespace lupine
