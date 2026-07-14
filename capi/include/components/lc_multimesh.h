/**
 * @file lc_multimesh.h
 * @brief Lupine Engine C API - MultiMesh Components
 *
 * This header provides GPU-instanced mesh rendering functionality including:
 * - MultiMeshGeneric: Core multi-mesh component for rendering many instances
 * - ScatterMultiMesh: Automatic scattering of mesh instances within a volume
 *
 * Features:
 * - GPU-instanced rendering for high performance
 * - Per-instance transforms, colors, and custom data
 * - Material and mesh loading support
 * - Shadow casting control
 * - Distance-based culling
 * - Automatic scatter generation with variation settings
 */

#ifndef LUPINE_CAPI_MULTIMESH_H
#define LUPINE_CAPI_MULTIMESH_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Shadow casting mode for MultiMesh components
 */
typedef enum LCShadowCastingMode {
    LC_SHADOW_OFF = 0,          /**< No shadow casting */
    LC_SHADOW_ON = 1,           /**< Cast shadows normally */
    LC_SHADOW_ONLY_SHADOWS = 2  /**< Cast shadows but don't render mesh */
} LCShadowCastingMode;

/**
 * @brief Scatter shape type for ScatterMultiMesh
 */
typedef enum LCScatterShapeType {
    LC_SCATTER_SHAPE_CUBE = 0,   /**< Scatter within a cube volume */
    LC_SCATTER_SHAPE_SPHERE = 1  /**< Scatter within a sphere volume */
} LCScatterShapeType;

/**
 * @brief Clumping mode for ScatterMultiMesh
 */
typedef enum LCClumpingMode {
    LC_CLUMPING_NONE = 0,       /**< Uniform distribution */
    LC_CLUMPING_CLUSTERED = 1,  /**< Instances cluster around random points */
    LC_CLUMPING_CENTERED = 2    /**< Instances cluster toward center */
} LCClumpingMode;

/* ============================================================================
 * MultiMeshGeneric Component
 * ============================================================================ */

/**
 * @brief Create a new MultiMeshGeneric component
 *
 * @param node Node to attach the component to
 * @param outHandle Output parameter for the component handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_multimesh_create(LCNodeHandle node, LCComponentHandle* outHandle);

/**
 * @brief Load a mesh from file
 *
 * Supports GLTF, GLB, FBX, and OBJ formats.
 *
 * @param handle Component handle
 * @param filepath Path to the mesh file
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_multimesh_load_mesh(LCComponentHandle handle, const char* filepath);

/**
 * @brief Get the current mesh path
 *
 * @param handle Component handle
 * @param outPath Output buffer for the path (must be pre-allocated)
 * @param maxLength Maximum length of output buffer
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_multimesh_get_mesh_path(LCComponentHandle handle, char* outPath, uint32_t maxLength);

/**
 * @brief Set the mesh path (will trigger reload)
 *
 * @param handle Component handle
 * @param path Path to the mesh file
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_multimesh_set_mesh_path(LCComponentHandle handle, const char* path);

/* ----------------------------------------------------------------------------
 * Material Override
 * ---------------------------------------------------------------------------- */

/**
 * @brief Get the material override path
 *
 * @param handle Component handle
 * @param outPath Output buffer for the path (must be pre-allocated)
 * @param maxLength Maximum length of output buffer
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_multimesh_get_material_override(LCComponentHandle handle, char* outPath, uint32_t maxLength);

/**
 * @brief Set the material override path
 *
 * @param handle Component handle
 * @param path Path to the material file (empty string to clear override)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_multimesh_set_material_override(LCComponentHandle handle, const char* path);

/* ----------------------------------------------------------------------------
 * Shadow Settings
 * ---------------------------------------------------------------------------- */

/**
 * @brief Get the shadow casting mode
 *
 * @param handle Component handle
 * @param outMode Output parameter for shadow mode
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_multimesh_get_cast_shadow(LCComponentHandle handle, LCShadowCastingMode* outMode);

/**
 * @brief Set the shadow casting mode
 *
 * @param handle Component handle
 * @param mode Shadow casting mode
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_multimesh_set_cast_shadow(LCComponentHandle handle, LCShadowCastingMode mode);

/**
 * @brief Get whether the mesh receives shadows
 *
 * @param handle Component handle
 * @param outReceive Output parameter for receive shadow flag
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_multimesh_get_receive_shadow(LCComponentHandle handle, bool* outReceive);

/**
 * @brief Set whether the mesh receives shadows
 *
 * @param handle Component handle
 * @param receive True to receive shadows
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_multimesh_set_receive_shadow(LCComponentHandle handle, bool receive);

/* ----------------------------------------------------------------------------
 * Instance Management
 * ---------------------------------------------------------------------------- */

