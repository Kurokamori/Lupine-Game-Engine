#pragma once

#include "ResourceHandles.hpp"
#include "gfx/GfxTypes.hpp"
#include "gfx/GfxDescriptors.hpp"
#include "PostProcessStack.hpp"   // FlipYMode
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

/**
 * How a camera's stacked effects are composited.
 *
 * Separate  - every effect runs as its own full-screen pass, ping-ponged between
 *             HDR targets in exact stack order. Each effect samples the previous
 *             effect's output, so effects truly chain. Most flexible; one draw per effect.
 * Composite - consecutive pointwise (single-tap) effects are folded into a single
 *             generated shader pass (fewer draws); multi-tap effects (blur/glow/
 *             outline/...) still run as their own passes. Cheaper, mathematically
 *             identical for the folded effects.
 */
enum class CameraEffectRenderMode : int {
    Separate = 0,
    Composite = 1
};

/**
 * A single named uniform value to push to a pass. The renderer dispatches by kind
 * to the matching IGfxCommandList::setUniform* call. Names containing the slot token
 * "%S%" are resolved to the effect's slot index when a composite group is assembled.
 */
struct CameraEffectUniform {
    enum class Kind : int { Float, Int, Vec2, Vec3, Vec4, Mat4 };

    Kind kind = Kind::Float;
    std::string name;
    float fValue = 0.0f;
    int iValue = 0;
    math::Vec2 v2Value;
    math::Vec3 v3Value;
    math::Vec4 v4Value;
    math::Mat4 m4Value = math::Mat4::Identity();

    static CameraEffectUniform Float(const std::string& n, float v) {
        CameraEffectUniform u; u.kind = Kind::Float; u.name = n; u.fValue = v; return u;
    }
    static CameraEffectUniform Int(const std::string& n, int v) {
        CameraEffectUniform u; u.kind = Kind::Int; u.name = n; u.iValue = v; return u;
    }
    static CameraEffectUniform Vec2(const std::string& n, const math::Vec2& v) {
        CameraEffectUniform u; u.kind = Kind::Vec2; u.name = n; u.v2Value = v; return u;
    }
    static CameraEffectUniform Vec3(const std::string& n, const math::Vec3& v) {
        CameraEffectUniform u; u.kind = Kind::Vec3; u.name = n; u.v3Value = v; return u;
    }
    static CameraEffectUniform Vec4(const std::string& n, const math::Vec4& v) {
        CameraEffectUniform u; u.kind = Kind::Vec4; u.name = n; u.v4Value = v; return u;
    }
    static CameraEffectUniform Mat4(const std::string& n, const math::Mat4& v) {
        CameraEffectUniform u; u.kind = Kind::Mat4; u.name = n; u.m4Value = v; return u;
    }
};

/**
 * A pointwise effect's contribution to a composite pass. Provided by composable
 * effects (color grade, vignette, grain, ...). Templates use the token "%S%" wherever
 * the effect's slot index belongs; the renderer substitutes it so multiple effects
 * (and duplicates of one effect) coexist in a single generated shader without name
 * collisions. The apply function MUST be named fx%S%_apply and have the signature
 *   vec3 fx%S%_apply(vec3 color, vec2 uv)
 * Any helper functions it declares must also be prefixed with fx%S%_.
 */
struct CameraComposeContribution {
    std::string id;                  // stable identifier (cache key component), e.g. "colorgrade"
    std::string uniformDecls;        // .lsh property declarations (uniform <type> u_fx%S%_*;)
    std::string applyFunction;       // defines vec3 fx%S%_apply(vec3, vec2) (+ helpers)
    std::vector<CameraEffectUniform> uniforms;  // names may contain "%S%"
};

/**
 * A standalone full-screen pass (multi-tap effect). The shader samples u_SceneTex at
 * input slot 0 (and, when needsDepth, u_DepthTex at slot 1) and writes the transformed
 * color. The renderer owns u_FlipV / u_SceneTex / u_DepthTex; everything else the effect
 * declares is supplied via uniforms.
 */
struct CameraEffectPass {
    std::string cacheKey;            // stable per shader source (pipeline cache key)
    std::string lshSource;           // complete .lsh source for this pass
    std::vector<CameraEffectUniform> uniforms;
    bool needsDepth = false;
    bool pointSampleScene = false;   // bind u_SceneTex with a point sampler
};

/**
 * One effect resolved to renderer-friendly data. Composable effects fill `compose`;
 * multi-tap effects fill `passes`. Built by the component layer in
 * RenderWorld::gatherCameraEffects and consumed here with no knowledge of components.
 */
struct ResolvedCameraEffect {
    bool composable = false;
    CameraComposeContribution compose;
    std::vector<CameraEffectPass> passes;
};

/**
 * The full, resolved per-camera effect chain for one view in stack order, plus the
 * renderer-supplied frame context (time, camera planes, projection for depth effects).
 */
struct CameraEffectChain {
    std::vector<ResolvedCameraEffect> effects;
    CameraEffectRenderMode mode = CameraEffectRenderMode::Separate;
    FlipYMode flipY = FlipYMode::Auto;

