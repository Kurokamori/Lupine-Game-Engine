#pragma once

#include "lupine/core/Core.hpp"
#include "lupine/core/UUID.hpp"
#include "lupine/math/Math.hpp"
#include <string>
#include <memory>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <atomic>

namespace lupine {

// Forward declarations
class RenderContext;

namespace core {
    class Node;
    class Component;
    class Scene;
    class SceneManager;
}

namespace asset {
    class ImageAsset;
    class AudioAsset;
    class ModelAsset;
    class ArchetypeInstance;
    template<typename T> class AssetRef;
}

namespace physics2d {
    class Physics2DWorld;
    class RigidBody2D;
}

namespace physics3d {
    class Physics3DWorld;
    class RigidBody3D;
}

namespace components {
    class RigidBody2DComponent;
    class RigidBody3DComponent;
    class CharacterController2D;
    class CharacterController3D;
}

namespace scripting {

// Forward declaration (defined in ScriptingCore.hpp)
class IScriptEnvironment;

/**
 * Process mode for scripts
 * Determines when the script's process function is called
 */
enum class ProcessMode {
    Inherit,        // Use parent's process mode
    Pausable,       // Only process when game is not paused (default)
    WhenPaused,     // Only process when game is paused
    Always,         // Always process regardless of pause state
    Disabled        // Never process
};

/**
 * Script API Context
 * Provides access to the engine from scripts
 * This is the main interface that scripts use to interact with the engine
 */
class ScriptAPI {
public:
    ScriptAPI();
    ~ScriptAPI();
    
    // Set the owner node for this script context
    void SetOwner(core::Node* owner);
    core::Node* GetOwner() const { return m_Owner; }
    
    // ========================================================================
    // Game State Control
    // ========================================================================
    
    // Set game run state
    void SetGamePaused(bool paused);
    bool IsGamePaused() const;

    // Request the host application to quit. The runtime's main loop polls the
    // request once per frame and exits cleanly; in the editor this stops the play
    // session. No-op if there is no active scene manager.
    void RequestQuit();

    // Extra command-line / runtime arguments the game was launched with. The
    // editor's multi-instance play forwards these to each instance so a game
    // can branch on its role (e.g. detect "--server" vs "--client"). Returns
    // an empty list when none were supplied.
    std::vector<std::string> GetCommandLineArgs() const;

    // Get delta time
    float GetDeltaTime() const;

    // Global time scale: 1.0 normal, 0.5 slow-motion, 2.0 fast-forward, 0.0 freeze
    // (input keeps flowing). Affects gameplay + physics; clamped to >= 0.
    void SetTimeScale(float timeScale);
    float GetTimeScale() const;

    // ========================================================================
    // Input Handling
    // ========================================================================
    
    // Action-based input (mapped inputs). player < 0 means "any device" (the
    // default); a valid player index restricts evaluation to that player's
    // assigned devices (see the local-multiplayer player API below).
    bool IsActionPressed(const std::string& action, int player = -1) const;
    bool IsActionJustPressed(const std::string& action, int player = -1) const;
    bool IsActionJustReleased(const std::string& action, int player = -1) const;
    float GetActionStrength(const std::string& action, int player = -1) const;

    // Axis input (mapped inputs)
    float GetAxis(const std::string& axis, int player = -1) const;
    math::Vec2 GetVector(const std::string& negativeX, const std::string& positiveX,
                         const std::string& negativeY, const std::string& positiveY,
                         int player = -1) const;
    
    // Raw input (direct device access)
    bool IsKeyPressed(int keyCode) const;
    bool IsKeyJustPressed(int keyCode) const;
    bool IsKeyJustReleased(int keyCode) const;
    
    bool IsMouseButtonPressed(int button) const;
    bool IsMouseButtonJustPressed(int button) const;
    bool IsMouseButtonJustReleased(int button) const;
    math::Vec2 GetMousePosition() const;
    math::Vec2 GetMouseDelta() const;
    math::Vec2 GetMouseScrollDelta() const;

    // Gamepad input (direct device access). gamepadId 0 is the first gamepad.
    // Buttons/axes are the input::GamepadButton / input::GamepadAxis enum values.
    bool IsGamepadConnected(int gamepadId = 0) const;
    int GetGamepadCount() const;
    std::vector<int> GetConnectedGamepadIds() const;
    std::string GetGamepadName(int gamepadId = 0) const;
    bool IsGamepadButtonPressed(int button, int gamepadId = 0) const;
    bool IsGamepadButtonJustPressed(int button, int gamepadId = 0) const;
    bool IsGamepadButtonJustReleased(int button, int gamepadId = 0) const;
    float GetGamepadAxis(int axis, int gamepadId = 0) const;

    // Rumble. Motor strengths are clamped to [0, 1]; durationSeconds <= 0 means
    // "until changed/stopped". StopGamepadVibration stops any active rumble.
    void SetGamepadVibration(int gamepadId, float leftMotor, float rightMotor, float durationSeconds = 0.0f);
    void StopGamepadVibration(int gamepadId);

    // Gamepad analog stick/trigger deadzone applied to mapped axes [0, 1].
    void SetGamepadDeadzone(float deadzone);
    float GetGamepadDeadzone() const;

    // Touch input (mobile / web; also synthesized from the mouse on desktop when
    // the platform reports touch events). Positions are in window pixels.
    bool IsTouchAvailable() const;
    bool IsTouching() const;
    int GetTouchCount() const;
    math::Vec2 GetTouchPosition(int index = 0) const;
    bool IsTouchJustStarted() const;
    bool IsTouchJustEnded() const;

    // System clipboard (UTF-8 text). Get returns "" when no backend is installed;
    // Set is a no-op in that case.
    std::string GetClipboardText() const;
    void SetClipboardText(const std::string& text);

    // ------------------------------------------------------------------------
    // Active device detection (auto-swap button prompts)
    // ------------------------------------------------------------------------
    // GetActiveDeviceType returns the input::InputDeviceType of the device that
    // most recently produced meaningful input. GetGamepadType returns the
    // input::GamepadType (Xbox / PlayStation / Nintendo / ...) of a gamepad.
    int GetActiveDeviceType() const;
    int GetLastGamepadId() const;
    int GetGamepadType(int gamepadId = 0) const;

    // ------------------------------------------------------------------------
    // Input contexts / action sets (enable/disable groups of inputs)
    // ------------------------------------------------------------------------
    void EnableInputContext(const std::string& context);
    void DisableInputContext(const std::string& context);
    void SetInputContextActive(const std::string& context, bool active);
    bool IsInputContextActive(const std::string& context) const;
    void SetExclusiveInputContext(const std::string& context);
    std::vector<std::string> GetActiveInputContexts() const;
    void SetActionEnabled(const std::string& action, bool enabled);
    void SetAxisEnabled(const std::string& axis, bool enabled);

    // ------------------------------------------------------------------------
    // Local multiplayer player slots
    // ------------------------------------------------------------------------
    void SetPlayerCount(int count);
    int GetPlayerCount() const;
    void ClearPlayerAssignments();
    void AssignKeyboardMouseToPlayer(int player);
    void AssignGamepadToPlayer(int player, int gamepadId);
    void UnassignGamepad(int gamepadId);
    int GetPlayerForGamepad(int gamepadId) const;
    int GetPlayerForKeyboardMouse() const;
    bool PlayerOwnsKeyboardMouse(int player) const;
    std::vector<int> GetPlayerGamepads(int player) const;
    void SetAutoJoinEnabled(bool enabled);
    bool IsAutoJoinEnabled() const;

    // ------------------------------------------------------------------------
    // Runtime rebinding (in-game key/button remap menus)
    // ------------------------------------------------------------------------
    // Convenience adders for the common binding kinds. keyCode/button/axis use the
    // input::KeyCode / MouseButton / GamepadButton / GamepadAxis enum values.
    void AddActionKey(const std::string& action, int keyCode);
    void AddActionMouseButton(const std::string& action, int button);
    void AddActionGamepadButton(const std::string& action, int button, int gamepadId = 0);
    void AddActionGamepadAxis(const std::string& action, int axis, float scale = 1.0f, int gamepadId = 0);
    void RemoveActionBinding(const std::string& action, int index);
    void ClearActionBindings(const std::string& action);
    nlohmann::json GetActionBindings(const std::string& action) const;

