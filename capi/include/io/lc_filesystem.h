/**
 * @file lc_filesystem.h
 * @brief Lupine Engine C API - File system and Virtual File System access
 *
 * This header exposes the engine's cross-platform file operations
 * (lupine::platform::FileSystem, operating on physical/OS paths) and the
 * Virtual File System (lupine::platform::VirtualFileSystem, operating on
 * virtual paths such as "res://", "user://", "temp://", "app://").
 *
 * Foreign-language bindings use these to read/write game data, enumerate
 * directories, and resolve virtual asset paths without needing the C++ engine.
 *
 * Memory ownership:
 * - Functions with a `char** out_*` parameter heap-allocate a NUL-terminated
 *   UTF-8 string; the caller MUST free it with lc_free().
 * - Functions with a `uint8_t** out_data` parameter heap-allocate a buffer of
 *   `*out_size` bytes; the caller MUST free it with lc_free(). The buffer is
 *   NOT NUL-terminated.
 * - Directory listings and mount-point lists are returned as a heap-allocated
 *   JSON array-of-strings (e.g. ["res://a.png","res://b.png"]); free with
 *   lc_free().
 */

#ifndef LUPINE_CAPI_FILESYSTEM_H
#define LUPINE_CAPI_FILESYSTEM_H

#include "core/lc_core.h"

/* ============================================================================
 * FileSystem - physical/OS path operations
 * ============================================================================ */

