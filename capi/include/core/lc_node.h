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
#include "../math/lc_math.h"

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
 * @brief Opaque handle to a Scene
 * @note Identical to the declaration in lc_scene.h; declared here so node
 *       functions that return scene handles do not require including lc_scene.h.
 */
typedef struct LCScene* LCSceneHandle;

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

/**
 * @brief Get whether a node is exposed as a unique name within its owner scope
 * @param node Node handle
 * @return true if the node can be resolved by unique name (e.g. "%Name")
 * @threadsafety Thread-safe
 */
LC_API bool lc_node_is_unique_name_in_owner(LCNodeHandle node);

/**
 * @brief Set whether a node is exposed as a unique name within its owner scope
 * @param node Node handle
 * @param unique New unique-name state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node_set_unique_name_in_owner(LCNodeHandle node, bool unique);

/**
 * @brief Resolve a uniquely-named node within this node's owner scope
 * @param node Context node whose scope is searched
 * @param name Unique node name to resolve (without the leading '%')
 * @param out_found Receives the resolved node handle, or an invalid handle if none
 * @return LC_SUCCESS on success (even if not found), error code on bad arguments
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node_resolve_unique_name(LCNodeHandle node, const char* name, LCNodeHandle* out_found);

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
 * @brief Reorder an existing direct child to a new position within the parent
 * @param parent Parent node handle
 * @param child Direct child node handle to move
 * @param new_index Target index (clamped to [0, child count - 1])
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 * @note Pure reorder: the child stays in the tree (no reparent, no scene change)
 */
LC_API LCResult lc_node_move_child(LCNodeHandle parent, LCNodeHandle child, size_t new_index);

/**
 * @brief Get the index of a direct child within the parent's child list
 * @param parent Parent node handle
 * @param child Direct child node handle
 * @param out_index Receives the index, or (size_t)-1 if not a direct child
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node_get_child_index(LCNodeHandle parent, LCNodeHandle child, size_t* out_index);

/**
 * @brief Get a node's index within its parent's child list
 * @param node Node handle
 * @param out_index Receives the index, or (size_t)-1 if the node has no parent
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node_get_sibling_index(LCNodeHandle node, size_t* out_index);

/**
 * @brief Move a node to a new position among its siblings
 * @param node Node handle
 * @param index Target index within the parent (clamped to range)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 * @note No-op if the node has no parent
 */
LC_API LCResult lc_node_set_sibling_index(LCNodeHandle node, size_t index);

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
 * Node Tree Utilities
 * ============================================================================ */

/**
 * @brief Find a node by path, returning success even when not found
 * @param node Node handle (starting point for search)
 * @param path Path to the node (e.g., "Parent/Child/GrandChild")
 * @param out_found Receives the found node handle, or an invalid handle if none
 * @return LC_SUCCESS on success (even if not found), error code on bad arguments
 * @threadsafety Thread-safe
 * @note Unlike lc_node_find, a missing node is reported as LC_SUCCESS with a
 *       NULL out_found rather than an error.
 */
LC_API LCResult lc_node_get_node_or_null(LCNodeHandle node, const char* path, LCNodeHandle* out_found);

/**
 * @brief Get the first node belonging to a group within the node's scene
 * @param node Node handle used to resolve the owning scene
 * @param group Group name to search
 * @param out_found Receives the first matching node handle, or an invalid handle if none
 * @return LC_SUCCESS on success (even if not found), error code on bad arguments
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node_get_first_node_in_group(LCNodeHandle node, const char* group, LCNodeHandle* out_found);

/**
 * @brief Find descendant nodes matching a type name
 * @param node Node handle whose subtree is searched
 * @param type_name Type name to match (NULL or empty matches any type)
 * @param recursive Whether to search the full subtree (true) or direct children only (false)
 * @param out_children Output array for matching node handles (can be NULL to get the count only)
 * @param max_count Maximum number of handles to write into out_children
 * @param out_count Receives the total number of matching descendants
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node_find_children(LCNodeHandle node, const char* type_name, bool recursive,
                                      LCNodeHandle* out_children, size_t max_count, size_t* out_count);

/**
 * @brief Check whether a node is an ancestor of another node
 * @param node Candidate ancestor node handle
 * @param other Node handle to test
 * @return true if node is an ancestor of other, false otherwise
 * @threadsafety Thread-safe
 */
