#include "lupine/scripting/GlobalsManager.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/platform/FileSystem.hpp"
#include "lupine/platform/Path.hpp"
#include <nlohmann/json.hpp>
#include <fstream>

namespace lupine {
namespace scripting {

GlobalsManager::GlobalsManager() {
}

GlobalsManager::~GlobalsManager() {
}

bool GlobalsManager::LoadGlobalsConfig(const std::string& filepath) {

    if (!platform::FileSystem::Exists(filepath)) {

        return false;
    }

    try {

        std::ifstream file(filepath);
        if (!file.is_open()) {

            return false;
        }

        nlohmann::json json;
        file >> json;
        file.close();

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

                if (var.type == "int") {
                    var.intValue = varData.value("value", 0);
                } else if (var.type == "float") {
                    var.floatValue = varData.value("value", 0.0f);
                } else if (var.type == "bool") {
                    var.boolValue = varData.value("value", false);
                } else if (var.type == "string") {
                    var.stringValue = varData.value("value", "");
                } else {

                    continue;
                }

                m_Variables.push_back(var);

            }
        }

        return true;

    } catch (const std::exception& e) {

        return false;
    }
}

void GlobalsManager::InitializeGlobalVariables(
    PythonEnvironment* pythonEnv,
    LuaEnvironment* luaEnv,
    MRubyEnvironment* mrubyEnv
) {

    for (const auto& var : m_Variables) {
        if (pythonEnv) {
            InitializeVariableInPython(pythonEnv, var);
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
    PythonEnvironment* pythonEnv,
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

        if (ext == ".py" && pythonEnv) {
            success = LoadSingletonInPython(pythonEnv, singleton, fullPath);
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

    if (allSuccess) {

    } else {

    }

    return allSuccess;
}

void GlobalsManager::Clear() {
    m_Variables.clear();
    m_Singletons.clear();
}

void GlobalsManager::InitializeVariableInPython(PythonEnvironment* env, const GlobalVariable& var) {
    if (!env) return;

    if (var.type == "int") {
        env->SetGlobal(var.name, var.intValue);
    } else if (var.type == "float") {
        env->SetGlobal(var.name, var.floatValue);
    } else if (var.type == "bool") {
        env->SetGlobal(var.name, var.boolValue);
    } else if (var.type == "string") {
        env->SetGlobal(var.name, var.stringValue);
    }
}

bool GlobalsManager::LoadSingletonInPython(PythonEnvironment* env, const SingletonScript& singleton, const std::string& fullPath) {
    if (!env) return false;

    auto result = env->ExecuteFile(fullPath);
    if (!result.success) {

        return false;
    }

    return true;
}

void GlobalsManager::InitializeVariableInLua(LuaEnvironment* env, const GlobalVariable& var) {
    if (!env) return;

    if (var.type == "int") {
        env->SetGlobal(var.name, var.intValue);
    } else if (var.type == "float") {
        env->SetGlobal(var.name, var.floatValue);
    } else if (var.type == "bool") {
        env->SetGlobal(var.name, var.boolValue);
    } else if (var.type == "string") {
        env->SetGlobal(var.name, var.stringValue);
    }
}

bool GlobalsManager::LoadSingletonInLua(LuaEnvironment* env, const SingletonScript& singleton, const std::string& fullPath) {
    if (!env) return false;

    auto result = env->ExecuteFile(fullPath);
    if (!result.success) {

        return false;
    }

    return true;
}

void GlobalsManager::InitializeVariableInMRuby(MRubyEnvironment* env, const GlobalVariable& var) {
    if (!env) return;

    if (var.type == "int") {
        env->SetGlobal(var.name, var.intValue);
    } else if (var.type == "float") {
        env->SetGlobal(var.name, var.floatValue);
    } else if (var.type == "bool") {
        env->SetGlobal(var.name, var.boolValue);
    } else if (var.type == "string") {
        env->SetGlobal(var.name, var.stringValue);
    }
}

bool GlobalsManager::LoadSingletonInMRuby(MRubyEnvironment* env, const SingletonScript& singleton, const std::string& fullPath) {
    if (!env) return false;

    auto result = env->ExecuteFile(fullPath);
    if (!result.success) {

        return false;
    }

    return true;
}

}
}
