/* Extended media C API tests. Included by capi_test_suite.c.
 *
 * Covers binding surface not exercised by test_ext_audio.inc,
 * test_ext_audio_global.inc, or test_ext_video.inc:
 *
 *  - Fire-and-forget mixer playback + control by UUID (audio/lc_audio.h):
 *    play / play_3d / play_scheduled / play_scheduled_3d, stop / pause /
 *    resume, source_is_playing / source_is_finished, and bus volume/muted.
 *    These require a running audio device that is absent headless, so they
 *    use TOLERANT assertions (success OR a plausible error code).
 *
 *  - VideoPlayer getters/setters not covered by test_ext_video.inc:
 *    auto_play / flip_h / flip_v / offset / size deterministic round-trips,
 *    plus tolerant callability for is_playing / resume / position /
 *    video dimensions / has_audio (all need a loaded video).
 *
 *  - GifPlayer getters/setters not covered by test_ext_video.inc:
 *    flip_v / modulate / offset / pixel_snap / frame deterministic
 *    round-trips, plus tolerant callability for is_loaded / is_playing /
 *    resume / current_frame.
 *
 * The VideoPlayer / GifPlayer C API functions are declared unconditionally in
 * their headers (they succeed but report an unloaded player when the engine is
 * built without the video backend), so no LUPINE_ENABLE_VIDEO guard is needed
 * for the fragment to compile. */

static bool media_ext_float_eq(float a, float b) {
    float d = a - b;
    if (d < 0.0f) d = -d;
    return d < 0.001f;
}

static void test_ext_media_ext_mixer_playback(void) {
    SECTION_START("Media Ext: Mixer Fire-and-Forget Playback");

    char uuid[64];
    uuid[0] = '\0';

    /* 2D play. No audio device headless, so tolerate failure. */
    LCResult result = lc_audio_play("res://audio/sfx.wav", "Master", false, 1.0f,
                                    uuid, sizeof(uuid));
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_AUDIO_PLAYBACK_FAILED ||
                result == LC_ERROR_AUDIO_ASSET_LOAD_FAILED || result == LC_ERROR_AUDIO_INIT_FAILED ||
                result == LC_ERROR_FILE_NOT_FOUND || result == LC_ERROR_OPERATION_FAILED ||
                result == LC_ERROR_NOT_INITIALIZED,
                "lc_audio_play() callable (tolerant headless)");

    /* 3D play. */
    char uuid3d[64];
    uuid3d[0] = '\0';
    result = lc_audio_play_3d("res://audio/sfx.wav", lc_vec3(1.0f, 2.0f, 3.0f), "Master",
                              false, 0.8f, uuid3d, sizeof(uuid3d));
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_AUDIO_PLAYBACK_FAILED ||
                result == LC_ERROR_AUDIO_ASSET_LOAD_FAILED || result == LC_ERROR_AUDIO_INIT_FAILED ||
                result == LC_ERROR_FILE_NOT_FOUND || result == LC_ERROR_OPERATION_FAILED ||
                result == LC_ERROR_NOT_INITIALIZED,
                "lc_audio_play_3d() callable (tolerant headless)");

    /* Scheduled 2D play. */
    char uuidSched[64];
    uuidSched[0] = '\0';
    result = lc_audio_play_scheduled("res://audio/sfx.wav", 0.5f, "Master", false, 1.0f,
                                     uuidSched, sizeof(uuidSched));
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_AUDIO_PLAYBACK_FAILED ||
                result == LC_ERROR_AUDIO_ASSET_LOAD_FAILED || result == LC_ERROR_AUDIO_INIT_FAILED ||
                result == LC_ERROR_FILE_NOT_FOUND || result == LC_ERROR_OPERATION_FAILED ||
                result == LC_ERROR_NOT_INITIALIZED,
                "lc_audio_play_scheduled() callable (tolerant headless)");

    /* Scheduled 3D play. */
    char uuidSched3d[64];
    uuidSched3d[0] = '\0';
    result = lc_audio_play_scheduled_3d("res://audio/sfx.wav", lc_vec3(0.0f, 0.0f, 0.0f), 0.5f,
                                        "Master", true, 0.5f, uuidSched3d, sizeof(uuidSched3d));
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_AUDIO_PLAYBACK_FAILED ||
                result == LC_ERROR_AUDIO_ASSET_LOAD_FAILED || result == LC_ERROR_AUDIO_INIT_FAILED ||
                result == LC_ERROR_FILE_NOT_FOUND || result == LC_ERROR_OPERATION_FAILED ||
                result == LC_ERROR_NOT_INITIALIZED,
                "lc_audio_play_scheduled_3d() callable (tolerant headless)");

    SECTION_END("Media Ext: Mixer Fire-and-Forget Playback");
}

