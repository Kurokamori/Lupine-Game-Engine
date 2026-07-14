#pragma once

#include "lupine/localization/LocalizationTable.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <cstdint>

namespace lupine {
namespace localization {

/**
 * Project-level localization configuration, persisted as localization.json at
 * the project root (mirrors how input_map.json sits alongside the project).
 */
struct LocalizationConfig {
    bool enabled = true;
    std::string defaultLocale = "en";
    std::string fallbackLocale = "en";
    std::vector<std::string> locales = {"en"};
    std::string tablesDir = "localization";
    bool csvMode = false;
    bool pseudolocalization = false;

    nlohmann::json ToJson() const;
    void FromJson(const nlohmann::json& json);
};

/**
 * Singleton that owns the loaded localization tables and the active locale.
 *
 * Discovery mirrors ArchetypeRegistry: on project load it reads the config and
 * scans the configured tables directory for .loctable (rich JSON) and .csv
 * (plain) files, one table per file. Keys can be looked up table-qualified or
 * globally across every table.
 *
 * Translation resolves the active locale, then the configured fallback, then
 * the default locale, and finally returns the key itself if nothing matches
 * (so a missing key is visible rather than blank). Format-argument substitution
 * ({name} placeholders), CLDR-style plural selection, and pseudolocalization
 * (for layout testing) are all applied here.
 */
class LocalizationManager {
public:
    static LocalizationManager& GetInstance();

    // ===== Lifecycle =====

    // Load config + scan tables for a project directory (the folder where res://
    // maps to). Applies the persisted user locale if present, else the default.
    void LoadProject(const std::string& projectDir);

    // Re-read every table file from disk (keeps the active locale).
    void ReloadTables();

    // Drop all tables and reset to defaults (project reload).
    void Clear();

    bool IsLoaded() const { return m_Loaded; }
    bool IsEnabled() const { return m_Config.enabled; }
    const std::string& GetProjectDir() const { return m_ProjectDir; }

    // ===== Config =====
    const LocalizationConfig& GetConfig() const { return m_Config; }

    // ===== Tables =====
    const std::vector<LocalizationTable>& GetTables() const { return m_Tables; }
    const LocalizationTable* GetTable(const std::string& name) const;

    // ===== Locale =====

    // Set the active locale. When persist is true the choice is written to
    // user://locale.cfg so it survives across runs (used by the game/scripts);
    // the editor previews with persist == false. Fires the locale-changed
    // callback and emits the "locale_changed" EventBus event.
    void SetLocale(const std::string& locale, bool persist = true);
    const std::string& GetLocale() const { return m_CurrentLocale; }
    const std::string& GetDefaultLocale() const { return m_Config.defaultLocale; }
    const std::string& GetFallbackLocale() const { return m_Config.fallbackLocale; }

    // Union of config locales and every locale found in the loaded tables.
    std::vector<std::string> GetAvailableLocales() const;

    // ===== Translation =====

    // Look up key (optionally constrained to a table). Returns the resolved
    // string for the active locale, or the key itself if unresolved.
    std::string Translate(const std::string& key, const std::string& table = "") const;

    // Translate then substitute {name} placeholders from args. Use {{ and }} for
    // literal braces.
    std::string TranslateFormat(const std::string& key,
                                const std::unordered_map<std::string, std::string>& args,
                                const std::string& table = "") const;

    // Plural-aware translation: selects the plural form for the active locale and
    // count, then substitutes {count} plus any extra args.
    std::string TranslatePlural(const std::string& key, long count,
                                const std::unordered_map<std::string, std::string>& args = {},
                                const std::string& table = "") const;

    bool HasKey(const std::string& key, const std::string& table = "") const;

    // ===== Pseudolocalization (layout testing) =====
    void SetPseudolocalizationEnabled(bool enabled) { m_Pseudo = enabled; }
    bool IsPseudolocalizationEnabled() const { return m_Pseudo; }

    // ===== Hot reload =====
    void SetHotReloadEnabled(bool enabled) { m_HotReloadEnabled = enabled; }
    bool IsHotReloadEnabled() const { return m_HotReloadEnabled; }
    // Poll table file modification times; reloads changed files. Returns true if
    // anything reloaded (caller may then refresh UI). Cheap to call per frame.
    bool PollHotReload();

    // ===== Callbacks (editor UI refresh) =====
    using LocaleChangedCallback = std::function<void(const std::string&)>;
    void SetLocaleChangedCallback(LocaleChangedCallback callback) {
        m_OnLocaleChanged = callback;
    }

    // ===== Reusable text transforms (exposed for tooling/tests) =====
    static std::string Substitute(const std::string& text,
                                  const std::unordered_map<std::string, std::string>& args);
    static std::string Pseudo(const std::string& text);

private:
    LocalizationManager() = default;
    ~LocalizationManager() = default;
    LocalizationManager(const LocalizationManager&) = delete;
    LocalizationManager& operator=(const LocalizationManager&) = delete;

    void LoadConfig(const std::string& projectDir);
    void ScanTables(const std::string& projectDir);
    // Enumerates the tables directory inside the .pck; used instead of a directory
    // walk in pack mode, where the project has no tree on disk.
    void ScanTablesFromPack();
    // Loads one table and registers it under its stem, ignoring duplicate names.
    void AddTable(const std::string& sourcePath);
    void RebuildGlobalIndex();
    void RecordMtimes();

    // Core resolution: returns the best-available raw string for key in the
    // active locale (no pseudo, no substitution). Returns false if unresolved,
    // and sets out to the key as the visible fallback.
    bool Resolve(const std::string& key, const std::string& table, std::string& out) const;
    const LocEntry* FindEntry(const std::string& key, const std::string& table) const;

    std::string ActiveLocale() const;
    std::string PersistedLocalePath() const;
    void PersistLocale(const std::string& locale) const;
    std::string LoadPersistedLocale() const;

    void NotifyLocaleChanged();

    LocalizationConfig m_Config;
    std::vector<LocalizationTable> m_Tables;
    std::unordered_map<std::string, size_t> m_TableIndex;     // table name -> index
    std::unordered_map<std::string, size_t> m_GlobalKeyIndex; // key -> first table index
    std::unordered_map<std::string, int64_t> m_FileMtimes;    // source path -> mtime

    std::string m_ProjectDir;
    std::string m_CurrentLocale = "en";
    bool m_Loaded = false;
    bool m_Pseudo = false;
    bool m_HotReloadEnabled = false;

    LocaleChangedCallback m_OnLocaleChanged;
};

} // namespace localization
} // namespace lupine
