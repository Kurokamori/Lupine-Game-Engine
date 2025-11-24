#include "lupine/platform/FileSystem.hpp"
#include "lupine/logger/Logger.hpp"
#include <fstream>
#include <sstream>
#include <cstring>

#if defined(LUPINE_PLATFORM_WINDOWS)
    #include <windows.h>
    #include <direct.h>
    #include <sys/stat.h>

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
    #ifdef SetCurrentDirectory
        #undef SetCurrentDirectory
    #endif
    #define getcwd _getcwd
    #define chdir _chdir
    #define stat_struct struct _stat
    #define stat_func _stat

    #define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
    #define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)

    #ifndef PATH_MAX
        #define PATH_MAX MAX_PATH
    #endif
#else
    #include <unistd.h>
    #include <dirent.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <limits.h>
    #define stat_struct struct stat
    #define stat_func stat
#endif

namespace lupine {
namespace platform {

namespace {

    std::string GetErrorMessage() {
        return std::strerror(errno);
    }

    bool GetFileStat(const std::string& path, stat_struct& statbuf) {
        return stat_func(path.c_str(), &statbuf) == 0;
    }
}

FileResult<std::string> FileSystem::ReadFile(const std::string& path) {
    std::ifstream file(path, std::ios::in);
    if (!file.is_open()) {
        std::string error = "Failed to open file: " + path + " - " + GetErrorMessage();

        return FileResult<std::string>(false, error);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    return FileResult<std::string>(true, "", buffer.str());
}

FileResult<std::vector<uint8_t>> FileSystem::ReadBinaryFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::string error = "Failed to open file: " + path + " - " + GetErrorMessage();

        return FileResult<std::vector<uint8_t>>(false, error);
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        std::string error = "Failed to read file: " + path + " - " + GetErrorMessage();

        return FileResult<std::vector<uint8_t>>(false, error);
    }

    file.close();
    return FileResult<std::vector<uint8_t>>(true, "", buffer);
}

FileResult<> FileSystem::WriteFile(const std::string& path, const std::string& contents) {
    std::ofstream file(path, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        std::string error = "Failed to open file for writing: " + path + " - " + GetErrorMessage();

        return FileResult<>(false, error);
    }

    file << contents;
    file.close();

    if (file.fail()) {
        std::string error = "Failed to write to file: " + path + " - " + GetErrorMessage();

        return FileResult<>(false, error);
    }

    return FileResult<>(true);
}

FileResult<> FileSystem::WriteBinaryFile(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        std::string error = "Failed to open file for writing: " + path + " - " + GetErrorMessage();

        return FileResult<>(false, error);
    }

    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    file.close();

    if (file.fail()) {
        std::string error = "Failed to write to file: " + path + " - " + GetErrorMessage();

        return FileResult<>(false, error);
    }

    return FileResult<>(true);
}

FileResult<> FileSystem::AppendFile(const std::string& path, const std::string& contents) {
    std::ofstream file(path, std::ios::out | std::ios::app);
    if (!file.is_open()) {
        std::string error = "Failed to open file for appending: " + path + " - " + GetErrorMessage();

        return FileResult<>(false, error);
    }

    file << contents;
    file.close();

    if (file.fail()) {
        std::string error = "Failed to append to file: " + path + " - " + GetErrorMessage();

        return FileResult<>(false, error);
    }

    return FileResult<>(true);
}

bool FileSystem::Exists(const std::string& path) {
    stat_struct statbuf;
    return GetFileStat(path, statbuf);
}

bool FileSystem::IsFile(const std::string& path) {
    stat_struct statbuf;
    if (!GetFileStat(path, statbuf)) {
        return false;
    }
    return S_ISREG(statbuf.st_mode);
}

bool FileSystem::IsDirectory(const std::string& path) {
    stat_struct statbuf;
    if (!GetFileStat(path, statbuf)) {
        return false;
    }
    return S_ISDIR(statbuf.st_mode);
}

FileResult<uint64_t> FileSystem::GetFileSize(const std::string& path) {
    stat_struct statbuf;
    if (!GetFileStat(path, statbuf)) {
        std::string error = "Failed to get file size: " + path + " - " + GetErrorMessage();

        return FileResult<uint64_t>(false, error);
    }

    return FileResult<uint64_t>(true, "", static_cast<uint64_t>(statbuf.st_size));
}

