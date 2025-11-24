#pragma once

#include "lupine/core/Core.hpp"
#include "lupine/scripting/ScriptingCore.hpp"
#include "lupine/scripting/PythonEnvironment.hpp"
#include "lupine/scripting/LuaEnvironment.hpp"
#include "lupine/scripting/MRubyEnvironment.hpp"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace lupine {
namespace scripting {

/**
 * Global variable definition
 */
struct GlobalVariable {
    std::string name;
    std::string type;  // "int", "float", "bool", "string"

    // Value storage (only one will be used based on type)
    int intValue = 0;
    float floatValue = 0.0f;
    bool boolValue = false;
    std::string stringValue;
};

/**
 * Singleton script definition
 */
struct SingletonScript {
    std::string globalName;  // The global identifier for this singleton
    std::string scriptPath;  // Path to the script file (relative to project)
};

/**
 * Manages global variables and singleton/autoload scripts
 * This class is responsible for:
 * - Loading globals configuration from globals.json
 * - Initializing global variables in all script environments
 * - Loading and executing singleton scripts
 * - Making singletons accessible globally in all script environments
 */
class GlobalsManager {
public:
    GlobalsManager();
    ~GlobalsManager();

    /**
     * Load globals configuration from a JSON file
     * @param filepath Path to globals.json
     * @return true if loaded successfully
     */
    bool LoadGlobalsConfig(const std::string& filepath);

    /**
     * Initialize global variables in all script environments
     * Must be called after script environments are initialized
     * @param pythonEnv Python environment (can be nullptr if not using Python)
     * @param luaEnv Lua environment (can be nullptr if not using Lua)
     * @param mrubyEnv MRuby environment (can be nullptr if not using MRuby)
     */
    void InitializeGlobalVariables(
        PythonEnvironment* pythonEnv,
        LuaEnvironment* luaEnv,
        MRubyEnvironment* mrubyEnv
    );

    /**
     * Load and execute all singleton scripts
     * Singletons are loaded in the order they appear in the config
     * @param pythonEnv Python environment
     * @param luaEnv Lua environment
     * @param mrubyEnv MRuby environment
     * @param projectDir Project directory (for resolving script paths)
     * @return true if all singletons loaded successfully
     */
    bool LoadSingletons(
        PythonEnvironment* pythonEnv,
        LuaEnvironment* luaEnv,
        MRubyEnvironment* mrubyEnv,
        const std::string& projectDir
    );

    /**
     * Get list of global variables
     */
    const std::vector<GlobalVariable>& GetGlobalVariables() const { return m_Variables; }

    /**
     * Get list of singletons
     */
    const std::vector<SingletonScript>& GetSingletons() const { return m_Singletons; }

    /**
     * Clear all globals
     */
    void Clear();

private:
    std::vector<GlobalVariable> m_Variables;
    std::vector<SingletonScript> m_Singletons;

    // Helper methods
    void InitializeVariableInPython(PythonEnvironment* env, const GlobalVariable& var);
    void InitializeVariableInLua(LuaEnvironment* env, const GlobalVariable& var);
    void InitializeVariableInMRuby(MRubyEnvironment* env, const GlobalVariable& var);

    bool LoadSingletonInPython(PythonEnvironment* env, const SingletonScript& singleton, const std::string& fullPath);
    bool LoadSingletonInLua(LuaEnvironment* env, const SingletonScript& singleton, const std::string& fullPath);
    bool LoadSingletonInMRuby(MRubyEnvironment* env, const SingletonScript& singleton, const std::string& fullPath);
};

} // namespace scripting
} // namespace lupine
