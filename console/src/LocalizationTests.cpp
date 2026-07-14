#include "lupine/localization/LocalizationTable.hpp"
#include "TestFramework.hpp"
#include <iostream>
#include <string>

using namespace lupine;
using namespace lupine::localization;

namespace {

const char* kSampleJson = R"({
    "name": "ui",
    "locales": ["en", "fr"],
    "entries": [
        {
            "key": "menu.start",
            "tags": ["menu", "button"],
            "comment": "Start button label",
            "values": { "en": "Start", "fr": "Demarrer" }
        },
        {
            "key": "menu.quit",
            "tags": ["menu"],
            "values": { "en": "Quit", "fr": "Quitter" }
        },
        {
            "key": "inventory.items",
            "values": { "en": "items" },
            "plurals": {
                "en": { "one": "1 item", "other": "{n} items" }
            }
        }
    ]
})";

bool TestJsonLoad() {
    TEST_SECTION("JSON Load Tests");

    LocalizationTable table;
    bool loaded = table.LoadFromJsonText(kSampleJson);
    TEST_ASSERT(loaded, "Table loads from JSON text");
    TEST_ASSERT(table.GetName() == "ui", "Table name parsed from JSON");
    TEST_ASSERT(table.GetEntries().size() == 3, "All three entries loaded");

    const std::vector<std::string>& locales = table.GetLocales();
    TEST_ASSERT(locales.size() == 2, "Two locales declared");
    bool hasEn = false, hasFr = false;
    for (const std::string& loc : locales) {
        if (loc == "en") hasEn = true;
        if (loc == "fr") hasFr = true;
    }
    TEST_ASSERT(hasEn && hasFr, "Both en and fr locales present");

    return true;
}

bool TestLookup() {
    TEST_SECTION("Lookup Tests");

    LocalizationTable table;
    table.LoadFromJsonText(kSampleJson);

    TEST_ASSERT(table.HasKey("menu.start"), "HasKey is true for an existing key");
    TEST_ASSERT(!table.HasKey("menu.missing"), "HasKey is false for a missing key");

    const LocEntry* entry = table.Find("menu.start");
    TEST_ASSERT(entry != nullptr, "Find returns an entry for an existing key");
    TEST_ASSERT(entry->GetValue("en") == "Start", "English value is correct");
    TEST_ASSERT(entry->GetValue("fr") == "Demarrer", "French value is correct");
    TEST_ASSERT(entry->GetValue("de").empty(), "Missing locale returns empty string");
    TEST_ASSERT(entry->HasValue("en"), "HasValue is true for a present locale");
    TEST_ASSERT(!entry->HasValue("de"), "HasValue is false for an absent locale");

    TEST_ASSERT(table.Find("menu.missing") == nullptr, "Find returns nullptr for missing key");

    return true;
}

bool TestTags() {
    TEST_SECTION("Tag Tests");

    LocalizationTable table;
    table.LoadFromJsonText(kSampleJson);

    const LocEntry* start = table.Find("menu.start");
    TEST_ASSERT(start != nullptr, "Found menu.start");
    TEST_ASSERT(start->tags.size() == 2, "menu.start has two tags");
    TEST_ASSERT(start->HasTag("menu"), "menu.start has 'menu' tag");
    TEST_ASSERT(start->HasTag("button"), "menu.start has 'button' tag");
    TEST_ASSERT(!start->HasTag("hud"), "menu.start does not have 'hud' tag");
    TEST_ASSERT(start->comment == "Start button label", "Comment parsed correctly");

    return true;
}

bool TestPlurals() {
    TEST_SECTION("Plural Tests");

    LocalizationTable table;
    table.LoadFromJsonText(kSampleJson);

    const LocEntry* items = table.Find("inventory.items");
    TEST_ASSERT(items != nullptr, "Found inventory.items");

    std::map<std::string, std::map<std::string, std::string>>::const_iterator it =
        items->plurals.find("en");
    TEST_ASSERT(it != items->plurals.end(), "English plural forms present");
    TEST_ASSERT(it->second.at("one") == "1 item", "Singular plural form correct");
    TEST_ASSERT(it->second.at("other") == "{n} items", "Plural 'other' form correct");

    return true;
}

