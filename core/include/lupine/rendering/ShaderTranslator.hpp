#pragma once

#include <string>
#include <vector>

namespace lupine {

enum class GraphicsBackend;  // forward declare from GfxTypes.hpp

/**
 * Per-texture sampler configuration extracted from sampler hints (Gap G). The renderer
 * uses this to configure the sampler state / fallback texture for a custom-shader texture.
 */
struct LshSamplerConfig {
    std::string name;
    int slot = -1;               // texture binding slot (declaration order: t0, t1, ...)
    bool sourceColor = false;
    std::string filter;          // "nearest" | "linear" (empty = engine default)
    std::string repeat;          // "repeat" | "clamp" | "mirror" (empty = engine default)
    std::string hintDefault;     // "black" | "white" | "normal" (empty = engine default)
};

/**
 * Pipeline render state declared via `#render_mode` (Gap B). Defaults match a Godot
 * spatial material. `specified` stays false until a `#render_mode` directive sets at
 * least one token, letting the renderer keep host-driven defaults when absent.
 */
struct LshRenderMode {
    bool specified = false;
    std::string cull = "back";   // back | front | disabled
    std::string blend = "mix";   // mix | add | sub | mul | premul | opaque
    bool depthTest = true;
    bool depthWrite = true;
    bool unshaded = false;
};

/**
 * Result of translating a .lsh shader to a specific backend.
 */
struct ShaderTranslatorResult {
    std::string vertexSource;
    std::string fragmentSource;
    std::string geometrySource;  // For DirectX11/12 shaders with a geometry stage
    std::string combinedSource;  // For Metal (vertex+fragment in one file)
    bool success = false;
    std::string errorMessage;

    // Feature flags from #feature directives
    bool usesLighting = false;
    bool usesShadows = false;
    bool usesNormalMapping = false;
    bool usesFog = false;
    bool usesTime = false;          // body references TIME (Gap E) -> renderer feeds u_Time

    // Sampler hints (Gap G) and render state (Gap B) for the renderer.
    std::vector<LshSamplerConfig> samplerConfigs;
    LshRenderMode renderMode;
};

/**
 * Parsed uniform/property declaration from a .lsh file.
 */
struct LshUniformInfo {
    std::string typeName;
    std::string name;
    std::string defaultValue;
    bool isTexture = false;
    bool isCubemap = false;
    int arraySize = 0;
    // Sampler hints (Gap G) — only meaningful for texture uniforms.
    bool sourceColor = false;    // decode sRGB->linear on sample
    std::string filter;          // "nearest" | "linear"
    std::string repeat;          // "repeat" | "clamp" | "mirror"
    std::string hintDefault;     // "black" | "white" | "normal"
};

/**
 * Parsed stage input declaration (#input directive).
 */
struct LshInputInfo {
    std::string typeName;
    std::string name;
    std::string semantic;   // POSITION, NORMAL, TEXCOORD0, COLOR, etc.
    int location = 0;
};

/**
 * Parsed stage output declaration (#output directive).
 */
struct LshOutputInfo {
    std::string typeName;
    std::string name;
    std::string semantic;   // For fragment: COLOR0, etc.
};

/**
 * Parsed vertex or fragment stage.
 */
struct LshStageInfo {
    std::vector<LshInputInfo> inputs;
    std::vector<LshOutputInfo> outputs;
    std::string body;
    std::string topology;    // For geometry: "point", "line", "triangle"
    int maxVertices = 0;     // For geometry: max output vertices
};

/**
 * Intermediate representation of a parsed .lsh shader file.
 */
struct LshShaderIR {
    std::string name;
    std::string description;
    std::vector<LshUniformInfo> properties;
    std::string sharedCode;
    LshStageInfo vertex;
    LshStageInfo fragment;
    LshStageInfo geometry;
    bool hasGeometry = false;

    // Feature flags from #feature directives
    bool hasLighting = false;
    bool hasShadows = false;
    bool hasNormalMapping = false;
    bool hasFog = false;

    // Built-in usage flags computed after parsing (drive entry-signature plumbing).
    bool usesTime = false;          // body references TIME (Gap E) -> u_Time uniform
    bool usesFrontFacing = false;   // fragment references FRONT_FACING (Gap D)
    bool usesPrimaryShadow = false; // fragment references PRIMARY_SHADOW_ATTENUATION (Gap A)

