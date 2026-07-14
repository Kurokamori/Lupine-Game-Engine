#pragma once

#include "lupine/core/SignalObject.hpp"
#include <memory>
#include <string>

namespace lupine {
namespace core {

/**
 * EventBus - global, decoupled publish/subscribe hub.
 *
 * Provides Unity-style global messaging / Godot-autoload-signal semantics: any
 * system can emit a named event and any number of listeners can subscribe,
 * without holding references to one another. Event names are created on demand
 * the first time they are subscribed to or emitted.
 *
 * Internally backed by a single SignalObject, so subscriptions reuse the exact
 * same lifetime-safe, deferred, and one-shot machinery as per-object signals.
 *
 * The bus is cleared on scene switches (SceneManager::UnloadCurrentScene) so
 * connections into a dead scene do not leak across scene boundaries.
 */
class EventBus {
public:
    static EventBus& Get();

    // Subscribe an object+method handler. Returns a subscription id (0 on failure).
    uint64_t Subscribe(const std::string& event, SignalObject* target,
                       const std::string& method, uint32_t flags = Connect_None,
                       const SignalArgs& binds = {});

    // Subscribe a native C++ slot. Returns a subscription id.
    uint64_t Subscribe(const std::string& event, SignalSlot slot,
                       uint32_t flags = Connect_None);

    // Remove a subscription by id.
    void Unsubscribe(const std::string& event, uint64_t subscriptionId);

    // Remove every subscription on an event that targets (target, method).
    void UnsubscribeTarget(const std::string& event, SignalObject* target,
                           const std::string& method);

    bool HasSubscribers(const std::string& event) const;

    // Emit an event to all subscribers.
    void Emit(const std::string& event, const SignalArgs& args = {});

    // Emit an event such that every subscriber is invoked at the next frame flush.
    void EmitDeferred(const std::string& event, const SignalArgs& args = {});

    // Drop all events and subscriptions.
    void Clear();

private:
    EventBus();

    // Ensures an event name is registered as a signal before use.
    void EnsureEvent(const std::string& event);

    std::unique_ptr<SignalObject> m_Hub;
};

} // namespace core
} // namespace lupine
