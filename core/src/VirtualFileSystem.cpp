#include "lupine/platform/VirtualFileSystem.hpp"
#include "lupine/platform/PackFile.hpp"
#include "lupine/platform/UserDataStore.hpp"
#include "lupine/asset/AssetDatabase.hpp"
#include "lupine/logger/Logger.hpp"
#include <algorithm>
#include <cctype>

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

            // Untrusted scene/config/script values reach this resolver, so the relative
            // segment must not be able to climb out of the mount with "..".
            std::string physicalPath = Path::JoinSandboxed(info.physicalPath, relativePath);
            if (physicalPath.empty()) {
                LOG_ERROR(LogCategory::Core,
                          "VirtualFileSystem: rejected path '{}' - it escapes mount point '{}'",
                          virtualPath, mountPoint);
                return "";
            }

            return physicalPath;
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
    if (physicalPath.empty()) {
        return FileResult<std::string>(false, "Path escapes its mount point: " + virtualPath);
    }
    return FileResult<std::string>(true, "", physicalPath);
}

FileResult<std::string> VirtualFileSystem::ResolveAsset(const std::string& pathOrUUID) const {
    if (pathOrUUID.empty()) {
        return FileResult<std::string>(false, "Empty path or UUID");
    }

    // Check if it looks like a UUID (16 hex characters)
    if (pathOrUUID.size() == 16) {
        bool allHex = true;
        for (char c : pathOrUUID) {
            if (!std::isxdigit(static_cast<unsigned char>(c))) {
                allHex = false;
                break;
            }
        }

        if (allHex) {
            // Try UUID resolution via AssetDatabase
            auto& assetDb = asset::AssetDatabase::GetInstance();
            if (assetDb.IsInitialized()) {
                core::UUID uuid = core::UUID::FromString(pathOrUUID);
                if (uuid.IsValid()) {
                    std::string path = assetDb.GetPathForUUID(uuid);
                    if (!path.empty()) {
                        return ResolvePath(path);
                    }
                }
            }
        }
    }

    // Fall back to standard path resolution
    return ResolvePath(pathOrUUID);
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
    // Packed content is served from the .pck; a mount point only backs what the pack
    // does not hold (notably the writable user:// space).
    PackFileSystem& packFS = PackFileSystem::Instance();
    if (packFS.isPackMode() && packFS.exists(virtualPath)) {
        return FileResult<std::string>(true, "", packFS.readFileAsString(virtualPath));
    }

    std::string physicalPath;
    {
        LockGuard lock(m_Mutex);
        physicalPath = ResolvePathInternal(virtualPath);
    }

    return FileSystem::ReadFile(physicalPath);
}

FileResult<std::vector<uint8_t>> VirtualFileSystem::ReadBinaryFile(const std::string& virtualPath) {
    PackFileSystem& packFS = PackFileSystem::Instance();
    if (packFS.isPackMode() && packFS.exists(virtualPath)) {
        return FileResult<std::vector<uint8_t>>(true, "", packFS.readFile(virtualPath));
    }

    std::string physicalPath;
    {
        LockGuard lock(m_Mutex);
        physicalPath = ResolvePathInternal(virtualPath);
    }

    return FileSystem::ReadBinaryFile(physicalPath);
}

namespace {

/**
 * Flag a successful mutation of the user:// space as needing a write-back to durable
 * storage. Only user:// is tracked: res://, app:// and temp:// are either read-only or
 * intentionally throwaway, and on web only user:// is backed by IDBFS.
 */
void MarkUserDataDirtyIf(bool succeeded, const std::string& virtualPath) {
    if (!succeeded) {
        return;
    }
    if (virtualPath.rfind("user://", 0) != 0) {
        return;
    }
    UserDataStore::GetInstance().MarkDirty();
}

} // namespace

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

    FileResult<> result = FileSystem::WriteFile(physicalPath, contents);
    MarkUserDataDirtyIf(result.success, virtualPath);
    return result;
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

    FileResult<> result = FileSystem::WriteBinaryFile(physicalPath, data);
    MarkUserDataDirtyIf(result.success, virtualPath);
    return result;
}

bool VirtualFileSystem::Exists(const std::string& virtualPath) {
    // In pack mode the res:// content lives inside the .pck, which FileSystem::Exists
    // (a bare stat against the real disk) cannot see. Without this an exported game
    // reports every packed asset as missing.
    PackFileSystem& packFS = PackFileSystem::Instance();
    if (packFS.isPackMode() && packFS.exists(virtualPath)) {
        return true;
    }

    std::string physicalPath;
    {
        LockGuard lock(m_Mutex);
        physicalPath = ResolvePathInternal(virtualPath);
    }

    return FileSystem::Exists(physicalPath);
}

bool VirtualFileSystem::IsFile(const std::string& virtualPath) {
    // Pack entries are always files.
    PackFileSystem& packFS = PackFileSystem::Instance();
    if (packFS.isPackMode() && packFS.exists(virtualPath)) {
        return true;
    }

    std::string physicalPath;
    {
        LockGuard lock(m_Mutex);
        physicalPath = ResolvePathInternal(virtualPath);
    }

    return FileSystem::IsFile(physicalPath);
}

bool VirtualFileSystem::IsDirectory(const std::string& virtualPath) {
    // A pack stores no directory entries, so a path is a directory exactly when the
    // pack holds at least one file beneath it.
    PackFileSystem& packFS = PackFileSystem::Instance();
    if (packFS.isPackMode() && !packFS.listFilesRecursive(virtualPath).empty()) {
        return true;
    }

    std::string physicalPath;
    {
        LockGuard lock(m_Mutex);
        physicalPath = ResolvePathInternal(virtualPath);
    }

    return FileSystem::IsDirectory(physicalPath);
}