    LshRenderMode renderMode;       // parsed from #render_mode (Gap B)
};

/**
 * Runtime translator for .lsh (Lupine Shader) files to backend-specific shader code.
 *
 * Mirrors the Python transpiler (lupine_shader_transpiler.py) logic, supporting:
 *   - OpenGL 3.3 (GLSL 330 core)
 *   - WebGL 2.0 (GLSL ES 3.0)
 *   - Vulkan (GLSL 450 with push constants)
 *   - DirectX 11/12 (HLSL with cbuffer)
 *   - Metal (MSL with structs and [[attribute]])
 *
 * Cross-platform macros expanded at translation time:
 *   VERTEX_OUTPUT, SAMPLE(), SAMPLE_LOD(), SAMPLE_CUBE(), MUL(), MAT3(),
 *   DISCARD, FRAG_COORD, INSTANCE_ID, VERTEX_ID
 */
class ShaderTranslator {
public:
    /// Translate .lsh source to a specific backend
    static ShaderTranslatorResult translate(const std::string& lshSource, GraphicsBackend backend);

    /// Parse .lsh source into IR (for inspection/validation)
    static bool parse(const std::string& lshSource, LshShaderIR& outIR, std::string& errorMsg);

    /// Get file extension for a backend ("vert"/"frag" or "metal")
    static const char* getShaderExtension(GraphicsBackend backend);

    /// Validate .lsh source without translating
    static bool validate(const std::string& lshSource, std::string& errorMsg);

private:
    // --- Emitters: GLSL 330 (OpenGL) ---
    static std::string emitGLSLVertex(const LshShaderIR& ir);
    static std::string emitGLSLFragment(const LshShaderIR& ir);

    // --- Emitters: GLSL ES 3.0 (WebGL) ---
    static std::string emitGLSLESVertex(const LshShaderIR& ir);
    static std::string emitGLSLESFragment(const LshShaderIR& ir);

    // --- Emitters: GLSL 450 (Vulkan) ---
    static std::string emitGLSL450Vertex(const LshShaderIR& ir);
    static std::string emitGLSL450Fragment(const LshShaderIR& ir);

    // --- Emitters: HLSL (DirectX 11/12) ---
    static std::string emitHLSLVertex(const LshShaderIR& ir, GraphicsBackend backend);
    static std::string emitHLSLFragment(const LshShaderIR& ir, GraphicsBackend backend);
    static std::string emitHLSLGeometry(const LshShaderIR& ir, GraphicsBackend backend);

    // --- Emitter: Metal (combined vertex+fragment) ---
    static std::string emitMetal(const LshShaderIR& ir);

    // --- Body code transformation ---
    static std::string transformBody(const std::string& body, GraphicsBackend backend,
                                     const LshShaderIR& ir, const std::string& stage);

    // --- Type conversion helpers ---
    static std::string toHLSLType(const std::string& glslType);
    static std::string toMetalType(const std::string& glslType);
    static std::string replaceHLSLTypes(const std::string& code);
    static std::string replaceMetalTypes(const std::string& code);
    static std::string replaceHLSLFunctions(const std::string& code);
    static std::string replaceMetalFunctions(const std::string& code);

    // --- Macro expansion ---
    static std::string expandMacros(const std::string& code, GraphicsBackend backend);
    static std::string expandSampleMacros(const std::string& code, GraphicsBackend backend);
    static std::string expandMulMacros(const std::string& code, GraphicsBackend backend);

    // --- Uniform/IO access transforms ---
    static std::string transformUniformAccess(const std::string& code, GraphicsBackend backend,
                                              const LshShaderIR& ir);
    static std::string transformIOAccess(const std::string& code, GraphicsBackend backend,
                                         const LshShaderIR& ir, const std::string& stage);

    // --- HLSL helpers ---
    static std::string buildHLSLCBuffer(const LshShaderIR& ir, GraphicsBackend backend);
    static std::string buildHLSLTextures(const LshShaderIR& ir);
    static int getTypeSize(const std::string& typeName);
    static int getTypeAlignment(const std::string& typeName);

    // --- Utility ---
    static std::string hlslSemantic(const std::string& semantic);
};

} // namespace lupine
