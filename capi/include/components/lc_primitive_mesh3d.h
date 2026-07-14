/**
 * @file lc_primitive_mesh3d.h
 * @brief Lupine Engine C API - PrimitiveMesh3D component
 *
 * This header provides procedural 3D primitive mesh rendering with
 * configurable shapes, materials, and shader support.
 */

#ifndef LUPINE_CAPI_PRIMITIVE_MESH3D_H
#define LUPINE_CAPI_PRIMITIVE_MESH3D_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

/**
 * @brief Primitive shape types
 */
typedef enum LCPrimitiveShape {
    LC_PRIMITIVE_CUBE = 0,
    LC_PRIMITIVE_SPHERE,
    LC_PRIMITIVE_CYLINDER,
    LC_PRIMITIVE_CONE,
    LC_PRIMITIVE_PYRAMID,
    LC_PRIMITIVE_TORUS,
    LC_PRIMITIVE_CAPSULE
} LCPrimitiveShape;

/**
 * @brief Shader types for rendering
 */
#ifndef LC_SHADER_TYPE_DEFINED
#define LC_SHADER_TYPE_DEFINED
typedef enum LCShaderType {
    LC_SHADER_PBR = 0,       /**< Physically-based rendering (default) */
    LC_SHADER_TOON,          /**< Cel/toon shading */
    LC_SHADER_STYLIZED,      /**< Fantasy-style soft shading */
    LC_SHADER_UNLIT,         /**< No lighting calculations */
    LC_SHADER_STANDARD3D,    /**< Basic lit shader */
    LC_SHADER_TRANSPARENT,   /**< Transparent/Glass with refraction */
    LC_SHADER_GLOW,          /**< Emissive glow effects */
    LC_SHADER_CUSTOM         /**< User-provided custom shader */
} LCShaderType;
#endif

/* ============================================================================
 * PrimitiveMesh3D Creation/Destruction
 * ============================================================================ */

/**
 * @brief Create a PrimitiveMesh3D component
 * @param name Optional name for the component (can be NULL)
 * @param out_component Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_primitive_mesh3d_create(const char* name, LCComponentHandle* out_component);

/* ============================================================================
 * Shape Configuration
 * ============================================================================ */

/**
 * @brief Get the primitive shape type
 * @param component Component handle
 * @param out_shape Output parameter for shape type
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_get_shape(LCComponentHandle component, LCPrimitiveShape* out_shape);

/**
 * @brief Set the primitive shape type
 * @param component Component handle
 * @param shape Shape type (cube, sphere, cylinder, etc.)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_set_shape(LCComponentHandle component, LCPrimitiveShape shape);

/**
 * @brief Get the vertex color (tint)
 * @param component Component handle
 * @param out_color Output parameter for color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_get_color(LCComponentHandle component, LCColor* out_color);

/**
 * @brief Set the vertex color (tint)
 * @param component Component handle
 * @param color Vertex color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_set_color(LCComponentHandle component, LCColor color);

/**
 * @brief Get the shape size parameter
 * @param component Component handle
 * @param out_size Output parameter for size
 * @return LC_SUCCESS on success, error code otherwise
 * @note For cube/pyramid: base size. For sphere/cone/cylinder/capsule: radius. For torus: major radius.
 */
LC_API LCResult lc_primitive_mesh3d_get_size(LCComponentHandle component, float* out_size);