LC_API bool lc_node_is_ancestor_of(LCNodeHandle node, LCNodeHandle other);

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

/**
 * @brief Translate a Node2D by a delta in local space
 * @param node Node handle (must be Node2D)
 * @param delta Amount to add to the current local position
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node2d_translate(LCNodeHandle node, LCVec2 delta);

/**
 * @brief Translate a Node3D by a delta in local space
 * @param node Node handle (must be Node3D)
 * @param delta Amount to add to the current local position
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node3d_translate(LCNodeHandle node, LCVec3 delta);

/**
 * @brief Rotate a Node2D by a delta angle (in radians)
 * @param node Node handle (must be Node2D)
 * @param radians Angle in radians to add to the current local rotation
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node2d_rotate(LCNodeHandle node, float radians);

/**
 * @brief Rotate a Node3D by a delta expressed as Euler angles (in radians)
 * @param node Node handle (must be Node3D)
 * @param euler_delta_radians Euler-angle delta (pitch, yaw, roll) in radians
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node3d_rotate_euler(LCNodeHandle node, LCVec3 euler_delta_radians);

/**
 * @brief Set the global position of a Node2D
 * @param node Node handle (must be Node2D)
 * @param position Target global position
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node2d_set_global_position(LCNodeHandle node, LCVec2 position);

/**
 * @brief Set the global position of a Node3D
 * @param node Node handle (must be Node3D)
 * @param position Target global position
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node3d_set_global_position(LCNodeHandle node, LCVec3 position);

/**
 * @brief Set the global rotation of a Node2D (in radians)
 * @param node Node handle (must be Node2D)
 * @param radians Target global rotation in radians
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node2d_set_global_rotation(LCNodeHandle node, float radians);

/**
 * @brief Set the global (world) rotation of a Node3D
 *
 * The node stores a local rotation and composes world = parentWorld * local, so the
 * local rotation written here is parentWorld⁻¹ * rotation.
 * @param node Node handle (must be Node3D)
 * @param rotation Target global rotation as a quaternion
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node3d_set_global_rotation(LCNodeHandle node, LCQuat rotation);

/**
 * @brief Get the global (world) rotation of a Node3D as Euler angles in radians
 * @param node Node handle (must be Node3D)
 * @param out_euler Receives the world rotation as Euler radians
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node3d_get_global_rotation_euler(LCNodeHandle node, LCVec3* out_euler);

/**
 * @brief Set the global (world) rotation of a Node3D from Euler angles in radians
 * @param node Node handle (must be Node3D)
 * @param euler Target world rotation as Euler radians
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node3d_set_global_rotation_euler(LCNodeHandle node, LCVec3 euler);

/**
 * @brief Degree-based convenience rotation API.
 *
 * These mirror the radian/quaternion rotation functions but take and return
 * **degrees** (and, for 3D, Euler degrees), matching the scripting API
 * convention. They are thin wrappers over the same node setters/getters, so the
 * underlying stored rotation is unchanged (2D radians, 3D quaternion). Use these
 * when a degree-based workflow is more convenient; the radian/quaternion
 * functions remain available and authoritative.
 * @threadsafety Thread-safe
 * @{
 */
LC_API LCResult lc_node2d_set_rotation_degrees(LCNodeHandle node, float degrees);
LC_API LCResult lc_node2d_get_rotation_degrees(LCNodeHandle node, float* out_degrees);
LC_API LCResult lc_node2d_rotate_degrees(LCNodeHandle node, float degrees);
LC_API LCResult lc_node2d_get_global_rotation_degrees(LCNodeHandle node, float* out_degrees);
LC_API LCResult lc_node2d_set_global_rotation_degrees(LCNodeHandle node, float degrees);
LC_API LCResult lc_node3d_set_rotation_euler_degrees(LCNodeHandle node, LCVec3 euler_degrees);
LC_API LCResult lc_node3d_get_rotation_euler_degrees(LCNodeHandle node, LCVec3* out_euler_degrees);
LC_API LCResult lc_node3d_rotate_euler_degrees(LCNodeHandle node, LCVec3 euler_delta_degrees);
LC_API LCResult lc_node3d_get_global_rotation_euler_degrees(LCNodeHandle node, LCVec3* out_euler_degrees);
LC_API LCResult lc_node3d_set_global_rotation_euler_degrees(LCNodeHandle node, LCVec3 euler_degrees);
/** @} */

