#include "lupine/rendering/ShaderTranslator.hpp"
#include "lupine/rendering/gfx/GfxTypes.hpp"

#include <algorithm>
#include <cctype>
#include <functional>
#include <regex>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace lupine {

// =============================================================================
// Type / Function Mapping Tables
// =============================================================================

static const std::unordered_map<std::string, std::string> kGLSLToHLSLTypes = {
    {"vec2",  "float2"},  {"vec3",  "float3"},  {"vec4",  "float4"},
    {"ivec2", "int2"},    {"ivec3", "int3"},     {"ivec4", "int4"},
    {"uvec2", "uint2"},   {"uvec3", "uint3"},    {"uvec4", "uint4"},
    {"bvec2", "bool2"},   {"bvec3", "bool3"},    {"bvec4", "bool4"},
    {"mat2",  "float2x2"},{"mat3",  "float3x3"}, {"mat4", "float4x4"},
    {"sampler2D", "Texture2D"}, {"samplerCube", "TextureCube"},
    {"sampler2DShadow", "Texture2D"},
};

static const std::unordered_map<std::string, std::string> kGLSLToMetalTypes = {
    {"vec2",  "float2"},  {"vec3",  "float3"},  {"vec4",  "float4"},
    {"ivec2", "int2"},    {"ivec3", "int3"},     {"ivec4", "int4"},
    {"uvec2", "uint2"},   {"uvec3", "uint3"},    {"uvec4", "uint4"},
    {"bvec2", "bool2"},   {"bvec3", "bool3"},    {"bvec4", "bool4"},
    {"mat2",  "float2x2"},{"mat3",  "float3x3"}, {"mat4", "float4x4"},
    {"sampler2D", "texture2d<float>"},
    {"samplerCube", "texturecube<float>"},
    {"sampler2DShadow", "depth2d<float>"},
};

// NOTE: GLSL 'mod' is intentionally NOT mapped to 'fmod'. GLSL mod is floored
// (result takes the divisor's sign); HLSL/Metal fmod is truncated (dividend's sign).
// They differ for negative operands, so mod is expanded separately to the floored
// equivalent (x - y * floor(x / y)) — see expandModMacros.
static const std::unordered_map<std::string, std::string> kHLSLFuncReplacements = {
    {"mix",          "lerp"},
    {"fract",        "frac"},
    {"dFdx",         "ddx"},
    {"dFdy",         "ddy"},
    {"inversesqrt",  "rsqrt"},
    {"atan",         "atan2"},
};

static const std::unordered_map<std::string, std::string> kMetalFuncReplacements = {
    {"dFdx",         "dfdx"},
    {"dFdy",         "dfdy"},
    {"inversesqrt",  "rsqrt"},
};

static const std::unordered_map<std::string, std::string> kHLSLSemanticMap = {
    {"POSITION",     "POSITION"},
    {"NORMAL",       "NORMAL"},
    {"TEXCOORD0",    "TEXCOORD0"},
    {"TEXCOORD1",    "TEXCOORD1"},
    {"TEXCOORD2",    "TEXCOORD2"},
    {"TEXCOORD3",    "TEXCOORD3"},
    {"COLOR",        "COLOR"},
    {"TANGENT",      "TANGENT"},
    {"BLENDWEIGHT",  "BLENDWEIGHT"},
    {"BLENDINDICES", "BLENDINDICES"},
};

static const std::unordered_set<std::string> kPushConstantNames = {
    "u_ViewProjection", "u_Model", "u_NormalMatrix", "u_TintColor"
};

// Uniforms the engine writes to a dedicated GPU buffer (Vulkan binding 1, HLSL cbuffer b2).
static const std::unordered_set<std::string> kSeparateBufferNames = {
    "u_BoneTransforms"
};

// Standard PerObjectUniforms fields. An HLSL shader that declares ANY of these is
// driven by the batch rendering path, where the engine uploads a fixed-layout struct
// as a raw byte blob via pushConstants: the full 352-byte PerObjectUniforms struct in
// RenderWorld::executeBatch, or the prefix-compatible ShadowUniforms struct in the
// shadow pass. Such a shader's b0 cbuffer MUST begin with the full fixed prefix so the
// raw uploads land at the correct byte offsets — applies to both DirectX 11 and 12.
// u_ViewProjection is excluded: it is at offset 0 in both layouts, so shaders that
// declare only u_ViewProjection (skybox, text) stay compact and are driven by
// reflection-based setUniform* calls. Must stay in sync with FULL_LAYOUT_TRIGGERS in
// core/shaders/lupine_shader_transpiler.py.
static const std::unordered_set<std::string> kHLSLFullLayoutTriggers = {
    "u_Model", "u_NormalMatrix", "u_TintColor", "u_Color",
    "u_UseTexture", "u_AlphaCutoff", "u_UVRect", "u_TextureFlags",
    "u_MaterialParams1", "u_MaterialParams2", "u_CameraPosition",
    "u_AlbedoColor", "u_EmissiveColor", "u_ReceiveShadow",
};

// Semantic UI/2D-shape uniforms that ALIAS the u_TextureFlags slot (byte offset 240).
// The engine packs their values into that slot in RenderWorld::executeBatch and uploads
// them as the raw pushConstants blob, so the HLSL cbuffer must emit the alias name AT the
// slot rather than after the 352-byte prefix. u_Size is handled separately (it aliases the
// u_MaterialParams1 slot @256 as a float2). Must stay in sync with
// TEXTUREFLAGS_SLOT_ALIASES in core/shaders/lupine_shader_transpiler.py.
static const std::vector<std::string> kHLSLTextureFlagsSlotAliases = {
    "u_CornerRadius", "u_GradientParams", "u_PolygonParams"
};

// =============================================================================
// Utility Helpers
// =============================================================================

static bool isBackendGLSL(GraphicsBackend b) {
    return b == GraphicsBackend::OpenGL || b == GraphicsBackend::WebGL;
}

static bool isBackendHLSL(GraphicsBackend b) {
    return b == GraphicsBackend::DirectX11 || b == GraphicsBackend::DirectX12;
}

/// Split a string into lines (preserves empty lines).
static std::vector<std::string> splitLines(const std::string& s) {
    std::vector<std::string> lines;
    std::istringstream iss(s);
    std::string line;
    while (std::getline(iss, line)) {
        lines.push_back(line);
    }
    return lines;
}

/// Trim whitespace from both ends.
static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

/// Word-boundary-aware regex replace (uses \b around the pattern).
static std::string regexReplaceWord(const std::string& src, const std::string& word, const std::string& replacement) {
    std::regex re("\\b" + word + "\\b");
    return std::regex_replace(src, re, replacement);
}

/// Word-boundary regex replace for function calls: \bname\s*\(  ->  replacement(
static std::string regexReplaceFuncCall(const std::string& src, const std::string& funcName, const std::string& replacement) {
    std::regex re("\\b" + funcName + "\\s*\\(");
    return std::regex_replace(src, re, replacement + "(");
}

/// Prefix standalone uniform references (e.g. u_Foo -> material.u_Foo) without rewriting
/// member accesses ('x.u_Foo') or identifier substrings. std::regex (ECMAScript) lacks
/// lookbehind, so the preceding non-identifier char is captured and re-emitted.
static std::string regexReplaceUniform(const std::string& src, const std::string& name, const std::string& replacement) {
    std::regex re("(^|[^.A-Za-z0-9_])" + name + "\\b");
    return std::regex_replace(src, re, "$1" + replacement);
}

/// Expand single-scalar HLSL/Metal vector constructors: float3(x) -> float3(x, x, x).
/// GLSL allows scalar broadcast; HLSL rejects it, so the constructor must be expanded.
static std::string expandScalarConstructors(const std::string& code) {
    static const std::unordered_map<std::string, int> dims = {
        {"float2", 2}, {"float3", 3}, {"float4", 4},
        {"int2", 2},   {"int3", 3},   {"int4", 4},
    };
    std::regex re(R"(\b(float[234]|int[234])\s*\(\s*([^(),]+)\s*\))");
    std::string result;
    auto begin = std::sregex_iterator(code.begin(), code.end(), re);
    auto end = std::sregex_iterator();
    size_t last = 0;
    for (auto it = begin; it != end; ++it) {
        const std::smatch& m = *it;
        result.append(code, last, static_cast<size_t>(m.position()) - last);
        std::string typeName = m[1].str();
        std::string arg = trim(m[2].str());
        auto d = dims.find(typeName);
        if (d != dims.end() && arg.find(',') == std::string::npos) {
            std::string expanded = typeName + "(";
            for (int i = 0; i < d->second; ++i) {
                if (i) expanded += ", ";
                expanded += arg;
            }
            expanded += ")";
            result += expanded;
        } else {
            result += m.str();
        }
        last = static_cast<size_t>(m.position()) + static_cast<size_t>(m.length());
    }
    result.append(code, last, std::string::npos);
    return result;
}

/// Rewrite global-scope HLSL 'const TYPE' to 'static const TYPE' (an external constant
/// otherwise ignores its initializer and reads as zero).
/// Only tokens at brace/paren depth zero are rewritten so function locals and
/// parameters keep plain 'const', and tokens already preceded by 'static' are
/// skipped (a blind regex would emit 'static static const').
static std::string hlslStaticConst(const std::string& code) {
    static const std::regex constTypeRe(
        R"(const\s+(?:float|float2|float3|float4|float2x2|float3x3|float4x4|int|int2|int3|int4|uint|uint2|uint3|uint4|half|double|bool)\b)");

    auto isWord = [](char ch) {
        return std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_';
    };

    std::string out;
    out.reserve(code.size() + 64);
    int brace = 0;
    int paren = 0;
    const size_t n = code.size();

    for (size_t i = 0; i < n; ++i) {
        const char c = code[i];
        if (c == '{') {
            ++brace;
        } else if (c == '}') {
            --brace;
        } else if (c == '(') {
            ++paren;
        } else if (c == ')') {
            --paren;
        } else if (c == 'c' && brace == 0 && paren == 0 &&
                   (i == 0 || !isWord(code[i - 1]))) {
            std::smatch m;
            if (std::regex_search(code.begin() + static_cast<std::ptrdiff_t>(i), code.end(), m,
                                  constTypeRe, std::regex_constants::match_continuous)) {
                size_t j = i;
                while (j > 0 && (code[j - 1] == ' ' || code[j - 1] == '\t')) {
                    --j;
                }
                const bool precededByStatic =
                    j >= 6 && code.compare(j - 6, 6, "static") == 0 &&
                    (j == 6 || !isWord(code[j - 7]));
                if (!precededByStatic) {
                    out += "static ";
                }
            }
        }
        out += c;
    }
    return out;
}

// =============================================================================
// Parser
// =============================================================================

/// Parse a non-negative integer from user .lsh input, clamped to [fallback, maxValue].
/// std::stoi throws on out-of-range literals (e.g. "uniform mat4 u_X[99999999999]"),
/// which would otherwise propagate out of translate() and crash the engine.
static int parseIntClamped(const std::string& s, int fallback, int maxValue) {
    try {
        long long v = std::stoll(s);
        if (v < 0) return fallback;
        if (v > static_cast<long long>(maxValue)) return maxValue;
        return static_cast<int>(v);
    } catch (const std::exception&) {
        return fallback;
    }
}

