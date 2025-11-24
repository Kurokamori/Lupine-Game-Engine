#include "lupine/scripting/PythonEnvironment.hpp"
#include "lupine/scripting/ScriptAPI.hpp"
#include "lupine/logger/Logger.hpp"
#include <fstream>
#include <sstream>

namespace lupine {
namespace scripting {

static ScriptAPI* GetScriptAPI(py::dict& ns) {
    if (ns.contains("__lupine_api__")) {
        try {
            return ns["__lupine_api__"].cast<ScriptAPI*>();
        }
        catch (...) {
            return nullptr;
        }
    }
    return nullptr;
}

PythonEnvironment::PythonEnvironment() {
}

PythonEnvironment::~PythonEnvironment() {
    Shutdown();
}

bool PythonEnvironment::Initialize() {
    if (m_Initialized) {
        return true;
    }

    try {

        m_Namespace = py::dict();

        py::module_ builtins = py::module_::import("builtins");
        m_Namespace["__builtins__"] = builtins;
        m_Namespace["__name__"] = "__main__";

        try {
            py::module_ sys = py::module_::import("sys");
            m_Namespace["sys"] = sys;

            if (py::hasattr(sys, "stdout") && !sys.attr("stdout").is_none()) {
                m_Namespace["__stdout__"] = sys.attr("stdout");
            }
            if (py::hasattr(sys, "stderr") && !sys.attr("stderr").is_none()) {
                m_Namespace["__stderr__"] = sys.attr("stderr");
            }
            if (py::hasattr(sys, "stdin") && !sys.attr("stdin").is_none()) {
                m_Namespace["__stdin__"] = sys.attr("stdin");
            }
        }
        catch (...) {

        }

        RegisterScriptAPI();

        m_Initialized = true;

        return true;
    }
    catch (const std::exception& e) {

        m_LastError = e.what();
        return false;
    }
}

void PythonEnvironment::Shutdown() {
    if (!m_Initialized) {
        return;
    }

    try {
        m_Namespace.clear();
        m_Initialized = false;

    }
    catch (const std::exception& e) {

    }
}

ScriptResult PythonEnvironment::ExecuteFile(const std::string& filepath) {
    if (!m_Initialized) {
        return ScriptResult(false, "Python environment not initialized");
    }

    try {

        std::ifstream file(filepath);
        if (!file.is_open()) {
            return ScriptResult(false, "Failed to open file: " + filepath);
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string script = buffer.str();

        py::exec(script, m_Namespace);

        return ScriptResult(true);
    }
    catch (const py::error_already_set& e) {
        return HandlePythonError();
    }
    catch (const std::exception& e) {
        return ScriptResult(false, std::string("Error: ") + e.what());
    }
}

ScriptResult PythonEnvironment::ExecuteString(const std::string& script) {
    if (!m_Initialized) {
        return ScriptResult(false, "Python environment not initialized");
    }

    try {
        py::exec(script, m_Namespace);
        return ScriptResult(true);
    }
    catch (const py::error_already_set& e) {
        return HandlePythonError();
    }
    catch (const std::exception& e) {
        return ScriptResult(false, std::string("Error: ") + e.what());
    }
}

ScriptResult PythonEnvironment::CallFunction(const std::string& functionName) {
    if (!m_Initialized) {
        return ScriptResult(false, "Python environment not initialized");
    }

    try {
        if (!HasFunction(functionName)) {
            return ScriptResult(false, "Function '" + functionName + "' not found");
        }

        py::object func = m_Namespace[functionName.c_str()];
        func();

        return ScriptResult(true);
    }
    catch (const py::error_already_set& e) {
        return HandlePythonError();
    }
    catch (const std::exception& e) {
        return ScriptResult(false, std::string("Error: ") + e.what());
    }
}

bool PythonEnvironment::HasFunction(const std::string& functionName) const {
    if (!m_Initialized) {
        return false;
    }

    try {
        return m_Namespace.contains(functionName.c_str()) &&
               py::isinstance<py::function>(m_Namespace[functionName.c_str()]);
    }
    catch (...) {
        return false;
    }
}

void PythonEnvironment::SetGlobal(const std::string& name, int value) {
    if (m_Initialized) {
        m_Namespace[name.c_str()] = value;
    }
}

void PythonEnvironment::SetGlobal(const std::string& name, float value) {
    if (m_Initialized) {
        m_Namespace[name.c_str()] = value;
    }
}

void PythonEnvironment::SetGlobal(const std::string& name, const std::string& value) {
    if (m_Initialized) {
        m_Namespace[name.c_str()] = value;
    }
}

void PythonEnvironment::SetGlobal(const std::string& name, bool value) {
    if (m_Initialized) {
        m_Namespace[name.c_str()] = value;
    }
}

int PythonEnvironment::GetGlobalInt(const std::string& name, int defaultValue) {
    if (!m_Initialized || !m_Namespace.contains(name.c_str())) {
        return defaultValue;
    }

    try {
        return m_Namespace[name.c_str()].cast<int>();
    }
    catch (...) {
        return defaultValue;
    }
}

float PythonEnvironment::GetGlobalFloat(const std::string& name, float defaultValue) {
    if (!m_Initialized || !m_Namespace.contains(name.c_str())) {
        return defaultValue;
    }

    try {
        return m_Namespace[name.c_str()].cast<float>();
    }
    catch (...) {
        return defaultValue;
    }
}

std::string PythonEnvironment::GetGlobalString(const std::string& name, const std::string& defaultValue) {
    if (!m_Initialized || !m_Namespace.contains(name.c_str())) {
        return defaultValue;
    }

    try {
        return m_Namespace[name.c_str()].cast<std::string>();
    }
    catch (...) {
        return defaultValue;
    }
}

bool PythonEnvironment::GetGlobalBool(const std::string& name, bool defaultValue) {
    if (!m_Initialized || !m_Namespace.contains(name.c_str())) {
        return defaultValue;
    }

    try {
        return m_Namespace[name.c_str()].cast<bool>();
    }
    catch (...) {
        return defaultValue;
    }
}

ScriptResult PythonEnvironment::HandlePythonError() {
    try {

        try {
            throw;
        }
        catch (const py::error_already_set& e) {
            m_LastError = e.what();

            return ScriptResult(false, m_LastError);
        }
        catch (...) {

        }

        if (PyErr_Occurred()) {
            PyObject *ptype, *pvalue, *ptraceback;
            PyErr_Fetch(&ptype, &pvalue, &ptraceback);

            if (pvalue != nullptr) {
                PyObject* pstr = PyObject_Str(pvalue);
                if (pstr != nullptr) {
                    const char* error_str = PyUnicode_AsUTF8(pstr);
                    if (error_str != nullptr) {
                        m_LastError = error_str;
                    }
                    Py_XDECREF(pstr);
                }
            }

            Py_XDECREF(ptype);
            Py_XDECREF(pvalue);
            Py_XDECREF(ptraceback);
        }

        if (m_LastError.empty()) {
            m_LastError = "Unknown Python error";
        }

        return ScriptResult(false, m_LastError);
    }
    catch (...) {
        return ScriptResult(false, "Failed to retrieve Python error information");
    }
}

void PythonEnvironment::SetScriptAPI(ScriptAPI* api) {
    if (m_Initialized && api) {

        m_Namespace["__lupine_api__"] = py::cast(api);
    }
}

ScriptAPI* PythonEnvironment::GetScriptAPI() const {
    if (!m_Initialized) {
        return nullptr;
    }

    try {
        if (m_Namespace.contains("__lupine_api__")) {
            return m_Namespace["__lupine_api__"].cast<ScriptAPI*>();
        }
    }
    catch (...) {
    }

    return nullptr;
}

void PythonEnvironment::RegisterScriptAPI() {
    if (!m_Initialized) return;

    try {

        py::class_<ScriptAPI> lupine_module(m_Namespace, "Lupine");

        lupine_module.def_static("log_info", [this](const std::string& message) {
            ScriptAPI* api = ::lupine::scripting::GetScriptAPI(m_Namespace);
            if (api) api->LogInfo(message);
        });

        lupine_module.def_static("log_warning", [this](const std::string& message) {
            ScriptAPI* api = ::lupine::scripting::GetScriptAPI(m_Namespace);
            if (api) api->LogWarning(message);
        });

        lupine_module.def_static("log_error", [this](const std::string& message) {
            ScriptAPI* api = ::lupine::scripting::GetScriptAPI(m_Namespace);
            if (api) api->LogError(message);
        });

        lupine_module.def_static("log_debug", [this](const std::string& message) {
            ScriptAPI* api = ::lupine::scripting::GetScriptAPI(m_Namespace);
            if (api) api->LogDebug(message);
        });

        lupine_module.def_static("is_action_pressed", [this](const std::string& action) -> bool {
            ScriptAPI* api = ::lupine::scripting::GetScriptAPI(m_Namespace);
            return api ? api->IsActionPressed(action) : false;
        });

        lupine_module.def_static("is_action_just_pressed", [this](const std::string& action) -> bool {
            ScriptAPI* api = ::lupine::scripting::GetScriptAPI(m_Namespace);
            return api ? api->IsActionJustPressed(action) : false;
        });

        lupine_module.def_static("is_action_just_released", [this](const std::string& action) -> bool {
            ScriptAPI* api = ::lupine::scripting::GetScriptAPI(m_Namespace);
            return api ? api->IsActionJustReleased(action) : false;
        });

        lupine_module.def_static("get_axis", [this](const std::string& axis) -> float {
            ScriptAPI* api = ::lupine::scripting::GetScriptAPI(m_Namespace);
            return api ? api->GetAxis(axis) : 0.0f;
        });

        lupine_module.def_static("is_key_pressed", [this](int keyCode) -> bool {
            ScriptAPI* api = ::lupine::scripting::GetScriptAPI(m_Namespace);
            return api ? api->IsKeyPressed(keyCode) : false;
        });

        lupine_module.def_static("is_mouse_button_pressed", [this](int button) -> bool {
            ScriptAPI* api = ::lupine::scripting::GetScriptAPI(m_Namespace);
            return api ? api->IsMouseButtonPressed(button) : false;
        });

        lupine_module.def_static("get_delta_time", [this]() -> float {
            ScriptAPI* api = ::lupine::scripting::GetScriptAPI(m_Namespace);
            return api ? api->GetDeltaTime() : 0.0f;
        });

        lupine_module.def_static("random_range", [this](float min, float max) -> float {
            ScriptAPI* api = ::lupine::scripting::GetScriptAPI(m_Namespace);
            return api ? api->RandomRange(min, max) : 0.0f;
        });

        lupine_module.def_static("random_range_int", [this](int min, int max) -> int {
            ScriptAPI* api = ::lupine::scripting::GetScriptAPI(m_Namespace);
            return api ? api->RandomRangeInt(min, max) : 0;
        });

        lupine_module.def_static("lerp", [this](float a, float b, float t) -> float {
            ScriptAPI* api = ::lupine::scripting::GetScriptAPI(m_Namespace);
            return api ? api->Lerp(a, b, t) : 0.0f;
        });

        lupine_module.def_static("clamp", [this](float value, float min, float max) -> float {
            ScriptAPI* api = ::lupine::scripting::GetScriptAPI(m_Namespace);
            return api ? api->Clamp(value, min, max) : value;
        });

        lupine_module.def_static("get_child_count", [this]() -> int {
            ScriptAPI* api = ::lupine::scripting::GetScriptAPI(m_Namespace);
            return api ? api->GetChildCount() : 0;
        });

        lupine_module.def_static("queue_free_self", [this]() {
            ScriptAPI* api = ::lupine::scripting::GetScriptAPI(m_Namespace);
            if (api) api->QueueFreeSelf();
        });

        lupine_module.def_static("set_bus_volume", [this](const std::string& busName, float volume) {
            ScriptAPI* api = ::lupine::scripting::GetScriptAPI(m_Namespace);
            if (api) api->SetBusVolume(busName, volume);
        });

        lupine_module.def_static("get_bus_volume", [this](const std::string& busName) -> float {
            ScriptAPI* api = ::lupine::scripting::GetScriptAPI(m_Namespace);
            return api ? api->GetBusVolume(busName) : 0.0f;
        });

        lupine_module.def_static("set_bus_muted", [this](const std::string& busName, bool muted) {
            ScriptAPI* api = ::lupine::scripting::GetScriptAPI(m_Namespace);
            if (api) api->SetBusMuted(busName, muted);
        });

        lupine_module.def_static("get_global_int", [this](const std::string& name, int defaultValue = 0) -> int {
            ScriptAPI* api = ::lupine::scripting::GetScriptAPI(m_Namespace);
            return api ? api->GetGlobalInt(name, defaultValue) : defaultValue;
        }, py::arg("name"), py::arg("default_value") = 0);

        lupine_module.def_static("get_global_float", [this](const std::string& name, float defaultValue = 0.0f) -> float {
            ScriptAPI* api = ::lupine::scripting::GetScriptAPI(m_Namespace);
            return api ? api->GetGlobalFloat(name, defaultValue) : defaultValue;
        }, py::arg("name"), py::arg("default_value") = 0.0f);

        lupine_module.def_static("get_global_string", [this](const std::string& name, const std::string& defaultValue = "") -> std::string {
            ScriptAPI* api = ::lupine::scripting::GetScriptAPI(m_Namespace);
            return api ? api->GetGlobalString(name, defaultValue) : defaultValue;
        }, py::arg("name"), py::arg("default_value") = "");

        lupine_module.def_static("get_global_bool", [this](const std::string& name, bool defaultValue = false) -> bool {
            ScriptAPI* api = ::lupine::scripting::GetScriptAPI(m_Namespace);
            return api ? api->GetGlobalBool(name, defaultValue) : defaultValue;
        }, py::arg("name"), py::arg("default_value") = false);

        lupine_module.def_static("set_global_int", [this](const std::string& name, int value) {
            ScriptAPI* api = ::lupine::scripting::GetScriptAPI(m_Namespace);
            if (api) api->SetGlobalInt(name, value);
        });

        lupine_module.def_static("set_global_float", [this](const std::string& name, float value) {
            ScriptAPI* api = ::lupine::scripting::GetScriptAPI(m_Namespace);
            if (api) api->SetGlobalFloat(name, value);
        });

        lupine_module.def_static("set_global_string", [this](const std::string& name, const std::string& value) {
            ScriptAPI* api = ::lupine::scripting::GetScriptAPI(m_Namespace);
            if (api) api->SetGlobalString(name, value);
        });

        lupine_module.def_static("set_global_bool", [this](const std::string& name, bool value) {
            ScriptAPI* api = ::lupine::scripting::GetScriptAPI(m_Namespace);
            if (api) api->SetGlobalBool(name, value);
        });
    }
    catch (const std::exception& e) {

    }
}

}
}
