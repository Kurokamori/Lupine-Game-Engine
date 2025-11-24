#pragma once

#include "ScriptingCore.hpp"
#include "ScriptAPI.hpp"
#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>
#include <memory>
#include <string>

namespace lupine {
namespace scripting {

/**
 * Lua script execution environment
 * Provides isolated Lua state for script execution
 */
class LuaEnvironment : public IScriptEnvironment {
public:
    LuaEnvironment();
    ~LuaEnvironment() override;
    
    ScriptLanguage GetLanguage() const override { return ScriptLanguage::Lua; }
    
    bool Initialize() override;
    void Shutdown() override;
    
    ScriptResult ExecuteFile(const std::string& filepath) override;
    ScriptResult ExecuteString(const std::string& script) override;
    ScriptResult CallFunction(const std::string& functionName) override;
    
    bool HasFunction(const std::string& functionName) const override;
    
    void SetGlobal(const std::string& name, int value) override;
    void SetGlobal(const std::string& name, float value) override;
    void SetGlobal(const std::string& name, const std::string& value) override;
    void SetGlobal(const std::string& name, bool value) override;
    
    int GetGlobalInt(const std::string& name, int defaultValue = 0) override;
    float GetGlobalFloat(const std::string& name, float defaultValue = 0.0f) override;
    std::string GetGlobalString(const std::string& name, const std::string& defaultValue = "") override;
    bool GetGlobalBool(const std::string& name, bool defaultValue = false) override;
    
    // Lua-specific: Get the Lua state
    sol::state& GetState() { return m_LuaState; }
    const sol::state& GetState() const { return m_LuaState; }
    
    // Lua-specific: Call function with arguments
    template<typename... Args>
    ScriptResult CallFunctionWithArgs(const std::string& functionName, Args&&... args);
    
    // Lua-specific: Get function result
    template<typename T, typename... Args>
    std::pair<ScriptResult, T> CallFunctionWithResult(const std::string& functionName, Args&&... args);

    // Set the script API context
    void SetScriptAPI(ScriptAPI* api);
    ScriptAPI* GetScriptAPI() const { return m_ScriptAPI; }

private:
    bool m_Initialized = false;
    sol::state m_LuaState;
    ScriptAPI* m_ScriptAPI = nullptr;
    std::string m_LastError;

    ScriptResult HandleLuaError(const sol::error& err);
    void RegisterScriptAPI();
};

// Template implementations
template<typename... Args>
ScriptResult LuaEnvironment::CallFunctionWithArgs(const std::string& functionName, Args&&... args) {
    if (!m_Initialized) {
        return ScriptResult(false, "Lua environment not initialized");
    }
    
    try {
        if (!HasFunction(functionName)) {
            return ScriptResult(false, "Function '" + functionName + "' not found");
        }
        
        sol::protected_function func = m_LuaState[functionName];
        auto result = func(std::forward<Args>(args)...);
        
        if (!result.valid()) {
            sol::error err = result;
            return HandleLuaError(err);
        }
        
        return ScriptResult(true);
    }
    catch (const std::exception& e) {
        return ScriptResult(false, std::string("Lua error: ") + e.what());
    }
}

template<typename T, typename... Args>
std::pair<ScriptResult, T> LuaEnvironment::CallFunctionWithResult(const std::string& functionName, Args&&... args) {
    if (!m_Initialized) {
        return {ScriptResult(false, "Lua environment not initialized"), T{}};
    }
    
    try {
        if (!HasFunction(functionName)) {
            return {ScriptResult(false, "Function '" + functionName + "' not found"), T{}};
        }
        
        sol::protected_function func = m_LuaState[functionName];
        auto result = func(std::forward<Args>(args)...);
        
        if (!result.valid()) {
            sol::error err = result;
            return {HandleLuaError(err), T{}};
        }
        
        T returnValue = result;
        return {ScriptResult(true), returnValue};
    }
    catch (const std::exception& e) {
        return {ScriptResult(false, std::string("Lua error: ") + e.what()), T{}};
    }
}

} // namespace scripting
} // namespace lupine
