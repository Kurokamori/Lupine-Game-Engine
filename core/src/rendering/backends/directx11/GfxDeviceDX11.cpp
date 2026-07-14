#include "lupine/rendering/backends/GfxDeviceDX11.hpp"

#ifdef LUPINE_HAS_DIRECTX11

#include "lupine/rendering/backends/directx11/DirectX11State.hpp"
#include "lupine/rendering/backends/directx11/GfxCommandListDX11.hpp"
#include "lupine/rendering/backends/directx11/HLSLCompiler.hpp"
#include "lupine/rendering/Mesh.hpp"
#include "lupine/rendering/Font.hpp"
#include "lupine/rendering/FontBaker.hpp"
#include "lupine/rendering/Light.hpp"  // For LightUniformBuffer debug logging
#include "lupine/logger/Logger.hpp"
#include "lupine/platform/FileSystem.hpp"
#include "lupine/platform/PackFile.hpp"
#include "lupine/asset/Asset.hpp"
#include <stb_truetype.h>
#include <vector>
#include <unordered_map>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#endif

namespace lupine {

struct GfxDeviceDX11::Impl {
    DirectX11State state;
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
};

GfxDeviceDX11::GfxDeviceDX11()
    : m_impl(std::make_unique<Impl>()) {
}

GfxDeviceDX11::~GfxDeviceDX11() {
    if (m_impl->initialized) {
        shutdown();
    }
}

bool GfxDeviceDX11::initialize() {
    if (m_impl->initialized) {
        return true;
    }

    HRESULT hr;

    // Create DXGI factory
    hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)m_impl->state.dxgiFactory.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "Failed to create DXGI factory");
        return false;
    }

    // Enumerate adapters and select the first one
    hr = m_impl->state.dxgiFactory->EnumAdapters(0, m_impl->state.dxgiAdapter.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "Failed to enumerate DXGI adapters");
        return false;
    }

    // Create D3D11 device and context
    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };

    hr = D3D11CreateDevice(
        m_impl->state.dxgiAdapter.Get(),
        D3D_DRIVER_TYPE_UNKNOWN,
        nullptr,
        createDeviceFlags,
        featureLevels,
        ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION,
        m_impl->state.device.GetAddressOf(),
        &m_impl->state.featureLevel,
        m_impl->state.context.GetAddressOf()
    );

    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "Failed to create D3D11 device");
        return false;
    }

#ifdef _DEBUG
    // Get debug interface
    hr = m_impl->state.device->QueryInterface(__uuidof(ID3D11Debug), (void**)m_impl->state.debugInterface.GetAddressOf());
    if (FAILED(hr)) {
        
    }
#endif

    // Query device capabilities
    DXGI_ADAPTER_DESC adapterDesc;
    m_impl->state.dxgiAdapter->GetDesc(&adapterDesc);

    // Convert wide string to narrow string
    char deviceName[128];
    WideCharToMultiByte(CP_UTF8, 0, adapterDesc.Description, -1, deviceName, sizeof(deviceName), nullptr, nullptr);

    m_impl->caps.backend = GraphicsBackend::DirectX11;
    m_impl->caps.deviceName = deviceName;

    // Feature level to string
    switch (m_impl->state.featureLevel) {
        case D3D_FEATURE_LEVEL_11_1: m_impl->caps.apiVersion = "DirectX 11.1"; break;
        case D3D_FEATURE_LEVEL_11_0: m_impl->caps.apiVersion = "DirectX 11.0"; break;
        case D3D_FEATURE_LEVEL_10_1: m_impl->caps.apiVersion = "DirectX 10.1"; break;
        case D3D_FEATURE_LEVEL_10_0: m_impl->caps.apiVersion = "DirectX 10.0"; break;
        default: m_impl->caps.apiVersion = "Unknown"; break;
    }

    // Set capabilities
    m_impl->caps.maxTextureSize = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;  // 16384
    m_impl->caps.maxTextureLayers = D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION;  // 2048
    m_impl->caps.maxVertexAttributes = D3D11_STANDARD_VERTEX_ELEMENT_COUNT;  // 32
    m_impl->caps.maxUniformBufferSize = D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16;  // 4096 * 16 bytes

    m_impl->caps.supportsCompute = (m_impl->state.featureLevel >= D3D_FEATURE_LEVEL_11_0);
    m_impl->caps.supportsGeometryShader = (m_impl->state.featureLevel >= D3D_FEATURE_LEVEL_10_0);
    m_impl->caps.supportsTessellation = (m_impl->state.featureLevel >= D3D_FEATURE_LEVEL_11_0);
    m_impl->caps.supportsBindless = false;  // DX11 doesn't support bindless textures
    m_impl->caps.supportsRayTracing = false;  // DX11 doesn't support ray tracing

    // Create dummy resources
    m_impl->state.createDummyResources();

    m_impl->initialized = true;

    return true;
}

void GfxDeviceDX11::shutdown() {
    if (!m_impl->initialized) {
        return;
    }

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

    // Destroy resources
    m_impl->state.buffers.clear();
    m_impl->state.textures.clear();
    m_impl->state.samplers.clear();
    m_impl->state.shaders.clear();
    m_impl->state.pipelines.clear();
    m_impl->state.renderTargets.clear();
    m_impl->state.swapchains.clear();
    m_impl->state.uniformBuffers.clear();

    m_impl->state.flushDeferredDestructions();
    m_impl->state.destroyDummyResources();

#ifdef _DEBUG
    if (m_impl->state.debugInterface) {
        m_impl->state.debugInterface->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
    }
#endif

    m_impl->state.context.Reset();
    m_impl->state.device.Reset();
    m_impl->state.dxgiAdapter.Reset();
    m_impl->state.dxgiFactory.Reset();

    m_impl->initialized = false;

}

const GfxDeviceCaps& GfxDeviceDX11::getCapabilities() const {
    return m_impl->caps;
}

GraphicsBackend GfxDeviceDX11::getBackend() const {
    return GraphicsBackend::DirectX11;
}

void GfxDeviceDX11::setDefaultTextureFiltering(FilterMode minFilter, FilterMode magFilter) {
    m_impl->defaultMinFilter = minFilter;
    m_impl->defaultMagFilter = magFilter;
}

