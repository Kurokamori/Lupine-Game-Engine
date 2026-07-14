/**
 * @file test_ext_ui_forms.inc
 * @brief Extended C API tests for Slider, LineEdit, TextEdit and SpinBox UI components.
 *
 * Included directly into capi_test_suite.c. No #include, no main(), no macro
 * redefinitions. Uses SECTION_START / SECTION_END / TEST_ASSERT / TEST_RESULT.
 */

static bool test_ext_ui_forms_close(float a, float b) {
    float diff = a - b;
    if (diff < 0.0f) {
        diff = -diff;
    }
    return diff <= 0.001f;
}

static bool test_ext_ui_forms_color_eq(LCColor c, float r, float g, float b, float a) {
    return test_ext_ui_forms_close(c.r, r)
        && test_ext_ui_forms_close(c.g, g)
        && test_ext_ui_forms_close(c.b, b)
        && test_ext_ui_forms_close(c.a, a);
}

static void test_ext_ui_slider(void) {
    SECTION_START("UI Ext: Slider Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_ext_slider_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "SliderNode", &node);
    TEST_RESULT(result, "Create node for Slider");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle slider = NULL;
    result = lc_slider_create(node, &slider);
    TEST_RESULT(result, "Create Slider component");

    float fval = 0.0f;

    result = lc_slider_set_min_value(slider, 0.0f);
    TEST_RESULT(result, "Set min value");
    result = lc_slider_get_min_value(slider, &fval);
    TEST_RESULT(result, "Get min value");
    TEST_ASSERT(test_ext_ui_forms_close(fval, 0.0f), "Min value round-trips");

    result = lc_slider_set_max_value(slider, 200.0f);
    TEST_RESULT(result, "Set max value");
    result = lc_slider_get_max_value(slider, &fval);
    TEST_RESULT(result, "Get max value");
    TEST_ASSERT(test_ext_ui_forms_close(fval, 200.0f), "Max value round-trips");

    result = lc_slider_set_value(slider, 75.0f);
    TEST_RESULT(result, "Set value");
    result = lc_slider_get_value(slider, &fval);
    TEST_RESULT(result, "Get value");
    TEST_ASSERT(test_ext_ui_forms_close(fval, 75.0f), "Value round-trips");

    result = lc_slider_set_value(slider, 1000.0f);
    TEST_RESULT(result, "Set value above max");
    result = lc_slider_get_value(slider, &fval);
    TEST_RESULT(result, "Get clamped value");
    TEST_ASSERT(fval <= 200.0f + 0.001f, "Value clamped to max");

    result = lc_slider_set_value(slider, -50.0f);
    TEST_RESULT(result, "Set value below min");
    result = lc_slider_get_value(slider, &fval);
    TEST_RESULT(result, "Get clamped value below min");
    TEST_ASSERT(fval >= 0.0f - 0.001f, "Value clamped to min");

    result = lc_slider_set_value(slider, 100.0f);
    TEST_RESULT(result, "Restore mid value");

    result = lc_slider_set_step(slider, 5.0f);
    TEST_RESULT(result, "Set step");
    result = lc_slider_get_step(slider, &fval);
    TEST_RESULT(result, "Get step");
    TEST_ASSERT(test_ext_ui_forms_close(fval, 5.0f), "Step round-trips");

    float ratio = -1.0f;
    result = lc_slider_get_ratio(slider, &ratio);
    TEST_RESULT(result, "Get ratio");
    TEST_ASSERT(ratio >= -0.001f && ratio <= 1.001f, "Ratio within [0,1]");

    LCSliderOrientation orientation = LC_SLIDER_HORIZONTAL;
    result = lc_slider_set_orientation(slider, LC_SLIDER_VERTICAL);
    TEST_RESULT(result, "Set orientation Vertical");
    result = lc_slider_get_orientation(slider, &orientation);
    TEST_RESULT(result, "Get orientation");
    TEST_ASSERT(orientation == LC_SLIDER_VERTICAL, "Orientation is Vertical");

    result = lc_slider_set_orientation(slider, LC_SLIDER_HORIZONTAL);
    TEST_RESULT(result, "Restore orientation Horizontal");
    result = lc_slider_get_orientation(slider, &orientation);
    TEST_RESULT(result, "Get orientation again");
    TEST_ASSERT(orientation == LC_SLIDER_HORIZONTAL, "Orientation is Horizontal");

    bool editable = true;
    result = lc_slider_set_editable(slider, false);
    TEST_RESULT(result, "Set editable false");
    result = lc_slider_get_editable(slider, &editable);
    TEST_RESULT(result, "Get editable");
    TEST_ASSERT(editable == false, "Editable is false");
    result = lc_slider_set_editable(slider, true);
    TEST_RESULT(result, "Restore editable true");
    result = lc_slider_get_editable(slider, &editable);
    TEST_RESULT(result, "Get editable again");
    TEST_ASSERT(editable == true, "Editable is true");

    LCColor color = lc_color(0.0f, 0.0f, 0.0f, 0.0f);

    result = lc_slider_set_track_color(slider, lc_color(0.1f, 0.2f, 0.3f, 1.0f));
    TEST_RESULT(result, "Set track color");
    result = lc_slider_get_track_color(slider, &color);
    TEST_RESULT(result, "Get track color");
    TEST_ASSERT(test_ext_ui_forms_color_eq(color, 0.1f, 0.2f, 0.3f, 1.0f), "Track color round-trips");

    result = lc_slider_set_fill_color(slider, lc_color(0.4f, 0.5f, 0.6f, 1.0f));
    TEST_RESULT(result, "Set fill color");
    result = lc_slider_get_fill_color(slider, &color);
    TEST_RESULT(result, "Get fill color");
    TEST_ASSERT(test_ext_ui_forms_color_eq(color, 0.4f, 0.5f, 0.6f, 1.0f), "Fill color round-trips");

    result = lc_slider_set_handle_color(slider, lc_color(0.7f, 0.8f, 0.9f, 1.0f));
    TEST_RESULT(result, "Set handle color");
    result = lc_slider_get_handle_color(slider, &color);
    TEST_RESULT(result, "Get handle color");
    TEST_ASSERT(test_ext_ui_forms_color_eq(color, 0.7f, 0.8f, 0.9f, 1.0f), "Handle color round-trips");

    result = lc_slider_set_track_thickness(slider, 6.0f);
    TEST_RESULT(result, "Set track thickness");
    result = lc_slider_get_track_thickness(slider, &fval);
    TEST_RESULT(result, "Get track thickness");
    TEST_ASSERT(test_ext_ui_forms_close(fval, 6.0f), "Track thickness round-trips");

    result = lc_slider_set_handle_radius(slider, 9.0f);
    TEST_RESULT(result, "Set handle radius");
    result = lc_slider_get_handle_radius(slider, &fval);
    TEST_RESULT(result, "Get handle radius");
    TEST_ASSERT(test_ext_ui_forms_close(fval, 9.0f), "Handle radius round-trips");

    lc_scene_destroy(scene);

    SECTION_END("UI Ext: Slider Component");
}

static void test_ext_ui_line_edit(void) {
    SECTION_START("UI Ext: LineEdit Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_ext_line_edit_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "LineEditNode", &node);
    TEST_RESULT(result, "Create node for LineEdit");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle lineEdit = NULL;
    result = lc_line_edit_create(node, &lineEdit);
    TEST_RESULT(result, "Create LineEdit component");

    char buffer[256];
    int required = -1;

    result = lc_line_edit_set_text(lineEdit, "Hello World");
    TEST_RESULT(result, "Set text");
    buffer[0] = '\0';
    result = lc_line_edit_get_text(lineEdit, buffer, (int)sizeof(buffer), &required);
    TEST_RESULT(result, "Get text");
    TEST_ASSERT(strcmp(buffer, "Hello World") == 0, "Text round-trips");
    TEST_ASSERT(required == (int)strlen("Hello World"), "Text required length correct");

    result = lc_line_edit_set_placeholder(lineEdit, "Enter name");
    TEST_RESULT(result, "Set placeholder");
    buffer[0] = '\0';
    result = lc_line_edit_get_placeholder(lineEdit, buffer, (int)sizeof(buffer), &required);
    TEST_RESULT(result, "Get placeholder");
    TEST_ASSERT(strcmp(buffer, "Enter name") == 0, "Placeholder round-trips");

    bool flag = false;
    result = lc_line_edit_set_editable(lineEdit, false);
    TEST_RESULT(result, "Set editable false");
    result = lc_line_edit_get_editable(lineEdit, &flag);
    TEST_RESULT(result, "Get editable");
    TEST_ASSERT(flag == false, "Editable is false");
    result = lc_line_edit_set_editable(lineEdit, true);
    TEST_RESULT(result, "Restore editable true");
    result = lc_line_edit_get_editable(lineEdit, &flag);
    TEST_RESULT(result, "Get editable again");
    TEST_ASSERT(flag == true, "Editable is true");

    result = lc_line_edit_set_secret(lineEdit, true);
    TEST_RESULT(result, "Set secret true");
    result = lc_line_edit_get_secret(lineEdit, &flag);
    TEST_RESULT(result, "Get secret");
    TEST_ASSERT(flag == true, "Secret is true");
    result = lc_line_edit_set_secret(lineEdit, false);
    TEST_RESULT(result, "Restore secret false");
    result = lc_line_edit_get_secret(lineEdit, &flag);
    TEST_RESULT(result, "Get secret again");
    TEST_ASSERT(flag == false, "Secret is false");

    int maxLength = -1;
    result = lc_line_edit_set_max_length(lineEdit, 32);
    TEST_RESULT(result, "Set max length");
    result = lc_line_edit_get_max_length(lineEdit, &maxLength);
    TEST_RESULT(result, "Get max length");
    TEST_ASSERT(maxLength == 32, "Max length round-trips");

    int caret = -1;
    result = lc_line_edit_set_caret_position(lineEdit, 3);
    TEST_RESULT(result, "Set caret position");
    result = lc_line_edit_get_caret_position(lineEdit, &caret);
    TEST_RESULT(result, "Get caret position");
    TEST_ASSERT(caret == 3, "Caret position round-trips");

    result = lc_line_edit_select_all(lineEdit);
    TEST_RESULT(result, "Select all");
    result = lc_line_edit_deselect(lineEdit);
    TEST_RESULT(result, "Deselect");

    result = lc_line_edit_set_font_path(lineEdit, "res://fonts/test.ttf");
    TEST_RESULT(result, "Set font path");
    buffer[0] = '\0';
    result = lc_line_edit_get_font_path(lineEdit, buffer, (int)sizeof(buffer), &required);
    TEST_RESULT(result, "Get font path");
    TEST_ASSERT(strcmp(buffer, "res://fonts/test.ttf") == 0, "Font path round-trips");

    float fsize = 0.0f;
    result = lc_line_edit_set_font_size(lineEdit, 18.0f);
    TEST_RESULT(result, "Set font size");
    result = lc_line_edit_get_font_size(lineEdit, &fsize);
    TEST_RESULT(result, "Get font size");
    TEST_ASSERT(test_ext_ui_forms_close(fsize, 18.0f), "Font size round-trips");

    LCColor color = lc_color(0.0f, 0.0f, 0.0f, 0.0f);

    result = lc_line_edit_set_font_color(lineEdit, lc_color(0.9f, 0.9f, 0.9f, 1.0f));
    TEST_RESULT(result, "Set font color");
    result = lc_line_edit_get_font_color(lineEdit, &color);
    TEST_RESULT(result, "Get font color");
    TEST_ASSERT(test_ext_ui_forms_color_eq(color, 0.9f, 0.9f, 0.9f, 1.0f), "Font color round-trips");

    result = lc_line_edit_set_background_color(lineEdit, lc_color(0.1f, 0.1f, 0.1f, 1.0f));
    TEST_RESULT(result, "Set background color");
    result = lc_line_edit_get_background_color(lineEdit, &color);
    TEST_RESULT(result, "Get background color");
    TEST_ASSERT(test_ext_ui_forms_color_eq(color, 0.1f, 0.1f, 0.1f, 1.0f), "Background color round-trips");

    result = lc_line_edit_set_selection_color(lineEdit, lc_color(0.2f, 0.4f, 0.8f, 0.5f));
    TEST_RESULT(result, "Set selection color");
    result = lc_line_edit_get_selection_color(lineEdit, &color);
    TEST_RESULT(result, "Get selection color");
    TEST_ASSERT(test_ext_ui_forms_color_eq(color, 0.2f, 0.4f, 0.8f, 0.5f), "Selection color round-trips");

    lc_scene_destroy(scene);

    SECTION_END("UI Ext: LineEdit Component");
}

static void test_ext_ui_text_edit(void) {
    SECTION_START("UI Ext: TextEdit Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_ext_text_edit_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "TextEditNode", &node);
    TEST_RESULT(result, "Create node for TextEdit");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle textEdit = NULL;
    result = lc_text_edit_create(node, &textEdit);
    TEST_RESULT(result, "Create TextEdit component");

    char buffer[256];
    int required = -1;

    result = lc_text_edit_set_text(textEdit, "Line one\nLine two");
    TEST_RESULT(result, "Set text");
    buffer[0] = '\0';
    result = lc_text_edit_get_text(textEdit, buffer, (int)sizeof(buffer), &required);
    TEST_RESULT(result, "Get text");
    TEST_ASSERT(strcmp(buffer, "Line one\nLine two") == 0, "Text round-trips");
    TEST_ASSERT(required == (int)strlen("Line one\nLine two"), "Text required length correct");

    bool editable = false;
    result = lc_text_edit_set_editable(textEdit, false);
    TEST_RESULT(result, "Set editable false");
    result = lc_text_edit_get_editable(textEdit, &editable);
    TEST_RESULT(result, "Get editable");
    TEST_ASSERT(editable == false, "Editable is false");
    result = lc_text_edit_set_editable(textEdit, true);
    TEST_RESULT(result, "Restore editable true");
    result = lc_text_edit_get_editable(textEdit, &editable);
    TEST_RESULT(result, "Get editable again");
    TEST_ASSERT(editable == true, "Editable is true");

    int caret = -1;
    result = lc_text_edit_set_caret_position(textEdit, 4);
    TEST_RESULT(result, "Set caret position");
    result = lc_text_edit_get_caret_position(textEdit, &caret);
    TEST_RESULT(result, "Get caret position");
    TEST_ASSERT(caret == 4, "Caret position round-trips");

    result = lc_text_edit_select_all(textEdit);
    TEST_RESULT(result, "Select all");
    result = lc_text_edit_deselect(textEdit);
    TEST_RESULT(result, "Deselect");

    result = lc_text_edit_set_font_path(textEdit, "res://fonts/mono.ttf");
    TEST_RESULT(result, "Set font path");
    buffer[0] = '\0';
    result = lc_text_edit_get_font_path(textEdit, buffer, (int)sizeof(buffer), &required);
    TEST_RESULT(result, "Get font path");
    TEST_ASSERT(strcmp(buffer, "res://fonts/mono.ttf") == 0, "Font path round-trips");

    float fval = 0.0f;
    result = lc_text_edit_set_font_size(textEdit, 14.0f);
    TEST_RESULT(result, "Set font size");
    result = lc_text_edit_get_font_size(textEdit, &fval);
    TEST_RESULT(result, "Get font size");
    TEST_ASSERT(test_ext_ui_forms_close(fval, 14.0f), "Font size round-trips");

    result = lc_text_edit_set_line_spacing(textEdit, 1.5f);
    TEST_RESULT(result, "Set line spacing");
    result = lc_text_edit_get_line_spacing(textEdit, &fval);
    TEST_RESULT(result, "Get line spacing");
    TEST_ASSERT(test_ext_ui_forms_close(fval, 1.5f), "Line spacing round-trips");

    LCColor color = lc_color(0.0f, 0.0f, 0.0f, 0.0f);

    result = lc_text_edit_set_font_color(textEdit, lc_color(0.85f, 0.85f, 0.85f, 1.0f));
    TEST_RESULT(result, "Set font color");
    result = lc_text_edit_get_font_color(textEdit, &color);
    TEST_RESULT(result, "Get font color");
    TEST_ASSERT(test_ext_ui_forms_color_eq(color, 0.85f, 0.85f, 0.85f, 1.0f), "Font color round-trips");

    result = lc_text_edit_set_background_color(textEdit, lc_color(0.05f, 0.05f, 0.05f, 1.0f));
    TEST_RESULT(result, "Set background color");
    result = lc_text_edit_get_background_color(textEdit, &color);
    TEST_RESULT(result, "Get background color");
    TEST_ASSERT(test_ext_ui_forms_color_eq(color, 0.05f, 0.05f, 0.05f, 1.0f), "Background color round-trips");

    result = lc_text_edit_set_selection_color(textEdit, lc_color(0.3f, 0.5f, 0.9f, 0.6f));
    TEST_RESULT(result, "Set selection color");
    result = lc_text_edit_get_selection_color(textEdit, &color);
    TEST_RESULT(result, "Get selection color");
    TEST_ASSERT(test_ext_ui_forms_color_eq(color, 0.3f, 0.5f, 0.9f, 0.6f), "Selection color round-trips");

    lc_scene_destroy(scene);

    SECTION_END("UI Ext: TextEdit Component");
}

static void test_ext_ui_spin_box(void) {
    SECTION_START("UI Ext: SpinBox Component");

    LCSceneHandle scene = NULL;
    LCResult result = lc_scene_create("test_ext_spin_box_scene", &scene);
    TEST_RESULT(result, "Create test scene");

    LCNodeHandle node = NULL;
    result = lc_node_create(LC_NODE_2D, "SpinBoxNode", &node);
    TEST_RESULT(result, "Create node for SpinBox");

    result = lc_scene_add_node(scene, node);
    TEST_RESULT(result, "Add node to scene");

    LCComponentHandle spinBox = NULL;
    result = lc_spin_box_create(node, &spinBox);
    TEST_RESULT(result, "Create SpinBox component");

    float fval = 0.0f;

    result = lc_spin_box_set_min_value(spinBox, -10.0f);
    TEST_RESULT(result, "Set min value");
    result = lc_spin_box_get_min_value(spinBox, &fval);
    TEST_RESULT(result, "Get min value");
    TEST_ASSERT(test_ext_ui_forms_close(fval, -10.0f), "Min value round-trips");

    result = lc_spin_box_set_max_value(spinBox, 50.0f);
    TEST_RESULT(result, "Set max value");
    result = lc_spin_box_get_max_value(spinBox, &fval);
    TEST_RESULT(result, "Get max value");
    TEST_ASSERT(test_ext_ui_forms_close(fval, 50.0f), "Max value round-trips");

    result = lc_spin_box_set_value(spinBox, 25.0f);
    TEST_RESULT(result, "Set value");
    result = lc_spin_box_get_value(spinBox, &fval);
    TEST_RESULT(result, "Get value");
    TEST_ASSERT(test_ext_ui_forms_close(fval, 25.0f), "Value round-trips");

    result = lc_spin_box_set_value(spinBox, 999.0f);
    TEST_RESULT(result, "Set value above max");
    result = lc_spin_box_get_value(spinBox, &fval);
    TEST_RESULT(result, "Get clamped value");
    TEST_ASSERT(fval <= 50.0f + 0.001f, "Value clamped to max");

    result = lc_spin_box_set_value(spinBox, -999.0f);
    TEST_RESULT(result, "Set value below min");
    result = lc_spin_box_get_value(spinBox, &fval);
    TEST_RESULT(result, "Get clamped value below min");
    TEST_ASSERT(fval >= -10.0f - 0.001f, "Value clamped to min");

    result = lc_spin_box_set_value(spinBox, 12.0f);
    TEST_RESULT(result, "Restore mid value");

    result = lc_spin_box_set_step(spinBox, 0.5f);
    TEST_RESULT(result, "Set step");
    result = lc_spin_box_get_step(spinBox, &fval);
    TEST_RESULT(result, "Get step");
    TEST_ASSERT(test_ext_ui_forms_close(fval, 0.5f), "Step round-trips");

    int decimals = -1;
    result = lc_spin_box_set_decimals(spinBox, 2);
    TEST_RESULT(result, "Set decimals");
    result = lc_spin_box_get_decimals(spinBox, &decimals);
    TEST_RESULT(result, "Get decimals");
    TEST_ASSERT(decimals == 2, "Decimals round-trips");

    bool editable = false;
    result = lc_spin_box_set_editable(spinBox, false);
    TEST_RESULT(result, "Set editable false");
    result = lc_spin_box_get_editable(spinBox, &editable);
    TEST_RESULT(result, "Get editable");
    TEST_ASSERT(editable == false, "Editable is false");
    result = lc_spin_box_set_editable(spinBox, true);
    TEST_RESULT(result, "Restore editable true");
    result = lc_spin_box_get_editable(spinBox, &editable);
    TEST_RESULT(result, "Get editable again");
    TEST_ASSERT(editable == true, "Editable is true");

    char buffer[256];
    int required = -1;

    result = lc_spin_box_set_prefix(spinBox, "$");
    TEST_RESULT(result, "Set prefix");
    buffer[0] = '\0';
    result = lc_spin_box_get_prefix(spinBox, buffer, (int)sizeof(buffer), &required);
    TEST_RESULT(result, "Get prefix");
    TEST_ASSERT(strcmp(buffer, "$") == 0, "Prefix round-trips");

    result = lc_spin_box_set_suffix(spinBox, " kg");
    TEST_RESULT(result, "Set suffix");
    buffer[0] = '\0';
    result = lc_spin_box_get_suffix(spinBox, buffer, (int)sizeof(buffer), &required);
    TEST_RESULT(result, "Get suffix");
    TEST_ASSERT(strcmp(buffer, " kg") == 0, "Suffix round-trips");

    result = lc_spin_box_set_font_path(spinBox, "res://fonts/spin.ttf");
    TEST_RESULT(result, "Set font path");
    buffer[0] = '\0';
    result = lc_spin_box_get_font_path(spinBox, buffer, (int)sizeof(buffer), &required);
    TEST_RESULT(result, "Get font path");
    TEST_ASSERT(strcmp(buffer, "res://fonts/spin.ttf") == 0, "Font path round-trips");

    result = lc_spin_box_set_font_size(spinBox, 16.0f);
    TEST_RESULT(result, "Set font size");
    result = lc_spin_box_get_font_size(spinBox, &fval);
    TEST_RESULT(result, "Get font size");
    TEST_ASSERT(test_ext_ui_forms_close(fval, 16.0f), "Font size round-trips");

    LCColor color = lc_color(0.0f, 0.0f, 0.0f, 0.0f);

    result = lc_spin_box_set_font_color(spinBox, lc_color(0.95f, 0.95f, 0.95f, 1.0f));
    TEST_RESULT(result, "Set font color");
    result = lc_spin_box_get_font_color(spinBox, &color);
    TEST_RESULT(result, "Get font color");
    TEST_ASSERT(test_ext_ui_forms_color_eq(color, 0.95f, 0.95f, 0.95f, 1.0f), "Font color round-trips");

    result = lc_spin_box_set_background_color(spinBox, lc_color(0.15f, 0.15f, 0.15f, 1.0f));
    TEST_RESULT(result, "Set background color");
    result = lc_spin_box_get_background_color(spinBox, &color);
    TEST_RESULT(result, "Get background color");
    TEST_ASSERT(test_ext_ui_forms_color_eq(color, 0.15f, 0.15f, 0.15f, 1.0f), "Background color round-trips");

    result = lc_spin_box_set_button_color(spinBox, lc_color(0.3f, 0.3f, 0.35f, 1.0f));
    TEST_RESULT(result, "Set button color");
    result = lc_spin_box_get_button_color(spinBox, &color);
    TEST_RESULT(result, "Get button color");
    TEST_ASSERT(test_ext_ui_forms_color_eq(color, 0.3f, 0.3f, 0.35f, 1.0f), "Button color round-trips");

    lc_scene_destroy(scene);

    SECTION_END("UI Ext: SpinBox Component");
}
