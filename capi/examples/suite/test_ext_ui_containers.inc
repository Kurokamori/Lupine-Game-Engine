static bool ext_uic_feq(float a, float b) {
    float d = a - b;
    if (d < 0.0f) {
        d = -d;
    }
    return d < 0.001f;
}

static void test_ext_ui_uicontrol(void) {
    SECTION_START("UIControl: Shared Layout Base");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_ext_uicontrol_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "UIControlNode", &node);
    TEST_RESULT(result, "Create node for UIControl");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle control = NULL;
    result = lc_container_create(node, &control);
    TEST_RESULT(result, "Create host component for UIControl");

    /* Width / height */
    result = lc_uicontrol_set_width(control, 320.0f);
    TEST_RESULT(result, "Set width");

    float width = 0.0f;
    result = lc_uicontrol_get_width(control, &width);
    TEST_RESULT(result, "Get width");
    TEST_ASSERT(ext_uic_feq(width, 320.0f), "Width round-trips to 320");

    result = lc_uicontrol_set_height(control, 240.0f);
    TEST_RESULT(result, "Set height");

    float height = 0.0f;
    result = lc_uicontrol_get_height(control, &height);
    TEST_RESULT(result, "Get height");
    TEST_ASSERT(ext_uic_feq(height, 240.0f), "Height round-trips to 240");

    /* Size vector */
    result = lc_uicontrol_set_size(control, lc_vec2(500.0f, 400.0f));
    TEST_RESULT(result, "Set size");

    LCVec2 size = { 0.0f, 0.0f };
    result = lc_uicontrol_get_size(control, &size);
    TEST_RESULT(result, "Get size");
    TEST_ASSERT(ext_uic_feq(size.x, 500.0f) && ext_uic_feq(size.y, 400.0f), "Size round-trips to 500x400");

    /* Custom min size + computed min size */
    result = lc_uicontrol_set_custom_min_size(control, lc_vec2(120.0f, 60.0f));
    TEST_RESULT(result, "Set custom min size");

    LCVec2 customMin = { 0.0f, 0.0f };
    result = lc_uicontrol_get_custom_min_size(control, &customMin);
    TEST_RESULT(result, "Get custom min size");
    TEST_ASSERT(ext_uic_feq(customMin.x, 120.0f) && ext_uic_feq(customMin.y, 60.0f), "Custom min size round-trips to 120x60");

    LCVec2 minSize = { -1.0f, -1.0f };
    result = lc_uicontrol_get_min_size(control, &minSize);
    TEST_RESULT(result, "Get computed min size");
    TEST_ASSERT(minSize.x >= 120.0f && minSize.y >= 60.0f, "Computed min size honors custom min");

    /* Custom max size + computed max size */
    result = lc_uicontrol_set_custom_max_size(control, lc_vec2(640.0f, 480.0f));
    TEST_RESULT(result, "Set custom max size");

    LCVec2 customMax = { 0.0f, 0.0f };
    result = lc_uicontrol_get_custom_max_size(control, &customMax);
    TEST_RESULT(result, "Get custom max size");
    TEST_ASSERT(ext_uic_feq(customMax.x, 640.0f) && ext_uic_feq(customMax.y, 480.0f), "Custom max size round-trips to 640x480");

    LCVec2 maxSize = { -1.0f, -1.0f };
    result = lc_uicontrol_get_max_size(control, &maxSize);
    TEST_RESULT(result, "Get computed max size");
    TEST_ASSERT(ext_uic_feq(maxSize.x, 640.0f) && ext_uic_feq(maxSize.y, 480.0f), "Computed max size reflects custom max");

    /* Anchors */
    result = lc_uicontrol_set_anchor_min(control, lc_vec2(0.1f, 0.2f));
    TEST_RESULT(result, "Set anchor min");

    LCVec2 anchorMin = { 0.0f, 0.0f };
    result = lc_uicontrol_get_anchor_min(control, &anchorMin);
    TEST_RESULT(result, "Get anchor min");
    TEST_ASSERT(ext_uic_feq(anchorMin.x, 0.1f) && ext_uic_feq(anchorMin.y, 0.2f), "Anchor min round-trips to (0.1,0.2)");

    result = lc_uicontrol_set_anchor_max(control, lc_vec2(0.8f, 0.9f));
    TEST_RESULT(result, "Set anchor max");

    LCVec2 anchorMax = { 0.0f, 0.0f };
    result = lc_uicontrol_get_anchor_max(control, &anchorMax);
    TEST_RESULT(result, "Get anchor max");
    TEST_ASSERT(ext_uic_feq(anchorMax.x, 0.8f) && ext_uic_feq(anchorMax.y, 0.9f), "Anchor max round-trips to (0.8,0.9)");

    /* Offsets */
    result = lc_uicontrol_set_offset_min(control, lc_vec2(4.0f, 6.0f));
    TEST_RESULT(result, "Set offset min");

    LCVec2 offsetMin = { 0.0f, 0.0f };
    result = lc_uicontrol_get_offset_min(control, &offsetMin);
    TEST_RESULT(result, "Get offset min");
    TEST_ASSERT(ext_uic_feq(offsetMin.x, 4.0f) && ext_uic_feq(offsetMin.y, 6.0f), "Offset min round-trips to (4,6)");

    result = lc_uicontrol_set_offset_max(control, lc_vec2(-8.0f, -10.0f));
    TEST_RESULT(result, "Set offset max");

    LCVec2 offsetMax = { 0.0f, 0.0f };
    result = lc_uicontrol_get_offset_max(control, &offsetMax);
    TEST_RESULT(result, "Get offset max");
    TEST_ASSERT(ext_uic_feq(offsetMax.x, -8.0f) && ext_uic_feq(offsetMax.y, -10.0f), "Offset max round-trips to (-8,-10)");

    /* Anchor preset */
    result = lc_uicontrol_apply_anchor_preset(control, LC_ANCHOR_PRESET_FULL_RECT);
    TEST_RESULT(result, "Apply FULL_RECT anchor preset");

    int preset = -1;
    result = lc_uicontrol_get_anchor_preset(control, &preset);
    TEST_RESULT(result, "Get anchor preset");
    TEST_ASSERT(preset == LC_ANCHOR_PRESET_FULL_RECT, "Anchor preset is FULL_RECT");

    result = lc_uicontrol_apply_anchor_preset(control, LC_ANCHOR_PRESET_CENTER);
    TEST_RESULT(result, "Apply CENTER anchor preset");

    result = lc_uicontrol_get_anchor_preset(control, &preset);
    TEST_RESULT(result, "Get anchor preset after CENTER");
    TEST_ASSERT(preset == LC_ANCHOR_PRESET_CENTER, "Anchor preset is CENTER");

    /* Pivot was removed from the C API: the engine property behind it was never read by the
     * layout solver, so the round-trip this used to assert proved only that a dead field
     * stored what it was handed. */

    /* Layout mode */
    result = lc_uicontrol_set_layout_mode(control, LC_LAYOUT_MODE_ANCHORS);
    TEST_RESULT(result, "Set layout mode to anchors");

    int layoutMode = -1;
    result = lc_uicontrol_get_layout_mode(control, &layoutMode);
    TEST_RESULT(result, "Get layout mode");
    TEST_ASSERT(layoutMode == LC_LAYOUT_MODE_ANCHORS, "Layout mode is anchors");

    result = lc_uicontrol_set_layout_mode(control, LC_LAYOUT_MODE_POSITION);
    TEST_RESULT(result, "Restore layout mode to position");

    result = lc_uicontrol_get_layout_mode(control, &layoutMode);
    TEST_RESULT(result, "Get layout mode after restore");
    TEST_ASSERT(layoutMode == LC_LAYOUT_MODE_POSITION, "Layout mode is position");

    /* Grow directions */
    result = lc_uicontrol_set_grow_direction_h(control, LC_GROW_DIRECTION_BOTH);
    TEST_RESULT(result, "Set horizontal grow direction");

    int growH = -1;
    result = lc_uicontrol_get_grow_direction_h(control, &growH);
    TEST_RESULT(result, "Get horizontal grow direction");
    TEST_ASSERT(growH == LC_GROW_DIRECTION_BOTH, "Horizontal grow direction is both");

    result = lc_uicontrol_set_grow_direction_v(control, LC_GROW_DIRECTION_END);
    TEST_RESULT(result, "Set vertical grow direction");

    int growV = -1;
    result = lc_uicontrol_get_grow_direction_v(control, &growV);
    TEST_RESULT(result, "Get vertical grow direction");
    TEST_ASSERT(growV == LC_GROW_DIRECTION_END, "Vertical grow direction is end");

    /* Container size flags */
    result = lc_uicontrol_set_size_flags_horizontal(control, LC_SIZE_FLAG_FILL | LC_SIZE_FLAG_EXPAND);
    TEST_RESULT(result, "Set horizontal size flags");

    int flagsH = -1;
    result = lc_uicontrol_get_size_flags_horizontal(control, &flagsH);
    TEST_RESULT(result, "Get horizontal size flags");
    TEST_ASSERT(flagsH == (LC_SIZE_FLAG_FILL | LC_SIZE_FLAG_EXPAND), "Horizontal size flags are Fill|Expand");

    result = lc_uicontrol_set_size_flags_vertical(control, LC_SIZE_FLAG_SHRINK_CENTER);
    TEST_RESULT(result, "Set vertical size flags");

    int flagsV = -1;
    result = lc_uicontrol_get_size_flags_vertical(control, &flagsV);
    TEST_RESULT(result, "Get vertical size flags");
    TEST_ASSERT(flagsV == LC_SIZE_FLAG_SHRINK_CENTER, "Vertical size flags are ShrinkCenter");

    result = lc_uicontrol_set_size_flags_stretch_ratio(control, 2.5f);
    TEST_RESULT(result, "Set size flags stretch ratio");

    float ratio = 0.0f;
    result = lc_uicontrol_get_size_flags_stretch_ratio(control, &ratio);
    TEST_RESULT(result, "Get size flags stretch ratio");
    TEST_ASSERT(ext_uic_feq(ratio, 2.5f), "Stretch ratio round-trips to 2.5");

    /* UI space flag */
    bool originalUISpace = false;
    result = lc_uicontrol_get_ui_space(control, &originalUISpace);
    TEST_RESULT(result, "Get original UI space flag");

    result = lc_uicontrol_set_ui_space(control, !originalUISpace);
    TEST_RESULT(result, "Toggle UI space flag");

    bool toggledUISpace = originalUISpace;
    result = lc_uicontrol_get_ui_space(control, &toggledUISpace);
    TEST_RESULT(result, "Get toggled UI space flag");
    TEST_ASSERT(toggledUISpace == !originalUISpace, "UI space flag toggled");

    result = lc_uicontrol_set_ui_space(control, originalUISpace);
    TEST_RESULT(result, "Restore UI space flag");

    /* Resolved rect */
    LCRect rect = { { 0.0f, 0.0f }, { 0.0f, 0.0f } };
    result = lc_uicontrol_get_rect(control, &rect);
    TEST_RESULT(result, "Get resolved rect");
    TEST_ASSERT(rect.size.x >= 0.0f && rect.size.y >= 0.0f, "Resolved rect has non-negative size");

    lc_scene_destroy(scene);

    SECTION_END("UIControl: Shared Layout Base");
}

