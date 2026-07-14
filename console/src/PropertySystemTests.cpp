#include "lupine/engine/Engine.hpp"
#include "TestFramework.hpp"
#include <iostream>
#include <cassert>

using namespace lupine;
using namespace lupine::engine;
using namespace lupine::core;

// ============================================================================
// Custom Test Component with Properties
// ============================================================================

class CustomTestComponent : public Component {
public:
    CustomTestComponent() : Component("CustomTestComponent") {
        DefineProperties();
        m_PropertiesDefined = true;
    }

    std::string GetTypeName() const override { return "CustomTestComponent"; }

    void DefineProperties() override {
        // Various property types for testing
        DefineProperty(PROPERTY_DEFAULT(health, Int, 100));
        DefineProperty(PROPERTY_DEFAULT(speed, Float, 5.5f));
        DefineProperty(PROPERTY_DEFAULT(name, String, std::string("Player")));
        DefineProperty(PROPERTY_DEFAULT(isAlive, Bool, true));
        DefineProperty(PROPERTY_DEFAULT(position, Vec2, math::Vec2(10.0f, 20.0f)));
        DefineProperty(PROPERTY_DEFAULT(rotation, Vec3, math::Vec3(0.0f, 90.0f, 0.0f)));
        DefineProperty(PROPERTY_DEFAULT(color, Color, math::Color(1.0f, 0.5f, 0.0f, 1.0f)));
        
        // Properties with hints
        DefineProperty(PROPERTY_INT_RANGE(level, 5, 1, 99, 1));
        DefineProperty(PROPERTY_FLOAT_RANGE(damage, 10.0f, 0.0f, 100.0f, 0.5f));
        DefineProperty(PROPERTY_HINT(enemyType, Enum, std::string("Orc"), Enum, "Goblin,Orc,Dragon"));
        
        // Path properties
        DefineProperty(PROPERTY_DEFAULT(targetPath, NodePath, std::string("/root/target")));
        DefineProperty(PROPERTY_DEFAULT(scenePath, ScenePath, std::string("res://scenes/level1.scene")));
    }
};

// Register the test component
REGISTER_COMPONENT_TYPE(CustomTestComponent)

// ============================================================================
// Test Functions
// ============================================================================

bool TestPropertyDescriptor() {
    TEST_SECTION("PropertyDescriptor Tests");

    // Test basic property creation
    PropertyDescriptor intProp("health", PropertyValueType::Int);
    TEST_ASSERT(intProp.name == "health", "Property name is correct");
    TEST_ASSERT(intProp.type == PropertyValueType::Int, "Property type is correct");

    // Test property with default value
    PropertyDescriptor floatProp("speed", PropertyValueType::Float, 5.5f);
    nlohmann::json defaultJson = floatProp.GetDefaultAsJson();
    TEST_ASSERT(defaultJson.is_number_float(), "Default value is float");
    TEST_ASSERT(math::Equals(defaultJson.get<float>(), 5.5f), "Default value is 5.5");

    // Test property with hint
    PropertyHint rangeHint(PropertyHintType::Range, "0,100,1");
    PropertyDescriptor rangedProp("level", PropertyValueType::Int, 50, rangeHint);
    TEST_ASSERT(rangedProp.hint.type == PropertyHintType::Range, "Hint type is Range");
    
    int min = 0, max = 0, step = 0;
    rangedProp.hint.GetRangeValues(min, max, step);
    TEST_ASSERT(min == 0 && max == 100 && step == 1, "Range values parsed correctly");

    // Test enum hint
    PropertyHint enumHint(PropertyHintType::Enum, "Red,Green,Blue");
    auto enumValues = enumHint.GetEnumValues();
    TEST_ASSERT(enumValues.size() == 3, "Enum has 3 values");
    TEST_ASSERT(enumValues[0] == "Red", "First enum value is Red");
    TEST_ASSERT(enumValues[1] == "Green", "Second enum value is Green");
    TEST_ASSERT(enumValues[2] == "Blue", "Third enum value is Blue");

    return true;
}

