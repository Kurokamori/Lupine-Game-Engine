#include "lupine/engine/Engine.hpp"
#include "lupine/platform/Platform.hpp"
#include "TestFramework.hpp"
#include <iostream>
#include <cassert>

using namespace lupine;
using namespace lupine::engine;
using namespace lupine::core;

// Forward declarations
bool TestSceneInstanceBasics();
bool TestSceneInstanceSerialization();
bool TestSceneInstanceNesting();
bool TestSceneInstanceReloading();
bool TestSceneInstanceHierarchy();

/**
 * Run all scene instantiation tests
 */
bool RunSceneInstantiationTests() {
    std::cout << "\n==================================================" << std::endl;
    std::cout << "           SCENE INSTANTIATION TESTS              " << std::endl;
    std::cout << "==================================================" << std::endl;

    lupine_test::SetCurrentSuite("Scene Instantiation");

    // Initialize logger
    Logger::Init("lupine_scene_instantiation_tests.log", true);
    LOG_INFO(LogCategory::Core, "=== Starting Scene Instantiation Tests ===");

    // Initialize engine (this registers all built-in types)
    InitializeEngine();

    bool allTestsPassed = true;

    allTestsPassed &= TestSceneInstanceBasics();
    allTestsPassed &= TestSceneInstanceSerialization();
    allTestsPassed &= TestSceneInstanceNesting();
    allTestsPassed &= TestSceneInstanceReloading();
    allTestsPassed &= TestSceneInstanceHierarchy();

    std::cout << "\n==================================================" << std::endl;
    if (allTestsPassed) {
        std::cout << "   ALL SCENE INSTANTIATION TESTS PASSED! ✓" << std::endl;
    } else {
        std::cout << "   SOME SCENE INSTANTIATION TESTS FAILED! ✗" << std::endl;
    }
    std::cout << "==================================================" << std::endl;

    // Shutdown engine
    ShutdownEngine();

    Logger::Flush();
    Logger::Shutdown();

    return allTestsPassed;
}

/**
 * Test basic scene instance creation and properties
 */
bool TestSceneInstanceBasics() {
    TEST_SECTION("Scene Instance Basics");

    // Create a scene instance
    auto sceneInstance = std::make_shared<SceneInstance>("TestSceneInstance");
    TEST_ASSERT(sceneInstance != nullptr, "SceneInstance created");
    TEST_ASSERT(sceneInstance->GetName() == "TestSceneInstance", "SceneInstance has correct name");
    TEST_ASSERT(sceneInstance->GetTypeName() == "SceneInstance", "SceneInstance has correct type");
    TEST_ASSERT(!sceneInstance->HasValidReference(), "New SceneInstance has no valid reference");
    TEST_ASSERT(sceneInstance->GetSceneReference().empty(), "Scene reference is empty");
    TEST_ASSERT(sceneInstance->GetInstancedRoot() == nullptr, "No instanced root yet");

    std::cout << "[PASS] Scene Instance Basics" << std::endl;
    return true;
}

/**
 * Test scene instance serialization and deserialization
 */