/**
 * @brief Get the number of instances
 *
 * @param handle Component handle
 * @param outCount Output parameter for instance count
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_multimesh_get_instance_count(LCComponentHandle handle, int* outCount);

/**
 * @brief Set the number of instances
 *
 * Allocates or deallocates instance data as needed.
 *
 * @param handle Component handle
 * @param count Number of instances
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_multimesh_set_instance_count(LCComponentHandle handle, int count);

/**
 * @brief Get an instance's transform matrix
 *
 * @param handle Component handle
 * @param index Instance index
 * @param outTransform Output parameter for the transform matrix
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_multimesh_get_instance_transform(LCComponentHandle handle, int index, LCMat4* outTransform);

/**
 * @brief Set an instance's transform matrix
 *
 * @param handle Component handle
 * @param index Instance index
 * @param transform The transform matrix
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_multimesh_set_instance_transform(LCComponentHandle handle, int index, LCMat4 transform);

/**
 * @brief Get an instance's color tint
 *
 * @param handle Component handle
 * @param index Instance index
 * @param outColor Output parameter for the color
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_multimesh_get_instance_color(LCComponentHandle handle, int index, LCColor* outColor);

/**
 * @brief Set an instance's color tint
 *
 * @param handle Component handle
 * @param index Instance index
 * @param color The color tint
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_multimesh_set_instance_color(LCComponentHandle handle, int index, LCColor color);

/**
 * @brief Get an instance's custom data
 *
 * @param handle Component handle
 * @param index Instance index
 * @param outData Output parameter for the custom data (Vec4)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_multimesh_get_instance_custom_data(LCComponentHandle handle, int index, LCVec4* outData);

/**
 * @brief Set an instance's custom data
 *
 * @param handle Component handle
 * @param index Instance index
 * @param data The custom data (Vec4)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_multimesh_set_instance_custom_data(LCComponentHandle handle, int index, LCVec4 data);

/* ----------------------------------------------------------------------------
 * Culling Settings
 * ---------------------------------------------------------------------------- */

/**
 * @brief Get whether per-instance culling is enabled
 *
 * @param handle Component handle
 * @param outCull Output parameter for culling flag
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_multimesh_get_cull_per_instance(LCComponentHandle handle, bool* outCull);

/**
 * @brief Set whether per-instance culling is enabled
 *
 * @param handle Component handle
 * @param cull True to enable per-instance culling
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_multimesh_set_cull_per_instance(LCComponentHandle handle, bool cull);

/**
 * @brief Get the maximum render distance
 *
 * @param handle Component handle
 * @param outDistance Output parameter for max distance
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_multimesh_get_max_distance(LCComponentHandle handle, float* outDistance);

/**
 * @brief Set the maximum render distance
 *
 * Instances beyond this distance will be culled.
 *
 * @param handle Component handle
 * @param distance Maximum render distance
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_multimesh_set_max_distance(LCComponentHandle handle, float distance);

/* ----------------------------------------------------------------------------
 * Discrete LOD levels
 *
 * LOD 0 is the base mesh (the meshPath); levels 1..3 are optional lower-detail
 * meshes selected by camera distance. An instance closer than the level-1
 * distance draws LOD 0, closer than the level-2 distance draws LOD 1, and so on.
 * Empty LOD mesh paths fall back to the nearest lower (more detailed) level.
 * Valid level indices are 0..3 (LC_MULTIMESH_LOD_LEVEL_COUNT - 1).
 * ---------------------------------------------------------------------------- */

#define LC_MULTIMESH_LOD_LEVEL_COUNT 4

