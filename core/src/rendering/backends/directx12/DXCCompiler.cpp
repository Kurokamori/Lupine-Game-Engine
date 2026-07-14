#include "lupine/rendering/backends/directx12/DXCCompiler.hpp"

#ifdef LUPINE_HAS_DIRECTX12

#include "lupine/logger/Logger.hpp"
#include <d3d12.h>
#include <d3d12shader.h>
#include <dxcapi.h>
#include <wrl/client.h>

// Dynamic loading - no static linking to dxcompiler.lib
// This allows the engine to run without dxcompiler.dll when using precompiled shaders

namespace lupine {

using Microsoft::WRL::ComPtr;

// DXC function pointer type
typedef HRESULT(WINAPI* DxcCreateInstanceProc)(REFCLSID rclsid, REFIID riid, LPVOID* ppv);

// Static DXC state
static HMODULE g_dxcModule = nullptr;
static DxcCreateInstanceProc g_DxcCreateInstance = nullptr;
static ComPtr<IDxcUtils> g_dxcUtils;
static ComPtr<IDxcCompiler3> g_dxcCompiler;
static ComPtr<IDxcIncludeHandler> g_dxcIncludeHandler;
static bool g_dxcInitialized = false;
static bool g_dxcLoadAttempted = false;

bool DXCCompiler::initialize() {
    if (g_dxcInitialized) {
        return true;
    }

    // Only attempt to load once
    if (g_dxcLoadAttempted) {
        return false;
    }
    g_dxcLoadAttempted = true;

    // Try to load dxcompiler.dll dynamically
    g_dxcModule = LoadLibraryW(L"dxcompiler.dll");
    if (!g_dxcModule) {
        LOG_ERROR(LogCategory::Render, "[DXC] Failed to load dxcompiler.dll - DirectX 12 shader compilation unavailable");
        return false;
    }

    // Get DxcCreateInstance function
    g_DxcCreateInstance = reinterpret_cast<DxcCreateInstanceProc>(
        GetProcAddress(g_dxcModule, "DxcCreateInstance"));
    if (!g_DxcCreateInstance) {
        LOG_ERROR(LogCategory::Render, "[DXC] Failed to get DxcCreateInstance from dxcompiler.dll");
        FreeLibrary(g_dxcModule);
        g_dxcModule = nullptr;
        return false;
    }

    // Create DXC utils
    HRESULT hr = g_DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&g_dxcUtils));
    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "[DXC] Failed to create DxcUtils: HRESULT = 0x{:08X}", static_cast<unsigned int>(hr));
        FreeLibrary(g_dxcModule);
        g_dxcModule = nullptr;
        g_DxcCreateInstance = nullptr;
        return false;
    }

    // Create DXC compiler
    hr = g_DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&g_dxcCompiler));
    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "[DXC] Failed to create DxcCompiler: HRESULT = 0x{:08X}", static_cast<unsigned int>(hr));
        g_dxcUtils.Reset();
        FreeLibrary(g_dxcModule);
        g_dxcModule = nullptr;
        g_DxcCreateInstance = nullptr;
        return false;
    }

    // Create default include handler
    hr = g_dxcUtils->CreateDefaultIncludeHandler(&g_dxcIncludeHandler);
    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "[DXC] Failed to create include handler: HRESULT = 0x{:08X}", static_cast<unsigned int>(hr));
        g_dxcCompiler.Reset();
        g_dxcUtils.Reset();
        FreeLibrary(g_dxcModule);
        g_dxcModule = nullptr;
        g_DxcCreateInstance = nullptr;
        return false;
    }

    g_dxcInitialized = true;
    LOG_INFO(LogCategory::Render, "[DXC] Shader compiler initialized (SM 6.0, HLSL 2021)");
    return true;
}

void DXCCompiler::shutdown() {
    if (!g_dxcInitialized && !g_dxcModule) {
        return;
    }

    g_dxcIncludeHandler.Reset();
    g_dxcCompiler.Reset();
    g_dxcUtils.Reset();

    if (g_dxcModule) {
        FreeLibrary(g_dxcModule);
        g_dxcModule = nullptr;
    }
    g_DxcCreateInstance = nullptr;
    g_dxcInitialized = false;
    g_dxcLoadAttempted = false;

}

bool DXCCompiler::isAvailable() {
    return g_dxcInitialized;
}

const wchar_t* DXCCompiler::getShaderTarget(ShaderStage stage) {
    switch (stage) {
        case ShaderStage::Vertex:   return L"vs_6_0";
        case ShaderStage::Fragment: return L"ps_6_0";
        case ShaderStage::Geometry: return L"gs_6_0";
        case ShaderStage::Compute:  return L"cs_6_0";
        default:                    return L"vs_6_0";
    }
}

