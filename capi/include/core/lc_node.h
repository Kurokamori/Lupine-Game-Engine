/**
 * @file lc_node.h
 * @brief Lupine Engine C API - Node system
 *
 * This header provides the node hierarchy and scene graph functionality.
 * Nodes form a tree structure and can have components attached.
 */

#ifndef LUPINE_CAPI_NODE_H
#define LUPINE_CAPI_NODE_H

#include "lc_core.h"
#include "..\math/lc_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Node Handle Types
 * ============================================================================ */

/**
 * @brief Opaque handle to a Node
 */
typedef struct LCNode* LCNodeHandle;

/**
 * @brief Opaque handle to a Component
 */
typedef struct LCComponent* LCComponentHandle;

/**
 * @brief Node type enumeration
 */
typedef enum LCNodeType {
    LC_NODE_BASE = 0,   /**< Base Node (no spatial transform) */
    LC_NODE_2D = 1,     /**< Node2D (2D spatial transform) */
    LC_NODE_3D = 2      /**< Node3D (3D spatial transform) */
} LCNodeType;

/* ============================================================================
 * Node Creation and Destruction
 * ============================================================================ */

/**
 * @brief Create a new node
 * @param type Type of node to create
 * @param name Optional name (can be NULL for default name)
 * @param out_node Output parameter for the created node handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node_create(LCNodeType type, const char* name, LCNodeHandle* out_node);

/**
 * @brief Destroy a node and all its children
 * @param node Node handle to destroy
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 * @note This will remove the node from its parent and destroy all children recursively
 */
LC_API LCResult lc_node_destroy(LCNodeHandle node);

/**
 * @brief Get the type of a node
 * @param node Node handle
 * @param out_type Output parameter for the node type
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node_get_type(LCNodeHandle node, LCNodeType* out_type);

/* ============================================================================
 * Node Properties
 * ============================================================================ */

/**
 * @brief Get the name of a node
 * @param node Node handle
 * @return Node name string - owned by engine, do not free. NULL on error.
 * @threadsafety Thread-safe
 */
LC_API const char* lc_node_get_name(LCNodeHandle node);

/**
 * @brief Set the name of a node
 * @param node Node handle
 * @param name New name for the node
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node_set_name(LCNodeHandle node, const char* name);

/**
 * @brief Get the UUID of a node as a string
 * @param node Node handle
 * @return UUID string - owned by engine, do not free. NULL on error.
 * @threadsafety Thread-safe
 */
LC_API const char* lc_node_get_uuid(LCNodeHandle node);

/**
 * @brief Get whether a node is active
 * @param node Node handle
 * @return true if active, false otherwise
 * @threadsafety Thread-safe
 */
LC_API bool lc_node_is_active(LCNodeHandle node);

/**
 * @brief Set whether a node is active
 * @param node Node handle
 * @param active New active state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node_set_active(LCNodeHandle node, bool active);

/**
 * @brief Get whether a node is active in the hierarchy
 * @param node Node handle
 * @return true if active in hierarchy, false otherwise
 * @threadsafety Thread-safe
 * @note A node is active in hierarchy if it and all its ancestors are active
 */
LC_API bool lc_node_is_active_in_hierarchy(LCNodeHandle node);

/**
 * @brief Get whether a node is visible
 * @param node Node handle
 * @return true if visible, false otherwise
 * @threadsafety Thread-safe
 */
LC_API bool lc_node_is_visible(LCNodeHandle node);

/**
 * @brief Set whether a node is visible
 * @param node Node handle
 * @param visible New visible state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node_set_visible(LCNodeHandle node, bool visible);

/**
 * @brief Get whether a node is visible in the hierarchy
 * @param node Node handle
 * @return true if visible in hierarchy, false otherwise
 * @threadsafety Thread-safe
 * @note A node is visible in hierarchy if it and all its ancestors are visible
 */
LC_API bool lc_node_is_visible_in_hierarchy(LCNodeHandle node);

/* ============================================================================
 * Node Hierarchy Management
 * ============================================================================ */

/**
 * @brief Add a child node to a parent node
 * @param parent Parent node handle
 * @param child Child node handle to add
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 * @note The child will be removed from its current parent if it has one
 */
LC_API LCResult lc_node_add_child(LCNodeHandle parent, LCNodeHandle child);

