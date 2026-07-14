#include "lupine/engine/Engine.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/UUID.hpp"
#include "lupine/core/Serialization.hpp"
#include "TestFramework.hpp"
#include <iostream>
#include <memory>
#include <string>

using namespace lupine;
using namespace lupine::core;

namespace {

bool TestUUIDRoundTrip() {
    TEST_SECTION("UUID Serialization Tests");

    UUID original(0x123456789ABCDEF0ULL);
    std::string str = original.ToString();
    TEST_ASSERT(str.size() == 16, "UUID string is 16 hex characters");

    UUID restored = UUID::FromString(str);
    TEST_ASSERT(restored == original, "UUID survives ToString/FromString round-trip");
    TEST_ASSERT(static_cast<uint64_t>(restored) == 0x123456789ABCDEF0ULL,
        "Restored UUID has the original numeric value");

    UUID zero(0);
    TEST_ASSERT(!zero.IsValid(), "Zero UUID is invalid");
    TEST_ASSERT(UUID(1).IsValid(), "Non-zero UUID is valid");

    UUID a;
    UUID b;
    TEST_ASSERT(a != b, "Two default-constructed UUIDs differ");

    return true;
}

bool TestNodeRoundTrip() {
    TEST_SECTION("Node Round-Trip Tests");

    std::shared_ptr<Node> node = std::make_shared<Node>("Original");
    node->SetActive(false);
    node->AddToGroup("alpha");
    node->AddToGroup("beta");

    nlohmann::json json = node->Serialize();
    TEST_ASSERT(json.contains("uuid"), "Serialized node has a uuid field");
    TEST_ASSERT(json.contains("type"), "Serialized node has a type field");

    std::shared_ptr<Node> restored = std::make_shared<Node>();
    restored->Deserialize(json);

    TEST_ASSERT(restored->GetName() == "Original", "Name survives round-trip");
    TEST_ASSERT(restored->GetUUID() == node->GetUUID(), "UUID survives round-trip");
    TEST_ASSERT(restored->IsInGroup("alpha") && restored->IsInGroup("beta"),
        "Group membership survives round-trip");
    TEST_ASSERT(restored->GetGroups().size() == 2, "Exactly two groups restored");

    return true;
}

bool TestNodeHierarchyRoundTrip() {
    TEST_SECTION("Node Hierarchy Round-Trip Tests");

    std::shared_ptr<Node> root = std::make_shared<Node>("Root");
    std::shared_ptr<Node> child1 = std::make_shared<Node>("Child1");
    std::shared_ptr<Node> child2 = std::make_shared<Node>("Child2");
    std::shared_ptr<Node> grandchild = std::make_shared<Node>("Grandchild");
    root->AddChild(child1);
    root->AddChild(child2);
    child1->AddChild(grandchild);

    nlohmann::json json = root->Serialize();

    std::shared_ptr<Node> restored = std::make_shared<Node>();
    restored->Deserialize(json);

    TEST_ASSERT(restored->GetChildCount() == 2, "Root keeps both children after round-trip");
    std::shared_ptr<Node> rc1 = restored->GetChild("Child1");
    std::shared_ptr<Node> rc2 = restored->GetChild("Child2");
    TEST_ASSERT(rc1 != nullptr && rc2 != nullptr, "Both named children restored");
    TEST_ASSERT(rc1->GetChild("Grandchild") != nullptr, "Nested grandchild restored");
    TEST_ASSERT(rc1->GetChild("Grandchild")->GetParent() == rc1.get(),
        "Restored grandchild has correct parent pointer");
    TEST_ASSERT(rc1->GetUUID() == child1->GetUUID(), "Child UUID preserved through hierarchy");

    return true;
}

bool TestNode2DRoundTrip() {
    TEST_SECTION("Node2D Transform Round-Trip Tests");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("Sprite");
    node->SetPosition(math::Vec2(123.0f, 456.0f));
    node->SetRotation(1.25f);
    node->SetScale(math::Vec2(2.0f, 3.0f));
    node->SetZIndex(5);

    nlohmann::json json = node->Serialize();
    TEST_ASSERT(json["type"].get<std::string>() == "Node2D", "Node2D serializes its type name");

    std::shared_ptr<Node2D> restored = std::make_shared<Node2D>();
    restored->Deserialize(json);

    TEST_ASSERT(math::Equals(restored->GetPosition().x, 123.0f) &&
                math::Equals(restored->GetPosition().y, 456.0f),
        "Position survives round-trip");
    TEST_ASSERT(math::Equals(restored->GetRotation(), 1.25f), "Rotation survives round-trip");
    TEST_ASSERT(math::Equals(restored->GetScale().x, 2.0f) &&
                math::Equals(restored->GetScale().y, 3.0f),
        "Scale survives round-trip");
    TEST_ASSERT(restored->GetZIndex() == 5, "Z-index survives round-trip");

    return true;
}

bool TestTypeRegistry() {
    TEST_SECTION("Type Registry Tests");

    TypeRegistry& registry = TypeRegistry::GetInstance();
    TEST_ASSERT(registry.IsTypeRegistered("Node"), "Node is registered after engine init");
    TEST_ASSERT(registry.IsTypeRegistered("Node2D"), "Node2D is registered");
    TEST_ASSERT(registry.IsTypeRegistered("Node3D"), "Node3D is registered");
    TEST_ASSERT(!registry.IsTypeRegistered("DoesNotExist"),
        "Unregistered type reports not registered");

    std::shared_ptr<ISerializable> instance = registry.CreateInstance("Node2D");
    TEST_ASSERT(instance != nullptr, "CreateInstance returns an object for a known type");
    TEST_ASSERT(instance->GetTypeName() == "Node2D", "Created instance has the correct type name");

    std::shared_ptr<ISerializable> missing = registry.CreateInstance("DoesNotExist");
    TEST_ASSERT(missing == nullptr, "CreateInstance returns null for an unknown type");

    return true;
}

bool TestSerializerString() {
    TEST_SECTION("Serializer String Round-Trip Tests");

    std::shared_ptr<Node> node = std::make_shared<Node>("StringTest");
    node->AddToGroup("serialized");

    std::string jsonStr = Serializer::SerializeToString(*node, 2);
    TEST_ASSERT(!jsonStr.empty(), "SerializeToString produces output");

    std::shared_ptr<Node> restored = std::make_shared<Node>();
    bool ok = Serializer::DeserializeFromString(*restored, jsonStr);
    TEST_ASSERT(ok, "DeserializeFromString succeeds");
    TEST_ASSERT(restored->GetName() == "StringTest", "Name restored from string round-trip");
    TEST_ASSERT(restored->IsInGroup("serialized"), "Group restored from string round-trip");

    return true;
}

} // namespace

void RunSerializationTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "SERIALIZATION TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    lupine_test::SetCurrentSuite("Serialization");

    engine::InitializeEngine();

    bool allPassed = true;

    allPassed &= TestUUIDRoundTrip();
    allPassed &= TestNodeRoundTrip();
    allPassed &= TestNodeHierarchyRoundTrip();
    allPassed &= TestNode2DRoundTrip();
    allPassed &= TestTypeRegistry();
    allPassed &= TestSerializerString();

    std::cout << "\n========================================" << std::endl;
    if (allPassed) {
        std::cout << "ALL SERIALIZATION TESTS PASSED!" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================\n" << std::endl;
}
