#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <nlohmann/json.hpp>

namespace lupine {
namespace localization {

/**
 * Source format a localization table was loaded from / will be saved as.
 */
enum class TableFormat {
    Json,   // Native .loctable JSON (rich: tags, comments, plurals)
    Csv     // Plain .csv (Key,Tags,Comment,<locale columns...>)
};

/**
 * A single localization entry: one key with per-locale translations,
 * optional grouping tags, an authoring comment, and optional plural forms.
 *
 * - values[locale] = the translated string for that locale.
 * - plurals[locale][category] = the plural form ("zero"/"one"/"two"/"few"/
 *   "many"/"other") for that locale, used by count-aware lookups. When a
 *   locale has no plural forms the singular value[] is used for all counts.
 */
struct LocEntry {
    std::string key;
    std::vector<std::string> tags;
    std::string comment;
    std::map<std::string, std::string> values;
    std::map<std::string, std::map<std::string, std::string>> plurals;

    bool HasValue(const std::string& locale) const;
    const std::string& GetValue(const std::string& locale) const;
    bool HasTag(const std::string& tag) const;
};

/**
 * A localization table = one group of keys, backed by a single file.
 *
 * Tables let a project break its strings across multiple files/groups (one
 * table per file). The table name is the file stem (e.g. "ui" for ui.loctable).
 * Both the rich native JSON format and a plain CSV format are first-class:
 * a project can be authored entirely with CSV files if desired.
 */
class LocalizationTable {
public:
    LocalizationTable() = default;

    // ===== Identity =====
    const std::string& GetName() const { return m_Name; }
    void SetName(const std::string& name) { m_Name = name; }

    const std::string& GetSourcePath() const { return m_SourcePath; }
    void SetSourcePath(const std::string& path) { m_SourcePath = path; }

    TableFormat GetFormat() const { return m_Format; }
    void SetFormat(TableFormat format) { m_Format = format; }

    // ===== Locales / entries =====
    const std::vector<std::string>& GetLocales() const { return m_Locales; }
    void SetLocales(const std::vector<std::string>& locales) { m_Locales = locales; }
    void AddLocale(const std::string& locale);

    const std::vector<LocEntry>& GetEntries() const { return m_Entries; }
    std::vector<LocEntry>& GetEntries() { return m_Entries; }

    // Find an entry by key, or nullptr if not present.
    const LocEntry* Find(const std::string& key) const;

    bool HasKey(const std::string& key) const { return Find(key) != nullptr; }

    // Insert or replace an entry (rebuilds the key index).
    void SetEntry(const LocEntry& entry);

    // ===== Load / Save =====

    // Load from a physical file path; format is chosen by extension
    // (.csv -> CSV, anything else -> JSON). Returns true on success.
    bool LoadFromFile(const std::string& physicalPath);

    bool LoadFromJsonText(const std::string& text);
    bool LoadFromJson(const nlohmann::json& json);
    bool LoadFromCsvText(const std::string& text);

    nlohmann::json ToJson() const;
    std::string ToJsonText() const;
    std::string ToCsvText() const;

    // ===== CSV helpers (RFC-4180 style: quoted fields, "" escaping) =====
    static std::vector<std::vector<std::string>> ParseCsv(const std::string& text);
    static std::string EscapeCsvField(const std::string& field);

private:
    void RebuildIndex();

    std::string m_Name;
    std::string m_SourcePath;
    TableFormat m_Format = TableFormat::Json;
    std::vector<std::string> m_Locales;
    std::vector<LocEntry> m_Entries;
    std::unordered_map<std::string, size_t> m_KeyIndex;
};

} // namespace localization
} // namespace lupine
