#pragma once

#include "lupine/core/Core.hpp"
#include <memory>
#include <string>
#include <functional>

namespace lupine {
namespace scripting {

/**
 * Scripting language types supported by the engine
 */
enum class ScriptLanguage {
    Python,
    Lua,
    MRuby
};

/**
 * Script execution result
 */
struct ScriptResult {
    bool success = false;
    std::string error;
    
    ScriptResult() = default;
    ScriptResult(bool s, const std::string& e = "") : success(s), error(e) {}
    
    operator bool() const { return success; }
};

/**
 * Base interface for script execution environments
 */
class IScriptEnvironment {
public:
    virtual ~IScriptEnvironment() = default;
    
    virtual ScriptLanguage GetLanguage() const = 0;
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;
    
    // Execute a script from file
    virtual ScriptResult ExecuteFile(const std::string& filepath) = 0;
    
    // Execute a script from string
    virtual ScriptResult ExecuteString(const std::string& script) = 0;
    
    // Call a function in the script
    virtual ScriptResult CallFunction(const std::string& functionName) = 0;
    
    // Check if a function exists
    virtual bool HasFunction(const std::string& functionName) const = 0;
    
    // Get/Set global variables
    virtual void SetGlobal(const std::string& name, int value) = 0;
    virtual void SetGlobal(const std::string& name, float value) = 0;
    virtual void SetGlobal(const std::string& name, const std::string& value) = 0;
    virtual void SetGlobal(const std::string& name, bool value) = 0;
    
    virtual int GetGlobalInt(const std::string& name, int defaultValue = 0) = 0;
    virtual float GetGlobalFloat(const std::string& name, float defaultValue = 0.0f) = 0;
    virtual std::string GetGlobalString(const std::string& name, const std::string& defaultValue = "") = 0;
    virtual bool GetGlobalBool(const std::string& name, bool defaultValue = false) = 0;
};

/**
 * Initialize scripting subsystem
 */
void InitializeScripting();

/**
 * Shutdown scripting subsystem
 */
void ShutdownScripting();

} // namespace scripting
} // namespace lupine
