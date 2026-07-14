#include "lupine/rendering/backends/GfxDeviceDX12.hpp"

#ifdef LUPINE_HAS_DIRECTX12

#include "lupine/rendering/backends/directx12/DirectX12State.hpp"
#include "lupine/rendering/backends/directx12/GfxCommandListDX12.hpp"
#include "lupine/rendering/backends/directx12/DXCCompiler.hpp"  // DXC for SM 6.0 support
#include "lupine/rendering/Mesh.hpp"
#include "lupine/rendering/Font.hpp"
#include "lupine/rendering/FontBaker.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/platform/FileSystem.hpp"
#include "lupine/platform/PackFile.hpp"
#include "lupine/asset/Asset.hpp"
#include <stb_truetype.h>
#include <vector>
#include <unordered_map>
#include <cstring>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#endif

namespace lupine {

namespace {

// True when the named environment variable is set to anything other than "" or "0".
// Uses _dupenv_s rather than getenv (deprecated by MSVC); the value is heap-allocated
// by the CRT and must be freed here.
bool IsEnvFlagEnabled(const char* name) {
    char* value = nullptr;
    size_t length = 0;
    if (_dupenv_s(&value, &length, name) != 0 || !value) {
        return false;
    }
    const bool enabled = (value[0] != '\0' && value[0] != '0');
    std::free(value);
    return enabled;
}

} // namespace

struct GfxDeviceDX12::Impl {
    DirectX12State state;
    GfxDeviceCaps caps;
    bool initialized = false;

    // Default filtering
    FilterMode defaultMinFilter = FilterMode::Linear;
    FilterMode defaultMagFilter = FilterMode::Linear;

    // Mesh storage
    std::unordered_map<uint32_t, GPUMesh> meshes;
    uint32_t nextMeshID = 1;

    // Font storage
    std::unordered_map<uint32_t, FontAtlas> fonts;
    std::unordered_map<uint32_t, FontDesc> fontDescs;
    uint32_t nextFontID = 1;

    // Helper to create a committed buffer resource
    bool createBuffer(uint64_t size, D3D12_HEAP_TYPE heapType,
                     D3D12_RESOURCE_STATES initialState,
                     ComPtr<ID3D12Resource>& outResource);

    // Helper to create root signature
    bool createRootSignature(const PipelineDesc& desc, DX12RootSignature& outRootSig);
};

bool GfxDeviceDX12::Impl::createBuffer(uint64_t size, D3D12_HEAP_TYPE heapType,
                                        D3D12_RESOURCE_STATES initialState,
                                        ComPtr<ID3D12Resource>& outResource) {
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = heapType;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Alignment = 0;
    resourceDesc.Width = size;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    HRESULT hr = state.device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        initialState,
        nullptr,
        IID_PPV_ARGS(&outResource)
    );

    return SUCCEEDED(hr);
}

bool GfxDeviceDX12::Impl::createRootSignature(const PipelineDesc&, DX12RootSignature& outRootSig) {
    // Create a root signature with:
    // - Root CBV for push constants (slot 0)
    // - Descriptor table for SRVs
    // - Descriptor table for samplers
    // - Additional root CBVs for uniform buffers

    std::vector<D3D12_ROOT_PARAMETER> rootParams;

    // Root parameter 0: Root CBV for material data (push constants equivalent, slot b0)
    D3D12_ROOT_PARAMETER materialCBV = {};
    materialCBV.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    materialCBV.Descriptor.ShaderRegister = 0;
    materialCBV.Descriptor.RegisterSpace = 0;
    materialCBV.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    outRootSig.cbvRootParamIndex = static_cast<int32_t>(rootParams.size());
    rootParams.push_back(materialCBV);

    // Root parameter 1: Root CBV for camera data (slot b1, used by Toon/PBR shaders)
    D3D12_ROOT_PARAMETER cameraCBV = {};
    cameraCBV.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    cameraCBV.Descriptor.ShaderRegister = 1;
    cameraCBV.Descriptor.RegisterSpace = 0;
    cameraCBV.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParams.push_back(cameraCBV);

    // Root parameter 2: Root CBV for bone transforms (slot b2)
    D3D12_ROOT_PARAMETER boneCBV = {};
    boneCBV.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    boneCBV.Descriptor.ShaderRegister = 2;
    boneCBV.Descriptor.RegisterSpace = 0;
    boneCBV.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParams.push_back(boneCBV);

    // Root parameter 3: Root CBV for light data (slot b3)
    D3D12_ROOT_PARAMETER lightCBV = {};
    lightCBV.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    lightCBV.Descriptor.ShaderRegister = 3;
    lightCBV.Descriptor.RegisterSpace = 0;
    lightCBV.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParams.push_back(lightCBV);

    // Descriptor ranges for SRVs (textures)
    D3D12_DESCRIPTOR_RANGE srvRange = {};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 32;  // Support up to 32 textures
    srvRange.BaseShaderRegister = 0;
    srvRange.RegisterSpace = 0;
    srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // Root parameter 4: Descriptor table for SRVs
    D3D12_ROOT_PARAMETER srvTable = {};
    srvTable.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    srvTable.DescriptorTable.NumDescriptorRanges = 1;
    srvTable.DescriptorTable.pDescriptorRanges = &srvRange;
    srvTable.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    outRootSig.descriptorTableIndex = static_cast<int32_t>(rootParams.size());
    rootParams.push_back(srvTable);

    // Descriptor ranges for samplers
    D3D12_DESCRIPTOR_RANGE samplerRange = {};
    samplerRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
    samplerRange.NumDescriptors = 16;  // Support up to 16 samplers
    samplerRange.BaseShaderRegister = 0;
    samplerRange.RegisterSpace = 0;
    samplerRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // Root parameter 5: Descriptor table for samplers
    D3D12_ROOT_PARAMETER samplerTable = {};
    samplerTable.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    samplerTable.DescriptorTable.NumDescriptorRanges = 1;
    samplerTable.DescriptorTable.pDescriptorRanges = &samplerRange;
    samplerTable.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    outRootSig.samplerTableIndex = static_cast<int32_t>(rootParams.size());
    rootParams.push_back(samplerTable);

    // Static samplers (for shadow mapping comparison sampler, etc.)
    // Note: Dynamic sampler table uses registers s0-s15 (16 samplers), so static samplers
    // must start at s16 or higher to avoid overlap
    D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
    staticSamplers[0].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[0].MipLODBias = 0;
    staticSamplers[0].MaxAnisotropy = 1;
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    staticSamplers[0].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    staticSamplers[0].MinLOD = 0.0f;
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers[0].ShaderRegister = 16;  // Use slot 16 for shadow comparison sampler (outside dynamic sampler range s0-s15)
    staticSamplers[0].RegisterSpace = 0;
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // Create root signature description
    D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
    rootSigDesc.NumParameters = static_cast<UINT>(rootParams.size());
    rootSigDesc.pParameters = rootParams.data();
    rootSigDesc.NumStaticSamplers = 1;
    rootSigDesc.pStaticSamplers = staticSamplers;
    rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // Serialize root signature
    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                              &signature, &error);
    if (FAILED(hr)) {
        if (error) {
            LOG_ERROR(LogCategory::Render, "[DX12] Root signature serialization failed: {}",
                     static_cast<const char*>(error->GetBufferPointer()));
        }
        return false;
    }

    // Create root signature
    hr = state.device->CreateRootSignature(0, signature->GetBufferPointer(),
                                           signature->GetBufferSize(),
                                           IID_PPV_ARGS(&outRootSig.rootSignature));
    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "[DX12] Failed to create root signature: HRESULT = 0x{:08X}",
                 static_cast<unsigned int>(hr));
        return false;
    }

    return true;
}

GfxDeviceDX12::GfxDeviceDX12()
    : m_impl(std::make_unique<Impl>()) {
}

GfxDeviceDX12::~GfxDeviceDX12() {
    if (m_impl->initialized) {
        shutdown();
    }
}

