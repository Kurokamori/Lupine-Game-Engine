/**
 * @file lc_filesystem.cpp
 * @brief Implementation of the FileSystem / VirtualFileSystem C API.
 *
 * Thin marshaling layer over lupine::platform::FileSystem (static, physical
 * paths) and lupine::platform::VirtualFileSystem (singleton, virtual paths).
 * Heap results follow the reflection-bridge convention: caller frees with
 * lc_free(); dynamic string lists are returned as JSON arrays of strings.
 */

#include "io/lc_filesystem.h"
#include "../core/lc_internal.h"

#include <lupine/platform/FileSystem.hpp>
#include <lupine/platform/VirtualFileSystem.hpp>

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

LCResult WriteAllocatedString(const std::string& value, char** out_str) {
    if (!out_str) {
        SetError(LC_ERROR_NULL_POINTER, "output string pointer is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    char* buffer = DuplicateString(value);
    if (!buffer) {
        SetError(LC_ERROR_OUT_OF_MEMORY, "Failed to allocate string result");
        return LC_ERROR_OUT_OF_MEMORY;
    }
    *out_str = buffer;
    return LC_SUCCESS;
}

LCResult WriteAllocatedBytes(const std::vector<uint8_t>& value, uint8_t** out_data, size_t* out_size) {
    if (!out_data || !out_size) {
        SetError(LC_ERROR_NULL_POINTER, "output buffer pointer is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (value.empty()) {
        *out_data = nullptr;
        *out_size = 0;
        return LC_SUCCESS;
    }
    uint8_t* buffer = static_cast<uint8_t*>(std::malloc(value.size()));
    if (!buffer) {
        SetError(LC_ERROR_OUT_OF_MEMORY, "Failed to allocate binary result");
        return LC_ERROR_OUT_OF_MEMORY;
    }
    std::memcpy(buffer, value.data(), value.size());
    *out_data = buffer;
    *out_size = value.size();
    return LC_SUCCESS;
}

LCResult WriteStringArrayJson(const std::vector<std::string>& values, char** out_json) {
    if (!out_json) {
        SetError(LC_ERROR_NULL_POINTER, "out_json is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    nlohmann::json array = nlohmann::json::array();
    for (const std::string& value : values) {
        array.push_back(value);
    }
    return WriteAllocatedString(array.dump(), out_json);
}

LCResult MapFileError(const std::string& message, LCResult fallback) {
    SetError(fallback, message.empty() ? "File operation failed" : message.c_str());
    return fallback;
}

lupine::platform::VirtualFileSystem& VFS() {
    return lupine::platform::VirtualFileSystem::GetInstance();
}

// Mirrors lupine::scripting::ScriptAPI::IsSandboxedPath: only res://, user://
// and temp:// roots are permitted, and any ".." traversal or embedded NUL is
// rejected.
bool IsSandboxedPath(const std::string& path) {
    const bool allowedPrefix =
        path.rfind("res://", 0) == 0 ||
        path.rfind("user://", 0) == 0 ||
        path.rfind("temp://", 0) == 0;
    if (!allowedPrefix) {
        return false;
    }
    if (path.find("..") != std::string::npos) {
        return false;
    }
    if (path.find('\0') != std::string::npos) {
        return false;
    }
    return true;
}

} // anonymous namespace

/* ============================================================================
 * FileSystem - physical/OS path operations
 * ============================================================================ */

LC_API LCResult lc_fs_read_text(const char* path, char** out_text) {
    if (!path || !out_text) {
        SetError(LC_ERROR_NULL_POINTER, "path or out_text is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto result = lupine::platform::FileSystem::ReadFile(path);
        if (!result.success) {
            return MapFileError(result.error, LC_ERROR_FILE_READ_FAILED);
        }
        return WriteAllocatedString(result.data, out_text);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception reading file");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_fs_read_binary(const char* path, uint8_t** out_data, size_t* out_size) {
    if (!path || !out_data || !out_size) {
        SetError(LC_ERROR_NULL_POINTER, "path, out_data or out_size is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto result = lupine::platform::FileSystem::ReadBinaryFile(path);
        if (!result.success) {
            return MapFileError(result.error, LC_ERROR_FILE_READ_FAILED);
        }
        return WriteAllocatedBytes(result.data, out_data, out_size);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception reading binary file");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_fs_write_text(const char* path, const char* contents) {
    if (!path || !contents) {
        SetError(LC_ERROR_NULL_POINTER, "path or contents is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto result = lupine::platform::FileSystem::WriteFile(path, contents);
        return result.success ? LC_SUCCESS : MapFileError(result.error, LC_ERROR_FILE_WRITE_FAILED);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception writing file");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_fs_write_binary(const char* path, const uint8_t* data, size_t size) {
    if (!path || (!data && size != 0)) {
        SetError(LC_ERROR_NULL_POINTER, "path or data is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        std::vector<uint8_t> bytes(data, data + size);
        auto result = lupine::platform::FileSystem::WriteBinaryFile(path, bytes);
        return result.success ? LC_SUCCESS : MapFileError(result.error, LC_ERROR_FILE_WRITE_FAILED);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception writing binary file");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_fs_append_text(const char* path, const char* contents) {
    if (!path || !contents) {
        SetError(LC_ERROR_NULL_POINTER, "path or contents is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto result = lupine::platform::FileSystem::AppendFile(path, contents);
        return result.success ? LC_SUCCESS : MapFileError(result.error, LC_ERROR_FILE_WRITE_FAILED);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception appending file");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_fs_exists(const char* path, bool* out_exists) {
    if (!path || !out_exists) {
        SetError(LC_ERROR_NULL_POINTER, "path or out_exists is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        *out_exists = lupine::platform::FileSystem::Exists(path);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception checking existence");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_fs_is_file(const char* path, bool* out_is_file) {
    if (!path || !out_is_file) {
        SetError(LC_ERROR_NULL_POINTER, "path or out_is_file is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        *out_is_file = lupine::platform::FileSystem::IsFile(path);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception checking file type");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_fs_is_directory(const char* path, bool* out_is_dir) {
    if (!path || !out_is_dir) {
        SetError(LC_ERROR_NULL_POINTER, "path or out_is_dir is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        *out_is_dir = lupine::platform::FileSystem::IsDirectory(path);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception checking directory type");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_fs_get_file_size(const char* path, uint64_t* out_size) {
    if (!path || !out_size) {
        SetError(LC_ERROR_NULL_POINTER, "path or out_size is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto result = lupine::platform::FileSystem::GetFileSize(path);
        if (!result.success) {
            return MapFileError(result.error, LC_ERROR_FILE_NOT_FOUND);
        }
        *out_size = result.data;
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception getting file size");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_fs_get_modification_time(const char* path, int64_t* out_time) {
    if (!path || !out_time) {
        SetError(LC_ERROR_NULL_POINTER, "path or out_time is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto result = lupine::platform::FileSystem::GetModificationTime(path);
        if (!result.success) {
            return MapFileError(result.error, LC_ERROR_FILE_NOT_FOUND);
        }
        *out_time = result.data;
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception getting modification time");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_fs_create_directory(const char* path, bool recursive) {
    if (!path) {
        SetError(LC_ERROR_NULL_POINTER, "path is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto result = lupine::platform::FileSystem::CreateDirectory(path, recursive);
        return result.success ? LC_SUCCESS : MapFileError(result.error, LC_ERROR_OPERATION_FAILED);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception creating directory");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_fs_delete_file(const char* path) {
    if (!path) {
        SetError(LC_ERROR_NULL_POINTER, "path is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto result = lupine::platform::FileSystem::DeleteFile(path);
        return result.success ? LC_SUCCESS : MapFileError(result.error, LC_ERROR_OPERATION_FAILED);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception deleting file");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_fs_delete_directory(const char* path, bool recursive) {
    if (!path) {
        SetError(LC_ERROR_NULL_POINTER, "path is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto result = lupine::platform::FileSystem::DeleteDirectory(path, recursive);
        return result.success ? LC_SUCCESS : MapFileError(result.error, LC_ERROR_OPERATION_FAILED);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception deleting directory");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_fs_copy_file(const char* source, const char* destination, bool overwrite) {
    if (!source || !destination) {
        SetError(LC_ERROR_NULL_POINTER, "source or destination is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto result = lupine::platform::FileSystem::CopyFile(source, destination, overwrite);
        return result.success ? LC_SUCCESS : MapFileError(result.error, LC_ERROR_OPERATION_FAILED);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception copying file");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_fs_move_file(const char* source, const char* destination) {
    if (!source || !destination) {
        SetError(LC_ERROR_NULL_POINTER, "source or destination is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto result = lupine::platform::FileSystem::MoveFile(source, destination);
        return result.success ? LC_SUCCESS : MapFileError(result.error, LC_ERROR_OPERATION_FAILED);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception moving file");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_fs_list_directory(const char* path, bool recursive, char** out_json) {
    if (!path || !out_json) {
        SetError(LC_ERROR_NULL_POINTER, "path or out_json is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto result = lupine::platform::FileSystem::ListDirectory(path, recursive);
        if (!result.success) {
            return MapFileError(result.error, LC_ERROR_OPERATION_FAILED);
        }
        return WriteStringArrayJson(result.data, out_json);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception listing directory");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_fs_get_current_directory(char** out_path) {
    if (!out_path) {
        SetError(LC_ERROR_NULL_POINTER, "out_path is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto result = lupine::platform::FileSystem::GetCurrentDirectory();
        if (!result.success) {
            return MapFileError(result.error, LC_ERROR_OPERATION_FAILED);
        }
        return WriteAllocatedString(result.data, out_path);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception getting current directory");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_fs_set_current_directory(const char* path) {
    if (!path) {
        SetError(LC_ERROR_NULL_POINTER, "path is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto result = lupine::platform::FileSystem::SetCurrentDirectory(path);
        return result.success ? LC_SUCCESS : MapFileError(result.error, LC_ERROR_OPERATION_FAILED);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception setting current directory");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_fs_get_absolute_path(const char* path, char** out_path) {
    if (!path || !out_path) {
        SetError(LC_ERROR_NULL_POINTER, "path or out_path is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto result = lupine::platform::FileSystem::GetAbsolutePath(path);
        if (!result.success) {
            return MapFileError(result.error, LC_ERROR_OPERATION_FAILED);
        }
        return WriteAllocatedString(result.data, out_path);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception resolving absolute path");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_fs_get_canonical_path(const char* path, char** out_path) {
    if (!path || !out_path) {
        SetError(LC_ERROR_NULL_POINTER, "path or out_path is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto result = lupine::platform::FileSystem::GetCanonicalPath(path);
        if (!result.success) {
            return MapFileError(result.error, LC_ERROR_OPERATION_FAILED);
        }
        return WriteAllocatedString(result.data, out_path);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception resolving canonical path");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * VirtualFileSystem - virtual path operations
 * ============================================================================ */

LC_API LCResult lc_vfs_is_initialized(bool* out_initialized) {
    if (!out_initialized) {
        SetError(LC_ERROR_NULL_POINTER, "out_initialized is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        *out_initialized = VFS().IsInitialized();
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception checking VFS init state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_vfs_mount(const char* mount_point, const char* physical_path, bool read_only) {
    if (!mount_point || !physical_path) {
        SetError(LC_ERROR_NULL_POINTER, "mount_point or physical_path is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        bool ok = VFS().Mount(mount_point, physical_path, read_only);
        return ok ? LC_SUCCESS : MapFileError("Mount failed", LC_ERROR_OPERATION_FAILED);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception mounting VFS path");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_vfs_unmount(const char* mount_point) {
    if (!mount_point) {
        SetError(LC_ERROR_NULL_POINTER, "mount_point is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        bool ok = VFS().Unmount(mount_point);
        return ok ? LC_SUCCESS : MapFileError("Unmount failed", LC_ERROR_OPERATION_FAILED);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception unmounting VFS path");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_vfs_is_mounted(const char* mount_point, bool* out_mounted) {
    if (!mount_point || !out_mounted) {
        SetError(LC_ERROR_NULL_POINTER, "mount_point or out_mounted is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        *out_mounted = VFS().IsMounted(mount_point);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception checking mount state");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_vfs_get_mount_path(const char* mount_point, char** out_path) {
    if (!mount_point || !out_path) {
        SetError(LC_ERROR_NULL_POINTER, "mount_point or out_path is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        std::string path = VFS().GetMountPath(mount_point);
        if (path.empty()) {
            SetError(LC_ERROR_NOT_FOUND, "Mount point not found");
            return LC_ERROR_NOT_FOUND;
        }
        return WriteAllocatedString(path, out_path);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception getting mount path");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_vfs_get_mount_points(char** out_json) {
    if (!out_json) {
        SetError(LC_ERROR_NULL_POINTER, "out_json is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        return WriteStringArrayJson(VFS().GetMountPoints(), out_json);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception listing mount points");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_vfs_resolve_path(const char* virtual_path, char** out_path) {
    if (!virtual_path || !out_path) {
        SetError(LC_ERROR_NULL_POINTER, "virtual_path or out_path is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto result = VFS().ResolvePath(virtual_path);
        if (!result.success) {
            return MapFileError(result.error, LC_ERROR_PATH_INVALID);
        }
        return WriteAllocatedString(result.data, out_path);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception resolving virtual path");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_vfs_resolve_asset(const char* path_or_uuid, char** out_path) {
    if (!path_or_uuid || !out_path) {
        SetError(LC_ERROR_NULL_POINTER, "path_or_uuid or out_path is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto result = VFS().ResolveAsset(path_or_uuid);
        if (!result.success) {
            return MapFileError(result.error, LC_ERROR_PATH_INVALID);
        }
        return WriteAllocatedString(result.data, out_path);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception resolving asset");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_vfs_read_text(const char* virtual_path, char** out_text) {
    if (!virtual_path || !out_text) {
        SetError(LC_ERROR_NULL_POINTER, "virtual_path or out_text is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto result = VFS().ReadFile(virtual_path);
        if (!result.success) {
            return MapFileError(result.error, LC_ERROR_FILE_READ_FAILED);
        }
        return WriteAllocatedString(result.data, out_text);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception reading virtual file");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_vfs_read_binary(const char* virtual_path, uint8_t** out_data, size_t* out_size) {
    if (!virtual_path || !out_data || !out_size) {
        SetError(LC_ERROR_NULL_POINTER, "virtual_path, out_data or out_size is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto result = VFS().ReadBinaryFile(virtual_path);
        if (!result.success) {
            return MapFileError(result.error, LC_ERROR_FILE_READ_FAILED);
        }
        return WriteAllocatedBytes(result.data, out_data, out_size);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception reading virtual binary file");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_vfs_write_text(const char* virtual_path, const char* contents) {
    if (!virtual_path || !contents) {
        SetError(LC_ERROR_NULL_POINTER, "virtual_path or contents is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto result = VFS().WriteFile(virtual_path, contents);
        return result.success ? LC_SUCCESS : MapFileError(result.error, LC_ERROR_FILE_WRITE_FAILED);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception writing virtual file");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_vfs_write_binary(const char* virtual_path, const uint8_t* data, size_t size) {
    if (!virtual_path || (!data && size != 0)) {
        SetError(LC_ERROR_NULL_POINTER, "virtual_path or data is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        std::vector<uint8_t> bytes(data, data + size);
        auto result = VFS().WriteBinaryFile(virtual_path, bytes);
        return result.success ? LC_SUCCESS : MapFileError(result.error, LC_ERROR_FILE_WRITE_FAILED);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception writing virtual binary file");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_vfs_exists(const char* virtual_path, bool* out_exists) {
    if (!virtual_path || !out_exists) {
        SetError(LC_ERROR_NULL_POINTER, "virtual_path or out_exists is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        *out_exists = VFS().Exists(virtual_path);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception checking virtual existence");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_vfs_is_file(const char* virtual_path, bool* out_is_file) {
    if (!virtual_path || !out_is_file) {
        SetError(LC_ERROR_NULL_POINTER, "virtual_path or out_is_file is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        *out_is_file = VFS().IsFile(virtual_path);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception checking virtual file type");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_vfs_is_directory(const char* virtual_path, bool* out_is_dir) {
    if (!virtual_path || !out_is_dir) {
        SetError(LC_ERROR_NULL_POINTER, "virtual_path or out_is_dir is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        *out_is_dir = VFS().IsDirectory(virtual_path);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception checking virtual directory type");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_vfs_get_file_size(const char* virtual_path, uint64_t* out_size) {
    if (!virtual_path || !out_size) {
        SetError(LC_ERROR_NULL_POINTER, "virtual_path or out_size is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto result = VFS().GetFileSize(virtual_path);
        if (!result.success) {
            return MapFileError(result.error, LC_ERROR_FILE_NOT_FOUND);
        }
        *out_size = result.data;
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception getting virtual file size");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_vfs_get_modification_time(const char* virtual_path, int64_t* out_time) {
    if (!virtual_path || !out_time) {
        SetError(LC_ERROR_NULL_POINTER, "virtual_path or out_time is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto result = VFS().GetModificationTime(virtual_path);
        if (!result.success) {
            return MapFileError(result.error, LC_ERROR_FILE_NOT_FOUND);
        }
        *out_time = result.data;
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception getting virtual modification time");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_vfs_create_directory(const char* virtual_path, bool recursive) {
    if (!virtual_path) {
        SetError(LC_ERROR_NULL_POINTER, "virtual_path is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto result = VFS().CreateDirectory(virtual_path, recursive);
        return result.success ? LC_SUCCESS : MapFileError(result.error, LC_ERROR_OPERATION_FAILED);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception creating virtual directory");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_vfs_delete_file(const char* virtual_path) {
    if (!virtual_path) {
        SetError(LC_ERROR_NULL_POINTER, "virtual_path is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto result = VFS().DeleteFile(virtual_path);
        return result.success ? LC_SUCCESS : MapFileError(result.error, LC_ERROR_OPERATION_FAILED);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception deleting virtual file");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_vfs_list_directory(const char* virtual_path, bool recursive, char** out_json) {
    if (!virtual_path || !out_json) {
        SetError(LC_ERROR_NULL_POINTER, "virtual_path or out_json is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto result = VFS().ListDirectory(virtual_path, recursive);
        if (!result.success) {
            return MapFileError(result.error, LC_ERROR_OPERATION_FAILED);
        }
        return WriteStringArrayJson(result.data, out_json);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception listing virtual directory");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_vfs_copy_file(const char* source_virtual_path, const char* dest_virtual_path, bool overwrite) {
    if (!source_virtual_path || !dest_virtual_path) {
        SetError(LC_ERROR_NULL_POINTER, "source or destination virtual path is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto result = VFS().CopyFile(source_virtual_path, dest_virtual_path, overwrite);
        return result.success ? LC_SUCCESS : MapFileError(result.error, LC_ERROR_OPERATION_FAILED);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception copying virtual file");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_vfs_get_default_user_data_path(const char* app_name, char** out_path) {
    if (!out_path) {
        SetError(LC_ERROR_NULL_POINTER, "out_path is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        std::string path = app_name
            ? lupine::platform::VirtualFileSystem::GetDefaultUserDataPath(app_name)
            : lupine::platform::VirtualFileSystem::GetDefaultUserDataPath();
        return WriteAllocatedString(path, out_path);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception getting default user data path");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_vfs_is_sandboxed_path(const char* path, bool* out_ok) {
    if (!path || !out_ok) {
        SetError(LC_ERROR_NULL_POINTER, "path or out_ok is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        *out_ok = IsSandboxedPath(path);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception checking sandboxed path");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_vfs_append_text(const char* virtual_path, const char* contents) {
    if (!virtual_path || !contents) {
        SetError(LC_ERROR_NULL_POINTER, "virtual_path or contents is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        std::string existing;
        auto read = VFS().ReadFile(virtual_path);
        if (read.success) {
            existing = read.data;
        }
        existing += contents;
        auto result = VFS().WriteFile(virtual_path, existing);
        return result.success ? LC_SUCCESS : MapFileError(result.error, LC_ERROR_FILE_WRITE_FAILED);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception appending virtual file");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_vfs_get_default_temp_path(char** out_path) {
    if (!out_path) {
        SetError(LC_ERROR_NULL_POINTER, "out_path is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        return WriteAllocatedString(lupine::platform::VirtualFileSystem::GetDefaultTempPath(), out_path);
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception getting default temp path");
        return LC_ERROR_INTERNAL_ERROR;
    }
}
