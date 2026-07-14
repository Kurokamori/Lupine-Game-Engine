#include "lupine/platform/Path.hpp"
#include "lupine/logger/Logger.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace lupine {
namespace platform {

std::string Path::Join(const std::vector<std::string>& segments) {
    if (segments.empty()) {
        return "";
    }

    std::string result = segments[0];
    for (size_t i = 1; i < segments.size(); ++i) {
        if (segments[i].empty()) {
            continue;
        }

        if (!result.empty() && !IsSeparator(result.back()) && !IsSeparator(segments[i].front())) {
            result += SEPARATOR;
        }

        if (!result.empty() && IsSeparator(result.back()) && IsSeparator(segments[i].front())) {
            result += segments[i].substr(1);
        } else {
            result += segments[i];
        }
    }

    return Normalize(result);
}

std::string Path::Join(const std::string& path1, const std::string& path2) {
    return Join({path1, path2});
}

std::string Path::Join(const std::string& path1, const std::string& path2, const std::string& path3) {
    return Join({path1, path2, path3});
}

std::string Path::Normalize(const std::string& path) {
    if (path.empty()) {
        return "";
    }

    // Check for virtual path schemes (res://, user://, app://, temp://)
    // These should preserve forward slashes and not use platform separators
    size_t schemeEnd = path.find("://");
    std::string scheme;
    std::string pathPart = path;
    bool isVirtualPath = false;

    if (schemeEnd != std::string::npos && schemeEnd < 10) {  // Reasonable scheme length
        std::string potentialScheme = path.substr(0, schemeEnd);
        // Check for known virtual path schemes
        if (potentialScheme == "res" || potentialScheme == "user" ||
            potentialScheme == "app" || potentialScheme == "temp") {
            isVirtualPath = true;
            scheme = path.substr(0, schemeEnd + 3);  // Include "://"
            pathPart = path.substr(schemeEnd + 3);
        }
    }

    std::string normalized = ToUnixSeparators(pathPart);

    std::vector<std::string> segments;
    std::stringstream ss(normalized);
    std::string segment;

    while (std::getline(ss, segment, '/')) {
        if (segment.empty() || segment == ".") {
            continue;
        }

        if (segment == "..") {

            if (!segments.empty() && segments.back() != "..") {
                segments.pop_back();
            } else if (!IsAbsolute(pathPart)) {

                segments.push_back(segment);
            }
        } else {
            segments.push_back(segment);
        }
    }

    std::string result;
    size_t segmentStartIndex = 0;

    // Virtual paths always use forward slashes
    char sep = isVirtualPath ? '/' : SEPARATOR;

    if (!isVirtualPath && IsAbsolute(pathPart)) {
#if defined(LUPINE_PLATFORM_WINDOWS)

        if (pathPart.size() >= 2 && pathPart[1] == ':') {
            result = pathPart.substr(0, 2);
            if (!segments.empty()) {
                result += sep;
            }

            if (!segments.empty() && segments[0].size() == 2 &&
                std::isalpha(segments[0][0]) && segments[0][1] == ':') {
                segmentStartIndex = 1;
            }
        } else if (pathPart.size() >= 2 && IsSeparator(pathPart[0]) && IsSeparator(pathPart[1])) {
            result = "\\\\";
        }
#else
        result = "/";
#endif
    }

    for (size_t i = segmentStartIndex; i < segments.size(); ++i) {
        result += segments[i];
        if (i < segments.size() - 1) {
            result += sep;
        }
    }

    if (result.empty()) {
        result = ".";
    }

    // Prepend the virtual scheme if present
    if (isVirtualPath) {
        return scheme + result;
    }

    return result;
}

bool Path::NormalizeSandboxRelative(const std::string& relativePath, std::string& outRelative) {
    outRelative.clear();

    std::string unixPath = ToUnixSeparators(relativePath);

    std::vector<std::string> segments;
    std::stringstream ss(unixPath);
    std::string segment;

    while (std::getline(ss, segment, '/')) {
        if (segment.empty() || segment == ".") {
            continue;
        }

        // A colon in a segment means a drive letter ("C:"), a drive-relative path
        // ("C:foo") or an NTFS alternate data stream - none of which may appear in a
        // sandboxed relative path, and all of which would defeat the join below.
        if (segment.find(':') != std::string::npos) {
            return false;
        }

        if (segment == "..") {
            if (segments.empty()) {
                return false;
            }
            segments.pop_back();
            continue;
        }

        segments.push_back(segment);
    }

    for (size_t i = 0; i < segments.size(); ++i) {
        if (i > 0) {
            outRelative += '/';
        }
        outRelative += segments[i];
    }

    return true;
}

std::string Path::JoinSandboxed(const std::string& root, const std::string& relativePath) {
    if (root.empty()) {
        return "";
    }

    std::string safeRelative;
    if (!NormalizeSandboxRelative(relativePath, safeRelative)) {
        return "";
    }

    if (safeRelative.empty()) {
        return Normalize(root);
    }

    std::string joined = Join(root, safeRelative);

    // NormalizeSandboxRelative already guarantees containment; this re-checks the
    // fully joined result so a future change to Join() cannot silently reopen the hole.
    if (!IsSubPath(root, joined)) {
        return "";
    }

    return joined;
}

bool Path::IsSubPath(const std::string& root, const std::string& path) {
    if (root.empty() || path.empty()) {
        return false;
    }

    std::string normRoot = ToUnixSeparators(Normalize(root));
    std::string normPath = ToUnixSeparators(Normalize(path));

    if (normRoot.empty() || normPath.empty()) {
        return false;
    }

    // The filesystem root is the one root whose only character is a separator, so
    // RemoveTrailingSeparators below would erase it entirely and the containment
    // check would reject every path. It contains every absolute path by definition.
    // (Reachable on Emscripten, where the pack/VFS base path is "/".)
    if (normRoot == "/") {
        return normPath[0] == '/';
    }

    normRoot = RemoveTrailingSeparators(normRoot);
    normPath = RemoveTrailingSeparators(normPath);

    if (normRoot.empty() || normPath.empty()) {
        return false;
    }

#if defined(LUPINE_PLATFORM_WINDOWS)
    std::transform(normRoot.begin(), normRoot.end(), normRoot.begin(),
                   [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
    std::transform(normPath.begin(), normPath.end(), normPath.begin(),
                   [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
#endif

    if (normPath.size() < normRoot.size()) {
        return false;
    }

    if (normPath.compare(0, normRoot.size(), normRoot) != 0) {
        return false;
    }

    return normPath.size() == normRoot.size() || normPath[normRoot.size()] == '/';
}

std::string Path::GetDirectory(const std::string& path) {
    if (path.empty()) {
        return "";
    }

    size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos) {
        return ".";
    }

    return path.substr(0, pos);
}

std::string Path::GetFilename(const std::string& path) {
    if (path.empty()) {
        return "";
    }

    size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos) {
        return path;
    }

    return path.substr(pos + 1);
}

std::string Path::GetFilenameWithoutExtension(const std::string& path) {
    std::string filename = GetFilename(path);
    size_t pos = filename.find_last_of('.');

    if (pos == std::string::npos || pos == 0) {
        return filename;
    }

    return filename.substr(0, pos);
}

std::string Path::GetExtension(const std::string& path) {
    std::string filename = GetFilename(path);
    size_t pos = filename.find_last_of('.');

    if (pos == std::string::npos || pos == 0) {
        return "";
    }

    return filename.substr(pos);
}

bool Path::IsAbsolute(const std::string& path) {
    if (path.empty()) {
        return false;
    }

#if defined(LUPINE_PLATFORM_WINDOWS)

    if (path.size() >= 2) {

        if (std::isalpha(path[0]) && path[1] == ':') {
            return true;
        }

        if (IsSeparator(path[0]) && IsSeparator(path[1])) {
            return true;
        }
    }
    return false;
#else

    return IsSeparator(path[0]);
#endif
}

bool Path::IsRelative(const std::string& path) {
    return !IsAbsolute(path);
}

std::string Path::ToNativeSeparators(const std::string& path) {
    std::string result = path;
    std::replace(result.begin(), result.end(), ALT_SEPARATOR, SEPARATOR);
    return result;
}

std::string Path::ToUnixSeparators(const std::string& path) {
    std::string result = path;
    std::replace(result.begin(), result.end(), '\\', '/');
    return result;
}

bool Path::IsSeparator(char c) {
    return c == '/' || c == '\\';
}

std::string Path::RemoveTrailingSeparators(const std::string& path) {
    if (path.empty()) {
        return "";
    }

    size_t end = path.size();
    while (end > 0 && IsSeparator(path[end - 1])) {
        --end;
    }

    return path.substr(0, end);
}

std::vector<std::string> Path::Split(const std::string& path) {
    std::vector<std::string> segments;
    std::string normalized = ToUnixSeparators(path);
    std::stringstream ss(normalized);
    std::string segment;

    while (std::getline(ss, segment, '/')) {
        if (!segment.empty()) {
            segments.push_back(segment);
        }
    }

    return segments;
}

bool Path::IsValid(const std::string& path) {
    if (path.empty()) {
        return false;
    }

    for (char c : path) {
        if (!IsValidPathChar(c)) {
            return false;
        }
    }

    return true;
}

bool Path::IsValidPathChar(char c) {
#if defined(LUPINE_PLATFORM_WINDOWS)

    static const char invalidChars[] = "<>:\"|?*";
    for (char invalid : invalidChars) {
        if (c == invalid) {
            return false;
        }
    }

    if (c < 32) {
        return false;
    }
#else

    if (c == '\0') {
        return false;
    }
#endif

    return true;
}

}
}
