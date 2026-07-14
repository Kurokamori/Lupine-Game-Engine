#include "lupine/components/UIImageDraw.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include <cmath>
#include <algorithm>
#include <vector>

namespace lupine {
namespace components {

using namespace math;

const char* UIImageStretchModeToString(UIImageStretchMode mode) {
    switch (mode) {
        case UIImageStretchMode::Stretch:      return "Stretch";
        case UIImageStretchMode::KeepCentered: return "KeepCentered";
        case UIImageStretchMode::NineSlice:    return "NineSlice";
    }
    return "Stretch";
}

UIImageStretchMode UIImageStretchModeFromInt(int value) {
    if (value < 0 || value > 2) {
        return UIImageStretchMode::Stretch;
    }
    return static_cast<UIImageStretchMode>(value);
}

const char* UINineSliceAxisModeToString(UINineSliceAxisMode mode) {
    switch (mode) {
        case UINineSliceAxisMode::Stretch: return "Stretch";
        case UINineSliceAxisMode::Tile:    return "Tile";
    }
    return "Stretch";
}

UINineSliceAxisMode UINineSliceAxisModeFromInt(int value) {
    if (value < 0 || value > 1) {
        return UINineSliceAxisMode::Stretch;
    }
    return static_cast<UINineSliceAxisMode>(value);
}

namespace {

void DrawStretch(RenderContext& ctx, const Vec2& center, const Vec2& size, float rotation,
                 TextureHandle texture, const Color& color, const Vec4& cornerRadius) {
    if (std::abs(rotation) > 0.0001f) {
        ctx.drawRoundedRect(center, size, cornerRadius, color, texture, rotation,
                            Vec2(0.0f, 0.0f), Vec2(1.0f, 1.0f), 0);
    } else {
        Vec2 topLeft = Vec2(center.x - size.x * 0.5f, center.y - size.y * 0.5f);
        ctx.drawRoundedRect(topLeft, size, cornerRadius, color, texture,
                            Vec2(0.0f, 0.0f), Vec2(1.0f, 1.0f), 0);
    }
}

void DrawKeepCentered(RenderContext& ctx, const Vec2& center, const Vec2& size, float rotation,
                      TextureHandle texture, int textureWidth, int textureHeight,
                      const Color& color, const Vec4& cornerRadius) {
    if (textureWidth <= 0 || textureHeight <= 0) {
        DrawStretch(ctx, center, size, rotation, texture, color, cornerRadius);
        return;
    }

    // Show only the centered portion of the texture that fits the rect at native scale.
    float uScale = size.x / static_cast<float>(textureWidth);
    float vScale = size.y / static_cast<float>(textureHeight);

    float uMin = (1.0f - uScale) * 0.5f;
    float vMin = (1.0f - vScale) * 0.5f;
    float uMax = uMin + uScale;
    float vMax = vMin + vScale;

    if (std::abs(rotation) > 0.0001f) {
        ctx.drawRoundedRect(center, size, cornerRadius, color, texture, rotation,
                            Vec2(uMin, vMin), Vec2(uMax, vMax), 0);
    } else {
        Vec2 topLeft = Vec2(center.x - size.x * 0.5f, center.y - size.y * 0.5f);
        ctx.drawRoundedRect(topLeft, size, cornerRadius, color, texture,
                            Vec2(uMin, vMin), Vec2(uMax, vMax), 0);
    }
}

// One contiguous strip along an axis: where it starts within a region, how long it
// is on screen, and the source-UV span it samples (fraction along the region's UV band).
struct AxisSegment {
    float offset;  // start offset from the region's leading edge (px)
    float length;  // strip length on screen (px)
    float uvMin;   // source UV at the strip's leading edge
    float uvMax;   // source UV at the strip's trailing edge
};

// Build the strips that fill a region of the given on-screen length: one strip when
// stretching, or repeated native-size tiles (last one clipped via UV) when tiling.
static std::vector<AxisSegment> BuildAxisSegments(float regionLength, float uvLow, float uvHigh,
                                                  bool tile, float nativeTileLength) {
    // Safety valve: if tiling would need more than this many repeats (e.g. a tiny
    // source band stretched across a huge rect), fall back to a single stretched
    // strip instead of emitting an unbounded number of draw items.
    const int kMaxTiles = 256;

    std::vector<AxisSegment> segments;
    if (regionLength <= 0.0f) {
        return segments;
    }
    if (!tile || nativeTileLength <= 0.0f || (regionLength / nativeTileLength) > kMaxTiles) {
        segments.push_back({0.0f, regionLength, uvLow, uvHigh});
        return segments;
    }
    float uvSpan = uvHigh - uvLow;
    float pos = 0.0f;
    int guard = 0;
    while (pos < regionLength - 0.01f && guard++ <= kMaxTiles) {
        float len = std::min(nativeTileLength, regionLength - pos);
        float frac = len / nativeTileLength;
        segments.push_back({pos, len, uvLow, uvLow + uvSpan * frac});
        pos += len;
    }
    if (segments.empty()) {
        segments.push_back({0.0f, regionLength, uvLow, uvHigh});
    }
    return segments;
}

void DrawNineSlice(RenderContext& ctx, const Vec2& center, const Vec2& size, float rotation,
                   TextureHandle texture, int textureWidth, int textureHeight,
                   const Color& color, const UINineSlice& nineSlice) {
    if (textureWidth <= 0 || textureHeight <= 0) {
        DrawStretch(ctx, center, size, rotation, texture, color, Vec4(0.0f, 0.0f, 0.0f, 0.0f));
        return;
    }

    float texW = static_cast<float>(textureWidth);
    float texH = static_cast<float>(textureHeight);

    // Source-pixel margins, clamped so opposing pairs never overlap in the texture.
    float mL = std::max(0.0f, nineSlice.marginLeft);
    float mR = std::max(0.0f, nineSlice.marginRight);
    float mT = std::max(0.0f, nineSlice.marginTop);
    float mB = std::max(0.0f, nineSlice.marginBottom);
    if (mL + mR > texW && (mL + mR) > 0.0f) {
        float k = texW / (mL + mR);
        mL *= k; mR *= k;
    }
    if (mT + mB > texH && (mT + mB) > 0.0f) {
        float k = texH / (mT + mB);
        mT *= k; mB *= k;
    }

    // On-screen corner band sizes (drawn 1:1 from source px), clamped to the rect.
    float sL = mL, sR = mR, sT = mT, sB = mB;
    if (sL + sR > size.x && (sL + sR) > 0.0f) {
        float k = size.x / (sL + sR);
        sL *= k; sR *= k;
    }
    if (sT + sB > size.y && (sT + sB) > 0.0f) {
        float k = size.y / (sT + sB);
        sT *= k; sB *= k;
    }

    float uvL = mL / texW;
    float uvR = 1.0f - (mR / texW);
    float uvT = mT / texH;
    float uvB = 1.0f - (mB / texH);

    // Region edges in center-relative local space (Y-down).
    float left = -size.x * 0.5f;
    float top = -size.y * 0.5f;

    const bool rotated = std::abs(rotation) > 0.0001f;

    // Snap a local boundary so that (center + boundary) lands on a whole device
    // pixel. Every region derives its edges from these SHARED snapped boundaries,
    // so abutting quads stay perfectly contiguous — no fractional-pixel seam
    // (the thin gaps at the divisions) can appear between them. Snapping is
    // axis-aligned, so it is bypassed when the rect is rotated.
    auto snapX = [&](float localX) -> float {
        return rotated ? localX : std::round(center.x + localX) - center.x;
    };
    auto snapY = [&](float localY) -> float {
        return rotated ? localY : std::round(center.y + localY) - center.y;
    };

    // Four vertical and four horizontal grid lines defining the 3x3 patch.
    float bx0 = snapX(left);
    float bx1 = snapX(left + sL);
    float bx2 = snapX(left + size.x - sR);
    float bx3 = snapX(left + size.x);
    float by0 = snapY(top);
    float by1 = snapY(top + sT);
    float by2 = snapY(top + size.y - sB);
    float by3 = snapY(top + size.y);

    // Native tile sizes for the stretch/tile bands (source middle dimensions in px).
    float nativeMidW = texW - mL - mR;
    float nativeMidH = texH - mT - mB;
    bool tileH = (nineSlice.axisHorizontal == UINineSliceAxisMode::Tile);
    bool tileV = (nineSlice.axisVertical == UINineSliceAxisMode::Tile);

    const Vec4 noRadius(0.0f, 0.0f, 0.0f, 0.0f);

    auto rotateOffset = [rotation, rotated](float x, float y) -> Vec2 {
        if (!rotated) {
            return Vec2(x, y);
        }
        float cosR = std::cos(rotation);
        float sinR = std::sin(rotation);
        return Vec2(x * cosR - y * sinR, x * sinR + y * cosR);
    };

    // Fill the region bounded by [x0,x1] x [y0,y1] in local space, sampling the
    // [uMin,uMax]x[vMin,vMax] source band, optionally tiling along each axis.
    // Each emitted quad's edges are re-snapped to the pixel grid so tiled
    // sub-quads abut just as cleanly as the outer regions.
    auto fillRegion = [&](float x0, float x1, float y0, float y1,
                          float uMin, float uMax, float vMin, float vMax,
                          bool tileX, bool tileY) {
        float regionW = x1 - x0;
        float regionH = y1 - y0;
        if (regionW <= 0.0f || regionH <= 0.0f) {
            return;
        }
        std::vector<AxisSegment> xs = BuildAxisSegments(regionW, uMin, uMax, tileX, nativeMidW);
        std::vector<AxisSegment> ys = BuildAxisSegments(regionH, vMin, vMax, tileY, nativeMidH);
        for (const AxisSegment& ys_seg : ys) {
            float sy0 = snapY(y0 + ys_seg.offset);
            float sy1 = snapY(y0 + ys_seg.offset + ys_seg.length);
            float segH = sy1 - sy0;
            if (segH <= 0.0f) {
                continue;
            }
            float cy = (sy0 + sy1) * 0.5f;
            for (const AxisSegment& xs_seg : xs) {
                float sx0 = snapX(x0 + xs_seg.offset);
                float sx1 = snapX(x0 + xs_seg.offset + xs_seg.length);
                float segW = sx1 - sx0;
                if (segW <= 0.0f) {
                    continue;
                }
                float cx = (sx0 + sx1) * 0.5f;
                Vec2 drawCenter = center + rotateOffset(cx, cy);
                ctx.drawRoundedRect(drawCenter, Vec2(segW, segH), noRadius, color,
                                    texture, rotation,
                                    Vec2(xs_seg.uvMin, ys_seg.uvMin),
                                    Vec2(xs_seg.uvMax, ys_seg.uvMax), 0);
            }
        }
    };

    // Corners (fixed, 1:1).
    fillRegion(bx0, bx1, by0, by1, 0.0f, uvL, 0.0f, uvT, false, false);
    fillRegion(bx2, bx3, by0, by1, uvR, 1.0f, 0.0f, uvT, false, false);
    fillRegion(bx0, bx1, by2, by3, 0.0f, uvL, uvB, 1.0f, false, false);
    fillRegion(bx2, bx3, by2, by3, uvR, 1.0f, uvB, 1.0f, false, false);

    // Edges (stretch/tile along their long axis).
    fillRegion(bx1, bx2, by0, by1, uvL, uvR, 0.0f, uvT, tileH, false);  // top
    fillRegion(bx1, bx2, by2, by3, uvL, uvR, uvB, 1.0f, tileH, false);  // bottom
    fillRegion(bx0, bx1, by1, by2, 0.0f, uvL, uvT, uvB, false, tileV);  // left
    fillRegion(bx2, bx3, by1, by2, uvR, 1.0f, uvT, uvB, false, tileV);  // right

    // Center (optional, stretch/tile on both axes).
    if (nineSlice.drawCenter) {
        fillRegion(bx1, bx2, by1, by2, uvL, uvR, uvT, uvB, tileH, tileV);
    }
}

} // namespace

void DrawUIImage(RenderContext& ctx,
                 const Vec2& center,
                 const Vec2& size,
                 float rotation,
                 TextureHandle texture,
                 int textureWidth,
                 int textureHeight,
                 const Color& color,
                 const Vec4& cornerRadius,
                 UIImageStretchMode stretchMode,
                 const UINineSlice& nineSlice) {
    if (!texture.isValid() || size.x <= 0.0f || size.y <= 0.0f || color.a <= 0.0f) {
        return;
    }

    switch (stretchMode) {
        case UIImageStretchMode::Stretch:
            DrawStretch(ctx, center, size, rotation, texture, color, cornerRadius);
            break;
        case UIImageStretchMode::KeepCentered:
            DrawKeepCentered(ctx, center, size, rotation, texture, textureWidth, textureHeight,
                             color, cornerRadius);
            break;
        case UIImageStretchMode::NineSlice:
            DrawNineSlice(ctx, center, size, rotation, texture, textureWidth, textureHeight,
                          color, nineSlice);
            break;
    }
}

} // namespace components
} // namespace lupine
