/**
 * @file lc_material.h
 * @brief Lupine Engine C API - Material System
 *
 * Provides access to the material system for 3D rendering including:
 * - PBR (Physically Based Rendering) materials
 * - Multiple shader types (PBR, Toon, Stylized, Transparent, Glow, etc.)
 * - Material property blocks for per-instance overrides
 * - Default material library
 * - Custom shader support
 */

#ifndef LUPINE_CAPI_MATERIAL_H
#define LUPINE_CAPI_MATERIAL_H

#include "core/lc_core.h"
#include "math/lc_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Handle Types
 * ============================================================================ */

/** @brief Opaque handle to a material */
typedef struct LCMaterial_T* LCMaterialHandle;

/** @brief Opaque handle to a material property block */
typedef struct LCMaterialPropertyBlock_T* LCMaterialPropertyBlockHandle;

/** @brief Opaque handle to a texture (for material textures) */
typedef struct LCTexture_T* LCTextureHandle;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Shader types for materials
 * Note: This enum is also defined in lc_primitive_mesh3d.h for convenience.
 * The definitions are kept compatible.
 */
#ifndef LC_SHADER_TYPE_DEFINED
#define LC_SHADER_TYPE_DEFINED
typedef enum LCShaderType {
    LC_SHADER_PBR = 0,          /**< Physically-based rendering (default) */
    LC_SHADER_TOON = 1,         /**< Cel/toon shading */
    LC_SHADER_STYLIZED = 2,     /**< Fantasy-style soft shading */
    LC_SHADER_UNLIT = 3,        /**< No lighting calculations */
    LC_SHADER_STANDARD3D = 4,   /**< Basic lit shader */
    LC_SHADER_TRANSPARENT = 5,  /**< Transparent/Glass with refraction */
    LC_SHADER_GLOW = 6,         /**< Emissive glow effects */
    LC_SHADER_CUSTOM = 7        /**< User-provided custom shader */
} LCShaderType;
#endif

/**
 * @brief Render layer for sorting and filtering
 */
typedef enum LCRenderLayer {
    LC_RENDER_LAYER_DEFAULT = 0,
    LC_RENDER_LAYER_BACKGROUND = 100,
    LC_RENDER_LAYER_OPAQUE = 1000,
    LC_RENDER_LAYER_TRANSPARENT = 2000,
    LC_RENDER_LAYER_UI = 3000,
    LC_RENDER_LAYER_OVERLAY = 4000,
    LC_RENDER_LAYER_DEBUG = 5000
} LCRenderLayer;

/* ============================================================================
 * PBR Material Properties Structure
 * ============================================================================ */

/**
 * @brief PBR material properties for creating materials
 *
 * This structure contains all properties for a PBR material.
 * Use lc_pbr_material_properties_default() to get default values.
 */
typedef struct LCPBRMaterialProperties {
    /* Albedo/Base Color */
    LCColor albedoColor;
    bool useAlbedoTexture;

    /* Metallic-Roughness */
    float metallic;
    float roughness;
    bool useMetallicRoughnessTexture;

    /* Normal Mapping */
    bool useNormalTexture;
    float normalScale;

    /* Emissive */
    LCColor emissiveColor;
    bool useEmissiveTexture;
    float emissiveStrength;

    /* Alpha/Transparency */
    float alphaCutoff;
    bool alphaBlend;

    /* Ambient Occlusion */
    bool useAOTexture;
    float aoStrength;

    /* Height/Displacement */
    bool useHeightTexture;
    float heightScale;

    /* Shader Selection */
    LCShaderType shaderType;

    /* Toon Shader Parameters */
    float toonShadowBands;
    float toonShadowThreshold;
    float toonShadowSoftness;
    float toonSpecularBands;
    float toonSpecularPower;
    float toonRimIntensity;
    float toonRimPower;

    /* Stylized Shader Parameters */
    float stylizedShadowSoftness;
    float stylizedSpecularSoftness;
    float stylizedShadowBrightness;
    float stylizedShadowWarmth;
    float stylizedSpecularIntensity;
    float stylizedHalfLambertPower;

    /* Transparent Shader Parameters */
    float transparentOpacity;
    float transparentRefractiveIndex;
    float transparentChromaticAberration;
    float transparentFresnelPower;
    float transparentReflectivity;
    float transparentRoughness;
    float transparentThickness;

    /* Glow Shader Parameters */
    float glowIntensity;
    float glowFalloff;
    float glowPulseSpeed;
    float glowPulseAmount;
    float glowCoreSize;
    float glowCoreBrightness;
    float glowOuterGlow;
    float glowFresnelPower;
    float glowFresnelIntensity;
    float glowColorShift;

    /* Rendering Options */
    bool doubleSided;
    bool castShadows;
    bool receiveShadows;

} LCPBRMaterialProperties;

/* ============================================================================
 * PBR Material Properties Functions
 * ============================================================================ */