    void AddAxisKey(const std::string& axis, int keyCode, float scale = 1.0f);
    void AddAxisGamepadAxis(const std::string& axis, int gamepadAxis, float scale = 1.0f, int gamepadId = 0);
    void RemoveAxisBinding(const std::string& axis, int index);
    void ClearAxisBindings(const std::string& axis);
    nlohmann::json GetAxisBindings(const std::string& axis) const;

    bool SaveInputMap(const std::string& filepath) const;
    bool LoadInputMap(const std::string& filepath);

    // ------------------------------------------------------------------------
    // Input capture (listen for the next physical input, for rebind menus)
    // ------------------------------------------------------------------------
    void StartInputCapture();
    void StartInputCaptureMask(bool keyboard, bool mouse, bool gamepad);
    void CancelInputCapture();
    bool IsCapturingInput() const;
    bool IsInputCaptureComplete() const;
    nlohmann::json GetCapturedBinding() const;   // binding object, or null if none
    void ClearCapturedBinding();
    // Append the most recently captured binding to an action (no-op if none).
    void ApplyCapturedBindingToAction(const std::string& action);

    // ------------------------------------------------------------------------
    // Glyph / prompt resolution (returns {glyph_id, label, art_path, device, gamepad_type})
    // ------------------------------------------------------------------------
    nlohmann::json GetActionGlyph(const std::string& action, int player = -1,
                                  int deviceOverride = -1) const;
    nlohmann::json GetActionGlyphs(const std::string& action) const;  // array, one per binding
    void SetGlyphLabel(const std::string& glyphId, const std::string& label);
    void SetGlyphArt(const std::string& glyphId, const std::string& artPath);
    void ClearGlyphOverride(const std::string& glyphId);
    void ClearGlyphOverrides();
    bool LoadGlyphMap(const std::string& filepath);
    bool SaveGlyphMap(const std::string& filepath) const;

    // ------------------------------------------------------------------------
    // Action delegation (connect a script function to an input action / event)
    // ------------------------------------------------------------------------
    // ConnectInputAction connects method(pressed, strength, player, device_type)
    // to fire whenever the action presses or releases. ConnectDeviceChanged
    // connects method(device_type, gamepad_id, gamepad_type). ConnectInputCaptured
    // connects method(binding) for the next captured binding. flags is a bitmask of
    // core::ConnectFlags (Deferred / OneShot). Returns a subscription id.
    uint64_t ConnectInputAction(const std::string& action, const std::string& method,
                                uint32_t flags = 0);
    void DisconnectInputAction(const std::string& action, uint64_t connectionId);
    uint64_t ConnectDeviceChanged(const std::string& method, uint32_t flags = 0);
    uint64_t ConnectInputCaptured(const std::string& method, uint32_t flags = 0);

    // ------------------------------------------------------------------------
    // Event-driven action matching (for use inside on_input_event(event))
    // ------------------------------------------------------------------------
    bool EventIsAction(const nlohmann::json& event, const std::string& action) const;
    bool EventIsActionPressed(const nlohmann::json& event, const std::string& action) const;
    bool EventIsActionReleased(const nlohmann::json& event, const std::string& action) const;

    // ========================================================================
    // Node/Scene References (available after on_ready)
    // ========================================================================

    // Get the owner node (self)
    core::Node* GetNode() const { return m_Owner; }
    core::Node* GetSelf() const { return m_Owner; }

    // Get the scene this node belongs to
    core::Scene* GetScene() const;

    // True when the owning scene is open in the editor (Godot's is_editor_hint).
    bool IsEditor() const;

    // Get the scene tree/manager
    core::SceneManager* GetTree() const { return m_SceneManager; }

    // Get the root node of the scene
    core::Node* GetRoot() const;

    // ========================================================================
    // Node/Scene Management
    // ========================================================================

    // Queue node for deletion (safe deletion at end of frame)
    void QueueFree(core::Node* node);
    void QueueFreeSelf();  // Queue the owner node for deletion

    // Queue node for deletion, deferring the queue request itself to the next
    // flush's deferred phase. Safe to call from inside a signal/physics callback.
    void QueueFreeDeferred(core::Node* node);
    void QueueFreeDeferredSelf();  // Deferred-queue the owner node for deletion

    // Free a node immediately, running lifecycle teardown now (use with caution;
    // prefer QueueFree from inside callbacks).
    void Free(core::Node* node);
    void FreeSelf();  // Free the owner node immediately

    // Destroy node immediately (use with caution)
    void DestroyNode(core::Node* node);

    // Add node to scene
    void AddChild(std::shared_ptr<core::Node> child);
    void AddSibling(std::shared_ptr<core::Node> sibling);

    // Remove node from scene
    void RemoveChild(core::Node* child);
    void RemoveChild(const std::string& name);

    // Reparent node
    void Reparent(core::Node* newParent);
    void ReparentTo(core::Node* node, core::Node* newParent);

    // Find nodes
    core::Node* FindNode(const std::string& path) const;
    core::Node* FindNodeByUUID(const std::string& uuidStr) const;

    // Resolve a global singleton/autoload node by its configured global name.
    // Returns nullptr if no singleton with that name exists.
    core::Node* GetSingleton(const std::string& name) const;
    core::Node* GetParent() const;
    core::Node* GetChild(const std::string& name) const;
    core::Node* GetChild(int index) const;
    int GetChildCount() const;
    std::vector<core::Node*> GetChildren() const;
    bool HasNode(const std::string& path) const;

    // Node properties
    std::string GetName() const;
    void SetName(const std::string& name);
    bool IsActive() const;
    void SetActive(bool active);
    bool IsVisible() const;
    void SetVisible(bool visible);
    int GetSiblingIndex() const;
    void SetSiblingIndex(int index);
    
    // ========================================================================
    // Component Management
    // ========================================================================

    // Find components
    core::Component* GetComponent(const std::string& typeName) const;
    std::vector<core::Component*> GetComponents(const std::string& typeName) const;
    core::Component* GetComponentInChildren(const std::string& typeName) const;
    core::Component* GetComponentInParent(const std::string& typeName) const;
    bool HasComponent(const std::string& typeName) const;

    // Add/Remove components
    void AddComponent(std::shared_ptr<core::Component> component);
    void RemoveComponent(core::Component* component);
    core::Component* AddComponentByType(const std::string& typeName);

    // Component property access (get/set by property name)
    // Returns empty string/0/false if property not found
    std::string GetComponentPropertyString(core::Component* component, const std::string& propName) const;
    int GetComponentPropertyInt(core::Component* component, const std::string& propName) const;
    float GetComponentPropertyFloat(core::Component* component, const std::string& propName) const;
    bool GetComponentPropertyBool(core::Component* component, const std::string& propName) const;

    void SetComponentPropertyString(core::Component* component, const std::string& propName, const std::string& value);
    void SetComponentPropertyInt(core::Component* component, const std::string& propName, int value);
    void SetComponentPropertyFloat(core::Component* component, const std::string& propName, float value);
    void SetComponentPropertyBool(core::Component* component, const std::string& propName, bool value);

    // ========================================================================
    // Groups (Godot-style node tagging)
    // ========================================================================

    // Owner-node group membership.
    void AddToGroup(const std::string& group);
    void RemoveFromGroup(const std::string& group);
    bool IsInGroup(const std::string& group) const;
    std::vector<std::string> GetGroups() const;

    // Group membership of an arbitrary node.
    void AddNodeToGroup(core::Node* node, const std::string& group);
    void RemoveNodeFromGroup(core::Node* node, const std::string& group);
    bool IsNodeInGroup(core::Node* node, const std::string& group) const;
    std::vector<std::string> GetNodeGroups(core::Node* node) const;

    // All nodes of the owner's scene (or current scene) that belong to a group.
    std::vector<core::Node*> GetNodesInGroup(const std::string& group) const;
    int GetNodeCountInGroup(const std::string& group) const;

    // First node belonging to a group (or nullptr).
    core::Node* GetFirstNodeInGroup(const std::string& group) const;

    // ========================================================================
    // Interfaces (capability contracts; see InterfaceRegistry)
    // ========================================================================

