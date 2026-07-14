#pragma once

#include "lupine/core/Core.hpp"
#include "lupine/scripting/ScriptingCore.hpp"
#include "lupine/scripting/LuaEnvironment.hpp"
#include "lupine/scripting/MRubyEnvironment.hpp"
#ifdef LUPINE_HAS_MICROPYTHON
#include "lupine/scripting/MicroPythonEnvironment.hpp"
#endif
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace lupine {
namespace scripting {

/**
 * Global variable definition.
 *
 * The value is stored as JSON so every engine type is representable: the scalar
 * types ("int", "float", "double", "bool", "string", "resource") encode as JSON
 * scalars, while the structured types ("vec2"/"vec3"/"vec4"/"color"/"quat"/"rect"
 * objects, "array"/"dictionary"/"string_array"/"int_array"/"float_array"
 * containers) encode as JSON objects/arrays. The value reaches scripts through
 * IScriptEnvironment::SetGlobalJson, which converts it to each VM's native type.
 */
struct GlobalVariable {
    std::string name;
    std::string type;       // canonical type name (see above)
    nlohmann::json value;   // value encoded as JSON (scalar or structured)
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
     * @param micropythonEnv MicroPython environment (can be nullptr if not using MicroPython)
     * @param luaEnv Lua environment (can be nullptr if not using Lua)
     * @param mrubyEnv MRuby environment (can be nullptr if not using MRuby)
     */
    void InitializeGlobalVariables(
        MicroPythonEnvironment* micropythonEnv,
        LuaEnvironment* luaEnv,
        MRubyEnvironment* mrubyEnv
    );

    /**
     * Load and execute all singleton scripts
     * Singletons are loaded in the order they appear in the config
     * @param micropythonEnv MicroPython environment
     * @param luaEnv Lua environment
     * @param mrubyEnv MRuby environment
     * @param projectDir Project directory (for resolving script paths)
     * @return true if all singletons loaded successfully
     */
    bool LoadSingletons(
        MicroPythonEnvironment* micropythonEnv,
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
    void InitializeVariableInMicroPython(MicroPythonEnvironment* env, const GlobalVariable& var);
    void InitializeVariableInLua(LuaEnvironment* env, const GlobalVariable& var);
    void InitializeVariableInMRuby(MRubyEnvironment* env, const GlobalVariable& var);

    bool LoadSingletonInMicroPython(MicroPythonEnvironment* env, const SingletonScript& singleton, const std::string& fullPath);
    bool LoadSingletonInLua(LuaEnvironment* env, const SingletonScript& singleton, const std::string& fullPath);
    bool LoadSingletonInMRuby(MRubyEnvironment* env, const SingletonScript& singleton, const std::string& fullPath);
};

} // namespace scripting
} // namespace lupine
