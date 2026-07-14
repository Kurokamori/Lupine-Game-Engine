#include "lupine/engine/Engine.hpp"
#include "TestFramework.hpp"
#include <iostream>
#include <cassert>

using namespace lupine;
using namespace lupine::engine;
using namespace lupine::core;

// Custom test component
class TestComponent : public Component {
public:
    TestComponent() : Component("TestComponent"), value(42) {}
    
    std::string GetTypeName() const override { return "TestComponent"; }
    
    void RegisterProperties() override {
        Component::RegisterProperties();
        RegisterProperty<int>("value", core::PropertyType::Int,
            [this]() { return value; },
            [this](const int& v) { value = v; });
    }
    
    void OnAwake() override {
        awakeCallCount++;
    }
    
    void OnReady() override {
        readyCallCount++;
    }
    
    void OnProcess(float) override {
        processCallCount++;
    }
    
    int value;
    int awakeCallCount = 0;
    int readyCallCount = 0;
    int processCallCount = 0;
};

// Register the test component
REGISTER_COMPONENT_TYPE(TestComponent)

bool TestSerialization() {
    TEST_SECTION("Serialization System Tests");
    
    // Test basic property serialization
    auto node = std::make_shared<Node2D>("TestNode");
    node->RegisterProperties();
    node->SetPosition(math::Vec2(100.0f, 200.0f));
    node->SetRotation(45.0f);
    node->SetScale(math::Vec2(2.0f, 2.0f));
    
    // Serialize
    std::string jsonStr = core::Serializer::SerializeToString(*node, 2);
    TEST_ASSERT(!jsonStr.empty(), "Node serialization produces output");
    
    // Deserialize
    auto loadedNode = std::make_shared<Node2D>();
    loadedNode->RegisterProperties();
    bool success = core::Serializer::DeserializeFromString(*loadedNode, jsonStr);
    TEST_ASSERT(success, "Node deserialization succeeds");
    TEST_ASSERT(loadedNode->GetName() == "TestNode", "Node name preserved");
    TEST_ASSERT(math::Equals(loadedNode->GetPosition().x, 100.0f), "Position X preserved");
    TEST_ASSERT(math::Equals(loadedNode->GetPosition().y, 200.0f), "Position Y preserved");
    TEST_ASSERT(math::Equals(loadedNode->GetRotation(), 45.0f), "Rotation preserved");
    
    return true;
}

bool TestNodeHierarchy() {
    TEST_SECTION("Node Hierarchy Tests");
    
    // Create node hierarchy
    auto root = std::make_shared<Node>("Root");
    auto child1 = std::make_shared<Node>("Child1");
    auto child2 = std::make_shared<Node>("Child2");
    auto grandchild = std::make_shared<Node>("Grandchild");
    
    root->AddChild(child1);
    root->AddChild(child2);
    child1->AddChild(grandchild);
    
    TEST_ASSERT(root->GetChildCount() == 2, "Root has 2 children");
    TEST_ASSERT(child1->GetChildCount() == 1, "Child1 has 1 child");
    TEST_ASSERT(child1->GetParent() == root.get(), "Child1 parent is root");
    TEST_ASSERT(grandchild->GetParent() == child1.get(), "Grandchild parent is child1");
    
    // Test finding nodes
    auto found = root->GetChild("Child1");
    TEST_ASSERT(found != nullptr, "Can find child by name");
    TEST_ASSERT(found->GetName() == "Child1", "Found correct child");
    
    auto foundPath = root->FindNode("Child1/Grandchild");
    TEST_ASSERT(foundPath != nullptr, "Can find node by path");
    TEST_ASSERT(foundPath->GetName() == "Grandchild", "Found correct node by path");
    
    // Test active/visible hierarchy
    root->SetActive(true);
    child1->SetActive(true);
    TEST_ASSERT(grandchild->IsActiveInHierarchy(), "Grandchild is active in hierarchy");
    
    child1->SetActive(false);
    TEST_ASSERT(!grandchild->IsActiveInHierarchy(), "Grandchild is inactive when parent is inactive");
    
    return true;
}

