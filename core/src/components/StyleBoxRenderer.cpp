#include "lupine/components/StyleBoxRenderer.hpp"
#include "lupine/components/StyleBox.hpp"
#include "lupine/components/UIImageDraw.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/rendering/TextureUpload.hpp"
#include "lupine/rendering/gfx/GfxDescriptors.hpp"

namespace lupine {
namespace components {

using namespace math;

// ============================================================ StyleBoxTexture GPU cache

int StyleBoxTexture::GetSourceTextureWidth() const {
    return (m_TextureAsset.IsValid() && m_TextureAsset->IsLoaded()) ? m_TextureAsset->GetWidth() : 0;
}

int StyleBoxTexture::GetSourceTextureHeight() const {
    return (m_TextureAsset.IsValid() && m_TextureAsset->IsLoaded()) ? m_TextureAsset->GetHeight() : 0;
}

TextureHandle StyleBoxTexture::EnsureGPUTexture(RenderContext& ctx) const {
    // Re-load when the source path changes (a fresh clone starts with an empty
    // cache, so themed/per-state styleboxes upload once on first draw).
    if (m_UploadedPath != m_TexturePath) {
        if (m_TextureHandle.isValid()) {
            if (auto* device = ctx.getDevice()) {
                device->destroyTexture(m_TextureHandle);
            }
            m_TextureHandle = TextureHandle();
        }
        m_TextureAsset.Reset();
        m_UploadedPath = m_TexturePath;

        if (!m_TexturePath.empty()) {
            m_TextureAsset = asset::AssetRef<asset::ImageAsset>(new asset::ImageAsset());
            if (!m_TextureAsset->LoadFromFile(m_TexturePath, true, asset::ImageColorSpace::sRGB)) {
                m_TextureAsset.Reset();
            }
        }
    }

    if (m_TextureHandle.isValid()) {
        return m_TextureHandle;
    }

    if (!m_TextureAsset.IsValid() || !m_TextureAsset->IsLoaded() || !m_TextureAsset->GetData()) {
        return TextureHandle();
    }

    auto* device = ctx.getDevice();
    if (!device) {
        return TextureHandle();
    }

    m_TextureHandle = lupine::CreateTexture2DFromImage(device, *m_TextureAsset, TextureFormat::RGBA8_UNORM);
    return m_TextureHandle;
}

// ============================================================ DrawStyleBox

namespace {

// (TL, TR, BR, BL) packing expected by RenderContext::drawRoundedRect.
Vec4 FlatCornerRadius(const StyleBoxFlat& flat) {
    return Vec4(flat.GetCornerRadiusTopLeft(), flat.GetCornerRadiusTopRight(),
                flat.GetCornerRadiusBottomRight(), flat.GetCornerRadiusBottomLeft());
}

// Component-wise colour tint (RGB and alpha).
Color Modulated(const Color& c, const Color& m) {
    return Color(c.r * m.r, c.g * m.g, c.b * m.b, c.a * m.a);
}

void DrawFlat(RenderContext& ctx, const StyleBoxFlat& flat,
              Vec2 center, Vec2 size, float rotation, const Color& modulate) {
    // Expand margins grow the painted box beyond the control rect (asymmetric).
    float ex = flat.GetExpandMarginLeft();
    float ey = flat.GetExpandMarginTop();
    float ez = flat.GetExpandMarginRight();
    float ew = flat.GetExpandMarginBottom();
    if (ex != 0.0f || ey != 0.0f || ez != 0.0f || ew != 0.0f) {
        Vec2 topLeft = Vec2(center.x - size.x * 0.5f - ex, center.y - size.y * 0.5f - ey);
        size = Vec2(size.x + ex + ez, size.y + ey + ew);
        center = Vec2(topLeft.x + size.x * 0.5f, topLeft.y + size.y * 0.5f);
    }

    Vec4 cornerRadius = FlatCornerRadius(flat);

    // Drop shadow first, behind the box (alpha-modulated only, never RGB-tinted).
    if (flat.GetShadowEnabled()) {
        float s = flat.GetShadowSize();
        Color shadowColor = flat.GetShadowColor();
        shadowColor.a *= modulate.a;
        Vec2 shadowCenter = center + flat.GetShadowOffset();
        Vec2 shadowSize = Vec2(size.x + s * 2.0f, size.y + s * 2.0f);
        ctx.drawRoundedRect(shadowCenter, shadowSize, cornerRadius, shadowColor, rotation, 0);
    }

    // Fill.
    Color bg = Modulated(flat.GetBackgroundColor(), modulate);
    bg.a *= flat.GetOpacity();
    if (bg.a > 0.0f) {
        ctx.drawRoundedRect(center, size, cornerRadius, bg, rotation, 0);
    }

    // Border (a single primitive: one color, per-side widths).
    float bl = flat.GetBorderWidthLeft();
    float bt = flat.GetBorderWidthTop();
    float br = flat.GetBorderWidthRight();
    float bb = flat.GetBorderWidthBottom();
    if (bl > 0.0f || bt > 0.0f || br > 0.0f || bb > 0.0f) {
        Color borderColor = Modulated(flat.GetBorderColorTop(), modulate);
        // RenderContext border width packing is (top, right, bottom, left).
        Vec4 borderWidth = Vec4(bt, br, bb, bl);
        ctx.drawRoundedRectBorder(center, size, cornerRadius, borderWidth, borderColor, rotation);
    }
}

UINineSliceAxisMode ToAxisMode(StyleBoxTexture::AxisStretchMode mode) {
    // The shared nine-slice painter supports Stretch and Tile; TileFit is
    // approximated as Tile (closest available behavior).
    switch (mode) {
        case StyleBoxTexture::AxisStretchMode::Stretch: return UINineSliceAxisMode::Stretch;
        case StyleBoxTexture::AxisStretchMode::Tile:    return UINineSliceAxisMode::Tile;
        case StyleBoxTexture::AxisStretchMode::TileFit:  return UINineSliceAxisMode::Tile;
    }
    return UINineSliceAxisMode::Stretch;
}

void DrawTexture(RenderContext& ctx, const StyleBoxTexture& tex,
                 Vec2 center, Vec2 size, float rotation, const Color& modulate) {
    TextureHandle handle = tex.EnsureGPUTexture(ctx);
    if (!handle.isValid()) {
        return;
    }
    int texW = tex.GetSourceTextureWidth();
    int texH = tex.GetSourceTextureHeight();
    if (texW == 0 || texH == 0) {
        return;
    }

    // Expand margins grow the drawn box beyond the control rect.
    float ex = tex.GetExpandMarginLeft();
    float ey = tex.GetExpandMarginTop();
    float ez = tex.GetExpandMarginRight();
    float ew = tex.GetExpandMarginBottom();
    if (ex != 0.0f || ey != 0.0f || ez != 0.0f || ew != 0.0f) {
        Vec2 topLeft = Vec2(center.x - size.x * 0.5f - ex, center.y - size.y * 0.5f - ey);
        size = Vec2(size.x + ex + ez, size.y + ey + ew);
        center = Vec2(topLeft.x + size.x * 0.5f, topLeft.y + size.y * 0.5f);
    }

    Color tint = Modulated(tex.GetModulateColor(), modulate);

    bool hasMargins = tex.GetTextureMarginLeft() > 0.0f || tex.GetTextureMarginTop() > 0.0f ||
                      tex.GetTextureMarginRight() > 0.0f || tex.GetTextureMarginBottom() > 0.0f;

    if (!hasMargins) {
        DrawUIImage(ctx, center, size, rotation, handle, texW, texH, tint,
                    Vec4(0.0f, 0.0f, 0.0f, 0.0f), UIImageStretchMode::Stretch);
        return;
    }

    UINineSlice nineSlice;
    nineSlice.marginLeft = tex.GetTextureMarginLeft();
    nineSlice.marginTop = tex.GetTextureMarginTop();
    nineSlice.marginRight = tex.GetTextureMarginRight();
    nineSlice.marginBottom = tex.GetTextureMarginBottom();
    nineSlice.axisHorizontal = ToAxisMode(tex.GetAxisStretchHorizontal());
    nineSlice.axisVertical = ToAxisMode(tex.GetAxisStretchVertical());
    nineSlice.drawCenter = tex.GetDrawCenter();

    DrawUIImage(ctx, center, size, rotation, handle, texW, texH, tint,
                Vec4(0.0f, 0.0f, 0.0f, 0.0f), UIImageStretchMode::NineSlice, nineSlice);
}

void DrawLine(RenderContext& ctx, const StyleBoxLine& line,
              Vec2 center, Vec2 size, const Color& modulate) {
    Color color = Modulated(line.GetColor(), modulate);
    float thickness = line.GetThickness();
    Vec2 topLeft = Vec2(center.x - size.x * 0.5f, center.y - size.y * 0.5f);

    if (line.GetVertical()) {
        float x = center.x;
        float y0 = topLeft.y - line.GetGrowBegin();
        float y1 = topLeft.y + size.y + line.GetGrowEnd();
        ctx.drawLine(Vec3(x, y0, 0.0f), Vec3(x, y1, 0.0f), color, thickness);
    } else {
        float y = center.y;
        float x0 = topLeft.x - line.GetGrowBegin();
        float x1 = topLeft.x + size.x + line.GetGrowEnd();
        ctx.drawLine(Vec3(x0, y, 0.0f), Vec3(x1, y, 0.0f), color, thickness);
    }
}

} // namespace

void DrawStyleBox(RenderContext& ctx, const StyleBox* box,
                  const math::Vec2& center, const math::Vec2& size,
                  float rotation, const math::Color& modulate) {
    if (!box || modulate.a <= 0.0f) {
        return;
    }

    if (const auto* flat = dynamic_cast<const StyleBoxFlat*>(box)) {
        DrawFlat(ctx, *flat, center, size, rotation, modulate);
    } else if (const auto* tex = dynamic_cast<const StyleBoxTexture*>(box)) {
        DrawTexture(ctx, *tex, center, size, rotation, modulate);
    } else if (const auto* line = dynamic_cast<const StyleBoxLine*>(box)) {
        DrawLine(ctx, *line, center, size, modulate);
    }
    // StyleBoxEmpty (and any unknown subtype) paints nothing.
}

} // namespace components
} // namespace lupine