bool TestSceneInstanceSerialization() {
    TEST_SECTION("Scene Instance Serialization");

    // Create a test scene to reference
    auto testScene = std::make_shared<Scene>("ReferencedScene");
    auto rootNode = std::make_shared<Node2D>("RootNode");
    auto child1 = std::make_shared<Node2D>("Child1");
    auto child2 = std::make_shared<Node2D>("Child2");
    
    rootNode->AddChild(child1);
    rootNode->AddChild(child2);
    testScene->SetRoot(rootNode);

    // Save the test scene
    std::string testScenePath = "test_referenced_scene.scene";
    bool saved = testScene->Save(testScenePath);
    TEST_ASSERT(saved, "Test scene saved successfully");

    // Create a scene with a scene instance
    auto mainScene = std::make_shared<Scene>("MainScene");
    auto mainRoot = std::make_shared<Node>("MainRoot");
    auto sceneInstance = std::make_shared<SceneInstance>("InstancedScene");
    
    // Set the scene reference
    bool referenced = sceneInstance->SetSceneReference(testScenePath);
    TEST_ASSERT(referenced, "Scene reference set successfully");
    TEST_ASSERT(sceneInstance->HasValidReference(), "Scene instance has valid reference");
    TEST_ASSERT(sceneInstance->GetInstancedRoot() != nullptr, "Instanced root exists");
    
    mainRoot->AddChild(sceneInstance);
    mainScene->SetRoot(mainRoot);

    // Serialize the main scene
    nlohmann::json sceneJson = mainScene->Serialize();
    TEST_ASSERT(sceneJson.contains("root"), "Serialized scene has root");

    // Check that scene instance serialized its reference
    std::string jsonStr = sceneJson.dump(2);
    TEST_ASSERT(jsonStr.find("scene_reference") != std::string::npos, 
                "Serialized scene contains scene_reference");
    TEST_ASSERT(jsonStr.find(testScenePath) != std::string::npos, 
                "Serialized scene contains referenced scene path");

    // Save and reload the main scene
    std::string mainScenePath = "test_main_scene_with_instance.scene";
    saved = mainScene->Save(mainScenePath);
    TEST_ASSERT(saved, "Main scene with instance saved successfully");

    // Load the scene back
    auto loadedScene = std::make_shared<Scene>();
    bool loaded = loadedScene->Load(mainScenePath);
    TEST_ASSERT(loaded, "Main scene loaded successfully");

    // Verify the scene instance was deserialized correctly
    auto loadedRoot = loadedScene->GetRoot();
    TEST_ASSERT(loadedRoot != nullptr, "Loaded scene has root");
    TEST_ASSERT(loadedRoot->GetChildCount() > 0, "Loaded root has children");

    auto loadedInstance = std::dynamic_pointer_cast<SceneInstance>(loadedRoot->GetChild(0));
    TEST_ASSERT(loadedInstance != nullptr, "First child is a SceneInstance");
    TEST_ASSERT(loadedInstance->GetSceneReference() == testScenePath, 
                "Scene reference preserved after serialization");
    TEST_ASSERT(loadedInstance->HasValidReference(), "Loaded instance has valid reference");
    TEST_ASSERT(loadedInstance->GetInstancedRoot() != nullptr, 
                "Loaded instance has instanced root");

    // Cleanup
    platform::FileSystem::DeleteFile(testScenePath);
    platform::FileSystem::DeleteFile(mainScenePath);

    std::cout << "[PASS] Scene Instance Serialization" << std::endl;
    return true;
}

/**
 * Test nested scene instances (scene within scene within scene)
 */
