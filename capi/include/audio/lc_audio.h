/**
 * @file lc_audio.h
 * @brief Lupine Engine C API - Audio system (AudioPlayer, AudioListener)
 *
 * This header provides audio functionality for 2D and 3D sound.
 */

#ifndef LUPINE_CAPI_AUDIO_H
#define LUPINE_CAPI_AUDIO_H

#include "core/lc_core.h"
#include "components/lc_light.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * AudioPlayer Functions
 * ============================================================================ */

/**
 * @brief Create an AudioPlayer component
 * @param name Optional name for the component (can be NULL)
 * @param out_component Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_player_create(const char* name, LCComponentHandle* out_component);

/**
 * @brief Play audio
 * @param component Component handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_player_play(LCComponentHandle component);

/**
 * @brief Stop audio playback
 * @param component Component handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_player_stop(LCComponentHandle component);

/**
 * @brief Pause audio playback
 * @param component Component handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_player_pause(LCComponentHandle component);

/**
 * @brief Resume audio playback
 * @param component Component handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_player_resume(LCComponentHandle component);

/**
 * @brief Check if audio is currently playing
 * @param component Component handle
 * @param out_playing Output parameter for playing state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_player_is_playing(LCComponentHandle component, bool* out_playing);

/**
 * @brief Get autoplay state
 * @param component Component handle
 * @param out_autoplay Output parameter for autoplay state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_player_get_autoplay(LCComponentHandle component, bool* out_autoplay);

/**
 * @brief Set autoplay state
 * @param component Component handle
 * @param autoplay Autoplay state (play on scene start)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_player_set_autoplay(LCComponentHandle component, bool autoplay);

/**
 * @brief Get loop state
 * @param component Component handle
 * @param out_loop Output parameter for loop state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_player_get_loop(LCComponentHandle component, bool* out_loop);

/**
 * @brief Set loop state
 * @param component Component handle
 * @param loop Loop state (repeat playback)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_player_set_loop(LCComponentHandle component, bool loop);

/**
 * @brief Get volume
 * @param component Component handle
 * @param out_volume Output parameter for volume (0-1)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_player_get_volume(LCComponentHandle component, float* out_volume);

/**
 * @brief Set volume
 * @param component Component handle
 * @param volume Volume level (0-1)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_player_set_volume(LCComponentHandle component, float volume);

/**
 * @brief Get pitch
 * @param component Component handle
 * @param out_pitch Output parameter for pitch (1.0 = normal)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_player_get_pitch(LCComponentHandle component, float* out_pitch);

/**
 * @brief Set pitch
 * @param component Component handle
 * @param pitch Pitch multiplier (1.0 = normal, 2.0 = double speed, 0.5 = half speed)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_player_set_pitch(LCComponentHandle component, float pitch);

/**
 * @brief Get stereo pan
 * @param component Component handle
 * @param out_pan Output parameter for pan (-1.0 left, 0.0 center, 1.0 right)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_player_get_pan(LCComponentHandle component, float* out_pan);

/**
 * @brief Set stereo pan
 * @param component Component handle
 * @param pan Stereo pan (-1.0 = left, 0.0 = center, 1.0 = right)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_player_set_pan(LCComponentHandle component, float pan);

/**
 * @brief Get audio bus name
 * @param component Component handle
 * @param out_bus Output buffer for bus name
 * @param bus_size Size of output buffer
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_player_get_bus(LCComponentHandle component, char* out_bus, size_t bus_size);

/**
 * @brief Set audio bus name
 * @param component Component handle
 * @param bus Bus name (for mixing, e.g., "Master", "SFX", "Music")
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_player_set_bus(LCComponentHandle component, const char* bus);

/**
 * @brief Check if audio is 3D
 * @param component Component handle
 * @param out_is3d Output parameter for 3D state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_player_get_is3d(LCComponentHandle component, bool* out_is3d);

/**
 * @brief Set whether audio is 3D
 * @param component Component handle
 * @param is3d 3D audio (attenuated by distance)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_player_set_is3d(LCComponentHandle component, bool is3d);

/**
 * @brief Get minimum distance for 3D audio
 * @param component Component handle
 * @param out_distance Output parameter for minimum distance
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_player_get_min_distance(LCComponentHandle component, float* out_distance);

/**
 * @brief Set minimum distance for 3D audio
 * @param component Component handle
 * @param distance Minimum distance (full volume within this distance)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_player_set_min_distance(LCComponentHandle component, float distance);

/**
 * @brief Get maximum distance for 3D audio
 * @param component Component handle
 * @param out_distance Output parameter for maximum distance
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_player_get_max_distance(LCComponentHandle component, float* out_distance);

/**
 * @brief Set maximum distance for 3D audio
 * @param component Component handle
 * @param distance Maximum distance (silent beyond this distance)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_player_set_max_distance(LCComponentHandle component, float distance);

/**
 * @brief Get rolloff factor for 3D audio
 * @param component Component handle
 * @param out_factor Output parameter for rolloff factor
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_player_get_rolloff_factor(LCComponentHandle component, float* out_factor);

/**
 * @brief Set rolloff factor for 3D audio
 * @param component Component handle
 * @param factor Rolloff factor (controls attenuation curve, 1.0 = linear)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_player_set_rolloff_factor(LCComponentHandle component, float factor);

/* ============================================================================
 * AudioListener Functions
 * ============================================================================ */

/**
 * @brief Create an AudioListener component
 * @param name Optional name for the component (can be NULL)
 * @param out_component Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_listener_create(const char* name, LCComponentHandle* out_component);

/**
 * @brief Check if audio listener is active
 * @param component Component handle
 * @param out_active Output parameter for active state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_listener_is_active(LCComponentHandle component, bool* out_active);

/**
 * @brief Set whether audio listener is active
 * @param component Component handle
 * @param active Active state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_listener_set_active(LCComponentHandle component, bool active);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_AUDIO_H */
