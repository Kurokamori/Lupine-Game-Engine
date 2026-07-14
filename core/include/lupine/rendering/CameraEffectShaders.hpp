#pragma once

#include <string>

namespace lupine {

/**
 * Shared .lsh text fragments for the camera-effect system, used by both the renderer
 * (CameraEffectStack's composite-shader generator) and the multi-tap effect components
 * (which author full standalone passes). Keeping the full-screen vertex stage and the
 * standard property block in one place guarantees both paths agree on uniform names and
 * the texture-V convention.
 *
 * Every camera-effect pass — generated or hand-authored — declares this standard set:
 *   u_FlipV       float    selects the texture-V convention (renderer-managed)
 *   u_Resolution  vec2     view pixel size (renderer-managed)
 *   u_TexelSize   vec2     1/Resolution (renderer-managed)
 *   u_Time        float    seconds, for animated effects (renderer-managed)
 *   u_SceneTex    sampler  input color at slot 0 (renderer-managed)
 * Depth-sampling passes additionally declare `uniform sampler2D u_DepthTex;` (slot 1).
 */
namespace CameraEffectShaders {

// Full-screen vertex stage. a_Position holds clip-space coordinates directly; u_FlipV
// picks the V convention so a pure pass-through preserves orientation on every backend.
inline const char* FullscreenVertex() {
    return R"LSH(
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
}

// The standard, always-present property declarations (renderer-managed uniforms).
inline std::string StandardPropertyDecls() {
    return
        "    uniform float u_FlipV = 0.0;\n"
        "    uniform vec2 u_Resolution = vec2(1920.0, 1080.0);\n"
        "    uniform vec2 u_TexelSize = vec2(0.00052, 0.000926);\n"
        "    uniform float u_Time = 0.0;\n"
        "    uniform sampler2D u_SceneTex;\n";
}

/**
 * Build a complete standalone effect shader.
 *
 * @param shaderName     unique shader name (also used for diagnostics)
 * @param extraDecls     effect-specific uniform declarations (and `uniform sampler2D u_DepthTex;`
 *                       for depth passes), one per line, already indented
 * @param fragmentBody   the fragment-stage body: any helper functions plus a `void main()`
 *                       that samples u_SceneTex and writes `FragColor`
 */
inline std::string BuildStandaloneShader(const std::string& shaderName,
                                         const std::string& extraDecls,
                                         const std::string& fragmentBody) {
    std::string src;
    src.reserve(extraDecls.size() + fragmentBody.size() + 512);
    src += "#shader \"";
    src += shaderName;
    src += "\"\n#description \"Camera effect pass.\"\n\n#begin properties\n";
    src += StandardPropertyDecls();
    src += extraDecls;
    src += "#end properties\n";
    src += FullscreenVertex();
    src += "#begin fragment\n    #output vec4 FragColor\n";
    src += fragmentBody;
    src += "\n#end fragment\n";
    return src;
}

} // namespace CameraEffectShaders
} // namespace lupine
