/**
 * @file lc_light.h
 * @brief Lupine Engine C API - 3D Lighting components
 *
 * This header provides lighting functionality for 3D scenes.
 * Includes directional lights, point lights (omni), and spotlights.
 */

#ifndef LUPINE_CAPI_LIGHT_H
#define LUPINE_CAPI_LIGHT_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Component Handle Type
 * ============================================================================ */

/**
 * @brief Opaque handle to a Component
 */
typedef struct LCComponent* LCComponentHandle;

/* ============================================================================
 * DirectionalLight3D Functions
 * ============================================================================ */

/**
 * @brief Create a DirectionalLight3D component
 * @param name Optional name for the component (can be NULL)
 * @param out_component Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_directional_light3d_create(const char* name, LCComponentHandle* out_component);

/**
 * @brief Get the color of a DirectionalLight3D
 * @param component Component handle
 * @param out_color Output parameter for color
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_directional_light3d_get_color(LCComponentHandle component, LCColor* out_color);

/**
 * @brief Set the color of a DirectionalLight3D
 * @param component Component handle
 * @param color Color to set
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_directional_light3d_set_color(LCComponentHandle component, LCColor color);

/**
 * @brief Get the intensity of a DirectionalLight3D
 * @param component Component handle
 * @param out_intensity Output parameter for intensity
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_directional_light3d_get_intensity(LCComponentHandle component, float* out_intensity);

/**
 * @brief Set the intensity of a DirectionalLight3D
 * @param component Component handle
 * @param intensity Intensity value (multiplier for color)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_directional_light3d_set_intensity(LCComponentHandle component, float intensity);

/**
 * @brief Check if a DirectionalLight3D is in negative mode
 * @param component Component handle
 * @param out_negative Output parameter for negative mode
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_directional_light3d_is_negative(LCComponentHandle component, bool* out_negative);

/**
 * @brief Set whether a DirectionalLight3D is in negative mode
 * @param component Component handle
 * @param negative Negative mode (subtracts light instead of adding)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_directional_light3d_set_negative(LCComponentHandle component, bool negative);

/**
 * @brief Check if a DirectionalLight3D casts shadows
 * @param component Component handle
 * @param out_casts_shadows Output parameter for shadow casting state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_directional_light3d_casts_shadows(LCComponentHandle component, bool* out_casts_shadows);

/**
 * @brief Set whether a DirectionalLight3D casts shadows
 * @param component Component handle
 * @param casts_shadows Shadow casting enabled
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_directional_light3d_set_casts_shadows(LCComponentHandle component, bool casts_shadows);

/**
 * @brief Get the shadow opacity of a DirectionalLight3D
 * @param component Component handle
 * @param out_opacity Output parameter for shadow opacity
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_directional_light3d_get_shadow_opacity(LCComponentHandle component, float* out_opacity);

/**
 * @brief Set the shadow opacity of a DirectionalLight3D
 * @param component Component handle
 * @param opacity Shadow opacity (0.0 = no shadow, 1.0 = full shadow)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_directional_light3d_set_shadow_opacity(LCComponentHandle component, float opacity);

/**
 * @brief Get the shadow bias of a DirectionalLight3D
 * @param component Component handle
 * @param out_bias Output parameter for shadow bias
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_directional_light3d_get_shadow_bias(LCComponentHandle component, float* out_bias);

/**
 * @brief Set the shadow bias of a DirectionalLight3D
 * @param component Component handle
 * @param bias Shadow bias (prevents shadow acne)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_directional_light3d_set_shadow_bias(LCComponentHandle component, float bias);

/* ============================================================================
 * OmniLight3D (Point Light) Functions
 * ============================================================================ */

/**
 * @brief Create an OmniLight3D component
 * @param name Optional name for the component (can be NULL)
 * @param out_component Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_omni_light3d_create(const char* name, LCComponentHandle* out_component);

/**
 * @brief Get the color of an OmniLight3D
 * @param component Component handle
 * @param out_color Output parameter for color
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_omni_light3d_get_color(LCComponentHandle component, LCColor* out_color);

/**
 * @brief Set the color of an OmniLight3D
 * @param component Component handle
 * @param color Color to set
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_omni_light3d_set_color(LCComponentHandle component, LCColor color);

/**
 * @brief Get the intensity of an OmniLight3D
 * @param component Component handle
 * @param out_intensity Output parameter for intensity
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_omni_light3d_get_intensity(LCComponentHandle component, float* out_intensity);

/**
 * @brief Set the intensity of an OmniLight3D
 * @param component Component handle
 * @param intensity Intensity value (multiplier for color)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_omni_light3d_set_intensity(LCComponentHandle component, float intensity);

/**
 * @brief Get the range of an OmniLight3D
 * @param component Component handle
 * @param out_range Output parameter for range
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_omni_light3d_get_range(LCComponentHandle component, float* out_range);

/**
 * @brief Set the range of an OmniLight3D
 * @param component Component handle
 * @param range Light range (distance at which light intensity reaches zero)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_omni_light3d_set_range(LCComponentHandle component, float range);

/**
 * @brief Get the attenuation of an OmniLight3D
 * @param component Component handle
 * @param out_attenuation Output parameter for attenuation
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_omni_light3d_get_attenuation(LCComponentHandle component, float* out_attenuation);

/**
 * @brief Set the attenuation of an OmniLight3D
 * @param component Component handle
 * @param attenuation Attenuation exponent (controls falloff curve)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_omni_light3d_set_attenuation(LCComponentHandle component, float attenuation);

/**
 * @brief Check if an OmniLight3D is in negative mode
 * @param component Component handle
 * @param out_negative Output parameter for negative mode
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_omni_light3d_is_negative(LCComponentHandle component, bool* out_negative);

/**
 * @brief Set whether an OmniLight3D is in negative mode
 * @param component Component handle
 * @param negative Negative mode (subtracts light instead of adding)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_omni_light3d_set_negative(LCComponentHandle component, bool negative);

/**
 * @brief Check if an OmniLight3D casts shadows
 * @param component Component handle
 * @param out_casts_shadows Output parameter for shadow casting state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_omni_light3d_casts_shadows(LCComponentHandle component, bool* out_casts_shadows);

/**
 * @brief Set whether an OmniLight3D casts shadows
 * @param component Component handle
 * @param casts_shadows Shadow casting enabled
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_omni_light3d_set_casts_shadows(LCComponentHandle component, bool casts_shadows);

/* ============================================================================
 * SpotLight3D Functions
 * ============================================================================ */

