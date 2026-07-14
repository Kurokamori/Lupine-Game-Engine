#include "lupine/core/EventBus.hpp"

namespace lupine {
namespace core {

namespace {
/**
 * Concrete SignalObject used internally by the bus. Events are registered as
 * signals on demand; the hub itself is never a connection target, so it does
 * not need to override InvokeSignalMethod.
 */
class EventHub : public SignalObject {
};
}

EventBus::EventBus()
    : m_Hub(std::make_unique<EventHub>()) {
}

EventBus& EventBus::Get() {
    static EventBus instance;
    return instance;
}

void EventBus::EnsureEvent(const std::string& event) {
    if (!m_Hub->HasSignal(event)) {
        m_Hub->AddUserSignal(event);
    }
}

uint64_t EventBus::Subscribe(const std::string& event, SignalObject* target,
                             const std::string& method, uint32_t flags,
                             const SignalArgs& binds) {
    EnsureEvent(event);
    return m_Hub->Connect(event, target, method, flags, binds);
}

uint64_t EventBus::Subscribe(const std::string& event, SignalSlot slot, uint32_t flags) {
    EnsureEvent(event);
    return m_Hub->Connect(event, std::move(slot), flags);
}

void EventBus::Unsubscribe(const std::string& event, uint64_t subscriptionId) {
    m_Hub->Disconnect(event, subscriptionId);
}

void EventBus::UnsubscribeTarget(const std::string& event, SignalObject* target,
                                 const std::string& method) {
    m_Hub->DisconnectTarget(event, target, method);
}

bool EventBus::HasSubscribers(const std::string& event) const {
    return m_Hub->HasConnections(event);
}

void EventBus::Emit(const std::string& event, const SignalArgs& args) {
    EnsureEvent(event);
    m_Hub->Emit(event, args);
}

void EventBus::EmitDeferred(const std::string& event, const SignalArgs& args) {
    EnsureEvent(event);
    m_Hub->EmitDeferred(event, args);
}

void EventBus::Clear() {
    m_Hub = std::make_unique<EventHub>();
}

} // namespace core
} // namespace lupine
