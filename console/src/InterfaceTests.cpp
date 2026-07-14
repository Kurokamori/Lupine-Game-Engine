#include "lupine/core/InterfaceRegistry.hpp"
#include "lupine/core/InterfaceDefinition.hpp"
#include "TestFramework.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_set>

using namespace lupine;
using namespace lupine::core;

namespace {

bool Contains(const std::vector<std::string>& values, const std::string& needle) {
    return std::find(values.begin(), values.end(), needle) != values.end();
}

InterfaceDefinition MakeInterface(const std::string& name,
                                  const std::vector<std::string>& bases,
                                  const std::vector<std::string>& methodNames,
                                  const std::vector<std::string>& signalNames) {
    InterfaceDefinition def;
    def.name = name;
    def.baseInterfaces = bases;
    for (const std::string& m : methodNames) {
        InterfaceMethod method;
        method.name = m;
        def.methods.push_back(method);
    }
    for (const std::string& s : signalNames) {
        SignalDesc sig;
        sig.name = s;
        def.signals.push_back(sig);
    }
    return def;
}

bool TestRegistrationAndDefinitions() {
    TEST_SECTION("Interface Registration & Definitions");

    InterfaceRegistry& reg = InterfaceRegistry::GetInstance();

    bool entity = reg.RegisterRuntimeInterface(MakeInterface("Entity", {}, {}, {"spawned"}));
    bool damageable = reg.RegisterRuntimeInterface(
        MakeInterface("Damageable", {"Entity"}, {"take_damage"}, {"died"}));

    TEST_ASSERT(entity, "Registering the Entity interface succeeds");
    TEST_ASSERT(damageable, "Registering the Damageable interface succeeds");

    TEST_ASSERT(reg.IsInterface("Damageable"), "Damageable is a registered interface");
    TEST_ASSERT(reg.IsInterface("Entity"), "Entity is a registered interface");
    TEST_ASSERT(!reg.IsInterface("NotAnInterface"), "An unknown name is not an interface");

    const InterfaceDefinition* def = reg.GetDefinition("Damageable");
    TEST_ASSERT(def != nullptr, "Damageable definition is retrievable");
    TEST_ASSERT(def && def->FindMethod("take_damage") != nullptr,
        "Damageable declares the take_damage method");
    TEST_ASSERT(def && def->FindSignal("died") != nullptr,
        "Damageable declares the died signal");

    TEST_ASSERT(Contains(reg.GetInterfaceNames(), "Damageable"),
        "GetInterfaceNames lists Damageable");

    return true;
}

bool TestInheritance() {
    TEST_SECTION("Interface Inheritance");

    InterfaceRegistry& reg = InterfaceRegistry::GetInstance();

    std::vector<std::string> chain = reg.GetInheritanceChain("Damageable");
    TEST_ASSERT(chain.size() == 2, "Damageable chain has two entries");
    TEST_ASSERT(!chain.empty() && chain[0] == "Damageable", "Chain starts with Damageable");
    TEST_ASSERT(Contains(chain, "Entity"), "Chain includes the base Entity");

    TEST_ASSERT(reg.IsSubInterfaceOf("Damageable", "Entity"),
        "Damageable is a sub-interface of Entity");
    TEST_ASSERT(reg.IsSubInterfaceOf("Damageable", "Damageable"),
        "An interface is a sub-interface of itself");
    TEST_ASSERT(!reg.IsSubInterfaceOf("Entity", "Damageable"),
        "Entity is not a sub-interface of Damageable");

    std::vector<std::string> implied = reg.ExpandImplied({"Damageable"});
    TEST_ASSERT(Contains(implied, "Damageable") && Contains(implied, "Entity"),
        "ExpandImplied({Damageable}) yields both Damageable and Entity");

    std::vector<SignalDesc> signals = reg.GetEffectiveSignals("Damageable");
    bool hasDied = false, hasSpawned = false;
    for (const SignalDesc& s : signals) {
        if (s.name == "died") hasDied = true;
        if (s.name == "spawned") hasSpawned = true;
    }
    TEST_ASSERT(hasDied && hasSpawned,
        "Effective signals of Damageable include its own and the inherited signal");

    std::vector<InterfaceMethod> methods = reg.GetEffectiveMethods("Damageable");
    bool hasTakeDamage = false;
    for (const InterfaceMethod& m : methods) {
        if (m.name == "take_damage") hasTakeDamage = true;
    }
    TEST_ASSERT(hasTakeDamage, "Effective methods of Damageable include take_damage");

    return true;
}

bool TestTypeConformance() {
    TEST_SECTION("Native Type Conformance");

    InterfaceRegistry& reg = InterfaceRegistry::GetInstance();
    reg.RegisterTypeConformance("HealthComponent", "Damageable");

    TEST_ASSERT(reg.TypeImplementsInterface("HealthComponent", "Damageable"),
        "HealthComponent implements Damageable directly");
    TEST_ASSERT(reg.TypeImplementsInterface("HealthComponent", "Entity"),
        "HealthComponent implements Entity through interface inheritance");
    TEST_ASSERT(!reg.TypeImplementsInterface("HealthComponent", "Flammable"),
        "HealthComponent does not implement an unrelated interface");

    TEST_ASSERT(Contains(reg.GetTypesImplementing("Damageable"), "HealthComponent"),
        "GetTypesImplementing(Damageable) includes HealthComponent");
    TEST_ASSERT(Contains(reg.GetTypesImplementing("Entity"), "HealthComponent"),
        "GetTypesImplementing(Entity) includes HealthComponent via inheritance");

    return true;
}

bool TestVerifyMembers() {
    TEST_SECTION("Contract Verification");

    InterfaceRegistry& reg = InterfaceRegistry::GetInstance();

    std::unordered_set<std::string> allMethods = {"take_damage"};
    std::unordered_set<std::string> allSignals = {"died", "spawned"};
    nlohmann::json full = reg.VerifyMembers("Damageable", allMethods, allSignals);
    TEST_ASSERT(full["exists"].get<bool>(), "Verify reports the interface exists");
    TEST_ASSERT(full["conforms"].get<bool>(),
        "A candidate with every method and signal conforms");

    std::unordered_set<std::string> none;
    nlohmann::json empty = reg.VerifyMembers("Damageable", none, none);
    TEST_ASSERT(!empty["conforms"].get<bool>(),
        "A candidate missing members does not conform");
    TEST_ASSERT(!empty["missing_methods"].empty(),
        "Missing methods are reported");
    TEST_ASSERT(!empty["missing_signals"].empty(),
        "Missing signals (own and inherited) are reported");

    nlohmann::json unknown = reg.VerifyMembers("NoSuchInterface", allMethods, allSignals);
    TEST_ASSERT(!unknown["exists"].get<bool>(),
        "Verifying an unknown interface reports it does not exist");
    TEST_ASSERT(!unknown["conforms"].get<bool>(),
        "An unknown interface never conforms");

    return true;
}

bool TestPersistenceAcrossClear() {
    TEST_SECTION("Runtime Interfaces Survive Clear");

    InterfaceRegistry& reg = InterfaceRegistry::GetInstance();

    TEST_ASSERT(reg.IsInterface("Damageable"), "Damageable present before Clear");
    reg.Clear();
    TEST_ASSERT(reg.IsInterface("Damageable"),
        "Runtime-registered Damageable survives Clear (re-applied from the persistent store)");
    TEST_ASSERT(reg.IsInterface("Entity"),
        "Runtime-registered Entity survives Clear");

    return true;
}

} // namespace

void RunInterfaceTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "INTERFACE TYPES TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    lupine_test::SetCurrentSuite("Interface Types");

    bool allPassed = true;

    allPassed &= TestRegistrationAndDefinitions();
    allPassed &= TestInheritance();
    allPassed &= TestTypeConformance();
    allPassed &= TestVerifyMembers();
    allPassed &= TestPersistenceAcrossClear();

    InterfaceRegistry::GetInstance().Clear();

    std::cout << "\n========================================" << std::endl;
    if (allPassed) {
        std::cout << "ALL INTERFACE TESTS PASSED!" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================\n" << std::endl;
}
