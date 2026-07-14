/**
 * @file lc_animated_sprite2d.h
 * @brief Lupine Engine C API - AnimatedSprite2D component
 *
 * This header provides functions for 2D animated sprites using sprite animation files.
 */

#ifndef LUPINE_CAPI_ANIMATED_SPRITE2D_H
#define LUPINE_CAPI_ANIMATED_SPRITE2D_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * AnimatedSprite2D Creation
 * ============================================================================ */

/**
 * @brief Create an AnimatedSprite2D component
 * @param name Optional name for the component (can be NULL)
 * @param out_component Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_animated_sprite2d_create(const char* name, LCComponentHandle* out_component);

/* ============================================================================
 * Animation File
 * ============================================================================ */

/**
 * @brief Get the animation file path
 * @param component Component handle
 * @param out_path Output buffer for file path
 * @param path_size Size of output buffer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_animated_sprite2d_get_animation_file(LCComponentHandle component, char* out_path, int path_size);

/**
 * @brief Set the animation file path (.spriteanimation file)
 * @param component Component handle
 * @param filepath Path to the animation file
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_animated_sprite2d_set_animation_file(LCComponentHandle component, const char* filepath);

/* ============================================================================
 * Animation Name
 * ============================================================================ */

/**
 * @brief Get the current animation name
 * @param component Component handle
 * @param out_name Output buffer for animation name
 * @param name_size Size of output buffer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_animated_sprite2d_get_animation_name(LCComponentHandle component, char* out_name, int name_size);

/**
 * @brief Set the animation name to play
 * @param component Component handle
 * @param name Name of the animation
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_animated_sprite2d_set_animation_name(LCComponentHandle component, const char* name);

/* ============================================================================
 * Playback State
 * ============================================================================ */

/**
 * @brief Check if the animation is currently playing
 * @param component Component handle
 * @param out_playing Output parameter for playing state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_animated_sprite2d_is_playing(LCComponentHandle component, bool* out_playing);

/**
 * @brief Set the playing state
 * @param component Component handle
 * @param playing Whether animation should be playing
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_animated_sprite2d_set_playing(LCComponentHandle component, bool playing);

/**
 * @brief Get the loop state
 * @param component Component handle
 * @param out_loop Output parameter for loop state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_animated_sprite2d_get_loop(LCComponentHandle component, bool* out_loop);

/**
 * @brief Set the loop state
 * @param component Component handle
 * @param loop Whether animation should loop
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_animated_sprite2d_set_loop(LCComponentHandle component, bool loop);

/**
 * @brief Get the auto-play state
 * @param component Component handle
 * @param out_auto_play Output parameter for auto-play state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_animated_sprite2d_get_auto_play(LCComponentHandle component, bool* out_auto_play);

/**
 * @brief Set the auto-play state
 * @param component Component handle
 * @param auto_play Whether animation should auto-play on ready
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_animated_sprite2d_set_auto_play(LCComponentHandle component, bool auto_play);

/* ============================================================================
 * Animation Control
 * ============================================================================ */

/**
 * @brief Play the animation (optionally with a specific animation name)
 * @param component Component handle
 * @param animation_name Optional animation name (NULL to use current)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_animated_sprite2d_play(LCComponentHandle component, const char* animation_name);

/**
 * @brief Stop the animation and reset to first frame
 * @param component Component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_animated_sprite2d_stop(LCComponentHandle component);

/**
 * @brief Pause the animation at current frame
 * @param component Component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_animated_sprite2d_pause(LCComponentHandle component);

/**
 * @brief Resume the animation from current frame
 * @param component Component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_animated_sprite2d_resume(LCComponentHandle component);

/**
 * @brief Get the current frame index
 * @param component Component handle
 * @param out_frame Output parameter for frame index
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_animated_sprite2d_get_frame(LCComponentHandle component, int* out_frame);

/**
 * @brief Set the current frame index
 * @param component Component handle
 * @param frame Frame index to jump to
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_animated_sprite2d_set_frame(LCComponentHandle component, int frame);

/* ============================================================================
 * Visual Properties
 * ============================================================================ */

/**
 * @brief Get the offset
 * @param component Component handle
 * @param out_offset Output parameter for offset
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_animated_sprite2d_get_offset(LCComponentHandle component, LCVec2* out_offset);

/**
 * @brief Set the offset
 * @param component Component handle
 * @param offset Offset value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_animated_sprite2d_set_offset(LCComponentHandle component, LCVec2 offset);

/**
 * @brief Get horizontal flip state
 * @param component Component handle
 * @param out_flip_h Output parameter for flip state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_animated_sprite2d_get_flip_h(LCComponentHandle component, bool* out_flip_h);

/**
 * @brief Set horizontal flip state
 * @param component Component handle
 * @param flip_h Whether to flip horizontally
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_animated_sprite2d_set_flip_h(LCComponentHandle component, bool flip_h);

/**
 * @brief Get vertical flip state
 * @param component Component handle
 * @param out_flip_v Output parameter for flip state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_animated_sprite2d_get_flip_v(LCComponentHandle component, bool* out_flip_v);

/**
 * @brief Set vertical flip state
 * @param component Component handle
 * @param flip_v Whether to flip vertically
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_animated_sprite2d_set_flip_v(LCComponentHandle component, bool flip_v);

/**
 * @brief Get the modulate color
 * @param component Component handle
 * @param out_color Output parameter for color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_animated_sprite2d_get_modulate(LCComponentHandle component, LCColor* out_color);

/**
 * @brief Set the modulate color
 * @param component Component handle
 * @param color Color to modulate the sprite with
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_animated_sprite2d_set_modulate(LCComponentHandle component, LCColor color);

/**
 * @brief Get pixel snap state
 * @param component Component handle
 * @param out_pixel_snap Output parameter for pixel snap state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_animated_sprite2d_get_pixel_snap(LCComponentHandle component, bool* out_pixel_snap);

/**
 * @brief Set pixel snap state (for crisp pixel art rendering)
 * @param component Component handle
 * @param pixel_snap Whether to snap to pixel grid
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_animated_sprite2d_set_pixel_snap(LCComponentHandle component, bool pixel_snap);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_ANIMATED_SPRITE2D_H */
