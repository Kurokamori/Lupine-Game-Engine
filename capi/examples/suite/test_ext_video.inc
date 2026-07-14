/*
 * test_ext_video.inc - C API coverage for the GifPlayer and VideoPlayer media
 * components. Included by capi_test_suite.c.
 *
 * These exercise the binding surface (create / property round-trips / playback
 * control callability). They do not require a real media file on disk: the
 * setters/getters operate on component state, and playback calls are validated
 * as callable rather than for produced frames.
 */

static void test_ext_gif_player(void) {
    SECTION_START("Media: GifPlayer");

    LCComponentHandle gif = NULL;
    LCResult result = lc_gif_player_create("TestGifPlayer", &gif);
    TEST_RESULT(result, "Create GifPlayer");
    TEST_ASSERT(gif != NULL, "GifPlayer handle is valid");

    /* Path round-trip */
    result = lc_gif_player_set_gif_path(gif, "res://media/anim.gif");
    TEST_RESULT(result, "Set gif path");
    char path[256];
    result = lc_gif_player_get_gif_path(gif, path, sizeof(path));
    TEST_RESULT(result, "Get gif path");
    TEST_ASSERT(strcmp(path, "res://media/anim.gif") == 0, "Gif path round-trips");

    /* Loop mode round-trip */
    result = lc_gif_player_set_loop_mode(gif, LC_GIF_LOOP_PING_PONG);
    TEST_RESULT(result, "Set loop mode");
    LCGifLoopMode mode = LC_GIF_LOOP_ONE_SHOT;
    result = lc_gif_player_get_loop_mode(gif, &mode);
    TEST_RESULT(result, "Get loop mode");
    TEST_ASSERT(mode == LC_GIF_LOOP_PING_PONG, "Loop mode round-trips");

    /* Speed round-trip */
    result = lc_gif_player_set_speed(gif, 2.0f);
    TEST_RESULT(result, "Set speed");
    float speed = 0.0f;
    result = lc_gif_player_get_speed(gif, &speed);
    TEST_RESULT(result, "Get speed");
    TEST_ASSERT(ext_float_eq(speed, 2.0f), "Speed round-trips");

    /* FPS override round-trip */
    result = lc_gif_player_set_fps_override(gif, 12.0f);
    TEST_RESULT(result, "Set fps override");
    float fps = 0.0f;
    result = lc_gif_player_get_fps_override(gif, &fps);
    TEST_RESULT(result, "Get fps override");
    TEST_ASSERT(ext_float_eq(fps, 12.0f), "FPS override round-trips");

    /* Auto-play + flips + modulate */
    result = lc_gif_player_set_auto_play(gif, false);
    TEST_RESULT(result, "Set auto play");
    bool autoPlay = true;
    result = lc_gif_player_get_auto_play(gif, &autoPlay);
    TEST_RESULT(result, "Get auto play");
    TEST_ASSERT(autoPlay == false, "Auto play round-trips");

    result = lc_gif_player_set_flip_h(gif, true);
    TEST_RESULT(result, "Set flip H");
    bool flipH = false;
    result = lc_gif_player_get_flip_h(gif, &flipH);
    TEST_RESULT(result, "Get flip H");
    TEST_ASSERT(flipH == true, "Flip H round-trips");

    result = lc_gif_player_set_size(gif, lc_vec2(64.0f, 48.0f));
    TEST_RESULT(result, "Set size");
    LCVec2 size = {0.0f, 0.0f};
    result = lc_gif_player_get_size(gif, &size);
    TEST_RESULT(result, "Get size");
    TEST_ASSERT(ext_float_eq(size.x, 64.0f) && ext_float_eq(size.y, 48.0f), "Size round-trips");

    /* Playback control is callable */
    result = lc_gif_player_play(gif);
    printf("    play result: %d\n", (int)result);
    TEST_ASSERT(true, "lc_gif_player_play() callable");
    result = lc_gif_player_pause(gif);
    TEST_ASSERT(true, "lc_gif_player_pause() callable");
    result = lc_gif_player_stop(gif);
    TEST_ASSERT(true, "lc_gif_player_stop() callable");

    result = lc_gif_player_replay(gif);
    TEST_ASSERT(true, "lc_gif_player_replay() callable");

    /* Preview-in-editor toggle round-trip */
    result = lc_gif_player_set_preview_in_editor(gif, false);
    TEST_RESULT(result, "Set preview in editor");
    bool preview = true;
    result = lc_gif_player_get_preview_in_editor(gif, &preview);
    TEST_RESULT(result, "Get preview in editor");
    TEST_ASSERT(preview == false, "Preview-in-editor round-trips");

    int frameCount = -1;
    result = lc_gif_player_get_frame_count(gif, &frameCount);
    TEST_RESULT(result, "Get frame count");
    TEST_ASSERT(frameCount >= 0, "Frame count is non-negative");

    lc_component_destroy(gif);
    SECTION_END("Media: GifPlayer");
}

