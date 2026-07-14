/**
 * @file lc_project.h
 * @brief Lupine Engine C API - Project and ProjectSettings access
 *
 * This header exposes lupine::core::Project (a .lupine project file) and its
 * lupine::core::ProjectSettings. Foreign-language bindings use these to load a
 * project, read or modify its settings, save it back, and query its directory
 * layout (scenes/assets/scripts).
 *
 * A project is referenced by an opaque LCProjectHandle obtained from
 * lc_project_create / lc_project_create_from_path and released with
 * lc_project_destroy.
 *
 * Functions with a `char** out_*` parameter heap-allocate a NUL-terminated
 * UTF-8 string; the caller MUST free it with lc_free().
 */

#ifndef LUPINE_CAPI_PROJECT_H
#define LUPINE_CAPI_PROJECT_H

#include "core/lc_core.h"
#include "math/lc_math.h"

/**
 * @brief Opaque handle to a Project instance.
 */
typedef struct LCProject* LCProjectHandle;

/* ============================================================================
 * Settings enumerations (mirror lupine::core::ProjectSettings enums)
 * ============================================================================ */

typedef enum LCScaleMode {
    LC_SCALE_MODE_PIXEL_PERFECT = 0,
    LC_SCALE_MODE_SCALE_WITH_SCREEN_SIZE = 1,
    LC_SCALE_MODE_CONSTANT_PHYSICAL_SIZE = 2
} LCScaleMode;

typedef enum LCViewportScaleMode {
    LC_VIEWPORT_SCALE_MODE_LETTERBOX = 0,
    LC_VIEWPORT_SCALE_MODE_STRETCH = 1,
    LC_VIEWPORT_SCALE_MODE_CROP = 2,
    LC_VIEWPORT_SCALE_MODE_IGNORE = 3
} LCViewportScaleMode;

typedef enum LCTextureFiltering {
    LC_TEXTURE_FILTERING_NEAREST = 0,
    LC_TEXTURE_FILTERING_BILINEAR = 1,
    LC_TEXTURE_FILTERING_CUBIC = 2
} LCTextureFiltering;

/* ============================================================================
 * Project lifecycle
 * ============================================================================ */

/**
 * @brief Create a new, empty (unloaded) Project.
 * @param out_project Output; receives the new project handle
 * @return LC_SUCCESS or error code
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_project_create(LCProjectHandle* out_project);

/**
 * @brief Create a Project bound to a path and load it immediately.
 * @param project_path OS path to the .lupine project file
 * @param out_project Output; receives the new project handle
 * @return LC_SUCCESS, or LC_ERROR_FILE_NOT_FOUND if the file could not be loaded
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_project_create_from_path(const char* project_path, LCProjectHandle* out_project);

/**
 * @brief Destroy a project handle and release its resources.
 * @param project Project handle
 * @return LC_SUCCESS or LC_ERROR_INVALID_HANDLE
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_project_destroy(LCProjectHandle project);

/**
 * @brief Load a project from a .lupine file into an existing handle.
 * @param project Project handle
 * @param project_path OS path to the .lupine file
 * @return LC_SUCCESS or LC_ERROR_FILE_NOT_FOUND
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_project_load(LCProjectHandle project, const char* project_path);

/**
 * @brief Save the project back to its current path.
 * @param project Project handle
 * @return LC_SUCCESS or LC_ERROR_OPERATION_FAILED
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_project_save(LCProjectHandle project);

/**
 * @brief Save the project to a new path (and adopt it as the current path).
 * @param project Project handle
 * @param project_path OS path to save to
 * @return LC_SUCCESS or LC_ERROR_OPERATION_FAILED
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_project_save_as(LCProjectHandle project, const char* project_path);

/**
 * @brief Check whether the project is loaded.
 * @param project Project handle
 * @param out_loaded Output; receives true if loaded
 * @return LC_SUCCESS or LC_ERROR_INVALID_HANDLE
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_project_is_loaded(LCProjectHandle project, bool* out_loaded);

/* ============================================================================
 * Project paths
 * ============================================================================ */

