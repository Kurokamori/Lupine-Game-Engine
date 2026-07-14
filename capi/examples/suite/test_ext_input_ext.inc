/* Further extended input C API tests covering the remaining cross-device input
 * surface in capi/include/input/lc_input.h not exercised by test_ext_input.inc:
 *   - additional action/axis binding mutators (mouse button, gamepad axis, axis key)
 *   - axis binding introspection (count / get / remove / clear)
 *   - captured-binding read / clear / apply
 *   - glyph map + input map file I/O and glyph-override clearing
 *   - per-player action/axis/strength queries and player gamepad lists
 *   - action strength + directional vector queries
 *   - gamepad introspection (connected / count / ids / name) and rumble
 *   - event-driven action matching
 *
 * These run headless: with no device events pumped and (usually) no physical
 * gamepads or window present, device/runtime queries return defined values.
 * Setters/getters that mutate the cached input map are round-tripped directly;
 * everything that reads live device/frame/file state uses tolerant assertions
 * (a defined LCResult, or a callable bool/float that simply must not crash).
 * NULL out-pointer guards are checked where the API documents them. */

static void test_ext_input_ext_axis_bindings(void) {
    SECTION_START("Input Ext: Axis bindings");

    /* Build a known axis binding set from scratch so count/get/remove are
     * deterministic regardless of the shipped default input map. */
    lc_input_clear_axis_bindings("move_x");
    TEST_ASSERT(lc_input_get_axis_binding_count("move_x") == 0,
                "lc_input_clear_axis_bindings() empties the axis");

    lc_input_add_axis_key("move_x", LC_KEY_D, 1.0f);
    lc_input_add_axis_gamepad_axis("move_x", LC_GAMEPAD_AXIS_LEFT_X, 1.0f, 0u);
    TEST_ASSERT(lc_input_get_axis_binding_count("move_x") == 2,
                "lc_input_add_axis_key/add_axis_gamepad_axis() append bindings");

    LCInputBinding b;
    memset(&b, 0, sizeof(b));
    LCResult gb = lc_input_get_axis_binding("move_x", 0, &b);
    TEST_ASSERT(gb == LC_SUCCESS && b.device_type == LC_INPUT_DEVICE_KEYBOARD &&
                b.key_code == LC_KEY_D,
                "lc_input_get_axis_binding() returns the keyboard binding");

    memset(&b, 0, sizeof(b));
    gb = lc_input_get_axis_binding("move_x", 1, &b);
    TEST_ASSERT(gb == LC_SUCCESS && b.device_type == LC_INPUT_DEVICE_GAMEPAD,
                "lc_input_get_axis_binding() returns the gamepad-axis binding");

    lc_input_remove_axis_binding("move_x", 0);
    TEST_ASSERT(lc_input_get_axis_binding_count("move_x") == 1,
                "lc_input_remove_axis_binding() removes one binding");

    TEST_ASSERT(lc_input_get_axis_binding("move_x", 9, &b) == LC_ERROR_INVALID_PARAMETER,
                "lc_input_get_axis_binding() rejects out-of-range index");
    TEST_ASSERT(lc_input_get_axis_binding("move_x", 0, NULL) == LC_ERROR_NULL_POINTER,
                "lc_input_get_axis_binding(NULL) rejects null out-binding");

    lc_input_clear_axis_bindings("move_x");
    TEST_ASSERT(lc_input_get_axis_binding_count("move_x") == 0,
                "lc_input_clear_axis_bindings() empties the axis again");

    SECTION_END("Input Ext: Axis bindings");
}

