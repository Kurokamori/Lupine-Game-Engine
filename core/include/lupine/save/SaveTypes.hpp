#pragma once

#include <string>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace lupine {
namespace save {

/**
 * Result of a save/load/slot operation. SaveResult::Success is the only success
 * value; everything else describes a distinct failure mode so callers (and the
 * scripting / C-API layers) can react precisely.
 */
enum class SaveResult {
    Success = 0,
    InvalidArgument,    // empty/illegal slot id, null output, etc.
    SlotNotFound,       // no save file for the requested slot
    WriteFailed,        // the VFS rejected the write
    ReadFailed,         // the VFS rejected the read
    ParseError,         // the payload could not be decoded (corrupt / wrong format)
    FormatError,        // the envelope decoded but is missing required fields
    VersionTooNew,      // the file's schema version is newer than the app supports
    MigrationFailed     // a required schema migration was missing or failed
};

const char* SaveResultToString(SaveResult result);

/**
 * On-disk container encoding for a save payload. The default is human-readable
 * JSON (easy to debug and mod); compact JSON and the two binary encodings trade
 * readability for size. Loads auto-detect the encoding, so the format may be
 * changed between releases without breaking existing saves.
 */
enum class SaveFormatType {
    JsonText = 0,   // pretty-printed JSON (default)
    JsonCompact,    // single-line JSON
    BinaryCbor,     // CBOR binary (nlohmann::json::to_cbor)
    BinaryMsgpack   // MessagePack binary (nlohmann::json::to_msgpack)
};

const char* SaveFormatTypeToString(SaveFormatType type);
SaveFormatType SaveFormatTypeFromString(const std::string& name);

/**
 * Header metadata supplied by the game when writing a save. The timestamp is
 * stamped automatically by the manager; everything here is optional and is
 * stored in the save's header so it can be read back for a load menu without
 * deserializing the full payload.
 */
struct SaveSlotMeta {
    std::string title;                                  // user-facing label
    std::string thumbnailPath;                          // optional screenshot (virtual path)
    double playtimeSeconds = 0.0;                       // accumulated playtime, app-managed
    nlohmann::json metadata = nlohmann::json::object(); // free-form per-game fields
};

/**
 * Lightweight description of a save slot. Populated either by enumerating the
 * save directory or by reading a single slot's header. Reading a SaveSlotInfo
 * never constructs the full SaveData payload.
 */
struct SaveSlotInfo {
    std::string slot;                                   // logical slot id (file stem)
    std::string path;                                   // resolved virtual path
    bool exists = false;
    int schemaVersion = 0;                              // app schema version recorded in the file
    int envelopeVersion = 0;                            // container/envelope version
    SaveFormatType format = SaveFormatType::JsonText;   // detected encoding
    double timestampUnix = 0.0;                         // wall-clock seconds at write time
    double playtimeSeconds = 0.0;                       // accumulated playtime
    std::string title;                                  // user-facing label
    std::string thumbnailPath;                          // optional screenshot (virtual path)
    uint64_t payloadBytes = 0;                          // encoded file size in bytes
    nlohmann::json metadata = nlohmann::json::object(); // free-form per-game fields
};

} // namespace save
} // namespace lupine
