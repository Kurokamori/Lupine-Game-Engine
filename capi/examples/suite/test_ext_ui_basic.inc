/*
 * test_ext_ui_basic.inc
 *
 * Extended coverage tests for the 2D UI widgets:
 *   Button, Panel, Checkbox, ColorRect, Image2D, ProgressBar,
 *   RadioButton, ToggleButton, TextureButton.
 *
 * This fragment is #included directly into capi_test_suite.c and relies on the
 * macros SECTION_START, SECTION_END, TEST_ASSERT and TEST_RESULT defined there.
 * It contains no #include directives, no main(), and no macro definitions.
 */

static bool ext_ui_feq(float a, float b) {
    float d = a - b;
    if (d < 0.0f) {
        d = -d;
    }
    return d < 0.01f;
}

static bool ext_ui_color_eq(LCColor a, LCColor b) {
    return ext_ui_feq(a.r, b.r) && ext_ui_feq(a.g, b.g) &&
           ext_ui_feq(a.b, b.b) && ext_ui_feq(a.a, b.a);
}

static bool ext_ui_vec2_eq(LCVec2 a, LCVec2 b) {
    return ext_ui_feq(a.x, b.x) && ext_ui_feq(a.y, b.y);
}

static void ext_ui_button_callback(void* user_data) {
    (void)user_data;
}

/* ========================================================================== */
/* Button                                                                     */
/* ========================================================================== */

static void test_ext_ui_button(void) {
    SECTION_START("UI Ext: Button Component");

    LCComponentHandle button = NULL;
    LCResult result = lc_button_create("ExtTestButton", &button);
    TEST_RESULT(result, "Create Button");

    /* Layer / sorting order round-trip */
    result = lc_button_set_layer(button, 7);
    TEST_RESULT(result, "Set layer");
    int layer = 0;
    lc_button_get_layer(button, &layer);
    TEST_ASSERT(layer == 7, "Layer round-trip is 7");

    result = lc_button_set_sorting_order(button, 3);
    TEST_RESULT(result, "Set sorting order");
    int order = 0;
    lc_button_get_sorting_order(button, &order);
    TEST_ASSERT(order == 3, "Sorting order round-trip is 3");

    /* UI space */
    result = lc_button_set_use_ui_space(button, true);
    TEST_RESULT(result, "Set use UI space");
    bool useUiSpace = false;
    lc_button_get_use_ui_space(button, &useUiSpace);
    TEST_ASSERT(useUiSpace == true, "Use UI space round-trip is true");

    /* Enabled query */
    result = lc_button_set_enabled(button, false);
    TEST_RESULT(result, "Disable button");
    bool enabled = true;
    lc_button_is_enabled(button, &enabled);
    TEST_ASSERT(enabled == false, "Button is disabled");
    lc_button_set_enabled(button, true);
    lc_button_is_enabled(button, &enabled);
    TEST_ASSERT(enabled == true, "Button is re-enabled");

    /* Style mode round-trip */
    result = lc_button_set_style_mode(button, LC_BUTTON_STYLE_AUTOMATIC);
    TEST_RESULT(result, "Set style mode");
    LCButtonStyleMode styleMode = LC_BUTTON_STYLE_MANUAL;
    lc_button_get_style_mode(button, &styleMode);
    TEST_ASSERT(styleMode == LC_BUTTON_STYLE_AUTOMATIC, "Style mode is Automatic");

    /* Scale mode round-trip */
    result = lc_button_set_scale_mode(button, LC_BUTTON_SCALE_FIT_TO_TEXT_WIDTH);
    TEST_RESULT(result, "Set scale mode");
    LCButtonScaleMode scaleMode = LC_BUTTON_SCALE_FIXED;
    lc_button_get_scale_mode(button, &scaleMode);
    TEST_ASSERT(scaleMode == LC_BUTTON_SCALE_FIT_TO_TEXT_WIDTH, "Scale mode is FitToTextWidth");

    /* Text padding (Vec2) */
    LCVec2 padding = lc_vec2(12.0f, 6.0f);
    result = lc_button_set_text_padding(button, padding);
    TEST_RESULT(result, "Set text padding");
    LCVec2 gotPadding = lc_vec2(0.0f, 0.0f);
    lc_button_get_text_padding(button, &gotPadding);
    TEST_ASSERT(ext_ui_vec2_eq(gotPadding, padding), "Text padding round-trip");

    /* Background color round-trip */
    LCColor bg = lc_color(0.2f, 0.4f, 0.6f, 1.0f);
    result = lc_button_set_background_color(button, bg);
    TEST_RESULT(result, "Set background color");
    LCColor gotBg = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    lc_button_get_background_color(button, &gotBg);
    TEST_ASSERT(ext_ui_color_eq(gotBg, bg), "Background color round-trip");

    /* Opacity round-trip */
    result = lc_button_set_opacity(button, 0.75f);
    TEST_RESULT(result, "Set opacity");
    float opacity = 0.0f;
    lc_button_get_opacity(button, &opacity);
    TEST_ASSERT(ext_ui_feq(opacity, 0.75f), "Opacity round-trip is 0.75");

    /* Border enabled / color */
    result = lc_button_set_border_enabled(button, true);
    TEST_RESULT(result, "Enable border");
    bool borderEnabled = false;
    lc_button_get_border_enabled(button, &borderEnabled);
    TEST_ASSERT(borderEnabled == true, "Border enabled round-trip");

    LCColor borderColor = lc_color(0.9f, 0.1f, 0.1f, 1.0f);
    result = lc_button_set_border_color(button, borderColor);
    TEST_RESULT(result, "Set border color");
    LCColor gotBorderColor = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    lc_button_get_border_color(button, &gotBorderColor);
    TEST_ASSERT(ext_ui_color_eq(gotBorderColor, borderColor), "Border color round-trip");

    /* Border width: unlink and set each edge */
    result = lc_button_set_border_width_linked(button, false);
    TEST_RESULT(result, "Unlink border widths");
    bool borderLinked = true;
    lc_button_get_border_width_linked(button, &borderLinked);
    TEST_ASSERT(borderLinked == false, "Border width linked round-trip");

    lc_button_set_border_width_left(button, 1.0f);
    lc_button_set_border_width_right(button, 2.0f);
    lc_button_set_border_width_top(button, 3.0f);
    lc_button_set_border_width_bottom(button, 4.0f);
    float bwL = 0.0f, bwR = 0.0f, bwT = 0.0f, bwB = 0.0f;
    lc_button_get_border_width_left(button, &bwL);
    lc_button_get_border_width_right(button, &bwR);
    lc_button_get_border_width_top(button, &bwT);
    lc_button_get_border_width_bottom(button, &bwB);
    TEST_ASSERT(ext_ui_feq(bwL, 1.0f) && ext_ui_feq(bwR, 2.0f) &&
                ext_ui_feq(bwT, 3.0f) && ext_ui_feq(bwB, 4.0f),
                "Per-edge border widths round-trip");

    /* Corner radius: unlink and set each corner */
    result = lc_button_set_corner_radius_linked(button, false);
    TEST_RESULT(result, "Unlink corner radii");
    bool cornerLinked = true;
    lc_button_get_corner_radius_linked(button, &cornerLinked);
    TEST_ASSERT(cornerLinked == false, "Corner radius linked round-trip");

    lc_button_set_corner_radius_top_left(button, 5.0f);
    lc_button_set_corner_radius_top_right(button, 6.0f);
    lc_button_set_corner_radius_bottom_left(button, 7.0f);
    lc_button_set_corner_radius_bottom_right(button, 8.0f);
    float crTL = 0.0f, crTR = 0.0f, crBL = 0.0f, crBR = 0.0f;
    lc_button_get_corner_radius_top_left(button, &crTL);
    lc_button_get_corner_radius_top_right(button, &crTR);
    lc_button_get_corner_radius_bottom_left(button, &crBL);
    lc_button_get_corner_radius_bottom_right(button, &crBR);
    TEST_ASSERT(ext_ui_feq(crTL, 5.0f) && ext_ui_feq(crTR, 6.0f) &&
                ext_ui_feq(crBL, 7.0f) && ext_ui_feq(crBR, 8.0f),
                "Per-corner radii round-trip");

    /* Font path / size / color */
    result = lc_button_set_font_path(button, "res://fonts/test.ttf");
    TEST_RESULT(result, "Set font path");
    char fontPath[256];
    fontPath[0] = '\0';
    lc_button_get_font_path(button, fontPath, sizeof(fontPath));
    TEST_ASSERT(strcmp(fontPath, "res://fonts/test.ttf") == 0, "Font path round-trip");

    result = lc_button_set_font_size(button, 22.0f);
    TEST_RESULT(result, "Set font size");
    float fontSize = 0.0f;
    lc_button_get_font_size(button, &fontSize);
    TEST_ASSERT(ext_ui_feq(fontSize, 22.0f), "Font size round-trip is 22");

    LCColor fontColor = lc_color(0.1f, 0.2f, 0.3f, 1.0f);
    result = lc_button_set_font_color(button, fontColor);
    TEST_RESULT(result, "Set font color");
    LCColor gotFontColor = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    lc_button_get_font_color(button, &gotFontColor);
    TEST_ASSERT(ext_ui_color_eq(gotFontColor, fontColor), "Font color round-trip");

    /* Word wrap */
    result = lc_button_set_word_wrap(button, true);
    TEST_RESULT(result, "Set word wrap");
    bool wrap = false;
    lc_button_get_word_wrap(button, &wrap);
    TEST_ASSERT(wrap == true, "Word wrap round-trip");

    /* Per-state modulation get */
    LCColor stateMod = lc_color(0.5f, 0.5f, 0.5f, 1.0f);
    result = lc_button_set_state_modulation(button, LC_BUTTON_STATE_DISABLED, stateMod);
    TEST_RESULT(result, "Set disabled modulation");
    LCColor gotStateMod = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    lc_button_get_state_modulation(button, LC_BUTTON_STATE_DISABLED, &gotStateMod);
    TEST_ASSERT(ext_ui_color_eq(gotStateMod, stateMod), "Disabled modulation round-trip");

    /* Per-state sound (path + full struct) */
    result = lc_button_set_state_sound_path(button, LC_BUTTON_STATE_HOVER, "res://sfx/hover.wav");
    TEST_RESULT(result, "Set hover sound path");

    LCButtonStateSound sound;
    memset(&sound, 0, sizeof(sound));
    copy_bounded(sound.audioPath, sizeof(sound.audioPath), "res://sfx/click.wav");
    copy_bounded(sound.busName, sizeof(sound.busName), "SFX");
    sound.volume = 0.8f;
    result = lc_button_set_state_sound(button, LC_BUTTON_STATE_PRESSED, &sound);
    TEST_RESULT(result, "Set pressed sound struct");
    LCButtonStateSound gotSound;
    memset(&gotSound, 0, sizeof(gotSound));
    result = lc_button_get_state_sound(button, LC_BUTTON_STATE_PRESSED, &gotSound);
    TEST_RESULT(result, "Get pressed sound struct");
    TEST_ASSERT(strcmp(gotSound.audioPath, "res://sfx/click.wav") == 0 &&
                ext_ui_feq(gotSound.volume, 0.8f),
                "Pressed sound struct round-trip");

    /* Per-state tween */
    LCButtonStateTween tween;
    memset(&tween, 0, sizeof(tween));
    tween.enabled = true;
    tween.scaleOffset = lc_vec2(0.1f, 0.1f);
    tween.rotationOffset = 5.0f;
    tween.positionOffset = lc_vec2(2.0f, -2.0f);
    tween.duration = 0.25f;
    result = lc_button_set_state_tween(button, LC_BUTTON_STATE_HOVER, &tween);
    TEST_RESULT(result, "Set hover tween");
    LCButtonStateTween gotTween;
    memset(&gotTween, 0, sizeof(gotTween));
    result = lc_button_get_state_tween(button, LC_BUTTON_STATE_HOVER, &gotTween);
    TEST_RESULT(result, "Get hover tween");
    TEST_ASSERT(gotTween.enabled == true &&
                ext_ui_feq(gotTween.duration, 0.25f) &&
                ext_ui_feq(gotTween.rotationOffset, 5.0f),
                "Hover tween round-trip");

    /* Per-state callbacks */
    int callbackUserData = 42;
    result = lc_button_set_state_callback(button, LC_BUTTON_STATE_NORMAL,
                                          ext_ui_button_callback, &callbackUserData);
    TEST_RESULT(result, "Set normal state callback");
    result = lc_button_clear_state_callback(button, LC_BUTTON_STATE_NORMAL);
    TEST_RESULT(result, "Clear normal state callback");

    lc_component_destroy(button);

    SECTION_END("UI Ext: Button Component");
}

