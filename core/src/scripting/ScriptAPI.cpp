#include "lupine/scripting/ScriptAPI.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include "lupine/core/Serialization.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/core/InterfaceRegistry.hpp"
#include "lupine/core/ArchetypeRegistry.hpp"
#include "lupine/core/SceneManager.hpp"
#include "lupine/core/Prefab.hpp"
#include "lupine/core/SceneInstance.hpp"
#include "lupine/core/SignalDispatcher.hpp"
#include "lupine/core/EventBus.hpp"
#include "lupine/input/InputManager.hpp"
#include "lupine/input/InputCodes.hpp"
#include "lupine/platform/DisplayServer.hpp"
#include "lupine/platform/Platform.hpp"
#include "lupine/core/CameraNodes.hpp"
#include "lupine/rendering/RenderCamera.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/TextureCache.hpp"
#include "lupine/rendering/TextureUpload.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/rendering/debug/DebugDrawQueue.hpp"
#include "lupine/rendering/debug/DebugDraw.hpp"
#include "lupine/math/Camera.hpp"
#include "lupine/math/Gradient.hpp"
#include "lupine/math/Curve.hpp"
#include "lupine/audio/AudioManager.hpp"
#include "lupine/localization/LocalizationManager.hpp"
#include "lupine/ui/ThemeManager.hpp"
#include "lupine/asset/Asset.hpp"
#include "lupine/platform/VirtualFileSystem.hpp"
#include "lupine/asset/ImageAsset.hpp"
#include "lupine/asset/AudioAsset.hpp"
#include "lupine/asset/ModelAsset.hpp"
#include "lupine/asset/ArchetypeInstance.hpp"
#include "lupine/asset/AsyncAssetLoader.hpp"
#include "lupine/save/SaveGameManager.hpp"
#include "lupine/save/SceneSaveState.hpp"
#include "lupine/scripting/ScriptingCore.hpp"
#include "lupine/physics2d/Physics2DWorld.hpp"
#include "lupine/physics2d/RigidBody2D.hpp"
#include "lupine/physics3d/Physics3DWorld.hpp"
#include "lupine/physics3d/RigidBody3D.hpp"
#include "lupine/components/RigidBody2DComponent.hpp"
#include "lupine/components/RigidBody3DComponent.hpp"
#include "lupine/components/CharacterController2D.hpp"
#include "lupine/components/CharacterController3D.hpp"
#include "lupine/components/Timer.hpp"
#include "lupine/components/Tween.hpp"
#include "lupine/logger/Logger.hpp"
#include <random>
#include <chrono>
#include <cmath>
#include <mutex>
#include <algorithm>

namespace lupine {
namespace scripting {

static auto s_StartTime = std::chrono::high_resolution_clock::now();
static int s_FrameCount = 0;

ScriptAPI::ScriptAPI() {
}

ScriptAPI::~ScriptAPI() {
    // Drop any async archetype requests still owned by this script so the global
    // loader does not retain records (or deliver callbacks) for a destroyed node.
    asset::AsyncAssetLoader& loader = asset::AsyncAssetLoader::GetInstance();
    for (std::unordered_map<uint64_t, AsyncArchetypeRequest>::iterator it = m_AsyncArchetypeRequests.begin();
         it != m_AsyncArchetypeRequests.end(); ++it) {
        loader.Forget(it->first);
    }
    m_AsyncArchetypeRequests.clear();
}

void ScriptAPI::SetOwner(core::Node* owner) {
    m_Owner = owner;
}

void ScriptAPI::SetGamePaused(bool paused) {
    core::SceneManager* sceneManager = m_SceneManager ? m_SceneManager : core::SceneManager::GetInstance();
    if (sceneManager) {
        sceneManager->SetGamePaused(paused);
    }
}

bool ScriptAPI::IsGamePaused() const {
    core::SceneManager* sceneManager = m_SceneManager ? m_SceneManager : core::SceneManager::GetInstance();
    return sceneManager ? sceneManager->IsGamePaused() : false;
}

void ScriptAPI::RequestQuit() {
    core::SceneManager* sceneManager = m_SceneManager ? m_SceneManager : core::SceneManager::GetInstance();
    if (sceneManager) {
        sceneManager->RequestQuit();
    }
}

float ScriptAPI::GetDeltaTime() const {
    return m_DeltaTime;
}

bool ScriptAPI::IsActionPressed(const std::string& action, int player) const {
    return input::InputManager::Get().IsActionPressed(action, player);
}

bool ScriptAPI::IsActionJustPressed(const std::string& action, int player) const {
    return input::InputManager::Get().IsActionJustPressed(action, player);
}

bool ScriptAPI::IsActionJustReleased(const std::string& action, int player) const {
    return input::InputManager::Get().IsActionJustReleased(action, player);
}

float ScriptAPI::GetActionStrength(const std::string& action, int player) const {
    return input::InputManager::Get().GetActionStrength(action, player);
}

float ScriptAPI::GetAxis(const std::string& axis, int player) const {
    return input::InputManager::Get().GetAxisValue(axis, player);
}

math::Vec2 ScriptAPI::GetVector(const std::string& negativeX, const std::string& positiveX,
                                const std::string& negativeY, const std::string& positiveY,
                                int player) const {
    // Analog-aware: each direction contributes its action strength, so gamepad
    // sticks/triggers bound to the actions produce smooth diagonal movement.
    input::InputManager& input = input::InputManager::Get();
    float x = input.GetActionStrength(positiveX, player) - input.GetActionStrength(negativeX, player);
    float y = input.GetActionStrength(positiveY, player) - input.GetActionStrength(negativeY, player);

    // Clamp the resulting vector to unit length so diagonal input is not faster.
    float lengthSq = x * x + y * y;
    if (lengthSq > 1.0f) {
        float length = std::sqrt(lengthSq);
        x /= length;
        y /= length;
    }

    return math::Vec2(x, y);
}

bool ScriptAPI::IsKeyPressed(int keyCode) const {
    return input::InputManager::Get().IsKeyPressed(static_cast<input::KeyCode>(keyCode));
}

bool ScriptAPI::IsKeyJustPressed(int keyCode) const {
    return input::InputManager::Get().IsKeyJustPressed(static_cast<input::KeyCode>(keyCode));
}

bool ScriptAPI::IsKeyJustReleased(int keyCode) const {
    return input::InputManager::Get().IsKeyJustReleased(static_cast<input::KeyCode>(keyCode));
}

bool ScriptAPI::IsMouseButtonPressed(int button) const {
    return input::InputManager::Get().IsMouseButtonPressed(static_cast<input::MouseButton>(button));
}

bool ScriptAPI::IsMouseButtonJustPressed(int button) const {
    return input::InputManager::Get().IsMouseButtonJustPressed(static_cast<input::MouseButton>(button));
}

bool ScriptAPI::IsMouseButtonJustReleased(int button) const {
    return input::InputManager::Get().IsMouseButtonJustReleased(static_cast<input::MouseButton>(button));
}

math::Vec2 ScriptAPI::GetMousePosition() const {
    auto pos = input::InputManager::Get().GetMousePosition();
    return math::Vec2(pos.x, pos.y);
}

math::Vec2 ScriptAPI::GetMouseDelta() const {
    auto delta = input::InputManager::Get().GetMouseDelta();
    return math::Vec2(delta.x, delta.y);
}

math::Vec2 ScriptAPI::GetMouseScrollDelta() const {
    auto scroll = input::InputManager::Get().GetMouseScrollDelta();
    return math::Vec2(scroll.x, scroll.y);
}

bool ScriptAPI::IsGamepadConnected(int gamepadId) const {
    return input::InputManager::Get().IsGamepadConnected(static_cast<uint32_t>(gamepadId));
}

int ScriptAPI::GetGamepadCount() const {
    return static_cast<int>(input::InputManager::Get().GetGamepadCount());
}

std::vector<int> ScriptAPI::GetConnectedGamepadIds() const {
    std::vector<uint32_t> ids = input::InputManager::Get().GetConnectedGamepadIDs();
    std::vector<int> result;
    result.reserve(ids.size());
    for (uint32_t id : ids) {
        result.push_back(static_cast<int>(id));
    }
    return result;
}

std::string ScriptAPI::GetGamepadName(int gamepadId) const {
    return input::InputManager::Get().GetGamepadName(static_cast<uint32_t>(gamepadId));
}

bool ScriptAPI::IsGamepadButtonPressed(int button, int gamepadId) const {
    return input::InputManager::Get().IsGamepadButtonPressed(
        static_cast<input::GamepadButton>(button), static_cast<uint32_t>(gamepadId));
}

bool ScriptAPI::IsGamepadButtonJustPressed(int button, int gamepadId) const {
    return input::InputManager::Get().IsGamepadButtonJustPressed(
        static_cast<input::GamepadButton>(button), static_cast<uint32_t>(gamepadId));
}

bool ScriptAPI::IsGamepadButtonJustReleased(int button, int gamepadId) const {
    return input::InputManager::Get().IsGamepadButtonJustReleased(
        static_cast<input::GamepadButton>(button), static_cast<uint32_t>(gamepadId));
}

float ScriptAPI::GetGamepadAxis(int axis, int gamepadId) const {
    return input::InputManager::Get().GetGamepadAxis(
        static_cast<input::GamepadAxis>(axis), static_cast<uint32_t>(gamepadId));
}

void ScriptAPI::SetGamepadVibration(int gamepadId, float leftMotor, float rightMotor, float durationSeconds) {
    uint32_t durationMs = durationSeconds > 0.0f ? static_cast<uint32_t>(durationSeconds * 1000.0f) : 0;
    input::InputManager::Get().SetGamepadVibration(
        static_cast<uint32_t>(gamepadId), leftMotor, rightMotor, durationMs);
}

void ScriptAPI::StopGamepadVibration(int gamepadId) {
    input::InputManager::Get().StopGamepadVibration(static_cast<uint32_t>(gamepadId));
}

void ScriptAPI::SetGamepadDeadzone(float deadzone) {
    input::InputManager::Get().SetGlobalGamepadDeadzone(deadzone);
}

float ScriptAPI::GetGamepadDeadzone() const {
    return input::InputManager::Get().GetGlobalGamepadDeadzone();
}

bool ScriptAPI::IsTouchAvailable() const {
    return input::InputManager::Get().IsTouchAvailable();
}

bool ScriptAPI::IsTouching() const {
    return input::InputManager::Get().IsTouching();
}

int ScriptAPI::GetTouchCount() const {
    return static_cast<int>(input::InputManager::Get().GetTouchCount());
}

math::Vec2 ScriptAPI::GetTouchPosition(int index) const {
    if (index < 0) {
        return math::Vec2(0.0f, 0.0f);
    }
    auto pos = input::InputManager::Get().GetTouchPosition(static_cast<size_t>(index));
    return math::Vec2(pos.x, pos.y);
}

bool ScriptAPI::IsTouchJustStarted() const {
    return input::InputManager::Get().IsTouchJustStarted();
}

bool ScriptAPI::IsTouchJustEnded() const {
    return input::InputManager::Get().IsTouchJustEnded();
}

std::string ScriptAPI::GetClipboardText() const {
    return input::InputManager::Get().GetClipboardText();
}

void ScriptAPI::SetClipboardText(const std::string& text) {
    input::InputManager::Get().SetClipboardText(text);
}

// ============================================================================
// Active device detection
// ============================================================================

int ScriptAPI::GetActiveDeviceType() const {
    return static_cast<int>(input::InputManager::Get().GetLastUsedDeviceType());
}

int ScriptAPI::GetLastGamepadId() const {
    return static_cast<int>(input::InputManager::Get().GetLastUsedGamepadID());
}

int ScriptAPI::GetGamepadType(int gamepadId) const {
    return static_cast<int>(
        input::InputManager::Get().GetGamepadType(static_cast<uint32_t>(gamepadId)));
}

// ============================================================================
// Input contexts / action sets
// ============================================================================

void ScriptAPI::EnableInputContext(const std::string& context) {
    input::InputManager::Get().EnableContext(context);
}

void ScriptAPI::DisableInputContext(const std::string& context) {
    input::InputManager::Get().DisableContext(context);
}

void ScriptAPI::SetInputContextActive(const std::string& context, bool active) {
    input::InputManager::Get().SetContextActive(context, active);
}

bool ScriptAPI::IsInputContextActive(const std::string& context) const {
    return input::InputManager::Get().IsContextActive(context);
}

void ScriptAPI::SetExclusiveInputContext(const std::string& context) {
    input::InputManager::Get().SetExclusiveContext(context);
}

std::vector<std::string> ScriptAPI::GetActiveInputContexts() const {
    return input::InputManager::Get().GetActiveContexts();
}

void ScriptAPI::SetActionEnabled(const std::string& action, bool enabled) {
    input::InputManager::Get().SetActionEnabled(action, enabled);
}

void ScriptAPI::SetAxisEnabled(const std::string& axis, bool enabled) {
    input::InputManager::Get().SetAxisEnabled(axis, enabled);
}

// ============================================================================
// Local multiplayer player slots
// ============================================================================

void ScriptAPI::SetPlayerCount(int count) {
    input::InputManager::Get().SetPlayerCount(count);
}

int ScriptAPI::GetPlayerCount() const {
    return input::InputManager::Get().GetPlayerCount();
}

void ScriptAPI::ClearPlayerAssignments() {
    input::InputManager::Get().ClearPlayerAssignments();
}

void ScriptAPI::AssignKeyboardMouseToPlayer(int player) {
    input::InputManager::Get().AssignKeyboardMouseToPlayer(player);
}

void ScriptAPI::AssignGamepadToPlayer(int player, int gamepadId) {
    input::InputManager::Get().AssignGamepadToPlayer(player, static_cast<uint32_t>(gamepadId));
}

void ScriptAPI::UnassignGamepad(int gamepadId) {
    input::InputManager::Get().UnassignGamepad(static_cast<uint32_t>(gamepadId));
}

int ScriptAPI::GetPlayerForGamepad(int gamepadId) const {
    return input::InputManager::Get().GetPlayerForGamepad(static_cast<uint32_t>(gamepadId));
}

int ScriptAPI::GetPlayerForKeyboardMouse() const {
    return input::InputManager::Get().GetPlayerForKeyboardMouse();
}

bool ScriptAPI::PlayerOwnsKeyboardMouse(int player) const {
    return input::InputManager::Get().PlayerOwnsKeyboardMouse(player);
}

std::vector<int> ScriptAPI::GetPlayerGamepads(int player) const {
    std::vector<uint32_t> ids = input::InputManager::Get().GetPlayerGamepads(player);
    std::vector<int> out;
    out.reserve(ids.size());
    for (uint32_t id : ids) {
        out.push_back(static_cast<int>(id));
    }
    return out;
}

void ScriptAPI::SetAutoJoinEnabled(bool enabled) {
    input::InputManager::Get().SetAutoJoinEnabled(enabled);
}

bool ScriptAPI::IsAutoJoinEnabled() const {
    return input::InputManager::Get().IsAutoJoinEnabled();
}

// ============================================================================
// Runtime rebinding
// ============================================================================

void ScriptAPI::AddActionKey(const std::string& action, int keyCode) {
    input::InputManager::Get().AddBindingToAction(
        action, input::InputBinding::FromKey(static_cast<input::KeyCode>(keyCode)));
}

void ScriptAPI::AddActionMouseButton(const std::string& action, int button) {
    input::InputManager::Get().AddBindingToAction(
        action, input::InputBinding::FromMouseButton(static_cast<input::MouseButton>(button)));
}

void ScriptAPI::AddActionGamepadButton(const std::string& action, int button, int gamepadId) {
    input::InputManager::Get().AddBindingToAction(
        action, input::InputBinding::FromGamepadButton(
                    static_cast<input::GamepadButton>(button), static_cast<uint32_t>(gamepadId)));
}

void ScriptAPI::AddActionGamepadAxis(const std::string& action, int axis, float scale, int gamepadId) {
    input::InputManager::Get().AddBindingToAction(
        action, input::InputBinding::FromGamepadAxis(
                    static_cast<input::GamepadAxis>(axis), scale, static_cast<uint32_t>(gamepadId)));
}

void ScriptAPI::RemoveActionBinding(const std::string& action, int index) {
    if (index >= 0) {
        input::InputManager::Get().RemoveBindingFromAction(action, static_cast<size_t>(index));
    }
}

void ScriptAPI::ClearActionBindings(const std::string& action) {
    input::InputManager::Get().ClearActionBindings(action);
}

nlohmann::json ScriptAPI::GetActionBindings(const std::string& action) const {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& b : input::InputManager::Get().GetActionBindings(action)) {
        arr.push_back(b.ToJson());
    }
    return arr;
}

void ScriptAPI::AddAxisKey(const std::string& axis, int keyCode, float scale) {
    input::InputBinding b = input::InputBinding::FromKey(static_cast<input::KeyCode>(keyCode));
    b.scale = scale;
    input::InputManager::Get().AddBindingToAxis(axis, b);
}

void ScriptAPI::AddAxisGamepadAxis(const std::string& axis, int gamepadAxis, float scale, int gamepadId) {
    input::InputManager::Get().AddBindingToAxis(
        axis, input::InputBinding::FromGamepadAxis(
                  static_cast<input::GamepadAxis>(gamepadAxis), scale, static_cast<uint32_t>(gamepadId)));
}

void ScriptAPI::RemoveAxisBinding(const std::string& axis, int index) {
    if (index >= 0) {
        input::InputManager::Get().RemoveBindingFromAxis(axis, static_cast<size_t>(index));
    }
}

void ScriptAPI::ClearAxisBindings(const std::string& axis) {
    input::InputManager::Get().ClearAxisBindings(axis);
}

nlohmann::json ScriptAPI::GetAxisBindings(const std::string& axis) const {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& b : input::InputManager::Get().GetAxisBindings(axis)) {
        arr.push_back(b.ToJson());
    }
    return arr;
}

bool ScriptAPI::SaveInputMap(const std::string& filepath) const {
    return input::InputManager::Get().SaveInputMap(filepath);
}

bool ScriptAPI::LoadInputMap(const std::string& filepath) {
    return input::InputManager::Get().LoadInputMap(filepath);
}

// ============================================================================
// Input capture (rebind menus)
// ============================================================================

void ScriptAPI::StartInputCapture() {
    input::InputManager::Get().StartInputCapture();
}

void ScriptAPI::StartInputCaptureMask(bool keyboard, bool mouse, bool gamepad) {
    uint32_t mask = 0;
    if (keyboard) mask |= input::InputManager::Capture_Keyboard;
    if (mouse) mask |= input::InputManager::Capture_Mouse;
    if (gamepad) mask |= input::InputManager::Capture_Gamepad;
    input::InputManager::Get().StartInputCapture(mask);
}

void ScriptAPI::CancelInputCapture() {
    input::InputManager::Get().CancelInputCapture();
}

bool ScriptAPI::IsCapturingInput() const {
    return input::InputManager::Get().IsCapturing();
}

bool ScriptAPI::IsInputCaptureComplete() const {
    return input::InputManager::Get().IsCaptureComplete();
}

nlohmann::json ScriptAPI::GetCapturedBinding() const {
    input::InputManager& mgr = input::InputManager::Get();
    if (!mgr.IsCaptureComplete()) {
        return nlohmann::json();
    }
    return mgr.GetCapturedBinding().ToJson();
}

void ScriptAPI::ClearCapturedBinding() {
    input::InputManager::Get().ClearCapturedBinding();
}

void ScriptAPI::ApplyCapturedBindingToAction(const std::string& action) {
    input::InputManager& mgr = input::InputManager::Get();
    if (mgr.IsCaptureComplete()) {
        mgr.AddBindingToAction(action, mgr.GetCapturedBinding());
        mgr.ClearCapturedBinding();
    }
}

// ============================================================================
// Glyph / prompt resolution
// ============================================================================

static nlohmann::json GlyphToJson(const input::InputGlyph& g) {
    nlohmann::json j;
    j["glyph_id"] = g.glyphId;
    j["label"] = g.label;
    j["art_path"] = g.artPath;
    j["device"] = static_cast<int>(g.device);
    j["gamepad_type"] = static_cast<int>(g.gamepadType);
    return j;
}

nlohmann::json ScriptAPI::GetActionGlyph(const std::string& action, int player,
                                         int deviceOverride) const {
    input::InputDeviceType dev = deviceOverride < 0
                                     ? input::InputDeviceType::Unknown
                                     : static_cast<input::InputDeviceType>(deviceOverride);
    return GlyphToJson(input::InputManager::Get().GetActionGlyph(action, player, dev));
}

nlohmann::json ScriptAPI::GetActionGlyphs(const std::string& action) const {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& g : input::InputManager::Get().GetActionGlyphs(action)) {
        arr.push_back(GlyphToJson(g));
    }
    return arr;
}

void ScriptAPI::SetGlyphLabel(const std::string& glyphId, const std::string& label) {
    input::InputManager::Get().SetGlyphLabel(glyphId, label);
}

void ScriptAPI::SetGlyphArt(const std::string& glyphId, const std::string& artPath) {
    input::InputManager::Get().SetGlyphArt(glyphId, artPath);
}