bool GfxDeviceDX12::initialize() {
    if (m_impl->initialized) {
        return true;
    }

    HRESULT hr;

    // Enable debug layer for diagnostics
    hr = D3D12GetDebugInterface(IID_PPV_ARGS(&m_impl->state.debugInterface));
    if (SUCCEEDED(hr)) {
        m_impl->state.debugInterface->EnableDebugLayer();

        // Enable GPU-based validation when LUPINE_DX12_GPU_VALIDATION is set.
        // GBV catches malformed GPU commands that surface as
        // DXGI_ERROR_DEVICE_HUNG (0x887A0006) — most importantly out-of-bounds
        // resource/descriptor/array indexing, which HLSL leaves undefined (and
        // which can hang the GPU) while GLSL typically clamps it. The reported
        // messages name the exact draw and shader at fault and are drained into
        // the engine log each frame. It is opt-in because it carries a large
        // per-draw performance cost and must not be on for normal release runs.
        if (IsEnvFlagEnabled("LUPINE_DX12_GPU_VALIDATION")) {
            ComPtr<ID3D12Debug1> debugInterface1;
            if (SUCCEEDED(m_impl->state.debugInterface.As(&debugInterface1))) {
                debugInterface1->SetEnableGPUBasedValidation(TRUE);
                debugInterface1->SetEnableSynchronizedCommandQueueValidation(TRUE);
                LOG_WARN(LogCategory::Render, "[DX12] GPU-based validation ENABLED "
                         "(LUPINE_DX12_GPU_VALIDATION set) — expect reduced performance.");
            }
        }

        // Enable DRED (Device Removed Extended Data) for better crash diagnostics
        ComPtr<ID3D12DeviceRemovedExtendedDataSettings1> dredSettings;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dredSettings)))) {
            dredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
            dredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
            dredSettings->SetBreadcrumbContextEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);

        }
    }

    // Create DXGI factory
    UINT dxgiFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;

    hr = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_impl->state.dxgiFactory));
    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "[DX12] Failed to create DXGI factory");
        return false;
    }

    // Enumerate adapters and select the first hardware adapter
    ComPtr<IDXGIAdapter1> adapter1;
    for (UINT i = 0; m_impl->state.dxgiFactory->EnumAdapters1(i, &adapter1) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc;
        adapter1->GetDesc1(&desc);

        // Skip software adapters
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            continue;
        }

        // Check if adapter supports DirectX 12
        if (SUCCEEDED(D3D12CreateDevice(adapter1.Get(), D3D_FEATURE_LEVEL_11_0,
                                        _uuidof(ID3D12Device), nullptr))) {
            hr = adapter1.As(&m_impl->state.dxgiAdapter);
            if (SUCCEEDED(hr)) {
                break;
            }
        }
    }

    if (!m_impl->state.dxgiAdapter) {
        LOG_ERROR(LogCategory::Render, "[DX12] No suitable adapter found");
        return false;
    }

    // Create D3D12 device
    hr = D3D12CreateDevice(
        m_impl->state.dxgiAdapter.Get(),
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&m_impl->state.device)
    );

    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "[DX12] Failed to create D3D12 device");
        return false;
    }

    // Get info queue for debug/validation messages. Created in all build types
    // (not just _DEBUG) so GPU-based-validation findings behind device-removal
    // are captured in the engine log instead of being lost. Messages are drained
    // each frame via DirectX12State::drainDebugMessages(); we deliberately do NOT
    // break on severity so a release run logs the error rather than crashing on a
    // DebugBreak() with no debugger attached.
    hr = m_impl->state.device->QueryInterface(IID_PPV_ARGS(&m_impl->state.infoQueue));
    if (SUCCEEDED(hr)) {
#ifdef _DEBUG
        m_impl->state.infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
#endif
        m_impl->state.infoQueue->SetMuteDebugOutput(FALSE);
    }

    // Create command queue
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.NodeMask = 0;

    hr = m_impl->state.device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_impl->state.commandQueue));
    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "[DX12] Failed to create command queue");
        return false;
    }

    // Create fence
    hr = m_impl->state.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_impl->state.fence));
    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "[DX12] Failed to create fence");
        return false;
    }

    m_impl->state.fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_impl->state.fenceEvent) {
        LOG_ERROR(LogCategory::Render, "[DX12] Failed to create fence event");
        return false;
    }

    // Create per-frame resources
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        // Seed each frame's command-allocator pool with one allocator; beginCommandList
        // grows the pool on demand (one allocator per pass).
        ComPtr<ID3D12CommandAllocator> initialAllocator;
        hr = m_impl->state.device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&initialAllocator)
        );
        if (FAILED(hr)) {
            LOG_ERROR(LogCategory::Render, "[DX12] Failed to create command allocator for frame {}", i);
            return false;
        }
        m_impl->state.frameResources[i].commandAllocatorPool.push_back(initialAllocator);

        // Initialize upload heap (128MB per frame)
        // Increased from 64MB to handle scenes with many skeletal meshes
        // Each skeletal draw uses ~8.5KB (512 bytes material + 8KB bones), supports ~15K skeletal draws
        // Each non-skeletal draw uses ~512 bytes, supports ~250K draws per frame
        m_impl->state.frameResources[i].uploadHeap.initialize(m_impl->state.device.Get(), 128 * 1024 * 1024);
    }

    // Create command list
    hr = m_impl->state.device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_impl->state.frameResources[0].commandAllocatorPool[0].Get(),
        nullptr,
        IID_PPV_ARGS(&m_impl->state.commandList)
    );
    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "[DX12] Failed to create command list");
        return false;
    }

    // Close command list (it starts in recording state)
    m_impl->state.commandList->Close();

    // Initialize descriptor heaps
    m_impl->state.rtvHeap.initialize(m_impl->state.device.Get(),
                                     D3D12_DESCRIPTOR_HEAP_TYPE_RTV, MAX_RTV_DESCRIPTORS, false);
    m_impl->state.dsvHeap.initialize(m_impl->state.device.Get(),
                                     D3D12_DESCRIPTOR_HEAP_TYPE_DSV, MAX_DSV_DESCRIPTORS, false);
    m_impl->state.cbvSrvUavHeap.initialize(m_impl->state.device.Get(),
                                           D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                                           MAX_CBV_SRV_UAV_DESCRIPTORS, true);
    m_impl->state.samplerHeap.initialize(m_impl->state.device.Get(),
                                         D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
                                         MAX_SAMPLER_DESCRIPTORS, true);

    // Initialize staging heaps (non-shader-visible for CPU-side descriptor creation)
    m_impl->state.cbvSrvUavStagingHeap.initialize(m_impl->state.device.Get(),
                                                   D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                                                   MAX_CBV_SRV_UAV_DESCRIPTORS, false);
    m_impl->state.samplerStagingHeap.initialize(m_impl->state.device.Get(),
                                                 D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
                                                 MAX_SAMPLER_DESCRIPTORS, false);

    // Create null SRVs for empty descriptor slots (prevents GPU page faults when
    // transpiled shaders with [flatten] switch sample all texture array elements).
    // Need both Texture2D and TextureCube variants to match shader declarations.
    m_impl->state.nullSrvIndex = m_impl->state.cbvSrvUavStagingHeap.allocate();
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC nullSrvDesc = {};
        nullSrvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        nullSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        nullSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        nullSrvDesc.Texture2D.MipLevels = 1;
        m_impl->state.device->CreateShaderResourceView(
            nullptr,
            &nullSrvDesc,
            m_impl->state.cbvSrvUavStagingHeap.getCPUHandle(m_impl->state.nullSrvIndex)
        );
    }

    // Null TextureCube SRV for shadow cube map slots (t12-t19)
    m_impl->state.nullSrvCubeIndex = m_impl->state.cbvSrvUavStagingHeap.allocate();
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC nullCubeSrvDesc = {};
        nullCubeSrvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        nullCubeSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        nullCubeSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        nullCubeSrvDesc.TextureCube.MipLevels = 1;
        m_impl->state.device->CreateShaderResourceView(
            nullptr,
            &nullCubeSrvDesc,
            m_impl->state.cbvSrvUavStagingHeap.getCPUHandle(m_impl->state.nullSrvCubeIndex)
        );
    }

    // Pre-allocate a contiguous sampler table (16 slots for s0-s15).
    // Fill every slot with a default LINEAR/CLAMP sampler so the GPU never reads
    // uninitialized sampler descriptors.  Transpiled shaders place per-texture
    // samplers at s0-s3 and the shadow sampler at s8 — slots in between must be valid.
    {
        m_impl->state.samplerTableStart = m_impl->state.samplerHeap.allocateRange(
            DirectX12State::SAMPLER_TABLE_SIZE);

        if (m_impl->state.samplerTableStart != UINT32_MAX) {
            D3D12_SAMPLER_DESC defaultSamplerDesc = {};
            defaultSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            defaultSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            defaultSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            defaultSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            defaultSamplerDesc.MipLODBias = 0.0f;
            defaultSamplerDesc.MaxAnisotropy = 1;
            defaultSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
            defaultSamplerDesc.MinLOD = 0.0f;
            defaultSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

            for (uint32_t i = 0; i < DirectX12State::SAMPLER_TABLE_SIZE; ++i) {
                m_impl->state.device->CreateSampler(
                    &defaultSamplerDesc,
                    m_impl->state.samplerHeap.getCPUHandle(
                        m_impl->state.samplerTableStart + i));
            }
        }
    }

    // Create default constant buffers for root params 1, 2, 3
    // These are bound in bindPipeline to ensure all root params have valid addresses
    // after SetGraphicsRootSignature invalidates them all
    D3D12_HEAP_PROPERTIES uploadHeapProps = {};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    // Helper to create and initialize a default CBV buffer
    auto createDefaultCBV = [&](uint32_t size, ComPtr<ID3D12Resource>& outBuffer,
                                D3D12_GPU_VIRTUAL_ADDRESS& outGPUAddress, const char* name) {
        uint32_t alignedSize = (size + 255) & ~255;

        D3D12_RESOURCE_DESC cbDesc = {};
        cbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        cbDesc.Width = alignedSize;
        cbDesc.Height = 1;
        cbDesc.DepthOrArraySize = 1;
        cbDesc.MipLevels = 1;
        cbDesc.SampleDesc.Count = 1;
        cbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        HRESULT result = m_impl->state.device->CreateCommittedResource(
            &uploadHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &cbDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&outBuffer)
        );

        if (FAILED(result)) {
            LOG_ERROR(LogCategory::Render, "[DX12] Failed to create default {} buffer: HRESULT = 0x{:08X}",
                      name, static_cast<unsigned int>(result));
            return false;
        }

        outGPUAddress = outBuffer->GetGPUVirtualAddress();

        // Initialize with zeros
        void* mappedData = nullptr;
        D3D12_RANGE readRange = {0, 0};
        result = outBuffer->Map(0, &readRange, &mappedData);
        if (SUCCEEDED(result) && mappedData) {
            std::memset(mappedData, 0, alignedSize);
            outBuffer->Unmap(0, nullptr);
        }

        return true;
    };

    // CBV1: Camera data (u_View + u_CameraPosition = 64 + 16 = 80 bytes, use 256 for safety)
    createDefaultCBV(256, m_impl->state.defaultCBV1Buffer, m_impl->state.defaultCBV1GPUAddress, "camera");

    // CBV2: Bone data (128 bones * 64 bytes = 8KB)
    createDefaultCBV(128 * 64, m_impl->state.defaultCBV2Buffer, m_impl->state.defaultCBV2GPUAddress, "bone");

    // CBV3: Light data (16 lights * 80 bytes + shadow maps + misc = ~4KB, use 8KB for safety)
    createDefaultCBV(8 * 1024, m_impl->state.defaultCBV3Buffer, m_impl->state.defaultCBV3GPUAddress, "light");

    // Query device capabilities
    DXGI_ADAPTER_DESC1 adapterDesc;
    m_impl->state.dxgiAdapter->GetDesc1(&adapterDesc);

    char deviceName[128];
    WideCharToMultiByte(CP_UTF8, 0, adapterDesc.Description, -1, deviceName, sizeof(deviceName), nullptr, nullptr);

    m_impl->caps.backend = GraphicsBackend::DirectX12;
    m_impl->caps.deviceName = deviceName;
    m_impl->caps.apiVersion = "DirectX 12";

    // Query feature level
    D3D12_FEATURE_DATA_FEATURE_LEVELS featureLevels = {};
    D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0
    };
    featureLevels.NumFeatureLevels = ARRAYSIZE(levels);
    featureLevels.pFeatureLevelsRequested = levels;
    if (SUCCEEDED(m_impl->state.device->CheckFeatureSupport(
            D3D12_FEATURE_FEATURE_LEVELS, &featureLevels, sizeof(featureLevels)))) {
        switch (featureLevels.MaxSupportedFeatureLevel) {
            case D3D_FEATURE_LEVEL_12_1: m_impl->caps.apiVersion = "DirectX 12.1"; break;
            case D3D_FEATURE_LEVEL_12_0: m_impl->caps.apiVersion = "DirectX 12.0"; break;
            case D3D_FEATURE_LEVEL_11_1: m_impl->caps.apiVersion = "DirectX 12 (FL 11.1)"; break;
            default: m_impl->caps.apiVersion = "DirectX 12 (FL 11.0)"; break;
        }
    }

    // Set capabilities
    m_impl->caps.maxTextureSize = D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION;  // 16384
    m_impl->caps.maxTextureLayers = D3D12_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION;  // 2048
    m_impl->caps.maxVertexAttributes = D3D12_VS_INPUT_REGISTER_COUNT;  // 32
    m_impl->caps.maxUniformBufferSize = D3D12_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16;  // 4096 * 16 bytes

    m_impl->caps.supportsCompute = true;
    m_impl->caps.supportsGeometryShader = true;
    m_impl->caps.supportsTessellation = true;
    m_impl->caps.supportsBindless = true;  // DX12 supports bindless via descriptor heaps
    m_impl->caps.supportsRayTracing = false;  // TODO: Check for DXR support

    // Null/unbound bindings are already handled inline above: every SRV-table slot
    // receives a dimension-matched null SRV (nullSrvIndex / nullSrvCubeIndex) and the
    // sampler table is pre-filled with a default sampler, so no dummy fallback
    // resources are required.

    m_impl->initialized = true;

    return true;
}

void GfxDeviceDX12::shutdown() {
    if (!m_impl->initialized) {
        return;
    }

    // Wait for GPU to finish all work
    m_impl->state.waitForGPU();

    // Destroy meshes
    for (auto& [id, mesh] : m_impl->meshes) {
        if (mesh.vertexBuffer.isValid()) {
            destroyBuffer(mesh.vertexBuffer);
        }
        if (mesh.indexBuffer.isValid()) {
            destroyBuffer(mesh.indexBuffer);
        }
    }
    m_impl->meshes.clear();

    // Destroy fonts
    for (auto& [id, font] : m_impl->fonts) {
        if (font.texture.isValid()) {
            destroyTexture(font.texture);
        }
    }
    m_impl->fonts.clear();

    // Clear all resources
    m_impl->state.flushDeferredDestructions();

    m_impl->state.buffers.clear();
    m_impl->state.textures.clear();
    m_impl->state.samplers.clear();
    m_impl->state.shaders.clear();
    m_impl->state.pipelines.clear();
    m_impl->state.renderTargets.clear();
    m_impl->state.swapchains.clear();
    m_impl->state.uniformBuffers.clear();

    m_impl->initialized = false;

}

const GfxDeviceCaps& GfxDeviceDX12::getCapabilities() const {
    return m_impl->caps;
}

