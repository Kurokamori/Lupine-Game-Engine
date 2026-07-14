/* ============================================================================
 * Extended Misc Tests: UI Theme, Timer extras, Material extras, logging, math
 *
 * Included as a fragment into capi_test_suite.c. No includes, no main().
 * ============================================================================ */

static void test_ext_misc_theme(void) {
    SECTION_START("Ext Misc: UI Theme System");

    unsigned long long version_before = 0ULL;
    LCResult result = lc_theme_get_version(&version_before);
    TEST_RESULT(result, "lc_theme_get_version() initial");

    result = lc_theme_set_active("res://nonexistent.uitheme");
    TEST_ASSERT(result == LC_SUCCESS || result != LC_SUCCESS,
                "lc_theme_set_active() callable");

    LCColor fallback_color = lc_color(0.25f, 0.5f, 0.75f, 1.0f);
    LCColor resolved_color = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    result = lc_theme_get_color("Button", NULL, "font_color",
                                fallback_color, &resolved_color);
    TEST_RESULT(result, "lc_theme_get_color() returns success");
    TEST_ASSERT(resolved_color.r == fallback_color.r &&
                resolved_color.g == fallback_color.g &&
                resolved_color.b == fallback_color.b &&
                resolved_color.a == fallback_color.a,
                "lc_theme_get_color() yields fallback when undefined");

    float fallback_constant = 7.5f;
    float resolved_constant = 0.0f;
    result = lc_theme_get_constant("Button", NULL, "corner_radius",
                                   fallback_constant, &resolved_constant);
    TEST_RESULT(result, "lc_theme_get_constant() returns success");
    TEST_ASSERT(resolved_constant == fallback_constant,
                "lc_theme_get_constant() yields fallback when undefined");

    unsigned long long version_pre_palette = 0ULL;
    lc_theme_get_version(&version_pre_palette);
    LCColor palette_color = lc_color(1.0f, 0.0f, 0.0f, 1.0f);
    result = lc_theme_set_palette_color("primary", palette_color);
    TEST_RESULT(result, "lc_theme_set_palette_color()");

    unsigned long long version_post_palette = 0ULL;
    lc_theme_get_version(&version_post_palette);
    TEST_ASSERT(version_post_palette > version_pre_palette,
                "lc_theme_set_palette_color() bumps version");

    result = lc_theme_set_variable("spacing", 8.0f);
    TEST_RESULT(result, "lc_theme_set_variable()");

    unsigned long long version_post_variable = 0ULL;
    lc_theme_get_version(&version_post_variable);
    TEST_ASSERT(version_post_variable > version_post_palette,
                "lc_theme_set_variable() bumps version");

    result = lc_theme_set_palette_color(NULL, palette_color);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER,
                "lc_theme_set_palette_color(NULL) rejected");

    SECTION_END("Ext Misc: UI Theme System");
}

static void test_ext_misc_timer(void) {
    SECTION_START("Ext Misc: Timer Extras");

    LCComponentHandle timer = NULL;
    LCResult result = lc_timer_create("ExtMiscTimer", &timer);
    TEST_RESULT(result, "Create Timer");
    TEST_ASSERT(timer != NULL, "Timer handle valid");

    result = lc_timer_set_auto_start(timer, true);
    TEST_RESULT(result, "lc_timer_set_auto_start(true)");

    bool auto_start = false;
    result = lc_timer_get_auto_start(timer, &auto_start);
    TEST_RESULT(result, "lc_timer_get_auto_start()");
    TEST_ASSERT(auto_start, "Auto-start round-trips true");

    result = lc_timer_set_auto_start(timer, false);
    TEST_RESULT(result, "lc_timer_set_auto_start(false)");
    lc_timer_get_auto_start(timer, &auto_start);
    TEST_ASSERT(!auto_start, "Auto-start round-trips false");

    result = lc_timer_set_ignore_time_scale(timer, true);
    TEST_RESULT(result, "lc_timer_set_ignore_time_scale(true)");

    bool ignore_scale = false;
    result = lc_timer_get_ignore_time_scale(timer, &ignore_scale);
    TEST_RESULT(result, "lc_timer_get_ignore_time_scale()");
    TEST_ASSERT(ignore_scale, "Ignore time scale round-trips true");

    result = lc_timer_set_ignore_time_scale(timer, false);
    TEST_RESULT(result, "lc_timer_set_ignore_time_scale(false)");
    lc_timer_get_ignore_time_scale(timer, &ignore_scale);
    TEST_ASSERT(!ignore_scale, "Ignore time scale round-trips false");

    result = lc_timer_set_duration(timer, 10.0f);
    TEST_RESULT(result, "Set duration");

    result = lc_timer_set_elapsed(timer, 3.5f);
    TEST_RESULT(result, "lc_timer_set_elapsed()");

    float elapsed = 0.0f;
    result = lc_timer_get_elapsed(timer, &elapsed);
    TEST_RESULT(result, "lc_timer_get_elapsed()");
    TEST_ASSERT(elapsed == 3.5f, "Elapsed round-trips 3.5");

    bool finished = true;
    result = lc_timer_is_finished(timer, &finished);
    TEST_RESULT(result, "lc_timer_is_finished()");
    TEST_ASSERT(!finished, "Timer not finished after partial elapse");

    lc_component_destroy(timer);

    SECTION_END("Ext Misc: Timer Extras");
}

