#include "lupine/scripting/GlobalsManager.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/platform/FileSystem.hpp"
#include "lupine/platform/PackFile.hpp"
#include "lupine/platform/Path.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>

namespace lupine {
namespace scripting {

GlobalsManager::GlobalsManager() {
}

GlobalsManager::~GlobalsManager() {
}

bool GlobalsManager::LoadGlobalsConfig(const std::string& filepath) {
    std::string contents;

    // In pack mode (exported games) the config lives inside the .pck, where neither
    // FileSystem::Exists nor ifstream can see it - both go straight to the real disk.
    platform::PackFileSystem& packFS = platform::PackFileSystem::Instance();
    if (packFS.isPackMode()) {
        const std::string resolved = packFS.resolveAsset(filepath);
        if (packFS.exists(resolved)) {
            contents = packFS.readFileAsString(resolved);
        }
    }

    if (contents.empty()) {
        if (!platform::FileSystem::Exists(filepath)) {
            return false;
        }

        std::ifstream file(filepath);
        if (!file.is_open()) {
            return false;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        contents = buffer.str();
    }

    if (contents.empty()) {
        return false;
    }

    try {

        nlohmann::json json = nlohmann::json::parse(contents);

        Clear();

        if (json.contains("singletons") && json["singletons"].is_array()) {
            for (const auto& singletonData : json["singletons"]) {
                SingletonScript singleton;
                singleton.globalName = singletonData.value("global_name", "");
                singleton.scriptPath = singletonData.value("script_path", "");

                if (!singleton.globalName.empty() && !singleton.scriptPath.empty()) {
                    m_Singletons.push_back(singleton);

                }
            }
        }

        if (json.contains("variables") && json["variables"].is_array()) {
            for (const auto& varData : json["variables"]) {
                GlobalVariable var;
                var.name = varData.value("name", "");
                var.type = varData.value("type", "");

                if (var.name.empty() || var.type.empty()) {
                    continue;
                }

                // The value is stored verbatim as JSON regardless of type, so every
                // engine type (scalars, vectors, colors, quats, rects, arrays,
                // dictionaries) is preserved and delivered through SetGlobalJson.
                if (varData.contains("value")) {
                    var.value = varData["value"];
                } else {
                    var.value = nlohmann::json();
                }

                m_Variables.push_back(var);

            }
        }

        return true;

    } catch (const std::exception& e) {
        LOG_ERROR(LogCategory::Scripting, "GlobalsManager: failed to load globals config '{}': {}", filepath, e.what());
        return false;
    }
}

void GlobalsManager::InitializeGlobalVariables(
    MicroPythonEnvironment* micropythonEnv,
    LuaEnvironment* luaEnv,
    MRubyEnvironment* mrubyEnv
) {

    for (const auto& var : m_Variables) {
        if (micropythonEnv) {
            InitializeVariableInMicroPython(micropythonEnv, var);
        }
        if (luaEnv) {
            InitializeVariableInLua(luaEnv, var);
        }
        if (mrubyEnv) {
            InitializeVariableInMRuby(mrubyEnv, var);
        }
    }

}

bool GlobalsManager::LoadSingletons(
    MicroPythonEnvironment* micropythonEnv,
    LuaEnvironment* luaEnv,
    MRubyEnvironment* mrubyEnv,
    const std::string& projectDir
) {
    if (m_Singletons.empty()) {
        return true;
    }

    bool allSuccess = true;

    for (const auto& singleton : m_Singletons) {
        std::string fullPath = platform::Path::Join(projectDir, singleton.scriptPath);

        if (!platform::FileSystem::Exists(fullPath)) {
            allSuccess = false;
            continue;
        }

        std::string ext = platform::Path::GetExtension(fullPath);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        bool success = false;

        if (ext == ".py" && micropythonEnv) {
            success = LoadSingletonInMicroPython(micropythonEnv, singleton, fullPath);
        } else if (ext == ".lua" && luaEnv) {
            success = LoadSingletonInLua(luaEnv, singleton, fullPath);
        } else if (ext == ".rb" && mrubyEnv) {
            success = LoadSingletonInMRuby(mrubyEnv, singleton, fullPath);
        } else {
            allSuccess = false;
            continue;
        }

        if (!success) {
            allSuccess = false;
        }
    }

    return allSuccess;
}

void GlobalsManager::Clear() {
    m_Variables.clear();
    m_Singletons.clear();
}

void GlobalsManager::InitializeVariableInMicroPython(MicroPythonEnvironment* env, const GlobalVariable& var) {
    if (!env) return;
    env->SetGlobalJson(var.name, var.value);
}

bool GlobalsManager::LoadSingletonInMicroPython(MicroPythonEnvironment* env, const SingletonScript&, const std::string& fullPath) {
    if (!env) return false;

    auto result = env->ExecuteFile(fullPath);
    if (!result.success) {
        return false;
    }

    return true;
}

void GlobalsManager::InitializeVariableInLua(LuaEnvironment* env, const GlobalVariable& var) {
    if (!env) return;
    env->SetGlobalJson(var.name, var.value);
}

bool GlobalsManager::LoadSingletonInLua(LuaEnvironment* env, const SingletonScript&, const std::string& fullPath) {
    if (!env) return false;

    auto result = env->ExecuteFile(fullPath);
    if (!result.success) {

        return false;
    }

    return true;
}

void GlobalsManager::InitializeVariableInMRuby(MRubyEnvironment* env, const GlobalVariable& var) {
    if (!env) return;
    env->SetGlobalJson(var.name, var.value);
}

bool GlobalsManager::LoadSingletonInMRuby(MRubyEnvironment* env, const SingletonScript&, const std::string& fullPath) {
    if (!env) return false;

    auto result = env->ExecuteFile(fullPath);
    if (!result.success) {

        return false;
    }

    return true;
}

}
}