GraphicsBackend GfxDeviceDX12::getBackend() const {
    return GraphicsBackend::DirectX12;
}

void GfxDeviceDX12::setDefaultTextureFiltering(FilterMode minFilter, FilterMode magFilter) {
    m_impl->defaultMinFilter = minFilter;
    m_impl->defaultMagFilter = magFilter;
}

SwapchainHandle GfxDeviceDX12::createSwapchain(const SwapchainDesc& desc) {
    if (desc.window.platformHandle == nullptr) {
        LOG_ERROR(LogCategory::Render, "[DX12] Invalid window handle (null)");
        return SwapchainHandle();
    }

    uint32_t actualWidth = desc.width;
    uint32_t actualHeight = desc.height;

    if (actualWidth == 0 || actualHeight == 0) {
        HWND hwnd = (HWND)desc.window.platformHandle;
        RECT clientRect;
        if (GetClientRect(hwnd, &clientRect)) {
            actualWidth = static_cast<uint32_t>(clientRect.right - clientRect.left);
            actualHeight = static_cast<uint32_t>(clientRect.bottom - clientRect.top);
        }
        if (actualWidth == 0) actualWidth = 1;
        if (actualHeight == 0) actualHeight = 1;
    }

    // Use linear format for swapchain
    TextureFormat actualColorFormat = desc.colorFormat;
    if (actualColorFormat == TextureFormat::RGBA8_SRGB) {
        actualColorFormat = TextureFormat::RGBA8_UNORM;
    } else if (actualColorFormat == TextureFormat::BGRA8_SRGB) {
        actualColorFormat = TextureFormat::BGRA8_UNORM;
    }

    DX12Swapchain swapchain;
    swapchain.window = desc.window;
    swapchain.width = actualWidth;
    swapchain.height = actualHeight;
    swapchain.colorFormat = actualColorFormat;
    swapchain.dxgiColorFormat = DX12Utils::toDXGIFormat(actualColorFormat);
    swapchain.vsync = desc.vsync;
    swapchain.bufferCount = std::max(2u, desc.bufferCount);

    // Create swapchain
    DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
    swapchainDesc.Width = actualWidth;
    swapchainDesc.Height = actualHeight;
    swapchainDesc.Format = swapchain.dxgiColorFormat;
    swapchainDesc.Stereo = FALSE;
    swapchainDesc.SampleDesc.Count = 1;
    swapchainDesc.SampleDesc.Quality = 0;
    swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchainDesc.BufferCount = swapchain.bufferCount;
    swapchainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    ComPtr<IDXGISwapChain1> swapchain1;
    HRESULT hr = m_impl->state.dxgiFactory->CreateSwapChainForHwnd(
        m_impl->state.commandQueue.Get(),
        (HWND)desc.window.platformHandle,
        &swapchainDesc,
        nullptr,
        nullptr,
        &swapchain1
    );

    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "[DX12] Failed to create swapchain: HRESULT = 0x{:08X}",
                 static_cast<unsigned int>(hr));
        return SwapchainHandle();
    }

    // Disable Alt+Enter fullscreen toggle
    m_impl->state.dxgiFactory->MakeWindowAssociation((HWND)desc.window.platformHandle, DXGI_MWA_NO_ALT_ENTER);

    hr = swapchain1.As(&swapchain.swapchain);
    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "[DX12] Failed to get IDXGISwapChain4");
        return SwapchainHandle();
    }

    // Get backbuffer resources
    swapchain.backbufferTextures.resize(swapchain.bufferCount);
    swapchain.backbufferRTVIndices.resize(swapchain.bufferCount);
    swapchain.backbufferStates.resize(swapchain.bufferCount, D3D12_RESOURCE_STATE_PRESENT);  // Start in PRESENT state
    swapchain.backbufferHandles.resize(swapchain.bufferCount);
    swapchain.frameFenceValues.resize(swapchain.bufferCount, 0);

    for (uint32_t i = 0; i < swapchain.bufferCount; ++i) {
        hr = swapchain.swapchain->GetBuffer(i, IID_PPV_ARGS(&swapchain.backbufferTextures[i]));
        if (FAILED(hr)) {
            LOG_ERROR(LogCategory::Render, "[DX12] Failed to get backbuffer {}", i);
            return SwapchainHandle();
        }

        // Create RTV
        swapchain.backbufferRTVIndices[i] = m_impl->state.rtvHeap.allocate();
        m_impl->state.device->CreateRenderTargetView(
            swapchain.backbufferTextures[i].Get(),
            nullptr,
            m_impl->state.rtvHeap.getCPUHandle(swapchain.backbufferRTVIndices[i])
        );
    }

    // Create depth buffer
    D3D12_HEAP_PROPERTIES depthHeapProps = {};
    depthHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC depthDesc = {};
    depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthDesc.Width = actualWidth;
    depthDesc.Height = actualHeight;
    depthDesc.DepthOrArraySize = 1;
    depthDesc.MipLevels = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE depthClearValue = {};
    depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthClearValue.DepthStencil.Depth = 1.0f;
    depthClearValue.DepthStencil.Stencil = 0;

    hr = m_impl->state.device->CreateCommittedResource(
        &depthHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &depthDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &depthClearValue,
        IID_PPV_ARGS(&swapchain.depthTexture)
    );

    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "[DX12] Failed to create depth buffer");
        return SwapchainHandle();
    }

    // Create DSV
    swapchain.depthDSVIndex = m_impl->state.dsvHeap.allocate();
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;

    m_impl->state.device->CreateDepthStencilView(
        swapchain.depthTexture.Get(),
        &dsvDesc,
        m_impl->state.dsvHeap.getCPUHandle(swapchain.depthDSVIndex)
    );

    // Create render target handles for each backbuffer
    uint32_t swapchainId = m_impl->state.nextSwapchainID++;

    for (uint32_t i = 0; i < swapchain.bufferCount; ++i) {
        DX12RenderTarget rt;
        rt.width = actualWidth;
        rt.height = actualHeight;
        rt.colorFormat = actualColorFormat;
        rt.depthFormat = TextureFormat::DEPTH24_STENCIL8;
        rt.dxgiColorFormat = swapchain.dxgiColorFormat;
        rt.dxgiDepthFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        rt.rtvIndex = swapchain.backbufferRTVIndices[i];
        rt.dsvIndex = swapchain.depthDSVIndex;
        rt.isSwapchainBackbuffer = true;
        rt.owningSwapchainId = swapchainId;

        uint32_t rtId = m_impl->state.nextRenderTargetID++;
        m_impl->state.renderTargets[rtId] = rt;
        swapchain.backbufferHandles[i] = RenderTargetHandle(rtId);
    }

    swapchain.currentBackbufferIndex = swapchain.swapchain->GetCurrentBackBufferIndex();
    m_impl->state.swapchains[swapchainId] = swapchain;

    return SwapchainHandle(swapchainId);
}

void GfxDeviceDX12::destroySwapchain(SwapchainHandle swapchain) {
    if (!swapchain.isValid()) {
        return;
    }

    auto it = m_impl->state.swapchains.find(swapchain.id);
    if (it == m_impl->state.swapchains.end()) {
        return;
    }

    // Wait for GPU to finish using the swapchain
    m_impl->state.waitForGPU();

    DX12Swapchain& sc = it->second;

    // Free RTVs
    for (uint32_t rtvIndex : sc.backbufferRTVIndices) {
        m_impl->state.rtvHeap.free(rtvIndex);
    }

    // Free DSV
    if (sc.depthDSVIndex != UINT32_MAX) {
        m_impl->state.dsvHeap.free(sc.depthDSVIndex);
    }

    // Remove render target handles
    for (const auto& rtHandle : sc.backbufferHandles) {
        m_impl->state.renderTargets.erase(rtHandle.id);
    }

    m_impl->state.swapchains.erase(it);
}

void GfxDeviceDX12::resizeSwapchain(SwapchainHandle swapchain, uint32_t width, uint32_t height) {
    if (!swapchain.isValid() || width == 0 || height == 0) {
        return;
    }

    auto it = m_impl->state.swapchains.find(swapchain.id);
    if (it == m_impl->state.swapchains.end()) {
        return;
    }

    DX12Swapchain& sc = it->second;

    if (sc.width == width && sc.height == height) {
        return;
    }

    // Wait for GPU to finish
    m_impl->state.waitForGPU();

    // Release backbuffer resources
    for (uint32_t i = 0; i < sc.bufferCount; ++i) {
        sc.backbufferTextures[i].Reset();
    }
    sc.depthTexture.Reset();

    // Resize swapchain
    HRESULT hr = sc.swapchain->ResizeBuffers(
        sc.bufferCount,
        width,
        height,
        sc.dxgiColorFormat,
        DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
    );

    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "[DX12] Failed to resize swapchain: HRESULT = 0x{:08X}",
                 static_cast<unsigned int>(hr));
        sc.isDead = true;
        return;
    }

    sc.width = width;
    sc.height = height;

    // Recreate backbuffer resources
    for (uint32_t i = 0; i < sc.bufferCount; ++i) {
        hr = sc.swapchain->GetBuffer(i, IID_PPV_ARGS(&sc.backbufferTextures[i]));
        if (FAILED(hr)) {
            LOG_ERROR(LogCategory::Render, "[DX12] Failed to get backbuffer after resize");
            sc.isDead = true;
            return;
        }

        // Update RTV
        m_impl->state.device->CreateRenderTargetView(
            sc.backbufferTextures[i].Get(),
            nullptr,
            m_impl->state.rtvHeap.getCPUHandle(sc.backbufferRTVIndices[i])
        );

        // Reset state to PRESENT (new buffers start in PRESENT state)
        sc.backbufferStates[i] = D3D12_RESOURCE_STATE_PRESENT;
    }

    // Recreate depth buffer
    D3D12_HEAP_PROPERTIES depthHeapProps = {};
    depthHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC depthDesc = {};
    depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.DepthOrArraySize = 1;
    depthDesc.MipLevels = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE depthClearValue = {};
    depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthClearValue.DepthStencil.Depth = 1.0f;
    depthClearValue.DepthStencil.Stencil = 0;

    hr = m_impl->state.device->CreateCommittedResource(
        &depthHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &depthDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &depthClearValue,
        IID_PPV_ARGS(&sc.depthTexture)
    );

    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "[DX12] Failed to recreate depth buffer after resize");
        sc.isDead = true;
        return;
    }

    // Update DSV
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;

    m_impl->state.device->CreateDepthStencilView(
        sc.depthTexture.Get(),
        &dsvDesc,
        m_impl->state.dsvHeap.getCPUHandle(sc.depthDSVIndex)
    );

    // Update render target handles
    for (uint32_t i = 0; i < sc.bufferCount; ++i) {
        auto rtIt = m_impl->state.renderTargets.find(sc.backbufferHandles[i].id);
        if (rtIt != m_impl->state.renderTargets.end()) {
            rtIt->second.width = width;
            rtIt->second.height = height;
        }
    }

    sc.currentBackbufferIndex = sc.swapchain->GetCurrentBackBufferIndex();

}

