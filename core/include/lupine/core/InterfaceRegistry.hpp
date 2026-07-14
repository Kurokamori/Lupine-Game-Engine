#pragma once

#include "InterfaceDefinition.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>

namespace lupine {
namespace core {

/**
 * Singleton registry that discovers and manages interface definitions and
 * tracks which native component types declare that they implement them.
 *
 * Interfaces are named capability contracts (required methods + required
 * signals). They are authored two ways, both discovered in a single scan pass:
 *   1. Data files: .interface JSON schemas authored in the editor.
 *   2. Scripts: .lua/.rb/.py files carrying @interface directives (these declare
 *      that the script IMPLEMENTS an interface; a script may also fully DEFINE
 *      an interface via @interface_define directives — see ParseScriptFile).
 * In addition, native C++ components register the interfaces they implement at
 * startup (RegisterTypeConformance), and built-in / native-extension interfaces
 * register their definitions natively (RegisterNativeInterface).
 *
 * Like ArchetypeRegistry, interfaces are data assets rather than scene node
 * types, so this registry does NOT register anything with TypeRegistry. It feeds
 * the editor (interface browser, conformance validation), the runtime query API
 * ("find every Damageable"), and the signal system (auto-declaring an
 * interface's required signals on implementers).
 */
class InterfaceRegistry {
public:
    /**
     * Get the singleton instance.
     */
    static InterfaceRegistry& GetInstance();

    // ------------------------------------------------------------------
    // Definition discovery / lifecycle
    // ------------------------------------------------------------------

    /**
     * Scan a project directory recursively for interface definitions. Native and
     * runtime-registered interfaces (and all type-conformance registrations) are
     * preserved across the scan.
     * @param projectPath Root path of the project (where res:// maps to)
     */
    void ScanProject(const std::string& projectPath);

    /**
     * Rescan a single file (for hot-reload when a definition changes).
     * @param sourcePath Path to the .interface or script file
     * @return true if a definition was added, updated, or removed
     */
    bool RescanFile(const std::string& sourcePath);

    /**
     * Remove a definition by source path (e.g., when the file is deleted).
     * @return true if a definition was removed
     */
    bool RemoveDefinition(const std::string& sourcePath);

    /**
     * Parse a single definition source file into an InterfaceDefinition WITHOUT
     * registering it (thread-safe, mutates no shared state).
     */
    InterfaceDefinition ParseDefinitionFile(const std::string& filePath);

    /**
     * Register (or update) a previously parsed file-sourced definition. MUST run
     * on the main thread.
     * @return true if a definition was added or updated.
     */
    bool RegisterParsedDefinition(const InterfaceDefinition& def);

    /**
     * Register a built-in / native-extension interface definition. Native
     * interfaces survive ScanProject and Clear; they are re-applied after every
     * project scan. Idempotent on name.
     */
    bool RegisterNativeInterface(const InterfaceDefinition& def);

    /**
     * Register an interface defined at runtime from a script or the C API. Like
     * native interfaces, runtime interfaces persist across project scans (so a
     * scene switch does not drop an interface a singleton declared). Idempotent
     * on name; a later registration replaces an earlier runtime definition.
     */
    bool RegisterRuntimeInterface(const InterfaceDefinition& def);

    /**
     * Clear file-sourced registrations (for project reload). Native and runtime
     * definitions and all type-conformance registrations are retained.
     */
    void Clear();

    // ------------------------------------------------------------------
    // Definition queries
    // ------------------------------------------------------------------

    /**
     * Get all registered interface definitions.
     */
    const std::vector<InterfaceDefinition>& GetDefinitions() const { return m_Definitions; }

    /**
     * Get a definition by name, or nullptr if not found.
     */
    const InterfaceDefinition* GetDefinition(const std::string& name) const;

    /**
     * Get a definition by source path, or nullptr if not found.
     */
    const InterfaceDefinition* GetDefinitionByPath(const std::string& sourcePath) const;

    /**
     * Check whether a name is a registered interface.
     */
    bool IsInterface(const std::string& name) const;

    /**
     * All registered interface names.
     */
    std::vector<std::string> GetInterfaceNames() const;

    /**
     * Get the inheritance chain for an interface, depth-first from the interface
     * itself through all (transitive) base interfaces, de-duplicated. Cycle-safe.
     */
    std::vector<std::string> GetInheritanceChain(const std::string& name) const;

    /**
     * Whether `name` is, or extends (transitively), `baseName`. A type that
     * declares it implements `name` therefore also satisfies `baseName`.
     */
    bool IsSubInterfaceOf(const std::string& name, const std::string& baseName) const;

    /**
     * The effective required methods of an interface: its own methods plus those
     * of every base interface (base contributions first; a derived method of the
     * same name overrides). Cycle-safe.
     */
    std::vector<InterfaceMethod> GetEffectiveMethods(const std::string& name) const;

