/**
 * @file lc_animation_player.h
 * @brief Lupine Engine C API - AnimationPlayer component
 *
 * Keyframe animation playback: a clip library, play/stop/seek/queue, cross-fade
 * blending, and looping. Channels target any node/component property relative to
 * the player's root node.
 */

#ifndef LUPINE_CAPI_ANIMATION_PLAYER_H
#define LUPINE_CAPI_ANIMATION_PLAYER_H

#include "core/lc_core.h"
#include "core/lc_node.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Creation
 * ============================================================================ */

/** Create an AnimationPlayer component. */
LC_API LCResult lc_animation_player_create(const char* name, LCComponentHandle* out_component);

/* ============================================================================
 * Clip library
 * ============================================================================ */

/** Register a clip name -> .animclip resource path. */
LC_API LCResult lc_animation_player_add_clip(LCComponentHandle component, const char* name, const char* path);
/** Remove a clip from the library by name. */
LC_API LCResult lc_animation_player_remove_clip(LCComponentHandle component, const char* name);
/** Test whether a clip is available. */
LC_API LCResult lc_animation_player_has_animation(LCComponentHandle component, const char* name, bool* out_has);
/** Reload all clip resources from disk. */
LC_API LCResult lc_animation_player_reload_clips(LCComponentHandle component);

/* ============================================================================
 * Playback
 * ============================================================================ */

/** Play a clip by name (uses the default blend time). */
LC_API LCResult lc_animation_player_play(LCComponentHandle component, const char* name);
/** Play a clip by name, cross-fading over blend_time seconds. */
LC_API LCResult lc_animation_player_play_blend(LCComponentHandle component, const char* name, float blend_time);
/** Play a clip backwards. */
LC_API LCResult lc_animation_player_play_backwards(LCComponentHandle component, const char* name);
/** Stop playback (optionally resetting the playhead to the start). */
LC_API LCResult lc_animation_player_stop(LCComponentHandle component, bool reset);
/** Pause playback in place. */
LC_API LCResult lc_animation_player_pause(LCComponentHandle component);
/** Resume paused playback. */
LC_API LCResult lc_animation_player_resume(LCComponentHandle component);
/** Seek to an absolute time (seconds); applies the pose when update is true. */
LC_API LCResult lc_animation_player_seek(LCComponentHandle component, float time, bool update);
/** Queue a clip to play after the current one finishes. */
LC_API LCResult lc_animation_player_queue(LCComponentHandle component, const char* name);
/** Clear the playback queue. */
LC_API LCResult lc_animation_player_clear_queue(LCComponentHandle component);
/** Query whether an animation is currently playing. */
LC_API LCResult lc_animation_player_is_playing(LCComponentHandle component, bool* out_playing);
/** Get the current animation name into out_name (truncated to name_size). */
LC_API LCResult lc_animation_player_get_current_animation(LCComponentHandle component, char* out_name, int name_size);
/** Apply a clip's pose at an absolute time to the scene (editor preview / manual). */
LC_API LCResult lc_animation_player_sample_at(LCComponentHandle component, const char* name, float time);

/* ============================================================================
 * Properties
 * ============================================================================ */

LC_API LCResult lc_animation_player_set_speed(LCComponentHandle component, float speed);
LC_API LCResult lc_animation_player_get_speed(LCComponentHandle component, float* out_speed);
LC_API LCResult lc_animation_player_set_auto_play(LCComponentHandle component, const char* name);
LC_API LCResult lc_animation_player_set_root_node(LCComponentHandle component, const char* path);
LC_API LCResult lc_animation_player_set_default_blend_time(LCComponentHandle component, float blend_time);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_ANIMATION_PLAYER_H */