void GfxDeviceDX12::present(SwapchainHandle swapchain) {

    // Flush any D3D12 debug/validation messages produced by this frame's GPU work
    // into the engine log. This is what surfaces the malformed command behind a
    // DEVICE_HUNG / device-removal so the exact draw and shader can be identified.
    m_impl->state.drainDebugMessages();

    if (m_impl->state.deviceLost) return;

    if (!swapchain.isValid()) {
        return;
    }

    auto it = m_impl->state.swapchains.find(swapchain.id);
    if (it == m_impl->state.swapchains.end() || it->second.isDead) {
        LOG_ERROR(LogCategory::Render, "[DX12] present: swapchain not found or dead");
        return;
    }

    DX12Swapchain& sc = it->second;

    // Safety check: ensure backbuffer is in PRESENT state before presenting
    if (sc.backbufferStates[sc.currentBackbufferIndex] != D3D12_RESOURCE_STATE_PRESENT) {
        LOG_ERROR(LogCategory::Render, "[DX12] Backbuffer not in PRESENT state before Present() call! State: {}",
                  static_cast<int>(sc.backbufferStates[sc.currentBackbufferIndex]));
        sc.backbufferStates[sc.currentBackbufferIndex] = D3D12_RESOURCE_STATE_PRESENT;
    }

    UINT syncInterval = sc.vsync ? 1 : 0;
    HRESULT hr = sc.swapchain->Present(syncInterval, 0);

    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        HRESULT reason = m_impl->state.device->GetDeviceRemovedReason();
        LOG_ERROR(LogCategory::Render, "[DX12] Device lost during present. Reason: 0x{:08X}", static_cast<unsigned int>(reason));

        // Try to get DRED data for more details
        ComPtr<ID3D12DeviceRemovedExtendedData1> dred;
        if (SUCCEEDED(m_impl->state.device->QueryInterface(IID_PPV_ARGS(&dred)))) {
            D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 breadcrumbs = {};
            if (SUCCEEDED(dred->GetAutoBreadcrumbsOutput1(&breadcrumbs))) {
                LOG_ERROR(LogCategory::Render, "[DX12 DRED] Auto-breadcrumbs available");
                const D3D12_AUTO_BREADCRUMB_NODE1* node = breadcrumbs.pHeadAutoBreadcrumbNode;
                while (node) {
                    if (node->pCommandListDebugNameW) {
                        LOG_ERROR(LogCategory::Render, "[DX12 DRED] Command list: {}",
                                  reinterpret_cast<const char*>(node->pCommandListDebugNameA ? node->pCommandListDebugNameA : "unnamed"));
                    }
                    if (node->pLastBreadcrumbValue && node->BreadcrumbCount > 0) {
                        LOG_ERROR(LogCategory::Render, "[DX12 DRED] Last completed op: {}/{}",
                                  *node->pLastBreadcrumbValue, node->BreadcrumbCount);
                    }
                    node = node->pNext;
                }
            }
            D3D12_DRED_PAGE_FAULT_OUTPUT1 pageFault = {};
            if (SUCCEEDED(dred->GetPageFaultAllocationOutput1(&pageFault))) {
                if (pageFault.PageFaultVA != 0) {
                    LOG_ERROR(LogCategory::Render, "[DX12 DRED] Page fault at VA: 0x{:016X}", pageFault.PageFaultVA);

                    // Log existing allocations near the faulting VA
                    const D3D12_DRED_ALLOCATION_NODE1* existNode = pageFault.pHeadExistingAllocationNode;
                    int existCount = 0;
                    while (existNode && existCount < 10) {
                        const char* name = existNode->ObjectNameA ? existNode->ObjectNameA : "unnamed";
                        LOG_ERROR(LogCategory::Render, "[DX12 DRED] Existing alloc: type={}, name='{}'",
                                  static_cast<int>(existNode->AllocationType), name);
                        existNode = existNode->pNext;
                        existCount++;
                    }

                    // Log recently freed allocations — the faulting VA likely belongs to one of these
                    const D3D12_DRED_ALLOCATION_NODE1* freedNode = pageFault.pHeadRecentFreedAllocationNode;
                    int freedCount = 0;
                    while (freedNode && freedCount < 10) {
                        const char* name = freedNode->ObjectNameA ? freedNode->ObjectNameA : "unnamed";
                        LOG_ERROR(LogCategory::Render, "[DX12 DRED] Recently freed: type={}, name='{}'",
                                  static_cast<int>(freedNode->AllocationType), name);
                        freedNode = freedNode->pNext;
                        freedCount++;
                    }
                    if (!pageFault.pHeadRecentFreedAllocationNode) {
                        LOG_ERROR(LogCategory::Render, "[DX12 DRED] No recently freed allocations (VA may be from never-allocated memory)");
                    }
                }
            }
        }

        sc.isDead = true;
        return;
    }

    // Update current backbuffer index (this is the next buffer we'll render to)
    sc.currentBackbufferIndex = sc.swapchain->GetCurrentBackBufferIndex();

    // Move to next frame
    m_impl->state.moveToNextFrame();

    // Process deferred destructions
    m_impl->state.processDeferredDestructions();
}

RenderTargetHandle GfxDeviceDX12::getSwapchainBackbuffer(SwapchainHandle swapchain) {

    if (!swapchain.isValid()) {
        LOG_ERROR(LogCategory::Render, "[DX12] getSwapchainBackbuffer: swapchain not valid");
        return RenderTargetHandle();
    }

    auto it = m_impl->state.swapchains.find(swapchain.id);
    if (it == m_impl->state.swapchains.end() || it->second.isDead) {
        LOG_ERROR(LogCategory::Render, "[DX12] getSwapchainBackbuffer: swapchain not found or dead");
        return RenderTargetHandle();
    }

    uint32_t backbufferIndex = it->second.currentBackbufferIndex;
    RenderTargetHandle handle = it->second.backbufferHandles[backbufferIndex];
    
    return handle;
}

void GfxDeviceDX12::makeContextCurrent(SwapchainHandle /*swapchain*/) {
    // No-op for DirectX 12 - DX12 doesn't have the concept of "current context" like OpenGL.
    // The device is always available once created.
    // This method exists for API compatibility with OpenGL-style backends.
}

void GfxDeviceDX12::setSwapchainHintForOffscreen(SwapchainHandle swapchain) {
    // Store hint for synchronization purposes
    if (swapchain.isValid()) {
        auto it = m_impl->state.swapchains.find(swapchain.id);
        if (it != m_impl->state.swapchains.end()) {
            m_impl->state.currentSwapchain = &it->second;
        }
    }
}

TextureHandle GfxDeviceDX12::createTexture(const TextureDesc& desc) {
    DX12Texture dx12Texture;
    dx12Texture.width = desc.width;
    dx12Texture.height = desc.height;
    dx12Texture.depth = desc.depth;
    dx12Texture.mipLevels = desc.mipLevels;
    dx12Texture.arrayLayers = desc.arrayLayers;
    dx12Texture.format = desc.format;
    dx12Texture.type = desc.type;
    dx12Texture.dxgiFormat = DX12Utils::toDXGIFormat(desc.format);

    // Use typeless format for depth textures
    DXGI_FORMAT resourceFormat = DX12Utils::isDepthFormat(desc.format) ?
        DX12Utils::toDXGIFormatTypeless(desc.format) : dx12Texture.dxgiFormat;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Alignment = 0;
    resourceDesc.Width = desc.width;
    resourceDesc.Height = desc.height;
    resourceDesc.DepthOrArraySize = desc.type == TextureType::TextureCube ? UINT16(6) : static_cast<UINT16>(desc.arrayLayers);
    resourceDesc.MipLevels = static_cast<UINT16>(desc.mipLevels);
    resourceDesc.Format = resourceFormat;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    // Set flags based on usage
    if ((desc.usage & TextureUsage::RenderTarget) != TextureUsage::None) {
        if (DX12Utils::isDepthFormat(desc.format)) {
            resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        } else {
            resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        }
    }
    if ((desc.usage & TextureUsage::DepthStencil) != TextureUsage::None) {
        resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    }
    if ((desc.usage & TextureUsage::Storage) != TextureUsage::None) {
        resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    // Determine initial state
    D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
    if ((desc.usage & TextureUsage::DepthStencil) != TextureUsage::None ||
        (DX12Utils::isDepthFormat(desc.format) && (desc.usage & TextureUsage::RenderTarget) != TextureUsage::None)) {
        initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    } else if ((desc.usage & TextureUsage::RenderTarget) != TextureUsage::None) {
        initialState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }

    D3D12_CLEAR_VALUE* pClearValue = nullptr;
    D3D12_CLEAR_VALUE clearValue = {};
    if (DX12Utils::isDepthFormat(desc.format) && resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) {
        clearValue.Format = DX12Utils::toDXGIFormatDSV(desc.format);
        clearValue.DepthStencil.Depth = 1.0f;
        clearValue.DepthStencil.Stencil = 0;
        pClearValue = &clearValue;
    } else if (resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) {
        clearValue.Format = dx12Texture.dxgiFormat;
        clearValue.Color[0] = 0.0f;
        clearValue.Color[1] = 0.0f;
        clearValue.Color[2] = 0.0f;
        clearValue.Color[3] = 1.0f;
        pClearValue = &clearValue;
    }

    HRESULT hr = m_impl->state.device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        initialState,
        pClearValue,
        IID_PPV_ARGS(&dx12Texture.resource)
    );

    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "[DX12] Failed to create texture: HRESULT = 0x{:08X}",
                 static_cast<unsigned int>(hr));
        return TextureHandle();
    }

    dx12Texture.currentState = initialState;

    // Create SRV in staging heap (non-shader-visible, can be used as copy source)
    // We copy from staging to shader-visible heap when building descriptor tables
    if ((desc.usage & TextureUsage::Sampled) != TextureUsage::None) {
        dx12Texture.srvIndex = m_impl->state.cbvSrvUavStagingHeap.allocate();

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DX12Utils::isDepthFormat(desc.format) ?
            DX12Utils::toDXGIFormatSRV(desc.format) : dx12Texture.dxgiFormat;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        if (desc.type == TextureType::TextureCube) {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
            srvDesc.TextureCube.MostDetailedMip = 0;
            srvDesc.TextureCube.MipLevels = desc.mipLevels;
            srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
        } else if (desc.arrayLayers > 1) {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
            srvDesc.Texture2DArray.MostDetailedMip = 0;
            srvDesc.Texture2DArray.MipLevels = desc.mipLevels;
            srvDesc.Texture2DArray.FirstArraySlice = 0;
            srvDesc.Texture2DArray.ArraySize = desc.arrayLayers;
            srvDesc.Texture2DArray.PlaneSlice = 0;
            srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
        } else {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.MipLevels = desc.mipLevels;
            srvDesc.Texture2D.PlaneSlice = 0;
            srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
        }

        // Create SRV in staging heap (copy source)
        m_impl->state.device->CreateShaderResourceView(
            dx12Texture.resource.Get(),
            &srvDesc,
            m_impl->state.cbvSrvUavStagingHeap.getCPUHandle(dx12Texture.srvIndex)
        );
    }

    uint32_t id = m_impl->state.nextTextureID++;
    m_impl->state.textures[id] = dx12Texture;
    TextureHandle handle(id);

    // Upload initial data if provided
    if (desc.initialData) {
        updateTexture(handle, desc.initialData, 0, 0);
    }

    return handle;
}

void GfxDeviceDX12::destroyTexture(TextureHandle texture) {
    if (!texture.isValid()) {
        return;
    }

    auto it = m_impl->state.textures.find(texture.id);
    if (it == m_impl->state.textures.end()) {
        return;
    }

    if (it->second.isSwapchainImage) {
        m_impl->state.textures.erase(it);
        return;
    }

    m_impl->state.deferDestroy(DirectX12State::DeferredDestroy::Type::Texture, texture.id);
}

