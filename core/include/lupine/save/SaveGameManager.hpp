#pragma once

#include "lupine/save/SaveTypes.hpp"
#include "lupine/save/SaveData.hpp"
#include "lupine/save/SaveFormat.hpp"
#include "lupine/save/SaveMigration.hpp"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace lupine {
namespace save {

/**
 * SaveGameManager - the central save-game toolkit.
 *
 * A process-wide singleton that reads and writes named "slots" under a virtual
 * directory (default "user://saves"), layering schema versioning, migration, a
 * pluggable container format, an optional byte transform, crash-safe atomic
 * writes with backups, and a header that can be read for a load menu without
 * deserializing the full payload.
 *
 * Everything is designed to be extended per game:
 *   - Store anything in a SaveData (free-form, typed, nested).
 *   - Replace the encoding with SetFormat / SetFormatType (JSON, CBOR, MessagePack
 *     or a custom ISaveFormat) and/or install an ISaveTransform for
 *     obfuscation / compression / encryption.
 *   - Register schema migrations so old saves keep loading after the data layout
 *     changes (see Migrations()).
 *   - Hook PreSave / PostLoad to inject or fix up data globally.
 *
 * The manager is intended to be driven from the main thread (it performs VFS I/O);
 * it is not internally synchronized.
 */
class SaveGameManager {
public:
    static SaveGameManager& GetInstance();

    SaveGameManager(const SaveGameManager&) = delete;
    SaveGameManager& operator=(const SaveGameManager&) = delete;

    // ------------------------------------------------------------------------
    // Configuration
    // ------------------------------------------------------------------------

    // Virtual directory that holds the slot files. Default "user://saves". Leading
    // and trailing slashes are normalized away; the directory is created on demand.
    void SetSaveDirectory(const std::string& virtualDir);
    const std::string& GetSaveDirectory() const { return m_SaveDirectory; }

    // File extension for slot files, including the leading dot. Default ".save".
    void SetExtension(const std::string& ext);
    const std::string& GetExtension() const { return m_Extension; }

    // The application's current data schema version, written into every save and
    // used as the migration target on load. Default 1.
    void SetSchemaVersion(int version);
    int GetSchemaVersion() const { return m_SchemaVersion; }

    // Container encoding. SetFormatType installs a built-in encoder; SetFormat
    // installs a custom one. The active format is used for writes; reads
    // auto-detect across all encodings, so changing this never orphans old saves.
    void SetFormatType(SaveFormatType type);
    void SetFormat(std::shared_ptr<ISaveFormat> format);
    std::shared_ptr<ISaveFormat> GetFormat() const { return m_Format; }

    // Optional byte transform applied after encode / before decode. Pass nullptr
    // to clear. The same transform must be installed to read a save written with
    // it (auto-detection covers the format, not the transform).
    void SetTransform(std::shared_ptr<ISaveTransform> transform);
    std::shared_ptr<ISaveTransform> GetTransform() const { return m_Transform; }

    // Write a ".bak" copy of the previous file before overwriting it. Default on.
    void SetBackupOnWrite(bool enabled) { m_BackupOnWrite = enabled; }
    bool GetBackupOnWrite() const { return m_BackupOnWrite; }

    // Write to a temp file then atomically move it into place, so an interrupted
    // write can never leave a half-written save. Default on.
    void SetAtomicWrite(bool enabled) { m_AtomicWrite = enabled; }
    bool GetAtomicWrite() const { return m_AtomicWrite; }

    // The schema-migration registry (mutable). Register upgrades here.
    SaveMigrationRegistry& Migrations() { return m_Migrations; }
    const SaveMigrationRegistry& Migrations() const { return m_Migrations; }

    // Global hooks. PreSave runs on a copy of the data just before encoding (e.g.
    // to stamp app-wide fields); PostLoad runs on the data just after a successful
    // load and migration. Pass an empty std::function to clear.
    void SetPreSaveHook(std::function<void(SaveData&)> hook) { m_PreSaveHook = std::move(hook); }
    void SetPostLoadHook(std::function<void(SaveData&)> hook) { m_PostLoadHook = std::move(hook); }

