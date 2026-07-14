/**
 * Metal Graphics Device Implementation
 *
 * This file implements the Metal rendering backend for macOS and iOS.
 * Uses Objective-C++ (.mm) for Metal API calls.
 */

#include "lupine/rendering/backends/GfxDeviceMetal.hpp"

#ifdef LUPINE_HAS_METAL
#ifdef __APPLE__

#include "lupine/rendering/backends/metal/MetalState.hpp"
#include "lupine/rendering/backends/metal/GfxCommandListMetal.hpp"
#include "lupine/rendering/Mesh.hpp"
#include "lupine/rendering/Font.hpp"
#include "lupine/rendering/FontBaker.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/platform/FileSystem.hpp"
#include "lupine/platform/PackFile.hpp"
#include "lupine/asset/Asset.hpp"

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/CAMetalLayer.h>
#import <TargetConditionals.h>

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#elif TARGET_OS_IOS || TARGET_OS_TV
#import <UIKit/UIKit.h>
#endif

// For font rendering (using stb_truetype like other backends)
#include <stb_truetype.h>

namespace lupine {

// ============================================================================
// Implementation Structure
// ============================================================================

struct GfxDeviceMetal::Impl {
    MetalState state;
    GfxDeviceCaps caps;

    // Mesh storage
    std::unordered_map<uint32_t, GPUMesh> meshes;
    uint32_t nextMeshID = 1;

    // Font atlas storage
    std::unordered_map<uint32_t, FontAtlas> fontAtlases;
    std::unordered_map<uint32_t, FontDesc> fontDescs;
    uint32_t nextFontID = 1;

    // Default texture filtering
    FilterMode defaultMinFilter = FilterMode::Linear;
    FilterMode defaultMagFilter = FilterMode::Linear;

