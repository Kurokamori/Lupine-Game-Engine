#include "lupine/core/ScriptAnnotationParser.hpp"
#include "TestFramework.hpp"
#include <iostream>
#include <string>

using namespace lupine;
using namespace lupine::core;

namespace {

const PropertyDescriptor* FindProp(const ScriptExportParseResult& result, const std::string& name) {
    for (const PropertyDescriptor& d : result.properties) {
        if (d.name == name) {
            return &d;
        }
    }
    return nullptr;
}

bool HasUsage(const PropertyDescriptor& d, PropertyUsageFlags flag) {
    return (d.usageFlags & static_cast<uint32_t>(flag)) != 0u;
}

// A Lua script exercising the full attribute vocabulary: block-annotation-above with
// both bracket [Tag] and legacy @tag forms, prose docstrings, an inline @struct, a
// custom widget, and class-filtered reference hints.
const char* kLuaScript = R"LUA(
--@component_class "Demo"

--@struct Stats { hp:int=10, mp:int=5, label:string="hero" }

--- Top movement speed in pixels per second.
--[Header("Movement")]
--[Range(0, 1000, 0.5)]
--[Suffix("px/s")]
--@export move_speed float 200.0

--[Tooltip("Internal cached value")]
--[HideInInspector]
--@export _cached int 0

--[ReadOnly]
--@export computed_label string "n/a"

--@export base_stats Stats

--[Enum(physical, fire, ice)]
--@export element enum "fire" 1

--[Custom("slider", "{\"min\": 0, \"max\": 11}")]
--@export volume float 5.0

--[NodeType("Sprite2D")]
--@export target nodepath

--[ArchetypeType("Weapon")]
--@export weapon resource

--@export legacy_speed float 1.0 @range "0,10,0.1" @group "Legacy" @desc "old style"
)LUA";

bool TestLuaFullVocabulary() {
    TEST_SECTION("ScriptAnnotationParser: full Lua vocabulary");

    ScriptExportParseResult result = ScriptAnnotationParser::ParseExports(kLuaScript, "--");

    TEST_ASSERT(result.structs.count("Stats") == 1, "Struct 'Stats' was registered");
    TEST_ASSERT(result.structs["Stats"].size() == 3, "Struct 'Stats' has 3 fields");

    const PropertyDescriptor* moveSpeed = FindProp(result, "move_speed");
    TEST_ASSERT(moveSpeed != nullptr, "move_speed parsed");
    TEST_ASSERT(moveSpeed->type == PropertyValueType::Float, "move_speed is Float");
    TEST_ASSERT(moveSpeed->hint.type == PropertyHintType::Range, "move_speed has a Range hint");
    TEST_ASSERT(moveSpeed->headerText == "Movement", "move_speed header is 'Movement'");
    TEST_ASSERT(moveSpeed->suffix == "px/s", "move_speed suffix is 'px/s'");
    TEST_ASSERT(moveSpeed->description == "Top movement speed in pixels per second.",
                "move_speed prose became the description");

    const PropertyDescriptor* cached = FindProp(result, "_cached");
    TEST_ASSERT(cached != nullptr && HasUsage(*cached, PropertyUsageFlags::Hidden),
                "_cached has the Hidden usage flag");
    TEST_ASSERT(cached->description == "Internal cached value",
                "[Tooltip] overrides the (absent) prose description");

    const PropertyDescriptor* computed = FindProp(result, "computed_label");
    TEST_ASSERT(computed != nullptr && HasUsage(*computed, PropertyUsageFlags::ReadOnly),
                "computed_label has the ReadOnly usage flag");

    const PropertyDescriptor* baseStats = FindProp(result, "base_stats");
    TEST_ASSERT(baseStats != nullptr, "base_stats parsed");
    TEST_ASSERT(baseStats->type == PropertyValueType::Dictionary,
                "struct-typed export is stored as a Dictionary");
    TEST_ASSERT(baseStats->objectTypeName == "Stats", "base_stats objectTypeName is 'Stats'");
    TEST_ASSERT(baseStats->objectSchema.size() == 3, "base_stats carries the 3-field schema");

    const PropertyDescriptor* element = FindProp(result, "element");
    TEST_ASSERT(element != nullptr && element->type == PropertyValueType::Enum,
                "element is an Enum");
    TEST_ASSERT(element->hint.type == PropertyHintType::Enum &&
                    element->hint.GetEnumValues().size() == 3,
                "element enum options parsed (3 values)");

    const PropertyDescriptor* volume = FindProp(result, "volume");
    TEST_ASSERT(volume != nullptr && volume->customWidget == "slider",
                "volume declares the 'slider' custom widget");
    TEST_ASSERT(volume->customWidgetConfig.find("max") != std::string::npos,
                "volume custom widget config round-trips JSON");

    const PropertyDescriptor* target = FindProp(result, "target");
    TEST_ASSERT(target != nullptr && target->hint.type == PropertyHintType::NodeType &&
                    target->hint.hintString == "Sprite2D",
                "target uses a NodeType hint filtered to Sprite2D");

    const PropertyDescriptor* weapon = FindProp(result, "weapon");
    TEST_ASSERT(weapon != nullptr && weapon->hint.type == PropertyHintType::ArchetypeType &&
                    weapon->hint.hintString == "Weapon",
                "weapon uses an ArchetypeType hint filtered to Weapon");

    const PropertyDescriptor* legacy = FindProp(result, "legacy_speed");
    TEST_ASSERT(legacy != nullptr && legacy->hint.type == PropertyHintType::Range,
                "legacy trailing @range still parses");
    TEST_ASSERT(legacy->group == "Legacy" && legacy->description == "old style",
                "legacy trailing @group/@desc still parse");

    return true;
}