/**
 * @brief Set the shape size parameter
 * @param component Component handle
 * @param size Size value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_set_size(LCComponentHandle component, float size);

/**
 * @brief Get the height (for cylinder, cone, pyramid, capsule)
 * @param component Component handle
 * @param out_height Output parameter for height
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_get_height(LCComponentHandle component, float* out_height);

/**
 * @brief Set the height (for cylinder, cone, pyramid, capsule)
 * @param component Component handle
 * @param height Height value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_set_height(LCComponentHandle component, float height);

/**
 * @brief Get the detail level (segments/rings for curved shapes)
 * @param component Component handle
 * @param out_detail Output parameter for detail level
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_get_detail(LCComponentHandle component, int* out_detail);

/**
 * @brief Set the detail level (segments/rings for curved shapes)
 * @param component Component handle
 * @param detail Detail level (higher = smoother curves)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_set_detail(LCComponentHandle component, int detail);

/**
 * @brief Get the minor radius (for torus)
 * @param component Component handle
 * @param out_radius Output parameter for minor radius
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_get_minor_radius(LCComponentHandle component, float* out_radius);

/**
 * @brief Set the minor radius (for torus)
 * @param component Component handle
 * @param radius Minor radius value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_set_minor_radius(LCComponentHandle component, float radius);

/* ============================================================================
 * Shadow Properties
 * ============================================================================ */

/**
 * @brief Check if mesh casts shadows
 * @param component Component handle
 * @param out_cast_shadow Output parameter for cast shadow state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_get_cast_shadow(LCComponentHandle component, bool* out_cast_shadow);

/**
 * @brief Set whether mesh casts shadows
 * @param component Component handle
 * @param cast_shadow Cast shadow state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_set_cast_shadow(LCComponentHandle component, bool cast_shadow);

/**
 * @brief Check if mesh receives shadows
 * @param component Component handle
 * @param out_receive_shadow Output parameter for receive shadow state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_get_receive_shadow(LCComponentHandle component, bool* out_receive_shadow);

/**
 * @brief Set whether mesh receives shadows
 * @param component Component handle
 * @param receive_shadow Receive shadow state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_set_receive_shadow(LCComponentHandle component, bool receive_shadow);

/**
 * @brief Check if mesh is double-sided
 * @param component Component handle
 * @param out_double_sided Output parameter for double-sided state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_get_double_sided(LCComponentHandle component, bool* out_double_sided);

/**
 * @brief Set whether mesh is double-sided (disables backface culling)
 * @param component Component handle
 * @param double_sided Double-sided state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_set_double_sided(LCComponentHandle component, bool double_sided);

/* ============================================================================
 * Material Override
 * ============================================================================ */

/**
 * @brief Check if material override is enabled
 * @param component Component handle
 * @param out_has_override Output parameter for override state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_has_material_override(LCComponentHandle component, bool* out_has_override);

/**
 * @brief Enable or disable material override
 * @param component Component handle
 * @param enabled Enable material override
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_set_material_override_enabled(LCComponentHandle component, bool enabled);

/**
 * @brief Get the albedo color
 * @param component Component handle
 * @param out_color Output parameter for albedo color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_get_albedo_color(LCComponentHandle component, LCColor* out_color);

/**
 * @brief Set the albedo color
 * @param component Component handle
 * @param color Albedo color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_set_albedo_color(LCComponentHandle component, LCColor color);

/**
 * @brief Get the metallic value
 * @param component Component handle
 * @param out_metallic Output parameter for metallic value (0.0 = dielectric, 1.0 = metal)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_get_metallic(LCComponentHandle component, float* out_metallic);

/**
 * @brief Set the metallic value
 * @param component Component handle
 * @param metallic Metallic value (0.0 = dielectric, 1.0 = metal)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_set_metallic(LCComponentHandle component, float metallic);

/**
 * @brief Get the roughness value
 * @param component Component handle
 * @param out_roughness Output parameter for roughness value (0.0 = smooth, 1.0 = rough)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_get_roughness(LCComponentHandle component, float* out_roughness);

/**
 * @brief Set the roughness value
 * @param component Component handle
 * @param roughness Roughness value (0.0 = smooth, 1.0 = rough)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_set_roughness(LCComponentHandle component, float roughness);

/**
 * @brief Get the normal map scale
 * @param component Component handle
 * @param out_scale Output parameter for normal scale
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_get_normal_scale(LCComponentHandle component, float* out_scale);

/**
 * @brief Set the normal map scale
 * @param component Component handle
 * @param scale Normal map scale
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_set_normal_scale(LCComponentHandle component, float scale);

/**
 * @brief Get the emissive color
 * @param component Component handle
 * @param out_color Output parameter for emissive color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_get_emissive_color(LCComponentHandle component, LCColor* out_color);

/**
 * @brief Set the emissive color
 * @param component Component handle
 * @param color Emissive color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_set_emissive_color(LCComponentHandle component, LCColor color);

/**
 * @brief Get the emissive strength
 * @param component Component handle
 * @param out_strength Output parameter for emissive strength
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_get_emissive_strength(LCComponentHandle component, float* out_strength);

/**
 * @brief Set the emissive strength
 * @param component Component handle
 * @param strength Emissive strength multiplier
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_set_emissive_strength(LCComponentHandle component, float strength);

/* ============================================================================
 * Shader Selection
 * ============================================================================ */

