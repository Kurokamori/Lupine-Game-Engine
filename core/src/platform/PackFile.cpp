#include "lupine/platform/PackFile.hpp"
#include "lupine/platform/FileSystem.hpp"
#include "lupine/platform/Path.hpp"
#include "lupine/logger/Logger.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstring>
#include <cctype>
#include <zlib.h>

namespace lupine {
namespace platform {

// CRC32 lookup table
static uint32_t s_crc32Table[256] = {0};
static bool s_crc32TableInitialized = false;

static void initCRC32Table() {
    if (s_crc32TableInitialized) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            crc = (crc & 1u) ? ((crc >> 1) ^ 0xEDB88320u) : (crc >> 1);
        }
        s_crc32Table[i] = crc;
    }
    s_crc32TableInitialized = true;
}

// =============================================================================
// PackFileReader Implementation
// =============================================================================

PackFileReader::PackFileReader() {
    initCRC32Table();
}

PackFileReader::~PackFileReader() {
    close();
}

bool PackFileReader::open(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_isOpen) {
        close();
    }

    m_file = std::make_unique<std::ifstream>(path, std::ios::binary);
    if (!m_file->is_open()) {
        LOG_ERROR(LogCategory::Core, "PackFileReader: Failed to open pack file: {}", path);
        return false;
    }

    m_path = path;
    m_isMemory = false;

    if (!readHeader()) {
        m_file.reset();
        return false;
    }

    if (!readIndex()) {
        m_file.reset();
        return false;
    }

    m_isOpen = true;
    
    return true;
}

bool PackFileReader::openFromMemory(const uint8_t* data, size_t size) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_isOpen) {
        close();
    }

    if (!data || size < sizeof(PackHeader)) {
        LOG_ERROR(LogCategory::Core, "PackFileReader: Invalid memory data");
        return false;
    }

    m_memoryData = data;
    m_memorySize = size;
    m_isMemory = true;
    m_path = "<memory>";

    if (!readHeader()) {
        m_memoryData = nullptr;
        m_memorySize = 0;
        return false;
    }

    if (!readIndex()) {
        m_memoryData = nullptr;
        m_memorySize = 0;
        return false;
    }

    m_isOpen = true;
    LOG_ERROR(LogCategory::Core, "PackFileReader: Opened pack from memory ({} files)", m_entries.size());
    return true;
}

void PackFileReader::close() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_file) {
        m_file->close();
        m_file.reset();
    }

    m_memoryData = nullptr;
    m_memorySize = 0;
    m_isOpen = false;
    m_isMemory = false;
    m_entries.clear();
    m_path.clear();
}

bool PackFileReader::readHeader() {
    if (m_isMemory) {
        if (m_memorySize < sizeof(PackHeader)) {
            LOG_ERROR(LogCategory::Core, "PackFileReader: Memory too small for header");
            return false;
        }
        std::memcpy(&m_header, m_memoryData, sizeof(PackHeader));
    } else {
        m_file->seekg(0, std::ios::beg);
        m_file->read(reinterpret_cast<char*>(&m_header), sizeof(PackHeader));
        if (!*m_file) {
            LOG_ERROR(LogCategory::Core, "PackFileReader: Failed to read header");
            return false;
        }
    }

    if (m_header.magic != PACK_MAGIC) {
        LOG_ERROR(LogCategory::Core, "PackFileReader: Invalid magic number (expected LPCK)");
        return false;
    }

    if (m_header.version > PACK_VERSION) {
        LOG_ERROR(LogCategory::Core, "PackFileReader: Unsupported version {} (max {})", m_header.version, PACK_VERSION);
        return false;
    }

    return true;
}