/**
 * @brief Read an entire file as a UTF-8 text string.
 * @param path OS path to the file
 * @param out_text Output; receives a heap string the caller frees with lc_free
 * @return LC_SUCCESS, LC_ERROR_FILE_NOT_FOUND, or LC_ERROR_FILE_READ_FAILED
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_fs_read_text(const char* path, char** out_text);

/**
 * @brief Read an entire file as raw bytes.
 * @param path OS path to the file
 * @param out_data Output; receives a heap buffer the caller frees with lc_free
 * @param out_size Output; receives the number of bytes in out_data
 * @return LC_SUCCESS, LC_ERROR_FILE_NOT_FOUND, or LC_ERROR_FILE_READ_FAILED
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_fs_read_binary(const char* path, uint8_t** out_data, size_t* out_size);

/**
 * @brief Write a UTF-8 text string to a file, creating or overwriting it.
 * @param path OS path to the file
 * @param contents NUL-terminated contents to write
 * @return LC_SUCCESS or LC_ERROR_FILE_WRITE_FAILED
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_fs_write_text(const char* path, const char* contents);

/**
 * @brief Write raw bytes to a file, creating or overwriting it.
 * @param path OS path to the file
 * @param data Bytes to write (may be NULL only if size is 0)
 * @param size Number of bytes to write
 * @return LC_SUCCESS or LC_ERROR_FILE_WRITE_FAILED
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_fs_write_binary(const char* path, const uint8_t* data, size_t size);

/**
 * @brief Append a UTF-8 text string to a file.
 * @param path OS path to the file
 * @param contents NUL-terminated contents to append
 * @return LC_SUCCESS or LC_ERROR_FILE_WRITE_FAILED
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_fs_append_text(const char* path, const char* contents);

/**
 * @brief Check if a file or directory exists.
 * @param path OS path to check
 * @param out_exists Output; receives true if the path exists
 * @return LC_SUCCESS or error code
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_fs_exists(const char* path, bool* out_exists);

/**
 * @brief Check if a path points to a regular file.
 * @param path OS path to check
 * @param out_is_file Output; receives true if the path is a file
 * @return LC_SUCCESS or error code
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_fs_is_file(const char* path, bool* out_is_file);

/**
 * @brief Check if a path points to a directory.
 * @param path OS path to check
 * @param out_is_dir Output; receives true if the path is a directory
 * @return LC_SUCCESS or error code
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_fs_is_directory(const char* path, bool* out_is_dir);

/**
 * @brief Get the size of a file in bytes.
 * @param path OS path to the file
 * @param out_size Output; receives the file size in bytes
 * @return LC_SUCCESS or LC_ERROR_FILE_NOT_FOUND
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_fs_get_file_size(const char* path, uint64_t* out_size);

/**
 * @brief Get the last modification time of a file (Unix timestamp, seconds).
 * @param path OS path to the file
 * @param out_time Output; receives the modification time
 * @return LC_SUCCESS or LC_ERROR_FILE_NOT_FOUND
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_fs_get_modification_time(const char* path, int64_t* out_time);

/**
 * @brief Create a directory.
 * @param path OS path to the directory
 * @param recursive If true, create parent directories as needed
 * @return LC_SUCCESS or LC_ERROR_OPERATION_FAILED
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_fs_create_directory(const char* path, bool recursive);

/**
 * @brief Delete a file.
 * @param path OS path to the file
 * @return LC_SUCCESS or LC_ERROR_OPERATION_FAILED
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_fs_delete_file(const char* path);

/**
 * @brief Delete a directory.
 * @param path OS path to the directory
 * @param recursive If true, delete contents recursively
 * @return LC_SUCCESS or LC_ERROR_OPERATION_FAILED
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_fs_delete_directory(const char* path, bool recursive);

/**
 * @brief Copy a file.
 * @param source Source OS path
 * @param destination Destination OS path
 * @param overwrite If true, overwrite an existing destination
 * @return LC_SUCCESS or LC_ERROR_OPERATION_FAILED
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_fs_copy_file(const char* source, const char* destination, bool overwrite);

/**
 * @brief Move or rename a file.
 * @param source Source OS path
 * @param destination Destination OS path
 * @return LC_SUCCESS or LC_ERROR_OPERATION_FAILED
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_fs_move_file(const char* source, const char* destination);

/**
 * @brief List the contents of a directory as a JSON array of OS paths.
 * @param path OS path to the directory
 * @param recursive If true, list recursively
 * @param out_json Output; receives a heap JSON array string, freed with lc_free
 * @return LC_SUCCESS or LC_ERROR_OPERATION_FAILED
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_fs_list_directory(const char* path, bool recursive, char** out_json);

/**
 * @brief Get the current working directory.
 * @param out_path Output; receives a heap string the caller frees with lc_free
 * @return LC_SUCCESS or LC_ERROR_OPERATION_FAILED
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_fs_get_current_directory(char** out_path);

/**
 * @brief Set the current working directory.
 * @param path OS path to set as the working directory
 * @return LC_SUCCESS or LC_ERROR_OPERATION_FAILED
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_fs_set_current_directory(const char* path);

/**
 * @brief Resolve a relative path to an absolute OS path.
 * @param path Relative OS path
 * @param out_path Output; receives a heap string the caller frees with lc_free
 * @return LC_SUCCESS or LC_ERROR_OPERATION_FAILED
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_fs_get_absolute_path(const char* path, char** out_path);

/**
 * @brief Resolve a path to its canonical form (symlinks resolved, normalized).
 * @param path OS path to resolve
 * @param out_path Output; receives a heap string the caller frees with lc_free
 * @return LC_SUCCESS or LC_ERROR_OPERATION_FAILED
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_fs_get_canonical_path(const char* path, char** out_path);

/* ============================================================================
 * VirtualFileSystem - virtual path operations (res://, user://, temp://, ...)
 * ============================================================================ */