/**
 * @brief Get the shader type
 * @param component Component handle
 * @param out_type Output parameter for shader type
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_get_shader_type(LCComponentHandle component, LCShaderType* out_type);

/**
 * @brief Set the shader type
 * @param component Component handle
 * @param type Shader type
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_set_shader_type(LCComponentHandle component, LCShaderType type);

/* ============================================================================
 * Toon Shader Parameters
 * ============================================================================ */

/**
 * @brief Get toon shadow bands
 * @param component Component handle
 * @param out_bands Output parameter for shadow bands
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_get_shadow_bands(LCComponentHandle component, float* out_bands);

/**
 * @brief Set toon shadow bands
 * @param component Component handle
 * @param bands Number of shadow bands
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_set_shadow_bands(LCComponentHandle component, float bands);

/**
 * @brief Get toon shadow threshold
 * @param component Component handle
 * @param out_threshold Output parameter for shadow threshold
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_get_shadow_threshold(LCComponentHandle component, float* out_threshold);

/**
 * @brief Set toon shadow threshold
 * @param component Component handle
 * @param threshold Shadow threshold
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_set_shadow_threshold(LCComponentHandle component, float threshold);

/**
 * @brief Get toon shadow softness
 * @param component Component handle
 * @param out_softness Output parameter for shadow softness
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_get_shadow_softness(LCComponentHandle component, float* out_softness);

/**
 * @brief Set toon shadow softness
 * @param component Component handle
 * @param softness Shadow softness
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_set_shadow_softness(LCComponentHandle component, float softness);

/**
 * @brief Get toon specular bands
 * @param component Component handle
 * @param out_bands Output parameter for specular bands
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_get_specular_bands(LCComponentHandle component, float* out_bands);

/**
 * @brief Set toon specular bands
 * @param component Component handle
 * @param bands Number of specular bands
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_set_specular_bands(LCComponentHandle component, float bands);

/**
 * @brief Get toon specular power
 * @param component Component handle
 * @param out_power Output parameter for specular power
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_get_specular_power(LCComponentHandle component, float* out_power);

/**
 * @brief Set toon specular power
 * @param component Component handle
 * @param power Specular power
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_set_specular_power(LCComponentHandle component, float power);

/**
 * @brief Get toon rim intensity
 * @param component Component handle
 * @param out_intensity Output parameter for rim intensity
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_get_rim_intensity(LCComponentHandle component, float* out_intensity);

/**
 * @brief Set toon rim intensity
 * @param component Component handle
 * @param intensity Rim light intensity
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_set_rim_intensity(LCComponentHandle component, float intensity);

/**
 * @brief Get toon rim power
 * @param component Component handle
 * @param out_power Output parameter for rim power
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_get_rim_power(LCComponentHandle component, float* out_power);

/**
 * @brief Set toon rim power
 * @param component Component handle
 * @param power Rim light power/falloff
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_primitive_mesh3d_set_rim_power(LCComponentHandle component, float power);


#endif /* LUPINE_CAPI_PRIMITIVE_MESH3D_H */
