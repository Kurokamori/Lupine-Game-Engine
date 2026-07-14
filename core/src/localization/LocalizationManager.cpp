#include "lupine/localization/LocalizationManager.hpp"
#include "lupine/localization/PluralRules.hpp"
#include "lupine/core/EventBus.hpp"
#include "lupine/platform/VirtualFileSystem.hpp"
#include "lupine/platform/PackFile.hpp"
#include "lupine/logger/Logger.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

namespace lupine {
namespace localization {

namespace {

const std::string kEmptyString;

int64_t FileMtime(const std::string& path) {
    std::error_code ec;
    std::filesystem::file_time_type t = std::filesystem::last_write_time(path, ec);
    if (ec) {
        return 0;
    }
    return static_cast<int64_t>(t.time_since_epoch().count());
}

std::string Trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

void AppendUtf8(std::string& out, uint32_t cp) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

} // namespace

// ========================================================== LocalizationConfig

nlohmann::json LocalizationConfig::ToJson() const {
    nlohmann::json json;
    json["lupine_localization"] = 1;
    json["enabled"] = enabled;
    json["default_locale"] = defaultLocale;
    json["fallback_locale"] = fallbackLocale;
    json["locales"] = locales;
    json["tables_dir"] = tablesDir;
    json["csv_mode"] = csvMode;
    json["pseudolocalization"] = pseudolocalization;
    return json;
}

void LocalizationConfig::FromJson(const nlohmann::json& json) {
    if (json.contains("enabled") && json["enabled"].is_boolean()) {
        enabled = json["enabled"].get<bool>();
    }
    if (json.contains("default_locale") && json["default_locale"].is_string()) {
        defaultLocale = json["default_locale"].get<std::string>();
    }
    if (json.contains("fallback_locale") && json["fallback_locale"].is_string()) {
        fallbackLocale = json["fallback_locale"].get<std::string>();
    }
    if (json.contains("locales") && json["locales"].is_array()) {
        locales.clear();
        for (const nlohmann::json& loc : json["locales"]) {
            if (loc.is_string()) {
                locales.push_back(loc.get<std::string>());
            }
        }
    }
    if (locales.empty()) {
        locales.push_back(defaultLocale);
    }
    if (json.contains("tables_dir") && json["tables_dir"].is_string()) {
        tablesDir = json["tables_dir"].get<std::string>();
    }
    if (json.contains("csv_mode") && json["csv_mode"].is_boolean()) {
        csvMode = json["csv_mode"].get<bool>();
    }
    if (json.contains("pseudolocalization") && json["pseudolocalization"].is_boolean()) {
        pseudolocalization = json["pseudolocalization"].get<bool>();
    }
}

// ========================================================= LocalizationManager

LocalizationManager& LocalizationManager::GetInstance() {
    static LocalizationManager instance;
    return instance;
}

void LocalizationManager::LoadProject(const std::string& projectDir) {
    Clear();
    m_ProjectDir = projectDir;

    LoadConfig(projectDir);
    ScanTables(projectDir);

    m_Pseudo = m_Config.pseudolocalization;

    std::string persisted = LoadPersistedLocale();
    std::vector<std::string> available = GetAvailableLocales();
    bool persistedValid = !persisted.empty() &&
        std::find(available.begin(), available.end(), persisted) != available.end();
    m_CurrentLocale = persistedValid ? persisted : m_Config.defaultLocale;

    // Hot-reload is a development convenience: enable it for loose-file projects,
    // disable for shipped pack builds where assets cannot change at runtime.
    m_HotReloadEnabled = !platform::PackFileSystem::Instance().isPackMode();

    m_Loaded = true;

    LOG_INFO(LogCategory::Core,
             "LocalizationManager: loaded {} table(s), locale '{}' (default '{}')",
             m_Tables.size(), m_CurrentLocale, m_Config.defaultLocale);
}

void LocalizationManager::Clear() {
    m_Tables.clear();
    m_TableIndex.clear();
    m_GlobalKeyIndex.clear();
    m_FileMtimes.clear();
    m_Config = LocalizationConfig();
    m_ProjectDir.clear();
    m_CurrentLocale = "en";
    m_Pseudo = false;
    m_Loaded = false;
}

void LocalizationManager::LoadConfig(const std::string& projectDir) {
    m_Config = LocalizationConfig();

    // In pack mode the project has no directory on disk, so joining against the empty
    // string would yield a leading "/" that the pack sandbox rejects as an absolute path.
    const std::string path = projectDir.empty()
        ? std::string("localization.json")
        : (projectDir + "/localization.json");

    std::string contents;

    platform::PackFileSystem& packFS = platform::PackFileSystem::Instance();
    if (packFS.isPackMode()) {
        const std::string resolved = packFS.resolveAsset(path);
        if (packFS.exists(resolved)) {
            contents = packFS.readFileAsString(resolved);
        }
    }

    if (contents.empty()) {
        std::ifstream file(path);
        if (!file.is_open()) {
            return;
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        contents = buffer.str();
    }

    if (contents.empty()) {
        return;
    }

    try {
        nlohmann::json json = nlohmann::json::parse(contents);
        m_Config.FromJson(json);
    } catch (const std::exception& e) {
        LOG_WARN(LogCategory::Core,
                 "LocalizationManager: failed to parse '{}': {}", path, e.what());
    }
}

void LocalizationManager::AddTable(const std::string& sourcePath) {
    const std::filesystem::path source(sourcePath);

    std::string ext = source.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext != ".loctable" && ext != ".csv") {
        return;
    }

    LocalizationTable table;
    table.SetName(source.stem().string());
    if (!table.LoadFromFile(sourcePath)) {
        return;
    }
    table.SetSourcePath(sourcePath);

    if (m_TableIndex.find(table.GetName()) != m_TableIndex.end()) {
        LOG_WARN(LogCategory::Core,
                 "LocalizationManager: duplicate table name '{}' ('{}'); keeping first",
                 table.GetName(), sourcePath);
        return;
    }

    const size_t idx = m_Tables.size();
    m_Tables.push_back(table);
    m_TableIndex[table.GetName()] = idx;
}

void LocalizationManager::ScanTablesFromPack() {
    platform::PackFileSystem& packFS = platform::PackFileSystem::Instance();

    // LocalizationTable::LoadFromFile is pack-aware, but only for a res:// key - the
    // enumeration has to come from the pack index, since there is no directory to walk.
    const std::vector<std::string> files = packFS.listFilesRecursive(m_Config.tablesDir);
    for (const std::string& file : files) {
        AddTable("res://" + file);
    }
}

void LocalizationManager::ScanTables(const std::string& projectDir) {
    m_Tables.clear();
    m_TableIndex.clear();

    platform::PackFileSystem& packFS = platform::PackFileSystem::Instance();
    if (packFS.isPackMode()) {
        ScanTablesFromPack();
        RebuildGlobalIndex();
        RecordMtimes();
        return;
    }

    const std::string dir = projectDir.empty()
        ? m_Config.tablesDir
        : (projectDir + "/" + m_Config.tablesDir);

    if (std::filesystem::exists(dir)) {
        try {
            for (const std::filesystem::directory_entry& entry :
                     std::filesystem::recursive_directory_iterator(dir)) {
                if (!entry.is_regular_file()) {
                    continue;
                }
                AddTable(entry.path().string());
            }
        } catch (const std::filesystem::filesystem_error& e) {
            LOG_ERROR(LogCategory::Core,
                      "LocalizationManager: error scanning '{}': {}", dir, e.what());
        }
    }

    RebuildGlobalIndex();
    RecordMtimes();
}

void LocalizationManager::RebuildGlobalIndex() {
    m_GlobalKeyIndex.clear();
    for (size_t i = 0; i < m_Tables.size(); ++i) {
        for (const LocEntry& entry : m_Tables[i].GetEntries()) {
            if (m_GlobalKeyIndex.find(entry.key) == m_GlobalKeyIndex.end()) {
                m_GlobalKeyIndex[entry.key] = i;
            }
        }
    }
}

void LocalizationManager::RecordMtimes() {
    m_FileMtimes.clear();
    for (const LocalizationTable& table : m_Tables) {
        if (!table.GetSourcePath().empty()) {
            m_FileMtimes[table.GetSourcePath()] = FileMtime(table.GetSourcePath());
        }
    }
}

void LocalizationManager::ReloadTables() {
    if (m_ProjectDir.empty()) {
        return;
    }
    ScanTables(m_ProjectDir);
}

bool LocalizationManager::PollHotReload() {
    if (!m_HotReloadEnabled || !m_Loaded) {
        return false;
    }
    bool changed = false;
    for (const std::pair<const std::string, int64_t>& entry : m_FileMtimes) {
        if (FileMtime(entry.first) != entry.second) {
            changed = true;
            break;
        }
    }
    if (!changed) {
        return false;
    }

    ReloadTables();
    if (m_OnLocaleChanged) {
        m_OnLocaleChanged(m_CurrentLocale);
    }
    core::EventBus::Get().Emit("localization_reloaded", { nlohmann::json(m_CurrentLocale) });
    return true;
}

const LocalizationTable* LocalizationManager::GetTable(const std::string& name) const {
    std::unordered_map<std::string, size_t>::const_iterator it = m_TableIndex.find(name);
    if (it == m_TableIndex.end()) {
        return nullptr;
    }
    return &m_Tables[it->second];
}

void LocalizationManager::SetLocale(const std::string& locale, bool persist) {
    m_CurrentLocale = locale;
    if (persist) {
        PersistLocale(locale);
    }
    NotifyLocaleChanged();
}

std::vector<std::string> LocalizationManager::GetAvailableLocales() const {
    std::vector<std::string> result = m_Config.locales;
    for (const LocalizationTable& table : m_Tables) {
        for (const std::string& locale : table.GetLocales()) {
            if (std::find(result.begin(), result.end(), locale) == result.end()) {
                result.push_back(locale);
            }
        }
    }
    return result;
}

std::string LocalizationManager::ActiveLocale() const {
    return m_Config.enabled ? m_CurrentLocale : m_Config.defaultLocale;
}

const LocEntry* LocalizationManager::FindEntry(const std::string& key,
                                               const std::string& table) const {
    if (!table.empty()) {
        const LocalizationTable* t = GetTable(table);
        return t ? t->Find(key) : nullptr;
    }
    std::unordered_map<std::string, size_t>::const_iterator it = m_GlobalKeyIndex.find(key);
    if (it == m_GlobalKeyIndex.end()) {
        return nullptr;
    }
    return m_Tables[it->second].Find(key);
}

bool LocalizationManager::Resolve(const std::string& key, const std::string& table,
                                  std::string& out) const {
    const LocEntry* entry = FindEntry(key, table);
    if (!entry) {
        out = key;
        return false;
    }

    const std::string locale = ActiveLocale();
    if (entry->HasValue(locale)) {
        out = entry->GetValue(locale);
        return true;
    }
    if (entry->HasValue(m_Config.fallbackLocale)) {
        out = entry->GetValue(m_Config.fallbackLocale);
        return true;
    }
    if (entry->HasValue(m_Config.defaultLocale)) {
        out = entry->GetValue(m_Config.defaultLocale);
        return true;
    }
    if (!entry->values.empty()) {
        out = entry->values.begin()->second;
        return true;
    }
    out = key;
    return false;
}

std::string LocalizationManager::Translate(const std::string& key,
                                           const std::string& table) const {
    std::string out;
    Resolve(key, table, out);
    if (m_Config.enabled && m_Pseudo) {
        out = Pseudo(out);
    }
    return out;
}

std::string LocalizationManager::TranslateFormat(
        const std::string& key,
        const std::unordered_map<std::string, std::string>& args,
        const std::string& table) const {
    return Substitute(Translate(key, table), args);
}

std::string LocalizationManager::TranslatePlural(
        const std::string& key, long count,
        const std::unordered_map<std::string, std::string>& args,
        const std::string& table) const {
    const LocEntry* entry = FindEntry(key, table);
    std::string raw;

    if (entry) {
        const std::string locale = ActiveLocale();
        const std::string category = PluralRules::Category(locale, count);

        std::map<std::string, std::map<std::string, std::string>>::const_iterator formsIt =
            entry->plurals.find(locale);
        if (formsIt == entry->plurals.end()) {
            formsIt = entry->plurals.find(m_Config.fallbackLocale);
        }
        if (formsIt == entry->plurals.end()) {
            formsIt = entry->plurals.find(m_Config.defaultLocale);
        }
        if (formsIt != entry->plurals.end()) {
            std::map<std::string, std::string>::const_iterator catIt =
                formsIt->second.find(category);
            if (catIt == formsIt->second.end()) {
                catIt = formsIt->second.find("other");
            }
            if (catIt != formsIt->second.end()) {
                raw = catIt->second;
            }
        }

        if (raw.empty()) {
            Resolve(key, table, raw);
        }
    } else {
        raw = key;
    }

    if (m_Config.enabled && m_Pseudo) {
        raw = Pseudo(raw);
    }

    std::unordered_map<std::string, std::string> allArgs = args;
    allArgs["count"] = std::to_string(count);
    return Substitute(raw, allArgs);
}

bool LocalizationManager::HasKey(const std::string& key, const std::string& table) const {
    return FindEntry(key, table) != nullptr;
}

// ===================================================================== Persistence

std::string LocalizationManager::PersistedLocalePath() const {
    return "user://locale.cfg";
}

void LocalizationManager::PersistLocale(const std::string& locale) const {
    platform::VirtualFileSystem& vfs = platform::VirtualFileSystem::GetInstance();
    if (!vfs.IsInitialized()) {
        return;
    }
    vfs.WriteFile(PersistedLocalePath(), locale);
}

std::string LocalizationManager::LoadPersistedLocale() const {
    platform::VirtualFileSystem& vfs = platform::VirtualFileSystem::GetInstance();
    if (!vfs.IsInitialized()) {
        return "";
    }
    if (!vfs.Exists(PersistedLocalePath())) {
        return "";
    }
    platform::FileResult<std::string> result = vfs.ReadFile(PersistedLocalePath());
    if (!result.success) {
        return "";
    }
    return Trim(result.data);
}

void LocalizationManager::NotifyLocaleChanged() {
    if (m_OnLocaleChanged) {
        m_OnLocaleChanged(m_CurrentLocale);
    }
    core::EventBus::Get().Emit("locale_changed", { nlohmann::json(m_CurrentLocale) });
}

// ============================================================ Text transforms

std::string LocalizationManager::Substitute(
        const std::string& text,
        const std::unordered_map<std::string, std::string>& args) {
    std::string out;
    out.reserve(text.size());

    const size_t n = text.size();
    for (size_t i = 0; i < n; ++i) {
        char c = text[i];
        if (c == '{') {
            if (i + 1 < n && text[i + 1] == '{') {
                out += '{';
                ++i;
                continue;
            }
            size_t close = text.find('}', i + 1);
            if (close == std::string::npos) {
                out += c;
                continue;
            }
            std::string name = text.substr(i + 1, close - i - 1);
            std::unordered_map<std::string, std::string>::const_iterator it = args.find(name);
            if (it != args.end()) {
                out += it->second;
            } else {
                out += text.substr(i, close - i + 1);
            }
            i = close;
        } else if (c == '}') {
            if (i + 1 < n && text[i + 1] == '}') {
                out += '}';
                ++i;
                continue;
            }
            out += c;
        } else {
            out += c;
        }
    }
    return out;
}

std::string LocalizationManager::Pseudo(const std::string& text) {
    static const uint32_t kLower[26] = {
        0xE1, 0x180, 0xE7, 0xF0, 0xE9, 0x192, 0x11D, 0x125, 0xED, 0x135,
        0x137, 0x142, 0x271, 0xF1, 0xF3, 0xFE, 0x1EB, 0x155, 0x161, 0x163,
        0xFA, 0x475, 0x175, 0x3C7, 0xFD, 0x17E
    };
    static const uint32_t kUpper[26] = {
        0xC1, 0x181, 0xC7, 0xD0, 0xC9, 0x191, 0x11C, 0x124, 0xCD, 0x134,
        0x136, 0x141, 0x1E40, 0xD1, 0xD3, 0xDE, 0x1EA, 0x154, 0x160, 0x162,
        0xDA, 0x474, 0x174, 0x3A7, 0xDD, 0x17D
    };

    std::string out;
    out.reserve(text.size() * 2 + 8);
    out += "[";

    bool inBrace = false;
    for (char c : text) {
        if (c == '{') {
            inBrace = true;
            out += c;
            continue;
        }
        if (c == '}') {
            inBrace = false;
            out += c;
            continue;
        }
        if (inBrace) {
            out += c;
            continue;
        }
        if (c >= 'a' && c <= 'z') {
            AppendUtf8(out, kLower[c - 'a']);
        } else if (c >= 'A' && c <= 'Z') {
            AppendUtf8(out, kUpper[c - 'A']);
        } else {
            out += c;
        }
    }

    size_t pad = text.size() / 3;
    if (pad < 2) {
        pad = 2;
    }
    out += " ";
    for (size_t i = 0; i < pad; ++i) {
        out += "~";
    }
    out += "]";
    return out;
}

} // namespace localization
} // namespace lupine
