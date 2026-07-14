#include "lupine/core/ArchetypeRuntime.hpp"
#include "lupine/core/ArchetypeRegistry.hpp"
#include "lupine/asset/ArchetypeInstance.hpp"
#include "lupine/asset/Asset.hpp"
#include "lupine/scripting/LuaEnvironment.hpp"
#include "lupine/platform/FileSystem.hpp"
#include "lupine/platform/PackFile.hpp"
#include "lupine/logger/Logger.hpp"

#ifdef LUPINE_HAS_MRUBY
#include "lupine/scripting/MRubyEnvironment.hpp"
#endif
#ifdef LUPINE_HAS_MICROPYTHON
#include "lupine/scripting/MicroPythonEnvironment.hpp"
#endif

namespace lupine {
namespace core {

namespace {

std::unique_ptr<scripting::IScriptEnvironment> CreateEnvironment(scripting::ScriptLanguage language) {
    switch (language) {
        case scripting::ScriptLanguage::Lua:
            return std::make_unique<scripting::LuaEnvironment>();
#ifdef LUPINE_HAS_MRUBY
        case scripting::ScriptLanguage::MRuby:
            return std::make_unique<scripting::MRubyEnvironment>();
#endif
#ifdef LUPINE_HAS_MICROPYTHON
        case scripting::ScriptLanguage::MicroPython:
        case scripting::ScriptLanguage::Python:
            return std::make_unique<scripting::MicroPythonEnvironment>();
#endif
        default:
            return nullptr;
    }
}

bool ReadScriptText(const std::string& sourcePath, std::string& outContents) {
    platform::PackFileSystem& packFS = platform::PackFileSystem::Instance();
    if (packFS.isPackMode() && packFS.exists(sourcePath)) {
        outContents = packFS.readFileAsString(sourcePath);
        return !outContents.empty();
    }

    std::string physical = sourcePath;
    if (!platform::FileSystem::Exists(physical)) {
        physical = asset::Asset::ResolveAssetPath(sourcePath);
    }
    platform::FileResult<std::string> result = platform::FileSystem::ReadFile(physical);
    if (!result.success) {
        return false;
    }
    outContents = std::move(result.data);
    return true;
}

} // namespace

ArchetypeRuntime& ArchetypeRuntime::GetInstance() {
    static ArchetypeRuntime instance;
    return instance;
}

scripting::IScriptEnvironment* ArchetypeRuntime::EnsureEnvironment(const std::string& className) {
    std::unordered_map<std::string, ClassEnvironment>::iterator it = m_Environments.find(className);
    if (it != m_Environments.end()) {
        return it->second.loaded ? it->second.env.get() : nullptr;
    }

    const ArchetypeDefinition* def = ArchetypeRegistry::GetInstance().GetDefinition(className);
    if (!def || def->source != ArchetypeSource::Script || def->sourcePath.empty()) {
        m_Environments[className] = ClassEnvironment{};
        return nullptr;
    }

    ClassEnvironment classEnv;
    classEnv.env = CreateEnvironment(def->language);
    if (!classEnv.env) {
        LOG_WARN(LogCategory::Core, "ArchetypeRuntime: No script environment for archetype '{}'", className);
        m_Environments[className] = std::move(classEnv);
        return nullptr;
    }

    if (!classEnv.env->Initialize()) {
        LOG_ERROR(LogCategory::Core, "ArchetypeRuntime: Failed to init environment for archetype '{}'", className);
        m_Environments[className] = ClassEnvironment{};
        return nullptr;
    }

    classEnv.api = std::make_unique<scripting::ScriptAPI>();
    classEnv.env->SetScriptAPI(classEnv.api.get());

    std::string scriptText;
    if (!ReadScriptText(def->sourcePath, scriptText)) {
        LOG_ERROR(LogCategory::Core, "ArchetypeRuntime: Could not read script '{}' for archetype '{}'",
                  def->sourcePath, className);
        m_Environments[className] = ClassEnvironment{};
        return nullptr;
    }

    scripting::ScriptResult result = classEnv.env->ExecuteString(scriptText);
    if (!result.success) {
        LOG_ERROR(LogCategory::Core, "ArchetypeRuntime: Error loading script for archetype '{}': {}",
                  className, result.error);
        m_Environments[className] = ClassEnvironment{};
        return nullptr;
    }

    classEnv.loaded = true;
    ClassEnvironment& stored = (m_Environments[className] = std::move(classEnv));
    return stored.env.get();
}

nlohmann::json ArchetypeRuntime::CallMethod(const std::string& instancePath,
                                            const std::string& methodName,
                                            const nlohmann::json& args) {
    asset::ArchetypeInstance instance;
    if (!instance.LoadFromFile(instancePath)) {
        return nlohmann::json(nullptr);
    }

    const std::string& className = instance.GetArchetypeClass();
    nlohmann::json selfData = instance.GetResolvedFields();

    scripting::IScriptEnvironment* env = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        env = EnsureEnvironment(className);
    }
    if (!env) {
        return nlohmann::json(nullptr);
    }

    return env->CallMethod(methodName, selfData, args);
}

bool ArchetypeRuntime::HasMethod(const std::string& className, const std::string& methodName) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    scripting::IScriptEnvironment* env = EnsureEnvironment(className);
    if (!env) {
        return false;
    }
    return env->HasFunction(methodName);
}

void ArchetypeRuntime::Clear() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Environments.clear();
}

} // namespace core
} // namespace lupine