    Impl() = default;
    ~Impl() = default;
};

// ============================================================================
// Constructor / Destructor
// ============================================================================

GfxDeviceMetal::GfxDeviceMetal() : m_impl(std::make_unique<Impl>()) {
}

GfxDeviceMetal::~GfxDeviceMetal() {
    shutdown();
}

// ============================================================================
// Initialization & Capabilities
// ============================================================================

bool GfxDeviceMetal::initialize() {
    LOG_DEBUG(LogCategory::Render, "Metal: Initializing Metal device...");

    // Get the default Metal device (usually the discrete GPU if available)
    m_impl->state.device = MTLCreateSystemDefaultDevice();
    if (!m_impl->state.device) {
        LOG_ERROR(LogCategory::Render, "Metal: Failed to create Metal device");
        return false;
    }

    // Create command queue
    m_impl->state.commandQueue = [m_impl->state.device newCommandQueue];
    if (!m_impl->state.commandQueue) {
        LOG_ERROR(LogCategory::Render, "Metal: Failed to create command queue");
        return false;
    }

    // Initialize capabilities
    m_impl->caps.backend = GraphicsBackend::Metal;
    m_impl->caps.deviceName = std::string([m_impl->state.device.name UTF8String]);

    // Comprehensive Metal feature detection
    // Detect GPU family and Metal version
    std::string metalVersion = "Metal";

    // Check for Metal 3 features (Apple Silicon, macOS 13+, iOS 16+)
    if (@available(macOS 13.0, iOS 16.0, *)) {
        if ([m_impl->state.device supportsFamily:MTLGPUFamilyMetal3]) {
            metalVersion = "Metal 3";
        }
    }

    // Check for Apple GPU families (Apple Silicon - macOS and iOS)
    if (@available(macOS 10.15, iOS 13.0, *)) {
        // Check for newest families first
#if defined(__IPHONE_17_0) || defined(__MAC_14_0)
        if (@available(macOS 14.0, iOS 17.0, *)) {
            if ([m_impl->state.device supportsFamily:MTLGPUFamilyApple9]) {
                metalVersion += " (Apple9 - M3/A17)";
            } else if ([m_impl->state.device supportsFamily:MTLGPUFamilyApple8]) {
                metalVersion += " (Apple8 - M2/A16)";
            }
        }
#endif
        if ([m_impl->state.device supportsFamily:MTLGPUFamilyApple7]) {
            metalVersion += " (Apple7 - M1 Pro/Max/Ultra or A15+)";
        } else if ([m_impl->state.device supportsFamily:MTLGPUFamilyApple6]) {
            metalVersion += " (Apple6 - M1 or A14)";
        } else if ([m_impl->state.device supportsFamily:MTLGPUFamilyApple5]) {
            metalVersion += " (Apple5 - A12/A13)";
        } else if ([m_impl->state.device supportsFamily:MTLGPUFamilyApple4]) {
            metalVersion += " (Apple4 - A11)";
        } else if ([m_impl->state.device supportsFamily:MTLGPUFamilyApple3]) {
            metalVersion += " (Apple3 - A9/A10)";
        }
#if TARGET_OS_IOS
        // iOS-specific older families
        else if ([m_impl->state.device supportsFamily:MTLGPUFamilyApple2]) {
            metalVersion += " (Apple2 - A8)";
        } else if ([m_impl->state.device supportsFamily:MTLGPUFamilyApple1]) {
            metalVersion += " (Apple1 - A7)";
        }
#endif
    }

    // Check for Mac GPU families (Intel/AMD discrete GPUs)
    if (@available(macOS 10.15, *)) {
        if ([m_impl->state.device supportsFamily:MTLGPUFamilyMac2]) {
            if (metalVersion == "Metal" || metalVersion == "Metal 3") {
                metalVersion += " (Mac2 - Modern AMD/Intel)";
            }
        } else if ([m_impl->state.device supportsFamily:MTLGPUFamilyMac1]) {
            if (metalVersion == "Metal" || metalVersion == "Metal 3") {
                metalVersion += " (Mac1 - Older AMD/Intel)";
            }
        }
    }

    m_impl->caps.apiVersion = metalVersion;

    // Query device limits based on GPU family
    // These are minimum guaranteed values; actual limits may be higher
    NSUInteger maxTextureSize = 16384;
    NSUInteger maxTextureLayers = 2048;

    // Check for higher limits on newer GPUs
    if (@available(macOS 10.15, iOS 13.0, *)) {
        if ([m_impl->state.device supportsFamily:MTLGPUFamilyApple6] ||
            [m_impl->state.device supportsFamily:MTLGPUFamilyMac2]) {
            maxTextureSize = 16384;
            maxTextureLayers = 2048;
        }
    }

    m_impl->caps.maxTextureSize = static_cast<uint32_t>(maxTextureSize);
    m_impl->caps.maxTextureLayers = static_cast<uint32_t>(maxTextureLayers);
    m_impl->caps.maxVertexAttributes = 31; // Metal supports up to 31 vertex attributes
    m_impl->caps.maxUniformBufferSize = static_cast<uint32_t>(m_impl->state.device.maxBufferLength);

    // Feature support detection
    m_impl->caps.supportsCompute = true; // All Metal devices support compute

    // Metal doesn't have traditional geometry shaders, but mesh shaders are available on newer GPUs
    m_impl->caps.supportsGeometryShader = false;

    // Tessellation support (available on most Metal GPUs)
    m_impl->caps.supportsTessellation = true;

    // Argument buffers (Metal's approach to bindless) - available on all Metal 2+ devices
    if (@available(macOS 10.13, iOS 11.0, *)) {
        // Check argument buffer tier
        MTLArgumentBuffersTier tier = m_impl->state.device.argumentBuffersSupport;
        m_impl->caps.supportsBindless = (tier == MTLArgumentBuffersTier2);
        if (tier == MTLArgumentBuffersTier2) {
            LOG_DEBUG(LogCategory::Render, "Metal: Argument Buffers Tier 2 supported (bindless-like)");
        }
    } else {
        m_impl->caps.supportsBindless = false;
    }

    // Ray tracing support (Apple Silicon with A14+ or M1+)
    m_impl->caps.supportsRayTracing = m_impl->state.device.supportsRaytracing;

    // Log additional capabilities for debugging
    LOG_DEBUG(LogCategory::Render, "Metal: Max buffer length: {} MB",
              m_impl->state.device.maxBufferLength / (1024 * 1024));
    LOG_DEBUG(LogCategory::Render, "Metal: Max threads per threadgroup: {}",
              m_impl->state.device.maxThreadsPerThreadgroup.width);
    LOG_DEBUG(LogCategory::Render, "Metal: Unified memory: {}",
              m_impl->state.device.hasUnifiedMemory ? "yes" : "no");
    LOG_DEBUG(LogCategory::Render, "Metal: Low power: {}",
              m_impl->state.device.lowPower ? "yes" : "no");
    LOG_DEBUG(LogCategory::Render, "Metal: Headless: {}",
              m_impl->state.device.headless ? "yes" : "no");

    // Create dummy resources
    m_impl->state.createDummyResources();

    LOG_DEBUG(LogCategory::Render, "Metal: Initialized device: {}", m_impl->caps.deviceName);
    LOG_DEBUG(LogCategory::Render, "Metal: Ray tracing support: {}", m_impl->caps.supportsRayTracing ? "yes" : "no");

    // Platform-specific logging
#if TARGET_OS_IOS
    LOG_DEBUG(LogCategory::Render, "Metal: Platform: iOS");
    LOG_DEBUG(LogCategory::Render, "Metal: Using TBDR optimizations (memoryless depth)");
#elif TARGET_OS_TV
    LOG_DEBUG(LogCategory::Render, "Metal: Platform: tvOS");
    LOG_DEBUG(LogCategory::Render, "Metal: Using TBDR optimizations (memoryless depth)");
#elif TARGET_OS_OSX
    LOG_DEBUG(LogCategory::Render, "Metal: Platform: macOS");
    if (m_impl->state.device.hasUnifiedMemory) {
        LOG_DEBUG(LogCategory::Render, "Metal: Apple Silicon detected (unified memory)");
    }
#endif

    return true;
}

void GfxDeviceMetal::shutdown() {
    LOG_DEBUG(LogCategory::Render, "Metal: Shutting down...");

    // Wait for all GPU work to complete
    waitIdle();

    // Destroy all resources
    for (auto& [id, mesh] : m_impl->meshes) {
        destroyBuffer(mesh.vertexBuffer);
        destroyBuffer(mesh.indexBuffer);
    }
    m_impl->meshes.clear();

    for (auto& [id, font] : m_impl->fontAtlases) {
        destroyTexture(font.texture);
    }
    m_impl->fontAtlases.clear();

    // Clear swapchains
    for (auto& [id, swapchain] : m_impl->state.swapchains) {
        if (swapchain.backbuffer.isValid()) {
            m_impl->state.renderTargets.erase(swapchain.backbuffer.id);
        }
    }
    m_impl->state.swapchains.clear();

    // Clear all other resources
    m_impl->state.buffers.clear();
    m_impl->state.textures.clear();
    m_impl->state.samplers.clear();
    m_impl->state.shaders.clear();
    m_impl->state.pipelines.clear();
    m_impl->state.computePipelines.clear();
    m_impl->state.renderTargets.clear();
    m_impl->state.uniformBuffers.clear();

    // Destroy dummy resources and flush deferred destructions
    m_impl->state.destroyDummyResources();
    m_impl->state.flushDeferredDestructions();

    LOG_DEBUG(LogCategory::Render, "Metal: Shutdown complete");
}

const GfxDeviceCaps& GfxDeviceMetal::getCapabilities() const {
    return m_impl->caps;
}

GraphicsBackend GfxDeviceMetal::getBackend() const {
    return GraphicsBackend::Metal;
}

void GfxDeviceMetal::setDefaultTextureFiltering(FilterMode minFilter, FilterMode magFilter) {
    m_impl->defaultMinFilter = minFilter;
    m_impl->defaultMagFilter = magFilter;
}

// ============================================================================
// Swapchain Management
// ============================================================================

SwapchainHandle GfxDeviceMetal::createSwapchain(const SwapchainDesc& desc) {
    LOG_INFO(LogCategory::Render, "Metal: Creating swapchain {}x{}", desc.width, desc.height);
    Logger::Flush();
    LOG_DEBUG(LogCategory::Render, "Metal: platformHandle={}", desc.window.platformHandle);
    Logger::Flush();

    uint32_t id = m_impl->state.nextSwapchainID++;
    MetalSwapchain swapchain;
    swapchain.window = desc.window;
    swapchain.width = desc.width;
    swapchain.height = desc.height;
    swapchain.vsync = desc.vsync;

    LOG_DEBUG(LogCategory::Render, "Metal: Processing texture format");

    // Metal's native swapchain format is BGRA, not RGBA
    // Convert RGBA formats to BGRA equivalents for optimal performance
    TextureFormat effectiveFormat = desc.colorFormat;
    if (effectiveFormat == TextureFormat::RGBA8_UNORM) {
        effectiveFormat = TextureFormat::BGRA8_UNORM;
    } else if (effectiveFormat == TextureFormat::RGBA8_SRGB) {
        effectiveFormat = TextureFormat::BGRA8_SRGB;
    }
    swapchain.colorFormat = effectiveFormat;
    swapchain.mtlColorFormat = MetalUtils::toMTLPixelFormat(effectiveFormat);

    LOG_DEBUG(LogCategory::Render, "Metal: About to process platformHandle for view extraction");

    // Get platform view from the window handle and set up CAMetalLayer
#if TARGET_OS_OSX
    // The platformHandle can be either an NSView* (from PyQt winId()) or an NSWindow* (from SDL2)
    // We need to detect which one it is and get the NSView accordingly
    NSView* view = nil;

    LOG_INFO(LogCategory::Render, "Metal: platformHandle raw pointer = {}", desc.window.platformHandle);

    if (!desc.window.platformHandle) {
        LOG_ERROR(LogCategory::Render, "Metal: Invalid window handle (null pointer)");
        fprintf(stderr, "Metal: platformHandle is NULL!\n");
        fflush(stderr);
        return SwapchainHandle();
    }

    fprintf(stderr, "Metal: platformHandle is not null, attempting isKindOfClass check\n");
    fflush(stderr);

    // Use @try/@catch for Objective-C exception safety
    @try {
        // First, try to treat it as an NSWindow (SDL2 case)
        // We cast to NSWindow first and check if it responds to contentView
        NSWindow* possibleWindow = (__bridge NSWindow*)desc.window.platformHandle;
        fprintf(stderr, "Metal: Cast to NSWindow done, checking isKindOfClass\n");
        fflush(stderr);

        if ([possibleWindow isKindOfClass:[NSWindow class]]) {
            // SDL2 passes NSWindow - get its contentView
            fprintf(stderr, "Metal: It's an NSWindow, getting contentView\n");
            fflush(stderr);
            view = [possibleWindow contentView];
            LOG_INFO(LogCategory::Render, "Metal: Received NSWindow, using contentView");
        } else {
            fprintf(stderr, "Metal: Not an NSWindow, trying NSView\n");
            fflush(stderr);
            // Try as NSView (PyQt case)
            NSView* possibleView = (__bridge NSView*)desc.window.platformHandle;
            if ([possibleView isKindOfClass:[NSView class]]) {
                view = possibleView;
                LOG_INFO(LogCategory::Render, "Metal: Received NSView directly");
            } else {
                LOG_ERROR(LogCategory::Render, "Metal: Invalid window handle - not NSWindow or NSView");
                fprintf(stderr, "Metal: Neither NSWindow nor NSView!\n");
                fflush(stderr);
                return SwapchainHandle();
            }
        }
    } @catch (NSException* exception) {
        LOG_ERROR(LogCategory::Render, "Metal: Exception while processing window handle: {}",
                  [[exception reason] UTF8String]);
        fprintf(stderr, "Metal: Caught NSException: %s\n", [[exception reason] UTF8String]);
        fflush(stderr);
        return SwapchainHandle();
    }

    fprintf(stderr, "Metal: Got view pointer: %p\n", (__bridge void*)view);
    fflush(stderr);

    if (!view) {
        LOG_ERROR(LogCategory::Render, "Metal: Could not get NSView from window handle");
        return SwapchainHandle();
    }

    // Get content scale factor for Retina displays
    CGFloat scaleFactor = 1.0;
    if (view.window) {
        scaleFactor = view.window.backingScaleFactor;
    } else {
        scaleFactor = [[NSScreen mainScreen] backingScaleFactor];
    }

    LOG_INFO(LogCategory::Render, "Metal: NSView pointer={}, frame={}x{}, isHidden={}, scaleFactor={}",
             (__bridge void*)view, view.frame.size.width, view.frame.size.height, view.isHidden, scaleFactor);

    // Log view class for debugging PyQt integration
    LOG_INFO(LogCategory::Render, "Metal: NSView class={}", [NSStringFromClass([view class]) UTF8String]);
    if (view.superview) {
        LOG_INFO(LogCategory::Render, "Metal: Superview class={}", [NSStringFromClass([view.superview class]) UTF8String]);
    }
    if (view.window) {
        LOG_INFO(LogCategory::Render, "Metal: Window class={}", [NSStringFromClass([view.window class]) UTF8String]);
    }

    // Check if view already has a CAMetalLayer
    if ([view.layer isKindOfClass:[CAMetalLayer class]]) {
        LOG_INFO(LogCategory::Render, "Metal: View already has a CAMetalLayer, reusing it");
        CAMetalLayer* existingLayer = (CAMetalLayer*)view.layer;
        existingLayer.device = m_impl->state.device;
        existingLayer.pixelFormat = swapchain.mtlColorFormat;
        existingLayer.framebufferOnly = YES;
        existingLayer.drawableSize = CGSizeMake(desc.width, desc.height);
        existingLayer.contentsScale = scaleFactor;
        existingLayer.displaySyncEnabled = desc.vsync;
        swapchain.metalLayer = existingLayer;
    } else {
        // Create and configure CAMetalLayer for macOS
        CAMetalLayer* metalLayer = [CAMetalLayer layer];
        metalLayer.device = m_impl->state.device;
        metalLayer.pixelFormat = swapchain.mtlColorFormat;
        metalLayer.framebufferOnly = YES;
        metalLayer.drawableSize = CGSizeMake(desc.width, desc.height);
        metalLayer.contentsScale = scaleFactor;
        metalLayer.displaySyncEnabled = desc.vsync;

        // For PyQt/Qt embedding, we need to make the view layer-backed first
        // and then set our layer as a sublayer or replace the layer
        view.wantsLayer = YES;

        // Check if we can replace the layer (layer-hosting view)
        // or if we need to add as sublayer (layer-backed view)
        if (view.layer) {
            // View already has a layer from Qt - add our Metal layer as sublayer
            metalLayer.frame = view.layer.bounds;
            metalLayer.autoresizingMask = kCALayerWidthSizable | kCALayerHeightSizable;

            // IMPORTANT: Set zPosition to bring Metal layer to front
            metalLayer.zPosition = 1000;

            // Make the Metal layer opaque so it's fully visible
            metalLayer.opaque = YES;

            [view.layer addSublayer:metalLayer];

            LOG_INFO(LogCategory::Render, "Metal: Added CAMetalLayer as sublayer of existing layer (class={})",
                     [NSStringFromClass([view.layer class]) UTF8String]);
            LOG_INFO(LogCategory::Render, "Metal: CAMetalLayer frame=({},{} {}x{}), bounds=({},{} {}x{}), zPosition={}",
                     metalLayer.frame.origin.x, metalLayer.frame.origin.y,
                     metalLayer.frame.size.width, metalLayer.frame.size.height,
                     metalLayer.bounds.origin.x, metalLayer.bounds.origin.y,
                     metalLayer.bounds.size.width, metalLayer.bounds.size.height,
                     metalLayer.zPosition);
            LOG_INFO(LogCategory::Render, "Metal: Parent layer bounds=({},{} {}x{}), sublayers count={}",
                     view.layer.bounds.origin.x, view.layer.bounds.origin.y,
                     view.layer.bounds.size.width, view.layer.bounds.size.height,
                     (unsigned long)view.layer.sublayers.count);
        } else {
            // No existing layer - set directly
            view.layer = metalLayer;
            LOG_INFO(LogCategory::Render, "Metal: Set CAMetalLayer as view's layer directly");
        }

        swapchain.metalLayer = metalLayer;
        LOG_INFO(LogCategory::Render, "Metal: Created new CAMetalLayer for view");
    }
#elif TARGET_OS_IOS || TARGET_OS_TV
    UIView* view = (__bridge UIView*)desc.window.platformHandle;
    if (!view) {
        LOG_ERROR(LogCategory::Render, "Metal: Invalid window handle (UIView)");
        return SwapchainHandle();
    }

    // Create and configure CAMetalLayer for iOS/tvOS
    CAMetalLayer* metalLayer = [CAMetalLayer layer];
    metalLayer.device = m_impl->state.device;
    metalLayer.pixelFormat = swapchain.mtlColorFormat;
    metalLayer.framebufferOnly = YES;
    metalLayer.drawableSize = CGSizeMake(desc.width, desc.height);

    // Scale factor for Retina displays
    metalLayer.contentsScale = view.contentScaleFactor;

    // Set the layer on the view
    view.layer = metalLayer;
    swapchain.metalLayer = metalLayer;
#else
    LOG_ERROR(LogCategory::Render, "Metal: Unsupported platform");
    return SwapchainHandle();
#endif

    // Create depth texture with optimal storage mode for platform
    MTLTextureDescriptor* depthDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float_Stencil8
                                                                                         width:desc.width
                                                                                        height:desc.height
                                                                                     mipmapped:NO];
    depthDesc.usage = MTLTextureUsageRenderTarget;

#if TARGET_OS_IOS || TARGET_OS_TV
    // iOS/tvOS: Use memoryless storage for depth buffer (TBDR optimization)
    // Depth data is discarded after the render pass, saving memory bandwidth
    depthDesc.storageMode = MTLStorageModeMemoryless;
#else
    // macOS: Use private storage for GPU-only depth buffer
    depthDesc.storageMode = MTLStorageModePrivate;
#endif