bool PackFileReader::readIndex() {
    m_entries.clear();

    // Seek to index section
    if (m_isMemory) {
        if (m_header.indexOffset >= m_memorySize) {
            LOG_ERROR(LogCategory::Core, "PackFileReader: Index offset out of bounds");
            return false;
        }
    } else {
        m_file->seekg(m_header.indexOffset, std::ios::beg);
        if (!*m_file) {
            LOG_ERROR(LogCategory::Core, "PackFileReader: Failed to seek to index");
            return false;
        }
    }

    size_t readPos = static_cast<size_t>(m_header.indexOffset);

    for (uint32_t i = 0; i < m_header.fileCount; i++) {
        PackEntry entry;

        // Read path length
        uint32_t pathLength = 0;
        if (m_isMemory) {
            if (readPos + sizeof(uint32_t) > m_memorySize) return false;
            std::memcpy(&pathLength, m_memoryData + readPos, sizeof(uint32_t));
            readPos += sizeof(uint32_t);
        } else {
            m_file->read(reinterpret_cast<char*>(&pathLength), sizeof(uint32_t));
            if (!*m_file) return false;
        }

        // Read path
        entry.path.resize(pathLength);
        if (m_isMemory) {
            if (readPos + pathLength > m_memorySize) return false;
            std::memcpy(&entry.path[0], m_memoryData + readPos, pathLength);
            readPos += pathLength;
        } else {
            m_file->read(&entry.path[0], pathLength);
            if (!*m_file) return false;
        }

        // Read entry data
        if (m_isMemory) {
            if (readPos + sizeof(uint64_t) * 3 + sizeof(uint32_t) * 2 > m_memorySize) return false;
            std::memcpy(&entry.dataOffset, m_memoryData + readPos, sizeof(uint64_t));
            readPos += sizeof(uint64_t);
            std::memcpy(&entry.compressedSize, m_memoryData + readPos, sizeof(uint64_t));
            readPos += sizeof(uint64_t);
            std::memcpy(&entry.uncompressedSize, m_memoryData + readPos, sizeof(uint64_t));
            readPos += sizeof(uint64_t);
            std::memcpy(&entry.flags, m_memoryData + readPos, sizeof(uint32_t));
            readPos += sizeof(uint32_t);
            std::memcpy(&entry.crc32, m_memoryData + readPos, sizeof(uint32_t));
            readPos += sizeof(uint32_t);
        } else {
            m_file->read(reinterpret_cast<char*>(&entry.dataOffset), sizeof(uint64_t));
            m_file->read(reinterpret_cast<char*>(&entry.compressedSize), sizeof(uint64_t));
            m_file->read(reinterpret_cast<char*>(&entry.uncompressedSize), sizeof(uint64_t));
            m_file->read(reinterpret_cast<char*>(&entry.flags), sizeof(uint32_t));
            m_file->read(reinterpret_cast<char*>(&entry.crc32), sizeof(uint32_t));
            if (!*m_file) return false;
        }

        // Normalize path and store
        std::string normalizedPath = entry.path;
        std::replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');
        m_entries[normalizedPath] = std::move(entry);
    }

    return true;
}

bool PackFileReader::exists(const std::string& path) const {
    std::string normalizedPath = path;
    std::replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');
    return m_entries.find(normalizedPath) != m_entries.end();
}

std::vector<std::string> PackFileReader::listFiles() const {
    std::vector<std::string> files;
    files.reserve(m_entries.size());
    for (const auto& [path, entry] : m_entries) {
        files.push_back(path);
    }
    return files;
}

std::vector<std::string> PackFileReader::listDirectory(const std::string& dir) const {
    std::vector<std::string> files;
    std::string normalizedDir = dir;
    std::replace(normalizedDir.begin(), normalizedDir.end(), '\\', '/');

    // Ensure directory ends with /
    if (!normalizedDir.empty() && normalizedDir.back() != '/') {
        normalizedDir += '/';
    }

    for (const auto& [path, entry] : m_entries) {
        if (path.compare(0, normalizedDir.size(), normalizedDir) == 0) {
            // Get the relative path
            std::string relativePath = path.substr(normalizedDir.size());
            // Only include direct children (no subdirectory)
            if (relativePath.find('/') == std::string::npos) {
                files.push_back(path);
            }
        }
    }

    return files;
}