BufferHandle GfxDeviceDX12::createBuffer(const BufferDesc& desc) {
    if (desc.size == 0) {
        LOG_ERROR(LogCategory::Render, "[DX12] Cannot create buffer with size 0");
        return BufferHandle();
    }

    DX12Buffer dx12Buffer;
    dx12Buffer.size = desc.size;
    dx12Buffer.usage = desc.usage;

    // Determine heap type and initial state
    D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;

    if (desc.dynamic || (desc.usage & BufferUsage::TransferDst) != BufferUsage::None) {
        // Upload buffer - CPU accessible (persistently mapped below so updateBuffer
        // is a memcpy with no staging copy / GPU stall).
        heapType = D3D12_HEAP_TYPE_UPLOAD;
        initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
    } else if ((desc.usage & BufferUsage::TransferSrc) != BufferUsage::None) {
        // Readback buffer
        heapType = D3D12_HEAP_TYPE_READBACK;
        initialState = D3D12_RESOURCE_STATE_COPY_DEST;
    }

    if (!m_impl->createBuffer(desc.size, heapType, initialState, dx12Buffer.resource)) {
        LOG_ERROR(LogCategory::Render, "[DX12] Failed to create buffer");
        return BufferHandle();
    }

    dx12Buffer.gpuAddress = dx12Buffer.resource->GetGPUVirtualAddress();
    dx12Buffer.currentState = initialState;

    // Map upload buffers permanently
    if (heapType == D3D12_HEAP_TYPE_UPLOAD) {
        D3D12_RANGE readRange = {0, 0};
        dx12Buffer.resource->Map(0, &readRange, &dx12Buffer.mappedData);
        dx12Buffer.isMapped = true;
    }

    // Upload initial data
    if (desc.initialData && heapType == D3D12_HEAP_TYPE_UPLOAD && dx12Buffer.mappedData) {
        std::memcpy(dx12Buffer.mappedData, desc.initialData, desc.size);
    }

    uint32_t id = m_impl->state.nextBufferID++;
    m_impl->state.buffers[id] = dx12Buffer;

    return BufferHandle(id);
}

void GfxDeviceDX12::destroyBuffer(BufferHandle buffer) {
    if (!buffer.isValid()) {
        return;
    }

    m_impl->state.deferDestroy(DirectX12State::DeferredDestroy::Type::Buffer, buffer.id);
}

SamplerHandle GfxDeviceDX12::createSampler(const SamplerDesc& desc) {
    DX12Sampler dx12Sampler;
    dx12Sampler.desc = desc;
    // Create the sampler descriptor in the NON-shader-visible staging heap.
    // Per-draw binding copies it from here into the shader-visible sampler table
    // via CopyDescriptorsSimple, which requires the copy source to be readable
    // (i.e. non-shader-visible). Allocating it in the shader-visible heap makes
    // it CPU-write-only and the copy illegal — that corrupts the descriptor heap
    // and hangs the GPU (DXGI_ERROR_DEVICE_HUNG). Mirrors the SRV staging path.
    dx12Sampler.samplerIndex = m_impl->state.samplerStagingHeap.allocate();

    D3D12_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = DX12Utils::toD3D12Filter(desc.minFilter, desc.magFilter, desc.mipFilter, desc.compareEnable);
    samplerDesc.AddressU = DX12Utils::toD3D12AddressMode(desc.wrapU);
    samplerDesc.AddressV = DX12Utils::toD3D12AddressMode(desc.wrapV);
    samplerDesc.AddressW = DX12Utils::toD3D12AddressMode(desc.wrapW);
    samplerDesc.MipLODBias = 0.0f;
    samplerDesc.MaxAnisotropy = static_cast<UINT>(desc.maxAnisotropy);
    samplerDesc.ComparisonFunc = desc.compareEnable ? DX12Utils::toD3D12CompareFunc(desc.compareFunc) : D3D12_COMPARISON_FUNC_NEVER;
    samplerDesc.BorderColor[0] = 0.0f;
    samplerDesc.BorderColor[1] = 0.0f;
    samplerDesc.BorderColor[2] = 0.0f;
    samplerDesc.BorderColor[3] = 0.0f;
    samplerDesc.MinLOD = 0.0f;
    samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

    m_impl->state.device->CreateSampler(&samplerDesc,
                                         m_impl->state.samplerStagingHeap.getCPUHandle(dx12Sampler.samplerIndex));

    uint32_t id = m_impl->state.nextSamplerID++;
    m_impl->state.samplers[id] = dx12Sampler;

    return SamplerHandle(id);
}

void GfxDeviceDX12::destroySampler(SamplerHandle sampler) {
    if (!sampler.isValid()) {
        return;
    }

    m_impl->state.deferDestroy(DirectX12State::DeferredDestroy::Type::Sampler, sampler.id);
}

ShaderHandle GfxDeviceDX12::createShader(const ShaderDesc& desc) {
    if (!desc.bytecode || desc.bytecodeSize == 0) {
        LOG_ERROR(LogCategory::Render, "[DX12] Invalid shader bytecode");
        return ShaderHandle();
    }

    // Check if this is precompiled bytecode (DXBC container format)
    // Both SM 5.0 DXBC (from fxc.exe) and SM 6.0 DXIL (from dxc.exe) use DXBC container
    // The difference is in the internal shader type chunks, not the container magic
    const uint8_t* bytes = static_cast<const uint8_t*>(desc.bytecode);
    // DXBC container header: "DXBC" (0x44 0x58 0x42 0x43)
    bool isPrecompiled = (desc.bytecodeSize >= 4 &&
                          bytes[0] == 0x44 && bytes[1] == 0x58 &&
                          bytes[2] == 0x42 && bytes[3] == 0x43);

    DX12Shader dx12Shader;
    dx12Shader.stage = desc.stage;
    dx12Shader.entryPoint = desc.entryPoint.empty() ? "main" : desc.entryPoint;

    if (!isPrecompiled) {
        // Runtime compilation from HLSL source - requires DXC (dxcompiler.dll)
        // This path is only used for custom shaders or when precompiled bytecode isn't available

        // First, try to initialize DXC if not already done
        if (!DXCCompiler::isAvailable()) {
            DXCCompiler::initialize();  // This will log warnings if DXC isn't available
        }

        if (!DXCCompiler::isAvailable()) {
            LOG_ERROR(LogCategory::Render, "[DX12] Runtime shader compilation requires dxcompiler.dll");
            LOG_ERROR(LogCategory::Render, "[DX12] Either use precompiled shaders or install DirectX Shader Compiler");
            return ShaderHandle();
        }

        std::string hlslSource(static_cast<const char*>(desc.bytecode), desc.bytecodeSize);
        std::string errors;

        // Use DXC for SM 6.0 features like NonUniformResourceIndex
        if (!DXCCompiler::compile(hlslSource, desc.stage, dx12Shader.entryPoint, dx12Shader.bytecode, errors)) {
            LOG_ERROR(LogCategory::Render, "[DX12] Failed to compile HLSL shader: {}", errors);
            return ShaderHandle();
        }

    } else {
        // Store precompiled bytecode directly (DXBC container with DXIL/DXBC chunks)
        dx12Shader.bytecode.assign(bytes, bytes + desc.bytecodeSize);
        
    }

    uint32_t id = m_impl->state.nextShaderID++;
    m_impl->state.shaders[id] = dx12Shader;

    return ShaderHandle(id);
}

void GfxDeviceDX12::destroyShader(ShaderHandle shader) {
    if (!shader.isValid()) {
        return;
    }

    m_impl->state.deferDestroy(DirectX12State::DeferredDestroy::Type::Shader, shader.id);
}