    swapchain.depthTexture = [m_impl->state.device newTextureWithDescriptor:depthDesc];

    // Create associated render target
    uint32_t rtId = m_impl->state.nextRenderTargetID++;
    MetalRenderTarget rt;
    rt.width = desc.width;
    rt.height = desc.height;
    rt.colorFormat = desc.colorFormat;
    rt.depthFormat = TextureFormat::DEPTH32F_STENCIL8;
    rt.mtlColorFormat = swapchain.mtlColorFormat;
    rt.mtlDepthFormat = MTLPixelFormatDepth32Float_Stencil8;
    rt.hasColor = true;
    rt.hasDepth = true;
    rt.hasStencil = true;
    rt.isSwapchainBackbuffer = true;
    rt.owningSwapchainId = id;
    rt.depthTexture = swapchain.depthTexture;

#if TARGET_OS_IOS || TARGET_OS_TV
    // iOS/tvOS: Use DontCare store action for memoryless depth
    // This tells the GPU it doesn't need to write depth data back to memory
    rt.depthStoreAction = MTLStoreActionDontCare;
    rt.useMemorylessDepth = true;
#else
    rt.depthStoreAction = MTLStoreActionStore;
    rt.useMemorylessDepth = false;
#endif
    // colorTexture will be set per-frame from drawable

    m_impl->state.renderTargets[rtId] = rt;
    swapchain.backbuffer = RenderTargetHandle(rtId);

    m_impl->state.swapchains[id] = swapchain;

    LOG_DEBUG(LogCategory::Render, "Metal: Created swapchain {}", id);
    return SwapchainHandle(id);
}

void GfxDeviceMetal::destroySwapchain(SwapchainHandle swapchain) {
    auto it = m_impl->state.swapchains.find(swapchain.id);
    if (it == m_impl->state.swapchains.end()) return;

    waitIdle();

    // Destroy associated render target
    if (it->second.backbuffer.isValid()) {
        m_impl->state.renderTargets.erase(it->second.backbuffer.id);
    }

    m_impl->state.swapchains.erase(it);
    LOG_DEBUG(LogCategory::Render, "Metal: Destroyed swapchain {}", swapchain.id);
}

void GfxDeviceMetal::resizeSwapchain(SwapchainHandle swapchain, uint32_t width, uint32_t height) {
    auto it = m_impl->state.swapchains.find(swapchain.id);
    if (it == m_impl->state.swapchains.end()) return;

    MetalSwapchain& sc = it->second;
    if (sc.width == width && sc.height == height) return;

    LOG_DEBUG(LogCategory::Render, "Metal: Resizing swapchain {} to {}x{}", swapchain.id, width, height);

    waitIdle();

    sc.width = width;
    sc.height = height;
    sc.metalLayer.drawableSize = CGSizeMake(width, height);

    // If our layer is a sublayer (added via addSublayer), update its frame
    // Note: autoresizingMask handles this automatically in most cases, but we
    // explicitly set the frame for immediate effect
    if (sc.metalLayer.superlayer) {
        sc.metalLayer.frame = sc.metalLayer.superlayer.bounds;
    }

    // Recreate depth texture
    MTLTextureDescriptor* depthDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float_Stencil8
                                                                                         width:width
                                                                                        height:height
                                                                                     mipmapped:NO];
    depthDesc.usage = MTLTextureUsageRenderTarget;
    depthDesc.storageMode = MTLStorageModePrivate;
    sc.depthTexture = [m_impl->state.device newTextureWithDescriptor:depthDesc];

    // Update render target
    auto rtIt = m_impl->state.renderTargets.find(sc.backbuffer.id);
    if (rtIt != m_impl->state.renderTargets.end()) {
        rtIt->second.width = width;
        rtIt->second.height = height;
        rtIt->second.depthTexture = sc.depthTexture;
    }
}

