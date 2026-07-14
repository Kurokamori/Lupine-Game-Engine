/**
 * @file lc_runtime.cpp
 * @brief Implementation of the runtime application / window / main-loop C API.
 *
 * Thin wrapper over lupine::RuntimeApp. Handles are validated against a registry
 * of live instances so a stale or foreign pointer is rejected rather than
 * dereferenced.
 */

#include "lc_runtime.h"
#include "core/lc_core.h"
#include "../../capi/src/core/lc_internal.h"

#include <lupine/runtime/RuntimeApp.hpp>
#include <lupine/rendering/gfx/GfxTypes.hpp>

#include <mutex>
#include <unordered_set>
#include <string>

namespace {

std::unordered_set<lupine::RuntimeApp*> g_runtimeApps;
std::mutex g_runtimeAppsMutex;

lupine::RuntimeApp* ResolveApp(LCRuntimeAppHandle handle) {
    if (!handle) return nullptr;
    std::lock_guard<std::mutex> lock(g_runtimeAppsMutex);
    lupine::RuntimeApp* app = reinterpret_cast<lupine::RuntimeApp*>(handle);
    return (g_runtimeApps.find(app) != g_runtimeApps.end()) ? app : nullptr;
}

lupine::GraphicsBackend ToEngineBackend(LCGraphicsBackend backend) {
    switch (backend) {
        case LC_BACKEND_OPENGL:    return lupine::GraphicsBackend::OpenGL;
        case LC_BACKEND_VULKAN:    return lupine::GraphicsBackend::Vulkan;
        case LC_BACKEND_METAL:     return lupine::GraphicsBackend::Metal;
        case LC_BACKEND_WEBGL:     return lupine::GraphicsBackend::WebGL;
        case LC_BACKEND_DIRECTX11: return lupine::GraphicsBackend::DirectX11;
        case LC_BACKEND_DIRECTX12: return lupine::GraphicsBackend::DirectX12;
        case LC_BACKEND_NONE:      return lupine::GraphicsBackend::None;
        default:                   return lupine::GraphicsBackend::OpenGL;
    }
}

} // namespace

