#pragma once

#include "InputCodes.hpp"
#include <glm/glm.hpp>
#include <string>
#include <cstdint>

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

private:
    static constexpr size_t MAX_KEYS = 512;
    bool m_CurrentKeyState[MAX_KEYS] = {false};
    bool m_PreviousKeyState[MAX_KEYS] = {false};
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

    void Update() override;
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

    static constexpr size_t MAX_BUTTONS = 15;
    bool m_CurrentButtonState[MAX_BUTTONS] = {false};
    bool m_PreviousButtonState[MAX_BUTTONS] = {false};

    static constexpr size_t MAX_AXES = 6;
    float m_AxisValues[MAX_AXES] = {0.0f};

    float ApplyDeadzone(float value) const;
};

} // namespace input
} // namespace lupine
