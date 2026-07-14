/**
 * @file test_ext_world_environment.inc
 * @brief Extended WorldEnvironment post-process coverage tests for the Lupine C API
 *
 * Covers the WorldEnvironment post-processing surface not exercised by the core
 * suite: bloom / SSAO / color-grading / vignette / chromatic-aberration /
 * film-grain / overlay / tonemap white-point round-trips, plus the enabled-flag
 * getters and NULL-pointer guards.
 *
 * This file is #included into capi_test_suite.c. It defines no main(),
 * includes no headers, and redefines no macros.
 */

static void test_ext_we_scalar_postprocess(void) {
    SECTION_START("WorldEnvironment Ext: Scalar Post-Process");

    LCComponentHandle we = NULL;
    LCResult result = lc_world_environment_create("ExtWEScalar", &we);
    TEST_RESULT(result, "Create WorldEnvironment");

    /* Bloom */
    result = lc_world_environment_set_bloom_threshold(we, 0.85f);
    TEST_RESULT(result, "Set bloom threshold 0.85");
    float fval = 0.0f;
    result = lc_world_environment_get_bloom_threshold(we, &fval);
    TEST_RESULT(result, "Get bloom threshold");
    TEST_ASSERT(fval > 0.84f && fval < 0.86f, "Bloom threshold round-trip 0.85");

    result = lc_world_environment_set_bloom_soft_knee(we, 0.40f);
    TEST_RESULT(result, "Set bloom soft knee 0.40");
    fval = 0.0f;
    result = lc_world_environment_get_bloom_soft_knee(we, &fval);
    TEST_RESULT(result, "Get bloom soft knee");
    TEST_ASSERT(fval > 0.39f && fval < 0.41f, "Bloom soft knee round-trip 0.40");

    /* SSAO */
    result = lc_world_environment_set_ssao_intensity(we, 1.25f);
    TEST_RESULT(result, "Set SSAO intensity 1.25");
    fval = 0.0f;
    result = lc_world_environment_get_ssao_intensity(we, &fval);
    TEST_RESULT(result, "Get SSAO intensity");
    TEST_ASSERT(fval > 1.24f && fval < 1.26f, "SSAO intensity round-trip 1.25");

    result = lc_world_environment_set_ssao_bias(we, 0.025f);
    TEST_RESULT(result, "Set SSAO bias 0.025");
    fval = 0.0f;
    result = lc_world_environment_get_ssao_bias(we, &fval);
    TEST_RESULT(result, "Get SSAO bias");
    TEST_ASSERT(fval > 0.015f && fval < 0.035f, "SSAO bias round-trip 0.025");

    result = lc_world_environment_set_ssao_power(we, 2.5f);
    TEST_RESULT(result, "Set SSAO power 2.5");
    fval = 0.0f;
    result = lc_world_environment_get_ssao_power(we, &fval);
    TEST_RESULT(result, "Get SSAO power");
    TEST_ASSERT(fval > 2.49f && fval < 2.51f, "SSAO power round-trip 2.5");

    /* Color grading scalars */
    result = lc_world_environment_set_contrast(we, 1.15f);
    TEST_RESULT(result, "Set contrast 1.15");
    fval = 0.0f;
    result = lc_world_environment_get_contrast(we, &fval);
    TEST_RESULT(result, "Get contrast");
    TEST_ASSERT(fval > 1.14f && fval < 1.16f, "Contrast round-trip 1.15");

    result = lc_world_environment_set_brightness(we, 0.90f);
    TEST_RESULT(result, "Set brightness 0.90");
    fval = 0.0f;
    result = lc_world_environment_get_brightness(we, &fval);
    TEST_RESULT(result, "Get brightness");
    TEST_ASSERT(fval > 0.89f && fval < 0.91f, "Brightness round-trip 0.90");

    result = lc_world_environment_set_temperature(we, 0.30f);
    TEST_RESULT(result, "Set temperature 0.30");
    fval = 0.0f;
    result = lc_world_environment_get_temperature(we, &fval);
    TEST_RESULT(result, "Get temperature");
    TEST_ASSERT(fval > 0.29f && fval < 0.31f, "Temperature round-trip 0.30");

    result = lc_world_environment_set_tint(we, -0.20f);
    TEST_RESULT(result, "Set tint -0.20");
    fval = 0.0f;
    result = lc_world_environment_get_tint(we, &fval);
    TEST_RESULT(result, "Get tint");
    TEST_ASSERT(fval > -0.21f && fval < -0.19f, "Tint round-trip -0.20");

    /* Tonemap white point */
    result = lc_world_environment_set_white_point(we, 11.0f);
    TEST_RESULT(result, "Set white point 11.0");
    fval = 0.0f;
    result = lc_world_environment_get_white_point(we, &fval);
    TEST_RESULT(result, "Get white point");
    TEST_ASSERT(fval > 10.99f && fval < 11.01f, "White point round-trip 11.0");

    /* Vignette scalars */
    result = lc_world_environment_set_vignette_smoothness(we, 0.55f);
    TEST_RESULT(result, "Set vignette smoothness 0.55");
    fval = 0.0f;
    result = lc_world_environment_get_vignette_smoothness(we, &fval);
    TEST_RESULT(result, "Get vignette smoothness");
    TEST_ASSERT(fval > 0.54f && fval < 0.56f, "Vignette smoothness round-trip 0.55");

    result = lc_world_environment_set_vignette_roundness(we, 0.70f);
    TEST_RESULT(result, "Set vignette roundness 0.70");
    fval = 0.0f;
    result = lc_world_environment_get_vignette_roundness(we, &fval);
    TEST_RESULT(result, "Get vignette roundness");
    TEST_ASSERT(fval > 0.69f && fval < 0.71f, "Vignette roundness round-trip 0.70");

    result = lc_world_environment_set_vignette_center_x(we, 0.45f);
    TEST_RESULT(result, "Set vignette center x 0.45");
    fval = 0.0f;
    result = lc_world_environment_get_vignette_center_x(we, &fval);
    TEST_RESULT(result, "Get vignette center x");
    TEST_ASSERT(fval > 0.44f && fval < 0.46f, "Vignette center x round-trip 0.45");

    result = lc_world_environment_set_vignette_center_y(we, 0.65f);
    TEST_RESULT(result, "Set vignette center y 0.65");
    fval = 0.0f;
    result = lc_world_environment_get_vignette_center_y(we, &fval);
    TEST_RESULT(result, "Get vignette center y");
    TEST_ASSERT(fval > 0.64f && fval < 0.66f, "Vignette center y round-trip 0.65");

    /* Chromatic aberration amount */
    result = lc_world_environment_set_chromatic_aberration_amount(we, 0.012f);
    TEST_RESULT(result, "Set chromatic aberration amount 0.012");
    fval = 0.0f;
    result = lc_world_environment_get_chromatic_aberration_amount(we, &fval);
    TEST_RESULT(result, "Get chromatic aberration amount");
    TEST_ASSERT(fval > 0.002f && fval < 0.022f, "Chromatic aberration amount round-trip 0.012");

    /* Film grain size */
    result = lc_world_environment_set_film_grain_size(we, 1.8f);
    TEST_RESULT(result, "Set film grain size 1.8");
    fval = 0.0f;
    result = lc_world_environment_get_film_grain_size(we, &fval);
    TEST_RESULT(result, "Get film grain size");
    TEST_ASSERT(fval > 1.79f && fval < 1.81f, "Film grain size round-trip 1.8");

    /* Overlay opacity */
    result = lc_world_environment_set_overlay_opacity(we, 0.75f);
    TEST_RESULT(result, "Set overlay opacity 0.75");
    fval = 0.0f;
    result = lc_world_environment_get_overlay_opacity(we, &fval);
    TEST_RESULT(result, "Get overlay opacity");
    TEST_ASSERT(fval > 0.74f && fval < 0.76f, "Overlay opacity round-trip 0.75");

    lc_component_destroy(we);
    SECTION_END("WorldEnvironment Ext: Scalar Post-Process");
}