FileResult<int64_t> FileSystem::GetModificationTime(const std::string& path) {
    stat_struct statbuf;
    if (!GetFileStat(path, statbuf)) {
        std::string error = "Failed to get modification time: " + path + " - " + GetErrorMessage();

        return FileResult<int64_t>(false, error);
    }

    return FileResult<int64_t>(true, "", static_cast<int64_t>(statbuf.st_mtime));
}

FileResult<FileMetadata> FileSystem::GetMetadata(const std::string& path) {
    stat_struct statbuf;
    if (!GetFileStat(path, statbuf)) {
        std::string error = "Failed to get metadata: " + path + " - " + GetErrorMessage();

        return FileResult<FileMetadata>(false, error);
    }

    FileMetadata metadata;
    metadata.path = path;
    metadata.size = static_cast<uint64_t>(statbuf.st_size);
    metadata.modifiedTime = static_cast<int64_t>(statbuf.st_mtime);
    metadata.isDirectory = S_ISDIR(statbuf.st_mode);
    metadata.isFile = S_ISREG(statbuf.st_mode);
    metadata.exists = true;

    return FileResult<FileMetadata>(true, "", metadata);
}

FileResult<> FileSystem::CreateDirectory(const std::string& path, bool recursive) {
    if (Exists(path)) {
        if (IsDirectory(path)) {
            return FileResult<>(true);
        } else {
            std::string error = "Path exists but is not a directory: " + path;

            return FileResult<>(false, error);
        }
    }

    if (recursive) {

        std::string parent = Path::GetDirectory(path);
        if (!parent.empty() && parent != "." && !Exists(parent)) {
            auto result = CreateDirectory(parent, true);
            if (!result.success) {
                return result;
            }
        }
    }

#if defined(LUPINE_PLATFORM_WINDOWS)
    if (_mkdir(path.c_str()) != 0) {
        std::string error = "Failed to create directory: " + path + " - " + GetErrorMessage();

        return FileResult<>(false, error);
    }
#else
    if (mkdir(path.c_str(), 0755) != 0) {
        std::string error = "Failed to create directory: " + path + " - " + GetErrorMessage();

        return FileResult<>(false, error);
    }
#endif

    return FileResult<>(true);
}

FileResult<> FileSystem::DeleteFile(const std::string& path) {
    if (!Exists(path)) {
        std::string error = "File does not exist: " + path;

        return FileResult<>(false, error);
    }

    if (std::remove(path.c_str()) != 0) {
        std::string error = "Failed to delete file: " + path + " - " + GetErrorMessage();

        return FileResult<>(false, error);
    }

    return FileResult<>(true);
}