bool TestPropertySerialization() {
    TEST_SECTION("Property Serialization Tests");

    // Test Vec2 serialization
    PropertyDescriptor vec2Prop("position", PropertyValueType::Vec2, math::Vec2(100.0f, 200.0f));
    nlohmann::json vec2Json = vec2Prop.Serialize();
    TEST_ASSERT(vec2Json.contains("name"), "Serialized property has name");
    TEST_ASSERT(vec2Json.contains("type"), "Serialized property has type");
    TEST_ASSERT(vec2Json.contains("default"), "Serialized property has default");
    
    auto defaultVec2 = vec2Json["default"];
    TEST_ASSERT(math::Equals(defaultVec2["x"].get<float>(), 100.0f), "Vec2 X serialized correctly");
    TEST_ASSERT(math::Equals(defaultVec2["y"].get<float>(), 200.0f), "Vec2 Y serialized correctly");

    // Test deserialization
    PropertyDescriptor deserializedProp = PropertyDescriptor::Deserialize(vec2Json);
    TEST_ASSERT(deserializedProp.name == "position", "Deserialized name matches");
    TEST_ASSERT(deserializedProp.type == PropertyValueType::Vec2, "Deserialized type matches");

    // Test Color serialization
    PropertyDescriptor colorProp("tint", PropertyValueType::Color, 
        math::Color(0.5f, 0.25f, 0.75f, 1.0f));
    nlohmann::json colorJson = colorProp.Serialize();
    auto defaultColor = colorJson["default"];
    TEST_ASSERT(math::Equals(defaultColor["r"].get<float>(), 0.5f), "Color R serialized");
    TEST_ASSERT(math::Equals(defaultColor["g"].get<float>(), 0.25f), "Color G serialized");
    TEST_ASSERT(math::Equals(defaultColor["b"].get<float>(), 0.75f), "Color B serialized");
    TEST_ASSERT(math::Equals(defaultColor["a"].get<float>(), 1.0f), "Color A serialized");

    return true;
}

bool TestComponentProperty() {
    TEST_SECTION("ComponentProperty Tests");

    // Create a property with default value
    PropertyDescriptor desc("damage", PropertyValueType::Float, 25.0f);
    ComponentProperty prop(desc);

    // Test getting default value
    float damage = prop.GetValue<float>();
    TEST_ASSERT(math::Equals(damage, 25.0f), "Property has default value");

    // Test setting value
    prop.SetValue<float>(50.0f);
    damage = prop.GetValue<float>();
    TEST_ASSERT(math::Equals(damage, 50.0f), "Property value can be set");

    // Test reset to default
    prop.ResetToDefault();
    damage = prop.GetValue<float>();
    TEST_ASSERT(math::Equals(damage, 25.0f), "Property resets to default");

    // Test Vec2 property
    PropertyDescriptor vec2Desc("velocity", PropertyValueType::Vec2, math::Vec2(5.0f, 10.0f));
    ComponentProperty vec2Prop(vec2Desc);
    math::Vec2 vel = vec2Prop.GetValue<math::Vec2>();
    TEST_ASSERT(math::Equals(vel.x, 5.0f) && math::Equals(vel.y, 10.0f), 
        "Vec2 property initialized correctly");

    vec2Prop.SetValue(math::Vec2(15.0f, 20.0f));
    vel = vec2Prop.GetValue<math::Vec2>();
    TEST_ASSERT(math::Equals(vel.x, 15.0f) && math::Equals(vel.y, 20.0f), 
        "Vec2 property can be modified");

    return true;
}

bool TestPropertyRegistry() {
    TEST_SECTION("ComponentPropertyRegistry Tests");

    ComponentPropertyRegistry registry;

    // Define multiple properties
    registry.DefineProperty(PROPERTY_DEFAULT(health, Int, 100));
    registry.DefineProperty(PROPERTY_DEFAULT(speed, Float, 5.0f));
    registry.DefineProperty(PROPERTY_DEFAULT(name, String, std::string("Test")));

    TEST_ASSERT(registry.HasProperty("health"), "Registry has health property");
    TEST_ASSERT(registry.HasProperty("speed"), "Registry has speed property");
    TEST_ASSERT(registry.HasProperty("name"), "Registry has name property");
    TEST_ASSERT(!registry.HasProperty("missing"), "Registry doesn't have missing property");

    // Get and modify properties
    ComponentProperty* healthProp = registry.GetProperty("health");
    TEST_ASSERT(healthProp != nullptr, "Can get health property");
    TEST_ASSERT(healthProp->GetValue<int>() == 100, "Health has default value");

    healthProp->SetValue(75);
    TEST_ASSERT(healthProp->GetValue<int>() == 75, "Health value can be modified");

    // Test serialization
    nlohmann::json serialized = registry.SerializeProperties();
    TEST_ASSERT(serialized.contains("health"), "Serialized JSON contains health");
    TEST_ASSERT(serialized["health"].get<int>() == 75, "Serialized health value is correct");

    // Test deserialization
    nlohmann::json newData = nlohmann::json::object();
    newData["health"] = 50;
    newData["speed"] = 7.5f;
    newData["name"] = "Updated";

    registry.DeserializeProperties(newData);
    TEST_ASSERT(registry.GetProperty("health")->GetValue<int>() == 50, 
        "Health deserialized correctly");
    TEST_ASSERT(math::Equals(registry.GetProperty("speed")->GetValue<float>(), 7.5f), 
        "Speed deserialized correctly");
    TEST_ASSERT(registry.GetProperty("name")->GetValue<std::string>() == "Updated", 
        "Name deserialized correctly");

    return true;
}

