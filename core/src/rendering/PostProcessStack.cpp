#include "lupine/rendering/PostProcessStack.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/rendering/gfx/GfxCommandList.hpp"
#include "lupine/rendering/gfx/GfxDescriptors.hpp"
#include "lupine/rendering/ShaderTranslator.hpp"
#include "lupine/rendering/Mesh.hpp"
#include "lupine/logger/Logger.hpp"

#include <algorithm>
#include <cstddef>

namespace lupine {

// ============================================================================
// Embedded post-process shaders (.lsh source, translated to the active backend
// at runtime). Kept self-contained so the stack has no asset-file dependency and
// works identically in editor, runtime, and packed builds on every backend.
// ============================================================================
namespace {

// Shared full-screen vertex stage. a_Position holds clip-space coordinates directly
// (the quad spans [-1,1]); u_FlipV selects the texture-V convention so that a pure
// pass-through preserves orientation on both bottom-left (GL/WebGL) and top-left
// (Vulkan/DX/Metal) origin backends.
constexpr const char* kFullscreenVertex = R"LSH(
#begin vertex
    #input vec3 a_Position : POSITION 0
    #input vec3 a_Normal : NORMAL 1
    #input vec2 a_TexCoord : TEXCOORD0 2
    #input vec4 a_Color : COLOR 3

    #output vec2 v_TexCoord

