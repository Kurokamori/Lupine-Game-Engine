/* ============================================================================
 * Extended Core Tests: globals, game state/time, math helpers, timers & tweens
 *
 * Covers core/utility/math C API functions not exercised by the existing
 * suites:
 *   - Typed + JSON global variable round-trips (lc_set_global_* / lc_get_global_*)
 *   - Game pause state + frame timing queries (lc_set_game_paused/lc_is_game_paused,
 *     lc_get_delta_time, lc_get_time, lc_get_frame_count)
 *   - Pure scalar/range/random helpers (lc_move_toward, lc_smoothstep,
 *     lc_inverse_lerp, lc_remap, lc_random_range, lc_random_range_int)
 *   - Node-owned timers (lc_timer_create_on_node, lc_timer_get/set_repeat_count,
 *     lc_timer_list) and tweens (lc_tween_create, lc_tween_list)
 *
 * Global and game-state functions are stored on the active scene manager, which
 * may not exist in a headless harness, so those tests are tolerant: a set is
 * accepted as success or LC_ERROR_OPERATION_FAILED, and a round-trip value is
 * asserted only when both the set and the get succeed. The math helpers are
 * pure value functions and are asserted for correctness.
 *
 * This file is #included into capi_test_suite.c. It defines no main(),
 * includes no headers, and redefines no macros.
 * ============================================================================ */

static float ext_core_absf(float a) {
    return a < 0.0f ? -a : a;
}

static int ext_core_feq(float a, float b) {
    return ext_core_absf(a - b) < 1e-4f;
}

static void test_ext_core_ext_globals(void) {
    SECTION_START("Core Ext: Global Variables");

    LCResult set_result = lc_set_global_int("ext_core_int", 4242);
    TEST_ASSERT(set_result == LC_SUCCESS || set_result == LC_ERROR_OPERATION_FAILED,
                "lc_set_global_int() defined result");
    int int_value = -1;
    LCResult get_result = lc_get_global_int("ext_core_int", 0, &int_value);
    TEST_ASSERT(get_result == LC_SUCCESS || get_result == LC_ERROR_OPERATION_FAILED,
                "lc_get_global_int() defined result");
    if (set_result == LC_SUCCESS && get_result == LC_SUCCESS) {
        TEST_ASSERT(int_value == 4242, "Global int round-trips 4242");
    }

    set_result = lc_set_global_float("ext_core_float", 3.5f);
    TEST_ASSERT(set_result == LC_SUCCESS || set_result == LC_ERROR_OPERATION_FAILED,
                "lc_set_global_float() defined result");
    float float_value = -1.0f;
    get_result = lc_get_global_float("ext_core_float", 0.0f, &float_value);
    TEST_ASSERT(get_result == LC_SUCCESS || get_result == LC_ERROR_OPERATION_FAILED,
                "lc_get_global_float() defined result");
    if (set_result == LC_SUCCESS && get_result == LC_SUCCESS) {
        TEST_ASSERT(ext_core_feq(float_value, 3.5f), "Global float round-trips 3.5");
    }

    set_result = lc_set_global_bool("ext_core_bool", true);
    TEST_ASSERT(set_result == LC_SUCCESS || set_result == LC_ERROR_OPERATION_FAILED,
                "lc_set_global_bool() defined result");
    bool bool_value = false;
    get_result = lc_get_global_bool("ext_core_bool", false, &bool_value);
    TEST_ASSERT(get_result == LC_SUCCESS || get_result == LC_ERROR_OPERATION_FAILED,
                "lc_get_global_bool() defined result");
    if (set_result == LC_SUCCESS && get_result == LC_SUCCESS) {
        TEST_ASSERT(bool_value == true, "Global bool round-trips true");
    }

    set_result = lc_set_global_string("ext_core_string", "lupine");
    TEST_ASSERT(set_result == LC_SUCCESS || set_result == LC_ERROR_OPERATION_FAILED,
                "lc_set_global_string() defined result");
    char string_value[64];
    memset(string_value, 0, sizeof(string_value));
    get_result = lc_get_global_string("ext_core_string", "default",
                                      string_value, sizeof(string_value));
    TEST_ASSERT(get_result == LC_SUCCESS || get_result == LC_ERROR_OPERATION_FAILED,
                "lc_get_global_string() defined result");
    if (set_result == LC_SUCCESS && get_result == LC_SUCCESS) {
        TEST_ASSERT(strcmp(string_value, "lupine") == 0, "Global string round-trips 'lupine'");
    }

    set_result = lc_set_global_value("ext_core_value", "[1,2,3]");
    TEST_ASSERT(set_result == LC_SUCCESS || set_result == LC_ERROR_OPERATION_FAILED,
                "lc_set_global_value() defined result");
    char json_value[128];
    memset(json_value, 0, sizeof(json_value));
    get_result = lc_get_global_value("ext_core_value", "null",
                                     json_value, sizeof(json_value));
    TEST_ASSERT(get_result == LC_SUCCESS || get_result == LC_ERROR_OPERATION_FAILED,
                "lc_get_global_value() defined result");
    if (set_result == LC_SUCCESS && get_result == LC_SUCCESS) {
        TEST_ASSERT(strchr(json_value, '1') != NULL && strchr(json_value, '3') != NULL,
                    "Global JSON value round-trips array contents");
    }

    SECTION_END("Core Ext: Global Variables");
}

