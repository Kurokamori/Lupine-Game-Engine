#include "lupine/rendering/gfx/GfxDeviceFactory.hpp"
#ifndef __EMSCRIPTEN__
#include "lupine/rendering/backends/GfxDeviceOpenGL.hpp"
#endif
#ifdef LUPINE_HAS_VULKAN
#include "lupine/rendering/backends/vulkan/GfxDeviceVulkan.hpp"
#endif
#ifndef __EMSCRIPTEN__
#include "lupine/rendering/backends/GfxDeviceMetal.hpp"
#endif
#ifdef __EMSCRIPTEN__
#include "lupine/rendering/backends/GfxDeviceWebGL.hpp"
#endif
#ifdef LUPINE_HAS_DIRECTX11
#include "lupine/rendering/backends/GfxDeviceDX11.hpp"
#endif
#ifdef LUPINE_HAS_DIRECTX12
#include "lupine/rendering/backends/GfxDeviceDX12.hpp"
#endif
#include "lupine/logger/Logger.hpp"

#if defined(_WIN32)
    #include <windows.h>
#elif defined(__linux__) || defined(__APPLE__)
    #include <dlfcn.h>
#endif

namespace lupine {

std::unique_ptr<IGfxDevice> GfxDeviceFactory::create(GraphicsBackend backend) {
    switch (backend) {
        case GraphicsBackend::OpenGL:
#ifndef __EMSCRIPTEN__
            return std::make_unique<GfxDeviceOpenGL>();
#else
            LOG_ERROR(LogCategory::Render, "OpenGL not available in WebAssembly builds, use WebGL");
            return nullptr;
#endif

        case GraphicsBackend::Vulkan:
#ifdef LUPINE_HAS_VULKAN
            return std::make_unique<GfxDeviceVulkan>();

            #else
            
            return nullptr;
#endif

        case GraphicsBackend::Metal:
#ifdef LUPINE_HAS_METAL
            return std::make_unique<GfxDeviceMetal>();
#else
            LOG_ERROR(LogCategory::Render, "Metal support not enabled in this build");
            return nullptr;
#endif

        case GraphicsBackend::WebGL:
#ifdef __EMSCRIPTEN__
            return std::make_unique<GfxDeviceWebGL>();
#else
            LOG_ERROR(LogCategory::Render, "WebGL is only available in Emscripten builds");
            return nullptr;
#endif

        case GraphicsBackend::DirectX11:
#ifdef LUPINE_HAS_DIRECTX11
            return std::make_unique<GfxDeviceDX11>();
#else
            LOG_ERROR(LogCategory::Render, "DirectX 11 support not enabled in this build");
            return nullptr;
#endif

        case GraphicsBackend::DirectX12:
#ifdef LUPINE_HAS_DIRECTX12
            return std::make_unique<GfxDeviceDX12>();
#else
            LOG_ERROR(LogCategory::Render, "DirectX 12 support not enabled in this build");
            return nullptr;
#endif

        default:

            return nullptr;
    }
}

GraphicsBackend GfxDeviceFactory::getDefaultBackend() {
#if defined(_WIN32)

    if (isBackendAvailable(GraphicsBackend::DirectX11)) {
        return GraphicsBackend::DirectX11;
    }
    return GraphicsBackend::OpenGL;
#elif defined(__APPLE__)

    if (isBackendAvailable(GraphicsBackend::Metal)) {
        return GraphicsBackend::Metal;
    }
    return GraphicsBackend::OpenGL;
#elif defined(__EMSCRIPTEN__)

    return GraphicsBackend::WebGL;
#else

    if (isBackendAvailable(GraphicsBackend::Vulkan)) {
        return GraphicsBackend::Vulkan;
    }
    return GraphicsBackend::OpenGL;
#endif
}

bool GfxDeviceFactory::isBackendAvailable(GraphicsBackend backend) {
    switch (backend) {
        case GraphicsBackend::OpenGL:

            return true;

        case GraphicsBackend::Vulkan:

#if defined(VK_VERSION_1_0) || defined(VULKAN_HPP_INCLUDED)

            try {

#if defined(_WIN32)
                HMODULE vulkanLib = LoadLibraryA("vulkan-1.dll");
                if (!vulkanLib) {
                    return false;
                }
                FreeLibrary(vulkanLib);
#elif defined(__linux__)
                void* vulkanLib = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
                if (!vulkanLib) {
                    vulkanLib = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
                }
                if (!vulkanLib) {
                    return false;
                }
                dlclose(vulkanLib);
#elif defined(__APPLE__)
                void* vulkanLib = dlopen("libvulkan.dylib", RTLD_NOW | RTLD_LOCAL);
                if (!vulkanLib) {
                    vulkanLib = dlopen("libMoltenVK.dylib", RTLD_NOW | RTLD_LOCAL);
                }
                if (!vulkanLib) {
                    return false;
                }
                dlclose(vulkanLib);
#endif

                return true;

            } catch (...) {
                return false;
            }
#else

            return false;
#endif

        case GraphicsBackend::Metal:
#if defined(__APPLE__)
            return true;
#else
            return false;
#endif

        case GraphicsBackend::WebGL:
#if defined(__EMSCRIPTEN__)
            return true;
#else
            return false;
#endif

        case GraphicsBackend::DirectX11: {
#if defined(_WIN32) && defined(LUPINE_HAS_DIRECTX11)
            // Check if D3D11 is available by attempting to load the DLL
            HMODULE d3d11Lib = LoadLibraryA("d3d11.dll");
            if (!d3d11Lib) {
                return false;
            }
            FreeLibrary(d3d11Lib);
            return true;
#else
            return false;
#endif
        }

        case GraphicsBackend::DirectX12: {
#if defined(_WIN32) && defined(LUPINE_HAS_DIRECTX12)
            // Check if D3D12 is available by attempting to load the DLL
            HMODULE d3d12Lib = LoadLibraryA("d3d12.dll");
            if (!d3d12Lib) {
                return false;
            }
            FreeLibrary(d3d12Lib);
            return true;
#else
            return false;
#endif
        }

        default:
            return false;
    }
}

const char* GfxDeviceFactory::getBackendName(GraphicsBackend backend) {
    switch (backend) {
        case GraphicsBackend::OpenGL: return "OpenGL";
        case GraphicsBackend::Vulkan: return "Vulkan";
        case GraphicsBackend::Metal: return "Metal";
        case GraphicsBackend::WebGL: return "WebGL";
        case GraphicsBackend::DirectX11: return "DirectX 11";
        case GraphicsBackend::DirectX12: return "DirectX 12";
        default: return "Unknown";
    }
}

std::unique_ptr<IGfxDevice> createGraphicsDevice(GraphicsBackend backend) {
    return GfxDeviceFactory::create(backend);
}

}
