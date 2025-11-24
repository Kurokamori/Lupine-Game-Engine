/**
 * @file lc_input.h
 * @brief Lupine Engine C API - Input management
 *
 * This header provides input functionality for keyboard, mouse, and gamepad.
 */

#ifndef LUPINE_CAPI_INPUT_H
#define LUPINE_CAPI_INPUT_H

#include "core/lc_core.h"
#include "math/lc_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Key Codes
 * ============================================================================ */

/**
 * @brief Keyboard key codes (subset of most common keys)
 */
typedef enum {
    LC_KEY_SPACE = 32,
    LC_KEY_APOSTROPHE = 39,
    LC_KEY_COMMA = 44,
    LC_KEY_MINUS = 45,
    LC_KEY_PERIOD = 46,
    LC_KEY_SLASH = 47,

    LC_KEY_0 = 48,
    LC_KEY_1 = 49,
    LC_KEY_2 = 50,
    LC_KEY_3 = 51,
    LC_KEY_4 = 52,
    LC_KEY_5 = 53,
    LC_KEY_6 = 54,
    LC_KEY_7 = 55,
    LC_KEY_8 = 56,
    LC_KEY_9 = 57,

    LC_KEY_SEMICOLON = 59,
    LC_KEY_EQUAL = 61,

    LC_KEY_A = 65,
    LC_KEY_B = 66,
    LC_KEY_C = 67,
    LC_KEY_D = 68,
    LC_KEY_E = 69,
    LC_KEY_F = 70,
    LC_KEY_G = 71,
    LC_KEY_H = 72,
    LC_KEY_I = 73,
    LC_KEY_J = 74,
    LC_KEY_K = 75,
    LC_KEY_L = 76,
    LC_KEY_M = 77,
    LC_KEY_N = 78,
    LC_KEY_O = 79,
    LC_KEY_P = 80,
    LC_KEY_Q = 81,
    LC_KEY_R = 82,
    LC_KEY_S = 83,
    LC_KEY_T = 84,
    LC_KEY_U = 85,
    LC_KEY_V = 86,
    LC_KEY_W = 87,
    LC_KEY_X = 88,
    LC_KEY_Y = 89,
    LC_KEY_Z = 90,

    LC_KEY_LEFT_BRACKET = 91,
    LC_KEY_BACKSLASH = 92,
    LC_KEY_RIGHT_BRACKET = 93,
    LC_KEY_GRAVE_ACCENT = 96,

    LC_KEY_ESCAPE = 256,
    LC_KEY_ENTER = 257,
    LC_KEY_TAB = 258,
    LC_KEY_BACKSPACE = 259,
    LC_KEY_INSERT = 260,
    LC_KEY_DELETE = 261,
    LC_KEY_RIGHT = 262,
    LC_KEY_LEFT = 263,
    LC_KEY_DOWN = 264,
    LC_KEY_UP = 265,
    LC_KEY_PAGE_UP = 266,
    LC_KEY_PAGE_DOWN = 267,
    LC_KEY_HOME = 268,
    LC_KEY_END = 269,

    LC_KEY_CAPS_LOCK = 280,
    LC_KEY_SCROLL_LOCK = 281,
    LC_KEY_NUM_LOCK = 282,
    LC_KEY_PRINT_SCREEN = 283,
    LC_KEY_PAUSE = 284,

    LC_KEY_F1 = 290,
    LC_KEY_F2 = 291,
    LC_KEY_F3 = 292,
    LC_KEY_F4 = 293,
    LC_KEY_F5 = 294,
    LC_KEY_F6 = 295,
    LC_KEY_F7 = 296,
    LC_KEY_F8 = 297,
    LC_KEY_F9 = 298,
    LC_KEY_F10 = 299,
    LC_KEY_F11 = 300,
    LC_KEY_F12 = 301,

    LC_KEY_LEFT_SHIFT = 340,
    LC_KEY_LEFT_CONTROL = 341,
    LC_KEY_LEFT_ALT = 342,
    LC_KEY_LEFT_SUPER = 343,
    LC_KEY_RIGHT_SHIFT = 344,
    LC_KEY_RIGHT_CONTROL = 345,
    LC_KEY_RIGHT_ALT = 346,
    LC_KEY_RIGHT_SUPER = 347
} LCKeyCode;

/**
 * @brief Mouse button codes
 */
typedef enum {
    LC_MOUSE_BUTTON_LEFT = 0,
    LC_MOUSE_BUTTON_RIGHT = 1,
    LC_MOUSE_BUTTON_MIDDLE = 2,
    LC_MOUSE_BUTTON_4 = 3,
    LC_MOUSE_BUTTON_5 = 4
} LCMouseButton;

/**
 * @brief Gamepad button codes
 */
typedef enum {
    LC_GAMEPAD_BUTTON_A = 0,
    LC_GAMEPAD_BUTTON_B = 1,
    LC_GAMEPAD_BUTTON_X = 2,
    LC_GAMEPAD_BUTTON_Y = 3,
    LC_GAMEPAD_BUTTON_LEFT_BUMPER = 4,
    LC_GAMEPAD_BUTTON_RIGHT_BUMPER = 5,
    LC_GAMEPAD_BUTTON_BACK = 6,
    LC_GAMEPAD_BUTTON_START = 7,
    LC_GAMEPAD_BUTTON_GUIDE = 8,
    LC_GAMEPAD_BUTTON_LEFT_THUMB = 9,
    LC_GAMEPAD_BUTTON_RIGHT_THUMB = 10,
    LC_GAMEPAD_BUTTON_DPAD_UP = 11,
    LC_GAMEPAD_BUTTON_DPAD_RIGHT = 12,
    LC_GAMEPAD_BUTTON_DPAD_DOWN = 13,
    LC_GAMEPAD_BUTTON_DPAD_LEFT = 14
} LCGamepadButton;

