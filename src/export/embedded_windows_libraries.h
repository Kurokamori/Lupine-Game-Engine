#ifndef LUPINE_EMBEDDED_EMBEDDED_WINDOWS_LIBRARIES_H_H
#define LUPINE_EMBEDDED_EMBEDDED_WINDOWS_LIBRARIES_H_H

// Auto-generated file - DO NOT EDIT
// Generated by scripts/generate_embedded_libraries.py --platform windows

#include <cstdint>
#include <unordered_map>
#include <vector>
#include <string>

namespace Lupine {

// Test library: test_lib (lib/test.lib)
// Size: 8 bytes
// Checksum: test_checksum
const uint8_t embedded_windows_test_lib_data[] = {
    0x4d, 0x5a, 0x90, 0x00, 0x03, 0x00, 0x00, 0x00,
};
const size_t embedded_windows_test_lib_data_size = 8;

/**
 * @brief Get map of embedded Windows libraries
 * @return Map of library name to data
 */
inline std::unordered_map<std::string, std::vector<uint8_t>> GetEmbeddedWindowsLibraries() {
    std::unordered_map<std::string, std::vector<uint8_t>> libraries;

    libraries["test_lib"] = std::vector<uint8_t>(
        embedded_windows_test_lib_data, embedded_windows_test_lib_data + embedded_windows_test_lib_data_size);

    return libraries;
}

} // namespace Lupine

#endif // LUPINE_EMBEDDED_EMBEDDED_WINDOWS_LIBRARIES_H_H