    void main() {
        vec2 uv = a_Position.xy * 0.5 + 0.5;
        uv.y = mix(uv.y, 1.0 - uv.y, u_FlipV);
        v_TexCoord = uv;
        VERTEX_OUTPUT = vec4(a_Position.x, a_Position.y, 0.0, 1.0);
    }
#end vertex
)LSH";

// ---- Bloom bright-pass + half-resolution downsample ----
const std::string kBloomPrefilterSource = std::string(R"LSH(
#shader "PostBloomPrefilter"
#description "Bloom bright-pass with soft knee + 4-tap half-res downsample."

#begin properties
    uniform float u_FlipV = 0.0;
    uniform vec2 u_TexelSize = vec2(0.001, 0.001);
    uniform float u_Threshold = 1.0;
    uniform float u_SoftKnee = 0.5;
    uniform sampler2D u_SceneTex;
#end properties
)LSH") + kFullscreenVertex + R"LSH(
#begin fragment
    #output vec4 FragColor

    vec3 prefilter(vec3 c) {
        float br = max(c.r, max(c.g, c.b));
        float knee = u_Threshold * u_SoftKnee + 0.0001;
        float soft = br - u_Threshold + knee;
        soft = clamp(soft, 0.0, 2.0 * knee);
        soft = soft * soft / (4.0 * knee + 0.0001);
        float contrib = max(soft, br - u_Threshold);
        contrib = contrib / max(br, 0.0001);
        return c * contrib;
    }

    void main() {
        vec2 t = u_TexelSize;
        vec3 a = SAMPLE(u_SceneTex, v_TexCoord + vec2(-t.x, -t.y)).rgb;
        vec3 b = SAMPLE(u_SceneTex, v_TexCoord + vec2( t.x, -t.y)).rgb;
        vec3 c = SAMPLE(u_SceneTex, v_TexCoord + vec2(-t.x,  t.y)).rgb;
        vec3 d = SAMPLE(u_SceneTex, v_TexCoord + vec2( t.x,  t.y)).rgb;
        vec3 col = (a + b + c + d) * 0.25;
        FragColor = vec4(prefilter(col), 1.0);
    }
#end fragment
)LSH";

// ---- Progressive downsample (Call of Duty / Next-gen 13-tap) ----
const std::string kBloomDownsampleSource = std::string(R"LSH(
#shader "PostBloomDownsample"
#description "13-tap bloom downsample filter."

#begin properties
    uniform float u_FlipV = 0.0;
    uniform vec2 u_TexelSize = vec2(0.001, 0.001);
    uniform sampler2D u_SourceTex;
#end properties
)LSH") + kFullscreenVertex + R"LSH(
#begin fragment
    #output vec4 FragColor

    void main() {
        vec2 t = u_TexelSize;
        vec3 a = SAMPLE(u_SourceTex, v_TexCoord + vec2(-2.0 * t.x,  2.0 * t.y)).rgb;
        vec3 b = SAMPLE(u_SourceTex, v_TexCoord + vec2( 0.0,        2.0 * t.y)).rgb;
        vec3 c = SAMPLE(u_SourceTex, v_TexCoord + vec2( 2.0 * t.x,  2.0 * t.y)).rgb;
        vec3 d = SAMPLE(u_SourceTex, v_TexCoord + vec2(-2.0 * t.x,  0.0)).rgb;
        vec3 e = SAMPLE(u_SourceTex, v_TexCoord).rgb;
        vec3 f = SAMPLE(u_SourceTex, v_TexCoord + vec2( 2.0 * t.x,  0.0)).rgb;
        vec3 g = SAMPLE(u_SourceTex, v_TexCoord + vec2(-2.0 * t.x, -2.0 * t.y)).rgb;
        vec3 h = SAMPLE(u_SourceTex, v_TexCoord + vec2( 0.0,       -2.0 * t.y)).rgb;
        vec3 i = SAMPLE(u_SourceTex, v_TexCoord + vec2( 2.0 * t.x, -2.0 * t.y)).rgb;
        vec3 j = SAMPLE(u_SourceTex, v_TexCoord + vec2(-1.0 * t.x,  1.0 * t.y)).rgb;
        vec3 k = SAMPLE(u_SourceTex, v_TexCoord + vec2( 1.0 * t.x,  1.0 * t.y)).rgb;
        vec3 l = SAMPLE(u_SourceTex, v_TexCoord + vec2(-1.0 * t.x, -1.0 * t.y)).rgb;
        vec3 m = SAMPLE(u_SourceTex, v_TexCoord + vec2( 1.0 * t.x, -1.0 * t.y)).rgb;

        vec3 result = e * 0.125;
        result = result + (a + c + g + i) * 0.03125;
        result = result + (b + d + f + h) * 0.0625;
        result = result + (j + k + l + m) * 0.125;
        FragColor = vec4(result, 1.0);
    }
#end fragment
)LSH";

// ---- Progressive upsample (9-tap tent). Rendered with additive blend so it
//      accumulates onto the already-downsampled content of the target level. ----
const std::string kBloomUpsampleSource = std::string(R"LSH(
#shader "PostBloomUpsample"
#description "9-tap tent bloom upsample (additive accumulation)."

#begin properties
    uniform float u_FlipV = 0.0;
    uniform vec2 u_TexelSize = vec2(0.001, 0.001);
    uniform float u_Radius = 1.0;
    uniform sampler2D u_SourceTex;
#end properties
)LSH") + kFullscreenVertex + R"LSH(
#begin fragment
    #output vec4 FragColor

    void main() {
        vec2 t = u_TexelSize * u_Radius;
        vec3 a = SAMPLE(u_SourceTex, v_TexCoord + vec2(-t.x,  t.y)).rgb;
        vec3 b = SAMPLE(u_SourceTex, v_TexCoord + vec2( 0.0,  t.y)).rgb;
        vec3 c = SAMPLE(u_SourceTex, v_TexCoord + vec2( t.x,  t.y)).rgb;
        vec3 d = SAMPLE(u_SourceTex, v_TexCoord + vec2(-t.x,  0.0)).rgb;
        vec3 e = SAMPLE(u_SourceTex, v_TexCoord).rgb;
        vec3 f = SAMPLE(u_SourceTex, v_TexCoord + vec2( t.x,  0.0)).rgb;
        vec3 g = SAMPLE(u_SourceTex, v_TexCoord + vec2(-t.x, -t.y)).rgb;
        vec3 h = SAMPLE(u_SourceTex, v_TexCoord + vec2( 0.0, -t.y)).rgb;
        vec3 i = SAMPLE(u_SourceTex, v_TexCoord + vec2( t.x, -t.y)).rgb;

        vec3 result = e * 4.0;
        result = result + (b + d + f + h) * 2.0;
        result = result + (a + c + g + i);
        result = result * 0.0625;
        FragColor = vec4(result, 1.0);
    }
#end fragment
)LSH";

// ---- SSAO (depth-only, normals reconstructed from depth) ----
const std::string kSSAOSource = std::string(R"LSH(
#shader "PostSSAO"
#description "Screen-space ambient occlusion from depth with reconstructed normals."

#begin properties
    uniform float u_FlipV = 0.0;
    uniform vec2 u_TexelSize = vec2(0.001, 0.001);
    uniform mat4 u_Projection = mat4(1.0);
    uniform mat4 u_InvProjection = mat4(1.0);
    uniform float u_Radius = 0.5;
    uniform float u_Bias = 0.025;
    uniform float u_Intensity = 1.0;
    uniform float u_Power = 1.5;
    uniform float u_SampleCount = 24.0;
    uniform float u_DepthZeroToOne = 1.0;
    uniform sampler2D u_DepthTex;
#end properties
)LSH") + kFullscreenVertex + R"LSH(
#begin fragment
    #output vec4 FragColor

    float hash12(vec2 p) {
        return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
    }

    vec3 viewPosFromUV(vec2 uv) {
        float d = SAMPLE(u_DepthTex, uv).r;
        float ndcZ = mix(d * 2.0 - 1.0, d, u_DepthZeroToOne);
        vec4 clip = vec4(uv * 2.0 - 1.0, ndcZ, 1.0);
        vec4 v = MUL(u_InvProjection, clip);
        return v.xyz / v.w;
    }

    void main() {
        float centerDepth = SAMPLE(u_DepthTex, v_TexCoord).r;
        if (centerDepth >= 0.99999) {
            FragColor = vec4(1.0, 1.0, 1.0, 1.0);
            return;
        }

        vec3 P = viewPosFromUV(v_TexCoord);
        vec3 Px = viewPosFromUV(v_TexCoord + vec2(u_TexelSize.x, 0.0));
        vec3 Py = viewPosFromUV(v_TexCoord + vec2(0.0, u_TexelSize.y));
        vec3 N = normalize(cross(Px - P, Py - P));

        float ra = hash12(v_TexCoord * 137.13) * 6.2831853;
        vec3 randVec = vec3(cos(ra), sin(ra), 0.0);
        vec3 T = normalize(randVec - N * dot(randVec, N));
        vec3 B = cross(N, T);

        float occlusion = 0.0;
        float taken = 0.0;
        for (int s = 0; s < 64; s = s + 1) {
            if (float(s) >= u_SampleCount) {
                break;
            }
            float fs = float(s);
            float a1 = hash12(v_TexCoord + vec2(fs * 0.37, fs * 0.71)) * 6.2831853;
            float r1 = hash12(v_TexCoord + vec2(fs * 1.13, fs * 0.19));
            float r2 = hash12(v_TexCoord + vec2(fs * 0.53, fs * 1.77));

            vec2 disk = vec2(cos(a1), sin(a1)) * sqrt(r1);
            float zc = 0.15 + 0.85 * r2;
            vec3 sampleVec = T * disk.x + B * disk.y + N * zc;

            float scale = mix(0.1, 1.0, (fs / max(u_SampleCount, 1.0)) * (fs / max(u_SampleCount, 1.0)));
            vec3 samplePos = P + sampleVec * u_Radius * scale;

            vec4 offset = MUL(u_Projection, vec4(samplePos, 1.0));
            offset.xyz = offset.xyz / offset.w;
            vec2 suv = offset.xy * 0.5 + 0.5;
            suv.y = mix(suv.y, 1.0 - suv.y, u_FlipV);

            float sampleSceneZ = viewPosFromUV(suv).z;
            float rangeCheck = u_Radius / max(abs(P.z - sampleSceneZ), 0.0001);
            rangeCheck = clamp(rangeCheck, 0.0, 1.0);
            float occluded = (sampleSceneZ >= samplePos.z + u_Bias) ? 1.0 : 0.0;
            occlusion = occlusion + occluded * rangeCheck;
            taken = taken + 1.0;
        }

        float ao = 1.0 - (occlusion / max(taken, 1.0)) * u_Intensity;
        ao = clamp(ao, 0.0, 1.0);
        ao = pow(ao, u_Power);
        FragColor = vec4(ao, ao, ao, 1.0);
    }
#end fragment
)LSH";

// ---- SSAO blur (4x4 box denoise) ----
const std::string kSSAOBlurSource = std::string(R"LSH(
#shader "PostSSAOBlur"
#description "4x4 box blur to denoise the raw SSAO buffer."

#begin properties
    uniform float u_FlipV = 0.0;
    uniform vec2 u_TexelSize = vec2(0.001, 0.001);
    uniform sampler2D u_AOTex;
#end properties
)LSH") + kFullscreenVertex + R"LSH(
#begin fragment
    #output vec4 FragColor

    void main() {
        float result = 0.0;
        for (int x = -2; x < 2; x = x + 1) {
            for (int y = -2; y < 2; y = y + 1) {
                vec2 off = vec2(float(x), float(y)) * u_TexelSize;
                result = result + SAMPLE(u_AOTex, v_TexCoord + off).r;
            }
        }
        result = result / 16.0;
        FragColor = vec4(result, result, result, 1.0);
    }
#end fragment
)LSH";

// ---- Final composite: SSAO modulation, bloom add, exposure, tonemap,
//      color grading, vignette, chromatic aberration, film grain, overlay. ----
const std::string kCompositeSource = std::string(R"LSH(
#shader "PostComposite"
#description "Post-process composite: bloom, tonemap, color grading, vignette, grain, overlay."

#begin properties
    uniform float u_FlipV = 0.0;
    uniform vec2 u_Resolution = vec2(1920.0, 1080.0);
    uniform float u_Time = 0.0;

    uniform float u_Exposure = 1.0;
    uniform float u_TonemapMode = 0.0;
    uniform float u_WhitePoint = 4.0;

    uniform float u_BloomEnabled = 0.0;
    uniform float u_BloomIntensity = 0.6;

    uniform float u_SsaoEnabled = 0.0;

    uniform float u_GradingEnabled = 0.0;
    uniform float u_Contrast = 1.0;
    uniform float u_Saturation = 1.0;
    uniform float u_Brightness = 0.0;
    uniform float u_Temperature = 0.0;
    uniform float u_Tint = 0.0;
    uniform vec4 u_ColorFilter = vec4(1.0, 1.0, 1.0, 1.0);
    uniform vec3 u_Lift = vec3(0.0, 0.0, 0.0);
    uniform vec3 u_Gamma = vec3(1.0, 1.0, 1.0);
    uniform vec3 u_Gain = vec3(1.0, 1.0, 1.0);

    uniform float u_VignetteEnabled = 0.0;
    uniform vec4 u_VignetteColor = vec4(0.0, 0.0, 0.0, 1.0);
    uniform float u_VignetteIntensity = 0.4;
    uniform float u_VignetteSmoothness = 0.5;
    uniform float u_VignetteRoundness = 1.0;
    uniform vec2 u_VignetteCenter = vec2(0.5, 0.5);

    uniform float u_CAEnabled = 0.0;
    uniform float u_CAAmount = 0.004;

    uniform float u_GrainEnabled = 0.0;
    uniform float u_GrainIntensity = 0.08;
    uniform float u_GrainSize = 1.0;

    uniform float u_OverlayEnabled = 0.0;
    uniform float u_OverlayBlend = 0.0;
    uniform float u_OverlayOpacity = 1.0;

    uniform sampler2D u_SceneTex;
    uniform sampler2D u_BloomTex;
    uniform sampler2D u_SsaoTex;
    uniform sampler2D u_OverlayTex;
#end properties
)LSH") + kFullscreenVertex + R"LSH(
#begin fragment
    #output vec4 FragColor

    vec3 tonemapReinhard(vec3 c) {
        return c / (c + vec3(1.0, 1.0, 1.0));
    }
    vec3 tonemapReinhardExt(vec3 c, float w) {
        vec3 num = c * (1.0 + c / vec3(w * w, w * w, w * w));
        return num / (1.0 + c);
    }
    vec3 tonemapACES(vec3 x) {
        float a = 2.51;
        float b = 0.03;
        float cc = 2.43;
        float d = 0.59;
        float e = 0.14;
        vec3 num = x * (a * x + vec3(b, b, b));
        vec3 den = x * (cc * x + vec3(d, d, d)) + vec3(e, e, e);
        return clamp(num / den, 0.0, 1.0);
    }
    vec3 tonemapFilmic(vec3 x) {
        vec3 X = max(vec3(0.0, 0.0, 0.0), x - vec3(0.004, 0.004, 0.004));
        vec3 result = (X * (6.2 * X + vec3(0.5, 0.5, 0.5))) / (X * (6.2 * X + vec3(1.7, 1.7, 1.7)) + vec3(0.06, 0.06, 0.06));
        return result;
    }
    vec3 tonemapAGX(vec3 c) {
        c = max(c, vec3(0.0, 0.0, 0.0));
        vec3 m = c / (c + vec3(0.155, 0.155, 0.155)) * 1.019;
        return clamp(m, 0.0, 1.0);
    }

    vec3 applyTonemap(vec3 c) {
        if (u_TonemapMode < 0.5) {
            return clamp(c, 0.0, 1.0);
        } else if (u_TonemapMode < 1.5) {
            return tonemapReinhard(c);
        } else if (u_TonemapMode < 2.5) {
            return tonemapReinhardExt(c, u_WhitePoint);
        } else if (u_TonemapMode < 3.5) {
            return tonemapACES(c);
        } else if (u_TonemapMode < 4.5) {
            return tonemapFilmic(c);
        }
        return tonemapAGX(c);
    }

    vec3 whiteBalance(vec3 c, float temp, float tnt) {
        vec3 warm = vec3(1.0 + temp * 0.2, 1.0, 1.0 - temp * 0.2);
        vec3 grn = vec3(1.0, 1.0 + tnt * 0.2, 1.0);
        return c * warm * grn;
    }

    vec3 hash33(vec2 p) {
        float n = sin(dot(p, vec2(41.0, 289.0)));
        return fract(vec3(262144.0, 32768.0, 4096.0) * n);
    }

    void main() {
        vec2 uv = v_TexCoord;

        vec3 color;
        if (u_CAEnabled > 0.5) {
            vec2 dir = uv - vec2(0.5, 0.5);
            float amt = u_CAAmount;
            color.r = SAMPLE(u_SceneTex, uv - dir * amt).r;
            color.g = SAMPLE(u_SceneTex, uv).g;
            color.b = SAMPLE(u_SceneTex, uv + dir * amt).b;
        } else {
            color = SAMPLE(u_SceneTex, uv).rgb;
        }

        if (u_SsaoEnabled > 0.5) {
            float ao = SAMPLE(u_SsaoTex, uv).r;
            color = color * ao;
        }

        if (u_BloomEnabled > 0.5) {
            vec3 bloom = SAMPLE(u_BloomTex, uv).rgb;
            color = color + bloom * u_BloomIntensity;
        }

        color = color * u_Exposure;
        color = applyTonemap(color);

        if (u_GradingEnabled > 0.5) {
            color = whiteBalance(color, u_Temperature, u_Tint);
            color = mix(color, color * u_ColorFilter.rgb, u_ColorFilter.a);
            color = (color - vec3(0.5, 0.5, 0.5)) * u_Contrast + vec3(0.5, 0.5, 0.5);
            color = color + vec3(u_Brightness, u_Brightness, u_Brightness);
            float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
            color = mix(vec3(luma, luma, luma), color, u_Saturation);
            color = color * u_Gain + u_Lift * (vec3(1.0, 1.0, 1.0) - color);
            color = max(color, vec3(0.0, 0.0, 0.0));
            color = pow(color, vec3(1.0, 1.0, 1.0) / max(u_Gamma, vec3(0.001, 0.001, 0.001)));
        }

        color = clamp(color, 0.0, 1.0);

        if (u_VignetteEnabled > 0.5) {
            vec2 d = uv - u_VignetteCenter;
            d.x = d.x * mix(1.0, u_Resolution.x / max(u_Resolution.y, 1.0), u_VignetteRoundness);
            float dist = length(d) * 1.41421356;
            float vig = 1.0 - smoothstep(u_VignetteSmoothness * 0.5, 1.0, dist) * u_VignetteIntensity;
            vig = clamp(vig, 0.0, 1.0);
            color = mix(u_VignetteColor.rgb, color, vig);
        }

        if (u_GrainEnabled > 0.5) {
            vec2 gp = floor(uv * u_Resolution / max(u_GrainSize, 0.5));
            float g = hash33(gp + vec2(u_Time, u_Time * 1.7)).x;
            color = color + (g - 0.5) * u_GrainIntensity;
        }

        if (u_OverlayEnabled > 0.5) {
            vec4 ov = SAMPLE(u_OverlayTex, uv);
            float oa = ov.a * u_OverlayOpacity;
            vec3 blended;
            if (u_OverlayBlend < 0.5) {
                blended = ov.rgb;
            } else if (u_OverlayBlend < 1.5) {
                blended = color + ov.rgb;
            } else if (u_OverlayBlend < 2.5) {
                blended = color * ov.rgb;
            } else if (u_OverlayBlend < 3.5) {
                blended = vec3(1.0, 1.0, 1.0) - (vec3(1.0, 1.0, 1.0) - color) * (vec3(1.0, 1.0, 1.0) - ov.rgb);
            } else if (u_OverlayBlend < 4.5) {
                vec3 mlt = color * ov.rgb;
                vec3 scr = vec3(1.0, 1.0, 1.0) - (vec3(1.0, 1.0, 1.0) - color) * (vec3(1.0, 1.0, 1.0) - ov.rgb);
                blended = mix(mlt, scr, step(vec3(0.5, 0.5, 0.5), color));
            } else {
                vec3 d2 = color * color;
                blended = mix(color, d2, vec3(0.5, 0.5, 0.5) - ov.rgb);
            }
            color = mix(color, blended, oa);
        }

        color = clamp(color, 0.0, 1.0);
        FragColor = vec4(color, 1.0);
    }
#end fragment
)LSH";

// One-time additive blend (One/One) for bloom upsample accumulation.
BlendState additiveOneOne() {
    BlendState s;
    s.blendEnable = true;
    s.srcColorBlend = BlendFactor::One;
    s.dstColorBlend = BlendFactor::One;
    s.colorBlendOp = BlendOp::Add;
    s.srcAlphaBlend = BlendFactor::One;
    s.dstAlphaBlend = BlendFactor::One;
    s.alphaBlendOp = BlendOp::Add;
    return s;
}

} // namespace