static void test_ext_media_ext_mixer_control(void) {
    SECTION_START("Media Ext: Mixer Source Control by UUID");

    /* A made-up UUID will not resolve to a live source headless, so tolerate
     * a not-found / failed result alongside success. */
    const char* uuid = "00000000-0000-0000-0000-000000000042";

    LCResult result = lc_audio_stop(uuid);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_NOT_FOUND ||
                result == LC_ERROR_OPERATION_FAILED || result == LC_ERROR_NOT_INITIALIZED ||
                result == LC_ERROR_AUDIO_PLAYBACK_FAILED,
                "lc_audio_stop() callable (tolerant headless)");

    result = lc_audio_pause(uuid);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_NOT_FOUND ||
                result == LC_ERROR_OPERATION_FAILED || result == LC_ERROR_NOT_INITIALIZED ||
                result == LC_ERROR_AUDIO_PLAYBACK_FAILED,
                "lc_audio_pause() callable (tolerant headless)");

    result = lc_audio_resume(uuid);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_NOT_FOUND ||
                result == LC_ERROR_OPERATION_FAILED || result == LC_ERROR_NOT_INITIALIZED ||
                result == LC_ERROR_AUDIO_PLAYBACK_FAILED,
                "lc_audio_resume() callable (tolerant headless)");

    bool playing = true;
    result = lc_audio_source_is_playing(uuid, &playing);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_NOT_FOUND ||
                result == LC_ERROR_OPERATION_FAILED || result == LC_ERROR_NOT_INITIALIZED,
                "lc_audio_source_is_playing() callable (tolerant headless)");

    bool finished = false;
    result = lc_audio_source_is_finished(uuid, &finished);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_NOT_FOUND ||
                result == LC_ERROR_OPERATION_FAILED || result == LC_ERROR_NOT_INITIALIZED,
                "lc_audio_source_is_finished() callable (tolerant headless)");

    /* NULL-pointer guards. */
    result = lc_audio_source_is_playing(uuid, NULL);
    TEST_ASSERT(result != LC_SUCCESS, "lc_audio_source_is_playing(NULL out) rejects null pointer");
    result = lc_audio_stop(NULL);
    TEST_ASSERT(result != LC_SUCCESS, "lc_audio_stop(NULL uuid) rejects null pointer");

    SECTION_END("Media Ext: Mixer Source Control by UUID");
}

static void test_ext_media_ext_bus_volume_mute(void) {
    SECTION_START("Media Ext: Bus Volume / Mute");

    lc_audio_create_bus("MediaExtBus", NULL);

    /* Set then get; tolerate failure if the bus mixer is not running. */
    LCResult result = lc_audio_set_bus_volume("MediaExtBus", 0.6f);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_NOT_FOUND ||
                result == LC_ERROR_OPERATION_FAILED || result == LC_ERROR_NOT_INITIALIZED,
                "lc_audio_set_bus_volume() callable (tolerant headless)");

    float volume = -1.0f;
    result = lc_audio_get_bus_volume("MediaExtBus", &volume);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_NOT_FOUND ||
                result == LC_ERROR_OPERATION_FAILED || result == LC_ERROR_NOT_INITIALIZED,
                "lc_audio_get_bus_volume() callable (tolerant headless)");
    if (result == LC_SUCCESS) {
        TEST_ASSERT(media_ext_float_eq(volume, 0.6f), "Bus volume round-trips when supported");
    }

    result = lc_audio_set_bus_muted("MediaExtBus", true);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_NOT_FOUND ||
                result == LC_ERROR_OPERATION_FAILED || result == LC_ERROR_NOT_INITIALIZED,
                "lc_audio_set_bus_muted() callable (tolerant headless)");

    bool muted = false;
    result = lc_audio_is_bus_muted("MediaExtBus", &muted);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_NOT_FOUND ||
                result == LC_ERROR_OPERATION_FAILED || result == LC_ERROR_NOT_INITIALIZED,
                "lc_audio_is_bus_muted() callable (tolerant headless)");
    if (result == LC_SUCCESS) {
        TEST_ASSERT(muted == true, "Bus muted round-trips when supported");
    }

    /* NULL-pointer guard. */
    result = lc_audio_get_bus_volume("MediaExtBus", NULL);
    TEST_ASSERT(result != LC_SUCCESS, "lc_audio_get_bus_volume(NULL out) rejects null pointer");

    lc_audio_destroy_bus("MediaExtBus");

    SECTION_END("Media Ext: Bus Volume / Mute");
}

