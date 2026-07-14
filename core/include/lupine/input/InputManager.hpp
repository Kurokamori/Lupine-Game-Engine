#pragma once

#include "InputCodes.hpp"
#include "InputDevice.hpp"
#include "InputAction.hpp"
#include "InputEvent.hpp"
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>
#include <functional>
#include <array>
#include <cstdint>
#include <mutex>

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
     * Promote this frame's polled events and recompute action/axis states.
     * Should be called once per frame by the engine, AFTER the platform layer has
     * polled this frame's events and BEFORE the scene reads input.
     */
    void Update(float deltaTime);

    /**
     * Advance per-device frame state (snapshot previous button/key states for edge
     * detection, reset per-frame scroll/text deltas). Must be called once per frame
     * BEFORE the platform layer polls this frame's events, so "just pressed/released"
     * and the scroll delta compare this frame's input against the previous frame
     * rather than against the post-poll state.
     */
    void BeginFrame();

    /**
     * Clear all input state
     */
    void Reset();

    // ========================================================================
    // Device management (Engine API)
    // ========================================================================
    
    KeyboardDevice* GetKeyboard() { return m_Keyboard.get(); }
    MouseDevice* GetMouse() { return m_Mouse.get(); }
    TouchDevice* GetTouch() { return m_Touch.get(); }
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

    /**
     * Unicode codepoints typed this frame (in event order), already resolved
     * through the OS keyboard layout / IME. Use this for text fields instead of
     * raw key polling. Cleared every frame.
     */
    const std::vector<uint32_t>& GetTextInput() const;

    // ========================================================================
    // Platform-agnostic modifier queries
    // These automatically use the correct modifier for the current platform:
    // - Windows/Linux: Control key
    // - Mac: Command (Super) key
    // ========================================================================

    /**
     * Check if the platform action modifier is pressed (Ctrl on Win/Linux, Cmd on Mac)
     */
    bool IsActionModifierPressed() const;

    /**
     * Check if Shift modifier is pressed (any Shift key)
     */
    bool IsShiftPressed() const;

    /**
     * Check if Alt modifier is pressed (any Alt key)
     */
    bool IsAltPressed() const;

    /**
     * Check if Control is pressed (specifically Control, not Command on Mac)
     * Use IsActionModifierPressed() for platform-agnostic behavior
     */
    bool IsControlPressed() const;

    /**
     * Check if Super/Command/Windows key is pressed
     */
    bool IsSuperPressed() const;
    
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

    // DPI and content scale handling for high-DPI displays and web CSS scaling
    // Content scale: ratio of display size to actual render target size (e.g., CSS size / canvas size on web)
    // DPI scale: device pixel ratio for high-DPI displays
    void SetContentScale(float scaleX, float scaleY);
    void SetDPIScale(float scale);
    glm::vec2 GetContentScale() const { return m_ContentScale; }
    float GetDPIScale() const { return m_DPIScale; }
    
    // Gamepad
    bool IsGamepadButtonPressed(GamepadButton button, uint32_t gamepadID = 0) const;
    bool IsGamepadButtonJustPressed(GamepadButton button, uint32_t gamepadID = 0) const;
    bool IsGamepadButtonJustReleased(GamepadButton button, uint32_t gamepadID = 0) const;
    float GetGamepadAxis(GamepadAxis axis, uint32_t gamepadID = 0) const;

    /**
     * Number of currently connected gamepads.
     */
    size_t GetGamepadCount() const;

    /**
     * True if a gamepad with the given id is connected.
     */
    bool IsGamepadConnected(uint32_t gamepadID = 0) const;

    /**
     * Device ids of all currently connected gamepads.
     */
    std::vector<uint32_t> GetConnectedGamepadIDs() const;

    /**
     * Human-readable name of the gamepad with the given id ("" if unknown).
     */
    std::string GetGamepadName(uint32_t gamepadID = 0) const;

    /**
     * Trigger rumble on a gamepad. Motor strengths are clamped to [0, 1]; a
     * durationMs of 0 means "until changed/stopped". Setting both motors to 0
     * stops vibration. Requires a vibration provider installed by the platform
     * layer (no-op otherwise).
     */
    void SetGamepadVibration(uint32_t gamepadID, float leftMotor, float rightMotor, uint32_t durationMs = 0);

    /**
     * Stop any active rumble on a gamepad.
     */
    void StopGamepadVibration(uint32_t gamepadID);

    // Touch input (for mobile/web platforms)
    bool IsTouchAvailable() const;
    bool IsTouching() const;
    size_t GetTouchCount() const;
    glm::vec2 GetTouchPosition(size_t index = 0) const;
    bool IsTouchJustStarted() const;
    bool IsTouchJustEnded() const;

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

    /**
     * Get the analog strength of an action in [0, 1].
     *
     * Evaluates every binding of the action: gamepad axis/trigger bindings
     * contribute their analog magnitude (after the binding scale), while digital
     * bindings (keys/buttons) contribute 1.0 when active. The largest contribution
     * is returned, with the action's deadzone applied (values below the deadzone
     * read as 0). Returns 0 for an unknown action.
     */
    float GetActionStrength(const std::string& actionName) const;

    // ========================================================================
    // Per-player Action/Axis queries (local multiplayer / Runtime API)
    //
    // playerIndex < 0 means "any device" — identical to the no-player overloads
    // above and the historical single-player behavior. A valid playerIndex
    // restricts evaluation to the devices assigned to that player (see the Player
    // slots section). A gamepad binding with gamepadID 0 ("any") resolves to that
    // player's assigned gamepad(s); keyboard/mouse bindings count only if the
    // player owns the keyboard & mouse.
    // ========================================================================
    bool IsActionPressed(const std::string& actionName, int playerIndex) const;
    bool IsActionJustPressed(const std::string& actionName, int playerIndex) const;
    bool IsActionJustReleased(const std::string& actionName, int playerIndex) const;
    float GetAxisValue(const std::string& axisName, int playerIndex) const;
    float GetActionStrength(const std::string& actionName, int playerIndex) const;

    // ========================================================================
    // Active device tracking (Runtime API)
    //
    // Tracks which device most recently produced meaningful input so UI can swap
    // button prompts automatically. Stick drift and tiny mouse movements are
    // ignored (only deliberate input switches the active device). On a change the
    // EventBus event "input_device_changed" is emitted with arguments
    // [deviceType:int, gamepadID:int, gamepadType:int].
    // ========================================================================
    InputDeviceType GetLastUsedDeviceType() const { return m_LastUsedDeviceType; }
    uint32_t GetLastUsedGamepadID() const { return m_LastUsedGamepadID; }
    GamepadType GetGamepadType(uint32_t gamepadID = 0) const;

    // ========================================================================
    // Input contexts / action sets (Runtime API)
    //
    // Every action/axis belongs to a context (empty = always active). A non-empty
    // context only contributes input while it is active. Lets a game enable/disable
    // whole groups of inputs (e.g. "gameplay", "menu", "vehicle") at once.
    // ========================================================================
    void EnableContext(const std::string& context);
    void DisableContext(const std::string& context);
    void SetContextActive(const std::string& context, bool active);
    bool IsContextActive(const std::string& context) const;

    /**
     * Activate exactly one context: enable `context` and disable every other
     * non-empty context. The empty (always-on) context is unaffected.
     */
    void SetExclusiveContext(const std::string& context);
    std::vector<std::string> GetActiveContexts() const;

    /**
     * Enable/disable a single action or axis by name (independent of its context).
     */
    void SetActionEnabled(const std::string& actionName, bool enabled);
    void SetAxisEnabled(const std::string& axisName, bool enabled);

    // ========================================================================
    // Player slots (local multiplayer / Engine + Runtime API)
    //
    // A player slot owns a set of devices (optionally the keyboard+mouse, and any
    // number of gamepads). Per-player action/axis queries resolve only against the
    // owning player's devices. Slots are opt-in: with no slots configured all
    // queries behave as "any device".
    // ========================================================================
    void SetPlayerCount(int count);
    int GetPlayerCount() const { return static_cast<int>(m_Players.size()); }
    void ClearPlayerAssignments();
    void AssignKeyboardMouseToPlayer(int playerIndex);
    void AssignGamepadToPlayer(int playerIndex, uint32_t gamepadID);
    void UnassignGamepad(uint32_t gamepadID);
    int GetPlayerForGamepad(uint32_t gamepadID) const;     // -1 if unassigned
    int GetPlayerForKeyboardMouse() const;                 // -1 if unassigned
    bool PlayerOwnsKeyboardMouse(int playerIndex) const;
    std::vector<uint32_t> GetPlayerGamepads(int playerIndex) const;

    /**
     * When enabled, the first input from a device that is not yet assigned to any
     * player automatically assigns it to the next player slot that lacks a device
     * of that kind (couch co-op "press any button to join").
     */
    void SetAutoJoinEnabled(bool enabled) { m_AutoJoin = enabled; }
    bool IsAutoJoinEnabled() const { return m_AutoJoin; }

    // ========================================================================
    // Runtime rebinding (Engine + Runtime API)
    //
    // Mutate an action/axis binding list at runtime (for in-game rebind menus).
    // These edit the live InputMap; call SaveInputMap() to persist.
    // ========================================================================
    void AddBindingToAction(const std::string& actionName, const InputBinding& binding);
    void RemoveBindingFromAction(const std::string& actionName, size_t index);
    void ClearActionBindings(const std::string& actionName);
    std::vector<InputBinding> GetActionBindings(const std::string& actionName) const;

    void AddBindingToAxis(const std::string& axisName, const InputBinding& binding);
    void RemoveBindingFromAxis(const std::string& axisName, size_t index);
    void ClearAxisBindings(const std::string& axisName);
    std::vector<InputBinding> GetAxisBindings(const std::string& axisName) const;

    /**
     * True if the action already has a binding matching the given binding's device
     * and physical input (ignoring scale and "any-gamepad" id). Useful for
     * conflict detection and matching dispatched events against actions.
     */
    bool ActionHasBinding(const std::string& actionName, const InputBinding& binding) const;

    // ========================================================================
    // Input capture (rebind menus / Runtime API)
    //
    // Capture the next physical input as an InputBinding. While capturing, the next
    // qualifying press is consumed (it does not also fire actions), stored, and the
    // EventBus event "input_capture_complete" is emitted with [bindingJson:string].
    // Poll IsCaptureComplete()/GetCapturedBinding() or subscribe to the event.
    // ========================================================================
    enum CaptureDevice : uint32_t {
        Capture_Keyboard = 1u << 0,
        Capture_Mouse    = 1u << 1,
        Capture_Gamepad  = 1u << 2,
        Capture_All      = Capture_Keyboard | Capture_Mouse | Capture_Gamepad
    };
    void StartInputCapture(uint32_t deviceMask = Capture_All);
    void CancelInputCapture();
    bool IsCapturing() const { return m_Capturing; }
    bool IsCaptureComplete() const { return m_CaptureComplete; }
    InputBinding GetCapturedBinding() const { return m_CapturedBinding; }
    void ClearCapturedBinding() { m_CaptureComplete = false; }

    // ========================================================================
    // Glyph / prompt resolution (Runtime API)
    //
    // Resolve an action to a device-appropriate on-screen prompt. glyphId is a
    // stable identifier the game maps to its own art; label/artPath are
    // user-overridable through the glyph map.
    // ========================================================================
    InputGlyph GetActionGlyph(const std::string& actionName, int playerIndex = -1,
                              InputDeviceType deviceOverride = InputDeviceType::Unknown) const;
    std::vector<InputGlyph> GetActionGlyphs(const std::string& actionName) const;
    InputGlyph GetBindingGlyph(const InputBinding& binding) const;

    void SetGlyphLabel(const std::string& glyphId, const std::string& label);
    void SetGlyphArt(const std::string& glyphId, const std::string& artPath);
    void ClearGlyphOverride(const std::string& glyphId);
    void ClearGlyphOverrides();
    bool LoadGlyphMap(const std::string& filepath);
    bool SaveGlyphMap(const std::string& filepath) const;
    bool LoadGlyphMapFromString(const std::string& jsonString);
    std::string SaveGlyphMapToString() const;

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
     * Load input map from a JSON string
     * Used for loading from pack files where file I/O isn't available
     */
    bool LoadInputMapFromString(const std::string& jsonString);

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

    // ========================================================================
    // Clipboard (Engine API)
    //
    // The clipboard is inherently platform-specific, so the platform/runtime
    // layer installs a provider (e.g. SDL) and core stays free of any windowing
    // dependency. When no provider is installed, get returns "" and set is a no-op.
    // ========================================================================

    using ClipboardGetter = std::function<std::string()>;
    using ClipboardSetter = std::function<void(const std::string&)>;

    /**
     * Install the clipboard backend. Called by the runtime during initialization.
     */
    void SetClipboardProvider(ClipboardGetter getter, ClipboardSetter setter);

    /**
     * Get clipboard text (UTF-8). Returns "" if no provider is installed.
     */
    std::string GetClipboardText() const;

    /**
     * Set clipboard text (UTF-8). No-op if no provider is installed.
     */
    void SetClipboardText(const std::string& text);

    // ========================================================================
    // Discrete input events (Engine API)
    //
    // The platform layer pushes one InputEvent per raw event each frame. The
    // events collected during a frame's event pump become the current frame's
    // event list when Update() runs, and are consumed once by the scene tree's
    // event-based input dispatch (script on_input_event). Double-buffered so the
    // list is stable for the whole frame and cleared automatically.
    // ========================================================================

    /**
     * Queue a discrete input event for this frame (called by the platform layer
     * from its event pump, before Update()).
     */
    void PushInputEvent(const InputEvent& event);

    /**
     * The discrete events to dispatch this frame (populated by Update()).
     */
    const std::vector<InputEvent>& GetFrameEvents() const { return m_FrameEvents; }

    /**
     * Monotonic input-frame counter, advanced once per BeginFrame(). Used by
     * per-frame caches (e.g. UIControl's topmost-pointer-target arbiter) to detect
     * a new frame and rebuild at most once, independent of how many callers query.
     */
    uint64_t GetFrameNumber() const { return m_FrameNumber; }

    // ========================================================================
    // Gamepad vibration provider (Engine API)
    //
    // Rumble is platform-specific, so the platform/runtime layer installs a
    // provider (e.g. SDL) and core stays free of any windowing dependency. When
    // no provider is installed, SetGamepadVibration is a no-op.
    // ========================================================================

    using VibrationProvider = std::function<void(uint32_t gamepadID, float leftMotor,
                                                 float rightMotor, uint32_t durationMs)>;

    /**
     * Install the gamepad vibration backend. Called by the runtime during init.
     */
    void SetVibrationProvider(VibrationProvider provider);

