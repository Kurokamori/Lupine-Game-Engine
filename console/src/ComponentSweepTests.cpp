/**
 * @file ComponentSweepTests.cpp
 * @brief Generic reflection sweep over every type in the engine TypeRegistry.
 *
 * The per-component behavioral suites verify each component's specific semantics.
 * This sweep complements them with breadth: it instantiates EVERY registered node
 * and component type and drives it through the engine's reflection and serialization
 * machinery (type-name, property-descriptor enumeration, Serialize -> Deserialize ->
 * re-Serialize round-trip). It scales automatically — a newly registered type is
 * exercised here the moment it appears in the registry, with no edit to this file —
 * and catches whole classes of regressions (a property dropped from serialization, a
 * descriptor with a bad type, a component that throws on construction or round-trip).
 *
 * Per-type work is wrapped in try/catch and the results accumulated, so a single
 * misbehaving type is reported as a failure without aborting the rest of the sweep.
 */

#include "lupine/engine/Engine.hpp"
#include "lupine/core/Serialization.hpp"
#include "lupine/core/Component.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/PropertyDescriptor.hpp"
#include "TestFramework.hpp"
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <exception>

using namespace lupine;
using namespace lupine::core;

namespace {

bool ValidPropertyType(PropertyValueType type) {
    return type >= PropertyValueType::Int && type <= PropertyValueType::Dictionary;
}

bool TestRegistryPopulated() {
    TEST_SECTION("Component Sweep: Type Registry Populated");

    std::vector<std::string> typeNames = TypeRegistry::GetInstance().GetRegisteredTypeNames();
    TEST_ASSERT(!typeNames.empty(), "Engine registers at least one type after initialization");
    std::cout << "    Registered types: " << typeNames.size() << std::endl;

    TEST_ASSERT(TypeRegistry::GetInstance().IsTypeRegistered(typeNames.front()),
                "IsTypeRegistered agrees for an enumerated name");
    TEST_ASSERT(!TypeRegistry::GetInstance().IsTypeRegistered("__not_a_real_type__"),
                "IsTypeRegistered is false for an unknown name");

    return true;
}

bool TestFullSweep() {
    TEST_SECTION("Component Sweep: All Registered Types");

    TypeRegistry& registry = TypeRegistry::GetInstance();
    std::vector<std::string> typeNames = registry.GetRegisteredTypeNames();

    int nodeTypes = 0;
    int componentTypes = 0;
    int otherTypes = 0;
    int nodeRoundTrips = 0;
    int componentRoundTrips = 0;
    long propertiesExercised = 0;
    int descriptorAnomalies = 0;
    int instantiationFailures = 0;
    std::vector<std::string> mismatches;

    for (const std::string& name : typeNames) {
        try {
            std::shared_ptr<ISerializable> instance = registry.CreateInstance(name);
            if (!instance) {
                ++instantiationFailures;
                continue;
            }

            if (std::shared_ptr<Node> node = std::dynamic_pointer_cast<Node>(instance)) {
                ++nodeTypes;
                nlohmann::json j1 = node->Serialize();
                std::shared_ptr<ISerializable> instance2 = registry.CreateInstance(name);
                std::shared_ptr<Node> node2 = std::dynamic_pointer_cast<Node>(instance2);
                if (node2) {
                    node2->Deserialize(j1);
                    nlohmann::json j2 = node2->Serialize();
                    if (j1 == j2) {
                        ++nodeRoundTrips;
                    } else {
                        mismatches.push_back(name + " (node)");
                    }
                }
                continue;
            }

            if (std::shared_ptr<Component> comp = std::dynamic_pointer_cast<Component>(instance)) {
                ++componentTypes;

                for (const PropertyDescriptor& desc : comp->GetPropertyDescriptors()) {
                    if (desc.name.empty() || !ValidPropertyType(desc.type)) {
                        ++descriptorAnomalies;
                    }
                    ++propertiesExercised;
                }

                bool toggled = comp->IsEnabled();
                comp->SetEnabled(!toggled);
                if (comp->IsEnabled() != (!toggled)) {
                    mismatches.push_back(name + " (enabled flag)");
                }
                comp->SetEnabled(toggled);

                nlohmann::json j1 = comp->Serialize();
                std::shared_ptr<ISerializable> instance2 = registry.CreateInstance(name);
                std::shared_ptr<Component> comp2 = std::dynamic_pointer_cast<Component>(instance2);
                if (comp2) {
                    comp2->Deserialize(j1);
                    nlohmann::json j2 = comp2->Serialize();
                    if (j1 == j2) {
                        ++componentRoundTrips;
                    } else {
                        mismatches.push_back(name + " (component)");
                    }
                }
                continue;
            }

            ++otherTypes;
        } catch (const std::exception& e) {
            ++instantiationFailures;
            mismatches.push_back(name + std::string(" (threw: ") + e.what() + ")");
        } catch (...) {
            ++instantiationFailures;
            mismatches.push_back(name + " (threw: unknown)");
        }
    }

    std::cout << "    Types swept:                  " << typeNames.size() << std::endl;
    std::cout << "    Node types:                   " << nodeTypes
              << " (" << nodeRoundTrips << " round-trips)" << std::endl;
    std::cout << "    Component types:              " << componentTypes
              << " (" << componentRoundTrips << " round-trips)" << std::endl;
    std::cout << "    Reflected properties swept:   " << propertiesExercised << std::endl;
    std::cout << "    Other (non-node/component):   " << otherTypes << std::endl;
    if (!mismatches.empty()) {
        std::cout << "    Anomalies:" << std::endl;
        for (const std::string& m : mismatches) {
            std::cout << "      - " << m << std::endl;
        }
    }

    TEST_ASSERT(nodeTypes > 0, "Sweep instantiated at least one node type");
    TEST_ASSERT(componentTypes > 0, "Sweep instantiated at least one component type");
    TEST_ASSERT(instantiationFailures == 0, "Every registered type instantiated without throwing");
    TEST_ASSERT(descriptorAnomalies == 0, "Every property descriptor has a name and a valid type");
    TEST_ASSERT(nodeRoundTrips == nodeTypes, "Every node type round-trips serialize/deserialize");
    TEST_ASSERT(componentRoundTrips == componentTypes,
                "Every component type round-trips serialize/deserialize");
    TEST_ASSERT(static_cast<int>(typeNames.size()) == (nodeTypes + componentTypes + otherTypes),
                "Every registered type was classified as node, component, or other");

    return true;
}

} // namespace

void RunComponentSweepTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "COMPONENT SWEEP TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    lupine_test::SetCurrentSuite("ComponentSweep");

    engine::InitializeEngine();

    bool allPassed = true;
    allPassed &= TestRegistryPopulated();
    allPassed &= TestFullSweep();

    std::cout << "\n========================================" << std::endl;
    if (allPassed) {
        std::cout << "ALL COMPONENT SWEEP TESTS PASSED!" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================\n" << std::endl;
}