/// Parse a single uniform/property line.
static bool parseUniformLine(const std::string& rawLine, LshUniformInfo& out) {
    // Extract sampler-hint annotations (Gap G) from the raw line (including the trailing
    // comment) before they are stripped.
    auto annVal = [&](const std::string& key) -> std::string {
        std::smatch am;
        std::regex are("@" + key + R"LSH(\s+"([^"]*)")LSH");
        if (std::regex_search(rawLine, am, are)) return am[1].str();
        return "";
    };
    const std::string srgb = annVal("source_color");
    const bool sourceColor = (srgb == "true" || srgb == "1" || srgb == "yes")
        || std::regex_search(rawLine, std::regex(R"LSH(@source_color\b(?!\s*"))LSH"));
    const std::string annFilter = annVal("filter");
    const std::string annRepeat = annVal("repeat");
    const std::string annHintDefault = annVal("hint_default");

    // Strip inline comments
    std::string line = std::regex_replace(rawLine, std::regex("//.*$"), "");
    // Strip annotations (@category "...", @display "...")
    line = std::regex_replace(line, std::regex(R"LSH(@\w+\s+"[^"]*")LSH"), "");
    line = trim(line);
    if (line.empty()) return false;
    // Remove trailing semicolon
    if (!line.empty() && line.back() == ';') line.pop_back();
    line = trim(line);

    // uniform TYPE NAME[SIZE] = DEFAULT
    std::regex re(R"(uniform\s+(\w+)\s+(\w+)(?:\[(\d+)\])?\s*(?:=\s*(.+))?)");
    std::smatch m;
    if (!std::regex_match(line, m, re)) return false;

    out.typeName = m[1].str();
    out.name = m[2].str();
    out.arraySize = m[3].matched ? parseIntClamped(m[3].str(), 0, 65536) : 0;
    out.defaultValue = m[4].matched ? trim(m[4].str()) : "";
    out.isTexture = (out.typeName == "sampler2D" || out.typeName == "sampler2DShadow");
    out.isCubemap = (out.typeName == "samplerCube");
    out.sourceColor = sourceColor;
    out.filter = annFilter;
    out.repeat = annRepeat;
    out.hintDefault = annHintDefault;
    return true;
}

/// Parse an #input directive.
static bool parseInputLine(const std::string& line, LshInputInfo& out) {
    // #input vec3 a_Position : POSITION 0
    std::regex re(R"(#input\s+(\w+)\s+(\w+)\s*:\s*(\w+)\s+(\d+))");
    std::smatch m;
    if (!std::regex_search(line, m, re)) return false;
    out.typeName = m[1].str();
    out.name = m[2].str();
    out.semantic = m[3].str();
    out.location = parseIntClamped(m[4].str(), 0, 64);
    return true;
}

/// Parse an #output directive.
static bool parseOutputLine(const std::string& line, LshOutputInfo& out) {
    // #output vec4 FragColor
    // #output vec4 FragColor : COLOR0
    std::regex re(R"(#output\s+(\w+)\s+(\w+)(?:\s*:\s*(\w+))?)");
    std::smatch m;
    if (!std::regex_search(line, m, re)) return false;
    out.typeName = m[1].str();
    out.name = m[2].str();
    out.semantic = m[3].matched ? m[3].str() : "";
    return true;
}

bool ShaderTranslator::parse(const std::string& lshSource, LshShaderIR& outIR, std::string& errorMsg) {
    outIR = LshShaderIR{};
    auto lines = splitLines(lshSource);
    size_t i = 0;

    while (i < lines.size()) {
        std::string stripped = trim(lines[i]);

        // #shader "Name"
        if (stripped.rfind("#shader", 0) == 0) {
            std::regex re(R"LSH(#shader\s+"([^"]*)")LSH");
            std::smatch m;
            if (std::regex_search(stripped, m, re)) {
                outIR.name = m[1].str();
            }
            ++i;
            continue;
        }

        // #description "..."
        if (stripped.rfind("#description", 0) == 0) {
            std::regex re(R"LSH(#description\s+"([^"]*)")LSH");
            std::smatch m;
            if (std::regex_search(stripped, m, re)) {
                outIR.description = m[1].str();
            }
            ++i;
            continue;
        }

        // #feature <name>
        if (stripped.rfind("#feature", 0) == 0) {
            std::regex re(R"(#feature\s+(\w+))");
            std::smatch m;
            if (std::regex_search(stripped, m, re)) {
                std::string feat = m[1].str();
                if (feat == "lighting") outIR.hasLighting = true;
                else if (feat == "shadows") { outIR.hasShadows = true; outIR.hasLighting = true; }
                else if (feat == "normal_mapping") outIR.hasNormalMapping = true;
                else if (feat == "fog") { outIR.hasFog = true; outIR.hasLighting = true; }
            }
            ++i;
            continue;
        }

        // #render_mode <tokens> (Gap B): pipeline render state, Godot-style tokens.
        if (stripped.rfind("#render_mode", 0) == 0) {
            LshRenderMode& rm = outIR.renderMode;
            rm.specified = true;
            std::string bodyTokens = std::regex_replace(stripped.substr(std::string("#render_mode").size()),
                                                        std::regex("//.*$"), "");
            std::regex tokRe(R"([^\s,]+)");
            for (auto it = std::sregex_iterator(bodyTokens.begin(), bodyTokens.end(), tokRe);
                 it != std::sregex_iterator(); ++it) {
                std::string tk = it->str();
                std::transform(tk.begin(), tk.end(), tk.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (tk == "cull_back" || tk == "cull_front" || tk == "cull_disabled") {
                    rm.cull = tk.substr(std::string("cull_").size());
                } else if (tk == "blend_mix" || tk == "blend_add" || tk == "blend_sub"
                           || tk == "blend_mul" || tk == "blend_premul" || tk == "blend_opaque") {
                    rm.blend = tk.substr(std::string("blend_").size());
                } else if (tk == "opaque") {
                    rm.blend = "opaque";
                } else if (tk == "depth_test_disabled") {
                    rm.depthTest = false;
                } else if (tk == "depth_draw_never") {
                    rm.depthWrite = false;
                } else if (tk == "depth_draw_always" || tk == "depth_draw_opaque"
                           || tk == "depth_prepass_alpha") {
                    rm.depthWrite = true;
                } else if (tk == "unshaded") {
                    rm.unshaded = true;
                }
            }
            ++i;
            continue;
        }

        // #begin properties ... #end properties
        if (stripped == "#begin properties") {
            ++i;
            while (i < lines.size()) {
                std::string pline = trim(lines[i]);
                if (pline == "#end properties") { ++i; break; }
                if (!pline.empty() && pline[0] != '/') {
                    LshUniformInfo u;
                    if (parseUniformLine(pline, u)) {
                        outIR.properties.push_back(u);
                    }
                }
                ++i;
            }
            continue;
        }

        // #begin shared ... #end shared
        if (stripped == "#begin shared") {
            ++i;
            std::ostringstream block;
            bool first = true;
            while (i < lines.size()) {
                std::string sline = trim(lines[i]);
                if (sline == "#end shared") { ++i; break; }
                if (!first) block << "\n";
                block << lines[i];
                first = false;
                ++i;
            }
            outIR.sharedCode = block.str();
            continue;
        }

        // #begin vertex ... #end vertex
        if (stripped == "#begin vertex") {
            ++i;
            std::ostringstream body;
            bool firstBody = true;
            while (i < lines.size()) {
                std::string vline = trim(lines[i]);
                if (vline == "#end vertex") { ++i; break; }
                if (vline.rfind("#input", 0) == 0) {
                    LshInputInfo inp;
                    if (parseInputLine(vline, inp)) {
                        outIR.vertex.inputs.push_back(inp);
                    }
                } else if (vline.rfind("#output", 0) == 0) {
                    LshOutputInfo out;
                    if (parseOutputLine(vline, out)) {
                        outIR.vertex.outputs.push_back(out);
                    }
                } else {
                    if (!firstBody) body << "\n";
                    body << lines[i];
                    firstBody = false;
                }
                ++i;
            }
            outIR.vertex.body = body.str();
            continue;
        }

        // #begin fragment ... #end fragment
        if (stripped == "#begin fragment") {
            ++i;
            std::ostringstream body;
            bool firstBody = true;
            while (i < lines.size()) {
                std::string fline = trim(lines[i]);
                if (fline == "#end fragment") { ++i; break; }
                if (fline.rfind("#input", 0) == 0) {
                    LshInputInfo inp;
                    if (parseInputLine(fline, inp)) {
                        outIR.fragment.inputs.push_back(inp);
                    }
                } else if (fline.rfind("#output", 0) == 0) {
                    LshOutputInfo out;
                    if (parseOutputLine(fline, out)) {
                        outIR.fragment.outputs.push_back(out);
                    }
                } else {
                    if (!firstBody) body << "\n";
                    body << lines[i];
                    firstBody = false;
                }
                ++i;
            }
            outIR.fragment.body = body.str();
            continue;
        }

        // #begin geometry ... #end geometry
        if (stripped == "#begin geometry") {
            ++i;
            outIR.hasGeometry = true;
            std::ostringstream body;
            bool firstBody = true;
            while (i < lines.size()) {
                std::string gline = trim(lines[i]);
                if (gline == "#end geometry") { ++i; break; }
                if (gline.rfind("#input", 0) == 0) {
                    LshInputInfo inp;
                    if (parseInputLine(gline, inp)) {
                        outIR.geometry.inputs.push_back(inp);
                    }
                } else if (gline.rfind("#output", 0) == 0) {
                    LshOutputInfo out;
                    if (parseOutputLine(gline, out)) {
                        outIR.geometry.outputs.push_back(out);
                    }
                } else if (gline.rfind("#topology", 0) == 0) {
                    std::smatch m;
                    if (std::regex_search(gline, m, std::regex(R"(#topology\s+(\w+))"))) {
                        outIR.geometry.topology = m[1].str();
                    }
                } else if (gline.rfind("#maxvertices", 0) == 0) {
                    std::smatch m;
                    if (std::regex_search(gline, m, std::regex(R"(#maxvertices\s+(\d+))"))) {
                        outIR.geometry.maxVertices = parseIntClamped(m[1].str(), 0, 256);
                    }
                } else {
                    if (!firstBody) body << "\n";
                    body << lines[i];
                    firstBody = false;
                }
                ++i;
            }
            outIR.geometry.body = body.str();
            continue;
        }

        ++i;
    }

    // Basic validation
    if (outIR.name.empty()) {
        errorMsg = "Missing #shader directive";
        return false;
    }
    if (outIR.vertex.body.empty()) {
        errorMsg = "Missing or empty #begin vertex block";
        return false;
    }
    if (outIR.fragment.body.empty()) {
        errorMsg = "Missing or empty #begin fragment block";
        return false;
    }

    // Built-in usage detection + system-uniform injection (mirror of the Python
    // transpiler's _post_process). TIME (Gap E) becomes the engine-fed u_Time float,
    // injected as a regular property so every emitter declares and routes it like any
    // uniform. FRONT_FACING (Gap D) flags the fragment entry so HLSL/Metal add the
    // system input.
    {
        const std::string both = outIR.vertex.body + "\n" + outIR.fragment.body;
        outIR.usesTime = std::regex_search(both, std::regex(R"(\bTIME\b)"));
        outIR.usesFrontFacing =
            std::regex_search(outIR.fragment.body, std::regex(R"(\bFRONT_FACING\b)"));
        outIR.usesPrimaryShadow =
            std::regex_search(outIR.fragment.body, std::regex(R"(\bPRIMARY_SHADOW_ATTENUATION\b)"));
        if (outIR.usesTime) {
            bool present = false;
            for (const auto& u : outIR.properties) {
                if (u.name == "u_Time") { present = true; break; }
            }
            if (!present) {
                LshUniformInfo timeU;
                timeU.typeName = "float";
                timeU.name = "u_Time";
                timeU.defaultValue = "0.0";
                outIR.properties.push_back(timeU);
            }
        }
    }
    return true;
}

// =============================================================================
// Validate
// =============================================================================

bool ShaderTranslator::validate(const std::string& lshSource, std::string& errorMsg) {
    LshShaderIR ir;
    return parse(lshSource, ir, errorMsg);
}

// =============================================================================
// Extension Helper
// =============================================================================

const char* ShaderTranslator::getShaderExtension(GraphicsBackend backend) {
    switch (backend) {
        case GraphicsBackend::Metal:     return "metal";
        case GraphicsBackend::OpenGL:    return "glsl";
        case GraphicsBackend::WebGL:     return "glsl";
        case GraphicsBackend::Vulkan:    return "glsl";
        case GraphicsBackend::DirectX11: return "hlsl";
        case GraphicsBackend::DirectX12: return "hlsl";
        default:                         return "txt";
    }
}

// =============================================================================
// Top-Level Translate
// =============================================================================

ShaderTranslatorResult ShaderTranslator::translate(const std::string& lshSource, GraphicsBackend backend) {
    ShaderTranslatorResult result;

    LshShaderIR ir;
    if (!parse(lshSource, ir, result.errorMessage)) {
        result.success = false;
        return result;
    }

    // Geometry stages are only expressible on DirectX11/DirectX12 (HLSL). On other
    // backends, fail loudly rather than silently dropping the stage and emitting a
    // fragment shader that references undeclared geometry varyings.
    if (ir.hasGeometry && !isBackendHLSL(backend)) {
        result.success = false;
        result.errorMessage = "Geometry-stage shaders are only supported on DirectX11/DirectX12";
        return result;
    }

    switch (backend) {
        case GraphicsBackend::OpenGL:
            result.vertexSource   = emitGLSLVertex(ir);
            result.fragmentSource = emitGLSLFragment(ir);
            break;
        case GraphicsBackend::WebGL:
            result.vertexSource   = emitGLSLESVertex(ir);
            result.fragmentSource = emitGLSLESFragment(ir);
            break;
        case GraphicsBackend::Vulkan:
            result.vertexSource   = emitGLSL450Vertex(ir);
            result.fragmentSource = emitGLSL450Fragment(ir);
            break;
        case GraphicsBackend::DirectX11:
        case GraphicsBackend::DirectX12:
            result.vertexSource   = emitHLSLVertex(ir, backend);
            result.fragmentSource = emitHLSLFragment(ir, backend);
            if (ir.hasGeometry) {
                result.geometrySource = emitHLSLGeometry(ir, backend);
            }
            break;
        case GraphicsBackend::Metal:
            result.combinedSource = emitMetal(ir);
            break;
        default:
            result.errorMessage = "Unsupported graphics backend";
            result.success = false;
            return result;
    }

    result.success = true;
    result.usesLighting = ir.hasLighting;
    result.usesShadows = ir.hasShadows;
    result.usesNormalMapping = ir.hasNormalMapping;
    result.usesFog = ir.hasFog;
    result.usesTime = ir.usesTime;

    // Expose sampler hints (Gap G) and render state (Gap B) to the renderer. The slot is
    // the texture's binding index in declaration order (t0, t1, ...), matching how the
    // emitters assign texture registers.
    int texIndex = 0;
    for (const auto& u : ir.properties) {
        if (!(u.isTexture || u.isCubemap)) continue;
        const int slot = texIndex++;
        if (!u.sourceColor && u.filter.empty() && u.repeat.empty() && u.hintDefault.empty())
            continue;
        LshSamplerConfig cfg;
        cfg.name = u.name;
        cfg.slot = slot;
        cfg.sourceColor = u.sourceColor;
        cfg.filter = u.filter;
        cfg.repeat = u.repeat;
        cfg.hintDefault = u.hintDefault;
        result.samplerConfigs.push_back(cfg);
    }
    result.renderMode = ir.renderMode;
    return result;
}

// =============================================================================
// Type Conversion
// =============================================================================

std::string ShaderTranslator::toHLSLType(const std::string& glslType) {
    auto it = kGLSLToHLSLTypes.find(glslType);
    return (it != kGLSLToHLSLTypes.end()) ? it->second : glslType;
}

std::string ShaderTranslator::toMetalType(const std::string& glslType) {
    auto it = kGLSLToMetalTypes.find(glslType);
    return (it != kGLSLToMetalTypes.end()) ? it->second : glslType;
}

std::string ShaderTranslator::replaceHLSLTypes(const std::string& code) {
    std::string result = code;
    for (auto& [glsl, hlsl] : kGLSLToHLSLTypes) {
        // Skip sampler types - handled specially
        if (glsl == "sampler2D" || glsl == "samplerCube" || glsl == "sampler2DShadow")
            continue;
        result = regexReplaceWord(result, glsl, hlsl);
    }
    result = expandScalarConstructors(result);
    return result;
}

std::string ShaderTranslator::replaceMetalTypes(const std::string& code) {
    std::string result = code;
    for (auto& [glsl, mtl] : kGLSLToMetalTypes) {
        // Skip sampler types - handled specially through function parameters
        if (glsl == "sampler2D" || glsl == "samplerCube" || glsl == "sampler2DShadow")
            continue;
        result = regexReplaceWord(result, glsl, mtl);
    }
    result = expandScalarConstructors(result);
    return result;
}

std::string ShaderTranslator::replaceHLSLFunctions(const std::string& code) {
    std::string result = code;
    for (auto& [glsl, hlsl] : kHLSLFuncReplacements) {
        result = regexReplaceFuncCall(result, glsl, hlsl);
    }
    return result;
}

std::string ShaderTranslator::replaceMetalFunctions(const std::string& code) {
    std::string result = code;
    for (auto& [glsl, mtl] : kMetalFuncReplacements) {
        result = regexReplaceFuncCall(result, glsl, mtl);
    }
    return result;
}

// =============================================================================
// HLSL Helpers
// =============================================================================

int ShaderTranslator::getTypeSize(const std::string& typeName) {
    static const std::unordered_map<std::string, int> sizes = {
        {"float", 4}, {"int", 4}, {"uint", 4}, {"bool", 4},
        {"vec2", 8}, {"float2", 8}, {"ivec2", 8}, {"int2", 8},
        {"vec3", 12}, {"float3", 12}, {"ivec3", 12},
        {"vec4", 16}, {"float4", 16}, {"ivec4", 16},
        {"mat3", 48}, {"float3x3", 48},   // 3 * 16 (padded rows)
        {"mat4", 64}, {"float4x4", 64},
    };
    auto it = sizes.find(typeName);
    return (it != sizes.end()) ? it->second : 16;
}

int ShaderTranslator::getTypeAlignment(const std::string& typeName) {
    if (typeName == "mat4" || typeName == "mat3" ||
        typeName == "float4x4" || typeName == "float3x3")
        return 16;
    if (typeName == "vec4" || typeName == "float4" ||
        typeName == "ivec4" || typeName == "int4" ||
        typeName == "uvec4" || typeName == "uint4")
        return 16;
    if (typeName == "vec3" || typeName == "float3" ||
        typeName == "ivec3" || typeName == "int3")
        return 16;  // vec3 aligns to 16 in std140/HLSL
    if (typeName == "vec2" || typeName == "float2" ||
        typeName == "ivec2" || typeName == "int2")
        return 8;
    return 4;
}

std::string ShaderTranslator::hlslSemantic(const std::string& semantic) {
    auto it = kHLSLSemanticMap.find(semantic);
    return (it != kHLSLSemanticMap.end()) ? it->second : semantic;
}

std::string ShaderTranslator::buildHLSLCBuffer(const LshShaderIR& ir, GraphicsBackend backend) {
    // Uniforms that go in a dedicated cbuffer (register b2) because the engine writes
    // them to a separate GPU buffer rather than the material/push-constant buffer.
    static const std::unordered_set<std::string> kSeparateCBuffer = {"u_BoneTransforms"};

    // DirectX12 always uses the fixed 352-byte PerObjectUniforms prefix: the engine
    // writes that prefix at bindPipeline and starts dynamic uniform allocation at 352.
    // DirectX11 uses the full layout whenever the shader declares any standard
    // per-object field, because those shaders receive the same raw pushConstants
    // upload (PerObjectUniforms from RenderWorld::executeBatch, or the
    // prefix-compatible ShadowUniforms from the shadow pass). Shaders that declare
    // none of these (skybox, text) stay compact and are driven by setUniform*.
    bool usesFullLayout = (backend == GraphicsBackend::DirectX12);
    if (!usesFullLayout) {
        for (auto& u : ir.properties) {
            if (!u.isTexture && !u.isCubemap && kHLSLFullLayoutTriggers.count(u.name)) {
                usesFullLayout = true;
                break;
            }
        }
    }

    std::ostringstream out;
    std::unordered_set<std::string> emitted;
    out << "cbuffer PushConstants : register(b0)\n{\n";

    if (usesFullLayout) {
        static const std::pair<const char*, const char*> kFixedLayout[] = {
            {"float4x4", "u_ViewProjection"}, {"float4x4", "u_Model"}, {"float4x4", "u_NormalMatrix"},
            {"float4", "u_TintColor"}, {"int", "u_UseTexture"}, {"float", "u_AlphaCutoff"},
            {"float", "_pad1"}, {"float", "_pad2"}, {"float4", "u_UVRect"}, {"float4", "u_TextureFlags"},
            {"float4", "u_MaterialParams1"}, {"float4", "u_MaterialParams2"}, {"float4", "u_CameraPosition"},
            {"float4", "u_AlbedoColor"}, {"float4", "u_EmissiveColor"}, {"int", "u_ReceiveShadow"},
            {"float", "_pad3"}, {"float", "_pad4"}, {"float", "_pad5"},
        };

        // Resolve slot aliases: UI/2D shaders declare semantic names that the engine
        // packs into the u_TextureFlags (@240) / u_MaterialParams1 (@256) slots. Emit
        // the alias name at the slot so the pushConstants blob delivers it.
        std::unordered_set<std::string> declared;
        for (auto& u : ir.properties) {
            if (!u.isTexture && !u.isCubemap) declared.insert(u.name);
        }
        std::string texflagsAlias;
        if (declared.find("u_TextureFlags") == declared.end()) {
            for (auto& a : kHLSLTextureFlagsSlotAliases) {
                if (declared.count(a)) { texflagsAlias = a; break; }
            }
        }
        bool sizeAlias = declared.count("u_Size") &&
                         declared.find("u_MaterialParams1") == declared.end();

        for (auto& [t, n] : kFixedLayout) {
            std::string name = n;
            if (name == "u_TextureFlags" && !texflagsAlias.empty()) {
                out << "    float4 " << texflagsAlias << ";\n";
                emitted.insert(texflagsAlias);
                emitted.insert(name);
            } else if (name == "u_MaterialParams1" && sizeAlias) {
                // u_Size is a float2 in the first 8 bytes of the 16-byte slot; pad the
                // rest so u_MaterialParams2 stays at offset 272.
                out << "    float2 u_Size;\n";
                out << "    float2 _padSize;\n";
                emitted.insert("u_Size");
                emitted.insert(name);
            } else {
                out << "    " << t << " " << name << ";\n";
                emitted.insert(name);
            }
        }
    }

    // Remaining declared uniforms (full layout: after the fixed prefix -> offsets >= 352;
    // compact layout: all declared uniforms in declaration order). Bone uniforms go in b2.
    for (auto& u : ir.properties) {
        if (u.isTexture || u.isCubemap) continue;
        if (kSeparateCBuffer.count(u.name)) continue;
        if (emitted.count(u.name)) continue;
        std::string ht = toHLSLType(u.typeName);
        out << "    " << ht << " " << u.name;
        if (u.arraySize > 0) out << "[" << u.arraySize << "]";
        out << ";\n";
        emitted.insert(u.name);
    }
    out << "};";

    std::vector<const LshUniformInfo*> separate;
    for (auto& u : ir.properties) {
        if (u.isTexture || u.isCubemap) continue;
        if (kSeparateCBuffer.count(u.name)) separate.push_back(&u);
    }
    if (!separate.empty()) {
        out << "\n\ncbuffer BoneData : register(b2)\n{\n";
        for (auto* u : separate) {
            std::string ht = toHLSLType(u->typeName);
            out << "    " << ht << " " << u->name;
            if (u->arraySize > 0) out << "[" << u->arraySize << "]";
            out << ";\n";
        }
        out << "};";
    }

    return out.str();
}

std::string ShaderTranslator::buildHLSLTextures(const LshShaderIR& ir) {
    std::ostringstream out;
    int slot = 0;
    for (auto& u : ir.properties) {
        if (u.isTexture) {
            out << "Texture2D " << u.name << " : register(t" << slot << ");\n";
            out << "SamplerState " << u.name << "_sampler : register(s" << slot << ");\n";
            ++slot;
        } else if (u.isCubemap) {
            out << "TextureCube " << u.name << " : register(t" << slot << ");\n";
            out << "SamplerState " << u.name << "_sampler : register(s" << slot << ");\n";
            ++slot;
        }
    }
    return out.str();
}

// =============================================================================
// Macro Expansion
// =============================================================================

std::string ShaderTranslator::expandMacros(const std::string& code, GraphicsBackend backend) {
    std::string result = code;

    // DISCARD
    result = std::regex_replace(result, std::regex(R"(\bDISCARD\s*;)"), "discard;");

    // TIME (Gap E): engine-fed per-frame seconds. Injected as the u_Time uniform
    // (see parse() post-process); rewrite here, before uniform-access prefixing, so the
    // Vulkan/Metal passes treat it like any other material uniform.
    result = regexReplaceWord(result, "TIME", "u_Time");

    // FRONT_FACING (Gap D): true for front-facing fragments.
    if (isBackendGLSL(backend) || backend == GraphicsBackend::Vulkan) {
        result = regexReplaceWord(result, "FRONT_FACING", "gl_FrontFacing");
    } else if (isBackendHLSL(backend)) {
        result = regexReplaceWord(result, "FRONT_FACING", "input.lupine_frontFacing");
    } else if (backend == GraphicsBackend::Metal) {
        result = regexReplaceWord(result, "FRONT_FACING", "lupine_frontFacing");
    }

    // FRAG_COORD
    if (isBackendGLSL(backend) || backend == GraphicsBackend::Vulkan) {
        result = regexReplaceWord(result, "FRAG_COORD", "gl_FragCoord");
    } else if (isBackendHLSL(backend)) {
        result = regexReplaceWord(result, "FRAG_COORD", "input.position");
    } else if (backend == GraphicsBackend::Metal) {
        result = regexReplaceWord(result, "FRAG_COORD", "in.position");
    }

    // INSTANCE_ID
    if (isBackendGLSL(backend) || backend == GraphicsBackend::Vulkan) {
        result = regexReplaceWord(result, "INSTANCE_ID", "gl_InstanceID");
    } else if (isBackendHLSL(backend)) {
        result = regexReplaceWord(result, "INSTANCE_ID", "input.instanceID");
    } else if (backend == GraphicsBackend::Metal) {
        result = regexReplaceWord(result, "INSTANCE_ID", "iid");
    }

    // VERTEX_ID
    if (isBackendGLSL(backend) || backend == GraphicsBackend::Vulkan) {
        result = regexReplaceWord(result, "VERTEX_ID", "gl_VertexID");
    } else if (isBackendHLSL(backend)) {
        result = regexReplaceWord(result, "VERTEX_ID", "input.vertexID");
    } else if (backend == GraphicsBackend::Metal) {
        result = regexReplaceWord(result, "VERTEX_ID", "vid");
    }

    return result;
}

std::string ShaderTranslator::expandSampleMacros(const std::string& code, GraphicsBackend backend) {
    std::string result = code;

    if (isBackendGLSL(backend) || backend == GraphicsBackend::Vulkan) {
        // SAMPLE_LOD(t, uv, lod) -> textureLod(t, uv, lod)
        result = std::regex_replace(result,
            std::regex(R"(SAMPLE_LOD\s*\(\s*(\w+)\s*,\s*)"),
            "textureLod($1, ");
        // SAMPLE_CUBE(t, d) -> texture(t, d)
        result = std::regex_replace(result,
            std::regex(R"(SAMPLE_CUBE\s*\(\s*(\w+)\s*,\s*)"),
            "texture($1, ");
        // SAMPLE(t, uv) -> texture(t, uv)
        result = std::regex_replace(result,
            std::regex(R"(SAMPLE\s*\(\s*(\w+)\s*,\s*)"),
            "texture($1, ");

    } else if (isBackendHLSL(backend)) {
        // SAMPLE_LOD(t, uv, lod) -> t.SampleLevel(t_sampler, uv, lod)
        result = std::regex_replace(result,
            std::regex(R"(SAMPLE_LOD\s*\(\s*(\w+)\s*,\s*([^,]+)\s*,\s*([^)]+)\))"),
            "$1.SampleLevel($1_sampler, $2, $3)");
        // SAMPLE_CUBE(t, d) -> t.Sample(t_sampler, d)
        result = std::regex_replace(result,
            std::regex(R"(SAMPLE_CUBE\s*\(\s*(\w+)\s*,\s*([^)]+)\))"),
            "$1.Sample($1_sampler, $2)");
        // SAMPLE(t, uv) -> t.Sample(t_sampler, uv)
        result = std::regex_replace(result,
            std::regex(R"(SAMPLE\s*\(\s*(\w+)\s*,\s*([^)]+)\))"),
            "$1.Sample($1_sampler, $2)");

    } else if (backend == GraphicsBackend::Metal) {
        // SAMPLE_LOD(t, uv, lod) -> t.sample(t_sampler, uv, level(lod))
        result = std::regex_replace(result,
            std::regex(R"(SAMPLE_LOD\s*\(\s*(\w+)\s*,\s*([^,]+)\s*,\s*([^)]+)\))"),
            "$1.sample($1_sampler, $2, level($3))");
        // SAMPLE_CUBE(t, d) -> t.sample(t_sampler, d)
        result = std::regex_replace(result,
            std::regex(R"(SAMPLE_CUBE\s*\(\s*(\w+)\s*,\s*([^)]+)\))"),
            "$1.sample($1_sampler, $2)");
        // SAMPLE(t, uv) -> t.sample(t_sampler, uv)
        result = std::regex_replace(result,
            std::regex(R"(SAMPLE\s*\(\s*(\w+)\s*,\s*([^)]+)\))"),
            "$1.sample($1_sampler, $2)");
    }

    return result;
}

/// Find the matching closing paren for an open paren at position `start`.
/// Returns std::string::npos if not found.
static size_t findMatchingParen(const std::string& s, size_t start) {
    int depth = 1;
    for (size_t i = start + 1; i < s.size(); ++i) {
        if (s[i] == '(') ++depth;
        else if (s[i] == ')') { --depth; if (depth == 0) return i; }
    }
    return std::string::npos;
}

/// Find the comma separating two top-level arguments inside balanced parens.
/// `start` points to the char after the opening '(', `end` points to the closing ')'.
static size_t findTopLevelComma(const std::string& s, size_t start, size_t end) {
    int depth = 0;
    for (size_t i = start; i < end; ++i) {
        if (s[i] == '(') ++depth;
        else if (s[i] == ')') --depth;
        else if (s[i] == ',' && depth == 0) return i;
    }
    return std::string::npos;
}

/// Expand all occurrences of MACRO(...) with balanced parentheses.
/// Handles nested macros by recursively expanding arguments.
/// `macroName` is e.g. "MUL" or "MAT3".
/// `twoArg` controls whether we expect (a, b) or (a).
/// `fmt2` is called with (arg1, arg2) for two-arg macros.
/// `fmt1` is called with (arg) for single-arg macros.
static std::string expandBalancedMacro(
    const std::string& code,
    const std::string& macroName,
    bool twoArg,
    std::function<std::string(const std::string&, const std::string&)> fmt2,
    std::function<std::string(const std::string&)> fmt1
) {
    std::string result;
    std::regex macroRe("\\b" + macroName + "\\s*\\(");

    // We process iteratively, restarting after each expansion to handle nesting
    std::string current = code;
    bool changed = true;
    int maxIterations = 32;  // safety limit for deeply nested macros

    while (changed && maxIterations-- > 0) {
        changed = false;
        result.clear();

        auto it = std::sregex_iterator(current.begin(), current.end(), macroRe);
        auto end = std::sregex_iterator();

        if (it == end) {
            return current;  // no more matches
        }

        size_t lastEnd = 0;
        for (; it != end; ++it) {
            auto& match = *it;
            size_t matchStart = match.position();

            // Skip if this match overlaps with already-processed region
            if (matchStart < lastEnd) continue;

            size_t openParen = matchStart + match.length() - 1;
            size_t closeParen = findMatchingParen(current, openParen);
            if (closeParen == std::string::npos) continue;

            result += current.substr(lastEnd, matchStart - lastEnd);

            size_t argsStart = openParen + 1;

            if (twoArg) {
                size_t comma = findTopLevelComma(current, argsStart, closeParen);
                if (comma != std::string::npos) {
                    std::string arg1 = trim(current.substr(argsStart, comma - argsStart));
                    std::string arg2 = trim(current.substr(comma + 1, closeParen - comma - 1));
                    result += fmt2(arg1, arg2);
                    changed = true;
                } else {
                    result += current.substr(matchStart, closeParen + 1 - matchStart);
                }
            } else {
                std::string inner = trim(current.substr(argsStart, closeParen - argsStart));
                result += fmt1(inner);
                changed = true;
            }

            lastEnd = closeParen + 1;
        }

        result += current.substr(lastEnd);
        current = result;
    }

    return current;
}

std::string ShaderTranslator::expandMulMacros(const std::string& code, GraphicsBackend backend) {
    std::string result = code;

    if (isBackendHLSL(backend)) {
        // MUL(a, b) -> mul(a, b)  (HLSL has native mul, so simple replacement works)
        result = std::regex_replace(result, std::regex(R"(\bMUL\s*\()"), "mul(");
        // MAT3(m) -> (float3x3)m
        result = expandBalancedMacro(result, "MAT3", false, nullptr,
            [](const std::string& a) { return "(float3x3)" + a; });

    } else if (backend == GraphicsBackend::Metal) {
        // MUL(a, b) -> (a * b) with balanced paren support for nested MUL
        result = expandBalancedMacro(result, "MUL", true,
            [](const std::string& a, const std::string& b) { return "(" + a + " * " + b + ")"; },
            nullptr);
        // MAT3(m) -> float3x3(m[0].xyz, m[1].xyz, m[2].xyz)
        result = expandBalancedMacro(result, "MAT3", false, nullptr,
            [](const std::string& a) { return "float3x3(" + a + "[0].xyz, " + a + "[1].xyz, " + a + "[2].xyz)"; });

    } else {
        // GLSL: MUL(a, b) -> (a * b) with balanced paren support for nested MUL
        result = expandBalancedMacro(result, "MUL", true,
            [](const std::string& a, const std::string& b) { return "(" + a + " * " + b + ")"; },
            nullptr);
        // MAT3(m) -> mat3(m)
        result = expandBalancedMacro(result, "MAT3", false, nullptr,
            [](const std::string& a) { return "mat3(" + a + ")"; });
    }

    return result;
}

// Expand GLSL mod(x, y) -> (x - y * floor(x / y)) for HLSL/Metal, whose native fmod
// truncates toward zero while GLSL mod floors. A naive mod->fmod rename is wrong for
// negative operands (e.g. the polygon SDF wraps an angle from atan2() that is negative
// on the lower half, giving a bad angle and jagged edges). Balanced-paren aware.
static std::string expandModMacros(const std::string& code) {
    return expandBalancedMacro(code, "mod", true,
        [](const std::string& x, const std::string& y) {
            return "((" + x + ") - (" + y + ") * floor((" + x + ") / (" + y + ")))";
        },
        nullptr);
}

// Expand TEXTURE_SIZE(tex) / TEXTURE_PIXEL_SIZE(tex) (Gap F). HLSL has no inline size
// intrinsic (GetDimensions writes out-params), so it routes through the
// lupine_textureSize() helper emitted by the HLSL emitter. TEXTURE_PIXEL_SIZE expands
// to 1.0 / TEXTURE_SIZE (Godot's TEXTURE_PIXEL_SIZE).
static std::string expandTextureSizeMacros(const std::string& code, GraphicsBackend backend) {
    auto texSize = [backend](const std::string& tex) -> std::string {
        if (backend == GraphicsBackend::Metal)
            return "float2(" + tex + ".get_width(), " + tex + ".get_height())";
        if (isBackendHLSL(backend))
            return "lupine_textureSize(" + tex + ")";
        return "vec2(textureSize(" + tex + ", 0))";
    };
    const std::string one = (backend == GraphicsBackend::Metal || isBackendHLSL(backend))
        ? "float2(1.0, 1.0)" : "vec2(1.0)";
    std::string result = expandBalancedMacro(code, "TEXTURE_PIXEL_SIZE", false, nullptr,
        [&](const std::string& a) { return "(" + one + " / " + texSize(a) + ")"; });
    result = expandBalancedMacro(result, "TEXTURE_SIZE", false, nullptr,
        [&](const std::string& a) { return texSize(a); });
    return result;
}

// sRGB->linear decode helper for @source_color samplers (Gap G). The engine loads
// custom-shader textures as UNORM (not _SRGB), so the decode happens in the shader.
// Gamma-2.2 approximation (matches the engine's gamma convention); alpha stays linear.
static std::string genSrgbHelper(GraphicsBackend backend) {
    if (isBackendHLSL(backend))
        return "float4 lupine_srgb_to_linear(float4 c) {\n"
               "    return float4(pow(max(c.rgb, float3(0.0, 0.0, 0.0)), "
               "float3(2.2, 2.2, 2.2)), c.a);\n}";
    if (backend == GraphicsBackend::Metal)
        return "float4 lupine_srgb_to_linear(float4 c) {\n"
               "    return float4(pow(max(c.rgb, float3(0.0)), float3(2.2)), c.a);\n}";
    return "vec4 lupine_srgb_to_linear(vec4 c) {\n"
           "    return vec4(pow(max(c.rgb, vec3(0.0)), vec3(2.2)), c.a);\n}";
}

// Expand PRIMARY_SHADOW_ATTENUATION(wPos, N) (Gap A). With #feature shadows it routes
// through lupine_primary_shadow(); without it, degrades to fully-lit (1.0).
static std::string expandPrimaryShadow(const std::string& code, const LshShaderIR& ir) {
    if (code.find("PRIMARY_SHADOW_ATTENUATION") == std::string::npos) return code;
    if (ir.hasShadows) {
        return expandBalancedMacro(code, "PRIMARY_SHADOW_ATTENUATION", true,
            [](const std::string& a, const std::string& b) {
                return "lupine_primary_shadow(" + a + ", " + b + ")";
            }, nullptr);
    }
    return expandBalancedMacro(code, "PRIMARY_SHADOW_ATTENUATION", true,
        [](const std::string&, const std::string&) { return std::string("1.0"); }, nullptr);
}

// Wrap SAMPLE*/SAMPLE_LOD/SAMPLE_CUBE reads of @source_color textures in
// lupine_srgb_to_linear(...) (Gap G). lupine_srgb_to_linear survives all downstream
// transforms and the inner SAMPLE is expanded normally afterward.
static std::string wrapSourceColorSamples(const std::string& codeIn, const LshShaderIR& ir) {
    std::unordered_set<std::string> names;
    for (const auto& u : ir.properties) {
        if ((u.isTexture || u.isCubemap) && u.sourceColor) names.insert(u.name);
    }
    if (names.empty()) return codeIn;

    std::string code = codeIn;
    const char* macros[] = {"SAMPLE_LOD", "SAMPLE_CUBE", "SAMPLE"};
    for (const char* macro : macros) {
        std::regex pat(std::string("\\b") + macro + "\\s*\\(");
        size_t searchStart = 0;
        while (searchStart < code.size()) {
            std::smatch m;
            std::string sub = code.substr(searchStart);
            if (!std::regex_search(sub, m, pat)) break;
            size_t matchStart = searchStart + static_cast<size_t>(m.position(0));
            size_t matchEnd = matchStart + static_cast<size_t>(m.length(0));
            size_t openParen = matchEnd - 1;
            size_t closeParen = findMatchingParen(code, openParen);
            if (closeParen == std::string::npos) break;
            size_t comma = findTopLevelComma(code, openParen + 1, closeParen);
            size_t argEnd = (comma == std::string::npos) ? closeParen : comma;
            std::string firstArg = trim(code.substr(openParen + 1, argEnd - (openParen + 1)));
            if (names.count(firstArg)) {
                std::string whole = code.substr(matchStart, closeParen + 1 - matchStart);
                std::string replacement = "lupine_srgb_to_linear(" + whole + ")";
                code = code.substr(0, matchStart) + replacement + code.substr(closeParen + 1);
                searchStart = matchStart + std::string("lupine_srgb_to_linear(").size() + whole.size();
            } else {
                searchStart = matchEnd;
            }
        }
    }
    return code;
}

// =============================================================================
// Uniform / IO Access Transforms
// =============================================================================

std::string ShaderTranslator::transformUniformAccess(const std::string& code, GraphicsBackend backend,
                                                     const LshShaderIR& ir) {
    std::string result = code;

    if (ir.hasLighting) {
        if (isBackendHLSL(backend)) {
            result = std::regex_replace(result, std::regex(R"(\bu_Lights\.)"), "");
        } else if (backend == GraphicsBackend::Metal) {
            result = std::regex_replace(result, std::regex(R"(\bu_Lights\.)"), "lightData.");
        }
    }

    if (backend == GraphicsBackend::Vulkan) {
        // Push constant uniforms -> pc.name, bone uniforms -> bones.name, material -> material.name
        for (auto& u : ir.properties) {
            if (u.isTexture || u.isCubemap) continue;
            if (kPushConstantNames.count(u.name)) {
                result = regexReplaceUniform(result, u.name, "pc." + u.name);
            } else if (kSeparateBufferNames.count(u.name)) {
                result = regexReplaceUniform(result, u.name, "bones." + u.name);
            } else {
                result = regexReplaceUniform(result, u.name, "material." + u.name);
            }
        }
    } else if (backend == GraphicsBackend::Metal) {
        // All non-texture uniforms -> uniforms.name
        for (auto& u : ir.properties) {
            if (u.isTexture || u.isCubemap) continue;
            result = regexReplaceUniform(result, u.name, "uniforms." + u.name);
        }
    }
    // HLSL: uniforms in cbuffer are accessed directly - no transform needed.

    return result;
}

std::string ShaderTranslator::transformIOAccess(const std::string& code, GraphicsBackend backend,
                                                const LshShaderIR& ir, const std::string& stage) {
    std::string result = code;

    // Geometry stage handles I/O via GS_* macros — skip the standard transforms.
    if (stage == "geometry") {
        return result;
    }

    // Fragment inputs come from the geometry stage's outputs when one exists, else the vertex outputs.
    const std::vector<LshOutputInfo>& fragInputs =
        ir.hasGeometry ? ir.geometry.outputs : ir.vertex.outputs;

    if (isBackendHLSL(backend)) {
        if (stage == "vertex") {
            // Inputs: a_Name -> input.a_Name
            for (auto& inp : ir.vertex.inputs) {
                result = regexReplaceWord(result, inp.name, "input." + inp.name);
            }
            // Outputs: v_Name -> output.v_Name
            for (auto& out : ir.vertex.outputs) {
                result = regexReplaceWord(result, out.name, "output." + out.name);
            }
            // VERTEX_OUTPUT -> output.position
            result = regexReplaceWord(result, "VERTEX_OUTPUT", "output.position");
        } else {
            for (auto& out : fragInputs) {
                if (out.name == "position") continue;
                result = regexReplaceWord(result, out.name, "input." + out.name);
            }
            // Fragment outputs stay as-is (mapped in return statement)
        }
    } else if (backend == GraphicsBackend::Metal) {
        if (stage == "vertex") {
            for (auto& inp : ir.vertex.inputs) {
                result = regexReplaceWord(result, inp.name, "in." + inp.name);
            }
            for (auto& out : ir.vertex.outputs) {
                result = regexReplaceWord(result, out.name, "out." + out.name);
            }
            result = regexReplaceWord(result, "VERTEX_OUTPUT", "out.position");
        } else {
            for (auto& out : fragInputs) {
                if (out.name == "position") continue;
                result = regexReplaceWord(result, out.name, "in." + out.name);
            }
            // Fragment outputs stay as local variable, returned at end
        }
    } else {
        // GLSL: VERTEX_OUTPUT -> gl_Position
        result = regexReplaceWord(result, "VERTEX_OUTPUT", "gl_Position");
    }

    return result;
}

// =============================================================================
// Geometry shader (GS_*) macro expansion — HLSL only
// =============================================================================

static std::string expandGSMacros(const std::string& code, const LshShaderIR& ir) {
    std::string result = code;
    result = std::regex_replace(result, std::regex(R"(GS_INPUT_POSITION\s*\(\s*(\d+)\s*\))"), "input[$1].position");
    result = std::regex_replace(result, std::regex(R"(GS_INPUT\s*\(\s*(\d+)\s*,\s*(\w+)\s*\))"), "input[$1].$2");
    result = regexReplaceWord(result, "GS_POSITION", "output.position");
    result = std::regex_replace(result, std::regex(R"(\bGS_EMIT\s*;)"), "outputStream.Append(output);");
    result = std::regex_replace(result, std::regex(R"(\bGS_RESTART\s*;)"), "outputStream.RestartStrip();");
    for (auto& out : ir.geometry.outputs) {
        if (out.name == "position") continue;
        std::regex re("(^|[^.A-Za-z0-9_])" + out.name + R"(\b\s*=)");
        result = std::regex_replace(result, re, "$1output." + out.name + " =");
    }
    return result;
}

// =============================================================================
// Body Transformation (orchestrator)
// =============================================================================

std::string ShaderTranslator::transformBody(const std::string& body, GraphicsBackend backend,
                                            const LshShaderIR& ir, const std::string& stage) {
    std::string code = body;

    // 0. Wrap @source_color sampler reads (Gap G) before macro expansion; track whether
    //    a wrap was inserted so the helper definition can be prepended after transforms.
    std::string wrapped = wrapSourceColorSamples(code, ir);
    const bool addedSrgb = (wrapped != code);
    code = wrapped;

    // 1. Expand cross-platform macros
    code = expandMacros(code, backend);

    // 1c. Expand TEXTURE_SIZE / TEXTURE_PIXEL_SIZE (Gap F)
    code = expandTextureSizeMacros(code, backend);

    // 1d. Expand PRIMARY_SHADOW_ATTENUATION (Gap A)
    code = expandPrimaryShadow(code, ir);

    // 1b. Geometry stage: expand GS_* macros (HLSL only)
    if (stage == "geometry" && isBackendHLSL(backend)) {
        code = expandGSMacros(code, ir);
    }

    // 2. Expand SAMPLE/SAMPLE_LOD/SAMPLE_CUBE
    code = expandSampleMacros(code, backend);

    // 3. Expand MUL/MAT3
    code = expandMulMacros(code, backend);

    // 4. Transform I/O variable access
    code = transformIOAccess(code, backend, ir, stage);

    // 5. Transform uniform access
    code = transformUniformAccess(code, backend, ir);

    // 6. Replace types (HLSL/Metal only)
    if (isBackendHLSL(backend)) {
        code = replaceHLSLTypes(code);
        code = replaceHLSLFunctions(code);
        code = expandModMacros(code);

        // In HLSL, 'const' at global scope is an external constant (defaults to 0).
        // Must use 'static const' for compile-time literal values.
        code = hlslStaticConst(code);

        // HLSL fragment: getNormalFromMap() -> getNormalFromMap(input)
        if (stage == "fragment" && ir.hasNormalMapping) {
            code = std::regex_replace(code,
                std::regex(R"(\bgetNormalFromMap\s*\(\s*\))"), "getNormalFromMap(input)");
        }
    } else if (backend == GraphicsBackend::Metal) {
        code = replaceMetalTypes(code);
        code = replaceMetalFunctions(code);
        code = expandModMacros(code);

        // Metal fragment: getNormalFromMap() -> getNormalFromMap(in, uniforms, u_NormalTexture, u_NormalTexture_sampler)
        if (stage == "fragment" && ir.hasNormalMapping) {
            code = std::regex_replace(code,
                std::regex(R"(\bgetNormalFromMap\s*\(\s*\))"),
                "getNormalFromMap(in, uniforms, u_NormalTexture, u_NormalTexture_sampler)");
        }
    }

    // Prepend the sRGB helper (already backend-correct) ahead of the transformed body so
    // it precedes void main() and any user pre-main helpers; the HLSL/Metal emitters
    // extract it into their pre-main region automatically.
    if (addedSrgb) {
        code = genSrgbHelper(backend) + "\n\n" + code;
    }

    return code;
}

// =============================================================================
// Normal Mapping Function Generation (per backend)
// =============================================================================

static std::string genNormalMapFunctionGLSL() {
    return R"(
vec3 getNormalFromMap() {
    if (u_TextureFlags.z < 0.5)
        return normalize(v_Normal);
    vec3 tNorm = texture(u_NormalTexture, v_TexCoord).xyz * 2.0 - 1.0;
    tNorm.xy *= u_MaterialParams1.z;
    vec3 Q1 = dFdx(v_WorldPos);
    vec3 Q2 = dFdy(v_WorldPos);
    vec2 st1 = dFdx(v_TexCoord);
    vec2 st2 = dFdy(v_TexCoord);
    vec3 Nn = normalize(v_Normal);
    vec3 T = normalize(Q1 * st2.y - Q2 * st1.y);
    vec3 B = -normalize(cross(Nn, T));
    mat3 TBN = mat3(T, B, Nn);
    return normalize(TBN * tNorm);
}
)";
}

static std::string genNormalMapFunctionVulkan() {
    return R"(
vec3 getNormalFromMap() {
    if (material.u_TextureFlags.z < 0.5)
        return normalize(v_Normal);
    vec3 tNorm = texture(u_NormalTexture, v_TexCoord).xyz * 2.0 - 1.0;
    tNorm.xy *= material.u_MaterialParams1.z;
    vec3 Q1 = dFdx(v_WorldPos);
    vec3 Q2 = dFdy(v_WorldPos);
    vec2 st1 = dFdx(v_TexCoord);
    vec2 st2 = dFdy(v_TexCoord);
    vec3 Nn = normalize(v_Normal);
    vec3 T = normalize(Q1 * st2.y - Q2 * st1.y);
    vec3 B = -normalize(cross(Nn, T));
    mat3 TBN = mat3(T, B, Nn);
    return normalize(TBN * tNorm);
}
)";
}

static std::string genNormalMapFunctionHLSL() {
    return R"(
float3 getNormalFromMap(PS_INPUT input) {
    if (u_TextureFlags.z < 0.5)
        return normalize(input.v_Normal);
    float3 tNorm = u_NormalTexture.Sample(u_NormalTexture_sampler, input.v_TexCoord).xyz * 2.0 - 1.0;
    tNorm.xy *= u_MaterialParams1.z;
    float3 Q1 = ddx(input.v_WorldPos);
    float3 Q2 = ddy(input.v_WorldPos);
    float2 st1 = ddx(input.v_TexCoord);
    float2 st2 = ddy(input.v_TexCoord);
    float3 Nn = normalize(input.v_Normal);
    float3 T = normalize(Q1 * st2.y - Q2 * st1.y);
    float3 B = -normalize(cross(Nn, T));
    float3x3 TBN = float3x3(T, B, Nn);
    return normalize(mul(TBN, tNorm));
}
)";
}

static std::string genNormalMapFunctionMetal() {
    return R"(
float3 getNormalFromMap(VertexOut in, constant MaterialUniforms& uniforms,
                        texture2d<float> u_NormalTexture, sampler u_NormalTexture_sampler) {
    if (uniforms.u_TextureFlags.z < 0.5)
        return normalize(in.v_Normal);
    float3 tNorm = u_NormalTexture.sample(u_NormalTexture_sampler, in.v_TexCoord).xyz * 2.0 - 1.0;
    tNorm.xy *= uniforms.u_MaterialParams1.z;
    float3 Q1 = dfdx(in.v_WorldPos);
    float3 Q2 = dfdy(in.v_WorldPos);
    float2 st1 = dfdx(in.v_TexCoord);
    float2 st2 = dfdy(in.v_TexCoord);
    float3 Nn = normalize(in.v_Normal);
    float3 T = normalize(Q1 * st2.y - Q2 * st1.y);
    float3 B = -normalize(cross(Nn, T));
    float3x3 TBN = float3x3(T, B, Nn);
    return normalize(TBN * tNorm);
}
)";
}

// =============================================================================
// Lighting / Shadow infrastructure (mirrors Python LightingCodeGen)
// =============================================================================

static std::string genLightStructsGLSL() {
    return R"(struct Light {
    vec4 positionOrDirection;
    vec4 direction;
    vec4 color;
    vec4 params;
    vec4 flags;
};
struct ShadowMap {
    mat4 lightSpaceMatrix;
    vec4 shadowParams;
    vec4 shadowParams2;
};
struct CascadedShadowMap {
    mat4 cascadeMatrices[8];
    vec4 cascadeSplits;
    vec4 cascadeSplits2;
    vec4 cascadeParams;
    vec4 cascadeParams2;
};)";
}

static std::string genLightStructsHLSL() {
    return R"(struct Light {
    float4 positionOrDirection;
    float4 direction;
    float4 color;
    float4 params;
    float4 flags;
};
struct ShadowMap {
    float4x4 lightSpaceMatrix;
    float4 shadowParams;
    float4 shadowParams2;
};
struct CascadedShadowMap {
    float4x4 cascadeMatrices[8];
    float4 cascadeSplits;
    float4 cascadeSplits2;
    float4 cascadeParams;
    float4 cascadeParams2;
};)";
}

static std::string genLightStructsMetal() {
    return R"(constant int MAX_LIGHTS = 16;
constant int MAX_SHADOW_MAPS = 8;
constant int MAX_CASCADES = 8;

struct Light {
    float4 positionOrDirection;
    float4 direction;
    float4 color;
    float4 params;
    float4 flags;
};
struct ShadowMapData {
    float4x4 lightSpaceMatrix;
    float4 shadowParams;
    float4 shadowParams2;
};
struct CascadedShadowMapData {
    float4x4 cascadeMatrices[MAX_CASCADES];
    float4 cascadeSplits;
    float4 cascadeSplits2;
    float4 cascadeParams;
    float4 cascadeParams2;
};
struct LightUniformBuffer {
    Light lights[MAX_LIGHTS];
    ShadowMapData shadowMaps[MAX_SHADOW_MAPS];
    CascadedShadowMapData cascadedShadowMaps[MAX_SHADOW_MAPS];
    float4 ambientLight;
    float4 lightCounts;
    float4 fogColor;
    float4 fogParams;
};)";
}