std::vector<uint8_t> PackFileReader::readFile(const std::string& path) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string normalizedPath = path;
    std::replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');

    auto it = m_entries.find(normalizedPath);
    if (it == m_entries.end()) {
        
        return {};
    }

    const PackEntry& entry = it->second;
    std::vector<uint8_t> data;
    data.resize(static_cast<size_t>(entry.compressedSize));

    // Read compressed data
    if (m_isMemory) {
        size_t offset = static_cast<size_t>(m_header.dataOffset + entry.dataOffset);
        if (offset + entry.compressedSize > m_memorySize) {
            LOG_ERROR(LogCategory::Core, "PackFileReader: Data read would exceed memory bounds");
            return {};
        }
        std::memcpy(data.data(), m_memoryData + offset, static_cast<size_t>(entry.compressedSize));
    } else {
        m_file->seekg(m_header.dataOffset + entry.dataOffset, std::ios::beg);
        m_file->read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(entry.compressedSize));
        if (!*m_file) {
            LOG_ERROR(LogCategory::Core, "PackFileReader: Failed to read file data: {}", path);
            return {};
        }
    }

    // Decompress if necessary
    if (entry.flags & static_cast<uint32_t>(PackEntryFlags::Compressed)) {
        
        auto decompressed = decompressData(data, static_cast<size_t>(entry.uncompressedSize));
        if (decompressed.empty()) {
            LOG_ERROR(LogCategory::Core, "PackFileReader: Decompression returned empty for {}", path);
        }
        return decompressed;
    }

    return data;
}

std::string PackFileReader::readFileAsString(const std::string& path) const {
    std::vector<uint8_t> data = readFile(path);
    if (data.empty()) {
        return {};
    }
    return std::string(reinterpret_cast<const char*>(data.data()), data.size());
}

