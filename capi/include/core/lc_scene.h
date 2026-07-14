/**
 * @file lc_scene.h
 * @brief Lupine Engine C API - Scene management
 *
 * This header provides scene graph management and serialization functionality.
 * Scenes contain a tree of nodes and can be saved/loaded from .scene files.
 */

#ifndef LUPINE_CAPI_SCENE_H
#define LUPINE_CAPI_SCENE_H

#include "lc_core.h"
#include "lc_node.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Scene Handle Type
 * ============================================================================ */

/**
 * @brief Opaque handle to a Scene
 */
typedef struct LCScene* LCSceneHandle;

/* ============================================================================
 * Scene Creation and Destruction
 * ============================================================================ */

/**
 * @brief Create a new scene
 * @param name Optional name for the scene (can be NULL)
 * @param out_scene Output parameter for the created scene handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scene_create(const char* name, LCSceneHandle* out_scene);

/**
 * @brief Destroy a scene and all its nodes
 * @param scene Scene handle to destroy
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scene_destroy(LCSceneHandle scene);

/* ============================================================================
 * Scene Loading and Saving
 * ============================================================================ */

/**
 * @brief Load a scene from a .scene file
 * @param filepath Path to the scene file
 * @param out_scene Output parameter for the loaded scene handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scene_load(const char* filepath, LCSceneHandle* out_scene);

/**
 * @brief Save a scene to a .scene file
 * @param scene Scene handle
 * @param filepath Path where to save the scene file
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scene_save(LCSceneHandle scene, const char* filepath);

/**
 * @brief Save a scene to its current filepath
 * @param scene Scene handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 * @note The scene must have been previously saved or loaded from a file
 */
LC_API LCResult lc_scene_save_current(LCSceneHandle scene);

/* ============================================================================
 * Scene Properties
 * ============================================================================ */

/**
 * @brief Get the name of a scene
 * @param scene Scene handle
 * @return Scene name - owned by engine, do not free. NULL on error.
 * @threadsafety Thread-safe
 */
LC_API const char* lc_scene_get_name(LCSceneHandle scene);

/**
 * @brief Set the name of a scene
 * @param scene Scene handle
 * @param name New name for the scene
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scene_set_name(LCSceneHandle scene, const char* name);

/**
 * @brief Get the file path of a scene
 * @param scene Scene handle
 * @return Scene file path - owned by engine, do not free. NULL if not saved/loaded.
 * @threadsafety Thread-safe
 */
LC_API const char* lc_scene_get_filepath(LCSceneHandle scene);

/**
 * @brief Check if a scene is initialized
 * @param scene Scene handle
 * @return true if initialized, false otherwise
 * @threadsafety Thread-safe
 */
LC_API bool lc_scene_is_initialized(LCSceneHandle scene);

/**
 * @brief Check if a scene is active
 * @param scene Scene handle
 * @return true if active, false otherwise
 * @threadsafety Thread-safe
 */
LC_API bool lc_scene_is_active(LCSceneHandle scene);

/**
 * @brief Set whether a scene is active
 * @param scene Scene handle
 * @param active New active state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scene_set_active(LCSceneHandle scene, bool active);

/**
 * @brief Check if a scene is in editor mode
 * @param scene Scene handle
 * @return true if in editor mode, false otherwise
 * @threadsafety Thread-safe
 */
LC_API bool lc_scene_is_in_editor(LCSceneHandle scene);

/**
 * @brief Set whether a scene is in editor mode
 * @param scene Scene handle
 * @param in_editor New editor mode state
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scene_set_in_editor(LCSceneHandle scene, bool in_editor);

/* ============================================================================
 * Scene Graph Management
 * ============================================================================ */

/**
 * @brief Set the root node of a scene
 * @param scene Scene handle
 * @param root Root node handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scene_set_root(LCSceneHandle scene, LCNodeHandle root);

/**
 * @brief Get the root node of a scene
 * @param scene Scene handle
 * @param out_root Output parameter for root node handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scene_get_root(LCSceneHandle scene, LCNodeHandle* out_root);

/**
 * @brief Add a node to the scene (as child of root)
 * @param scene Scene handle
 * @param node Node handle to add
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scene_add_node(LCSceneHandle scene, LCNodeHandle node);

/**
 * @brief Remove a node from the scene
 * @param scene Scene handle
 * @param node Node handle to remove
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scene_remove_node(LCSceneHandle scene, LCNodeHandle node);

/**
 * @brief Find a node in the scene by path
 * @param scene Scene handle
 * @param path Path to the node (e.g., "Root/Child/GrandChild")
 * @param out_node Output parameter for found node handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scene_find_node(LCSceneHandle scene, const char* path, LCNodeHandle* out_node);

/**
 * @brief Find a node in the scene by UUID
 * @param scene Scene handle
 * @param uuid UUID string of the node to find
 * @param out_node Output parameter for found node handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_scene_find_node_by_uuid(LCSceneHandle scene, const char* uuid, LCNodeHandle* out_node);

/* ============================================================================
 * Scene Lifecycle
 * ============================================================================ */

/**
 * @brief Initialize a scene (calls OnReady on all nodes)
 * @param scene Scene handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Main thread only
 */
LC_API LCResult lc_scene_initialize(LCSceneHandle scene);

/**
 * @brief Shutdown a scene (calls OnDestroy on all nodes)
 * @param scene Scene handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Main thread only
 */
LC_API LCResult lc_scene_shutdown(LCSceneHandle scene);

/**
 * @brief Update a scene (calls OnProcess on all nodes)
 * @param scene Scene handle
 * @param delta_time Time elapsed since last update in seconds
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Main thread only
 */
