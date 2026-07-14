#pragma once

#include "lupine/math/Math.hpp"
#include "lupine/math/Color.hpp"
#include "lupine/rendering/ResourceHandles.hpp"
#include <string>

namespace lupine {

class RenderContext;

namespace components {

/**
 * How a UI background/state image is fitted into the control's rect.
 *
 * Shared by every UI component that paints an optional image behind/over its
 * flat background (Button, Panel, ...). Kept value-stable (append-only) so it
 * can be serialized as an int via PROPERTY_ENUM.
 */
enum class UIImageStretchMode {
    Stretch = 0,        // Fill the rect, ignoring aspect ratio. Honors corner radius.
    KeepCentered = 1,   // Draw the texture at native size, centered and cropped to the rect.
    NineSlice = 2       // 3x3 slicing: corners stay fixed, edges/center stretch or tile. Corner radius ignored.
};

const char* UIImageStretchModeToString(UIImageStretchMode mode);
UIImageStretchMode UIImageStretchModeFromInt(int value);

/**
 * How a nine-slice edge/center region is filled along one axis.
 */
enum class UINineSliceAxisMode {
    Stretch = 0,   // Stretch the source middle band across the region.
    Tile = 1       // Repeat the source middle band at its native pixel size across the region.
};

const char* UINineSliceAxisModeToString(UINineSliceAxisMode mode);
UINineSliceAxisMode UINineSliceAxisModeFromInt(int value);

/**
 * Proper nine-slice (9-patch) configuration. Margins are measured in SOURCE texture
 * pixels and define both the fixed corner/edge bands in the texture (UV split) and
 * their on-screen size (corners are drawn 1:1, never stretched). Godot NinePatchRect
 * parity: independent per-side margins, per-axis stretch/tile, optional center fill.
 */
struct UINineSlice {
    float marginLeft = 0.0f;
    float marginTop = 0.0f;
    float marginRight = 0.0f;
    float marginBottom = 0.0f;
    UINineSliceAxisMode axisHorizontal = UINineSliceAxisMode::Stretch;
    UINineSliceAxisMode axisVertical = UINineSliceAxisMode::Stretch;
    bool drawCenter = true;
};

/**
 * Draw a textured rounded rectangle for a UI control using the given stretch mode.
 *
 * @param ctx             Render context recording the draw item.
 * @param center          Pivot center of the rect (canvas space, top-left origin / Y-down).
 * @param size            On-screen size of the rect.
 * @param rotation        Rotation in radians (about the center).
 * @param texture         GPU texture handle to sample.
 * @param textureWidth    Source texture width in pixels (for KeepCentered / NineSlice math).
 * @param textureHeight   Source texture height in pixels.
 * @param color           Tint/modulation color (alpha included).
 * @param cornerRadius    Per-corner radius (only applied in Stretch / KeepCentered modes).
 * @param stretchMode     Fit behavior.
 * @param nineSlice       Nine-slice configuration (used only when stretchMode == NineSlice).
 */
void DrawUIImage(RenderContext& ctx,
                 const math::Vec2& center,
                 const math::Vec2& size,
                 float rotation,
                 TextureHandle texture,
                 int textureWidth,
                 int textureHeight,
                 const math::Color& color,
                 const math::Vec4& cornerRadius,
                 UIImageStretchMode stretchMode,
                 const UINineSlice& nineSlice = UINineSlice{});

} // namespace components
} // namespace lupine