void ScriptAPI::ClearGlyphOverride(const std::string& glyphId) {
    input::InputManager::Get().ClearGlyphOverride(glyphId);
}

void ScriptAPI::ClearGlyphOverrides() {
    input::InputManager::Get().ClearGlyphOverrides();
}

bool ScriptAPI::LoadGlyphMap(const std::string& filepath) {
    return input::InputManager::Get().LoadGlyphMap(filepath);
}

bool ScriptAPI::SaveGlyphMap(const std::string& filepath) const {
    return input::InputManager::Get().SaveGlyphMap(filepath);
}

// ============================================================================
// Action delegation
// ============================================================================

uint64_t ScriptAPI::ConnectInputAction(const std::string& action, const std::string& method,
                                       uint32_t flags) {
    return SubscribeEvent("action:" + action, method, flags);
}

void ScriptAPI::DisconnectInputAction(const std::string& action, uint64_t connectionId) {
    UnsubscribeEvent("action:" + action, connectionId);
}

uint64_t ScriptAPI::ConnectDeviceChanged(const std::string& method, uint32_t flags) {
    return SubscribeEvent("input_device_changed", method, flags);
}

uint64_t ScriptAPI::ConnectInputCaptured(const std::string& method, uint32_t flags) {
    return SubscribeEvent("input_capture_complete", method, flags);
}

// ============================================================================
// Event-driven action matching (used inside on_input_event)
// ============================================================================

bool ScriptAPI::EventIsAction(const nlohmann::json& event, const std::string& action) const {
    if (!event.is_object() || !event.contains("type")) {
        return false;
    }
    const std::string type = event.value("type", std::string());

    input::InputBinding eb;
    if (type == "key_down" || type == "key_up") {
        eb.deviceType = input::InputDeviceType::Keyboard;
        eb.keyCode = static_cast<input::KeyCode>(event.value("key", 0));
    } else if (type == "mouse_button_down" || type == "mouse_button_up") {
        eb.deviceType = input::InputDeviceType::Mouse;
        eb.mouseButton = static_cast<input::MouseButton>(event.value("button", 255));
    } else if (type == "gamepad_button_down" || type == "gamepad_button_up") {
        eb.deviceType = input::InputDeviceType::Gamepad;
        eb.gamepadButton = static_cast<input::GamepadButton>(event.value("button", 255));
        eb.gamepadID = static_cast<uint32_t>(event.value("gamepad_id", 0));
    } else if (type == "gamepad_axis") {
        eb.deviceType = input::InputDeviceType::Gamepad;
        eb.gamepadAxis = static_cast<input::GamepadAxis>(event.value("axis", 255));
        eb.gamepadID = static_cast<uint32_t>(event.value("gamepad_id", 0));
    } else {
        return false;
    }

    // Lenient compare against the action's bindings (an "any-gamepad" binding,
    // gamepadID 0, matches any source device).
    for (const auto& b : input::InputManager::Get().GetActionBindings(action)) {
        if (b.deviceType != eb.deviceType) {
            continue;
        }
        switch (b.deviceType) {
            case input::InputDeviceType::Keyboard:
                if (b.keyCode == eb.keyCode) return true;
                break;
            case input::InputDeviceType::Mouse:
                if (b.mouseButton == eb.mouseButton) return true;
                break;
            case input::InputDeviceType::Gamepad:
                if (b.gamepadButton != input::GamepadButton::Unknown) {
                    if (b.gamepadButton == eb.gamepadButton &&
                        (b.gamepadID == 0 || b.gamepadID == eb.gamepadID)) {
                        return true;
                    }
                } else if (b.gamepadAxis != input::GamepadAxis::Unknown) {
                    if (b.gamepadAxis == eb.gamepadAxis &&
                        (b.gamepadID == 0 || b.gamepadID == eb.gamepadID)) {
                        return true;
                    }
                }
                break;
            default:
                break;
        }
    }
    return false;
}

bool ScriptAPI::EventIsActionPressed(const nlohmann::json& event, const std::string& action) const {
    if (!EventIsAction(event, action)) {
        return false;
    }
    const std::string type = event.value("type", std::string());
    if (type.size() >= 5 && type.compare(type.size() - 5, 5, "_down") == 0) {
        return true;
    }
    if (type == "gamepad_axis") {
        return std::abs(event.value("value", 0.0f)) > 0.5f;
    }
    return false;
}

bool ScriptAPI::EventIsActionReleased(const nlohmann::json& event, const std::string& action) const {
    if (!EventIsAction(event, action)) {
        return false;
    }
    const std::string type = event.value("type", std::string());
    if (type.size() >= 3 && type.compare(type.size() - 3, 3, "_up") == 0) {
        return true;
    }
    if (type == "gamepad_axis") {
        return std::abs(event.value("value", 0.0f)) <= 0.5f;
    }
    return false;
}

void ScriptAPI::QueueFree(core::Node* node) {
    if (!node) return;
    core::SignalDispatcher::Get().QueueFree(node);
}

void ScriptAPI::QueueFreeSelf() {
    QueueFree(m_Owner);
}

void ScriptAPI::QueueFreeDeferred(core::Node* node) {
    if (!node) return;
    core::SignalDispatcher::Get().QueueFreeDeferred(node);
}

void ScriptAPI::QueueFreeDeferredSelf() {
    QueueFreeDeferred(m_Owner);
}

void ScriptAPI::Free(core::Node* node) {
    if (!node) return;
    core::SignalDispatcher::Get().Free(node);
}

void ScriptAPI::FreeSelf() {
    Free(m_Owner);
}

namespace {
core::SignalArgs JsonToSignalArgs(const nlohmann::json& args) {
    core::SignalArgs out;
    if (args.is_array()) {
        for (const nlohmann::json& a : args) {
            out.push_back(a);
        }
    } else if (!args.is_null()) {
        out.push_back(args);
    }
    return out;
}
}

void ScriptAPI::EmitSignal(const std::string& signal, const nlohmann::json& args) {
    if (m_Owner) {
        m_Owner->Emit(signal, JsonToSignalArgs(args));
    }
}

uint64_t ScriptAPI::ConnectSignal(const std::string& signal, core::Node* target,
                                  const std::string& method, uint32_t flags) {
    if (!m_Owner || !target) {
        return 0;
    }
    return m_Owner->Connect(signal, target, method, flags);
}

void ScriptAPI::DisconnectSignal(const std::string& signal, uint64_t connectionId) {
    if (m_Owner) {
        m_Owner->Disconnect(signal, connectionId);
    }
}

bool ScriptAPI::IsSignalConnected(const std::string& signal) const {
    return m_Owner ? m_Owner->IsConnected(signal) : false;
}

void ScriptAPI::AddUserSignal(const std::string& name) {
    if (m_Owner) {
        m_Owner->AddUserSignal(name);
    }
}

void ScriptAPI::EmitEvent(const std::string& event, const nlohmann::json& args) {
    core::EventBus::Get().Emit(event, JsonToSignalArgs(args));
}

uint64_t ScriptAPI::SubscribeEvent(const std::string& event, const std::string& method, uint32_t flags) {
    if (!m_Owner) {
        return 0;
    }
    return core::EventBus::Get().Subscribe(event, m_Owner, method, flags);
}

void ScriptAPI::UnsubscribeEvent(const std::string& event, uint64_t subscriptionId) {
    core::EventBus::Get().Unsubscribe(event, subscriptionId);
}

void ScriptAPI::CallDeferred(const std::string& method, const nlohmann::json& args) {
    if (m_Owner) {
        core::SignalDispatcher::Get().CallDeferred(m_Owner, method, JsonToSignalArgs(args));
    }
}

// ============================================================================
// Window / Display
// ============================================================================

void ScriptAPI::SetWindowTitle(const std::string& title) {
    platform::DisplayServer::Get().SetWindowTitle(title);
}

std::string ScriptAPI::GetWindowTitle() const {
    return platform::DisplayServer::Get().GetWindowTitle();
}

void ScriptAPI::SetFullscreen(bool fullscreen) {
    platform::DisplayServer::Get().SetFullscreen(fullscreen);
}

bool ScriptAPI::IsFullscreen() const {
    return platform::DisplayServer::Get().IsFullscreen();
}

void ScriptAPI::SetVSync(bool enabled) {
    platform::DisplayServer::Get().SetVSync(enabled);
}

bool ScriptAPI::IsVSync() const {
    return platform::DisplayServer::Get().IsVSync();
}

void ScriptAPI::SetWindowSize(int width, int height) {
    platform::DisplayServer::Get().SetWindowSize(width, height);
}

math::Vec2 ScriptAPI::GetWindowSize() const {
    glm::ivec2 size = input::InputManager::Get().GetWindowSize();
    return math::Vec2(static_cast<float>(size.x), static_cast<float>(size.y));
}

math::Vec2 ScriptAPI::GetScreenSize() const {
    return platform::DisplayServer::Get().GetScreenSize();
}

void ScriptAPI::MaximizeWindow() {
    platform::DisplayServer::Get().MaximizeWindow();
}

void ScriptAPI::MinimizeWindow() {
    platform::DisplayServer::Get().MinimizeWindow();
}

void ScriptAPI::RestoreWindow() {
    platform::DisplayServer::Get().RestoreWindow();
}

void ScriptAPI::SetMouseMode(int mode) {
    if (mode < 0 || mode > static_cast<int>(platform::MouseMode::ConfinedHidden)) {
        mode = 0;
    }
    platform::DisplayServer::Get().SetMouseMode(static_cast<platform::MouseMode>(mode));
}

int ScriptAPI::GetMouseMode() const {
    return static_cast<int>(platform::DisplayServer::Get().GetMouseMode());
}

void ScriptAPI::SetMouseCursorVisible(bool visible) {
    platform::DisplayServer::Get().SetCursorVisible(visible);
    input::InputManager::Get().SetMouseCursorVisible(visible);
}

bool ScriptAPI::IsMouseCursorVisible() const {
    return platform::DisplayServer::Get().IsCursorVisible();
}

// ============================================================================
// Screen <-> World Coordinate Conversion
// ============================================================================

namespace {
// Depth-first search for the first active, visible camera of the requested type,
// mirroring RuntimeApp::findCamerasRecursive so script conversions resolve against
// the same camera the runtime renders with.
template<typename CamT>
CamT* FindActiveCamera(core::Node* node) {
    if (!node) {
        return nullptr;
    }
    if (node->IsActiveInHierarchy() && node->IsVisibleInHierarchy()) {
        if (CamT* cam = dynamic_cast<CamT*>(node)) {
            if (cam->IsActive()) {
                return cam;
            }
        }
    }
    for (const std::shared_ptr<core::Node>& child : node->GetChildren()) {
        if (CamT* found = FindActiveCamera<CamT>(child.get())) {
            return found;
        }
    }
    return nullptr;
}

// Window size and aspect ratio, or false when no usable window size is known.
bool ResolveViewport(float& outWidth, float& outHeight, float& outAspect) {
    glm::ivec2 size = input::InputManager::Get().GetWindowSize();
    if (size.x <= 0 || size.y <= 0) {
        return false;
    }
    outWidth = static_cast<float>(size.x);
    outHeight = static_cast<float>(size.y);
    outAspect = outWidth / outHeight;
    return true;
}
}

math::Vec2 ScriptAPI::ScreenToWorld2D(const math::Vec2& screenPos) const {
    core::Camera2D* cam = FindActiveCamera<core::Camera2D>(GetRoot());
    float width, height, aspect;
    if (!cam || !ResolveViewport(width, height, aspect)) {
        return screenPos;
    }

    lupine::Camera2D render;
    render.position = cam->GetEffectivePosition();
    render.rotation = cam->GetGlobalRotation();
    render.zoom = cam->GetZoom();
    render.orthoSize = cam->GetOrthoSize();

    math::Mat4 viewProj = render.getProjectionMatrix(aspect) * render.getViewMatrix();
    math::Mat4 inv = viewProj.Inverse();

    float ndcX = (screenPos.x / width) * 2.0f - 1.0f;
    float ndcY = 1.0f - (screenPos.y / height) * 2.0f;
    math::Vec4 world = inv * math::Vec4(ndcX, ndcY, 0.0f, 1.0f);
    if (!math::IsZero(world.w)) {
        world = world / world.w;
    }
    return math::Vec2(world.x, world.y);
}

math::Vec2 ScriptAPI::WorldToScreen2D(const math::Vec2& worldPos) const {
    core::Camera2D* cam = FindActiveCamera<core::Camera2D>(GetRoot());
    float width, height, aspect;
    if (!cam || !ResolveViewport(width, height, aspect)) {
        return worldPos;
    }

    lupine::Camera2D render;
    render.position = cam->GetEffectivePosition();
    render.rotation = cam->GetGlobalRotation();
    render.zoom = cam->GetZoom();
    render.orthoSize = cam->GetOrthoSize();

    math::Mat4 viewProj = render.getProjectionMatrix(aspect) * render.getViewMatrix();
    math::Vec3 screen = math::Camera::WorldToScreenPoint(
        math::Vec3(worldPos.x, worldPos.y, 0.0f), viewProj, width, height);
    return math::Vec2(screen.x, screen.y);
}

ScriptAPI::ScreenRay ScriptAPI::ScreenToWorldRay3D(const math::Vec2& screenPos) const {
    ScreenRay ray;
    ray.direction = math::Vec3(0.0f, 0.0f, -1.0f);

    core::Camera3D* cam = FindActiveCamera<core::Camera3D>(GetRoot());
    float width, height, aspect;
    if (!cam || !ResolveViewport(width, height, aspect)) {
        return ray;
    }

    lupine::Camera3D render;
    math::Vec3 pos = cam->GetGlobalPosition();
    math::Quat rot = cam->GetGlobalRotation();
    render.position = pos;
    render.target = pos + (rot * math::Vec3(0.0f, 0.0f, -1.0f));
    render.up = rot * math::Vec3(0.0f, 1.0f, 0.0f);
    render.projectionType = (cam->GetProjectionType() == core::Camera3D::ProjectionType::Perspective)
        ? lupine::ProjectionType::Perspective
        : lupine::ProjectionType::Orthographic;
    render.fov = cam->GetFOV();
    render.nearPlane = cam->GetNearPlane();
    render.farPlane = cam->GetFarPlane();
    render.orthoSize = cam->GetOrthoSize();

    math::Mat4 viewProj = render.getProjectionMatrix(aspect) * render.getViewMatrix();
    math::Mat4 inv = viewProj.Inverse();

    float ndcX = (screenPos.x / width) * 2.0f - 1.0f;
    float ndcY = 1.0f - (screenPos.y / height) * 2.0f;

    math::Vec4 nearW = inv * math::Vec4(ndcX, ndcY, -1.0f, 1.0f);
    math::Vec4 farW = inv * math::Vec4(ndcX, ndcY, 1.0f, 1.0f);
    if (!math::IsZero(nearW.w)) {
        nearW = nearW / nearW.w;
    }
    if (!math::IsZero(farW.w)) {
        farW = farW / farW.w;
    }

    ray.origin = nearW.ToVec3();
    math::Vec3 dir = farW.ToVec3() - nearW.ToVec3();
    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (len > 0.0001f) {
        ray.direction = math::Vec3(dir.x / len, dir.y / len, dir.z / len);
    }
    return ray;
}

math::Vec3 ScriptAPI::ScreenToWorld3D(const math::Vec2& screenPos, float distance) const {
    ScreenRay ray = ScreenToWorldRay3D(screenPos);
    return math::Vec3(
        ray.origin.x + ray.direction.x * distance,
        ray.origin.y + ray.direction.y * distance,
        ray.origin.z + ray.direction.z * distance);
}

math::Vec3 ScriptAPI::WorldToScreen3D(const math::Vec3& worldPos) const {
    core::Camera3D* cam = FindActiveCamera<core::Camera3D>(GetRoot());
    float width, height, aspect;
    if (!cam || !ResolveViewport(width, height, aspect)) {
        return math::Vec3(0.0f, 0.0f, 0.0f);
    }

    lupine::Camera3D render;
    math::Vec3 pos = cam->GetGlobalPosition();
    math::Quat rot = cam->GetGlobalRotation();
    render.position = pos;
    render.target = pos + (rot * math::Vec3(0.0f, 0.0f, -1.0f));
    render.up = rot * math::Vec3(0.0f, 1.0f, 0.0f);
    render.projectionType = (cam->GetProjectionType() == core::Camera3D::ProjectionType::Perspective)
        ? lupine::ProjectionType::Perspective
        : lupine::ProjectionType::Orthographic;
    render.fov = cam->GetFOV();
    render.nearPlane = cam->GetNearPlane();
    render.farPlane = cam->GetFarPlane();
    render.orthoSize = cam->GetOrthoSize();

    math::Mat4 viewProj = render.getProjectionMatrix(aspect) * render.getViewMatrix();
    return math::Camera::WorldToScreenPoint(worldPos, viewProj, width, height);
}

// ============================================================================
// Time scale
// ============================================================================

void ScriptAPI::SetTimeScale(float timeScale) {
    if (m_SceneManager) {
        m_SceneManager->SetTimeScale(timeScale);
    }
}

float ScriptAPI::GetTimeScale() const {
    return m_SceneManager ? m_SceneManager->GetTimeScale() : 1.0f;
}

// ============================================================================
// Audio control
// ============================================================================

void ScriptAPI::SetAudioSourceVolume(const std::string& sourceUUID, float volume) {
    audio::AudioManager::GetInstance().SetSourceVolume(core::UUID::FromString(sourceUUID), volume);
}

void ScriptAPI::SetAudioSourcePitch(const std::string& sourceUUID, float pitch) {
    audio::AudioManager::GetInstance().SetSourcePitch(core::UUID::FromString(sourceUUID), pitch);
}

void ScriptAPI::SetAudioSourcePan(const std::string& sourceUUID, float pan) {
    audio::AudioManager::GetInstance().SetSourcePan(core::UUID::FromString(sourceUUID), pan);
}

void ScriptAPI::SetMasterVolume(float volume) {
    audio::AudioManager::GetInstance().SetMasterVolume(volume);
}

float ScriptAPI::GetMasterVolume() const {
    return audio::AudioManager::GetInstance().GetMasterVolume();
}

void ScriptAPI::SetMasterMuted(bool muted) {
    audio::AudioManager::GetInstance().SetMasterMuted(muted);
}

bool ScriptAPI::IsMasterMuted() const {
    return audio::AudioManager::GetInstance().IsMasterMuted();
}

void ScriptAPI::SetListenerPosition(const math::Vec3& position) {
    audio::AudioManager::GetInstance().SetListenerPosition(position);
}

void ScriptAPI::SetListenerOrientation(const math::Vec3& forward, const math::Vec3& up) {
    audio::AudioManager::GetInstance().SetListenerOrientation(forward, up);
}

void ScriptAPI::SetListenerVelocity(const math::Vec3& velocity) {
    audio::AudioManager::GetInstance().SetListenerVelocity(velocity);
}

void ScriptAPI::CreateAudioBus(const std::string& name, const std::string& parentBus) {
    audio::AudioManager::GetInstance().CreateBus(name, parentBus);
}

void ScriptAPI::DestroyAudioBus(const std::string& name) {
    audio::AudioManager::GetInstance().DestroyBus(name);
}

bool ScriptAPI::HasAudioBus(const std::string& name) const {
    return audio::AudioManager::GetInstance().HasBus(name);
}

void ScriptAPI::SetBusSolo(const std::string& name, bool solo) {
    audio::AudioManager::GetInstance().SetBusSolo(name, solo);
}

bool ScriptAPI::IsBusSolo(const std::string& name) const {
    return audio::AudioManager::GetInstance().IsBusSolo(name);
}

// ============================================================================
// Engine / OS information
// ============================================================================

float ScriptAPI::GetFPS() const {
    static float s_SmoothedFps = 0.0f;
    if (m_DeltaTime > 0.0001f) {
        float instantaneous = 1.0f / m_DeltaTime;
        s_SmoothedFps = (s_SmoothedFps <= 0.0f)
            ? instantaneous
            : s_SmoothedFps * 0.9f + instantaneous * 0.1f;
    }
    return s_SmoothedFps;
}

int ScriptAPI::GetTicksMsec() const {
    return static_cast<int>(GetTime() * 1000.0f);
}

double ScriptAPI::GetUnixTime() const {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count() / 1000.0;
}

std::string ScriptAPI::GetPlatformName() const {
    return platform::Platform::GetPlatformName();
}

bool ScriptAPI::IsDebugBuild() const {
    return platform::Platform::IsDebug();
}

float ScriptAPI::GetDPIScale() const {
    return input::InputManager::Get().GetDPIScale();
}

bool ScriptAPI::OpenURL(const std::string& url) {
    return platform::DisplayServer::Get().OpenURL(url);
}

// ============================================================================
// Color helpers & data sampling
// ============================================================================

namespace {
int HexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
}

