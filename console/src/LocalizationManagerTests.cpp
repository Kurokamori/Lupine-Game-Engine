#include "lupine/localization/LocalizationManager.hpp"
#include "lupine/localization/PluralRules.hpp"
#include "TestFramework.hpp"
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>

using namespace lupine;
using namespace lupine::localization;

namespace {

bool TestSubstitute() {
    TEST_SECTION("Format Substitution Tests");

    std::unordered_map<std::string, std::string> args = {
        { "name", "Hero" },
        { "count", "3" }
    };

    TEST_ASSERT(LocalizationManager::Substitute("Hello {name}!", args) == "Hello Hero!",
        "Single placeholder is substituted");
    TEST_ASSERT(LocalizationManager::Substitute("{name} has {count} items", args) == "Hero has 3 items",
        "Multiple placeholders are substituted");
    TEST_ASSERT(LocalizationManager::Substitute("No placeholders here", args) == "No placeholders here",
        "Text without placeholders is unchanged");
    TEST_ASSERT(LocalizationManager::Substitute("Unknown {missing}", args) == "Unknown {missing}",
        "Unknown placeholder is left intact");
    TEST_ASSERT(LocalizationManager::Substitute("Literal {{name}}", args) == "Literal {name}",
        "Escaped braces produce literal braces");
    TEST_ASSERT(LocalizationManager::Substitute("Unclosed {name", args) == "Unclosed {name",
        "Unclosed brace is left as-is");

    return true;
}

bool TestPseudo() {
    TEST_SECTION("Pseudolocalization Tests");

    std::string out = LocalizationManager::Pseudo("Hi");
    TEST_ASSERT(!out.empty(), "Pseudo produces output");
    TEST_ASSERT(out.front() == '[', "Pseudo output is bracket-wrapped at the start");
    TEST_ASSERT(out.back() == ']', "Pseudo output is bracket-wrapped at the end");
    TEST_ASSERT(out.size() > std::string("Hi").size(),
        "Pseudo output is longer than the source (padding + accents)");

    std::string withArg = LocalizationManager::Pseudo("Hello {name}");
    TEST_ASSERT(withArg.find("{name}") != std::string::npos,
        "Pseudo preserves placeholder names verbatim");

    return true;
}

bool TestPluralEnglish() {
    TEST_SECTION("English Plural Category Tests");

    TEST_ASSERT(PluralRules::Category("en", 1) == "one", "English 1 is 'one'");
    TEST_ASSERT(PluralRules::Category("en", 0) == "other", "English 0 is 'other'");
    TEST_ASSERT(PluralRules::Category("en", 2) == "other", "English 2 is 'other'");
    TEST_ASSERT(PluralRules::Category("en", 100) == "other", "English 100 is 'other'");
    TEST_ASSERT(PluralRules::Category("en-US", 1) == "one",
        "Region subtag is ignored (en-US behaves like en)");

    return true;
}

bool TestPluralFrench() {
    TEST_SECTION("French Plural Category Tests");

    TEST_ASSERT(PluralRules::Category("fr", 0) == "one", "French 0 is 'one'");
    TEST_ASSERT(PluralRules::Category("fr", 1) == "one", "French 1 is 'one'");
    TEST_ASSERT(PluralRules::Category("fr", 2) == "other", "French 2 is 'other'");

    return true;
}

bool TestPluralRussian() {
    TEST_SECTION("Russian Plural Category Tests");

    TEST_ASSERT(PluralRules::Category("ru", 1) == "one", "Russian 1 is 'one'");
    TEST_ASSERT(PluralRules::Category("ru", 21) == "one", "Russian 21 is 'one'");
    TEST_ASSERT(PluralRules::Category("ru", 11) == "many", "Russian 11 is 'many' (teen exception)");
    TEST_ASSERT(PluralRules::Category("ru", 2) == "few", "Russian 2 is 'few'");
    TEST_ASSERT(PluralRules::Category("ru", 3) == "few", "Russian 3 is 'few'");
    TEST_ASSERT(PluralRules::Category("ru", 5) == "many", "Russian 5 is 'many'");
    TEST_ASSERT(PluralRules::Category("ru", 12) == "many", "Russian 12 is 'many'");

    return true;
}

bool TestPluralAsian() {
    TEST_SECTION("No-Plural Language Tests");

    TEST_ASSERT(PluralRules::Category("ja", 1) == "other", "Japanese 1 is 'other'");
    TEST_ASSERT(PluralRules::Category("ja", 5) == "other", "Japanese 5 is 'other'");
    TEST_ASSERT(PluralRules::Category("zh", 0) == "other", "Chinese 0 is 'other'");

    return true;
}

bool TestPluralArabic() {
    TEST_SECTION("Arabic Plural Category Tests");

    TEST_ASSERT(PluralRules::Category("ar", 0) == "zero", "Arabic 0 is 'zero'");
    TEST_ASSERT(PluralRules::Category("ar", 1) == "one", "Arabic 1 is 'one'");
    TEST_ASSERT(PluralRules::Category("ar", 2) == "two", "Arabic 2 is 'two'");
    TEST_ASSERT(PluralRules::Category("ar", 3) == "few", "Arabic 3 is 'few'");
    TEST_ASSERT(PluralRules::Category("ar", 11) == "many", "Arabic 11 is 'many'");

    return true;
}

bool TestPluralUnknownLocale() {
    TEST_SECTION("Unknown Locale Fallback Tests");

    TEST_ASSERT(PluralRules::Category("xx", 1) == "one",
        "Unknown locale falls back to English-style 'one' for 1");
    TEST_ASSERT(PluralRules::Category("xx", 5) == "other",
        "Unknown locale falls back to 'other' for non-1");

    return true;
}

bool TestPluralNegativeCounts() {
    TEST_SECTION("Negative Count Tests");

    TEST_ASSERT(PluralRules::Category("en", -1) == "one",
        "English -1 uses absolute value (one)");
    TEST_ASSERT(PluralRules::Category("ru", -21) == "one",
        "Russian -21 uses absolute value (one)");

    return true;
}

bool TestCategoriesForLocale() {
    TEST_SECTION("CategoriesForLocale Tests");

    std::vector<std::string> en = PluralRules::CategoriesForLocale("en");
    TEST_ASSERT(!en.empty(), "English category list is non-empty");
    TEST_ASSERT(std::find(en.begin(), en.end(), "other") != en.end(),
        "English category list always contains 'other'");

    std::vector<std::string> ar = PluralRules::CategoriesForLocale("ar");
    TEST_ASSERT(ar.size() >= en.size(),
        "Arabic exposes at least as many categories as English");
    TEST_ASSERT(std::find(ar.begin(), ar.end(), "zero") != ar.end(),
        "Arabic category list contains 'zero'");

    return true;
}

} // namespace

void RunLocalizationManagerTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "LOCALIZATION MANAGER / PLURAL TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    lupine_test::SetCurrentSuite("Localization Manager / Plurals");
    bool allPassed = true;

    allPassed &= TestSubstitute();
    allPassed &= TestPseudo();
    allPassed &= TestPluralEnglish();
    allPassed &= TestPluralFrench();
    allPassed &= TestPluralRussian();
    allPassed &= TestPluralAsian();
    allPassed &= TestPluralArabic();
    allPassed &= TestPluralUnknownLocale();
    allPassed &= TestPluralNegativeCounts();
    allPassed &= TestCategoriesForLocale();

    std::cout << "\n========================================" << std::endl;
    if (allPassed) {
        std::cout << "ALL LOCALIZATION MANAGER TESTS PASSED!" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================\n" << std::endl;
}
