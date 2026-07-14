/* ============================================================================
 * Extended Project Tests: Project + ProjectSettings (lc_project_*)
 *
 * Included as a fragment into capi_test_suite.c. No includes, no main().
 * Exercises lifecycle, every typed settings accessor, and splash entries.
 * ============================================================================ */

static void test_ext_project_settings(void) {
    SECTION_START("Ext Project: settings accessors");

    LCProjectHandle project = NULL;
    LCResult result = lc_project_create(&project);
    TEST_RESULT(result, "lc_project_create()");
    TEST_ASSERT(project != NULL, "project handle is non-NULL");

    bool loaded = true;
    result = lc_project_is_loaded(project, &loaded);
    TEST_RESULT(result, "lc_project_is_loaded() callable");
    TEST_ASSERT(!loaded, "freshly created project is not loaded");

    result = lc_project_set_name(project, "Capi Test Project");
    TEST_RESULT(result, "lc_project_set_name()");
    char* name = NULL;
    result = lc_project_get_name(project, &name);
    TEST_RESULT(result, "lc_project_get_name()");
    TEST_ASSERT(name != NULL && strcmp(name, "Capi Test Project") == 0,
                "project name round-trips");
    lc_free(name);

    result = lc_project_set_creator_name(project, "Tester");
    TEST_RESULT(result, "lc_project_set_creator_name()");
    char* creator = NULL;
    lc_project_get_creator_name(project, &creator);
    TEST_ASSERT(creator != NULL && strcmp(creator, "Tester") == 0,
                "creator name round-trips");
    lc_free(creator);

    result = lc_project_set_version(project, "2.1.0");
    TEST_RESULT(result, "lc_project_set_version()");
    char* version = NULL;
    lc_project_get_version(project, &version);
    TEST_ASSERT(version != NULL && strcmp(version, "2.1.0") == 0,
                "version round-trips");
    lc_free(version);

    result = lc_project_set_main_scene(project, "res://main.scene");
    TEST_RESULT(result, "lc_project_set_main_scene()");
    char* main_scene = NULL;
    lc_project_get_main_scene(project, &main_scene);
    TEST_ASSERT(main_scene != NULL && strcmp(main_scene, "res://main.scene") == 0,
                "main scene round-trips");
    lc_free(main_scene);

    result = lc_project_set_default_font(project, "res://font.ttf");
    TEST_RESULT(result, "lc_project_set_default_font()");
    char* font = NULL;
    lc_project_get_default_font(project, &font);
    TEST_ASSERT(font != NULL && strcmp(font, "res://font.ttf") == 0,
                "default font round-trips");
    lc_free(font);

    result = lc_project_set_default_theme(project, "res://ui.uitheme");
    TEST_RESULT(result, "lc_project_set_default_theme()");
    char* theme = NULL;
    lc_project_get_default_theme(project, &theme);
    TEST_ASSERT(theme != NULL && strcmp(theme, "res://ui.uitheme") == 0,
                "default theme round-trips");
    lc_free(theme);

    result = lc_project_set_window_width(project, 1920);
    TEST_RESULT(result, "lc_project_set_window_width()");
    result = lc_project_set_window_height(project, 1080);
    TEST_RESULT(result, "lc_project_set_window_height()");
    int width = 0, height = 0;
    lc_project_get_window_width(project, &width);
    lc_project_get_window_height(project, &height);
    TEST_ASSERT(width == 1920 && height == 1080, "window size round-trips");

    result = lc_project_set_window_size_override(project, true);
    TEST_RESULT(result, "lc_project_set_window_size_override()");
    result = lc_project_set_window_size_override_width(project, 1280);
    TEST_RESULT(result, "lc_project_set_window_size_override_width()");
    result = lc_project_set_window_size_override_height(project, 720);
    TEST_RESULT(result, "lc_project_set_window_size_override_height()");
    bool overrideEnabled = false;
    int overrideWidth = 0, overrideHeight = 0;
    lc_project_get_window_size_override(project, &overrideEnabled);
    lc_project_get_window_size_override_width(project, &overrideWidth);
    lc_project_get_window_size_override_height(project, &overrideHeight);
    TEST_ASSERT(overrideEnabled && overrideWidth == 1280 && overrideHeight == 720,
                "window size override round-trips");

    result = lc_project_set_fullscreen(project, true);
    TEST_RESULT(result, "lc_project_set_fullscreen()");
    bool fullscreen = false;
    lc_project_get_fullscreen(project, &fullscreen);
    TEST_ASSERT(fullscreen, "fullscreen round-trips");

    result = lc_project_set_vsync(project, false);
    TEST_RESULT(result, "lc_project_set_vsync()");
    bool vsync = true;
    lc_project_get_vsync(project, &vsync);
    TEST_ASSERT(!vsync, "vsync round-trips");

    result = lc_project_set_target_frame_rate(project, 144);
    TEST_RESULT(result, "lc_project_set_target_frame_rate()");
    int fps = 0;
    lc_project_get_target_frame_rate(project, &fps);
    TEST_ASSERT(fps == 144, "target frame rate round-trips");

    result = lc_project_set_max_undo_steps(project, 256);
    TEST_RESULT(result, "lc_project_set_max_undo_steps()");
    int undo = 0;
    lc_project_get_max_undo_steps(project, &undo);
    TEST_ASSERT(undo == 256, "max undo steps round-trips");

    LCColor clear = lc_color(0.2f, 0.3f, 0.4f, 1.0f);
    result = lc_project_set_clear_color(project, clear);
    TEST_RESULT(result, "lc_project_set_clear_color()");
    LCColor got_clear = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    lc_project_get_clear_color(project, &got_clear);
    TEST_ASSERT(got_clear.r == clear.r && got_clear.g == clear.g &&
                got_clear.b == clear.b && got_clear.a == clear.a,
                "clear color round-trips");

    LCVec2 ref = lc_vec2(1280.0f, 720.0f);
    result = lc_project_set_reference_resolution(project, ref);
    TEST_RESULT(result, "lc_project_set_reference_resolution()");
    LCVec2 got_ref = lc_vec2(0.0f, 0.0f);
    lc_project_get_reference_resolution(project, &got_ref);
    TEST_ASSERT(got_ref.x == ref.x && got_ref.y == ref.y,
                "reference resolution round-trips");

    result = lc_project_set_physics_tick_rate(project, 120.0f);
    TEST_RESULT(result, "lc_project_set_physics_tick_rate()");
    float tick = 0.0f;
    lc_project_get_physics_tick_rate(project, &tick);
    TEST_ASSERT(tick == 120.0f, "physics tick rate round-trips");

    LCVec2 g2 = lc_vec2(0.0f, -9.8f);
    result = lc_project_set_gravity_2d(project, g2);
    TEST_RESULT(result, "lc_project_set_gravity_2d()");
    LCVec2 got_g2 = lc_vec2(0.0f, 0.0f);
    lc_project_get_gravity_2d(project, &got_g2);
    TEST_ASSERT(got_g2.x == g2.x && got_g2.y == g2.y, "gravity 2D round-trips");

    LCVec3 g3 = lc_vec3(0.0f, -9.8f, 0.0f);
    result = lc_project_set_gravity_3d(project, g3);
    TEST_RESULT(result, "lc_project_set_gravity_3d()");
    LCVec3 got_g3 = lc_vec3(0.0f, 0.0f, 0.0f);
    lc_project_get_gravity_3d(project, &got_g3);
    TEST_ASSERT(got_g3.x == g3.x && got_g3.y == g3.y && got_g3.z == g3.z,
                "gravity 3D round-trips");

    result = lc_project_set_scale_mode(project, LC_SCALE_MODE_SCALE_WITH_SCREEN_SIZE);
    TEST_RESULT(result, "lc_project_set_scale_mode()");
    LCScaleMode scale_mode = LC_SCALE_MODE_PIXEL_PERFECT;
    lc_project_get_scale_mode(project, &scale_mode);
    TEST_ASSERT(scale_mode == LC_SCALE_MODE_SCALE_WITH_SCREEN_SIZE,
                "scale mode round-trips");

    result = lc_project_set_viewport_scale_mode(project, LC_VIEWPORT_SCALE_MODE_CROP);
    TEST_RESULT(result, "lc_project_set_viewport_scale_mode()");
    LCViewportScaleMode vp_mode = LC_VIEWPORT_SCALE_MODE_LETTERBOX;
    lc_project_get_viewport_scale_mode(project, &vp_mode);
    TEST_ASSERT(vp_mode == LC_VIEWPORT_SCALE_MODE_CROP,
                "viewport scale mode round-trips");

    result = lc_project_set_texture_filtering(project, LC_TEXTURE_FILTERING_NEAREST);
    TEST_RESULT(result, "lc_project_set_texture_filtering()");
    LCTextureFiltering filtering = LC_TEXTURE_FILTERING_BILINEAR;
    lc_project_get_texture_filtering(project, &filtering);
    TEST_ASSERT(filtering == LC_TEXTURE_FILTERING_NEAREST,
                "texture filtering round-trips");

    result = lc_project_set_name(NULL, "x");
    TEST_ASSERT(result == LC_ERROR_INVALID_HANDLE,
                "lc_project_set_name(NULL handle) rejected");

    result = lc_project_destroy(project);
    TEST_RESULT(result, "lc_project_destroy()");
    result = lc_project_destroy(project);
    TEST_ASSERT(result == LC_ERROR_INVALID_HANDLE,
                "double destroy rejected");

    SECTION_END("Ext Project: settings accessors");
}