/**
 * @brief Get the mesh path for a LOD level (0 = base mesh).
 *
 * @param handle Component handle
 * @param level LOD level index (0..3)
 * @param outBuffer Buffer to receive the null-terminated path
 * @param bufferSize Size of outBuffer in bytes
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_multimesh_get_lod_mesh_path(LCComponentHandle handle, int level, char* outBuffer, size_t bufferSize);

/**
 * @brief Set the mesh path for a LOD level (0 = base mesh).
 *
 * @param handle Component handle
 * @param level LOD level index (0..3)
 * @param path Mesh file path (res:// or physical)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_multimesh_set_lod_mesh_path(LCComponentHandle handle, int level, const char* path);

/**
 * @brief Get the camera distance at which a LOD level takes over (levels 1..3).
 *
 * @param handle Component handle
 * @param level LOD level index (1..3); level 0 always returns 0
 * @param outDistance Output parameter for the level's distance threshold
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_multimesh_get_lod_distance(LCComponentHandle handle, int level, float* outDistance);

/**
 * @brief Set the camera distance at which a LOD level takes over (levels 1..3).
 *
 * @param handle Component handle
 * @param level LOD level index (1..3)
 * @param distance Distance threshold
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_multimesh_set_lod_distance(LCComponentHandle handle, int level, float distance);

/* ----------------------------------------------------------------------------
 * Automatic LOD generation
 *
 * When enabled, any LOD level (1..3) without an author-supplied mesh path is
 * filled by decimating the base mesh. Each successive level keeps
 * autoLodReduction^level of the base triangle count.
 * ---------------------------------------------------------------------------- */

/**
 * @brief Get whether automatic LOD generation is enabled.
 *
 * @param handle Component handle
 * @param outEnabled Output parameter for the enabled flag
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_multimesh_get_auto_generate_lods(LCComponentHandle handle, bool* outEnabled);

/**
 * @brief Set whether automatic LOD generation is enabled.
 *
 * @param handle Component handle
 * @param enable True to auto-generate missing LOD meshes from the base mesh
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_multimesh_set_auto_generate_lods(LCComponentHandle handle, bool enable);

/**
 * @brief Get the per-level triangle reduction ratio for auto LOD generation.
 *
 * @param handle Component handle
 * @param outReduction Output parameter for the reduction ratio (0,1)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_multimesh_get_auto_lod_reduction(LCComponentHandle handle, float* outReduction);

/**
 * @brief Set the per-level triangle reduction ratio for auto LOD generation.
 *
 * @param handle Component handle
 * @param reduction Reduction ratio in (0,1); e.g. 0.5 = 50%/25%/12.5%
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_multimesh_set_auto_lod_reduction(LCComponentHandle handle, float reduction);

/* ----------------------------------------------------------------------------
 * Base transform (applied to every instance before per-instance variation)
 * ---------------------------------------------------------------------------- */

/**
 * @brief Get the base rotation (Euler XYZ degrees) applied to all instances.
 *
 * @param handle Component handle
 * @param outRotation Output parameter for the rotation (degrees)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_multimesh_get_base_rotation(LCComponentHandle handle, LCVec3* outRotation);

/**
 * @brief Set the base rotation (Euler XYZ degrees) applied to all instances.
 *
 * @param handle Component handle
 * @param rotation Rotation in degrees
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_multimesh_set_base_rotation(LCComponentHandle handle, LCVec3 rotation);

/**
 * @brief Get the base scale applied to all instances.
 *
 * @param handle Component handle
 * @param outScale Output parameter for the scale
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_multimesh_get_base_scale(LCComponentHandle handle, LCVec3* outScale);

/**
 * @brief Set the base scale applied to all instances.
 *
 * @param handle Component handle
 * @param scale Per-axis scale multiplier
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_multimesh_set_base_scale(LCComponentHandle handle, LCVec3 scale);

/* ============================================================================
 * ScatterMultiMesh Component
 * ============================================================================ */

/**
 * @brief Create a new ScatterMultiMesh component
 *
 * ScatterMultiMesh automatically generates instance positions within a volume.
 *
 * @param node Node to attach the component to
 * @param outHandle Output parameter for the component handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_create(LCNodeHandle node, LCComponentHandle* outHandle);

/* ----------------------------------------------------------------------------
 * Debug Visualization
 * ---------------------------------------------------------------------------- */