// ============================================================================
// PostProcessSettings
// ============================================================================

bool PostProcessSettings::anyEffectActive() const {
    if (bloomEnabled) return true;
    if (ssaoEnabled) return true;
    if (colorGradingEnabled) return true;
    if (vignetteEnabled) return true;
    if (chromaticAberrationEnabled) return true;
    if (grainEnabled) return true;
    if (overlayTexture.isValid()) return true;
    // Tonemapping / exposure are only meaningful when explicitly requested.
    if (tonemap != TonemapMode::Linear) return true;
    if (exposure != 1.0f) return true;
    return false;
}

// ============================================================================
// PostProcessStack lifecycle
// ============================================================================

PostProcessStack::~PostProcessStack() {
    shutdown();
}

bool PostProcessStack::initialize(IGfxDevice* device) {
    if (m_initialized) {
        return true;
    }
    if (!device) {
        return false;
    }
    m_device = device;
    m_backend = device->getBackend();

    SamplerDesc linear;
    linear.minFilter = FilterMode::Linear;
    linear.magFilter = FilterMode::Linear;
    linear.mipFilter = FilterMode::Linear;
    linear.wrapU = WrapMode::ClampToEdge;
    linear.wrapV = WrapMode::ClampToEdge;
    linear.wrapW = WrapMode::ClampToEdge;
    m_linearClamp = m_device->createSampler(linear);

    SamplerDesc point;
    point.minFilter = FilterMode::Nearest;
    point.magFilter = FilterMode::Nearest;
    point.mipFilter = FilterMode::Nearest;
    point.wrapU = WrapMode::ClampToEdge;
    point.wrapV = WrapMode::ClampToEdge;
    point.wrapW = WrapMode::ClampToEdge;
    m_pointClamp = m_device->createSampler(point);

    if (!ensureFullscreenMesh()) {
        LOG_ERROR(LogCategory::Render, "[PostProcess] Failed to create full-screen mesh");
        return false;
    }

    m_initialized = true;
    return true;
}