const PackEntry* PackFileReader::getEntry(const std::string& path) const {
    std::string normalizedPath = path;
    std::replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');

    auto it = m_entries.find(normalizedPath);
    if (it != m_entries.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<uint8_t> PackFileReader::decompressData(const std::vector<uint8_t>& compressed, size_t uncompressedSize) const {
    std::vector<uint8_t> decompressed;
    decompressed.resize(uncompressedSize);

    z_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    stream.avail_in = static_cast<uInt>(compressed.size());
    stream.next_in = const_cast<Bytef*>(compressed.data());
    stream.avail_out = static_cast<uInt>(uncompressedSize);
    stream.next_out = decompressed.data();

    // Use inflateInit2 with windowBits=15+32 to auto-detect zlib/gzip format
    // Python's zlib.compress() produces zlib-wrapped data (2-byte header + deflate + 4-byte checksum)
    // inflateInit() only handles raw deflate, so we need inflateInit2 with proper windowBits
    // windowBits = 15 (max window size) + 32 (enable zlib/gzip auto-detection)
    if (inflateInit2(&stream, 15 + 32) != Z_OK) {
        LOG_ERROR(LogCategory::Core, "PackFileReader: Failed to initialize decompression");
        return {};
    }

    int result = inflate(&stream, Z_FINISH);
    inflateEnd(&stream);

    if (result != Z_STREAM_END) {
        LOG_ERROR(LogCategory::Core, "PackFileReader: Decompression failed: {} (avail_in={}, avail_out={})",
                  result, stream.avail_in, stream.avail_out);
        return {};
    }

    return decompressed;
}

// =============================================================================
// PackFileWriter Implementation
// =============================================================================

PackFileWriter::PackFileWriter() {
    initCRC32Table();
}

PackFileWriter::~PackFileWriter() {
    if (m_file && m_file->is_open()) {
        // Force finalize if not already done
        finalize();
    }
}

bool PackFileWriter::create(const std::string& path) {
    m_file = std::make_unique<std::ofstream>(path, std::ios::binary);
    if (!m_file->is_open()) {
        LOG_ERROR(LogCategory::Core, "PackFileWriter: Failed to create pack file: {}", path);
        return false;
    }

    m_path = path;
    m_entries.clear();
    m_pendingData.clear();
    m_currentDataOffset = 0;

    // Write placeholder header (will be updated in finalize)
    PackHeader header;
    m_file->write(reinterpret_cast<const char*>(&header), sizeof(PackHeader));

    return true;
}

bool PackFileWriter::addFile(const std::string& packPath, const std::string& sourcePath) {
    auto result = FileSystem::ReadBinaryFile(sourcePath);
    if (!result.success) {
        LOG_ERROR(LogCategory::Core, "PackFileWriter: Failed to read source file: {}", sourcePath);
        return false;
    }

    return addData(packPath, result.data);
}

bool PackFileWriter::addData(const std::string& packPath, const std::vector<uint8_t>& data) {
    if (!m_file || !m_file->is_open()) {
        LOG_ERROR(LogCategory::Core, "PackFileWriter: Pack file not open");
        return false;
    }

    PackEntry entry;
    entry.path = packPath;
    std::replace(entry.path.begin(), entry.path.end(), '\\', '/');
    entry.uncompressedSize = data.size();
    entry.crc32 = calculateCRC32(data);
    entry.dataOffset = m_currentDataOffset;

    std::vector<uint8_t> writeData;
    if (m_compression && data.size() > 64) {
        writeData = compressData(data);
        if (writeData.size() < data.size()) {
            entry.flags |= static_cast<uint32_t>(PackEntryFlags::Compressed);
            entry.compressedSize = writeData.size();
        } else {
            // Compression didn't help, store raw
            writeData = data;
            entry.compressedSize = data.size();
        }
    } else {
        writeData = data;
        entry.compressedSize = data.size();
    }

    m_entries.push_back(entry);
    m_pendingData.push_back(std::move(writeData));
    m_currentDataOffset += entry.compressedSize;

    return true;
}

bool PackFileWriter::addString(const std::string& packPath, const std::string& content) {
    std::vector<uint8_t> data(content.begin(), content.end());
    return addData(packPath, data);
}

bool PackFileWriter::finalize() {
    if (!m_file || !m_file->is_open()) {
        return false;
    }

    // Write data section
    uint64_t dataOffset = static_cast<uint64_t>(m_file->tellp());
    for (const auto& data : m_pendingData) {
        m_file->write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    }

    // Write index section
    uint64_t indexOffset = static_cast<uint64_t>(m_file->tellp());
    for (const auto& entry : m_entries) {
        uint32_t pathLength = static_cast<uint32_t>(entry.path.size());
        m_file->write(reinterpret_cast<const char*>(&pathLength), sizeof(uint32_t));
        m_file->write(entry.path.data(), pathLength);
        m_file->write(reinterpret_cast<const char*>(&entry.dataOffset), sizeof(uint64_t));
        m_file->write(reinterpret_cast<const char*>(&entry.compressedSize), sizeof(uint64_t));
        m_file->write(reinterpret_cast<const char*>(&entry.uncompressedSize), sizeof(uint64_t));
        m_file->write(reinterpret_cast<const char*>(&entry.flags), sizeof(uint32_t));
        m_file->write(reinterpret_cast<const char*>(&entry.crc32), sizeof(uint32_t));
    }

    // Update header
    PackHeader header;
    header.magic = PACK_MAGIC;
    header.version = PACK_VERSION;
    header.flags = 0;
    header.fileCount = static_cast<uint32_t>(m_entries.size());
    header.indexOffset = indexOffset;
    header.dataOffset = dataOffset;

    m_file->seekp(0, std::ios::beg);
    m_file->write(reinterpret_cast<const char*>(&header), sizeof(PackHeader));

    m_file->close();
    m_file.reset();

    m_entries.clear();
    m_pendingData.clear();

    return true;
}

std::vector<uint8_t> PackFileWriter::compressData(const std::vector<uint8_t>& data) const {
    uLongf compressedSize = compressBound(static_cast<uLong>(data.size()));
    std::vector<uint8_t> compressed(compressedSize);

    int result = compress2(compressed.data(), &compressedSize,
                          data.data(), static_cast<uLong>(data.size()),
                          Z_BEST_COMPRESSION);

    if (result != Z_OK) {
        
        return data;
    }

    compressed.resize(compressedSize);
    return compressed;
}

uint32_t PackFileWriter::calculateCRC32(const std::vector<uint8_t>& data) const {
    uint32_t crc = 0xFFFFFFFF;
    for (uint8_t byte : data) {
        crc = s_crc32Table[(crc ^ byte) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

// =============================================================================
// PackFileSystem Implementation
// =============================================================================

namespace {

/**
 * True when a path can name a pack entry: either a bare relative path or an explicit
 * res:// path. Any other virtual scheme (user://, temp://, app://) names storage that
 * lives outside the pack and can only ever miss here.
 */
bool IsPackScheme(const std::string& unixPath) {
    const size_t schemeEnd = unixPath.find("://");
    if (schemeEnd == std::string::npos) {
        return true;
    }
    return unixPath.compare(0, schemeEnd, "res") == 0;
}

} // namespace

PackFileSystem& PackFileSystem::Instance() {
    static PackFileSystem instance;
    return instance;
}

bool PackFileSystem::mountPack(const std::string& packPath, int priority) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto reader = std::make_unique<PackFileReader>();
    if (!reader->open(packPath)) {
        return false;
    }

    MountedPack pack;
    pack.reader = std::move(reader);
    pack.name = packPath;
    pack.priority = priority;

    // Insert sorted by priority (highest first)
    auto insertPos = std::lower_bound(m_packs.begin(), m_packs.end(), pack,
        [](const MountedPack& a, const MountedPack& b) {
            return a.priority > b.priority;
        });

    m_packs.insert(insertPos, std::move(pack));

    return true;
}

bool PackFileSystem::mountPackFromMemory(const uint8_t* data, size_t size, const std::string& name, int priority) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto reader = std::make_unique<PackFileReader>();
    if (!reader->openFromMemory(data, size)) {
        return false;
    }

    MountedPack pack;
    pack.reader = std::move(reader);
    pack.name = name;
    pack.priority = priority;

    auto insertPos = std::lower_bound(m_packs.begin(), m_packs.end(), pack,
        [](const MountedPack& a, const MountedPack& b) {
            return a.priority > b.priority;
        });

    m_packs.insert(insertPos, std::move(pack));

    return true;
}

void PackFileSystem::unmountPack(const std::string& packPath) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = std::find_if(m_packs.begin(), m_packs.end(),
        [&packPath](const MountedPack& pack) {
            return pack.name == packPath;
        });

    if (it != m_packs.end()) {
        m_packs.erase(it);
        
    }
}

std::string PackFileSystem::normalizePath(const std::string& path) const {
    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');

    // Strip res:// prefix (editor uses this, pack files don't)
    if (normalized.size() >= 6 && normalized.substr(0, 6) == "res://") {
        normalized = normalized.substr(6);
    }

    // Remove leading ./
    if (normalized.size() >= 2 && normalized[0] == '.' && normalized[1] == '/') {
        normalized = normalized.substr(2);
    }

    // Remove leading /
    while (!normalized.empty() && normalized[0] == '/') {
        normalized = normalized.substr(1);
    }

    return normalized;
}

PackFileSystem::PathResolution PackFileSystem::resolvePath(const std::string& path) const {
    PathResolution resolution;

    if (path.empty()) {
        return resolution;
    }

    const std::string unixPath = Path::ToUnixSeparators(path);

    // A pack only ever holds res:// content. The VirtualFileSystem still probes the pack
    // for every path it is handed, including the writable user:// space, so the other
    // schemes have to resolve to a clean miss. Without this the "user:" segment reads as
    // a drive letter to the sandbox check below and every save-slot probe is reported as
    // an attempted path traversal.
    if (!IsPackScheme(unixPath)) {
        return resolution;
    }

    // An absolute path is never a pack key (pack entries are stored relative). It is
    // also outside any configured asset root by definition, so it is only honored when
    // no base path is set - there is then no root for it to escape.
    if (Path::IsAbsolute(unixPath)) {
        if (!m_basePath.empty()) {
            LOG_ERROR(LogCategory::Core,
                      "PackFileSystem: rejected absolute path outside the asset root: {}", path);
            return resolution;
        }

        resolution.valid = true;
        resolution.hasPhysical = true;
        resolution.physical = path;
        return resolution;
    }

    const std::string stripped = normalizePath(unixPath);

    std::string relative;
    if (!Path::NormalizeSandboxRelative(stripped, relative)) {
        LOG_ERROR(LogCategory::Core,
                  "PackFileSystem: rejected path traversal outside the asset root: {}", path);
        return resolution;
    }

    resolution.valid = true;
    resolution.hasRelative = true;
    resolution.relative = relative;

    if (m_basePath.empty()) {
        resolution.physical = relative;
        resolution.hasPhysical = !relative.empty();
        return resolution;
    }

    const std::string joined = Path::JoinSandboxed(m_basePath, relative);
    if (joined.empty()) {
        LOG_ERROR(LogCategory::Core,
                  "PackFileSystem: rejected path outside the base path '{}': {}", m_basePath, path);
        resolution.valid = false;
        resolution.hasRelative = false;
        resolution.relative.clear();
        return resolution;
    }

    resolution.physical = joined;
    resolution.hasPhysical = true;
    return resolution;
}

bool PackFileSystem::exists(const std::string& path) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    const PathResolution resolution = resolvePath(path);
    if (!resolution.valid) {
        return false;
    }

    // Check pack files first
    if (resolution.hasRelative && !resolution.relative.empty()) {
        for (const auto& pack : m_packs) {
            if (pack.reader->exists(resolution.relative)) {
                return true;
            }
        }
    }

    // Fallback to filesystem
    if (m_filesystemFallback && resolution.hasPhysical) {
        return FileSystem::Exists(resolution.physical);
    }

    return false;
}

bool PackFileSystem::getFileSize(const std::string& path, uint64_t& outSize) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    const PathResolution resolution = resolvePath(path);
    if (!resolution.valid) {
        return false;
    }

    // Packs are ordered by descending priority, so the first entry that matches is the one
    // readFile would return - the size has to come from that same entry.
    if (resolution.hasRelative && !resolution.relative.empty()) {
        for (const auto& pack : m_packs) {
            const PackEntry* entry = pack.reader->getEntry(resolution.relative);
            if (entry) {
                outSize = entry->uncompressedSize;
                return true;
            }
        }
    }

    // Fallback to filesystem
    if (m_filesystemFallback && resolution.hasPhysical) {
        FileResult<uint64_t> result = FileSystem::GetFileSize(resolution.physical);
        if (result.success) {
            outSize = result.data;
            return true;
        }
    }

    return false;
}