    // Owner-node conformance.
    bool ImplementsInterface(const std::string& interfaceName) const;
    std::vector<std::string> GetImplementedInterfaces() const;
    // Verify the owner against an interface's method+signal contract. Returns
    // { interface, exists, conforms, missing_methods, missing_signals }.
    nlohmann::json VerifyInterface(const std::string& interfaceName) const;

    // Conformance of an arbitrary node.
    bool NodeImplementsInterface(core::Node* node, const std::string& interfaceName) const;
    std::vector<std::string> GetNodeInterfaces(core::Node* node) const;
    nlohmann::json VerifyNodeInterface(core::Node* node, const std::string& interfaceName) const;

    // All nodes of the owner's scene (or current scene) implementing an interface.
    std::vector<core::Node*> GetNodesImplementingInterface(const std::string& interfaceName) const;
    int GetNodeCountImplementingInterface(const std::string& interfaceName) const;
    core::Node* GetFirstNodeImplementingInterface(const std::string& interfaceName) const;

    // Interface registry (definitions).
    bool InterfaceExists(const std::string& interfaceName) const;
    std::vector<std::string> GetAllInterfaces() const;
    // The serialized .interface definition, or null if the interface is unknown.
    nlohmann::json GetInterfaceDefinition(const std::string& interfaceName) const;
    // Register (or replace) an interface at runtime from a JSON definition object
    // (same shape as a .interface file). The interface persists across scene
    // switches. Returns true on success.
    bool RegisterInterface(const nlohmann::json& definitionJson);

    // Archetype conformance.
    bool ArchetypeImplementsInterface(const std::string& className,
                                      const std::string& interfaceName) const;
    std::vector<std::string> GetArchetypesImplementing(const std::string& interfaceName) const;

    // ========================================================================
    // Tree Utilities
    // ========================================================================

    // Like FindNode but its null result is explicit at the call site.
    core::Node* GetNodeOrNull(const std::string& path) const;

    // Descendants of the owner whose type name matches typeName (empty = any type).
    // recursive walks the whole subtree; otherwise only direct children.
    std::vector<core::Node*> FindChildren(const std::string& typeName, bool recursive = true) const;

    // True if the owner is an ancestor of `other` (anywhere above it in the tree).
    bool IsAncestorOf(core::Node* other) const;

    // ========================================================================
    // Instantiation
    // ========================================================================

    // Instantiate a prefab and add to scene
    core::Node* InstantiatePrefab(const std::string& prefabPath);
    core::Node* InstantiatePrefab(const std::string& prefabPath, core::Node* parent);

    // Duplicate a node
    core::Node* DuplicateNode(core::Node* node);
    core::Node* DuplicateSelf();

    // Create a new node by type
    core::Node* CreateNode(const std::string& typeName);
    core::Node* CreateNode(const std::string& typeName, const std::string& name);
    core::Node* CreateNodeChild(const std::string& typeName, const std::string& name, core::Node* parent = nullptr);
    core::Node* InstantiateScene(const std::string& scenePath, core::Node* parent = nullptr);
    
    // ========================================================================
    // Scene Management
    // ========================================================================

    // Change scene
    void ChangeScene(const std::string& scenePath);

    // Reload current scene
    void ReloadScene();

    // Add scene (additive loading)
    void AddScene(const std::string& scenePath);

    // Remove scene
    void RemoveScene(const std::string& sceneName);

    // Get current scene
    core::Scene* GetCurrentScene() const;

    // Get current scene path
    std::string GetCurrentScenePath() const;
    
    // ========================================================================
    // Asset Loading
    // ========================================================================

    // Load image asset (returns true if successful)
    bool LoadImageAsset(const std::string& assetPath);

    // Load audio asset (returns true if successful)
    // loadMode: "preload" for sound effects, "stream" for music
    bool LoadAudioAsset(const std::string& assetPath, const std::string& loadMode = "preload");

    // Load model asset (returns true if successful)
    bool LoadModelAsset(const std::string& assetPath);

    // Preload multiple assets at once
    void PreloadAssets(const std::vector<std::string>& assetPaths);

    // Load an archetype instance asset (.ares) by res:// path.
    // Returns nullptr on failure. The returned instance is owned by the
    // script API's archetype cache and remains valid for the script's lifetime.
    asset::ArchetypeInstance* LoadArchetype(const std::string& assetPath);

    // Invalidate every ScriptAPI instance's archetype cache. The next
    // LoadArchetype / GetAsyncArchetype on each instance re-reads its .ares from
    // disk, so edits to archetype instances (or their definitions) hot-reload
    // into running scripts. Cheap: bumps a shared generation counter that each
    // instance compares against lazily before serving a cached entry.
    static void InvalidateArchetypeCaches();

    // ------------------------------------------------------------------------
    // Asynchronous archetype loading
    // ------------------------------------------------------------------------
    //
    // The file read + JSON/directive parse runs on a background worker thread;
    // the registry-dependent finalize (resolving instance fields, registering a
    // definition) runs on the main thread when the engine pumps the loader.
    //
    // Two delivery styles are supported per request:
    //  - Callback: pass a non-empty callbackMethod and it is invoked once on this
    //    node's script when the load finishes. Instances call
    //    callbackMethod(handle, fields_or_nil); definitions call
    //    callbackMethod(handle, success, class_name).
    //  - Poll/await: leave callbackMethod empty and query GetAsyncLoadStatus /
    //    IsAsyncLoadComplete, then GetAsyncArchetype to take delivery.

    // Priority bands for streamed loads (mirror asset::AsyncLoadPriority).
    // priority is a plain int; higher streams in first. These name the common
    // values for scripts/host code.
    enum AsyncLoadPriority : int {
        ASYNC_PRIORITY_LOW      = -100,
        ASYNC_PRIORITY_NORMAL   = 0,
        ASYNC_PRIORITY_HIGH     = 100,
        ASYNC_PRIORITY_CRITICAL = 1000
    };

    // Begin loading an archetype instance (.ares). Returns a handle (>0), or 0 on
    // empty input. priority orders this request against other queued loads while
    // it waits for a streaming slot (higher = serviced first).
    uint64_t LoadArchetypeAsync(const std::string& assetPath, const std::string& callbackMethod = "",
                                int priority = ASYNC_PRIORITY_NORMAL);

    // Begin loading and registering an archetype definition file
    // (.archetype/.lua/.rb/.py). Returns a handle (>0), or 0 on empty input.
    uint64_t LoadArchetypeDefinitionAsync(const std::string& filePath, const std::string& callbackMethod = "",
                                          int priority = ASYNC_PRIORITY_NORMAL);

    // Re-prioritize a still-queued async load (no-op once it has started/finished).
    void SetAsyncLoadPriority(uint64_t handle, int priority);

    // The priority an async load was issued / last set with (0 if unknown).
    int GetAsyncLoadPriority(uint64_t handle) const;

    // Set the streaming budget: the maximum number of async loads that may run on
    // worker threads at once. 0 tracks the worker thread count (the default).
    void SetAsyncStreamingBudget(int maxConcurrent);

    // The effective streaming budget (resolves 0 = auto to the thread count).
    int GetAsyncStreamingBudget() const;

    // Diagnostics: number of async loads currently running on a worker, and the
    // number queued but not yet started.
    int GetAsyncInFlightCount() const;
    int GetAsyncQueuedCount() const;

    // Poll an async request. Returns: 0 pending, 1 loaded, 2 failed, 3 cancelled,
    // 4 unknown/forgotten.
    int GetAsyncLoadStatus(uint64_t handle) const;

    // True once the request has finished (loaded/failed/cancelled or unknown).
    bool IsAsyncLoadComplete(uint64_t handle) const;

    // Take delivery of a completed async instance. Caches it like LoadArchetype
    // and forgets the loader record. Returns nullptr if not loaded or failed.
    asset::ArchetypeInstance* GetAsyncArchetype(uint64_t handle);

    // Cancel and forget an async request.
    void CancelAsyncLoad(uint64_t handle);

    // Dispatch completion callbacks for finished callback-style async loads.
    // Called once per frame by the owning ScriptComponent while this node's
    // script environment is the active context.
    void DispatchCompletedAsyncLoads();