static std::string getLightStructs(GraphicsBackend backend) {
    if (isBackendHLSL(backend)) return genLightStructsHLSL();
    if (backend == GraphicsBackend::Metal) return genLightStructsMetal();
    return genLightStructsGLSL();
}

static const char* kLightUBOFieldsGLSL = R"(    Light lights[16];
    ShadowMap shadowMaps[8];
    CascadedShadowMap cascadedShadowMaps[8];
    vec4 ambientLight;
    vec4 lightCounts;
    vec4 fogColor;
    vec4 fogParams;)";

static const char* kLightUBOFieldsHLSL = R"(    Light lights[16];
    ShadowMap shadowMaps[8];
    CascadedShadowMap cascadedShadowMaps[8];
    float4 ambientLight;
    float4 lightCounts;
    float4 fogColor;
    float4 fogParams;)";

static std::string getLightUBODeclaration(GraphicsBackend backend) {
    switch (backend) {
        case GraphicsBackend::OpenGL:
        case GraphicsBackend::WebGL:
            return std::string("layout(std140) uniform LightData {\n") + kLightUBOFieldsGLSL + "\n} u_Lights;";
        case GraphicsBackend::Vulkan:
            return std::string("layout(std140, set = 0, binding = 3) uniform LightData {\n") + kLightUBOFieldsGLSL + "\n} u_Lights;";
        case GraphicsBackend::DirectX11:
        case GraphicsBackend::DirectX12:
            return std::string("cbuffer LightData : register(b3)\n{\n") + kLightUBOFieldsHLSL + "\n};";
        default:
            return "";
    }
}