/**
 * @brief Get the forward direction vector of a Node3D
 * @param node Node handle (must be Node3D)
 * @param out_forward Receives the forward direction (rotation applied to -Z)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node3d_get_forward(LCNodeHandle node, LCVec3* out_forward);

/**
 * @brief Get the right direction vector of a Node3D
 * @param node Node handle (must be Node3D)
 * @param out_right Receives the right direction (rotation applied to +X)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node3d_get_right(LCNodeHandle node, LCVec3* out_right);

/**
 * @brief Get the up direction vector of a Node3D
 * @param node Node handle (must be Node3D)
 * @param out_up Receives the up direction (rotation applied to +Y)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node3d_get_up(LCNodeHandle node, LCVec3* out_up);

/**
 * @brief Make a Node2D face a target point in local space
 * @param node Node handle (must be Node2D)
 * @param target Target point to look at
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node2d_look_at(LCNodeHandle node, LCVec2 target);

/**
 * @brief Get the distance from a Node2D to a point
 * @param node Node handle (must be Node2D)
 * @param point Point to measure to
 * @param out_distance Receives the distance
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node2d_distance_to_point(LCNodeHandle node, LCVec2 point, float* out_distance);

/**
 * @brief Get the distance from a Node2D to another Node2D
 * @param node Node handle (must be Node2D)
 * @param other Other node handle (must be Node2D)
 * @param out_distance Receives the distance
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node2d_distance_to_node(LCNodeHandle node, LCNodeHandle other, float* out_distance);

/**
 * @brief Get the distance from a Node3D to a point
 * @param node Node handle (must be Node3D)
 * @param point Point to measure to
 * @param out_distance Receives the distance
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node3d_distance_to_point(LCNodeHandle node, LCVec3 point, float* out_distance);

/**
 * @brief Get the distance from a Node3D to another Node3D
 * @param node Node handle (must be Node3D)
 * @param other Other node handle (must be Node3D)
 * @param out_distance Receives the distance
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node3d_distance_to_node(LCNodeHandle node, LCNodeHandle other, float* out_distance);

/**
 * @brief Move a Node2D toward a target by at most a maximum distance
 * @param node Node handle (must be Node2D)
 * @param target Target position
 * @param max_delta Maximum distance to move this call
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node2d_move_toward(LCNodeHandle node, LCVec2 target, float max_delta);

/**
 * @brief Move a Node3D toward a target by at most a maximum distance
 * @param node Node handle (must be Node3D)
 * @param target Target position
 * @param max_delta Maximum distance to move this call
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node3d_move_toward(LCNodeHandle node, LCVec3 target, float max_delta);

/* ============================================================================
 * Node References and Lifecycle
 * ============================================================================ */