void GfxDeviceMetal::present(SwapchainHandle swapchain) {
    auto it = m_impl->state.swapchains.find(swapchain.id);
    if (it == m_impl->state.swapchains.end()) return;

    MetalSwapchain& sc = it->second;
    if (sc.currentDrawable) {
        // Create a minimal command buffer just to present the drawable
        id<MTLCommandBuffer> presentCmdBuffer = [m_impl->state.commandQueue commandBuffer];
        if (presentCmdBuffer) {
            [presentCmdBuffer presentDrawable:sc.currentDrawable];

            // Add completion handler for frame semaphore
            __block dispatch_semaphore_t sema = m_impl->state.frameSemaphore;
            [presentCmdBuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer) {
                if (buffer.status == MTLCommandBufferStatusError) {
                    NSError* error = buffer.error;
                    NSLog(@"Metal: Present command buffer error: %@", error.localizedDescription);
                }
                dispatch_semaphore_signal(sema);
            }];

            [presentCmdBuffer commit];
            LOG_DEBUG(LogCategory::Render, "Metal: present() - drawable presented and semaphore handler added");
        } else {
            LOG_ERROR(LogCategory::Render, "Metal: present() - failed to create present command buffer");
            // Still need to signal semaphore to prevent deadlock
            dispatch_semaphore_signal(m_impl->state.frameSemaphore);
        }

        sc.currentDrawable = nil;
    } else {
        LOG_WARN(LogCategory::Render, "Metal: present() called but no drawable to present");
    }

    // Process deferred destructions
    m_impl->state.processDeferredDestructions();
    m_impl->state.currentFrameIndex = (m_impl->state.currentFrameIndex + 1) % MetalState::MAX_FRAMES_IN_FLIGHT;
}

RenderTargetHandle GfxDeviceMetal::getSwapchainBackbuffer(SwapchainHandle swapchain) {
    auto it = m_impl->state.swapchains.find(swapchain.id);
    if (it == m_impl->state.swapchains.end()) return RenderTargetHandle();

    return it->second.backbuffer;
}

void GfxDeviceMetal::setSwapchainHintForOffscreen(SwapchainHandle swapchain) {
    m_impl->state.hintedSwapchain = swapchain;
}

// ============================================================================
// Texture Management
// ============================================================================

TextureHandle GfxDeviceMetal::createTexture(const TextureDesc& desc) {
    uint32_t id = m_impl->state.nextTextureID++;
    MetalTexture texture;
    texture.width = desc.width;
    texture.height = desc.height;
    texture.depth = desc.depth;
    texture.mipLevels = desc.mipLevels;
    texture.arrayLayers = desc.arrayLayers;
    texture.format = desc.format;
    texture.type = desc.type;
    texture.mtlFormat = MetalUtils::toMTLPixelFormat(desc.format);

    // Create texture descriptor
    MTLTextureDescriptor* texDesc = [[MTLTextureDescriptor alloc] init];

    switch (desc.type) {
        case TextureType::Texture2D:
            texDesc.textureType = MTLTextureType2D;
            break;
        case TextureType::Texture3D:
            texDesc.textureType = MTLTextureType3D;
            break;
        case TextureType::TextureCube:
            texDesc.textureType = MTLTextureTypeCube;
            break;
        case TextureType::Texture2DArray:
            texDesc.textureType = MTLTextureType2DArray;
            break;
        case TextureType::TextureCubeArray:
            texDesc.textureType = MTLTextureTypeCubeArray;
            break;
    }

    texDesc.pixelFormat = texture.mtlFormat;
    texDesc.width = desc.width;
    texDesc.height = desc.height;
    texDesc.depth = desc.depth;
    texDesc.mipmapLevelCount = desc.mipLevels;
    texDesc.arrayLength = (desc.type == TextureType::TextureCube || desc.type == TextureType::TextureCubeArray) ? 1 : desc.arrayLayers;

    // Set usage flags
    MTLTextureUsage usage = 0;
    if ((desc.usage & TextureUsage::Sampled) != TextureUsage::None) {
        usage |= MTLTextureUsageShaderRead;
    }
    if ((desc.usage & TextureUsage::Storage) != TextureUsage::None) {
        usage |= MTLTextureUsageShaderWrite;
    }
    if ((desc.usage & TextureUsage::RenderTarget) != TextureUsage::None ||
        (desc.usage & TextureUsage::DepthStencil) != TextureUsage::None) {
        usage |= MTLTextureUsageRenderTarget;
    }
    if (usage == 0) {
        usage = MTLTextureUsageShaderRead; // Default
    }
    texDesc.usage = usage;

    // Storage mode. Shared (CPU accessible) is required whenever the texture's
    // pixels are uploaded from the CPU via replaceRegion - both the initial upload
    // and the per-level uploads used to fill a mip chain (desc.mipLevels > 1).
    // GPU-only resources (render targets, mip-free textures with no initial data)
    // stay Private.
    if (desc.initialData || desc.mipLevels > 1) {
        texDesc.storageMode = MTLStorageModeShared;
    } else {
        texDesc.storageMode = MTLStorageModePrivate; // GPU only
    }

    texture.texture = [m_impl->state.device newTextureWithDescriptor:texDesc];
    if (!texture.texture) {
        LOG_ERROR(LogCategory::Render, "Metal: Failed to create texture");
        return TextureHandle();
    }

    // Upload initial data if provided
    if (desc.initialData) {
        uint32_t bytesPerPixel = MetalUtils::getBytesPerPixel(texture.mtlFormat);
        uint32_t bytesPerRow = desc.width * bytesPerPixel;

        MTLRegion region = MTLRegionMake2D(0, 0, desc.width, desc.height);
        [texture.texture replaceRegion:region mipmapLevel:0 withBytes:desc.initialData bytesPerRow:bytesPerRow];
    }

    m_impl->state.textures[id] = texture;
    return TextureHandle(id);
}

void GfxDeviceMetal::destroyTexture(TextureHandle texture) {
    auto it = m_impl->state.textures.find(texture.id);
    if (it == m_impl->state.textures.end()) return;

    // Don't destroy swapchain images
    if (!it->second.isSwapchainImage) {
        m_impl->state.deferTextureDestroy(it->second);
    }
    m_impl->state.textures.erase(it);
}

void GfxDeviceMetal::updateTexture(TextureHandle texture, const void* data, uint32_t mipLevel, uint32_t arrayLayer) {
    auto it = m_impl->state.textures.find(texture.id);
    if (it == m_impl->state.textures.end() || !data) return;

    MetalTexture& tex = it->second;
    uint32_t mipWidth = std::max(1u, tex.width >> mipLevel);
    uint32_t mipHeight = std::max(1u, tex.height >> mipLevel);
    uint32_t bytesPerPixel = MetalUtils::getBytesPerPixel(tex.mtlFormat);
    uint32_t bytesPerRow = mipWidth * bytesPerPixel;

    MTLRegion region = MTLRegionMake2D(0, 0, mipWidth, mipHeight);
    [tex.texture replaceRegion:region mipmapLevel:mipLevel slice:arrayLayer withBytes:data bytesPerRow:bytesPerRow bytesPerImage:0];
}

// ============================================================================
// Buffer Management
// ============================================================================

BufferHandle GfxDeviceMetal::createBuffer(const BufferDesc& desc) {
    uint32_t id = m_impl->state.nextBufferID++;
    MetalBuffer buffer;
    buffer.size = desc.size;
    buffer.usage = desc.usage;

    // Determine storage mode
    // For buffers that need CPU updates (uniforms, staging), use shared mode
    // For vertex/index buffers with initial data only, we can use private
    buffer.resourceOptions = MTLResourceStorageModeShared;

    if (desc.initialData) {
        buffer.buffer = [m_impl->state.device newBufferWithBytes:desc.initialData
                                                          length:desc.size
                                                         options:buffer.resourceOptions];
    } else {
        buffer.buffer = [m_impl->state.device newBufferWithLength:desc.size
                                                          options:buffer.resourceOptions];
    }

    if (!buffer.buffer) {
        LOG_ERROR(LogCategory::Render, "Metal: Failed to create buffer");
        return BufferHandle();
    }

    m_impl->state.buffers[id] = buffer;
    return BufferHandle(id);
}

void GfxDeviceMetal::destroyBuffer(BufferHandle buffer) {
    auto it = m_impl->state.buffers.find(buffer.id);
    if (it == m_impl->state.buffers.end()) return;

    m_impl->state.deferBufferDestroy(it->second);
    m_impl->state.buffers.erase(it);
}

void GfxDeviceMetal::updateBuffer(BufferHandle buffer, const void* data, uint64_t size, uint64_t offset) {
    auto it = m_impl->state.buffers.find(buffer.id);
    if (it == m_impl->state.buffers.end() || !data) return;

    MetalBuffer& buf = it->second;
    if (offset + size > buf.size) {
        LOG_ERROR(LogCategory::Render, "Metal: Buffer update exceeds buffer size");
        return;
    }

    // Metal shared buffers are directly accessible
    void* contents = buf.buffer.contents;
    memcpy(static_cast<uint8_t*>(contents) + offset, data, size);

    // Notify Metal of the modification (for managed buffers)
    [buf.buffer didModifyRange:NSMakeRange(offset, size)];
}

// ============================================================================
// Sampler Management
// ============================================================================

