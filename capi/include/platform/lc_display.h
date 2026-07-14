/**
 * @file lc_display.h
 * @brief Lupine Engine C API - Window / Display server
 *
 * This header provides window and display control (title, fullscreen, vsync,
 * size, window state, and mouse mode / cursor visibility). When no platform
 * backend is installed every setter is a no-op and getters return the last
 * cached value, so these functions are safe to call in headless contexts.
 */

#ifndef LUPINE_CAPI_DISPLAY_H
#define LUPINE_CAPI_DISPLAY_H

#include "core/lc_core.h"
#include "math/lc_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Mouse Mode
 * ============================================================================ */

/**
 * @brief Mouse interaction mode.
 */
typedef enum {
    LC_MOUSE_MODE_VISIBLE = 0,        /**< Normal cursor, free movement */
    LC_MOUSE_MODE_HIDDEN = 1,         /**< Cursor hidden, free movement */
    LC_MOUSE_MODE_CAPTURED = 2,       /**< Cursor hidden and locked to window centre */
    LC_MOUSE_MODE_CONFINED = 3,       /**< Cursor visible but clamped to the window */
    LC_MOUSE_MODE_CONFINED_HIDDEN = 4 /**< Cursor hidden and clamped to the window */
} LCMouseMode;

/* ============================================================================
 * Window Title
 * ============================================================================ */

/**
 * @brief Set the window title
 * @param title Null-terminated title string
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_display_set_window_title(const char* title);

/**
 * @brief Get the window title
 * @param out_buffer Caller-owned buffer to receive the null-terminated title
 * @param buffer_size Size of out_buffer in bytes
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_display_get_window_title(char* out_buffer, size_t buffer_size);

/* ============================================================================
 * Fullscreen / VSync
 * ============================================================================ */

/**
 * @brief Set borderless desktop fullscreen
 * @param fullscreen true for fullscreen, false for windowed
 * @threadsafety Thread-safe
 */
LC_API void lc_display_set_fullscreen(bool fullscreen);

/**
 * @brief Check whether the window is fullscreen
 * @return true if fullscreen, false otherwise
 * @threadsafety Thread-safe
 */
LC_API bool lc_display_is_fullscreen(void);

/**
 * @brief Enable or disable vertical sync
 * @param enabled true to enable vsync, false to disable
 * @threadsafety Thread-safe
 */
LC_API void lc_display_set_vsync(bool enabled);

/**
 * @brief Check whether vertical sync is enabled
 * @return true if vsync is enabled, false otherwise
 * @threadsafety Thread-safe
 */
LC_API bool lc_display_is_vsync(void);

/* ============================================================================
 * Window Size / State
 * ============================================================================ */

/**
 * @brief Set the window client size in logical pixels
 * @param width New width in pixels
 * @param height New height in pixels
 * @threadsafety Thread-safe
 */
LC_API void lc_display_set_window_size(int width, int height);

/**
 * @brief Get the window client size in logical pixels
 * @param out_size Output parameter for the window size (x = width, y = height)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_display_get_window_size(LCVec2* out_size);

/**
 * @brief Get the primary display resolution in pixels
 * @param out_size Output parameter for the screen size (x = width, y = height)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_display_get_screen_size(LCVec2* out_size);

/**
 * @brief Maximize the window
 * @threadsafety Thread-safe
 */
LC_API void lc_display_maximize_window(void);

/**
 * @brief Minimize the window
 * @threadsafety Thread-safe
 */
LC_API void lc_display_minimize_window(void);

/**
 * @brief Restore the window from maximized/minimized state
 * @threadsafety Thread-safe
 */
LC_API void lc_display_restore_window(void);

/* ============================================================================
 * Mouse Mode / Cursor
 * ============================================================================ */

/**
 * @brief Set the mouse interaction mode
 * @param mode One of the LCMouseMode values
 * @threadsafety Thread-safe
 */
LC_API void lc_display_set_mouse_mode(LCMouseMode mode);

/**
 * @brief Get the current mouse interaction mode
 * @return The current LCMouseMode value
 * @threadsafety Thread-safe
 */
LC_API LCMouseMode lc_display_get_mouse_mode(void);

/**
 * @brief Show or hide the mouse cursor
 * @param visible true to show the cursor, false to hide it
 * @threadsafety Thread-safe
 */
LC_API void lc_display_set_mouse_cursor_visible(bool visible);

/**
 * @brief Check whether the mouse cursor is visible
 * @return true if visible, false otherwise
 * @threadsafety Thread-safe
 */
LC_API bool lc_display_is_mouse_cursor_visible(void);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_DISPLAY_H */
