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

/**
 * @brief Set the global gamepad stick deadzone
 * @param deadzone Deadzone radius (0..1) applied to analog sticks
 * @threadsafety Thread-safe
 */
LC_API void lc_input_set_gamepad_deadzone(float deadzone);

/**
 * @brief Get the global gamepad stick deadzone
 * @return Deadzone radius (0..1)
 * @threadsafety Thread-safe
 */
LC_API float lc_input_get_gamepad_deadzone(void);

/* ============================================================================
 * Touch Input
 * ============================================================================ */

/**
 * @brief Check if a touch device is available
 * @return true if touch input is available, false otherwise
 * @threadsafety Thread-safe
 */
LC_API bool lc_input_is_touch_available(void);

/**
 * @brief Check if there is at least one active touch point
 * @return true if currently touching, false otherwise
 * @threadsafety Thread-safe
 */
LC_API bool lc_input_is_touching(void);

/**
 * @brief Get the number of active touch points
 * @return Active touch count
 * @threadsafety Thread-safe
 */
LC_API uint32_t lc_input_get_touch_count(void);

/**
 * @brief Get the position of an active touch point
 * @param index Touch index (0-based, default 0 for the first touch)
 * @param out_position Output parameter for the touch position (window pixels)
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_input_get_touch_position(uint32_t index, LCVec2* out_position);

/**
 * @brief Check if a touch just started this frame
 * @return true if a touch began this frame, false otherwise
 * @threadsafety Thread-safe
 */
LC_API bool lc_input_is_touch_just_started(void);

/**
 * @brief Check if a touch just ended this frame
 * @return true if a touch ended this frame, false otherwise
 * @threadsafety Thread-safe
 */
LC_API bool lc_input_is_touch_just_ended(void);

/* ============================================================================
 * Clipboard
 * ============================================================================ */

/**
 * @brief Get the current system clipboard text
 * @param out_buffer Caller-owned buffer to receive the null-terminated text
 * @param buffer_size Size of out_buffer in bytes
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_input_get_clipboard(char* out_buffer, size_t buffer_size);

/**
 * @brief Set the system clipboard text
 * @param text Null-terminated text to place on the clipboard
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_input_set_clipboard(const char* text);

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

/* ============================================================================
 * Device types, glyphs and bindings
 * ============================================================================ */

/** @brief Input device categories (matches lupine::input::InputDeviceType). */
typedef enum {
    LC_INPUT_DEVICE_KEYBOARD = 0,
    LC_INPUT_DEVICE_MOUSE = 1,
    LC_INPUT_DEVICE_GAMEPAD = 2,
    LC_INPUT_DEVICE_TOUCH = 3,
    LC_INPUT_DEVICE_UNKNOWN = 4
} LCInputDeviceType;

/** @brief Physical gamepad family (matches lupine::input::GamepadType). */
typedef enum {
    LC_GAMEPAD_TYPE_UNKNOWN = 0,
    LC_GAMEPAD_TYPE_XBOX = 1,
    LC_GAMEPAD_TYPE_PLAYSTATION = 2,
    LC_GAMEPAD_TYPE_NINTENDO = 3,
    LC_GAMEPAD_TYPE_STEAM = 4,
    LC_GAMEPAD_TYPE_GENERIC = 5
} LCGamepadType;

/** @brief Capture device-mask bits for lc_input_start_input_capture(). */
typedef enum {
    LC_CAPTURE_KEYBOARD = 1 << 0,
    LC_CAPTURE_MOUSE = 1 << 1,
    LC_CAPTURE_GAMEPAD = 1 << 2,
    LC_CAPTURE_ALL = (1 << 0) | (1 << 1) | (1 << 2)
} LCCaptureDevice;

/** @brief A single resolved input binding. */
typedef struct LCInputBinding {
    int device_type;      /* LCInputDeviceType */
    int key_code;         /* LCKeyCode (keyboard) */
    int mouse_button;     /* LCMouseButton (mouse) */
    int gamepad_button;   /* LCGamepadButton (gamepad), -1/255 if none */
    int gamepad_axis;     /* LCGamepadAxis (gamepad), -1/255 if none */
    uint32_t gamepad_id;
    float scale;
} LCInputBinding;

/** @brief A resolved on-screen prompt glyph for an action. */
typedef struct LCInputGlyph {
    char glyph_id[128];
    char label[128];
    char art_path[256];
    int device;        /* LCInputDeviceType */
    int gamepad_type;  /* LCGamepadType */
} LCInputGlyph;

/** @brief Callback fired when an action changes pressed state. */
typedef void (*LCInputActionCallback)(const char* action_name, bool pressed, void* user_data);

/**
 * @brief Get the strength of an input action (any device)
 * @param action_name Name of the action
 * @return Action strength in [0, 1]
 * @threadsafety Thread-safe
 */
LC_API float lc_input_get_action_strength(const char* action_name);