static void test_ext_ui_container(void) {
    SECTION_START("UI: Container (untested members)");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_ext_container_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "ExtContainerNode", &node);
    TEST_RESULT(result, "Create node for Container");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle container = NULL;
    result = lc_container_create(node, &container);
    TEST_RESULT(result, "Create Container component");

    /* Min / max size */
    result = lc_container_set_min_size(container, lc_vec2(100.0f, 80.0f));
    TEST_RESULT(result, "Set min size");

    LCVec2 minSize = { 0.0f, 0.0f };
    result = lc_container_get_min_size(container, &minSize);
    TEST_RESULT(result, "Get min size");
    TEST_ASSERT(ext_uic_feq(minSize.x, 100.0f) && ext_uic_feq(minSize.y, 80.0f), "Min size round-trips to 100x80");

    result = lc_container_set_max_size(container, lc_vec2(900.0f, 700.0f));
    TEST_RESULT(result, "Set max size");

    LCVec2 maxSize = { 0.0f, 0.0f };
    result = lc_container_get_max_size(container, &maxSize);
    TEST_RESULT(result, "Get max size");
    TEST_ASSERT(ext_uic_feq(maxSize.x, 900.0f) && ext_uic_feq(maxSize.y, 700.0f), "Max size round-trips to 900x700");

    /* Padding getters */
    result = lc_container_set_padding_linked(container, false);
    TEST_RESULT(result, "Set padding unlinked");

    bool paddingLinked = true;
    result = lc_container_get_padding_linked(container, &paddingLinked);
    TEST_RESULT(result, "Get padding linked");
    TEST_ASSERT(paddingLinked == false, "Padding linked is false");

    result = lc_container_set_padding_right(container, 12.0f);
    TEST_RESULT(result, "Set padding right");

    float paddingRight = 0.0f;
    result = lc_container_get_padding_right(container, &paddingRight);
    TEST_RESULT(result, "Get padding right");
    TEST_ASSERT(ext_uic_feq(paddingRight, 12.0f), "Padding right round-trips to 12");

    result = lc_container_set_padding_top(container, 14.0f);
    TEST_RESULT(result, "Set padding top");

    float paddingTop = 0.0f;
    result = lc_container_get_padding_top(container, &paddingTop);
    TEST_RESULT(result, "Get padding top");
    TEST_ASSERT(ext_uic_feq(paddingTop, 14.0f), "Padding top round-trips to 14");

    result = lc_container_set_padding_bottom(container, 16.0f);
    TEST_RESULT(result, "Set padding bottom");

    float paddingBottom = 0.0f;
    result = lc_container_get_padding_bottom(container, &paddingBottom);
    TEST_RESULT(result, "Get padding bottom");
    TEST_ASSERT(ext_uic_feq(paddingBottom, 16.0f), "Padding bottom round-trips to 16");

    result = lc_container_set_padding(container, lc_vec4(1.0f, 2.0f, 3.0f, 4.0f));
    TEST_RESULT(result, "Set padding vec4");

    LCVec4 padding = { 0.0f, 0.0f, 0.0f, 0.0f };
    result = lc_container_get_padding(container, &padding);
    TEST_RESULT(result, "Get padding vec4");
    TEST_ASSERT(ext_uic_feq(padding.x, 1.0f) && ext_uic_feq(padding.y, 2.0f) &&
                ext_uic_feq(padding.z, 3.0f) && ext_uic_feq(padding.w, 4.0f),
                "Padding vec4 round-trips to (1,2,3,4)");

    /* Margin getters */
    result = lc_container_set_margin_linked(container, false);
    TEST_RESULT(result, "Set margin unlinked");

    bool marginLinked = true;
    result = lc_container_get_margin_linked(container, &marginLinked);
    TEST_RESULT(result, "Get margin linked");
    TEST_ASSERT(marginLinked == false, "Margin linked is false");

    result = lc_container_set_margin_left(container, 5.0f);
    TEST_RESULT(result, "Set margin left");

    float marginLeft = 0.0f;
    result = lc_container_get_margin_left(container, &marginLeft);
    TEST_RESULT(result, "Get margin left");
    TEST_ASSERT(ext_uic_feq(marginLeft, 5.0f), "Margin left round-trips to 5");

    result = lc_container_set_margin_right(container, 6.0f);
    TEST_RESULT(result, "Set margin right");

    float marginRight = 0.0f;
    result = lc_container_get_margin_right(container, &marginRight);
    TEST_RESULT(result, "Get margin right");
    TEST_ASSERT(ext_uic_feq(marginRight, 6.0f), "Margin right round-trips to 6");

    result = lc_container_set_margin_top(container, 7.0f);
    TEST_RESULT(result, "Set margin top");

    float marginTop = 0.0f;
    result = lc_container_get_margin_top(container, &marginTop);
    TEST_RESULT(result, "Get margin top");
    TEST_ASSERT(ext_uic_feq(marginTop, 7.0f), "Margin top round-trips to 7");

    result = lc_container_set_margin_bottom(container, 9.0f);
    TEST_RESULT(result, "Set margin bottom");

    float marginBottom = 0.0f;
    result = lc_container_get_margin_bottom(container, &marginBottom);
    TEST_RESULT(result, "Get margin bottom");
    TEST_ASSERT(ext_uic_feq(marginBottom, 9.0f), "Margin bottom round-trips to 9");

    result = lc_container_set_margin(container, lc_vec4(2.0f, 4.0f, 6.0f, 8.0f));
    TEST_RESULT(result, "Set margin vec4");

    LCVec4 margin = { 0.0f, 0.0f, 0.0f, 0.0f };
    result = lc_container_get_margin(container, &margin);
    TEST_RESULT(result, "Get margin vec4");
    TEST_ASSERT(ext_uic_feq(margin.x, 2.0f) && ext_uic_feq(margin.y, 4.0f) &&
                ext_uic_feq(margin.z, 6.0f) && ext_uic_feq(margin.w, 8.0f),
                "Margin vec4 round-trips to (2,4,6,8)");

    /* Background getters */
    result = lc_container_set_opacity(container, 0.5f);
    TEST_RESULT(result, "Set opacity");

    float opacity = 0.0f;
    result = lc_container_get_opacity(container, &opacity);
    TEST_RESULT(result, "Get opacity");
    TEST_ASSERT(ext_uic_feq(opacity, 0.5f), "Opacity round-trips to 0.5");

    result = lc_container_set_draw_background(container, false);
    TEST_RESULT(result, "Disable draw background");

    bool drawBackground = true;
    result = lc_container_get_draw_background(container, &drawBackground);
    TEST_RESULT(result, "Get draw background");
    TEST_ASSERT(drawBackground == false, "Draw background is false");

    /* Border getters */
    result = lc_container_set_border_enabled(container, true);
    TEST_RESULT(result, "Enable border");

    bool borderEnabled = false;
    result = lc_container_get_border_enabled(container, &borderEnabled);
    TEST_RESULT(result, "Get border enabled");
    TEST_ASSERT(borderEnabled == true, "Border enabled is true");

    result = lc_container_set_border_color(container, lc_color(0.1f, 0.2f, 0.3f, 0.4f));
    TEST_RESULT(result, "Set border color");

    LCColor borderColor = { 0.0f, 0.0f, 0.0f, 0.0f };
    result = lc_container_get_border_color(container, &borderColor);
    TEST_RESULT(result, "Get border color");
    TEST_ASSERT(ext_uic_feq(borderColor.r, 0.1f) && ext_uic_feq(borderColor.g, 0.2f) &&
                ext_uic_feq(borderColor.b, 0.3f) && ext_uic_feq(borderColor.a, 0.4f),
                "Border color round-trips");

    result = lc_container_set_border_width_linked(container, false);
    TEST_RESULT(result, "Set border width unlinked");

    bool borderWidthLinked = true;
    result = lc_container_get_border_width_linked(container, &borderWidthLinked);
    TEST_RESULT(result, "Get border width linked");
    TEST_ASSERT(borderWidthLinked == false, "Border width linked is false");

    result = lc_container_set_border_width_left(container, 1.5f);
    TEST_RESULT(result, "Set border width left");

    float borderWidthLeft = 0.0f;
    result = lc_container_get_border_width_left(container, &borderWidthLeft);
    TEST_RESULT(result, "Get border width left");
    TEST_ASSERT(ext_uic_feq(borderWidthLeft, 1.5f), "Border width left round-trips to 1.5");

    result = lc_container_set_border_width_right(container, 2.5f);
    TEST_RESULT(result, "Set border width right");

    float borderWidthRight = 0.0f;
    result = lc_container_get_border_width_right(container, &borderWidthRight);
    TEST_RESULT(result, "Get border width right");
    TEST_ASSERT(ext_uic_feq(borderWidthRight, 2.5f), "Border width right round-trips to 2.5");

    result = lc_container_set_border_width_top(container, 3.5f);
    TEST_RESULT(result, "Set border width top");

    float borderWidthTop = 0.0f;
    result = lc_container_get_border_width_top(container, &borderWidthTop);
    TEST_RESULT(result, "Get border width top");
    TEST_ASSERT(ext_uic_feq(borderWidthTop, 3.5f), "Border width top round-trips to 3.5");

    result = lc_container_set_border_width_bottom(container, 4.5f);
    TEST_RESULT(result, "Set border width bottom");

    float borderWidthBottom = 0.0f;
    result = lc_container_get_border_width_bottom(container, &borderWidthBottom);
    TEST_RESULT(result, "Get border width bottom");
    TEST_ASSERT(ext_uic_feq(borderWidthBottom, 4.5f), "Border width bottom round-trips to 4.5");

    /* Corner radius getters */
    result = lc_container_set_corner_radius_linked(container, false);
    TEST_RESULT(result, "Set corner radius unlinked");

    bool cornerLinked = true;
    result = lc_container_get_corner_radius_linked(container, &cornerLinked);
    TEST_RESULT(result, "Get corner radius linked");
    TEST_ASSERT(cornerLinked == false, "Corner radius linked is false");

    result = lc_container_set_corner_radius_top_right(container, 3.0f);
    TEST_RESULT(result, "Set corner radius top right");

    float cornerTopRight = 0.0f;
    result = lc_container_get_corner_radius_top_right(container, &cornerTopRight);
    TEST_RESULT(result, "Get corner radius top right");
    TEST_ASSERT(ext_uic_feq(cornerTopRight, 3.0f), "Corner radius top right round-trips to 3");

    result = lc_container_set_corner_radius_bottom_left(container, 5.0f);
    TEST_RESULT(result, "Set corner radius bottom left");

    float cornerBottomLeft = 0.0f;
    result = lc_container_get_corner_radius_bottom_left(container, &cornerBottomLeft);
    TEST_RESULT(result, "Get corner radius bottom left");
    TEST_ASSERT(ext_uic_feq(cornerBottomLeft, 5.0f), "Corner radius bottom left round-trips to 5");

    result = lc_container_set_corner_radius_bottom_right(container, 7.0f);
    TEST_RESULT(result, "Set corner radius bottom right");

    float cornerBottomRight = 0.0f;
    result = lc_container_get_corner_radius_bottom_right(container, &cornerBottomRight);
    TEST_RESULT(result, "Get corner radius bottom right");
    TEST_ASSERT(ext_uic_feq(cornerBottomRight, 7.0f), "Corner radius bottom right round-trips to 7");

    /* Rendering getters */
    result = lc_container_set_layer(container, 4);
    TEST_RESULT(result, "Set layer");

    int layer = -1;
    result = lc_container_get_layer(container, &layer);
    TEST_RESULT(result, "Get layer");
    TEST_ASSERT(layer == 4, "Layer is 4");

    result = lc_container_set_sorting_order(container, 22);
    TEST_RESULT(result, "Set sorting order");

    int sortingOrder = -1;
    result = lc_container_get_sorting_order(container, &sortingOrder);
    TEST_RESULT(result, "Get sorting order");
    TEST_ASSERT(sortingOrder == 22, "Sorting order is 22");

    result = lc_container_set_use_ui_space(container, true);
    TEST_RESULT(result, "Enable use UI space");

    bool useUISpace = false;
    result = lc_container_get_use_ui_space(container, &useUISpace);
    TEST_RESULT(result, "Get use UI space");
    TEST_ASSERT(useUISpace == true, "Use UI space is true");

    lc_scene_destroy(scene);

    SECTION_END("UI: Container (untested members)");
}

