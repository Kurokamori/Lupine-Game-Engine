/**
 * Embedded Python Runtime Implementation
 *
 * This module embeds the Python runtime directly into the executable.
 * On Windows, Python DLL and standard library are embedded as resources
 * and extracted to a temporary location at startup.
 */

#include "EmbeddedPython.hpp"
#include "lupine/platform/FileSystem.hpp"
#include "lupine/platform/Path.hpp"
#include "lupine/logger/Logger.hpp"

#include <fstream>
#include <cstdlib>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shlobj.h>

// Resource IDs for embedded Python files
#define IDR_PYTHON_DLL      1001
#define IDR_PYTHON_ZIP      1002
#define PYTHON_RESOURCE_TYPE "PYTHON_RUNTIME"

#endif

namespace lupine {
namespace runtime {

static std::string s_embeddedPythonPath;
static bool s_isEmbeddedMode = false;
static bool s_initialized = false;

#ifdef _WIN32

/**
 * Get the path to the executable
 */
static std::string GetExecutablePath() {
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    return std::string(path);
}

/**
 * Get the directory containing the executable
 */
static std::string GetExecutableDirectory() {
    std::string exePath = GetExecutablePath();
    return platform::Path::GetDirectory(exePath);
}

/**
 * Get a suitable directory for extracting embedded files
 */
static std::string GetExtractionDirectory() {
    // First try: directory next to executable (for portable installs)
    std::string exeDir = GetExecutableDirectory();
    std::string pythonDir = platform::Path::Join(exeDir, "python_runtime");

    // Check if we can write there
    if (CreateDirectoryA(pythonDir.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS) {
        return pythonDir;
    }

    // Fallback: user's local app data
    char appDataPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, appDataPath))) {
        std::string lupineDir = platform::Path::Join(std::string(appDataPath), "LupineEngine");
        CreateDirectoryA(lupineDir.c_str(), nullptr);

        pythonDir = platform::Path::Join(lupineDir, "python_runtime");
        CreateDirectoryA(pythonDir.c_str(), nullptr);
        return pythonDir;
    }

    // Last fallback: temp directory
    char tempPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);
    pythonDir = platform::Path::Join(std::string(tempPath), "lupine_python");
    CreateDirectoryA(pythonDir.c_str(), nullptr);
    return pythonDir;
}

/**
 * Extract a resource to a file
 */
static bool ExtractResource(HMODULE hModule, int resourceId, const char* resourceType,
                           const std::string& outputPath) {
    HRSRC hResource = FindResourceA(hModule, MAKEINTRESOURCEA(resourceId), resourceType);
    if (!hResource) {
        return false;
    }

    HGLOBAL hLoadedResource = LoadResource(hModule, hResource);
    if (!hLoadedResource) {
        return false;
    }

    void* pResourceData = LockResource(hLoadedResource);
    if (!pResourceData) {
        return false;
    }

    DWORD resourceSize = SizeofResource(hModule, hResource);
    if (resourceSize == 0) {
        return false;
    }

    // Write to file
    std::ofstream outFile(outputPath, std::ios::binary);
    if (!outFile) {
        return false;
    }

    outFile.write(static_cast<const char*>(pResourceData), resourceSize);
    outFile.close();

    return true;
}

/**
 * Check if the embedded Python files need to be extracted
 */
static bool NeedsExtraction(const std::string& pythonDir) {
    std::string dllPath = platform::Path::Join(pythonDir, "python312.dll");
    std::string zipPath = platform::Path::Join(pythonDir, "python312.zip");

    // Check if both files exist
    return !platform::FileSystem::Exists(dllPath) || !platform::FileSystem::Exists(zipPath);
}

/**
 * Extract embedded Python runtime files
 */