/**
 * @brief Get whether debug shape visualization is enabled
 *
 * @param handle Component handle
 * @param outShow Output parameter for show flag
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_get_show_debug_shape(LCComponentHandle handle, bool* outShow);

/**
 * @brief Set whether debug shape visualization is enabled
 *
 * @param handle Component handle
 * @param show True to show debug shape
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_set_show_debug_shape(LCComponentHandle handle, bool show);

/**
 * @brief Get the debug visualization color
 *
 * @param handle Component handle
 * @param outColor Output parameter for debug color
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_get_debug_color(LCComponentHandle handle, LCColor* outColor);

/**
 * @brief Set the debug visualization color
 *
 * @param handle Component handle
 * @param color Debug color
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_set_debug_color(LCComponentHandle handle, LCColor color);

/* ----------------------------------------------------------------------------
 * Scatter Shape Settings
 * ---------------------------------------------------------------------------- */

/**
 * @brief Get the scatter shape type
 *
 * @param handle Component handle
 * @param outShape Output parameter for shape type
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_get_scatter_shape(LCComponentHandle handle, LCScatterShapeType* outShape);

/**
 * @brief Set the scatter shape type
 *
 * @param handle Component handle
 * @param shape Shape type (cube or sphere)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_set_scatter_shape(LCComponentHandle handle, LCScatterShapeType shape);

/**
 * @brief Get the cube size (used when shape is Cube)
 *
 * @param handle Component handle
 * @param outSize Output parameter for cube size (width, height, depth)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_get_cube_size(LCComponentHandle handle, LCVec3* outSize);

/**
 * @brief Set the cube size (used when shape is Cube)
 *
 * @param handle Component handle
 * @param size Cube size (width, height, depth)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_set_cube_size(LCComponentHandle handle, LCVec3 size);

/**
 * @brief Get the sphere radius (used when shape is Sphere)
 *
 * @param handle Component handle
 * @param outRadius Output parameter for radius
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_get_radius(LCComponentHandle handle, float* outRadius);

/**
 * @brief Set the sphere radius (used when shape is Sphere)
 *
 * @param handle Component handle
 * @param radius Sphere radius
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_set_radius(LCComponentHandle handle, float radius);

/* ----------------------------------------------------------------------------
 * Scatter Count
 * ---------------------------------------------------------------------------- */

/**
 * @brief Get the number of scattered instances
 *
 * @param handle Component handle
 * @param outCount Output parameter for scatter count
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_get_scatter_count(LCComponentHandle handle, int* outCount);

/**
 * @brief Set the number of scattered instances
 *
 * @param handle Component handle
 * @param count Number of instances to scatter
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_set_scatter_count(LCComponentHandle handle, int count);

/* ----------------------------------------------------------------------------
 * Clumping Settings
 * ---------------------------------------------------------------------------- */

/**
 * @brief Get the clumping mode
 *
 * @param handle Component handle
 * @param outMode Output parameter for clumping mode
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_get_clumping_mode(LCComponentHandle handle, LCClumpingMode* outMode);

/**
 * @brief Set the clumping mode
 *
 * @param handle Component handle
 * @param mode Clumping mode
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_set_clumping_mode(LCComponentHandle handle, LCClumpingMode mode);

/**
 * @brief Get the clump factor
 *
 * @param handle Component handle
 * @param outFactor Output parameter for clump factor [0-1]
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_get_clump_factor(LCComponentHandle handle, float* outFactor);

/**
 * @brief Set the clump factor
 *
 * @param handle Component handle
 * @param factor Clump factor [0-1], higher = tighter clusters
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_set_clump_factor(LCComponentHandle handle, float factor);

/**
 * @brief Get the number of clump centers
 *
 * @param handle Component handle
 * @param outCount Output parameter for clump count
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_get_clump_count(LCComponentHandle handle, int* outCount);

/**
 * @brief Set the number of clump centers
 *
 * @param handle Component handle
 * @param count Number of clump centers
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_set_clump_count(LCComponentHandle handle, int count);

/**
 * @brief Get the clump radius
 *
 * @param handle Component handle
 * @param outRadius Output parameter for clump radius
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_get_clump_radius(LCComponentHandle handle, float* outRadius);

/**
 * @brief Set the clump radius
 *
 * @param handle Component handle
 * @param radius Radius around clump centers
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_set_clump_radius(LCComponentHandle handle, float radius);

/* ----------------------------------------------------------------------------
 * Variation Settings
 * ---------------------------------------------------------------------------- */