LC_API LCResult lc_project_get_path(LCProjectHandle project, char** out_path);
LC_API LCResult lc_project_get_directory(LCProjectHandle project, char** out_path);
LC_API LCResult lc_project_get_scenes_directory(LCProjectHandle project, char** out_path);
LC_API LCResult lc_project_get_assets_directory(LCProjectHandle project, char** out_path);
LC_API LCResult lc_project_get_scripts_directory(LCProjectHandle project, char** out_path);

/* ============================================================================
 * Settings - metadata strings
 * ============================================================================ */

LC_API LCResult lc_project_get_name(LCProjectHandle project, char** out_value);
LC_API LCResult lc_project_set_name(LCProjectHandle project, const char* value);
LC_API LCResult lc_project_get_creator_name(LCProjectHandle project, char** out_value);
LC_API LCResult lc_project_set_creator_name(LCProjectHandle project, const char* value);
LC_API LCResult lc_project_get_version(LCProjectHandle project, char** out_value);
LC_API LCResult lc_project_set_version(LCProjectHandle project, const char* value);
LC_API LCResult lc_project_get_main_scene(LCProjectHandle project, char** out_value);
LC_API LCResult lc_project_set_main_scene(LCProjectHandle project, const char* value);
LC_API LCResult lc_project_get_default_font(LCProjectHandle project, char** out_value);
LC_API LCResult lc_project_set_default_font(LCProjectHandle project, const char* value);
LC_API LCResult lc_project_get_default_theme(LCProjectHandle project, char** out_value);
LC_API LCResult lc_project_set_default_theme(LCProjectHandle project, const char* value);

/* ============================================================================
 * Settings - display / rendering
 * ============================================================================ */

LC_API LCResult lc_project_get_window_width(LCProjectHandle project, int* out_value);
LC_API LCResult lc_project_set_window_width(LCProjectHandle project, int value);
LC_API LCResult lc_project_get_window_height(LCProjectHandle project, int* out_value);
LC_API LCResult lc_project_set_window_height(LCProjectHandle project, int value);
/* Window-size override: when enabled the OS window opens at the override size
 * below instead of the design resolution (window_width/window_height). */
LC_API LCResult lc_project_get_window_size_override(LCProjectHandle project, bool* out_value);
LC_API LCResult lc_project_set_window_size_override(LCProjectHandle project, bool value);
LC_API LCResult lc_project_get_window_size_override_width(LCProjectHandle project, int* out_value);
LC_API LCResult lc_project_set_window_size_override_width(LCProjectHandle project, int value);
LC_API LCResult lc_project_get_window_size_override_height(LCProjectHandle project, int* out_value);
LC_API LCResult lc_project_set_window_size_override_height(LCProjectHandle project, int value);
LC_API LCResult lc_project_get_fullscreen(LCProjectHandle project, bool* out_value);
LC_API LCResult lc_project_set_fullscreen(LCProjectHandle project, bool value);
LC_API LCResult lc_project_get_vsync(LCProjectHandle project, bool* out_value);
LC_API LCResult lc_project_set_vsync(LCProjectHandle project, bool value);
LC_API LCResult lc_project_get_target_frame_rate(LCProjectHandle project, int* out_value);
LC_API LCResult lc_project_set_target_frame_rate(LCProjectHandle project, int value);
LC_API LCResult lc_project_get_clear_color(LCProjectHandle project, LCColor* out_value);
LC_API LCResult lc_project_set_clear_color(LCProjectHandle project, LCColor value);
LC_API LCResult lc_project_get_max_undo_steps(LCProjectHandle project, int* out_value);
LC_API LCResult lc_project_set_max_undo_steps(LCProjectHandle project, int value);

/* ============================================================================
 * Settings - scale / viewport / filtering
 * ============================================================================ */

