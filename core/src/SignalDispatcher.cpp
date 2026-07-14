#include "lupine/core/SignalDispatcher.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/logger/Logger.hpp"
#include <algorithm>

namespace lupine {
namespace core {

namespace {
// Upper bound on flush passes to guarantee termination if deferred work keeps
// queueing more deferred work (a programming error, not normal operation).
constexpr int kMaxFlushPasses = 64;
}

SignalDispatcher& SignalDispatcher::Get() {
    static SignalDispatcher instance;
    return instance;
}

void SignalDispatcher::QueueDeferredCall(const std::weak_ptr<SignalLifetime>& target,
                                         const std::string& method, const SignalArgs& args) {
    DeferredItem item;
    item.isSlot = false;
    item.targetLifetime = target;
    item.method = method;
    item.args = args;
    m_Deferred.push_back(std::move(item));
}

void SignalDispatcher::QueueDeferredSlot(const SignalSlot& slot, const SignalArgs& args) {
    if (!slot) {
        return;
    }
    DeferredItem item;
    item.isSlot = true;
    item.slot = slot;
    item.args = args;
    m_Deferred.push_back(std::move(item));
}

void SignalDispatcher::CallDeferred(SignalObject* target, const std::string& method,
                                    const SignalArgs& args) {
    if (!target || method.empty()) {
        return;
    }
    QueueDeferredCall(target->GetSignalLifetime(), method, args);
}

void SignalDispatcher::QueueFree(Node* node) {
    if (!node || IsQueuedForFree(node)) {
        return;
    }
    std::shared_ptr<Node> shared;
    try {
        shared = node->shared_from_this();
    } catch (const std::bad_weak_ptr&) {
        // Node is not managed by a shared_ptr (e.g. stack/raw allocation). Fall
        // back to an immediate detach; we cannot safely defer ownership.
        if (node->GetParent()) {
            LOG_WARN(LogCategory::Core,
                     "QueueFree on a non-shared node '{}'; detaching immediately", node->GetName());
        }
        return;
    }
    m_FreeQueue.push_back(std::move(shared));
}

bool SignalDispatcher::IsQueuedForFree(Node* node) const {
    return std::any_of(m_FreeQueue.begin(), m_FreeQueue.end(),
                       [node](const std::shared_ptr<Node>& n) { return n.get() == node; });
}

void SignalDispatcher::QueueFreeDeferred(Node* node) {
    if (!node) {
        return;
    }
    std::weak_ptr<Node> weak = node->weak_from_this();
    QueueDeferredSlot([this, weak](const SignalArgs&) {
        if (std::shared_ptr<Node> alive = weak.lock()) {
            QueueFree(alive.get());
        }
    }, {});
}

void SignalDispatcher::Free(Node* node) {
    if (!node) {
        return;
    }
    std::shared_ptr<Node> shared;
    try {
        shared = node->shared_from_this();
    } catch (const std::bad_weak_ptr&) {
        // Node is not managed by a shared_ptr (e.g. stack/raw allocation); we
        // cannot safely take ownership to free it. Detach by name if possible.
        if (node->GetParent()) {
            LOG_WARN(LogCategory::Core,
                     "Free on a non-shared node '{}'; cannot release ownership", node->GetName());
        }
        return;
    }

    // Drop any pending deferred free for this node so Flush() does not free it
    // a second time after we destroy it here.
    m_FreeQueue.erase(std::remove(m_FreeQueue.begin(), m_FreeQueue.end(), shared),
                      m_FreeQueue.end());

    if (m_FreeHandler) {
        m_FreeHandler(shared);
    } else if (node->GetParent()) {
        node->GetParent()->RemoveChild(shared);
    }
    // Releasing the local shared_ptr here drops the last reference once the
    // parent has detached the node, destroying it immediately.
}

void SignalDispatcher::Flush() {
    if (m_Flushing) {
        return;  // Guard against reentrant flushes triggered from within a slot.
    }
    m_Flushing = true;

    int pass = 0;
    while ((!m_Deferred.empty() || !m_FreeQueue.empty()) && pass < kMaxFlushPasses) {
        ++pass;

        if (!m_Deferred.empty()) {
            std::vector<DeferredItem> batch;
            batch.swap(m_Deferred);
            for (const DeferredItem& item : batch) {
                if (item.isSlot) {
                    if (item.slot) {
                        item.slot(item.args);
                    }
                } else {
                    std::shared_ptr<SignalLifetime> alive = item.targetLifetime.lock();
                    if (alive && alive->obj) {
                        alive->obj->InvokeSignalMethod(item.method, item.args);
                    }
                }
            }
        }

        if (!m_FreeQueue.empty()) {
            std::vector<std::shared_ptr<Node>> batch;
            batch.swap(m_FreeQueue);
            for (const std::shared_ptr<Node>& node : batch) {
                if (!node) {
                    continue;
                }
                if (m_FreeHandler) {
                    m_FreeHandler(node);
                } else if (node->GetParent()) {
                    node->GetParent()->RemoveChild(node);
                }
                // Releasing the last shared_ptr here destroys the node, which
                // disconnects its signals and invalidates its lifetime token.
            }
        }
    }

    if (pass >= kMaxFlushPasses) {
        LOG_WARN(LogCategory::Core,
                 "SignalDispatcher::Flush hit max passes; deferred work may be self-perpetuating");
    }

    m_Flushing = false;
}

void SignalDispatcher::Clear() {
    m_Deferred.clear();
    m_FreeQueue.clear();
}

} // namespace core
} // namespace lupine
