#pragma once

#include "ArchetypeDefinition.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace lupine {
namespace core {

/**
 * Singleton registry that discovers and manages archetype definitions.
 *
 * Archetypes are the Lupine equivalent of Unity ScriptableObjects / Godot
 * Resources: user-defined data types whose concrete values live in instance
 * assets (.ares). Definitions are authored two ways, both discovered in a single
 * scan pass:
 *   1. Data files: .archetype JSON schemas authored in the editor.
 *   2. Scripts: .lua/.rb/.py files carrying @archetype_class directives.
 *
 * Unlike CustomComponentRegistry, archetypes are data assets rather than scene
 * node types, so this registry does NOT register anything with TypeRegistry.
 * It only feeds the editor's Create menu / inspector and the runtime read API.
 */
class ArchetypeRegistry {
public:
    /**
     * Get the singleton instance.
     */
    static ArchetypeRegistry& GetInstance();

    /**
     * Scan a project directory recursively for archetype definitions.
     * @param projectPath Root path of the project (where res:// maps to)
     */
    void ScanProject(const std::string& projectPath);

    /**
     * Rescan a single file (for hot-reload when a definition changes).
     * @param sourcePath Path to the .archetype or script file
     * @return true if a definition was added, updated, or removed
     */
    bool RescanFile(const std::string& sourcePath);

    /**
     * Remove a definition by source path (e.g., when the file is deleted).
     * @param sourcePath Path to the source file
     * @return true if a definition was removed
     */
    bool RemoveDefinition(const std::string& sourcePath);

    /**
     * Get all registered archetype definitions.
     */
    const std::vector<ArchetypeDefinition>& GetDefinitions() const { return m_Definitions; }

    /**
     * Get a definition by class name, or nullptr if not found.
     */
    const ArchetypeDefinition* GetDefinition(const std::string& className) const;

    /**
     * Get a definition by source path, or nullptr if not found.
     */
    const ArchetypeDefinition* GetDefinitionByPath(const std::string& sourcePath) const;

    /**
     * Check whether a type name is a registered archetype.
     */
    bool IsArchetype(const std::string& className) const;

    /**
     * Get the merged (effective) field set for an archetype, including fields
     * inherited from its base chain. Base fields come first; a derived field
     * with the same name overrides the inherited one. Cycle-safe.
     */
    std::vector<PropertyDescriptor> GetEffectiveFields(const std::string& className) const;

    /**
     * Get the inheritance chain for an archetype, from the class itself up to
     * its root base: [className, base, base-of-base, ...]. Cycle-safe.
     */
    std::vector<std::string> GetInheritanceChain(const std::string& className) const;

    /**
     * Check whether className is, or descends from, baseName.
     */
    bool IsSubclassOf(const std::string& className, const std::string& baseName) const;

    /**
     * The interfaces an archetype declares it implements, unioned across its base
     * archetype chain (NOT expanded to the base interfaces those imply — call
     * InterfaceRegistry::ExpandImplied for that). Cycle-safe.
     */
    std::vector<std::string> GetImplementedInterfaces(const std::string& className) const;

    /**
     * Whether an archetype implements an interface, accounting for BOTH archetype
     * inheritance and interface inheritance.
     */
    bool ArchetypeImplementsInterface(const std::string& className,
                                      const std::string& interfaceName) const;

    /**
     * Every archetype class that implements an interface (archetype- and
     * interface-inheritance aware).
     */
    std::vector<std::string> GetArchetypesImplementing(const std::string& interfaceName) const;

    /**
     * Build a {name: default} map over the effective field set (for new instances).
     */
    nlohmann::json BuildEffectiveDefaultValues(const std::string& className) const;

    /**
     * Build a {name: descriptor} metadata map over the effective field set (for the inspector).
     */
    nlohmann::json BuildEffectivePropertyMetadata(const std::string& className) const;

    /**
     * Build a JSON array of serialized descriptors over the effective field set.
     */
    nlohmann::json BuildEffectiveFieldsJson(const std::string& className) const;

