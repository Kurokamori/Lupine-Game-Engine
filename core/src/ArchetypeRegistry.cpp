#include "lupine/core/ArchetypeRegistry.hpp"
#include "lupine/core/ScriptAnnotationParser.hpp"
#include "lupine/core/InterfaceRegistry.hpp"
#include "lupine/platform/PackFile.hpp"
#include "lupine/logger/Logger.hpp"
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <cctype>
#include <map>
#include <set>

namespace lupine {
namespace core {


ArchetypeRegistry& ArchetypeRegistry::GetInstance() {
    static ArchetypeRegistry instance;
    return instance;
}

void ArchetypeRegistry::ScanProject(const std::string& projectPath) {
    m_ProjectPath = projectPath;
    Clear();

    platform::PackFileSystem& packFS = platform::PackFileSystem::Instance();
    if (packFS.isPackMode()) {
        ScanPack(projectPath);
    } else if (std::filesystem::exists(projectPath)) {
        ScanDirectory(projectPath);
    }

    NotifyDefinitionsChanged();

    LOG_INFO(LogCategory::Core, "ArchetypeRegistry: Scanned project, found {} archetypes",
             m_Definitions.size());
}

void ArchetypeRegistry::ScanDirectory(const std::string& directory) {
    try {
        for (const std::filesystem::directory_entry& entry :
                 std::filesystem::recursive_directory_iterator(directory)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            ProcessCandidateFile(entry.path().string());
        }
    } catch (const std::filesystem::filesystem_error& e) {
        LOG_ERROR(LogCategory::Core, "ArchetypeRegistry: Error scanning directory '{}': {}",
                  directory, e.what());
    }
}

void ArchetypeRegistry::ScanPack(const std::string& projectPath) {
    (void)projectPath;
    platform::PackFileSystem& packFS = platform::PackFileSystem::Instance();

    std::vector<std::string> files = packFS.listFilesRecursive("");
    for (const std::string& file : files) {
        // ReadFileText/ToResourcePath operate on res:// paths; prefix the
        // pack-relative entry so the parsed sourcePath stays canonical.
        ProcessCandidateFile("res://" + file);
    }
}

void ArchetypeRegistry::ProcessCandidateFile(const std::string& filePath) {
    std::string extension = std::filesystem::path(filePath).extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    if (extension != ".archetype" && extension != ".lua" &&
        extension != ".rb" && extension != ".py") {
        return;
    }

    ArchetypeDefinition def = ParseFile(filePath);
    if (!def.isValid) {
        return;
    }

    if (m_NameToIndex.find(def.className) != m_NameToIndex.end()) {
        LOG_WARN(LogCategory::Core,
            "ArchetypeRegistry: Duplicate archetype '{}' in '{}', already defined in '{}'",
            def.className, def.sourcePath,
            m_Definitions[m_NameToIndex[def.className]].sourcePath);
        return;
    }

    StoreDefinition(def);
    LOG_DEBUG(LogCategory::Core, "ArchetypeRegistry: Found archetype '{}' in '{}'",
              def.className, def.sourcePath);
}

void ArchetypeRegistry::StoreDefinition(const ArchetypeDefinition& def) {
    size_t index = m_Definitions.size();
    m_Definitions.push_back(def);
    m_NameToIndex[def.className] = index;
    m_PathToIndex[def.sourcePath] = index;
}

bool ArchetypeRegistry::RescanFile(const std::string& sourcePath) {
    ArchetypeDefinition def = ParseFile(sourcePath);
    std::string resPath = ToResourcePath(sourcePath);

    std::unordered_map<std::string, size_t>::iterator pathIt = m_PathToIndex.find(resPath);
    if (pathIt != m_PathToIndex.end()) {
        size_t oldIndex = pathIt->second;
        std::string oldClassName = m_Definitions[oldIndex].className;

        if (def.isValid) {
            if (oldClassName != def.className) {
                m_NameToIndex.erase(oldClassName);
                m_NameToIndex[def.className] = oldIndex;
            }
            m_Definitions[oldIndex] = def;
            LOG_INFO(LogCategory::Core, "ArchetypeRegistry: Updated archetype '{}' from '{}'",
                     def.className, resPath);
        } else {
            RemoveDefinition(resPath);
        }

        NotifyDefinitionsChanged();
        return true;
    }

    if (def.isValid) {
        if (m_NameToIndex.find(def.className) != m_NameToIndex.end()) {
            LOG_WARN(LogCategory::Core,
                "ArchetypeRegistry: Cannot add '{}' - archetype name already exists",
                def.className);
            return false;
        }

        StoreDefinition(def);
        NotifyDefinitionsChanged();
        LOG_INFO(LogCategory::Core, "ArchetypeRegistry: Added new archetype '{}' from '{}'",
                 def.className, resPath);
        return true;
    }

    return false;
}

bool ArchetypeRegistry::RemoveDefinition(const std::string& sourcePath) {
    std::string resPath = ToResourcePath(sourcePath);

    std::unordered_map<std::string, size_t>::iterator pathIt = m_PathToIndex.find(resPath);
    if (pathIt == m_PathToIndex.end()) {
        pathIt = m_PathToIndex.find(sourcePath);
        if (pathIt == m_PathToIndex.end()) {
            return false;
        }
        resPath = sourcePath;
    }

    size_t indexToRemove = pathIt->second;
    std::string className = m_Definitions[indexToRemove].className;

    m_NameToIndex.erase(className);
    m_PathToIndex.erase(resPath);

    if (indexToRemove < m_Definitions.size() - 1) {
        std::swap(m_Definitions[indexToRemove], m_Definitions.back());
        const ArchetypeDefinition& swapped = m_Definitions[indexToRemove];
        m_NameToIndex[swapped.className] = indexToRemove;
        m_PathToIndex[swapped.sourcePath] = indexToRemove;
    }
    m_Definitions.pop_back();

    NotifyDefinitionsChanged();
    LOG_INFO(LogCategory::Core, "ArchetypeRegistry: Removed archetype '{}' from '{}'",
             className, resPath);
    return true;
}

const ArchetypeDefinition* ArchetypeRegistry::GetDefinition(const std::string& className) const {
    std::unordered_map<std::string, size_t>::const_iterator it = m_NameToIndex.find(className);
    if (it != m_NameToIndex.end()) {
        return &m_Definitions[it->second];
    }
    return nullptr;
}

const ArchetypeDefinition* ArchetypeRegistry::GetDefinitionByPath(const std::string& sourcePath) const {
    std::unordered_map<std::string, size_t>::const_iterator it = m_PathToIndex.find(sourcePath);
    if (it != m_PathToIndex.end()) {
        return &m_Definitions[it->second];
    }
    return nullptr;
}

bool ArchetypeRegistry::IsArchetype(const std::string& className) const {
    return m_NameToIndex.find(className) != m_NameToIndex.end();
}

std::vector<PropertyDescriptor> ArchetypeRegistry::GetEffectiveFields(const std::string& className) const {
    std::vector<const ArchetypeDefinition*> chain;
    std::set<std::string> visited;

    const ArchetypeDefinition* current = GetDefinition(className);
    while (current && visited.insert(current->className).second) {
        chain.push_back(current);
        if (current->baseClass.empty()) {
            break;
        }
        current = GetDefinition(current->baseClass);
    }

    std::vector<PropertyDescriptor> result;
    std::unordered_map<std::string, size_t> indexByName;

    for (std::vector<const ArchetypeDefinition*>::const_reverse_iterator it = chain.rbegin();
         it != chain.rend(); ++it) {
        for (const PropertyDescriptor& field : (*it)->fields) {
            std::unordered_map<std::string, size_t>::iterator found = indexByName.find(field.name);
            if (found != indexByName.end()) {
                result[found->second] = field;
            } else {
                indexByName[field.name] = result.size();
                result.push_back(field);
            }
        }
    }

    return result;
}

std::vector<std::string> ArchetypeRegistry::GetInheritanceChain(const std::string& className) const {
    std::vector<std::string> chain;
    std::set<std::string> visited;

    const ArchetypeDefinition* current = GetDefinition(className);
    if (!current) {
        chain.push_back(className);
        return chain;
    }

    while (current && visited.insert(current->className).second) {
        chain.push_back(current->className);
        if (current->baseClass.empty()) {
            break;
        }
        current = GetDefinition(current->baseClass);
    }

    return chain;
}

bool ArchetypeRegistry::IsSubclassOf(const std::string& className, const std::string& baseName) const {
    if (className == baseName) {
        return true;
    }
    std::vector<std::string> chain = GetInheritanceChain(className);
    for (const std::string& name : chain) {
        if (name == baseName) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> ArchetypeRegistry::GetImplementedInterfaces(const std::string& className) const {
    std::vector<std::string> result;
    std::set<std::string> seen;
    for (const std::string& cls : GetInheritanceChain(className)) {
        const ArchetypeDefinition* def = GetDefinition(cls);
        if (!def) {
            continue;
        }
        for (const std::string& iface : def->implementsInterfaces) {
            if (seen.insert(iface).second) {
                result.push_back(iface);
            }
        }
    }
    return result;
}

bool ArchetypeRegistry::ArchetypeImplementsInterface(const std::string& className,
                                                     const std::string& interfaceName) const {
    InterfaceRegistry& interfaceRegistry = InterfaceRegistry::GetInstance();
    for (const std::string& declared : GetImplementedInterfaces(className)) {
        if (interfaceRegistry.IsSubInterfaceOf(declared, interfaceName)) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> ArchetypeRegistry::GetArchetypesImplementing(const std::string& interfaceName) const {
    std::vector<std::string> result;
    for (const ArchetypeDefinition& def : m_Definitions) {
        if (ArchetypeImplementsInterface(def.className, interfaceName)) {
            result.push_back(def.className);
        }
    }
    return result;
}

nlohmann::json ArchetypeRegistry::BuildEffectiveDefaultValues(const std::string& className) const {
    nlohmann::json values = nlohmann::json::object();
    for (const PropertyDescriptor& field : GetEffectiveFields(className)) {
        values[field.name] = field.GetDefaultAsJson();
    }
    return values;
}

nlohmann::json ArchetypeRegistry::BuildEffectivePropertyMetadata(const std::string& className) const {
    nlohmann::json metadata = nlohmann::json::object();
    for (const PropertyDescriptor& field : GetEffectiveFields(className)) {
        metadata[field.name] = field.Serialize();
    }
    return metadata;
}

nlohmann::json ArchetypeRegistry::BuildEffectiveFieldsJson(const std::string& className) const {
    nlohmann::json fields = nlohmann::json::array();
    for (const PropertyDescriptor& field : GetEffectiveFields(className)) {
        fields.push_back(field.Serialize());
    }
    return fields;
}

ArchetypeDefinition ArchetypeRegistry::ParseDefinitionFile(const std::string& filePath) {
    return ParseFile(filePath);
}

bool ArchetypeRegistry::RegisterParsedDefinition(const ArchetypeDefinition& def) {
    if (!def.isValid) {
        return false;
    }

    std::unordered_map<std::string, size_t>::iterator pathIt = m_PathToIndex.find(def.sourcePath);
    if (pathIt != m_PathToIndex.end()) {
        size_t oldIndex = pathIt->second;
        std::string oldClassName = m_Definitions[oldIndex].className;

        if (oldClassName != def.className) {
            std::unordered_map<std::string, size_t>::iterator nameIt = m_NameToIndex.find(def.className);
            if (nameIt != m_NameToIndex.end() && nameIt->second != oldIndex) {
                LOG_WARN(LogCategory::Core,
                    "ArchetypeRegistry: Cannot update '{}' to class '{}' - name already used by '{}'",
                    def.sourcePath, def.className, m_Definitions[nameIt->second].sourcePath);
                return false;
            }
            m_NameToIndex.erase(oldClassName);
            m_NameToIndex[def.className] = oldIndex;
        }

        m_Definitions[oldIndex] = def;
        NotifyDefinitionsChanged();
        LOG_INFO(LogCategory::Core, "ArchetypeRegistry: Updated archetype '{}' from '{}' (async)",
                 def.className, def.sourcePath);
        return true;
    }

    if (m_NameToIndex.find(def.className) != m_NameToIndex.end()) {
        LOG_WARN(LogCategory::Core,
            "ArchetypeRegistry: Cannot add '{}' - archetype name already exists",
            def.className);
        return false;
    }

    StoreDefinition(def);
    NotifyDefinitionsChanged();
    LOG_INFO(LogCategory::Core, "ArchetypeRegistry: Added new archetype '{}' from '{}' (async)",
             def.className, def.sourcePath);
    return true;
}

void ArchetypeRegistry::Clear() {
    m_Definitions.clear();
    m_NameToIndex.clear();
    m_PathToIndex.clear();
}

ArchetypeDefinition ArchetypeRegistry::ParseFile(const std::string& filePath) {
    std::string extension = std::filesystem::path(filePath).extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    ArchetypeDefinition def;
    if (extension == ".archetype") {
        def = ParseDataFile(filePath);
    } else {
        def = ParseScriptFile(filePath);
    }

    if (def.isValid) {
        try {
            def.lastModified = std::filesystem::last_write_time(filePath);
        } catch (...) {
        }
    }
    return def;
}

ArchetypeDefinition ArchetypeRegistry::ParseDataFile(const std::string& filePath) {
    ArchetypeDefinition def;
    def.source = ArchetypeSource::DataFile;
    def.sourcePath = ToResourcePath(filePath);

    std::string contents;
    if (!ReadFileText(filePath, contents)) {
        def.parseError = "Could not open file: " + filePath;
        return def;
    }

    try {
        nlohmann::json json = nlohmann::json::parse(contents);
        def = ArchetypeDefinition::Deserialize(json, ArchetypeSource::DataFile,
                                               ToResourcePath(filePath));
    } catch (const std::exception& e) {
        def.parseError = std::string("JSON parse error: ") + e.what();
        def.isValid = false;
    }

    return def;
}

ArchetypeDefinition ArchetypeRegistry::ParseScriptFile(const std::string& filePath) {
    ArchetypeDefinition def;
    def.source = ArchetypeSource::Script;
    def.sourcePath = ToResourcePath(filePath);

    std::string extension = std::filesystem::path(filePath).extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    def.language = GetLanguageFromExtension(extension);

    std::string contents;
    if (!ReadFileText(filePath, contents)) {
        def.parseError = "Could not open file: " + filePath;
        return def;
    }

    std::string commentPrefix = (def.language == scripting::ScriptLanguage::Lua) ? "--" : "#";

    ParseDirectives(contents, commentPrefix, def);
    if (def.className.empty()) {
        return def;
    }

    ParseExportProperties(contents, commentPrefix, def);
    def.isValid = true;
    return def;
}

void ArchetypeRegistry::ParseDirectives(const std::string& content, const std::string& commentPrefix,
                                        ArchetypeDefinition& def) {
    std::regex classRegex(commentPrefix + R"(@archetype_class\s+[\"']([A-Za-z_]\w*)[\"'])");
    std::regex extendsRegex(commentPrefix + R"(@archetype_extends\s+[\"']([A-Za-z_]\w*)[\"'])");
    std::regex abstractRegex(commentPrefix + R"(@archetype_abstract\b)");
    std::regex menuRegex(commentPrefix + R"(@archetype_menu\s+[\"']([^\"']+)[\"'])");
    std::regex iconRegex(commentPrefix + R"(@archetype_icon\s+[\"']([^\"']+)[\"'])");
    std::regex descRegex(commentPrefix + R"(@archetype_description\s+[\"']([^\"']*)[\"'])");

    std::smatch match;
    if (std::regex_search(content, match, classRegex)) {
        def.className = match[1].str();
    }
    if (std::regex_search(content, match, extendsRegex)) {
        def.baseClass = match[1].str();
    }
    if (std::regex_search(content, abstractRegex)) {
        def.isAbstract = true;
    }
    if (std::regex_search(content, match, menuRegex)) {
        def.menuPath = match[1].str();
    }
    if (std::regex_search(content, match, iconRegex)) {
        def.iconPath = match[1].str();
    }
    if (std::regex_search(content, match, descRegex)) {
        def.description = match[1].str();
    }

    // @archetype_implements may list several interfaces (quoted or bare,
    // whitespace/comma separated) and may appear on multiple lines.
    std::regex implementsRegex(commentPrefix + R"(@archetype_implements\s+([^\r\n]+))");
    std::regex nameRegex(R"([\"']([A-Za-z_]\w*)[\"']|([A-Za-z_]\w*))");
    std::sregex_iterator implBegin(content.begin(), content.end(), implementsRegex);
    std::sregex_iterator implEnd;
    for (std::sregex_iterator it = implBegin; it != implEnd; ++it) {
        std::string tail = (*it)[1].str();
        std::sregex_iterator nameBegin(tail.begin(), tail.end(), nameRegex);
        std::sregex_iterator nameEnd;
        for (std::sregex_iterator nit = nameBegin; nit != nameEnd; ++nit) {
            std::string name = (*nit)[1].matched ? (*nit)[1].str() : (*nit)[2].str();
            if (!name.empty() &&
                std::find(def.implementsInterfaces.begin(), def.implementsInterfaces.end(), name) ==
                    def.implementsInterfaces.end()) {
                def.implementsInterfaces.push_back(name);
            }
        }
    }
}

void ArchetypeRegistry::ParseExportProperties(const std::string& content, const std::string& commentPrefix,
                                              ArchetypeDefinition& def) {
    // Field parsing is delegated to the shared annotation parser so archetype
    // scripts and component scripts share one grammar (block-annotation tags,
    // bracket + legacy attributes, @struct types, the full attribute vocabulary).
    ScriptExportParseResult parsed = ScriptAnnotationParser::ParseExports(content, commentPrefix);
    for (const PropertyDescriptor& descriptor : parsed.properties) {
        def.fields.push_back(descriptor);
    }
}

bool ArchetypeRegistry::ReadFileText(const std::string& filePath, std::string& outContents) {
    platform::PackFileSystem& packFS = platform::PackFileSystem::Instance();
    if (packFS.isPackMode() && packFS.exists(filePath)) {
        outContents = packFS.readFileAsString(filePath);
        return !outContents.empty();
    }

    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    outContents = buffer.str();
    return true;
}

scripting::ScriptLanguage ArchetypeRegistry::GetLanguageFromExtension(const std::string& extension) {
    if (extension == ".lua") {
        return scripting::ScriptLanguage::Lua;
    } else if (extension == ".rb") {
        return scripting::ScriptLanguage::MRuby;
    } else if (extension == ".py") {
        return scripting::ScriptLanguage::MicroPython;
    }
    return scripting::ScriptLanguage::Lua;
}

std::string ArchetypeRegistry::ToResourcePath(const std::string& absolutePath) const {
    if (absolutePath.rfind("res://", 0) == 0) {
        return absolutePath;
    }
    if (m_ProjectPath.empty()) {
        return absolutePath;
    }

    std::filesystem::path absPath(absolutePath);
    std::filesystem::path projPath(m_ProjectPath);

    try {
        std::filesystem::path relPath = std::filesystem::relative(absPath, projPath);
        return "res://" + relPath.generic_string();
    } catch (...) {
        return absolutePath;
    }
}

void ArchetypeRegistry::NotifyDefinitionsChanged() {
    if (m_OnDefinitionsChanged) {
        m_OnDefinitionsChanged();
    }
}

} // namespace core
} // namespace lupine
