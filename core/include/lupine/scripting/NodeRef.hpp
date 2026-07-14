#pragma once

#include "lupine/math/Math.hpp"
#include <nlohmann/json.hpp>
#include <cstdint>
#include <string>
#include <memory>
#include <vector>

namespace lupine {

namespace core {
    class Node;
    class Component;
    class Scene;
    class SceneManager;
    class SignalObject;
    struct SignalLifetime;
}

namespace scripting {

class ScriptAPI;
class NodeRef;
class SceneRef;
class TweenRef;
class SequenceRef;
class SignalAwaiter;

/**
 * ComponentRef - script-facing handle to a single Component.
 *
 * Wraps a weak reference to a core::Component so scripts can read and write its
 * declared properties as an object. Property values cross the language boundary
 * as JSON, so each runtime binding converts to/from its native value types once.
 */
class ComponentRef {
public:
    ComponentRef() = default;
    // The handle stores the (process-lifetime) SceneManager rather than the
    // originating component's ScriptAPI: a script may cache a handle and use it
    // after the component that produced it is destroyed, and dereferencing a
    // freed ScriptAPI is undefined behaviour. The ScriptAPI* overload exists for
    // call sites that only have the active api; it captures the api's tree.
    ComponentRef(std::weak_ptr<core::Component> component, ScriptAPI* api);
    ComponentRef(std::weak_ptr<core::Component> component, core::SceneManager* tree)
        : m_Component(component), m_Tree(tree) {}

    bool IsValid() const;
    std::shared_ptr<core::Component> Lock() const { return m_Component.lock(); }
    core::SceneManager* GetTree() const { return m_Tree; }

    std::string GetTypeName() const;
    std::string GetName() const;
    bool IsEnabled() const;
    void SetEnabled(bool enabled);

    // Property access (component-declared properties), values as JSON.
    bool HasProperty(const std::string& propName) const;
    nlohmann::json Get(const std::string& propName) const;
    void Set(const std::string& propName, const nlohmann::json& value);

    // Invoke a named control method on the component (generic dispatch to
    // Component::CallMethod, e.g. AnimationPlayer::Play). Arguments are a JSON
    // array; the JSON result is returned (null when the method has no result).
    nlohmann::json Call(const std::string& method, const nlohmann::json& args) const;

    // Signals (e.g. a Button's "pressed"). Arguments cross as a JSON array.
    void EmitSignal(const std::string& signal, const nlohmann::json& args) const;
    uint64_t ConnectSignal(const std::string& signal, const NodeRef& target,
                           const std::string& method, uint32_t flags = 0) const;
    void DisconnectSignal(const std::string& signal, uint64_t connectionId) const;
    void DisconnectSignalMethod(const std::string& signal, const NodeRef& target,
                                const std::string& method) const;
    bool IsSignalConnected(const std::string& signal) const;
    void AddUserSignal(const std::string& name) const;
    std::vector<std::string> GetSignalList() const;

    // Create a one-shot latch that records when `signal` next fires on this
    // component. Used by the scripting await schedulers (await_signal).
    SignalAwaiter AwaitSignal(const std::string& signal) const;

    NodeRef GetOwner() const;

private:
    std::weak_ptr<core::Component> m_Component;
    core::SceneManager* m_Tree = nullptr;
};

/**
 * NodeRef - script-facing handle to a single Node (Godot-style node object).
 *
 * Wraps a weak reference to a core::Node so scripts can navigate the tree, read
 * and write transform/state, access components, and read/write properties on an
 * object obtained via Lupine.get_node("%UniqueName") / get_self(). The weak
 * reference keeps handles safe across frames: once the node is destroyed the
 * handle reports IsValid() == false and operations become no-ops.
 *
 * Transform and query operations are delegated to a temporary ScriptAPI bound to
 * the wrapped node, reusing the engine's existing per-node logic rather than
 * duplicating it.
 */
class NodeRef {
public:
    NodeRef() = default;
    // See ComponentRef for why the handle stores the long-lived SceneManager
    // rather than a per-component ScriptAPI: a tile/UI handle stored in a Lua
    // table and replayed by a coroutine must stay valid after the component that
    // created it is freed. The ScriptAPI* overload captures the api's tree.
    NodeRef(std::weak_ptr<core::Node> node, ScriptAPI* api);
    NodeRef(std::weak_ptr<core::Node> node, core::SceneManager* tree)
        : m_Node(node), m_Tree(tree) {}

