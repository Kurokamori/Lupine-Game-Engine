#pragma once

#include "InputCodes.hpp"
#include "InputDevice.hpp"
#include "InputAction.hpp"
#include <memory>
#include <unordered_map>
#include <string>
#include <functional>

namespace lupine {
namespace input {

/**
 * Central input manager for the engine
 * 
 * This is NOT an input poller - it manages input state and actions/axes.
 * The runtime/engine layer is responsible for polling platform-specific input
 * and feeding events to this manager.
 * 
 * Features:
 * - Device management (keyboard, mouse, gamepads)
 * - Input action and axis mapping
 * - State tracking (pressed, just pressed, just released)
 * - Project settings persistence
 * - Runtime and engine APIs
 */
class InputManager {
public:
    InputManager();
    ~InputManager();

    // Singleton access (optional, can also be used as instance)
    static InputManager& Get();
    static void Initialize();
    static void Shutdown();

    // ========================================================================
    // Frame management - must be called by engine each frame
    // ========================================================================
    
    /**
     * Update all devices and process input state
     * Should be called once per frame by the engine
     */
    void Update(float deltaTime);
    
    /**
     * Clear all input state
     */
    void Reset();

    // ========================================================================
    // Device management (Engine API)
    // ========================================================================
    
    KeyboardDevice* GetKeyboard() { return m_Keyboard.get(); }
    MouseDevice* GetMouse() { return m_Mouse.get(); }
    GamepadDevice* GetGamepad(uint32_t id = 0);
    
    /**
     * Get all connected gamepads
     */
    std::vector<GamepadDevice*> GetConnectedGamepads();
    
    /**
     * Register a new gamepad (called by platform layer)
     */
    void RegisterGamepad(uint32_t id);
    
    /**
     * Unregister a gamepad (called by platform layer)
     */
    void UnregisterGamepad(uint32_t id);

    // ========================================================================
    // Raw input queries (Runtime API) - Direct device access
    // ========================================================================
    
    // Keyboard
    bool IsKeyPressed(KeyCode key) const;
    bool IsKeyJustPressed(KeyCode key) const;
    bool IsKeyJustReleased(KeyCode key) const;
    
    // Mouse
    bool IsMouseButtonPressed(MouseButton button) const;
    bool IsMouseButtonJustPressed(MouseButton button) const;
    bool IsMouseButtonJustReleased(MouseButton button) const;
    glm::vec2 GetMousePosition() const;
    glm::vec2 GetMousePositionFlippedY() const;  // For Canvas/UI rendering (OpenGL Y-flip)
    glm::vec2 GetMouseDelta() const;
    glm::vec2 GetMouseScrollDelta() const;

    // Window/Screen size (for coordinate conversions)
    void SetWindowSize(int width, int height);
    glm::ivec2 GetWindowSize() const;
    
    // Gamepad
    bool IsGamepadButtonPressed(GamepadButton button, uint32_t gamepadID = 0) const;
    bool IsGamepadButtonJustPressed(GamepadButton button, uint32_t gamepadID = 0) const;
    bool IsGamepadButtonJustReleased(GamepadButton button, uint32_t gamepadID = 0) const;
    float GetGamepadAxis(GamepadAxis axis, uint32_t gamepadID = 0) const;

    // ========================================================================
    // Action/Axis API (Runtime API) - Mapped inputs
    // ========================================================================
    
    /**
     * Check if an action is currently pressed
     * Returns true if any binding for this action is active
     */
    bool IsActionPressed(const std::string& actionName) const;
    
    /**
     * Check if an action was just pressed this frame
     */
    bool IsActionJustPressed(const std::string& actionName) const;
    
    /**
     * Check if an action was just released this frame
     */
    bool IsActionJustReleased(const std::string& actionName) const;
    
    /**
     * Get the current value of an axis (-1.0 to 1.0)
     * Combines all bindings for this axis
     */
    float GetAxisValue(const std::string& axisName) const;