LC_API LCResult lc_scene_update(LCSceneHandle scene, float delta_time);

/**
 * @brief Physics update a scene (calls OnPhysicsProcess on all nodes)
 * @param scene Scene handle
 * @param delta_time Time elapsed since last physics update in seconds
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Main thread only
 */
LC_API LCResult lc_scene_physics_update(LCSceneHandle scene, float delta_time);

/**
 * @brief Render a scene (calls OnRender on all nodes)
 * @param scene Scene handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Main thread only
 */
LC_API LCResult lc_scene_render(LCSceneHandle scene);

/**
 * @brief Process input for a scene (calls OnInput on all nodes)
 * @param scene Scene handle
 * @param delta_time Time elapsed since last input update in seconds
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Main thread only
 */
LC_API LCResult lc_scene_process_input(LCSceneHandle scene, float delta_time);

/* ============================================================================
 * Runtime Additive (Autoload) Scenes
 *
 * These operate on the active runtime SceneManager rather than a particular
 * scene handle, mirroring the scripting `add_scene()` / `remove_scene()` calls.
 * An additive scene is loaded and parented under the current scene's root so it
 * overlays the running scene without replacing it (HUDs, pause menus, persistent
 * managers). There is no scene handle argument because the active SceneManager is
 * a process-wide singleton, the same one `lc_quit()` acts on.
 * ============================================================================ */

/**
 * @brief Load a scene additively and overlay it on the running scene.
 *
 * The scene at @p scene_path is loaded, its root parented under the current
 * scene's root, scripts/connections/globals wired up, and OnAwake invoked. The
 * scene keeps running until removed with lc_scene_remove_autoload() or the
 * current scene is unloaded. C-API equivalent of the scripting `add_scene()`.
 *
 * @param scene_path Virtual path to the scene file to overlay (e.g. "res://ui/hud.scene")
 * @return LC_SUCCESS on success, LC_ERROR_NULL_POINTER if scene_path is NULL,
 *         LC_ERROR_OPERATION_FAILED if there is no active scene manager or the
 *         scene failed to load.
 * @threadsafety Main thread only
 */
LC_API LCResult lc_scene_add_autoload(const char* scene_path);

/**
 * @brief Remove a previously added additive (autoload) scene.
 *
 * Detaches and shuts down every overlay scene whose source file path matches
 * @p scene_path. Removing a path that was never added is a no-op (still returns
 * LC_SUCCESS). C-API equivalent of the scripting `remove_scene()`.
 *
 * @param scene_path Virtual path that was passed to lc_scene_add_autoload()
 * @return LC_SUCCESS on success, LC_ERROR_NULL_POINTER if scene_path is NULL,
 *         LC_ERROR_OPERATION_FAILED if there is no active scene manager.
 * @threadsafety Main thread only
 */
LC_API LCResult lc_scene_remove_autoload(const char* scene_path);

/* ============================================================================
 * Active Scene Control (runtime SceneManager)
 *
 * These act on the process-wide runtime SceneManager singleton, mirroring the
 * scripting `change_scene()` / `reload_scene()` / `get_current_scene()` calls.
 * ============================================================================ */

/**
 * @brief Request a deferred change to a different scene.
 *
 * The swap is deferred to the host loop rather than performed synchronously, so
 * it is safe to call from inside script/component callbacks (switching now would
 * free the scene that owns the running code). C-API equivalent of the scripting
 * `change_scene()`.
 *
 * @param scene_path Virtual path to the scene file to switch to.
 * @return LC_SUCCESS on success, LC_ERROR_NULL_POINTER if scene_path is NULL,
 *         LC_ERROR_OPERATION_FAILED if there is no active scene manager.
 * @threadsafety Main thread only
 */
LC_API LCResult lc_scene_change(const char* scene_path);

/**
 * @brief Reload the current scene from its source file.
 *
 * Resolves the current scene's file path and switches to it immediately. No-op
 * (still LC_SUCCESS) if there is no current scene or it has no file path. C-API
 * equivalent of the scripting `reload_scene()`.
 *
 * @return LC_SUCCESS on success, LC_ERROR_OPERATION_FAILED if there is no active
 *         scene manager.
 * @threadsafety Main thread only
 */
LC_API LCResult lc_scene_reload(void);

/**
 * @brief Get a handle to the runtime SceneManager's current scene.
 *
 * The returned handle borrows the SceneManager-owned scene; destroying it with
 * lc_scene_destroy only releases the handle, never the underlying scene.
 *
 * @param out_scene Output handle, set to NULL when there is no current scene.
 * @return LC_SUCCESS on success, LC_ERROR_NULL_POINTER if out_scene is NULL,
 *         LC_ERROR_OPERATION_FAILED if there is no active scene manager.
 * @threadsafety Main thread only
 */
LC_API LCResult lc_scene_get_current(LCSceneHandle* out_scene);

/**
 * @brief Copy the current scene's file path into a caller-provided buffer.
 *
 * Writes an empty string when there is no current scene or it has no file path.
 *
 * @param buffer Destination buffer (NUL-terminated on return).
 * @param buffer_size Capacity of @p buffer in bytes.
 * @return LC_SUCCESS on success, LC_ERROR_NULL_POINTER if buffer is NULL,
 *         LC_ERROR_INVALID_PARAMETER if buffer_size is 0,
 *         LC_ERROR_OPERATION_FAILED if there is no active scene manager.
 * @threadsafety Main thread only
 */
LC_API LCResult lc_scene_get_current_path(char* buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_SCENE_H */