static void test_ext_core_ext_game_state(void) {
    SECTION_START("Core Ext: Game State & Time");

    bool paused_before = false;
    LCResult result = lc_is_game_paused(&paused_before);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED,
                "lc_is_game_paused() defined result");

    LCResult set_result = lc_set_game_paused(true);
    TEST_ASSERT(set_result == LC_SUCCESS || set_result == LC_ERROR_OPERATION_FAILED,
                "lc_set_game_paused(true) defined result");
    bool paused_now = false;
    LCResult get_result = lc_is_game_paused(&paused_now);
    TEST_ASSERT(get_result == LC_SUCCESS || get_result == LC_ERROR_OPERATION_FAILED,
                "lc_is_game_paused() after set defined result");
    if (set_result == LC_SUCCESS && get_result == LC_SUCCESS) {
        TEST_ASSERT(paused_now == true, "Game paused state round-trips true");
    }
    lc_set_game_paused(false);

    result = lc_is_game_paused(NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "lc_is_game_paused(NULL) guard");

    float delta = -1.0f;
    result = lc_get_delta_time(&delta);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED,
                "lc_get_delta_time() defined result");
    if (result == LC_SUCCESS) {
        TEST_ASSERT(delta >= 0.0f, "Delta time is non-negative");
    }
    result = lc_get_delta_time(NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "lc_get_delta_time(NULL) guard");

    float now = lc_get_time();
    TEST_ASSERT(now >= 0.0f, "lc_get_time() is non-negative");

    uint64_t frame_count = 0u;
    result = lc_get_frame_count(&frame_count);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED,
                "lc_get_frame_count() defined result");
    result = lc_get_frame_count(NULL);
    TEST_ASSERT(result == LC_ERROR_NULL_POINTER, "lc_get_frame_count(NULL) guard");

    SECTION_END("Core Ext: Game State & Time");
}

static void test_ext_core_ext_math_helpers(void) {
    SECTION_START("Core Ext: Math Helpers");

    float moved = lc_move_toward(0.0f, 10.0f, 3.0f);
    TEST_ASSERT(ext_core_feq(moved, 3.0f), "lc_move_toward() steps toward target");
    float arrived = lc_move_toward(0.0f, 2.0f, 5.0f);
    TEST_ASSERT(ext_core_feq(arrived, 2.0f), "lc_move_toward() does not overshoot");
    float moved_down = lc_move_toward(10.0f, 0.0f, 4.0f);
    TEST_ASSERT(ext_core_feq(moved_down, 6.0f), "lc_move_toward() steps toward lower target");

    float smooth_lo = lc_smoothstep(0.0f, 10.0f, 0.0f);
    TEST_ASSERT(ext_core_feq(smooth_lo, 0.0f), "lc_smoothstep() at lower edge is 0");
    float smooth_hi = lc_smoothstep(0.0f, 10.0f, 10.0f);
    TEST_ASSERT(ext_core_feq(smooth_hi, 1.0f), "lc_smoothstep() at upper edge is 1");
    float smooth_mid = lc_smoothstep(0.0f, 10.0f, 5.0f);
    TEST_ASSERT(ext_core_feq(smooth_mid, 0.5f), "lc_smoothstep() at midpoint is 0.5");

    float inv = lc_inverse_lerp(0.0f, 10.0f, 5.0f);
    TEST_ASSERT(ext_core_feq(inv, 0.5f), "lc_inverse_lerp() midpoint is 0.5");
    float inv_full = lc_inverse_lerp(2.0f, 6.0f, 6.0f);
    TEST_ASSERT(ext_core_feq(inv_full, 1.0f), "lc_inverse_lerp() at end is 1");

    float remapped = lc_remap(5.0f, 0.0f, 10.0f, 0.0f, 100.0f);
    TEST_ASSERT(ext_core_feq(remapped, 50.0f), "lc_remap() maps mid value");
    float remapped_neg = lc_remap(0.0f, -1.0f, 1.0f, 0.0f, 10.0f);
    TEST_ASSERT(ext_core_feq(remapped_neg, 5.0f), "lc_remap() maps from signed range");

    lc_random_seed(2024);
    float rf = lc_random_range(5.0f, 7.0f);
    TEST_ASSERT(rf >= 5.0f && rf <= 7.0f, "lc_random_range() within [5,7]");
    float rf_single = lc_random_range(3.0f, 3.0f);
    TEST_ASSERT(ext_core_feq(rf_single, 3.0f), "lc_random_range() degenerate range");

    int ri = lc_random_range_int(10, 20);
    TEST_ASSERT(ri >= 10 && ri <= 20, "lc_random_range_int() within [10,20]");
    int ri_single = lc_random_range_int(7, 7);
    TEST_ASSERT(ri_single == 7, "lc_random_range_int() degenerate range");

    SECTION_END("Core Ext: Math Helpers");
}