math::Color ScriptAPI::ColorFromHex(const std::string& hex) const {
    std::string s = hex;
    if (!s.empty() && s[0] == '#') {
        s = s.substr(1);
    }

    auto pair = [&](size_t i) -> float {
        int hi = HexNibble(s[i]);
        int lo = HexNibble(s[i + 1]);
        if (hi < 0 || lo < 0) return 0.0f;
        return static_cast<float>(hi * 16 + lo) / 255.0f;
    };
    auto single = [&](size_t i) -> float {
        int v = HexNibble(s[i]);
        if (v < 0) return 0.0f;
        return static_cast<float>(v * 16 + v) / 255.0f;
    };

    if (s.size() == 3) {
        return math::Color(single(0), single(1), single(2), 1.0f);
    }
    if (s.size() == 4) {
        return math::Color(single(0), single(1), single(2), single(3));
    }
    if (s.size() == 6) {
        return math::Color(pair(0), pair(2), pair(4), 1.0f);
    }
    if (s.size() == 8) {
        return math::Color(pair(0), pair(2), pair(4), pair(6));
    }
    return math::Color(0.0f, 0.0f, 0.0f, 1.0f);
}

std::string ScriptAPI::ColorToHex(const math::Color& color) const {
    auto clampByte = [](float v) -> int {
        int b = static_cast<int>(v * 255.0f + 0.5f);
        if (b < 0) return 0;
        if (b > 255) return 255;
        return b;
    };
    static const char* digits = "0123456789abcdef";
    int comps[4] = { clampByte(color.r), clampByte(color.g), clampByte(color.b), clampByte(color.a) };
    std::string out = "#";
    for (int i = 0; i < 4; ++i) {
        out.push_back(digits[(comps[i] >> 4) & 0xF]);
        out.push_back(digits[comps[i] & 0xF]);
    }
    return out;
}

math::Color ScriptAPI::ColorFromHSV(float h, float s, float v, float a) const {
    h = h - std::floor(h);
    s = std::min(std::max(s, 0.0f), 1.0f);
    v = std::min(std::max(v, 0.0f), 1.0f);

    float r = v, g = v, b = v;
    if (s > 0.0f) {
        float hSector = h * 6.0f;
        int i = static_cast<int>(std::floor(hSector)) % 6;
        if (i < 0) i += 6;
        float f = hSector - std::floor(hSector);
        float p = v * (1.0f - s);
        float q = v * (1.0f - s * f);
        float t = v * (1.0f - s * (1.0f - f));
        switch (i) {
            case 0: r = v; g = t; b = p; break;
            case 1: r = q; g = v; b = p; break;
            case 2: r = p; g = v; b = t; break;
            case 3: r = p; g = q; b = v; break;
            case 4: r = t; g = p; b = v; break;
            default: r = v; g = p; b = q; break;
        }
    }
    return math::Color(r, g, b, a);
}

math::Color ScriptAPI::ColorLerp(const math::Color& a, const math::Color& b, float t) const {
    return a.Lerp(b, t);
}

math::Color ScriptAPI::SampleGradient(const nlohmann::json& gradient, float t) const {
    std::string json = gradient.is_string() ? gradient.get<std::string>() : gradient.dump();
    math::Gradient g = math::Gradient::FromJsonString(json);
    return g.Sample(t);
}

float ScriptAPI::SampleCurve(const nlohmann::json& curve, float t, float emptyDefault) const {
    std::string json = curve.is_string() ? curve.get<std::string>() : curve.dump();
    math::Curve c = math::Curve::FromJsonString(json);
    return c.Sample(t, emptyDefault);
}

// ============================================================================
// Tree utilities
// ============================================================================

core::Node* ScriptAPI::GetFirstNodeInGroup(const std::string& group) const {
    std::vector<core::Node*> nodes = GetNodesInGroup(group);
    return nodes.empty() ? nullptr : nodes.front();
}

core::Node* ScriptAPI::GetNodeOrNull(const std::string& path) const {
    return FindNode(path);
}

namespace {
void CollectChildrenByType(core::Node* node, const std::string& typeName, bool recursive,
                           std::vector<core::Node*>& out) {
    if (!node) return;
    for (const std::shared_ptr<core::Node>& child : node->GetChildren()) {
        if (!child) continue;
        if (typeName.empty() || child->GetTypeName() == typeName) {
            out.push_back(child.get());
        }
        if (recursive) {
            CollectChildrenByType(child.get(), typeName, recursive, out);
        }
    }
}
}

std::vector<core::Node*> ScriptAPI::FindChildren(const std::string& typeName, bool recursive) const {
    std::vector<core::Node*> result;
    if (m_Owner) {
        CollectChildrenByType(m_Owner, typeName, recursive, result);
    }
    return result;
}

bool ScriptAPI::IsAncestorOf(core::Node* other) const {
    if (!m_Owner || !other) return false;
    core::Node* parent = other->GetParent();
    while (parent) {
        if (parent == m_Owner) return true;
        parent = parent->GetParent();
    }
    return false;
}

// ============================================================================
// Debug draw
// ============================================================================

void ScriptAPI::DebugDrawLine(const math::Vec3& start, const math::Vec3& end, const math::Color& color, float duration) {
    core::DebugDrawQueue::Get().Line(start, end, color, duration);
}

void ScriptAPI::DebugDrawLine2D(const math::Vec2& start, const math::Vec2& end, const math::Color& color, float duration) {
    core::DebugDrawQueue::Get().Line(math::Vec3(start.x, start.y, 0.0f), math::Vec3(end.x, end.y, 0.0f), color, duration);
}

void ScriptAPI::DebugDrawRay(const math::Vec3& origin, const math::Vec3& direction, const math::Color& color, float duration) {
    core::DebugDrawQueue::Get().Ray(origin, direction, color, duration);
}

void ScriptAPI::DebugDrawBox(const math::Vec3& center, const math::Vec3& size, const math::Color& color, float duration) {
    core::DebugDrawQueue::Get().Box(center, size, color, duration);
}

void ScriptAPI::DebugDrawSphere(const math::Vec3& center, float radius, const math::Color& color, float duration) {
    core::DebugDrawQueue::Get().Sphere(center, radius, color, duration);
}

void ScriptAPI::DebugDrawCircle(const math::Vec3& center, const math::Vec3& normal, float radius, const math::Color& color, float duration) {
    core::DebugDrawQueue::Get().Circle(center, normal, radius, color, duration);
}

void ScriptAPI::DebugDrawText(const math::Vec3& position, const std::string& text, const math::Color& color, float duration) {
    core::DebugDrawQueue::Get().Text(position, text, color, duration);
}

void ScriptAPI::DebugDrawText2D(const math::Vec2& position, const std::string& text, const math::Color& color, float duration) {
    core::DebugDrawQueue::Get().Text(math::Vec3(position.x, position.y, 0.0f), text, color, duration);
}

// ============================================================================
// Custom component rendering (on_draw) + editor-only debug drawing
// ============================================================================

namespace {

std::unordered_map<std::string, TextureHandle>& GetScriptDrawTextureCache() {
    static std::unordered_map<std::string, TextureHandle> s_cache;
    return s_cache;
}

std::mutex& GetScriptDrawTextureCacheMutex() {
    static std::mutex s_mutex;
    return s_mutex;
}

void EnsureScriptDrawCacheRegistered() {
    static bool registered = false;
    if (registered) return;
    registered = true;
    rendering::TextureCache::RegisterCache(
        "ScriptDraw",
        [](const std::string& path) -> bool {
            std::lock_guard<std::mutex> lock(GetScriptDrawTextureCacheMutex());
            auto it = GetScriptDrawTextureCache().find(path);
            if (it != GetScriptDrawTextureCache().end()) {
                GetScriptDrawTextureCache().erase(it);
                return true;
            }
            return false;
        },
        []() {
            std::lock_guard<std::mutex> lock(GetScriptDrawTextureCacheMutex());
            GetScriptDrawTextureCache().clear();
        });
}

// Resolve a res:// image path to a GPU texture handle, loading + caching on
// first use. Shared by DrawSprite / DrawTexturedQuad. Returns an invalid handle
// on failure (the draw then falls back to an untextured quad).
TextureHandle ResolveScriptDrawTexture(IGfxDevice* device, const std::string& path) {
    if (path.empty() || !device) {
        return TextureHandle();
    }
    EnsureScriptDrawCacheRegistered();
    {
        std::lock_guard<std::mutex> lock(GetScriptDrawTextureCacheMutex());
        auto it = GetScriptDrawTextureCache().find(path);
        if (it != GetScriptDrawTextureCache().end() && it->second.isValid()) {
            return it->second;
        }
    }
    asset::ImageAsset image;
    if (!image.LoadFromFile(path, true, asset::ImageColorSpace::sRGB)) {
        LOG_ERROR(LogCategory::Scripting, "Script draw: failed to load texture '{}'", path);
        return TextureHandle();
    }
    if (!image.IsLoaded() || image.GetWidth() == 0 || image.GetHeight() == 0 ||
        image.GetData() == nullptr) {
        return TextureHandle();
    }
    TextureHandle handle = CreateTexture2DFromImage(device, image, TextureFormat::RGBA8_UNORM);
    {
        std::lock_guard<std::mutex> lock(GetScriptDrawTextureCacheMutex());
        GetScriptDrawTextureCache()[path] = handle;
    }
    return handle;
}

} // namespace

void ScriptAPI::DrawQuad(const math::Vec3& position, const math::Vec2& size,
                         const math::Color& color, int blendMode) {
    if (!m_CurrentRenderContext) return;
    m_CurrentRenderContext->drawQuad(position, size, color, TextureHandle(), blendMode);
}

void ScriptAPI::DrawTexturedQuad(const math::Vec3& position, const math::Vec2& size,
                                 const math::Color& tint, const std::string& texturePath,
                                 int blendMode) {
    if (!m_CurrentRenderContext) return;
    TextureHandle tex = ResolveScriptDrawTexture(m_CurrentRenderContext->getDevice(), texturePath);
    m_CurrentRenderContext->drawQuad(position, size, tint, tex, blendMode);
}

void ScriptAPI::DrawRect(const math::Vec2& position, const math::Vec2& size,
                         const math::Color& color, bool filled, float thickness) {
    if (!m_CurrentRenderContext) return;
    if (filled) {
        // drawQuad centers on its position; convert the top-left rect to a center.
        math::Vec3 center(position.x + size.x * 0.5f, position.y + size.y * 0.5f, 0.0f);
        m_CurrentRenderContext->drawQuad(center, size, color, TextureHandle(), 0);
    } else {
        math::Vec3 tl(position.x, position.y, 0.0f);
        math::Vec3 tr(position.x + size.x, position.y, 0.0f);
        math::Vec3 br(position.x + size.x, position.y + size.y, 0.0f);
        math::Vec3 bl(position.x, position.y + size.y, 0.0f);
        m_CurrentRenderContext->drawLine(tl, tr, color, thickness);
        m_CurrentRenderContext->drawLine(tr, br, color, thickness);
        m_CurrentRenderContext->drawLine(br, bl, color, thickness);
        m_CurrentRenderContext->drawLine(bl, tl, color, thickness);
    }
}

void ScriptAPI::DrawSprite(const std::string& texturePath, const math::Vec2& position,
                           const math::Vec2& size, const math::Color& tint, float rotation,
                           int blendMode) {
    if (!m_CurrentRenderContext) return;
    SpriteDrawData sprite;
    sprite.texture = ResolveScriptDrawTexture(m_CurrentRenderContext->getDevice(), texturePath);
    sprite.position = position;
    sprite.size = size;
    sprite.tint = tint;
    sprite.rotation = rotation;
    sprite.blendMode = blendMode;
    m_CurrentRenderContext->drawSprite(sprite);
}

void ScriptAPI::DrawLine(const math::Vec3& start, const math::Vec3& end,
                         const math::Color& color, float thickness) {
    if (!m_CurrentRenderContext) return;
    m_CurrentRenderContext->drawLine(start, end, color, thickness);
}

void ScriptAPI::DrawCircle(const math::Vec3& center, float radius, const math::Color& color,
                           bool filled) {
    if (!m_CurrentRenderContext) return;
    m_CurrentRenderContext->drawCircle(center, radius, color, filled);
}

void ScriptAPI::DrawPolygon(const math::Vec2& center, float radius, int sides,
                            const math::Color& color, float rotation, int blendMode) {
    if (!m_CurrentRenderContext) return;
    m_CurrentRenderContext->drawPolygon(center, radius, sides, color, rotation, blendMode);
}

void ScriptAPI::DrawBox(const math::Vec3& center, const math::Vec3& size,
                        const math::Color& color, bool wireframe) {
    if (!m_CurrentRenderContext) return;
    m_CurrentRenderContext->drawBox(center, size, color, wireframe);
}

void ScriptAPI::DrawRoundedRect(const math::Vec2& position, const math::Vec2& size,
                                float cornerRadius, const math::Color& color, int blendMode) {
    if (!m_CurrentRenderContext) return;
    m_CurrentRenderContext->drawRoundedRect(position, size, cornerRadius, color, blendMode);
}

bool ScriptAPI::IsEditorDrawAvailable() const {
    return DebugDraw::IsAvailable();
}

void ScriptAPI::EditorDrawLine(const math::Vec3& start, const math::Vec3& end,
                               const math::Color& color) {
    DebugDraw::Line(start, end, color);
}

void ScriptAPI::EditorDrawBox(const math::Vec3& center, const math::Vec3& size,
                              const math::Color& color, bool wireframe) {
    DebugDraw::Box(center, size, color, wireframe);
}

void ScriptAPI::EditorDrawSphere(const math::Vec3& center, float radius,
                                 const math::Color& color, bool wireframe) {
    DebugDraw::Sphere(center, radius, color, wireframe);
}

void ScriptAPI::EditorDrawCircle(const math::Vec3& center, const math::Vec3& normal,
                                 float radius, const math::Color& color) {
    DebugDraw::Circle(center, normal, radius, color);
}

void ScriptAPI::EditorDrawRect2D(const math::Vec2& center, const math::Vec2& size,
                                 const math::Color& color) {
    DebugDraw::OrientedBox2D(center, size, 0.0f, color);
}

void ScriptAPI::EditorDrawText(const math::Vec3& position, const std::string& text,
                               const math::Color& color) {
    DebugDraw::Text(position, text, color);
}

void ScriptAPI::DestroyNode(core::Node* node) {
    if (!node || !node->GetParent()) return;

    auto parent = node->GetParent();
    auto scene = node->GetScene();

    if (scene) {

        for (auto& child : parent->GetChildren()) {
            if (child.get() == node) {
                parent->RemoveChild(child);
                break;
            }
        }
    }
}

void ScriptAPI::AddChild(std::shared_ptr<core::Node> child) {
    if (!m_Owner || !child) return;
    m_Owner->AddChild(child);
}

void ScriptAPI::AddSibling(std::shared_ptr<core::Node> sibling) {
    if (!m_Owner || !sibling || !m_Owner->GetParent()) return;
    m_Owner->GetParent()->AddChild(sibling);
}

void ScriptAPI::RemoveChild(core::Node* child) {
    if (!m_Owner || !child) return;

    for (auto& c : m_Owner->GetChildren()) {
        if (c.get() == child) {
            m_Owner->RemoveChild(c);
            break;
        }
    }
}

void ScriptAPI::RemoveChild(const std::string& name) {
    if (!m_Owner) return;
    m_Owner->RemoveChild(name);
}

core::Node* ScriptAPI::FindNode(const std::string& path) const {
    if (!m_Owner) return nullptr;

    // Godot-style unique name access: "%Name" resolves within the owner scope,
    // optionally followed by a relative path ("%Name/Child/Grandchild").
    if (!path.empty() && path[0] == '%') {
        size_t slash = path.find('/');
        std::string uniqueName = (slash == std::string::npos)
            ? path.substr(1)
            : path.substr(1, slash - 1);

        core::Node* resolved = m_Owner->ResolveUniqueName(uniqueName);
        if (!resolved) return nullptr;

        if (slash == std::string::npos) {
            return resolved;
        }

        std::string remainder = path.substr(slash + 1);
        if (remainder.empty()) {
            return resolved;
        }
        return resolved->FindNode(remainder).get();
    }

    auto scene = m_Owner->GetScene();
    if (!scene) return nullptr;

    auto node = scene->FindNode(path);
    return node.get();
}

core::Node* ScriptAPI::FindNodeByUUID(const std::string& uuidStr) const {
    if (!m_Owner) return nullptr;

    auto scene = m_Owner->GetScene();
    if (!scene) return nullptr;

    core::UUID uuid = core::UUID::FromString(uuidStr);
    auto node = scene->FindNodeByUUID(uuid);
    return node.get();
}

core::Node* ScriptAPI::GetSingleton(const std::string& name) const {
    if (!m_SceneManager) return nullptr;
    return m_SceneManager->GetSingletonNode(name);
}

core::Node* ScriptAPI::GetParent() const {
    if (!m_Owner) return nullptr;
    return m_Owner->GetParent();
}

core::Node* ScriptAPI::GetChild(const std::string& name) const {
    if (!m_Owner) return nullptr;
    auto child = m_Owner->GetChild(name);
    return child.get();
}

core::Node* ScriptAPI::GetChild(int index) const {
    if (!m_Owner) return nullptr;
    auto child = m_Owner->GetChild(static_cast<size_t>(index));
    return child.get();
}

int ScriptAPI::GetChildCount() const {
    if (!m_Owner) return 0;
    return static_cast<int>(m_Owner->GetChildCount());
}

std::vector<core::Node*> ScriptAPI::GetChildren() const {
    std::vector<core::Node*> result;
    if (!m_Owner) return result;

    for (auto& child : m_Owner->GetChildren()) {
        result.push_back(child.get());
    }
    return result;
}

bool ScriptAPI::HasNode(const std::string& path) const {
    return FindNode(path) != nullptr;
}

std::string ScriptAPI::GetName() const {
    if (!m_Owner) return "";
    return m_Owner->GetName();
}

void ScriptAPI::SetName(const std::string& name) {
    if (!m_Owner) return;
    m_Owner->SetName(name);
}

bool ScriptAPI::IsActive() const {
    if (!m_Owner) return false;
    return m_Owner->IsActive();
}

void ScriptAPI::SetActive(bool active) {
    if (!m_Owner) return;
    m_Owner->SetActive(active);
}

bool ScriptAPI::IsVisible() const {
    if (!m_Owner) return false;
    return m_Owner->IsVisible();
}

void ScriptAPI::SetVisible(bool visible) {
    if (!m_Owner) return;
    m_Owner->SetVisible(visible);
}

int ScriptAPI::GetSiblingIndex() const {
    if (!m_Owner) return -1;
    return m_Owner->GetIndexInParent();
}

void ScriptAPI::SetSiblingIndex(int index) {
    if (!m_Owner || index < 0) return;
    m_Owner->SetSiblingIndex(static_cast<size_t>(index));
}

void ScriptAPI::Reparent(core::Node* newParent) {
    ReparentTo(m_Owner, newParent);
}

void ScriptAPI::ReparentTo(core::Node* node, core::Node* newParent) {
    if (!node || !newParent) return;

    auto* oldParent = node->GetParent();
    if (!oldParent) return;

    // Find the shared_ptr for this node
    std::shared_ptr<core::Node> nodePtr;
    for (auto& child : oldParent->GetChildren()) {
        if (child.get() == node) {
            nodePtr = child;
            break;
        }
    }

    if (nodePtr) {
        oldParent->RemoveChild(nodePtr);
        newParent->AddChild(nodePtr);
    }
}

core::Scene* ScriptAPI::GetScene() const {
    if (!m_Owner) return nullptr;
    return m_Owner->GetScene();
}

bool ScriptAPI::IsEditor() const {
    core::Scene* scene = GetScene();
    return scene ? scene->IsInEditor() : false;
}

core::Node* ScriptAPI::GetRoot() const {
    if (!m_Owner) return nullptr;
    auto scene = m_Owner->GetScene();
    if (scene) {
        auto root = scene->GetRoot();
        return root.get();
    }
    return nullptr;
}

core::Component* ScriptAPI::GetComponent(const std::string& typeName) const {
    if (!m_Owner) return nullptr;
    auto comp = m_Owner->GetComponent(typeName);
    return comp.get();
}

std::vector<core::Component*> ScriptAPI::GetComponents(const std::string& typeName) const {
    std::vector<core::Component*> result;
    if (!m_Owner) return result;

    for (auto& comp : m_Owner->GetComponents()) {
        if (comp->GetTypeName() == typeName) {
            result.push_back(comp.get());
        }
    }

    return result;
}

core::Component* ScriptAPI::GetComponentInChildren(const std::string& typeName) const {
    if (!m_Owner) return nullptr;

    auto comp = GetComponent(typeName);
    if (comp) return comp;

    for (auto& child : m_Owner->GetChildren()) {
        auto childComp = child->GetComponent(typeName);
        if (childComp) return childComp.get();

        ScriptAPI childAPI;
        childAPI.SetOwner(child.get());
        auto found = childAPI.GetComponentInChildren(typeName);
        if (found) return found;
    }

    return nullptr;
}

core::Component* ScriptAPI::GetComponentInParent(const std::string& typeName) const {
    if (!m_Owner) return nullptr;

    auto parent = m_Owner->GetParent();
    while (parent) {
        auto comp = parent->GetComponent(typeName);
        if (comp) return comp.get();
        parent = parent->GetParent();
    }

    return nullptr;
}

void ScriptAPI::AddComponent(std::shared_ptr<core::Component> component) {
    if (!m_Owner || !component) return;
    m_Owner->AddComponent(component);
}

