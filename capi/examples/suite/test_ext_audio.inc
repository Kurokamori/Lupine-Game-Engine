/* Extended audio C API tests (capi/include/audio/lc_audio.h).
 * Covers public lc_audio_* functions not exercised by test_audio():
 *   playback control (play/stop/pause/resume/is_playing),
 *   pitch/pan/autoplay getters with round-trip,
 *   audio bus get/set,
 *   3D getters (is3d/min_distance/max_distance/rolloff_factor) round-trip. */

static void test_ext_audio_playback(void) {
    SECTION_START("Ext Audio: Playback Control");

    LCComponentHandle player = NULL;
    LCResult result = lc_audio_player_create("ExtAudioPlayback", &player);
    TEST_RESULT(result, "Create AudioPlayer");

    bool playing = true;
    result = lc_audio_player_is_playing(player, &playing);
    TEST_RESULT(result, "Query is_playing on fresh player");
    TEST_ASSERT(!playing, "Fresh player is not playing");

    result = lc_audio_player_play(player);
    TEST_RESULT(result, "Play audio");

    result = lc_audio_player_pause(player);
    TEST_RESULT(result, "Pause audio");

    result = lc_audio_player_resume(player);
    TEST_RESULT(result, "Resume audio");

    result = lc_audio_player_stop(player);
    TEST_RESULT(result, "Stop audio");

    playing = true;
    result = lc_audio_player_is_playing(player, &playing);
    TEST_RESULT(result, "Query is_playing after stop");
    TEST_ASSERT(!playing, "Player is not playing after stop");

    lc_component_destroy(player);

    SECTION_END("Ext Audio: Playback Control");
}

static void test_ext_audio_params(void) {
    SECTION_START("Ext Audio: Pitch/Pan/Autoplay Getters");

    LCComponentHandle player = NULL;
    LCResult result = lc_audio_player_create("ExtAudioParams", &player);
    TEST_RESULT(result, "Create AudioPlayer");

    result = lc_audio_player_set_pitch(player, 1.5f);
    TEST_RESULT(result, "Set pitch to 1.5");

    float pitch = 0.0f;
    result = lc_audio_player_get_pitch(player, &pitch);
    TEST_RESULT(result, "Get pitch");
    TEST_ASSERT(pitch > 1.49f && pitch < 1.51f, "Pitch round-trip is 1.5");

    result = lc_audio_player_set_pan(player, -0.75f);
    TEST_RESULT(result, "Set pan to -0.75");

    float pan = 0.0f;
    result = lc_audio_player_get_pan(player, &pan);
    TEST_RESULT(result, "Get pan");
    TEST_ASSERT(pan > -0.76f && pan < -0.74f, "Pan round-trip is -0.75");

    result = lc_audio_player_set_autoplay(player, true);
    TEST_RESULT(result, "Enable autoplay");

    bool autoplay = false;
    result = lc_audio_player_get_autoplay(player, &autoplay);
    TEST_RESULT(result, "Get autoplay");
    TEST_ASSERT(autoplay, "Autoplay is enabled");

    result = lc_audio_player_set_autoplay(player, false);
    TEST_RESULT(result, "Disable autoplay (restore)");

    autoplay = true;
    result = lc_audio_player_get_autoplay(player, &autoplay);
    TEST_RESULT(result, "Get autoplay after restore");
    TEST_ASSERT(!autoplay, "Autoplay is disabled after restore");

    lc_component_destroy(player);

    SECTION_END("Ext Audio: Pitch/Pan/Autoplay Getters");
}

static void test_ext_audio_bus(void) {
    SECTION_START("Ext Audio: Audio Bus");

    LCComponentHandle player = NULL;
    LCResult result = lc_audio_player_create("ExtAudioBus", &player);
    TEST_RESULT(result, "Create AudioPlayer");

    result = lc_audio_player_set_bus(player, "SFX");
    TEST_RESULT(result, "Set bus to SFX");

    char bus[64];
    bus[0] = '\0';
    result = lc_audio_player_get_bus(player, bus, sizeof(bus));
    TEST_RESULT(result, "Get bus");
    TEST_ASSERT(strcmp(bus, "SFX") == 0, "Bus round-trip is SFX");

    result = lc_audio_player_set_bus(player, "Music");
    TEST_RESULT(result, "Set bus to Music");

    bus[0] = '\0';
    result = lc_audio_player_get_bus(player, bus, sizeof(bus));
    TEST_RESULT(result, "Get bus after change");
    TEST_ASSERT(strcmp(bus, "Music") == 0, "Bus round-trip is Music");

    lc_component_destroy(player);

    SECTION_END("Ext Audio: Audio Bus");
}

