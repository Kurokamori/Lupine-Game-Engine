/**
 * @file lc_world_environment.h
 * @brief Lupine Engine C API - WorldEnvironment component
 *
 * This header provides global environment settings including
 * skybox, fog, and ambient lighting for scenes.
 */

#ifndef LUPINE_CAPI_WORLD_ENVIRONMENT_H
#define LUPINE_CAPI_WORLD_ENVIRONMENT_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Skybox rendering types
 */
typedef enum LCSkyboxType {
    LC_SKYBOX_NONE = 0,        /**< No skybox */
    LC_SKYBOX_COLOR = 1,       /**< Solid color background */
    LC_SKYBOX_PROCEDURAL = 2,  /**< Procedural gradient sky */
    LC_SKYBOX_CUBEMAP = 3,     /**< 6-face cubemap texture */
    LC_SKYBOX_PANORAMIC = 4    /**< Panoramic/equirectangular texture */
} LCSkyboxType;

/**
 * @brief Fog calculation modes
 */
typedef enum LCFogMode {
    LC_FOG_LINEAR = 0,              /**< Linear fog falloff */
    LC_FOG_EXPONENTIAL = 1,         /**< Exponential fog falloff */
    LC_FOG_EXPONENTIAL_SQUARED = 2  /**< Exponential squared fog falloff */
} LCFogMode;

/**
 * @brief Tonemapping operators for the post-process chain
 */
typedef enum LCTonemapMode {
    LC_TONEMAP_LINEAR = 0,            /**< No tonemapping (clamp only) */
    LC_TONEMAP_REINHARD = 1,          /**< Reinhard */
    LC_TONEMAP_REINHARD_EXTENDED = 2, /**< Reinhard with white point */
    LC_TONEMAP_ACES = 3,              /**< ACES filmic approximation */
    LC_TONEMAP_FILMIC = 4,            /**< Uncharted 2 filmic */
    LC_TONEMAP_AGX = 5                /**< AgX (neutral) */
} LCTonemapMode;

/**
 * @brief Overlay texture blend modes
 */
typedef enum LCOverlayBlendMode {
    LC_OVERLAY_NORMAL = 0,
    LC_OVERLAY_ADDITIVE = 1,
    LC_OVERLAY_MULTIPLY = 2,
    LC_OVERLAY_SCREEN = 3,
    LC_OVERLAY_OVERLAY = 4,
    LC_OVERLAY_SOFT_LIGHT = 5
} LCOverlayBlendMode;

/**
 * @brief Post-process scene vertical-flip override
 */
typedef enum LCFlipYMode {
    LC_FLIPY_AUTO = 0,
    LC_FLIPY_OFF = 1,
    LC_FLIPY_ON = 2
} LCFlipYMode;

/* ============================================================================
 * WorldEnvironment Creation
 * ============================================================================ */

/**
 * @brief Create a WorldEnvironment component
 * @param name Optional name for the component (can be NULL)
 * @param out_component Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_world_environment_create(const char* name, LCComponentHandle* out_component);

/* ============================================================================
 * Skybox Settings
 * ============================================================================ */

/**
 * @brief Get the skybox type
 * @param component Component handle
 * @param out_type Output parameter for skybox type
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_world_environment_get_skybox_type(LCComponentHandle component, LCSkyboxType* out_type);

/**
 * @brief Set the skybox type
 * @param component Component handle
 * @param type Skybox type
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_world_environment_set_skybox_type(LCComponentHandle component, LCSkyboxType type);

/**
 * @brief Get the skybox solid color
 * @param component Component handle
 * @param out_color Output parameter for color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_world_environment_get_skybox_color(LCComponentHandle component, LCColor* out_color);

/**
 * @brief Set the skybox solid color
 * @param component Component handle
 * @param color Skybox color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_world_environment_set_skybox_color(LCComponentHandle component, LCColor color);

/* ============================================================================
 * Procedural Sky Gradient
 * ============================================================================ */