    // ========================================================================
    // Audio
    // ========================================================================

    // Play audio (returns audio source UUID as string)
    std::string PlayAudio(const std::string& audioPath, const std::string& busName = "Master",
                         bool loop = false, float volume = 1.0f);
    std::string PlayAudio3D(const std::string& audioPath, const math::Vec3& position,
                           const std::string& busName = "Master", bool loop = false, float volume = 1.0f);

    // Scheduled playback: the source is created immediately and starts playing
    // automatically after delaySeconds of engine time. Returns its UUID.
    std::string PlayAudioScheduled(const std::string& audioPath, float delaySeconds,
                                   const std::string& busName = "Master", bool loop = false, float volume = 1.0f);
    std::string PlayAudioScheduled3D(const std::string& audioPath, const math::Vec3& position, float delaySeconds,
                                     const std::string& busName = "Master", bool loop = false, float volume = 1.0f);

    // Control audio playback
    void StopAudio(const std::string& sourceUUID);
    void PauseAudio(const std::string& sourceUUID);
    void ResumeAudio(const std::string& sourceUUID);

    // Playback state. IsAudioPlaying is false for a finished/cleaned-up source.
    bool IsAudioPlaying(const std::string& sourceUUID) const;
    bool IsAudioFinished(const std::string& sourceUUID) const;

    // Audio bus control
    void SetBusVolume(const std::string& busName, float volume);
    float GetBusVolume(const std::string& busName) const;
    void SetBusMuted(const std::string& busName, bool muted);
    bool IsBusMuted(const std::string& busName) const;

    // Audio bus DSP effects. effectType is one of: "Gain", "LowPass", "HighPass",
    // "BandPass", "Peak", "LowShelf", "HighShelf", "Delay", "Reverb",
    // "Compressor", "Distortion", "Chorus". Returns the new effect's index (-1 on
    // failure). Parameter names depend on the effect (e.g. Reverb: "room_size",
    // "damping", "wet", "dry"; Compressor: "threshold_db", "ratio", ...).
    int AddBusEffect(const std::string& busName, const std::string& effectType);
    void RemoveBusEffect(const std::string& busName, int index);
    void MoveBusEffect(const std::string& busName, int fromIndex, int toIndex);
    void ClearBusEffects(const std::string& busName);
    void SetBusEffectEnabled(const std::string& busName, int index, bool enabled);
    void SetBusEffectParameter(const std::string& busName, int index,
                               const std::string& parameter, float value);
    int GetBusEffectCount(const std::string& busName) const;
    float GetBusEffectParameter(const std::string& busName, int index,
                                const std::string& parameter) const;
    bool IsBusEffectEnabled(const std::string& busName, int index) const;

    // Live post-fader/post-fx output peak of a bus (linear, 0..1+) for metering.
    float GetBusLevel(const std::string& busName) const;

    // Live control of a playing source (by the UUID string returned by PlayAudio*).
    void SetAudioSourceVolume(const std::string& sourceUUID, float volume);
    void SetAudioSourcePitch(const std::string& sourceUUID, float pitch);
    void SetAudioSourcePan(const std::string& sourceUUID, float pan);

    // Master output.
    void SetMasterVolume(float volume);
    float GetMasterVolume() const;
    void SetMasterMuted(bool muted);
    bool IsMasterMuted() const;

    // 3D listener (the "ears"); typically follows the active camera/player.
    void SetListenerPosition(const math::Vec3& position);
    void SetListenerOrientation(const math::Vec3& forward, const math::Vec3& up);
    void SetListenerVelocity(const math::Vec3& velocity);

    // Bus management.
    void CreateAudioBus(const std::string& name, const std::string& parentBus = "");
    void DestroyAudioBus(const std::string& name);
    bool HasAudioBus(const std::string& name) const;
    void SetBusSolo(const std::string& name, bool solo);
    bool IsBusSolo(const std::string& name) const;

    // ========================================================================
    // Localization
    // ========================================================================

    // Translate a key for the active locale. table is optional; empty searches
    // all loaded tables. Returns the key itself if unresolved.
    std::string Tr(const std::string& key) const;
    std::string TrInTable(const std::string& key, const std::string& table) const;

    // Translate then substitute {name} placeholders from args.
    std::string TrFormat(const std::string& key,
                         const std::unordered_map<std::string, std::string>& args,
                         const std::string& table = "") const;

    // Plural-aware translation: selects the plural form for the active locale and
    // count, then substitutes {count} plus any extra args.
    std::string TrPlural(const std::string& key, long count,
                         const std::unordered_map<std::string, std::string>& args = {},
                         const std::string& table = "") const;

    // Active locale management.
    void SetLocale(const std::string& locale);
    std::string GetLocale() const;
    std::string GetFallbackLocale() const;
    std::vector<std::string> GetAvailableLocales() const;
    bool HasLocaleKey(const std::string& key) const;

    // Reload tables from disk (dev convenience) and toggle pseudolocalization.
    void ReloadLocalization();
    void SetPseudolocalization(bool enabled);
    bool IsPseudolocalization() const;

    // ========================================================================
    // UI Theme (global)
    // ========================================================================

    // Activate the theme at a res:// path. Returns false if it cannot be loaded.
    bool SetActiveTheme(const std::string& resPath);

    // Resolve a themed colour/constant for a control type + entry against the active
    // theme. Returns white / 0 if the theme provides no such entry.
    math::Color GetThemeColor(const std::string& type, const std::string& entry) const;
    float GetThemeConstant(const std::string& type, const std::string& entry) const;

    // Live-edit the active theme's tokens (recolours every bound entry).
    bool SetThemePaletteColor(const std::string& key, const math::Color& color);
    bool SetThemeVariable(const std::string& key, float value);

    // Active theme change counter (bumps on any theme/token edit).
    uint64_t GetThemeVersion() const;

    // ========================================================================
    // Logging
    // ========================================================================

    void LogInfo(const std::string& message);
    void LogWarning(const std::string& message);
    void LogError(const std::string& message);
    void LogDebug(const std::string& message);

    // ========================================================================
    // Node Transform (Position/Movement)
    // ========================================================================

    // 2D Position (for Node2D owner)
    math::Vec2 GetPosition2D() const;
    void SetPosition2D(float x, float y);
    void Translate2D(float dx, float dy);

    // 3D Position (for Node3D owner)
    math::Vec3 GetPosition3D() const;
    void SetPosition3D(float x, float y, float z);
    void Translate3D(float dx, float dy, float dz);

    // 2D Rotation (for Node2D owner) - degrees
    float GetRotation2D() const;
    void SetRotation2D(float degrees);
    void Rotate2D(float degrees);

    // 2D Scale (for Node2D owner)
    math::Vec2 GetScale2D() const;
    void SetScale2D(float x, float y);

    // Global transforms (considering parent hierarchy)
    math::Vec2 GetGlobalPosition2D() const;
    void SetGlobalPosition2D(float x, float y);
    math::Vec3 GetGlobalPosition3D() const;
    void SetGlobalPosition3D(float x, float y, float z);
    float GetGlobalRotation2D() const;
    void SetGlobalRotation2D(float degrees);
    // 3D world rotation as Euler angles in degrees, matching the local GetRotation3D
    // convention below. The node's rotation is a quaternion internally; these compose
    // with (and decompose from) the parent's world rotation.
    math::Vec3 GetGlobalRotation3D() const;
    void SetGlobalRotation3D(float pitch, float yaw, float roll);
    // World scale (the product of every ancestor's scale and the node's own).
    math::Vec2 GetGlobalScale2D() const;
    math::Vec3 GetGlobalScale3D() const;

    // 3D Rotation (Euler angles in degrees for convenience)
    math::Vec3 GetRotation3D() const;  // Returns Euler angles in degrees
    void SetRotation3D(float pitch, float yaw, float roll);
    void Rotate3D(float pitch, float yaw, float roll);

    // 3D Scale
    math::Vec3 GetScale3D() const;
    void SetScale3D(float x, float y, float z);

    // Direction vectors (3D)
    math::Vec3 GetForward() const;
    math::Vec3 GetRight() const;
    math::Vec3 GetUp() const;

