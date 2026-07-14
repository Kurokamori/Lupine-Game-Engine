#include "lupine/core/ArchetypeRegistry.hpp"
#include "lupine/core/ArchetypeDefinition.hpp"
#include "TestFramework.hpp"
#include "TestProject.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

using namespace lupine;
using namespace lupine::core;

namespace {

bool TestScanDiscovery() {
    TEST_SECTION("Archetype Discovery Tests");

    ArchetypeRegistry& reg = ArchetypeRegistry::GetInstance();
    reg.ScanProject(lupine_test::TestProject::Root());

    TEST_ASSERT(reg.GetDefinitions().size() >= 2,
        "Scan finds at least the two generated archetypes");
    TEST_ASSERT(reg.IsArchetype(lupine_test::TestProject::kEnemyClass),
        "EnemyStats is recognised as an archetype");
    TEST_ASSERT(reg.IsArchetype(lupine_test::TestProject::kBossClass),
        "BossStats is recognised as an archetype");
    TEST_ASSERT(!reg.IsArchetype("NotAnArchetype"),
        "An unknown class is not an archetype");

    return true;
}

bool TestDefinitionFields() {
    TEST_SECTION("Archetype Definition Field Tests");

    ArchetypeRegistry& reg = ArchetypeRegistry::GetInstance();
    const ArchetypeDefinition* enemy = reg.GetDefinition(lupine_test::TestProject::kEnemyClass);
    TEST_ASSERT(enemy != nullptr, "EnemyStats definition is retrievable");
    TEST_ASSERT(enemy->isValid, "EnemyStats parsed as valid");
    TEST_ASSERT(enemy->fields.size() == 3, "EnemyStats has three own fields");

    const PropertyDescriptor* health = enemy->FindField("health");
    TEST_ASSERT(health != nullptr, "EnemyStats has a 'health' field");
    TEST_ASSERT(health->type == PropertyValueType::Int, "'health' field is an Int");
    TEST_ASSERT(health->GetDefaultAsJson().get<int>() == 100,
        "'health' default is 100 in the base");

    const PropertyDescriptor* name = enemy->FindField("name");
    TEST_ASSERT(name != nullptr && name->type == PropertyValueType::String,
        "EnemyStats has a String 'name' field");
    TEST_ASSERT(enemy->FindField("missing") == nullptr,
        "FindField returns null for an absent field");

    return true;
}

bool TestInheritanceChain() {
    TEST_SECTION("Inheritance Chain Tests");

    ArchetypeRegistry& reg = ArchetypeRegistry::GetInstance();

    std::vector<std::string> chain = reg.GetInheritanceChain(lupine_test::TestProject::kBossClass);
    TEST_ASSERT(chain.size() == 2, "BossStats chain has two entries");
    TEST_ASSERT(chain[0] == lupine_test::TestProject::kBossClass, "Chain starts with BossStats");
    TEST_ASSERT(chain[1] == lupine_test::TestProject::kEnemyClass, "Chain ends with EnemyStats");

    TEST_ASSERT(reg.IsSubclassOf(lupine_test::TestProject::kBossClass,
                                 lupine_test::TestProject::kEnemyClass),
        "BossStats is a subclass of EnemyStats");
    TEST_ASSERT(reg.IsSubclassOf(lupine_test::TestProject::kBossClass,
                                 lupine_test::TestProject::kBossClass),
        "An archetype is a subclass of itself");
    TEST_ASSERT(!reg.IsSubclassOf(lupine_test::TestProject::kEnemyClass,
                                  lupine_test::TestProject::kBossClass),
        "EnemyStats is not a subclass of BossStats");

    return true;
}

bool TestEffectiveFields() {
    TEST_SECTION("Effective (Merged) Field Tests");

    ArchetypeRegistry& reg = ArchetypeRegistry::GetInstance();

    std::vector<PropertyDescriptor> fields =
        reg.GetEffectiveFields(lupine_test::TestProject::kBossClass);

    bool hasHealth = false, hasSpeed = false, hasName = false, hasPhases = false;
    for (const PropertyDescriptor& f : fields) {
        if (f.name == "health") hasHealth = true;
        if (f.name == "speed") hasSpeed = true;
        if (f.name == "name") hasName = true;
        if (f.name == "phases") hasPhases = true;
    }

    TEST_ASSERT(fields.size() == 4, "BossStats has four effective fields (3 inherited + 1 new, override merged)");
    TEST_ASSERT(hasHealth && hasSpeed && hasName,
        "BossStats inherits health, speed and name from EnemyStats");
    TEST_ASSERT(hasPhases, "BossStats contributes its own 'phases' field");

    nlohmann::json defaults = reg.BuildEffectiveDefaultValues(lupine_test::TestProject::kBossClass);
    TEST_ASSERT(defaults["health"].get<int>() == 500,
        "Derived 'health' default (500) overrides the inherited 100");
    TEST_ASSERT(defaults["phases"].get<int>() == 3, "New 'phases' default is 3");
    TEST_ASSERT(defaults.contains("speed"), "Inherited 'speed' default is present in the merged set");

    return true;
}

bool TestRescanAndRemove() {
    TEST_SECTION("Rescan / Remove Tests");

    ArchetypeRegistry& reg = ArchetypeRegistry::GetInstance();
    size_t before = reg.GetDefinitions().size();

    std::string bossPath = lupine_test::TestProject::ArchetypesDir() + "/BossStats.archetype";
    bool removed = reg.RemoveDefinition(bossPath);
    TEST_ASSERT(removed, "RemoveDefinition reports success for a known source path");
    TEST_ASSERT(!reg.IsArchetype(lupine_test::TestProject::kBossClass),
        "BossStats is gone after removal");
    TEST_ASSERT(reg.GetDefinitions().size() == before - 1, "Definition count drops by one");

    bool rescanned = reg.RescanFile(bossPath);
    TEST_ASSERT(rescanned, "RescanFile re-adds the definition from disk");
    TEST_ASSERT(reg.IsArchetype(lupine_test::TestProject::kBossClass),
        "BossStats is present again after rescan");

    return true;
}

} // namespace

void RunArchetypeTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "ARCHETYPE SYSTEM TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    lupine_test::SetCurrentSuite("Archetype System");

    if (!lupine_test::TestProject::Create()) {
        lupine_test::RecordFail("Could not create the temp test project");
        std::cout << "  [FAIL] Could not create the temp test project" << std::endl;
        std::cout << "========================================\n" << std::endl;
        return;
    }

    bool allPassed = true;

    allPassed &= TestScanDiscovery();
    allPassed &= TestDefinitionFields();
    allPassed &= TestInheritanceChain();
    allPassed &= TestEffectiveFields();
    allPassed &= TestRescanAndRemove();

    ArchetypeRegistry::GetInstance().Clear();
    lupine_test::TestProject::Destroy();

    std::cout << "\n========================================" << std::endl;
    if (allPassed) {
        std::cout << "ALL ARCHETYPE TESTS PASSED!" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================\n" << std::endl;
}
