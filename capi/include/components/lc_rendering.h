/**
 * @file lc_rendering.h
 * @brief Lupine Engine C API - Rendering components (Sprite2D, Sprite3D, StaticMesh3D)
 *
 * This header provides rendering components for 2D/3D sprites and 3D meshes.
 */

#ifndef LUPINE_CAPI_RENDERING_H
#define LUPINE_CAPI_RENDERING_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"
#include "components/lc_light.h"


/* ============================================================================
 * Sprite2D Functions
 * ============================================================================ */

/**
 * @brief Create a Sprite2D component
 * @param name Optional name for the component (can be NULL)
 * @param out_component Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite2d_create(const char* name, LCComponentHandle* out_component);

/**
 * @brief Load texture from file path
 * @param component Component handle
 * @param filepath Path to texture file
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite2d_load_texture(LCComponentHandle component, const char* filepath);

/**
 * @brief Get the texture path of a Sprite2D
 * @param component Component handle
 * @param out_path Output buffer for texture path
 * @param path_size Size of output buffer
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite2d_get_texture_path(LCComponentHandle component, char* out_path, size_t path_size);

/**
 * @brief Set the texture path of a Sprite2D
 * @param component Component handle
 * @param path Path to texture file
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite2d_set_texture_path(LCComponentHandle component, const char* path);

/**
 * @brief Get the modulate color of a Sprite2D
 * @param component Component handle
 * @param out_color Output parameter for color
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite2d_get_modulate(LCComponentHandle component, LCColor* out_color);

/**
 * @brief Set the modulate color of a Sprite2D
 * @param component Component handle
 * @param color Color to set
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite2d_set_modulate(LCComponentHandle component, LCColor color);

/**
 * @brief Get the size of a Sprite2D
 * @param component Component handle
 * @param out_size Output parameter for size
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite2d_get_size(LCComponentHandle component, LCVec2* out_size);

/**
 * @brief Set the size of a Sprite2D
 * @param component Component handle
 * @param size Size to set (if 0,0 uses texture dimensions)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite2d_set_size(LCComponentHandle component, LCVec2 size);

/**
 * @brief Get the UV rectangle of a Sprite2D
 * @param component Component handle
 * @param out_uv_rect Output parameter for UV rect (x,y,w,h in 0-1 range)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite2d_get_uv_rect(LCComponentHandle component, LCVec4* out_uv_rect);

/**
 * @brief Set the UV rectangle of a Sprite2D
 * @param component Component handle
 * @param uv_rect UV rectangle (x,y,w,h in 0-1 range)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite2d_set_uv_rect(LCComponentHandle component, LCVec4 uv_rect);

/**
 * @brief Get the alpha cutoff of a Sprite2D
 * @param component Component handle
 * @param out_cutoff Output parameter for alpha cutoff
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite2d_get_alpha_cutoff(LCComponentHandle component, float* out_cutoff);

/**
 * @brief Set the alpha cutoff of a Sprite2D
 * @param component Component handle
 * @param cutoff Alpha cutoff value (0-1)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite2d_set_alpha_cutoff(LCComponentHandle component, float cutoff);

/**
 * @brief Check if a Sprite2D is flipped horizontally
 * @param component Component handle
 * @param out_flip Output parameter for flip state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite2d_get_flip_h(LCComponentHandle component, bool* out_flip);

/**
 * @brief Set whether a Sprite2D is flipped horizontally
 * @param component Component handle
 * @param flip Flip state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite2d_set_flip_h(LCComponentHandle component, bool flip);

/**
 * @brief Check if a Sprite2D is flipped vertically
 * @param component Component handle
 * @param out_flip Output parameter for flip state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite2d_get_flip_v(LCComponentHandle component, bool* out_flip);

/**
 * @brief Set whether a Sprite2D is flipped vertically
 * @param component Component handle
 * @param flip Flip state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite2d_set_flip_v(LCComponentHandle component, bool flip);

/**
 * @brief Check if a Sprite2D is centered
 * @param component Component handle
 * @param out_centered Output parameter for centered state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite2d_get_centered(LCComponentHandle component, bool* out_centered);

/**
 * @brief Set whether a Sprite2D is centered
 * @param component Component handle
 * @param centered Centered state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite2d_set_centered(LCComponentHandle component, bool centered);

/**
 * @brief Get the offset of a Sprite2D
 * @param component Component handle
 * @param out_offset Output parameter for offset
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite2d_get_offset(LCComponentHandle component, LCVec2* out_offset);

/**
 * @brief Set the offset of a Sprite2D
 * @param component Component handle
 * @param offset Offset to set (when not centered)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite2d_set_offset(LCComponentHandle component, LCVec2 offset);

/* ============================================================================
 * Sprite3D Functions
 * ============================================================================ */

