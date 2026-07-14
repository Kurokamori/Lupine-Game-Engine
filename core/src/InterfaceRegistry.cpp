#include "lupine/core/InterfaceRegistry.hpp"
#include "lupine/platform/PackFile.hpp"
#include "lupine/logger/Logger.hpp"
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <cctype>
#include <set>

namespace lupine {
namespace core {

namespace {

/**
 * Map a textual type name (as written in an @interface_method / @interface_signal
 * argument list) to a PropertyValueType. Mirrors ScriptComponent::ParseSignals.
 */
PropertyValueType TypeFromName(const std::string& raw) {
    std::string t = raw;
    std::transform(t.begin(), t.end(), t.begin(), ::tolower);
    if (t == "int" || t == "integer") return PropertyValueType::Int;
    if (t == "bool" || t == "boolean") return PropertyValueType::Bool;
    if (t == "string" || t == "str") return PropertyValueType::String;
    if (t == "vec2") return PropertyValueType::Vec2;
    if (t == "vec3") return PropertyValueType::Vec3;
    if (t == "vec4") return PropertyValueType::Vec4;
    if (t == "color") return PropertyValueType::Color;
    if (t == "double") return PropertyValueType::Double;
    return PropertyValueType::Float;
}

/**
 * Parse a parenthesised argument list like "float amount, int count" into typed
 * argument descriptors. A bare token is treated as the argument name (untyped).
 */
std::vector<SignalArgDesc> ParseArgList(const std::string& argText) {
    std::vector<SignalArgDesc> args;
    std::stringstream ss(argText);
    std::string item;
    while (std::getline(ss, item, ',')) {
        // Trim
        size_t start = item.find_first_not_of(" \t");
        size_t end = item.find_last_not_of(" \t");
        if (start == std::string::npos) {
            continue;
        }
        std::string token = item.substr(start, end - start + 1);

        SignalArgDesc arg;
        size_t space = token.find_first_of(" \t");
        if (space != std::string::npos) {
            std::string typePart = token.substr(0, space);
            std::string namePart = token.substr(space + 1);
            size_t ns = namePart.find_first_not_of(" \t");
            if (ns != std::string::npos) {
                namePart = namePart.substr(ns);
            }
            arg.type = TypeFromName(typePart);
            arg.name = namePart;
        } else {
            arg.name = token;
            arg.type = PropertyValueType::Float;
        }
        args.push_back(arg);
    }
    return args;
}

} // namespace

InterfaceRegistry& InterfaceRegistry::GetInstance() {
    static InterfaceRegistry instance;
    return instance;
}

// ----------------------------------------------------------------------------
// Discovery / lifecycle
// ----------------------------------------------------------------------------

void InterfaceRegistry::ScanProject(const std::string& projectPath) {
    m_ProjectPath = projectPath;
    Clear();

    platform::PackFileSystem& packFS = platform::PackFileSystem::Instance();
    if (packFS.isPackMode()) {
        ScanPack();
    } else if (std::filesystem::exists(projectPath)) {
        ScanDirectory(projectPath);
    }

    NotifyDefinitionsChanged();

    LOG_INFO(LogCategory::Core, "InterfaceRegistry: Scanned project, {} interfaces total",
             m_Definitions.size());
}

bool InterfaceRegistry::IsCandidateFile(const std::string& filePath) {
    std::string extension = std::filesystem::path(filePath).extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    return extension == ".interface" || extension == ".lua" ||
           extension == ".rb" || extension == ".py";
}

void InterfaceRegistry::RegisterInterfaceFile(const std::string& filePath) {
    InterfaceDefinition def = ParseFile(filePath);
    if (!def.isValid) {
        return;
    }

    if (m_NameToIndex.find(def.name) != m_NameToIndex.end()) {
        LOG_WARN(LogCategory::Core,
            "InterfaceRegistry: Duplicate interface '{}' in '{}', already defined in '{}'",
            def.name, def.sourcePath,
            m_Definitions[m_NameToIndex[def.name]].sourcePath);
        return;
    }

    StoreDefinition(def);
    LOG_DEBUG(LogCategory::Core, "InterfaceRegistry: Found interface '{}' in '{}'",
              def.name, def.sourcePath);
}

void InterfaceRegistry::ScanPack() {
    platform::PackFileSystem& packFS = platform::PackFileSystem::Instance();

    // ParseFile/ToResourcePath operate on res:// paths; prefix the pack-relative entry
    // so the recorded sourcePath stays canonical.
    const std::vector<std::string> files = packFS.listFilesRecursive("");
    for (const std::string& file : files) {
        if (IsCandidateFile(file)) {
            RegisterInterfaceFile("res://" + file);
        }
    }
}

void InterfaceRegistry::ScanDirectory(const std::string& directory) {
    try {
        for (const std::filesystem::directory_entry& entry :
                 std::filesystem::recursive_directory_iterator(directory)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            if (!IsCandidateFile(entry.path().string())) {
                continue;
            }

            RegisterInterfaceFile(entry.path().string());
        }
    } catch (const std::filesystem::filesystem_error& e) {
        LOG_ERROR(LogCategory::Core, "InterfaceRegistry: Error scanning directory '{}': {}",
                  directory, e.what());
    }
}

void InterfaceRegistry::StoreDefinition(const InterfaceDefinition& def) {
    size_t index = m_Definitions.size();
    m_Definitions.push_back(def);
    m_NameToIndex[def.name] = index;
    if (!def.sourcePath.empty()) {
        m_PathToIndex[def.sourcePath] = index;
    }
}

void InterfaceRegistry::RebuildIndices() {
    m_NameToIndex.clear();
    m_PathToIndex.clear();
    for (size_t i = 0; i < m_Definitions.size(); ++i) {
        m_NameToIndex[m_Definitions[i].name] = i;
        if (!m_Definitions[i].sourcePath.empty()) {
            m_PathToIndex[m_Definitions[i].sourcePath] = i;
        }
    }
}

void InterfaceRegistry::ApplyPersistentDefinitions() {
    for (const InterfaceDefinition& def : m_PersistentDefinitions) {
        if (m_NameToIndex.find(def.name) == m_NameToIndex.end()) {
            StoreDefinition(def);
        }
    }
}

void InterfaceRegistry::Clear() {
    m_Definitions.clear();
    m_NameToIndex.clear();
    m_PathToIndex.clear();
    ApplyPersistentDefinitions();
}

bool InterfaceRegistry::RescanFile(const std::string& sourcePath) {
    InterfaceDefinition def = ParseFile(sourcePath);
    std::string resPath = ToResourcePath(sourcePath);

    std::unordered_map<std::string, size_t>::iterator pathIt = m_PathToIndex.find(resPath);
    if (pathIt != m_PathToIndex.end()) {
        size_t oldIndex = pathIt->second;
        std::string oldName = m_Definitions[oldIndex].name;

        if (def.isValid) {
            if (oldName != def.name) {
                m_NameToIndex.erase(oldName);
                m_NameToIndex[def.name] = oldIndex;
            }
            m_Definitions[oldIndex] = def;
            LOG_INFO(LogCategory::Core, "InterfaceRegistry: Updated interface '{}' from '{}'",
                     def.name, resPath);
        } else {
            RemoveDefinition(resPath);
        }

        NotifyDefinitionsChanged();
        return true;
    }

    if (def.isValid) {
        if (m_NameToIndex.find(def.name) != m_NameToIndex.end()) {
            LOG_WARN(LogCategory::Core,
                "InterfaceRegistry: Cannot add '{}' - interface name already exists", def.name);
            return false;
        }
        StoreDefinition(def);
        NotifyDefinitionsChanged();
        LOG_INFO(LogCategory::Core, "InterfaceRegistry: Added new interface '{}' from '{}'",
                 def.name, resPath);
        return true;
    }

    return false;
}

bool InterfaceRegistry::RemoveDefinition(const std::string& sourcePath) {
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
    std::string name = m_Definitions[indexToRemove].name;

    m_Definitions.erase(m_Definitions.begin() + indexToRemove);
    RebuildIndices();

    NotifyDefinitionsChanged();
    LOG_INFO(LogCategory::Core, "InterfaceRegistry: Removed interface '{}' from '{}'", name, resPath);
    return true;
}

InterfaceDefinition InterfaceRegistry::ParseDefinitionFile(const std::string& filePath) {
    return ParseFile(filePath);
}

bool InterfaceRegistry::RegisterParsedDefinition(const InterfaceDefinition& def) {
    if (!def.isValid) {
        return false;
    }

    std::unordered_map<std::string, size_t>::iterator pathIt = m_PathToIndex.find(def.sourcePath);
    if (pathIt != m_PathToIndex.end()) {
        size_t oldIndex = pathIt->second;
        std::string oldName = m_Definitions[oldIndex].name;
        if (oldName != def.name) {
            std::unordered_map<std::string, size_t>::iterator nameIt = m_NameToIndex.find(def.name);
            if (nameIt != m_NameToIndex.end() && nameIt->second != oldIndex) {
                LOG_WARN(LogCategory::Core,
                    "InterfaceRegistry: Cannot update '{}' to name '{}' - already used by '{}'",
                    def.sourcePath, def.name, m_Definitions[nameIt->second].sourcePath);
                return false;
            }
            m_NameToIndex.erase(oldName);
            m_NameToIndex[def.name] = oldIndex;
        }
        m_Definitions[oldIndex] = def;
        NotifyDefinitionsChanged();
        return true;
    }

    if (m_NameToIndex.find(def.name) != m_NameToIndex.end()) {
        LOG_WARN(LogCategory::Core,
            "InterfaceRegistry: Cannot add '{}' - interface name already exists", def.name);
        return false;
    }

    StoreDefinition(def);
    NotifyDefinitionsChanged();
    return true;
}

bool InterfaceRegistry::RegisterNativeInterface(const InterfaceDefinition& defIn) {
    InterfaceDefinition def = defIn;
    def.source = InterfaceSource::Native;
    def.sourcePath.clear();
    def.isValid = !def.name.empty();
    if (!def.isValid) {
        return false;
    }

    // Upsert into the persistent store (survives ScanProject/Clear).
    bool replacedPersistent = false;
    for (InterfaceDefinition& existing : m_PersistentDefinitions) {
        if (existing.name == def.name) {
            existing = def;
            replacedPersistent = true;
            break;
        }
    }
    if (!replacedPersistent) {
        m_PersistentDefinitions.push_back(def);
    }

    // Reflect into the live table.
    std::unordered_map<std::string, size_t>::iterator nameIt = m_NameToIndex.find(def.name);
    if (nameIt != m_NameToIndex.end()) {
        m_Definitions[nameIt->second] = def;
    } else {
        StoreDefinition(def);
    }

    NotifyDefinitionsChanged();
    return true;
}

bool InterfaceRegistry::RegisterRuntimeInterface(const InterfaceDefinition& defIn) {
    InterfaceDefinition def = defIn;
    if (def.source == InterfaceSource::Native) {
        // Keep Native classification if the caller chose it; otherwise mark as a
        // runtime/script definition while still persisting it.
    }
    def.isValid = !def.name.empty();
    if (!def.isValid) {
        return false;
    }

    bool replacedPersistent = false;
    for (InterfaceDefinition& existing : m_PersistentDefinitions) {
        if (existing.name == def.name) {
            existing = def;
            replacedPersistent = true;
            break;
        }
    }
    if (!replacedPersistent) {
        m_PersistentDefinitions.push_back(def);
    }

    std::unordered_map<std::string, size_t>::iterator nameIt = m_NameToIndex.find(def.name);
    if (nameIt != m_NameToIndex.end()) {
        m_Definitions[nameIt->second] = def;
    } else {
        StoreDefinition(def);
    }

    NotifyDefinitionsChanged();
    return true;
}

// ----------------------------------------------------------------------------
// Definition queries
// ----------------------------------------------------------------------------

const InterfaceDefinition* InterfaceRegistry::GetDefinition(const std::string& name) const {
    std::unordered_map<std::string, size_t>::const_iterator it = m_NameToIndex.find(name);
    if (it != m_NameToIndex.end()) {
        return &m_Definitions[it->second];
    }
    return nullptr;
}

const InterfaceDefinition* InterfaceRegistry::GetDefinitionByPath(const std::string& sourcePath) const {
    std::unordered_map<std::string, size_t>::const_iterator it = m_PathToIndex.find(sourcePath);
    if (it != m_PathToIndex.end()) {
        return &m_Definitions[it->second];
    }
    return nullptr;
}

bool InterfaceRegistry::IsInterface(const std::string& name) const {
    return m_NameToIndex.find(name) != m_NameToIndex.end();
}

std::vector<std::string> InterfaceRegistry::GetInterfaceNames() const {
    std::vector<std::string> names;
    names.reserve(m_Definitions.size());
    for (const InterfaceDefinition& def : m_Definitions) {
        names.push_back(def.name);
    }
    return names;
}

std::vector<std::string> InterfaceRegistry::GetInheritanceChain(const std::string& name) const {
    std::vector<std::string> chain;
    std::set<std::string> visited;

    // Depth-first walk over base interfaces, de-duplicated and cycle-safe.
    std::function<void(const std::string&)> walk = [&](const std::string& current) {
        if (!visited.insert(current).second) {
            return;
        }
        chain.push_back(current);
        const InterfaceDefinition* def = GetDefinition(current);
        if (!def) {
            return;
        }
        for (const std::string& base : def->baseInterfaces) {
            walk(base);
        }
    };

    walk(name);
    return chain;
}

bool InterfaceRegistry::IsSubInterfaceOf(const std::string& name, const std::string& baseName) const {
    if (name == baseName) {
        return true;
    }
    std::vector<std::string> chain = GetInheritanceChain(name);
    for (const std::string& n : chain) {
        if (n == baseName) {
            return true;
        }
    }
    return false;
}

std::vector<InterfaceMethod> InterfaceRegistry::GetEffectiveMethods(const std::string& name) const {
    std::vector<std::string> chain = GetInheritanceChain(name);

    std::vector<InterfaceMethod> result;
    std::unordered_map<std::string, size_t> indexByName;

    // Base contributions first so a derived method of the same name overrides.
    for (std::vector<std::string>::const_reverse_iterator it = chain.rbegin();
         it != chain.rend(); ++it) {
        const InterfaceDefinition* def = GetDefinition(*it);
        if (!def) {
            continue;
        }
        for (const InterfaceMethod& method : def->methods) {
            std::unordered_map<std::string, size_t>::iterator found = indexByName.find(method.name);
            if (found != indexByName.end()) {
                result[found->second] = method;
            } else {
                indexByName[method.name] = result.size();
                result.push_back(method);
            }
        }
    }

    return result;
}

std::vector<SignalDesc> InterfaceRegistry::GetEffectiveSignals(const std::string& name) const {
    std::vector<std::string> chain = GetInheritanceChain(name);

    std::vector<SignalDesc> result;
    std::unordered_map<std::string, size_t> indexByName;

    for (std::vector<std::string>::const_reverse_iterator it = chain.rbegin();
         it != chain.rend(); ++it) {
        const InterfaceDefinition* def = GetDefinition(*it);
        if (!def) {
            continue;
        }
        for (const SignalDesc& sig : def->signals) {
            std::unordered_map<std::string, size_t>::iterator found = indexByName.find(sig.name);
            if (found != indexByName.end()) {
                result[found->second] = sig;
            } else {
                indexByName[sig.name] = result.size();
                result.push_back(sig);
            }
        }
    }

    return result;
}

std::vector<std::string> InterfaceRegistry::ExpandImplied(const std::vector<std::string>& declared) const {
    std::vector<std::string> result;
    std::set<std::string> seen;
    for (const std::string& name : declared) {
        for (const std::string& implied : GetInheritanceChain(name)) {
            if (seen.insert(implied).second) {
                result.push_back(implied);
            }
        }
    }
    return result;
}

// ----------------------------------------------------------------------------
// Native component type conformance
// ----------------------------------------------------------------------------

void InterfaceRegistry::RegisterTypeConformance(const std::string& typeName,
                                                const std::string& interfaceName) {
    if (typeName.empty() || interfaceName.empty()) {
        return;
    }
    m_TypeToInterfaces[typeName].insert(interfaceName);
    m_InterfaceToTypes[interfaceName].insert(typeName);
}

std::vector<std::string> InterfaceRegistry::GetTypeInterfaces(const std::string& typeName) const {
    std::vector<std::string> result;
    std::unordered_map<std::string, std::unordered_set<std::string>>::const_iterator it =
        m_TypeToInterfaces.find(typeName);
    if (it != m_TypeToInterfaces.end()) {
        result.assign(it->second.begin(), it->second.end());
    }
    return result;
}

bool InterfaceRegistry::TypeImplementsInterface(const std::string& typeName,
                                                const std::string& interfaceName) const {
    std::unordered_map<std::string, std::unordered_set<std::string>>::const_iterator it =
        m_TypeToInterfaces.find(typeName);
    if (it == m_TypeToInterfaces.end()) {
        return false;
    }
    for (const std::string& declared : it->second) {
        if (IsSubInterfaceOf(declared, interfaceName)) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> InterfaceRegistry::GetTypesImplementing(const std::string& interfaceName) const {
    std::vector<std::string> result;
    for (const std::pair<const std::string, std::unordered_set<std::string>>& entry : m_TypeToInterfaces) {
        for (const std::string& declared : entry.second) {
            if (IsSubInterfaceOf(declared, interfaceName)) {
                result.push_back(entry.first);
                break;
            }
        }
    }
    return result;
}

// ----------------------------------------------------------------------------
// Contract verification
// ----------------------------------------------------------------------------

nlohmann::json InterfaceRegistry::VerifyMembers(
        const std::string& interfaceName,
        const std::unordered_set<std::string>& presentMethods,
        const std::unordered_set<std::string>& presentSignals) const {
    nlohmann::json result;
    result["interface"] = interfaceName;

    const InterfaceDefinition* def = GetDefinition(interfaceName);
    if (!def) {
        result["exists"] = false;
        result["conforms"] = false;
        result["missing_methods"] = nlohmann::json::array();
        result["missing_signals"] = nlohmann::json::array();
        return result;
    }
    result["exists"] = true;

    nlohmann::json missingMethods = nlohmann::json::array();
    for (const InterfaceMethod& method : GetEffectiveMethods(interfaceName)) {
        if (presentMethods.find(method.name) == presentMethods.end()) {
            missingMethods.push_back(method.name);
        }
    }

    nlohmann::json missingSignals = nlohmann::json::array();
    for (const SignalDesc& sig : GetEffectiveSignals(interfaceName)) {
        if (presentSignals.find(sig.name) == presentSignals.end()) {
            missingSignals.push_back(sig.name);
        }
    }

    result["missing_methods"] = missingMethods;
    result["missing_signals"] = missingSignals;
    result["conforms"] = missingMethods.empty() && missingSignals.empty();
    return result;
}

// ----------------------------------------------------------------------------
// Parsing
// ----------------------------------------------------------------------------

InterfaceDefinition InterfaceRegistry::ParseFile(const std::string& filePath) {
    std::string extension = std::filesystem::path(filePath).extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    InterfaceDefinition def;
    if (extension == ".interface") {
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

InterfaceDefinition InterfaceRegistry::ParseDataFile(const std::string& filePath) {
    InterfaceDefinition def;
    def.source = InterfaceSource::DataFile;
    def.sourcePath = ToResourcePath(filePath);

    std::string contents;
    if (!ReadFileText(filePath, contents)) {
        def.parseError = "Could not open file: " + filePath;
        return def;
    }

    try {
        nlohmann::json json = nlohmann::json::parse(contents);
        def = InterfaceDefinition::Deserialize(json, InterfaceSource::DataFile,
                                               ToResourcePath(filePath));
    } catch (const std::exception& e) {
        def.parseError = std::string("JSON parse error: ") + e.what();
        def.isValid = false;
    }

    return def;
}

InterfaceDefinition InterfaceRegistry::ParseScriptFile(const std::string& filePath) {
    InterfaceDefinition def;
    def.source = InterfaceSource::Script;
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
    ParseScriptDefineDirectives(contents, commentPrefix, def);

    def.isValid = !def.name.empty();
    return def;
}

void InterfaceRegistry::ParseScriptDefineDirectives(const std::string& content,
                                                    const std::string& commentPrefix,
                                                    InterfaceDefinition& def) {
    // A script DEFINES an interface with @interface_define plus member directives.
    // (The separate @interface annotation that declares a script IMPLEMENTS an
    // interface is handled by ScriptComponent, not here.)
    std::regex defineRegex(commentPrefix + R"(\s*@interface_define\s+[\"']([A-Za-z_]\w*)[\"'])");
    std::regex extendsRegex(commentPrefix + R"(\s*@interface_extends\s+[\"']([A-Za-z_][\w,\s]*)[\"'])");
    std::regex descRegex(commentPrefix + R"(\s*@interface_description\s+[\"']([^\"']*)[\"'])");
    std::regex methodRegex(commentPrefix + R"(\s*@interface_method\s+(\w+)\s*(?:\(([^)]*)\))?)");
    std::regex signalRegex(commentPrefix + R"(\s*@interface_signal\s+(\w+)\s*(?:\(([^)]*)\))?)");
    std::regex tagRegex(commentPrefix + R"(\s*@interface_tag\s+([\w,\s]+))");

    std::smatch match;
    if (std::regex_search(content, match, defineRegex)) {
        def.name = match[1].str();
    } else {
        return;
    }

    if (std::regex_search(content, match, extendsRegex)) {
        std::stringstream ss(match[1].str());
        std::string base;
        while (std::getline(ss, base, ',')) {
            size_t s = base.find_first_not_of(" \t");
            size_t e = base.find_last_not_of(" \t");
            if (s != std::string::npos) {
                def.baseInterfaces.push_back(base.substr(s, e - s + 1));
            }
        }
    }
    if (std::regex_search(content, match, descRegex)) {
        def.description = match[1].str();
    }

    std::sregex_iterator methodBegin(content.begin(), content.end(), methodRegex);
    std::sregex_iterator iterEnd;
    for (std::sregex_iterator it = methodBegin; it != iterEnd; ++it) {
        InterfaceMethod method;
        method.name = (*it)[1].str();
        if ((*it)[2].matched) {
            method.params = ParseArgList((*it)[2].str());
        }
        def.methods.push_back(method);
    }

    std::sregex_iterator signalBegin(content.begin(), content.end(), signalRegex);
    for (std::sregex_iterator it = signalBegin; it != iterEnd; ++it) {
        SignalDesc sig;
        sig.name = (*it)[1].str();
        if ((*it)[2].matched) {
            sig.args = ParseArgList((*it)[2].str());
        }
        def.signals.push_back(sig);
    }

    std::sregex_iterator tagBegin(content.begin(), content.end(), tagRegex);
    for (std::sregex_iterator it = tagBegin; it != iterEnd; ++it) {
        std::stringstream ss((*it)[1].str());
        std::string tag;
        while (std::getline(ss, tag, ',')) {
            size_t s = tag.find_first_not_of(" \t");
            size_t e = tag.find_last_not_of(" \t");
            if (s != std::string::npos) {
                def.tags.push_back(tag.substr(s, e - s + 1));
            }
        }
    }
}

bool InterfaceRegistry::ReadFileText(const std::string& filePath, std::string& outContents) {
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

scripting::ScriptLanguage InterfaceRegistry::GetLanguageFromExtension(const std::string& extension) {
    if (extension == ".lua") {
        return scripting::ScriptLanguage::Lua;
    } else if (extension == ".rb") {
        return scripting::ScriptLanguage::MRuby;
    } else if (extension == ".py") {
        return scripting::ScriptLanguage::MicroPython;
    }
    return scripting::ScriptLanguage::Lua;
}

std::string InterfaceRegistry::ToResourcePath(const std::string& absolutePath) const {
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

void InterfaceRegistry::NotifyDefinitionsChanged() {
    if (m_OnDefinitionsChanged) {
        m_OnDefinitionsChanged();
    }
}

} // namespace core
} // namespace lupine