/**
 * @brief Get a 2D vector from four directional actions (analog-aware, clamped to
 *        unit length so diagonal input is not faster)
 * @param negative_x Action driving -X
 * @param positive_x Action driving +X
 * @param negative_y Action driving -Y
 * @param positive_y Action driving +Y
 * @param player Player slot, or -1 for any device
 * @param out_vector Output parameter for the resulting vector
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_input_get_vector(const char* negative_x, const char* positive_x,
                                    const char* negative_y, const char* positive_y,
                                    int player, LCVec2* out_vector);

/* ============================================================================
 * Gamepad device introspection and rumble
 * ============================================================================ */

/**
 * @brief Check if a gamepad is connected
 * @param gamepad_id Gamepad ID (0 = first)
 * @return true if connected, false otherwise
 * @threadsafety Thread-safe
 */
LC_API bool lc_input_is_gamepad_connected(uint32_t gamepad_id);

/**
 * @brief Get the number of connected gamepads
 * @return Connected gamepad count
 * @threadsafety Thread-safe
 */
LC_API int lc_input_get_gamepad_count(void);

/**
 * @brief Get the IDs of all connected gamepads as a JSON integer array
 * @param out_buffer Caller-owned buffer to receive the null-terminated JSON
 * @param buffer_size Size of out_buffer in bytes
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_input_get_connected_gamepad_ids(char* out_buffer, size_t buffer_size);

/**
 * @brief Get a gamepad's human-readable name
 * @param gamepad_id Gamepad ID (0 = first)
 * @param out_buffer Caller-owned buffer to receive the null-terminated name
 * @param buffer_size Size of out_buffer in bytes
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_input_get_gamepad_name(uint32_t gamepad_id, char* out_buffer, size_t buffer_size);

/**
 * @brief Set gamepad rumble. Motor strengths are clamped to [0, 1].
 * @param gamepad_id Gamepad ID
 * @param left_motor Low-frequency motor strength [0, 1]
 * @param right_motor High-frequency motor strength [0, 1]
 * @param duration_seconds Duration in seconds; <= 0 means until changed/stopped
 * @threadsafety Thread-safe
 */
LC_API void lc_input_set_gamepad_vibration(uint32_t gamepad_id, float left_motor,
                                           float right_motor, float duration_seconds);

/**
 * @brief Stop any active gamepad rumble
 * @param gamepad_id Gamepad ID
 * @threadsafety Thread-safe
 */
LC_API void lc_input_stop_gamepad_vibration(uint32_t gamepad_id);

/* ============================================================================
 * Per-player action/axis queries (local multiplayer)
 * ============================================================================ */

LC_API bool lc_input_is_action_pressed_for_player(const char* action_name, int player);
LC_API bool lc_input_is_action_just_pressed_for_player(const char* action_name, int player);
LC_API bool lc_input_is_action_just_released_for_player(const char* action_name, int player);
LC_API float lc_input_get_axis_for_player(const char* axis_name, int player);
LC_API float lc_input_get_action_strength_for_player(const char* action_name, int player);

/* ============================================================================
 * Active device detection
 * ============================================================================ */

LC_API LCInputDeviceType lc_input_get_active_device_type(void);
LC_API uint32_t lc_input_get_last_gamepad_id(void);
LC_API LCGamepadType lc_input_get_gamepad_type(uint32_t gamepad_id);

/* ============================================================================
 * Input contexts / action sets
 * ============================================================================ */

LC_API void lc_input_enable_context(const char* context);
LC_API void lc_input_disable_context(const char* context);
LC_API void lc_input_set_context_active(const char* context, bool active);
LC_API bool lc_input_is_context_active(const char* context);
LC_API void lc_input_set_exclusive_context(const char* context);
LC_API void lc_input_set_action_enabled(const char* action_name, bool enabled);
LC_API void lc_input_set_axis_enabled(const char* axis_name, bool enabled);

/* ============================================================================
 * Local multiplayer player slots
 * ============================================================================ */

LC_API void lc_input_set_player_count(int count);
LC_API int lc_input_get_player_count(void);
LC_API void lc_input_clear_player_assignments(void);
LC_API void lc_input_assign_keyboard_mouse_to_player(int player);
LC_API void lc_input_assign_gamepad_to_player(int player, uint32_t gamepad_id);
LC_API void lc_input_unassign_gamepad(uint32_t gamepad_id);
LC_API int lc_input_get_player_for_gamepad(uint32_t gamepad_id);
LC_API int lc_input_get_player_for_keyboard_mouse(void);
LC_API bool lc_input_player_owns_keyboard_mouse(int player);
LC_API void lc_input_set_auto_join_enabled(bool enabled);
LC_API bool lc_input_is_auto_join_enabled(void);

/* ============================================================================
 * Runtime rebinding
 * ============================================================================ */