/**
 * @brief Billboard mode for Sprite3D
 */
typedef enum {
    LC_BILLBOARD_DISABLED = 0,  // No billboarding
    LC_BILLBOARD_ENABLED = 1,   // Full billboarding
    LC_BILLBOARD_Y_AXIS_ONLY = 2  // Billboard only around Y axis
} LCBillboardMode;

/**
 * @brief Create a Sprite3D component
 * @param name Optional name for the component (can be NULL)
 * @param out_component Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite3d_create(const char* name, LCComponentHandle* out_component);

/**
 * @brief Load texture from file path
 * @param component Component handle
 * @param filepath Path to texture file
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite3d_load_texture(LCComponentHandle component, const char* filepath);

/**
 * @brief Get the texture path of a Sprite3D
 * @param component Component handle
 * @param out_path Output buffer for texture path
 * @param path_size Size of output buffer
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite3d_get_texture_path(LCComponentHandle component, char* out_path, size_t path_size);

/**
 * @brief Set the texture path of a Sprite3D
 * @param component Component handle
 * @param path Path to texture file
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite3d_set_texture_path(LCComponentHandle component, const char* path);

/**
 * @brief Get the modulate color of a Sprite3D
 * @param component Component handle
 * @param out_color Output parameter for color
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite3d_get_modulate(LCComponentHandle component, LCColor* out_color);

/**
 * @brief Set the modulate color of a Sprite3D
 * @param component Component handle
 * @param color Color to set
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite3d_set_modulate(LCComponentHandle component, LCColor color);

/**
 * @brief Get the size of a Sprite3D (in world units)
 * @param component Component handle
 * @param out_size Output parameter for size
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite3d_get_size(LCComponentHandle component, LCVec2* out_size);

/**
 * @brief Set the size of a Sprite3D (in world units)
 * @param component Component handle
 * @param size Size to set
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite3d_set_size(LCComponentHandle component, LCVec2 size);

/**
 * @brief Get the UV rectangle of a Sprite3D
 * @param component Component handle
 * @param out_uv_rect Output parameter for UV rect (x,y,w,h in 0-1 range)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite3d_get_uv_rect(LCComponentHandle component, LCVec4* out_uv_rect);

/**
 * @brief Set the UV rectangle of a Sprite3D
 * @param component Component handle
 * @param uv_rect UV rectangle (x,y,w,h in 0-1 range)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite3d_set_uv_rect(LCComponentHandle component, LCVec4 uv_rect);

/**
 * @brief Get the alpha cutoff of a Sprite3D
 * @param component Component handle
 * @param out_cutoff Output parameter for alpha cutoff
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite3d_get_alpha_cutoff(LCComponentHandle component, float* out_cutoff);

/**
 * @brief Set the alpha cutoff of a Sprite3D
 * @param component Component handle
 * @param cutoff Alpha cutoff value (0-1)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite3d_set_alpha_cutoff(LCComponentHandle component, float cutoff);

/**
 * @brief Check if a Sprite3D is flipped horizontally
 * @param component Component handle
 * @param out_flip Output parameter for flip state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite3d_get_flip_h(LCComponentHandle component, bool* out_flip);

/**
 * @brief Set whether a Sprite3D is flipped horizontally
 * @param component Component handle
 * @param flip Flip state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite3d_set_flip_h(LCComponentHandle component, bool flip);

/**
 * @brief Check if a Sprite3D is flipped vertically
 * @param component Component handle
 * @param out_flip Output parameter for flip state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite3d_get_flip_v(LCComponentHandle component, bool* out_flip);

/**
 * @brief Set whether a Sprite3D is flipped vertically
 * @param component Component handle
 * @param flip Flip state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite3d_set_flip_v(LCComponentHandle component, bool flip);

/**
 * @brief Get the billboard mode of a Sprite3D
 * @param component Component handle
 * @param out_mode Output parameter for billboard mode
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite3d_get_billboard_mode(LCComponentHandle component, LCBillboardMode* out_mode);

/**
 * @brief Set the billboard mode of a Sprite3D
 * @param component Component handle
 * @param mode Billboard mode
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite3d_set_billboard_mode(LCComponentHandle component, LCBillboardMode mode);

/**
 * @brief Check if a Sprite3D is double-sided
 * @param component Component handle
 * @param out_double_sided Output parameter for double-sided state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite3d_get_double_sided(LCComponentHandle component, bool* out_double_sided);

/**
 * @brief Set whether a Sprite3D is double-sided
 * @param component Component handle
 * @param double_sided Double-sided rendering
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite3d_set_double_sided(LCComponentHandle component, bool double_sided);

/**
 * @brief Check if a Sprite3D casts shadows
 * @param component Component handle
 * @param out_cast_shadow Output parameter for shadow casting state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite3d_get_cast_shadow(LCComponentHandle component, bool* out_cast_shadow);

/**
 * @brief Set whether a Sprite3D casts shadows
 * @param component Component handle
 * @param cast_shadow Shadow casting enabled
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite3d_set_cast_shadow(LCComponentHandle component, bool cast_shadow);

/**
 * @brief Check if a Sprite3D receives shadows
 * @param component Component handle
 * @param out_receive_shadow Output parameter for shadow receiving state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite3d_get_receive_shadow(LCComponentHandle component, bool* out_receive_shadow);

/**
 * @brief Set whether a Sprite3D receives shadows
 * @param component Component handle
 * @param receive_shadow Shadow receiving enabled
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_sprite3d_set_receive_shadow(LCComponentHandle component, bool receive_shadow);

/* ============================================================================
 * StaticMesh3D Functions
 * ============================================================================ */

