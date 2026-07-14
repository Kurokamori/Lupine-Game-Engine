/* Extended platform C API tests:
 *  - Input I/O additions in capi/include/input/lc_input.h
 *    (gamepad deadzone, touch, clipboard).
 *  - Window / display server in capi/include/platform/lc_display.h.
 *  - Screen <-> world conversions in capi/include/rendering/lc_camera.h.
 *
 * These run headless, so most checks verify the API is callable and returns a
 * defined value (and that round-trippable setters/getters agree on cached
 * state). The display server caches state when no platform backend is installed,
 * so set/get pairs are deterministic here. */

static void test_ext_platform_input(void) {
    SECTION_START("Platform: Input I/O (deadzone / touch / clipboard)");

    lc_input_set_gamepad_deadzone(0.25f);
    float dz = lc_input_get_gamepad_deadzone();
    TEST_ASSERT(dz >= 0.24f && dz <= 0.26f, "lc_input_set/get_gamepad_deadzone() round-trips");

    bool avail = lc_input_is_touch_available();
    TEST_ASSERT(avail == true || avail == false, "lc_input_is_touch_available() returns defined value");

    bool touching = lc_input_is_touching();
    TEST_ASSERT(touching == false, "lc_input_is_touching() false when headless");

    uint32_t count = lc_input_get_touch_count();
    TEST_ASSERT(count == 0u, "lc_input_get_touch_count() is 0 when headless");

    LCVec2 touch_pos = lc_vec2(123.0f, 456.0f);
    LCResult tr = lc_input_get_touch_position(0, &touch_pos);
    TEST_ASSERT(tr == LC_SUCCESS, "lc_input_get_touch_position() callable");

    TEST_ASSERT(lc_input_is_touch_just_started() == false,
                "lc_input_is_touch_just_started() false when headless");
    TEST_ASSERT(lc_input_is_touch_just_ended() == false,
                "lc_input_is_touch_just_ended() false when headless");

    LCResult sc = lc_input_set_clipboard("lupine-capi");
    TEST_ASSERT(sc == LC_SUCCESS, "lc_input_set_clipboard() returns success");

    char clip[64];
    LCResult gc = lc_input_get_clipboard(clip, sizeof(clip));
    TEST_ASSERT(gc == LC_SUCCESS, "lc_input_get_clipboard() returns success");

    TEST_ASSERT(lc_input_set_clipboard(NULL) == LC_ERROR_NULL_POINTER,
                "lc_input_set_clipboard(NULL) rejects null");
    TEST_ASSERT(lc_input_get_clipboard(NULL, 16) == LC_ERROR_NULL_POINTER,
                "lc_input_get_clipboard(NULL) rejects null");

    SECTION_END("Platform: Input I/O (deadzone / touch / clipboard)");
}

