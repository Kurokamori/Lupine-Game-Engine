#pragma once

#include "ResourceHandles.hpp"
#include "gfx/GfxTypes.hpp"
#include "gfx/GfxDescriptors.hpp"
#include "lupine/math/Vec2.hpp"
#include "lupine/math/Vec3.hpp"
#include "lupine/math/Vec4.hpp"
#include "lupine/math/Mat4.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace lupine {

class IGfxDevice;
class IGfxCommandList;
class RenderView;
class RenderCamera;

/**
 * Tonemapping operator applied at the end of the post-process chain.
 * Linear is a pass-through (no tonemapping), suitable for content authored in LDR
 * (e.g. pure-2D games) that should not be remapped.
 */
enum class TonemapMode : int {
    Linear = 0,            // No tonemapping (clamp only)
    Reinhard = 1,          // Simple Reinhard
    ReinhardExtended = 2,  // Reinhard with white point
    ACES = 3,              // ACES filmic approximation (Narkowicz)
    Filmic = 4,            // Uncharted 2 filmic (Hejl/Burgess)
    AGX = 5                // AgX (minimal, neutral look)
};

/**
 * Blend mode used to composite the overlay texture (dirt/grunge/scanlines/etc.)
 * over the graded image.
 */
enum class OverlayBlendMode : int {
    Normal = 0,
    Additive = 1,
    Multiply = 2,
    Screen = 3,
    Overlay = 4,
    SoftLight = 5
};

/**
 * Override for the scene-texture vertical flip. The chain bakes the correct default
 * per backend (top-left-origin backends flip), but an explicit override is exposed for
 * the rare backend/driver mismatch so it can be corrected without a rebuild.
 */
enum class FlipYMode : int {
    Auto = 0,
    Off = 1,
    On = 2
};

/**
 * Fully-resolved post-processing parameters for a single view, populated from the
 * active WorldEnvironment each frame (and augmented by the renderer with camera /
 * timing / texture-handle data the WorldEnvironment does not own).
 *
 * The chain runs in a fixed, well-defined order:
 *   SSAO -> Bloom -> Composite(exposure, tonemap, color-grading, vignette,
 *                              chromatic aberration, film grain, overlay)
 * Each effect is independently toggled; an all-disabled settings block makes
 * PostProcessStack::isActive() return false so the renderer takes the cheap direct path.
 */
struct PostProcessSettings {
    bool enabled = false;

    // ----- Tonemapping & exposure -----
    TonemapMode tonemap = TonemapMode::Linear;
    float exposure = 1.0f;     // linear multiplier applied before tonemapping
    float whitePoint = 4.0f;   // white point for ReinhardExtended

    // ----- Bloom -----
    bool  bloomEnabled = false;
    float bloomThreshold = 1.0f;   // luminance threshold for the bright-pass
    float bloomSoftKnee = 0.5f;    // 0 = hard threshold, 1 = soft knee
    float bloomIntensity = 0.6f;   // how much bloom is added back
    int   bloomIterations = 6;     // number of downsample levels (mip chain depth)

    // ----- SSAO (3D / perspective views only) -----
    bool  ssaoEnabled = false;
    float ssaoRadius = 0.5f;       // world-space sampling radius
    float ssaoIntensity = 1.0f;    // occlusion strength
    float ssaoBias = 0.025f;       // depth bias to avoid self-occlusion
    int   ssaoSamples = 24;        // hemisphere samples per pixel
    float ssaoPower = 1.5f;        // contrast power on the AO term

    // ----- Color grading -----
    bool  colorGradingEnabled = false;
    float contrast = 1.0f;         // around mid-grey
    float saturation = 1.0f;
    float brightness = 0.0f;       // additive
    float temperature = 0.0f;      // -1 cool .. +1 warm
    float tint = 0.0f;             // -1 green .. +1 magenta
    math::Vec4 colorFilter = math::Vec4(1.0f, 1.0f, 1.0f, 1.0f); // multiplicative tint (rgb), a = strength
    math::Vec3 lift = math::Vec3(0.0f, 0.0f, 0.0f);   // shadows
    math::Vec3 gamma = math::Vec3(1.0f, 1.0f, 1.0f);  // midtones
    math::Vec3 gain = math::Vec3(1.0f, 1.0f, 1.0f);   // highlights

    // ----- Vignette -----
    bool  vignetteEnabled = false;
    math::Vec4 vignetteColor = math::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
    float vignetteIntensity = 0.4f;
    float vignetteSmoothness = 0.5f;
    float vignetteRoundness = 1.0f;   // 0 = rectangular, 1 = circular
    math::Vec2 vignetteCenter = math::Vec2(0.5f, 0.5f);

    // ----- Chromatic aberration -----
    bool  chromaticAberrationEnabled = false;
    float chromaticAberrationAmount = 0.004f; // max channel offset (UV units) at the edges

    // ----- Film grain -----
    bool  grainEnabled = false;
    float grainIntensity = 0.08f;
    float grainSize = 1.0f;

    // ----- Overlay texture (dirt / vignette art / scanlines / etc.) -----
    OverlayBlendMode overlayBlend = OverlayBlendMode::Normal;
    float overlayOpacity = 1.0f;

    // ----- Backend / orientation -----
    FlipYMode flipY = FlipYMode::Auto;

    // ===== Renderer-supplied (not from WorldEnvironment) =====
    TextureHandle overlayTexture;        // resolved overlay image, if any
    float time = 0.0f;                   // seconds, for animated grain
    float cameraNear = 0.1f;
    float cameraFar = 1000.0f;
    bool  cameraPerspective = true;      // SSAO is skipped for orthographic/2D views
    math::Mat4 projection = math::Mat4::Identity();
    math::Mat4 invProjection = math::Mat4::Identity();

