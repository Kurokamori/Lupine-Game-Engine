#include "lupine/scripting/LuaEnvironment.hpp"
#include "lupine/scripting/ScriptAPI.hpp"
#include "lupine/scripting/NodeRef.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/CustomComponentRegistry.hpp"
#include "lupine/core/ScriptComponent.hpp"
#include "lupine/core/CameraNodes.hpp"
#include "lupine/network/NetworkManager.hpp"
#include "lupine/profiling/Profiler.hpp"
#include "lupine/components/UIControl.hpp"
#include "lupine/components/Particles2D.hpp"
#include "lupine/components/Particles3D.hpp"
#include "lupine/core/ArchetypeRuntime.hpp"
#include "lupine/asset/ArchetypeInstance.hpp"
#include "lupine/asset/AsyncImageLoader.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/platform/PackFile.hpp"
#include "lupine/math/Vec2.hpp"
#include "lupine/math/Vec3.hpp"
#include "lupine/math/Color.hpp"
#include <nlohmann/json.hpp>
#include <cmath>
#include <cstdio>
#include <vector>
#include <sstream>
#include <unordered_map>

namespace lupine {
namespace scripting {

namespace {

/**
 * Recursively convert a JSON value into a Lua value (tables for objects/arrays).
 */
sol::object JsonToLua(sol::state_view lua, const nlohmann::json& json) {
    if (json.is_boolean()) {
        return sol::make_object(lua, json.get<bool>());
    }
    if (json.is_number_integer()) {
        return sol::make_object(lua, json.get<int64_t>());
    }
    if (json.is_number()) {
        return sol::make_object(lua, json.get<double>());
    }
    if (json.is_string()) {
        return sol::make_object(lua, json.get<std::string>());
    }
    if (json.is_array()) {
        sol::table table = lua.create_table();
        int index = 1;
        for (const nlohmann::json& item : json) {
            table[index++] = JsonToLua(lua, item);
        }
        return table;
    }
    if (json.is_object()) {
        sol::table table = lua.create_table();
        for (nlohmann::json::const_iterator it = json.begin(); it != json.end(); ++it) {
            table[it.key()] = JsonToLua(lua, it.value());
        }
        return table;
    }
    return sol::make_object(lua, sol::lua_nil);
}

/**
 * Recursively convert a Lua value into JSON (tables become arrays when keyed by
 * a contiguous 1..n integer sequence, otherwise objects).
 */
nlohmann::json LuaToJson(const sol::object& obj) {
    switch (obj.get_type()) {
        case sol::type::boolean:
            return obj.as<bool>();
        case sol::type::number: {
            double value = obj.as<double>();
            if (value == std::floor(value) && std::abs(value) < 9.0e15) {
                return static_cast<int64_t>(value);
            }
            return value;
        }
        case sol::type::string:
            return obj.as<std::string>();
        case sol::type::table: {
            sol::table table = obj.as<sol::table>();

            std::size_t entryCount = 0;
            bool allIntegerKeys = true;
            for (const std::pair<sol::object, sol::object>& kv : table) {
                ++entryCount;
                if (kv.first.get_type() != sol::type::number) {
                    allIntegerKeys = false;
                }
            }

            if (allIntegerKeys && entryCount > 0) {
                bool contiguous = true;
                nlohmann::json array = nlohmann::json::array();
                for (std::size_t i = 1; i <= entryCount; ++i) {
                    sol::object value = table[i];
                    if (!value.valid()) {
                        contiguous = false;
                        break;
                    }
                    array.push_back(LuaToJson(value));
                }
                if (contiguous) {
                    return array;
                }
            }

            nlohmann::json object = nlohmann::json::object();
            for (const std::pair<sol::object, sol::object>& kv : table) {
                std::string key;
                if (kv.first.get_type() == sol::type::string) {
                    key = kv.first.as<std::string>();
                } else if (kv.first.get_type() == sol::type::number) {
                    key = std::to_string(kv.first.as<int64_t>());
                } else {
                    continue;
                }
                object[key] = LuaToJson(kv.second);
            }
            return object;
        }
        default:
            return nlohmann::json(nullptr);
    }
}

sol::table Vec2ToTable(sol::state_view lua, const math::Vec2& v) {
    sol::table t = lua.create_table();
    t["x"] = v.x;
    t["y"] = v.y;
    return t;
}

sol::table Vec3ToTable(sol::state_view lua, const math::Vec3& v) {
    sol::table t = lua.create_table();
    t["x"] = v.x;
    t["y"] = v.y;
    t["z"] = v.z;
    return t;
}

sol::table ColorToTable(sol::state_view lua, const math::Color& c) {
    sol::table t = lua.create_table();
    t["r"] = c.r;
    t["g"] = c.g;
    t["b"] = c.b;
    t["a"] = c.a;
    return t;
}

// Convert a Lua table of {name = value} into a string->string argument map for
// localization format substitution. Numbers and booleans are stringified.
std::unordered_map<std::string, std::string> LuaTableToArgs(const sol::table& t) {
    std::unordered_map<std::string, std::string> args;
    if (!t.valid()) {
        return args;
    }
    for (const std::pair<sol::object, sol::object>& kv : t) {
        if (kv.first.get_type() != sol::type::string) {
            continue;
        }
        std::string key = kv.first.as<std::string>();
        const sol::object& value = kv.second;
        switch (value.get_type()) {
            case sol::type::string:
                args[key] = value.as<std::string>();
                break;
            case sol::type::number: {
                double d = value.as<double>();
                if (d == std::floor(d) && std::abs(d) < 1e15) {
                    args[key] = std::to_string(static_cast<long long>(d));
                } else {
                    std::ostringstream ss;
                    ss << d;
                    args[key] = ss.str();
                }
                break;
            }
            case sol::type::boolean:
                args[key] = value.as<bool>() ? "true" : "false";
                break;
            default:
                break;
        }
    }
    return args;
}

// Wrap a node/component handle as a Lua value, returning nil when invalid so
// scripts can test results with `if node then ... end`.
sol::object WrapNode(sol::state_view lua, const NodeRef& ref) {
    if (!ref.IsValid()) return sol::make_object(lua, sol::lua_nil);
    return sol::make_object(lua, ref);
}

sol::object WrapComponent(sol::state_view lua, const ComponentRef& ref) {
    if (!ref.IsValid()) return sol::make_object(lua, sol::lua_nil);
    return sol::make_object(lua, ref);
}

sol::object WrapTimer(sol::state_view lua, const TimerRef& ref) {
    if (!ref.IsValid()) return sol::make_object(lua, sol::lua_nil);
    return sol::make_object(lua, ref);
}

sol::object WrapScene(sol::state_view lua, const SceneRef& ref) {
    if (!ref.IsValid()) return sol::make_object(lua, sol::lua_nil);
    return sol::make_object(lua, ref);
}

sol::object WrapTree(sol::state_view lua, const TreeRef& ref) {
    if (!ref.IsValid()) return sol::make_object(lua, sol::lua_nil);
    return sol::make_object(lua, ref);
}

sol::object WrapTween(sol::state_view lua, const TweenRef& ref) {
    if (!ref.IsValid()) return sol::make_object(lua, sol::lua_nil);
    return sol::make_object(lua, ref);
}

sol::object WrapAwaiter(sol::state_view lua, const SignalAwaiter& ref) {
    if (!ref.IsValid()) return sol::make_object(lua, sol::lua_nil);
    return sol::make_object(lua, ref);
}

sol::object WrapSequence(sol::state_view lua, const SequenceRef& ref) {
    if (!ref.IsValid()) return sol::make_object(lua, sol::lua_nil);
    return sol::make_object(lua, ref);
}

// Collect Lua varargs into a JSON array for signal/event emission.
nlohmann::json VarargsToJson(sol::variadic_args va) {
    nlohmann::json arr = nlohmann::json::array();
    for (std::size_t i = 0; i < va.size(); ++i) {
        sol::object o = va[i];
        arr.push_back(LuaToJson(o));
    }
    return arr;
}

// Convert a single signal argument to a Lua value. A node encoded with the
// {"__lupine_node__": "<uuid>"} convention is resolved and delivered as a node
// handle so handlers receive a real LupineNode, not a raw table.
sol::object SignalArgToLua(sol::state_view lua, const nlohmann::json& arg, ScriptAPI* api) {
    if (arg.is_object() && arg.contains("__lupine_node__") && api) {
        std::string uuid = arg["__lupine_node__"].get<std::string>();
        core::Node* node = api->FindNodeByUUID(uuid);
        return WrapNode(lua, NodeRef::FromRaw(node, api));
    }
    return JsonToLua(lua, arg);
}

} // namespace

// ============================================================================
// LuaHost - the single shared Lua VM
// ============================================================================

LuaHost& LuaHost::Instance() {
    static LuaHost s_Instance;
    return s_Instance;
}

int64_t LuaHost::GetHeapBytes() const {
    if (!m_Initialized) {
        return 0;
    }
    lua_State* L = const_cast<sol::state&>(m_LuaState).lua_state();
    if (!L) {
        return 0;
    }
    int kb = lua_gc(L, LUA_GCCOUNT, 0);
    int remainder = lua_gc(L, LUA_GCCOUNTB, 0);
    return static_cast<int64_t>(kb) * 1024 + static_cast<int64_t>(remainder);
}

bool LuaHost::EnsureInitialized() {
    if (m_Initialized) {
        return true;
    }

    try {
        m_LuaState.open_libraries(
            sol::lib::base,
            sol::lib::package,
            sol::lib::math,
            sol::lib::string,
            sol::lib::table,
            sol::lib::coroutine
        );

        m_Initialized = true;

        RegisterScriptAPI();

        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(LogCategory::Scripting, "[Lua] host initialization failed: {}", e.what());
        m_Initialized = false;
        return false;
    }
}

void LuaHost::PushApi(ScriptAPI* api) {
    m_ApiStack.push_back(m_ScriptAPI);
    m_ScriptAPI = api;
}

void LuaHost::PopApi() {
    if (m_ApiStack.empty()) {
        m_ScriptAPI = nullptr;
        return;
    }
    m_ScriptAPI = m_ApiStack.back();
    m_ApiStack.pop_back();
}

void LuaHost::Pump(float deltaTime) {
    if (!m_Initialized) {
        return;
    }
    // Drive the process-global coroutine/await scheduler (Lupine._pump),
    // installed in RegisterScriptAPI. Each coroutine restores its originating
    // script's API before resuming (see the scheduler's _step). Guarded so a VM
    // with no live coroutines pays nothing beyond a table lookup.
    sol::object lupineObj = m_LuaState["Lupine"];
    if (!lupineObj.is<sol::table>()) {
        return;
    }
    sol::table lupine = lupineObj.as<sol::table>();
    sol::object pumpObj = lupine["_pump"];
    if (pumpObj.is<sol::protected_function>()) {
        sol::protected_function pump = pumpObj.as<sol::protected_function>();
        auto result = pump(deltaTime);
        if (!result.valid()) {
            sol::error err = result;
            LOG_ERROR(LogCategory::Scripting, "[Lua] coroutine pump error: {}", err.what());
        }
    }
}

void LuaHost::StopCoroutinesFor(ScriptAPI* api) {
    if (!m_Initialized || !api) {
        return;
    }
    sol::object lupineObj = m_LuaState["Lupine"];
    if (!lupineObj.is<sol::table>()) {
        return;
    }
    sol::table lupine = lupineObj.as<sol::table>();
    sol::object stopObj = lupine["_stop_for_api"];
    if (!stopObj.is<sol::protected_function>()) {
        return;
    }
    sol::protected_function stop = stopObj.as<sol::protected_function>();
    sol::protected_function_result result = stop(static_cast<void*>(api));
    if (!result.valid()) {
        sol::error err = result;
        LOG_ERROR(LogCategory::Scripting, "[Lua] failed to stop instance coroutines: {}", err.what());
    }
}

void LuaHost::SetSharedGlobal(const std::string& name, int value) {
    if (m_Initialized) m_LuaState[name] = value;
}
void LuaHost::SetSharedGlobal(const std::string& name, float value) {
    if (m_Initialized) m_LuaState[name] = value;
}
void LuaHost::SetSharedGlobal(const std::string& name, const std::string& value) {
    if (m_Initialized) m_LuaState[name] = value;
}
void LuaHost::SetSharedGlobal(const std::string& name, bool value) {
    if (m_Initialized) m_LuaState[name] = value;
}

ScriptAPI* LuaHost::SharedNodeApi(ScriptAPI* treeSource) {
    if (treeSource) {
        m_SharedNodeApi.SetSceneManager(treeSource->GetTree());
    }
    return &m_SharedNodeApi;
}

void LuaHost::SetSharedGlobalNode(const std::string& name, core::Node* node) {
    if (!m_Initialized) return;
    m_LuaState[name] = WrapNode(m_LuaState, NodeRef::FromRaw(node, SharedNodeApi(m_ScriptAPI)));
}

std::string LuaHost::BindTransientCallback(const sol::object& callback, bool oneShot) {
    if (!m_Initialized || !callback.valid()) {
        return std::string();
    }

    // A name is passed through: it names a function the owning script defines.
    if (callback.is<std::string>()) {
        return callback.as<std::string>();
    }
    if (!callback.is<sol::function>()) {
        return std::string();
    }

    const std::string name = "__lupine_callback_" + std::to_string(++m_CallbackCounter);
    sol::protected_function target = callback.as<sol::protected_function>();

    if (!oneShot) {
        m_LuaState[name] = target;
        return name;
    }

    // A one-shot callback drops its own slot once it has run, so a script that
    // creates a timer per frame does not accumulate a dead global per timer.
    m_LuaState[name] = [this, name, target](sol::variadic_args va) {
        std::vector<sol::object> args(va.begin(), va.end());
        m_LuaState[name] = sol::lua_nil;

        sol::protected_function_result result = target(sol::as_args(args));
        if (!result.valid()) {
            sol::error err = result;
            LOG_ERROR(LogCategory::Scripting, "Timer callback error: {}", err.what());
        }
    };
    return name;
}

// ============================================================================
// LuaEnvironment - a per-component script instance (a sandboxed namespace)
// ============================================================================

LuaEnvironment::LuaEnvironment() {
}

LuaEnvironment::~LuaEnvironment() {
    Shutdown();
}

bool LuaEnvironment::Initialize() {
    if (m_Initialized) {
        return true;
    }

    LuaHost& host = LuaHost::Instance();
    if (!host.EnsureInitialized()) {
        m_LastError = "Lua host failed to initialize";
        return false;
    }

    try {
        // The instance namespace falls through to the shared _G for reads (so
        // singletons/globals are visible) while bare-name writes stay local
        // (sandboxed): script functions and per-instance vars never collide.
        m_Env = sol::environment(host.State(), sol::create, host.State().globals());
        m_Initialized = true;
        return true;
    }
    catch (const std::exception& e) {
        m_LastError = e.what();
        return false;
    }
}

void LuaEnvironment::Shutdown() {
    if (!m_Initialized) {
        return;
    }

    // The scheduler is process-global and outlives this instance. Its entries pin
    // this instance's ScriptAPI (and, through the coroutine's _ENV upvalue, this
    // sandbox), and they re-install that API as the host's active one on every
    // resume — so any coroutine left suspended here would resume against a freed
    // ScriptAPI once the owning component is destroyed.
    LuaHost::Instance().StopCoroutinesFor(m_ScriptAPI);

    // Drop this instance's registry references so the sandbox and any instance
    // table become collectable now rather than at C++ teardown, when the shared
    // sol::state may already be gone.
    m_Env = sol::environment();
    m_InstanceTable = sol::table();
    m_HasInstance = false;

    m_Initialized = false;
}

void LuaEnvironment::Update(float deltaTime) {
    // The coroutine/await scheduler is process-global and pumped exactly once per
    // frame by the SceneManager via LuaHost::Pump; the per-instance tick is a
    // no-op so coroutines are not advanced once per script.
    (void)deltaTime;
}

void LuaEnvironment::RegisterCustomComponentTypes() {
    // The proxies live in the shared _G, which every instance's sandbox falls
    // through to for reads, so this is host-wide rather than per-instance.
    LuaHost::Instance().RegisterCustomComponentTypes();
}

ScriptResult LuaEnvironment::ExecuteFile(const std::string& filepath) {
    if (!m_Initialized) {
        return ScriptResult(false, "Lua environment not initialized");
    }

    try {
        ApiScope scope(m_ScriptAPI);
        sol::state& lua = LuaHost::Instance().State();

        // Check if running from pack file first
        auto& packFS = platform::PackFileSystem::Instance();
        if (packFS.isPackMode() && packFS.exists(filepath)) {
            std::string scriptContents = packFS.readFileAsString(filepath);
            if (scriptContents.empty()) {
                return ScriptResult(false, "Failed to read script from pack: " + filepath);
            }
            auto result = lua.safe_script(scriptContents, m_Env, sol::script_pass_on_error);
            if (!result.valid()) {
                sol::error err = result;
                return HandleLuaError(err);
            }
            if (result.return_count() > 0) CaptureInstance(result.get<sol::object>(0));
            return ScriptResult(true);
        }

        // Fall back to filesystem
        auto result = lua.safe_script_file(filepath, m_Env, sol::script_pass_on_error);

        if (!result.valid()) {
            sol::error err = result;
            return HandleLuaError(err);
        }

        if (result.return_count() > 0) CaptureInstance(result.get<sol::object>(0));
        return ScriptResult(true);
    }
    catch (const std::exception& e) {
        return ScriptResult(false, std::string("Error: ") + e.what());
    }
}

ScriptResult LuaEnvironment::ExecuteString(const std::string& script) {
    if (!m_Initialized) {
        return ScriptResult(false, "Lua environment not initialized");
    }

    try {
        ApiScope scope(m_ScriptAPI);
        sol::state& lua = LuaHost::Instance().State();
        auto result = lua.safe_script(script, m_Env, sol::script_pass_on_error);

        if (!result.valid()) {
            sol::error err = result;
            return HandleLuaError(err);
        }

        if (result.return_count() > 0) CaptureInstance(result.get<sol::object>(0));
        return ScriptResult(true);
    }
    catch (const std::exception& e) {
        return ScriptResult(false, std::string("Error: ") + e.what());
    }
}

// When a script body returns a table, treat it as the script's instance object:
// remember it, expose it as `__instance` in the sandbox, and bridge any lifecycle
// callbacks it defines as colon-methods into bare callback names so the engine's
// existing dispatch (which looks up bare `on_ready`, `on_process`, ...) invokes
// them with the table as `self`. A bare top-level callback, if also present, wins.
void LuaEnvironment::CaptureInstance(const sol::object& returnValue) {
    if (!returnValue.valid() || !returnValue.is<sol::table>()) {
        return;
    }
    m_InstanceTable = returnValue.as<sol::table>();
    m_HasInstance = true;
    m_Env["__instance"] = m_InstanceTable;

    static const char* kLifecycleBridge = R"LUA(
local inst = __instance
local names = {
    "on_awake", "on_ready", "on_process", "on_physics_process", "on_input",
    "on_input_event", "on_unhandled_input", "on_late_update", "on_enter_tree",
    "on_exit_tree", "on_visibility_changed", "on_draw", "on_render",
    "on_destroy", "get_render_bounds"
}
for _, n in ipairs(names) do
    if type(inst[n]) == "function" and type(rawget(_ENV, n)) ~= "function" then
        _ENV[n] = function(...) return inst[n](inst, ...) end
    end
end
)LUA";
    try {
        LuaHost::Instance().State().safe_script(kLifecycleBridge, m_Env,
                                                sol::script_pass_on_error);
    } catch (const std::exception&) {
        // A malformed instance table must not abort script load.
    }
}

sol::object LuaEnvironment::GetInstanceTable() const {
    if (m_HasInstance) {
        return sol::make_object(LuaHost::Instance().State(), m_InstanceTable);
    }
    return sol::make_object(LuaHost::Instance().State(), sol::lua_nil);
}

sol::object LuaEnvironment::ResolveNodeInstanceTable(core::Node* node) const {
    if (node) {
        for (const auto& component : node->GetComponents()) {
            auto* scriptComp = dynamic_cast<core::ScriptComponent*>(component.get());
            if (!scriptComp) continue;
            auto* luaEnv = dynamic_cast<LuaEnvironment*>(scriptComp->GetScriptEnvironment());
            if (luaEnv && luaEnv->m_HasInstance) {
                return sol::make_object(LuaHost::Instance().State(), luaEnv->m_InstanceTable);
            }
        }
    }
    return sol::make_object(LuaHost::Instance().State(), sol::lua_nil);
}

ScriptResult LuaEnvironment::CallFunction(const std::string& functionName) {
    if (!m_Initialized) {
        return ScriptResult(false, "Lua environment not initialized");
    }

    try {
        if (!HasFunction(functionName)) {
            return ScriptResult(false, "Function '" + functionName + "' not found");
        }

        ApiScope scope(m_ScriptAPI);
        sol::protected_function func = m_Env[functionName];
        auto result = func();

        if (!result.valid()) {
            sol::error err = result;
            return HandleLuaError(err);
        }

        return ScriptResult(true);
    }
    catch (const std::exception& e) {
        return ScriptResult(false, std::string("Error: ") + e.what());
    }
}

nlohmann::json LuaEnvironment::CallMethod(const std::string& functionName,
                                          const nlohmann::json& selfData,
                                          const nlohmann::json& args) {
    if (!m_Initialized) {
        return nlohmann::json(nullptr);
    }

    try {
        sol::object funcObj = m_Env[functionName];
        if (!funcObj.is<sol::function>()) {
            return nlohmann::json(nullptr);
        }

        ApiScope scope(m_ScriptAPI);
        sol::state& lua = LuaHost::Instance().State();
        sol::protected_function func = funcObj;
        sol::object selfObj = JsonToLua(lua, selfData);

        std::vector<sol::object> argObjects;
        if (args.is_array()) {
            for (const nlohmann::json& arg : args) {
                argObjects.push_back(JsonToLua(lua, arg));
            }
        }

        sol::protected_function_result result = func(selfObj, sol::as_args(argObjects));
        if (!result.valid()) {
            sol::error err = result;
            LOG_ERROR(LogCategory::Scripting, "Archetype method '{}' error: {}",
                      functionName, err.what());
            return nlohmann::json(nullptr);
        }

        sol::object returnValue = result;
        return LuaToJson(returnValue);
    }
    catch (const std::exception& e) {
        LOG_ERROR(LogCategory::Scripting, "Archetype method '{}' exception: {}",
                  functionName, e.what());
        return nlohmann::json(nullptr);
    }
}

bool LuaEnvironment::CallFunctionArgs(const std::string& functionName, const nlohmann::json& args) {
    if (!m_Initialized) {
        return false;
    }

    try {
        sol::object funcObj = m_Env[functionName];
        if (!funcObj.is<sol::function>()) {
            return false;
        }

        ApiScope scope(m_ScriptAPI);
        sol::state& lua = LuaHost::Instance().State();
        sol::protected_function func = funcObj;

        std::vector<sol::object> argObjects;
        if (args.is_array()) {
            for (const nlohmann::json& arg : args) {
                argObjects.push_back(SignalArgToLua(lua, arg, m_ScriptAPI));
            }
        }

        sol::protected_function_result result = func(sol::as_args(argObjects));
        if (!result.valid()) {
            sol::error err = result;
            LOG_ERROR(LogCategory::Scripting, "Signal handler '{}' error: {}",
                      functionName, err.what());
            return false;
        }
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(LogCategory::Scripting, "Signal handler '{}' exception: {}",
                  functionName, e.what());
        return false;
    }
}

nlohmann::json LuaEnvironment::CallFunctionResult(const std::string& functionName,
                                                  const nlohmann::json& args) {
    if (!m_Initialized) {
        return nlohmann::json(nullptr);
    }

    try {
        sol::object funcObj = m_Env[functionName];
        if (!funcObj.is<sol::function>()) {
            return nlohmann::json(nullptr);
        }

        ApiScope scope(m_ScriptAPI);
        sol::state& lua = LuaHost::Instance().State();
        sol::protected_function func = funcObj;

        std::vector<sol::object> argObjects;
        if (args.is_array()) {
            for (const nlohmann::json& arg : args) {
                argObjects.push_back(SignalArgToLua(lua, arg, m_ScriptAPI));
            }
        }

        sol::protected_function_result result = func(sol::as_args(argObjects));
        if (!result.valid()) {
            sol::error err = result;
            LOG_ERROR(LogCategory::Scripting, "Script function '{}' error: {}",
                      functionName, err.what());
            return nlohmann::json(nullptr);
        }

        sol::object returnValue = result;
        return LuaToJson(returnValue);
    }
    catch (const std::exception& e) {
        LOG_ERROR(LogCategory::Scripting, "Script function '{}' exception: {}",
                  functionName, e.what());
        return nlohmann::json(nullptr);
    }
}

bool LuaEnvironment::HasFunction(const std::string& functionName) const {
    if (!m_Initialized) {
        return false;
    }

    try {
        sol::object obj = m_Env[functionName];
        return obj.is<sol::function>();
    }
    catch (...) {
        return false;
    }
}

// Bare-name variable writes (delta_time, export properties, ...) are per-instance
// state and stay inside this script's sandboxed namespace.
void LuaEnvironment::SetGlobal(const std::string& name, int value) {
    if (m_Initialized) {
        m_Env[name] = value;
    }
}

void LuaEnvironment::SetGlobal(const std::string& name, float value) {
    if (m_Initialized) {
        m_Env[name] = value;
    }
}

void LuaEnvironment::SetGlobal(const std::string& name, const std::string& value) {
    if (m_Initialized) {
        m_Env[name] = value;
    }
}

void LuaEnvironment::SetGlobal(const std::string& name, bool value) {
    if (m_Initialized) {
        m_Env[name] = value;
    }
}

// Autoload singletons bind by name into the shared VM so every script instance
// resolves the same live object through namespace fall-through. When the
// singleton's script returned a table (its instance object), that table is bound
// directly so `Foo.CONST` / `Foo:method()` work; otherwise a node handle is bound
// (use `Foo:call("method", ...)` and top-level script functions).
void LuaEnvironment::SetGlobalNode(const std::string& name, core::Node* node) {
    if (m_Initialized && m_ScriptAPI) {
        LuaHost& host = LuaHost::Instance();
        sol::state& lua = host.State();
        sol::object instance = ResolveNodeInstanceTable(node);
        if (instance.valid() && instance.is<sol::table>()) {
            lua[name] = instance;
        } else {
            lua[name] = WrapNode(lua, NodeRef::FromRaw(node, host.SharedNodeApi(m_ScriptAPI)));
        }
    }
}

int LuaEnvironment::GetGlobalInt(const std::string& name, int defaultValue) {
    if (!m_Initialized) {
        return defaultValue;
    }

    try {
        sol::object obj = m_Env[name];
        if (obj.is<int>()) {
            return obj.as<int>();
        }
        return defaultValue;
    }
    catch (...) {
        return defaultValue;
    }
}

float LuaEnvironment::GetGlobalFloat(const std::string& name, float defaultValue) {
    if (!m_Initialized) {
        return defaultValue;
    }

    try {
        sol::object obj = m_Env[name];
        if (obj.is<float>() || obj.is<double>()) {
            return obj.as<float>();
        }
        return defaultValue;
    }
    catch (...) {
        return defaultValue;
    }
}

std::string LuaEnvironment::GetGlobalString(const std::string& name, const std::string& defaultValue) {
    if (!m_Initialized) {
        return defaultValue;
    }

    try {
        sol::object obj = m_Env[name];
        if (obj.is<std::string>()) {
            return obj.as<std::string>();
        }
        return defaultValue;
    }
    catch (...) {
        return defaultValue;
    }
}

bool LuaEnvironment::GetGlobalBool(const std::string& name, bool defaultValue) {
    if (!m_Initialized) {
        return defaultValue;
    }

    try {
        sol::object obj = m_Env[name];
        if (obj.is<bool>()) {
            return obj.as<bool>();
        }
        return defaultValue;
    }
    catch (...) {
        return defaultValue;
    }
}

void LuaEnvironment::SetGlobalJson(const std::string& name, const nlohmann::json& value) {
    if (!m_Initialized) {
        return;
    }
    try {
        sol::state& lua = LuaHost::Instance().State();
        m_Env[name] = JsonToLua(lua, value);
    }
    catch (...) {
    }
}

nlohmann::json LuaEnvironment::GetGlobalJson(const std::string& name, const nlohmann::json& defaultValue) {
    if (!m_Initialized) {
        return defaultValue;
    }
    try {
        sol::object obj = m_Env[name];
        if (!obj.valid() || obj == sol::lua_nil) {
            return defaultValue;
        }
        return LuaToJson(obj);
    }
    catch (...) {
        return defaultValue;
    }
}

ScriptResult LuaEnvironment::HandleLuaError(const sol::error& err) {
    m_LastError = err.what();

    return ScriptResult(false, m_LastError);
}

void LuaEnvironment::SetScriptAPI(ScriptAPI* api) {
    m_ScriptAPI = api;

    if (m_Initialized && api) {

        // The API handle and the owner node ("self") are per-instance and live in
        // this script's sandboxed namespace, not in the shared globals.
        m_Env["lupine_api"] = api;

        core::Node* owner = api->GetSelf();
        if (owner) {
            m_Env["self"] = NodeRef::FromRaw(owner, api);
        } else {
            m_Env["self"] = sol::lua_nil;
        }
    }
}

void LuaHost::RegisterScriptAPI() {
    if (!m_Initialized) return;

    // Create the Lupine table as a proper global
    sol::table lupine = m_LuaState.create_named_table("Lupine");

    // Coroutine API plumbing: a coroutine started by one script must resume with
    // that script's API active so implicit-self API calls inside it resolve to
    // the right node. The scheduler stamps each entry with the active API
    // (as light userdata) and restores it before every resume.
    lupine["__cur_api"] = [this]() -> sol::object {
        if (!m_ScriptAPI) {
            return sol::make_object(m_LuaState, sol::lua_nil);
        }
        return sol::make_object(m_LuaState, static_cast<void*>(m_ScriptAPI));
    };
    lupine["__set_api"] = [this](sol::object handle) {
        if (handle.is<void*>()) {
            m_ScriptAPI = static_cast<ScriptAPI*>(handle.as<void*>());
        } else if (!handle.valid()) {
            // A nil/absent handle restores "no active api" so the scheduler can put
            // the active api back exactly as it found it (including the null case).
            m_ScriptAPI = nullptr;
        }
    };

    lupine["log_info"] = [](const std::string& msg) {
        LOG_INFO(LogCategory::Scripting, "[Lua] {}", msg);
    };
    lupine["log_warning"] = [](const std::string& msg) {
        LOG_WARN(LogCategory::Scripting, "[Lua] {}", msg);
    };
    lupine["log_error"] = [](const std::string& msg) {
        LOG_ERROR(LogCategory::Scripting, "[Lua] {}", msg);
    };
    lupine["log_debug"] = [](const std::string&) {
        
    };

    lupine["is_action_pressed"] = [this](const std::string& action, sol::optional<int> player) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsActionPressed(action, player.value_or(-1)) : false;
    };
    lupine["is_action_just_pressed"] = [this](const std::string& action, sol::optional<int> player) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsActionJustPressed(action, player.value_or(-1)) : false;
    };
    lupine["is_action_just_released"] = [this](const std::string& action, sol::optional<int> player) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsActionJustReleased(action, player.value_or(-1)) : false;
    };
    lupine["get_axis"] = [this](const std::string& axis, sol::optional<int> player) -> float {
        return m_ScriptAPI ? m_ScriptAPI->GetAxis(axis, player.value_or(-1)) : 0.0f;
    };

    lupine["get_delta_time"] = [this]() -> float {
        return m_ScriptAPI ? m_ScriptAPI->GetDeltaTime() : 0.0f;
    };
    // Godot-style is_editor_hint: true while the scene is open in the editor
    // viewport, so @tool-style component scripts can drive an editor preview.
    lupine["is_editor"] = [this]() -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsEditor() : false;
    };
    lupine["random_range"] = [this](float min, float max) -> float {
        return m_ScriptAPI ? m_ScriptAPI->RandomRange(min, max) : 0.0f;
    };
    lupine["random_range_int"] = [this](int min, int max) -> int {
        return m_ScriptAPI ? m_ScriptAPI->RandomRangeInt(min, max) : 0;
    };
    lupine["lerp"] = [this](float a, float b, float t) -> float {
        return m_ScriptAPI ? m_ScriptAPI->Lerp(a, b, t) : 0.0f;
    };
    lupine["clamp"] = [this](float value, float min, float max) -> float {
        return m_ScriptAPI ? m_ScriptAPI->Clamp(value, min, max) : value;
    };

    lupine["get_child_count"] = [this]() -> int {
        return m_ScriptAPI ? m_ScriptAPI->GetChildCount() : 0;
    };
    lupine["queue_free_self"] = [this]() {
        if (m_ScriptAPI) m_ScriptAPI->QueueFreeSelf();
    };

    lupine["set_bus_volume"] = [this](const std::string& busName, float volume) {
        if (m_ScriptAPI) m_ScriptAPI->SetBusVolume(busName, volume);
    };
    lupine["get_bus_volume"] = [this](const std::string& busName) -> float {
        return m_ScriptAPI ? m_ScriptAPI->GetBusVolume(busName) : 0.0f;
    };
    lupine["set_bus_muted"] = [this](const std::string& busName, bool muted) {
        if (m_ScriptAPI) m_ScriptAPI->SetBusMuted(busName, muted);
    };

    // Audio bus DSP effects
    lupine["add_bus_effect"] = [this](const std::string& busName, const std::string& effectType) -> int {
        return m_ScriptAPI ? m_ScriptAPI->AddBusEffect(busName, effectType) : -1;
    };
    lupine["remove_bus_effect"] = [this](const std::string& busName, int index) {
        if (m_ScriptAPI) m_ScriptAPI->RemoveBusEffect(busName, index);
    };
    lupine["move_bus_effect"] = [this](const std::string& busName, int fromIndex, int toIndex) {
        if (m_ScriptAPI) m_ScriptAPI->MoveBusEffect(busName, fromIndex, toIndex);
    };
    lupine["clear_bus_effects"] = [this](const std::string& busName) {
        if (m_ScriptAPI) m_ScriptAPI->ClearBusEffects(busName);
    };
    lupine["set_bus_effect_enabled"] = [this](const std::string& busName, int index, bool enabled) {
        if (m_ScriptAPI) m_ScriptAPI->SetBusEffectEnabled(busName, index, enabled);
    };
    lupine["set_bus_effect_parameter"] = [this](const std::string& busName, int index,
                                                const std::string& parameter, float value) {
        if (m_ScriptAPI) m_ScriptAPI->SetBusEffectParameter(busName, index, parameter, value);
    };
    lupine["get_bus_effect_count"] = [this](const std::string& busName) -> int {
        return m_ScriptAPI ? m_ScriptAPI->GetBusEffectCount(busName) : 0;
    };
    lupine["get_bus_effect_parameter"] = [this](const std::string& busName, int index,
                                                const std::string& parameter) -> float {
        return m_ScriptAPI ? m_ScriptAPI->GetBusEffectParameter(busName, index, parameter) : 0.0f;
    };
    lupine["is_bus_effect_enabled"] = [this](const std::string& busName, int index) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsBusEffectEnabled(busName, index) : false;
    };
    lupine["get_bus_level"] = [this](const std::string& busName) -> float {
        return m_ScriptAPI ? m_ScriptAPI->GetBusLevel(busName) : 0.0f;
    };

    // Localization
    lupine["tr"] = [this](const std::string& key, sol::optional<std::string> table) -> std::string {
        if (!m_ScriptAPI) return key;
        return table ? m_ScriptAPI->TrInTable(key, *table) : m_ScriptAPI->Tr(key);
    };
    lupine["tr_fmt"] = [this](const std::string& key, sol::table args,
                             sol::optional<std::string> table) -> std::string {
        if (!m_ScriptAPI) return key;
        return m_ScriptAPI->TrFormat(key, LuaTableToArgs(args), table.value_or(""));
    };
    lupine["tr_plural"] = [this](const std::string& key, long count,
                                sol::optional<sol::table> args,
                                sol::optional<std::string> table) -> std::string {
        if (!m_ScriptAPI) return key;
        std::unordered_map<std::string, std::string> argMap;
        if (args) argMap = LuaTableToArgs(*args);
        return m_ScriptAPI->TrPlural(key, count, argMap, table.value_or(""));
    };
    lupine["set_locale"] = [this](const std::string& locale) {
        if (m_ScriptAPI) m_ScriptAPI->SetLocale(locale);
    };
    lupine["get_locale"] = [this]() -> std::string {
        return m_ScriptAPI ? m_ScriptAPI->GetLocale() : "";
    };
    lupine["get_fallback_locale"] = [this]() -> std::string {
        return m_ScriptAPI ? m_ScriptAPI->GetFallbackLocale() : "";
    };
    lupine["get_locales"] = [this]() -> sol::as_table_t<std::vector<std::string>> {
        if (m_ScriptAPI) return sol::as_table(m_ScriptAPI->GetAvailableLocales());
        return sol::as_table(std::vector<std::string>());
    };
    lupine["has_loc_key"] = [this](const std::string& key) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->HasLocaleKey(key) : false;
    };
    lupine["reload_localization"] = [this]() {
        if (m_ScriptAPI) m_ScriptAPI->ReloadLocalization();
    };
    lupine["set_pseudolocalization"] = [this](bool enabled) {
        if (m_ScriptAPI) m_ScriptAPI->SetPseudolocalization(enabled);
    };
    lupine["is_pseudolocalization"] = [this]() -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsPseudolocalization() : false;
    };

    // Theme
    lupine["set_theme"] = [this](const std::string& path) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->SetActiveTheme(path) : false;
    };
    lupine["get_theme_color"] = [this](const std::string& type, const std::string& entry,
                                       sol::this_state ts) -> sol::table {
        sol::state_view lua(ts);
        if (!m_ScriptAPI) return ColorToTable(lua, math::Color());
        return ColorToTable(lua, m_ScriptAPI->GetThemeColor(type, entry));
    };
    lupine["get_theme_constant"] = [this](const std::string& type, const std::string& entry) -> float {
        return m_ScriptAPI ? m_ScriptAPI->GetThemeConstant(type, entry) : 0.0f;
    };
    lupine["set_palette_color"] = [this](const std::string& key, float r, float g, float b,
                                         sol::optional<float> a) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->SetThemePaletteColor(key, math::Color(r, g, b, a.value_or(1.0f))) : false;
    };
    lupine["set_theme_variable"] = [this](const std::string& key, float value) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->SetThemeVariable(key, value) : false;
    };
    lupine["get_theme_version"] = [this]() -> double {
        return m_ScriptAPI ? static_cast<double>(m_ScriptAPI->GetThemeVersion()) : 0.0;
    };

    // Color & data sampling
    lupine["color_from_hex"] = [this](const std::string& hex, sol::this_state ts) -> sol::table {
        sol::state_view lua(ts);
        if (!m_ScriptAPI) return ColorToTable(lua, math::Color());
        return ColorToTable(lua, m_ScriptAPI->ColorFromHex(hex));
    };
    lupine["color_to_hex"] = [this](float r, float g, float b, float a) -> std::string {
        return m_ScriptAPI ? m_ScriptAPI->ColorToHex(math::Color(r, g, b, a)) : "";
    };
    lupine["color_from_hsv"] = [this](float h, float s, float v, sol::optional<float> a,
                                      sol::this_state ts) -> sol::table {
        sol::state_view lua(ts);
        if (!m_ScriptAPI) return ColorToTable(lua, math::Color());
        return ColorToTable(lua, m_ScriptAPI->ColorFromHSV(h, s, v, a.value_or(1.0f)));
    };
    lupine["color_lerp"] = [this](float r1, float g1, float b1, float a1,
                                  float r2, float g2, float b2, float a2, float t,
                                  sol::this_state ts) -> sol::table {
        sol::state_view lua(ts);
        if (!m_ScriptAPI) return ColorToTable(lua, math::Color());
        return ColorToTable(lua, m_ScriptAPI->ColorLerp(math::Color(r1, g1, b1, a1),
                                                        math::Color(r2, g2, b2, a2), t));
    };
    lupine["sample_gradient"] = [this](const sol::object& gradient, float t, sol::this_state ts) -> sol::table {
        sol::state_view lua(ts);
        if (!m_ScriptAPI) return ColorToTable(lua, math::Color());
        return ColorToTable(lua, m_ScriptAPI->SampleGradient(LuaToJson(gradient), t));
    };
    lupine["sample_curve"] = [this](const sol::object& curve, float t) -> float {
        return m_ScriptAPI ? m_ScriptAPI->SampleCurve(LuaToJson(curve), t) : 0.0f;
    };

    lupine["get_global_int"] = [this](const std::string& name, sol::optional<int> defaultValue) -> int {
        return m_ScriptAPI ? m_ScriptAPI->GetGlobalInt(name, defaultValue.value_or(0)) : 0;
    };
    lupine["get_global_float"] = [this](const std::string& name, sol::optional<float> defaultValue) -> float {
        return m_ScriptAPI ? m_ScriptAPI->GetGlobalFloat(name, defaultValue.value_or(0.0f)) : 0.0f;
    };
    lupine["get_global_string"] = [this](const std::string& name, sol::optional<std::string> defaultValue) -> std::string {
        return m_ScriptAPI ? m_ScriptAPI->GetGlobalString(name, defaultValue.value_or("")) : "";
    };
    lupine["get_global_bool"] = [this](const std::string& name, sol::optional<bool> defaultValue) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->GetGlobalBool(name, defaultValue.value_or(false)) : false;
    };

    lupine["set_global_int"] = [this](const std::string& name, int value) {
        if (m_ScriptAPI) m_ScriptAPI->SetGlobalInt(name, value);
    };
    lupine["set_global_float"] = [this](const std::string& name, float value) {
        if (m_ScriptAPI) m_ScriptAPI->SetGlobalFloat(name, value);
    };
    lupine["set_global_string"] = [this](const std::string& name, const std::string& value) {
        if (m_ScriptAPI) m_ScriptAPI->SetGlobalString(name, value);
    };
    lupine["set_global_bool"] = [this](const std::string& name, bool value) {
        if (m_ScriptAPI) m_ScriptAPI->SetGlobalBool(name, value);
    };

    // Generic accessors for any-typed globals (vectors/colors/quaternions/rects as
    // tables, arrays/dictionaries as lists/tables). Scalars round-trip too.
    lupine["get_global"] = [this](const std::string& name, sol::optional<sol::object> defaultValue) -> sol::object {
        sol::state_view lua = LuaHost::Instance().State();
        if (!m_ScriptAPI) {
            return defaultValue ? *defaultValue : sol::make_object(lua, sol::lua_nil);
        }
        nlohmann::json def = defaultValue ? LuaToJson(*defaultValue) : nlohmann::json();
        return JsonToLua(lua, m_ScriptAPI->GetGlobalValue(name, def));
    };
    lupine["set_global"] = [this](const std::string& name, const sol::object& value) {
        if (m_ScriptAPI) m_ScriptAPI->SetGlobalValue(name, LuaToJson(value));
    };

    // Position/Movement APIs
    lupine["get_position_2d"] = [this]() -> sol::as_table_t<std::vector<float>> {
        if (m_ScriptAPI) {
            auto pos = m_ScriptAPI->GetPosition2D();
            return sol::as_table(std::vector<float>{pos.x, pos.y});
        }
        return sol::as_table(std::vector<float>{0.0f, 0.0f});
    };
    lupine["set_position_2d"] = [this](float x, float y) {
        if (m_ScriptAPI) m_ScriptAPI->SetPosition2D(x, y);
    };
    lupine["translate_2d"] = [this](float dx, float dy) {
        if (m_ScriptAPI) m_ScriptAPI->Translate2D(dx, dy);
    };

    lupine["get_position_3d"] = [this]() -> sol::as_table_t<std::vector<float>> {
        if (m_ScriptAPI) {
            auto pos = m_ScriptAPI->GetPosition3D();
            return sol::as_table(std::vector<float>{pos.x, pos.y, pos.z});
        }
        return sol::as_table(std::vector<float>{0.0f, 0.0f, 0.0f});
    };
    lupine["set_position_3d"] = [this](float x, float y, float z) {
        if (m_ScriptAPI) m_ScriptAPI->SetPosition3D(x, y, z);
    };
    lupine["translate_3d"] = [this](float dx, float dy, float dz) {
        if (m_ScriptAPI) m_ScriptAPI->Translate3D(dx, dy, dz);
    };

    lupine["get_rotation_2d"] = [this]() -> float {
        return m_ScriptAPI ? m_ScriptAPI->GetRotation2D() : 0.0f;
    };
    lupine["set_rotation_2d"] = [this](float degrees) {
        if (m_ScriptAPI) m_ScriptAPI->SetRotation2D(degrees);
    };
    lupine["rotate_2d"] = [this](float degrees) {
        if (m_ScriptAPI) m_ScriptAPI->Rotate2D(degrees);
    };

    lupine["get_scale_2d"] = [this]() -> sol::as_table_t<std::vector<float>> {
        if (m_ScriptAPI) {
            auto scale = m_ScriptAPI->GetScale2D();
            return sol::as_table(std::vector<float>{scale.x, scale.y});
        }
        return sol::as_table(std::vector<float>{1.0f, 1.0f});
    };
    lupine["set_scale_2d"] = [this](float x, float y) {
        if (m_ScriptAPI) m_ScriptAPI->SetScale2D(x, y);
    };

    // Node properties
    lupine["get_name"] = [this]() -> std::string {
        return m_ScriptAPI ? m_ScriptAPI->GetName() : "";
    };
    lupine["set_name"] = [this](const std::string& name) {
        if (m_ScriptAPI) m_ScriptAPI->SetName(name);
    };
    lupine["is_active"] = [this]() -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsActive() : false;
    };
    lupine["set_active"] = [this](bool active) {
        if (m_ScriptAPI) m_ScriptAPI->SetActive(active);
    };
    lupine["is_visible"] = [this]() -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsVisible() : false;
    };
    lupine["set_visible"] = [this](bool visible) {
        if (m_ScriptAPI) m_ScriptAPI->SetVisible(visible);
    };

    // Node tree access
    lupine["get_parent"] = [this]() -> core::Node* {
        return m_ScriptAPI ? m_ScriptAPI->GetParent() : nullptr;
    };
    lupine["get_child_count"] = [this]() -> int {
        return m_ScriptAPI ? m_ScriptAPI->GetChildCount() : 0;
    };
    lupine["has_node"] = [this](const std::string& path) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->HasNode(path) : false;
    };

    // ----------------------------------------------------------------------
    // Node object model (Godot-style scriptable node/component handles)
    // ----------------------------------------------------------------------
    m_LuaState.new_usertype<ComponentRef>("LupineComponent",
        sol::no_constructor,
        "is_valid", &ComponentRef::IsValid,
        "get_type_name", &ComponentRef::GetTypeName,
        "is_instance_of", [](ComponentRef& self, const std::string& typeName) -> bool {
            std::shared_ptr<core::Component> component = self.Lock();
            return component ? component->IsInstanceOf(typeName) : false;
        },
        "get_type_chain", [](ComponentRef& self, sol::this_state ts) -> sol::table {
            sol::state_view lua(ts);
            sol::table chain = lua.create_table();
            std::shared_ptr<core::Component> component = self.Lock();
            if (component) {
                int index = 1;
                for (const std::string& typeName : component->GetTypeChain()) {
                    chain[index++] = typeName;
                }
            }
            return chain;
        },
        "get_name", &ComponentRef::GetName,
        "is_enabled", &ComponentRef::IsEnabled,
        "set_enabled", &ComponentRef::SetEnabled,
        "has_property", &ComponentRef::HasProperty,
        "get", [](ComponentRef& self, const std::string& key, sol::this_state ts) -> sol::object {
            return JsonToLua(sol::state_view(ts), self.Get(key));
        },
        "set", [](ComponentRef& self, const std::string& key, sol::object value) {
            self.Set(key, LuaToJson(value));
        },
        "call", [](ComponentRef& self, const std::string& method, sol::variadic_args va,
                   sol::this_state ts) -> sol::object {
            return JsonToLua(sol::state_view(ts), self.Call(method, VarargsToJson(va)));
        },
        "get_owner", [](ComponentRef& self, sol::this_state ts) -> sol::object {
            return WrapNode(sol::state_view(ts), self.GetOwner());
        },
        "emit", [](ComponentRef& self, const std::string& signal, sol::variadic_args va) {
            self.EmitSignal(signal, VarargsToJson(va));
        },
        "connect", [](ComponentRef& self, const std::string& signal, const NodeRef& target,
                      const std::string& method, sol::optional<uint32_t> flags) -> uint64_t {
            return self.ConnectSignal(signal, target, method, flags.value_or(0));
        },
        "disconnect", [](ComponentRef& self, const std::string& signal, uint64_t id) {
            self.DisconnectSignal(signal, id);
        },
        "disconnect_method", [](ComponentRef& self, const std::string& signal, const NodeRef& target,
                                const std::string& method) {
            self.DisconnectSignalMethod(signal, target, method);
        },
        "is_connected", &ComponentRef::IsSignalConnected,
        "add_user_signal", [](ComponentRef& self, const std::string& name) {
            self.AddUserSignal(name);
        },
        "get_signal_list", [](ComponentRef& self, sol::this_state ts) -> sol::table {
            sol::state_view lua(ts);
            sol::table t = lua.create_table();
            int i = 1;
            for (const std::string& name : self.GetSignalList()) t[i++] = name;
            return t;
        },
        "await_signal", [](ComponentRef& self, const std::string& signal, sol::this_state ts) {
            return WrapAwaiter(sol::state_view(ts), self.AwaitSignal(signal));
        },
        sol::meta_function::index, [](ComponentRef& self, sol::object key, sol::this_state ts) -> sol::object {
            sol::state_view lua(ts);
            if (!key.is<std::string>()) return sol::make_object(lua, sol::lua_nil);
            return JsonToLua(lua, self.Get(key.as<std::string>()));
        },
        sol::meta_function::new_index, [](ComponentRef& self, sol::object key, sol::object value) {
            if (key.is<std::string>()) self.Set(key.as<std::string>(), LuaToJson(value));
        }
    );

    m_LuaState.new_usertype<NodeRef>("LupineNode",
        sol::no_constructor,
        "is_valid", &NodeRef::IsValid,
        "get_name", &NodeRef::GetName,
        "set_name", &NodeRef::SetName,
        "get_uuid", &NodeRef::GetUUID,
        "get_path", &NodeRef::GetPath,
        "get_type_name", &NodeRef::GetTypeName,
        "is_active", &NodeRef::IsActive,
        "set_active", &NodeRef::SetActive,
        "is_visible", &NodeRef::IsVisible,
        "set_visible", &NodeRef::SetVisible,
        "is_unique_name_in_owner", &NodeRef::IsUniqueNameInOwner,
        "set_unique_name_in_owner", &NodeRef::SetUniqueNameInOwner,
        "get_child_count", &NodeRef::GetChildCount,
        "has_node", &NodeRef::HasNode,
        "has_component", &NodeRef::HasComponent,
        "has_property", &NodeRef::HasProperty,
        "queue_free", &NodeRef::QueueFree,
        "queue_free_deferred", &NodeRef::QueueFreeDeferred,
        "free", &NodeRef::Free,
        "add_child", &NodeRef::AddChild,
        "remove_child", &NodeRef::RemoveChild,
        "reparent_to", &NodeRef::ReparentTo,
        "get_sibling_index", &NodeRef::GetSiblingIndex,
        "set_sibling_index", &NodeRef::SetSiblingIndex,
        "get_child_index", &NodeRef::GetChildIndex,
        "move_child", &NodeRef::MoveChild,
        "remove_component", &NodeRef::RemoveComponent,
        "distance_to", &NodeRef::DistanceTo,
        "set_position_2d", &NodeRef::SetPosition2D,
        "translate_2d", &NodeRef::Translate2D,
        "get_rotation_2d", &NodeRef::GetRotation2D,
        "set_rotation_2d", &NodeRef::SetRotation2D,
        "set_scale_2d", &NodeRef::SetScale2D,
        "set_global_position_2d", &NodeRef::SetGlobalPosition2D,
        "set_global_rotation_2d", &NodeRef::SetGlobalRotation2D,
        "get_global_rotation_2d", &NodeRef::GetGlobalRotation2D,
        "set_position_3d", &NodeRef::SetPosition3D,
        "translate_3d", &NodeRef::Translate3D,
        "set_rotation_3d", &NodeRef::SetRotation3D,
        "set_scale_3d", &NodeRef::SetScale3D,
        "set_global_position_3d", &NodeRef::SetGlobalPosition3D,
        "set_global_rotation_3d", &NodeRef::SetGlobalRotation3D,
        "get_position_2d", [](NodeRef& self, sol::this_state ts) { return Vec2ToTable(sol::state_view(ts), self.GetPosition2D()); },
        "get_scale_2d", [](NodeRef& self, sol::this_state ts) { return Vec2ToTable(sol::state_view(ts), self.GetScale2D()); },
        "get_global_position_2d", [](NodeRef& self, sol::this_state ts) { return Vec2ToTable(sol::state_view(ts), self.GetGlobalPosition2D()); },
        "get_global_scale_2d", [](NodeRef& self, sol::this_state ts) { return Vec2ToTable(sol::state_view(ts), self.GetGlobalScale2D()); },
        "get_position_3d", [](NodeRef& self, sol::this_state ts) { return Vec3ToTable(sol::state_view(ts), self.GetPosition3D()); },
        "get_rotation_3d", [](NodeRef& self, sol::this_state ts) { return Vec3ToTable(sol::state_view(ts), self.GetRotation3D()); },
        "get_scale_3d", [](NodeRef& self, sol::this_state ts) { return Vec3ToTable(sol::state_view(ts), self.GetScale3D()); },
        "get_global_rotation_3d", [](NodeRef& self, sol::this_state ts) { return Vec3ToTable(sol::state_view(ts), self.GetGlobalRotation3D()); },
        "get_global_scale_3d", [](NodeRef& self, sol::this_state ts) { return Vec3ToTable(sol::state_view(ts), self.GetGlobalScale3D()); },
        "get_global_position_3d", [](NodeRef& self, sol::this_state ts) { return Vec3ToTable(sol::state_view(ts), self.GetGlobalPosition3D()); },
        "get_parent", [](NodeRef& self, sol::this_state ts) { return WrapNode(sol::state_view(ts), self.GetParent()); },
        "get_child", [](NodeRef& self, const std::string& name, sol::this_state ts) { return WrapNode(sol::state_view(ts), self.GetChild(name)); },
        "get_child_at", [](NodeRef& self, int index, sol::this_state ts) { return WrapNode(sol::state_view(ts), self.GetChildAt(index)); },
        "find_node", [](NodeRef& self, const std::string& path, sol::this_state ts) { return WrapNode(sol::state_view(ts), self.FindNode(path)); },
        "get_node", [](NodeRef& self, const std::string& path, sol::this_state ts) { return WrapNode(sol::state_view(ts), self.FindNode(path)); },
        "duplicate", [](NodeRef& self, sol::this_state ts) { return WrapNode(sol::state_view(ts), self.Duplicate()); },
        "get_children", [](NodeRef& self, sol::this_state ts) -> sol::table {
            sol::state_view lua(ts);
            sol::table t = lua.create_table();
            int i = 1;
            for (const NodeRef& c : self.GetChildren()) t[i++] = c;
            return t;
        },
        "create_tween", [](NodeRef& self, const std::string& channel, sol::object to, float duration,
                           sol::optional<std::string> easing, sol::this_state ts) {
            return WrapTween(sol::state_view(ts), self.CreateTween(channel, LuaToJson(to), duration,
                                                                   easing.value_or("linear")));
        },
        "await_signal", [](NodeRef& self, const std::string& signal, sol::this_state ts) {
            return WrapAwaiter(sol::state_view(ts), self.AwaitSignal(signal));
        },
        "create_sequence", [](NodeRef& self, sol::this_state ts) {
            return WrapSequence(sol::state_view(ts), self.CreateSequence());
        },
        "get_component", [](NodeRef& self, const std::string& type, sol::this_state ts) { return WrapComponent(sol::state_view(ts), self.GetComponent(type)); },
        "add_component", [](NodeRef& self, const std::string& type, sol::this_state ts) { return WrapComponent(sol::state_view(ts), self.AddComponent(type)); },
        "get_components", [](NodeRef& self, const std::string& type, sol::this_state ts) -> sol::table {
            sol::state_view lua(ts);
            sol::table t = lua.create_table();
            int i = 1;
            for (const ComponentRef& c : self.GetComponents(type)) t[i++] = c;
            return t;
        },
        "get", [](NodeRef& self, const std::string& key, sol::this_state ts) -> sol::object {
            return JsonToLua(sol::state_view(ts), self.Get(key));
        },
        "set", [](NodeRef& self, const std::string& key, sol::object value) {
            self.Set(key, LuaToJson(value));
        },
        "has_method", &NodeRef::HasMethod,
        "call", [](NodeRef& self, const std::string& method, sol::variadic_args va,
                   sol::this_state ts) -> sol::object {
            return JsonToLua(sol::state_view(ts), self.Call(method, VarargsToJson(va)));
        },
        "emit", [](NodeRef& self, const std::string& signal, sol::variadic_args va) {
            self.EmitSignal(signal, VarargsToJson(va));
        },
        "connect", [](NodeRef& self, const std::string& signal, const NodeRef& target,
                      const std::string& method, sol::optional<uint32_t> flags) -> uint64_t {
            return self.ConnectSignal(signal, target, method, flags.value_or(0));
        },
        "disconnect", [](NodeRef& self, const std::string& signal, uint64_t id) {
            self.DisconnectSignal(signal, id);
        },
        "disconnect_method", [](NodeRef& self, const std::string& signal, const NodeRef& target,
                                const std::string& method) {
            self.DisconnectSignalMethod(signal, target, method);
        },
        "is_connected", &NodeRef::IsSignalConnected,
        "add_user_signal", [](NodeRef& self, const std::string& name) {
            self.AddUserSignal(name);
        },
        "get_signal_list", [](NodeRef& self, sol::this_state ts) -> sol::table {
            sol::state_view lua(ts);
            sol::table t = lua.create_table();
            int i = 1;
            for (const std::string& name : self.GetSignalList()) t[i++] = name;
            return t;
        },
        "rpc", [](NodeRef& self, const std::string& method, sol::variadic_args va) {
            self.Rpc(method, VarargsToJson(va));
        },
        "rpc_id", [](NodeRef& self, uint32_t peerId, const std::string& method, sol::variadic_args va) {
            self.RpcId(peerId, method, VarargsToJson(va));
        },
        "rpc_unreliable", [](NodeRef& self, const std::string& method, sol::variadic_args va) {
            self.RpcUnreliable(method, VarargsToJson(va));
        },
        "set_multiplayer_authority", [](NodeRef& self, uint32_t peerId) {
            self.SetMultiplayerAuthority(peerId);
        },
        "get_multiplayer_authority", &NodeRef::GetMultiplayerAuthority,
        "is_multiplayer_authority", &NodeRef::IsMultiplayerAuthority,
        "get_network_id", &NodeRef::GetNetworkId,
        "add_to_group", &NodeRef::AddToGroup,
        "remove_from_group", &NodeRef::RemoveFromGroup,
        "is_in_group", &NodeRef::IsInGroup,
        "get_groups", [](NodeRef& self, sol::this_state ts) -> sol::table {
            sol::state_view lua(ts);
            sol::table t = lua.create_table();
            int i = 1;
            for (const std::string& g : self.GetGroups()) t[i++] = g;
            return t;
        },
        "implements_interface", &NodeRef::ImplementsInterface,
        "get_interfaces", [](NodeRef& self, sol::this_state ts) -> sol::table {
            sol::state_view lua(ts);
            sol::table t = lua.create_table();
            int i = 1;
            for (const std::string& iface : self.GetInterfaces()) t[i++] = iface;
            return t;
        },
        "verify_interface", [](NodeRef& self, const std::string& interfaceName, sol::this_state ts) -> sol::object {
            sol::state_view lua(ts);
            return JsonToLua(lua, self.VerifyInterface(interfaceName));
        },
        "camera_shake", [](NodeRef& self, float amplitude, float duration, sol::optional<float> frequency) {
            std::shared_ptr<core::Node> node = self.Lock();
            float freq = frequency.value_or(30.0f);
            if (auto cam2d = std::dynamic_pointer_cast<core::Camera2D>(node)) {
                cam2d->Shake(amplitude, duration, freq);
            } else if (auto camui = std::dynamic_pointer_cast<core::CameraUI>(node)) {
                camui->Shake(amplitude, duration, freq);
            }
        },
        "camera_stop_shake", [](NodeRef& self) {
            std::shared_ptr<core::Node> node = self.Lock();
            if (auto cam2d = std::dynamic_pointer_cast<core::Camera2D>(node)) {
                cam2d->StopShake();
            } else if (auto camui = std::dynamic_pointer_cast<core::CameraUI>(node)) {
                camui->StopShake();
            }
        },
        "camera_is_shaking", [](NodeRef& self) -> bool {
            std::shared_ptr<core::Node> node = self.Lock();
            if (auto cam2d = std::dynamic_pointer_cast<core::Camera2D>(node)) {
                return cam2d->IsShaking();
            }
            if (auto camui = std::dynamic_pointer_cast<core::CameraUI>(node)) {
                return camui->IsShaking();
            }
            return false;
        },
        "camera_set_follow_target", [](NodeRef& self, const NodeRef& target) {
            std::shared_ptr<core::Node> node = self.Lock();
            std::shared_ptr<core::Node> tgt = target.Lock();
            if (auto cam2d = std::dynamic_pointer_cast<core::Camera2D>(node)) {
                cam2d->SetFollowTarget(tgt);
            } else if (auto camui = std::dynamic_pointer_cast<core::CameraUI>(node)) {
                camui->SetFollowTarget(tgt);
            }
        },
        "camera_clear_follow_target", [](NodeRef& self) {
            std::shared_ptr<core::Node> node = self.Lock();
            if (auto cam2d = std::dynamic_pointer_cast<core::Camera2D>(node)) {
                cam2d->ClearFollowTarget();
            } else if (auto camui = std::dynamic_pointer_cast<core::CameraUI>(node)) {
                camui->ClearFollowTarget();
            }
        },
        "camera_smooth_move_to", [](NodeRef& self, float x, float y, sol::optional<float> speed) {
            std::shared_ptr<core::Node> node = self.Lock();
            if (auto camui = std::dynamic_pointer_cast<core::CameraUI>(node)) {
                camui->SmoothMoveTo(math::Vec2(x, y), speed.value_or(0.0f));
            }
        },
        "camera_get_effective_position", [](NodeRef& self, sol::this_state ts) -> sol::object {
            sol::state_view lua(ts);
            std::shared_ptr<core::Node> node = self.Lock();
            if (auto cam2d = std::dynamic_pointer_cast<core::Camera2D>(node)) {
                return Vec2ToTable(lua, cam2d->GetEffectivePosition());
            }
            if (auto camui = std::dynamic_pointer_cast<core::CameraUI>(node)) {
                return Vec2ToTable(lua, camui->GetEffectivePosition());
            }
            return sol::make_object(lua, sol::lua_nil);
        },
        "particles_restart", [](NodeRef& self) {
            std::shared_ptr<core::Node> node = self.Lock();
            if (!node) return;
            if (auto p2d = node->GetComponent<components::Particles2D>()) {
                p2d->Restart();
            } else if (auto p3d = node->GetComponent<components::Particles3D>()) {
                p3d->Restart();
            }
        },
        "particles_emit_burst", [](NodeRef& self, int count) {
            std::shared_ptr<core::Node> node = self.Lock();
            if (!node) return;
            if (auto p2d = node->GetComponent<components::Particles2D>()) {
                p2d->EmitBurst(count);
            } else if (auto p3d = node->GetComponent<components::Particles3D>()) {
                p3d->EmitBurst(count);
            }
        },
        "particles_set_emitting", [](NodeRef& self, bool emitting) {
            std::shared_ptr<core::Node> node = self.Lock();
            if (!node) return;
            if (auto p2d = node->GetComponent<components::Particles2D>()) {
                p2d->SetEmitting(emitting);
            } else if (auto p3d = node->GetComponent<components::Particles3D>()) {
                p3d->SetEmitting(emitting);
            }
        },
        "particles_is_emitting", [](NodeRef& self) -> bool {
            std::shared_ptr<core::Node> node = self.Lock();
            if (!node) return false;
            if (auto p2d = node->GetComponent<components::Particles2D>()) {
                return p2d->GetEmitting();
            }
            if (auto p3d = node->GetComponent<components::Particles3D>()) {
                return p3d->GetEmitting();
            }
            return false;
        },
        "particles_get_alive_count", [](NodeRef& self) -> int {
            std::shared_ptr<core::Node> node = self.Lock();
            if (!node) return 0;
            if (auto p2d = node->GetComponent<components::Particles2D>()) {
                return p2d->GetAliveCount();
            }
            if (auto p3d = node->GetComponent<components::Particles3D>()) {
                return p3d->GetAliveCount();
            }
            return 0;
        },
        "set_theme", [](NodeRef& self, const std::string& path) {
            std::shared_ptr<core::Node> node = self.Lock();
            if (auto ui = node ? node->GetComponent<components::UIControl>() : nullptr) {
                ui->SetThemePath(path);
            }
        },
        "set_theme_type_variation", [](NodeRef& self, const std::string& variation) {
            std::shared_ptr<core::Node> node = self.Lock();
            if (auto ui = node ? node->GetComponent<components::UIControl>() : nullptr) {
                ui->SetThemeTypeVariation(variation);
            }
        },
        "clear_theme_override", [](NodeRef& self, const std::string& property) {
            std::shared_ptr<core::Node> node = self.Lock();
            if (auto ui = node ? node->GetComponent<components::UIControl>() : nullptr) {
                ui->SetThemeOverride(property, false);
            }
        },
        sol::meta_function::index, [](NodeRef& self, sol::object key, sol::this_state ts) -> sol::object {
            sol::state_view lua(ts);
            if (!key.is<std::string>()) return sol::make_object(lua, sol::lua_nil);
            return JsonToLua(lua, self.Get(key.as<std::string>()));
        },
        sol::meta_function::new_index, [](NodeRef& self, sol::object key, sol::object value) {
            if (key.is<std::string>()) self.Set(key.as<std::string>(), LuaToJson(value));
        }
    );

    // Timer handle: created/listed by the timer module functions below.
    m_LuaState.new_usertype<TimerRef>("LupineTimer",
        sol::no_constructor,
        "is_valid", &TimerRef::IsValid,
        "get_name", &TimerRef::GetName,
        "start", &TimerRef::Start,
        "stop", &TimerRef::Stop,
        "reset", &TimerRef::Reset,
        "restart", &TimerRef::Restart,
        "remove", &TimerRef::Remove,
        "is_running", &TimerRef::IsRunning,
        "is_finished", &TimerRef::IsFinished,
        "get_time_left", &TimerRef::GetTimeLeft,
        "get_fire_count", &TimerRef::GetFireCount,
        "get_duration", &TimerRef::GetDuration,
        "set_duration", &TimerRef::SetDuration,
        "get_elapsed", &TimerRef::GetElapsed,
        "set_elapsed", &TimerRef::SetElapsed,
        "get_loop", &TimerRef::GetLoop,
        "set_loop", &TimerRef::SetLoop,
        "get_repeat_count", &TimerRef::GetRepeatCount,
        "set_repeat_count", &TimerRef::SetRepeatCount,
        "get_owner", [](TimerRef& self, sol::this_state ts) { return WrapNode(sol::state_view(ts), self.GetOwner()); },
        "as_component", [](TimerRef& self, sol::this_state ts) { return WrapComponent(sol::state_view(ts), self.AsComponent()); }
    );

    // Scene handle: returned by get_scene() and tree:get_current_scene().
    m_LuaState.new_usertype<SceneRef>("LupineScene",
        sol::no_constructor,
        "is_valid", &SceneRef::IsValid,
        "get_name", &SceneRef::GetName,
        "get_path", &SceneRef::GetPath,
        "get_root", [](SceneRef& self, sol::this_state ts) { return WrapNode(sol::state_view(ts), self.GetRoot()); },
        "find_node", [](SceneRef& self, const std::string& path, sol::this_state ts) { return WrapNode(sol::state_view(ts), self.FindNode(path)); },
        "find_node_by_uuid", [](SceneRef& self, const std::string& uuid, sol::this_state ts) { return WrapNode(sol::state_view(ts), self.FindNodeByUUID(uuid)); }
    );

    // Scene tree handle: returned by get_tree().
    m_LuaState.new_usertype<TreeRef>("LupineTree",
        sol::no_constructor,
        "is_valid", &TreeRef::IsValid,
        "get_current_scene_path", &TreeRef::GetCurrentScenePath,
        "change_scene", &TreeRef::ChangeScene,
        "reload_scene", &TreeRef::ReloadScene,
        "add_scene", &TreeRef::AddScene,
        "remove_scene", &TreeRef::RemoveScene,
        "get_root", [](TreeRef& self, sol::this_state ts) { return WrapNode(sol::state_view(ts), self.GetRoot()); },
        "get_current_scene", [](TreeRef& self, sol::this_state ts) { return WrapScene(sol::state_view(ts), self.GetCurrentScene()); }
    );

    // Tween handle: created/listed by the tween module + node functions below.
    m_LuaState.new_usertype<TweenRef>("LupineTween",
        sol::no_constructor,
        "is_valid", &TweenRef::IsValid,
        "get_name", &TweenRef::GetName,
        "play", &TweenRef::Play,
        "pause", &TweenRef::Pause,
        "stop", &TweenRef::Stop,
        "restart", &TweenRef::Restart,
        "kill", &TweenRef::Kill,
        "is_running", &TweenRef::IsRunning,
        "is_finished", &TweenRef::IsFinished,
        "get_progress", &TweenRef::GetProgress,
        "get_duration", &TweenRef::GetDuration,
        "set_duration", &TweenRef::SetDuration,
        "get_easing", &TweenRef::GetEasing,
        "set_easing", &TweenRef::SetEasing,
        "get_loop", &TweenRef::GetLoop,
        "set_loop", &TweenRef::SetLoop,
        "set_auto_remove", &TweenRef::SetAutoRemove,
        "get_owner", [](TweenRef& self, sol::this_state ts) { return WrapNode(sol::state_view(ts), self.GetOwner()); },
        "as_component", [](TweenRef& self, sol::this_state ts) { return WrapComponent(sol::state_view(ts), self.AsComponent()); }
    );

    // Signal awaiter: one-shot latch used by Lupine.await_signal.
    m_LuaState.new_usertype<SignalAwaiter>("LupineSignalAwaiter",
        sol::no_constructor,
        "is_valid", &SignalAwaiter::IsValid,
        "is_fired", &SignalAwaiter::IsFired,
        "reset", &SignalAwaiter::Reset,
        "cancel", &SignalAwaiter::Cancel
    );

    // Tween sequence builder handle (create_sequence).
    m_LuaState.new_usertype<SequenceRef>("LupineSequence",
        sol::no_constructor,
        "is_valid", &SequenceRef::IsValid,
        "get_name", &SequenceRef::GetName,
        "append", [](SequenceRef& self, const std::string& channel, sol::object to, float duration,
                     sol::optional<std::string> easing, sol::optional<bool> parallel) {
            return self.Append(channel, LuaToJson(to), duration, easing.value_or("linear"),
                               parallel.value_or(false));
        },
        "append_on", [](SequenceRef& self, const NodeRef& target, const std::string& channel, sol::object to,
                        float duration, sol::optional<std::string> easing, sol::optional<bool> parallel) {
            return self.AppendOn(target, channel, LuaToJson(to), duration, easing.value_or("linear"),
                                 parallel.value_or(false));
        },
        "append_interval", [](SequenceRef& self, float duration, sol::optional<bool> parallel) {
            return self.AppendInterval(duration, parallel.value_or(false));
        },
        "append_callback", [](SequenceRef& self, const std::string& method, sol::optional<bool> parallel) {
            return self.AppendCallback(method, parallel.value_or(false));
        },
        "append_callback_on", [](SequenceRef& self, const NodeRef& target, const std::string& method,
                                 sol::optional<bool> parallel) {
            return self.AppendCallbackOn(target, method, parallel.value_or(false));
        },
        "play", &SequenceRef::Play,
        "stop", &SequenceRef::Stop,
        "reset", &SequenceRef::Reset,
        "restart", &SequenceRef::Restart,
        "kill", &SequenceRef::Kill,
        "is_running", &SequenceRef::IsRunning,
        "is_finished", &SequenceRef::IsFinished,
        "set_loops", &SequenceRef::SetLoops,
        "get_loops", &SequenceRef::GetLoops,
        "set_auto_remove", &SequenceRef::SetAutoRemove,
        "get_step_count", &SequenceRef::GetStepCount,
        "get_owner", [](SequenceRef& self, sol::this_state ts) { return WrapNode(sol::state_view(ts), self.GetOwner()); },
        "as_component", [](SequenceRef& self, sol::this_state ts) { return WrapComponent(sol::state_view(ts), self.AsComponent()); }
    );

    // Native value types: pure math objects scripts can construct and use directly.
    m_LuaState.new_usertype<lupine::math::Vec2>("Vector2",
        sol::constructors<lupine::math::Vec2(), lupine::math::Vec2(float), lupine::math::Vec2(float, float)>(),
        "x", &lupine::math::Vec2::x,
        "y", &lupine::math::Vec2::y,
        sol::meta_function::addition, [](const lupine::math::Vec2& a, const lupine::math::Vec2& b) { return a + b; },
        sol::meta_function::subtraction, [](const lupine::math::Vec2& a, const lupine::math::Vec2& b) { return a - b; },
        sol::meta_function::multiplication, sol::overload(
            [](const lupine::math::Vec2& a, float s) { return a * s; },
            [](const lupine::math::Vec2& a, const lupine::math::Vec2& b) { return a * b; }
        ),
        sol::meta_function::division, sol::overload(
            [](const lupine::math::Vec2& a, float s) { return a / s; },
            [](const lupine::math::Vec2& a, const lupine::math::Vec2& b) { return a / b; }
        ),
        sol::meta_function::unary_minus, [](const lupine::math::Vec2& a) { return -a; },
        sol::meta_function::equal_to, [](const lupine::math::Vec2& a, const lupine::math::Vec2& b) { return a == b; },
        "length", &lupine::math::Vec2::Length,
        "length_squared", &lupine::math::Vec2::LengthSquared,
        "normalized", &lupine::math::Vec2::Normalized,
        "dot", &lupine::math::Vec2::Dot,
        "distance_to", &lupine::math::Vec2::Distance,
        "lerp", &lupine::math::Vec2::Lerp,
        "angle", &lupine::math::Vec2::Angle,
        sol::meta_function::to_string, [](const lupine::math::Vec2& v) {
            std::ostringstream oss;
            oss << "Vector2(" << v.x << ", " << v.y << ")";
            return oss.str();
        }
    );

    m_LuaState.new_usertype<lupine::math::Vec3>("Vector3",
        sol::constructors<lupine::math::Vec3(), lupine::math::Vec3(float), lupine::math::Vec3(float, float, float)>(),
        "x", &lupine::math::Vec3::x,
        "y", &lupine::math::Vec3::y,
        "z", &lupine::math::Vec3::z,
        sol::meta_function::addition, [](const lupine::math::Vec3& a, const lupine::math::Vec3& b) { return a + b; },
        sol::meta_function::subtraction, [](const lupine::math::Vec3& a, const lupine::math::Vec3& b) { return a - b; },
        sol::meta_function::multiplication, sol::overload(
            [](const lupine::math::Vec3& a, float s) { return a * s; },
            [](const lupine::math::Vec3& a, const lupine::math::Vec3& b) { return a * b; }
        ),
        sol::meta_function::division, sol::overload(
            [](const lupine::math::Vec3& a, float s) { return a / s; },
            [](const lupine::math::Vec3& a, const lupine::math::Vec3& b) { return a / b; }
        ),
        sol::meta_function::unary_minus, [](const lupine::math::Vec3& a) { return -a; },
        sol::meta_function::equal_to, [](const lupine::math::Vec3& a, const lupine::math::Vec3& b) { return a == b; },
        "length", &lupine::math::Vec3::Length,
        "length_squared", &lupine::math::Vec3::LengthSquared,
        "normalized", &lupine::math::Vec3::Normalized,
        "dot", &lupine::math::Vec3::Dot,
        "cross", &lupine::math::Vec3::Cross,
        "distance_to", &lupine::math::Vec3::Distance,
        "lerp", &lupine::math::Vec3::Lerp,
        sol::meta_function::to_string, [](const lupine::math::Vec3& v) {
            std::ostringstream oss;
            oss << "Vector3(" << v.x << ", " << v.y << ", " << v.z << ")";
            return oss.str();
        }
    );

    m_LuaState.new_usertype<lupine::math::Color>("Color",
        sol::constructors<lupine::math::Color(), lupine::math::Color(float, float, float), lupine::math::Color(float, float, float, float)>(),
        "r", &lupine::math::Color::r,
        "g", &lupine::math::Color::g,
        "b", &lupine::math::Color::b,
        "a", &lupine::math::Color::a,
        sol::meta_function::addition, [](const lupine::math::Color& a, const lupine::math::Color& b) { return a + b; },
        sol::meta_function::subtraction, [](const lupine::math::Color& a, const lupine::math::Color& b) { return a - b; },
        sol::meta_function::multiplication, [](const lupine::math::Color& a, float s) { return a * s; },
        sol::meta_function::equal_to, [](const lupine::math::Color& a, const lupine::math::Color& b) { return a == b; },
        "lerp", &lupine::math::Color::Lerp,
        "with_alpha", [](const lupine::math::Color& c, float alpha) { return lupine::math::Color(c.r, c.g, c.b, alpha); },
        "to_hex", [](const lupine::math::Color& c) {
            auto toByte = [](float v) -> int {
                int i = static_cast<int>(lupine::math::Saturate(v) * 255.0f + 0.5f);
                if (i < 0) i = 0;
                if (i > 255) i = 255;
                return i;
            };
            char buf[16];
            std::snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", toByte(c.r), toByte(c.g), toByte(c.b), toByte(c.a));
            return std::string(buf);
        },
        sol::meta_function::to_string, [](const lupine::math::Color& c) {
            std::ostringstream oss;
            oss << "Color(" << c.r << ", " << c.g << ", " << c.b << ", " << c.a << ")";
            return oss.str();
        }
    );

    // Scene-level node lookup. "%Name" resolves a uniquely-named node within the
    // owner scope; otherwise the path is resolved from the scene root.
    lupine["get_node"] = [this](const std::string& path, sol::this_state ts) -> sol::object {
        sol::state_view lua(ts);
        if (!m_ScriptAPI) return sol::make_object(lua, sol::lua_nil);
        return WrapNode(lua, NodeRef::FromRaw(m_ScriptAPI->FindNode(path), m_ScriptAPI));
    };
    lupine["find_node"] = [this](const std::string& path, sol::this_state ts) -> sol::object {
        sol::state_view lua(ts);
        if (!m_ScriptAPI) return sol::make_object(lua, sol::lua_nil);
        return WrapNode(lua, NodeRef::FromRaw(m_ScriptAPI->FindNode(path), m_ScriptAPI));
    };
    lupine["find_node_by_uuid"] = [this](const std::string& uuid, sol::this_state ts) -> sol::object {
        sol::state_view lua(ts);
        if (!m_ScriptAPI) return sol::make_object(lua, sol::lua_nil);
        return WrapNode(lua, NodeRef::FromRaw(m_ScriptAPI->FindNodeByUUID(uuid), m_ScriptAPI));
    };
    lupine["get_self"] = [this](sol::this_state ts) -> sol::object {
        sol::state_view lua(ts);
        if (!m_ScriptAPI) return sol::make_object(lua, sol::lua_nil);
        return WrapNode(lua, NodeRef::FromRaw(m_ScriptAPI->GetSelf(), m_ScriptAPI));
    };
    lupine["get_root"] = [this](sol::this_state ts) -> sol::object {
        sol::state_view lua(ts);
        if (!m_ScriptAPI) return sol::make_object(lua, sol::lua_nil);
        return WrapNode(lua, NodeRef::FromRaw(m_ScriptAPI->GetRoot(), m_ScriptAPI));
    };
    lupine["get_singleton"] = [this](const std::string& name, sol::this_state ts) -> sol::object {
        sol::state_view lua(ts);
        if (!m_ScriptAPI) return sol::make_object(lua, sol::lua_nil);
        return WrapNode(lua, NodeRef::FromRaw(m_ScriptAPI->GetSingleton(name), m_ScriptAPI));
    };
    lupine["get_scene"] = [this](sol::this_state ts) -> sol::object {
        sol::state_view lua(ts);
        if (!m_ScriptAPI) return sol::make_object(lua, sol::lua_nil);
        return WrapScene(lua, SceneRef(m_ScriptAPI->GetScene(), m_ScriptAPI));
    };
    lupine["get_tree"] = [this](sol::this_state ts) -> sol::object {
        sol::state_view lua(ts);
        if (!m_ScriptAPI) return sol::make_object(lua, sol::lua_nil);
        return WrapTree(lua, TreeRef(m_ScriptAPI->GetTree(), m_ScriptAPI));
    };
    lupine["get_sibling_index"] = [this]() -> int {
        return m_ScriptAPI ? m_ScriptAPI->GetSiblingIndex() : -1;
    };

    // ----------------------------------------------------------------------
    // Runtime instantiation (prefab / scene / node), returns node handles.
    // ----------------------------------------------------------------------
    lupine["instantiate_prefab"] = [this](const std::string& path, sol::optional<NodeRef> parent,
                                          sol::this_state ts) -> sol::object {
        sol::state_view lua(ts);
        if (!m_ScriptAPI) return sol::make_object(lua, sol::lua_nil);
        core::Node* parentNode = (parent && parent->IsValid()) ? parent->Lock().get() : nullptr;
        return WrapNode(lua, NodeRef::FromRaw(m_ScriptAPI->InstantiatePrefab(path, parentNode), m_ScriptAPI));
    };
    lupine["instantiate_scene"] = [this](const std::string& path, sol::optional<NodeRef> parent,
                                         sol::this_state ts) -> sol::object {
        sol::state_view lua(ts);
        if (!m_ScriptAPI) return sol::make_object(lua, sol::lua_nil);
        core::Node* parentNode = (parent && parent->IsValid()) ? parent->Lock().get() : nullptr;
        return WrapNode(lua, NodeRef::FromRaw(m_ScriptAPI->InstantiateScene(path, parentNode), m_ScriptAPI));
    };
    lupine["create_node"] = [this](const std::string& type, sol::optional<std::string> name,
                                   sol::this_state ts) -> sol::object {
        sol::state_view lua(ts);
        if (!m_ScriptAPI) return sol::make_object(lua, sol::lua_nil);
        core::Node* node = name ? m_ScriptAPI->CreateNode(type, *name) : m_ScriptAPI->CreateNode(type);
        return WrapNode(lua, NodeRef::FromRaw(node, m_ScriptAPI));
    };
    lupine["create_node_child"] = [this](const std::string& type, const std::string& name,
                                         sol::optional<NodeRef> parent, sol::this_state ts) -> sol::object {
        sol::state_view lua(ts);
        if (!m_ScriptAPI) return sol::make_object(lua, sol::lua_nil);
        core::Node* parentNode = (parent && parent->IsValid()) ? parent->Lock().get() : nullptr;
        return WrapNode(lua, NodeRef::FromRaw(m_ScriptAPI->CreateNodeChild(type, name, parentNode), m_ScriptAPI));
    };
    lupine["duplicate_node"] = [this](const NodeRef& node, sol::this_state ts) -> sol::object {
        sol::state_view lua(ts);
        if (!m_ScriptAPI) return sol::make_object(lua, sol::lua_nil);
        return WrapNode(lua, NodeRef::FromRaw(m_ScriptAPI->DuplicateNode(node.Lock().get()), m_ScriptAPI));
    };

    // ----------------------------------------------------------------------
    // Timers: create one-shot/repeating/named timers and list/manage them.
    // ----------------------------------------------------------------------
    // The callback may be the name of a script function or a Lua function value;
    // BindTransientCallback binds a function value to a generated name, since the
    // Timer component dispatches its timeout by name through the signal system.
    lupine["create_timer"] = [this](float delay, sol::object callback,
                                    sol::this_state ts) -> sol::object {
        sol::state_view lua(ts);
        if (!m_ScriptAPI) return sol::make_object(lua, sol::lua_nil);
        std::string callbackName = BindTransientCallback(callback, true);
        core::Component* timer = m_ScriptAPI->CreateTimerComponent(delay, callbackName, false, -1, "");
        return WrapTimer(lua, TimerRef::FromComponent(timer, m_ScriptAPI));
    };
    lupine["create_repeating_timer"] = [this](float interval, sol::object callback,
                                              sol::optional<int> repeatCount, sol::this_state ts) -> sol::object {
        sol::state_view lua(ts);
        if (!m_ScriptAPI) return sol::make_object(lua, sol::lua_nil);
        std::string callbackName = BindTransientCallback(callback, false);
        core::Component* timer = m_ScriptAPI->CreateTimerComponent(interval, callbackName, true,
                                                                   repeatCount.value_or(-1), "");
        return WrapTimer(lua, TimerRef::FromComponent(timer, m_ScriptAPI));
    };
    lupine["create_named_timer"] = [this](const std::string& name, float delay, sol::object callback,
                                          sol::optional<bool> repeating, sol::optional<int> repeatCount,
                                          sol::this_state ts) -> sol::object {
        sol::state_view lua(ts);
        if (!m_ScriptAPI) return sol::make_object(lua, sol::lua_nil);
        const bool repeats = repeating.value_or(false);
        std::string callbackName = BindTransientCallback(callback, !repeats);
        core::Component* timer = m_ScriptAPI->CreateTimerComponent(delay, callbackName, repeats,
                                                                   repeatCount.value_or(-1), name);
        return WrapTimer(lua, TimerRef::FromComponent(timer, m_ScriptAPI));
    };
    lupine["list_timers"] = [this](sol::this_state ts) -> sol::table {
        sol::state_view lua(ts);
        sol::table t = lua.create_table();
        if (!m_ScriptAPI) return t;
        int i = 1;
        for (core::Component* timer : m_ScriptAPI->ListTimers()) {
            TimerRef ref = TimerRef::FromComponent(timer, m_ScriptAPI);
            if (ref.IsValid()) t[i++] = ref;
        }
        return t;
    };

    // ----------------------------------------------------------------------
    // Tweens: animate a channel on the owner node to a target value.
    // ----------------------------------------------------------------------
    lupine["create_tween"] = [this](const std::string& channel, sol::object to, float duration,
                                    sol::optional<std::string> easing, sol::this_state ts) -> sol::object {
        sol::state_view lua(ts);
        if (!m_ScriptAPI) return sol::make_object(lua, sol::lua_nil);
        core::Component* tween = m_ScriptAPI->CreateTweenComponent(channel, LuaToJson(to), duration,
                                                                   easing.value_or("linear"), nullptr);
        return WrapTween(lua, TweenRef::FromComponent(tween, m_ScriptAPI));
    };
    lupine["list_tweens"] = [this](sol::this_state ts) -> sol::table {
        sol::state_view lua(ts);
        sol::table t = lua.create_table();
        if (!m_ScriptAPI) return t;
        int i = 1;
        for (core::Component* tween : m_ScriptAPI->ListTweens()) {
            TweenRef ref = TweenRef::FromComponent(tween, m_ScriptAPI);
            if (ref.IsValid()) t[i++] = ref;
        }
        return t;
    };
    lupine["create_sequence"] = [this](sol::this_state ts) -> sol::object {
        sol::state_view lua(ts);
        if (!m_ScriptAPI) return sol::make_object(lua, sol::lua_nil);
        return WrapSequence(lua, NodeRef::FromRaw(m_ScriptAPI->GetSelf(), m_ScriptAPI).CreateSequence());
    };

    // ----------------------------------------------------------------------
    // Sandboxed file I/O (res:// user:// temp:// only) + JSON
    // ----------------------------------------------------------------------
    lupine["read_text"] = [this](const std::string& path, sol::this_state ts) -> sol::object {
        sol::state_view lua(ts);
        std::string out;
        if (m_ScriptAPI && m_ScriptAPI->ReadTextFile(path, out)) {
            return sol::make_object(lua, out);
        }
        return sol::make_object(lua, sol::lua_nil);
    };
    lupine["write_text"] = [this](const std::string& path, const std::string& text) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->WriteTextFile(path, text) : false;
    };
    lupine["append_text"] = [this](const std::string& path, const std::string& text) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->AppendTextFile(path, text) : false;
    };
    lupine["read_bytes"] = [this](const std::string& path, sol::this_state ts) -> sol::object {
        sol::state_view lua(ts);
        std::vector<uint8_t> data;
        if (m_ScriptAPI && m_ScriptAPI->ReadBytesFile(path, data)) {
            return sol::make_object(lua, std::string(reinterpret_cast<const char*>(data.data()), data.size()));
        }
        return sol::make_object(lua, sol::lua_nil);
    };
    lupine["write_bytes"] = [this](const std::string& path, const std::string& data) -> bool {
        if (!m_ScriptAPI) return false;
        std::vector<uint8_t> bytes(data.begin(), data.end());
        return m_ScriptAPI->WriteBytesFile(path, bytes);
    };
    lupine["file_exists"] = [this](const std::string& path) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->FileExists(path) : false;
    };
    lupine["is_file"] = [this](const std::string& path) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->FileIsFile(path) : false;
    };
    lupine["is_dir"] = [this](const std::string& path) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->FileIsDirectory(path) : false;
    };
    lupine["remove_file"] = [this](const std::string& path) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->DeleteFilePath(path) : false;
    };
    lupine["delete_file"] = [this](const std::string& path) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->DeleteFilePath(path) : false;
    };
    lupine["make_dir"] = [this](const std::string& path) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->MakeDirectory(path) : false;
    };
    lupine["ensure_dir"] = [this](const std::string& path) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->MakeDirectory(path) : false;
    };
    lupine["list_dir"] = [this](const std::string& path, sol::this_state ts) -> sol::table {
        sol::state_view lua(ts);
        sol::table t = lua.create_table();
        if (m_ScriptAPI) {
            int i = 1;
            for (const std::string& name : m_ScriptAPI->ListDirectory(path)) t[i++] = name;
        }
        return t;
    };
    lupine["file_size"] = [this](const std::string& path) -> int64_t {
        return m_ScriptAPI ? m_ScriptAPI->GetFileSize(path) : -1;
    };

    lupine["to_json"] = [](sol::object value, sol::optional<bool> pretty) -> std::string {
        nlohmann::json j = LuaToJson(value);
        return pretty.value_or(false) ? j.dump(2) : j.dump();
    };
    lupine["from_json"] = [](const std::string& str, sol::this_state ts) -> sol::object {
        sol::state_view lua(ts);
        try {
            return JsonToLua(lua, nlohmann::json::parse(str));
        } catch (...) {
            return sol::make_object(lua, sol::lua_nil);
        }
    };
    lupine["read_json"] = [this](const std::string& path, sol::this_state ts) -> sol::object {
        sol::state_view lua(ts);
        std::string text;
        if (!m_ScriptAPI || !m_ScriptAPI->ReadTextFile(path, text)) {
            return sol::make_object(lua, sol::lua_nil);
        }
        try {
            return JsonToLua(lua, nlohmann::json::parse(text));
        } catch (...) {
            return sol::make_object(lua, sol::lua_nil);
        }
    };
    lupine["write_json"] = [this](const std::string& path, sol::object value, sol::optional<bool> pretty) -> bool {
        if (!m_ScriptAPI) return false;
        nlohmann::json j = LuaToJson(value);
        return m_ScriptAPI->WriteTextFile(path, pretty.value_or(false) ? j.dump(2) : j.dump());
    };

    // Save games
    lupine["save_game"] = [this](const std::string& slot, sol::object data, sol::optional<sol::object> meta) -> bool {
        if (!m_ScriptAPI) return false;
        nlohmann::json metaJson = meta ? LuaToJson(*meta) : nlohmann::json::object();
        return m_ScriptAPI->SaveGame(slot, LuaToJson(data), metaJson);
    };
    lupine["load_game"] = [this](const std::string& slot, sol::this_state ts) -> sol::object {
        sol::state_view lua(ts);
        if (!m_ScriptAPI) return sol::make_object(lua, sol::lua_nil);
        nlohmann::json result = m_ScriptAPI->LoadGame(slot);
        if (result.is_null()) return sol::make_object(lua, sol::lua_nil);
        return JsonToLua(lua, result);
    };
    lupine["save_slot_exists"] = [this](const std::string& slot) -> bool {
        return m_ScriptAPI && m_ScriptAPI->SaveSlotExists(slot);
    };
    lupine["delete_save_slot"] = [this](const std::string& slot) -> bool {
        return m_ScriptAPI && m_ScriptAPI->DeleteSaveSlot(slot);
    };
    lupine["copy_save_slot"] = [this](const std::string& from, const std::string& to) -> bool {
        return m_ScriptAPI && m_ScriptAPI->CopySaveSlot(from, to, true);
    };
    lupine["rename_save_slot"] = [this](const std::string& from, const std::string& to) -> bool {
        return m_ScriptAPI && m_ScriptAPI->RenameSaveSlot(from, to, true);
    };
    lupine["list_save_slots"] = [this](sol::this_state ts) -> sol::object {
        sol::state_view lua(ts);
        if (!m_ScriptAPI) return sol::make_object(lua, sol::lua_nil);
        return JsonToLua(lua, m_ScriptAPI->ListSaveSlots());
    };
    lupine["list_save_slot_infos"] = [this](sol::this_state ts) -> sol::object {
        sol::state_view lua(ts);
        if (!m_ScriptAPI) return sol::make_object(lua, sol::lua_nil);
        return JsonToLua(lua, m_ScriptAPI->ListSaveSlotInfos());
    };
    lupine["get_save_slot_info"] = [this](const std::string& slot, sol::this_state ts) -> sol::object {
        sol::state_view lua(ts);
        if (!m_ScriptAPI) return sol::make_object(lua, sol::lua_nil);
        nlohmann::json result = m_ScriptAPI->GetSaveSlotInfo(slot);
        if (result.is_null()) return sol::make_object(lua, sol::lua_nil);
        return JsonToLua(lua, result);
    };
    lupine["quick_save"] = [this](sol::object data, sol::optional<sol::object> meta) -> bool {
        if (!m_ScriptAPI) return false;
        nlohmann::json metaJson = meta ? LuaToJson(*meta) : nlohmann::json::object();
        return m_ScriptAPI->QuickSaveGame(LuaToJson(data), metaJson);
    };
    lupine["quick_load"] = [this](sol::this_state ts) -> sol::object {
        sol::state_view lua(ts);
        if (!m_ScriptAPI) return sol::make_object(lua, sol::lua_nil);
        nlohmann::json result = m_ScriptAPI->QuickLoadGame();
        if (result.is_null()) return sol::make_object(lua, sol::lua_nil);
        return JsonToLua(lua, result);
    };
    lupine["has_quick_save"] = [this]() -> bool { return m_ScriptAPI && m_ScriptAPI->HasQuickSave(); };
    lupine["auto_save"] = [this](sol::object data, sol::optional<sol::object> meta) -> bool {
        if (!m_ScriptAPI) return false;
        nlohmann::json metaJson = meta ? LuaToJson(*meta) : nlohmann::json::object();
        return m_ScriptAPI->AutoSaveGame(LuaToJson(data), metaJson);
    };
    lupine["has_auto_save"] = [this]() -> bool { return m_ScriptAPI && m_ScriptAPI->HasAutoSave(); };
    lupine["get_last_save_error"] = [this]() -> std::string {
        return m_ScriptAPI ? m_ScriptAPI->GetLastSaveError() : std::string("Success");
    };
    lupine["set_save_directory"] = [this](const std::string& dir) { if (m_ScriptAPI) m_ScriptAPI->SetSaveDirectory(dir); };
    lupine["set_save_format"] = [this](const std::string& fmt) { if (m_ScriptAPI) m_ScriptAPI->SetSaveFormat(fmt); };
    lupine["set_save_schema_version"] = [this](int v) { if (m_ScriptAPI) m_ScriptAPI->SetSaveSchemaVersion(v); };
    lupine["get_save_schema_version"] = [this]() -> int { return m_ScriptAPI ? m_ScriptAPI->GetSaveSchemaVersion() : 0; };
    lupine["set_save_obfuscation_key"] = [this](const std::string& key) { if (m_ScriptAPI) m_ScriptAPI->SetSaveObfuscationKey(key); };
    lupine["set_quick_save_slot"] = [this](const std::string& slot) { if (m_ScriptAPI) m_ScriptAPI->SetQuickSaveSlot(slot); };
    lupine["set_auto_save_slot"] = [this](const std::string& slot) { if (m_ScriptAPI) m_ScriptAPI->SetAutoSaveSlot(slot); };
    lupine["capture_scene_state"] = [this](sol::optional<std::string> group, sol::this_state ts) -> sol::object {
        sol::state_view lua(ts);
        if (!m_ScriptAPI) return sol::make_object(lua, sol::lua_nil);
        return JsonToLua(lua, m_ScriptAPI->CaptureSceneState(group.value_or("persistent")));
    };
    lupine["restore_scene_state"] = [this](sol::object captured) -> int {
        return m_ScriptAPI ? m_ScriptAPI->RestoreSceneState(LuaToJson(captured)) : 0;
    };

    // user:// helpers
    lupine["user_dir"] = []() -> std::string { return "user://"; };
    lupine["res_dir"] = []() -> std::string { return "res://"; };
    lupine["temp_dir"] = []() -> std::string { return "temp://"; };
    lupine["join_path"] = [](sol::variadic_args va) -> std::string {
        std::string result;
        for (auto arg : va) {
            std::string part = arg.as<std::string>();
            if (part.empty()) continue;
            if (result.empty()) {
                result = part;
            } else if (result.back() == '/') {
                result += (part.front() == '/') ? part.substr(1) : part;
            } else {
                result += (part.front() == '/') ? part : ("/" + part);
            }
        }
        return result;
    };

    // ----------------------------------------------------------------------
    // Signals & global event bus
    // ----------------------------------------------------------------------
    lupine["CONNECT_DEFERRED"] = static_cast<uint32_t>(core::Connect_Deferred);
    lupine["CONNECT_ONESHOT"]  = static_cast<uint32_t>(core::Connect_OneShot);

    lupine["MOUSE_MODE_VISIBLE"]         = 0;
    lupine["MOUSE_MODE_HIDDEN"]          = 1;
    lupine["MOUSE_MODE_CAPTURED"]        = 2;
    lupine["MOUSE_MODE_CONFINED"]        = 3;
    lupine["MOUSE_MODE_CONFINED_HIDDEN"] = 4;

    lupine["emit"] = [this](const std::string& signal, sol::variadic_args va) {
        if (m_ScriptAPI) m_ScriptAPI->EmitSignal(signal, VarargsToJson(va));
    };
    lupine["connect"] = [this](const std::string& signal, const NodeRef& target,
                               const std::string& method, sol::optional<uint32_t> flags) -> uint64_t {
        if (!m_ScriptAPI) return 0;
        auto targetNode = target.Lock();
        return m_ScriptAPI->ConnectSignal(signal, targetNode.get(), method, flags.value_or(0));
    };
    lupine["disconnect"] = [this](const std::string& signal, uint64_t id) {
        if (m_ScriptAPI) m_ScriptAPI->DisconnectSignal(signal, id);
    };
    lupine["is_connected"] = [this](const std::string& signal) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsSignalConnected(signal) : false;
    };
    lupine["add_user_signal"] = [this](const std::string& name) {
        if (m_ScriptAPI) m_ScriptAPI->AddUserSignal(name);
    };
    lupine["call_deferred"] = [this](const std::string& method, sol::variadic_args va) {
        if (m_ScriptAPI) m_ScriptAPI->CallDeferred(method, VarargsToJson(va));
    };
    lupine["emit_event"] = [this](const std::string& event, sol::variadic_args va) {
        if (m_ScriptAPI) m_ScriptAPI->EmitEvent(event, VarargsToJson(va));
    };
    lupine["subscribe"] = [this](const std::string& event, const std::string& method,
                                 sol::optional<uint32_t> flags) -> uint64_t {
        return m_ScriptAPI ? m_ScriptAPI->SubscribeEvent(event, method, flags.value_or(0)) : 0;
    };
    lupine["unsubscribe"] = [this](const std::string& event, uint64_t id) {
        if (m_ScriptAPI) m_ScriptAPI->UnsubscribeEvent(event, id);
    };

    // ------------------------------------------------------------------
    // Networking / multiplayer ("Network" global table)
    // ------------------------------------------------------------------
    sol::table network = m_LuaState.create_named_table("Network");
    network["start_server"] = [](int port, sol::optional<int> maxPeers) -> bool {
        network::NetworkConfig config = network::NetworkManager::GetInstance().GetDefaultConfig();
        config.port = static_cast<uint16_t>(port);
        if (maxPeers) {
            config.maxPeers = static_cast<uint32_t>(*maxPeers);
        }
        return network::NetworkManager::GetInstance().StartServer(config);
    };
    network["start_host"] = [](int port, sol::optional<int> maxPeers) -> bool {
        network::NetworkConfig config = network::NetworkManager::GetInstance().GetDefaultConfig();
        config.port = static_cast<uint16_t>(port);
        if (maxPeers) {
            config.maxPeers = static_cast<uint32_t>(*maxPeers);
        }
        return network::NetworkManager::GetInstance().StartHost(config);
    };
    network["connect"] = [](const std::string& address, int port) -> bool {
        network::NetworkConfig config = network::NetworkManager::GetInstance().GetDefaultConfig();
        config.address = address;
        config.port = static_cast<uint16_t>(port);
        return network::NetworkManager::GetInstance().Connect(config);
    };
    network["disconnect"] = []() {
        network::NetworkManager::GetInstance().Disconnect();
    };
    network["is_server"] = []() -> bool { return network::NetworkManager::GetInstance().IsServer(); };
    network["is_client"] = []() -> bool { return network::NetworkManager::GetInstance().IsClient(); };
    network["is_active"] = []() -> bool { return network::NetworkManager::GetInstance().IsActive(); };
    network["get_local_peer_id"] = []() -> int {
        return static_cast<int>(network::NetworkManager::GetInstance().GetLocalPeerId());
    };
    network["get_peer_count"] = []() -> int {
        return static_cast<int>(network::NetworkManager::GetInstance().GetPeerCount());
    };
    network["get_peers"] = [](sol::this_state ts) -> sol::table {
        sol::state_view lua(ts);
        sol::table t = lua.create_table();
        int i = 1;
        for (const network::PeerInfo& peer : network::NetworkManager::GetInstance().GetPeers()) {
            t[i++] = static_cast<int>(peer.id);
        }
        return t;
    };
    network["connect_signal"] = [](const std::string& event, const NodeRef& target,
                                   const std::string& method) -> uint64_t {
        std::shared_ptr<core::Node> node = target.Lock();
        if (!node) {
            return 0;
        }
        return network::NetworkManager::GetInstance().Events().Connect(event, node.get(), method);
    };
    network["disconnect_signal"] = [](const std::string& event, uint64_t id) {
        network::NetworkManager::GetInstance().Events().Disconnect(event, id);
    };
    network["get_peer_rtt"] = [](int peerId) -> double {
        for (const network::PeerInfo& peer : network::NetworkManager::GetInstance().GetPeers()) {
            if (static_cast<int>(peer.id) == peerId) {
                return static_cast<double>(peer.roundTripMs);
            }
        }
        return 0.0;
    };
    network["get_stats"] = [](sol::this_state ts) -> sol::table {
        sol::state_view lua(ts);
        sol::table t = lua.create_table();
        const network::NetworkStats s = network::NetworkManager::GetInstance().GetStats();
        t["peer_count"] = static_cast<int>(s.peerCount);
        t["bytes_in_per_sec"] = s.bytesInPerSec;
        t["bytes_out_per_sec"] = s.bytesOutPerSec;
        t["total_bytes_in"] = static_cast<double>(s.totalBytesIn);
        t["total_bytes_out"] = static_cast<double>(s.totalBytesOut);
        t["average_rtt_ms"] = s.averageRttMs;
        t["packet_loss_percent"] = s.averagePacketLossPercent;
        t["snapshots_sent"] = static_cast<double>(s.snapshotsSent);
        t["snapshots_received"] = static_cast<double>(s.snapshotsReceived);
        return t;
    };
    network["get_peer_loss"] = [](int peerId) -> double {
        network::PeerInfo info;
        if (network::NetworkManager::GetInstance().GetPeerInfo(
                static_cast<network::PeerId>(peerId), info)) {
            return static_cast<double>(info.packetLossPercent);
        }
        return 0.0;
    };
    network["get_peer_jitter"] = [](int peerId) -> double {
        network::PeerInfo info;
        if (network::NetworkManager::GetInstance().GetPeerInfo(
                static_cast<network::PeerId>(peerId), info)) {
            return static_cast<double>(info.jitterMs);
        }
        return 0.0;
    };
    network["kick_peer"] = [](int peerId) -> bool {
        return network::NetworkManager::GetInstance().KickPeer(static_cast<network::PeerId>(peerId));
    };
    network["set_interest_2d"] = [](float x, float y) {
        network::NetworkManager::GetInstance().SetInterestPosition2D(x, y);
    };
    network["set_interest_3d"] = [](float x, float y, float z) {
        network::NetworkManager::GetInstance().SetInterestPosition3D(x, y, z);
    };

    // LAN discovery
    network["start_lan_advertising"] = []() -> bool {
        return network::NetworkManager::GetInstance().StartLanAdvertising();
    };
    network["stop_lan_advertising"] = []() {
        network::NetworkManager::GetInstance().StopLanAdvertising();
    };
    network["is_lan_advertising"] = []() -> bool {
        return network::NetworkManager::GetInstance().IsLanAdvertising();
    };
    network["start_lan_discovery"] = [](const std::string& gameId,
                                        sol::optional<int> discoveryPort) -> bool {
        return network::NetworkManager::GetInstance().StartLanDiscovery(
            gameId, static_cast<uint16_t>(discoveryPort.value_or(7779)));
    };
    network["stop_lan_discovery"] = []() {
        network::NetworkManager::GetInstance().StopLanDiscovery();
    };
    network["is_lan_discovering"] = []() -> bool {
        return network::NetworkManager::GetInstance().IsLanDiscovering();
    };
    network["get_discovered_servers"] = [](sol::this_state ts) -> sol::table {
        sol::state_view lua(ts);
        sol::table list = lua.create_table();
        int i = 1;
        for (const network::LanServerInfo& server :
             network::NetworkManager::GetInstance().GetDiscoveredServers()) {
            sol::table entry = lua.create_table();
            entry["game_id"] = server.gameId;
            entry["name"] = server.name;
            entry["address"] = server.address;
            entry["port"] = static_cast<int>(server.gamePort);
            entry["player_count"] = static_cast<int>(server.playerCount);
            entry["max_players"] = static_cast<int>(server.maxPlayers);
            entry["protocol_version"] = static_cast<int>(server.protocolVersion);
            entry["age_seconds"] = static_cast<double>(server.ageSeconds);
            list[i++] = entry;
        }
        return list;
    };

    // Direct key input
    lupine["is_key_pressed"] = [this](int keyCode) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsKeyPressed(keyCode) : false;
    };
    lupine["is_key_just_pressed"] = [this](int keyCode) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsKeyJustPressed(keyCode) : false;
    };
    lupine["is_key_just_released"] = [this](int keyCode) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsKeyJustReleased(keyCode) : false;
    };

    // Mouse input
    lupine["is_mouse_button_pressed"] = [this](int button) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsMouseButtonPressed(button) : false;
    };
    lupine["is_mouse_button_just_pressed"] = [this](int button) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsMouseButtonJustPressed(button) : false;
    };
    lupine["get_mouse_position"] = [this]() -> sol::as_table_t<std::vector<float>> {
        if (m_ScriptAPI) {
            auto pos = m_ScriptAPI->GetMousePosition();
            return sol::as_table(std::vector<float>{pos.x, pos.y});
        }
        return sol::as_table(std::vector<float>{0.0f, 0.0f});
    };
    lupine["is_mouse_button_just_released"] = [this](int button) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsMouseButtonJustReleased(button) : false;
    };
    lupine["get_mouse_delta"] = [this]() -> sol::as_table_t<std::vector<float>> {
        if (m_ScriptAPI) {
            auto delta = m_ScriptAPI->GetMouseDelta();
            return sol::as_table(std::vector<float>{delta.x, delta.y});
        }
        return sol::as_table(std::vector<float>{0.0f, 0.0f});
    };
    lupine["get_mouse_scroll_delta"] = [this]() -> sol::as_table_t<std::vector<float>> {
        if (m_ScriptAPI) {
            auto scroll = m_ScriptAPI->GetMouseScrollDelta();
            return sol::as_table(std::vector<float>{scroll.x, scroll.y});
        }
        return sol::as_table(std::vector<float>{0.0f, 0.0f});
    };

    // Mapped-input analog helpers
    lupine["get_action_strength"] = [this](const std::string& action, sol::optional<int> player) -> float {
        return m_ScriptAPI ? m_ScriptAPI->GetActionStrength(action, player.value_or(-1)) : 0.0f;
    };
    lupine["get_vector"] = [this](const std::string& negX, const std::string& posX,
                                  const std::string& negY, const std::string& posY,
                                  sol::optional<int> player) -> sol::as_table_t<std::vector<float>> {
        if (m_ScriptAPI) {
            auto v = m_ScriptAPI->GetVector(negX, posX, negY, posY, player.value_or(-1));
            return sol::as_table(std::vector<float>{v.x, v.y});
        }
        return sol::as_table(std::vector<float>{0.0f, 0.0f});
    };

    // Gamepad input
    lupine["is_gamepad_connected"] = [this](sol::optional<int> gamepadId) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsGamepadConnected(gamepadId.value_or(0)) : false;
    };
    lupine["get_gamepad_count"] = [this]() -> int {
        return m_ScriptAPI ? m_ScriptAPI->GetGamepadCount() : 0;
    };
    lupine["get_connected_gamepad_ids"] = [this]() -> sol::as_table_t<std::vector<int>> {
        return sol::as_table(m_ScriptAPI ? m_ScriptAPI->GetConnectedGamepadIds() : std::vector<int>{});
    };
    lupine["get_gamepad_name"] = [this](sol::optional<int> gamepadId) -> std::string {
        return m_ScriptAPI ? m_ScriptAPI->GetGamepadName(gamepadId.value_or(0)) : std::string();
    };
    lupine["is_gamepad_button_pressed"] = [this](int button, sol::optional<int> gamepadId) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsGamepadButtonPressed(button, gamepadId.value_or(0)) : false;
    };
    lupine["is_gamepad_button_just_pressed"] = [this](int button, sol::optional<int> gamepadId) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsGamepadButtonJustPressed(button, gamepadId.value_or(0)) : false;
    };
    lupine["is_gamepad_button_just_released"] = [this](int button, sol::optional<int> gamepadId) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsGamepadButtonJustReleased(button, gamepadId.value_or(0)) : false;
    };
    lupine["get_gamepad_axis"] = [this](int axis, sol::optional<int> gamepadId) -> float {
        return m_ScriptAPI ? m_ScriptAPI->GetGamepadAxis(axis, gamepadId.value_or(0)) : 0.0f;
    };
    lupine["set_gamepad_vibration"] = [this](int gamepadId, float left, float right,
                                             sol::optional<float> durationSeconds) {
        if (m_ScriptAPI) m_ScriptAPI->SetGamepadVibration(gamepadId, left, right, durationSeconds.value_or(0.0f));
    };
    lupine["stop_gamepad_vibration"] = [this](int gamepadId) {
        if (m_ScriptAPI) m_ScriptAPI->StopGamepadVibration(gamepadId);
    };
    lupine["set_gamepad_deadzone"] = [this](float deadzone) {
        if (m_ScriptAPI) m_ScriptAPI->SetGamepadDeadzone(deadzone);
    };
    lupine["get_gamepad_deadzone"] = [this]() -> float {
        return m_ScriptAPI ? m_ScriptAPI->GetGamepadDeadzone() : 0.0f;
    };

    // Touch input
    lupine["is_touch_available"] = [this]() -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsTouchAvailable() : false;
    };
    lupine["is_touching"] = [this]() -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsTouching() : false;
    };
    lupine["get_touch_count"] = [this]() -> int {
        return m_ScriptAPI ? m_ScriptAPI->GetTouchCount() : 0;
    };
    lupine["get_touch_position"] = [this](sol::optional<int> index) -> sol::as_table_t<std::vector<float>> {
        if (m_ScriptAPI) {
            auto pos = m_ScriptAPI->GetTouchPosition(index.value_or(0));
            return sol::as_table(std::vector<float>{pos.x, pos.y});
        }
        return sol::as_table(std::vector<float>{0.0f, 0.0f});
    };
    lupine["is_touch_just_started"] = [this]() -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsTouchJustStarted() : false;
    };
    lupine["is_touch_just_ended"] = [this]() -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsTouchJustEnded() : false;
    };

    // Clipboard
    lupine["get_clipboard"] = [this]() -> std::string {
        return m_ScriptAPI ? m_ScriptAPI->GetClipboardText() : std::string();
    };
    lupine["set_clipboard"] = [this](const std::string& text) {
        if (m_ScriptAPI) m_ScriptAPI->SetClipboardText(text);
    };

    // Active device detection (for auto-swapping button prompts)
    lupine["get_active_device_type"] = [this]() -> int {
        return m_ScriptAPI ? m_ScriptAPI->GetActiveDeviceType() : 0;
    };
    lupine["get_last_gamepad_id"] = [this]() -> int {
        return m_ScriptAPI ? m_ScriptAPI->GetLastGamepadId() : 0;
    };
    lupine["get_gamepad_type"] = [this](sol::optional<int> gamepadId) -> int {
        return m_ScriptAPI ? m_ScriptAPI->GetGamepadType(gamepadId.value_or(0)) : 0;
    };

    // Input contexts / action sets
    lupine["enable_input_context"] = [this](const std::string& context) {
        if (m_ScriptAPI) m_ScriptAPI->EnableInputContext(context);
    };
    lupine["disable_input_context"] = [this](const std::string& context) {
        if (m_ScriptAPI) m_ScriptAPI->DisableInputContext(context);
    };
    lupine["set_input_context_active"] = [this](const std::string& context, bool active) {
        if (m_ScriptAPI) m_ScriptAPI->SetInputContextActive(context, active);
    };
    lupine["is_input_context_active"] = [this](const std::string& context) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsInputContextActive(context) : false;
    };
    lupine["set_exclusive_input_context"] = [this](const std::string& context) {
        if (m_ScriptAPI) m_ScriptAPI->SetExclusiveInputContext(context);
    };
    lupine["get_active_input_contexts"] = [this]() -> sol::as_table_t<std::vector<std::string>> {
        return sol::as_table(m_ScriptAPI ? m_ScriptAPI->GetActiveInputContexts() : std::vector<std::string>{});
    };
    lupine["set_action_enabled"] = [this](const std::string& action, bool enabled) {
        if (m_ScriptAPI) m_ScriptAPI->SetActionEnabled(action, enabled);
    };
    lupine["set_axis_enabled"] = [this](const std::string& axis, bool enabled) {
        if (m_ScriptAPI) m_ScriptAPI->SetAxisEnabled(axis, enabled);
    };

    // Local multiplayer player slots
    lupine["set_player_count"] = [this](int count) {
        if (m_ScriptAPI) m_ScriptAPI->SetPlayerCount(count);
    };
    lupine["get_player_count"] = [this]() -> int {
        return m_ScriptAPI ? m_ScriptAPI->GetPlayerCount() : 0;
    };
    lupine["clear_player_assignments"] = [this]() {
        if (m_ScriptAPI) m_ScriptAPI->ClearPlayerAssignments();
    };
    lupine["assign_keyboard_mouse_to_player"] = [this](int player) {
        if (m_ScriptAPI) m_ScriptAPI->AssignKeyboardMouseToPlayer(player);
    };
    lupine["assign_gamepad_to_player"] = [this](int player, int gamepadId) {
        if (m_ScriptAPI) m_ScriptAPI->AssignGamepadToPlayer(player, gamepadId);
    };
    lupine["unassign_gamepad"] = [this](int gamepadId) {
        if (m_ScriptAPI) m_ScriptAPI->UnassignGamepad(gamepadId);
    };
    lupine["get_player_for_gamepad"] = [this](int gamepadId) -> int {
        return m_ScriptAPI ? m_ScriptAPI->GetPlayerForGamepad(gamepadId) : -1;
    };
    lupine["get_player_for_keyboard_mouse"] = [this]() -> int {
        return m_ScriptAPI ? m_ScriptAPI->GetPlayerForKeyboardMouse() : -1;
    };
    lupine["player_owns_keyboard_mouse"] = [this](int player) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->PlayerOwnsKeyboardMouse(player) : false;
    };
    lupine["get_player_gamepads"] = [this](int player) -> sol::as_table_t<std::vector<int>> {
        return sol::as_table(m_ScriptAPI ? m_ScriptAPI->GetPlayerGamepads(player) : std::vector<int>{});
    };
    lupine["set_auto_join_enabled"] = [this](bool enabled) {
        if (m_ScriptAPI) m_ScriptAPI->SetAutoJoinEnabled(enabled);
    };
    lupine["is_auto_join_enabled"] = [this]() -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsAutoJoinEnabled() : false;
    };

    // Runtime rebinding
    lupine["add_action_key"] = [this](const std::string& action, int keyCode) {
        if (m_ScriptAPI) m_ScriptAPI->AddActionKey(action, keyCode);
    };
    lupine["add_action_mouse_button"] = [this](const std::string& action, int button) {
        if (m_ScriptAPI) m_ScriptAPI->AddActionMouseButton(action, button);
    };
    lupine["add_action_gamepad_button"] = [this](const std::string& action, int button,
                                                 sol::optional<int> gamepadId) {
        if (m_ScriptAPI) m_ScriptAPI->AddActionGamepadButton(action, button, gamepadId.value_or(0));
    };
    lupine["add_action_gamepad_axis"] = [this](const std::string& action, int axis,
                                               sol::optional<float> scale, sol::optional<int> gamepadId) {
        if (m_ScriptAPI) m_ScriptAPI->AddActionGamepadAxis(action, axis, scale.value_or(1.0f), gamepadId.value_or(0));
    };
    lupine["remove_action_binding"] = [this](const std::string& action, int index) {
        if (m_ScriptAPI) m_ScriptAPI->RemoveActionBinding(action, index);
    };
    lupine["clear_action_bindings"] = [this](const std::string& action) {
        if (m_ScriptAPI) m_ScriptAPI->ClearActionBindings(action);
    };
    lupine["get_action_bindings"] = [this](const std::string& action, sol::this_state ts) -> sol::object {
        sol::state_view lua(ts);
        if (!m_ScriptAPI) return sol::make_object(lua, sol::nil);
        return JsonToLua(lua, m_ScriptAPI->GetActionBindings(action));
    };
    lupine["add_axis_key"] = [this](const std::string& axis, int keyCode, sol::optional<float> scale) {
        if (m_ScriptAPI) m_ScriptAPI->AddAxisKey(axis, keyCode, scale.value_or(1.0f));
    };
    lupine["add_axis_gamepad_axis"] = [this](const std::string& axis, int gamepadAxis,
                                             sol::optional<float> scale, sol::optional<int> gamepadId) {
        if (m_ScriptAPI) m_ScriptAPI->AddAxisGamepadAxis(axis, gamepadAxis, scale.value_or(1.0f), gamepadId.value_or(0));
    };
    lupine["remove_axis_binding"] = [this](const std::string& axis, int index) {
        if (m_ScriptAPI) m_ScriptAPI->RemoveAxisBinding(axis, index);
    };
    lupine["clear_axis_bindings"] = [this](const std::string& axis) {
        if (m_ScriptAPI) m_ScriptAPI->ClearAxisBindings(axis);
    };
    lupine["get_axis_bindings"] = [this](const std::string& axis, sol::this_state ts) -> sol::object {
        sol::state_view lua(ts);
        if (!m_ScriptAPI) return sol::make_object(lua, sol::nil);
        return JsonToLua(lua, m_ScriptAPI->GetAxisBindings(axis));
    };
    lupine["save_input_map"] = [this](const std::string& path) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->SaveInputMap(path) : false;
    };
    lupine["load_input_map"] = [this](const std::string& path) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->LoadInputMap(path) : false;
    };

    // Input capture (rebind menus)
    lupine["start_input_capture"] = [this]() {
        if (m_ScriptAPI) m_ScriptAPI->StartInputCapture();
    };
    lupine["start_input_capture_mask"] = [this](bool keyboard, bool mouse, bool gamepad) {
        if (m_ScriptAPI) m_ScriptAPI->StartInputCaptureMask(keyboard, mouse, gamepad);
    };
    lupine["cancel_input_capture"] = [this]() {
        if (m_ScriptAPI) m_ScriptAPI->CancelInputCapture();
    };
    lupine["is_capturing_input"] = [this]() -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsCapturingInput() : false;
    };
    lupine["is_input_capture_complete"] = [this]() -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsInputCaptureComplete() : false;
    };
    lupine["get_captured_binding"] = [this](sol::this_state ts) -> sol::object {
        sol::state_view lua(ts);
        if (!m_ScriptAPI) return sol::make_object(lua, sol::nil);
        return JsonToLua(lua, m_ScriptAPI->GetCapturedBinding());
    };
    lupine["clear_captured_binding"] = [this]() {
        if (m_ScriptAPI) m_ScriptAPI->ClearCapturedBinding();
    };
    lupine["apply_captured_binding_to_action"] = [this](const std::string& action) {
        if (m_ScriptAPI) m_ScriptAPI->ApplyCapturedBindingToAction(action);
    };

    // Glyph / prompt resolution
    lupine["get_action_glyph"] = [this](const std::string& action, sol::optional<int> player,
                                        sol::optional<int> deviceOverride, sol::this_state ts) -> sol::object {
        sol::state_view lua(ts);
        if (!m_ScriptAPI) return sol::make_object(lua, sol::nil);
        return JsonToLua(lua, m_ScriptAPI->GetActionGlyph(action, player.value_or(-1), deviceOverride.value_or(-1)));
    };
    lupine["get_action_glyphs"] = [this](const std::string& action, sol::this_state ts) -> sol::object {
        sol::state_view lua(ts);
        if (!m_ScriptAPI) return sol::make_object(lua, sol::nil);
        return JsonToLua(lua, m_ScriptAPI->GetActionGlyphs(action));
    };
    lupine["set_glyph_label"] = [this](const std::string& glyphId, const std::string& label) {
        if (m_ScriptAPI) m_ScriptAPI->SetGlyphLabel(glyphId, label);
    };
    lupine["set_glyph_art"] = [this](const std::string& glyphId, const std::string& artPath) {
        if (m_ScriptAPI) m_ScriptAPI->SetGlyphArt(glyphId, artPath);
    };
    lupine["clear_glyph_override"] = [this](const std::string& glyphId) {
        if (m_ScriptAPI) m_ScriptAPI->ClearGlyphOverride(glyphId);
    };
    lupine["clear_glyph_overrides"] = [this]() {
        if (m_ScriptAPI) m_ScriptAPI->ClearGlyphOverrides();
    };
    lupine["load_glyph_map"] = [this](const std::string& path) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->LoadGlyphMap(path) : false;
    };
    lupine["save_glyph_map"] = [this](const std::string& path) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->SaveGlyphMap(path) : false;
    };

    // Action delegation (connect a script function to an input action / event)
    lupine["connect_action"] = [this](const std::string& action, const std::string& method,
                                      sol::optional<uint32_t> flags) -> uint64_t {
        return m_ScriptAPI ? m_ScriptAPI->ConnectInputAction(action, method, flags.value_or(0)) : 0;
    };
    lupine["disconnect_action"] = [this](const std::string& action, uint64_t id) {
        if (m_ScriptAPI) m_ScriptAPI->DisconnectInputAction(action, id);
    };
    lupine["connect_device_changed"] = [this](const std::string& method,
                                              sol::optional<uint32_t> flags) -> uint64_t {
        return m_ScriptAPI ? m_ScriptAPI->ConnectDeviceChanged(method, flags.value_or(0)) : 0;
    };
    lupine["connect_input_captured"] = [this](const std::string& method,
                                              sol::optional<uint32_t> flags) -> uint64_t {
        return m_ScriptAPI ? m_ScriptAPI->ConnectInputCaptured(method, flags.value_or(0)) : 0;
    };

    // Event-driven action matching (inside on_input_event)
    lupine["event_is_action"] = [this](const sol::object& event, const std::string& action) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->EventIsAction(LuaToJson(event), action) : false;
    };
    lupine["event_is_action_pressed"] = [this](const sol::object& event, const std::string& action) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->EventIsActionPressed(LuaToJson(event), action) : false;
    };
    lupine["event_is_action_released"] = [this](const sol::object& event, const std::string& action) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->EventIsActionReleased(LuaToJson(event), action) : false;
    };

    // Window / Display
    lupine["set_window_title"] = [this](const std::string& title) {
        if (m_ScriptAPI) m_ScriptAPI->SetWindowTitle(title);
    };
    lupine["get_window_title"] = [this]() -> std::string {
        return m_ScriptAPI ? m_ScriptAPI->GetWindowTitle() : std::string();
    };
    lupine["set_fullscreen"] = [this](bool fullscreen) {
        if (m_ScriptAPI) m_ScriptAPI->SetFullscreen(fullscreen);
    };
    lupine["is_fullscreen"] = [this]() -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsFullscreen() : false;
    };
    lupine["set_vsync"] = [this](bool enabled) {
        if (m_ScriptAPI) m_ScriptAPI->SetVSync(enabled);
    };
    lupine["is_vsync"] = [this]() -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsVSync() : false;
    };
    lupine["set_window_size"] = [this](int width, int height) {
        if (m_ScriptAPI) m_ScriptAPI->SetWindowSize(width, height);
    };
    lupine["get_window_size"] = [this]() -> sol::as_table_t<std::vector<float>> {
        if (m_ScriptAPI) {
            auto size = m_ScriptAPI->GetWindowSize();
            return sol::as_table(std::vector<float>{size.x, size.y});
        }
        return sol::as_table(std::vector<float>{0.0f, 0.0f});
    };
    lupine["get_screen_size"] = [this]() -> sol::as_table_t<std::vector<float>> {
        if (m_ScriptAPI) {
            auto size = m_ScriptAPI->GetScreenSize();
            return sol::as_table(std::vector<float>{size.x, size.y});
        }
        return sol::as_table(std::vector<float>{0.0f, 0.0f});
    };
    lupine["maximize_window"] = [this]() {
        if (m_ScriptAPI) m_ScriptAPI->MaximizeWindow();
    };
    lupine["minimize_window"] = [this]() {
        if (m_ScriptAPI) m_ScriptAPI->MinimizeWindow();
    };
    lupine["restore_window"] = [this]() {
        if (m_ScriptAPI) m_ScriptAPI->RestoreWindow();
    };
    lupine["set_mouse_mode"] = [this](int mode) {
        if (m_ScriptAPI) m_ScriptAPI->SetMouseMode(mode);
    };
    lupine["get_mouse_mode"] = [this]() -> int {
        return m_ScriptAPI ? m_ScriptAPI->GetMouseMode() : 0;
    };
    lupine["set_mouse_cursor_visible"] = [this](bool visible) {
        if (m_ScriptAPI) m_ScriptAPI->SetMouseCursorVisible(visible);
    };
    lupine["is_mouse_cursor_visible"] = [this]() -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsMouseCursorVisible() : false;
    };

    // Direct audio playback
    lupine["play_audio"] = [this](const std::string& path, sol::optional<std::string> bus,
                                  sol::optional<bool> loop, sol::optional<float> volume) -> std::string {
        return m_ScriptAPI ? m_ScriptAPI->PlayAudio(path, bus.value_or("Master"), loop.value_or(false),
                                                    volume.value_or(1.0f)) : std::string();
    };
    lupine["play_audio_3d"] = [this](const std::string& path, float x, float y, float z,
                                     sol::optional<std::string> bus, sol::optional<bool> loop,
                                     sol::optional<float> volume) -> std::string {
        return m_ScriptAPI ? m_ScriptAPI->PlayAudio3D(path, math::Vec3(x, y, z), bus.value_or("Master"),
                                                      loop.value_or(false), volume.value_or(1.0f)) : std::string();
    };
    lupine["play_audio_scheduled"] = [this](const std::string& path, float delaySeconds,
                                            sol::optional<std::string> bus, sol::optional<bool> loop,
                                            sol::optional<float> volume) -> std::string {
        return m_ScriptAPI ? m_ScriptAPI->PlayAudioScheduled(path, delaySeconds, bus.value_or("Master"),
                                                             loop.value_or(false), volume.value_or(1.0f)) : std::string();
    };
    lupine["play_audio_scheduled_3d"] = [this](const std::string& path, float x, float y, float z, float delaySeconds,
                                               sol::optional<std::string> bus, sol::optional<bool> loop,
                                               sol::optional<float> volume) -> std::string {
        return m_ScriptAPI ? m_ScriptAPI->PlayAudioScheduled3D(path, math::Vec3(x, y, z), delaySeconds,
                                                               bus.value_or("Master"), loop.value_or(false),
                                                               volume.value_or(1.0f)) : std::string();
    };
    lupine["stop_audio"] = [this](const std::string& uuid) {
        if (m_ScriptAPI) m_ScriptAPI->StopAudio(uuid);
    };
    lupine["pause_audio"] = [this](const std::string& uuid) {
        if (m_ScriptAPI) m_ScriptAPI->PauseAudio(uuid);
    };
    lupine["resume_audio"] = [this](const std::string& uuid) {
        if (m_ScriptAPI) m_ScriptAPI->ResumeAudio(uuid);
    };
    lupine["load_audio_asset"] = [this](const std::string& path, sol::optional<std::string> mode) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->LoadAudioAsset(path, mode.value_or("preload")) : false;
    };
    lupine["is_bus_muted"] = [this](const std::string& bus) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsBusMuted(bus) : false;
    };
    lupine["is_audio_playing"] = [this](const std::string& uuid) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsAudioPlaying(uuid) : false;
    };
    lupine["is_audio_finished"] = [this](const std::string& uuid) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsAudioFinished(uuid) : false;
    };

    // Audio control
    lupine["set_audio_source_volume"] = [this](const std::string& uuid, float volume) {
        if (m_ScriptAPI) m_ScriptAPI->SetAudioSourceVolume(uuid, volume);
    };
    lupine["set_audio_source_pitch"] = [this](const std::string& uuid, float pitch) {
        if (m_ScriptAPI) m_ScriptAPI->SetAudioSourcePitch(uuid, pitch);
    };
    lupine["set_audio_source_pan"] = [this](const std::string& uuid, float pan) {
        if (m_ScriptAPI) m_ScriptAPI->SetAudioSourcePan(uuid, pan);
    };
    lupine["set_master_volume"] = [this](float volume) {
        if (m_ScriptAPI) m_ScriptAPI->SetMasterVolume(volume);
    };
    lupine["get_master_volume"] = [this]() -> float {
        return m_ScriptAPI ? m_ScriptAPI->GetMasterVolume() : 0.0f;
    };
    lupine["set_master_muted"] = [this](bool muted) {
        if (m_ScriptAPI) m_ScriptAPI->SetMasterMuted(muted);
    };
    lupine["is_master_muted"] = [this]() -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsMasterMuted() : false;
    };
    lupine["set_listener_position"] = [this](float x, float y, float z) {
        if (m_ScriptAPI) m_ScriptAPI->SetListenerPosition(math::Vec3(x, y, z));
    };
    lupine["set_listener_orientation"] = [this](float fx, float fy, float fz, float ux, float uy, float uz) {
        if (m_ScriptAPI) m_ScriptAPI->SetListenerOrientation(math::Vec3(fx, fy, fz), math::Vec3(ux, uy, uz));
    };
    lupine["set_listener_velocity"] = [this](float x, float y, float z) {
        if (m_ScriptAPI) m_ScriptAPI->SetListenerVelocity(math::Vec3(x, y, z));
    };
    lupine["create_audio_bus"] = [this](const std::string& name, sol::optional<std::string> parent) {
        if (m_ScriptAPI) m_ScriptAPI->CreateAudioBus(name, parent.value_or(""));
    };
    lupine["destroy_audio_bus"] = [this](const std::string& name) {
        if (m_ScriptAPI) m_ScriptAPI->DestroyAudioBus(name);
    };
    lupine["has_audio_bus"] = [this](const std::string& name) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->HasAudioBus(name) : false;
    };
    lupine["set_bus_solo"] = [this](const std::string& name, bool solo) {
        if (m_ScriptAPI) m_ScriptAPI->SetBusSolo(name, solo);
    };
    lupine["is_bus_solo"] = [this](const std::string& name) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsBusSolo(name) : false;
    };

    // Groups
    lupine["add_to_group"] = [this](const std::string& group) {
        if (m_ScriptAPI) m_ScriptAPI->AddToGroup(group);
    };
    lupine["remove_from_group"] = [this](const std::string& group) {
        if (m_ScriptAPI) m_ScriptAPI->RemoveFromGroup(group);
    };
    lupine["is_in_group"] = [this](const std::string& group) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsInGroup(group) : false;
    };
    lupine["get_groups"] = [this]() -> sol::as_table_t<std::vector<std::string>> {
        return sol::as_table(m_ScriptAPI ? m_ScriptAPI->GetGroups() : std::vector<std::string>{});
    };
    lupine["get_node_count_in_group"] = [this](const std::string& group) -> int {
        return m_ScriptAPI ? m_ScriptAPI->GetNodeCountInGroup(group) : 0;
    };
    lupine["get_nodes_in_group"] = [this](const std::string& group, sol::this_state ts) -> sol::table {
        sol::state_view lua(ts);
        sol::table t = lua.create_table();
        if (m_ScriptAPI) {
            int i = 1;
            for (core::Node* node : m_ScriptAPI->GetNodesInGroup(group)) {
                t[i++] = WrapNode(lua, NodeRef::FromRaw(node, m_ScriptAPI));
            }
        }
        return t;
    };

    // Interfaces (capability contracts)
    lupine["implements_interface"] = [this](const std::string& name) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->ImplementsInterface(name) : false;
    };
    lupine["get_implemented_interfaces"] = [this]() -> sol::as_table_t<std::vector<std::string>> {
        return sol::as_table(m_ScriptAPI ? m_ScriptAPI->GetImplementedInterfaces() : std::vector<std::string>{});
    };
    lupine["verify_interface"] = [this](const std::string& name, sol::this_state ts) -> sol::object {
        sol::state_view lua(ts);
        if (!m_ScriptAPI) return sol::make_object(lua, sol::lua_nil);
        return JsonToLua(lua, m_ScriptAPI->VerifyInterface(name));
    };
    lupine["get_nodes_with_interface"] = [this](const std::string& name, sol::this_state ts) -> sol::table {
        sol::state_view lua(ts);
        sol::table t = lua.create_table();
        if (m_ScriptAPI) {
            int i = 1;
            for (core::Node* node : m_ScriptAPI->GetNodesImplementingInterface(name)) {
                t[i++] = WrapNode(lua, NodeRef::FromRaw(node, m_ScriptAPI));
            }
        }
        return t;
    };
    lupine["get_node_count_with_interface"] = [this](const std::string& name) -> int {
        return m_ScriptAPI ? m_ScriptAPI->GetNodeCountImplementingInterface(name) : 0;
    };
    lupine["get_first_node_with_interface"] = [this](const std::string& name, sol::this_state ts) -> sol::object {
        sol::state_view lua(ts);
        if (!m_ScriptAPI) return sol::make_object(lua, sol::lua_nil);
        return WrapNode(lua, NodeRef::FromRaw(m_ScriptAPI->GetFirstNodeImplementingInterface(name), m_ScriptAPI));
    };
    lupine["interface_exists"] = [this](const std::string& name) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->InterfaceExists(name) : false;
    };
    lupine["get_all_interfaces"] = [this]() -> sol::as_table_t<std::vector<std::string>> {
        return sol::as_table(m_ScriptAPI ? m_ScriptAPI->GetAllInterfaces() : std::vector<std::string>{});
    };
    lupine["get_interface_definition"] = [this](const std::string& name, sol::this_state ts) -> sol::object {
        sol::state_view lua(ts);
        if (!m_ScriptAPI) return sol::make_object(lua, sol::lua_nil);
        nlohmann::json def = m_ScriptAPI->GetInterfaceDefinition(name);
        if (def.is_null()) return sol::make_object(lua, sol::lua_nil);
        return JsonToLua(lua, def);
    };
    lupine["register_interface"] = [this](const sol::object& def) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->RegisterInterface(LuaToJson(def)) : false;
    };
    lupine["archetype_implements_interface"] = [this](const std::string& className, const std::string& name) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->ArchetypeImplementsInterface(className, name) : false;
    };
    lupine["get_archetypes_with_interface"] = [this]
        (const std::string& name) -> sol::as_table_t<std::vector<std::string>> {
        return sol::as_table(m_ScriptAPI ? m_ScriptAPI->GetArchetypesImplementing(name) : std::vector<std::string>{});
    };

    // Tree utilities
    lupine["get_first_node_in_group"] = [this](const std::string& group, sol::this_state ts) -> sol::object {
        sol::state_view lua(ts);
        if (!m_ScriptAPI) return sol::make_object(lua, sol::lua_nil);
        return WrapNode(lua, NodeRef::FromRaw(m_ScriptAPI->GetFirstNodeInGroup(group), m_ScriptAPI));
    };
    lupine["get_node_or_null"] = [this](const std::string& path, sol::this_state ts) -> sol::object {
        sol::state_view lua(ts);
        if (!m_ScriptAPI) return sol::make_object(lua, sol::lua_nil);
        return WrapNode(lua, NodeRef::FromRaw(m_ScriptAPI->GetNodeOrNull(path), m_ScriptAPI));
    };
    lupine["find_children"] = [this](const std::string& type, sol::optional<bool> recursive,
                                     sol::this_state ts) -> sol::table {
        sol::state_view lua(ts);
        sol::table t = lua.create_table();
        if (m_ScriptAPI) {
            int i = 1;
            for (core::Node* node : m_ScriptAPI->FindChildren(type, recursive.value_or(true))) {
                t[i++] = WrapNode(lua, NodeRef::FromRaw(node, m_ScriptAPI));
            }
        }
        return t;
    };
    lupine["is_ancestor_of"] = [this](const NodeRef& other) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsAncestorOf(other.Lock().get()) : false;
    };

    // Scene management
    lupine["change_scene"] = [this](const std::string& path) {
        if (m_ScriptAPI) m_ScriptAPI->ChangeScene(path);
    };
    lupine["reload_scene"] = [this]() {
        if (m_ScriptAPI) m_ScriptAPI->ReloadScene();
    };
    lupine["add_scene"] = [this](const std::string& path) {
        if (m_ScriptAPI) m_ScriptAPI->AddScene(path);
    };
    lupine["remove_scene"] = [this](const std::string& name) {
        if (m_ScriptAPI) m_ScriptAPI->RemoveScene(name);
    };
    // Free a node at the end of the frame. With an explicit node argument the
    // given node is freed; with no argument (or nil) the script's own node is
    // freed. Scripts pervasively call Lupine.queue_free(node) to free a node
    // they created/own, so the argument MUST be honoured — a no-arg-only binding
    // silently frees the owner (self) instead, destroying the wrong node.
    lupine["queue_free"] = [this](sol::optional<NodeRef> node) {
        if (!m_ScriptAPI) return;
        if (!node) {
            // No argument: free the calling script's own node.
            m_ScriptAPI->QueueFreeSelf();
            return;
        }
        // An argument was passed: free THAT node. Lock() returns null for a
        // destroyed or corrupted handle, in which case we no-op — we must never
        // fall back to freeing self here, or a stale tile/handle would delete the
        // combat controller node mid-frame.
        std::shared_ptr<core::Node> target = node->Lock();
        if (target) {
            m_ScriptAPI->QueueFree(target.get());
        }
    };

    // Math helpers
    lupine["lerp"] = [this](float a, float b, float t) -> float {
        return m_ScriptAPI ? m_ScriptAPI->Lerp(a, b, t) : a;
    };
    lupine["clamp"] = [this](float value, float min, float max) -> float {
        return m_ScriptAPI ? m_ScriptAPI->Clamp(value, min, max) : value;
    };
    lupine["move_toward"] = [this](float from, float to, float delta) -> float {
        return m_ScriptAPI ? m_ScriptAPI->MoveToward(from, to, delta) : from;
    };
    lupine["random_range"] = [this](float min, float max) -> float {
        return m_ScriptAPI ? m_ScriptAPI->RandomRange(min, max) : min;
    };

    // Time
    lupine["get_time"] = [this]() -> float {
        return m_ScriptAPI ? m_ScriptAPI->GetTime() : 0.0f;
    };
    lupine["set_time_scale"] = [this](float timeScale) {
        if (m_ScriptAPI) m_ScriptAPI->SetTimeScale(timeScale);
    };
    lupine["get_time_scale"] = [this]() -> float {
        return m_ScriptAPI ? m_ScriptAPI->GetTimeScale() : 1.0f;
    };

    // Profiler: custom timing zones + counters from gameplay code. No-ops when the
    // profiler is disabled. Category names match the engine zone categories.
    lupine["profiler_begin_zone"] = [](const std::string& name, sol::optional<std::string> category) {
        ::lupine::profiling::ZoneCategory cat = ::lupine::profiling::ZoneCategory::User;
        if (category) {
            const std::string& c = *category;
            if (c == "Update") cat = ::lupine::profiling::ZoneCategory::Update;
            else if (c == "Physics") cat = ::lupine::profiling::ZoneCategory::Physics;
            else if (c == "Render") cat = ::lupine::profiling::ZoneCategory::Render;
            else if (c == "Scripting") cat = ::lupine::profiling::ZoneCategory::Scripting;
            else if (c == "Audio") cat = ::lupine::profiling::ZoneCategory::Audio;
        }
        ::lupine::profiling::Profiler::Get().BeginZone(name, cat);
    };
    lupine["profiler_end_zone"] = []() {
        ::lupine::profiling::Profiler::Get().EndZone();
    };
    lupine["profiler_set_counter"] = [](const std::string& name, double value) {
        ::lupine::profiling::Profiler::Get().SetCounter(name, value);
    };
    lupine["profiler_is_enabled"] = []() -> bool {
        return ::lupine::profiling::Profiler::Get().IsEnabled();
    };
    lupine["profiler_set_enabled"] = [](bool enabled) {
        ::lupine::profiling::Profiler::Get().SetEnabled(enabled);
    };
    lupine["profiler_frame_ms"] = []() -> double {
        return ::lupine::profiling::Profiler::Get().GetAverageFrameMs();
    };

    // Engine / OS info
    lupine["get_fps"] = [this]() -> float {
        return m_ScriptAPI ? m_ScriptAPI->GetFPS() : 0.0f;
    };
    lupine["get_ticks_msec"] = [this]() -> int {
        return m_ScriptAPI ? m_ScriptAPI->GetTicksMsec() : 0;
    };
    lupine["get_unix_time"] = [this]() -> double {
        return m_ScriptAPI ? m_ScriptAPI->GetUnixTime() : 0.0;
    };
    lupine["get_platform_name"] = [this]() -> std::string {
        return m_ScriptAPI ? m_ScriptAPI->GetPlatformName() : "";
    };
    lupine["is_debug_build"] = [this]() -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsDebugBuild() : false;
    };
    lupine["get_dpi_scale"] = [this]() -> float {
        return m_ScriptAPI ? m_ScriptAPI->GetDPIScale() : 1.0f;
    };
    lupine["open_url"] = [this](const std::string& url) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->OpenURL(url) : false;
    };

    // Distance calculations
    lupine["distance_to_2d"] = [this](float x, float y) -> float {
        return m_ScriptAPI ? m_ScriptAPI->DistanceTo2D(x, y) : 0.0f;
    };
    lupine["distance_to_3d"] = [this](float x, float y, float z) -> float {
        return m_ScriptAPI ? m_ScriptAPI->DistanceTo3D(x, y, z) : 0.0f;
    };

    // Look at
    lupine["look_at_2d"] = [this](float x, float y) {
        if (m_ScriptAPI) m_ScriptAPI->LookAt2D(x, y);
    };
    lupine["look_at_3d"] = [this](float x, float y, float z) {
        if (m_ScriptAPI) m_ScriptAPI->LookAt3D(x, y, z);
    };

    // Move toward target
    lupine["move_toward_2d"] = [this](float targetX, float targetY, float maxDelta) {
        if (m_ScriptAPI) m_ScriptAPI->MoveToward2D(targetX, targetY, maxDelta);
    };
    lupine["move_toward_3d"] = [this](float targetX, float targetY, float targetZ, float maxDelta) {
        if (m_ScriptAPI) m_ScriptAPI->MoveToward3D(targetX, targetY, targetZ, maxDelta);
    };

    // Debug draw
    lupine["debug_draw_line"] = [this](float x1, float y1, float z1, float x2, float y2, float z2,
                                       float r, float g, float b, float a, sol::optional<float> duration) {
        if (m_ScriptAPI) m_ScriptAPI->DebugDrawLine(math::Vec3(x1, y1, z1), math::Vec3(x2, y2, z2),
                                                    math::Color(r, g, b, a), duration.value_or(0.0f));
    };
    lupine["debug_draw_line_2d"] = [this](float x1, float y1, float x2, float y2,
                                          float r, float g, float b, float a, sol::optional<float> duration) {
        if (m_ScriptAPI) m_ScriptAPI->DebugDrawLine2D(math::Vec2(x1, y1), math::Vec2(x2, y2),
                                                      math::Color(r, g, b, a), duration.value_or(0.0f));
    };
    lupine["debug_draw_ray"] = [this](float ox, float oy, float oz, float dx, float dy, float dz,
                                      float r, float g, float b, float a, sol::optional<float> duration) {
        if (m_ScriptAPI) m_ScriptAPI->DebugDrawRay(math::Vec3(ox, oy, oz), math::Vec3(dx, dy, dz),
                                                   math::Color(r, g, b, a), duration.value_or(0.0f));
    };
    lupine["debug_draw_box"] = [this](float cx, float cy, float cz, float sx, float sy, float sz,
                                      float r, float g, float b, float a, sol::optional<float> duration) {
        if (m_ScriptAPI) m_ScriptAPI->DebugDrawBox(math::Vec3(cx, cy, cz), math::Vec3(sx, sy, sz),
                                                   math::Color(r, g, b, a), duration.value_or(0.0f));
    };
    lupine["debug_draw_sphere"] = [this](float cx, float cy, float cz, float radius,
                                         float r, float g, float b, float a, sol::optional<float> duration) {
        if (m_ScriptAPI) m_ScriptAPI->DebugDrawSphere(math::Vec3(cx, cy, cz), radius,
                                                      math::Color(r, g, b, a), duration.value_or(0.0f));
    };
    lupine["debug_draw_circle"] = [this](float cx, float cy, float cz, float nx, float ny, float nz, float radius,
                                         float r, float g, float b, float a, sol::optional<float> duration) {
        if (m_ScriptAPI) m_ScriptAPI->DebugDrawCircle(math::Vec3(cx, cy, cz), math::Vec3(nx, ny, nz), radius,
                                                      math::Color(r, g, b, a), duration.value_or(0.0f));
    };
    lupine["debug_draw_text"] = [this](float x, float y, float z, const std::string& text,
                                       float r, float g, float b, float a, sol::optional<float> duration) {
        if (m_ScriptAPI) m_ScriptAPI->DebugDrawText(math::Vec3(x, y, z), text,
                                                    math::Color(r, g, b, a), duration.value_or(0.0f));
    };
    lupine["debug_draw_text_2d"] = [this](float x, float y, const std::string& text,
                                          float r, float g, float b, float a, sol::optional<float> duration) {
        if (m_ScriptAPI) m_ScriptAPI->DebugDrawText2D(math::Vec2(x, y), text,
                                                      math::Color(r, g, b, a), duration.value_or(0.0f));
    };

    // Custom component rendering (valid only inside on_draw). These record real,
    // runtime-visible geometry into the active render context.
    lupine["is_drawing"] = [this]() -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsDrawing() : false;
    };
    lupine["draw_quad"] = [this](float x, float y, float z, float w, float h,
                                 float r, float g, float b, float a, sol::optional<int> blend) {
        if (m_ScriptAPI) m_ScriptAPI->DrawQuad(math::Vec3(x, y, z), math::Vec2(w, h),
                                               math::Color(r, g, b, a), blend.value_or(0));
    };
    lupine["draw_textured_quad"] = [this](float x, float y, float z, float w, float h,
                                          float r, float g, float b, float a,
                                          const std::string& path, sol::optional<int> blend) {
        if (m_ScriptAPI) m_ScriptAPI->DrawTexturedQuad(math::Vec3(x, y, z), math::Vec2(w, h),
                                                       math::Color(r, g, b, a), path, blend.value_or(0));
    };
    lupine["draw_rect"] = [this](float x, float y, float w, float h,
                                 float r, float g, float b, float a,
                                 sol::optional<bool> filled, sol::optional<float> thickness) {
        if (m_ScriptAPI) m_ScriptAPI->DrawRect(math::Vec2(x, y), math::Vec2(w, h),
                                               math::Color(r, g, b, a),
                                               filled.value_or(true), thickness.value_or(1.0f));
    };
    lupine["draw_sprite"] = [this](const std::string& path, float x, float y, float w, float h,
                                   float r, float g, float b, float a,
                                   sol::optional<float> rotation, sol::optional<int> blend) {
        if (m_ScriptAPI) m_ScriptAPI->DrawSprite(path, math::Vec2(x, y), math::Vec2(w, h),
                                                 math::Color(r, g, b, a),
                                                 rotation.value_or(0.0f), blend.value_or(0));
    };
    lupine["draw_line"] = [this](float x1, float y1, float z1, float x2, float y2, float z2,
                                 float r, float g, float b, float a, sol::optional<float> thickness) {
        if (m_ScriptAPI) m_ScriptAPI->DrawLine(math::Vec3(x1, y1, z1), math::Vec3(x2, y2, z2),
                                               math::Color(r, g, b, a), thickness.value_or(1.0f));
    };
    lupine["draw_circle"] = [this](float x, float y, float z, float radius,
                                   float r, float g, float b, float a, sol::optional<bool> filled) {
        if (m_ScriptAPI) m_ScriptAPI->DrawCircle(math::Vec3(x, y, z), radius,
                                                 math::Color(r, g, b, a), filled.value_or(true));
    };
    lupine["draw_polygon"] = [this](float x, float y, float radius, int sides,
                                    float r, float g, float b, float a,
                                    sol::optional<float> rotation, sol::optional<int> blend) {
        if (m_ScriptAPI) m_ScriptAPI->DrawPolygon(math::Vec2(x, y), radius, sides,
                                                  math::Color(r, g, b, a),
                                                  rotation.value_or(0.0f), blend.value_or(0));
    };
    lupine["draw_box"] = [this](float cx, float cy, float cz, float w, float h, float d,
                                float r, float g, float b, float a, sol::optional<bool> wireframe) {
        if (m_ScriptAPI) m_ScriptAPI->DrawBox(math::Vec3(cx, cy, cz), math::Vec3(w, h, d),
                                              math::Color(r, g, b, a), wireframe.value_or(false));
    };
    lupine["draw_rounded_rect"] = [this](float x, float y, float w, float h, float cornerRadius,
                                         float r, float g, float b, float a, sol::optional<int> blend) {
        if (m_ScriptAPI) m_ScriptAPI->DrawRoundedRect(math::Vec2(x, y), math::Vec2(w, h), cornerRadius,
                                                      math::Color(r, g, b, a), blend.value_or(0));
    };

    // Editor-only debug drawing (no runtime visual; call from on_render). These
    // are a no-op in shipped runtime builds.
    lupine["is_editor_draw_available"] = [this]() -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsEditorDrawAvailable() : false;
    };
    lupine["editor_draw_line"] = [this](float x1, float y1, float z1, float x2, float y2, float z2,
                                        float r, float g, float b, float a) {
        if (m_ScriptAPI) m_ScriptAPI->EditorDrawLine(math::Vec3(x1, y1, z1), math::Vec3(x2, y2, z2),
                                                     math::Color(r, g, b, a));
    };
    lupine["editor_draw_box"] = [this](float cx, float cy, float cz, float w, float h, float d,
                                       float r, float g, float b, float a, sol::optional<bool> wireframe) {
        if (m_ScriptAPI) m_ScriptAPI->EditorDrawBox(math::Vec3(cx, cy, cz), math::Vec3(w, h, d),
                                                    math::Color(r, g, b, a), wireframe.value_or(true));
    };
    lupine["editor_draw_sphere"] = [this](float cx, float cy, float cz, float radius,
                                          float r, float g, float b, float a, sol::optional<bool> wireframe) {
        if (m_ScriptAPI) m_ScriptAPI->EditorDrawSphere(math::Vec3(cx, cy, cz), radius,
                                                       math::Color(r, g, b, a), wireframe.value_or(true));
    };
    lupine["editor_draw_circle"] = [this](float cx, float cy, float cz, float nx, float ny, float nz,
                                          float radius, float r, float g, float b, float a) {
        if (m_ScriptAPI) m_ScriptAPI->EditorDrawCircle(math::Vec3(cx, cy, cz), math::Vec3(nx, ny, nz),
                                                       radius, math::Color(r, g, b, a));
    };
    lupine["editor_draw_rect_2d"] = [this](float cx, float cy, float w, float h,
                                           float r, float g, float b, float a) {
        if (m_ScriptAPI) m_ScriptAPI->EditorDrawRect2D(math::Vec2(cx, cy), math::Vec2(w, h),
                                                       math::Color(r, g, b, a));
    };
    lupine["editor_draw_text"] = [this](float x, float y, float z, const std::string& text,
                                        float r, float g, float b, float a) {
        if (m_ScriptAPI) m_ScriptAPI->EditorDrawText(math::Vec3(x, y, z), text, math::Color(r, g, b, a));
    };

    // Game state
    lupine["is_game_paused"] = [this]() -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsGamePaused() : false;
    };
    lupine["set_game_paused"] = [this](bool paused) {
        if (m_ScriptAPI) m_ScriptAPI->SetGamePaused(paused);
    };
    // Ask the host application to quit (stops the play session in the editor).
    lupine["quit"] = [this]() {
        if (m_ScriptAPI) m_ScriptAPI->RequestQuit();
    };
    // Extra command-line / runtime arguments the game was launched with, as a
    // 1-indexed array of strings (empty when none were supplied).
    lupine["get_cmdline_args"] = [this]() -> sol::as_table_t<std::vector<std::string>> {
        if (m_ScriptAPI) return sol::as_table(m_ScriptAPI->GetCommandLineArgs());
        return sol::as_table(std::vector<std::string>());
    };

    // Additional math helpers
    lupine["abs"] = [this](float value) -> float {
        return m_ScriptAPI ? m_ScriptAPI->Abs(value) : value;
    };
    lupine["sign"] = [this](float value) -> float {
        return m_ScriptAPI ? m_ScriptAPI->Sign(value) : 0.0f;
    };
    lupine["angle_difference"] = [this](float from, float to) -> float {
        return m_ScriptAPI ? m_ScriptAPI->AngleDifference(from, to) : 0.0f;
    };
    lupine["lerp_angle"] = [this](float from, float to, float weight) -> float {
        return m_ScriptAPI ? m_ScriptAPI->LerpAngle(from, to, weight) : from;
    };
    lupine["inverse_lerp"] = [this](float from, float to, float value) -> float {
        return m_ScriptAPI ? m_ScriptAPI->InverseLerp(from, to, value) : 0.0f;
    };
    lupine["smoothstep"] = [this](float from, float to, float t) -> float {
        return m_ScriptAPI ? m_ScriptAPI->Smoothstep(from, to, t) : 0.0f;
    };
    lupine["remap"] = [this](float value, float fromMin, float fromMax, float toMin, float toMax) -> float {
        return m_ScriptAPI ? m_ScriptAPI->Remap(value, fromMin, fromMax, toMin, toMax) : value;
    };

    // Random / math parity helpers
    lupine["random_float"] = [this]() -> float {
        return m_ScriptAPI ? m_ScriptAPI->RandomFloat() : 0.0f;
    };
    lupine["random_bool"] = [this]() -> bool {
        return m_ScriptAPI ? m_ScriptAPI->RandomBool() : false;
    };
    lupine["random_sign"] = [this]() -> int {
        return m_ScriptAPI ? m_ScriptAPI->RandomSign() : 1;
    };
    lupine["random_seed"] = [this](int seed) {
        if (m_ScriptAPI) m_ScriptAPI->RandomSeed(seed);
    };
    lupine["deg_to_rad"] = [this](float degrees) -> float {
        return m_ScriptAPI ? m_ScriptAPI->DegToRad(degrees) : degrees;
    };
    lupine["rad_to_deg"] = [this](float radians) -> float {
        return m_ScriptAPI ? m_ScriptAPI->RadToDeg(radians) : radians;
    };
    lupine["wrap"] = [this](float value, float min, float max) -> float {
        return m_ScriptAPI ? m_ScriptAPI->Wrap(value, min, max) : value;
    };
    lupine["wrap_int"] = [this](int value, int min, int max) -> int {
        return m_ScriptAPI ? m_ScriptAPI->WrapInt(value, min, max) : value;
    };
    lupine["ping_pong"] = [this](float value, float length) -> float {
        return m_ScriptAPI ? m_ScriptAPI->PingPong(value, length) : 0.0f;
    };
    lupine["snapped"] = [this](float value, float step) -> float {
        return m_ScriptAPI ? m_ScriptAPI->Snapped(value, step) : value;
    };
    lupine["is_equal_approx"] = [this](float a, float b) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsEqualApprox(a, b) : false;
    };
    lupine["ease"] = [this](float t, float curve) -> float {
        return m_ScriptAPI ? m_ScriptAPI->Ease(t, curve) : t;
    };
    lupine["pos_mod"] = [this](float a, float b) -> float {
        return m_ScriptAPI ? m_ScriptAPI->PosMod(a, b) : 0.0f;
    };
    lupine["pos_mod_int"] = [this](int a, int b) -> int {
        return m_ScriptAPI ? m_ScriptAPI->PosModInt(a, b) : 0;
    };

    // Vector math helpers (component args; array-style table returns)
    lupine["dot_2d"] = [this](float ax, float ay, float bx, float by) -> float {
        return m_ScriptAPI ? m_ScriptAPI->Dot2D(math::Vec2(ax, ay), math::Vec2(bx, by)) : 0.0f;
    };
    lupine["dot_3d"] = [this](float ax, float ay, float az, float bx, float by, float bz) -> float {
        return m_ScriptAPI ? m_ScriptAPI->Dot3D(math::Vec3(ax, ay, az), math::Vec3(bx, by, bz)) : 0.0f;
    };
    lupine["length_2d"] = [this](float x, float y) -> float {
        return m_ScriptAPI ? m_ScriptAPI->Length2D(math::Vec2(x, y)) : 0.0f;
    };
    lupine["length_3d"] = [this](float x, float y, float z) -> float {
        return m_ScriptAPI ? m_ScriptAPI->Length3D(math::Vec3(x, y, z)) : 0.0f;
    };
    lupine["normalize_2d"] = [this](float x, float y) -> sol::as_table_t<std::vector<float>> {
        if (m_ScriptAPI) {
            math::Vec2 n = m_ScriptAPI->Normalize2D(math::Vec2(x, y));
            return sol::as_table(std::vector<float>{n.x, n.y});
        }
        return sol::as_table(std::vector<float>{0.0f, 0.0f});
    };
    lupine["normalize_3d"] = [this](float x, float y, float z) -> sol::as_table_t<std::vector<float>> {
        if (m_ScriptAPI) {
            math::Vec3 n = m_ScriptAPI->Normalize3D(math::Vec3(x, y, z));
            return sol::as_table(std::vector<float>{n.x, n.y, n.z});
        }
        return sol::as_table(std::vector<float>{0.0f, 0.0f, 0.0f});
    };
    lupine["cross"] = [this](float ax, float ay, float az, float bx, float by, float bz) -> sol::as_table_t<std::vector<float>> {
        if (m_ScriptAPI) {
            math::Vec3 c = m_ScriptAPI->Cross(math::Vec3(ax, ay, az), math::Vec3(bx, by, bz));
            return sol::as_table(std::vector<float>{c.x, c.y, c.z});
        }
        return sol::as_table(std::vector<float>{0.0f, 0.0f, 0.0f});
    };

    // Direction vectors / transform helpers (operate on the owner node)
    lupine["get_forward"] = [this]() -> sol::as_table_t<std::vector<float>> {
        if (m_ScriptAPI) {
            math::Vec3 v = m_ScriptAPI->GetForward();
            return sol::as_table(std::vector<float>{v.x, v.y, v.z});
        }
        return sol::as_table(std::vector<float>{0.0f, 0.0f, -1.0f});
    };
    lupine["get_right"] = [this]() -> sol::as_table_t<std::vector<float>> {
        if (m_ScriptAPI) {
            math::Vec3 v = m_ScriptAPI->GetRight();
            return sol::as_table(std::vector<float>{v.x, v.y, v.z});
        }
        return sol::as_table(std::vector<float>{1.0f, 0.0f, 0.0f});
    };
    lupine["get_up"] = [this]() -> sol::as_table_t<std::vector<float>> {
        if (m_ScriptAPI) {
            math::Vec3 v = m_ScriptAPI->GetUp();
            return sol::as_table(std::vector<float>{v.x, v.y, v.z});
        }
        return sol::as_table(std::vector<float>{0.0f, 1.0f, 0.0f});
    };
    lupine["get_global_position_2d"] = [this]() -> sol::as_table_t<std::vector<float>> {
        if (m_ScriptAPI) {
            math::Vec2 v = m_ScriptAPI->GetGlobalPosition2D();
            return sol::as_table(std::vector<float>{v.x, v.y});
        }
        return sol::as_table(std::vector<float>{0.0f, 0.0f});
    };
    lupine["set_global_position_2d"] = [this](float x, float y) {
        if (m_ScriptAPI) m_ScriptAPI->SetGlobalPosition2D(x, y);
    };
    lupine["get_global_position_3d"] = [this]() -> sol::as_table_t<std::vector<float>> {
        if (m_ScriptAPI) {
            math::Vec3 v = m_ScriptAPI->GetGlobalPosition3D();
            return sol::as_table(std::vector<float>{v.x, v.y, v.z});
        }
        return sol::as_table(std::vector<float>{0.0f, 0.0f, 0.0f});
    };
    lupine["get_global_rotation_2d"] = [this]() -> float {
        return m_ScriptAPI ? m_ScriptAPI->GetGlobalRotation2D() : 0.0f;
    };
    lupine["set_global_position_3d"] = [this](float x, float y, float z) {
        if (m_ScriptAPI) m_ScriptAPI->SetGlobalPosition3D(x, y, z);
    };
    lupine["set_global_rotation_2d"] = [this](float degrees) {
        if (m_ScriptAPI) m_ScriptAPI->SetGlobalRotation2D(degrees);
    };
    lupine["get_global_rotation_3d"] = [this]() -> sol::as_table_t<std::vector<float>> {
        if (m_ScriptAPI) {
            math::Vec3 v = m_ScriptAPI->GetGlobalRotation3D();
            return sol::as_table(std::vector<float>{v.x, v.y, v.z});
        }
        return sol::as_table(std::vector<float>{0.0f, 0.0f, 0.0f});
    };
    lupine["set_global_rotation_3d"] = [this](float pitch, float yaw, float roll) {
        if (m_ScriptAPI) m_ScriptAPI->SetGlobalRotation3D(pitch, yaw, roll);
    };
    lupine["get_global_scale_2d"] = [this]() -> sol::as_table_t<std::vector<float>> {
        if (m_ScriptAPI) {
            math::Vec2 v = m_ScriptAPI->GetGlobalScale2D();
            return sol::as_table(std::vector<float>{v.x, v.y});
        }
        return sol::as_table(std::vector<float>{1.0f, 1.0f});
    };
    lupine["get_global_scale_3d"] = [this]() -> sol::as_table_t<std::vector<float>> {
        if (m_ScriptAPI) {
            math::Vec3 v = m_ScriptAPI->GetGlobalScale3D();
            return sol::as_table(std::vector<float>{v.x, v.y, v.z});
        }
        return sol::as_table(std::vector<float>{1.0f, 1.0f, 1.0f});
    };
    lupine["get_frame_count"] = [this]() -> int {
        return m_ScriptAPI ? m_ScriptAPI->GetFrameCount() : 0;
    };

    // Lifecycle (owner node)
    lupine["free_self"] = [this]() {
        if (m_ScriptAPI) m_ScriptAPI->FreeSelf();
    };
    lupine["queue_free_deferred_self"] = [this]() {
        if (m_ScriptAPI) m_ScriptAPI->QueueFreeDeferredSelf();
    };

    // Asset loading
    lupine["load_image_asset"] = [this](const std::string& path) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->LoadImageAsset(path) : false;
    };
    lupine["load_model_asset"] = [this](const std::string& path) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->LoadModelAsset(path) : false;
    };
    lupine["preload_assets"] = [this](sol::table paths) {
        if (!m_ScriptAPI) return;
        std::vector<std::string> assetPaths;
        for (std::size_t i = 1; i <= paths.size(); ++i) {
            sol::optional<std::string> p = paths[i];
            if (p) assetPaths.push_back(*p);
        }
        m_ScriptAPI->PreloadAssets(assetPaths);
    };

    // Warm a list of image textures on a background thread, decoding them into
    // the shared ImageCache so a later scene swap that uses them is a cache hit
    // rather than a main-thread decode. Fire-and-forget; returns the count
    // queued (already-cached/in-flight paths are skipped).
    lupine["preload_images_async"] = [](sol::table paths) -> double {
        std::vector<std::string> imagePaths;
        for (std::size_t i = 1; i <= paths.size(); ++i) {
            sol::optional<std::string> p = paths[i];
            if (p && !p->empty()) {
                imagePaths.push_back(*p);
            }
        }
        asset::AsyncImageLoader::GetInstance().PreloadManyAsync(imagePaths);
        return static_cast<double>(imagePaths.size());
    };

    // Scan a .scene file for the image textures it references and warm them all
    // (see preload_images_async). Lets a caller preload a destination screen's
    // backgrounds/icons before transitioning to it. Returns the count queued.
    lupine["preload_scene_images_async"] = [](const std::string& scenePath) -> double {
        std::vector<std::string> textures = asset::AsyncImageLoader::ScanSceneTextures(scenePath);
        asset::AsyncImageLoader::GetInstance().PreloadManyAsync(textures);
        return static_cast<double>(textures.size());
    };

    // ========================================================================
    // Physics 2D - Queries
    // ========================================================================

    lupine["raycast_2d"] = [this](float fromX, float fromY, float dirX, float dirY, float maxDistance) -> sol::table {
        sol::table result = m_LuaState.create_table();
        if (m_ScriptAPI) {
            auto hit = m_ScriptAPI->Raycast2D(math::Vec2(fromX, fromY), math::Vec2(dirX, dirY), maxDistance);
            result["hit"] = hit.hit;
            result["point_x"] = hit.point.x;
            result["point_y"] = hit.point.y;
            result["normal_x"] = hit.normal.x;
            result["normal_y"] = hit.normal.y;
            result["distance"] = hit.distance;
            result["fraction"] = hit.fraction;
            result["body_id"] = hit.bodyId;
        } else {
            result["hit"] = false;
        }
        return result;
    };

    lupine["circle_cast_2d"] = [this](float fromX, float fromY, float toX, float toY, float radius) -> sol::table {
        sol::table result = m_LuaState.create_table();
        if (m_ScriptAPI) {
            auto hit = m_ScriptAPI->CircleCast2D(math::Vec2(fromX, fromY), math::Vec2(toX, toY), radius);
            result["hit"] = hit.hit;
            result["point_x"] = hit.point.x;
            result["point_y"] = hit.point.y;
            result["normal_x"] = hit.normal.x;
            result["normal_y"] = hit.normal.y;
            result["fraction"] = hit.fraction;
            result["body_id"] = hit.bodyId;
        } else {
            result["hit"] = false;
        }
        return result;
    };

    lupine["raycast_all_2d"] = [this](float fromX, float fromY, float dirX, float dirY, float maxDistance,
                                      sol::optional<uint32_t> collisionMask, sol::this_state ts) -> sol::table {
        sol::state_view lua(ts);
        sol::table results = lua.create_table();
        if (m_ScriptAPI) {
            int i = 1;
            for (const auto& hit : m_ScriptAPI->RaycastAll2D(math::Vec2(fromX, fromY), math::Vec2(dirX, dirY),
                                                             maxDistance, collisionMask.value_or(0xFFFFFFFF))) {
                sol::table entry = lua.create_table();
                entry["hit"] = hit.hit;
                entry["point_x"] = hit.point.x;
                entry["point_y"] = hit.point.y;
                entry["normal_x"] = hit.normal.x;
                entry["normal_y"] = hit.normal.y;
                entry["distance"] = hit.distance;
                entry["fraction"] = hit.fraction;
                entry["body_id"] = hit.bodyId;
                entry["collider"] = WrapNode(lua, NodeRef::FromRaw(hit.collider, m_ScriptAPI));
                results[i++] = entry;
            }
        }
        return results;
    };

    lupine["overlap_circle"] = [this](float centerX, float centerY, float radius,
                                      sol::optional<uint32_t> collisionMask, sol::this_state ts) -> sol::table {
        sol::state_view lua(ts);
        sol::table t = lua.create_table();
        if (m_ScriptAPI) {
            int i = 1;
            for (core::Node* node : m_ScriptAPI->OverlapCircle(math::Vec2(centerX, centerY), radius,
                                                               collisionMask.value_or(0xFFFFFFFF))) {
                t[i++] = WrapNode(lua, NodeRef::FromRaw(node, m_ScriptAPI));
            }
        }
        return t;
    };

    lupine["overlap_rect"] = [this](float centerX, float centerY, float halfWidth, float halfHeight,
                                    sol::optional<uint32_t> collisionMask, sol::this_state ts) -> sol::table {
        sol::state_view lua(ts);
        sol::table t = lua.create_table();
        if (m_ScriptAPI) {
            int i = 1;
            for (core::Node* node : m_ScriptAPI->OverlapRect(math::Vec2(centerX, centerY),
                                                             math::Vec2(halfWidth, halfHeight),
                                                             collisionMask.value_or(0xFFFFFFFF))) {
                t[i++] = WrapNode(lua, NodeRef::FromRaw(node, m_ScriptAPI));
            }
        }
        return t;
    };

    // ========================================================================
    // Physics 2D - Body Manipulation
    // ========================================================================

    lupine["get_linear_velocity_2d"] = [this]() -> sol::as_table_t<std::vector<float>> {
        if (m_ScriptAPI) {
            auto vel = m_ScriptAPI->GetLinearVelocity2D();
            return sol::as_table(std::vector<float>{vel.x, vel.y});
        }
        return sol::as_table(std::vector<float>{0.0f, 0.0f});
    };
    lupine["set_linear_velocity_2d"] = [this](float x, float y) {
        if (m_ScriptAPI) m_ScriptAPI->SetLinearVelocity2D(x, y);
    };
    lupine["get_angular_velocity_2d"] = [this]() -> float {
        return m_ScriptAPI ? m_ScriptAPI->GetAngularVelocity2D() : 0.0f;
    };
    lupine["set_angular_velocity_2d"] = [this](float omega) {
        if (m_ScriptAPI) m_ScriptAPI->SetAngularVelocity2D(omega);
    };

    lupine["apply_force_2d"] = [this](float forceX, float forceY) {
        if (m_ScriptAPI) m_ScriptAPI->ApplyForce2D(forceX, forceY);
    };
    lupine["apply_force_at_point_2d"] = [this](float forceX, float forceY, float pointX, float pointY) {
        if (m_ScriptAPI) m_ScriptAPI->ApplyForceAtPoint2D(forceX, forceY, pointX, pointY);
    };
    lupine["apply_torque_2d"] = [this](float torque) {
        if (m_ScriptAPI) m_ScriptAPI->ApplyTorque2D(torque);
    };

    lupine["apply_impulse_2d"] = [this](float impulseX, float impulseY) {
        if (m_ScriptAPI) m_ScriptAPI->ApplyImpulse2D(impulseX, impulseY);
    };
    lupine["apply_impulse_at_point_2d"] = [this](float impulseX, float impulseY, float pointX, float pointY) {
        if (m_ScriptAPI) m_ScriptAPI->ApplyImpulseAtPoint2D(impulseX, impulseY, pointX, pointY);
    };
    lupine["apply_angular_impulse_2d"] = [this](float impulse) {
        if (m_ScriptAPI) m_ScriptAPI->ApplyAngularImpulse2D(impulse);
    };

    lupine["get_mass_2d"] = [this]() -> float {
        return m_ScriptAPI ? m_ScriptAPI->GetMass2D() : 0.0f;
    };
    lupine["get_gravity_scale_2d"] = [this]() -> float {
        return m_ScriptAPI ? m_ScriptAPI->GetGravityScale2D() : 1.0f;
    };
    lupine["set_gravity_scale_2d"] = [this](float scale) {
        if (m_ScriptAPI) m_ScriptAPI->SetGravityScale2D(scale);
    };
    lupine["get_linear_damping_2d"] = [this]() -> float {
        return m_ScriptAPI ? m_ScriptAPI->GetLinearDamping2D() : 0.0f;
    };
    lupine["set_linear_damping_2d"] = [this](float damping) {
        if (m_ScriptAPI) m_ScriptAPI->SetLinearDamping2D(damping);
    };
    lupine["get_angular_damping_2d"] = [this]() -> float {
        return m_ScriptAPI ? m_ScriptAPI->GetAngularDamping2D() : 0.0f;
    };
    lupine["set_angular_damping_2d"] = [this](float damping) {
        if (m_ScriptAPI) m_ScriptAPI->SetAngularDamping2D(damping);
    };
    lupine["is_fixed_rotation_2d"] = [this]() -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsFixedRotation2D() : false;
    };
    lupine["set_fixed_rotation_2d"] = [this](bool fixed) {
        if (m_ScriptAPI) m_ScriptAPI->SetFixedRotation2D(fixed);
    };
    lupine["is_bullet_2d"] = [this]() -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsBullet2D() : false;
    };
    lupine["set_bullet_2d"] = [this](bool bullet) {
        if (m_ScriptAPI) m_ScriptAPI->SetBullet2D(bullet);
    };

    // ========================================================================
    // Physics 3D - Queries
    // ========================================================================

    lupine["raycast_3d"] = [this](float fromX, float fromY, float fromZ, float dirX, float dirY, float dirZ, float maxDistance) -> sol::table {
        sol::table result = m_LuaState.create_table();
        if (m_ScriptAPI) {
            auto hit = m_ScriptAPI->Raycast3D(math::Vec3(fromX, fromY, fromZ), math::Vec3(dirX, dirY, dirZ), maxDistance);
            result["hit"] = hit.hit;
            result["point_x"] = hit.point.x;
            result["point_y"] = hit.point.y;
            result["point_z"] = hit.point.z;
            result["normal_x"] = hit.normal.x;
            result["normal_y"] = hit.normal.y;
            result["normal_z"] = hit.normal.z;
            result["distance"] = hit.distance;
            result["fraction"] = hit.fraction;
            result["body_id"] = hit.bodyId;
        } else {
            result["hit"] = false;
        }
        return result;
    };

    // Screen <-> world conversion
    lupine["screen_to_world_2d"] = [this](float x, float y) -> sol::as_table_t<std::vector<float>> {
        if (m_ScriptAPI) {
            math::Vec2 p = m_ScriptAPI->ScreenToWorld2D(math::Vec2(x, y));
            return sol::as_table(std::vector<float>{p.x, p.y});
        }
        return sol::as_table(std::vector<float>{0.0f, 0.0f});
    };
    lupine["world_to_screen_2d"] = [this](float x, float y) -> sol::as_table_t<std::vector<float>> {
        if (m_ScriptAPI) {
            math::Vec2 p = m_ScriptAPI->WorldToScreen2D(math::Vec2(x, y));
            return sol::as_table(std::vector<float>{p.x, p.y});
        }
        return sol::as_table(std::vector<float>{0.0f, 0.0f});
    };
    lupine["screen_to_world_3d"] = [this](float x, float y, float distance) -> sol::as_table_t<std::vector<float>> {
        if (m_ScriptAPI) {
            math::Vec3 p = m_ScriptAPI->ScreenToWorld3D(math::Vec2(x, y), distance);
            return sol::as_table(std::vector<float>{p.x, p.y, p.z});
        }
        return sol::as_table(std::vector<float>{0.0f, 0.0f, 0.0f});
    };
    lupine["world_to_screen_3d"] = [this](float x, float y, float z) -> sol::as_table_t<std::vector<float>> {
        if (m_ScriptAPI) {
            math::Vec3 p = m_ScriptAPI->WorldToScreen3D(math::Vec3(x, y, z));
            return sol::as_table(std::vector<float>{p.x, p.y, p.z});
        }
        return sol::as_table(std::vector<float>{0.0f, 0.0f, 0.0f});
    };
    lupine["screen_to_world_ray_3d"] = [this](float x, float y, sol::this_state ts) -> sol::table {
        sol::state_view lua(ts);
        sol::table result = lua.create_table();
        if (m_ScriptAPI) {
            ScriptAPI::ScreenRay ray = m_ScriptAPI->ScreenToWorldRay3D(math::Vec2(x, y));
            result["origin"] = sol::as_table(std::vector<float>{ray.origin.x, ray.origin.y, ray.origin.z});
            result["direction"] = sol::as_table(std::vector<float>{ray.direction.x, ray.direction.y, ray.direction.z});
        } else {
            result["origin"] = sol::as_table(std::vector<float>{0.0f, 0.0f, 0.0f});
            result["direction"] = sol::as_table(std::vector<float>{0.0f, 0.0f, 0.0f});
        }
        return result;
    };

    lupine["sphere_cast_3d"] = [this](float fromX, float fromY, float fromZ, float toX, float toY, float toZ, float radius) -> sol::table {
        sol::table result = m_LuaState.create_table();
        if (m_ScriptAPI) {
            auto hit = m_ScriptAPI->SphereCast3D(math::Vec3(fromX, fromY, fromZ), math::Vec3(toX, toY, toZ), radius);
            result["hit"] = hit.hit;
            result["point_x"] = hit.point.x;
            result["point_y"] = hit.point.y;
            result["point_z"] = hit.point.z;
            result["normal_x"] = hit.normal.x;
            result["normal_y"] = hit.normal.y;
            result["normal_z"] = hit.normal.z;
            result["fraction"] = hit.fraction;
            result["body_id"] = hit.bodyId;
        } else {
            result["hit"] = false;
        }
        return result;
    };

    lupine["raycast_all_3d"] = [this](float fromX, float fromY, float fromZ, float dirX, float dirY, float dirZ,
                                      float maxDistance, sol::optional<uint32_t> collisionMask,
                                      sol::this_state ts) -> sol::table {
        sol::state_view lua(ts);
        sol::table results = lua.create_table();
        if (m_ScriptAPI) {
            int i = 1;
            for (const auto& hit : m_ScriptAPI->RaycastAll3D(math::Vec3(fromX, fromY, fromZ),
                                                             math::Vec3(dirX, dirY, dirZ), maxDistance,
                                                             collisionMask.value_or(0xFFFFFFFF))) {
                sol::table entry = lua.create_table();
                entry["hit"] = hit.hit;
                entry["point_x"] = hit.point.x;
                entry["point_y"] = hit.point.y;
                entry["point_z"] = hit.point.z;
                entry["normal_x"] = hit.normal.x;
                entry["normal_y"] = hit.normal.y;
                entry["normal_z"] = hit.normal.z;
                entry["distance"] = hit.distance;
                entry["fraction"] = hit.fraction;
                entry["body_id"] = hit.bodyId;
                entry["collider"] = WrapNode(lua, NodeRef::FromRaw(hit.collider, m_ScriptAPI));
                results[i++] = entry;
            }
        }
        return results;
    };

    lupine["overlap_sphere"] = [this](float centerX, float centerY, float centerZ, float radius,
                                      sol::optional<uint32_t> collisionMask, sol::this_state ts) -> sol::table {
        sol::state_view lua(ts);
        sol::table t = lua.create_table();
        if (m_ScriptAPI) {
            int i = 1;
            for (core::Node* node : m_ScriptAPI->OverlapSphere(math::Vec3(centerX, centerY, centerZ), radius,
                                                               collisionMask.value_or(0xFFFFFFFF))) {
                t[i++] = WrapNode(lua, NodeRef::FromRaw(node, m_ScriptAPI));
            }
        }
        return t;
    };

    lupine["overlap_box"] = [this](float centerX, float centerY, float centerZ,
                                   float halfX, float halfY, float halfZ,
                                   sol::optional<uint32_t> collisionMask, sol::this_state ts) -> sol::table {
        sol::state_view lua(ts);
        sol::table t = lua.create_table();
        if (m_ScriptAPI) {
            int i = 1;
            for (core::Node* node : m_ScriptAPI->OverlapBox(math::Vec3(centerX, centerY, centerZ),
                                                            math::Vec3(halfX, halfY, halfZ),
                                                            collisionMask.value_or(0xFFFFFFFF))) {
                t[i++] = WrapNode(lua, NodeRef::FromRaw(node, m_ScriptAPI));
            }
        }
        return t;
    };

    // ========================================================================
    // Physics 3D - Body Manipulation
    // ========================================================================

    lupine["get_linear_velocity_3d"] = [this]() -> sol::as_table_t<std::vector<float>> {
        if (m_ScriptAPI) {
            auto vel = m_ScriptAPI->GetLinearVelocity3D();
            return sol::as_table(std::vector<float>{vel.x, vel.y, vel.z});
        }
        return sol::as_table(std::vector<float>{0.0f, 0.0f, 0.0f});
    };
    lupine["set_linear_velocity_3d"] = [this](float x, float y, float z) {
        if (m_ScriptAPI) m_ScriptAPI->SetLinearVelocity3D(x, y, z);
    };
    lupine["get_angular_velocity_3d"] = [this]() -> sol::as_table_t<std::vector<float>> {
        if (m_ScriptAPI) {
            auto vel = m_ScriptAPI->GetAngularVelocity3D();
            return sol::as_table(std::vector<float>{vel.x, vel.y, vel.z});
        }
        return sol::as_table(std::vector<float>{0.0f, 0.0f, 0.0f});
    };
    lupine["set_angular_velocity_3d"] = [this](float x, float y, float z) {
        if (m_ScriptAPI) m_ScriptAPI->SetAngularVelocity3D(x, y, z);
    };

    lupine["apply_force_3d"] = [this](float forceX, float forceY, float forceZ) {
        if (m_ScriptAPI) m_ScriptAPI->ApplyForce3D(forceX, forceY, forceZ);
    };
    lupine["apply_force_at_point_3d"] = [this](float forceX, float forceY, float forceZ, float pointX, float pointY, float pointZ) {
        if (m_ScriptAPI) m_ScriptAPI->ApplyForceAtPoint3D(forceX, forceY, forceZ, pointX, pointY, pointZ);
    };
    lupine["apply_torque_3d"] = [this](float x, float y, float z) {
        if (m_ScriptAPI) m_ScriptAPI->ApplyTorque3D(x, y, z);
    };

    lupine["apply_impulse_3d"] = [this](float impulseX, float impulseY, float impulseZ) {
        if (m_ScriptAPI) m_ScriptAPI->ApplyImpulse3D(impulseX, impulseY, impulseZ);
    };
    lupine["apply_impulse_at_point_3d"] = [this](float impulseX, float impulseY, float impulseZ, float pointX, float pointY, float pointZ) {
        if (m_ScriptAPI) m_ScriptAPI->ApplyImpulseAtPoint3D(impulseX, impulseY, impulseZ, pointX, pointY, pointZ);
    };
    lupine["apply_torque_impulse_3d"] = [this](float x, float y, float z) {
        if (m_ScriptAPI) m_ScriptAPI->ApplyTorqueImpulse3D(x, y, z);
    };

    lupine["get_mass_3d"] = [this]() -> float {
        return m_ScriptAPI ? m_ScriptAPI->GetMass3D() : 0.0f;
    };
    lupine["set_mass_3d"] = [this](float mass) {
        if (m_ScriptAPI) m_ScriptAPI->SetMass3D(mass);
    };
    lupine["get_gravity_scale_3d"] = [this]() -> float {
        return m_ScriptAPI ? m_ScriptAPI->GetGravityScale3D() : 1.0f;
    };
    lupine["set_gravity_scale_3d"] = [this](float scale) {
        if (m_ScriptAPI) m_ScriptAPI->SetGravityScale3D(scale);
    };
    lupine["get_linear_damping_3d"] = [this]() -> float {
        return m_ScriptAPI ? m_ScriptAPI->GetLinearDamping3D() : 0.0f;
    };
    lupine["set_linear_damping_3d"] = [this](float damping) {
        if (m_ScriptAPI) m_ScriptAPI->SetLinearDamping3D(damping);
    };
    lupine["get_angular_damping_3d"] = [this]() -> float {
        return m_ScriptAPI ? m_ScriptAPI->GetAngularDamping3D() : 0.0f;
    };
    lupine["set_angular_damping_3d"] = [this](float damping) {
        if (m_ScriptAPI) m_ScriptAPI->SetAngularDamping3D(damping);
    };
    lupine["get_linear_factor_3d"] = [this]() -> sol::as_table_t<std::vector<float>> {
        if (m_ScriptAPI) {
            auto factor = m_ScriptAPI->GetLinearFactor3D();
            return sol::as_table(std::vector<float>{factor.x, factor.y, factor.z});
        }
        return sol::as_table(std::vector<float>{1.0f, 1.0f, 1.0f});
    };
    lupine["set_linear_factor_3d"] = [this](float x, float y, float z) {
        if (m_ScriptAPI) m_ScriptAPI->SetLinearFactor3D(x, y, z);
    };
    lupine["get_angular_factor_3d"] = [this]() -> sol::as_table_t<std::vector<float>> {
        if (m_ScriptAPI) {
            auto factor = m_ScriptAPI->GetAngularFactor3D();
            return sol::as_table(std::vector<float>{factor.x, factor.y, factor.z});
        }
        return sol::as_table(std::vector<float>{1.0f, 1.0f, 1.0f});
    };
    lupine["set_angular_factor_3d"] = [this](float x, float y, float z) {
        if (m_ScriptAPI) m_ScriptAPI->SetAngularFactor3D(x, y, z);
    };

    // ========================================================================
    // Physics World Access
    // ========================================================================

    lupine["set_gravity_2d"] = [this](float x, float y) {
        if (m_ScriptAPI) m_ScriptAPI->SetGravity2D(x, y);
    };
    lupine["get_gravity_2d"] = [this]() -> sol::as_table_t<std::vector<float>> {
        if (m_ScriptAPI) {
            auto gravity = m_ScriptAPI->GetGravity2D();
            return sol::as_table(std::vector<float>{gravity.x, gravity.y});
        }
        return sol::as_table(std::vector<float>{0.0f, -9.81f});
    };
    lupine["set_gravity_3d"] = [this](float x, float y, float z) {
        if (m_ScriptAPI) m_ScriptAPI->SetGravity3D(x, y, z);
    };
    lupine["get_gravity_3d"] = [this]() -> sol::as_table_t<std::vector<float>> {
        if (m_ScriptAPI) {
            auto gravity = m_ScriptAPI->GetGravity3D();
            return sol::as_table(std::vector<float>{gravity.x, gravity.y, gravity.z});
        }
        return sol::as_table(std::vector<float>{0.0f, -9.81f, 0.0f});
    };

    // ========================================================================
    // Character Controller (2D)
    // ========================================================================

    lupine["move_and_slide_2d"] = [this](float velocityX, float velocityY) -> sol::as_table_t<std::vector<float>> {
        if (m_ScriptAPI) {
            auto result = m_ScriptAPI->MoveAndSlide2D(velocityX, velocityY);
            return sol::as_table(std::vector<float>{result.x, result.y});
        }
        return sol::as_table(std::vector<float>{0.0f, 0.0f});
    };

    lupine["get_character_velocity_2d"] = [this]() -> sol::as_table_t<std::vector<float>> {
        if (m_ScriptAPI) {
            auto velocity = m_ScriptAPI->GetCharacterVelocity2D();
            return sol::as_table(std::vector<float>{velocity.x, velocity.y});
        }
        return sol::as_table(std::vector<float>{0.0f, 0.0f});
    };

    lupine["set_character_velocity_2d"] = [this](float x, float y) {
        if (m_ScriptAPI) m_ScriptAPI->SetCharacterVelocity2D(x, y);
    };

    lupine["is_on_ground_2d"] = [this]() -> bool {
        if (m_ScriptAPI) return m_ScriptAPI->IsCharacterOnGround2D();
        return false;
    };

    lupine["is_on_wall_2d"] = [this]() -> bool {
        if (m_ScriptAPI) return m_ScriptAPI->IsCharacterOnWall2D();
        return false;
    };

    lupine["is_on_ceiling_2d"] = [this]() -> bool {
        if (m_ScriptAPI) return m_ScriptAPI->IsCharacterOnCeiling2D();
        return false;
    };

    lupine["get_ground_normal_2d"] = [this]() -> sol::as_table_t<std::vector<float>> {
        if (m_ScriptAPI) {
            auto normal = m_ScriptAPI->GetCharacterGroundNormal2D();
            return sol::as_table(std::vector<float>{normal.x, normal.y});
        }
        return sol::as_table(std::vector<float>{0.0f, 1.0f});
    };

    lupine["get_wall_normal_2d"] = [this]() -> sol::as_table_t<std::vector<float>> {
        if (m_ScriptAPI) {
            auto normal = m_ScriptAPI->GetCharacterWallNormal2D();
            return sol::as_table(std::vector<float>{normal.x, normal.y});
        }
        return sol::as_table(std::vector<float>{0.0f, 0.0f});
    };

    lupine["get_character_gravity_2d"] = [this]() -> float {
        if (m_ScriptAPI) return m_ScriptAPI->GetCharacterGravity2D();
        return 980.0f;
    };

    lupine["set_character_gravity_2d"] = [this](float gravity) {
        if (m_ScriptAPI) m_ScriptAPI->SetCharacterGravity2D(gravity);
    };

    lupine["get_character_max_fall_speed_2d"] = [this]() -> float {
        if (m_ScriptAPI) return m_ScriptAPI->GetCharacterMaxFallSpeed2D();
        return 1000.0f;
    };

    lupine["set_character_max_fall_speed_2d"] = [this](float speed) {
        if (m_ScriptAPI) m_ScriptAPI->SetCharacterMaxFallSpeed2D(speed);
    };

    lupine["get_character_max_slope_angle_2d"] = [this]() -> float {
        if (m_ScriptAPI) return m_ScriptAPI->GetCharacterMaxSlopeAngle2D();
        return 45.0f;
    };

    lupine["set_character_max_slope_angle_2d"] = [this](float angle) {
        if (m_ScriptAPI) m_ScriptAPI->SetCharacterMaxSlopeAngle2D(angle);
    };

    lupine["get_character_snap_to_ground_2d"] = [this]() -> bool {
        if (m_ScriptAPI) return m_ScriptAPI->GetCharacterSnapToGround2D();
        return true;
    };

    lupine["set_character_snap_to_ground_2d"] = [this](bool snap) {
        if (m_ScriptAPI) m_ScriptAPI->SetCharacterSnapToGround2D(snap);
    };

    // ========================================================================
    // Character Controller (3D)
    // ========================================================================

    lupine["move_and_slide_3d"] = [this](float velocityX, float velocityY, float velocityZ) -> sol::as_table_t<std::vector<float>> {
        if (m_ScriptAPI) {
            auto result = m_ScriptAPI->MoveAndSlide3D(velocityX, velocityY, velocityZ);
            return sol::as_table(std::vector<float>{result.x, result.y, result.z});
        }
        return sol::as_table(std::vector<float>{0.0f, 0.0f, 0.0f});
    };

    lupine["get_character_velocity_3d"] = [this]() -> sol::as_table_t<std::vector<float>> {
        if (m_ScriptAPI) {
            auto velocity = m_ScriptAPI->GetCharacterVelocity3D();
            return sol::as_table(std::vector<float>{velocity.x, velocity.y, velocity.z});
        }
        return sol::as_table(std::vector<float>{0.0f, 0.0f, 0.0f});
    };

    lupine["set_character_velocity_3d"] = [this](float x, float y, float z) {
        if (m_ScriptAPI) m_ScriptAPI->SetCharacterVelocity3D(x, y, z);
    };

    lupine["is_on_ground_3d"] = [this]() -> bool {
        if (m_ScriptAPI) return m_ScriptAPI->IsCharacterOnGround3D();
        return false;
    };

    lupine["is_on_wall_3d"] = [this]() -> bool {
        if (m_ScriptAPI) return m_ScriptAPI->IsCharacterOnWall3D();
        return false;
    };

    lupine["is_on_ceiling_3d"] = [this]() -> bool {
        if (m_ScriptAPI) return m_ScriptAPI->IsCharacterOnCeiling3D();
        return false;
    };

    lupine["get_ground_normal_3d"] = [this]() -> sol::as_table_t<std::vector<float>> {
        if (m_ScriptAPI) {
            auto normal = m_ScriptAPI->GetCharacterGroundNormal3D();
            return sol::as_table(std::vector<float>{normal.x, normal.y, normal.z});
        }
        return sol::as_table(std::vector<float>{0.0f, 1.0f, 0.0f});
    };

    lupine["get_wall_normal_3d"] = [this]() -> sol::as_table_t<std::vector<float>> {
        if (m_ScriptAPI) {
            auto normal = m_ScriptAPI->GetCharacterWallNormal3D();
            return sol::as_table(std::vector<float>{normal.x, normal.y, normal.z});
        }
        return sol::as_table(std::vector<float>{0.0f, 0.0f, 0.0f});
    };

    lupine["get_character_gravity_3d"] = [this]() -> float {
        if (m_ScriptAPI) return m_ScriptAPI->GetCharacterGravity3D();
        return 9.81f;
    };

    lupine["set_character_gravity_3d"] = [this](float gravity) {
        if (m_ScriptAPI) m_ScriptAPI->SetCharacterGravity3D(gravity);
    };

    lupine["get_character_max_fall_speed_3d"] = [this]() -> float {
        if (m_ScriptAPI) return m_ScriptAPI->GetCharacterMaxFallSpeed3D();
        return 50.0f;
    };

    lupine["set_character_max_fall_speed_3d"] = [this](float speed) {
        if (m_ScriptAPI) m_ScriptAPI->SetCharacterMaxFallSpeed3D(speed);
    };

    lupine["get_character_max_slope_angle_3d"] = [this]() -> float {
        if (m_ScriptAPI) return m_ScriptAPI->GetCharacterMaxSlopeAngle3D();
        return 45.0f;
    };

    lupine["set_character_max_slope_angle_3d"] = [this](float angle) {
        if (m_ScriptAPI) m_ScriptAPI->SetCharacterMaxSlopeAngle3D(angle);
    };

    lupine["get_character_step_height_3d"] = [this]() -> float {
        if (m_ScriptAPI) return m_ScriptAPI->GetCharacterStepHeight3D();
        return 0.3f;
    };

    lupine["set_character_step_height_3d"] = [this](float height) {
        if (m_ScriptAPI) m_ScriptAPI->SetCharacterStepHeight3D(height);
    };

    lupine["get_character_snap_to_ground_3d"] = [this]() -> bool {
        if (m_ScriptAPI) return m_ScriptAPI->GetCharacterSnapToGround3D();
        return true;
    };

    lupine["set_character_snap_to_ground_3d"] = [this](bool snap) {
        if (m_ScriptAPI) m_ScriptAPI->SetCharacterSnapToGround3D(snap);
    };

    // Archetype data assets (Unity ScriptableObject / Godot Resource equivalent)
    lupine["load_archetype"] = [this](const std::string& path) -> sol::object {
        if (!m_ScriptAPI) {
            return sol::make_object(m_LuaState, sol::lua_nil);
        }
        asset::ArchetypeInstance* instance = m_ScriptAPI->LoadArchetype(path);
        if (!instance) {
            return sol::make_object(m_LuaState, sol::lua_nil);
        }
        return JsonToLua(m_LuaState, instance->GetResolvedFields());
    };
    lupine["get_archetype_field"] = [this](const std::string& path, const std::string& name) -> sol::object {
        if (!m_ScriptAPI) {
            return sol::make_object(m_LuaState, sol::lua_nil);
        }
        asset::ArchetypeInstance* instance = m_ScriptAPI->LoadArchetype(path);
        if (!instance) {
            return sol::make_object(m_LuaState, sol::lua_nil);
        }
        return JsonToLua(m_LuaState, instance->GetFieldJson(name));
    };
    lupine["get_archetype_class"] = [this](const std::string& path) -> sol::object {
        if (!m_ScriptAPI) {
            return sol::make_object(m_LuaState, sol::lua_nil);
        }
        asset::ArchetypeInstance* instance = m_ScriptAPI->LoadArchetype(path);
        if (!instance) {
            return sol::make_object(m_LuaState, sol::lua_nil);
        }
        return sol::make_object(m_LuaState, instance->GetArchetypeClass());
    };
    lupine["archetype_is_a"] = [this](const std::string& path, const std::string& className) -> bool {
        if (!m_ScriptAPI) {
            return false;
        }
        asset::ArchetypeInstance* instance = m_ScriptAPI->LoadArchetype(path);
        return instance ? instance->IsArchetype(className) : false;
    };
    lupine["call_archetype"] = [this](const std::string& path, const std::string& method,
                                      sol::variadic_args va) -> sol::object {
        nlohmann::json args = nlohmann::json::array();
        for (auto arg : va) {
            sol::object value = arg;
            args.push_back(LuaToJson(value));
        }
        nlohmann::json result = core::ArchetypeRuntime::GetInstance().CallMethod(path, method, args);
        return JsonToLua(m_LuaState, result);
    };

    // Asynchronous archetype loading (file read + parse on a worker thread,
    // finalized on the main thread). Returns a numeric handle; poll with
    // get_archetype_load_status / is_archetype_load_complete and take delivery
    // with get_async_archetype, or pass a callback method name to be invoked as
    // callback(handle, fields_or_nil) when the load completes.
    lupine["load_archetype_async"] = [this](const std::string& path, sol::optional<std::string> callback,
                                            sol::optional<double> priority) -> double {
        if (!m_ScriptAPI) {
            return 0.0;
        }
        int prio = priority.has_value() ? static_cast<int>(priority.value())
                                        : static_cast<int>(ScriptAPI::ASYNC_PRIORITY_NORMAL);
        uint64_t handle = m_ScriptAPI->LoadArchetypeAsync(path, callback.value_or(std::string()), prio);
        return static_cast<double>(handle);
    };
    lupine["load_archetype_definition_async"] = [this](const std::string& path, sol::optional<std::string> callback,
                                                       sol::optional<double> priority) -> double {
        if (!m_ScriptAPI) {
            return 0.0;
        }
        int prio = priority.has_value() ? static_cast<int>(priority.value())
                                        : static_cast<int>(ScriptAPI::ASYNC_PRIORITY_NORMAL);
        uint64_t handle = m_ScriptAPI->LoadArchetypeDefinitionAsync(path, callback.value_or(std::string()), prio);
        return static_cast<double>(handle);
    };
    lupine["get_archetype_load_status"] = [this](double handle) -> int {
        if (!m_ScriptAPI) {
            return 4;
        }
        return m_ScriptAPI->GetAsyncLoadStatus(static_cast<uint64_t>(handle));
    };
    lupine["is_archetype_load_complete"] = [this](double handle) -> bool {
        if (!m_ScriptAPI) {
            return true;
        }
        return m_ScriptAPI->IsAsyncLoadComplete(static_cast<uint64_t>(handle));
    };
    lupine["get_async_archetype"] = [this](double handle) -> sol::object {
        if (!m_ScriptAPI) {
            return sol::make_object(m_LuaState, sol::lua_nil);
        }
        asset::ArchetypeInstance* instance = m_ScriptAPI->GetAsyncArchetype(static_cast<uint64_t>(handle));
        if (!instance) {
            return sol::make_object(m_LuaState, sol::lua_nil);
        }
        return JsonToLua(m_LuaState, instance->GetResolvedFields());
    };
    lupine["cancel_archetype_load"] = [this](double handle) {
        if (m_ScriptAPI) {
            m_ScriptAPI->CancelAsyncLoad(static_cast<uint64_t>(handle));
        }
    };

    // Priority streaming controls: re-prioritize a queued load, set the streaming
    // budget (max concurrent worker loads), and query the queue state. Priority
    // is a plain number (higher = streamed in first); the STREAM_PRIORITY_*
    // constants name the common bands.
    lupine["set_archetype_load_priority"] = [this](double handle, double priority) {
        if (m_ScriptAPI) {
            m_ScriptAPI->SetAsyncLoadPriority(static_cast<uint64_t>(handle), static_cast<int>(priority));
        }
    };
    lupine["get_archetype_load_priority"] = [this](double handle) -> int {
        if (!m_ScriptAPI) {
            return 0;
        }
        return m_ScriptAPI->GetAsyncLoadPriority(static_cast<uint64_t>(handle));
    };
    lupine["set_archetype_streaming_budget"] = [this](double maxConcurrent) {
        if (m_ScriptAPI) {
            m_ScriptAPI->SetAsyncStreamingBudget(static_cast<int>(maxConcurrent));
        }
    };
    lupine["get_archetype_streaming_budget"] = [this]() -> int {
        if (!m_ScriptAPI) {
            return 0;
        }
        return m_ScriptAPI->GetAsyncStreamingBudget();
    };
    lupine["get_archetype_inflight_count"] = [this]() -> int {
        if (!m_ScriptAPI) {
            return 0;
        }
        return m_ScriptAPI->GetAsyncInFlightCount();
    };
    lupine["get_archetype_queued_count"] = [this]() -> int {
        if (!m_ScriptAPI) {
            return 0;
        }
        return m_ScriptAPI->GetAsyncQueuedCount();
    };
    lupine["STREAM_PRIORITY_LOW"] = static_cast<int>(ScriptAPI::ASYNC_PRIORITY_LOW);
    lupine["STREAM_PRIORITY_NORMAL"] = static_cast<int>(ScriptAPI::ASYNC_PRIORITY_NORMAL);
    lupine["STREAM_PRIORITY_HIGH"] = static_cast<int>(ScriptAPI::ASYNC_PRIORITY_HIGH);
    lupine["STREAM_PRIORITY_CRITICAL"] = static_cast<int>(ScriptAPI::ASYNC_PRIORITY_CRITICAL);

    // ----------------------------------------------------------------------
    // Coroutine / await scheduler (pure Lua, pumped each frame by
    // LuaEnvironment::Update -> Lupine._pump). Lets scripts write sequential
    // async flows:
    //   Lupine.start_coroutine(function()
    //       local tw = self:create_tween("position_2d", {100,0}, 1.0, "sine_in_out")
    //       Lupine.await_tween(tw)
    //       Lupine.await_seconds(0.5)
    //       Lupine.log_info("done")
    //   end)
    // ----------------------------------------------------------------------
    m_LuaState.script(R"LUA(
        Lupine._coros = Lupine._coros or {}

        function Lupine._step(entry, dt)
            if coroutine.status(entry.co) == "dead" then
                entry.dead = true
                return
            end
            -- Resume with the coroutine's originating api active, then restore the
            -- previous api so a resumed coroutine never leaks its (possibly
            -- soon-freed) component api into the next pump iteration or the
            -- post-pump deferred-signal flush.
            local prev_api = Lupine.__cur_api()
            Lupine.__set_api(entry.api)
            local ok, req = coroutine.resume(entry.co, dt)
            Lupine.__set_api(prev_api)
            if not ok then
                Lupine.log_error("coroutine error: " .. tostring(req))
                entry.dead = true
                return
            end
            entry.wait = req
        end

        function Lupine.start_coroutine(fn)
            local co = coroutine.create(fn)
            local entry = { co = co, wait = nil, dead = false, api = Lupine.__cur_api() }
            table.insert(Lupine._coros, entry)
            Lupine._step(entry, 0.0)
            return co
        end

        function Lupine.stop_coroutine(co)
            for i, entry in ipairs(Lupine._coros) do
                if entry.co == co then
                    entry.dead = true
                    return
                end
            end
        end

        function Lupine.coroutine_count()
            return #Lupine._coros
        end

        -- Drop every entry started by a given script instance. Called from
        -- LuaEnvironment::Shutdown: an entry's `api` is a raw pointer to the
        -- owning component's ScriptAPI, which _step re-installs as the active api
        -- on each resume, so entries must not outlive their instance.
        function Lupine._stop_for_api(api)
            if api == nil then
                return
            end
            local i = 1
            while i <= #Lupine._coros do
                local entry = Lupine._coros[i]
                if entry.api == api then
                    entry.dead = true
                    table.remove(Lupine._coros, i)
                else
                    i = i + 1
                end
            end
        end

        -- An "until" predicate is script code that may call self-relative Lupine
        -- APIs (await_archetype's does), so it runs with its own coroutine's api
        -- active; and it is called protected so a raising predicate kills only its
        -- own entry instead of unwinding the whole pump (which would starve every
        -- later coroutine and re-raise every frame).
        function Lupine._ready(entry, dt)
            local w = entry.wait
            if w == nil then
                return true
            elseif w.type == "time" then
                w.t = w.t - dt
                return w.t <= 0.0
            elseif w.type == "frames" then
                w.n = w.n - 1
                return w.n <= 0
            elseif w.type == "until" then
                local prev_api = Lupine.__cur_api()
                Lupine.__set_api(entry.api)
                local ok, ready = pcall(w.fn)
                Lupine.__set_api(prev_api)
                if not ok then
                    Lupine.log_error("await predicate error: " .. tostring(ready))
                    entry.dead = true
                    return false
                end
                return ready and true or false
            end
            return true
        end

        function Lupine._pump(dt)
            local i = 1
            while i <= #Lupine._coros do
                local entry = Lupine._coros[i]
                local remove = entry.dead or coroutine.status(entry.co) == "dead"
                if not remove then
                    if Lupine._ready(entry, dt) then
                        Lupine._step(entry, dt)
                    end
                    remove = entry.dead or coroutine.status(entry.co) == "dead"
                end
                if remove then
                    table.remove(Lupine._coros, i)
                else
                    i = i + 1
                end
            end
        end

        function Lupine.await_seconds(s)
            return coroutine.yield({ type = "time", t = s })
        end

        function Lupine.await_frames(n)
            return coroutine.yield({ type = "frames", n = n or 1 })
        end

        function Lupine.await_until(predicate)
            return coroutine.yield({ type = "until", fn = predicate })
        end

        function Lupine.await_tween(tw)
            return coroutine.yield({ type = "until", fn = function()
                return (not tw:is_valid()) or tw:is_finished()
            end })
        end

        function Lupine.await_signal(obj, signal)
            local awaiter = obj:await_signal(signal)
            if awaiter == nil then return end
            return coroutine.yield({ type = "until", fn = function()
                return (not awaiter:is_valid()) or awaiter:is_fired()
            end })
        end

        function Lupine.await_next_frame()
            return coroutine.yield({ type = "frames", n = 1 })
        end

        function Lupine.await_archetype(handle)
            coroutine.yield({ type = "until", fn = function()
                return Lupine.is_archetype_load_complete(handle)
            end })
            return Lupine.get_async_archetype(handle)
        end
    )LUA");

    // Register base component proxies for native class syntax inheritance
    RegisterBaseComponentProxies();
}