bool DXCCompiler::compile(
    const std::string& source,
    ShaderStage stage,
    const std::string& entryPoint,
    std::vector<uint8_t>& outBytecode,
    std::string& outErrors
) {
    if (!g_dxcInitialized) {
        if (!initialize()) {
            outErrors = "Failed to initialize DXC compiler";
            return false;
        }
    }

    // Helper to get shader stage name
    auto getShaderStageName = [](ShaderStage s) -> const char* {
        switch (s) {
            case ShaderStage::Vertex:   return "Vertex";
            case ShaderStage::Fragment: return "Fragment/Pixel";
            case ShaderStage::Geometry: return "Geometry";
            case ShaderStage::Compute:  return "Compute";
            default:                    return "Unknown";
        }
    };

    if (source.empty()) {
        outErrors = "Empty shader source";
        LOG_ERROR(LogCategory::Render, "[DXC] {}", outErrors);
        return false;
    }

    // Convert source to blob
    ComPtr<IDxcBlobEncoding> sourceBlob;
    HRESULT hr = g_dxcUtils->CreateBlob(
        source.c_str(),
        static_cast<UINT32>(source.size()),
        CP_UTF8,
        &sourceBlob
    );
    if (FAILED(hr)) {
        outErrors = "Failed to create source blob";
        LOG_ERROR(LogCategory::Render, "[DXC] {}: HRESULT = 0x{:08X}", outErrors, static_cast<unsigned int>(hr));
        return false;
    }

    // Convert entry point to wide string
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, entryPoint.c_str(), -1, nullptr, 0);
    std::wstring wideEntryPoint(wideLen > 0 ? wideLen - 1 : 0, L'\0');
    if (wideLen > 0) {
        MultiByteToWideChar(CP_UTF8, 0, entryPoint.c_str(), -1, wideEntryPoint.data(), wideLen);
    }
    const wchar_t* target = getShaderTarget(stage);

    // Build compiler arguments
    std::vector<LPCWSTR> arguments;
    arguments.push_back(L"-E");
    arguments.push_back(wideEntryPoint.c_str());
    arguments.push_back(L"-T");
    arguments.push_back(target);

    // Column-major matrices (to match GLM)
    arguments.push_back(L"-Zpc");

    // Enable SM 6.0 features
    arguments.push_back(L"-HV");
    arguments.push_back(L"2021");  // HLSL 2021 for modern features

#ifdef _DEBUG
    arguments.push_back(L"-Zi");   // Debug info
    arguments.push_back(L"-Od");   // Disable optimization
    
#else
    arguments.push_back(L"-O3");   // Maximum optimization
    
#endif

    // Create DxcBuffer for source
    DxcBuffer sourceBuffer;
    sourceBuffer.Ptr = sourceBlob->GetBufferPointer();
    sourceBuffer.Size = sourceBlob->GetBufferSize();
    sourceBuffer.Encoding = CP_UTF8;

    // Compile shader
    ComPtr<IDxcResult> result;
    hr = g_dxcCompiler->Compile(
        &sourceBuffer,
        arguments.data(),
        static_cast<UINT32>(arguments.size()),
        g_dxcIncludeHandler.Get(),
        IID_PPV_ARGS(&result)
    );

    if (FAILED(hr) || !result) {
        outErrors = "DXC Compile() invocation failed";
        LOG_ERROR(LogCategory::Render, "[DXC] {}: HRESULT = 0x{:08X}", outErrors, static_cast<unsigned int>(hr));
        return false;
    }

    // Check for errors
    ComPtr<IDxcBlobUtf8> errors;
    result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
    if (errors && errors->GetStringLength() > 0) {
        outErrors = std::string(errors->GetStringPointer(), errors->GetStringLength());
    }

    // Check compilation status
    HRESULT status;
    result->GetStatus(&status);
    if (FAILED(status)) {
        LOG_ERROR(LogCategory::Render, "[DXC] Compilation failed - HRESULT: 0x{:08X}", static_cast<unsigned int>(status));
        LOG_ERROR(LogCategory::Render, "[DXC] Stage: {}, EntryPoint: '{}', Target: {}",
            getShaderStageName(stage), entryPoint,
            stage == ShaderStage::Vertex ? "vs_6_0" :
            stage == ShaderStage::Fragment ? "ps_6_0" :
            stage == ShaderStage::Geometry ? "gs_6_0" : "cs_6_0");
        LOG_ERROR(LogCategory::Render, "[DXC] Error details: {}", outErrors);
        return false;
    }

    // Get compiled bytecode
    ComPtr<IDxcBlob> bytecodeBlob;
    ComPtr<IDxcBlobUtf16> shaderName;
    hr = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&bytecodeBlob), &shaderName);
    if (FAILED(hr) || !bytecodeBlob) {
        outErrors = "Failed to get compiled bytecode";
        LOG_ERROR(LogCategory::Render, "[DXC] {}: HRESULT = 0x{:08X}", outErrors, static_cast<unsigned int>(hr));
        return false;
    }

    // Copy bytecode to output
    const uint8_t* bytecodeData = static_cast<const uint8_t*>(bytecodeBlob->GetBufferPointer());
    size_t bytecodeSize = bytecodeBlob->GetBufferSize();
    outBytecode.assign(bytecodeData, bytecodeData + bytecodeSize);

    // Verify DXIL header (DXBC container magic: 'DXBC' = 0x43425844)
    if (bytecodeSize >= 4) {
        uint32_t magic = *reinterpret_cast<const uint32_t*>(bytecodeData);
        if (magic != 0x43425844) {
            LOG_WARN(LogCategory::Render, "[DXC] Unexpected bytecode header: 0x{:08X} (expected DXBC 0x43425844), {} bytes",
                magic, bytecodeSize);
        }
    } else {
        LOG_WARN(LogCategory::Render, "[DXC] Compiled bytecode unexpectedly small: {} bytes", bytecodeSize);
    }

    return true;
}