SwapchainHandle GfxDeviceDX11::createSwapchain(const SwapchainDesc& desc) {
    // Validate window handle
    if (desc.window.platformHandle == nullptr) {
        LOG_ERROR(LogCategory::Render, "Invalid window handle (null)");
        return SwapchainHandle();
    }

    // Validate dimensions - if 0, query the window for its actual size
    uint32_t actualWidth = desc.width;
    uint32_t actualHeight = desc.height;

    if (actualWidth == 0 || actualHeight == 0) {
        HWND hwnd = (HWND)desc.window.platformHandle;
        RECT clientRect;
        if (GetClientRect(hwnd, &clientRect)) {
            actualWidth = static_cast<uint32_t>(clientRect.right - clientRect.left);
            actualHeight = static_cast<uint32_t>(clientRect.bottom - clientRect.top);
            
        }

        // If still zero (window not yet realized), use a minimum size
        if (actualWidth == 0) actualWidth = 1;
        if (actualHeight == 0) actualHeight = 1;
    }

    // Use linear (UNORM) format for swapchain to match OpenGL/Vulkan color space
    // This prevents double gamma correction (shader + hardware)
    // The shaders output linear colors, and the display handles gamma correction
    TextureFormat actualColorFormat = desc.colorFormat;
    if (actualColorFormat == TextureFormat::RGBA8_SRGB) {
        actualColorFormat = TextureFormat::RGBA8_UNORM;
    } else if (actualColorFormat == TextureFormat::BGRA8_SRGB) {
        actualColorFormat = TextureFormat::BGRA8_UNORM;
    }

    DXGI_SWAP_CHAIN_DESC swapchainDesc = {};
    swapchainDesc.BufferDesc.Width = actualWidth;
    swapchainDesc.BufferDesc.Height = actualHeight;
    swapchainDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapchainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapchainDesc.BufferDesc.Format = DX11Utils::toDXGIFormat(actualColorFormat);
    swapchainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    swapchainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    swapchainDesc.SampleDesc.Count = 1;
    swapchainDesc.SampleDesc.Quality = 0;
    swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchainDesc.BufferCount = 1;  // Single backbuffer for DISCARD mode
    swapchainDesc.OutputWindow = (HWND)desc.window.platformHandle;
    swapchainDesc.Windowed = TRUE;
    swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;  // Compatible with all DX11 systems
    swapchainDesc.Flags = 0;

    DX11Swapchain swapchain;
    HRESULT hr = m_impl->state.dxgiFactory->CreateSwapChain(
        m_impl->state.device.Get(),
        &swapchainDesc,
        swapchain.swapchain.GetAddressOf()
    );

    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "Failed to create swapchain: HRESULT = 0x{:08X}", static_cast<unsigned int>(hr));
        return SwapchainHandle();
    }

    swapchain.window = desc.window;
    swapchain.width = actualWidth;
    swapchain.height = actualHeight;
    swapchain.colorFormat = actualColorFormat;
    swapchain.dxgiColorFormat = DX11Utils::toDXGIFormat(actualColorFormat);
    swapchain.vsync = desc.vsync;

    // Get backbuffer texture
    hr = swapchain.swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)swapchain.backbufferTexture.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "Failed to get swapchain backbuffer");
        return SwapchainHandle();
    }

    // Create render target view
    hr = m_impl->state.device->CreateRenderTargetView(swapchain.backbufferTexture.Get(), nullptr, swapchain.backbufferRTV.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "Failed to create backbuffer RTV");
        return SwapchainHandle();
    }

    // Create depth buffer
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = actualWidth;
    depthDesc.Height = actualHeight;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    hr = m_impl->state.device->CreateTexture2D(&depthDesc, nullptr, swapchain.depthTexture.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "Failed to create depth buffer");
        return SwapchainHandle();
    }

    // Create depth stencil view
    hr = m_impl->state.device->CreateDepthStencilView(swapchain.depthTexture.Get(), nullptr, swapchain.depthDSV.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "Failed to create depth stencil view");
        return SwapchainHandle();
    }

    // Create render target handle for the backbuffer
    DX11RenderTarget rt;
    rt.rtv = swapchain.backbufferRTV;
    rt.dsv = swapchain.depthDSV;
    rt.width = actualWidth;
    rt.height = actualHeight;
    rt.colorFormat = desc.colorFormat;
    rt.depthFormat = TextureFormat::DEPTH24_STENCIL8;
    rt.dxgiColorFormat = swapchain.dxgiColorFormat;
    rt.dxgiDepthFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    rt.isSwapchainBackbuffer = true;

    uint32_t rtID = m_impl->state.nextRenderTargetID++;
    m_impl->state.renderTargets[rtID] = rt;
    swapchain.backbuffer = RenderTargetHandle(rtID);

    uint32_t id = m_impl->state.nextSwapchainID++;
    swapchain.backbuffer = RenderTargetHandle(rtID);
    m_impl->state.renderTargets[rtID].owningSwapchainId = id;
    m_impl->state.swapchains[id] = swapchain;

    return SwapchainHandle(id);
}

void GfxDeviceDX11::destroySwapchain(SwapchainHandle swapchain) {
    if (!swapchain.isValid()) {
        return;
    }

    auto it = m_impl->state.swapchains.find(swapchain.id);
    if (it == m_impl->state.swapchains.end()) {
        return;
    }

    // Destroy associated render target
    if (it->second.backbuffer.isValid()) {
        m_impl->state.renderTargets.erase(it->second.backbuffer.id);
    }

    m_impl->state.swapchains.erase(it);
}

void GfxDeviceDX11::resizeSwapchain(SwapchainHandle swapchain, uint32_t width, uint32_t height) {
    if (!swapchain.isValid()) {
        return;
    }

    auto it = m_impl->state.swapchains.find(swapchain.id);
    if (it == m_impl->state.swapchains.end()) {
        return;
    }

    DX11Swapchain& sc = it->second;

    // Prevent resize to zero dimensions (e.g., minimized window)
    if (width == 0 || height == 0) {
        
        return;
    }

    // Skip resize if dimensions haven't changed
    if (sc.width == width && sc.height == height) {
        
        return;
    }

    // ====================================================================================
    // CRITICAL: Clear ALL device state before resizing
    // ====================================================================================
    // DirectX 11 requires that ALL references to the backbuffer be released before
    // ResizeBuffers can be called. This includes:
    // 1. Render target views bound to the output merger
    // 2. Shader resource views that might reference the texture
    // 3. Any outstanding GPU work
    //
    // We must:
    // 1. Unbind all render targets from the pipeline
    // 2. Release all ComPtr references to the backbuffer texture and views
    // 3. Flush the context to ensure GPU has finished using the resources
    //
    // Failure to do this results in DXGI_ERROR_INVALID_CALL (0x887A0001)
    // ====================================================================================

    // Step 1: Explicitly unbind render targets from the output merger
    // This is more targeted than ClearState() and ensures the backbuffer is unbound
    m_impl->state.context->OMSetRenderTargets(0, nullptr, nullptr);

    // Step 2: Clear all pipeline state to ensure nothing references the backbuffer
    m_impl->state.context->ClearState();

    // Step 3: Flush to ensure all GPU commands are complete
    m_impl->state.context->Flush();

    // Step 4: Clear the render target entry's references BEFORE releasing swapchain resources
    // The render target map holds copies of the ComPtrs, so we must reset those too
    if (sc.backbuffer.isValid()) {
        auto rtIt = m_impl->state.renderTargets.find(sc.backbuffer.id);
        if (rtIt != m_impl->state.renderTargets.end()) {
            rtIt->second.rtv.Reset();
            rtIt->second.dsv.Reset();
        }
    }

    // Step 5: Release swapchain's own resource references
    sc.backbufferRTV.Reset();
    sc.backbufferTexture.Reset();
    sc.depthDSV.Reset();
    sc.depthTexture.Reset();

    // Try to resize swapchain buffers
    // Use explicit buffer count of 1 to match creation (BufferCount = 1)
    // Use the stored format to ensure consistency
    HRESULT hr = sc.swapchain->ResizeBuffers(
        1,  // Match creation: BufferCount = 1
        width,
        height,
        sc.dxgiColorFormat,  // Use stored format for consistency
        0
    );

    if (FAILED(hr)) {
        // ResizeBuffers failed - this can happen if there are still references
        // Try recreating the swapchain entirely as a fallback

        // Store the window handle before releasing the swapchain
        HWND hwnd = (HWND)sc.window.platformHandle;

        // Release the old swapchain
        sc.swapchain.Reset();

        // Create new swapchain
        DXGI_SWAP_CHAIN_DESC swapchainDesc = {};
        swapchainDesc.BufferDesc.Width = width;
        swapchainDesc.BufferDesc.Height = height;
        swapchainDesc.BufferDesc.RefreshRate.Numerator = 60;
        swapchainDesc.BufferDesc.RefreshRate.Denominator = 1;
        swapchainDesc.BufferDesc.Format = sc.dxgiColorFormat;
        swapchainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
        swapchainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
        swapchainDesc.SampleDesc.Count = 1;
        swapchainDesc.SampleDesc.Quality = 0;
        swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapchainDesc.BufferCount = 1;
        swapchainDesc.OutputWindow = hwnd;
        swapchainDesc.Windowed = TRUE;
        swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        swapchainDesc.Flags = 0;

        hr = m_impl->state.dxgiFactory->CreateSwapChain(
            m_impl->state.device.Get(),
            &swapchainDesc,
            sc.swapchain.GetAddressOf()
        );

        if (FAILED(hr)) {
            LOG_ERROR(LogCategory::Render, "Failed to recreate swapchain: HRESULT = 0x{:08X}", static_cast<unsigned int>(hr));
            sc.isDead = true;
            return;
        }

    }

    sc.width = width;
    sc.height = height;

    // Recreate backbuffer resources
    hr = sc.swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)sc.backbufferTexture.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "Failed to get backbuffer after resize: HRESULT = 0x{:08X}", static_cast<unsigned int>(hr));
        sc.isDead = true;
        return;
    }

    hr = m_impl->state.device->CreateRenderTargetView(sc.backbufferTexture.Get(), nullptr, sc.backbufferRTV.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "Failed to create RTV after resize: HRESULT = 0x{:08X}", static_cast<unsigned int>(hr));
        sc.isDead = true;
        return;
    }

    // Recreate depth buffer
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    hr = m_impl->state.device->CreateTexture2D(&depthDesc, nullptr, sc.depthTexture.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "Failed to create depth buffer after resize: HRESULT = 0x{:08X}", static_cast<unsigned int>(hr));
        sc.isDead = true;
        return;
    }

    hr = m_impl->state.device->CreateDepthStencilView(sc.depthTexture.Get(), nullptr, sc.depthDSV.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "Failed to create DSV after resize: HRESULT = 0x{:08X}", static_cast<unsigned int>(hr));
        sc.isDead = true;
        return;
    }

    // Update render target
    if (sc.backbuffer.isValid()) {
        auto rtIt = m_impl->state.renderTargets.find(sc.backbuffer.id);
        if (rtIt != m_impl->state.renderTargets.end()) {
            rtIt->second.rtv = sc.backbufferRTV;
            rtIt->second.dsv = sc.depthDSV;
            rtIt->second.width = width;
            rtIt->second.height = height;
        }
    }

}