sol::table LuaHost::MakeComponentProxy(const std::string& componentType) {
    sol::table proxy = m_LuaState.create_table();

    // Store the component type for later reference
    proxy["__componentType"] = componentType;
    proxy["__isLupineComponent"] = true;

    // Add extend() method that returns a new table inheriting from this component
    proxy["extend"] = [this, componentType]() -> sol::table {
        sol::table derived = m_LuaState.create_table();
        derived["__baseComponentType"] = componentType;
        derived["__isScriptedComponent"] = true;

        // Set up metatable for basic inheritance behavior
        sol::table mt = m_LuaState.create_table();
        mt["__index"] = derived;  // Self-referential for now
        derived[sol::metatable_key] = mt;

        return derived;
    };

    return proxy;
}

void LuaHost::RegisterCustomComponentTypes() {
    if (!m_Initialized) return;

    // A custom component is an inheritable base exactly like a built-in one, so every
    // discovered class name gets the same extendable proxy. Without this, a chain such
    // as SimObject(Sprite2D) -> SimBoulder(SimObject) dies at script-execution time:
    // SimObject is not a name in the VM, so `SimObject:extend()` indexes a nil value
    // and the whole component script fails to load.
    for (const core::CustomComponentDefinition& def :
         core::CustomComponentRegistry::GetInstance().GetDefinitions()) {
        if (!def.isValid || def.className.empty()) {
            continue;
        }
        // Leave any name that already resolves alone: it is either a built-in proxy
        // (which must not be shadowed) or a custom proxy from an earlier pass, and
        // swapping in a fresh table would orphan the references loaded scripts hold.
        if (m_LuaState[def.className].valid()) {
            continue;
        }
        m_LuaState[def.className] = MakeComponentProxy(def.className);
    }
}

