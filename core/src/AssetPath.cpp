#include "lupine/asset/AssetPath.hpp"
#include "lupine/asset/AssetDatabase.hpp"
#include "lupine/platform/Path.hpp"
#include "lupine/platform/VirtualFileSystem.hpp"
#include "lupine/platform/FileSystem.hpp"
#include <algorithm>

namespace lupine {
namespace asset {

using namespace platform;

bool AssetPath::IsResourcePath(const std::string& path) {
    return path.size() >= 6 && path.substr(0, 6) == RES_PREFIX;
}

bool AssetPath::IsVirtualPath(const std::string& path) {
    if (path.size() < 4) return false;
    
    size_t pos = path.find("://");
    if (pos == std::string::npos || pos == 0) return false;
    
    std::string prefix = path.substr(0, pos + 3);
    return prefix == RES_PREFIX || prefix == USER_PREFIX || 
           prefix == TEMP_PREFIX || prefix == APP_PREFIX;
}

bool AssetPath::IsAbsolutePath(const std::string& path) {
    return Path::IsAbsolute(path);
}

std::string AssetPath::GetVirtualPrefix(const std::string& path) {
    size_t pos = path.find("://");
    if (pos == std::string::npos || pos == 0) {
        return "";
    }
    return path.substr(0, pos + 3);
}

std::string AssetPath::GetRelativePath(const std::string& virtualPath) {
    size_t pos = virtualPath.find("://");
    if (pos == std::string::npos) {
        return virtualPath;
    }
    return virtualPath.substr(pos + 3);
}

std::string AssetPath::Normalize(const std::string& path) {
    if (path.empty()) return "";
    
    std::string prefix;
    std::string relativePart;
    
    if (IsVirtualPath(path)) {
        prefix = GetVirtualPrefix(path);
        relativePart = GetRelativePath(path);
    } else {
        relativePart = path;
    }
    
    // Convert to unix separators
    std::string normalized = Path::ToUnixSeparators(relativePart);
    
    // Split into segments
    std::vector<std::string> segments;
    std::string segment;
    
    for (char c : normalized) {
        if (c == '/') {
            if (!segment.empty()) {
                if (segment == "..") {
                    if (!segments.empty() && segments.back() != "..") {
                        segments.pop_back();
                    }
                } else if (segment != ".") {
                    segments.push_back(segment);
                }
                segment.clear();
            }
        } else {
            segment += c;
        }
    }
    
    // Handle last segment
    if (!segment.empty()) {
        if (segment == "..") {
            if (!segments.empty() && segments.back() != "..") {
                segments.pop_back();
            }
        } else if (segment != ".") {
            segments.push_back(segment);
        }
    }
    
    // Rebuild path
    std::string result = prefix;
    for (size_t i = 0; i < segments.size(); ++i) {
        if (i > 0) result += "/";
        result += segments[i];
    }
    
    return result;
}

std::string AssetPath::ToResourcePath(const std::string& path) {
    return AssetDatabase::GetInstance().ToResourcePath(path);
}

std::string AssetPath::ToPhysicalPath(const std::string& resPath) {
    if (!IsResourcePath(resPath)) {
        return resPath;
    }
    
    auto& vfs = VirtualFileSystem::GetInstance();
    auto result = vfs.ResolvePath(resPath);
    return result.success ? result.data : "";
}

std::string AssetPath::Resolve(const std::string& pathOrUUID) {
    return AssetDatabase::GetInstance().ResolveAsset(pathOrUUID);
}

std::string AssetPath::ResolveWithFallback(const core::UUID& uuid, const std::string& fallbackPath) {
    return AssetDatabase::GetInstance().ResolveAssetWithFallback(uuid, fallbackPath);
}

bool AssetPath::IsValidAssetPath(const std::string& path) {
    std::string resPath = ToResourcePath(path);
    if (resPath.empty()) return false;
    
    std::string physicalPath = ToPhysicalPath(resPath);
    return FileSystem::Exists(physicalPath);
}

std::string AssetPath::SanitizeForStorage(const std::string& path) {
    if (path.empty()) return "";
    
    // Convert to res:// path
    std::string resPath = ToResourcePath(path);
    if (resPath.empty()) return "";
    
    // Normalize
    return Normalize(resPath);
}

std::string AssetPath::GetExtension(const std::string& path) {
    return Path::GetExtension(path);
}

std::string AssetPath::GetFilename(const std::string& path) {
    return Path::GetFilename(path);
}

std::string AssetPath::GetDirectory(const std::string& path) {
    return Path::GetDirectory(path);
}

std::string AssetPath::Join(const std::string& base, const std::string& relative) {
    return Path::Join(base, relative);
}

std::string AssetPath::ChangeExtension(const std::string& path, const std::string& newExtension) {
    if (path.empty()) {
        return "";
    }

    // Find the last dot (for extension) and last separator
    size_t lastDot = path.rfind('.');
    size_t lastSep = path.rfind('/');
    if (lastSep == std::string::npos) {
        lastSep = path.rfind('\\');
    }

    // If there's no dot, or the dot is before the last separator (part of a directory name)
    if (lastDot == std::string::npos || (lastSep != std::string::npos && lastDot < lastSep)) {
        // No extension, just append the new one
        return path + newExtension;
    }

    // Replace the extension
    return path.substr(0, lastDot) + newExtension;
}

} // namespace asset
} // namespace lupine