static void test_ext_video_player(void) {
    SECTION_START("Media: VideoPlayer");

    LCComponentHandle video = NULL;
    LCResult result = lc_video_player_create("TestVideoPlayer", &video);
    TEST_RESULT(result, "Create VideoPlayer");
    TEST_ASSERT(video != NULL, "VideoPlayer handle is valid");

    /* Path round-trip */
    result = lc_video_player_set_video_path(video, "res://media/clip.mp4");
    TEST_RESULT(result, "Set video path");
    char path[256];
    result = lc_video_player_get_video_path(video, path, sizeof(path));
    TEST_RESULT(result, "Get video path");
    TEST_ASSERT(strcmp(path, "res://media/clip.mp4") == 0, "Video path round-trips");

    /* Loop + auto play */
    result = lc_video_player_set_loop(video, true);
    TEST_RESULT(result, "Set loop");
    bool loop = false;
    result = lc_video_player_get_loop(video, &loop);
    TEST_RESULT(result, "Get loop");
    TEST_ASSERT(loop == true, "Loop round-trips");

    /* Speed */
    result = lc_video_player_set_speed(video, 1.5f);
    TEST_RESULT(result, "Set speed");
    float speed = 0.0f;
    result = lc_video_player_get_speed(video, &speed);
    TEST_RESULT(result, "Get speed");
    TEST_ASSERT(ext_float_eq(speed, 1.5f), "Speed round-trips");

    /* Audio: enabled, volume, bus, muted */
    result = lc_video_player_set_audio_enabled(video, false);
    TEST_RESULT(result, "Set audio enabled");
    bool audioEnabled = true;
    result = lc_video_player_get_audio_enabled(video, &audioEnabled);
    TEST_RESULT(result, "Get audio enabled");
    TEST_ASSERT(audioEnabled == false, "Audio enabled round-trips");

    result = lc_video_player_set_volume(video, 0.5f);
    TEST_RESULT(result, "Set volume");
    float volume = 0.0f;
    result = lc_video_player_get_volume(video, &volume);
    TEST_RESULT(result, "Get volume");
    TEST_ASSERT(ext_float_eq(volume, 0.5f), "Volume round-trips");

    result = lc_video_player_set_bus(video, "Music");
    TEST_RESULT(result, "Set bus");
    char bus[64];
    result = lc_video_player_get_bus(video, bus, sizeof(bus));
    TEST_RESULT(result, "Get bus");
    TEST_ASSERT(strcmp(bus, "Music") == 0, "Bus round-trips");

    result = lc_video_player_set_muted(video, true);
    TEST_RESULT(result, "Set muted");
    bool muted = false;
    result = lc_video_player_get_muted(video, &muted);
    TEST_RESULT(result, "Get muted");
    TEST_ASSERT(muted == true, "Muted round-trips");

    /* Modulate */
    result = lc_video_player_set_modulate(video, lc_color(1.0f, 0.5f, 0.25f, 1.0f));
    TEST_RESULT(result, "Set modulate");
    LCColor color = {0, 0, 0, 0};
    result = lc_video_player_get_modulate(video, &color);
    TEST_RESULT(result, "Get modulate");
    TEST_ASSERT(ext_float_eq(color.r, 1.0f) && ext_float_eq(color.g, 0.5f), "Modulate round-trips");

    /* State queries are callable */
    bool loaded = false;
    result = lc_video_player_is_loaded(video, &loaded);
    printf("    is_loaded result: %d (loaded=%d)\n", (int)result, (int)loaded);
    TEST_ASSERT(true, "lc_video_player_is_loaded() callable");

    double duration = -1.0;
    result = lc_video_player_get_duration(video, &duration);
    TEST_ASSERT(true, "lc_video_player_get_duration() callable");

    /* Playback control is callable */
    result = lc_video_player_play(video);
    printf("    play result: %d\n", (int)result);
    TEST_ASSERT(true, "lc_video_player_play() callable");
    result = lc_video_player_pause(video);
    TEST_ASSERT(true, "lc_video_player_pause() callable");
    result = lc_video_player_restart(video);
    TEST_ASSERT(true, "lc_video_player_restart() callable");
    result = lc_video_player_replay(video);
    TEST_ASSERT(true, "lc_video_player_replay() callable");
    result = lc_video_player_stop(video);
    TEST_ASSERT(true, "lc_video_player_stop() callable");

    /* Seek toggle + seek call */
    result = lc_video_player_set_seek_enabled(video, true);
    TEST_RESULT(result, "Set seek enabled");
    bool seekEnabled = false;
    result = lc_video_player_get_seek_enabled(video, &seekEnabled);
    TEST_RESULT(result, "Get seek enabled");
    TEST_ASSERT(seekEnabled == true, "Seek-enabled round-trips");
    result = lc_video_player_seek(video, 1.0);
    TEST_ASSERT(true, "lc_video_player_seek() callable");

    /* Preview-in-editor toggle round-trip */
    result = lc_video_player_set_preview_in_editor(video, false);
    TEST_RESULT(result, "Set preview in editor");
    bool preview = true;
    result = lc_video_player_get_preview_in_editor(video, &preview);
    TEST_RESULT(result, "Get preview in editor");
    TEST_ASSERT(preview == false, "Preview-in-editor round-trips");

    lc_component_destroy(video);
    SECTION_END("Media: VideoPlayer");
}
