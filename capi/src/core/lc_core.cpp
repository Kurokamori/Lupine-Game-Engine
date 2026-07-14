

#include "core/lc_core.h"
#include "lc_internal.h"

#include <lupine/core/Core.hpp>
#include <lupine/core/SceneManager.hpp>
#include <lupine/logger/Logger.hpp>
#include <lupine/platform/Platform.hpp>
#include <lupine/platform/DisplayServer.hpp>
#include <lupine/input/InputManager.hpp>

#include <nlohmann/json.hpp>

#include <string>
#include <vector>
#include <mutex>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <atomic>
#include <chrono>

namespace {

std::atomic<bool> g_initialized{false};
std::mutex g_initMutex;

thread_local LCResult g_lastErrorCode = LC_SUCCESS;
thread_local char g_lastErrorMessage[512] = {0};

constexpr const char* g_capiVersion = "1.0.0";
constexpr const char* g_engineVersion = "0.1.0";

} // anonymous namespace

// Global SetError function - exported for use by other CAPI source files
void SetError(LCResult code, const char* message) {
    g_lastErrorCode = code;
    if (message) {
        CopyStringToBuffer(g_lastErrorMessage, sizeof(g_lastErrorMessage), message);
    } else {
        g_lastErrorMessage[0] = '\0';
    }
}

namespace {

LCLogLevel SpdlogToLCLogLevel(spdlog::level::level_enum level) {
    switch (level) {
        case spdlog::level::trace: return LC_LOG_TRACE;
        case spdlog::level::debug: return LC_LOG_DEBUG;
        case spdlog::level::info: return LC_LOG_INFO;
        case spdlog::level::warn: return LC_LOG_WARN;
        case spdlog::level::err: return LC_LOG_ERROR;
        case spdlog::level::critical: return LC_LOG_CRITICAL;
        case spdlog::level::off: return LC_LOG_OFF;
        default: return LC_LOG_INFO;
    }
}

spdlog::level::level_enum LCLogLevelToSpdlog(LCLogLevel level) {
    switch (level) {
        case LC_LOG_TRACE: return spdlog::level::trace;
        case LC_LOG_DEBUG: return spdlog::level::debug;
        case LC_LOG_INFO: return spdlog::level::info;
        case LC_LOG_WARN: return spdlog::level::warn;
        case LC_LOG_ERROR: return spdlog::level::err;
        case LC_LOG_CRITICAL: return spdlog::level::critical;
        case LC_LOG_OFF: return spdlog::level::off;
        default: return spdlog::level::info;
    }
}

}


LC_API void lc_get_version(int* major, int* minor, int* patch) {
    if (major) *major = LC_VERSION_MAJOR;
    if (minor) *minor = LC_VERSION_MINOR;
    if (patch) *patch = LC_VERSION_PATCH;
}

LC_API const char* lc_get_version_string(void) {
    return g_capiVersion;
}

LC_API const char* lc_get_engine_version(void) {
    return g_engineVersion;
}

LC_API const char* lc_get_last_error(void) {
    return g_lastErrorMessage;
}

LC_API LCResult lc_get_last_error_code(void) {
    return g_lastErrorCode;
}

LC_API void lc_clear_last_error(void) {
    g_lastErrorCode = LC_SUCCESS;
    g_lastErrorMessage[0] = '\0';
}