static std::string genShadowSamplersGL(int num2d, int numCube) {
    std::ostringstream o;
    for (int i = 0; i < num2d; ++i) o << "uniform sampler2D u_ShadowMap" << i << ";\n";
    for (int i = 0; i < numCube; ++i) o << "uniform samplerCube u_ShadowCubeMap" << i << ";\n";
    o << "\n";
    o << "float sampleShadowMap(int index, vec2 uv) {\n";
    for (int i = 0; i < num2d; ++i)
        o << (i == 0 ? "    if" : "    else if") << " (index == " << i
          << ") return texture(u_ShadowMap" << i << ", uv).r;\n";
    o << "    return 1.0;\n}\n\n";
    o << "float sampleShadowCubeMap(int index, vec3 dir) {\n    vec4 s;\n";
    if (numCube > 0) {
        for (int i = 0; i < numCube; ++i)
            o << (i == 0 ? "    if" : "    else if") << " (index == " << i
              << ") s = texture(u_ShadowCubeMap" << i << ", dir);\n";
        o << "    else return 1.0;\n";
    } else {
        o << "    return 1.0;\n";
    }
    o << "    return s.r;\n}";
    return o.str();
}

static std::string genShadowSamplersVulkan() {
    return R"(layout(set = 0, binding = 8) uniform sampler2D u_ShadowMaps[8];
layout(set = 0, binding = 9) uniform samplerCube u_ShadowCubeMaps[8];

float sampleShadowMap(int index, vec2 uv) {
    return texture(u_ShadowMaps[index], uv).r;
}
float sampleShadowCubeMap(int index, vec3 dir) {
    return texture(u_ShadowCubeMaps[index], dir).r;
})";
}

