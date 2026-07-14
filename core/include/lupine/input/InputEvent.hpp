#pragma once

#include "InputCodes.hpp"
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include <cstdint>

namespace lupine {
namespace input {

/**
 * Discrete input event type.
 *
 * Unlike the per-frame polling API on InputManager, these represent individual
 * platform input events as they happen and are delivered to scripts through the
 * event-based dispatch (script function on_input_event(event)).
 */
enum class InputEventType : uint8_t {
    KeyDown,
    KeyUp,
    Text,
    MouseButtonDown,
    MouseButtonUp,
    MouseMotion,
    MouseWheel,
    GamepadButtonDown,
    GamepadButtonUp,
    GamepadAxis
};

/**
 * A single discrete input event.
 *
 * Produced by the platform input adapter (e.g. SDL2InputAdapter) for each raw
 * event and buffered for one frame by InputManager. The script-facing payload is
 * a plain object produced by ToJson(); scripting bindings hand that object to the
 * script's on_input_event(event) function.
 */
struct InputEvent {
    InputEventType type = InputEventType::KeyDown;

    // Keyboard (KeyDown / KeyUp)
    KeyCode key = KeyCode::Unknown;
    bool repeat = false;            // true for OS key-repeat events

    // Text (Text) - resolved Unicode codepoint through layout/IME
    uint32_t codepoint = 0;

    // Mouse button (MouseButtonDown / MouseButtonUp)
    MouseButton mouseButton = MouseButton::Unknown;

    // Mouse position/movement (MouseButton*, MouseMotion)
    glm::vec2 position{0.0f, 0.0f};   // current cursor position (pixels)
    glm::vec2 relative{0.0f, 0.0f};   // motion since last event (MouseMotion)

    // Mouse wheel (MouseWheel) - x horizontal, y vertical
    glm::vec2 scroll{0.0f, 0.0f};

    // Gamepad (GamepadButton* / GamepadAxis)
    GamepadButton gamepadButton = GamepadButton::Unknown;
    GamepadAxis gamepadAxis = GamepadAxis::Unknown;
    float axisValue = 0.0f;          // normalized axis value (GamepadAxis)
    uint32_t gamepadID = 0;          // source gamepad device id

    /**
     * Convert to the script-facing event object. Keys present depend on `type`;
     * "type" is always one of: "key_down", "key_up", "text", "mouse_button_down",
     * "mouse_button_up", "mouse_motion", "mouse_wheel", "gamepad_button_down",
     * "gamepad_button_up", "gamepad_axis".
     */
    nlohmann::json ToJson() const {
        nlohmann::json e = nlohmann::json::object();
        switch (type) {
            case InputEventType::KeyDown:
            case InputEventType::KeyUp:
                e["type"] = (type == InputEventType::KeyDown) ? "key_down" : "key_up";
                e["key"] = static_cast<int>(key);
                e["pressed"] = (type == InputEventType::KeyDown);
                e["repeat"] = repeat;
                break;
            case InputEventType::Text:
                e["type"] = "text";
                e["codepoint"] = static_cast<int>(codepoint);
                break;
            case InputEventType::MouseButtonDown:
            case InputEventType::MouseButtonUp:
                e["type"] = (type == InputEventType::MouseButtonDown) ? "mouse_button_down" : "mouse_button_up";
                e["button"] = static_cast<int>(mouseButton);
                e["pressed"] = (type == InputEventType::MouseButtonDown);
                e["position"] = { position.x, position.y };
                break;
            case InputEventType::MouseMotion:
                e["type"] = "mouse_motion";
                e["position"] = { position.x, position.y };
                e["relative"] = { relative.x, relative.y };
                break;
            case InputEventType::MouseWheel:
                e["type"] = "mouse_wheel";
                e["delta"] = { scroll.x, scroll.y };
                break;
            case InputEventType::GamepadButtonDown:
            case InputEventType::GamepadButtonUp:
                e["type"] = (type == InputEventType::GamepadButtonDown) ? "gamepad_button_down" : "gamepad_button_up";
                e["button"] = static_cast<int>(gamepadButton);
                e["pressed"] = (type == InputEventType::GamepadButtonDown);
                e["gamepad_id"] = static_cast<int>(gamepadID);
                break;
            case InputEventType::GamepadAxis:
                e["type"] = "gamepad_axis";
                e["axis"] = static_cast<int>(gamepadAxis);
                e["value"] = axisValue;
                e["gamepad_id"] = static_cast<int>(gamepadID);
                break;
        }
        return e;
    }
};

} // namespace input
} // namespace lupine
