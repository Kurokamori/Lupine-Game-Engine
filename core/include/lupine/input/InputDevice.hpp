#pragma once

#include "InputCodes.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <cstdint>
#include <mutex>

namespace lupine {
namespace input {

/**
 * Abstract base class for input devices
 */
class InputDevice {
public:
    virtual ~InputDevice() = default;

    // Device information
    virtual InputDeviceType GetDeviceType() const = 0;
    virtual std::string GetDeviceName() const = 0;
    virtual uint32_t GetDeviceID() const = 0;
    virtual bool IsConnected() const = 0;

    // Update the device state (called each frame)
    virtual void Update() = 0;

    // Reset device state
    virtual void Reset() = 0;
};

/**
 * Keyboard device
 */
class KeyboardDevice : public InputDevice {
public:
    KeyboardDevice();
    virtual ~KeyboardDevice() = default;

    InputDeviceType GetDeviceType() const override { return InputDeviceType::Keyboard; }
    std::string GetDeviceName() const override { return "Keyboard"; }
    uint32_t GetDeviceID() const override { return 0; }
    bool IsConnected() const override { return true; }

    void Update() override;
    void Reset() override;

    // Key state management
    void SetKeyState(KeyCode key, bool pressed);
    bool IsKeyPressed(KeyCode key) const;
    bool IsKeyJustPressed(KeyCode key) const;
    bool IsKeyJustReleased(KeyCode key) const;

    // Text input (Unicode codepoints typed this frame, in event order).
    // Populated by the platform layer from OS text-input/IME events and cleared
    // every frame by Update(). Distinct from raw key state: this already accounts
    // for keyboard layout, shift, dead keys and IME composition.
    void AddTextInput(uint32_t codepoint);
    const std::vector<uint32_t>& GetTextInput() const { return m_TextInput; }
    void ClearTextInput();

    // Atomically move the accumulated text into `out` and clear the buffer. Used by the
    // consuming thread so it never copies the buffer while the platform thread is
    // appending to it (async runtime). `out` is overwritten.
    void DrainTextInput(std::vector<uint32_t>& out);

private:
    static constexpr size_t MAX_KEYS = 512;
    bool m_CurrentKeyState[MAX_KEYS] = {false};
    bool m_PreviousKeyState[MAX_KEYS] = {false};
    std::vector<uint32_t> m_TextInput;
    // Guards m_TextInput: appended by the platform thread, drained by the consuming
    // thread under the async runtime.
    std::mutex m_TextMutex;
};

/**
 * Mouse device
 */
class MouseDevice : public InputDevice {
public:
    MouseDevice();
    virtual ~MouseDevice() = default;

    InputDeviceType GetDeviceType() const override { return InputDeviceType::Mouse; }
    std::string GetDeviceName() const override { return "Mouse"; }
    uint32_t GetDeviceID() const override { return 0; }
    bool IsConnected() const override { return true; }

    // Begin-of-frame phase: snapshot the previous button state (for edge detection)
    // and clear the per-frame scroll delta. Must run BEFORE the platform pumps this
    // frame's events so Is*JustPressed/Released and GetScrollDelta see this frame's
    // input rather than having it overwritten.
    void Update() override;

    // End-of-poll phase: compute the per-frame mouse movement delta. Must run AFTER
    // this frame's position has been polled so GetDelta() is accurate with no lag.
    void FinalizeFrame();

    void Reset() override;

    // Button state management
    void SetButtonState(MouseButton button, bool pressed);
    bool IsButtonPressed(MouseButton button) const;
    bool IsButtonJustPressed(MouseButton button) const;
    bool IsButtonJustReleased(MouseButton button) const;

    // Position and movement
    void SetPosition(const glm::vec2& position);
    glm::vec2 GetPosition() const { return m_Position; }
    glm::vec2 GetDelta() const { return m_Delta; }

    // Scroll wheel
    void SetScrollDelta(const glm::vec2& delta);
    glm::vec2 GetScrollDelta() const { return m_ScrollDelta; }

private:
    static constexpr size_t MAX_BUTTONS = 8;
    bool m_CurrentButtonState[MAX_BUTTONS] = {false};
    bool m_PreviousButtonState[MAX_BUTTONS] = {false};

    glm::vec2 m_Position = glm::vec2(0.0f);
    glm::vec2 m_PreviousPosition = glm::vec2(0.0f);
    glm::vec2 m_Delta = glm::vec2(0.0f);
    glm::vec2 m_ScrollDelta = glm::vec2(0.0f);
};

/**
 * Gamepad device
 */
class GamepadDevice : public InputDevice {
public:
    GamepadDevice(uint32_t deviceID);
    virtual ~GamepadDevice() = default;

