#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <memory>
#include <fstream>
#include <mutex>

namespace lupine {
namespace platform {

/**
 * Pack file format (.pck) for bundling game assets
 *
 * File structure:
 * - Header (32 bytes):
 *   - Magic number: "LPCK" (4 bytes)
 *   - Version: uint32_t (4 bytes)
 *   - Flags: uint32_t (4 bytes)
 *   - File count: uint32_t (4 bytes)
 *   - Index offset: uint64_t (8 bytes)
 *   - Data offset: uint64_t (8 bytes)
 *
 * - Data section:
 *   - Concatenated file data (compressed or raw)
 *
 * - Index section:
 *   - For each file:
 *     - Path length: uint32_t
 *     - Path: UTF-8 string (without null terminator)
 *     - Data offset: uint64_t
 *     - Compressed size: uint64_t
 *     - Uncompressed size: uint64_t
 *     - Flags: uint32_t (compression method, etc.)
 *     - CRC32: uint32_t
 */

constexpr uint32_t PACK_MAGIC = 0x4B43504C;  // "LPCK" in little-endian
constexpr uint32_t PACK_VERSION = 1;

enum class PackFileFlags : uint32_t {
    None = 0,
    Encrypted = 1 << 0,
    Compressed = 1 << 1,
};

enum class PackEntryFlags : uint32_t {
    None = 0,
    Compressed = 1 << 0,       // Entry is compressed (zlib)
    Encrypted = 1 << 1,        // Entry is encrypted
    Binary = 1 << 2,           // Entry is binary data
};

struct PackHeader {
    uint32_t magic = PACK_MAGIC;
    uint32_t version = PACK_VERSION;
    uint32_t flags = 0;
    uint32_t fileCount = 0;
    uint64_t indexOffset = 0;
    uint64_t dataOffset = 0;
};

struct PackEntry {
    std::string path;
    uint64_t dataOffset = 0;
    uint64_t compressedSize = 0;
    uint64_t uncompressedSize = 0;
    uint32_t flags = 0;
    uint32_t crc32 = 0;
};

/**
 * PackFileReader - Reads pack files for runtime asset loading
 */
class PackFileReader {
public:
    PackFileReader();
    ~PackFileReader();

    // Open a pack file for reading
    bool open(const std::string& path);

    // Open from memory (for embedded packs)
    bool openFromMemory(const uint8_t* data, size_t size);

    // Close the pack file
    void close();

    // Check if pack is open
    bool isOpen() const { return m_isOpen; }

    // Check if a file exists in the pack
    bool exists(const std::string& path) const;

    // Get list of all files in pack
    std::vector<std::string> listFiles() const;

    // Get list of files in a directory (virtual path)
    std::vector<std::string> listDirectory(const std::string& dir) const;

    // Read a file's contents
    std::vector<uint8_t> readFile(const std::string& path) const;

    // Read a file as string
    std::string readFileAsString(const std::string& path) const;

    // Get file info
    const PackEntry* getEntry(const std::string& path) const;

    // Get pack file path
    const std::string& getPath() const { return m_path; }

private:
    bool readHeader();
    bool readIndex();
    std::vector<uint8_t> decompressData(const std::vector<uint8_t>& compressed, size_t uncompressedSize) const;

    std::string m_path;
    mutable std::mutex m_mutex;
    std::unique_ptr<std::ifstream> m_file;
    const uint8_t* m_memoryData = nullptr;
    size_t m_memorySize = 0;
    bool m_isOpen = false;
    bool m_isMemory = false;
    PackHeader m_header;
    std::unordered_map<std::string, PackEntry> m_entries;
};

/**
 * PackFileWriter - Creates pack files for export
 */
class PackFileWriter {
public:
    PackFileWriter();
    ~PackFileWriter();

    // Start writing a new pack file
    bool create(const std::string& path);

    // Add a file to the pack
    bool addFile(const std::string& packPath, const std::string& sourcePath);

    // Add data directly to the pack
    bool addData(const std::string& packPath, const std::vector<uint8_t>& data);

    // Add string data to the pack
    bool addString(const std::string& packPath, const std::string& content);

    // Enable compression for subsequent files
    void setCompression(bool enabled) { m_compression = enabled; }

    // Finalize and close the pack file
    bool finalize();

private:
    std::vector<uint8_t> compressData(const std::vector<uint8_t>& data) const;
    uint32_t calculateCRC32(const std::vector<uint8_t>& data) const;

    std::string m_path;
    std::unique_ptr<std::ofstream> m_file;
    std::vector<PackEntry> m_entries;
    std::vector<std::vector<uint8_t>> m_pendingData;
    bool m_compression = true;
    uint64_t m_currentDataOffset = 0;
};

/**
 * PackFileSystem - Unified access to pack files and filesystem
 *
 * Provides transparent access to files, first checking pack files,
 * then falling back to the real filesystem.
 *
 * Now supports UUID-based asset lookups for exported games.
 * UUID mappings are stored in the pack file and loaded at runtime.
 *
 * Note: This is separate from VirtualFileSystem which handles mount points.
 * PackFileSystem is specifically for .pck file loading in exported games.
 */
class PackFileSystem {
public:
    static PackFileSystem& Instance();

