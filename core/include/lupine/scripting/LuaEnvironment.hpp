#pragma once

#include "ScriptingCore.hpp"
#include "ScriptAPI.hpp"
#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>
#include <memory>
#include <string>
#include <vector>

namespace lupine {
namespace scripting {

/**
 * Shared Lua host.
 *
 * There is exactly one Lua VM (sol::state) for the whole process. The entire
 * Lupine.* API, the node/component object model, the base-component proxies and
 * the coroutine scheduler are registered into it once. Every script instance
 * (see LuaEnvironment) runs inside its own sandboxed sol::environment table that
 * falls through to this VM's globals for reads, so autoload singletons and
 * project globals set on the shared _G are visible to every script with no
 * per-script dependency injection.
 *
 * Because all API binding closures resolve the engine through a single
 * ScriptAPI*, the host tracks an "active API" that is pushed for the duration of
 * each dispatch (a stack so signal/coroutine re-entrancy restores correctly).
 */
class LuaHost {
public:
    static LuaHost& Instance();

    // Open the VM and perform one-time API/usertype/scheduler registration.
    // Safe to call repeatedly; only the first call does work.
    bool EnsureInitialized();
    bool IsInitialized() const { return m_Initialized; }

    sol::state& State() { return m_LuaState; }

    // Bytes currently allocated by the Lua VM (Lua GC accounting). Returns 0 when
    // the VM has not been initialized. Used by the profiler memory snapshot.
    int64_t GetHeapBytes() const;

    // The ScriptAPI whose owner the binding closures currently resolve against.
    ScriptAPI* ActiveApi() const { return m_ScriptAPI; }

    // Make `api` the active API for an upcoming dispatch; always pair with Pop.
    void PushApi(ScriptAPI* api);
    void PopApi();

    // Drive the (process-global) coroutine/await scheduler. Called once per frame
    // by the SceneManager; each coroutine resumes with its originating script's
    // API active so implicit-self API calls inside coroutines resolve correctly.
    void Pump(float deltaTime);

    // Kill every scheduler entry started by `api`'s script instance. Entries hold
    // their originating ScriptAPI* as a light-userdata tag and re-install it as the
    // active API on every resume, so an instance that shuts down without stopping
    // its coroutines leaves them resuming against a freed ScriptAPI.
    void StopCoroutinesFor(ScriptAPI* api);

    // Shared globals: written once into the VM's _G and visible to every script
    // instance through environment fall-through. Used for autoload singletons and
    // project global variables.
    void SetSharedGlobal(const std::string& name, int value);
    void SetSharedGlobal(const std::string& name, float value);
    void SetSharedGlobal(const std::string& name, const std::string& value);
    void SetSharedGlobal(const std::string& name, bool value);
    void SetSharedGlobalNode(const std::string& name, core::Node* node);

    // A process-lifetime ScriptAPI used to wrap nodes bound into the shared VM
    // (singletons). Because the shared globals outlive any single component, such
    // handles must not carry a component's ScriptAPI (which could be freed). The
    // ambient API's scene-manager is refreshed from `treeSource` on each use.
    ScriptAPI* SharedNodeApi(ScriptAPI* treeSource);

    // A Timer dispatches its timeout by function *name* through the signal system,
    // so a callback passed as a Lua function value has to be reachable by name.
    // This binds the function into the shared _G under a generated, collision-free
    // name (every script environment falls through to _G for reads, so the owner's
    // script resolves it) and returns that name. A string callback is returned
    // unchanged; any other value yields an empty name, meaning "no callback".
    // A one-shot callback clears its own slot once it has run.
    std::string BindTransientCallback(const sol::object& callback, bool oneShot);

    // Bind every custom component class the CustomComponentRegistry knows about into
    // the shared _G as an extendable proxy, so `SimBoulder = SimObject:extend()`
    // resolves a custom base exactly like a built-in one. The built-in proxies are
    // registered once at VM start-up, but custom classes only appear once the project
    // has been scanned (and more arrive as scripts are added), so this runs again
    // before each custom component's script executes. Existing names are left intact.
    void RegisterCustomComponentTypes();

private:
    LuaHost() = default;