bool DXCCompiler::reflect(
    const std::vector<uint8_t>& bytecode,
    ShaderReflectionData& outReflection
) {
    outReflection.constantBuffers.clear();
    outReflection.uniformOffsets.clear();

    if (bytecode.empty()) {
        return false;
    }

    // Try DXC reflection (requires dxcompiler.dll to be loaded).
    if (g_dxcUtils) {
        DxcBuffer reflectionData;
        reflectionData.Ptr = bytecode.data();
        reflectionData.Size = bytecode.size();
        reflectionData.Encoding = 0;

        ComPtr<ID3D12ShaderReflection> reflector;
        HRESULT hr = g_dxcUtils->CreateReflection(&reflectionData, IID_PPV_ARGS(&reflector));
        if (SUCCEEDED(hr) && reflector) {
            D3D12_SHADER_DESC shaderDesc;
            reflector->GetDesc(&shaderDesc);

            for (UINT cbIdx = 0; cbIdx < shaderDesc.ConstantBuffers; ++cbIdx) {
                ID3D12ShaderReflectionConstantBuffer* cbReflection = reflector->GetConstantBufferByIndex(cbIdx);
                D3D12_SHADER_BUFFER_DESC cbDesc;
                cbReflection->GetDesc(&cbDesc);

                // Find binding info for this cbuffer
                D3D12_SHADER_INPUT_BIND_DESC bindDesc{};
                for (UINT resIdx = 0; resIdx < shaderDesc.BoundResources; ++resIdx) {
                    reflector->GetResourceBindingDesc(resIdx, &bindDesc);
                    if (bindDesc.Type == D3D_SIT_CBUFFER &&
                        strcmp(bindDesc.Name, cbDesc.Name) == 0) {
                        break;
                    }
                }

                // Store constant buffer metadata
                ShaderCBufferInfo cbInfo;
                cbInfo.name = cbDesc.Name;
                cbInfo.size = cbDesc.Size;
                cbInfo.slot = bindDesc.BindPoint;
                outReflection.constantBuffers.push_back(cbInfo);

                // Extract all variable offsets (skip padding vars starting with '_')
                for (UINT varIdx = 0; varIdx < cbDesc.Variables; ++varIdx) {
                    ID3D12ShaderReflectionVariable* varReflection = cbReflection->GetVariableByIndex(varIdx);
                    D3D12_SHADER_VARIABLE_DESC varDesc;
                    varReflection->GetDesc(&varDesc);
                    if (varDesc.Name[0] == '_') continue;
                    outReflection.uniformOffsets[varDesc.Name] = varDesc.StartOffset;
                }
            }

            if (!outReflection.uniformOffsets.empty()) {
                return true;
            }
        }
    }

    // Fallback: the PerObjectUniforms fixed layout (352 bytes).
    // Shaders that declare u_NormalMatrix use this full layout (the
    // transpiler emits it).  Compact shaders (skybox, gizmo, etc.)
    // do NOT use these offsets — their uniforms are resolved via
    // dynamic allocation in getUniformLocation(), which naturally
    // matches the cbuffer declaration order.
    outReflection.uniformOffsets["u_ViewProjection"] = 0;
    outReflection.uniformOffsets["u_Model"] = 64;
    outReflection.uniformOffsets["u_NormalMatrix"] = 128;
    outReflection.uniformOffsets["u_TintColor"] = 192;
    outReflection.uniformOffsets["u_Color"] = 192;
    outReflection.uniformOffsets["u_UseTexture"] = 208;
    outReflection.uniformOffsets["u_AlphaCutoff"] = 212;
    outReflection.uniformOffsets["u_UVRect"] = 224;
    outReflection.uniformOffsets["u_TextureFlags"] = 240;
    outReflection.uniformOffsets["u_MaterialParams1"] = 256;
    outReflection.uniformOffsets["u_MaterialParams2"] = 272;
    outReflection.uniformOffsets["u_CameraPosition"] = 288;
    outReflection.uniformOffsets["u_AlbedoColor"] = 304;
    outReflection.uniformOffsets["u_EmissiveColor"] = 320;
    outReflection.uniformOffsets["u_ReceiveShadow"] = 336;

    return true;
}

} // namespace lupine

#endif // LUPINE_HAS_DIRECTX12
