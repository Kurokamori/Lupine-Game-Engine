#pragma once

#include "ScriptingCore.hpp"
#include "ScriptAPI.hpp"
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include <utility>

#ifdef LUPINE_HAS_MRUBY
#include <mruby.h>
#include <mruby/class.h>
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
 * Shared mRuby host.
 *
 * There is exactly one mrb_state for the whole process. The Lupine module, the
 * node/component object model, the base-component classes and the Fiber-based
 * coroutine scheduler are registered into it once. Each script instance (see
 * MRubyEnvironment) lives in its own anonymous class inside this state, so two
 * scripts never share a method table, while autoload singletons bound as shared
 * globals/constants are visible to every script with no per-script injection.
 *
 * Native bindings resolve the engine through the host's active-API slot, which the
 * host swaps (together with $self) for the duration of each dispatch via a stack,
 * so signal/coroutine re-entrancy restores correctly. The slot is never exposed as
 * a script-writable global.
 */
class MRubyHost {
public:
    static MRubyHost& Instance();

    bool EnsureInitialized();
    bool IsInitialized() const { return m_Initialized; }

    mrb_state* State() { return m_MRubyState; }

    ScriptAPI* ActiveApi() const { return m_ActiveApi; }
    int ActiveInstanceId() const { return m_ActiveInstanceId; }

    // Make `api` (and its owner, as $self) active for an upcoming dispatch; always
    // pair with PopContext.
    void PushContext(ScriptAPI* api, int instanceId = -1);
    void PopContext();

    // Re-establish the context of the instance that owns a suspended coroutine.
    // Returns false (pushing nothing) when that instance has already shut down.
    bool PushCoroutineContext(int instanceId);

    // Drive the process-global Fiber coroutine/await scheduler once per frame.
    void Pump(float deltaTime);

    // Bind an autoload singleton by name into the shared state ($name global plus,
    // for capitalised names, a top-level constant) so every instance resolves it.
    void SetSharedGlobalNode(const std::string& name, core::Node* node);

    // Monotonic id used to give each script instance a unique wrapper class.
    int NextInstanceId() { return m_NextInstanceId++; }

    // Instance registry: coroutines are tagged with the id of the instance that
    // started them, so the scheduler can resolve their API on resume and drop them
    // when that instance goes away.
    void RegisterInstance(int instanceId, ScriptAPI* api);
    void UnregisterInstance(int instanceId);
    ScriptAPI* FindInstanceApi(int instanceId, bool& found) const;

    // Mark every coroutine started by `instanceId` dead and drop it.
    void StopCoroutinesForInstance(int instanceId);

    // A process-lifetime ScriptAPI used to wrap singleton nodes bound into the
    // shared state, so those handles never carry a component API that could be
    // freed. Its scene-manager is refreshed from `treeSource` on each use.
    ScriptAPI* SharedNodeApi(ScriptAPI* treeSource);

    // Define every custom component class the CustomComponentRegistry knows about as a
    // real Ruby class, so `class SimBoulder < SimObject` resolves a custom base just
    // like a built-in one. Each class is defined under its actual base (recursively,
    // so a chain is defined base-first), which makes Ruby's own `is_a?`/`ancestors`
    // agree with the engine's type chain. Idempotent: an already-defined class is left
    // alone. The built-in classes are defined once at start-up, but custom ones only
    // exist after a project scan, so this runs again before each custom script loads.
    void RegisterCustomComponentTypes();

private:
    MRubyHost() = default;

    void RegisterScriptAPI();
    void RegisterBaseComponentClasses();

    // Define `className` (and, first, its base chain) as Ruby classes; returns the
    // resulting class, or the root Component class when the base cannot be resolved.
    // `visiting` guards against a cyclic definition recursing forever - the registry
    // already breaks cycles at scan time, so this is belt-and-braces.
    struct RClass* EnsureCustomComponentClass(const std::string& className,
                                              std::set<std::string>& visiting);

    // A saved $self is reachable only from this C++ stack once $self is overwritten,
    // so PushContext GC-registers it and PopContext unregisters it.
    struct ContextFrame {
        ScriptAPI* api;
        mrb_value self;
        int instanceId;
    };

    mrb_state* m_MRubyState = nullptr;
    ScriptAPI* m_ActiveApi = nullptr;                          // slot read by every native binding
    int m_ActiveInstanceId = -1;
    std::vector<ContextFrame> m_ContextStack;
    std::unordered_map<int, ScriptAPI*> m_InstanceApis;        // live script instances
    ScriptAPI m_SharedNodeApi;                                 // stable API for shared node handles
    bool m_Initialized = false;
    int m_NextInstanceId = 0;
};

// RAII helper: makes an instance's API/owner active for the enclosing dispatch.
struct MRubyContextScope {
    explicit MRubyContextScope(ScriptAPI* api, int instanceId = -1) {
        MRubyHost::Instance().PushContext(api, instanceId);
    }
    ~MRubyContextScope() { MRubyHost::Instance().PopContext(); }
};

/**
 * mRuby script instance.
 *
 * A thin per-component handle into the shared host: it owns a uniquely named
 * wrapper class and one instance of it. The script body is loaded as that class'
 * body (so its `def`s become per-instance methods that never collide), and
 * lifecycle/signal calls dispatch on the instance object.
 */
