/**
 * @file InputTests.cpp
 * @brief Console tests for the engine input system (lupine::input::InputManager)
 *
 * The input system is fully headless: InputManager has no window/SDL dependency.
 * The platform layer feeds device state (SetKeyState/SetButtonState/SetAxisValue)
 * and the manager derives per-frame edges, action/axis evaluation and callbacks in
 * Update(). These tests drive the manager exactly as the platform layer would and
 * assert the resulting state, so they verify real behavior without a GPU or window.
 *
 * Manager-level edge model (verified against InputManager::Update): Update() diffs
 * the held device state against the manager's own previous-frame snapshot, so a key
 * read as "just pressed" for exactly one Update() after it becomes held, and "just
 * released" for exactly one Update() after it is cleared. BeginFrame() only snapshots
 * the device-level previous state and is not required for the manager-level edges.
 */

#include "lupine/engine/Engine.hpp"
#include "lupine/input/Input.hpp"
#include "lupine/core/EventBus.hpp"
#include "TestFramework.hpp"
#include <iostream>
#include <cmath>
#include <string>

using namespace lupine;
using namespace lupine::input;

namespace {

bool FloatEq(float a, float b, float tol = 1e-4f) {
    return std::fabs(a - b) < tol;
}

InputManager& Manager() {
    InputManager::Initialize();
    return InputManager::Get();
}

bool TestKeyboardState() {
    TEST_SECTION("Input: Keyboard State & Edges");

    InputManager& mgr = Manager();
    mgr.Reset();

    KeyboardDevice* kb = mgr.GetKeyboard();
    TEST_ASSERT(kb != nullptr, "Keyboard device is available");

    kb->SetKeyState(KeyCode::Space, true);
    mgr.Update(0.016f);
    TEST_ASSERT(mgr.IsKeyPressed(KeyCode::Space), "Space reads as pressed after SetKeyState");
    TEST_ASSERT(mgr.IsKeyJustPressed(KeyCode::Space), "Space is just-pressed on the first frame held");
    TEST_ASSERT(!mgr.IsKeyJustReleased(KeyCode::Space), "Space is not just-released while held");

    mgr.Update(0.016f);
    TEST_ASSERT(mgr.IsKeyPressed(KeyCode::Space), "Space still pressed on the second frame");
    TEST_ASSERT(!mgr.IsKeyJustPressed(KeyCode::Space), "Space is not just-pressed on the second frame held");

    kb->SetKeyState(KeyCode::Space, false);
    mgr.Update(0.016f);
    TEST_ASSERT(!mgr.IsKeyPressed(KeyCode::Space), "Space released after SetKeyState false");
    TEST_ASSERT(mgr.IsKeyJustReleased(KeyCode::Space), "Space is just-released on the frame it is cleared");

    mgr.Update(0.016f);
    TEST_ASSERT(!mgr.IsKeyJustReleased(KeyCode::Space), "Space is not just-released on the following frame");

    TEST_ASSERT(!mgr.IsKeyPressed(KeyCode::A), "An untouched key reads as not pressed");

    return true;
}

bool TestMouseState() {
    TEST_SECTION("Input: Mouse State");

    InputManager& mgr = Manager();
    mgr.Reset();

    MouseDevice* mouse = mgr.GetMouse();
    TEST_ASSERT(mouse != nullptr, "Mouse device is available");

    mouse->SetButtonState(MouseButton::Left, true);
    mgr.Update(0.016f);
    TEST_ASSERT(mgr.IsMouseButtonPressed(MouseButton::Left), "Left mouse button pressed");
    TEST_ASSERT(mgr.IsMouseButtonJustPressed(MouseButton::Left), "Left mouse just-pressed first frame");

    mgr.Update(0.016f);
    TEST_ASSERT(!mgr.IsMouseButtonJustPressed(MouseButton::Left), "Left mouse not just-pressed second frame");

    mouse->SetButtonState(MouseButton::Left, false);
    mgr.Update(0.016f);
    TEST_ASSERT(mgr.IsMouseButtonJustReleased(MouseButton::Left), "Left mouse just-released when cleared");

    mouse->SetPosition(glm::vec2(0.0f, 0.0f));
    mgr.Update(0.016f);
    mouse->SetPosition(glm::vec2(120.0f, 80.0f));
    mgr.Update(0.016f);
    TEST_ASSERT(FloatEq(mouse->GetPosition().x, 120.0f) && FloatEq(mouse->GetPosition().y, 80.0f),
                "Mouse position reflects SetPosition");
    glm::vec2 delta = mgr.GetMouseDelta();
    TEST_ASSERT(FloatEq(delta.x, 120.0f) && FloatEq(delta.y, 80.0f),
                "Mouse delta equals movement since previous finalized frame");

    mgr.Update(0.016f);
    glm::vec2 delta2 = mgr.GetMouseDelta();
    TEST_ASSERT(FloatEq(delta2.x, 0.0f) && FloatEq(delta2.y, 0.0f),
                "Mouse delta is zero when the mouse does not move");

    mouse->SetScrollDelta(glm::vec2(0.0f, 3.0f));
    mgr.Update(0.016f);
    TEST_ASSERT(FloatEq(mgr.GetMouseScrollDelta().y, 3.0f), "Scroll delta is captured for the frame");
    mgr.Update(0.016f);
    TEST_ASSERT(FloatEq(mgr.GetMouseScrollDelta().y, 0.0f), "Scroll delta clears the following frame");

    return true;
}

bool TestGamepad() {
    TEST_SECTION("Input: Gamepad");

    InputManager& mgr = Manager();
    mgr.Reset();

    TEST_ASSERT(!mgr.IsGamepadConnected(0), "No gamepad connected before registration");

    mgr.RegisterGamepad(0);
    GamepadDevice* pad = mgr.GetGamepad(0);
    TEST_ASSERT(pad != nullptr, "Gamepad device created on RegisterGamepad");
    pad->SetConnected(true);
    TEST_ASSERT(mgr.IsGamepadConnected(0), "Gamepad reports connected");
    TEST_ASSERT(mgr.GetGamepadCount() >= 1, "Gamepad count includes the registered pad");

    std::vector<uint32_t> ids = mgr.GetConnectedGamepadIDs();
    bool foundId = false;
    for (uint32_t id : ids) {
        if (id == 0) { foundId = true; break; }
    }
    TEST_ASSERT(foundId, "Connected gamepad ids include id 0");

    pad->SetButtonState(GamepadButton::A, true);
    mgr.Update(0.016f);
    TEST_ASSERT(mgr.IsGamepadButtonPressed(GamepadButton::A, 0), "Gamepad A button pressed");
    TEST_ASSERT(mgr.IsGamepadButtonJustPressed(GamepadButton::A, 0), "Gamepad A just-pressed first frame");

    pad->SetAxisValue(GamepadAxis::LeftX, 1.0f);
    TEST_ASSERT(FloatEq(mgr.GetGamepadAxis(GamepadAxis::LeftX, 0), 1.0f),
                "Full-deflection axis reads as 1.0 after deadzone rescale");

    pad->SetAxisValue(GamepadAxis::LeftX, 0.05f);
    TEST_ASSERT(FloatEq(mgr.GetGamepadAxis(GamepadAxis::LeftX, 0), 0.0f),
                "Axis value below the deadzone reads as 0");

    mgr.UnregisterGamepad(0);
    TEST_ASSERT(!mgr.IsGamepadConnected(0), "Gamepad no longer connected after unregister");

    return true;
}

bool TestActions() {
    TEST_SECTION("Input: Actions");

    InputManager& mgr = Manager();
    mgr.Reset();
    mgr.GetInputMap().Clear();

    InputAction jump("jump");
    jump.AddBinding(InputBinding::FromKey(KeyCode::Space));
    jump.AddBinding(InputBinding::FromKey(KeyCode::W));
    mgr.AddAction(jump);
    TEST_ASSERT(mgr.GetInputMap().HasAction("jump"), "Action registered in the input map");

    KeyboardDevice* kb = mgr.GetKeyboard();

    kb->SetKeyState(KeyCode::Space, true);
    mgr.Update(0.016f);
    TEST_ASSERT(mgr.IsActionPressed("jump"), "Action pressed when its first binding is active");
    TEST_ASSERT(mgr.IsActionJustPressed("jump"), "Action just-pressed on the first active frame");

    mgr.Update(0.016f);
    TEST_ASSERT(!mgr.IsActionJustPressed("jump"), "Action not just-pressed while still held");

    kb->SetKeyState(KeyCode::Space, false);
    mgr.Update(0.016f);
    TEST_ASSERT(mgr.IsActionJustReleased("jump"), "Action just-released when its binding clears");

    kb->SetKeyState(KeyCode::W, true);
    mgr.Update(0.016f);
    TEST_ASSERT(mgr.IsActionPressed("jump"), "Action pressed via its second (OR) binding");
    kb->SetKeyState(KeyCode::W, false);
    mgr.Update(0.016f);

    TEST_ASSERT(!mgr.IsActionPressed("missing_action"), "Unknown action reads as not pressed");

    mgr.RemoveAction("jump");
    TEST_ASSERT(!mgr.GetInputMap().HasAction("jump"), "Action removed from the input map");

    return true;
}

bool TestAxes() {
    TEST_SECTION("Input: Axes");

    InputManager& mgr = Manager();
    mgr.Reset();
    mgr.GetInputMap().Clear();

    InputAxis moveX("move_x");
    InputBinding positive = InputBinding::FromKey(KeyCode::D);
    positive.scale = 1.0f;
    InputBinding negative = InputBinding::FromKey(KeyCode::A);
    negative.scale = -1.0f;
    moveX.AddBinding(positive);
    moveX.AddBinding(negative);
    mgr.AddAxis(moveX);
    TEST_ASSERT(mgr.GetInputMap().HasAxis("move_x"), "Axis registered in the input map");

    KeyboardDevice* kb = mgr.GetKeyboard();

    kb->SetKeyState(KeyCode::D, true);
    mgr.Update(0.016f);
    TEST_ASSERT(FloatEq(mgr.GetAxisValueRaw("move_x"), 1.0f), "Raw axis is +1 with the positive key held");

    kb->SetKeyState(KeyCode::D, false);
    kb->SetKeyState(KeyCode::A, true);
    mgr.Update(0.016f);
    TEST_ASSERT(FloatEq(mgr.GetAxisValueRaw("move_x"), -1.0f), "Raw axis is -1 with the negative key held");

    kb->SetKeyState(KeyCode::D, true);
    mgr.Update(0.016f);
    TEST_ASSERT(FloatEq(mgr.GetAxisValueRaw("move_x"), 0.0f), "Raw axis cancels to 0 with both keys held");

    kb->SetKeyState(KeyCode::A, false);
    kb->SetKeyState(KeyCode::D, false);
    mgr.Update(0.016f);

    TEST_ASSERT(FloatEq(mgr.GetAxisValueRaw("missing_axis"), 0.0f), "Unknown axis reads as 0");

    mgr.RemoveAxis("move_x");
    TEST_ASSERT(!mgr.GetInputMap().HasAxis("move_x"), "Axis removed from the input map");

    return true;
}

bool TestActionStrength() {
    TEST_SECTION("Input: Action Strength (analog)");

    InputManager& mgr = Manager();
    mgr.Reset();
    mgr.GetInputMap().Clear();
    mgr.RegisterGamepad(0);
    GamepadDevice* pad = mgr.GetGamepad(0);
    pad->SetConnected(true);

    InputAction accelerate("accelerate");
    accelerate.SetDeadzone(0.1f);
    accelerate.AddBinding(InputBinding::FromGamepadAxis(GamepadAxis::RightTrigger, 1.0f, 0));
    mgr.AddAction(accelerate);

    pad->SetAxisValue(GamepadAxis::RightTrigger, 1.0f);
    mgr.Update(0.016f);
    TEST_ASSERT(FloatEq(mgr.GetActionStrength("accelerate"), 1.0f),
                "Analog action strength is full at full trigger deflection");

    pad->SetAxisValue(GamepadAxis::RightTrigger, 0.05f);
    mgr.Update(0.016f);
    TEST_ASSERT(FloatEq(mgr.GetActionStrength("accelerate"), 0.0f),
                "Analog action strength is 0 below the action deadzone");

    InputAction fire("fire");
    fire.AddBinding(InputBinding::FromKey(KeyCode::Enter));
    mgr.AddAction(fire);
    mgr.GetKeyboard()->SetKeyState(KeyCode::Enter, true);
    mgr.Update(0.016f);
    TEST_ASSERT(FloatEq(mgr.GetActionStrength("fire"), 1.0f),
                "Digital action strength is 1.0 when its key is held");
    mgr.GetKeyboard()->SetKeyState(KeyCode::Enter, false);
    mgr.Update(0.016f);

    mgr.UnregisterGamepad(0);
    return true;
}

bool TestSerialization() {
    TEST_SECTION("Input: Input Map Serialization");

    InputManager& mgr = Manager();
    mgr.GetInputMap().Clear();

    InputAction jump("jump");
    jump.AddBinding(InputBinding::FromKey(KeyCode::Space));
    jump.AddBinding(InputBinding::FromGamepadButton(GamepadButton::A, 0));
    mgr.AddAction(jump);

    InputAxis moveX("move_x");
    InputBinding pos = InputBinding::FromKey(KeyCode::D);
    pos.scale = 1.0f;
    moveX.AddBinding(pos);
    moveX.AddBinding(InputBinding::FromGamepadAxis(GamepadAxis::LeftX, 1.0f, 0));
    mgr.AddAxis(moveX);

    nlohmann::json serialized = mgr.GetInputMap().Serialize();
    TEST_ASSERT(!serialized.empty(), "Input map serializes to a non-empty JSON object");

    InputMap restored;
    restored.Deserialize(serialized);
    TEST_ASSERT(restored.HasAction("jump"), "Deserialized map restores the action");
    TEST_ASSERT(restored.HasAxis("move_x"), "Deserialized map restores the axis");

    const InputAction* restoredJump = restored.GetAction("jump");
    TEST_ASSERT(restoredJump != nullptr && restoredJump->GetBindings().size() == 2,
                "Action bindings survive the round-trip");
    const InputAxis* restoredAxis = restored.GetAxis("move_x");
    TEST_ASSERT(restoredAxis != nullptr && restoredAxis->GetBindings().size() == 2,
                "Axis bindings survive the round-trip");

    std::string asString = serialized.dump();
    TEST_ASSERT(mgr.LoadInputMapFromString(asString), "Input map loads from a JSON string");
    TEST_ASSERT(mgr.GetInputMap().HasAction("jump"), "Action present after loading from string");

    mgr.GetInputMap().Clear();
    return true;
}

bool TestWindowAndScale() {
    TEST_SECTION("Input: Window Size, Content & DPI Scale");

    InputManager& mgr = Manager();

    mgr.SetWindowSize(1280, 720);
    glm::ivec2 size = mgr.GetWindowSize();
    TEST_ASSERT(size.x == 1280 && size.y == 720, "Window size round-trips");

    mgr.SetContentScale(1.5f, 2.0f);
    glm::vec2 cs = mgr.GetContentScale();
    TEST_ASSERT(FloatEq(cs.x, 1.5f) && FloatEq(cs.y, 2.0f), "Content scale round-trips");

    mgr.SetDPIScale(2.0f);
    TEST_ASSERT(FloatEq(mgr.GetDPIScale(), 2.0f), "DPI scale round-trips");

    mgr.SetGlobalGamepadDeadzone(0.25f);
    TEST_ASSERT(FloatEq(mgr.GetGlobalGamepadDeadzone(), 0.25f), "Global gamepad deadzone round-trips");

    mgr.SetContentScale(1.0f, 1.0f);
    mgr.SetDPIScale(1.0f);
    return true;
}

bool TestCallbacks() {
    TEST_SECTION("Input: Action Callbacks");

    InputManager& mgr = Manager();
    mgr.Reset();
    mgr.GetInputMap().Clear();

    InputAction interact("interact");
    interact.AddBinding(InputBinding::FromKey(KeyCode::E));
    mgr.AddAction(interact);

    int pressedCount = 0;
    int releasedCount = 0;
    mgr.RegisterActionCallback("interact", [&](const std::string& name, bool pressed) {
        if (name == "interact") {
            if (pressed) ++pressedCount; else ++releasedCount;
        }
    });

    mgr.GetKeyboard()->SetKeyState(KeyCode::E, true);
    mgr.Update(0.016f);
    TEST_ASSERT(pressedCount == 1, "Action press callback fired exactly once");

    mgr.Update(0.016f);
    TEST_ASSERT(pressedCount == 1, "Press callback does not refire while held");

    mgr.GetKeyboard()->SetKeyState(KeyCode::E, false);
    mgr.Update(0.016f);
    TEST_ASSERT(releasedCount == 1, "Action release callback fired exactly once");

    mgr.UnregisterActionCallback("interact");
    mgr.GetInputMap().Clear();
    return true;
}

// Build a discrete InputEvent for driving device-tracking/capture paths.
namespace {
InputEvent KeyDownEvent(KeyCode key) {
    InputEvent e;
    e.type = InputEventType::KeyDown;
    e.key = key;
    return e;
}
InputEvent GamepadButtonDownEvent(GamepadButton button, uint32_t gamepadID) {
    InputEvent e;
    e.type = InputEventType::GamepadButtonDown;
    e.gamepadButton = button;
    e.gamepadID = gamepadID;
    return e;
}
} // namespace

bool TestActiveDeviceTracking() {
    TEST_SECTION("Input: Active Device Tracking");

    InputManager& mgr = Manager();
    mgr.Reset();

    mgr.PushInputEvent(KeyDownEvent(KeyCode::Space));
    mgr.Update(0.016f);
    TEST_ASSERT(mgr.GetLastUsedDeviceType() == InputDeviceType::Keyboard,
                "Keyboard event marks the keyboard as the active device");

    mgr.RegisterGamepad(0);
    mgr.GetGamepad(0)->SetConnected(true);
    mgr.PushInputEvent(GamepadButtonDownEvent(GamepadButton::A, 0));
    mgr.Update(0.016f);
    TEST_ASSERT(mgr.GetLastUsedDeviceType() == InputDeviceType::Gamepad,
                "Gamepad button event switches the active device to gamepad");
    TEST_ASSERT(mgr.GetLastUsedGamepadID() == 0, "Active gamepad id tracks the source pad");

    mgr.UnregisterGamepad(0);
    return true;
}

bool TestGamepadType() {
    TEST_SECTION("Input: Gamepad Type");

    InputManager& mgr = Manager();
    mgr.Reset();
    mgr.RegisterGamepad(0);
    GamepadDevice* pad = mgr.GetGamepad(0);
    pad->SetConnected(true);
    pad->SetGamepadType(GamepadType::PlayStation);

    TEST_ASSERT(mgr.GetGamepadType(0) == GamepadType::PlayStation,
                "GetGamepadType returns the family set on the device");
    TEST_ASSERT(GamepadFaceLabel(GamepadButton::A, GamepadType::PlayStation) == "Cross",
                "PlayStation face label for the south button is 'Cross'");
    TEST_ASSERT(GamepadFaceLabel(GamepadButton::A, GamepadType::Nintendo) == "B",
                "Nintendo south button is labeled 'B' (swapped layout)");

    mgr.UnregisterGamepad(0);
    return true;
}

bool TestContexts() {
    TEST_SECTION("Input: Contexts / Action Sets");

    InputManager& mgr = Manager();
    mgr.Reset();
    mgr.GetInputMap().Clear();

    InputAction fire("fire");
    fire.SetContext("gameplay");
    fire.AddBinding(InputBinding::FromKey(KeyCode::Enter));
    mgr.AddAction(fire);

    KeyboardDevice* kb = mgr.GetKeyboard();
    kb->SetKeyState(KeyCode::Enter, true);

    mgr.Update(0.016f);
    TEST_ASSERT(!mgr.IsActionPressed("fire"),
                "Action in an inactive context does not register while its key is held");

    mgr.EnableContext("gameplay");
    mgr.Update(0.016f);
    TEST_ASSERT(mgr.IsActionPressed("fire"),
                "Action registers once its context is enabled");

    mgr.SetExclusiveContext("menu");
    mgr.Update(0.016f);
    TEST_ASSERT(!mgr.IsActionPressed("fire"),
                "SetExclusiveContext disables the previous context");
    TEST_ASSERT(mgr.IsContextActive("menu") && !mgr.IsContextActive("gameplay"),
                "Only the exclusive context is active");

    mgr.EnableContext("gameplay");
    mgr.SetActionEnabled("fire", false);
    mgr.Update(0.016f);
    TEST_ASSERT(!mgr.IsActionPressed("fire"),
                "A disabled action never registers even in an active context");

    kb->SetKeyState(KeyCode::Enter, false);
    mgr.Update(0.016f);
    mgr.SetActionEnabled("fire", true);
    mgr.GetInputMap().Clear();
    return true;
}

bool TestPlayerSlots() {
    TEST_SECTION("Input: Local Multiplayer Player Slots");

    InputManager& mgr = Manager();
    mgr.Reset();
    mgr.GetInputMap().Clear();
    mgr.SetPlayerCount(2);
    mgr.AssignKeyboardMouseToPlayer(0);
    mgr.RegisterGamepad(0);
    mgr.GetGamepad(0)->SetConnected(true);
    mgr.AssignGamepadToPlayer(1, 0);

    TEST_ASSERT(mgr.GetPlayerForKeyboardMouse() == 0, "Player 0 owns the keyboard & mouse");
    TEST_ASSERT(mgr.GetPlayerForGamepad(0) == 1, "Player 1 owns gamepad 0");

    InputAction jump("jump");
    jump.AddBinding(InputBinding::FromKey(KeyCode::Space));
    jump.AddBinding(InputBinding::FromGamepadButton(GamepadButton::A, 0));
    mgr.AddAction(jump);

    mgr.GetKeyboard()->SetKeyState(KeyCode::Space, true);
    mgr.Update(0.016f);
    TEST_ASSERT(mgr.IsActionPressed("jump", 0), "Player 0 (KBM) sees the keyboard binding");
    TEST_ASSERT(!mgr.IsActionPressed("jump", 1), "Player 1 (gamepad) does not see the keyboard binding");

    mgr.GetKeyboard()->SetKeyState(KeyCode::Space, false);
    mgr.GetGamepad(0)->SetButtonState(GamepadButton::A, true);
    mgr.Update(0.016f);
    TEST_ASSERT(mgr.IsActionPressed("jump", 1), "Player 1 sees its own gamepad's button binding");
    TEST_ASSERT(!mgr.IsActionPressed("jump", 0), "Player 0 does not see the other player's gamepad");
    TEST_ASSERT(mgr.IsActionPressed("jump"), "The global (-1) view still sees any device");

    mgr.GetGamepad(0)->SetButtonState(GamepadButton::A, false);
    mgr.Update(0.016f);
    mgr.UnregisterGamepad(0);
    mgr.SetPlayerCount(0);
    mgr.GetInputMap().Clear();
    return true;
}

bool TestRebindingAndCapture() {
    TEST_SECTION("Input: Runtime Rebinding & Capture");

    InputManager& mgr = Manager();
    mgr.Reset();
    mgr.GetInputMap().Clear();

    InputAction jump("jump");
    mgr.AddAction(jump);

    mgr.AddBindingToAction("jump", InputBinding::FromKey(KeyCode::Space));
    mgr.AddBindingToAction("jump", InputBinding::FromGamepadButton(GamepadButton::A, 0));
    TEST_ASSERT(mgr.GetActionBindings("jump").size() == 2, "Bindings added at runtime");

    InputBinding probe = InputBinding::FromKey(KeyCode::Space);
    TEST_ASSERT(mgr.ActionHasBinding("jump", probe), "ActionHasBinding finds the added key binding");

    mgr.RemoveBindingFromAction("jump", 0);
    TEST_ASSERT(mgr.GetActionBindings("jump").size() == 1, "Binding removed at runtime");

    // Capture the next physical input.
    mgr.StartInputCapture();
    TEST_ASSERT(mgr.IsCapturing(), "Capture mode is active");
    mgr.PushInputEvent(KeyDownEvent(KeyCode::J));
    mgr.Update(0.016f);
    TEST_ASSERT(!mgr.IsCapturing(), "Capture ends when an input arrives");
    TEST_ASSERT(mgr.IsCaptureComplete(), "Capture is marked complete");
    TEST_ASSERT(mgr.GetCapturedBinding().keyCode == KeyCode::J,
                "Captured binding records the pressed key");

    mgr.ClearCapturedBinding();
    TEST_ASSERT(!mgr.IsCaptureComplete(), "ClearCapturedBinding resets the complete flag");

    mgr.GetInputMap().Clear();
    return true;
}

bool TestGlyphs() {
    TEST_SECTION("Input: Glyph / Prompt Resolution");

    InputManager& mgr = Manager();
    mgr.Reset();
    mgr.GetInputMap().Clear();

    InputAction interact("interact");
    interact.AddBinding(InputBinding::FromKey(KeyCode::E));
    interact.AddBinding(InputBinding::FromGamepadButton(GamepadButton::X, 0));
    mgr.AddAction(interact);
    mgr.RegisterGamepad(0);
    mgr.GetGamepad(0)->SetConnected(true);
    mgr.GetGamepad(0)->SetGamepadType(GamepadType::Xbox);

    InputGlyph kbGlyph = mgr.GetActionGlyph("interact", -1, InputDeviceType::Keyboard);
    TEST_ASSERT(kbGlyph.glyphId == "key_e", "Keyboard glyph id is 'key_e'");
    TEST_ASSERT(kbGlyph.label == "E", "Keyboard glyph label is 'E'");

    InputGlyph padGlyph = mgr.GetActionGlyph("interact", -1, InputDeviceType::Gamepad);
    TEST_ASSERT(padGlyph.glyphId == "gamepad_xbox_x", "Xbox gamepad glyph id is 'gamepad_xbox_x'");
    TEST_ASSERT(padGlyph.label == "X", "Xbox gamepad glyph label is 'X'");

    mgr.SetGlyphLabel("key_e", "Use");
    mgr.SetGlyphArt("key_e", "res://glyphs/e.png");
    InputGlyph overridden = mgr.GetActionGlyph("interact", -1, InputDeviceType::Keyboard);
    TEST_ASSERT(overridden.label == "Use", "Glyph label override is applied");
    TEST_ASSERT(overridden.artPath == "res://glyphs/e.png", "Glyph art override is applied");

    mgr.ClearGlyphOverrides();
    InputGlyph restored = mgr.GetActionGlyph("interact", -1, InputDeviceType::Keyboard);
    TEST_ASSERT(restored.label == "E", "ClearGlyphOverrides restores the default label");

    mgr.UnregisterGamepad(0);
    mgr.GetInputMap().Clear();
    return true;
}

bool TestActionEvents() {
    TEST_SECTION("Input: Action Delegation Events");

    InputManager& mgr = Manager();
    mgr.Reset();
    mgr.GetInputMap().Clear();
    core::EventBus::Get().Clear();

    InputAction jump("jump");
    jump.AddBinding(InputBinding::FromKey(KeyCode::Space));
    mgr.AddAction(jump);

    int pressed = 0;
    int released = 0;
    core::EventBus::Get().Subscribe("action:jump", [&](const core::SignalArgs& args) {
        if (!args.empty() && args[0].is_boolean()) {
            if (args[0].get<bool>()) ++pressed; else ++released;
        }
    });

    mgr.GetKeyboard()->SetKeyState(KeyCode::Space, true);
    mgr.Update(0.016f);
    TEST_ASSERT(pressed == 1, "EventBus 'action:jump' fires once on press");

    mgr.Update(0.016f);
    TEST_ASSERT(pressed == 1, "Action event does not refire while held");

    mgr.GetKeyboard()->SetKeyState(KeyCode::Space, false);
    mgr.Update(0.016f);
    TEST_ASSERT(released == 1, "EventBus 'action:jump' fires once on release");

    core::EventBus::Get().Clear();
    mgr.GetInputMap().Clear();
    return true;
}

bool TestContextEnabledSerialization() {
    TEST_SECTION("Input: Context/Enabled Serialization");

    InputAction action("special");
    action.SetContext("vehicle");
    action.SetEnabled(false);
    action.AddBinding(InputBinding::FromKey(KeyCode::Space));

    nlohmann::json j = action.ToJson();
    TEST_ASSERT(j.value("context", std::string()) == "vehicle", "Action context serializes");
    TEST_ASSERT(j.value("enabled", true) == false, "Action enabled flag serializes");

    InputAction restored = InputAction::FromJson(j);
    TEST_ASSERT(restored.GetContext() == "vehicle", "Action context round-trips");
    TEST_ASSERT(!restored.IsEnabled(), "Action enabled flag round-trips");

    // Backward compatibility: missing fields default to "" / true.
    nlohmann::json legacy;
    legacy["name"] = "legacy";
    legacy["deadzone"] = 0.5f;
    legacy["bindings"] = nlohmann::json::array();
    InputAction legacyAction = InputAction::FromJson(legacy);
    TEST_ASSERT(legacyAction.GetContext().empty() && legacyAction.IsEnabled(),
                "Legacy action without context/enabled defaults to always-active");

    return true;
}

} // namespace

void RunInputTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "INPUT TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    lupine_test::SetCurrentSuite("Input");

    engine::InitializeEngine();

    bool allPassed = true;
    allPassed &= TestKeyboardState();
    allPassed &= TestMouseState();
    allPassed &= TestGamepad();
    allPassed &= TestActions();
    allPassed &= TestAxes();
    allPassed &= TestActionStrength();
    allPassed &= TestSerialization();
    allPassed &= TestWindowAndScale();
    allPassed &= TestCallbacks();
    allPassed &= TestActiveDeviceTracking();
    allPassed &= TestGamepadType();
    allPassed &= TestContexts();
    allPassed &= TestPlayerSlots();
    allPassed &= TestRebindingAndCapture();
    allPassed &= TestGlyphs();
    allPassed &= TestActionEvents();
    allPassed &= TestContextEnabledSerialization();

    std::cout << "\n========================================" << std::endl;
    if (allPassed) {
        std::cout << "ALL INPUT TESTS PASSED!" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================\n" << std::endl;
}