/**
 * @brief Get default PBR material properties
 * @return Default properties struct with sensible defaults
 */
LC_API LCPBRMaterialProperties lc_pbr_material_properties_default(void);

/* ============================================================================
 * Material Property Block Functions
 * ============================================================================ */

/**
 * @brief Create a new material property block for per-instance overrides
 * @param outHandle Output parameter for the property block handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_property_block_create(LCMaterialPropertyBlockHandle* outHandle);

/**
 * @brief Destroy a material property block
 * @param handle The property block to destroy
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_property_block_destroy(LCMaterialPropertyBlockHandle handle);

/**
 * @brief Clear all properties from a property block
 * @param handle The property block
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_property_block_clear(LCMaterialPropertyBlockHandle handle);

/**
 * @brief Check if property block is empty
 * @param handle The property block
 * @param outEmpty Output parameter for empty state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_property_block_is_empty(LCMaterialPropertyBlockHandle handle, bool* outEmpty);

/**
 * @brief Check if property block has a specific property
 * @param handle The property block
 * @param name Property name
 * @param outHas Output parameter for has property state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_property_block_has_property(LCMaterialPropertyBlockHandle handle, const char* name, bool* outHas);

/* Property Setters */

/**
 * @brief Set a float property
 * @param handle The property block
 * @param name Property name
 * @param value Float value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_property_block_set_float(LCMaterialPropertyBlockHandle handle, const char* name, float value);

/**
 * @brief Set an int property
 * @param handle The property block
 * @param name Property name
 * @param value Int value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_property_block_set_int(LCMaterialPropertyBlockHandle handle, const char* name, int value);

/**
 * @brief Set a bool property
 * @param handle The property block
 * @param name Property name
 * @param value Bool value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_property_block_set_bool(LCMaterialPropertyBlockHandle handle, const char* name, bool value);

/**
 * @brief Set a Vec2 property
 * @param handle The property block
 * @param name Property name
 * @param value Vec2 value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_property_block_set_vec2(LCMaterialPropertyBlockHandle handle, const char* name, LCVec2 value);

/**
 * @brief Set a Vec3 property
 * @param handle The property block
 * @param name Property name
 * @param value Vec3 value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_property_block_set_vec3(LCMaterialPropertyBlockHandle handle, const char* name, LCVec3 value);

/**
 * @brief Set a Vec4 property
 * @param handle The property block
 * @param name Property name
 * @param value Vec4 value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_property_block_set_vec4(LCMaterialPropertyBlockHandle handle, const char* name, LCVec4 value);

/**
 * @brief Set a Color property
 * @param handle The property block
 * @param name Property name
 * @param value Color value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_property_block_set_color(LCMaterialPropertyBlockHandle handle, const char* name, LCColor value);

/* Property Getters */

/**
 * @brief Get a float property
 * @param handle The property block
 * @param name Property name
 * @param outValue Output parameter for the value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_property_block_get_float(LCMaterialPropertyBlockHandle handle, const char* name, float* outValue);

/**
 * @brief Get an int property
 * @param handle The property block
 * @param name Property name
 * @param outValue Output parameter for the value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_property_block_get_int(LCMaterialPropertyBlockHandle handle, const char* name, int* outValue);

/**
 * @brief Get a bool property
 * @param handle The property block
 * @param name Property name
 * @param outValue Output parameter for the value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_property_block_get_bool(LCMaterialPropertyBlockHandle handle, const char* name, bool* outValue);

/**
 * @brief Get a Vec2 property
 * @param handle The property block
 * @param name Property name
 * @param outValue Output parameter for the value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_property_block_get_vec2(LCMaterialPropertyBlockHandle handle, const char* name, LCVec2* outValue);

/**
 * @brief Get a Vec3 property
 * @param handle The property block
 * @param name Property name
 * @param outValue Output parameter for the value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_property_block_get_vec3(LCMaterialPropertyBlockHandle handle, const char* name, LCVec3* outValue);

/**
 * @brief Get a Vec4 property
 * @param handle The property block
 * @param name Property name
 * @param outValue Output parameter for the value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_property_block_get_vec4(LCMaterialPropertyBlockHandle handle, const char* name, LCVec4* outValue);

/**
 * @brief Get a Color property
 * @param handle The property block
 * @param name Property name
 * @param outValue Output parameter for the value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_property_block_get_color(LCMaterialPropertyBlockHandle handle, const char* name, LCColor* outValue);

/* ============================================================================
 * Material Management Functions
 * ============================================================================ */