std::vector<uint8_t> PackFileSystem::readFile(const std::string& path) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    const PathResolution resolution = resolvePath(path);
    if (!resolution.valid) {
        return {};
    }

    // Check pack files first
    if (resolution.hasRelative && !resolution.relative.empty()) {
        for (const auto& pack : m_packs) {
            if (pack.reader->exists(resolution.relative)) {
                return pack.reader->readFile(resolution.relative);
            }
        }
    }

    // Fallback to filesystem
    if (m_filesystemFallback && resolution.hasPhysical) {
        auto result = FileSystem::ReadBinaryFile(resolution.physical);
        if (result.success) {
            return result.data;
        }
    }

    return {};
}

std::string PackFileSystem::readFileAsString(const std::string& path) const {
    std::vector<uint8_t> data = readFile(path);
    if (data.empty()) {
        return {};
    }
    return std::string(reinterpret_cast<const char*>(data.data()), data.size());
}

std::vector<std::string> PackFileSystem::listDirectory(const std::string& dir) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    const PathResolution resolution = resolvePath(dir.empty() ? std::string(".") : dir);
    if (!resolution.valid) {
        return {};
    }

    const std::string normalizedDir = resolution.relative;
    std::unordered_map<std::string, bool> uniqueFiles;

    // Collect from pack files
    if (resolution.hasRelative) {
        for (const auto& pack : m_packs) {
            auto files = pack.reader->listDirectory(normalizedDir);
            for (const auto& file : files) {
                uniqueFiles[file] = true;
            }
        }
    }

    // Fallback to filesystem
    if (m_filesystemFallback && resolution.hasPhysical) {
        const std::string& fullPath = resolution.physical;
        auto result = FileSystem::ListDirectory(fullPath, false);
        if (result.success) {
            for (const auto& file : result.data) {
                // Convert to relative path
                std::string relativePath = file;
                if (!m_basePath.empty() && relativePath.find(m_basePath) == 0) {
                    relativePath = relativePath.substr(m_basePath.size());
                    if (!relativePath.empty() && (relativePath[0] == '/' || relativePath[0] == '\\')) {
                        relativePath = relativePath.substr(1);
                    }
                }
                uniqueFiles[relativePath] = true;
            }
        }
    }

    std::vector<std::string> result;
    result.reserve(uniqueFiles.size());
    for (const auto& [path, _] : uniqueFiles) {
        result.push_back(path);
    }

    return result;
}