PipelineHandle GfxDeviceDX12::createPipeline(const PipelineDesc& desc) {
    DX12Pipeline dx12Pipeline;
    dx12Pipeline.shaders = desc.shaders;
    dx12Pipeline.vertexLayout = desc.vertexLayout;
    dx12Pipeline.extraVertexBuffers = desc.extraVertexBuffers;
    dx12Pipeline.topology = desc.topology;
    dx12Pipeline.d3dTopology = DX12Utils::toD3D12Topology(desc.topology);
    dx12Pipeline.blendStateDesc = desc.blendState;
    dx12Pipeline.depthStencilStateDesc = desc.depthStencilState;
    dx12Pipeline.rasterizerStateDesc = desc.rasterizerState;

    // Create root signature
    if (!m_impl->createRootSignature(desc, dx12Pipeline.rootSignature)) {
        LOG_ERROR(LogCategory::Render, "[DX12] Failed to create root signature");
        return PipelineHandle();
    }

    // Build input layout from vertex layout
    // Track semantic indices for duplicate semantic names
    std::unordered_map<std::string, uint32_t> semanticIndexMap;
    std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements;

    for (const auto& attr : desc.vertexLayout.attributes) {
        D3D12_INPUT_ELEMENT_DESC elementDesc = {};
        elementDesc.SemanticName = DX12Utils::getSemanticNameFromAttributeName(attr.name);

        // Track and increment semantic index for duplicate semantic names
        std::string semanticKey(elementDesc.SemanticName);
        uint32_t& semanticIndex = semanticIndexMap[semanticKey];
        elementDesc.SemanticIndex = semanticIndex++;

        elementDesc.Format = DX12Utils::toD3D12VertexFormat(attr.format);
        elementDesc.InputSlot = attr.binding;
        elementDesc.AlignedByteOffset = attr.offset;
        elementDesc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        elementDesc.InstanceDataStepRate = 0;
        inputElements.push_back(elementDesc);

    }

    // Append any additional vertex buffer bindings (e.g. per-instance data on
    // input slot 1). The semantic index counter is shared with the primary
    // layout so that, for example, the geometry TEXCOORD0 is followed by the
    // instance attributes' TEXCOORD1..N, matching the instanced shaders.
    for (const auto& extraLayout : desc.extraVertexBuffers) {
        const bool perInstance = (extraLayout.inputRate == VertexInputRate::Instance);
        for (const auto& attr : extraLayout.attributes) {
            D3D12_INPUT_ELEMENT_DESC elementDesc = {};
            // Per-instance attributes are emitted by the transpiler with
            // sequential TEXCOORD semantics, so force the TEXCOORD semantic here
            // rather than deriving it from the attribute name (which would map,
            // e.g., "a_InstanceColor" to COLOR and break the shader match).
            elementDesc.SemanticName = perInstance
                ? "TEXCOORD"
                : DX12Utils::getSemanticNameFromAttributeName(attr.name);

            std::string semanticKey(elementDesc.SemanticName);
            uint32_t& semanticIndex = semanticIndexMap[semanticKey];
            elementDesc.SemanticIndex = semanticIndex++;

            elementDesc.Format = DX12Utils::toD3D12VertexFormat(attr.format);
            elementDesc.InputSlot = attr.binding;
            elementDesc.AlignedByteOffset = attr.offset;
            elementDesc.InputSlotClass = perInstance
                ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA
                : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
            elementDesc.InstanceDataStepRate = perInstance ? 1u : 0u;
            inputElements.push_back(elementDesc);
        }
    }

    // Create graphics pipeline state
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

    // Validate root signature
    if (!dx12Pipeline.rootSignature.rootSignature) {
        LOG_ERROR(LogCategory::Render, "[DX12] Root signature is null");
        return PipelineHandle();
    }
    psoDesc.pRootSignature = dx12Pipeline.rootSignature.rootSignature.Get();

    // Set shader bytecodes
    bool hasVertexShader = false;
    bool hasPixelShader = false;
    for (const auto& shaderHandle : desc.shaders) {
        auto shaderIt = m_impl->state.shaders.find(shaderHandle.id);
        if (shaderIt == m_impl->state.shaders.end()) {
            
            continue;
        }

        const DX12Shader& shader = shaderIt->second;
        if (shader.bytecode.empty()) {
            LOG_ERROR(LogCategory::Render, "[DX12] Shader has empty bytecode");
            continue;
        }

        D3D12_SHADER_BYTECODE bytecode = {shader.bytecode.data(), shader.bytecode.size()};

        switch (shader.stage) {
            case ShaderStage::Vertex:
                psoDesc.VS = bytecode;
                hasVertexShader = true;
                break;
            case ShaderStage::Fragment:
                psoDesc.PS = bytecode;
                hasPixelShader = true;
                break;
            case ShaderStage::Geometry:
                psoDesc.GS = bytecode;
                break;
            default:
                break;
        }
    }

    // Validate required shaders
    if (!hasVertexShader) {
        LOG_ERROR(LogCategory::Render, "[DX12] Pipeline requires a vertex shader");
        return PipelineHandle();
    }
    if (!hasPixelShader) {
        LOG_ERROR(LogCategory::Render, "[DX12] Pipeline requires a pixel shader");
        return PipelineHandle();
    }

    // Input layout
    psoDesc.InputLayout.pInputElementDescs = inputElements.data();
    psoDesc.InputLayout.NumElements = static_cast<UINT>(inputElements.size());

    // Rasterizer state
    psoDesc.RasterizerState.FillMode = DX12Utils::toD3D12FillMode(desc.rasterizerState.fillMode);
    psoDesc.RasterizerState.CullMode = DX12Utils::toD3D12CullMode(desc.rasterizerState.cullMode);
    psoDesc.RasterizerState.FrontCounterClockwise = (desc.rasterizerState.frontFace == WindingOrder::CounterClockwise);
    psoDesc.RasterizerState.DepthBias = static_cast<INT>(desc.rasterizerState.depthBiasConstantFactor);
    psoDesc.RasterizerState.DepthBiasClamp = desc.rasterizerState.depthBiasClamp;
    psoDesc.RasterizerState.SlopeScaledDepthBias = desc.rasterizerState.depthBiasSlopeFactor;
    psoDesc.RasterizerState.DepthClipEnable = !desc.rasterizerState.depthClampEnable;
    psoDesc.RasterizerState.MultisampleEnable = FALSE;
    psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;
    psoDesc.RasterizerState.ForcedSampleCount = 0;
    psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    // Blend state
    psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
    psoDesc.BlendState.IndependentBlendEnable = FALSE;
    psoDesc.BlendState.RenderTarget[0].BlendEnable = desc.blendState.blendEnable;
    psoDesc.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
    psoDesc.BlendState.RenderTarget[0].SrcBlend = DX12Utils::toD3D12Blend(desc.blendState.srcColorBlend);
    psoDesc.BlendState.RenderTarget[0].DestBlend = DX12Utils::toD3D12Blend(desc.blendState.dstColorBlend);
    psoDesc.BlendState.RenderTarget[0].BlendOp = DX12Utils::toD3D12BlendOp(desc.blendState.colorBlendOp);
    psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = DX12Utils::toD3D12Blend(desc.blendState.srcAlphaBlend);
    psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = DX12Utils::toD3D12Blend(desc.blendState.dstAlphaBlend);
    psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = DX12Utils::toD3D12BlendOp(desc.blendState.alphaBlendOp);
    psoDesc.BlendState.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    // Depth/stencil state
    psoDesc.DepthStencilState.DepthEnable = desc.depthStencilState.depthTestEnable;
    psoDesc.DepthStencilState.DepthWriteMask = desc.depthStencilState.depthWriteEnable ?
        D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    psoDesc.DepthStencilState.DepthFunc = DX12Utils::toD3D12CompareFunc(desc.depthStencilState.depthCompareFunc);
    psoDesc.DepthStencilState.StencilEnable = desc.depthStencilState.stencilEnable;
    psoDesc.DepthStencilState.StencilReadMask = 0xFF;
    psoDesc.DepthStencilState.StencilWriteMask = 0xFF;
    psoDesc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    psoDesc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    psoDesc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
    psoDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    psoDesc.DepthStencilState.BackFace = psoDesc.DepthStencilState.FrontFace;

    // Other state
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = DX12Utils::toD3D12TopologyType(desc.topology);
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DX12Utils::toDXGIFormat(desc.colorFormat);
    psoDesc.DSVFormat = DX12Utils::toDXGIFormatDSV(desc.depthFormat);
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleDesc.Quality = 0;
    psoDesc.NodeMask = 0;

    // Log format information for debugging

    HRESULT hr = m_impl->state.device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&dx12Pipeline.pipelineState));
    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "[DX12] Failed to create pipeline state: HRESULT = 0x{:08X}",
                 static_cast<unsigned int>(hr));
        LOG_ERROR(LogCategory::Render, "[DX12] PSO details - VS: {} bytes, PS: {} bytes, InputElements: {}",
                 psoDesc.VS.BytecodeLength, psoDesc.PS.BytecodeLength, psoDesc.InputLayout.NumElements);
        LOG_ERROR(LogCategory::Render, "[DX12] RTVFormat[0]={}, DSVFormat={}, TopologyType={}",
                 static_cast<int>(psoDesc.RTVFormats[0]),
                 static_cast<int>(psoDesc.DSVFormat),
                 static_cast<int>(psoDesc.PrimitiveTopologyType));
        LOG_ERROR(LogCategory::Render, "[DX12] DepthTest={}, DepthWrite={}, BlendEnable={}",
                 psoDesc.DepthStencilState.DepthEnable,
                 psoDesc.DepthStencilState.DepthWriteMask == D3D12_DEPTH_WRITE_MASK_ALL,
                 psoDesc.BlendState.RenderTarget[0].BlendEnable);

        // Check device removed reason for more details
        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
            HRESULT reason = m_impl->state.device->GetDeviceRemovedReason();
            LOG_ERROR(LogCategory::Render, "[DX12] Device removed reason: 0x{:08X}", static_cast<unsigned int>(reason));
        }
        return PipelineHandle();
    }

    // Extract uniform offsets from shaders using reflection
    for (const auto& shaderHandle : desc.shaders) {
        auto shaderIt = m_impl->state.shaders.find(shaderHandle.id);
        if (shaderIt == m_impl->state.shaders.end()) {
            continue;
        }

        const DX12Shader& shader = shaderIt->second;
        if (shader.bytecode.empty()) {
            continue;
        }

        // Perform reflection on the shader bytecode
        ShaderReflectionData reflection;
        if (DXCCompiler::reflect(shader.bytecode, reflection)) {
            // Merge uniform offsets from this shader into the pipeline
            for (const auto& [name, offset] : reflection.uniformOffsets) {
                // Only add if not already present (vertex shader takes precedence)
                if (dx12Pipeline.uniformOffsets.find(name) == dx12Pipeline.uniformOffsets.end()) {
                    dx12Pipeline.uniformOffsets[name] = offset;
                    
                }
            }
        }
    }

    uint32_t id = m_impl->state.nextPipelineID++;
    // Retain the descriptor so a color-format variant can be recreated on demand.
    dx12Pipeline.sourceDesc = desc;
    m_impl->state.pipelines[id] = dx12Pipeline;

    return PipelineHandle(id);
}

void GfxDeviceDX12::destroyPipeline(PipelineHandle pipeline) {
    if (!pipeline.isValid()) {
        return;
    }

    m_impl->state.deferDestroy(DirectX12State::DeferredDestroy::Type::Pipeline, pipeline.id);
}

PipelineHandle GfxDeviceDX12::getColorFormatVariant(PipelineHandle base, TextureFormat colorFormat) {
    if (!base.isValid() || colorFormat == TextureFormat::Unknown) {
        return base;
    }

    auto it = m_impl->state.pipelines.find(base.id);
    if (it == m_impl->state.pipelines.end()) {
        return base;
    }
    // Already the requested format - no variant needed.
    if (it->second.sourceDesc.colorFormat == colorFormat) {
        return base;
    }

    const uint64_t key = (static_cast<uint64_t>(base.id) << 32) |
                         static_cast<uint64_t>(static_cast<uint32_t>(colorFormat));
    auto cached = m_impl->state.pipelineFormatVariants.find(key);
    if (cached != m_impl->state.pipelineFormatVariants.end()) {
        return PipelineHandle(cached->second);
    }

    // Copy the source descriptor BEFORE createPipeline (which inserts into the same map
    // and may invalidate `it`), then recreate with the requested render-target format.
    PipelineDesc variantDesc = it->second.sourceDesc;
    variantDesc.colorFormat = colorFormat;

    PipelineHandle variant = createPipeline(variantDesc);
    if (!variant.isValid()) {
        LOG_ERROR(LogCategory::Render,
                  "[DX12] Failed to create color-format pipeline variant (base id={}, "
                  "colorFormat={}, extraVertexBuffers={}); falling back to base pipeline - "
                  "this will trip the RTV-format validation error if the target differs.",
                  base.id, static_cast<int>(colorFormat),
                  static_cast<int>(variantDesc.extraVertexBuffers.size()));
        return base;
    }
    m_impl->state.pipelineFormatVariants[key] = variant.id;
    return variant;
}

TextureFormat GfxDeviceDX12::getRenderTargetColorFormat(RenderTargetHandle target) {
    if (!target.isValid()) {
        return TextureFormat::Unknown;
    }
    auto it = m_impl->state.renderTargets.find(target.id);
    if (it == m_impl->state.renderTargets.end()) {
        return TextureFormat::Unknown;
    }
    return it->second.colorFormat;
}

RenderTargetHandle GfxDeviceDX12::createRenderTarget(const RenderTargetDesc& desc) {
    DX12RenderTarget dx12RT;
    dx12RT.width = desc.width;
    dx12RT.height = desc.height;
    dx12RT.colorFormat = desc.colorFormat;
    dx12RT.depthFormat = desc.depthFormat;
    dx12RT.hasColor = desc.hasColor;
    dx12RT.hasDepth = desc.hasDepth;
    dx12RT.isCubeMap = desc.isCubeMap;
    dx12RT.dxgiColorFormat = DX12Utils::toDXGIFormat(desc.colorFormat);
    dx12RT.dxgiDepthFormat = DX12Utils::toDXGIFormatDSV(desc.depthFormat);

    // Create color texture
    if (desc.hasColor) {
        TextureDesc colorTexDesc = {};
        colorTexDesc.width = desc.width;
        colorTexDesc.height = desc.height;
        colorTexDesc.format = desc.colorFormat;
        colorTexDesc.mipLevels = 1;
        colorTexDesc.arrayLayers = desc.isCubeMap ? 6 : 1;
        colorTexDesc.type = desc.isCubeMap ? TextureType::TextureCube : TextureType::Texture2D;
        colorTexDesc.usage = TextureUsage::RenderTarget | TextureUsage::Sampled;

        dx12RT.colorTextureHandle = createTexture(colorTexDesc);
        if (!dx12RT.colorTextureHandle.isValid()) {
            LOG_ERROR(LogCategory::Render, "[DX12] Failed to create render target color texture");
            return RenderTargetHandle();
        }

        // Create RTV
        auto& colorTexture = m_impl->state.textures[dx12RT.colorTextureHandle.id];
        dx12RT.rtvIndex = m_impl->state.rtvHeap.allocate();

        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = dx12RT.dxgiColorFormat;
        if (desc.isCubeMap) {
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
            rtvDesc.Texture2DArray.MipSlice = 0;
            rtvDesc.Texture2DArray.FirstArraySlice = 0;
            rtvDesc.Texture2DArray.ArraySize = 6;
            rtvDesc.Texture2DArray.PlaneSlice = 0;
        } else {
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            rtvDesc.Texture2D.MipSlice = 0;
            rtvDesc.Texture2D.PlaneSlice = 0;
        }

        m_impl->state.device->CreateRenderTargetView(
            colorTexture.resource.Get(),
            &rtvDesc,
            m_impl->state.rtvHeap.getCPUHandle(dx12RT.rtvIndex)
        );
    }

    // Create depth texture
    if (desc.hasDepth) {
        TextureDesc depthTexDesc = {};
        depthTexDesc.width = desc.width;
        depthTexDesc.height = desc.height;
        depthTexDesc.format = desc.depthFormat;
        depthTexDesc.mipLevels = 1;
        depthTexDesc.arrayLayers = desc.isCubeMap ? 6 : 1;
        depthTexDesc.type = desc.isCubeMap ? TextureType::TextureCube : TextureType::Texture2D;
        depthTexDesc.usage = TextureUsage::DepthStencil | TextureUsage::Sampled;

        dx12RT.depthTextureHandle = createTexture(depthTexDesc);
        if (!dx12RT.depthTextureHandle.isValid()) {
            LOG_ERROR(LogCategory::Render, "[DX12] Failed to create render target depth texture");
            if (desc.hasColor) {
                destroyTexture(dx12RT.colorTextureHandle);
            }
            return RenderTargetHandle();
        }

        // Create DSV
        auto& depthTexture = m_impl->state.textures[dx12RT.depthTextureHandle.id];
        dx12RT.dsvIndex = m_impl->state.dsvHeap.allocate();

        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = dx12RT.dxgiDepthFormat;
        if (desc.isCubeMap) {
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
            dsvDesc.Texture2DArray.MipSlice = 0;
            dsvDesc.Texture2DArray.FirstArraySlice = 0;
            dsvDesc.Texture2DArray.ArraySize = 6;
        } else {
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            dsvDesc.Texture2D.MipSlice = 0;
        }

        m_impl->state.device->CreateDepthStencilView(
            depthTexture.resource.Get(),
            &dsvDesc,
            m_impl->state.dsvHeap.getCPUHandle(dx12RT.dsvIndex)
        );
    }

    uint32_t id = m_impl->state.nextRenderTargetID++;
    m_impl->state.renderTargets[id] = dx12RT;

    return RenderTargetHandle(id);
}