/**
 * @brief Create a StaticMesh3D component
 * @param name Optional name for the component (can be NULL)
 * @param out_component Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_static_mesh3d_create(const char* name, LCComponentHandle* out_component);

/**
 * @brief Load model from file path
 * @param component Component handle
 * @param filepath Path to model file (GLTF, FBX, OBJ)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_static_mesh3d_load_model(LCComponentHandle component, const char* filepath);

/**
 * @brief Get the model path of a StaticMesh3D
 * @param component Component handle
 * @param out_path Output buffer for model path
 * @param path_size Size of output buffer
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_static_mesh3d_get_model_path(LCComponentHandle component, char* out_path, size_t path_size);

/**
 * @brief Set the model path of a StaticMesh3D
 * @param component Component handle
 * @param path Path to model file
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_static_mesh3d_set_model_path(LCComponentHandle component, const char* path);

/**
 * @brief Check if a StaticMesh3D casts shadows
 * @param component Component handle
 * @param out_cast_shadow Output parameter for shadow casting state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_static_mesh3d_get_cast_shadow(LCComponentHandle component, bool* out_cast_shadow);

/**
 * @brief Set whether a StaticMesh3D casts shadows
 * @param component Component handle
 * @param cast_shadow Shadow casting enabled
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_static_mesh3d_set_cast_shadow(LCComponentHandle component, bool cast_shadow);

/**
 * @brief Check if a StaticMesh3D receives shadows
 * @param component Component handle
 * @param out_receive_shadow Output parameter for shadow receiving state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_static_mesh3d_get_receive_shadow(LCComponentHandle component, bool* out_receive_shadow);

/**
 * @brief Set whether a StaticMesh3D receives shadows
 * @param component Component handle
 * @param receive_shadow Shadow receiving enabled
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_static_mesh3d_set_receive_shadow(LCComponentHandle component, bool receive_shadow);

/**
 * @brief Check if a StaticMesh3D is double-sided
 * @param component Component handle
 * @param out_double_sided Output parameter for double-sided state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_static_mesh3d_get_double_sided(LCComponentHandle component, bool* out_double_sided);

/**
 * @brief Set whether a StaticMesh3D is double-sided
 * @param component Component handle
 * @param double_sided Double-sided rendering
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_static_mesh3d_set_double_sided(LCComponentHandle component, bool double_sided);


#endif /* LUPINE_CAPI_RENDERING_H */
