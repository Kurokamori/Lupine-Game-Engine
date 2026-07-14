#pragma once

#include "CustomComponentDefinition.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace lupine {
namespace core {

/**
 * Singleton registry that discovers and manages custom scripted component definitions.
 *
 * This class scans project scripts for custom component declarations using:
 * 1. Directive style: --@component_class "MySprite" / --@extends_component "Sprite2D"
 * 2. Native class syntax: class MySprite < Sprite2D (Ruby), class MySprite(Sprite2D): (Python)
 *
 * It integrates with TypeRegistry to make custom components available for instantiation
 * and with EditorBridge to populate the component menu.
 */
class CustomComponentRegistry {
public:
    /**
     * Get the singleton instance
     */
    static CustomComponentRegistry& GetInstance();

    /**
     * Scan all script files in the project for custom component declarations
     * @param projectPath Root path of the project (where res:// maps to)
     */
    void ScanProject(const std::string& projectPath);

    /**
     * Rescan a single file (for hot-reload when a script changes)
     * @param scriptPath Path to the script file
     * @return true if the file contained a custom component definition that was updated
     */
    bool RescanFile(const std::string& scriptPath);

    /**
     * Remove a definition by script path (e.g., when file is deleted)
     * @param scriptPath Path to the script file
     * @return true if a definition was removed
     */
    bool RemoveDefinition(const std::string& scriptPath);

    /**
     * Get all registered custom component definitions
     */
    const std::vector<CustomComponentDefinition>& GetDefinitions() const { return m_Definitions; }

    /**
     * Get a definition by class name
     * @return Pointer to definition or nullptr if not found
     */
    const CustomComponentDefinition* GetDefinition(const std::string& className) const;

    /**
     * Get a definition by script path
     * @return Pointer to definition or nullptr if not found
     */
    const CustomComponentDefinition* GetDefinitionByPath(const std::string& scriptPath) const;

    /**
     * Check if a type name is a custom scripted component
     */
    bool IsCustomComponent(const std::string& typeName) const;

    /**
     * Register all discovered custom types with TypeRegistry
     * This makes them available for component creation via CreateInstance
     */
    void RegisterWithTypeRegistry();

    /**
     * Unregister all custom types from TypeRegistry
     */
    void UnregisterFromTypeRegistry();

    /**
     * Clear all registrations (for project reload)
     */
    void Clear();

    /**
     * Get the project path
     */
    const std::string& GetProjectPath() const { return m_ProjectPath; }

    /**
     * Set callback for when definitions change (for editor UI refresh)
     */
    using DefinitionsChangedCallback = std::function<void()>;
    void SetDefinitionsChangedCallback(DefinitionsChangedCallback callback) {
        m_OnDefinitionsChanged = callback;
    }

    /**
     * Get list of all base component types that can be inherited
     */
    static std::vector<std::string> GetInheritableComponentTypes();

private:
    CustomComponentRegistry() = default;
    ~CustomComponentRegistry() = default;
    CustomComponentRegistry(const CustomComponentRegistry&) = delete;
    CustomComponentRegistry& operator=(const CustomComponentRegistry&) = delete;

    std::vector<CustomComponentDefinition> m_Definitions;
    std::unordered_map<std::string, size_t> m_NameToIndex;      // className -> index
    std::unordered_map<std::string, size_t> m_PathToIndex;      // scriptPath -> index
    std::string m_ProjectPath;
    DefinitionsChangedCallback m_OnDefinitionsChanged;

    // Class names discovered by the pre-scan pass (PreScanClassNames). These are
    // appended to GetInheritableComponentTypes() so a custom component can extend
    // another custom component (multi-level inheritance chains) regardless of the
    // order files happen to be scanned in.
    std::vector<std::string> m_ExtraInheritableTypes;

    /**
     * Parse a script file and return its definition (if it declares a custom component)
     */
    CustomComponentDefinition ParseScriptFile(const std::string& filePath);

    /**
     * Parse script content based on language
     */
    CustomComponentDefinition ParseLuaScript(const std::string& content, const std::string& path);
    CustomComponentDefinition ParseMRubyScript(const std::string& content, const std::string& path);
    CustomComponentDefinition ParseMicroPythonScript(const std::string& content, const std::string& path);

    /**
     * Parse directive-style declarations (works for all languages)
     * @param content Script content
     * @param commentPrefix Comment prefix for the language ("--" for Lua, "#" for Ruby/Python)
     * @param def Definition to populate
     */
    void ParseDirectives(const std::string& content, const std::string& commentPrefix,
                         CustomComponentDefinition& def);

    /**
     * Parse native class syntax (language-specific)
     */
    void ParseLuaNativeClass(const std::string& content, CustomComponentDefinition& def);
    void ParseMRubyNativeClass(const std::string& content, CustomComponentDefinition& def);
    void ParsePythonNativeClass(const std::string& content, CustomComponentDefinition& def);

    /**
     * Parse export properties from script content
     */
    void ParseExportProperties(const std::string& content, const std::string& commentPrefix,
                               CustomComponentDefinition& def);

    /**
     * Detect which lifecycle hooks (and override hooks) are defined
     */
    void DetectDefinedHooks(const std::string& content, scripting::ScriptLanguage language,
                            CustomComponentDefinition& def);

    /**
     * Determine script language from file extension
     */
    static scripting::ScriptLanguage GetLanguageFromExtension(const std::string& extension);

    /**
     * Convert a file path to res:// format
     */
    std::string ToResourcePath(const std::string& absolutePath) const;

    /**
     * Notify that definitions have changed
     */
    void NotifyDefinitionsChanged();

    /**
     * Recursively scan a directory for script files
     */
    void ScanDirectory(const std::string& directory);

    /**
     * Scan the .pck instead of a directory. In pack mode (exported games) the project
     * has no tree on disk, so the script files must be enumerated from the pack index.
     * Runs the same two passes as the directory scan, in the same order.
     */
    void ScanPack();

    /**
     * Read a script's source. Reads from the .pck first when one is mounted, since in
     * pack mode there is no file on disk for ifstream to open.
     */
    std::string ReadScriptSource(const std::string& filePath) const;

    /**
     * Parse one script file and register it if it declares a custom component.
     * Shared by the directory and pack scans.
     */
    void RegisterScriptFile(const std::string& filePath);

    /**
     * Pre-scan pass: walk every script file and extract only its declared class
     * name (ignoring whether its base type is known yet), populating
     * m_ExtraInheritableTypes. Run before the full ParseScriptFile pass so that a
     * custom component declared with native syntax can extend another custom
     * component that is defined in a file scanned later.
     */
    void PreScanClassNames(const std::string& directory);

    /** Pre-scan a single script file; shared by the directory and pack pre-scans. */
    void PreScanClassName(const std::string& filePath);

    /** True for the script extensions the registry parses (.lua / .rb / .py). */
    static bool IsScriptFile(const std::string& filePath);

    /**
     * Extract only the declared custom class name from script content for the
     * given file extension, without requiring the base type to be resolvable.
     * Returns an empty string if the content does not declare a custom component.
     */
    std::string ExtractClassNameOnly(const std::string& content, const std::string& extension) const;

    /**
     * After all definitions are parsed, walk each definition's base-type chain
     * through the custom definitions and break any inheritance cycle (including
     * self-extension) so instantiation cannot recurse forever. Cycles are logged
     * as errors and the offending base link is cleared.
     */
    void ValidateInheritanceChains();
};

} // namespace core
} // namespace lupine
