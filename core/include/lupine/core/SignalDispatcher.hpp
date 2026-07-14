#pragma once

#include "lupine/core/SignalObject.hpp"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace lupine {
namespace core {

class Node;

/**
 * SignalDispatcher - process-wide queue for deferred work.
 *
 * Owns three end-of-frame queues that are drained once per frame by
 * SceneManager::Update via Flush():
 *   1. Deferred signal/method invocations (connections made with Connect_Deferred
 *      or emitted via EmitDeferred), and explicit CallDeferred() requests.
 *   2. Deferred native slots.
 *   3. Node free requests (QueueFree) - this is what finally makes
 *      ScriptAPI::QueueFree real instead of a no-op.
 *
 * The dispatcher lives in core and has no dependency on SceneManager. Node
 * teardown semantics (OnDestroy / OnExitTree) are delegated to an optional free
 * handler that SceneManager installs; without a handler the node is simply
 * detached from its parent (sufficient for tests and the editor).
 *
 * Not thread-safe: queue and flush happen on the main game thread.
 */
class SignalDispatcher {
public:
    static SignalDispatcher& Get();

    // Queue an object+method invocation for the next flush.
    void QueueDeferredCall(const std::weak_ptr<SignalLifetime>& target,
                           const std::string& method, const SignalArgs& args);

    // Queue a native slot invocation for the next flush.
    void QueueDeferredSlot(const SignalSlot& slot, const SignalArgs& args);

    // Convenience: queue a method call on a live SignalObject for the next flush.
    void CallDeferred(SignalObject* target, const std::string& method,
                      const SignalArgs& args = {});

    // Queue a node for safe deletion at the next flush. The node is kept alive
    // (shared_ptr) until then. Duplicate requests for the same node are ignored.
    void QueueFree(Node* node);
    bool IsQueuedForFree(Node* node) const;

    // Queue a node for deletion, but defer the QueueFree request itself onto the
    // deferred-call phase of the next flush. Because Flush() drains deferred calls
    // before frees (and re-runs both queues until empty), the node is still freed
    // within the same flush, but the request is recorded only after any in-flight
    // signal/physics callback has finished - the safe-from-anywhere variant.
    void QueueFreeDeferred(Node* node);

    // Free a node immediately: run the lifecycle teardown (OnDestroy on the
    // subtree via the free handler), detach from the parent and release the
    // owning shared_ptr now. Use with caution - prefer QueueFree from inside
    // callbacks. If the node was already queued for a deferred free, that pending
    // request is dropped to avoid a double free.
    void Free(Node* node);

    // Installed by SceneManager to perform proper lifecycle teardown. When unset,
    // QueueFree falls back to detaching the node from its parent.
    using FreeHandler = std::function<void(const std::shared_ptr<Node>&)>;
    void SetFreeHandler(FreeHandler handler) { m_FreeHandler = std::move(handler); }

    // Drain all queues. Deferred calls run first (in enqueue order), then frees.
    // Calls enqueued during the flush are processed in the same flush until the
    // queues are empty, bounded by a safety iteration cap.
    void Flush();

    // Drop every pending item without running it (scene switch / shutdown).
    void Clear();

private:
    SignalDispatcher() = default;

    struct DeferredItem {
        bool isSlot = false;
        SignalSlot slot;
        std::weak_ptr<SignalLifetime> targetLifetime;
        std::string method;
        SignalArgs args;
    };

    std::vector<DeferredItem> m_Deferred;
    std::vector<std::shared_ptr<Node>> m_FreeQueue;
    FreeHandler m_FreeHandler;
    bool m_Flushing = false;
};

} // namespace core
} // namespace lupine