/**
 * @brief Get the procedural sky top color
 * @param component Component handle
 * @param out_color Output parameter for color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_world_environment_get_sky_top_color(LCComponentHandle component, LCColor* out_color);

/**
 * @brief Set the procedural sky top color
 * @param component Component handle
 * @param color Top color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_world_environment_set_sky_top_color(LCComponentHandle component, LCColor color);

/**
 * @brief Get the procedural sky horizon color
 * @param component Component handle
 * @param out_color Output parameter for color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_world_environment_get_sky_horizon_color(LCComponentHandle component, LCColor* out_color);

/**
 * @brief Set the procedural sky horizon color
 * @param component Component handle
 * @param color Horizon color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_world_environment_set_sky_horizon_color(LCComponentHandle component, LCColor color);

/**
 * @brief Get the procedural sky bottom color
 * @param component Component handle
 * @param out_color Output parameter for color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_world_environment_get_sky_bottom_color(LCComponentHandle component, LCColor* out_color);

/**
 * @brief Set the procedural sky bottom color
 * @param component Component handle
 * @param color Bottom color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_world_environment_set_sky_bottom_color(LCComponentHandle component, LCColor color);

/* ============================================================================
 * Cubemap Texture Paths
 * ============================================================================ */

/**
 * @brief Get cubemap positive X face texture path
 * @param component Component handle
 * @param out_path Output buffer for path
 * @param path_size Size of output buffer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_world_environment_get_cubemap_pos_x(LCComponentHandle component, char* out_path, size_t path_size);

/**
 * @brief Set cubemap positive X face texture path
 * @param component Component handle
 * @param path Texture path
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_world_environment_set_cubemap_pos_x(LCComponentHandle component, const char* path);

/**
 * @brief Get cubemap negative X face texture path
 */
LC_API LCResult lc_world_environment_get_cubemap_neg_x(LCComponentHandle component, char* out_path, size_t path_size);

/**
 * @brief Set cubemap negative X face texture path
 */
LC_API LCResult lc_world_environment_set_cubemap_neg_x(LCComponentHandle component, const char* path);

/**
 * @brief Get cubemap positive Y face texture path
 */
LC_API LCResult lc_world_environment_get_cubemap_pos_y(LCComponentHandle component, char* out_path, size_t path_size);

/**
 * @brief Set cubemap positive Y face texture path
 */
LC_API LCResult lc_world_environment_set_cubemap_pos_y(LCComponentHandle component, const char* path);

/**
 * @brief Get cubemap negative Y face texture path
 */
LC_API LCResult lc_world_environment_get_cubemap_neg_y(LCComponentHandle component, char* out_path, size_t path_size);

/**
 * @brief Set cubemap negative Y face texture path
 */
LC_API LCResult lc_world_environment_set_cubemap_neg_y(LCComponentHandle component, const char* path);

/**
 * @brief Get cubemap positive Z face texture path
 */
LC_API LCResult lc_world_environment_get_cubemap_pos_z(LCComponentHandle component, char* out_path, size_t path_size);

/**
 * @brief Set cubemap positive Z face texture path
 */
LC_API LCResult lc_world_environment_set_cubemap_pos_z(LCComponentHandle component, const char* path);

/**
 * @brief Get cubemap negative Z face texture path
 */
LC_API LCResult lc_world_environment_get_cubemap_neg_z(LCComponentHandle component, char* out_path, size_t path_size);

/**
 * @brief Set cubemap negative Z face texture path
 */
LC_API LCResult lc_world_environment_set_cubemap_neg_z(LCComponentHandle component, const char* path);

/* ============================================================================
 * Panoramic Texture
 * ============================================================================ */

/**
 * @brief Get panoramic texture path
 * @param component Component handle
 * @param out_path Output buffer for path
 * @param path_size Size of output buffer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_world_environment_get_panoramic_texture(LCComponentHandle component, char* out_path, size_t path_size);

/**
 * @brief Set panoramic texture path
 * @param component Component handle
 * @param path Texture path
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_world_environment_set_panoramic_texture(LCComponentHandle component, const char* path);

/* ============================================================================
 * Fog Settings
 * ============================================================================ */