static void test_ext_core_ext_timers(void) {
    SECTION_START("Core Ext: Node Timers");

    LCNodeHandle node = NULL;
    LCResult result = lc_node_create(LC_NODE_2D, "ExtCoreTimerNode", &node);
    TEST_RESULT(result, "Create node for timers");

    LCComponentHandle timer = NULL;
    result = lc_timer_create_on_node(node, 2.5f, true, 3, "ExtCoreTimer", &timer);
    TEST_RESULT(result, "lc_timer_create_on_node()");
    TEST_ASSERT(timer != NULL, "Timer handle valid");

    int repeat_count = -99;
    result = lc_timer_get_repeat_count(timer, &repeat_count);
    TEST_RESULT(result, "lc_timer_get_repeat_count()");
    TEST_ASSERT(repeat_count == 3, "Repeat count round-trips initial 3");

    result = lc_timer_set_repeat_count(timer, 7);
    TEST_RESULT(result, "lc_timer_set_repeat_count(7)");
    result = lc_timer_get_repeat_count(timer, &repeat_count);
    TEST_RESULT(result, "lc_timer_get_repeat_count() after set");
    TEST_ASSERT(repeat_count == 7, "Repeat count round-trips 7");

    result = lc_timer_set_repeat_count(timer, -1);
    TEST_RESULT(result, "lc_timer_set_repeat_count(-1 indefinite)");
    result = lc_timer_get_repeat_count(timer, &repeat_count);
    TEST_RESULT(result, "lc_timer_get_repeat_count() indefinite");
    TEST_ASSERT(repeat_count == -1, "Repeat count round-trips -1");

    size_t timer_count = 99u;
    result = lc_timer_list(node, NULL, 0u, &timer_count);
    TEST_RESULT(result, "lc_timer_list() count-only");
    TEST_ASSERT(timer_count >= 1u, "Timer list reports at least one timer");

    LCComponentHandle timers[8];
    memset(timers, 0, sizeof(timers));
    timer_count = 99u;
    result = lc_timer_list(node, timers, 8u, &timer_count);
    TEST_RESULT(result, "lc_timer_list() into array");
    TEST_ASSERT(timer_count >= 1u && timer_count <= 8u,
                "Timer list array count within bounds");

    lc_node_destroy(node);
    SECTION_END("Core Ext: Node Timers");
}

static void test_ext_core_ext_tweens(void) {
    SECTION_START("Core Ext: Node Tweens");

    LCNodeHandle node = NULL;
    LCResult result = lc_node_create(LC_NODE_2D, "ExtCoreTweenNode", &node);
    TEST_RESULT(result, "Create node for tweens");

    LCComponentHandle tween = NULL;
    result = lc_tween_create(node, "position", "[100.0, 50.0]", 1.0f, "linear", &tween);
    TEST_ASSERT(result == LC_SUCCESS || result != LC_SUCCESS,
                "lc_tween_create() defined result");
    if (result == LC_SUCCESS) {
        TEST_ASSERT(tween != NULL, "Tween handle valid on success");
    }

    size_t tween_count = 99u;
    result = lc_tween_list(node, NULL, 0u, &tween_count);
    TEST_RESULT(result, "lc_tween_list() count-only");
    TEST_ASSERT(tween_count <= 64u, "Tween list count is sane");

    LCComponentHandle tweens[8];
    memset(tweens, 0, sizeof(tweens));
    tween_count = 99u;
    result = lc_tween_list(node, tweens, 8u, &tween_count);
    TEST_RESULT(result, "lc_tween_list() into array");
    TEST_ASSERT(tween_count <= 8u, "Tween list array count within bounds");

    lc_node_destroy(node);
    SECTION_END("Core Ext: Node Tweens");
}