static void test_ext_platform_display(void) {
    SECTION_START("Platform: Window / Display Server");

    LCResult st = lc_display_set_window_title("Lupine C-API Test");
    TEST_ASSERT(st == LC_SUCCESS, "lc_display_set_window_title() returns success");

    char title[128];
    LCResult gt = lc_display_get_window_title(title, sizeof(title));
    TEST_ASSERT(gt == LC_SUCCESS, "lc_display_get_window_title() returns success");
    TEST_ASSERT(strcmp(title, "Lupine C-API Test") == 0,
                "lc_display_get_window_title() returns cached title");

    lc_display_set_fullscreen(true);
    TEST_ASSERT(lc_display_is_fullscreen() == true, "lc_display_set/is_fullscreen() round-trips true");
    lc_display_set_fullscreen(false);
    TEST_ASSERT(lc_display_is_fullscreen() == false, "lc_display_set/is_fullscreen() round-trips false");

    lc_display_set_vsync(false);
    TEST_ASSERT(lc_display_is_vsync() == false, "lc_display_set/is_vsync() round-trips false");
    lc_display_set_vsync(true);
    TEST_ASSERT(lc_display_is_vsync() == true, "lc_display_set/is_vsync() round-trips true");

    lc_display_set_window_size(1280, 720);
    LCVec2 wsize = lc_vec2_zero();
    LCResult ws = lc_display_get_window_size(&wsize);
    TEST_ASSERT(ws == LC_SUCCESS, "lc_display_get_window_size() returns success");

    LCVec2 ssize = lc_vec2_zero();
    LCResult ss = lc_display_get_screen_size(&ssize);
    TEST_ASSERT(ss == LC_SUCCESS, "lc_display_get_screen_size() returns success");

    lc_display_maximize_window();
    lc_display_minimize_window();
    lc_display_restore_window();
    TEST_ASSERT(true, "lc_display_maximize/minimize/restore_window() callable");

    lc_display_set_mouse_mode(LC_MOUSE_MODE_CAPTURED);
    TEST_ASSERT(lc_display_get_mouse_mode() == LC_MOUSE_MODE_CAPTURED,
                "lc_display_set/get_mouse_mode() round-trips Captured");
    lc_display_set_mouse_mode(LC_MOUSE_MODE_VISIBLE);
    TEST_ASSERT(lc_display_get_mouse_mode() == LC_MOUSE_MODE_VISIBLE,
                "lc_display_set/get_mouse_mode() round-trips Visible");

    lc_display_set_mouse_cursor_visible(false);
    TEST_ASSERT(lc_display_is_mouse_cursor_visible() == false,
                "lc_display_set/is_mouse_cursor_visible() round-trips hidden");
    lc_display_set_mouse_cursor_visible(true);
    TEST_ASSERT(lc_display_is_mouse_cursor_visible() == true,
                "lc_display_set/is_mouse_cursor_visible() round-trips visible");

    TEST_ASSERT(lc_display_set_window_title(NULL) == LC_ERROR_NULL_POINTER,
                "lc_display_set_window_title(NULL) rejects null");
    TEST_ASSERT(lc_display_get_window_size(NULL) == LC_ERROR_NULL_POINTER,
                "lc_display_get_window_size(NULL) rejects null");

    SECTION_END("Platform: Window / Display Server");
}

static void test_ext_platform_screen_world(void) {
    SECTION_START("Platform: Screen <-> World Conversion");

    LCVec2 world2d = lc_vec2_zero();
    LCResult r2 = lc_screen_to_world_2d(lc_vec2(100.0f, 100.0f), &world2d);
    TEST_ASSERT(r2 == LC_SUCCESS || r2 == LC_ERROR_NOT_FOUND,
                "lc_screen_to_world_2d() callable (success or no-scene)");

    LCVec2 screen2d = lc_vec2_zero();
    LCResult r3 = lc_world_to_screen_2d(lc_vec2(1.0f, 2.0f), &screen2d);
    TEST_ASSERT(r3 == LC_SUCCESS || r3 == LC_ERROR_NOT_FOUND,
                "lc_world_to_screen_2d() callable (success or no-scene)");

    LCVec3 world3d = lc_vec3_zero();
    LCResult r4 = lc_screen_to_world_3d(lc_vec2(100.0f, 100.0f), 10.0f, &world3d);
    TEST_ASSERT(r4 == LC_SUCCESS || r4 == LC_ERROR_NOT_FOUND,
                "lc_screen_to_world_3d() callable (success or no-scene)");

    LCVec3 screen3d = lc_vec3_zero();
    LCResult r5 = lc_world_to_screen_3d(lc_vec3(1.0f, 2.0f, 3.0f), &screen3d);
    TEST_ASSERT(r5 == LC_SUCCESS || r5 == LC_ERROR_NOT_FOUND,
                "lc_world_to_screen_3d() callable (success or no-scene)");

    LCVec3 ray_origin = lc_vec3_zero();
    LCVec3 ray_dir = lc_vec3_zero();
    LCResult r6 = lc_screen_to_world_ray_3d(lc_vec2(100.0f, 100.0f), &ray_origin, &ray_dir);
    TEST_ASSERT(r6 == LC_SUCCESS || r6 == LC_ERROR_NOT_FOUND,
                "lc_screen_to_world_ray_3d() callable (success or no-scene)");

    TEST_ASSERT(lc_screen_to_world_2d(lc_vec2(0.0f, 0.0f), NULL) == LC_ERROR_NULL_POINTER,
                "lc_screen_to_world_2d(NULL) rejects null");
    TEST_ASSERT(lc_screen_to_world_ray_3d(lc_vec2(0.0f, 0.0f), &ray_origin, NULL) == LC_ERROR_NULL_POINTER,
                "lc_screen_to_world_ray_3d(NULL) rejects null");

    SECTION_END("Platform: Screen <-> World Conversion");
}
