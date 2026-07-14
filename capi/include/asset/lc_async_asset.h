/**
 * @file lc_async_asset.h
 * @brief C API for the Lupine asynchronous priority-streaming asset loader.
 *
 * Wraps the engine's AsyncAssetLoader: archetype instances (.ares) and archetype
 * definitions (.archetype/.lua/.rb/.py) are read and parsed on background worker
 * threads, then finalized on the main thread when lc_async_pump() is called (the
 * engine pumps automatically each frame; a standalone host drives it manually).
 *
 * Priority streaming: each request carries a priority (higher = streamed in
 * first) and the loader only runs up to a configurable number concurrently (the
 * streaming budget). A high-priority request submitted while many lower-priority
 * ones are still queued jumps ahead of them. Requests that have already started
 * on a worker are never preempted; priority only governs queue order.
 *
 * Requests are identified by an opaque 64-bit handle. The loader retains each
 * request's record (status + result) until lc_async_forget() (or the implicit
 * forget that lc_async_get_resolved_fields_json() can perform). Functions that
 * emit a newly-allocated string to an out parameter transfer ownership to the
 * caller, who MUST release it with lc_free() (see lc_core.h).
 */

#ifndef LUPINE_CAPI_ASYNC_ASSET_H
#define LUPINE_CAPI_ASYNC_ASSET_H

#include "../core/lc_core.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque async-load request handle (0 is never a valid handle). */
typedef uint64_t LCAsyncHandle;

/**
 * @brief Status of an async load request (mirrors asset::AsyncLoadStatus).
 */
typedef enum LCAsyncStatus {
    LC_ASYNC_STATUS_UNKNOWN   = 0, /**< No such handle (never issued / forgotten) */
    LC_ASYNC_STATUS_PENDING   = 1, /**< Queued or loading on a worker thread */
    LC_ASYNC_STATUS_LOADED    = 2, /**< Completed successfully; result available */
    LC_ASYNC_STATUS_FAILED    = 3, /**< Completed with a read/parse/registration error */
    LC_ASYNC_STATUS_CANCELLED = 4  /**< Cancelled before completion */
} LCAsyncStatus;

/**
 * @brief Standard streaming priority bands (mirror asset::AsyncLoadPriority).
 * Priority is a plain int; these name the common values. Higher = serviced
 * first.
 */
#define LC_ASYNC_PRIORITY_LOW      (-100)
#define LC_ASYNC_PRIORITY_NORMAL   (0)
#define LC_ASYNC_PRIORITY_HIGH     (100)
#define LC_ASYNC_PRIORITY_CRITICAL (1000)

/* ======================================================================== */
/* Submission                                                               */
/* ======================================================================== */

/**
 * Begin loading an archetype instance (.ares) by res:// (or physical) path.
 * @param res_path  Path to the .ares file.
 * @param priority  Streaming priority (see LC_ASYNC_PRIORITY_*).
 * @param out_handle Receives the request handle (>0). 0 is written on empty input.
 * @return LC_SUCCESS, or LC_ERROR_INVALID_PARAMETER if res_path is NULL/empty.
 */
LC_API LCResult lc_async_load_archetype_instance(const char* res_path, int priority,
                                                 LCAsyncHandle* out_handle);

/**
 * Begin loading and registering an archetype definition file.
 * @param file_path Path to the .archetype/.lua/.rb/.py definition source.
 * @param priority  Streaming priority (see LC_ASYNC_PRIORITY_*).
 * @param out_handle Receives the request handle (>0). 0 on empty input.
 * @return LC_SUCCESS, or LC_ERROR_INVALID_PARAMETER if file_path is NULL/empty.
 */
LC_API LCResult lc_async_load_archetype_definition(const char* file_path, int priority,
                                                   LCAsyncHandle* out_handle);

/* ======================================================================== */
/* Main-thread pump                                                         */
/* ======================================================================== */

/**
 * Finalize any completed background work and run completion bookkeeping. Call
 * once per frame on the main thread. The engine does this automatically; a
 * standalone C host must call it to make loads progress to LOADED/FAILED.
 */
LC_API LCResult lc_async_pump(void);

/* ======================================================================== */
/* Status / results                                                         */
/* ======================================================================== */