/**
 * @brief Get whether fog is enabled
 * @param component Component handle
 * @param out_enabled Output parameter for enabled state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_world_environment_get_fog_enabled(LCComponentHandle component, bool* out_enabled);

/**
 * @brief Set whether fog is enabled
 * @param component Component handle
 * @param enabled Fog enabled state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_world_environment_set_fog_enabled(LCComponentHandle component, bool enabled);

/**
 * @brief Get the fog color
 * @param component Component handle
 * @param out_color Output parameter for color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_world_environment_get_fog_color(LCComponentHandle component, LCColor* out_color);

/**
 * @brief Set the fog color
 * @param component Component handle
 * @param color Fog color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_world_environment_set_fog_color(LCComponentHandle component, LCColor color);

/**
 * @brief Get the fog density
 * @param component Component handle
 * @param out_density Output parameter for density
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_world_environment_get_fog_density(LCComponentHandle component, float* out_density);

/**
 * @brief Set the fog density
 * @param component Component handle
 * @param density Fog density (typically 0.0-1.0)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_world_environment_set_fog_density(LCComponentHandle component, float density);

/**
 * @brief Get the fog start distance
 * @param component Component handle
 * @param out_start Output parameter for start distance
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_world_environment_get_fog_start(LCComponentHandle component, float* out_start);

/**
 * @brief Set the fog start distance
 * @param component Component handle
 * @param start Start distance
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_world_environment_set_fog_start(LCComponentHandle component, float start);

/**
 * @brief Get the fog end distance
 * @param component Component handle
 * @param out_end Output parameter for end distance
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_world_environment_get_fog_end(LCComponentHandle component, float* out_end);

/**
 * @brief Set the fog end distance
 * @param component Component handle
 * @param end End distance
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_world_environment_set_fog_end(LCComponentHandle component, float end);

/**
 * @brief Get the fog mode
 * @param component Component handle
 * @param out_mode Output parameter for fog mode
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_world_environment_get_fog_mode(LCComponentHandle component, LCFogMode* out_mode);

/**
 * @brief Set the fog mode
 * @param component Component handle
 * @param mode Fog mode
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_world_environment_set_fog_mode(LCComponentHandle component, LCFogMode mode);

/* ============================================================================
 * Ambient Light Settings
 * ============================================================================ */

/**
 * @brief Get whether ambient light is enabled
 * @param component Component handle
 * @param out_enabled Output parameter for enabled state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_world_environment_get_ambient_light_enabled(LCComponentHandle component, bool* out_enabled);

/**
 * @brief Set whether ambient light is enabled
 * @param component Component handle
 * @param enabled Ambient light enabled state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_world_environment_set_ambient_light_enabled(LCComponentHandle component, bool enabled);

/**
 * @brief Get the ambient light color
 * @param component Component handle
 * @param out_color Output parameter for color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_world_environment_get_ambient_light_color(LCComponentHandle component, LCColor* out_color);

/**
 * @brief Set the ambient light color
 * @param component Component handle
 * @param color Ambient light color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_world_environment_set_ambient_light_color(LCComponentHandle component, LCColor color);

/**
 * @brief Get the ambient light intensity
 * @param component Component handle
 * @param out_intensity Output parameter for intensity
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_world_environment_get_ambient_light_intensity(LCComponentHandle component, float* out_intensity);

/**
 * @brief Set the ambient light intensity
 * @param component Component handle
 * @param intensity Ambient light intensity (typically 0.0-10.0)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_world_environment_set_ambient_light_intensity(LCComponentHandle component, float intensity);

/* ============================================================================
 * Volumetric Fog (Vulkan only)
 * ============================================================================ */

/**
 * @brief Get whether volumetric fog is enabled
 * @param component Component handle
 * @param out_enabled Output parameter for enabled state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_world_environment_get_volumetric_fog_enabled(LCComponentHandle component, bool* out_enabled);

/**
 * @brief Set whether volumetric fog is enabled (Vulkan only)
 * @param component Component handle
 * @param enabled Volumetric fog enabled state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_world_environment_set_volumetric_fog_enabled(LCComponentHandle component, bool enabled);


/* ============================================================================
 * Post-Processing Settings
 * ============================================================================ */

LC_API LCResult lc_world_environment_get_post_processing_enabled(LCComponentHandle component, bool* out_value);
LC_API LCResult lc_world_environment_set_post_processing_enabled(LCComponentHandle component, bool value);

