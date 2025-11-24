#include "lupine/scripting/MRubyEnvironment.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/core/Node.hpp"
#include <fstream>
#include <sstream>

namespace lupine {
namespace scripting {

#ifdef LUPINE_HAS_MRUBY

static ScriptAPI* GetScriptAPIFromState(mrb_state* mrb) {
    mrb_value api_obj = mrb_gv_get(mrb, mrb_intern_lit(mrb, "$lupine_api"));
    if (mrb_nil_p(api_obj)) {
        return nullptr;
    }

    ScriptAPI** api_ptr = (ScriptAPI**)DATA_PTR(api_obj);
    return api_ptr ? *api_ptr : nullptr;
}

static mrb_value mrb_script_api_log_info(mrb_state* mrb, mrb_value self) {
    char* message;
    mrb_get_args(mrb, "z", &message);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->LogInfo(message);
    }

    return mrb_nil_value();
}

static mrb_value mrb_script_api_log_warning(mrb_state* mrb, mrb_value self) {
    char* message;
    mrb_get_args(mrb, "z", &message);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->LogWarning(message);
    }

    return mrb_nil_value();
}

static mrb_value mrb_script_api_log_error(mrb_state* mrb, mrb_value self) {
    char* message;
    mrb_get_args(mrb, "z", &message);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->LogError(message);
    }

    return mrb_nil_value();
}

static mrb_value mrb_script_api_is_action_pressed(mrb_state* mrb, mrb_value self) {
    char* action;
    mrb_get_args(mrb, "z", &action);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return api->IsActionPressed(action) ? mrb_true_value() : mrb_false_value();
    }

    return mrb_false_value();
}

static mrb_value mrb_script_api_is_action_just_pressed(mrb_state* mrb, mrb_value self) {
    char* action;
    mrb_get_args(mrb, "z", &action);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return api->IsActionJustPressed(action) ? mrb_true_value() : mrb_false_value();
    }

    return mrb_false_value();
}

static mrb_value mrb_script_api_get_axis(mrb_state* mrb, mrb_value self) {
    char* axis;
    mrb_get_args(mrb, "z", &axis);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return mrb_float_value(mrb, api->GetAxis(axis));
    }

    return mrb_float_value(mrb, 0.0f);
}

static mrb_value mrb_script_api_get_delta_time(mrb_state* mrb, mrb_value self) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return mrb_float_value(mrb, api->GetDeltaTime());
    }

    return mrb_float_value(mrb, 0.0f);
}

static mrb_value mrb_script_api_random_range(mrb_state* mrb, mrb_value self) {
    mrb_float min, max;
    mrb_get_args(mrb, "ff", &min, &max);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return mrb_float_value(mrb, api->RandomRange(static_cast<float>(min), static_cast<float>(max)));
    }

    return mrb_float_value(mrb, 0.0f);
}

static mrb_value mrb_script_api_get_child_count(mrb_state* mrb, mrb_value self) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return mrb_fixnum_value(api->GetChildCount());
    }

    return mrb_fixnum_value(0);
}

static mrb_value mrb_script_api_set_bus_volume(mrb_state* mrb, mrb_value self) {
    char* busName;
    mrb_float volume;
    mrb_get_args(mrb, "zf", &busName, &volume);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetBusVolume(busName, static_cast<float>(volume));
    }

    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_bus_volume(mrb_state* mrb, mrb_value self) {
    char* busName;
    mrb_get_args(mrb, "z", &busName);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return mrb_float_value(mrb, api->GetBusVolume(busName));
    }

    return mrb_float_value(mrb, 0.0f);
}

static mrb_value mrb_script_api_get_global_int(mrb_state* mrb, mrb_value self) {
    char* name;
    mrb_int defaultValue = 0;
    mrb_get_args(mrb, "z|i", &name, &defaultValue);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return mrb_fixnum_value(api->GetGlobalInt(name, static_cast<int>(defaultValue)));
    }

    return mrb_fixnum_value(defaultValue);
}