private:
    // Devices
    std::unique_ptr<KeyboardDevice> m_Keyboard;
    std::unique_ptr<MouseDevice> m_Mouse;
    std::unique_ptr<TouchDevice> m_Touch;
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

    // Content scale: ratio of display/CSS size to actual render target size
    // Used on web where canvas CSS size differs from actual canvas pixel size
    glm::vec2 m_ContentScale = glm::vec2(1.0f, 1.0f);

    // DPI scale: device pixel ratio for high-DPI displays
    float m_DPIScale = 1.0f;
    
    // Callbacks
    std::unordered_map<std::string, ActionCallback> m_ActionCallbacks;
    std::unordered_map<std::string, AxisCallback> m_AxisCallbacks;
    
    // Settings
    float m_GlobalGamepadDeadzone = 0.15f;
    bool m_MouseCursorVisible = true;

    // Clipboard provider (installed by the platform/runtime layer)
    ClipboardGetter m_ClipboardGetter;
    ClipboardSetter m_ClipboardSetter;

    // Gamepad vibration provider (installed by the platform/runtime layer)
    VibrationProvider m_VibrationProvider;

    // Discrete input events. Events pushed by the platform layer accumulate in
    // m_IncomingEvents during the event pump; Update() promotes them to
    // m_FrameEvents (the list dispatched this frame) and clears the incoming
    // buffer, so each event is delivered exactly once.
    // m_IncomingEvents is written by the platform thread (PushInputEvent) and drained
    // by the consuming thread (Update); the mutex guards that handoff under the async
    // runtime. m_FrameEvents is only touched by the consuming thread.
    std::mutex m_EventMutex;
    std::vector<InputEvent> m_IncomingEvents;
    std::vector<InputEvent> m_FrameEvents;

    // Per-frame edge state, computed once per frame on the consuming thread in
    // Update() by sampling the held device state and diffing against the previous
    // frame. This is robust under the async runtime (the platform thread produces
    // input on a different thread than the scene consumes it): the snapshot and the
    // edge computation happen together here, so a press/release applied at any time
    // is observed exactly once. Mouse buttons indexed by MouseButton, keys by KeyCode.
    static constexpr size_t kMouseButtonCount = 8;
    static constexpr size_t kKeyCount = 512;
    std::array<bool, kMouseButtonCount> m_MousePrevHeld{};
    std::array<bool, kMouseButtonCount> m_MouseJustPressed{};
    std::array<bool, kMouseButtonCount> m_MouseJustReleased{};
    std::array<bool, kKeyCount> m_KeyPrevHeld{};
    std::array<bool, kKeyCount> m_KeyJustPressed{};
    std::array<bool, kKeyCount> m_KeyJustReleased{};

    // Per-frame scroll delta and typed text, captured (and cleared off the device) in
    // Update() so they survive to the scene read regardless of when the platform
    // thread produced them.
    glm::vec2 m_FrameScroll{0.0f, 0.0f};
    std::vector<uint32_t> m_FrameText;

    // Monotonic frame counter, advanced once per BeginFrame() (see GetFrameNumber()).
    uint64_t m_FrameNumber = 0;

    // ========================================================================
    // Active device tracking, contexts, players, capture and glyphs
    // ========================================================================

    // The device that most recently produced meaningful input.
    InputDeviceType m_LastUsedDeviceType = InputDeviceType::Unknown;
    uint32_t m_LastUsedGamepadID = 0;

    // Active input contexts. The empty context is implicitly always active and is
    // never stored here.
    std::unordered_set<std::string> m_ActiveContexts;

    // Player device-slot assignments (opt-in; empty = "any device" behavior).
    struct PlayerSlot {
        bool ownsKeyboardMouse = false;
        std::vector<uint32_t> gamepadIDs;
    };
    std::vector<PlayerSlot> m_Players;
    bool m_AutoJoin = false;

    // Input capture state (rebind menus).
    bool m_Capturing = false;
    bool m_CaptureComplete = false;
    uint32_t m_CaptureMask = 0;
    InputBinding m_CapturedBinding;

    // Glyph overrides keyed by glyphId.
    std::unordered_map<std::string, std::string> m_GlyphLabels;
    std::unordered_map<std::string, std::string> m_GlyphArt;

    // Singleton instance
    static std::unique_ptr<InputManager> s_Instance;

    // Internal helpers. playerIndex < 0 evaluates against any device.
    bool EvaluateBinding(const InputBinding& binding, int playerIndex = -1) const;
    float EvaluateBindingAxis(const InputBinding& binding, int playerIndex = -1) const;
    void UpdateActionStates();
    void UpdateAxisStates(float deltaTime);
    void ProcessCallbacks();

    // Process this frame's discrete events for device tracking, auto-join and
    // capture (runs on the consuming thread inside Update()).
    void ProcessFrameEvents();
    void HandleDeviceUsed(InputDeviceType type, uint32_t gamepadID);

    // Context / enable gating.
    bool IsActionLive(const InputAction& action) const;
    bool IsAxisLive(const InputAxis& axis) const;

    // Player ownership test for a single binding.
    bool BindingBelongsToPlayer(const InputBinding& binding, int playerIndex) const;

    // Build the glyph for one binding using the given controller family.
    InputGlyph BuildGlyph(const InputBinding& binding, GamepadType gamepadType) const;

    // Resolve which device's prompt to show for a player (auto-detection).
    InputDeviceType ResolveActiveDeviceForPlayer(int playerIndex) const;

    // Composite per-(player,name) state key. playerIndex < 0 returns `name`
    // unchanged so the global state keeps its historical keys.
    static std::string StateKey(int playerIndex, const std::string& name);
};

} // namespace input
} // namespace lupine
