#pragma once

#include "ScriptingCore.hpp"
#include "ScriptAPI.hpp"
#include <memory>

#ifdef LUPINE_HAS_MRUBY
#include <mruby.h>
#include <mruby/compile.h>
#include <mruby/string.h>
#include <mruby/array.h>
#include <mruby/hash.h>
#include <mruby/variable.h>
#include <mruby/proc.h>
#include <mruby/data.h>
#endif

namespace lupine {
namespace scripting {

#ifdef LUPINE_HAS_MRUBY

/**
 * mRuby script execution environment
 * Provides isolated mRuby state for script execution
 */
class MRubyEnvironment : public IScriptEnvironment {
public:
    MRubyEnvironment();
    ~MRubyEnvironment() override;
    
    ScriptLanguage GetLanguage() const override { return ScriptLanguage::MRuby; }
    
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
    
    // mRuby-specific: Get the mRuby state
    mrb_state* GetState() { return m_MRubyState; }
    const mrb_state* GetState() const { return m_MRubyState; }
    
    // mRuby-specific: Call function with arguments
    template<typename... Args>
    ScriptResult CallFunctionWithArgs(const std::string& functionName, Args&&... args);
    
    // Set the script API context
    void SetScriptAPI(ScriptAPI* api);
    ScriptAPI* GetScriptAPI() const { return m_ScriptAPI; }

private:
    bool m_Initialized = false;
    mrb_state* m_MRubyState = nullptr;
    ScriptAPI* m_ScriptAPI = nullptr;
    std::string m_LastError;
    
    ScriptResult HandleMRubyError();
    void RegisterScriptAPI();
    
    // Helper to convert C++ args to mRuby values
    template<typename T>
    mrb_value ToMRubyValue(T&& value);
};

// Template implementations
template<typename... Args>
ScriptResult MRubyEnvironment::CallFunctionWithArgs(const std::string& functionName, Args&&... args) {
    if (!m_Initialized || !m_MRubyState) {
        return ScriptResult(false, "mRuby environment not initialized");
    }
    
    if (!HasFunction(functionName)) {
        return ScriptResult(false, "Function '" + functionName + "' not found");
    }
    
    // Get the function
    mrb_sym sym = mrb_intern_cstr(m_MRubyState, functionName.c_str());
    mrb_value func = mrb_gv_get(m_MRubyState, sym);
    
    if (mrb_nil_p(func)) {
        return ScriptResult(false, "Function '" + functionName + "' is nil");
    }
    
    // Convert arguments to mRuby values
    mrb_value argv[] = { ToMRubyValue(std::forward<Args>(args))... };
    int argc = sizeof...(Args);
    
    // Call the function
    mrb_value result = mrb_funcall_argv(m_MRubyState, func, mrb_intern_lit(m_MRubyState, "call"), argc, argv);
    
    if (m_MRubyState->exc) {
        return HandleMRubyError();
    }
    
    return ScriptResult(true);
}

template<typename T>
mrb_value MRubyEnvironment::ToMRubyValue(T&& value) {
    if constexpr (std::is_same_v<std::decay_t<T>, int>) {
        return mrb_fixnum_value(value);
    } else if constexpr (std::is_same_v<std::decay_t<T>, float> || std::is_same_v<std::decay_t<T>, double>) {
        return mrb_float_value(m_MRubyState, static_cast<mrb_float>(value));
    } else if constexpr (std::is_same_v<std::decay_t<T>, bool>) {
        return value ? mrb_true_value() : mrb_false_value();
    } else if constexpr (std::is_same_v<std::decay_t<T>, std::string> || std::is_same_v<std::decay_t<T>, const char*>) {
        return mrb_str_new_cstr(m_MRubyState, std::string(value).c_str());
    } else {
        return mrb_nil_value();
    }
}

#endif // LUPINE_HAS_MRUBY

} // namespace scripting
} // namespace lupine

