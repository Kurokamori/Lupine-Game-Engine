#include "lupine/localization/LocalizationManager.hpp"
#include "TestFramework.hpp"
#include "TestProject.hpp"
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>

using namespace lupine;
using namespace lupine::localization;

namespace {

LocalizationManager& Mgr() {
    return LocalizationManager::GetInstance();
}

bool TestLoadProject() {
    TEST_SECTION("Load Project Tests");

    Mgr().LoadProject(lupine_test::TestProject::Root());
    Mgr().SetLocale("en", false);

    TEST_ASSERT(Mgr().IsLoaded(), "Manager reports loaded after LoadProject");
    TEST_ASSERT(Mgr().IsEnabled(), "Localization is enabled per config");
    TEST_ASSERT(Mgr().GetDefaultLocale() == "en", "Default locale is 'en'");
    TEST_ASSERT(Mgr().GetTables().size() >= 2, "Both generated tables are discovered");
    TEST_ASSERT(Mgr().GetTable(lupine_test::TestProject::kUiTable) != nullptr,
        "The 'ui' table is loaded");
    TEST_ASSERT(Mgr().GetTable(lupine_test::TestProject::kDialogueTable) != nullptr,
        "The CSV 'dialogue' table is loaded");

    std::vector<std::string> locales = Mgr().GetAvailableLocales();
    TEST_ASSERT(std::find(locales.begin(), locales.end(), "fr") != locales.end(),
        "French is among the available locales");

    return true;
}

bool TestBasicTranslate() {
    TEST_SECTION("Basic Translation Tests");

    Mgr().SetLocale("en", false);
    TEST_ASSERT(Mgr().Translate("menu.start") == "Start", "English translation of menu.start");
    TEST_ASSERT(Mgr().HasKey("menu.start"), "HasKey is true for a known key");
    TEST_ASSERT(!Mgr().HasKey("does.not.exist"), "HasKey is false for an unknown key");

    TEST_ASSERT(Mgr().Translate("missing.key") == "missing.key",
        "Unknown key returns the key itself as a visible fallback");

    Mgr().SetLocale("fr", false);
    TEST_ASSERT(Mgr().Translate("menu.start") == "Demarrer", "French translation after locale switch");

    Mgr().SetLocale("en", false);
    return true;
}

bool TestFallback() {
    TEST_SECTION("Fallback Locale Tests");

    Mgr().SetLocale("fr", false);
    TEST_ASSERT(Mgr().Translate("english.only") == "English text",
        "Key missing in French falls back to the English value");

    Mgr().SetLocale("en", false);
    return true;
}

bool TestTableQualified() {
    TEST_SECTION("Table-Qualified Lookup Tests");

    Mgr().SetLocale("en", false);
    TEST_ASSERT(Mgr().Translate("npc.hello", lupine_test::TestProject::kDialogueTable)
                    == "Greetings, traveler",
        "Table-qualified lookup resolves from the dialogue table");
    TEST_ASSERT(Mgr().HasKey("npc.hello", lupine_test::TestProject::kDialogueTable),
        "HasKey honours the table argument");

    return true;
}

bool TestFormat() {
    TEST_SECTION("Format Translation Tests");

    Mgr().SetLocale("en", false);
    std::unordered_map<std::string, std::string> args = { { "name", "Ada" } };
    TEST_ASSERT(Mgr().TranslateFormat("greeting", args) == "Hello Ada",
        "TranslateFormat substitutes the placeholder (English)");

    Mgr().SetLocale("fr", false);
    TEST_ASSERT(Mgr().TranslateFormat("greeting", args) == "Bonjour Ada",
        "TranslateFormat substitutes the placeholder (French)");

    Mgr().SetLocale("en", false);
    return true;
}

bool TestPlural() {
    TEST_SECTION("Plural Translation Tests");

    Mgr().SetLocale("en", false);
    TEST_ASSERT(Mgr().TranslatePlural("inventory.items", 1) == "1 item",
        "English singular plural form with count substitution");
    TEST_ASSERT(Mgr().TranslatePlural("inventory.items", 5) == "5 items",
        "English plural form with count substitution");

    Mgr().SetLocale("fr", false);
    TEST_ASSERT(Mgr().TranslatePlural("inventory.items", 1) == "1 objet",
        "French singular plural form");
    TEST_ASSERT(Mgr().TranslatePlural("inventory.items", 3) == "3 objets",
        "French plural form");

    Mgr().SetLocale("en", false);
    return true;
}

bool TestPseudoToggle() {
    TEST_SECTION("Runtime Pseudolocalization Tests");

    Mgr().SetLocale("en", false);
    Mgr().SetPseudolocalizationEnabled(true);
    std::string pseudo = Mgr().Translate("menu.start");
    TEST_ASSERT(pseudo != "Start", "Pseudolocalization alters the translated output");
    TEST_ASSERT(!pseudo.empty() && pseudo.front() == '[',
        "Pseudolocalized output is bracket-wrapped");

    Mgr().SetPseudolocalizationEnabled(false);
    TEST_ASSERT(Mgr().Translate("menu.start") == "Start",
        "Disabling pseudolocalization restores the plain translation");

    return true;
}

} // namespace

void RunLocalizationProjectTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "LOCALIZATION PROJECT TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    lupine_test::SetCurrentSuite("Localization Project");

    if (!lupine_test::TestProject::Create()) {
        lupine_test::RecordFail("Could not create the temp test project");
        std::cout << "  [FAIL] Could not create the temp test project" << std::endl;
        std::cout << "========================================\n" << std::endl;
        return;
    }

    bool allPassed = true;

    allPassed &= TestLoadProject();
    allPassed &= TestBasicTranslate();
    allPassed &= TestFallback();
    allPassed &= TestTableQualified();
    allPassed &= TestFormat();
    allPassed &= TestPlural();
    allPassed &= TestPseudoToggle();

    Mgr().Clear();
    lupine_test::TestProject::Destroy();

    std::cout << "\n========================================" << std::endl;
    if (allPassed) {
        std::cout << "ALL LOCALIZATION PROJECT TESTS PASSED!" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================\n" << std::endl;
}