void GfxDeviceDX12::destroyRenderTarget(RenderTargetHandle target) {
    if (!target.isValid()) {
        return;
    }

    auto it = m_impl->state.renderTargets.find(target.id);
    if (it == m_impl->state.renderTargets.end()) {
        return;
    }

    DX12RenderTarget& rt = it->second;

    if (rt.isSwapchainBackbuffer) {
        m_impl->state.renderTargets.erase(it);
        return;
    }

    // Free descriptor heap slots
    if (rt.rtvIndex != UINT32_MAX) {
        m_impl->state.rtvHeap.free(rt.rtvIndex);
    }
    if (rt.dsvIndex != UINT32_MAX) {
        m_impl->state.dsvHeap.free(rt.dsvIndex);
    }

    // Destroy textures
    if (rt.colorTextureHandle.isValid()) {
        destroyTexture(rt.colorTextureHandle);
    }
    if (rt.depthTextureHandle.isValid()) {
        destroyTexture(rt.depthTextureHandle);
    }

    m_impl->state.renderTargets.erase(it);
}

TextureHandle GfxDeviceDX12::getRenderTargetColorTexture(RenderTargetHandle target) {
    if (!target.isValid()) {
        return TextureHandle();
    }

    auto it = m_impl->state.renderTargets.find(target.id);
    if (it == m_impl->state.renderTargets.end()) {
        return TextureHandle();
    }

    return it->second.colorTextureHandle;
}

TextureHandle GfxDeviceDX12::getRenderTargetDepthTexture(RenderTargetHandle target) {
    if (!target.isValid()) {
        return TextureHandle();
    }

    auto it = m_impl->state.renderTargets.find(target.id);
    if (it == m_impl->state.renderTargets.end()) {
        return TextureHandle();
    }

    return it->second.depthTextureHandle;
}

void GfxDeviceDX12::attachCubeMapFace(RenderTargetHandle target, uint32_t faceIndex) {
    if (!target.isValid() || faceIndex >= 6) {
        return;
    }

    auto it = m_impl->state.renderTargets.find(target.id);
    if (it == m_impl->state.renderTargets.end() || !it->second.isCubeMap) {
        return;
    }

    DX12RenderTarget& rt = it->second;
    rt.currentCubeFace = static_cast<int>(faceIndex);

    // Recreate RTV/DSV for specific face
    if (rt.hasColor && rt.colorTextureHandle.isValid()) {
        auto& colorTexture = m_impl->state.textures[rt.colorTextureHandle.id];

        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = rt.dxgiColorFormat;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
        rtvDesc.Texture2DArray.MipSlice = 0;
        rtvDesc.Texture2DArray.FirstArraySlice = faceIndex;
        rtvDesc.Texture2DArray.ArraySize = 1;
        rtvDesc.Texture2DArray.PlaneSlice = 0;

        m_impl->state.device->CreateRenderTargetView(
            colorTexture.resource.Get(),
            &rtvDesc,
            m_impl->state.rtvHeap.getCPUHandle(rt.rtvIndex)
        );
    }

    if (rt.hasDepth && rt.depthTextureHandle.isValid()) {
        auto& depthTexture = m_impl->state.textures[rt.depthTextureHandle.id];

        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = rt.dxgiDepthFormat;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
        dsvDesc.Texture2DArray.MipSlice = 0;
        dsvDesc.Texture2DArray.FirstArraySlice = faceIndex;
        dsvDesc.Texture2DArray.ArraySize = 1;

        m_impl->state.device->CreateDepthStencilView(
            depthTexture.resource.Get(),
            &dsvDesc,
            m_impl->state.dsvHeap.getCPUHandle(rt.dsvIndex)
        );
    }
}

void GfxDeviceDX12::unbindFramebuffer() {
    // In DX12, there's no concept of unbinding framebuffers like OpenGL
    // The render target is set per command list submission
    m_impl->state.currentRenderTarget = nullptr;
}

UniformBufferHandle GfxDeviceDX12::createUniformBuffer(uint32_t size) {
    // DX12 requires constant buffer sizes to be multiples of 256 bytes
    uint32_t alignedSize = static_cast<uint32_t>(DX12Utils::align(size, 256));

    DX12UniformBuffer dx12UBO;
    dx12UBO.size = alignedSize;

    // Create upload heap buffer (CPU-accessible for dynamic updates)
    if (!m_impl->createBuffer(alignedSize, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, dx12UBO.resource)) {
        LOG_ERROR(LogCategory::Render, "[DX12] Failed to create uniform buffer");
        return UniformBufferHandle();
    }

    dx12UBO.gpuAddress = dx12UBO.resource->GetGPUVirtualAddress();

    // Map the buffer
    D3D12_RANGE readRange = {0, 0};
    dx12UBO.resource->Map(0, &readRange, &dx12UBO.mappedData);

    // Create CBV
    dx12UBO.cbvIndex = m_impl->state.cbvSrvUavHeap.allocate();
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = dx12UBO.gpuAddress;
    cbvDesc.SizeInBytes = alignedSize;

    m_impl->state.device->CreateConstantBufferView(&cbvDesc,
                                                    m_impl->state.cbvSrvUavHeap.getCPUHandle(dx12UBO.cbvIndex));

    uint32_t id = m_impl->state.nextUniformBufferID++;
    m_impl->state.uniformBuffers[id] = dx12UBO;

    return UniformBufferHandle(id);
}

void GfxDeviceDX12::destroyUniformBuffer(UniformBufferHandle buffer) {
    if (!buffer.isValid()) {
        return;
    }

    m_impl->state.deferDestroy(DirectX12State::DeferredDestroy::Type::UniformBuffer, buffer.id);
}

void GfxDeviceDX12::updateBuffer(BufferHandle buffer, const void* data, uint64_t size, uint64_t offset) {
    if (!buffer.isValid() || !data || size == 0) {
        return;
    }

    auto it = m_impl->state.buffers.find(buffer.id);
    if (it == m_impl->state.buffers.end()) {
        return;
    }

    DX12Buffer& dx12Buffer = it->second;

    if (dx12Buffer.isMapped && dx12Buffer.mappedData) {
        std::memcpy(static_cast<uint8_t*>(dx12Buffer.mappedData) + offset, data, size);
    } else {
        // Use upload heap and copy command for non-mapped buffers
        // Create a temporary upload buffer
        ComPtr<ID3D12Resource> uploadBuffer;
        D3D12_HEAP_PROPERTIES uploadHeapProps = {};
        uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC uploadBufferDesc = {};
        uploadBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        uploadBufferDesc.Width = size;
        uploadBufferDesc.Height = 1;
        uploadBufferDesc.DepthOrArraySize = 1;
        uploadBufferDesc.MipLevels = 1;
        uploadBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
        uploadBufferDesc.SampleDesc.Count = 1;
        uploadBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        HRESULT hr = m_impl->state.device->CreateCommittedResource(
            &uploadHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &uploadBufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&uploadBuffer)
        );

        if (FAILED(hr)) {
            LOG_ERROR(LogCategory::Render, "[DX12] Failed to create upload buffer for buffer update");
            return;
        }

        // Map and copy data to upload buffer
        void* mappedData = nullptr;
        D3D12_RANGE readRange = {0, 0};
        hr = uploadBuffer->Map(0, &readRange, &mappedData);
        if (FAILED(hr)) {
            LOG_ERROR(LogCategory::Render, "[DX12] Failed to map upload buffer for buffer update");
            return;
        }

        std::memcpy(mappedData, data, size);
        uploadBuffer->Unmap(0, nullptr);

        // Use a dedicated command allocator for buffer uploads to avoid conflicts
        // with the main render command list
        ComPtr<ID3D12CommandAllocator> uploadAllocator;
        hr = m_impl->state.device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&uploadAllocator)
        );
        if (FAILED(hr)) {
            LOG_ERROR(LogCategory::Render, "[DX12] Failed to create upload command allocator for buffer");
            return;
        }

        ComPtr<ID3D12GraphicsCommandList> uploadCmdList;
        hr = m_impl->state.device->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            uploadAllocator.Get(),
            nullptr,
            IID_PPV_ARGS(&uploadCmdList)
        );
        if (FAILED(hr)) {
            LOG_ERROR(LogCategory::Render, "[DX12] Failed to create upload command list for buffer");
            return;
        }

        // Transition buffer to copy destination state if needed
        if (dx12Buffer.currentState != D3D12_RESOURCE_STATE_COPY_DEST) {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = dx12Buffer.resource.Get();
            barrier.Transition.StateBefore = dx12Buffer.currentState;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            uploadCmdList->ResourceBarrier(1, &barrier);
        }

        // Copy from upload buffer to target buffer
        uploadCmdList->CopyBufferRegion(dx12Buffer.resource.Get(), offset, uploadBuffer.Get(), 0, size);

        // Transition back to original state
        if (dx12Buffer.currentState != D3D12_RESOURCE_STATE_COPY_DEST) {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = dx12Buffer.resource.Get();
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barrier.Transition.StateAfter = dx12Buffer.currentState;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            uploadCmdList->ResourceBarrier(1, &barrier);
        }

        // Close and execute
        hr = uploadCmdList->Close();
        if (FAILED(hr)) {
            LOG_ERROR(LogCategory::Render, "[DX12] Failed to close upload command list for buffer");
            return;
        }

        ID3D12CommandList* cmdLists[] = { uploadCmdList.Get() };
        m_impl->state.commandQueue->ExecuteCommandLists(1, cmdLists);

        // Wait for upload to complete
        m_impl->state.waitForGPU();
    }
}

