
#include "input/lc_input.h"
#include "../core/lc_internal.h"

#include <lupine/input/InputManager.hpp>

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace {

void SetInputError(LCResult code, const char* message) {
    ::SetError(code, message);

// Convert LC key codes to engine key codes
}
lupine::input::KeyCode ToKeyCode(LCKeyCode key) {
    return static_cast<lupine::input::KeyCode>(static_cast<uint16_t>(key));

// Convert LC mouse buttons to engine mouse buttons
}
lupine::input::MouseButton ToMouseButton(LCMouseButton button) {
    return static_cast<lupine::input::MouseButton>(static_cast<uint8_t>(button));

// Convert LC gamepad buttons to engine gamepad buttons
}
lupine::input::GamepadButton ToGamepadButton(LCGamepadButton button) {
    return static_cast<lupine::input::GamepadButton>(static_cast<uint8_t>(button));

// Convert LC gamepad axes to engine gamepad axes
}
lupine::input::GamepadAxis ToGamepadAxis(LCGamepadAxis axis) {
    return static_cast<lupine::input::GamepadAxis>(static_cast<uint8_t>(axis));

}
} // anonymous namespace


/* ============================================================================
 * Keyboard Input
 * ============================================================================ */

LC_API bool lc_input_is_key_pressed(LCKeyCode key) {
    try {
        return lupine::input::InputManager::Get().IsKeyPressed(ToKeyCode(key));
    } catch (...) {
        return false;
    }

}
LC_API bool lc_input_is_key_just_pressed(LCKeyCode key) {
    try {
        return lupine::input::InputManager::Get().IsKeyJustPressed(ToKeyCode(key));
    } catch (...) {
        return false;
    }

}
LC_API bool lc_input_is_key_just_released(LCKeyCode key) {
    try {
        return lupine::input::InputManager::Get().IsKeyJustReleased(ToKeyCode(key));
    } catch (...) {
        return false;
    }

/* ============================================================================
 * Mouse Input
 * ============================================================================ */

}
LC_API bool lc_input_is_mouse_button_pressed(LCMouseButton button) {
    try {
        return lupine::input::InputManager::Get().IsMouseButtonPressed(ToMouseButton(button));
    } catch (...) {
        return false;
    }

}
LC_API bool lc_input_is_mouse_button_just_pressed(LCMouseButton button) {
    try {
        return lupine::input::InputManager::Get().IsMouseButtonJustPressed(ToMouseButton(button));
    } catch (...) {
        return false;
    }

}
LC_API bool lc_input_is_mouse_button_just_released(LCMouseButton button) {
    try {
        return lupine::input::InputManager::Get().IsMouseButtonJustReleased(ToMouseButton(button));
    } catch (...) {
        return false;
    }

}
LC_API LCResult lc_input_get_mouse_position(LCVec2* out_position) {
    if (!out_position) {
        SetInputError(LC_ERROR_NULL_POINTER, "out_position is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto pos = lupine::input::InputManager::Get().GetMousePosition();
        out_position->x = pos.x;
        out_position->y = pos.y;
        return LC_SUCCESS;
    } catch (...) {
        SetInputError(LC_ERROR_INTERNAL_ERROR, "Failed to get mouse position");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_input_get_mouse_delta(LCVec2* out_delta) {
    if (!out_delta) {
        SetInputError(LC_ERROR_NULL_POINTER, "out_delta is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto delta = lupine::input::InputManager::Get().GetMouseDelta();
        out_delta->x = delta.x;
        out_delta->y = delta.y;
        return LC_SUCCESS;
    } catch (...) {
        SetInputError(LC_ERROR_INTERNAL_ERROR, "Failed to get mouse delta");
        return LC_ERROR_INTERNAL_ERROR;
    }

}
LC_API LCResult lc_input_get_mouse_scroll_delta(LCVec2* out_delta) {
    if (!out_delta) {
        SetInputError(LC_ERROR_NULL_POINTER, "out_delta is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto delta = lupine::input::InputManager::Get().GetMouseScrollDelta();
        out_delta->x = delta.x;
        out_delta->y = delta.y;
        return LC_SUCCESS;
    } catch (...) {
        SetInputError(LC_ERROR_INTERNAL_ERROR, "Failed to get mouse scroll delta");
        return LC_ERROR_INTERNAL_ERROR;
    }

/* ============================================================================
 * Gamepad Input
 * ============================================================================ */

}
LC_API bool lc_input_is_gamepad_button_pressed(LCGamepadButton button, uint32_t gamepad_id) {
    try {
        return lupine::input::InputManager::Get().IsGamepadButtonPressed(ToGamepadButton(button), gamepad_id);
    } catch (...) {
        return false;
    }

}
LC_API bool lc_input_is_gamepad_button_just_pressed(LCGamepadButton button, uint32_t gamepad_id) {
    try {
        return lupine::input::InputManager::Get().IsGamepadButtonJustPressed(ToGamepadButton(button), gamepad_id);
    } catch (...) {
        return false;
    }

}
LC_API bool lc_input_is_gamepad_button_just_released(LCGamepadButton button, uint32_t gamepad_id) {
    try {
        return lupine::input::InputManager::Get().IsGamepadButtonJustReleased(ToGamepadButton(button), gamepad_id);
    } catch (...) {
        return false;
    }

}
LC_API float lc_input_get_gamepad_axis(LCGamepadAxis axis, uint32_t gamepad_id) {
    try {
        return lupine::input::InputManager::Get().GetGamepadAxis(ToGamepadAxis(axis), gamepad_id);
    } catch (...) {
        return 0.0f;
    }

/* ============================================================================
 * Input Actions (Mapped Inputs)
 * ============================================================================ */

}
LC_API bool lc_input_is_action_pressed(const char* action_name) {
    if (!action_name) return false;

    try {
        return lupine::input::InputManager::Get().IsActionPressed(std::string(action_name));
    } catch (...) {
        return false;
    }

}
LC_API bool lc_input_is_action_just_pressed(const char* action_name) {
    if (!action_name) return false;

    try {
        return lupine::input::InputManager::Get().IsActionJustPressed(std::string(action_name));
    } catch (...) {
        return false;
    }

}
LC_API bool lc_input_is_action_just_released(const char* action_name) {
    if (!action_name) return false;

    try {
        return lupine::input::InputManager::Get().IsActionJustReleased(std::string(action_name));
    } catch (...) {
        return false;
    }

}
LC_API float lc_input_get_axis(const char* axis_name) {
    if (!axis_name) return 0.0f;

    try {
        return lupine::input::InputManager::Get().GetAxisValue(std::string(axis_name));
    } catch (...) {
        return 0.0f;
    }


}

LC_API void lc_input_set_gamepad_deadzone(float deadzone) {
    try {
        lupine::input::InputManager::Get().SetGlobalGamepadDeadzone(deadzone);
    } catch (...) {
    }
}

LC_API float lc_input_get_gamepad_deadzone(void) {
    try {
        return lupine::input::InputManager::Get().GetGlobalGamepadDeadzone();
    } catch (...) {
        return 0.0f;
    }
}

/* ============================================================================
 * Touch Input
 * ============================================================================ */

LC_API bool lc_input_is_touch_available(void) {
    try {
        return lupine::input::InputManager::Get().IsTouchAvailable();
    } catch (...) {
        return false;
    }
}

LC_API bool lc_input_is_touching(void) {
    try {
        return lupine::input::InputManager::Get().IsTouching();
    } catch (...) {
        return false;
    }
}

LC_API uint32_t lc_input_get_touch_count(void) {
    try {
        return static_cast<uint32_t>(lupine::input::InputManager::Get().GetTouchCount());
    } catch (...) {
        return 0u;
    }
}

LC_API LCResult lc_input_get_touch_position(uint32_t index, LCVec2* out_position) {
    if (!out_position) {
        SetInputError(LC_ERROR_NULL_POINTER, "out_position is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto pos = lupine::input::InputManager::Get().GetTouchPosition(static_cast<size_t>(index));
        out_position->x = pos.x;
        out_position->y = pos.y;
        return LC_SUCCESS;
    } catch (...) {
        SetInputError(LC_ERROR_INTERNAL_ERROR, "Failed to get touch position");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API bool lc_input_is_touch_just_started(void) {
    try {
        return lupine::input::InputManager::Get().IsTouchJustStarted();
    } catch (...) {
        return false;
    }
}

LC_API bool lc_input_is_touch_just_ended(void) {
    try {
        return lupine::input::InputManager::Get().IsTouchJustEnded();
    } catch (...) {
        return false;
    }
}

/* ============================================================================
 * Clipboard
 * ============================================================================ */

LC_API LCResult lc_input_get_clipboard(char* out_buffer, size_t buffer_size) {
    if (!out_buffer) {
        SetInputError(LC_ERROR_NULL_POINTER, "out_buffer is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    if (buffer_size == 0) {
        SetInputError(LC_ERROR_INVALID_PARAMETER, "buffer_size is 0");
        return LC_ERROR_INVALID_PARAMETER;
    }

    try {
        std::string text = lupine::input::InputManager::Get().GetClipboardText();
        CopyStringToBuffer(out_buffer, buffer_size, text.c_str());
        return LC_SUCCESS;
    } catch (...) {
        SetInputError(LC_ERROR_INTERNAL_ERROR, "Failed to get clipboard text");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_input_set_clipboard(const char* text) {
    if (!text) {
        SetInputError(LC_ERROR_NULL_POINTER, "text is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        lupine::input::InputManager::Get().SetClipboardText(std::string(text));
        return LC_SUCCESS;
    } catch (...) {
        SetInputError(LC_ERROR_INTERNAL_ERROR, "Failed to set clipboard text");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Helpers for new bindings
 * ============================================================================ */

namespace {

void FillBinding(const lupine::input::InputBinding& b, LCInputBinding* out) {
    out->device_type = static_cast<int>(b.deviceType);
    out->key_code = static_cast<int>(b.keyCode);
    out->mouse_button = static_cast<int>(b.mouseButton);
    out->gamepad_button = static_cast<int>(b.gamepadButton);
    out->gamepad_axis = static_cast<int>(b.gamepadAxis);
    out->gamepad_id = b.gamepadID;
    out->scale = b.scale;
}

void CopyField(char* dst, size_t cap, const std::string& src) {
    CopyStringToBuffer(dst, cap, src.c_str());
}

} // anonymous namespace

/* ============================================================================
 * Per-player action/axis queries
 * ============================================================================ */

LC_API bool lc_input_is_action_pressed_for_player(const char* action_name, int player) {
    if (!action_name) return false;
    try { return lupine::input::InputManager::Get().IsActionPressed(std::string(action_name), player); }
    catch (...) { return false; }
}

LC_API bool lc_input_is_action_just_pressed_for_player(const char* action_name, int player) {
    if (!action_name) return false;
    try { return lupine::input::InputManager::Get().IsActionJustPressed(std::string(action_name), player); }
    catch (...) { return false; }
}

LC_API bool lc_input_is_action_just_released_for_player(const char* action_name, int player) {
    if (!action_name) return false;
    try { return lupine::input::InputManager::Get().IsActionJustReleased(std::string(action_name), player); }
    catch (...) { return false; }
}

LC_API float lc_input_get_axis_for_player(const char* axis_name, int player) {
    if (!axis_name) return 0.0f;
    try { return lupine::input::InputManager::Get().GetAxisValue(std::string(axis_name), player); }
    catch (...) { return 0.0f; }
}

LC_API float lc_input_get_action_strength_for_player(const char* action_name, int player) {
    if (!action_name) return 0.0f;
    try { return lupine::input::InputManager::Get().GetActionStrength(std::string(action_name), player); }
    catch (...) { return 0.0f; }
}

/* ============================================================================
 * Active device detection
 * ============================================================================ */

LC_API LCInputDeviceType lc_input_get_active_device_type(void) {
    try { return static_cast<LCInputDeviceType>(lupine::input::InputManager::Get().GetLastUsedDeviceType()); }
    catch (...) { return LC_INPUT_DEVICE_UNKNOWN; }
}

LC_API uint32_t lc_input_get_last_gamepad_id(void) {
    try { return lupine::input::InputManager::Get().GetLastUsedGamepadID(); }
    catch (...) { return 0u; }
}

LC_API LCGamepadType lc_input_get_gamepad_type(uint32_t gamepad_id) {
    try { return static_cast<LCGamepadType>(lupine::input::InputManager::Get().GetGamepadType(gamepad_id)); }
    catch (...) { return LC_GAMEPAD_TYPE_UNKNOWN; }
}

/* ============================================================================
 * Input contexts / action sets
 * ============================================================================ */

LC_API void lc_input_enable_context(const char* context) {
    if (!context) return;
    try { lupine::input::InputManager::Get().EnableContext(context); } catch (...) {}
}

LC_API void lc_input_disable_context(const char* context) {
    if (!context) return;
    try { lupine::input::InputManager::Get().DisableContext(context); } catch (...) {}
}

LC_API void lc_input_set_context_active(const char* context, bool active) {
    if (!context) return;
    try { lupine::input::InputManager::Get().SetContextActive(context, active); } catch (...) {}
}

LC_API bool lc_input_is_context_active(const char* context) {
    if (!context) return false;
    try { return lupine::input::InputManager::Get().IsContextActive(context); } catch (...) { return false; }
}

LC_API void lc_input_set_exclusive_context(const char* context) {
    try { lupine::input::InputManager::Get().SetExclusiveContext(context ? context : ""); } catch (...) {}
}

LC_API void lc_input_set_action_enabled(const char* action_name, bool enabled) {
    if (!action_name) return;
    try { lupine::input::InputManager::Get().SetActionEnabled(action_name, enabled); } catch (...) {}
}

LC_API void lc_input_set_axis_enabled(const char* axis_name, bool enabled) {
    if (!axis_name) return;
    try { lupine::input::InputManager::Get().SetAxisEnabled(axis_name, enabled); } catch (...) {}
}

/* ============================================================================
 * Local multiplayer player slots
 * ============================================================================ */

LC_API void lc_input_set_player_count(int count) {
    try { lupine::input::InputManager::Get().SetPlayerCount(count); } catch (...) {}
}

LC_API int lc_input_get_player_count(void) {
    try { return lupine::input::InputManager::Get().GetPlayerCount(); } catch (...) { return 0; }
}

LC_API void lc_input_clear_player_assignments(void) {
    try { lupine::input::InputManager::Get().ClearPlayerAssignments(); } catch (...) {}
}

LC_API void lc_input_assign_keyboard_mouse_to_player(int player) {
    try { lupine::input::InputManager::Get().AssignKeyboardMouseToPlayer(player); } catch (...) {}
}

LC_API void lc_input_assign_gamepad_to_player(int player, uint32_t gamepad_id) {
    try { lupine::input::InputManager::Get().AssignGamepadToPlayer(player, gamepad_id); } catch (...) {}
}

LC_API void lc_input_unassign_gamepad(uint32_t gamepad_id) {
    try { lupine::input::InputManager::Get().UnassignGamepad(gamepad_id); } catch (...) {}
}

LC_API int lc_input_get_player_for_gamepad(uint32_t gamepad_id) {
    try { return lupine::input::InputManager::Get().GetPlayerForGamepad(gamepad_id); } catch (...) { return -1; }
}

LC_API int lc_input_get_player_for_keyboard_mouse(void) {
    try { return lupine::input::InputManager::Get().GetPlayerForKeyboardMouse(); } catch (...) { return -1; }
}

LC_API bool lc_input_player_owns_keyboard_mouse(int player) {
    try { return lupine::input::InputManager::Get().PlayerOwnsKeyboardMouse(player); } catch (...) { return false; }
}

LC_API void lc_input_set_auto_join_enabled(bool enabled) {
    try { lupine::input::InputManager::Get().SetAutoJoinEnabled(enabled); } catch (...) {}
}

LC_API bool lc_input_is_auto_join_enabled(void) {
    try { return lupine::input::InputManager::Get().IsAutoJoinEnabled(); } catch (...) { return false; }
}

/* ============================================================================
 * Runtime rebinding
 * ============================================================================ */

LC_API void lc_input_add_action_key(const char* action_name, LCKeyCode key) {
    if (!action_name) return;
    try {
        lupine::input::InputManager::Get().AddBindingToAction(
            action_name, lupine::input::InputBinding::FromKey(ToKeyCode(key)));
    } catch (...) {}
}

LC_API void lc_input_add_action_mouse_button(const char* action_name, LCMouseButton button) {
    if (!action_name) return;
    try {
        lupine::input::InputManager::Get().AddBindingToAction(
            action_name, lupine::input::InputBinding::FromMouseButton(ToMouseButton(button)));
    } catch (...) {}
}

LC_API void lc_input_add_action_gamepad_button(const char* action_name, LCGamepadButton button, uint32_t gamepad_id) {
    if (!action_name) return;
    try {
        lupine::input::InputManager::Get().AddBindingToAction(
            action_name, lupine::input::InputBinding::FromGamepadButton(ToGamepadButton(button), gamepad_id));
    } catch (...) {}
}

LC_API void lc_input_add_action_gamepad_axis(const char* action_name, LCGamepadAxis axis, float scale, uint32_t gamepad_id) {
    if (!action_name) return;
    try {
        lupine::input::InputManager::Get().AddBindingToAction(
            action_name, lupine::input::InputBinding::FromGamepadAxis(ToGamepadAxis(axis), scale, gamepad_id));
    } catch (...) {}
}

LC_API void lc_input_remove_action_binding(const char* action_name, int index) {
    if (!action_name || index < 0) return;
    try { lupine::input::InputManager::Get().RemoveBindingFromAction(action_name, static_cast<size_t>(index)); } catch (...) {}
}

LC_API void lc_input_clear_action_bindings(const char* action_name) {
    if (!action_name) return;
    try { lupine::input::InputManager::Get().ClearActionBindings(action_name); } catch (...) {}
}

LC_API int lc_input_get_action_binding_count(const char* action_name) {
    if (!action_name) return 0;
    try { return static_cast<int>(lupine::input::InputManager::Get().GetActionBindings(action_name).size()); }
    catch (...) { return 0; }
}

LC_API LCResult lc_input_get_action_binding(const char* action_name, int index, LCInputBinding* out_binding) {
    if (!action_name || !out_binding) {
        SetInputError(LC_ERROR_NULL_POINTER, "action_name or out_binding is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto bindings = lupine::input::InputManager::Get().GetActionBindings(action_name);
        if (index < 0 || index >= static_cast<int>(bindings.size())) {
            SetInputError(LC_ERROR_INVALID_PARAMETER, "binding index out of range");
            return LC_ERROR_INVALID_PARAMETER;
        }
        FillBinding(bindings[static_cast<size_t>(index)], out_binding);
        return LC_SUCCESS;
    } catch (...) {
        SetInputError(LC_ERROR_INTERNAL_ERROR, "Failed to get action binding");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API void lc_input_add_axis_key(const char* axis_name, LCKeyCode key, float scale) {
    if (!axis_name) return;
    try {
        lupine::input::InputBinding b = lupine::input::InputBinding::FromKey(ToKeyCode(key));
        b.scale = scale;
        lupine::input::InputManager::Get().AddBindingToAxis(axis_name, b);
    } catch (...) {}
}

LC_API void lc_input_add_axis_gamepad_axis(const char* axis_name, LCGamepadAxis axis, float scale, uint32_t gamepad_id) {
    if (!axis_name) return;
    try {
        lupine::input::InputManager::Get().AddBindingToAxis(
            axis_name, lupine::input::InputBinding::FromGamepadAxis(ToGamepadAxis(axis), scale, gamepad_id));
    } catch (...) {}
}

LC_API void lc_input_remove_axis_binding(const char* axis_name, int index) {
    if (!axis_name || index < 0) return;
    try { lupine::input::InputManager::Get().RemoveBindingFromAxis(axis_name, static_cast<size_t>(index)); } catch (...) {}
}

LC_API void lc_input_clear_axis_bindings(const char* axis_name) {
    if (!axis_name) return;
    try { lupine::input::InputManager::Get().ClearAxisBindings(axis_name); } catch (...) {}
}

LC_API int lc_input_get_axis_binding_count(const char* axis_name) {
    if (!axis_name) return 0;
    try { return static_cast<int>(lupine::input::InputManager::Get().GetAxisBindings(axis_name).size()); }
    catch (...) { return 0; }
}

LC_API LCResult lc_input_get_axis_binding(const char* axis_name, int index, LCInputBinding* out_binding) {
    if (!axis_name || !out_binding) {
        SetInputError(LC_ERROR_NULL_POINTER, "axis_name or out_binding is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto bindings = lupine::input::InputManager::Get().GetAxisBindings(axis_name);
        if (index < 0 || index >= static_cast<int>(bindings.size())) {
            SetInputError(LC_ERROR_INVALID_PARAMETER, "binding index out of range");
            return LC_ERROR_INVALID_PARAMETER;
        }
        FillBinding(bindings[static_cast<size_t>(index)], out_binding);
        return LC_SUCCESS;
    } catch (...) {
        SetInputError(LC_ERROR_INTERNAL_ERROR, "Failed to get axis binding");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API bool lc_input_save_input_map(const char* filepath) {
    if (!filepath) return false;
    try { return lupine::input::InputManager::Get().SaveInputMap(filepath); } catch (...) { return false; }
}

LC_API bool lc_input_load_input_map(const char* filepath) {
    if (!filepath) return false;
    try { return lupine::input::InputManager::Get().LoadInputMap(filepath); } catch (...) { return false; }
}

/* ============================================================================
 * Input capture (rebind menus)
 * ============================================================================ */

LC_API void lc_input_start_input_capture(uint32_t device_mask) {
    try {
        if (device_mask == 0) device_mask = LC_CAPTURE_ALL;
        lupine::input::InputManager::Get().StartInputCapture(device_mask);
    } catch (...) {}
}

LC_API void lc_input_cancel_input_capture(void) {
    try { lupine::input::InputManager::Get().CancelInputCapture(); } catch (...) {}
}

LC_API bool lc_input_is_capturing(void) {
    try { return lupine::input::InputManager::Get().IsCapturing(); } catch (...) { return false; }
}

LC_API bool lc_input_is_capture_complete(void) {
    try { return lupine::input::InputManager::Get().IsCaptureComplete(); } catch (...) { return false; }
}

LC_API LCResult lc_input_get_captured_binding(LCInputBinding* out_binding) {
    if (!out_binding) {
        SetInputError(LC_ERROR_NULL_POINTER, "out_binding is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        auto& mgr = lupine::input::InputManager::Get();
        if (!mgr.IsCaptureComplete()) {
            SetInputError(LC_ERROR_INVALID_PARAMETER, "no captured binding available");
            return LC_ERROR_INVALID_PARAMETER;
        }
        FillBinding(mgr.GetCapturedBinding(), out_binding);
        return LC_SUCCESS;
    } catch (...) {
        SetInputError(LC_ERROR_INTERNAL_ERROR, "Failed to get captured binding");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API void lc_input_clear_captured_binding(void) {
    try { lupine::input::InputManager::Get().ClearCapturedBinding(); } catch (...) {}
}

LC_API void lc_input_apply_captured_binding_to_action(const char* action_name) {
    if (!action_name) return;
    try {
        auto& mgr = lupine::input::InputManager::Get();
        if (mgr.IsCaptureComplete()) {
            mgr.AddBindingToAction(action_name, mgr.GetCapturedBinding());
            mgr.ClearCapturedBinding();
        }
    } catch (...) {}
}

/* ============================================================================
 * Glyph / prompt resolution
 * ============================================================================ */

LC_API LCResult lc_input_get_action_glyph(const char* action_name, int player,
                                          int device_override, LCInputGlyph* out_glyph) {
    if (!action_name || !out_glyph) {
        SetInputError(LC_ERROR_NULL_POINTER, "action_name or out_glyph is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        lupine::input::InputDeviceType dev = device_override < 0
            ? lupine::input::InputDeviceType::Unknown
            : static_cast<lupine::input::InputDeviceType>(device_override);
        lupine::input::InputGlyph g = lupine::input::InputManager::Get().GetActionGlyph(action_name, player, dev);
        CopyField(out_glyph->glyph_id, sizeof(out_glyph->glyph_id), g.glyphId);
        CopyField(out_glyph->label, sizeof(out_glyph->label), g.label);
        CopyField(out_glyph->art_path, sizeof(out_glyph->art_path), g.artPath);
        out_glyph->device = static_cast<int>(g.device);
        out_glyph->gamepad_type = static_cast<int>(g.gamepadType);
        return LC_SUCCESS;
    } catch (...) {
        SetInputError(LC_ERROR_INTERNAL_ERROR, "Failed to get action glyph");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API void lc_input_set_glyph_label(const char* glyph_id, const char* label) {
    if (!glyph_id || !label) return;
    try { lupine::input::InputManager::Get().SetGlyphLabel(glyph_id, label); } catch (...) {}
}

LC_API void lc_input_set_glyph_art(const char* glyph_id, const char* art_path) {
    if (!glyph_id || !art_path) return;
    try { lupine::input::InputManager::Get().SetGlyphArt(glyph_id, art_path); } catch (...) {}
}

LC_API void lc_input_clear_glyph_override(const char* glyph_id) {
    if (!glyph_id) return;
    try { lupine::input::InputManager::Get().ClearGlyphOverride(glyph_id); } catch (...) {}
}

LC_API void lc_input_clear_glyph_overrides(void) {
    try { lupine::input::InputManager::Get().ClearGlyphOverrides(); } catch (...) {}
}

LC_API bool lc_input_load_glyph_map(const char* filepath) {
    if (!filepath) return false;
    try { return lupine::input::InputManager::Get().LoadGlyphMap(filepath); } catch (...) { return false; }
}

LC_API bool lc_input_save_glyph_map(const char* filepath) {
    if (!filepath) return false;
    try { return lupine::input::InputManager::Get().SaveGlyphMap(filepath); } catch (...) { return false; }
}

/* ============================================================================
 * Action delegation (native callbacks)
 * ============================================================================ */

LC_API void lc_input_register_action_callback(const char* action_name,
                                              LCInputActionCallback callback, void* user_data) {
    if (!action_name || !callback) return;
    try {
        lupine::input::InputManager::Get().RegisterActionCallback(
            action_name,
            [callback, user_data](const std::string& name, bool pressed) {
                callback(name.c_str(), pressed, user_data);
            });
    } catch (...) {}
}

LC_API void lc_input_unregister_action_callback(const char* action_name) {
    if (!action_name) return;
    try { lupine::input::InputManager::Get().UnregisterActionCallback(action_name); } catch (...) {}
}

/* ============================================================================
 * Action strength / vector
 * ============================================================================ */

namespace {

LCResult WriteJsonToBuffer(const nlohmann::json& value, char* out_buffer, size_t buffer_size) {
    if (!out_buffer || buffer_size == 0) {
        SetInputError(LC_ERROR_NULL_POINTER, "out_buffer is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    const std::string dumped = value.dump();
    if (dumped.size() + 1 > buffer_size) {
        SetInputError(LC_ERROR_INVALID_PARAMETER, "Buffer too small for JSON result");
        return LC_ERROR_INVALID_PARAMETER;
    }
    std::memcpy(out_buffer, dumped.c_str(), dumped.size() + 1);
    return LC_SUCCESS;
}

nlohmann::json GlyphToJson(const lupine::input::InputGlyph& g) {
    nlohmann::json j;
    j["glyph_id"] = g.glyphId;
    j["label"] = g.label;
    j["art_path"] = g.artPath;
    j["device"] = static_cast<int>(g.device);
    j["gamepad_type"] = static_cast<int>(g.gamepadType);
    return j;
}

bool ParseEventJson(const char* event_json, nlohmann::json& out) {
    if (!event_json) return false;
    try {
        out = nlohmann::json::parse(event_json);
        return out.is_object();
    } catch (...) {
        return false;
    }
}

bool EventMatchesAction(const nlohmann::json& event, const std::string& action) {
    if (!event.is_object() || !event.contains("type")) {
        return false;
    }
    const std::string type = event.value("type", std::string());

    lupine::input::InputBinding eb;
    if (type == "key_down" || type == "key_up") {
        eb.deviceType = lupine::input::InputDeviceType::Keyboard;
        eb.keyCode = static_cast<lupine::input::KeyCode>(event.value("key", 0));
    } else if (type == "mouse_button_down" || type == "mouse_button_up") {
        eb.deviceType = lupine::input::InputDeviceType::Mouse;
        eb.mouseButton = static_cast<lupine::input::MouseButton>(event.value("button", 255));
    } else if (type == "gamepad_button_down" || type == "gamepad_button_up") {
        eb.deviceType = lupine::input::InputDeviceType::Gamepad;
        eb.gamepadButton = static_cast<lupine::input::GamepadButton>(event.value("button", 255));
        eb.gamepadID = static_cast<uint32_t>(event.value("gamepad_id", 0));
    } else if (type == "gamepad_axis") {
        eb.deviceType = lupine::input::InputDeviceType::Gamepad;
        eb.gamepadAxis = static_cast<lupine::input::GamepadAxis>(event.value("axis", 255));
        eb.gamepadID = static_cast<uint32_t>(event.value("gamepad_id", 0));
    } else {
        return false;
    }

    for (const auto& b : lupine::input::InputManager::Get().GetActionBindings(action)) {
        if (b.deviceType != eb.deviceType) {
            continue;
        }
        switch (b.deviceType) {
            case lupine::input::InputDeviceType::Keyboard:
                if (b.keyCode == eb.keyCode) return true;
                break;
            case lupine::input::InputDeviceType::Mouse:
                if (b.mouseButton == eb.mouseButton) return true;
                break;
            case lupine::input::InputDeviceType::Gamepad:
                if (b.gamepadButton != lupine::input::GamepadButton::Unknown) {
                    if (b.gamepadButton == eb.gamepadButton &&
                        (b.gamepadID == 0 || b.gamepadID == eb.gamepadID)) {
                        return true;
                    }
                } else if (b.gamepadAxis != lupine::input::GamepadAxis::Unknown) {
                    if (b.gamepadAxis == eb.gamepadAxis &&
                        (b.gamepadID == 0 || b.gamepadID == eb.gamepadID)) {
                        return true;
                    }
                }
                break;
            default:
                break;
        }
    }
    return false;
}

} // anonymous namespace

LC_API float lc_input_get_action_strength(const char* action_name) {
    if (!action_name) return 0.0f;
    try { return lupine::input::InputManager::Get().GetActionStrength(std::string(action_name), -1); }
    catch (...) { return 0.0f; }
}

LC_API LCResult lc_input_get_vector(const char* negative_x, const char* positive_x,
                                    const char* negative_y, const char* positive_y,
                                    int player, LCVec2* out_vector) {
    if (!negative_x || !positive_x || !negative_y || !positive_y || !out_vector) {
        SetInputError(LC_ERROR_NULL_POINTER, "action name or out_vector is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        lupine::input::InputManager& input = lupine::input::InputManager::Get();
        float x = input.GetActionStrength(positive_x, player) - input.GetActionStrength(negative_x, player);
        float y = input.GetActionStrength(positive_y, player) - input.GetActionStrength(negative_y, player);
        float lengthSq = x * x + y * y;
        if (lengthSq > 1.0f) {
            float length = std::sqrt(lengthSq);
            x /= length;
            y /= length;
        }
        out_vector->x = x;
        out_vector->y = y;
        return LC_SUCCESS;
    } catch (...) {
        SetInputError(LC_ERROR_INTERNAL_ERROR, "Failed to compute input vector");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Gamepad device introspection and rumble
 * ============================================================================ */

LC_API bool lc_input_is_gamepad_connected(uint32_t gamepad_id) {
    try { return lupine::input::InputManager::Get().IsGamepadConnected(gamepad_id); }
    catch (...) { return false; }
}

LC_API int lc_input_get_gamepad_count(void) {
    try { return static_cast<int>(lupine::input::InputManager::Get().GetGamepadCount()); }
    catch (...) { return 0; }
}

LC_API LCResult lc_input_get_connected_gamepad_ids(char* out_buffer, size_t buffer_size) {
    try {
        nlohmann::json arr = nlohmann::json::array();
        for (uint32_t id : lupine::input::InputManager::Get().GetConnectedGamepadIDs()) {
            arr.push_back(id);
        }
        return WriteJsonToBuffer(arr, out_buffer, buffer_size);
    } catch (...) {
        SetInputError(LC_ERROR_INTERNAL_ERROR, "Failed to get connected gamepad ids");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_input_get_gamepad_name(uint32_t gamepad_id, char* out_buffer, size_t buffer_size) {
    if (!out_buffer || buffer_size == 0) {
        SetInputError(LC_ERROR_NULL_POINTER, "out_buffer is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        const std::string name = lupine::input::InputManager::Get().GetGamepadName(gamepad_id);
        if (name.size() + 1 > buffer_size) {
            SetInputError(LC_ERROR_INVALID_PARAMETER, "Buffer too small for gamepad name");
            return LC_ERROR_INVALID_PARAMETER;
        }
        std::memcpy(out_buffer, name.c_str(), name.size() + 1);
        return LC_SUCCESS;
    } catch (...) {
        SetInputError(LC_ERROR_INTERNAL_ERROR, "Failed to get gamepad name");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API void lc_input_set_gamepad_vibration(uint32_t gamepad_id, float left_motor,
                                           float right_motor, float duration_seconds) {
    try {
        uint32_t durationMs = duration_seconds > 0.0f
                                  ? static_cast<uint32_t>(duration_seconds * 1000.0f)
                                  : 0u;
        lupine::input::InputManager::Get().SetGamepadVibration(gamepad_id, left_motor, right_motor, durationMs);
    } catch (...) {}
}

LC_API void lc_input_stop_gamepad_vibration(uint32_t gamepad_id) {
    try { lupine::input::InputManager::Get().StopGamepadVibration(gamepad_id); } catch (...) {}
}

/* ============================================================================
 * Glyph / context / player JSON queries
 * ============================================================================ */

LC_API LCResult lc_input_get_action_glyphs(const char* action_name, char* out_buffer, size_t buffer_size) {
    if (!action_name) {
        SetInputError(LC_ERROR_NULL_POINTER, "action_name is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    try {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& g : lupine::input::InputManager::Get().GetActionGlyphs(action_name)) {
            arr.push_back(GlyphToJson(g));
        }
        return WriteJsonToBuffer(arr, out_buffer, buffer_size);
    } catch (...) {
        SetInputError(LC_ERROR_INTERNAL_ERROR, "Failed to get action glyphs");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_input_get_active_contexts(char* out_buffer, size_t buffer_size) {
    try {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& ctx : lupine::input::InputManager::Get().GetActiveContexts()) {
            arr.push_back(ctx);
        }
        return WriteJsonToBuffer(arr, out_buffer, buffer_size);
    } catch (...) {
        SetInputError(LC_ERROR_INTERNAL_ERROR, "Failed to get active contexts");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_input_get_player_gamepads(int player, char* out_buffer, size_t buffer_size) {
    try {
        nlohmann::json arr = nlohmann::json::array();
        for (uint32_t id : lupine::input::InputManager::Get().GetPlayerGamepads(player)) {
            arr.push_back(id);
        }
        return WriteJsonToBuffer(arr, out_buffer, buffer_size);
    } catch (...) {
        SetInputError(LC_ERROR_INTERNAL_ERROR, "Failed to get player gamepads");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Event-driven action matching
 * ============================================================================ */

LC_API LCResult lc_input_event_is_action(const char* event_json, const char* action_name, bool* out_result) {
    if (!action_name || !out_result) {
        SetInputError(LC_ERROR_NULL_POINTER, "action_name or out_result is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    nlohmann::json event;
    if (!ParseEventJson(event_json, event)) {
        SetInputError(LC_ERROR_INVALID_PARAMETER, "event_json is not a valid event object");
        return LC_ERROR_INVALID_PARAMETER;
    }
    try {
        *out_result = EventMatchesAction(event, action_name);
        return LC_SUCCESS;
    } catch (...) {
        SetInputError(LC_ERROR_INTERNAL_ERROR, "Failed to match input event");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_input_event_is_action_pressed(const char* event_json, const char* action_name, bool* out_result) {
    if (!action_name || !out_result) {
        SetInputError(LC_ERROR_NULL_POINTER, "action_name or out_result is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    nlohmann::json event;
    if (!ParseEventJson(event_json, event)) {
        SetInputError(LC_ERROR_INVALID_PARAMETER, "event_json is not a valid event object");
        return LC_ERROR_INVALID_PARAMETER;
    }
    try {
        bool result = false;
        if (EventMatchesAction(event, action_name)) {
            const std::string type = event.value("type", std::string());
            if (type.size() >= 5 && type.compare(type.size() - 5, 5, "_down") == 0) {
                result = true;
            } else if (type == "gamepad_axis") {
                result = std::abs(event.value("value", 0.0f)) > 0.5f;
            }
        }
        *out_result = result;
        return LC_SUCCESS;
    } catch (...) {
        SetInputError(LC_ERROR_INTERNAL_ERROR, "Failed to match input event");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_input_event_is_action_released(const char* event_json, const char* action_name, bool* out_result) {
    if (!action_name || !out_result) {
        SetInputError(LC_ERROR_NULL_POINTER, "action_name or out_result is NULL");
        return LC_ERROR_NULL_POINTER;
    }
    nlohmann::json event;
    if (!ParseEventJson(event_json, event)) {
        SetInputError(LC_ERROR_INVALID_PARAMETER, "event_json is not a valid event object");
        return LC_ERROR_INVALID_PARAMETER;
    }
    try {
        bool result = false;
        if (EventMatchesAction(event, action_name)) {
            const std::string type = event.value("type", std::string());
            if (type.size() >= 3 && type.compare(type.size() - 3, 3, "_up") == 0) {
                result = true;
            } else if (type == "gamepad_axis") {
                result = std::abs(event.value("value", 0.0f)) <= 0.5f;
            }
        }
        *out_result = result;
        return LC_SUCCESS;
    } catch (...) {
        SetInputError(LC_ERROR_INTERNAL_ERROR, "Failed to match input event");
        return LC_ERROR_INTERNAL_ERROR;
    }
}