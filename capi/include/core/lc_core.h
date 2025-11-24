/**
 * @file lc_core.h
 * @brief Lupine Engine C API - Core functionality
 *
 * This header provides core initialization, shutdown, error handling,
 * logging, and version information for the Lupine Engine C API.
 */

#ifndef LUPINE_CAPI_CORE_H
#define LUPINE_CAPI_CORE_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Platform-specific export macros
 * ============================================================================ */

#if defined(_WIN32) || defined(_WIN64)
    #ifdef LUPINE_CAPI_EXPORT
        #define LC_API __declspec(dllexport)
    #elif defined(LUPINE_CAPI_DLL)
        #define LC_API __declspec(dllimport)
    #else
        #define LC_API
    #endif
#else
    #ifdef LUPINE_CAPI_EXPORT
        #define LC_API __attribute__((visibility("default")))
    #else
        #define LC_API
    #endif
#endif

/* ============================================================================
 * Version Information
 * ============================================================================ */

#define LC_VERSION_MAJOR 1
#define LC_VERSION_MINOR 0
#define LC_VERSION_PATCH 0

/**
 * @brief Get the C API version
 * @param major Output parameter for major version (can be NULL)
 * @param minor Output parameter for minor version (can be NULL)
 * @param patch Output parameter for patch version (can be NULL)
 * @threadsafety Thread-safe
 */
LC_API void lc_get_version(int* major, int* minor, int* patch);

/**
 * @brief Get the C API version as a string
 * @return Version string (e.g., "1.0.0") - owned by engine, do not free
 * @threadsafety Thread-safe
 */
LC_API const char* lc_get_version_string(void);

/**
 * @brief Get the underlying Lupine Engine version
 * @return Engine version string - owned by engine, do not free
 * @threadsafety Thread-safe
 */
LC_API const char* lc_get_engine_version(void);

/* ============================================================================
 * Result Codes and Error Handling
 * ============================================================================ */

/**
 * @brief Result codes for C API functions
 */
typedef enum LCResult {
    LC_SUCCESS = 0,                          /**< Operation succeeded */

    /* General errors (-1 to -99) */
    LC_ERROR_INVALID_HANDLE = -1,            /**< Invalid or null handle */
    LC_ERROR_NULL_POINTER = -2,              /**< Null pointer argument */
    LC_ERROR_INVALID_PARAMETER = -3,         /**< Invalid parameter value */
    LC_ERROR_OUT_OF_MEMORY = -4,             /**< Memory allocation failed */
    LC_ERROR_NOT_INITIALIZED = -5,           /**< Engine not initialized */
    LC_ERROR_ALREADY_INITIALIZED = -6,       /**< Engine already initialized */
    LC_ERROR_INITIALIZATION_FAILED = -7,     /**< Initialization failed */
    LC_ERROR_SHUTDOWN_FAILED = -8,           /**< Shutdown failed */
    LC_ERROR_INTERNAL_ERROR = -9,            /**< Internal engine error */
    LC_ERROR_NOT_IMPLEMENTED = -10,          /**< Feature not implemented */
    LC_ERROR_OPERATION_FAILED = -11,         /**< Generic operation failure */

    /* File/IO errors (-100 to -199) */
    LC_ERROR_FILE_NOT_FOUND = -100,          /**< File not found */
    LC_ERROR_FILE_READ_FAILED = -101,        /**< File read failed */
    LC_ERROR_FILE_WRITE_FAILED = -102,       /**< File write failed */
    LC_ERROR_FILE_INVALID_FORMAT = -103,     /**< Invalid file format */
    LC_ERROR_PATH_INVALID = -104,            /**< Invalid path */

    /* Graphics errors (-200 to -299) */
    LC_ERROR_GFX_DEVICE_CREATION_FAILED = -200,  /**< Graphics device creation failed */
    LC_ERROR_GFX_INVALID_BACKEND = -201,         /**< Invalid graphics backend */
    LC_ERROR_GFX_RESOURCE_CREATION_FAILED = -202, /**< Graphics resource creation failed */
    LC_ERROR_GFX_INVALID_RESOURCE = -203,        /**< Invalid graphics resource */

    /* Scene/Node errors (-300 to -399) */
    LC_ERROR_NODE_INVALID_TYPE = -300,       /**< Invalid node type */
    LC_ERROR_NODE_INVALID_PARENT = -301,     /**< Invalid parent node */
    LC_ERROR_NODE_CIRCULAR_HIERARCHY = -302, /**< Circular node hierarchy */
    LC_ERROR_SCENE_INVALID = -303,           /**< Invalid scene */
    LC_ERROR_SCENE_LOAD_FAILED = -304,       /**< Scene load failed */
    LC_ERROR_SCENE_SAVE_FAILED = -305,       /**< Scene save failed */

    /* Component errors (-400 to -499) */
    LC_ERROR_COMPONENT_INVALID_TYPE = -400,  /**< Invalid component type */
    LC_ERROR_COMPONENT_NOT_FOUND = -401,     /**< Component not found */
    LC_ERROR_COMPONENT_ALREADY_EXISTS = -402, /**< Component already exists */
    LC_ERROR_TYPE_MISMATCH = -403,           /**< Component type mismatch */

    /* Physics errors (-500 to -599) */
    LC_ERROR_PHYSICS_WORLD_INVALID = -500,   /**< Invalid physics world */
    LC_ERROR_PHYSICS_BODY_INVALID = -501,    /**< Invalid physics body */
    LC_ERROR_PHYSICS_COLLIDER_INVALID = -502, /**< Invalid collider */

    /* Audio errors (-600 to -699) */
    LC_ERROR_AUDIO_INIT_FAILED = -600,       /**< Audio initialization failed */
    LC_ERROR_AUDIO_ASSET_LOAD_FAILED = -601, /**< Audio asset load failed */
    LC_ERROR_AUDIO_PLAYBACK_FAILED = -602,   /**< Audio playback failed */

    /* Asset errors (-700 to -799) */
    LC_ERROR_ASSET_LOAD_FAILED = -700,       /**< Asset load failed */
    LC_ERROR_ASSET_INVALID_TYPE = -701,      /**< Invalid asset type */
    LC_ERROR_ASSET_NOT_FOUND = -702,         /**< Asset not found */

} LCResult;