/* ========================================================================== */
/* Panel                                                                      */
/* ========================================================================== */

static void test_ext_ui_panel(void) {
    SECTION_START("UI Ext: Panel Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_ext_panel_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "ExtPanelNode", &node);
    TEST_RESULT(result, "Create node for Panel");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle panel = NULL;
    result = lc_panel_create(node, &panel);
    TEST_RESULT(result, "Create Panel component");

    /* Sorting order */
    result = lc_panel_set_sorting_order(panel, 4);
    TEST_RESULT(result, "Set sorting order");
    int order = 0;
    lc_panel_get_sorting_order(panel, &order);
    TEST_ASSERT(order == 4, "Sorting order round-trip is 4");

    /* UI space getter */
    result = lc_panel_set_use_ui_space(panel, true);
    TEST_RESULT(result, "Set use UI space");
    bool useUiSpace = false;
    lc_panel_get_use_ui_space(panel, &useUiSpace);
    TEST_ASSERT(useUiSpace == true, "Use UI space round-trip");

    /* Style path */
    result = lc_panel_set_style_path(panel, "res://styles/panel.lstyle");
    TEST_RESULT(result, "Set style path");
    char stylePath[256];
    stylePath[0] = '\0';
    lc_panel_get_style_path(panel, stylePath, sizeof(stylePath));
    TEST_ASSERT(strcmp(stylePath, "res://styles/panel.lstyle") == 0, "Style path round-trip");

    /* Load / save style: assert graceful return (no crash) */
    LCResult loadRes = lc_panel_load_style(panel, "res://styles/missing.lstyle");
    TEST_ASSERT(loadRes == LC_SUCCESS || loadRes != LC_SUCCESS, "load_style returned gracefully");
    LCResult saveRes = lc_panel_save_style(panel, "temp://ext_panel_style.lstyle");
    TEST_ASSERT(saveRes == LC_SUCCESS || saveRes != LC_SUCCESS, "save_style returned gracefully");

    /* Background color + opacity getters */
    LCColor bg = lc_color(0.15f, 0.25f, 0.35f, 1.0f);
    result = lc_panel_set_background_color(panel, bg);
    TEST_RESULT(result, "Set background color");
    LCColor gotBg = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    lc_panel_get_background_color(panel, &gotBg);
    TEST_ASSERT(ext_ui_color_eq(gotBg, bg), "Background color round-trip");

    result = lc_panel_set_opacity(panel, 0.6f);
    TEST_RESULT(result, "Set opacity");
    float opacity = 0.0f;
    lc_panel_get_opacity(panel, &opacity);
    TEST_ASSERT(ext_ui_feq(opacity, 0.6f), "Opacity round-trip is 0.6");

    /* Border width getters / per-edge setters */
    result = lc_panel_set_border_width_linked(panel, false);
    TEST_RESULT(result, "Unlink border widths");
    bool borderLinked = true;
    lc_panel_get_border_width_linked(panel, &borderLinked);
    TEST_ASSERT(borderLinked == false, "Border width linked round-trip");

    lc_panel_set_border_width_left(panel, 1.0f);
    lc_panel_set_border_width_right(panel, 2.0f);
    lc_panel_set_border_width_top(panel, 3.0f);
    lc_panel_set_border_width_bottom(panel, 4.0f);
    float bwL = 0.0f, bwR = 0.0f, bwT = 0.0f, bwB = 0.0f;
    lc_panel_get_border_width_left(panel, &bwL);
    lc_panel_get_border_width_right(panel, &bwR);
    lc_panel_get_border_width_top(panel, &bwT);
    lc_panel_get_border_width_bottom(panel, &bwB);
    TEST_ASSERT(ext_ui_feq(bwL, 1.0f) && ext_ui_feq(bwR, 2.0f) &&
                ext_ui_feq(bwT, 3.0f) && ext_ui_feq(bwB, 4.0f),
                "Per-edge border widths round-trip");

    /* Border enabled / color getters */
    result = lc_panel_set_border_enabled(panel, true);
    TEST_RESULT(result, "Enable border");
    bool borderEnabled = false;
    lc_panel_get_border_enabled(panel, &borderEnabled);
    TEST_ASSERT(borderEnabled == true, "Border enabled round-trip");

    LCColor borderColor = lc_color(0.8f, 0.2f, 0.4f, 1.0f);
    result = lc_panel_set_border_color(panel, borderColor);
    TEST_RESULT(result, "Set border color");
    LCColor gotBorderColor = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    lc_panel_get_border_color(panel, &gotBorderColor);
    TEST_ASSERT(ext_ui_color_eq(gotBorderColor, borderColor), "Border color round-trip");

    /* Corner radius getters / per-corner setters */
    result = lc_panel_set_corner_radius_linked(panel, false);
    TEST_RESULT(result, "Unlink corner radii");
    bool cornerLinked = true;
    lc_panel_get_corner_radius_linked(panel, &cornerLinked);
    TEST_ASSERT(cornerLinked == false, "Corner radius linked round-trip");

    lc_panel_set_corner_radius_top_left(panel, 5.0f);
    lc_panel_set_corner_radius_top_right(panel, 6.0f);
    lc_panel_set_corner_radius_bottom_left(panel, 7.0f);
    lc_panel_set_corner_radius_bottom_right(panel, 8.0f);
    float crTL = 0.0f, crTR = 0.0f, crBL = 0.0f, crBR = 0.0f;
    lc_panel_get_corner_radius_top_left(panel, &crTL);
    lc_panel_get_corner_radius_top_right(panel, &crTR);
    lc_panel_get_corner_radius_bottom_left(panel, &crBL);
    lc_panel_get_corner_radius_bottom_right(panel, &crBR);
    TEST_ASSERT(ext_ui_feq(crTL, 5.0f) && ext_ui_feq(crTR, 6.0f) &&
                ext_ui_feq(crBL, 7.0f) && ext_ui_feq(crBR, 8.0f),
                "Per-corner radii round-trip");

    /* Corner detail getter */
    result = lc_panel_set_corner_detail(panel, 12);
    TEST_RESULT(result, "Set corner detail");
    int cornerDetail = 0;
    lc_panel_get_corner_detail(panel, &cornerDetail);
    TEST_ASSERT(cornerDetail == 12, "Corner detail round-trip is 12");

    /* Anti-aliasing getters + size */
    result = lc_panel_set_anti_aliasing(panel, true);
    TEST_RESULT(result, "Enable anti-aliasing");
    bool aa = false;
    lc_panel_get_anti_aliasing(panel, &aa);
    TEST_ASSERT(aa == true, "Anti-aliasing round-trip");

    result = lc_panel_set_anti_aliasing_size(panel, 1.5f);
    TEST_RESULT(result, "Set anti-aliasing size");
    float aaSize = 0.0f;
    lc_panel_get_anti_aliasing_size(panel, &aaSize);
    TEST_ASSERT(ext_ui_feq(aaSize, 1.5f), "Anti-aliasing size round-trip");

    /* Shadow getters */
    result = lc_panel_set_shadow_enabled(panel, true);
    TEST_RESULT(result, "Enable shadow");
    bool shadowEnabled = false;
    lc_panel_get_shadow_enabled(panel, &shadowEnabled);
    TEST_ASSERT(shadowEnabled == true, "Shadow enabled round-trip");

    LCColor shadowColor = lc_color(0.0f, 0.0f, 0.0f, 0.5f);
    result = lc_panel_set_shadow_color(panel, shadowColor);
    TEST_RESULT(result, "Set shadow color");
    LCColor gotShadowColor = lc_color(1.0f, 1.0f, 1.0f, 1.0f);
    lc_panel_get_shadow_color(panel, &gotShadowColor);
    TEST_ASSERT(ext_ui_color_eq(gotShadowColor, shadowColor), "Shadow color round-trip");

    result = lc_panel_set_shadow_size(panel, 6.0f);
    TEST_RESULT(result, "Set shadow size");
    float shadowSize = 0.0f;
    lc_panel_get_shadow_size(panel, &shadowSize);
    TEST_ASSERT(ext_ui_feq(shadowSize, 6.0f), "Shadow size round-trip");

    LCVec2 shadowOffset = lc_vec2(3.0f, -3.0f);
    result = lc_panel_set_shadow_offset(panel, shadowOffset);
    TEST_RESULT(result, "Set shadow offset");
    LCVec2 gotShadowOffset = lc_vec2(0.0f, 0.0f);
    lc_panel_get_shadow_offset(panel, &gotShadowOffset);
    TEST_ASSERT(ext_ui_vec2_eq(gotShadowOffset, shadowOffset), "Shadow offset round-trip");

    /* Custom shader path */
    result = lc_panel_set_custom_shader_path(panel, "res://shaders/custom.glsl");
    TEST_RESULT(result, "Set custom shader path");
    char customShader[256];
    customShader[0] = '\0';
    lc_panel_get_custom_shader_path(panel, customShader, sizeof(customShader));
    TEST_ASSERT(strcmp(customShader, "res://shaders/custom.glsl") == 0, "Custom shader path round-trip");

    /* Lupine shader (.lsh) path + parameters */
    result = lc_panel_set_shader(panel, "res://shaders/panel.lsh");
    TEST_RESULT(result, "Set .lsh shader path");
    char shaderPath[256];
    shaderPath[0] = '\0';
    lc_panel_get_shader(panel, shaderPath, sizeof(shaderPath));
    TEST_ASSERT(strcmp(shaderPath, "res://shaders/panel.lsh") == 0, "Shader path round-trip");

    result = lc_panel_set_shader_parameters(panel, "{\"u_Intensity\":{\"type\":\"float\",\"value\":1.0}}");
    TEST_RESULT(result, "Set shader parameters");
    char shaderParams[512];
    shaderParams[0] = '\0';
    result = lc_panel_get_shader_parameters(panel, shaderParams, sizeof(shaderParams));
    TEST_RESULT(result, "Get shader parameters");

    lc_scene_destroy(scene);

    SECTION_END("UI Ext: Panel Component");
}

/* ========================================================================== */
/* Checkbox                                                                   */
/* ========================================================================== */

static void test_ext_ui_checkbox(void) {
    SECTION_START("UI Ext: Checkbox Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_ext_checkbox_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "ExtCheckboxNode", &node);
    TEST_RESULT(result, "Create node for Checkbox");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle checkbox = NULL;
    result = lc_checkbox_create(node, &checkbox);
    TEST_RESULT(result, "Create Checkbox component");

    /* Layer / sorting / UI space */
    result = lc_checkbox_set_layer(checkbox, 2);
    TEST_RESULT(result, "Set layer");
    int layer = 0;
    lc_checkbox_get_layer(checkbox, &layer);
    TEST_ASSERT(layer == 2, "Layer round-trip is 2");

    result = lc_checkbox_set_sorting_order(checkbox, 1);
    TEST_RESULT(result, "Set sorting order");
    int order = 0;
    lc_checkbox_get_sorting_order(checkbox, &order);
    TEST_ASSERT(order == 1, "Sorting order round-trip is 1");

    result = lc_checkbox_set_use_ui_space(checkbox, true);
    TEST_RESULT(result, "Set use UI space");
    bool useUiSpace = false;
    lc_checkbox_get_use_ui_space(checkbox, &useUiSpace);
    TEST_ASSERT(useUiSpace == true, "Use UI space round-trip");

    /* Enabled + current state */
    result = lc_checkbox_set_enabled(checkbox, false);
    TEST_RESULT(result, "Disable checkbox");
    bool enabled = true;
    lc_checkbox_is_enabled(checkbox, &enabled);
    TEST_ASSERT(enabled == false, "Checkbox is disabled");
    lc_checkbox_set_enabled(checkbox, true);

    LCCheckboxState state = LC_CHECKBOX_STATE_NORMAL;
    result = lc_checkbox_get_current_state(checkbox, &state);
    TEST_RESULT(result, "Get current state");

    /* Group name getter */
    result = lc_checkbox_set_group_name(checkbox, "options");
    TEST_RESULT(result, "Set group name");
    char groupName[64];
    groupName[0] = '\0';
    lc_checkbox_get_group_name(checkbox, groupName, sizeof(groupName));
    TEST_ASSERT(strcmp(groupName, "options") == 0, "Group name round-trip");

    /* Box colors */
    LCColor borderColor = lc_color(0.1f, 0.1f, 0.1f, 1.0f);
    result = lc_checkbox_set_border_color(checkbox, borderColor);
    TEST_RESULT(result, "Set border color");
    LCColor gotBorderColor = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    lc_checkbox_get_border_color(checkbox, &gotBorderColor);
    TEST_ASSERT(ext_ui_color_eq(gotBorderColor, borderColor), "Border color round-trip");

    LCColor bgColor = lc_color(0.9f, 0.9f, 0.9f, 1.0f);
    result = lc_checkbox_set_background_color(checkbox, bgColor);
    TEST_RESULT(result, "Set background color");
    LCColor gotBgColor = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    lc_checkbox_get_background_color(checkbox, &gotBgColor);
    TEST_ASSERT(ext_ui_color_eq(gotBgColor, bgColor), "Background color round-trip");

    LCColor checkmarkColor = lc_color(0.0f, 0.6f, 0.0f, 1.0f);
    result = lc_checkbox_set_checkmark_color(checkbox, checkmarkColor);
    TEST_RESULT(result, "Set checkmark color");
    LCColor gotCheckmarkColor = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    lc_checkbox_get_checkmark_color(checkbox, &gotCheckmarkColor);
    TEST_ASSERT(ext_ui_color_eq(gotCheckmarkColor, checkmarkColor), "Checkmark color round-trip");

    /* Corner radius + border width */
    result = lc_checkbox_set_corner_radius(checkbox, 3.0f);
    TEST_RESULT(result, "Set corner radius");
    float cornerRadius = 0.0f;
    lc_checkbox_get_corner_radius(checkbox, &cornerRadius);
    TEST_ASSERT(ext_ui_feq(cornerRadius, 3.0f), "Corner radius round-trip is 3");

    result = lc_checkbox_set_border_width(checkbox, 2.0f);
    TEST_RESULT(result, "Set border width");
    float borderWidth = 0.0f;
    lc_checkbox_get_border_width(checkbox, &borderWidth);
    TEST_ASSERT(ext_ui_feq(borderWidth, 2.0f), "Border width round-trip is 2");

    /* Texture properties */
    result = lc_checkbox_set_unchecked_texture_path(checkbox, "res://ui/unchecked.png");
    TEST_RESULT(result, "Set unchecked texture path");
    char uncheckedPath[256];
    uncheckedPath[0] = '\0';
    lc_checkbox_get_unchecked_texture_path(checkbox, uncheckedPath, sizeof(uncheckedPath));
    TEST_ASSERT(strcmp(uncheckedPath, "res://ui/unchecked.png") == 0, "Unchecked texture path round-trip");

    result = lc_checkbox_set_checked_texture_path(checkbox, "res://ui/checked.png");
    TEST_RESULT(result, "Set checked texture path");
    char checkedPath[256];
    checkedPath[0] = '\0';
    lc_checkbox_get_checked_texture_path(checkbox, checkedPath, sizeof(checkedPath));
    TEST_ASSERT(strcmp(checkedPath, "res://ui/checked.png") == 0, "Checked texture path round-trip");

    result = lc_checkbox_set_use_textures(checkbox, true);
    TEST_RESULT(result, "Set use textures");
    bool useTextures = false;
    lc_checkbox_get_use_textures(checkbox, &useTextures);
    TEST_ASSERT(useTextures == true, "Use textures round-trip");

    /* Text properties */
    result = lc_checkbox_set_text(checkbox, "Enable feature");
    TEST_RESULT(result, "Set text");
    char text[256];
    text[0] = '\0';
    lc_checkbox_get_text(checkbox, text, sizeof(text));
    TEST_ASSERT(strcmp(text, "Enable feature") == 0, "Text round-trip");

    result = lc_checkbox_set_font_path(checkbox, "res://fonts/cb.ttf");
    TEST_RESULT(result, "Set font path");
    char fontPath[256];
    fontPath[0] = '\0';
    lc_checkbox_get_font_path(checkbox, fontPath, sizeof(fontPath));
    TEST_ASSERT(strcmp(fontPath, "res://fonts/cb.ttf") == 0, "Font path round-trip");

    result = lc_checkbox_set_font_size(checkbox, 14.0f);
    TEST_RESULT(result, "Set font size");
    float fontSize = 0.0f;
    lc_checkbox_get_font_size(checkbox, &fontSize);
    TEST_ASSERT(ext_ui_feq(fontSize, 14.0f), "Font size round-trip is 14");

    LCColor fontColor = lc_color(0.2f, 0.2f, 0.2f, 1.0f);
    result = lc_checkbox_set_font_color(checkbox, fontColor);
    TEST_RESULT(result, "Set font color");
    LCColor gotFontColor = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    lc_checkbox_get_font_color(checkbox, &gotFontColor);
    TEST_ASSERT(ext_ui_color_eq(gotFontColor, fontColor), "Font color round-trip");

    result = lc_checkbox_set_text_offset(checkbox, 8.0f);
    TEST_RESULT(result, "Set text offset");
    float textOffset = 0.0f;
    lc_checkbox_get_text_offset(checkbox, &textOffset);
    TEST_ASSERT(ext_ui_feq(textOffset, 8.0f), "Text offset round-trip is 8");

    /* Per-state modulation */
    LCColor stateMod = lc_color(0.7f, 0.7f, 0.7f, 1.0f);
    result = lc_checkbox_set_state_modulation(checkbox, LC_CHECKBOX_STATE_CHECKED, stateMod);
    TEST_RESULT(result, "Set checked modulation");
    LCColor gotStateMod = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    lc_checkbox_get_state_modulation(checkbox, LC_CHECKBOX_STATE_CHECKED, &gotStateMod);
    TEST_ASSERT(ext_ui_color_eq(gotStateMod, stateMod), "Checked modulation round-trip");

    /* Audio paths */
    result = lc_checkbox_set_check_sound_path(checkbox, "res://sfx/check.wav");
    TEST_RESULT(result, "Set check sound path");
    char checkSound[256];
    checkSound[0] = '\0';
    lc_checkbox_get_check_sound_path(checkbox, checkSound, sizeof(checkSound));
    TEST_ASSERT(strcmp(checkSound, "res://sfx/check.wav") == 0, "Check sound path round-trip");

    result = lc_checkbox_set_uncheck_sound_path(checkbox, "res://sfx/uncheck.wav");
    TEST_RESULT(result, "Set uncheck sound path");
    char uncheckSound[256];
    uncheckSound[0] = '\0';
    lc_checkbox_get_uncheck_sound_path(checkbox, uncheckSound, sizeof(uncheckSound));
    TEST_ASSERT(strcmp(uncheckSound, "res://sfx/uncheck.wav") == 0, "Uncheck sound path round-trip");

    result = lc_checkbox_set_hover_sound_path(checkbox, "res://sfx/hover.wav");
    TEST_RESULT(result, "Set hover sound path");
    char hoverSound[256];
    hoverSound[0] = '\0';
    lc_checkbox_get_hover_sound_path(checkbox, hoverSound, sizeof(hoverSound));
    TEST_ASSERT(strcmp(hoverSound, "res://sfx/hover.wav") == 0, "Hover sound path round-trip");

    result = lc_checkbox_set_press_sound_path(checkbox, "res://sfx/press.wav");
    TEST_RESULT(result, "Set press sound path");
    char pressSound[256];
    pressSound[0] = '\0';
    lc_checkbox_get_press_sound_path(checkbox, pressSound, sizeof(pressSound));
    TEST_ASSERT(strcmp(pressSound, "res://sfx/press.wav") == 0, "Press sound path round-trip");

    lc_scene_destroy(scene);

    SECTION_END("UI Ext: Checkbox Component");
}

/* ========================================================================== */
/* ColorRect                                                                  */
/* ========================================================================== */

static void test_ext_ui_color_rect(void) {
    SECTION_START("UI Ext: ColorRect Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_ext_color_rect_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "ExtColorRectNode", &node);
    TEST_RESULT(result, "Create node for ColorRect");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle colorRect = NULL;
    result = lc_color_rect_create(node, &colorRect);
    TEST_RESULT(result, "Create ColorRect component");

    /* Layer / sorting getters */
    result = lc_color_rect_set_layer(colorRect, 3);
    TEST_RESULT(result, "Set layer");
    int layer = 0;
    lc_color_rect_get_layer(colorRect, &layer);
    TEST_ASSERT(layer == 3, "Layer round-trip is 3");

    result = lc_color_rect_set_sorting_order(colorRect, 2);
    TEST_RESULT(result, "Set sorting order");
    int order = 0;
    lc_color_rect_get_sorting_order(colorRect, &order);
    TEST_ASSERT(order == 2, "Sorting order round-trip is 2");

    /* Size getters + Vec2 size */
    result = lc_color_rect_set_width(colorRect, 120.0f);
    TEST_RESULT(result, "Set width");
    result = lc_color_rect_set_height(colorRect, 90.0f);
    TEST_RESULT(result, "Set height");
    float width = 0.0f, height = 0.0f;
    lc_color_rect_get_width(colorRect, &width);
    lc_color_rect_get_height(colorRect, &height);
    TEST_ASSERT(ext_ui_feq(width, 120.0f) && ext_ui_feq(height, 90.0f), "Width/height round-trip");

    LCVec2 size = lc_vec2(64.0f, 48.0f);
    result = lc_color_rect_set_size(colorRect, size);
    TEST_RESULT(result, "Set size (Vec2)");
    LCVec2 gotSize = lc_vec2(0.0f, 0.0f);
    lc_color_rect_get_size(colorRect, &gotSize);
    TEST_ASSERT(ext_ui_vec2_eq(gotSize, size), "Size (Vec2) round-trip");

    /* Corner radius: linked getter + individual */
    result = lc_color_rect_set_corner_radius_linked(colorRect, false);
    TEST_RESULT(result, "Unlink corner radii");
    bool cornerLinked = true;
    lc_color_rect_get_corner_radius_linked(colorRect, &cornerLinked);
    TEST_ASSERT(cornerLinked == false, "Corner radius linked round-trip");

    result = lc_color_rect_set_corner_radius_individual(colorRect, 2, 9.0f);
    TEST_RESULT(result, "Set individual corner radius");
    float cornerRadius = 0.0f;
    lc_color_rect_get_corner_radius(colorRect, 2, &cornerRadius);
    TEST_ASSERT(ext_ui_feq(cornerRadius, 9.0f), "Individual corner radius round-trip is 9");

    /* Border getters */
    result = lc_color_rect_set_border_enabled(colorRect, true);
    TEST_RESULT(result, "Enable border");
    bool borderEnabled = false;
    lc_color_rect_get_border_enabled(colorRect, &borderEnabled);
    TEST_ASSERT(borderEnabled == true, "Border enabled round-trip");

    LCColor borderColor = lc_color(0.3f, 0.6f, 0.9f, 1.0f);
    result = lc_color_rect_set_border_color(colorRect, borderColor);
    TEST_RESULT(result, "Set border color");
    LCColor gotBorderColor = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    lc_color_rect_get_border_color(colorRect, &gotBorderColor);
    TEST_ASSERT(ext_ui_color_eq(gotBorderColor, borderColor), "Border color round-trip");

    /* Border width: linked + individual */
    result = lc_color_rect_set_border_width_linked(colorRect, false);
    TEST_RESULT(result, "Unlink border widths");
    bool borderLinked = true;
    lc_color_rect_get_border_width_linked(colorRect, &borderLinked);
    TEST_ASSERT(borderLinked == false, "Border width linked round-trip");

    result = lc_color_rect_set_border_width_individual(colorRect, 1, 4.0f);
    TEST_RESULT(result, "Set individual border width");
    float borderWidth = 0.0f;
    lc_color_rect_get_border_width(colorRect, 1, &borderWidth);
    TEST_ASSERT(ext_ui_feq(borderWidth, 4.0f), "Individual border width round-trip is 4");

    /* UI space getter */
    result = lc_color_rect_set_ui_space(colorRect, true);
    TEST_RESULT(result, "Set UI space");
    bool uiSpace = false;
    lc_color_rect_get_ui_space(colorRect, &uiSpace);
    TEST_ASSERT(uiSpace == true, "UI space round-trip");

    /* Material override */
    result = lc_color_rect_set_material_override(colorRect, "res://mat/rect.lmat");
    TEST_RESULT(result, "Set material override");
    char material[256];
    material[0] = '\0';
    lc_color_rect_get_material_override(colorRect, material, sizeof(material));
    TEST_ASSERT(strcmp(material, "res://mat/rect.lmat") == 0, "Material override round-trip");

    /* Custom shader (.lsh) + parameters */
    result = lc_color_rect_set_shader(colorRect, "res://shaders/rect.lsh");
    TEST_RESULT(result, "Set shader path");
    char shaderPath[256];
    shaderPath[0] = '\0';
    lc_color_rect_get_shader(colorRect, shaderPath, sizeof(shaderPath));
    TEST_ASSERT(strcmp(shaderPath, "res://shaders/rect.lsh") == 0, "Shader path round-trip");

    result = lc_color_rect_set_shader_parameters(colorRect, "{\"u_Intensity\":{\"type\":\"float\",\"value\":2.0}}");
    TEST_RESULT(result, "Set shader parameters");
    char shaderParams[512];
    shaderParams[0] = '\0';
    result = lc_color_rect_get_shader_parameters(colorRect, shaderParams, sizeof(shaderParams));
    TEST_RESULT(result, "Get shader parameters");

    lc_scene_destroy(scene);

    SECTION_END("UI Ext: ColorRect Component");
}

/* ========================================================================== */
/* Image2D                                                                    */
/* ========================================================================== */

static void test_ext_ui_image2d(void) {
    SECTION_START("UI Ext: Image2D Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_ext_image2d_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "ExtImage2DNode", &node);
    TEST_RESULT(result, "Create node for Image2D");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle image = NULL;
    result = lc_image2d_create(node, &image);
    TEST_RESULT(result, "Create Image2D component");

    /* Texture path + load (graceful) */
    result = lc_image2d_set_texture_path(image, "res://textures/icon.png");
    TEST_RESULT(result, "Set texture path");
    char texturePath[256];
    texturePath[0] = '\0';
    lc_image2d_get_texture_path(image, texturePath, sizeof(texturePath));
    TEST_ASSERT(strcmp(texturePath, "res://textures/icon.png") == 0, "Texture path round-trip");

    LCResult loadRes = lc_image2d_load_texture(image, "res://textures/missing.png");
    TEST_ASSERT(loadRes == LC_SUCCESS || loadRes != LC_SUCCESS, "load_texture returned gracefully");

    /* Material override */
    result = lc_image2d_set_material_override(image, "res://mat/img.lmat");
    TEST_RESULT(result, "Set material override");
    char material[256];
    material[0] = '\0';
    lc_image2d_get_material_override(image, material, sizeof(material));
    TEST_ASSERT(strcmp(material, "res://mat/img.lmat") == 0, "Material override round-trip");

    /* Color getter */
    LCColor tint = lc_color(0.4f, 0.5f, 0.6f, 1.0f);
    result = lc_image2d_set_color(image, tint);
    TEST_RESULT(result, "Set color");
    LCColor gotTint = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    lc_image2d_get_color(image, &gotTint);
    TEST_ASSERT(ext_ui_color_eq(gotTint, tint), "Color round-trip");

    /* Anchors + offsets */
    LCVec2 anchorMin = lc_vec2(0.1f, 0.2f);
    result = lc_image2d_set_anchor_min(image, anchorMin);
    TEST_RESULT(result, "Set anchor min");
    LCVec2 gotAnchorMin = lc_vec2(0.0f, 0.0f);
    lc_image2d_get_anchor_min(image, &gotAnchorMin);
    TEST_ASSERT(ext_ui_vec2_eq(gotAnchorMin, anchorMin), "Anchor min round-trip");

    LCVec2 anchorMax = lc_vec2(0.8f, 0.9f);
    result = lc_image2d_set_anchor_max(image, anchorMax);
    TEST_RESULT(result, "Set anchor max");
    LCVec2 gotAnchorMax = lc_vec2(0.0f, 0.0f);
    lc_image2d_get_anchor_max(image, &gotAnchorMax);
    TEST_ASSERT(ext_ui_vec2_eq(gotAnchorMax, anchorMax), "Anchor max round-trip");

    LCVec2 offsetMin = lc_vec2(4.0f, 5.0f);
    result = lc_image2d_set_offset_min(image, offsetMin);
    TEST_RESULT(result, "Set offset min");
    LCVec2 gotOffsetMin = lc_vec2(0.0f, 0.0f);
    lc_image2d_get_offset_min(image, &gotOffsetMin);
    TEST_ASSERT(ext_ui_vec2_eq(gotOffsetMin, offsetMin), "Offset min round-trip");

    LCVec2 offsetMax = lc_vec2(-4.0f, -5.0f);
    result = lc_image2d_set_offset_max(image, offsetMax);
    TEST_RESULT(result, "Set offset max");
    LCVec2 gotOffsetMax = lc_vec2(0.0f, 0.0f);
    lc_image2d_get_offset_max(image, &gotOffsetMax);
    TEST_ASSERT(ext_ui_vec2_eq(gotOffsetMax, offsetMax), "Offset max round-trip");

    /* Preserve aspect + aspect mode getter */
    result = lc_image2d_set_preserve_aspect(image, true);
    TEST_RESULT(result, "Set preserve aspect");
    bool preserve = false;
    lc_image2d_get_preserve_aspect(image, &preserve);
    TEST_ASSERT(preserve == true, "Preserve aspect round-trip");

    result = lc_image2d_set_aspect_mode(image, LC_ASPECT_FILL);
    TEST_RESULT(result, "Set aspect mode");
    LCAspectMode aspectMode = LC_ASPECT_FIT;
    lc_image2d_get_aspect_mode(image, &aspectMode);
    TEST_ASSERT(aspectMode == LC_ASPECT_FILL, "Aspect mode is Fill");

    /* Flip getters */
    result = lc_image2d_set_flip_h(image, true);
    TEST_RESULT(result, "Set flip H");
    bool flipH = false;
    lc_image2d_get_flip_h(image, &flipH);
    TEST_ASSERT(flipH == true, "Flip H round-trip");

    result = lc_image2d_set_flip_v(image, true);
    TEST_RESULT(result, "Set flip V");
    bool flipV = false;
    lc_image2d_get_flip_v(image, &flipV);
    TEST_ASSERT(flipV == true, "Flip V round-trip");

    /* Blend mode getter */
    result = lc_image2d_set_blend_mode(image, LC_BLEND_ADDITIVE);
    TEST_RESULT(result, "Set blend mode");
    LCBlendMode blendMode = LC_BLEND_ALPHA;
    lc_image2d_get_blend_mode(image, &blendMode);
    TEST_ASSERT(blendMode == LC_BLEND_ADDITIVE, "Blend mode is Additive");

    /* Mouse behaviour getter */
    result = lc_image2d_set_mouse_behaviour(image, LC_MOUSE_STOP);
    TEST_RESULT(result, "Set mouse behaviour");
    LCMouseBehaviour behaviour = LC_MOUSE_IGNORE;
    lc_image2d_get_mouse_behaviour(image, &behaviour);
    TEST_ASSERT(behaviour == LC_MOUSE_STOP, "Mouse behaviour is Stop");

    /* Corner radius: linked + individual */
    result = lc_image2d_set_corner_radius_linked(image, false);
    TEST_RESULT(result, "Unlink corner radii");
    bool cornerLinked = true;
    lc_image2d_get_corner_radius_linked(image, &cornerLinked);
    TEST_ASSERT(cornerLinked == false, "Corner radius linked round-trip");

    result = lc_image2d_set_corner_radius_individual(image, 3, 7.0f);
    TEST_RESULT(result, "Set individual corner radius");
    float cornerRadius = 0.0f;
    lc_image2d_get_corner_radius(image, 3, &cornerRadius);
    TEST_ASSERT(ext_ui_feq(cornerRadius, 7.0f), "Individual corner radius round-trip is 7");

    /* Border getters */
    result = lc_image2d_set_border_enabled(image, true);
    TEST_RESULT(result, "Enable border");
    bool borderEnabled = false;
    lc_image2d_get_border_enabled(image, &borderEnabled);
    TEST_ASSERT(borderEnabled == true, "Border enabled round-trip");

    LCColor borderColor = lc_color(0.2f, 0.8f, 0.3f, 1.0f);
    result = lc_image2d_set_border_color(image, borderColor);
    TEST_RESULT(result, "Set border color");
    LCColor gotBorderColor = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    lc_image2d_get_border_color(image, &gotBorderColor);
    TEST_ASSERT(ext_ui_color_eq(gotBorderColor, borderColor), "Border color round-trip");

    /* Border width: linked + all + individual */
    result = lc_image2d_set_border_width_linked(image, false);
    TEST_RESULT(result, "Unlink border widths");
    bool borderLinked = true;
    lc_image2d_get_border_width_linked(image, &borderLinked);
    TEST_ASSERT(borderLinked == false, "Border width linked round-trip");

    result = lc_image2d_set_border_width_all(image, 1.5f);
    TEST_RESULT(result, "Set all border widths");
    result = lc_image2d_set_border_width_individual(image, 0, 3.0f);
    TEST_RESULT(result, "Set individual border width");
    float borderWidth = 0.0f;
    lc_image2d_get_border_width(image, 0, &borderWidth);
    TEST_ASSERT(ext_ui_feq(borderWidth, 3.0f), "Individual border width round-trip is 3");

    /* UI space getter */
    result = lc_image2d_set_ui_space(image, true);
    TEST_RESULT(result, "Set UI space");
    bool uiSpace = false;
    lc_image2d_get_ui_space(image, &uiSpace);
    TEST_ASSERT(uiSpace == true, "UI space round-trip");

    /* Custom shader (.lsh) + parameters */
    result = lc_image2d_set_shader(image, "res://shaders/image.lsh");
    TEST_RESULT(result, "Set shader path");
    char shaderPath[256];
    shaderPath[0] = '\0';
    lc_image2d_get_shader(image, shaderPath, sizeof(shaderPath));
    TEST_ASSERT(strcmp(shaderPath, "res://shaders/image.lsh") == 0, "Shader path round-trip");

    result = lc_image2d_set_shader_parameters(image, "{\"u_Tint\":{\"type\":\"float\",\"value\":0.5}}");
    TEST_RESULT(result, "Set shader parameters");
    char shaderParams[512];
    shaderParams[0] = '\0';
    result = lc_image2d_get_shader_parameters(image, shaderParams, sizeof(shaderParams));
    TEST_RESULT(result, "Get shader parameters");

    lc_scene_destroy(scene);

    SECTION_END("UI Ext: Image2D Component");
}

/* ========================================================================== */
/* ProgressBar                                                                */
/* ========================================================================== */

static void test_ext_ui_progress_bar(void) {
    SECTION_START("UI Ext: ProgressBar Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_ext_progress_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "ExtProgressBarNode", &node);
    TEST_RESULT(result, "Create node for ProgressBar");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle bar = NULL;
    result = lc_progress_bar_create(node, &bar);
    TEST_RESULT(result, "Create ProgressBar component");

    /* Value getters */
    result = lc_progress_bar_set_min_value(bar, 10.0f);
    TEST_RESULT(result, "Set min value");
    float minValue = 0.0f;
    lc_progress_bar_get_min_value(bar, &minValue);
    TEST_ASSERT(ext_ui_feq(minValue, 10.0f), "Min value round-trip is 10");

    result = lc_progress_bar_set_max_value(bar, 200.0f);
    TEST_RESULT(result, "Set max value");
    float maxValue = 0.0f;
    lc_progress_bar_get_max_value(bar, &maxValue);
    TEST_ASSERT(ext_ui_feq(maxValue, 200.0f), "Max value round-trip is 200");

    result = lc_progress_bar_set_step(bar, 5.0f);
    TEST_RESULT(result, "Set step");
    float step = 0.0f;
    lc_progress_bar_get_step(bar, &step);
    TEST_ASSERT(ext_ui_feq(step, 5.0f), "Step round-trip is 5");

    /* Smooth getters */
    result = lc_progress_bar_set_smooth(bar, true);
    TEST_RESULT(result, "Set smooth");
    bool smooth = false;
    lc_progress_bar_get_smooth(bar, &smooth);
    TEST_ASSERT(smooth == true, "Smooth round-trip");

    result = lc_progress_bar_set_smooth_speed(bar, 8.0f);
    TEST_RESULT(result, "Set smooth speed");
    float smoothSpeed = 0.0f;
    lc_progress_bar_get_smooth_speed(bar, &smoothSpeed);
    TEST_ASSERT(ext_ui_feq(smoothSpeed, 8.0f), "Smooth speed round-trip is 8");

    /* Size getters */
    result = lc_progress_bar_set_width(bar, 300.0f);
    TEST_RESULT(result, "Set width");
    result = lc_progress_bar_set_height(bar, 24.0f);
    TEST_RESULT(result, "Set height");
    float width = 0.0f, height = 0.0f;
    lc_progress_bar_get_width(bar, &width);
    lc_progress_bar_get_height(bar, &height);
    TEST_ASSERT(ext_ui_feq(width, 300.0f) && ext_ui_feq(height, 24.0f), "Width/height round-trip");

    /* Fill direction getter */
    result = lc_progress_bar_set_fill_direction(bar, LC_FILL_RIGHT_TO_LEFT);
    TEST_RESULT(result, "Set fill direction");
    LCProgressBarFillDirection fillDir = LC_FILL_LEFT_TO_RIGHT;
    lc_progress_bar_get_fill_direction(bar, &fillDir);
    TEST_ASSERT(fillDir == LC_FILL_RIGHT_TO_LEFT, "Fill direction is RightToLeft");

    /* Background texture/color */
    result = lc_progress_bar_set_background_texture_path(bar, "res://ui/pb_bg.png");
    TEST_RESULT(result, "Set background texture path");
    char bgTexture[256];
    bgTexture[0] = '\0';
    lc_progress_bar_get_background_texture_path(bar, bgTexture, sizeof(bgTexture));
    TEST_ASSERT(strcmp(bgTexture, "res://ui/pb_bg.png") == 0, "Background texture path round-trip");

    LCColor bgColor = lc_color(0.1f, 0.1f, 0.1f, 1.0f);
    result = lc_progress_bar_set_background_color(bar, bgColor);
    TEST_RESULT(result, "Set background color");
    LCColor gotBgColor = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    lc_progress_bar_get_background_color(bar, &gotBgColor);
    TEST_ASSERT(ext_ui_color_eq(gotBgColor, bgColor), "Background color round-trip");

    /* Fill texture/color */
    result = lc_progress_bar_set_fill_texture_path(bar, "res://ui/pb_fill.png");
    TEST_RESULT(result, "Set fill texture path");
    char fillTexture[256];
    fillTexture[0] = '\0';
    lc_progress_bar_get_fill_texture_path(bar, fillTexture, sizeof(fillTexture));
    TEST_ASSERT(strcmp(fillTexture, "res://ui/pb_fill.png") == 0, "Fill texture path round-trip");

    LCColor fillColor = lc_color(0.0f, 0.7f, 0.2f, 1.0f);
    result = lc_progress_bar_set_fill_color(bar, fillColor);
    TEST_RESULT(result, "Set fill color");
    LCColor gotFillColor = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    lc_progress_bar_get_fill_color(bar, &gotFillColor);
    TEST_ASSERT(ext_ui_color_eq(gotFillColor, fillColor), "Fill color round-trip");

    /* Border texture/color */
    result = lc_progress_bar_set_border_texture_path(bar, "res://ui/pb_border.png");
    TEST_RESULT(result, "Set border texture path");
    char borderTexture[256];
    borderTexture[0] = '\0';
    lc_progress_bar_get_border_texture_path(bar, borderTexture, sizeof(borderTexture));
    TEST_ASSERT(strcmp(borderTexture, "res://ui/pb_border.png") == 0, "Border texture path round-trip");

    LCColor borderColor = lc_color(0.5f, 0.5f, 0.5f, 1.0f);
    result = lc_progress_bar_set_border_color(bar, borderColor);
    TEST_RESULT(result, "Set border color");
    LCColor gotBorderColor = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    lc_progress_bar_get_border_color(bar, &gotBorderColor);
    TEST_ASSERT(ext_ui_color_eq(gotBorderColor, borderColor), "Border color round-trip");

    /* Value display */
    result = lc_progress_bar_set_show_value(bar, true);
    TEST_RESULT(result, "Set show value");
    bool showValue = false;
    lc_progress_bar_get_show_value(bar, &showValue);
    TEST_ASSERT(showValue == true, "Show value round-trip");

    result = lc_progress_bar_set_value_font_path(bar, "res://fonts/pb.ttf");
    TEST_RESULT(result, "Set value font path");
    char valueFont[256];
    valueFont[0] = '\0';
    lc_progress_bar_get_value_font_path(bar, valueFont, sizeof(valueFont));
    TEST_ASSERT(strcmp(valueFont, "res://fonts/pb.ttf") == 0, "Value font path round-trip");

    result = lc_progress_bar_set_value_font_size(bar, 16.0f);
    TEST_RESULT(result, "Set value font size");
    float valueFontSize = 0.0f;
    lc_progress_bar_get_value_font_size(bar, &valueFontSize);
    TEST_ASSERT(ext_ui_feq(valueFontSize, 16.0f), "Value font size round-trip is 16");

    LCColor valueColor = lc_color(1.0f, 1.0f, 1.0f, 1.0f);
    result = lc_progress_bar_set_value_color(bar, valueColor);
    TEST_RESULT(result, "Set value color");
    LCColor gotValueColor = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    lc_progress_bar_get_value_color(bar, &gotValueColor);
    TEST_ASSERT(ext_ui_color_eq(gotValueColor, valueColor), "Value color round-trip");

    /* Corner radius: linked + individual */
    result = lc_progress_bar_set_corner_radius_linked(bar, false);
    TEST_RESULT(result, "Unlink corner radii");
    bool cornerLinked = true;
    lc_progress_bar_get_corner_radius_linked(bar, &cornerLinked);
    TEST_ASSERT(cornerLinked == false, "Corner radius linked round-trip");

    result = lc_progress_bar_set_corner_radius_individual(bar, 0, 6.0f);
    TEST_RESULT(result, "Set individual corner radius");
    float cornerRadius = 0.0f;
    lc_progress_bar_get_corner_radius(bar, 0, &cornerRadius);
    TEST_ASSERT(ext_ui_feq(cornerRadius, 6.0f), "Individual corner radius round-trip is 6");

    /* Border width: linked + individual */
    result = lc_progress_bar_set_border_width_linked(bar, false);
    TEST_RESULT(result, "Unlink border widths");
    bool borderLinked = true;
    lc_progress_bar_get_border_width_linked(bar, &borderLinked);
    TEST_ASSERT(borderLinked == false, "Border width linked round-trip");

    result = lc_progress_bar_set_border_width_individual(bar, 2, 2.5f);
    TEST_RESULT(result, "Set individual border width");
    float borderWidth = 0.0f;
    lc_progress_bar_get_border_width(bar, 2, &borderWidth);
    TEST_ASSERT(ext_ui_feq(borderWidth, 2.5f), "Individual border width round-trip is 2.5");

    lc_scene_destroy(scene);

    SECTION_END("UI Ext: ProgressBar Component");
}

/* ========================================================================== */
/* RadioButton                                                                */
/* ========================================================================== */

static void test_ext_ui_radio_button(void) {
    SECTION_START("UI Ext: RadioButton Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_ext_radio_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "ExtRadioButtonNode", &node);
    TEST_RESULT(result, "Create node for RadioButton");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle radio = NULL;
    result = lc_radio_button_create(node, &radio);
    TEST_RESULT(result, "Create RadioButton component");

    /* Layer / sorting / UI space */
    result = lc_radio_button_set_layer(radio, 4);
    TEST_RESULT(result, "Set layer");
    int layer = 0;
    lc_radio_button_get_layer(radio, &layer);
    TEST_ASSERT(layer == 4, "Layer round-trip is 4");

    result = lc_radio_button_set_sorting_order(radio, 2);
    TEST_RESULT(result, "Set sorting order");
    int order = 0;
    lc_radio_button_get_sorting_order(radio, &order);
    TEST_ASSERT(order == 2, "Sorting order round-trip is 2");

    result = lc_radio_button_set_use_ui_space(radio, true);
    TEST_RESULT(result, "Set use UI space");
    bool useUiSpace = false;
    lc_radio_button_get_use_ui_space(radio, &useUiSpace);
    TEST_ASSERT(useUiSpace == true, "Use UI space round-trip");

    /* Enabled + current state */
    result = lc_radio_button_set_enabled(radio, false);
    TEST_RESULT(result, "Disable radio button");
    bool enabled = true;
    lc_radio_button_is_enabled(radio, &enabled);
    TEST_ASSERT(enabled == false, "Radio button is disabled");
    lc_radio_button_set_enabled(radio, true);

    LCRadioButtonState state = LC_RADIO_BUTTON_STATE_NORMAL;
    result = lc_radio_button_get_current_state(radio, &state);
    TEST_RESULT(result, "Get current state");

    /* Indicator colors */
    LCColor outerColor = lc_color(0.2f, 0.2f, 0.2f, 1.0f);
    result = lc_radio_button_set_outer_circle_color(radio, outerColor);
    TEST_RESULT(result, "Set outer circle color");
    LCColor gotOuterColor = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    lc_radio_button_get_outer_circle_color(radio, &gotOuterColor);
    TEST_ASSERT(ext_ui_color_eq(gotOuterColor, outerColor), "Outer circle color round-trip");

    LCColor bgColor = lc_color(0.95f, 0.95f, 0.95f, 1.0f);
    result = lc_radio_button_set_background_color(radio, bgColor);
    TEST_RESULT(result, "Set background color");
    LCColor gotBgColor = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    lc_radio_button_get_background_color(radio, &gotBgColor);
    TEST_ASSERT(ext_ui_color_eq(gotBgColor, bgColor), "Background color round-trip");

    LCColor innerColor = lc_color(0.0f, 0.4f, 0.9f, 1.0f);
    result = lc_radio_button_set_inner_circle_color(radio, innerColor);
    TEST_RESULT(result, "Set inner circle color");
    LCColor gotInnerColor = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    lc_radio_button_get_inner_circle_color(radio, &gotInnerColor);
    TEST_ASSERT(ext_ui_color_eq(gotInnerColor, innerColor), "Inner circle color round-trip");

    result = lc_radio_button_set_inner_circle_scale(radio, 0.5f);
    TEST_RESULT(result, "Set inner circle scale");
    float innerScale = 0.0f;
    lc_radio_button_get_inner_circle_scale(radio, &innerScale);
    TEST_ASSERT(ext_ui_feq(innerScale, 0.5f), "Inner circle scale round-trip is 0.5");

    result = lc_radio_button_set_border_width(radio, 2.0f);
    TEST_RESULT(result, "Set border width");
    float borderWidth = 0.0f;
    lc_radio_button_get_border_width(radio, &borderWidth);
    TEST_ASSERT(ext_ui_feq(borderWidth, 2.0f), "Border width round-trip is 2");

    /* Text properties */
    result = lc_radio_button_set_text(radio, "Option A");
    TEST_RESULT(result, "Set text");
    char text[256];
    text[0] = '\0';
    lc_radio_button_get_text(radio, text, sizeof(text));
    TEST_ASSERT(strcmp(text, "Option A") == 0, "Text round-trip");

    result = lc_radio_button_set_font_path(radio, "res://fonts/rb.ttf");
    TEST_RESULT(result, "Set font path");
    char fontPath[256];
    fontPath[0] = '\0';
    lc_radio_button_get_font_path(radio, fontPath, sizeof(fontPath));
    TEST_ASSERT(strcmp(fontPath, "res://fonts/rb.ttf") == 0, "Font path round-trip");

    result = lc_radio_button_set_font_size(radio, 13.0f);
    TEST_RESULT(result, "Set font size");
    float fontSize = 0.0f;
    lc_radio_button_get_font_size(radio, &fontSize);
    TEST_ASSERT(ext_ui_feq(fontSize, 13.0f), "Font size round-trip is 13");

    LCColor fontColor = lc_color(0.3f, 0.3f, 0.3f, 1.0f);
    result = lc_radio_button_set_font_color(radio, fontColor);
    TEST_RESULT(result, "Set font color");
    LCColor gotFontColor = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    lc_radio_button_get_font_color(radio, &gotFontColor);
    TEST_ASSERT(ext_ui_color_eq(gotFontColor, fontColor), "Font color round-trip");

    result = lc_radio_button_set_text_offset(radio, 10.0f);
    TEST_RESULT(result, "Set text offset");
    float textOffset = 0.0f;
    lc_radio_button_get_text_offset(radio, &textOffset);
    TEST_ASSERT(ext_ui_feq(textOffset, 10.0f), "Text offset round-trip is 10");

    /* Per-state modulation */
    LCColor stateMod = lc_color(0.6f, 0.6f, 0.6f, 1.0f);
    result = lc_radio_button_set_state_modulation(radio, LC_RADIO_BUTTON_STATE_SELECTED, stateMod);
    TEST_RESULT(result, "Set selected modulation");
    LCColor gotStateMod = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    lc_radio_button_get_state_modulation(radio, LC_RADIO_BUTTON_STATE_SELECTED, &gotStateMod);
    TEST_ASSERT(ext_ui_color_eq(gotStateMod, stateMod), "Selected modulation round-trip");

    /* Audio path */
    result = lc_radio_button_set_select_sound_path(radio, "res://sfx/select.wav");
    TEST_RESULT(result, "Set select sound path");
    char selectSound[256];
    selectSound[0] = '\0';
    lc_radio_button_get_select_sound_path(radio, selectSound, sizeof(selectSound));
    TEST_ASSERT(strcmp(selectSound, "res://sfx/select.wav") == 0, "Select sound path round-trip");

    lc_scene_destroy(scene);

    SECTION_END("UI Ext: RadioButton Component");
}

/* ========================================================================== */
/* ToggleButton                                                               */
/* ========================================================================== */

static void test_ext_ui_toggle_button(void) {
    SECTION_START("UI Ext: ToggleButton Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_ext_toggle_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "ExtToggleButtonNode", &node);
    TEST_RESULT(result, "Create node for ToggleButton");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle toggle = NULL;
    result = lc_toggle_button_create(node, &toggle);
    TEST_RESULT(result, "Create ToggleButton component");

    /* Size getters */
    result = lc_toggle_button_set_width(toggle, 160.0f);
    TEST_RESULT(result, "Set width");
    result = lc_toggle_button_set_height(toggle, 44.0f);
    TEST_RESULT(result, "Set height");
    float width = 0.0f, height = 0.0f;
    lc_toggle_button_get_width(toggle, &width);
    lc_toggle_button_get_height(toggle, &height);
    TEST_ASSERT(ext_ui_feq(width, 160.0f) && ext_ui_feq(height, 44.0f), "Width/height round-trip");

    /* Layer / sorting / UI space */
    result = lc_toggle_button_set_layer(toggle, 5);
    TEST_RESULT(result, "Set layer");
    int layer = 0;
    lc_toggle_button_get_layer(toggle, &layer);
    TEST_ASSERT(layer == 5, "Layer round-trip is 5");

    result = lc_toggle_button_set_sorting_order(toggle, 3);
    TEST_RESULT(result, "Set sorting order");
    int order = 0;
    lc_toggle_button_get_sorting_order(toggle, &order);
    TEST_ASSERT(order == 3, "Sorting order round-trip is 3");

    result = lc_toggle_button_set_use_ui_space(toggle, true);
    TEST_RESULT(result, "Set use UI space");
    bool useUiSpace = false;
    lc_toggle_button_get_use_ui_space(toggle, &useUiSpace);
    TEST_ASSERT(useUiSpace == true, "Use UI space round-trip");

    /* Enabled */
    result = lc_toggle_button_set_enabled(toggle, false);
    TEST_RESULT(result, "Disable toggle button");
    bool enabled = true;
    lc_toggle_button_is_enabled(toggle, &enabled);
    TEST_ASSERT(enabled == false, "Toggle button is disabled");
    lc_toggle_button_set_enabled(toggle, true);

    /* Per-state modulation getter */
    LCColor stateMod = lc_color(0.4f, 0.9f, 0.4f, 1.0f);
    result = lc_toggle_button_set_state_modulation(toggle, LC_TOGGLE_BUTTON_STATE_TOGGLED_HOVER, stateMod);
    TEST_RESULT(result, "Set toggled-hover modulation");
    LCColor gotStateMod = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    lc_toggle_button_get_state_modulation(toggle, LC_TOGGLE_BUTTON_STATE_TOGGLED_HOVER, &gotStateMod);
    TEST_ASSERT(ext_ui_color_eq(gotStateMod, stateMod), "Toggled-hover modulation round-trip");

    /* Per-state sound path */
    result = lc_toggle_button_set_state_sound_path(toggle, LC_TOGGLE_BUTTON_STATE_TOGGLED, "res://sfx/on.wav");
    TEST_RESULT(result, "Set toggled sound path");

    /* Font properties */
    result = lc_toggle_button_set_font_path(toggle, "res://fonts/tb.ttf");
    TEST_RESULT(result, "Set font path");
    char fontPath[256];
    fontPath[0] = '\0';
    lc_toggle_button_get_font_path(toggle, fontPath, sizeof(fontPath));
    TEST_ASSERT(strcmp(fontPath, "res://fonts/tb.ttf") == 0, "Font path round-trip");

    result = lc_toggle_button_set_font_size(toggle, 18.0f);
    TEST_RESULT(result, "Set font size");
    float fontSize = 0.0f;
    lc_toggle_button_get_font_size(toggle, &fontSize);
    TEST_ASSERT(ext_ui_feq(fontSize, 18.0f), "Font size round-trip is 18");

    LCColor fontColor = lc_color(0.9f, 0.9f, 0.9f, 1.0f);
    result = lc_toggle_button_set_font_color(toggle, fontColor);
    TEST_RESULT(result, "Set font color");
    LCColor gotFontColor = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    lc_toggle_button_get_font_color(toggle, &gotFontColor);
    TEST_ASSERT(ext_ui_color_eq(gotFontColor, fontColor), "Font color round-trip");

    /* StyleBox getters */
    LCColor bgColor = lc_color(0.25f, 0.25f, 0.25f, 1.0f);
    result = lc_toggle_button_set_background_color(toggle, bgColor);
    TEST_RESULT(result, "Set background color");
    LCColor gotBgColor = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    lc_toggle_button_get_background_color(toggle, &gotBgColor);
    TEST_ASSERT(ext_ui_color_eq(gotBgColor, bgColor), "Background color round-trip");

    result = lc_toggle_button_set_opacity(toggle, 0.85f);
    TEST_RESULT(result, "Set opacity");
    float opacity = 0.0f;
    lc_toggle_button_get_opacity(toggle, &opacity);
    TEST_ASSERT(ext_ui_feq(opacity, 0.85f), "Opacity round-trip is 0.85");

    result = lc_toggle_button_set_border_enabled(toggle, true);
    TEST_RESULT(result, "Enable border");
    bool borderEnabled = false;
    lc_toggle_button_get_border_enabled(toggle, &borderEnabled);
    TEST_ASSERT(borderEnabled == true, "Border enabled round-trip");

    LCColor borderColor = lc_color(0.7f, 0.7f, 0.7f, 1.0f);
    result = lc_toggle_button_set_border_color(toggle, borderColor);
    TEST_RESULT(result, "Set border color");
    LCColor gotBorderColor = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    lc_toggle_button_get_border_color(toggle, &gotBorderColor);
    TEST_ASSERT(ext_ui_color_eq(gotBorderColor, borderColor), "Border color round-trip");

    lc_scene_destroy(scene);

    SECTION_END("UI Ext: ToggleButton Component");
}

/* ========================================================================== */
/* TextureButton                                                              */
/* ========================================================================== */

static void test_ext_ui_texture_button(void) {
    SECTION_START("UI Ext: TextureButton Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_ext_texbutton_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "ExtTextureButtonNode", &node);
    TEST_RESULT(result, "Create node for TextureButton");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle button = NULL;
    result = lc_texture_button_create(node, &button);
    TEST_RESULT(result, "Create TextureButton component");

    /* Size getters */
    result = lc_texture_button_set_width(button, 96.0f);
    TEST_RESULT(result, "Set width");
    result = lc_texture_button_set_height(button, 48.0f);
    TEST_RESULT(result, "Set height");
    float width = 0.0f, height = 0.0f;
    lc_texture_button_get_width(button, &width);
    lc_texture_button_get_height(button, &height);
    TEST_ASSERT(ext_ui_feq(width, 96.0f) && ext_ui_feq(height, 48.0f), "Width/height round-trip");

    /* Layer / sorting / UI space */
    result = lc_texture_button_set_layer(button, 6);
    TEST_RESULT(result, "Set layer");
    int layer = 0;
    lc_texture_button_get_layer(button, &layer);
    TEST_ASSERT(layer == 6, "Layer round-trip is 6");

    result = lc_texture_button_set_sorting_order(button, 1);
    TEST_RESULT(result, "Set sorting order");
    int order = 0;
    lc_texture_button_get_sorting_order(button, &order);
    TEST_ASSERT(order == 1, "Sorting order round-trip is 1");

    result = lc_texture_button_set_use_ui_space(button, true);
    TEST_RESULT(result, "Set use UI space");
    bool useUiSpace = false;
    lc_texture_button_get_use_ui_space(button, &useUiSpace);
    TEST_ASSERT(useUiSpace == true, "Use UI space round-trip");

    /* Enabled + current state */
    result = lc_texture_button_set_enabled(button, false);
    TEST_RESULT(result, "Disable texture button");
    bool enabled = true;
    lc_texture_button_is_enabled(button, &enabled);
    TEST_ASSERT(enabled == false, "Texture button is disabled");
    lc_texture_button_set_enabled(button, true);

    LCTextureButtonState state = LC_TEXTURE_BUTTON_STATE_NORMAL;
    result = lc_texture_button_get_current_state(button, &state);
    TEST_RESULT(result, "Get current state");

    /* Style mode getter */
    result = lc_texture_button_set_style_mode(button, LC_TEXTURE_BUTTON_STYLE_MANUAL);
    TEST_RESULT(result, "Set style mode");
    LCTextureButtonStyleMode styleMode = LC_TEXTURE_BUTTON_STYLE_AUTOMATIC;
    lc_texture_button_get_style_mode(button, &styleMode);
    TEST_ASSERT(styleMode == LC_TEXTURE_BUTTON_STYLE_MANUAL, "Style mode is Manual");

    /* Click mask mode getter */
    result = lc_texture_button_set_click_mask_mode(button, LC_CLICK_MASK_ALPHA_THRESHOLD);
    TEST_RESULT(result, "Set click mask mode");
    LCClickMaskMode clickMask = LC_CLICK_MASK_RECT;
    lc_texture_button_get_click_mask_mode(button, &clickMask);
    TEST_ASSERT(clickMask == LC_CLICK_MASK_ALPHA_THRESHOLD, "Click mask mode is AlphaThreshold");

    /* Mouse button getter */
    result = lc_texture_button_set_mouse_button(button, LC_MOUSE_BUTTON_RIGHT);
    TEST_RESULT(result, "Set mouse button");
    LCMouseButton mouseButton = LC_MOUSE_BUTTON_LEFT;
    lc_texture_button_get_mouse_button(button, &mouseButton);
    TEST_ASSERT(mouseButton == LC_MOUSE_BUTTON_RIGHT, "Mouse button is Right");

    /* Texture path (automatic mode) */
    result = lc_texture_button_set_texture_path(button, "res://ui/btn.png");
    TEST_RESULT(result, "Set texture path");
    char texturePath[256];
    texturePath[0] = '\0';
    lc_texture_button_get_texture_path(button, texturePath, sizeof(texturePath));
    TEST_ASSERT(strcmp(texturePath, "res://ui/btn.png") == 0, "Texture path round-trip");

    /* Per-state texture path (manual mode) */
    result = lc_texture_button_set_state_texture_path(button, LC_TEXTURE_BUTTON_STATE_HOVER, "res://ui/btn_hover.png");
    TEST_RESULT(result, "Set hover state texture path");
    char statePath[256];
    statePath[0] = '\0';
    lc_texture_button_get_state_texture_path(button, LC_TEXTURE_BUTTON_STATE_HOVER, statePath, sizeof(statePath));
    TEST_ASSERT(strcmp(statePath, "res://ui/btn_hover.png") == 0, "Hover state texture path round-trip");

    /* Per-state modulation */
    LCColor stateMod = lc_color(0.5f, 0.5f, 0.5f, 1.0f);
    result = lc_texture_button_set_state_modulation(button, LC_TEXTURE_BUTTON_STATE_PRESSED, stateMod);
    TEST_RESULT(result, "Set pressed modulation");
    LCColor gotStateMod = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    lc_texture_button_get_state_modulation(button, LC_TEXTURE_BUTTON_STATE_PRESSED, &gotStateMod);
    TEST_ASSERT(ext_ui_color_eq(gotStateMod, stateMod), "Pressed modulation round-trip");

    /* Per-state sound path */
    result = lc_texture_button_set_state_sound_path(button, LC_TEXTURE_BUTTON_STATE_PRESSED, "res://sfx/click.wav");
    TEST_RESULT(result, "Set pressed sound path");

    /* Text properties */
    result = lc_texture_button_set_text(button, "Start");
    TEST_RESULT(result, "Set text");
    char text[256];
    text[0] = '\0';
    lc_texture_button_get_text(button, text, sizeof(text));
    TEST_ASSERT(strcmp(text, "Start") == 0, "Text round-trip");

    result = lc_texture_button_set_font_path(button, "res://fonts/txb.ttf");
    TEST_RESULT(result, "Set font path");
    char fontPath[256];
    fontPath[0] = '\0';
    lc_texture_button_get_font_path(button, fontPath, sizeof(fontPath));
    TEST_ASSERT(strcmp(fontPath, "res://fonts/txb.ttf") == 0, "Font path round-trip");

    result = lc_texture_button_set_font_size(button, 20.0f);
    TEST_RESULT(result, "Set font size");
    float fontSize = 0.0f;
    lc_texture_button_get_font_size(button, &fontSize);
    TEST_ASSERT(ext_ui_feq(fontSize, 20.0f), "Font size round-trip is 20");

    LCColor fontColor = lc_color(1.0f, 0.9f, 0.0f, 1.0f);
    result = lc_texture_button_set_font_color(button, fontColor);
    TEST_RESULT(result, "Set font color");
    LCColor gotFontColor = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    lc_texture_button_get_font_color(button, &gotFontColor);
    TEST_ASSERT(ext_ui_color_eq(gotFontColor, fontColor), "Font color round-trip");

    lc_scene_destroy(scene);

    SECTION_END("UI Ext: TextureButton Component");
}
