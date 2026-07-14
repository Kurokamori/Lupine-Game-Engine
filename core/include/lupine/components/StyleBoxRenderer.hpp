#pragma once

#include "lupine/math/Math.hpp"
#include "lupine/math/Color.hpp"

namespace lupine {

class RenderContext;

namespace components {

class StyleBox;

/**
 * Draw any concrete StyleBox subtype into a UI rect.
 *
 * A single painter backs every StyleBox kind (StyleBoxFlat / StyleBoxTexture /
 * StyleBoxLine / StyleBoxEmpty), so components and the theme system can render a
 * resolved style without caring about its concrete type:
 *  - StyleBoxFlat    -> shadow + filled rounded rect + rounded border
 *  - StyleBoxTexture -> nine-patch (or stretched) textured rect, reusing DrawUIImage
 *  - StyleBoxLine    -> a single horizontal/vertical separator line
 *  - StyleBoxEmpty   -> nothing (margins only)
 *
 * @param ctx          Render context recording the draw items.
 * @param box          The style box to draw (no-op if null).
 * @param center       Pivot center of the control rect (canvas space, Y-down).
 * @param size         On-screen size of the control rect (before expand margins).
 * @param rotation     Rotation in radians about the center.
 * @param modulate     Per-draw tint multiplied into every painted colour (RGB and
 *                     alpha) — e.g. a control's opacity (white with opacity alpha)
 *                     or an interaction-state modulation colour (hover/pressed).
 */
void DrawStyleBox(RenderContext& ctx,
                  const StyleBox* box,
                  const math::Vec2& center,
                  const math::Vec2& size,
                  float rotation,
                  const math::Color& modulate = math::Color::White());

} // namespace components
} // namespace lupine
