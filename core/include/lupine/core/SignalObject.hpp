#pragma once

#include "lupine/core/PropertyDescriptor.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace lupine {
namespace core {

class SignalObject;

/**
 * Connection flags controlling how a connected slot is invoked.
 * These mirror Godot's CONNECT_DEFERRED / CONNECT_ONE_SHOT semantics.
 */
enum ConnectFlags : uint32_t {
    Connect_None     = 0,
    Connect_Deferred = 1u << 0,  // Invoke the slot at the end of the frame instead of immediately
    Connect_OneShot  = 1u << 1   // Automatically disconnect the slot after it is invoked once
};

/**
 * Signal argument payload.
 *
 * Arguments cross every boundary (C++, Lua, MRuby, MicroPython, C API, editor,
 * serialization) as JSON, which is already the engine's universal scripting
 * currency (see LuaEnvironment::CallMethod). Each connection target converts
 * the JSON values to its own native representation exactly once.
 */
using SignalArgs = std::vector<nlohmann::json>;

/**
 * Native C++ slot signature. Receives the fully-resolved argument list
 * (emitted arguments followed by any bound arguments).
 */
using SignalSlot = std::function<void(const SignalArgs&)>;

/**
 * Lifetime token for safe weak targeting.
 *
 * SignalObject cannot inherit std::enable_shared_from_this because Node already
 * does (for std::shared_ptr<Node>), and a second enable_shared_from_this base is
 * ill-formed. Instead every SignalObject owns a shared_ptr<SignalLifetime> whose
 * back-pointer is cleared in the destructor. Connections hold a
 * weak_ptr<SignalLifetime>; locking it proves the target is still alive before
 * the raw pointer is dereferenced. This is destruction-safe in both directions
 * (emitter or target may die first) and never extends an object's lifetime.
 */
struct SignalLifetime {
    SignalObject* obj = nullptr;
};

/**
 * Metadata describing a single signal argument (for editor/reflection).
 */
struct SignalArgDesc {
    std::string name;
    PropertyValueType type = PropertyValueType::Float;
};

/**
 * Metadata describing a declared signal (for editor/reflection).
 */
struct SignalDesc {
    std::string name;
    std::vector<SignalArgDesc> args;
    std::string doc;  // Optional human-readable description
};

/**
 * Parsed, not-yet-resolved declarative connection loaded from a scene file.
 * The target node is referenced by UUID and/or path and resolved after the
 * whole tree has been built (see Node::ResolvePendingConnections).
 */
struct PendingConnection {
    std::string signal;
    std::string targetUuid;   // Preferred: stable across renames/moves
    std::string targetPath;   // Fallback / human-readable
    std::string method;       // Handler function/method name on the target
    uint32_t    flags = Connect_None;
    SignalArgs  binds;        // Extra trailing arguments appended on every emit
};

/**
 * SignalObject - signal/event capability mixin.
 *
 * Inherited by both Node and Component so any object in the engine can declare
 * named signals, connect listeners (native C++ slots OR object+method targets
 * that resolve to scripts), and emit. Provides multicast dispatch, deferred and
 * one-shot connections, bound arguments, lifetime-safe weak targeting, and
 * serialization of declarative connections.
 *
 * Not thread-safe: all emit/connect/disconnect calls are expected on the main
 * game thread, matching the rest of the node/component lifecycle.
 */
class SignalObject {
public:
    SignalObject();
    virtual ~SignalObject();

    SignalObject(const SignalObject&) = delete;
    SignalObject& operator=(const SignalObject&) = delete;

    // ------------------------------------------------------------------
    // Signal declaration / reflection
    // ------------------------------------------------------------------

    /**
     * Override in derived classes to declare built-in signals. Called once by
     * EnsureSignalsDefined() the first time the object's signals are touched.
     */
    virtual void DefineSignals() {}

    // Declare a signal with full metadata (idempotent: re-declaring updates it).
    void RegisterSignal(const SignalDesc& desc);

    // Declare a signal at runtime/from script. Argument descriptors optional.
    void AddUserSignal(const std::string& name, const std::vector<SignalArgDesc>& args = {});

    // True when this object itself declares the signal. Never delegates — this is
    // the primitive ResolveSignalOwner() is built on.
    bool HasOwnSignal(const std::string& name) const;

    // True when this object, or the object it delegates the signal to, declares it.
    bool HasSignal(const std::string& name) const;
    std::vector<SignalDesc> GetSignalDescriptors() const;

    /**
     * Which object actually owns `signal`.
     *
     * Node and Component are separate SignalObjects, so a built-in component's
     * signal (Button `pressed`, area `body_entered`) lives on the component, not
     * on the node that carries it. Connecting on the node would otherwise create a
     * second, never-emitted signal of the same name and silently do nothing.
     *
     * Node overrides this to forward any signal it does not declare itself to the
     * first component that does, so `node:connect("pressed", ...)` and the
     * component's own `Emit("pressed")` meet on the same object. Every connect,
     * disconnect, query and emit path routes through here, so both sides always
     * agree on where a signal lives.
     *
     * Returning `this` (the default, and what Component does) terminates the
     * delegation, so resolution is at most one hop and cannot cycle.
     */
    virtual SignalObject* ResolveSignalOwner(const std::string& signal) {
        (void)signal;
        return this;
    }