class MRubyEnvironment : public IScriptEnvironment {
public:
    MRubyEnvironment();
    ~MRubyEnvironment() override;

    ScriptLanguage GetLanguage() const override { return ScriptLanguage::MRuby; }

    bool Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;
    void RegisterCustomComponentTypes() override;

    ScriptResult ExecuteFile(const std::string& filepath) override;
    ScriptResult ExecuteString(const std::string& script) override;
    ScriptResult CallFunction(const std::string& functionName) override;

    nlohmann::json CallMethod(const std::string& functionName,
                              const nlohmann::json& selfData,
                              const nlohmann::json& args) override;

    bool CallFunctionArgs(const std::string& functionName,
                          const nlohmann::json& args) override;

    nlohmann::json CallFunctionResult(const std::string& functionName,
                                      const nlohmann::json& args) override;

    bool HasFunction(const std::string& functionName) const override;

    void SetGlobal(const std::string& name, int value) override;
    void SetGlobal(const std::string& name, float value) override;
    void SetGlobal(const std::string& name, const std::string& value) override;
    void SetGlobal(const std::string& name, bool value) override;
    void SetGlobalNode(const std::string& name, core::Node* node) override;

    int GetGlobalInt(const std::string& name, int defaultValue = 0) override;
    float GetGlobalFloat(const std::string& name, float defaultValue = 0.0f) override;
    std::string GetGlobalString(const std::string& name, const std::string& defaultValue = "") override;
    bool GetGlobalBool(const std::string& name, bool defaultValue = false) override;
    void SetGlobalJson(const std::string& name, const nlohmann::json& value) override;
    nlohmann::json GetGlobalJson(const std::string& name, const nlohmann::json& defaultValue) override;

    // mRuby-specific: the shared mRuby state (owned by MRubyHost).
    mrb_state* GetState() { return MRubyHost::Instance().State(); }
    const mrb_state* GetState() const { return MRubyHost::Instance().State(); }

    // mRuby-specific: Call function with arguments
    template<typename... Args>
    ScriptResult CallFunctionWithArgs(const std::string& functionName, Args&&... args);

    // Set the script API context
    void SetScriptAPI(ScriptAPI* api) override;
    ScriptAPI* GetScriptAPI() const { return m_ScriptAPI; }

private:
    bool m_Initialized = false;
    ScriptAPI* m_ScriptAPI = nullptr;
    std::string m_LastError;

    // The per-instance wrapper class name and the live instance object.
    std::string m_ClassName;
    mrb_value m_Instance;
    bool m_HasInstance = false;
    int m_InstanceId = -1;

    ScriptResult HandleMRubyError();

    // (Re)create the wrapper-class instance after its body has been loaded.
    void EnsureInstance();

    // Helper to convert C++ args to mRuby values
    template<typename T>
    mrb_value ToMRubyValue(T&& value);
};

// Template implementations
template<typename... Args>
ScriptResult MRubyEnvironment::CallFunctionWithArgs(const std::string& functionName, Args&&... args) {
    if (!m_Initialized || !m_HasInstance) {
        return ScriptResult(false, "mRuby environment not initialized");
    }

    if (!HasFunction(functionName)) {
        return ScriptResult(false, "Function '" + functionName + "' not found");
    }

    mrb_state* mrb = MRubyHost::Instance().State();
    MRubyContextScope ctx(m_ScriptAPI, m_InstanceId);

    // Everything allocated below is either an argument on the value stack for the
    // duration of the call or the (discarded) result, so the arena can be unwound.
    const int arena = mrb_gc_arena_save(mrb);

    mrb_sym sym = mrb_intern_cstr(mrb, functionName.c_str());
    std::vector<mrb_value> argv = { ToMRubyValue(std::forward<Args>(args))... };

    mrb_funcall_argv(mrb, m_Instance, sym,
                     static_cast<mrb_int>(argv.size()),
                     argv.empty() ? nullptr : argv.data());

    if (mrb->exc) {
        ScriptResult failure = HandleMRubyError();
        mrb_gc_arena_restore(mrb, arena);
        return failure;
    }

    mrb_gc_arena_restore(mrb, arena);
    return ScriptResult(true);
}

template<typename T>
mrb_value MRubyEnvironment::ToMRubyValue(T&& value) {
    mrb_state* mrb = MRubyHost::Instance().State();
    if constexpr (std::is_same_v<std::decay_t<T>, int>) {
        return mrb_fixnum_value(value);
    } else if constexpr (std::is_same_v<std::decay_t<T>, float> || std::is_same_v<std::decay_t<T>, double>) {
        return mrb_float_value(mrb, static_cast<mrb_float>(value));
    } else if constexpr (std::is_same_v<std::decay_t<T>, bool>) {
        return value ? mrb_true_value() : mrb_false_value();
    } else if constexpr (std::is_same_v<std::decay_t<T>, std::string> || std::is_same_v<std::decay_t<T>, const char*>) {
        return mrb_str_new_cstr(mrb, std::string(value).c_str());
    } else {
        return mrb_nil_value();
    }
}

#endif // LUPINE_HAS_MRUBY

} // namespace scripting
} // namespace lupine
