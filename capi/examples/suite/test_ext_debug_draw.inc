/* Debug draw C API tests.
 *
 * These mirror the scripting debug-draw globals. They enqueue primitives into
 * the engine debug draw queue; headless there is no consumer, so the checks
 * verify the functions are callable without error. */

static void test_ext_debug_draw(void) {
    SECTION_START("Debug Draw: Primitives");

    LCColor red = lc_color(1.0f, 0.0f, 0.0f, 1.0f);
    LCColor green = lc_color(0.0f, 1.0f, 0.0f, 1.0f);

    lc_debug_draw_line(lc_vec3(0.0f, 0.0f, 0.0f), lc_vec3(1.0f, 1.0f, 1.0f), red, 0.0f);
    lc_debug_draw_line_2d(lc_vec2(0.0f, 0.0f), lc_vec2(10.0f, 10.0f), green, 0.5f);
    lc_debug_draw_ray(lc_vec3(0.0f, 0.0f, 0.0f), lc_vec3(0.0f, 1.0f, 0.0f), red, 0.0f);
    lc_debug_draw_box(lc_vec3(0.0f, 0.0f, 0.0f), lc_vec3(2.0f, 2.0f, 2.0f), green, 1.0f);
    lc_debug_draw_sphere(lc_vec3(0.0f, 0.0f, 0.0f), 1.5f, red, 0.0f);
    lc_debug_draw_circle(lc_vec3(0.0f, 0.0f, 0.0f), lc_vec3(0.0f, 1.0f, 0.0f), 1.0f, green, 0.0f);
    lc_debug_draw_text(lc_vec3(0.0f, 2.0f, 0.0f), "hello 3d", red, 0.0f);
    lc_debug_draw_text_2d(lc_vec2(20.0f, 20.0f), "hello 2d", green, 0.0f);
    TEST_ASSERT(true, "lc_debug_draw_* primitives callable");

    lc_debug_draw_text(lc_vec3(0.0f, 0.0f, 0.0f), NULL, red, 0.0f);
    lc_debug_draw_text_2d(lc_vec2(0.0f, 0.0f), NULL, green, 0.0f);
    TEST_ASSERT(true, "lc_debug_draw_text/text_2d tolerate NULL text");

    SECTION_END("Debug Draw: Primitives");
}