void GfxDeviceDX11::present(SwapchainHandle swapchain) {
    if (!swapchain.isValid()) {
        return;
    }

    auto it = m_impl->state.swapchains.find(swapchain.id);
    if (it == m_impl->state.swapchains.end()) {
        return;
    }

    DX11Swapchain& sc = it->second;

    // Don't present a dead swapchain (resize failed)
    if (sc.isDead) {
        return;
    }

    // Present
    UINT syncInterval = sc.vsync ? 1 : 0;
    HRESULT hr = sc.swapchain->Present(syncInterval, 0);

    // Check for device removed/reset scenarios
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        LOG_ERROR(LogCategory::Render, "Device lost during present: HRESULT = 0x{:08X}", static_cast<unsigned int>(hr));
        sc.isDead = true;
    }

    // Process deferred destructions
    m_impl->state.processDeferredDestructions();
}

RenderTargetHandle GfxDeviceDX11::getSwapchainBackbuffer(SwapchainHandle swapchain) {
    if (!swapchain.isValid()) {
        return RenderTargetHandle();
    }

    auto it = m_impl->state.swapchains.find(swapchain.id);
    if (it == m_impl->state.swapchains.end()) {
        return RenderTargetHandle();
    }

    // Don't return backbuffer for a dead swapchain
    if (it->second.isDead) {
        return RenderTargetHandle();
    }

    return it->second.backbuffer;
}

void GfxDeviceDX11::makeContextCurrent(SwapchainHandle /*swapchain*/) {
    // No-op for DirectX 11 - DX11 doesn't have the concept of "current context" like OpenGL.
    // The device and immediate context are always available once created.
    // This method exists for API compatibility with OpenGL-style backends.
}

void GfxDeviceDX11::setSwapchainHintForOffscreen(SwapchainHandle /*swapchain*/) {
    // No-op for DirectX 11 - it doesn't have the same synchronization requirements as Vulkan
    // DirectX 11 handles off-screen rendering synchronization implicitly
}

// NOTE: Continued in next part due to size constraints - this file implements:
// - Texture creation/destruction
// - Buffer creation/destruction
// - Sampler creation/destruction
// - Shader creation/destruction (requires HLSL compiler)
// - Pipeline creation/destruction
// - Render target creation/destruction
// - Uniform buffer creation/destruction
// - Update methods
// - Frame begin/submit
// - Mesh management
// - Font management

// TODO: Implement remaining methods (texture, buffer, sampler, shader, pipeline, render target, etc.)
// TODO: See continuation in next section

TextureHandle GfxDeviceDX11::createTexture(const TextureDesc& desc) {
    DX11Texture dx11Texture;
    dx11Texture.width = desc.width;
    dx11Texture.height = desc.height;
    dx11Texture.depth = desc.depth;
    dx11Texture.mipLevels = desc.mipLevels;
    dx11Texture.arrayLayers = desc.arrayLayers;
    dx11Texture.format = desc.format;
    dx11Texture.type = desc.type;
    dx11Texture.dxgiFormat = DX11Utils::toDXGIFormat(desc.format);

    // Create texture
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = desc.width;
    texDesc.Height = desc.height;
    texDesc.MipLevels = desc.mipLevels;
    texDesc.ArraySize = desc.arrayLayers;
    texDesc.Format = DX11Utils::isDepthFormat(desc.format) ?
                     DX11Utils::toDXGIFormatTypeless(desc.format) :
                     dx11Texture.dxgiFormat;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage = D3D11_USAGE_DEFAULT;

    // Set bind flags
    UINT bindFlags = 0;
    if ((desc.usage & TextureUsage::Sampled) != TextureUsage::None) {
        bindFlags |= D3D11_BIND_SHADER_RESOURCE;
    }
    if ((desc.usage & TextureUsage::RenderTarget) != TextureUsage::None) {
        if (DX11Utils::isDepthFormat(desc.format)) {
            bindFlags |= D3D11_BIND_DEPTH_STENCIL;
        } else {
            bindFlags |= D3D11_BIND_RENDER_TARGET;
        }
    }
    if ((desc.usage & TextureUsage::DepthStencil) != TextureUsage::None) {
        bindFlags |= D3D11_BIND_DEPTH_STENCIL;
    }
    if ((desc.usage & TextureUsage::Storage) != TextureUsage::None) {
        bindFlags |= D3D11_BIND_UNORDERED_ACCESS;
    }
    texDesc.BindFlags = bindFlags;

    // Handle cube maps
    if (desc.type == TextureType::TextureCube) {
        texDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
        texDesc.ArraySize = 6;  // Cube maps always have 6 faces
    }

    // Create with initial data if provided
    D3D11_SUBRESOURCE_DATA* pInitialData = nullptr;
    D3D11_SUBRESOURCE_DATA initialData = {};
    if (desc.initialData) {
        initialData.pSysMem = desc.initialData;
        initialData.SysMemPitch = desc.width * DX11Utils::getBytesPerPixel(dx11Texture.dxgiFormat);
        initialData.SysMemSlicePitch = initialData.SysMemPitch * desc.height;
        pInitialData = &initialData;
    }

    HRESULT hr = m_impl->state.device->CreateTexture2D(&texDesc, pInitialData, &dx11Texture.texture);
    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "Failed to create DirectX 11 texture");
        return TextureHandle();
    }

    // Create shader resource view
    if ((desc.usage & TextureUsage::Sampled) != TextureUsage::None) {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

        if (DX11Utils::isDepthFormat(desc.format)) {
            srvDesc.Format = DX11Utils::toDXGIFormatSRV(desc.format);
        } else {
            srvDesc.Format = dx11Texture.dxgiFormat;
        }

        if (desc.type == TextureType::TextureCube) {
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
            srvDesc.TextureCube.MipLevels = desc.mipLevels;
            srvDesc.TextureCube.MostDetailedMip = 0;
        } else if (desc.arrayLayers > 1) {
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
            srvDesc.Texture2DArray.MipLevels = desc.mipLevels;
            srvDesc.Texture2DArray.MostDetailedMip = 0;
            srvDesc.Texture2DArray.ArraySize = desc.arrayLayers;
            srvDesc.Texture2DArray.FirstArraySlice = 0;
        } else {
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = desc.mipLevels;
            srvDesc.Texture2D.MostDetailedMip = 0;
        }

        hr = m_impl->state.device->CreateShaderResourceView(dx11Texture.texture.Get(), &srvDesc, &dx11Texture.srv);
        if (FAILED(hr)) {
            LOG_ERROR(LogCategory::Render, "Failed to create texture SRV");
            return TextureHandle();
        }
    }

    // Create UAV for storage textures
    if ((desc.usage & TextureUsage::Storage) != TextureUsage::None) {
        D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = dx11Texture.dxgiFormat;
        uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
        uavDesc.Texture2D.MipSlice = 0;

        hr = m_impl->state.device->CreateUnorderedAccessView(dx11Texture.texture.Get(), &uavDesc, &dx11Texture.uav);
        if (FAILED(hr)) {
            
        }
    }

    uint32_t id = m_impl->state.nextTextureID++;
    m_impl->state.textures[id] = dx11Texture;

    return TextureHandle(id);
}

