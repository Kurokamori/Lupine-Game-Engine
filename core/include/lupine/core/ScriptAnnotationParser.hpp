#pragma once

#include <string>
#include <vector>
#include <map>
#include "lupine/core/PropertyDescriptor.hpp"

namespace lupine {
namespace core {

/**
 * Result of parsing a script's export declarations.
 *
 * `properties` is the ordered list of exported properties (one PropertyDescriptor
 * each, with full editor metadata). `structs` maps an in-script `@struct` name to
 * its field schema, so a property typed as that struct can be edited inline.
 */
struct ScriptExportParseResult {
    std::vector<PropertyDescriptor> properties;
    std::map<std::string, std::vector<PropertyDescriptor>> structs;
};

/**
 * Shared parser for the engine's `@export` annotation grammar. A single source of
 * truth used by every script language (Lua `--`, Python/Ruby `#`) and by the
 * archetype definition loader, so the full attribute vocabulary is available
 * everywhere.
 *
 * Grammar (block-annotation-above is the documented form, but all forms parse):
 *
 *   --- Walk speed in px/s.            <- prose block becomes the description/tooltip
 *   --[Header("Movement")]            <- bracket attribute (Unity style)
 *   --[Range(0, 1000, 0.5)]
 *   --@export move_speed float 200.0  [Suffix("px")]   <- trailing tags also allowed
 *
 * Legacy `@tag value` spellings (e.g. `@range "0,1000,0.5"`, `@group "Combat"`)
 * remain valid both in the preceding block and trailing on the export line.
 *
 * In-script struct types:
 *   --@struct Stats { hp:int=10, mp:int=5, name:string="hero" }
 *   --@export base_stats Stats
 *
 * A type token that is neither a built-in type nor a declared `@struct` is treated
 * as a reference to a script/component class (stored as a NodePath with a
 * ScriptClass hint), letting fields point at another class by name.
 */
class ScriptAnnotationParser {
public:
    // Parse all `@export` declarations and `@struct` definitions from `content`.
    // `commentPrefix` is "--" for Lua or "#" for Python/Ruby.
    static ScriptExportParseResult ParseExports(const std::string& content,
                                                const std::string& commentPrefix);

    // Supported built-in type token (lower-cased) -> PropertyValueType. Returns
    // false when the token is not a built-in (caller then resolves it as a struct
    // or a script-class reference). Exposed for reuse/testing.
    static bool IsBuiltinTypeToken(const std::string& typeNameLower);
};

} // namespace core
} // namespace lupine