    /**
     * Parse a single definition source file into an ArchetypeDefinition WITHOUT
     * registering it. This performs the file read and (for scripts) the regex
     * directive/export parsing — the expensive part — but touches no shared
     * registry state, so it is safe to call from a background worker thread.
     *
     * The returned definition's isValid flag indicates whether the file declared
     * an archetype. Pass the result to RegisterParsedDefinition on the main
     * thread to complete the load.
     */
    ArchetypeDefinition ParseDefinitionFile(const std::string& filePath);

    /**
     * Register (or update) a previously parsed definition into the registry.
     * MUST run on the main thread — it mutates the definition tables and fires
     * the definitions-changed callback.
     *
     * If a definition with the same source path already exists it is updated;
     * otherwise the definition is added. A class-name collision with a different
     * source path is rejected (logged and returns false), mirroring ScanDirectory.
     * @return true if a definition was added or updated.
     */
    bool RegisterParsedDefinition(const ArchetypeDefinition& def);

    /**
     * Clear all registrations (for project reload).
     */
    void Clear();

    /**
     * Get the project path used by the last scan.
     */
    const std::string& GetProjectPath() const { return m_ProjectPath; }

    /**
     * Set a callback invoked when the set of definitions changes
     * (for editor UI refresh).
     */
    using DefinitionsChangedCallback = std::function<void()>;
    void SetDefinitionsChangedCallback(DefinitionsChangedCallback callback) {
        m_OnDefinitionsChanged = callback;
    }

private:
    ArchetypeRegistry() = default;
    ~ArchetypeRegistry() = default;
    ArchetypeRegistry(const ArchetypeRegistry&) = delete;
    ArchetypeRegistry& operator=(const ArchetypeRegistry&) = delete;

    std::vector<ArchetypeDefinition> m_Definitions;
    std::unordered_map<std::string, size_t> m_NameToIndex;   // className -> index
    std::unordered_map<std::string, size_t> m_PathToIndex;   // sourcePath -> index
    std::string m_ProjectPath;
    DefinitionsChangedCallback m_OnDefinitionsChanged;

    /**
     * Recursively scan a directory for definition sources.
     */
    void ScanDirectory(const std::string& directory);

    /**
     * Scan a mounted pack file (exported game) for definition sources. The
     * direct-filesystem iterator used by ScanDirectory cannot see files that
     * live only inside a .pck, so pack mode walks the PackFileSystem instead.
     */
    void ScanPack(const std::string& projectPath);

    /**
     * Parse a single candidate file path and, if it declares a valid archetype,
     * store it (handling duplicate detection). Shared by ScanDirectory/ScanPack.
     */
    void ProcessCandidateFile(const std::string& filePath);

    /**
     * Insert or replace a definition, maintaining the lookup indices.
     */
    void StoreDefinition(const ArchetypeDefinition& def);

    /**
     * Parse any supported file and return its definition (invalid if the file
     * does not declare an archetype).
     */
    ArchetypeDefinition ParseFile(const std::string& filePath);

    /**
     * Parse a .archetype JSON data file.
     */
    ArchetypeDefinition ParseDataFile(const std::string& filePath);

    /**
     * Parse a script file for @archetype_class directives.
     */
    ArchetypeDefinition ParseScriptFile(const std::string& filePath);

    /**
     * Parse the @archetype_* header directives.
     */
    void ParseDirectives(const std::string& content, const std::string& commentPrefix,
                         ArchetypeDefinition& def);

    /**
     * Parse @export field declarations (rich form with trailing @-annotations).
     */
    void ParseExportProperties(const std::string& content, const std::string& commentPrefix,
                               ArchetypeDefinition& def);

    /**
     * Read a file's text contents (pack-file aware).
     */
    static bool ReadFileText(const std::string& filePath, std::string& outContents);

    /**
     * Determine script language from file extension.
     */
    static scripting::ScriptLanguage GetLanguageFromExtension(const std::string& extension);

    /**
     * Convert a file path to res:// format relative to the project path.
     */
    std::string ToResourcePath(const std::string& absolutePath) const;

    /**
     * Notify listeners that definitions changed.
     */
    void NotifyDefinitionsChanged();
};

} // namespace core
} // namespace lupine
