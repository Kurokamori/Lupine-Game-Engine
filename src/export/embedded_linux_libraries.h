#ifndef LUPINE_EMBEDDED_EMBEDDED_LINUX_LIBRARIES_H_H
#define LUPINE_EMBEDDED_EMBEDDED_LINUX_LIBRARIES_H_H

// Auto-generated file - DO NOT EDIT
// Generated by scripts/generate_embedded_libraries.py --platform linux

#include <cstdint>
#include <unordered_map>
#include <vector>
#include <string>

namespace Lupine {

// Test library: test_lib (lib/libtest.a)
// Size: 8 bytes
// Checksum: test_checksum
const uint8_t embedded_linux_test_lib_data[] = {
    0x7f, 0x45, 0x4c, 0x46, 0x02, 0x01, 0x01, 0x00,
};
const size_t embedded_linux_test_lib_data_size = 8;

/**
 * @brief Get map of embedded Linux libraries
 * @return Map of library name to data
 */
inline std::unordered_map<std::string, std::vector<uint8_t>> GetEmbeddedLinuxLibraries() {
    std::unordered_map<std::string, std::vector<uint8_t>> libraries;

    libraries["test_lib"] = std::vector<uint8_t>(
        embedded_linux_test_lib_data, embedded_linux_test_lib_data + embedded_linux_test_lib_data_size);

    return libraries;
}

} // namespace Lupine

#endif // LUPINE_EMBEDDED_EMBEDDED_LINUX_LIBRARIES_H_H