/**
 * @brief Get the scene that owns a node
 * @param node Node handle
 * @param out_scene Receives the scene handle, or an invalid handle if the node has no scene
 * @return LC_SUCCESS on success (even when the node has no scene), error code on bad arguments
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node_get_scene(LCNodeHandle node, LCSceneHandle* out_scene);

/**
 * @brief Get the root node of the scene that owns a node
 * @param node Node handle
 * @param out_root Receives the root node handle, or an invalid handle if none
 * @return LC_SUCCESS on success (even when there is no root), error code on bad arguments
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node_get_root(LCNodeHandle node, LCNodeHandle* out_root);

/**
 * @brief Queue a node for deferred destruction at the end of the current frame
 * @param node Node handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node_queue_free(LCNodeHandle node);

/**
 * @brief Queue a node for deferred destruction on the next deferred flush
 * @param node Node handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node_queue_free_deferred(LCNodeHandle node);

/**
 * @brief Free a node immediately
 * @param node Node handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node_free(LCNodeHandle node);

/**
 * @brief Add a sibling to a node (as a child of the node's parent)
 * @param node Node handle whose parent receives the sibling
 * @param sibling Sibling node handle to add
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node_add_sibling(LCNodeHandle node, LCNodeHandle sibling);

/**
 * @brief Reparent a node to a new parent, preserving the node and its subtree
 * @param node Node handle to reparent
 * @param new_parent New parent node handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node_reparent(LCNodeHandle node, LCNodeHandle new_parent);

/**
 * @brief Get a registered singleton (autoload) node by name
 * @param name Singleton name
 * @param out_node Receives the singleton node handle, or an invalid handle if none
 * @return LC_SUCCESS on success (even when not found), error code on bad arguments
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node_get_singleton(const char* name, LCNodeHandle* out_node);

/**
 * @brief Find a node by UUID within the scene that owns a context node
 * @param node Context node handle used to resolve the owning scene
 * @param uuid UUID string to resolve
 * @param out_found Receives the found node handle, or an invalid handle if none
 * @return LC_SUCCESS on success (even when not found), error code on bad arguments
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node_find_by_uuid(LCNodeHandle node, const char* uuid, LCNodeHandle* out_found);

/* ============================================================================
 * Component Lookups
 * ============================================================================ */

/**
 * @brief Find the first component of a type on a node or any of its descendants
 * @param node Node handle whose subtree is searched (self first, then children)
 * @param type_name Component type name to match
 * @param out_component Receives the component handle, or an invalid handle if none
 * @return LC_SUCCESS on success (even when not found), error code on bad arguments
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node_get_component_in_children(LCNodeHandle node, const char* type_name, LCComponentHandle* out_component);

/**
 * @brief Find the first component of a type on a node's ancestors
 * @param node Node handle whose ancestors are searched
 * @param type_name Component type name to match
 * @param out_component Receives the component handle, or an invalid handle if none
 * @return LC_SUCCESS on success (even when not found), error code on bad arguments
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node_get_component_in_parent(LCNodeHandle node, const char* type_name, LCComponentHandle* out_component);

/* ============================================================================
 * Node Groups
 * ============================================================================ */

/**
 * @brief Add a node to a named group
 * @param node Node handle
 * @param group Group name
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node_add_to_group(LCNodeHandle node, const char* group);

/**
 * @brief Remove a node from a named group
 * @param node Node handle
 * @param group Group name
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node_remove_from_group(LCNodeHandle node, const char* group);

/**
 * @brief Check whether a node belongs to a named group
 * @param node Node handle
 * @param group Group name
 * @param out_in_group Receives true if the node is in the group
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node_is_in_group(LCNodeHandle node, const char* group, bool* out_in_group);

/**
 * @brief Get the groups a node belongs to as a JSON array of strings
 * @param node Node handle
 * @param out_json Receives a newly allocated JSON string (free with lc_free)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node_get_groups_json(LCNodeHandle node, char** out_json);

/**
 * @brief Get all nodes belonging to a group within the node's scene
 * @param node Context node handle used to resolve the owning scene
 * @param group Group name to search
 * @param out_nodes Output array for matching node handles (can be NULL to get the count only)
 * @param max_count Maximum number of handles to write into out_nodes
 * @param out_count Receives the total number of matching nodes
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_node_get_nodes_in_group(LCNodeHandle node, const char* group,
                                           LCNodeHandle* out_nodes, size_t max_count, size_t* out_count);

/* ============================================================================
 * Node Instantiation
 * ============================================================================ */

/**
 * @brief Duplicate a node (and its subtree) attaching the clone to the same parent
 * @param node Node handle to duplicate
 * @param out_node Receives the cloned node handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 * @note The clone receives fresh UUIDs for itself, its components and its descendants.
 */
LC_API LCResult lc_node_duplicate(LCNodeHandle node, LCNodeHandle* out_node);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_NODE_H */
