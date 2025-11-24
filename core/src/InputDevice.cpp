#include "lupine/input/InputDevice.hpp"
#include <cstring>
#include <algorithm>

namespace lupine {
namespace input {

KeyboardDevice::KeyboardDevice() {
    std::memset(m_CurrentKeyState, 0, sizeof(m_CurrentKeyState));
    std::memset(m_PreviousKeyState, 0, sizeof(m_PreviousKeyState));
}

void KeyboardDevice::Update() {
    std::memcpy(m_PreviousKeyState, m_CurrentKeyState, sizeof(m_CurrentKeyState));
}

void KeyboardDevice::Reset() {
    std::memset(m_CurrentKeyState, 0, sizeof(m_CurrentKeyState));
    std::memset(m_PreviousKeyState, 0, sizeof(m_PreviousKeyState));
}

void KeyboardDevice::SetKeyState(KeyCode key, bool pressed) {
    size_t index = static_cast<size_t>(key);
    if (index < MAX_KEYS) {
        m_CurrentKeyState[index] = pressed;
    }
}

bool KeyboardDevice::IsKeyPressed(KeyCode key) const {
    size_t index = static_cast<size_t>(key);
    return index < MAX_KEYS ? m_CurrentKeyState[index] : false;
}

bool KeyboardDevice::IsKeyJustPressed(KeyCode key) const {
    size_t index = static_cast<size_t>(key);
    if (index >= MAX_KEYS) return false;
    return m_CurrentKeyState[index] && !m_PreviousKeyState[index];
}

bool KeyboardDevice::IsKeyJustReleased(KeyCode key) const {
    size_t index = static_cast<size_t>(key);
    if (index >= MAX_KEYS) return false;
    return !m_CurrentKeyState[index] && m_PreviousKeyState[index];
}

MouseDevice::MouseDevice() {
    std::memset(m_CurrentButtonState, 0, sizeof(m_CurrentButtonState));
    std::memset(m_PreviousButtonState, 0, sizeof(m_PreviousButtonState));
}

void MouseDevice::Update() {
    std::memcpy(m_PreviousButtonState, m_CurrentButtonState, sizeof(m_CurrentButtonState));
    m_PreviousPosition = m_Position;
    m_Delta = m_Position - m_PreviousPosition;
    m_ScrollDelta = glm::vec2(0.0f);
}

void MouseDevice::Reset() {
    std::memset(m_CurrentButtonState, 0, sizeof(m_CurrentButtonState));
    std::memset(m_PreviousButtonState, 0, sizeof(m_PreviousButtonState));
    m_Position = glm::vec2(0.0f);
    m_PreviousPosition = glm::vec2(0.0f);
    m_Delta = glm::vec2(0.0f);
    m_ScrollDelta = glm::vec2(0.0f);
}

void MouseDevice::SetButtonState(MouseButton button, bool pressed) {
    size_t index = static_cast<size_t>(button);
    if (index < MAX_BUTTONS) {
        m_CurrentButtonState[index] = pressed;
    }
}

bool MouseDevice::IsButtonPressed(MouseButton button) const {
    size_t index = static_cast<size_t>(button);
    return index < MAX_BUTTONS ? m_CurrentButtonState[index] : false;
}

bool MouseDevice::IsButtonJustPressed(MouseButton button) const {
    size_t index = static_cast<size_t>(button);
    if (index >= MAX_BUTTONS) return false;
    return m_CurrentButtonState[index] && !m_PreviousButtonState[index];
}

bool MouseDevice::IsButtonJustReleased(MouseButton button) const {
    size_t index = static_cast<size_t>(button);
    if (index >= MAX_BUTTONS) return false;
    return !m_CurrentButtonState[index] && m_PreviousButtonState[index];
}

void MouseDevice::SetPosition(const glm::vec2& position) {
    m_Position = position;
}

void MouseDevice::SetScrollDelta(const glm::vec2& delta) {
    m_ScrollDelta = delta;
}

GamepadDevice::GamepadDevice(uint32_t deviceID)
    : m_DeviceID(deviceID) {
    std::memset(m_CurrentButtonState, 0, sizeof(m_CurrentButtonState));
    std::memset(m_PreviousButtonState, 0, sizeof(m_PreviousButtonState));
    std::memset(m_AxisValues, 0, sizeof(m_AxisValues));
}

std::string GamepadDevice::GetDeviceName() const {
    return "Gamepad " + std::to_string(m_DeviceID);
}

void GamepadDevice::Update() {
    std::memcpy(m_PreviousButtonState, m_CurrentButtonState, sizeof(m_CurrentButtonState));
}

void GamepadDevice::Reset() {
    std::memset(m_CurrentButtonState, 0, sizeof(m_CurrentButtonState));
    std::memset(m_PreviousButtonState, 0, sizeof(m_PreviousButtonState));
    std::memset(m_AxisValues, 0, sizeof(m_AxisValues));
}

void GamepadDevice::SetConnected(bool connected) {
    m_Connected = connected;
    if (!connected) {
        Reset();
    }
}

void GamepadDevice::SetButtonState(GamepadButton button, bool pressed) {
    size_t index = static_cast<size_t>(button);
    if (index < MAX_BUTTONS) {
        m_CurrentButtonState[index] = pressed;
    }
}

bool GamepadDevice::IsButtonPressed(GamepadButton button) const {
    size_t index = static_cast<size_t>(button);
    return index < MAX_BUTTONS ? m_CurrentButtonState[index] : false;
}

bool GamepadDevice::IsButtonJustPressed(GamepadButton button) const {
    size_t index = static_cast<size_t>(button);
    if (index >= MAX_BUTTONS) return false;
    return m_CurrentButtonState[index] && !m_PreviousButtonState[index];
}

bool GamepadDevice::IsButtonJustReleased(GamepadButton button) const {
    size_t index = static_cast<size_t>(button);
    if (index >= MAX_BUTTONS) return false;
    return !m_CurrentButtonState[index] && m_PreviousButtonState[index];
}

void GamepadDevice::SetAxisValue(GamepadAxis axis, float value) {
    size_t index = static_cast<size_t>(axis);
    if (index < MAX_AXES) {
        m_AxisValues[index] = ApplyDeadzone(value);
    }
}

float GamepadDevice::GetAxisValue(GamepadAxis axis) const {
    size_t index = static_cast<size_t>(axis);
    return index < MAX_AXES ? m_AxisValues[index] : 0.0f;
}

void GamepadDevice::SetVibration(float leftMotor, float rightMotor) {

}

float GamepadDevice::ApplyDeadzone(float value) const {
    if (std::abs(value) < m_Deadzone) {
        return 0.0f;
    }

    float sign = value > 0.0f ? 1.0f : -1.0f;
    float absValue = std::abs(value);
    return sign * ((absValue - m_Deadzone) / (1.0f - m_Deadzone));
}

}
}
