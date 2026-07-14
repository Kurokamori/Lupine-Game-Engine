/**
 * @file lc_video_player.h
 * @brief Lupine Engine C API - VideoPlayer component
 *
 * Plays a video file on a 2D node (FFmpeg-backed). Supports loop / one-shot
 * playback, a speed multiplier, and routing the decoded audio track through the
 * engine's audio mixer (volume, bus, mute). When the engine is built without
 * the video backend the component loads nothing; these functions still succeed
 * but report an unloaded player.
 */

#ifndef LUPINE_CAPI_VIDEO_PLAYER_H
#define LUPINE_CAPI_VIDEO_PLAYER_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Creation
 * ============================================================================ */

/**
 * @brief Create a VideoPlayer component
 * @param name Optional name for the component (can be NULL)
 * @param out_component Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_video_player_create(const char* name, LCComponentHandle* out_component);

/* ============================================================================
 * Source
 * ============================================================================ */

/** @brief Get the video file path. */
LC_API LCResult lc_video_player_get_video_path(LCComponentHandle component, char* out_path, int path_size);
/** @brief Set the video file path (opens the video). */
LC_API LCResult lc_video_player_set_video_path(LCComponentHandle component, const char* filepath);

/** @brief True once a video has been successfully opened. */
LC_API LCResult lc_video_player_is_loaded(LCComponentHandle component, bool* out_loaded);
/** @brief True if the opened video contains an audio track. */
LC_API LCResult lc_video_player_has_audio(LCComponentHandle component, bool* out_has_audio);

/** @brief Get the video duration in seconds (0 if unknown). */
LC_API LCResult lc_video_player_get_duration(LCComponentHandle component, double* out_seconds);
/** @brief Get the current playback position in seconds. */
LC_API LCResult lc_video_player_get_position(LCComponentHandle component, double* out_seconds);
/** @brief Get the native video width in pixels. */
LC_API LCResult lc_video_player_get_video_width(LCComponentHandle component, int* out_width);
/** @brief Get the native video height in pixels. */
LC_API LCResult lc_video_player_get_video_height(LCComponentHandle component, int* out_height);

/* ============================================================================
 * Playback control
 * ============================================================================ */

/** @brief Start playback (restarts from the beginning if finished). */
LC_API LCResult lc_video_player_play(LCComponentHandle component);
/** @brief Stop playback and reset to the first frame. */
LC_API LCResult lc_video_player_stop(LCComponentHandle component);
/** @brief Pause playback (also pauses audio). */
LC_API LCResult lc_video_player_pause(LCComponentHandle component);
/** @brief Resume a paused playback. */
LC_API LCResult lc_video_player_resume(LCComponentHandle component);
/** @brief Restart playback from the beginning. */
LC_API LCResult lc_video_player_restart(LCComponentHandle component);

/** @brief Restart from the beginning and play (replay a finished one-shot). */
LC_API LCResult lc_video_player_replay(LCComponentHandle component);

/** @brief Seek to a time in seconds (no-op unless seeking is enabled). */
LC_API LCResult lc_video_player_seek(LCComponentHandle component, double seconds);

/** @brief Query whether the video is currently playing. */
LC_API LCResult lc_video_player_is_playing(LCComponentHandle component, bool* out_playing);

/* ============================================================================
 * Playback settings
 * ============================================================================ */

/** @brief Set looping (true = repeat, false = play once). */
LC_API LCResult lc_video_player_set_loop(LCComponentHandle component, bool loop);
/** @brief Get the loop flag. */
LC_API LCResult lc_video_player_get_loop(LCComponentHandle component, bool* out_loop);

/** @brief Set whether playback starts automatically when the scene runs. */
LC_API LCResult lc_video_player_set_auto_play(LCComponentHandle component, bool auto_play);
/** @brief Get the auto-play flag. */
LC_API LCResult lc_video_player_get_auto_play(LCComponentHandle component, bool* out_auto_play);