    // Build a handle from a raw or shared node pointer. Returns an invalid handle
    // when the node is null or not owned by a shared_ptr.
    static NodeRef FromRaw(core::Node* node, ScriptAPI* api);
    static NodeRef FromShared(const std::shared_ptr<core::Node>& node, ScriptAPI* api);
    // Same, but against an already-resolved (long-lived) scene tree. Used by the
    // ref types to propagate a handle's tree to navigation/owner results without
    // re-deriving it from a (possibly already-freed) component api.
    static NodeRef FromRawTree(core::Node* node, core::SceneManager* tree);
    static NodeRef FromSharedTree(const std::shared_ptr<core::Node>& node, core::SceneManager* tree);

    bool IsValid() const;
    // Resolve the node. Returns null when the node is destroyed OR when the handle
    // resolves to a non-canonical (corrupted) pointer, so every NodeRef operation
    // safely no-ops on a dead/corrupted handle instead of dereferencing garbage.
    std::shared_ptr<core::Node> Lock() const;
    core::SceneManager* GetTree() const { return m_Tree; }

    // Identity / state
    std::string GetName() const;
    void SetName(const std::string& name);
    std::string GetUUID() const;
    std::string GetPath() const;
    std::string GetTypeName() const;
    bool IsActive() const;
    void SetActive(bool active);
    bool IsVisible() const;
    void SetVisible(bool visible);
    bool IsUniqueNameInOwner() const;
    void SetUniqueNameInOwner(bool unique);

    // Groups (Godot-style node tagging)
    void AddToGroup(const std::string& group) const;
    void RemoveFromGroup(const std::string& group) const;
    bool IsInGroup(const std::string& group) const;
    std::vector<std::string> GetGroups() const;

    // Interface conformance (capability contracts; see InterfaceRegistry).
    bool ImplementsInterface(const std::string& interfaceName) const;
    std::vector<std::string> GetInterfaces() const;
    nlohmann::json VerifyInterface(const std::string& interfaceName) const;

    // Tree navigation / structure
    NodeRef GetParent() const;
    NodeRef GetChild(const std::string& name) const;
    NodeRef GetChildAt(int index) const;
    int GetChildCount() const;
    std::vector<NodeRef> GetChildren() const;
    NodeRef FindNode(const std::string& path) const;   // %-aware, relative to this node
    bool HasNode(const std::string& path) const;
    void AddChild(const NodeRef& child);
    void RemoveChild(const NodeRef& child);
    void ReparentTo(const NodeRef& newParent);
    // Sibling ordering / child reordering.
    int GetSiblingIndex() const;                       // index within parent, -1 if none
    void SetSiblingIndex(int index);                   // move self among siblings
    int GetChildIndex(const NodeRef& child) const;     // index of a direct child, -1 if not
    void MoveChild(const NodeRef& child, int index);   // reorder a direct child
    void QueueFree();
    void QueueFreeDeferred();
    void Free();
    NodeRef Duplicate() const;

    // Transform 2D
    math::Vec2 GetPosition2D() const;
    void SetPosition2D(float x, float y);
    void Translate2D(float dx, float dy);
    float GetRotation2D() const;
    void SetRotation2D(float degrees);
    math::Vec2 GetScale2D() const;
    void SetScale2D(float x, float y);
    math::Vec2 GetGlobalPosition2D() const;
    void SetGlobalPosition2D(float x, float y);
    float GetGlobalRotation2D() const;
    void SetGlobalRotation2D(float degrees);
    math::Vec2 GetGlobalScale2D() const;

    // Transform 3D (rotation expressed as Euler degrees, matching ScriptAPI)
    math::Vec3 GetPosition3D() const;
    void SetPosition3D(float x, float y, float z);
    void Translate3D(float dx, float dy, float dz);
    math::Vec3 GetRotation3D() const;
    void SetRotation3D(float pitch, float yaw, float roll);
    math::Vec3 GetScale3D() const;
    void SetScale3D(float x, float y, float z);
    math::Vec3 GetGlobalPosition3D() const;
    void SetGlobalPosition3D(float x, float y, float z);
    math::Vec3 GetGlobalRotation3D() const;
    void SetGlobalRotation3D(float pitch, float yaw, float roll);
    math::Vec3 GetGlobalScale3D() const;

    // Distance helper (uses the wrapped node's transform vs another node's)
    float DistanceTo(const NodeRef& other) const;

    // Components
    ComponentRef GetComponent(const std::string& typeName) const;
    std::vector<ComponentRef> GetComponents(const std::string& typeName) const;
    bool HasComponent(const std::string& typeName) const;
    ComponentRef AddComponent(const std::string& typeName);
    void RemoveComponent(const ComponentRef& component);