LC_API LCResult lc_project_get_reference_resolution(LCProjectHandle project, LCVec2* out_value);
LC_API LCResult lc_project_set_reference_resolution(LCProjectHandle project, LCVec2 value);
LC_API LCResult lc_project_get_scale_mode(LCProjectHandle project, LCScaleMode* out_value);
LC_API LCResult lc_project_set_scale_mode(LCProjectHandle project, LCScaleMode value);
LC_API LCResult lc_project_get_viewport_scale_mode(LCProjectHandle project, LCViewportScaleMode* out_value);
LC_API LCResult lc_project_set_viewport_scale_mode(LCProjectHandle project, LCViewportScaleMode value);
LC_API LCResult lc_project_get_texture_filtering(LCProjectHandle project, LCTextureFiltering* out_value);
LC_API LCResult lc_project_set_texture_filtering(LCProjectHandle project, LCTextureFiltering value);

/* ============================================================================
 * Settings - physics
 * ============================================================================ */

LC_API LCResult lc_project_get_physics_tick_rate(LCProjectHandle project, float* out_value);
LC_API LCResult lc_project_set_physics_tick_rate(LCProjectHandle project, float value);
LC_API LCResult lc_project_get_gravity_2d(LCProjectHandle project, LCVec2* out_value);
LC_API LCResult lc_project_set_gravity_2d(LCProjectHandle project, LCVec2 value);
LC_API LCResult lc_project_get_gravity_3d(LCProjectHandle project, LCVec3* out_value);
LC_API LCResult lc_project_set_gravity_3d(LCProjectHandle project, LCVec3 value);

/* ============================================================================
 * Settings - splash screens
 * ============================================================================ */

LC_API LCResult lc_project_get_splash_enabled(LCProjectHandle project, bool* out_value);
LC_API LCResult lc_project_set_splash_enabled(LCProjectHandle project, bool value);
LC_API LCResult lc_project_get_splash_allow_skip(LCProjectHandle project, bool* out_value);
LC_API LCResult lc_project_set_splash_allow_skip(LCProjectHandle project, bool value);

/**
 * @brief Get the number of configured splash-screen entries.
 * @param project Project handle
 * @param out_count Output; receives the entry count
 * @return LC_SUCCESS or LC_ERROR_INVALID_HANDLE
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_project_get_splash_entry_count(LCProjectHandle project, int* out_count);

/**
 * @brief Read a splash-screen entry by index.
 * @param project Project handle
 * @param index Zero-based entry index
 * @param out_image_path Output; receives a heap string freed with lc_free (can be NULL)
 * @param out_fade_in Output; receives fade-in duration in seconds (can be NULL)
 * @param out_hold Output; receives hold duration in seconds (can be NULL)
 * @param out_fade_out Output; receives fade-out duration in seconds (can be NULL)
 * @return LC_SUCCESS, LC_ERROR_INVALID_HANDLE, or LC_ERROR_INVALID_PARAMETER
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_project_get_splash_entry(LCProjectHandle project, int index,
                                            char** out_image_path, float* out_fade_in,
                                            float* out_hold, float* out_fade_out);

/**
 * @brief Append a splash-screen entry.
 * @param project Project handle
 * @param image_path res:// path to the splash image
 * @param fade_in Fade-in duration in seconds
 * @param hold Hold duration in seconds
 * @param fade_out Fade-out duration in seconds
 * @return LC_SUCCESS or LC_ERROR_INVALID_HANDLE
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_project_add_splash_entry(LCProjectHandle project, const char* image_path,
                                            float fade_in, float hold, float fade_out);

/**
 * @brief Remove all splash-screen entries.
 * @param project Project handle
 * @return LC_SUCCESS or LC_ERROR_INVALID_HANDLE
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_project_clear_splash_entries(LCProjectHandle project);

#endif /* LUPINE_CAPI_PROJECT_H */