void GfxDeviceDX11::destroyTexture(TextureHandle texture) {
    if (!texture.isValid()) {
        return;
    }

    auto it = m_impl->state.textures.find(texture.id);
    if (it == m_impl->state.textures.end()) {
        return;
    }

    // Don't destroy swapchain images
    if (it->second.isSwapchainImage) {
        m_impl->state.textures.erase(it);
        return;
    }

    // Defer destruction for safety
    m_impl->state.deferTextureDestroy(it->second);
    m_impl->state.textures.erase(it);
}

BufferHandle GfxDeviceDX11::createBuffer(const BufferDesc& desc) {
    if (desc.size == 0) {
        LOG_ERROR(LogCategory::Render, "Cannot create buffer with size 0");
        return BufferHandle();
    }

    DX11Buffer dx11Buffer;
    dx11Buffer.size = desc.size;
    dx11Buffer.usage = desc.usage;

    // Determine D3D11 usage and CPU access flags
    D3D11_USAGE d3dUsage = D3D11_USAGE_DEFAULT;
    UINT cpuAccessFlags = 0;
    UINT bindFlags = 0;

    // Determine bind flags based on buffer usage
    if ((desc.usage & BufferUsage::Vertex) != BufferUsage::None) {
        bindFlags |= D3D11_BIND_VERTEX_BUFFER;
    }
    if ((desc.usage & BufferUsage::Index) != BufferUsage::None) {
        bindFlags |= D3D11_BIND_INDEX_BUFFER;
    }
    if ((desc.usage & BufferUsage::Uniform) != BufferUsage::None) {
        bindFlags |= D3D11_BIND_CONSTANT_BUFFER;
    }
    if ((desc.usage & BufferUsage::Storage) != BufferUsage::None) {
        bindFlags |= D3D11_BIND_UNORDERED_ACCESS;
    }

    // Determine CPU access based on transfer flags
    if ((desc.usage & BufferUsage::TransferDst) != BufferUsage::None) {
        d3dUsage = D3D11_USAGE_DYNAMIC;
        cpuAccessFlags = D3D11_CPU_ACCESS_WRITE;
    }
    if ((desc.usage & BufferUsage::TransferSrc) != BufferUsage::None) {
        d3dUsage = D3D11_USAGE_STAGING;
        cpuAccessFlags = D3D11_CPU_ACCESS_READ;
    }

    dx11Buffer.d3dUsage = d3dUsage;

    // Create buffer
    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.ByteWidth = static_cast<UINT>(desc.size);
    bufferDesc.Usage = d3dUsage;
    bufferDesc.BindFlags = bindFlags;
    bufferDesc.CPUAccessFlags = cpuAccessFlags;

    D3D11_SUBRESOURCE_DATA* pInitialData = nullptr;
    D3D11_SUBRESOURCE_DATA initialData = {};
    if (desc.initialData) {
        initialData.pSysMem = desc.initialData;
        pInitialData = &initialData;
    }

    HRESULT hr = m_impl->state.device->CreateBuffer(&bufferDesc, pInitialData, &dx11Buffer.buffer);
    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "Failed to create DirectX 11 buffer");
        return BufferHandle();
    }

    uint32_t id = m_impl->state.nextBufferID++;
    m_impl->state.buffers[id] = dx11Buffer;

    return BufferHandle(id);
}

void GfxDeviceDX11::destroyBuffer(BufferHandle buffer) {
    if (!buffer.isValid()) {
        return;
    }

    auto it = m_impl->state.buffers.find(buffer.id);
    if (it == m_impl->state.buffers.end()) {
        return;
    }

    // Defer destruction for safety
    m_impl->state.deferBufferDestroy(it->second);
    m_impl->state.buffers.erase(it);
}

SamplerHandle GfxDeviceDX11::createSampler(const SamplerDesc& desc) {
    DX11Sampler dx11Sampler;
    dx11Sampler.desc = desc;

    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = DX11Utils::toDX11Filter(desc.minFilter, desc.magFilter, desc.mipFilter);
    samplerDesc.AddressU = DX11Utils::toDX11AddressMode(desc.wrapU);
    samplerDesc.AddressV = DX11Utils::toDX11AddressMode(desc.wrapV);
    samplerDesc.AddressW = DX11Utils::toDX11AddressMode(desc.wrapW);
    samplerDesc.MipLODBias = 0.0f;
    samplerDesc.MaxAnisotropy = static_cast<UINT>(desc.maxAnisotropy);
    samplerDesc.ComparisonFunc = desc.compareEnable ? DX11Utils::toDX11CompareFunc(desc.compareFunc) : D3D11_COMPARISON_NEVER;
    samplerDesc.BorderColor[0] = 0.0f;
    samplerDesc.BorderColor[1] = 0.0f;
    samplerDesc.BorderColor[2] = 0.0f;
    samplerDesc.BorderColor[3] = 0.0f;
    samplerDesc.MinLOD = 0.0f;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    HRESULT hr = m_impl->state.device->CreateSamplerState(&samplerDesc, &dx11Sampler.sampler);
    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "Failed to create DirectX 11 sampler");
        return SamplerHandle();
    }

    uint32_t id = m_impl->state.nextSamplerID++;
    m_impl->state.samplers[id] = dx11Sampler;

    return SamplerHandle(id);
}

void GfxDeviceDX11::destroySampler(SamplerHandle sampler) {
    if (!sampler.isValid()) {
        return;
    }

    auto it = m_impl->state.samplers.find(sampler.id);
    if (it == m_impl->state.samplers.end()) {
        return;
    }

    m_impl->state.samplers.erase(it);
}

