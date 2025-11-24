#include "lupine/input/InputManager.hpp"
#include "lupine/logger/Logger.hpp"
#include <algorithm>
#include <cmath>

namespace lupine {
namespace input {

std::unique_ptr<InputManager> InputManager::s_Instance = nullptr;

InputManager::InputManager() {
    m_Keyboard = std::make_unique<KeyboardDevice>();
    m_Mouse = std::make_unique<MouseDevice>();

}

InputManager::~InputManager() {

}

InputManager& InputManager::Get() {
    if (!s_Instance) {
        s_Instance = std::make_unique<InputManager>();
    }
    return *s_Instance;
}

void InputManager::Initialize() {
    if (!s_Instance) {
        s_Instance = std::make_unique<InputManager>();
    }
}

void InputManager::Shutdown() {
    s_Instance.reset();
}

void InputManager::Update(float deltaTime) {

    m_Keyboard->Update();
    m_Mouse->Update();

    for (auto& [id, gamepad] : m_Gamepads) {
        if (gamepad->IsConnected()) {
            gamepad->Update();
        }
    }

    UpdateActionStates();
    UpdateAxisStates(deltaTime);

    ProcessCallbacks();
}

void InputManager::Reset() {
    m_Keyboard->Reset();
    m_Mouse->Reset();

    for (auto& [id, gamepad] : m_Gamepads) {
        gamepad->Reset();
    }

    m_ActionStates.clear();
    m_AxisStates.clear();
}

GamepadDevice* InputManager::GetGamepad(uint32_t id) {
    auto it = m_Gamepads.find(id);
    return it != m_Gamepads.end() ? it->second.get() : nullptr;
}

std::vector<GamepadDevice*> InputManager::GetConnectedGamepads() {
    std::vector<GamepadDevice*> result;
    for (auto& [id, gamepad] : m_Gamepads) {
        if (gamepad->IsConnected()) {
            result.push_back(gamepad.get());
        }
    }
    return result;
}

void InputManager::RegisterGamepad(uint32_t id) {
    if (m_Gamepads.find(id) == m_Gamepads.end()) {
        m_Gamepads[id] = std::make_unique<GamepadDevice>(id);
        m_Gamepads[id]->SetConnected(true);
        m_Gamepads[id]->SetDeadzone(m_GlobalGamepadDeadzone);

    }
}

void InputManager::UnregisterGamepad(uint32_t id) {
    auto it = m_Gamepads.find(id);
    if (it != m_Gamepads.end()) {
        it->second->SetConnected(false);

    }
}

bool InputManager::IsKeyPressed(KeyCode key) const {
    return m_Keyboard->IsKeyPressed(key);
}

bool InputManager::IsKeyJustPressed(KeyCode key) const {
    return m_Keyboard->IsKeyJustPressed(key);
}

bool InputManager::IsKeyJustReleased(KeyCode key) const {
    return m_Keyboard->IsKeyJustReleased(key);
}

bool InputManager::IsMouseButtonPressed(MouseButton button) const {
    return m_Mouse->IsButtonPressed(button);
}

bool InputManager::IsMouseButtonJustPressed(MouseButton button) const {
    return m_Mouse->IsButtonJustPressed(button);
}

bool InputManager::IsMouseButtonJustReleased(MouseButton button) const {
    return m_Mouse->IsButtonJustReleased(button);
}

glm::vec2 InputManager::GetMousePosition() const {
    return m_Mouse->GetPosition();
}

glm::vec2 InputManager::GetMousePositionFlippedY() const {

    glm::vec2 pos = m_Mouse->GetPosition();
    return glm::vec2(pos.x, static_cast<float>(m_WindowHeight) - pos.y);
}

glm::vec2 InputManager::GetMouseDelta() const {
    return m_Mouse->GetDelta();
}

glm::vec2 InputManager::GetMouseScrollDelta() const {
    return m_Mouse->GetScrollDelta();
}

void InputManager::SetWindowSize(int width, int height) {
    m_WindowWidth = width;
    m_WindowHeight = height;
}

glm::ivec2 InputManager::GetWindowSize() const {
    return glm::ivec2(m_WindowWidth, m_WindowHeight);
}

bool InputManager::IsGamepadButtonPressed(GamepadButton button, uint32_t gamepadID) const {
    auto it = m_Gamepads.find(gamepadID);
    if (it != m_Gamepads.end() && it->second->IsConnected()) {
        return it->second->IsButtonPressed(button);
    }
    return false;
}

bool InputManager::IsGamepadButtonJustPressed(GamepadButton button, uint32_t gamepadID) const {
    auto it = m_Gamepads.find(gamepadID);
    if (it != m_Gamepads.end() && it->second->IsConnected()) {
        return it->second->IsButtonJustPressed(button);
    }
    return false;
}

bool InputManager::IsGamepadButtonJustReleased(GamepadButton button, uint32_t gamepadID) const {
    auto it = m_Gamepads.find(gamepadID);
    if (it != m_Gamepads.end() && it->second->IsConnected()) {
        return it->second->IsButtonJustReleased(button);
    }
    return false;
}

float InputManager::GetGamepadAxis(GamepadAxis axis, uint32_t gamepadID) const {
    auto it = m_Gamepads.find(gamepadID);
    if (it != m_Gamepads.end() && it->second->IsConnected()) {
        return it->second->GetAxisValue(axis);
    }
    return 0.0f;
}

bool InputManager::IsActionPressed(const std::string& actionName) const {
    auto it = m_ActionStates.find(actionName);
    return it != m_ActionStates.end() && it->second.currentFrame;
}

bool InputManager::IsActionJustPressed(const std::string& actionName) const {
    auto it = m_ActionStates.find(actionName);
    if (it == m_ActionStates.end()) return false;
    return it->second.currentFrame && !it->second.previousFrame;
}

bool InputManager::IsActionJustReleased(const std::string& actionName) const {
    auto it = m_ActionStates.find(actionName);
    if (it == m_ActionStates.end()) return false;
    return !it->second.currentFrame && it->second.previousFrame;
}

float InputManager::GetAxisValue(const std::string& axisName) const {
    auto it = m_AxisStates.find(axisName);
    return it != m_AxisStates.end() ? it->second.currentValue : 0.0f;
}

float InputManager::GetAxisValueRaw(const std::string& axisName) const {
    const InputAxis* axis = m_InputMap.GetAxis(axisName);
    if (!axis) return 0.0f;

    float value = 0.0f;
    for (const auto& binding : axis->GetBindings()) {
        value += EvaluateBindingAxis(binding);
    }

    return std::clamp(value, -1.0f, 1.0f);
}

bool InputManager::LoadInputMap(const std::string& filepath) {
    return m_InputMap.LoadFromFile(filepath);
}

bool InputManager::SaveInputMap(const std::string& filepath) const {
    return m_InputMap.SaveToFile(filepath);
}

void InputManager::AddAction(const InputAction& action) {
    m_InputMap.AddAction(action);
}

void InputManager::AddAxis(const InputAxis& axis) {
    m_InputMap.AddAxis(axis);
}

void InputManager::RemoveAction(const std::string& name) {
    m_InputMap.RemoveAction(name);
    m_ActionStates.erase(name);
}

void InputManager::RemoveAxis(const std::string& name) {
    m_InputMap.RemoveAxis(name);
    m_AxisStates.erase(name);
}

void InputManager::RegisterActionCallback(const std::string& actionName, ActionCallback callback) {
    m_ActionCallbacks[actionName] = callback;
}

void InputManager::RegisterAxisCallback(const std::string& axisName, AxisCallback callback) {
    m_AxisCallbacks[axisName] = callback;
}

void InputManager::UnregisterActionCallback(const std::string& actionName) {
    m_ActionCallbacks.erase(actionName);
}

void InputManager::UnregisterAxisCallback(const std::string& axisName) {
    m_AxisCallbacks.erase(axisName);
}

void InputManager::SetGlobalGamepadDeadzone(float deadzone) {
    m_GlobalGamepadDeadzone = deadzone;
    for (auto& [id, gamepad] : m_Gamepads) {
        gamepad->SetDeadzone(deadzone);
    }
}

void InputManager::SetMouseCursorVisible(bool visible) {
    m_MouseCursorVisible = visible;

}

bool InputManager::EvaluateBinding(const InputBinding& binding) const {
    switch (binding.deviceType) {
        case InputDeviceType::Keyboard:
            return IsKeyPressed(binding.keyCode);

        case InputDeviceType::Mouse:
            return IsMouseButtonPressed(binding.mouseButton);

        case InputDeviceType::Gamepad: {

            if (binding.gamepadButton != GamepadButton::Unknown) {
                if (binding.gamepadID == 0) {

                    for (const auto& [id, gamepad] : m_Gamepads) {
                        if (gamepad->IsConnected() && gamepad->IsButtonPressed(binding.gamepadButton)) {
                            return true;
                        }
                    }
                    return false;
                } else {
                    return IsGamepadButtonPressed(binding.gamepadButton, binding.gamepadID);
                }
            }

            if (binding.gamepadAxis != GamepadAxis::Unknown) {
                float axisValue = 0.0f;
                if (binding.gamepadID == 0) {

                    for (const auto& [id, gamepad] : m_Gamepads) {
                        if (gamepad->IsConnected()) {
                            axisValue = gamepad->GetAxisValue(binding.gamepadAxis);
                            if (std::abs(axisValue) > 0.5f) break;
                        }
                    }
                } else {
                    axisValue = GetGamepadAxis(binding.gamepadAxis, binding.gamepadID);
                }

                if (binding.scale > 0.0f) {
                    return axisValue > 0.5f;
                } else {
                    return axisValue < -0.5f;
                }
            }
            break;
        }

        default:
            break;
    }

    return false;
}

float InputManager::EvaluateBindingAxis(const InputBinding& binding) const {
    switch (binding.deviceType) {
        case InputDeviceType::Keyboard: {

            bool pressed = IsKeyPressed(binding.keyCode);
            return pressed ? binding.scale : 0.0f;
        }

        case InputDeviceType::Mouse: {
            bool pressed = IsMouseButtonPressed(binding.mouseButton);
            return pressed ? binding.scale : 0.0f;
        }

        case InputDeviceType::Gamepad: {

            if (binding.gamepadButton != GamepadButton::Unknown) {
                bool pressed = false;
                if (binding.gamepadID == 0) {
                    for (const auto& [id, gamepad] : m_Gamepads) {
                        if (gamepad->IsConnected() && gamepad->IsButtonPressed(binding.gamepadButton)) {
                            pressed = true;
                            break;
                        }
                    }
                } else {
                    pressed = IsGamepadButtonPressed(binding.gamepadButton, binding.gamepadID);
                }
                return pressed ? binding.scale : 0.0f;
            }

            if (binding.gamepadAxis != GamepadAxis::Unknown) {
                float axisValue = 0.0f;
                if (binding.gamepadID == 0) {

                    for (const auto& [id, gamepad] : m_Gamepads) {
                        if (gamepad->IsConnected()) {
                            axisValue = gamepad->GetAxisValue(binding.gamepadAxis);
                            if (std::abs(axisValue) > 0.01f) break;
                        }
                    }
                } else {
                    axisValue = GetGamepadAxis(binding.gamepadAxis, binding.gamepadID);
                }
                return axisValue * binding.scale;
            }
            break;
        }

        default:
            break;
    }

    return 0.0f;
}

void InputManager::UpdateActionStates() {

    for (const auto& action : m_InputMap.GetAllActions()) {
        const std::string& name = action.GetName();

        auto& state = m_ActionStates[name];
        state.previousFrame = state.currentFrame;

        state.currentFrame = false;
        for (const auto& binding : action.GetBindings()) {
            if (EvaluateBinding(binding)) {
                state.currentFrame = true;
                break;
            }
        }
    }
}

void InputManager::UpdateAxisStates(float deltaTime) {

    for (const auto& axis : m_InputMap.GetAllAxes()) {
        const std::string& name = axis.GetName();

        auto& state = m_AxisStates[name];

        float targetValue = 0.0f;
        for (const auto& binding : axis.GetBindings()) {
            targetValue += EvaluateBindingAxis(binding);
        }
        targetValue = std::clamp(targetValue, -1.0f, 1.0f);

        if (std::abs(targetValue) < axis.GetDeadzone()) {
            targetValue = 0.0f;
        }

        if (axis.GetSnap() &&
            ((state.currentValue > 0.0f && targetValue < 0.0f) ||
             (state.currentValue < 0.0f && targetValue > 0.0f))) {
            state.currentValue = 0.0f;
        }

        if (targetValue != 0.0f) {

            float delta = targetValue * axis.GetSensitivity() * deltaTime;
            if (std::abs(targetValue - state.currentValue) < std::abs(delta)) {
                state.currentValue = targetValue;
            } else {
                state.currentValue += delta * (targetValue > state.currentValue ? 1.0f : -1.0f);
            }
        } else {

            if (std::abs(state.currentValue) > 0.0f) {
                float delta = axis.GetGravity() * deltaTime;
                if (std::abs(state.currentValue) < delta) {
                    state.currentValue = 0.0f;
                } else {
                    state.currentValue -= delta * (state.currentValue > 0.0f ? 1.0f : -1.0f);
                }
            }
        }

        state.targetValue = targetValue;
    }
}

void InputManager::ProcessCallbacks() {

    for (const auto& [actionName, callback] : m_ActionCallbacks) {
        auto it = m_ActionStates.find(actionName);
        if (it != m_ActionStates.end()) {
            if (it->second.currentFrame != it->second.previousFrame) {
                callback(actionName, it->second.currentFrame);
            }
        }
    }

    for (const auto& [axisName, callback] : m_AxisCallbacks) {
        auto it = m_AxisStates.find(axisName);
        if (it != m_AxisStates.end()) {
            callback(axisName, it->second.currentValue);
        }
    }
}

}
}