LC_API LCResult lc_world_environment_get_tonemap_mode(LCComponentHandle component, LCTonemapMode* out_value);
LC_API LCResult lc_world_environment_set_tonemap_mode(LCComponentHandle component, LCTonemapMode value);

LC_API LCResult lc_world_environment_get_exposure(LCComponentHandle component, float* out_value);
LC_API LCResult lc_world_environment_set_exposure(LCComponentHandle component, float value);

LC_API LCResult lc_world_environment_get_white_point(LCComponentHandle component, float* out_value);
LC_API LCResult lc_world_environment_set_white_point(LCComponentHandle component, float value);

LC_API LCResult lc_world_environment_get_post_flip_y(LCComponentHandle component, LCFlipYMode* out_value);
LC_API LCResult lc_world_environment_set_post_flip_y(LCComponentHandle component, LCFlipYMode value);

LC_API LCResult lc_world_environment_get_bloom_enabled(LCComponentHandle component, bool* out_value);
LC_API LCResult lc_world_environment_set_bloom_enabled(LCComponentHandle component, bool value);

LC_API LCResult lc_world_environment_get_bloom_threshold(LCComponentHandle component, float* out_value);
LC_API LCResult lc_world_environment_set_bloom_threshold(LCComponentHandle component, float value);

LC_API LCResult lc_world_environment_get_bloom_soft_knee(LCComponentHandle component, float* out_value);
LC_API LCResult lc_world_environment_set_bloom_soft_knee(LCComponentHandle component, float value);

LC_API LCResult lc_world_environment_get_bloom_intensity(LCComponentHandle component, float* out_value);
LC_API LCResult lc_world_environment_set_bloom_intensity(LCComponentHandle component, float value);

LC_API LCResult lc_world_environment_get_bloom_iterations(LCComponentHandle component, int* out_value);
LC_API LCResult lc_world_environment_set_bloom_iterations(LCComponentHandle component, int value);

LC_API LCResult lc_world_environment_get_ssao_enabled(LCComponentHandle component, bool* out_value);
LC_API LCResult lc_world_environment_set_ssao_enabled(LCComponentHandle component, bool value);

LC_API LCResult lc_world_environment_get_ssao_radius(LCComponentHandle component, float* out_value);
LC_API LCResult lc_world_environment_set_ssao_radius(LCComponentHandle component, float value);

LC_API LCResult lc_world_environment_get_ssao_intensity(LCComponentHandle component, float* out_value);
LC_API LCResult lc_world_environment_set_ssao_intensity(LCComponentHandle component, float value);

LC_API LCResult lc_world_environment_get_ssao_bias(LCComponentHandle component, float* out_value);
LC_API LCResult lc_world_environment_set_ssao_bias(LCComponentHandle component, float value);

LC_API LCResult lc_world_environment_get_ssao_samples(LCComponentHandle component, int* out_value);
LC_API LCResult lc_world_environment_set_ssao_samples(LCComponentHandle component, int value);

LC_API LCResult lc_world_environment_get_ssao_power(LCComponentHandle component, float* out_value);
LC_API LCResult lc_world_environment_set_ssao_power(LCComponentHandle component, float value);

LC_API LCResult lc_world_environment_get_color_grading_enabled(LCComponentHandle component, bool* out_value);
LC_API LCResult lc_world_environment_set_color_grading_enabled(LCComponentHandle component, bool value);

LC_API LCResult lc_world_environment_get_contrast(LCComponentHandle component, float* out_value);
LC_API LCResult lc_world_environment_set_contrast(LCComponentHandle component, float value);

LC_API LCResult lc_world_environment_get_saturation(LCComponentHandle component, float* out_value);
LC_API LCResult lc_world_environment_set_saturation(LCComponentHandle component, float value);

LC_API LCResult lc_world_environment_get_brightness(LCComponentHandle component, float* out_value);
LC_API LCResult lc_world_environment_set_brightness(LCComponentHandle component, float value);

LC_API LCResult lc_world_environment_get_temperature(LCComponentHandle component, float* out_value);
LC_API LCResult lc_world_environment_set_temperature(LCComponentHandle component, float value);

LC_API LCResult lc_world_environment_get_tint(LCComponentHandle component, float* out_value);
LC_API LCResult lc_world_environment_set_tint(LCComponentHandle component, float value);

