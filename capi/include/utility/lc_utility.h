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

/**
 * @brief Create a Timer component on a node, configure it, and start it
 * @param owner Node that will own the timer (must not be NULL)
 * @param delay Timer duration (seconds)
 * @param repeating Whether the timer loops
 * @param repeat_count Maximum number of times the timer fires while looping
 *                     (-1 for indefinite); ignored when not repeating
 * @param name Optional component name (can be NULL)
 * @param out_timer Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_timer_create_on_node(LCNodeHandle owner, float delay, bool repeating,
                                        int repeat_count, const char* name,
                                        LCComponentHandle* out_timer);

/**
 * @brief Set the maximum number of times a Timer fires while looping
 * @param component Component handle
 * @param repeat_count Repeat count (-1 for indefinite)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_timer_set_repeat_count(LCComponentHandle component, int repeat_count);

/**
 * @brief Get the maximum number of times a Timer fires while looping
 * @param component Component handle
 * @param out_count Output parameter for the repeat count
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_timer_get_repeat_count(LCComponentHandle component, int* out_count);

/**
 * @brief List the Timer components attached to a node
 * @param owner Node to query
 * @param out_array Output array of component handles (NULL for count-only)
 * @param cap Capacity of out_array
 * @param out_count Receives the total number of Timer components (must not be NULL)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_timer_list(LCNodeHandle owner, LCComponentHandle* out_array,
                              size_t cap, size_t* out_count);

/* ============================================================================
 * Tween Functions
 * ============================================================================ */

/**
 * @brief Create a Tween component on a node, configure it, and start it
 * @param target Node whose value channel will be interpolated (must not be NULL)
 * @param channel Channel name (transform channel or property name)
 * @param to_value_json Target value as JSON (a number or [x,y]/[x,y,z]/[x,y,z,w])
 * @param duration Tween duration (seconds)
 * @param easing Easing curve name (NULL defaults to "linear")
 * @param out_tween Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_tween_create(LCNodeHandle target, const char* channel,
                                const char* to_value_json, float duration,
                                const char* easing, LCComponentHandle* out_tween);

/**
 * @brief List the Tween components attached to a node
 * @param owner Node to query
 * @param out_array Output array of component handles (NULL for count-only)
 * @param cap Capacity of out_array
 * @param out_count Receives the total number of Tween components (must not be NULL)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_tween_list(LCNodeHandle owner, LCComponentHandle* out_array,
                              size_t cap, size_t* out_count);


#endif /* LUPINE_CAPI_UTILITY_H */