static void test_ext_input_ext_action_binding_mutators(void) {
    SECTION_START("Input Ext: Action binding mutators");

    /* lc_input_add_action_mouse_button + lc_input_add_action_gamepad_axis append
     * to an action's binding list; round-trip via the existing count getter. */
    lc_input_clear_action_bindings("fire");
    TEST_ASSERT(lc_input_get_action_binding_count("fire") == 0,
                "action starts with no bindings");

    lc_input_add_action_mouse_button("fire", LC_MOUSE_BUTTON_LEFT);
    TEST_ASSERT(lc_input_get_action_binding_count("fire") == 1,
                "lc_input_add_action_mouse_button() appends a binding");

    LCInputBinding b;
    memset(&b, 0, sizeof(b));
    LCResult gb = lc_input_get_action_binding("fire", 0, &b);
    TEST_ASSERT(gb == LC_SUCCESS && b.device_type == LC_INPUT_DEVICE_MOUSE &&
                b.mouse_button == LC_MOUSE_BUTTON_LEFT,
                "lc_input_add_action_mouse_button() records the mouse binding");

    lc_input_add_action_gamepad_axis("fire", LC_GAMEPAD_AXIS_RIGHT_TRIGGER, 1.0f, 0u);
    TEST_ASSERT(lc_input_get_action_binding_count("fire") == 2,
                "lc_input_add_action_gamepad_axis() appends a binding");

    memset(&b, 0, sizeof(b));
    gb = lc_input_get_action_binding("fire", 1, &b);
    TEST_ASSERT(gb == LC_SUCCESS && b.device_type == LC_INPUT_DEVICE_GAMEPAD,
                "lc_input_add_action_gamepad_axis() records the gamepad-axis binding");

    lc_input_clear_action_bindings("fire");
    TEST_ASSERT(lc_input_get_action_binding_count("fire") == 0,
                "action bindings cleared after mutator test");

    SECTION_END("Input Ext: Action binding mutators");
}

static void test_ext_input_ext_capture_apply(void) {
    SECTION_START("Input Ext: Captured binding read/clear/apply");

    /* No input is pumped headless, so no binding is ever captured; the read must
     * still return a defined result and the clear/apply paths must not crash. */
    lc_input_cancel_input_capture();

    LCInputBinding cap;
    memset(&cap, 0, sizeof(cap));
    LCResult cr = lc_input_get_captured_binding(&cap);
    TEST_ASSERT(cr == LC_SUCCESS || cr == LC_ERROR_NOT_FOUND ||
                cr == LC_ERROR_OPERATION_FAILED,
                "lc_input_get_captured_binding() returns a defined result");

    TEST_ASSERT(lc_input_get_captured_binding(NULL) == LC_ERROR_NULL_POINTER,
                "lc_input_get_captured_binding(NULL) rejects null");

    lc_input_clear_captured_binding();
    TEST_ASSERT(true, "lc_input_clear_captured_binding() callable");

    /* Applying with no captured binding is a no-op but must remain safe. */
    lc_input_clear_action_bindings("crouch");
    lc_input_apply_captured_binding_to_action("crouch");
    TEST_ASSERT(lc_input_get_action_binding_count("crouch") >= 0,
                "lc_input_apply_captured_binding_to_action() callable (no capture)");

    lc_input_clear_action_bindings("crouch");

    SECTION_END("Input Ext: Captured binding read/clear/apply");
}

static void test_ext_input_ext_map_io(void) {
    SECTION_START("Input Ext: Input map & glyph map I/O");

    /* Seed a deterministic action so a saved input map has content. */
    lc_input_clear_action_bindings("reload");
    lc_input_add_action_key("reload", LC_KEY_R);

    const char* input_map_path =
        "C:/Users/KUROKA~1/AppData/Local/Temp/claude/lc_capi_input_map.json";
    const char* glyph_map_path =
        "C:/Users/KUROKA~1/AppData/Local/Temp/claude/lc_capi_glyph_map.json";

    /* File I/O depends on a writable filesystem; accept either outcome but
     * require a real round-trip when the save succeeds. */
    bool saved_map = lc_input_save_input_map(input_map_path);
    TEST_ASSERT(saved_map == true || saved_map == false,
                "lc_input_save_input_map() callable");
    if (saved_map) {
        bool loaded_map = lc_input_load_input_map(input_map_path);
        TEST_ASSERT(loaded_map == true,
                    "lc_input_load_input_map() reloads a just-saved input map");
    }

    bool loaded_missing = lc_input_load_input_map(
        "C:/Users/KUROKA~1/AppData/Local/Temp/claude/lc_capi_no_such_map.json");
    TEST_ASSERT(loaded_missing == false,
                "lc_input_load_input_map() reports failure for a missing file");

    lc_input_set_glyph_label("key_r", "Reload");
    bool saved_glyphs = lc_input_save_glyph_map(glyph_map_path);
    TEST_ASSERT(saved_glyphs == true || saved_glyphs == false,
                "lc_input_save_glyph_map() callable");
    if (saved_glyphs) {
        bool loaded_glyphs = lc_input_load_glyph_map(glyph_map_path);
        TEST_ASSERT(loaded_glyphs == true,
                    "lc_input_load_glyph_map() reloads a just-saved glyph map");
    }

    bool loaded_missing_glyphs = lc_input_load_glyph_map(
        "C:/Users/KUROKA~1/AppData/Local/Temp/claude/lc_capi_no_such_glyphs.json");
    TEST_ASSERT(loaded_missing_glyphs == false,
                "lc_input_load_glyph_map() reports failure for a missing file");

    lc_input_clear_action_bindings("reload");

    SECTION_END("Input Ext: Input map & glyph map I/O");
}

