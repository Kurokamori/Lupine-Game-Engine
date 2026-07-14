/**
 * @file lc_gif_player.h
 * @brief Lupine Engine C API - GifPlayer component
 *
 * Plays back an animated GIF on a 2D node with loop / one-shot / ping-pong
 * modes, a playback speed multiplier, and an optional fixed frame rate that
 * overrides the GIF's own per-frame delays.
 */

#ifndef LUPINE_CAPI_GIF_PLAYER_H
#define LUPINE_CAPI_GIF_PLAYER_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/** GifPlayer loop modes (matches lupine::components::GifPlayer::LoopMode). */
typedef enum LCGifLoopMode {
    LC_GIF_LOOP_ONE_SHOT = 0,
    LC_GIF_LOOP_LOOP = 1,
    LC_GIF_LOOP_PING_PONG = 2
} LCGifLoopMode;

/* ============================================================================
 * Creation
 * ============================================================================ */

/**
 * @brief Create a GifPlayer component
 * @param name Optional name for the component (can be NULL)
 * @param out_component Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_gif_player_create(const char* name, LCComponentHandle* out_component);

/* ============================================================================
 * Source
 * ============================================================================ */

/** @brief Get the GIF file path. */
LC_API LCResult lc_gif_player_get_gif_path(LCComponentHandle component, char* out_path, int path_size);
/** @brief Set the GIF file path (loads the GIF). */
LC_API LCResult lc_gif_player_set_gif_path(LCComponentHandle component, const char* filepath);

/** @brief True once a GIF has been successfully loaded. */
LC_API LCResult lc_gif_player_is_loaded(LCComponentHandle component, bool* out_loaded);

/* ============================================================================
 * Playback control
 * ============================================================================ */

/** @brief Start (or restart a finished) playback. */
LC_API LCResult lc_gif_player_play(LCComponentHandle component);
/** @brief Stop playback and reset to the first frame. */
LC_API LCResult lc_gif_player_stop(LCComponentHandle component);
/** @brief Pause playback on the current frame. */
LC_API LCResult lc_gif_player_pause(LCComponentHandle component);
/** @brief Resume a paused playback. */
LC_API LCResult lc_gif_player_resume(LCComponentHandle component);

/** @brief Restart from the first frame and play (replay a finished one-shot). */
LC_API LCResult lc_gif_player_replay(LCComponentHandle component);

/** @brief Query whether the GIF is currently playing. */
LC_API LCResult lc_gif_player_is_playing(LCComponentHandle component, bool* out_playing);

/** @brief Jump to a specific frame index (clamped to range). */
LC_API LCResult lc_gif_player_set_frame(LCComponentHandle component, int frame_index);
/** @brief Get the currently displayed frame index. */
LC_API LCResult lc_gif_player_get_current_frame(LCComponentHandle component, int* out_frame);
/** @brief Get the total number of frames in the loaded GIF. */
LC_API LCResult lc_gif_player_get_frame_count(LCComponentHandle component, int* out_count);

/* ============================================================================
 * Playback settings
 * ============================================================================ */

/** @brief Set the loop mode (one-shot / loop / ping-pong). */
LC_API LCResult lc_gif_player_set_loop_mode(LCComponentHandle component, LCGifLoopMode mode);
/** @brief Get the loop mode. */
LC_API LCResult lc_gif_player_get_loop_mode(LCComponentHandle component, LCGifLoopMode* out_mode);

/** @brief Set whether playback starts automatically when the scene runs. */
LC_API LCResult lc_gif_player_set_auto_play(LCComponentHandle component, bool auto_play);
/** @brief Get the auto-play flag. */
LC_API LCResult lc_gif_player_get_auto_play(LCComponentHandle component, bool* out_auto_play);

/** @brief Set whether the GIF animates live in the editor viewport (overrides auto-play in-editor). */
LC_API LCResult lc_gif_player_set_preview_in_editor(LCComponentHandle component, bool enabled);
/** @brief Get the editor-preview flag. */
LC_API LCResult lc_gif_player_get_preview_in_editor(LCComponentHandle component, bool* out_enabled);

/** @brief Set the playback speed multiplier (1.0 = normal). */
LC_API LCResult lc_gif_player_set_speed(LCComponentHandle component, float speed);
/** @brief Get the playback speed multiplier. */
LC_API LCResult lc_gif_player_get_speed(LCComponentHandle component, float* out_speed);

/** @brief Force a constant frame rate (0 = honor the GIF's own per-frame delays). */
LC_API LCResult lc_gif_player_set_fps_override(LCComponentHandle component, float fps);
/** @brief Get the forced frame rate (0 if disabled). */
LC_API LCResult lc_gif_player_get_fps_override(LCComponentHandle component, float* out_fps);

/* ============================================================================
 * Visual properties
 * ============================================================================ */

/** @brief Set the render offset (in pixels) from the node position. */
LC_API LCResult lc_gif_player_set_offset(LCComponentHandle component, LCVec2 offset);
/** @brief Get the render offset. */
LC_API LCResult lc_gif_player_get_offset(LCComponentHandle component, LCVec2* out_offset);

/** @brief Set an explicit render size in pixels (0,0 = use native GIF size). */
LC_API LCResult lc_gif_player_set_size(LCComponentHandle component, LCVec2 size);
/** @brief Get the explicit render size (0,0 if using native size). */
LC_API LCResult lc_gif_player_get_size(LCComponentHandle component, LCVec2* out_size);

/** @brief Set horizontal flip. */
LC_API LCResult lc_gif_player_set_flip_h(LCComponentHandle component, bool flip);
/** @brief Get horizontal flip. */
LC_API LCResult lc_gif_player_get_flip_h(LCComponentHandle component, bool* out_flip);

/** @brief Set vertical flip. */
LC_API LCResult lc_gif_player_set_flip_v(LCComponentHandle component, bool flip);
/** @brief Get vertical flip. */
LC_API LCResult lc_gif_player_get_flip_v(LCComponentHandle component, bool* out_flip);

/** @brief Set the modulate (tint) color. */
LC_API LCResult lc_gif_player_set_modulate(LCComponentHandle component, LCColor color);
/** @brief Get the modulate (tint) color. */
LC_API LCResult lc_gif_player_get_modulate(LCComponentHandle component, LCColor* out_color);

/** @brief Set pixel snapping for crisp rendering. */
LC_API LCResult lc_gif_player_set_pixel_snap(LCComponentHandle component, bool snap);
/** @brief Get pixel snapping. */
LC_API LCResult lc_gif_player_get_pixel_snap(LCComponentHandle component, bool* out_snap);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_GIF_PLAYER_H */
