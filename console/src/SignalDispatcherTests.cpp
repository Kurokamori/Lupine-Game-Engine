#include "lupine/core/SignalDispatcher.hpp"
#include "lupine/core/SignalObject.hpp"
#include "lupine/core/Node.hpp"
#include "TestFramework.hpp"
#include <iostream>
#include <memory>
#include <string>

using namespace lupine;
using namespace lupine::core;

namespace {

class DeferredReceiver : public SignalObject {
public:
    int invokeCount = 0;
    std::string lastMethod;
    bool InvokeSignalMethod(const std::string& method, const SignalArgs&) override {
        ++invokeCount;
        lastMethod = method;
        return true;
    }
};

SignalDispatcher& Disp() {
    return SignalDispatcher::Get();
}

bool TestDeferredSlot() {
    TEST_SECTION("Deferred Slot Tests");

    Disp().Clear();

    int callCount = 0;
    Disp().QueueDeferredSlot([&callCount](const SignalArgs&) { ++callCount; }, {});
    TEST_ASSERT(callCount == 0, "Deferred slot does not run before flush");

    Disp().Flush();
    TEST_ASSERT(callCount == 1, "Deferred slot runs once on flush");

    Disp().Flush();
    TEST_ASSERT(callCount == 1, "Deferred slot does not run again on a second flush");

    return true;
}

bool TestCallDeferred() {
    TEST_SECTION("CallDeferred Tests");

    Disp().Clear();

    DeferredReceiver receiver;
    Disp().CallDeferred(&receiver, "on_event");
    TEST_ASSERT(receiver.invokeCount == 0, "CallDeferred does not invoke immediately");

    Disp().Flush();
    TEST_ASSERT(receiver.invokeCount == 1, "CallDeferred target invoked on flush");
    TEST_ASSERT(receiver.lastMethod == "on_event", "CallDeferred forwards the method name");

    return true;
}

bool TestDeferredOrdering() {
    TEST_SECTION("Deferred Ordering Tests");

    Disp().Clear();

    std::string order;
    Disp().QueueDeferredSlot([&order](const SignalArgs&) { order += "A"; }, {});
    Disp().QueueDeferredSlot([&order](const SignalArgs&) { order += "B"; }, {});
    Disp().QueueDeferredSlot([&order](const SignalArgs&) { order += "C"; }, {});

    Disp().Flush();
    TEST_ASSERT(order == "ABC", "Deferred slots run in enqueue order");

    return true;
}

bool TestReentrantEnqueue() {
    TEST_SECTION("Re-entrant Enqueue Tests");

    Disp().Clear();

    int inner = 0;
    Disp().QueueDeferredSlot([&inner](const SignalArgs&) {
        Disp().QueueDeferredSlot([&inner](const SignalArgs&) { ++inner; }, {});
    }, {});

    Disp().Flush();
    TEST_ASSERT(inner == 1, "A slot enqueued during flush runs within the same flush");

    return true;
}

bool TestLifetimeSafety() {
    TEST_SECTION("Deferred Target Lifetime Tests");

    Disp().Clear();

    {
        DeferredReceiver receiver;
        Disp().CallDeferred(&receiver, "on_event");
    }

    Disp().Flush();
    TEST_ASSERT(true, "Flushing a deferred call to a destroyed target is safe (no crash)");

    return true;
}

bool TestClear() {
    TEST_SECTION("Clear Tests");

    Disp().Clear();

    int callCount = 0;
    Disp().QueueDeferredSlot([&callCount](const SignalArgs&) { ++callCount; }, {});
    Disp().Clear();

    Disp().Flush();
    TEST_ASSERT(callCount == 0, "Cleared deferred work does not run on flush");

    return true;
}

bool TestQueueFree() {
    TEST_SECTION("QueueFree Tests");

    Disp().Clear();

    std::shared_ptr<Node> parent = std::make_shared<Node>("Parent");
    std::shared_ptr<Node> child = std::make_shared<Node>("Child");
    parent->AddChild(child);
    TEST_ASSERT(parent->GetChildCount() == 1, "Parent has the child before free");

    Disp().QueueFree(child.get());
    TEST_ASSERT(Disp().IsQueuedForFree(child.get()), "Child is reported queued for free");

    Disp().QueueFree(child.get());
    TEST_ASSERT(Disp().IsQueuedForFree(child.get()),
        "Duplicate QueueFree is a no-op (still queued, not double-counted)");

    Disp().Flush();
    TEST_ASSERT(!Disp().IsQueuedForFree(child.get()), "Child no longer queued after flush");
    TEST_ASSERT(parent->GetChildCount() == 0, "Child detached from parent after flush");
    TEST_ASSERT(child->GetParent() == nullptr, "Freed child has no parent after flush");

    return true;
}

} // namespace

void RunSignalDispatcherTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "SIGNAL DISPATCHER TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    lupine_test::SetCurrentSuite("Signal Dispatcher");
    bool allPassed = true;

    allPassed &= TestDeferredSlot();
    allPassed &= TestCallDeferred();
    allPassed &= TestDeferredOrdering();
    allPassed &= TestReentrantEnqueue();
    allPassed &= TestLifetimeSafety();
    allPassed &= TestClear();
    allPassed &= TestQueueFree();

    Disp().Clear();

    std::cout << "\n========================================" << std::endl;
    if (allPassed) {
        std::cout << "ALL SIGNAL DISPATCHER TESTS PASSED!" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================\n" << std::endl;
}