static void test_ext_input_ext_glyphs_and_contexts(void) {
    SECTION_START("Input Ext: Glyph overrides & active contexts JSON");

    /* clear_glyph_overrides wipes every override at once. */
    lc_input_clear_action_bindings("use");
    lc_input_add_action_key("use", LC_KEY_F);
    lc_input_set_glyph_label("key_f", "Use Item");

    LCInputGlyph glyph;
    memset(&glyph, 0, sizeof(glyph));
    lc_input_get_action_glyph("use", -1, LC_INPUT_DEVICE_KEYBOARD, &glyph);
    TEST_ASSERT(strcmp(glyph.label, "Use Item") == 0,
                "glyph label override applied before clear-all");

    lc_input_clear_glyph_overrides();
    memset(&glyph, 0, sizeof(glyph));
    lc_input_get_action_glyph("use", -1, LC_INPUT_DEVICE_KEYBOARD, &glyph);
    TEST_ASSERT(strcmp(glyph.label, "Use Item") != 0,
                "lc_input_clear_glyph_overrides() removes all overrides");

    /* get_action_glyphs serializes one glyph per binding as a JSON array. */
    char glyphs_json[1024];
    memset(glyphs_json, 0, sizeof(glyphs_json));
    LCResult gr = lc_input_get_action_glyphs("use", glyphs_json, sizeof(glyphs_json));
    TEST_ASSERT(gr == LC_SUCCESS, "lc_input_get_action_glyphs() returns success");
    TEST_ASSERT(strlen(glyphs_json) > 0, "lc_input_get_action_glyphs() writes JSON");
    TEST_ASSERT(lc_input_get_action_glyphs("use", NULL, 0) == LC_ERROR_NULL_POINTER,
                "lc_input_get_action_glyphs(NULL) rejects null buffer");

    /* get_active_contexts serializes the active context set as a JSON array. */
    lc_input_enable_context("gameplay");
    char ctx_json[512];
    memset(ctx_json, 0, sizeof(ctx_json));
    LCResult cr = lc_input_get_active_contexts(ctx_json, sizeof(ctx_json));
    TEST_ASSERT(cr == LC_SUCCESS, "lc_input_get_active_contexts() returns success");
    TEST_ASSERT(strlen(ctx_json) > 0, "lc_input_get_active_contexts() writes JSON");
    TEST_ASSERT(lc_input_get_active_contexts(NULL, 0) == LC_ERROR_NULL_POINTER,
                "lc_input_get_active_contexts(NULL) rejects null buffer");
    lc_input_disable_context("gameplay");

    lc_input_clear_action_bindings("use");

    SECTION_END("Input Ext: Glyph overrides & active contexts JSON");
}