SamplerHandle GfxDeviceMetal::createSampler(const SamplerDesc& desc) {
    uint32_t id = m_impl->state.nextSamplerID++;
    MetalSampler sampler;
    sampler.desc = desc;

    MTLSamplerDescriptor* samplerDesc = [[MTLSamplerDescriptor alloc] init];
    samplerDesc.minFilter = MetalUtils::toMTLFilter(desc.minFilter);
    samplerDesc.magFilter = MetalUtils::toMTLFilter(desc.magFilter);
    samplerDesc.mipFilter = MetalUtils::toMTLMipFilter(desc.mipFilter);
    samplerDesc.sAddressMode = MetalUtils::toMTLAddressMode(desc.wrapU);
    samplerDesc.tAddressMode = MetalUtils::toMTLAddressMode(desc.wrapV);
    samplerDesc.rAddressMode = MetalUtils::toMTLAddressMode(desc.wrapW);
    samplerDesc.maxAnisotropy = static_cast<NSUInteger>(desc.maxAnisotropy);

    if (desc.compareEnable) {
        samplerDesc.compareFunction = MetalUtils::toMTLCompareFunction(desc.compareFunc);
    }

    sampler.sampler = [m_impl->state.device newSamplerStateWithDescriptor:samplerDesc];
    if (!sampler.sampler) {
        LOG_ERROR(LogCategory::Render, "Metal: Failed to create sampler");
        return SamplerHandle();
    }

    m_impl->state.samplers[id] = sampler;
    return SamplerHandle(id);
}

void GfxDeviceMetal::destroySampler(SamplerHandle sampler) {
    m_impl->state.samplers.erase(sampler.id);
}

// ============================================================================
// Shader Management
// ============================================================================

ShaderHandle GfxDeviceMetal::createShader(const ShaderDesc& desc) {
    uint32_t id = m_impl->state.nextShaderID++;
    MetalShader shader;
    shader.stage = desc.stage;
    shader.entryPoint = desc.entryPoint;

    // The bytecode should be MSL source code for Metal
    if (!desc.bytecode || desc.bytecodeSize == 0) {
        LOG_ERROR(LogCategory::Render, "Metal: No shader source provided");
        return ShaderHandle();
    }

    // Store source code
    shader.sourceCode.resize(desc.bytecodeSize);
    memcpy(shader.sourceCode.data(), desc.bytecode, desc.bytecodeSize);

    // Create NSString from source
    NSString* source = [[NSString alloc] initWithBytes:desc.bytecode
                                                length:desc.bytecodeSize
                                              encoding:NSUTF8StringEncoding];
    if (!source) {
        LOG_ERROR(LogCategory::Render, "Metal: Invalid shader source encoding");
        return ShaderHandle();
    }

    // Compile the shader
    NSError* error = nil;
    MTLCompileOptions* options = [[MTLCompileOptions alloc] init];
    shader.library = [m_impl->state.device newLibraryWithSource:source options:options error:&error];

    if (!shader.library) {
        NSString* errorStr = error ? error.localizedDescription : @"Unknown error";
        LOG_ERROR(LogCategory::Render, "Metal: Shader compilation failed: {}", [errorStr UTF8String]);
        return ShaderHandle();
    }

    // Get the function
    NSString* funcName = [NSString stringWithUTF8String:desc.entryPoint.c_str()];
    shader.function = [shader.library newFunctionWithName:funcName];

    if (!shader.function) {
        LOG_ERROR(LogCategory::Render, "Metal: Function '{}' not found in shader", desc.entryPoint);
        return ShaderHandle();
    }

    m_impl->state.shaders[id] = shader;
    LOG_DEBUG(LogCategory::Render, "Metal: Created shader {} with entry point '{}'", id, desc.entryPoint);
    return ShaderHandle(id);
}

void GfxDeviceMetal::destroyShader(ShaderHandle shader) {
    auto it = m_impl->state.shaders.find(shader.id);
    if (it == m_impl->state.shaders.end()) return;

    m_impl->state.deferShaderDestroy(it->second);
    m_impl->state.shaders.erase(it);
}

// ============================================================================
// Pipeline Management
// ============================================================================

PipelineHandle GfxDeviceMetal::createPipeline(const PipelineDesc& desc) {
    uint32_t id = m_impl->state.nextPipelineID++;
    MetalPipeline pipeline;
    pipeline.shaders = desc.shaders;
    pipeline.vertexLayout = desc.vertexLayout;
    pipeline.topology = desc.topology;
    pipeline.mtlPrimitiveType = MetalUtils::toMTLPrimitiveType(desc.topology);
    pipeline.blendStateDesc = desc.blendState;
    pipeline.depthStencilStateDesc = desc.depthStencilState;
    pipeline.rasterizerStateDesc = desc.rasterizerState;

    // Create render pipeline descriptor
    MTLRenderPipelineDescriptor* pipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];

    // Set shaders
    for (const auto& shaderHandle : desc.shaders) {
        auto shaderIt = m_impl->state.shaders.find(shaderHandle.id);
        if (shaderIt == m_impl->state.shaders.end()) {
            LOG_ERROR(LogCategory::Render, "Metal: Invalid shader handle in pipeline");
            return PipelineHandle();
        }

        const MetalShader& shader = shaderIt->second;
        switch (shader.stage) {
            case ShaderStage::Vertex:
                pipelineDesc.vertexFunction = shader.function;
                break;
            case ShaderStage::Fragment:
                pipelineDesc.fragmentFunction = shader.function;
                break;
            default:
                break;
        }
    }

    // Set vertex descriptor
    MTLVertexDescriptor* vertexDesc = [[MTLVertexDescriptor alloc] init];
    for (const auto& attr : desc.vertexLayout.attributes) {
        vertexDesc.attributes[attr.location].format = MetalUtils::toMTLVertexFormat(attr.format);
        vertexDesc.attributes[attr.location].offset = attr.offset;
        vertexDesc.attributes[attr.location].bufferIndex = attr.binding;
    }
    // Configure buffer layout - buffer index 0 for vertex data
    vertexDesc.layouts[0].stride = desc.vertexLayout.stride;
    vertexDesc.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
    vertexDesc.layouts[0].stepRate = 1;

    // Additional per-binding vertex buffers (e.g. binding 1 = per-instance data
    // for GPU instancing). Buffer index equals the binding, matching the command
    // list's setVertexBuffer:offset:atIndex:binding convention. Instance-rate
    // layouts advance once per instance via MTLVertexStepFunctionPerInstance.
    for (const auto& extraLayout : desc.extraVertexBuffers) {
        for (const auto& attr : extraLayout.attributes) {
            vertexDesc.attributes[attr.location].format = MetalUtils::toMTLVertexFormat(attr.format);
            vertexDesc.attributes[attr.location].offset = attr.offset;
            vertexDesc.attributes[attr.location].bufferIndex = attr.binding;
        }
        if (!extraLayout.attributes.empty()) {
            const uint32_t bufferIndex = extraLayout.attributes.front().binding;
            vertexDesc.layouts[bufferIndex].stride = extraLayout.stride;
            vertexDesc.layouts[bufferIndex].stepFunction =
                (extraLayout.inputRate == VertexInputRate::Instance)
                    ? MTLVertexStepFunctionPerInstance
                    : MTLVertexStepFunctionPerVertex;
            vertexDesc.layouts[bufferIndex].stepRate = 1;
        }
    }
    pipelineDesc.vertexDescriptor = vertexDesc;

    // Set color attachment
    // On Metal, RGBA8 formats need to be BGRA8 for the native swapchain format
    // If the pipeline specifies RGBA8_UNORM or RGBA8_SRGB, convert to BGRA8 equivalent
    TextureFormat effectiveColorFormat = desc.colorFormat;
    if (effectiveColorFormat == TextureFormat::RGBA8_UNORM) {
        effectiveColorFormat = TextureFormat::BGRA8_UNORM;
    } else if (effectiveColorFormat == TextureFormat::RGBA8_SRGB) {
        effectiveColorFormat = TextureFormat::BGRA8_SRGB;
    }
    MTLPixelFormat mtlColorFormat = MetalUtils::toMTLPixelFormat(effectiveColorFormat);
    pipelineDesc.colorAttachments[0].pixelFormat = mtlColorFormat;

    // DEBUG: Log the pipeline color format
    LOG_DEBUG(LogCategory::Render, "Metal: createPipeline colorFormat={} (mtl={})",
              static_cast<int>(effectiveColorFormat), static_cast<int>(mtlColorFormat));

    // Set blend state
    MTLRenderPipelineColorAttachmentDescriptor* colorAttach = pipelineDesc.colorAttachments[0];
    colorAttach.blendingEnabled = desc.blendState.blendEnable;
    if (desc.blendState.blendEnable) {
        colorAttach.sourceRGBBlendFactor = MetalUtils::toMTLBlendFactor(desc.blendState.srcColorBlend);
        colorAttach.destinationRGBBlendFactor = MetalUtils::toMTLBlendFactor(desc.blendState.dstColorBlend);
        colorAttach.rgbBlendOperation = MetalUtils::toMTLBlendOperation(desc.blendState.colorBlendOp);
        colorAttach.sourceAlphaBlendFactor = MetalUtils::toMTLBlendFactor(desc.blendState.srcAlphaBlend);
        colorAttach.destinationAlphaBlendFactor = MetalUtils::toMTLBlendFactor(desc.blendState.dstAlphaBlend);
        colorAttach.alphaBlendOperation = MetalUtils::toMTLBlendOperation(desc.blendState.alphaBlendOp);
    }

    // Set depth format
    pipelineDesc.depthAttachmentPixelFormat = MetalUtils::toMTLPixelFormat(desc.depthFormat);
    if (MetalUtils::hasStencilComponent(MetalUtils::toMTLPixelFormat(desc.depthFormat))) {
        pipelineDesc.stencilAttachmentPixelFormat = MetalUtils::toMTLPixelFormat(desc.depthFormat);
    }

    // Create pipeline state with reflection
    NSError* error = nil;
    MTLRenderPipelineReflection* reflection = nil;
    pipeline.pipelineState = [m_impl->state.device newRenderPipelineStateWithDescriptor:pipelineDesc
                                                                                options:MTLPipelineOptionArgumentInfo
                                                                             reflection:&reflection
                                                                                  error:&error];
    if (!pipeline.pipelineState) {
        NSString* errorStr = error ? error.localizedDescription : @"Unknown error";
        LOG_ERROR(LogCategory::Render, "Metal: Pipeline creation failed: {}", [errorStr UTF8String]);
        return PipelineHandle();
    }

    // Extract uniform reflection data
    if (reflection) {
        MetalUtils::extractPipelineReflection(pipeline, reflection);
    }

    // Create depth-stencil state
    MTLDepthStencilDescriptor* dsDesc = [[MTLDepthStencilDescriptor alloc] init];
    dsDesc.depthCompareFunction = desc.depthStencilState.depthTestEnable ?
        MetalUtils::toMTLCompareFunction(desc.depthStencilState.depthCompareFunc) :
        MTLCompareFunctionAlways;
    dsDesc.depthWriteEnabled = desc.depthStencilState.depthWriteEnable;
    pipeline.depthStencilState = [m_impl->state.device newDepthStencilStateWithDescriptor:dsDesc];

    // Store rasterizer state (applied per-encoder in Metal)
    pipeline.cullMode = MetalUtils::toMTLCullMode(desc.rasterizerState.cullMode);
    pipeline.frontFace = MetalUtils::toMTLWinding(desc.rasterizerState.frontFace);
    pipeline.fillMode = MetalUtils::toMTLFillMode(desc.rasterizerState.fillMode);
    pipeline.depthBiasEnable = desc.rasterizerState.depthBiasEnable;
    pipeline.depthBiasConstant = desc.rasterizerState.depthBiasConstantFactor;
    pipeline.depthBiasSlope = desc.rasterizerState.depthBiasSlopeFactor;
    pipeline.depthBiasClamp = desc.rasterizerState.depthBiasClamp;

    m_impl->state.pipelines[id] = pipeline;
    LOG_DEBUG(LogCategory::Render, "Metal: Created pipeline {}", id);
    return PipelineHandle(id);
}