    // Create and start a Tween on this node that animates a channel to a target
    // value over a duration with an easing curve. See components::Tween for the
    // supported channel names. Returns a handle to the running tween.
    TweenRef CreateTween(const std::string& channel, const nlohmann::json& toValue,
                         float duration, const std::string& easing = "linear") const;

    // Create an (empty, not-yet-playing) tween sequence on this node. Append steps
    // via the returned handle, then call play().
    SequenceRef CreateSequence() const;

    // Property bag: searches the node's components, then node-level properties.
    // Values cross the boundary as JSON.
    bool HasProperty(const std::string& propName) const;
    nlohmann::json Get(const std::string& propName) const;
    void Set(const std::string& propName, const nlohmann::json& value);

    // Invoke a named method on this node by searching its components: the first
    // component that advertises `method` (a script component defining a function
    // of that name, or a native component advertising a CallMethod handler) is
    // the one that runs. This is how a script reaches another script's functions,
    // including an autoload singleton bound as a node handle. Arguments are a JSON
    // array; the JSON result is returned (null when nothing handles the method).
    bool HasMethod(const std::string& method) const;
    nlohmann::json Call(const std::string& method, const nlohmann::json& args) const;

    // Signals (node-level: ready, tree_entered, plus any user signals). Arguments
    // cross the boundary as a JSON array. Connection targets are nodes; the
    // handler method resolves to a function on the target node's script(s).
    void EmitSignal(const std::string& signal, const nlohmann::json& args) const;
    uint64_t ConnectSignal(const std::string& signal, const NodeRef& target,
                           const std::string& method, uint32_t flags = 0) const;
    void DisconnectSignal(const std::string& signal, uint64_t connectionId) const;
    void DisconnectSignalMethod(const std::string& signal, const NodeRef& target,
                                const std::string& method) const;
    bool IsSignalConnected(const std::string& signal) const;
    void AddUserSignal(const std::string& name) const;
    std::vector<std::string> GetSignalList() const;

    // Create a one-shot latch that records when `signal` next fires on this node.
    // Used by the scripting await schedulers (await_signal).
    SignalAwaiter AwaitSignal(const std::string& signal) const;

    // ------------------------------------------------------------------
    // Networking (RPC + multiplayer authority)
    // ------------------------------------------------------------------
    // These require a NetworkObject component on the node. The peer ids are
    // network::PeerId values (uint32). Offline they degrade to a local call /
    // sensible defaults, so rpc-annotated functions still run in single-player.
    // Arguments cross the boundary as a JSON array, like signals.

    // Call `method` per its `--@rpc` config (broadcast to the relevant peers).
    void Rpc(const std::string& method, const nlohmann::json& args) const;
    // Call `method` on exactly one peer.
    void RpcId(uint32_t peerId, const std::string& method, const nlohmann::json& args) const;
    // Call `method` over the unreliable channel regardless of its declared transfer.
    void RpcUnreliable(const std::string& method, const nlohmann::json& args) const;

    // Authority (ownership) of this node's NetworkObject.
    void SetMultiplayerAuthority(uint32_t peerId) const;
    uint32_t GetMultiplayerAuthority() const;
    bool IsMultiplayerAuthority() const;

    // Network-stable id of this node (0 if it has no NetworkObject / unassigned).
    uint32_t GetNetworkId() const;

private:
    std::weak_ptr<core::Node> m_Node;
    core::SceneManager* m_Tree = nullptr;
};

/**
 * SignalAwaiter - one-shot latch for awaiting a signal from scripts.
 *
 * Connects a native one-shot slot to a node/component signal; the latch records
 * when the signal next fires. The scripting await schedulers poll IsFired() to
 * resume a coroutine. Copyable value handle (shares the latch flag).
 */
class SignalAwaiter {
public:
    SignalAwaiter() = default;

    // Connect a one-shot latch to `signal` on `source` (a Node or Component).
    static SignalAwaiter Connect(core::SignalObject* source, const std::string& signal);