ShaderHandle GfxDeviceDX11::createShader(const ShaderDesc& desc) {
    // Helper to get shader stage name
    auto getShaderStageName = [](ShaderStage stage) -> const char* {
        switch (stage) {
            case ShaderStage::Vertex: return "Vertex";
            case ShaderStage::Fragment: return "Fragment/Pixel";
            case ShaderStage::Geometry: return "Geometry";
            case ShaderStage::Compute: return "Compute";
            default: return "Unknown";
        }
    };

    if (!desc.bytecode || desc.bytecodeSize == 0) {
        LOG_ERROR(LogCategory::Render, "[DX11] Invalid shader bytecode - bytecode: {}, size: {}",
            desc.bytecode ? "valid" : "null", desc.bytecodeSize);
        return ShaderHandle();
    }

    // Check if this is DXBC bytecode or HLSL source code
    // DXBC magic number is "DXBC" (0x44 0x58 0x42 0x43)
    const uint8_t* bytes = static_cast<const uint8_t*>(desc.bytecode);
    bool isDXBC = (desc.bytecodeSize >= 4 &&
                   bytes[0] == 0x44 && bytes[1] == 0x58 &&
                   bytes[2] == 0x42 && bytes[3] == 0x43);

    std::vector<uint8_t> compiledBytecode;
    const void* shaderBytecode = desc.bytecode;
    size_t shaderBytecodeSize = desc.bytecodeSize;

    // If this is HLSL source code, compile it to DXBC bytecode
    if (!isDXBC) {

        std::string hlslSource(static_cast<const char*>(desc.bytecode), desc.bytecodeSize);
        std::string entryPoint = desc.entryPoint.empty() ? "main" : desc.entryPoint;
        std::string errors;

        if (!HLSLCompiler::compile(hlslSource, desc.stage, entryPoint, compiledBytecode, errors)) {
            LOG_ERROR(LogCategory::Render, "[DX11] Failed to compile HLSL {} shader", getShaderStageName(desc.stage));
            return ShaderHandle();
        }

        shaderBytecode = compiledBytecode.data();
        shaderBytecodeSize = compiledBytecode.size();
        
    }

    DX11Shader dx11Shader;
    dx11Shader.stage = desc.stage;
    dx11Shader.entryPoint = desc.entryPoint.empty() ? "main" : desc.entryPoint;

    // Store bytecode (compiled DXBC)
    const uint8_t* bytecodeData = static_cast<const uint8_t*>(shaderBytecode);
    dx11Shader.bytecode.assign(bytecodeData, bytecodeData + shaderBytecodeSize);

    HRESULT hr = E_FAIL;

    // Create shader based on stage
    switch (desc.stage) {
        case ShaderStage::Vertex:
            
            hr = m_impl->state.device->CreateVertexShader(
                shaderBytecode,
                shaderBytecodeSize,
                nullptr,
                &dx11Shader.vertexShader
            );
            dx11Shader.shader = dx11Shader.vertexShader;
            if (SUCCEEDED(hr)) {
                
            }
            break;

        case ShaderStage::Fragment:
            
            hr = m_impl->state.device->CreatePixelShader(
                shaderBytecode,
                shaderBytecodeSize,
                nullptr,
                &dx11Shader.pixelShader
            );
            dx11Shader.shader = dx11Shader.pixelShader;
            if (SUCCEEDED(hr)) {
                
            }
            break;

        case ShaderStage::Geometry:
            
            hr = m_impl->state.device->CreateGeometryShader(
                shaderBytecode,
                shaderBytecodeSize,
                nullptr,
                &dx11Shader.geometryShader
            );
            dx11Shader.shader = dx11Shader.geometryShader;
            if (SUCCEEDED(hr)) {
                
            }
            break;

        case ShaderStage::Compute:
            
            hr = m_impl->state.device->CreateComputeShader(
                shaderBytecode,
                shaderBytecodeSize,
                nullptr,
                &dx11Shader.computeShader
            );
            dx11Shader.shader = dx11Shader.computeShader;
            if (SUCCEEDED(hr)) {
                
            }
            break;

        default:
            LOG_ERROR(LogCategory::Render, "[DX11] Unknown shader stage: {}", static_cast<int>(desc.stage));
            return ShaderHandle();
    }

    if (FAILED(hr)) {
        // Get detailed HRESULT information
        LOG_ERROR(LogCategory::Render, "[DX11] Failed to create {} shader - HRESULT: 0x{:08X}",
            getShaderStageName(desc.stage), static_cast<unsigned int>(hr));

        // Log common error codes
        switch (hr) {
            case E_INVALIDARG:
                LOG_ERROR(LogCategory::Render, "[DX11] Error: E_INVALIDARG - Invalid bytecode or parameters");
                break;
            case E_OUTOFMEMORY:
                LOG_ERROR(LogCategory::Render, "[DX11] Error: E_OUTOFMEMORY - Out of memory");
                break;
            case DXGI_ERROR_DEVICE_REMOVED:
                LOG_ERROR(LogCategory::Render, "[DX11] Error: DXGI_ERROR_DEVICE_REMOVED - Device was removed");
                break;
            default:
                LOG_ERROR(LogCategory::Render, "[DX11] Error: Unknown HRESULT code");
                break;
        }
        return ShaderHandle();
    }

    uint32_t id = m_impl->state.nextShaderID++;
    m_impl->state.shaders[id] = dx11Shader;

    return ShaderHandle(id);
}

void GfxDeviceDX11::destroyShader(ShaderHandle shader) {
    if (!shader.isValid()) {
        return;
    }

    auto it = m_impl->state.shaders.find(shader.id);
    if (it == m_impl->state.shaders.end()) {
        return;
    }

    // Defer destruction for safety
    m_impl->state.deferShaderDestroy(it->second);
    m_impl->state.shaders.erase(it);
}

