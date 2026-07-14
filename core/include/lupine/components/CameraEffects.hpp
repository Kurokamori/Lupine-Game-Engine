#pragma once

#include "lupine/components/CameraEffect.hpp"

namespace lupine {
namespace components {

// ===========================================================================
// Pointwise (composable) effects — fold into a single generated pass.
// ===========================================================================

/** Full color grading: contrast, saturation, brightness, white balance, lift/gamma/gain. */
class CameraEffectColorGrade : public CameraEffect {
public:
    CameraEffectColorGrade() = default;
    std::string GetTypeName() const override { return "CameraEffectColorGrade"; }
    void DefineProperties() override;
    bool IsComposable() const override { return true; }
    void BuildCompose(CameraComposeContribution& out) const override;
};

/** Exposure + tonemapping operator (Linear/Reinhard/ReinhardExt/ACES/Filmic/AGX). */
class CameraEffectTonemap : public CameraEffect {
public:
    CameraEffectTonemap() = default;
    std::string GetTypeName() const override { return "CameraEffectTonemap"; }
    void DefineProperties() override;
    bool IsComposable() const override { return true; }
    void BuildCompose(CameraComposeContribution& out) const override;
};

/** Vignette: darkened/colored edges with adjustable shape and falloff. */
class CameraEffectVignette : public CameraEffect {
public:
    CameraEffectVignette() = default;
    std::string GetTypeName() const override { return "CameraEffectVignette"; }
    void DefineProperties() override;
    bool IsComposable() const override { return true; }
    void BuildCompose(CameraComposeContribution& out) const override;
};

/** Animated film grain. */
class CameraEffectFilmGrain : public CameraEffect {
public:
    CameraEffectFilmGrain() = default;
    std::string GetTypeName() const override { return "CameraEffectFilmGrain"; }
    void DefineProperties() override;
    bool IsComposable() const override { return true; }
    void BuildCompose(CameraComposeContribution& out) const override;
};

/** Color inversion (negative), mixable by strength. */
class CameraEffectColorInvert : public CameraEffect {
public:
    CameraEffectColorInvert() = default;
    std::string GetTypeName() const override { return "CameraEffectColorInvert"; }
    void DefineProperties() override;
    bool IsComposable() const override { return true; }
    void BuildCompose(CameraComposeContribution& out) const override;
};

/** Posterize: quantize each channel to a number of levels. */
class CameraEffectPosterize : public CameraEffect {
public:
    CameraEffectPosterize() = default;
    std::string GetTypeName() const override { return "CameraEffectPosterize"; }
    void DefineProperties() override;
    bool IsComposable() const override { return true; }
    void BuildCompose(CameraComposeContribution& out) const override;
};

/** Hue rotation plus saturation/value scaling (YIQ-space, luminance preserving). */
class CameraEffectHueShift : public CameraEffect {
public:
    CameraEffectHueShift() = default;
    std::string GetTypeName() const override { return "CameraEffectHueShift"; }
    void DefineProperties() override;
    bool IsComposable() const override { return true; }
    void BuildCompose(CameraComposeContribution& out) const override;
};

// ===========================================================================
// Multi-tap effects — author their own standalone pass(es).
// ===========================================================================

/** Separable box/tent blur (two passes: horizontal then vertical). */
class CameraEffectBlur : public CameraEffect {
public:
    CameraEffectBlur() = default;
    std::string GetTypeName() const override { return "CameraEffectBlur"; }
    void DefineProperties() override;
    void BuildPasses(const CameraEffectContext& ctx, std::vector<CameraEffectPass>& out) const override;
};

/** Glow: radial bright-pass bloom added back, in a single pass. */
class CameraEffectGlow : public CameraEffect {
public:
    CameraEffectGlow() = default;
    std::string GetTypeName() const override { return "CameraEffectGlow"; }
    void DefineProperties() override;
    void BuildPasses(const CameraEffectContext& ctx, std::vector<CameraEffectPass>& out) const override;
};

/** Outline: edge detection (luminance Sobel, or depth discontinuity) overlaid in a color. */
class CameraEffectOutline : public CameraEffect {
public:
    CameraEffectOutline() = default;
    std::string GetTypeName() const override { return "CameraEffectOutline"; }
    void DefineProperties() override;
    void BuildPasses(const CameraEffectContext& ctx, std::vector<CameraEffectPass>& out) const override;
};

/** Pixelate: snap sampling to a coarse pixel grid. */
class CameraEffectPixelate : public CameraEffect {
public:
    CameraEffectPixelate() = default;
    std::string GetTypeName() const override { return "CameraEffectPixelate"; }
    void DefineProperties() override;
    void BuildPasses(const CameraEffectContext& ctx, std::vector<CameraEffectPass>& out) const override;
};

/** Unsharp-mask sharpening. */
class CameraEffectSharpen : public CameraEffect {
public:
    CameraEffectSharpen() = default;
    std::string GetTypeName() const override { return "CameraEffectSharpen"; }
    void DefineProperties() override;
    void BuildPasses(const CameraEffectContext& ctx, std::vector<CameraEffectPass>& out) const override;
};

/** Chromatic aberration: per-channel radial UV offset. */
class CameraEffectChromaticAberration : public CameraEffect {
public:
    CameraEffectChromaticAberration() = default;
    std::string GetTypeName() const override { return "CameraEffectChromaticAberration"; }
    void DefineProperties() override;
    void BuildPasses(const CameraEffectContext& ctx, std::vector<CameraEffectPass>& out) const override;
};

} // namespace components
} // namespace lupine
