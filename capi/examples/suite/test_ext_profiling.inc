/* Profiler C API tests (capi/include/profiling/lc_profiler.h).
 * Headless: there is no RuntimeApp frame loop, so BeginFrame/EndFrame are not
 * driven here. Instead we exercise the public surface that is observable without
 * a running game: enable/disable round-trip, history sizing, clear, custom zone
 * begin/end as no-ops while no frame is open, JSON export shape, and capture
 * save. The JSON is parsed loosely (substring checks) to avoid a JSON dependency
 * in the C harness. */

static void test_ext_profiler_toggle(void) {
    SECTION_START("Profiler: Enable / Disable");

    lc_profiler_set_enabled(true);
    TEST_ASSERT(lc_profiler_is_enabled(), "lc_profiler_set_enabled(true) sticks");

    lc_profiler_set_enabled(false);
    TEST_ASSERT(!lc_profiler_is_enabled(), "lc_profiler_set_enabled(false) sticks");

    lc_profiler_set_enabled(true);
    TEST_ASSERT(lc_profiler_is_enabled(), "re-enable sticks");
}

static void test_ext_profiler_history(void) {
    SECTION_START("Profiler: History / Clear");

    lc_profiler_set_history_size(120);
    lc_profiler_clear();

    char* json = NULL;
    LCResult r = lc_profiler_get_history_json(&json);
    TEST_RESULT(r, "lc_profiler_get_history_json() succeeds");
    TEST_ASSERT(json != NULL, "history json is non-NULL");
    if (json) {
        TEST_ASSERT(strstr(json, "\"frames\"") != NULL, "history json has frames array");
        TEST_ASSERT(strstr(json, "\"historySize\":120") != NULL, "history size reflected in json");
        lc_free(json);
    }
}

static void test_ext_profiler_zones(void) {
    SECTION_START("Profiler: Custom Zones (no open frame)");

    /* With no frame open these are safe no-ops and must not crash. */
    lc_profiler_begin_zone("gameplay.ai", "Update");
    lc_profiler_begin_zone("gameplay.path", NULL);
    lc_profiler_end_zone();
    lc_profiler_end_zone();
    /* Extra end is tolerated (empty stack guard). */
    lc_profiler_end_zone();
    TEST_ASSERT(1, "zone begin/end without an open frame do not crash");

    double ms = lc_profiler_average_frame_ms();
    TEST_ASSERT(ms >= 0.0, "lc_profiler_average_frame_ms() non-negative");
    double fps = lc_profiler_fps();
    TEST_ASSERT(fps >= 0.0, "lc_profiler_fps() non-negative");
}

static void test_ext_profiler_save(void) {
    SECTION_START("Profiler: Save Capture");

    LCResult r = lc_profiler_save_capture("user://profiler_capture_test.lprof");
    TEST_ASSERT(r == LC_SUCCESS || r == LC_ERROR_INTERNAL_ERROR,
                "lc_profiler_save_capture() returns a defined result");

    /* NULL path must be rejected, not crash. */
    r = lc_profiler_save_capture(NULL);
    TEST_ASSERT(r == LC_ERROR_NULL_POINTER, "save with NULL path rejected");
}

static void test_ext_profiling(void) {
    test_ext_profiler_toggle();
    test_ext_profiler_history();
    test_ext_profiler_zones();
    test_ext_profiler_save();
}