void ScriptAPI::RemoveComponent(core::Component* component) {
    if (!m_Owner || !component) return;

    for (auto& comp : m_Owner->GetComponents()) {
        if (comp.get() == component) {
            m_Owner->RemoveComponent(comp);
            break;
        }
    }
}

bool ScriptAPI::HasComponent(const std::string& typeName) const {
    return GetComponent(typeName) != nullptr;
}

core::Component* ScriptAPI::AddComponentByType(const std::string& typeName) {
    if (!m_Owner) return nullptr;

    auto comp = std::dynamic_pointer_cast<core::Component>(
        core::TypeRegistry::GetInstance().CreateInstance(typeName));

    if (comp) {
        m_Owner->AddComponent(comp);
        return comp.get();
    }
    return nullptr;
}

std::string ScriptAPI::GetComponentPropertyString(core::Component* component, const std::string& propName) const {
    if (!component) return "";

    const auto* prop = component->GetPropertyRegistry().GetProperty(propName);
    if (prop && prop->GetType() == core::PropertyValueType::String) {
        return prop->GetValue<std::string>();
    }
    return "";
}

int ScriptAPI::GetComponentPropertyInt(core::Component* component, const std::string& propName) const {
    if (!component) return 0;

    const auto* prop = component->GetPropertyRegistry().GetProperty(propName);
    if (prop && prop->GetType() == core::PropertyValueType::Int) {
        return prop->GetValue<int>();
    }
    return 0;
}

float ScriptAPI::GetComponentPropertyFloat(core::Component* component, const std::string& propName) const {
    if (!component) return 0.0f;

    const auto* prop = component->GetPropertyRegistry().GetProperty(propName);
    if (prop && prop->GetType() == core::PropertyValueType::Float) {
        return prop->GetValue<float>();
    }
    return 0.0f;
}

bool ScriptAPI::GetComponentPropertyBool(core::Component* component, const std::string& propName) const {
    if (!component) return false;

    const auto* prop = component->GetPropertyRegistry().GetProperty(propName);
    if (prop && prop->GetType() == core::PropertyValueType::Bool) {
        return prop->GetValue<bool>();
    }
    return false;
}

void ScriptAPI::SetComponentPropertyString(core::Component* component, const std::string& propName, const std::string& value) {
    if (!component) return;

    auto* prop = component->GetPropertyRegistry().GetProperty(propName);
    if (prop && prop->GetType() == core::PropertyValueType::String) {
        prop->SetValue(value);
    }
}

void ScriptAPI::SetComponentPropertyInt(core::Component* component, const std::string& propName, int value) {
    if (!component) return;

    auto* prop = component->GetPropertyRegistry().GetProperty(propName);
    if (prop && prop->GetType() == core::PropertyValueType::Int) {
        prop->SetValue(value);
    }
}

void ScriptAPI::SetComponentPropertyFloat(core::Component* component, const std::string& propName, float value) {
    if (!component) return;

    auto* prop = component->GetPropertyRegistry().GetProperty(propName);
    if (prop && prop->GetType() == core::PropertyValueType::Float) {
        prop->SetValue(value);
    }
}

void ScriptAPI::SetComponentPropertyBool(core::Component* component, const std::string& propName, bool value) {
    if (!component) return;

    auto* prop = component->GetPropertyRegistry().GetProperty(propName);
    if (prop && prop->GetType() == core::PropertyValueType::Bool) {
        prop->SetValue(value);
    }
}

namespace {

// Recursively strip UUIDs from serialized node JSON so a cloned subtree receives
// fresh identifiers on deserialize. Mirrors core::Prefab / SceneInstance cloning.
void StripNodeUUIDs(nlohmann::json& json) {
    if (json.contains("uuid")) {
        json.erase("uuid");
    }
    if (json.contains("components") && json["components"].is_array()) {
        for (nlohmann::json& componentJson : json["components"]) {
            if (componentJson.contains("uuid")) {
                componentJson.erase("uuid");
            }
        }
    }
    if (json.contains("children") && json["children"].is_array()) {
        for (nlohmann::json& childJson : json["children"]) {
            StripNodeUUIDs(childJson);
        }
    }
}

} // namespace

core::Node* ScriptAPI::ResolveSpawnParent(core::Node* parent) const {
    if (parent) {
        return parent;
    }
    if (m_Owner && m_Owner->GetScene()) {
        auto root = m_Owner->GetScene()->GetRoot();
        if (root) {
            return root.get();
        }
    }
    if (m_SceneManager) {
        auto* scene = m_SceneManager->GetCurrentScene();
        if (scene && scene->GetRoot()) {
            return scene->GetRoot().get();
        }
    }
    return nullptr;
}

core::Node* ScriptAPI::InstantiatePrefab(const std::string& prefabPath) {
    return InstantiatePrefab(prefabPath, nullptr);
}

core::Node* ScriptAPI::InstantiatePrefab(const std::string& prefabPath, core::Node* parent) {
    std::string resolvedPath = asset::Asset::ResolveAssetPath(prefabPath);
    if (resolvedPath.empty()) {
        resolvedPath = prefabPath;
    }

    core::Prefab prefab;
    if (!prefab.Load(resolvedPath)) {
        LOG_ERROR(LogCategory::Core, "InstantiatePrefab: failed to load prefab '{}'", prefabPath);
        return nullptr;
    }

    std::shared_ptr<core::Node> instance = prefab.Instantiate();
    if (!instance) {
        LOG_ERROR(LogCategory::Core, "InstantiatePrefab: prefab '{}' produced no instance", prefabPath);
        return nullptr;
    }

    core::Node* attachParent = ResolveSpawnParent(parent);
    if (!attachParent) {
        LOG_ERROR(LogCategory::Core, "InstantiatePrefab: no parent or scene root to attach '{}'", prefabPath);
        return nullptr;
    }

    core::SceneManager* sceneManager = m_SceneManager ? m_SceneManager : core::SceneManager::GetInstance();
    if (sceneManager) {
        sceneManager->PrepareRuntimeSubtree(instance);
    }

    attachParent->AddChild(instance);
    return instance.get();
}

core::Node* ScriptAPI::InstantiateScene(const std::string& scenePath, core::Node* parent) {
    core::Node* attachParent = ResolveSpawnParent(parent);
    if (!attachParent) {
        LOG_ERROR(LogCategory::Core, "InstantiateScene: no parent or scene root to attach '{}'", scenePath);
        return nullptr;
    }

    std::string resolvedPath = asset::Asset::ResolveAssetPath(scenePath);
    if (resolvedPath.empty()) {
        resolvedPath = scenePath;
    }

    auto sceneInstance = std::make_shared<core::SceneInstance>();
    sceneInstance->RegisterProperties();
    attachParent->AddChild(sceneInstance);

    if (!sceneInstance->SetSceneReference(resolvedPath)) {
        LOG_ERROR(LogCategory::Core, "InstantiateScene: failed to load scene '{}'", scenePath);
        attachParent->RemoveChild(sceneInstance);
        return nullptr;
    }

    // The instanced inner tree is built by SetSceneReference (above), so wire its
    // script components to the manager and seed globals now that it exists.
    core::SceneManager* sceneManager = m_SceneManager ? m_SceneManager : core::SceneManager::GetInstance();
    if (sceneManager) {
        sceneManager->PrepareRuntimeSubtree(sceneInstance);
    }

    return sceneInstance.get();
}

core::Node* ScriptAPI::DuplicateNode(core::Node* node) {
    if (!node) return nullptr;

    nlohmann::json data = node->Serialize();
    StripNodeUUIDs(data);

    if (!data.contains("type")) return nullptr;
    std::string typeName = data["type"].get<std::string>();

    auto clone = std::dynamic_pointer_cast<core::Node>(
        core::TypeRegistry::GetInstance().CreateInstance(typeName));
    if (!clone) return nullptr;

    clone->Deserialize(data);

    core::Node* parent = node->GetParent();
    if (!parent) {
        parent = ResolveSpawnParent(nullptr);
    }
    if (!parent) {
        LOG_ERROR(LogCategory::Core, "DuplicateNode: no parent or scene root to attach clone");
        return nullptr;
    }

    core::SceneManager* sceneManager = m_SceneManager ? m_SceneManager : core::SceneManager::GetInstance();
    if (sceneManager) {
        sceneManager->PrepareRuntimeSubtree(clone);
    }

    parent->AddChild(clone);
    return clone.get();
}

core::Node* ScriptAPI::DuplicateSelf() {
    return DuplicateNode(m_Owner);
}

core::Node* ScriptAPI::CreateNode(const std::string& typeName) {
    return CreateNode(typeName, typeName);
}

core::Node* ScriptAPI::CreateNode(const std::string& typeName, const std::string& name) {
    return CreateNodeChild(typeName, name, nullptr);
}

core::Node* ScriptAPI::CreateNodeChild(const std::string& typeName, const std::string& name, core::Node* parent) {
    auto node = std::dynamic_pointer_cast<core::Node>(
        core::TypeRegistry::GetInstance().CreateInstance(typeName));

    if (!node) {
        LOG_ERROR(LogCategory::Core, "CreateNode: unknown node type '{}'", typeName);
        return nullptr;
    }

    node->SetName(name);
    node->RegisterProperties();

    core::Node* attachParent = ResolveSpawnParent(parent);
    if (!attachParent) {
        LOG_ERROR(LogCategory::Core, "CreateNode: no parent or scene root to attach '{}'", name);
        return nullptr;
    }

    core::SceneManager* sceneManager = m_SceneManager ? m_SceneManager : core::SceneManager::GetInstance();
    if (sceneManager) {
        sceneManager->PrepareRuntimeSubtree(node);
    }

    attachParent->AddChild(node);
    return node.get();
}

void ScriptAPI::ChangeScene(const std::string& scenePath) {
    if (!m_SceneManager) {

        return;
    }
    // Defer the actual swap to the host loop. Switching synchronously here would
    // unload the scene that owns this very script while it is still executing.
    m_SceneManager->RequestSceneChange(scenePath);
}

void ScriptAPI::AddScene(const std::string& scenePath) {
    if (!m_SceneManager) {

        return;
    }
    m_SceneManager->AddAutoloadScene(scenePath);
}

void ScriptAPI::RemoveScene(const std::string& sceneName) {
    if (!m_SceneManager) {

        return;
    }
    m_SceneManager->RemoveAutoloadScene(sceneName);
}

core::Scene* ScriptAPI::GetCurrentScene() const {
    if (m_SceneManager) {
        return m_SceneManager->GetCurrentScene();
    }

    if (!m_Owner) return nullptr;
    return m_Owner->GetScene();
}

void ScriptAPI::ReloadScene() {
    if (!m_SceneManager) return;
    std::string currentPath = GetCurrentScenePath();
    if (!currentPath.empty()) {
        m_SceneManager->SwitchScene(currentPath);
    }
}

std::string ScriptAPI::GetCurrentScenePath() const {
    if (m_SceneManager) {
        auto* scene = m_SceneManager->GetCurrentScene();
        if (scene) {
            return scene->GetFilePath();
        }
    }
    return "";
}

bool ScriptAPI::LoadImageAsset(const std::string& assetPath) {

    if (m_ImageCache.find(assetPath) != m_ImageCache.end()) {
        return true;
    }

    asset::AssetRef<asset::ImageAsset> image(new asset::ImageAsset());
    bool loaded = image->LoadFromFile(assetPath, true, asset::ImageColorSpace::sRGB);

    if (loaded) {
        m_ImageCache[assetPath] = image;

        return true;
    } else {

        return false;
    }
}

bool ScriptAPI::LoadAudioAsset(const std::string& assetPath, const std::string& loadMode) {

    if (m_AudioCache.find(assetPath) != m_AudioCache.end()) {
        return true;
    }

    asset::AudioLoadMode mode = asset::AudioLoadMode::Preload;
    if (loadMode == "stream" || loadMode == "streaming") {
        mode = asset::AudioLoadMode::Streaming;
    }

    asset::AssetRef<asset::AudioAsset> audio(new asset::AudioAsset());
    bool loaded = audio->LoadFromFile(assetPath, mode);

    if (loaded) {
        m_AudioCache[assetPath] = audio;

        return true;
    } else {

        return false;
    }
}

bool ScriptAPI::LoadModelAsset(const std::string& assetPath) {

    if (m_ModelCache.find(assetPath) != m_ModelCache.end()) {
        return true;
    }

    asset::AssetRef<asset::ModelAsset> model(new asset::ModelAsset());
    bool loaded = model->LoadFromFile(assetPath, true);

    if (loaded) {
        m_ModelCache[assetPath] = model;

        return true;
    } else {

        return false;
    }
}

std::atomic<uint64_t> ScriptAPI::s_ArchetypeCacheGeneration{0};

void ScriptAPI::InvalidateArchetypeCaches() {
    s_ArchetypeCacheGeneration.fetch_add(1, std::memory_order_relaxed);
}

void ScriptAPI::SyncArchetypeCacheGeneration() {
    const uint64_t current = s_ArchetypeCacheGeneration.load(std::memory_order_relaxed);
    if (m_ArchetypeCacheGeneration != current) {
        m_ArchetypeCache.clear();
        m_ArchetypeCacheGeneration = current;
    }
}

asset::ArchetypeInstance* ScriptAPI::LoadArchetype(const std::string& assetPath) {
    SyncArchetypeCacheGeneration();
    std::unordered_map<std::string, asset::AssetRef<asset::ArchetypeInstance>>::iterator it =
        m_ArchetypeCache.find(assetPath);
    if (it != m_ArchetypeCache.end()) {
        return it->second.Get();
    }

    asset::AssetRef<asset::ArchetypeInstance> instance(new asset::ArchetypeInstance());
    if (!instance->LoadFromFile(assetPath)) {
        return nullptr;
    }

    m_ArchetypeCache[assetPath] = instance;
    return instance.Get();
}

uint64_t ScriptAPI::LoadArchetypeAsync(const std::string& assetPath, const std::string& callbackMethod,
                                       int priority) {
    asset::AsyncAssetLoader::Handle handle =
        asset::AsyncAssetLoader::GetInstance().LoadArchetypeInstanceAsync(assetPath, nullptr, priority);
    if (handle == 0) {
        return 0;
    }

    AsyncArchetypeRequest request;
    request.path = assetPath;
    request.callbackMethod = callbackMethod;
    request.isDefinition = false;
    m_AsyncArchetypeRequests[handle] = request;
    return handle;
}

uint64_t ScriptAPI::LoadArchetypeDefinitionAsync(const std::string& filePath, const std::string& callbackMethod,
                                                 int priority) {
    asset::AsyncAssetLoader::Handle handle =
        asset::AsyncAssetLoader::GetInstance().LoadArchetypeDefinitionAsync(filePath, nullptr, priority);
    if (handle == 0) {
        return 0;
    }

    AsyncArchetypeRequest request;
    request.path = filePath;
    request.callbackMethod = callbackMethod;
    request.isDefinition = true;
    m_AsyncArchetypeRequests[handle] = request;
    return handle;
}

void ScriptAPI::SetAsyncLoadPriority(uint64_t handle, int priority) {
    asset::AsyncAssetLoader::GetInstance().SetPriority(handle, priority);
}

int ScriptAPI::GetAsyncLoadPriority(uint64_t handle) const {
    return asset::AsyncAssetLoader::GetInstance().GetPriority(handle);
}

void ScriptAPI::SetAsyncStreamingBudget(int maxConcurrent) {
    size_t budget = maxConcurrent > 0 ? static_cast<size_t>(maxConcurrent) : 0;
    asset::AsyncAssetLoader::GetInstance().SetMaxConcurrentLoads(budget);
}

int ScriptAPI::GetAsyncStreamingBudget() const {
    return static_cast<int>(asset::AsyncAssetLoader::GetInstance().GetMaxConcurrentLoads());
}

int ScriptAPI::GetAsyncInFlightCount() const {
    return static_cast<int>(asset::AsyncAssetLoader::GetInstance().GetInFlightCount());
}

int ScriptAPI::GetAsyncQueuedCount() const {
    return static_cast<int>(asset::AsyncAssetLoader::GetInstance().GetQueuedCount());
}

int ScriptAPI::GetAsyncLoadStatus(uint64_t handle) const {
    switch (asset::AsyncAssetLoader::GetInstance().GetStatus(handle)) {
        case asset::AsyncLoadStatus::Pending:   return 0;
        case asset::AsyncLoadStatus::Loaded:    return 1;
        case asset::AsyncLoadStatus::Failed:    return 2;
        case asset::AsyncLoadStatus::Cancelled: return 3;
        case asset::AsyncLoadStatus::Unknown:   return 4;
    }
    return 4;
}

bool ScriptAPI::IsAsyncLoadComplete(uint64_t handle) const {
    return asset::AsyncAssetLoader::GetInstance().IsComplete(handle);
}

asset::ArchetypeInstance* ScriptAPI::GetAsyncArchetype(uint64_t handle) {
    asset::AsyncAssetLoader& loader = asset::AsyncAssetLoader::GetInstance();
    asset::AssetRef<asset::ArchetypeInstance> instance = loader.GetArchetypeInstance(handle);
    if (!instance) {
        return nullptr;
    }

    SyncArchetypeCacheGeneration();

    // Promote the loaded instance into the synchronous cache so a later
    // load_archetype(path) hits, and so it outlives the forgotten loader record.
    std::string resolvedPath = instance->GetPath();
    if (!resolvedPath.empty()) {
        m_ArchetypeCache[resolvedPath] = instance;
    }
    std::unordered_map<uint64_t, AsyncArchetypeRequest>::iterator reqIt =
        m_AsyncArchetypeRequests.find(handle);
    if (reqIt != m_AsyncArchetypeRequests.end() && !reqIt->second.path.empty()) {
        m_ArchetypeCache[reqIt->second.path] = instance;
    }

    asset::ArchetypeInstance* pointer = instance.Get();

    // The script has taken delivery; release the loader's record and our tracking.
    loader.Forget(handle);
    if (reqIt != m_AsyncArchetypeRequests.end()) {
        m_AsyncArchetypeRequests.erase(reqIt);
    }
    return pointer;
}

void ScriptAPI::CancelAsyncLoad(uint64_t handle) {
    asset::AsyncAssetLoader& loader = asset::AsyncAssetLoader::GetInstance();
    loader.Cancel(handle);
    loader.Forget(handle);
    m_AsyncArchetypeRequests.erase(handle);
}

void ScriptAPI::DispatchCompletedAsyncLoads() {
    if (m_AsyncArchetypeRequests.empty()) {
        return;
    }

    asset::AsyncAssetLoader& loader = asset::AsyncAssetLoader::GetInstance();
    std::vector<uint64_t> completed;

    for (std::unordered_map<uint64_t, AsyncArchetypeRequest>::iterator it = m_AsyncArchetypeRequests.begin();
         it != m_AsyncArchetypeRequests.end(); ++it) {
        const uint64_t handle = it->first;
        AsyncArchetypeRequest& request = it->second;

        // Only callback-style requests are dispatched here; poll/await requests
        // (empty callbackMethod) are retrieved by the script via GetAsyncArchetype.
        if (request.callbackMethod.empty()) {
            continue;
        }

        asset::AsyncLoadStatus status = loader.GetStatus(handle);
        if (status == asset::AsyncLoadStatus::Pending) {
            continue;
        }

        if (m_Environment) {
            nlohmann::json args = nlohmann::json::array();
            args.push_back(handle);

            if (!request.isDefinition) {
                if (status == asset::AsyncLoadStatus::Loaded) {
                    asset::AssetRef<asset::ArchetypeInstance> instance =
                        loader.GetArchetypeInstance(handle);
                    if (instance) {
                        std::string resolvedPath = instance->GetPath();
                        if (!resolvedPath.empty()) {
                            m_ArchetypeCache[resolvedPath] = instance;
                        }
                        if (!request.path.empty()) {
                            m_ArchetypeCache[request.path] = instance;
                        }
                        args.push_back(instance->GetResolvedFields());
                    } else {
                        args.push_back(nlohmann::json(nullptr));
                    }
                } else {
                    args.push_back(nlohmann::json(nullptr));
                }
            } else {
                args.push_back(status == asset::AsyncLoadStatus::Loaded);
                args.push_back(loader.GetClassName(handle));
            }

            m_Environment->CallFunctionArgs(request.callbackMethod, args);
        }

        loader.Forget(handle);
        completed.push_back(handle);
    }

    for (uint64_t handle : completed) {
        m_AsyncArchetypeRequests.erase(handle);
    }
}

void ScriptAPI::PreloadAssets(const std::vector<std::string>& assetPaths) {
    for (const auto& path : assetPaths) {

        std::string ext = path.substr(path.find_last_of('.'));
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" ||
            ext == ".tga" || ext == ".hdr") {
            LoadImageAsset(path);
        } else if (ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac") {
            LoadAudioAsset(path, "preload");
        } else if (ext == ".gltf" || ext == ".glb" || ext == ".fbx" || ext == ".obj") {
            LoadModelAsset(path);
        } else {

        }
    }
}

