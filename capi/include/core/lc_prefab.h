/**
 * @file lc_prefab.h
 * @brief Lupine Engine C API - Prefab system
 *
 * This header provides prefab creation and instantiation.
 * Features:
 * - Create prefabs from existing node hierarchies
 * - Save and load prefabs from files
 * - Instantiate prefabs as standalone or children
 * - UUID regeneration for instances
 */

#ifndef LUPINE_CAPI_PREFAB_H
#define LUPINE_CAPI_PREFAB_H

#include "core/lc_core.h"
#include "core/lc_node.h"

/* ============================================================================
 * Prefab Handle
 * ============================================================================ */

/**
 * @brief Opaque handle to a prefab
 */
typedef struct LCPrefabImpl* LCPrefabHandle;

/* ============================================================================
 * Prefab Creation and Destruction
 * ============================================================================ */

/**
 * @brief Create an empty prefab
 * @param name Optional name for the prefab (can be NULL)
 * @param out_prefab Output parameter for the created prefab handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_prefab_create(const char* name, LCPrefabHandle* out_prefab);

/**
 * @brief Create a prefab from an existing node hierarchy
 * @param root_node Root node of the hierarchy to convert to prefab
 * @param out_prefab Output parameter for the created prefab handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_prefab_create_from_node(LCNodeHandle root_node, LCPrefabHandle* out_prefab);

/**
 * @brief Destroy a prefab and free resources
 * @param prefab Prefab handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_prefab_destroy(LCPrefabHandle prefab);

/* ============================================================================
 * File Operations
 * ============================================================================ */

/**
 * @brief Load a prefab from file
 * @param filepath Path to the .prefab file
 * @param out_prefab Output parameter for the loaded prefab handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_prefab_load(const char* filepath, LCPrefabHandle* out_prefab);

/**
 * @brief Save a prefab to file
 * @param prefab Prefab handle
 * @param filepath Path to save the .prefab file (NULL to use current path)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_prefab_save(LCPrefabHandle prefab, const char* filepath);

/* ============================================================================
 * Properties
 * ============================================================================ */

/**
 * @brief Get prefab name
 * @param prefab Prefab handle
 * @param out_buffer Output buffer for name
 * @param buffer_size Size of output buffer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_prefab_get_name(LCPrefabHandle prefab, char* out_buffer, size_t buffer_size);

/**
 * @brief Set prefab name
 * @param prefab Prefab handle
 * @param name Prefab name
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_prefab_set_name(LCPrefabHandle prefab, const char* name);

/**
 * @brief Get prefab file path
 * @param prefab Prefab handle
 * @param out_buffer Output buffer for path
 * @param buffer_size Size of output buffer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_prefab_get_filepath(LCPrefabHandle prefab, char* out_buffer, size_t buffer_size);

/**
 * @brief Check if prefab is valid (contains node data)
 * @param prefab Prefab handle
 * @param out_valid Output parameter for validity
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_prefab_is_valid(LCPrefabHandle prefab, bool* out_valid);

/* ============================================================================
 * Instantiation
 * ============================================================================ */

/**
 * @brief Instantiate prefab as a standalone node hierarchy
 * @param prefab Prefab handle
 * @param out_root Output parameter for the instantiated root node
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_prefab_instantiate(LCPrefabHandle prefab, LCNodeHandle* out_root);

/**
 * @brief Instantiate prefab as a child of a parent node
 * @param prefab Prefab handle
 * @param parent Parent node to attach to
 * @param out_root Output parameter for the instantiated root node
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_prefab_instantiate_as_child(LCPrefabHandle prefab, LCNodeHandle parent, LCNodeHandle* out_root);

/* ============================================================================
 * Utility
 * ============================================================================ */

/**
 * @brief Clear all prefab data
 * @param prefab Prefab handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_prefab_clear(LCPrefabHandle prefab);

#endif /* LUPINE_CAPI_PREFAB_H */