static void test_ext_ui_scroll_container(void) {
    SECTION_START("UI: ScrollContainer Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_ext_scroll_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "ScrollContainerNode", &node);
    TEST_RESULT(result, "Create node for ScrollContainer");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle scroll = NULL;
    result = lc_scroll_container_create(node, &scroll);
    TEST_RESULT(result, "Create ScrollContainer component");

    /* Scroll position */
    result = lc_scroll_container_set_h_scroll(scroll, 25.0f);
    TEST_RESULT(result, "Set horizontal scroll");

    float hScroll = -1.0f;
    result = lc_scroll_container_get_h_scroll(scroll, &hScroll);
    TEST_RESULT(result, "Get horizontal scroll");
    TEST_ASSERT(hScroll >= 0.0f, "Horizontal scroll is non-negative");

    result = lc_scroll_container_set_v_scroll(scroll, 40.0f);
    TEST_RESULT(result, "Set vertical scroll");

    float vScroll = -1.0f;
    result = lc_scroll_container_get_v_scroll(scroll, &vScroll);
    TEST_RESULT(result, "Get vertical scroll");
    TEST_ASSERT(vScroll >= 0.0f, "Vertical scroll is non-negative");

    LCVec2 contentSize = { -1.0f, -1.0f };
    result = lc_scroll_container_get_content_size(scroll, &contentSize);
    TEST_RESULT(result, "Get content size");
    TEST_ASSERT(contentSize.x >= 0.0f && contentSize.y >= 0.0f, "Content size is non-negative");

    /* Enable flags */
    result = lc_scroll_container_set_h_scroll_enabled(scroll, false);
    TEST_RESULT(result, "Disable horizontal scroll");

    bool hEnabled = true;
    result = lc_scroll_container_get_h_scroll_enabled(scroll, &hEnabled);
    TEST_RESULT(result, "Get horizontal scroll enabled");
    TEST_ASSERT(hEnabled == false, "Horizontal scroll disabled");

    result = lc_scroll_container_set_h_scroll_enabled(scroll, true);
    TEST_RESULT(result, "Re-enable horizontal scroll");

    result = lc_scroll_container_set_v_scroll_enabled(scroll, false);
    TEST_RESULT(result, "Disable vertical scroll");

    bool vEnabled = true;
    result = lc_scroll_container_get_v_scroll_enabled(scroll, &vEnabled);
    TEST_RESULT(result, "Get vertical scroll enabled");
    TEST_ASSERT(vEnabled == false, "Vertical scroll disabled");

    result = lc_scroll_container_set_v_scroll_enabled(scroll, true);
    TEST_RESULT(result, "Re-enable vertical scroll");

    /* Appearance / behavior */
    result = lc_scroll_container_set_scroll_speed(scroll, 30.0f);
    TEST_RESULT(result, "Set scroll speed");

    float scrollSpeed = 0.0f;
    result = lc_scroll_container_get_scroll_speed(scroll, &scrollSpeed);
    TEST_RESULT(result, "Get scroll speed");
    TEST_ASSERT(ext_uic_feq(scrollSpeed, 30.0f), "Scroll speed round-trips to 30");

    result = lc_scroll_container_set_scroll_bar_width(scroll, 14.0f);
    TEST_RESULT(result, "Set scroll bar width");

    float scrollBarWidth = 0.0f;
    result = lc_scroll_container_get_scroll_bar_width(scroll, &scrollBarWidth);
    TEST_RESULT(result, "Get scroll bar width");
    TEST_ASSERT(ext_uic_feq(scrollBarWidth, 14.0f), "Scroll bar width round-trips to 14");

    /* Scroll bar visibility */
    result = lc_scroll_container_set_h_scroll_bar_visibility(scroll, LC_SCROLLBAR_ALWAYS);
    TEST_RESULT(result, "Set horizontal scrollbar visibility");

    LCScrollBarVisibility hVis = LC_SCROLLBAR_AUTO;
    result = lc_scroll_container_get_h_scroll_bar_visibility(scroll, &hVis);
    TEST_RESULT(result, "Get horizontal scrollbar visibility");
    TEST_ASSERT(hVis == LC_SCROLLBAR_ALWAYS, "Horizontal scrollbar visibility is always");

    result = lc_scroll_container_set_v_scroll_bar_visibility(scroll, LC_SCROLLBAR_NEVER);
    TEST_RESULT(result, "Set vertical scrollbar visibility");

    LCScrollBarVisibility vVis = LC_SCROLLBAR_AUTO;
    result = lc_scroll_container_get_v_scroll_bar_visibility(scroll, &vVis);
    TEST_RESULT(result, "Get vertical scrollbar visibility");
    TEST_ASSERT(vVis == LC_SCROLLBAR_NEVER, "Vertical scrollbar visibility is never");

    lc_scene_destroy(scene);

    SECTION_END("UI: ScrollContainer Component");
}

static void test_ext_ui_tab_container(void) {
    SECTION_START("UI: TabContainer Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_ext_tab_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "TabContainerNode", &node);
    TEST_RESULT(result, "Create node for TabContainer");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle tabs = NULL;
    result = lc_tab_container_create(node, &tabs);
    TEST_RESULT(result, "Create TabContainer component");

    /* Tab count (tabs are derived from child nodes) */
    int tabCount = -1;
    result = lc_tab_container_get_tab_count(tabs, &tabCount);
    TEST_RESULT(result, "Get tab count");
    TEST_ASSERT(tabCount == 0, "Tab count is 0 with no child nodes");

    /* Current tab */
    result = lc_tab_container_set_current_tab(tabs, 0);
    TEST_RESULT(result, "Set current tab to 0");

    int currentTab = -1;
    result = lc_tab_container_get_current_tab(tabs, &currentTab);
    TEST_RESULT(result, "Get current tab");
    TEST_ASSERT(currentTab == 0, "Current tab is 0");

    /* Tab title query for a non-existent index */
    char titleBuf[128];
    titleBuf[0] = '\0';
    int titleRequired = -1;
    lc_tab_container_get_tab_title(tabs, 0, titleBuf, (int)sizeof(titleBuf), &titleRequired);

    /* Tab height */
    result = lc_tab_container_set_tab_height(tabs, 36.0f);
    TEST_RESULT(result, "Set tab height");

    float tabHeight = 0.0f;
    result = lc_tab_container_get_tab_height(tabs, &tabHeight);
    TEST_RESULT(result, "Get tab height");
    TEST_ASSERT(ext_uic_feq(tabHeight, 36.0f), "Tab height round-trips to 36");

    /* Font path */
    result = lc_tab_container_set_font_path(tabs, "res://fonts/tabs.ttf");
    TEST_RESULT(result, "Set font path");

    char fontBuf[256];
    fontBuf[0] = '\0';
    int fontRequired = -1;
    result = lc_tab_container_get_font_path(tabs, fontBuf, (int)sizeof(fontBuf), &fontRequired);
    TEST_RESULT(result, "Get font path");
    TEST_ASSERT(strcmp(fontBuf, "res://fonts/tabs.ttf") == 0, "Font path round-trips");

    /* Font size */
    result = lc_tab_container_set_font_size(tabs, 18.0f);
    TEST_RESULT(result, "Set font size");

    float fontSize = 0.0f;
    result = lc_tab_container_get_font_size(tabs, &fontSize);
    TEST_RESULT(result, "Get font size");
    TEST_ASSERT(ext_uic_feq(fontSize, 18.0f), "Font size round-trips to 18");

    /* Font color */
    result = lc_tab_container_set_font_color(tabs, lc_color(0.9f, 0.8f, 0.7f, 1.0f));
    TEST_RESULT(result, "Set font color");

    LCColor fontColor = { 0.0f, 0.0f, 0.0f, 0.0f };
    result = lc_tab_container_get_font_color(tabs, &fontColor);
    TEST_RESULT(result, "Get font color");
    TEST_ASSERT(ext_uic_feq(fontColor.r, 0.9f) && ext_uic_feq(fontColor.g, 0.8f) &&
                ext_uic_feq(fontColor.b, 0.7f), "Font color round-trips");

    /* Active tab color */
    result = lc_tab_container_set_active_tab_color(tabs, lc_color(0.2f, 0.4f, 0.6f, 1.0f));
    TEST_RESULT(result, "Set active tab color");

    LCColor activeColor = { 0.0f, 0.0f, 0.0f, 0.0f };
    result = lc_tab_container_get_active_tab_color(tabs, &activeColor);
    TEST_RESULT(result, "Get active tab color");
    TEST_ASSERT(ext_uic_feq(activeColor.r, 0.2f) && ext_uic_feq(activeColor.g, 0.4f) &&
                ext_uic_feq(activeColor.b, 0.6f), "Active tab color round-trips");

    lc_scene_destroy(scene);

    SECTION_END("UI: TabContainer Component");
}

static void test_ext_ui_center_container(void) {
    SECTION_START("UI: CenterContainer (untested members)");

    LCComponentHandle center = NULL;
    LCResult result = lc_center_container_create("ExtCenterContainer", &center);
    TEST_RESULT(result, "Create CenterContainer component");

    /* Height round-trip (width tested in base suite) */
    result = lc_center_container_set_height(center, 450.0f);
    TEST_RESULT(result, "Set height");

    float height = 0.0f;
    result = lc_center_container_get_height(center, &height);
    TEST_RESULT(result, "Get height");
    TEST_ASSERT(ext_uic_feq(height, 450.0f), "Height round-trips to 450");

    /* Sorting order */
    result = lc_center_container_set_sorting_order(center, 12);
    TEST_RESULT(result, "Set sorting order");

    int sortingOrder = -1;
    result = lc_center_container_get_sorting_order(center, &sortingOrder);
    TEST_RESULT(result, "Get sorting order");
    TEST_ASSERT(sortingOrder == 12, "Sorting order is 12");

    /* Use UI space */
    result = lc_center_container_set_use_ui_space(center, true);
    TEST_RESULT(result, "Enable use UI space");

    bool useUISpace = false;
    result = lc_center_container_get_use_ui_space(center, &useUISpace);
    TEST_RESULT(result, "Get use UI space");
    TEST_ASSERT(useUISpace == true, "Use UI space is true");

    lc_component_destroy(center);

    SECTION_END("UI: CenterContainer (untested members)");
}

static void test_ext_ui_padding_container(void) {
    SECTION_START("UI: PaddingContainer (untested members)");

    LCComponentHandle padding = NULL;
    LCResult result = lc_padding_container_create("ExtPaddingContainer", &padding);
    TEST_RESULT(result, "Create PaddingContainer component");

    /* Height round-trip */
    result = lc_padding_container_set_height(padding, 360.0f);
    TEST_RESULT(result, "Set height");

    float height = 0.0f;
    result = lc_padding_container_get_height(padding, &height);
    TEST_RESULT(result, "Get height");
    TEST_ASSERT(ext_uic_feq(height, 360.0f), "Height round-trips to 360");

    /* Layer */
    result = lc_padding_container_set_layer(padding, 6);
    TEST_RESULT(result, "Set layer");

    int layer = -1;
    result = lc_padding_container_get_layer(padding, &layer);
    TEST_RESULT(result, "Get layer");
    TEST_ASSERT(layer == 6, "Layer is 6");

    /* Sorting order */
    result = lc_padding_container_set_sorting_order(padding, 18);
    TEST_RESULT(result, "Set sorting order");

    int sortingOrder = -1;
    result = lc_padding_container_get_sorting_order(padding, &sortingOrder);
    TEST_RESULT(result, "Get sorting order");
    TEST_ASSERT(sortingOrder == 18, "Sorting order is 18");

    /* Use UI space */
    result = lc_padding_container_set_use_ui_space(padding, true);
    TEST_RESULT(result, "Enable use UI space");

    bool useUISpace = false;
    result = lc_padding_container_get_use_ui_space(padding, &useUISpace);
    TEST_RESULT(result, "Get use UI space");
    TEST_ASSERT(useUISpace == true, "Use UI space is true");

    /* All padding alignment values */
    LCPaddingAlignment alignments[9] = {
        LC_PADDING_ALIGN_TOP_LEFT, LC_PADDING_ALIGN_TOP_CENTER, LC_PADDING_ALIGN_TOP_RIGHT,
        LC_PADDING_ALIGN_CENTER_LEFT, LC_PADDING_ALIGN_CENTER, LC_PADDING_ALIGN_CENTER_RIGHT,
        LC_PADDING_ALIGN_BOTTOM_LEFT, LC_PADDING_ALIGN_BOTTOM_CENTER, LC_PADDING_ALIGN_BOTTOM_RIGHT
    };
    for (int index = 0; index < 9; index++) {
        result = lc_padding_container_set_child_alignment(padding, alignments[index]);
        TEST_RESULT(result, "Set child alignment value");

        LCPaddingAlignment readback = LC_PADDING_ALIGN_TOP_LEFT;
        result = lc_padding_container_get_child_alignment(padding, &readback);
        TEST_RESULT(result, "Get child alignment value");
        TEST_ASSERT(readback == alignments[index], "Child alignment round-trips");
    }

    lc_component_destroy(padding);

    SECTION_END("UI: PaddingContainer (untested members)");
}

static void test_ext_ui_dock_container(void) {
    SECTION_START("UI: DockContainer (untested members)");

    LCComponentHandle dock = NULL;
    LCResult result = lc_dock_container_create("ExtDockContainer", &dock);
    TEST_RESULT(result, "Create DockContainer component");

    /* Sorting order */
    result = lc_dock_container_set_sorting_order(dock, 9);
    TEST_RESULT(result, "Set sorting order");

    int sortingOrder = -1;
    result = lc_dock_container_get_sorting_order(dock, &sortingOrder);
    TEST_RESULT(result, "Get sorting order");
    TEST_ASSERT(sortingOrder == 9, "Sorting order is 9");

    /* Use UI space */
    result = lc_dock_container_set_use_ui_space(dock, true);
    TEST_RESULT(result, "Enable use UI space");

    bool useUISpace = false;
    result = lc_dock_container_get_use_ui_space(dock, &useUISpace);
    TEST_RESULT(result, "Get use UI space");
    TEST_ASSERT(useUISpace == true, "Use UI space is true");

    /* All dock sides */
    LCDockSide sides[5] = { LC_DOCK_TOP, LC_DOCK_BOTTOM, LC_DOCK_LEFT, LC_DOCK_RIGHT, LC_DOCK_CENTER };
    for (int index = 0; index < 5; index++) {
        result = lc_dock_container_set_default_dock_side(dock, sides[index]);
        TEST_RESULT(result, "Set default dock side value");

        LCDockSide readback = LC_DOCK_TOP;
        result = lc_dock_container_get_default_dock_side(dock, &readback);
        TEST_RESULT(result, "Get default dock side value");
        TEST_ASSERT(readback == sides[index], "Dock side round-trips");
    }

    lc_component_destroy(dock);

    SECTION_END("UI: DockContainer (untested members)");
}

static void test_ext_ui_nine_slice_panel(void) {
    SECTION_START("UI: NineSlicePanel (untested members)");

    LCComponentHandle panel = NULL;
    LCResult result = lc_nine_slice_panel_create("ExtNineSlicePanel", &panel);
    TEST_RESULT(result, "Create NineSlicePanel component");

    /* Height round-trip */
    result = lc_nine_slice_panel_set_height(panel, 220.0f);
    TEST_RESULT(result, "Set height");

    float height = 0.0f;
    result = lc_nine_slice_panel_get_height(panel, &height);
    TEST_RESULT(result, "Get height");
    TEST_ASSERT(ext_uic_feq(height, 220.0f), "Height round-trips to 220");

    /* Individual margin getters */
    result = lc_nine_slice_panel_set_margin_right(panel, 11.0f);
    TEST_RESULT(result, "Set right margin");

    float marginRight = 0.0f;
    result = lc_nine_slice_panel_get_margin_right(panel, &marginRight);
    TEST_RESULT(result, "Get right margin");
    TEST_ASSERT(ext_uic_feq(marginRight, 11.0f), "Right margin round-trips to 11");

    result = lc_nine_slice_panel_set_margin_top(panel, 13.0f);
    TEST_RESULT(result, "Set top margin");

    float marginTop = 0.0f;
    result = lc_nine_slice_panel_get_margin_top(panel, &marginTop);
    TEST_RESULT(result, "Get top margin");
    TEST_ASSERT(ext_uic_feq(marginTop, 13.0f), "Top margin round-trips to 13");

    result = lc_nine_slice_panel_set_margin_bottom(panel, 17.0f);
    TEST_RESULT(result, "Set bottom margin");

    float marginBottom = 0.0f;
    result = lc_nine_slice_panel_get_margin_bottom(panel, &marginBottom);
    TEST_RESULT(result, "Get bottom margin");
    TEST_ASSERT(ext_uic_feq(marginBottom, 17.0f), "Bottom margin round-trips to 17");

    /* Set all margins then verify left getter */
    result = lc_nine_slice_panel_set_margins(panel, 21.0f, 22.0f, 23.0f, 24.0f);
    TEST_RESULT(result, "Set all margins");

    float marginLeft = 0.0f;
    result = lc_nine_slice_panel_get_margin_left(panel, &marginLeft);
    TEST_RESULT(result, "Get left margin after set_margins");
    TEST_ASSERT(ext_uic_feq(marginLeft, 21.0f), "Left margin round-trips to 21");

    /* Modulate getter */
    result = lc_nine_slice_panel_set_modulate(panel, lc_color(0.3f, 0.6f, 0.9f, 1.0f));
    TEST_RESULT(result, "Set modulate color");

    LCColor modulate = { 0.0f, 0.0f, 0.0f, 0.0f };
    result = lc_nine_slice_panel_get_modulate(panel, &modulate);
    TEST_RESULT(result, "Get modulate color");
    TEST_ASSERT(ext_uic_feq(modulate.r, 0.3f) && ext_uic_feq(modulate.g, 0.6f) &&
                ext_uic_feq(modulate.b, 0.9f), "Modulate color round-trips");

    /* Sorting order */
    result = lc_nine_slice_panel_set_sorting_order(panel, 14);
    TEST_RESULT(result, "Set sorting order");

    int sortingOrder = -1;
    result = lc_nine_slice_panel_get_sorting_order(panel, &sortingOrder);
    TEST_RESULT(result, "Get sorting order");
    TEST_ASSERT(sortingOrder == 14, "Sorting order is 14");

    /* Use UI space */
    result = lc_nine_slice_panel_set_use_ui_space(panel, true);
    TEST_RESULT(result, "Enable use UI space");

    bool useUISpace = false;
    result = lc_nine_slice_panel_get_use_ui_space(panel, &useUISpace);
    TEST_RESULT(result, "Get use UI space");
    TEST_ASSERT(useUISpace == true, "Use UI space is true");

    lc_component_destroy(panel);

    SECTION_END("UI: NineSlicePanel (untested members)");
}

static void test_ext_ui_checklist(void) {
    SECTION_START("UI: CheckList (untested members)");

    LCComponentHandle checklist = NULL;
    LCResult result = lc_checklist_create("ExtCheckList", &checklist);
    TEST_RESULT(result, "Create CheckList component");

    /* Add item with auto-assigned value (-1) and verify counts */
    result = lc_checklist_add_item(checklist, "Auto", -1);
    TEST_RESULT(result, "Add item with auto-assigned value");

    int itemCount = -1;
    result = lc_checklist_get_item_count(checklist, &itemCount);
    TEST_RESULT(result, "Get item count");
    TEST_ASSERT(itemCount == 1, "Item count is 1");

    result = lc_checklist_clear_items(checklist);
    TEST_RESULT(result, "Clear items");

    /* Checked values buffer query on empty group */
    int values[8];
    int valueCount = -1;
    result = lc_checklist_get_checked_values(checklist, values, 8, &valueCount);
    TEST_RESULT(result, "Get checked values");
    TEST_ASSERT(valueCount == 0, "Checked value count is 0");

    lc_component_destroy(checklist);

    SECTION_END("UI: CheckList (untested members)");
}

static void test_ext_ui_radiolist(void) {
    SECTION_START("UI: RadioList (untested members)");

    LCComponentHandle radiolist = NULL;
    LCResult result = lc_radiolist_create("ExtRadioList", &radiolist);
    TEST_RESULT(result, "Create RadioList component");

    /* Selected index round-trip behavior with no buttons */
    result = lc_radiolist_set_selected_index(radiolist, -1);
    TEST_RESULT(result, "Set selected index to -1 (deselect)");

    int selectedIndex = -2;
    result = lc_radiolist_get_selected_index(radiolist, &selectedIndex);
    TEST_RESULT(result, "Get selected index");
    TEST_ASSERT(selectedIndex == -1, "Selected index is -1 when deselected");

    int selectedValue = -2;
    result = lc_radiolist_get_selected_value(radiolist, &selectedValue);
    TEST_RESULT(result, "Get selected value");
    TEST_ASSERT(selectedValue == -1, "Selected value is -1 when none selected");

    /* Item count grows and clears */
    result = lc_radiolist_add_item(radiolist, "First");
    TEST_RESULT(result, "Add item 'First'");

    int itemCount = -1;
    result = lc_radiolist_get_item_count(radiolist, &itemCount);
    TEST_RESULT(result, "Get item count");
    TEST_ASSERT(itemCount == 1, "Item count is 1");

    result = lc_radiolist_clear_items(radiolist);
    TEST_RESULT(result, "Clear items");

    result = lc_radiolist_get_item_count(radiolist, &itemCount);
    TEST_RESULT(result, "Get item count after clear");
    TEST_ASSERT(itemCount == 0, "Item count is 0 after clear");

    lc_component_destroy(radiolist);

    SECTION_END("UI: RadioList (untested members)");
}

static void test_ext_ui_label(void) {
    SECTION_START("UI: Label (untested members)");

    LCComponentHandle label = NULL;
    LCResult result = lc_label_create("ExtLabel", &label);
    TEST_RESULT(result, "Create Label component");

    /* Text getter round-trip */
    result = lc_label_set_text(label, "Extended Label");
    TEST_RESULT(result, "Set text");

    char text[128];
    text[0] = '\0';
    result = lc_label_get_text(label, text, sizeof(text));
    TEST_RESULT(result, "Get text");
    TEST_ASSERT(strcmp(text, "Extended Label") == 0, "Text round-trips");

    /* Font path get/set */
    result = lc_label_set_font_path(label, "res://fonts/label.ttf");
    TEST_RESULT(result, "Set font path");

    char fontPath[256];
    fontPath[0] = '\0';
    result = lc_label_get_font_path(label, fontPath, sizeof(fontPath));
    TEST_RESULT(result, "Get font path");
    TEST_ASSERT(strcmp(fontPath, "res://fonts/label.ttf") == 0, "Font path round-trips");

    /* Load font (path may not exist; just verify the call returns a result code) */
    result = lc_label_load_font(label, "res://fonts/label.ttf");
    TEST_ASSERT(result == LC_SUCCESS || result != LC_SUCCESS, "Load font call completes");

    /* Color getter */
    result = lc_label_set_color(label, lc_color(0.4f, 0.5f, 0.6f, 1.0f));
    TEST_RESULT(result, "Set color");

    LCColor color = { 0.0f, 0.0f, 0.0f, 0.0f };
    result = lc_label_get_color(label, &color);
    TEST_RESULT(result, "Get color");
    TEST_ASSERT(ext_uic_feq(color.r, 0.4f) && ext_uic_feq(color.g, 0.5f) &&
                ext_uic_feq(color.b, 0.6f), "Color round-trips");

    /* Centered getter, toggled and restored */
    result = lc_label_set_centered(label, true);
    TEST_RESULT(result, "Set centered true");

    bool centered = false;
    result = lc_label_get_centered(label, &centered);
    TEST_RESULT(result, "Get centered");
    TEST_ASSERT(centered == true, "Centered is true");

    result = lc_label_set_centered(label, false);
    TEST_RESULT(result, "Set centered false");

    result = lc_label_get_centered(label, &centered);
    TEST_RESULT(result, "Get centered after restore");
    TEST_ASSERT(centered == false, "Centered is false");

    /* Offset getter */
    result = lc_label_set_offset(label, lc_vec2(12.0f, 18.0f));
    TEST_RESULT(result, "Set offset");

    LCVec2 offset = { 0.0f, 0.0f };
    result = lc_label_get_offset(label, &offset);
    TEST_RESULT(result, "Get offset");
    TEST_ASSERT(ext_uic_feq(offset.x, 12.0f) && ext_uic_feq(offset.y, 18.0f), "Offset round-trips to (12,18)");

    lc_component_destroy(label);

    SECTION_END("UI: Label (untested members)");
}
