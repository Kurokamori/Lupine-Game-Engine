/**
 * @file lc_skeletal_mesh3d.h
 * @brief Lupine Engine C API - SkeletalMesh3D component
 *
 * This header provides functions for 3D skeletal mesh rendering with animation support.
 * Supports FBX and GLTF models with skeletons and animations.
 */

#ifndef LUPINE_CAPI_SKELETAL_MESH3D_H
#define LUPINE_CAPI_SKELETAL_MESH3D_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * SkeletalMesh3D Creation
 * ============================================================================ */

/**
 * @brief Create a SkeletalMesh3D component
 * @param name Optional name for the component (can be NULL)
 * @param out_component Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_create(const char* name, LCComponentHandle* out_component);

/* ============================================================================
 * Model Loading
 * ============================================================================ */

/**
 * @brief Get the model file path
 * @param component Component handle
 * @param out_path Output buffer for file path
 * @param path_size Size of output buffer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_get_model_path(LCComponentHandle component, char* out_path, int path_size);

/**
 * @brief Set the model file path and load the model
 * @param component Component handle
 * @param filepath Path to the model file (FBX, GLTF, etc.)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_set_model_path(LCComponentHandle component, const char* filepath);

/**
 * @brief Load a model from file path
 * @param component Component handle
 * @param filepath Path to the model file
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_load_model(LCComponentHandle component, const char* filepath);

/* ============================================================================
 * Animation Information
 * ============================================================================ */

/**
 * @brief Get the number of animations in the loaded model
 * @param component Component handle
 * @param out_count Output parameter for animation count
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_get_animation_count(LCComponentHandle component, int* out_count);

/**
 * @brief Get the name of an animation by index
 * @param component Component handle
 * @param index Animation index
 * @param out_name Output buffer for animation name
 * @param name_size Size of output buffer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_get_animation_name_at(LCComponentHandle component, int index, char* out_name, int name_size);

/**
 * @brief Get the current animation name
 * @param component Component handle
 * @param out_name Output buffer for animation name
 * @param name_size Size of output buffer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_get_current_animation(LCComponentHandle component, char* out_name, int name_size);

/**
 * @brief Set the current animation by name
 * @param component Component handle
 * @param name Animation name
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_set_current_animation(LCComponentHandle component, const char* name);

/**
 * @brief Get the default animation name
 * @param component Component handle
 * @param out_name Output buffer for animation name
 * @param name_size Size of output buffer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_get_default_animation(LCComponentHandle component, char* out_name, int name_size);

/**
 * @brief Set the default animation name (played on auto-play)
 * @param component Component handle
 * @param name Animation name
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_set_default_animation(LCComponentHandle component, const char* name);

/* ============================================================================
 * Playback Control
 * ============================================================================ */

/**
 * @brief Play an animation by name
 * @param component Component handle
 * @param animation_name Animation name to play
 * @param loop Whether to loop the animation
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_play_animation(LCComponentHandle component, const char* animation_name, bool loop);

/**
 * @brief Stop the current animation
 * @param component Component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_stop_animation(LCComponentHandle component);

/**
 * @brief Check if an animation is currently playing
 * @param component Component handle
 * @param out_playing Output parameter for playing state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_is_playing(LCComponentHandle component, bool* out_playing);

/**
 * @brief Set the playing state
 * @param component Component handle
 * @param playing Whether animation should be playing
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_set_playing(LCComponentHandle component, bool playing);

/**
 * @brief Get the current animation time
 * @param component Component handle
 * @param out_time Output parameter for animation time
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_get_animation_time(LCComponentHandle component, float* out_time);

/**
 * @brief Set the animation time (seek)
 * @param component Component handle
 * @param time Animation time to seek to
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_set_animation_time(LCComponentHandle component, float time);

/* ============================================================================
 * Playback Properties
 * ============================================================================ */

/**
 * @brief Get the playback speed
 * @param component Component handle
 * @param out_speed Output parameter for playback speed
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_get_playback_speed(LCComponentHandle component, float* out_speed);

/**
 * @brief Set the playback speed (1.0 = normal, 2.0 = double speed)
 * @param component Component handle
 * @param speed Playback speed multiplier
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_set_playback_speed(LCComponentHandle component, float speed);

/**
 * @brief Get the loop state
 * @param component Component handle
 * @param out_loop Output parameter for loop state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_get_loop(LCComponentHandle component, bool* out_loop);

/**
 * @brief Set the loop state
 * @param component Component handle
 * @param loop Whether animation should loop
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_set_loop(LCComponentHandle component, bool loop);

/**
 * @brief Get the auto-play state
 * @param component Component handle
 * @param out_auto_play Output parameter for auto-play state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_get_auto_play(LCComponentHandle component, bool* out_auto_play);

/**
 * @brief Set the auto-play state
 * @param component Component handle
 * @param auto_play Whether default animation should auto-play on ready
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_set_auto_play(LCComponentHandle component, bool auto_play);

/* ============================================================================
 * GPU Skinning
 * ============================================================================ */

