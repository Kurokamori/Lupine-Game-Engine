#include "TestProject.hpp"

#include "lupine/core/ArchetypeDefinition.hpp"
#include "lupine/core/PropertyDescriptor.hpp"
#include "lupine/localization/LocalizationTable.hpp"
#include "lupine/logger/Logger.hpp"

#include <filesystem>
#include <fstream>
#include <string>

namespace lupine_test {

const char* TestProject::kEnemyClass = "EnemyStats";
const char* TestProject::kBossClass = "BossStats";
const char* TestProject::kUiTable = "ui";
const char* TestProject::kDialogueTable = "dialogue";

namespace {

std::string g_Root;

const std::string& RootPath() {
    if (g_Root.empty()) {
        std::filesystem::path base = std::filesystem::temp_directory_path();
        g_Root = (base / "lupine_engine_test_project").string();
    }
    return g_Root;
}

bool WriteTextFile(const std::string& path, const std::string& contents) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        LOG_ERROR(lupine::LogCategory::Core, "TestProject: cannot write '{}'", path);
        return false;
    }
    file << contents;
    return true;
}

bool WriteArchetypes() {
    using lupine::core::ArchetypeDefinition;
    using lupine::core::PropertyDescriptor;
    using lupine::core::PropertyValueType;

    ArchetypeDefinition enemy;
    enemy.className = TestProject::kEnemyClass;
    enemy.menuPath = "Tests/Enemies";
    enemy.description = "Base enemy stats";
    enemy.fields.push_back(PropertyDescriptor("health", PropertyValueType::Int, 100));
    enemy.fields.push_back(PropertyDescriptor("speed", PropertyValueType::Float, 5.0f));
    enemy.fields.push_back(PropertyDescriptor("name", PropertyValueType::String,
                                              std::string("Enemy")));

    ArchetypeDefinition boss;
    boss.className = TestProject::kBossClass;
    boss.baseClass = TestProject::kEnemyClass;
    boss.menuPath = "Tests/Enemies";
    boss.description = "Boss extends enemy";
    // Overrides the inherited health default and adds a new field.
    boss.fields.push_back(PropertyDescriptor("health", PropertyValueType::Int, 500));
    boss.fields.push_back(PropertyDescriptor("phases", PropertyValueType::Int, 3));

    bool ok = true;
    ok &= WriteTextFile(TestProject::ArchetypesDir() + "/EnemyStats.archetype",
                        enemy.Serialize().dump(2));
    ok &= WriteTextFile(TestProject::ArchetypesDir() + "/BossStats.archetype",
                        boss.Serialize().dump(2));
    return ok;
}

bool WriteLocalization() {
    using lupine::localization::LocalizationTable;
    using lupine::localization::LocEntry;

    const std::string config = R"({
    "enabled": true,
    "defaultLocale": "en",
    "fallbackLocale": "en",
    "locales": ["en", "fr"],
    "tablesDir": "localization",
    "csvMode": false,
    "pseudolocalization": false
})";

    bool ok = WriteTextFile(TestProject::Root() + "/localization.json", config);

    LocalizationTable ui;
    ui.SetName(TestProject::kUiTable);
    ui.AddLocale("en");
    ui.AddLocale("fr");

    LocEntry start;
    start.key = "menu.start";
    start.tags = { "menu" };
    start.values["en"] = "Start";
    start.values["fr"] = "Demarrer";
    ui.SetEntry(start);

    LocEntry greeting;
    greeting.key = "greeting";
    greeting.values["en"] = "Hello {name}";
    greeting.values["fr"] = "Bonjour {name}";
    ui.SetEntry(greeting);

    LocEntry fallbackOnly;
    fallbackOnly.key = "english.only";
    fallbackOnly.values["en"] = "English text";
    ui.SetEntry(fallbackOnly);

    LocEntry items;
    items.key = "inventory.items";
    items.values["en"] = "items";
    items.values["fr"] = "objets";
    items.plurals["en"] = { { "one", "{count} item" }, { "other", "{count} items" } };
    items.plurals["fr"] = { { "one", "{count} objet" }, { "other", "{count} objets" } };
    ui.SetEntry(items);

    ok &= WriteTextFile(TestProject::LocalizationDir() + "/ui.loctable", ui.ToJsonText());

    LocalizationTable dialogue;
    dialogue.SetName(TestProject::kDialogueTable);
    dialogue.AddLocale("en");
    dialogue.AddLocale("fr");
    LocEntry hello;
    hello.key = "npc.hello";
    hello.values["en"] = "Greetings, traveler";
    hello.values["fr"] = "Salutations, voyageur";
    dialogue.SetEntry(hello);

    ok &= WriteTextFile(TestProject::LocalizationDir() + "/dialogue.csv", dialogue.ToCsvText());

    return ok;
}

} // namespace

const std::string& TestProject::Root() {
    return RootPath();
}

std::string TestProject::ArchetypesDir() {
    return RootPath() + "/archetypes";
}

std::string TestProject::LocalizationDir() {
    return RootPath() + "/localization";
}

bool TestProject::Create() {
    Destroy();

    std::error_code ec;
    std::filesystem::create_directories(ArchetypesDir(), ec);
    if (ec) {
        LOG_ERROR(lupine::LogCategory::Core, "TestProject: cannot create '{}': {}",
                  ArchetypesDir(), ec.message());
        return false;
    }
    std::filesystem::create_directories(LocalizationDir(), ec);
    if (ec) {
        LOG_ERROR(lupine::LogCategory::Core, "TestProject: cannot create '{}': {}",
                  LocalizationDir(), ec.message());
        return false;
    }

    bool ok = true;
    ok &= WriteTextFile(Root() + "/project.lupine", "{\n  \"projectName\": \"TestProject\"\n}\n");
    ok &= WriteArchetypes();
    ok &= WriteLocalization();
    return ok;
}

void TestProject::Destroy() {
    std::error_code ec;
    std::filesystem::remove_all(RootPath(), ec);
}

} // namespace lupine_test
