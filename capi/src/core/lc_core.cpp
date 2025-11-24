

#include "core/lc_core.h"

#include <lupine\core\Core.hpp>
#include <lupine\logger\Logger.hpp>

#include <string>
#include <mutex>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <atomic>

namespace {

std::atomic<bool> g_initialized{false};
std::mutex g_initMutex;

thread_local LCResult g_lastErrorCode = LC_SUCCESS;
thread_local char g_lastErrorMessage[512] = {0};

constexpr const char* g_capiVersion = "1.0.0";
constexpr const char* g_engineVersion = "0.1.0";

void SetError(LCResult code, const char* message) {
    g_lastErrorCode = code;
    if (message) {
        std::strncpy(g_lastErrorMessage, message, sizeof(g_lastErrorMessage) - 1);
        g_lastErrorMessage[sizeof(g_lastErrorMessage) - 1] = '\0';
    } else {
        g_lastErrorMessage[0] = '\0';
    }
}

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

extern "C" {

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

LC_API void lc_free(void* ptr) {
    if (ptr) {
        free(ptr);
    }
}

}