static std::string genShadowSamplersHLSL() {
    return R"(Texture2D u_ShadowMaps[8] : register(t4);
TextureCube u_ShadowCubeMaps[8] : register(t12);
SamplerState u_ShadowSampler : register(s8);

float sampleShadowMap(int idx, float2 uv) {
    float r = 1.0;
    if (idx == 0) r = u_ShadowMaps[0].SampleLevel(u_ShadowSampler, uv, 0).r;
    else if (idx == 1) r = u_ShadowMaps[1].SampleLevel(u_ShadowSampler, uv, 0).r;
    else if (idx == 2) r = u_ShadowMaps[2].SampleLevel(u_ShadowSampler, uv, 0).r;
    else if (idx == 3) r = u_ShadowMaps[3].SampleLevel(u_ShadowSampler, uv, 0).r;
    else if (idx == 4) r = u_ShadowMaps[4].SampleLevel(u_ShadowSampler, uv, 0).r;
    else if (idx == 5) r = u_ShadowMaps[5].SampleLevel(u_ShadowSampler, uv, 0).r;
    else if (idx == 6) r = u_ShadowMaps[6].SampleLevel(u_ShadowSampler, uv, 0).r;
    else if (idx == 7) r = u_ShadowMaps[7].SampleLevel(u_ShadowSampler, uv, 0).r;
    return r;
}
float sampleShadowCubeMap(int idx, float3 dir) {
    float r = 1.0;
    if (idx == 0) r = u_ShadowCubeMaps[0].SampleLevel(u_ShadowSampler, dir, 0).r;
    else if (idx == 1) r = u_ShadowCubeMaps[1].SampleLevel(u_ShadowSampler, dir, 0).r;
    else if (idx == 2) r = u_ShadowCubeMaps[2].SampleLevel(u_ShadowSampler, dir, 0).r;
    else if (idx == 3) r = u_ShadowCubeMaps[3].SampleLevel(u_ShadowSampler, dir, 0).r;
    else if (idx == 4) r = u_ShadowCubeMaps[4].SampleLevel(u_ShadowSampler, dir, 0).r;
    else if (idx == 5) r = u_ShadowCubeMaps[5].SampleLevel(u_ShadowSampler, dir, 0).r;
    else if (idx == 6) r = u_ShadowCubeMaps[6].SampleLevel(u_ShadowSampler, dir, 0).r;
    else if (idx == 7) r = u_ShadowCubeMaps[7].SampleLevel(u_ShadowSampler, dir, 0).r;
    return r;
})";
}

static std::string genShadowSamplersMetal() {
    return R"(float sampleShadowMap2D(int idx,
    float2 uv,
    texture2d<float> sm0, texture2d<float> sm1, texture2d<float> sm2, texture2d<float> sm3,
    texture2d<float> sm4, texture2d<float> sm5, texture2d<float> sm6, texture2d<float> sm7,
    sampler s) {
    float r = 1.0;
    switch(idx) {
        case 0: r = sm0.sample(s, uv).r; break;
        case 1: r = sm1.sample(s, uv).r; break;
        case 2: r = sm2.sample(s, uv).r; break;
        case 3: r = sm3.sample(s, uv).r; break;
        case 4: r = sm4.sample(s, uv).r; break;
        case 5: r = sm5.sample(s, uv).r; break;
        case 6: r = sm6.sample(s, uv).r; break;
        case 7: r = sm7.sample(s, uv).r; break;
    }
    return r;
}
float sampleShadowCubeM(int idx,
    float3 dir,
    texturecube<float> cm0, texturecube<float> cm1, texturecube<float> cm2, texturecube<float> cm3,
    texturecube<float> cm4, texturecube<float> cm5, texturecube<float> cm6, texturecube<float> cm7,
    sampler s) {
    float r = 1.0;
    switch(idx) {
        case 0: r = cm0.sample(s, dir).r; break;
        case 1: r = cm1.sample(s, dir).r; break;
        case 2: r = cm2.sample(s, dir).r; break;
        case 3: r = cm3.sample(s, dir).r; break;
        case 4: r = cm4.sample(s, dir).r; break;
        case 5: r = cm5.sample(s, dir).r; break;
        case 6: r = cm6.sample(s, dir).r; break;
        case 7: r = cm7.sample(s, dir).r; break;
    }
    return r;
})";
}

static std::string getShadowSamplers(GraphicsBackend backend) {
    switch (backend) {
        case GraphicsBackend::OpenGL:    return genShadowSamplersGL(8, 8);
        case GraphicsBackend::WebGL:     return genShadowSamplersGL(4, 4);
        case GraphicsBackend::Vulkan:    return genShadowSamplersVulkan();
        case GraphicsBackend::DirectX11:
        case GraphicsBackend::DirectX12: return genShadowSamplersHLSL();
        case GraphicsBackend::Metal:     return genShadowSamplersMetal();
        default:                         return "";
    }
}

static const char* kShadowCalcGLSL = R"(float calculateShadowPCF(mat4 lsMatrix, int smIndex, vec3 wPos, vec3 N, vec3 lDir,
                         float bias, float nBias, float blur, float opacity, float res) {
    vec4 lsPos = MUL(lsMatrix, vec4(wPos, 1.0));
    vec3 proj = lsPos.xyz / lsPos.w;
    proj = proj * 0.5 + 0.5;
    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0)
        return 1.0;
    float curDepth = proj.z;
    float NdL = max(dot(N, lDir), 0.0);
    float fBias = mix(bias * 2.0, bias, NdL);
    float shadow = 0.0;
    vec2 texelSz = 1.0 / vec2(res, res);
    int kr = clamp(int(blur), 1, 3);
    int sc = 0;
    for (int x = -kr; x <= kr; ++x) {
        for (int y = -kr; y <= kr; ++y) {
            vec2 off = vec2(float(x), float(y)) * texelSz * blur;
            float d = sampleShadowMap(smIndex, proj.xy + off);
            shadow += curDepth - fBias > d ? 1.0 : 0.0;
            sc++;
        }
    }
    shadow /= float(sc);
    shadow *= opacity;
    return 1.0 - shadow;
}

