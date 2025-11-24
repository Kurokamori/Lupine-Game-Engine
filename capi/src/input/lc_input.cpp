
#include "input/lc_input.h"
#include "lc_internal.h"

#include <lupine\input\InputManager.hpp>

namespace {

void SetInputError(LCResult code, const char* message) {
    ::SetError(code, message);
}

// Convert LC key codes to engine key codes
lupine::input::KeyCode ToKeyCode(LCKeyCode key) {
    return static_cast<lupine::input::KeyCode>(static_cast<uint16_t>(key));
}

// Convert LC mouse buttons to engine mouse buttons
lupine::input::MouseButton ToMouseButton(LCMouseButton button) {
    return static_cast<lupine::input::MouseButton>(static_cast<uint8_t>(button));
}

// Convert LC gamepad buttons to engine gamepad buttons
lupine::input::GamepadButton ToGamepadButton(LCGamepadButton button) {
    return static_cast<lupine::input::GamepadButton>(static_cast<uint8_t>(button));
}

// Convert LC gamepad axes to engine gamepad axes
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
}

/* ============================================================================
 * Mouse Input
 * ============================================================================ */

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
}

/* ============================================================================
 * Gamepad Input
 * ============================================================================ */

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
}

/* ============================================================================
 * Input Actions (Mapped Inputs)
 * ============================================================================ */

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