    // ------------------------------------------------------------------
    // Connection management
    // ------------------------------------------------------------------

    // Connect a native C++ slot. Returns a connection id (0 on failure).
    uint64_t Connect(const std::string& signal, SignalSlot slot, uint32_t flags = Connect_None);

    // Connect an object+method target. The target's InvokeSignalMethod() is
    // called with the emitted arguments (plus binds). Returns a connection id.
    uint64_t Connect(const std::string& signal, SignalObject* target,
                     const std::string& method, uint32_t flags = Connect_None,
                     const SignalArgs& binds = {});

    // Disconnect a single connection by id.
    void Disconnect(const std::string& signal, uint64_t connectionId);

    // Disconnect every connection on a signal that targets (target, method).
    void DisconnectTarget(const std::string& signal, SignalObject* target,
                          const std::string& method);

    // Disconnect everything (used during teardown).
    void DisconnectAll();

    bool IsConnected(const std::string& signal) const;
    bool IsConnectedTo(const std::string& signal, SignalObject* target,
                       const std::string& method) const;
    size_t GetConnectionCount(const std::string& signal) const;

    // Fast guard for hot paths: true only if the signal has >=1 live connection.
    // Lets callers skip building a JSON payload when nobody is listening.
    bool HasConnections(const std::string& signal) const;

    // ------------------------------------------------------------------
    // Emission
    // ------------------------------------------------------------------

    // Emit immediately (deferred-flagged connections are still queued).
    void Emit(const std::string& signal, const SignalArgs& args = {});

    // Emit such that every connection is queued to the end-of-frame flush.
    void EmitDeferred(const std::string& signal, const SignalArgs& args = {});

    // ------------------------------------------------------------------
    // Dispatch target
    // ------------------------------------------------------------------

    /**
     * Invoke a named method on this object with the given arguments. Overridden
     * by Node (forwards to attached script components) and ScriptComponent
     * (calls the script function). Returns true if a method was actually called.
     */
    virtual bool InvokeSignalMethod(const std::string& method, const SignalArgs& args) {
        (void)method; (void)args; return false;
    }

    // Access this object's lifetime token (for connections / deferred dispatch).
    const std::shared_ptr<SignalLifetime>& GetSignalLifetime() const { return m_Lifetime; }

    // ------------------------------------------------------------------
    // Serialization of declarative (object+method) connections
    // ------------------------------------------------------------------

    /**
     * Serialize object+method connections originating from this object.
     * Native-slot connections are established in code and are never serialized.
     * The caller supplies a resolver that maps a target SignalObject to its
     * owning node's UUID/path (Node provides this; see Node::Serialize).
     */
    struct TargetLocator {
        std::string uuid;
        std::string path;
    };
    using TargetResolveFn = std::function<TargetLocator(SignalObject*)>;
    nlohmann::json SerializeConnections(const TargetResolveFn& resolve) const;

    // Store raw connection JSON for later resolution (parsed into PendingConnection).
    void LoadConnectionData(const nlohmann::json& connectionsJson);
    const std::vector<PendingConnection>& GetPendingConnections() const { return m_PendingConnections; }
    void ClearPendingConnections() { m_PendingConnections.clear(); }

    // Rewrite the target UUIDs of any not-yet-resolved connections through the
    // given old->new map. Used when a subtree's UUIDs are regenerated (e.g. prefab
    // instantiation) so UUID-based connections that point inside the same subtree
    // survive the renumbering; targets not present in the map are left untouched.
    void RemapPendingConnectionTargets(const std::unordered_map<std::string, std::string>& uuidRemap);

protected:
    // Lazily runs DefineSignals() exactly once.
    void EnsureSignalsDefined();

private:
    struct Connection {
        uint64_t id = 0;
        SignalSlot slot;                              // Non-empty => native slot
        std::weak_ptr<SignalLifetime> targetLifetime; // Valid => object+method
        SignalObject* targetRaw = nullptr;
        std::string method;
        uint32_t flags = Connect_None;
        SignalArgs binds;

        bool IsObjectTarget() const { return !slot; }
    };

    struct SignalEntry {
        SignalDesc desc;
        std::vector<Connection> connections;
    };

    SignalEntry* FindSignal(const std::string& name);
    const SignalEntry* FindSignal(const std::string& name) const;
    SignalEntry& GetOrCreateSignal(const std::string& name);

    void DoEmit(const std::string& signal, const SignalArgs& args, bool forceDeferred);
    static SignalArgs CombineArgs(const SignalArgs& args, const SignalArgs& binds);

    std::shared_ptr<SignalLifetime> m_Lifetime;
    std::unordered_map<std::string, SignalEntry> m_Signals;
    std::vector<PendingConnection> m_PendingConnections;
    uint64_t m_NextConnectionId = 1;
    bool m_SignalsDefined = false;
};

} // namespace core
} // namespace lupine