static void test_ext_media_ext_video_player(void) {
    SECTION_START("Media Ext: VideoPlayer Getters/Setters");

    LCComponentHandle video = NULL;
    LCResult result = lc_video_player_create("MediaExtVideoPlayer", &video);
    TEST_RESULT(result, "Create VideoPlayer");
    TEST_ASSERT(video != NULL, "VideoPlayer handle is valid");

    /* Auto-play round-trip. */
    result = lc_video_player_set_auto_play(video, true);
    TEST_RESULT(result, "Set auto play");
    bool autoPlay = false;
    result = lc_video_player_get_auto_play(video, &autoPlay);
    TEST_RESULT(result, "Get auto play");
    TEST_ASSERT(autoPlay == true, "Auto play round-trips");

    /* Flip H round-trip. */
    result = lc_video_player_set_flip_h(video, true);
    TEST_RESULT(result, "Set flip H");
    bool flipH = false;
    result = lc_video_player_get_flip_h(video, &flipH);
    TEST_RESULT(result, "Get flip H");
    TEST_ASSERT(flipH == true, "Flip H round-trips");

    /* Flip V round-trip. */
    result = lc_video_player_set_flip_v(video, true);
    TEST_RESULT(result, "Set flip V");
    bool flipV = false;
    result = lc_video_player_get_flip_v(video, &flipV);
    TEST_RESULT(result, "Get flip V");
    TEST_ASSERT(flipV == true, "Flip V round-trips");

    /* Offset round-trip. */
    result = lc_video_player_set_offset(video, lc_vec2(16.0f, -24.0f));
    TEST_RESULT(result, "Set offset");
    LCVec2 offset = {0.0f, 0.0f};
    result = lc_video_player_get_offset(video, &offset);
    TEST_RESULT(result, "Get offset");
    TEST_ASSERT(media_ext_float_eq(offset.x, 16.0f) && media_ext_float_eq(offset.y, -24.0f),
                "Offset round-trips");

    /* Size round-trip. */
    result = lc_video_player_set_size(video, lc_vec2(320.0f, 240.0f));
    TEST_RESULT(result, "Set size");
    LCVec2 size = {0.0f, 0.0f};
    result = lc_video_player_get_size(video, &size);
    TEST_RESULT(result, "Get size");
    TEST_ASSERT(media_ext_float_eq(size.x, 320.0f) && media_ext_float_eq(size.y, 240.0f),
                "Size round-trips");

    /* State queries needing a loaded video: callable, tolerant. */
    bool playing = true;
    result = lc_video_player_is_playing(video, &playing);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED ||
                result == LC_ERROR_NOT_SUPPORTED,
                "lc_video_player_is_playing() callable (tolerant)");

    result = lc_video_player_resume(video);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED ||
                result == LC_ERROR_NOT_SUPPORTED,
                "lc_video_player_resume() callable (tolerant)");

    double position = -1.0;
    result = lc_video_player_get_position(video, &position);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED ||
                result == LC_ERROR_NOT_SUPPORTED,
                "lc_video_player_get_position() callable (tolerant)");

    int width = -1;
    result = lc_video_player_get_video_width(video, &width);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED ||
                result == LC_ERROR_NOT_SUPPORTED,
                "lc_video_player_get_video_width() callable (tolerant)");

    int height = -1;
    result = lc_video_player_get_video_height(video, &height);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED ||
                result == LC_ERROR_NOT_SUPPORTED,
                "lc_video_player_get_video_height() callable (tolerant)");

    bool hasAudio = true;
    result = lc_video_player_has_audio(video, &hasAudio);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED ||
                result == LC_ERROR_NOT_SUPPORTED,
                "lc_video_player_has_audio() callable (tolerant)");

    /* NULL-pointer guard. */
    result = lc_video_player_get_auto_play(video, NULL);
    TEST_ASSERT(result != LC_SUCCESS, "lc_video_player_get_auto_play(NULL out) rejects null pointer");

    lc_component_destroy(video);

    SECTION_END("Media Ext: VideoPlayer Getters/Setters");
}