void PostProcessStack::shutdown() {
    if (!m_device) {
        m_initialized = false;
        return;
    }

    destroyPipelines();
    releaseTargets();

    if (m_fullscreenMesh.isValid()) {
        m_device->destroyMesh(m_fullscreenMesh);
        m_fullscreenMesh = MeshHandle();
    }
    if (m_linearClamp.isValid()) {
        m_device->destroySampler(m_linearClamp);
        m_linearClamp = SamplerHandle();
    }
    if (m_pointClamp.isValid()) {
        m_device->destroySampler(m_pointClamp);
        m_pointClamp = SamplerHandle();
    }

    m_device = nullptr;
    m_initialized = false;
}

// ============================================================================
// Full-screen geometry
// ============================================================================

bool PostProcessStack::ensureFullscreenMesh() {
    if (m_fullscreenMesh.isValid()) {
        return true;
    }

    MeshData data;
    data.vertices.resize(4);
    // Clip-space quad. UVs are computed in the vertex shader from the position
    // (with u_FlipV), so the per-vertex texCoord here is informational only.
    data.vertices[0].position = Vec3(-1.0f, -1.0f, 0.0f);
    data.vertices[0].texCoord = Vec2(0.0f, 0.0f);
    data.vertices[1].position = Vec3(1.0f, -1.0f, 0.0f);
    data.vertices[1].texCoord = Vec2(1.0f, 0.0f);
    data.vertices[2].position = Vec3(1.0f, 1.0f, 0.0f);
    data.vertices[2].texCoord = Vec2(1.0f, 1.0f);
    data.vertices[3].position = Vec3(-1.0f, 1.0f, 0.0f);
    data.vertices[3].texCoord = Vec2(0.0f, 1.0f);
    for (auto& v : data.vertices) {
        v.normal = Vec3(0.0f, 0.0f, 1.0f);
        v.color = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
    data.indices = {0, 1, 2, 0, 2, 3};
    data.addSubmesh(6, 4);
    data.calculateBounds();

    m_fullscreenMesh = m_device->createMesh(data);
    return m_fullscreenMesh.isValid();
}

void PostProcessStack::drawFullscreen(IGfxCommandList* cmd) {
    const GPUMesh* mesh = m_device->getMesh(m_fullscreenMesh);
    if (!mesh) {
        return;
    }
    cmd->bindVertexBuffer(mesh->vertexBuffer, 0);
    cmd->bindIndexBuffer(mesh->indexBuffer, IndexFormat::UInt32);
    cmd->drawIndexed(mesh->indexCount, 1, 0, 0, 0);
}

// ============================================================================
// Pipelines
// ============================================================================

PostProcessStack::PostPipeline* PostProcessStack::getPipeline(
    const std::string& name, const char* lshSource, const BlendState& blend) {
    auto it = m_pipelines.find(name);
    if (it != m_pipelines.end() && it->second.valid()) {
        return &it->second;
    }

    ShaderTranslatorResult translated = ShaderTranslator::translate(lshSource, m_backend);
    if (!translated.success) {
        LOG_ERROR(LogCategory::Render, "[PostProcess] Translate failed for '{}': {}",
                  name, translated.errorMessage);
        return nullptr;
    }

    const std::string& vertSource = (m_backend == GraphicsBackend::Metal)
        ? translated.combinedSource : translated.vertexSource;
    const std::string& fragSource = (m_backend == GraphicsBackend::Metal)
        ? translated.combinedSource : translated.fragmentSource;

    ShaderDesc vertDesc;
    vertDesc.stage = ShaderStage::Vertex;
    vertDesc.bytecode = vertSource.c_str();
    vertDesc.bytecodeSize = vertSource.size();
    ShaderHandle vs = m_device->createShader(vertDesc);
    if (!vs.isValid()) {
        LOG_ERROR(LogCategory::Render, "[PostProcess] Vertex compile failed for '{}'", name);
        return nullptr;
    }

    ShaderDesc fragDesc;
    fragDesc.stage = ShaderStage::Fragment;
    fragDesc.bytecode = fragSource.c_str();
    fragDesc.bytecodeSize = fragSource.size();
    ShaderHandle fs = m_device->createShader(fragDesc);
    if (!fs.isValid()) {
        LOG_ERROR(LogCategory::Render, "[PostProcess] Fragment compile failed for '{}'", name);
        m_device->destroyShader(vs);
        return nullptr;
    }

    VertexBufferLayout layout;
    layout.stride = sizeof(Vertex);
    layout.attributes.push_back({"a_Position", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, position)), 0, 0});
    layout.attributes.push_back({"a_Normal", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, normal)), 0, 1});
    layout.attributes.push_back({"a_TexCoord", VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex, texCoord)), 0, 2});
    layout.attributes.push_back({"a_Color", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, color)), 0, 3});

    PipelineDesc desc;
    desc.shaders = {vs, fs};
    desc.vertexLayout = layout;
    desc.topology = PrimitiveTopology::TriangleList;
    desc.blendState = blend;
    desc.depthStencilState = DepthStencilState::noDepth();
    desc.rasterizerState.cullMode = CullMode::None;
    desc.rasterizerState.fillMode = FillMode::Solid;
    desc.colorFormat = TextureFormat::RGBA16_FLOAT;

    PipelineHandle pipeline = m_device->createPipeline(desc);
    if (!pipeline.isValid()) {
        LOG_ERROR(LogCategory::Render, "[PostProcess] Pipeline creation failed for '{}'", name);
        m_device->destroyShader(vs);
        m_device->destroyShader(fs);
        return nullptr;
    }

    PostPipeline pp;
    pp.vertex = vs;
    pp.fragment = fs;
    pp.pipeline = pipeline;
    m_pipelines[name] = pp;
    return &m_pipelines[name];
}