float calculateShadowCube(int cmIndex, vec3 lPos, vec3 wPos, vec3 N,
                          float bias, float blur, float opacity, float lRange) {
    vec3 frag2light = wPos - lPos;
    float curDepth = length(frag2light);
    float normDepth = curDepth / lRange;
    vec3 sDir = normalize(frag2light);
    float closest = sampleShadowCubeMap(cmIndex, sDir);
    vec3 lDir2 = -sDir;
    float NdL = max(dot(N, lDir2), 0.0);
    float nBias = bias / lRange;
    float fBias = mix(nBias * 3.0, nBias * 0.5, NdL);
    float shadow = (normDepth - fBias) > closest ? 1.0 : 0.0;
    shadow *= opacity;
    return 1.0 - shadow;
}

float calculateShadow(int smIndex, vec3 wPos, vec3 N, vec3 lDir, vec3 lPos) {
    if (smIndex < 0 || smIndex >= 8) return 1.0;
    ShadowMap sm = u_Lights.shadowMaps[smIndex];
    bool isCube = sm.shadowParams2.y > 0.5;
    if (isCube) {
        float lRange = sm.shadowParams2.z;
        return calculateShadowCube(smIndex, lPos, wPos, N,
            sm.shadowParams.x, sm.shadowParams.z, sm.shadowParams.w, lRange);
    } else {
        return calculateShadowPCF(sm.lightSpaceMatrix, smIndex, wPos, N, lDir,
            sm.shadowParams.x, sm.shadowParams.y, sm.shadowParams.z,
            sm.shadowParams.w, sm.shadowParams2.x);
    }
})";

// Per-backend light-space projection remap (mirrors the Python transpiler's
// _shadow_proj_remap, see [[project_shadow_backend_conventions]]): zero-to-one-depth
// backends (Vulkan/DX/Metal) already produce proj.z in [0,1] so only xy is remapped;
// DX/Metal additionally flip the shadow UV.y. Without this the runtime translator emits
// the GL-only full remap and custom shadow shaders break on Vulkan/DX/Metal.
static std::string shadowProjRemap(bool zeroToOneDepth, bool flipY) {
    std::string stmt = zeroToOneDepth ? "proj.xy = proj.xy * 0.5 + 0.5;"
                                      : "proj = proj * 0.5 + 0.5;";
    if (flipY) stmt += "\n    proj.y = 1.0 - proj.y;";
    return stmt;
}

static std::string getShadowCalcGLSL(int maxShadows, bool zeroToOneDepth = false,
                                     bool flipY = false) {
    std::string s = kShadowCalcGLSL;
    if (maxShadows != 8) {
        s = std::regex_replace(s, std::regex("smIndex >= 8"), "smIndex >= " + std::to_string(maxShadows));
    }
    const std::string base = "proj = proj * 0.5 + 0.5;";
    size_t p = s.find(base);
    if (p != std::string::npos) {
        s.replace(p, base.size(), shadowProjRemap(zeroToOneDepth, flipY));
    }
    return s;
}

// Primary directional-shadow attenuation helper (Gap A). GLSL form is run through the
// body transformer (GL/Vulkan/HLSL); Metal needs the explicit texture/sampler/lightData
// parameter list. See the Python transpiler's _PRIMARY_SHADOW_* for the canonical source.
static const char* kPrimaryShadowGLSL = R"(float lupine_primary_shadow(vec3 wPos, vec3 N) {
    int lpsCount = int(u_Lights.lightCounts.x);
    for (int i = 0; i < 16; ++i) {
        if (i >= lpsCount) break;
        if (int(u_Lights.lights[i].flags.x) != 0) continue;
        if (u_Lights.lights[i].flags.y <= 0.5) continue;
        vec3 L = normalize(-u_Lights.lights[i].direction.xyz);
        return calculateShadow(int(u_Lights.lights[i].flags.z), wPos, N, L,
            u_Lights.lights[i].positionOrDirection.xyz);
    }
    return 1.0;
})";

static const char* kPrimaryShadowMetal = R"(float lupine_primary_shadow(float3 wPos, float3 N,
    constant LightUniformBuffer& lightData,
    texture2d<float> sm0, texture2d<float> sm1, texture2d<float> sm2, texture2d<float> sm3,
    texture2d<float> sm4, texture2d<float> sm5, texture2d<float> sm6, texture2d<float> sm7,
    texturecube<float> cm0, texturecube<float> cm1, texturecube<float> cm2, texturecube<float> cm3,
    texturecube<float> cm4, texturecube<float> cm5, texturecube<float> cm6, texturecube<float> cm7,
    sampler shadowSampler) {
    int lpsCount = int(lightData.lightCounts.x);
    for (int i = 0; i < 16; ++i) {
        if (i >= lpsCount) break;
        if (int(lightData.lights[i].flags.x) != 0) continue;
        if (lightData.lights[i].flags.y <= 0.5) continue;
        float3 L = normalize(-lightData.lights[i].direction.xyz);
        return calculateShadow(int(lightData.lights[i].flags.z), wPos, N, L,
            lightData.lights[i].positionOrDirection.xyz, lightData,
            sm0, sm1, sm2, sm3, sm4, sm5, sm6, sm7,
            cm0, cm1, cm2, cm3, cm4, cm5, cm6, cm7, shadowSampler);
    }
    return 1.0;
})";

static const char* kShadowCalcMetal = R"(float calculateShadowPCF(float4x4 lsMatrix, int smIndex, float3 wPos, float3 N, float3 lDir,
                         float bias, float nBias, float blur, float opacity, float res,
                         texture2d<float> sm0, texture2d<float> sm1, texture2d<float> sm2, texture2d<float> sm3,
                         texture2d<float> sm4, texture2d<float> sm5, texture2d<float> sm6, texture2d<float> sm7,
                         sampler shadowSampler) {
    float4 lsPos = (lsMatrix * float4(wPos, 1.0));
    float3 proj = lsPos.xyz / lsPos.w;
    proj = proj * 0.5 + 0.5;
    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0)
        return 1.0;
    float curDepth = proj.z;
    float NdL = max(dot(N, lDir), 0.0);
    float fBias = mix(bias * 2.0, bias, NdL);
    float shadow = 0.0;
    float2 texelSz = 1.0 / float2(res, res);
    int kr = clamp(int(blur), 1, 3);
    int sc = 0;
    for (int x = -kr; x <= kr; ++x) {
        for (int y = -kr; y <= kr; ++y) {
            float2 off = float2(float(x), float(y)) * texelSz * blur;
            float d = sampleShadowMap2D(smIndex, proj.xy + off, sm0, sm1, sm2, sm3, sm4, sm5, sm6, sm7, shadowSampler);
            shadow += curDepth - fBias > d ? 1.0 : 0.0;
            sc++;
        }
    }
    shadow /= float(sc);
    shadow *= opacity;
    return 1.0 - shadow;
}

float calculateShadowCube(int cmIndex, float3 lPos, float3 wPos, float3 N,
                          float bias, float blur, float opacity, float lRange,
                          texturecube<float> cm0, texturecube<float> cm1, texturecube<float> cm2, texturecube<float> cm3,
                          texturecube<float> cm4, texturecube<float> cm5, texturecube<float> cm6, texturecube<float> cm7,
                          sampler shadowSampler) {
    float3 frag2light = wPos - lPos;
    float curDepth = length(frag2light);
    float normDepth = curDepth / lRange;
    float3 sDir = normalize(frag2light);
    float closest = sampleShadowCubeM(cmIndex, sDir, cm0, cm1, cm2, cm3, cm4, cm5, cm6, cm7, shadowSampler);
    float3 lDir2 = -sDir;
    float NdL = max(dot(N, lDir2), 0.0);
    float nBias = bias / lRange;
    float fBias = mix(nBias * 3.0, nBias * 0.5, NdL);
    float shadow = (normDepth - fBias) > closest ? 1.0 : 0.0;
    shadow *= opacity;
    return 1.0 - shadow;
}

float calculateShadow(int smIndex, float3 wPos, float3 N, float3 lDir, float3 lPos,
                      constant LightUniformBuffer& lightData,
                      texture2d<float> sm0, texture2d<float> sm1, texture2d<float> sm2, texture2d<float> sm3,
                      texture2d<float> sm4, texture2d<float> sm5, texture2d<float> sm6, texture2d<float> sm7,
                      texturecube<float> cm0, texturecube<float> cm1, texturecube<float> cm2, texturecube<float> cm3,
                      texturecube<float> cm4, texturecube<float> cm5, texturecube<float> cm6, texturecube<float> cm7,
                      sampler shadowSampler) {
    if (smIndex < 0 || smIndex >= 8) return 1.0;
    ShadowMapData sm = lightData.shadowMaps[smIndex];
    bool isCube = sm.shadowParams2.y > 0.5;
    if (isCube) {
        float lRange = sm.shadowParams2.z;
        return calculateShadowCube(smIndex, lPos, wPos, N,
            sm.shadowParams.x, sm.shadowParams.z, sm.shadowParams.w, lRange,
            cm0, cm1, cm2, cm3, cm4, cm5, cm6, cm7, shadowSampler);
    } else {
        return calculateShadowPCF(sm.lightSpaceMatrix, smIndex, wPos, N, lDir,
            sm.shadowParams.x, sm.shadowParams.y, sm.shadowParams.z,
            sm.shadowParams.w, sm.shadowParams2.x,
            sm0, sm1, sm2, sm3, sm4, sm5, sm6, sm7, shadowSampler);
    }
})";

// Metal uses zero-to-one clip depth + top-left texture origin, so xy is remapped and
// UV.y is flipped (mirrors get_shadow_calc_metal in the Python transpiler).
static std::string getShadowCalcMetal() {
    std::string s = kShadowCalcMetal;
    const std::string base = "proj = proj * 0.5 + 0.5;";
    size_t p = s.find(base);
    if (p != std::string::npos) {
        s.replace(p, base.size(), shadowProjRemap(true, true));
    }
    return s;
}

// Fog calculation in GLSL-like syntax (run through the body transformer per backend).
// Reads fogColor/fogParams from the light UBO:
//   fogColor.rgb = color, fogColor.w = enabled flag
//   fogParams = (density, start, end, mode) with mode 0=linear, 1=exp, 2=exp2
static const char* kFogCalcGLSL = R"(vec3 applyFog(vec3 color, vec3 wPos, vec3 camPos) {
    if (u_Lights.fogColor.w < 0.5) {
        return color;
    }
    float dist = length(wPos - camPos);
    float density = u_Lights.fogParams.x;
    float fogStart = u_Lights.fogParams.y;
    float fogEnd = u_Lights.fogParams.z;
    int mode = int(u_Lights.fogParams.w);
    float factor = 1.0;
    if (mode == 0) {
        factor = clamp((fogEnd - dist) / max(fogEnd - fogStart, 0.0001), 0.0, 1.0);
    } else if (mode == 1) {
        factor = exp(-density * max(dist - fogStart, 0.0));
    } else {
        float dd = density * max(dist - fogStart, 0.0);
        factor = exp(-dd * dd);
    }
    return mix(u_Lights.fogColor.rgb, color, clamp(factor, 0.0, 1.0));
})";

static const char* kFogCalcMetal = R"(float3 applyFog(float3 color, float3 wPos, float3 camPos,
                constant LightUniformBuffer& lightData) {
    if (lightData.fogColor.w < 0.5) {
        return color;
    }
    float dist = length(wPos - camPos);
    float density = lightData.fogParams.x;
    float fogStart = lightData.fogParams.y;
    float fogEnd = lightData.fogParams.z;
    int mode = int(lightData.fogParams.w);
    float factor = 1.0;
    if (mode == 0) {
        factor = clamp((fogEnd - dist) / max(fogEnd - fogStart, 0.0001), 0.0, 1.0);
    } else if (mode == 1) {
        factor = exp(-density * max(dist - fogStart, 0.0));
    } else {
        float dd = density * max(dist - fogStart, 0.0);
        factor = exp(-dd * dd);
    }
    return mix(lightData.fogColor.rgb, color, clamp(factor, 0.0, 1.0));
})";

static std::string appendMetalShadowArgs(const std::string& code) {
    std::string args = ", lightData";
    for (int i = 0; i < 8; ++i) args += ", shadowMap" + std::to_string(i);
    for (int i = 0; i < 8; ++i) args += ", shadowCubeMap" + std::to_string(i);
    args += ", shadowSampler";

    // calculateShadow() and lupine_primary_shadow() take the same trailing parameter list.
    std::regex re(R"(\b(?:calculateShadow|lupine_primary_shadow)\s*\()");
    std::string out;
    size_t pos = 0;
    auto findClose = [](const std::string& s, size_t open) -> size_t {
        int depth = 0;
        for (size_t i = open; i < s.size(); ++i) {
            if (s[i] == '(') ++depth;
            else if (s[i] == ')') { if (--depth == 0) return i; }
        }
        return std::string::npos;
    };
    while (true) {
        std::smatch m;
        std::string rest = code.substr(pos);
        if (!std::regex_search(rest, m, re)) { out += rest; break; }
        size_t matchPos = pos + m.position();
        size_t openParen = matchPos + m.length() - 1;
        size_t close = findClose(code, openParen);
        if (close == std::string::npos) { out += code.substr(pos); break; }
        out += code.substr(pos, close - pos);
        out += args;
        pos = close;
    }
    return out;
}

static std::string appendMetalFogArgs(const std::string& code) {
    std::regex re(R"(\bapplyFog\s*\()");
    std::string out;
    size_t pos = 0;
    auto findClose = [](const std::string& s, size_t open) -> size_t {
        int depth = 0;
        for (size_t i = open; i < s.size(); ++i) {
            if (s[i] == '(') ++depth;
            else if (s[i] == ')') { if (--depth == 0) return i; }
        }
        return std::string::npos;
    };
    while (true) {
        std::smatch m;
        std::string rest = code.substr(pos);
        if (!std::regex_search(rest, m, re)) { out += rest; break; }
        size_t matchPos = pos + m.position();
        size_t openParen = matchPos + m.length() - 1;
        size_t close = findClose(code, openParen);
        if (close == std::string::npos) { out += code.substr(pos); break; }
        out += code.substr(pos, close - pos);
        out += ", lightData";
        pos = close;
    }
    return out;
}

// =============================================================================
// Strip void main() { ... } wrapper, return inner body lines.
// =============================================================================

static std::string extractPreMain(const std::string& body) {
    std::ostringstream out;
    bool first = true;
    for (auto& line : splitLines(body)) {
        if (trim(line).rfind("void main", 0) == 0) break;
        if (!first) out << "\n";
        out << line;
        first = false;
    }
    return out.str();
}

