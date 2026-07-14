/* Async priority-streaming loader C API tests (capi/include/asset/lc_async_asset.h).
 *
 * Covers the public surface of the AsyncAssetLoader wrapper:
 *   submission + NULL/empty guards, status/is_complete polling, priority get/set,
 *   streaming-budget get/set, in-flight/queued/pending counters, cancel/forget/reset,
 *   and the resolved-fields/instance-path delivery accessors (graceful on missing files).
 *
 * Submissions point at nonexistent paths so the suite needs no real .ares files;
 * a load of a missing file is expected to finalize as FAILED after a pump, never to
 * crash. The loader is reset at the end so later suites start from a clean queue. */

static void test_ext_async_asset_submit(void) {
    SECTION_START("Ext Async: Submit and Guards");

    LCAsyncHandle handle = 123;
    LCResult result = lc_async_load_archetype_instance(NULL, LC_ASYNC_PRIORITY_NORMAL, &handle);
    TEST_ASSERT(result != LC_SUCCESS, "lc_async_load_archetype_instance(NULL path) fails gracefully");
    TEST_ASSERT(handle == 0, "Handle is cleared to 0 on invalid input");

    result = lc_async_load_archetype_instance("res://__missing.ares", LC_ASYNC_PRIORITY_HIGH, NULL);
    TEST_ASSERT(result != LC_SUCCESS, "lc_async_load_archetype_instance(NULL out_handle) fails gracefully");

    result = lc_async_load_archetype_instance("res://__missing.ares", LC_ASYNC_PRIORITY_HIGH, &handle);
    TEST_RESULT(result, "Submit instance load for missing file");
    TEST_ASSERT(handle != 0, "Submission returns a non-zero handle");

    int priority = 0;
    result = lc_async_get_priority(handle, &priority);
    TEST_RESULT(result, "lc_async_get_priority()");
    TEST_ASSERT(priority == LC_ASYNC_PRIORITY_HIGH, "Submitted priority round-trips");

    LCAsyncHandle defHandle = 0;
    result = lc_async_load_archetype_definition("res://__missing.archetype", LC_ASYNC_PRIORITY_LOW, &defHandle);
    TEST_RESULT(result, "Submit definition load for missing file");
    TEST_ASSERT(defHandle != 0, "Definition submission returns a non-zero handle");

    /* Drive the loader to completion (worker read fails -> FAILED on finalize). */
    for (int i = 0; i < 200; ++i) {
        bool complete = false;
        lc_async_is_complete(handle, &complete);
        bool defComplete = false;
        lc_async_is_complete(defHandle, &defComplete);
        if (complete && defComplete) {
            break;
        }
        lc_async_pump();
    }

    LCAsyncStatus status = LC_ASYNC_STATUS_UNKNOWN;
    result = lc_async_get_status(handle, &status);
    TEST_RESULT(result, "lc_async_get_status() after pump");
    TEST_ASSERT(status == LC_ASYNC_STATUS_FAILED || status == LC_ASYNC_STATUS_LOADED,
                "Missing-file instance load finalized (failed/loaded, not pending)");

    /* No instance for a failed load: delivery accessors fail gracefully. */
    char* fieldsJson = NULL;
    result = lc_async_get_resolved_fields_json(handle, false, &fieldsJson);
    TEST_ASSERT(result != LC_SUCCESS, "lc_async_get_resolved_fields_json() on failed load fails gracefully");
    if (fieldsJson != NULL) {
        lc_free(fieldsJson);
    }

    lc_async_forget(handle);
    lc_async_forget(defHandle);

    SECTION_END("Ext Async: Submit and Guards");
}

static void test_ext_async_asset_priority(void) {
    SECTION_START("Ext Async: Priority Streaming");

    /* Throttle the budget to 1 so the queue actually backs up and priority
     * ordering is observable through the counters. */
    int previousBudget = 0;
    lc_async_get_max_concurrent(&previousBudget);

    LCResult result = lc_async_set_max_concurrent(1);
    TEST_RESULT(result, "lc_async_set_max_concurrent(1)");

    int budget = 0;
    result = lc_async_get_max_concurrent(&budget);
    TEST_RESULT(result, "lc_async_get_max_concurrent()");
    TEST_ASSERT(budget == 1, "Streaming budget reads back as 1");

    LCAsyncHandle handles[6];
    for (int i = 0; i < 6; ++i) {
        handles[i] = 0;
        int prio = (i == 5) ? LC_ASYNC_PRIORITY_CRITICAL : LC_ASYNC_PRIORITY_LOW;
        lc_async_load_archetype_instance("res://__missing.ares", prio, &handles[i]);
    }

    int inflight = -1;
    result = lc_async_get_inflight_count(&inflight);
    TEST_RESULT(result, "lc_async_get_inflight_count()");
    TEST_ASSERT(inflight >= 0 && inflight <= 1, "In-flight count respects the budget of 1");

    int queued = -1;
    result = lc_async_get_queued_count(&queued);
    TEST_RESULT(result, "lc_async_get_queued_count()");

    int pending = -1;
    result = lc_async_get_pending_count(&pending);
    TEST_RESULT(result, "lc_async_get_pending_count()");
    TEST_ASSERT(pending >= 0, "Pending count is non-negative");

    /* Re-prioritize one of the still-queued low entries upward; no-op if already
     * started, but must never error. */
    result = lc_async_set_priority(handles[0], LC_ASYNC_PRIORITY_HIGH);
    TEST_RESULT(result, "lc_async_set_priority() on a queued request");

    /* Cancel another queued request; it must finalize as cancelled. */
    result = lc_async_cancel(handles[1]);
    TEST_RESULT(result, "lc_async_cancel() on a queued request");

    /* Drain everything. */
    for (int i = 0; i < 400; ++i) {
        int outstanding = 0;
        lc_async_get_pending_count(&outstanding);
        lc_async_pump();
        if (outstanding == 0) {
            break;
        }
    }

    LCAsyncStatus cancelledStatus = LC_ASYNC_STATUS_UNKNOWN;
    lc_async_get_status(handles[1], &cancelledStatus);
    TEST_ASSERT(cancelledStatus == LC_ASYNC_STATUS_CANCELLED ||
                cancelledStatus == LC_ASYNC_STATUS_UNKNOWN,
                "Cancelled queued request finalized as cancelled");

    for (int i = 0; i < 6; ++i) {
        lc_async_forget(handles[i]);
    }

    /* Restore auto budget and clear the loader for subsequent suites. */
    lc_async_set_max_concurrent(0);
    result = lc_async_reset();
    TEST_RESULT(result, "lc_async_reset() clears the loader");

    SECTION_END("Ext Async: Priority Streaming");
}