static bool ExtractEmbeddedPython(const std::string& pythonDir) {
    HMODULE hModule = GetModuleHandle(nullptr);

    // Check if we have embedded resources
    HRSRC hDllResource = FindResourceA(hModule, MAKEINTRESOURCEA(IDR_PYTHON_DLL), PYTHON_RESOURCE_TYPE);
    if (!hDllResource) {
        // No embedded Python - we're in development mode
        return false;
    }

    // Extract Python DLL
    std::string dllPath = platform::Path::Join(pythonDir, "python312.dll");
    if (!ExtractResource(hModule, IDR_PYTHON_DLL, PYTHON_RESOURCE_TYPE, dllPath)) {
        LOG_ERROR(LogCategory::Core, "Failed to extract Python DLL");
        return false;
    }

    // Extract Python standard library
    std::string zipPath = platform::Path::Join(pythonDir, "python312.zip");
    if (!ExtractResource(hModule, IDR_PYTHON_ZIP, PYTHON_RESOURCE_TYPE, zipPath)) {
        LOG_ERROR(LogCategory::Core, "Failed to extract Python standard library");
        return false;
    }

    return true;
}

/**
 * Add Python directory to DLL search path
 */
static bool SetupPythonDllPath(const std::string& pythonDir) {
    // Add to DLL search path
    SetDllDirectoryA(pythonDir.c_str());

    // Also add to PATH environment variable
    char currentPath[32768];
    GetEnvironmentVariableA("PATH", currentPath, sizeof(currentPath));

    std::string newPath = pythonDir + ";" + currentPath;
    SetEnvironmentVariableA("PATH", newPath.c_str());

    // Set PYTHONHOME and PYTHONPATH
    SetEnvironmentVariableA("PYTHONHOME", pythonDir.c_str());

    std::string pythonPath = platform::Path::Join(pythonDir, "python312.zip");
    SetEnvironmentVariableA("PYTHONPATH", pythonPath.c_str());

    return true;
}

bool InitializeEmbeddedPython() {
    if (s_initialized) {
        return true;
    }

    // Get extraction directory
    std::string pythonDir = GetExtractionDirectory();

    // Check if we have embedded Python resources
    HMODULE hModule = GetModuleHandle(nullptr);
    HRSRC hDllResource = FindResourceA(hModule, MAKEINTRESOURCEA(IDR_PYTHON_DLL), PYTHON_RESOURCE_TYPE);

    if (hDllResource) {
        // We have embedded Python - this is an exported game
        s_isEmbeddedMode = true;

        // Extract if needed
        if (NeedsExtraction(pythonDir)) {
            if (!ExtractEmbeddedPython(pythonDir)) {
                LOG_ERROR(LogCategory::Core, "Failed to extract embedded Python");
                return false;
            }
        }

        // Setup DLL search path
        if (!SetupPythonDllPath(pythonDir)) {
            LOG_ERROR(LogCategory::Core, "Failed to setup Python DLL path");
            return false;
        }

        s_embeddedPythonPath = pythonDir;
    } else {
        // No embedded Python - we're in development mode
        // Python should be available from vcpkg or system installation
        s_isEmbeddedMode = false;
        s_embeddedPythonPath = "";
    }

    s_initialized = true;
    return true;
}

void ShutdownEmbeddedPython() {
    // Reset DLL search path
    if (s_isEmbeddedMode) {
        SetDllDirectoryA(nullptr);
    }

    s_initialized = false;
    s_isEmbeddedMode = false;
    s_embeddedPythonPath.clear();
}

#else // Non-Windows platforms

bool InitializeEmbeddedPython() {
    if (s_initialized) {
        return true;
    }

    // On Unix-like systems, Python is typically available system-wide
    // or can be bundled as a shared library
    s_isEmbeddedMode = false;
    s_embeddedPythonPath = "";
    s_initialized = true;

    return true;
}

void ShutdownEmbeddedPython() {
    s_initialized = false;
    s_isEmbeddedMode = false;
    s_embeddedPythonPath.clear();
}

#endif // _WIN32

std::string GetEmbeddedPythonPath() {
    return s_embeddedPythonPath;
}

bool IsEmbeddedMode() {
    return s_isEmbeddedMode;
}

} // namespace runtime
} // namespace lupine