    float time = 0.0f;
    float cameraNear = 0.1f;
    float cameraFar = 1000.0f;
    bool cameraPerspective = true;

    // True when the camera clears color (owns/fills the target): the final pass overwrites
    // it, preserving the layer's own alpha. False for an overlay camera (e.g. a UI camera
    // composited over an earlier 2D/3D pass): the final pass alpha-blends onto existing content.
    bool cameraClearsColor = true;
    math::Mat4 projection = math::Mat4::Identity();
    math::Mat4 invProjection = math::Mat4::Identity();

    bool empty() const { return effects.empty(); }
};

/**
 * CameraEffectStack — backend-agnostic executor for a camera's stacked effects.
 *
 * Owned by RenderWorld. Runs an ordered chain of full-screen passes that read a view's
 * (already-rendered, possibly post-processed) HDR color and write the final image to the
 * view's real target. Like PostProcessStack it talks only to IGfxDevice/IGfxCommandList
 * and translates embedded/generated .lsh at runtime, so it works on every backend.
 *
 * Pointwise effects are assembled through a single composite-shader generator: a run of
 * one snippet (Separate mode) or many snippets (Composite mode) produces one pass. Multi-
 * tap effects render as their own passes. Intermediate results ping-pong between two
 * full-resolution HDR targets; the final pass writes the real target.
 */
class CameraEffectStack {
public:
    CameraEffectStack() = default;
    ~CameraEffectStack();

    CameraEffectStack(const CameraEffectStack&) = delete;
    CameraEffectStack& operator=(const CameraEffectStack&) = delete;

    bool initialize(IGfxDevice* device);
    void shutdown();

    bool isActive(const CameraEffectChain& chain) const { return !chain.empty(); }

    /**
     * Run the effect chain.
     * @param finalTarget Where the final image is written (view's backbuffer/RT).
     * @param sceneColor  Input HDR color (scene, or PostProcessStack output).
     * @param sceneDepth  Scene depth (for depth-based effects; may be invalid otherwise).
     * @param width,height Pixel dimensions of the view.
     * @param chain       Resolved, ordered effect chain + frame context.
     */
    void execute(RenderTargetHandle finalTarget,
                 TextureHandle sceneColor,
                 TextureHandle sceneDepth,
                 uint32_t width,
                 uint32_t height,
                 const CameraEffectChain& chain);

private:
    struct Pipeline {
        ShaderHandle vertex;
        ShaderHandle fragment;
        PipelineHandle pipeline;
        bool valid() const { return pipeline.isValid(); }
    };

    struct ColorTarget {
        RenderTargetHandle target;
        TextureHandle color;
        uint32_t width = 0;
        uint32_t height = 0;
        bool valid() const { return target.isValid(); }
    };

    // A single concrete render step, derived from the chain + mode.
    struct RenderStep {
        std::string cacheKey;
        std::string lshSource;
        std::vector<CameraEffectUniform> uniforms;
        bool needsDepth = false;
        bool pointSampleScene = false;
        bool usesTime = false;
    };

    // ----- Pipeline / target management -----
    // alphaBlend selects straight-alpha "over" blending (used for the final pass so the
    // camera's layer composites onto earlier cameras) vs opaque overwrite (intermediates).
    Pipeline* getPipeline(const std::string& key, const std::string& lshSource, bool alphaBlend);
    void destroyPipelines();
    void ensureTargets(uint32_t width, uint32_t height);
    void releaseTargets();
    ColorTarget createColorTarget(uint32_t width, uint32_t height, TextureFormat format);
    void destroyColorTarget(ColorTarget& t);

    // ----- Mesh / helpers -----
    bool ensureFullscreenMesh();
    void drawFullscreen(IGfxCommandList* cmd);
    void bindInputTexture(IGfxCommandList* cmd, const char* uniformName, TextureHandle tex,
                          uint32_t index, bool pointSampler);
    uint32_t textureSlot(uint32_t index) const;
    bool resolveFlipV(FlipYMode mode) const;
    void applyUniform(IGfxCommandList* cmd, const CameraEffectUniform& u);

    // ----- Step assembly / execution -----
    void buildSteps(const CameraEffectChain& chain, std::vector<RenderStep>& out);
    RenderStep buildCompositeStep(const std::vector<const CameraComposeContribution*>& group);
    void runStep(const RenderStep& step, RenderTargetHandle dst, TextureHandle srcColor,
                 TextureHandle srcDepth, uint32_t width, uint32_t height,
                 const CameraEffectChain& chain, bool toFinal);

    IGfxDevice* m_device = nullptr;
    GraphicsBackend m_backend = GraphicsBackend::None;
    bool m_initialized = false;

    MeshHandle m_fullscreenMesh;
    SamplerHandle m_linearClamp;
    SamplerHandle m_pointClamp;

    std::unordered_map<std::string, Pipeline> m_pipelines;

    uint32_t m_width = 0;
    uint32_t m_height = 0;
    ColorTarget m_pingA;
    ColorTarget m_pingB;
};

} // namespace lupine
