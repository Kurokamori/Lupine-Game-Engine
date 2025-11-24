#include "lupine/rendering/gfx/GfxDeviceFactory.hpp"
#include "lupine/rendering/backends/GfxDeviceOpenGL.hpp"
#include "lupine/rendering/backends/GfxDeviceVulkan.hpp"
#include "lupine/rendering/backends/GfxDeviceMetal.hpp"
#include "lupine/rendering/backends/GfxDeviceWebGL.hpp"
#include "lupine/rendering/backends/GfxDeviceDX11.hpp"
#include "lupine/rendering/backends/GfxDeviceDX12.hpp"
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
            return std::make_unique<GfxDeviceOpenGL>();

        case GraphicsBackend::Vulkan:

            return nullptr;

        case GraphicsBackend::Metal:

            return nullptr;

        case GraphicsBackend::WebGL:

            return nullptr;

        case GraphicsBackend::DirectX11:

            return nullptr;

        case GraphicsBackend::DirectX12:

            return nullptr;

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

        case GraphicsBackend::DirectX11:
        case GraphicsBackend::DirectX12:
#if defined(_WIN32)
            return false;
#else
            return false;
#endif

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