void PostProcessStack::destroyPipelines() {
    if (!m_device) {
        m_pipelines.clear();
        return;
    }
    for (auto& [name, pp] : m_pipelines) {
        if (pp.pipeline.isValid()) m_device->destroyPipeline(pp.pipeline);
        if (pp.vertex.isValid()) m_device->destroyShader(pp.vertex);
        if (pp.fragment.isValid()) m_device->destroyShader(pp.fragment);
    }
    m_pipelines.clear();
}

// ============================================================================
// Render targets
// ============================================================================

PostProcessStack::ColorTarget PostProcessStack::createColorTarget(
    uint32_t width, uint32_t height, TextureFormat format) {
    ColorTarget t;
    RenderTargetDesc desc;
    desc.width = width;
    desc.height = height;
    desc.colorFormat = format;
    desc.hasColor = true;
    desc.hasDepth = false;
    desc.sampleCount = 1;
    t.target = m_device->createRenderTarget(desc);
    if (t.target.isValid()) {
        t.color = m_device->getRenderTargetColorTexture(t.target);
        t.width = width;
        t.height = height;
    }
    return t;
}

void PostProcessStack::destroyColorTarget(ColorTarget& t) {
    if (t.target.isValid() && m_device) {
        m_device->destroyRenderTarget(t.target);
    }
    t = ColorTarget();
}