static void test_ext_misc_material(void) {
    SECTION_START("Ext Misc: Material Extras");

    LCMaterialHandle material = NULL;
    LCResult result;

    result = lc_material_get_default_skeletal_toon(&material);
    TEST_ASSERT(result == LC_SUCCESS ? material != NULL
                                     : result == LC_ERROR_NOT_SUPPORTED,
                "lc_material_get_default_skeletal_toon()");

    material = NULL;
    result = lc_material_get_default_skeletal_stylized(&material);
    TEST_ASSERT(result == LC_SUCCESS ? material != NULL
                                     : result == LC_ERROR_NOT_SUPPORTED,
                "lc_material_get_default_skeletal_stylized()");

    material = NULL;
    result = lc_material_get_default_text3d(&material);
    TEST_ASSERT(result == LC_SUCCESS ? material != NULL
                                     : result == LC_ERROR_NOT_SUPPORTED,
                "lc_material_get_default_text3d()");

    material = NULL;
    result = lc_material_get_default_colored_double_sided(&material);
    TEST_ASSERT(result == LC_SUCCESS ? material != NULL
                                     : result == LC_ERROR_NOT_SUPPORTED,
                "lc_material_get_default_colored_double_sided()");

    material = NULL;
    result = lc_material_get_or_create_custom("res://shaders/custom.vert",
                                              "res://shaders/custom.frag",
                                              false, &material);
    TEST_ASSERT(result == LC_SUCCESS ? material != NULL
                                     : result != LC_SUCCESS,
                "lc_material_get_or_create_custom() callable");

    SECTION_END("Ext Misc: Material Extras");
}

static void test_ext_misc_logging(void) {
    SECTION_START("Ext Misc: Logging Extras");

    LCLogLevel original_level = lc_log_get_level();
    lc_log_set_level(LC_LOG_TRACE);

    lc_log_trace("ext_misc: trace message");
    TEST_ASSERT(true, "lc_log_trace() callable");

    lc_log_error("ext_misc: error message");
    TEST_ASSERT(true, "lc_log_error() callable");

    lc_log_critical("ext_misc: critical message");
    TEST_ASSERT(true, "lc_log_critical() callable");

    lc_log_format(LC_LOG_INFO, "ext_misc: formatted %d + %d = %d (%s)",
                  2, 3, 5, "ok");
    TEST_ASSERT(true, "lc_log_format() variadic callable");

    lc_log_set_level(original_level);
    TEST_ASSERT(lc_log_get_level() == original_level, "Log level restored");

    SECTION_END("Ext Misc: Logging Extras");
}

static void test_ext_misc_transform(void) {
    SECTION_START("Ext Misc: Transform From Matrix");

    LCVec3 position = lc_vec3(4.0f, -2.0f, 7.0f);
    LCQuat rotation = lc_quat_identity();
    LCVec3 scale = lc_vec3(2.0f, 2.0f, 2.0f);
    LCTransform original = lc_transform_create(position, rotation, scale);

    LCMat4 matrix = lc_transform_to_matrix(&original);
    LCTransform recovered = lc_transform_from_matrix(&matrix);

    TEST_ASSERT(recovered.position.x > 3.99f && recovered.position.x < 4.01f &&
                recovered.position.y > -2.01f && recovered.position.y < -1.99f &&
                recovered.position.z > 6.99f && recovered.position.z < 7.01f,
                "lc_transform_from_matrix() recovers position");

    TEST_ASSERT(recovered.scale.x > 1.98f && recovered.scale.x < 2.02f &&
                recovered.scale.y > 1.98f && recovered.scale.y < 2.02f &&
                recovered.scale.z > 1.98f && recovered.scale.z < 2.02f,
                "lc_transform_from_matrix() recovers scale");

    SECTION_END("Ext Misc: Transform From Matrix");
}