static mrb_value mrb_script_api_get_global_float(mrb_state* mrb, mrb_value self) {
    char* name;
    mrb_float defaultValue = 0.0f;
    mrb_get_args(mrb, "z|f", &name, &defaultValue);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return mrb_float_value(mrb, api->GetGlobalFloat(name, static_cast<float>(defaultValue)));
    }

    return mrb_float_value(mrb, defaultValue);
}

static mrb_value mrb_script_api_get_global_string(mrb_state* mrb, mrb_value self) {
    char* name;
    char* defaultValue = (char*)"";
    mrb_get_args(mrb, "z|z", &name, &defaultValue);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        std::string result = api->GetGlobalString(name, defaultValue);
        return mrb_str_new_cstr(mrb, result.c_str());
    }

    return mrb_str_new_cstr(mrb, defaultValue);
}

static mrb_value mrb_script_api_get_global_bool(mrb_state* mrb, mrb_value self) {
    char* name;
    mrb_bool defaultValue = false;
    mrb_get_args(mrb, "z|b", &name, &defaultValue);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return api->GetGlobalBool(name, defaultValue) ? mrb_true_value() : mrb_false_value();
    }

    return defaultValue ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_set_global_int(mrb_state* mrb, mrb_value self) {
    char* name;
    mrb_int value;
    mrb_get_args(mrb, "zi", &name, &value);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetGlobalInt(name, static_cast<int>(value));
    }

    return mrb_nil_value();
}

static mrb_value mrb_script_api_set_global_float(mrb_state* mrb, mrb_value self) {
    char* name;
    mrb_float value;
    mrb_get_args(mrb, "zf", &name, &value);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetGlobalFloat(name, static_cast<float>(value));
    }

    return mrb_nil_value();
}

static mrb_value mrb_script_api_set_global_string(mrb_state* mrb, mrb_value self) {
    char* name;
    char* value;
    mrb_get_args(mrb, "zz", &name, &value);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetGlobalString(name, value);
    }

    return mrb_nil_value();
}

static mrb_value mrb_script_api_set_global_bool(mrb_state* mrb, mrb_value self) {
    char* name;
    mrb_bool value;
    mrb_get_args(mrb, "zb", &name, &value);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetGlobalBool(name, value);
    }

    return mrb_nil_value();
}

MRubyEnvironment::MRubyEnvironment() {
}

MRubyEnvironment::~MRubyEnvironment() {
    Shutdown();
}

bool MRubyEnvironment::Initialize() {
    if (m_Initialized) {
        return true;
    }

    m_MRubyState = mrb_open();
    if (!m_MRubyState) {

        return false;
    }

    RegisterScriptAPI();

    m_Initialized = true;
    return true;
}

void MRubyEnvironment::Shutdown() {
    if (!m_Initialized) {
        return;
    }

    if (m_MRubyState) {
        mrb_close(m_MRubyState);
        m_MRubyState = nullptr;
    }

    m_Initialized = false;
}