void PostProcessStack::ensureTargets(uint32_t width, uint32_t height, const PostProcessSettings& settings) {
    const bool sizeChanged = (width != m_width || height != m_height);
    if (sizeChanged) {
        releaseTargets();
        m_width = width;
        m_height = height;
    }

    // Full-resolution HDR ping/pong (used for SSAO-modulated intermediates if needed).
    if (!m_pingA.valid()) {
        m_pingA = createColorTarget(width, height, TextureFormat::RGBA16_FLOAT);
    }
    if (!m_pingB.valid()) {
        m_pingB = createColorTarget(width, height, TextureFormat::RGBA16_FLOAT);
    }

    // SSAO buffers (full-res, 8-bit is sufficient for an occlusion factor).
    if (settings.ssaoEnabled && settings.cameraPerspective) {
        if (!m_ssao.valid()) {
            m_ssao = createColorTarget(width, height, TextureFormat::RGBA8_UNORM);
        }
        if (!m_ssaoBlur.valid()) {
            m_ssaoBlur = createColorTarget(width, height, TextureFormat::RGBA8_UNORM);
        }
        m_ssaoTargetsBuilt = true;
    }

    // Bloom mip chain (each level is half the previous, down to a small floor).
    if (settings.bloomEnabled) {
        int wantLevels = std::max(1, std::min(settings.bloomIterations, 8));
        if (!m_bloomTargetsBuilt || m_bloomLevelsBuilt != wantLevels) {
            for (auto& bt : m_bloomChain) destroyColorTarget(bt);
            m_bloomChain.clear();
            uint32_t w = width;
            uint32_t h = height;
            for (int i = 0; i < wantLevels; ++i) {
                w = std::max(1u, w / 2u);
                h = std::max(1u, h / 2u);
                if (w < 2 || h < 2) break;
                m_bloomChain.push_back(createColorTarget(w, h, TextureFormat::RGBA16_FLOAT));
            }
            m_bloomLevelsBuilt = static_cast<int>(m_bloomChain.size());
            m_bloomTargetsBuilt = true;
        }
    }
}

void PostProcessStack::releaseTargets() {
    destroyColorTarget(m_pingA);
    destroyColorTarget(m_pingB);
    destroyColorTarget(m_ssao);
    destroyColorTarget(m_ssaoBlur);
    for (auto& bt : m_bloomChain) destroyColorTarget(bt);
    m_bloomChain.clear();
    m_ssaoTargetsBuilt = false;
    m_bloomTargetsBuilt = false;
    m_bloomLevelsBuilt = 0;
}

// ============================================================================
// Per-pass helpers
// ============================================================================

uint32_t PostProcessStack::textureSlot(uint32_t index) const {
    // Mirrors the engine's texture binding convention: Vulkan declares .lsh
    // textures starting at binding 4; every other backend uses sequential slots.
    return (m_backend == GraphicsBackend::Vulkan) ? (4u + index) : index;
}

void PostProcessStack::bindInputTexture(IGfxCommandList* cmd, const char* uniformName,
                                        TextureHandle tex, uint32_t index, bool pointSampler) {
    const uint32_t slot = textureSlot(index);
    cmd->bindTexture(tex, slot, 0);
    cmd->setUniformInt(uniformName, static_cast<int>(slot));
    SamplerHandle sampler = pointSampler ? m_pointClamp : m_linearClamp;
    if (sampler.isValid()) {
        cmd->bindSampler(sampler, index);
    }
}

bool PostProcessStack::resolveFlipV(const PostProcessSettings& settings) const {
    switch (settings.flipY) {
        case FlipYMode::Off: return false;
        case FlipYMode::On:  return true;
        case FlipYMode::Auto:
        default:
            return IsTopLeftOrigin(m_backend);
    }
}

// ============================================================================
// Sub-stages
// ============================================================================