bool TestComponents() {
    TEST_SECTION("Component System Tests");
    
    auto node = std::make_shared<Node>("TestNode");
    auto component = std::make_shared<TestComponent>();
    
    node->AddComponent(component);
    TEST_ASSERT(node->GetComponents().size() == 1, "Component added to node");
    TEST_ASSERT(component->GetOwner() == node.get(), "Component owner set correctly");
    
    // Test component retrieval
    auto retrieved = node->GetComponent<TestComponent>();
    TEST_ASSERT(retrieved != nullptr, "Can retrieve component by type");
    TEST_ASSERT(retrieved->value == 42, "Component has correct initial value");
    
    // Test component lifecycle
    node->OnReady();
    TEST_ASSERT(component->awakeCallCount == 1, "OnAwake called once");
    TEST_ASSERT(component->readyCallCount == 1, "OnReady called once");
    
    node->OnProcess(0.016f);
    TEST_ASSERT(component->processCallCount == 1, "OnProcess called once");
    
    return true;
}

bool TestComponentSerialization() {
    TEST_SECTION("Component Serialization Tests");
    
    auto node = std::make_shared<Node>("NodeWithComponent");
    node->RegisterProperties();
    
    auto component = std::make_shared<TestComponent>();
    component->RegisterProperties();
    component->value = 123;
    node->AddComponent(component);
    
    // Serialize
    std::string jsonStr = core::Serializer::SerializeToString(*node, 2);
    TEST_ASSERT(!jsonStr.empty(), "Node with component serializes");
    
    // Deserialize
    auto loadedNode = std::make_shared<Node>();
    loadedNode->RegisterProperties();
    bool success = core::Serializer::DeserializeFromString(*loadedNode, jsonStr);
    TEST_ASSERT(success, "Node with component deserializes");
    
    auto loadedComponent = loadedNode->GetComponent<TestComponent>();
    TEST_ASSERT(loadedComponent != nullptr, "Component restored after deserialization");
    TEST_ASSERT(loadedComponent->value == 123, "Component property value preserved");

    return true;
}

bool TestSceneManagement() {
    TEST_SECTION("Scene Management Tests");

    // Create a scene with hierarchy
    auto scene = std::make_shared<Scene>("TestScene");
    scene->RegisterProperties();

    auto root = std::make_shared<Node2D>("SceneRoot");
    root->RegisterProperties();
    root->SetPosition(math::Vec2(50.0f, 50.0f));

    auto child = std::make_shared<Node2D>("Child");
    child->RegisterProperties();
    child->SetPosition(math::Vec2(10.0f, 10.0f));

    root->AddChild(child);
    scene->SetRoot(root);

    TEST_ASSERT(scene->GetRoot() != nullptr, "Scene has root node");
    TEST_ASSERT(scene->GetRoot()->GetName() == "SceneRoot", "Scene root is correct");

    // Test scene initialization
    scene->Initialize();
    TEST_ASSERT(scene->IsInitialized(), "Scene is initialized");

    // Test finding nodes
    auto foundChild = scene->FindNode("Child");
    TEST_ASSERT(foundChild != nullptr, "Can find child node in scene");

    // Test scene serialization
    std::string jsonStr = core::Serializer::SerializeToString(*scene, 2);
    TEST_ASSERT(!jsonStr.empty(), "Scene serializes to JSON");

    // Test scene deserialization
    auto loadedScene = std::make_shared<Scene>();
    loadedScene->RegisterProperties();
    bool success = core::Serializer::DeserializeFromString(*loadedScene, jsonStr);
    TEST_ASSERT(success, "Scene deserializes from JSON");
    TEST_ASSERT(loadedScene->GetName() == "TestScene", "Scene name preserved");
    TEST_ASSERT(loadedScene->GetRoot() != nullptr, "Scene root restored");
    TEST_ASSERT(loadedScene->GetRoot()->GetChildCount() == 1, "Scene hierarchy preserved");

    auto loadedRoot = std::dynamic_pointer_cast<Node2D>(loadedScene->GetRoot());
    TEST_ASSERT(loadedRoot != nullptr, "Root node is Node2D");
    TEST_ASSERT(math::Equals(loadedRoot->GetPosition().x, 50.0f), "Root position preserved");

    return true;
}