/** @brief Enable or disable seeking (gates lc_video_player_seek). */
LC_API LCResult lc_video_player_set_seek_enabled(LCComponentHandle component, bool enabled);
/** @brief Get whether seeking is enabled. */
LC_API LCResult lc_video_player_get_seek_enabled(LCComponentHandle component, bool* out_enabled);

/** @brief Set whether the video animates live in the editor viewport (overrides auto-play in-editor). */
LC_API LCResult lc_video_player_set_preview_in_editor(LCComponentHandle component, bool enabled);
/** @brief Get the editor-preview flag. */
LC_API LCResult lc_video_player_get_preview_in_editor(LCComponentHandle component, bool* out_enabled);

/** @brief Set the playback speed multiplier (audio plays only at 1.0). */
LC_API LCResult lc_video_player_set_speed(LCComponentHandle component, float speed);
/** @brief Get the playback speed multiplier. */
LC_API LCResult lc_video_player_get_speed(LCComponentHandle component, float* out_speed);

/* ============================================================================
 * Audio
 * ============================================================================ */

/** @brief Enable or disable playback of the video's audio track. */
LC_API LCResult lc_video_player_set_audio_enabled(LCComponentHandle component, bool enabled);
/** @brief Get whether audio playback is enabled. */
LC_API LCResult lc_video_player_get_audio_enabled(LCComponentHandle component, bool* out_enabled);

/** @brief Set the audio volume (0.0 - 1.0). */
LC_API LCResult lc_video_player_set_volume(LCComponentHandle component, float volume);
/** @brief Get the audio volume. */
LC_API LCResult lc_video_player_get_volume(LCComponentHandle component, float* out_volume);

/** @brief Set the audio bus the track plays on. */
LC_API LCResult lc_video_player_set_bus(LCComponentHandle component, const char* bus);
/** @brief Get the audio bus name. */
LC_API LCResult lc_video_player_get_bus(LCComponentHandle component, char* out_bus, int bus_size);

/** @brief Mute or unmute the audio track. */
LC_API LCResult lc_video_player_set_muted(LCComponentHandle component, bool muted);
/** @brief Get the mute state. */
LC_API LCResult lc_video_player_get_muted(LCComponentHandle component, bool* out_muted);

/* ============================================================================
 * Visual properties
 * ============================================================================ */

/** @brief Set the render offset (in pixels) from the node position. */
LC_API LCResult lc_video_player_set_offset(LCComponentHandle component, LCVec2 offset);
/** @brief Get the render offset. */
LC_API LCResult lc_video_player_get_offset(LCComponentHandle component, LCVec2* out_offset);

/** @brief Set an explicit render size in pixels (0,0 = use native video size). */
LC_API LCResult lc_video_player_set_size(LCComponentHandle component, LCVec2 size);
/** @brief Get the explicit render size (0,0 if using native size). */
LC_API LCResult lc_video_player_get_size(LCComponentHandle component, LCVec2* out_size);

/** @brief Set horizontal flip. */
LC_API LCResult lc_video_player_set_flip_h(LCComponentHandle component, bool flip);
/** @brief Get horizontal flip. */
LC_API LCResult lc_video_player_get_flip_h(LCComponentHandle component, bool* out_flip);

/** @brief Set vertical flip. */
LC_API LCResult lc_video_player_set_flip_v(LCComponentHandle component, bool flip);
/** @brief Get vertical flip. */
LC_API LCResult lc_video_player_get_flip_v(LCComponentHandle component, bool* out_flip);

/** @brief Set the modulate (tint) color. */
LC_API LCResult lc_video_player_set_modulate(LCComponentHandle component, LCColor color);
/** @brief Get the modulate (tint) color. */
LC_API LCResult lc_video_player_get_modulate(LCComponentHandle component, LCColor* out_color);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_VIDEO_PLAYER_H */
