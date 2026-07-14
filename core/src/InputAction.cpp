#include "lupine/input/InputAction.hpp"
#include "lupine/logger/Logger.hpp"
#include <fstream>
#include <algorithm>

namespace lupine {
namespace input {

InputBinding InputBinding::FromKey(KeyCode key) {
    InputBinding binding;
    binding.deviceType = InputDeviceType::Keyboard;
    binding.keyCode = key;
    return binding;
}

InputBinding InputBinding::FromMouseButton(MouseButton button) {
    InputBinding binding;
    binding.deviceType = InputDeviceType::Mouse;
    binding.mouseButton = button;
    return binding;
}

InputBinding InputBinding::FromGamepadButton(GamepadButton button, uint32_t gamepadID) {
    InputBinding binding;
    binding.deviceType = InputDeviceType::Gamepad;
    binding.gamepadButton = button;
    binding.gamepadID = gamepadID;
    return binding;
}

InputBinding InputBinding::FromGamepadAxis(GamepadAxis axis, float scale, uint32_t gamepadID) {
    InputBinding binding;
    binding.deviceType = InputDeviceType::Gamepad;
    binding.gamepadAxis = axis;
    binding.scale = scale;
    binding.gamepadID = gamepadID;
    return binding;
}

nlohmann::json InputBinding::ToJson() const {
    nlohmann::json j;

    j["deviceType"] = static_cast<int>(deviceType);

    if (deviceType == InputDeviceType::Keyboard) {
        j["keyCode"] = static_cast<int>(keyCode);
        j["keyName"] = KeyCodeToString(keyCode);
    }
    else if (deviceType == InputDeviceType::Mouse) {
        j["mouseButton"] = static_cast<int>(mouseButton);
        j["buttonName"] = MouseButtonToString(mouseButton);
    }
    else if (deviceType == InputDeviceType::Gamepad) {
        if (gamepadButton != GamepadButton::Unknown) {
            j["gamepadButton"] = static_cast<int>(gamepadButton);
            j["buttonName"] = GamepadButtonToString(gamepadButton);
        }
        if (gamepadAxis != GamepadAxis::Unknown) {
            j["gamepadAxis"] = static_cast<int>(gamepadAxis);
            j["axisName"] = GamepadAxisToString(gamepadAxis);
            j["scale"] = scale;
        }
        j["gamepadID"] = gamepadID;
    }

    return j;
}

InputBinding InputBinding::FromJson(const nlohmann::json& json) {
    InputBinding binding;

    binding.deviceType = static_cast<InputDeviceType>(json.value("deviceType", 0));

    if (binding.deviceType == InputDeviceType::Keyboard) {
        if (json.contains("keyName")) {
            binding.keyCode = StringToKeyCode(json["keyName"]);
        } else {
            binding.keyCode = static_cast<KeyCode>(json.value("keyCode", 0));
        }
    }
    else if (binding.deviceType == InputDeviceType::Mouse) {
        if (json.contains("buttonName")) {
            binding.mouseButton = StringToMouseButton(json["buttonName"]);
        } else {
            binding.mouseButton = static_cast<MouseButton>(json.value("mouseButton", 255));
        }
    }
    else if (binding.deviceType == InputDeviceType::Gamepad) {
        if (json.contains("buttonName")) {
            binding.gamepadButton = StringToGamepadButton(json["buttonName"]);
        } else if (json.contains("gamepadButton")) {
            binding.gamepadButton = static_cast<GamepadButton>(json["gamepadButton"]);
        }

        if (json.contains("axisName")) {
            binding.gamepadAxis = StringToGamepadAxis(json["axisName"]);
        } else if (json.contains("gamepadAxis")) {
            binding.gamepadAxis = static_cast<GamepadAxis>(json["gamepadAxis"]);
        }

        binding.scale = json.value("scale", 1.0f);
        binding.gamepadID = json.value("gamepadID", 0);
    }

    return binding;
}

bool InputBinding::operator==(const InputBinding& other) const {
    if (deviceType != other.deviceType) return false;

    switch (deviceType) {
        case InputDeviceType::Keyboard:
            return keyCode == other.keyCode;
        case InputDeviceType::Mouse:
            return mouseButton == other.mouseButton;
        case InputDeviceType::Gamepad:
            return gamepadButton == other.gamepadButton &&
                   gamepadAxis == other.gamepadAxis &&
                   gamepadID == other.gamepadID;
        default:
            return false;
    }
}

InputAction::InputAction(const std::string& name)
    : m_Name(name) {
}

void InputAction::AddBinding(const InputBinding& binding) {

    auto it = std::find(m_Bindings.begin(), m_Bindings.end(), binding);
    if (it == m_Bindings.end()) {
        m_Bindings.push_back(binding);
    }
}

void InputAction::RemoveBinding(size_t index) {
    if (index < m_Bindings.size()) {
        m_Bindings.erase(m_Bindings.begin() + index);
    }
}

void InputAction::ClearBindings() {
    m_Bindings.clear();
}

nlohmann::json InputAction::ToJson() const {
    nlohmann::json j;
    j["name"] = m_Name;
    j["deadzone"] = m_Deadzone;
    j["context"] = m_Context;
    j["enabled"] = m_Enabled;

    nlohmann::json bindingsArray = nlohmann::json::array();
    for (const auto& binding : m_Bindings) {
        bindingsArray.push_back(binding.ToJson());
    }
    j["bindings"] = bindingsArray;

    return j;
}

InputAction InputAction::FromJson(const nlohmann::json& json) {
    InputAction action;
    action.m_Name = json.value("name", "");
    action.m_Deadzone = json.value("deadzone", 0.5f);
    action.m_Context = json.value("context", std::string());
    action.m_Enabled = json.value("enabled", true);

    if (json.contains("bindings")) {
        for (const auto& bindingJson : json["bindings"]) {
            action.m_Bindings.push_back(InputBinding::FromJson(bindingJson));
        }
    }

    return action;
}

InputAxis::InputAxis(const std::string& name)
    : m_Name(name) {
}

void InputAxis::AddBinding(const InputBinding& binding) {
    auto it = std::find(m_Bindings.begin(), m_Bindings.end(), binding);
    if (it == m_Bindings.end()) {
        m_Bindings.push_back(binding);
    }
}

void InputAxis::RemoveBinding(size_t index) {
    if (index < m_Bindings.size()) {
        m_Bindings.erase(m_Bindings.begin() + index);
    }
}

void InputAxis::ClearBindings() {
    m_Bindings.clear();
}

nlohmann::json InputAxis::ToJson() const {
    nlohmann::json j;
    j["name"] = m_Name;
    j["deadzone"] = m_Deadzone;
    j["sensitivity"] = m_Sensitivity;
    j["gravity"] = m_Gravity;
    j["snap"] = m_Snap;
    j["context"] = m_Context;
    j["enabled"] = m_Enabled;

    nlohmann::json bindingsArray = nlohmann::json::array();
    for (const auto& binding : m_Bindings) {
        bindingsArray.push_back(binding.ToJson());
    }
    j["bindings"] = bindingsArray;

    return j;
}

InputAxis InputAxis::FromJson(const nlohmann::json& json) {
    InputAxis axis;
    axis.m_Name = json.value("name", "");
    axis.m_Deadzone = json.value("deadzone", 0.2f);
    axis.m_Sensitivity = json.value("sensitivity", 1.0f);
    axis.m_Gravity = json.value("gravity", 3.0f);
    axis.m_Snap = json.value("snap", false);
    axis.m_Context = json.value("context", std::string());
    axis.m_Enabled = json.value("enabled", true);

    if (json.contains("bindings")) {
        for (const auto& bindingJson : json["bindings"]) {
            axis.m_Bindings.push_back(InputBinding::FromJson(bindingJson));
        }
    }

    return axis;
}

void InputMap::AddAction(const InputAction& action) {

    auto it = std::find_if(m_Actions.begin(), m_Actions.end(),
        [&action](const InputAction& a) { return a.GetName() == action.GetName(); });

    if (it != m_Actions.end()) {
        *it = action;
    } else {
        m_Actions.push_back(action);
    }
}

void InputMap::RemoveAction(const std::string& name) {
    m_Actions.erase(
        std::remove_if(m_Actions.begin(), m_Actions.end(),
            [&name](const InputAction& a) { return a.GetName() == name; }),
        m_Actions.end()
    );
}

InputAction* InputMap::GetAction(const std::string& name) {
    auto it = std::find_if(m_Actions.begin(), m_Actions.end(),
        [&name](const InputAction& a) { return a.GetName() == name; });
    return it != m_Actions.end() ? &(*it) : nullptr;
}

const InputAction* InputMap::GetAction(const std::string& name) const {
    auto it = std::find_if(m_Actions.begin(), m_Actions.end(),
        [&name](const InputAction& a) { return a.GetName() == name; });
    return it != m_Actions.end() ? &(*it) : nullptr;
}

bool InputMap::HasAction(const std::string& name) const {
    return GetAction(name) != nullptr;
}

void InputMap::AddAxis(const InputAxis& axis) {
    auto it = std::find_if(m_Axes.begin(), m_Axes.end(),
        [&axis](const InputAxis& a) { return a.GetName() == axis.GetName(); });

    if (it != m_Axes.end()) {
        *it = axis;
    } else {
        m_Axes.push_back(axis);
    }
}

void InputMap::RemoveAxis(const std::string& name) {
    m_Axes.erase(
        std::remove_if(m_Axes.begin(), m_Axes.end(),
            [&name](const InputAxis& a) { return a.GetName() == name; }),
        m_Axes.end()
    );
}

InputAxis* InputMap::GetAxis(const std::string& name) {
    auto it = std::find_if(m_Axes.begin(), m_Axes.end(),
        [&name](const InputAxis& a) { return a.GetName() == name; });
    return it != m_Axes.end() ? &(*it) : nullptr;
}

const InputAxis* InputMap::GetAxis(const std::string& name) const {
    auto it = std::find_if(m_Axes.begin(), m_Axes.end(),
        [&name](const InputAxis& a) { return a.GetName() == name; });
    return it != m_Axes.end() ? &(*it) : nullptr;
}

bool InputMap::HasAxis(const std::string& name) const {
    return GetAxis(name) != nullptr;
}

void InputMap::Clear() {
    m_Actions.clear();
    m_Axes.clear();
}

nlohmann::json InputMap::Serialize() const {
    nlohmann::json j;

    nlohmann::json actionsArray = nlohmann::json::array();
    for (const auto& action : m_Actions) {
        actionsArray.push_back(action.ToJson());
    }
    j["actions"] = actionsArray;

    nlohmann::json axesArray = nlohmann::json::array();
    for (const auto& axis : m_Axes) {
        axesArray.push_back(axis.ToJson());
    }
    j["axes"] = axesArray;

    return j;
}

void InputMap::Deserialize(const nlohmann::json& json) {
    Clear();

    if (json.contains("actions")) {
        for (const auto& actionJson : json["actions"]) {
            m_Actions.push_back(InputAction::FromJson(actionJson));
        }
    }

    if (json.contains("axes")) {
        for (const auto& axisJson : json["axes"]) {
            m_Axes.push_back(InputAxis::FromJson(axisJson));
        }
    }
}

bool InputMap::SaveToFile(const std::string& filepath) const {
    try {
        nlohmann::json j = Serialize();
        std::ofstream file(filepath);
        if (!file.is_open()) {

            return false;
        }
        file << j.dump(4);

        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(LogCategory::Input, "InputMap: failed to save '{}': {}", filepath, e.what());
        return false;
    }
}

bool InputMap::LoadFromFile(const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {

            return false;
        }

        nlohmann::json j;
        file >> j;
        Deserialize(j);

        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(LogCategory::Input, "InputMap: failed to load '{}': {}", filepath, e.what());
        return false;
    }
}

bool InputMap::LoadFromString(const std::string& jsonString) {
    try {
        if (jsonString.empty()) {
            return false;
        }

        nlohmann::json j = nlohmann::json::parse(jsonString);
        Deserialize(j);

        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(LogCategory::Input, "InputMap: failed to parse input map JSON: {}", e.what());
        return false;
    }
}

}
}
