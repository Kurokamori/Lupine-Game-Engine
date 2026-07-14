/**
 * @file lc_node_scatter.h
 * @brief Lupine Engine C API - NodeScatter component
 *
 * This header provides node scattering functionality for distributing
 * instances of nodes across an area or collision surfaces.
 */

#ifndef LUPINE_CAPI_NODE_SCATTER_H
#define LUPINE_CAPI_NODE_SCATTER_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Node scatter modes
 */
typedef enum LCNodeScatterMode {
    LC_NODE_SCATTER_SIMPLE = 0,    /**< Simple area-based scattering */
    LC_NODE_SCATTER_COLLISION = 1  /**< Scatter on collision surfaces */
} LCNodeScatterMode;

/**
 * @brief Node scatter distribution types
 */
typedef enum LCNodeScatterDistribution {
    LC_NODE_SCATTER_RANDOM = 0,  /**< Random distribution */
    LC_NODE_SCATTER_GRID = 1     /**< Grid-based distribution */
} LCNodeScatterDistribution;

/**
 * @brief Scatter collision optimization modes
 */
typedef enum LCScatterCollisionMode {
    LC_SCATTER_COLLISION_FULL = 0,            /**< Full collision detection */
    LC_SCATTER_COLLISION_SIMPLIFIED_BOX = 1,  /**< Simplified box collision */
    LC_SCATTER_COLLISION_SIMPLIFIED_SPHERE = 2, /**< Simplified sphere collision */
    LC_SCATTER_COLLISION_NONE = 3             /**< No collision optimization */
} LCScatterCollisionMode;

/* ============================================================================
 * NodeScatter Creation
 * ============================================================================ */

/**
 * @brief Create a NodeScatter component
 * @param name Optional name for the component (can be NULL)
 * @param out_component Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_create(const char* name, LCComponentHandle* out_component);

/* ============================================================================
 * Scatter Mode
 * ============================================================================ */

/**
 * @brief Get the scatter mode
 * @param component Component handle
 * @param out_mode Output parameter for scatter mode
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_get_mode(LCComponentHandle component, LCNodeScatterMode* out_mode);

/**
 * @brief Set the scatter mode
 * @param component Component handle
 * @param mode Scatter mode (Simple or Collision)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_set_mode(LCComponentHandle component, LCNodeScatterMode mode);

/* ============================================================================
 * Collision Layer Settings
 * ============================================================================ */

/**
 * @brief Get the scatter layers (bitmask for surfaces to scatter on)
 * @param component Component handle
 * @param out_layers Output parameter for layer mask
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_get_scatter_layers(LCComponentHandle component, uint32_t* out_layers);

/**
 * @brief Set the scatter layers
 * @param component Component handle
 * @param layers Layer bitmask
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_set_scatter_layers(LCComponentHandle component, uint32_t layers);

/**
 * @brief Get the cutout layers (surfaces that block scattering)
 * @param component Component handle
 * @param out_layers Output parameter for layer mask
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_get_cutout_layers(LCComponentHandle component, uint32_t* out_layers);

/**
 * @brief Set the cutout layers
 * @param component Component handle
 * @param layers Layer bitmask
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_set_cutout_layers(LCComponentHandle component, uint32_t layers);

/* ============================================================================
 * Distribution Settings
 * ============================================================================ */

/**
 * @brief Get the distribution mode
 * @param component Component handle
 * @param out_mode Output parameter for distribution mode
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_get_distribution_mode(LCComponentHandle component, LCNodeScatterDistribution* out_mode);

/**
 * @brief Set the distribution mode
 * @param component Component handle
 * @param mode Distribution mode (Random or Grid)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_set_distribution_mode(LCComponentHandle component, LCNodeScatterDistribution mode);

/**
 * @brief Get the instance count
 * @param component Component handle
 * @param out_count Output parameter for instance count
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_get_instance_count(LCComponentHandle component, int* out_count);

/**
 * @brief Set the instance count
 * @param component Component handle
 * @param count Target instance count
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_set_instance_count(LCComponentHandle component, int count);

/* ============================================================================
 * Grid Settings
 * ============================================================================ */

