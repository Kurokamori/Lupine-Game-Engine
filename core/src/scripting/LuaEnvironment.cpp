#include "lupine/scripting/LuaEnvironment.hpp"
#include "lupine/logger/Logger.hpp"

namespace lupine {
namespace scripting {

LuaEnvironment::LuaEnvironment() {
}

LuaEnvironment::~LuaEnvironment() {
    Shutdown();
}

bool LuaEnvironment::Initialize() {
    if (m_Initialized) {
        return true;
    }

    try {

        m_LuaState.open_libraries(
            sol::lib::base,
            sol::lib::package,
            sol::lib::math,
            sol::lib::string,
            sol::lib::table
        );

        RegisterScriptAPI();

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

    try {

        m_Initialized = false;

    }
    catch (const std::exception& e) {

    }
}

ScriptResult LuaEnvironment::ExecuteFile(const std::string& filepath) {
    if (!m_Initialized) {
        return ScriptResult(false, "Lua environment not initialized");
    }

    try {
        auto result = m_LuaState.script_file(filepath);

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

ScriptResult LuaEnvironment::ExecuteString(const std::string& script) {
    if (!m_Initialized) {
        return ScriptResult(false, "Lua environment not initialized");
    }

    try {
        auto result = m_LuaState.script(script);

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

ScriptResult LuaEnvironment::CallFunction(const std::string& functionName) {
    if (!m_Initialized) {
        return ScriptResult(false, "Lua environment not initialized");
    }

    try {
        if (!HasFunction(functionName)) {
            return ScriptResult(false, "Function '" + functionName + "' not found");
        }

        sol::protected_function func = m_LuaState[functionName];
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

bool LuaEnvironment::HasFunction(const std::string& functionName) const {
    if (!m_Initialized) {
        return false;
    }

    try {
        sol::object obj = m_LuaState[functionName];
        return obj.is<sol::function>();
    }
    catch (...) {
        return false;
    }
}

void LuaEnvironment::SetGlobal(const std::string& name, int value) {
    if (m_Initialized) {
        m_LuaState[name] = value;
    }
}

void LuaEnvironment::SetGlobal(const std::string& name, float value) {
    if (m_Initialized) {
        m_LuaState[name] = value;
    }
}

void LuaEnvironment::SetGlobal(const std::string& name, const std::string& value) {
    if (m_Initialized) {
        m_LuaState[name] = value;
    }
}

void LuaEnvironment::SetGlobal(const std::string& name, bool value) {
    if (m_Initialized) {
        m_LuaState[name] = value;
    }
}

int LuaEnvironment::GetGlobalInt(const std::string& name, int defaultValue) {
    if (!m_Initialized) {
        return defaultValue;
    }

    try {
        sol::object obj = m_LuaState[name];
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
        sol::object obj = m_LuaState[name];
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
        sol::object obj = m_LuaState[name];
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
        sol::object obj = m_LuaState[name];
        if (obj.is<bool>()) {
            return obj.as<bool>();
        }
        return defaultValue;
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

        m_LuaState["lupine_api"] = api;
    }
}

void LuaEnvironment::RegisterScriptAPI() {
    if (!m_Initialized) return;

    auto lupine = m_LuaState["Lupine"].get_or_create<sol::table>();

    lupine["log_info"] = [](const std::string& msg) {

    };
    lupine["log_warning"] = [](const std::string& msg) {

    };
    lupine["log_error"] = [](const std::string& msg) {

    };
    lupine["log_debug"] = [](const std::string& msg) {

    };

    lupine["is_action_pressed"] = [this](const std::string& action) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsActionPressed(action) : false;
    };
    lupine["is_action_just_pressed"] = [this](const std::string& action) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsActionJustPressed(action) : false;
    };
    lupine["is_action_just_released"] = [this](const std::string& action) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsActionJustReleased(action) : false;
    };
    lupine["get_axis"] = [this](const std::string& axis) -> float {
        return m_ScriptAPI ? m_ScriptAPI->GetAxis(axis) : 0.0f;
    };
    lupine["is_key_pressed"] = [this](int keyCode) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsKeyPressed(keyCode) : false;
    };
    lupine["is_mouse_button_pressed"] = [this](int button) -> bool {
        return m_ScriptAPI ? m_ScriptAPI->IsMouseButtonPressed(button) : false;
    };

    lupine["get_delta_time"] = [this]() -> float {
        return m_ScriptAPI ? m_ScriptAPI->GetDeltaTime() : 0.0f;
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
}

}
}