    InputDeviceType GetDeviceType() const override { return InputDeviceType::Gamepad; }
    std::string GetDeviceName() const override;
    uint32_t GetDeviceID() const override { return m_DeviceID; }
    bool IsConnected() const override { return m_Connected; }

    void Update() override;
    void Reset() override;

    // Connection management
    void SetConnected(bool connected);

    // Human-readable controller name (set by the platform layer; defaults to
    // "Gamepad <id>" when unset).
    void SetDeviceName(const std::string& name) { m_Name = name; }

    // Physical controller family, used to pick the right on-screen glyphs. Set by
    // the platform layer from SDL_GameControllerGetType.
    void SetGamepadType(GamepadType type) { m_Type = type; }
    GamepadType GetGamepadType() const { return m_Type; }

    // Button state management
    void SetButtonState(GamepadButton button, bool pressed);
    bool IsButtonPressed(GamepadButton button) const;
    bool IsButtonJustPressed(GamepadButton button) const;
    bool IsButtonJustReleased(GamepadButton button) const;

    // Axis state management
    void SetAxisValue(GamepadAxis axis, float value);
    float GetAxisValue(GamepadAxis axis) const;

    // Deadzone settings
    void SetDeadzone(float deadzone) { m_Deadzone = deadzone; }
    float GetDeadzone() const { return m_Deadzone; }

    // Vibration/rumble (optional, platform-dependent)
    void SetVibration(float leftMotor, float rightMotor);

private:
    uint32_t m_DeviceID;
    bool m_Connected = false;
    float m_Deadzone = 0.15f;
    std::string m_Name;
    GamepadType m_Type = GamepadType::Unknown;

    static constexpr size_t MAX_BUTTONS = 15;
    bool m_CurrentButtonState[MAX_BUTTONS] = {false};
    bool m_PreviousButtonState[MAX_BUTTONS] = {false};

    static constexpr size_t MAX_AXES = 6;
    float m_AxisValues[MAX_AXES] = {0.0f};

    float ApplyDeadzone(float value) const;
};

/**
 * Touch point data
 */
struct TouchPoint {
    uint32_t id = 0;           // Unique identifier for this touch
    glm::vec2 position;        // Current position
    glm::vec2 startPosition;   // Position where touch started
    glm::vec2 delta;           // Movement since last frame
    float pressure = 1.0f;     // Pressure (0-1, 1 if not supported)
    bool active = false;       // Is this touch currently active
    bool justStarted = false;  // Did this touch start this frame
    bool justEnded = false;    // Did this touch end this frame
};

/**
 * Touch input device for mobile/web platforms
 * Supports multi-touch with up to 10 simultaneous touch points
 */
class TouchDevice : public InputDevice {
public:
    static constexpr size_t MAX_TOUCH_POINTS = 10;

    TouchDevice();
    virtual ~TouchDevice() = default;

    InputDeviceType GetDeviceType() const override { return InputDeviceType::Touch; }
    std::string GetDeviceName() const override { return "Touch"; }
    uint32_t GetDeviceID() const override { return 0; }
    bool IsConnected() const override { return m_Available; }

    void Update() override;
    void Reset() override;

    // Availability
    void SetAvailable(bool available) { m_Available = available; }
    bool IsAvailable() const { return m_Available; }

    // Touch state management
    void BeginTouch(uint32_t id, const glm::vec2& position, float pressure = 1.0f);
    void UpdateTouch(uint32_t id, const glm::vec2& position, float pressure = 1.0f);
    void EndTouch(uint32_t id);
    void CancelTouch(uint32_t id);

    // Touch queries
    size_t GetActiveTouchCount() const;
    const TouchPoint* GetTouch(size_t index) const;
    const TouchPoint* GetTouchByID(uint32_t id) const;
    bool IsTouching() const { return GetActiveTouchCount() > 0; }

    // Convenience for single-touch games
    glm::vec2 GetPrimaryTouchPosition() const;
    bool HasPrimaryTouch() const;
    bool IsPrimaryTouchJustStarted() const;
    bool IsPrimaryTouchJustEnded() const;

private:
    bool m_Available = false;
    TouchPoint m_TouchPoints[MAX_TOUCH_POINTS];

    TouchPoint* FindTouchByID(uint32_t id);
    TouchPoint* FindFreeTouchSlot();
};

} // namespace input
} // namespace lupine