void GfxDeviceDX12::updateTexture(TextureHandle texture, const void* data, uint32_t mipLevel, uint32_t arrayLayer) {
    if (!texture.isValid() || !data) {
        return;
    }

    auto it = m_impl->state.textures.find(texture.id);
    if (it == m_impl->state.textures.end()) {
        return;
    }

    DX12Texture& dx12Texture = it->second;

    // Calculate subresource index (MipSlice + ArraySlice * MipLevels + PlaneSlice * MipLevels * ArraySize)
    UINT subresource = mipLevel + (arrayLayer * dx12Texture.mipLevels);

    // Get texture layout for upload
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
    UINT numRows = 0;
    UINT64 rowSizeInBytes = 0;
    UINT64 totalBytes = 0;

    D3D12_RESOURCE_DESC texDesc = dx12Texture.resource->GetDesc();
    m_impl->state.device->GetCopyableFootprints(&texDesc, subresource, 1, 0,
                                                  &footprint, &numRows, &rowSizeInBytes, &totalBytes);

    // Validate footprint values
    if (totalBytes == 0 || numRows == 0 || rowSizeInBytes == 0) {
        return;
    }

    // Create a temporary upload buffer for this texture data
    ComPtr<ID3D12Resource> uploadBuffer;
    D3D12_HEAP_PROPERTIES uploadHeapProps = {};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC uploadBufferDesc = {};
    uploadBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadBufferDesc.Width = totalBytes;
    uploadBufferDesc.Height = 1;
    uploadBufferDesc.DepthOrArraySize = 1;
    uploadBufferDesc.MipLevels = 1;
    uploadBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    uploadBufferDesc.SampleDesc.Count = 1;
    uploadBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    HRESULT hr = m_impl->state.device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &uploadBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&uploadBuffer)
    );

    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "[DX12] Failed to create upload buffer for texture update");
        OutputDebugStringA("[DX12] CRASH DEBUG: Failed to create upload buffer\n");
        return;
    }

    OutputDebugStringA("[DX12] CRASH DEBUG: Mapping upload buffer\n");

    // Map and copy data to upload buffer
    void* mappedData = nullptr;
    D3D12_RANGE readRange = {0, 0};
    hr = uploadBuffer->Map(0, &readRange, &mappedData);
    if (FAILED(hr) || !mappedData) {
        LOG_ERROR(LogCategory::Render, "[DX12] Failed to map upload buffer for texture update");
        OutputDebugStringA("[DX12] CRASH DEBUG: Failed to map upload buffer\n");
        return;
    }

    OutputDebugStringA("[DX12] CRASH DEBUG: Copying texture data\n");

    // Copy row by row to handle potential row pitch differences
    const uint8_t* srcData = static_cast<const uint8_t*>(data);
    uint8_t* dstData = static_cast<uint8_t*>(mappedData);
    uint32_t srcRowPitch = static_cast<uint32_t>(rowSizeInBytes);

    for (UINT row = 0; row < numRows; ++row) {
        std::memcpy(dstData + row * footprint.Footprint.RowPitch,
                   srcData + row * srcRowPitch,
                   static_cast<size_t>(rowSizeInBytes));
    }

    uploadBuffer->Unmap(0, nullptr);

    // Use a dedicated command allocator for texture uploads to avoid conflicts
    // with the main render command list
    ComPtr<ID3D12CommandAllocator> uploadAllocator;
    hr = m_impl->state.device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&uploadAllocator)
    );
    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "[DX12] Failed to create upload command allocator");
        return;
    }

    ComPtr<ID3D12GraphicsCommandList> uploadCmdList;
    hr = m_impl->state.device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        uploadAllocator.Get(),
        nullptr,
        IID_PPV_ARGS(&uploadCmdList)
    );
    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "[DX12] Failed to create upload command list");
        return;
    }

    // Transition texture to copy destination state
    D3D12_RESOURCE_STATES previousState = dx12Texture.currentState;
    if (previousState != D3D12_RESOURCE_STATE_COPY_DEST) {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = dx12Texture.resource.Get();
        barrier.Transition.StateBefore = previousState;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        uploadCmdList->ResourceBarrier(1, &barrier);
    }

    // Copy from upload buffer to texture
    D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
    dstLocation.pResource = dx12Texture.resource.Get();
    dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLocation.SubresourceIndex = subresource;

    D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
    srcLocation.pResource = uploadBuffer.Get();
    srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    srcLocation.PlacedFootprint = footprint;

    uploadCmdList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);

    // Transition texture back to shader resource state
    {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = dx12Texture.resource.Get();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        uploadCmdList->ResourceBarrier(1, &barrier);
    }
    dx12Texture.currentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    // Close and execute the upload command list
    hr = uploadCmdList->Close();
    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "[DX12] Failed to close upload command list");
        return;
    }

    ID3D12CommandList* cmdLists[] = { uploadCmdList.Get() };
    m_impl->state.commandQueue->ExecuteCommandLists(1, cmdLists);

    // Wait for upload to complete
    m_impl->state.waitForGPU();
}

void GfxDeviceDX12::updateUniformBuffer(UniformBufferHandle buffer, const void* data, uint32_t size, uint32_t offset) {
    if (!buffer.isValid() || !data || size == 0) {
        return;
    }

    auto it = m_impl->state.uniformBuffers.find(buffer.id);
    if (it == m_impl->state.uniformBuffers.end()) {
        return;
    }

    DX12UniformBuffer& dx12UBO = it->second;

    if (dx12UBO.mappedData) {
        std::memcpy(static_cast<uint8_t*>(dx12UBO.mappedData) + offset, data, size);
    }
}

std::unique_ptr<IGfxCommandList> GfxDeviceDX12::beginFrame(RenderTargetHandle target) {

    m_impl->state.beginCommandList();

    ID3D12DescriptorHeap* heaps[] = {
        m_impl->state.cbvSrvUavHeap.getHeap(),
        m_impl->state.samplerHeap.getHeap()
    };
    m_impl->state.commandList->SetDescriptorHeaps(2, heaps);

    auto commandList = std::make_unique<GfxCommandListDX12>(this, &m_impl->state);

    if (target.isValid()) {
        commandList->setRenderTarget(target);
    }

    return commandList;
}

void GfxDeviceDX12::submit(std::unique_ptr<IGfxCommandList> commandList) {

    GfxCommandListDX12* dx12CmdList = static_cast<GfxCommandListDX12*>(commandList.get());
    if (dx12CmdList) {
        dx12CmdList->closeOpenDebugMarkers();
        dx12CmdList->endRendering();
    }

    m_impl->state.executeCommandList();
    commandList.reset();
}

void GfxDeviceDX12::waitIdle() {
    m_impl->state.waitForGPU();
}

bool GfxDeviceDX12::isContextValid() const {
    return !m_impl->state.deviceLost;
}

MeshHandle GfxDeviceDX12::createMesh(const MeshData& meshData) {
    GPUMesh gpuMesh;
    gpuMesh.vertexCount = static_cast<uint32_t>(meshData.vertices.size());
    gpuMesh.indexCount = static_cast<uint32_t>(meshData.indices.size());
    gpuMesh.submeshes = meshData.submeshes;
    gpuMesh.bounds = meshData.bounds;

    // Create vertex buffer
    if (!meshData.vertices.empty()) {
        BufferDesc vertexBufferDesc;
        vertexBufferDesc.size = meshData.vertices.size() * sizeof(Vertex);
        vertexBufferDesc.usage = BufferUsage::Vertex | BufferUsage::TransferDst;
        vertexBufferDesc.initialData = meshData.vertices.data();

        gpuMesh.vertexBuffer = createBuffer(vertexBufferDesc);
        if (!gpuMesh.vertexBuffer.isValid()) {
            LOG_ERROR(LogCategory::Render, "[DX12] Failed to create mesh vertex buffer");
            return MeshHandle();
        }
    }

    // Create index buffer
    if (!meshData.indices.empty()) {
        BufferDesc indexBufferDesc;
        indexBufferDesc.size = meshData.indices.size() * sizeof(uint32_t);
        indexBufferDesc.usage = BufferUsage::Index | BufferUsage::TransferDst;
        indexBufferDesc.initialData = meshData.indices.data();

        gpuMesh.indexBuffer = createBuffer(indexBufferDesc);
        if (!gpuMesh.indexBuffer.isValid()) {
            LOG_ERROR(LogCategory::Render, "[DX12] Failed to create mesh index buffer");
            if (gpuMesh.vertexBuffer.isValid()) {
                destroyBuffer(gpuMesh.vertexBuffer);
            }
            return MeshHandle();
        }
    }

    uint32_t id = m_impl->nextMeshID++;
    m_impl->meshes[id] = gpuMesh;

    return MeshHandle(id);
}

const GPUMesh* GfxDeviceDX12::getMesh(MeshHandle handle) const {
    if (!handle.isValid()) {
        return nullptr;
    }

    auto it = m_impl->meshes.find(handle.id);
    if (it == m_impl->meshes.end()) {
        return nullptr;
    }

    return &it->second;
}

void GfxDeviceDX12::destroyMesh(MeshHandle handle) {
    if (!handle.isValid()) {
        return;
    }

    auto it = m_impl->meshes.find(handle.id);
    if (it == m_impl->meshes.end()) {
        return;
    }

    if (it->second.vertexBuffer.isValid()) {
        destroyBuffer(it->second.vertexBuffer);
    }
    if (it->second.indexBuffer.isValid()) {
        destroyBuffer(it->second.indexBuffer);
    }

    m_impl->meshes.erase(it);
}

FontAtlas GfxDeviceDX12::buildBakedAtlas(const BakedFontAtlas& baked) {
    FontAtlas atlas;
    if (!baked.success) {
        return atlas;
    }

    TextureDesc texDesc = {};
    texDesc.width = baked.atlasWidth;
    texDesc.height = baked.atlasHeight;
    texDesc.format = TextureFormat::R8_UNORM;
    texDesc.usage = TextureUsage::Sampled;
    texDesc.mipLevels = 1;
    texDesc.initialData = baked.bitmap.data();

    TextureHandle atlasTexture = createTexture(texDesc);
    if (!atlasTexture.isValid()) {
        LOG_ERROR(LogCategory::Render, "[DX12] Failed to create font atlas texture");
        return atlas;
    }

    atlas.texture = atlasTexture;
    atlas.atlasWidth = baked.atlasWidth;
    atlas.atlasHeight = baked.atlasHeight;
    atlas.fontSize = baked.fontSize;
    atlas.lineHeight = baked.lineHeight;
    atlas.ascent = baked.ascent;
    atlas.descent = baked.descent;
    atlas.glyphs = baked.glyphs;
    atlas.kerning = baked.kerning;
    return atlas;
}

FontHandle GfxDeviceDX12::createFontAtlas(const FontDesc& desc) {
    BakedFontAtlas baked = BakeFontAtlas(desc, GetFontOversample());
    if (!baked.success) {
        LOG_ERROR(LogCategory::Render, "[DX12] Failed to bake font: {}", desc.fontPath);
        return FontHandle();
    }

    FontAtlas atlas = buildBakedAtlas(baked);
    if (!atlas.texture.isValid()) {
        return FontHandle();
    }

    uint32_t id = m_impl->nextFontID++;
    m_impl->fonts[id] = std::move(atlas);
    m_impl->fontDescs[id] = desc;

    return FontHandle(id);
}

void GfxDeviceDX12::refreshFontAtlases() {
    const float oversample = GetFontOversample();
    for (const auto& [fontID, desc] : m_impl->fontDescs) {
        BakedFontAtlas baked = BakeFontAtlas(desc, oversample);
        FontAtlas atlas = buildBakedAtlas(baked);
        if (!atlas.texture.isValid()) {
            continue;
        }

        auto it = m_impl->fonts.find(fontID);
        if (it != m_impl->fonts.end() && it->second.texture.isValid()) {
            destroyTexture(it->second.texture);
        }
        m_impl->fonts[fontID] = std::move(atlas);
    }
}

const FontAtlas* GfxDeviceDX12::getFontAtlas(FontHandle handle) const {
    if (!handle.isValid()) {
        return nullptr;
    }

    auto it = m_impl->fonts.find(handle.id);
    if (it == m_impl->fonts.end()) {
        return nullptr;
    }

    return &it->second;
}

void GfxDeviceDX12::destroyFontAtlas(FontHandle handle) {
    if (!handle.isValid()) {
        return;
    }

    auto it = m_impl->fonts.find(handle.id);
    if (it == m_impl->fonts.end()) {
        return;
    }

    if (it->second.texture.isValid()) {
        destroyTexture(it->second.texture);
    }

    m_impl->fonts.erase(it);
    m_impl->fontDescs.erase(handle.id);
}

} // namespace lupine

#endif // LUPINE_HAS_DIRECTX12
