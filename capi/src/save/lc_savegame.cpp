#include "save/lc_savegame.h"
#include "../core/lc_internal.h"

#include <lupine/save/SaveGameManager.hpp>
#include <lupine/save/SceneSaveState.hpp>
#include <lupine/core/SceneManager.hpp>

#include <nlohmann/json.hpp>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

using namespace lupine;

// Provided by lc_core.cpp.
void SetError(LCResult code, const char* message);

namespace {

std::unordered_map<LCSaveDataHandle, std::shared_ptr<save::SaveData>> g_SaveDataHandles;
std::mutex g_SaveDataMutex;
uint64_t g_NextSaveDataHandle = 1;

LCSaveDataHandle CreateSaveDataHandle(std::shared_ptr<save::SaveData> data) {
    std::lock_guard<std::mutex> lock(g_SaveDataMutex);
    LCSaveDataHandle handle = reinterpret_cast<LCSaveDataHandle>(g_NextSaveDataHandle++);
    g_SaveDataHandles[handle] = std::move(data);
    return handle;
}

std::shared_ptr<save::SaveData> GetSaveData(LCSaveDataHandle handle) {
    std::lock_guard<std::mutex> lock(g_SaveDataMutex);
    auto it = g_SaveDataHandles.find(handle);
    return it != g_SaveDataHandles.end() ? it->second : nullptr;
}

bool RemoveSaveDataHandle(LCSaveDataHandle handle) {
    std::lock_guard<std::mutex> lock(g_SaveDataMutex);
    return g_SaveDataHandles.erase(handle) > 0;
}

std::shared_ptr<save::SaveData> RequireSaveData(LCSaveDataHandle handle) {
    auto data = GetSaveData(handle);
    if (!data) {
        SetError(LC_ERROR_INVALID_HANDLE, "invalid SaveData handle");
    }
    return data;
}

LCResult WriteAllocatedString(const std::string& value, char** out_str) {
    if (!out_str) {
        SetError(LC_ERROR_NULL_POINTER, "output string pointer is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    char* buffer = DuplicateString(value);
    if (!buffer) {
        SetError(LC_ERROR_OUT_OF_MEMORY, "failed to allocate string result");
        return LC_ERROR_OUT_OF_MEMORY;
    }
    *out_str = buffer;
    return LC_SUCCESS;
}

// Most recent SaveResult produced by a manager operation, kept per-thread so
// lc_savegame_get_last_error can report it (SaveGameManager exposes no
// GetLastResult of its own). Updated wherever a SaveResult is converted.
thread_local save::SaveResult g_LastSaveResult = save::SaveResult::Success;

LCResult SaveResultToLc(save::SaveResult result) {
    g_LastSaveResult = result;
    switch (result) {
        case save::SaveResult::Success:         return LC_SUCCESS;
        case save::SaveResult::InvalidArgument: return LC_ERROR_INVALID_PARAMETER;
        case save::SaveResult::SlotNotFound:    return LC_ERROR_FILE_NOT_FOUND;
        case save::SaveResult::WriteFailed:     return LC_ERROR_FILE_WRITE_FAILED;
        case save::SaveResult::ReadFailed:      return LC_ERROR_FILE_READ_FAILED;
        case save::SaveResult::ParseError:      return LC_ERROR_FILE_INVALID_FORMAT;
        case save::SaveResult::FormatError:     return LC_ERROR_FILE_INVALID_FORMAT;
        case save::SaveResult::VersionTooNew:   return LC_ERROR_OPERATION_FAILED;
        case save::SaveResult::MigrationFailed: return LC_ERROR_OPERATION_FAILED;
    }
    return LC_ERROR_OPERATION_FAILED;
}

save::SaveSlotMeta ParseMeta(const char* meta_json) {
    save::SaveSlotMeta meta;
    if (!meta_json || meta_json[0] == '\0') {
        return meta;
    }
    try {
        nlohmann::json json = nlohmann::json::parse(meta_json);
        if (json.is_object()) {
            if (json.contains("title") && json["title"].is_string()) {
                meta.title = json["title"].get<std::string>();
            }
            if (json.contains("thumbnail") && json["thumbnail"].is_string()) {
                meta.thumbnailPath = json["thumbnail"].get<std::string>();
            }
            if (json.contains("playtime") && json["playtime"].is_number()) {
                meta.playtimeSeconds = json["playtime"].get<double>();
            }
            if (json.contains("metadata") && json["metadata"].is_object()) {
                meta.metadata = json["metadata"];
            }
        }
    } catch (const std::exception&) {
        // Ignore malformed meta; the save still proceeds with empty header fields.
    }
    return meta;
}

nlohmann::json SlotInfoToJson(const save::SaveSlotInfo& info) {
    nlohmann::json json = nlohmann::json::object();
    json["slot"] = info.slot;
    json["path"] = info.path;
    json["exists"] = info.exists;
    json["schema_version"] = info.schemaVersion;
    json["envelope_version"] = info.envelopeVersion;
    json["format"] = save::SaveFormatTypeToString(info.format);
    json["timestamp_unix"] = info.timestampUnix;
    json["playtime_seconds"] = info.playtimeSeconds;
    json["title"] = info.title;
    json["thumbnail"] = info.thumbnailPath;
    json["payload_bytes"] = info.payloadBytes;
    json["metadata"] = info.metadata;
    return json;
}

} // namespace

/* ============================================================================
 * Save document lifecycle
 * ============================================================================ */

LC_API LCResult lc_savedata_create(LCSaveDataHandle* out_handle) {
    if (!out_handle) {
        SetError(LC_ERROR_NULL_POINTER, "out_handle is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    *out_handle = CreateSaveDataHandle(std::make_shared<save::SaveData>());
    return LC_SUCCESS;
}

LC_API LCResult lc_savedata_create_from_json(const char* json_text, LCSaveDataHandle* out_handle) {
    if (!out_handle) {
        SetError(LC_ERROR_NULL_POINTER, "out_handle is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    bool ok = true;
    save::SaveData data = save::SaveData::Parse(json_text ? json_text : "{}", &ok);
    if (!ok) {
        SetError(LC_ERROR_FILE_INVALID_FORMAT, "failed to parse SaveData JSON");
        return LC_ERROR_FILE_INVALID_FORMAT;
    }
    *out_handle = CreateSaveDataHandle(std::make_shared<save::SaveData>(std::move(data)));
    return LC_SUCCESS;
}

LC_API LCResult lc_savedata_destroy(LCSaveDataHandle handle) {
    if (!RemoveSaveDataHandle(handle)) {
        SetError(LC_ERROR_INVALID_HANDLE, "invalid SaveData handle");
        return LC_ERROR_INVALID_HANDLE;
    }
    return LC_SUCCESS;
}

LC_API LCResult lc_savedata_clear(LCSaveDataHandle handle) {
    auto data = RequireSaveData(handle);
    if (!data) return LC_ERROR_INVALID_HANDLE;
    data->Clear();
    return LC_SUCCESS;
}

/* ============================================================================
 * Save document typed accessors
 * ============================================================================ */

LC_API LCResult lc_savedata_set_bool(LCSaveDataHandle handle, const char* key, int value) {
    auto data = RequireSaveData(handle);
    if (!data) return LC_ERROR_INVALID_HANDLE;
    if (!key) { SetError(LC_ERROR_NULL_POINTER, "key is NULL"); return LC_ERROR_NULL_POINTER; }
    data->SetBool(key, value != 0);
    return LC_SUCCESS;
}

LC_API LCResult lc_savedata_get_bool(LCSaveDataHandle handle, const char* key, int def, int* out_value) {
    auto data = RequireSaveData(handle);
    if (!data) return LC_ERROR_INVALID_HANDLE;
    if (!key || !out_value) { SetError(LC_ERROR_NULL_POINTER, "key or out_value is NULL"); return LC_ERROR_NULL_POINTER; }
    *out_value = data->GetBool(key, def != 0) ? 1 : 0;
    return LC_SUCCESS;
}

LC_API LCResult lc_savedata_set_int(LCSaveDataHandle handle, const char* key, int64_t value) {
    auto data = RequireSaveData(handle);
    if (!data) return LC_ERROR_INVALID_HANDLE;
    if (!key) { SetError(LC_ERROR_NULL_POINTER, "key is NULL"); return LC_ERROR_NULL_POINTER; }
    data->SetInt64(key, value);
    return LC_SUCCESS;
}

LC_API LCResult lc_savedata_get_int(LCSaveDataHandle handle, const char* key, int64_t def, int64_t* out_value) {
    auto data = RequireSaveData(handle);
    if (!data) return LC_ERROR_INVALID_HANDLE;
    if (!key || !out_value) { SetError(LC_ERROR_NULL_POINTER, "key or out_value is NULL"); return LC_ERROR_NULL_POINTER; }
    *out_value = data->GetInt64(key, def);
    return LC_SUCCESS;
}

LC_API LCResult lc_savedata_set_double(LCSaveDataHandle handle, const char* key, double value) {
    auto data = RequireSaveData(handle);
    if (!data) return LC_ERROR_INVALID_HANDLE;
    if (!key) { SetError(LC_ERROR_NULL_POINTER, "key is NULL"); return LC_ERROR_NULL_POINTER; }
    data->SetDouble(key, value);
    return LC_SUCCESS;
}

LC_API LCResult lc_savedata_get_double(LCSaveDataHandle handle, const char* key, double def, double* out_value) {
    auto data = RequireSaveData(handle);
    if (!data) return LC_ERROR_INVALID_HANDLE;
    if (!key || !out_value) { SetError(LC_ERROR_NULL_POINTER, "key or out_value is NULL"); return LC_ERROR_NULL_POINTER; }
    *out_value = data->GetDouble(key, def);
    return LC_SUCCESS;
}

LC_API LCResult lc_savedata_set_string(LCSaveDataHandle handle, const char* key, const char* value) {
    auto data = RequireSaveData(handle);
    if (!data) return LC_ERROR_INVALID_HANDLE;
    if (!key || !value) { SetError(LC_ERROR_NULL_POINTER, "key or value is NULL"); return LC_ERROR_NULL_POINTER; }
    data->SetString(key, value);
    return LC_SUCCESS;
}

LC_API LCResult lc_savedata_get_string(LCSaveDataHandle handle, const char* key, const char* def, char** out_value) {
    auto data = RequireSaveData(handle);
    if (!data) return LC_ERROR_INVALID_HANDLE;
    if (!key || !out_value) { SetError(LC_ERROR_NULL_POINTER, "key or out_value is NULL"); return LC_ERROR_NULL_POINTER; }
    return WriteAllocatedString(data->GetString(key, def ? def : ""), out_value);
}

LC_API LCResult lc_savedata_set_json(LCSaveDataHandle handle, const char* key, const char* json_text) {
    auto data = RequireSaveData(handle);
    if (!data) return LC_ERROR_INVALID_HANDLE;
    if (!key || !json_text) { SetError(LC_ERROR_NULL_POINTER, "key or json_text is NULL"); return LC_ERROR_NULL_POINTER; }
    try {
        data->SetJson(key, nlohmann::json::parse(json_text));
    } catch (const std::exception&) {
        SetError(LC_ERROR_FILE_INVALID_FORMAT, "failed to parse JSON value");
        return LC_ERROR_FILE_INVALID_FORMAT;
    }
    return LC_SUCCESS;
}

LC_API LCResult lc_savedata_get_json(LCSaveDataHandle handle, const char* key, char** out_json) {
    auto data = RequireSaveData(handle);
    if (!data) return LC_ERROR_INVALID_HANDLE;
    if (!key || !out_json) { SetError(LC_ERROR_NULL_POINTER, "key or out_json is NULL"); return LC_ERROR_NULL_POINTER; }
    return WriteAllocatedString(data->GetJson(key, nlohmann::json()).dump(), out_json);
}

LC_API LCResult lc_savedata_has(LCSaveDataHandle handle, const char* key, int* out_has) {
    auto data = RequireSaveData(handle);
    if (!data) return LC_ERROR_INVALID_HANDLE;
    if (!key || !out_has) { SetError(LC_ERROR_NULL_POINTER, "key or out_has is NULL"); return LC_ERROR_NULL_POINTER; }
    *out_has = data->Has(key) ? 1 : 0;
    return LC_SUCCESS;
}

LC_API LCResult lc_savedata_erase(LCSaveDataHandle handle, const char* key) {
    auto data = RequireSaveData(handle);
    if (!data) return LC_ERROR_INVALID_HANDLE;
    if (!key) { SetError(LC_ERROR_NULL_POINTER, "key is NULL"); return LC_ERROR_NULL_POINTER; }
    data->Erase(key);
    return LC_SUCCESS;
}

LC_API LCResult lc_savedata_to_json(LCSaveDataHandle handle, int pretty, char** out_json) {
    auto data = RequireSaveData(handle);
    if (!data) return LC_ERROR_INVALID_HANDLE;
    if (!out_json) { SetError(LC_ERROR_NULL_POINTER, "out_json is NULL"); return LC_ERROR_NULL_POINTER; }
    return WriteAllocatedString(data->ToString(pretty != 0), out_json);
}

/* ============================================================================
 * Manager configuration
 * ============================================================================ */

LC_API LCResult lc_savegame_set_directory(const char* virtual_dir) {
    if (!virtual_dir) { SetError(LC_ERROR_NULL_POINTER, "virtual_dir is NULL"); return LC_ERROR_NULL_POINTER; }
    save::SaveGameManager::GetInstance().SetSaveDirectory(virtual_dir);
    return LC_SUCCESS;
}

LC_API LCResult lc_savegame_set_format(const char* format_name) {
    if (!format_name) { SetError(LC_ERROR_NULL_POINTER, "format_name is NULL"); return LC_ERROR_NULL_POINTER; }
    save::SaveGameManager::GetInstance().SetFormatType(save::SaveFormatTypeFromString(format_name));
    return LC_SUCCESS;
}

LC_API LCResult lc_savegame_set_schema_version(int version) {
    save::SaveGameManager::GetInstance().SetSchemaVersion(version);
    return LC_SUCCESS;
}

LC_API LCResult lc_savegame_get_schema_version(int* out_version) {
    if (!out_version) { SetError(LC_ERROR_NULL_POINTER, "out_version is NULL"); return LC_ERROR_NULL_POINTER; }
    *out_version = save::SaveGameManager::GetInstance().GetSchemaVersion();
    return LC_SUCCESS;
}

LC_API LCResult lc_savegame_set_obfuscation_key(const char* key) {
    if (!key || key[0] == '\0') {
        save::SaveGameManager::GetInstance().SetTransform(nullptr);
    } else {
        save::SaveGameManager::GetInstance().SetTransform(
            std::make_shared<save::XorObfuscationTransform>(key));
    }
    return LC_SUCCESS;
}

LC_API LCResult lc_savegame_set_backup_on_write(int enabled) {
    save::SaveGameManager::GetInstance().SetBackupOnWrite(enabled != 0);
    return LC_SUCCESS;
}

LC_API LCResult lc_savegame_set_atomic_write(int enabled) {
    save::SaveGameManager::GetInstance().SetAtomicWrite(enabled != 0);
    return LC_SUCCESS;
}

LC_API LCResult lc_savegame_set_quick_slot(const char* slot) {
    if (!slot) { SetError(LC_ERROR_NULL_POINTER, "slot is NULL"); return LC_ERROR_NULL_POINTER; }
    save::SaveGameManager::GetInstance().SetQuickSaveSlot(slot);
    return LC_SUCCESS;
}

LC_API LCResult lc_savegame_set_auto_slot(const char* slot) {
    if (!slot) { SetError(LC_ERROR_NULL_POINTER, "slot is NULL"); return LC_ERROR_NULL_POINTER; }
    save::SaveGameManager::GetInstance().SetAutoSaveSlot(slot);
    return LC_SUCCESS;
}

/* ============================================================================
 * Manager slot operations
 * ============================================================================ */

LC_API LCResult lc_savegame_save(const char* slot, LCSaveDataHandle data, const char* meta_json) {
    if (!slot) { SetError(LC_ERROR_NULL_POINTER, "slot is NULL"); return LC_ERROR_NULL_POINTER; }
    auto saveData = RequireSaveData(data);
    if (!saveData) return LC_ERROR_INVALID_HANDLE;
    save::SaveResult result =
        save::SaveGameManager::GetInstance().Save(slot, *saveData, ParseMeta(meta_json));
    return SaveResultToLc(result);
}

LC_API LCResult lc_savegame_load(const char* slot, LCSaveDataHandle* out_data) {
    if (!slot || !out_data) { SetError(LC_ERROR_NULL_POINTER, "slot or out_data is NULL"); return LC_ERROR_NULL_POINTER; }
    auto loaded = std::make_shared<save::SaveData>();
    save::SaveResult result = save::SaveGameManager::GetInstance().Load(slot, *loaded);
    g_LastSaveResult = result;
    if (result != save::SaveResult::Success) {
        return SaveResultToLc(result);
    }
    *out_data = CreateSaveDataHandle(loaded);
    return LC_SUCCESS;
}

LC_API LCResult lc_savegame_slot_exists(const char* slot, int* out_exists) {
    if (!slot || !out_exists) { SetError(LC_ERROR_NULL_POINTER, "slot or out_exists is NULL"); return LC_ERROR_NULL_POINTER; }
    *out_exists = save::SaveGameManager::GetInstance().SlotExists(slot) ? 1 : 0;
    return LC_SUCCESS;
}

LC_API LCResult lc_savegame_delete_slot(const char* slot) {
    if (!slot) { SetError(LC_ERROR_NULL_POINTER, "slot is NULL"); return LC_ERROR_NULL_POINTER; }
    return SaveResultToLc(save::SaveGameManager::GetInstance().DeleteSlot(slot));
}

LC_API LCResult lc_savegame_copy_slot(const char* from_slot, const char* to_slot, int overwrite) {
    if (!from_slot || !to_slot) { SetError(LC_ERROR_NULL_POINTER, "slot is NULL"); return LC_ERROR_NULL_POINTER; }
    return SaveResultToLc(save::SaveGameManager::GetInstance().CopySlot(from_slot, to_slot, overwrite != 0));
}

LC_API LCResult lc_savegame_rename_slot(const char* from_slot, const char* to_slot, int overwrite) {
    if (!from_slot || !to_slot) { SetError(LC_ERROR_NULL_POINTER, "slot is NULL"); return LC_ERROR_NULL_POINTER; }
    return SaveResultToLc(save::SaveGameManager::GetInstance().RenameSlot(from_slot, to_slot, overwrite != 0));
}

LC_API LCResult lc_savegame_list_slots(char** out_json) {
    if (!out_json) { SetError(LC_ERROR_NULL_POINTER, "out_json is NULL"); return LC_ERROR_NULL_POINTER; }
    nlohmann::json array = nlohmann::json::array();
    for (const std::string& slot : save::SaveGameManager::GetInstance().ListSlots()) {
        array.push_back(slot);
    }
    return WriteAllocatedString(array.dump(), out_json);
}

LC_API LCResult lc_savegame_list_slot_infos(char** out_json) {
    if (!out_json) { SetError(LC_ERROR_NULL_POINTER, "out_json is NULL"); return LC_ERROR_NULL_POINTER; }
    nlohmann::json array = nlohmann::json::array();
    for (const save::SaveSlotInfo& info : save::SaveGameManager::GetInstance().ListSlotInfos()) {
        array.push_back(SlotInfoToJson(info));
    }
    return WriteAllocatedString(array.dump(), out_json);
}

LC_API LCResult lc_savegame_get_slot_info(const char* slot, char** out_json) {
    if (!slot || !out_json) { SetError(LC_ERROR_NULL_POINTER, "slot or out_json is NULL"); return LC_ERROR_NULL_POINTER; }
    save::SaveSlotInfo info;
    save::SaveResult result = save::SaveGameManager::GetInstance().GetSlotInfo(slot, info);
    g_LastSaveResult = result;
    if (result != save::SaveResult::Success) {
        return SaveResultToLc(result);
    }
    return WriteAllocatedString(SlotInfoToJson(info).dump(), out_json);
}

/* ============================================================================
 * Quick / auto save helpers
 * ============================================================================ */

LC_API LCResult lc_savegame_quick_save(LCSaveDataHandle data, const char* meta_json) {
    auto saveData = RequireSaveData(data);
    if (!saveData) return LC_ERROR_INVALID_HANDLE;
    return SaveResultToLc(save::SaveGameManager::GetInstance().QuickSave(*saveData, ParseMeta(meta_json)));
}

LC_API LCResult lc_savegame_quick_load(LCSaveDataHandle* out_data) {
    if (!out_data) { SetError(LC_ERROR_NULL_POINTER, "out_data is NULL"); return LC_ERROR_NULL_POINTER; }
    auto loaded = std::make_shared<save::SaveData>();
    save::SaveResult result = save::SaveGameManager::GetInstance().QuickLoad(*loaded);
    g_LastSaveResult = result;
    if (result != save::SaveResult::Success) {
        return SaveResultToLc(result);
    }
    *out_data = CreateSaveDataHandle(loaded);
    return LC_SUCCESS;
}

LC_API LCResult lc_savegame_has_quick_save(int* out_exists) {
    if (!out_exists) { SetError(LC_ERROR_NULL_POINTER, "out_exists is NULL"); return LC_ERROR_NULL_POINTER; }
    *out_exists = save::SaveGameManager::GetInstance().HasQuickSave() ? 1 : 0;
    return LC_SUCCESS;
}

LC_API LCResult lc_savegame_auto_save(LCSaveDataHandle data, const char* meta_json) {
    auto saveData = RequireSaveData(data);
    if (!saveData) return LC_ERROR_INVALID_HANDLE;
    return SaveResultToLc(save::SaveGameManager::GetInstance().AutoSave(*saveData, ParseMeta(meta_json)));
}

LC_API LCResult lc_savegame_has_auto_save(int* out_exists) {
    if (!out_exists) { SetError(LC_ERROR_NULL_POINTER, "out_exists is NULL"); return LC_ERROR_NULL_POINTER; }
    *out_exists = save::SaveGameManager::GetInstance().HasAutoSave() ? 1 : 0;
    return LC_SUCCESS;
}

/* ============================================================================
 * Scene-state capture / restore
 * ============================================================================ */

LC_API LCResult lc_savegame_capture_scene_state(const char* group, char** out_json) {
    if (!out_json) { SetError(LC_ERROR_NULL_POINTER, "out_json is NULL"); return LC_ERROR_NULL_POINTER; }
    core::SceneManager* manager = core::SceneManager::GetInstance();
    core::Scene* scene = manager ? manager->GetCurrentScene() : nullptr;
    nlohmann::json captured = save::SceneSaveState::CaptureGroup(scene, group ? group : "persistent");
    return WriteAllocatedString(captured.dump(), out_json);
}

LC_API LCResult lc_savegame_restore_scene_state(const char* captured_json, int* out_restored) {
    if (!captured_json || !out_restored) { SetError(LC_ERROR_NULL_POINTER, "captured_json or out_restored is NULL"); return LC_ERROR_NULL_POINTER; }
    core::SceneManager* manager = core::SceneManager::GetInstance();
    core::Scene* scene = manager ? manager->GetCurrentScene() : nullptr;
    nlohmann::json captured;
    try {
        captured = nlohmann::json::parse(captured_json);
    } catch (const std::exception&) {
        SetError(LC_ERROR_FILE_INVALID_FORMAT, "failed to parse captured scene state JSON");
        return LC_ERROR_FILE_INVALID_FORMAT;
    }
    *out_restored = save::SceneSaveState::RestoreGroup(scene, captured);
    return LC_SUCCESS;
}

/* ============================================================================
 * Diagnostics
 * ============================================================================ */

LC_API LCResult lc_savegame_get_last_error(char* out_buffer, size_t buffer_size) {
    if (!out_buffer) {
        SetError(LC_ERROR_NULL_POINTER, "out_buffer is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (buffer_size == 0) {
        SetError(LC_ERROR_INVALID_PARAMETER, "buffer_size is 0");
        return LC_ERROR_INVALID_PARAMETER;
    }
    const char* message = save::SaveResultToString(g_LastSaveResult);
    std::string value = message ? message : "";
    CopyStringToBuffer(out_buffer, buffer_size, value.c_str());
    return LC_SUCCESS;
}