    /**
     * The effective required signals of an interface (own + inherited).
     */
    std::vector<SignalDesc> GetEffectiveSignals(const std::string& name) const;

    /**
     * Expand a set of declared interface names to include every base interface
     * each one extends (transitively), de-duplicated. Given a node/archetype that
     * declares {"Destructible"} where Destructible extends Damageable, this
     * returns {"Destructible", "Damageable"}.
     */
    std::vector<std::string> ExpandImplied(const std::vector<std::string>& declared) const;

    // ------------------------------------------------------------------
    // Native component type conformance
    // ------------------------------------------------------------------

    /**
     * Record that a native component type (by GetTypeName()) implements an
     * interface. Registered at startup via the REGISTER_COMPONENT_INTERFACE
     * macro; persists across project scans. Idempotent.
     */
    void RegisterTypeConformance(const std::string& typeName, const std::string& interfaceName);

    /**
     * The interfaces a native component type declares it implements (direct
     * declarations only; call ExpandImplied for the transitive set).
     */
    std::vector<std::string> GetTypeInterfaces(const std::string& typeName) const;

    /**
     * Whether a native component type declares (directly or via interface
     * inheritance) that it implements an interface.
     */
    bool TypeImplementsInterface(const std::string& typeName, const std::string& interfaceName) const;

    /**
     * Every native component type that implements an interface (directly or via
     * interface inheritance).
     */
    std::vector<std::string> GetTypesImplementing(const std::string& interfaceName) const;

    // ------------------------------------------------------------------
    // Contract verification
    // ------------------------------------------------------------------

    /**
     * Verify a candidate against an interface's effective contract given the set
     * of method names and signal names it actually exposes. Returns a JSON object
     * of the form:
     *   { "interface": "Damageable", "exists": true, "conforms": false,
     *     "missing_methods": ["heal"], "missing_signals": ["died"] }
     * "exists" is false (and conforms false) when the interface is unknown.
     */
    nlohmann::json VerifyMembers(const std::string& interfaceName,
                                 const std::unordered_set<std::string>& presentMethods,
                                 const std::unordered_set<std::string>& presentSignals) const;

    // ------------------------------------------------------------------
    // Misc
    // ------------------------------------------------------------------

    const std::string& GetProjectPath() const { return m_ProjectPath; }

    using DefinitionsChangedCallback = std::function<void()>;
    void SetDefinitionsChangedCallback(DefinitionsChangedCallback callback) {
        m_OnDefinitionsChanged = callback;
    }

private:
    InterfaceRegistry() = default;
    ~InterfaceRegistry() = default;
    InterfaceRegistry(const InterfaceRegistry&) = delete;
    InterfaceRegistry& operator=(const InterfaceRegistry&) = delete;

    std::vector<InterfaceDefinition> m_Definitions;
    std::unordered_map<std::string, size_t> m_NameToIndex;   // name -> index
    std::unordered_map<std::string, size_t> m_PathToIndex;   // sourcePath -> index

    // Native + runtime definitions, retained across Clear()/ScanProject and
    // re-applied after each scan.
    std::vector<InterfaceDefinition> m_PersistentDefinitions;

    // Native component type conformance (always valid; never cleared by a scan).
    std::unordered_map<std::string, std::unordered_set<std::string>> m_TypeToInterfaces;
    std::unordered_map<std::string, std::unordered_set<std::string>> m_InterfaceToTypes;

    std::string m_ProjectPath;
    DefinitionsChangedCallback m_OnDefinitionsChanged;

    void ScanDirectory(const std::string& directory);
    // Enumerate the .pck instead of a directory. In pack mode (exported games) the
    // project has no tree on disk, so there is nothing for a directory walk to find.
    void ScanPack();
    // Parse one candidate file and register it if it declares an interface.
    // Shared by the directory and pack scans.
    void RegisterInterfaceFile(const std::string& filePath);
    // True for the extensions the registry parses (.interface / .lua / .rb / .py).
    static bool IsCandidateFile(const std::string& filePath);
    void StoreDefinition(const InterfaceDefinition& def);
    void RebuildIndices();
    void ApplyPersistentDefinitions();

    InterfaceDefinition ParseFile(const std::string& filePath);
    InterfaceDefinition ParseDataFile(const std::string& filePath);
    InterfaceDefinition ParseScriptFile(const std::string& filePath);
    void ParseScriptDefineDirectives(const std::string& content, const std::string& commentPrefix,
                                     InterfaceDefinition& def);

    static bool ReadFileText(const std::string& filePath, std::string& outContents);
    static scripting::ScriptLanguage GetLanguageFromExtension(const std::string& extension);
    std::string ToResourcePath(const std::string& absolutePath) const;
    void NotifyDefinitionsChanged();
};

} // namespace core
} // namespace lupine