bool TestSceneInstanceNesting() {
    TEST_SECTION("Scene Instance Nesting");

    // Create a base scene (level 1)
    auto baseScene = std::make_shared<Scene>("BaseScene");
    auto baseRoot = std::make_shared<Node2D>("BaseRoot");
    auto baseChild = std::make_shared<Node2D>("BaseChild");
    baseRoot->AddChild(baseChild);
    baseScene->SetRoot(baseRoot);
    
    std::string baseScenePath = "test_base_scene.scene";
    bool saved = baseScene->Save(baseScenePath);
    TEST_ASSERT(saved, "Base scene saved");

    // Create a middle scene that instances the base scene (level 2)
    auto middleScene = std::make_shared<Scene>("MiddleScene");
    auto middleRoot = std::make_shared<Node>("MiddleRoot");
    auto middleInstance = std::make_shared<SceneInstance>("MiddleInstance");
    
    bool referenced = middleInstance->SetSceneReference(baseScenePath);
    TEST_ASSERT(referenced, "Middle scene references base scene");
    
    middleRoot->AddChild(middleInstance);
    
    // Add additional nodes to middle scene
    auto middleChild = std::make_shared<Node2D>("MiddleChild");
    middleRoot->AddChild(middleChild);
    
    middleScene->SetRoot(middleRoot);
    
    std::string middleScenePath = "test_middle_scene.scene";
    saved = middleScene->Save(middleScenePath);
    TEST_ASSERT(saved, "Middle scene saved");

    // Create a top scene that instances the middle scene (level 3)
    auto topScene = std::make_shared<Scene>("TopScene");
    auto topRoot = std::make_shared<Node>("TopRoot");
    auto topInstance = std::make_shared<SceneInstance>("TopInstance");
    
    referenced = topInstance->SetSceneReference(middleScenePath);
    TEST_ASSERT(referenced, "Top scene references middle scene");
    TEST_ASSERT(topInstance->GetChildCount() > 0, "Top instance has children");
    
    topRoot->AddChild(topInstance);
    topScene->SetRoot(topRoot);

    // Verify the nested hierarchy
    TEST_ASSERT(topInstance->GetInstancedRoot() != nullptr, 
                "Top instance has instanced root");
    TEST_ASSERT(topInstance->GetInstancedRoot()->GetName() == "MiddleRoot", 
                "Top instance root is MiddleRoot");

    // Find the nested scene instance
    auto middleRootFromTop = topInstance->GetInstancedRoot();
    TEST_ASSERT(middleRootFromTop->GetChildCount() >= 2, 
                "Middle root has at least 2 children (instance + regular node)");

    // Find the nested SceneInstance
    std::shared_ptr<SceneInstance> nestedInstance = nullptr;
    for (const auto& child : middleRootFromTop->GetChildren()) {
        auto si = std::dynamic_pointer_cast<SceneInstance>(child);
        if (si) {
            nestedInstance = si;
            break;
        }
    }
    
    TEST_ASSERT(nestedInstance != nullptr, "Found nested scene instance in hierarchy");
    TEST_ASSERT(nestedInstance->GetSceneReference() == baseScenePath, 
                "Nested instance has correct reference");

    // Save and reload to test nested serialization
    std::string topScenePath = "test_top_scene.scene";
    saved = topScene->Save(topScenePath);
    TEST_ASSERT(saved, "Top scene with nested instances saved");

    auto loadedTopScene = std::make_shared<Scene>();
    bool loaded = loadedTopScene->Load(topScenePath);
    TEST_ASSERT(loaded, "Top scene with nested instances loaded");

    // Verify nested structure is intact
    auto loadedTopRoot = loadedTopScene->GetRoot();
    TEST_ASSERT(loadedTopRoot != nullptr, "Loaded top scene has root");
    
    auto loadedTopInstance = std::dynamic_pointer_cast<SceneInstance>(
        loadedTopRoot->GetChild(0));
    TEST_ASSERT(loadedTopInstance != nullptr, "Loaded top instance exists");
    TEST_ASSERT(loadedTopInstance->HasValidReference(), 
                "Loaded top instance has valid reference");

    // Cleanup
    platform::FileSystem::DeleteFile(baseScenePath);
    platform::FileSystem::DeleteFile(middleScenePath);
    platform::FileSystem::DeleteFile(topScenePath);

    std::cout << "[PASS] Scene Instance Nesting" << std::endl;
    return true;
}

/**
 * Test scene instance reloading
 */
bool TestSceneInstanceReloading() {
    TEST_SECTION("Scene Instance Reloading");

    // Create initial scene
    auto originalScene = std::make_shared<Scene>("OriginalScene");
    auto root = std::make_shared<Node2D>("Root");
    auto child1 = std::make_shared<Node2D>("Child1");
    root->AddChild(child1);
    originalScene->SetRoot(root);
    
    std::string scenePath = "test_reload_scene.scene";
    bool saved = originalScene->Save(scenePath);
    TEST_ASSERT(saved, "Original scene saved");

    // Create scene instance
    auto sceneInstance = std::make_shared<SceneInstance>("ReloadTestInstance");
    bool referenced = sceneInstance->SetSceneReference(scenePath);
    TEST_ASSERT(referenced, "Scene referenced");
    TEST_ASSERT(sceneInstance->GetChildCount() == 1, "Instance has 1 child (root)");

    // Get initial child count
    auto instancedRoot = sceneInstance->GetInstancedRoot();
    TEST_ASSERT(instancedRoot != nullptr, "Instanced root exists");
    size_t initialChildCount = instancedRoot->GetChildCount();
    TEST_ASSERT(initialChildCount == 1, "Initial instanced root has 1 child");

    // Modify the original scene (add another child)
    auto child2 = std::make_shared<Node2D>("Child2");
    root->AddChild(child2);
    saved = originalScene->Save(scenePath);
    TEST_ASSERT(saved, "Modified scene saved");

    // Reload the scene instance
    bool reloaded = sceneInstance->ReloadScene();
    TEST_ASSERT(reloaded, "Scene instance reloaded");

    // Verify the instance was updated
    instancedRoot = sceneInstance->GetInstancedRoot();
    TEST_ASSERT(instancedRoot != nullptr, "Instanced root exists after reload");
    size_t newChildCount = instancedRoot->GetChildCount();
    TEST_ASSERT(newChildCount == 2, "Reloaded instanced root has 2 children");

    // Cleanup
    platform::FileSystem::DeleteFile(scenePath);

    std::cout << "[PASS] Scene Instance Reloading" << std::endl;
    return true;
}