FileResult<uint64_t> VirtualFileSystem::GetFileSize(const std::string& virtualPath) {
    // Without this a packed asset reports a size of -1 while Exists() on the same path
    // reports true, so any "how big is it, then read it" caller fails in an exported game.
    PackFileSystem& packFS = PackFileSystem::Instance();
    if (packFS.isPackMode()) {
        uint64_t packSize = 0;
        if (packFS.getFileSize(virtualPath, packSize)) {
            return FileResult<uint64_t>(true, "", packSize);
        }
    }

    std::string physicalPath;
    {
        LockGuard lock(m_Mutex);
        physicalPath = ResolvePathInternal(virtualPath);
    }

    return FileSystem::GetFileSize(physicalPath);
}

FileResult<int64_t> VirtualFileSystem::GetModificationTime(const std::string& virtualPath) {
    // A pack stores no timestamps, and its contents cannot change while the game runs, so a
    // packed asset is reported with a fixed epoch time rather than a failure. That keeps the
    // "did this change since I last saw it?" comparisons that consume this answering "no"
    // (which is the truth for immutable content) instead of erroring on every probe.
    PackFileSystem& packFS = PackFileSystem::Instance();
    if (packFS.isPackMode() && packFS.exists(virtualPath)) {
        return FileResult<int64_t>(true, "", 0);
    }

    std::string physicalPath;
    {
        LockGuard lock(m_Mutex);
        physicalPath = ResolvePathInternal(virtualPath);
    }

    return FileSystem::GetModificationTime(physicalPath);
}

FileResult<FileMetadata> VirtualFileSystem::GetMetadata(const std::string& virtualPath) {
    // Assembled from the pack index rather than a stat, and kept consistent with what
    // Exists/IsFile/IsDirectory/GetFileSize report for the same path in pack mode.
    PackFileSystem& packFS = PackFileSystem::Instance();
    if (packFS.isPackMode()) {
        uint64_t packSize = 0;
        const bool isPackedFile = packFS.getFileSize(virtualPath, packSize);

        // A pack holds no directory entries, so a path is a directory exactly when the pack
        // holds at least one file beneath it - the same reconstruction IsDirectory uses.
        const bool isPackedDir = !isPackedFile && !packFS.listFilesRecursive(virtualPath).empty();

        if (isPackedFile || isPackedDir) {
            FileMetadata metadata;
            metadata.path = virtualPath;
            metadata.size = isPackedFile ? packSize : 0;
            metadata.modifiedTime = 0;
            metadata.isDirectory = isPackedDir;
            metadata.isFile = isPackedFile;
            metadata.exists = true;
            return FileResult<FileMetadata>(true, "", metadata);
        }
    }

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

    FileResult<> result = FileSystem::CreateDirectory(physicalPath, recursive);
    MarkUserDataDirtyIf(result.success, virtualPath);
    return result;
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

    FileResult<> result = FileSystem::DeleteFile(physicalPath);
    MarkUserDataDirtyIf(result.success, virtualPath);
    return result;
}

FileResult<std::vector<std::string>> VirtualFileSystem::ListDirectory(const std::string& virtualPath, bool recursive) {
    // In pack mode the res:// tree lives inside the .pck and never touches the real disk,
    // so the FileSystem::ListDirectory below reports the directory as missing and hands
    // back a failure - which every caller reads as "the directory is empty". A pack stores
    // no directory entries, but its index is a full path map, so a listing is recoverable
    // by prefix; this is the same reconstruction IsDirectory already relies on. Pack keys
    // are stored res://-stripped and relative, so they are re-prefixed back into virtual
    // paths to match the shape the real-disk branch returns.
    std::vector<std::string> virtualPaths;
    PackFileSystem& packFS = PackFileSystem::Instance();

    if (packFS.isPackMode()) {
        const std::vector<std::string> packEntries = recursive
            ? packFS.listFilesRecursive(virtualPath)
            : packFS.listDirectory(virtualPath);

        virtualPaths.reserve(packEntries.size());
        for (const std::string& entry : packEntries) {
            virtualPaths.push_back("res://" + entry);
        }
    }

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

    const FileResult<std::vector<std::string>> listResult = FileSystem::ListDirectory(physicalPath, recursive);
    if (!listResult.success) {
        // A pack listing alone is a valid answer: in an exported game the res:// mount has
        // no on-disk counterpart at all. Only report the failure when the pack had nothing
        // either, so a genuinely missing user:// directory still surfaces its error.
        if (!virtualPaths.empty()) {
            return FileResult<std::vector<std::string>>(true, "", virtualPaths);
        }
        return listResult;
    }

    // Both sources can be live at once (a loose file shadowing or extending a packed one),
    // so the disk pass unions into the pack pass rather than replacing it.
    for (const std::string& physPath : listResult.data) {
        std::string virtualPathResult = PhysicalToVirtual(physPath, mountPoint, mountPath);
        if (std::find(virtualPaths.begin(), virtualPaths.end(), virtualPathResult) == virtualPaths.end()) {
            virtualPaths.push_back(virtualPathResult);
        }
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

    FileResult<> result = FileSystem::CopyFile(sourcePath, destPath, overwrite);
    MarkUserDataDirtyIf(result.success, destVirtualPath);
    return result;
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
#elif defined(__EMSCRIPTEN__) || defined(LUPINE_PLATFORM_WEB)
    // Fixed mount point, deliberately independent of appName: this exact path is where
    // runtime/web/shell.html mounts IDBFS before main() runs, and the two must agree or
    // user:// writes land in ephemeral MEMFS and are lost on reload. IndexedDB is
    // already origin-scoped, so there is nothing for appName to disambiguate here.
    (void)appName;
    return UserDataStore::kWebUserDataPath;
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
