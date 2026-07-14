/**
 * MicroPython Environment Implementation
 *
 * Provides embedded Python scripting using MicroPython.
 * This compiles directly into the executable with no external dependencies.
 *
 * The lupine module provides access to all engine APIs:
 *   import lupine
 *   lupine.translate_2d(100, 0)
 */

#include "lupine/scripting/MicroPythonEnvironment.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/platform/FileSystem.hpp"
#include "lupine/platform/PackFile.hpp"
#include <iostream>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
#include <sstream>
#include <unordered_map>

#ifdef LUPINE_HAS_MICROPYTHON

#if defined(_MSC_VER)
// MicroPython's C headers define static/inline helpers with unreferenced parameters
// (e.g. py/pystack.h, py/bc.h). We don't control them, so silence C4100 across the
// third-party include block only.
#pragma warning(push)
#pragma warning(disable : 4100)
#endif
extern "C" {
#include "port/micropython_embed.h"
#include "py/runtime.h"
#include "py/gc.h"
#include "py/mphal.h"
#include "py/compile.h"
#include "py/objstr.h"
#include "py/obj.h"
#include "py/objmodule.h"
#include "py/stackctrl.h"
#include "py/builtin.h"
#include "py/qstr.h"
#include "py/mpstate.h"

// Safe qstr lookup - just call qstr_from_str directly
// Note: Previously had a workaround for empty qstr pools, but that tried to write
// to static/const pools which caused access violations. Static pools are read-only.
static qstr safe_qstr_from_str(const char* str) {
    return qstr_from_str(str);
}
}
#if defined(_MSC_VER)
#pragma warning(pop)

// MicroPython's NLR unwinding is setjmp/longjmp based, which MSVC flags (C4611)
// wherever a function that uses it also has C++ objects with destructors in scope.
// Every nlr_push scope in this binding layer holds only POD or manually-managed
// locals, so an NLR long-jump has nothing to destroy. There are ~19 identical such
// scopes; disable the warning for the whole translation unit rather than wrap each.
#pragma warning(disable : 4611)
#endif

// MicroPython uses STATIC as static - define it if not already
#ifndef STATIC
#define STATIC static
#endif

// MicroPython's MP_DEFINE_CONST_OBJ_TYPE_NARGS_* macros drop each slot function
// pointer straight into the type's `const void *slots[]` initializer. C (and
// MSVC as an extension) implicitly convert a function pointer to void*, but a
// conforming C++ compiler such as Clang/Emscripten does not, so the stock
// macros fail to compile here. Re-define them to cast every slot value to
// `(const void *)` - the very same cast MicroPython's own runtime
// MP_OBJ_TYPE_SET_SLOT already performs - so the type tables build under Clang
// while remaining byte-for-byte identical to the upstream layout.
#undef MP_DEFINE_CONST_OBJ_TYPE_NARGS_1
#undef MP_DEFINE_CONST_OBJ_TYPE_NARGS_2
#undef MP_DEFINE_CONST_OBJ_TYPE_NARGS_3
#undef MP_DEFINE_CONST_OBJ_TYPE_NARGS_4
#undef MP_DEFINE_CONST_OBJ_TYPE_NARGS_5
#undef MP_DEFINE_CONST_OBJ_TYPE_NARGS_6
#undef MP_DEFINE_CONST_OBJ_TYPE_NARGS_7
#undef MP_DEFINE_CONST_OBJ_TYPE_NARGS_8
#undef MP_DEFINE_CONST_OBJ_TYPE_NARGS_9
#undef MP_DEFINE_CONST_OBJ_TYPE_NARGS_10
#undef MP_DEFINE_CONST_OBJ_TYPE_NARGS_11
#undef MP_DEFINE_CONST_OBJ_TYPE_NARGS_12
#define MP_DEFINE_CONST_OBJ_TYPE_NARGS_1(_struct_type, _typename, _name, _flags, f1, v1) const _struct_type _typename = { .base = { &mp_type_type }, .flags = _flags, .name = _name, .slot_index_##f1 = 1, .slots = { (const void *)v1, } }
#define MP_DEFINE_CONST_OBJ_TYPE_NARGS_2(_struct_type, _typename, _name, _flags, f1, v1, f2, v2) const _struct_type _typename = { .base = { &mp_type_type }, .flags = _flags, .name = _name, .slot_index_##f1 = 1, .slot_index_##f2 = 2, .slots = { (const void *)v1, (const void *)v2, } }
#define MP_DEFINE_CONST_OBJ_TYPE_NARGS_3(_struct_type, _typename, _name, _flags, f1, v1, f2, v2, f3, v3) const _struct_type _typename = { .base = { &mp_type_type }, .flags = _flags, .name = _name, .slot_index_##f1 = 1, .slot_index_##f2 = 2, .slot_index_##f3 = 3, .slots = { (const void *)v1, (const void *)v2, (const void *)v3, } }
#define MP_DEFINE_CONST_OBJ_TYPE_NARGS_4(_struct_type, _typename, _name, _flags, f1, v1, f2, v2, f3, v3, f4, v4) const _struct_type _typename = { .base = { &mp_type_type }, .flags = _flags, .name = _name, .slot_index_##f1 = 1, .slot_index_##f2 = 2, .slot_index_##f3 = 3, .slot_index_##f4 = 4, .slots = { (const void *)v1, (const void *)v2, (const void *)v3, (const void *)v4, } }
#define MP_DEFINE_CONST_OBJ_TYPE_NARGS_5(_struct_type, _typename, _name, _flags, f1, v1, f2, v2, f3, v3, f4, v4, f5, v5) const _struct_type _typename = { .base = { &mp_type_type }, .flags = _flags, .name = _name, .slot_index_##f1 = 1, .slot_index_##f2 = 2, .slot_index_##f3 = 3, .slot_index_##f4 = 4, .slot_index_##f5 = 5, .slots = { (const void *)v1, (const void *)v2, (const void *)v3, (const void *)v4, (const void *)v5, } }
#define MP_DEFINE_CONST_OBJ_TYPE_NARGS_6(_struct_type, _typename, _name, _flags, f1, v1, f2, v2, f3, v3, f4, v4, f5, v5, f6, v6) const _struct_type _typename = { .base = { &mp_type_type }, .flags = _flags, .name = _name, .slot_index_##f1 = 1, .slot_index_##f2 = 2, .slot_index_##f3 = 3, .slot_index_##f4 = 4, .slot_index_##f5 = 5, .slot_index_##f6 = 6, .slots = { (const void *)v1, (const void *)v2, (const void *)v3, (const void *)v4, (const void *)v5, (const void *)v6, } }
#define MP_DEFINE_CONST_OBJ_TYPE_NARGS_7(_struct_type, _typename, _name, _flags, f1, v1, f2, v2, f3, v3, f4, v4, f5, v5, f6, v6, f7, v7) const _struct_type _typename = { .base = { &mp_type_type }, .flags = _flags, .name = _name, .slot_index_##f1 = 1, .slot_index_##f2 = 2, .slot_index_##f3 = 3, .slot_index_##f4 = 4, .slot_index_##f5 = 5, .slot_index_##f6 = 6, .slot_index_##f7 = 7, .slots = { (const void *)v1, (const void *)v2, (const void *)v3, (const void *)v4, (const void *)v5, (const void *)v6, (const void *)v7, } }
#define MP_DEFINE_CONST_OBJ_TYPE_NARGS_8(_struct_type, _typename, _name, _flags, f1, v1, f2, v2, f3, v3, f4, v4, f5, v5, f6, v6, f7, v7, f8, v8) const _struct_type _typename = { .base = { &mp_type_type }, .flags = _flags, .name = _name, .slot_index_##f1 = 1, .slot_index_##f2 = 2, .slot_index_##f3 = 3, .slot_index_##f4 = 4, .slot_index_##f5 = 5, .slot_index_##f6 = 6, .slot_index_##f7 = 7, .slot_index_##f8 = 8, .slots = { (const void *)v1, (const void *)v2, (const void *)v3, (const void *)v4, (const void *)v5, (const void *)v6, (const void *)v7, (const void *)v8, } }
#define MP_DEFINE_CONST_OBJ_TYPE_NARGS_9(_struct_type, _typename, _name, _flags, f1, v1, f2, v2, f3, v3, f4, v4, f5, v5, f6, v6, f7, v7, f8, v8, f9, v9) const _struct_type _typename = { .base = { &mp_type_type }, .flags = _flags, .name = _name, .slot_index_##f1 = 1, .slot_index_##f2 = 2, .slot_index_##f3 = 3, .slot_index_##f4 = 4, .slot_index_##f5 = 5, .slot_index_##f6 = 6, .slot_index_##f7 = 7, .slot_index_##f8 = 8, .slot_index_##f9 = 9, .slots = { (const void *)v1, (const void *)v2, (const void *)v3, (const void *)v4, (const void *)v5, (const void *)v6, (const void *)v7, (const void *)v8, (const void *)v9, } }
#define MP_DEFINE_CONST_OBJ_TYPE_NARGS_10(_struct_type, _typename, _name, _flags, f1, v1, f2, v2, f3, v3, f4, v4, f5, v5, f6, v6, f7, v7, f8, v8, f9, v9, f10, v10) const _struct_type _typename = { .base = { &mp_type_type }, .flags = _flags, .name = _name, .slot_index_##f1 = 1, .slot_index_##f2 = 2, .slot_index_##f3 = 3, .slot_index_##f4 = 4, .slot_index_##f5 = 5, .slot_index_##f6 = 6, .slot_index_##f7 = 7, .slot_index_##f8 = 8, .slot_index_##f9 = 9, .slot_index_##f10 = 10, .slots = { (const void *)v1, (const void *)v2, (const void *)v3, (const void *)v4, (const void *)v5, (const void *)v6, (const void *)v7, (const void *)v8, (const void *)v9, (const void *)v10, } }
#define MP_DEFINE_CONST_OBJ_TYPE_NARGS_11(_struct_type, _typename, _name, _flags, f1, v1, f2, v2, f3, v3, f4, v4, f5, v5, f6, v6, f7, v7, f8, v8, f9, v9, f10, v10, f11, v11) const _struct_type _typename = { .base = { &mp_type_type }, .flags = _flags, .name = _name, .slot_index_##f1 = 1, .slot_index_##f2 = 2, .slot_index_##f3 = 3, .slot_index_##f4 = 4, .slot_index_##f5 = 5, .slot_index_##f6 = 6, .slot_index_##f7 = 7, .slot_index_##f8 = 8, .slot_index_##f9 = 9, .slot_index_##f10 = 10, .slot_index_##f11 = 11, .slots = { (const void *)v1, (const void *)v2, (const void *)v3, (const void *)v4, (const void *)v5, (const void *)v6, (const void *)v7, (const void *)v8, (const void *)v9, (const void *)v10, (const void *)v11, } }
#define MP_DEFINE_CONST_OBJ_TYPE_NARGS_12(_struct_type, _typename, _name, _flags, f1, v1, f2, v2, f3, v3, f4, v4, f5, v5, f6, v6, f7, v7, f8, v8, f9, v9, f10, v10, f11, v11, f12, v12) const _struct_type _typename = { .base = { &mp_type_type }, .flags = _flags, .name = _name, .slot_index_##f1 = 1, .slot_index_##f2 = 2, .slot_index_##f3 = 3, .slot_index_##f4 = 4, .slot_index_##f5 = 5, .slot_index_##f6 = 6, .slot_index_##f7 = 7, .slot_index_##f8 = 8, .slot_index_##f9 = 9, .slot_index_##f10 = 10, .slot_index_##f11 = 11, .slot_index_##f12 = 12, .slots = { (const void *)v1, (const void *)v2, (const void *)v3, (const void *)v4, (const void *)v5, (const void *)v6, (const void *)v7, (const void *)v8, (const void *)v9, (const void *)v10, (const void *)v11, (const void *)v12, } }

#include "lupine/asset/ArchetypeInstance.hpp"
#include "lupine/core/ArchetypeRuntime.hpp"
#include "lupine/scripting/ScriptAPI.hpp"
#include "lupine/scripting/NodeRef.hpp"
#include "lupine/network/NetworkManager.hpp"
#include "lupine/profiling/Profiler.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/CameraNodes.hpp"
#include "lupine/core/Component.hpp"
#include "lupine/core/CustomComponentRegistry.hpp"
#include "lupine/components/UIControl.hpp"
#include "lupine/components/Particles2D.hpp"
#include "lupine/components/Particles3D.hpp"
#include <nlohmann/json.hpp>

namespace lupine {
namespace scripting {

// The conservative GC scans the C stack from the collector's own frame *up to*
// MP_STATE_THREAD(stack_top) (see shared/runtime/gchelper_*.c), so stack_top must
// always be the highest (outermost) address of any C frame that can hold a live
// mp_obj_t. Dispatch re-enters this layer (script A emits a directly-connected
// signal into script B), so a marker taken in the inner frame is *lower* than the
// outer one: adopting it unconditionally would shrink the scan window below the
// outer frames and let the GC sweep objects that are still live there - and, once
// the inner frame returns, make stack_top point below the stack pointer, whose
// unsigned subtraction underflows into a wild scan length. This scope therefore
// only raises stack_top and always restores the previous value on exit.
static int s_MpStackDepth = 0;

struct MpStackScope {
    char* previousTop;

    explicit MpStackScope(void* marker) {
        previousTop = MP_STATE_THREAD(stack_top);
        const bool outermost = (s_MpStackDepth == 0);
        if (outermost || previousTop == nullptr || static_cast<char*>(marker) > previousTop) {
            mp_stack_set_top(marker);
        }
        ++s_MpStackDepth;
        mp_stack_set_limit(40000);
    }

    ~MpStackScope() {
        --s_MpStackDepth;
        MP_STATE_THREAD(stack_top) = previousTop;
    }

    MpStackScope(const MpStackScope&) = delete;
    MpStackScope& operator=(const MpStackScope&) = delete;
};

// Helper macro to properly set up stack for MicroPython operations
#define MP_SETUP_STACK() \
    int _mp_stack_marker = 0; \
    MpStackScope _mp_stack_scope(&_mp_stack_marker)

// Static output buffer for capturing MicroPython print output
static std::string s_OutputBuffer;
static MicroPythonEnvironment* s_CurrentEnv = nullptr;

// Live script instances by id. The coroutine scheduler stores the *id* of the
// instance that started each coroutine (never the pointer): the instance can be
// destroyed while its coroutine is still parked in the scheduler, and the id then
// simply fails to resolve, which is how a dead instance's coroutines are retired.
static std::unordered_map<int, MicroPythonEnvironment*> s_EnvById;
static int s_NextEnvId = 1;
static std::vector<MicroPythonEnvironment*> s_EnvActivationStack;

static MicroPythonEnvironment* FindEnvById(int id) {
    std::unordered_map<int, MicroPythonEnvironment*>::iterator it = s_EnvById.find(id);
    return (it == s_EnvById.end()) ? nullptr : it->second;
}

// HAL implementations
extern "C" {

void mp_hal_stdout_tx_strn_cooked(const char* str, size_t len) {
    s_OutputBuffer.append(str, len);
    std::string line(str, len);
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
        line.pop_back();
    }
    if (!line.empty()) {
        
    }
}

int mp_hal_stdin_rx_chr(void) {
    return -1;
}

mp_uint_t mp_hal_ticks_ms(void) {
    return static_cast<mp_uint_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );
}

mp_uint_t mp_hal_ticks_us(void) {
    return static_cast<mp_uint_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );
}

mp_uint_t mp_hal_ticks_cpu(void) {
    return mp_hal_ticks_us();
}

void mp_hal_delay_ms(mp_uint_t ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

void mp_hal_delay_us(mp_uint_t us) {
    std::this_thread::sleep_for(std::chrono::microseconds(us));
}

} // extern "C"

// ============================================================================
// Helper to get ScriptAPI from current environment
// ============================================================================

static ScriptAPI* GetCurrentScriptAPI() {
    return s_CurrentEnv ? s_CurrentEnv->GetScriptAPI() : nullptr;
}

// ============================================================================
// Helper to create Vec2 list [x, y]
// ============================================================================

static mp_obj_t make_vec2(float x, float y) {
    mp_obj_t items[2];
    items[0] = mp_obj_new_float(x);
    items[1] = mp_obj_new_float(y);
    return mp_obj_new_list(2, items);
}

// ============================================================================
// Helper to create Vec3 list [x, y, z]
// ============================================================================

static mp_obj_t make_vec3(float x, float y, float z) {
    mp_obj_t items[3];
    items[0] = mp_obj_new_float(x);
    items[1] = mp_obj_new_float(y);
    items[2] = mp_obj_new_float(z);
    return mp_obj_new_list(3, items);
}

// ============================================================================
// Helper to create Color list [r, g, b, a]
// ============================================================================

static mp_obj_t make_color(float r, float g, float b, float a) {
    mp_obj_t items[4];
    items[0] = mp_obj_new_float(r); items[1] = mp_obj_new_float(g);
    items[2] = mp_obj_new_float(b); items[3] = mp_obj_new_float(a);
    return mp_obj_new_list(4, items);
}

// ============================================================================
// Helper to extract Vec2 from list/tuple
// ============================================================================

static bool get_vec2(mp_obj_t obj, float& x, float& y) {
    if (mp_obj_is_type(obj, &mp_type_list) || mp_obj_is_type(obj, &mp_type_tuple)) {
        size_t len;
        mp_obj_t* items;
        mp_obj_get_array(obj, &len, &items);
        if (len >= 2) {
            x = mp_obj_get_float(items[0]);
            y = mp_obj_get_float(items[1]);
            return true;
        }
    }
    return false;
}

// ============================================================================
// Helper to extract Vec3 from list/tuple
// ============================================================================

static bool get_vec3(mp_obj_t obj, float& x, float& y, float& z) {
    if (mp_obj_is_type(obj, &mp_type_list) || mp_obj_is_type(obj, &mp_type_tuple)) {
        size_t len;
        mp_obj_t* items;
        mp_obj_get_array(obj, &len, &items);
        if (len >= 3) {
            x = mp_obj_get_float(items[0]);
            y = mp_obj_get_float(items[1]);
            z = mp_obj_get_float(items[2]);
            return true;
        }
    }
    return false;
}

// ============================================================================
// LOGGING FUNCTIONS
// ============================================================================

STATIC mp_obj_t lupine_log_info(mp_obj_t msg_obj) {
    const char* msg = mp_obj_str_get_str(msg_obj);
    LOG_INFO(LogCategory::Scripting, "[Script] {}", msg);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_log_info_obj, lupine_log_info);

STATIC mp_obj_t lupine_log_warning(mp_obj_t msg_obj) {
    const char* msg = mp_obj_str_get_str(msg_obj);
    LOG_WARN(LogCategory::Scripting, "[Script] {}", msg);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_log_warning_obj, lupine_log_warning);

STATIC mp_obj_t lupine_log_error(mp_obj_t msg_obj) {
    const char* msg = mp_obj_str_get_str(msg_obj);
    LOG_ERROR(LogCategory::Scripting, "[Script] {}", msg);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_log_error_obj, lupine_log_error);

STATIC mp_obj_t lupine_log_debug(mp_obj_t msg_obj) {
    const char* msg = mp_obj_str_get_str(msg_obj);
    LOG_DEBUG(LogCategory::Scripting, "[Script] {}", msg);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_log_debug_obj, lupine_log_debug);

// ============================================================================
// GAME STATE FUNCTIONS
// ============================================================================

STATIC mp_obj_t lupine_get_delta_time(void) {
    auto* api = GetCurrentScriptAPI();
    return mp_obj_new_float(api ? api->GetDeltaTime() : 0.016f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_delta_time_obj, lupine_get_delta_time);

STATIC mp_obj_t lupine_set_game_paused(mp_obj_t paused_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        api->SetGamePaused(mp_obj_is_true(paused_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_game_paused_obj, lupine_set_game_paused);

STATIC mp_obj_t lupine_is_game_paused(void) {
    auto* api = GetCurrentScriptAPI();
    return (api && api->IsGamePaused()) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_is_game_paused_obj, lupine_is_game_paused);

STATIC mp_obj_t lupine_set_time_scale(mp_obj_t scale_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->SetTimeScale(mp_obj_get_float(scale_obj));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_time_scale_obj, lupine_set_time_scale);

STATIC mp_obj_t lupine_get_time_scale(void) {
    auto* api = GetCurrentScriptAPI();
    return mp_obj_new_float(api ? api->GetTimeScale() : 1.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_time_scale_obj, lupine_get_time_scale);

// ============================================================================
// INPUT - ACTION FUNCTIONS
// ============================================================================

STATIC mp_obj_t lupine_is_action_pressed(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api) return mp_const_false;
    int player = (n_args > 1) ? mp_obj_get_int(args[1]) : -1;
    return api->IsActionPressed(mp_obj_str_get_str(args[0]), player) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_is_action_pressed_obj, 1, 2, lupine_is_action_pressed);

STATIC mp_obj_t lupine_is_action_just_pressed(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api) return mp_const_false;
    int player = (n_args > 1) ? mp_obj_get_int(args[1]) : -1;
    return api->IsActionJustPressed(mp_obj_str_get_str(args[0]), player) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_is_action_just_pressed_obj, 1, 2, lupine_is_action_just_pressed);

STATIC mp_obj_t lupine_is_action_just_released(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api) return mp_const_false;
    int player = (n_args > 1) ? mp_obj_get_int(args[1]) : -1;
    return api->IsActionJustReleased(mp_obj_str_get_str(args[0]), player) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_is_action_just_released_obj, 1, 2, lupine_is_action_just_released);

STATIC mp_obj_t lupine_get_action_strength(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api) return mp_obj_new_float(0.0f);
    int player = (n_args > 1) ? mp_obj_get_int(args[1]) : -1;
    return mp_obj_new_float(api->GetActionStrength(mp_obj_str_get_str(args[0]), player));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_get_action_strength_obj, 1, 2, lupine_get_action_strength);

STATIC mp_obj_t lupine_get_axis(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api) return mp_obj_new_float(0.0f);
    int player = (n_args > 1) ? mp_obj_get_int(args[1]) : -1;
    return mp_obj_new_float(api->GetAxis(mp_obj_str_get_str(args[0]), player));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_get_axis_obj, 1, 2, lupine_get_axis);

STATIC mp_obj_t lupine_get_vector(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 4) return make_vec2(0.0f, 0.0f);

    const char* negX = mp_obj_str_get_str(args[0]);
    const char* posX = mp_obj_str_get_str(args[1]);
    const char* negY = mp_obj_str_get_str(args[2]);
    const char* posY = mp_obj_str_get_str(args[3]);
    int player = (n_args > 4) ? mp_obj_get_int(args[4]) : -1;

    auto vec = api->GetVector(negX, posX, negY, posY, player);
    return make_vec2(vec.x, vec.y);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_get_vector_obj, 4, 5, lupine_get_vector);

// ============================================================================
// INPUT - KEYBOARD FUNCTIONS
// ============================================================================

STATIC mp_obj_t lupine_is_key_pressed(mp_obj_t key_obj) {
    auto* api = GetCurrentScriptAPI();
    if (!api) return mp_const_false;
    int key = mp_obj_get_int(key_obj);
    return api->IsKeyPressed(key) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_is_key_pressed_obj, lupine_is_key_pressed);

STATIC mp_obj_t lupine_is_key_just_pressed(mp_obj_t key_obj) {
    auto* api = GetCurrentScriptAPI();
    if (!api) return mp_const_false;
    int key = mp_obj_get_int(key_obj);
    return api->IsKeyJustPressed(key) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_is_key_just_pressed_obj, lupine_is_key_just_pressed);

STATIC mp_obj_t lupine_is_key_just_released(mp_obj_t key_obj) {
    auto* api = GetCurrentScriptAPI();
    if (!api) return mp_const_false;
    int key = mp_obj_get_int(key_obj);
    return api->IsKeyJustReleased(key) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_is_key_just_released_obj, lupine_is_key_just_released);

// ============================================================================
// INPUT - MOUSE FUNCTIONS
// ============================================================================

STATIC mp_obj_t lupine_is_mouse_button_pressed(mp_obj_t button_obj) {
    auto* api = GetCurrentScriptAPI();
    if (!api) return mp_const_false;
    int button = mp_obj_get_int(button_obj);
    return api->IsMouseButtonPressed(button) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_is_mouse_button_pressed_obj, lupine_is_mouse_button_pressed);

STATIC mp_obj_t lupine_is_mouse_button_just_pressed(mp_obj_t button_obj) {
    auto* api = GetCurrentScriptAPI();
    if (!api) return mp_const_false;
    int button = mp_obj_get_int(button_obj);
    return api->IsMouseButtonJustPressed(button) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_is_mouse_button_just_pressed_obj, lupine_is_mouse_button_just_pressed);

STATIC mp_obj_t lupine_is_mouse_button_just_released(mp_obj_t button_obj) {
    auto* api = GetCurrentScriptAPI();
    if (!api) return mp_const_false;
    int button = mp_obj_get_int(button_obj);
    return api->IsMouseButtonJustReleased(button) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_is_mouse_button_just_released_obj, lupine_is_mouse_button_just_released);

STATIC mp_obj_t lupine_get_mouse_position(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        auto pos = api->GetMousePosition();
        return make_vec2(pos.x, pos.y);
    }
    return make_vec2(0.0f, 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_mouse_position_obj, lupine_get_mouse_position);

STATIC mp_obj_t lupine_get_mouse_delta(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        auto delta = api->GetMouseDelta();
        return make_vec2(delta.x, delta.y);
    }
    return make_vec2(0.0f, 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_mouse_delta_obj, lupine_get_mouse_delta);

STATIC mp_obj_t lupine_get_mouse_scroll_delta(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        auto delta = api->GetMouseScrollDelta();
        return make_vec2(delta.x, delta.y);
    }
    return make_vec2(0.0f, 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_mouse_scroll_delta_obj, lupine_get_mouse_scroll_delta);

// ============================================================================
// INPUT - GAMEPAD FUNCTIONS
// ============================================================================

STATIC mp_obj_t lupine_is_gamepad_connected(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api) return mp_const_false;
    int gamepadId = n_args > 0 ? mp_obj_get_int(args[0]) : 0;
    return api->IsGamepadConnected(gamepadId) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_is_gamepad_connected_obj, 0, 1, lupine_is_gamepad_connected);

STATIC mp_obj_t lupine_get_gamepad_count(void) {
    auto* api = GetCurrentScriptAPI();
    return mp_obj_new_int(api ? api->GetGamepadCount() : 0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_gamepad_count_obj, lupine_get_gamepad_count);

STATIC mp_obj_t lupine_get_connected_gamepad_ids(void) {
    mp_obj_t list = mp_obj_new_list(0, nullptr);
    auto* api = GetCurrentScriptAPI();
    if (api) {
        for (int id : api->GetConnectedGamepadIds()) {
            mp_obj_list_append(list, mp_obj_new_int(id));
        }
    }
    return list;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_connected_gamepad_ids_obj, lupine_get_connected_gamepad_ids);

STATIC mp_obj_t lupine_get_gamepad_name(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api) return mp_obj_new_str("", 0);
    int gamepadId = n_args > 0 ? mp_obj_get_int(args[0]) : 0;
    std::string name = api->GetGamepadName(gamepadId);
    return mp_obj_new_str(name.c_str(), name.size());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_get_gamepad_name_obj, 0, 1, lupine_get_gamepad_name);

STATIC mp_obj_t lupine_is_gamepad_button_pressed(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 1) return mp_const_false;
    int button = mp_obj_get_int(args[0]);
    int gamepadId = n_args > 1 ? mp_obj_get_int(args[1]) : 0;
    return api->IsGamepadButtonPressed(button, gamepadId) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_is_gamepad_button_pressed_obj, 1, 2, lupine_is_gamepad_button_pressed);

STATIC mp_obj_t lupine_is_gamepad_button_just_pressed(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 1) return mp_const_false;
    int button = mp_obj_get_int(args[0]);
    int gamepadId = n_args > 1 ? mp_obj_get_int(args[1]) : 0;
    return api->IsGamepadButtonJustPressed(button, gamepadId) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_is_gamepad_button_just_pressed_obj, 1, 2, lupine_is_gamepad_button_just_pressed);

STATIC mp_obj_t lupine_is_gamepad_button_just_released(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 1) return mp_const_false;
    int button = mp_obj_get_int(args[0]);
    int gamepadId = n_args > 1 ? mp_obj_get_int(args[1]) : 0;
    return api->IsGamepadButtonJustReleased(button, gamepadId) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_is_gamepad_button_just_released_obj, 1, 2, lupine_is_gamepad_button_just_released);

STATIC mp_obj_t lupine_get_gamepad_axis(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 1) return mp_obj_new_float(0.0f);
    int axis = mp_obj_get_int(args[0]);
    int gamepadId = n_args > 1 ? mp_obj_get_int(args[1]) : 0;
    return mp_obj_new_float(api->GetGamepadAxis(axis, gamepadId));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_get_gamepad_axis_obj, 1, 2, lupine_get_gamepad_axis);

STATIC mp_obj_t lupine_set_gamepad_vibration(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 3) return mp_const_none;
    int gamepadId = mp_obj_get_int(args[0]);
    float left = mp_obj_get_float(args[1]);
    float right = mp_obj_get_float(args[2]);
    float duration = n_args > 3 ? mp_obj_get_float(args[3]) : 0.0f;
    api->SetGamepadVibration(gamepadId, left, right, duration);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_set_gamepad_vibration_obj, 3, 4, lupine_set_gamepad_vibration);

STATIC mp_obj_t lupine_stop_gamepad_vibration(mp_obj_t gamepad_id_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->StopGamepadVibration(mp_obj_get_int(gamepad_id_obj));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_stop_gamepad_vibration_obj, lupine_stop_gamepad_vibration);

STATIC mp_obj_t lupine_set_gamepad_deadzone(mp_obj_t deadzone_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->SetGamepadDeadzone(mp_obj_get_float(deadzone_obj));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_gamepad_deadzone_obj, lupine_set_gamepad_deadzone);

STATIC mp_obj_t lupine_get_gamepad_deadzone(void) {
    auto* api = GetCurrentScriptAPI();
    return mp_obj_new_float(api ? api->GetGamepadDeadzone() : 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_gamepad_deadzone_obj, lupine_get_gamepad_deadzone);

// ============================================================================
// INPUT - TOUCH FUNCTIONS
// ============================================================================

STATIC mp_obj_t lupine_is_touch_available(void) {
    auto* api = GetCurrentScriptAPI();
    return (api && api->IsTouchAvailable()) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_is_touch_available_obj, lupine_is_touch_available);

STATIC mp_obj_t lupine_is_touching(void) {
    auto* api = GetCurrentScriptAPI();
    return (api && api->IsTouching()) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_is_touching_obj, lupine_is_touching);

STATIC mp_obj_t lupine_get_touch_count(void) {
    auto* api = GetCurrentScriptAPI();
    return mp_obj_new_int(api ? api->GetTouchCount() : 0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_touch_count_obj, lupine_get_touch_count);

STATIC mp_obj_t lupine_get_touch_position(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api) return make_vec2(0.0f, 0.0f);
    int index = n_args > 0 ? mp_obj_get_int(args[0]) : 0;
    auto pos = api->GetTouchPosition(index);
    return make_vec2(pos.x, pos.y);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_get_touch_position_obj, 0, 1, lupine_get_touch_position);

STATIC mp_obj_t lupine_is_touch_just_started(void) {
    auto* api = GetCurrentScriptAPI();
    return (api && api->IsTouchJustStarted()) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_is_touch_just_started_obj, lupine_is_touch_just_started);

STATIC mp_obj_t lupine_is_touch_just_ended(void) {
    auto* api = GetCurrentScriptAPI();
    return (api && api->IsTouchJustEnded()) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_is_touch_just_ended_obj, lupine_is_touch_just_ended);

// ============================================================================
// INPUT - CLIPBOARD FUNCTIONS
// ============================================================================

STATIC mp_obj_t lupine_get_clipboard(void) {
    auto* api = GetCurrentScriptAPI();
    std::string r = api ? api->GetClipboardText() : "";
    return mp_obj_new_str(r.c_str(), r.size());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_clipboard_obj, lupine_get_clipboard);

STATIC mp_obj_t lupine_set_clipboard(mp_obj_t text_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->SetClipboardText(mp_obj_str_get_str(text_obj));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_clipboard_obj, lupine_set_clipboard);

// ============================================================================
// WINDOW / DISPLAY FUNCTIONS
// ============================================================================

STATIC mp_obj_t lupine_set_window_title(mp_obj_t title_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->SetWindowTitle(mp_obj_str_get_str(title_obj));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_window_title_obj, lupine_set_window_title);

STATIC mp_obj_t lupine_get_window_title(void) {
    auto* api = GetCurrentScriptAPI();
    std::string r = api ? api->GetWindowTitle() : "";
    return mp_obj_new_str(r.c_str(), r.size());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_window_title_obj, lupine_get_window_title);

STATIC mp_obj_t lupine_set_fullscreen(mp_obj_t fullscreen_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->SetFullscreen(mp_obj_is_true(fullscreen_obj));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_fullscreen_obj, lupine_set_fullscreen);

STATIC mp_obj_t lupine_is_fullscreen(void) {
    auto* api = GetCurrentScriptAPI();
    return (api && api->IsFullscreen()) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_is_fullscreen_obj, lupine_is_fullscreen);

STATIC mp_obj_t lupine_set_vsync(mp_obj_t enabled_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->SetVSync(mp_obj_is_true(enabled_obj));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_vsync_obj, lupine_set_vsync);

STATIC mp_obj_t lupine_is_vsync(void) {
    auto* api = GetCurrentScriptAPI();
    return (api && api->IsVSync()) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_is_vsync_obj, lupine_is_vsync);

STATIC mp_obj_t lupine_set_window_size(mp_obj_t w_obj, mp_obj_t h_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->SetWindowSize(mp_obj_get_int(w_obj), mp_obj_get_int(h_obj));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_set_window_size_obj, lupine_set_window_size);

STATIC mp_obj_t lupine_get_window_size(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        auto size = api->GetWindowSize();
        return make_vec2(size.x, size.y);
    }
    return make_vec2(0.0f, 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_window_size_obj, lupine_get_window_size);

STATIC mp_obj_t lupine_get_screen_size(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        auto size = api->GetScreenSize();
        return make_vec2(size.x, size.y);
    }
    return make_vec2(0.0f, 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_screen_size_obj, lupine_get_screen_size);

STATIC mp_obj_t lupine_maximize_window(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->MaximizeWindow();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_maximize_window_obj, lupine_maximize_window);

STATIC mp_obj_t lupine_minimize_window(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->MinimizeWindow();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_minimize_window_obj, lupine_minimize_window);

STATIC mp_obj_t lupine_restore_window(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->RestoreWindow();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_restore_window_obj, lupine_restore_window);

STATIC mp_obj_t lupine_set_mouse_mode(mp_obj_t mode_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->SetMouseMode(mp_obj_get_int(mode_obj));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_mouse_mode_obj, lupine_set_mouse_mode);

STATIC mp_obj_t lupine_get_mouse_mode(void) {
    auto* api = GetCurrentScriptAPI();
    return mp_obj_new_int(api ? api->GetMouseMode() : 0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_mouse_mode_obj, lupine_get_mouse_mode);

STATIC mp_obj_t lupine_set_mouse_cursor_visible(mp_obj_t visible_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->SetMouseCursorVisible(mp_obj_is_true(visible_obj));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_mouse_cursor_visible_obj, lupine_set_mouse_cursor_visible);

STATIC mp_obj_t lupine_is_mouse_cursor_visible(void) {
    auto* api = GetCurrentScriptAPI();
    return (api && api->IsMouseCursorVisible()) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_is_mouse_cursor_visible_obj, lupine_is_mouse_cursor_visible);

// ============================================================================
// SCREEN <-> WORLD CONVERSION
// ============================================================================

STATIC mp_obj_t lupine_screen_to_world_2d(mp_obj_t x_obj, mp_obj_t y_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        auto result = api->ScreenToWorld2D(math::Vec2(mp_obj_get_float(x_obj), mp_obj_get_float(y_obj)));
        return make_vec2(result.x, result.y);
    }
    return make_vec2(0.0f, 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_screen_to_world_2d_obj, lupine_screen_to_world_2d);

STATIC mp_obj_t lupine_world_to_screen_2d(mp_obj_t x_obj, mp_obj_t y_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        auto result = api->WorldToScreen2D(math::Vec2(mp_obj_get_float(x_obj), mp_obj_get_float(y_obj)));
        return make_vec2(result.x, result.y);
    }
    return make_vec2(0.0f, 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_world_to_screen_2d_obj, lupine_world_to_screen_2d);

STATIC mp_obj_t lupine_screen_to_world_3d(mp_obj_t x_obj, mp_obj_t y_obj, mp_obj_t distance_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        auto result = api->ScreenToWorld3D(math::Vec2(mp_obj_get_float(x_obj), mp_obj_get_float(y_obj)), mp_obj_get_float(distance_obj));
        return make_vec3(result.x, result.y, result.z);
    }
    return make_vec3(0.0f, 0.0f, 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lupine_screen_to_world_3d_obj, lupine_screen_to_world_3d);

STATIC mp_obj_t lupine_world_to_screen_3d(mp_obj_t x_obj, mp_obj_t y_obj, mp_obj_t z_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        auto result = api->WorldToScreen3D(math::Vec3(mp_obj_get_float(x_obj), mp_obj_get_float(y_obj), mp_obj_get_float(z_obj)));
        return make_vec3(result.x, result.y, result.z);
    }
    return make_vec3(0.0f, 0.0f, 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lupine_world_to_screen_3d_obj, lupine_world_to_screen_3d);

STATIC mp_obj_t lupine_screen_to_world_ray_3d(mp_obj_t x_obj, mp_obj_t y_obj) {
    auto* api = GetCurrentScriptAPI();
    ScriptAPI::ScreenRay ray;
    if (api) {
        ray = api->ScreenToWorldRay3D(math::Vec2(mp_obj_get_float(x_obj), mp_obj_get_float(y_obj)));
    }
    mp_obj_t dict = mp_obj_new_dict(2);
    mp_obj_dict_store(dict, mp_obj_new_str("origin", 6), make_vec3(ray.origin.x, ray.origin.y, ray.origin.z));
    mp_obj_dict_store(dict, mp_obj_new_str("direction", 9), make_vec3(ray.direction.x, ray.direction.y, ray.direction.z));
    return dict;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_screen_to_world_ray_3d_obj, lupine_screen_to_world_ray_3d);

// ============================================================================
// DEBUG DRAW
// ============================================================================

STATIC mp_obj_t lupine_debug_draw_line(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 10) return mp_const_none;
    math::Vec3 start(mp_obj_get_float(args[0]), mp_obj_get_float(args[1]), mp_obj_get_float(args[2]));
    math::Vec3 end(mp_obj_get_float(args[3]), mp_obj_get_float(args[4]), mp_obj_get_float(args[5]));
    math::Color color(mp_obj_get_float(args[6]), mp_obj_get_float(args[7]), mp_obj_get_float(args[8]), mp_obj_get_float(args[9]));
    float duration = n_args > 10 ? mp_obj_get_float(args[10]) : 0.0f;
    api->DebugDrawLine(start, end, color, duration);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_debug_draw_line_obj, 10, 11, lupine_debug_draw_line);

STATIC mp_obj_t lupine_debug_draw_line_2d(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 8) return mp_const_none;
    math::Vec2 start(mp_obj_get_float(args[0]), mp_obj_get_float(args[1]));
    math::Vec2 end(mp_obj_get_float(args[2]), mp_obj_get_float(args[3]));
    math::Color color(mp_obj_get_float(args[4]), mp_obj_get_float(args[5]), mp_obj_get_float(args[6]), mp_obj_get_float(args[7]));
    float duration = n_args > 8 ? mp_obj_get_float(args[8]) : 0.0f;
    api->DebugDrawLine2D(start, end, color, duration);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_debug_draw_line_2d_obj, 8, 9, lupine_debug_draw_line_2d);

STATIC mp_obj_t lupine_debug_draw_ray(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 10) return mp_const_none;
    math::Vec3 origin(mp_obj_get_float(args[0]), mp_obj_get_float(args[1]), mp_obj_get_float(args[2]));
    math::Vec3 direction(mp_obj_get_float(args[3]), mp_obj_get_float(args[4]), mp_obj_get_float(args[5]));
    math::Color color(mp_obj_get_float(args[6]), mp_obj_get_float(args[7]), mp_obj_get_float(args[8]), mp_obj_get_float(args[9]));
    float duration = n_args > 10 ? mp_obj_get_float(args[10]) : 0.0f;
    api->DebugDrawRay(origin, direction, color, duration);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_debug_draw_ray_obj, 10, 11, lupine_debug_draw_ray);

STATIC mp_obj_t lupine_debug_draw_box(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 10) return mp_const_none;
    math::Vec3 center(mp_obj_get_float(args[0]), mp_obj_get_float(args[1]), mp_obj_get_float(args[2]));
    math::Vec3 size(mp_obj_get_float(args[3]), mp_obj_get_float(args[4]), mp_obj_get_float(args[5]));
    math::Color color(mp_obj_get_float(args[6]), mp_obj_get_float(args[7]), mp_obj_get_float(args[8]), mp_obj_get_float(args[9]));
    float duration = n_args > 10 ? mp_obj_get_float(args[10]) : 0.0f;
    api->DebugDrawBox(center, size, color, duration);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_debug_draw_box_obj, 10, 11, lupine_debug_draw_box);

STATIC mp_obj_t lupine_debug_draw_sphere(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 8) return mp_const_none;
    math::Vec3 center(mp_obj_get_float(args[0]), mp_obj_get_float(args[1]), mp_obj_get_float(args[2]));
    float radius = mp_obj_get_float(args[3]);
    math::Color color(mp_obj_get_float(args[4]), mp_obj_get_float(args[5]), mp_obj_get_float(args[6]), mp_obj_get_float(args[7]));
    float duration = n_args > 8 ? mp_obj_get_float(args[8]) : 0.0f;
    api->DebugDrawSphere(center, radius, color, duration);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_debug_draw_sphere_obj, 8, 9, lupine_debug_draw_sphere);

STATIC mp_obj_t lupine_debug_draw_circle(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 11) return mp_const_none;
    math::Vec3 center(mp_obj_get_float(args[0]), mp_obj_get_float(args[1]), mp_obj_get_float(args[2]));
    math::Vec3 normal(mp_obj_get_float(args[3]), mp_obj_get_float(args[4]), mp_obj_get_float(args[5]));
    float radius = mp_obj_get_float(args[6]);
    math::Color color(mp_obj_get_float(args[7]), mp_obj_get_float(args[8]), mp_obj_get_float(args[9]), mp_obj_get_float(args[10]));
    float duration = n_args > 11 ? mp_obj_get_float(args[11]) : 0.0f;
    api->DebugDrawCircle(center, normal, radius, color, duration);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_debug_draw_circle_obj, 11, 12, lupine_debug_draw_circle);

STATIC mp_obj_t lupine_debug_draw_text(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 8) return mp_const_none;
    math::Vec3 position(mp_obj_get_float(args[0]), mp_obj_get_float(args[1]), mp_obj_get_float(args[2]));
    const char* text = mp_obj_str_get_str(args[3]);
    math::Color color(mp_obj_get_float(args[4]), mp_obj_get_float(args[5]), mp_obj_get_float(args[6]), mp_obj_get_float(args[7]));
    float duration = n_args > 8 ? mp_obj_get_float(args[8]) : 0.0f;
    api->DebugDrawText(position, text, color, duration);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_debug_draw_text_obj, 8, 9, lupine_debug_draw_text);

STATIC mp_obj_t lupine_debug_draw_text_2d(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 7) return mp_const_none;
    math::Vec2 position(mp_obj_get_float(args[0]), mp_obj_get_float(args[1]));
    const char* text = mp_obj_str_get_str(args[2]);
    math::Color color(mp_obj_get_float(args[3]), mp_obj_get_float(args[4]), mp_obj_get_float(args[5]), mp_obj_get_float(args[6]));
    float duration = n_args > 7 ? mp_obj_get_float(args[7]) : 0.0f;
    api->DebugDrawText2D(position, text, color, duration);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_debug_draw_text_2d_obj, 7, 8, lupine_debug_draw_text_2d);

// ============================================================================
// CUSTOM COMPONENT RENDERING (on_draw) + EDITOR-ONLY DEBUG DRAW
// ============================================================================

STATIC mp_obj_t lupine_draw_quad(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 9) return mp_const_none;
    math::Vec3 position(mp_obj_get_float(args[0]), mp_obj_get_float(args[1]), mp_obj_get_float(args[2]));
    math::Vec2 size(mp_obj_get_float(args[3]), mp_obj_get_float(args[4]));
    math::Color color(mp_obj_get_float(args[5]), mp_obj_get_float(args[6]), mp_obj_get_float(args[7]), mp_obj_get_float(args[8]));
    int blend = n_args > 9 ? mp_obj_get_int(args[9]) : 0;
    api->DrawQuad(position, size, color, blend);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_draw_quad_obj, 9, 10, lupine_draw_quad);

STATIC mp_obj_t lupine_draw_textured_quad(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 10) return mp_const_none;
    math::Vec3 position(mp_obj_get_float(args[0]), mp_obj_get_float(args[1]), mp_obj_get_float(args[2]));
    math::Vec2 size(mp_obj_get_float(args[3]), mp_obj_get_float(args[4]));
    math::Color tint(mp_obj_get_float(args[5]), mp_obj_get_float(args[6]), mp_obj_get_float(args[7]), mp_obj_get_float(args[8]));
    const char* path = mp_obj_str_get_str(args[9]);
    int blend = n_args > 10 ? mp_obj_get_int(args[10]) : 0;
    api->DrawTexturedQuad(position, size, tint, path, blend);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_draw_textured_quad_obj, 10, 11, lupine_draw_textured_quad);

STATIC mp_obj_t lupine_draw_rect(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 8) return mp_const_none;
    math::Vec2 position(mp_obj_get_float(args[0]), mp_obj_get_float(args[1]));
    math::Vec2 size(mp_obj_get_float(args[2]), mp_obj_get_float(args[3]));
    math::Color color(mp_obj_get_float(args[4]), mp_obj_get_float(args[5]), mp_obj_get_float(args[6]), mp_obj_get_float(args[7]));
    bool filled = n_args > 8 ? mp_obj_is_true(args[8]) : true;
    float thickness = n_args > 9 ? mp_obj_get_float(args[9]) : 1.0f;
    api->DrawRect(position, size, color, filled, thickness);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_draw_rect_obj, 8, 10, lupine_draw_rect);

STATIC mp_obj_t lupine_draw_sprite(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 9) return mp_const_none;
    const char* path = mp_obj_str_get_str(args[0]);
    math::Vec2 position(mp_obj_get_float(args[1]), mp_obj_get_float(args[2]));
    math::Vec2 size(mp_obj_get_float(args[3]), mp_obj_get_float(args[4]));
    math::Color tint(mp_obj_get_float(args[5]), mp_obj_get_float(args[6]), mp_obj_get_float(args[7]), mp_obj_get_float(args[8]));
    float rotation = n_args > 9 ? mp_obj_get_float(args[9]) : 0.0f;
    int blend = n_args > 10 ? mp_obj_get_int(args[10]) : 0;
    api->DrawSprite(path, position, size, tint, rotation, blend);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_draw_sprite_obj, 9, 11, lupine_draw_sprite);

STATIC mp_obj_t lupine_draw_line(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 10) return mp_const_none;
    math::Vec3 start(mp_obj_get_float(args[0]), mp_obj_get_float(args[1]), mp_obj_get_float(args[2]));
    math::Vec3 end(mp_obj_get_float(args[3]), mp_obj_get_float(args[4]), mp_obj_get_float(args[5]));
    math::Color color(mp_obj_get_float(args[6]), mp_obj_get_float(args[7]), mp_obj_get_float(args[8]), mp_obj_get_float(args[9]));
    float thickness = n_args > 10 ? mp_obj_get_float(args[10]) : 1.0f;
    api->DrawLine(start, end, color, thickness);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_draw_line_obj, 10, 11, lupine_draw_line);

STATIC mp_obj_t lupine_draw_circle(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 8) return mp_const_none;
    math::Vec3 center(mp_obj_get_float(args[0]), mp_obj_get_float(args[1]), mp_obj_get_float(args[2]));
    float radius = mp_obj_get_float(args[3]);
    math::Color color(mp_obj_get_float(args[4]), mp_obj_get_float(args[5]), mp_obj_get_float(args[6]), mp_obj_get_float(args[7]));
    bool filled = n_args > 8 ? mp_obj_is_true(args[8]) : true;
    api->DrawCircle(center, radius, color, filled);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_draw_circle_obj, 8, 9, lupine_draw_circle);

STATIC mp_obj_t lupine_draw_polygon(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 8) return mp_const_none;
    math::Vec2 center(mp_obj_get_float(args[0]), mp_obj_get_float(args[1]));
    float radius = mp_obj_get_float(args[2]);
    int sides = mp_obj_get_int(args[3]);
    math::Color color(mp_obj_get_float(args[4]), mp_obj_get_float(args[5]), mp_obj_get_float(args[6]), mp_obj_get_float(args[7]));
    float rotation = n_args > 8 ? mp_obj_get_float(args[8]) : 0.0f;
    int blend = n_args > 9 ? mp_obj_get_int(args[9]) : 0;
    api->DrawPolygon(center, radius, sides, color, rotation, blend);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_draw_polygon_obj, 8, 10, lupine_draw_polygon);

STATIC mp_obj_t lupine_draw_box(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 10) return mp_const_none;
    math::Vec3 center(mp_obj_get_float(args[0]), mp_obj_get_float(args[1]), mp_obj_get_float(args[2]));
    math::Vec3 size(mp_obj_get_float(args[3]), mp_obj_get_float(args[4]), mp_obj_get_float(args[5]));
    math::Color color(mp_obj_get_float(args[6]), mp_obj_get_float(args[7]), mp_obj_get_float(args[8]), mp_obj_get_float(args[9]));
    bool wireframe = n_args > 10 ? mp_obj_is_true(args[10]) : false;
    api->DrawBox(center, size, color, wireframe);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_draw_box_obj, 10, 11, lupine_draw_box);

STATIC mp_obj_t lupine_draw_rounded_rect(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 9) return mp_const_none;
    math::Vec2 position(mp_obj_get_float(args[0]), mp_obj_get_float(args[1]));
    math::Vec2 size(mp_obj_get_float(args[2]), mp_obj_get_float(args[3]));
    float cornerRadius = mp_obj_get_float(args[4]);
    math::Color color(mp_obj_get_float(args[5]), mp_obj_get_float(args[6]), mp_obj_get_float(args[7]), mp_obj_get_float(args[8]));
    int blend = n_args > 9 ? mp_obj_get_int(args[9]) : 0;
    api->DrawRoundedRect(position, size, cornerRadius, color, blend);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_draw_rounded_rect_obj, 9, 10, lupine_draw_rounded_rect);

STATIC mp_obj_t lupine_is_drawing(void) {
    auto* api = GetCurrentScriptAPI();
    return mp_obj_new_bool(api ? api->IsDrawing() : false);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_is_drawing_obj, lupine_is_drawing);

STATIC mp_obj_t lupine_editor_draw_line(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 10) return mp_const_none;
    math::Vec3 start(mp_obj_get_float(args[0]), mp_obj_get_float(args[1]), mp_obj_get_float(args[2]));
    math::Vec3 end(mp_obj_get_float(args[3]), mp_obj_get_float(args[4]), mp_obj_get_float(args[5]));
    math::Color color(mp_obj_get_float(args[6]), mp_obj_get_float(args[7]), mp_obj_get_float(args[8]), mp_obj_get_float(args[9]));
    api->EditorDrawLine(start, end, color);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_editor_draw_line_obj, 10, 10, lupine_editor_draw_line);

STATIC mp_obj_t lupine_editor_draw_box(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 10) return mp_const_none;
    math::Vec3 center(mp_obj_get_float(args[0]), mp_obj_get_float(args[1]), mp_obj_get_float(args[2]));
    math::Vec3 size(mp_obj_get_float(args[3]), mp_obj_get_float(args[4]), mp_obj_get_float(args[5]));
    math::Color color(mp_obj_get_float(args[6]), mp_obj_get_float(args[7]), mp_obj_get_float(args[8]), mp_obj_get_float(args[9]));
    bool wireframe = n_args > 10 ? mp_obj_is_true(args[10]) : true;
    api->EditorDrawBox(center, size, color, wireframe);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_editor_draw_box_obj, 10, 11, lupine_editor_draw_box);

STATIC mp_obj_t lupine_editor_draw_sphere(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 8) return mp_const_none;
    math::Vec3 center(mp_obj_get_float(args[0]), mp_obj_get_float(args[1]), mp_obj_get_float(args[2]));
    float radius = mp_obj_get_float(args[3]);
    math::Color color(mp_obj_get_float(args[4]), mp_obj_get_float(args[5]), mp_obj_get_float(args[6]), mp_obj_get_float(args[7]));
    bool wireframe = n_args > 8 ? mp_obj_is_true(args[8]) : true;
    api->EditorDrawSphere(center, radius, color, wireframe);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_editor_draw_sphere_obj, 8, 9, lupine_editor_draw_sphere);

STATIC mp_obj_t lupine_editor_draw_circle(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 11) return mp_const_none;
    math::Vec3 center(mp_obj_get_float(args[0]), mp_obj_get_float(args[1]), mp_obj_get_float(args[2]));
    math::Vec3 normal(mp_obj_get_float(args[3]), mp_obj_get_float(args[4]), mp_obj_get_float(args[5]));
    float radius = mp_obj_get_float(args[6]);
    math::Color color(mp_obj_get_float(args[7]), mp_obj_get_float(args[8]), mp_obj_get_float(args[9]), mp_obj_get_float(args[10]));
    api->EditorDrawCircle(center, normal, radius, color);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_editor_draw_circle_obj, 11, 11, lupine_editor_draw_circle);

STATIC mp_obj_t lupine_editor_draw_rect_2d(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 8) return mp_const_none;
    math::Vec2 center(mp_obj_get_float(args[0]), mp_obj_get_float(args[1]));
    math::Vec2 size(mp_obj_get_float(args[2]), mp_obj_get_float(args[3]));
    math::Color color(mp_obj_get_float(args[4]), mp_obj_get_float(args[5]), mp_obj_get_float(args[6]), mp_obj_get_float(args[7]));
    api->EditorDrawRect2D(center, size, color);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_editor_draw_rect_2d_obj, 8, 8, lupine_editor_draw_rect_2d);

STATIC mp_obj_t lupine_editor_draw_text(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 8) return mp_const_none;
    math::Vec3 position(mp_obj_get_float(args[0]), mp_obj_get_float(args[1]), mp_obj_get_float(args[2]));
    const char* text = mp_obj_str_get_str(args[3]);
    math::Color color(mp_obj_get_float(args[4]), mp_obj_get_float(args[5]), mp_obj_get_float(args[6]), mp_obj_get_float(args[7]));
    api->EditorDrawText(position, text, color);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_editor_draw_text_obj, 8, 8, lupine_editor_draw_text);

STATIC mp_obj_t lupine_is_editor_draw_available(void) {
    auto* api = GetCurrentScriptAPI();
    return mp_obj_new_bool(api ? api->IsEditorDrawAvailable() : false);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_is_editor_draw_available_obj, lupine_is_editor_draw_available);

// ============================================================================
// TRANSFORM 2D - POSITION
// ============================================================================

STATIC mp_obj_t lupine_get_position_2d(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        auto pos = api->GetPosition2D();
        return make_vec2(pos.x, pos.y);
    }
    return make_vec2(0.0f, 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_position_2d_obj, lupine_get_position_2d);

STATIC mp_obj_t lupine_set_position_2d(mp_obj_t x_obj, mp_obj_t y_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        float x = mp_obj_get_float(x_obj);
        float y = mp_obj_get_float(y_obj);
        api->SetPosition2D(x, y);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_set_position_2d_obj, lupine_set_position_2d);

STATIC mp_obj_t lupine_translate_2d(mp_obj_t dx_obj, mp_obj_t dy_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        float dx = mp_obj_get_float(dx_obj);
        float dy = mp_obj_get_float(dy_obj);
        api->Translate2D(dx, dy);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_translate_2d_obj, lupine_translate_2d);

STATIC mp_obj_t lupine_get_global_position_2d(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        auto pos = api->GetGlobalPosition2D();
        return make_vec2(pos.x, pos.y);
    }
    return make_vec2(0.0f, 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_global_position_2d_obj, lupine_get_global_position_2d);

STATIC mp_obj_t lupine_set_global_position_2d(mp_obj_t x_obj, mp_obj_t y_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        float x = mp_obj_get_float(x_obj);
        float y = mp_obj_get_float(y_obj);
        api->SetGlobalPosition2D(x, y);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_set_global_position_2d_obj, lupine_set_global_position_2d);

// ============================================================================
// TRANSFORM 2D - ROTATION
// ============================================================================

STATIC mp_obj_t lupine_get_rotation_2d(void) {
    auto* api = GetCurrentScriptAPI();
    return mp_obj_new_float(api ? api->GetRotation2D() : 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_rotation_2d_obj, lupine_get_rotation_2d);

STATIC mp_obj_t lupine_set_rotation_2d(mp_obj_t angle_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        float angle = mp_obj_get_float(angle_obj);
        api->SetRotation2D(angle);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_rotation_2d_obj, lupine_set_rotation_2d);

STATIC mp_obj_t lupine_rotate_2d(mp_obj_t delta_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        float delta = mp_obj_get_float(delta_obj);
        api->Rotate2D(delta);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_rotate_2d_obj, lupine_rotate_2d);

STATIC mp_obj_t lupine_get_global_rotation_2d(void) {
    auto* api = GetCurrentScriptAPI();
    return mp_obj_new_float(api ? api->GetGlobalRotation2D() : 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_global_rotation_2d_obj, lupine_get_global_rotation_2d);

STATIC mp_obj_t lupine_set_global_rotation_2d(mp_obj_t angle_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        float angle = mp_obj_get_float(angle_obj);
        api->SetGlobalRotation2D(angle);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_global_rotation_2d_obj, lupine_set_global_rotation_2d);

// ============================================================================
// TRANSFORM 2D - SCALE
// ============================================================================

STATIC mp_obj_t lupine_get_scale_2d(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        auto scale = api->GetScale2D();
        return make_vec2(scale.x, scale.y);
    }
    return make_vec2(1.0f, 1.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_scale_2d_obj, lupine_get_scale_2d);

STATIC mp_obj_t lupine_set_scale_2d(mp_obj_t x_obj, mp_obj_t y_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        float x = mp_obj_get_float(x_obj);
        float y = mp_obj_get_float(y_obj);
        api->SetScale2D(x, y);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_set_scale_2d_obj, lupine_set_scale_2d);

// ============================================================================
// TRANSFORM 3D - POSITION
// ============================================================================

STATIC mp_obj_t lupine_get_position_3d(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        auto pos = api->GetPosition3D();
        return make_vec3(pos.x, pos.y, pos.z);
    }
    return make_vec3(0.0f, 0.0f, 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_position_3d_obj, lupine_get_position_3d);

STATIC mp_obj_t lupine_set_position_3d(mp_obj_t x_obj, mp_obj_t y_obj, mp_obj_t z_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        float x = mp_obj_get_float(x_obj);
        float y = mp_obj_get_float(y_obj);
        float z = mp_obj_get_float(z_obj);
        api->SetPosition3D(x, y, z);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lupine_set_position_3d_obj, lupine_set_position_3d);

STATIC mp_obj_t lupine_translate_3d(mp_obj_t dx_obj, mp_obj_t dy_obj, mp_obj_t dz_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        float dx = mp_obj_get_float(dx_obj);
        float dy = mp_obj_get_float(dy_obj);
        float dz = mp_obj_get_float(dz_obj);
        api->Translate3D(dx, dy, dz);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lupine_translate_3d_obj, lupine_translate_3d);

STATIC mp_obj_t lupine_get_global_position_3d(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        auto pos = api->GetGlobalPosition3D();
        return make_vec3(pos.x, pos.y, pos.z);
    }
    return make_vec3(0.0f, 0.0f, 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_global_position_3d_obj, lupine_get_global_position_3d);

STATIC mp_obj_t lupine_set_global_position_3d(mp_obj_t x_obj, mp_obj_t y_obj, mp_obj_t z_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        float x = mp_obj_get_float(x_obj);
        float y = mp_obj_get_float(y_obj);
        float z = mp_obj_get_float(z_obj);
        api->SetGlobalPosition3D(x, y, z);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lupine_set_global_position_3d_obj, lupine_set_global_position_3d);

STATIC mp_obj_t lupine_get_global_rotation_3d(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        auto rot = api->GetGlobalRotation3D();
        return make_vec3(rot.x, rot.y, rot.z);
    }
    return make_vec3(0.0f, 0.0f, 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_global_rotation_3d_obj, lupine_get_global_rotation_3d);

STATIC mp_obj_t lupine_set_global_rotation_3d(mp_obj_t pitch_obj, mp_obj_t yaw_obj, mp_obj_t roll_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        float pitch = mp_obj_get_float(pitch_obj);
        float yaw = mp_obj_get_float(yaw_obj);
        float roll = mp_obj_get_float(roll_obj);
        api->SetGlobalRotation3D(pitch, yaw, roll);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lupine_set_global_rotation_3d_obj, lupine_set_global_rotation_3d);

STATIC mp_obj_t lupine_get_global_scale_2d(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        auto s = api->GetGlobalScale2D();
        return make_vec2(s.x, s.y);
    }
    return make_vec2(1.0f, 1.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_global_scale_2d_obj, lupine_get_global_scale_2d);

STATIC mp_obj_t lupine_get_global_scale_3d(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        auto s = api->GetGlobalScale3D();
        return make_vec3(s.x, s.y, s.z);
    }
    return make_vec3(1.0f, 1.0f, 1.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_global_scale_3d_obj, lupine_get_global_scale_3d);

// ============================================================================
// TRANSFORM 3D - ROTATION
// ============================================================================

STATIC mp_obj_t lupine_get_rotation_3d(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        auto rot = api->GetRotation3D();
        return make_vec3(rot.x, rot.y, rot.z);
    }
    return make_vec3(0.0f, 0.0f, 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_rotation_3d_obj, lupine_get_rotation_3d);

STATIC mp_obj_t lupine_set_rotation_3d(mp_obj_t pitch_obj, mp_obj_t yaw_obj, mp_obj_t roll_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        float pitch = mp_obj_get_float(pitch_obj);
        float yaw = mp_obj_get_float(yaw_obj);
        float roll = mp_obj_get_float(roll_obj);
        api->SetRotation3D(pitch, yaw, roll);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lupine_set_rotation_3d_obj, lupine_set_rotation_3d);

STATIC mp_obj_t lupine_rotate_3d(mp_obj_t pitch_obj, mp_obj_t yaw_obj, mp_obj_t roll_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        float pitch = mp_obj_get_float(pitch_obj);
        float yaw = mp_obj_get_float(yaw_obj);
        float roll = mp_obj_get_float(roll_obj);
        api->Rotate3D(pitch, yaw, roll);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lupine_rotate_3d_obj, lupine_rotate_3d);

// ============================================================================
// TRANSFORM 3D - SCALE
// ============================================================================

STATIC mp_obj_t lupine_get_scale_3d(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        auto scale = api->GetScale3D();
        return make_vec3(scale.x, scale.y, scale.z);
    }
    return make_vec3(1.0f, 1.0f, 1.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_scale_3d_obj, lupine_get_scale_3d);

STATIC mp_obj_t lupine_set_scale_3d(mp_obj_t x_obj, mp_obj_t y_obj, mp_obj_t z_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        float x = mp_obj_get_float(x_obj);
        float y = mp_obj_get_float(y_obj);
        float z = mp_obj_get_float(z_obj);
        api->SetScale3D(x, y, z);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lupine_set_scale_3d_obj, lupine_set_scale_3d);

// ============================================================================
// TRANSFORM 3D - DIRECTION VECTORS
// ============================================================================

STATIC mp_obj_t lupine_get_forward(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        auto fwd = api->GetForward();
        return make_vec3(fwd.x, fwd.y, fwd.z);
    }
    return make_vec3(0.0f, 0.0f, -1.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_forward_obj, lupine_get_forward);

STATIC mp_obj_t lupine_get_right(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        auto right = api->GetRight();
        return make_vec3(right.x, right.y, right.z);
    }
    return make_vec3(1.0f, 0.0f, 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_right_obj, lupine_get_right);

STATIC mp_obj_t lupine_get_up(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        auto up = api->GetUp();
        return make_vec3(up.x, up.y, up.z);
    }
    return make_vec3(0.0f, 1.0f, 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_up_obj, lupine_get_up);

// ============================================================================
// LOOK AT
// ============================================================================

STATIC mp_obj_t lupine_look_at_2d(mp_obj_t x_obj, mp_obj_t y_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        float x = mp_obj_get_float(x_obj);
        float y = mp_obj_get_float(y_obj);
        api->LookAt2D(x, y);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_look_at_2d_obj, lupine_look_at_2d);

STATIC mp_obj_t lupine_look_at_3d(mp_obj_t x_obj, mp_obj_t y_obj, mp_obj_t z_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        float x = mp_obj_get_float(x_obj);
        float y = mp_obj_get_float(y_obj);
        float z = mp_obj_get_float(z_obj);
        api->LookAt3D(x, y, z);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lupine_look_at_3d_obj, lupine_look_at_3d);

// ============================================================================
// DISTANCE
// ============================================================================

STATIC mp_obj_t lupine_distance_to_2d(mp_obj_t x_obj, mp_obj_t y_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        float x = mp_obj_get_float(x_obj);
        float y = mp_obj_get_float(y_obj);
        return mp_obj_new_float(api->DistanceTo2D(x, y));
    }
    return mp_obj_new_float(0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_distance_to_2d_obj, lupine_distance_to_2d);

STATIC mp_obj_t lupine_distance_to_3d(mp_obj_t x_obj, mp_obj_t y_obj, mp_obj_t z_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        float x = mp_obj_get_float(x_obj);
        float y = mp_obj_get_float(y_obj);
        float z = mp_obj_get_float(z_obj);
        return mp_obj_new_float(api->DistanceTo3D(x, y, z));
    }
    return mp_obj_new_float(0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lupine_distance_to_3d_obj, lupine_distance_to_3d);

// ============================================================================
// MOVE TOWARD
// ============================================================================

STATIC mp_obj_t lupine_move_toward_2d(mp_obj_t x_obj, mp_obj_t y_obj, mp_obj_t delta_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        float x = mp_obj_get_float(x_obj);
        float y = mp_obj_get_float(y_obj);
        float delta = mp_obj_get_float(delta_obj);
        api->MoveToward2D(x, y, delta);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lupine_move_toward_2d_obj, lupine_move_toward_2d);

STATIC mp_obj_t lupine_move_toward_3d(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (api && n_args >= 4) {
        float x = mp_obj_get_float(args[0]);
        float y = mp_obj_get_float(args[1]);
        float z = mp_obj_get_float(args[2]);
        float delta = mp_obj_get_float(args[3]);
        api->MoveToward3D(x, y, z, delta);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_move_toward_3d_obj, 4, 4, lupine_move_toward_3d);

// ============================================================================
// NODE PROPERTIES
// ============================================================================

STATIC mp_obj_t lupine_get_name(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        std::string name = api->GetName();
        return mp_obj_new_str(name.c_str(), name.length());
    }
    return mp_obj_new_str("", 0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_name_obj, lupine_get_name);

STATIC mp_obj_t lupine_set_name(mp_obj_t name_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        const char* name = mp_obj_str_get_str(name_obj);
        api->SetName(name);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_name_obj, lupine_set_name);

STATIC mp_obj_t lupine_is_active(void) {
    auto* api = GetCurrentScriptAPI();
    return (api && api->IsActive()) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_is_active_obj, lupine_is_active);

STATIC mp_obj_t lupine_set_active(mp_obj_t active_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        api->SetActive(mp_obj_is_true(active_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_active_obj, lupine_set_active);

STATIC mp_obj_t lupine_is_visible(void) {
    auto* api = GetCurrentScriptAPI();
    return (api && api->IsVisible()) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_is_visible_obj, lupine_is_visible);

STATIC mp_obj_t lupine_set_visible(mp_obj_t visible_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        api->SetVisible(mp_obj_is_true(visible_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_visible_obj, lupine_set_visible);

STATIC mp_obj_t lupine_get_sibling_index(void) {
    auto* api = GetCurrentScriptAPI();
    return mp_obj_new_int(api ? api->GetSiblingIndex() : 0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_sibling_index_obj, lupine_get_sibling_index);

STATIC mp_obj_t lupine_set_sibling_index(mp_obj_t index_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        api->SetSiblingIndex(mp_obj_get_int(index_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_sibling_index_obj, lupine_set_sibling_index);

// ============================================================================
// NODE HIERARCHY
// ============================================================================

STATIC mp_obj_t lupine_get_child_count(void) {
    auto* api = GetCurrentScriptAPI();
    return mp_obj_new_int(api ? api->GetChildCount() : 0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_child_count_obj, lupine_get_child_count);

STATIC mp_obj_t lupine_has_node(mp_obj_t path_obj) {
    auto* api = GetCurrentScriptAPI();
    if (!api) return mp_const_false;
    const char* path = mp_obj_str_get_str(path_obj);
    return api->HasNode(path) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_has_node_obj, lupine_has_node);

STATIC mp_obj_t lupine_has_component(mp_obj_t type_obj) {
    auto* api = GetCurrentScriptAPI();
    if (!api) return mp_const_false;
    const char* typeName = mp_obj_str_get_str(type_obj);
    return api->HasComponent(typeName) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_has_component_obj, lupine_has_component);

// ============================================================================
// NODE LIFETIME
// ============================================================================

STATIC mp_obj_t lupine_queue_free_self(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        api->QueueFreeSelf();
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_queue_free_self_obj, lupine_queue_free_self);

STATIC mp_obj_t lupine_queue_free_deferred_self(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        api->QueueFreeDeferredSelf();
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_queue_free_deferred_self_obj, lupine_queue_free_deferred_self);

STATIC mp_obj_t lupine_free_self(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        api->FreeSelf();
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_free_self_obj, lupine_free_self);

STATIC mp_obj_t lupine_quit(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        api->RequestQuit();
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_quit_obj, lupine_quit);

STATIC mp_obj_t lupine_get_cmdline_args(void) {
    auto* api = GetCurrentScriptAPI();
    mp_obj_t list = mp_obj_new_list(0, nullptr);
    if (api) {
        for (const std::string& arg : api->GetCommandLineArgs()) {
            mp_obj_list_append(list, mp_obj_new_str(arg.c_str(), arg.size()));
        }
    }
    return list;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_cmdline_args_obj, lupine_get_cmdline_args);

// ============================================================================
// SCENE MANAGEMENT
// ============================================================================

STATIC mp_obj_t lupine_change_scene(mp_obj_t path_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        const char* path = mp_obj_str_get_str(path_obj);
        api->ChangeScene(path);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_change_scene_obj, lupine_change_scene);

STATIC mp_obj_t lupine_reload_scene(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        api->ReloadScene();
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_reload_scene_obj, lupine_reload_scene);

STATIC mp_obj_t lupine_add_scene(mp_obj_t path_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        const char* path = mp_obj_str_get_str(path_obj);
        api->AddScene(path);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_add_scene_obj, lupine_add_scene);

STATIC mp_obj_t lupine_remove_scene(mp_obj_t name_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        const char* name = mp_obj_str_get_str(name_obj);
        api->RemoveScene(name);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_remove_scene_obj, lupine_remove_scene);

STATIC mp_obj_t lupine_get_current_scene_path(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        std::string path = api->GetCurrentScenePath();
        return mp_obj_new_str(path.c_str(), path.length());
    }
    return mp_obj_new_str("", 0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_current_scene_path_obj, lupine_get_current_scene_path);

// ============================================================================
// AUDIO
// ============================================================================

STATIC mp_obj_t lupine_play_audio(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 1) return mp_obj_new_str("", 0);

    const char* path = mp_obj_str_get_str(args[0]);
    std::string bus = n_args > 1 ? mp_obj_str_get_str(args[1]) : "Master";
    bool loop = n_args > 2 ? mp_obj_is_true(args[2]) : false;
    float volume = n_args > 3 ? mp_obj_get_float(args[3]) : 1.0f;

    std::string uuid = api->PlayAudio(path, bus, loop, volume);
    return mp_obj_new_str(uuid.c_str(), uuid.length());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_play_audio_obj, 1, 4, lupine_play_audio);

STATIC mp_obj_t lupine_play_audio_3d(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 4) return mp_obj_new_str("", 0);

    const char* path = mp_obj_str_get_str(args[0]);
    float x = mp_obj_get_float(args[1]);
    float y = mp_obj_get_float(args[2]);
    float z = mp_obj_get_float(args[3]);
    std::string bus = n_args > 4 ? mp_obj_str_get_str(args[4]) : "Master";
    bool loop = n_args > 5 ? mp_obj_is_true(args[5]) : false;
    float volume = n_args > 6 ? mp_obj_get_float(args[6]) : 1.0f;

    std::string uuid = api->PlayAudio3D(path, math::Vec3(x, y, z), bus, loop, volume);
    return mp_obj_new_str(uuid.c_str(), uuid.length());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_play_audio_3d_obj, 4, 7, lupine_play_audio_3d);

STATIC mp_obj_t lupine_stop_audio(mp_obj_t uuid_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        const char* uuid = mp_obj_str_get_str(uuid_obj);
        api->StopAudio(uuid);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_stop_audio_obj, lupine_stop_audio);

STATIC mp_obj_t lupine_pause_audio(mp_obj_t uuid_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        const char* uuid = mp_obj_str_get_str(uuid_obj);
        api->PauseAudio(uuid);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_pause_audio_obj, lupine_pause_audio);

STATIC mp_obj_t lupine_resume_audio(mp_obj_t uuid_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        const char* uuid = mp_obj_str_get_str(uuid_obj);
        api->ResumeAudio(uuid);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_resume_audio_obj, lupine_resume_audio);

STATIC mp_obj_t lupine_set_bus_volume(mp_obj_t bus_obj, mp_obj_t volume_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        const char* bus = mp_obj_str_get_str(bus_obj);
        float volume = mp_obj_get_float(volume_obj);
        api->SetBusVolume(bus, volume);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_set_bus_volume_obj, lupine_set_bus_volume);

STATIC mp_obj_t lupine_get_bus_volume(mp_obj_t bus_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        const char* bus = mp_obj_str_get_str(bus_obj);
        return mp_obj_new_float(api->GetBusVolume(bus));
    }
    return mp_obj_new_float(1.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_get_bus_volume_obj, lupine_get_bus_volume);

STATIC mp_obj_t lupine_add_bus_effect(size_t, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        return mp_obj_new_int(api->AddBusEffect(mp_obj_str_get_str(args[0]), mp_obj_str_get_str(args[1])));
    }
    return mp_obj_new_int(-1);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_add_bus_effect_obj, 2, 2, lupine_add_bus_effect);

STATIC mp_obj_t lupine_remove_bus_effect(size_t, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        api->RemoveBusEffect(mp_obj_str_get_str(args[0]), mp_obj_get_int(args[1]));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_remove_bus_effect_obj, 2, 2, lupine_remove_bus_effect);

STATIC mp_obj_t lupine_move_bus_effect(size_t, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        api->MoveBusEffect(mp_obj_str_get_str(args[0]), mp_obj_get_int(args[1]), mp_obj_get_int(args[2]));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_move_bus_effect_obj, 3, 3, lupine_move_bus_effect);

STATIC mp_obj_t lupine_clear_bus_effects(size_t, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        api->ClearBusEffects(mp_obj_str_get_str(args[0]));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_clear_bus_effects_obj, 1, 1, lupine_clear_bus_effects);

STATIC mp_obj_t lupine_set_bus_effect_enabled(size_t, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        api->SetBusEffectEnabled(mp_obj_str_get_str(args[0]), mp_obj_get_int(args[1]), mp_obj_is_true(args[2]));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_set_bus_effect_enabled_obj, 3, 3, lupine_set_bus_effect_enabled);

STATIC mp_obj_t lupine_set_bus_effect_parameter(size_t, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        api->SetBusEffectParameter(mp_obj_str_get_str(args[0]), mp_obj_get_int(args[1]),
                                   mp_obj_str_get_str(args[2]), mp_obj_get_float(args[3]));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_set_bus_effect_parameter_obj, 4, 4, lupine_set_bus_effect_parameter);

STATIC mp_obj_t lupine_get_bus_effect_count(size_t, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    return mp_obj_new_int(api ? api->GetBusEffectCount(mp_obj_str_get_str(args[0])) : 0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_get_bus_effect_count_obj, 1, 1, lupine_get_bus_effect_count);

STATIC mp_obj_t lupine_get_bus_effect_parameter(size_t, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    return mp_obj_new_float(api ? api->GetBusEffectParameter(
        mp_obj_str_get_str(args[0]), mp_obj_get_int(args[1]), mp_obj_str_get_str(args[2])) : 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_get_bus_effect_parameter_obj, 3, 3, lupine_get_bus_effect_parameter);

STATIC mp_obj_t lupine_is_bus_effect_enabled(size_t, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    return mp_obj_new_bool(api ? api->IsBusEffectEnabled(mp_obj_str_get_str(args[0]), mp_obj_get_int(args[1])) : false);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_is_bus_effect_enabled_obj, 2, 2, lupine_is_bus_effect_enabled);

STATIC mp_obj_t lupine_get_bus_level(mp_obj_t bus_obj) {
    auto* api = GetCurrentScriptAPI();
    return mp_obj_new_float(api ? api->GetBusLevel(mp_obj_str_get_str(bus_obj)) : 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_get_bus_level_obj, lupine_get_bus_level);

STATIC mp_obj_t lupine_set_bus_muted(mp_obj_t bus_obj, mp_obj_t muted_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        const char* bus = mp_obj_str_get_str(bus_obj);
        api->SetBusMuted(bus, mp_obj_is_true(muted_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_set_bus_muted_obj, lupine_set_bus_muted);

STATIC mp_obj_t lupine_is_bus_muted(mp_obj_t bus_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        const char* bus = mp_obj_str_get_str(bus_obj);
        return api->IsBusMuted(bus) ? mp_const_true : mp_const_false;
    }
    return mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_is_bus_muted_obj, lupine_is_bus_muted);

STATIC mp_obj_t lupine_set_audio_source_volume(mp_obj_t uuid_obj, mp_obj_t volume_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->SetAudioSourceVolume(mp_obj_str_get_str(uuid_obj), mp_obj_get_float(volume_obj));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_set_audio_source_volume_obj, lupine_set_audio_source_volume);

STATIC mp_obj_t lupine_set_audio_source_pitch(mp_obj_t uuid_obj, mp_obj_t pitch_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->SetAudioSourcePitch(mp_obj_str_get_str(uuid_obj), mp_obj_get_float(pitch_obj));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_set_audio_source_pitch_obj, lupine_set_audio_source_pitch);

STATIC mp_obj_t lupine_set_audio_source_pan(mp_obj_t uuid_obj, mp_obj_t pan_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->SetAudioSourcePan(mp_obj_str_get_str(uuid_obj), mp_obj_get_float(pan_obj));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_set_audio_source_pan_obj, lupine_set_audio_source_pan);

STATIC mp_obj_t lupine_set_master_volume(mp_obj_t volume_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->SetMasterVolume(mp_obj_get_float(volume_obj));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_master_volume_obj, lupine_set_master_volume);

STATIC mp_obj_t lupine_get_master_volume(void) {
    auto* api = GetCurrentScriptAPI();
    return mp_obj_new_float(api ? api->GetMasterVolume() : 1.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_master_volume_obj, lupine_get_master_volume);

STATIC mp_obj_t lupine_set_master_muted(mp_obj_t muted_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->SetMasterMuted(mp_obj_is_true(muted_obj));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_master_muted_obj, lupine_set_master_muted);

STATIC mp_obj_t lupine_is_master_muted(void) {
    auto* api = GetCurrentScriptAPI();
    return (api && api->IsMasterMuted()) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_is_master_muted_obj, lupine_is_master_muted);

STATIC mp_obj_t lupine_set_listener_position(mp_obj_t x_obj, mp_obj_t y_obj, mp_obj_t z_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->SetListenerPosition(math::Vec3(mp_obj_get_float(x_obj), mp_obj_get_float(y_obj), mp_obj_get_float(z_obj)));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lupine_set_listener_position_obj, lupine_set_listener_position);

STATIC mp_obj_t lupine_set_listener_orientation(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 6) return mp_const_none;
    math::Vec3 forward(mp_obj_get_float(args[0]), mp_obj_get_float(args[1]), mp_obj_get_float(args[2]));
    math::Vec3 up(mp_obj_get_float(args[3]), mp_obj_get_float(args[4]), mp_obj_get_float(args[5]));
    api->SetListenerOrientation(forward, up);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_set_listener_orientation_obj, 6, 6, lupine_set_listener_orientation);

STATIC mp_obj_t lupine_set_listener_velocity(mp_obj_t x_obj, mp_obj_t y_obj, mp_obj_t z_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->SetListenerVelocity(math::Vec3(mp_obj_get_float(x_obj), mp_obj_get_float(y_obj), mp_obj_get_float(z_obj)));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lupine_set_listener_velocity_obj, lupine_set_listener_velocity);

STATIC mp_obj_t lupine_create_audio_bus(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 1) return mp_const_none;
    std::string parent = n_args > 1 ? mp_obj_str_get_str(args[1]) : "";
    api->CreateAudioBus(mp_obj_str_get_str(args[0]), parent);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_create_audio_bus_obj, 1, 2, lupine_create_audio_bus);

STATIC mp_obj_t lupine_destroy_audio_bus(mp_obj_t name_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->DestroyAudioBus(mp_obj_str_get_str(name_obj));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_destroy_audio_bus_obj, lupine_destroy_audio_bus);

STATIC mp_obj_t lupine_has_audio_bus(mp_obj_t name_obj) {
    auto* api = GetCurrentScriptAPI();
    return (api && api->HasAudioBus(mp_obj_str_get_str(name_obj))) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_has_audio_bus_obj, lupine_has_audio_bus);

STATIC mp_obj_t lupine_set_bus_solo(mp_obj_t name_obj, mp_obj_t solo_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->SetBusSolo(mp_obj_str_get_str(name_obj), mp_obj_is_true(solo_obj));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_set_bus_solo_obj, lupine_set_bus_solo);

STATIC mp_obj_t lupine_is_bus_solo(mp_obj_t name_obj) {
    auto* api = GetCurrentScriptAPI();
    return (api && api->IsBusSolo(mp_obj_str_get_str(name_obj))) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_is_bus_solo_obj, lupine_is_bus_solo);

// ---- Localization ----

static std::unordered_map<std::string, std::string> MpDictToArgs(mp_obj_t obj) {
    std::unordered_map<std::string, std::string> args;
    if (!mp_obj_is_type(obj, &mp_type_dict)) {
        return args;
    }
    mp_map_t* map = mp_obj_dict_get_map(obj);
    for (size_t i = 0; i < map->alloc; ++i) {
        if (!mp_map_slot_is_filled(map, i)) {
            continue;
        }
        mp_obj_t k = map->table[i].key;
        mp_obj_t v = map->table[i].value;
        if (!mp_obj_is_str(k)) {
            continue;
        }
        std::string key = mp_obj_str_get_str(k);
        std::string val;
        if (mp_obj_is_str(v)) {
            val = mp_obj_str_get_str(v);
        } else if (v == mp_const_true) {
            val = "true";
        } else if (v == mp_const_false) {
            val = "false";
        } else if (mp_obj_is_int(v)) {
            val = std::to_string(static_cast<long long>(mp_obj_get_int(v)));
        } else if (mp_obj_is_float(v)) {
            std::ostringstream ss;
            ss << static_cast<double>(mp_obj_get_float(v));
            val = ss.str();
        } else {
            continue;
        }
        args[key] = val;
    }
    return args;
}

STATIC mp_obj_t lupine_tr(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    const char* key = mp_obj_str_get_str(args[0]);
    if (!api) return mp_obj_new_str(key, strlen(key));
    std::string result = (n_args >= 2)
        ? api->TrInTable(key, mp_obj_str_get_str(args[1]))
        : api->Tr(key);
    return mp_obj_new_str(result.c_str(), result.size());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_tr_obj, 1, 2, lupine_tr);

STATIC mp_obj_t lupine_tr_fmt(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    const char* key = mp_obj_str_get_str(args[0]);
    if (!api) return mp_obj_new_str(key, strlen(key));
    std::unordered_map<std::string, std::string> argMap;
    if (n_args >= 2) argMap = MpDictToArgs(args[1]);
    std::string table = (n_args >= 3) ? mp_obj_str_get_str(args[2]) : "";
    std::string result = api->TrFormat(key, argMap, table);
    return mp_obj_new_str(result.c_str(), result.size());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_tr_fmt_obj, 1, 3, lupine_tr_fmt);

STATIC mp_obj_t lupine_tr_plural(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    const char* key = mp_obj_str_get_str(args[0]);
    if (!api) return mp_obj_new_str(key, strlen(key));
    long count = static_cast<long>(mp_obj_get_int(args[1]));
    std::unordered_map<std::string, std::string> argMap;
    if (n_args >= 3) argMap = MpDictToArgs(args[2]);
    std::string table = (n_args >= 4) ? mp_obj_str_get_str(args[3]) : "";
    std::string result = api->TrPlural(key, count, argMap, table);
    return mp_obj_new_str(result.c_str(), result.size());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_tr_plural_obj, 2, 4, lupine_tr_plural);

STATIC mp_obj_t lupine_set_locale(mp_obj_t locale_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->SetLocale(mp_obj_str_get_str(locale_obj));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_locale_obj, lupine_set_locale);

STATIC mp_obj_t lupine_get_locale(void) {
    auto* api = GetCurrentScriptAPI();
    std::string r = api ? api->GetLocale() : "";
    return mp_obj_new_str(r.c_str(), r.size());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_locale_obj, lupine_get_locale);

STATIC mp_obj_t lupine_get_fallback_locale(void) {
    auto* api = GetCurrentScriptAPI();
    std::string r = api ? api->GetFallbackLocale() : "";
    return mp_obj_new_str(r.c_str(), r.size());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_fallback_locale_obj, lupine_get_fallback_locale);

STATIC mp_obj_t lupine_get_locales(void) {
    auto* api = GetCurrentScriptAPI();
    mp_obj_t list = mp_obj_new_list(0, nullptr);
    if (api) {
        for (const std::string& locale : api->GetAvailableLocales()) {
            mp_obj_list_append(list, mp_obj_new_str(locale.c_str(), locale.size()));
        }
    }
    return list;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_locales_obj, lupine_get_locales);

STATIC mp_obj_t lupine_has_loc_key(mp_obj_t key_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api && api->HasLocaleKey(mp_obj_str_get_str(key_obj))) return mp_const_true;
    return mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_has_loc_key_obj, lupine_has_loc_key);

STATIC mp_obj_t lupine_reload_localization(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->ReloadLocalization();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_reload_localization_obj, lupine_reload_localization);

STATIC mp_obj_t lupine_set_pseudolocalization(mp_obj_t enabled_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->SetPseudolocalization(mp_obj_is_true(enabled_obj));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_pseudolocalization_obj, lupine_set_pseudolocalization);

STATIC mp_obj_t lupine_is_pseudolocalization(void) {
    auto* api = GetCurrentScriptAPI();
    return (api && api->IsPseudolocalization()) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_is_pseudolocalization_obj, lupine_is_pseudolocalization);

STATIC mp_obj_t lupine_set_theme(mp_obj_t path_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api && api->SetActiveTheme(mp_obj_str_get_str(path_obj))) return mp_const_true;
    return mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_theme_obj, lupine_set_theme);

STATIC mp_obj_t lupine_get_theme_color(mp_obj_t type_obj, mp_obj_t entry_obj) {
    auto* api = GetCurrentScriptAPI();
    if (!api) return make_color(1.0f, 1.0f, 1.0f, 1.0f);
    math::Color c = api->GetThemeColor(mp_obj_str_get_str(type_obj), mp_obj_str_get_str(entry_obj));
    return make_color(c.r, c.g, c.b, c.a);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_get_theme_color_obj, lupine_get_theme_color);

STATIC mp_obj_t lupine_get_theme_constant(mp_obj_t type_obj, mp_obj_t entry_obj) {
    auto* api = GetCurrentScriptAPI();
    if (!api) return mp_obj_new_float(0.0f);
    return mp_obj_new_float(api->GetThemeConstant(mp_obj_str_get_str(type_obj), mp_obj_str_get_str(entry_obj)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_get_theme_constant_obj, lupine_get_theme_constant);

STATIC mp_obj_t lupine_set_palette_color(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api) return mp_const_false;
    const char* key = mp_obj_str_get_str(args[0]);
    float r = mp_obj_get_float(args[1]);
    float g = mp_obj_get_float(args[2]);
    float b = mp_obj_get_float(args[3]);
    float a = (n_args >= 5) ? mp_obj_get_float(args[4]) : 1.0f;
    return api->SetThemePaletteColor(key, math::Color(r, g, b, a)) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_set_palette_color_obj, 4, 5, lupine_set_palette_color);

STATIC mp_obj_t lupine_set_theme_variable(mp_obj_t key_obj, mp_obj_t value_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api && api->SetThemeVariable(mp_obj_str_get_str(key_obj), mp_obj_get_float(value_obj))) return mp_const_true;
    return mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_set_theme_variable_obj, lupine_set_theme_variable);

STATIC mp_obj_t lupine_get_theme_version(void) {
    auto* api = GetCurrentScriptAPI();
    return mp_obj_new_int_from_uint(api ? api->GetThemeVersion() : 0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_theme_version_obj, lupine_get_theme_version);

STATIC mp_obj_t lupine_play_audio_scheduled(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 2) return mp_obj_new_str("", 0);

    const char* path = mp_obj_str_get_str(args[0]);
    float delay = mp_obj_get_float(args[1]);
    std::string bus = n_args > 2 ? mp_obj_str_get_str(args[2]) : "Master";
    bool loop = n_args > 3 ? mp_obj_is_true(args[3]) : false;
    float volume = n_args > 4 ? mp_obj_get_float(args[4]) : 1.0f;

    std::string uuid = api->PlayAudioScheduled(path, delay, bus, loop, volume);
    return mp_obj_new_str(uuid.c_str(), uuid.length());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_play_audio_scheduled_obj, 2, 5, lupine_play_audio_scheduled);

STATIC mp_obj_t lupine_play_audio_scheduled_3d(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 5) return mp_obj_new_str("", 0);

    const char* path = mp_obj_str_get_str(args[0]);
    float x = mp_obj_get_float(args[1]);
    float y = mp_obj_get_float(args[2]);
    float z = mp_obj_get_float(args[3]);
    float delay = mp_obj_get_float(args[4]);
    std::string bus = n_args > 5 ? mp_obj_str_get_str(args[5]) : "Master";
    bool loop = n_args > 6 ? mp_obj_is_true(args[6]) : false;
    float volume = n_args > 7 ? mp_obj_get_float(args[7]) : 1.0f;

    std::string uuid = api->PlayAudioScheduled3D(path, math::Vec3(x, y, z), delay, bus, loop, volume);
    return mp_obj_new_str(uuid.c_str(), uuid.length());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_play_audio_scheduled_3d_obj, 5, 8, lupine_play_audio_scheduled_3d);

STATIC mp_obj_t lupine_is_audio_playing(mp_obj_t uuid_obj) {
    auto* api = GetCurrentScriptAPI();
    if (!api) return mp_const_false;
    return api->IsAudioPlaying(mp_obj_str_get_str(uuid_obj)) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_is_audio_playing_obj, lupine_is_audio_playing);

STATIC mp_obj_t lupine_is_audio_finished(mp_obj_t uuid_obj) {
    auto* api = GetCurrentScriptAPI();
    if (!api) return mp_const_false;
    return api->IsAudioFinished(mp_obj_str_get_str(uuid_obj)) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_is_audio_finished_obj, lupine_is_audio_finished);

// ============================================================================
// UTILITY - TIME
// ============================================================================

STATIC mp_obj_t lupine_get_time(void) {
    auto* api = GetCurrentScriptAPI();
    return mp_obj_new_float(api ? api->GetTime() : 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_time_obj, lupine_get_time);

STATIC mp_obj_t lupine_get_frame_count(void) {
    auto* api = GetCurrentScriptAPI();
    return mp_obj_new_int(api ? api->GetFrameCount() : 0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_frame_count_obj, lupine_get_frame_count);

// ============================================================================
// ENGINE / OS INFO
// ============================================================================

STATIC ::lupine::profiling::ZoneCategory MicroPythonZoneCategory(const char* category) {
    if (!category) return ::lupine::profiling::ZoneCategory::User;
    std::string c(category);
    if (c == "Update") return ::lupine::profiling::ZoneCategory::Update;
    if (c == "Physics") return ::lupine::profiling::ZoneCategory::Physics;
    if (c == "Render") return ::lupine::profiling::ZoneCategory::Render;
    if (c == "Scripting") return ::lupine::profiling::ZoneCategory::Scripting;
    if (c == "Audio") return ::lupine::profiling::ZoneCategory::Audio;
    return ::lupine::profiling::ZoneCategory::User;
}

STATIC mp_obj_t lupine_profiler_begin_zone(size_t n_args, const mp_obj_t* args) {
    const char* name = mp_obj_str_get_str(args[0]);
    const char* category = (n_args > 1) ? mp_obj_str_get_str(args[1]) : nullptr;
    ::lupine::profiling::Profiler::Get().BeginZone(name ? name : "", MicroPythonZoneCategory(category));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_profiler_begin_zone_obj, 1, 2, lupine_profiler_begin_zone);

STATIC mp_obj_t lupine_profiler_end_zone(void) {
    ::lupine::profiling::Profiler::Get().EndZone();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_profiler_end_zone_obj, lupine_profiler_end_zone);

STATIC mp_obj_t lupine_profiler_set_counter(mp_obj_t name_obj, mp_obj_t value_obj) {
    const char* name = mp_obj_str_get_str(name_obj);
    double value = mp_obj_get_float(value_obj);
    ::lupine::profiling::Profiler::Get().SetCounter(name ? name : "", value);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_profiler_set_counter_obj, lupine_profiler_set_counter);

STATIC mp_obj_t lupine_profiler_is_enabled(void) {
    return mp_obj_new_bool(::lupine::profiling::Profiler::Get().IsEnabled());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_profiler_is_enabled_obj, lupine_profiler_is_enabled);

STATIC mp_obj_t lupine_profiler_set_enabled(mp_obj_t enabled_obj) {
    ::lupine::profiling::Profiler::Get().SetEnabled(mp_obj_is_true(enabled_obj));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_profiler_set_enabled_obj, lupine_profiler_set_enabled);

STATIC mp_obj_t lupine_profiler_frame_ms(void) {
    return mp_obj_new_float(::lupine::profiling::Profiler::Get().GetAverageFrameMs());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_profiler_frame_ms_obj, lupine_profiler_frame_ms);

STATIC mp_obj_t lupine_get_fps(void) {
    auto* api = GetCurrentScriptAPI();
    return mp_obj_new_float(api ? api->GetFPS() : 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_fps_obj, lupine_get_fps);

STATIC mp_obj_t lupine_get_ticks_msec(void) {
    auto* api = GetCurrentScriptAPI();
    return mp_obj_new_int(api ? api->GetTicksMsec() : 0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_ticks_msec_obj, lupine_get_ticks_msec);

STATIC mp_obj_t lupine_get_unix_time(void) {
    auto* api = GetCurrentScriptAPI();
    return mp_obj_new_float(api ? static_cast<mp_float_t>(api->GetUnixTime()) : 0.0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_unix_time_obj, lupine_get_unix_time);

STATIC mp_obj_t lupine_get_platform_name(void) {
    auto* api = GetCurrentScriptAPI();
    std::string name = api ? api->GetPlatformName() : "";
    return mp_obj_new_str(name.c_str(), name.size());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_platform_name_obj, lupine_get_platform_name);

STATIC mp_obj_t lupine_is_debug_build(void) {
    auto* api = GetCurrentScriptAPI();
    return (api && api->IsDebugBuild()) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_is_debug_build_obj, lupine_is_debug_build);

STATIC mp_obj_t lupine_get_dpi_scale(void) {
    auto* api = GetCurrentScriptAPI();
    return mp_obj_new_float(api ? api->GetDPIScale() : 1.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_dpi_scale_obj, lupine_get_dpi_scale);

STATIC mp_obj_t lupine_open_url(mp_obj_t url_obj) {
    auto* api = GetCurrentScriptAPI();
    return (api && api->OpenURL(mp_obj_str_get_str(url_obj))) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_open_url_obj, lupine_open_url);

// ============================================================================
// COLOR HELPERS
// ============================================================================

STATIC mp_obj_t lupine_color_from_hex(mp_obj_t hex_obj) {
    auto* api = GetCurrentScriptAPI();
    if (!api) return make_color(0.0f, 0.0f, 0.0f, 1.0f);
    math::Color c = api->ColorFromHex(mp_obj_str_get_str(hex_obj));
    return make_color(c.r, c.g, c.b, c.a);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_color_from_hex_obj, lupine_color_from_hex);

STATIC mp_obj_t lupine_color_to_hex(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 4) return mp_obj_new_str("", 0);
    math::Color c(mp_obj_get_float(args[0]), mp_obj_get_float(args[1]),
                  mp_obj_get_float(args[2]), mp_obj_get_float(args[3]));
    std::string hex = api->ColorToHex(c);
    return mp_obj_new_str(hex.c_str(), hex.size());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_color_to_hex_obj, 4, 4, lupine_color_to_hex);

STATIC mp_obj_t lupine_color_from_hsv(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 3) return make_color(0.0f, 0.0f, 0.0f, 1.0f);
    float a = n_args > 3 ? mp_obj_get_float(args[3]) : 1.0f;
    math::Color c = api->ColorFromHSV(mp_obj_get_float(args[0]), mp_obj_get_float(args[1]),
                                      mp_obj_get_float(args[2]), a);
    return make_color(c.r, c.g, c.b, c.a);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_color_from_hsv_obj, 3, 4, lupine_color_from_hsv);

STATIC mp_obj_t lupine_color_lerp(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 9) return make_color(0.0f, 0.0f, 0.0f, 1.0f);
    math::Color a(mp_obj_get_float(args[0]), mp_obj_get_float(args[1]),
                  mp_obj_get_float(args[2]), mp_obj_get_float(args[3]));
    math::Color b(mp_obj_get_float(args[4]), mp_obj_get_float(args[5]),
                  mp_obj_get_float(args[6]), mp_obj_get_float(args[7]));
    math::Color c = api->ColorLerp(a, b, mp_obj_get_float(args[8]));
    return make_color(c.r, c.g, c.b, c.a);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_color_lerp_obj, 9, 9, lupine_color_lerp);

// ============================================================================
// UTILITY - RANDOM
// ============================================================================

STATIC mp_obj_t lupine_random_range(mp_obj_t min_obj, mp_obj_t max_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        float min = mp_obj_get_float(min_obj);
        float max = mp_obj_get_float(max_obj);
        return mp_obj_new_float(api->RandomRange(min, max));
    }
    return mp_obj_new_float(0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_random_range_obj, lupine_random_range);

STATIC mp_obj_t lupine_random_range_int(mp_obj_t min_obj, mp_obj_t max_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        int min = mp_obj_get_int(min_obj);
        int max = mp_obj_get_int(max_obj);
        return mp_obj_new_int(api->RandomRangeInt(min, max));
    }
    return mp_obj_new_int(0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_random_range_int_obj, lupine_random_range_int);

STATIC mp_obj_t lupine_random_float(void) {
    auto* api = GetCurrentScriptAPI();
    return mp_obj_new_float(api ? api->RandomFloat() : 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_random_float_obj, lupine_random_float);

STATIC mp_obj_t lupine_random_bool(void) {
    auto* api = GetCurrentScriptAPI();
    return (api && api->RandomBool()) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_random_bool_obj, lupine_random_bool);

STATIC mp_obj_t lupine_random_sign(void) {
    auto* api = GetCurrentScriptAPI();
    return mp_obj_new_int(api ? api->RandomSign() : 1);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_random_sign_obj, lupine_random_sign);

STATIC mp_obj_t lupine_random_seed(mp_obj_t seed_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->RandomSeed(mp_obj_get_int(seed_obj));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_random_seed_obj, lupine_random_seed);

// ============================================================================
// UTILITY - MATH
// ============================================================================

STATIC mp_obj_t lupine_lerp(mp_obj_t a_obj, mp_obj_t b_obj, mp_obj_t t_obj) {
    auto* api = GetCurrentScriptAPI();
    float a = mp_obj_get_float(a_obj);
    float b = mp_obj_get_float(b_obj);
    float t = mp_obj_get_float(t_obj);
    return mp_obj_new_float(api ? api->Lerp(a, b, t) : (a + (b - a) * t));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lupine_lerp_obj, lupine_lerp);

STATIC mp_obj_t lupine_clamp(mp_obj_t value_obj, mp_obj_t min_obj, mp_obj_t max_obj) {
    auto* api = GetCurrentScriptAPI();
    float value = mp_obj_get_float(value_obj);
    float min = mp_obj_get_float(min_obj);
    float max = mp_obj_get_float(max_obj);
    return mp_obj_new_float(api ? api->Clamp(value, min, max) : std::fmax(min, std::fmin(max, value)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lupine_clamp_obj, lupine_clamp);

STATIC mp_obj_t lupine_abs(mp_obj_t value_obj) {
    auto* api = GetCurrentScriptAPI();
    float value = mp_obj_get_float(value_obj);
    return mp_obj_new_float(api ? api->Abs(value) : std::fabs(value));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_abs_obj, lupine_abs);

STATIC mp_obj_t lupine_sign(mp_obj_t value_obj) {
    auto* api = GetCurrentScriptAPI();
    float value = mp_obj_get_float(value_obj);
    return mp_obj_new_float(api ? api->Sign(value) : (value > 0 ? 1.0f : (value < 0 ? -1.0f : 0.0f)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_sign_obj, lupine_sign);

STATIC mp_obj_t lupine_move_toward(mp_obj_t from_obj, mp_obj_t to_obj, mp_obj_t delta_obj) {
    auto* api = GetCurrentScriptAPI();
    float from = mp_obj_get_float(from_obj);
    float to = mp_obj_get_float(to_obj);
    float delta = mp_obj_get_float(delta_obj);
    return mp_obj_new_float(api ? api->MoveToward(from, to, delta) : from);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lupine_move_toward_obj, lupine_move_toward);

STATIC mp_obj_t lupine_lerp_angle(mp_obj_t from_obj, mp_obj_t to_obj, mp_obj_t weight_obj) {
    auto* api = GetCurrentScriptAPI();
    float from = mp_obj_get_float(from_obj);
    float to = mp_obj_get_float(to_obj);
    float weight = mp_obj_get_float(weight_obj);
    return mp_obj_new_float(api ? api->LerpAngle(from, to, weight) : from);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lupine_lerp_angle_obj, lupine_lerp_angle);

STATIC mp_obj_t lupine_angle_difference(mp_obj_t from_obj, mp_obj_t to_obj) {
    auto* api = GetCurrentScriptAPI();
    float from = mp_obj_get_float(from_obj);
    float to = mp_obj_get_float(to_obj);
    return mp_obj_new_float(api ? api->AngleDifference(from, to) : 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_angle_difference_obj, lupine_angle_difference);

STATIC mp_obj_t lupine_smoothstep(mp_obj_t from_obj, mp_obj_t to_obj, mp_obj_t t_obj) {
    auto* api = GetCurrentScriptAPI();
    float from = mp_obj_get_float(from_obj);
    float to = mp_obj_get_float(to_obj);
    float t = mp_obj_get_float(t_obj);
    return mp_obj_new_float(api ? api->Smoothstep(from, to, t) : from);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lupine_smoothstep_obj, lupine_smoothstep);

STATIC mp_obj_t lupine_inverse_lerp(mp_obj_t from_obj, mp_obj_t to_obj, mp_obj_t value_obj) {
    auto* api = GetCurrentScriptAPI();
    float from = mp_obj_get_float(from_obj);
    float to = mp_obj_get_float(to_obj);
    float value = mp_obj_get_float(value_obj);
    return mp_obj_new_float(api ? api->InverseLerp(from, to, value) : 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lupine_inverse_lerp_obj, lupine_inverse_lerp);

STATIC mp_obj_t lupine_remap(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 5) return mp_obj_new_float(0.0f);

    float value = mp_obj_get_float(args[0]);
    float fromMin = mp_obj_get_float(args[1]);
    float fromMax = mp_obj_get_float(args[2]);
    float toMin = mp_obj_get_float(args[3]);
    float toMax = mp_obj_get_float(args[4]);

    return mp_obj_new_float(api->Remap(value, fromMin, fromMax, toMin, toMax));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_remap_obj, 5, 5, lupine_remap);

STATIC mp_obj_t lupine_deg_to_rad(mp_obj_t degrees_obj) {
    auto* api = GetCurrentScriptAPI();
    float degrees = mp_obj_get_float(degrees_obj);
    return mp_obj_new_float(api ? api->DegToRad(degrees) : degrees);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_deg_to_rad_obj, lupine_deg_to_rad);

STATIC mp_obj_t lupine_rad_to_deg(mp_obj_t radians_obj) {
    auto* api = GetCurrentScriptAPI();
    float radians = mp_obj_get_float(radians_obj);
    return mp_obj_new_float(api ? api->RadToDeg(radians) : radians);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_rad_to_deg_obj, lupine_rad_to_deg);

STATIC mp_obj_t lupine_wrap(mp_obj_t value_obj, mp_obj_t min_obj, mp_obj_t max_obj) {
    auto* api = GetCurrentScriptAPI();
    float value = mp_obj_get_float(value_obj);
    float min = mp_obj_get_float(min_obj);
    float max = mp_obj_get_float(max_obj);
    return mp_obj_new_float(api ? api->Wrap(value, min, max) : value);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lupine_wrap_obj, lupine_wrap);

STATIC mp_obj_t lupine_wrap_int(mp_obj_t value_obj, mp_obj_t min_obj, mp_obj_t max_obj) {
    auto* api = GetCurrentScriptAPI();
    int value = mp_obj_get_int(value_obj);
    int min = mp_obj_get_int(min_obj);
    int max = mp_obj_get_int(max_obj);
    return mp_obj_new_int(api ? api->WrapInt(value, min, max) : value);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lupine_wrap_int_obj, lupine_wrap_int);

STATIC mp_obj_t lupine_ping_pong(mp_obj_t value_obj, mp_obj_t length_obj) {
    auto* api = GetCurrentScriptAPI();
    float value = mp_obj_get_float(value_obj);
    float length = mp_obj_get_float(length_obj);
    return mp_obj_new_float(api ? api->PingPong(value, length) : value);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_ping_pong_obj, lupine_ping_pong);

STATIC mp_obj_t lupine_snapped(mp_obj_t value_obj, mp_obj_t step_obj) {
    auto* api = GetCurrentScriptAPI();
    float value = mp_obj_get_float(value_obj);
    float step = mp_obj_get_float(step_obj);
    return mp_obj_new_float(api ? api->Snapped(value, step) : value);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_snapped_obj, lupine_snapped);

STATIC mp_obj_t lupine_is_equal_approx(mp_obj_t a_obj, mp_obj_t b_obj) {
    auto* api = GetCurrentScriptAPI();
    float a = mp_obj_get_float(a_obj);
    float b = mp_obj_get_float(b_obj);
    return (api && api->IsEqualApprox(a, b)) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_is_equal_approx_obj, lupine_is_equal_approx);

STATIC mp_obj_t lupine_ease(mp_obj_t t_obj, mp_obj_t curve_obj) {
    auto* api = GetCurrentScriptAPI();
    float t = mp_obj_get_float(t_obj);
    float curve = mp_obj_get_float(curve_obj);
    return mp_obj_new_float(api ? api->Ease(t, curve) : t);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_ease_obj, lupine_ease);

STATIC mp_obj_t lupine_pos_mod(mp_obj_t a_obj, mp_obj_t b_obj) {
    auto* api = GetCurrentScriptAPI();
    float a = mp_obj_get_float(a_obj);
    float b = mp_obj_get_float(b_obj);
    return mp_obj_new_float(api ? api->PosMod(a, b) : a);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_pos_mod_obj, lupine_pos_mod);

STATIC mp_obj_t lupine_pos_mod_int(mp_obj_t a_obj, mp_obj_t b_obj) {
    auto* api = GetCurrentScriptAPI();
    int a = mp_obj_get_int(a_obj);
    int b = mp_obj_get_int(b_obj);
    return mp_obj_new_int(api ? api->PosModInt(a, b) : a);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_pos_mod_int_obj, lupine_pos_mod_int);

// ============================================================================
// UTILITY - VECTOR MATH
// ============================================================================

STATIC mp_obj_t lupine_normalize_2d(mp_obj_t vec_obj) {
    auto* api = GetCurrentScriptAPI();
    float x, y;
    if (api && get_vec2(vec_obj, x, y)) {
        auto result = api->Normalize2D(math::Vec2(x, y));
        return make_vec2(result.x, result.y);
    }
    return make_vec2(0.0f, 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_normalize_2d_obj, lupine_normalize_2d);

STATIC mp_obj_t lupine_normalize_3d(mp_obj_t vec_obj) {
    auto* api = GetCurrentScriptAPI();
    float x, y, z;
    if (api && get_vec3(vec_obj, x, y, z)) {
        auto result = api->Normalize3D(math::Vec3(x, y, z));
        return make_vec3(result.x, result.y, result.z);
    }
    return make_vec3(0.0f, 0.0f, 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_normalize_3d_obj, lupine_normalize_3d);

STATIC mp_obj_t lupine_length_2d(mp_obj_t vec_obj) {
    auto* api = GetCurrentScriptAPI();
    float x, y;
    if (api && get_vec2(vec_obj, x, y)) {
        return mp_obj_new_float(api->Length2D(math::Vec2(x, y)));
    }
    return mp_obj_new_float(0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_length_2d_obj, lupine_length_2d);

STATIC mp_obj_t lupine_length_3d(mp_obj_t vec_obj) {
    auto* api = GetCurrentScriptAPI();
    float x, y, z;
    if (api && get_vec3(vec_obj, x, y, z)) {
        return mp_obj_new_float(api->Length3D(math::Vec3(x, y, z)));
    }
    return mp_obj_new_float(0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_length_3d_obj, lupine_length_3d);

STATIC mp_obj_t lupine_dot_2d(mp_obj_t a_obj, mp_obj_t b_obj) {
    auto* api = GetCurrentScriptAPI();
    float ax, ay, bx, by;
    if (api && get_vec2(a_obj, ax, ay) && get_vec2(b_obj, bx, by)) {
        return mp_obj_new_float(api->Dot2D(math::Vec2(ax, ay), math::Vec2(bx, by)));
    }
    return mp_obj_new_float(0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_dot_2d_obj, lupine_dot_2d);

STATIC mp_obj_t lupine_dot_3d(mp_obj_t a_obj, mp_obj_t b_obj) {
    auto* api = GetCurrentScriptAPI();
    float ax, ay, az, bx, by, bz;
    if (api && get_vec3(a_obj, ax, ay, az) && get_vec3(b_obj, bx, by, bz)) {
        return mp_obj_new_float(api->Dot3D(math::Vec3(ax, ay, az), math::Vec3(bx, by, bz)));
    }
    return mp_obj_new_float(0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_dot_3d_obj, lupine_dot_3d);

STATIC mp_obj_t lupine_cross(mp_obj_t a_obj, mp_obj_t b_obj) {
    auto* api = GetCurrentScriptAPI();
    float ax, ay, az, bx, by, bz;
    if (api && get_vec3(a_obj, ax, ay, az) && get_vec3(b_obj, bx, by, bz)) {
        auto result = api->Cross(math::Vec3(ax, ay, az), math::Vec3(bx, by, bz));
        return make_vec3(result.x, result.y, result.z);
    }
    return make_vec3(0.0f, 0.0f, 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_cross_obj, lupine_cross);

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

STATIC mp_obj_t lupine_get_global_int(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 1) return mp_obj_new_int(0);

    const char* name = mp_obj_str_get_str(args[0]);
    int defaultValue = n_args > 1 ? mp_obj_get_int(args[1]) : 0;
    return mp_obj_new_int(api->GetGlobalInt(name, defaultValue));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_get_global_int_obj, 1, 2, lupine_get_global_int);

STATIC mp_obj_t lupine_get_global_float(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 1) return mp_obj_new_float(0.0f);

    const char* name = mp_obj_str_get_str(args[0]);
    float defaultValue = n_args > 1 ? mp_obj_get_float(args[1]) : 0.0f;
    return mp_obj_new_float(api->GetGlobalFloat(name, defaultValue));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_get_global_float_obj, 1, 2, lupine_get_global_float);

STATIC mp_obj_t lupine_get_global_string(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 1) return mp_obj_new_str("", 0);

    const char* name = mp_obj_str_get_str(args[0]);
    std::string defaultValue = n_args > 1 ? mp_obj_str_get_str(args[1]) : "";
    std::string result = api->GetGlobalString(name, defaultValue);
    return mp_obj_new_str(result.c_str(), result.length());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_get_global_string_obj, 1, 2, lupine_get_global_string);

STATIC mp_obj_t lupine_get_global_bool(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 1) return mp_const_false;

    const char* name = mp_obj_str_get_str(args[0]);
    bool defaultValue = n_args > 1 ? mp_obj_is_true(args[1]) : false;
    return api->GetGlobalBool(name, defaultValue) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_get_global_bool_obj, 1, 2, lupine_get_global_bool);

STATIC mp_obj_t lupine_set_global_int(mp_obj_t name_obj, mp_obj_t value_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        const char* name = mp_obj_str_get_str(name_obj);
        api->SetGlobalInt(name, mp_obj_get_int(value_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_set_global_int_obj, lupine_set_global_int);

STATIC mp_obj_t lupine_set_global_float(mp_obj_t name_obj, mp_obj_t value_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        const char* name = mp_obj_str_get_str(name_obj);
        api->SetGlobalFloat(name, mp_obj_get_float(value_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_set_global_float_obj, lupine_set_global_float);

STATIC mp_obj_t lupine_set_global_string(mp_obj_t name_obj, mp_obj_t value_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        const char* name = mp_obj_str_get_str(name_obj);
        const char* value = mp_obj_str_get_str(value_obj);
        api->SetGlobalString(name, value);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_set_global_string_obj, lupine_set_global_string);

STATIC mp_obj_t lupine_set_global_bool(mp_obj_t name_obj, mp_obj_t value_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        const char* name = mp_obj_str_get_str(name_obj);
        api->SetGlobalBool(name, mp_obj_is_true(value_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_set_global_bool_obj, lupine_set_global_bool);

// ============================================================================
// TIMERS
// ============================================================================

// Timer creation/management functions are defined in the object model section
// below (they return LupineTimer handles, so they must follow the timer type).

// ============================================================================
// ASSET LOADING
// ============================================================================

STATIC mp_obj_t lupine_load_image_asset(mp_obj_t path_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        const char* path = mp_obj_str_get_str(path_obj);
        return api->LoadImageAsset(path) ? mp_const_true : mp_const_false;
    }
    return mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_load_image_asset_obj, lupine_load_image_asset);

STATIC mp_obj_t lupine_load_audio_asset(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 1) return mp_const_false;

    const char* path = mp_obj_str_get_str(args[0]);
    std::string loadMode = n_args > 1 ? mp_obj_str_get_str(args[1]) : "preload";
    return api->LoadAudioAsset(path, loadMode) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_load_audio_asset_obj, 1, 2, lupine_load_audio_asset);

STATIC mp_obj_t lupine_load_model_asset(mp_obj_t path_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        const char* path = mp_obj_str_get_str(path_obj);
        return api->LoadModelAsset(path) ? mp_const_true : mp_const_false;
    }
    return mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_load_model_asset_obj, lupine_load_model_asset);

STATIC mp_obj_t lupine_preload_assets(mp_obj_t paths_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        std::vector<std::string> paths;
        size_t len = 0;
        mp_obj_t* items = nullptr;
        mp_obj_get_array(paths_obj, &len, &items);
        paths.reserve(len);
        for (size_t i = 0; i < len; ++i) {
            paths.emplace_back(mp_obj_str_get_str(items[i]));
        }
        api->PreloadAssets(paths);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_preload_assets_obj, lupine_preload_assets);

// ============================================================================
// PHYSICS - RAYCAST 2D
// ============================================================================

// Helper to create a raycast hit dictionary
static mp_obj_t make_raycast_hit_2d(const ScriptAPI::RaycastHit2D& hit) {
    mp_obj_t dict = mp_obj_new_dict(5);
    mp_obj_dict_store(dict, mp_obj_new_str("hit", 3), hit.hit ? mp_const_true : mp_const_false);
    mp_obj_dict_store(dict, mp_obj_new_str("point", 5), make_vec2(hit.point.x, hit.point.y));
    mp_obj_dict_store(dict, mp_obj_new_str("normal", 6), make_vec2(hit.normal.x, hit.normal.y));
    mp_obj_dict_store(dict, mp_obj_new_str("distance", 8), mp_obj_new_float(hit.distance));
    // Note: collider node pointer not exposed to Python (complex to handle safely)
    return dict;
}

STATIC mp_obj_t lupine_raycast_2d(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 5) {
        return make_raycast_hit_2d(ScriptAPI::RaycastHit2D());
    }

    float fromX = mp_obj_get_float(args[0]);
    float fromY = mp_obj_get_float(args[1]);
    float dirX = mp_obj_get_float(args[2]);
    float dirY = mp_obj_get_float(args[3]);
    float maxDist = mp_obj_get_float(args[4]);
    uint32_t mask = n_args > 5 ? static_cast<uint32_t>(mp_obj_get_int(args[5])) : 0xFFFFFFFF;

    auto hit = api->Raycast2D(math::Vec2(fromX, fromY), math::Vec2(dirX, dirY), maxDist, mask);
    return make_raycast_hit_2d(hit);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_raycast_2d_obj, 5, 6, lupine_raycast_2d);

STATIC mp_obj_t lupine_raycast_all_2d(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 5) {
        return mp_obj_new_list(0, nullptr);
    }

    float fromX = mp_obj_get_float(args[0]);
    float fromY = mp_obj_get_float(args[1]);
    float dirX = mp_obj_get_float(args[2]);
    float dirY = mp_obj_get_float(args[3]);
    float maxDist = mp_obj_get_float(args[4]);
    uint32_t mask = n_args > 5 ? static_cast<uint32_t>(mp_obj_get_int(args[5])) : 0xFFFFFFFF;

    auto hits = api->RaycastAll2D(math::Vec2(fromX, fromY), math::Vec2(dirX, dirY), maxDist, mask);
    mp_obj_t list = mp_obj_new_list(0, nullptr);
    for (const auto& hit : hits) {
        mp_obj_list_append(list, make_raycast_hit_2d(hit));
    }
    return list;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_raycast_all_2d_obj, 5, 6, lupine_raycast_all_2d);

static mp_obj_t make_shape_cast_hit_2d(const ScriptAPI::ShapeCastHit2D& hit) {
    mp_obj_t dict = mp_obj_new_dict(4);
    mp_obj_dict_store(dict, mp_obj_new_str("hit", 3), hit.hit ? mp_const_true : mp_const_false);
    mp_obj_dict_store(dict, mp_obj_new_str("point", 5), make_vec2(hit.point.x, hit.point.y));
    mp_obj_dict_store(dict, mp_obj_new_str("normal", 6), make_vec2(hit.normal.x, hit.normal.y));
    mp_obj_dict_store(dict, mp_obj_new_str("fraction", 8), mp_obj_new_float(hit.fraction));
    return dict;
}

STATIC mp_obj_t lupine_circle_cast_2d(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 5) {
        return make_shape_cast_hit_2d(ScriptAPI::ShapeCastHit2D());
    }

    float fromX = mp_obj_get_float(args[0]);
    float fromY = mp_obj_get_float(args[1]);
    float toX = mp_obj_get_float(args[2]);
    float toY = mp_obj_get_float(args[3]);
    float radius = mp_obj_get_float(args[4]);
    uint32_t mask = n_args > 5 ? static_cast<uint32_t>(mp_obj_get_int(args[5])) : 0xFFFFFFFF;

    auto hit = api->CircleCast2D(math::Vec2(fromX, fromY), math::Vec2(toX, toY), radius, mask);
    return make_shape_cast_hit_2d(hit);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_circle_cast_2d_obj, 5, 6, lupine_circle_cast_2d);

// ============================================================================
// PHYSICS - RAYCAST 3D
// ============================================================================

static mp_obj_t make_raycast_hit_3d(const ScriptAPI::RaycastHit3D& hit) {
    mp_obj_t dict = mp_obj_new_dict(5);
    mp_obj_dict_store(dict, mp_obj_new_str("hit", 3), hit.hit ? mp_const_true : mp_const_false);
    mp_obj_dict_store(dict, mp_obj_new_str("point", 5), make_vec3(hit.point.x, hit.point.y, hit.point.z));
    mp_obj_dict_store(dict, mp_obj_new_str("normal", 6), make_vec3(hit.normal.x, hit.normal.y, hit.normal.z));
    mp_obj_dict_store(dict, mp_obj_new_str("distance", 8), mp_obj_new_float(hit.distance));
    return dict;
}

STATIC mp_obj_t lupine_raycast_3d(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 7) {
        return make_raycast_hit_3d(ScriptAPI::RaycastHit3D());
    }

    float fromX = mp_obj_get_float(args[0]);
    float fromY = mp_obj_get_float(args[1]);
    float fromZ = mp_obj_get_float(args[2]);
    float dirX = mp_obj_get_float(args[3]);
    float dirY = mp_obj_get_float(args[4]);
    float dirZ = mp_obj_get_float(args[5]);
    float maxDist = mp_obj_get_float(args[6]);
    uint32_t mask = n_args > 7 ? static_cast<uint32_t>(mp_obj_get_int(args[7])) : 0xFFFFFFFF;

    auto hit = api->Raycast3D(math::Vec3(fromX, fromY, fromZ), math::Vec3(dirX, dirY, dirZ), maxDist, mask);
    return make_raycast_hit_3d(hit);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_raycast_3d_obj, 7, 8, lupine_raycast_3d);

STATIC mp_obj_t lupine_raycast_all_3d(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 7) {
        return mp_obj_new_list(0, nullptr);
    }

    float fromX = mp_obj_get_float(args[0]);
    float fromY = mp_obj_get_float(args[1]);
    float fromZ = mp_obj_get_float(args[2]);
    float dirX = mp_obj_get_float(args[3]);
    float dirY = mp_obj_get_float(args[4]);
    float dirZ = mp_obj_get_float(args[5]);
    float maxDist = mp_obj_get_float(args[6]);
    uint32_t mask = n_args > 7 ? static_cast<uint32_t>(mp_obj_get_int(args[7])) : 0xFFFFFFFF;

    auto hits = api->RaycastAll3D(math::Vec3(fromX, fromY, fromZ), math::Vec3(dirX, dirY, dirZ), maxDist, mask);
    mp_obj_t list = mp_obj_new_list(0, nullptr);
    for (const auto& hit : hits) {
        mp_obj_list_append(list, make_raycast_hit_3d(hit));
    }
    return list;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_raycast_all_3d_obj, 7, 8, lupine_raycast_all_3d);

static mp_obj_t make_shape_cast_hit_3d(const ScriptAPI::ShapeCastHit3D& hit) {
    mp_obj_t dict = mp_obj_new_dict(4);
    mp_obj_dict_store(dict, mp_obj_new_str("hit", 3), hit.hit ? mp_const_true : mp_const_false);
    mp_obj_dict_store(dict, mp_obj_new_str("point", 5), make_vec3(hit.point.x, hit.point.y, hit.point.z));
    mp_obj_dict_store(dict, mp_obj_new_str("normal", 6), make_vec3(hit.normal.x, hit.normal.y, hit.normal.z));
    mp_obj_dict_store(dict, mp_obj_new_str("fraction", 8), mp_obj_new_float(hit.fraction));
    return dict;
}

STATIC mp_obj_t lupine_sphere_cast_3d(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 7) {
        return make_shape_cast_hit_3d(ScriptAPI::ShapeCastHit3D());
    }

    float fromX = mp_obj_get_float(args[0]);
    float fromY = mp_obj_get_float(args[1]);
    float fromZ = mp_obj_get_float(args[2]);
    float toX = mp_obj_get_float(args[3]);
    float toY = mp_obj_get_float(args[4]);
    float toZ = mp_obj_get_float(args[5]);
    float radius = mp_obj_get_float(args[6]);
    uint32_t mask = n_args > 7 ? static_cast<uint32_t>(mp_obj_get_int(args[7])) : 0xFFFFFFFF;

    auto hit = api->SphereCast3D(math::Vec3(fromX, fromY, fromZ), math::Vec3(toX, toY, toZ), radius, mask);
    return make_shape_cast_hit_3d(hit);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_sphere_cast_3d_obj, 7, 8, lupine_sphere_cast_3d);

// ============================================================================
// PHYSICS - OVERLAP QUERIES
// ============================================================================

STATIC mp_obj_t lupine_overlap_circle(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 3) return mp_obj_new_list(0, nullptr);

    float x = mp_obj_get_float(args[0]);
    float y = mp_obj_get_float(args[1]);
    float radius = mp_obj_get_float(args[2]);
    uint32_t mask = n_args > 3 ? static_cast<uint32_t>(mp_obj_get_int(args[3])) : 0xFFFFFFFF;

    auto nodes = api->OverlapCircle(math::Vec2(x, y), radius, mask);

    // Return count of overlapping nodes (node pointers not exposed)
    return mp_obj_new_int(static_cast<int>(nodes.size()));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_overlap_circle_obj, 3, 4, lupine_overlap_circle);

STATIC mp_obj_t lupine_overlap_rect(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 4) return mp_obj_new_int(0);

    float x = mp_obj_get_float(args[0]);
    float y = mp_obj_get_float(args[1]);
    float halfW = mp_obj_get_float(args[2]);
    float halfH = mp_obj_get_float(args[3]);
    uint32_t mask = n_args > 4 ? static_cast<uint32_t>(mp_obj_get_int(args[4])) : 0xFFFFFFFF;

    auto nodes = api->OverlapRect(math::Vec2(x, y), math::Vec2(halfW, halfH), mask);
    return mp_obj_new_int(static_cast<int>(nodes.size()));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_overlap_rect_obj, 4, 5, lupine_overlap_rect);

STATIC mp_obj_t lupine_overlap_sphere(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 4) return mp_obj_new_int(0);

    float x = mp_obj_get_float(args[0]);
    float y = mp_obj_get_float(args[1]);
    float z = mp_obj_get_float(args[2]);
    float radius = mp_obj_get_float(args[3]);
    uint32_t mask = n_args > 4 ? static_cast<uint32_t>(mp_obj_get_int(args[4])) : 0xFFFFFFFF;

    auto nodes = api->OverlapSphere(math::Vec3(x, y, z), radius, mask);
    return mp_obj_new_int(static_cast<int>(nodes.size()));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_overlap_sphere_obj, 4, 5, lupine_overlap_sphere);

STATIC mp_obj_t lupine_overlap_box(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 6) return mp_obj_new_int(0);

    float x = mp_obj_get_float(args[0]);
    float y = mp_obj_get_float(args[1]);
    float z = mp_obj_get_float(args[2]);
    float halfX = mp_obj_get_float(args[3]);
    float halfY = mp_obj_get_float(args[4]);
    float halfZ = mp_obj_get_float(args[5]);
    uint32_t mask = n_args > 6 ? static_cast<uint32_t>(mp_obj_get_int(args[6])) : 0xFFFFFFFF;

    auto nodes = api->OverlapBox(math::Vec3(x, y, z), math::Vec3(halfX, halfY, halfZ), mask);
    return mp_obj_new_int(static_cast<int>(nodes.size()));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_overlap_box_obj, 6, 7, lupine_overlap_box);

// ============================================================================
// PHYSICS 2D - BODY MANIPULATION
// ============================================================================

STATIC mp_obj_t lupine_get_linear_velocity_2d(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        auto vel = api->GetLinearVelocity2D();
        return make_vec2(vel.x, vel.y);
    }
    return make_vec2(0.0f, 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_linear_velocity_2d_obj, lupine_get_linear_velocity_2d);

STATIC mp_obj_t lupine_set_linear_velocity_2d(mp_obj_t x_obj, mp_obj_t y_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        api->SetLinearVelocity2D(mp_obj_get_float(x_obj), mp_obj_get_float(y_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_set_linear_velocity_2d_obj, lupine_set_linear_velocity_2d);

STATIC mp_obj_t lupine_get_angular_velocity_2d(void) {
    auto* api = GetCurrentScriptAPI();
    return mp_obj_new_float(api ? api->GetAngularVelocity2D() : 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_angular_velocity_2d_obj, lupine_get_angular_velocity_2d);

STATIC mp_obj_t lupine_set_angular_velocity_2d(mp_obj_t omega_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        api->SetAngularVelocity2D(mp_obj_get_float(omega_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_angular_velocity_2d_obj, lupine_set_angular_velocity_2d);

STATIC mp_obj_t lupine_apply_force_2d(mp_obj_t fx_obj, mp_obj_t fy_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        api->ApplyForce2D(mp_obj_get_float(fx_obj), mp_obj_get_float(fy_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_apply_force_2d_obj, lupine_apply_force_2d);

STATIC mp_obj_t lupine_apply_force_at_point_2d(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (api && n_args >= 4) {
        api->ApplyForceAtPoint2D(
            mp_obj_get_float(args[0]), mp_obj_get_float(args[1]),
            mp_obj_get_float(args[2]), mp_obj_get_float(args[3])
        );
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_apply_force_at_point_2d_obj, 4, 4, lupine_apply_force_at_point_2d);

STATIC mp_obj_t lupine_apply_torque_2d(mp_obj_t torque_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        api->ApplyTorque2D(mp_obj_get_float(torque_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_apply_torque_2d_obj, lupine_apply_torque_2d);

STATIC mp_obj_t lupine_apply_impulse_2d(mp_obj_t ix_obj, mp_obj_t iy_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        api->ApplyImpulse2D(mp_obj_get_float(ix_obj), mp_obj_get_float(iy_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_apply_impulse_2d_obj, lupine_apply_impulse_2d);

STATIC mp_obj_t lupine_apply_impulse_at_point_2d(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (api && n_args >= 4) {
        api->ApplyImpulseAtPoint2D(
            mp_obj_get_float(args[0]), mp_obj_get_float(args[1]),
            mp_obj_get_float(args[2]), mp_obj_get_float(args[3])
        );
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_apply_impulse_at_point_2d_obj, 4, 4, lupine_apply_impulse_at_point_2d);

STATIC mp_obj_t lupine_apply_angular_impulse_2d(mp_obj_t impulse_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        api->ApplyAngularImpulse2D(mp_obj_get_float(impulse_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_apply_angular_impulse_2d_obj, lupine_apply_angular_impulse_2d);

STATIC mp_obj_t lupine_get_mass_2d(void) {
    auto* api = GetCurrentScriptAPI();
    return mp_obj_new_float(api ? api->GetMass2D() : 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_mass_2d_obj, lupine_get_mass_2d);

STATIC mp_obj_t lupine_get_gravity_scale_2d(void) {
    auto* api = GetCurrentScriptAPI();
    return mp_obj_new_float(api ? api->GetGravityScale2D() : 1.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_gravity_scale_2d_obj, lupine_get_gravity_scale_2d);

STATIC mp_obj_t lupine_set_gravity_scale_2d(mp_obj_t scale_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        api->SetGravityScale2D(mp_obj_get_float(scale_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_gravity_scale_2d_obj, lupine_set_gravity_scale_2d);

STATIC mp_obj_t lupine_get_linear_damping_2d(void) {
    auto* api = GetCurrentScriptAPI();
    return mp_obj_new_float(api ? api->GetLinearDamping2D() : 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_linear_damping_2d_obj, lupine_get_linear_damping_2d);

STATIC mp_obj_t lupine_set_linear_damping_2d(mp_obj_t damping_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        api->SetLinearDamping2D(mp_obj_get_float(damping_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_linear_damping_2d_obj, lupine_set_linear_damping_2d);

// ============================================================================
// PHYSICS 3D - BODY MANIPULATION
// ============================================================================

STATIC mp_obj_t lupine_get_linear_velocity_3d(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        auto vel = api->GetLinearVelocity3D();
        return make_vec3(vel.x, vel.y, vel.z);
    }
    return make_vec3(0.0f, 0.0f, 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_linear_velocity_3d_obj, lupine_get_linear_velocity_3d);

STATIC mp_obj_t lupine_set_linear_velocity_3d(mp_obj_t x_obj, mp_obj_t y_obj, mp_obj_t z_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        api->SetLinearVelocity3D(mp_obj_get_float(x_obj), mp_obj_get_float(y_obj), mp_obj_get_float(z_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lupine_set_linear_velocity_3d_obj, lupine_set_linear_velocity_3d);

STATIC mp_obj_t lupine_get_angular_velocity_3d(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        auto vel = api->GetAngularVelocity3D();
        return make_vec3(vel.x, vel.y, vel.z);
    }
    return make_vec3(0.0f, 0.0f, 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_angular_velocity_3d_obj, lupine_get_angular_velocity_3d);

STATIC mp_obj_t lupine_set_angular_velocity_3d(mp_obj_t x_obj, mp_obj_t y_obj, mp_obj_t z_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        api->SetAngularVelocity3D(mp_obj_get_float(x_obj), mp_obj_get_float(y_obj), mp_obj_get_float(z_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lupine_set_angular_velocity_3d_obj, lupine_set_angular_velocity_3d);

STATIC mp_obj_t lupine_apply_force_3d(mp_obj_t fx_obj, mp_obj_t fy_obj, mp_obj_t fz_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        api->ApplyForce3D(mp_obj_get_float(fx_obj), mp_obj_get_float(fy_obj), mp_obj_get_float(fz_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lupine_apply_force_3d_obj, lupine_apply_force_3d);

STATIC mp_obj_t lupine_apply_force_at_point_3d(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (api && n_args >= 6) {
        api->ApplyForceAtPoint3D(
            mp_obj_get_float(args[0]), mp_obj_get_float(args[1]), mp_obj_get_float(args[2]),
            mp_obj_get_float(args[3]), mp_obj_get_float(args[4]), mp_obj_get_float(args[5])
        );
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_apply_force_at_point_3d_obj, 6, 6, lupine_apply_force_at_point_3d);

STATIC mp_obj_t lupine_apply_torque_3d(mp_obj_t x_obj, mp_obj_t y_obj, mp_obj_t z_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        api->ApplyTorque3D(mp_obj_get_float(x_obj), mp_obj_get_float(y_obj), mp_obj_get_float(z_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lupine_apply_torque_3d_obj, lupine_apply_torque_3d);

STATIC mp_obj_t lupine_apply_impulse_3d(mp_obj_t ix_obj, mp_obj_t iy_obj, mp_obj_t iz_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        api->ApplyImpulse3D(mp_obj_get_float(ix_obj), mp_obj_get_float(iy_obj), mp_obj_get_float(iz_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lupine_apply_impulse_3d_obj, lupine_apply_impulse_3d);

STATIC mp_obj_t lupine_apply_impulse_at_point_3d(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (api && n_args >= 6) {
        api->ApplyImpulseAtPoint3D(
            mp_obj_get_float(args[0]), mp_obj_get_float(args[1]), mp_obj_get_float(args[2]),
            mp_obj_get_float(args[3]), mp_obj_get_float(args[4]), mp_obj_get_float(args[5])
        );
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_apply_impulse_at_point_3d_obj, 6, 6, lupine_apply_impulse_at_point_3d);

STATIC mp_obj_t lupine_apply_torque_impulse_3d(mp_obj_t x_obj, mp_obj_t y_obj, mp_obj_t z_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        api->ApplyTorqueImpulse3D(mp_obj_get_float(x_obj), mp_obj_get_float(y_obj), mp_obj_get_float(z_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lupine_apply_torque_impulse_3d_obj, lupine_apply_torque_impulse_3d);

STATIC mp_obj_t lupine_get_mass_3d(void) {
    auto* api = GetCurrentScriptAPI();
    return mp_obj_new_float(api ? api->GetMass3D() : 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_mass_3d_obj, lupine_get_mass_3d);

STATIC mp_obj_t lupine_set_mass_3d(mp_obj_t mass_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        api->SetMass3D(mp_obj_get_float(mass_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_mass_3d_obj, lupine_set_mass_3d);

STATIC mp_obj_t lupine_get_gravity_scale_3d(void) {
    auto* api = GetCurrentScriptAPI();
    return mp_obj_new_float(api ? api->GetGravityScale3D() : 1.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_gravity_scale_3d_obj, lupine_get_gravity_scale_3d);

STATIC mp_obj_t lupine_set_gravity_scale_3d(mp_obj_t scale_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        api->SetGravityScale3D(mp_obj_get_float(scale_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_gravity_scale_3d_obj, lupine_set_gravity_scale_3d);

STATIC mp_obj_t lupine_get_linear_damping_3d(void) {
    auto* api = GetCurrentScriptAPI();
    return mp_obj_new_float(api ? api->GetLinearDamping3D() : 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_linear_damping_3d_obj, lupine_get_linear_damping_3d);

STATIC mp_obj_t lupine_set_linear_damping_3d(mp_obj_t damping_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        api->SetLinearDamping3D(mp_obj_get_float(damping_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_linear_damping_3d_obj, lupine_set_linear_damping_3d);

STATIC mp_obj_t lupine_get_angular_damping_3d(void) {
    auto* api = GetCurrentScriptAPI();
    return mp_obj_new_float(api ? api->GetAngularDamping3D() : 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_angular_damping_3d_obj, lupine_get_angular_damping_3d);

STATIC mp_obj_t lupine_set_angular_damping_3d(mp_obj_t damping_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        api->SetAngularDamping3D(mp_obj_get_float(damping_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_angular_damping_3d_obj, lupine_set_angular_damping_3d);

STATIC mp_obj_t lupine_get_linear_factor_3d(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        auto factor = api->GetLinearFactor3D();
        return make_vec3(factor.x, factor.y, factor.z);
    }
    return make_vec3(1.0f, 1.0f, 1.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_linear_factor_3d_obj, lupine_get_linear_factor_3d);

STATIC mp_obj_t lupine_set_linear_factor_3d(mp_obj_t x_obj, mp_obj_t y_obj, mp_obj_t z_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        api->SetLinearFactor3D(mp_obj_get_float(x_obj), mp_obj_get_float(y_obj), mp_obj_get_float(z_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lupine_set_linear_factor_3d_obj, lupine_set_linear_factor_3d);

STATIC mp_obj_t lupine_get_angular_factor_3d(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        auto factor = api->GetAngularFactor3D();
        return make_vec3(factor.x, factor.y, factor.z);
    }
    return make_vec3(1.0f, 1.0f, 1.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_angular_factor_3d_obj, lupine_get_angular_factor_3d);

STATIC mp_obj_t lupine_set_angular_factor_3d(mp_obj_t x_obj, mp_obj_t y_obj, mp_obj_t z_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        api->SetAngularFactor3D(mp_obj_get_float(x_obj), mp_obj_get_float(y_obj), mp_obj_get_float(z_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lupine_set_angular_factor_3d_obj, lupine_set_angular_factor_3d);

// ============================================================================
// PHYSICS WORLD ACCESS
// ============================================================================

STATIC mp_obj_t lupine_set_gravity_2d(mp_obj_t x_obj, mp_obj_t y_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        api->SetGravity2D(mp_obj_get_float(x_obj), mp_obj_get_float(y_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_set_gravity_2d_obj, lupine_set_gravity_2d);

STATIC mp_obj_t lupine_get_gravity_2d(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        auto gravity = api->GetGravity2D();
        return make_vec2(gravity.x, gravity.y);
    }
    return make_vec2(0.0f, -9.81f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_gravity_2d_obj, lupine_get_gravity_2d);

STATIC mp_obj_t lupine_set_gravity_3d(mp_obj_t x_obj, mp_obj_t y_obj, mp_obj_t z_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        api->SetGravity3D(mp_obj_get_float(x_obj), mp_obj_get_float(y_obj), mp_obj_get_float(z_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lupine_set_gravity_3d_obj, lupine_set_gravity_3d);

STATIC mp_obj_t lupine_get_gravity_3d(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        auto gravity = api->GetGravity3D();
        return make_vec3(gravity.x, gravity.y, gravity.z);
    }
    return make_vec3(0.0f, -9.81f, 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_gravity_3d_obj, lupine_get_gravity_3d);

// ============================================================================
// CHARACTER CONTROLLER (2D)
// ============================================================================

STATIC mp_obj_t lupine_move_and_slide_2d(mp_obj_t vx_obj, mp_obj_t vy_obj) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        float vx = mp_obj_get_float(vx_obj);
        float vy = mp_obj_get_float(vy_obj);
        auto result = api->MoveAndSlide2D(vx, vy);
        return make_vec2(result.x, result.y);
    }
    return make_vec2(0.0f, 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_move_and_slide_2d_obj, lupine_move_and_slide_2d);

STATIC mp_obj_t lupine_get_character_velocity_2d(void) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        auto velocity = api->GetCharacterVelocity2D();
        return make_vec2(velocity.x, velocity.y);
    }
    return make_vec2(0.0f, 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_character_velocity_2d_obj, lupine_get_character_velocity_2d);

STATIC mp_obj_t lupine_set_character_velocity_2d(mp_obj_t x_obj, mp_obj_t y_obj) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        api->SetCharacterVelocity2D(mp_obj_get_float(x_obj), mp_obj_get_float(y_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_set_character_velocity_2d_obj, lupine_set_character_velocity_2d);

STATIC mp_obj_t lupine_is_on_ground_2d(void) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        return mp_obj_new_bool(api->IsCharacterOnGround2D());
    }
    return mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_is_on_ground_2d_obj, lupine_is_on_ground_2d);

STATIC mp_obj_t lupine_is_on_wall_2d(void) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        return mp_obj_new_bool(api->IsCharacterOnWall2D());
    }
    return mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_is_on_wall_2d_obj, lupine_is_on_wall_2d);

STATIC mp_obj_t lupine_is_on_ceiling_2d(void) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        return mp_obj_new_bool(api->IsCharacterOnCeiling2D());
    }
    return mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_is_on_ceiling_2d_obj, lupine_is_on_ceiling_2d);

STATIC mp_obj_t lupine_get_ground_normal_2d(void) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        auto normal = api->GetCharacterGroundNormal2D();
        return make_vec2(normal.x, normal.y);
    }
    return make_vec2(0.0f, 1.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_ground_normal_2d_obj, lupine_get_ground_normal_2d);

STATIC mp_obj_t lupine_get_wall_normal_2d(void) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        auto normal = api->GetCharacterWallNormal2D();
        return make_vec2(normal.x, normal.y);
    }
    return make_vec2(0.0f, 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_wall_normal_2d_obj, lupine_get_wall_normal_2d);

STATIC mp_obj_t lupine_get_character_gravity_2d(void) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        return mp_obj_new_float(api->GetCharacterGravity2D());
    }
    return mp_obj_new_float(980.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_character_gravity_2d_obj, lupine_get_character_gravity_2d);

STATIC mp_obj_t lupine_set_character_gravity_2d(mp_obj_t gravity_obj) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        api->SetCharacterGravity2D(mp_obj_get_float(gravity_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_character_gravity_2d_obj, lupine_set_character_gravity_2d);

STATIC mp_obj_t lupine_get_character_max_fall_speed_2d(void) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        return mp_obj_new_float(api->GetCharacterMaxFallSpeed2D());
    }
    return mp_obj_new_float(1000.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_character_max_fall_speed_2d_obj, lupine_get_character_max_fall_speed_2d);

STATIC mp_obj_t lupine_set_character_max_fall_speed_2d(mp_obj_t speed_obj) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        api->SetCharacterMaxFallSpeed2D(mp_obj_get_float(speed_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_character_max_fall_speed_2d_obj, lupine_set_character_max_fall_speed_2d);

STATIC mp_obj_t lupine_get_character_max_slope_angle_2d(void) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        return mp_obj_new_float(api->GetCharacterMaxSlopeAngle2D());
    }
    return mp_obj_new_float(45.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_character_max_slope_angle_2d_obj, lupine_get_character_max_slope_angle_2d);

STATIC mp_obj_t lupine_set_character_max_slope_angle_2d(mp_obj_t angle_obj) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        api->SetCharacterMaxSlopeAngle2D(mp_obj_get_float(angle_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_character_max_slope_angle_2d_obj, lupine_set_character_max_slope_angle_2d);

STATIC mp_obj_t lupine_get_character_snap_to_ground_2d(void) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        return mp_obj_new_bool(api->GetCharacterSnapToGround2D());
    }
    return mp_const_true;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_character_snap_to_ground_2d_obj, lupine_get_character_snap_to_ground_2d);

STATIC mp_obj_t lupine_set_character_snap_to_ground_2d(mp_obj_t snap_obj) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        api->SetCharacterSnapToGround2D(mp_obj_is_true(snap_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_character_snap_to_ground_2d_obj, lupine_set_character_snap_to_ground_2d);

// ============================================================================
// CHARACTER CONTROLLER (3D)
// ============================================================================

STATIC mp_obj_t lupine_move_and_slide_3d(mp_obj_t vx_obj, mp_obj_t vy_obj, mp_obj_t vz_obj) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        float vx = mp_obj_get_float(vx_obj);
        float vy = mp_obj_get_float(vy_obj);
        float vz = mp_obj_get_float(vz_obj);
        auto result = api->MoveAndSlide3D(vx, vy, vz);
        return make_vec3(result.x, result.y, result.z);
    }
    return make_vec3(0.0f, 0.0f, 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lupine_move_and_slide_3d_obj, lupine_move_and_slide_3d);

STATIC mp_obj_t lupine_get_character_velocity_3d(void) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        auto velocity = api->GetCharacterVelocity3D();
        return make_vec3(velocity.x, velocity.y, velocity.z);
    }
    return make_vec3(0.0f, 0.0f, 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_character_velocity_3d_obj, lupine_get_character_velocity_3d);

STATIC mp_obj_t lupine_set_character_velocity_3d(mp_obj_t x_obj, mp_obj_t y_obj, mp_obj_t z_obj) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        api->SetCharacterVelocity3D(mp_obj_get_float(x_obj), mp_obj_get_float(y_obj), mp_obj_get_float(z_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lupine_set_character_velocity_3d_obj, lupine_set_character_velocity_3d);

STATIC mp_obj_t lupine_is_on_ground_3d(void) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        return mp_obj_new_bool(api->IsCharacterOnGround3D());
    }
    return mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_is_on_ground_3d_obj, lupine_is_on_ground_3d);

STATIC mp_obj_t lupine_is_on_wall_3d(void) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        return mp_obj_new_bool(api->IsCharacterOnWall3D());
    }
    return mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_is_on_wall_3d_obj, lupine_is_on_wall_3d);

STATIC mp_obj_t lupine_is_on_ceiling_3d(void) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        return mp_obj_new_bool(api->IsCharacterOnCeiling3D());
    }
    return mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_is_on_ceiling_3d_obj, lupine_is_on_ceiling_3d);

STATIC mp_obj_t lupine_get_ground_normal_3d(void) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        auto normal = api->GetCharacterGroundNormal3D();
        return make_vec3(normal.x, normal.y, normal.z);
    }
    return make_vec3(0.0f, 1.0f, 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_ground_normal_3d_obj, lupine_get_ground_normal_3d);

STATIC mp_obj_t lupine_get_wall_normal_3d(void) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        auto normal = api->GetCharacterWallNormal3D();
        return make_vec3(normal.x, normal.y, normal.z);
    }
    return make_vec3(0.0f, 0.0f, 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_wall_normal_3d_obj, lupine_get_wall_normal_3d);

STATIC mp_obj_t lupine_get_character_gravity_3d(void) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        return mp_obj_new_float(api->GetCharacterGravity3D());
    }
    return mp_obj_new_float(9.81f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_character_gravity_3d_obj, lupine_get_character_gravity_3d);

STATIC mp_obj_t lupine_set_character_gravity_3d(mp_obj_t gravity_obj) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        api->SetCharacterGravity3D(mp_obj_get_float(gravity_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_character_gravity_3d_obj, lupine_set_character_gravity_3d);

STATIC mp_obj_t lupine_get_character_max_fall_speed_3d(void) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        return mp_obj_new_float(api->GetCharacterMaxFallSpeed3D());
    }
    return mp_obj_new_float(50.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_character_max_fall_speed_3d_obj, lupine_get_character_max_fall_speed_3d);

STATIC mp_obj_t lupine_set_character_max_fall_speed_3d(mp_obj_t speed_obj) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        api->SetCharacterMaxFallSpeed3D(mp_obj_get_float(speed_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_character_max_fall_speed_3d_obj, lupine_set_character_max_fall_speed_3d);

STATIC mp_obj_t lupine_get_character_max_slope_angle_3d(void) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        return mp_obj_new_float(api->GetCharacterMaxSlopeAngle3D());
    }
    return mp_obj_new_float(45.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_character_max_slope_angle_3d_obj, lupine_get_character_max_slope_angle_3d);

STATIC mp_obj_t lupine_set_character_max_slope_angle_3d(mp_obj_t angle_obj) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        api->SetCharacterMaxSlopeAngle3D(mp_obj_get_float(angle_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_character_max_slope_angle_3d_obj, lupine_set_character_max_slope_angle_3d);

STATIC mp_obj_t lupine_get_character_step_height_3d(void) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        return mp_obj_new_float(api->GetCharacterStepHeight3D());
    }
    return mp_obj_new_float(0.3f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_character_step_height_3d_obj, lupine_get_character_step_height_3d);

STATIC mp_obj_t lupine_set_character_step_height_3d(mp_obj_t height_obj) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        api->SetCharacterStepHeight3D(mp_obj_get_float(height_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_character_step_height_3d_obj, lupine_set_character_step_height_3d);

STATIC mp_obj_t lupine_get_character_snap_to_ground_3d(void) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        return mp_obj_new_bool(api->GetCharacterSnapToGround3D());
    }
    return mp_const_true;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_character_snap_to_ground_3d_obj, lupine_get_character_snap_to_ground_3d);

STATIC mp_obj_t lupine_set_character_snap_to_ground_3d(mp_obj_t snap_obj) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        api->SetCharacterSnapToGround3D(mp_obj_is_true(snap_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_character_snap_to_ground_3d_obj, lupine_set_character_snap_to_ground_3d);

// ============================================================================
// ARCHETYPE DATA ASSETS (Unity ScriptableObject / Godot Resource equivalent)
// ============================================================================

static mp_obj_t JsonToMp(const nlohmann::json& json) {
    if (json.is_boolean()) {
        return mp_obj_new_bool(json.get<bool>());
    }
    if (json.is_number_integer()) {
        return mp_obj_new_int(static_cast<mp_int_t>(json.get<int64_t>()));
    }
    if (json.is_number()) {
        return mp_obj_new_float(static_cast<mp_float_t>(json.get<double>()));
    }
    if (json.is_string()) {
        std::string value = json.get<std::string>();
        return mp_obj_new_str(value.c_str(), value.size());
    }
    if (json.is_array()) {
        mp_obj_t list = mp_obj_new_list(0, nullptr);
        for (const nlohmann::json& item : json) {
            mp_obj_list_append(list, JsonToMp(item));
        }
        return list;
    }
    if (json.is_object()) {
        mp_obj_t dict = mp_obj_new_dict(0);
        for (nlohmann::json::const_iterator it = json.begin(); it != json.end(); ++it) {
            mp_obj_t key = mp_obj_new_str(it.key().c_str(), it.key().size());
            mp_obj_dict_store(dict, key, JsonToMp(it.value()));
        }
        return dict;
    }
    return mp_const_none;
}

static nlohmann::json MpToJson(mp_obj_t obj) {
    if (obj == mp_const_none) {
        return nlohmann::json(nullptr);
    }
    if (obj == mp_const_true) {
        return true;
    }
    if (obj == mp_const_false) {
        return false;
    }
    if (mp_obj_is_str(obj)) {
        size_t len = 0;
        const char* data = mp_obj_str_get_data(obj, &len);
        return std::string(data, len);
    }
    if (mp_obj_is_int(obj)) {
        return static_cast<int64_t>(mp_obj_get_int(obj));
    }
    if (mp_obj_is_float(obj)) {
        return static_cast<double>(mp_obj_get_float(obj));
    }
    if (mp_obj_is_type(obj, &mp_type_list) || mp_obj_is_type(obj, &mp_type_tuple)) {
        size_t len = 0;
        mp_obj_t* items = nullptr;
        mp_obj_get_array(obj, &len, &items);
        nlohmann::json array = nlohmann::json::array();
        for (size_t i = 0; i < len; ++i) {
            array.push_back(MpToJson(items[i]));
        }
        return array;
    }
    if (mp_obj_is_type(obj, &mp_type_dict)) {
        nlohmann::json object = nlohmann::json::object();
        mp_map_t* map = mp_obj_dict_get_map(obj);
        for (size_t i = 0; i < map->alloc; ++i) {
            if (!mp_map_slot_is_filled(map, i)) {
                continue;
            }
            mp_obj_t key = map->table[i].key;
            std::string keyStr;
            if (mp_obj_is_str(key)) {
                size_t klen = 0;
                const char* kdata = mp_obj_str_get_data(key, &klen);
                keyStr.assign(kdata, klen);
            } else if (mp_obj_is_int(key)) {
                keyStr = std::to_string(static_cast<int64_t>(mp_obj_get_int(key)));
            } else {
                continue;
            }
            object[keyStr] = MpToJson(map->table[i].value);
        }
        return object;
    }
    return nlohmann::json(nullptr);
}

// Generic accessors for any-typed globals. Structured types arrive as dicts/lists.
// Defined here (after JsonToMp/MpToJson) so the converters are in scope.
STATIC mp_obj_t lupine_get_global(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api || n_args < 1) return n_args > 1 ? args[1] : mp_const_none;

    const char* name = mp_obj_str_get_str(args[0]);
    nlohmann::json def = n_args > 1 ? MpToJson(args[1]) : nlohmann::json();
    return JsonToMp(api->GetGlobalValue(name, def));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_get_global_obj, 1, 2, lupine_get_global);

STATIC mp_obj_t lupine_set_global(mp_obj_t name_obj, mp_obj_t value_obj) {
    auto* api = GetCurrentScriptAPI();
    if (api) {
        const char* name = mp_obj_str_get_str(name_obj);
        api->SetGlobalValue(name, MpToJson(value_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_set_global_obj, lupine_set_global);

// ============================================================================
// DATA SAMPLING (Gradient / Curve)
// ============================================================================

STATIC mp_obj_t lupine_sample_gradient(mp_obj_t gradient_obj, mp_obj_t t_obj) {
    auto* api = GetCurrentScriptAPI();
    if (!api) return make_color(0.0f, 0.0f, 0.0f, 1.0f);
    math::Color c = api->SampleGradient(MpToJson(gradient_obj), mp_obj_get_float(t_obj));
    return make_color(c.r, c.g, c.b, c.a);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_sample_gradient_obj, lupine_sample_gradient);

STATIC mp_obj_t lupine_sample_curve(mp_obj_t curve_obj, mp_obj_t t_obj) {
    auto* api = GetCurrentScriptAPI();
    if (!api) return mp_obj_new_float(0.0f);
    return mp_obj_new_float(api->SampleCurve(MpToJson(curve_obj), mp_obj_get_float(t_obj)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_sample_curve_obj, lupine_sample_curve);

STATIC mp_obj_t lupine_load_archetype(mp_obj_t path_obj) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) {
        return mp_const_none;
    }
    const char* path = mp_obj_str_get_str(path_obj);
    asset::ArchetypeInstance* instance = api->LoadArchetype(path);
    if (!instance) {
        return mp_const_none;
    }
    return JsonToMp(instance->GetResolvedFields());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_load_archetype_obj, lupine_load_archetype);

STATIC mp_obj_t lupine_get_archetype_field(mp_obj_t path_obj, mp_obj_t name_obj) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) {
        return mp_const_none;
    }
    const char* path = mp_obj_str_get_str(path_obj);
    const char* name = mp_obj_str_get_str(name_obj);
    asset::ArchetypeInstance* instance = api->LoadArchetype(path);
    if (!instance) {
        return mp_const_none;
    }
    return JsonToMp(instance->GetFieldJson(name));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_get_archetype_field_obj, lupine_get_archetype_field);

STATIC mp_obj_t lupine_get_archetype_class(mp_obj_t path_obj) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) {
        return mp_const_none;
    }
    const char* path = mp_obj_str_get_str(path_obj);
    asset::ArchetypeInstance* instance = api->LoadArchetype(path);
    if (!instance) {
        return mp_const_none;
    }
    std::string className = instance->GetArchetypeClass();
    return mp_obj_new_str(className.c_str(), className.size());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_get_archetype_class_obj, lupine_get_archetype_class);

STATIC mp_obj_t lupine_archetype_is_a(mp_obj_t path_obj, mp_obj_t class_obj) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) {
        return mp_const_false;
    }
    const char* path = mp_obj_str_get_str(path_obj);
    const char* className = mp_obj_str_get_str(class_obj);
    asset::ArchetypeInstance* instance = api->LoadArchetype(path);
    if (!instance) {
        return mp_const_false;
    }
    return mp_obj_new_bool(instance->IsArchetype(className));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_archetype_is_a_obj, lupine_archetype_is_a);

STATIC mp_obj_t lupine_call_archetype(size_t n_args, const mp_obj_t* args) {
    const char* path = mp_obj_str_get_str(args[0]);
    const char* method = mp_obj_str_get_str(args[1]);

    nlohmann::json callArgs = nlohmann::json::array();
    for (size_t i = 2; i < n_args; ++i) {
        callArgs.push_back(MpToJson(args[i]));
    }

    nlohmann::json result = core::ArchetypeRuntime::GetInstance().CallMethod(path, method, callArgs);
    return JsonToMp(result);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_call_archetype_obj, 2, MP_OBJ_FUN_ARGS_MAX, lupine_call_archetype);

STATIC mp_obj_t lupine_load_archetype_async(size_t n_args, const mp_obj_t* args) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) {
        return mp_obj_new_int(0);
    }
    const char* path = mp_obj_str_get_str(args[0]);
    const char* callback = (n_args > 1 && args[1] != mp_const_none) ? mp_obj_str_get_str(args[1]) : "";
    int priority = (n_args > 2) ? static_cast<int>(mp_obj_get_int(args[2]))
                                : static_cast<int>(ScriptAPI::ASYNC_PRIORITY_NORMAL);
    uint64_t handle = api->LoadArchetypeAsync(path, callback, priority);
    return mp_obj_new_int(static_cast<mp_int_t>(handle));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_load_archetype_async_obj, 1, 3, lupine_load_archetype_async);

STATIC mp_obj_t lupine_load_archetype_definition_async(size_t n_args, const mp_obj_t* args) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) {
        return mp_obj_new_int(0);
    }
    const char* path = mp_obj_str_get_str(args[0]);
    const char* callback = (n_args > 1 && args[1] != mp_const_none) ? mp_obj_str_get_str(args[1]) : "";
    int priority = (n_args > 2) ? static_cast<int>(mp_obj_get_int(args[2]))
                                : static_cast<int>(ScriptAPI::ASYNC_PRIORITY_NORMAL);
    uint64_t handle = api->LoadArchetypeDefinitionAsync(path, callback, priority);
    return mp_obj_new_int(static_cast<mp_int_t>(handle));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_load_archetype_definition_async_obj, 1, 3, lupine_load_archetype_definition_async);

STATIC mp_obj_t lupine_get_archetype_load_status(mp_obj_t handle_obj) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) {
        return mp_obj_new_int(4);
    }
    uint64_t handle = static_cast<uint64_t>(mp_obj_get_int(handle_obj));
    return mp_obj_new_int(api->GetAsyncLoadStatus(handle));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_get_archetype_load_status_obj, lupine_get_archetype_load_status);

STATIC mp_obj_t lupine_is_archetype_load_complete(mp_obj_t handle_obj) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) {
        return mp_const_true;
    }
    uint64_t handle = static_cast<uint64_t>(mp_obj_get_int(handle_obj));
    return mp_obj_new_bool(api->IsAsyncLoadComplete(handle));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_is_archetype_load_complete_obj, lupine_is_archetype_load_complete);

STATIC mp_obj_t lupine_get_async_archetype(mp_obj_t handle_obj) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) {
        return mp_const_none;
    }
    uint64_t handle = static_cast<uint64_t>(mp_obj_get_int(handle_obj));
    asset::ArchetypeInstance* instance = api->GetAsyncArchetype(handle);
    if (!instance) {
        return mp_const_none;
    }
    return JsonToMp(instance->GetResolvedFields());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_get_async_archetype_obj, lupine_get_async_archetype);

STATIC mp_obj_t lupine_cancel_archetype_load(mp_obj_t handle_obj) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        uint64_t handle = static_cast<uint64_t>(mp_obj_get_int(handle_obj));
        api->CancelAsyncLoad(handle);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_cancel_archetype_load_obj, lupine_cancel_archetype_load);

STATIC mp_obj_t lupine_set_archetype_load_priority(mp_obj_t handle_obj, mp_obj_t priority_obj) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        uint64_t handle = static_cast<uint64_t>(mp_obj_get_int(handle_obj));
        api->SetAsyncLoadPriority(handle, static_cast<int>(mp_obj_get_int(priority_obj)));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_set_archetype_load_priority_obj, lupine_set_archetype_load_priority);

STATIC mp_obj_t lupine_get_archetype_load_priority(mp_obj_t handle_obj) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) {
        return mp_obj_new_int(0);
    }
    uint64_t handle = static_cast<uint64_t>(mp_obj_get_int(handle_obj));
    return mp_obj_new_int(api->GetAsyncLoadPriority(handle));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_get_archetype_load_priority_obj, lupine_get_archetype_load_priority);

STATIC mp_obj_t lupine_set_archetype_streaming_budget(mp_obj_t max_obj) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        api->SetAsyncStreamingBudget(static_cast<int>(mp_obj_get_int(max_obj)));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_archetype_streaming_budget_obj, lupine_set_archetype_streaming_budget);

STATIC mp_obj_t lupine_get_archetype_streaming_budget(void) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) {
        return mp_obj_new_int(0);
    }
    return mp_obj_new_int(api->GetAsyncStreamingBudget());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_archetype_streaming_budget_obj, lupine_get_archetype_streaming_budget);

STATIC mp_obj_t lupine_get_archetype_inflight_count(void) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) {
        return mp_obj_new_int(0);
    }
    return mp_obj_new_int(api->GetAsyncInFlightCount());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_archetype_inflight_count_obj, lupine_get_archetype_inflight_count);

STATIC mp_obj_t lupine_get_archetype_queued_count(void) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) {
        return mp_obj_new_int(0);
    }
    return mp_obj_new_int(api->GetAsyncQueuedCount());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_archetype_queued_count_obj, lupine_get_archetype_queued_count);

// ============================================================================
// HANDLE POOLS (node / component object identity)
//
// Script-visible handles must be plain PODs: the MicroPython GC in this build
// runs no finalisers, so a handle object is never destructed and can therefore
// own no C++ state. Storing the raw engine pointer in the handle (the previous
// design) meant every method dereferenced a pointer that may long since have been
// freed - `n.is_valid()` on a queue-freed node was itself a use-after-free.
//
// Instead a handle stores only a slot index + generation into these C++-side
// pools. A slot owns the weak reference and the long-lived scene tree, so a
// handle never holds or dereferences a raw engine pointer. Slots whose object has
// expired are reclaimed once per frame (SweepHandlePools) and their generation is
// bumped, so any stale handle pointing at them resolves to an empty ref and every
// operation safely no-ops. Slots are keyed by object identity, so re-wrapping the
// same live object reuses its slot and the pool stays bounded by the number of
// live wrapped objects rather than growing per wrap call.
// ============================================================================

struct NodeHandleSlot {
    std::weak_ptr<core::Node> object;
    const core::Node* key = nullptr;
    core::SceneManager* tree = nullptr;
    uint32_t generation = 1;
    bool inUse = false;
};

struct ComponentHandleSlot {
    std::weak_ptr<core::Component> object;
    const core::Component* key = nullptr;
    core::SceneManager* tree = nullptr;
    uint32_t generation = 1;
    bool inUse = false;
};

static std::vector<NodeHandleSlot> s_NodeSlots;
static std::vector<uint32_t> s_FreeNodeSlots;
static std::unordered_map<const core::Node*, uint32_t> s_NodeSlotByKey;

static std::vector<ComponentHandleSlot> s_ComponentSlots;
static std::vector<uint32_t> s_FreeComponentSlots;
static std::unordered_map<const core::Component*, uint32_t> s_ComponentSlotByKey;

static uint32_t NextGeneration(uint32_t generation) {
    return (generation == 0xFFFFFFFFu) ? 1u : (generation + 1u);
}

static void ReleaseNodeSlot(uint32_t slot) {
    if (slot >= s_NodeSlots.size()) return;
    NodeHandleSlot& s = s_NodeSlots[slot];
    if (!s.inUse) return;
    std::unordered_map<const core::Node*, uint32_t>::iterator it = s_NodeSlotByKey.find(s.key);
    if (it != s_NodeSlotByKey.end() && it->second == slot) {
        s_NodeSlotByKey.erase(it);
    }
    s.object.reset();
    s.key = nullptr;
    s.tree = nullptr;
    s.inUse = false;
    s.generation = NextGeneration(s.generation);
    s_FreeNodeSlots.push_back(slot);
}

static void ReleaseComponentSlot(uint32_t slot) {
    if (slot >= s_ComponentSlots.size()) return;
    ComponentHandleSlot& s = s_ComponentSlots[slot];
    if (!s.inUse) return;
    std::unordered_map<const core::Component*, uint32_t>::iterator it = s_ComponentSlotByKey.find(s.key);
    if (it != s_ComponentSlotByKey.end() && it->second == slot) {
        s_ComponentSlotByKey.erase(it);
    }
    s.object.reset();
    s.key = nullptr;
    s.tree = nullptr;
    s.inUse = false;
    s.generation = NextGeneration(s.generation);
    s_FreeComponentSlots.push_back(slot);
}

static uint32_t AcquireNodeSlot(const std::shared_ptr<core::Node>& node, core::SceneManager* tree,
                                uint32_t& outGeneration) {
    std::unordered_map<const core::Node*, uint32_t>::iterator it = s_NodeSlotByKey.find(node.get());
    if (it != s_NodeSlotByKey.end()) {
        const uint32_t existing = it->second;
        NodeHandleSlot& s = s_NodeSlots[existing];
        if (s.inUse && s.object.lock() == node) {
            if (tree && !s.tree) s.tree = tree;
            outGeneration = s.generation;
            return existing;
        }
        // The address was reused by a different node: retire the stale slot so
        // handles to the previous occupant fail their generation check.
        ReleaseNodeSlot(existing);
    }

    uint32_t slot;
    if (!s_FreeNodeSlots.empty()) {
        slot = s_FreeNodeSlots.back();
        s_FreeNodeSlots.pop_back();
    } else {
        slot = static_cast<uint32_t>(s_NodeSlots.size());
        s_NodeSlots.emplace_back();
    }
    NodeHandleSlot& s = s_NodeSlots[slot];
    s.object = node;
    s.key = node.get();
    s.tree = tree;
    s.inUse = true;
    s_NodeSlotByKey[s.key] = slot;
    outGeneration = s.generation;
    return slot;
}

static uint32_t AcquireComponentSlot(const std::shared_ptr<core::Component>& comp, core::SceneManager* tree,
                                     uint32_t& outGeneration) {
    std::unordered_map<const core::Component*, uint32_t>::iterator it = s_ComponentSlotByKey.find(comp.get());
    if (it != s_ComponentSlotByKey.end()) {
        const uint32_t existing = it->second;
        ComponentHandleSlot& s = s_ComponentSlots[existing];
        if (s.inUse && s.object.lock() == comp) {
            if (tree && !s.tree) s.tree = tree;
            outGeneration = s.generation;
            return existing;
        }
        ReleaseComponentSlot(existing);
    }

    uint32_t slot;
    if (!s_FreeComponentSlots.empty()) {
        slot = s_FreeComponentSlots.back();
        s_FreeComponentSlots.pop_back();
    } else {
        slot = static_cast<uint32_t>(s_ComponentSlots.size());
        s_ComponentSlots.emplace_back();
    }
    ComponentHandleSlot& s = s_ComponentSlots[slot];
    s.object = comp;
    s.key = comp.get();
    s.tree = tree;
    s.inUse = true;
    s_ComponentSlotByKey[s.key] = slot;
    outGeneration = s.generation;
    return slot;
}

static void SweepHandlePools() {
    for (uint32_t i = 0; i < static_cast<uint32_t>(s_NodeSlots.size()); ++i) {
        if (s_NodeSlots[i].inUse && s_NodeSlots[i].object.expired()) {
            ReleaseNodeSlot(i);
        }
    }
    for (uint32_t i = 0; i < static_cast<uint32_t>(s_ComponentSlots.size()); ++i) {
        if (s_ComponentSlots[i].inUse && s_ComponentSlots[i].object.expired()) {
            ReleaseComponentSlot(i);
        }
    }
}

// The scene tree is a process-lifetime object, so it is the only pointer a handle
// may capture directly. The originating ScriptAPI is not: it belongs to the script
// component that produced the handle and may be freed while the handle lives.
static core::SceneManager* ApiTree(ScriptAPI* api) {
    return api ? api->GetTree() : nullptr;
}

// Resolve the owning shared_ptr of a live component (Component is not
// enable_shared_from_this, so it cannot self-reference like Node does). Only ever
// called with a component the caller just obtained from the engine.
static std::shared_ptr<core::Component> SharedComponent(core::Component* comp) {
    if (!comp) return nullptr;
    core::Node* owner = comp->GetOwner();
    if (!owner) return nullptr;
    for (const std::shared_ptr<core::Component>& c : owner->GetComponents()) {
        if (c.get() == comp) {
            return c;
        }
    }
    return nullptr;
}

// ============================================================================
// NODE OBJECT MODEL (Godot-style scriptable node handles)
//
// A node handle stores only a pooled slot + generation (no C++ destructor) so it
// is safe for the MicroPython GC, which does not run finalisers in this build. A
// transient NodeRef is rebuilt per call to perform the actual work. Component
// access is provided through the node's property bag (get/set/has_component).
// ============================================================================

extern const mp_obj_type_t lupine_node_type;

typedef struct _lupine_node_obj_t {
    mp_obj_base_t base;
    uint32_t slot;
    uint32_t generation;
} lupine_node_obj_t;

static mp_obj_t wrap_node_ref(const NodeRef& ref) {
    std::shared_ptr<core::Node> node = ref.Lock();
    if (!node) return mp_const_none;
    uint32_t generation = 0;
    const uint32_t slot = AcquireNodeSlot(node, ref.GetTree(), generation);
    lupine_node_obj_t* o = mp_obj_malloc(lupine_node_obj_t, &lupine_node_type);
    o->slot = slot;
    o->generation = generation;
    return MP_OBJ_FROM_PTR(o);
}

static mp_obj_t wrap_node(core::Node* node, ScriptAPI* api) {
    if (!node) return mp_const_none;
    return wrap_node_ref(NodeRef::FromRawTree(node, ApiTree(api)));
}

static lupine_node_obj_t* node_self(mp_obj_t self_in) {
    return static_cast<lupine_node_obj_t*>(MP_OBJ_TO_PTR(self_in));
}

static NodeRef node_ref(mp_obj_t self_in) {
    lupine_node_obj_t* o = node_self(self_in);
    if (!o || o->slot >= s_NodeSlots.size()) return NodeRef();
    NodeHandleSlot& s = s_NodeSlots[o->slot];
    if (!s.inUse || s.generation != o->generation) return NodeRef();
    return NodeRef(s.object, s.tree);
}

// The wrapped node as a raw pointer, for the few ScriptAPI entry points that take
// one. Null whenever the handle is stale, so a dead handle can never smuggle a
// freed pointer back into the engine.
static core::Node* node_raw(mp_obj_t self_in) {
    return node_ref(self_in).Lock().get();
}

static bool node_arg(mp_obj_t v, NodeRef& out) {
    if (!mp_obj_is_type(v, &lupine_node_type)) return false;
    out = node_ref(v);
    return true;
}

// ============================================================================
// COMPONENT OBJECT MODEL
//
// Mirrors the node handle: a POD wrapper (no C++ destructor) storing a pooled
// slot + generation. A transient ComponentRef is rebuilt per call from the slot's
// weak reference. The timer/tween/sequence handles below share this same pool -
// they are all components, so the same component always resolves to one slot.
// ============================================================================

extern const mp_obj_type_t lupine_component_type;

typedef struct _lupine_component_obj_t {
    mp_obj_base_t base;
    uint32_t slot;
    uint32_t generation;
} lupine_component_obj_t;

static mp_obj_t wrap_component_shared(const std::shared_ptr<core::Component>& comp, core::SceneManager* tree) {
    if (!comp) return mp_const_none;
    uint32_t generation = 0;
    const uint32_t slot = AcquireComponentSlot(comp, tree, generation);
    lupine_component_obj_t* o = mp_obj_malloc(lupine_component_obj_t, &lupine_component_type);
    o->slot = slot;
    o->generation = generation;
    return MP_OBJ_FROM_PTR(o);
}

static mp_obj_t wrap_component_ref(const ComponentRef& ref) {
    return wrap_component_shared(ref.Lock(), ref.GetTree());
}

static lupine_component_obj_t* component_self(mp_obj_t self_in) {
    return static_cast<lupine_component_obj_t*>(MP_OBJ_TO_PTR(self_in));
}

// Resolve a component handle's pooled slot, or null when the handle is stale.
static ComponentHandleSlot* component_slot(mp_obj_t self_in) {
    lupine_component_obj_t* o = component_self(self_in);
    if (!o || o->slot >= s_ComponentSlots.size()) return nullptr;
    ComponentHandleSlot& s = s_ComponentSlots[o->slot];
    if (!s.inUse || s.generation != o->generation) return nullptr;
    return &s;
}

static ComponentRef component_ref(mp_obj_t self_in) {
    ComponentHandleSlot* s = component_slot(self_in);
    if (!s) return ComponentRef();
    return ComponentRef(s->object, s->tree);
}

// Defined with the tween object model further below; forward-declared so node
// methods (create_tween) can return tween handles.
static mp_obj_t wrap_tween(core::Component* tween, ScriptAPI* api);
static mp_obj_t wrap_tween_ref(const TweenRef& ref);
static mp_obj_t wrap_awaiter(const SignalAwaiter& awaiter);
static mp_obj_t wrap_sequence(core::Component* sequence, ScriptAPI* api);
static mp_obj_t wrap_sequence_ref(const SequenceRef& ref);

static mp_obj_t comp_is_valid(mp_obj_t self_in) {
    return mp_obj_new_bool(component_ref(self_in).IsValid());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(comp_is_valid_obj, comp_is_valid);

static mp_obj_t comp_get_type_name(mp_obj_t self_in) {
    std::string s = component_ref(self_in).GetTypeName();
    return mp_obj_new_str(s.c_str(), s.size());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(comp_get_type_name_obj, comp_get_type_name);

static mp_obj_t comp_get_name(mp_obj_t self_in) {
    std::string s = component_ref(self_in).GetName();
    return mp_obj_new_str(s.c_str(), s.size());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(comp_get_name_obj, comp_get_name);

static mp_obj_t comp_is_enabled(mp_obj_t self_in) {
    return mp_obj_new_bool(component_ref(self_in).IsEnabled());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(comp_is_enabled_obj, comp_is_enabled);

static mp_obj_t comp_set_enabled(mp_obj_t self_in, mp_obj_t v) {
    component_ref(self_in).SetEnabled(mp_obj_is_true(v));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(comp_set_enabled_obj, comp_set_enabled);

static mp_obj_t comp_has_property(mp_obj_t self_in, mp_obj_t name_in) {
    return mp_obj_new_bool(component_ref(self_in).HasProperty(mp_obj_str_get_str(name_in)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(comp_has_property_obj, comp_has_property);

static mp_obj_t comp_get(mp_obj_t self_in, mp_obj_t key_in) {
    return JsonToMp(component_ref(self_in).Get(mp_obj_str_get_str(key_in)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(comp_get_obj, comp_get);

static mp_obj_t comp_set(mp_obj_t self_in, mp_obj_t key_in, mp_obj_t val_in) {
    component_ref(self_in).Set(mp_obj_str_get_str(key_in), MpToJson(val_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(comp_set_obj, comp_set);

static mp_obj_t comp_get_owner(mp_obj_t self_in) {
    return wrap_node_ref(component_ref(self_in).GetOwner());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(comp_get_owner_obj, comp_get_owner);

static mp_obj_t comp_emit(size_t n_args, const mp_obj_t* args) {
    nlohmann::json payload = nlohmann::json::array();
    for (size_t i = 2; i < n_args; ++i) {
        payload.push_back(MpToJson(args[i]));
    }
    component_ref(args[0]).EmitSignal(mp_obj_str_get_str(args[1]), payload);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR(comp_emit_obj, 2, comp_emit);

static mp_obj_t comp_call(size_t n_args, const mp_obj_t* args) {
    nlohmann::json callArgs = nlohmann::json::array();
    for (size_t i = 2; i < n_args; ++i) {
        callArgs.push_back(MpToJson(args[i]));
    }
    return JsonToMp(component_ref(args[0]).Call(mp_obj_str_get_str(args[1]), callArgs));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR(comp_call_obj, 2, comp_call);

static mp_obj_t comp_connect(size_t n_args, const mp_obj_t* args) {
    NodeRef target;
    if (!node_arg(args[2], target)) {
        return mp_obj_new_int(0);
    }
    uint32_t flags = (n_args > 4) ? static_cast<uint32_t>(mp_obj_get_int(args[4])) : 0;
    uint64_t id = component_ref(args[0]).ConnectSignal(mp_obj_str_get_str(args[1]), target,
                                                       mp_obj_str_get_str(args[3]), flags);
    return mp_obj_new_int_from_uint(id);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(comp_connect_obj, 4, 5, comp_connect);

static mp_obj_t comp_disconnect(mp_obj_t self_in, mp_obj_t signal_in, mp_obj_t id_in) {
    component_ref(self_in).DisconnectSignal(mp_obj_str_get_str(signal_in),
                                            static_cast<uint64_t>(mp_obj_get_int(id_in)));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(comp_disconnect_obj, comp_disconnect);

static mp_obj_t comp_is_connected(mp_obj_t self_in, mp_obj_t signal_in) {
    return mp_obj_new_bool(component_ref(self_in).IsSignalConnected(mp_obj_str_get_str(signal_in)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(comp_is_connected_obj, comp_is_connected);

static mp_obj_t comp_add_user_signal(mp_obj_t self_in, mp_obj_t name_in) {
    component_ref(self_in).AddUserSignal(mp_obj_str_get_str(name_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(comp_add_user_signal_obj, comp_add_user_signal);

static mp_obj_t comp_get_signal_list(mp_obj_t self_in) {
    mp_obj_t list = mp_obj_new_list(0, nullptr);
    for (const std::string& n : component_ref(self_in).GetSignalList()) {
        mp_obj_list_append(list, mp_obj_new_str(n.c_str(), n.size()));
    }
    return list;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(comp_get_signal_list_obj, comp_get_signal_list);

static mp_obj_t comp_await_signal(mp_obj_t self_in, mp_obj_t signal_in) {
    return wrap_awaiter(component_ref(self_in).AwaitSignal(mp_obj_str_get_str(signal_in)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(comp_await_signal_obj, comp_await_signal);

static mp_obj_t comp_is_instance_of(mp_obj_t self_in, mp_obj_t name_in) {
    std::shared_ptr<core::Component> comp = component_ref(self_in).Lock();
    bool result = comp ? comp->IsInstanceOf(mp_obj_str_get_str(name_in)) : false;
    return mp_obj_new_bool(result);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(comp_is_instance_of_obj, comp_is_instance_of);

static mp_obj_t comp_get_type_chain(mp_obj_t self_in) {
    mp_obj_t list = mp_obj_new_list(0, nullptr);
    std::shared_ptr<core::Component> comp = component_ref(self_in).Lock();
    if (comp) {
        for (const std::string& n : comp->GetTypeChain()) {
            mp_obj_list_append(list, mp_obj_new_str(n.c_str(), n.size()));
        }
    }
    return list;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(comp_get_type_chain_obj, comp_get_type_chain);

static const struct { const char* name; const void* obj; } s_ComponentMethods[] = {
    {"is_valid", &comp_is_valid_obj},
    {"get_type_name", &comp_get_type_name_obj},
    {"get_name", &comp_get_name_obj},
    {"is_enabled", &comp_is_enabled_obj},
    {"set_enabled", &comp_set_enabled_obj},
    {"has_property", &comp_has_property_obj},
    {"get", &comp_get_obj},
    {"set", &comp_set_obj},
    {"call", &comp_call_obj},
    {"get_owner", &comp_get_owner_obj},
    {"emit", &comp_emit_obj},
    {"connect", &comp_connect_obj},
    {"disconnect", &comp_disconnect_obj},
    {"is_connected", &comp_is_connected_obj},
    {"add_user_signal", &comp_add_user_signal_obj},
    {"get_signal_list", &comp_get_signal_list_obj},
    {"await_signal", &comp_await_signal_obj},
    {"is_instance_of", &comp_is_instance_of_obj},
    {"get_type_chain", &comp_get_type_chain_obj},
    {nullptr, nullptr},
};

static void component_attr(mp_obj_t self_in, qstr attr, mp_obj_t* dest) {
    const char* name = qstr_str(attr);

    if (name[0] == '_' && name[1] == '_') {
        return;
    }

    if (dest[0] == MP_OBJ_NULL) {
        for (int i = 0; s_ComponentMethods[i].name != nullptr; ++i) {
            if (std::strcmp(name, s_ComponentMethods[i].name) == 0) {
                dest[0] = MP_OBJ_FROM_PTR(s_ComponentMethods[i].obj);
                dest[1] = self_in;
                return;
            }
        }
        dest[0] = JsonToMp(component_ref(self_in).Get(name));
        return;
    }

    if (dest[0] == MP_OBJ_SENTINEL && dest[1] != MP_OBJ_NULL) {
        component_ref(self_in).Set(name, MpToJson(dest[1]));
        dest[0] = MP_OBJ_NULL;
    }
}

MP_DEFINE_CONST_OBJ_TYPE(
    lupine_component_type,
    MP_QSTR_object,
    MP_TYPE_FLAG_NONE,
    attr, component_attr
);

// Node methods that return component handles (defined here, after the component
// type, so the wrappers are available; registered in s_NodeMethods below).
static mp_obj_t node_get_component(mp_obj_t self_in, mp_obj_t type_in) {
    return wrap_component_ref(node_ref(self_in).GetComponent(mp_obj_str_get_str(type_in)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(node_get_component_obj, node_get_component);

static mp_obj_t node_get_components(mp_obj_t self_in, mp_obj_t type_in) {
    mp_obj_t list = mp_obj_new_list(0, nullptr);
    for (const ComponentRef& c : node_ref(self_in).GetComponents(mp_obj_str_get_str(type_in))) {
        mp_obj_list_append(list, wrap_component_ref(c));
    }
    return list;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(node_get_components_obj, node_get_components);

static mp_obj_t node_add_component(mp_obj_t self_in, mp_obj_t type_in) {
    return wrap_component_ref(node_ref(self_in).AddComponent(mp_obj_str_get_str(type_in)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(node_add_component_obj, node_add_component);

static mp_obj_t node_remove_component(mp_obj_t self_in, mp_obj_t comp_in) {
    if (mp_obj_is_type(comp_in, &lupine_component_type)) {
        node_ref(self_in).RemoveComponent(component_ref(comp_in));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(node_remove_component_obj, node_remove_component);

static mp_obj_t node_is_valid(mp_obj_t self_in) {
    return mp_obj_new_bool(node_ref(self_in).IsValid());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_is_valid_obj, node_is_valid);

static mp_obj_t node_get_name(mp_obj_t self_in) {
    std::string name = node_ref(self_in).GetName();
    return mp_obj_new_str(name.c_str(), name.size());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_get_name_obj, node_get_name);

static mp_obj_t node_set_name(mp_obj_t self_in, mp_obj_t name_in) {
    node_ref(self_in).SetName(mp_obj_str_get_str(name_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(node_set_name_obj, node_set_name);

static mp_obj_t node_get_uuid(mp_obj_t self_in) {
    std::string uuid = node_ref(self_in).GetUUID();
    return mp_obj_new_str(uuid.c_str(), uuid.size());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_get_uuid_obj, node_get_uuid);

static mp_obj_t node_get_path(mp_obj_t self_in) {
    std::string path = node_ref(self_in).GetPath();
    return mp_obj_new_str(path.c_str(), path.size());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_get_path_obj, node_get_path);

static mp_obj_t node_get_type_name(mp_obj_t self_in) {
    std::string type = node_ref(self_in).GetTypeName();
    return mp_obj_new_str(type.c_str(), type.size());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_get_type_name_obj, node_get_type_name);

static mp_obj_t node_is_active(mp_obj_t self_in) {
    return mp_obj_new_bool(node_ref(self_in).IsActive());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_is_active_obj, node_is_active);

static mp_obj_t node_set_active(mp_obj_t self_in, mp_obj_t v) {
    node_ref(self_in).SetActive(mp_obj_is_true(v));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(node_set_active_obj, node_set_active);

static mp_obj_t node_is_visible(mp_obj_t self_in) {
    return mp_obj_new_bool(node_ref(self_in).IsVisible());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_is_visible_obj, node_is_visible);

static mp_obj_t node_set_visible(mp_obj_t self_in, mp_obj_t v) {
    node_ref(self_in).SetVisible(mp_obj_is_true(v));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(node_set_visible_obj, node_set_visible);

static mp_obj_t node_is_unique(mp_obj_t self_in) {
    return mp_obj_new_bool(node_ref(self_in).IsUniqueNameInOwner());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_is_unique_obj, node_is_unique);

static mp_obj_t node_set_unique(mp_obj_t self_in, mp_obj_t v) {
    node_ref(self_in).SetUniqueNameInOwner(mp_obj_is_true(v));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(node_set_unique_obj, node_set_unique);

static mp_obj_t node_get_child_count(mp_obj_t self_in) {
    return mp_obj_new_int(node_ref(self_in).GetChildCount());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_get_child_count_obj, node_get_child_count);

static mp_obj_t node_has_node(mp_obj_t self_in, mp_obj_t path_in) {
    return mp_obj_new_bool(node_ref(self_in).HasNode(mp_obj_str_get_str(path_in)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(node_has_node_obj, node_has_node);

static mp_obj_t node_has_component(mp_obj_t self_in, mp_obj_t type_in) {
    return mp_obj_new_bool(node_ref(self_in).HasComponent(mp_obj_str_get_str(type_in)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(node_has_component_obj, node_has_component);

static mp_obj_t node_has_property(mp_obj_t self_in, mp_obj_t prop_in) {
    return mp_obj_new_bool(node_ref(self_in).HasProperty(mp_obj_str_get_str(prop_in)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(node_has_property_obj, node_has_property);

static mp_obj_t node_queue_free(mp_obj_t self_in) {
    node_ref(self_in).QueueFree();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_queue_free_obj, node_queue_free);

static mp_obj_t node_queue_free_deferred(mp_obj_t self_in) {
    node_ref(self_in).QueueFreeDeferred();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_queue_free_deferred_obj, node_queue_free_deferred);

static mp_obj_t node_free(mp_obj_t self_in) {
    node_ref(self_in).Free();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_free_obj, node_free);

static mp_obj_t node_get_parent(mp_obj_t self_in) {
    return wrap_node_ref(node_ref(self_in).GetParent());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_get_parent_obj, node_get_parent);

static mp_obj_t node_get_child(mp_obj_t self_in, mp_obj_t name_in) {
    return wrap_node_ref(node_ref(self_in).GetChild(mp_obj_str_get_str(name_in)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(node_get_child_obj, node_get_child);

static mp_obj_t node_get_child_at(mp_obj_t self_in, mp_obj_t index_in) {
    return wrap_node_ref(node_ref(self_in).GetChildAt(mp_obj_get_int(index_in)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(node_get_child_at_obj, node_get_child_at);

static mp_obj_t node_get_children(mp_obj_t self_in) {
    std::vector<NodeRef> children = node_ref(self_in).GetChildren();
    mp_obj_t list = mp_obj_new_list(0, nullptr);
    for (const NodeRef& c : children) {
        mp_obj_list_append(list, wrap_node_ref(c));
    }
    return list;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_get_children_obj, node_get_children);

static mp_obj_t node_find_node(mp_obj_t self_in, mp_obj_t path_in) {
    return wrap_node_ref(node_ref(self_in).FindNode(mp_obj_str_get_str(path_in)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(node_find_node_obj, node_find_node);

static mp_obj_t node_add_child(mp_obj_t self_in, mp_obj_t other) {
    NodeRef child;
    if (node_arg(other, child)) node_ref(self_in).AddChild(child);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(node_add_child_obj, node_add_child);

static mp_obj_t node_remove_child(mp_obj_t self_in, mp_obj_t other) {
    NodeRef child;
    if (node_arg(other, child)) node_ref(self_in).RemoveChild(child);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(node_remove_child_obj, node_remove_child);

static mp_obj_t node_reparent_to(mp_obj_t self_in, mp_obj_t other) {
    NodeRef parent;
    if (node_arg(other, parent)) node_ref(self_in).ReparentTo(parent);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(node_reparent_to_obj, node_reparent_to);

static mp_obj_t node_duplicate(mp_obj_t self_in) {
    return wrap_node_ref(node_ref(self_in).Duplicate());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_duplicate_obj, node_duplicate);

static mp_obj_t node_get_sibling_index(mp_obj_t self_in) {
    return mp_obj_new_int(node_ref(self_in).GetSiblingIndex());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_get_sibling_index_obj, node_get_sibling_index);

static mp_obj_t node_set_sibling_index(mp_obj_t self_in, mp_obj_t index_in) {
    node_ref(self_in).SetSiblingIndex(mp_obj_get_int(index_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(node_set_sibling_index_obj, node_set_sibling_index);

static mp_obj_t node_get_child_index(mp_obj_t self_in, mp_obj_t child_in) {
    NodeRef child;
    if (!node_arg(child_in, child)) return mp_obj_new_int(-1);
    return mp_obj_new_int(node_ref(self_in).GetChildIndex(child));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(node_get_child_index_obj, node_get_child_index);

static mp_obj_t node_move_child(mp_obj_t self_in, mp_obj_t child_in, mp_obj_t index_in) {
    NodeRef child;
    if (node_arg(child_in, child)) node_ref(self_in).MoveChild(child, mp_obj_get_int(index_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(node_move_child_obj, node_move_child);

static mp_obj_t node_create_tween(size_t n_args, const mp_obj_t* args) {
    const char* channel = mp_obj_str_get_str(args[1]);
    nlohmann::json to = MpToJson(args[2]);
    float duration = mp_obj_get_float(args[3]);
    const char* easing = (n_args > 4) ? mp_obj_str_get_str(args[4]) : "linear";
    return wrap_tween_ref(node_ref(args[0]).CreateTween(channel, to, duration, easing));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(node_create_tween_obj, 4, 5, node_create_tween);

static mp_obj_t node_create_sequence(mp_obj_t self_in) {
    return wrap_sequence_ref(node_ref(self_in).CreateSequence());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_create_sequence_obj, node_create_sequence);

static mp_obj_t node_await_signal(mp_obj_t self_in, mp_obj_t signal_in) {
    return wrap_awaiter(node_ref(self_in).AwaitSignal(mp_obj_str_get_str(signal_in)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(node_await_signal_obj, node_await_signal);

static mp_obj_t node_distance_to(mp_obj_t self_in, mp_obj_t other) {
    NodeRef target;
    if (!node_arg(other, target)) return mp_obj_new_float(0.0f);
    return mp_obj_new_float(node_ref(self_in).DistanceTo(target));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(node_distance_to_obj, node_distance_to);

static mp_obj_t node_get_position_2d(mp_obj_t self_in) {
    math::Vec2 p = node_ref(self_in).GetPosition2D();
    return make_vec2(p.x, p.y);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_get_position_2d_obj, node_get_position_2d);

static mp_obj_t node_set_position_2d(mp_obj_t self_in, mp_obj_t x, mp_obj_t y) {
    node_ref(self_in).SetPosition2D(mp_obj_get_float(x), mp_obj_get_float(y));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(node_set_position_2d_obj, node_set_position_2d);

static mp_obj_t node_translate_2d(mp_obj_t self_in, mp_obj_t x, mp_obj_t y) {
    node_ref(self_in).Translate2D(mp_obj_get_float(x), mp_obj_get_float(y));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(node_translate_2d_obj, node_translate_2d);

static mp_obj_t node_get_rotation_2d(mp_obj_t self_in) {
    return mp_obj_new_float(node_ref(self_in).GetRotation2D());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_get_rotation_2d_obj, node_get_rotation_2d);

static mp_obj_t node_set_rotation_2d(mp_obj_t self_in, mp_obj_t deg) {
    node_ref(self_in).SetRotation2D(mp_obj_get_float(deg));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(node_set_rotation_2d_obj, node_set_rotation_2d);

static mp_obj_t node_get_scale_2d(mp_obj_t self_in) {
    math::Vec2 s = node_ref(self_in).GetScale2D();
    return make_vec2(s.x, s.y);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_get_scale_2d_obj, node_get_scale_2d);

static mp_obj_t node_set_scale_2d(mp_obj_t self_in, mp_obj_t x, mp_obj_t y) {
    node_ref(self_in).SetScale2D(mp_obj_get_float(x), mp_obj_get_float(y));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(node_set_scale_2d_obj, node_set_scale_2d);

static mp_obj_t node_get_position_3d(mp_obj_t self_in) {
    math::Vec3 p = node_ref(self_in).GetPosition3D();
    return make_vec3(p.x, p.y, p.z);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_get_position_3d_obj, node_get_position_3d);

static mp_obj_t node_set_position_3d(size_t, const mp_obj_t* args) {
    node_ref(args[0]).SetPosition3D(mp_obj_get_float(args[1]), mp_obj_get_float(args[2]), mp_obj_get_float(args[3]));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(node_set_position_3d_obj, 4, 4, node_set_position_3d);

static mp_obj_t node_translate_3d(size_t, const mp_obj_t* args) {
    node_ref(args[0]).Translate3D(mp_obj_get_float(args[1]), mp_obj_get_float(args[2]), mp_obj_get_float(args[3]));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(node_translate_3d_obj, 4, 4, node_translate_3d);

static mp_obj_t node_get_rotation_3d(mp_obj_t self_in) {
    math::Vec3 r = node_ref(self_in).GetRotation3D();
    return make_vec3(r.x, r.y, r.z);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_get_rotation_3d_obj, node_get_rotation_3d);

static mp_obj_t node_set_rotation_3d(size_t, const mp_obj_t* args) {
    node_ref(args[0]).SetRotation3D(mp_obj_get_float(args[1]), mp_obj_get_float(args[2]), mp_obj_get_float(args[3]));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(node_set_rotation_3d_obj, 4, 4, node_set_rotation_3d);

static mp_obj_t node_get_scale_3d(mp_obj_t self_in) {
    math::Vec3 s = node_ref(self_in).GetScale3D();
    return make_vec3(s.x, s.y, s.z);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_get_scale_3d_obj, node_get_scale_3d);

static mp_obj_t node_set_scale_3d(size_t, const mp_obj_t* args) {
    node_ref(args[0]).SetScale3D(mp_obj_get_float(args[1]), mp_obj_get_float(args[2]), mp_obj_get_float(args[3]));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(node_set_scale_3d_obj, 4, 4, node_set_scale_3d);

static mp_obj_t node_get_global_position_2d(mp_obj_t self_in) {
    math::Vec2 p = node_ref(self_in).GetGlobalPosition2D();
    return make_vec2(p.x, p.y);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_get_global_position_2d_obj, node_get_global_position_2d);

static mp_obj_t node_set_global_position_2d(size_t, const mp_obj_t* args) {
    node_ref(args[0]).SetGlobalPosition2D(mp_obj_get_float(args[1]), mp_obj_get_float(args[2]));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(node_set_global_position_2d_obj, 3, 3, node_set_global_position_2d);

static mp_obj_t node_get_global_rotation_2d(mp_obj_t self_in) {
    return mp_obj_new_float(node_ref(self_in).GetGlobalRotation2D());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_get_global_rotation_2d_obj, node_get_global_rotation_2d);

static mp_obj_t node_set_global_rotation_2d(mp_obj_t self_in, mp_obj_t degrees_obj) {
    node_ref(self_in).SetGlobalRotation2D(mp_obj_get_float(degrees_obj));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(node_set_global_rotation_2d_obj, node_set_global_rotation_2d);

static mp_obj_t node_get_global_scale_2d(mp_obj_t self_in) {
    math::Vec2 s = node_ref(self_in).GetGlobalScale2D();
    return make_vec2(s.x, s.y);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_get_global_scale_2d_obj, node_get_global_scale_2d);

static mp_obj_t node_get_global_position_3d(mp_obj_t self_in) {
    math::Vec3 p = node_ref(self_in).GetGlobalPosition3D();
    return make_vec3(p.x, p.y, p.z);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_get_global_position_3d_obj, node_get_global_position_3d);

static mp_obj_t node_set_global_position_3d(size_t, const mp_obj_t* args) {
    node_ref(args[0]).SetGlobalPosition3D(mp_obj_get_float(args[1]), mp_obj_get_float(args[2]),
                                          mp_obj_get_float(args[3]));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(node_set_global_position_3d_obj, 4, 4, node_set_global_position_3d);

static mp_obj_t node_get_global_rotation_3d(mp_obj_t self_in) {
    math::Vec3 r = node_ref(self_in).GetGlobalRotation3D();
    return make_vec3(r.x, r.y, r.z);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_get_global_rotation_3d_obj, node_get_global_rotation_3d);

static mp_obj_t node_set_global_rotation_3d(size_t, const mp_obj_t* args) {
    node_ref(args[0]).SetGlobalRotation3D(mp_obj_get_float(args[1]), mp_obj_get_float(args[2]),
                                          mp_obj_get_float(args[3]));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(node_set_global_rotation_3d_obj, 4, 4, node_set_global_rotation_3d);

static mp_obj_t node_get_global_scale_3d(mp_obj_t self_in) {
    math::Vec3 s = node_ref(self_in).GetGlobalScale3D();
    return make_vec3(s.x, s.y, s.z);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_get_global_scale_3d_obj, node_get_global_scale_3d);

static mp_obj_t node_get(mp_obj_t self_in, mp_obj_t key_in) {
    return JsonToMp(node_ref(self_in).Get(mp_obj_str_get_str(key_in)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(node_get_obj, node_get);

static mp_obj_t node_set(mp_obj_t self_in, mp_obj_t key_in, mp_obj_t val_in) {
    node_ref(self_in).Set(mp_obj_str_get_str(key_in), MpToJson(val_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(node_set_obj, node_set);

static mp_obj_t node_has_method(mp_obj_t self_in, mp_obj_t method_in) {
    return mp_obj_new_bool(node_ref(self_in).HasMethod(mp_obj_str_get_str(method_in)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(node_has_method_obj, node_has_method);

static mp_obj_t node_call(size_t n_args, const mp_obj_t* args) {
    nlohmann::json callArgs = nlohmann::json::array();
    for (size_t i = 2; i < n_args; ++i) {
        callArgs.push_back(MpToJson(args[i]));
    }
    return JsonToMp(node_ref(args[0]).Call(mp_obj_str_get_str(args[1]), callArgs));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR(node_call_obj, 2, node_call);

// --- Signals (node handle methods) -----------------------------------------

static mp_obj_t node_emit(size_t n_args, const mp_obj_t* args) {
    nlohmann::json payload = nlohmann::json::array();
    for (size_t i = 2; i < n_args; ++i) {
        payload.push_back(MpToJson(args[i]));
    }
    node_ref(args[0]).EmitSignal(mp_obj_str_get_str(args[1]), payload);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR(node_emit_obj, 2, node_emit);

// --- Networking (RPC + authority, node handle methods) ---------------------

static mp_obj_t node_rpc(size_t n_args, const mp_obj_t* args) {
    nlohmann::json payload = nlohmann::json::array();
    for (size_t i = 2; i < n_args; ++i) {
        payload.push_back(MpToJson(args[i]));
    }
    node_ref(args[0]).Rpc(mp_obj_str_get_str(args[1]), payload);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR(node_rpc_obj, 2, node_rpc);

static mp_obj_t node_rpc_id(size_t n_args, const mp_obj_t* args) {
    nlohmann::json payload = nlohmann::json::array();
    for (size_t i = 3; i < n_args; ++i) {
        payload.push_back(MpToJson(args[i]));
    }
    node_ref(args[0]).RpcId(static_cast<uint32_t>(mp_obj_get_int(args[1])),
                            mp_obj_str_get_str(args[2]), payload);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR(node_rpc_id_obj, 3, node_rpc_id);

static mp_obj_t node_rpc_unreliable(size_t n_args, const mp_obj_t* args) {
    nlohmann::json payload = nlohmann::json::array();
    for (size_t i = 2; i < n_args; ++i) {
        payload.push_back(MpToJson(args[i]));
    }
    node_ref(args[0]).RpcUnreliable(mp_obj_str_get_str(args[1]), payload);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR(node_rpc_unreliable_obj, 2, node_rpc_unreliable);

static mp_obj_t node_set_authority(mp_obj_t self_in, mp_obj_t peer_in) {
    node_ref(self_in).SetMultiplayerAuthority(static_cast<uint32_t>(mp_obj_get_int(peer_in)));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(node_set_authority_obj, node_set_authority);

static mp_obj_t node_get_authority(mp_obj_t self_in) {
    return mp_obj_new_int(static_cast<mp_int_t>(node_ref(self_in).GetMultiplayerAuthority()));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_get_authority_obj, node_get_authority);

static mp_obj_t node_is_authority(mp_obj_t self_in) {
    return mp_obj_new_bool(node_ref(self_in).IsMultiplayerAuthority());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_is_authority_obj, node_is_authority);

static mp_obj_t node_get_network_id(mp_obj_t self_in) {
    return mp_obj_new_int(static_cast<mp_int_t>(node_ref(self_in).GetNetworkId()));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_get_network_id_obj, node_get_network_id);

static mp_obj_t node_connect(size_t n_args, const mp_obj_t* args) {
    NodeRef target;
    if (!node_arg(args[2], target)) {
        return mp_obj_new_int(0);
    }
    uint32_t flags = (n_args > 4) ? static_cast<uint32_t>(mp_obj_get_int(args[4])) : 0;
    uint64_t id = node_ref(args[0]).ConnectSignal(mp_obj_str_get_str(args[1]), target,
                                                  mp_obj_str_get_str(args[3]), flags);
    return mp_obj_new_int_from_uint(id);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(node_connect_obj, 4, 5, node_connect);

static mp_obj_t node_disconnect(mp_obj_t self_in, mp_obj_t signal_in, mp_obj_t id_in) {
    node_ref(self_in).DisconnectSignal(mp_obj_str_get_str(signal_in),
                                       static_cast<uint64_t>(mp_obj_get_int(id_in)));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(node_disconnect_obj, node_disconnect);

static mp_obj_t node_is_connected(mp_obj_t self_in, mp_obj_t signal_in) {
    return mp_obj_new_bool(node_ref(self_in).IsSignalConnected(mp_obj_str_get_str(signal_in)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(node_is_connected_obj, node_is_connected);

static mp_obj_t node_add_user_signal(mp_obj_t self_in, mp_obj_t name_in) {
    node_ref(self_in).AddUserSignal(mp_obj_str_get_str(name_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(node_add_user_signal_obj, node_add_user_signal);

static mp_obj_t node_get_signal_list(mp_obj_t self_in) {
    mp_obj_t list = mp_obj_new_list(0, nullptr);
    for (const std::string& name : node_ref(self_in).GetSignalList()) {
        mp_obj_list_append(list, mp_obj_new_str(name.c_str(), name.size()));
    }
    return list;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_get_signal_list_obj, node_get_signal_list);

static mp_obj_t node_add_to_group(mp_obj_t self_in, mp_obj_t group_in) {
    node_ref(self_in).AddToGroup(mp_obj_str_get_str(group_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(node_add_to_group_obj, node_add_to_group);

static mp_obj_t node_remove_from_group(mp_obj_t self_in, mp_obj_t group_in) {
    node_ref(self_in).RemoveFromGroup(mp_obj_str_get_str(group_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(node_remove_from_group_obj, node_remove_from_group);

static mp_obj_t node_is_in_group(mp_obj_t self_in, mp_obj_t group_in) {
    return node_ref(self_in).IsInGroup(mp_obj_str_get_str(group_in)) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(node_is_in_group_obj, node_is_in_group);

static mp_obj_t node_get_groups(mp_obj_t self_in) {
    mp_obj_t list = mp_obj_new_list(0, nullptr);
    for (const std::string& group : node_ref(self_in).GetGroups()) {
        mp_obj_list_append(list, mp_obj_new_str(group.c_str(), group.size()));
    }
    return list;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_get_groups_obj, node_get_groups);

// --- Interfaces (node methods) ---------------------------------------------

static mp_obj_t node_implements_interface(mp_obj_t self_in, mp_obj_t name_in) {
    return node_ref(self_in).ImplementsInterface(mp_obj_str_get_str(name_in)) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(node_implements_interface_obj, node_implements_interface);

static mp_obj_t node_get_interfaces(mp_obj_t self_in) {
    mp_obj_t list = mp_obj_new_list(0, nullptr);
    for (const std::string& iface : node_ref(self_in).GetInterfaces()) {
        mp_obj_list_append(list, mp_obj_new_str(iface.c_str(), iface.size()));
    }
    return list;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_get_interfaces_obj, node_get_interfaces);

static mp_obj_t node_verify_interface(mp_obj_t self_in, mp_obj_t name_in) {
    return JsonToMp(node_ref(self_in).VerifyInterface(mp_obj_str_get_str(name_in)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(node_verify_interface_obj, node_verify_interface);

// --- Signals & events (Lupine module functions) ----------------------------

static mp_obj_t lupine_emit(size_t n_args, const mp_obj_t* args) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        nlohmann::json payload = nlohmann::json::array();
        for (size_t i = 1; i < n_args; ++i) {
            payload.push_back(MpToJson(args[i]));
        }
        api->EmitSignal(mp_obj_str_get_str(args[0]), payload);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR(lupine_emit_obj, 1, lupine_emit);

static mp_obj_t lupine_connect(size_t n_args, const mp_obj_t* args) {
    ScriptAPI* api = GetCurrentScriptAPI();
    NodeRef target;
    if (api && node_arg(args[1], target)) {
        auto node = target.Lock();
        uint32_t flags = (n_args > 3) ? static_cast<uint32_t>(mp_obj_get_int(args[3])) : 0;
        return mp_obj_new_int_from_uint(
            api->ConnectSignal(mp_obj_str_get_str(args[0]), node.get(),
                               mp_obj_str_get_str(args[2]), flags));
    }
    return mp_obj_new_int(0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_connect_obj, 3, 4, lupine_connect);

static mp_obj_t lupine_disconnect(mp_obj_t signal_in, mp_obj_t id_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        api->DisconnectSignal(mp_obj_str_get_str(signal_in),
                              static_cast<uint64_t>(mp_obj_get_int(id_in)));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_disconnect_obj, lupine_disconnect);

static mp_obj_t lupine_is_connected(mp_obj_t signal_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    return mp_obj_new_bool(api ? api->IsSignalConnected(mp_obj_str_get_str(signal_in)) : false);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_is_connected_obj, lupine_is_connected);

static mp_obj_t lupine_add_user_signal(mp_obj_t name_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) api->AddUserSignal(mp_obj_str_get_str(name_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_add_user_signal_obj, lupine_add_user_signal);

static mp_obj_t lupine_call_deferred(size_t n_args, const mp_obj_t* args) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        nlohmann::json payload = nlohmann::json::array();
        for (size_t i = 1; i < n_args; ++i) {
            payload.push_back(MpToJson(args[i]));
        }
        api->CallDeferred(mp_obj_str_get_str(args[0]), payload);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR(lupine_call_deferred_obj, 1, lupine_call_deferred);

static mp_obj_t lupine_emit_event(size_t n_args, const mp_obj_t* args) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        nlohmann::json payload = nlohmann::json::array();
        for (size_t i = 1; i < n_args; ++i) {
            payload.push_back(MpToJson(args[i]));
        }
        api->EmitEvent(mp_obj_str_get_str(args[0]), payload);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR(lupine_emit_event_obj, 1, lupine_emit_event);

static mp_obj_t lupine_subscribe(size_t n_args, const mp_obj_t* args) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        uint32_t flags = (n_args > 2) ? static_cast<uint32_t>(mp_obj_get_int(args[2])) : 0;
        return mp_obj_new_int_from_uint(
            api->SubscribeEvent(mp_obj_str_get_str(args[0]), mp_obj_str_get_str(args[1]), flags));
    }
    return mp_obj_new_int(0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_subscribe_obj, 2, 3, lupine_subscribe);

static mp_obj_t lupine_unsubscribe(mp_obj_t event_in, mp_obj_t id_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        api->UnsubscribeEvent(mp_obj_str_get_str(event_in),
                              static_cast<uint64_t>(mp_obj_get_int(id_in)));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_unsubscribe_obj, lupine_unsubscribe);

// ============================================================================
// INPUT - ACTIVE DEVICE DETECTION
// ============================================================================

STATIC mp_obj_t lupine_get_active_device_type(void) {
    auto* api = GetCurrentScriptAPI();
    return mp_obj_new_int(api ? api->GetActiveDeviceType() : 0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_active_device_type_obj, lupine_get_active_device_type);

STATIC mp_obj_t lupine_get_last_gamepad_id(void) {
    auto* api = GetCurrentScriptAPI();
    return mp_obj_new_int(api ? api->GetLastGamepadId() : 0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_last_gamepad_id_obj, lupine_get_last_gamepad_id);

STATIC mp_obj_t lupine_get_gamepad_type(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    int gamepadId = (n_args > 0) ? mp_obj_get_int(args[0]) : 0;
    return mp_obj_new_int(api ? api->GetGamepadType(gamepadId) : 0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_get_gamepad_type_obj, 0, 1, lupine_get_gamepad_type);

// ============================================================================
// INPUT - CONTEXTS / ACTION SETS
// ============================================================================

STATIC mp_obj_t lupine_enable_input_context(mp_obj_t ctx_in) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->EnableInputContext(mp_obj_str_get_str(ctx_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_enable_input_context_obj, lupine_enable_input_context);

STATIC mp_obj_t lupine_disable_input_context(mp_obj_t ctx_in) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->DisableInputContext(mp_obj_str_get_str(ctx_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_disable_input_context_obj, lupine_disable_input_context);

STATIC mp_obj_t lupine_set_input_context_active(mp_obj_t ctx_in, mp_obj_t active_in) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->SetInputContextActive(mp_obj_str_get_str(ctx_in), mp_obj_is_true(active_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_set_input_context_active_obj, lupine_set_input_context_active);

STATIC mp_obj_t lupine_is_input_context_active(mp_obj_t ctx_in) {
    auto* api = GetCurrentScriptAPI();
    return (api && api->IsInputContextActive(mp_obj_str_get_str(ctx_in))) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_is_input_context_active_obj, lupine_is_input_context_active);

STATIC mp_obj_t lupine_set_exclusive_input_context(mp_obj_t ctx_in) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->SetExclusiveInputContext(mp_obj_str_get_str(ctx_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_exclusive_input_context_obj, lupine_set_exclusive_input_context);

STATIC mp_obj_t lupine_get_active_input_contexts(void) {
    auto* api = GetCurrentScriptAPI();
    mp_obj_t list = mp_obj_new_list(0, nullptr);
    if (api) {
        for (const std::string& c : api->GetActiveInputContexts()) {
            mp_obj_list_append(list, mp_obj_new_str(c.c_str(), c.size()));
        }
    }
    return list;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_active_input_contexts_obj, lupine_get_active_input_contexts);

STATIC mp_obj_t lupine_set_action_enabled(mp_obj_t action_in, mp_obj_t enabled_in) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->SetActionEnabled(mp_obj_str_get_str(action_in), mp_obj_is_true(enabled_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_set_action_enabled_obj, lupine_set_action_enabled);

STATIC mp_obj_t lupine_set_axis_enabled(mp_obj_t axis_in, mp_obj_t enabled_in) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->SetAxisEnabled(mp_obj_str_get_str(axis_in), mp_obj_is_true(enabled_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_set_axis_enabled_obj, lupine_set_axis_enabled);

// ============================================================================
// INPUT - LOCAL MULTIPLAYER PLAYER SLOTS
// ============================================================================

STATIC mp_obj_t lupine_set_player_count(mp_obj_t count_in) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->SetPlayerCount(mp_obj_get_int(count_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_player_count_obj, lupine_set_player_count);

STATIC mp_obj_t lupine_get_player_count(void) {
    auto* api = GetCurrentScriptAPI();
    return mp_obj_new_int(api ? api->GetPlayerCount() : 0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_player_count_obj, lupine_get_player_count);

STATIC mp_obj_t lupine_clear_player_assignments(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->ClearPlayerAssignments();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_clear_player_assignments_obj, lupine_clear_player_assignments);

STATIC mp_obj_t lupine_assign_keyboard_mouse_to_player(mp_obj_t player_in) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->AssignKeyboardMouseToPlayer(mp_obj_get_int(player_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_assign_keyboard_mouse_to_player_obj, lupine_assign_keyboard_mouse_to_player);

STATIC mp_obj_t lupine_assign_gamepad_to_player(mp_obj_t player_in, mp_obj_t gamepad_in) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->AssignGamepadToPlayer(mp_obj_get_int(player_in), mp_obj_get_int(gamepad_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_assign_gamepad_to_player_obj, lupine_assign_gamepad_to_player);

STATIC mp_obj_t lupine_unassign_gamepad(mp_obj_t gamepad_in) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->UnassignGamepad(mp_obj_get_int(gamepad_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_unassign_gamepad_obj, lupine_unassign_gamepad);

STATIC mp_obj_t lupine_get_player_for_gamepad(mp_obj_t gamepad_in) {
    auto* api = GetCurrentScriptAPI();
    return mp_obj_new_int(api ? api->GetPlayerForGamepad(mp_obj_get_int(gamepad_in)) : -1);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_get_player_for_gamepad_obj, lupine_get_player_for_gamepad);

STATIC mp_obj_t lupine_get_player_for_keyboard_mouse(void) {
    auto* api = GetCurrentScriptAPI();
    return mp_obj_new_int(api ? api->GetPlayerForKeyboardMouse() : -1);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_player_for_keyboard_mouse_obj, lupine_get_player_for_keyboard_mouse);

STATIC mp_obj_t lupine_player_owns_keyboard_mouse(mp_obj_t player_in) {
    auto* api = GetCurrentScriptAPI();
    return (api && api->PlayerOwnsKeyboardMouse(mp_obj_get_int(player_in))) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_player_owns_keyboard_mouse_obj, lupine_player_owns_keyboard_mouse);

STATIC mp_obj_t lupine_get_player_gamepads(mp_obj_t player_in) {
    auto* api = GetCurrentScriptAPI();
    mp_obj_t list = mp_obj_new_list(0, nullptr);
    if (api) {
        for (int id : api->GetPlayerGamepads(mp_obj_get_int(player_in))) {
            mp_obj_list_append(list, mp_obj_new_int(id));
        }
    }
    return list;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_get_player_gamepads_obj, lupine_get_player_gamepads);

STATIC mp_obj_t lupine_set_auto_join_enabled(mp_obj_t enabled_in) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->SetAutoJoinEnabled(mp_obj_is_true(enabled_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_auto_join_enabled_obj, lupine_set_auto_join_enabled);

STATIC mp_obj_t lupine_is_auto_join_enabled(void) {
    auto* api = GetCurrentScriptAPI();
    return (api && api->IsAutoJoinEnabled()) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_is_auto_join_enabled_obj, lupine_is_auto_join_enabled);

// ============================================================================
// INPUT - RUNTIME REBINDING
// ============================================================================

STATIC mp_obj_t lupine_add_action_key(mp_obj_t action_in, mp_obj_t key_in) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->AddActionKey(mp_obj_str_get_str(action_in), mp_obj_get_int(key_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_add_action_key_obj, lupine_add_action_key);

STATIC mp_obj_t lupine_add_action_mouse_button(mp_obj_t action_in, mp_obj_t button_in) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->AddActionMouseButton(mp_obj_str_get_str(action_in), mp_obj_get_int(button_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_add_action_mouse_button_obj, lupine_add_action_mouse_button);

STATIC mp_obj_t lupine_add_action_gamepad_button(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    int gamepadId = (n_args > 2) ? mp_obj_get_int(args[2]) : 0;
    if (api) api->AddActionGamepadButton(mp_obj_str_get_str(args[0]), mp_obj_get_int(args[1]), gamepadId);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_add_action_gamepad_button_obj, 2, 3, lupine_add_action_gamepad_button);

STATIC mp_obj_t lupine_add_action_gamepad_axis(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    float scale = (n_args > 2) ? mp_obj_get_float(args[2]) : 1.0f;
    int gamepadId = (n_args > 3) ? mp_obj_get_int(args[3]) : 0;
    if (api) api->AddActionGamepadAxis(mp_obj_str_get_str(args[0]), mp_obj_get_int(args[1]), scale, gamepadId);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_add_action_gamepad_axis_obj, 2, 4, lupine_add_action_gamepad_axis);

STATIC mp_obj_t lupine_remove_action_binding(mp_obj_t action_in, mp_obj_t index_in) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->RemoveActionBinding(mp_obj_str_get_str(action_in), mp_obj_get_int(index_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_remove_action_binding_obj, lupine_remove_action_binding);

STATIC mp_obj_t lupine_clear_action_bindings(mp_obj_t action_in) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->ClearActionBindings(mp_obj_str_get_str(action_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_clear_action_bindings_obj, lupine_clear_action_bindings);

STATIC mp_obj_t lupine_get_action_bindings(mp_obj_t action_in) {
    auto* api = GetCurrentScriptAPI();
    if (!api) return mp_obj_new_list(0, nullptr);
    return JsonToMp(api->GetActionBindings(mp_obj_str_get_str(action_in)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_get_action_bindings_obj, lupine_get_action_bindings);

STATIC mp_obj_t lupine_add_axis_key(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    float scale = (n_args > 2) ? mp_obj_get_float(args[2]) : 1.0f;
    if (api) api->AddAxisKey(mp_obj_str_get_str(args[0]), mp_obj_get_int(args[1]), scale);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_add_axis_key_obj, 2, 3, lupine_add_axis_key);

STATIC mp_obj_t lupine_add_axis_gamepad_axis(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    float scale = (n_args > 2) ? mp_obj_get_float(args[2]) : 1.0f;
    int gamepadId = (n_args > 3) ? mp_obj_get_int(args[3]) : 0;
    if (api) api->AddAxisGamepadAxis(mp_obj_str_get_str(args[0]), mp_obj_get_int(args[1]), scale, gamepadId);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_add_axis_gamepad_axis_obj, 2, 4, lupine_add_axis_gamepad_axis);

STATIC mp_obj_t lupine_remove_axis_binding(mp_obj_t axis_in, mp_obj_t index_in) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->RemoveAxisBinding(mp_obj_str_get_str(axis_in), mp_obj_get_int(index_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_remove_axis_binding_obj, lupine_remove_axis_binding);

STATIC mp_obj_t lupine_clear_axis_bindings(mp_obj_t axis_in) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->ClearAxisBindings(mp_obj_str_get_str(axis_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_clear_axis_bindings_obj, lupine_clear_axis_bindings);

STATIC mp_obj_t lupine_get_axis_bindings(mp_obj_t axis_in) {
    auto* api = GetCurrentScriptAPI();
    if (!api) return mp_obj_new_list(0, nullptr);
    return JsonToMp(api->GetAxisBindings(mp_obj_str_get_str(axis_in)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_get_axis_bindings_obj, lupine_get_axis_bindings);

STATIC mp_obj_t lupine_save_input_map(mp_obj_t path_in) {
    auto* api = GetCurrentScriptAPI();
    return (api && api->SaveInputMap(mp_obj_str_get_str(path_in))) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_save_input_map_obj, lupine_save_input_map);

STATIC mp_obj_t lupine_load_input_map(mp_obj_t path_in) {
    auto* api = GetCurrentScriptAPI();
    return (api && api->LoadInputMap(mp_obj_str_get_str(path_in))) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_load_input_map_obj, lupine_load_input_map);

// ============================================================================
// INPUT - CAPTURE (rebind menus)
// ============================================================================

STATIC mp_obj_t lupine_start_input_capture(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->StartInputCapture();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_start_input_capture_obj, lupine_start_input_capture);

STATIC mp_obj_t lupine_start_input_capture_mask(mp_obj_t kb_in, mp_obj_t mouse_in, mp_obj_t gp_in) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->StartInputCaptureMask(mp_obj_is_true(kb_in), mp_obj_is_true(mouse_in), mp_obj_is_true(gp_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lupine_start_input_capture_mask_obj, lupine_start_input_capture_mask);

STATIC mp_obj_t lupine_cancel_input_capture(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->CancelInputCapture();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_cancel_input_capture_obj, lupine_cancel_input_capture);

STATIC mp_obj_t lupine_is_capturing_input(void) {
    auto* api = GetCurrentScriptAPI();
    return (api && api->IsCapturingInput()) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_is_capturing_input_obj, lupine_is_capturing_input);

STATIC mp_obj_t lupine_is_input_capture_complete(void) {
    auto* api = GetCurrentScriptAPI();
    return (api && api->IsInputCaptureComplete()) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_is_input_capture_complete_obj, lupine_is_input_capture_complete);

STATIC mp_obj_t lupine_get_captured_binding(void) {
    auto* api = GetCurrentScriptAPI();
    if (!api) return mp_const_none;
    return JsonToMp(api->GetCapturedBinding());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_captured_binding_obj, lupine_get_captured_binding);

STATIC mp_obj_t lupine_clear_captured_binding(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->ClearCapturedBinding();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_clear_captured_binding_obj, lupine_clear_captured_binding);

STATIC mp_obj_t lupine_apply_captured_binding_to_action(mp_obj_t action_in) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->ApplyCapturedBindingToAction(mp_obj_str_get_str(action_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_apply_captured_binding_to_action_obj, lupine_apply_captured_binding_to_action);

// ============================================================================
// INPUT - GLYPH / PROMPT RESOLUTION
// ============================================================================

STATIC mp_obj_t lupine_get_action_glyph(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api) return mp_const_none;
    int player = (n_args > 1) ? mp_obj_get_int(args[1]) : -1;
    int deviceOverride = (n_args > 2) ? mp_obj_get_int(args[2]) : -1;
    return JsonToMp(api->GetActionGlyph(mp_obj_str_get_str(args[0]), player, deviceOverride));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_get_action_glyph_obj, 1, 3, lupine_get_action_glyph);

STATIC mp_obj_t lupine_get_action_glyphs(mp_obj_t action_in) {
    auto* api = GetCurrentScriptAPI();
    if (!api) return mp_obj_new_list(0, nullptr);
    return JsonToMp(api->GetActionGlyphs(mp_obj_str_get_str(action_in)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_get_action_glyphs_obj, lupine_get_action_glyphs);

STATIC mp_obj_t lupine_set_glyph_label(mp_obj_t glyph_in, mp_obj_t label_in) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->SetGlyphLabel(mp_obj_str_get_str(glyph_in), mp_obj_str_get_str(label_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_set_glyph_label_obj, lupine_set_glyph_label);

STATIC mp_obj_t lupine_set_glyph_art(mp_obj_t glyph_in, mp_obj_t art_in) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->SetGlyphArt(mp_obj_str_get_str(glyph_in), mp_obj_str_get_str(art_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_set_glyph_art_obj, lupine_set_glyph_art);

STATIC mp_obj_t lupine_clear_glyph_override(mp_obj_t glyph_in) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->ClearGlyphOverride(mp_obj_str_get_str(glyph_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_clear_glyph_override_obj, lupine_clear_glyph_override);

STATIC mp_obj_t lupine_clear_glyph_overrides(void) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->ClearGlyphOverrides();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_clear_glyph_overrides_obj, lupine_clear_glyph_overrides);

STATIC mp_obj_t lupine_load_glyph_map(mp_obj_t path_in) {
    auto* api = GetCurrentScriptAPI();
    return (api && api->LoadGlyphMap(mp_obj_str_get_str(path_in))) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_load_glyph_map_obj, lupine_load_glyph_map);

STATIC mp_obj_t lupine_save_glyph_map(mp_obj_t path_in) {
    auto* api = GetCurrentScriptAPI();
    return (api && api->SaveGlyphMap(mp_obj_str_get_str(path_in))) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_save_glyph_map_obj, lupine_save_glyph_map);

// ============================================================================
// INPUT - ACTION DELEGATION
// ============================================================================

STATIC mp_obj_t lupine_connect_action(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api) return mp_obj_new_int(0);
    uint32_t flags = (n_args > 2) ? static_cast<uint32_t>(mp_obj_get_int(args[2])) : 0;
    return mp_obj_new_int_from_uint(
        api->ConnectInputAction(mp_obj_str_get_str(args[0]), mp_obj_str_get_str(args[1]), flags));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_connect_action_obj, 2, 3, lupine_connect_action);

STATIC mp_obj_t lupine_disconnect_action(mp_obj_t action_in, mp_obj_t id_in) {
    auto* api = GetCurrentScriptAPI();
    if (api) api->DisconnectInputAction(mp_obj_str_get_str(action_in),
                                        static_cast<uint64_t>(mp_obj_get_int(id_in)));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_disconnect_action_obj, lupine_disconnect_action);

STATIC mp_obj_t lupine_connect_device_changed(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api) return mp_obj_new_int(0);
    uint32_t flags = (n_args > 1) ? static_cast<uint32_t>(mp_obj_get_int(args[1])) : 0;
    return mp_obj_new_int_from_uint(api->ConnectDeviceChanged(mp_obj_str_get_str(args[0]), flags));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_connect_device_changed_obj, 1, 2, lupine_connect_device_changed);

STATIC mp_obj_t lupine_connect_input_captured(size_t n_args, const mp_obj_t* args) {
    auto* api = GetCurrentScriptAPI();
    if (!api) return mp_obj_new_int(0);
    uint32_t flags = (n_args > 1) ? static_cast<uint32_t>(mp_obj_get_int(args[1])) : 0;
    return mp_obj_new_int_from_uint(api->ConnectInputCaptured(mp_obj_str_get_str(args[0]), flags));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_connect_input_captured_obj, 1, 2, lupine_connect_input_captured);

// ============================================================================
// INPUT - EVENT-DRIVEN ACTION MATCHING (inside on_input_event)
// ============================================================================

STATIC mp_obj_t lupine_event_is_action(mp_obj_t event_in, mp_obj_t action_in) {
    auto* api = GetCurrentScriptAPI();
    return (api && api->EventIsAction(MpToJson(event_in), mp_obj_str_get_str(action_in)))
               ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_event_is_action_obj, lupine_event_is_action);

STATIC mp_obj_t lupine_event_is_action_pressed(mp_obj_t event_in, mp_obj_t action_in) {
    auto* api = GetCurrentScriptAPI();
    return (api && api->EventIsActionPressed(MpToJson(event_in), mp_obj_str_get_str(action_in)))
               ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_event_is_action_pressed_obj, lupine_event_is_action_pressed);

STATIC mp_obj_t lupine_event_is_action_released(mp_obj_t event_in, mp_obj_t action_in) {
    auto* api = GetCurrentScriptAPI();
    return (api && api->EventIsActionReleased(MpToJson(event_in), mp_obj_str_get_str(action_in)))
               ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_event_is_action_released_obj, lupine_event_is_action_released);

// --- Camera control methods (Camera2D / CameraUI) ---------------------------

static mp_obj_t node_camera_shake(size_t n_args, const mp_obj_t* args) {
    std::shared_ptr<core::Node> node = node_ref(args[0]).Lock();
    float amplitude = mp_obj_get_float(args[1]);
    float duration = mp_obj_get_float(args[2]);
    float frequency = (n_args >= 4) ? mp_obj_get_float(args[3]) : 30.0f;
    if (auto cam2d = std::dynamic_pointer_cast<core::Camera2D>(node)) {
        cam2d->Shake(amplitude, duration, frequency);
    } else if (auto camui = std::dynamic_pointer_cast<core::CameraUI>(node)) {
        camui->Shake(amplitude, duration, frequency);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(node_camera_shake_obj, 3, 4, node_camera_shake);

static mp_obj_t node_camera_stop_shake(mp_obj_t self_in) {
    std::shared_ptr<core::Node> node = node_ref(self_in).Lock();
    if (auto cam2d = std::dynamic_pointer_cast<core::Camera2D>(node)) {
        cam2d->StopShake();
    } else if (auto camui = std::dynamic_pointer_cast<core::CameraUI>(node)) {
        camui->StopShake();
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_camera_stop_shake_obj, node_camera_stop_shake);

static mp_obj_t node_camera_is_shaking(mp_obj_t self_in) {
    std::shared_ptr<core::Node> node = node_ref(self_in).Lock();
    if (auto cam2d = std::dynamic_pointer_cast<core::Camera2D>(node)) {
        return mp_obj_new_bool(cam2d->IsShaking());
    }
    if (auto camui = std::dynamic_pointer_cast<core::CameraUI>(node)) {
        return mp_obj_new_bool(camui->IsShaking());
    }
    return mp_obj_new_bool(false);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_camera_is_shaking_obj, node_camera_is_shaking);

static mp_obj_t node_camera_set_follow_target(mp_obj_t self_in, mp_obj_t target_in) {
    std::shared_ptr<core::Node> node = node_ref(self_in).Lock();
    std::shared_ptr<core::Node> tgt;
    NodeRef targetRef;
    if (node_arg(target_in, targetRef)) {
        tgt = targetRef.Lock();
    }
    if (auto cam2d = std::dynamic_pointer_cast<core::Camera2D>(node)) {
        cam2d->SetFollowTarget(tgt);
    } else if (auto camui = std::dynamic_pointer_cast<core::CameraUI>(node)) {
        camui->SetFollowTarget(tgt);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(node_camera_set_follow_target_obj, node_camera_set_follow_target);

static mp_obj_t node_camera_clear_follow_target(mp_obj_t self_in) {
    std::shared_ptr<core::Node> node = node_ref(self_in).Lock();
    if (auto cam2d = std::dynamic_pointer_cast<core::Camera2D>(node)) {
        cam2d->ClearFollowTarget();
    } else if (auto camui = std::dynamic_pointer_cast<core::CameraUI>(node)) {
        camui->ClearFollowTarget();
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_camera_clear_follow_target_obj, node_camera_clear_follow_target);

static mp_obj_t node_camera_smooth_move_to(size_t n_args, const mp_obj_t* args) {
    std::shared_ptr<core::Node> node = node_ref(args[0]).Lock();
    float x = mp_obj_get_float(args[1]);
    float y = mp_obj_get_float(args[2]);
    float speed = (n_args >= 4) ? mp_obj_get_float(args[3]) : 0.0f;
    if (auto camui = std::dynamic_pointer_cast<core::CameraUI>(node)) {
        camui->SmoothMoveTo(math::Vec2(x, y), speed);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(node_camera_smooth_move_to_obj, 3, 4, node_camera_smooth_move_to);

static mp_obj_t node_camera_get_effective_position(mp_obj_t self_in) {
    std::shared_ptr<core::Node> node = node_ref(self_in).Lock();
    if (auto cam2d = std::dynamic_pointer_cast<core::Camera2D>(node)) {
        math::Vec2 p = cam2d->GetEffectivePosition();
        return make_vec2(p.x, p.y);
    }
    if (auto camui = std::dynamic_pointer_cast<core::CameraUI>(node)) {
        math::Vec2 p = camui->GetEffectivePosition();
        return make_vec2(p.x, p.y);
    }
    return make_vec2(0.0f, 0.0f);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_camera_get_effective_position_obj, node_camera_get_effective_position);

// --- Particle methods (Particles2D / Particles3D) ---------------------------

static mp_obj_t node_particles_restart(mp_obj_t self_in) {
    std::shared_ptr<core::Node> node = node_ref(self_in).Lock();
    if (node) {
        if (auto p2d = node->GetComponent<components::Particles2D>()) {
            p2d->Restart();
        } else if (auto p3d = node->GetComponent<components::Particles3D>()) {
            p3d->Restart();
        }
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_particles_restart_obj, node_particles_restart);

static mp_obj_t node_particles_emit_burst(mp_obj_t self_in, mp_obj_t count_in) {
    std::shared_ptr<core::Node> node = node_ref(self_in).Lock();
    int count = mp_obj_get_int(count_in);
    if (node) {
        if (auto p2d = node->GetComponent<components::Particles2D>()) {
            p2d->EmitBurst(count);
        } else if (auto p3d = node->GetComponent<components::Particles3D>()) {
            p3d->EmitBurst(count);
        }
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(node_particles_emit_burst_obj, node_particles_emit_burst);

static mp_obj_t node_particles_set_emitting(mp_obj_t self_in, mp_obj_t emitting_in) {
    std::shared_ptr<core::Node> node = node_ref(self_in).Lock();
    bool emitting = mp_obj_is_true(emitting_in);
    if (node) {
        if (auto p2d = node->GetComponent<components::Particles2D>()) {
            p2d->SetEmitting(emitting);
        } else if (auto p3d = node->GetComponent<components::Particles3D>()) {
            p3d->SetEmitting(emitting);
        }
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(node_particles_set_emitting_obj, node_particles_set_emitting);

static mp_obj_t node_particles_is_emitting(mp_obj_t self_in) {
    std::shared_ptr<core::Node> node = node_ref(self_in).Lock();
    if (node) {
        if (auto p2d = node->GetComponent<components::Particles2D>()) {
            return mp_obj_new_bool(p2d->GetEmitting());
        }
        if (auto p3d = node->GetComponent<components::Particles3D>()) {
            return mp_obj_new_bool(p3d->GetEmitting());
        }
    }
    return mp_obj_new_bool(false);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_particles_is_emitting_obj, node_particles_is_emitting);

static mp_obj_t node_particles_get_alive_count(mp_obj_t self_in) {
    std::shared_ptr<core::Node> node = node_ref(self_in).Lock();
    if (node) {
        if (auto p2d = node->GetComponent<components::Particles2D>()) {
            return mp_obj_new_int(p2d->GetAliveCount());
        }
        if (auto p3d = node->GetComponent<components::Particles3D>()) {
            return mp_obj_new_int(p3d->GetAliveCount());
        }
    }
    return mp_obj_new_int(0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(node_particles_get_alive_count_obj, node_particles_get_alive_count);

// --- Theme methods (UIControl) ----------------------------------------------

static mp_obj_t node_set_theme(mp_obj_t self_in, mp_obj_t path_obj) {
    std::shared_ptr<core::Node> node = node_ref(self_in).Lock();
    std::shared_ptr<components::UIControl> ui = node ? node->GetComponent<components::UIControl>() : nullptr;
    if (ui) {
        ui->SetThemePath(mp_obj_str_get_str(path_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(node_set_theme_obj, node_set_theme);

static mp_obj_t node_set_theme_type_variation(mp_obj_t self_in, mp_obj_t var_obj) {
    std::shared_ptr<core::Node> node = node_ref(self_in).Lock();
    std::shared_ptr<components::UIControl> ui = node ? node->GetComponent<components::UIControl>() : nullptr;
    if (ui) {
        ui->SetThemeTypeVariation(mp_obj_str_get_str(var_obj));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(node_set_theme_type_variation_obj, node_set_theme_type_variation);

static mp_obj_t node_clear_theme_override(mp_obj_t self_in, mp_obj_t prop_obj) {
    std::shared_ptr<core::Node> node = node_ref(self_in).Lock();
    std::shared_ptr<components::UIControl> ui = node ? node->GetComponent<components::UIControl>() : nullptr;
    if (ui) {
        ui->SetThemeOverride(mp_obj_str_get_str(prop_obj), false);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(node_clear_theme_override_obj, node_clear_theme_override);

// Method dispatch table consulted by the attr handler (runtime name match,
// since this build does not generate static qstrs for our symbols).
static const struct { const char* name; const void* obj; } s_NodeMethods[] = {
    {"is_valid", &node_is_valid_obj},
    {"get_name", &node_get_name_obj},
    {"set_name", &node_set_name_obj},
    {"get_uuid", &node_get_uuid_obj},
    {"get_path", &node_get_path_obj},
    {"get_type_name", &node_get_type_name_obj},
    {"is_active", &node_is_active_obj},
    {"set_active", &node_set_active_obj},
    {"is_visible", &node_is_visible_obj},
    {"set_visible", &node_set_visible_obj},
    {"is_unique_name_in_owner", &node_is_unique_obj},
    {"set_unique_name_in_owner", &node_set_unique_obj},
    {"get_child_count", &node_get_child_count_obj},
    {"has_node", &node_has_node_obj},
    {"has_component", &node_has_component_obj},
    {"has_property", &node_has_property_obj},
    {"queue_free", &node_queue_free_obj},
    {"queue_free_deferred", &node_queue_free_deferred_obj},
    {"free", &node_free_obj},
    {"get_parent", &node_get_parent_obj},
    {"get_child", &node_get_child_obj},
    {"get_child_at", &node_get_child_at_obj},
    {"get_children", &node_get_children_obj},
    {"find_node", &node_find_node_obj},
    {"get_node", &node_find_node_obj},
    {"add_child", &node_add_child_obj},
    {"remove_child", &node_remove_child_obj},
    {"reparent_to", &node_reparent_to_obj},
    {"get_sibling_index", &node_get_sibling_index_obj},
    {"set_sibling_index", &node_set_sibling_index_obj},
    {"get_child_index", &node_get_child_index_obj},
    {"move_child", &node_move_child_obj},
    {"create_tween", &node_create_tween_obj},
    {"create_sequence", &node_create_sequence_obj},
    {"await_signal", &node_await_signal_obj},
    {"duplicate", &node_duplicate_obj},
    {"distance_to", &node_distance_to_obj},
    {"get_position_2d", &node_get_position_2d_obj},
    {"set_position_2d", &node_set_position_2d_obj},
    {"translate_2d", &node_translate_2d_obj},
    {"get_rotation_2d", &node_get_rotation_2d_obj},
    {"set_rotation_2d", &node_set_rotation_2d_obj},
    {"get_scale_2d", &node_get_scale_2d_obj},
    {"set_scale_2d", &node_set_scale_2d_obj},
    {"get_position_3d", &node_get_position_3d_obj},
    {"set_position_3d", &node_set_position_3d_obj},
    {"translate_3d", &node_translate_3d_obj},
    {"get_rotation_3d", &node_get_rotation_3d_obj},
    {"set_rotation_3d", &node_set_rotation_3d_obj},
    {"get_scale_3d", &node_get_scale_3d_obj},
    {"set_scale_3d", &node_set_scale_3d_obj},
    {"get_global_position_2d", &node_get_global_position_2d_obj},
    {"set_global_position_2d", &node_set_global_position_2d_obj},
    {"get_global_rotation_2d", &node_get_global_rotation_2d_obj},
    {"set_global_rotation_2d", &node_set_global_rotation_2d_obj},
    {"get_global_scale_2d", &node_get_global_scale_2d_obj},
    {"get_global_position_3d", &node_get_global_position_3d_obj},
    {"set_global_position_3d", &node_set_global_position_3d_obj},
    {"get_global_rotation_3d", &node_get_global_rotation_3d_obj},
    {"set_global_rotation_3d", &node_set_global_rotation_3d_obj},
    {"get_global_scale_3d", &node_get_global_scale_3d_obj},
    {"get", &node_get_obj},
    {"set", &node_set_obj},
    {"has_method", &node_has_method_obj},
    {"call", &node_call_obj},
    {"get_component", &node_get_component_obj},
    {"get_components", &node_get_components_obj},
    {"add_component", &node_add_component_obj},
    {"remove_component", &node_remove_component_obj},
    {"emit", &node_emit_obj},
    {"connect", &node_connect_obj},
    {"disconnect", &node_disconnect_obj},
    {"is_connected", &node_is_connected_obj},
    {"add_user_signal", &node_add_user_signal_obj},
    {"get_signal_list", &node_get_signal_list_obj},
    {"rpc", &node_rpc_obj},
    {"rpc_id", &node_rpc_id_obj},
    {"rpc_unreliable", &node_rpc_unreliable_obj},
    {"set_multiplayer_authority", &node_set_authority_obj},
    {"get_multiplayer_authority", &node_get_authority_obj},
    {"is_multiplayer_authority", &node_is_authority_obj},
    {"get_network_id", &node_get_network_id_obj},
    {"add_to_group", &node_add_to_group_obj},
    {"remove_from_group", &node_remove_from_group_obj},
    {"is_in_group", &node_is_in_group_obj},
    {"get_groups", &node_get_groups_obj},
    {"implements_interface", &node_implements_interface_obj},
    {"get_interfaces", &node_get_interfaces_obj},
    {"verify_interface", &node_verify_interface_obj},
    {"camera_shake", &node_camera_shake_obj},
    {"camera_stop_shake", &node_camera_stop_shake_obj},
    {"camera_is_shaking", &node_camera_is_shaking_obj},
    {"camera_set_follow_target", &node_camera_set_follow_target_obj},
    {"camera_clear_follow_target", &node_camera_clear_follow_target_obj},
    {"camera_smooth_move_to", &node_camera_smooth_move_to_obj},
    {"camera_get_effective_position", &node_camera_get_effective_position_obj},
    {"particles_restart", &node_particles_restart_obj},
    {"particles_emit_burst", &node_particles_emit_burst_obj},
    {"particles_set_emitting", &node_particles_set_emitting_obj},
    {"particles_is_emitting", &node_particles_is_emitting_obj},
    {"particles_get_alive_count", &node_particles_get_alive_count_obj},
    {"set_theme", &node_set_theme_obj},
    {"set_theme_type_variation", &node_set_theme_type_variation_obj},
    {"clear_theme_override", &node_clear_theme_override_obj},
    {nullptr, nullptr},
};

static void node_attr(mp_obj_t self_in, qstr attr, mp_obj_t* dest) {
    const char* name = qstr_str(attr);

    // Leave Python internals (dunder names) to the default machinery.
    if (name[0] == '_' && name[1] == '_') {
        return;
    }

    if (dest[0] == MP_OBJ_NULL) {
        for (int i = 0; s_NodeMethods[i].name != nullptr; ++i) {
            if (std::strcmp(name, s_NodeMethods[i].name) == 0) {
                dest[0] = MP_OBJ_FROM_PTR(s_NodeMethods[i].obj);
                dest[1] = self_in;
                return;
            }
        }
        dest[0] = JsonToMp(node_ref(self_in).Get(name));
        return;
    }

    if (dest[0] == MP_OBJ_SENTINEL && dest[1] != MP_OBJ_NULL) {
        node_ref(self_in).Set(name, MpToJson(dest[1]));
        dest[0] = MP_OBJ_NULL;
    }
}

MP_DEFINE_CONST_OBJ_TYPE(
    lupine_node_type,
    MP_QSTR_object,
    MP_TYPE_FLAG_NONE,
    attr, node_attr
);

static mp_obj_t lupine_get_node(mp_obj_t path_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_const_none;
    return wrap_node(api->FindNode(mp_obj_str_get_str(path_in)), api);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_get_node_obj, lupine_get_node);

static mp_obj_t lupine_get_singleton(mp_obj_t name_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_const_none;
    return wrap_node(api->GetSingleton(mp_obj_str_get_str(name_in)), api);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_get_singleton_obj, lupine_get_singleton);

// --- Groups (Lupine module functions, operate on the owner node) ------------

static mp_obj_t lupine_add_to_group(mp_obj_t group_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) api->AddToGroup(mp_obj_str_get_str(group_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_add_to_group_obj, lupine_add_to_group);

static mp_obj_t lupine_remove_from_group(mp_obj_t group_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) api->RemoveFromGroup(mp_obj_str_get_str(group_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_remove_from_group_obj, lupine_remove_from_group);

static mp_obj_t lupine_is_in_group(mp_obj_t group_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_const_false;
    return api->IsInGroup(mp_obj_str_get_str(group_in)) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_is_in_group_obj, lupine_is_in_group);

static mp_obj_t lupine_get_groups(void) {
    mp_obj_t list = mp_obj_new_list(0, nullptr);
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        for (const std::string& group : api->GetGroups()) {
            mp_obj_list_append(list, mp_obj_new_str(group.c_str(), group.size()));
        }
    }
    return list;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_groups_obj, lupine_get_groups);

static mp_obj_t lupine_get_nodes_in_group(mp_obj_t group_in) {
    mp_obj_t list = mp_obj_new_list(0, nullptr);
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        for (core::Node* node : api->GetNodesInGroup(mp_obj_str_get_str(group_in))) {
            mp_obj_list_append(list, wrap_node(node, api));
        }
    }
    return list;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_get_nodes_in_group_obj, lupine_get_nodes_in_group);

static mp_obj_t lupine_get_node_count_in_group(mp_obj_t group_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    return mp_obj_new_int(api ? api->GetNodeCountInGroup(mp_obj_str_get_str(group_in)) : 0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_get_node_count_in_group_obj, lupine_get_node_count_in_group);

// --- Interfaces (Lupine module functions) -----------------------------------

static mp_obj_t lupine_implements_interface(mp_obj_t name_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_const_false;
    return api->ImplementsInterface(mp_obj_str_get_str(name_in)) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_implements_interface_obj, lupine_implements_interface);

static mp_obj_t lupine_get_implemented_interfaces(void) {
    mp_obj_t list = mp_obj_new_list(0, nullptr);
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        for (const std::string& iface : api->GetImplementedInterfaces()) {
            mp_obj_list_append(list, mp_obj_new_str(iface.c_str(), iface.size()));
        }
    }
    return list;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_implemented_interfaces_obj, lupine_get_implemented_interfaces);

static mp_obj_t lupine_verify_interface(mp_obj_t name_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_const_none;
    return JsonToMp(api->VerifyInterface(mp_obj_str_get_str(name_in)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_verify_interface_obj, lupine_verify_interface);

static mp_obj_t lupine_get_nodes_with_interface(mp_obj_t name_in) {
    mp_obj_t list = mp_obj_new_list(0, nullptr);
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        for (core::Node* node : api->GetNodesImplementingInterface(mp_obj_str_get_str(name_in))) {
            mp_obj_list_append(list, wrap_node(node, api));
        }
    }
    return list;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_get_nodes_with_interface_obj, lupine_get_nodes_with_interface);

static mp_obj_t lupine_get_node_count_with_interface(mp_obj_t name_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    return mp_obj_new_int(api ? api->GetNodeCountImplementingInterface(mp_obj_str_get_str(name_in)) : 0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_get_node_count_with_interface_obj, lupine_get_node_count_with_interface);

static mp_obj_t lupine_get_first_node_with_interface(mp_obj_t name_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_const_none;
    return wrap_node(api->GetFirstNodeImplementingInterface(mp_obj_str_get_str(name_in)), api);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_get_first_node_with_interface_obj, lupine_get_first_node_with_interface);

static mp_obj_t lupine_interface_exists(mp_obj_t name_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_const_false;
    return api->InterfaceExists(mp_obj_str_get_str(name_in)) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_interface_exists_obj, lupine_interface_exists);

static mp_obj_t lupine_get_all_interfaces(void) {
    mp_obj_t list = mp_obj_new_list(0, nullptr);
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        for (const std::string& iface : api->GetAllInterfaces()) {
            mp_obj_list_append(list, mp_obj_new_str(iface.c_str(), iface.size()));
        }
    }
    return list;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_all_interfaces_obj, lupine_get_all_interfaces);

static mp_obj_t lupine_get_interface_definition(mp_obj_t name_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_const_none;
    nlohmann::json def = api->GetInterfaceDefinition(mp_obj_str_get_str(name_in));
    if (def.is_null()) return mp_const_none;
    return JsonToMp(def);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_get_interface_definition_obj, lupine_get_interface_definition);

static mp_obj_t lupine_register_interface(mp_obj_t def_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_const_false;
    return api->RegisterInterface(MpToJson(def_in)) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_register_interface_obj, lupine_register_interface);

static mp_obj_t lupine_archetype_implements_interface(mp_obj_t class_in, mp_obj_t name_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_const_false;
    return api->ArchetypeImplementsInterface(mp_obj_str_get_str(class_in),
                                             mp_obj_str_get_str(name_in)) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_archetype_implements_interface_obj, lupine_archetype_implements_interface);

static mp_obj_t lupine_get_archetypes_with_interface(mp_obj_t name_in) {
    mp_obj_t list = mp_obj_new_list(0, nullptr);
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        for (const std::string& cls : api->GetArchetypesImplementing(mp_obj_str_get_str(name_in))) {
            mp_obj_list_append(list, mp_obj_new_str(cls.c_str(), cls.size()));
        }
    }
    return list;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_get_archetypes_with_interface_obj, lupine_get_archetypes_with_interface);

// --- Tree utilities (Lupine module functions) -------------------------------

static mp_obj_t lupine_get_first_node_in_group(mp_obj_t group_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_const_none;
    return wrap_node(api->GetFirstNodeInGroup(mp_obj_str_get_str(group_in)), api);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_get_first_node_in_group_obj, lupine_get_first_node_in_group);

static mp_obj_t lupine_get_node_or_null(mp_obj_t path_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_const_none;
    return wrap_node(api->GetNodeOrNull(mp_obj_str_get_str(path_in)), api);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_get_node_or_null_obj, lupine_get_node_or_null);

static mp_obj_t lupine_find_children(size_t n_args, const mp_obj_t* args) {
    mp_obj_t list = mp_obj_new_list(0, nullptr);
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) {
        const char* type = n_args > 0 ? mp_obj_str_get_str(args[0]) : "";
        bool recursive = n_args > 1 ? mp_obj_is_true(args[1]) : true;
        for (core::Node* node : api->FindChildren(type, recursive)) {
            mp_obj_list_append(list, wrap_node(node, api));
        }
    }
    return list;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_find_children_obj, 1, 2, lupine_find_children);

static mp_obj_t lupine_is_ancestor_of(mp_obj_t other_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api || !mp_obj_is_type(other_in, &lupine_node_type)) return mp_const_false;
    core::Node* other = node_raw(other_in);
    return (other && api->IsAncestorOf(other)) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_is_ancestor_of_obj, lupine_is_ancestor_of);

static mp_obj_t lupine_find_node_by_uuid(mp_obj_t uuid_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_const_none;
    return wrap_node(api->FindNodeByUUID(mp_obj_str_get_str(uuid_in)), api);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_find_node_by_uuid_obj, lupine_find_node_by_uuid);

static mp_obj_t lupine_get_self(void) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_const_none;
    return wrap_node(api->GetSelf(), api);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_self_obj, lupine_get_self);

// ============================================================================
// TIMER / SCENE / TREE OBJECT MODEL
//
// Each handle stores plain pointers (no C++ destructor) for GC safety, mirroring
// the node/component handles. A transient TimerRef/SceneRef/TreeRef is rebuilt
// per call to perform the work.
// ============================================================================

extern const mp_obj_type_t lupine_timer_type;
extern const mp_obj_type_t lupine_scene_type;
extern const mp_obj_type_t lupine_tree_type;

typedef struct _lupine_timer_obj_t {
    mp_obj_base_t base;
    uint32_t slot;
    uint32_t generation;
} lupine_timer_obj_t;

// A scene has no shared_ptr the handle could weakly reference (scenes are owned
// outright by the SceneManager), so the handle stores the raw scene plus its tree
// and every resolve re-proves the scene is still one of that tree's live scenes.
// A handle captured before a scene switch therefore reports invalid instead of
// dereferencing the freed scene.
typedef struct _lupine_scene_obj_t {
    mp_obj_base_t base;
    core::Scene* scene;
    core::SceneManager* tree;
} lupine_scene_obj_t;

// The SceneManager is the process-lifetime tree itself, so a tree handle can hold
// it directly.
typedef struct _lupine_tree_obj_t {
    mp_obj_base_t base;
    core::SceneManager* manager;
} lupine_tree_obj_t;

static mp_obj_t wrap_timer_shared(const std::shared_ptr<core::Component>& timer, core::SceneManager* tree) {
    if (!timer) return mp_const_none;
    uint32_t generation = 0;
    const uint32_t slot = AcquireComponentSlot(timer, tree, generation);
    lupine_timer_obj_t* o = mp_obj_malloc(lupine_timer_obj_t, &lupine_timer_type);
    o->slot = slot;
    o->generation = generation;
    return MP_OBJ_FROM_PTR(o);
}
static mp_obj_t wrap_timer(core::Component* timer, ScriptAPI* api) {
    return wrap_timer_shared(SharedComponent(timer), ApiTree(api));
}
static mp_obj_t wrap_scene_tree(core::Scene* scene, core::SceneManager* tree) {
    if (!scene) return mp_const_none;
    lupine_scene_obj_t* o = mp_obj_malloc(lupine_scene_obj_t, &lupine_scene_type);
    o->scene = scene;
    o->tree = tree;
    return MP_OBJ_FROM_PTR(o);
}
static mp_obj_t wrap_scene(core::Scene* scene, ScriptAPI* api) {
    return wrap_scene_tree(scene, ApiTree(api));
}
static mp_obj_t wrap_tree_manager(core::SceneManager* manager) {
    if (!manager) return mp_const_none;
    lupine_tree_obj_t* o = mp_obj_malloc(lupine_tree_obj_t, &lupine_tree_type);
    o->manager = manager;
    return MP_OBJ_FROM_PTR(o);
}
static mp_obj_t wrap_tree(core::SceneManager* manager, ScriptAPI* api) {
    (void)api;
    return wrap_tree_manager(manager);
}

static TimerRef timer_ref(mp_obj_t self_in) {
    lupine_timer_obj_t* o = static_cast<lupine_timer_obj_t*>(MP_OBJ_TO_PTR(self_in));
    if (!o || o->slot >= s_ComponentSlots.size()) return TimerRef();
    ComponentHandleSlot& s = s_ComponentSlots[o->slot];
    if (!s.inUse || s.generation != o->generation) return TimerRef();
    return TimerRef(s.object, s.tree);
}
static SceneRef scene_ref(mp_obj_t self_in) {
    lupine_scene_obj_t* o = static_cast<lupine_scene_obj_t*>(MP_OBJ_TO_PTR(self_in));
    if (!o || !SceneRef::IsLiveScene(o->tree, o->scene)) return SceneRef();
    return SceneRef(o->scene, o->tree);
}
static TreeRef tree_ref(mp_obj_t self_in) {
    lupine_tree_obj_t* o = static_cast<lupine_tree_obj_t*>(MP_OBJ_TO_PTR(self_in));
    return TreeRef(o->manager, nullptr);
}

// --- Timer handle methods --------------------------------------------------

static mp_obj_t timer_is_valid(mp_obj_t self_in) { return mp_obj_new_bool(timer_ref(self_in).IsValid()); }
STATIC MP_DEFINE_CONST_FUN_OBJ_1(timer_is_valid_obj, timer_is_valid);
static mp_obj_t timer_get_name(mp_obj_t self_in) {
    std::string s = timer_ref(self_in).GetName();
    return mp_obj_new_str(s.c_str(), s.size());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(timer_get_name_obj, timer_get_name);
static mp_obj_t timer_start(mp_obj_t self_in) { timer_ref(self_in).Start(); return mp_const_none; }
STATIC MP_DEFINE_CONST_FUN_OBJ_1(timer_start_obj, timer_start);
static mp_obj_t timer_stop(mp_obj_t self_in) { timer_ref(self_in).Stop(); return mp_const_none; }
STATIC MP_DEFINE_CONST_FUN_OBJ_1(timer_stop_obj, timer_stop);
static mp_obj_t timer_reset(mp_obj_t self_in) { timer_ref(self_in).Reset(); return mp_const_none; }
STATIC MP_DEFINE_CONST_FUN_OBJ_1(timer_reset_obj, timer_reset);
static mp_obj_t timer_restart(mp_obj_t self_in) { timer_ref(self_in).Restart(); return mp_const_none; }
STATIC MP_DEFINE_CONST_FUN_OBJ_1(timer_restart_obj, timer_restart);
static mp_obj_t timer_remove(mp_obj_t self_in) { timer_ref(self_in).Remove(); return mp_const_none; }
STATIC MP_DEFINE_CONST_FUN_OBJ_1(timer_remove_obj, timer_remove);
static mp_obj_t timer_is_running(mp_obj_t self_in) { return mp_obj_new_bool(timer_ref(self_in).IsRunning()); }
STATIC MP_DEFINE_CONST_FUN_OBJ_1(timer_is_running_obj, timer_is_running);
static mp_obj_t timer_is_finished(mp_obj_t self_in) { return mp_obj_new_bool(timer_ref(self_in).IsFinished()); }
STATIC MP_DEFINE_CONST_FUN_OBJ_1(timer_is_finished_obj, timer_is_finished);
static mp_obj_t timer_get_time_left(mp_obj_t self_in) { return mp_obj_new_float(timer_ref(self_in).GetTimeLeft()); }
STATIC MP_DEFINE_CONST_FUN_OBJ_1(timer_get_time_left_obj, timer_get_time_left);
static mp_obj_t timer_get_fire_count(mp_obj_t self_in) { return mp_obj_new_int(timer_ref(self_in).GetFireCount()); }
STATIC MP_DEFINE_CONST_FUN_OBJ_1(timer_get_fire_count_obj, timer_get_fire_count);
static mp_obj_t timer_get_duration(mp_obj_t self_in) { return mp_obj_new_float(timer_ref(self_in).GetDuration()); }
STATIC MP_DEFINE_CONST_FUN_OBJ_1(timer_get_duration_obj, timer_get_duration);
static mp_obj_t timer_set_duration(mp_obj_t self_in, mp_obj_t v) {
    timer_ref(self_in).SetDuration(mp_obj_get_float(v));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(timer_set_duration_obj, timer_set_duration);
static mp_obj_t timer_get_elapsed(mp_obj_t self_in) { return mp_obj_new_float(timer_ref(self_in).GetElapsed()); }
STATIC MP_DEFINE_CONST_FUN_OBJ_1(timer_get_elapsed_obj, timer_get_elapsed);
static mp_obj_t timer_set_elapsed(mp_obj_t self_in, mp_obj_t v) {
    timer_ref(self_in).SetElapsed(mp_obj_get_float(v));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(timer_set_elapsed_obj, timer_set_elapsed);
static mp_obj_t timer_get_loop(mp_obj_t self_in) { return mp_obj_new_bool(timer_ref(self_in).GetLoop()); }
STATIC MP_DEFINE_CONST_FUN_OBJ_1(timer_get_loop_obj, timer_get_loop);
static mp_obj_t timer_set_loop(mp_obj_t self_in, mp_obj_t v) {
    timer_ref(self_in).SetLoop(mp_obj_is_true(v));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(timer_set_loop_obj, timer_set_loop);
static mp_obj_t timer_get_repeat_count(mp_obj_t self_in) { return mp_obj_new_int(timer_ref(self_in).GetRepeatCount()); }
STATIC MP_DEFINE_CONST_FUN_OBJ_1(timer_get_repeat_count_obj, timer_get_repeat_count);
static mp_obj_t timer_set_repeat_count(mp_obj_t self_in, mp_obj_t v) {
    timer_ref(self_in).SetRepeatCount(mp_obj_get_int(v));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(timer_set_repeat_count_obj, timer_set_repeat_count);
static mp_obj_t timer_get_owner(mp_obj_t self_in) {
    return wrap_node_ref(timer_ref(self_in).GetOwner());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(timer_get_owner_obj, timer_get_owner);
static mp_obj_t timer_as_component(mp_obj_t self_in) {
    return wrap_component_ref(timer_ref(self_in).AsComponent());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(timer_as_component_obj, timer_as_component);

static const struct { const char* name; const void* obj; } s_TimerMethods[] = {
    {"is_valid", &timer_is_valid_obj},
    {"get_name", &timer_get_name_obj},
    {"start", &timer_start_obj},
    {"stop", &timer_stop_obj},
    {"reset", &timer_reset_obj},
    {"restart", &timer_restart_obj},
    {"remove", &timer_remove_obj},
    {"is_running", &timer_is_running_obj},
    {"is_finished", &timer_is_finished_obj},
    {"get_time_left", &timer_get_time_left_obj},
    {"get_fire_count", &timer_get_fire_count_obj},
    {"get_duration", &timer_get_duration_obj},
    {"set_duration", &timer_set_duration_obj},
    {"get_elapsed", &timer_get_elapsed_obj},
    {"set_elapsed", &timer_set_elapsed_obj},
    {"get_loop", &timer_get_loop_obj},
    {"set_loop", &timer_set_loop_obj},
    {"get_repeat_count", &timer_get_repeat_count_obj},
    {"set_repeat_count", &timer_set_repeat_count_obj},
    {"get_owner", &timer_get_owner_obj},
    {"as_component", &timer_as_component_obj},
    {nullptr, nullptr},
};

static void timer_attr(mp_obj_t self_in, qstr attr, mp_obj_t* dest) {
    const char* name = qstr_str(attr);
    if (name[0] == '_' && name[1] == '_') return;
    if (dest[0] == MP_OBJ_NULL) {
        for (int i = 0; s_TimerMethods[i].name != nullptr; ++i) {
            if (std::strcmp(name, s_TimerMethods[i].name) == 0) {
                dest[0] = MP_OBJ_FROM_PTR(s_TimerMethods[i].obj);
                dest[1] = self_in;
                return;
            }
        }
    }
}

MP_DEFINE_CONST_OBJ_TYPE(
    lupine_timer_type,
    MP_QSTR_object,
    MP_TYPE_FLAG_NONE,
    attr, timer_attr
);

// --- Scene handle methods --------------------------------------------------

static mp_obj_t scene_is_valid(mp_obj_t self_in) { return mp_obj_new_bool(scene_ref(self_in).IsValid()); }
STATIC MP_DEFINE_CONST_FUN_OBJ_1(scene_is_valid_obj, scene_is_valid);
static mp_obj_t scene_get_name(mp_obj_t self_in) {
    std::string s = scene_ref(self_in).GetName();
    return mp_obj_new_str(s.c_str(), s.size());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(scene_get_name_obj, scene_get_name);
static mp_obj_t scene_get_path(mp_obj_t self_in) {
    std::string s = scene_ref(self_in).GetPath();
    return mp_obj_new_str(s.c_str(), s.size());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(scene_get_path_obj, scene_get_path);
static mp_obj_t scene_get_root(mp_obj_t self_in) {
    return wrap_node_ref(scene_ref(self_in).GetRoot());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(scene_get_root_obj, scene_get_root);
static mp_obj_t scene_find_node(mp_obj_t self_in, mp_obj_t path_in) {
    return wrap_node_ref(scene_ref(self_in).FindNode(mp_obj_str_get_str(path_in)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(scene_find_node_obj, scene_find_node);
static mp_obj_t scene_find_node_by_uuid(mp_obj_t self_in, mp_obj_t uuid_in) {
    return wrap_node_ref(scene_ref(self_in).FindNodeByUUID(mp_obj_str_get_str(uuid_in)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(scene_find_node_by_uuid_obj, scene_find_node_by_uuid);

static const struct { const char* name; const void* obj; } s_SceneMethods[] = {
    {"is_valid", &scene_is_valid_obj},
    {"get_name", &scene_get_name_obj},
    {"get_path", &scene_get_path_obj},
    {"get_root", &scene_get_root_obj},
    {"find_node", &scene_find_node_obj},
    {"find_node_by_uuid", &scene_find_node_by_uuid_obj},
    {nullptr, nullptr},
};

static void scene_attr(mp_obj_t self_in, qstr attr, mp_obj_t* dest) {
    const char* name = qstr_str(attr);
    if (name[0] == '_' && name[1] == '_') return;
    if (dest[0] == MP_OBJ_NULL) {
        for (int i = 0; s_SceneMethods[i].name != nullptr; ++i) {
            if (std::strcmp(name, s_SceneMethods[i].name) == 0) {
                dest[0] = MP_OBJ_FROM_PTR(s_SceneMethods[i].obj);
                dest[1] = self_in;
                return;
            }
        }
    }
}

MP_DEFINE_CONST_OBJ_TYPE(
    lupine_scene_type,
    MP_QSTR_object,
    MP_TYPE_FLAG_NONE,
    attr, scene_attr
);

// --- Tree handle methods ---------------------------------------------------

static mp_obj_t tree_is_valid(mp_obj_t self_in) { return mp_obj_new_bool(tree_ref(self_in).IsValid()); }
STATIC MP_DEFINE_CONST_FUN_OBJ_1(tree_is_valid_obj, tree_is_valid);
static mp_obj_t tree_get_root(mp_obj_t self_in) {
    return wrap_node_ref(tree_ref(self_in).GetRoot());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(tree_get_root_obj, tree_get_root);
static mp_obj_t tree_get_current_scene(mp_obj_t self_in) {
    TreeRef tree = tree_ref(self_in);
    return wrap_scene_tree(tree.GetCurrentScene().Get(), tree.Get());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(tree_get_current_scene_obj, tree_get_current_scene);
static mp_obj_t tree_get_current_scene_path(mp_obj_t self_in) {
    std::string s = tree_ref(self_in).GetCurrentScenePath();
    return mp_obj_new_str(s.c_str(), s.size());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(tree_get_current_scene_path_obj, tree_get_current_scene_path);
static mp_obj_t tree_change_scene(mp_obj_t self_in, mp_obj_t path_in) {
    tree_ref(self_in).ChangeScene(mp_obj_str_get_str(path_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(tree_change_scene_obj, tree_change_scene);
static mp_obj_t tree_reload_scene(mp_obj_t self_in) {
    tree_ref(self_in).ReloadScene();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(tree_reload_scene_obj, tree_reload_scene);
static mp_obj_t tree_add_scene(mp_obj_t self_in, mp_obj_t path_in) {
    tree_ref(self_in).AddScene(mp_obj_str_get_str(path_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(tree_add_scene_obj, tree_add_scene);
static mp_obj_t tree_remove_scene(mp_obj_t self_in, mp_obj_t name_in) {
    tree_ref(self_in).RemoveScene(mp_obj_str_get_str(name_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(tree_remove_scene_obj, tree_remove_scene);

static const struct { const char* name; const void* obj; } s_TreeMethods[] = {
    {"is_valid", &tree_is_valid_obj},
    {"get_root", &tree_get_root_obj},
    {"get_current_scene", &tree_get_current_scene_obj},
    {"get_current_scene_path", &tree_get_current_scene_path_obj},
    {"change_scene", &tree_change_scene_obj},
    {"reload_scene", &tree_reload_scene_obj},
    {"add_scene", &tree_add_scene_obj},
    {"remove_scene", &tree_remove_scene_obj},
    {nullptr, nullptr},
};

static void tree_attr(mp_obj_t self_in, qstr attr, mp_obj_t* dest) {
    const char* name = qstr_str(attr);
    if (name[0] == '_' && name[1] == '_') return;
    if (dest[0] == MP_OBJ_NULL) {
        for (int i = 0; s_TreeMethods[i].name != nullptr; ++i) {
            if (std::strcmp(name, s_TreeMethods[i].name) == 0) {
                dest[0] = MP_OBJ_FROM_PTR(s_TreeMethods[i].obj);
                dest[1] = self_in;
                return;
            }
        }
    }
}

MP_DEFINE_CONST_OBJ_TYPE(
    lupine_tree_type,
    MP_QSTR_object,
    MP_TYPE_FLAG_NONE,
    attr, tree_attr
);

// --- Tween handle ----------------------------------------------------------

extern const mp_obj_type_t lupine_tween_type;

typedef struct _lupine_tween_obj_t {
    mp_obj_base_t base;
    uint32_t slot;
    uint32_t generation;
} lupine_tween_obj_t;

static mp_obj_t wrap_tween_shared(const std::shared_ptr<core::Component>& tween, core::SceneManager* tree) {
    if (!tween) return mp_const_none;
    uint32_t generation = 0;
    const uint32_t slot = AcquireComponentSlot(tween, tree, generation);
    lupine_tween_obj_t* o = mp_obj_malloc(lupine_tween_obj_t, &lupine_tween_type);
    o->slot = slot;
    o->generation = generation;
    return MP_OBJ_FROM_PTR(o);
}

static mp_obj_t wrap_tween_ref(const TweenRef& ref) {
    return wrap_tween_shared(ref.Lock(), ref.GetTree());
}

static mp_obj_t wrap_tween(core::Component* tween, ScriptAPI* api) {
    return wrap_tween_shared(SharedComponent(tween), ApiTree(api));
}

// A tween with auto-remove on removes itself from its owner the frame it finishes,
// so a script-held handle routinely outlives the component. Resolving through the
// pooled weak reference rather than a stored raw pointer is what makes the
// scheduler predicate behind await_tween safe on a finished tween.
static TweenRef tween_ref(mp_obj_t self_in) {
    lupine_tween_obj_t* o = static_cast<lupine_tween_obj_t*>(MP_OBJ_TO_PTR(self_in));
    if (!o || o->slot >= s_ComponentSlots.size()) return TweenRef();
    ComponentHandleSlot& s = s_ComponentSlots[o->slot];
    if (!s.inUse || s.generation != o->generation) return TweenRef();
    return TweenRef(s.object, s.tree);
}

static mp_obj_t tween_is_valid(mp_obj_t self_in) { return mp_obj_new_bool(tween_ref(self_in).IsValid()); }
STATIC MP_DEFINE_CONST_FUN_OBJ_1(tween_is_valid_obj, tween_is_valid);
static mp_obj_t tween_get_name(mp_obj_t self_in) {
    std::string s = tween_ref(self_in).GetName();
    return mp_obj_new_str(s.c_str(), s.size());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(tween_get_name_obj, tween_get_name);
static mp_obj_t tween_play(mp_obj_t self_in) { tween_ref(self_in).Play(); return mp_const_none; }
STATIC MP_DEFINE_CONST_FUN_OBJ_1(tween_play_obj, tween_play);
static mp_obj_t tween_pause(mp_obj_t self_in) { tween_ref(self_in).Pause(); return mp_const_none; }
STATIC MP_DEFINE_CONST_FUN_OBJ_1(tween_pause_obj, tween_pause);
static mp_obj_t tween_stop(mp_obj_t self_in) { tween_ref(self_in).Stop(); return mp_const_none; }
STATIC MP_DEFINE_CONST_FUN_OBJ_1(tween_stop_obj, tween_stop);
static mp_obj_t tween_restart(mp_obj_t self_in) { tween_ref(self_in).Restart(); return mp_const_none; }
STATIC MP_DEFINE_CONST_FUN_OBJ_1(tween_restart_obj, tween_restart);
static mp_obj_t tween_kill(mp_obj_t self_in) { tween_ref(self_in).Kill(); return mp_const_none; }
STATIC MP_DEFINE_CONST_FUN_OBJ_1(tween_kill_obj, tween_kill);
static mp_obj_t tween_is_running(mp_obj_t self_in) { return mp_obj_new_bool(tween_ref(self_in).IsRunning()); }
STATIC MP_DEFINE_CONST_FUN_OBJ_1(tween_is_running_obj, tween_is_running);
static mp_obj_t tween_is_finished(mp_obj_t self_in) { return mp_obj_new_bool(tween_ref(self_in).IsFinished()); }
STATIC MP_DEFINE_CONST_FUN_OBJ_1(tween_is_finished_obj, tween_is_finished);
static mp_obj_t tween_get_progress(mp_obj_t self_in) { return mp_obj_new_float(tween_ref(self_in).GetProgress()); }
STATIC MP_DEFINE_CONST_FUN_OBJ_1(tween_get_progress_obj, tween_get_progress);
static mp_obj_t tween_get_duration(mp_obj_t self_in) { return mp_obj_new_float(tween_ref(self_in).GetDuration()); }
STATIC MP_DEFINE_CONST_FUN_OBJ_1(tween_get_duration_obj, tween_get_duration);
static mp_obj_t tween_set_duration(mp_obj_t self_in, mp_obj_t v) {
    tween_ref(self_in).SetDuration(mp_obj_get_float(v));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(tween_set_duration_obj, tween_set_duration);
static mp_obj_t tween_get_easing(mp_obj_t self_in) {
    std::string s = tween_ref(self_in).GetEasing();
    return mp_obj_new_str(s.c_str(), s.size());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(tween_get_easing_obj, tween_get_easing);
static mp_obj_t tween_set_easing(mp_obj_t self_in, mp_obj_t v) {
    tween_ref(self_in).SetEasing(mp_obj_str_get_str(v));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(tween_set_easing_obj, tween_set_easing);
static mp_obj_t tween_get_loop(mp_obj_t self_in) { return mp_obj_new_bool(tween_ref(self_in).GetLoop()); }
STATIC MP_DEFINE_CONST_FUN_OBJ_1(tween_get_loop_obj, tween_get_loop);
static mp_obj_t tween_set_loop(mp_obj_t self_in, mp_obj_t v) {
    tween_ref(self_in).SetLoop(mp_obj_is_true(v));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(tween_set_loop_obj, tween_set_loop);
static mp_obj_t tween_set_auto_remove(mp_obj_t self_in, mp_obj_t v) {
    tween_ref(self_in).SetAutoRemove(mp_obj_is_true(v));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(tween_set_auto_remove_obj, tween_set_auto_remove);
static mp_obj_t tween_get_owner(mp_obj_t self_in) {
    return wrap_node_ref(tween_ref(self_in).GetOwner());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(tween_get_owner_obj, tween_get_owner);
static mp_obj_t tween_as_component(mp_obj_t self_in) {
    return wrap_component_ref(tween_ref(self_in).AsComponent());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(tween_as_component_obj, tween_as_component);

static const struct { const char* name; const void* obj; } s_TweenMethods[] = {
    {"is_valid", &tween_is_valid_obj},
    {"get_name", &tween_get_name_obj},
    {"play", &tween_play_obj},
    {"pause", &tween_pause_obj},
    {"stop", &tween_stop_obj},
    {"restart", &tween_restart_obj},
    {"kill", &tween_kill_obj},
    {"is_running", &tween_is_running_obj},
    {"is_finished", &tween_is_finished_obj},
    {"get_progress", &tween_get_progress_obj},
    {"get_duration", &tween_get_duration_obj},
    {"set_duration", &tween_set_duration_obj},
    {"get_easing", &tween_get_easing_obj},
    {"set_easing", &tween_set_easing_obj},
    {"get_loop", &tween_get_loop_obj},
    {"set_loop", &tween_set_loop_obj},
    {"set_auto_remove", &tween_set_auto_remove_obj},
    {"get_owner", &tween_get_owner_obj},
    {"as_component", &tween_as_component_obj},
    {nullptr, nullptr},
};

static void tween_attr(mp_obj_t self_in, qstr attr, mp_obj_t* dest) {
    const char* name = qstr_str(attr);
    if (name[0] == '_' && name[1] == '_') return;
    if (dest[0] == MP_OBJ_NULL) {
        for (int i = 0; s_TweenMethods[i].name != nullptr; ++i) {
            if (std::strcmp(name, s_TweenMethods[i].name) == 0) {
                dest[0] = MP_OBJ_FROM_PTR(s_TweenMethods[i].obj);
                dest[1] = self_in;
                return;
            }
        }
    }
}

MP_DEFINE_CONST_OBJ_TYPE(
    lupine_tween_type,
    MP_QSTR_object,
    MP_TYPE_FLAG_NONE,
    attr, tween_attr
);

// --- Signal awaiter handle (pooled) ----------------------------------------
//
// SignalAwaiter objects live in a C++-side pool (ordinary heap, not GC memory).
// The Python handle is a small POD holding only a slot index + generation, so it
// never owns a C++ destructor - which matters because this embed build runs
// finalisers OFF. Awaiters are reused across awaits, so there is no per-await
// leak. Fired awaiters are reclaimed once per frame by SweepAwaiterPool(); the
// scheduler's predicate treats a reclaimed/invalid awaiter as "done", so
// reclamation can never strand a waiting coroutine. cancel() reclaims immediately.
// A generation counter invalidates stale handles that point at a reused slot.

extern const mp_obj_type_t lupine_awaiter_type;

typedef struct _lupine_awaiter_obj_t {
    mp_obj_base_t base;
    uint32_t slot;
    uint32_t generation;
} lupine_awaiter_obj_t;

struct AwaiterSlot {
    SignalAwaiter awaiter;
    uint32_t generation = 1;
    bool inUse = false;
};

// The MicroPython VM is created once and lives for the whole process (no host
// exposes a Shutdown), so this pool is only released by static destruction at exit.
static std::vector<AwaiterSlot> s_AwaiterPool;
static std::vector<uint32_t> s_AwaiterFreeSlots;

static void ReclaimAwaiterSlot(uint32_t slot) {
    if (slot >= s_AwaiterPool.size()) return;
    AwaiterSlot& s = s_AwaiterPool[slot];
    if (!s.inUse) return;
    s.awaiter.Cancel();
    s.awaiter = SignalAwaiter();
    s.inUse = false;
    s.generation = (s.generation == 0xFFFFFFFFu) ? 1u : (s.generation + 1u);
    s_AwaiterFreeSlots.push_back(slot);
}

static AwaiterSlot* AwaiterFromHandle(mp_obj_t self_in) {
    lupine_awaiter_obj_t* o = static_cast<lupine_awaiter_obj_t*>(MP_OBJ_TO_PTR(self_in));
    if (!o || o->slot >= s_AwaiterPool.size()) return nullptr;
    AwaiterSlot& s = s_AwaiterPool[o->slot];
    if (!s.inUse || s.generation != o->generation) return nullptr;
    return &s;
}

static mp_obj_t wrap_awaiter(const SignalAwaiter& awaiter) {
    if (!awaiter.IsValid()) return mp_const_none;
    uint32_t slot;
    if (!s_AwaiterFreeSlots.empty()) {
        slot = s_AwaiterFreeSlots.back();
        s_AwaiterFreeSlots.pop_back();
    } else {
        slot = static_cast<uint32_t>(s_AwaiterPool.size());
        s_AwaiterPool.emplace_back();
    }
    AwaiterSlot& s = s_AwaiterPool[slot];
    s.awaiter = awaiter;
    s.inUse = true;
    lupine_awaiter_obj_t* o = mp_obj_malloc(lupine_awaiter_obj_t, &lupine_awaiter_type);
    o->slot = slot;
    o->generation = s.generation;
    return MP_OBJ_FROM_PTR(o);
}

// Reclaim finished awaiters. Called once per frame from Pump() after the pump has
// run, so any coroutine waiting on a now-fired signal has already resumed.
// An awaiter whose source object was destroyed can never fire, so it is finished
// too: reclaiming it flips the handle to invalid, which the scheduler predicate
// ((not aw.is_valid()) or aw.is_fired()) reads as done. Without this, a coroutine
// awaiting a signal from an object that dies before emitting is stranded and
// pumped for the rest of the process, and its pool slot is never released.
static void SweepAwaiterPool() {
    for (uint32_t i = 0; i < s_AwaiterPool.size(); ++i) {
        if (!s_AwaiterPool[i].inUse) {
            continue;
        }
        if (s_AwaiterPool[i].awaiter.IsFired() || !s_AwaiterPool[i].awaiter.IsSourceAlive()) {
            ReclaimAwaiterSlot(i);
        }
    }
}

static mp_obj_t awaiter_is_valid(mp_obj_t self_in) {
    AwaiterSlot* s = AwaiterFromHandle(self_in);
    return mp_obj_new_bool(s && s->awaiter.IsValid());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(awaiter_is_valid_obj, awaiter_is_valid);
static mp_obj_t awaiter_is_fired(mp_obj_t self_in) {
    AwaiterSlot* s = AwaiterFromHandle(self_in);
    return mp_obj_new_bool(s && s->awaiter.IsFired());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(awaiter_is_fired_obj, awaiter_is_fired);
static mp_obj_t awaiter_reset(mp_obj_t self_in) {
    AwaiterSlot* s = AwaiterFromHandle(self_in);
    if (s) s->awaiter.Reset();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(awaiter_reset_obj, awaiter_reset);
static mp_obj_t awaiter_cancel(mp_obj_t self_in) {
    lupine_awaiter_obj_t* o = static_cast<lupine_awaiter_obj_t*>(MP_OBJ_TO_PTR(self_in));
    if (AwaiterFromHandle(self_in)) {
        ReclaimAwaiterSlot(o->slot);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(awaiter_cancel_obj, awaiter_cancel);

static const struct { const char* name; const void* obj; } s_AwaiterMethods[] = {
    {"is_valid", &awaiter_is_valid_obj},
    {"is_fired", &awaiter_is_fired_obj},
    {"reset", &awaiter_reset_obj},
    {"cancel", &awaiter_cancel_obj},
    {nullptr, nullptr},
};

static void awaiter_attr(mp_obj_t self_in, qstr attr, mp_obj_t* dest) {
    const char* name = qstr_str(attr);
    if (name[0] == '_' && name[1] == '_') return;
    if (dest[0] == MP_OBJ_NULL) {
        for (int i = 0; s_AwaiterMethods[i].name != nullptr; ++i) {
            if (std::strcmp(name, s_AwaiterMethods[i].name) == 0) {
                dest[0] = MP_OBJ_FROM_PTR(s_AwaiterMethods[i].obj);
                dest[1] = self_in;
                return;
            }
        }
    }
}

MP_DEFINE_CONST_OBJ_TYPE(
    lupine_awaiter_type,
    MP_QSTR_object,
    MP_TYPE_FLAG_NONE,
    attr, awaiter_attr
);

// --- Tween sequence handle (chainable builder) -----------------------------

extern const mp_obj_type_t lupine_sequence_type;

typedef struct _lupine_sequence_obj_t {
    mp_obj_base_t base;
    uint32_t slot;
    uint32_t generation;
} lupine_sequence_obj_t;

static mp_obj_t wrap_sequence_shared(const std::shared_ptr<core::Component>& sequence, core::SceneManager* tree) {
    if (!sequence) return mp_const_none;
    uint32_t generation = 0;
    const uint32_t slot = AcquireComponentSlot(sequence, tree, generation);
    lupine_sequence_obj_t* o = mp_obj_malloc(lupine_sequence_obj_t, &lupine_sequence_type);
    o->slot = slot;
    o->generation = generation;
    return MP_OBJ_FROM_PTR(o);
}

static mp_obj_t wrap_sequence_ref(const SequenceRef& ref) {
    return wrap_sequence_shared(ref.Lock(), ref.GetTree());
}

static mp_obj_t wrap_sequence(core::Component* sequence, ScriptAPI* api) {
    return wrap_sequence_shared(SharedComponent(sequence), ApiTree(api));
}

static SequenceRef sequence_ref(mp_obj_t self_in) {
    lupine_sequence_obj_t* o = static_cast<lupine_sequence_obj_t*>(MP_OBJ_TO_PTR(self_in));
    if (!o || o->slot >= s_ComponentSlots.size()) return SequenceRef();
    ComponentHandleSlot& s = s_ComponentSlots[o->slot];
    if (!s.inUse || s.generation != o->generation) return SequenceRef();
    return SequenceRef(s.object, s.tree);
}

static mp_obj_t seq_is_valid(mp_obj_t self_in) { return mp_obj_new_bool(sequence_ref(self_in).IsValid()); }
STATIC MP_DEFINE_CONST_FUN_OBJ_1(seq_is_valid_obj, seq_is_valid);
static mp_obj_t seq_get_name(mp_obj_t self_in) {
    std::string s = sequence_ref(self_in).GetName();
    return mp_obj_new_str(s.c_str(), s.size());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(seq_get_name_obj, seq_get_name);
static mp_obj_t seq_append(size_t n_args, const mp_obj_t* args) {
    const char* channel = mp_obj_str_get_str(args[1]);
    nlohmann::json to = MpToJson(args[2]);
    float duration = mp_obj_get_float(args[3]);
    const char* easing = (n_args > 4) ? mp_obj_str_get_str(args[4]) : "linear";
    bool parallel = (n_args > 5) ? mp_obj_is_true(args[5]) : false;
    sequence_ref(args[0]).Append(channel, to, duration, easing, parallel);
    return args[0];
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(seq_append_obj, 4, 6, seq_append);
static mp_obj_t seq_append_on(size_t n_args, const mp_obj_t* args) {
    NodeRef target;
    node_arg(args[1], target);
    const char* channel = mp_obj_str_get_str(args[2]);
    nlohmann::json to = MpToJson(args[3]);
    float duration = mp_obj_get_float(args[4]);
    const char* easing = (n_args > 5) ? mp_obj_str_get_str(args[5]) : "linear";
    bool parallel = (n_args > 6) ? mp_obj_is_true(args[6]) : false;
    sequence_ref(args[0]).AppendOn(target, channel, to, duration, easing, parallel);
    return args[0];
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(seq_append_on_obj, 5, 7, seq_append_on);
static mp_obj_t seq_append_interval(size_t n_args, const mp_obj_t* args) {
    float duration = mp_obj_get_float(args[1]);
    bool parallel = (n_args > 2) ? mp_obj_is_true(args[2]) : false;
    sequence_ref(args[0]).AppendInterval(duration, parallel);
    return args[0];
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(seq_append_interval_obj, 2, 3, seq_append_interval);
static mp_obj_t seq_append_callback(size_t n_args, const mp_obj_t* args) {
    const char* method = mp_obj_str_get_str(args[1]);
    bool parallel = (n_args > 2) ? mp_obj_is_true(args[2]) : false;
    sequence_ref(args[0]).AppendCallback(method, parallel);
    return args[0];
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(seq_append_callback_obj, 2, 3, seq_append_callback);
static mp_obj_t seq_append_callback_on(size_t n_args, const mp_obj_t* args) {
    NodeRef target;
    node_arg(args[1], target);
    const char* method = mp_obj_str_get_str(args[2]);
    bool parallel = (n_args > 3) ? mp_obj_is_true(args[3]) : false;
    sequence_ref(args[0]).AppendCallbackOn(target, method, parallel);
    return args[0];
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(seq_append_callback_on_obj, 3, 4, seq_append_callback_on);
static mp_obj_t seq_play(mp_obj_t self_in) { sequence_ref(self_in).Play(); return self_in; }
STATIC MP_DEFINE_CONST_FUN_OBJ_1(seq_play_obj, seq_play);
static mp_obj_t seq_stop(mp_obj_t self_in) { sequence_ref(self_in).Stop(); return mp_const_none; }
STATIC MP_DEFINE_CONST_FUN_OBJ_1(seq_stop_obj, seq_stop);
static mp_obj_t seq_reset(mp_obj_t self_in) { sequence_ref(self_in).Reset(); return mp_const_none; }
STATIC MP_DEFINE_CONST_FUN_OBJ_1(seq_reset_obj, seq_reset);
static mp_obj_t seq_restart(mp_obj_t self_in) { sequence_ref(self_in).Restart(); return mp_const_none; }
STATIC MP_DEFINE_CONST_FUN_OBJ_1(seq_restart_obj, seq_restart);
static mp_obj_t seq_kill(mp_obj_t self_in) { sequence_ref(self_in).Kill(); return mp_const_none; }
STATIC MP_DEFINE_CONST_FUN_OBJ_1(seq_kill_obj, seq_kill);
static mp_obj_t seq_is_running(mp_obj_t self_in) { return mp_obj_new_bool(sequence_ref(self_in).IsRunning()); }
STATIC MP_DEFINE_CONST_FUN_OBJ_1(seq_is_running_obj, seq_is_running);
static mp_obj_t seq_is_finished(mp_obj_t self_in) { return mp_obj_new_bool(sequence_ref(self_in).IsFinished()); }
STATIC MP_DEFINE_CONST_FUN_OBJ_1(seq_is_finished_obj, seq_is_finished);
static mp_obj_t seq_set_loops(mp_obj_t self_in, mp_obj_t v) {
    sequence_ref(self_in).SetLoops(mp_obj_get_int(v));
    return self_in;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(seq_set_loops_obj, seq_set_loops);
static mp_obj_t seq_get_loops(mp_obj_t self_in) { return mp_obj_new_int(sequence_ref(self_in).GetLoops()); }
STATIC MP_DEFINE_CONST_FUN_OBJ_1(seq_get_loops_obj, seq_get_loops);
static mp_obj_t seq_set_auto_remove(mp_obj_t self_in, mp_obj_t v) {
    sequence_ref(self_in).SetAutoRemove(mp_obj_is_true(v));
    return self_in;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(seq_set_auto_remove_obj, seq_set_auto_remove);
static mp_obj_t seq_get_step_count(mp_obj_t self_in) { return mp_obj_new_int(sequence_ref(self_in).GetStepCount()); }
STATIC MP_DEFINE_CONST_FUN_OBJ_1(seq_get_step_count_obj, seq_get_step_count);
static mp_obj_t seq_get_owner(mp_obj_t self_in) {
    return wrap_node_ref(sequence_ref(self_in).GetOwner());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(seq_get_owner_obj, seq_get_owner);
static mp_obj_t seq_as_component(mp_obj_t self_in) {
    return wrap_component_ref(sequence_ref(self_in).AsComponent());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(seq_as_component_obj, seq_as_component);

static const struct { const char* name; const void* obj; } s_SequenceMethods[] = {
    {"is_valid", &seq_is_valid_obj},
    {"get_name", &seq_get_name_obj},
    {"append", (const void*)&seq_append_obj},
    {"append_on", (const void*)&seq_append_on_obj},
    {"append_interval", (const void*)&seq_append_interval_obj},
    {"append_callback", (const void*)&seq_append_callback_obj},
    {"append_callback_on", (const void*)&seq_append_callback_on_obj},
    {"play", &seq_play_obj},
    {"stop", &seq_stop_obj},
    {"reset", &seq_reset_obj},
    {"restart", &seq_restart_obj},
    {"kill", &seq_kill_obj},
    {"is_running", &seq_is_running_obj},
    {"is_finished", &seq_is_finished_obj},
    {"set_loops", &seq_set_loops_obj},
    {"get_loops", &seq_get_loops_obj},
    {"set_auto_remove", &seq_set_auto_remove_obj},
    {"get_step_count", &seq_get_step_count_obj},
    {"get_owner", &seq_get_owner_obj},
    {"as_component", &seq_as_component_obj},
    {nullptr, nullptr},
};

static void sequence_attr(mp_obj_t self_in, qstr attr, mp_obj_t* dest) {
    const char* name = qstr_str(attr);
    if (name[0] == '_' && name[1] == '_') return;
    if (dest[0] == MP_OBJ_NULL) {
        for (int i = 0; s_SequenceMethods[i].name != nullptr; ++i) {
            if (std::strcmp(name, s_SequenceMethods[i].name) == 0) {
                dest[0] = MP_OBJ_FROM_PTR(s_SequenceMethods[i].obj);
                dest[1] = self_in;
                return;
            }
        }
    }
}

MP_DEFINE_CONST_OBJ_TYPE(
    lupine_sequence_type,
    MP_QSTR_object,
    MP_TYPE_FLAG_NONE,
    attr, sequence_attr
);

// ============================================================================
// NATIVE VALUE TYPES (Vector2 / Vector3 / Color)
//
// Pure-math script value objects backed by lupine::math types. They store their
// components inline (no engine/scene state) so they are safe for the GC, which
// does not run finalisers in this build. Methods are dispatched by the attr
// handler (matching the node/component idiom) because qstrs are created at
// runtime and a compile-time locals_dict would require generated qstr entries.
// ============================================================================

extern const mp_obj_type_t lupine_vector2_type;
extern const mp_obj_type_t lupine_vector3_type;
extern const mp_obj_type_t lupine_color_type;

typedef struct _lupine_vector2_obj_t {
    mp_obj_base_t base;
    float x, y;
} lupine_vector2_obj_t;

typedef struct _lupine_vector3_obj_t {
    mp_obj_base_t base;
    float x, y, z;
} lupine_vector3_obj_t;

typedef struct _lupine_color_obj_t {
    mp_obj_base_t base;
    float r, g, b, a;
} lupine_color_obj_t;

static bool mp_is_number(mp_obj_t o) {
    return mp_obj_is_int(o) || mp_obj_is_float(o);
}

static mp_obj_t make_vector2(float x, float y) {
    lupine_vector2_obj_t* o = mp_obj_malloc(lupine_vector2_obj_t, &lupine_vector2_type);
    o->x = x; o->y = y;
    return MP_OBJ_FROM_PTR(o);
}

static mp_obj_t make_vector3(float x, float y, float z) {
    lupine_vector3_obj_t* o = mp_obj_malloc(lupine_vector3_obj_t, &lupine_vector3_type);
    o->x = x; o->y = y; o->z = z;
    return MP_OBJ_FROM_PTR(o);
}

static mp_obj_t color_obj_new(float r, float g, float b, float a) {
    lupine_color_obj_t* o = mp_obj_malloc(lupine_color_obj_t, &lupine_color_type);
    o->r = r; o->g = g; o->b = b; o->a = a;
    return MP_OBJ_FROM_PTR(o);
}

static math::Vec2 vector2_val(mp_obj_t o) {
    lupine_vector2_obj_t* p = static_cast<lupine_vector2_obj_t*>(MP_OBJ_TO_PTR(o));
    return math::Vec2(p->x, p->y);
}

static math::Vec3 vector3_val(mp_obj_t o) {
    lupine_vector3_obj_t* p = static_cast<lupine_vector3_obj_t*>(MP_OBJ_TO_PTR(o));
    return math::Vec3(p->x, p->y, p->z);
}

static math::Color color_val(mp_obj_t o) {
    lupine_color_obj_t* p = static_cast<lupine_color_obj_t*>(MP_OBJ_TO_PTR(o));
    return math::Color(p->r, p->g, p->b, p->a);
}

// --- Vector2 ----------------------------------------------------------------

static mp_obj_t vector2_make_new(const mp_obj_type_t* type, size_t n_args, size_t n_kw, const mp_obj_t* args) {
    (void)type;
    mp_arg_check_num(n_args, n_kw, 0, 2, false);
    if (n_args == 0) return make_vector2(0.0f, 0.0f);
    if (n_args == 1) {
        float s = mp_obj_get_float(args[0]);
        return make_vector2(s, s);
    }
    return make_vector2(mp_obj_get_float(args[0]), mp_obj_get_float(args[1]));
}

static void vector2_print(const mp_print_t* print, mp_obj_t self_in, mp_print_kind_t kind) {
    (void)kind;
    lupine_vector2_obj_t* o = static_cast<lupine_vector2_obj_t*>(MP_OBJ_TO_PTR(self_in));
    char buf[64];
    std::snprintf(buf, sizeof(buf), "Vector2(%g, %g)", (double)o->x, (double)o->y);
    mp_print_str(print, buf);
}

static mp_obj_t vector2_unary_op(mp_unary_op_t op, mp_obj_t self_in) {
    lupine_vector2_obj_t* o = static_cast<lupine_vector2_obj_t*>(MP_OBJ_TO_PTR(self_in));
    switch (op) {
        case MP_UNARY_OP_NEGATIVE: return make_vector2(-o->x, -o->y);
        default: return MP_OBJ_NULL;
    }
}

static mp_obj_t vector2_binary_op(mp_binary_op_t op, mp_obj_t lhs_in, mp_obj_t rhs_in) {
    math::Vec2 a = vector2_val(lhs_in);
    bool rhs_is_v2 = mp_obj_is_type(rhs_in, &lupine_vector2_type);
    if (op == MP_BINARY_OP_EQUAL) {
        if (!rhs_is_v2) return mp_const_false;
        return mp_obj_new_bool(a == vector2_val(rhs_in));
    }
    if (rhs_is_v2) {
        math::Vec2 b = vector2_val(rhs_in);
        switch (op) {
            case MP_BINARY_OP_ADD: case MP_BINARY_OP_INPLACE_ADD: return make_vector2(a.x + b.x, a.y + b.y);
            case MP_BINARY_OP_SUBTRACT: case MP_BINARY_OP_INPLACE_SUBTRACT: return make_vector2(a.x - b.x, a.y - b.y);
            case MP_BINARY_OP_MULTIPLY: case MP_BINARY_OP_INPLACE_MULTIPLY: return make_vector2(a.x * b.x, a.y * b.y);
            case MP_BINARY_OP_TRUE_DIVIDE: case MP_BINARY_OP_INPLACE_TRUE_DIVIDE: return make_vector2(a.x / b.x, a.y / b.y);
            default: return MP_OBJ_NULL;
        }
    }
    if (mp_is_number(rhs_in)) {
        float s = mp_obj_get_float(rhs_in);
        switch (op) {
            case MP_BINARY_OP_MULTIPLY: case MP_BINARY_OP_INPLACE_MULTIPLY: return make_vector2(a.x * s, a.y * s);
            case MP_BINARY_OP_TRUE_DIVIDE: case MP_BINARY_OP_INPLACE_TRUE_DIVIDE: return make_vector2(a.x / s, a.y / s);
            default: return MP_OBJ_NULL;
        }
    }
    return MP_OBJ_NULL;
}

static mp_obj_t vector2_length(mp_obj_t self_in) {
    return mp_obj_new_float(vector2_val(self_in).Length());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(vector2_length_obj, vector2_length);

static mp_obj_t vector2_length_squared(mp_obj_t self_in) {
    return mp_obj_new_float(vector2_val(self_in).LengthSquared());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(vector2_length_squared_obj, vector2_length_squared);

static mp_obj_t vector2_normalized(mp_obj_t self_in) {
    math::Vec2 n = vector2_val(self_in).Normalized();
    return make_vector2(n.x, n.y);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(vector2_normalized_obj, vector2_normalized);

static mp_obj_t vector2_dot(mp_obj_t self_in, mp_obj_t other_in) {
    if (!mp_obj_is_type(other_in, &lupine_vector2_type)) {
        mp_raise_TypeError(MP_ERROR_TEXT("expected Vector2"));
    }
    return mp_obj_new_float(vector2_val(self_in).Dot(vector2_val(other_in)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(vector2_dot_obj, vector2_dot);

static mp_obj_t vector2_distance_to(mp_obj_t self_in, mp_obj_t other_in) {
    if (!mp_obj_is_type(other_in, &lupine_vector2_type)) {
        mp_raise_TypeError(MP_ERROR_TEXT("expected Vector2"));
    }
    return mp_obj_new_float(vector2_val(self_in).Distance(vector2_val(other_in)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(vector2_distance_to_obj, vector2_distance_to);

static mp_obj_t vector2_lerp(mp_obj_t self_in, mp_obj_t other_in, mp_obj_t t_in) {
    if (!mp_obj_is_type(other_in, &lupine_vector2_type)) {
        mp_raise_TypeError(MP_ERROR_TEXT("expected Vector2"));
    }
    math::Vec2 r = vector2_val(self_in).Lerp(vector2_val(other_in), mp_obj_get_float(t_in));
    return make_vector2(r.x, r.y);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(vector2_lerp_obj, vector2_lerp);

static mp_obj_t vector2_angle(mp_obj_t self_in) {
    return mp_obj_new_float(vector2_val(self_in).Angle());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(vector2_angle_obj, vector2_angle);

static const struct { const char* name; const void* obj; } s_Vector2Methods[] = {
    {"length", &vector2_length_obj},
    {"length_squared", &vector2_length_squared_obj},
    {"normalized", &vector2_normalized_obj},
    {"dot", &vector2_dot_obj},
    {"distance_to", &vector2_distance_to_obj},
    {"lerp", &vector2_lerp_obj},
    {"angle", &vector2_angle_obj},
    {nullptr, nullptr},
};

static void vector2_attr(mp_obj_t self_in, qstr attr, mp_obj_t* dest) {
    const char* name = qstr_str(attr);
    if (name[0] == '_' && name[1] == '_') return;
    lupine_vector2_obj_t* o = static_cast<lupine_vector2_obj_t*>(MP_OBJ_TO_PTR(self_in));
    if (dest[0] == MP_OBJ_NULL) {
        if (std::strcmp(name, "x") == 0) { dest[0] = mp_obj_new_float(o->x); return; }
        if (std::strcmp(name, "y") == 0) { dest[0] = mp_obj_new_float(o->y); return; }
        for (int i = 0; s_Vector2Methods[i].name != nullptr; ++i) {
            if (std::strcmp(name, s_Vector2Methods[i].name) == 0) {
                dest[0] = MP_OBJ_FROM_PTR(s_Vector2Methods[i].obj);
                dest[1] = self_in;
                return;
            }
        }
        return;
    }
    if (dest[0] == MP_OBJ_SENTINEL && dest[1] != MP_OBJ_NULL) {
        if (std::strcmp(name, "x") == 0) { o->x = mp_obj_get_float(dest[1]); dest[0] = MP_OBJ_NULL; return; }
        if (std::strcmp(name, "y") == 0) { o->y = mp_obj_get_float(dest[1]); dest[0] = MP_OBJ_NULL; return; }
    }
}

MP_DEFINE_CONST_OBJ_TYPE(
    lupine_vector2_type,
    MP_QSTR_object,
    MP_TYPE_FLAG_NONE,
    make_new, vector2_make_new,
    print, vector2_print,
    unary_op, vector2_unary_op,
    binary_op, vector2_binary_op,
    attr, vector2_attr
);

// --- Vector3 ----------------------------------------------------------------

static mp_obj_t vector3_make_new(const mp_obj_type_t* type, size_t n_args, size_t n_kw, const mp_obj_t* args) {
    (void)type;
    mp_arg_check_num(n_args, n_kw, 0, 3, false);
    if (n_args == 0) return make_vector3(0.0f, 0.0f, 0.0f);
    if (n_args == 1) {
        float s = mp_obj_get_float(args[0]);
        return make_vector3(s, s, s);
    }
    if (n_args == 2) {
        return make_vector3(mp_obj_get_float(args[0]), mp_obj_get_float(args[1]), 0.0f);
    }
    return make_vector3(mp_obj_get_float(args[0]), mp_obj_get_float(args[1]), mp_obj_get_float(args[2]));
}

static void vector3_print(const mp_print_t* print, mp_obj_t self_in, mp_print_kind_t kind) {
    (void)kind;
    lupine_vector3_obj_t* o = static_cast<lupine_vector3_obj_t*>(MP_OBJ_TO_PTR(self_in));
    char buf[96];
    std::snprintf(buf, sizeof(buf), "Vector3(%g, %g, %g)", (double)o->x, (double)o->y, (double)o->z);
    mp_print_str(print, buf);
}

static mp_obj_t vector3_unary_op(mp_unary_op_t op, mp_obj_t self_in) {
    lupine_vector3_obj_t* o = static_cast<lupine_vector3_obj_t*>(MP_OBJ_TO_PTR(self_in));
    switch (op) {
        case MP_UNARY_OP_NEGATIVE: return make_vector3(-o->x, -o->y, -o->z);
        default: return MP_OBJ_NULL;
    }
}

static mp_obj_t vector3_binary_op(mp_binary_op_t op, mp_obj_t lhs_in, mp_obj_t rhs_in) {
    math::Vec3 a = vector3_val(lhs_in);
    bool rhs_is_v3 = mp_obj_is_type(rhs_in, &lupine_vector3_type);
    if (op == MP_BINARY_OP_EQUAL) {
        if (!rhs_is_v3) return mp_const_false;
        return mp_obj_new_bool(a == vector3_val(rhs_in));
    }
    if (rhs_is_v3) {
        math::Vec3 b = vector3_val(rhs_in);
        switch (op) {
            case MP_BINARY_OP_ADD: case MP_BINARY_OP_INPLACE_ADD: return make_vector3(a.x + b.x, a.y + b.y, a.z + b.z);
            case MP_BINARY_OP_SUBTRACT: case MP_BINARY_OP_INPLACE_SUBTRACT: return make_vector3(a.x - b.x, a.y - b.y, a.z - b.z);
            case MP_BINARY_OP_MULTIPLY: case MP_BINARY_OP_INPLACE_MULTIPLY: return make_vector3(a.x * b.x, a.y * b.y, a.z * b.z);
            case MP_BINARY_OP_TRUE_DIVIDE: case MP_BINARY_OP_INPLACE_TRUE_DIVIDE: return make_vector3(a.x / b.x, a.y / b.y, a.z / b.z);
            default: return MP_OBJ_NULL;
        }
    }
    if (mp_is_number(rhs_in)) {
        float s = mp_obj_get_float(rhs_in);
        switch (op) {
            case MP_BINARY_OP_MULTIPLY: case MP_BINARY_OP_INPLACE_MULTIPLY: return make_vector3(a.x * s, a.y * s, a.z * s);
            case MP_BINARY_OP_TRUE_DIVIDE: case MP_BINARY_OP_INPLACE_TRUE_DIVIDE: return make_vector3(a.x / s, a.y / s, a.z / s);
            default: return MP_OBJ_NULL;
        }
    }
    return MP_OBJ_NULL;
}

static mp_obj_t vector3_length(mp_obj_t self_in) {
    return mp_obj_new_float(vector3_val(self_in).Length());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(vector3_length_obj, vector3_length);

static mp_obj_t vector3_length_squared(mp_obj_t self_in) {
    return mp_obj_new_float(vector3_val(self_in).LengthSquared());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(vector3_length_squared_obj, vector3_length_squared);

static mp_obj_t vector3_normalized(mp_obj_t self_in) {
    math::Vec3 n = vector3_val(self_in).Normalized();
    return make_vector3(n.x, n.y, n.z);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(vector3_normalized_obj, vector3_normalized);

static mp_obj_t vector3_dot(mp_obj_t self_in, mp_obj_t other_in) {
    if (!mp_obj_is_type(other_in, &lupine_vector3_type)) {
        mp_raise_TypeError(MP_ERROR_TEXT("expected Vector3"));
    }
    return mp_obj_new_float(vector3_val(self_in).Dot(vector3_val(other_in)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(vector3_dot_obj, vector3_dot);

static mp_obj_t vector3_cross(mp_obj_t self_in, mp_obj_t other_in) {
    if (!mp_obj_is_type(other_in, &lupine_vector3_type)) {
        mp_raise_TypeError(MP_ERROR_TEXT("expected Vector3"));
    }
    math::Vec3 r = vector3_val(self_in).Cross(vector3_val(other_in));
    return make_vector3(r.x, r.y, r.z);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(vector3_cross_obj, vector3_cross);

static mp_obj_t vector3_distance_to(mp_obj_t self_in, mp_obj_t other_in) {
    if (!mp_obj_is_type(other_in, &lupine_vector3_type)) {
        mp_raise_TypeError(MP_ERROR_TEXT("expected Vector3"));
    }
    return mp_obj_new_float(vector3_val(self_in).Distance(vector3_val(other_in)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(vector3_distance_to_obj, vector3_distance_to);

static mp_obj_t vector3_lerp(mp_obj_t self_in, mp_obj_t other_in, mp_obj_t t_in) {
    if (!mp_obj_is_type(other_in, &lupine_vector3_type)) {
        mp_raise_TypeError(MP_ERROR_TEXT("expected Vector3"));
    }
    math::Vec3 r = vector3_val(self_in).Lerp(vector3_val(other_in), mp_obj_get_float(t_in));
    return make_vector3(r.x, r.y, r.z);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(vector3_lerp_obj, vector3_lerp);

static const struct { const char* name; const void* obj; } s_Vector3Methods[] = {
    {"length", &vector3_length_obj},
    {"length_squared", &vector3_length_squared_obj},
    {"normalized", &vector3_normalized_obj},
    {"dot", &vector3_dot_obj},
    {"cross", &vector3_cross_obj},
    {"distance_to", &vector3_distance_to_obj},
    {"lerp", &vector3_lerp_obj},
    {nullptr, nullptr},
};

static void vector3_attr(mp_obj_t self_in, qstr attr, mp_obj_t* dest) {
    const char* name = qstr_str(attr);
    if (name[0] == '_' && name[1] == '_') return;
    lupine_vector3_obj_t* o = static_cast<lupine_vector3_obj_t*>(MP_OBJ_TO_PTR(self_in));
    if (dest[0] == MP_OBJ_NULL) {
        if (std::strcmp(name, "x") == 0) { dest[0] = mp_obj_new_float(o->x); return; }
        if (std::strcmp(name, "y") == 0) { dest[0] = mp_obj_new_float(o->y); return; }
        if (std::strcmp(name, "z") == 0) { dest[0] = mp_obj_new_float(o->z); return; }
        for (int i = 0; s_Vector3Methods[i].name != nullptr; ++i) {
            if (std::strcmp(name, s_Vector3Methods[i].name) == 0) {
                dest[0] = MP_OBJ_FROM_PTR(s_Vector3Methods[i].obj);
                dest[1] = self_in;
                return;
            }
        }
        return;
    }
    if (dest[0] == MP_OBJ_SENTINEL && dest[1] != MP_OBJ_NULL) {
        if (std::strcmp(name, "x") == 0) { o->x = mp_obj_get_float(dest[1]); dest[0] = MP_OBJ_NULL; return; }
        if (std::strcmp(name, "y") == 0) { o->y = mp_obj_get_float(dest[1]); dest[0] = MP_OBJ_NULL; return; }
        if (std::strcmp(name, "z") == 0) { o->z = mp_obj_get_float(dest[1]); dest[0] = MP_OBJ_NULL; return; }
    }
}

MP_DEFINE_CONST_OBJ_TYPE(
    lupine_vector3_type,
    MP_QSTR_object,
    MP_TYPE_FLAG_NONE,
    make_new, vector3_make_new,
    print, vector3_print,
    unary_op, vector3_unary_op,
    binary_op, vector3_binary_op,
    attr, vector3_attr
);

// --- Color ------------------------------------------------------------------

static mp_obj_t color_make_new(const mp_obj_type_t* type, size_t n_args, size_t n_kw, const mp_obj_t* args) {
    (void)type;
    mp_arg_check_num(n_args, n_kw, 0, 4, false);
    if (n_args == 0) return color_obj_new(0.0f, 0.0f, 0.0f, 1.0f);
    if (n_args == 1) {
        float gray = mp_obj_get_float(args[0]);
        return color_obj_new(gray, gray, gray, 1.0f);
    }
    if (n_args == 2) {
        float gray = mp_obj_get_float(args[0]);
        return color_obj_new(gray, gray, gray, mp_obj_get_float(args[1]));
    }
    if (n_args == 3) {
        return color_obj_new(mp_obj_get_float(args[0]), mp_obj_get_float(args[1]), mp_obj_get_float(args[2]), 1.0f);
    }
    return color_obj_new(mp_obj_get_float(args[0]), mp_obj_get_float(args[1]), mp_obj_get_float(args[2]), mp_obj_get_float(args[3]));
}

static void color_print(const mp_print_t* print, mp_obj_t self_in, mp_print_kind_t kind) {
    (void)kind;
    lupine_color_obj_t* o = static_cast<lupine_color_obj_t*>(MP_OBJ_TO_PTR(self_in));
    char buf[128];
    std::snprintf(buf, sizeof(buf), "Color(%g, %g, %g, %g)", (double)o->r, (double)o->g, (double)o->b, (double)o->a);
    mp_print_str(print, buf);
}

static mp_obj_t color_binary_op(mp_binary_op_t op, mp_obj_t lhs_in, mp_obj_t rhs_in) {
    math::Color a = color_val(lhs_in);
    bool rhs_is_color = mp_obj_is_type(rhs_in, &lupine_color_type);
    if (op == MP_BINARY_OP_EQUAL) {
        if (!rhs_is_color) return mp_const_false;
        return mp_obj_new_bool(a == color_val(rhs_in));
    }
    if (rhs_is_color) {
        math::Color b = color_val(rhs_in);
        switch (op) {
            case MP_BINARY_OP_ADD: case MP_BINARY_OP_INPLACE_ADD: return color_obj_new(a.r + b.r, a.g + b.g, a.b + b.b, a.a + b.a);
            case MP_BINARY_OP_SUBTRACT: case MP_BINARY_OP_INPLACE_SUBTRACT: return color_obj_new(a.r - b.r, a.g - b.g, a.b - b.b, a.a - b.a);
            case MP_BINARY_OP_MULTIPLY: case MP_BINARY_OP_INPLACE_MULTIPLY: return color_obj_new(a.r * b.r, a.g * b.g, a.b * b.b, a.a * b.a);
            default: return MP_OBJ_NULL;
        }
    }
    if (mp_is_number(rhs_in)) {
        float s = mp_obj_get_float(rhs_in);
        switch (op) {
            case MP_BINARY_OP_MULTIPLY: case MP_BINARY_OP_INPLACE_MULTIPLY: return color_obj_new(a.r * s, a.g * s, a.b * s, a.a * s);
            default: return MP_OBJ_NULL;
        }
    }
    return MP_OBJ_NULL;
}

static mp_obj_t color_lerp(mp_obj_t self_in, mp_obj_t other_in, mp_obj_t t_in) {
    if (!mp_obj_is_type(other_in, &lupine_color_type)) {
        mp_raise_TypeError(MP_ERROR_TEXT("expected Color"));
    }
    math::Color r = color_val(self_in).Lerp(color_val(other_in), mp_obj_get_float(t_in));
    return color_obj_new(r.r, r.g, r.b, r.a);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(color_lerp_obj, color_lerp);

static mp_obj_t color_with_alpha(mp_obj_t self_in, mp_obj_t a_in) {
    math::Color c = color_val(self_in);
    return color_obj_new(c.r, c.g, c.b, mp_obj_get_float(a_in));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(color_with_alpha_obj, color_with_alpha);

static mp_obj_t color_to_hex(mp_obj_t self_in) {
    math::Color c = color_val(self_in);
    uint32_t rgba = c.ToRGBA32();
    char buf[10];
    std::snprintf(buf, sizeof(buf), "#%08X", (unsigned int)rgba);
    return mp_obj_new_str(buf, std::strlen(buf));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(color_to_hex_obj, color_to_hex);

static const struct { const char* name; const void* obj; } s_ColorMethods[] = {
    {"lerp", &color_lerp_obj},
    {"with_alpha", &color_with_alpha_obj},
    {"to_hex", &color_to_hex_obj},
    {nullptr, nullptr},
};

static void color_attr(mp_obj_t self_in, qstr attr, mp_obj_t* dest) {
    const char* name = qstr_str(attr);
    if (name[0] == '_' && name[1] == '_') return;
    lupine_color_obj_t* o = static_cast<lupine_color_obj_t*>(MP_OBJ_TO_PTR(self_in));
    if (dest[0] == MP_OBJ_NULL) {
        if (std::strcmp(name, "r") == 0) { dest[0] = mp_obj_new_float(o->r); return; }
        if (std::strcmp(name, "g") == 0) { dest[0] = mp_obj_new_float(o->g); return; }
        if (std::strcmp(name, "b") == 0) { dest[0] = mp_obj_new_float(o->b); return; }
        if (std::strcmp(name, "a") == 0) { dest[0] = mp_obj_new_float(o->a); return; }
        for (int i = 0; s_ColorMethods[i].name != nullptr; ++i) {
            if (std::strcmp(name, s_ColorMethods[i].name) == 0) {
                dest[0] = MP_OBJ_FROM_PTR(s_ColorMethods[i].obj);
                dest[1] = self_in;
                return;
            }
        }
        return;
    }
    if (dest[0] == MP_OBJ_SENTINEL && dest[1] != MP_OBJ_NULL) {
        if (std::strcmp(name, "r") == 0) { o->r = mp_obj_get_float(dest[1]); dest[0] = MP_OBJ_NULL; return; }
        if (std::strcmp(name, "g") == 0) { o->g = mp_obj_get_float(dest[1]); dest[0] = MP_OBJ_NULL; return; }
        if (std::strcmp(name, "b") == 0) { o->b = mp_obj_get_float(dest[1]); dest[0] = MP_OBJ_NULL; return; }
        if (std::strcmp(name, "a") == 0) { o->a = mp_obj_get_float(dest[1]); dest[0] = MP_OBJ_NULL; return; }
    }
}

MP_DEFINE_CONST_OBJ_TYPE(
    lupine_color_type,
    MP_QSTR_object,
    MP_TYPE_FLAG_NONE,
    make_new, color_make_new,
    print, color_print,
    binary_op, color_binary_op,
    attr, color_attr
);

// --- Lupine module: root / scene / tree / instancing / timers --------------

static core::Node* parent_arg(size_t n_args, const mp_obj_t* args, size_t index) {
    if (n_args > index && mp_obj_is_type(args[index], &lupine_node_type)) {
        return node_raw(args[index]);
    }
    return nullptr;
}

static mp_obj_t lupine_get_root(void) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_const_none;
    return wrap_node(api->GetRoot(), api);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_root_obj, lupine_get_root);
static mp_obj_t lupine_get_scene(void) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_const_none;
    return wrap_scene(api->GetScene(), api);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_scene_obj, lupine_get_scene);
static mp_obj_t lupine_get_tree(void) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_const_none;
    return wrap_tree(api->GetTree(), api);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_tree_obj, lupine_get_tree);

static mp_obj_t lupine_instantiate_prefab(size_t n_args, const mp_obj_t* args) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_const_none;
    core::Node* parent = parent_arg(n_args, args, 1);
    return wrap_node(api->InstantiatePrefab(mp_obj_str_get_str(args[0]), parent), api);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_instantiate_prefab_obj, 1, 2, lupine_instantiate_prefab);
static mp_obj_t lupine_instantiate_scene(size_t n_args, const mp_obj_t* args) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_const_none;
    core::Node* parent = parent_arg(n_args, args, 1);
    return wrap_node(api->InstantiateScene(mp_obj_str_get_str(args[0]), parent), api);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_instantiate_scene_obj, 1, 2, lupine_instantiate_scene);
static mp_obj_t lupine_create_node(size_t n_args, const mp_obj_t* args) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_const_none;
    const char* type = mp_obj_str_get_str(args[0]);
    core::Node* node = (n_args > 1) ? api->CreateNode(type, mp_obj_str_get_str(args[1])) : api->CreateNode(type);
    return wrap_node(node, api);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_create_node_obj, 1, 2, lupine_create_node);
static mp_obj_t lupine_create_node_child(size_t n_args, const mp_obj_t* args) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_const_none;
    core::Node* parent = parent_arg(n_args, args, 2);
    return wrap_node(api->CreateNodeChild(mp_obj_str_get_str(args[0]), mp_obj_str_get_str(args[1]), parent), api);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_create_node_child_obj, 2, 3, lupine_create_node_child);
static mp_obj_t lupine_duplicate_node(mp_obj_t node_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_const_none;
    core::Node* raw = nullptr;
    if (mp_obj_is_type(node_in, &lupine_node_type)) {
        raw = node_raw(node_in);
    }
    return wrap_node(api->DuplicateNode(raw), api);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_duplicate_node_obj, lupine_duplicate_node);

static mp_obj_t lupine_create_timer(size_t n_args, const mp_obj_t* args) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_const_none;
    float delay = mp_obj_get_float(args[0]);
    const char* callback = (n_args > 1) ? mp_obj_str_get_str(args[1]) : "";
    core::Component* timer = api->CreateTimerComponent(delay, callback, false, -1, "");
    return wrap_timer(timer, api);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_create_timer_obj, 1, 2, lupine_create_timer);
static mp_obj_t lupine_create_repeating_timer(size_t n_args, const mp_obj_t* args) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_const_none;
    float interval = mp_obj_get_float(args[0]);
    const char* callback = (n_args > 1) ? mp_obj_str_get_str(args[1]) : "";
    int repeatCount = (n_args > 2) ? mp_obj_get_int(args[2]) : -1;
    core::Component* timer = api->CreateTimerComponent(interval, callback, true, repeatCount, "");
    return wrap_timer(timer, api);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_create_repeating_timer_obj, 1, 3, lupine_create_repeating_timer);
static mp_obj_t lupine_create_named_timer(size_t n_args, const mp_obj_t* args) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_const_none;
    const char* name = mp_obj_str_get_str(args[0]);
    float delay = mp_obj_get_float(args[1]);
    const char* callback = (n_args > 2) ? mp_obj_str_get_str(args[2]) : "";
    bool repeating = (n_args > 3) ? mp_obj_is_true(args[3]) : false;
    int repeatCount = (n_args > 4) ? mp_obj_get_int(args[4]) : -1;
    core::Component* timer = api->CreateTimerComponent(delay, callback, repeating, repeatCount, name);
    return wrap_timer(timer, api);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_create_named_timer_obj, 2, 5, lupine_create_named_timer);
static mp_obj_t lupine_list_timers(void) {
    ScriptAPI* api = GetCurrentScriptAPI();
    mp_obj_t list = mp_obj_new_list(0, nullptr);
    if (!api) return list;
    for (core::Component* timer : api->ListTimers()) {
        mp_obj_list_append(list, wrap_timer(timer, api));
    }
    return list;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_list_timers_obj, lupine_list_timers);

static mp_obj_t lupine_create_tween(size_t n_args, const mp_obj_t* args) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_const_none;
    const char* channel = mp_obj_str_get_str(args[0]);
    nlohmann::json to = MpToJson(args[1]);
    float duration = mp_obj_get_float(args[2]);
    const char* easing = (n_args > 3) ? mp_obj_str_get_str(args[3]) : "linear";
    core::Component* tween = api->CreateTweenComponent(channel, to, duration, easing, nullptr);
    return wrap_tween(tween, api);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_create_tween_obj, 3, 4, lupine_create_tween);

static mp_obj_t lupine_list_tweens(void) {
    ScriptAPI* api = GetCurrentScriptAPI();
    mp_obj_t list = mp_obj_new_list(0, nullptr);
    if (!api) return list;
    for (core::Component* tween : api->ListTweens()) {
        mp_obj_list_append(list, wrap_tween(tween, api));
    }
    return list;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_list_tweens_obj, lupine_list_tweens);

static mp_obj_t lupine_create_sequence(void) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_const_none;
    SequenceRef ref = NodeRef::FromRaw(api->GetSelf(), api).CreateSequence();
    return wrap_sequence(ref.Lock().get(), api);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_create_sequence_obj, lupine_create_sequence);

// ============================================================================
// Sandboxed file I/O + JSON (res:// user:// temp:// only; the C++ API validates
// the sandbox and rejects anything else or ".." traversal).
// ============================================================================

static mp_obj_t lupine_read_text(mp_obj_t path_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_const_none;
    std::string out;
    if (!api->ReadTextFile(mp_obj_str_get_str(path_in), out)) return mp_const_none;
    return mp_obj_new_str(out.c_str(), out.size());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_read_text_obj, lupine_read_text);

static mp_obj_t lupine_write_text(mp_obj_t path_in, mp_obj_t text_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_obj_new_bool(false);
    size_t len = 0;
    const char* data = mp_obj_str_get_data(text_in, &len);
    return mp_obj_new_bool(api->WriteTextFile(mp_obj_str_get_str(path_in), std::string(data, len)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_write_text_obj, lupine_write_text);

static mp_obj_t lupine_append_text(mp_obj_t path_in, mp_obj_t text_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_obj_new_bool(false);
    size_t len = 0;
    const char* data = mp_obj_str_get_data(text_in, &len);
    return mp_obj_new_bool(api->AppendTextFile(mp_obj_str_get_str(path_in), std::string(data, len)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_append_text_obj, lupine_append_text);

static mp_obj_t lupine_read_bytes(mp_obj_t path_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_const_none;
    std::vector<uint8_t> data;
    if (!api->ReadBytesFile(mp_obj_str_get_str(path_in), data)) return mp_const_none;
    return mp_obj_new_bytes(data.data(), data.size());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_read_bytes_obj, lupine_read_bytes);

static mp_obj_t lupine_write_bytes(mp_obj_t path_in, mp_obj_t data_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_obj_new_bool(false);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(data_in, &bufinfo, MP_BUFFER_READ);
    const uint8_t* p = static_cast<const uint8_t*>(bufinfo.buf);
    std::vector<uint8_t> bytes(p, p + bufinfo.len);
    return mp_obj_new_bool(api->WriteBytesFile(mp_obj_str_get_str(path_in), bytes));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_write_bytes_obj, lupine_write_bytes);

static mp_obj_t lupine_file_exists(mp_obj_t path_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    return mp_obj_new_bool(api && api->FileExists(mp_obj_str_get_str(path_in)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_file_exists_obj, lupine_file_exists);

static mp_obj_t lupine_is_file(mp_obj_t path_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    return mp_obj_new_bool(api && api->FileIsFile(mp_obj_str_get_str(path_in)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_is_file_obj, lupine_is_file);

static mp_obj_t lupine_is_dir(mp_obj_t path_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    return mp_obj_new_bool(api && api->FileIsDirectory(mp_obj_str_get_str(path_in)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_is_dir_obj, lupine_is_dir);

static mp_obj_t lupine_remove_file(mp_obj_t path_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    return mp_obj_new_bool(api && api->DeleteFilePath(mp_obj_str_get_str(path_in)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_remove_file_obj, lupine_remove_file);

static mp_obj_t lupine_make_dir(mp_obj_t path_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    return mp_obj_new_bool(api && api->MakeDirectory(mp_obj_str_get_str(path_in)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_make_dir_obj, lupine_make_dir);

static mp_obj_t lupine_list_dir(mp_obj_t path_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    mp_obj_t list = mp_obj_new_list(0, nullptr);
    if (!api) return list;
    for (const std::string& name : api->ListDirectory(mp_obj_str_get_str(path_in))) {
        mp_obj_list_append(list, mp_obj_new_str(name.c_str(), name.size()));
    }
    return list;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_list_dir_obj, lupine_list_dir);

static mp_obj_t lupine_file_size(mp_obj_t path_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_obj_new_int(-1);
    return mp_obj_new_int_from_ll(api->GetFileSize(mp_obj_str_get_str(path_in)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_file_size_obj, lupine_file_size);

static mp_obj_t lupine_to_json(size_t n_args, const mp_obj_t* args) {
    nlohmann::json j = MpToJson(args[0]);
    bool pretty = (n_args > 1) && mp_obj_is_true(args[1]);
    std::string s = pretty ? j.dump(2) : j.dump();
    return mp_obj_new_str(s.c_str(), s.size());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_to_json_obj, 1, 2, lupine_to_json);

static mp_obj_t lupine_from_json(mp_obj_t str_in) {
    size_t len = 0;
    const char* data = mp_obj_str_get_data(str_in, &len);
    try {
        return JsonToMp(nlohmann::json::parse(std::string(data, len)));
    } catch (...) {
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_from_json_obj, lupine_from_json);

static mp_obj_t lupine_read_json(mp_obj_t path_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_const_none;
    std::string text;
    if (!api->ReadTextFile(mp_obj_str_get_str(path_in), text)) return mp_const_none;
    try {
        return JsonToMp(nlohmann::json::parse(text));
    } catch (...) {
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_read_json_obj, lupine_read_json);

static mp_obj_t lupine_write_json(size_t n_args, const mp_obj_t* args) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_obj_new_bool(false);
    nlohmann::json j = MpToJson(args[1]);
    bool pretty = (n_args > 2) && mp_obj_is_true(args[2]);
    std::string s = pretty ? j.dump(2) : j.dump();
    return mp_obj_new_bool(api->WriteTextFile(mp_obj_str_get_str(args[0]), s));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_write_json_obj, 2, 3, lupine_write_json);

static mp_obj_t lupine_save_game(size_t n_args, const mp_obj_t* args) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_obj_new_bool(false);
    nlohmann::json metaJson = (n_args > 2) ? MpToJson(args[2]) : nlohmann::json::object();
    return mp_obj_new_bool(api->SaveGame(mp_obj_str_get_str(args[0]), MpToJson(args[1]), metaJson));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_save_game_obj, 2, 3, lupine_save_game);

static mp_obj_t lupine_load_game(mp_obj_t slot_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_const_none;
    nlohmann::json result = api->LoadGame(mp_obj_str_get_str(slot_in));
    if (result.is_null()) return mp_const_none;
    return JsonToMp(result);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_load_game_obj, lupine_load_game);

static mp_obj_t lupine_save_slot_exists(mp_obj_t slot_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    return mp_obj_new_bool(api && api->SaveSlotExists(mp_obj_str_get_str(slot_in)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_save_slot_exists_obj, lupine_save_slot_exists);

static mp_obj_t lupine_delete_save_slot(mp_obj_t slot_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    return mp_obj_new_bool(api && api->DeleteSaveSlot(mp_obj_str_get_str(slot_in)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_delete_save_slot_obj, lupine_delete_save_slot);

static mp_obj_t lupine_copy_save_slot(mp_obj_t from_in, mp_obj_t to_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    return mp_obj_new_bool(api && api->CopySaveSlot(mp_obj_str_get_str(from_in), mp_obj_str_get_str(to_in), true));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_copy_save_slot_obj, lupine_copy_save_slot);

static mp_obj_t lupine_rename_save_slot(mp_obj_t from_in, mp_obj_t to_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    return mp_obj_new_bool(api && api->RenameSaveSlot(mp_obj_str_get_str(from_in), mp_obj_str_get_str(to_in), true));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_rename_save_slot_obj, lupine_rename_save_slot);

static mp_obj_t lupine_list_save_slots(void) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_obj_new_list(0, nullptr);
    return JsonToMp(api->ListSaveSlots());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_list_save_slots_obj, lupine_list_save_slots);

static mp_obj_t lupine_list_save_slot_infos(void) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_obj_new_list(0, nullptr);
    return JsonToMp(api->ListSaveSlotInfos());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_list_save_slot_infos_obj, lupine_list_save_slot_infos);

static mp_obj_t lupine_get_save_slot_info(mp_obj_t slot_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_const_none;
    nlohmann::json result = api->GetSaveSlotInfo(mp_obj_str_get_str(slot_in));
    if (result.is_null()) return mp_const_none;
    return JsonToMp(result);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_get_save_slot_info_obj, lupine_get_save_slot_info);

static mp_obj_t lupine_quick_save(size_t n_args, const mp_obj_t* args) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_obj_new_bool(false);
    nlohmann::json metaJson = (n_args > 1) ? MpToJson(args[1]) : nlohmann::json::object();
    return mp_obj_new_bool(api->QuickSaveGame(MpToJson(args[0]), metaJson));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_quick_save_obj, 1, 2, lupine_quick_save);

static mp_obj_t lupine_quick_load(void) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_const_none;
    nlohmann::json result = api->QuickLoadGame();
    if (result.is_null()) return mp_const_none;
    return JsonToMp(result);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_quick_load_obj, lupine_quick_load);

static mp_obj_t lupine_has_quick_save(void) {
    ScriptAPI* api = GetCurrentScriptAPI();
    return mp_obj_new_bool(api && api->HasQuickSave());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_has_quick_save_obj, lupine_has_quick_save);

static mp_obj_t lupine_auto_save(size_t n_args, const mp_obj_t* args) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_obj_new_bool(false);
    nlohmann::json metaJson = (n_args > 1) ? MpToJson(args[1]) : nlohmann::json::object();
    return mp_obj_new_bool(api->AutoSaveGame(MpToJson(args[0]), metaJson));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_auto_save_obj, 1, 2, lupine_auto_save);

static mp_obj_t lupine_has_auto_save(void) {
    ScriptAPI* api = GetCurrentScriptAPI();
    return mp_obj_new_bool(api && api->HasAutoSave());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_has_auto_save_obj, lupine_has_auto_save);

static mp_obj_t lupine_get_last_save_error(void) {
    ScriptAPI* api = GetCurrentScriptAPI();
    std::string s = api ? api->GetLastSaveError() : std::string("Success");
    return mp_obj_new_str(s.c_str(), s.size());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_last_save_error_obj, lupine_get_last_save_error);

static mp_obj_t lupine_set_save_directory(mp_obj_t dir_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) api->SetSaveDirectory(mp_obj_str_get_str(dir_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_save_directory_obj, lupine_set_save_directory);

static mp_obj_t lupine_set_save_format(mp_obj_t fmt_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) api->SetSaveFormat(mp_obj_str_get_str(fmt_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_save_format_obj, lupine_set_save_format);

static mp_obj_t lupine_set_save_schema_version(mp_obj_t v_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) api->SetSaveSchemaVersion(mp_obj_get_int(v_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_save_schema_version_obj, lupine_set_save_schema_version);

static mp_obj_t lupine_get_save_schema_version(void) {
    ScriptAPI* api = GetCurrentScriptAPI();
    return mp_obj_new_int(api ? api->GetSaveSchemaVersion() : 0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_get_save_schema_version_obj, lupine_get_save_schema_version);

static mp_obj_t lupine_set_save_obfuscation_key(mp_obj_t key_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) api->SetSaveObfuscationKey(mp_obj_str_get_str(key_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_save_obfuscation_key_obj, lupine_set_save_obfuscation_key);

static mp_obj_t lupine_set_quick_save_slot(mp_obj_t slot_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) api->SetQuickSaveSlot(mp_obj_str_get_str(slot_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_quick_save_slot_obj, lupine_set_quick_save_slot);

static mp_obj_t lupine_set_auto_save_slot(mp_obj_t slot_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (api) api->SetAutoSaveSlot(mp_obj_str_get_str(slot_in));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_set_auto_save_slot_obj, lupine_set_auto_save_slot);

static mp_obj_t lupine_capture_scene_state(size_t n_args, const mp_obj_t* args) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_obj_new_list(0, nullptr);
    const char* group = (n_args > 0) ? mp_obj_str_get_str(args[0]) : "persistent";
    return JsonToMp(api->CaptureSceneState(group));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_capture_scene_state_obj, 0, 1, lupine_capture_scene_state);

static mp_obj_t lupine_restore_scene_state(mp_obj_t captured_in) {
    ScriptAPI* api = GetCurrentScriptAPI();
    if (!api) return mp_obj_new_int(0);
    return mp_obj_new_int(api->RestoreSceneState(MpToJson(captured_in)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_restore_scene_state_obj, lupine_restore_scene_state);

static mp_obj_t lupine_user_dir(void) { return mp_obj_new_str("user://", 7); }
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_user_dir_obj, lupine_user_dir);
static mp_obj_t lupine_res_dir(void) { return mp_obj_new_str("res://", 6); }
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_res_dir_obj, lupine_res_dir);
static mp_obj_t lupine_temp_dir(void) { return mp_obj_new_str("temp://", 7); }
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_temp_dir_obj, lupine_temp_dir);

static mp_obj_t lupine_join_path(size_t n_args, const mp_obj_t* args) {
    std::string result;
    for (size_t i = 0; i < n_args; ++i) {
        std::string part = mp_obj_str_get_str(args[i]);
        if (part.empty()) continue;
        if (result.empty()) {
            result = part;
        } else if (result.back() == '/') {
            result += (part.front() == '/') ? part.substr(1) : part;
        } else {
            result += (part.front() == '/') ? part : ("/" + part);
        }
    }
    return mp_obj_new_str(result.c_str(), result.size());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR(lupine_join_path_obj, 0, lupine_join_path);

// ============================================================================
// Scheduler support: coroutine <-> owning script instance
//
// The scheduler lives in __main__ and is pumped once per frame from outside any
// script dispatch, so nothing would otherwise make the coroutine's own instance
// active while it is resumed - every self-relative lupine.* call after the first
// yield would resolve against a null ScriptAPI and silently do nothing. Each
// scheduler entry therefore records the id of the instance that started it, and
// the pump brackets the entry with _env_enter/_env_exit. _env_enter also reports
// a destroyed instance (id no longer registered), which is how the pump retires
// the coroutines of a freed script.
// ============================================================================

static mp_obj_t lupine_env_id(void) {
    return mp_obj_new_int(s_CurrentEnv ? s_CurrentEnv->GetEnvId() : -1);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_env_id_obj, lupine_env_id);

static mp_obj_t lupine_env_enter(mp_obj_t id_in) {
    MicroPythonEnvironment* env = FindEnvById(mp_obj_get_int(id_in));
    if (!env) {
        return mp_const_false;
    }
    s_EnvActivationStack.push_back(s_CurrentEnv);
    s_CurrentEnv = env;
    return mp_const_true;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_env_enter_obj, lupine_env_enter);

static mp_obj_t lupine_env_exit(void) {
    if (!s_EnvActivationStack.empty()) {
        s_CurrentEnv = s_EnvActivationStack.back();
        s_EnvActivationStack.pop_back();
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_env_exit_obj, lupine_env_exit);

// ============================================================================
// RUNTIME MODULE REGISTRATION
// ============================================================================

// Structure to hold function name and pointer
struct LupineFunction {
    const char* name;
    const mp_obj_fun_builtin_fixed_t* func;
};

// All lupine functions - registered at runtime using qstr_from_str
static const LupineFunction s_LupineFunctions[] = {
    // Scheduler support (used by the coroutine pump, not by game scripts)
    {"_env_id", &lupine_env_id_obj},
    {"_env_enter", &lupine_env_enter_obj},
    {"_env_exit", &lupine_env_exit_obj},

    // Logging
    {"log_info", &lupine_log_info_obj},
    {"log_warning", &lupine_log_warning_obj},
    {"log_error", &lupine_log_error_obj},
    {"log_debug", &lupine_log_debug_obj},

    // Node object model
    {"get_node", &lupine_get_node_obj},
    {"find_node", &lupine_get_node_obj},
    {"find_node_by_uuid", &lupine_find_node_by_uuid_obj},
    {"get_self", &lupine_get_self_obj},
    {"get_singleton", &lupine_get_singleton_obj},
    {"get_root", &lupine_get_root_obj},
    {"get_scene", &lupine_get_scene_obj},
    {"get_tree", &lupine_get_tree_obj},

    // Runtime instantiation
    {"instantiate_prefab", (const mp_obj_fun_builtin_fixed_t*)&lupine_instantiate_prefab_obj},
    {"instantiate_scene", (const mp_obj_fun_builtin_fixed_t*)&lupine_instantiate_scene_obj},
    {"create_node", (const mp_obj_fun_builtin_fixed_t*)&lupine_create_node_obj},
    {"create_node_child", (const mp_obj_fun_builtin_fixed_t*)&lupine_create_node_child_obj},
    {"duplicate_node", &lupine_duplicate_node_obj},

    // Signals & global event bus
    {"emit", (const mp_obj_fun_builtin_fixed_t*)&lupine_emit_obj},
    {"connect", (const mp_obj_fun_builtin_fixed_t*)&lupine_connect_obj},
    {"disconnect", &lupine_disconnect_obj},
    {"is_connected", &lupine_is_connected_obj},
    {"add_user_signal", &lupine_add_user_signal_obj},
    {"call_deferred", (const mp_obj_fun_builtin_fixed_t*)&lupine_call_deferred_obj},
    {"emit_event", (const mp_obj_fun_builtin_fixed_t*)&lupine_emit_event_obj},
    {"subscribe", (const mp_obj_fun_builtin_fixed_t*)&lupine_subscribe_obj},
    {"unsubscribe", &lupine_unsubscribe_obj},

    // Game State
    {"get_delta_time", &lupine_get_delta_time_obj},
    {"set_game_paused", &lupine_set_game_paused_obj},
    {"is_game_paused", &lupine_is_game_paused_obj},
    {"quit", &lupine_quit_obj},
    {"get_cmdline_args", &lupine_get_cmdline_args_obj},
    {"set_time_scale", &lupine_set_time_scale_obj},
    {"get_time_scale", &lupine_get_time_scale_obj},

    // Input - Actions
    {"is_action_pressed", (const mp_obj_fun_builtin_fixed_t*)&lupine_is_action_pressed_obj},
    {"is_action_just_pressed", (const mp_obj_fun_builtin_fixed_t*)&lupine_is_action_just_pressed_obj},
    {"is_action_just_released", (const mp_obj_fun_builtin_fixed_t*)&lupine_is_action_just_released_obj},
    {"get_action_strength", (const mp_obj_fun_builtin_fixed_t*)&lupine_get_action_strength_obj},
    {"get_axis", (const mp_obj_fun_builtin_fixed_t*)&lupine_get_axis_obj},
    {"get_vector", (const mp_obj_fun_builtin_fixed_t*)&lupine_get_vector_obj},

    // Input - Keyboard
    {"is_key_pressed", &lupine_is_key_pressed_obj},
    {"is_key_just_pressed", &lupine_is_key_just_pressed_obj},
    {"is_key_just_released", &lupine_is_key_just_released_obj},

    // Input - Mouse
    {"is_mouse_button_pressed", &lupine_is_mouse_button_pressed_obj},
    {"is_mouse_button_just_pressed", &lupine_is_mouse_button_just_pressed_obj},
    {"is_mouse_button_just_released", &lupine_is_mouse_button_just_released_obj},
    {"get_mouse_position", &lupine_get_mouse_position_obj},
    {"get_mouse_delta", &lupine_get_mouse_delta_obj},
    {"get_mouse_scroll_delta", &lupine_get_mouse_scroll_delta_obj},

    // Input - Gamepad
    {"is_gamepad_connected", (const mp_obj_fun_builtin_fixed_t*)&lupine_is_gamepad_connected_obj},
    {"get_gamepad_count", &lupine_get_gamepad_count_obj},
    {"get_connected_gamepad_ids", &lupine_get_connected_gamepad_ids_obj},
    {"get_gamepad_name", (const mp_obj_fun_builtin_fixed_t*)&lupine_get_gamepad_name_obj},
    {"is_gamepad_button_pressed", (const mp_obj_fun_builtin_fixed_t*)&lupine_is_gamepad_button_pressed_obj},
    {"is_gamepad_button_just_pressed", (const mp_obj_fun_builtin_fixed_t*)&lupine_is_gamepad_button_just_pressed_obj},
    {"is_gamepad_button_just_released", (const mp_obj_fun_builtin_fixed_t*)&lupine_is_gamepad_button_just_released_obj},
    {"get_gamepad_axis", (const mp_obj_fun_builtin_fixed_t*)&lupine_get_gamepad_axis_obj},
    {"set_gamepad_vibration", (const mp_obj_fun_builtin_fixed_t*)&lupine_set_gamepad_vibration_obj},
    {"stop_gamepad_vibration", &lupine_stop_gamepad_vibration_obj},
    {"set_gamepad_deadzone", &lupine_set_gamepad_deadzone_obj},
    {"get_gamepad_deadzone", &lupine_get_gamepad_deadzone_obj},

    // Input - Touch
    {"is_touch_available", &lupine_is_touch_available_obj},
    {"is_touching", &lupine_is_touching_obj},
    {"get_touch_count", &lupine_get_touch_count_obj},
    {"get_touch_position", (const mp_obj_fun_builtin_fixed_t*)&lupine_get_touch_position_obj},
    {"is_touch_just_started", &lupine_is_touch_just_started_obj},
    {"is_touch_just_ended", &lupine_is_touch_just_ended_obj},

    // Input - Clipboard
    {"get_clipboard", &lupine_get_clipboard_obj},
    {"set_clipboard", &lupine_set_clipboard_obj},

    // Active device detection
    {"get_active_device_type", &lupine_get_active_device_type_obj},
    {"get_last_gamepad_id", &lupine_get_last_gamepad_id_obj},
    {"get_gamepad_type", (const mp_obj_fun_builtin_fixed_t*)&lupine_get_gamepad_type_obj},

    // Input contexts / action sets
    {"enable_input_context", &lupine_enable_input_context_obj},
    {"disable_input_context", &lupine_disable_input_context_obj},
    {"set_input_context_active", &lupine_set_input_context_active_obj},
    {"is_input_context_active", &lupine_is_input_context_active_obj},
    {"set_exclusive_input_context", &lupine_set_exclusive_input_context_obj},
    {"get_active_input_contexts", &lupine_get_active_input_contexts_obj},
    {"set_action_enabled", &lupine_set_action_enabled_obj},
    {"set_axis_enabled", &lupine_set_axis_enabled_obj},

    // Local multiplayer player slots
    {"set_player_count", &lupine_set_player_count_obj},
    {"get_player_count", &lupine_get_player_count_obj},
    {"clear_player_assignments", &lupine_clear_player_assignments_obj},
    {"assign_keyboard_mouse_to_player", &lupine_assign_keyboard_mouse_to_player_obj},
    {"assign_gamepad_to_player", &lupine_assign_gamepad_to_player_obj},
    {"unassign_gamepad", &lupine_unassign_gamepad_obj},
    {"get_player_for_gamepad", &lupine_get_player_for_gamepad_obj},
    {"get_player_for_keyboard_mouse", &lupine_get_player_for_keyboard_mouse_obj},
    {"player_owns_keyboard_mouse", &lupine_player_owns_keyboard_mouse_obj},
    {"get_player_gamepads", &lupine_get_player_gamepads_obj},
    {"set_auto_join_enabled", &lupine_set_auto_join_enabled_obj},
    {"is_auto_join_enabled", &lupine_is_auto_join_enabled_obj},

    // Runtime rebinding
    {"add_action_key", &lupine_add_action_key_obj},
    {"add_action_mouse_button", &lupine_add_action_mouse_button_obj},
    {"add_action_gamepad_button", (const mp_obj_fun_builtin_fixed_t*)&lupine_add_action_gamepad_button_obj},
    {"add_action_gamepad_axis", (const mp_obj_fun_builtin_fixed_t*)&lupine_add_action_gamepad_axis_obj},
    {"remove_action_binding", &lupine_remove_action_binding_obj},
    {"clear_action_bindings", &lupine_clear_action_bindings_obj},
    {"get_action_bindings", &lupine_get_action_bindings_obj},
    {"add_axis_key", (const mp_obj_fun_builtin_fixed_t*)&lupine_add_axis_key_obj},
    {"add_axis_gamepad_axis", (const mp_obj_fun_builtin_fixed_t*)&lupine_add_axis_gamepad_axis_obj},
    {"remove_axis_binding", &lupine_remove_axis_binding_obj},
    {"clear_axis_bindings", &lupine_clear_axis_bindings_obj},
    {"get_axis_bindings", &lupine_get_axis_bindings_obj},
    {"save_input_map", &lupine_save_input_map_obj},
    {"load_input_map", &lupine_load_input_map_obj},

    // Input capture (rebind menus)
    {"start_input_capture", &lupine_start_input_capture_obj},
    {"start_input_capture_mask", &lupine_start_input_capture_mask_obj},
    {"cancel_input_capture", &lupine_cancel_input_capture_obj},
    {"is_capturing_input", &lupine_is_capturing_input_obj},
    {"is_input_capture_complete", &lupine_is_input_capture_complete_obj},
    {"get_captured_binding", &lupine_get_captured_binding_obj},
    {"clear_captured_binding", &lupine_clear_captured_binding_obj},
    {"apply_captured_binding_to_action", &lupine_apply_captured_binding_to_action_obj},

    // Glyph / prompt resolution
    {"get_action_glyph", (const mp_obj_fun_builtin_fixed_t*)&lupine_get_action_glyph_obj},
    {"get_action_glyphs", &lupine_get_action_glyphs_obj},
    {"set_glyph_label", &lupine_set_glyph_label_obj},
    {"set_glyph_art", &lupine_set_glyph_art_obj},
    {"clear_glyph_override", &lupine_clear_glyph_override_obj},
    {"clear_glyph_overrides", &lupine_clear_glyph_overrides_obj},
    {"load_glyph_map", &lupine_load_glyph_map_obj},
    {"save_glyph_map", &lupine_save_glyph_map_obj},

    // Action delegation
    {"connect_action", (const mp_obj_fun_builtin_fixed_t*)&lupine_connect_action_obj},
    {"disconnect_action", &lupine_disconnect_action_obj},
    {"connect_device_changed", (const mp_obj_fun_builtin_fixed_t*)&lupine_connect_device_changed_obj},
    {"connect_input_captured", (const mp_obj_fun_builtin_fixed_t*)&lupine_connect_input_captured_obj},

    // Event-driven action matching (inside on_input_event)
    {"event_is_action", &lupine_event_is_action_obj},
    {"event_is_action_pressed", &lupine_event_is_action_pressed_obj},
    {"event_is_action_released", &lupine_event_is_action_released_obj},

    // Window / Display
    {"set_window_title", &lupine_set_window_title_obj},
    {"get_window_title", &lupine_get_window_title_obj},
    {"set_fullscreen", &lupine_set_fullscreen_obj},
    {"is_fullscreen", &lupine_is_fullscreen_obj},
    {"set_vsync", &lupine_set_vsync_obj},
    {"is_vsync", &lupine_is_vsync_obj},
    {"set_window_size", &lupine_set_window_size_obj},
    {"get_window_size", &lupine_get_window_size_obj},
    {"get_screen_size", &lupine_get_screen_size_obj},
    {"maximize_window", &lupine_maximize_window_obj},
    {"minimize_window", &lupine_minimize_window_obj},
    {"restore_window", &lupine_restore_window_obj},
    {"set_mouse_mode", &lupine_set_mouse_mode_obj},
    {"get_mouse_mode", &lupine_get_mouse_mode_obj},
    {"set_mouse_cursor_visible", &lupine_set_mouse_cursor_visible_obj},
    {"is_mouse_cursor_visible", &lupine_is_mouse_cursor_visible_obj},

    // Screen <-> World Conversion
    {"screen_to_world_2d", &lupine_screen_to_world_2d_obj},
    {"world_to_screen_2d", &lupine_world_to_screen_2d_obj},
    {"screen_to_world_3d", &lupine_screen_to_world_3d_obj},
    {"world_to_screen_3d", &lupine_world_to_screen_3d_obj},
    {"screen_to_world_ray_3d", &lupine_screen_to_world_ray_3d_obj},

    // Debug Draw
    {"debug_draw_line", (const mp_obj_fun_builtin_fixed_t*)&lupine_debug_draw_line_obj},
    {"debug_draw_line_2d", (const mp_obj_fun_builtin_fixed_t*)&lupine_debug_draw_line_2d_obj},
    {"debug_draw_ray", (const mp_obj_fun_builtin_fixed_t*)&lupine_debug_draw_ray_obj},
    {"debug_draw_box", (const mp_obj_fun_builtin_fixed_t*)&lupine_debug_draw_box_obj},
    {"debug_draw_sphere", (const mp_obj_fun_builtin_fixed_t*)&lupine_debug_draw_sphere_obj},
    {"debug_draw_circle", (const mp_obj_fun_builtin_fixed_t*)&lupine_debug_draw_circle_obj},
    {"debug_draw_text", (const mp_obj_fun_builtin_fixed_t*)&lupine_debug_draw_text_obj},
    {"debug_draw_text_2d", (const mp_obj_fun_builtin_fixed_t*)&lupine_debug_draw_text_2d_obj},

    // Custom component rendering (on_draw)
    {"draw_quad", (const mp_obj_fun_builtin_fixed_t*)&lupine_draw_quad_obj},
    {"draw_textured_quad", (const mp_obj_fun_builtin_fixed_t*)&lupine_draw_textured_quad_obj},
    {"draw_rect", (const mp_obj_fun_builtin_fixed_t*)&lupine_draw_rect_obj},
    {"draw_sprite", (const mp_obj_fun_builtin_fixed_t*)&lupine_draw_sprite_obj},
    {"draw_line", (const mp_obj_fun_builtin_fixed_t*)&lupine_draw_line_obj},
    {"draw_circle", (const mp_obj_fun_builtin_fixed_t*)&lupine_draw_circle_obj},
    {"draw_polygon", (const mp_obj_fun_builtin_fixed_t*)&lupine_draw_polygon_obj},
    {"draw_box", (const mp_obj_fun_builtin_fixed_t*)&lupine_draw_box_obj},
    {"draw_rounded_rect", (const mp_obj_fun_builtin_fixed_t*)&lupine_draw_rounded_rect_obj},
    {"is_drawing", &lupine_is_drawing_obj},

    // Editor-only debug draw (no runtime visual)
    {"editor_draw_line", (const mp_obj_fun_builtin_fixed_t*)&lupine_editor_draw_line_obj},
    {"editor_draw_box", (const mp_obj_fun_builtin_fixed_t*)&lupine_editor_draw_box_obj},
    {"editor_draw_sphere", (const mp_obj_fun_builtin_fixed_t*)&lupine_editor_draw_sphere_obj},
    {"editor_draw_circle", (const mp_obj_fun_builtin_fixed_t*)&lupine_editor_draw_circle_obj},
    {"editor_draw_rect_2d", (const mp_obj_fun_builtin_fixed_t*)&lupine_editor_draw_rect_2d_obj},
    {"editor_draw_text", (const mp_obj_fun_builtin_fixed_t*)&lupine_editor_draw_text_obj},
    {"is_editor_draw_available", &lupine_is_editor_draw_available_obj},

    // Transform 2D - Position
    {"get_position_2d", &lupine_get_position_2d_obj},
    {"set_position_2d", &lupine_set_position_2d_obj},
    {"translate_2d", &lupine_translate_2d_obj},
    {"get_global_position_2d", &lupine_get_global_position_2d_obj},
    {"set_global_position_2d", &lupine_set_global_position_2d_obj},

    // Transform 2D - Rotation
    {"get_rotation_2d", &lupine_get_rotation_2d_obj},
    {"set_rotation_2d", &lupine_set_rotation_2d_obj},
    {"rotate_2d", &lupine_rotate_2d_obj},
    {"get_global_rotation_2d", &lupine_get_global_rotation_2d_obj},
    {"set_global_rotation_2d", &lupine_set_global_rotation_2d_obj},

    // Transform 2D - Scale
    {"get_scale_2d", &lupine_get_scale_2d_obj},
    {"set_scale_2d", &lupine_set_scale_2d_obj},
    {"get_global_scale_2d", &lupine_get_global_scale_2d_obj},

    // Transform 3D - Position
    {"get_position_3d", &lupine_get_position_3d_obj},
    {"set_position_3d", &lupine_set_position_3d_obj},
    {"translate_3d", &lupine_translate_3d_obj},
    {"get_global_position_3d", &lupine_get_global_position_3d_obj},
    {"set_global_position_3d", &lupine_set_global_position_3d_obj},

    // Transform 3D - Rotation
    {"get_rotation_3d", &lupine_get_rotation_3d_obj},
    {"set_rotation_3d", &lupine_set_rotation_3d_obj},
    {"rotate_3d", &lupine_rotate_3d_obj},
    {"get_global_rotation_3d", &lupine_get_global_rotation_3d_obj},
    {"set_global_rotation_3d", &lupine_set_global_rotation_3d_obj},

    // Transform 3D - Scale
    {"get_scale_3d", &lupine_get_scale_3d_obj},
    {"set_scale_3d", &lupine_set_scale_3d_obj},
    {"get_global_scale_3d", &lupine_get_global_scale_3d_obj},

    // Transform 3D - Direction
    {"get_forward", &lupine_get_forward_obj},
    {"get_right", &lupine_get_right_obj},
    {"get_up", &lupine_get_up_obj},

    // Look At
    {"look_at_2d", &lupine_look_at_2d_obj},
    {"look_at_3d", &lupine_look_at_3d_obj},

    // Distance
    {"distance_to_2d", &lupine_distance_to_2d_obj},
    {"distance_to_3d", &lupine_distance_to_3d_obj},

    // Move Toward
    {"move_toward_2d", &lupine_move_toward_2d_obj},
    {"move_toward_3d", (const mp_obj_fun_builtin_fixed_t*)&lupine_move_toward_3d_obj},

    // Node Properties
    {"get_name", &lupine_get_name_obj},
    {"set_name", &lupine_set_name_obj},
    {"is_active", &lupine_is_active_obj},
    {"set_active", &lupine_set_active_obj},
    {"is_visible", &lupine_is_visible_obj},
    {"set_visible", &lupine_set_visible_obj},
    {"get_sibling_index", &lupine_get_sibling_index_obj},
    {"set_sibling_index", &lupine_set_sibling_index_obj},

    // Node Hierarchy
    {"get_child_count", &lupine_get_child_count_obj},
    {"has_node", &lupine_has_node_obj},
    {"has_component", &lupine_has_component_obj},

    // Node Lifetime
    {"queue_free", &lupine_queue_free_self_obj},
    {"queue_free_self", &lupine_queue_free_self_obj},
    {"queue_free_deferred", &lupine_queue_free_deferred_self_obj},
    {"queue_free_deferred_self", &lupine_queue_free_deferred_self_obj},
    {"free", &lupine_free_self_obj},
    {"free_self", &lupine_free_self_obj},

    // Scene Management
    {"change_scene", &lupine_change_scene_obj},
    {"reload_scene", &lupine_reload_scene_obj},
    {"add_scene", &lupine_add_scene_obj},
    {"remove_scene", &lupine_remove_scene_obj},
    {"get_current_scene_path", &lupine_get_current_scene_path_obj},

    // Audio
    {"play_audio", (const mp_obj_fun_builtin_fixed_t*)&lupine_play_audio_obj},
    {"play_audio_3d", (const mp_obj_fun_builtin_fixed_t*)&lupine_play_audio_3d_obj},
    {"stop_audio", &lupine_stop_audio_obj},
    {"pause_audio", &lupine_pause_audio_obj},
    {"resume_audio", &lupine_resume_audio_obj},
    {"set_bus_volume", &lupine_set_bus_volume_obj},
    {"get_bus_volume", &lupine_get_bus_volume_obj},
    {"set_bus_muted", &lupine_set_bus_muted_obj},
    {"add_bus_effect", (const mp_obj_fun_builtin_fixed_t*)&lupine_add_bus_effect_obj},
    {"remove_bus_effect", (const mp_obj_fun_builtin_fixed_t*)&lupine_remove_bus_effect_obj},
    {"move_bus_effect", (const mp_obj_fun_builtin_fixed_t*)&lupine_move_bus_effect_obj},
    {"clear_bus_effects", (const mp_obj_fun_builtin_fixed_t*)&lupine_clear_bus_effects_obj},
    {"set_bus_effect_enabled", (const mp_obj_fun_builtin_fixed_t*)&lupine_set_bus_effect_enabled_obj},
    {"set_bus_effect_parameter", (const mp_obj_fun_builtin_fixed_t*)&lupine_set_bus_effect_parameter_obj},
    {"get_bus_effect_count", (const mp_obj_fun_builtin_fixed_t*)&lupine_get_bus_effect_count_obj},
    {"get_bus_effect_parameter", (const mp_obj_fun_builtin_fixed_t*)&lupine_get_bus_effect_parameter_obj},
    {"is_bus_effect_enabled", (const mp_obj_fun_builtin_fixed_t*)&lupine_is_bus_effect_enabled_obj},
    {"get_bus_level", &lupine_get_bus_level_obj},
    {"is_bus_muted", &lupine_is_bus_muted_obj},
    {"set_audio_source_volume", &lupine_set_audio_source_volume_obj},
    {"set_audio_source_pitch", &lupine_set_audio_source_pitch_obj},
    {"set_audio_source_pan", &lupine_set_audio_source_pan_obj},
    {"set_master_volume", &lupine_set_master_volume_obj},
    {"get_master_volume", &lupine_get_master_volume_obj},
    {"set_master_muted", &lupine_set_master_muted_obj},
    {"is_master_muted", &lupine_is_master_muted_obj},
    {"set_listener_position", &lupine_set_listener_position_obj},
    {"set_listener_orientation", (const mp_obj_fun_builtin_fixed_t*)&lupine_set_listener_orientation_obj},
    {"set_listener_velocity", &lupine_set_listener_velocity_obj},
    {"create_audio_bus", (const mp_obj_fun_builtin_fixed_t*)&lupine_create_audio_bus_obj},
    {"destroy_audio_bus", &lupine_destroy_audio_bus_obj},
    {"has_audio_bus", &lupine_has_audio_bus_obj},
    {"set_bus_solo", &lupine_set_bus_solo_obj},
    {"is_bus_solo", &lupine_is_bus_solo_obj},

    {"tr", (const mp_obj_fun_builtin_fixed_t*)&lupine_tr_obj},
    {"tr_fmt", (const mp_obj_fun_builtin_fixed_t*)&lupine_tr_fmt_obj},
    {"tr_plural", (const mp_obj_fun_builtin_fixed_t*)&lupine_tr_plural_obj},
    {"set_locale", &lupine_set_locale_obj},
    {"get_locale", &lupine_get_locale_obj},
    {"get_fallback_locale", &lupine_get_fallback_locale_obj},
    {"get_locales", &lupine_get_locales_obj},
    {"has_loc_key", &lupine_has_loc_key_obj},
    {"reload_localization", &lupine_reload_localization_obj},
    {"set_pseudolocalization", &lupine_set_pseudolocalization_obj},
    {"is_pseudolocalization", &lupine_is_pseudolocalization_obj},

    // Theme
    {"set_theme", &lupine_set_theme_obj},
    {"get_theme_color", &lupine_get_theme_color_obj},
    {"get_theme_constant", &lupine_get_theme_constant_obj},
    {"set_palette_color", (const mp_obj_fun_builtin_fixed_t*)&lupine_set_palette_color_obj},
    {"set_theme_variable", &lupine_set_theme_variable_obj},
    {"get_theme_version", &lupine_get_theme_version_obj},

    {"play_audio_scheduled", (const mp_obj_fun_builtin_fixed_t*)&lupine_play_audio_scheduled_obj},
    {"play_audio_scheduled_3d", (const mp_obj_fun_builtin_fixed_t*)&lupine_play_audio_scheduled_3d_obj},
    {"is_audio_playing", &lupine_is_audio_playing_obj},
    {"is_audio_finished", &lupine_is_audio_finished_obj},

    // Groups
    {"implements_interface", &lupine_implements_interface_obj},
    {"get_implemented_interfaces", &lupine_get_implemented_interfaces_obj},
    {"verify_interface", &lupine_verify_interface_obj},
    {"get_nodes_with_interface", &lupine_get_nodes_with_interface_obj},
    {"get_node_count_with_interface", &lupine_get_node_count_with_interface_obj},
    {"get_first_node_with_interface", &lupine_get_first_node_with_interface_obj},
    {"interface_exists", &lupine_interface_exists_obj},
    {"get_all_interfaces", &lupine_get_all_interfaces_obj},
    {"get_interface_definition", &lupine_get_interface_definition_obj},
    {"register_interface", &lupine_register_interface_obj},
    {"archetype_implements_interface", &lupine_archetype_implements_interface_obj},
    {"get_archetypes_with_interface", &lupine_get_archetypes_with_interface_obj},
    {"add_to_group", &lupine_add_to_group_obj},
    {"remove_from_group", &lupine_remove_from_group_obj},
    {"is_in_group", &lupine_is_in_group_obj},
    {"get_groups", &lupine_get_groups_obj},
    {"get_nodes_in_group", &lupine_get_nodes_in_group_obj},
    {"get_node_count_in_group", &lupine_get_node_count_in_group_obj},

    // Tree Utilities
    {"get_first_node_in_group", &lupine_get_first_node_in_group_obj},
    {"get_node_or_null", &lupine_get_node_or_null_obj},
    {"find_children", (const mp_obj_fun_builtin_fixed_t*)&lupine_find_children_obj},
    {"is_ancestor_of", &lupine_is_ancestor_of_obj},

    // Utility - Time
    {"get_time", &lupine_get_time_obj},
    {"get_frame_count", &lupine_get_frame_count_obj},

    // Profiler
    {"profiler_begin_zone", (const mp_obj_fun_builtin_fixed_t*)&lupine_profiler_begin_zone_obj},
    {"profiler_end_zone", &lupine_profiler_end_zone_obj},
    {"profiler_set_counter", &lupine_profiler_set_counter_obj},
    {"profiler_is_enabled", &lupine_profiler_is_enabled_obj},
    {"profiler_set_enabled", &lupine_profiler_set_enabled_obj},
    {"profiler_frame_ms", &lupine_profiler_frame_ms_obj},

    // Engine / OS Info
    {"get_fps", &lupine_get_fps_obj},
    {"get_ticks_msec", &lupine_get_ticks_msec_obj},
    {"get_unix_time", &lupine_get_unix_time_obj},
    {"get_platform_name", &lupine_get_platform_name_obj},
    {"is_debug_build", &lupine_is_debug_build_obj},
    {"get_dpi_scale", &lupine_get_dpi_scale_obj},
    {"open_url", &lupine_open_url_obj},

    // Color & Data Sampling
    {"color_from_hex", &lupine_color_from_hex_obj},
    {"color_to_hex", (const mp_obj_fun_builtin_fixed_t*)&lupine_color_to_hex_obj},
    {"color_from_hsv", (const mp_obj_fun_builtin_fixed_t*)&lupine_color_from_hsv_obj},
    {"color_lerp", (const mp_obj_fun_builtin_fixed_t*)&lupine_color_lerp_obj},
    {"sample_gradient", &lupine_sample_gradient_obj},
    {"sample_curve", &lupine_sample_curve_obj},

    // Utility - Random
    {"random_range", &lupine_random_range_obj},
    {"random_range_int", &lupine_random_range_int_obj},
    {"random_float", &lupine_random_float_obj},
    {"random_bool", &lupine_random_bool_obj},
    {"random_sign", &lupine_random_sign_obj},
    {"random_seed", &lupine_random_seed_obj},

    // Utility - Math
    {"lerp", &lupine_lerp_obj},
    {"clamp", &lupine_clamp_obj},
    {"abs", &lupine_abs_obj},
    {"sign", &lupine_sign_obj},
    {"move_toward", &lupine_move_toward_obj},
    {"lerp_angle", &lupine_lerp_angle_obj},
    {"angle_difference", &lupine_angle_difference_obj},
    {"smoothstep", &lupine_smoothstep_obj},
    {"inverse_lerp", &lupine_inverse_lerp_obj},
    {"remap", (const mp_obj_fun_builtin_fixed_t*)&lupine_remap_obj},
    {"deg_to_rad", &lupine_deg_to_rad_obj},
    {"rad_to_deg", &lupine_rad_to_deg_obj},
    {"wrap", &lupine_wrap_obj},
    {"wrap_int", &lupine_wrap_int_obj},
    {"ping_pong", &lupine_ping_pong_obj},
    {"snapped", &lupine_snapped_obj},
    {"is_equal_approx", &lupine_is_equal_approx_obj},
    {"ease", &lupine_ease_obj},
    {"pos_mod", &lupine_pos_mod_obj},
    {"pos_mod_int", &lupine_pos_mod_int_obj},

    // Utility - Vector Math
    {"normalize_2d", &lupine_normalize_2d_obj},
    {"normalize_3d", &lupine_normalize_3d_obj},
    {"length_2d", &lupine_length_2d_obj},
    {"length_3d", &lupine_length_3d_obj},
    {"dot_2d", &lupine_dot_2d_obj},
    {"dot_3d", &lupine_dot_3d_obj},
    {"cross", &lupine_cross_obj},

    // Global Variables
    {"get_global_int", (const mp_obj_fun_builtin_fixed_t*)&lupine_get_global_int_obj},
    {"get_global_float", (const mp_obj_fun_builtin_fixed_t*)&lupine_get_global_float_obj},
    {"get_global_string", (const mp_obj_fun_builtin_fixed_t*)&lupine_get_global_string_obj},
    {"get_global_bool", (const mp_obj_fun_builtin_fixed_t*)&lupine_get_global_bool_obj},
    {"set_global_int", &lupine_set_global_int_obj},
    {"set_global_float", &lupine_set_global_float_obj},
    {"set_global_string", &lupine_set_global_string_obj},
    {"set_global_bool", &lupine_set_global_bool_obj},
    {"get_global", (const mp_obj_fun_builtin_fixed_t*)&lupine_get_global_obj},
    {"set_global", &lupine_set_global_obj},

    // Timers
    {"create_timer", (const mp_obj_fun_builtin_fixed_t*)&lupine_create_timer_obj},
    {"create_repeating_timer", (const mp_obj_fun_builtin_fixed_t*)&lupine_create_repeating_timer_obj},
    {"create_named_timer", (const mp_obj_fun_builtin_fixed_t*)&lupine_create_named_timer_obj},
    {"list_timers", &lupine_list_timers_obj},
    {"create_tween", (const mp_obj_fun_builtin_fixed_t*)&lupine_create_tween_obj},
    {"list_tweens", &lupine_list_tweens_obj},
    {"create_sequence", &lupine_create_sequence_obj},

    // Sandboxed file I/O (res:// user:// temp://)
    {"read_text", &lupine_read_text_obj},
    {"write_text", &lupine_write_text_obj},
    {"append_text", &lupine_append_text_obj},
    {"read_bytes", &lupine_read_bytes_obj},
    {"write_bytes", &lupine_write_bytes_obj},
    {"file_exists", &lupine_file_exists_obj},
    {"is_file", &lupine_is_file_obj},
    {"is_dir", &lupine_is_dir_obj},
    {"remove_file", &lupine_remove_file_obj},
    {"delete_file", &lupine_remove_file_obj},
    {"make_dir", &lupine_make_dir_obj},
    {"ensure_dir", &lupine_make_dir_obj},
    {"list_dir", &lupine_list_dir_obj},
    {"file_size", &lupine_file_size_obj},

    // JSON
    {"to_json", (const mp_obj_fun_builtin_fixed_t*)&lupine_to_json_obj},
    {"from_json", &lupine_from_json_obj},
    {"read_json", &lupine_read_json_obj},
    {"write_json", (const mp_obj_fun_builtin_fixed_t*)&lupine_write_json_obj},

    // Save games
    {"save_game", (const mp_obj_fun_builtin_fixed_t*)&lupine_save_game_obj},
    {"load_game", &lupine_load_game_obj},
    {"save_slot_exists", &lupine_save_slot_exists_obj},
    {"delete_save_slot", &lupine_delete_save_slot_obj},
    {"copy_save_slot", &lupine_copy_save_slot_obj},
    {"rename_save_slot", &lupine_rename_save_slot_obj},
    {"list_save_slots", &lupine_list_save_slots_obj},
    {"list_save_slot_infos", &lupine_list_save_slot_infos_obj},
    {"get_save_slot_info", &lupine_get_save_slot_info_obj},
    {"quick_save", (const mp_obj_fun_builtin_fixed_t*)&lupine_quick_save_obj},
    {"quick_load", &lupine_quick_load_obj},
    {"has_quick_save", &lupine_has_quick_save_obj},
    {"auto_save", (const mp_obj_fun_builtin_fixed_t*)&lupine_auto_save_obj},
    {"has_auto_save", &lupine_has_auto_save_obj},
    {"get_last_save_error", &lupine_get_last_save_error_obj},
    {"set_save_directory", &lupine_set_save_directory_obj},
    {"set_save_format", &lupine_set_save_format_obj},
    {"set_save_schema_version", &lupine_set_save_schema_version_obj},
    {"get_save_schema_version", &lupine_get_save_schema_version_obj},
    {"set_save_obfuscation_key", &lupine_set_save_obfuscation_key_obj},
    {"set_quick_save_slot", &lupine_set_quick_save_slot_obj},
    {"set_auto_save_slot", &lupine_set_auto_save_slot_obj},
    {"capture_scene_state", (const mp_obj_fun_builtin_fixed_t*)&lupine_capture_scene_state_obj},
    {"restore_scene_state", &lupine_restore_scene_state_obj},

    // user:// helpers
    {"user_dir", &lupine_user_dir_obj},
    {"res_dir", &lupine_res_dir_obj},
    {"temp_dir", &lupine_temp_dir_obj},
    {"join_path", (const mp_obj_fun_builtin_fixed_t*)&lupine_join_path_obj},

    // Asset Loading
    {"load_image_asset", &lupine_load_image_asset_obj},
    {"load_audio_asset", (const mp_obj_fun_builtin_fixed_t*)&lupine_load_audio_asset_obj},
    {"load_model_asset", &lupine_load_model_asset_obj},
    {"preload_assets", &lupine_preload_assets_obj},

    // Physics - Raycast
    {"raycast_2d", (const mp_obj_fun_builtin_fixed_t*)&lupine_raycast_2d_obj},
    {"raycast_all_2d", (const mp_obj_fun_builtin_fixed_t*)&lupine_raycast_all_2d_obj},
    {"raycast_3d", (const mp_obj_fun_builtin_fixed_t*)&lupine_raycast_3d_obj},
    {"raycast_all_3d", (const mp_obj_fun_builtin_fixed_t*)&lupine_raycast_all_3d_obj},

    // Physics - Shape Casts
    {"circle_cast_2d", (const mp_obj_fun_builtin_fixed_t*)&lupine_circle_cast_2d_obj},
    {"sphere_cast_3d", (const mp_obj_fun_builtin_fixed_t*)&lupine_sphere_cast_3d_obj},

    // Physics - Overlap Queries
    {"overlap_circle", (const mp_obj_fun_builtin_fixed_t*)&lupine_overlap_circle_obj},
    {"overlap_rect", (const mp_obj_fun_builtin_fixed_t*)&lupine_overlap_rect_obj},
    {"overlap_sphere", (const mp_obj_fun_builtin_fixed_t*)&lupine_overlap_sphere_obj},
    {"overlap_box", (const mp_obj_fun_builtin_fixed_t*)&lupine_overlap_box_obj},

    // Physics 2D - Body Manipulation
    {"get_linear_velocity_2d", &lupine_get_linear_velocity_2d_obj},
    {"set_linear_velocity_2d", &lupine_set_linear_velocity_2d_obj},
    {"get_angular_velocity_2d", &lupine_get_angular_velocity_2d_obj},
    {"set_angular_velocity_2d", &lupine_set_angular_velocity_2d_obj},
    {"apply_force_2d", &lupine_apply_force_2d_obj},
    {"apply_force_at_point_2d", (const mp_obj_fun_builtin_fixed_t*)&lupine_apply_force_at_point_2d_obj},
    {"apply_torque_2d", &lupine_apply_torque_2d_obj},
    {"apply_impulse_2d", &lupine_apply_impulse_2d_obj},
    {"apply_impulse_at_point_2d", (const mp_obj_fun_builtin_fixed_t*)&lupine_apply_impulse_at_point_2d_obj},
    {"apply_angular_impulse_2d", &lupine_apply_angular_impulse_2d_obj},
    {"get_mass_2d", &lupine_get_mass_2d_obj},
    {"get_gravity_scale_2d", &lupine_get_gravity_scale_2d_obj},
    {"set_gravity_scale_2d", &lupine_set_gravity_scale_2d_obj},
    {"get_linear_damping_2d", &lupine_get_linear_damping_2d_obj},
    {"set_linear_damping_2d", &lupine_set_linear_damping_2d_obj},

    // Physics 3D - Body Manipulation
    {"get_linear_velocity_3d", &lupine_get_linear_velocity_3d_obj},
    {"set_linear_velocity_3d", &lupine_set_linear_velocity_3d_obj},
    {"get_angular_velocity_3d", &lupine_get_angular_velocity_3d_obj},
    {"set_angular_velocity_3d", &lupine_set_angular_velocity_3d_obj},
    {"apply_force_3d", &lupine_apply_force_3d_obj},
    {"apply_force_at_point_3d", (const mp_obj_fun_builtin_fixed_t*)&lupine_apply_force_at_point_3d_obj},
    {"apply_torque_3d", &lupine_apply_torque_3d_obj},
    {"apply_torque_impulse_3d", &lupine_apply_torque_impulse_3d_obj},
    {"apply_impulse_3d", &lupine_apply_impulse_3d_obj},
    {"apply_impulse_at_point_3d", (const mp_obj_fun_builtin_fixed_t*)&lupine_apply_impulse_at_point_3d_obj},
    {"get_mass_3d", &lupine_get_mass_3d_obj},
    {"set_mass_3d", &lupine_set_mass_3d_obj},
    {"get_gravity_scale_3d", &lupine_get_gravity_scale_3d_obj},
    {"set_gravity_scale_3d", &lupine_set_gravity_scale_3d_obj},
    {"get_linear_damping_3d", &lupine_get_linear_damping_3d_obj},
    {"set_linear_damping_3d", &lupine_set_linear_damping_3d_obj},
    {"get_angular_damping_3d", &lupine_get_angular_damping_3d_obj},
    {"set_angular_damping_3d", &lupine_set_angular_damping_3d_obj},
    {"get_linear_factor_3d", &lupine_get_linear_factor_3d_obj},
    {"set_linear_factor_3d", &lupine_set_linear_factor_3d_obj},
    {"get_angular_factor_3d", &lupine_get_angular_factor_3d_obj},
    {"set_angular_factor_3d", &lupine_set_angular_factor_3d_obj},

    // Physics World Access
    {"set_gravity_2d", &lupine_set_gravity_2d_obj},
    {"get_gravity_2d", &lupine_get_gravity_2d_obj},
    {"set_gravity_3d", &lupine_set_gravity_3d_obj},
    {"get_gravity_3d", &lupine_get_gravity_3d_obj},

    // Character Controller 2D
    {"move_and_slide_2d", &lupine_move_and_slide_2d_obj},
    {"get_character_velocity_2d", &lupine_get_character_velocity_2d_obj},
    {"set_character_velocity_2d", &lupine_set_character_velocity_2d_obj},
    {"is_on_ground_2d", &lupine_is_on_ground_2d_obj},
    {"is_on_wall_2d", &lupine_is_on_wall_2d_obj},
    {"is_on_ceiling_2d", &lupine_is_on_ceiling_2d_obj},
    {"get_ground_normal_2d", &lupine_get_ground_normal_2d_obj},
    {"get_wall_normal_2d", &lupine_get_wall_normal_2d_obj},
    {"get_character_gravity_2d", &lupine_get_character_gravity_2d_obj},
    {"set_character_gravity_2d", &lupine_set_character_gravity_2d_obj},
    {"get_character_max_fall_speed_2d", &lupine_get_character_max_fall_speed_2d_obj},
    {"set_character_max_fall_speed_2d", &lupine_set_character_max_fall_speed_2d_obj},
    {"get_character_max_slope_angle_2d", &lupine_get_character_max_slope_angle_2d_obj},
    {"set_character_max_slope_angle_2d", &lupine_set_character_max_slope_angle_2d_obj},
    {"get_character_snap_to_ground_2d", &lupine_get_character_snap_to_ground_2d_obj},
    {"set_character_snap_to_ground_2d", &lupine_set_character_snap_to_ground_2d_obj},

    // Character Controller 3D
    {"move_and_slide_3d", &lupine_move_and_slide_3d_obj},
    {"get_character_velocity_3d", &lupine_get_character_velocity_3d_obj},
    {"set_character_velocity_3d", &lupine_set_character_velocity_3d_obj},
    {"is_on_ground_3d", &lupine_is_on_ground_3d_obj},
    {"is_on_wall_3d", &lupine_is_on_wall_3d_obj},
    {"is_on_ceiling_3d", &lupine_is_on_ceiling_3d_obj},
    {"get_ground_normal_3d", &lupine_get_ground_normal_3d_obj},
    {"get_wall_normal_3d", &lupine_get_wall_normal_3d_obj},
    {"get_character_gravity_3d", &lupine_get_character_gravity_3d_obj},
    {"set_character_gravity_3d", &lupine_set_character_gravity_3d_obj},
    {"get_character_max_fall_speed_3d", &lupine_get_character_max_fall_speed_3d_obj},
    {"set_character_max_fall_speed_3d", &lupine_set_character_max_fall_speed_3d_obj},
    {"get_character_max_slope_angle_3d", &lupine_get_character_max_slope_angle_3d_obj},
    {"set_character_max_slope_angle_3d", &lupine_set_character_max_slope_angle_3d_obj},
    {"get_character_step_height_3d", &lupine_get_character_step_height_3d_obj},
    {"set_character_step_height_3d", &lupine_set_character_step_height_3d_obj},
    {"get_character_snap_to_ground_3d", &lupine_get_character_snap_to_ground_3d_obj},
    {"set_character_snap_to_ground_3d", &lupine_set_character_snap_to_ground_3d_obj},

    {"load_archetype", &lupine_load_archetype_obj},
    {"get_archetype_field", (const mp_obj_fun_builtin_fixed_t*)&lupine_get_archetype_field_obj},
    {"get_archetype_class", &lupine_get_archetype_class_obj},
    {"archetype_is_a", (const mp_obj_fun_builtin_fixed_t*)&lupine_archetype_is_a_obj},
    {"call_archetype", (const mp_obj_fun_builtin_fixed_t*)&lupine_call_archetype_obj},
    {"load_archetype_async", (const mp_obj_fun_builtin_fixed_t*)&lupine_load_archetype_async_obj},
    {"load_archetype_definition_async", (const mp_obj_fun_builtin_fixed_t*)&lupine_load_archetype_definition_async_obj},
    {"get_archetype_load_status", &lupine_get_archetype_load_status_obj},
    {"is_archetype_load_complete", &lupine_is_archetype_load_complete_obj},
    {"get_async_archetype", &lupine_get_async_archetype_obj},
    {"cancel_archetype_load", &lupine_cancel_archetype_load_obj},
    {"set_archetype_load_priority", &lupine_set_archetype_load_priority_obj},
    {"get_archetype_load_priority", &lupine_get_archetype_load_priority_obj},
    {"set_archetype_streaming_budget", &lupine_set_archetype_streaming_budget_obj},
    {"get_archetype_streaming_budget", &lupine_get_archetype_streaming_budget_obj},
    {"get_archetype_inflight_count", &lupine_get_archetype_inflight_count_obj},
    {"get_archetype_queued_count", &lupine_get_archetype_queued_count_obj},

    {nullptr, nullptr}  // Sentinel
};

// --- Networking module ("network.start_server(...)", etc.) -----------------

static mp_obj_t lupine_net_start_server(size_t n_args, const mp_obj_t* args) {
    network::NetworkConfig config = network::NetworkManager::GetInstance().GetDefaultConfig();
    config.port = static_cast<uint16_t>(mp_obj_get_int(args[0]));
    if (n_args > 1) {
        config.maxPeers = static_cast<uint32_t>(mp_obj_get_int(args[1]));
    }
    return mp_obj_new_bool(network::NetworkManager::GetInstance().StartServer(config));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR(lupine_net_start_server_obj, 1, lupine_net_start_server);

static mp_obj_t lupine_net_start_host(size_t n_args, const mp_obj_t* args) {
    network::NetworkConfig config = network::NetworkManager::GetInstance().GetDefaultConfig();
    config.port = static_cast<uint16_t>(mp_obj_get_int(args[0]));
    if (n_args > 1) {
        config.maxPeers = static_cast<uint32_t>(mp_obj_get_int(args[1]));
    }
    return mp_obj_new_bool(network::NetworkManager::GetInstance().StartHost(config));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR(lupine_net_start_host_obj, 1, lupine_net_start_host);

static mp_obj_t lupine_net_connect(mp_obj_t address_in, mp_obj_t port_in) {
    network::NetworkConfig config = network::NetworkManager::GetInstance().GetDefaultConfig();
    config.address = mp_obj_str_get_str(address_in);
    config.port = static_cast<uint16_t>(mp_obj_get_int(port_in));
    return mp_obj_new_bool(network::NetworkManager::GetInstance().Connect(config));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_net_connect_obj, lupine_net_connect);

static mp_obj_t lupine_net_disconnect() {
    network::NetworkManager::GetInstance().Disconnect();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_net_disconnect_obj, lupine_net_disconnect);

static mp_obj_t lupine_net_is_server() {
    return mp_obj_new_bool(network::NetworkManager::GetInstance().IsServer());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_net_is_server_obj, lupine_net_is_server);

static mp_obj_t lupine_net_is_client() {
    return mp_obj_new_bool(network::NetworkManager::GetInstance().IsClient());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_net_is_client_obj, lupine_net_is_client);

static mp_obj_t lupine_net_is_active() {
    return mp_obj_new_bool(network::NetworkManager::GetInstance().IsActive());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_net_is_active_obj, lupine_net_is_active);

static mp_obj_t lupine_net_local_peer_id() {
    return mp_obj_new_int(static_cast<mp_int_t>(network::NetworkManager::GetInstance().GetLocalPeerId()));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_net_local_peer_id_obj, lupine_net_local_peer_id);

static mp_obj_t lupine_net_peer_count() {
    return mp_obj_new_int(static_cast<mp_int_t>(network::NetworkManager::GetInstance().GetPeerCount()));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_net_peer_count_obj, lupine_net_peer_count);

static mp_obj_t lupine_net_get_peers() {
    mp_obj_t list = mp_obj_new_list(0, nullptr);
    for (const network::PeerInfo& peer : network::NetworkManager::GetInstance().GetPeers()) {
        mp_obj_list_append(list, mp_obj_new_int(static_cast<mp_int_t>(peer.id)));
    }
    return list;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_net_get_peers_obj, lupine_net_get_peers);

static mp_obj_t lupine_net_connect_signal(mp_obj_t event_in, mp_obj_t node_in, mp_obj_t method_in) {
    NodeRef target;
    if (!node_arg(node_in, target)) {
        return mp_obj_new_int(0);
    }
    std::shared_ptr<core::Node> shared = target.Lock();
    if (!shared) {
        return mp_obj_new_int(0);
    }
    uint64_t id = network::NetworkManager::GetInstance().Events().Connect(
        mp_obj_str_get_str(event_in), shared.get(), mp_obj_str_get_str(method_in));
    return mp_obj_new_int_from_ull(id);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lupine_net_connect_signal_obj, lupine_net_connect_signal);

static mp_obj_t lupine_net_get_peer_rtt(mp_obj_t peer_in) {
    const mp_int_t peerId = mp_obj_get_int(peer_in);
    for (const network::PeerInfo& peer : network::NetworkManager::GetInstance().GetPeers()) {
        if (static_cast<mp_int_t>(peer.id) == peerId) {
            return mp_obj_new_float(static_cast<mp_float_t>(peer.roundTripMs));
        }
    }
    return mp_obj_new_float(0.0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_net_get_peer_rtt_obj, lupine_net_get_peer_rtt);

static mp_obj_t lupine_net_get_stats() {
    const network::NetworkStats s = network::NetworkManager::GetInstance().GetStats();
    mp_obj_t dict = mp_obj_new_dict(9);
    auto put = [&](const char* key, mp_obj_t value) {
        mp_obj_dict_store(dict, mp_obj_new_str(key, strlen(key)), value);
    };
    put("peer_count", mp_obj_new_int(static_cast<mp_int_t>(s.peerCount)));
    put("bytes_in_per_sec", mp_obj_new_float(static_cast<mp_float_t>(s.bytesInPerSec)));
    put("bytes_out_per_sec", mp_obj_new_float(static_cast<mp_float_t>(s.bytesOutPerSec)));
    put("total_bytes_in", mp_obj_new_float(static_cast<mp_float_t>(s.totalBytesIn)));
    put("total_bytes_out", mp_obj_new_float(static_cast<mp_float_t>(s.totalBytesOut)));
    put("average_rtt_ms", mp_obj_new_float(static_cast<mp_float_t>(s.averageRttMs)));
    put("packet_loss_percent", mp_obj_new_float(static_cast<mp_float_t>(s.averagePacketLossPercent)));
    put("snapshots_sent", mp_obj_new_float(static_cast<mp_float_t>(s.snapshotsSent)));
    put("snapshots_received", mp_obj_new_float(static_cast<mp_float_t>(s.snapshotsReceived)));
    return dict;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_net_get_stats_obj, lupine_net_get_stats);

static mp_obj_t lupine_net_get_peer_loss(mp_obj_t peer_in) {
    network::PeerInfo info;
    if (network::NetworkManager::GetInstance().GetPeerInfo(
            static_cast<network::PeerId>(mp_obj_get_int(peer_in)), info)) {
        return mp_obj_new_float(static_cast<mp_float_t>(info.packetLossPercent));
    }
    return mp_obj_new_float(0.0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_net_get_peer_loss_obj, lupine_net_get_peer_loss);

static mp_obj_t lupine_net_get_peer_jitter(mp_obj_t peer_in) {
    network::PeerInfo info;
    if (network::NetworkManager::GetInstance().GetPeerInfo(
            static_cast<network::PeerId>(mp_obj_get_int(peer_in)), info)) {
        return mp_obj_new_float(static_cast<mp_float_t>(info.jitterMs));
    }
    return mp_obj_new_float(0.0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_net_get_peer_jitter_obj, lupine_net_get_peer_jitter);

static mp_obj_t lupine_net_kick_peer(mp_obj_t peer_in) {
    return mp_obj_new_bool(network::NetworkManager::GetInstance().KickPeer(
        static_cast<network::PeerId>(mp_obj_get_int(peer_in))));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lupine_net_kick_peer_obj, lupine_net_kick_peer);

static mp_obj_t lupine_net_set_interest_2d(mp_obj_t x_in, mp_obj_t y_in) {
    network::NetworkManager::GetInstance().SetInterestPosition2D(
        static_cast<float>(mp_obj_get_float(x_in)), static_cast<float>(mp_obj_get_float(y_in)));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lupine_net_set_interest_2d_obj, lupine_net_set_interest_2d);

static mp_obj_t lupine_net_set_interest_3d(mp_obj_t x_in, mp_obj_t y_in, mp_obj_t z_in) {
    network::NetworkManager::GetInstance().SetInterestPosition3D(
        static_cast<float>(mp_obj_get_float(x_in)), static_cast<float>(mp_obj_get_float(y_in)),
        static_cast<float>(mp_obj_get_float(z_in)));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lupine_net_set_interest_3d_obj, lupine_net_set_interest_3d);

static mp_obj_t lupine_net_start_lan_advertising() {
    return mp_obj_new_bool(network::NetworkManager::GetInstance().StartLanAdvertising());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_net_start_lan_advertising_obj, lupine_net_start_lan_advertising);

static mp_obj_t lupine_net_stop_lan_advertising() {
    network::NetworkManager::GetInstance().StopLanAdvertising();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_net_stop_lan_advertising_obj, lupine_net_stop_lan_advertising);

static mp_obj_t lupine_net_is_lan_advertising() {
    return mp_obj_new_bool(network::NetworkManager::GetInstance().IsLanAdvertising());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_net_is_lan_advertising_obj, lupine_net_is_lan_advertising);

static mp_obj_t lupine_net_start_lan_discovery(size_t n_args, const mp_obj_t* args) {
    const char* gameId = mp_obj_str_get_str(args[0]);
    const uint16_t port = (n_args > 1) ? static_cast<uint16_t>(mp_obj_get_int(args[1])) : 7779;
    return mp_obj_new_bool(network::NetworkManager::GetInstance().StartLanDiscovery(gameId, port));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lupine_net_start_lan_discovery_obj, 1, 2,
                                           lupine_net_start_lan_discovery);

static mp_obj_t lupine_net_stop_lan_discovery() {
    network::NetworkManager::GetInstance().StopLanDiscovery();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_net_stop_lan_discovery_obj, lupine_net_stop_lan_discovery);

static mp_obj_t lupine_net_is_lan_discovering() {
    return mp_obj_new_bool(network::NetworkManager::GetInstance().IsLanDiscovering());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_net_is_lan_discovering_obj, lupine_net_is_lan_discovering);

static mp_obj_t lupine_net_get_discovered_servers() {
    mp_obj_t list = mp_obj_new_list(0, nullptr);
    for (const network::LanServerInfo& server :
         network::NetworkManager::GetInstance().GetDiscoveredServers()) {
        mp_obj_t d = mp_obj_new_dict(8);
        mp_obj_dict_store(d, mp_obj_new_str("game_id", 7), mp_obj_new_str(server.gameId.c_str(), server.gameId.size()));
        mp_obj_dict_store(d, mp_obj_new_str("name", 4), mp_obj_new_str(server.name.c_str(), server.name.size()));
        mp_obj_dict_store(d, mp_obj_new_str("address", 7), mp_obj_new_str(server.address.c_str(), server.address.size()));
        mp_obj_dict_store(d, mp_obj_new_str("port", 4), mp_obj_new_int(static_cast<mp_int_t>(server.gamePort)));
        mp_obj_dict_store(d, mp_obj_new_str("player_count", 12), mp_obj_new_int(static_cast<mp_int_t>(server.playerCount)));
        mp_obj_dict_store(d, mp_obj_new_str("max_players", 11), mp_obj_new_int(static_cast<mp_int_t>(server.maxPlayers)));
        mp_obj_dict_store(d, mp_obj_new_str("protocol_version", 16), mp_obj_new_int(static_cast<mp_int_t>(server.protocolVersion)));
        mp_obj_dict_store(d, mp_obj_new_str("age_seconds", 11), mp_obj_new_float(static_cast<mp_float_t>(server.ageSeconds)));
        mp_obj_list_append(list, d);
    }
    return list;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lupine_net_get_discovered_servers_obj, lupine_net_get_discovered_servers);

static const LupineFunction s_NetworkFunctions[] = {
    {"start_server", (const mp_obj_fun_builtin_fixed_t*)&lupine_net_start_server_obj},
    {"start_host", (const mp_obj_fun_builtin_fixed_t*)&lupine_net_start_host_obj},
    {"connect", &lupine_net_connect_obj},
    {"disconnect", &lupine_net_disconnect_obj},
    {"is_server", &lupine_net_is_server_obj},
    {"is_client", &lupine_net_is_client_obj},
    {"is_active", &lupine_net_is_active_obj},
    {"get_local_peer_id", &lupine_net_local_peer_id_obj},
    {"get_peer_count", &lupine_net_peer_count_obj},
    {"get_peers", &lupine_net_get_peers_obj},
    {"connect_signal", &lupine_net_connect_signal_obj},
    {"get_peer_rtt", (const mp_obj_fun_builtin_fixed_t*)&lupine_net_get_peer_rtt_obj},
    {"get_stats", (const mp_obj_fun_builtin_fixed_t*)&lupine_net_get_stats_obj},
    {"get_peer_loss", (const mp_obj_fun_builtin_fixed_t*)&lupine_net_get_peer_loss_obj},
    {"get_peer_jitter", (const mp_obj_fun_builtin_fixed_t*)&lupine_net_get_peer_jitter_obj},
    {"kick_peer", (const mp_obj_fun_builtin_fixed_t*)&lupine_net_kick_peer_obj},
    {"set_interest_2d", (const mp_obj_fun_builtin_fixed_t*)&lupine_net_set_interest_2d_obj},
    {"set_interest_3d", (const mp_obj_fun_builtin_fixed_t*)&lupine_net_set_interest_3d_obj},
    {"start_lan_advertising", &lupine_net_start_lan_advertising_obj},
    {"stop_lan_advertising", &lupine_net_stop_lan_advertising_obj},
    {"is_lan_advertising", &lupine_net_is_lan_advertising_obj},
    {"start_lan_discovery", (const mp_obj_fun_builtin_fixed_t*)&lupine_net_start_lan_discovery_obj},
    {"stop_lan_discovery", &lupine_net_stop_lan_discovery_obj},
    {"is_lan_discovering", &lupine_net_is_lan_discovering_obj},
    {"get_discovered_servers", &lupine_net_get_discovered_servers_obj},
    {nullptr, nullptr}  // Sentinel
};

// Flag to track if module is registered
static bool s_LupineModuleRegistered = false;

// Helper to create an empty Python class, inheriting from `base`, that scripts can
// inherit from. Built by calling type(name, (base,), {}).
static mp_obj_t create_component_class_with_base(const char* name, mp_obj_t base) {
    // Get the qstr for the class name
    qstr name_qstr = safe_qstr_from_str(name);

    // Create the arguments for type() call
    mp_obj_t name_obj = MP_OBJ_NEW_QSTR(name_qstr);
    mp_obj_t bases_items[1] = { base };
    mp_obj_t bases_tuple = mp_obj_new_tuple(1, bases_items);
    mp_obj_t dict = mp_obj_new_dict(0);

    // Build the class by calling type(name, bases, dict)
    mp_obj_t args[3] = { name_obj, bases_tuple, dict };

    // Call type() as a function to create a new class
    mp_obj_t new_class = mp_call_function_n_kw(MP_OBJ_FROM_PTR(&mp_type_type), 3, 0, args);
    return new_class;
}

// Helper to create an empty Python class that scripts can inherit from
static mp_obj_t create_component_class(const char* name) {
    return create_component_class_with_base(name, MP_OBJ_FROM_PTR(&mp_type_object));
}

// Look a name up in a globals dict; MP_OBJ_NULL when absent.
static mp_obj_t lookup_in_globals(mp_obj_t globals, const char* name) {
    if (!globals) {
        return MP_OBJ_NULL;
    }
    mp_obj_dict_t* dict = (mp_obj_dict_t*)MP_OBJ_TO_PTR(globals);
    mp_map_elem_t* elem = mp_map_lookup(&dict->map,
                                        MP_OBJ_NEW_QSTR(safe_qstr_from_str(name)),
                                        MP_MAP_LOOKUP);
    return elem ? elem->value : MP_OBJ_NULL;
}

// Define `className` - and, first, its whole base chain - as classes in `globals`,
// so a custom component is an inheritable base exactly like a built-in one. Without
// this a chain such as SimObject(Sprite2D) -> SimBoulder(SimObject) dies when the
// script is executed: SimObject is not a name in the VM, so `class SimBoulder(SimObject):`
// raises NameError and the whole component script fails to load.
// `visiting` guards a cyclic definition; the registry already breaks cycles at scan
// time, so this is belt-and-braces. Returns the class, or MP_OBJ_NULL if unresolvable.
static mp_obj_t ensure_custom_component_class(mp_obj_t globals, const std::string& className,
                                              std::set<std::string>& visiting) {
    if (!globals || className.empty()) {
        return MP_OBJ_NULL;
    }

    // Already present: a built-in component class, or a custom one from an earlier
    // pass. Either way it is the base to inherit from, and must not be replaced.
    if (mp_obj_t existing = lookup_in_globals(globals, className.c_str())) {
        return existing;
    }

    const lupine::core::CustomComponentDefinition* def =
        lupine::core::CustomComponentRegistry::GetInstance().GetDefinition(className);
    if (!def || !def->isValid) {
        return MP_OBJ_NULL;  // not a component type we know about
    }

    if (!visiting.insert(className).second) {
        return MP_OBJ_NULL;  // cycle
    }

    // Define the base first, so each custom class inherits from its real base rather
    // than a flattened Component and Python's own isinstance() matches the type chain.
    mp_obj_t base = MP_OBJ_NULL;
    if (!def->baseComponentType.empty()) {
        base = ensure_custom_component_class(globals, def->baseComponentType, visiting);
    }
    if (!base) {
        base = lookup_in_globals(globals, "Component");
    }
    if (!base) {
        base = MP_OBJ_FROM_PTR(&mp_type_object);
    }

    visiting.erase(className);

    mp_obj_t cls = create_component_class_with_base(className.c_str(), base);
    mp_obj_dict_store(globals, MP_OBJ_NEW_QSTR(safe_qstr_from_str(className.c_str())), cls);
    return cls;
}

// List of base component types that scripts can inherit from
static const char* s_BaseComponentTypes[] = {
    // Base
    "Component",

    // 2D Rendering
    "Sprite2D", "Particles2D", "AnimatedSprite2D", "GifPlayer", "VideoPlayer", "ColorRect",
    "Image2D", "NineSlicePanel",
    "Shape2D", "Line2D", "Curve2D", "Light2D", "LightOccluder2D", "VectorGraphic2D", "Empty2D",

    // 3D Rendering
    "Sprite3D", "Particles3D", "AnimatedSprite3D", "StaticMesh3D", "SkeletalMesh3D",
    "PrimitiveMesh3D", "Label3D", "Panel3D", "Button3D", "ProgressBar3D", "Curve3D", "Path3D", "PathFollow3D", "Empty3D",

    // UI
    "Button", "TextureButton", "ToggleButton", "Checkbox", "RadioButton",
    "Label", "ProgressBar", "Slider", "LineEdit", "SpinBox", "TextEdit", "ItemList", "Dropdown", "PopupMenu", "RichTextLabel", "Tree", "Panel", "Container", "HorizontalContainer",
    "VerticalContainer", "GridContainer", "PaddingContainer", "CenterContainer",
    "DockContainer", "Stack", "Wrap", "ScrollContainer", "TabContainer",
    "SplitContainer", "AspectRatioContainer",
    "Spacer", "LayoutSlot",

    // Audio
    "AudioPlayer", "AudioListener",

    // Physics 2D
    "RigidBody2DComponent", "StaticBody2DComponent", "KinematicBody2DComponent",
    "AreaTrigger2DComponent", "CollisionBody2DComponent", "CharacterController2DComponent",
    "RayCast2D", "ShapeCast2D",

    // Physics 3D
    "RigidBody3DComponent", "StaticBody3DComponent", "KinematicBody3DComponent",
    "AreaTrigger3DComponent", "CharacterController3DComponent",
    "RayCast3D", "ShapeCast3D",

    // Lighting
    "DirectionalLight3D", "OmniLight3D", "SpotLight3D",

    // Utility
    "Timer", "WorldEnvironment", "Camera2D", "Camera3D",
    "TileMap2D", "ParticleEmitter2D", "ParticleEmitter3D", "YSort",
    "ParallaxBackground", "ParallaxLayer",
    "NavigationRegion2D", "NavigationAgent2D", "NavigationObstacle2D",
    "NavigationRegion3D", "NavigationAgent3D", "NavigationObstacle3D",

    // Networking
    "NetworkObject", "NetworkSynchronizer", "NetworkTransform2D", "NetworkTransform3D", "NetworkSpawner",
    "NetworkController", "NetworkAnimator", "NetworkRigidBody2D", "NetworkRigidBody3D",
    "SubViewport", "AnimationPlayer", "AnimationTree",

    // Camera effects (stackable per-camera post-effects)
    "CameraEffectColorGrade", "CameraEffectTonemap", "CameraEffectVignette", "CameraEffectFilmGrain",
    "CameraEffectColorInvert", "CameraEffectPosterize", "CameraEffectHueShift", "CameraEffectBlur",
    "CameraEffectGlow", "CameraEffectOutline", "CameraEffectPixelate", "CameraEffectSharpen",
    "CameraEffectChromaticAberration",

    nullptr  // Sentinel
};

// Register the lupine module at runtime
static void RegisterLupineModule() {
    if (s_LupineModuleRegistered) return;

    // Create a proper MicroPython module (supports attribute access via lupine.func())
    qstr lupine_qstr = safe_qstr_from_str("lupine");
    mp_obj_t module_obj = mp_obj_new_module(lupine_qstr);

    // Get the module's globals dictionary
    mp_obj_dict_t* module_globals = mp_obj_module_get_globals(module_obj);

    // Add all functions to the module globals
    for (const LupineFunction* func = s_LupineFunctions; func->name != nullptr; func++) {
        qstr func_qstr = safe_qstr_from_str(func->name);
        mp_obj_dict_store(MP_OBJ_FROM_PTR(module_globals), MP_OBJ_NEW_QSTR(func_qstr), MP_OBJ_FROM_PTR(func->func));
    }

    // Signal connection flag constants (match core::ConnectFlags / LC_CONNECT_*).
    mp_obj_dict_store(MP_OBJ_FROM_PTR(module_globals),
                      MP_OBJ_NEW_QSTR(safe_qstr_from_str("CONNECT_NONE")),
                      MP_OBJ_NEW_SMALL_INT(0));
    mp_obj_dict_store(MP_OBJ_FROM_PTR(module_globals),
                      MP_OBJ_NEW_QSTR(safe_qstr_from_str("CONNECT_DEFERRED")),
                      MP_OBJ_NEW_SMALL_INT(1));
    mp_obj_dict_store(MP_OBJ_FROM_PTR(module_globals),
                      MP_OBJ_NEW_QSTR(safe_qstr_from_str("CONNECT_ONESHOT")),
                      MP_OBJ_NEW_SMALL_INT(2));

    // Mouse mode constants (match ScriptAPI::MouseMode).
    mp_obj_dict_store(MP_OBJ_FROM_PTR(module_globals),
                      MP_OBJ_NEW_QSTR(safe_qstr_from_str("MOUSE_MODE_VISIBLE")),
                      MP_OBJ_NEW_SMALL_INT(0));
    mp_obj_dict_store(MP_OBJ_FROM_PTR(module_globals),
                      MP_OBJ_NEW_QSTR(safe_qstr_from_str("MOUSE_MODE_HIDDEN")),
                      MP_OBJ_NEW_SMALL_INT(1));
    mp_obj_dict_store(MP_OBJ_FROM_PTR(module_globals),
                      MP_OBJ_NEW_QSTR(safe_qstr_from_str("MOUSE_MODE_CAPTURED")),
                      MP_OBJ_NEW_SMALL_INT(2));
    mp_obj_dict_store(MP_OBJ_FROM_PTR(module_globals),
                      MP_OBJ_NEW_QSTR(safe_qstr_from_str("MOUSE_MODE_CONFINED")),
                      MP_OBJ_NEW_SMALL_INT(3));
    mp_obj_dict_store(MP_OBJ_FROM_PTR(module_globals),
                      MP_OBJ_NEW_QSTR(safe_qstr_from_str("MOUSE_MODE_CONFINED_HIDDEN")),
                      MP_OBJ_NEW_SMALL_INT(4));

    // Native value types (pure-math objects: Vector2 / Vector3 / Color). The
    // type objects are themselves callable, so storing them makes the names act
    // as constructors both as lupine.Vector2(...) and bare Vector2(...).
    struct ValueTypeEntry { const char* name; const mp_obj_type_t* type; };
    const ValueTypeEntry valueTypes[] = {
        {"Vector2", &lupine_vector2_type},
        {"Vector3", &lupine_vector3_type},
        {"Color", &lupine_color_type},
    };
    for (const ValueTypeEntry& vt : valueTypes) {
        qstr vt_qstr = safe_qstr_from_str(vt.name);
        mp_obj_dict_store(MP_OBJ_FROM_PTR(module_globals), MP_OBJ_NEW_QSTR(vt_qstr),
                          MP_OBJ_FROM_PTR(vt.type));
    }

    // Store the module in the script's global namespace
    mp_store_name(lupine_qstr, module_obj);

    // Native value types are also exposed as bare globals for ergonomic use.
    for (const ValueTypeEntry& vt : valueTypes) {
        mp_store_name(safe_qstr_from_str(vt.name), MP_OBJ_FROM_PTR(vt.type));
    }

    // Register base component classes in the global namespace for inheritance
    // These allow patterns like: class MySprite(Sprite2D):
    for (const char** type = s_BaseComponentTypes; *type != nullptr; type++) {
        mp_obj_t class_obj = create_component_class(*type);
        qstr type_qstr = safe_qstr_from_str(*type);
        mp_store_name(type_qstr, class_obj);
    }

    // Networking module (network.start_server(...), network.is_server(), ...).
    qstr network_qstr = safe_qstr_from_str("network");
    mp_obj_t network_module = mp_obj_new_module(network_qstr);
    mp_obj_dict_t* network_globals = mp_obj_module_get_globals(network_module);
    for (const LupineFunction* func = s_NetworkFunctions; func->name != nullptr; func++) {
        qstr func_qstr = safe_qstr_from_str(func->name);
        mp_obj_dict_store(MP_OBJ_FROM_PTR(network_globals), MP_OBJ_NEW_QSTR(func_qstr),
                          MP_OBJ_FROM_PTR(func->func));
    }
    mp_store_name(network_qstr, network_module);

    s_LupineModuleRegistered = true;

}

// ============================================================================
// MicroPythonEnvironment Implementation
// ============================================================================

// ============================================================================
// Dispatch scope: makes an instance active (s_CurrentEnv, for API resolution)
// and swaps the VM's globals/locals to that instance's module namespace.
// ============================================================================
namespace {
struct MpDispatchScope {
    MicroPythonEnvironment* prevEnv;
    mp_obj_dict_t* prevGlobals;
    mp_obj_dict_t* prevLocals;
    explicit MpDispatchScope(MicroPythonEnvironment* env) {
        prevEnv = s_CurrentEnv;
        prevGlobals = mp_globals_get();
        prevLocals = mp_locals_get();
        s_CurrentEnv = env;
        mp_obj_t g = env ? env->GetGlobalsDict() : nullptr;
        if (g) {
            mp_obj_dict_t* d = (mp_obj_dict_t*)MP_OBJ_TO_PTR(g);
            mp_globals_set(d);
            mp_locals_set(d);
        }
    }
    ~MpDispatchScope() {
        mp_globals_set(prevGlobals);
        mp_locals_set(prevLocals);
        s_CurrentEnv = prevEnv;
    }
};
} // namespace

// ============================================================================
// MicroPythonHost - the single shared MicroPython VM
// ============================================================================

MicroPythonHost& MicroPythonHost::Instance() {
    static MicroPythonHost s_Instance;
    return s_Instance;
}

bool MicroPythonHost::EnsureInitialized(size_t heapSize) {
    if (m_Initialized) {
        return m_ModuleReady;
    }

    m_HeapSize = heapSize ? heapSize : m_HeapSize;
    m_Heap.resize(m_HeapSize);
    mp_embed_init(m_Heap.data(), m_Heap.size(), m_StackTop);

    // The VM exists from here on and must never be re-initialised, so the flag is
    // set even if the registration below fails; EnsureInitialized then reports the
    // failure through m_ModuleReady instead of re-running mp_embed_init.
    m_Initialized = true;

    // Register the lupine module + component classes into __main__ (the namespace
    // active right after init). This is the shared base every instance seeds from.
    // It performs hundreds of dict stores and ~140 class creations, every one of
    // which can raise MemoryError on the fixed heap; without an NLR handler that
    // exception reaches nlr_jump_fail and aborts the process.
    {
        MP_SETUP_STACK();
        nlr_buf_t nlr;
        if (nlr_push(&nlr) == 0) {
            RegisterLupineModule();
            nlr_pop();
            m_ModuleReady = true;
        } else {
            m_ModuleReady = false;
        }
    }

    if (!m_ModuleReady) {
        LOG_ERROR(LogCategory::Scripting,
                  "MicroPython: failed to register the lupine module (heap too small?); "
                  "scripting is unavailable");
        return false;
    }

    // Capture __main__ as the shared globals.
    m_SharedGlobals = MP_OBJ_FROM_PTR(mp_globals_get());

    // Install the pure-Python coroutine/await scheduler into __main__. `lupine`
    // is a name there so the scheduler references it directly; the scheduler's
    // closures keep _lup_coros in __main__, so it is process-global and pumped
    // once per frame by Pump(). Instances see lupine via their seeded namespace.
    {
        MP_SETUP_STACK();
        nlr_buf_t nlr;
        if (nlr_push(&nlr) == 0) {
            const char* schedulerCode = R"PY(
_lup = lupine
_lup_coros = []
def _lup_step(entry, dt):
    try:
        if entry["started"]:
            entry["wait"] = entry["gen"].send(dt)
        else:
            entry["wait"] = next(entry["gen"])
            entry["started"] = True
    except StopIteration:
        entry["dead"] = True
    except Exception as e:
        _lup.log_error("coroutine error: " + str(e))
        entry["dead"] = True
def _lup_start(fn):
    gen = fn()
    entry = {"gen": gen, "wait": None, "dead": False, "started": False, "env": _lup._env_id()}
    _lup_coros.append(entry)
    _lup_step(entry, 0.0)
    return gen
def _lupine_pump(dt):
    global _lup_coros
    alive = []
    for entry in _lup_coros:
        if entry["dead"]:
            continue
        if not _lup._env_enter(entry["env"]):
            entry["dead"] = True
            continue
        try:
            w = entry["wait"]
            ready = False
            if w is None:
                ready = True
            elif w["type"] == "time":
                w["t"] -= dt
                ready = w["t"] <= 0.0
            elif w["type"] == "frames":
                w["n"] -= 1
                ready = w["n"] <= 0
            elif w["type"] == "until":
                ready = bool(w["fn"]())
            else:
                ready = True
            if ready:
                _lup_step(entry, dt)
        except Exception as e:
            _lup.log_error("coroutine error: " + str(e))
            entry["dead"] = True
        finally:
            _lup._env_exit()
        if not entry["dead"]:
            alive.append(entry)
    _lup_coros = alive
def _lup_stop(gen):
    for e in _lup_coros:
        if e["gen"] is gen:
            e["dead"] = True
def _lup_kill_env(env):
    for e in _lup_coros:
        if e["env"] == env:
            e["dead"] = True
def _lup_await_signal(obj, signal):
    aw = obj.await_signal(signal)
    if aw is None:
        return {"type": "frames", "n": 0}
    return {"type": "until", "fn": (lambda: (not aw.is_valid()) or aw.is_fired())}
_lup.start_coroutine = _lup_start
_lup.stop_coroutine = _lup_stop
_lup.coroutine_count = lambda: len(_lup_coros)
_lup.await_seconds = lambda s: {"type": "time", "t": s}
_lup.await_frames = lambda n=1: {"type": "frames", "n": n}
_lup.await_next_frame = lambda: {"type": "frames", "n": 1}
_lup.await_until = lambda fn: {"type": "until", "fn": fn}
_lup.await_tween = lambda tw: {"type": "until", "fn": (lambda: (not tw.is_valid()) or tw.is_finished())}
_lup.await_signal = _lup_await_signal
_lup.await_archetype = lambda handle: {"type": "until", "fn": (lambda: _lup.is_archetype_load_complete(handle))}
_lup.STREAM_PRIORITY_LOW = -100
_lup.STREAM_PRIORITY_NORMAL = 0
_lup.STREAM_PRIORITY_HIGH = 100
_lup.STREAM_PRIORITY_CRITICAL = 1000
)PY";
            mp_lexer_t* lex = mp_lexer_new_from_str_len(MP_QSTR__lt_string_gt_, schedulerCode,
                                                        strlen(schedulerCode), 0);
            qstr source_name = lex->source_name;
            mp_parse_tree_t parse_tree = mp_parse(lex, MP_PARSE_FILE_INPUT);
            mp_obj_t module_fun = mp_compile(&parse_tree, source_name, true);
            mp_call_function_0(module_fun);
            nlr_pop();
        }
        // A failed scheduler install is non-fatal: scripts that never use
        // coroutines are unaffected.
    }

    return true;
}

void MicroPythonHost::RegisterCustomComponentTypes() {
    if (!m_Initialized || !m_SharedGlobals) {
        return;
    }

    MP_SETUP_STACK();
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        // type() is called to build each class, so run with __main__ as the active
        // namespace, exactly as the built-in class registration does.
        mp_obj_dict_t* prevGlobals = mp_globals_get();
        mp_obj_dict_t* prevLocals = mp_locals_get();
        mp_obj_dict_t* shared = (mp_obj_dict_t*)MP_OBJ_TO_PTR(m_SharedGlobals);
        mp_globals_set(shared);
        mp_locals_set(shared);

        for (const core::CustomComponentDefinition& def :
             core::CustomComponentRegistry::GetInstance().GetDefinitions()) {
            if (!def.isValid || def.className.empty()) {
                continue;
            }
            std::set<std::string> visiting;
            ensure_custom_component_class(m_SharedGlobals, def.className, visiting);
        }

        mp_globals_set(prevGlobals);
        mp_locals_set(prevLocals);
        nlr_pop();
    }
}

int MicroPythonHost::AcquireModuleId() {
    if (!m_FreeModuleIds.empty()) {
        const int id = m_FreeModuleIds.back();
        m_FreeModuleIds.pop_back();
        return id;
    }
    return m_NextModuleId++;
}

void MicroPythonHost::ReleaseModuleId(int id) {
    if (id < 0) {
        return;
    }
    for (size_t i = 0; i < m_FreeModuleIds.size(); ++i) {
        if (m_FreeModuleIds[i] == id) {
            return;
        }
    }
    m_FreeModuleIds.push_back(id);
}

void MicroPythonHost::Pump(float deltaTime) {
    if (!m_Initialized || !m_ModuleReady || !m_SharedGlobals) {
        return;
    }

    // The scheduler's state lives in __main__; pump it with __main__ active. Each
    // coroutine's own instance is made active around its resume by the scheduler
    // itself (lupine._env_enter / _env_exit), so self-relative APIs keep working
    // after the first yield.
    mp_obj_dict_t* prevGlobals = mp_globals_get();
    mp_obj_dict_t* prevLocals = mp_locals_get();
    mp_obj_dict_t* shared = (mp_obj_dict_t*)MP_OBJ_TO_PTR(m_SharedGlobals);
    mp_globals_set(shared);
    mp_locals_set(shared);

    {
        MP_SETUP_STACK();
        nlr_buf_t nlr;
        if (nlr_push(&nlr) == 0) {
            qstr qst = safe_qstr_from_str("_lupine_pump");
            mp_obj_t func = mp_load_name(qst);
            if (mp_obj_is_callable(func)) {
                mp_call_function_1(func, mp_obj_new_float(deltaTime));
            }
            nlr_pop();
        } else {
            // A pump that raised long-jumped past the scheduler's own _env_exit.
            s_EnvActivationStack.clear();
            s_CurrentEnv = nullptr;
        }
    }

    mp_globals_set(prevGlobals);
    mp_locals_set(prevLocals);

    // Reclaim finished signal awaiters and the handle slots of destroyed objects
    // (pure C++, no VM allocation) after the pump.
    SweepAwaiterPool();
    SweepHandlePools();
}

// ============================================================================
// MicroPythonEnvironment - a per-component script instance (its own module)
// ============================================================================

MicroPythonEnvironment::MicroPythonEnvironment()
    : m_ScriptAPI(nullptr)
{
}

MicroPythonEnvironment::~MicroPythonEnvironment() {
    Shutdown();
}

void MicroPythonEnvironment::SetHeapSize(size_t bytes) {
    MicroPythonHost::Instance().SetHeapSize(bytes);
}

bool MicroPythonEnvironment::Initialize() {
    if (m_Initialized) {
        return true;
    }

    MicroPythonHost& host = MicroPythonHost::Instance();
    if (!host.EnsureInitialized(host.GetHeapSize())) {
        m_LastError = "MicroPython host failed to initialize";
        return false;
    }

    m_Initialized = true;
    if (!EnsureGlobals()) {
        m_Initialized = false;
        m_LastError = "Failed to create MicroPython instance namespace";
        return false;
    }
    return true;
}

void MicroPythonEnvironment::Shutdown() {
    if (!m_Initialized) {
        return;
    }

    MicroPythonHost& host = MicroPythonHost::Instance();

    // The VM is owned by the host and outlives every instance, so it is not torn
    // down here. This instance's namespace, however, must be: the module is rooted
    // by sys.modules and holds a full copy of the __main__ seed plus every def the
    // script made, so leaving it registered leaks one whole namespace per script
    // instance out of a fixed 256 KB heap.
    if (host.IsInitialized() && m_Globals) {
        MP_SETUP_STACK();
        nlr_buf_t nlr;
        if (nlr_push(&nlr) == 0) {
            // Retire this instance's parked coroutines first: they hold the module
            // (through their generator's context) and would otherwise be resumed
            // against a freed instance.
            mp_obj_dict_t* prevGlobals = mp_globals_get();
            mp_obj_dict_t* prevLocals = mp_locals_get();
            mp_obj_t shared = host.SharedGlobals();
            if (shared) {
                mp_obj_dict_t* d = (mp_obj_dict_t*)MP_OBJ_TO_PTR(shared);
                mp_globals_set(d);
                mp_locals_set(d);
                mp_obj_t killer = mp_load_name(safe_qstr_from_str("_lup_kill_env"));
                if (mp_obj_is_callable(killer)) {
                    mp_call_function_1(killer, mp_obj_new_int(m_EnvId));
                }
            }
            mp_globals_set(prevGlobals);
            mp_locals_set(prevLocals);

            if (!m_ModuleName.empty()) {
                qstr modQstr = safe_qstr_from_str(m_ModuleName.c_str());
                mp_map_lookup(&MP_STATE_VM(mp_loaded_modules_dict).map, MP_OBJ_NEW_QSTR(modQstr),
                              MP_MAP_LOOKUP_REMOVE_IF_FOUND);
            }
            nlr_pop();
        }
    }

    // The module name (and its interned qstr) is recycled through the host's free
    // list, so a spawn/despawn cycle reuses one name rather than interning a new
    // one into the permanent qstr pool every time.
    if (m_ModuleId >= 0) {
        host.ReleaseModuleId(m_ModuleId);
        m_ModuleId = -1;
    }
    m_ModuleName.clear();

    if (m_EnvId > 0) {
        s_EnvById.erase(m_EnvId);
        m_EnvId = 0;
    }

    m_Globals = nullptr;
    m_Initialized = false;
    for (size_t i = 0; i < s_EnvActivationStack.size(); ++i) {
        if (s_EnvActivationStack[i] == this) {
            s_EnvActivationStack[i] = nullptr;
        }
    }
    if (s_CurrentEnv == this) {
        s_CurrentEnv = nullptr;
    }
}

void MicroPythonEnvironment::Update(float deltaTime) {
    // The coroutine/await scheduler is process-global and pumped exactly once per
    // frame by the SceneManager via MicroPythonHost::Pump; the per-instance tick
    // is a no-op so coroutines are not advanced once per script.
    (void)deltaTime;
}

void MicroPythonEnvironment::RegisterCustomComponentTypes() {
    if (!m_Initialized) {
        return;
    }

    MicroPythonHost& host = MicroPythonHost::Instance();
    host.RegisterCustomComponentTypes();

    // An instance namespace is a *snapshot copy* of __main__ taken when it was created
    // (see EnsureGlobals), not a live fall-through. Classes registered into __main__
    // after that point - which is every custom class discovered since this instance was
    // created - therefore have to be copied in explicitly, or the script would still
    // raise NameError on its own base class.
    mp_obj_t shared = host.SharedGlobals();
    if (!m_Globals || !shared) {
        return;
    }

    MP_SETUP_STACK();
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        for (const core::CustomComponentDefinition& def :
             core::CustomComponentRegistry::GetInstance().GetDefinitions()) {
            if (!def.isValid || def.className.empty()) {
                continue;
            }
            mp_obj_t cls = lookup_in_globals(shared, def.className.c_str());
            if (cls) {
                mp_obj_dict_store(m_Globals,
                                  MP_OBJ_NEW_QSTR(safe_qstr_from_str(def.className.c_str())),
                                  cls);
            }
        }
        nlr_pop();
    }
}

bool MicroPythonEnvironment::EnsureGlobals() {
    if (m_Globals) {
        return true;
    }

    MicroPythonHost& host = MicroPythonHost::Instance();
    if (!host.IsInitialized()) {
        return false;
    }

    const int moduleId = host.AcquireModuleId();
    m_ModuleName = "__lup_inst_" + std::to_string(moduleId);

    MP_SETUP_STACK();
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        // A per-instance module gives the script its own globals dict and keeps it
        // GC-rooted (modules are held by sys.modules), with a unique name so two
        // instances never share a namespace. The id (and therefore the name and its
        // interned qstr) is recycled by Shutdown.
        qstr modQstr = safe_qstr_from_str(m_ModuleName.c_str());
        mp_obj_t module = mp_obj_new_module(modQstr);
        mp_obj_dict_t* modGlobals = mp_obj_module_get_globals(module);
        m_Globals = MP_OBJ_FROM_PTR(modGlobals);

        // Seed from __main__ so lupine, the component classes, the scheduler and
        // any already-bound singletons resolve from this instance's namespace.
        mp_obj_t shared = host.SharedGlobals();
        if (shared) {
            mp_obj_dict_t* src = (mp_obj_dict_t*)MP_OBJ_TO_PTR(shared);
            mp_map_t* map = &src->map;
            for (size_t i = 0; i < map->alloc; i++) {
                if (mp_map_slot_is_filled(map, i)) {
                    mp_obj_dict_store(m_Globals, map->table[i].key, map->table[i].value);
                }
            }
        }
        nlr_pop();
        m_ModuleId = moduleId;
        if (m_EnvId <= 0) {
            m_EnvId = s_NextEnvId++;
        }
        s_EnvById[m_EnvId] = this;
        return true;
    }

    host.ReleaseModuleId(moduleId);
    m_ModuleName.clear();
    m_Globals = nullptr;
    return false;
}

ScriptResult MicroPythonEnvironment::ExecuteFile(const std::string& filepath) {
    if (!m_Initialized) {
        return ScriptResult(false, "MicroPython environment not initialized");
    }

    std::string scriptContents;

    auto& packFS = platform::PackFileSystem::Instance();
    if (packFS.isPackMode() && packFS.exists(filepath)) {
        scriptContents = packFS.readFileAsString(filepath);
        if (scriptContents.empty()) {
            m_LastError = "Failed to read script from pack: " + filepath;
            return ScriptResult(false, m_LastError);
        }
    } else {
        auto result = platform::FileSystem::ReadFile(filepath);
        if (!result.success) {
            m_LastError = "Failed to read script file: " + filepath + " - " + result.error;
            return ScriptResult(false, m_LastError);
        }
        scriptContents = std::move(result.data);
    }

    return ExecuteString(scriptContents);
}

ScriptResult MicroPythonEnvironment::ExecuteString(const std::string& script) {
    if (!m_Initialized) {
        LOG_ERROR(LogCategory::Scripting, "MicroPython: ExecuteString called but not initialized");
        return ScriptResult(false, "MicroPython environment not initialized");
    }

    // Normalize line endings to Unix-style (LF only)
    // This is required for MicroPython on web builds where CRLF causes syntax errors
    std::string normalizedScript;
    normalizedScript.reserve(script.size());
    for (size_t i = 0; i < script.size(); ++i) {
        if (script[i] == '\r') {
            // Skip \r, but add \n if not followed by \n
            if (i + 1 < script.size() && script[i + 1] == '\n') {
                // \r\n -> \n (skip \r, next iteration will add \n)
                continue;
            } else {
                // Standalone \r -> \n
                normalizedScript += '\n';
            }
        } else {
            normalizedScript += script[i];
        }
    }

    // Debug: Log first 500 chars and line 9 specifically

    // Find and log line 9 specifically with hex dump for debugging
    {
        size_t lineNum = 1;
        size_t lineStart = 0;
        for (size_t i = 0; i < normalizedScript.size(); ++i) {
            if (normalizedScript[i] == '\n') {
                if (lineNum >= 7 && lineNum <= 11) {
                    std::string line = normalizedScript.substr(lineStart, i - lineStart);

                    // Hex dump of line for debugging hidden characters
                    if (lineNum == 9 || lineNum == 8) {
                        std::string hexDump;
                        for (size_t j = 0; j < line.size() && j < 50; ++j) {
                            char buf[8];
                            snprintf(buf, sizeof(buf), "%02X ", (unsigned char)line[j]);
                            hexDump += buf;
                        }
                        
                    }
                }
                lineNum++;
                lineStart = i + 1;
            }
        }

        // Also log the bytes around line 9's start position in the raw script
        if (normalizedScript.size() > 200) {
            std::string first200;
            for (size_t j = 0; j < 200 && j < normalizedScript.size(); ++j) {
                char buf[8];
                snprintf(buf, sizeof(buf), "%02X ", (unsigned char)normalizedScript[j]);
                first200 += buf;
            }
            
        }
    }

    if (!EnsureGlobals()) {
        return ScriptResult(false, "MicroPython instance namespace unavailable");
    }

    MP_SETUP_STACK();

    s_OutputBuffer.clear();

    // Run in this instance's module namespace: the compiled module function
    // captures the active globals at make-time, so top-level defs/vars land in
    // m_Globals, isolated from other scripts.
    MpDispatchScope scope(this);

    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        mp_lexer_t* lex = mp_lexer_new_from_str_len(MP_QSTR__lt_string_gt_, normalizedScript.c_str(), normalizedScript.length(), 0);
        qstr source_name = lex->source_name;
        mp_parse_tree_t parse_tree = mp_parse(lex, MP_PARSE_FILE_INPUT);
        mp_obj_t module_fun = mp_compile(&parse_tree, source_name, true);
        mp_call_function_0(module_fun);
        nlr_pop();
        return ScriptResult(true);
    } else {
        return HandleError(MP_OBJ_FROM_PTR(nlr.ret_val));
    }
}

ScriptResult MicroPythonEnvironment::CallFunction(const std::string& functionName) {
    if (!m_Initialized) {
        return ScriptResult(false, "MicroPython environment not initialized");
    }

    if (!HasFunction(functionName)) {
        return ScriptResult(true);  // Not an error if function doesn't exist
    }

    MP_SETUP_STACK();
    MpDispatchScope scope(this);

    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        qstr qst = safe_qstr_from_str(functionName.c_str());
        mp_obj_t func = mp_load_name(qst);
        if (mp_obj_is_callable(func)) {
            mp_call_function_0(func);
        }
        nlr_pop();
        return ScriptResult(true);
    } else {
        return HandleError(MP_OBJ_FROM_PTR(nlr.ret_val));
    }
}

// MpToJson can itself raise (mp_obj_get_int overflows on an int wider than a machine
// word, a container's own conversion can fail), and MicroPython unwinds with longjmp,
// which skips C++ destructors. The destination json therefore has to be owned by a
// frame the jump cannot skip - the caller's - while only the conversion runs inside
// the nlr scope. Returns false and reports the raised exception through outError.
static bool MpToJsonGuarded(mp_obj_t value, nlohmann::json& out, mp_obj_t& outError) {
    outError = MP_OBJ_NULL;

    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        out = MpToJson(value);
        nlr_pop();
        return true;
    }

    outError = MP_OBJ_FROM_PTR(nlr.ret_val);
    return false;
}

nlohmann::json MicroPythonEnvironment::CallMethod(const std::string& functionName,
                                                  const nlohmann::json& selfData,
                                                  const nlohmann::json& args) {
    if (!m_Initialized) {
        return nlohmann::json(nullptr);
    }

    MP_SETUP_STACK();
    MpDispatchScope scope(this);

    // Owned by this frame, outside every nlr scope below: a long-jump out of one of
    // those scopes would skip its destructor.
    nlohmann::json jsonResult(nullptr);
    mp_obj_t result = MP_OBJ_NULL;

    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        qstr qst = safe_qstr_from_str(functionName.c_str());
        mp_obj_t func = mp_load_name(qst);
        if (!mp_obj_is_callable(func)) {
            nlr_pop();
            return nlohmann::json(nullptr);
        }

        // The argument objects must be GC-reachable while the *next* argument is
        // converted: JsonToMp allocates, any allocation can trigger gc_collect, and
        // the collector only scans the VM heap, this C frame and the registers - not
        // the CRT heap. A std::vector's buffer lives on the CRT heap, so objects held
        // only by it were reclaimed mid-build and the call received dangling handles.
        // A MicroPython list keeps its items in GC memory and is itself rooted by
        // this frame's `argsList` local, so every argument stays reachable. It also
        // owns no C++ destructor, which an NLR long-jump out of this scope would skip.
        mp_obj_t argsList = mp_obj_new_list(0, nullptr);
        mp_obj_list_append(argsList, JsonToMp(selfData));
        if (args.is_array()) {
            for (const nlohmann::json& arg : args) {
                mp_obj_list_append(argsList, JsonToMp(arg));
            }
        }

        size_t argc = 0;
        mp_obj_t* argv = nullptr;
        mp_obj_list_get(argsList, &argc, &argv);

        result = mp_call_function_n_kw(func, argc, 0, argv);
        nlr_pop();
    } else {
        HandleError(MP_OBJ_FROM_PTR(nlr.ret_val));
        return nlohmann::json(nullptr);
    }

    mp_obj_t conversionError = MP_OBJ_NULL;
    if (!MpToJsonGuarded(result, jsonResult, conversionError)) {
        HandleError(conversionError);
        return nlohmann::json(nullptr);
    }

    return jsonResult;
}

bool MicroPythonEnvironment::CallFunctionArgs(const std::string& functionName,
                                              const nlohmann::json& args) {
    if (!m_Initialized) {
        return false;
    }

    MP_SETUP_STACK();
    MpDispatchScope scope(this);

    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        qstr qst = safe_qstr_from_str(functionName.c_str());
        mp_obj_t func = mp_load_name(qst);
        if (!mp_obj_is_callable(func)) {
            nlr_pop();
            return false;
        }

        // See CallMethod: the arguments are built into a GC-scanned list, never a
        // CRT-heap vector, so a collection triggered by a later conversion cannot
        // reclaim an already-converted argument. Nothing in this scope owns a C++
        // destructor either, so an NLR long-jump leaks nothing (the uuid is read as
        // a reference into the json rather than copied into a std::string).
        mp_obj_t argsList = mp_obj_new_list(0, nullptr);
        if (args.is_array()) {
            for (const nlohmann::json& arg : args) {
                // Deliver node-encoded args as node handles.
                if (arg.is_object() && m_ScriptAPI) {
                    nlohmann::json::const_iterator encoded = arg.find("__lupine_node__");
                    if (encoded != arg.end() && encoded->is_string()) {
                        const std::string& uuid = encoded->get_ref<const std::string&>();
                        mp_obj_list_append(argsList,
                                           wrap_node(m_ScriptAPI->FindNodeByUUID(uuid), m_ScriptAPI));
                        continue;
                    }
                }
                mp_obj_list_append(argsList, JsonToMp(arg));
            }
        }

        size_t argc = 0;
        mp_obj_t* argv = nullptr;
        mp_obj_list_get(argsList, &argc, &argv);

        mp_call_function_n_kw(func, argc, 0, argv);
        nlr_pop();
        return true;
    } else {
        HandleError(MP_OBJ_FROM_PTR(nlr.ret_val));
        return false;
    }
}

nlohmann::json MicroPythonEnvironment::CallFunctionResult(const std::string& functionName,
                                                          const nlohmann::json& args) {
    if (!m_Initialized) {
        return nlohmann::json(nullptr);
    }

    MP_SETUP_STACK();
    MpDispatchScope scope(this);

    // See CallMethod: owned by this frame so an NLR long-jump cannot skip its destructor.
    nlohmann::json converted(nullptr);
    mp_obj_t result = MP_OBJ_NULL;

    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        qstr qst = safe_qstr_from_str(functionName.c_str());
        mp_obj_t func = mp_load_name(qst);
        if (!mp_obj_is_callable(func)) {
            nlr_pop();
            return nlohmann::json(nullptr);
        }

        // See CallFunctionArgs: arguments are built into a GC-scanned list so a
        // collection triggered by a later conversion cannot reclaim an argument
        // that was already converted.
        mp_obj_t argsList = mp_obj_new_list(0, nullptr);
        if (args.is_array()) {
            for (const nlohmann::json& arg : args) {
                if (arg.is_object() && m_ScriptAPI) {
                    nlohmann::json::const_iterator encoded = arg.find("__lupine_node__");
                    if (encoded != arg.end() && encoded->is_string()) {
                        const std::string& uuid = encoded->get_ref<const std::string&>();
                        mp_obj_list_append(argsList,
                                           wrap_node(m_ScriptAPI->FindNodeByUUID(uuid), m_ScriptAPI));
                        continue;
                    }
                }
                mp_obj_list_append(argsList, JsonToMp(arg));
            }
        }

        size_t argc = 0;
        mp_obj_t* argv = nullptr;
        mp_obj_list_get(argsList, &argc, &argv);

        result = mp_call_function_n_kw(func, argc, 0, argv);
        nlr_pop();
    } else {
        HandleError(MP_OBJ_FROM_PTR(nlr.ret_val));
        return nlohmann::json(nullptr);
    }

    mp_obj_t conversionError = MP_OBJ_NULL;
    if (!MpToJsonGuarded(result, converted, conversionError)) {
        HandleError(conversionError);
        return nlohmann::json(nullptr);
    }

    return converted;
}

bool MicroPythonEnvironment::HasFunction(const std::string& functionName) const {
    if (!m_Initialized || !m_Globals) {
        return false;
    }

    MP_SETUP_STACK();
    MpDispatchScope scope(const_cast<MicroPythonEnvironment*>(this));

    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        qstr qst = safe_qstr_from_str(functionName.c_str());
        mp_obj_t obj = mp_load_name(qst);
        nlr_pop();
        return mp_obj_is_callable(obj);
    }

    return false;
}

// Bare-name variable writes (delta_time, export properties, ...) stay in this
// instance's namespace.
void MicroPythonEnvironment::SetGlobal(const std::string& name, int value) {
    if (!m_Initialized || !m_Globals) return;

    MP_SETUP_STACK();
    MpDispatchScope scope(this);

    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        mp_obj_t value_obj = mp_obj_new_int(value);
        qstr qst = safe_qstr_from_str(name.c_str());
        mp_store_name(qst, value_obj);
        nlr_pop();
    }
}

void MicroPythonEnvironment::SetGlobal(const std::string& name, float value) {
    if (!m_Initialized || !m_Globals) return;

    MP_SETUP_STACK();
    MpDispatchScope scope(this);

    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        mp_obj_t value_obj = mp_obj_new_float(value);
        qstr qst = safe_qstr_from_str(name.c_str());
        mp_store_name(qst, value_obj);
        nlr_pop();
    }
}

void MicroPythonEnvironment::SetGlobal(const std::string& name, const std::string& value) {
    if (!m_Initialized || !m_Globals) return;

    MP_SETUP_STACK();
    MpDispatchScope scope(this);

    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        mp_obj_t value_obj = mp_obj_new_str(value.c_str(), value.length());
        qstr qst = safe_qstr_from_str(name.c_str());
        mp_store_name(qst, value_obj);
        nlr_pop();
    }
}

void MicroPythonEnvironment::SetGlobal(const std::string& name, bool value) {
    if (!m_Initialized || !m_Globals) return;

    MP_SETUP_STACK();
    MpDispatchScope scope(this);

    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        mp_obj_t value_obj = value ? mp_const_true : mp_const_false;
        qstr qst = safe_qstr_from_str(name.c_str());
        mp_store_name(qst, value_obj);
        nlr_pop();
    }
}

// Autoload singletons bind into this instance's namespace by name. (MicroPython
// at this ROM level has no writable shared fallthrough namespace, so each
// instance is given the binding; the bound node object is the same live object.)
ScriptAPI* MicroPythonHost::SharedNodeApi(ScriptAPI* treeSource) {
    if (treeSource) {
        m_SharedNodeApi.SetSceneManager(treeSource->GetTree());
    }
    return &m_SharedNodeApi;
}

void MicroPythonEnvironment::SetGlobalNode(const std::string& name, core::Node* node) {
    if (!m_Initialized || !m_Globals || !m_ScriptAPI) return;

    MP_SETUP_STACK();
    MpDispatchScope scope(this);

    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        mp_obj_t value_obj = wrap_node(node, MicroPythonHost::Instance().SharedNodeApi(m_ScriptAPI));
        qstr qst = safe_qstr_from_str(name.c_str());
        mp_store_name(qst, value_obj);

        // Also propagate into __main__ (the seed source) so script instances
        // created later inherit the same singleton by name.
        mp_obj_t shared = MicroPythonHost::Instance().SharedGlobals();
        if (shared) {
            mp_obj_dict_store(shared, MP_OBJ_NEW_QSTR(qst), value_obj);
        }
        nlr_pop();
    }
}

int MicroPythonEnvironment::GetGlobalInt(const std::string& name, int defaultValue) {
    if (!m_Initialized || !m_Globals) return defaultValue;

    MP_SETUP_STACK();
    MpDispatchScope scope(this);

    // The conversion has to stay inside the NLR scope: mp_obj_get_int raises
    // OverflowError for a value that does not fit a machine int (a script storing
    // x = 2**80), and an exception with no handler aborts the process.
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        qstr qst = safe_qstr_from_str(name.c_str());
        mp_obj_t obj = mp_load_name(qst);
        int value = mp_obj_is_int(obj) ? mp_obj_get_int(obj) : defaultValue;
        nlr_pop();
        return value;
    }

    return defaultValue;
}

float MicroPythonEnvironment::GetGlobalFloat(const std::string& name, float defaultValue) {
    if (!m_Initialized || !m_Globals) return defaultValue;

    MP_SETUP_STACK();
    MpDispatchScope scope(this);

    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        qstr qst = safe_qstr_from_str(name.c_str());
        mp_obj_t obj = mp_load_name(qst);
        float value = defaultValue;
        if (mp_obj_is_float(obj)) {
            value = static_cast<float>(mp_obj_get_float(obj));
        } else if (mp_obj_is_int(obj)) {
            value = static_cast<float>(mp_obj_get_float(obj));
        }
        nlr_pop();
        return value;
    }

    return defaultValue;
}

std::string MicroPythonEnvironment::GetGlobalString(const std::string& name, const std::string& defaultValue) {
    if (!m_Initialized || !m_Globals) return defaultValue;

    MP_SETUP_STACK();
    MpDispatchScope scope(this);

    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        qstr qst = safe_qstr_from_str(name.c_str());
        mp_obj_t obj = mp_load_name(qst);
        const char* str = nullptr;
        size_t len = 0;
        if (mp_obj_is_str(obj)) {
            str = mp_obj_str_get_data(obj, &len);
        }
        nlr_pop();

        if (str) {
            return std::string(str, len);
        }
    }

    return defaultValue;
}

bool MicroPythonEnvironment::GetGlobalBool(const std::string& name, bool defaultValue) {
    if (!m_Initialized || !m_Globals) return defaultValue;

    MP_SETUP_STACK();
    MpDispatchScope scope(this);

    // mp_obj_is_true runs the object's __bool__/__len__, which can raise, so it
    // must stay inside the NLR scope.
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        qstr qst = safe_qstr_from_str(name.c_str());
        mp_obj_t obj = mp_load_name(qst);
        bool value = mp_obj_is_true(obj);
        nlr_pop();
        return value;
    }

    return defaultValue;
}

void MicroPythonEnvironment::SetGlobalJson(const std::string& name, const nlohmann::json& value) {
    if (!m_Initialized || !m_Globals) return;

    MP_SETUP_STACK();
    MpDispatchScope scope(this);

    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        mp_obj_t value_obj = JsonToMp(value);
        qstr qst = safe_qstr_from_str(name.c_str());
        mp_store_name(qst, value_obj);
        nlr_pop();
    }
}

nlohmann::json MicroPythonEnvironment::GetGlobalJson(const std::string& name,
                                                     const nlohmann::json& defaultValue) {
    if (!m_Initialized || !m_Globals) return defaultValue;

    MP_SETUP_STACK();
    MpDispatchScope scope(this);

    // See CallMethod: the json is owned by this frame, outside the nlr scopes, because
    // a long-jump out of one of them would skip its destructor.
    nlohmann::json result(nullptr);
    mp_obj_t obj = MP_OBJ_NULL;

    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        qstr qst = safe_qstr_from_str(name.c_str());
        obj = mp_load_name(qst);
        nlr_pop();
    } else {
        return defaultValue;
    }

    mp_obj_t conversionError = MP_OBJ_NULL;
    if (!MpToJsonGuarded(obj, result, conversionError)) {
        return defaultValue;
    }

    return result;
}

void MicroPythonEnvironment::RunGC() {
    if (m_Initialized) {
        MP_SETUP_STACK();
        gc_collect();
    }
}

void MicroPythonEnvironment::SetScriptAPI(ScriptAPI* api) {
    // The API is made active per-dispatch via MpDispatchScope (s_CurrentEnv), so
    // this only records which API this instance dispatches with.
    m_ScriptAPI = api;
}

ScriptResult MicroPythonEnvironment::HandleError(mp_obj_t exc) {
    s_OutputBuffer.clear();

    if (exc != MP_OBJ_NULL) {
        mp_obj_print_exception(&mp_plat_print, exc);
        m_LastError = s_OutputBuffer;
    } else {
        m_LastError = "Unknown MicroPython error";
    }

    LOG_ERROR(LogCategory::Scripting, "MicroPython error: {}", m_LastError);
    return ScriptResult(false, m_LastError);
}

} // namespace scripting
} // namespace lupine

#endif // LUPINE_HAS_MICROPYTHON
