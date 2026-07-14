#pragma once

#ifdef LUPINE_HAS_VULKAN

#include "lupine/rendering/gfx/GfxTypes.hpp"
#include <vector>
#include <string>
#include <cstdint>

namespace lupine {

/**
 * A single reflected uniform block member with its byte offset and size
 * within the containing block (computed using std140 layout rules).
 */
struct GlslUniformMember {
    std::string name;
    uint32_t offset = 0;
    uint32_t size = 0;
};

/**
 * Reflection result for a Vulkan GLSL shader. Captures the member layout of
 * the push_constant block and the per-material uniform block (set=0, binding=2)
 * so the command list can route engine uniforms (set by name or via the
 * canonical push-constant blob) to the exact byte offset each shader expects.
 */
struct GlslReflection {
    std::vector<GlslUniformMember> pushConstants;  // members of the push_constant block
    std::vector<GlslUniformMember> materialUBO;    // members of the set=0, binding=2 block
    uint32_t materialUBOSize = 0;                  // std140 size of the binding=2 block
};

/**
 * GLSL to SPIR-V compiler for Vulkan
 * Uses glslang to compile GLSL source code to SPIR-V bytecode
 */
class GLSLCompiler {
public:
    /**
     * Compile GLSL source code to SPIR-V bytecode
     * @param source GLSL source code
     * @param stage Shader stage (vertex, fragment, etc.)
     * @param outSpirv Output SPIR-V bytecode (uint32_t aligned)
     * @param errorLog Output error log if compilation fails
     * @return true if compilation succeeded, false otherwise
     */
    static bool compileGLSLToSPIRV(
        const char* source,
        ShaderStage stage,
        std::vector<uint32_t>& outSpirv,
        std::string& errorLog
    );

    /**
     * Reflect the uniform layout of a Vulkan GLSL shader by parsing its
     * push_constant block and its set=0, binding=2 material uniform block.
     * Member offsets are computed using std140 rules (which match std430 for
     * the 16-byte-aligned member types the engine uses in these blocks).
     * This is deterministic and does not require the shader to be compiled.
     * @param source GLSL source code
     * @param out Reflection result (members + material block size)
     */
    static void reflectUniformLayout(const char* source, GlslReflection& out);

    /**
     * Initialize the compiler (must be called once before first use)
     */
    static void initialize();

    /**
     * Shutdown the compiler (should be called on cleanup)
     */
    static void shutdown();

private:
    static bool s_initialized;
};

} // namespace lupine

#endif // LUPINE_HAS_VULKAN

