#include "TestFramework.hpp"

#include "lupine/core/CustomComponentRegistry.hpp"
#include "lupine/core/ScriptedComponentWrapper.hpp"
#include "lupine/core/Serialization.hpp"
#include "lupine/core/Component.hpp"
#include "lupine/scripting/LuaEnvironment.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using namespace lupine;
using namespace lupine::core;

namespace {

bool ChainContains(const std::vector<std::string>& values, const std::string& needle) {
    return std::find(values.begin(), values.end(), needle) != values.end();
}

void WriteScript(const std::filesystem::path& dir, const std::string& fileName,
                 const std::string& className, const std::string& base) {
    std::ofstream out(dir / fileName);
    out << "--@component_class \"" << className << "\"\n";
    if (!base.empty()) {
        out << "--@extends_component \"" << base << "\"\n";
    }
    out << "function on_ready() end\n";
}

std::filesystem::path MakeChainProject() {
    std::filesystem::path root =
        std::filesystem::temp_directory_path() / "lupine_chain_test_project";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root, ec);

    // A three-level custom chain rooted at a built-in renderable, declared in an
    // order where children are scanned before their custom parents to exercise the
    // two-pass resolution.
    WriteScript(root, "leaf.lua", "LeafThing", "MidThing");
    WriteScript(root, "mid.lua", "MidThing", "BaseThing");
    WriteScript(root, "base.lua", "BaseThing", "Sprite2D");

    // A two-node cycle plus a self-extending class to exercise cycle detection.
    WriteScript(root, "cycle_a.lua", "CycleA", "CycleB");
    WriteScript(root, "cycle_b.lua", "CycleB", "CycleA");
    WriteScript(root, "selfish.lua", "Selfish", "Selfish");

    return root;
}

bool TestChainResolution() {
    TEST_SECTION("Custom-type bases resolve regardless of scan order");

    std::filesystem::path root = MakeChainProject();
    CustomComponentRegistry& registry = CustomComponentRegistry::GetInstance();
    registry.ScanProject(root.string());

    const CustomComponentDefinition* leaf = registry.GetDefinition("LeafThing");
    const CustomComponentDefinition* mid = registry.GetDefinition("MidThing");
    const CustomComponentDefinition* base = registry.GetDefinition("BaseThing");

    TEST_ASSERT(leaf != nullptr, "LeafThing was discovered");
    TEST_ASSERT(mid != nullptr, "MidThing was discovered");
    TEST_ASSERT(base != nullptr, "BaseThing was discovered");
    TEST_ASSERT(leaf->baseComponentType == "MidThing",
                "LeafThing resolves its custom base MidThing");
    TEST_ASSERT(mid->baseComponentType == "BaseThing",
                "MidThing resolves its custom base BaseThing");
    TEST_ASSERT(base->baseComponentType == "Sprite2D",
                "BaseThing resolves its built-in base Sprite2D");

    std::vector<std::string> inheritable =
        CustomComponentRegistry::GetInheritableComponentTypes();
    TEST_ASSERT(ChainContains(inheritable, "BaseThing"),
                "Inheritable list includes custom type BaseThing");
    TEST_ASSERT(ChainContains(inheritable, "MidThing"),
                "Inheritable list includes custom type MidThing");
    TEST_ASSERT(ChainContains(inheritable, "Sprite2D"),
                "Inheritable list still includes built-in Sprite2D");

    return true;
}

bool TestCycleDetection() {
    TEST_SECTION("Inheritance cycles are broken at scan time");

    CustomComponentRegistry& registry = CustomComponentRegistry::GetInstance();

    const CustomComponentDefinition* cycleA = registry.GetDefinition("CycleA");
    const CustomComponentDefinition* cycleB = registry.GetDefinition("CycleB");
    const CustomComponentDefinition* selfish = registry.GetDefinition("Selfish");

    TEST_ASSERT(cycleA != nullptr && cycleB != nullptr,
                "Cyclic classes were still discovered");
    TEST_ASSERT(cycleA->baseComponentType.empty() || cycleB->baseComponentType.empty(),
                "At least one link of the A<->B cycle was cleared");
    TEST_ASSERT(selfish != nullptr && selfish->baseComponentType.empty(),
                "Self-extending class had its base link cleared");

    return true;
}