/**
 * @brief Check whether the VFS has been initialized.
 * @param out_initialized Output; receives true if initialized
 * @return LC_SUCCESS or error code
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_vfs_is_initialized(bool* out_initialized);

/**
 * @brief Mount a physical directory to a virtual path prefix.
 * @param mount_point Virtual prefix (e.g. "res://")
 * @param physical_path Physical OS path to map the prefix to
 * @param read_only If true, write operations through this prefix fail
 * @return LC_SUCCESS or LC_ERROR_OPERATION_FAILED
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_vfs_mount(const char* mount_point, const char* physical_path, bool read_only);

/**
 * @brief Unmount a virtual path prefix.
 * @param mount_point Virtual prefix to unmount
 * @return LC_SUCCESS or LC_ERROR_OPERATION_FAILED
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_vfs_unmount(const char* mount_point);

/**
 * @brief Check whether a virtual path prefix is mounted.
 * @param mount_point Virtual prefix to check
 * @param out_mounted Output; receives true if mounted
 * @return LC_SUCCESS or error code
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_vfs_is_mounted(const char* mount_point, bool* out_mounted);

/**
 * @brief Get the physical path backing a mount point.
 * @param mount_point Virtual prefix
 * @param out_path Output; receives a heap string the caller frees with lc_free
 * @return LC_SUCCESS or LC_ERROR_NOT_FOUND
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_vfs_get_mount_path(const char* mount_point, char** out_path);

/**
 * @brief List all registered mount-point prefixes as a JSON array of strings.
 * @param out_json Output; receives a heap JSON array string, freed with lc_free
 * @return LC_SUCCESS or error code
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_vfs_get_mount_points(char** out_json);

/**
 * @brief Resolve a virtual path to a physical OS path.
 * @param virtual_path Virtual path (e.g. "res://textures/icon.png")
 * @param out_path Output; receives a heap string the caller frees with lc_free
 * @return LC_SUCCESS or LC_ERROR_PATH_INVALID
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_vfs_resolve_path(const char* virtual_path, char** out_path);

/**
 * @brief Resolve a virtual path or asset UUID to a physical OS path.
 *        UUID resolution is attempted first, then path resolution.
 * @param path_or_uuid Virtual path or UUID string
 * @param out_path Output; receives a heap string the caller frees with lc_free
 * @return LC_SUCCESS or LC_ERROR_PATH_INVALID
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_vfs_resolve_asset(const char* path_or_uuid, char** out_path);

/**
 * @brief Read a file via a virtual path as a UTF-8 text string.
 * @param virtual_path Virtual path to the file
 * @param out_text Output; receives a heap string the caller frees with lc_free
 * @return LC_SUCCESS, LC_ERROR_FILE_NOT_FOUND, or LC_ERROR_FILE_READ_FAILED
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_vfs_read_text(const char* virtual_path, char** out_text);

/**
 * @brief Read a file via a virtual path as raw bytes.
 * @param virtual_path Virtual path to the file
 * @param out_data Output; receives a heap buffer the caller frees with lc_free
 * @param out_size Output; receives the number of bytes in out_data
 * @return LC_SUCCESS, LC_ERROR_FILE_NOT_FOUND, or LC_ERROR_FILE_READ_FAILED
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_vfs_read_binary(const char* virtual_path, uint8_t** out_data, size_t* out_size);

/**
 * @brief Write a UTF-8 text string via a virtual path.
 * @param virtual_path Virtual path to the file
 * @param contents NUL-terminated contents to write
 * @return LC_SUCCESS or LC_ERROR_FILE_WRITE_FAILED
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_vfs_write_text(const char* virtual_path, const char* contents);

/**
 * @brief Write raw bytes via a virtual path.
 * @param virtual_path Virtual path to the file
 * @param data Bytes to write (may be NULL only if size is 0)
 * @param size Number of bytes to write
 * @return LC_SUCCESS or LC_ERROR_FILE_WRITE_FAILED
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_vfs_write_binary(const char* virtual_path, const uint8_t* data, size_t size);

/**
 * @brief Check if a file or directory exists via a virtual path.
 * @param virtual_path Virtual path to check
 * @param out_exists Output; receives true if the path exists
 * @return LC_SUCCESS or error code
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_vfs_exists(const char* virtual_path, bool* out_exists);

/**
 * @brief Check if a virtual path points to a regular file.
 * @param virtual_path Virtual path to check
 * @param out_is_file Output; receives true if the path is a file
 * @return LC_SUCCESS or error code
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_vfs_is_file(const char* virtual_path, bool* out_is_file);

/**
 * @brief Check if a virtual path points to a directory.
 * @param virtual_path Virtual path to check
 * @param out_is_dir Output; receives true if the path is a directory
 * @return LC_SUCCESS or error code
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_vfs_is_directory(const char* virtual_path, bool* out_is_dir);

/**
 * @brief Get the size of a file in bytes via a virtual path.
 * @param virtual_path Virtual path to the file
 * @param out_size Output; receives the file size in bytes
 * @return LC_SUCCESS or LC_ERROR_FILE_NOT_FOUND
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_vfs_get_file_size(const char* virtual_path, uint64_t* out_size);

/**
 * @brief Get the modification time (Unix timestamp) of a file via virtual path.
 * @param virtual_path Virtual path to the file
 * @param out_time Output; receives the modification time
 * @return LC_SUCCESS or LC_ERROR_FILE_NOT_FOUND
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_vfs_get_modification_time(const char* virtual_path, int64_t* out_time);

/**
 * @brief Create a directory via a virtual path.
 * @param virtual_path Virtual path to the directory
 * @param recursive If true, create parent directories as needed
 * @return LC_SUCCESS or LC_ERROR_OPERATION_FAILED
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_vfs_create_directory(const char* virtual_path, bool recursive);

/**
 * @brief Delete a file via a virtual path.
 * @param virtual_path Virtual path to the file
 * @return LC_SUCCESS or LC_ERROR_OPERATION_FAILED
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_vfs_delete_file(const char* virtual_path);

/**
 * @brief List the contents of a directory via a virtual path as a JSON array
 *        of virtual paths.
 * @param virtual_path Virtual path to the directory
 * @param recursive If true, list recursively
 * @param out_json Output; receives a heap JSON array string, freed with lc_free
 * @return LC_SUCCESS or LC_ERROR_OPERATION_FAILED
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_vfs_list_directory(const char* virtual_path, bool recursive, char** out_json);

/**
 * @brief Copy a file using virtual paths.
 * @param source_virtual_path Source virtual path
 * @param dest_virtual_path Destination virtual path
 * @param overwrite If true, overwrite an existing destination
 * @return LC_SUCCESS or LC_ERROR_OPERATION_FAILED
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_vfs_copy_file(const char* source_virtual_path, const char* dest_virtual_path, bool overwrite);

/**
 * @brief Test whether a virtual path is permitted under the scripting sandbox
 *        rule: it must start with res://, user:// or temp://, and must not
 *        contain any ".." traversal segment or embedded NUL.
 * @param path Virtual path to test
 * @param out_ok Output; receives true if the path is allowed by the sandbox
 * @return LC_SUCCESS or error code
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_vfs_is_sandboxed_path(const char* path, bool* out_ok);

/**
 * @brief Append a UTF-8 text string to a file via a virtual path, creating it if
 *        it does not yet exist.
 * @param virtual_path Virtual path to the file
 * @param contents NUL-terminated contents to append
 * @return LC_SUCCESS or LC_ERROR_FILE_WRITE_FAILED
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_vfs_append_text(const char* virtual_path, const char* contents);

/**
 * @brief Get the platform default user-data directory for an application.
 * @param app_name Application name (may be NULL for the default "Lupine")
 * @param out_path Output; receives a heap string the caller frees with lc_free
 * @return LC_SUCCESS or error code
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_vfs_get_default_user_data_path(const char* app_name, char** out_path);

/**
 * @brief Get the platform default temporary directory.
 * @param out_path Output; receives a heap string the caller frees with lc_free
 * @return LC_SUCCESS or error code
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_vfs_get_default_temp_path(char** out_path);

#endif /* LUPINE_CAPI_FILESYSTEM_H */