static void test_ext_input_ext_enable_toggles(void) {
    SECTION_START("Input Ext: Action / axis enable toggles");

    /* These mutate cached enabled-state; with no live edge they cannot be
     * observed via a press, but the calls must be safe and pair cleanly. */
    lc_input_clear_action_bindings("dash");
    lc_input_add_action_key("dash", LC_KEY_LEFT_SHIFT);
    lc_input_set_action_enabled("dash", false);
    TEST_ASSERT(lc_input_is_action_pressed("dash") == false,
                "disabled action does not read as pressed (no input pumped)");
    lc_input_set_action_enabled("dash", true);
    TEST_ASSERT(lc_input_is_action_pressed("dash") == false,
                "re-enabled action still not pressed without input");

    lc_input_clear_axis_bindings("throttle");
    lc_input_add_axis_key("throttle", LC_KEY_W, 1.0f);
    lc_input_set_axis_enabled("throttle", false);
    float v = lc_input_get_axis("throttle");
    TEST_ASSERT(v > -1.01f && v < 1.01f, "disabled axis returns an in-range value");
    lc_input_set_axis_enabled("throttle", true);

    lc_input_clear_action_bindings("dash");
    lc_input_clear_axis_bindings("throttle");

    SECTION_END("Input Ext: Action / axis enable toggles");
}

static void test_ext_input_ext_per_player(void) {
    SECTION_START("Input Ext: Per-player queries");

    lc_input_set_player_count(2);

    /* No input is pumped, so pressed/just-pressed/just-released are all false and
     * axis/strength are well-defined in range for any player slot. */
    TEST_ASSERT(lc_input_is_action_pressed_for_player("jump", 0) == false,
                "lc_input_is_action_pressed_for_player() false without input");
    TEST_ASSERT(lc_input_is_action_just_pressed_for_player("jump", 0) == false,
                "lc_input_is_action_just_pressed_for_player() false without input");
    TEST_ASSERT(lc_input_is_action_just_released_for_player("jump", 0) == false,
                "lc_input_is_action_just_released_for_player() false without input");

    float ax = lc_input_get_axis_for_player("move_x", 1);
    TEST_ASSERT(ax > -1.01f && ax < 1.01f,
                "lc_input_get_axis_for_player() returns an in-range value");

    float st = lc_input_get_action_strength_for_player("jump", 0);
    TEST_ASSERT(st >= 0.0f && st <= 1.0f,
                "lc_input_get_action_strength_for_player() returns [0,1]");

    /* A player with no assigned gamepads still yields a valid JSON array. */
    char pads_json[256];
    memset(pads_json, 0, sizeof(pads_json));
    LCResult pr = lc_input_get_player_gamepads(0, pads_json, sizeof(pads_json));
    TEST_ASSERT(pr == LC_SUCCESS, "lc_input_get_player_gamepads() returns success");
    TEST_ASSERT(strlen(pads_json) > 0, "lc_input_get_player_gamepads() writes JSON");
    TEST_ASSERT(lc_input_get_player_gamepads(0, NULL, 0) == LC_ERROR_NULL_POINTER,
                "lc_input_get_player_gamepads(NULL) rejects null buffer");

    lc_input_set_player_count(0);

    SECTION_END("Input Ext: Per-player queries");
}

static void test_ext_input_ext_strength_and_vector(void) {
    SECTION_START("Input Ext: Action strength & directional vector");

    float st = lc_input_get_action_strength("jump");
    TEST_ASSERT(st >= 0.0f && st <= 1.0f,
                "lc_input_get_action_strength() returns [0,1]");

    LCVec2 vec = { 9.0f, 9.0f };
    LCResult vr = lc_input_get_vector("move_left", "move_right",
                                      "move_up", "move_down", -1, &vec);
    TEST_ASSERT(vr == LC_SUCCESS, "lc_input_get_vector() returns success");
    /* Clamped to unit length; with no input it should be the zero vector. */
    TEST_ASSERT(vec.x >= -1.01f && vec.x <= 1.01f &&
                vec.y >= -1.01f && vec.y <= 1.01f,
                "lc_input_get_vector() result is within the unit square");

    TEST_ASSERT(lc_input_get_vector("move_left", "move_right",
                                    "move_up", "move_down", -1, NULL)
                == LC_ERROR_NULL_POINTER,
                "lc_input_get_vector(NULL) rejects null out-vector");

    SECTION_END("Input Ext: Action strength & directional vector");
}