bool TestSceneFiles() {
    TEST_SECTION("Scene File I/O Tests");

    // Create a test scene
    auto scene = std::make_shared<Scene>("FileTestScene");
    scene->RegisterProperties();

    auto root = std::make_shared<Node3D>("Root3D");
    root->RegisterProperties();
    root->SetPosition(math::Vec3(1.0f, 2.0f, 3.0f));
    root->SetScale(math::Vec3(2.0f, 2.0f, 2.0f));

    scene->SetRoot(root);

    // Save to file
    std::string filepath = "test_scene.scene";
    bool saveSuccess = scene->Save(filepath);
    TEST_ASSERT(saveSuccess, "Scene saves to file");

    // Load from file
    auto loadedScene = std::make_shared<Scene>();
    bool loadSuccess = loadedScene->Load(filepath);
    TEST_ASSERT(loadSuccess, "Scene loads from file");
    TEST_ASSERT(loadedScene->GetName() == "FileTestScene", "Loaded scene has correct name");

    auto loadedRoot = std::dynamic_pointer_cast<Node3D>(loadedScene->GetRoot());
    TEST_ASSERT(loadedRoot != nullptr, "Loaded root is Node3D");
    TEST_ASSERT(math::Equals(loadedRoot->GetPosition().x, 1.0f), "Position X preserved in file");
    TEST_ASSERT(math::Equals(loadedRoot->GetPosition().y, 2.0f), "Position Y preserved in file");
    TEST_ASSERT(math::Equals(loadedRoot->GetPosition().z, 3.0f), "Position Z preserved in file");

    // Clean up test file
    platform::FileSystem::DeleteFile(filepath);

    return true;
}

bool TestProjectManagement() {
    TEST_SECTION("Project Management Tests");

    // Create a project
    auto project = std::make_shared<Project>();
    auto& settings = project->GetSettings();
    settings.RegisterProperties();

    settings.projectName = "Test Project";
    settings.creatorName = "Test Creator";
    settings.version = "1.0.0";
    settings.mainScene = "res://scenes/main.scene";
    settings.windowWidth = 1920;
    settings.windowHeight = 1080;
    settings.clearColor = math::Color(0.2f, 0.3f, 0.4f, 1.0f);

    // Save project
    std::string projectPath = "test_project.lupine";
    bool saveSuccess = project->SaveAs(projectPath);
    TEST_ASSERT(saveSuccess, "Project saves to file");

    // Load project
    auto loadedProject = std::make_shared<Project>();
    bool loadSuccess = loadedProject->Load(projectPath);
    TEST_ASSERT(loadSuccess, "Project loads from file");

    auto& loadedSettings = loadedProject->GetSettings();
    TEST_ASSERT(loadedSettings.projectName == "Test Project", "Project name preserved");
    TEST_ASSERT(loadedSettings.creatorName == "Test Creator", "Creator name preserved");
    TEST_ASSERT(loadedSettings.windowWidth == 1920, "Window width preserved");
    TEST_ASSERT(loadedSettings.windowHeight == 1080, "Window height preserved");
    TEST_ASSERT(math::Equals(loadedSettings.clearColor.r, 0.2f), "Clear color R preserved");

    // Clean up test file
    platform::FileSystem::DeleteFile(projectPath);

    return true;
}