static std::string extractMainBody(const std::string& body) {
    size_t idx = body.find("void main");
    if (idx == std::string::npos) return "";
    size_t open = body.find('{', idx);
    if (open == std::string::npos) return "";
    int depth = 1;
    size_t start = open + 1;
    for (size_t i = start; i < body.size(); ++i) {
        if (body[i] == '{') {
            ++depth;
        } else if (body[i] == '}') {
            if (--depth == 0) {
                std::string inner = body.substr(start, i - start);
                size_t a = inner.find_first_not_of("\r\n");
                size_t b = inner.find_last_not_of("\r\n");
                if (a == std::string::npos) return "";
                return inner.substr(a, b - a + 1);
            }
        }
    }
    return body.substr(start);
}

static std::string reindentMainBody(const std::string& inner) {
    std::ostringstream out;
    for (auto& line : splitLines(inner)) {
        std::string s = trim(line);
        if (!s.empty()) out << "    " << s << "\n";
    }
    return out.str();
}

// GLSL allows a bare `return;` inside a void main() that writes its result to an `out`
// variable (a common early-out, e.g. for sky/background pixels). The HLSL and Metal entry
// points this translator emits are value-returning - they return the output struct or
// color - so a bare `return;` there is a hard compile error ("non-void function 'main'
// should return a value"). Rewrite every early `return;` in the MAIN body to carry the
// entry's return value. This is applied to the extracted main body only, so void helper
// functions (whose bare returns are perfectly valid) are never touched.
static std::string rewriteBareReturns(const std::string& mainBody, const std::string& returnExpr) {
    static const std::regex bareReturnRe(R"(\breturn\s*;)");
    return std::regex_replace(mainBody, bareReturnRe, "return " + returnExpr + ";");
}

// =============================================================================
// GLSL 330 Emitters (OpenGL)
// =============================================================================

std::string ShaderTranslator::emitGLSLVertex(const LshShaderIR& ir) {
    std::ostringstream out;
    out << "\n#version 330 core\n\n";

    // Vertex inputs
    for (auto& inp : ir.vertex.inputs) {
        out << "layout(location = " << inp.location << ") in " << inp.typeName << " " << inp.name << ";\n";
    }
    out << "\n";

    // Uniforms (non-texture)
    for (auto& u : ir.properties) {
        if (!u.isTexture && !u.isCubemap) {
            out << "uniform " << u.typeName << " " << u.name;
            if (u.arraySize > 0) out << "[" << u.arraySize << "]";
            out << ";\n";
        }
    }
    out << "\n";

    // Outputs
    for (auto& o : ir.vertex.outputs) {
        out << "out " << o.typeName << " " << o.name << ";\n";
    }
    out << "\n";

    // Shared code
    if (!trim(ir.sharedCode).empty()) {
        out << ir.sharedCode << "\n\n";
    }

    // Body
    std::string body = transformBody(ir.vertex.body, GraphicsBackend::OpenGL, ir, "vertex");
    out << body << "\n";

    return out.str();
}

std::string ShaderTranslator::emitGLSLFragment(const LshShaderIR& ir) {
    std::ostringstream out;
    out << "\n#version 330 core\n\n";

    // Fragment inputs (from vertex outputs)
    for (auto& o : ir.vertex.outputs) {
        out << "in " << o.typeName << " " << o.name << ";\n";
    }
    out << "\n";

    // Uniforms (non-texture)
    for (auto& u : ir.properties) {
        if (!u.isTexture && !u.isCubemap) {
            out << "uniform " << u.typeName << " " << u.name;
            if (u.arraySize > 0) out << "[" << u.arraySize << "]";
            out << ";\n";
        }
    }

    // Texture uniforms
    for (auto& u : ir.properties) {
        if (u.isTexture) {
            out << "uniform sampler2D " << u.name << ";\n";
        } else if (u.isCubemap) {
            out << "uniform samplerCube " << u.name << ";\n";
        }
    }
    out << "\n";

    // Lighting feature: LightData UBO + shadow textures + functions
    if (ir.hasLighting) {
        out << getLightStructs(GraphicsBackend::OpenGL) << "\n";
        out << getLightUBODeclaration(GraphicsBackend::OpenGL) << "\n\n";
    }
    if (ir.hasShadows) {
        out << getShadowSamplers(GraphicsBackend::OpenGL) << "\n\n";
    }

    // Fragment outputs
    for (auto& o : ir.fragment.outputs) {
        out << "out " << o.typeName << " " << o.name << ";\n";
    }
    out << "\n";

    // Shadow calculation functions (transformed through body transformer)
    if (ir.hasShadows) {
        out << transformBody(getShadowCalcGLSL(8), GraphicsBackend::OpenGL, ir, "fragment") << "\n";
        if (ir.usesPrimaryShadow)
            out << transformBody(kPrimaryShadowGLSL, GraphicsBackend::OpenGL, ir, "fragment") << "\n";
    }

    // Fog helper function
    if (ir.hasFog) {
        out << transformBody(kFogCalcGLSL, GraphicsBackend::OpenGL, ir, "fragment") << "\n";
    }

    // Normal mapping helper function
    if (ir.hasNormalMapping) {
        out << genNormalMapFunctionGLSL() << "\n";
    }

    // Shared code
    if (!trim(ir.sharedCode).empty()) {
        out << ir.sharedCode << "\n\n";
    }

    // Body
    std::string body = transformBody(ir.fragment.body, GraphicsBackend::OpenGL, ir, "fragment");
    out << body << "\n";

    return out.str();
}

// =============================================================================
// GLSL ES 3.0 Emitters (WebGL)
// =============================================================================

std::string ShaderTranslator::emitGLSLESVertex(const LshShaderIR& ir) {
    std::ostringstream out;
    out << "#version 300 es\n\nprecision highp float;\nprecision highp int;\n\n";

    for (auto& inp : ir.vertex.inputs) {
        out << "layout(location = " << inp.location << ") in " << inp.typeName << " " << inp.name << ";\n";
    }
    out << "\n";

    for (auto& u : ir.properties) {
        if (!u.isTexture && !u.isCubemap) {
            out << "uniform " << u.typeName << " " << u.name;
            if (u.arraySize > 0) out << "[" << u.arraySize << "]";
            out << ";\n";
        }
    }
    out << "\n";

    for (auto& o : ir.vertex.outputs) {
        out << "out " << o.typeName << " " << o.name << ";\n";
    }
    out << "\n";

    if (!trim(ir.sharedCode).empty()) {
        out << ir.sharedCode << "\n\n";
    }

    std::string body = transformBody(ir.vertex.body, GraphicsBackend::WebGL, ir, "vertex");
    out << body << "\n";

    return out.str();
}

std::string ShaderTranslator::emitGLSLESFragment(const LshShaderIR& ir) {
    std::ostringstream out;
    out << "#version 300 es\n\nprecision highp float;\nprecision highp int;\n\n";

    for (auto& o : ir.vertex.outputs) {
        out << "in " << o.typeName << " " << o.name << ";\n";
    }
    out << "\n";

    for (auto& u : ir.properties) {
        if (!u.isTexture && !u.isCubemap) {
            out << "uniform " << u.typeName << " " << u.name;
            if (u.arraySize > 0) out << "[" << u.arraySize << "]";
            out << ";\n";
        }
    }

    for (auto& u : ir.properties) {
        if (u.isTexture) {
            out << "uniform sampler2D " << u.name << ";\n";
        } else if (u.isCubemap) {
            out << "uniform samplerCube " << u.name << ";\n";
        }
    }
    out << "\n";

    if (ir.hasLighting) {
        out << getLightStructs(GraphicsBackend::WebGL) << "\n";
        out << getLightUBODeclaration(GraphicsBackend::WebGL) << "\n\n";
    }
    if (ir.hasShadows) {
        out << getShadowSamplers(GraphicsBackend::WebGL) << "\n\n";
    }

    for (auto& o : ir.fragment.outputs) {
        out << "out " << o.typeName << " " << o.name << ";\n";
    }
    out << "\n";

    if (ir.hasShadows) {
        out << transformBody(getShadowCalcGLSL(4), GraphicsBackend::WebGL, ir, "fragment") << "\n";
        if (ir.usesPrimaryShadow)
            out << transformBody(kPrimaryShadowGLSL, GraphicsBackend::WebGL, ir, "fragment") << "\n";
    }

    // Fog helper function
    if (ir.hasFog) {
        out << transformBody(kFogCalcGLSL, GraphicsBackend::WebGL, ir, "fragment") << "\n";
    }

    // Normal mapping helper function
    if (ir.hasNormalMapping) {
        out << genNormalMapFunctionGLSL() << "\n";
    }

    if (!trim(ir.sharedCode).empty()) {
        out << ir.sharedCode << "\n\n";
    }

    std::string body = transformBody(ir.fragment.body, GraphicsBackend::WebGL, ir, "fragment");
    out << body << "\n";

    return out.str();
}

// =============================================================================
// GLSL 450 Emitters (Vulkan)
// =============================================================================

std::string ShaderTranslator::emitGLSL450Vertex(const LshShaderIR& ir) {
    std::ostringstream out;
    out << "\n#version 450\n\n";

    // Vertex inputs
    for (auto& inp : ir.vertex.inputs) {
        out << "layout(location = " << inp.location << ") in " << inp.typeName << " " << inp.name << ";\n";
    }
    out << "\n";

    // Push constants
    std::vector<const LshUniformInfo*> pushConsts;
    std::vector<const LshUniformInfo*> materialUniforms;
    std::vector<const LshUniformInfo*> separateUniforms;
    for (auto& u : ir.properties) {
        if (u.isTexture || u.isCubemap) continue;
        if (kPushConstantNames.count(u.name)) {
            pushConsts.push_back(&u);
        } else if (kSeparateBufferNames.count(u.name)) {
            separateUniforms.push_back(&u);
        } else {
            materialUniforms.push_back(&u);
        }
    }

    if (!pushConsts.empty()) {
        out << "layout(push_constant) uniform PushConstants {\n";
        for (auto* u : pushConsts) {
            out << "    " << u->typeName << " " << u->name;
            if (u->arraySize > 0) out << "[" << u->arraySize << "]";
            out << ";\n";
        }
        out << "} pc;\n\n";
    }

    // Material UBO
    if (!materialUniforms.empty()) {
        out << "layout(set = 0, binding = 2) uniform MaterialData {\n";
        for (auto* u : materialUniforms) {
            out << "    " << u->typeName << " " << u->name;
            if (u->arraySize > 0) out << "[" << u->arraySize << "]";
            out << ";\n";
        }
        out << "} material;\n\n";
    }

    if (!separateUniforms.empty()) {
        out << "layout(std140, set = 0, binding = 1) uniform BoneData {\n";
        for (auto* u : separateUniforms) {
            out << "    " << u->typeName << " " << u->name;
            if (u->arraySize > 0) out << "[" << u->arraySize << "]";
            out << ";\n";
        }
        out << "} bones;\n\n";
    }

    // Outputs
    for (size_t i = 0; i < ir.vertex.outputs.size(); ++i) {
        auto& o = ir.vertex.outputs[i];
        out << "layout(location = " << i << ") out " << o.typeName << " " << o.name << ";\n";
    }
    out << "\n";

    if (!trim(ir.sharedCode).empty()) {
        out << ir.sharedCode << "\n\n";
    }

    std::string body = transformBody(ir.vertex.body, GraphicsBackend::Vulkan, ir, "vertex");
    out << body << "\n";

    return out.str();
}

std::string ShaderTranslator::emitGLSL450Fragment(const LshShaderIR& ir) {
    std::ostringstream out;
    out << "\n#version 450\n\n";

    // Fragment inputs
    for (size_t i = 0; i < ir.vertex.outputs.size(); ++i) {
        auto& o = ir.vertex.outputs[i];
        out << "layout(location = " << i << ") in " << o.typeName << " " << o.name << ";\n";
    }
    out << "\n";

    // Fragment outputs
    for (size_t i = 0; i < ir.fragment.outputs.size(); ++i) {
        auto& o = ir.fragment.outputs[i];
        out << "layout(location = " << i << ") out " << o.typeName << " " << o.name << ";\n";
    }
    out << "\n";

    // Push constants
    std::vector<const LshUniformInfo*> pushConsts;
    std::vector<const LshUniformInfo*> materialUniforms;
    std::vector<const LshUniformInfo*> separateUniforms;
    for (auto& u : ir.properties) {
        if (u.isTexture || u.isCubemap) continue;
        if (kPushConstantNames.count(u.name)) {
            pushConsts.push_back(&u);
        } else if (kSeparateBufferNames.count(u.name)) {
            separateUniforms.push_back(&u);
        } else {
            materialUniforms.push_back(&u);
        }
    }

    if (!pushConsts.empty()) {
        out << "layout(push_constant) uniform PushConstants {\n";
        for (auto* u : pushConsts) {
            out << "    " << u->typeName << " " << u->name;
            if (u->arraySize > 0) out << "[" << u->arraySize << "]";
            out << ";\n";
        }
        out << "} pc;\n\n";
    }

    if (!materialUniforms.empty()) {
        out << "layout(set = 0, binding = 2) uniform MaterialData {\n";
        for (auto* u : materialUniforms) {
            out << "    " << u->typeName << " " << u->name;
            if (u->arraySize > 0) out << "[" << u->arraySize << "]";
            out << ";\n";
        }
        out << "} material;\n\n";
    }

    if (!separateUniforms.empty()) {
        out << "layout(std140, set = 0, binding = 1) uniform BoneData {\n";
        for (auto* u : separateUniforms) {
            out << "    " << u->typeName << " " << u->name;
            if (u->arraySize > 0) out << "[" << u->arraySize << "]";
            out << ";\n";
        }
        out << "} bones;\n\n";
    }

    // Textures
    int binding = 4;
    bool hasTextures = false;
    for (auto& u : ir.properties) {
        if (u.isTexture) {
            out << "layout(set = 0, binding = " << binding << ") uniform sampler2D " << u.name << ";\n";
            ++binding;
            hasTextures = true;
        } else if (u.isCubemap) {
            out << "layout(set = 0, binding = " << binding << ") uniform samplerCube " << u.name << ";\n";
            ++binding;
            hasTextures = true;
        }
    }
    if (hasTextures) out << "\n";

    if (ir.hasLighting) {
        out << getLightStructs(GraphicsBackend::Vulkan) << "\n";
        out << getLightUBODeclaration(GraphicsBackend::Vulkan) << "\n\n";
    }
    if (ir.hasShadows) {
        out << getShadowSamplers(GraphicsBackend::Vulkan) << "\n\n";
    }

    if (ir.hasShadows) {
        out << transformBody(getShadowCalcGLSL(8, /*zeroToOneDepth*/ true), GraphicsBackend::Vulkan, ir, "fragment") << "\n";
        if (ir.usesPrimaryShadow)
            out << transformBody(kPrimaryShadowGLSL, GraphicsBackend::Vulkan, ir, "fragment") << "\n";
    }

    // Fog helper function
    if (ir.hasFog) {
        out << transformBody(kFogCalcGLSL, GraphicsBackend::Vulkan, ir, "fragment") << "\n";
    }

    // Normal mapping helper function
    if (ir.hasNormalMapping) {
        out << genNormalMapFunctionVulkan() << "\n";
    }

    if (!trim(ir.sharedCode).empty()) {
        out << ir.sharedCode << "\n\n";
    }

    std::string body = transformBody(ir.fragment.body, GraphicsBackend::Vulkan, ir, "fragment");
    out << body << "\n";

    return out.str();
}