    bool IsValid() const { return static_cast<bool>(m_Fired); }
    bool IsFired() const { return m_Fired && *m_Fired; }
    // True while the object the latch is connected to is still alive. A source
    // destroyed before it ever emitted can never fire the latch, so an awaiting
    // scheduler must treat !IsSourceAlive() as "done (cancelled)" or the waiter
    // is stranded forever.
    bool IsSourceAlive() const;
    void Reset() const { if (m_Fired) *m_Fired = false; }
    void Cancel() const;  // disconnect early (if the signal never fired)

private:
    std::shared_ptr<bool> m_Fired;
    std::weak_ptr<core::SignalLifetime> m_SourceLifetime;
    uint64_t m_ConnectionId = 0;
    std::string m_Signal;
};

/**
 * TimerRef - script-facing handle to a Timer component.
 *
 * Wraps a weak reference to a core::Component that is expected to be a
 * components::Timer, letting scripts start/stop/reset/restart the timer, read and
 * write its duration/loop/repeat configuration, query its running/finished state,
 * and remove it from its owner. Returned by the timer-creation and timer-listing
 * script functions. Once the underlying component is destroyed the handle reports
 * IsValid() == false and operations become no-ops.
 */
class TimerRef {
public:
    TimerRef() = default;
    TimerRef(std::weak_ptr<core::Component> timer, ScriptAPI* api);
    TimerRef(std::weak_ptr<core::Component> timer, core::SceneManager* tree)
        : m_Timer(timer), m_Tree(tree) {}

    // Build a handle from a raw Timer component pointer (as returned by
    // ScriptAPI timer methods). Resolves the owning shared_ptr via the component's
    // owner node so the handle holds a proper weak reference. Returns an invalid
    // handle when the component is null or not attached to a node.
    static TimerRef FromComponent(core::Component* timer, ScriptAPI* api);

    bool IsValid() const;
    std::shared_ptr<core::Component> Lock() const { return m_Timer.lock(); }
    core::SceneManager* GetTree() const { return m_Tree; }

    std::string GetName() const;

    // Control
    void Start() const;
    void Stop() const;
    void Reset() const;
    void Restart() const;
    void Remove() const;

    // State queries
    bool IsRunning() const;
    bool IsFinished() const;
    float GetTimeLeft() const;
    int GetFireCount() const;

    // Configuration
    float GetDuration() const;
    void SetDuration(float duration) const;
    float GetElapsed() const;
    void SetElapsed(float elapsed) const;
    bool GetLoop() const;
    void SetLoop(bool loop) const;
    int GetRepeatCount() const;
    void SetRepeatCount(int repeatCount) const;

    // Cross-handle access
    NodeRef GetOwner() const;
    ComponentRef AsComponent() const;

private:
    std::weak_ptr<core::Component> m_Timer;
    core::SceneManager* m_Tree = nullptr;
};

/**
 * TweenRef - script-facing handle to a Tween component.
 *
 * Wraps a weak reference to a core::Component that is expected to be a
 * components::Tween, letting scripts control playback (play/pause/stop/kill),
 * query progress/state, and adjust easing/loop/duration. Returned by the
 * tween-creation script functions and node `create_tween`. Once the underlying
 * component is destroyed the handle reports IsValid() == false.
 */
class TweenRef {
public:
    TweenRef() = default;
    TweenRef(std::weak_ptr<core::Component> tween, ScriptAPI* api);
    TweenRef(std::weak_ptr<core::Component> tween, core::SceneManager* tree)
        : m_Tween(tween), m_Tree(tree) {}

    // Build a handle from a raw Tween component pointer (as returned by the
    // ScriptAPI tween methods), resolving the owning shared_ptr via its node.
    static TweenRef FromComponent(core::Component* tween, ScriptAPI* api);

    bool IsValid() const;
    std::shared_ptr<core::Component> Lock() const { return m_Tween.lock(); }
    core::SceneManager* GetTree() const { return m_Tree; }

    std::string GetName() const;

    // Playback
    void Play() const;     // start / resume
    void Pause() const;    // stop without resetting
    void Stop() const;     // stop and reset to start
    void Restart() const;
    void Kill() const;     // remove the tween component from its owner

    // State
    bool IsRunning() const;
    bool IsFinished() const;
    float GetProgress() const;

    // Configuration
    float GetDuration() const;
    void SetDuration(float duration) const;
    std::string GetEasing() const;
    void SetEasing(const std::string& easing) const;
    bool GetLoop() const;
    void SetLoop(bool loop) const;
    void SetAutoRemove(bool autoRemove) const;

    NodeRef GetOwner() const;
    ComponentRef AsComponent() const;

private:
    std::weak_ptr<core::Component> m_Tween;
    core::SceneManager* m_Tree = nullptr;
};

/**
 * SequenceRef - script-facing handle to a TweenSequence component.
 *
 * A chainable builder: Append* add steps (call before Play); the control methods
 * drive playback. Append/Set methods return the same handle so calls can be
 * chained. Steps target the sequence's owner node unless an explicit target
 * NodeRef is given (the *On variants).
 */
class SequenceRef {
public:
    SequenceRef() = default;
    SequenceRef(std::weak_ptr<core::Component> sequence, ScriptAPI* api);
    SequenceRef(std::weak_ptr<core::Component> sequence, core::SceneManager* tree)
        : m_Sequence(sequence), m_Tree(tree) {}

