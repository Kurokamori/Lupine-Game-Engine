#include "lupine/localization/LocalizationTable.hpp"
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

std::string ToLowerExt(const std::string& path) {
    std::string ext = std::filesystem::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}

bool ReadFileText(const std::string& filePath, std::string& outContents) {
    platform::PackFileSystem& packFS = platform::PackFileSystem::Instance();
    if (packFS.isPackMode() && packFS.exists(filePath)) {
        outContents = packFS.readFileAsString(filePath);
        return !outContents.empty();
    }

    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    outContents = buffer.str();
    return true;
}

// Split a semicolon-separated tag list into trimmed, non-empty tags.
std::vector<std::string> SplitTags(const std::string& text) {
    std::vector<std::string> tags;
    std::stringstream ss(text);
    std::string item;
    while (std::getline(ss, item, ';')) {
        size_t start = item.find_first_not_of(" \t\r\n");
        size_t end = item.find_last_not_of(" \t\r\n");
        if (start == std::string::npos) {
            continue;
        }
        tags.push_back(item.substr(start, end - start + 1));
    }
    return tags;
}

std::string JoinTags(const std::vector<std::string>& tags) {
    std::string result;
    for (size_t i = 0; i < tags.size(); ++i) {
        if (i > 0) {
            result += ";";
        }
        result += tags[i];
    }
    return result;
}

bool IsReservedColumn(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower == "key" || lower == "tags" || lower == "comment" || lower == "description";
}

} // namespace

// ===================================================================== LocEntry

bool LocEntry::HasValue(const std::string& locale) const {
    std::map<std::string, std::string>::const_iterator it = values.find(locale);
    return it != values.end() && !it->second.empty();
}

const std::string& LocEntry::GetValue(const std::string& locale) const {
    std::map<std::string, std::string>::const_iterator it = values.find(locale);
    if (it != values.end()) {
        return it->second;
    }
    return kEmptyString;
}

bool LocEntry::HasTag(const std::string& tag) const {
    return std::find(tags.begin(), tags.end(), tag) != tags.end();
}

// ============================================================ LocalizationTable

void LocalizationTable::AddLocale(const std::string& locale) {
    if (locale.empty()) {
        return;
    }
    if (std::find(m_Locales.begin(), m_Locales.end(), locale) == m_Locales.end()) {
        m_Locales.push_back(locale);
    }
}

const LocEntry* LocalizationTable::Find(const std::string& key) const {
    std::unordered_map<std::string, size_t>::const_iterator it = m_KeyIndex.find(key);
    if (it == m_KeyIndex.end()) {
        return nullptr;
    }
    return &m_Entries[it->second];
}

void LocalizationTable::SetEntry(const LocEntry& entry) {
    std::unordered_map<std::string, size_t>::iterator it = m_KeyIndex.find(entry.key);
    if (it != m_KeyIndex.end()) {
        m_Entries[it->second] = entry;
    } else {
        m_KeyIndex[entry.key] = m_Entries.size();
        m_Entries.push_back(entry);
    }
}

void LocalizationTable::RebuildIndex() {
    m_KeyIndex.clear();
    for (size_t i = 0; i < m_Entries.size(); ++i) {
        m_KeyIndex[m_Entries[i].key] = i;
    }
}

bool LocalizationTable::LoadFromFile(const std::string& physicalPath) {
    if (m_Name.empty()) {
        m_Name = std::filesystem::path(physicalPath).stem().string();
    }

    std::string contents;
    if (!ReadFileText(physicalPath, contents)) {
        LOG_ERROR(LogCategory::Core, "LocalizationTable: cannot read '{}'", physicalPath);
        return false;
    }

    if (ToLowerExt(physicalPath) == ".csv") {
        m_Format = TableFormat::Csv;
        return LoadFromCsvText(contents);
    }

    m_Format = TableFormat::Json;
    return LoadFromJsonText(contents);
}

bool LocalizationTable::LoadFromJsonText(const std::string& text) {
    try {
        nlohmann::json json = nlohmann::json::parse(text);
        return LoadFromJson(json);
    } catch (const std::exception& e) {
        LOG_ERROR(LogCategory::Core, "LocalizationTable '{}': JSON parse error: {}",
                  m_Name, e.what());
        return false;
    }
}

