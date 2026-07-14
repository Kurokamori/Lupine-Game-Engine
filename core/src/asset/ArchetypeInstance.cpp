#include "lupine/asset/ArchetypeInstance.hpp"
#include "lupine/core/ArchetypeRegistry.hpp"
#include "lupine/core/PropertyDescriptor.hpp"
#include "lupine/platform/FileSystem.hpp"
#include "lupine/platform/PackFile.hpp"
#include "lupine/logger/Logger.hpp"

namespace lupine {
namespace asset {

using namespace math;

ArchetypeInstance::ArchetypeInstance()
    : Asset() {
}

ArchetypeInstance::ArchetypeInstance(const core::UUID& uuid)
    : Asset(uuid) {
}

ArchetypeInstance::~ArchetypeInstance() {
}

bool ArchetypeInstance::LoadFromFile(const std::string& filepath) {
    std::string contents;
    if (!ReadFileContents(filepath, contents)) {
        return false;
    }
    if (!ParseFromString(filepath, contents)) {
        return false;
    }
    FinalizeResolve();
    return true;
}

bool ArchetypeInstance::ReadFileContents(const std::string& filepath, std::string& outContents) {
    platform::PackFileSystem& packFS = platform::PackFileSystem::Instance();
    if (packFS.isPackMode() && packFS.exists(filepath)) {
        outContents = packFS.readFileAsString(filepath);
        return !outContents.empty();
    }

    std::string physical = filepath;
    if (!platform::FileSystem::Exists(physical)) {
        physical = Asset::ResolveAssetPath(filepath);
    }
    platform::FileResult<std::string> result = platform::FileSystem::ReadFile(physical);
    if (!result.success) {
        return false;
    }
    outContents = std::move(result.data);
    return true;
}

bool ArchetypeInstance::ParseFromString(const std::string& filepath, const std::string& contents) {
    SetPath(filepath);

    try {
        nlohmann::json json = nlohmann::json::parse(contents);

        if (json.contains("archetype_class") && json["archetype_class"].is_string()) {
            m_ClassName = json["archetype_class"].get<std::string>();
        }
        if (json.contains("definition") && json["definition"].is_string()) {
            m_DefinitionPath = json["definition"].get<std::string>();
        }
        if (json.contains("definition_uuid") && json["definition_uuid"].is_string()) {
            m_DefinitionUUID = json["definition_uuid"].get<std::string>();
        }
        if (json.contains("fields") && json["fields"].is_object()) {
            m_Fields = json["fields"];
        } else {
            m_Fields = nlohmann::json::object();
        }

        return true;

    } catch (const std::exception& e) {
        LOG_ERROR(LogCategory::Core, "ArchetypeInstance: Failed to parse '{}': {}", filepath, e.what());
        return false;
    }
}

void ArchetypeInstance::FinalizeResolve() {
    Resolve();
    SetLoaded(true);
}

void ArchetypeInstance::Resolve() {
    m_Resolved = nlohmann::json::object();

    std::vector<core::PropertyDescriptor> fields =
        core::ArchetypeRegistry::GetInstance().GetEffectiveFields(m_ClassName);

    for (const core::PropertyDescriptor& field : fields) {
        if (m_Fields.contains(field.name) && !m_Fields[field.name].is_null()) {
            m_Resolved[field.name] = m_Fields[field.name];
        } else {
            m_Resolved[field.name] = field.GetDefaultAsJson();
        }
    }

    for (nlohmann::json::const_iterator it = m_Fields.begin(); it != m_Fields.end(); ++it) {
        if (!m_Resolved.contains(it.key())) {
            m_Resolved[it.key()] = it.value();
        }
    }
}

std::vector<std::string> ArchetypeInstance::GetArchetypeChain() const {
    return core::ArchetypeRegistry::GetInstance().GetInheritanceChain(m_ClassName);
}

bool ArchetypeInstance::IsArchetype(const std::string& className) const {
    return core::ArchetypeRegistry::GetInstance().IsSubclassOf(m_ClassName, className);
}

std::vector<std::string> ArchetypeInstance::GetImplementedInterfaces() const {
    return core::ArchetypeRegistry::GetInstance().GetImplementedInterfaces(m_ClassName);
}

bool ArchetypeInstance::ImplementsInterface(const std::string& interfaceName) const {
    return core::ArchetypeRegistry::GetInstance().ArchetypeImplementsInterface(m_ClassName, interfaceName);
}

bool ArchetypeInstance::HasField(const std::string& name) const {
    return m_Resolved.contains(name);
}

std::vector<std::string> ArchetypeInstance::GetFieldNames() const {
    std::vector<std::string> names;
    names.reserve(m_Resolved.size());
    for (nlohmann::json::const_iterator it = m_Resolved.begin(); it != m_Resolved.end(); ++it) {
        names.push_back(it.key());
    }
    return names;
}

nlohmann::json ArchetypeInstance::GetFieldJson(const std::string& name) const {
    nlohmann::json::const_iterator it = m_Resolved.find(name);
    if (it != m_Resolved.end()) {
        return *it;
    }
    return nlohmann::json(nullptr);
}

int ArchetypeInstance::GetInt(const std::string& name, int fallback) const {
    nlohmann::json value = GetFieldJson(name);
    if (value.is_number_integer()) {
        return value.get<int>();
    }
    if (value.is_number()) {
        return static_cast<int>(value.get<double>());
    }
    return fallback;
}

float ArchetypeInstance::GetFloat(const std::string& name, float fallback) const {
    nlohmann::json value = GetFieldJson(name);
    if (value.is_number()) {
        return value.get<float>();
    }
    return fallback;
}

double ArchetypeInstance::GetDouble(const std::string& name, double fallback) const {
    nlohmann::json value = GetFieldJson(name);
    if (value.is_number()) {
        return value.get<double>();
    }
    return fallback;
}

bool ArchetypeInstance::GetBool(const std::string& name, bool fallback) const {
    nlohmann::json value = GetFieldJson(name);
    if (value.is_boolean()) {
        return value.get<bool>();
    }
    return fallback;
}

std::string ArchetypeInstance::GetString(const std::string& name, const std::string& fallback) const {
    nlohmann::json value = GetFieldJson(name);
    if (value.is_string()) {
        return value.get<std::string>();
    }
    return fallback;
}

Vec2 ArchetypeInstance::GetVec2(const std::string& name, const Vec2& fallback) const {
    nlohmann::json value = GetFieldJson(name);
    if (value.is_object() && value.contains("x") && value.contains("y")) {
        return Vec2(value["x"].get<float>(), value["y"].get<float>());
    }
    return fallback;
}

Vec3 ArchetypeInstance::GetVec3(const std::string& name, const Vec3& fallback) const {
    nlohmann::json value = GetFieldJson(name);
    if (value.is_object() && value.contains("x") && value.contains("y") && value.contains("z")) {
        return Vec3(value["x"].get<float>(), value["y"].get<float>(), value["z"].get<float>());
    }
    return fallback;
}

Vec4 ArchetypeInstance::GetVec4(const std::string& name, const Vec4& fallback) const {
    nlohmann::json value = GetFieldJson(name);
    if (value.is_object() && value.contains("x") && value.contains("y") &&
        value.contains("z") && value.contains("w")) {
        return Vec4(value["x"].get<float>(), value["y"].get<float>(),
                    value["z"].get<float>(), value["w"].get<float>());
    }
    return fallback;
}

Color ArchetypeInstance::GetColor(const std::string& name, const Color& fallback) const {
    nlohmann::json value = GetFieldJson(name);
    if (value.is_object() && value.contains("r") && value.contains("g") &&
        value.contains("b") && value.contains("a")) {
        return Color(value["r"].get<float>(), value["g"].get<float>(),
                     value["b"].get<float>(), value["a"].get<float>());
    }
    return fallback;
}

Quat ArchetypeInstance::GetQuat(const std::string& name, const Quat& fallback) const {
    nlohmann::json value = GetFieldJson(name);
    if (value.is_object() && value.contains("w") && value.contains("x") &&
        value.contains("y") && value.contains("z")) {
        return Quat(value["w"].get<float>(), value["x"].get<float>(),
                    value["y"].get<float>(), value["z"].get<float>());
    }
    return fallback;
}

Rect ArchetypeInstance::GetRect(const std::string& name, const Rect& fallback) const {
    nlohmann::json value = GetFieldJson(name);
    if (value.is_object() && value.contains("x") && value.contains("y") &&
        value.contains("w") && value.contains("h")) {
        return Rect(value["x"].get<float>(), value["y"].get<float>(),
                    value["w"].get<float>(), value["h"].get<float>());
    }
    return fallback;
}

std::string ArchetypeInstance::GetResource(const std::string& name, const std::string& fallback) const {
    nlohmann::json value = GetFieldJson(name);
    if (value.is_string()) {
        return value.get<std::string>();
    }
    return fallback;
}

std::vector<std::string> ArchetypeInstance::GetStringArray(const std::string& name) const {
    std::vector<std::string> result;
    nlohmann::json value = GetFieldJson(name);
    if (value.is_array()) {
        for (const nlohmann::json& item : value) {
            if (item.is_string()) {
                result.push_back(item.get<std::string>());
            }
        }
    }
    return result;
}

std::vector<int> ArchetypeInstance::GetIntArray(const std::string& name) const {
    std::vector<int> result;
    nlohmann::json value = GetFieldJson(name);
    if (value.is_array()) {
        for (const nlohmann::json& item : value) {
            if (item.is_number_integer()) {
                result.push_back(item.get<int>());
            } else if (item.is_number()) {
                result.push_back(static_cast<int>(item.get<double>()));
            }
        }
    }
    return result;
}

std::vector<float> ArchetypeInstance::GetFloatArray(const std::string& name) const {
    std::vector<float> result;
    nlohmann::json value = GetFieldJson(name);
    if (value.is_array()) {
        for (const nlohmann::json& item : value) {
            if (item.is_number()) {
                result.push_back(item.get<float>());
            }
        }
    }
    return result;
}

nlohmann::json ArchetypeInstance::GetArray(const std::string& name) const {
    nlohmann::json value = GetFieldJson(name);
    if (value.is_array()) {
        return value;
    }
    return nlohmann::json::array();
}

nlohmann::json ArchetypeInstance::GetDictionary(const std::string& name) const {
    nlohmann::json value = GetFieldJson(name);
    if (value.is_object()) {
        return value;
    }
    return nlohmann::json::object();
}

} // namespace asset
} // namespace lupine