/**
 * @brief Get the grid spacing X
 * @param component Component handle
 * @param out_spacing Output parameter for X spacing
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_get_grid_spacing_x(LCComponentHandle component, float* out_spacing);

/**
 * @brief Set the grid spacing X
 * @param component Component handle
 * @param spacing X spacing value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_set_grid_spacing_x(LCComponentHandle component, float spacing);

/**
 * @brief Get the grid spacing Z
 * @param component Component handle
 * @param out_spacing Output parameter for Z spacing
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_get_grid_spacing_z(LCComponentHandle component, float* out_spacing);

/**
 * @brief Set the grid spacing Z
 * @param component Component handle
 * @param spacing Z spacing value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_set_grid_spacing_z(LCComponentHandle component, float spacing);

/**
 * @brief Get the grid jitter
 * @param component Component handle
 * @param out_jitter Output parameter for jitter
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_get_grid_jitter(LCComponentHandle component, float* out_jitter);

/**
 * @brief Set the grid jitter
 * @param component Component handle
 * @param jitter Jitter value (randomness applied to grid positions)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_set_grid_jitter(LCComponentHandle component, float jitter);

/* ============================================================================
 * Area Settings
 * ============================================================================ */

/**
 * @brief Get the scatter area size
 * @param component Component handle
 * @param out_size Output parameter for area size
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_get_area_size(LCComponentHandle component, LCVec3* out_size);

/**
 * @brief Set the scatter area size
 * @param component Component handle
 * @param size Area size (X, Y, Z dimensions)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_set_area_size(LCComponentHandle component, LCVec3 size);

/**
 * @brief Get the scatter area offset
 * @param component Component handle
 * @param out_offset Output parameter for area offset
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_get_area_offset(LCComponentHandle component, LCVec3* out_offset);

/**
 * @brief Set the scatter area offset
 * @param component Component handle
 * @param offset Area offset from node position
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_set_area_offset(LCComponentHandle component, LCVec3 offset);

/* ============================================================================
 * Surface Settings (Collision mode)
 * ============================================================================ */

/**
 * @brief Get whether instances align to surface normal
 * @param component Component handle
 * @param out_align Output parameter for align state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_get_align_to_normal(LCComponentHandle component, bool* out_align);

/**
 * @brief Set whether instances align to surface normal
 * @param component Component handle
 * @param align Align to normal state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_set_align_to_normal(LCComponentHandle component, bool align);

/**
 * @brief Get the normal alignment strength
 * @param component Component handle
 * @param out_strength Output parameter for strength
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_get_normal_alignment_strength(LCComponentHandle component, float* out_strength);

/**
 * @brief Set the normal alignment strength
 * @param component Component handle
 * @param strength Alignment strength (0.0 = upright, 1.0 = fully aligned)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_set_normal_alignment_strength(LCComponentHandle component, float strength);

/**
 * @brief Get the surface offset
 * @param component Component handle
 * @param out_offset Output parameter for offset
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_get_surface_offset(LCComponentHandle component, float* out_offset);

/**
 * @brief Set the surface offset
 * @param component Component handle
 * @param offset Offset from surface
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_set_surface_offset(LCComponentHandle component, float offset);

/* ============================================================================
 * Slope Filtering
 * ============================================================================ */

/**
 * @brief Get the minimum slope angle
 * @param component Component handle
 * @param out_slope Output parameter for min slope (degrees)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_get_min_slope(LCComponentHandle component, float* out_slope);

/**
 * @brief Set the minimum slope angle
 * @param component Component handle
 * @param slope Min slope in degrees
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_set_min_slope(LCComponentHandle component, float slope);

/**
 * @brief Get the maximum slope angle
 * @param component Component handle
 * @param out_slope Output parameter for max slope (degrees)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_get_max_slope(LCComponentHandle component, float* out_slope);

/**
 * @brief Set the maximum slope angle
 * @param component Component handle
 * @param slope Max slope in degrees
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_set_max_slope(LCComponentHandle component, float slope);

/* ============================================================================
 * Scale Variation
 * ============================================================================ */

/**
 * @brief Get the scale range
 * @param component Component handle
 * @param out_range Output parameter for scale range (min, max)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_get_scale_range(LCComponentHandle component, LCVec2* out_range);

/**
 * @brief Set the scale range
 * @param component Component handle
 * @param range Scale range (x = min, y = max)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_set_scale_range(LCComponentHandle component, LCVec2 range);

/**
 * @brief Get whether scale is uniform across axes
 * @param component Component handle
 * @param out_uniform Output parameter for uniform state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_get_uniform_scale(LCComponentHandle component, bool* out_uniform);

/**
 * @brief Set whether scale is uniform across axes
 * @param component Component handle
 * @param uniform Uniform scale state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_set_uniform_scale(LCComponentHandle component, bool uniform);

/* ============================================================================
 * Rotation Variation
 * ============================================================================ */

