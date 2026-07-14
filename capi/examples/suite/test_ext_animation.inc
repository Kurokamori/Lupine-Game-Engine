/* ============================================================================
 * AnimationPlayer C API Tests
 * ============================================================================ */

static void test_ext_animation_player(void) {
    SECTION_START("Ext Animation: AnimationPlayer");

    LCComponentHandle player = NULL;
    LCResult result = lc_animation_player_create("TestAnimationPlayer", &player);
    TEST_RESULT(result, "Create AnimationPlayer");
    TEST_ASSERT(player != NULL, "Component handle is valid");

    /* Clip library */
    result = lc_animation_player_add_clip(player, "idle", "res://anims/idle.animclip");
    TEST_RESULT(result, "Add clip 'idle'");

    result = lc_animation_player_add_clip(player, "walk", "res://anims/walk.animclip");
    TEST_RESULT(result, "Add clip 'walk'");

    result = lc_animation_player_add_clip(player, "run", "res://anims/run.animclip");
    TEST_RESULT(result, "Add clip 'run'");

    bool hasAnim = false;
    result = lc_animation_player_has_animation(player, "idle", &hasAnim);
    TEST_RESULT(result, "Query has_animation 'idle'");
    TEST_ASSERT(hasAnim == true, "Clip 'idle' is registered");

    hasAnim = true;
    result = lc_animation_player_has_animation(player, "does_not_exist", &hasAnim);
    TEST_RESULT(result, "Query has_animation missing clip");
    TEST_ASSERT(hasAnim == false, "Missing clip reports false");

    result = lc_animation_player_remove_clip(player, "run");
    TEST_RESULT(result, "Remove clip 'run'");

    hasAnim = true;
    result = lc_animation_player_has_animation(player, "run", &hasAnim);
    TEST_RESULT(result, "Query has_animation after removal");
    TEST_ASSERT(hasAnim == false, "Removed clip reports false");

    result = lc_animation_player_reload_clips(player);
    TEST_RESULT(result, "Reload clips");

    /* Speed round-trip */
    result = lc_animation_player_set_speed(player, 2.5f);
    TEST_RESULT(result, "Set speed");

    float speed = 0.0f;
    result = lc_animation_player_get_speed(player, &speed);
    TEST_RESULT(result, "Get speed");
    TEST_ASSERT(speed > 2.49f && speed < 2.51f, "Speed round-trips");

    result = lc_animation_player_set_speed(player, 1.0f);
    TEST_RESULT(result, "Restore speed");

    result = lc_animation_player_get_speed(player, &speed);
    TEST_RESULT(result, "Get speed after restore");
    TEST_ASSERT(speed > 0.99f && speed < 1.01f, "Restored speed round-trips");

    /* Default blend / auto play / root node */
    result = lc_animation_player_set_default_blend_time(player, 0.25f);
    TEST_RESULT(result, "Set default blend time");

    result = lc_animation_player_set_auto_play(player, "idle");
    TEST_RESULT(result, "Set auto play clip");

    result = lc_animation_player_set_root_node(player, "..");
    TEST_RESULT(result, "Set root node path");

    /* Playback control with state verification */
    result = lc_animation_player_play(player, "idle");
    TEST_RESULT(result, "Play 'idle'");

    bool playing = false;
    result = lc_animation_player_is_playing(player, &playing);
    TEST_RESULT(result, "Query is_playing after play");
    TEST_ASSERT(playing == true, "Player reports playing after play");

    char current[256];
    current[0] = '\0';
    result = lc_animation_player_get_current_animation(player, current, (int)sizeof(current));
    TEST_RESULT(result, "Get current animation after play");
    TEST_ASSERT(strcmp(current, "idle") == 0, "Current animation is 'idle'");

    result = lc_animation_player_pause(player);
    TEST_RESULT(result, "Pause playback");

    playing = true;
    result = lc_animation_player_is_playing(player, &playing);
    TEST_RESULT(result, "Query is_playing after pause");
    TEST_ASSERT(playing == false, "Player reports not playing after pause");

    result = lc_animation_player_resume(player);
    TEST_RESULT(result, "Resume playback");

    playing = false;
    result = lc_animation_player_is_playing(player, &playing);
    TEST_RESULT(result, "Query is_playing after resume");
    TEST_ASSERT(playing == true, "Player reports playing after resume");

    /* Blend / backwards playback */
    result = lc_animation_player_play_blend(player, "walk", 0.3f);
    TEST_RESULT(result, "Play 'walk' with blend");

    current[0] = '\0';
    result = lc_animation_player_get_current_animation(player, current, (int)sizeof(current));
    TEST_RESULT(result, "Get current animation after blend");
    TEST_ASSERT(strcmp(current, "walk") == 0, "Current animation is 'walk'");

    result = lc_animation_player_play_backwards(player, "walk");
    TEST_RESULT(result, "Play 'walk' backwards");

    /* Seek */
    result = lc_animation_player_seek(player, 0.5f, true);
    TEST_RESULT(result, "Seek to 0.5s with update");

    result = lc_animation_player_seek(player, 0.0f, false);
    TEST_RESULT(result, "Seek to 0.0s without update");

    /* Sample */
    result = lc_animation_player_sample_at(player, "idle", 0.1f);
    TEST_RESULT(result, "Sample 'idle' at 0.1s");

    /* Queue */
    result = lc_animation_player_queue(player, "idle");
    TEST_RESULT(result, "Queue 'idle'");

    result = lc_animation_player_queue(player, "walk");
    TEST_RESULT(result, "Queue 'walk'");

    result = lc_animation_player_clear_queue(player);
    TEST_RESULT(result, "Clear queue");

    /* Stop with reset */
    result = lc_animation_player_stop(player, true);
    TEST_RESULT(result, "Stop with reset");

    playing = true;
    result = lc_animation_player_is_playing(player, &playing);
    TEST_RESULT(result, "Query is_playing after stop");
    TEST_ASSERT(playing == false, "Player reports not playing after stop");

    lc_component_destroy(player);

    SECTION_END("Ext Animation: AnimationPlayer");
}