LC_API const char* lc_result_to_string(LCResult result) {
    switch (result) {
        case LC_SUCCESS: return "Success";

        case LC_ERROR_INVALID_HANDLE: return "Invalid handle";
        case LC_ERROR_NULL_POINTER: return "Null pointer";
        case LC_ERROR_INVALID_PARAMETER: return "Invalid parameter";
        case LC_ERROR_OUT_OF_MEMORY: return "Out of memory";
        case LC_ERROR_NOT_INITIALIZED: return "Engine not initialized";
        case LC_ERROR_ALREADY_INITIALIZED: return "Engine already initialized";
        case LC_ERROR_INITIALIZATION_FAILED: return "Initialization failed";
        case LC_ERROR_SHUTDOWN_FAILED: return "Shutdown failed";
        case LC_ERROR_INTERNAL_ERROR: return "Internal error";
        case LC_ERROR_NOT_IMPLEMENTED: return "Not implemented";
        case LC_ERROR_OPERATION_FAILED: return "Operation failed";

        case LC_ERROR_FILE_NOT_FOUND: return "File not found";
        case LC_ERROR_FILE_READ_FAILED: return "File read failed";
        case LC_ERROR_FILE_WRITE_FAILED: return "File write failed";
        case LC_ERROR_FILE_INVALID_FORMAT: return "Invalid file format";
        case LC_ERROR_PATH_INVALID: return "Invalid path";

        case LC_ERROR_GFX_DEVICE_CREATION_FAILED: return "Graphics device creation failed";
        case LC_ERROR_GFX_INVALID_BACKEND: return "Invalid graphics backend";
        case LC_ERROR_GFX_RESOURCE_CREATION_FAILED: return "Graphics resource creation failed";
        case LC_ERROR_GFX_INVALID_RESOURCE: return "Invalid graphics resource";

        case LC_ERROR_NODE_INVALID_TYPE: return "Invalid node type";
        case LC_ERROR_NODE_INVALID_PARENT: return "Invalid parent node";
        case LC_ERROR_NODE_CIRCULAR_HIERARCHY: return "Circular node hierarchy";
        case LC_ERROR_SCENE_INVALID: return "Invalid scene";
        case LC_ERROR_SCENE_LOAD_FAILED: return "Scene load failed";
        case LC_ERROR_SCENE_SAVE_FAILED: return "Scene save failed";

        case LC_ERROR_COMPONENT_INVALID_TYPE: return "Invalid component type";
        case LC_ERROR_COMPONENT_NOT_FOUND: return "Component not found";
        case LC_ERROR_COMPONENT_ALREADY_EXISTS: return "Component already exists";

        case LC_ERROR_PHYSICS_WORLD_INVALID: return "Invalid physics world";
        case LC_ERROR_PHYSICS_BODY_INVALID: return "Invalid physics body";
        case LC_ERROR_PHYSICS_COLLIDER_INVALID: return "Invalid collider";

        case LC_ERROR_AUDIO_INIT_FAILED: return "Audio initialization failed";
        case LC_ERROR_AUDIO_ASSET_LOAD_FAILED: return "Audio asset load failed";
        case LC_ERROR_AUDIO_PLAYBACK_FAILED: return "Audio playback failed";

        case LC_ERROR_ASSET_LOAD_FAILED: return "Asset load failed";
        case LC_ERROR_ASSET_INVALID_TYPE: return "Invalid asset type";
        case LC_ERROR_ASSET_NOT_FOUND: return "Asset not found";

        default: return "Unknown error";
    }
}

LC_API void lc_log_set_level(LCLogLevel level) {
    try {
        lupine::Logger::SetLogLevel(LCLogLevelToSpdlog(level));
    } catch (const std::exception& e) {
        SetError(LC_ERROR_INTERNAL_ERROR, e.what());
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Unknown exception in lc_log_set_level");
    }
}

LC_API LCLogLevel lc_log_get_level(void) {
    try {
        auto logger = lupine::Logger::GetLogger();
        if (logger) {
            return SpdlogToLCLogLevel(logger->level());
        }
        return LC_LOG_INFO;
    } catch (...) {
        return LC_LOG_INFO;
    }
}

LC_API void lc_log(LCLogLevel level, const char* message) {
    if (!message) return;

    try {
        auto logger = lupine::Logger::GetLogger();
        if (!logger) return;

        switch (level) {
            case LC_LOG_TRACE:
                logger->trace("[C API] {}", message);
                break;
            case LC_LOG_DEBUG:
                logger->debug("[C API] {}", message);
                break;
            case LC_LOG_INFO:
                logger->info("[C API] {}", message);
                break;
            case LC_LOG_WARN:
                logger->warn("[C API] {}", message);
                break;
            case LC_LOG_ERROR:
                logger->error("[C API] {}", message);
                break;
            case LC_LOG_CRITICAL:
                logger->critical("[C API] {}", message);
                break;
            case LC_LOG_OFF:
            default:
                break;
        }
    } catch (const std::exception& e) {
        SetError(LC_ERROR_INTERNAL_ERROR, e.what());
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Unknown exception in lc_log");
    }
}