bool TestCustomComponent() {
    TEST_SECTION("Custom Component with Properties Tests");

    auto component = std::make_shared<CustomTestComponent>();
    
    // Test default values
    TEST_ASSERT(component->GetPropertyValue<int>("health") == 100, 
        "Health has default value");
    TEST_ASSERT(math::Equals(component->GetPropertyValue<float>("speed"), 5.5f), 
        "Speed has default value");
    TEST_ASSERT(component->GetPropertyValue<std::string>("name") == "Player", 
        "Name has default value");
    TEST_ASSERT(component->GetPropertyValue<bool>("isAlive") == true, 
        "IsAlive has default value");

    // Test Vec2 default
    math::Vec2 pos = component->GetPropertyValue<math::Vec2>("position");
    TEST_ASSERT(math::Equals(pos.x, 10.0f) && math::Equals(pos.y, 20.0f), 
        "Position has default value");

    // Test Color default
    math::Color color = component->GetPropertyValue<math::Color>("color");
    TEST_ASSERT(math::Equals(color.r, 1.0f) && math::Equals(color.g, 0.5f), 
        "Color has default value");

    // Test modifying values
    component->SetPropertyValue("health", 75);
    TEST_ASSERT(component->GetPropertyValue<int>("health") == 75, 
        "Health can be modified");

    component->SetPropertyValue("speed", 7.5f);
    TEST_ASSERT(math::Equals(component->GetPropertyValue<float>("speed"), 7.5f), 
        "Speed can be modified");

    component->SetPropertyValue("position", math::Vec2(100.0f, 200.0f));
    pos = component->GetPropertyValue<math::Vec2>("position");
    TEST_ASSERT(math::Equals(pos.x, 100.0f) && math::Equals(pos.y, 200.0f), 
        "Position can be modified");

    // Test property descriptors
    auto descriptors = component->GetPropertyDescriptors();
    TEST_ASSERT(descriptors.size() > 0, "Component has property descriptors");

    // Find the level property and check its hint
    bool foundLevel = false;
    for (const auto& desc : descriptors) {
        if (desc.name == "level") {
            foundLevel = true;
            TEST_ASSERT(desc.hint.type == PropertyHintType::Range, 
                "Level property has Range hint");
            int min, max, step;
            desc.hint.GetRangeValues(min, max, step);
            TEST_ASSERT(min == 1 && max == 99, "Level range is 1-99");
            break;
        }
    }
    TEST_ASSERT(foundLevel, "Level property descriptor found");

    return true;
}

bool TestComponentPropertySerialization() {
    TEST_SECTION("Component Serialization with Properties Tests");

    // Create and configure a component
    auto component = std::make_shared<CustomTestComponent>();
    component->RegisterProperties();
    
    component->SetPropertyValue("health", 50);
    component->SetPropertyValue("speed", 10.0f);
    component->SetPropertyValue("name", std::string("Hero"));
    component->SetPropertyValue("position", math::Vec2(100.0f, 200.0f));

    // Serialize
    nlohmann::json serialized = component->Serialize();
    TEST_ASSERT(serialized.contains("type"), "Serialized component has type");
    TEST_ASSERT(serialized.contains("custom_properties"), 
        "Serialized component has custom_properties");

    std::string jsonStr = serialized.dump(2);
    TEST_ASSERT(!jsonStr.empty(), "Serialization produces output");
    std::cout << "  Serialized component:\n" << jsonStr.substr(0, 200) << "..." << std::endl;

    // Create a new component and deserialize
    auto loadedComponent = std::make_shared<CustomTestComponent>();
    loadedComponent->Deserialize(serialized);

    // Verify values
    TEST_ASSERT(loadedComponent->GetPropertyValue<int>("health") == 50, 
        "Deserialized health is correct");
    TEST_ASSERT(math::Equals(loadedComponent->GetPropertyValue<float>("speed"), 10.0f), 
        "Deserialized speed is correct");
    TEST_ASSERT(loadedComponent->GetPropertyValue<std::string>("name") == "Hero", 
        "Deserialized name is correct");

    math::Vec2 loadedPos = loadedComponent->GetPropertyValue<math::Vec2>("position");
    TEST_ASSERT(math::Equals(loadedPos.x, 100.0f) && math::Equals(loadedPos.y, 200.0f), 
        "Deserialized position is correct");

    return true;
}

// ============================================================================
// Main Test Runner
// ============================================================================

void RunPropertySystemTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "COMPONENT PROPERTY SYSTEM TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    lupine_test::SetCurrentSuite("Component Property System");
    bool allPassed = true;

    allPassed &= TestPropertyDescriptor();
    allPassed &= TestPropertySerialization();
    allPassed &= TestComponentProperty();
    allPassed &= TestPropertyRegistry();
    allPassed &= TestCustomComponent();
    allPassed &= TestComponentPropertySerialization();

    std::cout << "\n========================================" << std::endl;
    if (allPassed) {
        std::cout << "ALL PROPERTY SYSTEM TESTS PASSED!" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================\n" << std::endl;
}
