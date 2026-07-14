/* Extended input C API tests for the cross-device input additions in
 * capi/include/input/lc_input.h:
 *   - active device detection
 *   - input contexts / action sets
 *   - local-multiplayer player slots
 *   - runtime rebinding + capture
 *   - glyph / prompt resolution
 *   - native action-callback delegation
 *
 * These run headless: with no device events pumped, queries return defined
 * values and round-trippable setters/getters agree on cached state. Rebinding
 * mutates the live input map (an action is created up front), so the binding
 * add/count/get/clear path is verified directly. */

static int g_input_cb_fired = 0;

static void input_test_action_cb(const char* action_name, bool pressed, void* user_data) {
    (void)action_name;
    (void)pressed;
    if (user_data) {
        *(int*)user_data += 1;
    }
    g_input_cb_fired += 1;
}

static void test_ext_input_device(void) {
    SECTION_START("Input: Active device detection");

    LCInputDeviceType dev = lc_input_get_active_device_type();
    TEST_ASSERT(dev >= LC_INPUT_DEVICE_KEYBOARD && dev <= LC_INPUT_DEVICE_UNKNOWN,
                "lc_input_get_active_device_type() returns a defined device");

    uint32_t lastPad = lc_input_get_last_gamepad_id();
    TEST_ASSERT(lastPad == lastPad, "lc_input_get_last_gamepad_id() callable");

    LCGamepadType gt = lc_input_get_gamepad_type(0);
    TEST_ASSERT(gt >= LC_GAMEPAD_TYPE_UNKNOWN && gt <= LC_GAMEPAD_TYPE_GENERIC,
                "lc_input_get_gamepad_type() returns a defined family");

    SECTION_END("Input: Active device detection");
}

static void test_ext_input_contexts(void) {
    SECTION_START("Input: Contexts / action sets");

    lc_input_enable_context("gameplay");
    TEST_ASSERT(lc_input_is_context_active("gameplay") == true,
                "lc_input_enable_context() activates a context");

    lc_input_disable_context("gameplay");
    TEST_ASSERT(lc_input_is_context_active("gameplay") == false,
                "lc_input_disable_context() deactivates a context");

    TEST_ASSERT(lc_input_is_context_active("") == true,
                "the empty context is always active");

    lc_input_enable_context("menu");
    lc_input_set_exclusive_context("vehicle");
    TEST_ASSERT(lc_input_is_context_active("vehicle") == true &&
                lc_input_is_context_active("menu") == false,
                "lc_input_set_exclusive_context() activates only one context");

    lc_input_set_context_active("vehicle", false);
    TEST_ASSERT(lc_input_is_context_active("vehicle") == false,
                "lc_input_set_context_active(false) deactivates");

    SECTION_END("Input: Contexts / action sets");
}

static void test_ext_input_players(void) {
    SECTION_START("Input: Local multiplayer player slots");

    lc_input_set_player_count(2);
    TEST_ASSERT(lc_input_get_player_count() == 2, "lc_input_set/get_player_count() round-trips");

    lc_input_assign_keyboard_mouse_to_player(0);
    TEST_ASSERT(lc_input_player_owns_keyboard_mouse(0) == true,
                "lc_input_assign_keyboard_mouse_to_player() assigns KBM");
    TEST_ASSERT(lc_input_get_player_for_keyboard_mouse() == 0,
                "lc_input_get_player_for_keyboard_mouse() returns the owner");

    lc_input_assign_gamepad_to_player(1, 5);
    TEST_ASSERT(lc_input_get_player_for_gamepad(5) == 1,
                "lc_input_assign_gamepad_to_player() assigns gamepad to a player");

    lc_input_unassign_gamepad(5);
    TEST_ASSERT(lc_input_get_player_for_gamepad(5) == -1,
                "lc_input_unassign_gamepad() removes the assignment");

    lc_input_set_auto_join_enabled(true);
    TEST_ASSERT(lc_input_is_auto_join_enabled() == true,
                "lc_input_set/is_auto_join_enabled() round-trips");
    lc_input_set_auto_join_enabled(false);

    lc_input_clear_player_assignments();
    TEST_ASSERT(lc_input_player_owns_keyboard_mouse(0) == false,
                "lc_input_clear_player_assignments() clears KBM ownership");

    lc_input_set_player_count(0);

    SECTION_END("Input: Local multiplayer player slots");
}