LC_API void lc_log_format(LCLogLevel level, const char* format, ...) {
    if (!format) return;

    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    lc_log(level, buffer);
}

LC_API void lc_log_trace(const char* message) {
    lc_log(LC_LOG_TRACE, message);
}

LC_API void lc_log_debug(const char* message) {
    lc_log(LC_LOG_DEBUG, message);
}

LC_API void lc_log_info(const char* message) {
    lc_log(LC_LOG_INFO, message);
}

LC_API void lc_log_warn(const char* message) {
    lc_log(LC_LOG_WARN, message);
}

LC_API void lc_log_error(const char* message) {
    lc_log(LC_LOG_ERROR, message);
}

LC_API void lc_log_critical(const char* message) {
    lc_log(LC_LOG_CRITICAL, message);
}

LC_API LCResult lc_init(void) {
    std::lock_guard<std::mutex> lock(g_initMutex);

    if (g_initialized.load()) {
        return LC_SUCCESS;
    }

    try {

        lupine::Logger::Init("lupine_capi.log", true);

        lupine::core::InitializeCore();

        g_initialized.store(true);

        lc_log_info("Lupine Engine C API initialized successfully");
        lc_log_format(LC_LOG_INFO, "C API Version: %s", g_capiVersion);
        lc_log_format(LC_LOG_INFO, "Engine Version: %s", g_engineVersion);

        return LC_SUCCESS;

    } catch (const std::exception& e) {
        SetError(LC_ERROR_INITIALIZATION_FAILED, e.what());
        return LC_ERROR_INITIALIZATION_FAILED;
    } catch (...) {
        SetError(LC_ERROR_INITIALIZATION_FAILED, "Unknown exception during initialization");
        return LC_ERROR_INITIALIZATION_FAILED;
    }
}

LC_API LCResult lc_shutdown(void) {
    std::lock_guard<std::mutex> lock(g_initMutex);

    if (!g_initialized.load()) {
        return LC_SUCCESS;
    }

    try {
        lc_log_info("Shutting down Lupine Engine C API");

        lupine::core::ShutdownCore();

        lupine::Logger::Flush();
        lupine::Logger::Shutdown();

        g_initialized.store(false);

        return LC_SUCCESS;

    } catch (const std::exception& e) {
        SetError(LC_ERROR_SHUTDOWN_FAILED, e.what());
        return LC_ERROR_SHUTDOWN_FAILED;
    } catch (...) {
        SetError(LC_ERROR_SHUTDOWN_FAILED, "Unknown exception during shutdown");
        return LC_ERROR_SHUTDOWN_FAILED;
    }
}

LC_API bool lc_is_initialized(void) {
    return g_initialized.load();
}