static void test_ext_audio_bus_effects(void) {
    SECTION_START("Ext Audio: Bus DSP Effects");

    lc_audio_create_bus("FXBus", "Master");

    /* Add a reverb effect. */
    int reverbIndex = lc_audio_bus_add_effect("FXBus", "Reverb");
    TEST_ASSERT(reverbIndex == 0, "First effect added at index 0");
    TEST_ASSERT(lc_audio_bus_get_effect_count("FXBus") == 1, "Bus has 1 effect");

    /* Parameter round-trip. */
    lc_audio_bus_set_effect_parameter("FXBus", reverbIndex, "room_size", 0.8f);
    float roomSize = lc_audio_bus_get_effect_parameter("FXBus", reverbIndex, "room_size");
    TEST_ASSERT(roomSize > 0.79f && roomSize < 0.81f, "room_size round-trips");

    /* Enable/disable. */
    TEST_ASSERT(lc_audio_bus_is_effect_enabled("FXBus", reverbIndex) == true, "Effect starts enabled");
    lc_audio_bus_set_effect_enabled("FXBus", reverbIndex, false);
    TEST_ASSERT(lc_audio_bus_is_effect_enabled("FXBus", reverbIndex) == false, "Effect disabled");

    /* Add a second effect and reorder. */
    int lowpassIndex = lc_audio_bus_add_effect("FXBus", "LowPass");
    TEST_ASSERT(lowpassIndex == 1, "Second effect added at index 1");
    TEST_ASSERT(lc_audio_bus_get_effect_count("FXBus") == 2, "Bus has 2 effects");

    lc_audio_bus_move_effect("FXBus", 1, 0);
    TEST_ASSERT(lc_audio_bus_get_effect_count("FXBus") == 2, "Move keeps effect count");

    /* An unknown effect type fails gracefully. */
    int bad = lc_audio_bus_add_effect("FXBus", "NotARealEffect");
    TEST_ASSERT(bad == -1, "Unknown effect type returns -1");

    /* Remove and clear. */
    lc_audio_bus_remove_effect("FXBus", 0);
    TEST_ASSERT(lc_audio_bus_get_effect_count("FXBus") == 1, "Remove drops count to 1");
    lc_audio_bus_clear_effects("FXBus");
    TEST_ASSERT(lc_audio_bus_get_effect_count("FXBus") == 0, "Clear empties the chain");

    /* Live VU levels (headless: silent, so peak is 0 but the calls must be safe). */
    float level = lc_audio_bus_get_level("FXBus");
    TEST_ASSERT(level >= 0.0f, "bus level is non-negative");
    float lvlL = -1.0f, lvlR = -1.0f;
    lc_audio_bus_get_levels("FXBus", &lvlL, &lvlR);
    TEST_ASSERT(lvlL >= 0.0f && lvlR >= 0.0f, "stereo bus levels are non-negative");
    TEST_ASSERT(lc_audio_bus_get_level(NULL) == 0.0f, "get_level(NULL) returns 0");
    lc_audio_bus_get_levels(NULL, &lvlL, &lvlR);
    TEST_ASSERT(lvlL == 0.0f, "get_levels(NULL) yields 0");

    /* Null/robustness. */
    TEST_ASSERT(lc_audio_bus_add_effect(NULL, "Reverb") == -1, "add_effect(NULL) fails gracefully");
    TEST_ASSERT(lc_audio_bus_get_effect_count("NoSuchBus") == 0, "Unknown bus reports 0 effects");

    lc_audio_destroy_bus("FXBus");

    SECTION_END("Ext Audio: Bus DSP Effects");
}

static void test_ext_audio_3d(void) {
    SECTION_START("Ext Audio: 3D Getters");

    LCComponentHandle player = NULL;
    LCResult result = lc_audio_player_create("ExtAudio3D", &player);
    TEST_RESULT(result, "Create AudioPlayer");

    result = lc_audio_player_set_is3d(player, true);
    TEST_RESULT(result, "Enable 3D audio");

    bool is3d = false;
    result = lc_audio_player_get_is3d(player, &is3d);
    TEST_RESULT(result, "Get is3d");
    TEST_ASSERT(is3d, "is3d is enabled");

    result = lc_audio_player_set_is3d(player, false);
    TEST_RESULT(result, "Disable 3D audio (restore)");

    is3d = true;
    result = lc_audio_player_get_is3d(player, &is3d);
    TEST_RESULT(result, "Get is3d after restore");
    TEST_ASSERT(!is3d, "is3d is disabled after restore");

    result = lc_audio_player_set_min_distance(player, 2.5f);
    TEST_RESULT(result, "Set min distance to 2.5");

    float min_distance = 0.0f;
    result = lc_audio_player_get_min_distance(player, &min_distance);
    TEST_RESULT(result, "Get min distance");
    TEST_ASSERT(min_distance > 2.49f && min_distance < 2.51f, "Min distance round-trip is 2.5");

    result = lc_audio_player_set_max_distance(player, 120.0f);
    TEST_RESULT(result, "Set max distance to 120");

    float max_distance = 0.0f;
    result = lc_audio_player_get_max_distance(player, &max_distance);
    TEST_RESULT(result, "Get max distance");
    TEST_ASSERT(max_distance > 119.9f && max_distance < 120.1f, "Max distance round-trip is 120");

    result = lc_audio_player_set_rolloff_factor(player, 2.0f);
    TEST_RESULT(result, "Set rolloff factor to 2.0");

    float rolloff = 0.0f;
    result = lc_audio_player_get_rolloff_factor(player, &rolloff);
    TEST_RESULT(result, "Get rolloff factor");
    TEST_ASSERT(rolloff > 1.99f && rolloff < 2.01f, "Rolloff factor round-trip is 2.0");

    lc_component_destroy(player);

    SECTION_END("Ext Audio: 3D Getters");
}