    // Look at target
    void LookAt2D(float targetX, float targetY);
    void LookAt3D(float targetX, float targetY, float targetZ);
    void LookAt3D(const math::Vec3& target, const math::Vec3& up = math::Vec3(0, 1, 0));

    // Distance calculations
    float DistanceTo2D(float x, float y) const;
    float DistanceTo2D(core::Node* other) const;
    float DistanceTo3D(float x, float y, float z) const;
    float DistanceTo3D(core::Node* other) const;

    // Move towards target
    void MoveToward2D(float targetX, float targetY, float maxDelta);
    void MoveToward3D(float targetX, float targetY, float targetZ, float maxDelta);

    // ========================================================================
    // Utility
    // ========================================================================

    // Random
    float RandomRange(float min, float max);
    int RandomRangeInt(int min, int max);
    float RandomFloat();        // Uniform in [0, 1)
    bool RandomBool();          // Even coin flip
    int RandomSign();           // -1 or +1 with even probability
    void RandomSeed(int seed);  // Reseed the shared script RNG (deterministic runs)

    // Time
    float GetTime() const;  // Time since game started
    int GetFrameCount() const;

    // Engine / OS information
    float GetFPS() const;             // Smoothed frames per second
    int GetTicksMsec() const;         // Milliseconds since game started
    double GetUnixTime() const;       // Seconds since the Unix epoch (wall clock)
    std::string GetPlatformName() const;  // "Windows", "Linux", "macOS", "Web"
    bool IsDebugBuild() const;
    float GetDPIScale() const;        // Device pixel ratio of the window's display
    bool OpenURL(const std::string& url);  // Open a URL in the default handler

    // Color helpers (colors cross as r,g,b,a components in [0,1])
    math::Color ColorFromHex(const std::string& hex) const;       // "#RRGGBB" / "#RRGGBBAA" / "#RGB"
    std::string ColorToHex(const math::Color& color) const;       // "#RRGGBBAA"
    math::Color ColorFromHSV(float h, float s, float v, float a = 1.0f) const;  // h,s,v in [0,1]
    math::Color ColorLerp(const math::Color& a, const math::Color& b, float t) const;

    // Sample a serialized Gradient/Curve resource (e.g. a component property read
    // via ComponentRef:get) at t in [0,1].
    math::Color SampleGradient(const nlohmann::json& gradient, float t) const;
    float SampleCurve(const nlohmann::json& curve, float t, float emptyDefault = 1.0f) const;

    // Math helpers
    float Lerp(float a, float b, float t);
    float Clamp(float value, float min, float max);
    float Abs(float value);
    float Sign(float value);
    float MoveToward(float from, float to, float delta);
    float LerpAngle(float from, float to, float weight);
    float AngleDifference(float from, float to);
    float Smoothstep(float from, float to, float t);
    float InverseLerp(float from, float to, float value);
    float Remap(float value, float fromMin, float fromMax, float toMin, float toMax);
    static float DegToRad(float degrees);
    static float RadToDeg(float radians);
    float Wrap(float value, float min, float max);     // Wrap float into [min, max)
    int WrapInt(int value, int min, int max);          // Wrap int into [min, max)
    float PingPong(float value, float length);         // Triangle wave 0..length..0
    float Snapped(float value, float step);            // Snap to nearest multiple of step
    bool IsEqualApprox(float a, float b);              // Compare with epsilon tolerance
    float Ease(float t, float curve);                  // Godot-style easing curve on [0,1]
    float PosMod(float a, float b);                    // Modulo with sign of divisor (float)
    int PosModInt(int a, int b);                       // Modulo with sign of divisor (int)

    // Vector math helpers
    math::Vec2 Normalize2D(const math::Vec2& v);
    math::Vec3 Normalize3D(const math::Vec3& v);
    float Length2D(const math::Vec2& v);
    float Length3D(const math::Vec3& v);
    float Dot2D(const math::Vec2& a, const math::Vec2& b);
    float Dot3D(const math::Vec3& a, const math::Vec3& b);
    math::Vec3 Cross(const math::Vec3& a, const math::Vec3& b);

    // ========================================================================
    // Physics Queries (2D)
    // ========================================================================

    // Raycast 2D - returns hit info as struct-like data
    struct RaycastHit2D {
        bool hit = false;
        math::Vec2 point;
        math::Vec2 normal;
        float distance = 0.0f;
        float fraction = 0.0f;
        core::Node* collider = nullptr;
        std::string bodyId;
    };
    RaycastHit2D Raycast2D(const math::Vec2& from, const math::Vec2& direction, float maxDistance, uint32_t collisionMask = 0xFFFFFFFF);
    std::vector<RaycastHit2D> RaycastAll2D(const math::Vec2& from, const math::Vec2& direction, float maxDistance, uint32_t collisionMask = 0xFFFFFFFF);

    // Shape queries 2D
    std::vector<core::Node*> OverlapCircle(const math::Vec2& center, float radius, uint32_t collisionMask = 0xFFFFFFFF);
    std::vector<core::Node*> OverlapRect(const math::Vec2& center, const math::Vec2& halfExtents, uint32_t collisionMask = 0xFFFFFFFF);

    // Shape cast 2D
    struct ShapeCastHit2D {
        bool hit = false;
        math::Vec2 point;
        math::Vec2 normal;
        float fraction = 0.0f;
        core::Node* collider = nullptr;
        std::string bodyId;
    };
    ShapeCastHit2D CircleCast2D(const math::Vec2& from, const math::Vec2& to, float radius, uint32_t collisionMask = 0xFFFFFFFF);

    // ========================================================================
    // Physics Queries (3D)
    // ========================================================================

    struct RaycastHit3D {
        bool hit = false;
        math::Vec3 point;
        math::Vec3 normal;
        float distance = 0.0f;
        float fraction = 0.0f;
        core::Node* collider = nullptr;
        std::string bodyId;
    };
    RaycastHit3D Raycast3D(const math::Vec3& from, const math::Vec3& direction, float maxDistance, uint32_t collisionMask = 0xFFFFFFFF);
    std::vector<RaycastHit3D> RaycastAll3D(const math::Vec3& from, const math::Vec3& direction, float maxDistance, uint32_t collisionMask = 0xFFFFFFFF);

    // Shape queries 3D
    std::vector<core::Node*> OverlapSphere(const math::Vec3& center, float radius, uint32_t collisionMask = 0xFFFFFFFF);
    std::vector<core::Node*> OverlapBox(const math::Vec3& center, const math::Vec3& halfExtents, uint32_t collisionMask = 0xFFFFFFFF);

    // Shape cast 3D
    struct ShapeCastHit3D {
        bool hit = false;
        math::Vec3 point;
        math::Vec3 normal;
        float fraction = 0.0f;
        core::Node* collider = nullptr;
        std::string bodyId;
    };
    ShapeCastHit3D SphereCast3D(const math::Vec3& from, const math::Vec3& to, float radius, uint32_t collisionMask = 0xFFFFFFFF);

    // ========================================================================
    // Physics Body Manipulation (2D)
    // ========================================================================

    // Get the RigidBody2D component from the owner node (if it has one)
    components::RigidBody2DComponent* GetRigidBody2D() const;

    // Velocity
    math::Vec2 GetLinearVelocity2D() const;
    void SetLinearVelocity2D(float x, float y);
    void SetLinearVelocity2D(const math::Vec2& velocity);
    float GetAngularVelocity2D() const;
    void SetAngularVelocity2D(float omega);

    // Forces
    void ApplyForce2D(float forceX, float forceY);
    void ApplyForce2D(const math::Vec2& force);
    void ApplyForceAtPoint2D(float forceX, float forceY, float pointX, float pointY);
    void ApplyForceAtPoint2D(const math::Vec2& force, const math::Vec2& point);
    void ApplyTorque2D(float torque);

    // Impulses
    void ApplyImpulse2D(float impulseX, float impulseY);
    void ApplyImpulse2D(const math::Vec2& impulse);
    void ApplyImpulseAtPoint2D(float impulseX, float impulseY, float pointX, float pointY);
    void ApplyImpulseAtPoint2D(const math::Vec2& impulse, const math::Vec2& point);
    void ApplyAngularImpulse2D(float impulse);