// The same export grammar must work with the '#' comment prefix (Python/Ruby).
const char* kPythonScript = R"PY(
#@component_class "Demo"

#[Range(0, 100, 1)]
#@export health int 100

#[ReadOnly]
#@export tag string "enemy"
)PY";

bool TestPythonPrefix() {
    TEST_SECTION("ScriptAnnotationParser: '#' comment prefix");

    ScriptExportParseResult result = ScriptAnnotationParser::ParseExports(kPythonScript, "#");

    const PropertyDescriptor* health = FindProp(result, "health");
    TEST_ASSERT(health != nullptr && health->type == PropertyValueType::Int,
                "health (int) parsed under '#'");
    TEST_ASSERT(health->hint.type == PropertyHintType::Range, "health has a Range hint under '#'");

    const PropertyDescriptor* tag = FindProp(result, "tag");
    TEST_ASSERT(tag != nullptr && HasUsage(*tag, PropertyUsageFlags::ReadOnly),
                "tag ReadOnly flag parsed under '#'");

    return true;
}

// Backward-compatibility: a bare legacy export with no attributes still works and the
// serialized descriptor omits the new keys.
bool TestBackwardCompatAndSerialize() {
    TEST_SECTION("ScriptAnnotationParser: backward compatibility + serialize");

    ScriptExportParseResult result =
        ScriptAnnotationParser::ParseExports("--@export hp int 5\n", "--");
    const PropertyDescriptor* hp = FindProp(result, "hp");
    TEST_ASSERT(hp != nullptr && hp->type == PropertyValueType::Int, "bare export parses");
    TEST_ASSERT(hp->usageFlags == 0 && hp->headerText.empty() && hp->customWidget.empty(),
                "bare export carries no extended metadata");

    nlohmann::json json = hp->Serialize();
    TEST_ASSERT(!json.contains("usage") && !json.contains("header") &&
                    !json.contains("custom_widget") && !json.contains("object_schema"),
                "Serialize omits absent extended keys");

    // A rich descriptor round-trips through Serialize/Deserialize.
    const PropertyDescriptor* richResult = nullptr;
    ScriptExportParseResult rich = ScriptAnnotationParser::ParseExports(kLuaScript, "--");
    richResult = FindProp(rich, "move_speed");
    TEST_ASSERT(richResult != nullptr, "rich descriptor present");
    PropertyDescriptor restored = PropertyDescriptor::Deserialize(richResult->Serialize());
    TEST_ASSERT(restored.headerText == "Movement" && restored.suffix == "px/s",
                "header/suffix round-trip through Serialize/Deserialize");

    PropertyDescriptor structRestored =
        PropertyDescriptor::Deserialize(FindProp(rich, "base_stats")->Serialize());
    TEST_ASSERT(structRestored.objectSchema.size() == 3,
                "object_schema round-trips through Serialize/Deserialize");

    return true;
}

} // namespace

void RunScriptAnnotationParserTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "SCRIPT ANNOTATION PARSER TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    lupine_test::SetCurrentSuite("Script Annotation Parser");
    bool allPassed = true;

    allPassed &= TestLuaFullVocabulary();
    allPassed &= TestPythonPrefix();
    allPassed &= TestBackwardCompatAndSerialize();

    std::cout << "\n========================================" << std::endl;
    if (allPassed) {
        std::cout << "ALL SCRIPT ANNOTATION PARSER TESTS PASSED!" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================" << std::endl;
}