/**
 * @brief Get the scale range for variation
 *
 * @param handle Component handle
 * @param outRange Output parameter for scale range (min, max)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_get_scale_range(LCComponentHandle handle, LCVec2* outRange);

/**
 * @brief Set the scale range for variation
 *
 * @param handle Component handle
 * @param range Scale range (min, max)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_set_scale_range(LCComponentHandle handle, LCVec2 range);

/**
 * @brief Get whether uniform scaling is used
 *
 * @param handle Component handle
 * @param outUniform Output parameter for uniform scale flag
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_get_uniform_scale(LCComponentHandle handle, bool* outUniform);

/**
 * @brief Set whether uniform scaling is used
 *
 * @param handle Component handle
 * @param uniform True for uniform scaling on all axes
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_set_uniform_scale(LCComponentHandle handle, bool uniform);

/**
 * @brief Get the rotation variation (degrees)
 *
 * @param handle Component handle
 * @param outVariation Output parameter for rotation variation (x, y, z degrees)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_get_rotation_variation(LCComponentHandle handle, LCVec3* outVariation);

/**
 * @brief Set the rotation variation (degrees)
 *
 * @param handle Component handle
 * @param variation Rotation variation (x, y, z degrees)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_set_rotation_variation(LCComponentHandle handle, LCVec3 variation);

/**
 * @brief Get whether instances align to surface normals
 *
 * @param handle Component handle
 * @param outAlign Output parameter for align to surface flag
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_get_align_to_surface(LCComponentHandle handle, bool* outAlign);

/**
 * @brief Set whether instances align to surface normals
 *
 * @param handle Component handle
 * @param align True to align instances to surface normals
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_set_align_to_surface(LCComponentHandle handle, bool align);

/* ----------------------------------------------------------------------------
 * Color Variation
 * ---------------------------------------------------------------------------- */

/**
 * @brief Get whether color variation is enabled
 *
 * @param handle Component handle
 * @param outEnabled Output parameter for enabled flag
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_get_color_variation_enabled(LCComponentHandle handle, bool* outEnabled);

/**
 * @brief Set whether color variation is enabled
 *
 * @param handle Component handle
 * @param enabled True to enable color variation
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_set_color_variation_enabled(LCComponentHandle handle, bool enabled);

/**
 * @brief Get the base color for color variation
 *
 * @param handle Component handle
 * @param outColor Output parameter for base color
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_get_base_color(LCComponentHandle handle, LCColor* outColor);

/**
 * @brief Set the base color for color variation
 *
 * @param handle Component handle
 * @param color Base color
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_set_base_color(LCComponentHandle handle, LCColor color);

/**
 * @brief Get the color variation amount
 *
 * @param handle Component handle
 * @param outVariation Output parameter for variation amount [0-1]
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_get_color_variation(LCComponentHandle handle, float* outVariation);

/**
 * @brief Set the color variation amount
 *
 * @param handle Component handle
 * @param variation Variation amount [0-1]
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_set_color_variation(LCComponentHandle handle, float variation);

/* ----------------------------------------------------------------------------
 * Randomization
 * ---------------------------------------------------------------------------- */

/**
 * @brief Get the random seed
 *
 * @param handle Component handle
 * @param outSeed Output parameter for random seed
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_get_random_seed(LCComponentHandle handle, int* outSeed);

/**
 * @brief Set the random seed
 *
 * @param handle Component handle
 * @param seed Random seed for reproducible results
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_set_random_seed(LCComponentHandle handle, int seed);

/**
 * @brief Get whether a custom random seed is used
 *
 * @param handle Component handle
 * @param outUse Output parameter for use random seed flag
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_get_use_random_seed(LCComponentHandle handle, bool* outUse);

/**
 * @brief Set whether a custom random seed is used
 *
 * @param handle Component handle
 * @param use True to use custom seed, false for random seed each time
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_set_use_random_seed(LCComponentHandle handle, bool use);

/* ----------------------------------------------------------------------------
 * Regeneration
 * ---------------------------------------------------------------------------- */

/**
 * @brief Regenerate all scattered instances
 *
 * Call this to regenerate the scatter distribution.
 *
 * @param handle Component handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_regenerate(LCComponentHandle handle);

/**
 * @brief Check if regeneration is needed
 *
 * @param handle Component handle
 * @param outNeeds Output parameter for needs regeneration flag
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scatter_multimesh_needs_regeneration(LCComponentHandle handle, bool* outNeeds);

#endif /* LUPINE_CAPI_MULTIMESH_H */