void PostProcessStack::runSSAO(TextureHandle sceneDepth, uint32_t width, uint32_t height,
                               const PostProcessSettings& s) {
    m_ssaoResult = TextureHandle();
    if (!s.ssaoEnabled || !s.cameraPerspective || !sceneDepth.isValid()) {
        return;
    }
    if (!m_ssao.valid() || !m_ssaoBlur.valid()) {
        return;
    }

    const float flipV = resolveFlipV(s) ? 1.0f : 0.0f;
    const Vec2 texel(1.0f / static_cast<float>(width), 1.0f / static_cast<float>(height));

    // --- SSAO pass ---
    PostPipeline* pp = getPipeline("ssao", kSSAOSource.c_str(), BlendState::opaque());
    if (!pp) return;
    {
        auto cmd = m_device->beginFrame(m_ssao.target);
        if (!cmd) return;
        Viewport vp{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f};
        cmd->setViewport(vp);
        cmd->setScissor(ScissorRect{0, 0, width, height});
        cmd->beginDebugMarker("PostProcess.SSAO");
        // The pass pipelines are created as RGBA16F; the SSAO target is RGBA8. Bind the
        // variant matching this target's format (no-op on backends that don't bake it).
        cmd->bindPipeline(m_device->getColorFormatVariant(pp->pipeline, m_device->getRenderTargetColorFormat(m_ssao.target)));
        cmd->setUniformFloat("u_FlipV", flipV);
        cmd->setUniformVec2("u_TexelSize", texel);
        cmd->setUniformMat4("u_Projection", s.projection);
        cmd->setUniformMat4("u_InvProjection", s.invProjection);
        cmd->setUniformFloat("u_Radius", s.ssaoRadius);
        cmd->setUniformFloat("u_Bias", s.ssaoBias);
        cmd->setUniformFloat("u_Intensity", s.ssaoIntensity);
        cmd->setUniformFloat("u_Power", s.ssaoPower);
        cmd->setUniformFloat("u_SampleCount", static_cast<float>(std::clamp(s.ssaoSamples, 1, 64)));
        cmd->setUniformFloat("u_DepthZeroToOne", IsZeroToOneDepth(m_backend) ? 1.0f : 0.0f);
        bindInputTexture(cmd.get(), "u_DepthTex", sceneDepth, 0, true);
        drawFullscreen(cmd.get());
        cmd->endDebugMarker();
        m_device->submit(std::move(cmd));
    }

    // --- SSAO blur pass ---
    PostPipeline* blur = getPipeline("ssao_blur", kSSAOBlurSource.c_str(), BlendState::opaque());
    if (!blur) {
        m_ssaoResult = m_ssao.color;
        return;
    }
    {
        auto cmd = m_device->beginFrame(m_ssaoBlur.target);
        if (!cmd) return;
        Viewport vp{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f};
        cmd->setViewport(vp);
        cmd->setScissor(ScissorRect{0, 0, width, height});
        cmd->beginDebugMarker("PostProcess.SSAOBlur");
        cmd->bindPipeline(m_device->getColorFormatVariant(blur->pipeline, m_device->getRenderTargetColorFormat(m_ssaoBlur.target)));
        cmd->setUniformFloat("u_FlipV", flipV);
        cmd->setUniformVec2("u_TexelSize", texel);
        bindInputTexture(cmd.get(), "u_AOTex", m_ssao.color, 0, false);
        drawFullscreen(cmd.get());
        cmd->endDebugMarker();
        m_device->submit(std::move(cmd));
    }

    m_ssaoResult = m_ssaoBlur.color;
}

void PostProcessStack::runBloom(TextureHandle sceneColor, uint32_t width, uint32_t height,
                                const PostProcessSettings& s) {
    m_bloomResult = TextureHandle();
    if (!s.bloomEnabled || m_bloomChain.empty()) {
        return;
    }

    const float flipV = resolveFlipV(s) ? 1.0f : 0.0f;
    const int levels = static_cast<int>(m_bloomChain.size());

    // --- Prefilter: scene -> bloom level 0 ---
    PostPipeline* pre = getPipeline("bloom_prefilter", kBloomPrefilterSource.c_str(), BlendState::opaque());
    if (!pre) return;
    {
        const ColorTarget& dst = m_bloomChain[0];
        auto cmd = m_device->beginFrame(dst.target);
        if (!cmd) return;
        Viewport vp{0.0f, 0.0f, static_cast<float>(dst.width), static_cast<float>(dst.height), 0.0f, 1.0f};
        cmd->setViewport(vp);
        cmd->setScissor(ScissorRect{0, 0, dst.width, dst.height});
        cmd->beginDebugMarker("PostProcess.BloomPrefilter");
        cmd->bindPipeline(pre->pipeline);
        cmd->setUniformFloat("u_FlipV", flipV);
        cmd->setUniformVec2("u_TexelSize", Vec2(1.0f / static_cast<float>(width), 1.0f / static_cast<float>(height)));
        cmd->setUniformFloat("u_Threshold", s.bloomThreshold);
        cmd->setUniformFloat("u_SoftKnee", s.bloomSoftKnee);
        bindInputTexture(cmd.get(), "u_SceneTex", sceneColor, 0, false);
        drawFullscreen(cmd.get());
        cmd->endDebugMarker();
        m_device->submit(std::move(cmd));
    }

    // --- Downsample: level i-1 -> level i ---
    PostPipeline* down = getPipeline("bloom_downsample", kBloomDownsampleSource.c_str(), BlendState::opaque());
    if (!down) { m_bloomResult = m_bloomChain[0].color; return; }
    for (int i = 1; i < levels; ++i) {
        const ColorTarget& src = m_bloomChain[i - 1];
        const ColorTarget& dst = m_bloomChain[i];
        auto cmd = m_device->beginFrame(dst.target);
        if (!cmd) return;
        Viewport vp{0.0f, 0.0f, static_cast<float>(dst.width), static_cast<float>(dst.height), 0.0f, 1.0f};
        cmd->setViewport(vp);
        cmd->setScissor(ScissorRect{0, 0, dst.width, dst.height});
        cmd->beginDebugMarker("PostProcess.BloomDownsample");
        cmd->bindPipeline(down->pipeline);
        cmd->setUniformFloat("u_FlipV", flipV);
        cmd->setUniformVec2("u_TexelSize", Vec2(1.0f / static_cast<float>(src.width), 1.0f / static_cast<float>(src.height)));
        bindInputTexture(cmd.get(), "u_SourceTex", src.color, 0, false);
        drawFullscreen(cmd.get());
        cmd->endDebugMarker();
        m_device->submit(std::move(cmd));
    }

    // --- Upsample: additively accumulate level i+1 onto level i ---
    PostPipeline* up = getPipeline("bloom_upsample", kBloomUpsampleSource.c_str(), additiveOneOne());
    if (!up) { m_bloomResult = m_bloomChain[0].color; return; }
    for (int i = levels - 2; i >= 0; --i) {
        const ColorTarget& src = m_bloomChain[i + 1];
        const ColorTarget& dst = m_bloomChain[i];
        auto cmd = m_device->beginFrame(dst.target);
        if (!cmd) return;
        Viewport vp{0.0f, 0.0f, static_cast<float>(dst.width), static_cast<float>(dst.height), 0.0f, 1.0f};
        cmd->setViewport(vp);
        cmd->setScissor(ScissorRect{0, 0, dst.width, dst.height});
        cmd->beginDebugMarker("PostProcess.BloomUpsample");
        cmd->bindPipeline(up->pipeline);
        cmd->setUniformFloat("u_FlipV", flipV);
        cmd->setUniformVec2("u_TexelSize", Vec2(1.0f / static_cast<float>(src.width), 1.0f / static_cast<float>(src.height)));
        cmd->setUniformFloat("u_Radius", 1.0f);
        bindInputTexture(cmd.get(), "u_SourceTex", src.color, 0, false);
        drawFullscreen(cmd.get());
        cmd->endDebugMarker();
        m_device->submit(std::move(cmd));
    }

    m_bloomResult = m_bloomChain[0].color;
}

