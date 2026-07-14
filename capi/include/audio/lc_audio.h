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
#include "../math/lc_math.h"


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

/* ============================================================================
 * Global Audio Control (AudioManager)
 *
 * These functions operate on the engine-wide audio mixer rather than on a
 * specific AudioPlayer component. Audio sources are addressed by their UUID
 * string (the same value reported by a node/component UUID).
 * ============================================================================ */

/**
 * @brief Set the playback volume of an active audio source by UUID
 * @param uuid UUID string of the audio source
 * @param volume Linear volume multiplier (1.0 = unchanged)
 * @threadsafety Thread-safe
 */
LC_API void lc_audio_set_source_volume(const char* uuid, float volume);

/**
 * @brief Set the playback pitch of an active audio source by UUID
 * @param uuid UUID string of the audio source
 * @param pitch Pitch multiplier (1.0 = normal)
 * @threadsafety Thread-safe
 */
LC_API void lc_audio_set_source_pitch(const char* uuid, float pitch);

/**
 * @brief Set the stereo pan of an active audio source by UUID
 * @param uuid UUID string of the audio source
 * @param pan Stereo pan (-1.0 left, 0.0 center, 1.0 right)
 * @threadsafety Thread-safe
 */
LC_API void lc_audio_set_source_pan(const char* uuid, float pan);

/**
 * @brief Set the master output volume
 * @param volume Linear master volume (0-1)
 * @threadsafety Thread-safe
 */
LC_API void lc_audio_set_master_volume(float volume);

/**
 * @brief Get the master output volume
 * @return Linear master volume
 * @threadsafety Thread-safe
 */
LC_API float lc_audio_get_master_volume(void);

/**
 * @brief Set whether the master output is muted
 * @param muted Muted state
 * @threadsafety Thread-safe
 */
LC_API void lc_audio_set_master_muted(bool muted);

/**
 * @brief Check whether the master output is muted
 * @return true if muted, false otherwise
 * @threadsafety Thread-safe
 */
LC_API bool lc_audio_is_master_muted(void);

/**
 * @brief Set the 3D audio listener position
 * @param position Listener world position
 * @threadsafety Thread-safe
 */
LC_API void lc_audio_set_listener_position(LCVec3 position);

/**
 * @brief Set the 3D audio listener orientation
 * @param forward Forward direction vector
 * @param up Up direction vector
 * @threadsafety Thread-safe
 */
LC_API void lc_audio_set_listener_orientation(LCVec3 forward, LCVec3 up);

/**
 * @brief Set the 3D audio listener velocity (for Doppler)
 * @param velocity Listener velocity vector
 * @threadsafety Thread-safe
 */
LC_API void lc_audio_set_listener_velocity(LCVec3 velocity);

/**
 * @brief Create an audio bus
 * @param name Name of the bus to create
 * @param parent Optional parent bus name (can be NULL or empty for the master bus)
 * @threadsafety Thread-safe
 */
LC_API void lc_audio_create_bus(const char* name, const char* parent);

/**
 * @brief Destroy an audio bus
 * @param name Name of the bus to destroy
 * @threadsafety Thread-safe
 */
LC_API void lc_audio_destroy_bus(const char* name);

/**
 * @brief Check whether an audio bus exists
 * @param name Bus name
 * @return true if the bus exists, false otherwise
 * @threadsafety Thread-safe
 */
LC_API bool lc_audio_has_bus(const char* name);

/**
 * @brief Set the solo state of an audio bus
 * @param name Bus name
 * @param solo Solo state
 * @threadsafety Thread-safe
 */
LC_API void lc_audio_set_bus_solo(const char* name, bool solo);

/**
 * @brief Check whether an audio bus is soloed
 * @param name Bus name
 * @return true if the bus is soloed, false otherwise
 * @threadsafety Thread-safe
 */
LC_API bool lc_audio_is_bus_solo(const char* name);

/* ============================================================================
 * Bus DSP Effects
 * ============================================================================ */

/**
 * @brief Append a DSP effect to a bus.
 * @param bus Bus name.
 * @param effect_type One of: "Gain", "LowPass", "HighPass", "BandPass", "Peak",
 *        "LowShelf", "HighShelf", "Delay", "Reverb", "Compressor",
 *        "Distortion", "Chorus".
 * @return The new effect's index, or -1 on failure.
 * @threadsafety Thread-safe
 */
LC_API int lc_audio_bus_add_effect(const char* bus, const char* effect_type);

/** @brief Remove the effect at `index` from a bus. */
LC_API void lc_audio_bus_remove_effect(const char* bus, int index);

/** @brief Reorder an effect within a bus's chain. */
LC_API void lc_audio_bus_move_effect(const char* bus, int from_index, int to_index);

/** @brief Remove all effects from a bus. */
LC_API void lc_audio_bus_clear_effects(const char* bus);

/** @brief Enable or disable a single effect. */
LC_API void lc_audio_bus_set_effect_enabled(const char* bus, int index, bool enabled);

/**
 * @brief Set a named parameter on an effect (e.g. "cutoff_hz", "room_size",
 *        "threshold_db", "wet").
 */
LC_API void lc_audio_bus_set_effect_parameter(const char* bus, int index,
                                              const char* parameter, float value);

/** @brief Number of effects on a bus. */
LC_API int lc_audio_bus_get_effect_count(const char* bus);

