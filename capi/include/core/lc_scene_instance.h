/**
 * @file lc_scene_instance.h
 * @brief Lupine Engine C API - SceneInstance node
 *
 * This header provides scene instantiation functionality.
 * Features:
 * - Load and instantiate external scene files
 * - Nested scene support
 * - Scene reference management
 * - Automatic UUID regeneration for instances
 */

#ifndef LUPINE_CAPI_SCENE_INSTANCE_H
#define LUPINE_CAPI_SCENE_INSTANCE_H

#include "core/lc_core.h"
#include "core/lc_node.h"

/* ============================================================================
 * SceneInstance Creation
 * ============================================================================ */

/**
 * @brief Create a SceneInstance node
 * @param name Optional name for the node (can be NULL)
 * @param out_node Output parameter for the created node handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_scene_instance_create(const char* name, LCNodeHandle* out_node);

/* ============================================================================
 * Scene Reference
 * ============================================================================ */

/**
 * @brief Set the scene file to instantiate
 * @param node Node handle
 * @param filepath Path to the scene file
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_scene_instance_set_scene_reference(LCNodeHandle node, const char* filepath);

/**
 * @brief Get the scene reference path
 * @param node Node handle
 * @param out_buffer Output buffer for path
 * @param buffer_size Size of output buffer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_scene_instance_get_scene_reference(LCNodeHandle node, char* out_buffer, size_t buffer_size);

/**
 * @brief Reload the referenced scene
 * @param node Node handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_scene_instance_reload_scene(LCNodeHandle node);

/**
 * @brief Clear the instance (remove instantiated nodes and clear reference)
 * @param node Node handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_scene_instance_clear(LCNodeHandle node);

/* ============================================================================
 * State Queries
 * ============================================================================ */

/**
 * @brief Check if scene instance has a valid reference
 * @param node Node handle
 * @param out_valid Output parameter for validity
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_scene_instance_has_valid_reference(LCNodeHandle node, bool* out_valid);

/**
 * @brief Get the instanced root node
 * @param node Node handle
 * @param out_root Output parameter for root node handle (may be NULL if not instantiated)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_scene_instance_get_instanced_root(LCNodeHandle node, LCNodeHandle* out_root);

/* ============================================================================
 * One-call Instantiation
 *
 * These mirror the scripting `instantiate_scene()` / `instantiate_prefab()`
 * helpers: load and attach an external scene or prefab to a parent in a single
 * call. When @p parent_or_null is NULL the new node is attached to the runtime
 * SceneManager's current scene root.
 * ============================================================================ */

/**
 * @brief Instantiate an external scene file under a parent in one call.
 *
 * Creates a SceneInstance node, attaches it to @p parent_or_null (or the current
 * scene root if NULL), and sets its scene reference to @p scene_path so the
 * referenced scene is loaded and instantiated as children. C-API equivalent of
 * the scripting `instantiate_scene()`.
 *
 * @param scene_path Virtual path to the scene file to instantiate.
 * @param parent_or_null Parent node handle, or NULL to attach to the current scene root.
 * @param out_node Output handle to the created SceneInstance node.
 * @return LC_SUCCESS on success, LC_ERROR_NULL_POINTER if scene_path/out_node is
 *         NULL, LC_ERROR_OPERATION_FAILED if no attach parent could be resolved or
 *         the scene failed to load.
 * @threadsafety Main thread only
 */
LC_API LCResult lc_instantiate_scene(const char* scene_path, LCNodeHandle parent_or_null, LCNodeHandle* out_node);

/**
 * @brief Instantiate a prefab file under a parent in one call.
 *
 * Loads the prefab at @p prefab_path, instantiates it, and attaches the produced
 * node to @p parent_or_null (or the current scene root if NULL). C-API equivalent
 * of the scripting `instantiate_prefab()`.
 *
 * @param prefab_path Virtual path to the prefab file to instantiate.
 * @param parent_or_null Parent node handle, or NULL to attach to the current scene root.
 * @param out_node Output handle to the instantiated prefab root node.
 * @return LC_SUCCESS on success, LC_ERROR_NULL_POINTER if prefab_path/out_node is
 *         NULL, LC_ERROR_OPERATION_FAILED if the prefab failed to load/instantiate
 *         or no attach parent could be resolved.
 * @threadsafety Main thread only
 */
LC_API LCResult lc_instantiate_prefab(const char* prefab_path, LCNodeHandle parent_or_null, LCNodeHandle* out_node);

#endif /* LUPINE_CAPI_SCENE_INSTANCE_H */
