#include "lupine/platform/Path.hpp"
#include "lupine/logger/Logger.hpp"
#include <algorithm>
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

    std::string normalized = ToUnixSeparators(path);

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
            } else if (!IsAbsolute(path)) {

                segments.push_back(segment);
            }
        } else {
            segments.push_back(segment);
        }
    }

    std::string result;
    size_t segmentStartIndex = 0;

    if (IsAbsolute(path)) {
#if defined(LUPINE_PLATFORM_WINDOWS)

        if (path.size() >= 2 && path[1] == ':') {
            result = path.substr(0, 2);
            if (!segments.empty()) {
                result += SEPARATOR;
            }

            if (!segments.empty() && segments[0].size() == 2 &&
                std::isalpha(segments[0][0]) && segments[0][1] == ':') {
                segmentStartIndex = 1;
            }
        } else if (path.size() >= 2 && IsSeparator(path[0]) && IsSeparator(path[1])) {
            result = "\\\\";
        }
#else
        result = "/";
#endif
    }

    for (size_t i = segmentStartIndex; i < segments.size(); ++i) {
        result += segments[i];
        if (i < segments.size() - 1) {
            result += SEPARATOR;
        }
    }

    if (result.empty()) {
        result = ".";
    }

    return result;
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