bool TestPrefabSystem() {
    TEST_SECTION("Prefab System Tests");

    // Create a node tree to use as a prefab
    auto prefabRoot = std::make_shared<Node2D>("PrefabRoot");
    prefabRoot->RegisterProperties();
    prefabRoot->SetPosition(math::Vec2(10.0f, 20.0f));
    prefabRoot->SetRotation(45.0f);
    prefabRoot->SetScale(math::Vec2(1.5f, 1.5f));
    prefabRoot->SetVisible(true);

    auto child1 = std::make_shared<Node2D>("PrefabChild1");
    child1->RegisterProperties();
    child1->SetPosition(math::Vec2(5.0f, 0.0f));
    child1->SetZIndex(10);

    auto child2 = std::make_shared<Node2D>("PrefabChild2");
    child2->RegisterProperties();
    child2->SetPosition(math::Vec2(-5.0f, 0.0f));
    child2->SetVisible(false);  // Test visibility preservation

    // Add components to the prefab
    auto component1 = std::make_shared<TestComponent>();
    component1->RegisterProperties();
    component1->value = 999;
    prefabRoot->AddComponent(component1);

    auto component2 = std::make_shared<TestComponent>();
    component2->RegisterProperties();
    component2->value = 777;
    child1->AddComponent(component2);

    prefabRoot->AddChild(child1);
    prefabRoot->AddChild(child2);

    // Create prefab from node tree
    auto prefab = std::make_shared<Prefab>("TestPrefab");
    prefab->RegisterProperties();
    prefab->CreateFromNode(prefabRoot);
    TEST_ASSERT(prefab->IsValid(), "Prefab created from node tree");

    // Save prefab to file
    std::string prefabPath = "test_prefab.prefab";
    bool saveSuccess = prefab->Save(prefabPath);
    TEST_ASSERT(saveSuccess, "Prefab saves to file");

    // Load prefab from file
    auto loadedPrefab = std::make_shared<Prefab>();
    bool loadSuccess = loadedPrefab->Load(prefabPath);
    TEST_ASSERT(loadSuccess, "Prefab loads from file");
    TEST_ASSERT(loadedPrefab->IsValid(), "Loaded prefab is valid");
    TEST_ASSERT(loadedPrefab->GetName() == "TestPrefab", "Prefab name preserved");

    // Instantiate the prefab
    auto instance1 = loadedPrefab->Instantiate();
    TEST_ASSERT(instance1 != nullptr, "Prefab instantiates successfully");
    TEST_ASSERT(instance1->GetName() == "PrefabRoot", "Instance has correct name");

    // Check that the instance is a proper copy with correct properties
    auto instance1_2D = std::dynamic_pointer_cast<Node2D>(instance1);
    TEST_ASSERT(instance1_2D != nullptr, "Instance is correct type (Node2D)");
    TEST_ASSERT(math::Equals(instance1_2D->GetPosition().x, 10.0f), "Instance position X preserved");
    TEST_ASSERT(math::Equals(instance1_2D->GetPosition().y, 20.0f), "Instance position Y preserved");
    TEST_ASSERT(math::Equals(instance1_2D->GetRotation(), 45.0f), "Instance rotation preserved");
    TEST_ASSERT(math::Equals(instance1_2D->GetScale().x, 1.5f), "Instance scale X preserved");
    TEST_ASSERT(instance1_2D->IsVisible(), "Instance visibility preserved");

    // Check hierarchy is preserved
    TEST_ASSERT(instance1->GetChildCount() == 2, "Instance has correct number of children");
    
    auto instanceChild1 = std::dynamic_pointer_cast<Node2D>(instance1->GetChild("PrefabChild1"));
    TEST_ASSERT(instanceChild1 != nullptr, "Instance child 1 exists and is correct type");
    TEST_ASSERT(math::Equals(instanceChild1->GetPosition().x, 5.0f), "Child 1 position preserved");
    TEST_ASSERT(instanceChild1->GetZIndex() == 10, "Child 1 z-index preserved");

    auto instanceChild2 = instance1->GetChild("PrefabChild2");
    TEST_ASSERT(instanceChild2 != nullptr, "Instance child 2 exists");
    TEST_ASSERT(!instanceChild2->IsVisible(), "Child 2 visibility (false) preserved");

    // Check components are preserved
    auto instanceComp1 = instance1->GetComponent<TestComponent>();
    TEST_ASSERT(instanceComp1 != nullptr, "Root component restored");
    TEST_ASSERT(instanceComp1->value == 999, "Root component value preserved");

    auto instanceComp2 = instanceChild1->GetComponent<TestComponent>();
    TEST_ASSERT(instanceComp2 != nullptr, "Child component restored");
    TEST_ASSERT(instanceComp2->value == 777, "Child component value preserved");

    // Check that UUIDs are different (each instance should have unique UUIDs)
    TEST_ASSERT(instance1->GetUUID() != prefabRoot->GetUUID(), "Instance has new UUID (different from original)");
    TEST_ASSERT(instanceChild1->GetUUID() != child1->GetUUID(), "Child instance has new UUID");

    // Test instantiating multiple times - each should be independent
    auto instance2 = loadedPrefab->Instantiate();
    TEST_ASSERT(instance2 != nullptr, "Second instantiation succeeds");
    TEST_ASSERT(instance1->GetUUID() != instance2->GetUUID(), "Multiple instances have unique UUIDs");

    // Test InstantiateAsChild
    auto parentNode = std::make_shared<Node>("Parent");
    auto instance3 = loadedPrefab->InstantiateAsChild(parentNode);
    TEST_ASSERT(instance3 != nullptr, "InstantiateAsChild succeeds");
    TEST_ASSERT(instance3->GetParent() == parentNode.get(), "Instance correctly parented");
    TEST_ASSERT(parentNode->GetChildCount() == 1, "Parent has instance as child");

    // Test clear functionality
    loadedPrefab->Clear();
    TEST_ASSERT(!loadedPrefab->IsValid(), "Prefab cleared successfully");
    auto nullInstance = loadedPrefab->Instantiate();
    TEST_ASSERT(nullInstance == nullptr, "Cleared prefab cannot instantiate");

    // Clean up test file
    platform::FileSystem::DeleteFile(prefabPath);

    return true;
}

