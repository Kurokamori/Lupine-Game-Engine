#include "lupine/save/SaveGameManager.hpp"
#include "lupine/platform/VirtualFileSystem.hpp"
#include "lupine/platform/FileSystem.hpp"
#include "lupine/logger/Logger.hpp"

#include <algorithm>
#include <ctime>

namespace lupine {
namespace save {

namespace {

// Envelope identity and header keys.
constexpr const char* kMagic = "LUPINE_SAVE";
constexpr int kEnvelopeVersion = 1;

constexpr const char* kKeyMagic = "magic";
constexpr const char* kKeyEnvelopeVersion = "envelope_version";
constexpr const char* kKeySchemaVersion = "schema_version";
constexpr const char* kKeyFormat = "format";
constexpr const char* kKeyTimestamp = "timestamp_unix";
constexpr const char* kKeyPlaytime = "playtime_seconds";
constexpr const char* kKeyTitle = "title";
constexpr const char* kKeyThumbnail = "thumbnail";
constexpr const char* kKeyMetadata = "metadata";
constexpr const char* kKeyData = "data";

double CurrentUnixTime() {
    return static_cast<double>(std::time(nullptr));
}

// Trim leading/trailing '/' from a virtual directory while preserving the scheme
// (e.g. "user://saves/" -> "user://saves").
std::string NormalizeDirectory(const std::string& dir) {
    std::string result = dir;
    while (!result.empty() && (result.back() == '/' || result.back() == '\\')) {
        result.pop_back();
    }
    return result;
}

std::string BaseName(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos) {
        return path;
    }
    return path.substr(pos + 1);
}

bool EndsWith(const std::string& value, const std::string& suffix) {
    if (suffix.size() > value.size()) {
        return false;
    }
    return std::equal(suffix.rbegin(), suffix.rend(), value.rbegin());
}

} // namespace

SaveGameManager::SaveGameManager()
    : m_SaveDirectory("user://saves"),
      m_Extension(".save"),
      m_SchemaVersion(1),
      m_Format(CreateSaveFormat(SaveFormatType::JsonText)),
      m_Transform(nullptr),
      m_BackupOnWrite(true),
      m_AtomicWrite(true),
      m_QuickSaveSlot("quicksave"),
      m_AutoSaveSlot("autosave") {}

SaveGameManager& SaveGameManager::GetInstance() {
    static SaveGameManager instance;
    return instance;
}

void SaveGameManager::SetSaveDirectory(const std::string& virtualDir) {
    m_SaveDirectory = NormalizeDirectory(virtualDir);
    if (m_SaveDirectory.empty()) {
        m_SaveDirectory = "user://saves";
    }
}

void SaveGameManager::SetExtension(const std::string& ext) {
    if (ext.empty()) {
        m_Extension = ".save";
        return;
    }
    m_Extension = (ext.front() == '.') ? ext : ("." + ext);
}

void SaveGameManager::SetSchemaVersion(int version) {
    m_SchemaVersion = version < 0 ? 0 : version;
}

void SaveGameManager::SetFormatType(SaveFormatType type) {
    m_Format = CreateSaveFormat(type);
}

void SaveGameManager::SetFormat(std::shared_ptr<ISaveFormat> format) {
    if (format) {
        m_Format = std::move(format);
    }
}

void SaveGameManager::SetTransform(std::shared_ptr<ISaveTransform> transform) {
    m_Transform = std::move(transform);
}

void SaveGameManager::SetQuickSaveSlot(const std::string& slot) {
    std::string clean;
    if (SanitizeSlot(slot, clean)) {
        m_QuickSaveSlot = clean;
    }
}

void SaveGameManager::SetAutoSaveSlot(const std::string& slot) {
    std::string clean;
    if (SanitizeSlot(slot, clean)) {
        m_AutoSaveSlot = clean;
    }
}

bool SaveGameManager::SanitizeSlot(const std::string& slot, std::string& outClean) const {
    outClean.clear();
    if (slot.empty()) {
        return false;
    }
    if (slot.find("..") != std::string::npos) {
        return false;
    }
    for (char c : slot) {
        if (c == '/' || c == '\\' || c == ':') {
            return false;
        }
    }
    outClean = slot;
    return true;
}

std::string SaveGameManager::ResolveSlotPath(const std::string& slot) const {
    std::string clean;
    if (!SanitizeSlot(slot, clean)) {
        return std::string();
    }
    return m_SaveDirectory + "/" + clean + m_Extension;
}

std::string SaveGameManager::ResolveBackupPath(const std::string& slot) const {
    std::string clean;
    if (!SanitizeSlot(slot, clean)) {
        return std::string();
    }
    return m_SaveDirectory + "/" + clean + m_Extension + ".bak";
}

nlohmann::json SaveGameManager::BuildEnvelope(const SaveData& data, const SaveSlotMeta& meta) const {
    nlohmann::json envelope = nlohmann::json::object();
    envelope[kKeyMagic] = kMagic;
    envelope[kKeyEnvelopeVersion] = kEnvelopeVersion;
    envelope[kKeySchemaVersion] = m_SchemaVersion;
    envelope[kKeyFormat] = SaveFormatTypeToString(m_Format ? m_Format->Type() : SaveFormatType::JsonText);
    envelope[kKeyTimestamp] = CurrentUnixTime();
    envelope[kKeyPlaytime] = meta.playtimeSeconds;
    envelope[kKeyTitle] = meta.title;
    envelope[kKeyThumbnail] = meta.thumbnailPath;
    envelope[kKeyMetadata] = meta.metadata.is_null() ? nlohmann::json::object() : meta.metadata;
    envelope[kKeyData] = data.ToJson();
    return envelope;
}

bool SaveGameManager::DecodeEnvelope(const std::vector<uint8_t>& bytes, nlohmann::json& outEnvelope) const {
    auto tryDecode = [&](const ISaveFormat& format) -> bool {
        nlohmann::json candidate;
        if (format.Decode(bytes, candidate) && candidate.is_object() &&
            candidate.value(kKeyMagic, std::string()) == kMagic) {
            outEnvelope = std::move(candidate);
            return true;
        }
        return false;
    };

    if (m_Format && tryDecode(*m_Format)) {
        return true;
    }

    const SaveFormatType kAllFormats[] = {
        SaveFormatType::JsonText,
        SaveFormatType::JsonCompact,
        SaveFormatType::BinaryCbor,
        SaveFormatType::BinaryMsgpack
    };
    for (SaveFormatType type : kAllFormats) {
        std::shared_ptr<ISaveFormat> format = CreateSaveFormat(type);
        if (format && tryDecode(*format)) {
            return true;
        }
    }
    return false;
}

void SaveGameManager::FillSlotInfo(const nlohmann::json& envelope, SaveSlotInfo& outInfo) {
    outInfo.schemaVersion = envelope.value(kKeySchemaVersion, 0);
    outInfo.envelopeVersion = envelope.value(kKeyEnvelopeVersion, 0);
    outInfo.format = SaveFormatTypeFromString(envelope.value(kKeyFormat, std::string("json_text")));
    outInfo.timestampUnix = envelope.value(kKeyTimestamp, 0.0);
    outInfo.playtimeSeconds = envelope.value(kKeyPlaytime, 0.0);
    outInfo.title = envelope.value(kKeyTitle, std::string());
    outInfo.thumbnailPath = envelope.value(kKeyThumbnail, std::string());
    if (envelope.contains(kKeyMetadata) && envelope[kKeyMetadata].is_object()) {
        outInfo.metadata = envelope[kKeyMetadata];
    } else {
        outInfo.metadata = nlohmann::json::object();
    }
}

SaveResult SaveGameManager::Save(const std::string& slot, const SaveData& data, const SaveSlotMeta& meta) {
    std::string clean;
    if (!SanitizeSlot(slot, clean)) {
        return SaveResult::InvalidArgument;
    }

    SaveData working = data;
    if (m_PreSaveHook) {
        m_PreSaveHook(working);
    }

    nlohmann::json envelope = BuildEnvelope(working, meta);

    std::vector<uint8_t> encoded;
    if (!m_Format || !m_Format->Encode(envelope, encoded)) {
        LOG_ERROR(LogCategory::Core, "SaveGameManager: failed to encode slot '{}'", clean);
        return SaveResult::WriteFailed;
    }

    std::vector<uint8_t> bytes;
    if (m_Transform) {
        if (!m_Transform->Apply(encoded, bytes)) {
            LOG_ERROR(LogCategory::Core, "SaveGameManager: transform failed for slot '{}'", clean);
            return SaveResult::WriteFailed;
        }
    } else {
        bytes = std::move(encoded);
    }

    auto& vfs = platform::VirtualFileSystem::GetInstance();
    vfs.CreateDirectory(m_SaveDirectory, true);

    const std::string target = m_SaveDirectory + "/" + clean + m_Extension;

    if (m_AtomicWrite) {
        const std::string tmp = target + ".tmp";
        auto written = vfs.WriteBinaryFile(tmp, bytes);
        if (!written.success) {
            LOG_ERROR(LogCategory::Core, "SaveGameManager: write failed for '{}': {}", tmp, written.error);
            return SaveResult::WriteFailed;
        }

        if (m_BackupOnWrite && vfs.Exists(target)) {
            vfs.CopyFile(target, target + ".bak", true);
        }
        if (vfs.Exists(target)) {
            vfs.DeleteFile(target);
        }

        bool moved = false;
        auto tmpOs = vfs.ResolvePath(tmp);
        auto dstOs = vfs.ResolvePath(target);
        if (tmpOs.success && dstOs.success) {
            auto mv = platform::FileSystem::MoveFile(tmpOs.data, dstOs.data);
            moved = mv.success;
        }
        if (!moved) {
            auto copied = vfs.CopyFile(tmp, target, true);
            vfs.DeleteFile(tmp);
            if (!copied.success) {
                LOG_ERROR(LogCategory::Core, "SaveGameManager: finalize failed for slot '{}'", clean);
                return SaveResult::WriteFailed;
            }
        }
    } else {
        if (m_BackupOnWrite && vfs.Exists(target)) {
            vfs.CopyFile(target, target + ".bak", true);
        }
        auto written = vfs.WriteBinaryFile(target, bytes);
        if (!written.success) {
            LOG_ERROR(LogCategory::Core, "SaveGameManager: write failed for '{}': {}", target, written.error);
            return SaveResult::WriteFailed;
        }
    }

    return SaveResult::Success;
}

SaveResult SaveGameManager::Load(const std::string& slot, SaveData& outData) const {
    SaveSlotInfo info;
    return Load(slot, outData, info);
}

SaveResult SaveGameManager::Load(const std::string& slot, SaveData& outData, SaveSlotInfo& outInfo) const {
    std::string clean;
    if (!SanitizeSlot(slot, clean)) {
        return SaveResult::InvalidArgument;
    }

    auto& vfs = platform::VirtualFileSystem::GetInstance();
    const std::string target = m_SaveDirectory + "/" + clean + m_Extension;
    if (!vfs.Exists(target)) {
        return SaveResult::SlotNotFound;
    }

    auto read = vfs.ReadBinaryFile(target);
    if (!read.success) {
        return SaveResult::ReadFailed;
    }

    std::vector<uint8_t> decoded;
    if (m_Transform) {
        if (!m_Transform->Revert(read.data, decoded)) {
            return SaveResult::ParseError;
        }
    } else {
        decoded = std::move(read.data);
    }

    nlohmann::json envelope;
    if (!DecodeEnvelope(decoded, envelope)) {
        return SaveResult::ParseError;
    }

    const int fileSchema = envelope.value(kKeySchemaVersion, 0);
    if (fileSchema > m_SchemaVersion) {
        return SaveResult::VersionTooNew;
    }

    nlohmann::json payload = nlohmann::json::object();
    if (envelope.contains(kKeyData)) {
        payload = envelope[kKeyData];
    }

    if (fileSchema < m_SchemaVersion) {
        std::string error;
        if (!m_Migrations.Migrate(payload, fileSchema, m_SchemaVersion, error)) {
            LOG_ERROR(LogCategory::Core, "SaveGameManager: migration failed for slot '{}': {}", clean, error);
            return SaveResult::MigrationFailed;
        }
    }

    outData = SaveData::FromJson(payload);
    if (m_PostLoadHook) {
        m_PostLoadHook(outData);
    }

    outInfo = SaveSlotInfo();
    outInfo.slot = clean;
    outInfo.path = target;
    outInfo.exists = true;
    FillSlotInfo(envelope, outInfo);
    auto size = vfs.GetFileSize(target);
    outInfo.payloadBytes = size.success ? size.data : 0;

    return SaveResult::Success;
}

bool SaveGameManager::SlotExists(const std::string& slot) const {
    std::string path = ResolveSlotPath(slot);
    if (path.empty()) {
        return false;
    }
    return platform::VirtualFileSystem::GetInstance().Exists(path);
}

std::vector<std::string> SaveGameManager::ListSlots() const {
    std::vector<std::string> slots;
    auto& vfs = platform::VirtualFileSystem::GetInstance();
    if (!vfs.Exists(m_SaveDirectory)) {
        return slots;
    }

    auto listing = vfs.ListDirectory(m_SaveDirectory, false);
    if (!listing.success) {
        return slots;
    }

    for (const std::string& entry : listing.data) {
        std::string name = BaseName(entry);
        if (!EndsWith(name, m_Extension)) {
            continue;
        }
        std::string stem = name.substr(0, name.size() - m_Extension.size());
        if (stem.empty()) {
            continue;
        }
        slots.push_back(stem);
    }

    std::sort(slots.begin(), slots.end());
    return slots;
}

std::vector<SaveSlotInfo> SaveGameManager::ListSlotInfos() const {
    std::vector<SaveSlotInfo> infos;
    for (const std::string& slot : ListSlots()) {
        SaveSlotInfo info;
        if (GetSlotInfo(slot, info) == SaveResult::Success) {
            infos.push_back(std::move(info));
        }
    }
    std::sort(infos.begin(), infos.end(), [](const SaveSlotInfo& a, const SaveSlotInfo& b) {
        return a.timestampUnix > b.timestampUnix;
    });
    return infos;
}

SaveResult SaveGameManager::GetSlotInfo(const std::string& slot, SaveSlotInfo& outInfo) const {
    std::string clean;
    if (!SanitizeSlot(slot, clean)) {
        return SaveResult::InvalidArgument;
    }

    auto& vfs = platform::VirtualFileSystem::GetInstance();
    const std::string target = m_SaveDirectory + "/" + clean + m_Extension;
    if (!vfs.Exists(target)) {
        return SaveResult::SlotNotFound;
    }

    auto read = vfs.ReadBinaryFile(target);
    if (!read.success) {
        return SaveResult::ReadFailed;
    }

    std::vector<uint8_t> decoded;
    if (m_Transform) {
        if (!m_Transform->Revert(read.data, decoded)) {
            return SaveResult::ParseError;
        }
    } else {
        decoded = std::move(read.data);
    }

    nlohmann::json envelope;
    if (!DecodeEnvelope(decoded, envelope)) {
        return SaveResult::ParseError;
    }

    outInfo = SaveSlotInfo();
    outInfo.slot = clean;
    outInfo.path = target;
    outInfo.exists = true;
    FillSlotInfo(envelope, outInfo);
    auto size = vfs.GetFileSize(target);
    outInfo.payloadBytes = size.success ? size.data : 0;
    return SaveResult::Success;
}

SaveResult SaveGameManager::DeleteSlot(const std::string& slot) {
    std::string clean;
    if (!SanitizeSlot(slot, clean)) {
        return SaveResult::InvalidArgument;
    }
    auto& vfs = platform::VirtualFileSystem::GetInstance();
    const std::string target = m_SaveDirectory + "/" + clean + m_Extension;
    if (!vfs.Exists(target)) {
        return SaveResult::SlotNotFound;
    }
    auto deleted = vfs.DeleteFile(target);
    const std::string backup = target + ".bak";
    if (vfs.Exists(backup)) {
        vfs.DeleteFile(backup);
    }
    return deleted.success ? SaveResult::Success : SaveResult::WriteFailed;
}

SaveResult SaveGameManager::CopySlot(const std::string& fromSlot, const std::string& toSlot, bool overwrite) {
    std::string fromClean;
    std::string toClean;
    if (!SanitizeSlot(fromSlot, fromClean) || !SanitizeSlot(toSlot, toClean)) {
        return SaveResult::InvalidArgument;
    }
    auto& vfs = platform::VirtualFileSystem::GetInstance();
    const std::string source = m_SaveDirectory + "/" + fromClean + m_Extension;
    const std::string dest = m_SaveDirectory + "/" + toClean + m_Extension;
    if (!vfs.Exists(source)) {
        return SaveResult::SlotNotFound;
    }
    if (!overwrite && vfs.Exists(dest)) {
        return SaveResult::InvalidArgument;
    }
    auto copied = vfs.CopyFile(source, dest, true);
    return copied.success ? SaveResult::Success : SaveResult::WriteFailed;
}

SaveResult SaveGameManager::RenameSlot(const std::string& fromSlot, const std::string& toSlot, bool overwrite) {
    SaveResult copyResult = CopySlot(fromSlot, toSlot, overwrite);
    if (copyResult != SaveResult::Success) {
        return copyResult;
    }
    return DeleteSlot(fromSlot);
}

SaveResult SaveGameManager::QuickSave(const SaveData& data, const SaveSlotMeta& meta) {
    return Save(m_QuickSaveSlot, data, meta);
}

SaveResult SaveGameManager::QuickLoad(SaveData& outData) const {
    return Load(m_QuickSaveSlot, outData);
}

bool SaveGameManager::HasQuickSave() const {
    return SlotExists(m_QuickSaveSlot);
}

SaveResult SaveGameManager::AutoSave(const SaveData& data, const SaveSlotMeta& meta) {
    return Save(m_AutoSaveSlot, data, meta);
}

bool SaveGameManager::HasAutoSave() const {
    return SlotExists(m_AutoSaveSlot);
}

} // namespace save
} // namespace lupine