void GfxDeviceMetal::destroyPipeline(PipelineHandle pipeline) {
    auto it = m_impl->state.pipelines.find(pipeline.id);
    if (it == m_impl->state.pipelines.end()) return;

    m_impl->state.deferPipelineDestroy(it->second);
    m_impl->state.pipelines.erase(it);
}

// ============================================================================
// Compute Pipeline Management
// ============================================================================

uint32_t GfxDeviceMetal::createComputePipeline(ShaderHandle computeShader,
                                                uint32_t threadGroupSizeX,
                                                uint32_t threadGroupSizeY,
                                                uint32_t threadGroupSizeZ) {
    auto shaderIt = m_impl->state.shaders.find(computeShader.id);
    if (shaderIt == m_impl->state.shaders.end()) {
        LOG_ERROR(LogCategory::Render, "Metal: Invalid compute shader handle");
        return 0;
    }

    const MetalShader& shader = shaderIt->second;
    if (shader.stage != ShaderStage::Compute) {
        LOG_ERROR(LogCategory::Render, "Metal: Shader is not a compute shader");
        return 0;
    }

    uint32_t id = m_impl->state.nextComputePipelineID++;
    MetalComputePipeline pipeline;
    pipeline.shader = computeShader;
    pipeline.threadGroupSize = MTLSizeMake(threadGroupSizeX, threadGroupSizeY, threadGroupSizeZ);

    // Create compute pipeline state with reflection
    NSError* error = nil;
    MTLComputePipelineReflection* reflection = nil;
    pipeline.pipelineState = [m_impl->state.device newComputePipelineStateWithFunction:shader.function
                                                                               options:MTLPipelineOptionArgumentInfo
                                                                            reflection:&reflection
                                                                                 error:&error];
    if (!pipeline.pipelineState) {
        NSString* errorStr = error ? error.localizedDescription : @"Unknown error";
        LOG_ERROR(LogCategory::Render, "Metal: Compute pipeline creation failed: {}", [errorStr UTF8String]);
        return 0;
    }

    // Extract compute shader reflection data
    if (reflection) {
        for (MTLArgument* arg in reflection.arguments) {
            if (arg.type == MTLArgumentTypeBuffer && arg.bufferDataType != MTLDataTypeNone) {
                MTLStructType* structType = arg.bufferStructType;
                if (structType) {
                    for (MTLStructMember* member in structType.members) {
                        NSString* memberName = member.name;
                        std::string name = [memberName UTF8String];

                        uint32_t size = 16; // Default
                        switch (member.dataType) {
                            case MTLDataTypeFloat:  size = 4; break;
                            case MTLDataTypeFloat2: size = 8; break;
                            case MTLDataTypeFloat3: size = 12; break;
                            case MTLDataTypeFloat4: size = 16; break;
                            case MTLDataTypeInt:    size = 4; break;
                            case MTLDataTypeFloat4x4: size = 64; break;
                            default: break;
                        }

                        pipeline.uniformOffsets[name] = static_cast<uint32_t>(member.offset);
                        pipeline.uniformBufferSize = std::max(pipeline.uniformBufferSize,
                            static_cast<uint32_t>(member.offset + size));

                        LOG_DEBUG(LogCategory::Render, "Metal: Reflected compute uniform '{}' at offset {}",
                                  name, member.offset);
                    }
                }
            } else if (arg.type == MTLArgumentTypeTexture) {
                LOG_DEBUG(LogCategory::Render, "Metal: Reflected compute texture '{}' at index {}",
                          [arg.name UTF8String], arg.index);
            }
        }
        pipeline.uniformBufferSize = (pipeline.uniformBufferSize + 15) & ~15;
    }

    // Query pipeline properties
    pipeline.maxTotalThreadsPerThreadgroup = static_cast<uint32_t>(pipeline.pipelineState.maxTotalThreadsPerThreadgroup);

    // Validate thread group size
    NSUInteger totalThreads = threadGroupSizeX * threadGroupSizeY * threadGroupSizeZ;
    if (totalThreads > pipeline.maxTotalThreadsPerThreadgroup) {
        LOG_WARN(LogCategory::Render, "Metal: Requested thread group size ({}) exceeds max ({})",
                 totalThreads, pipeline.maxTotalThreadsPerThreadgroup);
        // Adjust to max
        pipeline.threadGroupSize = MTLSizeMake(
            std::min(threadGroupSizeX, static_cast<uint32_t>(pipeline.maxTotalThreadsPerThreadgroup)),
            1, 1
        );
    }

    m_impl->state.computePipelines[id] = pipeline;
    LOG_DEBUG(LogCategory::Render, "Metal: Created compute pipeline {} with thread group size {}x{}x{}",
              id, threadGroupSizeX, threadGroupSizeY, threadGroupSizeZ);
    return id;
}

void GfxDeviceMetal::destroyComputePipeline(uint32_t pipelineId) {
    auto it = m_impl->state.computePipelines.find(pipelineId);
    if (it == m_impl->state.computePipelines.end()) return;

    m_impl->state.deferComputePipelineDestroy(it->second);
    m_impl->state.computePipelines.erase(it);
}

// ============================================================================
// Render Target Management
// ============================================================================