std::vector<std::string> PackFileSystem::listFilesRecursive(const std::string& dir) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    const PathResolution resolution = resolvePath(dir.empty() ? std::string(".") : dir);
    if (!resolution.valid) {
        return {};
    }

    std::string normalizedDir = resolution.relative;
    if (!normalizedDir.empty() && normalizedDir.back() != '/') {
        normalizedDir += '/';
    }

    std::unordered_map<std::string, bool> uniqueFiles;

    // Collect every pack entry whose path is under the directory prefix.
    if (resolution.hasRelative) {
        for (const auto& pack : m_packs) {
            std::vector<std::string> files = pack.reader->listFiles();
            for (const std::string& file : files) {
                if (normalizedDir.empty() ||
                    file.compare(0, normalizedDir.size(), normalizedDir) == 0) {
                    uniqueFiles[file] = true;
                }
            }
        }
    }

    // Fallback to filesystem (recursive).
    if (m_filesystemFallback && resolution.hasPhysical) {
        const std::string& fullPath = resolution.physical;
        auto result = FileSystem::ListDirectory(fullPath, true);
        if (result.success) {
            for (const std::string& file : result.data) {
                std::string relativePath = file;
                std::replace(relativePath.begin(), relativePath.end(), '\\', '/');
                if (!m_basePath.empty()) {
                    std::string normBase = m_basePath;
                    std::replace(normBase.begin(), normBase.end(), '\\', '/');
                    if (relativePath.find(normBase) == 0) {
                        relativePath = relativePath.substr(normBase.size());
                        while (!relativePath.empty() && relativePath[0] == '/') {
                            relativePath = relativePath.substr(1);
                        }
                    }
                }
                uniqueFiles[relativePath] = true;
            }
        }
    }

    std::vector<std::string> result;
    result.reserve(uniqueFiles.size());
    for (const auto& [path, _] : uniqueFiles) {
        result.push_back(path);
    }

    return result;
}