    void RegisterScriptAPI();
    void RegisterBaseComponentProxies();

    // One extendable base-component proxy table: carries the component type and an
    // extend() that stamps the derived table with its base. Shared by the built-in
    // and the custom registration passes so both produce identical proxies.
    sol::table MakeComponentProxy(const std::string& componentType);

    sol::state m_LuaState;
    ScriptAPI* m_ScriptAPI = nullptr;          // active API (binding closures read this)
    std::vector<ScriptAPI*> m_ApiStack;
    ScriptAPI m_SharedNodeApi;                 // stable API for shared-VM node handles
    bool m_Initialized = false;
    uint64_t m_CallbackCounter = 0;            // names generated by BindTransientCallback
};

/**
 * Lua script instance.
 *
 * A thin per-component handle: it owns only its sandboxed namespace (a
 * sol::environment in the shared VM whose __index falls through to _G). The
 * script's top-level functions and bare variables (delta_time, export
 * properties, ...) live in this environment, so two scripts never collide and
 * reads of shared globals/singletons fall through to the host.
 */
class LuaEnvironment : public IScriptEnvironment {
public:
    LuaEnvironment();
    ~LuaEnvironment() override;

    ScriptLanguage GetLanguage() const override { return ScriptLanguage::Lua; }

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

    // Lua-specific: the shared Lua state (owned by LuaHost).
    sol::state& GetState() { return LuaHost::Instance().State(); }

    // Lua-specific: this instance's sandboxed namespace.
    sol::environment& GetEnv() { return m_Env; }
    const sol::environment& GetEnv() const { return m_Env; }

    // Lua-specific: a table the script returned (a "module/instance" object), or
    // nil when the script returned nothing. When present, lifecycle callbacks are
    // bridged to this table's methods and a singleton bound from this script
    // resolves to this table instead of a bare node handle. This lets a singleton
    // be authored either as top-level functions (node-handle + :call) or as a
    // returned table with colon-methods — both work.
    sol::object GetInstanceTable() const;
    bool HasInstanceTable() const { return m_HasInstance; }

    // Lua-specific: Call function with arguments
    template<typename... Args>
    ScriptResult CallFunctionWithArgs(const std::string& functionName, Args&&... args);

    // Lua-specific: Get function result
    template<typename T, typename... Args>
    std::pair<ScriptResult, T> CallFunctionWithResult(const std::string& functionName, Args&&... args);

    // Set the script API context
    void SetScriptAPI(ScriptAPI* api) override;
    ScriptAPI* GetScriptAPI() const { return m_ScriptAPI; }

private:
    bool m_Initialized = false;
    sol::environment m_Env;
    ScriptAPI* m_ScriptAPI = nullptr;
    std::string m_LastError;

    // A table returned by the script body, if any (see GetInstanceTable).
    sol::table m_InstanceTable;
    bool m_HasInstance = false;

    // Capture a script's return value; if it is a table, remember it and bridge
    // its lifecycle methods into this environment's bare callback names.
    void CaptureInstance(const sol::object& returnValue);

    // Resolve the instance table exposed by a node's Lua script component (if any),
    // used so singleton globals can resolve to a returned table.
    sol::object ResolveNodeInstanceTable(core::Node* node) const;

    ScriptResult HandleLuaError(const sol::error& err);

    // RAII guard that makes this instance's API active for a dispatch.
    struct ApiScope {
        explicit ApiScope(ScriptAPI* api) { LuaHost::Instance().PushApi(api); }
        ~ApiScope() { LuaHost::Instance().PopApi(); }
    };
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

        ApiScope scope(m_ScriptAPI);
        sol::protected_function func = m_Env[functionName];
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

        ApiScope scope(m_ScriptAPI);
        sol::protected_function func = m_Env[functionName];
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