/* ============================================================================
 * AnimationTree C API Tests
 * ============================================================================ */

static void test_ext_animation_tree(void) {
    SECTION_START("Ext Animation: AnimationTree");

    LCComponentHandle tree = NULL;
    LCResult result = lc_animation_tree_create("TestAnimationTree", &tree);
    TEST_RESULT(result, "Create AnimationTree");
    TEST_ASSERT(tree != NULL, "Component handle is valid");

    /* Configuration */
    result = lc_animation_tree_set_graph_path(tree, "res://anims/locomotion.animtree");
    TEST_RESULT(result, "Set graph path");

    result = lc_animation_tree_set_animation_player(tree, "../AnimationPlayer");
    TEST_RESULT(result, "Set animation player path");

    result = lc_animation_tree_set_root_node(tree, "..");
    TEST_RESULT(result, "Set root node path");

    result = lc_animation_tree_reload_graph(tree);
    TEST_RESULT(result, "Reload graph");

    /* Active toggle round-trip */
    result = lc_animation_tree_set_active(tree, true);
    TEST_RESULT(result, "Set active true");

    bool active = false;
    result = lc_animation_tree_is_active(tree, &active);
    TEST_RESULT(result, "Query is_active after enable");
    TEST_ASSERT(active == true, "Tree reports active");

    result = lc_animation_tree_set_active(tree, false);
    TEST_RESULT(result, "Set active false");

    active = true;
    result = lc_animation_tree_is_active(tree, &active);
    TEST_RESULT(result, "Query is_active after disable");
    TEST_ASSERT(active == false, "Tree reports inactive");

    result = lc_animation_tree_set_active(tree, true);
    TEST_RESULT(result, "Re-enable active for parameter tests");

    /* Float parameter round-trip */
    result = lc_animation_tree_set_float(tree, "speed", 3.75f);
    TEST_RESULT(result, "Set float parameter 'speed'");

    float floatValue = 0.0f;
    result = lc_animation_tree_get_float(tree, "speed", &floatValue);
    TEST_RESULT(result, "Get float parameter 'speed'");
    TEST_ASSERT(floatValue > 3.74f && floatValue < 3.76f, "Float parameter round-trips");

    /* Int parameter round-trip */
    result = lc_animation_tree_set_int(tree, "variant", 7);
    TEST_RESULT(result, "Set int parameter 'variant'");

    int intValue = 0;
    result = lc_animation_tree_get_int(tree, "variant", &intValue);
    TEST_RESULT(result, "Get int parameter 'variant'");
    TEST_ASSERT(intValue == 7, "Int parameter round-trips");

    /* Bool parameter round-trip */
    result = lc_animation_tree_set_bool(tree, "grounded", true);
    TEST_RESULT(result, "Set bool parameter 'grounded' true");

    bool boolValue = false;
    result = lc_animation_tree_get_bool(tree, "grounded", &boolValue);
    TEST_RESULT(result, "Get bool parameter 'grounded'");
    TEST_ASSERT(boolValue == true, "Bool parameter round-trips true");

    result = lc_animation_tree_set_bool(tree, "grounded", false);
    TEST_RESULT(result, "Set bool parameter 'grounded' false");

    boolValue = true;
    result = lc_animation_tree_get_bool(tree, "grounded", &boolValue);
    TEST_RESULT(result, "Get bool parameter 'grounded' after toggle");
    TEST_ASSERT(boolValue == false, "Bool parameter round-trips false");

    /* Trigger parameter */
    result = lc_animation_tree_set_trigger(tree, "jump");
    TEST_RESULT(result, "Set trigger 'jump'");

    /* State machine */
    result = lc_animation_tree_travel(tree, "Walk", 0);
    TEST_RESULT(result, "Travel to 'Walk' on layer 0");

    char state[256];
    state[0] = '\0';
    result = lc_animation_tree_get_current_state(tree, 0, state, (int)sizeof(state));
    TEST_RESULT(result, "Get current state on layer 0");
    TEST_ASSERT(state != NULL, "Current state buffer is populated");

    lc_component_destroy(tree);

    SECTION_END("Ext Animation: AnimationTree");
}
