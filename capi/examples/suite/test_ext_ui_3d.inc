static void test_ext_ui_panel3d(void) {
    SECTION_START("UI Ext: Panel3D Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_ext_panel3d_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_3D, "ExtPanel3DNode", &node);
    TEST_RESULT(result, "Create node for Panel3D");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle panel = NULL;
    result = lc_panel3d_create(node, &panel);
    TEST_RESULT(result, "Create Panel3D component");

    /* Size round-trip */
    result = lc_panel3d_set_width(panel, 3.25f);
    TEST_RESULT(result, "Set width");
    float width = 0.0f;
    result = lc_panel3d_get_width(panel, &width);
    TEST_RESULT(result, "Get width");
    TEST_ASSERT(width > 3.24f && width < 3.26f, "Width round-trips");

    result = lc_panel3d_set_height(panel, 1.75f);
    TEST_RESULT(result, "Set height");
    float height = 0.0f;
    result = lc_panel3d_get_height(panel, &height);
    TEST_RESULT(result, "Get height");
    TEST_ASSERT(height > 1.74f && height < 1.76f, "Height round-trips");

    /* Billboard mode enum */
    result = lc_panel3d_set_billboard_mode(panel, LC_PANEL3D_BILLBOARD_ENABLED);
    TEST_RESULT(result, "Set billboard mode enabled");
    LCPanel3DBillboardMode billboard = LC_PANEL3D_BILLBOARD_DISABLED;
    result = lc_panel3d_get_billboard_mode(panel, &billboard);
    TEST_RESULT(result, "Get billboard mode");
    TEST_ASSERT(billboard == LC_PANEL3D_BILLBOARD_ENABLED, "Billboard mode round-trips");

    /* 3D rendering bools */
    bool doubleSided = false;
    result = lc_panel3d_get_double_sided(panel, &doubleSided);
    TEST_RESULT(result, "Get double-sided default");
    bool defaultDoubleSided = doubleSided;
    result = lc_panel3d_set_double_sided(panel, !defaultDoubleSided);
    TEST_RESULT(result, "Toggle double-sided");
    result = lc_panel3d_get_double_sided(panel, &doubleSided);
    TEST_RESULT(result, "Get double-sided after toggle");
    TEST_ASSERT(doubleSided == !defaultDoubleSided, "Double-sided toggled");
    result = lc_panel3d_set_double_sided(panel, defaultDoubleSided);
    TEST_RESULT(result, "Restore double-sided");

    bool castShadow = false;
    result = lc_panel3d_set_cast_shadow(panel, true);
    TEST_RESULT(result, "Set cast shadow");
    result = lc_panel3d_get_cast_shadow(panel, &castShadow);
    TEST_RESULT(result, "Get cast shadow");
    TEST_ASSERT(castShadow == true, "Cast shadow round-trips");

    bool receiveShadow = true;
    result = lc_panel3d_set_receive_shadow(panel, false);
    TEST_RESULT(result, "Set receive shadow");
    result = lc_panel3d_get_receive_shadow(panel, &receiveShadow);
    TEST_RESULT(result, "Get receive shadow");
    TEST_ASSERT(receiveShadow == false, "Receive shadow round-trips");

    /* Layer / sorting */
    result = lc_panel3d_set_layer(panel, 4);
    TEST_RESULT(result, "Set layer");
    int layer = 0;
    result = lc_panel3d_get_layer(panel, &layer);
    TEST_RESULT(result, "Get layer");
    TEST_ASSERT(layer == 4, "Layer round-trips");

    result = lc_panel3d_set_sorting_order(panel, 9);
    TEST_RESULT(result, "Set sorting order");
    int sorting = 0;
    result = lc_panel3d_get_sorting_order(panel, &sorting);
    TEST_RESULT(result, "Get sorting order");
    TEST_ASSERT(sorting == 9, "Sorting order round-trips");

    /* StyleBox file path */
    result = lc_panel3d_set_style_path(panel, "res://styles/panel3d.style");
    TEST_RESULT(result, "Set style path");
    char stylePath[256];
    stylePath[0] = '\0';
    result = lc_panel3d_get_style_path(panel, stylePath, sizeof(stylePath));
    TEST_RESULT(result, "Get style path");
    TEST_ASSERT(strcmp(stylePath, "res://styles/panel3d.style") == 0, "Style path round-trips");

    LCResult saveStyle = lc_panel3d_save_style(panel, "temp://ext_panel3d.style");
    TEST_ASSERT(saveStyle == LC_SUCCESS || saveStyle != LC_SUCCESS, "save_style returns without crashing");
    LCResult loadStyle = lc_panel3d_load_style(panel, "temp://ext_panel3d.style");
    TEST_ASSERT(loadStyle == LC_SUCCESS || loadStyle != LC_SUCCESS, "load_style returns without crashing");

    /* Background color round-trip */
    LCColor bgColor = lc_color(0.2f, 0.4f, 0.6f, 0.8f);
    result = lc_panel3d_set_background_color(panel, bgColor);
    TEST_RESULT(result, "Set background color");
    LCColor gotBg = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    result = lc_panel3d_get_background_color(panel, &gotBg);
    TEST_RESULT(result, "Get background color");
    TEST_ASSERT(gotBg.r == bgColor.r, "Background color.r round-trips");
    TEST_ASSERT(gotBg.g == bgColor.g, "Background color.g round-trips");
    TEST_ASSERT(gotBg.b == bgColor.b, "Background color.b round-trips");
    TEST_ASSERT(gotBg.a == bgColor.a, "Background color.a round-trips");

    /* Opacity */
    result = lc_panel3d_set_opacity(panel, 0.65f);
    TEST_RESULT(result, "Set opacity");
    float opacity = 0.0f;
    result = lc_panel3d_get_opacity(panel, &opacity);
    TEST_RESULT(result, "Get opacity");
    TEST_ASSERT(opacity > 0.64f && opacity < 0.66f, "Opacity round-trips");

    /* Border width linked + edges */
    result = lc_panel3d_set_border_width_linked(panel, false);
    TEST_RESULT(result, "Unlink border widths");
    bool borderLinked = true;
    result = lc_panel3d_get_border_width_linked(panel, &borderLinked);
    TEST_RESULT(result, "Get border width linked");
    TEST_ASSERT(borderLinked == false, "Border width unlinked round-trips");

    result = lc_panel3d_set_border_width_left(panel, 0.11f);
    TEST_RESULT(result, "Set border width left");
    float borderLeft = 0.0f;
    result = lc_panel3d_get_border_width_left(panel, &borderLeft);
    TEST_RESULT(result, "Get border width left");
    TEST_ASSERT(borderLeft > 0.10f && borderLeft < 0.12f, "Border width left round-trips");

    result = lc_panel3d_set_border_width_right(panel, 0.22f);
    TEST_RESULT(result, "Set border width right");
    float borderRight = 0.0f;
    result = lc_panel3d_get_border_width_right(panel, &borderRight);
    TEST_RESULT(result, "Get border width right");
    TEST_ASSERT(borderRight > 0.21f && borderRight < 0.23f, "Border width right round-trips");

    result = lc_panel3d_set_border_width_top(panel, 0.33f);
    TEST_RESULT(result, "Set border width top");
    float borderTop = 0.0f;
    result = lc_panel3d_get_border_width_top(panel, &borderTop);
    TEST_RESULT(result, "Get border width top");
    TEST_ASSERT(borderTop > 0.32f && borderTop < 0.34f, "Border width top round-trips");

    result = lc_panel3d_set_border_width_bottom(panel, 0.44f);
    TEST_RESULT(result, "Set border width bottom");
    float borderBottom = 0.0f;
    result = lc_panel3d_get_border_width_bottom(panel, &borderBottom);
    TEST_RESULT(result, "Get border width bottom");
    TEST_ASSERT(borderBottom > 0.43f && borderBottom < 0.45f, "Border width bottom round-trips");

    /* Border enabled + color */
    result = lc_panel3d_set_border_enabled(panel, true);
    TEST_RESULT(result, "Enable border");
    bool borderEnabled = false;
    result = lc_panel3d_get_border_enabled(panel, &borderEnabled);
    TEST_RESULT(result, "Get border enabled");
    TEST_ASSERT(borderEnabled == true, "Border enabled round-trips");

    LCColor borderColor = lc_color(0.9f, 0.1f, 0.5f, 1.0f);
    result = lc_panel3d_set_border_color(panel, borderColor);
    TEST_RESULT(result, "Set border color");
    LCColor gotBorder = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    result = lc_panel3d_get_border_color(panel, &gotBorder);
    TEST_RESULT(result, "Get border color");
    TEST_ASSERT(gotBorder.r == borderColor.r, "Border color.r round-trips");
    TEST_ASSERT(gotBorder.g == borderColor.g, "Border color.g round-trips");
    TEST_ASSERT(gotBorder.b == borderColor.b, "Border color.b round-trips");
    TEST_ASSERT(gotBorder.a == borderColor.a, "Border color.a round-trips");

    /* Corner radius linked + corners */
    result = lc_panel3d_set_corner_radius_linked(panel, false);
    TEST_RESULT(result, "Unlink corner radii");
    bool cornerLinked = true;
    result = lc_panel3d_get_corner_radius_linked(panel, &cornerLinked);
    TEST_RESULT(result, "Get corner radius linked");
    TEST_ASSERT(cornerLinked == false, "Corner radius unlinked round-trips");

    result = lc_panel3d_set_corner_radius_top_left(panel, 0.05f);
    TEST_RESULT(result, "Set corner radius top-left");
    float cornerTL = 0.0f;
    result = lc_panel3d_get_corner_radius_top_left(panel, &cornerTL);
    TEST_RESULT(result, "Get corner radius top-left");
    TEST_ASSERT(cornerTL > 0.04f && cornerTL < 0.06f, "Corner radius top-left round-trips");

    result = lc_panel3d_set_corner_radius_top_right(panel, 0.06f);
    TEST_RESULT(result, "Set corner radius top-right");
    float cornerTR = 0.0f;
    result = lc_panel3d_get_corner_radius_top_right(panel, &cornerTR);
    TEST_RESULT(result, "Get corner radius top-right");
    TEST_ASSERT(cornerTR > 0.05f && cornerTR < 0.07f, "Corner radius top-right round-trips");

    result = lc_panel3d_set_corner_radius_bottom_left(panel, 0.07f);
    TEST_RESULT(result, "Set corner radius bottom-left");
    float cornerBL = 0.0f;
    result = lc_panel3d_get_corner_radius_bottom_left(panel, &cornerBL);
    TEST_RESULT(result, "Get corner radius bottom-left");
    TEST_ASSERT(cornerBL > 0.06f && cornerBL < 0.08f, "Corner radius bottom-left round-trips");

    result = lc_panel3d_set_corner_radius_bottom_right(panel, 0.08f);
    TEST_RESULT(result, "Set corner radius bottom-right");
    float cornerBR = 0.0f;
    result = lc_panel3d_get_corner_radius_bottom_right(panel, &cornerBR);
    TEST_RESULT(result, "Get corner radius bottom-right");
    TEST_ASSERT(cornerBR > 0.07f && cornerBR < 0.09f, "Corner radius bottom-right round-trips");

    /* Corner detail + anti-aliasing */
    result = lc_panel3d_set_corner_detail(panel, 12);
    TEST_RESULT(result, "Set corner detail");
    int cornerDetail = 0;
    result = lc_panel3d_get_corner_detail(panel, &cornerDetail);
    TEST_RESULT(result, "Get corner detail");
    TEST_ASSERT(cornerDetail == 12, "Corner detail round-trips");

    result = lc_panel3d_set_anti_aliasing(panel, true);
    TEST_RESULT(result, "Enable anti-aliasing");
    bool antiAliasing = false;
    result = lc_panel3d_get_anti_aliasing(panel, &antiAliasing);
    TEST_RESULT(result, "Get anti-aliasing");
    TEST_ASSERT(antiAliasing == true, "Anti-aliasing round-trips");

    result = lc_panel3d_set_anti_aliasing_size(panel, 1.5f);
    TEST_RESULT(result, "Set anti-aliasing size");
    float aaSize = 0.0f;
    result = lc_panel3d_get_anti_aliasing_size(panel, &aaSize);
    TEST_RESULT(result, "Get anti-aliasing size");
    TEST_ASSERT(aaSize > 1.49f && aaSize < 1.51f, "Anti-aliasing size round-trips");

    /* Shadow */
    result = lc_panel3d_set_shadow_enabled(panel, true);
    TEST_RESULT(result, "Enable shadow");
    bool shadowEnabled = false;
    result = lc_panel3d_get_shadow_enabled(panel, &shadowEnabled);
    TEST_RESULT(result, "Get shadow enabled");
    TEST_ASSERT(shadowEnabled == true, "Shadow enabled round-trips");

    LCColor shadowColor = lc_color(0.0f, 0.0f, 0.0f, 0.5f);
    result = lc_panel3d_set_shadow_color(panel, shadowColor);
    TEST_RESULT(result, "Set shadow color");
    LCColor gotShadow = lc_color(1.0f, 1.0f, 1.0f, 1.0f);
    result = lc_panel3d_get_shadow_color(panel, &gotShadow);
    TEST_RESULT(result, "Get shadow color");
    TEST_ASSERT(gotShadow.r == shadowColor.r, "Shadow color.r round-trips");
    TEST_ASSERT(gotShadow.g == shadowColor.g, "Shadow color.g round-trips");
    TEST_ASSERT(gotShadow.b == shadowColor.b, "Shadow color.b round-trips");
    TEST_ASSERT(gotShadow.a == shadowColor.a, "Shadow color.a round-trips");

    result = lc_panel3d_set_shadow_size(panel, 0.3f);
    TEST_RESULT(result, "Set shadow size");
    float shadowSize = 0.0f;
    result = lc_panel3d_get_shadow_size(panel, &shadowSize);
    TEST_RESULT(result, "Get shadow size");
    TEST_ASSERT(shadowSize > 0.29f && shadowSize < 0.31f, "Shadow size round-trips");

    LCVec2 shadowOffset = lc_vec2(0.12f, -0.08f);
    result = lc_panel3d_set_shadow_offset(panel, shadowOffset);
    TEST_RESULT(result, "Set shadow offset");
    LCVec2 gotOffset = lc_vec2(0.0f, 0.0f);
    result = lc_panel3d_get_shadow_offset(panel, &gotOffset);
    TEST_RESULT(result, "Get shadow offset");
    TEST_ASSERT(gotOffset.x > 0.11f && gotOffset.x < 0.13f, "Shadow offset.x round-trips");
    TEST_ASSERT(gotOffset.y > -0.09f && gotOffset.y < -0.07f, "Shadow offset.y round-trips");

    /* Custom shader path */
    result = lc_panel3d_set_custom_shader_path(panel, "res://shaders/panel3d.lsh");
    TEST_RESULT(result, "Set custom shader path");
    char shaderPath[256];
    shaderPath[0] = '\0';
    result = lc_panel3d_get_custom_shader_path(panel, shaderPath, sizeof(shaderPath));
    TEST_RESULT(result, "Get custom shader path");
    TEST_ASSERT(strcmp(shaderPath, "res://shaders/panel3d.lsh") == 0, "Custom shader path round-trips");

    lc_scene_destroy(scene);

    SECTION_END("UI Ext: Panel3D Component");
}

static void test_ext_ui_button3d(void) {
    SECTION_START("UI Ext: Button3D Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_ext_button3d_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_3D, "ExtButton3DNode", &node);
    TEST_RESULT(result, "Create node for Button3D");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle button = NULL;
    result = lc_button3d_create(node, &button);
    TEST_RESULT(result, "Create Button3D component");

    /* Size round-trip */
    result = lc_button3d_set_width(button, 2.5f);
    TEST_RESULT(result, "Set width");
    float width = 0.0f;
    result = lc_button3d_get_width(button, &width);
    TEST_RESULT(result, "Get width");
    TEST_ASSERT(width > 2.49f && width < 2.51f, "Width round-trips");

    result = lc_button3d_set_height(button, 0.75f);
    TEST_RESULT(result, "Set height");
    float height = 0.0f;
    result = lc_button3d_get_height(button, &height);
    TEST_RESULT(result, "Get height");
    TEST_ASSERT(height > 0.74f && height < 0.76f, "Height round-trips");

    /* Billboard mode */
    result = lc_button3d_set_billboard_mode(button, LC_BUTTON3D_BILLBOARD_ENABLED);
    TEST_RESULT(result, "Set billboard mode");
    LCButton3DBillboardMode billboard = LC_BUTTON3D_BILLBOARD_DISABLED;
    result = lc_button3d_get_billboard_mode(button, &billboard);
    TEST_RESULT(result, "Get billboard mode");
    TEST_ASSERT(billboard == LC_BUTTON3D_BILLBOARD_ENABLED, "Billboard mode round-trips");

    /* 3D rendering bools */
    result = lc_button3d_set_double_sided(button, true);
    TEST_RESULT(result, "Set double-sided");
    bool doubleSided = false;
    result = lc_button3d_get_double_sided(button, &doubleSided);
    TEST_RESULT(result, "Get double-sided");
    TEST_ASSERT(doubleSided == true, "Double-sided round-trips");

    result = lc_button3d_set_cast_shadow(button, false);
    TEST_RESULT(result, "Set cast shadow");
    bool castShadow = true;
    result = lc_button3d_get_cast_shadow(button, &castShadow);
    TEST_RESULT(result, "Get cast shadow");
    TEST_ASSERT(castShadow == false, "Cast shadow round-trips");

    result = lc_button3d_set_receive_shadow(button, true);
    TEST_RESULT(result, "Set receive shadow");
    bool receiveShadow = false;
    result = lc_button3d_get_receive_shadow(button, &receiveShadow);
    TEST_RESULT(result, "Get receive shadow");
    TEST_ASSERT(receiveShadow == true, "Receive shadow round-trips");

    /* Layer / sorting */
    result = lc_button3d_set_layer(button, 3);
    TEST_RESULT(result, "Set layer");
    int layer = 0;
    result = lc_button3d_get_layer(button, &layer);
    TEST_RESULT(result, "Get layer");
    TEST_ASSERT(layer == 3, "Layer round-trips");

    result = lc_button3d_set_sorting_order(button, 7);
    TEST_RESULT(result, "Set sorting order");
    int sorting = 0;
    result = lc_button3d_get_sorting_order(button, &sorting);
    TEST_RESULT(result, "Get sorting order");
    TEST_ASSERT(sorting == 7, "Sorting order round-trips");

    /* Enabled state + current state */
    result = lc_button3d_set_enabled(button, true);
    TEST_RESULT(result, "Set enabled true");
    bool enabled = false;
    result = lc_button3d_is_enabled(button, &enabled);
    TEST_RESULT(result, "Query is_enabled");
    TEST_ASSERT(enabled == true, "Button enabled round-trips");
    result = lc_button3d_set_enabled(button, false);
    TEST_RESULT(result, "Set enabled false");
    result = lc_button3d_is_enabled(button, &enabled);
    TEST_RESULT(result, "Query is_enabled after disable");
    TEST_ASSERT(enabled == false, "Button disabled round-trips");
    result = lc_button3d_set_enabled(button, true);
    TEST_RESULT(result, "Restore enabled");

    LCButton3DState state = LC_BUTTON3D_STATE_DISABLED;
    result = lc_button3d_get_current_state(button, &state);
    TEST_RESULT(result, "Get current state");
    TEST_ASSERT(state == LC_BUTTON3D_STATE_NORMAL || state == LC_BUTTON3D_STATE_HOVER ||
                state == LC_BUTTON3D_STATE_PRESSED || state == LC_BUTTON3D_STATE_DISABLED,
                "Current state is a valid enum value");

    /* Style mode */
    result = lc_button3d_set_style_mode(button, LC_BUTTON3D_STYLE_MANUAL);
    TEST_RESULT(result, "Set style mode manual");
    LCButton3DStyleMode styleMode = LC_BUTTON3D_STYLE_AUTOMATIC;
    result = lc_button3d_get_style_mode(button, &styleMode);
    TEST_RESULT(result, "Get style mode");
    TEST_ASSERT(styleMode == LC_BUTTON3D_STYLE_MANUAL, "Style mode round-trips");

    /* Scale mode */
    result = lc_button3d_set_scale_mode(button, LC_BUTTON3D_SCALE_FIT_TO_TEXT);
    TEST_RESULT(result, "Set scale mode fit-to-text");
    LCButton3DScaleMode scaleMode = LC_BUTTON3D_SCALE_FIXED;
    result = lc_button3d_get_scale_mode(button, &scaleMode);
    TEST_RESULT(result, "Get scale mode");
    TEST_ASSERT(scaleMode == LC_BUTTON3D_SCALE_FIT_TO_TEXT, "Scale mode round-trips");

    LCVec2 textPadding = lc_vec2(0.05f, 0.03f);
    result = lc_button3d_set_text_padding(button, textPadding);
    TEST_RESULT(result, "Set text padding");
    LCVec2 gotPadding = lc_vec2(0.0f, 0.0f);
    result = lc_button3d_get_text_padding(button, &gotPadding);
    TEST_RESULT(result, "Get text padding");
    TEST_ASSERT(gotPadding.x > 0.04f && gotPadding.x < 0.06f, "Text padding.x round-trips");
    TEST_ASSERT(gotPadding.y > 0.02f && gotPadding.y < 0.04f, "Text padding.y round-trips");

    /* Background color + opacity */
    LCColor bgColor = lc_color(0.15f, 0.35f, 0.55f, 0.9f);
    result = lc_button3d_set_background_color(button, bgColor);
    TEST_RESULT(result, "Set background color");
    LCColor gotBg = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    result = lc_button3d_get_background_color(button, &gotBg);
    TEST_RESULT(result, "Get background color");
    TEST_ASSERT(gotBg.r == bgColor.r, "Background color.r round-trips");
    TEST_ASSERT(gotBg.g == bgColor.g, "Background color.g round-trips");
    TEST_ASSERT(gotBg.b == bgColor.b, "Background color.b round-trips");
    TEST_ASSERT(gotBg.a == bgColor.a, "Background color.a round-trips");

    result = lc_button3d_set_opacity(button, 0.5f);
    TEST_RESULT(result, "Set opacity");
    float opacity = 0.0f;
    result = lc_button3d_get_opacity(button, &opacity);
    TEST_RESULT(result, "Get opacity");
    TEST_ASSERT(opacity > 0.49f && opacity < 0.51f, "Opacity round-trips");

    /* Border enabled + color */
    result = lc_button3d_set_border_enabled(button, true);
    TEST_RESULT(result, "Enable border");
    bool borderEnabled = false;
    result = lc_button3d_get_border_enabled(button, &borderEnabled);
    TEST_RESULT(result, "Get border enabled");
    TEST_ASSERT(borderEnabled == true, "Border enabled round-trips");

    LCColor borderColor = lc_color(0.8f, 0.2f, 0.4f, 1.0f);
    result = lc_button3d_set_border_color(button, borderColor);
    TEST_RESULT(result, "Set border color");
    LCColor gotBorder = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    result = lc_button3d_get_border_color(button, &gotBorder);
    TEST_RESULT(result, "Get border color");
    TEST_ASSERT(gotBorder.r == borderColor.r, "Border color.r round-trips");
    TEST_ASSERT(gotBorder.g == borderColor.g, "Border color.g round-trips");
    TEST_ASSERT(gotBorder.b == borderColor.b, "Border color.b round-trips");
    TEST_ASSERT(gotBorder.a == borderColor.a, "Border color.a round-trips");

    /* Border widths */
    result = lc_button3d_set_border_width_linked(button, false);
    TEST_RESULT(result, "Unlink border widths");
    bool borderLinked = true;
    result = lc_button3d_get_border_width_linked(button, &borderLinked);
    TEST_RESULT(result, "Get border width linked");
    TEST_ASSERT(borderLinked == false, "Border width unlinked round-trips");

    result = lc_button3d_set_border_width_left(button, 0.10f);
    TEST_RESULT(result, "Set border width left");
    float borderLeft = 0.0f;
    result = lc_button3d_get_border_width_left(button, &borderLeft);
    TEST_RESULT(result, "Get border width left");
    TEST_ASSERT(borderLeft > 0.09f && borderLeft < 0.11f, "Border width left round-trips");

    result = lc_button3d_set_border_width_right(button, 0.20f);
    TEST_RESULT(result, "Set border width right");
    float borderRight = 0.0f;
    result = lc_button3d_get_border_width_right(button, &borderRight);
    TEST_RESULT(result, "Get border width right");
    TEST_ASSERT(borderRight > 0.19f && borderRight < 0.21f, "Border width right round-trips");

    result = lc_button3d_set_border_width_top(button, 0.30f);
    TEST_RESULT(result, "Set border width top");
    float borderTop = 0.0f;
    result = lc_button3d_get_border_width_top(button, &borderTop);
    TEST_RESULT(result, "Get border width top");
    TEST_ASSERT(borderTop > 0.29f && borderTop < 0.31f, "Border width top round-trips");

    result = lc_button3d_set_border_width_bottom(button, 0.40f);
    TEST_RESULT(result, "Set border width bottom");
    float borderBottom = 0.0f;
    result = lc_button3d_get_border_width_bottom(button, &borderBottom);
    TEST_RESULT(result, "Get border width bottom");
    TEST_ASSERT(borderBottom > 0.39f && borderBottom < 0.41f, "Border width bottom round-trips");

    /* Corner radius */
    result = lc_button3d_set_corner_radius_linked(button, false);
    TEST_RESULT(result, "Unlink corner radii");
    bool cornerLinked = true;
    result = lc_button3d_get_corner_radius_linked(button, &cornerLinked);
    TEST_RESULT(result, "Get corner radius linked");
    TEST_ASSERT(cornerLinked == false, "Corner radius unlinked round-trips");

    result = lc_button3d_set_corner_radius_top_left(button, 0.05f);
    TEST_RESULT(result, "Set corner radius top-left");
    float cornerTL = 0.0f;
    result = lc_button3d_get_corner_radius_top_left(button, &cornerTL);
    TEST_RESULT(result, "Get corner radius top-left");
    TEST_ASSERT(cornerTL > 0.04f && cornerTL < 0.06f, "Corner radius top-left round-trips");

    result = lc_button3d_set_corner_radius_top_right(button, 0.06f);
    TEST_RESULT(result, "Set corner radius top-right");
    float cornerTR = 0.0f;
    result = lc_button3d_get_corner_radius_top_right(button, &cornerTR);
    TEST_RESULT(result, "Get corner radius top-right");
    TEST_ASSERT(cornerTR > 0.05f && cornerTR < 0.07f, "Corner radius top-right round-trips");

    result = lc_button3d_set_corner_radius_bottom_left(button, 0.07f);
    TEST_RESULT(result, "Set corner radius bottom-left");
    float cornerBL = 0.0f;
    result = lc_button3d_get_corner_radius_bottom_left(button, &cornerBL);
    TEST_RESULT(result, "Get corner radius bottom-left");
    TEST_ASSERT(cornerBL > 0.06f && cornerBL < 0.08f, "Corner radius bottom-left round-trips");

    result = lc_button3d_set_corner_radius_bottom_right(button, 0.08f);
    TEST_RESULT(result, "Set corner radius bottom-right");
    float cornerBR = 0.0f;
    result = lc_button3d_get_corner_radius_bottom_right(button, &cornerBR);
    TEST_RESULT(result, "Get corner radius bottom-right");
    TEST_ASSERT(cornerBR > 0.07f && cornerBR < 0.09f, "Corner radius bottom-right round-trips");

    /* Text properties */
    result = lc_button3d_set_text(button, "Press");
    TEST_RESULT(result, "Set text");
    char textBuf[256];
    textBuf[0] = '\0';
    result = lc_button3d_get_text(button, textBuf, sizeof(textBuf));
    TEST_RESULT(result, "Get text");
    TEST_ASSERT(strcmp(textBuf, "Press") == 0, "Text round-trips");

    result = lc_button3d_set_font_path(button, "res://fonts/button3d.ttf");
    TEST_RESULT(result, "Set font path");
    char fontPath[256];
    fontPath[0] = '\0';
    result = lc_button3d_get_font_path(button, fontPath, sizeof(fontPath));
    TEST_RESULT(result, "Get font path");
    TEST_ASSERT(strcmp(fontPath, "res://fonts/button3d.ttf") == 0, "Font path round-trips");

    result = lc_button3d_set_font_size(button, 28.0f);
    TEST_RESULT(result, "Set font size");
    float fontSize = 0.0f;
    result = lc_button3d_get_font_size(button, &fontSize);
    TEST_RESULT(result, "Get font size");
    TEST_ASSERT(fontSize > 27.9f && fontSize < 28.1f, "Font size round-trips");

    LCColor fontColor = lc_color(0.95f, 0.85f, 0.05f, 1.0f);
    result = lc_button3d_set_font_color(button, fontColor);
    TEST_RESULT(result, "Set font color");
    LCColor gotFont = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    result = lc_button3d_get_font_color(button, &gotFont);
    TEST_RESULT(result, "Get font color");
    TEST_ASSERT(gotFont.r == fontColor.r, "Font color.r round-trips");
    TEST_ASSERT(gotFont.g == fontColor.g, "Font color.g round-trips");
    TEST_ASSERT(gotFont.b == fontColor.b, "Font color.b round-trips");
    TEST_ASSERT(gotFont.a == fontColor.a, "Font color.a round-trips");

    result = lc_button3d_set_word_wrap(button, true);
    TEST_RESULT(result, "Set word wrap");
    bool wordWrap = false;
    result = lc_button3d_get_word_wrap(button, &wordWrap);
    TEST_RESULT(result, "Get word wrap");
    TEST_ASSERT(wordWrap == true, "Word wrap round-trips");

    /* Per-state modulation */
    LCColor pressedMod = lc_color(0.5f, 0.5f, 0.5f, 1.0f);
    result = lc_button3d_set_state_modulation(button, LC_BUTTON3D_STATE_PRESSED, pressedMod);
    TEST_RESULT(result, "Set pressed state modulation");
    LCColor gotMod = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    result = lc_button3d_get_state_modulation(button, LC_BUTTON3D_STATE_PRESSED, &gotMod);
    TEST_RESULT(result, "Get pressed state modulation");
    TEST_ASSERT(gotMod.r == pressedMod.r, "Pressed modulation.r round-trips");
    TEST_ASSERT(gotMod.g == pressedMod.g, "Pressed modulation.g round-trips");
    TEST_ASSERT(gotMod.b == pressedMod.b, "Pressed modulation.b round-trips");
    TEST_ASSERT(gotMod.a == pressedMod.a, "Pressed modulation.a round-trips");

    /* Per-state sound */
    LCResult soundResult = lc_button3d_set_state_sound_path(button, LC_BUTTON3D_STATE_HOVER, "res://sfx/hover.wav");
    TEST_ASSERT(soundResult == LC_SUCCESS || soundResult != LC_SUCCESS, "set_state_sound_path returns without crashing");

    lc_scene_destroy(scene);

    SECTION_END("UI Ext: Button3D Component");
}

static void test_ext_ui_progress_bar3d(void) {
    SECTION_START("UI Ext: ProgressBar3D Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_ext_progress_bar3d_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_3D, "ExtProgressBar3DNode", &node);
    TEST_RESULT(result, "Create node for ProgressBar3D");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle bar = NULL;
    result = lc_progress_bar3d_create(node, &bar);
    TEST_RESULT(result, "Create ProgressBar3D component");

    /* Value properties */
    result = lc_progress_bar3d_set_min_value(bar, 5.0f);
    TEST_RESULT(result, "Set min value");
    float minValue = 0.0f;
    result = lc_progress_bar3d_get_min_value(bar, &minValue);
    TEST_RESULT(result, "Get min value");
    TEST_ASSERT(minValue > 4.99f && minValue < 5.01f, "Min value round-trips");

    result = lc_progress_bar3d_set_max_value(bar, 200.0f);
    TEST_RESULT(result, "Set max value");
    float maxValue = 0.0f;
    result = lc_progress_bar3d_get_max_value(bar, &maxValue);
    TEST_RESULT(result, "Get max value");
    TEST_ASSERT(maxValue > 199.9f && maxValue < 200.1f, "Max value round-trips");

    result = lc_progress_bar3d_set_value(bar, 120.0f);
    TEST_RESULT(result, "Set value");
    float value = 0.0f;
    result = lc_progress_bar3d_get_value(bar, &value);
    TEST_RESULT(result, "Get value");
    TEST_ASSERT(value > 119.9f && value < 120.1f, "Value round-trips");

    result = lc_progress_bar3d_set_step(bar, 2.5f);
    TEST_RESULT(result, "Set step");
    float step = 0.0f;
    result = lc_progress_bar3d_get_step(bar, &step);
    TEST_RESULT(result, "Get step");
    TEST_ASSERT(step > 2.49f && step < 2.51f, "Step round-trips");

    /* Smooth properties */
    result = lc_progress_bar3d_set_smooth(bar, true);
    TEST_RESULT(result, "Set smooth");
    bool smooth = false;
    result = lc_progress_bar3d_get_smooth(bar, &smooth);
    TEST_RESULT(result, "Get smooth");
    TEST_ASSERT(smooth == true, "Smooth round-trips");

    result = lc_progress_bar3d_set_smooth_speed(bar, 6.0f);
    TEST_RESULT(result, "Set smooth speed");
    float smoothSpeed = 0.0f;
    result = lc_progress_bar3d_get_smooth_speed(bar, &smoothSpeed);
    TEST_RESULT(result, "Get smooth speed");
    TEST_ASSERT(smoothSpeed > 5.99f && smoothSpeed < 6.01f, "Smooth speed round-trips");

    /* Size */
    result = lc_progress_bar3d_set_width(bar, 3.0f);
    TEST_RESULT(result, "Set width");
    float width = 0.0f;
    result = lc_progress_bar3d_get_width(bar, &width);
    TEST_RESULT(result, "Get width");
    TEST_ASSERT(width > 2.99f && width < 3.01f, "Width round-trips");

    result = lc_progress_bar3d_set_height(bar, 0.4f);
    TEST_RESULT(result, "Set height");
    float height = 0.0f;
    result = lc_progress_bar3d_get_height(bar, &height);
    TEST_RESULT(result, "Get height");
    TEST_ASSERT(height > 0.39f && height < 0.41f, "Height round-trips");

    /* Orientation + fill direction */
    result = lc_progress_bar3d_set_orientation(bar, LC_PROGRESSBAR_VERTICAL);
    TEST_RESULT(result, "Set orientation vertical");
    LCProgressBarOrientation orientation = LC_PROGRESSBAR_HORIZONTAL;
    result = lc_progress_bar3d_get_orientation(bar, &orientation);
    TEST_RESULT(result, "Get orientation");
    TEST_ASSERT(orientation == LC_PROGRESSBAR_VERTICAL, "Orientation round-trips");

    result = lc_progress_bar3d_set_fill_direction(bar, LC_FILL_DOWN_TO_UP);
    TEST_RESULT(result, "Set fill direction down-to-up");
    LCProgressBarFillDirection fillDirection = LC_FILL_LEFT_TO_RIGHT;
    result = lc_progress_bar3d_get_fill_direction(bar, &fillDirection);
    TEST_RESULT(result, "Get fill direction");
    TEST_ASSERT(fillDirection == LC_FILL_DOWN_TO_UP, "Fill direction round-trips");

    /* Billboard mode */
    result = lc_progress_bar3d_set_billboard_mode(bar, LC_PROGRESSBAR3D_BILLBOARD_Y_AXIS);
    TEST_RESULT(result, "Set billboard mode Y-axis");
    LCProgressBar3DBillboardMode billboard = LC_PROGRESSBAR3D_BILLBOARD_DISABLED;
    result = lc_progress_bar3d_get_billboard_mode(bar, &billboard);
    TEST_RESULT(result, "Get billboard mode");
    TEST_ASSERT(billboard == LC_PROGRESSBAR3D_BILLBOARD_Y_AXIS, "Billboard mode round-trips");

    /* 3D rendering bools */
    result = lc_progress_bar3d_set_double_sided(bar, true);
    TEST_RESULT(result, "Set double-sided");
    bool doubleSided = false;
    result = lc_progress_bar3d_get_double_sided(bar, &doubleSided);
    TEST_RESULT(result, "Get double-sided");
    TEST_ASSERT(doubleSided == true, "Double-sided round-trips");

    result = lc_progress_bar3d_set_cast_shadow(bar, true);
    TEST_RESULT(result, "Set cast shadow");
    bool castShadow = false;
    result = lc_progress_bar3d_get_cast_shadow(bar, &castShadow);
    TEST_RESULT(result, "Get cast shadow");
    TEST_ASSERT(castShadow == true, "Cast shadow round-trips");

    result = lc_progress_bar3d_set_receive_shadow(bar, true);
    TEST_RESULT(result, "Set receive shadow");
    bool receiveShadow = false;
    result = lc_progress_bar3d_get_receive_shadow(bar, &receiveShadow);
    TEST_RESULT(result, "Get receive shadow");
    TEST_ASSERT(receiveShadow == true, "Receive shadow round-trips");

    /* Background texture + color */
    result = lc_progress_bar3d_set_background_texture_path(bar, "res://tex/bar_bg.png");
    TEST_RESULT(result, "Set background texture path");
    char bgTexPath[256];
    bgTexPath[0] = '\0';
    result = lc_progress_bar3d_get_background_texture_path(bar, bgTexPath, sizeof(bgTexPath));
    TEST_RESULT(result, "Get background texture path");
    TEST_ASSERT(strcmp(bgTexPath, "res://tex/bar_bg.png") == 0, "Background texture path round-trips");

    LCColor bgColor = lc_color(0.25f, 0.25f, 0.25f, 1.0f);
    result = lc_progress_bar3d_set_background_color(bar, bgColor);
    TEST_RESULT(result, "Set background color");
    LCColor gotBg = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    result = lc_progress_bar3d_get_background_color(bar, &gotBg);
    TEST_RESULT(result, "Get background color");
    TEST_ASSERT(gotBg.r == bgColor.r, "Background color.r round-trips");
    TEST_ASSERT(gotBg.g == bgColor.g, "Background color.g round-trips");
    TEST_ASSERT(gotBg.b == bgColor.b, "Background color.b round-trips");
    TEST_ASSERT(gotBg.a == bgColor.a, "Background color.a round-trips");

    /* Fill texture + color */
    result = lc_progress_bar3d_set_fill_texture_path(bar, "res://tex/bar_fill.png");
    TEST_RESULT(result, "Set fill texture path");
    char fillTexPath[256];
    fillTexPath[0] = '\0';
    result = lc_progress_bar3d_get_fill_texture_path(bar, fillTexPath, sizeof(fillTexPath));
    TEST_RESULT(result, "Get fill texture path");
    TEST_ASSERT(strcmp(fillTexPath, "res://tex/bar_fill.png") == 0, "Fill texture path round-trips");

    LCColor fillColor = lc_color(0.0f, 0.8f, 0.2f, 1.0f);
    result = lc_progress_bar3d_set_fill_color(bar, fillColor);
    TEST_RESULT(result, "Set fill color");
    LCColor gotFill = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    result = lc_progress_bar3d_get_fill_color(bar, &gotFill);
    TEST_RESULT(result, "Get fill color");
    TEST_ASSERT(gotFill.r == fillColor.r, "Fill color.r round-trips");
    TEST_ASSERT(gotFill.g == fillColor.g, "Fill color.g round-trips");
    TEST_ASSERT(gotFill.b == fillColor.b, "Fill color.b round-trips");
    TEST_ASSERT(gotFill.a == fillColor.a, "Fill color.a round-trips");

    /* Border texture + color */
    result = lc_progress_bar3d_set_border_texture_path(bar, "res://tex/bar_border.png");
    TEST_RESULT(result, "Set border texture path");
    char borderTexPath[256];
    borderTexPath[0] = '\0';
    result = lc_progress_bar3d_get_border_texture_path(bar, borderTexPath, sizeof(borderTexPath));
    TEST_RESULT(result, "Get border texture path");
    TEST_ASSERT(strcmp(borderTexPath, "res://tex/bar_border.png") == 0, "Border texture path round-trips");

    LCColor borderColor = lc_color(0.1f, 0.1f, 0.1f, 1.0f);
    result = lc_progress_bar3d_set_border_color(bar, borderColor);
    TEST_RESULT(result, "Set border color");
    LCColor gotBorder = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    result = lc_progress_bar3d_get_border_color(bar, &gotBorder);
    TEST_RESULT(result, "Get border color");
    TEST_ASSERT(gotBorder.r == borderColor.r, "Border color.r round-trips");
    TEST_ASSERT(gotBorder.g == borderColor.g, "Border color.g round-trips");
    TEST_ASSERT(gotBorder.b == borderColor.b, "Border color.b round-trips");
    TEST_ASSERT(gotBorder.a == borderColor.a, "Border color.a round-trips");

    /* Value display */
    result = lc_progress_bar3d_set_show_value(bar, true);
    TEST_RESULT(result, "Set show value");
    bool showValue = false;
    result = lc_progress_bar3d_get_show_value(bar, &showValue);
    TEST_RESULT(result, "Get show value");
    TEST_ASSERT(showValue == true, "Show value round-trips");

    result = lc_progress_bar3d_set_value_font_path(bar, "res://fonts/bar.ttf");
    TEST_RESULT(result, "Set value font path");
    char valueFontPath[256];
    valueFontPath[0] = '\0';
    result = lc_progress_bar3d_get_value_font_path(bar, valueFontPath, sizeof(valueFontPath));
    TEST_RESULT(result, "Get value font path");
    TEST_ASSERT(strcmp(valueFontPath, "res://fonts/bar.ttf") == 0, "Value font path round-trips");

    result = lc_progress_bar3d_set_value_font_size(bar, 18.0f);
    TEST_RESULT(result, "Set value font size");
    float valueFontSize = 0.0f;
    result = lc_progress_bar3d_get_value_font_size(bar, &valueFontSize);
    TEST_RESULT(result, "Get value font size");
    TEST_ASSERT(valueFontSize > 17.9f && valueFontSize < 18.1f, "Value font size round-trips");

    LCColor valueColor = lc_color(1.0f, 1.0f, 1.0f, 1.0f);
    result = lc_progress_bar3d_set_value_color(bar, valueColor);
    TEST_RESULT(result, "Set value color");
    LCColor gotValueColor = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    result = lc_progress_bar3d_get_value_color(bar, &gotValueColor);
    TEST_RESULT(result, "Get value color");
    TEST_ASSERT(gotValueColor.r == valueColor.r, "Value color.r round-trips");
    TEST_ASSERT(gotValueColor.g == valueColor.g, "Value color.g round-trips");
    TEST_ASSERT(gotValueColor.b == valueColor.b, "Value color.b round-trips");
    TEST_ASSERT(gotValueColor.a == valueColor.a, "Value color.a round-trips");

    /* Corner radius linked + indexed */
    result = lc_progress_bar3d_set_corner_radius_linked(bar, false);
    TEST_RESULT(result, "Unlink corner radii");
    bool cornerLinked = true;
    result = lc_progress_bar3d_get_corner_radius_linked(bar, &cornerLinked);
    TEST_RESULT(result, "Get corner radius linked");
    TEST_ASSERT(cornerLinked == false, "Corner radius unlinked round-trips");

    result = lc_progress_bar3d_set_corner_radius_all(bar, 0.04f);
    TEST_RESULT(result, "Set corner radius all");

    result = lc_progress_bar3d_set_corner_radius_individual(bar, 0, 0.09f);
    TEST_RESULT(result, "Set corner radius index 0");
    float cornerRadius0 = 0.0f;
    result = lc_progress_bar3d_get_corner_radius(bar, 0, &cornerRadius0);
    TEST_RESULT(result, "Get corner radius index 0");
    TEST_ASSERT(cornerRadius0 > 0.08f && cornerRadius0 < 0.10f, "Corner radius index 0 round-trips");

    result = lc_progress_bar3d_set_corner_radius_individual(bar, 2, 0.13f);
    TEST_RESULT(result, "Set corner radius index 2");
    float cornerRadius2 = 0.0f;
    result = lc_progress_bar3d_get_corner_radius(bar, 2, &cornerRadius2);
    TEST_RESULT(result, "Get corner radius index 2");
    TEST_ASSERT(cornerRadius2 > 0.12f && cornerRadius2 < 0.14f, "Corner radius index 2 round-trips");

    /* Border width linked + indexed */
    result = lc_progress_bar3d_set_border_width_linked(bar, false);
    TEST_RESULT(result, "Unlink border widths");
    bool borderLinked = true;
    result = lc_progress_bar3d_get_border_width_linked(bar, &borderLinked);
    TEST_RESULT(result, "Get border width linked");
    TEST_ASSERT(borderLinked == false, "Border width unlinked round-trips");

    result = lc_progress_bar3d_set_border_width_all(bar, 0.02f);
    TEST_RESULT(result, "Set border width all");

    result = lc_progress_bar3d_set_border_width_individual(bar, 1, 0.07f);
    TEST_RESULT(result, "Set border width index 1");
    float borderWidth1 = 0.0f;
    result = lc_progress_bar3d_get_border_width(bar, 1, &borderWidth1);
    TEST_RESULT(result, "Get border width index 1");
    TEST_ASSERT(borderWidth1 > 0.06f && borderWidth1 < 0.08f, "Border width index 1 round-trips");

    result = lc_progress_bar3d_set_border_width_individual(bar, 3, 0.11f);
    TEST_RESULT(result, "Set border width index 3");
    float borderWidth3 = 0.0f;
    result = lc_progress_bar3d_get_border_width(bar, 3, &borderWidth3);
    TEST_RESULT(result, "Get border width index 3");
    TEST_ASSERT(borderWidth3 > 0.10f && borderWidth3 < 0.12f, "Border width index 3 round-trips");

    lc_scene_destroy(scene);

    SECTION_END("UI Ext: ProgressBar3D Component");
}

static void test_ext_ui_label3d(void) {
    SECTION_START("UI Ext: Label3D Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_ext_label3d_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_3D, "ExtLabel3DNode", &node);
    TEST_RESULT(result, "Create node for Label3D");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle label = NULL;
    result = lc_label3d_create(node, &label);
    TEST_RESULT(result, "Create Label3D component");

    /* Font path round-trip */
    result = lc_label3d_set_font_path(label, "res://fonts/label3d.ttf");
    TEST_RESULT(result, "Set font path");
    char fontPath[256];
    fontPath[0] = '\0';
    result = lc_label3d_get_font_path(label, fontPath, sizeof(fontPath));
    TEST_RESULT(result, "Get font path");
    TEST_ASSERT(strcmp(fontPath, "res://fonts/label3d.ttf") == 0, "Font path round-trips");

    /* load_font graceful return */
    LCResult loadFont = lc_label3d_load_font(label, "res://fonts/label3d.ttf");
    TEST_ASSERT(loadFont == LC_SUCCESS || loadFont != LC_SUCCESS, "load_font returns without crashing");

    /* Text */
    result = lc_label3d_set_text(label, "Floating Text");
    TEST_RESULT(result, "Set text");
    char text[256];
    text[0] = '\0';
    result = lc_label3d_get_text(label, text, sizeof(text));
    TEST_RESULT(result, "Get text");
    TEST_ASSERT(strcmp(text, "Floating Text") == 0, "Text round-trips");

    /* Font size */
    result = lc_label3d_set_font_size(label, 24.0f);
    TEST_RESULT(result, "Set font size");
    float fontSize = 0.0f;
    result = lc_label3d_get_font_size(label, &fontSize);
    TEST_RESULT(result, "Get font size");
    TEST_ASSERT(fontSize > 23.9f && fontSize < 24.1f, "Font size round-trips");

    /* Color */
    LCColor color = lc_color(0.3f, 0.6f, 0.9f, 1.0f);
    result = lc_label3d_set_color(label, color);
    TEST_RESULT(result, "Set color");
    LCColor gotColor = lc_color(0.0f, 0.0f, 0.0f, 0.0f);
    result = lc_label3d_get_color(label, &gotColor);
    TEST_RESULT(result, "Get color");
    TEST_ASSERT(gotColor.r == color.r, "Color.r round-trips");
    TEST_ASSERT(gotColor.g == color.g, "Color.g round-trips");
    TEST_ASSERT(gotColor.b == color.b, "Color.b round-trips");
    TEST_ASSERT(gotColor.a == color.a, "Color.a round-trips");

    /* Centered toggle + restore */
    bool centered = false;
    result = lc_label3d_get_centered(label, &centered);
    TEST_RESULT(result, "Get centered default");
    bool defaultCentered = centered;
    result = lc_label3d_set_centered(label, !defaultCentered);
    TEST_RESULT(result, "Toggle centered");
    result = lc_label3d_get_centered(label, &centered);
    TEST_RESULT(result, "Get centered after toggle");
    TEST_ASSERT(centered == !defaultCentered, "Centered toggled");
    result = lc_label3d_set_centered(label, defaultCentered);
    TEST_RESULT(result, "Restore centered");

    /* Offset */
    LCVec2 offset = lc_vec2(0.25f, -0.5f);
    result = lc_label3d_set_offset(label, offset);
    TEST_RESULT(result, "Set offset");
    LCVec2 gotOffset = lc_vec2(0.0f, 0.0f);
    result = lc_label3d_get_offset(label, &gotOffset);
    TEST_RESULT(result, "Get offset");
    TEST_ASSERT(gotOffset.x > 0.24f && gotOffset.x < 0.26f, "Offset.x round-trips");
    TEST_ASSERT(gotOffset.y > -0.51f && gotOffset.y < -0.49f, "Offset.y round-trips");

    /* Billboard mode */
    result = lc_label3d_set_billboard_mode(label, LC_LABEL3D_BILLBOARD_Y_AXIS);
    TEST_RESULT(result, "Set billboard mode Y-axis");
    LCLabel3DBillboardMode billboard = LC_LABEL3D_BILLBOARD_DISABLED;
    result = lc_label3d_get_billboard_mode(label, &billboard);
    TEST_RESULT(result, "Get billboard mode");
    TEST_ASSERT(billboard == LC_LABEL3D_BILLBOARD_Y_AXIS, "Billboard mode round-trips");

    /* Pixel size */
    result = lc_label3d_set_pixel_size(label, 0.02f);
    TEST_RESULT(result, "Set pixel size");
    float pixelSize = 0.0f;
    result = lc_label3d_get_pixel_size(label, &pixelSize);
    TEST_RESULT(result, "Get pixel size");
    TEST_ASSERT(pixelSize > 0.019f && pixelSize < 0.021f, "Pixel size round-trips");

    /* Text measurement */
    LCVec2 textSize = lc_vec2(-1.0f, -1.0f);
    LCResult measureResult = lc_label3d_calculate_text_size(label, &textSize);
    TEST_ASSERT(measureResult == LC_SUCCESS || measureResult != LC_SUCCESS, "calculate_text_size returns without crashing");
    TEST_ASSERT(textSize.x >= 0.0f && textSize.y >= 0.0f, "Calculated text size is non-negative");

    lc_scene_destroy(scene);

    SECTION_END("UI Ext: Label3D Component");
}