static void test_ext_we_color_postprocess(void) {
    SECTION_START("WorldEnvironment Ext: Color Post-Process");

    LCComponentHandle we = NULL;
    LCResult result = lc_world_environment_create("ExtWEColor", &we);
    TEST_RESULT(result, "Create WorldEnvironment");

    LCColor lift = lc_color(0.10f, 0.20f, 0.30f, 1.0f);
    result = lc_world_environment_set_color_lift(we, lift);
    TEST_RESULT(result, "Set color lift");
    LCColor c = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    result = lc_world_environment_get_color_lift(we, &c);
    TEST_RESULT(result, "Get color lift");
    TEST_ASSERT(c.r > 0.09f && c.r < 0.11f && c.g > 0.19f && c.g < 0.21f &&
                c.b > 0.29f && c.b < 0.31f, "Color lift round-trip");

    LCColor gamma = lc_color(0.40f, 0.50f, 0.60f, 1.0f);
    result = lc_world_environment_set_color_gamma(we, gamma);
    TEST_RESULT(result, "Set color gamma");
    c = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    result = lc_world_environment_get_color_gamma(we, &c);
    TEST_RESULT(result, "Get color gamma");
    TEST_ASSERT(c.r > 0.39f && c.r < 0.41f && c.g > 0.49f && c.g < 0.51f &&
                c.b > 0.59f && c.b < 0.61f, "Color gamma round-trip");

    LCColor gain = lc_color(0.70f, 0.80f, 0.90f, 1.0f);
    result = lc_world_environment_set_color_gain(we, gain);
    TEST_RESULT(result, "Set color gain");
    c = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    result = lc_world_environment_get_color_gain(we, &c);
    TEST_RESULT(result, "Get color gain");
    TEST_ASSERT(c.r > 0.69f && c.r < 0.71f && c.g > 0.79f && c.g < 0.81f &&
                c.b > 0.89f && c.b < 0.91f, "Color gain round-trip");

    LCColor vig = lc_color(0.05f, 0.06f, 0.07f, 0.95f);
    result = lc_world_environment_set_vignette_color(we, vig);
    TEST_RESULT(result, "Set vignette color");
    c = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    result = lc_world_environment_get_vignette_color(we, &c);
    TEST_RESULT(result, "Get vignette color");
    TEST_ASSERT(c.r > 0.04f && c.r < 0.06f && c.g > 0.05f && c.g < 0.07f &&
                c.b > 0.06f && c.b < 0.08f && c.a > 0.94f && c.a < 0.96f,
                "Vignette color round-trip");

    lc_component_destroy(we);
    SECTION_END("WorldEnvironment Ext: Color Post-Process");
}