    static SequenceRef FromComponent(core::Component* sequence, ScriptAPI* api);

    bool IsValid() const;
    std::shared_ptr<core::Component> Lock() const { return m_Sequence.lock(); }
    core::SceneManager* GetTree() const { return m_Tree; }
    std::string GetName() const;

    // Builder (chainable). `parallel` joins the previous step's phase.
    SequenceRef Append(const std::string& channel, const nlohmann::json& toValue,
                       float duration, const std::string& easing = "linear",
                       bool parallel = false) const;
    SequenceRef AppendOn(const NodeRef& target, const std::string& channel, const nlohmann::json& toValue,
                         float duration, const std::string& easing = "linear",
                         bool parallel = false) const;
    SequenceRef AppendInterval(float duration, bool parallel = false) const;
    SequenceRef AppendCallback(const std::string& method, bool parallel = false) const;
    SequenceRef AppendCallbackOn(const NodeRef& target, const std::string& method,
                                 bool parallel = false) const;

    // Control
    SequenceRef Play() const;   // returns self for chaining
    void Stop() const;
    void Reset() const;
    void Restart() const;
    void Kill() const;
    bool IsRunning() const;
    bool IsFinished() const;

    // Config
    SequenceRef SetLoops(int loops) const;   // -1 = infinite; chainable
    int GetLoops() const;
    SequenceRef SetAutoRemove(bool autoRemove) const;  // chainable
    int GetStepCount() const;

    NodeRef GetOwner() const;
    ComponentRef AsComponent() const;

private:
    std::weak_ptr<core::Component> m_Sequence;
    core::SceneManager* m_Tree = nullptr;
};

/**
 * SceneRef - script-facing handle to a single core::Scene.
 *
 * Provides read access to a scene's identity (name/path), its root node, and node
 * lookups scoped to the scene. Scenes are owned by the SceneManager; this handle
 * stores a raw pointer consistent with the engine's raw-Scene* model, so a handle
 * captured before a scene switch must not be reused afterwards. Re-fetch with
 * get_scene() / get_tree():get_current_scene() rather than caching across switches.
 */
class SceneRef {
public:
    SceneRef() = default;
    SceneRef(core::Scene* scene, ScriptAPI* api);
    SceneRef(core::Scene* scene, core::SceneManager* tree)
        : m_Scene(scene), m_Tree(tree) {}

    bool IsValid() const { return m_Scene != nullptr; }
    core::Scene* Get() const { return m_Scene; }
    core::SceneManager* GetTree() const { return m_Tree; }

    // True when `scene` is still one of `tree`'s live scenes (current, root or an
    // autoload overlay). A raw Scene* captured by a script handle must be checked
    // against this before it is dereferenced, because a scene switch frees the
    // previous scene while script-side handles to it may still exist.
    static bool IsLiveScene(core::SceneManager* tree, core::Scene* scene);

    std::string GetName() const;
    std::string GetPath() const;

    NodeRef GetRoot() const;
    NodeRef FindNode(const std::string& path) const;
    NodeRef FindNodeByUUID(const std::string& uuid) const;

private:
    core::Scene* m_Scene = nullptr;
    core::SceneManager* m_Tree = nullptr;
};

/**
 * TreeRef - script-facing handle to the scene tree (core::SceneManager).
 *
 * Godot-style get_tree() object: navigate to the current scene and its root, and
 * drive scene-level operations (change/reload/add/remove scene). The SceneManager
 * is a long-lived singleton, so this handle is safe to hold across frames.
 */
class TreeRef {
public:
    TreeRef() = default;
    // The SceneManager is the long-lived tree itself, so TreeRef needs no api: it
    // hands its own manager to the node/scene handles it produces.
    TreeRef(core::SceneManager* manager, ScriptAPI* /*api*/)
        : m_Manager(manager) {}

    bool IsValid() const { return m_Manager != nullptr; }
    core::SceneManager* Get() const { return m_Manager; }

    NodeRef GetRoot() const;
    SceneRef GetCurrentScene() const;
    std::string GetCurrentScenePath() const;

    void ChangeScene(const std::string& scenePath) const;
    void ReloadScene() const;
    void AddScene(const std::string& scenePath) const;
    void RemoveScene(const std::string& sceneName) const;

private:
    core::SceneManager* m_Manager = nullptr;
};

} // namespace scripting
} // namespace lupine