std::string ScriptAPI::PlayAudio(const std::string& audioPath, const std::string& busName,
                                 bool loop, float volume) {

    if (m_AudioCache.find(audioPath) == m_AudioCache.end()) {
        if (!LoadAudioAsset(audioPath, "preload")) {

            return "";
        }
    }

    auto& audioAsset = m_AudioCache[audioPath];

    audio::PlaybackMode mode = loop ? audio::PlaybackMode::Loop : audio::PlaybackMode::OneShot;
    core::UUID sourceUUID = audio::AudioManager::GetInstance().Play(audioAsset, busName, mode, volume);

    return sourceUUID.ToString();
}

std::string ScriptAPI::PlayAudio3D(const std::string& audioPath, const math::Vec3& position,
                                   const std::string& busName, bool loop, float volume) {

    if (m_AudioCache.find(audioPath) == m_AudioCache.end()) {
        if (!LoadAudioAsset(audioPath, "preload")) {

            return "";
        }
    }

    auto& audioAsset = m_AudioCache[audioPath];

    audio::PlaybackMode mode = loop ? audio::PlaybackMode::Loop : audio::PlaybackMode::OneShot;
    core::UUID sourceUUID = audio::AudioManager::GetInstance().Play3D(audioAsset, position, busName, mode, volume);

    return sourceUUID.ToString();
}

std::string ScriptAPI::PlayAudioScheduled(const std::string& audioPath, float delaySeconds,
                                          const std::string& busName, bool loop, float volume) {
    if (m_AudioCache.find(audioPath) == m_AudioCache.end()) {
        if (!LoadAudioAsset(audioPath, "preload")) {
            return "";
        }
    }

    auto& audioAsset = m_AudioCache[audioPath];

    audio::PlaybackMode mode = loop ? audio::PlaybackMode::Loop : audio::PlaybackMode::OneShot;
    core::UUID sourceUUID = audio::AudioManager::GetInstance().Play(
        audioAsset, busName, mode, volume, static_cast<double>(delaySeconds));

    return sourceUUID.ToString();
}

std::string ScriptAPI::PlayAudioScheduled3D(const std::string& audioPath, const math::Vec3& position,
                                            float delaySeconds, const std::string& busName, bool loop, float volume) {
    if (m_AudioCache.find(audioPath) == m_AudioCache.end()) {
        if (!LoadAudioAsset(audioPath, "preload")) {
            return "";
        }
    }

    auto& audioAsset = m_AudioCache[audioPath];

    audio::PlaybackMode mode = loop ? audio::PlaybackMode::Loop : audio::PlaybackMode::OneShot;
    core::UUID sourceUUID = audio::AudioManager::GetInstance().Play3D(
        audioAsset, position, busName, mode, volume, static_cast<double>(delaySeconds));

    return sourceUUID.ToString();
}

void ScriptAPI::StopAudio(const std::string& sourceUUID) {
    core::UUID uuid = core::UUID::FromString(sourceUUID);
    audio::AudioManager::GetInstance().Stop(uuid);
}

void ScriptAPI::PauseAudio(const std::string& sourceUUID) {
    core::UUID uuid = core::UUID::FromString(sourceUUID);
    audio::AudioManager::GetInstance().Pause(uuid);
}

void ScriptAPI::ResumeAudio(const std::string& sourceUUID) {
    core::UUID uuid = core::UUID::FromString(sourceUUID);
    audio::AudioManager::GetInstance().Resume(uuid);
}

bool ScriptAPI::IsAudioPlaying(const std::string& sourceUUID) const {
    core::UUID uuid = core::UUID::FromString(sourceUUID);
    return audio::AudioManager::GetInstance().IsSourcePlaying(uuid);
}

bool ScriptAPI::IsAudioFinished(const std::string& sourceUUID) const {
    core::UUID uuid = core::UUID::FromString(sourceUUID);
    return audio::AudioManager::GetInstance().IsSourceFinished(uuid);
}

void ScriptAPI::SetBusVolume(const std::string& busName, float volume) {
    audio::AudioManager::GetInstance().SetBusVolume(busName, volume);
}

float ScriptAPI::GetBusVolume(const std::string& busName) const {
    return audio::AudioManager::GetInstance().GetBusVolume(busName);
}

void ScriptAPI::SetBusMuted(const std::string& busName, bool muted) {
    audio::AudioManager::GetInstance().SetBusMuted(busName, muted);
}

bool ScriptAPI::IsBusMuted(const std::string& busName) const {
    return audio::AudioManager::GetInstance().IsBusMuted(busName);
}

int ScriptAPI::AddBusEffect(const std::string& busName, const std::string& effectType) {
    audio::AudioEffectType type;
    if (!audio::AudioEffectTypeFromString(effectType, type)) {
        return -1;
    }
    return audio::AudioManager::GetInstance().AddBusEffect(busName, type);
}

void ScriptAPI::RemoveBusEffect(const std::string& busName, int index) {
    audio::AudioManager::GetInstance().RemoveBusEffect(busName, index);
}

void ScriptAPI::MoveBusEffect(const std::string& busName, int fromIndex, int toIndex) {
    audio::AudioManager::GetInstance().MoveBusEffect(busName, fromIndex, toIndex);
}

void ScriptAPI::ClearBusEffects(const std::string& busName) {
    audio::AudioManager::GetInstance().ClearBusEffects(busName);
}

void ScriptAPI::SetBusEffectEnabled(const std::string& busName, int index, bool enabled) {
    audio::AudioManager::GetInstance().SetBusEffectEnabled(busName, index, enabled);
}

void ScriptAPI::SetBusEffectParameter(const std::string& busName, int index,
                                      const std::string& parameter, float value) {
    audio::AudioManager::GetInstance().SetBusEffectParameter(busName, index, parameter, value);
}

int ScriptAPI::GetBusEffectCount(const std::string& busName) const {
    return audio::AudioManager::GetInstance().GetBusEffectCount(busName);
}

float ScriptAPI::GetBusEffectParameter(const std::string& busName, int index,
                                       const std::string& parameter) const {
    return audio::AudioManager::GetInstance().GetBusEffectParameter(busName, index, parameter);
}

bool ScriptAPI::IsBusEffectEnabled(const std::string& busName, int index) const {
    return audio::AudioManager::GetInstance().IsBusEffectEnabled(busName, index);
}

float ScriptAPI::GetBusLevel(const std::string& busName) const {
    return audio::AudioManager::GetInstance().GetBusPeak(busName);
}

// ========================================================================
// Localization
// ========================================================================

std::string ScriptAPI::Tr(const std::string& key) const {
    return localization::LocalizationManager::GetInstance().Translate(key);
}

std::string ScriptAPI::TrInTable(const std::string& key, const std::string& table) const {
    return localization::LocalizationManager::GetInstance().Translate(key, table);
}

std::string ScriptAPI::TrFormat(const std::string& key,
                                const std::unordered_map<std::string, std::string>& args,
                                const std::string& table) const {
    return localization::LocalizationManager::GetInstance().TranslateFormat(key, args, table);
}

std::string ScriptAPI::TrPlural(const std::string& key, long count,
                                const std::unordered_map<std::string, std::string>& args,
                                const std::string& table) const {
    return localization::LocalizationManager::GetInstance().TranslatePlural(key, count, args, table);
}

void ScriptAPI::SetLocale(const std::string& locale) {
    localization::LocalizationManager::GetInstance().SetLocale(locale);
}

std::string ScriptAPI::GetLocale() const {
    return localization::LocalizationManager::GetInstance().GetLocale();
}

std::string ScriptAPI::GetFallbackLocale() const {
    return localization::LocalizationManager::GetInstance().GetFallbackLocale();
}

std::vector<std::string> ScriptAPI::GetAvailableLocales() const {
    return localization::LocalizationManager::GetInstance().GetAvailableLocales();
}

std::vector<std::string> ScriptAPI::GetCommandLineArgs() const {
    return core::GetCommandLineArgs();
}

bool ScriptAPI::HasLocaleKey(const std::string& key) const {
    return localization::LocalizationManager::GetInstance().HasKey(key);
}

void ScriptAPI::ReloadLocalization() {
    localization::LocalizationManager::GetInstance().ReloadTables();
}

void ScriptAPI::SetPseudolocalization(bool enabled) {
    localization::LocalizationManager::GetInstance().SetPseudolocalizationEnabled(enabled);
}

bool ScriptAPI::IsPseudolocalization() const {
    return localization::LocalizationManager::GetInstance().IsPseudolocalizationEnabled();
}

// ========================================================================
// UI Theme
// ========================================================================

bool ScriptAPI::SetActiveTheme(const std::string& resPath) {
    return ui::ThemeManager::GetInstance().SetActiveTheme(resPath);
}

math::Color ScriptAPI::GetThemeColor(const std::string& type, const std::string& entry) const {
    return ui::ThemeManager::GetInstance().GetColor(type, "", entry, math::Color::White());
}

float ScriptAPI::GetThemeConstant(const std::string& type, const std::string& entry) const {
    return ui::ThemeManager::GetInstance().GetConstant(type, "", entry, 0.0f);
}

bool ScriptAPI::SetThemePaletteColor(const std::string& key, const math::Color& color) {
    return ui::ThemeManager::GetInstance().SetPaletteColor(key, color);
}

bool ScriptAPI::SetThemeVariable(const std::string& key, float value) {
    return ui::ThemeManager::GetInstance().SetVariable(key, value);
}

uint64_t ScriptAPI::GetThemeVersion() const {
    return ui::ThemeManager::GetInstance().Version();
}

// ========================================================================
// Groups
// ========================================================================

void ScriptAPI::AddToGroup(const std::string& group) {
    AddNodeToGroup(m_Owner, group);
}

void ScriptAPI::RemoveFromGroup(const std::string& group) {
    RemoveNodeFromGroup(m_Owner, group);
}

bool ScriptAPI::IsInGroup(const std::string& group) const {
    return IsNodeInGroup(m_Owner, group);
}

std::vector<std::string> ScriptAPI::GetGroups() const {
    return GetNodeGroups(m_Owner);
}

void ScriptAPI::AddNodeToGroup(core::Node* node, const std::string& group) {
    if (node) {
        node->AddToGroup(group);
    }
}

void ScriptAPI::RemoveNodeFromGroup(core::Node* node, const std::string& group) {
    if (node) {
        node->RemoveFromGroup(group);
    }
}

bool ScriptAPI::IsNodeInGroup(core::Node* node, const std::string& group) const {
    return node ? node->IsInGroup(group) : false;
}

std::vector<std::string> ScriptAPI::GetNodeGroups(core::Node* node) const {
    return node ? node->GetGroups() : std::vector<std::string>();
}

std::vector<core::Node*> ScriptAPI::GetNodesInGroup(const std::string& group) const {
    core::Scene* scene = GetScene();
    if (!scene) {
        scene = GetCurrentScene();
    }
    if (!scene) {
        return std::vector<core::Node*>();
    }
    return scene->GetNodesInGroup(group);
}

int ScriptAPI::GetNodeCountInGroup(const std::string& group) const {
    return static_cast<int>(GetNodesInGroup(group).size());
}

// ----------------------------------------------------------------------------
// Interfaces
// ----------------------------------------------------------------------------

bool ScriptAPI::ImplementsInterface(const std::string& interfaceName) const {
    return NodeImplementsInterface(m_Owner, interfaceName);
}

std::vector<std::string> ScriptAPI::GetImplementedInterfaces() const {
    return GetNodeInterfaces(m_Owner);
}

nlohmann::json ScriptAPI::VerifyInterface(const std::string& interfaceName) const {
    return VerifyNodeInterface(m_Owner, interfaceName);
}

bool ScriptAPI::NodeImplementsInterface(core::Node* node, const std::string& interfaceName) const {
    return node ? node->ImplementsInterface(interfaceName) : false;
}

std::vector<std::string> ScriptAPI::GetNodeInterfaces(core::Node* node) const {
    return node ? node->GetImplementedInterfaces() : std::vector<std::string>();
}

nlohmann::json ScriptAPI::VerifyNodeInterface(core::Node* node, const std::string& interfaceName) const {
    if (!node) {
        nlohmann::json result;
        result["interface"] = interfaceName;
        result["exists"] = core::InterfaceRegistry::GetInstance().IsInterface(interfaceName);
        result["conforms"] = false;
        result["missing_methods"] = nlohmann::json::array();
        result["missing_signals"] = nlohmann::json::array();
        return result;
    }
    return node->VerifyInterface(interfaceName);
}

std::vector<core::Node*> ScriptAPI::GetNodesImplementingInterface(const std::string& interfaceName) const {
    core::Scene* scene = GetScene();
    if (!scene) {
        scene = GetCurrentScene();
    }
    if (!scene) {
        return std::vector<core::Node*>();
    }
    return scene->GetNodesImplementingInterface(interfaceName);
}

int ScriptAPI::GetNodeCountImplementingInterface(const std::string& interfaceName) const {
    return static_cast<int>(GetNodesImplementingInterface(interfaceName).size());
}

core::Node* ScriptAPI::GetFirstNodeImplementingInterface(const std::string& interfaceName) const {
    std::vector<core::Node*> nodes = GetNodesImplementingInterface(interfaceName);
    return nodes.empty() ? nullptr : nodes.front();
}

bool ScriptAPI::InterfaceExists(const std::string& interfaceName) const {
    return core::InterfaceRegistry::GetInstance().IsInterface(interfaceName);
}

std::vector<std::string> ScriptAPI::GetAllInterfaces() const {
    return core::InterfaceRegistry::GetInstance().GetInterfaceNames();
}

nlohmann::json ScriptAPI::GetInterfaceDefinition(const std::string& interfaceName) const {
    const core::InterfaceDefinition* def =
        core::InterfaceRegistry::GetInstance().GetDefinition(interfaceName);
    return def ? def->Serialize() : nlohmann::json();
}

bool ScriptAPI::RegisterInterface(const nlohmann::json& definitionJson) {
    if (!definitionJson.is_object()) {
        return false;
    }
    core::InterfaceDefinition def =
        core::InterfaceDefinition::Deserialize(definitionJson, core::InterfaceSource::Script, "");
    if (!def.isValid) {
        return false;
    }
    return core::InterfaceRegistry::GetInstance().RegisterRuntimeInterface(def);
}

bool ScriptAPI::ArchetypeImplementsInterface(const std::string& className,
                                             const std::string& interfaceName) const {
    return core::ArchetypeRegistry::GetInstance().ArchetypeImplementsInterface(className, interfaceName);
}

std::vector<std::string> ScriptAPI::GetArchetypesImplementing(const std::string& interfaceName) const {
    return core::ArchetypeRegistry::GetInstance().GetArchetypesImplementing(interfaceName);
}

void ScriptAPI::LogInfo(const std::string& message) {
    LOG_INFO(LogCategory::Scripting, "[Script] {}", message);
}

void ScriptAPI::LogWarning(const std::string& message) {
    LOG_WARN(LogCategory::Scripting, "[Script] {}", message);
}

void ScriptAPI::LogError(const std::string& message) {
    LOG_ERROR(LogCategory::Scripting, "[Script] {}", message);
}

void ScriptAPI::LogDebug(const std::string&) {
    
}

// ========================================================================
// Node Transform (Position/Movement)
// ========================================================================

math::Vec2 ScriptAPI::GetPosition2D() const {
    if (!m_Owner) return math::Vec2(0.0f, 0.0f);

    auto* node2D = dynamic_cast<core::Node2D*>(m_Owner);
    if (node2D) {
        return node2D->GetPosition();
    }
    return math::Vec2(0.0f, 0.0f);
}

void ScriptAPI::SetPosition2D(float x, float y) {
    if (!m_Owner) return;

    auto* node2D = dynamic_cast<core::Node2D*>(m_Owner);
    if (node2D) {
        node2D->SetPosition(math::Vec2(x, y));
    }
}

void ScriptAPI::Translate2D(float dx, float dy) {
    if (!m_Owner) return;

    auto* node2D = dynamic_cast<core::Node2D*>(m_Owner);
    if (node2D) {
        math::Vec2 pos = node2D->GetPosition();
        node2D->SetPosition(math::Vec2(pos.x + dx, pos.y + dy));
    }
}

math::Vec3 ScriptAPI::GetPosition3D() const {
    if (!m_Owner) return math::Vec3(0.0f, 0.0f, 0.0f);

    auto* node3D = dynamic_cast<core::Node3D*>(m_Owner);
    if (node3D) {
        return node3D->GetPosition();
    }
    return math::Vec3(0.0f, 0.0f, 0.0f);
}

void ScriptAPI::SetPosition3D(float x, float y, float z) {
    if (!m_Owner) return;

    auto* node3D = dynamic_cast<core::Node3D*>(m_Owner);
    if (node3D) {
        node3D->SetPosition(math::Vec3(x, y, z));
    }
}

void ScriptAPI::Translate3D(float dx, float dy, float dz) {
    if (!m_Owner) return;

    auto* node3D = dynamic_cast<core::Node3D*>(m_Owner);
    if (node3D) {
        math::Vec3 pos = node3D->GetPosition();
        node3D->SetPosition(math::Vec3(pos.x + dx, pos.y + dy, pos.z + dz));
    }
}

float ScriptAPI::GetRotation2D() const {
    if (!m_Owner) return 0.0f;

    auto* node2D = dynamic_cast<core::Node2D*>(m_Owner);
    if (node2D) {
        return RadToDeg(node2D->GetRotation());
    }
    return 0.0f;
}

void ScriptAPI::SetRotation2D(float degrees) {
    if (!m_Owner) return;

    auto* node2D = dynamic_cast<core::Node2D*>(m_Owner);
    if (node2D) {
        node2D->SetRotation(DegToRad(degrees));
    }
}

void ScriptAPI::Rotate2D(float degrees) {
    if (!m_Owner) return;

    auto* node2D = dynamic_cast<core::Node2D*>(m_Owner);
    if (node2D) {
        node2D->SetRotation(node2D->GetRotation() + DegToRad(degrees));
    }
}

math::Vec2 ScriptAPI::GetScale2D() const {
    if (!m_Owner) return math::Vec2(1.0f, 1.0f);

    auto* node2D = dynamic_cast<core::Node2D*>(m_Owner);
    if (node2D) {
        return node2D->GetScale();
    }
    return math::Vec2(1.0f, 1.0f);
}

void ScriptAPI::SetScale2D(float x, float y) {
    if (!m_Owner) return;

    auto* node2D = dynamic_cast<core::Node2D*>(m_Owner);
    if (node2D) {
        node2D->SetScale(math::Vec2(x, y));
    }
}

// ========================================================================
// Global Transforms
// ========================================================================

math::Vec2 ScriptAPI::GetGlobalPosition2D() const {
    if (!m_Owner) return math::Vec2(0.0f, 0.0f);

    auto* node2D = dynamic_cast<core::Node2D*>(m_Owner);
    if (node2D) {
        return node2D->GetGlobalPosition();
    }
    return math::Vec2(0.0f, 0.0f);
}

void ScriptAPI::SetGlobalPosition2D(float x, float y) {
    if (!m_Owner) return;

    auto* node2D = dynamic_cast<core::Node2D*>(m_Owner);
    if (node2D) {
        // Calculate local position from global
        math::Vec2 globalPos(x, y);
        core::Node2D* parent2D = dynamic_cast<core::Node2D*>(m_Owner->GetParent());
        if (parent2D) {
            math::Vec2 parentGlobal = parent2D->GetGlobalPosition();
            node2D->SetPosition(globalPos - parentGlobal);
        } else {
            node2D->SetPosition(globalPos);
        }
    }
}

math::Vec3 ScriptAPI::GetGlobalPosition3D() const {
    if (!m_Owner) return math::Vec3(0.0f, 0.0f, 0.0f);

    auto* node3D = dynamic_cast<core::Node3D*>(m_Owner);
    if (node3D) {
        return node3D->GetGlobalPosition();
    }
    return math::Vec3(0.0f, 0.0f, 0.0f);
}

void ScriptAPI::SetGlobalPosition3D(float x, float y, float z) {
    if (!m_Owner) return;

    auto* node3D = dynamic_cast<core::Node3D*>(m_Owner);
    if (node3D) {
        math::Vec3 globalPos(x, y, z);
        core::Node3D* parent3D = dynamic_cast<core::Node3D*>(m_Owner->GetParent());
        if (parent3D) {
            math::Vec3 parentGlobal = parent3D->GetGlobalPosition();
            node3D->SetPosition(globalPos - parentGlobal);
        } else {
            node3D->SetPosition(globalPos);
        }
    }
}

float ScriptAPI::GetGlobalRotation2D() const {
    if (!m_Owner) return 0.0f;

    auto* node2D = dynamic_cast<core::Node2D*>(m_Owner);
    if (node2D) {
        return RadToDeg(node2D->GetGlobalRotation());
    }
    return 0.0f;
}

void ScriptAPI::SetGlobalRotation2D(float degrees) {
    if (!m_Owner) return;

    auto* node2D = dynamic_cast<core::Node2D*>(m_Owner);
    if (node2D) {
        float radians = DegToRad(degrees);
        core::Node2D* parent2D = dynamic_cast<core::Node2D*>(m_Owner->GetParent());
        if (parent2D) {
            float parentGlobalRot = parent2D->GetGlobalRotation();
            node2D->SetRotation(radians - parentGlobalRot);
        } else {
            node2D->SetRotation(radians);
        }
    }
}