bool LocalizationTable::LoadFromJson(const nlohmann::json& json) {
    m_Locales.clear();
    m_Entries.clear();

    if (json.contains("name") && json["name"].is_string()) {
        m_Name = json["name"].get<std::string>();
    }

    if (json.contains("locales") && json["locales"].is_array()) {
        for (const nlohmann::json& loc : json["locales"]) {
            if (loc.is_string()) {
                AddLocale(loc.get<std::string>());
            }
        }
    }

    if (json.contains("entries") && json["entries"].is_array()) {
        for (const nlohmann::json& entryJson : json["entries"]) {
            if (!entryJson.is_object() || !entryJson.contains("key")) {
                continue;
            }
            LocEntry entry;
            entry.key = entryJson["key"].get<std::string>();

            if (entryJson.contains("tags") && entryJson["tags"].is_array()) {
                for (const nlohmann::json& tag : entryJson["tags"]) {
                    if (tag.is_string()) {
                        entry.tags.push_back(tag.get<std::string>());
                    }
                }
            }

            if (entryJson.contains("comment") && entryJson["comment"].is_string()) {
                entry.comment = entryJson["comment"].get<std::string>();
            }

            if (entryJson.contains("values") && entryJson["values"].is_object()) {
                for (nlohmann::json::const_iterator it = entryJson["values"].begin();
                     it != entryJson["values"].end(); ++it) {
                    if (it.value().is_string()) {
                        entry.values[it.key()] = it.value().get<std::string>();
                        AddLocale(it.key());
                    }
                }
            }

            if (entryJson.contains("plurals") && entryJson["plurals"].is_object()) {
                for (nlohmann::json::const_iterator localeIt = entryJson["plurals"].begin();
                     localeIt != entryJson["plurals"].end(); ++localeIt) {
                    if (!localeIt.value().is_object()) {
                        continue;
                    }
                    std::map<std::string, std::string> forms;
                    for (nlohmann::json::const_iterator formIt = localeIt.value().begin();
                         formIt != localeIt.value().end(); ++formIt) {
                        if (formIt.value().is_string()) {
                            forms[formIt.key()] = formIt.value().get<std::string>();
                        }
                    }
                    if (!forms.empty()) {
                        entry.plurals[localeIt.key()] = forms;
                        AddLocale(localeIt.key());
                    }
                }
            }

            m_Entries.push_back(entry);
        }
    }

    RebuildIndex();
    return true;
}

bool LocalizationTable::LoadFromCsvText(const std::string& text) {
    m_Locales.clear();
    m_Entries.clear();

    std::vector<std::vector<std::string>> rows = ParseCsv(text);
    if (rows.empty()) {
        RebuildIndex();
        return true;
    }

    const std::vector<std::string>& header = rows[0];
    int keyCol = -1;
    int tagsCol = -1;
    int commentCol = -1;
    std::vector<std::pair<int, std::string>> localeCols;

    for (size_t c = 0; c < header.size(); ++c) {
        std::string name = header[c];
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (lower == "key") {
            keyCol = static_cast<int>(c);
        } else if (lower == "tags") {
            tagsCol = static_cast<int>(c);
        } else if (lower == "comment" || lower == "description") {
            commentCol = static_cast<int>(c);
        } else if (!name.empty()) {
            localeCols.push_back(std::make_pair(static_cast<int>(c), name));
            AddLocale(name);
        }
    }

    if (keyCol < 0) {
        LOG_ERROR(LogCategory::Core,
                  "LocalizationTable '{}': CSV missing required 'Key' column", m_Name);
        return false;
    }

    for (size_t r = 1; r < rows.size(); ++r) {
        const std::vector<std::string>& row = rows[r];
        if (static_cast<int>(row.size()) <= keyCol || row[keyCol].empty()) {
            continue;
        }

        LocEntry entry;
        entry.key = row[keyCol];
        if (tagsCol >= 0 && static_cast<int>(row.size()) > tagsCol) {
            entry.tags = SplitTags(row[tagsCol]);
        }
        if (commentCol >= 0 && static_cast<int>(row.size()) > commentCol) {
            entry.comment = row[commentCol];
        }
        for (const std::pair<int, std::string>& col : localeCols) {
            if (static_cast<int>(row.size()) > col.first && !row[col.first].empty()) {
                entry.values[col.second] = row[col.first];
            }
        }
        m_Entries.push_back(entry);
    }

    RebuildIndex();
    return true;
}