static void test_ext_media_ext_gif_player(void) {
    SECTION_START("Media Ext: GifPlayer Getters/Setters");

    LCComponentHandle gif = NULL;
    LCResult result = lc_gif_player_create("MediaExtGifPlayer", &gif);
    TEST_RESULT(result, "Create GifPlayer");
    TEST_ASSERT(gif != NULL, "GifPlayer handle is valid");

    /* Flip V round-trip. */
    result = lc_gif_player_set_flip_v(gif, true);
    TEST_RESULT(result, "Set flip V");
    bool flipV = false;
    result = lc_gif_player_get_flip_v(gif, &flipV);
    TEST_RESULT(result, "Get flip V");
    TEST_ASSERT(flipV == true, "Flip V round-trips");

    /* Modulate round-trip. */
    result = lc_gif_player_set_modulate(gif, lc_color(0.25f, 0.5f, 0.75f, 1.0f));
    TEST_RESULT(result, "Set modulate");
    LCColor color = {0, 0, 0, 0};
    result = lc_gif_player_get_modulate(gif, &color);
    TEST_RESULT(result, "Get modulate");
    TEST_ASSERT(media_ext_float_eq(color.r, 0.25f) && media_ext_float_eq(color.g, 0.5f) &&
                media_ext_float_eq(color.b, 0.75f) && media_ext_float_eq(color.a, 1.0f),
                "Modulate round-trips");

    /* Offset round-trip. */
    result = lc_gif_player_set_offset(gif, lc_vec2(8.0f, 12.0f));
    TEST_RESULT(result, "Set offset");
    LCVec2 offset = {0.0f, 0.0f};
    result = lc_gif_player_get_offset(gif, &offset);
    TEST_RESULT(result, "Get offset");
    TEST_ASSERT(media_ext_float_eq(offset.x, 8.0f) && media_ext_float_eq(offset.y, 12.0f),
                "Offset round-trips");

    /* Pixel snap round-trip. */
    result = lc_gif_player_set_pixel_snap(gif, true);
    TEST_RESULT(result, "Set pixel snap");
    bool pixelSnap = false;
    result = lc_gif_player_get_pixel_snap(gif, &pixelSnap);
    TEST_RESULT(result, "Get pixel snap");
    TEST_ASSERT(pixelSnap == true, "Pixel snap round-trips");

    /* Frame set/get. Without a loaded GIF the current frame clamps to 0, so
     * verify the setter is callable and the getter is consistent. */
    result = lc_gif_player_set_frame(gif, 3);
    TEST_RESULT(result, "Set frame");
    int currentFrame = -1;
    result = lc_gif_player_get_current_frame(gif, &currentFrame);
    TEST_RESULT(result, "Get current frame");
    TEST_ASSERT(currentFrame >= 0, "Current frame is non-negative");

    /* State queries needing a loaded GIF: callable, tolerant. */
    bool loaded = true;
    result = lc_gif_player_is_loaded(gif, &loaded);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED ||
                result == LC_ERROR_NOT_SUPPORTED,
                "lc_gif_player_is_loaded() callable (tolerant)");

    bool playing = true;
    result = lc_gif_player_is_playing(gif, &playing);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED ||
                result == LC_ERROR_NOT_SUPPORTED,
                "lc_gif_player_is_playing() callable (tolerant)");

    result = lc_gif_player_resume(gif);
    TEST_ASSERT(result == LC_SUCCESS || result == LC_ERROR_OPERATION_FAILED ||
                result == LC_ERROR_NOT_SUPPORTED,
                "lc_gif_player_resume() callable (tolerant)");

    /* NULL-pointer guard. */
    result = lc_gif_player_get_flip_v(gif, NULL);
    TEST_ASSERT(result != LC_SUCCESS, "lc_gif_player_get_flip_v(NULL out) rejects null pointer");

    lc_component_destroy(gif);

    SECTION_END("Media Ext: GifPlayer Getters/Setters");
}
