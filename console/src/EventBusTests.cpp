#include "lupine/core/EventBus.hpp"
#include "lupine/core/SignalDispatcher.hpp"
#include "TestFramework.hpp"
#include <iostream>
#include <string>

using namespace lupine;
using namespace lupine::core;

namespace {

class BusReceiver : public SignalObject {
public:
    int invokeCount = 0;
    int lastValue = 0;
    bool InvokeSignalMethod(const std::string&, const SignalArgs& args) override {
        ++invokeCount;
        if (!args.empty() && args[0].is_number_integer()) {
            lastValue = args[0].get<int>();
        }
        return true;
    }
};

bool TestNativeSubscribe() {
    TEST_SECTION("Native Slot Subscription Tests");

    EventBus::Get().Clear();

    int callCount = 0;
    int lastValue = 0;
    uint64_t id = EventBus::Get().Subscribe("score_changed",
        [&callCount, &lastValue](const SignalArgs& args) {
            ++callCount;
            if (!args.empty() && args[0].is_number_integer()) {
                lastValue = args[0].get<int>();
            }
        });

    TEST_ASSERT(id != 0, "Subscribe returns a non-zero subscription id");
    TEST_ASSERT(EventBus::Get().HasSubscribers("score_changed"), "Event reports subscribers");

    EventBus::Get().Emit("score_changed", { 100 });
    TEST_ASSERT(callCount == 1, "Subscriber invoked once on emit");
    TEST_ASSERT(lastValue == 100, "Subscriber received the emitted argument");

    EventBus::Get().Emit("score_changed", { 250 });
    TEST_ASSERT(callCount == 2 && lastValue == 250, "Subscriber invoked again on second emit");

    EventBus::Get().Clear();
    return true;
}

bool TestUnknownEvent() {
    TEST_SECTION("Unknown Event Tests");

    EventBus::Get().Clear();
    TEST_ASSERT(!EventBus::Get().HasSubscribers("never_used"),
        "Event with no subscribers reports none");
    EventBus::Get().Emit("never_used", { 1 });
    TEST_ASSERT(true, "Emitting an event with no subscribers is safe");

    return true;
}

bool TestUnsubscribe() {
    TEST_SECTION("Unsubscribe Tests");

    EventBus::Get().Clear();

    int callCount = 0;
    uint64_t id = EventBus::Get().Subscribe("evt",
        [&callCount](const SignalArgs&) { ++callCount; });

    EventBus::Get().Emit("evt");
    TEST_ASSERT(callCount == 1, "Invoked before unsubscribe");

    EventBus::Get().Unsubscribe("evt", id);
    EventBus::Get().Emit("evt");
    TEST_ASSERT(callCount == 1, "Not invoked after Unsubscribe");
    TEST_ASSERT(!EventBus::Get().HasSubscribers("evt"), "No subscribers remain after Unsubscribe");

    EventBus::Get().Clear();
    return true;
}

bool TestObjectTarget() {
    TEST_SECTION("Object+Method Subscription Tests");

    EventBus::Get().Clear();

    BusReceiver receiver;
    EventBus::Get().Subscribe("player_hit", &receiver, "on_player_hit");

    EventBus::Get().Emit("player_hit", { 42 });
    TEST_ASSERT(receiver.invokeCount == 1, "Object target invoked on emit");
    TEST_ASSERT(receiver.lastValue == 42, "Object target received argument");

    EventBus::Get().UnsubscribeTarget("player_hit", &receiver, "on_player_hit");
    EventBus::Get().Emit("player_hit", { 7 });
    TEST_ASSERT(receiver.invokeCount == 1, "Object target not invoked after UnsubscribeTarget");

    EventBus::Get().Clear();
    return true;
}

bool TestDeferredEmit() {
    TEST_SECTION("Deferred Emit Tests");

    EventBus::Get().Clear();
    SignalDispatcher::Get().Clear();

    int callCount = 0;
    EventBus::Get().Subscribe("deferred_evt",
        [&callCount](const SignalArgs&) { ++callCount; });

    EventBus::Get().EmitDeferred("deferred_evt");
    TEST_ASSERT(callCount == 0, "Deferred emit does not invoke immediately");

    SignalDispatcher::Get().Flush();
    TEST_ASSERT(callCount == 1, "Deferred subscriber invoked after dispatcher flush");

    EventBus::Get().Clear();
    SignalDispatcher::Get().Clear();
    return true;
}

bool TestClear() {
    TEST_SECTION("Clear Tests");

    EventBus::Get().Clear();

    int callCount = 0;
    EventBus::Get().Subscribe("a", [&callCount](const SignalArgs&) { ++callCount; });
    EventBus::Get().Subscribe("b", [&callCount](const SignalArgs&) { ++callCount; });

    EventBus::Get().Clear();
    TEST_ASSERT(!EventBus::Get().HasSubscribers("a"), "Event 'a' cleared");
    TEST_ASSERT(!EventBus::Get().HasSubscribers("b"), "Event 'b' cleared");

    EventBus::Get().Emit("a");
    EventBus::Get().Emit("b");
    TEST_ASSERT(callCount == 0, "No subscribers fire after Clear");

    return true;
}

bool TestLifetimeSafety() {
    TEST_SECTION("Subscriber Lifetime Tests");

    EventBus::Get().Clear();

    {
        BusReceiver receiver;
        EventBus::Get().Subscribe("evt", &receiver, "handler");
        EventBus::Get().Emit("evt");
        TEST_ASSERT(receiver.invokeCount == 1, "Live subscriber invoked");
    }

    EventBus::Get().Emit("evt");
    TEST_ASSERT(true, "Emitting after a subscriber is destroyed is safe (no crash)");

    EventBus::Get().Clear();
    return true;
}

} // namespace

void RunEventBusTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "EVENT BUS TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    lupine_test::SetCurrentSuite("Event Bus");
    bool allPassed = true;

    allPassed &= TestNativeSubscribe();
    allPassed &= TestUnknownEvent();
    allPassed &= TestUnsubscribe();
    allPassed &= TestObjectTarget();
    allPassed &= TestDeferredEmit();
    allPassed &= TestClear();
    allPassed &= TestLifetimeSafety();

    std::cout << "\n========================================" << std::endl;
    if (allPassed) {
        std::cout << "ALL EVENT BUS TESTS PASSED!" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================\n" << std::endl;
}