/**
 * @brief Insert a child node at a specific index
 * @param parent Parent node handle
 * @param child Child node handle to insert
 * @param index Index at which to insert the child
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node_insert_child(LCNodeHandle parent, LCNodeHandle child, size_t index);

/**
 * @brief Remove a child node from a parent node
 * @param parent Parent node handle
 * @param child Child node handle to remove
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node_remove_child(LCNodeHandle parent, LCNodeHandle child);

/**
 * @brief Remove a child node by name
 * @param parent Parent node handle
 * @param child_name Name of child to remove
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node_remove_child_by_name(LCNodeHandle parent, const char* child_name);

/**
 * @brief Get the parent of a node
 * @param node Node handle
 * @param out_parent Output parameter for parent node handle (can be NULL if no parent)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node_get_parent(LCNodeHandle node, LCNodeHandle* out_parent);

/**
 * @brief Get the number of children of a node
 * @param node Node handle
 * @param out_count Output parameter for child count
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node_get_child_count(LCNodeHandle node, size_t* out_count);

/**
 * @brief Get a child node by index
 * @param node Node handle
 * @param index Index of child to get
 * @param out_child Output parameter for child node handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node_get_child(LCNodeHandle node, size_t index, LCNodeHandle* out_child);

/**
 * @brief Get a child node by name
 * @param node Node handle
 * @param name Name of child to find
 * @param out_child Output parameter for child node handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node_get_child_by_name(LCNodeHandle node, const char* name, LCNodeHandle* out_child);

/**
 * @brief Get all children of a node
 * @param node Node handle
 * @param out_children Output array for child node handles (can be NULL to get count only)
 * @param max_count Maximum number of children to retrieve
 * @param out_count Output parameter for actual number of children
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node_get_children(LCNodeHandle node, LCNodeHandle* out_children, size_t max_count, size_t* out_count);

/**
 * @brief Find a node in the hierarchy by path
 * @param node Node handle (starting point for search)
 * @param path Path to node (e.g., "Parent/Child/GrandChild")
 * @param out_found Output parameter for found node handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node_find(LCNodeHandle node, const char* path, LCNodeHandle* out_found);

/**
 * @brief Get the path of a node in the hierarchy
 * @param node Node handle
 * @return Path string - owned by engine, do not free. NULL on error.
 * @threadsafety Thread-safe
 */
LC_API const char* lc_node_get_path(LCNodeHandle node);

/* ============================================================================
 * Node2D Transform Operations
 * ============================================================================ */

/**
 * @brief Set the local position of a Node2D
 * @param node Node handle (must be Node2D)
 * @param position Local position
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node2d_set_position(LCNodeHandle node, LCVec2 position);

/**
 * @brief Get the local position of a Node2D
 * @param node Node handle (must be Node2D)
 * @param out_position Output parameter for local position
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node2d_get_position(LCNodeHandle node, LCVec2* out_position);

/**
 * @brief Set the local rotation of a Node2D (in radians)
 * @param node Node handle (must be Node2D)
 * @param rotation Local rotation in radians
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node2d_set_rotation(LCNodeHandle node, float rotation);

/**
 * @brief Get the local rotation of a Node2D (in radians)
 * @param node Node handle (must be Node2D)
 * @param out_rotation Output parameter for local rotation in radians
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node2d_get_rotation(LCNodeHandle node, float* out_rotation);

/**
 * @brief Set the local scale of a Node2D
 * @param node Node handle (must be Node2D)
 * @param scale Local scale
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node2d_set_scale(LCNodeHandle node, LCVec2 scale);

/**
 * @brief Get the local scale of a Node2D
 * @param node Node handle (must be Node2D)
 * @param out_scale Output parameter for local scale
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node2d_get_scale(LCNodeHandle node, LCVec2* out_scale);

/**
 * @brief Set the z-index of a Node2D (for render ordering)
 * @param node Node handle (must be Node2D)
 * @param z_index Z-index value
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node2d_set_z_index(LCNodeHandle node, int z_index);

/**
 * @brief Get the z-index of a Node2D
 * @param node Node handle (must be Node2D)
 * @param out_z_index Output parameter for z-index
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node2d_get_z_index(LCNodeHandle node, int* out_z_index);

/**
 * @brief Get the global position of a Node2D (considering parent hierarchy)
 * @param node Node handle (must be Node2D)
 * @param out_position Output parameter for global position
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node2d_get_global_position(LCNodeHandle node, LCVec2* out_position);

/**
 * @brief Get the global rotation of a Node2D (considering parent hierarchy)
 * @param node Node handle (must be Node2D)
 * @param out_rotation Output parameter for global rotation in radians
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node2d_get_global_rotation(LCNodeHandle node, float* out_rotation);

/**
 * @brief Get the global scale of a Node2D (considering parent hierarchy)
 * @param node Node handle (must be Node2D)
 * @param out_scale Output parameter for global scale
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node2d_get_global_scale(LCNodeHandle node, LCVec2* out_scale);

/**
 * @brief Get the local transform matrix of a Node2D
 * @param node Node handle (must be Node2D)
 * @param out_matrix Output parameter for transform matrix
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node2d_get_transform_matrix(LCNodeHandle node, LCMat4* out_matrix);

/**
 * @brief Get the global transform matrix of a Node2D
 * @param node Node handle (must be Node2D)
 * @param out_matrix Output parameter for global transform matrix
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node2d_get_global_transform_matrix(LCNodeHandle node, LCMat4* out_matrix);

/* ============================================================================
 * Node3D Transform Operations
 * ============================================================================ */