PipelineHandle GfxDeviceDX11::createPipeline(const PipelineDesc& desc) {
    DX11Pipeline dx11Pipeline;
    dx11Pipeline.shaders = desc.shaders;
    dx11Pipeline.vertexLayout = desc.vertexLayout;
    dx11Pipeline.extraVertexBuffers = desc.extraVertexBuffers;
    dx11Pipeline.topology = desc.topology;

    // Build the per-input-slot stride table: slot 0 is the primary geometry
    // layout; each extra vertex buffer (e.g. binding 1 = per-instance data)
    // contributes its own stride at its binding index.
    dx11Pipeline.vertexStrides.assign(1, desc.vertexLayout.stride);
    for (const VertexBufferLayout& extra : desc.extraVertexBuffers) {
        if (extra.attributes.empty()) {
            continue;
        }
        const uint32_t slot = extra.attributes.front().binding;
        if (slot >= dx11Pipeline.vertexStrides.size()) {
            dx11Pipeline.vertexStrides.resize(slot + 1, 0);
        }
        dx11Pipeline.vertexStrides[slot] = extra.stride;
    }
    dx11Pipeline.d3dTopology = DX11Utils::toDX11Topology(desc.topology);
    dx11Pipeline.blendStateDesc = desc.blendState;
    dx11Pipeline.depthStencilStateDesc = desc.depthStencilState;
    dx11Pipeline.rasterizerStateDesc = desc.rasterizerState;

    // Find and store shader objects
    DX11Shader* vertexShader = nullptr;
    for (const auto& shaderHandle : desc.shaders) {
        auto shaderIt = m_impl->state.shaders.find(shaderHandle.id);
        if (shaderIt == m_impl->state.shaders.end()) {
            continue;
        }

        DX11Shader& shader = shaderIt->second;

        switch (shader.stage) {
            case ShaderStage::Vertex:
                dx11Pipeline.vertexShader = shader.vertexShader;
                vertexShader = &shader;
                break;
            case ShaderStage::Fragment:
                dx11Pipeline.pixelShader = shader.pixelShader;
                break;
            case ShaderStage::Geometry:
                dx11Pipeline.geometryShader = shader.geometryShader;
                break;
            default:
                break;
        }
    }

    // Create input layout from vertex shader bytecode
    if (vertexShader && !desc.vertexLayout.attributes.empty()) {
        // Track semantic indices for duplicate semantic names
        std::unordered_map<std::string, uint32_t> semanticIndexMap;
        std::vector<D3D11_INPUT_ELEMENT_DESC> inputElements;

        for (const auto& attr : desc.vertexLayout.attributes) {
            D3D11_INPUT_ELEMENT_DESC elementDesc = {};
            elementDesc.SemanticName = DX11Utils::getSemanticNameFromAttributeName(attr.name);

            // Track and increment semantic index for duplicate semantic names
            std::string semanticKey(elementDesc.SemanticName);
            uint32_t& semanticIndex = semanticIndexMap[semanticKey];
            elementDesc.SemanticIndex = semanticIndex++;

            elementDesc.Format = DX11Utils::toDX11VertexFormat(attr.format);
            elementDesc.InputSlot = attr.binding;
            elementDesc.AlignedByteOffset = attr.offset;
            elementDesc.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
            elementDesc.InstanceDataStepRate = 0;

            inputElements.push_back(elementDesc);
        }

        // Append per-instance (and any other extra) vertex buffer attributes.
        // The semanticIndexMap is shared with the geometry attributes above so
        // TEXCOORD indices continue incrementing (e.g. geometry a_TexCoord is
        // TEXCOORD0, instance attributes become TEXCOORD1..N) to match the
        // instanced shaders.
        for (const VertexBufferLayout& extra : desc.extraVertexBuffers) {
            const bool perInstance = (extra.inputRate == VertexInputRate::Instance);
            for (const auto& attr : extra.attributes) {
                D3D11_INPUT_ELEMENT_DESC elementDesc = {};
                elementDesc.SemanticName = DX11Utils::getSemanticNameFromAttributeName(attr.name);

                std::string semanticKey(elementDesc.SemanticName);
                uint32_t& semanticIndex = semanticIndexMap[semanticKey];
                elementDesc.SemanticIndex = semanticIndex++;

                elementDesc.Format = DX11Utils::toDX11VertexFormat(attr.format);
                elementDesc.InputSlot = attr.binding;
                elementDesc.AlignedByteOffset = attr.offset;
                elementDesc.InputSlotClass = perInstance ? D3D11_INPUT_PER_INSTANCE_DATA : D3D11_INPUT_PER_VERTEX_DATA;
                elementDesc.InstanceDataStepRate = perInstance ? 1u : 0u;

                inputElements.push_back(elementDesc);
            }
        }

        HRESULT hr = m_impl->state.device->CreateInputLayout(
            inputElements.data(),
            static_cast<UINT>(inputElements.size()),
            vertexShader->bytecode.data(),
            vertexShader->bytecode.size(),
            &dx11Pipeline.inputLayout
        );

        if (FAILED(hr)) {
            LOG_ERROR(LogCategory::Render, "[DX11] Failed to create input layout - HRESULT: 0x{:08X}", static_cast<unsigned int>(hr));

            // Log common error codes
            switch (hr) {
                case E_INVALIDARG:
                    LOG_ERROR(LogCategory::Render, "[DX11] Error: E_INVALIDARG - Semantic mismatch between input layout and vertex shader");
                    break;
                case E_OUTOFMEMORY:
                    LOG_ERROR(LogCategory::Render, "[DX11] Error: E_OUTOFMEMORY - Out of memory");
                    break;
                default:
                    LOG_ERROR(LogCategory::Render, "[DX11] Error: Unknown HRESULT code");
                    break;
            }
            return PipelineHandle();
        }

    }

    // Create rasterizer state
    D3D11_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.FillMode = DX11Utils::toDX11FillMode(desc.rasterizerState.fillMode);
    rasterizerDesc.CullMode = DX11Utils::toDX11CullMode(desc.rasterizerState.cullMode);
    rasterizerDesc.FrontCounterClockwise = (desc.rasterizerState.frontFace == WindingOrder::CounterClockwise);
    rasterizerDesc.DepthBias = static_cast<INT>(desc.rasterizerState.depthBiasConstantFactor);
    rasterizerDesc.DepthBiasClamp = desc.rasterizerState.depthBiasClamp;
    rasterizerDesc.SlopeScaledDepthBias = desc.rasterizerState.depthBiasSlopeFactor;
    rasterizerDesc.DepthClipEnable = !desc.rasterizerState.depthClampEnable;
    // The engine drives scissor dynamically (like glEnable(GL_SCISSOR_TEST) on GL):
    // executeRenderPasses sets a scissor rect for every pass (full-target by default,
    // tightened for UI clipping). D3D11 bakes scissor enable into the rasterizer state
    // and ignores RSSetScissorRects unless it is on, so enable it unconditionally; an
    // unclipped pass simply gets a full-render-target rect.
    rasterizerDesc.ScissorEnable = TRUE;
    rasterizerDesc.MultisampleEnable = FALSE;
    rasterizerDesc.AntialiasedLineEnable = FALSE;

    HRESULT hr = m_impl->state.device->CreateRasterizerState(&rasterizerDesc, &dx11Pipeline.rasterizerState);
    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "Failed to create rasterizer state");
        return PipelineHandle();
    }

    // Create depth/stencil state
    D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
    depthStencilDesc.DepthEnable = desc.depthStencilState.depthTestEnable;
    depthStencilDesc.DepthWriteMask = desc.depthStencilState.depthWriteEnable ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
    depthStencilDesc.DepthFunc = DX11Utils::toDX11CompareFunc(desc.depthStencilState.depthCompareFunc);
    depthStencilDesc.StencilEnable = desc.depthStencilState.stencilEnable;
    depthStencilDesc.StencilReadMask = 0xFF;
    depthStencilDesc.StencilWriteMask = 0xFF;

    // Default stencil operations (front/back faces)
    depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    depthStencilDesc.BackFace = depthStencilDesc.FrontFace;

    hr = m_impl->state.device->CreateDepthStencilState(&depthStencilDesc, &dx11Pipeline.depthStencilState);
    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "Failed to create depth/stencil state");
        return PipelineHandle();
    }

    // Create blend state
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;  // Use same blend state for all render targets

    D3D11_RENDER_TARGET_BLEND_DESC& rtBlendDesc = blendDesc.RenderTarget[0];
    rtBlendDesc.BlendEnable = desc.blendState.blendEnable;
    rtBlendDesc.SrcBlend = DX11Utils::toDX11Blend(desc.blendState.srcColorBlend);
    rtBlendDesc.DestBlend = DX11Utils::toDX11Blend(desc.blendState.dstColorBlend);
    rtBlendDesc.BlendOp = DX11Utils::toDX11BlendOp(desc.blendState.colorBlendOp);
    rtBlendDesc.SrcBlendAlpha = DX11Utils::toDX11Blend(desc.blendState.srcAlphaBlend);
    rtBlendDesc.DestBlendAlpha = DX11Utils::toDX11Blend(desc.blendState.dstAlphaBlend);
    rtBlendDesc.BlendOpAlpha = DX11Utils::toDX11BlendOp(desc.blendState.alphaBlendOp);
    rtBlendDesc.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    hr = m_impl->state.device->CreateBlendState(&blendDesc, &dx11Pipeline.blendState);
    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "Failed to create blend state");
        return PipelineHandle();
    }

    // Perform shader reflection to extract constant buffer and texture bindings
    for (const auto& shaderHandle : desc.shaders) {
        auto shaderIt = m_impl->state.shaders.find(shaderHandle.id);
        if (shaderIt == m_impl->state.shaders.end()) {
            continue;
        }

        DX11Shader& shader = shaderIt->second;

        // Reflect constant buffers, textures, and samplers
        std::vector<HLSLCompiler::ConstantBufferInfo> constantBuffers;
        std::vector<HLSLCompiler::TextureBindingInfo> textures;
        std::vector<HLSLCompiler::TextureBindingInfo> samplers;

        if (HLSLCompiler::reflectShader(shader.bytecode, constantBuffers, textures, samplers)) {
            // Store constant buffer info
            for (const auto& cb : constantBuffers) {
                DX11Pipeline::ConstantBufferInfo cbInfo;
                cbInfo.name = cb.name;
                cbInfo.size = cb.size;
                cbInfo.slot = cb.slot;
                cbInfo.stage = shader.stage;
                dx11Pipeline.constantBuffers.push_back(cbInfo);
            }

            // Store texture binding info
            for (const auto& tex : textures) {
                DX11Pipeline::TextureBindingInfo texInfo;
                texInfo.name = tex.name;
                texInfo.slot = tex.slot;
                texInfo.stage = shader.stage;
                dx11Pipeline.textureBindings.push_back(texInfo);
            }

            // Store sampler binding info
            for (const auto& samp : samplers) {
                DX11Pipeline::TextureBindingInfo sampInfo;
                sampInfo.name = samp.name;
                sampInfo.slot = samp.slot;
                sampInfo.stage = shader.stage;
                dx11Pipeline.samplerBindings.push_back(sampInfo);
            }

            // Extract variable offsets from constant buffer slot 0 (push constants equivalent)
            // We extract from BOTH vertex and fragment shaders because the fragment shader
            // may have additional material uniforms that the vertex shader doesn't need
            std::vector<HLSLCompiler::VariableInfo> variables;
            if (HLSLCompiler::reflectConstantBufferVariables(shader.bytecode, 0, variables)) {
                for (const auto& var : variables) {
                    // Only add if not already present (vertex shader takes priority for overlapping names)
                    if (dx11Pipeline.uniformOffsets.find(var.name) == dx11Pipeline.uniformOffsets.end()) {
                        dx11Pipeline.uniformOffsets[var.name] = var.offset;
                        
                    }
                    // Track the maximum offset + size to know the total buffer size
                    uint32_t endOffset = var.offset + var.size;
                    if (endOffset > dx11Pipeline.uniformBufferSize) {
                        dx11Pipeline.uniformBufferSize = endOffset;
                    }
                }
                
            }
        }
    }

    uint32_t id = m_impl->state.nextPipelineID++;
    m_impl->state.pipelines[id] = dx11Pipeline;

    return PipelineHandle(id);
}

void GfxDeviceDX11::destroyPipeline(PipelineHandle pipeline) {
    if (!pipeline.isValid()) {
        return;
    }

    auto it = m_impl->state.pipelines.find(pipeline.id);
    if (it == m_impl->state.pipelines.end()) {
        return;
    }

    // Defer destruction for safety
    m_impl->state.deferPipelineDestroy(it->second);
    m_impl->state.pipelines.erase(it);
}

