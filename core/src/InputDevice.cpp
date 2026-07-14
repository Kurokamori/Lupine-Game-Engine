#include "lupine/input/InputDevice.hpp"
#include "lupine/input/InputManager.hpp"
#include <cstring>
#include <algorithm>

namespace lupine {
namespace input {

KeyboardDevice::KeyboardDevice() {
    std::memset(m_CurrentKeyState, 0, sizeof(m_CurrentKeyState));
    std::memset(m_PreviousKeyState, 0, sizeof(m_PreviousKeyState));
}

void KeyboardDevice::Update() {
    // Begin-of-frame: snapshot previous key state. Typed text is NOT cleared here:
    // InputManager::Update captures and clears it after the event pump, so clearing it
    // at frame start would race the platform thread under the async runtime and drop
    // characters.
    std::memcpy(m_PreviousKeyState, m_CurrentKeyState, sizeof(m_CurrentKeyState));
}

void KeyboardDevice::Reset() {
    std::memset(m_CurrentKeyState, 0, sizeof(m_CurrentKeyState));
    std::memset(m_PreviousKeyState, 0, sizeof(m_PreviousKeyState));
    std::lock_guard<std::mutex> lock(m_TextMutex);
    m_TextInput.clear();
}

void KeyboardDevice::AddTextInput(uint32_t codepoint) {
    std::lock_guard<std::mutex> lock(m_TextMutex);
    m_TextInput.push_back(codepoint);
}

void KeyboardDevice::ClearTextInput() {
    std::lock_guard<std::mutex> lock(m_TextMutex);
    m_TextInput.clear();
}

void KeyboardDevice::DrainTextInput(std::vector<uint32_t>& out) {
    std::lock_guard<std::mutex> lock(m_TextMutex);
    out = m_TextInput;
    m_TextInput.clear();
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
    // Begin-of-frame: snapshot previous button state. The scroll delta and position
    // delta are intentionally NOT touched here: InputManager::Update captures and
    // clears the scroll delta, and FinalizeFrame() computes the position delta, both
    // after the new state has been polled. Clearing the scroll here would race the
    // platform thread under the async runtime and drop wheel input.
    std::memcpy(m_PreviousButtonState, m_CurrentButtonState, sizeof(m_CurrentButtonState));
}

void MouseDevice::FinalizeFrame() {
    // Compute movement since the previous finalized frame, then advance the baseline.
    m_Delta = m_Position - m_PreviousPosition;
    m_PreviousPosition = m_Position;
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
    if (!m_Name.empty()) {
        return m_Name;
    }
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
    // Rumble is platform-specific; route through the InputManager's installed
    // vibration provider (no-op if the platform layer has not installed one).
    InputManager::Get().SetGamepadVibration(m_DeviceID, leftMotor, rightMotor, 0);
}

float GamepadDevice::ApplyDeadzone(float value) const {
    if (std::abs(value) < m_Deadzone) {
        return 0.0f;
    }

    float sign = value > 0.0f ? 1.0f : -1.0f;
    float absValue = std::abs(value);
    return sign * ((absValue - m_Deadzone) / (1.0f - m_Deadzone));
}

// ============================================================================
// TouchDevice implementation
// ============================================================================

TouchDevice::TouchDevice() {
    // Initialize all touch points
    for (size_t i = 0; i < MAX_TOUCH_POINTS; ++i) {
        m_TouchPoints[i] = TouchPoint();
    }
}

void TouchDevice::Update() {
    // Clear just started/ended flags for next frame
    for (size_t i = 0; i < MAX_TOUCH_POINTS; ++i) {
        m_TouchPoints[i].justStarted = false;
        m_TouchPoints[i].justEnded = false;
        m_TouchPoints[i].delta = glm::vec2(0.0f);
    }
}

void TouchDevice::Reset() {
    for (size_t i = 0; i < MAX_TOUCH_POINTS; ++i) {
        m_TouchPoints[i] = TouchPoint();
    }
}

void TouchDevice::BeginTouch(uint32_t id, const glm::vec2& position, float pressure) {
    TouchPoint* touch = FindTouchByID(id);
    if (!touch) {
        touch = FindFreeTouchSlot();
    }

    if (touch) {
        touch->id = id;
        touch->position = position;
        touch->startPosition = position;
        touch->delta = glm::vec2(0.0f);
        touch->pressure = pressure;
        touch->active = true;
        touch->justStarted = true;
        touch->justEnded = false;
    }
}

void TouchDevice::UpdateTouch(uint32_t id, const glm::vec2& position, float pressure) {
    TouchPoint* touch = FindTouchByID(id);
    if (touch && touch->active) {
        touch->delta = position - touch->position;
        touch->position = position;
        touch->pressure = pressure;
    }
}

void TouchDevice::EndTouch(uint32_t id) {
    TouchPoint* touch = FindTouchByID(id);
    if (touch) {
        touch->active = false;
        touch->justEnded = true;
    }
}

void TouchDevice::CancelTouch(uint32_t id) {
    TouchPoint* touch = FindTouchByID(id);
    if (touch) {
        touch->active = false;
        touch->justEnded = true;
    }
}

size_t TouchDevice::GetActiveTouchCount() const {
    size_t count = 0;
    for (size_t i = 0; i < MAX_TOUCH_POINTS; ++i) {
        if (m_TouchPoints[i].active) {
            ++count;
        }
    }
    return count;
}

const TouchPoint* TouchDevice::GetTouch(size_t index) const {
    size_t currentIndex = 0;
    for (size_t i = 0; i < MAX_TOUCH_POINTS; ++i) {
        if (m_TouchPoints[i].active) {
            if (currentIndex == index) {
                return &m_TouchPoints[i];
            }
            ++currentIndex;
        }
    }
    return nullptr;
}

const TouchPoint* TouchDevice::GetTouchByID(uint32_t id) const {
    for (size_t i = 0; i < MAX_TOUCH_POINTS; ++i) {
        if (m_TouchPoints[i].id == id && m_TouchPoints[i].active) {
            return &m_TouchPoints[i];
        }
    }
    return nullptr;
}

glm::vec2 TouchDevice::GetPrimaryTouchPosition() const {
    const TouchPoint* primary = GetTouch(0);
    return primary ? primary->position : glm::vec2(0.0f);
}

bool TouchDevice::HasPrimaryTouch() const {
    return GetTouch(0) != nullptr;
}

bool TouchDevice::IsPrimaryTouchJustStarted() const {
    // Look for any touch that just started (could be first touch)
    for (size_t i = 0; i < MAX_TOUCH_POINTS; ++i) {
        if (m_TouchPoints[i].justStarted) {
            return true;
        }
    }
    return false;
}

bool TouchDevice::IsPrimaryTouchJustEnded() const {
    // Look for any touch that just ended
    for (size_t i = 0; i < MAX_TOUCH_POINTS; ++i) {
        if (m_TouchPoints[i].justEnded) {
            return true;
        }
    }
    return false;
}

TouchPoint* TouchDevice::FindTouchByID(uint32_t id) {
    for (size_t i = 0; i < MAX_TOUCH_POINTS; ++i) {
        if (m_TouchPoints[i].id == id) {
            return &m_TouchPoints[i];
        }
    }
    return nullptr;
}

TouchPoint* TouchDevice::FindFreeTouchSlot() {
    for (size_t i = 0; i < MAX_TOUCH_POINTS; ++i) {
        if (!m_TouchPoints[i].active && !m_TouchPoints[i].justEnded) {
            return &m_TouchPoints[i];
        }
    }
    return nullptr;
}

}
}