ScriptResult MRubyEnvironment::ExecuteFile(const std::string& filepath) {
    if (!m_Initialized || !m_MRubyState) {
        return ScriptResult(false, "mRuby environment not initialized");
    }

    std::ifstream file(filepath);
    if (!file.is_open()) {
        return ScriptResult(false, "Failed to open file: " + filepath);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string script = buffer.str();
    file.close();

    return ExecuteString(script);
}

ScriptResult MRubyEnvironment::ExecuteString(const std::string& script) {
    if (!m_Initialized || !m_MRubyState) {
        return ScriptResult(false, "mRuby environment not initialized");
    }

    mrbc_context* context = mrbc_context_new(m_MRubyState);
    mrb_load_string_cxt(m_MRubyState, script.c_str(), context);
    mrbc_context_free(m_MRubyState, context);

    if (m_MRubyState->exc) {
        return HandleMRubyError();
    }

    return ScriptResult(true);
}

ScriptResult MRubyEnvironment::CallFunction(const std::string& functionName) {
    if (!m_Initialized || !m_MRubyState) {
        return ScriptResult(false, "mRuby environment not initialized");
    }

    if (!HasFunction(functionName)) {
        return ScriptResult(false, "Function '" + functionName + "' not found");
    }

    mrb_sym sym = mrb_intern_cstr(m_MRubyState, functionName.c_str());
    mrb_value func = mrb_gv_get(m_MRubyState, sym);

    mrb_funcall_argv(m_MRubyState, func, mrb_intern_lit(m_MRubyState, "call"), 0, nullptr);

    if (m_MRubyState->exc) {
        return HandleMRubyError();
    }

    return ScriptResult(true);
}

bool MRubyEnvironment::HasFunction(const std::string& functionName) const {
    if (!m_Initialized || !m_MRubyState) {
        return false;
    }

    mrb_sym sym = mrb_intern_cstr(m_MRubyState, functionName.c_str());
    mrb_value func = mrb_gv_get(m_MRubyState, sym);

    return !mrb_nil_p(func);
}

void MRubyEnvironment::SetGlobal(const std::string& name, int value) {
    if (!m_Initialized || !m_MRubyState) return;

    mrb_sym sym = mrb_intern_cstr(m_MRubyState, name.c_str());
    mrb_gv_set(m_MRubyState, sym, mrb_fixnum_value(value));
}

void MRubyEnvironment::SetGlobal(const std::string& name, float value) {
    if (!m_Initialized || !m_MRubyState) return;

    mrb_sym sym = mrb_intern_cstr(m_MRubyState, name.c_str());
    mrb_gv_set(m_MRubyState, sym, mrb_float_value(m_MRubyState, value));
}

void MRubyEnvironment::SetGlobal(const std::string& name, const std::string& value) {
    if (!m_Initialized || !m_MRubyState) return;

    mrb_sym sym = mrb_intern_cstr(m_MRubyState, name.c_str());
    mrb_gv_set(m_MRubyState, sym, mrb_str_new_cstr(m_MRubyState, value.c_str()));
}

void MRubyEnvironment::SetGlobal(const std::string& name, bool value) {
    if (!m_Initialized || !m_MRubyState) return;

    mrb_sym sym = mrb_intern_cstr(m_MRubyState, name.c_str());
    mrb_gv_set(m_MRubyState, sym, value ? mrb_true_value() : mrb_false_value());
}

int MRubyEnvironment::GetGlobalInt(const std::string& name, int defaultValue) {
    if (!m_Initialized || !m_MRubyState) return defaultValue;

    mrb_sym sym = mrb_intern_cstr(m_MRubyState, name.c_str());
    mrb_value val = mrb_gv_get(m_MRubyState, sym);

    if (mrb_nil_p(val)) return defaultValue;

    return mrb_fixnum(val);
}

float MRubyEnvironment::GetGlobalFloat(const std::string& name, float defaultValue) {
    if (!m_Initialized || !m_MRubyState) return defaultValue;

    mrb_sym sym = mrb_intern_cstr(m_MRubyState, name.c_str());
    mrb_value val = mrb_gv_get(m_MRubyState, sym);

    if (mrb_nil_p(val)) return defaultValue;

    if (mrb_float_p(val)) {
        return static_cast<float>(mrb_float(val));
    } else if (mrb_fixnum_p(val)) {
        return static_cast<float>(mrb_fixnum(val));
    }

    return defaultValue;
}

std::string MRubyEnvironment::GetGlobalString(const std::string& name, const std::string& defaultValue) {
    if (!m_Initialized || !m_MRubyState) return defaultValue;

    mrb_sym sym = mrb_intern_cstr(m_MRubyState, name.c_str());
    mrb_value val = mrb_gv_get(m_MRubyState, sym);

    if (mrb_nil_p(val)) return defaultValue;

    if (mrb_string_p(val)) {
        return std::string(RSTRING_PTR(val), RSTRING_LEN(val));
    }

    return defaultValue;
}

bool MRubyEnvironment::GetGlobalBool(const std::string& name, bool defaultValue) {
    if (!m_Initialized || !m_MRubyState) return defaultValue;

    mrb_sym sym = mrb_intern_cstr(m_MRubyState, name.c_str());
    mrb_value val = mrb_gv_get(m_MRubyState, sym);

    if (mrb_nil_p(val)) return defaultValue;

    return mrb_test(val);
}

void MRubyEnvironment::SetScriptAPI(ScriptAPI* api) {
    m_ScriptAPI = api;

    if (m_Initialized && m_MRubyState && api) {

        mrb_value api_obj = mrb_obj_value(Data_Wrap_Struct(m_MRubyState, m_MRubyState->object_class,
                                                            nullptr, &m_ScriptAPI));
        mrb_gv_set(m_MRubyState, mrb_intern_lit(m_MRubyState, "$lupine_api"), api_obj);
    }
}

ScriptResult MRubyEnvironment::HandleMRubyError() {
    if (!m_MRubyState || !m_MRubyState->exc) {
        return ScriptResult(false, "Unknown mRuby error");
    }

    mrb_value exc = mrb_obj_value(m_MRubyState->exc);
    mrb_value msg = mrb_funcall(m_MRubyState, exc, "to_s", 0);

    std::string error = "mRuby error: ";
    if (mrb_string_p(msg)) {
        error += std::string(RSTRING_PTR(msg), RSTRING_LEN(msg));
    } else {
        error += "Unknown error";
    }

    m_LastError = error;
    m_MRubyState->exc = nullptr;

    return ScriptResult(false, error);
}

void MRubyEnvironment::RegisterScriptAPI() {
    if (!m_MRubyState) return;

    struct RClass* lupine_module = mrb_define_module(m_MRubyState, "Lupine");

    mrb_define_module_function(m_MRubyState, lupine_module, "log_info", mrb_script_api_log_info, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "log_warning", mrb_script_api_log_warning, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "log_error", mrb_script_api_log_error, MRB_ARGS_REQ(1));

    mrb_define_module_function(m_MRubyState, lupine_module, "is_action_pressed", mrb_script_api_is_action_pressed, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "is_action_just_pressed", mrb_script_api_is_action_just_pressed, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_axis", mrb_script_api_get_axis, MRB_ARGS_REQ(1));

    mrb_define_module_function(m_MRubyState, lupine_module, "get_delta_time", mrb_script_api_get_delta_time, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "random_range", mrb_script_api_random_range, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_child_count", mrb_script_api_get_child_count, MRB_ARGS_NONE());

    mrb_define_module_function(m_MRubyState, lupine_module, "set_bus_volume", mrb_script_api_set_bus_volume, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_bus_volume", mrb_script_api_get_bus_volume, MRB_ARGS_REQ(1));

    mrb_define_module_function(m_MRubyState, lupine_module, "get_global_int", mrb_script_api_get_global_int, MRB_ARGS_ARG(1, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_global_float", mrb_script_api_get_global_float, MRB_ARGS_ARG(1, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_global_string", mrb_script_api_get_global_string, MRB_ARGS_ARG(1, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_global_bool", mrb_script_api_get_global_bool, MRB_ARGS_ARG(1, 1));

    mrb_define_module_function(m_MRubyState, lupine_module, "set_global_int", mrb_script_api_set_global_int, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "set_global_float", mrb_script_api_set_global_float, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "set_global_string", mrb_script_api_set_global_string, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "set_global_bool", mrb_script_api_set_global_bool, MRB_ARGS_REQ(2));
}

#endif

}
}