bool TestComplexSceneGraph() {
    TEST_SECTION("Complex Scene Graph Tests");

    // Create a complex scene with mixed node types and components
    auto scene = std::make_shared<Scene>("ComplexScene");
    scene->RegisterProperties();

    auto root = std::make_shared<Node>("Root");
    root->RegisterProperties();

    auto node2D = std::make_shared<Node2D>("2DNode");
    node2D->RegisterProperties();
    node2D->SetPosition(math::Vec2(100.0f, 200.0f));
    node2D->SetRotation(90.0f);

    auto node3D = std::make_shared<Node3D>("3DNode");
    node3D->RegisterProperties();
    node3D->SetPosition(math::Vec3(1.0f, 2.0f, 3.0f));

    auto component1 = std::make_shared<TestComponent>();
    component1->RegisterProperties();
    component1->value = 100;
    node2D->AddComponent(component1);

    auto component2 = std::make_shared<TestComponent>();
    component2->RegisterProperties();
    component2->value = 200;
    node3D->AddComponent(component2);

    root->AddChild(node2D);
    root->AddChild(node3D);
    scene->SetRoot(root);

    // Save and load
    std::string filepath = "complex_scene.scene";
    bool saveSuccess = scene->Save(filepath);
    TEST_ASSERT(saveSuccess, "Complex scene saves");

    auto loadedScene = std::make_shared<Scene>();
    bool loadSuccess = loadedScene->Load(filepath);
    TEST_ASSERT(loadSuccess, "Complex scene loads");

    // Verify structure
    TEST_ASSERT(loadedScene->GetRoot()->GetChildCount() == 2, "Root has 2 children");

    auto loaded2D = std::dynamic_pointer_cast<Node2D>(loadedScene->FindNode("2DNode"));
    TEST_ASSERT(loaded2D != nullptr, "2D node found and correct type");
    TEST_ASSERT(math::Equals(loaded2D->GetPosition().x, 100.0f), "2D position preserved");

    auto loaded3D = std::dynamic_pointer_cast<Node3D>(loadedScene->FindNode("3DNode"));
    TEST_ASSERT(loaded3D != nullptr, "3D node found and correct type");
    TEST_ASSERT(math::Equals(loaded3D->GetPosition().z, 3.0f), "3D position preserved");

    auto loadedComp1 = loaded2D->GetComponent<TestComponent>();
    TEST_ASSERT(loadedComp1 != nullptr, "2D node component restored");
    TEST_ASSERT(loadedComp1->value == 100, "2D node component value preserved");

    auto loadedComp2 = loaded3D->GetComponent<TestComponent>();
    TEST_ASSERT(loadedComp2 != nullptr, "3D node component restored");
    TEST_ASSERT(loadedComp2->value == 200, "3D node component value preserved");

    // Clean up
    platform::FileSystem::DeleteFile(filepath);

    return true;
}

void RunEngineTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "   Lupine Engine System Tests" << std::endl;
    std::cout << "========================================\n" << std::endl;

    lupine_test::SetCurrentSuite("Engine System");

    // Initialize logger
    Logger::Init("lupine_engine_tests.log", true);
    LOG_INFO(LogCategory::Core, "=== Starting Engine System Tests ===");

    // Initialize engine
    InitializeEngine();

    int passedTests = 0;
    int totalTests = 0;

    auto runTest = [&](bool (*testFunc)(), const char* testName) {
        totalTests++;
        std::cout << "\n[TEST] " << testName << std::endl;
        if (testFunc()) {
            passedTests++;
            std::cout << "[SUCCESS] " << testName << " passed\n" << std::endl;
        } else {
            std::cout << "[FAILED] " << testName << " failed\n" << std::endl;
        }
    };

    runTest(TestSerialization, "Serialization System");
    runTest(TestNodeHierarchy, "Node Hierarchy");
    runTest(TestComponents, "Component System");
    runTest(TestComponentSerialization, "Component Serialization");
    runTest(TestSceneManagement, "Scene Management");
    runTest(TestSceneFiles, "Scene File I/O");
    runTest(TestProjectManagement, "Project Management");
    runTest(TestPrefabSystem, "Prefab System");
    runTest(TestComplexSceneGraph, "Complex Scene Graph");

    std::cout << "\n========================================" << std::endl;
    std::cout << "   Test Results: " << passedTests << "/" << totalTests << " passed" << std::endl;
    std::cout << "========================================\n" << std::endl;

    // Shutdown engine
    ShutdownEngine();

    Logger::Flush();
    Logger::Shutdown();
}


