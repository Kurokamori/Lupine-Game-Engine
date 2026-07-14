#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/rendering/CameraEffectStack.hpp"
#include <vector>

namespace lupine {
namespace components {

/**
 * Per-frame, renderer-supplied context handed to multi-tap effects when they author
 * their standalone passes (camera planes + projection for depth-based effects).
 */
struct CameraEffectContext {
    float cameraNear = 0.1f;
    float cameraFar = 1000.0f;
    bool cameraPerspective = true;
    math::Mat4 projection = math::Mat4::Identity();
    math::Mat4 invProjection = math::Mat4::Identity();
};

/**
 * CameraEffect — base class for a single stackable post-effect attached to a camera node
 * (Camera2D / Camera3D / CameraUI). Multiple effects on one camera form an ordered stack
 * (child/declaration order) that the renderer applies after the view's WorldEnvironment
 * post-processing, ping-ponged between HDR targets.
 *
 * An effect is one of two shapes:
 *  - Pointwise / composable (IsComposable() == true): contributes a small GLSL snippet via
 *    BuildCompose(). Consecutive composable effects fold into one pass in Composite mode
 *    (and each becomes its own one-draw pass in Separate mode). Examples: ColorGrade,
 *    Vignette, FilmGrain, Tonemap, ColorInvert, Posterize, HueShift.
 *  - Multi-tap (IsComposable() == false): authors one or more full standalone passes via
 *    BuildPasses() because it samples neighbouring texels (and so cannot fold). Examples:
 *    Blur, Glow, Outline, Pixelate, Sharpen, ChromaticAberration.
 *
 * The on/off toggle is the base Component `Enabled` flag — a disabled effect is skipped by
 * the gather. All parameters are plain registered properties, so they are editable in the
 * inspector and get/set-able from every scripting language with no per-effect binding code.
 */
class CameraEffect : public core::Component {
public:
    CameraEffect() = default;
    ~CameraEffect() override = default;

    std::string GetTypeName() const override { return "CameraEffect"; }

    /** True for pointwise effects that fold a snippet (BuildCompose); false for multi-tap. */
    virtual bool IsComposable() const { return false; }

    /** Composable effects fill `out` with their snippet contribution (uses the "%S%" token). */
    virtual void BuildCompose(CameraComposeContribution& out) const { (void)out; }

    /** Multi-tap effects append one or more standalone passes. */
    virtual void BuildPasses(const CameraEffectContext& ctx,
                             std::vector<CameraEffectPass>& out) const {
        (void)ctx; (void)out;
    }
};

} // namespace components
} // namespace lupine