/**
 * @brief Create a SpotLight3D component
 * @param name Optional name for the component (can be NULL)
 * @param out_component Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_spot_light3d_create(const char* name, LCComponentHandle* out_component);

/**
 * @brief Get the color of a SpotLight3D
 * @param component Component handle
 * @param out_color Output parameter for color
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_spot_light3d_get_color(LCComponentHandle component, LCColor* out_color);

/**
 * @brief Set the color of a SpotLight3D
 * @param component Component handle
 * @param color Color to set
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_spot_light3d_set_color(LCComponentHandle component, LCColor color);

/**
 * @brief Get the intensity of a SpotLight3D
 * @param component Component handle
 * @param out_intensity Output parameter for intensity
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_spot_light3d_get_intensity(LCComponentHandle component, float* out_intensity);

/**
 * @brief Set the intensity of a SpotLight3D
 * @param component Component handle
 * @param intensity Intensity value (multiplier for color)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_spot_light3d_set_intensity(LCComponentHandle component, float intensity);

/**
 * @brief Get the range of a SpotLight3D
 * @param component Component handle
 * @param out_range Output parameter for range
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_spot_light3d_get_range(LCComponentHandle component, float* out_range);

/**
 * @brief Set the range of a SpotLight3D
 * @param component Component handle
 * @param range Light range (distance at which light intensity reaches zero)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_spot_light3d_set_range(LCComponentHandle component, float range);

/**
 * @brief Get the attenuation of a SpotLight3D
 * @param component Component handle
 * @param out_attenuation Output parameter for attenuation
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_spot_light3d_get_attenuation(LCComponentHandle component, float* out_attenuation);

/**
 * @brief Set the attenuation of a SpotLight3D
 * @param component Component handle
 * @param attenuation Attenuation exponent (controls falloff curve)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_spot_light3d_set_attenuation(LCComponentHandle component, float attenuation);

/**
 * @brief Get the inner cone angle of a SpotLight3D
 * @param component Component handle
 * @param out_angle Output parameter for inner cone angle in degrees
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_spot_light3d_get_inner_cone_angle(LCComponentHandle component, float* out_angle);

/**
 * @brief Set the inner cone angle of a SpotLight3D
 * @param component Component handle
 * @param angle Inner cone angle in degrees (full brightness)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_spot_light3d_set_inner_cone_angle(LCComponentHandle component, float angle);

/**
 * @brief Get the outer cone angle of a SpotLight3D
 * @param component Component handle
 * @param out_angle Output parameter for outer cone angle in degrees
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_spot_light3d_get_outer_cone_angle(LCComponentHandle component, float* out_angle);

/**
 * @brief Set the outer cone angle of a SpotLight3D
 * @param component Component handle
 * @param angle Outer cone angle in degrees (light cutoff)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_spot_light3d_set_outer_cone_angle(LCComponentHandle component, float angle);

/**
 * @brief Check if a SpotLight3D is in negative mode
 * @param component Component handle
 * @param out_negative Output parameter for negative mode
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_spot_light3d_is_negative(LCComponentHandle component, bool* out_negative);

/**
 * @brief Set whether a SpotLight3D is in negative mode
 * @param component Component handle
 * @param negative Negative mode (subtracts light instead of adding)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_spot_light3d_set_negative(LCComponentHandle component, bool negative);

/**
 * @brief Check if a SpotLight3D casts shadows
 * @param component Component handle
 * @param out_casts_shadows Output parameter for shadow casting state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_spot_light3d_casts_shadows(LCComponentHandle component, bool* out_casts_shadows);

/**
 * @brief Set whether a SpotLight3D casts shadows
 * @param component Component handle
 * @param casts_shadows Shadow casting enabled
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_spot_light3d_set_casts_shadows(LCComponentHandle component, bool casts_shadows);

/* ============================================================================
 * Component Management
 * ============================================================================ */

/**
 * @brief Destroy a component handle
 * @param component Component handle to destroy
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_component_destroy(LCComponentHandle component);

/**
 * @brief Add a component to a node
 * @param node Node handle to add component to
 * @param component Component handle to add
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node_add_component(LCNodeHandle node, LCComponentHandle component);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_LIGHT_H */