// ============================================================================
// UUID-based Asset Resolution
// ============================================================================

bool PackFileSystem::isUUIDString(const std::string& str) const {
    if (str.size() != 16) return false;
    for (char c : str) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    return true;
}

std::string PackFileSystem::getPathForUUID(const std::string& uuidStr) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_uuidToPath.find(uuidStr);
    if (it != m_uuidToPath.end()) {
        return it->second;
    }
    return "";
}

std::string PackFileSystem::resolveAsset(const std::string& pathOrUUID) const {
    if (pathOrUUID.empty()) {
        return "";
    }

    std::string candidate = pathOrUUID;

    // Try UUID resolution first. The mapping table comes from the pack file, so its
    // values are untrusted too and are validated below along with plain paths.
    if (isUUIDString(pathOrUUID)) {
        std::string mapped = getPathForUUID(pathOrUUID);
        if (!mapped.empty()) {
            candidate = mapped;
        }
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    const PathResolution resolution = resolvePath(candidate);
    if (!resolution.valid) {
        return "";
    }

    if (resolution.hasRelative) {
        return resolution.relative;
    }

    return resolution.physical;
}

bool PackFileSystem::assetExists(const std::string& pathOrUUID) const {
    std::string resolved = resolveAsset(pathOrUUID);
    return exists(resolved);
}

std::vector<uint8_t> PackFileSystem::readAsset(const std::string& pathOrUUID) const {
    std::string resolved = resolveAsset(pathOrUUID);
    return readFile(resolved);
}

void PackFileSystem::loadUUIDMappings() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_uuidMappingsLoaded) {
        return;
    }

    m_uuidMappingsLoaded = true;

    // Try to read UUID mappings from pack file
    std::string normalizedPath = normalizePath(PACK_UUID_MAPPINGS_FILE);

    for (const auto& pack : m_packs) {
        if (pack.reader->exists(normalizedPath)) {
            std::vector<uint8_t> data = pack.reader->readFile(normalizedPath);
            if (!data.empty()) {
                try {
                    std::string jsonStr(reinterpret_cast<const char*>(data.data()), data.size());
                    nlohmann::json j = nlohmann::json::parse(jsonStr);

                    if (j.contains("mappings") && j["mappings"].is_array()) {
                        for (const auto& mapping : j["mappings"]) {
                            if (mapping.contains("uuid") && mapping.contains("path")) {
                                std::string uuid = mapping["uuid"].get<std::string>();
                                std::string path = mapping["path"].get<std::string>();
                                m_uuidToPath[uuid] = path;
                            }
                        }
                    }

                    return;
                } catch (const std::exception& e) {
                    LOG_ERROR(LogCategory::Core, "Failed to parse UUID mappings: {}", e.what());
                }
            }
        }
    }

    LOG_WARN(LogCategory::Core, "No UUID mappings found in pack files");
}

} // namespace platform
} // namespace lupine