LC_API void lc_input_add_action_key(const char* action_name, LCKeyCode key);
LC_API void lc_input_add_action_mouse_button(const char* action_name, LCMouseButton button);
LC_API void lc_input_add_action_gamepad_button(const char* action_name, LCGamepadButton button, uint32_t gamepad_id);
LC_API void lc_input_add_action_gamepad_axis(const char* action_name, LCGamepadAxis axis, float scale, uint32_t gamepad_id);
LC_API void lc_input_remove_action_binding(const char* action_name, int index);
LC_API void lc_input_clear_action_bindings(const char* action_name);
LC_API int lc_input_get_action_binding_count(const char* action_name);
LC_API LCResult lc_input_get_action_binding(const char* action_name, int index, LCInputBinding* out_binding);
LC_API void lc_input_add_axis_key(const char* axis_name, LCKeyCode key, float scale);
LC_API void lc_input_add_axis_gamepad_axis(const char* axis_name, LCGamepadAxis axis, float scale, uint32_t gamepad_id);
LC_API void lc_input_remove_axis_binding(const char* axis_name, int index);
LC_API void lc_input_clear_axis_bindings(const char* axis_name);
LC_API int lc_input_get_axis_binding_count(const char* axis_name);
LC_API LCResult lc_input_get_axis_binding(const char* axis_name, int index, LCInputBinding* out_binding);
LC_API bool lc_input_save_input_map(const char* filepath);
LC_API bool lc_input_load_input_map(const char* filepath);

/* ============================================================================
 * Input capture (rebind menus)
 * ============================================================================ */

LC_API void lc_input_start_input_capture(uint32_t device_mask);
LC_API void lc_input_cancel_input_capture(void);
LC_API bool lc_input_is_capturing(void);
LC_API bool lc_input_is_capture_complete(void);
LC_API LCResult lc_input_get_captured_binding(LCInputBinding* out_binding);
LC_API void lc_input_clear_captured_binding(void);
LC_API void lc_input_apply_captured_binding_to_action(const char* action_name);

/* ============================================================================
 * Glyph / prompt resolution
 * ============================================================================ */

LC_API LCResult lc_input_get_action_glyph(const char* action_name, int player,
                                          int device_override, LCInputGlyph* out_glyph);
LC_API void lc_input_set_glyph_label(const char* glyph_id, const char* label);
LC_API void lc_input_set_glyph_art(const char* glyph_id, const char* art_path);
LC_API void lc_input_clear_glyph_override(const char* glyph_id);
LC_API void lc_input_clear_glyph_overrides(void);
LC_API bool lc_input_load_glyph_map(const char* filepath);
LC_API bool lc_input_save_glyph_map(const char* filepath);

/**
 * @brief Get all on-screen prompt glyphs for an action (one per binding) as a
 *        JSON array of {glyph_id,label,art_path,device,gamepad_type} objects
 * @param action_name Name of the action
 * @param out_buffer Caller-owned buffer to receive the null-terminated JSON
 * @param buffer_size Size of out_buffer in bytes
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_input_get_action_glyphs(const char* action_name, char* out_buffer, size_t buffer_size);

/* ============================================================================
 * Input contexts / player list queries (JSON)
 * ============================================================================ */

/**
 * @brief Get the currently active input contexts as a JSON string array
 * @param out_buffer Caller-owned buffer to receive the null-terminated JSON
 * @param buffer_size Size of out_buffer in bytes
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_input_get_active_contexts(char* out_buffer, size_t buffer_size);

/**
 * @brief Get the gamepad IDs assigned to a player slot as a JSON integer array
 * @param player Player slot index
 * @param out_buffer Caller-owned buffer to receive the null-terminated JSON
 * @param buffer_size Size of out_buffer in bytes
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_input_get_player_gamepads(int player, char* out_buffer, size_t buffer_size);

/* ============================================================================
 * Event-driven action matching (for use inside an input-event handler)
 * ============================================================================ */

/**
 * @brief Test whether a serialized input event matches an action's bindings
 * @param event_json The input event as a JSON object string
 * @param action_name Name of the action
 * @param out_result Output parameter for the match result
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_input_event_is_action(const char* event_json, const char* action_name, bool* out_result);

/**
 * @brief Test whether an input event is the "pressed" edge of an action
 * @param event_json The input event as a JSON object string
 * @param action_name Name of the action
 * @param out_result Output parameter for the match result
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_input_event_is_action_pressed(const char* event_json, const char* action_name, bool* out_result);

/**
 * @brief Test whether an input event is the "released" edge of an action
 * @param event_json The input event as a JSON object string
 * @param action_name Name of the action
 * @param out_result Output parameter for the match result
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_input_event_is_action_released(const char* event_json, const char* action_name, bool* out_result);

/* ============================================================================
 * Action delegation (native callbacks)
 * ============================================================================ */

LC_API void lc_input_register_action_callback(const char* action_name,
                                              LCInputActionCallback callback, void* user_data);
LC_API void lc_input_unregister_action_callback(const char* action_name);


#endif /* LUPINE_CAPI_INPUT_H */
