#pragma once

#include "ScriptingCore.hpp"
#include <pybind11/pybind11.h>
#include <pybind11/embed.h>
#include <pybind11/stl.h>
#include <memory>
#include <string>

namespace py = pybind11;

namespace lupine {
namespace scripting {

// Forward declaration
class ScriptAPI;

/**
 * Python script execution environment
 * Provides isolated Python interpreter for script execution
 */
class PythonEnvironment : public IScriptEnvironment {
public:
    PythonEnvironment();
    ~PythonEnvironment() override;
    
    ScriptLanguage GetLanguage() const override { return ScriptLanguage::Python; }
    
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
    
    // Python-specific: Get the module namespace
    py::dict& GetNamespace() { return m_Namespace; }
    const py::dict& GetNamespace() const { return m_Namespace; }

    // Python-specific: Call function with arguments
    template<typename... Args>
    ScriptResult CallFunctionWithArgs(const std::string& functionName, Args&&... args);

    // Set the script API context
    void SetScriptAPI(ScriptAPI* api);
    ScriptAPI* GetScriptAPI() const;

private:
    bool m_Initialized = false;
    py::dict m_Namespace;
    std::string m_LastError;

    ScriptResult HandlePythonError();
    void RegisterScriptAPI();
};

// Template implementation
template<typename... Args>
ScriptResult PythonEnvironment::CallFunctionWithArgs(const std::string& functionName, Args&&... args) {
    if (!m_Initialized) {
        return ScriptResult(false, "Python environment not initialized");
    }
    
    try {
        if (!HasFunction(functionName)) {
            return ScriptResult(false, "Function '" + functionName + "' not found");
        }
        
        py::object func = m_Namespace[functionName.c_str()];
        func(std::forward<Args>(args)...);
        
        return ScriptResult(true);
    }
    catch (const std::exception& e) {
        return ScriptResult(false, std::string("Python error: ") + e.what());
    }
}

} // namespace scripting
} // namespace lupine
