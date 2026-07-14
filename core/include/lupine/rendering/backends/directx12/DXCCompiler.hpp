#pragma once

#include "../../gfx/GfxTypes.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace lupine {

#ifdef LUPINE_HAS_DIRECTX12

/**
 * Shader reflection data for constant buffer variables
 */
struct ShaderUniformInfo {
    std::string name;
    uint32_t offset;
    uint32_t size;
};

/**
 * Shader reflection data for a constant buffer
 */
struct ShaderCBufferInfo {
    std::string name;
    uint32_t slot;
    uint32_t size;
    std::vector<ShaderUniformInfo> variables;
};

/**
 * Complete shader reflection data
 */
struct ShaderReflectionData {
    std::vector<ShaderCBufferInfo> constantBuffers;
    std::unordered_map<std::string, uint32_t> uniformOffsets; // Quick lookup: name -> offset in slot 0
};

/**
 * DirectX Shader Compiler (DXC) wrapper for Shader Model 6.0+
 * Uses dxcompiler.dll for modern HLSL features like NonUniformResourceIndex
 */
class DXCCompiler {
public:
    /**
     * Initialize the DXC compiler.
     * Loads dxcompiler.dll and creates compiler instance.
     * @return true if initialization succeeded, false otherwise
     */
    static bool initialize();

    /**
     * Shutdown the DXC compiler and release resources.
     */
    static void shutdown();

    /**
     * Check if DXC is available and initialized.
     * @return true if DXC is ready to use
     */
    static bool isAvailable();

    /**
     * Compile HLSL source code to DXIL bytecode using Shader Model 6.0.
     *
     * @param source HLSL source code
     * @param stage Shader stage (vertex, pixel, etc.)
     * @param entryPoint Entry point function name (default: "main")
     * @param outBytecode Output DXIL bytecode
     * @param outErrors Output error messages (if compilation fails)
     * @return true if compilation succeeded, false otherwise
     */
    static bool compile(
        const std::string& source,
        ShaderStage stage,
        const std::string& entryPoint,
        std::vector<uint8_t>& outBytecode,
        std::string& outErrors
    );

    /**
     * Perform shader reflection on compiled bytecode.
     * Extracts constant buffer and uniform information.
     *
     * @param bytecode Compiled shader bytecode (DXIL or DXBC)
     * @param outReflection Output reflection data
     * @return true if reflection succeeded, false otherwise
     */
    static bool reflect(
        const std::vector<uint8_t>& bytecode,
        ShaderReflectionData& outReflection
    );

    /**
     * Get shader model target string for a shader stage (SM 6.0).
     *
     * @param stage Shader stage
     * @return Shader model target string (e.g., "vs_6_0", "ps_6_0")
     */
    static const wchar_t* getShaderTarget(ShaderStage stage);
};

#endif // LUPINE_HAS_DIRECTX12

} // namespace lupine