static void test_ext_we_enabled_flags(void) {
    SECTION_START("WorldEnvironment Ext: Enabled Flags");

    LCComponentHandle we = NULL;
    LCResult result = lc_world_environment_create("ExtWEFlags", &we);
    TEST_RESULT(result, "Create WorldEnvironment");

    /* film_grain_enabled has both setter and getter in the covered list. */
    result = lc_world_environment_set_film_grain_enabled(we, true);
    TEST_RESULT(result, "Set film grain enabled true");
    bool flag = false;
    result = lc_world_environment_get_film_grain_enabled(we, &flag);
    TEST_RESULT(result, "Get film grain enabled");
    TEST_ASSERT(flag == true, "Film grain enabled round-trip true");
    result = lc_world_environment_set_film_grain_enabled(we, false);
    TEST_RESULT(result, "Restore film grain enabled false");

    /* The remaining enabled getters are driven through their header setters. */
    result = lc_world_environment_set_bloom_enabled(we, true);
    TEST_RESULT(result, "Set bloom enabled true");
    flag = false;
    result = lc_world_environment_get_bloom_enabled(we, &flag);
    TEST_RESULT(result, "Get bloom enabled");
    TEST_ASSERT(flag == true, "Bloom enabled round-trip true");

    result = lc_world_environment_set_ssao_enabled(we, true);
    TEST_RESULT(result, "Set SSAO enabled true");
    flag = false;
    result = lc_world_environment_get_ssao_enabled(we, &flag);
    TEST_RESULT(result, "Get SSAO enabled");
    TEST_ASSERT(flag == true, "SSAO enabled round-trip true");

    result = lc_world_environment_set_color_grading_enabled(we, true);
    TEST_RESULT(result, "Set color grading enabled true");
    flag = false;
    result = lc_world_environment_get_color_grading_enabled(we, &flag);
    TEST_RESULT(result, "Get color grading enabled");
    TEST_ASSERT(flag == true, "Color grading enabled round-trip true");

    result = lc_world_environment_set_vignette_enabled(we, true);
    TEST_RESULT(result, "Set vignette enabled true");
    flag = false;
    result = lc_world_environment_get_vignette_enabled(we, &flag);
    TEST_RESULT(result, "Get vignette enabled");
    TEST_ASSERT(flag == true, "Vignette enabled round-trip true");

    result = lc_world_environment_set_chromatic_aberration_enabled(we, true);
    TEST_RESULT(result, "Set chromatic aberration enabled true");
    flag = false;
    result = lc_world_environment_get_chromatic_aberration_enabled(we, &flag);
    TEST_RESULT(result, "Get chromatic aberration enabled");
    TEST_ASSERT(flag == true, "Chromatic aberration enabled round-trip true");

    lc_component_destroy(we);
    SECTION_END("WorldEnvironment Ext: Enabled Flags");
}

static void test_ext_we_null_guards(void) {
    SECTION_START("WorldEnvironment Ext: NULL Guards");

    LCComponentHandle we = NULL;
    LCResult result = lc_world_environment_create("ExtWENull", &we);
    TEST_RESULT(result, "Create WorldEnvironment");

    /* NULL out-pointer on a scalar getter must report LC_ERROR_NULL_POINTER. */
    result = lc_world_environment_get_white_point(we, NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "Get white point NULL out guard");

    result = lc_world_environment_get_brightness(we, NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "Get brightness NULL out guard");

    /* NULL out-pointer on a color getter must report LC_ERROR_NULL_POINTER. */
    result = lc_world_environment_get_color_gain(we, NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "Get color gain NULL out guard");

    lc_component_destroy(we);
    SECTION_END("WorldEnvironment Ext: NULL Guards");
}