static void test_ext_project_splash(void) {
    SECTION_START("Ext Project: splash screens");

    LCProjectHandle project = NULL;
    LCResult result = lc_project_create(&project);
    TEST_RESULT(result, "lc_project_create() (splash)");

    result = lc_project_set_splash_enabled(project, true);
    TEST_RESULT(result, "lc_project_set_splash_enabled()");
    bool enabled = false;
    lc_project_get_splash_enabled(project, &enabled);
    TEST_ASSERT(enabled, "splash enabled round-trips");

    result = lc_project_set_splash_allow_skip(project, false);
    TEST_RESULT(result, "lc_project_set_splash_allow_skip()");
    bool allow_skip = true;
    lc_project_get_splash_allow_skip(project, &allow_skip);
    TEST_ASSERT(!allow_skip, "splash allow_skip round-trips");

    result = lc_project_clear_splash_entries(project);
    TEST_RESULT(result, "lc_project_clear_splash_entries()");
    int count = -1;
    lc_project_get_splash_entry_count(project, &count);
    TEST_ASSERT(count == 0, "no splash entries after clear");

    result = lc_project_add_splash_entry(project, "res://splash1.png", 0.5f, 2.0f, 0.5f);
    TEST_RESULT(result, "lc_project_add_splash_entry() #1");
    result = lc_project_add_splash_entry(project, "res://splash2.png", 0.3f, 1.5f, 0.3f);
    TEST_RESULT(result, "lc_project_add_splash_entry() #2");

    lc_project_get_splash_entry_count(project, &count);
    TEST_ASSERT(count == 2, "two splash entries after add");

    char* image_path = NULL;
    float fade_in = 0.0f, hold = 0.0f, fade_out = 0.0f;
    result = lc_project_get_splash_entry(project, 1, &image_path, &fade_in, &hold, &fade_out);
    TEST_RESULT(result, "lc_project_get_splash_entry()");
    TEST_ASSERT(image_path != NULL && strcmp(image_path, "res://splash2.png") == 0,
                "splash entry image path round-trips");
    TEST_ASSERT(fade_in == 0.3f && hold == 1.5f && fade_out == 0.3f,
                "splash entry durations round-trip");
    lc_free(image_path);

    result = lc_project_get_splash_entry(project, 99, NULL, NULL, NULL, NULL);
    TEST_ASSERT(result == LC_ERROR_INVALID_PARAMETER,
                "out-of-range splash entry rejected");

    lc_project_destroy(project);

    SECTION_END("Ext Project: splash screens");
}