/**
 * @brief Create a PBR material with specified properties
 * @param name Material name
 * @param properties PBR material properties
 * @param outHandle Output parameter for the material handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_create_pbr(const char* name, const LCPBRMaterialProperties* properties, LCMaterialHandle* outHandle);

/**
 * @brief Destroy a material
 * @param handle The material to destroy
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_destroy(LCMaterialHandle handle);

/**
 * @brief Check if a material handle is valid
 * @param handle The material handle to check
 * @param outValid Output parameter for validity
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_is_valid(LCMaterialHandle handle, bool* outValid);

/**
 * @brief Get the name of a material
 * @param handle The material
 * @param outName Output buffer for the name
 * @param maxLen Maximum length of the buffer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_get_name(LCMaterialHandle handle, char* outName, uint32_t maxLen);

/* ============================================================================
 * Material Property Functions
 * ============================================================================ */

/**
 * @brief Set the render layer for a material
 * @param handle The material
 * @param layer The render layer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_set_render_layer(LCMaterialHandle handle, LCRenderLayer layer);

/**
 * @brief Get the render layer of a material
 * @param handle The material
 * @param outLayer Output parameter for the render layer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_get_render_layer(LCMaterialHandle handle, LCRenderLayer* outLayer);

/**
 * @brief Set whether material casts shadows
 * @param handle The material
 * @param castShadows Whether to cast shadows
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_set_cast_shadows(LCMaterialHandle handle, bool castShadows);

/**
 * @brief Get whether material casts shadows
 * @param handle The material
 * @param outCastShadows Output parameter
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_get_cast_shadows(LCMaterialHandle handle, bool* outCastShadows);

/**
 * @brief Set whether material receives shadows
 * @param handle The material
 * @param receiveShadows Whether to receive shadows
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_set_receive_shadows(LCMaterialHandle handle, bool receiveShadows);

/**
 * @brief Get whether material receives shadows
 * @param handle The material
 * @param outReceiveShadows Output parameter
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_get_receive_shadows(LCMaterialHandle handle, bool* outReceiveShadows);

/**
 * @brief Set whether material is transparent
 * @param handle The material
 * @param transparent Whether material is transparent
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_set_transparent(LCMaterialHandle handle, bool transparent);

/**
 * @brief Get whether material is transparent
 * @param handle The material
 * @param outTransparent Output parameter
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_get_transparent(LCMaterialHandle handle, bool* outTransparent);

/**
 * @brief Set alpha clip threshold for masked transparency
 * @param handle The material
 * @param threshold Alpha cutoff threshold (0.0-1.0)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_set_alpha_clip_threshold(LCMaterialHandle handle, float threshold);

/**
 * @brief Get alpha clip threshold
 * @param handle The material
 * @param outThreshold Output parameter
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_get_alpha_clip_threshold(LCMaterialHandle handle, float* outThreshold);

/* ============================================================================
 * Default Material Library
 * ============================================================================ */

/**
 * @brief Get the default PBR material
 * @param outHandle Output parameter for the material handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_get_default_pbr(LCMaterialHandle* outHandle);

/**
 * @brief Get the default toon/cel-shaded material
 * @param outHandle Output parameter for the material handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_get_default_toon(LCMaterialHandle* outHandle);

/**
 * @brief Get the default stylized material
 * @param outHandle Output parameter for the material handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_get_default_stylized(LCMaterialHandle* outHandle);

/**
 * @brief Get the default transparent/glass material
 * @param outHandle Output parameter for the material handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_get_default_transparent(LCMaterialHandle* outHandle);

/**
 * @brief Get the default glow/emissive material
 * @param outHandle Output parameter for the material handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_get_default_glow(LCMaterialHandle* outHandle);

/**
 * @brief Get the default skeletal PBR material (for animated meshes)
 * @param outHandle Output parameter for the material handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_get_default_skeletal(LCMaterialHandle* outHandle);

/**
 * @brief Get the default skeletal toon material
 * @param outHandle Output parameter for the material handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_get_default_skeletal_toon(LCMaterialHandle* outHandle);

/**
 * @brief Get the default skeletal stylized material
 * @param outHandle Output parameter for the material handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_get_default_skeletal_stylized(LCMaterialHandle* outHandle);

/**
 * @brief Get the default 3D text material
 * @param outHandle Output parameter for the material handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_get_default_text3d(LCMaterialHandle* outHandle);

/**
 * @brief Get the default colored double-sided material
 * @param outHandle Output parameter for the material handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_get_default_colored_double_sided(LCMaterialHandle* outHandle);

/* ============================================================================
 * Custom Shader Support
 * ============================================================================ */

/**
 * @brief Create or get a material with custom shaders
 * @param vertexShaderPath Path to vertex shader
 * @param fragmentShaderPath Path to fragment shader
 * @param isSkeletal Whether to use skeletal animation vertex attributes
 * @param outHandle Output parameter for the material handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_get_or_create_custom(
    const char* vertexShaderPath,
    const char* fragmentShaderPath,
    bool isSkeletal,
    LCMaterialHandle* outHandle
);

/**
 * @brief Clear the custom shader cache
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_material_clear_custom_shader_cache(void);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_MATERIAL_H */