LC_API LCResult lc_quit(void) {
    try {
        lupine::core::SceneManager* sceneManager = lupine::core::SceneManager::GetInstance();
        if (!sceneManager) {
            SetError(LC_ERROR_OPERATION_FAILED, "No active scene manager to request quit on");
            return LC_ERROR_OPERATION_FAILED;
        }
        sceneManager->RequestQuit();
        return LC_SUCCESS;
    } catch (const std::exception& e) {
        SetError(LC_ERROR_INTERNAL_ERROR, e.what());
        return LC_ERROR_INTERNAL_ERROR;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Unknown exception in lc_quit");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_cmdline_arg_count(int* out_count) {
    if (!out_count) {
        SetError(LC_ERROR_INVALID_PARAMETER, "out_count must not be NULL");
        return LC_ERROR_INVALID_PARAMETER;
    }
    try {
        *out_count = static_cast<int>(lupine::core::GetCommandLineArgs().size());
        return LC_SUCCESS;
    } catch (const std::exception& e) {
        SetError(LC_ERROR_INTERNAL_ERROR, e.what());
        return LC_ERROR_INTERNAL_ERROR;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Unknown exception in lc_cmdline_arg_count");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_cmdline_arg_at(int index, char* out_buffer, size_t buffer_size) {
    if (!out_buffer || buffer_size == 0) {
        SetError(LC_ERROR_INVALID_PARAMETER, "out_buffer must not be NULL and buffer_size must be > 0");
        return LC_ERROR_INVALID_PARAMETER;
    }
    try {
        const std::vector<std::string>& args = lupine::core::GetCommandLineArgs();
        if (index < 0 || static_cast<size_t>(index) >= args.size()) {
            out_buffer[0] = '\0';
            SetError(LC_ERROR_INVALID_PARAMETER, "command-line argument index out of range");
            return LC_ERROR_INVALID_PARAMETER;
        }
        const std::string& value = args[static_cast<size_t>(index)];
        CopyStringToBuffer(out_buffer, buffer_size, value.c_str());
        return LC_SUCCESS;
    } catch (const std::exception& e) {
        SetError(LC_ERROR_INTERNAL_ERROR, e.what());
        return LC_ERROR_INTERNAL_ERROR;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Unknown exception in lc_cmdline_arg_at");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API void lc_free(void* ptr) {
    if (ptr) {
        free(ptr);
    }
}

/* ============================================================================
 * Time Scale
 * ============================================================================ */

LC_API void lc_set_time_scale(float time_scale) {
    try {
        lupine::core::SceneManager* sceneManager = lupine::core::SceneManager::GetInstance();
        if (sceneManager) {
            sceneManager->SetTimeScale(time_scale);
        }
    } catch (...) {
    }
}

LC_API float lc_get_time_scale(void) {
    try {
        lupine::core::SceneManager* sceneManager = lupine::core::SceneManager::GetInstance();
        return sceneManager ? sceneManager->GetTimeScale() : 1.0f;
    } catch (...) {
        return 1.0f;
    }
}

/* ============================================================================
 * Engine / OS Information
 * ============================================================================ */

namespace {

std::chrono::steady_clock::time_point EngineStartTime() {
    static const std::chrono::steady_clock::time_point s_start = std::chrono::steady_clock::now();
    return s_start;
}

} // anonymous namespace

LC_API float lc_get_fps(void) {
    static std::chrono::steady_clock::time_point s_lastCall = std::chrono::steady_clock::now();
    static float s_smoothedFps = 0.0f;

    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    float delta = std::chrono::duration_cast<std::chrono::duration<float>>(now - s_lastCall).count();
    s_lastCall = now;

    if (delta > 0.0001f) {
        float instantaneous = 1.0f / delta;
        s_smoothedFps = (s_smoothedFps <= 0.0f)
            ? instantaneous
            : s_smoothedFps * 0.9f + instantaneous * 0.1f;
    }
    return s_smoothedFps;
}

LC_API int lc_get_ticks_msec(void) {
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    return static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now - EngineStartTime()).count());
}

LC_API double lc_get_unix_time(void) {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count() / 1000.0;
}

LC_API const char* lc_get_platform_name(void) {
    try {
        return lupine::platform::Platform::GetPlatformName();
    } catch (...) {
        return "";
    }
}

LC_API bool lc_is_debug_build(void) {
    try {
        return lupine::platform::Platform::IsDebug();
    } catch (...) {
        return false;
    }
}

LC_API float lc_get_dpi_scale(void) {
    try {
        return lupine::input::InputManager::Get().GetDPIScale();
    } catch (...) {
        return 1.0f;
    }
}

LC_API bool lc_open_url(const char* url) {
    if (!url) {
        SetError(LC_ERROR_NULL_POINTER, "url is NULL");
        return false;
    }
    try {
        return lupine::platform::DisplayServer::Get().OpenURL(std::string(url));
    } catch (...) {
        return false;
    }
}

/* ============================================================================
 * Game State
 * ============================================================================ */

LC_API LCResult lc_set_game_paused(bool paused) {
    try {
        lupine::core::SceneManager* sceneManager = lupine::core::SceneManager::GetInstance();
        if (!sceneManager) {
            SetError(LC_ERROR_OPERATION_FAILED, "No active scene manager");
            return LC_ERROR_OPERATION_FAILED;
        }
        sceneManager->SetGamePaused(paused);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to set game paused");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_is_game_paused(bool* out_paused) {
    if (!out_paused) {
        SetError(LC_ERROR_NULL_POINTER, "out_paused is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        lupine::core::SceneManager* sceneManager = lupine::core::SceneManager::GetInstance();
        *out_paused = sceneManager ? sceneManager->IsGamePaused() : false;
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to query game paused");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_get_delta_time(float* out_delta) {
    if (!out_delta) {
        SetError(LC_ERROR_NULL_POINTER, "out_delta is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        lupine::core::SceneManager* sceneManager = lupine::core::SceneManager::GetInstance();
        if (!sceneManager) {
            SetError(LC_ERROR_OPERATION_FAILED, "No active scene manager");
            return LC_ERROR_OPERATION_FAILED;
        }
        *out_delta = sceneManager->GetDeltaTime();
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to get delta time");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API float lc_get_time(void) {
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::duration<float>>(now - EngineStartTime()).count();
}

LC_API LCResult lc_get_frame_count(uint64_t* out_count) {
    if (!out_count) {
        SetError(LC_ERROR_NULL_POINTER, "out_count is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        lupine::core::SceneManager* sceneManager = lupine::core::SceneManager::GetInstance();
        if (!sceneManager) {
            SetError(LC_ERROR_OPERATION_FAILED, "No active scene manager");
            return LC_ERROR_OPERATION_FAILED;
        }
        *out_count = sceneManager->GetFrameCount();
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to get frame count");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Global Variables
 * ============================================================================ */

namespace {

LCResult CopyStringToBuffer(const std::string& value, char* out_buffer, size_t buffer_size) {
    if (!out_buffer || buffer_size == 0) {
        SetError(LC_ERROR_NULL_POINTER, "out_buffer is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (value.size() + 1 > buffer_size) {
        SetError(LC_ERROR_INVALID_PARAMETER, "Buffer too small for result");
        return LC_ERROR_INVALID_PARAMETER;
    }
    std::memcpy(out_buffer, value.c_str(), value.size() + 1);
    return LC_SUCCESS;
}

} // anonymous namespace

LC_API LCResult lc_get_global_int(const char* name, int default_value, int* out_value) {
    if (!name || !out_value) {
        SetError(LC_ERROR_NULL_POINTER, "name or out_value is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        lupine::core::SceneManager* sceneManager = lupine::core::SceneManager::GetInstance();
        if (!sceneManager) {
            SetError(LC_ERROR_OPERATION_FAILED, "No active scene manager");
            return LC_ERROR_OPERATION_FAILED;
        }
        *out_value = sceneManager->GetGlobalInt(name, default_value);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to get global int");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_get_global_float(const char* name, float default_value, float* out_value) {
    if (!name || !out_value) {
        SetError(LC_ERROR_NULL_POINTER, "name or out_value is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        lupine::core::SceneManager* sceneManager = lupine::core::SceneManager::GetInstance();
        if (!sceneManager) {
            SetError(LC_ERROR_OPERATION_FAILED, "No active scene manager");
            return LC_ERROR_OPERATION_FAILED;
        }
        *out_value = sceneManager->GetGlobalFloat(name, default_value);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to get global float");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_get_global_string(const char* name, const char* default_value,
                                     char* out_buffer, size_t buffer_size) {
    if (!name) {
        SetError(LC_ERROR_NULL_POINTER, "name is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        lupine::core::SceneManager* sceneManager = lupine::core::SceneManager::GetInstance();
        if (!sceneManager) {
            SetError(LC_ERROR_OPERATION_FAILED, "No active scene manager");
            return LC_ERROR_OPERATION_FAILED;
        }
        const std::string value = sceneManager->GetGlobalString(name, default_value ? default_value : "");
        return CopyStringToBuffer(value, out_buffer, buffer_size);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to get global string");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_get_global_bool(const char* name, bool default_value, bool* out_value) {
    if (!name || !out_value) {
        SetError(LC_ERROR_NULL_POINTER, "name or out_value is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        lupine::core::SceneManager* sceneManager = lupine::core::SceneManager::GetInstance();
        if (!sceneManager) {
            SetError(LC_ERROR_OPERATION_FAILED, "No active scene manager");
            return LC_ERROR_OPERATION_FAILED;
        }
        *out_value = sceneManager->GetGlobalBool(name, default_value);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to get global bool");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_set_global_int(const char* name, int value) {
    if (!name) {
        SetError(LC_ERROR_NULL_POINTER, "name is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        lupine::core::SceneManager* sceneManager = lupine::core::SceneManager::GetInstance();
        if (!sceneManager) {
            SetError(LC_ERROR_OPERATION_FAILED, "No active scene manager");
            return LC_ERROR_OPERATION_FAILED;
        }
        sceneManager->SetGlobalInt(name, value);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to set global int");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_set_global_float(const char* name, float value) {
    if (!name) {
        SetError(LC_ERROR_NULL_POINTER, "name is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        lupine::core::SceneManager* sceneManager = lupine::core::SceneManager::GetInstance();
        if (!sceneManager) {
            SetError(LC_ERROR_OPERATION_FAILED, "No active scene manager");
            return LC_ERROR_OPERATION_FAILED;
        }
        sceneManager->SetGlobalFloat(name, value);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to set global float");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_set_global_string(const char* name, const char* value) {
    if (!name || !value) {
        SetError(LC_ERROR_NULL_POINTER, "name or value is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        lupine::core::SceneManager* sceneManager = lupine::core::SceneManager::GetInstance();
        if (!sceneManager) {
            SetError(LC_ERROR_OPERATION_FAILED, "No active scene manager");
            return LC_ERROR_OPERATION_FAILED;
        }
        sceneManager->SetGlobalString(name, value);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to set global string");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_set_global_bool(const char* name, bool value) {
    if (!name) {
        SetError(LC_ERROR_NULL_POINTER, "name is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        lupine::core::SceneManager* sceneManager = lupine::core::SceneManager::GetInstance();
        if (!sceneManager) {
            SetError(LC_ERROR_OPERATION_FAILED, "No active scene manager");
            return LC_ERROR_OPERATION_FAILED;
        }
        sceneManager->SetGlobalBool(name, value);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to set global bool");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_get_global_value(const char* name, const char* default_json,
                                    char* out_buffer, size_t buffer_size) {
    if (!name) {
        SetError(LC_ERROR_NULL_POINTER, "name is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        lupine::core::SceneManager* sceneManager = lupine::core::SceneManager::GetInstance();
        if (!sceneManager) {
            SetError(LC_ERROR_OPERATION_FAILED, "No active scene manager");
            return LC_ERROR_OPERATION_FAILED;
        }
        nlohmann::json defaultValue;
        if (default_json && default_json[0] != '\0') {
            try {
                defaultValue = nlohmann::json::parse(default_json);
            } catch (...) {
                SetError(LC_ERROR_INVALID_PARAMETER, "default_json is not valid JSON");
                return LC_ERROR_INVALID_PARAMETER;
            }
        }
        const nlohmann::json value = sceneManager->GetGlobalValue(name, defaultValue);
        return CopyStringToBuffer(value.dump(), out_buffer, buffer_size);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to get global value");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_set_global_value(const char* name, const char* value_json) {
    if (!name || !value_json) {
        SetError(LC_ERROR_NULL_POINTER, "name or value_json is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        lupine::core::SceneManager* sceneManager = lupine::core::SceneManager::GetInstance();
        if (!sceneManager) {
            SetError(LC_ERROR_OPERATION_FAILED, "No active scene manager");
            return LC_ERROR_OPERATION_FAILED;
        }
        nlohmann::json value;
        try {
            value = nlohmann::json::parse(value_json);
        } catch (...) {
            SetError(LC_ERROR_INVALID_PARAMETER, "value_json is not valid JSON");
            return LC_ERROR_INVALID_PARAMETER;
        }
        sceneManager->SetGlobalValue(name, value);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to set global value");
        return LC_ERROR_INTERNAL_ERROR;
    }
}
