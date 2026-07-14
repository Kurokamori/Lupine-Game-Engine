#pragma once

#include "../../gfx/GfxTypes.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace lupine {

#if defined(LUPINE_HAS_DIRECTX11) || defined(LUPINE_HAS_DIRECTX12)

/**
 * HLSL shader compiler.
 * Compiles HLSL source code to bytecode for DirectX 11.
 */
class HLSLCompiler {
public:
    /**
     * Compile HLSL source code to bytecode.
     *
     * @param source HLSL source code
     * @param stage Shader stage (vertex, pixel, etc.)
     * @param entryPoint Entry point function name (default: "main")
     * @param outBytecode Output bytecode
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
     * Compile HLSL source code from file to bytecode.
     *
     * @param filename HLSL source file path
     * @param stage Shader stage (vertex, pixel, etc.)
     * @param entryPoint Entry point function name (default: "main")
     * @param outBytecode Output bytecode
     * @param outErrors Output error messages (if compilation fails)
     * @return true if compilation succeeded, false otherwise
     */
    static bool compileFromFile(
        const std::string& filename,
        ShaderStage stage,
        const std::string& entryPoint,
        std::vector<uint8_t>& outBytecode,
        std::string& outErrors
    );

    /**
     * Get shader model target string for a shader stage.
     *
     * @param stage Shader stage
     * @return Shader model target string (e.g., "vs_5_0", "ps_5_0")
     */
    static const char* getShaderTarget(ShaderStage stage);

    /**
     * Reflect shader bytecode to extract constant buffer, texture, and sampler information.
     * This is used for automatic resource binding.
     *
     * @param bytecode Compiled shader bytecode
     * @param outConstantBuffers Output constant buffer information
     * @param outTextures Output texture binding information
     * @param outSamplers Output sampler binding information
     * @return true if reflection succeeded, false otherwise
     */
    struct ConstantBufferInfo {
        std::string name;
        uint32_t size;
        uint32_t slot;
    };

    struct TextureBindingInfo {
        std::string name;
        uint32_t slot;
    };

    struct VariableInfo {
        std::string name;
        uint32_t offset;
        uint32_t size;
    };

    static bool reflectShader(
        const std::vector<uint8_t>& bytecode,
        std::vector<ConstantBufferInfo>& outConstantBuffers,
        std::vector<TextureBindingInfo>& outTextures,
        std::vector<TextureBindingInfo>& outSamplers
    );

    /**
     * Reflect shader bytecode to extract variable offsets from a specific constant buffer.
     * Used to map uniform names to their byte offsets for setUniform* functions.
     *
     * @param bytecode Compiled shader bytecode
     * @param cbSlot Constant buffer slot to reflect (usually 0 for push constants equivalent)
     * @param outVariables Output variable information (name, offset, size)
     * @return true if reflection succeeded, false otherwise
     */
    static bool reflectConstantBufferVariables(
        const std::vector<uint8_t>& bytecode,
        uint32_t cbSlot,
        std::vector<VariableInfo>& outVariables
    );
};

#endif // LUPINE_HAS_DIRECTX11 || LUPINE_HAS_DIRECTX12

} // namespace lupine
