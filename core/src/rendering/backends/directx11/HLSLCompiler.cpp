#include "lupine/rendering/backends/directx11/HLSLCompiler.hpp"

#if defined(LUPINE_HAS_DIRECTX11) || defined(LUPINE_HAS_DIRECTX12)

#include "lupine/logger/Logger.hpp"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <fstream>
#include <sstream>
#include <wrl/client.h>

#pragma comment(lib, "d3dcompiler.lib")

namespace lupine {

using Microsoft::WRL::ComPtr;

const char* HLSLCompiler::getShaderTarget(ShaderStage stage) {
    switch (stage) {
        case ShaderStage::Vertex: return "vs_5_0";
        case ShaderStage::Fragment: return "ps_5_0";
        case ShaderStage::Geometry: return "gs_5_0";
        case ShaderStage::Compute: return "cs_5_0";
        default: return "vs_5_0";
    }
}

bool HLSLCompiler::compile(
    const std::string& source,
    ShaderStage stage,
    const std::string& entryPoint,
    std::vector<uint8_t>& outBytecode,
    std::string& outErrors
) {
    // Helper to get shader stage name
    auto getShaderStageName = [](ShaderStage s) -> const char* {
        switch (s) {
            case ShaderStage::Vertex: return "Vertex";
            case ShaderStage::Fragment: return "Fragment/Pixel";
            case ShaderStage::Geometry: return "Geometry";
            case ShaderStage::Compute: return "Compute";
            default: return "Unknown";
        }
    };

    if (source.empty()) {
        outErrors = "Empty shader source";
        LOG_ERROR(LogCategory::Render, "[HLSL] {}", outErrors);
        return false;
    }

    UINT compileFlags = 0;

    // Use column-major matrix packing to match GLM's memory layout
    // GLM stores matrices in column-major order, HLSL with this flag interprets them correctly
    // Shaders use mul(matrix, vector) form (column vector convention)
    compileFlags |= D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR;

#ifdef _DEBUG
    compileFlags |= D3DCOMPILE_DEBUG;
    compileFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
    
#else
    compileFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
    
#endif

    const char* target = getShaderTarget(stage);

    ComPtr<ID3DBlob> shaderBlob;
    ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3DCompile(
        source.c_str(),
        source.size(),
        nullptr,  // Source name (optional)
        nullptr,  // Defines (optional)
        D3D_COMPILE_STANDARD_FILE_INCLUDE,  // Include handler
        entryPoint.c_str(),
        target,
        compileFlags,
        0,  // Effect flags
        &shaderBlob,
        &errorBlob
    );

    if (FAILED(hr)) {
        if (errorBlob) {
            outErrors = std::string(
                static_cast<const char*>(errorBlob->GetBufferPointer()),
                errorBlob->GetBufferSize()
            );
        } else {
            outErrors = "Unknown shader compilation error";
        }
        LOG_ERROR(LogCategory::Render, "[HLSL] Compilation failed - HRESULT: 0x{:08X}", static_cast<unsigned int>(hr));
        LOG_ERROR(LogCategory::Render, "[HLSL] Stage: {}, EntryPoint: '{}', Target: {}",
            getShaderStageName(stage), entryPoint, target);
        LOG_ERROR(LogCategory::Render, "[HLSL] Error details: {}", outErrors);
        return false;
    }

    // Check for warnings even on success
    if (errorBlob && errorBlob->GetBufferSize() > 0) {
        std::string warnings(
            static_cast<const char*>(errorBlob->GetBufferPointer()),
            errorBlob->GetBufferSize()
        );
        LOG_WARN(LogCategory::Render, "[HLSL] Compilation warnings for {} ({}): {}",
            getShaderStageName(stage), entryPoint, warnings);
    }

    // Copy bytecode to output vector
    const uint8_t* bytecodeData = static_cast<const uint8_t*>(shaderBlob->GetBufferPointer());
    size_t bytecodeSize = shaderBlob->GetBufferSize();
    outBytecode.assign(bytecodeData, bytecodeData + bytecodeSize);

    // Verify DXBC container magic header (0x43425844 = 'DXBC')
    if (bytecodeSize >= 4) {
        uint32_t magic = *reinterpret_cast<const uint8_t*>(bytecodeData) |
                         (static_cast<uint32_t>(*(bytecodeData + 1)) << 8) |
                         (static_cast<uint32_t>(*(bytecodeData + 2)) << 16) |
                         (static_cast<uint32_t>(*(bytecodeData + 3)) << 24);
        if (magic != 0x43425844) {
            LOG_WARN(LogCategory::Render, "[HLSL] Unexpected bytecode header: 0x{:08X} (expected DXBC 0x43425844)",
                magic);
        }
    }

    return true;
}

bool HLSLCompiler::compileFromFile(
    const std::string& filename,
    ShaderStage stage,
    const std::string& entryPoint,
    std::vector<uint8_t>& outBytecode,
    std::string& outErrors
) {

    // Read file contents
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        outErrors = "Failed to open file: " + filename;
        LOG_ERROR(LogCategory::Render, "[HLSL] {}", outErrors);
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();
    file.close();

    return compile(source, stage, entryPoint, outBytecode, outErrors);
}

bool HLSLCompiler::reflectShader(
    const std::vector<uint8_t>& bytecode,
    std::vector<ConstantBufferInfo>& outConstantBuffers,
    std::vector<TextureBindingInfo>& outTextures,
    std::vector<TextureBindingInfo>& outSamplers
) {

    if (bytecode.empty()) {
        LOG_ERROR(LogCategory::Render, "[HLSL] Cannot reflect empty bytecode");
        return false;
    }

    ComPtr<ID3D11ShaderReflection> reflector;

    HRESULT hr = D3DReflect(
        bytecode.data(),
        bytecode.size(),
        IID_ID3D11ShaderReflection,
        (void**)reflector.GetAddressOf()
    );

    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "[HLSL] Failed to reflect shader - HRESULT: 0x{:08X}", static_cast<unsigned int>(hr));
        return false;
    }

    D3D11_SHADER_DESC shaderDesc;
    reflector->GetDesc(&shaderDesc);

    // Reflect constant buffers
    for (UINT i = 0; i < shaderDesc.ConstantBuffers; ++i) {
        ID3D11ShaderReflectionConstantBuffer* cbReflection = reflector->GetConstantBufferByIndex(i);

        D3D11_SHADER_BUFFER_DESC cbDesc;
        cbReflection->GetDesc(&cbDesc);

        // Find the binding slot for this constant buffer
        for (UINT j = 0; j < shaderDesc.BoundResources; ++j) {
            D3D11_SHADER_INPUT_BIND_DESC bindDesc;
            reflector->GetResourceBindingDesc(j, &bindDesc);

            if (bindDesc.Type == D3D_SIT_CBUFFER && std::string(bindDesc.Name) == cbDesc.Name) {
                ConstantBufferInfo info;
                info.name = cbDesc.Name;
                info.size = cbDesc.Size;
                info.slot = bindDesc.BindPoint;
                outConstantBuffers.push_back(info);
                
                break;
            }
        }
    }

    // Reflect textures and samplers
    for (UINT i = 0; i < shaderDesc.BoundResources; ++i) {
        D3D11_SHADER_INPUT_BIND_DESC bindDesc;
        reflector->GetResourceBindingDesc(i, &bindDesc);

        if (bindDesc.Type == D3D_SIT_TEXTURE) {
            TextureBindingInfo info;
            info.name = bindDesc.Name;
            info.slot = bindDesc.BindPoint;
            outTextures.push_back(info);
            
        } else if (bindDesc.Type == D3D_SIT_SAMPLER) {
            TextureBindingInfo info;
            info.name = bindDesc.Name;
            info.slot = bindDesc.BindPoint;
            outSamplers.push_back(info);
            
        }
    }

    return true;
}

