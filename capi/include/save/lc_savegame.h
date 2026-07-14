/**
 * @file lc_savegame.h
 * @brief C API for the Lupine save-game toolkit (save::SaveGameManager / SaveData).
 *
 * Two layers are exposed:
 *  - LCSaveDataHandle: a flexible save document. Build it with typed, key-addressed
 *    setters (keys may address nested objects with '/'), or pass JSON directly.
 *  - The global save manager: named save slots under a virtual directory (default
 *    "user://saves") with schema versioning + migration, a pluggable container
 *    format, optional XOR obfuscation, crash-safe atomic writes with backups, and
 *    per-slot header metadata readable without loading the full payload.
 *
 * Strings and JSON returned through `char**` out-parameters are heap-allocated;
 * the caller owns them and must release them with lc_free(). All functions return
 * LC_SUCCESS or an LC_ERROR_* code.
 */
#ifndef LUPINE_C_SAVEGAME_H
#define LUPINE_C_SAVEGAME_H

#include "core/lc_core.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Opaque handle to a save document (save::SaveData). */
typedef struct LCSaveData* LCSaveDataHandle;

/* ============================================================================
 * Save document lifecycle
 * ============================================================================ */

LC_API LCResult lc_savedata_create(LCSaveDataHandle* out_handle);
LC_API LCResult lc_savedata_create_from_json(const char* json_text, LCSaveDataHandle* out_handle);
LC_API LCResult lc_savedata_destroy(LCSaveDataHandle handle);
LC_API LCResult lc_savedata_clear(LCSaveDataHandle handle);

/* ============================================================================
 * Save document typed accessors
 *
 * Keys may address nested objects with '/', e.g. "player/stats/hp". Getters write
 * to *out_value and fall back to `def` when the key is missing or its type does
 * not match.
 * ============================================================================ */

LC_API LCResult lc_savedata_set_bool(LCSaveDataHandle handle, const char* key, int value);
LC_API LCResult lc_savedata_get_bool(LCSaveDataHandle handle, const char* key, int def, int* out_value);

LC_API LCResult lc_savedata_set_int(LCSaveDataHandle handle, const char* key, int64_t value);
LC_API LCResult lc_savedata_get_int(LCSaveDataHandle handle, const char* key, int64_t def, int64_t* out_value);

LC_API LCResult lc_savedata_set_double(LCSaveDataHandle handle, const char* key, double value);
LC_API LCResult lc_savedata_get_double(LCSaveDataHandle handle, const char* key, double def, double* out_value);

LC_API LCResult lc_savedata_set_string(LCSaveDataHandle handle, const char* key, const char* value);
/** @brief Writes a heap-allocated copy to *out_value (free with lc_free). */
LC_API LCResult lc_savedata_get_string(LCSaveDataHandle handle, const char* key, const char* def, char** out_value);

/** @brief Set the value at key to a parsed JSON value (object/array/scalar). */
LC_API LCResult lc_savedata_set_json(LCSaveDataHandle handle, const char* key, const char* json_text);
/** @brief Writes the value at key as JSON text to *out_json (free with lc_free). */
LC_API LCResult lc_savedata_get_json(LCSaveDataHandle handle, const char* key, char** out_json);

LC_API LCResult lc_savedata_has(LCSaveDataHandle handle, const char* key, int* out_has);
LC_API LCResult lc_savedata_erase(LCSaveDataHandle handle, const char* key);

/** @brief Serialize the whole document to JSON text (free with lc_free). */
LC_API LCResult lc_savedata_to_json(LCSaveDataHandle handle, int pretty, char** out_json);

/* ============================================================================
 * Manager configuration (global SaveGameManager)
 * ============================================================================ */

LC_API LCResult lc_savegame_set_directory(const char* virtual_dir);
/** @brief Format name: "json_text", "json_compact", "cbor" or "msgpack". */
LC_API LCResult lc_savegame_set_format(const char* format_name);
LC_API LCResult lc_savegame_set_schema_version(int version);
LC_API LCResult lc_savegame_get_schema_version(int* out_version);
/** @brief Install a reversible XOR obfuscation transform; NULL/empty clears it. */
LC_API LCResult lc_savegame_set_obfuscation_key(const char* key);
LC_API LCResult lc_savegame_set_backup_on_write(int enabled);
LC_API LCResult lc_savegame_set_atomic_write(int enabled);
LC_API LCResult lc_savegame_set_quick_slot(const char* slot);
LC_API LCResult lc_savegame_set_auto_slot(const char* slot);

/* ============================================================================
 * Manager slot operations
 *
 * `meta_json`, when non-NULL, is a JSON object string with optional keys "title",
 * "thumbnail", "playtime" and "metadata".
 * ============================================================================ */

LC_API LCResult lc_savegame_save(const char* slot, LCSaveDataHandle data, const char* meta_json);
LC_API LCResult lc_savegame_load(const char* slot, LCSaveDataHandle* out_data);
LC_API LCResult lc_savegame_slot_exists(const char* slot, int* out_exists);
LC_API LCResult lc_savegame_delete_slot(const char* slot);
LC_API LCResult lc_savegame_copy_slot(const char* from_slot, const char* to_slot, int overwrite);
LC_API LCResult lc_savegame_rename_slot(const char* from_slot, const char* to_slot, int overwrite);

/** @brief JSON array of slot id strings (free with lc_free). */
LC_API LCResult lc_savegame_list_slots(char** out_json);
/** @brief JSON array of slot info objects (free with lc_free). */
LC_API LCResult lc_savegame_list_slot_infos(char** out_json);
/** @brief JSON object of a single slot's header (free with lc_free). */
LC_API LCResult lc_savegame_get_slot_info(const char* slot, char** out_json);

/* ============================================================================
 * Quick / auto save helpers
 * ============================================================================ */

LC_API LCResult lc_savegame_quick_save(LCSaveDataHandle data, const char* meta_json);
LC_API LCResult lc_savegame_quick_load(LCSaveDataHandle* out_data);
LC_API LCResult lc_savegame_has_quick_save(int* out_exists);
LC_API LCResult lc_savegame_auto_save(LCSaveDataHandle data, const char* meta_json);
LC_API LCResult lc_savegame_has_auto_save(int* out_exists);

/* ============================================================================
 * Scene-state capture / restore (operates on the current scene)
 *
 * Captures/restores the serialized state of nodes tagged with `group`
 * (NULL means "persistent") via the engine's reflection.
 * ============================================================================ */

/** @brief Capture current-scene group state as a JSON array (free with lc_free). */
LC_API LCResult lc_savegame_capture_scene_state(const char* group, char** out_json);
/** @brief Restore group state from a JSON array; *out_restored receives the count. */
LC_API LCResult lc_savegame_restore_scene_state(const char* captured_json, int* out_restored);

/* ============================================================================
 * Diagnostics
 * ============================================================================ */

/**
 * @brief Get a human-readable description of the most recent save-manager
 *        operation's result on the calling thread (e.g. "Success",
 *        "WriteFailed"). The result is tracked per-thread across every
 *        save/load/copy/rename/delete slot operation. Writes a NUL-terminated
 *        string into out_buffer, truncated to buffer_size - 1 if needed.
 */
LC_API LCResult lc_savegame_get_last_error(char* out_buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_C_SAVEGAME_H */