    // Slot names used by the QuickSave/AutoSave helpers. Defaults "quicksave" and
    // "autosave".
    void SetQuickSaveSlot(const std::string& slot);
    void SetAutoSaveSlot(const std::string& slot);
    const std::string& GetQuickSaveSlot() const { return m_QuickSaveSlot; }
    const std::string& GetAutoSaveSlot() const { return m_AutoSaveSlot; }

    // ------------------------------------------------------------------------
    // Path helpers
    // ------------------------------------------------------------------------

    // Resolved virtual path of a slot's main file / backup file. Empty if the slot
    // id is illegal.
    std::string ResolveSlotPath(const std::string& slot) const;
    std::string ResolveBackupPath(const std::string& slot) const;

    // ------------------------------------------------------------------------
    // Slot management
    // ------------------------------------------------------------------------

    bool SlotExists(const std::string& slot) const;

    // Logical slot ids found in the save directory (extension stripped), sorted.
    std::vector<std::string> ListSlots() const;

    // Headers for every slot, sorted by descending timestamp (newest first). Slots
    // whose header cannot be read are skipped.
    std::vector<SaveSlotInfo> ListSlotInfos() const;

    // Read a single slot's header without deserializing the full payload.
    SaveResult GetSlotInfo(const std::string& slot, SaveSlotInfo& outInfo) const;

    // Delete a slot's file (and its backup). SlotNotFound if it does not exist.
    SaveResult DeleteSlot(const std::string& slot);

    // Copy / rename a slot file. Fails with InvalidArgument if `toSlot` exists and
    // `overwrite` is false.
    SaveResult CopySlot(const std::string& fromSlot, const std::string& toSlot, bool overwrite = true);
    SaveResult RenameSlot(const std::string& fromSlot, const std::string& toSlot, bool overwrite = true);

    // ------------------------------------------------------------------------
    // Save / load
    // ------------------------------------------------------------------------

    SaveResult Save(const std::string& slot, const SaveData& data, const SaveSlotMeta& meta = SaveSlotMeta());
    SaveResult Load(const std::string& slot, SaveData& outData) const;
    SaveResult Load(const std::string& slot, SaveData& outData, SaveSlotInfo& outInfo) const;

    // Convenience wrappers over the configured quick/auto slots.
    SaveResult QuickSave(const SaveData& data, const SaveSlotMeta& meta = SaveSlotMeta());
    SaveResult QuickLoad(SaveData& outData) const;
    bool HasQuickSave() const;
    SaveResult AutoSave(const SaveData& data, const SaveSlotMeta& meta = SaveSlotMeta());
    bool HasAutoSave() const;

private:
    SaveGameManager();
    ~SaveGameManager() = default;

    // Validate and normalize a slot id (rejects empty ids and path-traversal /
    // separator characters). Returns false and leaves `outClean` empty on failure.
    bool SanitizeSlot(const std::string& slot, std::string& outClean) const;

    // Build the on-disk envelope (header + data payload) for a save.
    nlohmann::json BuildEnvelope(const SaveData& data, const SaveSlotMeta& meta) const;

    // Decode raw (post-transform) bytes into an envelope, trying the active format
    // first and then every built-in format. Validates the envelope magic.
    bool DecodeEnvelope(const std::vector<uint8_t>& bytes, nlohmann::json& outEnvelope) const;

    // Populate a SaveSlotInfo from a decoded envelope.
    static void FillSlotInfo(const nlohmann::json& envelope, SaveSlotInfo& outInfo);

    std::string m_SaveDirectory;
    std::string m_Extension;
    int m_SchemaVersion;
    std::shared_ptr<ISaveFormat> m_Format;
    std::shared_ptr<ISaveTransform> m_Transform;
    bool m_BackupOnWrite;
    bool m_AtomicWrite;
    SaveMigrationRegistry m_Migrations;
    std::function<void(SaveData&)> m_PreSaveHook;
    std::function<void(SaveData&)> m_PostLoadHook;
    std::string m_QuickSaveSlot;
    std::string m_AutoSaveSlot;
};

} // namespace save
} // namespace lupine