/**
 * @brief Get the GPU skinning state
 * @param component Component handle
 * @param out_gpu_skinning Output parameter for GPU skinning state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_get_gpu_skinning(LCComponentHandle component, bool* out_gpu_skinning);

/**
 * @brief Set the GPU skinning state
 * @param component Component handle
 * @param gpu_skinning Whether to use GPU skinning (faster)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_set_gpu_skinning(LCComponentHandle component, bool gpu_skinning);

/* ============================================================================
 * Root Motion
 * ============================================================================ */

/**
 * @brief Get the root motion enabled state
 * @param component Component handle
 * @param out_enabled Output parameter for root motion state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_get_root_motion_enabled(LCComponentHandle component, bool* out_enabled);

/**
 * @brief Set the root motion enabled state
 * @param component Component handle
 * @param enabled Whether root motion is enabled
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_set_root_motion_enabled(LCComponentHandle component, bool enabled);

/**
 * @brief Get the root bone name
 * @param component Component handle
 * @param out_name Output buffer for bone name
 * @param name_size Size of output buffer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_get_root_bone_name(LCComponentHandle component, char* out_name, int name_size);

/**
 * @brief Set the root bone name for root motion
 * @param component Component handle
 * @param name Name of the root bone
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_set_root_bone_name(LCComponentHandle component, const char* name);

/* ============================================================================
 * Shadow Settings
 * ============================================================================ */

/**
 * @brief Get cast shadow state
 * @param component Component handle
 * @param out_cast Output parameter for cast shadow state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_get_cast_shadow(LCComponentHandle component, bool* out_cast);

/**
 * @brief Set cast shadow state
 * @param component Component handle
 * @param cast Whether to cast shadows
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_set_cast_shadow(LCComponentHandle component, bool cast);

/**
 * @brief Get receive shadow state
 * @param component Component handle
 * @param out_receive Output parameter for receive shadow state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_get_receive_shadow(LCComponentHandle component, bool* out_receive);

/**
 * @brief Set receive shadow state
 * @param component Component handle
 * @param receive Whether to receive shadows
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_set_receive_shadow(LCComponentHandle component, bool receive);

/* ============================================================================
 * Material Slots
 * ============================================================================ */

/**
 * @brief Get the number of material slots
 * @param component Component handle
 * @param out_count Output parameter for slot count
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_get_material_slot_count(LCComponentHandle component, int* out_count);

/**
 * @brief Get the name of a material slot
 * @param component Component handle
 * @param slot_index Material slot index
 * @param out_name Output buffer for slot name
 * @param name_size Size of output buffer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_get_material_slot_name(LCComponentHandle component, int slot_index, char* out_name, int name_size);

/**
 * @brief Check if material override is enabled for a slot
 * @param component Component handle
 * @param slot_index Material slot index
 * @param out_enabled Output parameter for override enabled state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_get_material_slot_override_enabled(LCComponentHandle component, int slot_index, bool* out_enabled);

/**
 * @brief Enable or disable material override for a slot
 * @param component Component handle
 * @param slot_index Material slot index
 * @param enabled Whether to enable override
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_set_material_slot_override_enabled(LCComponentHandle component, int slot_index, bool enabled);

/**
 * @brief Get the albedo color of a material slot
 * @param component Component handle
 * @param slot_index Material slot index
 * @param out_color Output parameter for albedo color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_get_material_slot_albedo_color(LCComponentHandle component, int slot_index, LCColor* out_color);

/**
 * @brief Set the albedo color of a material slot
 * @param component Component handle
 * @param slot_index Material slot index
 * @param color Albedo color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_set_material_slot_albedo_color(LCComponentHandle component, int slot_index, LCColor color);

/**
 * @brief Set the albedo texture of a material slot
 * @param component Component handle
 * @param slot_index Material slot index
 * @param texture_path Path to albedo texture
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_set_material_slot_albedo_texture(LCComponentHandle component, int slot_index, const char* texture_path);

/**
 * @brief Get the metallic value of a material slot
 * @param component Component handle
 * @param slot_index Material slot index
 * @param out_metallic Output parameter for metallic value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_get_material_slot_metallic(LCComponentHandle component, int slot_index, float* out_metallic);

/**
 * @brief Set the metallic value of a material slot
 * @param component Component handle
 * @param slot_index Material slot index
 * @param metallic Metallic value (0-1)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_set_material_slot_metallic(LCComponentHandle component, int slot_index, float metallic);

/**
 * @brief Get the roughness value of a material slot
 * @param component Component handle
 * @param slot_index Material slot index
 * @param out_roughness Output parameter for roughness value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_get_material_slot_roughness(LCComponentHandle component, int slot_index, float* out_roughness);

/**
 * @brief Set the roughness value of a material slot
 * @param component Component handle
 * @param slot_index Material slot index
 * @param roughness Roughness value (0-1)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_skeletal_mesh3d_set_material_slot_roughness(LCComponentHandle component, int slot_index, float roughness);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_SKELETAL_MESH3D_H */
