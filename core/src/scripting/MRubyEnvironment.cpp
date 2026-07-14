#include "lupine/scripting/MRubyEnvironment.hpp"
#include "lupine/scripting/ScriptAPI.hpp"
#include "lupine/scripting/NodeRef.hpp"
#include "lupine/core/Component.hpp"
#include "lupine/core/CustomComponentRegistry.hpp"
#include "lupine/network/NetworkManager.hpp"
#include "lupine/profiling/Profiler.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/platform/PackFile.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/CameraNodes.hpp"
#include "lupine/core/ArchetypeRuntime.hpp"
#include "lupine/asset/ArchetypeInstance.hpp"
#include "lupine/components/UIControl.hpp"
#include "lupine/components/Particles2D.hpp"
#include "lupine/components/Particles3D.hpp"
#include <fstream>
#include <sstream>
#include <vector>
#include <cstdio>
#include <nlohmann/json.hpp>

namespace lupine {
namespace scripting {

#ifdef LUPINE_HAS_MRUBY

// The active API lives in the host, never in a script-writable global: a global would
// let any script (`$lupine_api = 42`) redirect every native binding at a bogus pointer.
static ScriptAPI* GetScriptAPIFromState(mrb_state* mrb) {
    (void)mrb;
    return MRubyHost::Instance().ActiveApi();
}

// Takes ownership of a pending exception: `mrb->exc` is cleared *before* `to_s` runs,
// because a Ruby-defined `to_s` executing with an exception still pending re-raises on
// its first send and destroys the original diagnostic.
static std::string TakeMRubyError(mrb_state* mrb) {
    if (!mrb || !mrb->exc) {
        return std::string("Unknown mRuby error");
    }

    const int arena = mrb_gc_arena_save(mrb);

    mrb_value exc = mrb_obj_value(mrb->exc);
    mrb->exc = nullptr;
    mrb_gc_protect(mrb, exc);

    std::string message;
    mrb_value text = mrb_funcall(mrb, exc, "to_s", 0);
    if (mrb->exc) {
        mrb->exc = nullptr;
        message = "Unknown error (exception raised while formatting)";
    } else if (mrb_string_p(text)) {
        message.assign(RSTRING_PTR(text), static_cast<size_t>(RSTRING_LEN(text)));
    } else {
        message = "Unknown error";
    }

    mrb_gc_arena_restore(mrb, arena);
    return message;
}

// mrb_float is a double (unless mruby is built with MRB_USE_FLOAT), while every engine
// math type is float-based. These adapters keep that narrowing explicit and in one place.
static inline float mrbF(mrb_float v) { return static_cast<float>(v); }
static inline math::Vec2 mrbVec2(mrb_float x, mrb_float y) {
    return math::Vec2(mrbF(x), mrbF(y));
}
static inline math::Vec3 mrbVec3(mrb_float x, mrb_float y, mrb_float z) {
    return math::Vec3(mrbF(x), mrbF(y), mrbF(z));
}
static inline math::Color mrbColor(mrb_float r, mrb_float g, mrb_float b, mrb_float a) {
    return math::Color(mrbF(r), mrbF(g), mrbF(b), mrbF(a));
}

// Defined further below alongside the node object-model bindings.
static mrb_value WrapNodeRef(mrb_state* mrb, const NodeRef& ref);
static mrb_value WrapComponentRef(mrb_state* mrb, const ComponentRef& ref);
static mrb_value WrapTweenRef(mrb_state* mrb, const TweenRef& ref);
static mrb_value WrapSignalAwaiter(mrb_state* mrb, const SignalAwaiter& ref);
static mrb_value WrapSequenceRef(mrb_state* mrb, const SequenceRef& ref);

static mrb_value mrb_script_api_log_info(mrb_state* mrb, mrb_value) {
    char* message;
    mrb_get_args(mrb, "z", &message);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->LogInfo(message);
    }

    return mrb_nil_value();
}

static mrb_value mrb_script_api_log_warning(mrb_state* mrb, mrb_value) {
    char* message;
    mrb_get_args(mrb, "z", &message);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->LogWarning(message);
    }

    return mrb_nil_value();
}

static mrb_value mrb_script_api_log_error(mrb_state* mrb, mrb_value) {
    char* message;
    mrb_get_args(mrb, "z", &message);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->LogError(message);
    }

    return mrb_nil_value();
}

static mrb_value mrb_script_api_is_action_pressed(mrb_state* mrb, mrb_value) {
    char* action; mrb_int player = -1;
    mrb_get_args(mrb, "z|i", &action, &player);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return api->IsActionPressed(action, static_cast<int>(player)) ? mrb_true_value() : mrb_false_value();
    }

    return mrb_false_value();
}

static mrb_value mrb_script_api_is_action_just_pressed(mrb_state* mrb, mrb_value) {
    char* action; mrb_int player = -1;
    mrb_get_args(mrb, "z|i", &action, &player);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return api->IsActionJustPressed(action, static_cast<int>(player)) ? mrb_true_value() : mrb_false_value();
    }

    return mrb_false_value();
}

static mrb_value mrb_script_api_get_axis(mrb_state* mrb, mrb_value) {
    char* axis; mrb_int player = -1;
    mrb_get_args(mrb, "z|i", &axis, &player);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return mrb_float_value(mrb, api->GetAxis(axis, static_cast<int>(player)));
    }

    return mrb_float_value(mrb, 0.0f);
}

static mrb_value mrb_script_api_get_delta_time(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return mrb_float_value(mrb, api->GetDeltaTime());
    }

    return mrb_float_value(mrb, 0.0f);
}

static mrb_value mrb_script_api_get_time(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return mrb_float_value(mrb, api->GetTime());
    }
    return mrb_float_value(mrb, 0.0f);
}

static mrb_value mrb_script_api_get_frame_count(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return mrb_fixnum_value(api->GetFrameCount());
    }
    return mrb_fixnum_value(0);
}

static mrb_value mrb_script_api_set_time_scale(mrb_state* mrb, mrb_value) {
    mrb_float timeScale;
    mrb_get_args(mrb, "f", &timeScale);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetTimeScale(static_cast<float>(timeScale));
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_time_scale(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->GetTimeScale() : 1.0f);
}

static mrb_value mrb_script_api_get_fps(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->GetFPS() : 0.0f);
}

static ::lupine::profiling::ZoneCategory MRubyZoneCategory(const char* category) {
    if (!category) return ::lupine::profiling::ZoneCategory::User;
    std::string c(category);
    if (c == "Update") return ::lupine::profiling::ZoneCategory::Update;
    if (c == "Physics") return ::lupine::profiling::ZoneCategory::Physics;
    if (c == "Render") return ::lupine::profiling::ZoneCategory::Render;
    if (c == "Scripting") return ::lupine::profiling::ZoneCategory::Scripting;
    if (c == "Audio") return ::lupine::profiling::ZoneCategory::Audio;
    return ::lupine::profiling::ZoneCategory::User;
}

static mrb_value mrb_script_api_profiler_begin_zone(mrb_state* mrb, mrb_value) {
    char* name = nullptr;
    char* category = nullptr;
    mrb_int argc = mrb_get_args(mrb, "z|z", &name, &category);
    (void)argc;
    ::lupine::profiling::Profiler::Get().BeginZone(name ? name : "", MRubyZoneCategory(category));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_profiler_end_zone(mrb_state* mrb, mrb_value self) {
    (void)mrb; (void)self;
    ::lupine::profiling::Profiler::Get().EndZone();
    return mrb_nil_value();
}

static mrb_value mrb_script_api_profiler_set_counter(mrb_state* mrb, mrb_value) {
    char* name = nullptr;
    mrb_float value = 0.0;
    mrb_get_args(mrb, "zf", &name, &value);
    ::lupine::profiling::Profiler::Get().SetCounter(name ? name : "", static_cast<double>(value));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_profiler_is_enabled(mrb_state* mrb, mrb_value self) {
    (void)mrb; (void)self;
    return ::lupine::profiling::Profiler::Get().IsEnabled() ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_profiler_set_enabled(mrb_state* mrb, mrb_value) {
    mrb_bool enabled = false;
    mrb_get_args(mrb, "b", &enabled);
    ::lupine::profiling::Profiler::Get().SetEnabled(enabled != 0);
    return mrb_nil_value();
}

static mrb_value mrb_script_api_profiler_frame_ms(mrb_state* mrb, mrb_value self) {
    (void)self;
    return mrb_float_value(mrb, ::lupine::profiling::Profiler::Get().GetAverageFrameMs());
}

static mrb_value mrb_script_api_get_ticks_msec(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_fixnum_value(api ? api->GetTicksMsec() : 0);
}

static mrb_value mrb_script_api_get_unix_time(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->GetUnixTime() : 0.0);
}

static mrb_value mrb_script_api_get_platform_name(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        std::string name = api->GetPlatformName();
        return mrb_str_new_cstr(mrb, name.c_str());
    }
    return mrb_str_new_cstr(mrb, "");
}

static mrb_value mrb_script_api_is_debug_build(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->IsDebugBuild()) ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_get_dpi_scale(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->GetDPIScale() : 1.0f);
}

static mrb_value mrb_script_api_open_url(mrb_state* mrb, mrb_value) {
    char* url;
    mrb_get_args(mrb, "z", &url);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->OpenURL(url)) ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_get_name(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        std::string name = api->GetName();
        return mrb_str_new_cstr(mrb, name.c_str());
    }
    return mrb_str_new_cstr(mrb, "");
}

static mrb_value mrb_script_api_set_name(mrb_state* mrb, mrb_value) {
    char* name;
    mrb_get_args(mrb, "z", &name);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetName(name);
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_is_key_pressed(mrb_state* mrb, mrb_value) {
    mrb_int keyCode;
    mrb_get_args(mrb, "i", &keyCode);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return api->IsKeyPressed(static_cast<int>(keyCode)) ? mrb_true_value() : mrb_false_value();
    }
    return mrb_false_value();
}

static mrb_value mrb_script_api_is_key_just_pressed(mrb_state* mrb, mrb_value) {
    mrb_int keyCode;
    mrb_get_args(mrb, "i", &keyCode);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return api->IsKeyJustPressed(static_cast<int>(keyCode)) ? mrb_true_value() : mrb_false_value();
    }
    return mrb_false_value();
}

static mrb_value mrb_script_api_is_key_just_released(mrb_state* mrb, mrb_value) {
    mrb_int keyCode;
    mrb_get_args(mrb, "i", &keyCode);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return api->IsKeyJustReleased(static_cast<int>(keyCode)) ? mrb_true_value() : mrb_false_value();
    }
    return mrb_false_value();
}

static mrb_value mrb_script_api_is_mouse_button_pressed(mrb_state* mrb, mrb_value) {
    mrb_int button;
    mrb_get_args(mrb, "i", &button);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return api->IsMouseButtonPressed(static_cast<int>(button)) ? mrb_true_value() : mrb_false_value();
    }
    return mrb_false_value();
}

static mrb_value mrb_script_api_is_mouse_button_just_pressed(mrb_state* mrb, mrb_value) {
    mrb_int button;
    mrb_get_args(mrb, "i", &button);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return api->IsMouseButtonJustPressed(static_cast<int>(button)) ? mrb_true_value() : mrb_false_value();
    }
    return mrb_false_value();
}

static mrb_value mrb_script_api_get_mouse_position(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        auto pos = api->GetMousePosition();
        mrb_value result = mrb_ary_new_capa(mrb, 2);
        mrb_ary_push(mrb, result, mrb_float_value(mrb, pos.x));
        mrb_ary_push(mrb, result, mrb_float_value(mrb, pos.y));
        return result;
    }
    mrb_value result = mrb_ary_new_capa(mrb, 2);
    mrb_ary_push(mrb, result, mrb_float_value(mrb, 0.0f));
    mrb_ary_push(mrb, result, mrb_float_value(mrb, 0.0f));
    return result;
}

static mrb_value mrb_script_api_is_active(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return api->IsActive() ? mrb_true_value() : mrb_false_value();
    }
    return mrb_false_value();
}

static mrb_value mrb_script_api_set_active(mrb_state* mrb, mrb_value) {
    mrb_bool active;
    mrb_get_args(mrb, "b", &active);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetActive(active);
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_is_visible(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return api->IsVisible() ? mrb_true_value() : mrb_false_value();
    }
    return mrb_false_value();
}

static mrb_value mrb_script_api_set_visible(mrb_state* mrb, mrb_value) {
    mrb_bool visible;
    mrb_get_args(mrb, "b", &visible);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetVisible(visible);
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_queue_free(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->QueueFreeSelf();
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_change_scene(mrb_state* mrb, mrb_value) {
    char* path;
    mrb_get_args(mrb, "z", &path);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->ChangeScene(path);
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_reload_scene(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->ReloadScene();
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_random_range(mrb_state* mrb, mrb_value) {
    mrb_float min, max;
    mrb_get_args(mrb, "ff", &min, &max);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return mrb_float_value(mrb, api->RandomRange(static_cast<float>(min), static_cast<float>(max)));
    }

    return mrb_float_value(mrb, 0.0f);
}

static mrb_value mrb_script_api_get_child_count(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return mrb_fixnum_value(api->GetChildCount());
    }

    return mrb_fixnum_value(0);
}

static mrb_value mrb_script_api_set_bus_volume(mrb_state* mrb, mrb_value) {
    char* busName;
    mrb_float volume;
    mrb_get_args(mrb, "zf", &busName, &volume);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetBusVolume(busName, static_cast<float>(volume));
    }

    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_bus_volume(mrb_state* mrb, mrb_value) {
    char* busName;
    mrb_get_args(mrb, "z", &busName);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return mrb_float_value(mrb, api->GetBusVolume(busName));
    }

    return mrb_float_value(mrb, 0.0f);
}

static mrb_value mrb_script_api_add_bus_effect(mrb_state* mrb, mrb_value) {
    char* busName; char* effectType;
    mrb_get_args(mrb, "zz", &busName, &effectType);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_fixnum_value(api ? api->AddBusEffect(busName, effectType) : -1);
}

static mrb_value mrb_script_api_remove_bus_effect(mrb_state* mrb, mrb_value) {
    char* busName; mrb_int index;
    mrb_get_args(mrb, "zi", &busName, &index);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->RemoveBusEffect(busName, static_cast<int>(index));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_move_bus_effect(mrb_state* mrb, mrb_value) {
    char* busName; mrb_int fromIndex; mrb_int toIndex;
    mrb_get_args(mrb, "zii", &busName, &fromIndex, &toIndex);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->MoveBusEffect(busName, static_cast<int>(fromIndex), static_cast<int>(toIndex));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_clear_bus_effects(mrb_state* mrb, mrb_value) {
    char* busName;
    mrb_get_args(mrb, "z", &busName);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->ClearBusEffects(busName);
    return mrb_nil_value();
}

static mrb_value mrb_script_api_set_bus_effect_enabled(mrb_state* mrb, mrb_value) {
    char* busName; mrb_int index; mrb_bool enabled;
    mrb_get_args(mrb, "zib", &busName, &index, &enabled);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetBusEffectEnabled(busName, static_cast<int>(index), enabled != 0);
    return mrb_nil_value();
}

static mrb_value mrb_script_api_set_bus_effect_parameter(mrb_state* mrb, mrb_value) {
    char* busName; mrb_int index; char* parameter; mrb_float value;
    mrb_get_args(mrb, "zizf", &busName, &index, &parameter, &value);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetBusEffectParameter(busName, static_cast<int>(index), parameter, static_cast<float>(value));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_bus_effect_count(mrb_state* mrb, mrb_value) {
    char* busName;
    mrb_get_args(mrb, "z", &busName);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_fixnum_value(api ? api->GetBusEffectCount(busName) : 0);
}

static mrb_value mrb_script_api_get_bus_effect_parameter(mrb_state* mrb, mrb_value) {
    char* busName; mrb_int index; char* parameter;
    mrb_get_args(mrb, "ziz", &busName, &index, &parameter);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->GetBusEffectParameter(busName, static_cast<int>(index), parameter) : 0.0f);
}

static mrb_value mrb_script_api_is_bus_effect_enabled(mrb_state* mrb, mrb_value) {
    char* busName; mrb_int index;
    mrb_get_args(mrb, "zi", &busName, &index);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_bool_value(api ? api->IsBusEffectEnabled(busName, static_cast<int>(index)) : false);
}

static mrb_value mrb_script_api_get_bus_level(mrb_state* mrb, mrb_value) {
    char* busName;
    mrb_get_args(mrb, "z", &busName);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->GetBusLevel(busName) : 0.0f);
}

static mrb_value mrb_script_api_get_global_int(mrb_state* mrb, mrb_value) {
    char* name;
    mrb_int defaultValue = 0;
    mrb_get_args(mrb, "z|i", &name, &defaultValue);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return mrb_fixnum_value(api->GetGlobalInt(name, static_cast<int>(defaultValue)));
    }

    return mrb_fixnum_value(defaultValue);
}

static mrb_value mrb_script_api_get_global_float(mrb_state* mrb, mrb_value) {
    char* name;
    mrb_float defaultValue = 0.0f;
    mrb_get_args(mrb, "z|f", &name, &defaultValue);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return mrb_float_value(mrb, api->GetGlobalFloat(name, static_cast<float>(defaultValue)));
    }

    return mrb_float_value(mrb, defaultValue);
}

static mrb_value mrb_script_api_get_global_string(mrb_state* mrb, mrb_value) {
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

static mrb_value mrb_script_api_get_global_bool(mrb_state* mrb, mrb_value) {
    char* name;
    mrb_bool defaultValue = false;
    mrb_get_args(mrb, "z|b", &name, &defaultValue);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return api->GetGlobalBool(name, defaultValue) ? mrb_true_value() : mrb_false_value();
    }

    return defaultValue ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_set_global_int(mrb_state* mrb, mrb_value) {
    char* name;
    mrb_int value;
    mrb_get_args(mrb, "zi", &name, &value);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetGlobalInt(name, static_cast<int>(value));
    }

    return mrb_nil_value();
}

static mrb_value mrb_script_api_set_global_float(mrb_state* mrb, mrb_value) {
    char* name;
    mrb_float value;
    mrb_get_args(mrb, "zf", &name, &value);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetGlobalFloat(name, static_cast<float>(value));
    }

    return mrb_nil_value();
}

static mrb_value mrb_script_api_set_global_string(mrb_state* mrb, mrb_value) {
    char* name;
    char* value;
    mrb_get_args(mrb, "zz", &name, &value);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetGlobalString(name, value);
    }

    return mrb_nil_value();
}

static mrb_value mrb_script_api_set_global_bool(mrb_state* mrb, mrb_value) {
    char* name;
    mrb_bool value;
    mrb_get_args(mrb, "zb", &name, &value);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetGlobalBool(name, value);
    }

    return mrb_nil_value();
}

// Position/Movement APIs
static mrb_value mrb_script_api_get_position_2d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        auto pos = api->GetPosition2D();
        mrb_value result = mrb_ary_new_capa(mrb, 2);
        mrb_ary_push(mrb, result, mrb_float_value(mrb, pos.x));
        mrb_ary_push(mrb, result, mrb_float_value(mrb, pos.y));
        return result;
    }
    mrb_value result = mrb_ary_new_capa(mrb, 2);
    mrb_ary_push(mrb, result, mrb_float_value(mrb, 0.0f));
    mrb_ary_push(mrb, result, mrb_float_value(mrb, 0.0f));
    return result;
}

static mrb_value mrb_script_api_set_position_2d(mrb_state* mrb, mrb_value) {
    mrb_float x, y;
    mrb_get_args(mrb, "ff", &x, &y);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetPosition2D(static_cast<float>(x), static_cast<float>(y));
    }

    return mrb_nil_value();
}

static mrb_value mrb_script_api_translate_2d(mrb_state* mrb, mrb_value) {
    mrb_float dx, dy;
    mrb_get_args(mrb, "ff", &dx, &dy);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->Translate2D(static_cast<float>(dx), static_cast<float>(dy));
    }

    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_position_3d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        auto pos = api->GetPosition3D();
        mrb_value result = mrb_ary_new_capa(mrb, 3);
        mrb_ary_push(mrb, result, mrb_float_value(mrb, pos.x));
        mrb_ary_push(mrb, result, mrb_float_value(mrb, pos.y));
        mrb_ary_push(mrb, result, mrb_float_value(mrb, pos.z));
        return result;
    }
    mrb_value result = mrb_ary_new_capa(mrb, 3);
    mrb_ary_push(mrb, result, mrb_float_value(mrb, 0.0f));
    mrb_ary_push(mrb, result, mrb_float_value(mrb, 0.0f));
    mrb_ary_push(mrb, result, mrb_float_value(mrb, 0.0f));
    return result;
}

static mrb_value mrb_script_api_set_position_3d(mrb_state* mrb, mrb_value) {
    mrb_float x, y, z;
    mrb_get_args(mrb, "fff", &x, &y, &z);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetPosition3D(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
    }

    return mrb_nil_value();
}

static mrb_value mrb_script_api_translate_3d(mrb_state* mrb, mrb_value) {
    mrb_float dx, dy, dz;
    mrb_get_args(mrb, "fff", &dx, &dy, &dz);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->Translate3D(static_cast<float>(dx), static_cast<float>(dy), static_cast<float>(dz));
    }

    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_rotation_2d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return mrb_float_value(mrb, api->GetRotation2D());
    }
    return mrb_float_value(mrb, 0.0f);
}

static mrb_value mrb_script_api_set_rotation_2d(mrb_state* mrb, mrb_value) {
    mrb_float degrees;
    mrb_get_args(mrb, "f", &degrees);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetRotation2D(static_cast<float>(degrees));
    }

    return mrb_nil_value();
}

static mrb_value mrb_script_api_rotate_2d(mrb_state* mrb, mrb_value) {
    mrb_float degrees;
    mrb_get_args(mrb, "f", &degrees);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->Rotate2D(static_cast<float>(degrees));
    }

    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_scale_2d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        auto scale = api->GetScale2D();
        mrb_value result = mrb_ary_new_capa(mrb, 2);
        mrb_ary_push(mrb, result, mrb_float_value(mrb, scale.x));
        mrb_ary_push(mrb, result, mrb_float_value(mrb, scale.y));
        return result;
    }
    mrb_value result = mrb_ary_new_capa(mrb, 2);
    mrb_ary_push(mrb, result, mrb_float_value(mrb, 1.0f));
    mrb_ary_push(mrb, result, mrb_float_value(mrb, 1.0f));
    return result;
}

static mrb_value mrb_script_api_set_scale_2d(mrb_state* mrb, mrb_value) {
    mrb_float x, y;
    mrb_get_args(mrb, "ff", &x, &y);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetScale2D(static_cast<float>(x), static_cast<float>(y));
    }

    return mrb_nil_value();
}

// ============================================================================
// PHYSICS 2D - QUERIES
// ============================================================================

static mrb_value mrb_script_api_raycast_2d(mrb_state* mrb, mrb_value) {
    mrb_float fromX, fromY, dirX, dirY, maxDist;
    mrb_get_args(mrb, "fffff", &fromX, &fromY, &dirX, &dirY, &maxDist);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        auto hit = api->Raycast2D(mrbVec2(fromX, fromY), mrbVec2(dirX, dirY), static_cast<float>(maxDist));
        mrb_value result = mrb_hash_new(mrb);
        mrb_hash_set(mrb, result, mrb_symbol_value(mrb_intern_lit(mrb, "hit")), hit.hit ? mrb_true_value() : mrb_false_value());
        mrb_hash_set(mrb, result, mrb_symbol_value(mrb_intern_lit(mrb, "point_x")), mrb_float_value(mrb, hit.point.x));
        mrb_hash_set(mrb, result, mrb_symbol_value(mrb_intern_lit(mrb, "point_y")), mrb_float_value(mrb, hit.point.y));
        mrb_hash_set(mrb, result, mrb_symbol_value(mrb_intern_lit(mrb, "normal_x")), mrb_float_value(mrb, hit.normal.x));
        mrb_hash_set(mrb, result, mrb_symbol_value(mrb_intern_lit(mrb, "normal_y")), mrb_float_value(mrb, hit.normal.y));
        mrb_hash_set(mrb, result, mrb_symbol_value(mrb_intern_lit(mrb, "distance")), mrb_float_value(mrb, hit.distance));
        return result;
    }

    mrb_value result = mrb_hash_new(mrb);
    mrb_hash_set(mrb, result, mrb_symbol_value(mrb_intern_lit(mrb, "hit")), mrb_false_value());
    return result;
}

static mrb_value mrb_script_api_raycast_3d(mrb_state* mrb, mrb_value) {
    mrb_float fromX, fromY, fromZ, dirX, dirY, dirZ, maxDist;
    mrb_get_args(mrb, "fffffff", &fromX, &fromY, &fromZ, &dirX, &dirY, &dirZ, &maxDist);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        auto hit = api->Raycast3D(mrbVec3(fromX, fromY, fromZ), mrbVec3(dirX, dirY, dirZ), static_cast<float>(maxDist));
        mrb_value result = mrb_hash_new(mrb);
        mrb_hash_set(mrb, result, mrb_symbol_value(mrb_intern_lit(mrb, "hit")), hit.hit ? mrb_true_value() : mrb_false_value());
        mrb_hash_set(mrb, result, mrb_symbol_value(mrb_intern_lit(mrb, "point_x")), mrb_float_value(mrb, hit.point.x));
        mrb_hash_set(mrb, result, mrb_symbol_value(mrb_intern_lit(mrb, "point_y")), mrb_float_value(mrb, hit.point.y));
        mrb_hash_set(mrb, result, mrb_symbol_value(mrb_intern_lit(mrb, "point_z")), mrb_float_value(mrb, hit.point.z));
        mrb_hash_set(mrb, result, mrb_symbol_value(mrb_intern_lit(mrb, "normal_x")), mrb_float_value(mrb, hit.normal.x));
        mrb_hash_set(mrb, result, mrb_symbol_value(mrb_intern_lit(mrb, "normal_y")), mrb_float_value(mrb, hit.normal.y));
        mrb_hash_set(mrb, result, mrb_symbol_value(mrb_intern_lit(mrb, "normal_z")), mrb_float_value(mrb, hit.normal.z));
        mrb_hash_set(mrb, result, mrb_symbol_value(mrb_intern_lit(mrb, "distance")), mrb_float_value(mrb, hit.distance));
        return result;
    }

    mrb_value result = mrb_hash_new(mrb);
    mrb_hash_set(mrb, result, mrb_symbol_value(mrb_intern_lit(mrb, "hit")), mrb_false_value());
    return result;
}

// ============================================================================
// PHYSICS 2D - BODY MANIPULATION
// ============================================================================

static mrb_value mrb_script_api_get_linear_velocity_2d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        auto vel = api->GetLinearVelocity2D();
        mrb_value result = mrb_ary_new_capa(mrb, 2);
        mrb_ary_push(mrb, result, mrb_float_value(mrb, vel.x));
        mrb_ary_push(mrb, result, mrb_float_value(mrb, vel.y));
        return result;
    }
    mrb_value result = mrb_ary_new_capa(mrb, 2);
    mrb_ary_push(mrb, result, mrb_float_value(mrb, 0.0f));
    mrb_ary_push(mrb, result, mrb_float_value(mrb, 0.0f));
    return result;
}

static mrb_value mrb_script_api_set_linear_velocity_2d(mrb_state* mrb, mrb_value) {
    mrb_float x, y;
    mrb_get_args(mrb, "ff", &x, &y);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetLinearVelocity2D(static_cast<float>(x), static_cast<float>(y));
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_angular_velocity_2d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->GetAngularVelocity2D() : 0.0f);
}

static mrb_value mrb_script_api_set_angular_velocity_2d(mrb_state* mrb, mrb_value) {
    mrb_float omega;
    mrb_get_args(mrb, "f", &omega);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetAngularVelocity2D(static_cast<float>(omega));
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_apply_force_2d(mrb_state* mrb, mrb_value) {
    mrb_float fx, fy;
    mrb_get_args(mrb, "ff", &fx, &fy);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->ApplyForce2D(static_cast<float>(fx), static_cast<float>(fy));
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_apply_force_at_point_2d(mrb_state* mrb, mrb_value) {
    mrb_float fx, fy, px, py;
    mrb_get_args(mrb, "ffff", &fx, &fy, &px, &py);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->ApplyForceAtPoint2D(static_cast<float>(fx), static_cast<float>(fy),
                                  static_cast<float>(px), static_cast<float>(py));
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_apply_torque_2d(mrb_state* mrb, mrb_value) {
    mrb_float torque;
    mrb_get_args(mrb, "f", &torque);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->ApplyTorque2D(static_cast<float>(torque));
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_apply_impulse_2d(mrb_state* mrb, mrb_value) {
    mrb_float ix, iy;
    mrb_get_args(mrb, "ff", &ix, &iy);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->ApplyImpulse2D(static_cast<float>(ix), static_cast<float>(iy));
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_apply_impulse_at_point_2d(mrb_state* mrb, mrb_value) {
    mrb_float ix, iy, px, py;
    mrb_get_args(mrb, "ffff", &ix, &iy, &px, &py);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->ApplyImpulseAtPoint2D(static_cast<float>(ix), static_cast<float>(iy),
                                    static_cast<float>(px), static_cast<float>(py));
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_apply_angular_impulse_2d(mrb_state* mrb, mrb_value) {
    mrb_float impulse;
    mrb_get_args(mrb, "f", &impulse);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->ApplyAngularImpulse2D(static_cast<float>(impulse));
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_mass_2d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->GetMass2D() : 0.0f);
}

static mrb_value mrb_script_api_get_gravity_scale_2d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->GetGravityScale2D() : 1.0f);
}

static mrb_value mrb_script_api_set_gravity_scale_2d(mrb_state* mrb, mrb_value) {
    mrb_float scale;
    mrb_get_args(mrb, "f", &scale);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetGravityScale2D(static_cast<float>(scale));
    }
    return mrb_nil_value();
}

// ============================================================================
// PHYSICS 3D - BODY MANIPULATION
// ============================================================================

static mrb_value mrb_script_api_get_linear_velocity_3d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        auto vel = api->GetLinearVelocity3D();
        mrb_value result = mrb_ary_new_capa(mrb, 3);
        mrb_ary_push(mrb, result, mrb_float_value(mrb, vel.x));
        mrb_ary_push(mrb, result, mrb_float_value(mrb, vel.y));
        mrb_ary_push(mrb, result, mrb_float_value(mrb, vel.z));
        return result;
    }
    mrb_value result = mrb_ary_new_capa(mrb, 3);
    mrb_ary_push(mrb, result, mrb_float_value(mrb, 0.0f));
    mrb_ary_push(mrb, result, mrb_float_value(mrb, 0.0f));
    mrb_ary_push(mrb, result, mrb_float_value(mrb, 0.0f));
    return result;
}

static mrb_value mrb_script_api_set_linear_velocity_3d(mrb_state* mrb, mrb_value) {
    mrb_float x, y, z;
    mrb_get_args(mrb, "fff", &x, &y, &z);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetLinearVelocity3D(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_angular_velocity_3d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        auto vel = api->GetAngularVelocity3D();
        mrb_value result = mrb_ary_new_capa(mrb, 3);
        mrb_ary_push(mrb, result, mrb_float_value(mrb, vel.x));
        mrb_ary_push(mrb, result, mrb_float_value(mrb, vel.y));
        mrb_ary_push(mrb, result, mrb_float_value(mrb, vel.z));
        return result;
    }
    mrb_value result = mrb_ary_new_capa(mrb, 3);
    mrb_ary_push(mrb, result, mrb_float_value(mrb, 0.0f));
    mrb_ary_push(mrb, result, mrb_float_value(mrb, 0.0f));
    mrb_ary_push(mrb, result, mrb_float_value(mrb, 0.0f));
    return result;
}

static mrb_value mrb_script_api_set_angular_velocity_3d(mrb_state* mrb, mrb_value) {
    mrb_float x, y, z;
    mrb_get_args(mrb, "fff", &x, &y, &z);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetAngularVelocity3D(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_apply_force_3d(mrb_state* mrb, mrb_value) {
    mrb_float fx, fy, fz;
    mrb_get_args(mrb, "fff", &fx, &fy, &fz);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->ApplyForce3D(static_cast<float>(fx), static_cast<float>(fy), static_cast<float>(fz));
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_apply_impulse_3d(mrb_state* mrb, mrb_value) {
    mrb_float ix, iy, iz;
    mrb_get_args(mrb, "fff", &ix, &iy, &iz);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->ApplyImpulse3D(static_cast<float>(ix), static_cast<float>(iy), static_cast<float>(iz));
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_apply_torque_3d(mrb_state* mrb, mrb_value) {
    mrb_float x, y, z;
    mrb_get_args(mrb, "fff", &x, &y, &z);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->ApplyTorque3D(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_mass_3d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->GetMass3D() : 0.0f);
}

static mrb_value mrb_script_api_set_mass_3d(mrb_state* mrb, mrb_value) {
    mrb_float mass;
    mrb_get_args(mrb, "f", &mass);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetMass3D(static_cast<float>(mass));
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_gravity_scale_3d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->GetGravityScale3D() : 1.0f);
}

static mrb_value mrb_script_api_set_gravity_scale_3d(mrb_state* mrb, mrb_value) {
    mrb_float scale;
    mrb_get_args(mrb, "f", &scale);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetGravityScale3D(static_cast<float>(scale));
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_linear_damping_2d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->GetLinearDamping2D() : 0.0f);
}

static mrb_value mrb_script_api_set_linear_damping_2d(mrb_state* mrb, mrb_value) {
    mrb_float damping;
    mrb_get_args(mrb, "f", &damping);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetLinearDamping2D(static_cast<float>(damping));
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_linear_damping_3d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->GetLinearDamping3D() : 0.0f);
}

static mrb_value mrb_script_api_set_linear_damping_3d(mrb_state* mrb, mrb_value) {
    mrb_float damping;
    mrb_get_args(mrb, "f", &damping);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetLinearDamping3D(static_cast<float>(damping));
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_angular_damping_3d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->GetAngularDamping3D() : 0.0f);
}

static mrb_value mrb_script_api_set_angular_damping_3d(mrb_state* mrb, mrb_value) {
    mrb_float damping;
    mrb_get_args(mrb, "f", &damping);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetAngularDamping3D(static_cast<float>(damping));
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_linear_factor_3d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    math::Vec3 factor = api ? api->GetLinearFactor3D() : math::Vec3(1.0f, 1.0f, 1.0f);
    mrb_value result = mrb_ary_new_capa(mrb, 3);
    mrb_ary_push(mrb, result, mrb_float_value(mrb, factor.x));
    mrb_ary_push(mrb, result, mrb_float_value(mrb, factor.y));
    mrb_ary_push(mrb, result, mrb_float_value(mrb, factor.z));
    return result;
}

static mrb_value mrb_script_api_set_linear_factor_3d(mrb_state* mrb, mrb_value) {
    mrb_float x, y, z;
    mrb_get_args(mrb, "fff", &x, &y, &z);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetLinearFactor3D(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_angular_factor_3d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    math::Vec3 factor = api ? api->GetAngularFactor3D() : math::Vec3(1.0f, 1.0f, 1.0f);
    mrb_value result = mrb_ary_new_capa(mrb, 3);
    mrb_ary_push(mrb, result, mrb_float_value(mrb, factor.x));
    mrb_ary_push(mrb, result, mrb_float_value(mrb, factor.y));
    mrb_ary_push(mrb, result, mrb_float_value(mrb, factor.z));
    return result;
}

static mrb_value mrb_script_api_set_angular_factor_3d(mrb_state* mrb, mrb_value) {
    mrb_float x, y, z;
    mrb_get_args(mrb, "fff", &x, &y, &z);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetAngularFactor3D(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_apply_force_at_point_3d(mrb_state* mrb, mrb_value) {
    mrb_float fx, fy, fz, px, py, pz;
    mrb_get_args(mrb, "ffffff", &fx, &fy, &fz, &px, &py, &pz);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->ApplyForceAtPoint3D(static_cast<float>(fx), static_cast<float>(fy), static_cast<float>(fz),
                                  static_cast<float>(px), static_cast<float>(py), static_cast<float>(pz));
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_apply_impulse_at_point_3d(mrb_state* mrb, mrb_value) {
    mrb_float ix, iy, iz, px, py, pz;
    mrb_get_args(mrb, "ffffff", &ix, &iy, &iz, &px, &py, &pz);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->ApplyImpulseAtPoint3D(static_cast<float>(ix), static_cast<float>(iy), static_cast<float>(iz),
                                    static_cast<float>(px), static_cast<float>(py), static_cast<float>(pz));
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_apply_torque_impulse_3d(mrb_state* mrb, mrb_value) {
    mrb_float x, y, z;
    mrb_get_args(mrb, "fff", &x, &y, &z);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->ApplyTorqueImpulse3D(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_log_debug(mrb_state* mrb, mrb_value) {
    char* message;
    mrb_get_args(mrb, "z", &message);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->LogDebug(message);
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_queue_free_self(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->QueueFreeSelf();
    return mrb_nil_value();
}

static mrb_value mrb_script_api_random_range_int(mrb_state* mrb, mrb_value) {
    mrb_int min, max;
    mrb_get_args(mrb, "ii", &min, &max);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return mrb_fixnum_value(api->RandomRangeInt(static_cast<int>(min), static_cast<int>(max)));
    }
    return mrb_fixnum_value(0);
}

static mrb_value mrb_script_api_quit(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->RequestQuit();
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_cmdline_args(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    mrb_value arr = mrb_ary_new(mrb);
    if (api) {
        for (const std::string& arg : api->GetCommandLineArgs()) {
            mrb_ary_push(mrb, arr, mrb_str_new(mrb, arg.c_str(), arg.size()));
        }
    }
    return arr;
}

// ============================================================================
// PHYSICS WORLD ACCESS
// ============================================================================

static mrb_value mrb_script_api_set_gravity_2d(mrb_state* mrb, mrb_value) {
    mrb_float x, y;
    mrb_get_args(mrb, "ff", &x, &y);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetGravity2D(static_cast<float>(x), static_cast<float>(y));
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_gravity_2d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        auto gravity = api->GetGravity2D();
        mrb_value result = mrb_ary_new_capa(mrb, 2);
        mrb_ary_push(mrb, result, mrb_float_value(mrb, gravity.x));
        mrb_ary_push(mrb, result, mrb_float_value(mrb, gravity.y));
        return result;
    }
    mrb_value result = mrb_ary_new_capa(mrb, 2);
    mrb_ary_push(mrb, result, mrb_float_value(mrb, 0.0f));
    mrb_ary_push(mrb, result, mrb_float_value(mrb, -9.81f));
    return result;
}

static mrb_value mrb_script_api_set_gravity_3d(mrb_state* mrb, mrb_value) {
    mrb_float x, y, z;
    mrb_get_args(mrb, "fff", &x, &y, &z);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetGravity3D(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_gravity_3d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        auto gravity = api->GetGravity3D();
        mrb_value result = mrb_ary_new_capa(mrb, 3);
        mrb_ary_push(mrb, result, mrb_float_value(mrb, gravity.x));
        mrb_ary_push(mrb, result, mrb_float_value(mrb, gravity.y));
        mrb_ary_push(mrb, result, mrb_float_value(mrb, gravity.z));
        return result;
    }
    mrb_value result = mrb_ary_new_capa(mrb, 3);
    mrb_ary_push(mrb, result, mrb_float_value(mrb, 0.0f));
    mrb_ary_push(mrb, result, mrb_float_value(mrb, -9.81f));
    mrb_ary_push(mrb, result, mrb_float_value(mrb, 0.0f));
    return result;
}

// ============================================================================
// CHARACTER CONTROLLER (2D)
// ============================================================================

static mrb_value mrb_script_api_move_and_slide_2d(mrb_state* mrb, mrb_value) {
    mrb_float vx, vy;
    mrb_get_args(mrb, "ff", &vx, &vy);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        auto result = api->MoveAndSlide2D(static_cast<float>(vx), static_cast<float>(vy));
        mrb_value arr = mrb_ary_new_capa(mrb, 2);
        mrb_ary_push(mrb, arr, mrb_float_value(mrb, result.x));
        mrb_ary_push(mrb, arr, mrb_float_value(mrb, result.y));
        return arr;
    }
    mrb_value arr = mrb_ary_new_capa(mrb, 2);
    mrb_ary_push(mrb, arr, mrb_float_value(mrb, 0.0f));
    mrb_ary_push(mrb, arr, mrb_float_value(mrb, 0.0f));
    return arr;
}

static mrb_value mrb_script_api_get_character_velocity_2d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        auto vel = api->GetCharacterVelocity2D();
        mrb_value result = mrb_ary_new_capa(mrb, 2);
        mrb_ary_push(mrb, result, mrb_float_value(mrb, vel.x));
        mrb_ary_push(mrb, result, mrb_float_value(mrb, vel.y));
        return result;
    }
    mrb_value result = mrb_ary_new_capa(mrb, 2);
    mrb_ary_push(mrb, result, mrb_float_value(mrb, 0.0f));
    mrb_ary_push(mrb, result, mrb_float_value(mrb, 0.0f));
    return result;
}

static mrb_value mrb_script_api_set_character_velocity_2d(mrb_state* mrb, mrb_value) {
    mrb_float x, y;
    mrb_get_args(mrb, "ff", &x, &y);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetCharacterVelocity2D(static_cast<float>(x), static_cast<float>(y));
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_is_on_ground_2d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return api && api->IsCharacterOnGround2D() ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_is_on_wall_2d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return api && api->IsCharacterOnWall2D() ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_is_on_ceiling_2d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return api && api->IsCharacterOnCeiling2D() ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_get_ground_normal_2d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        auto normal = api->GetCharacterGroundNormal2D();
        mrb_value result = mrb_ary_new_capa(mrb, 2);
        mrb_ary_push(mrb, result, mrb_float_value(mrb, normal.x));
        mrb_ary_push(mrb, result, mrb_float_value(mrb, normal.y));
        return result;
    }
    mrb_value result = mrb_ary_new_capa(mrb, 2);
    mrb_ary_push(mrb, result, mrb_float_value(mrb, 0.0f));
    mrb_ary_push(mrb, result, mrb_float_value(mrb, 1.0f));
    return result;
}

static mrb_value mrb_script_api_get_wall_normal_2d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        auto normal = api->GetCharacterWallNormal2D();
        mrb_value result = mrb_ary_new_capa(mrb, 2);
        mrb_ary_push(mrb, result, mrb_float_value(mrb, normal.x));
        mrb_ary_push(mrb, result, mrb_float_value(mrb, normal.y));
        return result;
    }
    mrb_value result = mrb_ary_new_capa(mrb, 2);
    mrb_ary_push(mrb, result, mrb_float_value(mrb, 0.0f));
    mrb_ary_push(mrb, result, mrb_float_value(mrb, 0.0f));
    return result;
}

static mrb_value mrb_script_api_get_character_gravity_2d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->GetCharacterGravity2D() : 980.0f);
}

static mrb_value mrb_script_api_set_character_gravity_2d(mrb_state* mrb, mrb_value) {
    mrb_float gravity;
    mrb_get_args(mrb, "f", &gravity);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetCharacterGravity2D(static_cast<float>(gravity));
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_character_max_fall_speed_2d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->GetCharacterMaxFallSpeed2D() : 1000.0f);
}

static mrb_value mrb_script_api_set_character_max_fall_speed_2d(mrb_state* mrb, mrb_value) {
    mrb_float speed;
    mrb_get_args(mrb, "f", &speed);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetCharacterMaxFallSpeed2D(static_cast<float>(speed));
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_character_max_slope_angle_2d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->GetCharacterMaxSlopeAngle2D() : 45.0f);
}

static mrb_value mrb_script_api_set_character_max_slope_angle_2d(mrb_state* mrb, mrb_value) {
    mrb_float angle;
    mrb_get_args(mrb, "f", &angle);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetCharacterMaxSlopeAngle2D(static_cast<float>(angle));
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_character_snap_to_ground_2d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return api && api->GetCharacterSnapToGround2D() ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_set_character_snap_to_ground_2d(mrb_state* mrb, mrb_value) {
    mrb_bool snap;
    mrb_get_args(mrb, "b", &snap);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetCharacterSnapToGround2D(snap);
    }
    return mrb_nil_value();
}

// ============================================================================
// CHARACTER CONTROLLER (3D)
// ============================================================================

static mrb_value mrb_script_api_move_and_slide_3d(mrb_state* mrb, mrb_value) {
    mrb_float vx, vy, vz;
    mrb_get_args(mrb, "fff", &vx, &vy, &vz);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        auto result = api->MoveAndSlide3D(static_cast<float>(vx), static_cast<float>(vy), static_cast<float>(vz));
        mrb_value arr = mrb_ary_new_capa(mrb, 3);
        mrb_ary_push(mrb, arr, mrb_float_value(mrb, result.x));
        mrb_ary_push(mrb, arr, mrb_float_value(mrb, result.y));
        mrb_ary_push(mrb, arr, mrb_float_value(mrb, result.z));
        return arr;
    }
    mrb_value arr = mrb_ary_new_capa(mrb, 3);
    mrb_ary_push(mrb, arr, mrb_float_value(mrb, 0.0f));
    mrb_ary_push(mrb, arr, mrb_float_value(mrb, 0.0f));
    mrb_ary_push(mrb, arr, mrb_float_value(mrb, 0.0f));
    return arr;
}

static mrb_value mrb_script_api_get_character_velocity_3d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        auto vel = api->GetCharacterVelocity3D();
        mrb_value result = mrb_ary_new_capa(mrb, 3);
        mrb_ary_push(mrb, result, mrb_float_value(mrb, vel.x));
        mrb_ary_push(mrb, result, mrb_float_value(mrb, vel.y));
        mrb_ary_push(mrb, result, mrb_float_value(mrb, vel.z));
        return result;
    }
    mrb_value result = mrb_ary_new_capa(mrb, 3);
    mrb_ary_push(mrb, result, mrb_float_value(mrb, 0.0f));
    mrb_ary_push(mrb, result, mrb_float_value(mrb, 0.0f));
    mrb_ary_push(mrb, result, mrb_float_value(mrb, 0.0f));
    return result;
}

static mrb_value mrb_script_api_set_character_velocity_3d(mrb_state* mrb, mrb_value) {
    mrb_float x, y, z;
    mrb_get_args(mrb, "fff", &x, &y, &z);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetCharacterVelocity3D(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_is_on_ground_3d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return api && api->IsCharacterOnGround3D() ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_is_on_wall_3d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return api && api->IsCharacterOnWall3D() ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_is_on_ceiling_3d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return api && api->IsCharacterOnCeiling3D() ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_get_ground_normal_3d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        auto normal = api->GetCharacterGroundNormal3D();
        mrb_value result = mrb_ary_new_capa(mrb, 3);
        mrb_ary_push(mrb, result, mrb_float_value(mrb, normal.x));
        mrb_ary_push(mrb, result, mrb_float_value(mrb, normal.y));
        mrb_ary_push(mrb, result, mrb_float_value(mrb, normal.z));
        return result;
    }
    mrb_value result = mrb_ary_new_capa(mrb, 3);
    mrb_ary_push(mrb, result, mrb_float_value(mrb, 0.0f));
    mrb_ary_push(mrb, result, mrb_float_value(mrb, 1.0f));
    mrb_ary_push(mrb, result, mrb_float_value(mrb, 0.0f));
    return result;
}

static mrb_value mrb_script_api_get_wall_normal_3d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        auto normal = api->GetCharacterWallNormal3D();
        mrb_value result = mrb_ary_new_capa(mrb, 3);
        mrb_ary_push(mrb, result, mrb_float_value(mrb, normal.x));
        mrb_ary_push(mrb, result, mrb_float_value(mrb, normal.y));
        mrb_ary_push(mrb, result, mrb_float_value(mrb, normal.z));
        return result;
    }
    mrb_value result = mrb_ary_new_capa(mrb, 3);
    mrb_ary_push(mrb, result, mrb_float_value(mrb, 0.0f));
    mrb_ary_push(mrb, result, mrb_float_value(mrb, 0.0f));
    mrb_ary_push(mrb, result, mrb_float_value(mrb, 0.0f));
    return result;
}

static mrb_value mrb_script_api_get_character_gravity_3d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->GetCharacterGravity3D() : 9.81f);
}

static mrb_value mrb_script_api_set_character_gravity_3d(mrb_state* mrb, mrb_value) {
    mrb_float gravity;
    mrb_get_args(mrb, "f", &gravity);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetCharacterGravity3D(static_cast<float>(gravity));
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_character_max_fall_speed_3d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->GetCharacterMaxFallSpeed3D() : 50.0f);
}

static mrb_value mrb_script_api_set_character_max_fall_speed_3d(mrb_state* mrb, mrb_value) {
    mrb_float speed;
    mrb_get_args(mrb, "f", &speed);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetCharacterMaxFallSpeed3D(static_cast<float>(speed));
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_character_max_slope_angle_3d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->GetCharacterMaxSlopeAngle3D() : 45.0f);
}

static mrb_value mrb_script_api_set_character_max_slope_angle_3d(mrb_state* mrb, mrb_value) {
    mrb_float angle;
    mrb_get_args(mrb, "f", &angle);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetCharacterMaxSlopeAngle3D(static_cast<float>(angle));
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_character_step_height_3d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->GetCharacterStepHeight3D() : 0.3f);
}

static mrb_value mrb_script_api_set_character_step_height_3d(mrb_state* mrb, mrb_value) {
    mrb_float height;
    mrb_get_args(mrb, "f", &height);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetCharacterStepHeight3D(static_cast<float>(height));
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_character_snap_to_ground_3d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return api && api->GetCharacterSnapToGround3D() ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_set_character_snap_to_ground_3d(mrb_state* mrb, mrb_value) {
    mrb_bool snap;
    mrb_get_args(mrb, "b", &snap);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetCharacterSnapToGround3D(snap);
    }
    return mrb_nil_value();
}

// ============================================================================
// MRubyHost - the single shared mRuby VM
// ============================================================================

MRubyHost& MRubyHost::Instance() {
    static MRubyHost s_Instance;
    return s_Instance;
}

bool MRubyHost::EnsureInitialized() {
    if (m_Initialized) {
        return true;
    }

    m_MRubyState = mrb_open();
    if (!m_MRubyState) {
        return false;
    }

    RegisterScriptAPI();

    m_Initialized = true;

    // Install the Fiber-based coroutine / await scheduler (pumped each frame by
    // MRubyHost::Pump -> Lupine._pump). Mirrors the Lua scheduler.
    //
    // Every entry is tagged with the id of the instance that started it. Both the
    // resume and the `:until` predicate run inside that instance's context, so
    // self-relative APIs keep working after the first await; an entry whose owner
    // has shut down (or whose predicate raised) is marked dead and dropped.
    static const char* kSchedulerRuby = R"RUBY(
module Lupine
  @coros = []
  def self.start_coroutine(&blk)
    fib = Fiber.new(&blk)
    entry = { fiber: fib, wait: nil, dead: false, ctx: Lupine._context_id }
    @coros << entry
    _step(entry)
    fib
  end
  def self._step(entry)
    return unless entry[:fiber].alive?
    unless Lupine._push_context(entry[:ctx])
      entry[:dead] = true
      return
    end
    begin
      entry[:wait] = entry[:fiber].resume
    rescue => e
      Lupine._scheduler_error("coroutine error: #{e}")
      entry[:dead] = true
    ensure
      Lupine._pop_context
    end
  end
  def self._ready(entry, fn)
    unless Lupine._push_context(entry[:ctx])
      entry[:dead] = true
      return false
    end
    begin
      !!fn.call
    rescue => e
      Lupine._scheduler_error("coroutine predicate error: #{e}")
      entry[:dead] = true
      false
    ensure
      Lupine._pop_context
    end
  end
  def self._pump(dt)
    @coros.reject! { |e| e[:dead] || !e[:fiber].alive? }
    @coros.dup.each do |entry|
      next if entry[:dead]
      w = entry[:wait]
      ready = false
      if w.nil?
        ready = true
      elsif w[:type] == :time
        w[:t] -= dt
        ready = w[:t] <= 0.0
      elsif w[:type] == :frames
        w[:n] -= 1
        ready = w[:n] <= 0
      elsif w[:type] == :until
        ready = _ready(entry, w[:fn])
      else
        ready = true
      end
      _step(entry) if ready && !entry[:dead]
    end
    @coros.reject! { |e| e[:dead] || !e[:fiber].alive? }
  end
  def self.coroutine_count; @coros.size; end
  def self.stop_coroutine(fib); @coros.each { |e| e[:dead] = true if e[:fiber] == fib }; end
  def self._stop_all_for(ctx)
    @coros.each { |e| e[:dead] = true if e[:ctx] == ctx }
    @coros.reject! { |e| e[:dead] }
  end
  def self.await_seconds(s); Fiber.yield({ type: :time, t: s }); end
  def self.await_frames(n = 1); Fiber.yield({ type: :frames, n: n }); end
  def self.await_next_frame; Fiber.yield({ type: :frames, n: 1 }); end
  def self.await_until(&pred); Fiber.yield({ type: :until, fn: pred }); end
  def self.await_tween(tw); Fiber.yield({ type: :until, fn: ->() { (!tw.is_valid) || tw.is_finished } }); end
  def self.await_signal(obj, signal)
    aw = obj.await_signal(signal)
    return if aw.nil?
    Fiber.yield({ type: :until, fn: ->() { (!aw.is_valid) || aw.is_fired } })
  end
  def self.await_archetype(handle)
    Fiber.yield({ type: :until, fn: ->() { Lupine.is_archetype_load_complete(handle) } })
    Lupine.get_async_archetype(handle)
  end
end
)RUBY";
    {
        const int arena = mrb_gc_arena_save(m_MRubyState);
        mrbc_context* ctx = mrbc_context_new(m_MRubyState);
        mrb_load_string_cxt(m_MRubyState, kSchedulerRuby, ctx);
        mrbc_context_free(m_MRubyState, ctx);
        if (m_MRubyState->exc) {
            const std::string error = TakeMRubyError(m_MRubyState);
            LOG_ERROR(LogCategory::Scripting, "mRuby coroutine scheduler failed to load: {}", error);
        }
        mrb_gc_arena_restore(m_MRubyState, arena);
    }

    return true;
}

void MRubyHost::PushContext(ScriptAPI* api, int instanceId) {
    if (!m_MRubyState) {
        return;
    }

    mrb_value prevSelf = mrb_gv_get(m_MRubyState, mrb_intern_lit(m_MRubyState, "$self"));

    // Once $self is overwritten below, the saved handle's only reference is this C++
    // vector, which the GC does not scan - it must become an explicit GC root.
    if (!mrb_immediate_p(prevSelf)) {
        mrb_gc_register(m_MRubyState, prevSelf);
    }

    ContextFrame frame;
    frame.api = m_ActiveApi;
    frame.self = prevSelf;
    frame.instanceId = m_ActiveInstanceId;
    m_ContextStack.push_back(frame);

    m_ActiveApi = api;
    m_ActiveInstanceId = instanceId;

    const int arena = mrb_gc_arena_save(m_MRubyState);
    core::Node* owner = api ? api->GetSelf() : nullptr;
    mrb_value selfRef = owner ? WrapNodeRef(m_MRubyState, NodeRef::FromRaw(owner, api))
                              : mrb_nil_value();
    mrb_gv_set(m_MRubyState, mrb_intern_lit(m_MRubyState, "$self"), selfRef);
    mrb_gc_arena_restore(m_MRubyState, arena);
}

void MRubyHost::PopContext() {
    if (m_ContextStack.empty()) {
        m_ActiveApi = nullptr;
        m_ActiveInstanceId = -1;
        return;
    }
    ContextFrame frame = m_ContextStack.back();
    m_ContextStack.pop_back();
    m_ActiveApi = frame.api;
    m_ActiveInstanceId = frame.instanceId;
    if (m_MRubyState) {
        mrb_gv_set(m_MRubyState, mrb_intern_lit(m_MRubyState, "$self"), frame.self);
        if (!mrb_immediate_p(frame.self)) {
            mrb_gc_unregister(m_MRubyState, frame.self);
        }
    }
}

bool MRubyHost::PushCoroutineContext(int instanceId) {
    if (instanceId < 0) {
        PushContext(nullptr, -1);
        return true;
    }

    bool found = false;
    ScriptAPI* api = FindInstanceApi(instanceId, found);
    if (!found) {
        return false;
    }

    PushContext(api, instanceId);
    return true;
}

void MRubyHost::RegisterInstance(int instanceId, ScriptAPI* api) {
    if (instanceId < 0) {
        return;
    }
    m_InstanceApis[instanceId] = api;
}

void MRubyHost::UnregisterInstance(int instanceId) {
    if (instanceId < 0) {
        return;
    }
    m_InstanceApis.erase(instanceId);
}

ScriptAPI* MRubyHost::FindInstanceApi(int instanceId, bool& found) const {
    std::unordered_map<int, ScriptAPI*>::const_iterator it = m_InstanceApis.find(instanceId);
    if (it == m_InstanceApis.end()) {
        found = false;
        return nullptr;
    }
    found = true;
    return it->second;
}

void MRubyHost::StopCoroutinesForInstance(int instanceId) {
    if (!m_Initialized || !m_MRubyState || instanceId < 0) {
        return;
    }
    if (!mrb_class_defined(m_MRubyState, "Lupine")) {
        return;
    }
    struct RClass* mod = mrb_module_get(m_MRubyState, "Lupine");
    mrb_value modv = mrb_obj_value(mod);
    mrb_sym stopAll = mrb_intern_lit(m_MRubyState, "_stop_all_for");
    if (!mrb_respond_to(m_MRubyState, modv, stopAll)) {
        return;
    }

    const int arena = mrb_gc_arena_save(m_MRubyState);
    mrb_value id = mrb_fixnum_value(instanceId);
    mrb_funcall_argv(m_MRubyState, modv, stopAll, 1, &id);
    if (m_MRubyState->exc) {
        const std::string error = TakeMRubyError(m_MRubyState);
        LOG_ERROR(LogCategory::Scripting, "mRuby coroutine teardown error: {}", error);
    }
    mrb_gc_arena_restore(m_MRubyState, arena);
}

void MRubyHost::Pump(float deltaTime) {
    if (!m_Initialized || !m_MRubyState) {
        return;
    }
    if (!mrb_class_defined(m_MRubyState, "Lupine")) {
        return;
    }
    struct RClass* mod = mrb_module_get(m_MRubyState, "Lupine");
    mrb_value modv = mrb_obj_value(mod);
    mrb_sym pump = mrb_intern_lit(m_MRubyState, "_pump");
    if (!mrb_respond_to(m_MRubyState, modv, pump)) {
        return;
    }

    // mrb_funcall_argv protects its result in the arena, and the scheduler allocates
    // freely while it runs; without this bracket every frame permanently roots them.
    const int arena = mrb_gc_arena_save(m_MRubyState);
    mrb_value dt = mrb_float_value(m_MRubyState, static_cast<mrb_float>(deltaTime));
    mrb_funcall_argv(m_MRubyState, modv, pump, 1, &dt);
    if (m_MRubyState->exc) {
        const std::string error = TakeMRubyError(m_MRubyState);
        LOG_ERROR(LogCategory::Scripting, "mRuby coroutine scheduler error: {}", error);
    }
    mrb_gc_arena_restore(m_MRubyState, arena);
}

ScriptAPI* MRubyHost::SharedNodeApi(ScriptAPI* treeSource) {
    if (treeSource) {
        m_SharedNodeApi.SetSceneManager(treeSource->GetTree());
    }
    return &m_SharedNodeApi;
}

void MRubyHost::SetSharedGlobalNode(const std::string& name, core::Node* node) {
    if (!m_Initialized || !m_MRubyState || name.empty()) return;

    int arena = mrb_gc_arena_save(m_MRubyState);

    mrb_value wrapped = WrapNodeRef(m_MRubyState, NodeRef::FromRaw(node, SharedNodeApi(m_ActiveApi)));

    // Expose as a global variable ($Name); the lexer interns global names with the
    // leading '$', so the prefix is required for bare `$Name` access from scripts.
    std::string gvarName = "$" + name;
    mrb_gv_set(m_MRubyState, mrb_intern_cstr(m_MRubyState, gvarName.c_str()), wrapped);

    // If the name is a valid Ruby constant (uppercase first letter), also expose it
    // as a top-level constant so scripts can read it naturally, e.g. GameManager.foo.
    if (name[0] >= 'A' && name[0] <= 'Z') {
        mrb_define_global_const(m_MRubyState, name.c_str(), wrapped);
    }

    mrb_gc_arena_restore(m_MRubyState, arena);
}

// ============================================================================
// MRubyEnvironment - a per-component script instance (its own wrapper class)
// ============================================================================

MRubyEnvironment::MRubyEnvironment() : m_Instance(mrb_nil_value()) {
}

MRubyEnvironment::~MRubyEnvironment() {
    Shutdown();
}

bool MRubyEnvironment::Initialize() {
    if (m_Initialized) {
        return true;
    }

    MRubyHost& host = MRubyHost::Instance();
    if (!host.EnsureInitialized()) {
        m_LastError = "mRuby host failed to initialize";
        return false;
    }

    m_InstanceId = host.NextInstanceId();
    m_ClassName = "LupineScript_" + std::to_string(m_InstanceId);
    m_Instance = mrb_nil_value();
    m_HasInstance = false;
    m_Initialized = true;
    host.RegisterInstance(m_InstanceId, m_ScriptAPI);
    return true;
}

void MRubyEnvironment::Shutdown() {
    if (!m_Initialized) {
        return;
    }

    MRubyHost& host = MRubyHost::Instance();

    // Coroutines this instance started outlive it otherwise: they stay in the shared
    // scheduler and keep being resumed against a destroyed node.
    host.StopCoroutinesForInstance(m_InstanceId);

    mrb_state* mrb = host.State();
    if (mrb) {
        if (m_HasInstance) {
            mrb_gc_unregister(mrb, m_Instance);
        }
        // The wrapper class is a constant under Object, i.e. a permanent GC root in the
        // process-lifetime state; instance ids are monotonic, so it must be removed.
        if (!m_ClassName.empty() && mrb_class_defined(mrb, m_ClassName.c_str())) {
            mrb_const_remove(mrb, mrb_obj_value(mrb->object_class),
                             mrb_intern_cstr(mrb, m_ClassName.c_str()));
        }
    }

    host.UnregisterInstance(m_InstanceId);

    m_HasInstance = false;
    m_Instance = mrb_nil_value();
    m_ClassName.clear();
    m_InstanceId = -1;
    m_Initialized = false;
}

void MRubyEnvironment::Update(float deltaTime) {
    // The coroutine/await scheduler is process-global and pumped exactly once per
    // frame by the SceneManager via MRubyHost::Pump; the per-instance tick is a
    // no-op so coroutines are not advanced once per script.
    (void)deltaTime;
}

void MRubyEnvironment::RegisterCustomComponentTypes() {
    // Ruby classes live in the shared mrb_state, so this is host-wide rather than
    // per-instance.
    MRubyHost::Instance().RegisterCustomComponentTypes();
}

void MRubyEnvironment::EnsureInstance() {
    mrb_state* mrb = MRubyHost::Instance().State();
    if (!mrb || m_ClassName.empty()) {
        return;
    }
    if (!mrb_class_defined(mrb, m_ClassName.c_str())) {
        return;
    }
    struct RClass* klass = mrb_class_get(mrb, m_ClassName.c_str());
    if (!klass) {
        return;
    }

    int arena = mrb_gc_arena_save(mrb);
    if (m_HasInstance) {
        mrb_gc_unregister(mrb, m_Instance);
        m_HasInstance = false;
    }
    // The instance's `initialize` (if the script defined one) runs here, so make
    // this script's API/owner active for it.
    MRubyContextScope ctx(m_ScriptAPI, m_InstanceId);
    m_Instance = mrb_obj_new(mrb, klass, 0, nullptr);
    if (mrb->exc) {
        const std::string error = TakeMRubyError(mrb);
        LOG_ERROR(LogCategory::Scripting, "mRuby script '{}' failed to instantiate: {}",
                  m_ClassName, error);
        m_Instance = mrb_nil_value();
    } else {
        mrb_gc_register(mrb, m_Instance);
        m_HasInstance = true;
    }
    mrb_gc_arena_restore(mrb, arena);
}

ScriptResult MRubyEnvironment::ExecuteFile(const std::string& filepath) {
    if (!m_Initialized) {
        return ScriptResult(false, "mRuby environment not initialized");
    }

    std::string script;

    // Check if running from pack file first
    auto& packFS = platform::PackFileSystem::Instance();
    if (packFS.isPackMode() && packFS.exists(filepath)) {
        script = packFS.readFileAsString(filepath);
        if (script.empty()) {
            return ScriptResult(false, "Failed to read script from pack: " + filepath);
        }
    } else {
        // Fall back to filesystem
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return ScriptResult(false, "Failed to open file: " + filepath);
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        script = buffer.str();
        file.close();
    }

    return ExecuteString(script);
}

ScriptResult MRubyEnvironment::ExecuteString(const std::string& script) {
    if (!m_Initialized) {
        return ScriptResult(false, "mRuby environment not initialized");
    }

    mrb_state* mrb = MRubyHost::Instance().State();

    // Load the script body as the body of this instance's wrapper class, so its
    // `def`s become per-instance methods that never collide with another script's.
    // `Lupine.*` (a top-level constant) and $self/$lupine_api (globals) resolve
    // unchanged from inside the class body. Top-level code in the body runs once,
    // in class context, with this script's API/owner active.
    std::string wrapped = "class " + m_ClassName + "\n" + script + "\nend\n";

    MRubyContextScope ctx(m_ScriptAPI, m_InstanceId);
    const int arena = mrb_gc_arena_save(mrb);
    mrbc_context* context = mrbc_context_new(mrb);
    mrb_load_string_cxt(mrb, wrapped.c_str(), context);
    mrbc_context_free(mrb, context);

    if (mrb->exc) {
        ScriptResult failure = HandleMRubyError();
        mrb_gc_arena_restore(mrb, arena);
        return failure;
    }
    mrb_gc_arena_restore(mrb, arena);

    // Build (or rebuild, on reload) the live instance now its methods exist.
    EnsureInstance();
    if (!m_HasInstance) {
        return ScriptResult(false, "Failed to instantiate script class " + m_ClassName);
    }

    return ScriptResult(true);
}

ScriptResult MRubyEnvironment::CallFunction(const std::string& functionName) {
    if (!m_Initialized || !m_HasInstance) {
        return ScriptResult(false, "mRuby environment not initialized");
    }

    if (!HasFunction(functionName)) {
        return ScriptResult(false, "Function '" + functionName + "' not found");
    }

    mrb_state* mrb = MRubyHost::Instance().State();
    MRubyContextScope ctx(m_ScriptAPI, m_InstanceId);

    // mrb_funcall_argv arena-protects its return value, so a per-frame lifecycle call
    // (on_update, ...) permanently roots one object per frame unless bracketed.
    const int arena = mrb_gc_arena_save(mrb);
    mrb_sym sym = mrb_intern_cstr(mrb, functionName.c_str());
    mrb_funcall_argv(mrb, m_Instance, sym, 0, nullptr);

    if (mrb->exc) {
        ScriptResult failure = HandleMRubyError();
        mrb_gc_arena_restore(mrb, arena);
        return failure;
    }

    mrb_gc_arena_restore(mrb, arena);
    return ScriptResult(true);
}

bool MRubyEnvironment::HasFunction(const std::string& functionName) const {
    if (!m_Initialized || !m_HasInstance) {
        return false;
    }

    // Check if this script's instance responds to the method.
    mrb_state* mrb = MRubyHost::Instance().State();
    mrb_sym sym = mrb_intern_cstr(mrb, functionName.c_str());
    return mrb_respond_to(mrb, m_Instance, sym);
}

// Bare-name variable writes (delta_time, export properties, ...) keep mRuby's
// existing global-variable semantics; the engine re-pushes them immediately
// before each lifecycle call.
// A global variable is a GC root in its own right, so the arena slot the boxed value
// takes on the way in must be released - these run per script per frame.
void MRubyEnvironment::SetGlobal(const std::string& name, int value) {
    if (!m_Initialized) return;
    mrb_state* mrb = MRubyHost::Instance().State();
    const int arena = mrb_gc_arena_save(mrb);
    mrb_sym sym = mrb_intern_cstr(mrb, name.c_str());
    mrb_gv_set(mrb, sym, mrb_fixnum_value(value));
    mrb_gc_arena_restore(mrb, arena);
}

void MRubyEnvironment::SetGlobal(const std::string& name, float value) {
    if (!m_Initialized) return;
    mrb_state* mrb = MRubyHost::Instance().State();
    const int arena = mrb_gc_arena_save(mrb);
    mrb_sym sym = mrb_intern_cstr(mrb, name.c_str());
    mrb_gv_set(mrb, sym, mrb_float_value(mrb, value));
    mrb_gc_arena_restore(mrb, arena);
}

void MRubyEnvironment::SetGlobal(const std::string& name, const std::string& value) {
    if (!m_Initialized) return;
    mrb_state* mrb = MRubyHost::Instance().State();
    const int arena = mrb_gc_arena_save(mrb);
    mrb_sym sym = mrb_intern_cstr(mrb, name.c_str());
    mrb_gv_set(mrb, sym, mrb_str_new_cstr(mrb, value.c_str()));
    mrb_gc_arena_restore(mrb, arena);
}

void MRubyEnvironment::SetGlobal(const std::string& name, bool value) {
    if (!m_Initialized) return;
    mrb_state* mrb = MRubyHost::Instance().State();
    mrb_sym sym = mrb_intern_cstr(mrb, name.c_str());
    mrb_gv_set(mrb, sym, value ? mrb_true_value() : mrb_false_value());
}

// Autoload singletons bind into the shared state so every instance resolves the
// same live node by name.
void MRubyEnvironment::SetGlobalNode(const std::string& name, core::Node* node) {
    if (!m_Initialized || !m_ScriptAPI) return;
    MRubyContextScope ctx(m_ScriptAPI, m_InstanceId);
    MRubyHost::Instance().SetSharedGlobalNode(name, node);
}

int MRubyEnvironment::GetGlobalInt(const std::string& name, int defaultValue) {
    if (!m_Initialized) return defaultValue;
    mrb_state* mrb = MRubyHost::Instance().State();
    mrb_sym sym = mrb_intern_cstr(mrb, name.c_str());
    mrb_value val = mrb_gv_get(mrb, sym);

    if (mrb_nil_p(val)) return defaultValue;

    if (mrb_fixnum_p(val)) {
        return static_cast<int>(mrb_fixnum(val));
    } else if (mrb_float_p(val)) {
        return static_cast<int>(mrb_float(val));
    }

    return defaultValue;
}

float MRubyEnvironment::GetGlobalFloat(const std::string& name, float defaultValue) {
    if (!m_Initialized) return defaultValue;
    mrb_state* mrb = MRubyHost::Instance().State();
    mrb_sym sym = mrb_intern_cstr(mrb, name.c_str());
    mrb_value val = mrb_gv_get(mrb, sym);

    if (mrb_nil_p(val)) return defaultValue;

    if (mrb_float_p(val)) {
        return static_cast<float>(mrb_float(val));
    } else if (mrb_fixnum_p(val)) {
        return static_cast<float>(mrb_fixnum(val));
    }

    return defaultValue;
}

std::string MRubyEnvironment::GetGlobalString(const std::string& name, const std::string& defaultValue) {
    if (!m_Initialized) return defaultValue;
    mrb_state* mrb = MRubyHost::Instance().State();
    mrb_sym sym = mrb_intern_cstr(mrb, name.c_str());
    mrb_value val = mrb_gv_get(mrb, sym);

    if (mrb_nil_p(val)) return defaultValue;

    if (mrb_string_p(val)) {
        return std::string(RSTRING_PTR(val), RSTRING_LEN(val));
    }

    return defaultValue;
}

bool MRubyEnvironment::GetGlobalBool(const std::string& name, bool defaultValue) {
    if (!m_Initialized) return defaultValue;
    mrb_state* mrb = MRubyHost::Instance().State();
    mrb_sym sym = mrb_intern_cstr(mrb, name.c_str());
    mrb_value val = mrb_gv_get(mrb, sym);

    if (mrb_nil_p(val)) return defaultValue;

    return mrb_test(val);
}

void MRubyEnvironment::SetScriptAPI(ScriptAPI* api) {
    // The API/owner are made active per-dispatch via MRubyContextScope, so this
    // only needs to remember which API this instance dispatches with - and keep the
    // host registry (used to resume this instance's coroutines) in step.
    m_ScriptAPI = api;
    MRubyHost::Instance().RegisterInstance(m_InstanceId, api);
}

ScriptResult MRubyEnvironment::HandleMRubyError() {
    mrb_state* mrb = MRubyHost::Instance().State();
    if (!mrb || !mrb->exc) {
        return ScriptResult(false, "Unknown mRuby error");
    }

    const std::string error = "mRuby error: " + TakeMRubyError(mrb);
    m_LastError = error;

    return ScriptResult(false, error);
}

static mrb_value JsonToMrb(mrb_state* mrb, const nlohmann::json& json) {
    if (json.is_boolean()) {
        return json.get<bool>() ? mrb_true_value() : mrb_false_value();
    }
    if (json.is_number_integer()) {
        return mrb_fixnum_value(static_cast<mrb_int>(json.get<int64_t>()));
    }
    if (json.is_number()) {
        return mrb_float_value(mrb, json.get<double>());
    }
    if (json.is_string()) {
        std::string value = json.get<std::string>();
        return mrb_str_new(mrb, value.c_str(), value.size());
    }
    if (json.is_array()) {
        mrb_value array = mrb_ary_new_capa(mrb, static_cast<mrb_int>(json.size()));
        for (const nlohmann::json& item : json) {
            mrb_ary_push(mrb, array, JsonToMrb(mrb, item));
        }
        return array;
    }
    if (json.is_object()) {
        mrb_value hash = mrb_hash_new(mrb);
        for (nlohmann::json::const_iterator it = json.begin(); it != json.end(); ++it) {
            mrb_value key = mrb_str_new(mrb, it.key().c_str(), it.key().size());
            mrb_hash_set(mrb, hash, key, JsonToMrb(mrb, it.value()));
        }
        return hash;
    }
    return mrb_nil_value();
}

static nlohmann::json MrbToJson(mrb_state* mrb, mrb_value value) {
    if (mrb_nil_p(value)) {
        return nlohmann::json(nullptr);
    }
    if (mrb_fixnum_p(value)) {
        return static_cast<int64_t>(mrb_fixnum(value));
    }
    if (mrb_float_p(value)) {
        return static_cast<double>(mrb_float(value));
    }
    if (mrb_string_p(value)) {
        return std::string(RSTRING_PTR(value), RSTRING_LEN(value));
    }
    if (mrb_array_p(value)) {
        nlohmann::json array = nlohmann::json::array();
        mrb_int len = RARRAY_LEN(value);
        for (mrb_int i = 0; i < len; ++i) {
            array.push_back(MrbToJson(mrb, mrb_ary_ref(mrb, value, i)));
        }
        return array;
    }
    if (mrb_hash_p(value)) {
        nlohmann::json object = nlohmann::json::object();
        mrb_value keys = mrb_hash_keys(mrb, value);
        mrb_int len = RARRAY_LEN(keys);
        for (mrb_int i = 0; i < len; ++i) {
            mrb_value key = mrb_ary_ref(mrb, keys, i);
            mrb_value item = mrb_hash_get(mrb, value, key);
            std::string keyStr;
            if (mrb_string_p(key)) {
                keyStr.assign(RSTRING_PTR(key), RSTRING_LEN(key));
            } else {
                mrb_value keyAsString = mrb_obj_as_string(mrb, key);
                keyStr.assign(RSTRING_PTR(keyAsString), RSTRING_LEN(keyAsString));
            }
            object[keyStr] = MrbToJson(mrb, item);
        }
        return object;
    }
    return nlohmann::json(static_cast<bool>(mrb_test(value)));
}

void MRubyEnvironment::SetGlobalJson(const std::string& name, const nlohmann::json& value) {
    if (!m_Initialized) return;
    mrb_state* mrb = MRubyHost::Instance().State();
    const int arena = mrb_gc_arena_save(mrb);
    mrb_sym sym = mrb_intern_cstr(mrb, name.c_str());
    mrb_gv_set(mrb, sym, JsonToMrb(mrb, value));
    mrb_gc_arena_restore(mrb, arena);
}

nlohmann::json MRubyEnvironment::GetGlobalJson(const std::string& name, const nlohmann::json& defaultValue) {
    if (!m_Initialized) return defaultValue;
    mrb_state* mrb = MRubyHost::Instance().State();
    mrb_sym sym = mrb_intern_cstr(mrb, name.c_str());
    mrb_value val = mrb_gv_get(mrb, sym);
    if (mrb_nil_p(val)) return defaultValue;

    // MrbToJson allocates (hash key arrays, to_s results) while walking the value.
    const int arena = mrb_gc_arena_save(mrb);
    nlohmann::json result = MrbToJson(mrb, val);
    mrb_gc_arena_restore(mrb, arena);
    return result;
}

// Generic accessors for any-typed globals (structured types arrive as hashes/arrays).
static mrb_value mrb_script_api_get_global(mrb_state* mrb, mrb_value self) {
    (void)self;
    char* name;
    mrb_value defaultValue = mrb_nil_value();
    mrb_get_args(mrb, "z|o", &name, &defaultValue);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        nlohmann::json def = mrb_nil_p(defaultValue) ? nlohmann::json() : MrbToJson(mrb, defaultValue);
        return JsonToMrb(mrb, api->GetGlobalValue(name, def));
    }

    return defaultValue;
}

static mrb_value mrb_script_api_set_global(mrb_state* mrb, mrb_value self) {
    (void)self;
    char* name;
    mrb_value value;
    mrb_get_args(mrb, "zo", &name, &value);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetGlobalValue(name, MrbToJson(mrb, value));
    }

    return mrb_nil_value();
}

static mrb_value ColorToMrb(mrb_state* mrb, const math::Color& color);

static mrb_value mrb_script_api_color_from_hex(mrb_state* mrb, mrb_value) {
    char* hex;
    mrb_get_args(mrb, "z", &hex);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return ColorToMrb(mrb, api->ColorFromHex(hex));
    }
    return ColorToMrb(mrb, math::Color());
}

static mrb_value mrb_script_api_color_to_hex(mrb_state* mrb, mrb_value) {
    mrb_float r, g, b, a;
    mrb_get_args(mrb, "ffff", &r, &g, &b, &a);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        std::string hex = api->ColorToHex(mrbColor(r, g, b, a));
        return mrb_str_new_cstr(mrb, hex.c_str());
    }
    return mrb_str_new_cstr(mrb, "");
}

static mrb_value mrb_script_api_color_from_hsv(mrb_state* mrb, mrb_value) {
    mrb_float h, s, v, a = 1.0;
    mrb_get_args(mrb, "fff|f", &h, &s, &v, &a);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return ColorToMrb(mrb, api->ColorFromHSV(static_cast<float>(h), static_cast<float>(s),
                                                 static_cast<float>(v), static_cast<float>(a)));
    }
    return ColorToMrb(mrb, math::Color());
}

static mrb_value mrb_script_api_color_lerp(mrb_state* mrb, mrb_value) {
    mrb_float r1, g1, b1, a1, r2, g2, b2, a2, t;
    mrb_get_args(mrb, "fffffffff", &r1, &g1, &b1, &a1, &r2, &g2, &b2, &a2, &t);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        math::Color from(static_cast<float>(r1), static_cast<float>(g1), static_cast<float>(b1), static_cast<float>(a1));
        math::Color to(static_cast<float>(r2), static_cast<float>(g2), static_cast<float>(b2), static_cast<float>(a2));
        return ColorToMrb(mrb, api->ColorLerp(from, to, static_cast<float>(t)));
    }
    return ColorToMrb(mrb, math::Color());
}

static mrb_value mrb_script_api_sample_gradient(mrb_state* mrb, mrb_value) {
    mrb_value gradient;
    mrb_float t;
    mrb_get_args(mrb, "of", &gradient, &t);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return ColorToMrb(mrb, api->SampleGradient(MrbToJson(mrb, gradient), static_cast<float>(t)));
    }
    return ColorToMrb(mrb, math::Color());
}

static mrb_value mrb_script_api_sample_curve(mrb_state* mrb, mrb_value) {
    mrb_value curve;
    mrb_float t;
    mrb_get_args(mrb, "of", &curve, &t);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->SampleCurve(MrbToJson(mrb, curve), static_cast<float>(t)) : 0.0f);
}

static mrb_value mrb_script_api_call_archetype(mrb_state* mrb, mrb_value) {
    const char* path = nullptr;
    const char* method = nullptr;
    mrb_value* rest = nullptr;
    mrb_int restLen = 0;
    mrb_get_args(mrb, "zz*", &path, &method, &rest, &restLen);

    nlohmann::json args = nlohmann::json::array();
    for (mrb_int i = 0; i < restLen; ++i) {
        args.push_back(MrbToJson(mrb, rest[i]));
    }

    nlohmann::json result = core::ArchetypeRuntime::GetInstance().CallMethod(
        path ? path : "", method ? method : "", args);
    return JsonToMrb(mrb, result);
}

static mrb_value mrb_script_api_load_archetype(mrb_state* mrb, mrb_value) {
    const char* path = nullptr;
    mrb_get_args(mrb, "z", &path);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api && path) {
        asset::ArchetypeInstance* instance = api->LoadArchetype(path);
        if (instance) {
            return JsonToMrb(mrb, instance->GetResolvedFields());
        }
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_archetype_field(mrb_state* mrb, mrb_value) {
    const char* path = nullptr;
    const char* name = nullptr;
    mrb_get_args(mrb, "zz", &path, &name);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api && path && name) {
        asset::ArchetypeInstance* instance = api->LoadArchetype(path);
        if (instance) {
            return JsonToMrb(mrb, instance->GetFieldJson(name));
        }
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_archetype_class(mrb_state* mrb, mrb_value) {
    const char* path = nullptr;
    mrb_get_args(mrb, "z", &path);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api && path) {
        asset::ArchetypeInstance* instance = api->LoadArchetype(path);
        if (instance) {
            std::string className = instance->GetArchetypeClass();
            return mrb_str_new(mrb, className.c_str(), className.size());
        }
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_archetype_is_a(mrb_state* mrb, mrb_value) {
    const char* path = nullptr;
    const char* className = nullptr;
    mrb_get_args(mrb, "zz", &path, &className);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api && path && className) {
        asset::ArchetypeInstance* instance = api->LoadArchetype(path);
        if (instance) {
            return instance->IsArchetype(className) ? mrb_true_value() : mrb_false_value();
        }
    }
    return mrb_false_value();
}

static mrb_value mrb_script_api_load_archetype_async(mrb_state* mrb, mrb_value) {
    const char* path = nullptr;
    const char* callback = nullptr;
    mrb_int priority = ScriptAPI::ASYNC_PRIORITY_NORMAL;
    mrb_get_args(mrb, "z|zi", &path, &callback, &priority);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api && path) {
        uint64_t handle = api->LoadArchetypeAsync(path, callback ? callback : "", static_cast<int>(priority));
        return mrb_fixnum_value(static_cast<mrb_int>(handle));
    }
    return mrb_fixnum_value(0);
}

static mrb_value mrb_script_api_load_archetype_definition_async(mrb_state* mrb, mrb_value) {
    const char* path = nullptr;
    const char* callback = nullptr;
    mrb_int priority = ScriptAPI::ASYNC_PRIORITY_NORMAL;
    mrb_get_args(mrb, "z|zi", &path, &callback, &priority);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api && path) {
        uint64_t handle = api->LoadArchetypeDefinitionAsync(path, callback ? callback : "", static_cast<int>(priority));
        return mrb_fixnum_value(static_cast<mrb_int>(handle));
    }
    return mrb_fixnum_value(0);
}

static mrb_value mrb_script_api_set_archetype_load_priority(mrb_state* mrb, mrb_value) {
    mrb_int handle = 0;
    mrb_int priority = ScriptAPI::ASYNC_PRIORITY_NORMAL;
    mrb_get_args(mrb, "ii", &handle, &priority);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetAsyncLoadPriority(static_cast<uint64_t>(handle), static_cast<int>(priority));
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_archetype_load_priority(mrb_state* mrb, mrb_value) {
    mrb_int handle = 0;
    mrb_get_args(mrb, "i", &handle);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return mrb_fixnum_value(api->GetAsyncLoadPriority(static_cast<uint64_t>(handle)));
    }
    return mrb_fixnum_value(0);
}

static mrb_value mrb_script_api_set_archetype_streaming_budget(mrb_state* mrb, mrb_value) {
    mrb_int maxConcurrent = 0;
    mrb_get_args(mrb, "i", &maxConcurrent);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetAsyncStreamingBudget(static_cast<int>(maxConcurrent));
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_archetype_streaming_budget(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return mrb_fixnum_value(api->GetAsyncStreamingBudget());
    }
    return mrb_fixnum_value(0);
}

static mrb_value mrb_script_api_get_archetype_inflight_count(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return mrb_fixnum_value(api->GetAsyncInFlightCount());
    }
    return mrb_fixnum_value(0);
}

static mrb_value mrb_script_api_get_archetype_queued_count(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return mrb_fixnum_value(api->GetAsyncQueuedCount());
    }
    return mrb_fixnum_value(0);
}

static mrb_value mrb_script_api_get_archetype_load_status(mrb_state* mrb, mrb_value) {
    mrb_int handle = 0;
    mrb_get_args(mrb, "i", &handle);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return mrb_fixnum_value(api->GetAsyncLoadStatus(static_cast<uint64_t>(handle)));
    }
    return mrb_fixnum_value(4);
}

static mrb_value mrb_script_api_is_archetype_load_complete(mrb_state* mrb, mrb_value) {
    mrb_int handle = 0;
    mrb_get_args(mrb, "i", &handle);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return api->IsAsyncLoadComplete(static_cast<uint64_t>(handle)) ? mrb_true_value() : mrb_false_value();
    }
    return mrb_true_value();
}

static mrb_value mrb_script_api_get_async_archetype(mrb_state* mrb, mrb_value) {
    mrb_int handle = 0;
    mrb_get_args(mrb, "i", &handle);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        asset::ArchetypeInstance* instance = api->GetAsyncArchetype(static_cast<uint64_t>(handle));
        if (instance) {
            return JsonToMrb(mrb, instance->GetResolvedFields());
        }
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_cancel_archetype_load(mrb_state* mrb, mrb_value) {
    mrb_int handle = 0;
    mrb_get_args(mrb, "i", &handle);

    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->CancelAsyncLoad(static_cast<uint64_t>(handle));
    }
    return mrb_nil_value();
}

nlohmann::json MRubyEnvironment::CallMethod(const std::string& functionName,
                                            const nlohmann::json& selfData,
                                            const nlohmann::json& args) {
    if (!m_Initialized || !m_HasInstance) {
        return nlohmann::json(nullptr);
    }

    mrb_state* mrb = MRubyHost::Instance().State();
    MRubyContextScope ctx(m_ScriptAPI, m_InstanceId);

    mrb_sym sym = mrb_intern_cstr(mrb, functionName.c_str());
    if (!mrb_respond_to(mrb, m_Instance, sym)) {
        return nlohmann::json(nullptr);
    }

    // The arguments and the result only need to survive until the result has been
    // converted back to JSON, so the whole dispatch is arena-bracketed.
    const int arena = mrb_gc_arena_save(mrb);

    std::vector<mrb_value> argv;
    argv.push_back(JsonToMrb(mrb, selfData));
    if (args.is_array()) {
        for (const nlohmann::json& arg : args) {
            argv.push_back(JsonToMrb(mrb, arg));
        }
    }

    mrb_value result = mrb_funcall_argv(mrb, m_Instance, sym,
                                        static_cast<mrb_int>(argv.size()), argv.data());

    if (mrb->exc) {
        const std::string error = TakeMRubyError(mrb);
        mrb_gc_arena_restore(mrb, arena);
        LOG_ERROR(LogCategory::Scripting, "Archetype method '{}' raised an error: {}",
                  functionName, error);
        return nlohmann::json(nullptr);
    }

    nlohmann::json converted = MrbToJson(mrb, result);
    mrb_gc_arena_restore(mrb, arena);
    return converted;
}

// ===========================================================================
// Node object model (Godot-style scriptable node/component handles)
// ===========================================================================

static void noderef_free(mrb_state*, void* ptr) {
    delete static_cast<NodeRef*>(ptr);
}
static void componentref_free(mrb_state*, void* ptr) {
    delete static_cast<ComponentRef*>(ptr);
}

static const mrb_data_type NodeRefDataType = { "LupineNode", noderef_free };
static const mrb_data_type ComponentRefDataType = { "LupineComponent", componentref_free };

static mrb_value WrapNodeRef(mrb_state* mrb, const NodeRef& ref) {
    if (!ref.IsValid()) return mrb_nil_value();
    if (!mrb_class_defined(mrb, "LupineNode")) return mrb_nil_value();
    struct RClass* cls = mrb_class_get(mrb, "LupineNode");
    NodeRef* heap = new NodeRef(ref);
    return mrb_obj_value(Data_Wrap_Struct(mrb, cls, &NodeRefDataType, heap));
}

static mrb_value WrapComponentRef(mrb_state* mrb, const ComponentRef& ref) {
    if (!ref.IsValid()) return mrb_nil_value();
    if (!mrb_class_defined(mrb, "LupineComponent")) return mrb_nil_value();
    struct RClass* cls = mrb_class_get(mrb, "LupineComponent");
    ComponentRef* heap = new ComponentRef(ref);
    return mrb_obj_value(Data_Wrap_Struct(mrb, cls, &ComponentRefDataType, heap));
}

// Convert a single signal argument, delivering node-encoded args as node handles.
static mrb_value SignalArgToMrb(mrb_state* mrb, const nlohmann::json& arg, ScriptAPI* api) {
    if (arg.is_object() && arg.contains("__lupine_node__") && api) {
        std::string uuid = arg["__lupine_node__"].get<std::string>();
        core::Node* node = api->FindNodeByUUID(uuid);
        return WrapNodeRef(mrb, NodeRef::FromRaw(node, api));
    }
    return JsonToMrb(mrb, arg);
}

bool MRubyEnvironment::CallFunctionArgs(const std::string& functionName, const nlohmann::json& args) {
    if (!m_Initialized || !m_HasInstance) {
        return false;
    }

    mrb_state* mrb = MRubyHost::Instance().State();
    MRubyContextScope ctx(m_ScriptAPI, m_InstanceId);

    mrb_sym sym = mrb_intern_cstr(mrb, functionName.c_str());
    if (!mrb_respond_to(mrb, m_Instance, sym)) {
        return false;
    }

    // Signal args (node handles, JSON payloads) are only needed for the duration of
    // the dispatch; the callee holds them on the value stack while it runs.
    const int arena = mrb_gc_arena_save(mrb);

    std::vector<mrb_value> argv;
    if (args.is_array()) {
        for (const nlohmann::json& arg : args) {
            argv.push_back(SignalArgToMrb(mrb, arg, m_ScriptAPI));
        }
    }

    mrb_funcall_argv(mrb, m_Instance, sym,
                     static_cast<mrb_int>(argv.size()),
                     argv.empty() ? nullptr : argv.data());

    if (mrb->exc) {
        const std::string error = TakeMRubyError(mrb);
        mrb_gc_arena_restore(mrb, arena);
        LOG_ERROR(LogCategory::Scripting, "Signal handler '{}' raised an error: {}",
                  functionName, error);
        return false;
    }

    mrb_gc_arena_restore(mrb, arena);
    return true;
}

nlohmann::json MRubyEnvironment::CallFunctionResult(const std::string& functionName,
                                                    const nlohmann::json& args) {
    if (!m_Initialized || !m_HasInstance) {
        return nlohmann::json(nullptr);
    }

    mrb_state* mrb = MRubyHost::Instance().State();
    MRubyContextScope ctx(m_ScriptAPI, m_InstanceId);

    mrb_sym sym = mrb_intern_cstr(mrb, functionName.c_str());
    if (!mrb_respond_to(mrb, m_Instance, sym)) {
        return nlohmann::json(nullptr);
    }

    const int arena = mrb_gc_arena_save(mrb);

    std::vector<mrb_value> argv;
    if (args.is_array()) {
        for (const nlohmann::json& arg : args) {
            argv.push_back(SignalArgToMrb(mrb, arg, m_ScriptAPI));
        }
    }

    mrb_value result = mrb_funcall_argv(mrb, m_Instance, sym,
                                        static_cast<mrb_int>(argv.size()),
                                        argv.empty() ? nullptr : argv.data());

    if (mrb->exc) {
        const std::string error = TakeMRubyError(mrb);
        mrb_gc_arena_restore(mrb, arena);
        LOG_ERROR(LogCategory::Scripting, "Script method '{}' raised an error: {}",
                  functionName, error);
        return nlohmann::json(nullptr);
    }

    // Convert before restoring the arena: the result is only rooted until then.
    nlohmann::json converted = MrbToJson(mrb, result);
    mrb_gc_arena_restore(mrb, arena);
    return converted;
}

// The `self` of a handle method is only ever produced by the Wrap* helpers, but a
// script can reach these methods with a `dup`/`clone`d receiver (data == NULL) or, with
// MRB_TT_DATA unset, with a plain object. mrb_data_get_ptr raises a clean TypeError
// instead of reinterpreting foreign memory as a NodeRef.
template <typename T>
static T* SelfData(mrb_value self, const mrb_data_type* type) {
    mrb_state* mrb = MRubyHost::Instance().State();
    void* ptr = mrb_data_get_ptr(mrb, self, type);
    if (!ptr) {
        mrb_raise(mrb, E_TYPE_ERROR, "uninitialized Lupine handle");
    }
    return static_cast<T*>(ptr);
}

static NodeRef* SelfNode(mrb_value self) {
    return SelfData<NodeRef>(self, &NodeRefDataType);
}
static ComponentRef* SelfComp(mrb_value self) {
    return SelfData<ComponentRef>(self, &ComponentRefDataType);
}
static NodeRef* ArgNode(mrb_value v) {
    if (mrb_type(v) != MRB_TT_DATA) return nullptr;
    if (DATA_TYPE(v) != &NodeRefDataType) return nullptr;
    return static_cast<NodeRef*>(DATA_PTR(v));
}
static ComponentRef* ArgComp(mrb_value v) {
    if (mrb_type(v) != MRB_TT_DATA) return nullptr;
    if (DATA_TYPE(v) != &ComponentRefDataType) return nullptr;
    return static_cast<ComponentRef*>(DATA_PTR(v));
}

static mrb_value Vec2ToMrb(mrb_state* mrb, const math::Vec2& v) {
    mrb_value a = mrb_ary_new_capa(mrb, 2);
    mrb_ary_push(mrb, a, mrb_float_value(mrb, v.x));
    mrb_ary_push(mrb, a, mrb_float_value(mrb, v.y));
    return a;
}
static mrb_value Vec3ToMrb(mrb_state* mrb, const math::Vec3& v) {
    mrb_value a = mrb_ary_new_capa(mrb, 3);
    mrb_ary_push(mrb, a, mrb_float_value(mrb, v.x));
    mrb_ary_push(mrb, a, mrb_float_value(mrb, v.y));
    mrb_ary_push(mrb, a, mrb_float_value(mrb, v.z));
    return a;
}
static mrb_value ColorToMrb(mrb_state* mrb, const math::Color& c) {
    mrb_value a = mrb_ary_new_capa(mrb, 4);
    mrb_ary_push(mrb, a, mrb_float_value(mrb, c.r));
    mrb_ary_push(mrb, a, mrb_float_value(mrb, c.g));
    mrb_ary_push(mrb, a, mrb_float_value(mrb, c.b));
    mrb_ary_push(mrb, a, mrb_float_value(mrb, c.a));
    return a;
}

// --- Node methods ----------------------------------------------------------

static mrb_value mrb_node_is_valid(mrb_state*, mrb_value self) {
    return mrb_bool_value(SelfNode(self)->IsValid());
}
static mrb_value mrb_node_get_name(mrb_state* mrb, mrb_value self) {
    return mrb_str_new_cstr(mrb, SelfNode(self)->GetName().c_str());
}
static mrb_value mrb_node_set_name(mrb_state* mrb, mrb_value self) {
    char* name; mrb_get_args(mrb, "z", &name);
    SelfNode(self)->SetName(name);
    return mrb_nil_value();
}
static mrb_value mrb_node_get_uuid(mrb_state* mrb, mrb_value self) {
    return mrb_str_new_cstr(mrb, SelfNode(self)->GetUUID().c_str());
}
static mrb_value mrb_node_get_path(mrb_state* mrb, mrb_value self) {
    return mrb_str_new_cstr(mrb, SelfNode(self)->GetPath().c_str());
}
static mrb_value mrb_node_get_type_name(mrb_state* mrb, mrb_value self) {
    return mrb_str_new_cstr(mrb, SelfNode(self)->GetTypeName().c_str());
}
static mrb_value mrb_node_is_active(mrb_state*, mrb_value self) {
    return mrb_bool_value(SelfNode(self)->IsActive());
}
static mrb_value mrb_node_set_active(mrb_state* mrb, mrb_value self) {
    mrb_bool b; mrb_get_args(mrb, "b", &b);
    SelfNode(self)->SetActive(b);
    return mrb_nil_value();
}
static mrb_value mrb_node_is_visible(mrb_state*, mrb_value self) {
    return mrb_bool_value(SelfNode(self)->IsVisible());
}
static mrb_value mrb_node_set_visible(mrb_state* mrb, mrb_value self) {
    mrb_bool b; mrb_get_args(mrb, "b", &b);
    SelfNode(self)->SetVisible(b);
    return mrb_nil_value();
}
static mrb_value mrb_node_is_unique(mrb_state*, mrb_value self) {
    return mrb_bool_value(SelfNode(self)->IsUniqueNameInOwner());
}
static mrb_value mrb_node_set_unique(mrb_state* mrb, mrb_value self) {
    mrb_bool b; mrb_get_args(mrb, "b", &b);
    SelfNode(self)->SetUniqueNameInOwner(b);
    return mrb_nil_value();
}
static mrb_value mrb_node_get_child_count(mrb_state*, mrb_value self) {
    return mrb_fixnum_value(SelfNode(self)->GetChildCount());
}
static mrb_value mrb_node_has_node(mrb_state* mrb, mrb_value self) {
    char* path; mrb_get_args(mrb, "z", &path);
    return mrb_bool_value(SelfNode(self)->HasNode(path));
}
static mrb_value mrb_node_has_component(mrb_state* mrb, mrb_value self) {
    char* type; mrb_get_args(mrb, "z", &type);
    return mrb_bool_value(SelfNode(self)->HasComponent(type));
}
static mrb_value mrb_node_has_property(mrb_state* mrb, mrb_value self) {
    char* prop; mrb_get_args(mrb, "z", &prop);
    return mrb_bool_value(SelfNode(self)->HasProperty(prop));
}
static mrb_value mrb_node_add_to_group(mrb_state* mrb, mrb_value self) {
    char* group; mrb_get_args(mrb, "z", &group);
    SelfNode(self)->AddToGroup(group);
    return mrb_nil_value();
}
static mrb_value mrb_node_remove_from_group(mrb_state* mrb, mrb_value self) {
    char* group; mrb_get_args(mrb, "z", &group);
    SelfNode(self)->RemoveFromGroup(group);
    return mrb_nil_value();
}
static mrb_value mrb_node_is_in_group(mrb_state* mrb, mrb_value self) {
    char* group; mrb_get_args(mrb, "z", &group);
    return mrb_bool_value(SelfNode(self)->IsInGroup(group));
}
static mrb_value mrb_node_get_groups(mrb_state* mrb, mrb_value self) {
    std::vector<std::string> groups = SelfNode(self)->GetGroups();
    mrb_value a = mrb_ary_new_capa(mrb, static_cast<mrb_int>(groups.size()));
    for (const std::string& g : groups) {
        mrb_ary_push(mrb, a, mrb_str_new_cstr(mrb, g.c_str()));
    }
    return a;
}
static mrb_value mrb_node_implements_interface(mrb_state* mrb, mrb_value self) {
    char* name; mrb_get_args(mrb, "z", &name);
    return mrb_bool_value(SelfNode(self)->ImplementsInterface(name));
}
static mrb_value mrb_node_get_interfaces(mrb_state* mrb, mrb_value self) {
    std::vector<std::string> interfaces = SelfNode(self)->GetInterfaces();
    mrb_value a = mrb_ary_new_capa(mrb, static_cast<mrb_int>(interfaces.size()));
    for (const std::string& i : interfaces) {
        mrb_ary_push(mrb, a, mrb_str_new_cstr(mrb, i.c_str()));
    }
    return a;
}
static mrb_value mrb_node_verify_interface(mrb_state* mrb, mrb_value self) {
    char* name; mrb_get_args(mrb, "z", &name);
    return JsonToMrb(mrb, SelfNode(self)->VerifyInterface(name));
}
static mrb_value mrb_node_queue_free(mrb_state*, mrb_value self) {
    SelfNode(self)->QueueFree();
    return mrb_nil_value();
}
static mrb_value mrb_node_queue_free_deferred(mrb_state*, mrb_value self) {
    SelfNode(self)->QueueFreeDeferred();
    return mrb_nil_value();
}
static mrb_value mrb_node_free(mrb_state*, mrb_value self) {
    SelfNode(self)->Free();
    return mrb_nil_value();
}
static mrb_value mrb_node_get_parent(mrb_state* mrb, mrb_value self) {
    return WrapNodeRef(mrb, SelfNode(self)->GetParent());
}
static mrb_value mrb_node_get_child(mrb_state* mrb, mrb_value self) {
    char* name; mrb_get_args(mrb, "z", &name);
    return WrapNodeRef(mrb, SelfNode(self)->GetChild(name));
}
static mrb_value mrb_node_get_child_at(mrb_state* mrb, mrb_value self) {
    mrb_int index; mrb_get_args(mrb, "i", &index);
    return WrapNodeRef(mrb, SelfNode(self)->GetChildAt(static_cast<int>(index)));
}
static mrb_value mrb_node_get_children(mrb_state* mrb, mrb_value self) {
    std::vector<NodeRef> children = SelfNode(self)->GetChildren();
    mrb_value a = mrb_ary_new_capa(mrb, static_cast<mrb_int>(children.size()));
    for (const NodeRef& c : children) {
        mrb_ary_push(mrb, a, WrapNodeRef(mrb, c));
    }
    return a;
}
static mrb_value mrb_node_find_node(mrb_state* mrb, mrb_value self) {
    char* path; mrb_get_args(mrb, "z", &path);
    return WrapNodeRef(mrb, SelfNode(self)->FindNode(path));
}
static mrb_value mrb_node_add_child(mrb_state* mrb, mrb_value self) {
    mrb_value other; mrb_get_args(mrb, "o", &other);
    NodeRef* child = ArgNode(other);
    if (child) SelfNode(self)->AddChild(*child);
    return mrb_nil_value();
}
static mrb_value mrb_node_remove_child(mrb_state* mrb, mrb_value self) {
    mrb_value other; mrb_get_args(mrb, "o", &other);
    NodeRef* child = ArgNode(other);
    if (child) SelfNode(self)->RemoveChild(*child);
    return mrb_nil_value();
}
static mrb_value mrb_node_reparent_to(mrb_state* mrb, mrb_value self) {
    mrb_value other; mrb_get_args(mrb, "o", &other);
    NodeRef* parent = ArgNode(other);
    if (parent) SelfNode(self)->ReparentTo(*parent);
    return mrb_nil_value();
}
static mrb_value mrb_node_duplicate(mrb_state* mrb, mrb_value self) {
    return WrapNodeRef(mrb, SelfNode(self)->Duplicate());
}
static mrb_value mrb_node_get_sibling_index(mrb_state*, mrb_value self) {
    return mrb_fixnum_value(SelfNode(self)->GetSiblingIndex());
}
static mrb_value mrb_node_set_sibling_index(mrb_state* mrb, mrb_value self) {
    mrb_int index; mrb_get_args(mrb, "i", &index);
    SelfNode(self)->SetSiblingIndex(static_cast<int>(index));
    return mrb_nil_value();
}
static mrb_value mrb_node_get_child_index(mrb_state* mrb, mrb_value self) {
    mrb_value other; mrb_get_args(mrb, "o", &other);
    NodeRef* child = ArgNode(other);
    return mrb_fixnum_value(child ? SelfNode(self)->GetChildIndex(*child) : -1);
}
static mrb_value mrb_node_move_child(mrb_state* mrb, mrb_value self) {
    mrb_value other; mrb_int index;
    mrb_get_args(mrb, "oi", &other, &index);
    NodeRef* child = ArgNode(other);
    if (child) SelfNode(self)->MoveChild(*child, static_cast<int>(index));
    return mrb_nil_value();
}
static mrb_value mrb_node_create_tween(mrb_state* mrb, mrb_value self) {
    char* channel; mrb_value toVal; mrb_float duration; char* easing = nullptr;
    mrb_get_args(mrb, "zof|z", &channel, &toVal, &duration, &easing);
    return WrapTweenRef(mrb, SelfNode(self)->CreateTween(channel, MrbToJson(mrb, toVal),
                                                         static_cast<float>(duration),
                                                         easing ? easing : "linear"));
}
static mrb_value mrb_node_create_sequence(mrb_state* mrb, mrb_value self) {
    return WrapSequenceRef(mrb, SelfNode(self)->CreateSequence());
}
static mrb_value mrb_node_await_signal(mrb_state* mrb, mrb_value self) {
    char* signal; mrb_get_args(mrb, "z", &signal);
    return WrapSignalAwaiter(mrb, SelfNode(self)->AwaitSignal(signal));
}
static mrb_value mrb_node_distance_to(mrb_state* mrb, mrb_value self) {
    mrb_value other; mrb_get_args(mrb, "o", &other);
    NodeRef* target = ArgNode(other);
    if (!target) return mrb_float_value(mrb, 0.0);
    return mrb_float_value(mrb, SelfNode(self)->DistanceTo(*target));
}
static mrb_value mrb_node_get_position_2d(mrb_state* mrb, mrb_value self) {
    return Vec2ToMrb(mrb, SelfNode(self)->GetPosition2D());
}
static mrb_value mrb_node_set_position_2d(mrb_state* mrb, mrb_value self) {
    mrb_float x, y; mrb_get_args(mrb, "ff", &x, &y);
    SelfNode(self)->SetPosition2D(static_cast<float>(x), static_cast<float>(y));
    return mrb_nil_value();
}
static mrb_value mrb_node_translate_2d(mrb_state* mrb, mrb_value self) {
    mrb_float x, y; mrb_get_args(mrb, "ff", &x, &y);
    SelfNode(self)->Translate2D(static_cast<float>(x), static_cast<float>(y));
    return mrb_nil_value();
}
static mrb_value mrb_node_get_rotation_2d(mrb_state* mrb, mrb_value self) {
    return mrb_float_value(mrb, SelfNode(self)->GetRotation2D());
}
static mrb_value mrb_node_set_rotation_2d(mrb_state* mrb, mrb_value self) {
    mrb_float d; mrb_get_args(mrb, "f", &d);
    SelfNode(self)->SetRotation2D(static_cast<float>(d));
    return mrb_nil_value();
}
static mrb_value mrb_node_get_scale_2d(mrb_state* mrb, mrb_value self) {
    return Vec2ToMrb(mrb, SelfNode(self)->GetScale2D());
}
static mrb_value mrb_node_set_scale_2d(mrb_state* mrb, mrb_value self) {
    mrb_float x, y; mrb_get_args(mrb, "ff", &x, &y);
    SelfNode(self)->SetScale2D(static_cast<float>(x), static_cast<float>(y));
    return mrb_nil_value();
}
static mrb_value mrb_node_get_position_3d(mrb_state* mrb, mrb_value self) {
    return Vec3ToMrb(mrb, SelfNode(self)->GetPosition3D());
}
static mrb_value mrb_node_set_position_3d(mrb_state* mrb, mrb_value self) {
    mrb_float x, y, z; mrb_get_args(mrb, "fff", &x, &y, &z);
    SelfNode(self)->SetPosition3D(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
    return mrb_nil_value();
}
static mrb_value mrb_node_translate_3d(mrb_state* mrb, mrb_value self) {
    mrb_float x, y, z; mrb_get_args(mrb, "fff", &x, &y, &z);
    SelfNode(self)->Translate3D(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
    return mrb_nil_value();
}
static mrb_value mrb_node_get_rotation_3d(mrb_state* mrb, mrb_value self) {
    return Vec3ToMrb(mrb, SelfNode(self)->GetRotation3D());
}
static mrb_value mrb_node_set_rotation_3d(mrb_state* mrb, mrb_value self) {
    mrb_float x, y, z; mrb_get_args(mrb, "fff", &x, &y, &z);
    SelfNode(self)->SetRotation3D(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
    return mrb_nil_value();
}
static mrb_value mrb_node_get_scale_3d(mrb_state* mrb, mrb_value self) {
    return Vec3ToMrb(mrb, SelfNode(self)->GetScale3D());
}
static mrb_value mrb_node_set_scale_3d(mrb_state* mrb, mrb_value self) {
    mrb_float x, y, z; mrb_get_args(mrb, "fff", &x, &y, &z);
    SelfNode(self)->SetScale3D(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
    return mrb_nil_value();
}
static mrb_value mrb_node_get_global_position_2d(mrb_state* mrb, mrb_value self) {
    return Vec2ToMrb(mrb, SelfNode(self)->GetGlobalPosition2D());
}
static mrb_value mrb_node_set_global_position_2d(mrb_state* mrb, mrb_value self) {
    mrb_float x, y; mrb_get_args(mrb, "ff", &x, &y);
    SelfNode(self)->SetGlobalPosition2D(static_cast<float>(x), static_cast<float>(y));
    return mrb_nil_value();
}
static mrb_value mrb_node_get_global_rotation_2d(mrb_state* mrb, mrb_value self) {
    return mrb_float_value(mrb, SelfNode(self)->GetGlobalRotation2D());
}
static mrb_value mrb_node_set_global_rotation_2d(mrb_state* mrb, mrb_value self) {
    mrb_float degrees; mrb_get_args(mrb, "f", &degrees);
    SelfNode(self)->SetGlobalRotation2D(static_cast<float>(degrees));
    return mrb_nil_value();
}
static mrb_value mrb_node_get_global_scale_2d(mrb_state* mrb, mrb_value self) {
    return Vec2ToMrb(mrb, SelfNode(self)->GetGlobalScale2D());
}
static mrb_value mrb_node_get_global_position_3d(mrb_state* mrb, mrb_value self) {
    return Vec3ToMrb(mrb, SelfNode(self)->GetGlobalPosition3D());
}
static mrb_value mrb_node_set_global_position_3d(mrb_state* mrb, mrb_value self) {
    mrb_float x, y, z; mrb_get_args(mrb, "fff", &x, &y, &z);
    SelfNode(self)->SetGlobalPosition3D(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
    return mrb_nil_value();
}
static mrb_value mrb_node_get_global_rotation_3d(mrb_state* mrb, mrb_value self) {
    return Vec3ToMrb(mrb, SelfNode(self)->GetGlobalRotation3D());
}
static mrb_value mrb_node_set_global_rotation_3d(mrb_state* mrb, mrb_value self) {
    mrb_float pitch, yaw, roll; mrb_get_args(mrb, "fff", &pitch, &yaw, &roll);
    SelfNode(self)->SetGlobalRotation3D(static_cast<float>(pitch), static_cast<float>(yaw),
                                        static_cast<float>(roll));
    return mrb_nil_value();
}
static mrb_value mrb_node_get_global_scale_3d(mrb_state* mrb, mrb_value self) {
    return Vec3ToMrb(mrb, SelfNode(self)->GetGlobalScale3D());
}
static mrb_value mrb_node_get_component(mrb_state* mrb, mrb_value self) {
    char* type; mrb_get_args(mrb, "z", &type);
    return WrapComponentRef(mrb, SelfNode(self)->GetComponent(type));
}
static mrb_value mrb_node_get_components(mrb_state* mrb, mrb_value self) {
    char* type; mrb_get_args(mrb, "z", &type);
    std::vector<ComponentRef> comps = SelfNode(self)->GetComponents(type);
    mrb_value a = mrb_ary_new_capa(mrb, static_cast<mrb_int>(comps.size()));
    for (const ComponentRef& c : comps) {
        mrb_ary_push(mrb, a, WrapComponentRef(mrb, c));
    }
    return a;
}
static mrb_value mrb_node_add_component(mrb_state* mrb, mrb_value self) {
    char* type; mrb_get_args(mrb, "z", &type);
    return WrapComponentRef(mrb, SelfNode(self)->AddComponent(type));
}
static mrb_value mrb_node_remove_component(mrb_state* mrb, mrb_value self) {
    mrb_value other; mrb_get_args(mrb, "o", &other);
    ComponentRef* comp = ArgComp(other);
    if (comp) SelfNode(self)->RemoveComponent(*comp);
    return mrb_nil_value();
}
static mrb_value mrb_node_get(mrb_state* mrb, mrb_value self) {
    char* key; mrb_get_args(mrb, "z", &key);
    return JsonToMrb(mrb, SelfNode(self)->Get(key));
}
static mrb_value mrb_node_set(mrb_state* mrb, mrb_value self) {
    char* key; mrb_value value;
    mrb_get_args(mrb, "zo", &key, &value);
    SelfNode(self)->Set(key, MrbToJson(mrb, value));
    return mrb_nil_value();
}
static mrb_value mrb_node_has_method(mrb_state* mrb, mrb_value self) {
    char* method; mrb_get_args(mrb, "z", &method);
    return mrb_bool_value(SelfNode(self)->HasMethod(method));
}

// --- Component methods -----------------------------------------------------

static mrb_value mrb_comp_is_valid(mrb_state*, mrb_value self) {
    return mrb_bool_value(SelfComp(self)->IsValid());
}
static mrb_value mrb_comp_get_type_name(mrb_state* mrb, mrb_value self) {
    return mrb_str_new_cstr(mrb, SelfComp(self)->GetTypeName().c_str());
}
static mrb_value mrb_comp_get_name(mrb_state* mrb, mrb_value self) {
    return mrb_str_new_cstr(mrb, SelfComp(self)->GetName().c_str());
}
static mrb_value mrb_comp_is_enabled(mrb_state*, mrb_value self) {
    return mrb_bool_value(SelfComp(self)->IsEnabled());
}
static mrb_value mrb_comp_set_enabled(mrb_state* mrb, mrb_value self) {
    mrb_bool b; mrb_get_args(mrb, "b", &b);
    SelfComp(self)->SetEnabled(b);
    return mrb_nil_value();
}
static mrb_value mrb_comp_has_property(mrb_state* mrb, mrb_value self) {
    char* prop; mrb_get_args(mrb, "z", &prop);
    return mrb_bool_value(SelfComp(self)->HasProperty(prop));
}
static mrb_value mrb_comp_get(mrb_state* mrb, mrb_value self) {
    char* key; mrb_get_args(mrb, "z", &key);
    return JsonToMrb(mrb, SelfComp(self)->Get(key));
}
static mrb_value mrb_comp_set(mrb_state* mrb, mrb_value self) {
    char* key; mrb_value value;
    mrb_get_args(mrb, "zo", &key, &value);
    SelfComp(self)->Set(key, MrbToJson(mrb, value));
    return mrb_nil_value();
}
static mrb_value mrb_comp_get_owner(mrb_state* mrb, mrb_value self) {
    return WrapNodeRef(mrb, SelfComp(self)->GetOwner());
}
static mrb_value mrb_comp_is_instance_of(mrb_state* mrb, mrb_value self) {
    char* typeName; mrb_get_args(mrb, "z", &typeName);
    std::shared_ptr<core::Component> comp = SelfComp(self)->Lock();
    return mrb_bool_value(comp ? comp->IsInstanceOf(typeName) : false);
}
static mrb_value mrb_comp_get_type_chain(mrb_state* mrb, mrb_value self) {
    std::shared_ptr<core::Component> comp = SelfComp(self)->Lock();
    mrb_value arr = mrb_ary_new(mrb);
    if (comp) {
        for (const std::string& chainEntry : comp->GetTypeChain()) {
            mrb_ary_push(mrb, arr, mrb_str_new_cstr(mrb, chainEntry.c_str()));
        }
    }
    return arr;
}
static mrb_value mrb_comp_await_signal(mrb_state* mrb, mrb_value self) {
    char* signal; mrb_get_args(mrb, "z", &signal);
    return WrapSignalAwaiter(mrb, SelfComp(self)->AwaitSignal(signal));
}

// --- Lupine module-level node lookup ---------------------------------------

static mrb_value mrb_script_api_get_node(mrb_state* mrb, mrb_value) {
    char* path; mrb_get_args(mrb, "z", &path);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_nil_value();
    return WrapNodeRef(mrb, NodeRef::FromRaw(api->FindNode(path), api));
}
static mrb_value mrb_script_api_find_node_by_uuid(mrb_state* mrb, mrb_value) {
    char* uuid; mrb_get_args(mrb, "z", &uuid);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_nil_value();
    return WrapNodeRef(mrb, NodeRef::FromRaw(api->FindNodeByUUID(uuid), api));
}
static mrb_value mrb_script_api_get_self(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_nil_value();
    return WrapNodeRef(mrb, NodeRef::FromRaw(api->GetSelf(), api));
}
static mrb_value mrb_script_api_get_singleton(mrb_state* mrb, mrb_value) {
    char* name; mrb_get_args(mrb, "z", &name);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_nil_value();
    return WrapNodeRef(mrb, NodeRef::FromRaw(api->GetSingleton(name), api));
}

// --- Signals (node/component handle methods) -------------------------------

// `rest` points into the VM's value stack, which MrbToJson can grow (a Ruby-defined
// `to_s` reached through mrb_obj_as_string reallocates it), leaving the pointer stale
// for the remaining elements - so snapshot the values first. The values themselves stay
// reachable from the (possibly moved) stack, so they remain GC-safe.
static nlohmann::json MrbRestToJson(mrb_state* mrb, mrb_value* rest, mrb_int restLen) {
    const std::vector<mrb_value> values(rest, rest + restLen);

    nlohmann::json args = nlohmann::json::array();
    for (std::vector<mrb_value>::const_iterator it = values.begin(); it != values.end(); ++it) {
        args.push_back(MrbToJson(mrb, *it));
    }
    return args;
}

static mrb_value mrb_node_call(mrb_state* mrb, mrb_value self) {
    char* method; mrb_value* rest; mrb_int restLen;
    mrb_get_args(mrb, "z*", &method, &rest, &restLen);
    return JsonToMrb(mrb, SelfNode(self)->Call(method, MrbRestToJson(mrb, rest, restLen)));
}

static mrb_value mrb_node_emit(mrb_state* mrb, mrb_value self) {
    char* signal; mrb_value* rest; mrb_int restLen;
    mrb_get_args(mrb, "z*", &signal, &rest, &restLen);
    SelfNode(self)->EmitSignal(signal, MrbRestToJson(mrb, rest, restLen));
    return mrb_nil_value();
}
static mrb_value mrb_node_rpc(mrb_state* mrb, mrb_value self) {
    char* method; mrb_value* rest; mrb_int restLen;
    mrb_get_args(mrb, "z*", &method, &rest, &restLen);
    SelfNode(self)->Rpc(method, MrbRestToJson(mrb, rest, restLen));
    return mrb_nil_value();
}
static mrb_value mrb_node_rpc_id(mrb_state* mrb, mrb_value self) {
    mrb_int peer; char* method; mrb_value* rest; mrb_int restLen;
    mrb_get_args(mrb, "iz*", &peer, &method, &rest, &restLen);
    SelfNode(self)->RpcId(static_cast<uint32_t>(peer), method, MrbRestToJson(mrb, rest, restLen));
    return mrb_nil_value();
}
static mrb_value mrb_node_rpc_unreliable(mrb_state* mrb, mrb_value self) {
    char* method; mrb_value* rest; mrb_int restLen;
    mrb_get_args(mrb, "z*", &method, &rest, &restLen);
    SelfNode(self)->RpcUnreliable(method, MrbRestToJson(mrb, rest, restLen));
    return mrb_nil_value();
}
static mrb_value mrb_node_set_authority(mrb_state* mrb, mrb_value self) {
    mrb_int peer;
    mrb_get_args(mrb, "i", &peer);
    SelfNode(self)->SetMultiplayerAuthority(static_cast<uint32_t>(peer));
    return mrb_nil_value();
}
static mrb_value mrb_node_get_authority(mrb_state* mrb, mrb_value self) {
    (void)mrb;
    return mrb_fixnum_value(static_cast<mrb_int>(SelfNode(self)->GetMultiplayerAuthority()));
}
static mrb_value mrb_node_is_authority(mrb_state* mrb, mrb_value self) {
    (void)mrb;
    return mrb_bool_value(SelfNode(self)->IsMultiplayerAuthority());
}
static mrb_value mrb_node_get_network_id(mrb_state* mrb, mrb_value self) {
    (void)mrb;
    return mrb_fixnum_value(static_cast<mrb_int>(SelfNode(self)->GetNetworkId()));
}
static mrb_value mrb_node_connect(mrb_state* mrb, mrb_value self) {
    char* signal; mrb_value target; char* method; mrb_int flags = 0;
    mrb_get_args(mrb, "zoz|i", &signal, &target, &method, &flags);
    NodeRef* t = ArgNode(target);
    if (!t) return mrb_fixnum_value(0);
    uint64_t id = SelfNode(self)->ConnectSignal(signal, *t, method, static_cast<uint32_t>(flags));
    return mrb_fixnum_value(static_cast<mrb_int>(id));
}
static mrb_value mrb_node_disconnect(mrb_state* mrb, mrb_value self) {
    char* signal; mrb_int id;
    mrb_get_args(mrb, "zi", &signal, &id);
    SelfNode(self)->DisconnectSignal(signal, static_cast<uint64_t>(id));
    return mrb_nil_value();
}
static mrb_value mrb_node_is_connected(mrb_state* mrb, mrb_value self) {
    char* signal; mrb_get_args(mrb, "z", &signal);
    return mrb_bool_value(SelfNode(self)->IsSignalConnected(signal));
}
static mrb_value mrb_node_add_user_signal(mrb_state* mrb, mrb_value self) {
    char* name; mrb_get_args(mrb, "z", &name);
    SelfNode(self)->AddUserSignal(name);
    return mrb_nil_value();
}
static mrb_value mrb_node_get_signal_list(mrb_state* mrb, mrb_value self) {
    mrb_value arr = mrb_ary_new(mrb);
    for (const std::string& name : SelfNode(self)->GetSignalList()) {
        mrb_ary_push(mrb, arr, mrb_str_new_cstr(mrb, name.c_str()));
    }
    return arr;
}

// --- Camera control methods (Camera2D / CameraUI) ---------------------------

static mrb_value mrb_node_camera_shake(mrb_state* mrb, mrb_value self) {
    mrb_float amplitude, duration, frequency = 30.0;
    mrb_get_args(mrb, "ff|f", &amplitude, &duration, &frequency);
    std::shared_ptr<core::Node> node = SelfNode(self)->Lock();
    if (auto cam2d = std::dynamic_pointer_cast<core::Camera2D>(node)) {
        cam2d->Shake(static_cast<float>(amplitude), static_cast<float>(duration), static_cast<float>(frequency));
    } else if (auto camui = std::dynamic_pointer_cast<core::CameraUI>(node)) {
        camui->Shake(static_cast<float>(amplitude), static_cast<float>(duration), static_cast<float>(frequency));
    }
    return mrb_nil_value();
}

static mrb_value mrb_node_camera_stop_shake(mrb_state*, mrb_value self) {
    std::shared_ptr<core::Node> node = SelfNode(self)->Lock();
    if (auto cam2d = std::dynamic_pointer_cast<core::Camera2D>(node)) {
        cam2d->StopShake();
    } else if (auto camui = std::dynamic_pointer_cast<core::CameraUI>(node)) {
        camui->StopShake();
    }
    return mrb_nil_value();
}

static mrb_value mrb_node_camera_is_shaking(mrb_state*, mrb_value self) {
    std::shared_ptr<core::Node> node = SelfNode(self)->Lock();
    if (auto cam2d = std::dynamic_pointer_cast<core::Camera2D>(node)) {
        return mrb_bool_value(cam2d->IsShaking());
    }
    if (auto camui = std::dynamic_pointer_cast<core::CameraUI>(node)) {
        return mrb_bool_value(camui->IsShaking());
    }
    return mrb_bool_value(false);
}

static mrb_value mrb_node_camera_set_follow_target(mrb_state* mrb, mrb_value self) {
    mrb_value other; mrb_get_args(mrb, "o", &other);
    std::shared_ptr<core::Node> node = SelfNode(self)->Lock();
    std::shared_ptr<core::Node> tgt;
    NodeRef* targetRef = ArgNode(other);
    if (targetRef) {
        tgt = targetRef->Lock();
    }
    if (auto cam2d = std::dynamic_pointer_cast<core::Camera2D>(node)) {
        cam2d->SetFollowTarget(tgt);
    } else if (auto camui = std::dynamic_pointer_cast<core::CameraUI>(node)) {
        camui->SetFollowTarget(tgt);
    }
    return mrb_nil_value();
}

static mrb_value mrb_node_camera_clear_follow_target(mrb_state*, mrb_value self) {
    std::shared_ptr<core::Node> node = SelfNode(self)->Lock();
    if (auto cam2d = std::dynamic_pointer_cast<core::Camera2D>(node)) {
        cam2d->ClearFollowTarget();
    } else if (auto camui = std::dynamic_pointer_cast<core::CameraUI>(node)) {
        camui->ClearFollowTarget();
    }
    return mrb_nil_value();
}

static mrb_value mrb_node_camera_smooth_move_to(mrb_state* mrb, mrb_value self) {
    mrb_float x, y, speed = 0.0;
    mrb_get_args(mrb, "ff|f", &x, &y, &speed);
    std::shared_ptr<core::Node> node = SelfNode(self)->Lock();
    if (auto camui = std::dynamic_pointer_cast<core::CameraUI>(node)) {
        camui->SmoothMoveTo(mrbVec2(x, y), static_cast<float>(speed));
    }
    return mrb_nil_value();
}

static mrb_value mrb_node_camera_get_effective_position(mrb_state* mrb, mrb_value self) {
    std::shared_ptr<core::Node> node = SelfNode(self)->Lock();
    if (auto cam2d = std::dynamic_pointer_cast<core::Camera2D>(node)) {
        return Vec2ToMrb(mrb, cam2d->GetEffectivePosition());
    }
    if (auto camui = std::dynamic_pointer_cast<core::CameraUI>(node)) {
        return Vec2ToMrb(mrb, camui->GetEffectivePosition());
    }
    return Vec2ToMrb(mrb, math::Vec2(0.0f, 0.0f));
}

// --- Particle methods (Particles2D / Particles3D) ---------------------------

static mrb_value mrb_node_particles_restart(mrb_state*, mrb_value self) {
    std::shared_ptr<core::Node> node = SelfNode(self)->Lock();
    if (!node) return mrb_nil_value();
    if (auto p2d = node->GetComponent<components::Particles2D>()) {
        p2d->Restart();
    } else if (auto p3d = node->GetComponent<components::Particles3D>()) {
        p3d->Restart();
    }
    return mrb_nil_value();
}

static mrb_value mrb_node_particles_emit_burst(mrb_state* mrb, mrb_value self) {
    mrb_int count; mrb_get_args(mrb, "i", &count);
    std::shared_ptr<core::Node> node = SelfNode(self)->Lock();
    if (!node) return mrb_nil_value();
    if (auto p2d = node->GetComponent<components::Particles2D>()) {
        p2d->EmitBurst(static_cast<int>(count));
    } else if (auto p3d = node->GetComponent<components::Particles3D>()) {
        p3d->EmitBurst(static_cast<int>(count));
    }
    return mrb_nil_value();
}

static mrb_value mrb_node_particles_set_emitting(mrb_state* mrb, mrb_value self) {
    mrb_bool emitting; mrb_get_args(mrb, "b", &emitting);
    std::shared_ptr<core::Node> node = SelfNode(self)->Lock();
    if (!node) return mrb_nil_value();
    if (auto p2d = node->GetComponent<components::Particles2D>()) {
        p2d->SetEmitting(emitting);
    } else if (auto p3d = node->GetComponent<components::Particles3D>()) {
        p3d->SetEmitting(emitting);
    }
    return mrb_nil_value();
}

static mrb_value mrb_node_particles_is_emitting(mrb_state*, mrb_value self) {
    std::shared_ptr<core::Node> node = SelfNode(self)->Lock();
    if (!node) return mrb_bool_value(false);
    if (auto p2d = node->GetComponent<components::Particles2D>()) {
        return mrb_bool_value(p2d->GetEmitting());
    }
    if (auto p3d = node->GetComponent<components::Particles3D>()) {
        return mrb_bool_value(p3d->GetEmitting());
    }
    return mrb_bool_value(false);
}

static mrb_value mrb_node_particles_get_alive_count(mrb_state*, mrb_value self) {
    std::shared_ptr<core::Node> node = SelfNode(self)->Lock();
    if (!node) return mrb_fixnum_value(0);
    if (auto p2d = node->GetComponent<components::Particles2D>()) {
        return mrb_fixnum_value(p2d->GetAliveCount());
    }
    if (auto p3d = node->GetComponent<components::Particles3D>()) {
        return mrb_fixnum_value(p3d->GetAliveCount());
    }
    return mrb_fixnum_value(0);
}

// --- Theme methods (UIControl) ----------------------------------------------

static mrb_value mrb_node_set_theme(mrb_state* mrb, mrb_value self) {
    char* path; mrb_get_args(mrb, "z", &path);
    std::shared_ptr<core::Node> node = SelfNode(self)->Lock();
    if (node) {
        std::shared_ptr<components::UIControl> ui = node->GetComponent<components::UIControl>();
        if (ui) {
            ui->SetThemePath(path);
        }
    }
    return mrb_nil_value();
}

static mrb_value mrb_node_set_theme_type_variation(mrb_state* mrb, mrb_value self) {
    char* variation; mrb_get_args(mrb, "z", &variation);
    std::shared_ptr<core::Node> node = SelfNode(self)->Lock();
    if (node) {
        std::shared_ptr<components::UIControl> ui = node->GetComponent<components::UIControl>();
        if (ui) {
            ui->SetThemeTypeVariation(variation);
        }
    }
    return mrb_nil_value();
}

static mrb_value mrb_node_clear_theme_override(mrb_state* mrb, mrb_value self) {
    char* property; mrb_get_args(mrb, "z", &property);
    std::shared_ptr<core::Node> node = SelfNode(self)->Lock();
    if (node) {
        std::shared_ptr<components::UIControl> ui = node->GetComponent<components::UIControl>();
        if (ui) {
            ui->SetThemeOverride(property, false);
        }
    }
    return mrb_nil_value();
}

static mrb_value mrb_comp_emit(mrb_state* mrb, mrb_value self) {
    char* signal; mrb_value* rest; mrb_int restLen;
    mrb_get_args(mrb, "z*", &signal, &rest, &restLen);
    SelfComp(self)->EmitSignal(signal, MrbRestToJson(mrb, rest, restLen));
    return mrb_nil_value();
}
static mrb_value mrb_comp_call(mrb_state* mrb, mrb_value self) {
    char* method; mrb_value* rest; mrb_int restLen;
    mrb_get_args(mrb, "z*", &method, &rest, &restLen);
    return JsonToMrb(mrb, SelfComp(self)->Call(method, MrbRestToJson(mrb, rest, restLen)));
}
static mrb_value mrb_comp_connect(mrb_state* mrb, mrb_value self) {
    char* signal; mrb_value target; char* method; mrb_int flags = 0;
    mrb_get_args(mrb, "zoz|i", &signal, &target, &method, &flags);
    NodeRef* t = ArgNode(target);
    if (!t) return mrb_fixnum_value(0);
    uint64_t id = SelfComp(self)->ConnectSignal(signal, *t, method, static_cast<uint32_t>(flags));
    return mrb_fixnum_value(static_cast<mrb_int>(id));
}
static mrb_value mrb_comp_disconnect(mrb_state* mrb, mrb_value self) {
    char* signal; mrb_int id;
    mrb_get_args(mrb, "zi", &signal, &id);
    SelfComp(self)->DisconnectSignal(signal, static_cast<uint64_t>(id));
    return mrb_nil_value();
}
static mrb_value mrb_comp_is_connected(mrb_state* mrb, mrb_value self) {
    char* signal; mrb_get_args(mrb, "z", &signal);
    return mrb_bool_value(SelfComp(self)->IsSignalConnected(signal));
}
static mrb_value mrb_comp_add_user_signal(mrb_state* mrb, mrb_value self) {
    char* name; mrb_get_args(mrb, "z", &name);
    SelfComp(self)->AddUserSignal(name);
    return mrb_nil_value();
}
static mrb_value mrb_comp_get_signal_list(mrb_state* mrb, mrb_value self) {
    mrb_value arr = mrb_ary_new(mrb);
    for (const std::string& name : SelfComp(self)->GetSignalList()) {
        mrb_ary_push(mrb, arr, mrb_str_new_cstr(mrb, name.c_str()));
    }
    return arr;
}

// --- Signals & events (Lupine module functions) ----------------------------

static mrb_value mrb_script_api_emit(mrb_state* mrb, mrb_value) {
    char* signal; mrb_value* rest; mrb_int restLen;
    mrb_get_args(mrb, "z*", &signal, &rest, &restLen);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->EmitSignal(signal, MrbRestToJson(mrb, rest, restLen));
    return mrb_nil_value();
}
static mrb_value mrb_script_api_connect(mrb_state* mrb, mrb_value) {
    char* signal; mrb_value target; char* method; mrb_int flags = 0;
    mrb_get_args(mrb, "zoz|i", &signal, &target, &method, &flags);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    NodeRef* t = ArgNode(target);
    if (api && t) {
        auto node = t->Lock();
        return mrb_fixnum_value(static_cast<mrb_int>(
            api->ConnectSignal(signal, node.get(), method, static_cast<uint32_t>(flags))));
    }
    return mrb_fixnum_value(0);
}
static mrb_value mrb_script_api_disconnect(mrb_state* mrb, mrb_value) {
    char* signal; mrb_int id;
    mrb_get_args(mrb, "zi", &signal, &id);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->DisconnectSignal(signal, static_cast<uint64_t>(id));
    return mrb_nil_value();
}
static mrb_value mrb_script_api_is_connected(mrb_state* mrb, mrb_value) {
    char* signal; mrb_get_args(mrb, "z", &signal);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_bool_value(api ? api->IsSignalConnected(signal) : false);
}
static mrb_value mrb_script_api_add_user_signal(mrb_state* mrb, mrb_value) {
    char* name; mrb_get_args(mrb, "z", &name);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->AddUserSignal(name);
    return mrb_nil_value();
}
static mrb_value mrb_script_api_call_deferred(mrb_state* mrb, mrb_value) {
    char* method; mrb_value* rest; mrb_int restLen;
    mrb_get_args(mrb, "z*", &method, &rest, &restLen);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->CallDeferred(method, MrbRestToJson(mrb, rest, restLen));
    return mrb_nil_value();
}
static mrb_value mrb_script_api_emit_event(mrb_state* mrb, mrb_value) {
    char* event; mrb_value* rest; mrb_int restLen;
    mrb_get_args(mrb, "z*", &event, &rest, &restLen);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->EmitEvent(event, MrbRestToJson(mrb, rest, restLen));
    return mrb_nil_value();
}
static mrb_value mrb_script_api_subscribe(mrb_state* mrb, mrb_value) {
    char* event; char* method; mrb_int flags = 0;
    mrb_get_args(mrb, "zz|i", &event, &method, &flags);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return mrb_fixnum_value(static_cast<mrb_int>(
            api->SubscribeEvent(event, method, static_cast<uint32_t>(flags))));
    }
    return mrb_fixnum_value(0);
}
static mrb_value mrb_script_api_unsubscribe(mrb_state* mrb, mrb_value) {
    char* event; mrb_int id;
    mrb_get_args(mrb, "zi", &event, &id);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->UnsubscribeEvent(event, static_cast<uint64_t>(id));
    return mrb_nil_value();
}

// ============================================================================
// LIFECYCLE (module-level self helpers)
// ============================================================================

static mrb_value mrb_script_api_free_self(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->FreeSelf();
    return mrb_nil_value();
}

static mrb_value mrb_script_api_queue_free_deferred_self(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->QueueFreeDeferredSelf();
    return mrb_nil_value();
}

// ============================================================================
// INPUT (parity)
// ============================================================================

static mrb_value mrb_script_api_is_action_just_released(mrb_state* mrb, mrb_value) {
    char* action; mrb_int player = -1;
    mrb_get_args(mrb, "z|i", &action, &player);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->IsActionJustReleased(action, static_cast<int>(player))) ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_get_action_strength(mrb_state* mrb, mrb_value) {
    char* action; mrb_int player = -1;
    mrb_get_args(mrb, "z|i", &action, &player);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->GetActionStrength(action, static_cast<int>(player)) : 0.0f);
}

static mrb_value mrb_script_api_get_vector(mrb_state* mrb, mrb_value) {
    char* negX; char* posX; char* negY; char* posY; mrb_int player = -1;
    mrb_get_args(mrb, "zzzz|i", &negX, &posX, &negY, &posY, &player);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) return Vec2ToMrb(mrb, api->GetVector(negX, posX, negY, posY, static_cast<int>(player)));
    return Vec2ToMrb(mrb, math::Vec2(0.0f, 0.0f));
}

static mrb_value mrb_script_api_get_mouse_delta(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return Vec2ToMrb(mrb, api ? api->GetMouseDelta() : math::Vec2(0.0f, 0.0f));
}

static mrb_value mrb_script_api_get_mouse_scroll_delta(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return Vec2ToMrb(mrb, api ? api->GetMouseScrollDelta() : math::Vec2(0.0f, 0.0f));
}

static mrb_value mrb_script_api_is_mouse_button_just_released(mrb_state* mrb, mrb_value) {
    mrb_int button;
    mrb_get_args(mrb, "i", &button);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->IsMouseButtonJustReleased(static_cast<int>(button))) ? mrb_true_value() : mrb_false_value();
}

// ============================================================================
// GAMEPAD + VIBRATION (parity)
// ============================================================================

static mrb_value mrb_script_api_is_gamepad_connected(mrb_state* mrb, mrb_value) {
    mrb_int gamepadId = 0;
    mrb_get_args(mrb, "|i", &gamepadId);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->IsGamepadConnected(static_cast<int>(gamepadId))) ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_get_gamepad_count(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_fixnum_value(api ? api->GetGamepadCount() : 0);
}

static mrb_value mrb_script_api_get_connected_gamepad_ids(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    mrb_value arr = mrb_ary_new(mrb);
    if (api) {
        std::vector<int> ids = api->GetConnectedGamepadIds();
        for (int id : ids) {
            mrb_ary_push(mrb, arr, mrb_fixnum_value(id));
        }
    }
    return arr;
}

static mrb_value mrb_script_api_get_gamepad_name(mrb_state* mrb, mrb_value) {
    mrb_int gamepadId = 0;
    mrb_get_args(mrb, "|i", &gamepadId);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_str_new_cstr(mrb, api ? api->GetGamepadName(static_cast<int>(gamepadId)).c_str() : "");
}

static mrb_value mrb_script_api_is_gamepad_button_pressed(mrb_state* mrb, mrb_value) {
    mrb_int button; mrb_int gamepadId = 0;
    mrb_get_args(mrb, "i|i", &button, &gamepadId);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->IsGamepadButtonPressed(static_cast<int>(button), static_cast<int>(gamepadId)))
        ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_is_gamepad_button_just_pressed(mrb_state* mrb, mrb_value) {
    mrb_int button; mrb_int gamepadId = 0;
    mrb_get_args(mrb, "i|i", &button, &gamepadId);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->IsGamepadButtonJustPressed(static_cast<int>(button), static_cast<int>(gamepadId)))
        ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_is_gamepad_button_just_released(mrb_state* mrb, mrb_value) {
    mrb_int button; mrb_int gamepadId = 0;
    mrb_get_args(mrb, "i|i", &button, &gamepadId);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->IsGamepadButtonJustReleased(static_cast<int>(button), static_cast<int>(gamepadId)))
        ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_get_gamepad_axis(mrb_state* mrb, mrb_value) {
    mrb_int axis; mrb_int gamepadId = 0;
    mrb_get_args(mrb, "i|i", &axis, &gamepadId);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->GetGamepadAxis(static_cast<int>(axis), static_cast<int>(gamepadId)) : 0.0f);
}

static mrb_value mrb_script_api_set_gamepad_vibration(mrb_state* mrb, mrb_value) {
    mrb_int gamepadId; mrb_float left; mrb_float right; mrb_float duration = 0.0;
    mrb_get_args(mrb, "iff|f", &gamepadId, &left, &right, &duration);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        api->SetGamepadVibration(static_cast<int>(gamepadId), static_cast<float>(left),
                                 static_cast<float>(right), static_cast<float>(duration));
    }
    return mrb_nil_value();
}

static mrb_value mrb_script_api_stop_gamepad_vibration(mrb_state* mrb, mrb_value) {
    mrb_int gamepadId;
    mrb_get_args(mrb, "i", &gamepadId);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->StopGamepadVibration(static_cast<int>(gamepadId));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_set_gamepad_deadzone(mrb_state* mrb, mrb_value) {
    mrb_float deadzone;
    mrb_get_args(mrb, "f", &deadzone);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetGamepadDeadzone(static_cast<float>(deadzone));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_gamepad_deadzone(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->GetGamepadDeadzone() : 0.0f);
}

// ============================================================================
// TOUCH INPUT (parity)
// ============================================================================

static mrb_value mrb_script_api_is_touch_available(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->IsTouchAvailable()) ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_is_touching(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->IsTouching()) ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_get_touch_count(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_fixnum_value(api ? api->GetTouchCount() : 0);
}

static mrb_value mrb_script_api_get_touch_position(mrb_state* mrb, mrb_value) {
    mrb_int index = 0;
    mrb_get_args(mrb, "|i", &index);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return Vec2ToMrb(mrb, api ? api->GetTouchPosition(static_cast<int>(index)) : math::Vec2(0.0f, 0.0f));
}

static mrb_value mrb_script_api_is_touch_just_started(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->IsTouchJustStarted()) ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_is_touch_just_ended(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->IsTouchJustEnded()) ? mrb_true_value() : mrb_false_value();
}

// ============================================================================
// CLIPBOARD (parity)
// ============================================================================

static mrb_value mrb_script_api_get_clipboard(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    std::string r = api ? api->GetClipboardText() : "";
    return mrb_str_new(mrb, r.c_str(), r.size());
}

static mrb_value mrb_script_api_set_clipboard(mrb_state* mrb, mrb_value) {
    char* text;
    mrb_get_args(mrb, "z", &text);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetClipboardText(text);
    return mrb_nil_value();
}

// ============================================================================
// ACTIVE DEVICE DETECTION
// ============================================================================

static mrb_value mrb_script_api_get_active_device_type(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_fixnum_value(api ? api->GetActiveDeviceType() : 0);
}

static mrb_value mrb_script_api_get_last_gamepad_id(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_fixnum_value(api ? api->GetLastGamepadId() : 0);
}

static mrb_value mrb_script_api_get_gamepad_type(mrb_state* mrb, mrb_value) {
    mrb_int gamepadId = 0;
    mrb_get_args(mrb, "|i", &gamepadId);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_fixnum_value(api ? api->GetGamepadType(static_cast<int>(gamepadId)) : 0);
}

// ============================================================================
// INPUT CONTEXTS / ACTION SETS
// ============================================================================

static mrb_value mrb_script_api_enable_input_context(mrb_state* mrb, mrb_value) {
    char* ctx;
    mrb_get_args(mrb, "z", &ctx);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->EnableInputContext(ctx);
    return mrb_nil_value();
}

static mrb_value mrb_script_api_disable_input_context(mrb_state* mrb, mrb_value) {
    char* ctx;
    mrb_get_args(mrb, "z", &ctx);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->DisableInputContext(ctx);
    return mrb_nil_value();
}

static mrb_value mrb_script_api_set_input_context_active(mrb_state* mrb, mrb_value) {
    char* ctx; mrb_bool active;
    mrb_get_args(mrb, "zb", &ctx, &active);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetInputContextActive(ctx, active);
    return mrb_nil_value();
}

static mrb_value mrb_script_api_is_input_context_active(mrb_state* mrb, mrb_value) {
    char* ctx;
    mrb_get_args(mrb, "z", &ctx);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->IsInputContextActive(ctx)) ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_set_exclusive_input_context(mrb_state* mrb, mrb_value) {
    char* ctx;
    mrb_get_args(mrb, "z", &ctx);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetExclusiveInputContext(ctx);
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_active_input_contexts(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    mrb_value arr = mrb_ary_new(mrb);
    if (api) {
        for (const std::string& c : api->GetActiveInputContexts()) {
            mrb_ary_push(mrb, arr, mrb_str_new(mrb, c.c_str(), c.size()));
        }
    }
    return arr;
}

static mrb_value mrb_script_api_set_action_enabled(mrb_state* mrb, mrb_value) {
    char* action; mrb_bool enabled;
    mrb_get_args(mrb, "zb", &action, &enabled);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetActionEnabled(action, enabled);
    return mrb_nil_value();
}

static mrb_value mrb_script_api_set_axis_enabled(mrb_state* mrb, mrb_value) {
    char* axis; mrb_bool enabled;
    mrb_get_args(mrb, "zb", &axis, &enabled);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetAxisEnabled(axis, enabled);
    return mrb_nil_value();
}

// ============================================================================
// LOCAL MULTIPLAYER PLAYER SLOTS
// ============================================================================

static mrb_value mrb_script_api_set_player_count(mrb_state* mrb, mrb_value) {
    mrb_int count;
    mrb_get_args(mrb, "i", &count);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetPlayerCount(static_cast<int>(count));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_player_count(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_fixnum_value(api ? api->GetPlayerCount() : 0);
}

static mrb_value mrb_script_api_clear_player_assignments(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->ClearPlayerAssignments();
    return mrb_nil_value();
}

static mrb_value mrb_script_api_assign_keyboard_mouse_to_player(mrb_state* mrb, mrb_value) {
    mrb_int player;
    mrb_get_args(mrb, "i", &player);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->AssignKeyboardMouseToPlayer(static_cast<int>(player));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_assign_gamepad_to_player(mrb_state* mrb, mrb_value) {
    mrb_int player; mrb_int gamepadId;
    mrb_get_args(mrb, "ii", &player, &gamepadId);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->AssignGamepadToPlayer(static_cast<int>(player), static_cast<int>(gamepadId));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_unassign_gamepad(mrb_state* mrb, mrb_value) {
    mrb_int gamepadId;
    mrb_get_args(mrb, "i", &gamepadId);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->UnassignGamepad(static_cast<int>(gamepadId));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_player_for_gamepad(mrb_state* mrb, mrb_value) {
    mrb_int gamepadId;
    mrb_get_args(mrb, "i", &gamepadId);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_fixnum_value(api ? api->GetPlayerForGamepad(static_cast<int>(gamepadId)) : -1);
}

static mrb_value mrb_script_api_get_player_for_keyboard_mouse(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_fixnum_value(api ? api->GetPlayerForKeyboardMouse() : -1);
}

static mrb_value mrb_script_api_player_owns_keyboard_mouse(mrb_state* mrb, mrb_value) {
    mrb_int player;
    mrb_get_args(mrb, "i", &player);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->PlayerOwnsKeyboardMouse(static_cast<int>(player))) ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_get_player_gamepads(mrb_state* mrb, mrb_value) {
    mrb_int player;
    mrb_get_args(mrb, "i", &player);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    mrb_value arr = mrb_ary_new(mrb);
    if (api) {
        for (int id : api->GetPlayerGamepads(static_cast<int>(player))) {
            mrb_ary_push(mrb, arr, mrb_fixnum_value(id));
        }
    }
    return arr;
}

static mrb_value mrb_script_api_set_auto_join_enabled(mrb_state* mrb, mrb_value) {
    mrb_bool enabled;
    mrb_get_args(mrb, "b", &enabled);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetAutoJoinEnabled(enabled);
    return mrb_nil_value();
}

static mrb_value mrb_script_api_is_auto_join_enabled(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->IsAutoJoinEnabled()) ? mrb_true_value() : mrb_false_value();
}

// ============================================================================
// RUNTIME REBINDING
// ============================================================================

static mrb_value mrb_script_api_add_action_key(mrb_state* mrb, mrb_value) {
    char* action; mrb_int keyCode;
    mrb_get_args(mrb, "zi", &action, &keyCode);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->AddActionKey(action, static_cast<int>(keyCode));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_add_action_mouse_button(mrb_state* mrb, mrb_value) {
    char* action; mrb_int button;
    mrb_get_args(mrb, "zi", &action, &button);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->AddActionMouseButton(action, static_cast<int>(button));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_add_action_gamepad_button(mrb_state* mrb, mrb_value) {
    char* action; mrb_int button; mrb_int gamepadId = 0;
    mrb_get_args(mrb, "zi|i", &action, &button, &gamepadId);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->AddActionGamepadButton(action, static_cast<int>(button), static_cast<int>(gamepadId));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_add_action_gamepad_axis(mrb_state* mrb, mrb_value) {
    char* action; mrb_int axis; mrb_float scale = 1.0; mrb_int gamepadId = 0;
    mrb_get_args(mrb, "zi|fi", &action, &axis, &scale, &gamepadId);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->AddActionGamepadAxis(action, static_cast<int>(axis),
                                       static_cast<float>(scale), static_cast<int>(gamepadId));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_remove_action_binding(mrb_state* mrb, mrb_value) {
    char* action; mrb_int index;
    mrb_get_args(mrb, "zi", &action, &index);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->RemoveActionBinding(action, static_cast<int>(index));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_clear_action_bindings(mrb_state* mrb, mrb_value) {
    char* action;
    mrb_get_args(mrb, "z", &action);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->ClearActionBindings(action);
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_action_bindings(mrb_state* mrb, mrb_value) {
    char* action;
    mrb_get_args(mrb, "z", &action);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_ary_new(mrb);
    return JsonToMrb(mrb, api->GetActionBindings(action));
}

static mrb_value mrb_script_api_add_axis_key(mrb_state* mrb, mrb_value) {
    char* axis; mrb_int keyCode; mrb_float scale = 1.0;
    mrb_get_args(mrb, "zi|f", &axis, &keyCode, &scale);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->AddAxisKey(axis, static_cast<int>(keyCode), static_cast<float>(scale));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_add_axis_gamepad_axis(mrb_state* mrb, mrb_value) {
    char* axis; mrb_int gpAxis; mrb_float scale = 1.0; mrb_int gamepadId = 0;
    mrb_get_args(mrb, "zi|fi", &axis, &gpAxis, &scale, &gamepadId);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->AddAxisGamepadAxis(axis, static_cast<int>(gpAxis),
                                     static_cast<float>(scale), static_cast<int>(gamepadId));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_remove_axis_binding(mrb_state* mrb, mrb_value) {
    char* axis; mrb_int index;
    mrb_get_args(mrb, "zi", &axis, &index);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->RemoveAxisBinding(axis, static_cast<int>(index));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_clear_axis_bindings(mrb_state* mrb, mrb_value) {
    char* axis;
    mrb_get_args(mrb, "z", &axis);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->ClearAxisBindings(axis);
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_axis_bindings(mrb_state* mrb, mrb_value) {
    char* axis;
    mrb_get_args(mrb, "z", &axis);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_ary_new(mrb);
    return JsonToMrb(mrb, api->GetAxisBindings(axis));
}

static mrb_value mrb_script_api_save_input_map(mrb_state* mrb, mrb_value) {
    char* path;
    mrb_get_args(mrb, "z", &path);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->SaveInputMap(path)) ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_load_input_map(mrb_state* mrb, mrb_value) {
    char* path;
    mrb_get_args(mrb, "z", &path);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->LoadInputMap(path)) ? mrb_true_value() : mrb_false_value();
}

// ============================================================================
// INPUT CAPTURE (rebind menus)
// ============================================================================

static mrb_value mrb_script_api_start_input_capture(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->StartInputCapture();
    return mrb_nil_value();
}

static mrb_value mrb_script_api_start_input_capture_mask(mrb_state* mrb, mrb_value) {
    mrb_bool keyboard; mrb_bool mouse; mrb_bool gamepad;
    mrb_get_args(mrb, "bbb", &keyboard, &mouse, &gamepad);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->StartInputCaptureMask(keyboard, mouse, gamepad);
    return mrb_nil_value();
}

static mrb_value mrb_script_api_cancel_input_capture(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->CancelInputCapture();
    return mrb_nil_value();
}

static mrb_value mrb_script_api_is_capturing_input(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->IsCapturingInput()) ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_is_input_capture_complete(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->IsInputCaptureComplete()) ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_get_captured_binding(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_nil_value();
    return JsonToMrb(mrb, api->GetCapturedBinding());
}

static mrb_value mrb_script_api_clear_captured_binding(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->ClearCapturedBinding();
    return mrb_nil_value();
}

static mrb_value mrb_script_api_apply_captured_binding_to_action(mrb_state* mrb, mrb_value) {
    char* action;
    mrb_get_args(mrb, "z", &action);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->ApplyCapturedBindingToAction(action);
    return mrb_nil_value();
}

// ============================================================================
// GLYPH / PROMPT RESOLUTION
// ============================================================================

static mrb_value mrb_script_api_get_action_glyph(mrb_state* mrb, mrb_value) {
    char* action; mrb_int player = -1; mrb_int deviceOverride = -1;
    mrb_get_args(mrb, "z|ii", &action, &player, &deviceOverride);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_nil_value();
    return JsonToMrb(mrb, api->GetActionGlyph(action, static_cast<int>(player), static_cast<int>(deviceOverride)));
}

static mrb_value mrb_script_api_get_action_glyphs(mrb_state* mrb, mrb_value) {
    char* action;
    mrb_get_args(mrb, "z", &action);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_ary_new(mrb);
    return JsonToMrb(mrb, api->GetActionGlyphs(action));
}

static mrb_value mrb_script_api_set_glyph_label(mrb_state* mrb, mrb_value) {
    char* glyphId; char* label;
    mrb_get_args(mrb, "zz", &glyphId, &label);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetGlyphLabel(glyphId, label);
    return mrb_nil_value();
}

static mrb_value mrb_script_api_set_glyph_art(mrb_state* mrb, mrb_value) {
    char* glyphId; char* artPath;
    mrb_get_args(mrb, "zz", &glyphId, &artPath);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetGlyphArt(glyphId, artPath);
    return mrb_nil_value();
}

static mrb_value mrb_script_api_clear_glyph_override(mrb_state* mrb, mrb_value) {
    char* glyphId;
    mrb_get_args(mrb, "z", &glyphId);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->ClearGlyphOverride(glyphId);
    return mrb_nil_value();
}

static mrb_value mrb_script_api_clear_glyph_overrides(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->ClearGlyphOverrides();
    return mrb_nil_value();
}

static mrb_value mrb_script_api_load_glyph_map(mrb_state* mrb, mrb_value) {
    char* path;
    mrb_get_args(mrb, "z", &path);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->LoadGlyphMap(path)) ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_save_glyph_map(mrb_state* mrb, mrb_value) {
    char* path;
    mrb_get_args(mrb, "z", &path);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->SaveGlyphMap(path)) ? mrb_true_value() : mrb_false_value();
}

// ============================================================================
// ACTION DELEGATION
// ============================================================================

static mrb_value mrb_script_api_connect_action(mrb_state* mrb, mrb_value) {
    char* action; char* method; mrb_int flags = 0;
    mrb_get_args(mrb, "zz|i", &action, &method, &flags);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return mrb_fixnum_value(static_cast<mrb_int>(
            api->ConnectInputAction(action, method, static_cast<uint32_t>(flags))));
    }
    return mrb_fixnum_value(0);
}

static mrb_value mrb_script_api_disconnect_action(mrb_state* mrb, mrb_value) {
    char* action; mrb_int id;
    mrb_get_args(mrb, "zi", &action, &id);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->DisconnectInputAction(action, static_cast<uint64_t>(id));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_connect_device_changed(mrb_state* mrb, mrb_value) {
    char* method; mrb_int flags = 0;
    mrb_get_args(mrb, "z|i", &method, &flags);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return mrb_fixnum_value(static_cast<mrb_int>(
            api->ConnectDeviceChanged(method, static_cast<uint32_t>(flags))));
    }
    return mrb_fixnum_value(0);
}

static mrb_value mrb_script_api_connect_input_captured(mrb_state* mrb, mrb_value) {
    char* method; mrb_int flags = 0;
    mrb_get_args(mrb, "z|i", &method, &flags);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        return mrb_fixnum_value(static_cast<mrb_int>(
            api->ConnectInputCaptured(method, static_cast<uint32_t>(flags))));
    }
    return mrb_fixnum_value(0);
}

// ============================================================================
// EVENT-DRIVEN ACTION MATCHING (inside on_input_event)
// ============================================================================

static mrb_value mrb_script_api_event_is_action(mrb_state* mrb, mrb_value) {
    mrb_value event; char* action;
    mrb_get_args(mrb, "oz", &event, &action);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->EventIsAction(MrbToJson(mrb, event), action)) ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_event_is_action_pressed(mrb_state* mrb, mrb_value) {
    mrb_value event; char* action;
    mrb_get_args(mrb, "oz", &event, &action);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->EventIsActionPressed(MrbToJson(mrb, event), action)) ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_event_is_action_released(mrb_state* mrb, mrb_value) {
    mrb_value event; char* action;
    mrb_get_args(mrb, "oz", &event, &action);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->EventIsActionReleased(MrbToJson(mrb, event), action)) ? mrb_true_value() : mrb_false_value();
}

// ============================================================================
// GAME STATE (parity)
// ============================================================================

static mrb_value mrb_script_api_set_game_paused(mrb_state* mrb, mrb_value) {
    mrb_bool paused;
    mrb_get_args(mrb, "b", &paused);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetGamePaused(paused);
    return mrb_nil_value();
}

static mrb_value mrb_script_api_is_game_paused(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->IsGamePaused()) ? mrb_true_value() : mrb_false_value();
}

// ============================================================================
// TRANSFORM - GLOBAL & 3D ROTATION/SCALE (parity)
// ============================================================================

static mrb_value mrb_script_api_get_global_position_2d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return Vec2ToMrb(mrb, api ? api->GetGlobalPosition2D() : math::Vec2(0.0f, 0.0f));
}

static mrb_value mrb_script_api_set_global_position_2d(mrb_state* mrb, mrb_value) {
    mrb_float x, y;
    mrb_get_args(mrb, "ff", &x, &y);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetGlobalPosition2D(static_cast<float>(x), static_cast<float>(y));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_global_position_3d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return Vec3ToMrb(mrb, api ? api->GetGlobalPosition3D() : math::Vec3(0.0f, 0.0f, 0.0f));
}

static mrb_value mrb_script_api_set_global_position_3d(mrb_state* mrb, mrb_value) {
    mrb_float x, y, z;
    mrb_get_args(mrb, "fff", &x, &y, &z);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetGlobalPosition3D(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_global_rotation_2d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->GetGlobalRotation2D() : 0.0f);
}

static mrb_value mrb_script_api_set_global_rotation_2d(mrb_state* mrb, mrb_value) {
    mrb_float degrees;
    mrb_get_args(mrb, "f", &degrees);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetGlobalRotation2D(static_cast<float>(degrees));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_global_rotation_3d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return Vec3ToMrb(mrb, api ? api->GetGlobalRotation3D() : math::Vec3(0.0f, 0.0f, 0.0f));
}

static mrb_value mrb_script_api_set_global_rotation_3d(mrb_state* mrb, mrb_value) {
    mrb_float pitch, yaw, roll;
    mrb_get_args(mrb, "fff", &pitch, &yaw, &roll);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetGlobalRotation3D(static_cast<float>(pitch), static_cast<float>(yaw),
                                      static_cast<float>(roll));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_global_scale_2d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return Vec2ToMrb(mrb, api ? api->GetGlobalScale2D() : math::Vec2(1.0f, 1.0f));
}

static mrb_value mrb_script_api_get_global_scale_3d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return Vec3ToMrb(mrb, api ? api->GetGlobalScale3D() : math::Vec3(1.0f, 1.0f, 1.0f));
}

static mrb_value mrb_script_api_get_rotation_3d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return Vec3ToMrb(mrb, api ? api->GetRotation3D() : math::Vec3(0.0f, 0.0f, 0.0f));
}

static mrb_value mrb_script_api_set_rotation_3d(mrb_state* mrb, mrb_value) {
    mrb_float pitch, yaw, roll;
    mrb_get_args(mrb, "fff", &pitch, &yaw, &roll);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetRotation3D(static_cast<float>(pitch), static_cast<float>(yaw), static_cast<float>(roll));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_scale_3d(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return Vec3ToMrb(mrb, api ? api->GetScale3D() : math::Vec3(1.0f, 1.0f, 1.0f));
}

static mrb_value mrb_script_api_set_scale_3d(mrb_state* mrb, mrb_value) {
    mrb_float x, y, z;
    mrb_get_args(mrb, "fff", &x, &y, &z);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetScale3D(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_forward(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return Vec3ToMrb(mrb, api ? api->GetForward() : math::Vec3(0.0f, 0.0f, -1.0f));
}

static mrb_value mrb_script_api_get_right(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return Vec3ToMrb(mrb, api ? api->GetRight() : math::Vec3(1.0f, 0.0f, 0.0f));
}

static mrb_value mrb_script_api_get_up(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return Vec3ToMrb(mrb, api ? api->GetUp() : math::Vec3(0.0f, 1.0f, 0.0f));
}

static mrb_value mrb_script_api_look_at_2d(mrb_state* mrb, mrb_value) {
    mrb_float x, y;
    mrb_get_args(mrb, "ff", &x, &y);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->LookAt2D(static_cast<float>(x), static_cast<float>(y));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_look_at_3d(mrb_state* mrb, mrb_value) {
    mrb_float x, y, z;
    mrb_get_args(mrb, "fff", &x, &y, &z);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->LookAt3D(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_distance_to_2d(mrb_state* mrb, mrb_value) {
    mrb_float x, y;
    mrb_get_args(mrb, "ff", &x, &y);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->DistanceTo2D(static_cast<float>(x), static_cast<float>(y)) : 0.0f);
}

static mrb_value mrb_script_api_distance_to_3d(mrb_state* mrb, mrb_value) {
    mrb_float x, y, z;
    mrb_get_args(mrb, "fff", &x, &y, &z);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->DistanceTo3D(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)) : 0.0f);
}

static mrb_value mrb_script_api_move_toward_2d(mrb_state* mrb, mrb_value) {
    mrb_float tx, ty, maxDelta;
    mrb_get_args(mrb, "fff", &tx, &ty, &maxDelta);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->MoveToward2D(static_cast<float>(tx), static_cast<float>(ty), static_cast<float>(maxDelta));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_move_toward_3d(mrb_state* mrb, mrb_value) {
    mrb_float tx, ty, tz, maxDelta;
    mrb_get_args(mrb, "ffff", &tx, &ty, &tz, &maxDelta);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->MoveToward3D(static_cast<float>(tx), static_cast<float>(ty), static_cast<float>(tz), static_cast<float>(maxDelta));
    return mrb_nil_value();
}

// ============================================================================
// MATH HELPERS (parity)
// ============================================================================

static mrb_value mrb_script_api_abs(mrb_state* mrb, mrb_value) {
    mrb_float value;
    mrb_get_args(mrb, "f", &value);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->Abs(static_cast<float>(value)) : 0.0f);
}

static mrb_value mrb_script_api_sign(mrb_state* mrb, mrb_value) {
    mrb_float value;
    mrb_get_args(mrb, "f", &value);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->Sign(static_cast<float>(value)) : 0.0f);
}

static mrb_value mrb_script_api_lerp(mrb_state* mrb, mrb_value) {
    mrb_float a, b, t;
    mrb_get_args(mrb, "fff", &a, &b, &t);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->Lerp(static_cast<float>(a), static_cast<float>(b), static_cast<float>(t)) : 0.0f);
}

static mrb_value mrb_script_api_clamp(mrb_state* mrb, mrb_value) {
    mrb_float value, minVal, maxVal;
    mrb_get_args(mrb, "fff", &value, &minVal, &maxVal);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->Clamp(static_cast<float>(value), static_cast<float>(minVal), static_cast<float>(maxVal)) : 0.0f);
}

static mrb_value mrb_script_api_move_toward(mrb_state* mrb, mrb_value) {
    mrb_float from, to, delta;
    mrb_get_args(mrb, "fff", &from, &to, &delta);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->MoveToward(static_cast<float>(from), static_cast<float>(to), static_cast<float>(delta)) : 0.0f);
}

static mrb_value mrb_script_api_lerp_angle(mrb_state* mrb, mrb_value) {
    mrb_float from, to, weight;
    mrb_get_args(mrb, "fff", &from, &to, &weight);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->LerpAngle(static_cast<float>(from), static_cast<float>(to), static_cast<float>(weight)) : 0.0f);
}

static mrb_value mrb_script_api_angle_difference(mrb_state* mrb, mrb_value) {
    mrb_float from, to;
    mrb_get_args(mrb, "ff", &from, &to);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->AngleDifference(static_cast<float>(from), static_cast<float>(to)) : 0.0f);
}

static mrb_value mrb_script_api_smoothstep(mrb_state* mrb, mrb_value) {
    mrb_float from, to, t;
    mrb_get_args(mrb, "fff", &from, &to, &t);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->Smoothstep(static_cast<float>(from), static_cast<float>(to), static_cast<float>(t)) : 0.0f);
}

static mrb_value mrb_script_api_inverse_lerp(mrb_state* mrb, mrb_value) {
    mrb_float from, to, value;
    mrb_get_args(mrb, "fff", &from, &to, &value);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->InverseLerp(static_cast<float>(from), static_cast<float>(to), static_cast<float>(value)) : 0.0f);
}

static mrb_value mrb_script_api_remap(mrb_state* mrb, mrb_value) {
    mrb_float value, fromMin, fromMax, toMin, toMax;
    mrb_get_args(mrb, "fffff", &value, &fromMin, &fromMax, &toMin, &toMax);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->Remap(static_cast<float>(value), static_cast<float>(fromMin),
        static_cast<float>(fromMax), static_cast<float>(toMin), static_cast<float>(toMax)) : 0.0f);
}

static mrb_value mrb_script_api_normalize_2d(mrb_state* mrb, mrb_value) {
    mrb_float x, y;
    mrb_get_args(mrb, "ff", &x, &y);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) return Vec2ToMrb(mrb, api->Normalize2D(mrbVec2(x, y)));
    return Vec2ToMrb(mrb, math::Vec2(0.0f, 0.0f));
}

static mrb_value mrb_script_api_normalize_3d(mrb_state* mrb, mrb_value) {
    mrb_float x, y, z;
    mrb_get_args(mrb, "fff", &x, &y, &z);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) return Vec3ToMrb(mrb, api->Normalize3D(mrbVec3(x, y, z)));
    return Vec3ToMrb(mrb, math::Vec3(0.0f, 0.0f, 0.0f));
}

static mrb_value mrb_script_api_length_2d(mrb_state* mrb, mrb_value) {
    mrb_float x, y;
    mrb_get_args(mrb, "ff", &x, &y);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->Length2D(mrbVec2(x, y)) : 0.0f);
}

static mrb_value mrb_script_api_length_3d(mrb_state* mrb, mrb_value) {
    mrb_float x, y, z;
    mrb_get_args(mrb, "fff", &x, &y, &z);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->Length3D(mrbVec3(x, y, z)) : 0.0f);
}

static mrb_value mrb_script_api_dot_2d(mrb_state* mrb, mrb_value) {
    mrb_float ax, ay, bx, by;
    mrb_get_args(mrb, "ffff", &ax, &ay, &bx, &by);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->Dot2D(mrbVec2(ax, ay),
        mrbVec2(bx, by)) : 0.0f);
}

static mrb_value mrb_script_api_dot_3d(mrb_state* mrb, mrb_value) {
    mrb_float ax, ay, az, bx, by, bz;
    mrb_get_args(mrb, "ffffff", &ax, &ay, &az, &bx, &by, &bz);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->Dot3D(mrbVec3(ax, ay, az),
        mrbVec3(bx, by, bz)) : 0.0f);
}

static mrb_value mrb_script_api_cross(mrb_state* mrb, mrb_value) {
    mrb_float ax, ay, az, bx, by, bz;
    mrb_get_args(mrb, "ffffff", &ax, &ay, &az, &bx, &by, &bz);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) return Vec3ToMrb(mrb, api->Cross(mrbVec3(ax, ay, az),
        mrbVec3(bx, by, bz)));
    return Vec3ToMrb(mrb, math::Vec3(0.0f, 0.0f, 0.0f));
}

// ============================================================================
// RANDOM & MATH (parity)
// ============================================================================

static mrb_value mrb_script_api_random_float(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->RandomFloat() : 0.0f);
}

static mrb_value mrb_script_api_random_bool(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->RandomBool()) ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_random_sign(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_fixnum_value(api ? api->RandomSign() : 1);
}

static mrb_value mrb_script_api_random_seed(mrb_state* mrb, mrb_value) {
    mrb_int seed;
    mrb_get_args(mrb, "i", &seed);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->RandomSeed(static_cast<int>(seed));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_deg_to_rad(mrb_state* mrb, mrb_value) {
    mrb_float degrees;
    mrb_get_args(mrb, "f", &degrees);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->DegToRad(static_cast<float>(degrees)) : 0.0f);
}

static mrb_value mrb_script_api_rad_to_deg(mrb_state* mrb, mrb_value) {
    mrb_float radians;
    mrb_get_args(mrb, "f", &radians);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->RadToDeg(static_cast<float>(radians)) : 0.0f);
}

static mrb_value mrb_script_api_wrap(mrb_state* mrb, mrb_value) {
    mrb_float value, minVal, maxVal;
    mrb_get_args(mrb, "fff", &value, &minVal, &maxVal);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->Wrap(static_cast<float>(value), static_cast<float>(minVal), static_cast<float>(maxVal)) : 0.0f);
}

static mrb_value mrb_script_api_wrap_int(mrb_state* mrb, mrb_value) {
    mrb_int value, minVal, maxVal;
    mrb_get_args(mrb, "iii", &value, &minVal, &maxVal);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_fixnum_value(api ? api->WrapInt(static_cast<int>(value), static_cast<int>(minVal), static_cast<int>(maxVal)) : 0);
}

static mrb_value mrb_script_api_ping_pong(mrb_state* mrb, mrb_value) {
    mrb_float value, length;
    mrb_get_args(mrb, "ff", &value, &length);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->PingPong(static_cast<float>(value), static_cast<float>(length)) : 0.0f);
}

static mrb_value mrb_script_api_snapped(mrb_state* mrb, mrb_value) {
    mrb_float value, step;
    mrb_get_args(mrb, "ff", &value, &step);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->Snapped(static_cast<float>(value), static_cast<float>(step)) : 0.0f);
}

static mrb_value mrb_script_api_is_equal_approx(mrb_state* mrb, mrb_value) {
    mrb_float a, b;
    mrb_get_args(mrb, "ff", &a, &b);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->IsEqualApprox(static_cast<float>(a), static_cast<float>(b))) ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_ease(mrb_state* mrb, mrb_value) {
    mrb_float t, curve;
    mrb_get_args(mrb, "ff", &t, &curve);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->Ease(static_cast<float>(t), static_cast<float>(curve)) : 0.0f);
}

static mrb_value mrb_script_api_pos_mod(mrb_state* mrb, mrb_value) {
    mrb_float a, b;
    mrb_get_args(mrb, "ff", &a, &b);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->PosMod(static_cast<float>(a), static_cast<float>(b)) : 0.0f);
}

static mrb_value mrb_script_api_pos_mod_int(mrb_state* mrb, mrb_value) {
    mrb_int a, b;
    mrb_get_args(mrb, "ii", &a, &b);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_fixnum_value(api ? api->PosModInt(static_cast<int>(a), static_cast<int>(b)) : 0);
}

// ============================================================================
// PHYSICS QUERIES (parity) - multi-hit raycasts, shape casts, overlaps
// ============================================================================

static mrb_value mrb_script_api_raycast_all_2d(mrb_state* mrb, mrb_value) {
    mrb_float fromX, fromY, dirX, dirY, maxDist;
    mrb_get_args(mrb, "fffff", &fromX, &fromY, &dirX, &dirY, &maxDist);
    mrb_value result = mrb_ary_new(mrb);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        std::vector<ScriptAPI::RaycastHit2D> hits =
            api->RaycastAll2D(mrbVec2(fromX, fromY), mrbVec2(dirX, dirY), static_cast<float>(maxDist));
        for (const ScriptAPI::RaycastHit2D& hit : hits) {
            mrb_value h = mrb_hash_new(mrb);
            mrb_hash_set(mrb, h, mrb_symbol_value(mrb_intern_lit(mrb, "hit")), hit.hit ? mrb_true_value() : mrb_false_value());
            mrb_hash_set(mrb, h, mrb_symbol_value(mrb_intern_lit(mrb, "point_x")), mrb_float_value(mrb, hit.point.x));
            mrb_hash_set(mrb, h, mrb_symbol_value(mrb_intern_lit(mrb, "point_y")), mrb_float_value(mrb, hit.point.y));
            mrb_hash_set(mrb, h, mrb_symbol_value(mrb_intern_lit(mrb, "normal_x")), mrb_float_value(mrb, hit.normal.x));
            mrb_hash_set(mrb, h, mrb_symbol_value(mrb_intern_lit(mrb, "normal_y")), mrb_float_value(mrb, hit.normal.y));
            mrb_hash_set(mrb, h, mrb_symbol_value(mrb_intern_lit(mrb, "distance")), mrb_float_value(mrb, hit.distance));
            mrb_ary_push(mrb, result, h);
        }
    }
    return result;
}

static mrb_value mrb_script_api_raycast_all_3d(mrb_state* mrb, mrb_value) {
    mrb_float fromX, fromY, fromZ, dirX, dirY, dirZ, maxDist;
    mrb_get_args(mrb, "fffffff", &fromX, &fromY, &fromZ, &dirX, &dirY, &dirZ, &maxDist);
    mrb_value result = mrb_ary_new(mrb);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        std::vector<ScriptAPI::RaycastHit3D> hits =
            api->RaycastAll3D(mrbVec3(fromX, fromY, fromZ), mrbVec3(dirX, dirY, dirZ), static_cast<float>(maxDist));
        for (const ScriptAPI::RaycastHit3D& hit : hits) {
            mrb_value h = mrb_hash_new(mrb);
            mrb_hash_set(mrb, h, mrb_symbol_value(mrb_intern_lit(mrb, "hit")), hit.hit ? mrb_true_value() : mrb_false_value());
            mrb_hash_set(mrb, h, mrb_symbol_value(mrb_intern_lit(mrb, "point_x")), mrb_float_value(mrb, hit.point.x));
            mrb_hash_set(mrb, h, mrb_symbol_value(mrb_intern_lit(mrb, "point_y")), mrb_float_value(mrb, hit.point.y));
            mrb_hash_set(mrb, h, mrb_symbol_value(mrb_intern_lit(mrb, "point_z")), mrb_float_value(mrb, hit.point.z));
            mrb_hash_set(mrb, h, mrb_symbol_value(mrb_intern_lit(mrb, "normal_x")), mrb_float_value(mrb, hit.normal.x));
            mrb_hash_set(mrb, h, mrb_symbol_value(mrb_intern_lit(mrb, "normal_y")), mrb_float_value(mrb, hit.normal.y));
            mrb_hash_set(mrb, h, mrb_symbol_value(mrb_intern_lit(mrb, "normal_z")), mrb_float_value(mrb, hit.normal.z));
            mrb_hash_set(mrb, h, mrb_symbol_value(mrb_intern_lit(mrb, "distance")), mrb_float_value(mrb, hit.distance));
            mrb_ary_push(mrb, result, h);
        }
    }
    return result;
}

static mrb_value mrb_script_api_circle_cast_2d(mrb_state* mrb, mrb_value) {
    mrb_float fromX, fromY, toX, toY, radius;
    mrb_get_args(mrb, "fffff", &fromX, &fromY, &toX, &toY, &radius);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    mrb_value result = mrb_hash_new(mrb);
    if (api) {
        ScriptAPI::ShapeCastHit2D hit = api->CircleCast2D(mrbVec2(fromX, fromY), mrbVec2(toX, toY), static_cast<float>(radius));
        mrb_hash_set(mrb, result, mrb_symbol_value(mrb_intern_lit(mrb, "hit")), hit.hit ? mrb_true_value() : mrb_false_value());
        mrb_hash_set(mrb, result, mrb_symbol_value(mrb_intern_lit(mrb, "point_x")), mrb_float_value(mrb, hit.point.x));
        mrb_hash_set(mrb, result, mrb_symbol_value(mrb_intern_lit(mrb, "point_y")), mrb_float_value(mrb, hit.point.y));
        mrb_hash_set(mrb, result, mrb_symbol_value(mrb_intern_lit(mrb, "normal_x")), mrb_float_value(mrb, hit.normal.x));
        mrb_hash_set(mrb, result, mrb_symbol_value(mrb_intern_lit(mrb, "normal_y")), mrb_float_value(mrb, hit.normal.y));
        mrb_hash_set(mrb, result, mrb_symbol_value(mrb_intern_lit(mrb, "fraction")), mrb_float_value(mrb, hit.fraction));
        return result;
    }
    mrb_hash_set(mrb, result, mrb_symbol_value(mrb_intern_lit(mrb, "hit")), mrb_false_value());
    return result;
}

static mrb_value mrb_script_api_sphere_cast_3d(mrb_state* mrb, mrb_value) {
    mrb_float fromX, fromY, fromZ, toX, toY, toZ, radius;
    mrb_get_args(mrb, "fffffff", &fromX, &fromY, &fromZ, &toX, &toY, &toZ, &radius);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    mrb_value result = mrb_hash_new(mrb);
    if (api) {
        ScriptAPI::ShapeCastHit3D hit = api->SphereCast3D(mrbVec3(fromX, fromY, fromZ), mrbVec3(toX, toY, toZ), static_cast<float>(radius));
        mrb_hash_set(mrb, result, mrb_symbol_value(mrb_intern_lit(mrb, "hit")), hit.hit ? mrb_true_value() : mrb_false_value());
        mrb_hash_set(mrb, result, mrb_symbol_value(mrb_intern_lit(mrb, "point_x")), mrb_float_value(mrb, hit.point.x));
        mrb_hash_set(mrb, result, mrb_symbol_value(mrb_intern_lit(mrb, "point_y")), mrb_float_value(mrb, hit.point.y));
        mrb_hash_set(mrb, result, mrb_symbol_value(mrb_intern_lit(mrb, "point_z")), mrb_float_value(mrb, hit.point.z));
        mrb_hash_set(mrb, result, mrb_symbol_value(mrb_intern_lit(mrb, "normal_x")), mrb_float_value(mrb, hit.normal.x));
        mrb_hash_set(mrb, result, mrb_symbol_value(mrb_intern_lit(mrb, "normal_y")), mrb_float_value(mrb, hit.normal.y));
        mrb_hash_set(mrb, result, mrb_symbol_value(mrb_intern_lit(mrb, "normal_z")), mrb_float_value(mrb, hit.normal.z));
        mrb_hash_set(mrb, result, mrb_symbol_value(mrb_intern_lit(mrb, "fraction")), mrb_float_value(mrb, hit.fraction));
        return result;
    }
    mrb_hash_set(mrb, result, mrb_symbol_value(mrb_intern_lit(mrb, "hit")), mrb_false_value());
    return result;
}

static mrb_value mrb_script_api_overlap_circle(mrb_state* mrb, mrb_value) {
    mrb_float cx, cy, radius;
    mrb_get_args(mrb, "fff", &cx, &cy, &radius);
    mrb_value result = mrb_ary_new(mrb);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        std::vector<core::Node*> nodes = api->OverlapCircle(mrbVec2(cx, cy), static_cast<float>(radius));
        for (core::Node* node : nodes) {
            mrb_ary_push(mrb, result, WrapNodeRef(mrb, NodeRef::FromRaw(node, api)));
        }
    }
    return result;
}

static mrb_value mrb_script_api_overlap_rect(mrb_state* mrb, mrb_value) {
    mrb_float cx, cy, hx, hy;
    mrb_get_args(mrb, "ffff", &cx, &cy, &hx, &hy);
    mrb_value result = mrb_ary_new(mrb);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        std::vector<core::Node*> nodes = api->OverlapRect(mrbVec2(cx, cy), mrbVec2(hx, hy));
        for (core::Node* node : nodes) {
            mrb_ary_push(mrb, result, WrapNodeRef(mrb, NodeRef::FromRaw(node, api)));
        }
    }
    return result;
}

static mrb_value mrb_script_api_overlap_sphere(mrb_state* mrb, mrb_value) {
    mrb_float cx, cy, cz, radius;
    mrb_get_args(mrb, "ffff", &cx, &cy, &cz, &radius);
    mrb_value result = mrb_ary_new(mrb);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        std::vector<core::Node*> nodes = api->OverlapSphere(mrbVec3(cx, cy, cz), static_cast<float>(radius));
        for (core::Node* node : nodes) {
            mrb_ary_push(mrb, result, WrapNodeRef(mrb, NodeRef::FromRaw(node, api)));
        }
    }
    return result;
}

static mrb_value mrb_script_api_overlap_box(mrb_state* mrb, mrb_value) {
    mrb_float cx, cy, cz, hx, hy, hz;
    mrb_get_args(mrb, "ffffff", &cx, &cy, &cz, &hx, &hy, &hz);
    mrb_value result = mrb_ary_new(mrb);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        std::vector<core::Node*> nodes = api->OverlapBox(mrbVec3(cx, cy, cz), mrbVec3(hx, hy, hz));
        for (core::Node* node : nodes) {
            mrb_ary_push(mrb, result, WrapNodeRef(mrb, NodeRef::FromRaw(node, api)));
        }
    }
    return result;
}

// ============================================================================
// AUDIO (parity)
// ============================================================================

static mrb_value mrb_script_api_play_audio(mrb_state* mrb, mrb_value) {
    char* path; char* bus = const_cast<char*>("Master");
    mrb_bool loop = false; mrb_float volume = 1.0;
    mrb_get_args(mrb, "z|zbf", &path, &bus, &loop, &volume);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        std::string id = api->PlayAudio(path, bus, loop, static_cast<float>(volume));
        return mrb_str_new_cstr(mrb, id.c_str());
    }
    return mrb_str_new_cstr(mrb, "");
}

static mrb_value mrb_script_api_play_audio_3d(mrb_state* mrb, mrb_value) {
    char* path; mrb_float x, y, z;
    char* bus = const_cast<char*>("Master");
    mrb_bool loop = false; mrb_float volume = 1.0;
    mrb_get_args(mrb, "zfff|zbf", &path, &x, &y, &z, &bus, &loop, &volume);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        std::string id = api->PlayAudio3D(path, mrbVec3(x, y, z), bus, loop, static_cast<float>(volume));
        return mrb_str_new_cstr(mrb, id.c_str());
    }
    return mrb_str_new_cstr(mrb, "");
}

static mrb_value mrb_script_api_stop_audio(mrb_state* mrb, mrb_value) {
    char* uuid;
    mrb_get_args(mrb, "z", &uuid);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->StopAudio(uuid);
    return mrb_nil_value();
}

static mrb_value mrb_script_api_pause_audio(mrb_state* mrb, mrb_value) {
    char* uuid;
    mrb_get_args(mrb, "z", &uuid);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->PauseAudio(uuid);
    return mrb_nil_value();
}

static mrb_value mrb_script_api_resume_audio(mrb_state* mrb, mrb_value) {
    char* uuid;
    mrb_get_args(mrb, "z", &uuid);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->ResumeAudio(uuid);
    return mrb_nil_value();
}

static mrb_value mrb_script_api_set_bus_muted(mrb_state* mrb, mrb_value) {
    char* bus; mrb_bool muted;
    mrb_get_args(mrb, "zb", &bus, &muted);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetBusMuted(bus, muted);
    return mrb_nil_value();
}

static mrb_value mrb_script_api_is_bus_muted(mrb_state* mrb, mrb_value) {
    char* bus;
    mrb_get_args(mrb, "z", &bus);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->IsBusMuted(bus)) ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_set_audio_source_volume(mrb_state* mrb, mrb_value) {
    char* uuid; mrb_float volume;
    mrb_get_args(mrb, "zf", &uuid, &volume);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetAudioSourceVolume(uuid, static_cast<float>(volume));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_set_audio_source_pitch(mrb_state* mrb, mrb_value) {
    char* uuid; mrb_float pitch;
    mrb_get_args(mrb, "zf", &uuid, &pitch);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetAudioSourcePitch(uuid, static_cast<float>(pitch));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_set_audio_source_pan(mrb_state* mrb, mrb_value) {
    char* uuid; mrb_float pan;
    mrb_get_args(mrb, "zf", &uuid, &pan);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetAudioSourcePan(uuid, static_cast<float>(pan));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_set_master_volume(mrb_state* mrb, mrb_value) {
    mrb_float volume;
    mrb_get_args(mrb, "f", &volume);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetMasterVolume(static_cast<float>(volume));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_master_volume(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->GetMasterVolume() : 1.0f);
}

static mrb_value mrb_script_api_set_master_muted(mrb_state* mrb, mrb_value) {
    mrb_bool muted;
    mrb_get_args(mrb, "b", &muted);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetMasterMuted(muted);
    return mrb_nil_value();
}

static mrb_value mrb_script_api_is_master_muted(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->IsMasterMuted()) ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_set_listener_position(mrb_state* mrb, mrb_value) {
    mrb_float x, y, z;
    mrb_get_args(mrb, "fff", &x, &y, &z);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetListenerPosition(mrbVec3(x, y, z));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_set_listener_orientation(mrb_state* mrb, mrb_value) {
    mrb_float fx, fy, fz, ux, uy, uz;
    mrb_get_args(mrb, "ffffff", &fx, &fy, &fz, &ux, &uy, &uz);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetListenerOrientation(mrbVec3(fx, fy, fz), mrbVec3(ux, uy, uz));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_set_listener_velocity(mrb_state* mrb, mrb_value) {
    mrb_float x, y, z;
    mrb_get_args(mrb, "fff", &x, &y, &z);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetListenerVelocity(mrbVec3(x, y, z));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_create_audio_bus(mrb_state* mrb, mrb_value) {
    char* name; char* parent = const_cast<char*>("");
    mrb_get_args(mrb, "z|z", &name, &parent);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->CreateAudioBus(name, parent);
    return mrb_nil_value();
}

static mrb_value mrb_script_api_destroy_audio_bus(mrb_state* mrb, mrb_value) {
    char* name;
    mrb_get_args(mrb, "z", &name);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->DestroyAudioBus(name);
    return mrb_nil_value();
}

static mrb_value mrb_script_api_has_audio_bus(mrb_state* mrb, mrb_value) {
    char* name;
    mrb_get_args(mrb, "z", &name);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->HasAudioBus(name)) ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_set_bus_solo(mrb_state* mrb, mrb_value) {
    char* name; mrb_bool solo;
    mrb_get_args(mrb, "zb", &name, &solo);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetBusSolo(name, solo);
    return mrb_nil_value();
}

static mrb_value mrb_script_api_is_bus_solo(mrb_state* mrb, mrb_value) {
    char* name;
    mrb_get_args(mrb, "z", &name);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->IsBusSolo(name)) ? mrb_true_value() : mrb_false_value();
}

// ---- Localization ----

static std::unordered_map<std::string, std::string> MrbHashToArgs(mrb_state* mrb, mrb_value hash) {
    std::unordered_map<std::string, std::string> args;
    if (!mrb_hash_p(hash)) {
        return args;
    }
    mrb_value keys = mrb_hash_keys(mrb, hash);
    mrb_int len = RARRAY_LEN(keys);
    for (mrb_int i = 0; i < len; ++i) {
        mrb_value key = mrb_ary_ref(mrb, keys, i);
        mrb_value val = mrb_hash_get(mrb, hash, key);
        mrb_value keyStr = mrb_obj_as_string(mrb, key);
        mrb_value valStr = mrb_obj_as_string(mrb, val);
        args[std::string(RSTRING_PTR(keyStr), RSTRING_LEN(keyStr))] =
            std::string(RSTRING_PTR(valStr), RSTRING_LEN(valStr));
    }
    return args;
}

static mrb_value mrb_script_api_tr(mrb_state* mrb, mrb_value) {
    char* key; char* table = nullptr;
    mrb_get_args(mrb, "z|z", &key, &table);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_str_new_cstr(mrb, key);
    std::string r = (table && table[0]) ? api->TrInTable(key, table) : api->Tr(key);
    return mrb_str_new(mrb, r.c_str(), r.size());
}

static mrb_value mrb_script_api_tr_fmt(mrb_state* mrb, mrb_value) {
    char* key; mrb_value hash = mrb_nil_value(); char* table = nullptr;
    mrb_get_args(mrb, "z|Hz", &key, &hash, &table);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_str_new_cstr(mrb, key);
    std::string r = api->TrFormat(key, MrbHashToArgs(mrb, hash), table ? table : "");
    return mrb_str_new(mrb, r.c_str(), r.size());
}

static mrb_value mrb_script_api_tr_plural(mrb_state* mrb, mrb_value) {
    char* key; mrb_int count; mrb_value hash = mrb_nil_value(); char* table = nullptr;
    mrb_get_args(mrb, "zi|Hz", &key, &count, &hash, &table);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_str_new_cstr(mrb, key);
    std::string r = api->TrPlural(key, static_cast<long>(count),
                                  MrbHashToArgs(mrb, hash), table ? table : "");
    return mrb_str_new(mrb, r.c_str(), r.size());
}

static mrb_value mrb_script_api_set_locale(mrb_state* mrb, mrb_value) {
    char* locale;
    mrb_get_args(mrb, "z", &locale);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetLocale(locale);
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_locale(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    std::string r = api ? api->GetLocale() : "";
    return mrb_str_new(mrb, r.c_str(), r.size());
}

static mrb_value mrb_script_api_get_fallback_locale(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    std::string r = api ? api->GetFallbackLocale() : "";
    return mrb_str_new(mrb, r.c_str(), r.size());
}

static mrb_value mrb_script_api_get_locales(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    mrb_value arr = mrb_ary_new(mrb);
    if (api) {
        for (const std::string& locale : api->GetAvailableLocales()) {
            mrb_ary_push(mrb, arr, mrb_str_new(mrb, locale.c_str(), locale.size()));
        }
    }
    return arr;
}

static mrb_value mrb_script_api_has_loc_key(mrb_state* mrb, mrb_value) {
    char* key;
    mrb_get_args(mrb, "z", &key);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->HasLocaleKey(key)) ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_reload_localization(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->ReloadLocalization();
    return mrb_nil_value();
}

static mrb_value mrb_script_api_set_pseudolocalization(mrb_state* mrb, mrb_value) {
    mrb_bool enabled;
    mrb_get_args(mrb, "b", &enabled);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetPseudolocalization(enabled);
    return mrb_nil_value();
}

static mrb_value mrb_script_api_is_pseudolocalization(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->IsPseudolocalization()) ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_set_theme(mrb_state* mrb, mrb_value) {
    char* path;
    mrb_get_args(mrb, "z", &path);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->SetActiveTheme(path)) ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_get_theme_color(mrb_state* mrb, mrb_value) {
    char* type; char* entry;
    mrb_get_args(mrb, "zz", &type, &entry);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return ColorToMrb(mrb, math::Color(1.0f, 1.0f, 1.0f, 1.0f));
    return ColorToMrb(mrb, api->GetThemeColor(type, entry));
}

static mrb_value mrb_script_api_get_theme_constant(mrb_state* mrb, mrb_value) {
    char* type; char* entry;
    mrb_get_args(mrb, "zz", &type, &entry);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_float_value(mrb, api ? api->GetThemeConstant(type, entry) : 0.0f);
}

static mrb_value mrb_script_api_set_palette_color(mrb_state* mrb, mrb_value) {
    char* key; mrb_float r, g, b, a = 1.0;
    mrb_get_args(mrb, "zfff|f", &key, &r, &g, &b, &a);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->SetThemePaletteColor(key, math::Color((float)r, (float)g, (float)b, (float)a)))
               ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_set_theme_variable(mrb_state* mrb, mrb_value) {
    char* key; mrb_float value;
    mrb_get_args(mrb, "zf", &key, &value);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->SetThemeVariable(key, (float)value)) ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_get_theme_version(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_fixnum_value(api ? static_cast<mrb_int>(api->GetThemeVersion()) : 0);
}

// ============================================================================
// WINDOW / DISPLAY (parity)
// ============================================================================

static mrb_value mrb_script_api_set_window_title(mrb_state* mrb, mrb_value) {
    char* title;
    mrb_get_args(mrb, "z", &title);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetWindowTitle(title);
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_window_title(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    std::string r = api ? api->GetWindowTitle() : "";
    return mrb_str_new(mrb, r.c_str(), r.size());
}

static mrb_value mrb_script_api_set_fullscreen(mrb_state* mrb, mrb_value) {
    mrb_bool fullscreen;
    mrb_get_args(mrb, "b", &fullscreen);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetFullscreen(fullscreen);
    return mrb_nil_value();
}

static mrb_value mrb_script_api_is_fullscreen(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->IsFullscreen()) ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_set_vsync(mrb_state* mrb, mrb_value) {
    mrb_bool enabled;
    mrb_get_args(mrb, "b", &enabled);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetVSync(enabled);
    return mrb_nil_value();
}

static mrb_value mrb_script_api_is_vsync(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->IsVSync()) ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_set_window_size(mrb_state* mrb, mrb_value) {
    mrb_int width, height;
    mrb_get_args(mrb, "ii", &width, &height);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetWindowSize(static_cast<int>(width), static_cast<int>(height));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_window_size(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return Vec2ToMrb(mrb, api ? api->GetWindowSize() : math::Vec2(0.0f, 0.0f));
}

static mrb_value mrb_script_api_get_screen_size(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return Vec2ToMrb(mrb, api ? api->GetScreenSize() : math::Vec2(0.0f, 0.0f));
}

static mrb_value mrb_script_api_maximize_window(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->MaximizeWindow();
    return mrb_nil_value();
}

static mrb_value mrb_script_api_minimize_window(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->MinimizeWindow();
    return mrb_nil_value();
}

static mrb_value mrb_script_api_restore_window(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->RestoreWindow();
    return mrb_nil_value();
}

static mrb_value mrb_script_api_set_mouse_mode(mrb_state* mrb, mrb_value) {
    mrb_int mode;
    mrb_get_args(mrb, "i", &mode);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetMouseMode(static_cast<int>(mode));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_mouse_mode(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_fixnum_value(api ? api->GetMouseMode() : 0);
}

static mrb_value mrb_script_api_set_mouse_cursor_visible(mrb_state* mrb, mrb_value) {
    mrb_bool visible;
    mrb_get_args(mrb, "b", &visible);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetMouseCursorVisible(visible);
    return mrb_nil_value();
}

static mrb_value mrb_script_api_is_mouse_cursor_visible(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->IsMouseCursorVisible()) ? mrb_true_value() : mrb_false_value();
}

// ============================================================================
// SCREEN <-> WORLD CONVERSION (parity)
// ============================================================================

static mrb_value mrb_script_api_screen_to_world_2d(mrb_state* mrb, mrb_value) {
    mrb_float x, y;
    mrb_get_args(mrb, "ff", &x, &y);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) return Vec2ToMrb(mrb, api->ScreenToWorld2D(mrbVec2(x, y)));
    return Vec2ToMrb(mrb, math::Vec2(0.0f, 0.0f));
}

static mrb_value mrb_script_api_world_to_screen_2d(mrb_state* mrb, mrb_value) {
    mrb_float x, y;
    mrb_get_args(mrb, "ff", &x, &y);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) return Vec2ToMrb(mrb, api->WorldToScreen2D(mrbVec2(x, y)));
    return Vec2ToMrb(mrb, math::Vec2(0.0f, 0.0f));
}

static mrb_value mrb_script_api_screen_to_world_3d(mrb_state* mrb, mrb_value) {
    mrb_float x, y, distance;
    mrb_get_args(mrb, "fff", &x, &y, &distance);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) return Vec3ToMrb(mrb, api->ScreenToWorld3D(mrbVec2(x, y), static_cast<float>(distance)));
    return Vec3ToMrb(mrb, math::Vec3(0.0f, 0.0f, 0.0f));
}

static mrb_value mrb_script_api_world_to_screen_3d(mrb_state* mrb, mrb_value) {
    mrb_float x, y, z;
    mrb_get_args(mrb, "fff", &x, &y, &z);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) return Vec3ToMrb(mrb, api->WorldToScreen3D(mrbVec3(x, y, z)));
    return Vec3ToMrb(mrb, math::Vec3(0.0f, 0.0f, 0.0f));
}

static mrb_value mrb_script_api_screen_to_world_ray_3d(mrb_state* mrb, mrb_value) {
    mrb_float x, y;
    mrb_get_args(mrb, "ff", &x, &y);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    mrb_value result = mrb_hash_new(mrb);
    if (api) {
        ScriptAPI::ScreenRay ray = api->ScreenToWorldRay3D(mrbVec2(x, y));
        mrb_hash_set(mrb, result, mrb_symbol_value(mrb_intern_lit(mrb, "origin")), Vec3ToMrb(mrb, ray.origin));
        mrb_hash_set(mrb, result, mrb_symbol_value(mrb_intern_lit(mrb, "direction")), Vec3ToMrb(mrb, ray.direction));
        return result;
    }
    mrb_hash_set(mrb, result, mrb_symbol_value(mrb_intern_lit(mrb, "origin")), Vec3ToMrb(mrb, math::Vec3(0.0f, 0.0f, 0.0f)));
    mrb_hash_set(mrb, result, mrb_symbol_value(mrb_intern_lit(mrb, "direction")), Vec3ToMrb(mrb, math::Vec3(0.0f, 0.0f, 0.0f)));
    return result;
}

static mrb_value mrb_script_api_play_audio_scheduled(mrb_state* mrb, mrb_value) {
    char* path; mrb_float delay;
    char* bus = const_cast<char*>("Master");
    mrb_bool loop = false; mrb_float volume = 1.0;
    mrb_get_args(mrb, "zf|zbf", &path, &delay, &bus, &loop, &volume);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        std::string id = api->PlayAudioScheduled(path, static_cast<float>(delay), bus, loop, static_cast<float>(volume));
        return mrb_str_new_cstr(mrb, id.c_str());
    }
    return mrb_str_new_cstr(mrb, "");
}

static mrb_value mrb_script_api_play_audio_scheduled_3d(mrb_state* mrb, mrb_value) {
    char* path; mrb_float x, y, z, delay;
    char* bus = const_cast<char*>("Master");
    mrb_bool loop = false; mrb_float volume = 1.0;
    mrb_get_args(mrb, "zffff|zbf", &path, &x, &y, &z, &delay, &bus, &loop, &volume);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        std::string id = api->PlayAudioScheduled3D(path, mrbVec3(x, y, z), static_cast<float>(delay),
                                                   bus, loop, static_cast<float>(volume));
        return mrb_str_new_cstr(mrb, id.c_str());
    }
    return mrb_str_new_cstr(mrb, "");
}

static mrb_value mrb_script_api_is_audio_playing(mrb_state* mrb, mrb_value) {
    char* uuid;
    mrb_get_args(mrb, "z", &uuid);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->IsAudioPlaying(uuid)) ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_is_audio_finished(mrb_state* mrb, mrb_value) {
    char* uuid;
    mrb_get_args(mrb, "z", &uuid);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->IsAudioFinished(uuid)) ? mrb_true_value() : mrb_false_value();
}

// ============================================================================
// GROUPS (parity)
// ============================================================================

static mrb_value mrb_script_api_add_to_group(mrb_state* mrb, mrb_value) {
    char* group;
    mrb_get_args(mrb, "z", &group);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->AddToGroup(group);
    return mrb_nil_value();
}

static mrb_value mrb_script_api_remove_from_group(mrb_state* mrb, mrb_value) {
    char* group;
    mrb_get_args(mrb, "z", &group);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->RemoveFromGroup(group);
    return mrb_nil_value();
}

static mrb_value mrb_script_api_is_in_group(mrb_state* mrb, mrb_value) {
    char* group;
    mrb_get_args(mrb, "z", &group);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->IsInGroup(group)) ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_get_groups(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    mrb_value arr = mrb_ary_new(mrb);
    if (api) {
        std::vector<std::string> groups = api->GetGroups();
        for (const std::string& g : groups) {
            mrb_ary_push(mrb, arr, mrb_str_new_cstr(mrb, g.c_str()));
        }
    }
    return arr;
}

static mrb_value mrb_script_api_get_nodes_in_group(mrb_state* mrb, mrb_value) {
    char* group;
    mrb_get_args(mrb, "z", &group);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    mrb_value arr = mrb_ary_new(mrb);
    if (api) {
        std::vector<core::Node*> nodes = api->GetNodesInGroup(group);
        for (core::Node* node : nodes) {
            mrb_ary_push(mrb, arr, WrapNodeRef(mrb, NodeRef::FromRaw(node, api)));
        }
    }
    return arr;
}

static mrb_value mrb_script_api_get_node_count_in_group(mrb_state* mrb, mrb_value) {
    char* group;
    mrb_get_args(mrb, "z", &group);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_fixnum_value(api ? api->GetNodeCountInGroup(group) : 0);
}

static mrb_value mrb_script_api_implements_interface(mrb_state* mrb, mrb_value) {
    char* name;
    mrb_get_args(mrb, "z", &name);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->ImplementsInterface(name)) ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_get_implemented_interfaces(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    mrb_value arr = mrb_ary_new(mrb);
    if (api) {
        std::vector<std::string> interfaces = api->GetImplementedInterfaces();
        for (const std::string& i : interfaces) {
            mrb_ary_push(mrb, arr, mrb_str_new_cstr(mrb, i.c_str()));
        }
    }
    return arr;
}

static mrb_value mrb_script_api_verify_interface(mrb_state* mrb, mrb_value) {
    char* name;
    mrb_get_args(mrb, "z", &name);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_nil_value();
    return JsonToMrb(mrb, api->VerifyInterface(name));
}

static mrb_value mrb_script_api_get_nodes_with_interface(mrb_state* mrb, mrb_value) {
    char* name;
    mrb_get_args(mrb, "z", &name);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    mrb_value arr = mrb_ary_new(mrb);
    if (api) {
        std::vector<core::Node*> nodes = api->GetNodesImplementingInterface(name);
        for (core::Node* node : nodes) {
            mrb_ary_push(mrb, arr, WrapNodeRef(mrb, NodeRef::FromRaw(node, api)));
        }
    }
    return arr;
}

static mrb_value mrb_script_api_get_node_count_with_interface(mrb_state* mrb, mrb_value) {
    char* name;
    mrb_get_args(mrb, "z", &name);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_fixnum_value(api ? api->GetNodeCountImplementingInterface(name) : 0);
}

static mrb_value mrb_script_api_get_first_node_with_interface(mrb_state* mrb, mrb_value) {
    char* name;
    mrb_get_args(mrb, "z", &name);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_nil_value();
    return WrapNodeRef(mrb, NodeRef::FromRaw(api->GetFirstNodeImplementingInterface(name), api));
}

static mrb_value mrb_script_api_interface_exists(mrb_state* mrb, mrb_value) {
    char* name;
    mrb_get_args(mrb, "z", &name);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->InterfaceExists(name)) ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_get_all_interfaces(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    mrb_value arr = mrb_ary_new(mrb);
    if (api) {
        std::vector<std::string> interfaces = api->GetAllInterfaces();
        for (const std::string& i : interfaces) {
            mrb_ary_push(mrb, arr, mrb_str_new_cstr(mrb, i.c_str()));
        }
    }
    return arr;
}

static mrb_value mrb_script_api_get_interface_definition(mrb_state* mrb, mrb_value) {
    char* name;
    mrb_get_args(mrb, "z", &name);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_nil_value();
    nlohmann::json def = api->GetInterfaceDefinition(name);
    if (def.is_null()) return mrb_nil_value();
    return JsonToMrb(mrb, def);
}

static mrb_value mrb_script_api_register_interface(mrb_state* mrb, mrb_value) {
    mrb_value def;
    mrb_get_args(mrb, "o", &def);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->RegisterInterface(MrbToJson(mrb, def))) ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_archetype_implements_interface(mrb_state* mrb, mrb_value) {
    char* className; char* name;
    mrb_get_args(mrb, "zz", &className, &name);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->ArchetypeImplementsInterface(className, name)) ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_get_archetypes_with_interface(mrb_state* mrb, mrb_value) {
    char* name;
    mrb_get_args(mrb, "z", &name);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    mrb_value arr = mrb_ary_new(mrb);
    if (api) {
        std::vector<std::string> classes = api->GetArchetypesImplementing(name);
        for (const std::string& c : classes) {
            mrb_ary_push(mrb, arr, mrb_str_new_cstr(mrb, c.c_str()));
        }
    }
    return arr;
}

static mrb_value mrb_script_api_get_first_node_in_group(mrb_state* mrb, mrb_value) {
    char* group;
    mrb_get_args(mrb, "z", &group);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_nil_value();
    return WrapNodeRef(mrb, NodeRef::FromRaw(api->GetFirstNodeInGroup(group), api));
}

static mrb_value mrb_script_api_get_node_or_null(mrb_state* mrb, mrb_value) {
    char* path;
    mrb_get_args(mrb, "z", &path);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_nil_value();
    return WrapNodeRef(mrb, NodeRef::FromRaw(api->GetNodeOrNull(path), api));
}

static mrb_value mrb_script_api_find_children(mrb_state* mrb, mrb_value) {
    char* typeName = const_cast<char*>(""); mrb_bool recursive = true;
    mrb_get_args(mrb, "z|b", &typeName, &recursive);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    mrb_value arr = mrb_ary_new(mrb);
    if (api) {
        std::vector<core::Node*> nodes = api->FindChildren(typeName, recursive);
        for (core::Node* node : nodes) {
            mrb_ary_push(mrb, arr, WrapNodeRef(mrb, NodeRef::FromRaw(node, api)));
        }
    }
    return arr;
}

static mrb_value mrb_script_api_is_ancestor_of(mrb_state* mrb, mrb_value) {
    mrb_value other;
    mrb_get_args(mrb, "o", &other);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    NodeRef* ref = ArgNode(other);
    core::Node* raw = ref ? ref->Lock().get() : nullptr;
    return (api && api->IsAncestorOf(raw)) ? mrb_true_value() : mrb_false_value();
}

// ============================================================================
// ASSETS (parity)
// ============================================================================

static mrb_value mrb_script_api_load_image_asset(mrb_state* mrb, mrb_value) {
    char* path;
    mrb_get_args(mrb, "z", &path);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->LoadImageAsset(path)) ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_load_audio_asset(mrb_state* mrb, mrb_value) {
    char* path; char* loadMode = const_cast<char*>("preload");
    mrb_get_args(mrb, "z|z", &path, &loadMode);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->LoadAudioAsset(path, loadMode)) ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_load_model_asset(mrb_state* mrb, mrb_value) {
    char* path;
    mrb_get_args(mrb, "z", &path);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->LoadModelAsset(path)) ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_preload_assets(mrb_state* mrb, mrb_value) {
    mrb_value arr;
    mrb_get_args(mrb, "A", &arr);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) {
        std::vector<std::string> paths;
        mrb_int len = RARRAY_LEN(arr);
        for (mrb_int i = 0; i < len; ++i) {
            mrb_value item = mrb_obj_as_string(mrb, mrb_ary_ref(mrb, arr, i));
            paths.push_back(std::string(RSTRING_PTR(item), RSTRING_LEN(item)));
        }
        api->PreloadAssets(paths);
    }
    return mrb_nil_value();
}

// ============================================================================
// SCENE (parity)
// ============================================================================

static mrb_value mrb_script_api_add_scene(mrb_state* mrb, mrb_value) {
    char* path;
    mrb_get_args(mrb, "z", &path);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->AddScene(path);
    return mrb_nil_value();
}

static mrb_value mrb_script_api_remove_scene(mrb_state* mrb, mrb_value) {
    char* name;
    mrb_get_args(mrb, "z", &name);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->RemoveScene(name);
    return mrb_nil_value();
}

static mrb_value mrb_script_api_get_current_scene_path(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) return mrb_str_new_cstr(mrb, api->GetCurrentScenePath().c_str());
    return mrb_str_new_cstr(mrb, "");
}

// ============================================================================
// NODE (flat module-level helpers, parity)
// ============================================================================

static mrb_value mrb_script_api_has_node(mrb_state* mrb, mrb_value) {
    char* path;
    mrb_get_args(mrb, "z", &path);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return (api && api->HasNode(path)) ? mrb_true_value() : mrb_false_value();
}

static mrb_value mrb_script_api_set_sibling_index(mrb_state* mrb, mrb_value) {
    mrb_int index;
    mrb_get_args(mrb, "i", &index);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetSiblingIndex(static_cast<int>(index));
    return mrb_nil_value();
}

// ===========================================================================
// Timer / Scene / Tree object model
// ===========================================================================

static void timerref_free(mrb_state*, void* ptr) {
    delete static_cast<TimerRef*>(ptr);
}
static void sceneref_free(mrb_state*, void* ptr) {
    delete static_cast<SceneRef*>(ptr);
}
static void treeref_free(mrb_state*, void* ptr) {
    delete static_cast<TreeRef*>(ptr);
}
static void tweenref_free(mrb_state*, void* ptr) {
    delete static_cast<TweenRef*>(ptr);
}
static void awaiterref_free(mrb_state*, void* ptr) {
    delete static_cast<SignalAwaiter*>(ptr);
}
static void sequenceref_free(mrb_state*, void* ptr) {
    delete static_cast<SequenceRef*>(ptr);
}

static const mrb_data_type TimerRefDataType = { "LupineTimer", timerref_free };
static const mrb_data_type SceneRefDataType = { "LupineScene", sceneref_free };
static const mrb_data_type TreeRefDataType  = { "LupineTree",  treeref_free };
static const mrb_data_type TweenRefDataType = { "LupineTween", tweenref_free };
static const mrb_data_type SignalAwaiterDataType = { "LupineSignalAwaiter", awaiterref_free };
static const mrb_data_type SequenceRefDataType = { "LupineSequence", sequenceref_free };

static mrb_value WrapTimerRef(mrb_state* mrb, const TimerRef& ref) {
    if (!ref.IsValid()) return mrb_nil_value();
    if (!mrb_class_defined(mrb, "LupineTimer")) return mrb_nil_value();
    struct RClass* cls = mrb_class_get(mrb, "LupineTimer");
    TimerRef* heap = new TimerRef(ref);
    return mrb_obj_value(Data_Wrap_Struct(mrb, cls, &TimerRefDataType, heap));
}
static mrb_value WrapSceneRef(mrb_state* mrb, const SceneRef& ref) {
    if (!ref.IsValid()) return mrb_nil_value();
    if (!mrb_class_defined(mrb, "LupineScene")) return mrb_nil_value();
    struct RClass* cls = mrb_class_get(mrb, "LupineScene");
    SceneRef* heap = new SceneRef(ref);
    return mrb_obj_value(Data_Wrap_Struct(mrb, cls, &SceneRefDataType, heap));
}
static mrb_value WrapTreeRef(mrb_state* mrb, const TreeRef& ref) {
    if (!ref.IsValid()) return mrb_nil_value();
    if (!mrb_class_defined(mrb, "LupineTree")) return mrb_nil_value();
    struct RClass* cls = mrb_class_get(mrb, "LupineTree");
    TreeRef* heap = new TreeRef(ref);
    return mrb_obj_value(Data_Wrap_Struct(mrb, cls, &TreeRefDataType, heap));
}
static mrb_value WrapTweenRef(mrb_state* mrb, const TweenRef& ref) {
    if (!ref.IsValid()) return mrb_nil_value();
    if (!mrb_class_defined(mrb, "LupineTween")) return mrb_nil_value();
    struct RClass* cls = mrb_class_get(mrb, "LupineTween");
    TweenRef* heap = new TweenRef(ref);
    return mrb_obj_value(Data_Wrap_Struct(mrb, cls, &TweenRefDataType, heap));
}
static mrb_value WrapSignalAwaiter(mrb_state* mrb, const SignalAwaiter& ref) {
    if (!ref.IsValid()) return mrb_nil_value();
    if (!mrb_class_defined(mrb, "LupineSignalAwaiter")) return mrb_nil_value();
    struct RClass* cls = mrb_class_get(mrb, "LupineSignalAwaiter");
    SignalAwaiter* heap = new SignalAwaiter(ref);
    return mrb_obj_value(Data_Wrap_Struct(mrb, cls, &SignalAwaiterDataType, heap));
}
static mrb_value WrapSequenceRef(mrb_state* mrb, const SequenceRef& ref) {
    if (!ref.IsValid()) return mrb_nil_value();
    if (!mrb_class_defined(mrb, "LupineSequence")) return mrb_nil_value();
    struct RClass* cls = mrb_class_get(mrb, "LupineSequence");
    SequenceRef* heap = new SequenceRef(ref);
    return mrb_obj_value(Data_Wrap_Struct(mrb, cls, &SequenceRefDataType, heap));
}

static TimerRef* SelfTimer(mrb_value self) {
    return SelfData<TimerRef>(self, &TimerRefDataType);
}
static SceneRef* SelfScene(mrb_value self) {
    return SelfData<SceneRef>(self, &SceneRefDataType);
}
static TreeRef* SelfTree(mrb_value self) {
    return SelfData<TreeRef>(self, &TreeRefDataType);
}
static TweenRef* SelfTween(mrb_value self) {
    return SelfData<TweenRef>(self, &TweenRefDataType);
}
static SignalAwaiter* SelfAwaiter(mrb_value self) {
    return SelfData<SignalAwaiter>(self, &SignalAwaiterDataType);
}
static SequenceRef* SelfSeq(mrb_value self) {
    return SelfData<SequenceRef>(self, &SequenceRefDataType);
}

// --- Timer handle methods --------------------------------------------------

static mrb_value mrb_timer_is_valid(mrb_state*, mrb_value self) {
    return mrb_bool_value(SelfTimer(self)->IsValid());
}
static mrb_value mrb_timer_get_name(mrb_state* mrb, mrb_value self) {
    return mrb_str_new_cstr(mrb, SelfTimer(self)->GetName().c_str());
}
static mrb_value mrb_timer_start(mrb_state*, mrb_value self) {
    SelfTimer(self)->Start();
    return mrb_nil_value();
}
static mrb_value mrb_timer_stop(mrb_state*, mrb_value self) {
    SelfTimer(self)->Stop();
    return mrb_nil_value();
}
static mrb_value mrb_timer_reset(mrb_state*, mrb_value self) {
    SelfTimer(self)->Reset();
    return mrb_nil_value();
}
static mrb_value mrb_timer_restart(mrb_state*, mrb_value self) {
    SelfTimer(self)->Restart();
    return mrb_nil_value();
}
static mrb_value mrb_timer_remove(mrb_state*, mrb_value self) {
    SelfTimer(self)->Remove();
    return mrb_nil_value();
}
static mrb_value mrb_timer_is_running(mrb_state*, mrb_value self) {
    return mrb_bool_value(SelfTimer(self)->IsRunning());
}
static mrb_value mrb_timer_is_finished(mrb_state*, mrb_value self) {
    return mrb_bool_value(SelfTimer(self)->IsFinished());
}
static mrb_value mrb_timer_get_time_left(mrb_state* mrb, mrb_value self) {
    return mrb_float_value(mrb, SelfTimer(self)->GetTimeLeft());
}
static mrb_value mrb_timer_get_fire_count(mrb_state*, mrb_value self) {
    return mrb_fixnum_value(SelfTimer(self)->GetFireCount());
}
static mrb_value mrb_timer_get_duration(mrb_state* mrb, mrb_value self) {
    return mrb_float_value(mrb, SelfTimer(self)->GetDuration());
}
static mrb_value mrb_timer_set_duration(mrb_state* mrb, mrb_value self) {
    mrb_float v; mrb_get_args(mrb, "f", &v);
    SelfTimer(self)->SetDuration(static_cast<float>(v));
    return mrb_nil_value();
}
static mrb_value mrb_timer_get_elapsed(mrb_state* mrb, mrb_value self) {
    return mrb_float_value(mrb, SelfTimer(self)->GetElapsed());
}
static mrb_value mrb_timer_set_elapsed(mrb_state* mrb, mrb_value self) {
    mrb_float v; mrb_get_args(mrb, "f", &v);
    SelfTimer(self)->SetElapsed(static_cast<float>(v));
    return mrb_nil_value();
}
static mrb_value mrb_timer_get_loop(mrb_state*, mrb_value self) {
    return mrb_bool_value(SelfTimer(self)->GetLoop());
}
static mrb_value mrb_timer_set_loop(mrb_state* mrb, mrb_value self) {
    mrb_bool v; mrb_get_args(mrb, "b", &v);
    SelfTimer(self)->SetLoop(v);
    return mrb_nil_value();
}
static mrb_value mrb_timer_get_repeat_count(mrb_state*, mrb_value self) {
    return mrb_fixnum_value(SelfTimer(self)->GetRepeatCount());
}
static mrb_value mrb_timer_set_repeat_count(mrb_state* mrb, mrb_value self) {
    mrb_int v; mrb_get_args(mrb, "i", &v);
    SelfTimer(self)->SetRepeatCount(static_cast<int>(v));
    return mrb_nil_value();
}
static mrb_value mrb_timer_get_owner(mrb_state* mrb, mrb_value self) {
    return WrapNodeRef(mrb, SelfTimer(self)->GetOwner());
}
static mrb_value mrb_timer_as_component(mrb_state* mrb, mrb_value self) {
    return WrapComponentRef(mrb, SelfTimer(self)->AsComponent());
}

// --- Scene handle methods --------------------------------------------------

static mrb_value mrb_scene_is_valid(mrb_state*, mrb_value self) {
    return mrb_bool_value(SelfScene(self)->IsValid());
}
static mrb_value mrb_scene_get_name(mrb_state* mrb, mrb_value self) {
    return mrb_str_new_cstr(mrb, SelfScene(self)->GetName().c_str());
}
static mrb_value mrb_scene_get_path(mrb_state* mrb, mrb_value self) {
    return mrb_str_new_cstr(mrb, SelfScene(self)->GetPath().c_str());
}
static mrb_value mrb_scene_get_root(mrb_state* mrb, mrb_value self) {
    return WrapNodeRef(mrb, SelfScene(self)->GetRoot());
}
static mrb_value mrb_scene_find_node(mrb_state* mrb, mrb_value self) {
    char* path; mrb_get_args(mrb, "z", &path);
    return WrapNodeRef(mrb, SelfScene(self)->FindNode(path));
}
static mrb_value mrb_scene_find_node_by_uuid(mrb_state* mrb, mrb_value self) {
    char* uuid; mrb_get_args(mrb, "z", &uuid);
    return WrapNodeRef(mrb, SelfScene(self)->FindNodeByUUID(uuid));
}

// --- Tree handle methods ---------------------------------------------------

static mrb_value mrb_tree_is_valid(mrb_state*, mrb_value self) {
    return mrb_bool_value(SelfTree(self)->IsValid());
}
static mrb_value mrb_tree_get_root(mrb_state* mrb, mrb_value self) {
    return WrapNodeRef(mrb, SelfTree(self)->GetRoot());
}
static mrb_value mrb_tree_get_current_scene(mrb_state* mrb, mrb_value self) {
    return WrapSceneRef(mrb, SelfTree(self)->GetCurrentScene());
}
static mrb_value mrb_tree_get_current_scene_path(mrb_state* mrb, mrb_value self) {
    return mrb_str_new_cstr(mrb, SelfTree(self)->GetCurrentScenePath().c_str());
}
static mrb_value mrb_tree_change_scene(mrb_state* mrb, mrb_value self) {
    char* path; mrb_get_args(mrb, "z", &path);
    SelfTree(self)->ChangeScene(path);
    return mrb_nil_value();
}
static mrb_value mrb_tree_reload_scene(mrb_state*, mrb_value self) {
    SelfTree(self)->ReloadScene();
    return mrb_nil_value();
}
static mrb_value mrb_tree_add_scene(mrb_state* mrb, mrb_value self) {
    char* path; mrb_get_args(mrb, "z", &path);
    SelfTree(self)->AddScene(path);
    return mrb_nil_value();
}
static mrb_value mrb_tree_remove_scene(mrb_state* mrb, mrb_value self) {
    char* name; mrb_get_args(mrb, "z", &name);
    SelfTree(self)->RemoveScene(name);
    return mrb_nil_value();
}

// --- Tween handle methods --------------------------------------------------

static mrb_value mrb_tween_is_valid(mrb_state*, mrb_value self) {
    return mrb_bool_value(SelfTween(self)->IsValid());
}
static mrb_value mrb_tween_get_name(mrb_state* mrb, mrb_value self) {
    return mrb_str_new_cstr(mrb, SelfTween(self)->GetName().c_str());
}
static mrb_value mrb_tween_play(mrb_state*, mrb_value self) {
    SelfTween(self)->Play();
    return mrb_nil_value();
}
static mrb_value mrb_tween_pause(mrb_state*, mrb_value self) {
    SelfTween(self)->Pause();
    return mrb_nil_value();
}
static mrb_value mrb_tween_stop(mrb_state*, mrb_value self) {
    SelfTween(self)->Stop();
    return mrb_nil_value();
}
static mrb_value mrb_tween_restart(mrb_state*, mrb_value self) {
    SelfTween(self)->Restart();
    return mrb_nil_value();
}
static mrb_value mrb_tween_kill(mrb_state*, mrb_value self) {
    SelfTween(self)->Kill();
    return mrb_nil_value();
}
static mrb_value mrb_tween_is_running(mrb_state*, mrb_value self) {
    return mrb_bool_value(SelfTween(self)->IsRunning());
}
static mrb_value mrb_tween_is_finished(mrb_state*, mrb_value self) {
    return mrb_bool_value(SelfTween(self)->IsFinished());
}
static mrb_value mrb_tween_get_progress(mrb_state* mrb, mrb_value self) {
    return mrb_float_value(mrb, SelfTween(self)->GetProgress());
}
static mrb_value mrb_tween_get_duration(mrb_state* mrb, mrb_value self) {
    return mrb_float_value(mrb, SelfTween(self)->GetDuration());
}
static mrb_value mrb_tween_set_duration(mrb_state* mrb, mrb_value self) {
    mrb_float v; mrb_get_args(mrb, "f", &v);
    SelfTween(self)->SetDuration(static_cast<float>(v));
    return mrb_nil_value();
}
static mrb_value mrb_tween_get_easing(mrb_state* mrb, mrb_value self) {
    return mrb_str_new_cstr(mrb, SelfTween(self)->GetEasing().c_str());
}
static mrb_value mrb_tween_set_easing(mrb_state* mrb, mrb_value self) {
    char* e; mrb_get_args(mrb, "z", &e);
    SelfTween(self)->SetEasing(e);
    return mrb_nil_value();
}
static mrb_value mrb_tween_get_loop(mrb_state*, mrb_value self) {
    return mrb_bool_value(SelfTween(self)->GetLoop());
}
static mrb_value mrb_tween_set_loop(mrb_state* mrb, mrb_value self) {
    mrb_bool v; mrb_get_args(mrb, "b", &v);
    SelfTween(self)->SetLoop(v);
    return mrb_nil_value();
}
static mrb_value mrb_tween_set_auto_remove(mrb_state* mrb, mrb_value self) {
    mrb_bool v; mrb_get_args(mrb, "b", &v);
    SelfTween(self)->SetAutoRemove(v);
    return mrb_nil_value();
}
static mrb_value mrb_tween_get_owner(mrb_state* mrb, mrb_value self) {
    return WrapNodeRef(mrb, SelfTween(self)->GetOwner());
}
static mrb_value mrb_tween_as_component(mrb_state* mrb, mrb_value self) {
    return WrapComponentRef(mrb, SelfTween(self)->AsComponent());
}

// --- Signal awaiter handle methods -----------------------------------------

static mrb_value mrb_awaiter_is_valid(mrb_state*, mrb_value self) {
    return mrb_bool_value(SelfAwaiter(self)->IsValid());
}
static mrb_value mrb_awaiter_is_fired(mrb_state*, mrb_value self) {
    return mrb_bool_value(SelfAwaiter(self)->IsFired());
}
static mrb_value mrb_awaiter_reset(mrb_state*, mrb_value self) {
    SelfAwaiter(self)->Reset();
    return mrb_nil_value();
}
static mrb_value mrb_awaiter_cancel(mrb_state*, mrb_value self) {
    SelfAwaiter(self)->Cancel();
    return mrb_nil_value();
}

// --- Tween sequence handle methods -----------------------------------------

static mrb_value mrb_seq_is_valid(mrb_state*, mrb_value self) {
    return mrb_bool_value(SelfSeq(self)->IsValid());
}
static mrb_value mrb_seq_get_name(mrb_state* mrb, mrb_value self) {
    return mrb_str_new_cstr(mrb, SelfSeq(self)->GetName().c_str());
}
static mrb_value mrb_seq_append(mrb_state* mrb, mrb_value self) {
    char* channel; mrb_value toVal; mrb_float duration;
    char* easing = nullptr; mrb_bool parallel = false;
    mrb_get_args(mrb, "zof|zb", &channel, &toVal, &duration, &easing, &parallel);
    SelfSeq(self)->Append(channel, MrbToJson(mrb, toVal), static_cast<float>(duration),
                          easing ? easing : "linear", parallel);
    return self;
}
static mrb_value mrb_seq_append_on(mrb_state* mrb, mrb_value self) {
    mrb_value targetVal; char* channel; mrb_value toVal; mrb_float duration;
    char* easing = nullptr; mrb_bool parallel = false;
    mrb_get_args(mrb, "ozof|zb", &targetVal, &channel, &toVal, &duration, &easing, &parallel);
    NodeRef* target = ArgNode(targetVal);
    if (target) {
        SelfSeq(self)->AppendOn(*target, channel, MrbToJson(mrb, toVal),
                                static_cast<float>(duration), easing ? easing : "linear", parallel);
    }
    return self;
}
static mrb_value mrb_seq_append_interval(mrb_state* mrb, mrb_value self) {
    mrb_float duration; mrb_bool parallel = false;
    mrb_get_args(mrb, "f|b", &duration, &parallel);
    SelfSeq(self)->AppendInterval(static_cast<float>(duration), parallel);
    return self;
}
static mrb_value mrb_seq_append_callback(mrb_state* mrb, mrb_value self) {
    char* method; mrb_bool parallel = false;
    mrb_get_args(mrb, "z|b", &method, &parallel);
    SelfSeq(self)->AppendCallback(method, parallel);
    return self;
}
static mrb_value mrb_seq_append_callback_on(mrb_state* mrb, mrb_value self) {
    mrb_value targetVal; char* method; mrb_bool parallel = false;
    mrb_get_args(mrb, "oz|b", &targetVal, &method, &parallel);
    NodeRef* target = ArgNode(targetVal);
    if (target) {
        SelfSeq(self)->AppendCallbackOn(*target, method, parallel);
    }
    return self;
}
static mrb_value mrb_seq_play(mrb_state*, mrb_value self) {
    SelfSeq(self)->Play();
    return self;
}
static mrb_value mrb_seq_stop(mrb_state*, mrb_value self) {
    SelfSeq(self)->Stop();
    return mrb_nil_value();
}
static mrb_value mrb_seq_reset(mrb_state*, mrb_value self) {
    SelfSeq(self)->Reset();
    return mrb_nil_value();
}
static mrb_value mrb_seq_restart(mrb_state*, mrb_value self) {
    SelfSeq(self)->Restart();
    return mrb_nil_value();
}
static mrb_value mrb_seq_kill(mrb_state*, mrb_value self) {
    SelfSeq(self)->Kill();
    return mrb_nil_value();
}
static mrb_value mrb_seq_is_running(mrb_state*, mrb_value self) {
    return mrb_bool_value(SelfSeq(self)->IsRunning());
}
static mrb_value mrb_seq_is_finished(mrb_state*, mrb_value self) {
    return mrb_bool_value(SelfSeq(self)->IsFinished());
}
static mrb_value mrb_seq_set_loops(mrb_state* mrb, mrb_value self) {
    mrb_int loops; mrb_get_args(mrb, "i", &loops);
    SelfSeq(self)->SetLoops(static_cast<int>(loops));
    return self;
}
static mrb_value mrb_seq_get_loops(mrb_state*, mrb_value self) {
    return mrb_fixnum_value(SelfSeq(self)->GetLoops());
}
static mrb_value mrb_seq_set_auto_remove(mrb_state* mrb, mrb_value self) {
    mrb_bool v; mrb_get_args(mrb, "b", &v);
    SelfSeq(self)->SetAutoRemove(v);
    return self;
}
static mrb_value mrb_seq_get_step_count(mrb_state*, mrb_value self) {
    return mrb_fixnum_value(SelfSeq(self)->GetStepCount());
}
static mrb_value mrb_seq_get_owner(mrb_state* mrb, mrb_value self) {
    return WrapNodeRef(mrb, SelfSeq(self)->GetOwner());
}
static mrb_value mrb_seq_as_component(mrb_state* mrb, mrb_value self) {
    return WrapComponentRef(mrb, SelfSeq(self)->AsComponent());
}

// --- Lupine module: root / scene / tree / instancing / timers --------------

static mrb_value mrb_script_api_get_root(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_nil_value();
    return WrapNodeRef(mrb, NodeRef::FromRaw(api->GetRoot(), api));
}
static mrb_value mrb_script_api_get_scene(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_nil_value();
    return WrapSceneRef(mrb, SceneRef(api->GetScene(), api));
}
static mrb_value mrb_script_api_get_tree(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_nil_value();
    return WrapTreeRef(mrb, TreeRef(api->GetTree(), api));
}
static mrb_value mrb_script_api_get_sibling_index(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_fixnum_value(api ? api->GetSiblingIndex() : -1);
}
static mrb_value mrb_script_api_instantiate_prefab(mrb_state* mrb, mrb_value) {
    char* path; mrb_value parentVal = mrb_nil_value();
    mrb_get_args(mrb, "z|o", &path, &parentVal);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_nil_value();
    NodeRef* parent = ArgNode(parentVal);
    core::Node* parentNode = parent ? parent->Lock().get() : nullptr;
    return WrapNodeRef(mrb, NodeRef::FromRaw(api->InstantiatePrefab(path, parentNode), api));
}
static mrb_value mrb_script_api_instantiate_scene(mrb_state* mrb, mrb_value) {
    char* path; mrb_value parentVal = mrb_nil_value();
    mrb_get_args(mrb, "z|o", &path, &parentVal);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_nil_value();
    NodeRef* parent = ArgNode(parentVal);
    core::Node* parentNode = parent ? parent->Lock().get() : nullptr;
    return WrapNodeRef(mrb, NodeRef::FromRaw(api->InstantiateScene(path, parentNode), api));
}
static mrb_value mrb_script_api_create_node(mrb_state* mrb, mrb_value) {
    char* type; char* name = nullptr;
    mrb_get_args(mrb, "z|z", &type, &name);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_nil_value();
    core::Node* node = name ? api->CreateNode(type, name) : api->CreateNode(type);
    return WrapNodeRef(mrb, NodeRef::FromRaw(node, api));
}
static mrb_value mrb_script_api_create_node_child(mrb_state* mrb, mrb_value) {
    char* type; char* name; mrb_value parentVal = mrb_nil_value();
    mrb_get_args(mrb, "zz|o", &type, &name, &parentVal);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_nil_value();
    NodeRef* parent = ArgNode(parentVal);
    core::Node* parentNode = parent ? parent->Lock().get() : nullptr;
    return WrapNodeRef(mrb, NodeRef::FromRaw(api->CreateNodeChild(type, name, parentNode), api));
}
static mrb_value mrb_script_api_duplicate_node(mrb_state* mrb, mrb_value) {
    mrb_value nodeVal; mrb_get_args(mrb, "o", &nodeVal);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_nil_value();
    NodeRef* n = ArgNode(nodeVal);
    core::Node* raw = n ? n->Lock().get() : nullptr;
    return WrapNodeRef(mrb, NodeRef::FromRaw(api->DuplicateNode(raw), api));
}
static mrb_value mrb_script_api_create_timer(mrb_state* mrb, mrb_value) {
    mrb_float delay; char* callback = nullptr;
    mrb_get_args(mrb, "f|z", &delay, &callback);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_nil_value();
    core::Component* timer = api->CreateTimerComponent(static_cast<float>(delay),
                                                       callback ? callback : "", false, -1, "");
    return WrapTimerRef(mrb, TimerRef::FromComponent(timer, api));
}
static mrb_value mrb_script_api_create_repeating_timer(mrb_state* mrb, mrb_value) {
    mrb_float interval; char* callback = nullptr; mrb_int repeatCount = -1;
    mrb_get_args(mrb, "f|zi", &interval, &callback, &repeatCount);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_nil_value();
    core::Component* timer = api->CreateTimerComponent(static_cast<float>(interval),
                                                       callback ? callback : "", true,
                                                       static_cast<int>(repeatCount), "");
    return WrapTimerRef(mrb, TimerRef::FromComponent(timer, api));
}
static mrb_value mrb_script_api_create_named_timer(mrb_state* mrb, mrb_value) {
    char* name; mrb_float delay; char* callback = nullptr;
    mrb_bool repeating = false; mrb_int repeatCount = -1;
    mrb_get_args(mrb, "zf|zbi", &name, &delay, &callback, &repeating, &repeatCount);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_nil_value();
    core::Component* timer = api->CreateTimerComponent(static_cast<float>(delay),
                                                       callback ? callback : "", repeating,
                                                       static_cast<int>(repeatCount), name);
    return WrapTimerRef(mrb, TimerRef::FromComponent(timer, api));
}
static mrb_value mrb_script_api_list_timers(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    mrb_value arr = mrb_ary_new(mrb);
    if (!api) return arr;
    for (core::Component* timer : api->ListTimers()) {
        TimerRef ref = TimerRef::FromComponent(timer, api);
        if (ref.IsValid()) mrb_ary_push(mrb, arr, WrapTimerRef(mrb, ref));
    }
    return arr;
}
static mrb_value mrb_script_api_create_tween(mrb_state* mrb, mrb_value) {
    char* channel; mrb_value toVal; mrb_float duration; char* easing = nullptr;
    mrb_get_args(mrb, "zof|z", &channel, &toVal, &duration, &easing);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_nil_value();
    core::Component* tween = api->CreateTweenComponent(channel, MrbToJson(mrb, toVal),
                                                       static_cast<float>(duration),
                                                       easing ? easing : "linear", nullptr);
    return WrapTweenRef(mrb, TweenRef::FromComponent(tween, api));
}
static mrb_value mrb_script_api_list_tweens(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    mrb_value arr = mrb_ary_new(mrb);
    if (!api) return arr;
    for (core::Component* tween : api->ListTweens()) {
        TweenRef ref = TweenRef::FromComponent(tween, api);
        if (ref.IsValid()) mrb_ary_push(mrb, arr, WrapTweenRef(mrb, ref));
    }
    return arr;
}
static mrb_value mrb_script_api_create_sequence(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_nil_value();
    return WrapSequenceRef(mrb, NodeRef::FromRaw(api->GetSelf(), api).CreateSequence());
}

// --- Sandboxed file I/O + JSON (res:// user:// temp:// only) ----------------

static mrb_value mrb_script_api_read_text(mrb_state* mrb, mrb_value) {
    char* path; mrb_get_args(mrb, "z", &path);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    std::string out;
    if (api && api->ReadTextFile(path, out)) {
        return mrb_str_new(mrb, out.data(), out.size());
    }
    return mrb_nil_value();
}
static mrb_value mrb_script_api_write_text(mrb_state* mrb, mrb_value) {
    char* path; char* text; mrb_int tlen;
    mrb_get_args(mrb, "zs", &path, &text, &tlen);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_bool_value(api && api->WriteTextFile(path, std::string(text, tlen)));
}
static mrb_value mrb_script_api_append_text(mrb_state* mrb, mrb_value) {
    char* path; char* text; mrb_int tlen;
    mrb_get_args(mrb, "zs", &path, &text, &tlen);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_bool_value(api && api->AppendTextFile(path, std::string(text, tlen)));
}
static mrb_value mrb_script_api_read_bytes(mrb_state* mrb, mrb_value) {
    char* path; mrb_get_args(mrb, "z", &path);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    std::vector<uint8_t> data;
    if (api && api->ReadBytesFile(path, data)) {
        return mrb_str_new(mrb, reinterpret_cast<const char*>(data.data()), data.size());
    }
    return mrb_nil_value();
}
static mrb_value mrb_script_api_write_bytes(mrb_state* mrb, mrb_value) {
    char* path; char* buf; mrb_int len;
    mrb_get_args(mrb, "zs", &path, &buf, &len);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    std::vector<uint8_t> data(buf, buf + len);
    return mrb_bool_value(api && api->WriteBytesFile(path, data));
}
static mrb_value mrb_script_api_file_exists(mrb_state* mrb, mrb_value) {
    char* path; mrb_get_args(mrb, "z", &path);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_bool_value(api && api->FileExists(path));
}
static mrb_value mrb_script_api_is_file(mrb_state* mrb, mrb_value) {
    char* path; mrb_get_args(mrb, "z", &path);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_bool_value(api && api->FileIsFile(path));
}
static mrb_value mrb_script_api_is_dir(mrb_state* mrb, mrb_value) {
    char* path; mrb_get_args(mrb, "z", &path);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_bool_value(api && api->FileIsDirectory(path));
}
static mrb_value mrb_script_api_remove_file(mrb_state* mrb, mrb_value) {
    char* path; mrb_get_args(mrb, "z", &path);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_bool_value(api && api->DeleteFilePath(path));
}
static mrb_value mrb_script_api_make_dir(mrb_state* mrb, mrb_value) {
    char* path; mrb_get_args(mrb, "z", &path);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_bool_value(api && api->MakeDirectory(path));
}
static mrb_value mrb_script_api_list_dir(mrb_state* mrb, mrb_value) {
    char* path; mrb_get_args(mrb, "z", &path);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    mrb_value arr = mrb_ary_new(mrb);
    if (api) {
        for (const std::string& name : api->ListDirectory(path)) {
            mrb_ary_push(mrb, arr, mrb_str_new(mrb, name.data(), name.size()));
        }
    }
    return arr;
}
static mrb_value mrb_script_api_file_size(mrb_state* mrb, mrb_value) {
    char* path; mrb_get_args(mrb, "z", &path);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    int64_t sz = api ? api->GetFileSize(path) : -1;
    return mrb_fixnum_value(static_cast<mrb_int>(sz));
}
static mrb_value mrb_script_api_to_json(mrb_state* mrb, mrb_value) {
    mrb_value val; mrb_bool pretty = false;
    mrb_get_args(mrb, "o|b", &val, &pretty);
    nlohmann::json j = MrbToJson(mrb, val);
    std::string s = pretty ? j.dump(2) : j.dump();
    return mrb_str_new(mrb, s.data(), s.size());
}
static mrb_value mrb_script_api_from_json(mrb_state* mrb, mrb_value) {
    char* str; mrb_int len;
    mrb_get_args(mrb, "s", &str, &len);
    try {
        nlohmann::json j = nlohmann::json::parse(std::string(str, len));
        return JsonToMrb(mrb, j);
    } catch (...) {
        return mrb_nil_value();
    }
}
static mrb_value mrb_script_api_read_json(mrb_state* mrb, mrb_value) {
    char* path; mrb_get_args(mrb, "z", &path);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    std::string text;
    if (!api || !api->ReadTextFile(path, text)) return mrb_nil_value();
    try {
        return JsonToMrb(mrb, nlohmann::json::parse(text));
    } catch (...) {
        return mrb_nil_value();
    }
}
static mrb_value mrb_script_api_write_json(mrb_state* mrb, mrb_value) {
    char* path; mrb_value val; mrb_bool pretty = false;
    mrb_get_args(mrb, "zo|b", &path, &val, &pretty);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_false_value();
    nlohmann::json j = MrbToJson(mrb, val);
    std::string s = pretty ? j.dump(2) : j.dump();
    return mrb_bool_value(api->WriteTextFile(path, s));
}
static mrb_value mrb_script_api_save_game(mrb_state* mrb, mrb_value) {
    char* slot; mrb_value data; mrb_value meta = mrb_nil_value();
    mrb_get_args(mrb, "zo|o", &slot, &data, &meta);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_false_value();
    nlohmann::json metaJson = mrb_nil_p(meta) ? nlohmann::json::object() : MrbToJson(mrb, meta);
    return mrb_bool_value(api->SaveGame(slot, MrbToJson(mrb, data), metaJson));
}
static mrb_value mrb_script_api_load_game(mrb_state* mrb, mrb_value) {
    char* slot; mrb_get_args(mrb, "z", &slot);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_nil_value();
    nlohmann::json result = api->LoadGame(slot);
    if (result.is_null()) return mrb_nil_value();
    return JsonToMrb(mrb, result);
}
static mrb_value mrb_script_api_save_slot_exists(mrb_state* mrb, mrb_value) {
    char* slot; mrb_get_args(mrb, "z", &slot);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_bool_value(api && api->SaveSlotExists(slot));
}
static mrb_value mrb_script_api_delete_save_slot(mrb_state* mrb, mrb_value) {
    char* slot; mrb_get_args(mrb, "z", &slot);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_bool_value(api && api->DeleteSaveSlot(slot));
}
static mrb_value mrb_script_api_copy_save_slot(mrb_state* mrb, mrb_value) {
    char* from; char* to; mrb_get_args(mrb, "zz", &from, &to);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_bool_value(api && api->CopySaveSlot(from, to, true));
}
static mrb_value mrb_script_api_rename_save_slot(mrb_state* mrb, mrb_value) {
    char* from; char* to; mrb_get_args(mrb, "zz", &from, &to);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_bool_value(api && api->RenameSaveSlot(from, to, true));
}
static mrb_value mrb_script_api_list_save_slots(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_ary_new(mrb);
    return JsonToMrb(mrb, api->ListSaveSlots());
}
static mrb_value mrb_script_api_list_save_slot_infos(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_ary_new(mrb);
    return JsonToMrb(mrb, api->ListSaveSlotInfos());
}
static mrb_value mrb_script_api_get_save_slot_info(mrb_state* mrb, mrb_value) {
    char* slot; mrb_get_args(mrb, "z", &slot);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_nil_value();
    nlohmann::json result = api->GetSaveSlotInfo(slot);
    if (result.is_null()) return mrb_nil_value();
    return JsonToMrb(mrb, result);
}
static mrb_value mrb_script_api_quick_save(mrb_state* mrb, mrb_value) {
    mrb_value data; mrb_value meta = mrb_nil_value();
    mrb_get_args(mrb, "o|o", &data, &meta);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_false_value();
    nlohmann::json metaJson = mrb_nil_p(meta) ? nlohmann::json::object() : MrbToJson(mrb, meta);
    return mrb_bool_value(api->QuickSaveGame(MrbToJson(mrb, data), metaJson));
}
static mrb_value mrb_script_api_quick_load(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_nil_value();
    nlohmann::json result = api->QuickLoadGame();
    if (result.is_null()) return mrb_nil_value();
    return JsonToMrb(mrb, result);
}
static mrb_value mrb_script_api_has_quick_save(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_bool_value(api && api->HasQuickSave());
}
static mrb_value mrb_script_api_auto_save(mrb_state* mrb, mrb_value) {
    mrb_value data; mrb_value meta = mrb_nil_value();
    mrb_get_args(mrb, "o|o", &data, &meta);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_false_value();
    nlohmann::json metaJson = mrb_nil_p(meta) ? nlohmann::json::object() : MrbToJson(mrb, meta);
    return mrb_bool_value(api->AutoSaveGame(MrbToJson(mrb, data), metaJson));
}
static mrb_value mrb_script_api_has_auto_save(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_bool_value(api && api->HasAutoSave());
}
static mrb_value mrb_script_api_get_last_save_error(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    std::string s = api ? api->GetLastSaveError() : std::string("Success");
    return mrb_str_new(mrb, s.data(), s.size());
}
static mrb_value mrb_script_api_set_save_directory(mrb_state* mrb, mrb_value) {
    char* dir; mrb_get_args(mrb, "z", &dir);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetSaveDirectory(dir);
    return mrb_nil_value();
}
static mrb_value mrb_script_api_set_save_format(mrb_state* mrb, mrb_value) {
    char* fmt; mrb_get_args(mrb, "z", &fmt);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetSaveFormat(fmt);
    return mrb_nil_value();
}
static mrb_value mrb_script_api_set_save_schema_version(mrb_state* mrb, mrb_value) {
    mrb_int v; mrb_get_args(mrb, "i", &v);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetSaveSchemaVersion(static_cast<int>(v));
    return mrb_nil_value();
}
static mrb_value mrb_script_api_get_save_schema_version(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_fixnum_value(api ? api->GetSaveSchemaVersion() : 0);
}
static mrb_value mrb_script_api_set_save_obfuscation_key(mrb_state* mrb, mrb_value) {
    char* key; mrb_get_args(mrb, "z", &key);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetSaveObfuscationKey(key);
    return mrb_nil_value();
}
static mrb_value mrb_script_api_set_quick_save_slot(mrb_state* mrb, mrb_value) {
    char* slot; mrb_get_args(mrb, "z", &slot);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetQuickSaveSlot(slot);
    return mrb_nil_value();
}
static mrb_value mrb_script_api_set_auto_save_slot(mrb_state* mrb, mrb_value) {
    char* slot; mrb_get_args(mrb, "z", &slot);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->SetAutoSaveSlot(slot);
    return mrb_nil_value();
}
static mrb_value mrb_script_api_capture_scene_state(mrb_state* mrb, mrb_value) {
    char* group = nullptr; mrb_get_args(mrb, "|z", &group);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_ary_new(mrb);
    return JsonToMrb(mrb, api->CaptureSceneState(group ? group : "persistent"));
}
static mrb_value mrb_script_api_restore_scene_state(mrb_state* mrb, mrb_value) {
    mrb_value captured; mrb_get_args(mrb, "o", &captured);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (!api) return mrb_fixnum_value(0);
    return mrb_fixnum_value(api->RestoreSceneState(MrbToJson(mrb, captured)));
}
static mrb_value mrb_script_api_user_dir(mrb_state* mrb, mrb_value) {
    return mrb_str_new_cstr(mrb, "user://");
}
static mrb_value mrb_script_api_res_dir(mrb_state* mrb, mrb_value) {
    return mrb_str_new_cstr(mrb, "res://");
}
static mrb_value mrb_script_api_temp_dir(mrb_state* mrb, mrb_value) {
    return mrb_str_new_cstr(mrb, "temp://");
}
static mrb_value mrb_script_api_join_path(mrb_state* mrb, mrb_value) {
    mrb_value* argv; mrb_int argc;
    mrb_get_args(mrb, "*", &argv, &argc);

    // mrb_obj_as_string can run a Ruby `to_s` and reallocate the value stack `argv`
    // points into, so take a copy before converting anything.
    const std::vector<mrb_value> parts(argv, argv + argc);

    std::string result;
    for (std::vector<mrb_value>::const_iterator it = parts.begin(); it != parts.end(); ++it) {
        mrb_value sv = mrb_obj_as_string(mrb, *it);
        std::string part(RSTRING_PTR(sv), RSTRING_LEN(sv));
        if (part.empty()) continue;
        if (result.empty()) {
            result = part;
        } else if (result.back() == '/') {
            result += (part.front() == '/') ? part.substr(1) : part;
        } else {
            result += (part.front() == '/') ? part : ("/" + part);
        }
    }
    return mrb_str_new(mrb, result.data(), result.size());
}

// Handle classes wrap a C++ pointer in an RData. Their instance type must therefore be
// MRB_TT_DATA (otherwise `LupineNode.new` yields a plain RObject whose ivar table the
// Self* accessors would reinterpret as a NodeRef), and `new` is undefined because only
// the engine may mint a handle.
static void DefineHandleClass(mrb_state* mrb, struct RClass* cls) {
    MRB_SET_INSTANCE_TT(cls, MRB_TT_DATA);
    mrb_undef_class_method(mrb, cls, "new");
}

// --- Coroutine scheduler hooks ---------------------------------------------
//
// The scheduler runs from MRubyHost::Pump, i.e. with no dispatch in progress, so it
// re-establishes the owning instance's context itself around every resume and every
// `:until` predicate. Without this, self-relative APIs (set_position_2d, ...) resolve a
// null ScriptAPI and silently no-op after a coroutine's first await.

static mrb_value mrb_lupine_context_id(mrb_state* mrb, mrb_value) {
    (void)mrb;
    return mrb_fixnum_value(MRubyHost::Instance().ActiveInstanceId());
}

static mrb_value mrb_lupine_push_context(mrb_state* mrb, mrb_value) {
    mrb_int instanceId = -1;
    mrb_get_args(mrb, "i", &instanceId);
    return MRubyHost::Instance().PushCoroutineContext(static_cast<int>(instanceId))
               ? mrb_true_value()
               : mrb_false_value();
}

static mrb_value mrb_lupine_pop_context(mrb_state* mrb, mrb_value) {
    (void)mrb;
    MRubyHost::Instance().PopContext();
    return mrb_nil_value();
}

// Reports through the engine logger rather than Lupine.log_error, which needs a live
// ScriptAPI - the instance owning a failing coroutine may already be gone.
static mrb_value mrb_lupine_scheduler_error(mrb_state* mrb, mrb_value) {
    char* message = nullptr;
    mrb_get_args(mrb, "z", &message);
    LOG_ERROR(LogCategory::Scripting, "mRuby scheduler: {}", message ? message : "");
    return mrb_nil_value();
}

static void RegisterTimerSceneTreeClasses(mrb_state* mrb) {
    struct RClass* timerClass = mrb_define_class(mrb, "LupineTimer", mrb->object_class);
    DefineHandleClass(mrb, timerClass);
    mrb_define_method(mrb, timerClass, "is_valid", mrb_timer_is_valid, MRB_ARGS_NONE());
    mrb_define_method(mrb, timerClass, "get_name", mrb_timer_get_name, MRB_ARGS_NONE());
    mrb_define_method(mrb, timerClass, "start", mrb_timer_start, MRB_ARGS_NONE());
    mrb_define_method(mrb, timerClass, "stop", mrb_timer_stop, MRB_ARGS_NONE());
    mrb_define_method(mrb, timerClass, "reset", mrb_timer_reset, MRB_ARGS_NONE());
    mrb_define_method(mrb, timerClass, "restart", mrb_timer_restart, MRB_ARGS_NONE());
    mrb_define_method(mrb, timerClass, "remove", mrb_timer_remove, MRB_ARGS_NONE());
    mrb_define_method(mrb, timerClass, "is_running", mrb_timer_is_running, MRB_ARGS_NONE());
    mrb_define_method(mrb, timerClass, "is_finished", mrb_timer_is_finished, MRB_ARGS_NONE());
    mrb_define_method(mrb, timerClass, "get_time_left", mrb_timer_get_time_left, MRB_ARGS_NONE());
    mrb_define_method(mrb, timerClass, "get_fire_count", mrb_timer_get_fire_count, MRB_ARGS_NONE());
    mrb_define_method(mrb, timerClass, "get_duration", mrb_timer_get_duration, MRB_ARGS_NONE());
    mrb_define_method(mrb, timerClass, "set_duration", mrb_timer_set_duration, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, timerClass, "get_elapsed", mrb_timer_get_elapsed, MRB_ARGS_NONE());
    mrb_define_method(mrb, timerClass, "set_elapsed", mrb_timer_set_elapsed, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, timerClass, "get_loop", mrb_timer_get_loop, MRB_ARGS_NONE());
    mrb_define_method(mrb, timerClass, "set_loop", mrb_timer_set_loop, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, timerClass, "get_repeat_count", mrb_timer_get_repeat_count, MRB_ARGS_NONE());
    mrb_define_method(mrb, timerClass, "set_repeat_count", mrb_timer_set_repeat_count, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, timerClass, "get_owner", mrb_timer_get_owner, MRB_ARGS_NONE());
    mrb_define_method(mrb, timerClass, "as_component", mrb_timer_as_component, MRB_ARGS_NONE());

    struct RClass* sceneClass = mrb_define_class(mrb, "LupineScene", mrb->object_class);
    DefineHandleClass(mrb, sceneClass);
    mrb_define_method(mrb, sceneClass, "is_valid", mrb_scene_is_valid, MRB_ARGS_NONE());
    mrb_define_method(mrb, sceneClass, "get_name", mrb_scene_get_name, MRB_ARGS_NONE());
    mrb_define_method(mrb, sceneClass, "get_path", mrb_scene_get_path, MRB_ARGS_NONE());
    mrb_define_method(mrb, sceneClass, "get_root", mrb_scene_get_root, MRB_ARGS_NONE());
    mrb_define_method(mrb, sceneClass, "find_node", mrb_scene_find_node, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, sceneClass, "find_node_by_uuid", mrb_scene_find_node_by_uuid, MRB_ARGS_REQ(1));

    struct RClass* treeClass = mrb_define_class(mrb, "LupineTree", mrb->object_class);
    DefineHandleClass(mrb, treeClass);
    mrb_define_method(mrb, treeClass, "is_valid", mrb_tree_is_valid, MRB_ARGS_NONE());
    mrb_define_method(mrb, treeClass, "get_root", mrb_tree_get_root, MRB_ARGS_NONE());
    mrb_define_method(mrb, treeClass, "get_current_scene", mrb_tree_get_current_scene, MRB_ARGS_NONE());
    mrb_define_method(mrb, treeClass, "get_current_scene_path", mrb_tree_get_current_scene_path, MRB_ARGS_NONE());
    mrb_define_method(mrb, treeClass, "change_scene", mrb_tree_change_scene, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, treeClass, "reload_scene", mrb_tree_reload_scene, MRB_ARGS_NONE());
    mrb_define_method(mrb, treeClass, "add_scene", mrb_tree_add_scene, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, treeClass, "remove_scene", mrb_tree_remove_scene, MRB_ARGS_REQ(1));

    struct RClass* tweenClass = mrb_define_class(mrb, "LupineTween", mrb->object_class);
    DefineHandleClass(mrb, tweenClass);
    mrb_define_method(mrb, tweenClass, "is_valid", mrb_tween_is_valid, MRB_ARGS_NONE());
    mrb_define_method(mrb, tweenClass, "get_name", mrb_tween_get_name, MRB_ARGS_NONE());
    mrb_define_method(mrb, tweenClass, "play", mrb_tween_play, MRB_ARGS_NONE());
    mrb_define_method(mrb, tweenClass, "pause", mrb_tween_pause, MRB_ARGS_NONE());
    mrb_define_method(mrb, tweenClass, "stop", mrb_tween_stop, MRB_ARGS_NONE());
    mrb_define_method(mrb, tweenClass, "restart", mrb_tween_restart, MRB_ARGS_NONE());
    mrb_define_method(mrb, tweenClass, "kill", mrb_tween_kill, MRB_ARGS_NONE());
    mrb_define_method(mrb, tweenClass, "is_running", mrb_tween_is_running, MRB_ARGS_NONE());
    mrb_define_method(mrb, tweenClass, "is_finished", mrb_tween_is_finished, MRB_ARGS_NONE());
    mrb_define_method(mrb, tweenClass, "get_progress", mrb_tween_get_progress, MRB_ARGS_NONE());
    mrb_define_method(mrb, tweenClass, "get_duration", mrb_tween_get_duration, MRB_ARGS_NONE());
    mrb_define_method(mrb, tweenClass, "set_duration", mrb_tween_set_duration, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, tweenClass, "get_easing", mrb_tween_get_easing, MRB_ARGS_NONE());
    mrb_define_method(mrb, tweenClass, "set_easing", mrb_tween_set_easing, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, tweenClass, "get_loop", mrb_tween_get_loop, MRB_ARGS_NONE());
    mrb_define_method(mrb, tweenClass, "set_loop", mrb_tween_set_loop, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, tweenClass, "set_auto_remove", mrb_tween_set_auto_remove, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, tweenClass, "get_owner", mrb_tween_get_owner, MRB_ARGS_NONE());
    mrb_define_method(mrb, tweenClass, "as_component", mrb_tween_as_component, MRB_ARGS_NONE());

    struct RClass* awaiterClass = mrb_define_class(mrb, "LupineSignalAwaiter", mrb->object_class);
    DefineHandleClass(mrb, awaiterClass);
    mrb_define_method(mrb, awaiterClass, "is_valid", mrb_awaiter_is_valid, MRB_ARGS_NONE());
    mrb_define_method(mrb, awaiterClass, "is_fired", mrb_awaiter_is_fired, MRB_ARGS_NONE());
    mrb_define_method(mrb, awaiterClass, "reset", mrb_awaiter_reset, MRB_ARGS_NONE());
    mrb_define_method(mrb, awaiterClass, "cancel", mrb_awaiter_cancel, MRB_ARGS_NONE());

    struct RClass* seqClass = mrb_define_class(mrb, "LupineSequence", mrb->object_class);
    DefineHandleClass(mrb, seqClass);
    mrb_define_method(mrb, seqClass, "is_valid", mrb_seq_is_valid, MRB_ARGS_NONE());
    mrb_define_method(mrb, seqClass, "get_name", mrb_seq_get_name, MRB_ARGS_NONE());
    mrb_define_method(mrb, seqClass, "append", mrb_seq_append, MRB_ARGS_ARG(3, 2));
    mrb_define_method(mrb, seqClass, "append_on", mrb_seq_append_on, MRB_ARGS_ARG(4, 2));
    mrb_define_method(mrb, seqClass, "append_interval", mrb_seq_append_interval, MRB_ARGS_ARG(1, 1));
    mrb_define_method(mrb, seqClass, "append_callback", mrb_seq_append_callback, MRB_ARGS_ARG(1, 1));
    mrb_define_method(mrb, seqClass, "append_callback_on", mrb_seq_append_callback_on, MRB_ARGS_ARG(2, 1));
    mrb_define_method(mrb, seqClass, "play", mrb_seq_play, MRB_ARGS_NONE());
    mrb_define_method(mrb, seqClass, "stop", mrb_seq_stop, MRB_ARGS_NONE());
    mrb_define_method(mrb, seqClass, "reset", mrb_seq_reset, MRB_ARGS_NONE());
    mrb_define_method(mrb, seqClass, "restart", mrb_seq_restart, MRB_ARGS_NONE());
    mrb_define_method(mrb, seqClass, "kill", mrb_seq_kill, MRB_ARGS_NONE());
    mrb_define_method(mrb, seqClass, "is_running", mrb_seq_is_running, MRB_ARGS_NONE());
    mrb_define_method(mrb, seqClass, "is_finished", mrb_seq_is_finished, MRB_ARGS_NONE());
    mrb_define_method(mrb, seqClass, "set_loops", mrb_seq_set_loops, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, seqClass, "get_loops", mrb_seq_get_loops, MRB_ARGS_NONE());
    mrb_define_method(mrb, seqClass, "set_auto_remove", mrb_seq_set_auto_remove, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, seqClass, "get_step_count", mrb_seq_get_step_count, MRB_ARGS_NONE());
    mrb_define_method(mrb, seqClass, "get_owner", mrb_seq_get_owner, MRB_ARGS_NONE());
    mrb_define_method(mrb, seqClass, "as_component", mrb_seq_as_component, MRB_ARGS_NONE());
}

static void RegisterNodeObjectClasses(mrb_state* mrb) {
    struct RClass* nodeClass = mrb_define_class(mrb, "LupineNode", mrb->object_class);
    DefineHandleClass(mrb, nodeClass);

    mrb_define_method(mrb, nodeClass, "is_valid", mrb_node_is_valid, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "get_name", mrb_node_get_name, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "set_name", mrb_node_set_name, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, nodeClass, "get_uuid", mrb_node_get_uuid, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "get_path", mrb_node_get_path, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "get_type_name", mrb_node_get_type_name, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "is_active", mrb_node_is_active, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "set_active", mrb_node_set_active, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, nodeClass, "is_visible", mrb_node_is_visible, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "set_visible", mrb_node_set_visible, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, nodeClass, "is_unique_name_in_owner", mrb_node_is_unique, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "set_unique_name_in_owner", mrb_node_set_unique, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, nodeClass, "get_child_count", mrb_node_get_child_count, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "has_node", mrb_node_has_node, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, nodeClass, "has_component", mrb_node_has_component, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, nodeClass, "has_property", mrb_node_has_property, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, nodeClass, "queue_free", mrb_node_queue_free, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "queue_free_deferred", mrb_node_queue_free_deferred, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "free", mrb_node_free, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "get_parent", mrb_node_get_parent, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "get_child", mrb_node_get_child, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, nodeClass, "get_child_at", mrb_node_get_child_at, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, nodeClass, "get_children", mrb_node_get_children, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "find_node", mrb_node_find_node, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, nodeClass, "get_node", mrb_node_find_node, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, nodeClass, "add_child", mrb_node_add_child, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, nodeClass, "remove_child", mrb_node_remove_child, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, nodeClass, "reparent_to", mrb_node_reparent_to, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, nodeClass, "get_sibling_index", mrb_node_get_sibling_index, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "set_sibling_index", mrb_node_set_sibling_index, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, nodeClass, "get_child_index", mrb_node_get_child_index, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, nodeClass, "move_child", mrb_node_move_child, MRB_ARGS_REQ(2));
    mrb_define_method(mrb, nodeClass, "create_tween", mrb_node_create_tween, MRB_ARGS_ARG(3, 1));
    mrb_define_method(mrb, nodeClass, "create_sequence", mrb_node_create_sequence, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "await_signal", mrb_node_await_signal, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, nodeClass, "duplicate", mrb_node_duplicate, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "distance_to", mrb_node_distance_to, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, nodeClass, "get_position_2d", mrb_node_get_position_2d, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "set_position_2d", mrb_node_set_position_2d, MRB_ARGS_REQ(2));
    mrb_define_method(mrb, nodeClass, "translate_2d", mrb_node_translate_2d, MRB_ARGS_REQ(2));
    mrb_define_method(mrb, nodeClass, "get_rotation_2d", mrb_node_get_rotation_2d, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "set_rotation_2d", mrb_node_set_rotation_2d, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, nodeClass, "get_scale_2d", mrb_node_get_scale_2d, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "set_scale_2d", mrb_node_set_scale_2d, MRB_ARGS_REQ(2));
    mrb_define_method(mrb, nodeClass, "get_position_3d", mrb_node_get_position_3d, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "set_position_3d", mrb_node_set_position_3d, MRB_ARGS_REQ(3));
    mrb_define_method(mrb, nodeClass, "translate_3d", mrb_node_translate_3d, MRB_ARGS_REQ(3));
    mrb_define_method(mrb, nodeClass, "get_rotation_3d", mrb_node_get_rotation_3d, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "set_rotation_3d", mrb_node_set_rotation_3d, MRB_ARGS_REQ(3));
    mrb_define_method(mrb, nodeClass, "get_scale_3d", mrb_node_get_scale_3d, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "set_scale_3d", mrb_node_set_scale_3d, MRB_ARGS_REQ(3));
    mrb_define_method(mrb, nodeClass, "get_global_position_2d", mrb_node_get_global_position_2d, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "set_global_position_2d", mrb_node_set_global_position_2d, MRB_ARGS_REQ(2));
    mrb_define_method(mrb, nodeClass, "get_global_rotation_2d", mrb_node_get_global_rotation_2d, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "set_global_rotation_2d", mrb_node_set_global_rotation_2d, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, nodeClass, "get_global_scale_2d", mrb_node_get_global_scale_2d, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "get_global_position_3d", mrb_node_get_global_position_3d, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "set_global_position_3d", mrb_node_set_global_position_3d, MRB_ARGS_REQ(3));
    mrb_define_method(mrb, nodeClass, "get_global_rotation_3d", mrb_node_get_global_rotation_3d, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "set_global_rotation_3d", mrb_node_set_global_rotation_3d, MRB_ARGS_REQ(3));
    mrb_define_method(mrb, nodeClass, "get_global_scale_3d", mrb_node_get_global_scale_3d, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "get_component", mrb_node_get_component, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, nodeClass, "get_components", mrb_node_get_components, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, nodeClass, "add_component", mrb_node_add_component, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, nodeClass, "remove_component", mrb_node_remove_component, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, nodeClass, "get", mrb_node_get, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, nodeClass, "set", mrb_node_set, MRB_ARGS_REQ(2));
    mrb_define_method(mrb, nodeClass, "has_method", mrb_node_has_method, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, nodeClass, "call", mrb_node_call, MRB_ARGS_REQ(1) | MRB_ARGS_REST());
    mrb_define_method(mrb, nodeClass, "emit", mrb_node_emit, MRB_ARGS_REQ(1) | MRB_ARGS_REST());
    mrb_define_method(mrb, nodeClass, "connect", mrb_node_connect, MRB_ARGS_ARG(3, 1));
    mrb_define_method(mrb, nodeClass, "disconnect", mrb_node_disconnect, MRB_ARGS_REQ(2));
    mrb_define_method(mrb, nodeClass, "is_connected", mrb_node_is_connected, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, nodeClass, "add_user_signal", mrb_node_add_user_signal, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, nodeClass, "add_to_group", mrb_node_add_to_group, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, nodeClass, "remove_from_group", mrb_node_remove_from_group, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, nodeClass, "is_in_group", mrb_node_is_in_group, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, nodeClass, "get_groups", mrb_node_get_groups, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "implements_interface", mrb_node_implements_interface, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, nodeClass, "get_interfaces", mrb_node_get_interfaces, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "verify_interface", mrb_node_verify_interface, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, nodeClass, "get_signal_list", mrb_node_get_signal_list, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "rpc", mrb_node_rpc, MRB_ARGS_REQ(1) | MRB_ARGS_REST());
    mrb_define_method(mrb, nodeClass, "rpc_id", mrb_node_rpc_id, MRB_ARGS_REQ(2) | MRB_ARGS_REST());
    mrb_define_method(mrb, nodeClass, "rpc_unreliable", mrb_node_rpc_unreliable, MRB_ARGS_REQ(1) | MRB_ARGS_REST());
    mrb_define_method(mrb, nodeClass, "set_multiplayer_authority", mrb_node_set_authority, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, nodeClass, "get_multiplayer_authority", mrb_node_get_authority, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "is_multiplayer_authority", mrb_node_is_authority, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "get_network_id", mrb_node_get_network_id, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "camera_shake", mrb_node_camera_shake, MRB_ARGS_ARG(2, 1));
    mrb_define_method(mrb, nodeClass, "camera_stop_shake", mrb_node_camera_stop_shake, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "camera_is_shaking", mrb_node_camera_is_shaking, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "camera_set_follow_target", mrb_node_camera_set_follow_target, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, nodeClass, "camera_clear_follow_target", mrb_node_camera_clear_follow_target, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "camera_smooth_move_to", mrb_node_camera_smooth_move_to, MRB_ARGS_ARG(2, 1));
    mrb_define_method(mrb, nodeClass, "camera_get_effective_position", mrb_node_camera_get_effective_position, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "particles_restart", mrb_node_particles_restart, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "particles_emit_burst", mrb_node_particles_emit_burst, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, nodeClass, "particles_set_emitting", mrb_node_particles_set_emitting, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, nodeClass, "particles_is_emitting", mrb_node_particles_is_emitting, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "particles_get_alive_count", mrb_node_particles_get_alive_count, MRB_ARGS_NONE());
    mrb_define_method(mrb, nodeClass, "set_theme", mrb_node_set_theme, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, nodeClass, "set_theme_type_variation", mrb_node_set_theme_type_variation, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, nodeClass, "clear_theme_override", mrb_node_clear_theme_override, MRB_ARGS_REQ(1));

    struct RClass* compClass = mrb_define_class(mrb, "LupineComponent", mrb->object_class);
    DefineHandleClass(mrb, compClass);

    mrb_define_method(mrb, compClass, "is_valid", mrb_comp_is_valid, MRB_ARGS_NONE());
    mrb_define_method(mrb, compClass, "get_type_name", mrb_comp_get_type_name, MRB_ARGS_NONE());
    mrb_define_method(mrb, compClass, "get_name", mrb_comp_get_name, MRB_ARGS_NONE());
    mrb_define_method(mrb, compClass, "is_enabled", mrb_comp_is_enabled, MRB_ARGS_NONE());
    mrb_define_method(mrb, compClass, "set_enabled", mrb_comp_set_enabled, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, compClass, "has_property", mrb_comp_has_property, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, compClass, "get", mrb_comp_get, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, compClass, "set", mrb_comp_set, MRB_ARGS_REQ(2));
    mrb_define_method(mrb, compClass, "call", mrb_comp_call, MRB_ARGS_REQ(1) | MRB_ARGS_REST());
    mrb_define_method(mrb, compClass, "get_owner", mrb_comp_get_owner, MRB_ARGS_NONE());
    mrb_define_method(mrb, compClass, "is_instance_of", mrb_comp_is_instance_of, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, compClass, "get_type_chain", mrb_comp_get_type_chain, MRB_ARGS_NONE());
    mrb_define_method(mrb, compClass, "await_signal", mrb_comp_await_signal, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, compClass, "emit", mrb_comp_emit, MRB_ARGS_REQ(1) | MRB_ARGS_REST());
    mrb_define_method(mrb, compClass, "connect", mrb_comp_connect, MRB_ARGS_ARG(3, 1));
    mrb_define_method(mrb, compClass, "disconnect", mrb_comp_disconnect, MRB_ARGS_REQ(2));
    mrb_define_method(mrb, compClass, "is_connected", mrb_comp_is_connected, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, compClass, "add_user_signal", mrb_comp_add_user_signal, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, compClass, "get_signal_list", mrb_comp_get_signal_list, MRB_ARGS_NONE());
}

static mrb_value mrb_script_api_debug_draw_line(mrb_state* mrb, mrb_value) {
    mrb_float x1, y1, z1, x2, y2, z2, r, g, b, a, duration = 0.0;
    mrb_get_args(mrb, "ffffffffff|f", &x1, &y1, &z1, &x2, &y2, &z2, &r, &g, &b, &a, &duration);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->DebugDrawLine(mrbVec3(x1, y1, z1), mrbVec3(x2, y2, z2),
                                mrbColor(r, g, b, a), static_cast<float>(duration));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_debug_draw_line_2d(mrb_state* mrb, mrb_value) {
    mrb_float x1, y1, x2, y2, r, g, b, a, duration = 0.0;
    mrb_get_args(mrb, "ffffffff|f", &x1, &y1, &x2, &y2, &r, &g, &b, &a, &duration);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->DebugDrawLine2D(mrbVec2(x1, y1), mrbVec2(x2, y2),
                                  mrbColor(r, g, b, a), static_cast<float>(duration));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_debug_draw_ray(mrb_state* mrb, mrb_value) {
    mrb_float ox, oy, oz, dx, dy, dz, r, g, b, a, duration = 0.0;
    mrb_get_args(mrb, "ffffffffff|f", &ox, &oy, &oz, &dx, &dy, &dz, &r, &g, &b, &a, &duration);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->DebugDrawRay(mrbVec3(ox, oy, oz), mrbVec3(dx, dy, dz),
                               mrbColor(r, g, b, a), static_cast<float>(duration));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_debug_draw_box(mrb_state* mrb, mrb_value) {
    mrb_float cx, cy, cz, sx, sy, sz, r, g, b, a, duration = 0.0;
    mrb_get_args(mrb, "ffffffffff|f", &cx, &cy, &cz, &sx, &sy, &sz, &r, &g, &b, &a, &duration);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->DebugDrawBox(mrbVec3(cx, cy, cz), mrbVec3(sx, sy, sz),
                               mrbColor(r, g, b, a), static_cast<float>(duration));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_debug_draw_sphere(mrb_state* mrb, mrb_value) {
    mrb_float cx, cy, cz, radius, r, g, b, a, duration = 0.0;
    mrb_get_args(mrb, "ffffffff|f", &cx, &cy, &cz, &radius, &r, &g, &b, &a, &duration);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->DebugDrawSphere(mrbVec3(cx, cy, cz), static_cast<float>(radius),
                                  mrbColor(r, g, b, a), static_cast<float>(duration));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_debug_draw_circle(mrb_state* mrb, mrb_value) {
    mrb_float cx, cy, cz, nx, ny, nz, radius, r, g, b, a, duration = 0.0;
    mrb_get_args(mrb, "fffffffffff|f", &cx, &cy, &cz, &nx, &ny, &nz, &radius, &r, &g, &b, &a, &duration);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->DebugDrawCircle(mrbVec3(cx, cy, cz), mrbVec3(nx, ny, nz), static_cast<float>(radius),
                                  mrbColor(r, g, b, a), static_cast<float>(duration));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_debug_draw_text(mrb_state* mrb, mrb_value) {
    mrb_float x, y, z, r, g, b, a, duration = 0.0;
    char* text;
    mrb_get_args(mrb, "fffzffff|f", &x, &y, &z, &text, &r, &g, &b, &a, &duration);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->DebugDrawText(mrbVec3(x, y, z), text,
                                mrbColor(r, g, b, a), static_cast<float>(duration));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_debug_draw_text_2d(mrb_state* mrb, mrb_value) {
    mrb_float x, y, r, g, b, a, duration = 0.0;
    char* text;
    mrb_get_args(mrb, "ffzffff|f", &x, &y, &text, &r, &g, &b, &a, &duration);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->DebugDrawText2D(mrbVec2(x, y), text,
                                  mrbColor(r, g, b, a), static_cast<float>(duration));
    return mrb_nil_value();
}

// --- Custom component rendering (on_draw) ----------------------------------

static mrb_value mrb_script_api_draw_quad(mrb_state* mrb, mrb_value) {
    mrb_float x, y, z, w, h, r, g, b, a;
    mrb_int blend = 0;
    mrb_get_args(mrb, "fffffffff|i", &x, &y, &z, &w, &h, &r, &g, &b, &a, &blend);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->DrawQuad(mrbVec3(x, y, z), mrbVec2(w, h),
                           mrbColor(r, g, b, a), static_cast<int>(blend));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_draw_textured_quad(mrb_state* mrb, mrb_value) {
    mrb_float x, y, z, w, h, r, g, b, a;
    char* path;
    mrb_int blend = 0;
    mrb_get_args(mrb, "fffffffffz|i", &x, &y, &z, &w, &h, &r, &g, &b, &a, &path, &blend);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->DrawTexturedQuad(mrbVec3(x, y, z), mrbVec2(w, h),
                                   mrbColor(r, g, b, a), path, static_cast<int>(blend));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_draw_rect(mrb_state* mrb, mrb_value) {
    mrb_float x, y, w, h, r, g, b, a, thickness = 1.0;
    mrb_bool filled = true;
    mrb_get_args(mrb, "ffffffff|bf", &x, &y, &w, &h, &r, &g, &b, &a, &filled, &thickness);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->DrawRect(mrbVec2(x, y), mrbVec2(w, h), mrbColor(r, g, b, a),
                           filled, static_cast<float>(thickness));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_draw_sprite(mrb_state* mrb, mrb_value) {
    char* path;
    mrb_float x, y, w, h, r, g, b, a, rotation = 0.0;
    mrb_int blend = 0;
    mrb_get_args(mrb, "zffffffff|fi", &path, &x, &y, &w, &h, &r, &g, &b, &a, &rotation, &blend);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->DrawSprite(path, mrbVec2(x, y), mrbVec2(w, h), mrbColor(r, g, b, a),
                             static_cast<float>(rotation), static_cast<int>(blend));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_draw_line(mrb_state* mrb, mrb_value) {
    mrb_float x1, y1, z1, x2, y2, z2, r, g, b, a, thickness = 1.0;
    mrb_get_args(mrb, "ffffffffff|f", &x1, &y1, &z1, &x2, &y2, &z2, &r, &g, &b, &a, &thickness);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->DrawLine(mrbVec3(x1, y1, z1), mrbVec3(x2, y2, z2),
                           mrbColor(r, g, b, a), static_cast<float>(thickness));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_draw_circle(mrb_state* mrb, mrb_value) {
    mrb_float x, y, z, radius, r, g, b, a;
    mrb_bool filled = true;
    mrb_get_args(mrb, "fffffff|b", &x, &y, &z, &radius, &r, &g, &b, &a, &filled);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->DrawCircle(mrbVec3(x, y, z), static_cast<float>(radius),
                             mrbColor(r, g, b, a), filled);
    return mrb_nil_value();
}

static mrb_value mrb_script_api_draw_polygon(mrb_state* mrb, mrb_value) {
    mrb_float x, y, radius, r, g, b, a, rotation = 0.0;
    mrb_int sides, blend = 0;
    mrb_get_args(mrb, "fffiffff|fi", &x, &y, &radius, &sides, &r, &g, &b, &a, &rotation, &blend);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->DrawPolygon(mrbVec2(x, y), static_cast<float>(radius), static_cast<int>(sides),
                              mrbColor(r, g, b, a), static_cast<float>(rotation),
                              static_cast<int>(blend));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_draw_box(mrb_state* mrb, mrb_value) {
    mrb_float x, y, z, w, h, d, r, g, b, a;
    mrb_bool wireframe = false;
    mrb_get_args(mrb, "ffffffffff|b", &x, &y, &z, &w, &h, &d, &r, &g, &b, &a, &wireframe);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->DrawBox(mrbVec3(x, y, z), mrbVec3(w, h, d),
                          mrbColor(r, g, b, a), wireframe);
    return mrb_nil_value();
}

static mrb_value mrb_script_api_draw_rounded_rect(mrb_state* mrb, mrb_value) {
    mrb_float x, y, w, h, cornerRadius, r, g, b, a;
    mrb_int blend = 0;
    mrb_get_args(mrb, "fffffffff|i", &x, &y, &w, &h, &cornerRadius, &r, &g, &b, &a, &blend);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->DrawRoundedRect(mrbVec2(x, y), mrbVec2(w, h),
                                  static_cast<float>(cornerRadius), mrbColor(r, g, b, a),
                                  static_cast<int>(blend));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_is_drawing(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_bool_value(api ? api->IsDrawing() : false);
}

// --- Editor-only debug drawing (no runtime visual) -------------------------

static mrb_value mrb_script_api_editor_draw_line(mrb_state* mrb, mrb_value) {
    mrb_float x1, y1, z1, x2, y2, z2, r, g, b, a;
    mrb_get_args(mrb, "ffffffffff", &x1, &y1, &z1, &x2, &y2, &z2, &r, &g, &b, &a);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->EditorDrawLine(mrbVec3(x1, y1, z1), mrbVec3(x2, y2, z2),
                                 mrbColor(r, g, b, a));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_editor_draw_box(mrb_state* mrb, mrb_value) {
    mrb_float cx, cy, cz, w, h, d, r, g, b, a;
    mrb_bool wireframe = true;
    mrb_get_args(mrb, "ffffffffff|b", &cx, &cy, &cz, &w, &h, &d, &r, &g, &b, &a, &wireframe);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->EditorDrawBox(mrbVec3(cx, cy, cz), mrbVec3(w, h, d),
                                mrbColor(r, g, b, a), wireframe);
    return mrb_nil_value();
}

static mrb_value mrb_script_api_editor_draw_sphere(mrb_state* mrb, mrb_value) {
    mrb_float cx, cy, cz, radius, r, g, b, a;
    mrb_bool wireframe = true;
    mrb_get_args(mrb, "ffffffff|b", &cx, &cy, &cz, &radius, &r, &g, &b, &a, &wireframe);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->EditorDrawSphere(mrbVec3(cx, cy, cz), static_cast<float>(radius),
                                   mrbColor(r, g, b, a), wireframe);
    return mrb_nil_value();
}

static mrb_value mrb_script_api_editor_draw_circle(mrb_state* mrb, mrb_value) {
    mrb_float cx, cy, cz, nx, ny, nz, radius, r, g, b, a;
    mrb_get_args(mrb, "fffffffffff", &cx, &cy, &cz, &nx, &ny, &nz, &radius, &r, &g, &b, &a);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->EditorDrawCircle(mrbVec3(cx, cy, cz), mrbVec3(nx, ny, nz),
                                   static_cast<float>(radius), mrbColor(r, g, b, a));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_editor_draw_rect_2d(mrb_state* mrb, mrb_value) {
    mrb_float cx, cy, w, h, r, g, b, a;
    mrb_get_args(mrb, "ffffffff", &cx, &cy, &w, &h, &r, &g, &b, &a);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->EditorDrawRect2D(mrbVec2(cx, cy), mrbVec2(w, h), mrbColor(r, g, b, a));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_editor_draw_text(mrb_state* mrb, mrb_value) {
    mrb_float x, y, z, r, g, b, a;
    char* text;
    mrb_get_args(mrb, "fffzffff", &x, &y, &z, &text, &r, &g, &b, &a);
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    if (api) api->EditorDrawText(mrbVec3(x, y, z), text, mrbColor(r, g, b, a));
    return mrb_nil_value();
}

static mrb_value mrb_script_api_is_editor_draw_available(mrb_state* mrb, mrb_value) {
    ScriptAPI* api = GetScriptAPIFromState(mrb);
    return mrb_bool_value(api ? api->IsEditorDrawAvailable() : false);
}

static float MrbValueToFloat(mrb_state*, mrb_value v) {
    if (mrb_float_p(v)) return static_cast<float>(mrb_float(v));
    if (mrb_fixnum_p(v)) return static_cast<float>(mrb_fixnum(v));
    return 0.0f;
}

static bool MrbValueIsNumber(mrb_value v) {
    return mrb_float_p(v) || mrb_fixnum_p(v);
}

static float MrbReadIvarFloat(mrb_state* mrb, mrb_value obj, const char* name) {
    return MrbValueToFloat(mrb, mrb_iv_get(mrb, obj, mrb_intern_cstr(mrb, name)));
}

static void MrbSetIvarFloat(mrb_state* mrb, mrb_value obj, const char* name, float value) {
    mrb_iv_set(mrb, obj, mrb_intern_cstr(mrb, name), mrb_float_value(mrb, value));
}

static math::Vec2 MrbReadVec2(mrb_state* mrb, mrb_value obj) {
    return math::Vec2(MrbReadIvarFloat(mrb, obj, "@x"), MrbReadIvarFloat(mrb, obj, "@y"));
}

static math::Vec3 MrbReadVec3(mrb_state* mrb, mrb_value obj) {
    return math::Vec3(MrbReadIvarFloat(mrb, obj, "@x"),
                      MrbReadIvarFloat(mrb, obj, "@y"),
                      MrbReadIvarFloat(mrb, obj, "@z"));
}

static math::Color MrbReadColor(mrb_state* mrb, mrb_value obj) {
    return math::Color(MrbReadIvarFloat(mrb, obj, "@r"),
                       MrbReadIvarFloat(mrb, obj, "@g"),
                       MrbReadIvarFloat(mrb, obj, "@b"),
                       MrbReadIvarFloat(mrb, obj, "@a"));
}

static mrb_value MrbMakeVec2(mrb_state* mrb, const math::Vec2& v) {
    struct RClass* cls = mrb_class_get(mrb, "Vector2");
    mrb_value obj = mrb_obj_new(mrb, cls, 0, NULL);
    MrbSetIvarFloat(mrb, obj, "@x", v.x);
    MrbSetIvarFloat(mrb, obj, "@y", v.y);
    return obj;
}

static mrb_value MrbMakeVec3(mrb_state* mrb, const math::Vec3& v) {
    struct RClass* cls = mrb_class_get(mrb, "Vector3");
    mrb_value obj = mrb_obj_new(mrb, cls, 0, NULL);
    MrbSetIvarFloat(mrb, obj, "@x", v.x);
    MrbSetIvarFloat(mrb, obj, "@y", v.y);
    MrbSetIvarFloat(mrb, obj, "@z", v.z);
    return obj;
}

static mrb_value MrbMakeColor(mrb_state* mrb, const math::Color& c) {
    struct RClass* cls = mrb_class_get(mrb, "Color");
    mrb_value obj = mrb_obj_new(mrb, cls, 0, NULL);
    MrbSetIvarFloat(mrb, obj, "@r", c.r);
    MrbSetIvarFloat(mrb, obj, "@g", c.g);
    MrbSetIvarFloat(mrb, obj, "@b", c.b);
    MrbSetIvarFloat(mrb, obj, "@a", c.a);
    return obj;
}

static mrb_value mrb_vec2_initialize(mrb_state* mrb, mrb_value self) {
    mrb_value* argv = nullptr;
    mrb_int argc = 0;
    mrb_get_args(mrb, "*", &argv, &argc);
    float x = 0.0f, y = 0.0f;
    if (argc == 1) {
        x = y = MrbValueToFloat(mrb, argv[0]);
    } else if (argc >= 2) {
        x = MrbValueToFloat(mrb, argv[0]);
        y = MrbValueToFloat(mrb, argv[1]);
    }
    MrbSetIvarFloat(mrb, self, "@x", x);
    MrbSetIvarFloat(mrb, self, "@y", y);
    return self;
}

static mrb_value mrb_vec2_get_x(mrb_state* mrb, mrb_value self) {
    return mrb_iv_get(mrb, self, mrb_intern_cstr(mrb, "@x"));
}

static mrb_value mrb_vec2_get_y(mrb_state* mrb, mrb_value self) {
    return mrb_iv_get(mrb, self, mrb_intern_cstr(mrb, "@y"));
}

static mrb_value mrb_vec2_set_x(mrb_state* mrb, mrb_value self) {
    mrb_float v = 0.0;
    mrb_get_args(mrb, "f", &v);
    MrbSetIvarFloat(mrb, self, "@x", static_cast<float>(v));
    return mrb_float_value(mrb, static_cast<float>(v));
}

static mrb_value mrb_vec2_set_y(mrb_state* mrb, mrb_value self) {
    mrb_float v = 0.0;
    mrb_get_args(mrb, "f", &v);
    MrbSetIvarFloat(mrb, self, "@y", static_cast<float>(v));
    return mrb_float_value(mrb, static_cast<float>(v));
}

static mrb_value mrb_vec2_add(mrb_state* mrb, mrb_value self) {
    mrb_value other;
    mrb_get_args(mrb, "o", &other);
    return MrbMakeVec2(mrb, MrbReadVec2(mrb, self) + MrbReadVec2(mrb, other));
}

static mrb_value mrb_vec2_sub(mrb_state* mrb, mrb_value self) {
    mrb_value other;
    mrb_get_args(mrb, "o", &other);
    return MrbMakeVec2(mrb, MrbReadVec2(mrb, self) - MrbReadVec2(mrb, other));
}

static mrb_value mrb_vec2_mul(mrb_state* mrb, mrb_value self) {
    mrb_value other;
    mrb_get_args(mrb, "o", &other);
    math::Vec2 a = MrbReadVec2(mrb, self);
    if (MrbValueIsNumber(other)) {
        return MrbMakeVec2(mrb, a * MrbValueToFloat(mrb, other));
    }
    return MrbMakeVec2(mrb, a * MrbReadVec2(mrb, other));
}

static mrb_value mrb_vec2_div(mrb_state* mrb, mrb_value self) {
    mrb_value other;
    mrb_get_args(mrb, "o", &other);
    math::Vec2 a = MrbReadVec2(mrb, self);
    if (MrbValueIsNumber(other)) {
        return MrbMakeVec2(mrb, a / MrbValueToFloat(mrb, other));
    }
    return MrbMakeVec2(mrb, a / MrbReadVec2(mrb, other));
}

static mrb_value mrb_vec2_neg(mrb_state* mrb, mrb_value self) {
    return MrbMakeVec2(mrb, -MrbReadVec2(mrb, self));
}

static mrb_value mrb_vec2_eq(mrb_state* mrb, mrb_value self) {
    mrb_value other;
    mrb_get_args(mrb, "o", &other);
    struct RClass* cls = mrb_class_get(mrb, "Vector2");
    if (!mrb_obj_is_kind_of(mrb, other, cls)) return mrb_false_value();
    return mrb_bool_value(MrbReadVec2(mrb, self) == MrbReadVec2(mrb, other));
}

static mrb_value mrb_vec2_length(mrb_state* mrb, mrb_value self) {
    return mrb_float_value(mrb, MrbReadVec2(mrb, self).Length());
}

static mrb_value mrb_vec2_length_squared(mrb_state* mrb, mrb_value self) {
    return mrb_float_value(mrb, MrbReadVec2(mrb, self).LengthSquared());
}

static mrb_value mrb_vec2_normalized(mrb_state* mrb, mrb_value self) {
    return MrbMakeVec2(mrb, MrbReadVec2(mrb, self).Normalized());
}

static mrb_value mrb_vec2_dot(mrb_state* mrb, mrb_value self) {
    mrb_value other;
    mrb_get_args(mrb, "o", &other);
    return mrb_float_value(mrb, MrbReadVec2(mrb, self).Dot(MrbReadVec2(mrb, other)));
}

static mrb_value mrb_vec2_distance_to(mrb_state* mrb, mrb_value self) {
    mrb_value other;
    mrb_get_args(mrb, "o", &other);
    return mrb_float_value(mrb, MrbReadVec2(mrb, self).Distance(MrbReadVec2(mrb, other)));
}

static mrb_value mrb_vec2_lerp(mrb_state* mrb, mrb_value self) {
    mrb_value other;
    mrb_float t = 0.0;
    mrb_get_args(mrb, "of", &other, &t);
    return MrbMakeVec2(mrb, MrbReadVec2(mrb, self).Lerp(MrbReadVec2(mrb, other), static_cast<float>(t)));
}

static mrb_value mrb_vec2_angle(mrb_state* mrb, mrb_value self) {
    return mrb_float_value(mrb, MrbReadVec2(mrb, self).Angle());
}

static mrb_value mrb_vec2_to_s(mrb_state* mrb, mrb_value self) {
    math::Vec2 v = MrbReadVec2(mrb, self);
    std::ostringstream oss;
    oss << "Vector2(" << v.x << ", " << v.y << ")";
    return mrb_str_new_cstr(mrb, oss.str().c_str());
}

static mrb_value mrb_vec3_initialize(mrb_state* mrb, mrb_value self) {
    mrb_value* argv = nullptr;
    mrb_int argc = 0;
    mrb_get_args(mrb, "*", &argv, &argc);
    float x = 0.0f, y = 0.0f, z = 0.0f;
    if (argc == 1) {
        x = y = z = MrbValueToFloat(mrb, argv[0]);
    } else if (argc >= 3) {
        x = MrbValueToFloat(mrb, argv[0]);
        y = MrbValueToFloat(mrb, argv[1]);
        z = MrbValueToFloat(mrb, argv[2]);
    } else if (argc == 2) {
        x = MrbValueToFloat(mrb, argv[0]);
        y = MrbValueToFloat(mrb, argv[1]);
    }
    MrbSetIvarFloat(mrb, self, "@x", x);
    MrbSetIvarFloat(mrb, self, "@y", y);
    MrbSetIvarFloat(mrb, self, "@z", z);
    return self;
}

static mrb_value mrb_vec3_get_x(mrb_state* mrb, mrb_value self) {
    return mrb_iv_get(mrb, self, mrb_intern_cstr(mrb, "@x"));
}

static mrb_value mrb_vec3_get_y(mrb_state* mrb, mrb_value self) {
    return mrb_iv_get(mrb, self, mrb_intern_cstr(mrb, "@y"));
}

static mrb_value mrb_vec3_get_z(mrb_state* mrb, mrb_value self) {
    return mrb_iv_get(mrb, self, mrb_intern_cstr(mrb, "@z"));
}

static mrb_value mrb_vec3_set_x(mrb_state* mrb, mrb_value self) {
    mrb_float v = 0.0;
    mrb_get_args(mrb, "f", &v);
    MrbSetIvarFloat(mrb, self, "@x", static_cast<float>(v));
    return mrb_float_value(mrb, static_cast<float>(v));
}

static mrb_value mrb_vec3_set_y(mrb_state* mrb, mrb_value self) {
    mrb_float v = 0.0;
    mrb_get_args(mrb, "f", &v);
    MrbSetIvarFloat(mrb, self, "@y", static_cast<float>(v));
    return mrb_float_value(mrb, static_cast<float>(v));
}

static mrb_value mrb_vec3_set_z(mrb_state* mrb, mrb_value self) {
    mrb_float v = 0.0;
    mrb_get_args(mrb, "f", &v);
    MrbSetIvarFloat(mrb, self, "@z", static_cast<float>(v));
    return mrb_float_value(mrb, static_cast<float>(v));
}

static mrb_value mrb_vec3_add(mrb_state* mrb, mrb_value self) {
    mrb_value other;
    mrb_get_args(mrb, "o", &other);
    return MrbMakeVec3(mrb, MrbReadVec3(mrb, self) + MrbReadVec3(mrb, other));
}

static mrb_value mrb_vec3_sub(mrb_state* mrb, mrb_value self) {
    mrb_value other;
    mrb_get_args(mrb, "o", &other);
    return MrbMakeVec3(mrb, MrbReadVec3(mrb, self) - MrbReadVec3(mrb, other));
}

static mrb_value mrb_vec3_mul(mrb_state* mrb, mrb_value self) {
    mrb_value other;
    mrb_get_args(mrb, "o", &other);
    math::Vec3 a = MrbReadVec3(mrb, self);
    if (MrbValueIsNumber(other)) {
        return MrbMakeVec3(mrb, a * MrbValueToFloat(mrb, other));
    }
    return MrbMakeVec3(mrb, a * MrbReadVec3(mrb, other));
}

static mrb_value mrb_vec3_div(mrb_state* mrb, mrb_value self) {
    mrb_value other;
    mrb_get_args(mrb, "o", &other);
    math::Vec3 a = MrbReadVec3(mrb, self);
    if (MrbValueIsNumber(other)) {
        return MrbMakeVec3(mrb, a / MrbValueToFloat(mrb, other));
    }
    return MrbMakeVec3(mrb, a / MrbReadVec3(mrb, other));
}

static mrb_value mrb_vec3_neg(mrb_state* mrb, mrb_value self) {
    return MrbMakeVec3(mrb, -MrbReadVec3(mrb, self));
}

static mrb_value mrb_vec3_eq(mrb_state* mrb, mrb_value self) {
    mrb_value other;
    mrb_get_args(mrb, "o", &other);
    struct RClass* cls = mrb_class_get(mrb, "Vector3");
    if (!mrb_obj_is_kind_of(mrb, other, cls)) return mrb_false_value();
    return mrb_bool_value(MrbReadVec3(mrb, self) == MrbReadVec3(mrb, other));
}

static mrb_value mrb_vec3_length(mrb_state* mrb, mrb_value self) {
    return mrb_float_value(mrb, MrbReadVec3(mrb, self).Length());
}

static mrb_value mrb_vec3_length_squared(mrb_state* mrb, mrb_value self) {
    return mrb_float_value(mrb, MrbReadVec3(mrb, self).LengthSquared());
}

static mrb_value mrb_vec3_normalized(mrb_state* mrb, mrb_value self) {
    return MrbMakeVec3(mrb, MrbReadVec3(mrb, self).Normalized());
}

static mrb_value mrb_vec3_dot(mrb_state* mrb, mrb_value self) {
    mrb_value other;
    mrb_get_args(mrb, "o", &other);
    return mrb_float_value(mrb, MrbReadVec3(mrb, self).Dot(MrbReadVec3(mrb, other)));
}

static mrb_value mrb_vec3_cross(mrb_state* mrb, mrb_value self) {
    mrb_value other;
    mrb_get_args(mrb, "o", &other);
    return MrbMakeVec3(mrb, MrbReadVec3(mrb, self).Cross(MrbReadVec3(mrb, other)));
}

static mrb_value mrb_vec3_distance_to(mrb_state* mrb, mrb_value self) {
    mrb_value other;
    mrb_get_args(mrb, "o", &other);
    return mrb_float_value(mrb, MrbReadVec3(mrb, self).Distance(MrbReadVec3(mrb, other)));
}

static mrb_value mrb_vec3_lerp(mrb_state* mrb, mrb_value self) {
    mrb_value other;
    mrb_float t = 0.0;
    mrb_get_args(mrb, "of", &other, &t);
    return MrbMakeVec3(mrb, MrbReadVec3(mrb, self).Lerp(MrbReadVec3(mrb, other), static_cast<float>(t)));
}

static mrb_value mrb_vec3_to_s(mrb_state* mrb, mrb_value self) {
    math::Vec3 v = MrbReadVec3(mrb, self);
    std::ostringstream oss;
    oss << "Vector3(" << v.x << ", " << v.y << ", " << v.z << ")";
    return mrb_str_new_cstr(mrb, oss.str().c_str());
}

static mrb_value mrb_color_initialize(mrb_state* mrb, mrb_value self) {
    mrb_value* argv = nullptr;
    mrb_int argc = 0;
    mrb_get_args(mrb, "*", &argv, &argc);
    float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;
    if (argc >= 1) r = MrbValueToFloat(mrb, argv[0]);
    if (argc >= 2) g = MrbValueToFloat(mrb, argv[1]);
    if (argc >= 3) b = MrbValueToFloat(mrb, argv[2]);
    if (argc >= 4) a = MrbValueToFloat(mrb, argv[3]);
    MrbSetIvarFloat(mrb, self, "@r", r);
    MrbSetIvarFloat(mrb, self, "@g", g);
    MrbSetIvarFloat(mrb, self, "@b", b);
    MrbSetIvarFloat(mrb, self, "@a", a);
    return self;
}

static mrb_value mrb_color_get_r(mrb_state* mrb, mrb_value self) {
    return mrb_iv_get(mrb, self, mrb_intern_cstr(mrb, "@r"));
}

static mrb_value mrb_color_get_g(mrb_state* mrb, mrb_value self) {
    return mrb_iv_get(mrb, self, mrb_intern_cstr(mrb, "@g"));
}

static mrb_value mrb_color_get_b(mrb_state* mrb, mrb_value self) {
    return mrb_iv_get(mrb, self, mrb_intern_cstr(mrb, "@b"));
}

static mrb_value mrb_color_get_a(mrb_state* mrb, mrb_value self) {
    return mrb_iv_get(mrb, self, mrb_intern_cstr(mrb, "@a"));
}

static mrb_value mrb_color_set_r(mrb_state* mrb, mrb_value self) {
    mrb_float v = 0.0;
    mrb_get_args(mrb, "f", &v);
    MrbSetIvarFloat(mrb, self, "@r", static_cast<float>(v));
    return mrb_float_value(mrb, static_cast<float>(v));
}

static mrb_value mrb_color_set_g(mrb_state* mrb, mrb_value self) {
    mrb_float v = 0.0;
    mrb_get_args(mrb, "f", &v);
    MrbSetIvarFloat(mrb, self, "@g", static_cast<float>(v));
    return mrb_float_value(mrb, static_cast<float>(v));
}

static mrb_value mrb_color_set_b(mrb_state* mrb, mrb_value self) {
    mrb_float v = 0.0;
    mrb_get_args(mrb, "f", &v);
    MrbSetIvarFloat(mrb, self, "@b", static_cast<float>(v));
    return mrb_float_value(mrb, static_cast<float>(v));
}

static mrb_value mrb_color_set_a(mrb_state* mrb, mrb_value self) {
    mrb_float v = 0.0;
    mrb_get_args(mrb, "f", &v);
    MrbSetIvarFloat(mrb, self, "@a", static_cast<float>(v));
    return mrb_float_value(mrb, static_cast<float>(v));
}

static mrb_value mrb_color_add(mrb_state* mrb, mrb_value self) {
    mrb_value other;
    mrb_get_args(mrb, "o", &other);
    return MrbMakeColor(mrb, MrbReadColor(mrb, self) + MrbReadColor(mrb, other));
}

static mrb_value mrb_color_sub(mrb_state* mrb, mrb_value self) {
    mrb_value other;
    mrb_get_args(mrb, "o", &other);
    return MrbMakeColor(mrb, MrbReadColor(mrb, self) - MrbReadColor(mrb, other));
}

static mrb_value mrb_color_mul(mrb_state* mrb, mrb_value self) {
    mrb_value other;
    mrb_get_args(mrb, "o", &other);
    math::Color c = MrbReadColor(mrb, self);
    if (MrbValueIsNumber(other)) {
        return MrbMakeColor(mrb, c * MrbValueToFloat(mrb, other));
    }
    return MrbMakeColor(mrb, c * MrbReadColor(mrb, other));
}

static mrb_value mrb_color_eq(mrb_state* mrb, mrb_value self) {
    mrb_value other;
    mrb_get_args(mrb, "o", &other);
    struct RClass* cls = mrb_class_get(mrb, "Color");
    if (!mrb_obj_is_kind_of(mrb, other, cls)) return mrb_false_value();
    return mrb_bool_value(MrbReadColor(mrb, self) == MrbReadColor(mrb, other));
}

static mrb_value mrb_color_lerp(mrb_state* mrb, mrb_value self) {
    mrb_value other;
    mrb_float t = 0.0;
    mrb_get_args(mrb, "of", &other, &t);
    return MrbMakeColor(mrb, MrbReadColor(mrb, self).Lerp(MrbReadColor(mrb, other), static_cast<float>(t)));
}

static mrb_value mrb_color_with_alpha(mrb_state* mrb, mrb_value self) {
    mrb_float a = 1.0;
    mrb_get_args(mrb, "f", &a);
    math::Color c = MrbReadColor(mrb, self);
    c.a = static_cast<float>(a);
    return MrbMakeColor(mrb, c);
}

static mrb_value mrb_color_to_hex(mrb_state* mrb, mrb_value self) {
    math::Color c = MrbReadColor(mrb, self);
    auto toByte = [](float f) -> int {
        int i = static_cast<int>(f * 255.0f + 0.5f);
        if (i < 0) i = 0;
        if (i > 255) i = 255;
        return i;
    };
    char buf[16];
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X",
                  toByte(c.r), toByte(c.g), toByte(c.b), toByte(c.a));
    return mrb_str_new_cstr(mrb, buf);
}

static mrb_value mrb_color_to_s(mrb_state* mrb, mrb_value self) {
    math::Color c = MrbReadColor(mrb, self);
    std::ostringstream oss;
    oss << "Color(" << c.r << ", " << c.g << ", " << c.b << ", " << c.a << ")";
    return mrb_str_new_cstr(mrb, oss.str().c_str());
}

static void RegisterValueTypeClasses(mrb_state* mrb) {
    struct RClass* vec2Class = mrb_define_class(mrb, "Vector2", mrb->object_class);
    mrb_define_method(mrb, vec2Class, "initialize", mrb_vec2_initialize, MRB_ARGS_OPT(2));
    mrb_define_method(mrb, vec2Class, "x", mrb_vec2_get_x, MRB_ARGS_NONE());
    mrb_define_method(mrb, vec2Class, "y", mrb_vec2_get_y, MRB_ARGS_NONE());
    mrb_define_method(mrb, vec2Class, "x=", mrb_vec2_set_x, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, vec2Class, "y=", mrb_vec2_set_y, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, vec2Class, "+", mrb_vec2_add, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, vec2Class, "-", mrb_vec2_sub, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, vec2Class, "*", mrb_vec2_mul, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, vec2Class, "/", mrb_vec2_div, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, vec2Class, "-@", mrb_vec2_neg, MRB_ARGS_NONE());
    mrb_define_method(mrb, vec2Class, "==", mrb_vec2_eq, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, vec2Class, "length", mrb_vec2_length, MRB_ARGS_NONE());
    mrb_define_method(mrb, vec2Class, "length_squared", mrb_vec2_length_squared, MRB_ARGS_NONE());
    mrb_define_method(mrb, vec2Class, "normalized", mrb_vec2_normalized, MRB_ARGS_NONE());
    mrb_define_method(mrb, vec2Class, "dot", mrb_vec2_dot, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, vec2Class, "distance_to", mrb_vec2_distance_to, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, vec2Class, "lerp", mrb_vec2_lerp, MRB_ARGS_REQ(2));
    mrb_define_method(mrb, vec2Class, "angle", mrb_vec2_angle, MRB_ARGS_NONE());
    mrb_define_method(mrb, vec2Class, "to_s", mrb_vec2_to_s, MRB_ARGS_NONE());

    struct RClass* vec3Class = mrb_define_class(mrb, "Vector3", mrb->object_class);
    mrb_define_method(mrb, vec3Class, "initialize", mrb_vec3_initialize, MRB_ARGS_OPT(3));
    mrb_define_method(mrb, vec3Class, "x", mrb_vec3_get_x, MRB_ARGS_NONE());
    mrb_define_method(mrb, vec3Class, "y", mrb_vec3_get_y, MRB_ARGS_NONE());
    mrb_define_method(mrb, vec3Class, "z", mrb_vec3_get_z, MRB_ARGS_NONE());
    mrb_define_method(mrb, vec3Class, "x=", mrb_vec3_set_x, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, vec3Class, "y=", mrb_vec3_set_y, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, vec3Class, "z=", mrb_vec3_set_z, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, vec3Class, "+", mrb_vec3_add, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, vec3Class, "-", mrb_vec3_sub, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, vec3Class, "*", mrb_vec3_mul, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, vec3Class, "/", mrb_vec3_div, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, vec3Class, "-@", mrb_vec3_neg, MRB_ARGS_NONE());
    mrb_define_method(mrb, vec3Class, "==", mrb_vec3_eq, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, vec3Class, "length", mrb_vec3_length, MRB_ARGS_NONE());
    mrb_define_method(mrb, vec3Class, "length_squared", mrb_vec3_length_squared, MRB_ARGS_NONE());
    mrb_define_method(mrb, vec3Class, "normalized", mrb_vec3_normalized, MRB_ARGS_NONE());
    mrb_define_method(mrb, vec3Class, "dot", mrb_vec3_dot, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, vec3Class, "cross", mrb_vec3_cross, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, vec3Class, "distance_to", mrb_vec3_distance_to, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, vec3Class, "lerp", mrb_vec3_lerp, MRB_ARGS_REQ(2));
    mrb_define_method(mrb, vec3Class, "to_s", mrb_vec3_to_s, MRB_ARGS_NONE());

    struct RClass* colorClass = mrb_define_class(mrb, "Color", mrb->object_class);
    mrb_define_method(mrb, colorClass, "initialize", mrb_color_initialize, MRB_ARGS_OPT(4));
    mrb_define_method(mrb, colorClass, "r", mrb_color_get_r, MRB_ARGS_NONE());
    mrb_define_method(mrb, colorClass, "g", mrb_color_get_g, MRB_ARGS_NONE());
    mrb_define_method(mrb, colorClass, "b", mrb_color_get_b, MRB_ARGS_NONE());
    mrb_define_method(mrb, colorClass, "a", mrb_color_get_a, MRB_ARGS_NONE());
    mrb_define_method(mrb, colorClass, "r=", mrb_color_set_r, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, colorClass, "g=", mrb_color_set_g, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, colorClass, "b=", mrb_color_set_b, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, colorClass, "a=", mrb_color_set_a, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, colorClass, "+", mrb_color_add, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, colorClass, "-", mrb_color_sub, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, colorClass, "*", mrb_color_mul, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, colorClass, "==", mrb_color_eq, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, colorClass, "lerp", mrb_color_lerp, MRB_ARGS_REQ(2));
    mrb_define_method(mrb, colorClass, "with_alpha", mrb_color_with_alpha, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, colorClass, "to_hex", mrb_color_to_hex, MRB_ARGS_NONE());
    mrb_define_method(mrb, colorClass, "to_s", mrb_color_to_s, MRB_ARGS_NONE());
}

// --- Networking module functions ------------------------------------------

static network::NetworkConfig MrbNetConfig(mrb_int port) {
    network::NetworkConfig config = network::NetworkManager::GetInstance().GetDefaultConfig();
    config.port = static_cast<uint16_t>(port);
    return config;
}

static mrb_value mrb_net_start_server(mrb_state* mrb, mrb_value) {
    mrb_int port; mrb_int maxPeers = 0;
    const mrb_int argc = mrb_get_args(mrb, "i|i", &port, &maxPeers);
    network::NetworkConfig config = MrbNetConfig(port);
    if (argc >= 2) {
        config.maxPeers = static_cast<uint32_t>(maxPeers);
    }
    return mrb_bool_value(network::NetworkManager::GetInstance().StartServer(config));
}
static mrb_value mrb_net_start_host(mrb_state* mrb, mrb_value) {
    mrb_int port; mrb_int maxPeers = 0;
    const mrb_int argc = mrb_get_args(mrb, "i|i", &port, &maxPeers);
    network::NetworkConfig config = MrbNetConfig(port);
    if (argc >= 2) {
        config.maxPeers = static_cast<uint32_t>(maxPeers);
    }
    return mrb_bool_value(network::NetworkManager::GetInstance().StartHost(config));
}
static mrb_value mrb_net_connect(mrb_state* mrb, mrb_value) {
    char* address; mrb_int port;
    mrb_get_args(mrb, "zi", &address, &port);
    network::NetworkConfig config = MrbNetConfig(port);
    config.address = address;
    return mrb_bool_value(network::NetworkManager::GetInstance().Connect(config));
}
static mrb_value mrb_net_disconnect(mrb_state* mrb, mrb_value) {
    (void)mrb;
    network::NetworkManager::GetInstance().Disconnect();
    return mrb_nil_value();
}
static mrb_value mrb_net_is_server(mrb_state* mrb, mrb_value) {
    (void)mrb; return mrb_bool_value(network::NetworkManager::GetInstance().IsServer());
}
static mrb_value mrb_net_is_client(mrb_state* mrb, mrb_value) {
    (void)mrb; return mrb_bool_value(network::NetworkManager::GetInstance().IsClient());
}
static mrb_value mrb_net_is_active(mrb_state* mrb, mrb_value) {
    (void)mrb; return mrb_bool_value(network::NetworkManager::GetInstance().IsActive());
}
static mrb_value mrb_net_local_peer_id(mrb_state* mrb, mrb_value) {
    (void)mrb;
    return mrb_fixnum_value(static_cast<mrb_int>(network::NetworkManager::GetInstance().GetLocalPeerId()));
}
static mrb_value mrb_net_peer_count(mrb_state* mrb, mrb_value) {
    (void)mrb;
    return mrb_fixnum_value(static_cast<mrb_int>(network::NetworkManager::GetInstance().GetPeerCount()));
}
static mrb_value mrb_net_get_peers(mrb_state* mrb, mrb_value) {
    mrb_value arr = mrb_ary_new(mrb);
    for (const network::PeerInfo& peer : network::NetworkManager::GetInstance().GetPeers()) {
        mrb_ary_push(mrb, arr, mrb_fixnum_value(static_cast<mrb_int>(peer.id)));
    }
    return arr;
}
static mrb_value mrb_net_connect_signal(mrb_state* mrb, mrb_value) {
    char* event; mrb_value target; char* method;
    mrb_get_args(mrb, "zoz", &event, &target, &method);
    NodeRef* node = ArgNode(target);
    if (node == nullptr) {
        return mrb_fixnum_value(0);
    }
    std::shared_ptr<core::Node> shared = node->Lock();
    if (!shared) {
        return mrb_fixnum_value(0);
    }
    uint64_t id = network::NetworkManager::GetInstance().Events().Connect(event, shared.get(), method);
    return mrb_fixnum_value(static_cast<mrb_int>(id));
}
static mrb_value mrb_net_get_peer_rtt(mrb_state* mrb, mrb_value) {
    mrb_int peerId;
    mrb_get_args(mrb, "i", &peerId);
    for (const network::PeerInfo& peer : network::NetworkManager::GetInstance().GetPeers()) {
        if (static_cast<mrb_int>(peer.id) == peerId) {
            return mrb_float_value(mrb, static_cast<mrb_float>(peer.roundTripMs));
        }
    }
    return mrb_float_value(mrb, 0.0);
}
static mrb_value mrb_net_get_stats(mrb_state* mrb, mrb_value) {
    const network::NetworkStats s = network::NetworkManager::GetInstance().GetStats();
    mrb_value hash = mrb_hash_new(mrb);
    auto put = [&](const char* key, mrb_value value) {
        mrb_hash_set(mrb, hash, mrb_symbol_value(mrb_intern_cstr(mrb, key)), value);
    };
    put("peer_count", mrb_fixnum_value(static_cast<mrb_int>(s.peerCount)));
    put("bytes_in_per_sec", mrb_float_value(mrb, s.bytesInPerSec));
    put("bytes_out_per_sec", mrb_float_value(mrb, s.bytesOutPerSec));
    put("total_bytes_in", mrb_float_value(mrb, static_cast<mrb_float>(s.totalBytesIn)));
    put("total_bytes_out", mrb_float_value(mrb, static_cast<mrb_float>(s.totalBytesOut)));
    put("average_rtt_ms", mrb_float_value(mrb, s.averageRttMs));
    put("packet_loss_percent", mrb_float_value(mrb, s.averagePacketLossPercent));
    put("snapshots_sent", mrb_float_value(mrb, static_cast<mrb_float>(s.snapshotsSent)));
    put("snapshots_received", mrb_float_value(mrb, static_cast<mrb_float>(s.snapshotsReceived)));
    return hash;
}
static mrb_value mrb_net_get_peer_loss(mrb_state* mrb, mrb_value) {
    mrb_int peerId;
    mrb_get_args(mrb, "i", &peerId);
    network::PeerInfo info;
    if (network::NetworkManager::GetInstance().GetPeerInfo(static_cast<network::PeerId>(peerId), info)) {
        return mrb_float_value(mrb, static_cast<mrb_float>(info.packetLossPercent));
    }
    return mrb_float_value(mrb, 0.0);
}
static mrb_value mrb_net_get_peer_jitter(mrb_state* mrb, mrb_value) {
    mrb_int peerId;
    mrb_get_args(mrb, "i", &peerId);
    network::PeerInfo info;
    if (network::NetworkManager::GetInstance().GetPeerInfo(static_cast<network::PeerId>(peerId), info)) {
        return mrb_float_value(mrb, static_cast<mrb_float>(info.jitterMs));
    }
    return mrb_float_value(mrb, 0.0);
}
static mrb_value mrb_net_kick_peer(mrb_state* mrb, mrb_value) {
    mrb_int peerId;
    mrb_get_args(mrb, "i", &peerId);
    return mrb_bool_value(network::NetworkManager::GetInstance().KickPeer(
        static_cast<network::PeerId>(peerId)));
}
static mrb_value mrb_net_set_interest_2d(mrb_state* mrb, mrb_value) {
    mrb_float x, y;
    mrb_get_args(mrb, "ff", &x, &y);
    network::NetworkManager::GetInstance().SetInterestPosition2D(
        static_cast<float>(x), static_cast<float>(y));
    return mrb_nil_value();
}
static mrb_value mrb_net_set_interest_3d(mrb_state* mrb, mrb_value) {
    mrb_float x, y, z;
    mrb_get_args(mrb, "fff", &x, &y, &z);
    network::NetworkManager::GetInstance().SetInterestPosition3D(
        static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
    return mrb_nil_value();
}
static mrb_value mrb_net_start_lan_advertising(mrb_state* mrb, mrb_value) {
    (void)mrb; return mrb_bool_value(network::NetworkManager::GetInstance().StartLanAdvertising());
}
static mrb_value mrb_net_stop_lan_advertising(mrb_state* mrb, mrb_value) {
    (void)mrb; network::NetworkManager::GetInstance().StopLanAdvertising(); return mrb_nil_value();
}
static mrb_value mrb_net_is_lan_advertising(mrb_state* mrb, mrb_value) {
    (void)mrb; return mrb_bool_value(network::NetworkManager::GetInstance().IsLanAdvertising());
}
static mrb_value mrb_net_start_lan_discovery(mrb_state* mrb, mrb_value) {
    char* gameId; mrb_int port = 7779;
    mrb_get_args(mrb, "z|i", &gameId, &port);
    return mrb_bool_value(network::NetworkManager::GetInstance().StartLanDiscovery(
        gameId, static_cast<uint16_t>(port)));
}
static mrb_value mrb_net_stop_lan_discovery(mrb_state* mrb, mrb_value) {
    (void)mrb; network::NetworkManager::GetInstance().StopLanDiscovery(); return mrb_nil_value();
}
static mrb_value mrb_net_is_lan_discovering(mrb_state* mrb, mrb_value) {
    (void)mrb; return mrb_bool_value(network::NetworkManager::GetInstance().IsLanDiscovering());
}
static mrb_value mrb_net_get_discovered_servers(mrb_state* mrb, mrb_value) {
    mrb_value arr = mrb_ary_new(mrb);
    for (const network::LanServerInfo& server :
         network::NetworkManager::GetInstance().GetDiscoveredServers()) {
        mrb_value h = mrb_hash_new(mrb);
        mrb_hash_set(mrb, h, mrb_str_new_cstr(mrb, "game_id"), mrb_str_new_cstr(mrb, server.gameId.c_str()));
        mrb_hash_set(mrb, h, mrb_str_new_cstr(mrb, "name"), mrb_str_new_cstr(mrb, server.name.c_str()));
        mrb_hash_set(mrb, h, mrb_str_new_cstr(mrb, "address"), mrb_str_new_cstr(mrb, server.address.c_str()));
        mrb_hash_set(mrb, h, mrb_str_new_cstr(mrb, "port"), mrb_fixnum_value(static_cast<mrb_int>(server.gamePort)));
        mrb_hash_set(mrb, h, mrb_str_new_cstr(mrb, "player_count"), mrb_fixnum_value(static_cast<mrb_int>(server.playerCount)));
        mrb_hash_set(mrb, h, mrb_str_new_cstr(mrb, "max_players"), mrb_fixnum_value(static_cast<mrb_int>(server.maxPlayers)));
        mrb_hash_set(mrb, h, mrb_str_new_cstr(mrb, "protocol_version"), mrb_fixnum_value(static_cast<mrb_int>(server.protocolVersion)));
        mrb_hash_set(mrb, h, mrb_str_new_cstr(mrb, "age_seconds"), mrb_float_value(mrb, static_cast<mrb_float>(server.ageSeconds)));
        mrb_ary_push(mrb, arr, h);
    }
    return arr;
}

void MRubyHost::RegisterScriptAPI() {
    if (!m_MRubyState) return;

    struct RClass* lupine_module = mrb_define_module(m_MRubyState, "Lupine");

    struct RClass* network_module = mrb_define_module(m_MRubyState, "Network");
    mrb_define_module_function(m_MRubyState, network_module, "start_server", mrb_net_start_server, MRB_ARGS_ARG(1, 1));
    mrb_define_module_function(m_MRubyState, network_module, "start_host", mrb_net_start_host, MRB_ARGS_ARG(1, 1));
    mrb_define_module_function(m_MRubyState, network_module, "connect", mrb_net_connect, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, network_module, "disconnect", mrb_net_disconnect, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, network_module, "is_server", mrb_net_is_server, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, network_module, "is_client", mrb_net_is_client, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, network_module, "is_active", mrb_net_is_active, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, network_module, "get_local_peer_id", mrb_net_local_peer_id, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, network_module, "get_peer_count", mrb_net_peer_count, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, network_module, "get_peers", mrb_net_get_peers, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, network_module, "connect_signal", mrb_net_connect_signal, MRB_ARGS_REQ(3));
    mrb_define_module_function(m_MRubyState, network_module, "get_peer_rtt", mrb_net_get_peer_rtt, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, network_module, "get_stats", mrb_net_get_stats, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, network_module, "get_peer_loss", mrb_net_get_peer_loss, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, network_module, "get_peer_jitter", mrb_net_get_peer_jitter, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, network_module, "kick_peer", mrb_net_kick_peer, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, network_module, "set_interest_2d", mrb_net_set_interest_2d, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, network_module, "set_interest_3d", mrb_net_set_interest_3d, MRB_ARGS_REQ(3));
    mrb_define_module_function(m_MRubyState, network_module, "start_lan_advertising", mrb_net_start_lan_advertising, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, network_module, "stop_lan_advertising", mrb_net_stop_lan_advertising, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, network_module, "is_lan_advertising", mrb_net_is_lan_advertising, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, network_module, "start_lan_discovery", mrb_net_start_lan_discovery, MRB_ARGS_ARG(1, 1));
    mrb_define_module_function(m_MRubyState, network_module, "stop_lan_discovery", mrb_net_stop_lan_discovery, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, network_module, "is_lan_discovering", mrb_net_is_lan_discovering, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, network_module, "get_discovered_servers", mrb_net_get_discovered_servers, MRB_ARGS_NONE());

    RegisterNodeObjectClasses(m_MRubyState);
    RegisterTimerSceneTreeClasses(m_MRubyState);
    RegisterValueTypeClasses(m_MRubyState);

    // Used only by the Fiber scheduler installed in EnsureInitialized.
    mrb_define_module_function(m_MRubyState, lupine_module, "_context_id", mrb_lupine_context_id, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "_push_context", mrb_lupine_push_context, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "_pop_context", mrb_lupine_pop_context, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "_scheduler_error", mrb_lupine_scheduler_error, MRB_ARGS_REQ(1));

    mrb_define_module_function(m_MRubyState, lupine_module, "get_node", mrb_script_api_get_node, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "find_node", mrb_script_api_get_node, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "find_node_by_uuid", mrb_script_api_find_node_by_uuid, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_self", mrb_script_api_get_self, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "get_singleton", mrb_script_api_get_singleton, MRB_ARGS_REQ(1));

    // Root / scene / tree access
    mrb_define_module_function(m_MRubyState, lupine_module, "get_root", mrb_script_api_get_root, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "get_scene", mrb_script_api_get_scene, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "get_tree", mrb_script_api_get_tree, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "get_sibling_index", mrb_script_api_get_sibling_index, MRB_ARGS_NONE());

    // Runtime instantiation
    mrb_define_module_function(m_MRubyState, lupine_module, "instantiate_prefab", mrb_script_api_instantiate_prefab, MRB_ARGS_ARG(1, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "instantiate_scene", mrb_script_api_instantiate_scene, MRB_ARGS_ARG(1, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "create_node", mrb_script_api_create_node, MRB_ARGS_ARG(1, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "create_node_child", mrb_script_api_create_node_child, MRB_ARGS_ARG(2, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "duplicate_node", mrb_script_api_duplicate_node, MRB_ARGS_REQ(1));

    // Timers
    mrb_define_module_function(m_MRubyState, lupine_module, "create_timer", mrb_script_api_create_timer, MRB_ARGS_ARG(1, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "create_repeating_timer", mrb_script_api_create_repeating_timer, MRB_ARGS_ARG(1, 2));
    mrb_define_module_function(m_MRubyState, lupine_module, "create_named_timer", mrb_script_api_create_named_timer, MRB_ARGS_ARG(2, 3));
    mrb_define_module_function(m_MRubyState, lupine_module, "list_timers", mrb_script_api_list_timers, MRB_ARGS_NONE());

    // Tweens
    mrb_define_module_function(m_MRubyState, lupine_module, "create_tween", mrb_script_api_create_tween, MRB_ARGS_ARG(3, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "list_tweens", mrb_script_api_list_tweens, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "create_sequence", mrb_script_api_create_sequence, MRB_ARGS_NONE());

    // Sandboxed file I/O + JSON (res:// user:// temp:// only)
    mrb_define_module_function(m_MRubyState, lupine_module, "read_text", mrb_script_api_read_text, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "write_text", mrb_script_api_write_text, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "append_text", mrb_script_api_append_text, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "read_bytes", mrb_script_api_read_bytes, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "write_bytes", mrb_script_api_write_bytes, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "file_exists", mrb_script_api_file_exists, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "is_file", mrb_script_api_is_file, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "is_dir", mrb_script_api_is_dir, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "remove_file", mrb_script_api_remove_file, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "delete_file", mrb_script_api_remove_file, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "make_dir", mrb_script_api_make_dir, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "ensure_dir", mrb_script_api_make_dir, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "list_dir", mrb_script_api_list_dir, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "file_size", mrb_script_api_file_size, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "to_json", mrb_script_api_to_json, MRB_ARGS_ARG(1, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "from_json", mrb_script_api_from_json, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "read_json", mrb_script_api_read_json, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "write_json", mrb_script_api_write_json, MRB_ARGS_ARG(2, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "user_dir", mrb_script_api_user_dir, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "res_dir", mrb_script_api_res_dir, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "temp_dir", mrb_script_api_temp_dir, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "join_path", mrb_script_api_join_path, MRB_ARGS_ANY());

    // Save games
    mrb_define_module_function(m_MRubyState, lupine_module, "save_game", mrb_script_api_save_game, MRB_ARGS_ARG(2, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "load_game", mrb_script_api_load_game, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "save_slot_exists", mrb_script_api_save_slot_exists, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "delete_save_slot", mrb_script_api_delete_save_slot, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "copy_save_slot", mrb_script_api_copy_save_slot, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "rename_save_slot", mrb_script_api_rename_save_slot, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "list_save_slots", mrb_script_api_list_save_slots, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "list_save_slot_infos", mrb_script_api_list_save_slot_infos, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "get_save_slot_info", mrb_script_api_get_save_slot_info, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "quick_save", mrb_script_api_quick_save, MRB_ARGS_ARG(1, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "quick_load", mrb_script_api_quick_load, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "has_quick_save", mrb_script_api_has_quick_save, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "auto_save", mrb_script_api_auto_save, MRB_ARGS_ARG(1, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "has_auto_save", mrb_script_api_has_auto_save, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "get_last_save_error", mrb_script_api_get_last_save_error, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_save_directory", mrb_script_api_set_save_directory, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "set_save_format", mrb_script_api_set_save_format, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "set_save_schema_version", mrb_script_api_set_save_schema_version, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_save_schema_version", mrb_script_api_get_save_schema_version, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_save_obfuscation_key", mrb_script_api_set_save_obfuscation_key, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "set_quick_save_slot", mrb_script_api_set_quick_save_slot, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "set_auto_save_slot", mrb_script_api_set_auto_save_slot, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "capture_scene_state", mrb_script_api_capture_scene_state, MRB_ARGS_OPT(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "restore_scene_state", mrb_script_api_restore_scene_state, MRB_ARGS_REQ(1));

    // Signals & global event bus
    mrb_define_module_function(m_MRubyState, lupine_module, "emit", mrb_script_api_emit, MRB_ARGS_REQ(1) | MRB_ARGS_REST());
    mrb_define_module_function(m_MRubyState, lupine_module, "connect", mrb_script_api_connect, MRB_ARGS_ARG(3, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "disconnect", mrb_script_api_disconnect, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "is_connected", mrb_script_api_is_connected, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "add_user_signal", mrb_script_api_add_user_signal, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "call_deferred", mrb_script_api_call_deferred, MRB_ARGS_REQ(1) | MRB_ARGS_REST());
    mrb_define_module_function(m_MRubyState, lupine_module, "emit_event", mrb_script_api_emit_event, MRB_ARGS_REQ(1) | MRB_ARGS_REST());
    mrb_define_module_function(m_MRubyState, lupine_module, "subscribe", mrb_script_api_subscribe, MRB_ARGS_ARG(2, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "unsubscribe", mrb_script_api_unsubscribe, MRB_ARGS_REQ(2));
    mrb_define_const(m_MRubyState, lupine_module, "CONNECT_DEFERRED", mrb_fixnum_value(static_cast<mrb_int>(core::Connect_Deferred)));
    mrb_define_const(m_MRubyState, lupine_module, "CONNECT_ONESHOT", mrb_fixnum_value(static_cast<mrb_int>(core::Connect_OneShot)));

    mrb_define_module_function(m_MRubyState, lupine_module, "log_info", mrb_script_api_log_info, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "log_warning", mrb_script_api_log_warning, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "log_error", mrb_script_api_log_error, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "log_debug", mrb_script_api_log_debug, MRB_ARGS_REQ(1));

    mrb_define_module_function(m_MRubyState, lupine_module, "is_action_pressed", mrb_script_api_is_action_pressed, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "is_action_just_pressed", mrb_script_api_is_action_just_pressed, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_axis", mrb_script_api_get_axis, MRB_ARGS_REQ(1));

    mrb_define_module_function(m_MRubyState, lupine_module, "get_delta_time", mrb_script_api_get_delta_time, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "get_time", mrb_script_api_get_time, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "get_frame_count", mrb_script_api_get_frame_count, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "random_range", mrb_script_api_random_range, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "random_range_int", mrb_script_api_random_range_int, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "quit", mrb_script_api_quit, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "get_cmdline_args", mrb_script_api_get_cmdline_args, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "get_child_count", mrb_script_api_get_child_count, MRB_ARGS_NONE());

    // Node name
    mrb_define_module_function(m_MRubyState, lupine_module, "get_name", mrb_script_api_get_name, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_name", mrb_script_api_set_name, MRB_ARGS_REQ(1));

    // Key/Mouse input
    mrb_define_module_function(m_MRubyState, lupine_module, "is_key_pressed", mrb_script_api_is_key_pressed, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "is_key_just_pressed", mrb_script_api_is_key_just_pressed, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "is_key_just_released", mrb_script_api_is_key_just_released, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "is_mouse_button_pressed", mrb_script_api_is_mouse_button_pressed, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "is_mouse_button_just_pressed", mrb_script_api_is_mouse_button_just_pressed, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_mouse_position", mrb_script_api_get_mouse_position, MRB_ARGS_NONE());

    // Node state
    mrb_define_module_function(m_MRubyState, lupine_module, "is_active", mrb_script_api_is_active, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_active", mrb_script_api_set_active, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "is_visible", mrb_script_api_is_visible, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_visible", mrb_script_api_set_visible, MRB_ARGS_REQ(1));

    // Scene management
    mrb_define_module_function(m_MRubyState, lupine_module, "queue_free", mrb_script_api_queue_free, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "change_scene", mrb_script_api_change_scene, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "reload_scene", mrb_script_api_reload_scene, MRB_ARGS_NONE());

    mrb_define_module_function(m_MRubyState, lupine_module, "set_bus_volume", mrb_script_api_set_bus_volume, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_bus_volume", mrb_script_api_get_bus_volume, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "add_bus_effect", mrb_script_api_add_bus_effect, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "remove_bus_effect", mrb_script_api_remove_bus_effect, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "move_bus_effect", mrb_script_api_move_bus_effect, MRB_ARGS_REQ(3));
    mrb_define_module_function(m_MRubyState, lupine_module, "clear_bus_effects", mrb_script_api_clear_bus_effects, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "set_bus_effect_enabled", mrb_script_api_set_bus_effect_enabled, MRB_ARGS_REQ(3));
    mrb_define_module_function(m_MRubyState, lupine_module, "set_bus_effect_parameter", mrb_script_api_set_bus_effect_parameter, MRB_ARGS_REQ(4));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_bus_effect_count", mrb_script_api_get_bus_effect_count, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_bus_effect_parameter", mrb_script_api_get_bus_effect_parameter, MRB_ARGS_REQ(3));
    mrb_define_module_function(m_MRubyState, lupine_module, "is_bus_effect_enabled", mrb_script_api_is_bus_effect_enabled, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_bus_level", mrb_script_api_get_bus_level, MRB_ARGS_REQ(1));

    mrb_define_module_function(m_MRubyState, lupine_module, "get_global_int", mrb_script_api_get_global_int, MRB_ARGS_ARG(1, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_global_float", mrb_script_api_get_global_float, MRB_ARGS_ARG(1, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_global_string", mrb_script_api_get_global_string, MRB_ARGS_ARG(1, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_global_bool", mrb_script_api_get_global_bool, MRB_ARGS_ARG(1, 1));

    mrb_define_module_function(m_MRubyState, lupine_module, "set_global_int", mrb_script_api_set_global_int, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "set_global_float", mrb_script_api_set_global_float, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "set_global_string", mrb_script_api_set_global_string, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "set_global_bool", mrb_script_api_set_global_bool, MRB_ARGS_REQ(2));

    mrb_define_module_function(m_MRubyState, lupine_module, "get_global", mrb_script_api_get_global, MRB_ARGS_ARG(1, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "set_global", mrb_script_api_set_global, MRB_ARGS_REQ(2));

    // Position/Movement APIs
    mrb_define_module_function(m_MRubyState, lupine_module, "load_archetype", mrb_script_api_load_archetype, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_archetype_field", mrb_script_api_get_archetype_field, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_archetype_class", mrb_script_api_get_archetype_class, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "archetype_is_a", mrb_script_api_archetype_is_a, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "call_archetype", mrb_script_api_call_archetype, MRB_ARGS_REQ(2) | MRB_ARGS_REST());
    mrb_define_module_function(m_MRubyState, lupine_module, "load_archetype_async", mrb_script_api_load_archetype_async, MRB_ARGS_ARG(1, 2));
    mrb_define_module_function(m_MRubyState, lupine_module, "load_archetype_definition_async", mrb_script_api_load_archetype_definition_async, MRB_ARGS_ARG(1, 2));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_archetype_load_status", mrb_script_api_get_archetype_load_status, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "is_archetype_load_complete", mrb_script_api_is_archetype_load_complete, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_async_archetype", mrb_script_api_get_async_archetype, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "cancel_archetype_load", mrb_script_api_cancel_archetype_load, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "set_archetype_load_priority", mrb_script_api_set_archetype_load_priority, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_archetype_load_priority", mrb_script_api_get_archetype_load_priority, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "set_archetype_streaming_budget", mrb_script_api_set_archetype_streaming_budget, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_archetype_streaming_budget", mrb_script_api_get_archetype_streaming_budget, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "get_archetype_inflight_count", mrb_script_api_get_archetype_inflight_count, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "get_archetype_queued_count", mrb_script_api_get_archetype_queued_count, MRB_ARGS_NONE());
    mrb_define_const(m_MRubyState, lupine_module, "STREAM_PRIORITY_LOW", mrb_fixnum_value(ScriptAPI::ASYNC_PRIORITY_LOW));
    mrb_define_const(m_MRubyState, lupine_module, "STREAM_PRIORITY_NORMAL", mrb_fixnum_value(ScriptAPI::ASYNC_PRIORITY_NORMAL));
    mrb_define_const(m_MRubyState, lupine_module, "STREAM_PRIORITY_HIGH", mrb_fixnum_value(ScriptAPI::ASYNC_PRIORITY_HIGH));
    mrb_define_const(m_MRubyState, lupine_module, "STREAM_PRIORITY_CRITICAL", mrb_fixnum_value(ScriptAPI::ASYNC_PRIORITY_CRITICAL));

    mrb_define_module_function(m_MRubyState, lupine_module, "get_position_2d", mrb_script_api_get_position_2d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_position_2d", mrb_script_api_set_position_2d, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "translate_2d", mrb_script_api_translate_2d, MRB_ARGS_REQ(2));

    mrb_define_module_function(m_MRubyState, lupine_module, "get_position_3d", mrb_script_api_get_position_3d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_position_3d", mrb_script_api_set_position_3d, MRB_ARGS_REQ(3));
    mrb_define_module_function(m_MRubyState, lupine_module, "translate_3d", mrb_script_api_translate_3d, MRB_ARGS_REQ(3));

    mrb_define_module_function(m_MRubyState, lupine_module, "get_rotation_2d", mrb_script_api_get_rotation_2d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_rotation_2d", mrb_script_api_set_rotation_2d, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "rotate_2d", mrb_script_api_rotate_2d, MRB_ARGS_REQ(1));

    mrb_define_module_function(m_MRubyState, lupine_module, "get_scale_2d", mrb_script_api_get_scale_2d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_scale_2d", mrb_script_api_set_scale_2d, MRB_ARGS_REQ(2));

    // Physics 2D - Queries
    mrb_define_module_function(m_MRubyState, lupine_module, "raycast_2d", mrb_script_api_raycast_2d, MRB_ARGS_REQ(5));
    mrb_define_module_function(m_MRubyState, lupine_module, "raycast_3d", mrb_script_api_raycast_3d, MRB_ARGS_REQ(7));

    // Physics 2D - Body Manipulation
    mrb_define_module_function(m_MRubyState, lupine_module, "get_linear_velocity_2d", mrb_script_api_get_linear_velocity_2d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_linear_velocity_2d", mrb_script_api_set_linear_velocity_2d, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_angular_velocity_2d", mrb_script_api_get_angular_velocity_2d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_angular_velocity_2d", mrb_script_api_set_angular_velocity_2d, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "apply_force_2d", mrb_script_api_apply_force_2d, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "apply_force_at_point_2d", mrb_script_api_apply_force_at_point_2d, MRB_ARGS_REQ(4));
    mrb_define_module_function(m_MRubyState, lupine_module, "apply_torque_2d", mrb_script_api_apply_torque_2d, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "apply_impulse_2d", mrb_script_api_apply_impulse_2d, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "apply_impulse_at_point_2d", mrb_script_api_apply_impulse_at_point_2d, MRB_ARGS_REQ(4));
    mrb_define_module_function(m_MRubyState, lupine_module, "apply_angular_impulse_2d", mrb_script_api_apply_angular_impulse_2d, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_mass_2d", mrb_script_api_get_mass_2d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "get_gravity_scale_2d", mrb_script_api_get_gravity_scale_2d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_gravity_scale_2d", mrb_script_api_set_gravity_scale_2d, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_linear_damping_2d", mrb_script_api_get_linear_damping_2d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_linear_damping_2d", mrb_script_api_set_linear_damping_2d, MRB_ARGS_REQ(1));

    // Physics 3D - Body Manipulation
    mrb_define_module_function(m_MRubyState, lupine_module, "get_linear_velocity_3d", mrb_script_api_get_linear_velocity_3d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_linear_velocity_3d", mrb_script_api_set_linear_velocity_3d, MRB_ARGS_REQ(3));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_angular_velocity_3d", mrb_script_api_get_angular_velocity_3d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_angular_velocity_3d", mrb_script_api_set_angular_velocity_3d, MRB_ARGS_REQ(3));
    mrb_define_module_function(m_MRubyState, lupine_module, "apply_force_3d", mrb_script_api_apply_force_3d, MRB_ARGS_REQ(3));
    mrb_define_module_function(m_MRubyState, lupine_module, "apply_torque_3d", mrb_script_api_apply_torque_3d, MRB_ARGS_REQ(3));
    mrb_define_module_function(m_MRubyState, lupine_module, "apply_impulse_3d", mrb_script_api_apply_impulse_3d, MRB_ARGS_REQ(3));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_mass_3d", mrb_script_api_get_mass_3d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_mass_3d", mrb_script_api_set_mass_3d, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_gravity_scale_3d", mrb_script_api_get_gravity_scale_3d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_gravity_scale_3d", mrb_script_api_set_gravity_scale_3d, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_linear_damping_3d", mrb_script_api_get_linear_damping_3d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_linear_damping_3d", mrb_script_api_set_linear_damping_3d, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_angular_damping_3d", mrb_script_api_get_angular_damping_3d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_angular_damping_3d", mrb_script_api_set_angular_damping_3d, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_linear_factor_3d", mrb_script_api_get_linear_factor_3d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_linear_factor_3d", mrb_script_api_set_linear_factor_3d, MRB_ARGS_REQ(3));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_angular_factor_3d", mrb_script_api_get_angular_factor_3d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_angular_factor_3d", mrb_script_api_set_angular_factor_3d, MRB_ARGS_REQ(3));
    mrb_define_module_function(m_MRubyState, lupine_module, "apply_force_at_point_3d", mrb_script_api_apply_force_at_point_3d, MRB_ARGS_REQ(6));
    mrb_define_module_function(m_MRubyState, lupine_module, "apply_impulse_at_point_3d", mrb_script_api_apply_impulse_at_point_3d, MRB_ARGS_REQ(6));
    mrb_define_module_function(m_MRubyState, lupine_module, "apply_torque_impulse_3d", mrb_script_api_apply_torque_impulse_3d, MRB_ARGS_REQ(3));

    // Physics World Access
    mrb_define_module_function(m_MRubyState, lupine_module, "set_gravity_2d", mrb_script_api_set_gravity_2d, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_gravity_2d", mrb_script_api_get_gravity_2d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_gravity_3d", mrb_script_api_set_gravity_3d, MRB_ARGS_REQ(3));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_gravity_3d", mrb_script_api_get_gravity_3d, MRB_ARGS_NONE());

    // Character Controller 2D
    mrb_define_module_function(m_MRubyState, lupine_module, "move_and_slide_2d", mrb_script_api_move_and_slide_2d, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_character_velocity_2d", mrb_script_api_get_character_velocity_2d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_character_velocity_2d", mrb_script_api_set_character_velocity_2d, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "is_on_ground_2d", mrb_script_api_is_on_ground_2d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "is_on_wall_2d", mrb_script_api_is_on_wall_2d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "is_on_ceiling_2d", mrb_script_api_is_on_ceiling_2d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "get_ground_normal_2d", mrb_script_api_get_ground_normal_2d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "get_wall_normal_2d", mrb_script_api_get_wall_normal_2d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "get_character_gravity_2d", mrb_script_api_get_character_gravity_2d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_character_gravity_2d", mrb_script_api_set_character_gravity_2d, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_character_max_fall_speed_2d", mrb_script_api_get_character_max_fall_speed_2d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_character_max_fall_speed_2d", mrb_script_api_set_character_max_fall_speed_2d, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_character_max_slope_angle_2d", mrb_script_api_get_character_max_slope_angle_2d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_character_max_slope_angle_2d", mrb_script_api_set_character_max_slope_angle_2d, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_character_snap_to_ground_2d", mrb_script_api_get_character_snap_to_ground_2d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_character_snap_to_ground_2d", mrb_script_api_set_character_snap_to_ground_2d, MRB_ARGS_REQ(1));

    // Character Controller 3D
    mrb_define_module_function(m_MRubyState, lupine_module, "move_and_slide_3d", mrb_script_api_move_and_slide_3d, MRB_ARGS_REQ(3));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_character_velocity_3d", mrb_script_api_get_character_velocity_3d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_character_velocity_3d", mrb_script_api_set_character_velocity_3d, MRB_ARGS_REQ(3));
    mrb_define_module_function(m_MRubyState, lupine_module, "is_on_ground_3d", mrb_script_api_is_on_ground_3d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "is_on_wall_3d", mrb_script_api_is_on_wall_3d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "is_on_ceiling_3d", mrb_script_api_is_on_ceiling_3d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "get_ground_normal_3d", mrb_script_api_get_ground_normal_3d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "get_wall_normal_3d", mrb_script_api_get_wall_normal_3d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "get_character_gravity_3d", mrb_script_api_get_character_gravity_3d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_character_gravity_3d", mrb_script_api_set_character_gravity_3d, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_character_max_fall_speed_3d", mrb_script_api_get_character_max_fall_speed_3d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_character_max_fall_speed_3d", mrb_script_api_set_character_max_fall_speed_3d, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_character_max_slope_angle_3d", mrb_script_api_get_character_max_slope_angle_3d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_character_max_slope_angle_3d", mrb_script_api_set_character_max_slope_angle_3d, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_character_step_height_3d", mrb_script_api_get_character_step_height_3d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_character_step_height_3d", mrb_script_api_set_character_step_height_3d, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_character_snap_to_ground_3d", mrb_script_api_get_character_snap_to_ground_3d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_character_snap_to_ground_3d", mrb_script_api_set_character_snap_to_ground_3d, MRB_ARGS_REQ(1));

    // Lifecycle (parity)
    mrb_define_module_function(m_MRubyState, lupine_module, "queue_free_self", mrb_script_api_queue_free_self, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "free_self", mrb_script_api_free_self, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "queue_free_deferred_self", mrb_script_api_queue_free_deferred_self, MRB_ARGS_NONE());

    // Input (parity)
    mrb_define_module_function(m_MRubyState, lupine_module, "is_action_just_released", mrb_script_api_is_action_just_released, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_action_strength", mrb_script_api_get_action_strength, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_vector", mrb_script_api_get_vector, MRB_ARGS_REQ(4));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_mouse_delta", mrb_script_api_get_mouse_delta, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "get_mouse_scroll_delta", mrb_script_api_get_mouse_scroll_delta, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "is_mouse_button_just_released", mrb_script_api_is_mouse_button_just_released, MRB_ARGS_REQ(1));

    // Gamepad + vibration (parity)
    mrb_define_module_function(m_MRubyState, lupine_module, "is_gamepad_connected", mrb_script_api_is_gamepad_connected, MRB_ARGS_OPT(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_gamepad_count", mrb_script_api_get_gamepad_count, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "get_connected_gamepad_ids", mrb_script_api_get_connected_gamepad_ids, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "get_gamepad_name", mrb_script_api_get_gamepad_name, MRB_ARGS_OPT(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "is_gamepad_button_pressed", mrb_script_api_is_gamepad_button_pressed, MRB_ARGS_ARG(1, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "is_gamepad_button_just_pressed", mrb_script_api_is_gamepad_button_just_pressed, MRB_ARGS_ARG(1, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "is_gamepad_button_just_released", mrb_script_api_is_gamepad_button_just_released, MRB_ARGS_ARG(1, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_gamepad_axis", mrb_script_api_get_gamepad_axis, MRB_ARGS_ARG(1, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "set_gamepad_vibration", mrb_script_api_set_gamepad_vibration, MRB_ARGS_ARG(3, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "stop_gamepad_vibration", mrb_script_api_stop_gamepad_vibration, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "set_gamepad_deadzone", mrb_script_api_set_gamepad_deadzone, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_gamepad_deadzone", mrb_script_api_get_gamepad_deadzone, MRB_ARGS_NONE());

    // Touch input (parity)
    mrb_define_module_function(m_MRubyState, lupine_module, "is_touch_available", mrb_script_api_is_touch_available, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "is_touching", mrb_script_api_is_touching, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "get_touch_count", mrb_script_api_get_touch_count, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "get_touch_position", mrb_script_api_get_touch_position, MRB_ARGS_OPT(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "is_touch_just_started", mrb_script_api_is_touch_just_started, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "is_touch_just_ended", mrb_script_api_is_touch_just_ended, MRB_ARGS_NONE());

    // Clipboard (parity)
    mrb_define_module_function(m_MRubyState, lupine_module, "get_clipboard", mrb_script_api_get_clipboard, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_clipboard", mrb_script_api_set_clipboard, MRB_ARGS_REQ(1));

    // Active device detection
    mrb_define_module_function(m_MRubyState, lupine_module, "get_active_device_type", mrb_script_api_get_active_device_type, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "get_last_gamepad_id", mrb_script_api_get_last_gamepad_id, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "get_gamepad_type", mrb_script_api_get_gamepad_type, MRB_ARGS_OPT(1));

    // Input contexts / action sets
    mrb_define_module_function(m_MRubyState, lupine_module, "enable_input_context", mrb_script_api_enable_input_context, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "disable_input_context", mrb_script_api_disable_input_context, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "set_input_context_active", mrb_script_api_set_input_context_active, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "is_input_context_active", mrb_script_api_is_input_context_active, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "set_exclusive_input_context", mrb_script_api_set_exclusive_input_context, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_active_input_contexts", mrb_script_api_get_active_input_contexts, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_action_enabled", mrb_script_api_set_action_enabled, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "set_axis_enabled", mrb_script_api_set_axis_enabled, MRB_ARGS_REQ(2));

    // Local multiplayer player slots
    mrb_define_module_function(m_MRubyState, lupine_module, "set_player_count", mrb_script_api_set_player_count, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_player_count", mrb_script_api_get_player_count, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "clear_player_assignments", mrb_script_api_clear_player_assignments, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "assign_keyboard_mouse_to_player", mrb_script_api_assign_keyboard_mouse_to_player, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "assign_gamepad_to_player", mrb_script_api_assign_gamepad_to_player, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "unassign_gamepad", mrb_script_api_unassign_gamepad, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_player_for_gamepad", mrb_script_api_get_player_for_gamepad, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_player_for_keyboard_mouse", mrb_script_api_get_player_for_keyboard_mouse, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "player_owns_keyboard_mouse", mrb_script_api_player_owns_keyboard_mouse, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_player_gamepads", mrb_script_api_get_player_gamepads, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "set_auto_join_enabled", mrb_script_api_set_auto_join_enabled, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "is_auto_join_enabled", mrb_script_api_is_auto_join_enabled, MRB_ARGS_NONE());

    // Runtime rebinding
    mrb_define_module_function(m_MRubyState, lupine_module, "add_action_key", mrb_script_api_add_action_key, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "add_action_mouse_button", mrb_script_api_add_action_mouse_button, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "add_action_gamepad_button", mrb_script_api_add_action_gamepad_button, MRB_ARGS_ARG(2, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "add_action_gamepad_axis", mrb_script_api_add_action_gamepad_axis, MRB_ARGS_ARG(2, 2));
    mrb_define_module_function(m_MRubyState, lupine_module, "remove_action_binding", mrb_script_api_remove_action_binding, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "clear_action_bindings", mrb_script_api_clear_action_bindings, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_action_bindings", mrb_script_api_get_action_bindings, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "add_axis_key", mrb_script_api_add_axis_key, MRB_ARGS_ARG(2, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "add_axis_gamepad_axis", mrb_script_api_add_axis_gamepad_axis, MRB_ARGS_ARG(2, 2));
    mrb_define_module_function(m_MRubyState, lupine_module, "remove_axis_binding", mrb_script_api_remove_axis_binding, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "clear_axis_bindings", mrb_script_api_clear_axis_bindings, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_axis_bindings", mrb_script_api_get_axis_bindings, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "save_input_map", mrb_script_api_save_input_map, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "load_input_map", mrb_script_api_load_input_map, MRB_ARGS_REQ(1));

    // Input capture (rebind menus)
    mrb_define_module_function(m_MRubyState, lupine_module, "start_input_capture", mrb_script_api_start_input_capture, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "start_input_capture_mask", mrb_script_api_start_input_capture_mask, MRB_ARGS_REQ(3));
    mrb_define_module_function(m_MRubyState, lupine_module, "cancel_input_capture", mrb_script_api_cancel_input_capture, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "is_capturing_input", mrb_script_api_is_capturing_input, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "is_input_capture_complete", mrb_script_api_is_input_capture_complete, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "get_captured_binding", mrb_script_api_get_captured_binding, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "clear_captured_binding", mrb_script_api_clear_captured_binding, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "apply_captured_binding_to_action", mrb_script_api_apply_captured_binding_to_action, MRB_ARGS_REQ(1));

    // Glyph / prompt resolution
    mrb_define_module_function(m_MRubyState, lupine_module, "get_action_glyph", mrb_script_api_get_action_glyph, MRB_ARGS_ARG(1, 2));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_action_glyphs", mrb_script_api_get_action_glyphs, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "set_glyph_label", mrb_script_api_set_glyph_label, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "set_glyph_art", mrb_script_api_set_glyph_art, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "clear_glyph_override", mrb_script_api_clear_glyph_override, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "clear_glyph_overrides", mrb_script_api_clear_glyph_overrides, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "load_glyph_map", mrb_script_api_load_glyph_map, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "save_glyph_map", mrb_script_api_save_glyph_map, MRB_ARGS_REQ(1));

    // Action delegation
    mrb_define_module_function(m_MRubyState, lupine_module, "connect_action", mrb_script_api_connect_action, MRB_ARGS_ARG(2, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "disconnect_action", mrb_script_api_disconnect_action, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "connect_device_changed", mrb_script_api_connect_device_changed, MRB_ARGS_ARG(1, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "connect_input_captured", mrb_script_api_connect_input_captured, MRB_ARGS_ARG(1, 1));

    // Event-driven action matching (inside on_input_event)
    mrb_define_module_function(m_MRubyState, lupine_module, "event_is_action", mrb_script_api_event_is_action, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "event_is_action_pressed", mrb_script_api_event_is_action_pressed, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "event_is_action_released", mrb_script_api_event_is_action_released, MRB_ARGS_REQ(2));

    // Game state (parity)
    mrb_define_module_function(m_MRubyState, lupine_module, "set_game_paused", mrb_script_api_set_game_paused, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "is_game_paused", mrb_script_api_is_game_paused, MRB_ARGS_NONE());

    // Transform - global & 3D rotation/scale (parity)
    mrb_define_module_function(m_MRubyState, lupine_module, "get_global_position_2d", mrb_script_api_get_global_position_2d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_global_position_2d", mrb_script_api_set_global_position_2d, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_global_position_3d", mrb_script_api_get_global_position_3d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_global_position_3d", mrb_script_api_set_global_position_3d, MRB_ARGS_REQ(3));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_global_rotation_2d", mrb_script_api_get_global_rotation_2d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_global_rotation_2d", mrb_script_api_set_global_rotation_2d, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_global_rotation_3d", mrb_script_api_get_global_rotation_3d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_global_rotation_3d", mrb_script_api_set_global_rotation_3d, MRB_ARGS_REQ(3));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_global_scale_2d", mrb_script_api_get_global_scale_2d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "get_global_scale_3d", mrb_script_api_get_global_scale_3d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "get_rotation_3d", mrb_script_api_get_rotation_3d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_rotation_3d", mrb_script_api_set_rotation_3d, MRB_ARGS_REQ(3));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_scale_3d", mrb_script_api_get_scale_3d, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_scale_3d", mrb_script_api_set_scale_3d, MRB_ARGS_REQ(3));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_forward", mrb_script_api_get_forward, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "get_right", mrb_script_api_get_right, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "get_up", mrb_script_api_get_up, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "look_at_2d", mrb_script_api_look_at_2d, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "look_at_3d", mrb_script_api_look_at_3d, MRB_ARGS_REQ(3));
    mrb_define_module_function(m_MRubyState, lupine_module, "distance_to_2d", mrb_script_api_distance_to_2d, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "distance_to_3d", mrb_script_api_distance_to_3d, MRB_ARGS_REQ(3));
    mrb_define_module_function(m_MRubyState, lupine_module, "move_toward_2d", mrb_script_api_move_toward_2d, MRB_ARGS_REQ(3));
    mrb_define_module_function(m_MRubyState, lupine_module, "move_toward_3d", mrb_script_api_move_toward_3d, MRB_ARGS_REQ(4));

    // Math helpers (parity)
    mrb_define_module_function(m_MRubyState, lupine_module, "abs", mrb_script_api_abs, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "sign", mrb_script_api_sign, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "lerp", mrb_script_api_lerp, MRB_ARGS_REQ(3));
    mrb_define_module_function(m_MRubyState, lupine_module, "clamp", mrb_script_api_clamp, MRB_ARGS_REQ(3));
    mrb_define_module_function(m_MRubyState, lupine_module, "move_toward", mrb_script_api_move_toward, MRB_ARGS_REQ(3));
    mrb_define_module_function(m_MRubyState, lupine_module, "lerp_angle", mrb_script_api_lerp_angle, MRB_ARGS_REQ(3));
    mrb_define_module_function(m_MRubyState, lupine_module, "angle_difference", mrb_script_api_angle_difference, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "smoothstep", mrb_script_api_smoothstep, MRB_ARGS_REQ(3));
    mrb_define_module_function(m_MRubyState, lupine_module, "inverse_lerp", mrb_script_api_inverse_lerp, MRB_ARGS_REQ(3));
    mrb_define_module_function(m_MRubyState, lupine_module, "remap", mrb_script_api_remap, MRB_ARGS_REQ(5));
    mrb_define_module_function(m_MRubyState, lupine_module, "normalize_2d", mrb_script_api_normalize_2d, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "normalize_3d", mrb_script_api_normalize_3d, MRB_ARGS_REQ(3));
    mrb_define_module_function(m_MRubyState, lupine_module, "length_2d", mrb_script_api_length_2d, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "length_3d", mrb_script_api_length_3d, MRB_ARGS_REQ(3));
    mrb_define_module_function(m_MRubyState, lupine_module, "dot_2d", mrb_script_api_dot_2d, MRB_ARGS_REQ(4));
    mrb_define_module_function(m_MRubyState, lupine_module, "dot_3d", mrb_script_api_dot_3d, MRB_ARGS_REQ(6));
    mrb_define_module_function(m_MRubyState, lupine_module, "cross", mrb_script_api_cross, MRB_ARGS_REQ(6));

    // Random & math (parity)
    mrb_define_module_function(m_MRubyState, lupine_module, "random_float", mrb_script_api_random_float, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "random_bool", mrb_script_api_random_bool, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "random_sign", mrb_script_api_random_sign, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "random_seed", mrb_script_api_random_seed, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "deg_to_rad", mrb_script_api_deg_to_rad, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "rad_to_deg", mrb_script_api_rad_to_deg, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "wrap", mrb_script_api_wrap, MRB_ARGS_REQ(3));
    mrb_define_module_function(m_MRubyState, lupine_module, "wrap_int", mrb_script_api_wrap_int, MRB_ARGS_REQ(3));
    mrb_define_module_function(m_MRubyState, lupine_module, "ping_pong", mrb_script_api_ping_pong, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "snapped", mrb_script_api_snapped, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "is_equal_approx", mrb_script_api_is_equal_approx, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "ease", mrb_script_api_ease, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "pos_mod", mrb_script_api_pos_mod, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "pos_mod_int", mrb_script_api_pos_mod_int, MRB_ARGS_REQ(2));

    // Physics queries (parity)
    mrb_define_module_function(m_MRubyState, lupine_module, "raycast_all_2d", mrb_script_api_raycast_all_2d, MRB_ARGS_REQ(5));
    mrb_define_module_function(m_MRubyState, lupine_module, "raycast_all_3d", mrb_script_api_raycast_all_3d, MRB_ARGS_REQ(7));
    mrb_define_module_function(m_MRubyState, lupine_module, "circle_cast_2d", mrb_script_api_circle_cast_2d, MRB_ARGS_REQ(5));
    mrb_define_module_function(m_MRubyState, lupine_module, "sphere_cast_3d", mrb_script_api_sphere_cast_3d, MRB_ARGS_REQ(7));
    mrb_define_module_function(m_MRubyState, lupine_module, "overlap_circle", mrb_script_api_overlap_circle, MRB_ARGS_REQ(3));
    mrb_define_module_function(m_MRubyState, lupine_module, "overlap_rect", mrb_script_api_overlap_rect, MRB_ARGS_REQ(4));
    mrb_define_module_function(m_MRubyState, lupine_module, "overlap_sphere", mrb_script_api_overlap_sphere, MRB_ARGS_REQ(4));
    mrb_define_module_function(m_MRubyState, lupine_module, "overlap_box", mrb_script_api_overlap_box, MRB_ARGS_REQ(6));

    // Audio (parity)
    mrb_define_module_function(m_MRubyState, lupine_module, "play_audio", mrb_script_api_play_audio, MRB_ARGS_ARG(1, 3));
    mrb_define_module_function(m_MRubyState, lupine_module, "play_audio_3d", mrb_script_api_play_audio_3d, MRB_ARGS_ARG(4, 3));
    mrb_define_module_function(m_MRubyState, lupine_module, "stop_audio", mrb_script_api_stop_audio, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "pause_audio", mrb_script_api_pause_audio, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "resume_audio", mrb_script_api_resume_audio, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "set_bus_muted", mrb_script_api_set_bus_muted, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "is_bus_muted", mrb_script_api_is_bus_muted, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "play_audio_scheduled", mrb_script_api_play_audio_scheduled, MRB_ARGS_ARG(2, 3));
    mrb_define_module_function(m_MRubyState, lupine_module, "play_audio_scheduled_3d", mrb_script_api_play_audio_scheduled_3d, MRB_ARGS_ARG(5, 3));
    mrb_define_module_function(m_MRubyState, lupine_module, "is_audio_playing", mrb_script_api_is_audio_playing, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "is_audio_finished", mrb_script_api_is_audio_finished, MRB_ARGS_REQ(1));

    // Localization
    mrb_define_module_function(m_MRubyState, lupine_module, "tr", mrb_script_api_tr, MRB_ARGS_ARG(1, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "tr_fmt", mrb_script_api_tr_fmt, MRB_ARGS_ARG(1, 2));
    mrb_define_module_function(m_MRubyState, lupine_module, "tr_plural", mrb_script_api_tr_plural, MRB_ARGS_ARG(2, 2));
    mrb_define_module_function(m_MRubyState, lupine_module, "set_locale", mrb_script_api_set_locale, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_locale", mrb_script_api_get_locale, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "get_fallback_locale", mrb_script_api_get_fallback_locale, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "get_locales", mrb_script_api_get_locales, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "has_loc_key", mrb_script_api_has_loc_key, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "reload_localization", mrb_script_api_reload_localization, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_pseudolocalization", mrb_script_api_set_pseudolocalization, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "is_pseudolocalization", mrb_script_api_is_pseudolocalization, MRB_ARGS_NONE());

    // Theme
    mrb_define_module_function(m_MRubyState, lupine_module, "set_theme", mrb_script_api_set_theme, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_theme_color", mrb_script_api_get_theme_color, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_theme_constant", mrb_script_api_get_theme_constant, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "set_palette_color", mrb_script_api_set_palette_color, MRB_ARGS_ARG(4, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "set_theme_variable", mrb_script_api_set_theme_variable, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_theme_version", mrb_script_api_get_theme_version, MRB_ARGS_NONE());

    // Window / display (parity)
    mrb_define_module_function(m_MRubyState, lupine_module, "set_window_title", mrb_script_api_set_window_title, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_window_title", mrb_script_api_get_window_title, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_fullscreen", mrb_script_api_set_fullscreen, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "is_fullscreen", mrb_script_api_is_fullscreen, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_vsync", mrb_script_api_set_vsync, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "is_vsync", mrb_script_api_is_vsync, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_window_size", mrb_script_api_set_window_size, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_window_size", mrb_script_api_get_window_size, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "get_screen_size", mrb_script_api_get_screen_size, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "maximize_window", mrb_script_api_maximize_window, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "minimize_window", mrb_script_api_minimize_window, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "restore_window", mrb_script_api_restore_window, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_mouse_mode", mrb_script_api_set_mouse_mode, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_mouse_mode", mrb_script_api_get_mouse_mode, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_mouse_cursor_visible", mrb_script_api_set_mouse_cursor_visible, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "is_mouse_cursor_visible", mrb_script_api_is_mouse_cursor_visible, MRB_ARGS_NONE());
    mrb_define_const(m_MRubyState, lupine_module, "MOUSE_MODE_VISIBLE", mrb_fixnum_value(0));
    mrb_define_const(m_MRubyState, lupine_module, "MOUSE_MODE_HIDDEN", mrb_fixnum_value(1));
    mrb_define_const(m_MRubyState, lupine_module, "MOUSE_MODE_CAPTURED", mrb_fixnum_value(2));
    mrb_define_const(m_MRubyState, lupine_module, "MOUSE_MODE_CONFINED", mrb_fixnum_value(3));
    mrb_define_const(m_MRubyState, lupine_module, "MOUSE_MODE_CONFINED_HIDDEN", mrb_fixnum_value(4));

    // Screen <-> world conversion (parity)
    mrb_define_module_function(m_MRubyState, lupine_module, "screen_to_world_2d", mrb_script_api_screen_to_world_2d, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "world_to_screen_2d", mrb_script_api_world_to_screen_2d, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "screen_to_world_3d", mrb_script_api_screen_to_world_3d, MRB_ARGS_REQ(3));
    mrb_define_module_function(m_MRubyState, lupine_module, "world_to_screen_3d", mrb_script_api_world_to_screen_3d, MRB_ARGS_REQ(3));
    mrb_define_module_function(m_MRubyState, lupine_module, "screen_to_world_ray_3d", mrb_script_api_screen_to_world_ray_3d, MRB_ARGS_REQ(2));

    // Groups (parity)
    mrb_define_module_function(m_MRubyState, lupine_module, "add_to_group", mrb_script_api_add_to_group, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "remove_from_group", mrb_script_api_remove_from_group, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "is_in_group", mrb_script_api_is_in_group, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_groups", mrb_script_api_get_groups, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "get_nodes_in_group", mrb_script_api_get_nodes_in_group, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_node_count_in_group", mrb_script_api_get_node_count_in_group, MRB_ARGS_REQ(1));

    // Interfaces (capability contracts; parity with Lua/MicroPython)
    mrb_define_module_function(m_MRubyState, lupine_module, "implements_interface", mrb_script_api_implements_interface, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_implemented_interfaces", mrb_script_api_get_implemented_interfaces, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "verify_interface", mrb_script_api_verify_interface, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_nodes_with_interface", mrb_script_api_get_nodes_with_interface, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_node_count_with_interface", mrb_script_api_get_node_count_with_interface, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_first_node_with_interface", mrb_script_api_get_first_node_with_interface, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "interface_exists", mrb_script_api_interface_exists, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_all_interfaces", mrb_script_api_get_all_interfaces, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "get_interface_definition", mrb_script_api_get_interface_definition, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "register_interface", mrb_script_api_register_interface, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "archetype_implements_interface", mrb_script_api_archetype_implements_interface, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_archetypes_with_interface", mrb_script_api_get_archetypes_with_interface, MRB_ARGS_REQ(1));

    // Assets (parity)
    mrb_define_module_function(m_MRubyState, lupine_module, "load_image_asset", mrb_script_api_load_image_asset, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "load_audio_asset", mrb_script_api_load_audio_asset, MRB_ARGS_ARG(1, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "load_model_asset", mrb_script_api_load_model_asset, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "preload_assets", mrb_script_api_preload_assets, MRB_ARGS_REQ(1));

    // Scene (parity)
    mrb_define_module_function(m_MRubyState, lupine_module, "add_scene", mrb_script_api_add_scene, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "remove_scene", mrb_script_api_remove_scene, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_current_scene_path", mrb_script_api_get_current_scene_path, MRB_ARGS_NONE());

    // Node flat helpers (parity)
    mrb_define_module_function(m_MRubyState, lupine_module, "has_node", mrb_script_api_has_node, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "set_sibling_index", mrb_script_api_set_sibling_index, MRB_ARGS_REQ(1));

    // Time scale (parity)
    mrb_define_module_function(m_MRubyState, lupine_module, "set_time_scale", mrb_script_api_set_time_scale, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_time_scale", mrb_script_api_get_time_scale, MRB_ARGS_NONE());

    // Profiler (parity)
    mrb_define_module_function(m_MRubyState, lupine_module, "profiler_begin_zone", mrb_script_api_profiler_begin_zone, MRB_ARGS_ARG(1, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "profiler_end_zone", mrb_script_api_profiler_end_zone, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "profiler_set_counter", mrb_script_api_profiler_set_counter, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "profiler_is_enabled", mrb_script_api_profiler_is_enabled, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "profiler_set_enabled", mrb_script_api_profiler_set_enabled, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "profiler_frame_ms", mrb_script_api_profiler_frame_ms, MRB_ARGS_NONE());

    // Engine / OS info (parity)
    mrb_define_module_function(m_MRubyState, lupine_module, "get_fps", mrb_script_api_get_fps, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "get_ticks_msec", mrb_script_api_get_ticks_msec, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "get_unix_time", mrb_script_api_get_unix_time, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "get_platform_name", mrb_script_api_get_platform_name, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "is_debug_build", mrb_script_api_is_debug_build, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "get_dpi_scale", mrb_script_api_get_dpi_scale, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "open_url", mrb_script_api_open_url, MRB_ARGS_REQ(1));

    // Color & data sampling (parity)
    mrb_define_module_function(m_MRubyState, lupine_module, "color_from_hex", mrb_script_api_color_from_hex, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "color_to_hex", mrb_script_api_color_to_hex, MRB_ARGS_REQ(4));
    mrb_define_module_function(m_MRubyState, lupine_module, "color_from_hsv", mrb_script_api_color_from_hsv, MRB_ARGS_ARG(3, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "color_lerp", mrb_script_api_color_lerp, MRB_ARGS_REQ(9));
    mrb_define_module_function(m_MRubyState, lupine_module, "sample_gradient", mrb_script_api_sample_gradient, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "sample_curve", mrb_script_api_sample_curve, MRB_ARGS_REQ(2));

    // Audio control (parity)
    mrb_define_module_function(m_MRubyState, lupine_module, "set_audio_source_volume", mrb_script_api_set_audio_source_volume, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "set_audio_source_pitch", mrb_script_api_set_audio_source_pitch, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "set_audio_source_pan", mrb_script_api_set_audio_source_pan, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "set_master_volume", mrb_script_api_set_master_volume, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_master_volume", mrb_script_api_get_master_volume, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_master_muted", mrb_script_api_set_master_muted, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "is_master_muted", mrb_script_api_is_master_muted, MRB_ARGS_NONE());
    mrb_define_module_function(m_MRubyState, lupine_module, "set_listener_position", mrb_script_api_set_listener_position, MRB_ARGS_REQ(3));
    mrb_define_module_function(m_MRubyState, lupine_module, "set_listener_orientation", mrb_script_api_set_listener_orientation, MRB_ARGS_REQ(6));
    mrb_define_module_function(m_MRubyState, lupine_module, "set_listener_velocity", mrb_script_api_set_listener_velocity, MRB_ARGS_REQ(3));
    mrb_define_module_function(m_MRubyState, lupine_module, "create_audio_bus", mrb_script_api_create_audio_bus, MRB_ARGS_ARG(1, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "destroy_audio_bus", mrb_script_api_destroy_audio_bus, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "has_audio_bus", mrb_script_api_has_audio_bus, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "set_bus_solo", mrb_script_api_set_bus_solo, MRB_ARGS_REQ(2));
    mrb_define_module_function(m_MRubyState, lupine_module, "is_bus_solo", mrb_script_api_is_bus_solo, MRB_ARGS_REQ(1));

    // Tree utilities (parity)
    mrb_define_module_function(m_MRubyState, lupine_module, "get_first_node_in_group", mrb_script_api_get_first_node_in_group, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "get_node_or_null", mrb_script_api_get_node_or_null, MRB_ARGS_REQ(1));
    mrb_define_module_function(m_MRubyState, lupine_module, "find_children", mrb_script_api_find_children, MRB_ARGS_ARG(1, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "is_ancestor_of", mrb_script_api_is_ancestor_of, MRB_ARGS_REQ(1));

    // Debug draw (parity)
    mrb_define_module_function(m_MRubyState, lupine_module, "debug_draw_line", mrb_script_api_debug_draw_line, MRB_ARGS_ARG(10, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "debug_draw_line_2d", mrb_script_api_debug_draw_line_2d, MRB_ARGS_ARG(8, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "debug_draw_ray", mrb_script_api_debug_draw_ray, MRB_ARGS_ARG(10, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "debug_draw_box", mrb_script_api_debug_draw_box, MRB_ARGS_ARG(10, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "debug_draw_sphere", mrb_script_api_debug_draw_sphere, MRB_ARGS_ARG(8, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "debug_draw_circle", mrb_script_api_debug_draw_circle, MRB_ARGS_ARG(11, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "debug_draw_text", mrb_script_api_debug_draw_text, MRB_ARGS_ARG(8, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "debug_draw_text_2d", mrb_script_api_debug_draw_text_2d, MRB_ARGS_ARG(7, 1));

    mrb_define_module_function(m_MRubyState, lupine_module, "draw_quad", mrb_script_api_draw_quad, MRB_ARGS_ARG(9, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "draw_textured_quad", mrb_script_api_draw_textured_quad, MRB_ARGS_ARG(10, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "draw_rect", mrb_script_api_draw_rect, MRB_ARGS_ARG(8, 2));
    mrb_define_module_function(m_MRubyState, lupine_module, "draw_sprite", mrb_script_api_draw_sprite, MRB_ARGS_ARG(9, 2));
    mrb_define_module_function(m_MRubyState, lupine_module, "draw_line", mrb_script_api_draw_line, MRB_ARGS_ARG(10, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "draw_circle", mrb_script_api_draw_circle, MRB_ARGS_ARG(7, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "draw_polygon", mrb_script_api_draw_polygon, MRB_ARGS_ARG(8, 2));
    mrb_define_module_function(m_MRubyState, lupine_module, "draw_box", mrb_script_api_draw_box, MRB_ARGS_ARG(10, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "draw_rounded_rect", mrb_script_api_draw_rounded_rect, MRB_ARGS_ARG(9, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "is_drawing", mrb_script_api_is_drawing, MRB_ARGS_NONE());

    mrb_define_module_function(m_MRubyState, lupine_module, "editor_draw_line", mrb_script_api_editor_draw_line, MRB_ARGS_REQ(10));
    mrb_define_module_function(m_MRubyState, lupine_module, "editor_draw_box", mrb_script_api_editor_draw_box, MRB_ARGS_ARG(10, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "editor_draw_sphere", mrb_script_api_editor_draw_sphere, MRB_ARGS_ARG(8, 1));
    mrb_define_module_function(m_MRubyState, lupine_module, "editor_draw_circle", mrb_script_api_editor_draw_circle, MRB_ARGS_REQ(11));
    mrb_define_module_function(m_MRubyState, lupine_module, "editor_draw_rect_2d", mrb_script_api_editor_draw_rect_2d, MRB_ARGS_REQ(8));
    mrb_define_module_function(m_MRubyState, lupine_module, "editor_draw_text", mrb_script_api_editor_draw_text, MRB_ARGS_REQ(8));
    mrb_define_module_function(m_MRubyState, lupine_module, "is_editor_draw_available", mrb_script_api_is_editor_draw_available, MRB_ARGS_NONE());

    // Register base component classes for native class inheritance
    RegisterBaseComponentClasses();
}

void MRubyHost::RegisterBaseComponentClasses() {
    if (!m_MRubyState) return;

    // Define the base Component class that all engine components inherit from
    struct RClass* component_class = mrb_define_class(m_MRubyState, "Component", m_MRubyState->object_class);

    // Helper macro to define component classes
    #define DEFINE_COMPONENT_CLASS(name) \
        mrb_define_class(m_MRubyState, #name, component_class)

    // 2D Rendering Components
    DEFINE_COMPONENT_CLASS(Sprite2D);
    DEFINE_COMPONENT_CLASS(Particles2D);
    DEFINE_COMPONENT_CLASS(AnimatedSprite2D);
    DEFINE_COMPONENT_CLASS(GifPlayer);
    DEFINE_COMPONENT_CLASS(VideoPlayer);
    DEFINE_COMPONENT_CLASS(ColorRect);
    DEFINE_COMPONENT_CLASS(Image2D);
    DEFINE_COMPONENT_CLASS(NineSlicePanel);
    DEFINE_COMPONENT_CLASS(Shape2D);
    DEFINE_COMPONENT_CLASS(Line2D);
    DEFINE_COMPONENT_CLASS(Curve2D);
    DEFINE_COMPONENT_CLASS(Light2D);
    DEFINE_COMPONENT_CLASS(LightOccluder2D);
    DEFINE_COMPONENT_CLASS(VectorGraphic2D);
    DEFINE_COMPONENT_CLASS(Empty2D);

    // 3D Rendering Components
    DEFINE_COMPONENT_CLASS(Sprite3D);
    DEFINE_COMPONENT_CLASS(Particles3D);
    DEFINE_COMPONENT_CLASS(AnimatedSprite3D);
    DEFINE_COMPONENT_CLASS(StaticMesh3D);
    DEFINE_COMPONENT_CLASS(SkeletalMesh3D);
    DEFINE_COMPONENT_CLASS(PrimitiveMesh3D);
    DEFINE_COMPONENT_CLASS(Label3D);
    DEFINE_COMPONENT_CLASS(Panel3D);
    DEFINE_COMPONENT_CLASS(Button3D);
    DEFINE_COMPONENT_CLASS(ProgressBar3D);
    DEFINE_COMPONENT_CLASS(Curve3D);
    DEFINE_COMPONENT_CLASS(Path3D);
    DEFINE_COMPONENT_CLASS(PathFollow3D);
    DEFINE_COMPONENT_CLASS(Empty3D);

    // UI Components
    DEFINE_COMPONENT_CLASS(Button);
    DEFINE_COMPONENT_CLASS(TextureButton);
    DEFINE_COMPONENT_CLASS(ToggleButton);
    DEFINE_COMPONENT_CLASS(Checkbox);
    DEFINE_COMPONENT_CLASS(RadioButton);
    DEFINE_COMPONENT_CLASS(Label);
    DEFINE_COMPONENT_CLASS(ProgressBar);
    DEFINE_COMPONENT_CLASS(Slider);
    DEFINE_COMPONENT_CLASS(LineEdit);
    DEFINE_COMPONENT_CLASS(SpinBox);
    DEFINE_COMPONENT_CLASS(TextEdit);
    DEFINE_COMPONENT_CLASS(ItemList);
    DEFINE_COMPONENT_CLASS(Dropdown);
    DEFINE_COMPONENT_CLASS(PopupMenu);
    DEFINE_COMPONENT_CLASS(RichTextLabel);
    DEFINE_COMPONENT_CLASS(Tree);
    DEFINE_COMPONENT_CLASS(Panel);
    DEFINE_COMPONENT_CLASS(Container);
    DEFINE_COMPONENT_CLASS(HorizontalContainer);
    DEFINE_COMPONENT_CLASS(VerticalContainer);
    DEFINE_COMPONENT_CLASS(GridContainer);
    DEFINE_COMPONENT_CLASS(PaddingContainer);
    DEFINE_COMPONENT_CLASS(CenterContainer);
    DEFINE_COMPONENT_CLASS(DockContainer);
    DEFINE_COMPONENT_CLASS(Stack);
    DEFINE_COMPONENT_CLASS(Wrap);
    DEFINE_COMPONENT_CLASS(LayoutSlot);
    DEFINE_COMPONENT_CLASS(SplitContainer);
    DEFINE_COMPONENT_CLASS(AspectRatioContainer);
    DEFINE_COMPONENT_CLASS(ScrollContainer);
    DEFINE_COMPONENT_CLASS(TabContainer);

    // Audio Components
    DEFINE_COMPONENT_CLASS(AudioPlayer);
    DEFINE_COMPONENT_CLASS(AudioListener);

    // Physics 2D Components
    DEFINE_COMPONENT_CLASS(RigidBody2DComponent);
    DEFINE_COMPONENT_CLASS(StaticBody2DComponent);
    DEFINE_COMPONENT_CLASS(KinematicBody2DComponent);
    DEFINE_COMPONENT_CLASS(AreaTrigger2DComponent);
    DEFINE_COMPONENT_CLASS(CollisionBody2DComponent);
    DEFINE_COMPONENT_CLASS(CharacterController2DComponent);
    DEFINE_COMPONENT_CLASS(RayCast2D);
    DEFINE_COMPONENT_CLASS(ShapeCast2D);

    // Physics 3D Components
    DEFINE_COMPONENT_CLASS(RigidBody3DComponent);
    DEFINE_COMPONENT_CLASS(StaticBody3DComponent);
    DEFINE_COMPONENT_CLASS(KinematicBody3DComponent);
    DEFINE_COMPONENT_CLASS(AreaTrigger3DComponent);
    DEFINE_COMPONENT_CLASS(CharacterController3DComponent);
    DEFINE_COMPONENT_CLASS(RayCast3D);
    DEFINE_COMPONENT_CLASS(ShapeCast3D);

    // Lighting Components
    DEFINE_COMPONENT_CLASS(DirectionalLight3D);
    DEFINE_COMPONENT_CLASS(OmniLight3D);
    DEFINE_COMPONENT_CLASS(SpotLight3D);

    // Utility Components
    DEFINE_COMPONENT_CLASS(Timer);
    DEFINE_COMPONENT_CLASS(AnimationPlayer);
    DEFINE_COMPONENT_CLASS(AnimationTree);
    DEFINE_COMPONENT_CLASS(SubViewport);
    DEFINE_COMPONENT_CLASS(CameraEffectColorGrade);
    DEFINE_COMPONENT_CLASS(CameraEffectTonemap);
    DEFINE_COMPONENT_CLASS(CameraEffectVignette);
    DEFINE_COMPONENT_CLASS(CameraEffectFilmGrain);
    DEFINE_COMPONENT_CLASS(CameraEffectColorInvert);
    DEFINE_COMPONENT_CLASS(CameraEffectPosterize);
    DEFINE_COMPONENT_CLASS(CameraEffectHueShift);
    DEFINE_COMPONENT_CLASS(CameraEffectBlur);
    DEFINE_COMPONENT_CLASS(CameraEffectGlow);
    DEFINE_COMPONENT_CLASS(CameraEffectOutline);
    DEFINE_COMPONENT_CLASS(CameraEffectPixelate);
    DEFINE_COMPONENT_CLASS(CameraEffectSharpen);
    DEFINE_COMPONENT_CLASS(CameraEffectChromaticAberration);
    DEFINE_COMPONENT_CLASS(WorldEnvironment);
    DEFINE_COMPONENT_CLASS(Camera2D);
    DEFINE_COMPONENT_CLASS(Camera3D);
    DEFINE_COMPONENT_CLASS(TileMap2D);
    DEFINE_COMPONENT_CLASS(NavigationRegion2D);
    DEFINE_COMPONENT_CLASS(NavigationAgent2D);
    DEFINE_COMPONENT_CLASS(NavigationObstacle2D);
    DEFINE_COMPONENT_CLASS(NavigationRegion3D);
    DEFINE_COMPONENT_CLASS(NavigationAgent3D);
    DEFINE_COMPONENT_CLASS(NavigationObstacle3D);
    DEFINE_COMPONENT_CLASS(NetworkObject);
    DEFINE_COMPONENT_CLASS(NetworkSynchronizer);
    DEFINE_COMPONENT_CLASS(NetworkTransform2D);
    DEFINE_COMPONENT_CLASS(NetworkTransform3D);
    DEFINE_COMPONENT_CLASS(NetworkSpawner);
    DEFINE_COMPONENT_CLASS(NetworkController);
    DEFINE_COMPONENT_CLASS(NetworkAnimator);
    DEFINE_COMPONENT_CLASS(NetworkRigidBody2D);
    DEFINE_COMPONENT_CLASS(NetworkRigidBody3D);
    DEFINE_COMPONENT_CLASS(ParticleEmitter2D);
    DEFINE_COMPONENT_CLASS(ParticleEmitter3D);
    DEFINE_COMPONENT_CLASS(YSort);
    DEFINE_COMPONENT_CLASS(ParallaxBackground);
    DEFINE_COMPONENT_CLASS(ParallaxLayer);

    #undef DEFINE_COMPONENT_CLASS
}

struct RClass* MRubyHost::EnsureCustomComponentClass(const std::string& className,
                                                     std::set<std::string>& visiting) {
    if (!m_MRubyState || className.empty()) {
        return nullptr;
    }

    // Already a class in this VM: a built-in component stub, or a custom class defined
    // by an earlier pass. Either way it is the base to inherit from.
    if (mrb_class_defined(m_MRubyState, className.c_str())) {
        return mrb_class_get(m_MRubyState, className.c_str());
    }

    const core::CustomComponentDefinition* def =
        core::CustomComponentRegistry::GetInstance().GetDefinition(className);
    if (!def || !def->isValid) {
        return nullptr;  // not a component type we know about
    }

    if (!visiting.insert(className).second) {
        return nullptr;  // cycle; the registry should already have broken it
    }

    // Define the base first so the chain is built root-downwards and each custom class
    // inherits from its real base rather than a flattened Component.
    struct RClass* base = nullptr;
    if (!def->baseComponentType.empty()) {
        base = EnsureCustomComponentClass(def->baseComponentType, visiting);
    }
    if (!base) {
        base = mrb_class_defined(m_MRubyState, "Component")
                   ? mrb_class_get(m_MRubyState, "Component")
                   : m_MRubyState->object_class;
    }

    visiting.erase(className);

    return mrb_define_class(m_MRubyState, className.c_str(), base);
}

void MRubyHost::RegisterCustomComponentTypes() {
    if (!m_MRubyState || !m_Initialized) return;

    // Without this a chain such as SimObject(Sprite2D) -> SimBoulder(SimObject) dies
    // when the script is executed: SimObject is not a constant in the VM, so
    // `class SimBoulder < SimObject` raises and the component script fails to load.
    for (const core::CustomComponentDefinition& def :
         core::CustomComponentRegistry::GetInstance().GetDefinitions()) {
        if (!def.isValid || def.className.empty()) {
            continue;
        }
        std::set<std::string> visiting;
        EnsureCustomComponentClass(def.className, visiting);
    }
}

#endif

}
}

