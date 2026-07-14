#pragma once

#include "lupine/scripting/ScriptingCore.hpp"
#include "PropertyDescriptor.hpp"
#include <string>
#include <vector>
#include <set>
#include <filesystem>

namespace lupine {
namespace core {

/**
 * Metadata structure holding parsed information about a custom scripted component type.
 * This is discovered by scanning project scripts for @component_class directives
 * or native class syntax (class MySprite < Sprite2D, etc.)
 */
struct CustomComponentDefinition {
    // The class name declared in the script (e.g., "MySprite")
    std::string className;

    // The base component type to inherit from (e.g., "Sprite2D", "Label")
    // Empty string means no inheritance (pure script component)
    std::string baseComponentType;

    // Path to the script file (e.g., "res://scripts/MySprite.lua")
    std::string scriptPath;

    // The scripting language used
    scripting::ScriptLanguage language = scripting::ScriptLanguage::Lua;

    // Export properties parsed from the script (@export directives)
    struct ExportProperty {
        std::string name;
        std::string group;  // Export group for organizing in inspector
        PropertyDescriptor descriptor;
    };
    std::vector<ExportProperty> exportProperties;

    // Set of lifecycle hooks that have override versions defined
    // e.g., {"on_render_override", "on_process_override"}
    // When an override hook exists, it replaces C++ behavior entirely
    std::set<std::string> overrideHooks;

    // Set of regular lifecycle hooks defined in the script
    // e.g., {"on_process", "on_ready"}
    std::set<std::string> definedHooks;

    // Editor subcategory for component menu (e.g., "Custom/2D", "Custom/UI")
    // Defaults to "Custom" if not specified
    std::string subcategory = "Custom";

    // Whether the script declared --@tool / #@tool. A tool component runs its
    // script lifecycle hooks inside the editor as well as at runtime; a regular
    // (game) component runs them only at runtime, so opening a scene in the
    // editor never executes its gameplay logic.
    bool isTool = false;

    // File modification time for hot-reload detection
    std::filesystem::file_time_type lastModified;

    // Whether parsing was successful
    bool isValid = false;

    // Error message if parsing failed
    std::string parseError;

    /**
     * Check if this definition extends a built-in component
     */
    bool HasBaseComponent() const { return !baseComponentType.empty(); }

    /**
     * Check if a specific override hook is defined
     */
    bool HasOverrideHook(const std::string& hookName) const {
        return overrideHooks.find(hookName + "_override") != overrideHooks.end();
    }

    /**
     * Check if a specific hook (regular or override) is defined
     */
    bool HasHook(const std::string& hookName) const {
        return definedHooks.find(hookName) != definedHooks.end() ||
               overrideHooks.find(hookName + "_override") != overrideHooks.end();
    }

    /**
     * Get file extension for the script language
     */
    std::string GetFileExtension() const {
        switch (language) {
            case scripting::ScriptLanguage::Lua: return ".lua";
            case scripting::ScriptLanguage::MRuby: return ".rb";
            case scripting::ScriptLanguage::MicroPython: return ".py";
            case scripting::ScriptLanguage::Python: return ".py";
            default: return "";
        }
    }
};

} // namespace core
} // namespace lupine