    /**
     * Get the raw axis value without smoothing/gravity
     */
    float GetAxisValueRaw(const std::string& axisName) const;

    // ========================================================================
    // Input mapping management (Engine API)
    // ========================================================================
    
    /**
     * Get the input map for editing
     */
    InputMap& GetInputMap() { return m_InputMap; }
    const InputMap& GetInputMap() const { return m_InputMap; }
    
    /**
     * Load input map from file
     */
    bool LoadInputMap(const std::string& filepath);
    
    /**
     * Save input map to file
     */
    bool SaveInputMap(const std::string& filepath) const;
    
    /**
     * Add a new action
     */
    void AddAction(const InputAction& action);
    
    /**
     * Add a new axis
     */
    void AddAxis(const InputAxis& axis);
    
    /**
     * Remove an action
     */
    void RemoveAction(const std::string& name);
    
    /**
     * Remove an axis
     */
    void RemoveAxis(const std::string& name);

    // ========================================================================
    // Event callbacks (Engine API)
    // ========================================================================
    
    using ActionCallback = std::function<void(const std::string& actionName, bool pressed)>;
    using AxisCallback = std::function<void(const std::string& axisName, float value)>;
    
    /**
     * Register a callback for when an action state changes
     */
    void RegisterActionCallback(const std::string& actionName, ActionCallback callback);
    
    /**
     * Register a callback for when an axis value changes
     */
    void RegisterAxisCallback(const std::string& axisName, AxisCallback callback);
    
    /**
     * Remove action callback
     */
    void UnregisterActionCallback(const std::string& actionName);
    
    /**
     * Remove axis callback
     */
    void UnregisterAxisCallback(const std::string& axisName);

    // ========================================================================
    // Settings
    // ========================================================================
    
    /**
     * Set the default gamepad deadzone
     */
    void SetGlobalGamepadDeadzone(float deadzone);
    
    /**
     * Get the default gamepad deadzone
     */
    float GetGlobalGamepadDeadzone() const { return m_GlobalGamepadDeadzone; }
    
    /**
     * Enable/disable mouse cursor
     */
    void SetMouseCursorVisible(bool visible);
    bool IsMouseCursorVisible() const { return m_MouseCursorVisible; }

private:
    // Devices
    std::unique_ptr<KeyboardDevice> m_Keyboard;
    std::unique_ptr<MouseDevice> m_Mouse;
    std::unordered_map<uint32_t, std::unique_ptr<GamepadDevice>> m_Gamepads;
    
    // Input mapping
    InputMap m_InputMap;
    
    // Axis state tracking (for smoothing/gravity)
    struct AxisState {
        float currentValue = 0.0f;
        float targetValue = 0.0f;
    };
    mutable std::unordered_map<std::string, AxisState> m_AxisStates;
    
    // Action state tracking (for just pressed/released detection)
    struct ActionState {
        bool currentFrame = false;
        bool previousFrame = false;
    };
    mutable std::unordered_map<std::string, ActionState> m_ActionStates;

    // Window size for coordinate conversions (SDL: Y=0 top, OpenGL: Y=0 bottom)
    int m_WindowWidth = 1920;
    int m_WindowHeight = 1080;
    
    // Callbacks
    std::unordered_map<std::string, ActionCallback> m_ActionCallbacks;
    std::unordered_map<std::string, AxisCallback> m_AxisCallbacks;
    
    // Settings
    float m_GlobalGamepadDeadzone = 0.15f;
    bool m_MouseCursorVisible = true;
    
    // Singleton instance
    static std::unique_ptr<InputManager> s_Instance;
    
    // Internal helpers
    bool EvaluateBinding(const InputBinding& binding) const;
    float EvaluateBindingAxis(const InputBinding& binding) const;
    void UpdateActionStates();
    void UpdateAxisStates(float deltaTime);
    void ProcessCallbacks();
};

} // namespace input
} // namespace lupine
