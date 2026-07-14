/* Global audio control C API tests (AudioManager mixer surface):
 *  - Master volume / mute, listener placement, source params by UUID, buses.
 *
 * These mirror the scripting global audio API. They run headless, so the
 * checks verify the functions are callable and that round-trippable mixer
 * state (master volume/mute, bus existence/solo) agrees with its getter. */

static void test_ext_audio_global_master(void) {
    SECTION_START("Audio Global: Master Volume / Mute");

    lc_audio_set_master_volume(0.5f);
    float vol = lc_audio_get_master_volume();
    TEST_ASSERT(vol >= 0.49f && vol <= 0.51f, "lc_audio_set/get_master_volume() round-trips 0.5");

    lc_audio_set_master_volume(1.0f);
    TEST_ASSERT(lc_audio_get_master_volume() >= 0.99f,
                "lc_audio_set/get_master_volume() round-trips 1.0");

    lc_audio_set_master_muted(true);
    TEST_ASSERT(lc_audio_is_master_muted() == true, "lc_audio_set/is_master_muted() round-trips true");
    lc_audio_set_master_muted(false);
    TEST_ASSERT(lc_audio_is_master_muted() == false, "lc_audio_set/is_master_muted() round-trips false");

    SECTION_END("Audio Global: Master Volume / Mute");
}

static void test_ext_audio_global_listener(void) {
    SECTION_START("Audio Global: Listener");

    lc_audio_set_listener_position(lc_vec3(1.0f, 2.0f, 3.0f));
    lc_audio_set_listener_orientation(lc_vec3(0.0f, 0.0f, -1.0f), lc_vec3(0.0f, 1.0f, 0.0f));
    lc_audio_set_listener_velocity(lc_vec3(0.0f, 0.0f, 0.0f));
    TEST_ASSERT(true, "lc_audio_set_listener_position/orientation/velocity() callable");

    SECTION_END("Audio Global: Listener");
}

static void test_ext_audio_global_source(void) {
    SECTION_START("Audio Global: Source Params by UUID");

    const char* uuid = "00000000-0000-0000-0000-000000000001";
    lc_audio_set_source_volume(uuid, 0.75f);
    lc_audio_set_source_pitch(uuid, 1.5f);
    lc_audio_set_source_pan(uuid, -0.25f);
    TEST_ASSERT(true, "lc_audio_set_source_volume/pitch/pan() callable");

    lc_audio_set_source_volume(NULL, 1.0f);
    lc_audio_set_source_pitch(NULL, 1.0f);
    lc_audio_set_source_pan(NULL, 0.0f);
    TEST_ASSERT(true, "lc_audio_set_source_* tolerate NULL uuid");

    SECTION_END("Audio Global: Source Params by UUID");
}

static void test_ext_audio_global_bus(void) {
    SECTION_START("Audio Global: Buses");

    lc_audio_create_bus("SFX", NULL);
    TEST_ASSERT(lc_audio_has_bus("SFX") == true, "lc_audio_create_bus()/has_bus() create + report SFX");

    lc_audio_set_bus_solo("SFX", true);
    TEST_ASSERT(lc_audio_is_bus_solo("SFX") == true, "lc_audio_set/is_bus_solo() round-trips true");
    lc_audio_set_bus_solo("SFX", false);
    TEST_ASSERT(lc_audio_is_bus_solo("SFX") == false, "lc_audio_set/is_bus_solo() round-trips false");

    lc_audio_destroy_bus("SFX");
    TEST_ASSERT(lc_audio_has_bus("SFX") == false, "lc_audio_destroy_bus() removes SFX");

    TEST_ASSERT(lc_audio_has_bus(NULL) == false, "lc_audio_has_bus(NULL) returns false");
    TEST_ASSERT(lc_audio_is_bus_solo(NULL) == false, "lc_audio_is_bus_solo(NULL) returns false");

    SECTION_END("Audio Global: Buses");
}