math::Vec3 ScriptAPI::GetGlobalRotation3D() const {
    if (!m_Owner) return math::Vec3(0.0f, 0.0f, 0.0f);

    auto* node3D = dynamic_cast<core::Node3D*>(m_Owner);
    if (node3D) {
        // Node3D composes world rotation as parentWorld * local, so the quaternion is
        // already the world orientation. Report it as Euler degrees to match the local
        // GetRotation3D convention scripts already use.
        math::Vec3 euler = node3D->GetGlobalRotation().ToEuler();
        return euler * (180.0f / 3.14159265f);
    }
    return math::Vec3(0.0f, 0.0f, 0.0f);
}

void ScriptAPI::SetGlobalRotation3D(float pitch, float yaw, float roll) {
    if (!m_Owner) return;

    auto* node3D = dynamic_cast<core::Node3D*>(m_Owner);
    if (node3D) {
        float toRad = 3.14159265f / 180.0f;
        math::Quat target = math::Quat::FromEuler(
            math::Vec3(pitch * toRad, yaw * toRad, roll * toRad));

        // world = parentWorld * local, so local = parentWorld⁻¹ * world.
        core::Node3D* parent3D = dynamic_cast<core::Node3D*>(m_Owner->GetParent());
        if (parent3D) {
            node3D->SetRotation((parent3D->GetGlobalRotation().Inverse() * target).Normalized());
        } else {
            node3D->SetRotation(target);
        }
    }
}

math::Vec2 ScriptAPI::GetGlobalScale2D() const {
    if (!m_Owner) return math::Vec2(1.0f, 1.0f);

    auto* node2D = dynamic_cast<core::Node2D*>(m_Owner);
    if (node2D) {
        return node2D->GetGlobalScale();
    }
    return math::Vec2(1.0f, 1.0f);
}

math::Vec3 ScriptAPI::GetGlobalScale3D() const {
    if (!m_Owner) return math::Vec3(1.0f, 1.0f, 1.0f);

    auto* node3D = dynamic_cast<core::Node3D*>(m_Owner);
    if (node3D) {
        return node3D->GetGlobalScale();
    }
    return math::Vec3(1.0f, 1.0f, 1.0f);
}

math::Vec3 ScriptAPI::GetRotation3D() const {
    if (!m_Owner) return math::Vec3(0.0f, 0.0f, 0.0f);

    auto* node3D = dynamic_cast<core::Node3D*>(m_Owner);
    if (node3D) {
        // Convert quaternion to Euler angles (degrees)
        math::Quat q = node3D->GetRotation();
        math::Vec3 euler = q.ToEuler();
        return euler * (180.0f / 3.14159265f);  // Convert to degrees
    }
    return math::Vec3(0.0f, 0.0f, 0.0f);
}

void ScriptAPI::SetRotation3D(float pitch, float yaw, float roll) {
    if (!m_Owner) return;

    auto* node3D = dynamic_cast<core::Node3D*>(m_Owner);
    if (node3D) {
        // Convert degrees to radians
        float toDeg = 3.14159265f / 180.0f;
        math::Vec3 eulerRad(pitch * toDeg, yaw * toDeg, roll * toDeg);
        node3D->SetRotation(math::Quat::FromEuler(eulerRad));
    }
}

void ScriptAPI::Rotate3D(float pitch, float yaw, float roll) {
    if (!m_Owner) return;

    auto* node3D = dynamic_cast<core::Node3D*>(m_Owner);
    if (node3D) {
        float toDeg = 3.14159265f / 180.0f;
        math::Quat current = node3D->GetRotation();
        math::Quat delta = math::Quat::FromEuler(math::Vec3(pitch * toDeg, yaw * toDeg, roll * toDeg));
        node3D->SetRotation(current * delta);
    }
}

math::Vec3 ScriptAPI::GetScale3D() const {
    if (!m_Owner) return math::Vec3(1.0f, 1.0f, 1.0f);

    auto* node3D = dynamic_cast<core::Node3D*>(m_Owner);
    if (node3D) {
        return node3D->GetScale();
    }
    return math::Vec3(1.0f, 1.0f, 1.0f);
}

void ScriptAPI::SetScale3D(float x, float y, float z) {
    if (!m_Owner) return;

    auto* node3D = dynamic_cast<core::Node3D*>(m_Owner);
    if (node3D) {
        node3D->SetScale(math::Vec3(x, y, z));
    }
}

math::Vec3 ScriptAPI::GetForward() const {
    if (!m_Owner) return math::Vec3(0.0f, 0.0f, -1.0f);

    auto* node3D = dynamic_cast<core::Node3D*>(m_Owner);
    if (node3D) {
        math::Quat rot = node3D->GetRotation();
        return rot * math::Vec3(0.0f, 0.0f, -1.0f);
    }
    return math::Vec3(0.0f, 0.0f, -1.0f);
}

math::Vec3 ScriptAPI::GetRight() const {
    if (!m_Owner) return math::Vec3(1.0f, 0.0f, 0.0f);

    auto* node3D = dynamic_cast<core::Node3D*>(m_Owner);
    if (node3D) {
        math::Quat rot = node3D->GetRotation();
        return rot * math::Vec3(1.0f, 0.0f, 0.0f);
    }
    return math::Vec3(1.0f, 0.0f, 0.0f);
}

math::Vec3 ScriptAPI::GetUp() const {
    if (!m_Owner) return math::Vec3(0.0f, 1.0f, 0.0f);

    auto* node3D = dynamic_cast<core::Node3D*>(m_Owner);
    if (node3D) {
        math::Quat rot = node3D->GetRotation();
        return rot * math::Vec3(0.0f, 1.0f, 0.0f);
    }
    return math::Vec3(0.0f, 1.0f, 0.0f);
}

void ScriptAPI::LookAt2D(float targetX, float targetY) {
    if (!m_Owner) return;

    auto* node2D = dynamic_cast<core::Node2D*>(m_Owner);
    if (node2D) {
        math::Vec2 pos = node2D->GetPosition();
        float angle = std::atan2(targetY - pos.y, targetX - pos.x) * (180.0f / 3.14159265f);
        node2D->SetRotation(angle);
    }
}

void ScriptAPI::LookAt3D(float targetX, float targetY, float targetZ) {
    LookAt3D(math::Vec3(targetX, targetY, targetZ), math::Vec3(0.0f, 1.0f, 0.0f));
}

void ScriptAPI::LookAt3D(const math::Vec3& target, const math::Vec3& up) {
    if (!m_Owner) return;

    auto* node3D = dynamic_cast<core::Node3D*>(m_Owner);
    if (node3D) {
        math::Vec3 pos = node3D->GetPosition();
        math::Vec3 forward = (target - pos).Normalized();

        // Create rotation quaternion from look direction
        node3D->SetRotation(math::Quat::LookRotation(forward, up));
    }
}

float ScriptAPI::DistanceTo2D(float x, float y) const {
    math::Vec2 pos = GetPosition2D();
    float dx = x - pos.x;
    float dy = y - pos.y;
    return std::sqrt(dx * dx + dy * dy);
}

float ScriptAPI::DistanceTo2D(core::Node* other) const {
    if (!other) return 0.0f;

    auto* other2D = dynamic_cast<core::Node2D*>(other);
    if (other2D) {
        math::Vec2 otherPos = other2D->GetPosition();
        return DistanceTo2D(otherPos.x, otherPos.y);
    }
    return 0.0f;
}

float ScriptAPI::DistanceTo3D(float x, float y, float z) const {
    math::Vec3 pos = GetPosition3D();
    float dx = x - pos.x;
    float dy = y - pos.y;
    float dz = z - pos.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

float ScriptAPI::DistanceTo3D(core::Node* other) const {
    if (!other) return 0.0f;

    auto* other3D = dynamic_cast<core::Node3D*>(other);
    if (other3D) {
        math::Vec3 otherPos = other3D->GetPosition();
        return DistanceTo3D(otherPos.x, otherPos.y, otherPos.z);
    }
    return 0.0f;
}

void ScriptAPI::MoveToward2D(float targetX, float targetY, float maxDelta) {
    if (!m_Owner) return;

    auto* node2D = dynamic_cast<core::Node2D*>(m_Owner);
    if (node2D) {
        math::Vec2 pos = node2D->GetPosition();
        math::Vec2 target(targetX, targetY);
        math::Vec2 diff = target - pos;
        float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);

        if (dist <= maxDelta || dist < 0.0001f) {
            node2D->SetPosition(target);
        } else {
            math::Vec2 direction = diff / dist;
            node2D->SetPosition(pos + direction * maxDelta);
        }
    }
}

void ScriptAPI::MoveToward3D(float targetX, float targetY, float targetZ, float maxDelta) {
    if (!m_Owner) return;

    auto* node3D = dynamic_cast<core::Node3D*>(m_Owner);
    if (node3D) {
        math::Vec3 pos = node3D->GetPosition();
        math::Vec3 target(targetX, targetY, targetZ);
        math::Vec3 diff = target - pos;
        float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);

        if (dist <= maxDelta || dist < 0.0001f) {
            node3D->SetPosition(target);
        } else {
            math::Vec3 direction = diff / dist;
            node3D->SetPosition(pos + direction * maxDelta);
        }
    }
}

namespace {
// Shared script-side RNG so RandomSeed makes every random_* helper deterministic.
std::mt19937& ScriptRng() {
    static std::mt19937 gen(std::random_device{}());
    return gen;
}
}

float ScriptAPI::RandomRange(float min, float max) {
    std::uniform_real_distribution<float> dis(min, max);
    return dis(ScriptRng());
}

int ScriptAPI::RandomRangeInt(int min, int max) {
    std::uniform_int_distribution<int> dis(min, max);
    return dis(ScriptRng());
}

float ScriptAPI::RandomFloat() {
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);
    return dis(ScriptRng());
}

bool ScriptAPI::RandomBool() {
    std::uniform_int_distribution<int> dis(0, 1);
    return dis(ScriptRng()) != 0;
}

int ScriptAPI::RandomSign() {
    std::uniform_int_distribution<int> dis(0, 1);
    return dis(ScriptRng()) == 0 ? -1 : 1;
}

void ScriptAPI::RandomSeed(int seed) {
    ScriptRng().seed(static_cast<std::mt19937::result_type>(seed));
}

float ScriptAPI::GetTime() const {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - s_StartTime);
    return duration.count() / 1000.0f;
}

int ScriptAPI::GetFrameCount() const {
    return s_FrameCount;
}

float ScriptAPI::Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