LC_API LCResult lc_world_environment_get_color_filter(LCComponentHandle component, LCColor* out_color);
LC_API LCResult lc_world_environment_set_color_filter(LCComponentHandle component, LCColor value);

LC_API LCResult lc_world_environment_get_color_lift(LCComponentHandle component, LCColor* out_color);
LC_API LCResult lc_world_environment_set_color_lift(LCComponentHandle component, LCColor value);

LC_API LCResult lc_world_environment_get_color_gamma(LCComponentHandle component, LCColor* out_color);
LC_API LCResult lc_world_environment_set_color_gamma(LCComponentHandle component, LCColor value);

LC_API LCResult lc_world_environment_get_color_gain(LCComponentHandle component, LCColor* out_color);
LC_API LCResult lc_world_environment_set_color_gain(LCComponentHandle component, LCColor value);

LC_API LCResult lc_world_environment_get_vignette_enabled(LCComponentHandle component, bool* out_value);
LC_API LCResult lc_world_environment_set_vignette_enabled(LCComponentHandle component, bool value);

LC_API LCResult lc_world_environment_get_vignette_color(LCComponentHandle component, LCColor* out_color);
LC_API LCResult lc_world_environment_set_vignette_color(LCComponentHandle component, LCColor value);

LC_API LCResult lc_world_environment_get_vignette_intensity(LCComponentHandle component, float* out_value);
LC_API LCResult lc_world_environment_set_vignette_intensity(LCComponentHandle component, float value);

LC_API LCResult lc_world_environment_get_vignette_smoothness(LCComponentHandle component, float* out_value);
LC_API LCResult lc_world_environment_set_vignette_smoothness(LCComponentHandle component, float value);

LC_API LCResult lc_world_environment_get_vignette_roundness(LCComponentHandle component, float* out_value);
LC_API LCResult lc_world_environment_set_vignette_roundness(LCComponentHandle component, float value);

LC_API LCResult lc_world_environment_get_vignette_center_x(LCComponentHandle component, float* out_value);
LC_API LCResult lc_world_environment_set_vignette_center_x(LCComponentHandle component, float value);

LC_API LCResult lc_world_environment_get_vignette_center_y(LCComponentHandle component, float* out_value);
LC_API LCResult lc_world_environment_set_vignette_center_y(LCComponentHandle component, float value);

LC_API LCResult lc_world_environment_get_chromatic_aberration_enabled(LCComponentHandle component, bool* out_value);
LC_API LCResult lc_world_environment_set_chromatic_aberration_enabled(LCComponentHandle component, bool value);

LC_API LCResult lc_world_environment_get_chromatic_aberration_amount(LCComponentHandle component, float* out_value);
LC_API LCResult lc_world_environment_set_chromatic_aberration_amount(LCComponentHandle component, float value);

LC_API LCResult lc_world_environment_get_film_grain_enabled(LCComponentHandle component, bool* out_value);
LC_API LCResult lc_world_environment_set_film_grain_enabled(LCComponentHandle component, bool value);

LC_API LCResult lc_world_environment_get_film_grain_intensity(LCComponentHandle component, float* out_value);
LC_API LCResult lc_world_environment_set_film_grain_intensity(LCComponentHandle component, float value);

LC_API LCResult lc_world_environment_get_film_grain_size(LCComponentHandle component, float* out_value);
LC_API LCResult lc_world_environment_set_film_grain_size(LCComponentHandle component, float value);

LC_API LCResult lc_world_environment_get_overlay_texture(LCComponentHandle component, char* out_path, size_t path_size);
LC_API LCResult lc_world_environment_set_overlay_texture(LCComponentHandle component, const char* path);

LC_API LCResult lc_world_environment_get_overlay_blend_mode(LCComponentHandle component, LCOverlayBlendMode* out_value);
LC_API LCResult lc_world_environment_set_overlay_blend_mode(LCComponentHandle component, LCOverlayBlendMode value);

LC_API LCResult lc_world_environment_get_overlay_opacity(LCComponentHandle component, float* out_value);
LC_API LCResult lc_world_environment_set_overlay_opacity(LCComponentHandle component, float value);

#endif /* LUPINE_CAPI_WORLD_ENVIRONMENT_H */