/**
 * @brief Get the last error message
 * @return Error message string - owned by engine, do not free.
 *         Valid until next API call on the same thread.
 * @threadsafety Thread-safe (thread-local storage)
 */
LC_API const char* lc_get_last_error(void);

/**
 * @brief Get the last error code
 * @return Last error code from LCResult enum
 * @threadsafety Thread-safe (thread-local storage)
 */
LC_API LCResult lc_get_last_error_code(void);

/**
 * @brief Clear the last error
 * @threadsafety Thread-safe (thread-local storage)
 */
LC_API void lc_clear_last_error(void);

/**
 * @brief Convert result code to string description
 * @param result Result code to convert
 * @return String description - owned by engine, do not free
 * @threadsafety Thread-safe
 */
LC_API const char* lc_result_to_string(LCResult result);

/* ============================================================================
 * Logging
 * ============================================================================ */

/**
 * @brief Log level enumeration
 */
typedef enum LCLogLevel {
    LC_LOG_TRACE = 0,      /**< Trace-level logging (most verbose) */
    LC_LOG_DEBUG = 1,      /**< Debug-level logging */
    LC_LOG_INFO = 2,       /**< Info-level logging */
    LC_LOG_WARN = 3,       /**< Warning-level logging */
    LC_LOG_ERROR = 4,      /**< Error-level logging */
    LC_LOG_CRITICAL = 5,   /**< Critical-level logging (least verbose) */
    LC_LOG_OFF = 6         /**< Disable logging */
} LCLogLevel;

/**
 * @brief Set the global log level
 * @param level Minimum log level to display
 * @threadsafety Thread-safe
 */
LC_API void lc_log_set_level(LCLogLevel level);

/**
 * @brief Get the current global log level
 * @return Current log level
 * @threadsafety Thread-safe
 */
LC_API LCLogLevel lc_log_get_level(void);

/**
 * @brief Log a message
 * @param level Log level
 * @param message Message to log (null-terminated string)
 * @threadsafety Thread-safe
 */
LC_API void lc_log(LCLogLevel level, const char* message);

/**
 * @brief Log a formatted message (printf-style)
 * @param level Log level
 * @param format Format string (printf-style)
 * @param ... Variable arguments for format string
 * @threadsafety Thread-safe
 */
LC_API void lc_log_format(LCLogLevel level, const char* format, ...);

/**
 * @brief Convenience functions for logging at specific levels
 */
LC_API void lc_log_trace(const char* message);
LC_API void lc_log_debug(const char* message);
LC_API void lc_log_info(const char* message);
LC_API void lc_log_warn(const char* message);
LC_API void lc_log_error(const char* message);
LC_API void lc_log_critical(const char* message);

/* ============================================================================
 * Initialization and Shutdown
 * ============================================================================ */

/**
 * @brief Initialize the Lupine Engine core systems
 *
 * This must be called before any other Lupine API functions.
 * Can be called multiple times (subsequent calls are no-ops).
 *
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe (internally synchronized)
 */
LC_API LCResult lc_init(void);

/**
 * @brief Shutdown the Lupine Engine core systems
 *
 * After calling this, you must call lc_init again before using
 * any other Lupine API functions. All handles become invalid.
 *
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe (internally synchronized)
 */
LC_API LCResult lc_shutdown(void);

/**
 * @brief Check if the engine is initialized
 * @return true if initialized, false otherwise
 * @threadsafety Thread-safe
 */
LC_API bool lc_is_initialized(void);

/* ============================================================================
 * Memory Utilities
 * ============================================================================ */

/**
 * @brief Free memory allocated by the C API
 *
 * Use this to free arrays or strings allocated by the engine
 * (when documented that the caller owns the memory).
 *
 * @param ptr Pointer to memory to free (can be NULL)
 * @threadsafety Thread-safe
 */
LC_API void lc_free(void* ptr);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_CORE_H */