void LuaHost::RegisterBaseComponentProxies() {
    if (!m_Initialized) return;

    auto createComponentProxy = [this](const std::string& componentType) {
        return MakeComponentProxy(componentType);
    };

    // Register all inheritable component types as global tables
    // 2D Rendering
    m_LuaState["Sprite2D"] = createComponentProxy("Sprite2D");
    m_LuaState["Particles2D"] = createComponentProxy("Particles2D");
    m_LuaState["AnimatedSprite2D"] = createComponentProxy("AnimatedSprite2D");
    m_LuaState["GifPlayer"] = createComponentProxy("GifPlayer");
    m_LuaState["VideoPlayer"] = createComponentProxy("VideoPlayer");
    m_LuaState["ColorRect"] = createComponentProxy("ColorRect");
    m_LuaState["Image2D"] = createComponentProxy("Image2D");
    m_LuaState["NineSlicePanel"] = createComponentProxy("NineSlicePanel");
    m_LuaState["Shape2D"] = createComponentProxy("Shape2D");
    m_LuaState["Line2D"] = createComponentProxy("Line2D");
    m_LuaState["Curve2D"] = createComponentProxy("Curve2D");
    m_LuaState["Light2D"] = createComponentProxy("Light2D");
    m_LuaState["LightOccluder2D"] = createComponentProxy("LightOccluder2D");
    m_LuaState["VectorGraphic2D"] = createComponentProxy("VectorGraphic2D");
    m_LuaState["Empty2D"] = createComponentProxy("Empty2D");

    // 3D Rendering
    m_LuaState["Sprite3D"] = createComponentProxy("Sprite3D");
    m_LuaState["Particles3D"] = createComponentProxy("Particles3D");
    m_LuaState["AnimatedSprite3D"] = createComponentProxy("AnimatedSprite3D");
    m_LuaState["StaticMesh3D"] = createComponentProxy("StaticMesh3D");
    m_LuaState["SkeletalMesh3D"] = createComponentProxy("SkeletalMesh3D");
    m_LuaState["PrimitiveMesh3D"] = createComponentProxy("PrimitiveMesh3D");
    m_LuaState["Label3D"] = createComponentProxy("Label3D");
    m_LuaState["Panel3D"] = createComponentProxy("Panel3D");
    m_LuaState["Button3D"] = createComponentProxy("Button3D");
    m_LuaState["ProgressBar3D"] = createComponentProxy("ProgressBar3D");
    m_LuaState["Curve3D"] = createComponentProxy("Curve3D");
    m_LuaState["Path3D"] = createComponentProxy("Path3D");
    m_LuaState["PathFollow3D"] = createComponentProxy("PathFollow3D");
    m_LuaState["Empty3D"] = createComponentProxy("Empty3D");

    // UI Components
    m_LuaState["Button"] = createComponentProxy("Button");
    m_LuaState["TextureButton"] = createComponentProxy("TextureButton");
    m_LuaState["ToggleButton"] = createComponentProxy("ToggleButton");
    m_LuaState["Checkbox"] = createComponentProxy("Checkbox");
    m_LuaState["RadioButton"] = createComponentProxy("RadioButton");
    m_LuaState["Label"] = createComponentProxy("Label");
    m_LuaState["ProgressBar"] = createComponentProxy("ProgressBar");
    m_LuaState["Slider"] = createComponentProxy("Slider");
    m_LuaState["LineEdit"] = createComponentProxy("LineEdit");
    m_LuaState["SpinBox"] = createComponentProxy("SpinBox");
    m_LuaState["TextEdit"] = createComponentProxy("TextEdit");
    m_LuaState["ItemList"] = createComponentProxy("ItemList");
    m_LuaState["Dropdown"] = createComponentProxy("Dropdown");
    m_LuaState["PopupMenu"] = createComponentProxy("PopupMenu");
    m_LuaState["RichTextLabel"] = createComponentProxy("RichTextLabel");
    m_LuaState["Tree"] = createComponentProxy("Tree");
    m_LuaState["Panel"] = createComponentProxy("Panel");
    m_LuaState["Container"] = createComponentProxy("Container");
    m_LuaState["HorizontalContainer"] = createComponentProxy("HorizontalContainer");
    m_LuaState["VerticalContainer"] = createComponentProxy("VerticalContainer");
    m_LuaState["GridContainer"] = createComponentProxy("GridContainer");
    m_LuaState["PaddingContainer"] = createComponentProxy("PaddingContainer");
    m_LuaState["CenterContainer"] = createComponentProxy("CenterContainer");
    m_LuaState["DockContainer"] = createComponentProxy("DockContainer");
    m_LuaState["Stack"] = createComponentProxy("Stack");
    m_LuaState["Wrap"] = createComponentProxy("Wrap");
    m_LuaState["SplitContainer"] = createComponentProxy("SplitContainer");
    m_LuaState["AspectRatioContainer"] = createComponentProxy("AspectRatioContainer");
    m_LuaState["LayoutSlot"] = createComponentProxy("LayoutSlot");
    m_LuaState["ScrollContainer"] = createComponentProxy("ScrollContainer");
    m_LuaState["TabContainer"] = createComponentProxy("TabContainer");

    // Animation
    m_LuaState["AnimationPlayer"] = createComponentProxy("AnimationPlayer");
    m_LuaState["AnimationTree"] = createComponentProxy("AnimationTree");

    // Audio
    m_LuaState["AudioPlayer"] = createComponentProxy("AudioPlayer");
    m_LuaState["AudioListener"] = createComponentProxy("AudioListener");

    // Physics 2D
    m_LuaState["RigidBody2DComponent"] = createComponentProxy("RigidBody2DComponent");
    m_LuaState["StaticBody2DComponent"] = createComponentProxy("StaticBody2DComponent");
    m_LuaState["KinematicBody2DComponent"] = createComponentProxy("KinematicBody2DComponent");
    m_LuaState["AreaTrigger2DComponent"] = createComponentProxy("AreaTrigger2DComponent");
    m_LuaState["CollisionBody2DComponent"] = createComponentProxy("CollisionBody2DComponent");
    m_LuaState["CharacterController2DComponent"] = createComponentProxy("CharacterController2DComponent");
    m_LuaState["RayCast2D"] = createComponentProxy("RayCast2D");
    m_LuaState["ShapeCast2D"] = createComponentProxy("ShapeCast2D");

    // Physics 3D
    m_LuaState["RigidBody3DComponent"] = createComponentProxy("RigidBody3DComponent");
    m_LuaState["StaticBody3DComponent"] = createComponentProxy("StaticBody3DComponent");
    m_LuaState["KinematicBody3DComponent"] = createComponentProxy("KinematicBody3DComponent");
    m_LuaState["AreaTrigger3DComponent"] = createComponentProxy("AreaTrigger3DComponent");
    m_LuaState["CharacterController3DComponent"] = createComponentProxy("CharacterController3DComponent");
    m_LuaState["RayCast3D"] = createComponentProxy("RayCast3D");
    m_LuaState["ShapeCast3D"] = createComponentProxy("ShapeCast3D");

    // Lighting
    m_LuaState["DirectionalLight3D"] = createComponentProxy("DirectionalLight3D");
    m_LuaState["OmniLight3D"] = createComponentProxy("OmniLight3D");
    m_LuaState["SpotLight3D"] = createComponentProxy("SpotLight3D");

    // Utility
    m_LuaState["Timer"] = createComponentProxy("Timer");
    m_LuaState["SubViewport"] = createComponentProxy("SubViewport");
    m_LuaState["CameraEffectColorGrade"] = createComponentProxy("CameraEffectColorGrade");
    m_LuaState["CameraEffectTonemap"] = createComponentProxy("CameraEffectTonemap");
    m_LuaState["CameraEffectVignette"] = createComponentProxy("CameraEffectVignette");
    m_LuaState["CameraEffectFilmGrain"] = createComponentProxy("CameraEffectFilmGrain");
    m_LuaState["CameraEffectColorInvert"] = createComponentProxy("CameraEffectColorInvert");
    m_LuaState["CameraEffectPosterize"] = createComponentProxy("CameraEffectPosterize");
    m_LuaState["CameraEffectHueShift"] = createComponentProxy("CameraEffectHueShift");
    m_LuaState["CameraEffectBlur"] = createComponentProxy("CameraEffectBlur");
    m_LuaState["CameraEffectGlow"] = createComponentProxy("CameraEffectGlow");
    m_LuaState["CameraEffectOutline"] = createComponentProxy("CameraEffectOutline");
    m_LuaState["CameraEffectPixelate"] = createComponentProxy("CameraEffectPixelate");
    m_LuaState["CameraEffectSharpen"] = createComponentProxy("CameraEffectSharpen");
    m_LuaState["CameraEffectChromaticAberration"] = createComponentProxy("CameraEffectChromaticAberration");
    m_LuaState["WorldEnvironment"] = createComponentProxy("WorldEnvironment");
    m_LuaState["Camera2D"] = createComponentProxy("Camera2D");
    m_LuaState["Camera3D"] = createComponentProxy("Camera3D");
    m_LuaState["TileMap2D"] = createComponentProxy("TileMap2D");
    m_LuaState["NavigationRegion2D"] = createComponentProxy("NavigationRegion2D");
    m_LuaState["NavigationAgent2D"] = createComponentProxy("NavigationAgent2D");
    m_LuaState["NavigationObstacle2D"] = createComponentProxy("NavigationObstacle2D");
    m_LuaState["NavigationRegion3D"] = createComponentProxy("NavigationRegion3D");
    m_LuaState["NavigationAgent3D"] = createComponentProxy("NavigationAgent3D");
    m_LuaState["NavigationObstacle3D"] = createComponentProxy("NavigationObstacle3D");
    m_LuaState["NetworkObject"] = createComponentProxy("NetworkObject");
    m_LuaState["NetworkSynchronizer"] = createComponentProxy("NetworkSynchronizer");
    m_LuaState["NetworkTransform2D"] = createComponentProxy("NetworkTransform2D");
    m_LuaState["NetworkTransform3D"] = createComponentProxy("NetworkTransform3D");
    m_LuaState["NetworkSpawner"] = createComponentProxy("NetworkSpawner");
    m_LuaState["NetworkController"] = createComponentProxy("NetworkController");
    m_LuaState["NetworkAnimator"] = createComponentProxy("NetworkAnimator");
    m_LuaState["NetworkRigidBody2D"] = createComponentProxy("NetworkRigidBody2D");
    m_LuaState["NetworkRigidBody3D"] = createComponentProxy("NetworkRigidBody3D");
    m_LuaState["ParticleEmitter2D"] = createComponentProxy("ParticleEmitter2D");
    m_LuaState["ParticleEmitter3D"] = createComponentProxy("ParticleEmitter3D");
    m_LuaState["YSort"] = createComponentProxy("YSort");
    m_LuaState["ParallaxBackground"] = createComponentProxy("ParallaxBackground");
    m_LuaState["ParallaxLayer"] = createComponentProxy("ParallaxLayer");

    // Also provide a generic class() function for OOP patterns
    m_LuaState.set_function("class", [this](const std::string& name, sol::object baseObj) -> sol::table {
        sol::table derived = m_LuaState.create_table();
        derived["__className"] = name;

        if (baseObj.is<sol::table>()) {
            sol::table base = baseObj.as<sol::table>();
            if (base["__isLupineComponent"].valid() && base["__isLupineComponent"].get<bool>()) {
                derived["__baseComponentType"] = base["__componentType"].get<std::string>();
                derived["__isScriptedComponent"] = true;
            }
        }

        return derived;
    });
}

}
}
