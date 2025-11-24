#include "lupine/platform/VirtualFileSystem.hpp"
#include "lupine/logger/Logger.hpp"
#include <algorithm>

#if defined(LUPINE_PLATFORM_WINDOWS)
    #include <windows.h>
    #include <shlobj.h>

    #ifdef CreateDirectory
        #undef CreateDirectory
    #endif
    #ifdef DeleteFile
        #undef DeleteFile
    #endif
    #ifdef CopyFile
        #undef CopyFile
    #endif
    #ifdef MoveFile
        #undef MoveFile
    #endif
    #ifdef GetCurrentDirectory
        #undef GetCurrentDirectory
    #endif
#else
    #include <unistd.h>
    #include <pwd.h>
#endif

namespace lupine {
namespace platform {

VirtualFileSystem& VirtualFileSystem::GetInstance() {
    static VirtualFileSystem instance;
    return instance;
}

bool VirtualFileSystem::Initialize(const std::string& appPath, const std::string& resPath, const std::string& userPath) {
    LockGuard lock(m_Mutex);

    if (m_Initialized) {

        return true;
    }

    std::string appDir = appPath;
    if (appDir.empty()) {
        auto cwdResult = FileSystem::GetCurrentDirectory();
        if (!cwdResult.success) {

            return false;
        }
        appDir = cwdResult.data;
    }

    if (!MountInternal("app://", appDir, true)) {

        return false;
    }

    std::string resourceDir = resPath;
    if (resourceDir.empty()) {
        resourceDir = Path::Join(appDir, "resources");
    }

    if (!FileSystem::Exists(resourceDir)) {

        auto createResult = FileSystem::CreateDirectory(resourceDir, true);
        if (!createResult.success) {

            return false;
        }
    }

    if (!MountInternal("res://", resourceDir, false)) {

        return false;
    }

    std::string userDir = userPath;
    if (userDir.empty()) {
        userDir = GetDefaultUserDataPath();
    }

    if (!FileSystem::Exists(userDir)) {

        auto createResult = FileSystem::CreateDirectory(userDir, true);
        if (!createResult.success) {

            return false;
        }
    }

    if (!MountInternal("user://", userDir, false)) {

        return false;
    }

    std::string tempDir = GetDefaultTempPath();
    if (!MountInternal("temp://", tempDir, false)) {

        return false;
    }

    m_Initialized = true;

    return true;
}

void VirtualFileSystem::Shutdown() {
    LockGuard lock(m_Mutex);

    m_MountPoints.clear();
    m_Initialized = false;
}

bool VirtualFileSystem::IsInitialized() const {
    LockGuard lock(m_Mutex);
    return m_Initialized;
}

bool VirtualFileSystem::MountInternal(const std::string& mountPoint, const std::string& physicalPath, bool readOnly) {

    if (mountPoint.size() < 3 || mountPoint.substr(mountPoint.size() - 3) != "://") {

        return false;
    }

    if (m_MountPoints.find(mountPoint) != m_MountPoints.end()) {

    }

    std::string normalizedPath = Path::Normalize(physicalPath);

    if (!FileSystem::Exists(normalizedPath)) {

        return false;
    }

    if (!FileSystem::IsDirectory(normalizedPath)) {

        return false;
    }

    MountInfo info;
    info.physicalPath = normalizedPath;
    info.readOnly = readOnly;
    m_MountPoints[mountPoint] = info;

    return true;
}

bool VirtualFileSystem::Mount(const std::string& mountPoint, const std::string& physicalPath, bool readOnly) {
    LockGuard lock(m_Mutex);
    return MountInternal(mountPoint, physicalPath, readOnly);
}

bool VirtualFileSystem::Unmount(const std::string& mountPoint) {
    LockGuard lock(m_Mutex);

    auto it = m_MountPoints.find(mountPoint);
    if (it == m_MountPoints.end()) {

        return false;
    }

    m_MountPoints.erase(it);
    return true;
}

bool VirtualFileSystem::IsMounted(const std::string& mountPoint) const {
    LockGuard lock(m_Mutex);
    return m_MountPoints.find(mountPoint) != m_MountPoints.end();
}

std::string VirtualFileSystem::GetMountPath(const std::string& mountPoint) const {
    LockGuard lock(m_Mutex);

    auto it = m_MountPoints.find(mountPoint);
    if (it == m_MountPoints.end()) {
        return "";
    }

    return it->second.physicalPath;
}

std::string VirtualFileSystem::ResolvePathInternal(const std::string& virtualPath) const {

    for (const auto& pair : m_MountPoints) {
        const std::string& mountPoint = pair.first;
        const MountInfo& info = pair.second;

        if (virtualPath.size() >= mountPoint.size() &&
            virtualPath.substr(0, mountPoint.size()) == mountPoint) {

            std::string relativePath = virtualPath.substr(mountPoint.size());

            return Path::Join(info.physicalPath, relativePath);
        }
    }

    return virtualPath;
}

bool VirtualFileSystem::IsReadOnlyViolationInternal(const std::string& virtualPath) const {

    for (const auto& pair : m_MountPoints) {
        const std::string& mountPoint = pair.first;
        const MountInfo& info = pair.second;

        if (virtualPath.size() >= mountPoint.size() &&
            virtualPath.substr(0, mountPoint.size()) == mountPoint) {
            return info.readOnly;
        }
    }

    return false;
}

FileResult<std::string> VirtualFileSystem::ResolvePath(const std::string& virtualPath) const {
    LockGuard lock(m_Mutex);
    std::string physicalPath = ResolvePathInternal(virtualPath);
    return FileResult<std::string>(true, "", physicalPath);
}

std::vector<std::string> VirtualFileSystem::GetMountPoints() const {
    LockGuard lock(m_Mutex);

    std::vector<std::string> mountPoints;
    mountPoints.reserve(m_MountPoints.size());

    for (const auto& pair : m_MountPoints) {
        mountPoints.push_back(pair.first);
    }

    return mountPoints;
}

FileResult<std::string> VirtualFileSystem::ReadFile(const std::string& virtualPath) {
    std::string physicalPath;
    {
        LockGuard lock(m_Mutex);
        physicalPath = ResolvePathInternal(virtualPath);
    }

    return FileSystem::ReadFile(physicalPath);
}

FileResult<std::vector<uint8_t>> VirtualFileSystem::ReadBinaryFile(const std::string& virtualPath) {
    std::string physicalPath;
    {
        LockGuard lock(m_Mutex);
        physicalPath = ResolvePathInternal(virtualPath);
    }

    return FileSystem::ReadBinaryFile(physicalPath);
}

FileResult<> VirtualFileSystem::WriteFile(const std::string& virtualPath, const std::string& contents) {
    std::string physicalPath;
    {
        LockGuard lock(m_Mutex);

        if (IsReadOnlyViolationInternal(virtualPath)) {
            std::string error = "Cannot write to read-only mount point for path: " + virtualPath;

            return FileResult<>(false, error);
        }

        physicalPath = ResolvePathInternal(virtualPath);
    }

    return FileSystem::WriteFile(physicalPath, contents);
}

FileResult<> VirtualFileSystem::WriteBinaryFile(const std::string& virtualPath, const std::vector<uint8_t>& data) {
    std::string physicalPath;
    {
        LockGuard lock(m_Mutex);

        if (IsReadOnlyViolationInternal(virtualPath)) {
            std::string error = "Cannot write to read-only mount point for path: " + virtualPath;

            return FileResult<>(false, error);
        }

        physicalPath = ResolvePathInternal(virtualPath);
    }

    return FileSystem::WriteBinaryFile(physicalPath, data);
}

bool VirtualFileSystem::Exists(const std::string& virtualPath) {
    std::string physicalPath;
    {
        LockGuard lock(m_Mutex);
        physicalPath = ResolvePathInternal(virtualPath);
    }

    return FileSystem::Exists(physicalPath);
}

bool VirtualFileSystem::IsFile(const std::string& virtualPath) {
    std::string physicalPath;
    {
        LockGuard lock(m_Mutex);
        physicalPath = ResolvePathInternal(virtualPath);
    }

    return FileSystem::IsFile(physicalPath);
}

bool VirtualFileSystem::IsDirectory(const std::string& virtualPath) {
    std::string physicalPath;
    {
        LockGuard lock(m_Mutex);
        physicalPath = ResolvePathInternal(virtualPath);
    }

    return FileSystem::IsDirectory(physicalPath);
}

FileResult<uint64_t> VirtualFileSystem::GetFileSize(const std::string& virtualPath) {
    std::string physicalPath;
    {
        LockGuard lock(m_Mutex);
        physicalPath = ResolvePathInternal(virtualPath);
    }

    return FileSystem::GetFileSize(physicalPath);
}

FileResult<int64_t> VirtualFileSystem::GetModificationTime(const std::string& virtualPath) {
    std::string physicalPath;
    {
        LockGuard lock(m_Mutex);
        physicalPath = ResolvePathInternal(virtualPath);
    }

    return FileSystem::GetModificationTime(physicalPath);
}

FileResult<FileMetadata> VirtualFileSystem::GetMetadata(const std::string& virtualPath) {
    std::string physicalPath;
    {
        LockGuard lock(m_Mutex);
        physicalPath = ResolvePathInternal(virtualPath);
    }

    return FileSystem::GetMetadata(physicalPath);
}

FileResult<> VirtualFileSystem::CreateDirectory(const std::string& virtualPath, bool recursive) {
    std::string physicalPath;
    {
        LockGuard lock(m_Mutex);

        if (IsReadOnlyViolationInternal(virtualPath)) {
            std::string error = "Cannot create directory in read-only mount point for path: " + virtualPath;

            return FileResult<>(false, error);
        }

        physicalPath = ResolvePathInternal(virtualPath);
    }

    return FileSystem::CreateDirectory(physicalPath, recursive);
}

FileResult<> VirtualFileSystem::DeleteFile(const std::string& virtualPath) {
    std::string physicalPath;
    {
        LockGuard lock(m_Mutex);

        if (IsReadOnlyViolationInternal(virtualPath)) {
            std::string error = "Cannot delete from read-only mount point for path: " + virtualPath;

            return FileResult<>(false, error);
        }

        physicalPath = ResolvePathInternal(virtualPath);
    }

    return FileSystem::DeleteFile(physicalPath);
}

FileResult<std::vector<std::string>> VirtualFileSystem::ListDirectory(const std::string& virtualPath, bool recursive) {
    std::string physicalPath;
    std::string mountPoint;
    std::string mountPath;

    {
        LockGuard lock(m_Mutex);
        physicalPath = ResolvePathInternal(virtualPath);

        for (const auto& pair : m_MountPoints) {
            const std::string& mp = pair.first;
            const MountInfo& info = pair.second;

            if (virtualPath.size() >= mp.size() &&
                virtualPath.substr(0, mp.size()) == mp) {
                mountPoint = mp;
                mountPath = info.physicalPath;
                break;
            }
        }
    }

    auto listResult = FileSystem::ListDirectory(physicalPath, recursive);
    if (!listResult.success) {
        return listResult;
    }

    std::vector<std::string> virtualPaths;
    virtualPaths.reserve(listResult.data.size());

    for (const auto& physPath : listResult.data) {
        std::string virtualPathResult = PhysicalToVirtual(physPath, mountPoint, mountPath);
        virtualPaths.push_back(virtualPathResult);
    }

    return FileResult<std::vector<std::string>>(true, "", virtualPaths);
}

FileResult<> VirtualFileSystem::CopyFile(const std::string& sourceVirtualPath, const std::string& destVirtualPath, bool overwrite) {
    std::string sourcePath;
    std::string destPath;

    {
        LockGuard lock(m_Mutex);
        sourcePath = ResolvePathInternal(sourceVirtualPath);

        if (IsReadOnlyViolationInternal(destVirtualPath)) {
            std::string error = "Cannot write to read-only mount point for path: " + destVirtualPath;

            return FileResult<>(false, error);
        }

        destPath = ResolvePathInternal(destVirtualPath);
    }

    return FileSystem::CopyFile(sourcePath, destPath, overwrite);
}

std::string VirtualFileSystem::GetDefaultUserDataPath(const std::string& appName) {
#if defined(LUPINE_PLATFORM_WINDOWS)
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        return Path::Join(std::string(path), appName);
    }
    return Path::Join(".", appName);
#elif defined(LUPINE_PLATFORM_MACOS)
    const char* home = getenv("HOME");
    if (home) {
        return Path::Join(Path::Join(Path::Join(home, "Library"), "Application Support"), appName);
    }
    return Path::Join(".", appName);
#else
    const char* xdgData = getenv("XDG_DATA_HOME");
    if (xdgData) {
        return Path::Join(std::string(xdgData), appName);
    }