RenderTargetHandle GfxDeviceDX11::createRenderTarget(const RenderTargetDesc& desc) {
    DX11RenderTarget dx11RT;
    dx11RT.width = desc.width;
    dx11RT.height = desc.height;
    dx11RT.colorFormat = desc.colorFormat;
    dx11RT.depthFormat = desc.depthFormat;
    dx11RT.hasColor = desc.hasColor;
    dx11RT.hasDepth = desc.hasDepth;
    dx11RT.isCubeMap = desc.isCubeMap;
    dx11RT.dxgiColorFormat = DX11Utils::toDXGIFormat(desc.colorFormat);
    dx11RT.dxgiDepthFormat = DX11Utils::toDXGIFormat(desc.depthFormat);

    // Create color texture and RTV
    if (desc.hasColor) {
        TextureDesc colorTexDesc = {};
        colorTexDesc.width = desc.width;
        colorTexDesc.height = desc.height;
        colorTexDesc.format = desc.colorFormat;
        colorTexDesc.mipLevels = 1;
        colorTexDesc.arrayLayers = desc.isCubeMap ? 6 : 1;
        colorTexDesc.type = desc.isCubeMap ? TextureType::TextureCube : TextureType::Texture2D;
        colorTexDesc.usage = TextureUsage::RenderTarget | TextureUsage::Sampled;

        dx11RT.colorTextureHandle = createTexture(colorTexDesc);
        if (!dx11RT.colorTextureHandle.isValid()) {
            LOG_ERROR(LogCategory::Render, "Failed to create render target color texture");
            return RenderTargetHandle();
        }

        auto& colorTexture = m_impl->state.textures[dx11RT.colorTextureHandle.id];

        // Create render target view
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = dx11RT.dxgiColorFormat;

        if (desc.isCubeMap) {
            rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
            rtvDesc.Texture2DArray.MipSlice = 0;
            rtvDesc.Texture2DArray.FirstArraySlice = 0;
            rtvDesc.Texture2DArray.ArraySize = 6;
        } else {
            rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            rtvDesc.Texture2D.MipSlice = 0;
        }

        HRESULT hr = m_impl->state.device->CreateRenderTargetView(
            colorTexture.texture.Get(),
            &rtvDesc,
            &dx11RT.rtv
        );

        if (FAILED(hr)) {
            LOG_ERROR(LogCategory::Render, "Failed to create RTV");
            destroyTexture(dx11RT.colorTextureHandle);
            return RenderTargetHandle();
        }
    }

    // Create depth texture and DSV
    if (desc.hasDepth) {
        TextureDesc depthTexDesc = {};
        depthTexDesc.width = desc.width;
        depthTexDesc.height = desc.height;
        depthTexDesc.format = desc.depthFormat;
        depthTexDesc.mipLevels = 1;
        depthTexDesc.arrayLayers = desc.isCubeMap ? 6 : 1;
        depthTexDesc.type = desc.isCubeMap ? TextureType::TextureCube : TextureType::Texture2D;
        depthTexDesc.usage = TextureUsage::DepthStencil | TextureUsage::Sampled;

        dx11RT.depthTextureHandle = createTexture(depthTexDesc);
        if (!dx11RT.depthTextureHandle.isValid()) {
            LOG_ERROR(LogCategory::Render, "Failed to create render target depth texture");
            if (desc.hasColor) {
                destroyTexture(dx11RT.colorTextureHandle);
            }
            return RenderTargetHandle();
        }

        auto& depthTexture = m_impl->state.textures[dx11RT.depthTextureHandle.id];

        // Create depth/stencil view
        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DX11Utils::toDXGIFormatDSV(desc.depthFormat);

        if (desc.isCubeMap) {
            dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
            dsvDesc.Texture2DArray.MipSlice = 0;
            dsvDesc.Texture2DArray.FirstArraySlice = 0;
            dsvDesc.Texture2DArray.ArraySize = 6;
        } else {
            dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
            dsvDesc.Texture2D.MipSlice = 0;
        }

        HRESULT hr = m_impl->state.device->CreateDepthStencilView(
            depthTexture.texture.Get(),
            &dsvDesc,
            &dx11RT.dsv
        );

        if (FAILED(hr)) {
            LOG_ERROR(LogCategory::Render, "[DX11] Failed to create DSV - HRESULT: 0x{:08X}, format: {}, viewDim: {}",
                static_cast<unsigned int>(hr), static_cast<int>(dsvDesc.Format), static_cast<int>(dsvDesc.ViewDimension));
            destroyTexture(dx11RT.depthTextureHandle);
            if (desc.hasColor) {
                destroyTexture(dx11RT.colorTextureHandle);
            }
            return RenderTargetHandle();
        }

    }

    uint32_t id = m_impl->state.nextRenderTargetID++;
    m_impl->state.renderTargets[id] = dx11RT;

    return RenderTargetHandle(id);
}

void GfxDeviceDX11::destroyRenderTarget(RenderTargetHandle target) {
    if (!target.isValid()) {
        return;
    }

    auto it = m_impl->state.renderTargets.find(target.id);
    if (it == m_impl->state.renderTargets.end()) {
        return;
    }

    DX11RenderTarget& rt = it->second;

    // Don't destroy swapchain backbuffers
    if (rt.isSwapchainBackbuffer) {
        m_impl->state.renderTargets.erase(it);
        return;
    }

    // Destroy associated textures
    if (rt.colorTextureHandle.isValid()) {
        destroyTexture(rt.colorTextureHandle);
    }
    if (rt.depthTextureHandle.isValid()) {
        destroyTexture(rt.depthTextureHandle);
    }

    m_impl->state.renderTargets.erase(it);
}

TextureHandle GfxDeviceDX11::getRenderTargetColorTexture(RenderTargetHandle target) {
    if (!target.isValid()) {
        return TextureHandle();
    }

    auto it = m_impl->state.renderTargets.find(target.id);
    if (it == m_impl->state.renderTargets.end()) {
        return TextureHandle();
    }

    return it->second.colorTextureHandle;
}

TextureHandle GfxDeviceDX11::getRenderTargetDepthTexture(RenderTargetHandle target) {
    if (!target.isValid()) {
        return TextureHandle();
    }

    auto it = m_impl->state.renderTargets.find(target.id);
    if (it == m_impl->state.renderTargets.end()) {
        return TextureHandle();
    }

    return it->second.depthTextureHandle;
}

void GfxDeviceDX11::attachCubeMapFace(RenderTargetHandle target, uint32_t faceIndex) {
    if (!target.isValid() || faceIndex >= 6) {
        return;
    }

    auto it = m_impl->state.renderTargets.find(target.id);
    if (it == m_impl->state.renderTargets.end()) {
        return;
    }

    DX11RenderTarget& rt = it->second;
    if (!rt.isCubeMap) {
        
        return;
    }

    rt.currentCubeFace = static_cast<int>(faceIndex);

    // Recreate RTVs/DSVs for the specific face
    if (rt.hasColor && rt.colorTextureHandle.isValid()) {
        auto& colorTexture = m_impl->state.textures[rt.colorTextureHandle.id];

        rt.rtv.Reset();

        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = rt.dxgiColorFormat;
        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
        rtvDesc.Texture2DArray.MipSlice = 0;
        rtvDesc.Texture2DArray.FirstArraySlice = faceIndex;
        rtvDesc.Texture2DArray.ArraySize = 1;

        m_impl->state.device->CreateRenderTargetView(
            colorTexture.texture.Get(),
            &rtvDesc,
            &rt.rtv
        );
    }

    if (rt.hasDepth && rt.depthTextureHandle.isValid()) {
        auto& depthTexture = m_impl->state.textures[rt.depthTextureHandle.id];

        rt.dsv.Reset();

        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = rt.dxgiDepthFormat;
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
        dsvDesc.Texture2DArray.MipSlice = 0;
        dsvDesc.Texture2DArray.FirstArraySlice = faceIndex;
        dsvDesc.Texture2DArray.ArraySize = 1;

        m_impl->state.device->CreateDepthStencilView(
            depthTexture.texture.Get(),
            &dsvDesc,
            &rt.dsv
        );
    }
}

void GfxDeviceDX11::unbindFramebuffer() {
    // Unbind all render targets
    m_impl->state.context->OMSetRenderTargets(0, nullptr, nullptr);
}