    // Mount a pack file with a priority (higher = checked first)
    bool mountPack(const std::string& packPath, int priority = 0);

    // Mount from embedded data
    bool mountPackFromMemory(const uint8_t* data, size_t size, const std::string& name, int priority = 0);

    // Unmount a pack file
    void unmountPack(const std::string& packPath);

    // Set the base path for filesystem fallback
    void setBasePath(const std::string& path) { m_basePath = path; }
    const std::string& getBasePath() const { return m_basePath; }

    // Enable/disable filesystem fallback
    void setFilesystemFallback(bool enabled) { m_filesystemFallback = enabled; }

    // Check if running from pack file (export mode)
    bool isPackMode() const { return !m_packs.empty(); }

    // File operations
    bool exists(const std::string& path) const;
    std::vector<uint8_t> readFile(const std::string& path) const;
    std::string readFileAsString(const std::string& path) const;
    std::vector<std::string> listDirectory(const std::string& dir) const;

    /**
     * Size in bytes of a packed file, without reading or decompressing it.
     * Reports the UNCOMPRESSED size, which is what a caller sizing a buffer needs -
     * the on-disk compressed size is an internal detail of the pack.
     * @param path Path to size (res:// or pack-relative)
     * @param outSize Receives the size on success; untouched otherwise
     * @return true when the path resolved to a pack entry or a fallback file
     */
    bool getFileSize(const std::string& path, uint64_t& outSize) const;

    // Recursively list all files under a directory prefix. Returns pack-relative
    // paths (no res:// prefix) for every entry whose path begins with the given
    // directory, descending into all subdirectories. Includes filesystem-fallback
    // results when enabled.
    std::vector<std::string> listFilesRecursive(const std::string& dir) const;

    // ========================================================================
    // UUID-based Asset Resolution
    // ========================================================================

    /**
     * Resolve a path or UUID to an actual path
     * Checks UUID mappings first, then tries as a regular path
     * @param pathOrUUID Either a res:// path or a UUID string
     * @return Resolved path (suitable for exists/readFile calls)
     */
    std::string resolveAsset(const std::string& pathOrUUID) const;

    /**
     * Check if an asset exists by UUID or path
     */
    bool assetExists(const std::string& pathOrUUID) const;

    /**
     * Read an asset file by UUID or path
     */
    std::vector<uint8_t> readAsset(const std::string& pathOrUUID) const;

    /**
     * Load UUID mappings from pack file
     * Typically stored as "__uuid_mappings__.json" in the pack
     */
    void loadUUIDMappings();

    /**
     * Get path for a UUID (from loaded mappings)
     */
    std::string getPathForUUID(const std::string& uuidStr) const;

private:
    PackFileSystem() = default;
    ~PackFileSystem() = default;

    /**
     * Result of validating an untrusted asset path.
     *
     * `valid` is false when the path escapes the asset root (a `..` segment that
     * climbs above it, a drive letter, an absolute path while a base path is set);
     * the caller must then fail the operation.
     *
     * `hasRelative` says the path is a legal pack-relative key (`relative`), and
     * `hasPhysical` that it may be handed to the real filesystem (`physical`).
     * An absolute path with no configured base path has a physical form but no
     * pack-relative one.
     */
    struct PathResolution {
        bool valid = false;
        bool hasRelative = false;
        bool hasPhysical = false;
        std::string relative;
        std::string physical;
    };

    // Validate an untrusted path and derive its pack key / physical form.
    // Callers must hold m_mutex (it reads m_basePath).
    PathResolution resolvePath(const std::string& path) const;

    // Strip the res:// scheme and any leading separators for consistent lookup.
    // This does NOT remove `..` segments - use resolvePath for untrusted input.
    std::string normalizePath(const std::string& path) const;

    // Check if string looks like a UUID
    bool isUUIDString(const std::string& str) const;

    struct MountedPack {
        std::unique_ptr<PackFileReader> reader;
        std::string name;
        int priority;
    };

    std::vector<MountedPack> m_packs;
    std::string m_basePath;
    bool m_filesystemFallback = true;
    mutable std::mutex m_mutex;

    // UUID -> path mappings (loaded from pack file)
    std::unordered_map<std::string, std::string> m_uuidToPath;
    bool m_uuidMappingsLoaded = false;
};

// UUID mappings file name in pack files
constexpr const char* PACK_UUID_MAPPINGS_FILE = "__uuid_mappings__.json";

} // namespace platform
} // namespace lupine