FileResult<> FileSystem::DeleteDirectory(const std::string& path, bool recursive) {
    if (!Exists(path)) {
        std::string error = "Directory does not exist: " + path;

        return FileResult<>(false, error);
    }

    if (!IsDirectory(path)) {
        std::string error = "Path is not a directory: " + path;

        return FileResult<>(false, error);
    }

    if (recursive) {

        auto listResult = ListDirectory(path, false);
        if (!listResult.success) {
            return FileResult<>(false, listResult.error);
        }

        for (const auto& item : listResult.data) {
            if (IsDirectory(item)) {
                auto result = DeleteDirectory(item, true);
                if (!result.success) {
                    return result;
                }
            } else {
                auto result = DeleteFile(item);
                if (!result.success) {
                    return result;
                }
            }
        }
    }

#if defined(LUPINE_PLATFORM_WINDOWS)
    if (_rmdir(path.c_str()) != 0) {
#else
    if (rmdir(path.c_str()) != 0) {
#endif
        std::string error = "Failed to delete directory: " + path + " - " + GetErrorMessage();

        return FileResult<>(false, error);
    }

    return FileResult<>(true);
}

FileResult<> FileSystem::CopyFile(const std::string& source, const std::string& destination, bool overwrite) {
    if (!Exists(source)) {
        std::string error = "Source file does not exist: " + source;

        return FileResult<>(false, error);
    }

    if (Exists(destination) && !overwrite) {
        std::string error = "Destination file already exists: " + destination;

        return FileResult<>(false, error);
    }

    auto readResult = ReadBinaryFile(source);
    if (!readResult.success) {
        return FileResult<>(false, readResult.error);
    }

    auto writeResult = WriteBinaryFile(destination, readResult.data);
    if (!writeResult.success) {
        return FileResult<>(false, writeResult.error);
    }

    return FileResult<>(true);
}

FileResult<> FileSystem::MoveFile(const std::string& source, const std::string& destination) {
    if (!Exists(source)) {
        std::string error = "Source file does not exist: " + source;

        return FileResult<>(false, error);
    }

    if (std::rename(source.c_str(), destination.c_str()) != 0) {
        std::string error = "Failed to move file: " + source + " -> " + destination + " - " + GetErrorMessage();

        return FileResult<>(false, error);
    }

    return FileResult<>(true);
}

FileResult<std::vector<std::string>> FileSystem::ListDirectory(const std::string& path, bool recursive) {
    if (!Exists(path)) {
        std::string error = "Directory does not exist: " + path;

        return FileResult<std::vector<std::string>>(false, error);
    }

    if (!IsDirectory(path)) {
        std::string error = "Path is not a directory: " + path;

        return FileResult<std::vector<std::string>>(false, error);
    }

    std::vector<std::string> results;

#if defined(LUPINE_PLATFORM_WINDOWS)
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA((path + "\\*").c_str(), &findData);

    if (hFind == INVALID_HANDLE_VALUE) {
        std::string error = "Failed to list directory: " + path + " - " + GetErrorMessage();

        return FileResult<std::vector<std::string>>(false, error);
    }

    do {
        std::string name = findData.cFileName;
        if (name != "." && name != "..") {
            std::string fullPath = Path::Join(path, name);
            results.push_back(fullPath);

            if (recursive && (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                ListDirectoryRecursive(fullPath, results);
            }
        }
    } while (FindNextFileA(hFind, &findData) != 0);

    FindClose(hFind);
#else
    DIR* dir = opendir(path.c_str());
    if (!dir) {
        std::string error = "Failed to list directory: " + path + " - " + GetErrorMessage();

        return FileResult<std::vector<std::string>>(false, error);
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name != "." && name != "..") {
            std::string fullPath = Path::Join(path, name);
            results.push_back(fullPath);

            if (recursive && IsDirectory(fullPath)) {
                ListDirectoryRecursive(fullPath, results);
            }
        }
    }

    closedir(dir);
#endif

    return FileResult<std::vector<std::string>>(true, "", results);
}

void FileSystem::ListDirectoryRecursive(const std::string& path, std::vector<std::string>& results, size_t baseDepth) {
    auto listResult = ListDirectory(path, false);
    if (!listResult.success) {
        return;
    }

    for (const auto& item : listResult.data) {
        results.push_back(item);
        if (IsDirectory(item)) {
            ListDirectoryRecursive(item, results, baseDepth + 1);
        }
    }
}

FileResult<std::string> FileSystem::GetCurrentDirectory() {
    char buffer[PATH_MAX];
    if (getcwd(buffer, sizeof(buffer)) == nullptr) {
        std::string error = "Failed to get current directory - " + GetErrorMessage();

        return FileResult<std::string>(false, error);
    }

    return FileResult<std::string>(true, "", std::string(buffer));
}

FileResult<> FileSystem::SetCurrentDirectory(const std::string& path) {
    if (chdir(path.c_str()) != 0) {
        std::string error = "Failed to set current directory: " + path + " - " + GetErrorMessage();

        return FileResult<>(false, error);
    }

    return FileResult<>(true);
}

FileResult<std::string> FileSystem::GetAbsolutePath(const std::string& path) {
    if (Path::IsAbsolute(path)) {
        return FileResult<std::string>(true, "", path);
    }

    auto cwdResult = GetCurrentDirectory();
    if (!cwdResult.success) {
        return FileResult<std::string>(false, cwdResult.error);
    }

    std::string absolute = Path::Join(cwdResult.data, path);
    return FileResult<std::string>(true, "", Path::Normalize(absolute));
}

FileResult<std::string> FileSystem::GetCanonicalPath(const std::string& path) {
#if defined(LUPINE_PLATFORM_WINDOWS)
    char buffer[MAX_PATH];
    if (GetFullPathNameA(path.c_str(), MAX_PATH, buffer, nullptr) == 0) {
        std::string error = "Failed to get canonical path: " + path + " - " + GetErrorMessage();

        return FileResult<std::string>(false, error);
    }
    return FileResult<std::string>(true, "", std::string(buffer));
#else
    char buffer[PATH_MAX];
    if (realpath(path.c_str(), buffer) == nullptr) {
        std::string error = "Failed to get canonical path: " + path + " - " + GetErrorMessage();

        return FileResult<std::string>(false, error);
    }
    return FileResult<std::string>(true, "", std::string(buffer));
#endif
}

}
}
