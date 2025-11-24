#pragma once

#include "InputCodes.hpp"
#include "lupine/core/Serialization.hpp"
#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

namespace lupine {
namespace input {

/**
 * Represents a single input binding (e.g., a key, button, or axis)
 */
struct InputBinding {
    InputDeviceType deviceType = InputDeviceType::Unknown;
    
    // For keyboard
    KeyCode keyCode = KeyCode::Unknown;
    
    // For mouse
    MouseButton mouseButton = MouseButton::Unknown;
    
    // For gamepad
    GamepadButton gamepadButton = GamepadButton::Unknown;
    GamepadAxis gamepadAxis = GamepadAxis::Unknown;
    uint32_t gamepadID = 0; // 0 = any gamepad
    
    // For axis bindings
    float scale = 1.0f; // Scale factor for axis values (can be negative for inversion)
    
    InputBinding() = default;
    
    // Convenience constructors
    static InputBinding FromKey(KeyCode key);
    static InputBinding FromMouseButton(MouseButton button);
    static InputBinding FromGamepadButton(GamepadButton button, uint32_t gamepadID = 0);
    static InputBinding FromGamepadAxis(GamepadAxis axis, float scale = 1.0f, uint32_t gamepadID = 0);
    
    // Serialization
    nlohmann::json ToJson() const;
    static InputBinding FromJson(const nlohmann::json& json);
    
    bool operator==(const InputBinding& other) const;
};

/**
 * Input action - represents a digital input (pressed/not pressed)
 * Example: "ui_accept", "jump", "attack"
 */
class InputAction {
public:
    InputAction() = default;
    InputAction(const std::string& name);
    
    const std::string& GetName() const { return m_Name; }
    void SetName(const std::string& name) { m_Name = name; }
    
    // Manage bindings
    void AddBinding(const InputBinding& binding);
    void RemoveBinding(size_t index);
    void ClearBindings();
    const std::vector<InputBinding>& GetBindings() const { return m_Bindings; }
    
    // Settings
    void SetDeadzone(float deadzone) { m_Deadzone = deadzone; }
    float GetDeadzone() const { return m_Deadzone; }
    
    // Serialization
    nlohmann::json ToJson() const;
    static InputAction FromJson(const nlohmann::json& json);
    
private:
    std::string m_Name;
    std::vector<InputBinding> m_Bindings;
    float m_Deadzone = 0.5f; // For axis-based inputs
    
    friend class InputManager;
};

/**
 * Input axis - represents an analog input with a value range
 * Example: "move_horizontal", "look_vertical", "accelerate"
 */
class InputAxis {
public:
    InputAxis() = default;
    InputAxis(const std::string& name);
    
    const std::string& GetName() const { return m_Name; }
    void SetName(const std::string& name) { m_Name = name; }
    
    // Manage bindings
    void AddBinding(const InputBinding& binding);
    void RemoveBinding(size_t index);
    void ClearBindings();
    const std::vector<InputBinding>& GetBindings() const { return m_Bindings; }
    
    // Settings
    void SetDeadzone(float deadzone) { m_Deadzone = deadzone; }
    float GetDeadzone() const { return m_Deadzone; }
    
    void SetSensitivity(float sensitivity) { m_Sensitivity = sensitivity; }
    float GetSensitivity() const { return m_Sensitivity; }
    
    void SetGravity(float gravity) { m_Gravity = gravity; }
    float GetGravity() const { return m_Gravity; }
    
    void SetSnap(bool snap) { m_Snap = snap; }
    bool GetSnap() const { return m_Snap; }
    
    // Serialization
    nlohmann::json ToJson() const;
    static InputAxis FromJson(const nlohmann::json& json);
    
private:
    std::string m_Name;
    std::vector<InputBinding> m_Bindings;
    
    float m_Deadzone = 0.2f;      // Minimum value to register input
    float m_Sensitivity = 1.0f;    // Multiplier for digital inputs
    float m_Gravity = 3.0f;        // How fast the input resets to neutral
    bool m_Snap = false;           // If true, axis immediately resets when opposite input is pressed
    
    friend class InputManager;
};

/**
 * Input mapping configuration
 * Contains all actions and axes for the project
 */
class InputMap : public core::ISerializable {
public:
    InputMap() = default;
    virtual ~InputMap() = default;
    
    // Actions
    void AddAction(const InputAction& action);
    void RemoveAction(const std::string& name);
    InputAction* GetAction(const std::string& name);
    const InputAction* GetAction(const std::string& name) const;
    bool HasAction(const std::string& name) const;
    const std::vector<InputAction>& GetAllActions() const { return m_Actions; }
    
    // Axes
    void AddAxis(const InputAxis& axis);
    void RemoveAxis(const std::string& name);
    InputAxis* GetAxis(const std::string& name);
    const InputAxis* GetAxis(const std::string& name) const;
    bool HasAxis(const std::string& name) const;
    const std::vector<InputAxis>& GetAllAxes() const { return m_Axes; }
    
    // Clear all
    void Clear();
    
    // Serialization
    std::string GetTypeName() const override { return "InputMap"; }
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& json) override;
    void RegisterProperties() override {}
    
    // File I/O
    bool SaveToFile(const std::string& filepath) const;
    bool LoadFromFile(const std::string& filepath);
    
private:
    std::vector<InputAction> m_Actions;
    std::vector<InputAxis> m_Axes;
};

} // namespace input
} // namespace lupine