/** @brief Get a named parameter value from an effect. */
LC_API float lc_audio_bus_get_effect_parameter(const char* bus, int index, const char* parameter);

/** @brief Whether an effect is enabled. */
LC_API bool lc_audio_bus_is_effect_enabled(const char* bus, int index);

/** @brief Live post-fader/post-fx peak level of a bus (linear, 0..1+). */
LC_API float lc_audio_bus_get_level(const char* bus);

/**
 * @brief Live stereo VU levels of a bus.
 * @param out_left Receives the left-channel peak (may be NULL).
 * @param out_right Receives the right-channel peak (mirrors left for mono; may be NULL).
 */
LC_API void lc_audio_bus_get_levels(const char* bus, float* out_left, float* out_right);

/* ============================================================================
 * Fire-and-Forget Mixer Playback (AudioManager, by UUID)
 *
 * These functions play an audio file directly through the engine mixer without
 * an AudioPlayer component. The played asset is cached internally so it stays
 * alive for the lifetime of the process. Each play call returns the UUID string
 * of the spawned audio source, which can be passed back to the stop/pause/
 * resume/query functions below.
 * ============================================================================ */

/**
 * @brief Play an audio file through the mixer (2D, non-positional)
 * @param audio_path Path to the audio asset to play
 * @param bus_name Bus to route playback to (NULL defaults to "Master")
 * @param loop Whether to loop continuously (false plays once)
 * @param volume Linear volume multiplier (1.0 = unchanged)
 * @param out_uuid Output buffer for the source UUID string
 * @param uuid_size Size of the output buffer
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_play(const char* audio_path, const char* bus_name, bool loop,
                              float volume, char* out_uuid, size_t uuid_size);

/**
 * @brief Play an audio file through the mixer at a 3D position
 * @param audio_path Path to the audio asset to play
 * @param position World position of the sound
 * @param bus_name Bus to route playback to (NULL defaults to "Master")
 * @param loop Whether to loop continuously (false plays once)
 * @param volume Linear volume multiplier (1.0 = unchanged)
 * @param out_uuid Output buffer for the source UUID string
 * @param uuid_size Size of the output buffer
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_play_3d(const char* audio_path, LCVec3 position, const char* bus_name,
                                 bool loop, float volume, char* out_uuid, size_t uuid_size);

/**
 * @brief Play an audio file through the mixer after a scheduled delay (2D)
 * @param audio_path Path to the audio asset to play
 * @param delay_seconds Delay in seconds before playback begins
 * @param bus_name Bus to route playback to (NULL defaults to "Master")
 * @param loop Whether to loop continuously (false plays once)
 * @param volume Linear volume multiplier (1.0 = unchanged)
 * @param out_uuid Output buffer for the source UUID string
 * @param uuid_size Size of the output buffer
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_play_scheduled(const char* audio_path, float delay_seconds,
                                        const char* bus_name, bool loop, float volume,
                                        char* out_uuid, size_t uuid_size);

/**
 * @brief Play an audio file through the mixer after a scheduled delay at a 3D position
 * @param audio_path Path to the audio asset to play
 * @param position World position of the sound
 * @param delay_seconds Delay in seconds before playback begins
 * @param bus_name Bus to route playback to (NULL defaults to "Master")
 * @param loop Whether to loop continuously (false plays once)
 * @param volume Linear volume multiplier (1.0 = unchanged)
 * @param out_uuid Output buffer for the source UUID string
 * @param uuid_size Size of the output buffer
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_play_scheduled_3d(const char* audio_path, LCVec3 position,
                                           float delay_seconds, const char* bus_name, bool loop,
                                           float volume, char* out_uuid, size_t uuid_size);

/**
 * @brief Stop a mixer audio source by UUID
 * @param uuid UUID string of the audio source
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_stop(const char* uuid);

/**
 * @brief Pause a mixer audio source by UUID
 * @param uuid UUID string of the audio source
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_pause(const char* uuid);

/**
 * @brief Resume a paused mixer audio source by UUID
 * @param uuid UUID string of the audio source
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_resume(const char* uuid);

/**
 * @brief Check whether a mixer audio source is actively playing
 * @param uuid UUID string of the audio source
 * @param out_playing Output parameter for the playing state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_source_is_playing(const char* uuid, bool* out_playing);

/**
 * @brief Check whether a one-shot mixer audio source has finished
 * @param uuid UUID string of the audio source
 * @param out_finished Output parameter for the finished state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_source_is_finished(const char* uuid, bool* out_finished);

/**
 * @brief Set the volume of an audio bus
 * @param bus Bus name
 * @param volume Linear bus volume (0-1)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_set_bus_volume(const char* bus, float volume);

/**
 * @brief Get the volume of an audio bus
 * @param bus Bus name
 * @param out_volume Output parameter for the bus volume
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_get_bus_volume(const char* bus, float* out_volume);

/**
 * @brief Set whether an audio bus is muted
 * @param bus Bus name
 * @param muted Muted state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_set_bus_muted(const char* bus, bool muted);

/**
 * @brief Check whether an audio bus is muted
 * @param bus Bus name
 * @param out_muted Output parameter for the muted state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_audio_is_bus_muted(const char* bus, bool* out_muted);


#endif /* LUPINE_CAPI_AUDIO_H */