nlohmann::json LocalizationTable::ToJson() const {
    nlohmann::json json;
    json["lupine_loctable"] = 1;
    json["name"] = m_Name;
    json["locales"] = m_Locales;

    nlohmann::json entriesJson = nlohmann::json::array();
    for (const LocEntry& entry : m_Entries) {
        nlohmann::json entryJson;
        entryJson["key"] = entry.key;
        if (!entry.tags.empty()) {
            entryJson["tags"] = entry.tags;
        }
        if (!entry.comment.empty()) {
            entryJson["comment"] = entry.comment;
        }

        nlohmann::json valuesJson = nlohmann::json::object();
        for (const std::pair<const std::string, std::string>& v : entry.values) {
            valuesJson[v.first] = v.second;
        }
        entryJson["values"] = valuesJson;

        if (!entry.plurals.empty()) {
            nlohmann::json pluralsJson = nlohmann::json::object();
            for (const std::pair<const std::string, std::map<std::string, std::string>>& localeForms :
                     entry.plurals) {
                nlohmann::json formsJson = nlohmann::json::object();
                for (const std::pair<const std::string, std::string>& form : localeForms.second) {
                    formsJson[form.first] = form.second;
                }
                pluralsJson[localeForms.first] = formsJson;
            }
            entryJson["plurals"] = pluralsJson;
        }

        entriesJson.push_back(entryJson);
    }
    json["entries"] = entriesJson;
    return json;
}

std::string LocalizationTable::ToJsonText() const {
    return ToJson().dump(2);
}

std::string LocalizationTable::ToCsvText() const {
    std::string out;
    out += "Key,Tags,Comment";
    for (const std::string& locale : m_Locales) {
        out += "," + EscapeCsvField(locale);
    }
    out += "\n";

    for (const LocEntry& entry : m_Entries) {
        out += EscapeCsvField(entry.key);
        out += "," + EscapeCsvField(JoinTags(entry.tags));
        out += "," + EscapeCsvField(entry.comment);
        for (const std::string& locale : m_Locales) {
            out += "," + EscapeCsvField(entry.GetValue(locale));
        }
        out += "\n";
    }
    return out;
}

std::string LocalizationTable::EscapeCsvField(const std::string& field) {
    bool needsQuotes = field.find(',') != std::string::npos ||
                       field.find('"') != std::string::npos ||
                       field.find('\n') != std::string::npos ||
                       field.find('\r') != std::string::npos;
    if (!needsQuotes) {
        return field;
    }
    std::string escaped = "\"";
    for (char c : field) {
        if (c == '"') {
            escaped += "\"\"";
        } else {
            escaped += c;
        }
    }
    escaped += "\"";
    return escaped;
}

std::vector<std::vector<std::string>> LocalizationTable::ParseCsv(const std::string& text) {
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> currentRow;
    std::string field;
    bool inQuotes = false;
    bool fieldStarted = false;

    size_t i = 0;
    const size_t n = text.size();
    while (i < n) {
        char c = text[i];

        if (inQuotes) {
            if (c == '"') {
                if (i + 1 < n && text[i + 1] == '"') {
                    field += '"';
                    i += 2;
                    continue;
                }
                inQuotes = false;
                i++;
                continue;
            }
            field += c;
            i++;
            continue;
        }

        if (c == '"') {
            inQuotes = true;
            fieldStarted = true;
            i++;
            continue;
        }
        if (c == ',') {
            currentRow.push_back(field);
            field.clear();
            fieldStarted = true;
            i++;
            continue;
        }
        if (c == '\r') {
            i++;
            continue;
        }
        if (c == '\n') {
            currentRow.push_back(field);
            field.clear();
            // Skip fully blank lines.
            bool blank = currentRow.size() == 1 && currentRow[0].empty();
            if (!blank) {
                rows.push_back(currentRow);
            }
            currentRow.clear();
            fieldStarted = false;
            i++;
            continue;
        }

        field += c;
        fieldStarted = true;
        i++;
    }

    if (fieldStarted || !field.empty() || !currentRow.empty()) {
        currentRow.push_back(field);
        bool blank = currentRow.size() == 1 && currentRow[0].empty();
        if (!blank) {
            rows.push_back(currentRow);
        }
    }

    return rows;
}

} // namespace localization
} // namespace lupine