bool TestSetEntry() {
    TEST_SECTION("SetEntry Tests");

    LocalizationTable table;
    table.LoadFromJsonText(kSampleJson);
    size_t before = table.GetEntries().size();

    LocEntry fresh;
    fresh.key = "menu.options";
    fresh.values["en"] = "Options";
    table.SetEntry(fresh);

    TEST_ASSERT(table.GetEntries().size() == before + 1, "New entry increases entry count");
    TEST_ASSERT(table.HasKey("menu.options"), "New key is findable after SetEntry");
    TEST_ASSERT(table.Find("menu.options")->GetValue("en") == "Options", "New entry value correct");

    LocEntry replacement;
    replacement.key = "menu.start";
    replacement.values["en"] = "Begin";
    table.SetEntry(replacement);

    TEST_ASSERT(table.GetEntries().size() == before + 1, "Replacing an existing key does not grow count");
    TEST_ASSERT(table.Find("menu.start")->GetValue("en") == "Begin", "Existing entry replaced in place");

    return true;
}

bool TestCsvEscaping() {
    TEST_SECTION("CSV Escaping Tests");

    TEST_ASSERT(LocalizationTable::EscapeCsvField("plain") == "plain",
        "Plain field is not quoted");
    TEST_ASSERT(LocalizationTable::EscapeCsvField("a,b") == "\"a,b\"",
        "Field with comma is quoted");
    TEST_ASSERT(LocalizationTable::EscapeCsvField("say \"hi\"") == "\"say \"\"hi\"\"\"",
        "Field with quotes is escaped and quoted");
    TEST_ASSERT(LocalizationTable::EscapeCsvField("line1\nline2") == "\"line1\nline2\"",
        "Field with newline is quoted");

    return true;
}

bool TestCsvParse() {
    TEST_SECTION("CSV Parse Tests");

    const std::string csv = "Key,Tags,en\nmenu.start,menu;button,Start\n\"with,comma\",,\"a \"\"quoted\"\" value\"\n";
    std::vector<std::vector<std::string>> rows = LocalizationTable::ParseCsv(csv);

    TEST_ASSERT(rows.size() == 3, "Three rows parsed (blank line skipped)");
    TEST_ASSERT(rows[0].size() == 3, "Header has three columns");
    TEST_ASSERT(rows[0][0] == "Key", "First header column is Key");
    TEST_ASSERT(rows[1][0] == "menu.start", "Second row key parsed");
    TEST_ASSERT(rows[1][2] == "Start", "Second row value parsed");
    TEST_ASSERT(rows[2][0] == "with,comma", "Quoted comma field parsed as single field");
    TEST_ASSERT(rows[2][2] == "a \"quoted\" value", "Escaped quotes unescaped on parse");

    return true;
}

bool TestCsvRoundTrip() {
    TEST_SECTION("CSV Round-Trip Tests");

    LocalizationTable table;
    table.LoadFromJsonText(kSampleJson);

    std::string csvText = table.ToCsvText();
    TEST_ASSERT(!csvText.empty(), "ToCsvText produces output");

    LocalizationTable reloaded;
    bool ok = reloaded.LoadFromCsvText(csvText);
    TEST_ASSERT(ok, "Reload from generated CSV succeeds");
    TEST_ASSERT(reloaded.HasKey("menu.start"), "Key survives CSV round-trip");

    const LocEntry* entry = reloaded.Find("menu.start");
    TEST_ASSERT(entry != nullptr, "Entry found after CSV round-trip");
    TEST_ASSERT(entry->GetValue("en") == "Start", "English value survives CSV round-trip");
    TEST_ASSERT(entry->HasTag("menu"), "Tag survives CSV round-trip");

    return true;
}

bool TestJsonRoundTrip() {
    TEST_SECTION("JSON Round-Trip Tests");

    LocalizationTable table;
    table.LoadFromJsonText(kSampleJson);

    std::string jsonText = table.ToJsonText();
    TEST_ASSERT(!jsonText.empty(), "ToJsonText produces output");

    LocalizationTable reloaded;
    bool ok = reloaded.LoadFromJsonText(jsonText);
    TEST_ASSERT(ok, "Reload from generated JSON succeeds");
    TEST_ASSERT(reloaded.GetEntries().size() == 3, "All entries survive JSON round-trip");
    TEST_ASSERT(reloaded.Find("menu.quit")->GetValue("fr") == "Quitter",
        "Locale value survives JSON round-trip");

    return true;
}

} // namespace

void RunLocalizationTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "LOCALIZATION SYSTEM TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    lupine_test::SetCurrentSuite("Localization System");
    bool allPassed = true;

    allPassed &= TestJsonLoad();
    allPassed &= TestLookup();
    allPassed &= TestTags();
    allPassed &= TestPlurals();
    allPassed &= TestSetEntry();
    allPassed &= TestCsvEscaping();
    allPassed &= TestCsvParse();
    allPassed &= TestCsvRoundTrip();
    allPassed &= TestJsonRoundTrip();

    std::cout << "\n========================================" << std::endl;
    if (allPassed) {
        std::cout << "ALL LOCALIZATION TESTS PASSED!" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================\n" << std::endl;
}