/** Query the status of a request. */
LC_API LCResult lc_async_get_status(LCAsyncHandle handle, LCAsyncStatus* out_status);

/** True once a request has finished (loaded/failed/cancelled, or is unknown). */
LC_API LCResult lc_async_is_complete(LCAsyncHandle handle, bool* out_complete);

/**
 * The archetype class name parsed for a request (instance or definition).
 * Writes into @p buffer (NUL-terminated, truncated to max_length). Empty until
 * the request finalizes, or if it failed.
 */
LC_API LCResult lc_async_get_class_name(LCAsyncHandle handle, char* buffer, uint32_t max_length);

/**
 * The resolved field values of a completed instance request, as a JSON object.
 * Writes a newly allocated string to *out_json (free with lc_free).
 * @param forget If true, the loader record is released after delivery (the
 *               instance keeps the data alive only as long as the engine caches
 *               it elsewhere); if false, the record is retained for re-query.
 * @return LC_ERROR_NOT_FOUND if the request is not LOADED or is a definition.
 */
LC_API LCResult lc_async_get_resolved_fields_json(LCAsyncHandle handle, bool forget,
                                                  char** out_json);

/**
 * The res:// path the loaded instance resolved to. Writes into @p buffer.
 * @return LC_ERROR_NOT_FOUND if the request is not a LOADED instance.
 */
LC_API LCResult lc_async_get_instance_path(LCAsyncHandle handle, char* buffer, uint32_t max_length);

/* ======================================================================== */
/* Priority                                                                 */
/* ======================================================================== */

/** Re-prioritize a still-queued request (no-op once it has started/finished). */
LC_API LCResult lc_async_set_priority(LCAsyncHandle handle, int priority);

/** The priority a request was issued / last set with (0 if unknown). */
LC_API LCResult lc_async_get_priority(LCAsyncHandle handle, int* out_priority);

/* ======================================================================== */
/* Streaming budget / diagnostics                                           */
/* ======================================================================== */

/**
 * Set the streaming budget: the maximum number of requests loading on worker
 * threads at once. Pass 0 to track the worker thread count (the default).
 */
LC_API LCResult lc_async_set_max_concurrent(int max_concurrent);

/** The effective streaming budget (resolves 0 = auto to the thread count). */
LC_API LCResult lc_async_get_max_concurrent(int* out_max);

/** Number of requests currently loading on a worker thread. */
LC_API LCResult lc_async_get_inflight_count(int* out_count);

/** Number of requests queued but not yet dispatched to a worker. */
LC_API LCResult lc_async_get_queued_count(int* out_count);

/** Number of requests that have not yet been finalized (in-flight + queued). */
LC_API LCResult lc_async_get_pending_count(int* out_count);

/* ======================================================================== */
/* Lifecycle                                                                */
/* ======================================================================== */

/** Cancel a request. Still-pending work is discarded; the record stays queryable. */
LC_API LCResult lc_async_cancel(LCAsyncHandle handle);

/** Drop the retained record for a handle (cancels first if still pending). */
LC_API LCResult lc_async_forget(LCAsyncHandle handle);

/** Cancel everything and clear all records; blocks until workers drain. */
LC_API LCResult lc_async_reset(void);

/* ======================================================================== */
/* Synchronous archetype access                                             */
/* ======================================================================== */

/**
 * Load an archetype instance (.ares) synchronously on the calling thread and
 * emit its resolved field values as a JSON object string. This produces the
 * same field data as lc_async_get_resolved_fields_json, but without the async
 * loader / pump machinery.
 * @param res_path  Path to the .ares file (res:// or physical).
 * @param out_fields_json Receives a newly allocated JSON string (free with lc_free).
 * @return LC_SUCCESS, LC_ERROR_INVALID_PARAMETER if res_path is NULL/empty, or
 *         LC_ERROR_OPERATION_FAILED if the instance could not be loaded.
 */
LC_API LCResult lc_archetype_load_instance(const char* res_path, char** out_fields_json);

/**
 * Invalidate every cached archetype the scripting layer has resolved, forcing a
 * fresh load on the next access (use after editing archetype definitions on disk).
 */
LC_API LCResult lc_archetype_invalidate_caches(void);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_ASYNC_ASSET_H */