/**
 * @brief Gamepad axis codes
 */
typedef enum {
    LC_GAMEPAD_AXIS_LEFT_X = 0,
    LC_GAMEPAD_AXIS_LEFT_Y = 1,
    LC_GAMEPAD_AXIS_RIGHT_X = 2,
    LC_GAMEPAD_AXIS_RIGHT_Y = 3,
    LC_GAMEPAD_AXIS_LEFT_TRIGGER = 4,
    LC_GAMEPAD_AXIS_RIGHT_TRIGGER = 5
} LCGamepadAxis;

/* ============================================================================
 * Keyboard Input
 * ============================================================================ */

/**
 * @brief Check if a key is currently pressed
 * @param key Key code to check
 * @return true if pressed, false otherwise
 * @threadsafety Thread-safe
 */
LC_API bool lc_input_is_key_pressed(LCKeyCode key);

/**
 * @brief Check if a key was just pressed this frame
 * @param key Key code to check
 * @return true if just pressed, false otherwise
 * @threadsafety Thread-safe
 */
LC_API bool lc_input_is_key_just_pressed(LCKeyCode key);

/**
 * @brief Check if a key was just released this frame
 * @param key Key code to check
 * @return true if just released, false otherwise
 * @threadsafety Thread-safe
 */
LC_API bool lc_input_is_key_just_released(LCKeyCode key);

/* ============================================================================
 * Mouse Input
 * ============================================================================ */

/**
 * @brief Check if a mouse button is currently pressed
 * @param button Mouse button to check
 * @return true if pressed, false otherwise
 * @threadsafety Thread-safe
 */
LC_API bool lc_input_is_mouse_button_pressed(LCMouseButton button);

/**
 * @brief Check if a mouse button was just pressed this frame
 * @param button Mouse button to check
 * @return true if just pressed, false otherwise
 * @threadsafety Thread-safe
 */
LC_API bool lc_input_is_mouse_button_just_pressed(LCMouseButton button);

/**
 * @brief Check if a mouse button was just released this frame
 * @param button Mouse button to check
 * @return true if just released, false otherwise
 * @threadsafety Thread-safe
 */
LC_API bool lc_input_is_mouse_button_just_released(LCMouseButton button);

/**
 * @brief Get the current mouse position
 * @param out_position Output parameter for mouse position
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_input_get_mouse_position(LCVec2* out_position);

/**
 * @brief Get the mouse movement delta this frame
 * @param out_delta Output parameter for mouse delta
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_input_get_mouse_delta(LCVec2* out_delta);

/**
 * @brief Get the mouse scroll delta this frame
 * @param out_delta Output parameter for scroll delta
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_input_get_mouse_scroll_delta(LCVec2* out_delta);

/* ============================================================================
 * Gamepad Input
 * ============================================================================ */

/**
 * @brief Check if a gamepad button is currently pressed
 * @param button Gamepad button to check
 * @param gamepad_id Gamepad ID (default: 0)
 * @return true if pressed, false otherwise
 * @threadsafety Thread-safe
 */
LC_API bool lc_input_is_gamepad_button_pressed(LCGamepadButton button, uint32_t gamepad_id);

/**
 * @brief Check if a gamepad button was just pressed this frame
 * @param button Gamepad button to check
 * @param gamepad_id Gamepad ID (default: 0)
 * @return true if just pressed, false otherwise
 * @threadsafety Thread-safe
 */
LC_API bool lc_input_is_gamepad_button_just_pressed(LCGamepadButton button, uint32_t gamepad_id);

/**
 * @brief Check if a gamepad button was just released this frame
 * @param button Gamepad button to check
 * @param gamepad_id Gamepad ID (default: 0)
 * @return true if just released, false otherwise
 * @threadsafety Thread-safe
 */
LC_API bool lc_input_is_gamepad_button_just_released(LCGamepadButton button, uint32_t gamepad_id);

/**
 * @brief Get a gamepad axis value
 * @param axis Gamepad axis to read
 * @param gamepad_id Gamepad ID (default: 0)
 * @return Axis value (-1.0 to 1.0 for sticks, 0.0 to 1.0 for triggers)
 * @threadsafety Thread-safe
 */
LC_API float lc_input_get_gamepad_axis(LCGamepadAxis axis, uint32_t gamepad_id);

/* ============================================================================
 * Input Actions (Mapped Inputs)
 * ============================================================================ */

/**
 * @brief Check if an input action is currently pressed
 * @param action_name Name of the action
 * @return true if pressed, false otherwise
 * @threadsafety Thread-safe
 */
LC_API bool lc_input_is_action_pressed(const char* action_name);

/**
 * @brief Check if an input action was just pressed this frame
 * @param action_name Name of the action
 * @return true if just pressed, false otherwise
 * @threadsafety Thread-safe
 */
LC_API bool lc_input_is_action_just_pressed(const char* action_name);

/**
 * @brief Check if an input action was just released this frame
 * @param action_name Name of the action
 * @return true if just released, false otherwise
 * @threadsafety Thread-safe
 */
LC_API bool lc_input_is_action_just_released(const char* action_name);

/**
 * @brief Get an input axis value (mapped input)
 * @param axis_name Name of the axis
 * @return Axis value (typically -1.0 to 1.0)
 * @threadsafety Thread-safe
 */
LC_API float lc_input_get_axis(const char* axis_name);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_INPUT_H */