void PostProcessStack::runComposite(RenderTargetHandle finalTarget, TextureHandle sceneColor,
                                    uint32_t width, uint32_t height, const PostProcessSettings& s) {
    PostPipeline* pp = getPipeline("composite", kCompositeSource.c_str(), BlendState::opaque());
    if (!pp) {
        return;
    }

    const float flipV = resolveFlipV(s) ? 1.0f : 0.0f;
    const bool bloomOn = s.bloomEnabled && m_bloomResult.isValid();
    const bool ssaoOn = s.ssaoEnabled && s.cameraPerspective && m_ssaoResult.isValid();
    const bool overlayOn = s.overlayTexture.isValid();

    auto cmd = m_device->beginFrame(finalTarget);
    if (!cmd) {
        return;
    }
    Viewport vp{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f};
    cmd->setViewport(vp);
    cmd->setScissor(ScissorRect{0, 0, width, height});
    cmd->beginDebugMarker("PostProcess.Composite");
    // The composite outputs to the final target (the swapchain backbuffer, RGBA8) while
    // the pass pipeline is RGBA16F; bind the variant matching the final target's format.
    cmd->bindPipeline(m_device->getColorFormatVariant(pp->pipeline, m_device->getRenderTargetColorFormat(finalTarget)));

    cmd->setUniformFloat("u_FlipV", flipV);
    cmd->setUniformVec2("u_Resolution", Vec2(static_cast<float>(width), static_cast<float>(height)));
    cmd->setUniformFloat("u_Time", s.time);

    cmd->setUniformFloat("u_Exposure", s.exposure);
    cmd->setUniformFloat("u_TonemapMode", static_cast<float>(static_cast<int>(s.tonemap)));
    cmd->setUniformFloat("u_WhitePoint", s.whitePoint);

    cmd->setUniformFloat("u_BloomEnabled", bloomOn ? 1.0f : 0.0f);
    cmd->setUniformFloat("u_BloomIntensity", s.bloomIntensity);

    cmd->setUniformFloat("u_SsaoEnabled", ssaoOn ? 1.0f : 0.0f);

    cmd->setUniformFloat("u_GradingEnabled", s.colorGradingEnabled ? 1.0f : 0.0f);
    cmd->setUniformFloat("u_Contrast", s.contrast);
    cmd->setUniformFloat("u_Saturation", s.saturation);
    cmd->setUniformFloat("u_Brightness", s.brightness);
    cmd->setUniformFloat("u_Temperature", s.temperature);
    cmd->setUniformFloat("u_Tint", s.tint);
    cmd->setUniformVec4("u_ColorFilter", s.colorFilter);
    cmd->setUniformVec3("u_Lift", s.lift);
    cmd->setUniformVec3("u_Gamma", s.gamma);
    cmd->setUniformVec3("u_Gain", s.gain);

    cmd->setUniformFloat("u_VignetteEnabled", s.vignetteEnabled ? 1.0f : 0.0f);
    cmd->setUniformVec4("u_VignetteColor", s.vignetteColor);
    cmd->setUniformFloat("u_VignetteIntensity", s.vignetteIntensity);
    cmd->setUniformFloat("u_VignetteSmoothness", s.vignetteSmoothness);
    cmd->setUniformFloat("u_VignetteRoundness", s.vignetteRoundness);
    cmd->setUniformVec2("u_VignetteCenter", s.vignetteCenter);

    cmd->setUniformFloat("u_CAEnabled", s.chromaticAberrationEnabled ? 1.0f : 0.0f);
    cmd->setUniformFloat("u_CAAmount", s.chromaticAberrationAmount);

    cmd->setUniformFloat("u_GrainEnabled", s.grainEnabled ? 1.0f : 0.0f);
    cmd->setUniformFloat("u_GrainIntensity", s.grainIntensity);
    cmd->setUniformFloat("u_GrainSize", s.grainSize);

    cmd->setUniformFloat("u_OverlayEnabled", overlayOn ? 1.0f : 0.0f);
    cmd->setUniformFloat("u_OverlayBlend", static_cast<float>(static_cast<int>(s.overlayBlend)));
    cmd->setUniformFloat("u_OverlayOpacity", s.overlayOpacity);

    // Texture inputs: scene(0), bloom(1), ssao(2), overlay(3). Always bind valid
    // textures (fall back to the scene color) so descriptor slots are never empty.
    bindInputTexture(cmd.get(), "u_SceneTex", sceneColor, 0, false);
    bindInputTexture(cmd.get(), "u_BloomTex", bloomOn ? m_bloomResult : sceneColor, 1, false);
    bindInputTexture(cmd.get(), "u_SsaoTex", ssaoOn ? m_ssaoResult : sceneColor, 2, false);
    bindInputTexture(cmd.get(), "u_OverlayTex", overlayOn ? s.overlayTexture : sceneColor, 3, false);

    drawFullscreen(cmd.get());
    cmd->endDebugMarker();
    m_device->submit(std::move(cmd));
}

// ============================================================================
// Entry point
// ============================================================================

void PostProcessStack::execute(RenderTargetHandle finalTarget,
                               TextureHandle sceneColor,
                               TextureHandle sceneDepth,
                               uint32_t width,
                               uint32_t height,
                               const PostProcessSettings& settings) {
    if (!m_initialized || !m_device || !sceneColor.isValid() || width == 0 || height == 0) {
        return;
    }

    ensureTargets(width, height, settings);

    runSSAO(sceneDepth, width, height, settings);
    runBloom(sceneColor, width, height, settings);
    runComposite(finalTarget, sceneColor, width, height, settings);
}

} // namespace lupine