    // Mass properties
    float GetMass2D() const;

    // Physics body properties
    float GetGravityScale2D() const;
    void SetGravityScale2D(float scale);
    float GetLinearDamping2D() const;
    void SetLinearDamping2D(float damping);
    float GetAngularDamping2D() const;
    void SetAngularDamping2D(float damping);
    bool IsFixedRotation2D() const;
    void SetFixedRotation2D(bool fixed);
    bool IsBullet2D() const;
    void SetBullet2D(bool bullet);

    // ========================================================================
    // Physics Body Manipulation (3D)
    // ========================================================================

    // Get the RigidBody3D component from the owner node (if it has one)
    components::RigidBody3DComponent* GetRigidBody3D() const;

    // Velocity
    math::Vec3 GetLinearVelocity3D() const;
    void SetLinearVelocity3D(float x, float y, float z);
    void SetLinearVelocity3D(const math::Vec3& velocity);
    math::Vec3 GetAngularVelocity3D() const;
    void SetAngularVelocity3D(float x, float y, float z);
    void SetAngularVelocity3D(const math::Vec3& omega);

    // Forces
    void ApplyForce3D(float forceX, float forceY, float forceZ);
    void ApplyForce3D(const math::Vec3& force);
    void ApplyForceAtPoint3D(float forceX, float forceY, float forceZ, float pointX, float pointY, float pointZ);
    void ApplyForceAtPoint3D(const math::Vec3& force, const math::Vec3& point);
    void ApplyTorque3D(float x, float y, float z);
    void ApplyTorque3D(const math::Vec3& torque);

    // Impulses
    void ApplyImpulse3D(float impulseX, float impulseY, float impulseZ);
    void ApplyImpulse3D(const math::Vec3& impulse);
    void ApplyImpulseAtPoint3D(float impulseX, float impulseY, float impulseZ, float pointX, float pointY, float pointZ);
    void ApplyImpulseAtPoint3D(const math::Vec3& impulse, const math::Vec3& point);
    void ApplyTorqueImpulse3D(float x, float y, float z);
    void ApplyTorqueImpulse3D(const math::Vec3& torque);

    // Mass properties
    float GetMass3D() const;
    void SetMass3D(float mass);

    // Physics body properties
    float GetGravityScale3D() const;
    void SetGravityScale3D(float scale);
    float GetLinearDamping3D() const;
    void SetLinearDamping3D(float damping);
    float GetAngularDamping3D() const;
    void SetAngularDamping3D(float damping);
    math::Vec3 GetLinearFactor3D() const;
    void SetLinearFactor3D(float x, float y, float z);
    math::Vec3 GetAngularFactor3D() const;
    void SetAngularFactor3D(float x, float y, float z);

    // Physics world access
    void SetGravity2D(float x, float y);
    math::Vec2 GetGravity2D() const;
    void SetGravity3D(float x, float y, float z);
    math::Vec3 GetGravity3D() const;

    // ========================================================================
    // Character Controller (2D)
    // ========================================================================

    // Get the CharacterController2D component from the owner node (if it has one)
    components::CharacterController2D* GetCharacterController2D() const;

    // Movement API
    math::Vec2 MoveAndSlide2D(float velocityX, float velocityY);
    math::Vec2 MoveAndSlide2D(const math::Vec2& velocity);
    bool MoveAndCollide2D(float velocityX, float velocityY, math::Vec2& outActualMovement);

    // Velocity
    math::Vec2 GetCharacterVelocity2D() const;
    void SetCharacterVelocity2D(float x, float y);
    void SetCharacterVelocity2D(const math::Vec2& velocity);

    // Ground/Wall/Ceiling detection
    bool IsCharacterOnGround2D() const;
    bool IsCharacterOnWall2D() const;
    bool IsCharacterOnCeiling2D() const;
    math::Vec2 GetCharacterGroundNormal2D() const;
    math::Vec2 GetCharacterWallNormal2D() const;

    // Character controller properties
    float GetCharacterGravity2D() const;
    void SetCharacterGravity2D(float gravity);
    float GetCharacterMaxFallSpeed2D() const;
    void SetCharacterMaxFallSpeed2D(float speed);
    float GetCharacterMaxSlopeAngle2D() const;
    void SetCharacterMaxSlopeAngle2D(float angle);
    bool GetCharacterSnapToGround2D() const;
    void SetCharacterSnapToGround2D(bool snap);

    // ========================================================================
    // Character Controller (3D)
    // ========================================================================

    // Get the CharacterController3D component from the owner node (if it has one)
    components::CharacterController3D* GetCharacterController3D() const;

    // Movement API
    math::Vec3 MoveAndSlide3D(float velocityX, float velocityY, float velocityZ);
    math::Vec3 MoveAndSlide3D(const math::Vec3& velocity);
    bool MoveAndCollide3D(float velocityX, float velocityY, float velocityZ, math::Vec3& outActualMovement);

    // Velocity
    math::Vec3 GetCharacterVelocity3D() const;
    void SetCharacterVelocity3D(float x, float y, float z);
    void SetCharacterVelocity3D(const math::Vec3& velocity);

    // Ground/Wall/Ceiling detection
    bool IsCharacterOnGround3D() const;
    bool IsCharacterOnWall3D() const;
    bool IsCharacterOnCeiling3D() const;
    math::Vec3 GetCharacterGroundNormal3D() const;
    math::Vec3 GetCharacterWallNormal3D() const;

    // Character controller properties
    float GetCharacterGravity3D() const;
    void SetCharacterGravity3D(float gravity);
    float GetCharacterMaxFallSpeed3D() const;
    void SetCharacterMaxFallSpeed3D(float speed);
    float GetCharacterMaxSlopeAngle3D() const;
    void SetCharacterMaxSlopeAngle3D(float angle);
    float GetCharacterStepHeight3D() const;
    void SetCharacterStepHeight3D(float height);
    bool GetCharacterSnapToGround3D() const;
    void SetCharacterSnapToGround3D(bool snap);

    // ========================================================================
    // Timers
    // ========================================================================

    // Create a one-shot timer (calls callback after delay)
    void CreateTimer(float delay, const std::string& callbackFunctionName);

    // Create a repeating timer
    void CreateRepeatingTimer(float interval, const std::string& callbackFunctionName, int repeatCount = -1);

    core::Component* CreateTimerComponent(float delay, const std::string& callbackFunctionName,
                                          bool repeating = false, int repeatCount = -1,
                                          const std::string& timerName = "");
    std::vector<core::Component*> ListTimers() const;
    void StartTimer(core::Component* timer);
    void StopTimer(core::Component* timer);
    void ResetTimer(core::Component* timer);
    void RestartTimer(core::Component* timer);
    void RemoveTimer(core::Component* timer);

    // ========================================================================
    // Tweens
    // ========================================================================

    // Create and start a Tween component on a target node (defaults to the owner)
    // that interpolates a channel to a target value over a duration with easing.
    // The target value crosses as JSON (number or [x,y]/[x,y,z]/[x,y,z,w]).
    // Returns the created Tween component (nullptr on failure).
    core::Component* CreateTweenComponent(const std::string& channel, const nlohmann::json& toValue,
                                          float duration, const std::string& easing = "linear",
                                          core::Node* target = nullptr);
    std::vector<core::Component*> ListTweens() const;

    // ========================================================================
    // Sandboxed File I/O
    // ========================================================================
    //
    // All paths must be virtual paths under an allowed mount: "res://" (project
    // data root, read-only in packed/release builds), "user://" (user/save root,
    // read-write), or "temp://" (scratch, read-write). Any path that is not one of
    // these prefixes, or that contains a ".." traversal segment, is rejected -
    // there is no arbitrary filesystem access. Backed by the engine VFS.

    // True if `path` is an allowed, traversal-free virtual path.
    static bool IsSandboxedPath(const std::string& path);

    bool FileExists(const std::string& path) const;
    bool FileIsFile(const std::string& path) const;
    bool FileIsDirectory(const std::string& path) const;