LC_API LCResult lc_runtime_config_default(LCRuntimeConfig* out_config) {
    if (!out_config) {
        SetError(LC_ERROR_NULL_POINTER, "out_config is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    lupine::RuntimeApp::Config defaults;
    out_config->title = "Lupine Runtime";
    out_config->window_width = defaults.windowWidth;
    out_config->window_height = defaults.windowHeight;
    out_config->debugging = defaults.debugging;
    out_config->vsync = defaults.vsync;
    out_config->resizable = defaults.resizable;
    out_config->fullscreen = defaults.fullscreen;
    out_config->backend = LC_BACKEND_OPENGL;
    out_config->project_path = NULL;
    out_config->scene_path = NULL;
    return LC_SUCCESS;
}

LC_API LCResult lc_runtime_create(LCRuntimeAppHandle* out_app) {
    if (!out_app) {
        SetError(LC_ERROR_NULL_POINTER, "out_app is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        lupine::RuntimeApp* app = new lupine::RuntimeApp();
        {
            std::lock_guard<std::mutex> lock(g_runtimeAppsMutex);
            g_runtimeApps.insert(app);
        }
        *out_app = reinterpret_cast<LCRuntimeAppHandle>(app);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_OUT_OF_MEMORY, "Failed to allocate RuntimeApp");
        return LC_ERROR_OUT_OF_MEMORY;
    }
}

LC_API LCResult lc_runtime_destroy(LCRuntimeAppHandle app) {
    lupine::RuntimeApp* instance = ResolveApp(app);
    if (!instance) return LC_ERROR_INVALID_HANDLE;
    try {
        {
            std::lock_guard<std::mutex> lock(g_runtimeAppsMutex);
            g_runtimeApps.erase(instance);
        }
        delete instance;
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_runtime_destroy");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_runtime_initialize(LCRuntimeAppHandle app, const LCRuntimeConfig* config) {
    lupine::RuntimeApp* instance = ResolveApp(app);
    if (!instance) return LC_ERROR_INVALID_HANDLE;
    if (!config) {
        SetError(LC_ERROR_NULL_POINTER, "config is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        lupine::RuntimeApp::Config engineConfig;
        engineConfig.title = config->title ? config->title : "Lupine Runtime";
        engineConfig.windowWidth = config->window_width;
        engineConfig.windowHeight = config->window_height;
        engineConfig.debugging = config->debugging;
        engineConfig.vsync = config->vsync;
        engineConfig.resizable = config->resizable;
        engineConfig.fullscreen = config->fullscreen;
        engineConfig.backend = ToEngineBackend(config->backend);
        engineConfig.projectPath = config->project_path ? config->project_path : "";
        engineConfig.scenePath = config->scene_path ? config->scene_path : "";

        if (!instance->initialize(engineConfig)) {
            SetError(LC_ERROR_INITIALIZATION_FAILED, "RuntimeApp initialization failed");
            return LC_ERROR_INITIALIZATION_FAILED;
        }
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INITIALIZATION_FAILED, "Exception in lc_runtime_initialize");
        return LC_ERROR_INITIALIZATION_FAILED;
    }
}

LC_API LCResult lc_runtime_run(LCRuntimeAppHandle app) {
    lupine::RuntimeApp* instance = ResolveApp(app);
    if (!instance) return LC_ERROR_INVALID_HANDLE;
    try {
        instance->run();
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_runtime_run");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_runtime_run_frame(LCRuntimeAppHandle app, bool* out_keep_running) {
    lupine::RuntimeApp* instance = ResolveApp(app);
    if (!instance) return LC_ERROR_INVALID_HANDLE;
    try {
        bool keepRunning = instance->runFrame();
        if (out_keep_running) *out_keep_running = keepRunning;
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_runtime_run_frame");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_runtime_process_events(LCRuntimeAppHandle app, bool* out_keep_running) {
    lupine::RuntimeApp* instance = ResolveApp(app);
    if (!instance) return LC_ERROR_INVALID_HANDLE;
    try {
        bool keepRunning = instance->processEvents();
        if (out_keep_running) *out_keep_running = keepRunning;
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_runtime_process_events");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_runtime_stop(LCRuntimeAppHandle app) {
    lupine::RuntimeApp* instance = ResolveApp(app);
    if (!instance) return LC_ERROR_INVALID_HANDLE;
    try {
        instance->stop();
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_runtime_stop");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_runtime_shutdown(LCRuntimeAppHandle app) {
    lupine::RuntimeApp* instance = ResolveApp(app);
    if (!instance) return LC_ERROR_INVALID_HANDLE;
    try {
        instance->shutdown();
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_runtime_shutdown");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_runtime_pause(LCRuntimeAppHandle app) {
    lupine::RuntimeApp* instance = ResolveApp(app);
    if (!instance) return LC_ERROR_INVALID_HANDLE;
    try {
        instance->pause();
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_runtime_pause");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_runtime_resume(LCRuntimeAppHandle app) {
    lupine::RuntimeApp* instance = ResolveApp(app);
    if (!instance) return LC_ERROR_INVALID_HANDLE;
    try {
        instance->resume();
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_runtime_resume");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_runtime_load_scene(LCRuntimeAppHandle app, const char* scene_path) {
    lupine::RuntimeApp* instance = ResolveApp(app);
    if (!instance) return LC_ERROR_INVALID_HANDLE;
    if (!scene_path) {
        SetError(LC_ERROR_NULL_POINTER, "scene_path is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        if (!instance->loadScene(scene_path)) {
            SetError(LC_ERROR_OPERATION_FAILED, "Failed to load scene");
            return LC_ERROR_OPERATION_FAILED;
        }
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_runtime_load_scene");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_runtime_reload_scene(LCRuntimeAppHandle app) {
    lupine::RuntimeApp* instance = ResolveApp(app);
    if (!instance) return LC_ERROR_INVALID_HANDLE;
    try {
        instance->reloadScene();
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_runtime_reload_scene");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API bool lc_runtime_is_running(LCRuntimeAppHandle app) {
    lupine::RuntimeApp* instance = ResolveApp(app);
    if (!instance) return false;
    try {
        return instance->isRunning();
    } catch (...) {
        return false;
    }
}

LC_API bool lc_runtime_is_paused(LCRuntimeAppHandle app) {
    lupine::RuntimeApp* instance = ResolveApp(app);
    if (!instance) return false;
    try {
        return instance->isPaused();
    } catch (...) {
        return false;
    }
}

LC_API bool lc_runtime_is_showing_splash(LCRuntimeAppHandle app) {
    lupine::RuntimeApp* instance = ResolveApp(app);
    if (!instance) return false;
    try {
        return instance->isShowingSplashScreens();
    } catch (...) {
        return false;
    }
}

LC_API LCResult lc_runtime_skip_splash(LCRuntimeAppHandle app) {
    lupine::RuntimeApp* instance = ResolveApp(app);
    if (!instance) return LC_ERROR_INVALID_HANDLE;
    try {
        instance->skipSplashScreens();
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_runtime_skip_splash");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_runtime_get_window_size(LCRuntimeAppHandle app, int* out_width, int* out_height) {
    lupine::RuntimeApp* instance = ResolveApp(app);
    if (!instance) return LC_ERROR_INVALID_HANDLE;
    if (!out_width || !out_height) {
        SetError(LC_ERROR_NULL_POINTER, "out_width/out_height is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        lupine::Window* window = instance->getWindow();
        if (!window) {
            SetError(LC_ERROR_NOT_INITIALIZED, "Runtime window not created yet");
            return LC_ERROR_NOT_INITIALIZED;
        }
        int width = 0;
        int height = 0;
        window->getSize(width, height);
        *out_width = width;
        *out_height = height;
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception in lc_runtime_get_window_size");
        return LC_ERROR_INTERNAL_ERROR;
    }
}