// =============================================================================
// HLSL Emitters (DirectX 11/12)
// =============================================================================

// HLSL has no inline texture-size intrinsic (GetDimensions writes out-params), so
// TEXTURE_SIZE(tex) routes through this helper. Passing Texture2D by value is valid
// in Shader Model 5.0+ (DX11 and DX12).
static const char* kHLSLTextureSizeHelper =
    "float2 lupine_textureSize(Texture2D t) {\n"
    "    float2 d;\n"
    "    t.GetDimensions(d.x, d.y);\n"
    "    return d;\n"
    "}";

std::string ShaderTranslator::emitHLSLVertex(const LshShaderIR& ir, GraphicsBackend backend) {
    std::string backendName = (backend == GraphicsBackend::DirectX11) ? "DirectX11" : "DirectX12";
    std::ostringstream out;
    out << "// " << backendName << " " << ir.name << " Vertex Shader\n\n";

    // cbuffer
    out << buildHLSLCBuffer(ir, backend) << "\n\n";

    // VS_INPUT
    out << "struct VS_INPUT\n{\n";
    for (auto& inp : ir.vertex.inputs) {
        std::string ht = toHLSLType(inp.typeName);
        std::string sem = hlslSemantic(inp.semantic);
        out << "    " << ht << " " << inp.name << " : " << sem << ";\n";
    }
    out << "};\n\n";

    // VS_OUTPUT
    out << "struct VS_OUTPUT\n{\n";
    out << "    float4 position : SV_POSITION;\n";
    int texcoordIdx = 0;
    for (auto& o : ir.vertex.outputs) {
        std::string ht = toHLSLType(o.typeName);
        if (!o.semantic.empty()) {
            out << "    " << ht << " " << o.name << " : " << hlslSemantic(o.semantic) << ";\n";
        } else {
            out << "    " << ht << " " << o.name << " : TEXCOORD" << texcoordIdx << ";\n";
            ++texcoordIdx;
        }
    }
    out << "};\n\n";

    // Shared code
    if (!trim(ir.sharedCode).empty()) {
        std::string shared = replaceHLSLTypes(ir.sharedCode);
        shared = replaceHLSLFunctions(shared);
        shared = hlslStaticConst(shared);
        out << shared << "\n\n";
    }

    // main function
    std::string body = transformBody(ir.vertex.body, backend, ir, "vertex");
    if (body.find("lupine_textureSize(") != std::string::npos) {
        out << kHLSLTextureSizeHelper << "\n\n";
    }
    std::string preMain = extractPreMain(body);
    if (!trim(preMain).empty()) {
        out << preMain << "\n\n";
    }

    out << "VS_OUTPUT main(VS_INPUT input)\n{\n";
    out << "    VS_OUTPUT output;\n\n";
    out << reindentMainBody(rewriteBareReturns(extractMainBody(body), "output"));
    out << "\n    return output;\n}\n";

    return out.str();
}

std::string ShaderTranslator::emitHLSLFragment(const LshShaderIR& ir, GraphicsBackend backend) {
    std::string backendName = (backend == GraphicsBackend::DirectX11) ? "DirectX11" : "DirectX12";
    std::ostringstream out;
    out << "// " << backendName << " " << ir.name << " Fragment Shader\n\n";

    // cbuffer
    out << buildHLSLCBuffer(ir, backend) << "\n\n";

    // Textures
    std::string texStr = buildHLSLTextures(ir);
    if (!texStr.empty()) {
        out << texStr << "\n";
    }

    // Lighting feature: structs + cbuffer + shadow textures
    if (ir.hasLighting) {
        out << getLightStructs(backend) << "\n\n";
        out << getLightUBODeclaration(backend) << "\n\n";
    }
    if (ir.hasShadows) {
        out << getShadowSamplers(backend) << "\n\n";
    }

    // PS_INPUT (must match VS_OUTPUT / GS_OUTPUT semantics exactly)
    out << "struct PS_INPUT\n{\n";
    out << "    float4 position : SV_POSITION;\n";
    int texcoordIdx = 0;
    const std::vector<LshOutputInfo>& psOutputs = ir.hasGeometry ? ir.geometry.outputs : ir.vertex.outputs;
    for (auto& o : psOutputs) {
        if (o.name == "position" || o.semantic == "SV_POSITION") continue;
        std::string ht = toHLSLType(o.typeName);
        if (!o.semantic.empty()) {
            out << "    " << ht << " " << o.name << " : " << hlslSemantic(o.semantic) << ";\n";
        } else {
            out << "    " << ht << " " << o.name << " : TEXCOORD" << texcoordIdx << ";\n";
            ++texcoordIdx;
        }
    }
    if (ir.usesFrontFacing) {
        out << "    bool lupine_frontFacing : SV_IsFrontFace;\n";
    }
    out << "};\n\n";

    // Shadow calculation functions (transformed)
    if (ir.hasShadows) {
        out << transformBody(getShadowCalcGLSL(8, /*zeroToOneDepth*/ true, /*flipY*/ true), backend, ir, "fragment") << "\n";
        if (ir.usesPrimaryShadow)
            out << transformBody(kPrimaryShadowGLSL, backend, ir, "fragment") << "\n";
    }

    // Fog helper function
    if (ir.hasFog) {
        out << transformBody(kFogCalcGLSL, backend, ir, "fragment") << "\n";
    }

    // Normal mapping helper function
    if (ir.hasNormalMapping) {
        out << genNormalMapFunctionHLSL() << "\n";
    }

    // Shared code
    if (!trim(ir.sharedCode).empty()) {
        std::string shared = replaceHLSLTypes(ir.sharedCode);
        shared = replaceHLSLFunctions(shared);
        shared = hlslStaticConst(shared);
        out << shared << "\n\n";
    }

    std::string body = transformBody(ir.fragment.body, backend, ir, "fragment");
    if (body.find("lupine_textureSize(") != std::string::npos) {
        out << kHLSLTextureSizeHelper << "\n\n";
    }
    std::string preMain = extractPreMain(body);
    if (!trim(preMain).empty()) {
        out << preMain << "\n\n";
    }

    // Determine output type
    std::string outType = "float4";
    std::string outName = "FragColor";
    if (!ir.fragment.outputs.empty()) {
        outType = toHLSLType(ir.fragment.outputs[0].typeName);
        outName = ir.fragment.outputs[0].name;
    }

    out << outType << " main(PS_INPUT input) : SV_TARGET\n{\n";
    out << "    " << outType << " " << outName << ";\n\n";
    out << reindentMainBody(rewriteBareReturns(extractMainBody(body), outName));
    out << "\n    return " << outName << ";\n}\n";

    return out.str();
}

std::string ShaderTranslator::emitHLSLGeometry(const LshShaderIR& ir, GraphicsBackend backend) {
    if (!ir.hasGeometry) return "";
    std::string backendName = (backend == GraphicsBackend::DirectX11) ? "DirectX11" : "DirectX12";
    std::ostringstream out;
    out << "// " << backendName << " " << ir.name << " Geometry Shader\n\n";

    out << buildHLSLCBuffer(ir, backend) << "\n\n";

    // GS_INPUT (matches VS_OUTPUT)
    out << "struct GS_INPUT\n{\n";
    out << "    float4 position : SV_POSITION;\n";
    int texcoordIdx = 0;
    for (auto& o : ir.vertex.outputs) {
        std::string ht = toHLSLType(o.typeName);
        if (!o.semantic.empty()) {
            out << "    " << ht << " " << o.name << " : " << hlslSemantic(o.semantic) << ";\n";
        } else {
            out << "    " << ht << " " << o.name << " : TEXCOORD" << texcoordIdx << ";\n";
            ++texcoordIdx;
        }
    }
    out << "};\n\n";

    // GS_OUTPUT
    out << "struct GS_OUTPUT\n{\n";
    out << "    float4 position : SV_POSITION;\n";
    texcoordIdx = 0;
    for (auto& o : ir.geometry.outputs) {
        if (o.name == "position" || o.semantic == "SV_POSITION") continue;
        std::string ht = toHLSLType(o.typeName);
        if (!o.semantic.empty()) {
            out << "    " << ht << " " << o.name << " : " << hlslSemantic(o.semantic) << ";\n";
        } else {
            out << "    " << ht << " " << o.name << " : TEXCOORD" << texcoordIdx << ";\n";
            ++texcoordIdx;
        }
    }
    out << "};\n\n";

    std::string topo = ir.geometry.topology;
    std::string inputTopo = (topo == "line" || topo == "point" || topo == "triangle") ? topo : "line";
    int arraySize = (inputTopo == "line") ? 2 : (inputTopo == "triangle") ? 3 : 1;
    int maxVerts = ir.geometry.maxVertices > 0 ? ir.geometry.maxVertices : 4;

    out << "[maxvertexcount(" << maxVerts << ")]\n";
    out << "void main(" << inputTopo << " GS_INPUT input[" << arraySize
        << "], inout TriangleStream<GS_OUTPUT> outputStream)\n{\n";
    out << "    GS_OUTPUT output;\n\n";

    std::string body = transformBody(ir.geometry.body, backend, ir, "geometry");
    out << reindentMainBody(extractMainBody(body));
    out << "}\n";

    return out.str();
}

// =============================================================================
// Metal Emitter (combined vertex + fragment)
// =============================================================================

std::string ShaderTranslator::emitMetal(const LshShaderIR& ir) {
    std::ostringstream out;
    out << "// " << ir.name << " - Metal Shading Language\n";
    out << "// " << ir.description << "\n\n";
    out << "#include <metal_stdlib>\n";
    out << "using namespace metal;\n\n";

    // VertexIn struct
    out << "struct VertexIn {\n";
    for (auto& inp : ir.vertex.inputs) {
        std::string mt = toMetalType(inp.typeName);
        out << "    " << mt << " " << inp.name << " [[attribute(" << inp.location << ")]];\n";
    }
    out << "};\n\n";

    // VertexOut struct
    out << "struct VertexOut {\n";
    out << "    float4 position [[position]];\n";
    for (auto& o : ir.vertex.outputs) {
        std::string mt = toMetalType(o.typeName);
        out << "    " << mt << " " << o.name << ";\n";
    }
    out << "};\n\n";

    // MaterialUniforms struct
    std::vector<const LshUniformInfo*> nonTexUniforms;
    for (auto& u : ir.properties) {
        if (!u.isTexture && !u.isCubemap) {
            nonTexUniforms.push_back(&u);
        }
    }

    if (!nonTexUniforms.empty()) {
        out << "struct MaterialUniforms {\n";
        for (auto* u : nonTexUniforms) {
            std::string mt = toMetalType(u->typeName);
            out << "    " << mt << " " << u->name;
            if (u->arraySize > 0) out << "[" << u->arraySize << "]";
            out << ";\n";
        }
        out << "};\n\n";
    }

    // Lighting structs + shadow samplers (file scope)
    if (ir.hasLighting) {
        out << getLightStructs(GraphicsBackend::Metal) << "\n\n";
    }
    if (ir.hasShadows) {
        out << getShadowSamplers(GraphicsBackend::Metal) << "\n\n";
    }

    // Shared code
    if (!trim(ir.sharedCode).empty()) {
        std::string shared = replaceMetalTypes(ir.sharedCode);
        shared = replaceMetalFunctions(shared);
        out << shared << "\n\n";
    }

    // Normal mapping helper function (before vertex so it's available in fragment)
    if (ir.hasNormalMapping) {
        out << genNormalMapFunctionMetal() << "\n";
    }

    // Vertex function
    out << "vertex VertexOut vertex_main(\n";
    out << "    VertexIn in [[stage_in]],\n";
    if (!nonTexUniforms.empty()) {
        out << "    constant MaterialUniforms& uniforms [[buffer(16)]],\n";
    }
    out << "    uint vid [[vertex_id]],\n";
    out << "    uint iid [[instance_id]]\n";
    out << ") {\n";
    out << "    VertexOut out;\n\n";

    std::string vertBody = transformBody(ir.vertex.body, GraphicsBackend::Metal, ir, "vertex");
    out << reindentMainBody(rewriteBareReturns(extractMainBody(vertBody), "out"));

    out << "\n    return out;\n}\n\n";

    // Fragment function
    std::string fragOutName = "FragColor";
    if (!ir.fragment.outputs.empty()) {
        fragOutName = ir.fragment.outputs[0].name;
    }

    std::string fragBody = transformBody(ir.fragment.body, GraphicsBackend::Metal, ir, "fragment");
    if (ir.hasShadows) {
        fragBody = appendMetalShadowArgs(fragBody);
        out << getShadowCalcMetal() << "\n\n";
        if (ir.usesPrimaryShadow)
            out << kPrimaryShadowMetal << "\n\n";
    }
    if (ir.hasFog) {
        fragBody = appendMetalFogArgs(fragBody);
        out << kFogCalcMetal << "\n\n";
    }
    std::string fragPreMain = extractPreMain(fragBody);
    if (!trim(fragPreMain).empty()) {
        out << fragPreMain << "\n\n";
    }

    out << "fragment float4 fragment_main(\n";
    out << "    VertexOut in [[stage_in]]";
    if (ir.usesFrontFacing) {
        out << ",\n    bool lupine_frontFacing [[front_facing]]";
    }
    if (!nonTexUniforms.empty()) {
        out << ",\n    constant MaterialUniforms& uniforms [[buffer(16)]]";
    }
    if (ir.hasLighting) {
        out << ",\n    constant LightUniformBuffer& lightData [[buffer(3)]]";
    }

    // Texture parameters
    std::vector<const LshUniformInfo*> textures;
    for (auto& u : ir.properties) {
        if (u.isTexture || u.isCubemap) textures.push_back(&u);
    }

    for (size_t idx = 0; idx < textures.size(); ++idx) {
        auto* u = textures[idx];
        if (u->isCubemap) {
            out << ",\n    texturecube<float> " << u->name << " [[texture(" << idx << ")]]";
            out << ",\n    sampler " << u->name << "_sampler [[sampler(" << idx << ")]]";
        } else {
            out << ",\n    texture2d<float> " << u->name << " [[texture(" << idx << ")]]";
            out << ",\n    sampler " << u->name << "_sampler [[sampler(" << idx << ")]]";
        }
    }

    if (ir.hasShadows) {
        size_t texStart = textures.size();
        for (size_t i = 0; i < 8; ++i)
            out << ",\n    texture2d<float> shadowMap" << i << " [[texture(" << (texStart + i) << ")]]";
        for (size_t i = 0; i < 8; ++i)
            out << ",\n    texturecube<float> shadowCubeMap" << i << " [[texture(" << (texStart + 8 + i) << ")]]";
        out << ",\n    sampler shadowSampler [[sampler(" << textures.size() << ")]]";
    }

    out << "\n) {\n";
    out << "    float4 " << fragOutName << ";\n\n";
    out << reindentMainBody(rewriteBareReturns(extractMainBody(fragBody), fragOutName));
    out << "\n    return " << fragOutName << ";\n}\n";

    return out.str();
}

} // namespace lupine
