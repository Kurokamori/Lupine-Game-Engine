#include "lupine/input/InputManager.hpp"
#include "lupine/core/EventBus.hpp"
#include "lupine/logger/Logger.hpp"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <fstream>
#include <sstream>

namespace lupine {
namespace input {

std::unique_ptr<InputManager> InputManager::s_Instance = nullptr;

InputManager::InputManager() {
    m_Keyboard = std::make_unique<KeyboardDevice>();
    m_Mouse = std::make_unique<MouseDevice>();
    m_Touch = std::make_unique<TouchDevice>();

    // Enable touch on platforms that support it
#if defined(__EMSCRIPTEN__) || defined(__ANDROID__) || (defined(__APPLE__) && (TARGET_OS_IOS || TARGET_OS_IPHONE))
    m_Touch->SetAvailable(true);
#endif
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

const std::vector<uint32_t>& InputManager::GetTextInput() const {
    return m_FrameText;
}

void InputManager::SetClipboardProvider(ClipboardGetter getter, ClipboardSetter setter) {
    m_ClipboardGetter = std::move(getter);
    m_ClipboardSetter = std::move(setter);
}

std::string InputManager::GetClipboardText() const {
    if (m_ClipboardGetter) {
        return m_ClipboardGetter();
    }
    return std::string();
}

void InputManager::SetClipboardText(const std::string& text) {
    if (m_ClipboardSetter) {
        m_ClipboardSetter(text);
    }
}

void InputManager::BeginFrame() {
    ++m_FrameNumber;

    static bool s_diagLoggedBeginFrame = false;
    if (!s_diagLoggedBeginFrame) {
        s_diagLoggedBeginFrame = true;
        LOG_INPUT_INFO("[diag] InputManager::BeginFrame is live (edge-detection ordering fix active)");
    }

    // Snapshot the previous-frame button/key states (for edge detection) and reset
    // per-frame deltas (mouse scroll, typed text) BEFORE the platform layer polls
    // this frame's events. Doing this after polling — as the old single Update() did
    // — left previous == current and the scroll delta zeroed by the time the scene
    // read input, so Is*JustPressed/Released and GetMouseScrollDelta always read as
    // empty. Edge-triggered widgets (sliders, dropdowns, scrollbar drag, etc.)
    // depend on this ordering; only continuously-polled input (held buttons) worked
    // without it.
    m_Keyboard->Update();
    m_Mouse->Update();
    m_Touch->Update();

    for (auto& [id, gamepad] : m_Gamepads) {
        if (gamepad->IsConnected()) {
            gamepad->Update();
        }
    }
}

void InputManager::Update(float deltaTime) {
    // The mouse position has now been polled for this frame (platform events +
    // continuous poll), so finalize the movement delta. This is the end-of-poll
    // counterpart to the begin-of-frame snapshot done in BeginFrame().
    m_Mouse->FinalizeFrame();

    // Derive per-frame button/key edges on this (consuming) thread by sampling the
    // held device state and diffing against the previous frame. Computing the
    // snapshot and the edge together here makes "just pressed/released" correct under
    // the async runtime: the platform thread can set the held state at any moment, and
    // a transition is still observed by exactly one frame. (The old device-level
    // snapshot in BeginFrame, read later in the scene's input pass, lost the edge
    // whenever the press landed in the update/render gap between the two.)
    for (size_t b = 0; b < kMouseButtonCount; ++b) {
        const bool held = m_Mouse->IsButtonPressed(static_cast<MouseButton>(b));
        m_MouseJustPressed[b]  = held && !m_MousePrevHeld[b];
        m_MouseJustReleased[b] = !held && m_MousePrevHeld[b];
        m_MousePrevHeld[b] = held;
    }
    for (size_t k = 0; k < kKeyCount; ++k) {
        const bool held = m_Keyboard->IsKeyPressed(static_cast<KeyCode>(k));
        m_KeyJustPressed[k]  = held && !m_KeyPrevHeld[k];
        m_KeyJustReleased[k] = !held && m_KeyPrevHeld[k];
        m_KeyPrevHeld[k] = held;
    }

    // Capture and consume this frame's scroll delta and typed text off the device, so
    // they survive to the scene read regardless of when the platform thread produced
    // them (the device is no longer cleared in BeginFrame, which would otherwise race
    // the producer under the async runtime).
    m_FrameScroll = m_Mouse->GetScrollDelta();
    m_Mouse->SetScrollDelta(glm::vec2(0.0f));
    m_Keyboard->DrainTextInput(m_FrameText);

    // Promote the events collected during this frame's event pump to the frame
    // event list (dispatched once by the scene tree), then clear the incoming
    // buffer so each event is delivered exactly once. Guard the swap so it does not
    // race the platform thread pushing into m_IncomingEvents under the async runtime.
    {
        std::lock_guard<std::mutex> lock(m_EventMutex);
        m_FrameEvents = std::move(m_IncomingEvents);
        m_IncomingEvents.clear();
    }

    // Derive active-device tracking, auto-join and capture from this frame's
    // discrete events on the consuming thread (safe to touch EventBus / scene here).
    ProcessFrameEvents();

    UpdateActionStates();
    UpdateAxisStates(deltaTime);

    ProcessCallbacks();
}

void InputManager::PushInputEvent(const InputEvent& event) {
    std::lock_guard<std::mutex> lock(m_EventMutex);
    m_IncomingEvents.push_back(event);
}

void InputManager::SetVibrationProvider(VibrationProvider provider) {
    m_VibrationProvider = std::move(provider);
}

void InputManager::Reset() {
    m_Keyboard->Reset();
    m_Mouse->Reset();
    m_Touch->Reset();

    for (auto& [id, gamepad] : m_Gamepads) {
        gamepad->Reset();
    }

    m_ActionStates.clear();
    m_AxisStates.clear();

    m_Capturing = false;
    m_CaptureComplete = false;
    m_LastUsedDeviceType = InputDeviceType::Unknown;
    m_LastUsedGamepadID = 0;
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
    const size_t idx = static_cast<size_t>(key);
    return idx < kKeyCount && m_KeyJustPressed[idx];
}

bool InputManager::IsKeyJustReleased(KeyCode key) const {
    const size_t idx = static_cast<size_t>(key);
    return idx < kKeyCount && m_KeyJustReleased[idx];
}

bool InputManager::IsActionModifierPressed() const {
    // Platform-agnostic action modifier:
    // - Windows/Linux: Control
    // - Mac: Command (Super)
#ifdef __APPLE__
    return m_Keyboard->IsKeyPressed(KeyCode::LeftSuper) ||
           m_Keyboard->IsKeyPressed(KeyCode::RightSuper);
#else
    return m_Keyboard->IsKeyPressed(KeyCode::LeftControl) ||
           m_Keyboard->IsKeyPressed(KeyCode::RightControl);
#endif
}

bool InputManager::IsShiftPressed() const {
    return m_Keyboard->IsKeyPressed(KeyCode::LeftShift) ||
           m_Keyboard->IsKeyPressed(KeyCode::RightShift);
}

bool InputManager::IsAltPressed() const {
    return m_Keyboard->IsKeyPressed(KeyCode::LeftAlt) ||
           m_Keyboard->IsKeyPressed(KeyCode::RightAlt);
}

bool InputManager::IsControlPressed() const {
    return m_Keyboard->IsKeyPressed(KeyCode::LeftControl) ||
           m_Keyboard->IsKeyPressed(KeyCode::RightControl);
}

bool InputManager::IsSuperPressed() const {
    return m_Keyboard->IsKeyPressed(KeyCode::LeftSuper) ||
           m_Keyboard->IsKeyPressed(KeyCode::RightSuper);
}

bool InputManager::IsMouseButtonPressed(MouseButton button) const {
    return m_Mouse->IsButtonPressed(button);
}

bool InputManager::IsMouseButtonJustPressed(MouseButton button) const {
    const size_t idx = static_cast<size_t>(button);
    return idx < kMouseButtonCount && m_MouseJustPressed[idx];
}

bool InputManager::IsMouseButtonJustReleased(MouseButton button) const {
    const size_t idx = static_cast<size_t>(button);
    return idx < kMouseButtonCount && m_MouseJustReleased[idx];
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
    return m_FrameScroll;
}

void InputManager::SetWindowSize(int width, int height) {
    m_WindowWidth = width;
    m_WindowHeight = height;
}

glm::ivec2 InputManager::GetWindowSize() const {
    return glm::ivec2(m_WindowWidth, m_WindowHeight);
}

void InputManager::SetContentScale(float scaleX, float scaleY) {
    m_ContentScale = glm::vec2(scaleX, scaleY);
}

void InputManager::SetDPIScale(float scale) {
    m_DPIScale = scale;
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

size_t InputManager::GetGamepadCount() const {
    size_t count = 0;
    for (const auto& [id, gamepad] : m_Gamepads) {
        if (gamepad->IsConnected()) {
            ++count;
        }
    }
    return count;
}

bool InputManager::IsGamepadConnected(uint32_t gamepadID) const {
    auto it = m_Gamepads.find(gamepadID);
    return it != m_Gamepads.end() && it->second->IsConnected();
}

std::vector<uint32_t> InputManager::GetConnectedGamepadIDs() const {
    std::vector<uint32_t> ids;
    for (const auto& [id, gamepad] : m_Gamepads) {
        if (gamepad->IsConnected()) {
            ids.push_back(id);
        }
    }
    return ids;
}

std::string InputManager::GetGamepadName(uint32_t gamepadID) const {
    auto it = m_Gamepads.find(gamepadID);
    if (it != m_Gamepads.end() && it->second->IsConnected()) {
        return it->second->GetDeviceName();
    }
    return std::string();
}

void InputManager::SetGamepadVibration(uint32_t gamepadID, float leftMotor, float rightMotor, uint32_t durationMs) {
    if (m_VibrationProvider) {
        m_VibrationProvider(gamepadID,
                            std::clamp(leftMotor, 0.0f, 1.0f),
                            std::clamp(rightMotor, 0.0f, 1.0f),
                            durationMs);
    }
}

void InputManager::StopGamepadVibration(uint32_t gamepadID) {
    if (m_VibrationProvider) {
        m_VibrationProvider(gamepadID, 0.0f, 0.0f, 0);
    }
}

// ============================================================================
// Touch input queries
// ============================================================================

bool InputManager::IsTouchAvailable() const {
    return m_Touch->IsAvailable();
}

bool InputManager::IsTouching() const {
    return m_Touch->IsTouching();
}

size_t InputManager::GetTouchCount() const {
    return m_Touch->GetActiveTouchCount();
}

glm::vec2 InputManager::GetTouchPosition(size_t index) const {
    const TouchPoint* touch = m_Touch->GetTouch(index);
    return touch ? touch->position : glm::vec2(0.0f);
}

bool InputManager::IsTouchJustStarted() const {
    return m_Touch->IsPrimaryTouchJustStarted();
}

bool InputManager::IsTouchJustEnded() const {
    return m_Touch->IsPrimaryTouchJustEnded();
}

bool InputManager::IsActionPressed(const std::string& actionName) const {
    return IsActionPressed(actionName, -1);
}

bool InputManager::IsActionJustPressed(const std::string& actionName) const {
    return IsActionJustPressed(actionName, -1);
}

bool InputManager::IsActionJustReleased(const std::string& actionName) const {
    return IsActionJustReleased(actionName, -1);
}

bool InputManager::IsActionPressed(const std::string& actionName, int playerIndex) const {
    auto it = m_ActionStates.find(StateKey(playerIndex, actionName));
    return it != m_ActionStates.end() && it->second.currentFrame;
}

bool InputManager::IsActionJustPressed(const std::string& actionName, int playerIndex) const {
    auto it = m_ActionStates.find(StateKey(playerIndex, actionName));
    if (it == m_ActionStates.end()) return false;
    return it->second.currentFrame && !it->second.previousFrame;
}

bool InputManager::IsActionJustReleased(const std::string& actionName, int playerIndex) const {
    auto it = m_ActionStates.find(StateKey(playerIndex, actionName));
    if (it == m_ActionStates.end()) return false;
    return !it->second.currentFrame && it->second.previousFrame;
}

float InputManager::GetAxisValue(const std::string& axisName) const {
    return GetAxisValue(axisName, -1);
}

float InputManager::GetAxisValue(const std::string& axisName, int playerIndex) const {
    auto it = m_AxisStates.find(StateKey(playerIndex, axisName));
    return it != m_AxisStates.end() ? it->second.currentValue : 0.0f;
}

float InputManager::GetAxisValueRaw(const std::string& axisName) const {
    const InputAxis* axis = m_InputMap.GetAxis(axisName);
    if (!axis || !IsAxisLive(*axis)) return 0.0f;

    float value = 0.0f;
    for (const auto& binding : axis->GetBindings()) {
        value += EvaluateBindingAxis(binding);
    }

    return std::clamp(value, -1.0f, 1.0f);
}

float InputManager::GetActionStrength(const std::string& actionName) const {
    return GetActionStrength(actionName, -1);
}

float InputManager::GetActionStrength(const std::string& actionName, int playerIndex) const {
    const InputAction* action = m_InputMap.GetAction(actionName);
    if (!action || !IsActionLive(*action)) return 0.0f;

    float strength = 0.0f;
    for (const InputBinding& binding : action->GetBindings()) {
        if (!BindingBelongsToPlayer(binding, playerIndex)) {
            continue;
        }

        float value = 0.0f;

        // Analog gamepad axis/trigger bindings contribute their magnitude (after
        // the binding's scale); every other binding is digital (0 or 1).
        if (binding.deviceType == InputDeviceType::Gamepad &&
            binding.gamepadButton == GamepadButton::Unknown &&
            binding.gamepadAxis != GamepadAxis::Unknown) {
            float axisValue = 0.0f;
            if (binding.gamepadID == 0) {
                for (const auto& [id, gamepad] : m_Gamepads) {
                    if (gamepad->IsConnected() &&
                        (playerIndex < 0 || GetPlayerForGamepad(id) == playerIndex)) {
                        float v = gamepad->GetAxisValue(binding.gamepadAxis);
                        if (std::abs(v) > std::abs(axisValue)) {
                            axisValue = v;
                        }
                    }
                }
            } else {
                axisValue = GetGamepadAxis(binding.gamepadAxis, binding.gamepadID);
            }
            value = std::abs(axisValue * binding.scale);
        } else {
            value = EvaluateBinding(binding, playerIndex) ? 1.0f : 0.0f;
        }

        strength = std::max(strength, value);
    }

    if (strength < action->GetDeadzone()) {
        return 0.0f;
    }
    return std::min(strength, 1.0f);
}

bool InputManager::LoadInputMap(const std::string& filepath) {
    return m_InputMap.LoadFromFile(filepath);
}

bool InputManager::LoadInputMapFromString(const std::string& jsonString) {
    return m_InputMap.LoadFromString(jsonString);
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
    // Erase the global state and every per-player state ("<player>\x1f<name>").
    const std::string suffix = std::string("\x1f") + name;
    for (auto it = m_ActionStates.begin(); it != m_ActionStates.end();) {
        if (it->first == name ||
            (it->first.size() >= suffix.size() &&
             it->first.compare(it->first.size() - suffix.size(), suffix.size(), suffix) == 0)) {
            it = m_ActionStates.erase(it);
        } else {
            ++it;
        }
    }
}

void InputManager::RemoveAxis(const std::string& name) {
    m_InputMap.RemoveAxis(name);
    const std::string suffix = std::string("\x1f") + name;
    for (auto it = m_AxisStates.begin(); it != m_AxisStates.end();) {
        if (it->first == name ||
            (it->first.size() >= suffix.size() &&
             it->first.compare(it->first.size() - suffix.size(), suffix.size(), suffix) == 0)) {
            it = m_AxisStates.erase(it);
        } else {
            ++it;
        }
    }
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

bool InputManager::EvaluateBinding(const InputBinding& binding, int playerIndex) const {
    if (!BindingBelongsToPlayer(binding, playerIndex)) {
        return false;
    }

    switch (binding.deviceType) {
        case InputDeviceType::Keyboard:
            return IsKeyPressed(binding.keyCode);

        case InputDeviceType::Mouse:
            return IsMouseButtonPressed(binding.mouseButton);

        case InputDeviceType::Gamepad: {

            if (binding.gamepadButton != GamepadButton::Unknown) {
                if (binding.gamepadID == 0) {

                    for (const auto& [id, gamepad] : m_Gamepads) {
                        if (gamepad->IsConnected() &&
                            (playerIndex < 0 || GetPlayerForGamepad(id) == playerIndex) &&
                            gamepad->IsButtonPressed(binding.gamepadButton)) {
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
                        if (gamepad->IsConnected() &&
                            (playerIndex < 0 || GetPlayerForGamepad(id) == playerIndex)) {
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

float InputManager::EvaluateBindingAxis(const InputBinding& binding, int playerIndex) const {
    if (!BindingBelongsToPlayer(binding, playerIndex)) {
        return 0.0f;
    }

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
                        if (gamepad->IsConnected() &&
                            (playerIndex < 0 || GetPlayerForGamepad(id) == playerIndex) &&
                            gamepad->IsButtonPressed(binding.gamepadButton)) {
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
                        if (gamepad->IsConnected() &&
                            (playerIndex < 0 || GetPlayerForGamepad(id) == playerIndex)) {
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
    core::EventBus& bus = core::EventBus::Get();

    // Evaluate the global view (player -1) plus each configured player slot.
    std::vector<int> players;
    players.push_back(-1);
    for (int p = 0; p < static_cast<int>(m_Players.size()); ++p) {
        players.push_back(p);
    }

    for (const auto& action : m_InputMap.GetAllActions()) {
        const std::string& name = action.GetName();
        const bool live = IsActionLive(action);

        for (int player : players) {
            auto& state = m_ActionStates[StateKey(player, name)];
            state.previousFrame = state.currentFrame;

            bool pressed = false;
            if (live) {
                for (const auto& binding : action.GetBindings()) {
                    if (EvaluateBinding(binding, player)) {
                        pressed = true;
                        break;
                    }
                }
            }
            state.currentFrame = pressed;

            // Delegation: fire EventBus events on a press/release transition so
            // scripts can connect functions to actions (see ScriptAPI::ConnectInputAction).
            if (state.currentFrame != state.previousFrame) {
                float strength = GetActionStrength(name, player);
                bus.Emit("action:" + name, core::SignalArgs{
                    nlohmann::json(state.currentFrame),
                    nlohmann::json(strength),
                    nlohmann::json(player),
                    nlohmann::json(static_cast<int>(m_LastUsedDeviceType))
                });
                bus.Emit(state.currentFrame ? "input_action_pressed" : "input_action_released",
                         core::SignalArgs{
                             nlohmann::json(name),
                             nlohmann::json(player)
                         });
            }
        }
    }
}

void InputManager::UpdateAxisStates(float deltaTime) {
    std::vector<int> players;
    players.push_back(-1);
    for (int p = 0; p < static_cast<int>(m_Players.size()); ++p) {
        players.push_back(p);
    }

    for (const auto& axis : m_InputMap.GetAllAxes()) {
        const std::string& name = axis.GetName();
        const bool live = IsAxisLive(axis);

        for (int player : players) {
            auto& state = m_AxisStates[StateKey(player, name)];

            float targetValue = 0.0f;
            if (live) {
                for (const auto& binding : axis.GetBindings()) {
                    targetValue += EvaluateBindingAxis(binding, player);
                }
                targetValue = std::clamp(targetValue, -1.0f, 1.0f);

                if (std::abs(targetValue) < axis.GetDeadzone()) {
                    targetValue = 0.0f;
                }
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

// ============================================================================
// Per-(player,name) state keys, context/player gating helpers
// ============================================================================

std::string InputManager::StateKey(int playerIndex, const std::string& name) {
    if (playerIndex < 0) {
        return name;
    }
    return std::to_string(playerIndex) + "\x1f" + name;
}

bool InputManager::IsActionLive(const InputAction& action) const {
    return action.IsEnabled() && IsContextActive(action.GetContext());
}

bool InputManager::IsAxisLive(const InputAxis& axis) const {
    return axis.IsEnabled() && IsContextActive(axis.GetContext());
}

bool InputManager::BindingBelongsToPlayer(const InputBinding& binding, int playerIndex) const {
    if (playerIndex < 0) {
        return true;
    }
    if (playerIndex >= static_cast<int>(m_Players.size())) {
        return false;
    }
    const PlayerSlot& slot = m_Players[playerIndex];
    switch (binding.deviceType) {
        case InputDeviceType::Keyboard:
        case InputDeviceType::Mouse:
            return slot.ownsKeyboardMouse;
        case InputDeviceType::Gamepad:
            if (binding.gamepadID == 0) {
                return !slot.gamepadIDs.empty();
            }
            return std::find(slot.gamepadIDs.begin(), slot.gamepadIDs.end(),
                             binding.gamepadID) != slot.gamepadIDs.end();
        default:
            return false;
    }
}

// ============================================================================
// Active device tracking
// ============================================================================

GamepadType InputManager::GetGamepadType(uint32_t gamepadID) const {
    auto it = m_Gamepads.find(gamepadID);
    if (it != m_Gamepads.end()) {
        return it->second->GetGamepadType();
    }
    return GamepadType::Unknown;
}

void InputManager::HandleDeviceUsed(InputDeviceType type, uint32_t gamepadID) {
    // Auto-join: bind a not-yet-assigned device to the next free player slot.
    if (m_AutoJoin && !m_Players.empty()) {
        if (type == InputDeviceType::Gamepad) {
            if (GetPlayerForGamepad(gamepadID) < 0) {
                for (int p = 0; p < static_cast<int>(m_Players.size()); ++p) {
                    if (m_Players[p].gamepadIDs.empty()) {
                        AssignGamepadToPlayer(p, gamepadID);
                        break;
                    }
                }
            }
        } else if (type == InputDeviceType::Keyboard || type == InputDeviceType::Mouse) {
            if (GetPlayerForKeyboardMouse() < 0) {
                for (int p = 0; p < static_cast<int>(m_Players.size()); ++p) {
                    if (!m_Players[p].ownsKeyboardMouse) {
                        AssignKeyboardMouseToPlayer(p);
                        break;
                    }
                }
            }
        }
    }

    bool changed = (type != m_LastUsedDeviceType) ||
                   (type == InputDeviceType::Gamepad && gamepadID != m_LastUsedGamepadID);
    m_LastUsedDeviceType = type;
    if (type == InputDeviceType::Gamepad) {
        m_LastUsedGamepadID = gamepadID;
    }

    if (changed) {
        GamepadType gt = (type == InputDeviceType::Gamepad)
                             ? GetGamepadType(gamepadID) : GamepadType::Unknown;
        core::EventBus::Get().Emit("input_device_changed", core::SignalArgs{
            nlohmann::json(static_cast<int>(type)),
            nlohmann::json(static_cast<int>(gamepadID)),
            nlohmann::json(static_cast<int>(gt))
        });
    }
}

void InputManager::ProcessFrameEvents() {
    for (const InputEvent& ev : m_FrameEvents) {
        // Capture mode consumes the first qualifying press (it does not also drive
        // device tracking or auto-join this frame).
        if (m_Capturing) {
            InputBinding cb;
            bool got = false;
            switch (ev.type) {
                case InputEventType::KeyDown:
                    if ((m_CaptureMask & Capture_Keyboard) && ev.key != KeyCode::Unknown && !ev.repeat) {
                        cb = InputBinding::FromKey(ev.key);
                        got = true;
                    }
                    break;
                case InputEventType::MouseButtonDown:
                    if ((m_CaptureMask & Capture_Mouse) && ev.mouseButton != MouseButton::Unknown) {
                        cb = InputBinding::FromMouseButton(ev.mouseButton);
                        got = true;
                    }
                    break;
                case InputEventType::GamepadButtonDown:
                    if ((m_CaptureMask & Capture_Gamepad) && ev.gamepadButton != GamepadButton::Unknown) {
                        cb = InputBinding::FromGamepadButton(ev.gamepadButton, ev.gamepadID);
                        got = true;
                    }
                    break;
                case InputEventType::GamepadAxis:
                    if ((m_CaptureMask & Capture_Gamepad) && ev.gamepadAxis != GamepadAxis::Unknown &&
                        std::abs(ev.axisValue) > 0.5f) {
                        cb = InputBinding::FromGamepadAxis(ev.gamepadAxis,
                                                           ev.axisValue > 0.0f ? 1.0f : -1.0f,
                                                           ev.gamepadID);
                        got = true;
                    }
                    break;
                default:
                    break;
            }
            if (got) {
                m_CapturedBinding = cb;
                m_Capturing = false;
                m_CaptureComplete = true;
                core::EventBus::Get().Emit("input_capture_complete",
                                           core::SignalArgs{ cb.ToJson() });
                continue;
            }
        }

        switch (ev.type) {
            case InputEventType::KeyDown:
            case InputEventType::Text:
                HandleDeviceUsed(InputDeviceType::Keyboard, 0);
                break;
            case InputEventType::MouseButtonDown:
            case InputEventType::MouseWheel:
                HandleDeviceUsed(InputDeviceType::Mouse, 0);
                break;
            case InputEventType::MouseMotion:
                // Ignore sub-pixel jitter so a resting mouse never steals the active
                // device from a gamepad.
                if (std::abs(ev.relative.x) + std::abs(ev.relative.y) > 2.0f) {
                    HandleDeviceUsed(InputDeviceType::Mouse, 0);
                }
                break;
            case InputEventType::GamepadButtonDown:
                HandleDeviceUsed(InputDeviceType::Gamepad, ev.gamepadID);
                break;
            case InputEventType::GamepadAxis:
                // Past the switch threshold so stick drift does not steal the device.
                if (std::abs(ev.axisValue) > 0.5f) {
                    HandleDeviceUsed(InputDeviceType::Gamepad, ev.gamepadID);
                }
                break;
            default:
                break;
        }
    }
}

// ============================================================================
// Input contexts / action sets
// ============================================================================

void InputManager::EnableContext(const std::string& context) {
    if (!context.empty()) {
        m_ActiveContexts.insert(context);
    }
}

void InputManager::DisableContext(const std::string& context) {
    m_ActiveContexts.erase(context);
}

void InputManager::SetContextActive(const std::string& context, bool active) {
    if (active) {
        EnableContext(context);
    } else {
        DisableContext(context);
    }
}

bool InputManager::IsContextActive(const std::string& context) const {
    if (context.empty()) {
        return true;
    }
    return m_ActiveContexts.count(context) > 0;
}

void InputManager::SetExclusiveContext(const std::string& context) {
    m_ActiveContexts.clear();
    if (!context.empty()) {
        m_ActiveContexts.insert(context);
    }
}

std::vector<std::string> InputManager::GetActiveContexts() const {
    return std::vector<std::string>(m_ActiveContexts.begin(), m_ActiveContexts.end());
}

void InputManager::SetActionEnabled(const std::string& actionName, bool enabled) {
    if (InputAction* action = m_InputMap.GetAction(actionName)) {
        action->SetEnabled(enabled);
    }
}

void InputManager::SetAxisEnabled(const std::string& axisName, bool enabled) {
    if (InputAxis* axis = m_InputMap.GetAxis(axisName)) {
        axis->SetEnabled(enabled);
    }
}

// ============================================================================
// Player slots
// ============================================================================

void InputManager::SetPlayerCount(int count) {
    if (count < 0) {
        count = 0;
    }
    m_Players.resize(static_cast<size_t>(count));
}

void InputManager::ClearPlayerAssignments() {
    for (auto& slot : m_Players) {
        slot.ownsKeyboardMouse = false;
        slot.gamepadIDs.clear();
    }
}

void InputManager::AssignKeyboardMouseToPlayer(int playerIndex) {
    if (playerIndex < 0 || playerIndex >= static_cast<int>(m_Players.size())) {
        return;
    }
    for (auto& slot : m_Players) {
        slot.ownsKeyboardMouse = false;
    }
    m_Players[playerIndex].ownsKeyboardMouse = true;
}

void InputManager::AssignGamepadToPlayer(int playerIndex, uint32_t gamepadID) {
    if (playerIndex < 0 || playerIndex >= static_cast<int>(m_Players.size())) {
        return;
    }
    UnassignGamepad(gamepadID);
    auto& ids = m_Players[playerIndex].gamepadIDs;
    if (std::find(ids.begin(), ids.end(), gamepadID) == ids.end()) {
        ids.push_back(gamepadID);
    }
}

void InputManager::UnassignGamepad(uint32_t gamepadID) {
    for (auto& slot : m_Players) {
        slot.gamepadIDs.erase(
            std::remove(slot.gamepadIDs.begin(), slot.gamepadIDs.end(), gamepadID),
            slot.gamepadIDs.end());
    }
}

int InputManager::GetPlayerForGamepad(uint32_t gamepadID) const {
    for (int p = 0; p < static_cast<int>(m_Players.size()); ++p) {
        const auto& ids = m_Players[p].gamepadIDs;
        if (std::find(ids.begin(), ids.end(), gamepadID) != ids.end()) {
            return p;
        }
    }
    return -1;
}

int InputManager::GetPlayerForKeyboardMouse() const {
    for (int p = 0; p < static_cast<int>(m_Players.size()); ++p) {
        if (m_Players[p].ownsKeyboardMouse) {
            return p;
        }
    }
    return -1;
}

bool InputManager::PlayerOwnsKeyboardMouse(int playerIndex) const {
    return playerIndex >= 0 && playerIndex < static_cast<int>(m_Players.size()) &&
           m_Players[playerIndex].ownsKeyboardMouse;
}

std::vector<uint32_t> InputManager::GetPlayerGamepads(int playerIndex) const {
    if (playerIndex < 0 || playerIndex >= static_cast<int>(m_Players.size())) {
        return {};
    }
    return m_Players[playerIndex].gamepadIDs;
}

// ============================================================================
// Runtime rebinding
// ============================================================================

void InputManager::AddBindingToAction(const std::string& actionName, const InputBinding& binding) {
    InputAction* action = m_InputMap.GetAction(actionName);
    if (!action) {
        m_InputMap.AddAction(InputAction(actionName));
        action = m_InputMap.GetAction(actionName);
    }
    if (action) {
        action->AddBinding(binding);
    }
}

void InputManager::RemoveBindingFromAction(const std::string& actionName, size_t index) {
    if (InputAction* action = m_InputMap.GetAction(actionName)) {
        action->RemoveBinding(index);
    }
}

void InputManager::ClearActionBindings(const std::string& actionName) {
    if (InputAction* action = m_InputMap.GetAction(actionName)) {
        action->ClearBindings();
    }
}

std::vector<InputBinding> InputManager::GetActionBindings(const std::string& actionName) const {
    if (const InputAction* action = m_InputMap.GetAction(actionName)) {
        return action->GetBindings();
    }
    return {};
}

void InputManager::AddBindingToAxis(const std::string& axisName, const InputBinding& binding) {
    InputAxis* axis = m_InputMap.GetAxis(axisName);
    if (!axis) {
        m_InputMap.AddAxis(InputAxis(axisName));
        axis = m_InputMap.GetAxis(axisName);
    }
    if (axis) {
        axis->AddBinding(binding);
    }
}

void InputManager::RemoveBindingFromAxis(const std::string& axisName, size_t index) {
    if (InputAxis* axis = m_InputMap.GetAxis(axisName)) {
        axis->RemoveBinding(index);
    }
}

void InputManager::ClearAxisBindings(const std::string& axisName) {
    if (InputAxis* axis = m_InputMap.GetAxis(axisName)) {
        axis->ClearBindings();
    }
}

std::vector<InputBinding> InputManager::GetAxisBindings(const std::string& axisName) const {
    if (const InputAxis* axis = m_InputMap.GetAxis(axisName)) {
        return axis->GetBindings();
    }
    return {};
}

bool InputManager::ActionHasBinding(const std::string& actionName, const InputBinding& binding) const {
    const InputAction* action = m_InputMap.GetAction(actionName);
    if (!action) {
        return false;
    }
    for (const auto& b : action->GetBindings()) {
        if (b == binding) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// Input capture (rebind menus)
// ============================================================================

void InputManager::StartInputCapture(uint32_t deviceMask) {
    m_Capturing = true;
    m_CaptureComplete = false;
    m_CaptureMask = deviceMask;
}

void InputManager::CancelInputCapture() {
    m_Capturing = false;
}

// ============================================================================
// Glyph / prompt resolution
// ============================================================================

namespace {

std::string GlyphLower(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == ' ' || c == '-') {
            continue;
        }
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

std::string MouseClickLabel(MouseButton button) {
    switch (button) {
        case MouseButton::Left: return "Left Click";
        case MouseButton::Right: return "Right Click";
        case MouseButton::Middle: return "Middle Click";
        default: return MouseButtonToString(button);
    }
}

} // namespace

InputGlyph InputManager::BuildGlyph(const InputBinding& binding, GamepadType gamepadType) const {
    InputGlyph glyph;
    glyph.device = binding.deviceType;
    glyph.gamepadType = gamepadType;

    switch (binding.deviceType) {
        case InputDeviceType::Keyboard:
            glyph.glyphId = "key_" + GlyphLower(KeyCodeToString(binding.keyCode));
            glyph.label = KeyCodeToString(binding.keyCode);
            break;
        case InputDeviceType::Mouse:
            glyph.glyphId = "mouse_" + GlyphLower(MouseButtonToString(binding.mouseButton));
            glyph.label = MouseClickLabel(binding.mouseButton);
            break;
        case InputDeviceType::Gamepad: {
            const std::string prefix = GamepadTypePrefix(gamepadType);
            if (binding.gamepadButton != GamepadButton::Unknown) {
                glyph.glyphId = "gamepad_" + prefix + "_" +
                                GlyphLower(GamepadButtonToString(binding.gamepadButton));
                glyph.label = GamepadFaceLabel(binding.gamepadButton, gamepadType);
            } else if (binding.gamepadAxis != GamepadAxis::Unknown) {
                const std::string dir = binding.scale < 0.0f ? "_neg" : "_pos";
                glyph.glyphId = "gamepad_" + prefix + "_" +
                                GlyphLower(GamepadAxisToString(binding.gamepadAxis)) + dir;
                glyph.label = GamepadAxisLabel(binding.gamepadAxis, gamepadType);
            }
            break;
        }
        default:
            break;
    }

    auto lit = m_GlyphLabels.find(glyph.glyphId);
    if (lit != m_GlyphLabels.end()) {
        glyph.label = lit->second;
    }
    auto ait = m_GlyphArt.find(glyph.glyphId);
    if (ait != m_GlyphArt.end()) {
        glyph.artPath = ait->second;
    }
    return glyph;
}

InputDeviceType InputManager::ResolveActiveDeviceForPlayer(int playerIndex) const {
    if (playerIndex < 0) {
        return m_LastUsedDeviceType == InputDeviceType::Unknown
                   ? InputDeviceType::Keyboard : m_LastUsedDeviceType;
    }
    if (playerIndex >= static_cast<int>(m_Players.size())) {
        return InputDeviceType::Keyboard;
    }
    const PlayerSlot& slot = m_Players[playerIndex];
    if (m_LastUsedDeviceType == InputDeviceType::Gamepad) {
        if (GetPlayerForGamepad(m_LastUsedGamepadID) == playerIndex) {
            return InputDeviceType::Gamepad;
        }
    } else if (m_LastUsedDeviceType == InputDeviceType::Keyboard ||
               m_LastUsedDeviceType == InputDeviceType::Mouse) {
        if (slot.ownsKeyboardMouse) {
            return m_LastUsedDeviceType;
        }
    }
    if (!slot.gamepadIDs.empty()) {
        return InputDeviceType::Gamepad;
    }
    return InputDeviceType::Keyboard;
}

InputGlyph InputManager::GetBindingGlyph(const InputBinding& binding) const {
    GamepadType gt = GamepadType::Unknown;
    if (binding.deviceType == InputDeviceType::Gamepad) {
        uint32_t gid = binding.gamepadID != 0 ? binding.gamepadID : m_LastUsedGamepadID;
        gt = GetGamepadType(gid);
    }
    return BuildGlyph(binding, gt);
}

InputGlyph InputManager::GetActionGlyph(const std::string& actionName, int playerIndex,
                                        InputDeviceType deviceOverride) const {
    const InputAction* action = m_InputMap.GetAction(actionName);
    if (!action || action->GetBindings().empty()) {
        return InputGlyph{};
    }

    const InputDeviceType target = deviceOverride != InputDeviceType::Unknown
                                       ? deviceOverride
                                       : ResolveActiveDeviceForPlayer(playerIndex);

    uint32_t gid = 0;
    if (playerIndex >= 0 && playerIndex < static_cast<int>(m_Players.size()) &&
        !m_Players[playerIndex].gamepadIDs.empty()) {
        gid = m_Players[playerIndex].gamepadIDs.front();
    } else {
        gid = m_LastUsedGamepadID;
    }
    const GamepadType gt = GetGamepadType(gid);

    // Keyboard and mouse are treated as one "device" for prompt selection.
    auto matches = [&](const InputBinding& b) {
        if (target == InputDeviceType::Gamepad) {
            return b.deviceType == InputDeviceType::Gamepad;
        }
        return b.deviceType == InputDeviceType::Keyboard ||
               b.deviceType == InputDeviceType::Mouse;
    };

    const InputBinding* chosen = nullptr;
    for (const auto& b : action->GetBindings()) {
        if (matches(b) && BindingBelongsToPlayer(b, playerIndex)) {
            chosen = &b;
            break;
        }
    }
    if (!chosen) {
        for (const auto& b : action->GetBindings()) {
            if (BindingBelongsToPlayer(b, playerIndex)) {
                chosen = &b;
                break;
            }
        }
    }
    if (!chosen) {
        chosen = &action->GetBindings().front();
    }
    return BuildGlyph(*chosen, gt);
}

std::vector<InputGlyph> InputManager::GetActionGlyphs(const std::string& actionName) const {
    std::vector<InputGlyph> out;
    const InputAction* action = m_InputMap.GetAction(actionName);
    if (!action) {
        return out;
    }
    for (const auto& b : action->GetBindings()) {
        out.push_back(GetBindingGlyph(b));
    }
    return out;
}

void InputManager::SetGlyphLabel(const std::string& glyphId, const std::string& label) {
    m_GlyphLabels[glyphId] = label;
}

void InputManager::SetGlyphArt(const std::string& glyphId, const std::string& artPath) {
    m_GlyphArt[glyphId] = artPath;
}

void InputManager::ClearGlyphOverride(const std::string& glyphId) {
    m_GlyphLabels.erase(glyphId);
    m_GlyphArt.erase(glyphId);
}

void InputManager::ClearGlyphOverrides() {
    m_GlyphLabels.clear();
    m_GlyphArt.clear();
}

std::string InputManager::SaveGlyphMapToString() const {
    nlohmann::json j = nlohmann::json::object();
    std::unordered_set<std::string> ids;
    for (const auto& [k, v] : m_GlyphLabels) { (void)v; ids.insert(k); }
    for (const auto& [k, v] : m_GlyphArt) { (void)v; ids.insert(k); }
    for (const auto& id : ids) {
        nlohmann::json entry = nlohmann::json::object();
        auto lit = m_GlyphLabels.find(id);
        if (lit != m_GlyphLabels.end()) {
            entry["label"] = lit->second;
        }
        auto ait = m_GlyphArt.find(id);
        if (ait != m_GlyphArt.end()) {
            entry["art"] = ait->second;
        }
        j[id] = entry;
    }
    return j.dump(4);
}

bool InputManager::LoadGlyphMapFromString(const std::string& jsonString) {
    try {
        if (jsonString.empty()) {
            return false;
        }
        nlohmann::json j = nlohmann::json::parse(jsonString);
        if (!j.is_object()) {
            return false;
        }
        for (auto it = j.begin(); it != j.end(); ++it) {
            const auto& entry = it.value();
            if (entry.contains("label")) {
                m_GlyphLabels[it.key()] = entry["label"].get<std::string>();
            }
            if (entry.contains("art")) {
                m_GlyphArt[it.key()] = entry["art"].get<std::string>();
            }
        }
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool InputManager::LoadGlyphMap(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return false;
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return LoadGlyphMapFromString(ss.str());
}

bool InputManager::SaveGlyphMap(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        return false;
    }
    file << SaveGlyphMapToString();
    return true;
}

}
}