    const char* home = getenv("HOME");
    if (home) {
        return Path::Join(Path::Join(Path::Join(home, ".local"), "share"), appName);
    }

    return Path::Join(".", appName);
#endif
}

std::string VirtualFileSystem::GetDefaultTempPath() {
#if defined(LUPINE_PLATFORM_WINDOWS)
    char path[MAX_PATH];
    DWORD length = GetTempPathA(MAX_PATH, path);
    if (length > 0 && length < MAX_PATH) {
        return std::string(path);
    }
    return ".\\temp";
#else
    const char* tmpdir = getenv("TMPDIR");
    if (tmpdir) {
        return std::string(tmpdir);
    }
    return "/tmp";
#endif
}

std::string VirtualFileSystem::PhysicalToVirtual(const std::string& physicalPath, const std::string& mountPoint, const std::string& mountPath) const {

    std::string normalizedPhysical = Path::ToUnixSeparators(physicalPath);
    std::string normalizedMount = Path::ToUnixSeparators(mountPath);

    if (normalizedPhysical.size() >= normalizedMount.size() &&
        normalizedPhysical.substr(0, normalizedMount.size()) == normalizedMount) {

        std::string relativePath = normalizedPhysical.substr(normalizedMount.size());

        if (!relativePath.empty() && Path::IsSeparator(relativePath[0])) {
            relativePath = relativePath.substr(1);
        }

        return mountPoint + relativePath;
    }

    return physicalPath;
}

}
}