RenderTargetHandle GfxDeviceMetal::createRenderTarget(const RenderTargetDesc& desc) {
    uint32_t id = m_impl->state.nextRenderTargetID++;
    MetalRenderTarget rt;
    rt.width = desc.width;
    rt.height = desc.height;
    rt.colorFormat = desc.colorFormat;
    rt.depthFormat = desc.depthFormat;
    rt.mtlColorFormat = MetalUtils::toMTLPixelFormat(desc.colorFormat);
    rt.mtlDepthFormat = MetalUtils::toMTLPixelFormatDepth(desc.depthFormat);
    rt.hasColor = desc.hasColor;
    rt.hasDepth = desc.hasDepth;
    rt.hasStencil = MetalUtils::hasStencilComponent(rt.mtlDepthFormat);
    rt.isCubeMap = desc.isCubeMap;
    rt.currentCubeFace = desc.cubeMapFace;

    // Create color texture
    if (desc.hasColor) {
        MTLTextureDescriptor* colorDesc;
        if (desc.isCubeMap) {
            colorDesc = [MTLTextureDescriptor textureCubeDescriptorWithPixelFormat:rt.mtlColorFormat
                                                                              size:desc.width
                                                                         mipmapped:NO];
        } else {
            colorDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:rt.mtlColorFormat
                                                                           width:desc.width
                                                                          height:desc.height
                                                                       mipmapped:NO];
        }
        colorDesc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
        colorDesc.storageMode = MTLStorageModePrivate;
        rt.colorTexture = [m_impl->state.device newTextureWithDescriptor:colorDesc];

        // Register as a texture resource
        uint32_t texId = m_impl->state.nextTextureID++;
        MetalTexture tex;
        tex.texture = rt.colorTexture;
        tex.width = desc.width;
        tex.height = desc.height;
        tex.format = desc.colorFormat;
        tex.type = desc.isCubeMap ? TextureType::TextureCube : TextureType::Texture2D;
        tex.mtlFormat = rt.mtlColorFormat;
        m_impl->state.textures[texId] = tex;
        rt.colorTextureHandle = TextureHandle(texId);
    }

    // Create depth texture
    if (desc.hasDepth) {
        MTLTextureDescriptor* depthDesc;
        if (desc.isCubeMap) {
            depthDesc = [MTLTextureDescriptor textureCubeDescriptorWithPixelFormat:rt.mtlDepthFormat
                                                                              size:desc.width
                                                                         mipmapped:NO];
        } else {
            depthDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:rt.mtlDepthFormat
                                                                           width:desc.width
                                                                          height:desc.height
                                                                       mipmapped:NO];
        }
        depthDesc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
        depthDesc.storageMode = MTLStorageModePrivate;
        rt.depthTexture = [m_impl->state.device newTextureWithDescriptor:depthDesc];

        // Register as a texture resource
        uint32_t texId = m_impl->state.nextTextureID++;
        MetalTexture tex;
        tex.texture = rt.depthTexture;
        tex.width = desc.width;
        tex.height = desc.height;
        tex.format = desc.depthFormat;
        tex.type = desc.isCubeMap ? TextureType::TextureCube : TextureType::Texture2D;
        tex.mtlFormat = rt.mtlDepthFormat;
        m_impl->state.textures[texId] = tex;
        rt.depthTextureHandle = TextureHandle(texId);

        // CRITICAL: Set depth store action to Store so depth data persists after rendering
        // This is essential for shadow maps which need to be sampled in subsequent passes
        rt.depthStoreAction = MTLStoreActionStore;
    }

    m_impl->state.renderTargets[id] = rt;
    LOG_DEBUG(LogCategory::Render, "Metal: Created render target {} ({}x{})", id, desc.width, desc.height);
    return RenderTargetHandle(id);
}

void GfxDeviceMetal::destroyRenderTarget(RenderTargetHandle target) {
    auto it = m_impl->state.renderTargets.find(target.id);
    if (it == m_impl->state.renderTargets.end()) return;

    // Don't destroy swapchain backbuffers here
    if (it->second.isSwapchainBackbuffer) return;

    // Destroy associated textures
    if (it->second.colorTextureHandle.isValid()) {
        destroyTexture(it->second.colorTextureHandle);
    }
    if (it->second.depthTextureHandle.isValid()) {
        destroyTexture(it->second.depthTextureHandle);
    }

    m_impl->state.renderTargets.erase(it);
}

TextureHandle GfxDeviceMetal::getRenderTargetColorTexture(RenderTargetHandle target) {
    auto it = m_impl->state.renderTargets.find(target.id);
    if (it == m_impl->state.renderTargets.end()) return TextureHandle();
    return it->second.colorTextureHandle;
}

TextureHandle GfxDeviceMetal::getRenderTargetDepthTexture(RenderTargetHandle target) {
    auto it = m_impl->state.renderTargets.find(target.id);
    if (it == m_impl->state.renderTargets.end()) return TextureHandle();
    return it->second.depthTextureHandle;
}

void GfxDeviceMetal::attachCubeMapFace(RenderTargetHandle target, uint32_t faceIndex) {
    auto it = m_impl->state.renderTargets.find(target.id);
    if (it == m_impl->state.renderTargets.end()) return;

    if (!it->second.isCubeMap) {
        LOG_WARN(LogCategory::Render, "Metal: attachCubeMapFace called on non-cubemap render target");
        return;
    }

    it->second.currentCubeFace = static_cast<int>(faceIndex);
}

void GfxDeviceMetal::unbindFramebuffer() {
    // In Metal, this is handled by ending the render command encoder
    // Nothing specific to do here since Metal doesn't have a global framebuffer binding
}

// ============================================================================
// Uniform Buffer Management
// ============================================================================

UniformBufferHandle GfxDeviceMetal::createUniformBuffer(uint32_t size) {
    uint32_t id = m_impl->state.nextUniformBufferID++;
    MetalUniformBuffer ub;
    ub.size = size;

    // Use shared storage for uniform buffers (CPU + GPU accessible)
    ub.buffer = [m_impl->state.device newBufferWithLength:size options:MTLResourceStorageModeShared];
    if (!ub.buffer) {
        LOG_ERROR(LogCategory::Render, "Metal: Failed to create uniform buffer");
        return UniformBufferHandle();
    }

    ub.contents = ub.buffer.contents;

    m_impl->state.uniformBuffers[id] = ub;
    return UniformBufferHandle(id);
}

void GfxDeviceMetal::destroyUniformBuffer(UniformBufferHandle buffer) {
    m_impl->state.uniformBuffers.erase(buffer.id);
}

void GfxDeviceMetal::updateUniformBuffer(UniformBufferHandle buffer, const void* data, uint32_t size, uint32_t offset) {
    auto it = m_impl->state.uniformBuffers.find(buffer.id);
    if (it == m_impl->state.uniformBuffers.end() || !data) return;

    MetalUniformBuffer& ub = it->second;
    if (offset + size > ub.size) {
        LOG_ERROR(LogCategory::Render, "Metal: Uniform buffer update exceeds buffer size");
        return;
    }

    memcpy(static_cast<uint8_t*>(ub.contents) + offset, data, size);
}

// ============================================================================
// Command Recording & Execution
// ============================================================================

std::unique_ptr<IGfxCommandList> GfxDeviceMetal::beginFrame(RenderTargetHandle target) {
    // Get render target first to check if we already have a drawable
    auto rtIt = m_impl->state.renderTargets.find(target.id);
    if (rtIt == m_impl->state.renderTargets.end()) {
        LOG_ERROR(LogCategory::Render, "Metal: Invalid render target for beginFrame");
        return nullptr;
    }

    MetalRenderTarget* rt = &rtIt->second;

    // Check if this is a swapchain backbuffer and if we already have a drawable
    bool needNewDrawable = false;
    if (rt->isSwapchainBackbuffer && rt->owningSwapchainId != 0) {
        auto scIt = m_impl->state.swapchains.find(rt->owningSwapchainId);
        if (scIt != m_impl->state.swapchains.end()) {
            MetalSwapchain& sc = scIt->second;
            // Only acquire new drawable if we don't have one
            needNewDrawable = (sc.currentDrawable == nil);
        }
    }

    // Only wait on semaphore if we need a new drawable (first camera of the frame)
    if (needNewDrawable) {
        // Wait for a slot in the frame semaphore (triple buffering)
        // Use a timeout to prevent hanging during shutdown
        dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(100 * NSEC_PER_MSEC)); // 100ms timeout
        long result = dispatch_semaphore_wait(m_impl->state.frameSemaphore, timeout);
        if (result != 0) {
            // Timeout occurred - this can happen during shutdown
            LOG_DEBUG(LogCategory::Render, "Metal: beginFrame semaphore timeout (likely shutting down)");
            return nullptr;
        }
    }

    auto cmdList = std::make_unique<GfxCommandListMetal>(this, &m_impl->state);

    // If this is a swapchain backbuffer, acquire the next drawable (if needed)
    if (rt->isSwapchainBackbuffer && rt->owningSwapchainId != 0) {
        auto scIt = m_impl->state.swapchains.find(rt->owningSwapchainId);
        if (scIt != m_impl->state.swapchains.end()) {
            MetalSwapchain& sc = scIt->second;
            if (!sc.metalLayer) {
                LOG_ERROR(LogCategory::Render, "Metal: beginFrame - metalLayer is nil!");
                // Signal semaphore since we're returning early (only if we waited)
                if (needNewDrawable) {
                    dispatch_semaphore_signal(m_impl->state.frameSemaphore);
                }
                return nullptr;
            } else if (needNewDrawable) {
                // Acquire new drawable only if we don't have one
                @autoreleasepool {
                    sc.currentDrawable = [sc.metalLayer nextDrawable];
                }
                if (sc.currentDrawable) {
                    rt->colorTexture = sc.currentDrawable.texture;
                    LOG_DEBUG(LogCategory::Render, "Metal: beginFrame acquired NEW drawable, texture size={}x{}",
                             [sc.currentDrawable.texture width], [sc.currentDrawable.texture height]);
                } else {
                    LOG_WARN(LogCategory::Render, "Metal: beginFrame - nextDrawable returned nil (window may be closing)");
                    // Signal semaphore since we're returning early
                    dispatch_semaphore_signal(m_impl->state.frameSemaphore);
                    return nullptr;
                }
            } else {
                // Reuse existing drawable - just update the render target texture
                rt->colorTexture = sc.currentDrawable.texture;
                LOG_DEBUG(LogCategory::Render, "Metal: beginFrame reusing existing drawable, texture size={}x{}",
                         [sc.currentDrawable.texture width], [sc.currentDrawable.texture height]);
            }
        } else {
            LOG_ERROR(LogCategory::Render, "Metal: beginFrame - swapchain {} not found", rt->owningSwapchainId);
            // Signal semaphore since we're returning early (only if we waited)
            if (needNewDrawable) {
                dispatch_semaphore_signal(m_impl->state.frameSemaphore);
            }
            return nullptr;
        }
    }

    cmdList->beginRendering(rt);
    return cmdList;
}