/**
 * @brief Get the rotation variation
 * @param component Component handle
 * @param out_variation Output parameter for rotation variation (degrees per axis)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_get_rotation_variation(LCComponentHandle component, LCVec3* out_variation);

/**
 * @brief Set the rotation variation
 * @param component Component handle
 * @param variation Rotation variation in degrees per axis
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_set_rotation_variation(LCComponentHandle component, LCVec3 variation);

/**
 * @brief Get whether only Y rotation is randomized
 * @param component Component handle
 * @param out_random Output parameter for random Y state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_get_random_y_rotation(LCComponentHandle component, bool* out_random);

/**
 * @brief Set whether only Y rotation is randomized
 * @param component Component handle
 * @param random Random Y rotation state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_set_random_y_rotation(LCComponentHandle component, bool random);

/* ============================================================================
 * Randomization
 * ============================================================================ */

/**
 * @brief Get the random seed
 * @param component Component handle
 * @param out_seed Output parameter for seed
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_get_random_seed(LCComponentHandle component, int* out_seed);

/**
 * @brief Set the random seed
 * @param component Component handle
 * @param seed Random seed value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_set_random_seed(LCComponentHandle component, int seed);

/**
 * @brief Get whether to use the random seed
 * @param component Component handle
 * @param out_use Output parameter for use seed state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_get_use_random_seed(LCComponentHandle component, bool* out_use);

/**
 * @brief Set whether to use the random seed
 * @param component Component handle
 * @param use Use random seed state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_set_use_random_seed(LCComponentHandle component, bool use);

/* ============================================================================
 * Debug Visualization
 * ============================================================================ */

/**
 * @brief Get whether debug shape is shown
 * @param component Component handle
 * @param out_show Output parameter for show state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_get_show_debug_shape(LCComponentHandle component, bool* out_show);

/**
 * @brief Set whether to show debug shape
 * @param component Component handle
 * @param show Show debug shape state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_set_show_debug_shape(LCComponentHandle component, bool show);

/**
 * @brief Get the debug color
 * @param component Component handle
 * @param out_color Output parameter for debug color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_get_debug_color(LCComponentHandle component, LCColor* out_color);

/**
 * @brief Set the debug color
 * @param component Component handle
 * @param color Debug visualization color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_set_debug_color(LCComponentHandle component, LCColor color);

/* ============================================================================
 * Performance / Optimization
 * ============================================================================ */

/**
 * @brief Get the instances per frame for progressive loading
 * @param component Component handle
 * @param out_count Output parameter for instances per frame
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_get_instances_per_frame(LCComponentHandle component, int* out_count);

/**
 * @brief Set the instances per frame
 * @param component Component handle
 * @param count Instances to create per frame
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_set_instances_per_frame(LCComponentHandle component, int count);

/**
 * @brief Get the collision optimization mode
 * @param component Component handle
 * @param out_mode Output parameter for collision mode
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_get_collision_optimization(LCComponentHandle component, LCScatterCollisionMode* out_mode);

/**
 * @brief Set the collision optimization mode
 * @param component Component handle
 * @param mode Collision optimization mode
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_set_collision_optimization(LCComponentHandle component, LCScatterCollisionMode mode);

/**
 * @brief Get the collision distance
 * @param component Component handle
 * @param out_distance Output parameter for distance
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_get_collision_distance(LCComponentHandle component, float* out_distance);

/**
 * @brief Set the collision distance
 * @param component Component handle
 * @param distance Collision detection distance
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_set_collision_distance(LCComponentHandle component, float distance);

/**
 * @brief Get whether progressive loading is enabled
 * @param component Component handle
 * @param out_enabled Output parameter for enabled state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_get_progressive_loading(LCComponentHandle component, bool* out_enabled);

/**
 * @brief Set whether progressive loading is enabled
 * @param component Component handle
 * @param enabled Progressive loading state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_set_progressive_loading(LCComponentHandle component, bool enabled);

/* ============================================================================
 * Regeneration
 * ============================================================================ */

/**
 * @brief Force regenerate the scatter
 * @param component Component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_regenerate(LCComponentHandle component);

/**
 * @brief Check if scatter needs regeneration
 * @param component Component handle
 * @param out_needs Output parameter for needs regeneration state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_needs_regeneration(LCComponentHandle component, bool* out_needs);

/**
 * @brief Get the number of scattered instances
 * @param component Component handle
 * @param out_count Output parameter for instance count
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_node_scatter_get_scattered_instance_count(LCComponentHandle component, int* out_count);

#endif /* LUPINE_CAPI_NODE_SCATTER_H */