/**
 * Test scene instance hierarchy and node finding
 */
bool TestSceneInstanceHierarchy() {
    TEST_SECTION("Scene Instance Hierarchy");

    // Create a referenced scene with deep hierarchy
    auto refScene = std::make_shared<Scene>("ReferenceScene");
    auto root = std::make_shared<Node2D>("Root");
    auto level1 = std::make_shared<Node2D>("Level1");
    auto level2 = std::make_shared<Node2D>("Level2");
    auto level3 = std::make_shared<Node2D>("Level3");
    
    level2->AddChild(level3);
    level1->AddChild(level2);
    root->AddChild(level1);
    refScene->SetRoot(root);
    
    std::string refScenePath = "test_hierarchy_scene.scene";
    bool saved = refScene->Save(refScenePath);
    TEST_ASSERT(saved, "Reference scene with hierarchy saved");

    // Create main scene with scene instance
    auto mainScene = std::make_shared<Scene>("MainScene");
    auto mainRoot = std::make_shared<Node>("MainRoot");
    auto sceneInstance = std::make_shared<SceneInstance>("HierarchyInstance");
    
    bool referenced = sceneInstance->SetSceneReference(refScenePath);
    TEST_ASSERT(referenced, "Scene referenced");
    
    mainRoot->AddChild(sceneInstance);
    
    // Add sibling to scene instance
    auto sibling = std::make_shared<Node2D>("Sibling");
    mainRoot->AddChild(sibling);
    
    mainScene->SetRoot(mainRoot);

    // Verify hierarchy structure
    TEST_ASSERT(mainRoot->GetChildCount() == 2, 
                "Main root has 2 children (instance + sibling)");
    TEST_ASSERT(sceneInstance->GetChildCount() == 1, 
                "Scene instance has 1 child (instanced root)");

    auto instancedRoot = sceneInstance->GetInstancedRoot();
    TEST_ASSERT(instancedRoot != nullptr, "Instanced root exists");
    TEST_ASSERT(instancedRoot->GetName() == "Root", "Instanced root name preserved");

    // Verify we can traverse the instanced hierarchy
    TEST_ASSERT(instancedRoot->GetChildCount() == 1, 
                "Instanced root has 1 child");
    auto instancedLevel1 = instancedRoot->GetChild(0);
    TEST_ASSERT(instancedLevel1 != nullptr && instancedLevel1->GetName() == "Level1", 
                "Level1 node preserved");

    auto instancedLevel2 = instancedLevel1->GetChild(0);
    TEST_ASSERT(instancedLevel2 != nullptr && instancedLevel2->GetName() == "Level2", 
                "Level2 node preserved");

    auto instancedLevel3 = instancedLevel2->GetChild(0);
    TEST_ASSERT(instancedLevel3 != nullptr && instancedLevel3->GetName() == "Level3", 
                "Level3 node preserved");

    // Test node finding through instance
    auto foundNode = mainScene->FindNode("/MainRoot/HierarchyInstance");
    TEST_ASSERT(foundNode != nullptr, "Found scene instance by path");
    TEST_ASSERT(foundNode->GetName() == "HierarchyInstance", 
                "Found correct scene instance");

    // Test UUID uniqueness (instanced nodes should have different UUIDs)
    auto originalLevel3UUID = level3->GetUUID();
    auto instancedLevel3UUID = instancedLevel3->GetUUID();
    TEST_ASSERT(originalLevel3UUID != instancedLevel3UUID, 
                "Instanced nodes have unique UUIDs");

    // Cleanup
    platform::FileSystem::DeleteFile(refScenePath);

    std::cout << "[PASS] Scene Instance Hierarchy" << std::endl;
    return true;
}