void GfxDeviceMetal::submit(std::unique_ptr<IGfxCommandList> commandList) {
    auto* metalCmdList = static_cast<GfxCommandListMetal*>(commandList.get());
    if (!metalCmdList) {
        LOG_ERROR(LogCategory::Render, "Metal: submit called with null command list");
        return;
    }

    metalCmdList->endRendering();

    id<MTLCommandBuffer> cmdBuffer = metalCmdList->getCommandBuffer();
    if (!cmdBuffer) {
        LOG_ERROR(LogCategory::Render, "Metal: submit called but command buffer is nil");
        return;
    }

    // DO NOT present here - present() will handle that after all cameras are rendered
    // DO NOT signal semaphore here - present() will handle that too

    // Just commit this command buffer to execute the rendering
    // Add error checking completion handler (but NOT semaphore signal)
    [cmdBuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer) {
        if (buffer.status == MTLCommandBufferStatusError) {
            NSError* error = buffer.error;
            NSLog(@"Metal: Command buffer error: %@", error.localizedDescription);
            if (error.userInfo) {
                NSLog(@"Metal: Error details: %@", error.userInfo);
            }
        }
    }];

    // Commit the command buffer
    [cmdBuffer commit];
    LOG_DEBUG(LogCategory::Render, "Metal: command buffer committed (no present)");
}

void GfxDeviceMetal::waitIdle() {
    // Wait for all frames in flight with a timeout to prevent hanging during shutdown
    dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1000 * NSEC_PER_MSEC)); // 1 second total timeout

    for (uint32_t i = 0; i < MetalState::MAX_FRAMES_IN_FLIGHT; ++i) {
        long result = dispatch_semaphore_wait(m_impl->state.frameSemaphore, timeout);
        if (result != 0) {
            LOG_WARN(LogCategory::Render, "Metal: waitIdle timeout on frame {}", i);
            break;
        }
    }
    for (uint32_t i = 0; i < MetalState::MAX_FRAMES_IN_FLIGHT; ++i) {
        dispatch_semaphore_signal(m_impl->state.frameSemaphore);
    }
}

// ============================================================================
// Mesh Management
// ============================================================================

MeshHandle GfxDeviceMetal::createMesh(const MeshData& meshData) {
    uint32_t id = m_impl->nextMeshID++;
    GPUMesh mesh;

    // Create vertex buffer
    if (!meshData.vertices.empty()) {
        BufferDesc vbDesc;
        vbDesc.size = meshData.vertices.size() * sizeof(Vertex);
        vbDesc.usage = BufferUsage::Vertex;
        vbDesc.initialData = meshData.vertices.data();
        mesh.vertexBuffer = createBuffer(vbDesc);
        mesh.vertexCount = static_cast<uint32_t>(meshData.vertices.size());
    }

    // Create index buffer
    if (!meshData.indices.empty()) {
        BufferDesc ibDesc;
        ibDesc.size = meshData.indices.size() * sizeof(uint32_t);
        ibDesc.usage = BufferUsage::Index;
        ibDesc.initialData = meshData.indices.data();
        mesh.indexBuffer = createBuffer(ibDesc);
        mesh.indexCount = static_cast<uint32_t>(meshData.indices.size());
    }

    mesh.submeshes = meshData.submeshes;
    mesh.bounds = meshData.bounds;

    m_impl->meshes[id] = mesh;
    return MeshHandle(id);
}

const GPUMesh* GfxDeviceMetal::getMesh(MeshHandle handle) const {
    auto it = m_impl->meshes.find(handle.id);
    if (it == m_impl->meshes.end()) return nullptr;
    return &it->second;
}

void GfxDeviceMetal::destroyMesh(MeshHandle handle) {
    auto it = m_impl->meshes.find(handle.id);
    if (it == m_impl->meshes.end()) return;

    destroyBuffer(it->second.vertexBuffer);
    destroyBuffer(it->second.indexBuffer);
    m_impl->meshes.erase(it);
}

// ============================================================================
// Font Management
// ============================================================================

FontAtlas GfxDeviceMetal::buildBakedAtlas(const BakedFontAtlas& baked) {
    FontAtlas fontAtlas;
    if (!baked.success) {
        return fontAtlas;
    }

    TextureDesc texDesc;
    texDesc.type = TextureType::Texture2D;
    texDesc.width = baked.atlasWidth;
    texDesc.height = baked.atlasHeight;
    texDesc.format = TextureFormat::R8_UNORM;
    texDesc.mipLevels = 1;
    texDesc.usage = TextureUsage::Sampled;
    texDesc.initialData = baked.bitmap.data();

    TextureHandle atlasTexture = createTexture(texDesc);
    if (!atlasTexture.isValid()) {
        return fontAtlas;
    }

    fontAtlas.texture = atlasTexture;
    fontAtlas.atlasWidth = baked.atlasWidth;
    fontAtlas.atlasHeight = baked.atlasHeight;
    fontAtlas.fontSize = baked.fontSize;
    fontAtlas.lineHeight = baked.lineHeight;
    fontAtlas.glyphs = baked.glyphs;
    return fontAtlas;
}

FontHandle GfxDeviceMetal::createFontAtlas(const FontDesc& desc) {
    BakedFontAtlas baked = BakeFontAtlas(desc, GetFontOversample());
    if (!baked.success) {
        LOG_ERROR(LogCategory::Render, "Metal: Failed to bake font: {}", desc.fontPath);
        return FontHandle();
    }

    FontAtlas fontAtlas = buildBakedAtlas(baked);
    if (!fontAtlas.texture.isValid()) {
        return FontHandle();
    }

    uint32_t fontID = m_impl->nextFontID++;
    m_impl->fontAtlases[fontID] = std::move(fontAtlas);
    m_impl->fontDescs[fontID] = desc;

    LOG_DEBUG(LogCategory::Render, "Metal: Created font atlas {} with {} glyphs", fontID, m_impl->fontAtlases[fontID].glyphs.size());
    return FontHandle(fontID);
}

void GfxDeviceMetal::refreshFontAtlases() {
    const float oversample = GetFontOversample();
    for (const auto& [fontID, desc] : m_impl->fontDescs) {
        BakedFontAtlas baked = BakeFontAtlas(desc, oversample);
        FontAtlas fontAtlas = buildBakedAtlas(baked);
        if (!fontAtlas.texture.isValid()) {
            continue;
        }

        auto it = m_impl->fontAtlases.find(fontID);
        if (it != m_impl->fontAtlases.end() && it->second.texture.isValid()) {
            destroyTexture(it->second.texture);
        }
        m_impl->fontAtlases[fontID] = std::move(fontAtlas);
    }
}

const FontAtlas* GfxDeviceMetal::getFontAtlas(FontHandle handle) const {
    auto it = m_impl->fontAtlases.find(handle.id);
    if (it == m_impl->fontAtlases.end()) return nullptr;
    return &it->second;
}

void GfxDeviceMetal::destroyFontAtlas(FontHandle handle) {
    m_impl->fontDescs.erase(handle.id);

    auto it = m_impl->fontAtlases.find(handle.id);
    if (it == m_impl->fontAtlases.end()) return;

    destroyTexture(it->second.texture);
    m_impl->fontAtlases.erase(it);
}

} // namespace lupine

#endif // __APPLE__
#endif // LUPINE_HAS_METAL