static void test_ext_input_rebinding(void) {
    SECTION_START("Input: Runtime rebinding + capture");

    /* The default input map ships a "jump" action; rebuild a known binding set. */
    lc_input_clear_action_bindings("jump");
    TEST_ASSERT(lc_input_get_action_binding_count("jump") == 0,
                "lc_input_clear_action_bindings() empties the action");

    lc_input_add_action_key("jump", LC_KEY_SPACE);
    lc_input_add_action_gamepad_button("jump", LC_GAMEPAD_BUTTON_A, 0);
    TEST_ASSERT(lc_input_get_action_binding_count("jump") == 2,
                "lc_input_add_action_*() appends bindings");

    LCInputBinding b;
    memset(&b, 0, sizeof(b));
    LCResult gb = lc_input_get_action_binding("jump", 0, &b);
    TEST_ASSERT(gb == LC_SUCCESS && b.device_type == LC_INPUT_DEVICE_KEYBOARD &&
                b.key_code == LC_KEY_SPACE,
                "lc_input_get_action_binding() returns the keyboard binding");

    lc_input_remove_action_binding("jump", 0);
    TEST_ASSERT(lc_input_get_action_binding_count("jump") == 1,
                "lc_input_remove_action_binding() removes one binding");

    TEST_ASSERT(lc_input_get_action_binding("jump", 9, &b) == LC_ERROR_INVALID_PARAMETER,
                "lc_input_get_action_binding() rejects out-of-range index");
    TEST_ASSERT(lc_input_get_action_binding("jump", 0, NULL) == LC_ERROR_NULL_POINTER,
                "lc_input_get_action_binding(NULL) rejects null");

    /* Capture: nothing pumped headless, so no binding is captured yet. */
    lc_input_start_input_capture(LC_CAPTURE_ALL);
    TEST_ASSERT(lc_input_is_capturing() == true, "lc_input_start_input_capture() begins capture");
    TEST_ASSERT(lc_input_is_capture_complete() == false,
                "no capture completes without pumped input");
    lc_input_cancel_input_capture();
    TEST_ASSERT(lc_input_is_capturing() == false, "lc_input_cancel_input_capture() ends capture");

    SECTION_END("Input: Runtime rebinding + capture");
}

static void test_ext_input_glyphs(void) {
    SECTION_START("Input: Glyph / prompt resolution");

    /* Ensure a known keyboard binding so the glyph resolves deterministically. */
    lc_input_clear_action_bindings("interact");
    lc_input_add_action_key("interact", LC_KEY_E);

    LCInputGlyph glyph;
    memset(&glyph, 0, sizeof(glyph));
    LCResult gr = lc_input_get_action_glyph("interact", -1, LC_INPUT_DEVICE_KEYBOARD, &glyph);
    TEST_ASSERT(gr == LC_SUCCESS, "lc_input_get_action_glyph() returns success");
    TEST_ASSERT(strlen(glyph.glyph_id) > 0, "glyph has a non-empty glyph_id");
    TEST_ASSERT(strcmp(glyph.glyph_id, "key_e") == 0, "keyboard glyph id is 'key_e'");

    lc_input_set_glyph_label("key_e", "Interact Key");
    memset(&glyph, 0, sizeof(glyph));
    lc_input_get_action_glyph("interact", -1, LC_INPUT_DEVICE_KEYBOARD, &glyph);
    TEST_ASSERT(strcmp(glyph.label, "Interact Key") == 0,
                "lc_input_set_glyph_label() overrides the resolved label");

    lc_input_set_glyph_art("key_e", "res://glyphs/e.png");
    memset(&glyph, 0, sizeof(glyph));
    lc_input_get_action_glyph("interact", -1, LC_INPUT_DEVICE_KEYBOARD, &glyph);
    TEST_ASSERT(strcmp(glyph.art_path, "res://glyphs/e.png") == 0,
                "lc_input_set_glyph_art() supplies the art path");

    lc_input_clear_glyph_override("key_e");
    memset(&glyph, 0, sizeof(glyph));
    lc_input_get_action_glyph("interact", -1, LC_INPUT_DEVICE_KEYBOARD, &glyph);
    TEST_ASSERT(strcmp(glyph.label, "Interact Key") != 0,
                "lc_input_clear_glyph_override() restores the default label");

    TEST_ASSERT(lc_input_get_action_glyph(NULL, -1, -1, &glyph) == LC_ERROR_NULL_POINTER,
                "lc_input_get_action_glyph(NULL) rejects null");

    SECTION_END("Input: Glyph / prompt resolution");
}

static void test_ext_input_delegation(void) {
    SECTION_START("Input: Action callback delegation");

    int local = 0;
    g_input_cb_fired = 0;
    lc_input_register_action_callback("jump", input_test_action_cb, &local);
    TEST_ASSERT(true, "lc_input_register_action_callback() callable");

    lc_input_unregister_action_callback("jump");
    TEST_ASSERT(true, "lc_input_unregister_action_callback() callable");

    /* No frame is pumped headless, so the callback should not have fired. */
    TEST_ASSERT(g_input_cb_fired == 0 && local == 0,
                "action callback does not fire without an input transition");

    SECTION_END("Input: Action callback delegation");
}
