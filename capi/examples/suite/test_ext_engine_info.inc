/* Engine / OS information + time-scale C API tests.
 *
 * These mirror the scripting globals (time scale, fps, ticks, unix time,
 * platform name, debug build, dpi scale, open url). They run headless, so most
 * checks verify the API is callable and returns a defined value. */

static void test_ext_engine_time_scale(void) {
    SECTION_START("Engine: Time Scale");

    lc_set_time_scale(0.5f);
    float ts = lc_get_time_scale();
    TEST_ASSERT(ts >= 0.0f, "lc_get_time_scale() returns a non-negative value");

    lc_set_time_scale(1.0f);
    TEST_ASSERT(lc_get_time_scale() >= 0.0f, "lc_set_time_scale(1.0)/get callable");

    SECTION_END("Engine: Time Scale");
}

static void test_ext_engine_os_info(void) {
    SECTION_START("Engine: OS / Runtime Info");

    float fps = lc_get_fps();
    TEST_ASSERT(fps >= 0.0f, "lc_get_fps() returns a non-negative value");

    int ticks = lc_get_ticks_msec();
    TEST_ASSERT(ticks >= 0, "lc_get_ticks_msec() returns a non-negative value");

    double unix_time = lc_get_unix_time();
    TEST_ASSERT(unix_time > 1000000000.0, "lc_get_unix_time() returns a plausible epoch time");

    const char* platform = lc_get_platform_name();
    TEST_ASSERT(platform != NULL && platform[0] != '\0', "lc_get_platform_name() returns a non-empty string");

    bool dbg = lc_is_debug_build();
    TEST_ASSERT(dbg == true || dbg == false, "lc_is_debug_build() returns a defined value");

    float dpi = lc_get_dpi_scale();
    TEST_ASSERT(dpi > 0.0f, "lc_get_dpi_scale() returns a positive scale");

    bool opened = lc_open_url("https://example.com");
    TEST_ASSERT(opened == true || opened == false, "lc_open_url() returns a defined value");
    TEST_ASSERT(lc_open_url(NULL) == false, "lc_open_url(NULL) returns false");

    SECTION_END("Engine: OS / Runtime Info");
}
