#include "lupine/core/SignalObject.hpp"
#include "TestFramework.hpp"
#include <iostream>
#include <string>

using namespace lupine;
using namespace lupine::core;

namespace {

bool TestSignalDeclaration() {
    TEST_SECTION("Signal Declaration Tests");

    SignalObject obj;
    TEST_ASSERT(!obj.HasSignal("damaged"), "Unknown signal is not declared");

    SignalArgDesc amountArg;
    amountArg.name = "amount";
    amountArg.type = PropertyValueType::Int;
    obj.AddUserSignal("damaged", { amountArg });

    TEST_ASSERT(obj.HasSignal("damaged"), "User signal is declared after AddUserSignal");

    std::vector<SignalDesc> descriptors = obj.GetSignalDescriptors();
    bool foundDamaged = false;
    for (const SignalDesc& desc : descriptors) {
        if (desc.name == "damaged") {
            foundDamaged = true;
            TEST_ASSERT(desc.args.size() == 1, "Signal descriptor has one argument");
            TEST_ASSERT(desc.args[0].name == "amount", "Argument name is correct");
            TEST_ASSERT(desc.args[0].type == PropertyValueType::Int, "Argument type is correct");
        }
    }
    TEST_ASSERT(foundDamaged, "Declared signal appears in descriptors");

    return true;
}

bool TestNativeSlotEmission() {
    TEST_SECTION("Native Slot Emission Tests");

    SignalObject obj;
    obj.AddUserSignal("fired");

    int callCount = 0;
    int lastValue = 0;
    uint64_t id = obj.Connect("fired", [&callCount, &lastValue](const SignalArgs& args) {
        ++callCount;
        if (!args.empty() && args[0].is_number_integer()) {
            lastValue = args[0].get<int>();
        }
    });

    TEST_ASSERT(id != 0, "Connect returns a non-zero connection id");
    TEST_ASSERT(obj.IsConnected("fired"), "Signal reports it is connected");
    TEST_ASSERT(obj.GetConnectionCount("fired") == 1, "Connection count is 1");
    TEST_ASSERT(obj.HasConnections("fired"), "HasConnections is true with a live slot");

    obj.Emit("fired", { 42 });
    TEST_ASSERT(callCount == 1, "Slot invoked exactly once on emit");
    TEST_ASSERT(lastValue == 42, "Slot received emitted argument");

    obj.Emit("fired", { 7 });
    TEST_ASSERT(callCount == 2, "Slot invoked again on second emit");
    TEST_ASSERT(lastValue == 7, "Slot received second argument");

    return true;
}

bool TestMulticastDispatch() {
    TEST_SECTION("Multicast Dispatch Tests");

    SignalObject obj;
    obj.AddUserSignal("ping");

    int a = 0;
    int b = 0;
    obj.Connect("ping", [&a](const SignalArgs&) { ++a; });
    obj.Connect("ping", [&b](const SignalArgs&) { ++b; });

    TEST_ASSERT(obj.GetConnectionCount("ping") == 2, "Two connections registered");

    obj.Emit("ping");
    TEST_ASSERT(a == 1 && b == 1, "Both slots invoked on single emit");

    return true;
}

bool TestOneShotConnection() {
    TEST_SECTION("One-Shot Connection Tests");

    SignalObject obj;
    obj.AddUserSignal("once");

    int callCount = 0;
    obj.Connect("once", [&callCount](const SignalArgs&) { ++callCount; }, Connect_OneShot);

    obj.Emit("once");
    obj.Emit("once");
    obj.Emit("once");

    TEST_ASSERT(callCount == 1, "One-shot slot invoked only once across multiple emits");
    TEST_ASSERT(obj.GetConnectionCount("once") == 0, "One-shot slot auto-disconnected");

    return true;
}

bool TestDisconnect() {
    TEST_SECTION("Disconnect Tests");

    SignalObject obj;
    obj.AddUserSignal("evt");

    int callCount = 0;
    uint64_t id = obj.Connect("evt", [&callCount](const SignalArgs&) { ++callCount; });

    obj.Emit("evt");
    TEST_ASSERT(callCount == 1, "Slot invoked before disconnect");

    obj.Disconnect("evt", id);
    TEST_ASSERT(obj.GetConnectionCount("evt") == 0, "Connection removed after Disconnect");

    obj.Emit("evt");
    TEST_ASSERT(callCount == 1, "Slot not invoked after disconnect");

    obj.Connect("evt", [](const SignalArgs&) {});
    obj.Connect("evt", [](const SignalArgs&) {});
    TEST_ASSERT(obj.GetConnectionCount("evt") == 2, "Two new connections established");

    obj.DisconnectAll();
    TEST_ASSERT(obj.GetConnectionCount("evt") == 0, "DisconnectAll clears all connections");

    return true;
}

bool TestObjectMethodTarget() {
    TEST_SECTION("Object+Method Target Tests");

    class Receiver : public SignalObject {
    public:
        int invokeCount = 0;
        int lastArg = 0;
        std::string lastMethod;

        bool InvokeSignalMethod(const std::string& method, const SignalArgs& args) override {
            ++invokeCount;
            lastMethod = method;
            if (!args.empty() && args[0].is_number_integer()) {
                lastArg = args[0].get<int>();
            }
            return true;
        }
    };

    SignalObject emitter;
    emitter.AddUserSignal("hit");

    Receiver receiver;
    uint64_t id = emitter.Connect("hit", &receiver, "on_hit");
    TEST_ASSERT(id != 0, "Object+method connection succeeds");
    TEST_ASSERT(emitter.IsConnectedTo("hit", &receiver, "on_hit"),
        "IsConnectedTo reports the target connection");

    emitter.Emit("hit", { 99 });
    TEST_ASSERT(receiver.invokeCount == 1, "Target method invoked once");
    TEST_ASSERT(receiver.lastMethod == "on_hit", "Target method name forwarded");
    TEST_ASSERT(receiver.lastArg == 99, "Target method received argument");

    emitter.DisconnectTarget("hit", &receiver, "on_hit");
    TEST_ASSERT(!emitter.IsConnectedTo("hit", &receiver, "on_hit"),
        "Target connection removed by DisconnectTarget");

    emitter.Emit("hit", { 1 });
    TEST_ASSERT(receiver.invokeCount == 1, "Target not invoked after DisconnectTarget");

    return true;
}

bool TestBoundArguments() {
    TEST_SECTION("Bound Argument Tests");

    class Receiver : public SignalObject {
    public:
        SignalArgs received;
        bool InvokeSignalMethod(const std::string&, const SignalArgs& args) override {
            received = args;
            return true;
        }
    };

    SignalObject emitter;
    emitter.AddUserSignal("evt");

    Receiver receiver;
    SignalArgs binds = { std::string("bound") };
    emitter.Connect("evt", &receiver, "handler", Connect_None, binds);

    emitter.Emit("evt", { 5 });
    TEST_ASSERT(receiver.received.size() == 2, "Emitted and bound args are combined");
    TEST_ASSERT(receiver.received[0].is_number_integer() && receiver.received[0].get<int>() == 5,
        "First arg is the emitted value");
    TEST_ASSERT(receiver.received[1].is_string() && receiver.received[1].get<std::string>() == "bound",
        "Second arg is the bound value");

    return true;
}

bool TestLifetimeSafety() {
    TEST_SECTION("Lifetime Safety Tests");

    SignalObject emitter;
    emitter.AddUserSignal("evt");

    int invokeCount = 0;
    {
        class Receiver : public SignalObject {
        public:
            int* counter = nullptr;
            bool InvokeSignalMethod(const std::string&, const SignalArgs&) override {
                if (counter) { ++(*counter); }
                return true;
            }
        };

        Receiver receiver;
        receiver.counter = &invokeCount;
        emitter.Connect("evt", &receiver, "handler");

        emitter.Emit("evt");
        TEST_ASSERT(invokeCount == 1, "Target invoked while alive");
    }

    emitter.Emit("evt");
    TEST_ASSERT(invokeCount == 1, "Dead target is not invoked (no crash, no extra call)");

    return true;
}

} // namespace

void RunSignalTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "SIGNAL / EVENT SYSTEM TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    lupine_test::SetCurrentSuite("Signal / Event System");
    bool allPassed = true;

    allPassed &= TestSignalDeclaration();
    allPassed &= TestNativeSlotEmission();
    allPassed &= TestMulticastDispatch();
    allPassed &= TestOneShotConnection();
    allPassed &= TestDisconnect();
    allPassed &= TestObjectMethodTarget();
    allPassed &= TestBoundArguments();
    allPassed &= TestLifetimeSafety();

    std::cout << "\n========================================" << std::endl;
    if (allPassed) {
        std::cout << "ALL SIGNAL SYSTEM TESTS PASSED!" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================\n" << std::endl;
}