    /** True if at least one effect that actually changes the image is enabled. */
    bool anyEffectActive() const;
};

/**
 * PostProcessStack — a backend-agnostic, composable post-processing renderer.
 *
 * Owned by RenderWorld. Renders a fixed chain of full-screen passes that read the
 * HDR scene color (and depth) produced for a view and write the final, graded image
 * to the view's real output target (swapchain backbuffer or off-screen render target).
 *
 * Because it talks only to IGfxDevice / IGfxCommandList and translates its shaders
 * from embedded .lsh source at runtime, it works on every backend (OpenGL, Vulkan,
 * DirectX 11/12, Metal, WebGL) without any per-backend code.
 *
 * Per-SubViewport post-processing is automatic: each view gathers its own
 * WorldEnvironment, so a SubViewport subtree with its own WorldEnvironment gets its
 * own independent chain when RenderWorld renders that view.
 */
class PostProcessStack {
public:
    PostProcessStack() = default;
    ~PostProcessStack();

    PostProcessStack(const PostProcessStack&) = delete;
    PostProcessStack& operator=(const PostProcessStack&) = delete;

    /** Initialize with the active graphics device. Creates the fullscreen mesh + samplers. */
    bool initialize(IGfxDevice* device);

    /** Release all GPU resources (pipelines, targets, mesh, samplers). */
    void shutdown();

    /** Whether the settings request any work at all. */
    bool isActive(const PostProcessSettings& settings) const {
        return settings.enabled && settings.anyEffectActive();
    }

    /**
     * Run the post-process chain.
     *
     * @param finalTarget  Where the final composited image is written (the view's real backbuffer/RT).
     * @param sceneColor   HDR scene color texture (input).
     * @param sceneDepth   Scene depth texture (input; required for SSAO, may be invalid otherwise).
     * @param width,height Pixel dimensions of the view being processed.
     * @param settings     Fully-resolved effect parameters.
     */
    void execute(RenderTargetHandle finalTarget,
                 TextureHandle sceneColor,
                 TextureHandle sceneDepth,
                 uint32_t width,
                 uint32_t height,
                 const PostProcessSettings& settings);

private:
    // A compiled full-screen pipeline (shaders + pipeline object) for one effect shader.
    struct PostPipeline {
        ShaderHandle vertex;
        ShaderHandle fragment;
        PipelineHandle pipeline;
        bool valid() const { return pipeline.isValid(); }
    };

    // A color-only off-screen target plus its color texture and size.
    struct ColorTarget {
        RenderTargetHandle target;
        TextureHandle color;
        uint32_t width = 0;
        uint32_t height = 0;
        bool valid() const { return target.isValid(); }
    };

    // ----- Pipeline management -----
    PostPipeline* getPipeline(const std::string& name, const char* lshSource, const BlendState& blend);
    void destroyPipelines();

    // ----- Target management -----
    void ensureTargets(uint32_t width, uint32_t height, const PostProcessSettings& settings);
    void releaseTargets();
    ColorTarget createColorTarget(uint32_t width, uint32_t height, TextureFormat format);
    void destroyColorTarget(ColorTarget& t);

    // ----- Mesh / samplers -----
    bool ensureFullscreenMesh();
    void drawFullscreen(IGfxCommandList* cmd);

    // ----- Per-pass helpers -----
    void bindInputTexture(IGfxCommandList* cmd, const char* uniformName, TextureHandle tex, uint32_t index, bool pointSampler);
    uint32_t textureSlot(uint32_t index) const;   // backend texture binding for the Nth input
    bool resolveFlipV(const PostProcessSettings& settings) const;

    // ----- Sub-stages -----
    void runSSAO(TextureHandle sceneDepth, uint32_t width, uint32_t height, const PostProcessSettings& s);
    void runBloom(TextureHandle sceneColor, uint32_t width, uint32_t height, const PostProcessSettings& s);
    void runComposite(RenderTargetHandle finalTarget,
                      TextureHandle sceneColor,
                      uint32_t width, uint32_t height,
                      const PostProcessSettings& s);

    IGfxDevice* m_device = nullptr;
    GraphicsBackend m_backend = GraphicsBackend::None;
    bool m_initialized = false;

    // Full-screen triangle/quad geometry (clip-space positions, backend-correct UVs).
    MeshHandle m_fullscreenMesh;

    // Samplers: linear-clamp for color sampling, point-clamp for depth / exact reads.
    SamplerHandle m_linearClamp;
    SamplerHandle m_pointClamp;

    // Compiled pipelines, keyed by effect name.
    std::unordered_map<std::string, PostPipeline> m_pipelines;

    // Per-size targets (recreated when the view size changes).
    uint32_t m_width = 0;
    uint32_t m_height = 0;

    ColorTarget m_pingA;   // full-res HDR ping
    ColorTarget m_pingB;   // full-res HDR pong
    ColorTarget m_ssao;    // full-res AO (R/RGBA16F)
    ColorTarget m_ssaoBlur;
    std::vector<ColorTarget> m_bloomChain;  // half, quarter, ... (downsample + accumulate)
    bool m_ssaoTargetsBuilt = false;
    bool m_bloomTargetsBuilt = false;
    int  m_bloomLevelsBuilt = 0;

    // Result texture of the last SSAO/bloom sub-stage, consumed by the composite.
    TextureHandle m_ssaoResult;
    TextureHandle m_bloomResult;
};

} // namespace lupine
