/**
 * @file lc_profiler.h
 * @brief Lupine Engine C API - Profiler instrumentation
 *
 * Wraps the engine's profiling::Profiler singleton: enable/disable capture,
 * configure history depth, query the most-recent frame breakdown and aggregated
 * timings, and export a capture to disk. Snapshot queries return heap JSON
 * strings the caller frees with lc_free().
 */

#ifndef LUPINE_CAPI_PROFILER_H
#define LUPINE_CAPI_PROFILER_H

#include "../core/lc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Enable or disable profiler capture.
 * @threadsafety Thread-safe
 */
LC_API void lc_profiler_set_enabled(bool enabled);

/**
 * @brief Query whether profiler capture is currently enabled.
 * @threadsafety Thread-safe
 */
LC_API bool lc_profiler_is_enabled(void);

/**
 * @brief Set the number of frames retained in the history ring buffer.
 * @threadsafety Thread-safe
 */
LC_API void lc_profiler_set_history_size(uint32_t frames);

/**
 * @brief Discard all captured history.
 * @threadsafety Thread-safe
 */
LC_API void lc_profiler_clear(void);

/**
 * @brief Open a custom timing zone on the current frame.
 *
 * Must be paired with lc_profiler_end_zone(). No-op when capture is disabled or
 * no frame is open. Categories are matched case-insensitively against the engine
 * zone categories ("User", "Update", "Render", ...); unknown names map to "User".
 * @threadsafety Thread-safe
 */
LC_API void lc_profiler_begin_zone(const char* name, const char* category);

/**
 * @brief Close the most-recently opened custom zone.
 * @threadsafety Thread-safe
 */
LC_API void lc_profiler_end_zone(void);

/**
 * @brief Average frame time over the history window, in milliseconds.
 * @threadsafety Thread-safe
 */
LC_API double lc_profiler_average_frame_ms(void);

/**
 * @brief Frames-per-second derived from the average frame time.
 * @threadsafety Thread-safe
 */
LC_API double lc_profiler_fps(void);

/**
 * @brief Serialize the entire history window to a JSON string.
 * @param out_json Output; receives a heap JSON string freed with lc_free.
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_profiler_get_history_json(char** out_json);

/**
 * @brief Serialize the most-recent completed frame to a JSON string.
 * @param out_json Output; receives a heap JSON string freed with lc_free. Empty
 *                 object string when no frame has completed.
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_profiler_get_last_frame_json(char** out_json);

/**
 * @brief Save the current history window to a .lprof (JSON) file.
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_profiler_save_capture(const char* path);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_PROFILER_H */