UniformBufferHandle GfxDeviceDX11::createUniformBuffer(uint32_t size) {
    // DirectX 11 requires constant buffer sizes to be multiples of 16 bytes
    uint32_t alignedSize = (size + 15) & ~15;

    DX11UniformBuffer dx11UBO;
    dx11UBO.size = alignedSize;

    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = alignedSize;
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    HRESULT hr = m_impl->state.device->CreateBuffer(&cbDesc, nullptr, &dx11UBO.buffer);
    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "Failed to create uniform buffer");
        return UniformBufferHandle();
    }

    uint32_t id = m_impl->state.nextUniformBufferID++;
    m_impl->state.uniformBuffers[id] = dx11UBO;

    return UniformBufferHandle(id);
}

void GfxDeviceDX11::destroyUniformBuffer(UniformBufferHandle buffer) {
    if (!buffer.isValid()) {
        return;
    }

    auto it = m_impl->state.uniformBuffers.find(buffer.id);
    if (it == m_impl->state.uniformBuffers.end()) {
        return;
    }

    m_impl->state.uniformBuffers.erase(it);
}

void GfxDeviceDX11::updateBuffer(BufferHandle buffer, const void* data, uint64_t size, uint64_t offset) {
    if (!buffer.isValid() || !data || size == 0) {
        return;
    }

    auto it = m_impl->state.buffers.find(buffer.id);
    if (it == m_impl->state.buffers.end()) {
        return;
    }

    DX11Buffer& dx11Buffer = it->second;

    if (dx11Buffer.d3dUsage == D3D11_USAGE_DYNAMIC) {
        // Map and update dynamic buffer
        D3D11_MAPPED_SUBRESOURCE mapped;
        HRESULT hr = m_impl->state.context->Map(dx11Buffer.buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (SUCCEEDED(hr)) {
            std::memcpy(static_cast<uint8_t*>(mapped.pData) + offset, data, size);
            m_impl->state.context->Unmap(dx11Buffer.buffer.Get(), 0);
        } else {
            LOG_ERROR(LogCategory::Render, "Failed to map buffer for update");
        }
    } else {
        // Use UpdateSubresource for default buffers
        D3D11_BOX box;
        box.left = static_cast<UINT>(offset);
        box.right = static_cast<UINT>(offset + size);
        box.top = 0;
        box.bottom = 1;
        box.front = 0;
        box.back = 1;

        m_impl->state.context->UpdateSubresource(dx11Buffer.buffer.Get(), 0, &box, data, 0, 0);
    }
}

void GfxDeviceDX11::updateTexture(TextureHandle texture, const void* data, uint32_t mipLevel, uint32_t arrayLayer) {
    if (!texture.isValid() || !data) {
        return;
    }

    auto it = m_impl->state.textures.find(texture.id);
    if (it == m_impl->state.textures.end()) {
        return;
    }

    DX11Texture& dx11Texture = it->second;

    // Calculate subresource index
    uint32_t subresource = D3D11CalcSubresource(mipLevel, arrayLayer, dx11Texture.mipLevels);

    // Calculate mip dimensions
    uint32_t mipWidth = std::max(1u, dx11Texture.width >> mipLevel);
    uint32_t mipHeight = std::max(1u, dx11Texture.height >> mipLevel);
    uint32_t rowPitch = mipWidth * DX11Utils::getBytesPerPixel(dx11Texture.dxgiFormat);

    // Update texture data
    m_impl->state.context->UpdateSubresource(
        dx11Texture.texture.Get(),
        subresource,
        nullptr,  // Update entire subresource
        data,
        rowPitch,
        rowPitch * mipHeight
    );
}

void GfxDeviceDX11::updateUniformBuffer(UniformBufferHandle buffer, const void* data, uint32_t size, uint32_t offset) {
    if (!buffer.isValid() || !data || size == 0) {
        return;
    }

    auto it = m_impl->state.uniformBuffers.find(buffer.id);
    if (it == m_impl->state.uniformBuffers.end()) {
        return;
    }

    DX11UniformBuffer& dx11UBO = it->second;

    // Debug: Log light UBO updates (LightUniformBuffer is 6592 bytes)
    // Map and update the buffer
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = m_impl->state.context->Map(dx11UBO.buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr)) {
        std::memcpy(static_cast<uint8_t*>(mapped.pData) + offset, data, size);
        m_impl->state.context->Unmap(dx11UBO.buffer.Get(), 0);
    } else {
        LOG_ERROR(LogCategory::Render, "Failed to map uniform buffer for update");
    }
}

std::unique_ptr<IGfxCommandList> GfxDeviceDX11::beginFrame(RenderTargetHandle target) {
    auto commandList = std::make_unique<GfxCommandListDX11>(this, &m_impl->state);

    if (target.isValid()) {
        commandList->setRenderTarget(target);
    }

    return commandList;
}

void GfxDeviceDX11::submit(std::unique_ptr<IGfxCommandList> commandList) {
    // DirectX 11 uses immediate mode - commands are already executed
    // Just clean up the command list
    commandList.reset();
}

void GfxDeviceDX11::waitIdle() {
    if (m_impl->state.context) {
        m_impl->state.context->Flush();
        m_impl->state.context->ClearState();
    }
}

MeshHandle GfxDeviceDX11::createMesh(const MeshData& meshData) {
    GPUMesh gpuMesh;
    gpuMesh.vertexCount = static_cast<uint32_t>(meshData.vertices.size());
    gpuMesh.indexCount = static_cast<uint32_t>(meshData.indices.size());
    gpuMesh.submeshes = meshData.submeshes;
    gpuMesh.bounds = meshData.bounds;

    // Create vertex buffer
    if (!meshData.vertices.empty()) {
        BufferDesc vertexBufferDesc;
        vertexBufferDesc.size = meshData.vertices.size() * sizeof(Vertex);
        vertexBufferDesc.usage = BufferUsage::Vertex;
        vertexBufferDesc.initialData = meshData.vertices.data();

        gpuMesh.vertexBuffer = createBuffer(vertexBufferDesc);
        if (!gpuMesh.vertexBuffer.isValid()) {
            LOG_ERROR(LogCategory::Render, "Failed to create mesh vertex buffer");
            return MeshHandle();
        }
    }

    // Create index buffer
    if (!meshData.indices.empty()) {
        BufferDesc indexBufferDesc;
        indexBufferDesc.size = meshData.indices.size() * sizeof(uint32_t);
        indexBufferDesc.usage = BufferUsage::Index;
        indexBufferDesc.initialData = meshData.indices.data();

        gpuMesh.indexBuffer = createBuffer(indexBufferDesc);
        if (!gpuMesh.indexBuffer.isValid()) {
            LOG_ERROR(LogCategory::Render, "Failed to create mesh index buffer");
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

const GPUMesh* GfxDeviceDX11::getMesh(MeshHandle handle) const {
    if (!handle.isValid()) {
        return nullptr;
    }

    auto it = m_impl->meshes.find(handle.id);
    if (it == m_impl->meshes.end()) {
        return nullptr;
    }

    return &it->second;
}

void GfxDeviceDX11::destroyMesh(MeshHandle handle) {
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

FontAtlas GfxDeviceDX11::buildBakedAtlas(const BakedFontAtlas& baked) {
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
        LOG_ERROR(LogCategory::Render, "Failed to create font atlas texture");
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

FontHandle GfxDeviceDX11::createFontAtlas(const FontDesc& desc) {
    BakedFontAtlas baked = BakeFontAtlas(desc, GetFontOversample());
    if (!baked.success) {
        LOG_ERROR(LogCategory::Render, "Failed to bake font: {}", desc.fontPath);
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

void GfxDeviceDX11::refreshFontAtlases() {
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

const FontAtlas* GfxDeviceDX11::getFontAtlas(FontHandle handle) const {
    if (!handle.isValid()) {
        return nullptr;
    }

    auto it = m_impl->fonts.find(handle.id);
    if (it == m_impl->fonts.end()) {
        return nullptr;
    }

    return &it->second;
}

void GfxDeviceDX11::destroyFontAtlas(FontHandle handle) {
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

#endif // LUPINE_HAS_DIRECTX11