    // Read/write whole files. Read* return false (and leave out untouched) on
    // failure or sandbox violation. Text is binary-safe (std::string).
    bool ReadTextFile(const std::string& path, std::string& outText) const;
    bool WriteTextFile(const std::string& path, const std::string& text) const;
    bool AppendTextFile(const std::string& path, const std::string& text) const;
    bool ReadBytesFile(const std::string& path, std::vector<uint8_t>& outData) const;
    bool WriteBytesFile(const std::string& path, const std::vector<uint8_t>& data) const;

    // Filesystem management (within the sandbox).
    bool DeleteFilePath(const std::string& path) const;
    bool MakeDirectory(const std::string& path) const;        // recursive
    std::vector<std::string> ListDirectory(const std::string& path) const;  // virtual paths
    int64_t GetFileSize(const std::string& path) const;       // -1 on failure

    // ========================================================================
    // Save Games
    // ========================================================================
    //
    // A high-level save-slot API over save::SaveGameManager. Game state crosses as
    // a JSON value (a native table/dict in scripts). The toolkit handles slot
    // files under "user://saves", schema versioning + migration, the container
    // format, optional obfuscation, crash-safe atomic writes and per-slot header
    // metadata. The optional `meta` object accepts the keys: "title" (string),
    // "thumbnail" (string virtual path), "playtime" (number, seconds) and
    // "metadata" (object).

    bool SaveGame(const std::string& slot, const nlohmann::json& data,
                  const nlohmann::json& meta = nlohmann::json::object());
    // Returns the loaded data object, or null when the slot is missing/unreadable.
    nlohmann::json LoadGame(const std::string& slot) const;
    bool SaveSlotExists(const std::string& slot) const;
    bool DeleteSaveSlot(const std::string& slot);
    bool CopySaveSlot(const std::string& fromSlot, const std::string& toSlot, bool overwrite = true);
    bool RenameSaveSlot(const std::string& fromSlot, const std::string& toSlot, bool overwrite = true);

    // Slot enumeration for load menus. ListSaveSlots returns an array of slot id
    // strings; ListSaveSlotInfos / GetSaveSlotInfo return header objects (slot,
    // title, timestamp_unix, playtime_seconds, schema_version, thumbnail, metadata,
    // ...) read without deserializing the full payload. GetSaveSlotInfo returns
    // null for a missing slot.
    nlohmann::json ListSaveSlots() const;
    nlohmann::json ListSaveSlotInfos() const;
    nlohmann::json GetSaveSlotInfo(const std::string& slot) const;

    // Quick/auto save convenience over the configured quick/auto slots.
    bool QuickSaveGame(const nlohmann::json& data, const nlohmann::json& meta = nlohmann::json::object());
    nlohmann::json QuickLoadGame() const;
    bool HasQuickSave() const;
    bool AutoSaveGame(const nlohmann::json& data, const nlohmann::json& meta = nlohmann::json::object());
    bool HasAutoSave() const;

    // The result string of the most recent save/load operation ("Success",
    // "SlotNotFound", "MigrationFailed", ...).
    std::string GetLastSaveError() const;

    // Configuration. Format names: "json_text", "json_compact", "cbor", "msgpack".
    // SetSaveObfuscationKey installs a reversible XOR transform (empty key clears
    // it). SetSaveSchemaVersion sets the migration target written into new saves.
    void SetSaveDirectory(const std::string& virtualDir);
    void SetSaveFormat(const std::string& formatName);
    void SetSaveSchemaVersion(int version);
    int GetSaveSchemaVersion() const;
    void SetSaveObfuscationKey(const std::string& key);
    void SetQuickSaveSlot(const std::string& slot);
    void SetAutoSaveSlot(const std::string& slot);

    // Capture/restore live scene-node state for the owner's scene. Nodes tagged
    // with `group` (default "persistent") are serialized via the engine's
    // reflection. CaptureSceneState returns an array suitable for storing in save
    // data; RestoreSceneState applies it and returns the number of nodes restored.
    nlohmann::json CaptureSceneState(const std::string& group = "persistent") const;
    int RestoreSceneState(const nlohmann::json& captured) const;

    // ========================================================================
    // Global Variables Access
    // ========================================================================

    // Get global variables
    int GetGlobalInt(const std::string& name, int defaultValue = 0) const;
    float GetGlobalFloat(const std::string& name, float defaultValue = 0.0f) const;
    std::string GetGlobalString(const std::string& name, const std::string& defaultValue = "") const;
    bool GetGlobalBool(const std::string& name, bool defaultValue = false) const;

    // Set global variables
    void SetGlobalInt(const std::string& name, int value);
    void SetGlobalFloat(const std::string& name, float value);
    void SetGlobalString(const std::string& name, const std::string& value);
    void SetGlobalBool(const std::string& name, bool value);

    // Generic JSON-valued global access. Carries any engine type, including the
    // structured ones (vectors/colors/quaternions/rects as JSON objects, and
    // arrays/dictionaries as JSON arrays/objects).
    nlohmann::json GetGlobalValue(const std::string& name,
                                  const nlohmann::json& defaultValue = nlohmann::json()) const;
    void SetGlobalValue(const std::string& name, const nlohmann::json& value);

    // ========================================================================
    // Signals & Events
    // ========================================================================

    // Emit a signal on the owner node. Arguments are supplied as a JSON array.
    void EmitSignal(const std::string& signal, const nlohmann::json& args);

    // Connect a signal on the owner node to a handler on a target node. The
    // method names a function on the target node's script(s). flags is a
    // bitmask of core::ConnectFlags (Deferred / OneShot). Returns a connection id.
    uint64_t ConnectSignal(const std::string& signal, core::Node* target,
                           const std::string& method, uint32_t flags = 0);
    void DisconnectSignal(const std::string& signal, uint64_t connectionId);
    bool IsSignalConnected(const std::string& signal) const;

    // Declare a user signal on the owner node at runtime.
    void AddUserSignal(const std::string& name);

    // Global event bus (decoupled pub/sub). Handlers are functions on the owner
    // node's script(s).
    void EmitEvent(const std::string& event, const nlohmann::json& args);
    uint64_t SubscribeEvent(const std::string& event, const std::string& method, uint32_t flags = 0);
    void UnsubscribeEvent(const std::string& event, uint64_t subscriptionId);

    // Defer a method call on the owner node to the end of the frame.
    void CallDeferred(const std::string& method, const nlohmann::json& args);

    // ========================================================================
    // Window / Display
    // ========================================================================
    //
    // Backed by platform::DisplayServer. The runtime installs the platform backend;
    // in headless/editor contexts setters cache their value and getters report the
    // cached state. Mouse mode values match platform::MouseMode.

    void SetWindowTitle(const std::string& title);
    std::string GetWindowTitle() const;
    void SetFullscreen(bool fullscreen);
    bool IsFullscreen() const;
    void SetVSync(bool enabled);
    bool IsVSync() const;
    void SetWindowSize(int width, int height);
    math::Vec2 GetWindowSize() const;       // Window client size, pixels
    math::Vec2 GetScreenSize() const;       // Primary display resolution, pixels
    void MaximizeWindow();
    void MinimizeWindow();
    void RestoreWindow();

    // Mouse mode: 0 Visible, 1 Hidden, 2 Captured, 3 Confined, 4 ConfinedHidden.
    void SetMouseMode(int mode);
    int GetMouseMode() const;
    void SetMouseCursorVisible(bool visible);
    bool IsMouseCursorVisible() const;

    // ========================================================================
    // Screen <-> World Coordinate Conversion
    // ========================================================================
    //
    // Resolve against the current scene's active Camera2D / Camera3D and the
    // window size. Screen coordinates are window pixels with the origin at the
    // top-left (matching GetMousePosition). When no suitable active camera or
    // window size is available the input value is returned unchanged / zero.

    // A world-space ray (origin + normalized direction), for 3D mouse picking.
    struct ScreenRay {
        math::Vec3 origin;
        math::Vec3 direction;
    };

    math::Vec2 ScreenToWorld2D(const math::Vec2& screenPos) const;
    math::Vec2 WorldToScreen2D(const math::Vec2& worldPos) const;
    ScreenRay ScreenToWorldRay3D(const math::Vec2& screenPos) const;
    math::Vec3 ScreenToWorld3D(const math::Vec2& screenPos, float distance) const;
    math::Vec3 WorldToScreen3D(const math::Vec3& worldPos) const;  // xy = pixels, z = ndc depth