/**
 * @brief Set the local position of a Node3D
 * @param node Node handle (must be Node3D)
 * @param position Local position
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node3d_set_position(LCNodeHandle node, LCVec3 position);

/**
 * @brief Get the local position of a Node3D
 * @param node Node handle (must be Node3D)
 * @param out_position Output parameter for local position
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node3d_get_position(LCNodeHandle node, LCVec3* out_position);

/**
 * @brief Set the local rotation of a Node3D (as quaternion)
 * @param node Node handle (must be Node3D)
 * @param rotation Local rotation quaternion
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node3d_set_rotation(LCNodeHandle node, LCQuat rotation);

/**
 * @brief Get the local rotation of a Node3D (as quaternion)
 * @param node Node handle (must be Node3D)
 * @param out_rotation Output parameter for local rotation quaternion
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node3d_get_rotation(LCNodeHandle node, LCQuat* out_rotation);

/**
 * @brief Set the local rotation of a Node3D from Euler angles
 * @param node Node handle (must be Node3D)
 * @param euler Euler angles (pitch, yaw, roll) in radians
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node3d_set_rotation_euler(LCNodeHandle node, LCVec3 euler);

/**
 * @brief Get the local rotation of a Node3D as Euler angles
 * @param node Node handle (must be Node3D)
 * @param out_euler Output parameter for Euler angles (pitch, yaw, roll) in radians
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node3d_get_rotation_euler(LCNodeHandle node, LCVec3* out_euler);

/**
 * @brief Set the local scale of a Node3D
 * @param node Node handle (must be Node3D)
 * @param scale Local scale
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node3d_set_scale(LCNodeHandle node, LCVec3 scale);

/**
 * @brief Get the local scale of a Node3D
 * @param node Node handle (must be Node3D)
 * @param out_scale Output parameter for local scale
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node3d_get_scale(LCNodeHandle node, LCVec3* out_scale);

/**
 * @brief Get the global position of a Node3D (considering parent hierarchy)
 * @param node Node handle (must be Node3D)
 * @param out_position Output parameter for global position
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node3d_get_global_position(LCNodeHandle node, LCVec3* out_position);

/**
 * @brief Get the global rotation of a Node3D (considering parent hierarchy)
 * @param node Node handle (must be Node3D)
 * @param out_rotation Output parameter for global rotation quaternion
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node3d_get_global_rotation(LCNodeHandle node, LCQuat* out_rotation);

/**
 * @brief Get the global scale of a Node3D (considering parent hierarchy)
 * @param node Node handle (must be Node3D)
 * @param out_scale Output parameter for global scale
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node3d_get_global_scale(LCNodeHandle node, LCVec3* out_scale);

/**
 * @brief Get the local transform matrix of a Node3D
 * @param node Node handle (must be Node3D)
 * @param out_matrix Output parameter for transform matrix
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node3d_get_transform_matrix(LCNodeHandle node, LCMat4* out_matrix);

/**
 * @brief Get the global transform matrix of a Node3D
 * @param node Node handle (must be Node3D)
 * @param out_matrix Output parameter for global transform matrix
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node3d_get_global_transform_matrix(LCNodeHandle node, LCMat4* out_matrix);

/**
 * @brief Make a Node3D look at a target position
 * @param node Node handle (must be Node3D)
 * @param target Target position to look at
 * @param up Up vector (usually lc_vec3_up())
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node3d_look_at(LCNodeHandle node, LCVec3 target, LCVec3 up);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_NODE_H */