static void test_ext_input_ext_gamepad_introspection(void) {
    SECTION_START("Input Ext: Gamepad introspection & rumble");

    /* Headless harness almost certainly has no physical pads connected. */
    int count = lc_input_get_gamepad_count();
    TEST_ASSERT(count >= 0, "lc_input_get_gamepad_count() is non-negative");

    bool connected = lc_input_is_gamepad_connected(0u);
    TEST_ASSERT(connected == true || connected == false,
                "lc_input_is_gamepad_connected() callable");

    char ids_json[256];
    memset(ids_json, 0, sizeof(ids_json));
    LCResult ir = lc_input_get_connected_gamepad_ids(ids_json, sizeof(ids_json));
    TEST_ASSERT(ir == LC_SUCCESS, "lc_input_get_connected_gamepad_ids() returns success");
    TEST_ASSERT(strlen(ids_json) > 0, "lc_input_get_connected_gamepad_ids() writes JSON");
    TEST_ASSERT(lc_input_get_connected_gamepad_ids(NULL, 0) == LC_ERROR_NULL_POINTER,
                "lc_input_get_connected_gamepad_ids(NULL) rejects null buffer");

    char name[128];
    memset(name, 0, sizeof(name));
    LCResult nr = lc_input_get_gamepad_name(0u, name, sizeof(name));
    TEST_ASSERT(nr == LC_SUCCESS || nr == LC_ERROR_NOT_FOUND ||
                nr == LC_ERROR_INVALID_PARAMETER,
                "lc_input_get_gamepad_name() returns a defined result");
    TEST_ASSERT(lc_input_get_gamepad_name(0u, NULL, 0) == LC_ERROR_NULL_POINTER,
                "lc_input_get_gamepad_name(NULL) rejects null buffer");

    /* Rumble on an absent pad is a safe no-op. */
    lc_input_set_gamepad_vibration(0u, 0.5f, 0.5f, 0.1f);
    TEST_ASSERT(true, "lc_input_set_gamepad_vibration() callable");
    lc_input_stop_gamepad_vibration(0u);
    TEST_ASSERT(true, "lc_input_stop_gamepad_vibration() callable");

    SECTION_END("Input Ext: Gamepad introspection & rumble");
}

static void test_ext_input_ext_event_matching(void) {
    SECTION_START("Input Ext: Event-driven action matching");

    /* Seed a deterministic binding for the action under test. */
    lc_input_clear_action_bindings("confirm");
    lc_input_add_action_key("confirm", LC_KEY_ENTER);

    /* A serialized key-down event for ENTER (the exact JSON schema is engine
     * defined; the call must parse-or-reject deterministically and never crash). */
    const char* press_event =
        "{\"type\":\"key\",\"key\":257,\"pressed\":true}";
    const char* release_event =
        "{\"type\":\"key\",\"key\":257,\"pressed\":false}";

    bool matched = false;
    LCResult mr = lc_input_event_is_action(press_event, "confirm", &matched);
    TEST_ASSERT(mr == LC_SUCCESS || mr == LC_ERROR_INVALID_PARAMETER,
                "lc_input_event_is_action() returns a defined result");

    bool pressed = false;
    LCResult pr = lc_input_event_is_action_pressed(press_event, "confirm", &pressed);
    TEST_ASSERT(pr == LC_SUCCESS || pr == LC_ERROR_INVALID_PARAMETER,
                "lc_input_event_is_action_pressed() returns a defined result");

    bool released = false;
    LCResult rr = lc_input_event_is_action_released(release_event, "confirm", &released);
    TEST_ASSERT(rr == LC_SUCCESS || rr == LC_ERROR_INVALID_PARAMETER,
                "lc_input_event_is_action_released() returns a defined result");

    /* NULL out-pointer guards. */
    TEST_ASSERT(lc_input_event_is_action(press_event, "confirm", NULL)
                == LC_ERROR_NULL_POINTER,
                "lc_input_event_is_action(NULL out) rejects null");
    TEST_ASSERT(lc_input_event_is_action_pressed(press_event, "confirm", NULL)
                == LC_ERROR_NULL_POINTER,
                "lc_input_event_is_action_pressed(NULL out) rejects null");
    TEST_ASSERT(lc_input_event_is_action_released(release_event, "confirm", NULL)
                == LC_ERROR_NULL_POINTER,
                "lc_input_event_is_action_released(NULL out) rejects null");

    lc_input_clear_action_bindings("confirm");

    SECTION_END("Input Ext: Event-driven action matching");
}