    // ========================================================================
    // Debug Draw
    // ========================================================================
    //
    // Immediate-mode debug primitives in world space. A duration of 0 draws for the
    // current frame only; a positive duration keeps the primitive alive that many
    // seconds. Rendered for every active view in both the runtime and the editor;
    // a no-op when no debug renderer exists. Colors cross as r,g,b,a in [0,1].

    void DebugDrawLine(const math::Vec3& start, const math::Vec3& end, const math::Color& color, float duration = 0.0f);
    void DebugDrawLine2D(const math::Vec2& start, const math::Vec2& end, const math::Color& color, float duration = 0.0f);
    void DebugDrawRay(const math::Vec3& origin, const math::Vec3& direction, const math::Color& color, float duration = 0.0f);
    void DebugDrawBox(const math::Vec3& center, const math::Vec3& size, const math::Color& color, float duration = 0.0f);
    void DebugDrawSphere(const math::Vec3& center, float radius, const math::Color& color, float duration = 0.0f);
    void DebugDrawCircle(const math::Vec3& center, const math::Vec3& normal, float radius, const math::Color& color, float duration = 0.0f);
    void DebugDrawText(const math::Vec3& position, const std::string& text, const math::Color& color, float duration = 0.0f);
    void DebugDrawText2D(const math::Vec2& position, const std::string& text, const math::Color& color, float duration = 0.0f);

    // ========================================================================
    // Custom Component Rendering (valid only inside on_draw)
    // ========================================================================
    //
    // These record real, runtime-visible draw commands and are only valid while
    // the engine is calling a custom component's on_draw hook (the active render
    // context is set for the duration of that call). Outside on_draw they are
    // no-ops. Positions/sizes are in the owner node's spatial space (world units
    // for World2D/World3D, canvas pixels for UI). Colors cross as r,g,b,a in
    // [0,1]. blendMode: 0 Alpha, 1 Additive, 2 Multiply, 3 Opaque, 4 Overlay.

    // True while an on_draw call is in flight (a render context is bound).
    bool IsDrawing() const { return m_CurrentRenderContext != nullptr; }

    // Colored quad (2D/3D) centered-or-corner per the underlying RenderContext;
    // position is a world point, size in world/canvas units.
    void DrawQuad(const math::Vec3& position, const math::Vec2& size, const math::Color& color,
                  int blendMode = 0);
    // Textured quad. texturePath is a res:// image path; resolved + cached.
    void DrawTexturedQuad(const math::Vec3& position, const math::Vec2& size, const math::Color& tint,
                          const std::string& texturePath, int blendMode = 0);
    // Axis-aligned 2D rectangle (top-left position + size). filled=false draws a
    // 1px outline of the given thickness.
    void DrawRect(const math::Vec2& position, const math::Vec2& size, const math::Color& color,
                  bool filled = true, float thickness = 1.0f);
    // 2D sprite from a res:// image path (resolved + cached), centered at position.
    void DrawSprite(const std::string& texturePath, const math::Vec2& position, const math::Vec2& size,
                    const math::Color& tint = math::Color(1.0f, 1.0f, 1.0f, 1.0f),
                    float rotation = 0.0f, int blendMode = 0);
    void DrawLine(const math::Vec3& start, const math::Vec3& end, const math::Color& color,
                  float thickness = 1.0f);
    void DrawCircle(const math::Vec3& center, float radius, const math::Color& color, bool filled = true);
    void DrawPolygon(const math::Vec2& center, float radius, int sides, const math::Color& color,
                     float rotation = 0.0f, int blendMode = 0);
    void DrawBox(const math::Vec3& center, const math::Vec3& size, const math::Color& color,
                 bool wireframe = false);
    void DrawRoundedRect(const math::Vec2& position, const math::Vec2& size, float cornerRadius,
                         const math::Color& color, int blendMode = 0);

    // ========================================================================
    // Editor-only debug drawing (no runtime visual)
    // ========================================================================
    //
    // Routed through the editor DebugDraw overlay, which is a no-op in shipped
    // runtime builds. Intended to be called from a custom component's on_render
    // hook to draw editor gizmo geometry (selection helpers, ranges, scalable
    // bounds boxes) that must NOT appear in the running game. Contrast with the
    // debug_draw_* family above, which renders in BOTH the editor and the
    // runtime. Returns from IsEditorDrawAvailable() are true only while the
    // editor overlay pass is active.

    bool IsEditorDrawAvailable() const;
    void EditorDrawLine(const math::Vec3& start, const math::Vec3& end, const math::Color& color);
    void EditorDrawBox(const math::Vec3& center, const math::Vec3& size, const math::Color& color,
                       bool wireframe = true);
    void EditorDrawSphere(const math::Vec3& center, float radius, const math::Color& color,
                          bool wireframe = true);
    void EditorDrawCircle(const math::Vec3& center, const math::Vec3& normal, float radius,
                          const math::Color& color);
    void EditorDrawRect2D(const math::Vec2& center, const math::Vec2& size, const math::Color& color);
    void EditorDrawText(const math::Vec3& position, const std::string& text, const math::Color& color);

    // Internal setters (used by ScriptComponent)
    void SetDeltaTime(float deltaTime) { m_DeltaTime = deltaTime; }
    void SetSceneManager(core::SceneManager* sceneManager) { m_SceneManager = sceneManager; }

    // Bind/unbind the active render context around an on_draw call. Set by the
    // carrier (ScriptComponent / ScriptedComponentWrapper) immediately before
    // invoking on_draw and cleared immediately after, so the Draw* methods above
    // only ever record into the in-flight gather.
    void SetRenderContext(RenderContext* ctx) { m_CurrentRenderContext = ctx; }
    RenderContext* GetRenderContext() const { return m_CurrentRenderContext; }

    // The script environment used to dispatch async completion callbacks. Set by
    // the owning ScriptComponent once the environment is created.
    void SetEnvironment(IScriptEnvironment* environment) { m_Environment = environment; }

private:
    // Resolve the node a spawned/instantiated node should attach to: the supplied
    // parent if non-null, otherwise the owner's scene root, otherwise the current
    // scene's root via the scene manager. Returns nullptr when none is available.
    core::Node* ResolveSpawnParent(core::Node* parent) const;

    core::Node* m_Owner = nullptr;
    core::SceneManager* m_SceneManager = nullptr;
    IScriptEnvironment* m_Environment = nullptr;
    float m_DeltaTime = 0.0f;

    // Active render context, non-null only for the duration of an on_draw call.
    RenderContext* m_CurrentRenderContext = nullptr;

    // Asset cache - stores loaded assets by path
    std::unordered_map<std::string, asset::AssetRef<asset::ImageAsset>> m_ImageCache;
    std::unordered_map<std::string, asset::AssetRef<asset::AudioAsset>> m_AudioCache;
    std::unordered_map<std::string, asset::AssetRef<asset::ModelAsset>> m_ModelCache;
    std::unordered_map<std::string, asset::AssetRef<asset::ArchetypeInstance>> m_ArchetypeCache;

    // Generation this instance's m_ArchetypeCache was last synced to. When the
    // shared s_ArchetypeCacheGeneration moves ahead (an archetype asset changed),
    // the cache is cleared lazily on the next archetype access. See
    // InvalidateArchetypeCaches() and SyncArchetypeCacheGeneration().
    uint64_t m_ArchetypeCacheGeneration = 0;
    static std::atomic<uint64_t> s_ArchetypeCacheGeneration;

    // Clear m_ArchetypeCache if the shared generation advanced past this
    // instance's. Called at the top of every archetype-cache read path.
    void SyncArchetypeCacheGeneration();

    // Outstanding async archetype load requests issued by this script, keyed by
    // the loader handle. Tracked so callbacks can be dispatched, results promoted
    // into the cache, and records released when this script is destroyed.
    struct AsyncArchetypeRequest {
        std::string path;            // original instance/definition path requested
        std::string callbackMethod;  // empty for poll/await-style requests
        bool isDefinition = false;
    };
    std::unordered_map<uint64_t, AsyncArchetypeRequest> m_AsyncArchetypeRequests;

    friend class ScriptComponent;
};

} // namespace scripting
} // namespace lupine

