#include "lupine/save/SaveTypes.hpp"

#include <algorithm>
#include <cctype>

namespace lupine {
namespace save {

const char* SaveResultToString(SaveResult result) {
    switch (result) {
        case SaveResult::Success:         return "Success";
        case SaveResult::InvalidArgument: return "InvalidArgument";
        case SaveResult::SlotNotFound:    return "SlotNotFound";
        case SaveResult::WriteFailed:     return "WriteFailed";
        case SaveResult::ReadFailed:      return "ReadFailed";
        case SaveResult::ParseError:      return "ParseError";
        case SaveResult::FormatError:     return "FormatError";
        case SaveResult::VersionTooNew:   return "VersionTooNew";
        case SaveResult::MigrationFailed: return "MigrationFailed";
    }
    return "Unknown";
}

const char* SaveFormatTypeToString(SaveFormatType type) {
    switch (type) {
        case SaveFormatType::JsonText:      return "json_text";
        case SaveFormatType::JsonCompact:   return "json_compact";
        case SaveFormatType::BinaryCbor:    return "cbor";
        case SaveFormatType::BinaryMsgpack: return "msgpack";
    }
    return "json_text";
}

SaveFormatType SaveFormatTypeFromString(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (lower == "json_text" || lower == "json" || lower == "text") {
        return SaveFormatType::JsonText;
    }
    if (lower == "json_compact" || lower == "compact") {
        return SaveFormatType::JsonCompact;
    }
    if (lower == "cbor" || lower == "binary_cbor") {
        return SaveFormatType::BinaryCbor;
    }
    if (lower == "msgpack" || lower == "messagepack" || lower == "binary_msgpack") {
        return SaveFormatType::BinaryMsgpack;
    }
    return SaveFormatType::JsonText;
}

} // namespace save
} // namespace lupine