bool HLSLCompiler::reflectConstantBufferVariables(
    const std::vector<uint8_t>& bytecode,
    uint32_t cbSlot,
    std::vector<VariableInfo>& outVariables
) {
    if (bytecode.empty()) {
        return false;
    }

    ComPtr<ID3D11ShaderReflection> reflector;
    HRESULT hr = D3DReflect(
        bytecode.data(),
        bytecode.size(),
        IID_ID3D11ShaderReflection,
        (void**)reflector.GetAddressOf()
    );

    if (FAILED(hr)) {
        return false;
    }

    D3D11_SHADER_DESC shaderDesc;
    reflector->GetDesc(&shaderDesc);

    // Find the constant buffer at the specified slot
    for (UINT i = 0; i < shaderDesc.ConstantBuffers; ++i) {
        ID3D11ShaderReflectionConstantBuffer* cbReflection = reflector->GetConstantBufferByIndex(i);

        D3D11_SHADER_BUFFER_DESC cbDesc;
        cbReflection->GetDesc(&cbDesc);

        // Find the binding slot for this constant buffer
        uint32_t slot = UINT32_MAX;
        for (UINT j = 0; j < shaderDesc.BoundResources; ++j) {
            D3D11_SHADER_INPUT_BIND_DESC bindDesc;
            reflector->GetResourceBindingDesc(j, &bindDesc);

            if (bindDesc.Type == D3D_SIT_CBUFFER && std::string(bindDesc.Name) == cbDesc.Name) {
                slot = bindDesc.BindPoint;
                break;
            }
        }

        if (slot != cbSlot) {
            continue;
        }

        // Found the right constant buffer - extract all variables
        for (UINT v = 0; v < cbDesc.Variables; ++v) {
            ID3D11ShaderReflectionVariable* varReflection = cbReflection->GetVariableByIndex(v);

            D3D11_SHADER_VARIABLE_DESC varDesc;
            varReflection->GetDesc(&varDesc);

            VariableInfo info;
            info.name = varDesc.Name;
            info.offset = varDesc.StartOffset;
            info.size = varDesc.Size;
            outVariables.push_back(info);

        }

        return true;
    }

    return false;
}

} // namespace lupine

#endif // LUPINE_HAS_DIRECTX11 || LUPINE_HAS_DIRECTX12