bool TestInstantiatedAncestry() {
    TEST_SECTION("Instantiated chain reports full ancestry");

    std::shared_ptr<ISerializable> instance =
        TypeRegistry::GetInstance().CreateInstance("LeafThing");
    TEST_ASSERT(instance != nullptr, "LeafThing instantiated through TypeRegistry");

    auto* component = dynamic_cast<Component*>(instance.get());
    TEST_ASSERT(component != nullptr, "LeafThing instance is a Component");

    std::vector<std::string> chain = component->GetTypeChain();
    TEST_ASSERT(chain.size() >= 4, "Type chain has all four levels");
    TEST_ASSERT(!chain.empty() && chain.front() == "LeafThing",
                "Type chain starts at the most-derived type");
    TEST_ASSERT(ChainContains(chain, "MidThing"), "Type chain contains MidThing");
    TEST_ASSERT(ChainContains(chain, "BaseThing"), "Type chain contains BaseThing");
    TEST_ASSERT(ChainContains(chain, "Sprite2D"), "Type chain reaches the built-in base");

    TEST_ASSERT(component->IsInstanceOf("LeafThing"), "IsInstanceOf self is true");
    TEST_ASSERT(component->IsInstanceOf("BaseThing"), "IsInstanceOf an intermediate ancestor is true");
    TEST_ASSERT(component->IsInstanceOf("Sprite2D"), "IsInstanceOf the built-in base is true");
    TEST_ASSERT(!component->IsInstanceOf("Label"), "IsInstanceOf an unrelated type is false");

    // A wrapper extending a renderable base is itself renderable through the chain.
    auto* renderable = dynamic_cast<IRenderableComponent*>(instance.get());
    TEST_ASSERT(renderable != nullptr,
                "LeafThing (extends Sprite2D) is an IRenderableComponent");

    return true;
}

bool TestCustomBaseResolvesInScriptVM() {
    TEST_SECTION("Custom component classes are inheritable inside the script VM");

    // The C++ registry resolving a custom base is only half the story: the script
    // itself executes `SimBoulder = SimObject:extend()`, so the custom base has to be
    // a real name in the VM. It used to be that only the built-in component types were
    // registered, so any script inheriting from another custom component died on its
    // first line with "attempt to index a nil value" and the component never loaded.
    scripting::LuaEnvironment env;
    TEST_ASSERT(env.Initialize(), "Lua environment initializes");

    env.RegisterCustomComponentTypes();

    // BaseThing/MidThing/LeafThing are the chain registered by TestChainResolution.
    scripting::ScriptResult builtIn = env.ExecuteString("FromBuiltIn = Sprite2D:extend()");
    TEST_ASSERT(builtIn.success, "A built-in base still extends");

    scripting::ScriptResult custom = env.ExecuteString("FromCustom = BaseThing:extend()");
    TEST_ASSERT(custom.success,
                std::string("A custom base extends: ") + custom.error);

    scripting::ScriptResult chained = env.ExecuteString("FromChain = MidThing:extend()");
    TEST_ASSERT(chained.success,
                std::string("A custom base that itself extends a custom base extends: ") +
                    chained.error);

    // The derived table records the base it extended, which is what the registry's
    // native-class-syntax parse keys off.
    scripting::ScriptResult recorded = env.ExecuteString(
        "assert(FromCustom.__baseComponentType == 'BaseThing')\n"
        "assert(FromChain.__baseComponentType == 'MidThing')");
    TEST_ASSERT(recorded.success,
                std::string("Derived tables record their custom base type: ") + recorded.error);

    // An unknown name must stay nil - registration must not invent classes.
    scripting::ScriptResult unknown = env.ExecuteString("NotAThing = (NoSuchComponent == nil)");
    TEST_ASSERT(unknown.success && env.GetGlobalBool("NotAThing", false),
                "An unregistered name is still nil in the VM");

    env.Shutdown();
    return true;
}

} // namespace

void RunCustomComponentChainTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "CUSTOM COMPONENT INHERITANCE CHAIN TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    lupine_test::SetCurrentSuite("Custom Component Chains");

    bool allPassed = true;
    allPassed &= TestChainResolution();
    allPassed &= TestCycleDetection();
    allPassed &= TestInstantiatedAncestry();
    allPassed &= TestCustomBaseResolvesInScriptVM();

    std::filesystem::path root =
        std::filesystem::temp_directory_path() / "lupine_chain_test_project";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);

    std::cout << "\n========================================" << std::endl;
    if (allPassed) {
        std::cout << "ALL CUSTOM COMPONENT CHAIN TESTS PASSED!" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================\n" << std::endl;
}
