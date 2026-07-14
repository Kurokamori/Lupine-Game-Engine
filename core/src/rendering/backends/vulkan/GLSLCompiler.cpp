#include "lupine/rendering/backends/vulkan/GLSLCompiler.hpp"

#ifdef LUPINE_HAS_VULKAN

#include "lupine/logger/Logger.hpp"
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <glslang/Public/ResourceLimits.h>
#include <cctype>

namespace lupine {

bool GLSLCompiler::s_initialized = false;

namespace {

// std140 base alignment and consumed size (in bytes) for the GLSL types used in
// the engine's push_constant and material uniform blocks. For these 16-byte- or
// smaller-aligned scalar/vector/matrix types std140 and std430 produce identical
// member offsets, so a single table serves both block kinds.
struct Std140TypeInfo {
    uint32_t alignment;
    uint32_t size;
    bool known;
};

Std140TypeInfo std140TypeInfo(const std::string& type) {
    if (type == "float" || type == "int" || type == "uint" || type == "bool") return {4, 4, true};
    if (type == "vec2"  || type == "ivec2" || type == "uvec2" || type == "bvec2") return {8, 8, true};
    if (type == "vec3"  || type == "ivec3" || type == "uvec3" || type == "bvec3") return {16, 12, true};
    if (type == "vec4"  || type == "ivec4" || type == "uvec4" || type == "bvec4") return {16, 16, true};
    if (type == "mat2") return {16, 32, true};   // std140: 2 columns, each padded to vec4
    if (type == "mat3") return {16, 48, true};   // std140: 3 columns, each padded to vec4
    if (type == "mat4") return {16, 64, true};
    return {16, 0, false};
}

inline uint32_t alignUp(uint32_t value, uint32_t alignment) {
    if (alignment == 0) return value;
    return (value + alignment - 1) / alignment * alignment;
}

// Strip // line comments and /* */ block comments from GLSL source so the simple
// block scanner below does not trip over braces or semicolons inside comments.
std::string stripComments(const std::string& src) {
    std::string out;
    out.reserve(src.size());
    for (size_t i = 0; i < src.size(); ++i) {
        if (i + 1 < src.size() && src[i] == '/' && src[i + 1] == '/') {
            while (i < src.size() && src[i] != '\n') ++i;
            if (i < src.size()) out.push_back('\n');
        } else if (i + 1 < src.size() && src[i] == '/' && src[i + 1] == '*') {
            i += 2;
            while (i + 1 < src.size() && !(src[i] == '*' && src[i + 1] == '/')) ++i;
            ++i;  // skip the closing '/'
        } else {
            out.push_back(src[i]);
        }
    }
    return out;
}

// Extract the body (text between the first '{' after startPos and its matching
// '}') of a block. Returns false if no balanced braces are found.
bool extractBlockBody(const std::string& src, size_t startPos, std::string& outBody) {
    size_t open = src.find('{', startPos);
    if (open == std::string::npos) return false;
    int depth = 0;
    for (size_t i = open; i < src.size(); ++i) {
        if (src[i] == '{') {
            ++depth;
        } else if (src[i] == '}') {
            --depth;
            if (depth == 0) {
                outBody = src.substr(open + 1, i - open - 1);
                return true;
            }
        }
    }
    return false;
}

std::string trim(const std::string& s) {
    size_t a = 0;
    size_t b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

// Parse the members of a uniform block body into std140-laid-out members.
// Each member is "type name" or "type name[N]" terminated by ';'. Returns the
// std140 size of the whole block (rounded up to a 16-byte boundary).
uint32_t parseBlockMembers(const std::string& body, std::vector<GlslUniformMember>& outMembers) {
    uint32_t offset = 0;
    size_t pos = 0;
    while (pos < body.size()) {
        size_t semi = body.find(';', pos);
        if (semi == std::string::npos) break;
        std::string decl = trim(body.substr(pos, semi - pos));
        pos = semi + 1;
        if (decl.empty()) continue;

        // Split into whitespace-separated tokens; ignore layout/precision
        // qualifiers and keep the final two meaningful tokens as type and name.
        std::vector<std::string> tokens;
        size_t t = 0;
        while (t < decl.size()) {
            while (t < decl.size() && std::isspace(static_cast<unsigned char>(decl[t]))) ++t;
            size_t start = t;
            while (t < decl.size() && !std::isspace(static_cast<unsigned char>(decl[t]))) ++t;
            if (t > start) tokens.push_back(decl.substr(start, t - start));
        }
        if (tokens.size() < 2) continue;

        std::string name = tokens.back();
        std::string type = tokens[tokens.size() - 2];

        // Skip precision qualifiers that may sit directly before the type.
        if (type == "highp" || type == "mediump" || type == "lowp") {
            type = "float";  // qualifier without a type is not a real member; bail safely
        }

        // Pull an optional array suffix off the name: "u_Foo[3]".
        uint32_t arrayCount = 1;
        size_t bracket = name.find('[');
        if (bracket != std::string::npos) {
            size_t close = name.find(']', bracket);
            std::string countStr = (close != std::string::npos)
                ? trim(name.substr(bracket + 1, close - bracket - 1)) : std::string();
            name = name.substr(0, bracket);
            int parsed = 0;
            for (char c : countStr) {
                if (c >= '0' && c <= '9') parsed = parsed * 10 + (c - '0');
                else { parsed = 0; break; }
            }
            arrayCount = parsed > 0 ? static_cast<uint32_t>(parsed) : 1;
        }

        Std140TypeInfo info = std140TypeInfo(type);
        if (!info.known) {
            // Unrecognized member type: cannot reliably compute the rest of the
            // block layout, so stop parsing this block.
            break;
        }

        uint32_t memberAlign = info.alignment;
        uint32_t memberSize = info.size;
        if (arrayCount > 1) {
            // std140 array element stride is rounded up to a vec4 boundary.
            uint32_t stride = alignUp(info.size, 16);
            memberAlign = 16;
            memberSize = stride * arrayCount;
        }

        offset = alignUp(offset, memberAlign);
        GlslUniformMember member;
        member.name = name;
        member.offset = offset;
        member.size = (arrayCount > 1) ? memberSize : info.size;
        outMembers.push_back(member);

        offset += memberSize;
    }
    return alignUp(offset, 16);
}

} // namespace

void GLSLCompiler::reflectUniformLayout(const char* source, GlslReflection& out) {
    out.pushConstants.clear();
    out.materialUBO.clear();
    out.materialUBOSize = 0;
    if (source == nullptr) return;

    std::string src = stripComments(source);

    // Push-constant block: layout(push_constant) uniform <Name> { ... };
    size_t pcKeyword = src.find("push_constant");
    if (pcKeyword != std::string::npos) {
        std::string body;
        if (extractBlockBody(src, pcKeyword, body)) {
            parseBlockMembers(body, out.pushConstants);
        }
    }

    // Material block: the uniform block declared at set = 0, binding = 2.
    // Scan every uniform block and pick the one whose binding is 2.
    size_t searchPos = 0;
    while (true) {
        size_t bindingPos = src.find("binding", searchPos);
        if (bindingPos == std::string::npos) break;
        searchPos = bindingPos + 7;

        // Read the integer after "binding" (allowing "binding = 2" / "binding=2").
        size_t p = bindingPos + 7;
        while (p < src.size() && (src[p] == ' ' || src[p] == '\t' || src[p] == '=')) ++p;
        int bindingIndex = -1;
        if (p < src.size() && src[p] >= '0' && src[p] <= '9') {
            bindingIndex = 0;
            while (p < src.size() && src[p] >= '0' && src[p] <= '9') {
                bindingIndex = bindingIndex * 10 + (src[p] - '0');
                ++p;
            }
        }
        if (bindingIndex != 2) continue;

        // Confirm this is a uniform block (not a sampler) before the opening brace.
        size_t brace = src.find('{', p);
        if (brace == std::string::npos) continue;
        std::string between = src.substr(p, brace - p);
        if (between.find("uniform") == std::string::npos) continue;

        std::string body;
        if (extractBlockBody(src, p, body)) {
            out.materialUBOSize = parseBlockMembers(body, out.materialUBO);
        }
        break;
    }
}

void GLSLCompiler::initialize() {
    if (!s_initialized) {
        glslang::InitializeProcess();
        s_initialized = true;
        LOG_INFO(LogCategory::Render, "GLSLCompiler: glslang initialized (Vulkan 1.2 / SPIR-V 1.5)");
    }
}

void GLSLCompiler::shutdown() {
    if (s_initialized) {
        glslang::FinalizeProcess();
        s_initialized = false;
        LOG_INFO(LogCategory::Render, "GLSLCompiler: glslang finalized");
    }
}

static EShLanguage getGlslangStage(ShaderStage stage) {
    switch (stage) {
        case ShaderStage::Vertex: return EShLangVertex;
        case ShaderStage::Fragment: return EShLangFragment;
        case ShaderStage::Geometry: return EShLangGeometry;
        case ShaderStage::Compute: return EShLangCompute;
        default: return EShLangVertex;
    }
}

bool GLSLCompiler::compileGLSLToSPIRV(
    const char* source,
    ShaderStage stage,
    std::vector<uint32_t>& outSpirv,
    std::string& errorLog
) {
    if (!s_initialized) {
        errorLog = "GLSLCompiler not initialized. Call initialize() first.";
        return false;
    }

    EShLanguage glslangStage = getGlslangStage(stage);
    glslang::TShader shader(glslangStage);
    
    // Set shader source
    shader.setStrings(&source, 1);
    
    // Set environment to Vulkan 1.2 with SPIR-V 1.5, GLSL 450
    shader.setEnvInput(glslang::EShSourceGlsl, glslangStage, glslang::EShClientVulkan, 450);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_2);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_5);
    
    // Use default built-in resource limits
    const TBuiltInResource* resources = GetDefaultResources();
    
    // Parse the shader
    EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);
    if (!shader.parse(resources, 450, false, messages)) {
        errorLog = shader.getInfoLog();
        errorLog += "\n";
        errorLog += shader.getInfoDebugLog();
        return false;
    }
    
    // Link the shader into a program
    glslang::TProgram program;
    program.addShader(&shader);
    
    if (!program.link(messages)) {
        errorLog = program.getInfoLog();
        errorLog += "\n";
        errorLog += program.getInfoDebugLog();
        return false;
    }
    
    // Convert to SPIR-V
    glslang::SpvOptions spvOptions;
#ifdef NDEBUG
    spvOptions.generateDebugInfo = false;
    spvOptions.disableOptimizer = false;
    spvOptions.optimizeSize = true;
#else
    spvOptions.generateDebugInfo = true;
    spvOptions.disableOptimizer = true;
    spvOptions.optimizeSize = false;
#endif
    
    glslang::GlslangToSpv(*program.getIntermediate(glslangStage), outSpirv, &spvOptions);
    
    return true;
}

} // namespace lupine

#endif // LUPINE_HAS_VULKAN

