#include "lupine/core/SignalObject.hpp"
#include "lupine/core/SignalDispatcher.hpp"
#include <algorithm>

namespace lupine {
namespace core {

SignalObject::SignalObject()
    : m_Lifetime(std::make_shared<SignalLifetime>()) {
    m_Lifetime->obj = this;
}

SignalObject::~SignalObject() {
    // Drop our own outgoing connections and invalidate the lifetime token so any
    // emitter still holding a weak reference to us treats this object as dead.
    m_Signals.clear();
    if (m_Lifetime) {
        m_Lifetime->obj = nullptr;
    }
}

void SignalObject::EnsureSignalsDefined() {
    if (m_SignalsDefined) {
        return;
    }
    // Set the flag before invoking DefineSignals() so RegisterSignal() calls made
    // from within the override do not recurse back into EnsureSignalsDefined().
    m_SignalsDefined = true;
    DefineSignals();
}

SignalObject::SignalEntry* SignalObject::FindSignal(const std::string& name) {
    auto it = m_Signals.find(name);
    return it != m_Signals.end() ? &it->second : nullptr;
}

const SignalObject::SignalEntry* SignalObject::FindSignal(const std::string& name) const {
    auto it = m_Signals.find(name);
    return it != m_Signals.end() ? &it->second : nullptr;
}

SignalObject::SignalEntry& SignalObject::GetOrCreateSignal(const std::string& name) {
    auto it = m_Signals.find(name);
    if (it != m_Signals.end()) {
        return it->second;
    }
    SignalEntry& entry = m_Signals[name];
    entry.desc.name = name;
    return entry;
}

void SignalObject::RegisterSignal(const SignalDesc& desc) {
    SignalEntry& entry = GetOrCreateSignal(desc.name);
    // Preserve existing connections while updating the declared metadata.
    entry.desc = desc;
}

void SignalObject::AddUserSignal(const std::string& name, const std::vector<SignalArgDesc>& args) {
    EnsureSignalsDefined();
    SignalDesc desc;
    desc.name = name;
    desc.args = args;
    RegisterSignal(desc);
}

bool SignalObject::HasOwnSignal(const std::string& name) const {
    const_cast<SignalObject*>(this)->EnsureSignalsDefined();
    return FindSignal(name) != nullptr;
}

bool SignalObject::HasSignal(const std::string& name) const {
    if (HasOwnSignal(name)) {
        return true;
    }
    SignalObject* owner = const_cast<SignalObject*>(this)->ResolveSignalOwner(name);
    return owner != this && owner->HasOwnSignal(name);
}

std::vector<SignalDesc> SignalObject::GetSignalDescriptors() const {
    const_cast<SignalObject*>(this)->EnsureSignalsDefined();
    std::vector<SignalDesc> result;
    result.reserve(m_Signals.size());
    for (const auto& pair : m_Signals) {
        result.push_back(pair.second.desc);
    }
    return result;
}

uint64_t SignalObject::Connect(const std::string& signal, SignalSlot slot, uint32_t flags) {
    if (!slot) {
        return 0;
    }
    SignalObject* owner = ResolveSignalOwner(signal);
    if (owner != this) {
        return owner->Connect(signal, std::move(slot), flags);
    }
    EnsureSignalsDefined();
    SignalEntry& entry = GetOrCreateSignal(signal);

    Connection conn;
    conn.id = m_NextConnectionId++;
    conn.slot = std::move(slot);
    conn.flags = flags;
    entry.connections.push_back(std::move(conn));
    return entry.connections.back().id;
}

uint64_t SignalObject::Connect(const std::string& signal, SignalObject* target,
                               const std::string& method, uint32_t flags,
                               const SignalArgs& binds) {
    if (!target || method.empty()) {
        return 0;
    }
    SignalObject* owner = ResolveSignalOwner(signal);
    if (owner != this) {
        return owner->Connect(signal, target, method, flags, binds);
    }
    EnsureSignalsDefined();
    SignalEntry& entry = GetOrCreateSignal(signal);

    Connection conn;
    conn.id = m_NextConnectionId++;
    conn.targetLifetime = target->GetSignalLifetime();
    conn.targetRaw = target;
    conn.method = method;
    conn.flags = flags;
    conn.binds = binds;
    entry.connections.push_back(std::move(conn));
    return entry.connections.back().id;
}

void SignalObject::Disconnect(const std::string& signal, uint64_t connectionId) {
    SignalObject* owner = ResolveSignalOwner(signal);
    if (owner != this) {
        owner->Disconnect(signal, connectionId);
        return;
    }
    SignalEntry* entry = FindSignal(signal);
    if (!entry) {
        return;
    }
    auto& conns = entry->connections;
    conns.erase(std::remove_if(conns.begin(), conns.end(),
                               [connectionId](const Connection& c) { return c.id == connectionId; }),
                conns.end());
}

void SignalObject::DisconnectTarget(const std::string& signal, SignalObject* target,
                                    const std::string& method) {
    SignalObject* owner = ResolveSignalOwner(signal);
    if (owner != this) {
        owner->DisconnectTarget(signal, target, method);
        return;
    }
    SignalEntry* entry = FindSignal(signal);
    if (!entry || !target) {
        return;
    }
    auto& conns = entry->connections;
    conns.erase(std::remove_if(conns.begin(), conns.end(),
                               [target, &method](const Connection& c) {
                                   return c.IsObjectTarget() && c.targetRaw == target &&
                                          c.method == method;
                               }),
                conns.end());
}

void SignalObject::DisconnectAll() {
    for (auto& pair : m_Signals) {
        pair.second.connections.clear();
    }
}

bool SignalObject::IsConnected(const std::string& signal) const {
    return GetConnectionCount(signal) > 0;
}

bool SignalObject::IsConnectedTo(const std::string& signal, SignalObject* target,
                                 const std::string& method) const {
    SignalObject* owner = const_cast<SignalObject*>(this)->ResolveSignalOwner(signal);
    if (owner != this) {
        return owner->IsConnectedTo(signal, target, method);
    }
    const SignalEntry* entry = FindSignal(signal);
    if (!entry || !target) {
        return false;
    }
    for (const Connection& c : entry->connections) {
        if (c.IsObjectTarget() && c.targetRaw == target && c.method == method &&
            !c.targetLifetime.expired()) {
            return true;
        }
    }
    return false;
}

size_t SignalObject::GetConnectionCount(const std::string& signal) const {
    SignalObject* owner = const_cast<SignalObject*>(this)->ResolveSignalOwner(signal);
    if (owner != this) {
        return owner->GetConnectionCount(signal);
    }
    const SignalEntry* entry = FindSignal(signal);
    if (!entry) {
        return 0;
    }
    size_t count = 0;
    for (const Connection& c : entry->connections) {
        if (c.IsObjectTarget()) {
            if (!c.targetLifetime.expired()) {
                ++count;
            }
        } else {
            ++count;
        }
    }
    return count;
}

bool SignalObject::HasConnections(const std::string& signal) const {
    return GetConnectionCount(signal) > 0;
}

SignalArgs SignalObject::CombineArgs(const SignalArgs& args, const SignalArgs& binds) {
    if (binds.empty()) {
        return args;
    }
    SignalArgs combined;
    combined.reserve(args.size() + binds.size());
    combined.insert(combined.end(), args.begin(), args.end());
    combined.insert(combined.end(), binds.begin(), binds.end());
    return combined;
}

void SignalObject::Emit(const std::string& signal, const SignalArgs& args) {
    DoEmit(signal, args, /*forceDeferred=*/false);
}

void SignalObject::EmitDeferred(const std::string& signal, const SignalArgs& args) {
    DoEmit(signal, args, /*forceDeferred=*/true);
}

void SignalObject::DoEmit(const std::string& signal, const SignalArgs& args, bool forceDeferred) {
    // Emit resolves the same way connect does, so an emit on a node reaches the
    // listeners that connected to the component's signal through that node.
    SignalObject* owner = ResolveSignalOwner(signal);
    if (owner != this) {
        owner->DoEmit(signal, args, forceDeferred);
        return;
    }
    EnsureSignalsDefined();
    SignalEntry* entry = FindSignal(signal);
    if (!entry || entry->connections.empty()) {
        return;
    }

    // Snapshot the connection list: a slot may connect/disconnect/emit on this
    // same object while we iterate. Snapshotting matches Godot's semantics
    // (connections made during emission are not invoked by the current emission).
    std::vector<Connection> snapshot = entry->connections;

    std::vector<uint64_t> idsToRemove;  // one-shot + expired object targets

    // Guard against a handler synchronously destroying this emitter mid-emit:
    // hold our lifetime token so we can detect it before touching `this` again.
    std::shared_ptr<SignalLifetime> selfToken = m_Lifetime;

    SignalDispatcher& dispatcher = SignalDispatcher::Get();

    for (const Connection& conn : snapshot) {
        // An earlier handler in this same emission may have disconnected this connection
        // (typically by tearing down the object whose lambda captured `this`). Disconnect
        // only edits the live list, not this snapshot, so re-check that the connection is
        // still registered before invoking it. Object targets are additionally guarded by
        // targetLifetime below; plain slots have no such guard and would otherwise run
        // against freed captures.
        if (!selfToken->obj) {
            return;
        }
        SignalEntry* liveEntry = FindSignal(signal);
        if (!liveEntry) {
            return;
        }
        const bool stillConnected = std::any_of(
            liveEntry->connections.begin(), liveEntry->connections.end(),
            [&conn](const Connection& live) { return live.id == conn.id; });
        if (!stillConnected) {
            continue;
        }

        const SignalArgs finalArgs = CombineArgs(args, conn.binds);
        const bool deferred = forceDeferred || (conn.flags & Connect_Deferred) != 0;

        if (conn.IsObjectTarget()) {
            if (conn.targetLifetime.expired()) {
                idsToRemove.push_back(conn.id);
                continue;
            }
            if (deferred) {
                dispatcher.QueueDeferredCall(conn.targetLifetime, conn.method, finalArgs);
            } else {
                std::shared_ptr<SignalLifetime> alive = conn.targetLifetime.lock();
                if (alive && alive->obj) {
                    alive->obj->InvokeSignalMethod(conn.method, finalArgs);
                } else {
                    idsToRemove.push_back(conn.id);
                    continue;
                }
            }
        } else {
            if (deferred) {
                dispatcher.QueueDeferredSlot(conn.slot, finalArgs);
            } else {
                conn.slot(finalArgs);
            }
        }

        if (conn.flags & Connect_OneShot) {
            idsToRemove.push_back(conn.id);
        }
    }

    // If a handler destroyed this emitter, the token's back-pointer is cleared;
    // do not touch `this` (its members are gone).
    if (!selfToken->obj) {
        return;
    }

    // Re-find the live entry: a reentrant Connect() to a new signal name could
    // have rehashed the map and invalidated the earlier pointer.
    if (!idsToRemove.empty()) {
        SignalEntry* live = FindSignal(signal);
        if (live) {
            auto& conns = live->connections;
            conns.erase(std::remove_if(conns.begin(), conns.end(),
                                       [&idsToRemove](const Connection& c) {
                                           return std::find(idsToRemove.begin(), idsToRemove.end(),
                                                            c.id) != idsToRemove.end();
                                       }),
                        conns.end());
        }
    }
}

nlohmann::json SignalObject::SerializeConnections(const TargetResolveFn& resolve) const {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& pair : m_Signals) {
        const SignalEntry& entry = pair.second;
        for (const Connection& conn : entry.connections) {
            if (!conn.IsObjectTarget()) {
                continue;  // Native slots are code-established, never serialized.
            }
            std::shared_ptr<SignalLifetime> alive = conn.targetLifetime.lock();
            if (!alive || !alive->obj) {
                continue;
            }
            TargetLocator loc = resolve ? resolve(alive->obj) : TargetLocator{};
            if (loc.uuid.empty() && loc.path.empty()) {
                continue;  // Target is not addressable within the saved tree.
            }

            nlohmann::json c;
            c["signal"] = entry.desc.name;
            c["target_uuid"] = loc.uuid;
            c["target_path"] = loc.path;
            c["method"] = conn.method;
            c["flags"] = conn.flags;
            c["binds"] = conn.binds;
            arr.push_back(std::move(c));
        }
    }
    return arr;
}

void SignalObject::LoadConnectionData(const nlohmann::json& connectionsJson) {
    if (!connectionsJson.is_array()) {
        return;
    }
    for (const nlohmann::json& c : connectionsJson) {
        if (!c.is_object() || !c.contains("signal") || !c.contains("method")) {
            continue;
        }
        PendingConnection pending;
        pending.signal = c.value("signal", std::string());
        pending.targetUuid = c.value("target_uuid", std::string());
        pending.targetPath = c.value("target_path", std::string());
        pending.method = c.value("method", std::string());
        pending.flags = c.value("flags", static_cast<uint32_t>(Connect_None));
        if (c.contains("binds") && c["binds"].is_array()) {
            for (const nlohmann::json& bind : c["binds"]) {
                pending.binds.push_back(bind);
            }
        }
        m_PendingConnections.push_back(std::move(pending));
    }
}

void SignalObject::RemapPendingConnectionTargets(const std::unordered_map<std::string, std::string>& uuidRemap) {
    if (uuidRemap.empty()) {
        return;
    }
    for (PendingConnection& pc : m_PendingConnections) {
        if (pc.targetUuid.empty()) {
            continue;
        }
        std::unordered_map<std::string, std::string>::const_iterator it = uuidRemap.find(pc.targetUuid);
        if (it != uuidRemap.end()) {
            pc.targetUuid = it->second;
        }
    }
}

} // namespace core
} // namespace lupine