float ScriptAPI::Clamp(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

float ScriptAPI::Abs(float value) {
    return std::abs(value);
}

float ScriptAPI::Sign(float value) {
    if (value > 0.0f) return 1.0f;
    if (value < 0.0f) return -1.0f;
    return 0.0f;
}

float ScriptAPI::MoveToward(float from, float to, float delta) {
    if (std::abs(to - from) <= delta) {
        return to;
    }
    return from + Sign(to - from) * delta;
}

float ScriptAPI::LerpAngle(float from, float to, float weight) {
    float diff = std::fmod(to - from, 360.0f);
    if (diff > 180.0f) diff -= 360.0f;
    if (diff < -180.0f) diff += 360.0f;
    return from + diff * weight;
}

float ScriptAPI::AngleDifference(float from, float to) {
    float diff = std::fmod(to - from, 360.0f);
    if (diff > 180.0f) diff -= 360.0f;
    if (diff < -180.0f) diff += 360.0f;
    return diff;
}

float ScriptAPI::Smoothstep(float from, float to, float t) {
    t = Clamp((t - from) / (to - from), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float ScriptAPI::InverseLerp(float from, float to, float value) {
    if (std::abs(to - from) < 0.0001f) return 0.0f;
    return (value - from) / (to - from);
}

float ScriptAPI::Remap(float value, float fromMin, float fromMax, float toMin, float toMax) {
    float t = InverseLerp(fromMin, fromMax, value);
    return Lerp(toMin, toMax, t);
}

/*static*/ float ScriptAPI::DegToRad(float degrees) {
    return degrees * 0.01745329251994329577f;
}

/*static*/ float ScriptAPI::RadToDeg(float radians) {
    return radians * 57.29577951308232087680f;
}

float ScriptAPI::Wrap(float value, float min, float max) {
    float range = max - min;
    if (std::abs(range) < 0.00001f) {
        return min;
    }
    float result = std::fmod(value - min, range);
    if (result < 0.0f) {
        result += range;
    }
    return result + min;
}

int ScriptAPI::WrapInt(int value, int min, int max) {
    int range = max - min;
    if (range == 0) {
        return min;
    }
    int result = (value - min) % range;
    if (result < 0) {
        result += range;
    }
    return result + min;
}

float ScriptAPI::PingPong(float value, float length) {
    if (length <= 0.0f) {
        return 0.0f;
    }
    float t = std::fmod(std::abs(value), length * 2.0f);
    return length - std::abs(t - length);
}

float ScriptAPI::Snapped(float value, float step) {
    if (std::abs(step) < 0.00001f) {
        return value;
    }
    return std::round(value / step) * step;
}

bool ScriptAPI::IsEqualApprox(float a, float b) {
    float tolerance = 0.00001f * std::max(1.0f, std::max(std::abs(a), std::abs(b)));
    return std::abs(a - b) <= tolerance;
}

float ScriptAPI::Ease(float t, float curve) {
    t = Clamp(t, 0.0f, 1.0f);
    if (curve > 0.0f) {
        if (curve < 1.0f) {
            return 1.0f - std::pow(1.0f - t, 1.0f / curve);
        }
        return std::pow(t, curve);
    }
    if (curve < 0.0f) {
        if (t < 0.5f) {
            return std::pow(t * 2.0f, -curve) * 0.5f;
        }
        return (1.0f - std::pow(1.0f - (t - 0.5f) * 2.0f, -curve)) * 0.5f + 0.5f;
    }
    return 0.0f;
}

float ScriptAPI::PosMod(float a, float b) {
    if (std::abs(b) < 0.00001f) {
        return 0.0f;
    }
    float result = std::fmod(a, b);
    if ((result < 0.0f && b > 0.0f) || (result > 0.0f && b < 0.0f)) {
        result += b;
    }
    return result;
}

int ScriptAPI::PosModInt(int a, int b) {
    if (b == 0) {
        return 0;
    }
    int result = a % b;
    if ((result < 0 && b > 0) || (result > 0 && b < 0)) {
        result += b;
    }
    return result;
}

math::Vec2 ScriptAPI::Normalize2D(const math::Vec2& v) {
    float len = Length2D(v);
    if (len < 0.0001f) return math::Vec2(0.0f, 0.0f);
    return math::Vec2(v.x / len, v.y / len);
}

math::Vec3 ScriptAPI::Normalize3D(const math::Vec3& v) {
    float len = Length3D(v);
    if (len < 0.0001f) return math::Vec3(0.0f, 0.0f, 0.0f);
    return math::Vec3(v.x / len, v.y / len, v.z / len);
}

float ScriptAPI::Length2D(const math::Vec2& v) {
    return std::sqrt(v.x * v.x + v.y * v.y);
}

float ScriptAPI::Length3D(const math::Vec3& v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

float ScriptAPI::Dot2D(const math::Vec2& a, const math::Vec2& b) {
    return a.x * b.x + a.y * b.y;
}

float ScriptAPI::Dot3D(const math::Vec3& a, const math::Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

math::Vec3 ScriptAPI::Cross(const math::Vec3& a, const math::Vec3& b) {
    return math::Vec3(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}

// ========================================================================
// Physics Queries (2D)
// ========================================================================

ScriptAPI::RaycastHit2D ScriptAPI::Raycast2D(const math::Vec2& from, const math::Vec2& direction, float maxDistance, uint32_t collisionMask) {
    RaycastHit2D result;
    result.hit = false;

    auto* sceneManager = m_SceneManager ? m_SceneManager : core::SceneManager::GetInstance();
    if (!sceneManager) return result;

    auto* physicsWorld = sceneManager->GetPhysics2DWorld();
    if (!physicsWorld) return result;

    physics2d::RaycastHit2D physicsHit;
    if (physicsWorld->Raycast(from, direction, maxDistance, physicsHit, nullptr, static_cast<uint64_t>(collisionMask))) {
        result.hit = true;
        result.point = physicsHit.point;
        result.normal = physicsHit.normal;
        result.fraction = physicsHit.fraction;
        result.distance = maxDistance * physicsHit.fraction;
        result.bodyId = physicsHit.bodyId.ToString();
        result.collider = physicsWorld->GetBodyNode(physicsHit.bodyId);
    }

    return result;
}

std::vector<ScriptAPI::RaycastHit2D> ScriptAPI::RaycastAll2D(const math::Vec2& from, const math::Vec2& direction, float maxDistance, uint32_t collisionMask) {
    std::vector<RaycastHit2D> results;

    auto* sceneManager = m_SceneManager ? m_SceneManager : core::SceneManager::GetInstance();
    if (!sceneManager) return results;

    auto* physicsWorld = sceneManager->GetPhysics2DWorld();
    if (!physicsWorld) return results;

    auto physicsHits = physicsWorld->RaycastAll(from, direction, maxDistance, nullptr, static_cast<uint64_t>(collisionMask));
    for (const auto& physicsHit : physicsHits) {
        RaycastHit2D hit;
        hit.hit = true;
        hit.point = physicsHit.point;
        hit.normal = physicsHit.normal;
        hit.fraction = physicsHit.fraction;
        hit.distance = maxDistance * physicsHit.fraction;
        hit.bodyId = physicsHit.bodyId.ToString();
        hit.collider = physicsWorld->GetBodyNode(physicsHit.bodyId);
        results.push_back(hit);
    }

    return results;
}

std::vector<core::Node*> ScriptAPI::OverlapCircle(const math::Vec2& center, float radius, uint32_t collisionMask) {
    std::vector<core::Node*> results;

    auto* sceneManager = m_SceneManager ? m_SceneManager : core::SceneManager::GetInstance();
    if (!sceneManager) return results;

    auto* physicsWorld = sceneManager->GetPhysics2DWorld();
    if (!physicsWorld) return results;

    auto overlaps = physicsWorld->OverlapCircle(center, radius, static_cast<uint64_t>(collisionMask));
    results.reserve(overlaps.size());
    for (const auto& overlap : overlaps) {
        if (core::Node* node = physicsWorld->GetBodyNode(overlap.bodyId)) {
            results.push_back(node);
        }
    }

    return results;
}

std::vector<core::Node*> ScriptAPI::OverlapRect(const math::Vec2& center, const math::Vec2& halfExtents, uint32_t collisionMask) {
    std::vector<core::Node*> results;

    auto* sceneManager = m_SceneManager ? m_SceneManager : core::SceneManager::GetInstance();
    if (!sceneManager) return results;

    auto* physicsWorld = sceneManager->GetPhysics2DWorld();
    if (!physicsWorld) return results;

    auto overlaps = physicsWorld->OverlapBox(center, halfExtents * 2.0f, 0.0f, static_cast<uint64_t>(collisionMask));
    results.reserve(overlaps.size());
    for (const auto& overlap : overlaps) {
        if (core::Node* node = physicsWorld->GetBodyNode(overlap.bodyId)) {
            results.push_back(node);
        }
    }

    return results;
}

ScriptAPI::ShapeCastHit2D ScriptAPI::CircleCast2D(const math::Vec2& from, const math::Vec2& to, float radius, uint32_t collisionMask) {
    ShapeCastHit2D result;
    result.hit = false;

    auto* sceneManager = m_SceneManager ? m_SceneManager : core::SceneManager::GetInstance();
    if (!sceneManager) return result;

    auto* physicsWorld = sceneManager->GetPhysics2DWorld();
    if (!physicsWorld) return result;

    physics2d::ShapeCastHit2D physicsHit;
    if (physicsWorld->ShapeCast(from, to, radius, physicsHit, nullptr, static_cast<uint64_t>(collisionMask))) {
        result.hit = true;
        result.point = physicsHit.point;
        result.normal = physicsHit.normal;
        result.fraction = physicsHit.fraction;
        result.bodyId = physicsHit.bodyId.ToString();
        result.collider = physicsWorld->GetBodyNode(physicsHit.bodyId);
    }

    return result;
}

// ========================================================================
// Physics Queries (3D)
// ========================================================================

ScriptAPI::RaycastHit3D ScriptAPI::Raycast3D(const math::Vec3& from, const math::Vec3& direction, float maxDistance, uint32_t) {
    RaycastHit3D result;
    result.hit = false;

    auto* sceneManager = m_SceneManager ? m_SceneManager : core::SceneManager::GetInstance();
    if (!sceneManager) return result;

    auto* physicsWorld = sceneManager->GetPhysics3DWorld();
    if (!physicsWorld) return result;

    physics3d::RaycastHit3D physicsHit;
    if (physicsWorld->Raycast(from, direction, maxDistance, physicsHit, nullptr)) {
        result.hit = true;
        result.point = physicsHit.point;
        result.normal = physicsHit.normal;
        result.fraction = physicsHit.fraction;
        result.distance = maxDistance * physicsHit.fraction;
        result.bodyId = physicsHit.bodyId.ToString();
        result.collider = physicsWorld->GetBodyNode(physicsHit.bodyId);
    }

    return result;
}

std::vector<ScriptAPI::RaycastHit3D> ScriptAPI::RaycastAll3D(const math::Vec3& from, const math::Vec3& direction, float maxDistance, uint32_t) {
    std::vector<RaycastHit3D> results;

    auto* sceneManager = m_SceneManager ? m_SceneManager : core::SceneManager::GetInstance();
    if (!sceneManager) return results;

    auto* physicsWorld = sceneManager->GetPhysics3DWorld();
    if (!physicsWorld) return results;

    auto physicsHits = physicsWorld->RaycastAll(from, direction, maxDistance, nullptr);
    for (const auto& physicsHit : physicsHits) {
        RaycastHit3D hit;
        hit.hit = true;
        hit.point = physicsHit.point;
        hit.normal = physicsHit.normal;
        hit.fraction = physicsHit.fraction;
        hit.distance = maxDistance * physicsHit.fraction;
        hit.bodyId = physicsHit.bodyId.ToString();
        hit.collider = physicsWorld->GetBodyNode(physicsHit.bodyId);
        results.push_back(hit);
    }

    return results;
}

std::vector<core::Node*> ScriptAPI::OverlapSphere(const math::Vec3& center, float radius, uint32_t) {
    std::vector<core::Node*> results;

    auto* sceneManager = m_SceneManager ? m_SceneManager : core::SceneManager::GetInstance();
    if (!sceneManager) return results;

    auto* physicsWorld = sceneManager->GetPhysics3DWorld();
    if (!physicsWorld) return results;

    auto overlaps = physicsWorld->OverlapSphere(center, radius);
    for (const auto& overlap : overlaps) {
        if (core::Node* node = physicsWorld->GetBodyNode(overlap.bodyId)) {
            results.push_back(node);
        }
    }

    return results;
}

std::vector<core::Node*> ScriptAPI::OverlapBox(const math::Vec3& center, const math::Vec3& halfExtents, uint32_t) {
    std::vector<core::Node*> results;

    auto* sceneManager = m_SceneManager ? m_SceneManager : core::SceneManager::GetInstance();
    if (!sceneManager) return results;

    auto* physicsWorld = sceneManager->GetPhysics3DWorld();
    if (!physicsWorld) return results;

    auto overlaps = physicsWorld->OverlapBox(center, halfExtents);
    for (const auto& overlap : overlaps) {
        if (core::Node* node = physicsWorld->GetBodyNode(overlap.bodyId)) {
            results.push_back(node);
        }
    }

    return results;
}

ScriptAPI::ShapeCastHit3D ScriptAPI::SphereCast3D(const math::Vec3& from, const math::Vec3& to, float radius, uint32_t) {
    ShapeCastHit3D result;
    result.hit = false;

    auto* sceneManager = m_SceneManager ? m_SceneManager : core::SceneManager::GetInstance();
    if (!sceneManager) return result;

    auto* physicsWorld = sceneManager->GetPhysics3DWorld();
    if (!physicsWorld) return result;

    physics3d::ShapeCastHit3D physicsHit;
    if (physicsWorld->SphereCast(from, to, radius, physicsHit, nullptr)) {
        result.hit = true;
        result.point = physicsHit.point;
        result.normal = physicsHit.normal;
        result.fraction = physicsHit.fraction;
        result.bodyId = physicsHit.bodyId.ToString();
        result.collider = physicsWorld->GetBodyNode(physicsHit.bodyId);
    }

    return result;
}

// ========================================================================
// Physics Body Manipulation (2D)
// ========================================================================

components::RigidBody2DComponent* ScriptAPI::GetRigidBody2D() const {
    if (!m_Owner) return nullptr;
    auto comp = m_Owner->GetComponent("RigidBody2DComponent");
    return dynamic_cast<components::RigidBody2DComponent*>(comp.get());
}

math::Vec2 ScriptAPI::GetLinearVelocity2D() const {
    auto* rb = GetRigidBody2D();
    return rb ? rb->GetLinearVelocity() : math::Vec2(0.0f, 0.0f);
}

void ScriptAPI::SetLinearVelocity2D(float x, float y) {
    SetLinearVelocity2D(math::Vec2(x, y));
}

void ScriptAPI::SetLinearVelocity2D(const math::Vec2& velocity) {
    auto* rb = GetRigidBody2D();
    if (rb) rb->SetLinearVelocity(velocity);
}

float ScriptAPI::GetAngularVelocity2D() const {
    auto* rb = GetRigidBody2D();
    return rb ? rb->GetAngularVelocity() : 0.0f;
}

void ScriptAPI::SetAngularVelocity2D(float omega) {
    auto* rb = GetRigidBody2D();
    if (rb) rb->SetAngularVelocity(omega);
}

void ScriptAPI::ApplyForce2D(float forceX, float forceY) {
    ApplyForce2D(math::Vec2(forceX, forceY));
}

void ScriptAPI::ApplyForce2D(const math::Vec2& force) {
    auto* rb = GetRigidBody2D();
    if (rb) rb->ApplyForceToCenter(force);
}

void ScriptAPI::ApplyForceAtPoint2D(float forceX, float forceY, float pointX, float pointY) {
    ApplyForceAtPoint2D(math::Vec2(forceX, forceY), math::Vec2(pointX, pointY));
}

void ScriptAPI::ApplyForceAtPoint2D(const math::Vec2& force, const math::Vec2& point) {
    auto* rb = GetRigidBody2D();
    if (rb) rb->ApplyForce(force, point);
}

void ScriptAPI::ApplyTorque2D(float torque) {
    auto* rb = GetRigidBody2D();
    if (rb) rb->ApplyTorque(torque);
}

void ScriptAPI::ApplyImpulse2D(float impulseX, float impulseY) {
    ApplyImpulse2D(math::Vec2(impulseX, impulseY));
}

void ScriptAPI::ApplyImpulse2D(const math::Vec2& impulse) {
    auto* rb = GetRigidBody2D();
    if (rb) rb->ApplyLinearImpulseToCenter(impulse);
}

void ScriptAPI::ApplyImpulseAtPoint2D(float impulseX, float impulseY, float pointX, float pointY) {
    ApplyImpulseAtPoint2D(math::Vec2(impulseX, impulseY), math::Vec2(pointX, pointY));
}

void ScriptAPI::ApplyImpulseAtPoint2D(const math::Vec2& impulse, const math::Vec2& point) {
    auto* rb = GetRigidBody2D();
    if (rb) rb->ApplyLinearImpulse(impulse, point);
}

void ScriptAPI::ApplyAngularImpulse2D(float impulse) {
    auto* rb = GetRigidBody2D();
    if (rb) rb->ApplyAngularImpulse(impulse);
}

float ScriptAPI::GetMass2D() const {
    auto* rb = GetRigidBody2D();
    return rb ? rb->GetMass() : 0.0f;
}

float ScriptAPI::GetGravityScale2D() const {
    auto* rb = GetRigidBody2D();
    return rb ? rb->GetGravityScale() : 1.0f;
}

void ScriptAPI::SetGravityScale2D(float scale) {
    auto* rb = GetRigidBody2D();
    if (rb) rb->SetGravityScale(scale);
}

float ScriptAPI::GetLinearDamping2D() const {
    auto* rb = GetRigidBody2D();
    return rb ? rb->GetLinearDamping() : 0.0f;
}

void ScriptAPI::SetLinearDamping2D(float damping) {
    auto* rb = GetRigidBody2D();
    if (rb) rb->SetLinearDamping(damping);
}

float ScriptAPI::GetAngularDamping2D() const {
    auto* rb = GetRigidBody2D();
    return rb ? rb->GetAngularDamping() : 0.0f;
}

void ScriptAPI::SetAngularDamping2D(float damping) {
    auto* rb = GetRigidBody2D();
    if (rb) rb->SetAngularDamping(damping);
}

bool ScriptAPI::IsFixedRotation2D() const {
    auto* rb = GetRigidBody2D();
    return rb ? rb->GetFixedRotation() : false;
}

void ScriptAPI::SetFixedRotation2D(bool fixed) {
    auto* rb = GetRigidBody2D();
    if (rb) rb->SetFixedRotation(fixed);
}

bool ScriptAPI::IsBullet2D() const {
    auto* rb = GetRigidBody2D();
    return rb ? rb->GetBullet() : false;
}

void ScriptAPI::SetBullet2D(bool bullet) {
    auto* rb = GetRigidBody2D();
    if (rb) rb->SetBullet(bullet);
}

// ========================================================================
// Physics Body Manipulation (3D)
// ========================================================================

components::RigidBody3DComponent* ScriptAPI::GetRigidBody3D() const {
    if (!m_Owner) return nullptr;
    auto comp = m_Owner->GetComponent("RigidBody3DComponent");
    return dynamic_cast<components::RigidBody3DComponent*>(comp.get());
}

math::Vec3 ScriptAPI::GetLinearVelocity3D() const {
    auto* rb = GetRigidBody3D();
    return rb ? rb->GetLinearVelocity() : math::Vec3(0.0f, 0.0f, 0.0f);
}

void ScriptAPI::SetLinearVelocity3D(float x, float y, float z) {
    SetLinearVelocity3D(math::Vec3(x, y, z));
}

void ScriptAPI::SetLinearVelocity3D(const math::Vec3& velocity) {
    auto* rb = GetRigidBody3D();
    if (rb) rb->SetLinearVelocity(velocity);
}

math::Vec3 ScriptAPI::GetAngularVelocity3D() const {
    auto* rb = GetRigidBody3D();
    return rb ? rb->GetAngularVelocity() : math::Vec3(0.0f, 0.0f, 0.0f);
}

void ScriptAPI::SetAngularVelocity3D(float x, float y, float z) {
    SetAngularVelocity3D(math::Vec3(x, y, z));
}

void ScriptAPI::SetAngularVelocity3D(const math::Vec3& omega) {
    auto* rb = GetRigidBody3D();
    if (rb) rb->SetAngularVelocity(omega);
}

void ScriptAPI::ApplyForce3D(float forceX, float forceY, float forceZ) {
    ApplyForce3D(math::Vec3(forceX, forceY, forceZ));
}

void ScriptAPI::ApplyForce3D(const math::Vec3& force) {
    auto* rb = GetRigidBody3D();
    if (rb) rb->ApplyForceToCenter(force);
}

void ScriptAPI::ApplyForceAtPoint3D(float forceX, float forceY, float forceZ, float pointX, float pointY, float pointZ) {
    ApplyForceAtPoint3D(math::Vec3(forceX, forceY, forceZ), math::Vec3(pointX, pointY, pointZ));
}

void ScriptAPI::ApplyForceAtPoint3D(const math::Vec3& force, const math::Vec3& point) {
    auto* rb = GetRigidBody3D();
    if (rb) rb->ApplyForce(force, point);
}

void ScriptAPI::ApplyTorque3D(float x, float y, float z) {
    ApplyTorque3D(math::Vec3(x, y, z));
}

void ScriptAPI::ApplyTorque3D(const math::Vec3& torque) {
    auto* rb = GetRigidBody3D();
    if (rb) rb->ApplyTorque(torque);
}

void ScriptAPI::ApplyImpulse3D(float impulseX, float impulseY, float impulseZ) {
    ApplyImpulse3D(math::Vec3(impulseX, impulseY, impulseZ));
}

void ScriptAPI::ApplyImpulse3D(const math::Vec3& impulse) {
    auto* rb = GetRigidBody3D();
    if (rb) rb->ApplyImpulseToCenter(impulse);
}

void ScriptAPI::ApplyImpulseAtPoint3D(float impulseX, float impulseY, float impulseZ, float pointX, float pointY, float pointZ) {
    ApplyImpulseAtPoint3D(math::Vec3(impulseX, impulseY, impulseZ), math::Vec3(pointX, pointY, pointZ));
}

void ScriptAPI::ApplyImpulseAtPoint3D(const math::Vec3& impulse, const math::Vec3& point) {
    auto* rb = GetRigidBody3D();
    if (rb) rb->ApplyImpulse(impulse, point);
}

void ScriptAPI::ApplyTorqueImpulse3D(float x, float y, float z) {
    ApplyTorqueImpulse3D(math::Vec3(x, y, z));
}

void ScriptAPI::ApplyTorqueImpulse3D(const math::Vec3& torque) {
    auto* rb = GetRigidBody3D();
    if (rb) rb->ApplyTorqueImpulse(torque);
}

float ScriptAPI::GetMass3D() const {
    auto* rb = GetRigidBody3D();
    return rb ? rb->GetMass() : 0.0f;
}

void ScriptAPI::SetMass3D(float mass) {
    auto* rb = GetRigidBody3D();
    if (rb) rb->SetMass(mass);
}

float ScriptAPI::GetGravityScale3D() const {
    auto* rb = GetRigidBody3D();
    return rb ? rb->GetGravityScale() : 1.0f;
}

void ScriptAPI::SetGravityScale3D(float scale) {
    auto* rb = GetRigidBody3D();
    if (rb) rb->SetGravityScale(scale);
}

float ScriptAPI::GetLinearDamping3D() const {
    auto* rb = GetRigidBody3D();
    return rb ? rb->GetLinearDamping() : 0.0f;
}

void ScriptAPI::SetLinearDamping3D(float damping) {
    auto* rb = GetRigidBody3D();
    if (rb) rb->SetLinearDamping(damping);
}

float ScriptAPI::GetAngularDamping3D() const {
    auto* rb = GetRigidBody3D();
    return rb ? rb->GetAngularDamping() : 0.0f;
}

void ScriptAPI::SetAngularDamping3D(float damping) {
    auto* rb = GetRigidBody3D();
    if (rb) rb->SetAngularDamping(damping);
}

math::Vec3 ScriptAPI::GetLinearFactor3D() const {
    // Linear factor not supported by RigidBody3DComponent yet
    return math::Vec3(1.0f, 1.0f, 1.0f);
}

void ScriptAPI::SetLinearFactor3D(float x, float y, float z) {
    // Linear factor not supported by RigidBody3DComponent yet
    (void)x; (void)y; (void)z;
}

math::Vec3 ScriptAPI::GetAngularFactor3D() const {
    // Angular factor not supported by RigidBody3DComponent yet
    return math::Vec3(1.0f, 1.0f, 1.0f);
}

void ScriptAPI::SetAngularFactor3D(float x, float y, float z) {
    // Angular factor not supported by RigidBody3DComponent yet
    (void)x; (void)y; (void)z;
}

// ========================================================================
// Physics World Access
// ========================================================================

void ScriptAPI::SetGravity2D(float x, float y) {
    auto* sceneManager = m_SceneManager ? m_SceneManager : core::SceneManager::GetInstance();
    if (!sceneManager) return;

    auto* physicsWorld = sceneManager->GetPhysics2DWorld();
    if (physicsWorld) {
        physicsWorld->SetGravity(math::Vec2(x, y));
    }
}

math::Vec2 ScriptAPI::GetGravity2D() const {
    auto* sceneManager = m_SceneManager ? m_SceneManager : core::SceneManager::GetInstance();
    if (!sceneManager) return math::Vec2(0.0f, -9.81f);

    auto* physicsWorld = sceneManager->GetPhysics2DWorld();
    if (physicsWorld) {
        return physicsWorld->GetGravity();
    }
    return math::Vec2(0.0f, -9.81f);
}

void ScriptAPI::SetGravity3D(float x, float y, float z) {
    auto* sceneManager = m_SceneManager ? m_SceneManager : core::SceneManager::GetInstance();
    if (!sceneManager) return;

    auto* physicsWorld = sceneManager->GetPhysics3DWorld();
    if (physicsWorld) {
        physicsWorld->SetGravity(math::Vec3(x, y, z));
    }
}

math::Vec3 ScriptAPI::GetGravity3D() const {
    auto* sceneManager = m_SceneManager ? m_SceneManager : core::SceneManager::GetInstance();
    if (!sceneManager) return math::Vec3(0.0f, -9.81f, 0.0f);

    auto* physicsWorld = sceneManager->GetPhysics3DWorld();
    if (physicsWorld) {
        return physicsWorld->GetGravity();
    }
    return math::Vec3(0.0f, -9.81f, 0.0f);
}

// ========================================================================
// Character Controller (2D)
// ========================================================================

components::CharacterController2D* ScriptAPI::GetCharacterController2D() const {
    if (!m_Owner) return nullptr;
    auto component = m_Owner->GetComponent<components::CharacterController2D>();
    return component ? component.get() : nullptr;
}

math::Vec2 ScriptAPI::MoveAndSlide2D(float velocityX, float velocityY) {
    return MoveAndSlide2D(math::Vec2(velocityX, velocityY));
}

math::Vec2 ScriptAPI::MoveAndSlide2D(const math::Vec2& velocity) {
    auto* controller = GetCharacterController2D();
    if (controller) {
        return controller->MoveAndSlide(velocity, m_DeltaTime);
    }
    return math::Vec2(0.0f, 0.0f);
}

bool ScriptAPI::MoveAndCollide2D(float velocityX, float velocityY, math::Vec2& outActualMovement) {
    auto* controller = GetCharacterController2D();
    if (controller) {
        return controller->MoveAndCollide(math::Vec2(velocityX, velocityY), m_DeltaTime, outActualMovement);
    }
    outActualMovement = math::Vec2(0.0f, 0.0f);
    return false;
}

math::Vec2 ScriptAPI::GetCharacterVelocity2D() const {
    auto* controller = GetCharacterController2D();
    if (controller) {
        return controller->GetVelocity();
    }
    return math::Vec2(0.0f, 0.0f);
}

void ScriptAPI::SetCharacterVelocity2D(float x, float y) {
    SetCharacterVelocity2D(math::Vec2(x, y));
}

void ScriptAPI::SetCharacterVelocity2D(const math::Vec2& velocity) {
    auto* controller = GetCharacterController2D();
    if (controller) {
        controller->SetVelocity(velocity);
    }
}

bool ScriptAPI::IsCharacterOnGround2D() const {
    auto* controller = GetCharacterController2D();
    if (controller) {
        return controller->IsOnGround();
    }
    return false;
}

bool ScriptAPI::IsCharacterOnWall2D() const {
    auto* controller = GetCharacterController2D();
    if (controller) {
        return controller->IsOnWall();
    }
    return false;
}

bool ScriptAPI::IsCharacterOnCeiling2D() const {
    auto* controller = GetCharacterController2D();
    if (controller) {
        return controller->IsOnCeiling();
    }
    return false;
}

math::Vec2 ScriptAPI::GetCharacterGroundNormal2D() const {
    auto* controller = GetCharacterController2D();
    if (controller) {
        return controller->GetGroundNormal();
    }
    return math::Vec2(0.0f, 1.0f);
}

math::Vec2 ScriptAPI::GetCharacterWallNormal2D() const {
    auto* controller = GetCharacterController2D();
    if (controller) {
        return controller->GetWallNormal();
    }
    return math::Vec2(0.0f, 0.0f);
}

float ScriptAPI::GetCharacterGravity2D() const {
    auto* controller = GetCharacterController2D();
    if (controller) {
        return controller->GetGravity();
    }
    return 980.0f;
}

void ScriptAPI::SetCharacterGravity2D(float gravity) {
    auto* controller = GetCharacterController2D();
    if (controller) {
        controller->SetGravity(gravity);
    }
}

float ScriptAPI::GetCharacterMaxFallSpeed2D() const {
    auto* controller = GetCharacterController2D();
    if (controller) {
        return controller->GetMaxFallSpeed();
    }
    return 1000.0f;
}

void ScriptAPI::SetCharacterMaxFallSpeed2D(float speed) {
    auto* controller = GetCharacterController2D();
    if (controller) {
        controller->SetMaxFallSpeed(speed);
    }
}

float ScriptAPI::GetCharacterMaxSlopeAngle2D() const {
    auto* controller = GetCharacterController2D();
    if (controller) {
        return controller->GetMaxSlopeAngle();
    }
    return 45.0f;
}

void ScriptAPI::SetCharacterMaxSlopeAngle2D(float angle) {
    auto* controller = GetCharacterController2D();
    if (controller) {
        controller->SetMaxSlopeAngle(angle);
    }
}

bool ScriptAPI::GetCharacterSnapToGround2D() const {
    auto* controller = GetCharacterController2D();
    if (controller) {
        return controller->GetSnapToGround();
    }
    return true;
}

void ScriptAPI::SetCharacterSnapToGround2D(bool snap) {
    auto* controller = GetCharacterController2D();
    if (controller) {
        controller->SetSnapToGround(snap);
    }
}

// ========================================================================
// Character Controller (3D)
// ========================================================================

components::CharacterController3D* ScriptAPI::GetCharacterController3D() const {
    if (!m_Owner) return nullptr;
    auto component = m_Owner->GetComponent<components::CharacterController3D>();
    return component ? component.get() : nullptr;
}

math::Vec3 ScriptAPI::MoveAndSlide3D(float velocityX, float velocityY, float velocityZ) {
    return MoveAndSlide3D(math::Vec3(velocityX, velocityY, velocityZ));
}

math::Vec3 ScriptAPI::MoveAndSlide3D(const math::Vec3& velocity) {
    auto* controller = GetCharacterController3D();
    if (controller) {
        return controller->MoveAndSlide(velocity, m_DeltaTime);
    }
    return math::Vec3(0.0f, 0.0f, 0.0f);
}

bool ScriptAPI::MoveAndCollide3D(float velocityX, float velocityY, float velocityZ, math::Vec3& outActualMovement) {
    auto* controller = GetCharacterController3D();
    if (controller) {
        return controller->MoveAndCollide(math::Vec3(velocityX, velocityY, velocityZ), m_DeltaTime, outActualMovement);
    }
    outActualMovement = math::Vec3(0.0f, 0.0f, 0.0f);
    return false;
}

math::Vec3 ScriptAPI::GetCharacterVelocity3D() const {
    auto* controller = GetCharacterController3D();
    if (controller) {
        return controller->GetVelocity();
    }
    return math::Vec3(0.0f, 0.0f, 0.0f);
}

void ScriptAPI::SetCharacterVelocity3D(float x, float y, float z) {
    SetCharacterVelocity3D(math::Vec3(x, y, z));
}

void ScriptAPI::SetCharacterVelocity3D(const math::Vec3& velocity) {
    auto* controller = GetCharacterController3D();
    if (controller) {
        controller->SetVelocity(velocity);
    }
}

bool ScriptAPI::IsCharacterOnGround3D() const {
    auto* controller = GetCharacterController3D();
    if (controller) {
        return controller->IsOnGround();
    }
    return false;
}

bool ScriptAPI::IsCharacterOnWall3D() const {
    auto* controller = GetCharacterController3D();
    if (controller) {
        return controller->IsOnWall();
    }
    return false;
}

bool ScriptAPI::IsCharacterOnCeiling3D() const {
    auto* controller = GetCharacterController3D();
    if (controller) {
        return controller->IsOnCeiling();
    }
    return false;
}

math::Vec3 ScriptAPI::GetCharacterGroundNormal3D() const {
    auto* controller = GetCharacterController3D();
    if (controller) {
        return controller->GetGroundNormal();
    }
    return math::Vec3(0.0f, 1.0f, 0.0f);
}

math::Vec3 ScriptAPI::GetCharacterWallNormal3D() const {
    auto* controller = GetCharacterController3D();
    if (controller) {
        return controller->GetWallNormal();
    }
    return math::Vec3(0.0f, 0.0f, 0.0f);
}

float ScriptAPI::GetCharacterGravity3D() const {
    auto* controller = GetCharacterController3D();
    if (controller) {
        return controller->GetGravity();
    }
    return 9.81f;
}

void ScriptAPI::SetCharacterGravity3D(float gravity) {
    auto* controller = GetCharacterController3D();
    if (controller) {
        controller->SetGravity(gravity);
    }
}

float ScriptAPI::GetCharacterMaxFallSpeed3D() const {
    auto* controller = GetCharacterController3D();
    if (controller) {
        return controller->GetMaxFallSpeed();
    }
    return 50.0f;
}

void ScriptAPI::SetCharacterMaxFallSpeed3D(float speed) {
    auto* controller = GetCharacterController3D();
    if (controller) {
        controller->SetMaxFallSpeed(speed);
    }
}

float ScriptAPI::GetCharacterMaxSlopeAngle3D() const {
    auto* controller = GetCharacterController3D();
    if (controller) {
        return controller->GetMaxSlopeAngle();
    }
    return 45.0f;
}

void ScriptAPI::SetCharacterMaxSlopeAngle3D(float angle) {
    auto* controller = GetCharacterController3D();
    if (controller) {
        controller->SetMaxSlopeAngle(angle);
    }
}

float ScriptAPI::GetCharacterStepHeight3D() const {
    auto* controller = GetCharacterController3D();
    if (controller) {
        return controller->GetStepHeight();
    }
    return 0.3f;
}

void ScriptAPI::SetCharacterStepHeight3D(float height) {
    auto* controller = GetCharacterController3D();
    if (controller) {
        controller->SetStepHeight(height);
    }
}

bool ScriptAPI::GetCharacterSnapToGround3D() const {
    auto* controller = GetCharacterController3D();
    if (controller) {
        return controller->GetSnapToGround();
    }
    return true;
}

void ScriptAPI::SetCharacterSnapToGround3D(bool snap) {
    auto* controller = GetCharacterController3D();
    if (controller) {
        controller->SetSnapToGround(snap);
    }
}

// ========================================================================
// Timers
// ========================================================================

void ScriptAPI::CreateTimer(float delay, const std::string& callbackFunctionName) {
    CreateTimerComponent(delay, callbackFunctionName, false, -1, "");
}

void ScriptAPI::CreateRepeatingTimer(float interval, const std::string& callbackFunctionName, int repeatCount) {
    CreateTimerComponent(interval, callbackFunctionName, true, repeatCount, "");
}

core::Component* ScriptAPI::CreateTimerComponent(float delay, const std::string& callbackFunctionName,
                                                 bool repeating, int repeatCount,
                                                 const std::string& timerName) {
    if (!m_Owner) {
        return nullptr;
    }

    auto timer = std::make_shared<components::Timer>(timerName.empty() ? "Timer" : timerName);
    timer->RegisterProperties();
    timer->SetDuration(delay);
    timer->SetLoop(repeating);
    timer->SetRepeatCount(repeating ? repeatCount : 1);

    m_Owner->AddComponent(timer);

    if (!callbackFunctionName.empty()) {
        timer->Connect("timeout", m_Owner, callbackFunctionName, 0);
    }

    timer->Start();
    return timer.get();
}

std::vector<core::Component*> ScriptAPI::ListTimers() const {
    std::vector<core::Component*> result;
    if (!m_Owner) {
        return result;
    }
    for (auto& comp : m_Owner->GetComponents()) {
        if (comp && comp->GetTypeName() == "Timer") {
            result.push_back(comp.get());
        }
    }
    return result;
}

void ScriptAPI::StartTimer(core::Component* timer) {
    if (auto* t = dynamic_cast<components::Timer*>(timer)) {
        t->Start();
    }
}

void ScriptAPI::StopTimer(core::Component* timer) {
    if (auto* t = dynamic_cast<components::Timer*>(timer)) {
        t->Stop();
    }
}

void ScriptAPI::ResetTimer(core::Component* timer) {
    if (auto* t = dynamic_cast<components::Timer*>(timer)) {
        t->Reset();
    }
}

void ScriptAPI::RestartTimer(core::Component* timer) {
    if (auto* t = dynamic_cast<components::Timer*>(timer)) {
        t->Restart();
    }
}

void ScriptAPI::RemoveTimer(core::Component* timer) {
    if (!m_Owner || !timer) {
        return;
    }
    for (auto& comp : m_Owner->GetComponents()) {
        if (comp.get() == timer) {
            m_Owner->RemoveComponent(comp);
            break;
        }
    }
}

core::Component* ScriptAPI::CreateTweenComponent(const std::string& channel, const nlohmann::json& toValue,
                                                 float duration, const std::string& easing,
                                                 core::Node* target) {
    core::Node* node = target ? target : m_Owner;
    if (!node) {
        return nullptr;
    }

    auto tween = std::make_shared<components::Tween>();
    tween->RegisterProperties();
    tween->Configure(channel, toValue, duration, easing);
    tween->SetAutoRemove(true);

    node->AddComponent(tween);
    tween->Start();
    return tween.get();
}

std::vector<core::Component*> ScriptAPI::ListTweens() const {
    std::vector<core::Component*> result;
    if (!m_Owner) {
        return result;
    }
    for (auto& comp : m_Owner->GetComponents()) {
        if (comp && comp->GetTypeName() == "Tween") {
            result.push_back(comp.get());
        }
    }
    return result;
}

// ========================================================================
// Sandboxed File I/O
// ========================================================================

bool ScriptAPI::IsSandboxedPath(const std::string& path) {
    // Only the engine's writable/readable virtual roots are permitted.
    const bool allowedPrefix =
        path.rfind("res://", 0) == 0 ||
        path.rfind("user://", 0) == 0 ||
        path.rfind("temp://", 0) == 0;
    if (!allowedPrefix) {
        return false;
    }
    // Reject parent-directory traversal and embedded NULs. Rejecting any ".."
    // occurrence is intentionally conservative for a sandbox.
    if (path.find("..") != std::string::npos) {
        return false;
    }
    if (path.find('\0') != std::string::npos) {
        return false;
    }
    return true;
}

bool ScriptAPI::FileExists(const std::string& path) const {
    if (!IsSandboxedPath(path)) return false;
    return platform::VirtualFileSystem::GetInstance().Exists(path);
}

bool ScriptAPI::FileIsFile(const std::string& path) const {
    if (!IsSandboxedPath(path)) return false;
    return platform::VirtualFileSystem::GetInstance().IsFile(path);
}

bool ScriptAPI::FileIsDirectory(const std::string& path) const {
    if (!IsSandboxedPath(path)) return false;
    return platform::VirtualFileSystem::GetInstance().IsDirectory(path);
}

bool ScriptAPI::ReadTextFile(const std::string& path, std::string& outText) const {
    if (!IsSandboxedPath(path)) return false;
    auto result = platform::VirtualFileSystem::GetInstance().ReadFile(path);
    if (!result.success) return false;
    outText = result.data;
    return true;
}

bool ScriptAPI::WriteTextFile(const std::string& path, const std::string& text) const {
    if (!IsSandboxedPath(path)) return false;
    return platform::VirtualFileSystem::GetInstance().WriteFile(path, text).success;
}

bool ScriptAPI::AppendTextFile(const std::string& path, const std::string& text) const {
    if (!IsSandboxedPath(path)) return false;
    auto& vfs = platform::VirtualFileSystem::GetInstance();
    std::string existing;
    auto read = vfs.ReadFile(path);
    if (read.success) {
        existing = read.data;
    }
    existing += text;
    return vfs.WriteFile(path, existing).success;
}

bool ScriptAPI::ReadBytesFile(const std::string& path, std::vector<uint8_t>& outData) const {
    if (!IsSandboxedPath(path)) return false;
    auto result = platform::VirtualFileSystem::GetInstance().ReadBinaryFile(path);
    if (!result.success) return false;
    outData = result.data;
    return true;
}

bool ScriptAPI::WriteBytesFile(const std::string& path, const std::vector<uint8_t>& data) const {
    if (!IsSandboxedPath(path)) return false;
    return platform::VirtualFileSystem::GetInstance().WriteBinaryFile(path, data).success;
}

bool ScriptAPI::DeleteFilePath(const std::string& path) const {
    if (!IsSandboxedPath(path)) return false;
    return platform::VirtualFileSystem::GetInstance().DeleteFile(path).success;
}

bool ScriptAPI::MakeDirectory(const std::string& path) const {
    if (!IsSandboxedPath(path)) return false;
    return platform::VirtualFileSystem::GetInstance().CreateDirectory(path, true).success;
}

std::vector<std::string> ScriptAPI::ListDirectory(const std::string& path) const {
    if (!IsSandboxedPath(path)) return {};
    auto result = platform::VirtualFileSystem::GetInstance().ListDirectory(path, false);
    if (!result.success) return {};
    return result.data;
}

int64_t ScriptAPI::GetFileSize(const std::string& path) const {
    if (!IsSandboxedPath(path)) return -1;
    auto result = platform::VirtualFileSystem::GetInstance().GetFileSize(path);
    if (!result.success) return -1;
    return static_cast<int64_t>(result.data);
}

int ScriptAPI::GetGlobalInt(const std::string& name, int defaultValue) const {
    if (m_SceneManager) {
        return m_SceneManager->GetGlobalInt(name, defaultValue);
    }
    return defaultValue;
}

float ScriptAPI::GetGlobalFloat(const std::string& name, float defaultValue) const {
    if (m_SceneManager) {
        return m_SceneManager->GetGlobalFloat(name, defaultValue);
    }
    return defaultValue;
}

std::string ScriptAPI::GetGlobalString(const std::string& name, const std::string& defaultValue) const {
    if (m_SceneManager) {
        return m_SceneManager->GetGlobalString(name, defaultValue);
    }
    return defaultValue;
}

bool ScriptAPI::GetGlobalBool(const std::string& name, bool defaultValue) const {
    if (m_SceneManager) {
        return m_SceneManager->GetGlobalBool(name, defaultValue);
    }
    return defaultValue;
}

void ScriptAPI::SetGlobalInt(const std::string& name, int value) {
    if (m_SceneManager) {
        m_SceneManager->SetGlobalInt(name, value);
    }
}

void ScriptAPI::SetGlobalFloat(const std::string& name, float value) {
    if (m_SceneManager) {
        m_SceneManager->SetGlobalFloat(name, value);
    }
}

void ScriptAPI::SetGlobalString(const std::string& name, const std::string& value) {
    if (m_SceneManager) {
        m_SceneManager->SetGlobalString(name, value);
    }
}

void ScriptAPI::SetGlobalBool(const std::string& name, bool value) {
    if (m_SceneManager) {
        m_SceneManager->SetGlobalBool(name, value);
    }
}

nlohmann::json ScriptAPI::GetGlobalValue(const std::string& name, const nlohmann::json& defaultValue) const {
    if (m_SceneManager) {
        return m_SceneManager->GetGlobalValue(name, defaultValue);
    }
    return defaultValue;
}

void ScriptAPI::SetGlobalValue(const std::string& name, const nlohmann::json& value) {
    if (m_SceneManager) {
        m_SceneManager->SetGlobalValue(name, value);
    }
}

// ============================================================================
// Save Games
// ============================================================================

namespace {

// Result string of the most recent save/load operation, per thread.
thread_local std::string g_LastSaveError = "Success";

save::SaveSlotMeta ParseSaveMeta(const nlohmann::json& meta) {
    save::SaveSlotMeta result;
    if (meta.is_object()) {
        if (meta.contains("title") && meta["title"].is_string()) {
            result.title = meta["title"].get<std::string>();
        }
        if (meta.contains("thumbnail") && meta["thumbnail"].is_string()) {
            result.thumbnailPath = meta["thumbnail"].get<std::string>();
        }
        if (meta.contains("playtime") && meta["playtime"].is_number()) {
            result.playtimeSeconds = meta["playtime"].get<double>();
        }
        if (meta.contains("metadata") && meta["metadata"].is_object()) {
            result.metadata = meta["metadata"];
        }
    }
    return result;
}

nlohmann::json SaveSlotInfoToJson(const save::SaveSlotInfo& info) {
    nlohmann::json json = nlohmann::json::object();
    json["slot"] = info.slot;
    json["path"] = info.path;
    json["exists"] = info.exists;
    json["schema_version"] = info.schemaVersion;
    json["envelope_version"] = info.envelopeVersion;
    json["format"] = save::SaveFormatTypeToString(info.format);
    json["timestamp_unix"] = info.timestampUnix;
    json["playtime_seconds"] = info.playtimeSeconds;
    json["title"] = info.title;
    json["thumbnail"] = info.thumbnailPath;
    json["payload_bytes"] = info.payloadBytes;
    json["metadata"] = info.metadata;
    return json;
}

} // namespace

bool ScriptAPI::SaveGame(const std::string& slot, const nlohmann::json& data, const nlohmann::json& meta) {
    save::SaveData saveData = save::SaveData::FromJson(data.is_object() ? data : nlohmann::json::object());
    save::SaveResult result = save::SaveGameManager::GetInstance().Save(slot, saveData, ParseSaveMeta(meta));
    g_LastSaveError = save::SaveResultToString(result);
    return result == save::SaveResult::Success;
}

nlohmann::json ScriptAPI::LoadGame(const std::string& slot) const {
    save::SaveData saveData;
    save::SaveResult result = save::SaveGameManager::GetInstance().Load(slot, saveData);
    g_LastSaveError = save::SaveResultToString(result);
    if (result != save::SaveResult::Success) {
        return nlohmann::json();
    }
    return saveData.ToJson();
}

bool ScriptAPI::SaveSlotExists(const std::string& slot) const {
    return save::SaveGameManager::GetInstance().SlotExists(slot);
}

bool ScriptAPI::DeleteSaveSlot(const std::string& slot) {
    save::SaveResult result = save::SaveGameManager::GetInstance().DeleteSlot(slot);
    g_LastSaveError = save::SaveResultToString(result);
    return result == save::SaveResult::Success;
}

bool ScriptAPI::CopySaveSlot(const std::string& fromSlot, const std::string& toSlot, bool overwrite) {
    save::SaveResult result = save::SaveGameManager::GetInstance().CopySlot(fromSlot, toSlot, overwrite);
    g_LastSaveError = save::SaveResultToString(result);
    return result == save::SaveResult::Success;
}

bool ScriptAPI::RenameSaveSlot(const std::string& fromSlot, const std::string& toSlot, bool overwrite) {
    save::SaveResult result = save::SaveGameManager::GetInstance().RenameSlot(fromSlot, toSlot, overwrite);
    g_LastSaveError = save::SaveResultToString(result);
    return result == save::SaveResult::Success;
}

nlohmann::json ScriptAPI::ListSaveSlots() const {
    nlohmann::json array = nlohmann::json::array();
    for (const std::string& slot : save::SaveGameManager::GetInstance().ListSlots()) {
        array.push_back(slot);
    }
    return array;
}

nlohmann::json ScriptAPI::ListSaveSlotInfos() const {
    nlohmann::json array = nlohmann::json::array();
    for (const save::SaveSlotInfo& info : save::SaveGameManager::GetInstance().ListSlotInfos()) {
        array.push_back(SaveSlotInfoToJson(info));
    }
    return array;
}

nlohmann::json ScriptAPI::GetSaveSlotInfo(const std::string& slot) const {
    save::SaveSlotInfo info;
    save::SaveResult result = save::SaveGameManager::GetInstance().GetSlotInfo(slot, info);
    g_LastSaveError = save::SaveResultToString(result);
    if (result != save::SaveResult::Success) {
        return nlohmann::json();
    }
    return SaveSlotInfoToJson(info);
}

bool ScriptAPI::QuickSaveGame(const nlohmann::json& data, const nlohmann::json& meta) {
    save::SaveData saveData = save::SaveData::FromJson(data.is_object() ? data : nlohmann::json::object());
    save::SaveResult result = save::SaveGameManager::GetInstance().QuickSave(saveData, ParseSaveMeta(meta));
    g_LastSaveError = save::SaveResultToString(result);
    return result == save::SaveResult::Success;
}

nlohmann::json ScriptAPI::QuickLoadGame() const {
    save::SaveData saveData;
    save::SaveResult result = save::SaveGameManager::GetInstance().QuickLoad(saveData);
    g_LastSaveError = save::SaveResultToString(result);
    if (result != save::SaveResult::Success) {
        return nlohmann::json();
    }
    return saveData.ToJson();
}

bool ScriptAPI::HasQuickSave() const {
    return save::SaveGameManager::GetInstance().HasQuickSave();
}

bool ScriptAPI::AutoSaveGame(const nlohmann::json& data, const nlohmann::json& meta) {
    save::SaveData saveData = save::SaveData::FromJson(data.is_object() ? data : nlohmann::json::object());
    save::SaveResult result = save::SaveGameManager::GetInstance().AutoSave(saveData, ParseSaveMeta(meta));
    g_LastSaveError = save::SaveResultToString(result);
    return result == save::SaveResult::Success;
}

bool ScriptAPI::HasAutoSave() const {
    return save::SaveGameManager::GetInstance().HasAutoSave();
}

std::string ScriptAPI::GetLastSaveError() const {
    return g_LastSaveError;
}

void ScriptAPI::SetSaveDirectory(const std::string& virtualDir) {
    save::SaveGameManager::GetInstance().SetSaveDirectory(virtualDir);
}

void ScriptAPI::SetSaveFormat(const std::string& formatName) {
    save::SaveGameManager::GetInstance().SetFormatType(save::SaveFormatTypeFromString(formatName));
}

void ScriptAPI::SetSaveSchemaVersion(int version) {
    save::SaveGameManager::GetInstance().SetSchemaVersion(version);
}

int ScriptAPI::GetSaveSchemaVersion() const {
    return save::SaveGameManager::GetInstance().GetSchemaVersion();
}

void ScriptAPI::SetSaveObfuscationKey(const std::string& key) {
    if (key.empty()) {
        save::SaveGameManager::GetInstance().SetTransform(nullptr);
    } else {
        save::SaveGameManager::GetInstance().SetTransform(
            std::make_shared<save::XorObfuscationTransform>(key));
    }
}

void ScriptAPI::SetQuickSaveSlot(const std::string& slot) {
    save::SaveGameManager::GetInstance().SetQuickSaveSlot(slot);
}

void ScriptAPI::SetAutoSaveSlot(const std::string& slot) {
    save::SaveGameManager::GetInstance().SetAutoSaveSlot(slot);
}

nlohmann::json ScriptAPI::CaptureSceneState(const std::string& group) const {
    core::Scene* scene = GetScene();
    if (!scene) {
        scene = GetCurrentScene();
    }
    return save::SceneSaveState::CaptureGroup(scene, group);
}

int ScriptAPI::RestoreSceneState(const nlohmann::json& captured) const {
    core::Scene* scene = GetScene();
    if (!scene) {
        scene = GetCurrentScene();
    }
    return save::SceneSaveState::RestoreGroup(scene, captured);
}

}
}

