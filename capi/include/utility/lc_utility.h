/**
 * @file lc_utility.h
 * @brief Lupine Engine C API - Utility components (Timer, etc.)
 *
 * This header provides utility components for gameplay logic.
 */

#ifndef LUPINE_CAPI_UTILITY_H
#define LUPINE_CAPI_UTILITY_H

#include "core/lc_core.h"
#include "core/lc_node.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Timer Functions
 * ============================================================================ */

/**
 * @brief Timer timeout callback function pointer
 * @param user_data User data passed when setting the callback
 */
typedef void (*LCTimerTimeoutCallback)(void* user_data);

/**
 * @brief Create a Timer component
 * @param name Optional name for the component (can be NULL)
 * @param out_component Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_timer_create(const char* name, LCComponentHandle* out_component);

/**
 * @brief Start the timer
 * @param component Component handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_timer_start(LCComponentHandle component);

/**
 * @brief Stop the timer
 * @param component Component handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_timer_stop(LCComponentHandle component);

/**
 * @brief Reset the timer
 * @param component Component handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_timer_reset(LCComponentHandle component);

/**
 * @brief Restart the timer (reset + start)
 * @param component Component handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_timer_restart(LCComponentHandle component);

/**
 * @brief Check if timer is running
 * @param component Component handle
 * @param out_running Output parameter for running state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_timer_is_running(LCComponentHandle component, bool* out_running);

/**
 * @brief Check if timer has finished
 * @param component Component handle
 * @param out_finished Output parameter for finished state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_timer_is_finished(LCComponentHandle component, bool* out_finished);

/**
 * @brief Get time remaining until timeout
 * @param component Component handle
 * @param out_time Output parameter for time remaining (seconds)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_timer_get_time_remaining(LCComponentHandle component, float* out_time);

/**
 * @brief Get the duration of a Timer
 * @param component Component handle
 * @param out_duration Output parameter for duration (seconds)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_timer_get_duration(LCComponentHandle component, float* out_duration);

/**
 * @brief Set the duration of a Timer
 * @param component Component handle
 * @param duration Timer duration (seconds)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_timer_set_duration(LCComponentHandle component, float duration);

/**
 * @brief Get the elapsed time of a Timer
 * @param component Component handle
 * @param out_elapsed Output parameter for elapsed time (seconds)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_timer_get_elapsed(LCComponentHandle component, float* out_elapsed);

/**
 * @brief Set the elapsed time of a Timer
 * @param component Component handle
 * @param elapsed Elapsed time (seconds)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_timer_set_elapsed(LCComponentHandle component, float elapsed);

/**
 * @brief Check if a Timer loops
 * @param component Component handle
 * @param out_loop Output parameter for loop state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_timer_get_loop(LCComponentHandle component, bool* out_loop);

/**
 * @brief Set whether a Timer loops
 * @param component Component handle
 * @param loop Loop state (restart automatically when done)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_timer_set_loop(LCComponentHandle component, bool loop);

/**
 * @brief Check if a Timer auto-starts
 * @param component Component handle
 * @param out_auto_start Output parameter for auto-start state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_timer_get_auto_start(LCComponentHandle component, bool* out_auto_start);

/**
 * @brief Set whether a Timer auto-starts
 * @param component Component handle
 * @param auto_start Auto-start state (starts when component becomes active)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_timer_set_auto_start(LCComponentHandle component, bool auto_start);

/**
 * @brief Check if a Timer ignores time scale
 * @param component Component handle
 * @param out_ignore Output parameter for ignore time scale state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_timer_get_ignore_time_scale(LCComponentHandle component, bool* out_ignore);

/**
 * @brief Set whether a Timer ignores time scale
 * @param component Component handle
 * @param ignore Ignore time scale (uses unscaled real time)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_timer_set_ignore_time_scale(LCComponentHandle component, bool ignore);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_UTILITY_H */
