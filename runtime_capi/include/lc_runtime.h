/**
 * @file lc_runtime.h
 * @brief Lupine Engine C API - Runtime application, window and main loop
 *
 * This header belongs to the OPTIONAL `lupine_runtime_capi` library, a superset of
 * the headless `lupine_capi`: it bundles the full scene/node/component/reflection C
 * API (every lc_* function from lupine_c.h) AND adds the runtime layer below, so a
 * language binding can open a window, choose a graphics backend, load scenes and run
 * (or single-step) the main loop. Consumers that only manipulate scene/logic data
 * headlessly should link the lean `lupine_capi` instead and avoid the SDL2 /
 * graphics-backend dependencies this library pulls in.
 *
 * Link `lupine_runtime_capi` and `#include <lupine_c.h>` (for the data API) plus
 * `#include <lc_runtime.h>` (for this runtime API). Do not link both DLLs at once.
 */

#ifndef LUPINE_CAPI_RUNTIME_H
#define LUPINE_CAPI_RUNTIME_H

#include <core/lc_core.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque handle to a runtime application instance.
 */
typedef struct LCRuntimeApp* LCRuntimeAppHandle;

/**
 * @brief Graphics backend selection (mirrors lupine::GraphicsBackend).
 */
typedef enum LCGraphicsBackend {
    LC_BACKEND_OPENGL    = 0,
    LC_BACKEND_VULKAN    = 1,
    LC_BACKEND_METAL     = 2,
    LC_BACKEND_WEBGL     = 3,
    LC_BACKEND_DIRECTX11 = 4,
    LC_BACKEND_DIRECTX12 = 5,
    LC_BACKEND_NONE      = 6
} LCGraphicsBackend;

/**
 * @brief Runtime configuration (mirrors lupine::RuntimeApp::Config).
 *
 * String fields may be NULL (treated as empty). Populate with sensible defaults
 * via lc_runtime_config_default() then override the fields you care about.
 */
typedef struct LCRuntimeConfig {
    const char* title;           /**< Window title (NULL -> "Lupine Runtime"). */
    int window_width;            /**< Initial window width in pixels. */
    int window_height;           /**< Initial window height in pixels. */
    bool debugging;              /**< Enable debug console/logging. */
    bool vsync;                  /**< Enable vertical sync. */
    bool resizable;              /**< Allow the window to be resized. */
    bool fullscreen;             /**< Start in fullscreen mode. */
    LCGraphicsBackend backend;   /**< Graphics backend to use. */
    const char* project_path;    /**< Optional project file to load (NULL/empty for none). */
    const char* scene_path;      /**< Optional scene to load (requires project_path). */
} LCRuntimeConfig;

/**
 * @brief Fill a config struct with the engine's default values.
 */
LC_API LCResult lc_runtime_config_default(LCRuntimeConfig* out_config);

/**
 * @brief Create a runtime application instance (not yet initialized).
 */
LC_API LCResult lc_runtime_create(LCRuntimeAppHandle* out_app);

/**
 * @brief Destroy a runtime application instance, shutting it down if needed.
 */
LC_API LCResult lc_runtime_destroy(LCRuntimeAppHandle app);

/**
 * @brief Initialize the runtime (creates the window, graphics device and scene
 *        manager) from a configuration. Must be called before run/run_frame.
 * @return LC_ERROR_INITIALIZATION_FAILED if the runtime could not start.
 */
LC_API LCResult lc_runtime_initialize(LCRuntimeAppHandle app, const LCRuntimeConfig* config);

/**
 * @brief Run the blocking main loop until the application exits (window closed or
 *        lc_runtime_stop called).
 */
LC_API LCResult lc_runtime_run(LCRuntimeAppHandle app);

/**
 * @brief Advance the application by a single frame (process events, update,
 *        physics, render). Intended for hosts that own their own loop.
 * @param out_keep_running Receives true while the app should continue, false when
 *        it should exit. May be NULL.
 * @return LC_SUCCESS on a processed frame; an error if the handle is invalid.
 */
LC_API LCResult lc_runtime_run_frame(LCRuntimeAppHandle app, bool* out_keep_running);

/**
 * @brief Process pending window/OS events only (no update or render). Must be
 *        called from the main thread when running asynchronously.
 * @param out_keep_running Receives true while the app should continue. May be NULL.
 */
LC_API LCResult lc_runtime_process_events(LCRuntimeAppHandle app, bool* out_keep_running);

/**
 * @brief Stop the main loop gracefully (causes run()/run_frame() to exit).
 */
LC_API LCResult lc_runtime_stop(LCRuntimeAppHandle app);

/**
 * @brief Shut down the runtime (releases the window and graphics resources). The
 *        handle remains valid for destruction but should not be run again.
 */
LC_API LCResult lc_runtime_shutdown(LCRuntimeAppHandle app);

/**
 * @brief Pause / resume per-frame scene updates (rendering continues).
 */
LC_API LCResult lc_runtime_pause(LCRuntimeAppHandle app);
LC_API LCResult lc_runtime_resume(LCRuntimeAppHandle app);

/**
 * @brief Load a different scene at runtime.
 */
LC_API LCResult lc_runtime_load_scene(LCRuntimeAppHandle app, const char* scene_path);

/**
 * @brief Reload the current scene.
 */
LC_API LCResult lc_runtime_reload_scene(LCRuntimeAppHandle app);

/**
 * @brief Query running / paused state.
 */
LC_API bool lc_runtime_is_running(LCRuntimeAppHandle app);
LC_API bool lc_runtime_is_paused(LCRuntimeAppHandle app);

/**
 * @brief Query / advance splash screens.
 */
LC_API bool lc_runtime_is_showing_splash(LCRuntimeAppHandle app);
LC_API LCResult lc_runtime_skip_splash(LCRuntimeAppHandle app);

/**
 * @brief Get the current window size in pixels.
 */
LC_API LCResult lc_runtime_get_window_size(LCRuntimeAppHandle app, int* out_width, int* out_height);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_RUNTIME_H */
