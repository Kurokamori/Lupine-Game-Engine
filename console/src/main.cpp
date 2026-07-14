#include "lupine/logger/Logger.hpp"
#include "TestFramework.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <limits>

// Forward declarations of test functions
void RunLoggerTest();
void RunInteractiveMathTests();
void RunAllMathTests();
void RunPlatformTests();
void RunEngineTests();
void RunAllAssetTests();
bool RunSceneInstantiationTests();
void RunPhysics2DTests();
void RunPhysics3DTests();
void RunPropertySystemTests();
void RunScriptAnnotationParserTests();
void RunScriptingTests();
void RunSignalTests();
void RunLocalizationTests();
void RunNodeTests();
void RunEventBusTests();
void RunAnimationInterpTests();
void RunLocalizationManagerTests();
void RunSerializationTests();
void RunArchetypeTests();
void RunInterfaceTests();
void RunLocalizationProjectTests();
void RunTweenTests();
void RunSignalDispatcherTests();
void RunCurvePathTests();
void RunSceneTests();
void RunInputTests();
void RunRenderingTests();
void RunAudioTests();
void RunComponentSweepTests();
void RunSprite2DComponentTests();
void RunMesh3DComponentTests();
void RunLightComponentTests();
void RunUIBasicComponentTests();
void RunUIInputComponentTests();
void RunUIListComponentTests();
void RunUIContainerComponentTests();
void RunUI3DComponentTests();
void RunPhysics2DComponentTests();
void RunPhysics3DComponentTests();
void RunAnimationComponentTests();
void RunMiscComponentTests();
void RunSaveGameTests();
void RunNetworkTests();
void RunNavigation3DTests();
void RunCustomComponentChainTests();
void RunAllTests();
void WaitForInput();
void ClearScreen();
void ShowMenu();

int main() {
    bool running = true;

    ClearScreen();
    std::cout << "======================================" << std::endl;
    std::cout << "   Lupine Engine Console Test Suite  " << std::endl;
    std::cout << "======================================" << std::endl;
    std::cout << std::endl;

    while (running) {
        ShowMenu();

        std::string input;
        std::cout << "Enter your choice: ";
        std::getline(std::cin, input);
        std::cout << std::endl;

        if (input == "1") {
            RunLoggerTest();
            WaitForInput();
        }
        else if (input == "2") {
            RunInteractiveMathTests();
            // Math tests handle their own input waiting
        }
        else if (input == "3") {
            RunPlatformTests();
            WaitForInput();
        }
        else if (input == "4") {
            RunEngineTests();
            WaitForInput();
        }
        else if (input == "5") {
            RunAllAssetTests();
            WaitForInput();
        }
        else if (input == "6") {
            RunSceneInstantiationTests();
            WaitForInput();
        }
        else if (input == "7") {
            RunPhysics2DTests();
            WaitForInput();
        }
        else if (input == "8") {
            RunPhysics3DTests();
            WaitForInput();
        }
        else if (input == "9") {
            RunPropertySystemTests();
            WaitForInput();
        }
        else if (input == "10") {
            RunScriptingTests();
            WaitForInput();
        }
        else if (input == "11") {
            RunSignalTests();
            WaitForInput();
        }
        else if (input == "12") {
            RunLocalizationTests();
            WaitForInput();
        }
        else if (input == "13") {
            RunNodeTests();
            WaitForInput();
        }
        else if (input == "14") {
            RunEventBusTests();
            WaitForInput();
        }
        else if (input == "15") {
            RunAnimationInterpTests();
            WaitForInput();
        }
        else if (input == "16") {
            RunLocalizationManagerTests();
            WaitForInput();
        }
        else if (input == "17") {
            RunSerializationTests();
            WaitForInput();
        }
        else if (input == "18") {
            RunArchetypeTests();
            WaitForInput();
        }
        else if (input == "19") {
            RunLocalizationProjectTests();
            WaitForInput();
        }
        else if (input == "20") {
            RunTweenTests();
            WaitForInput();
        }
        else if (input == "21") {
            RunSignalDispatcherTests();
            WaitForInput();
        }
        else if (input == "22") {
            RunCurvePathTests();
            WaitForInput();
        }
        else if (input == "23") {
            RunSceneTests();
            WaitForInput();
        }
        else if (input == "24") {
            RunInputTests();
            WaitForInput();
        }
        else if (input == "25") {
            RunRenderingTests();
            WaitForInput();
        }
        else if (input == "26") {
            RunAudioTests();
            WaitForInput();
        }
        else if (input == "27") {
            RunComponentSweepTests();
            WaitForInput();
        }
        else if (input == "28") {
            RunSprite2DComponentTests();
            WaitForInput();
        }
        else if (input == "29") {
            RunMesh3DComponentTests();
            WaitForInput();
        }
        else if (input == "30") {
            RunLightComponentTests();
            WaitForInput();
        }
        else if (input == "31") {
            RunUIBasicComponentTests();
            WaitForInput();
        }
        else if (input == "32") {
            RunUIInputComponentTests();
            WaitForInput();
        }
        else if (input == "33") {
            RunUIListComponentTests();
            WaitForInput();
        }
        else if (input == "34") {
            RunUIContainerComponentTests();
            WaitForInput();
        }
        else if (input == "35") {
            RunUI3DComponentTests();
            WaitForInput();
        }
        else if (input == "36") {
            RunPhysics2DComponentTests();
            WaitForInput();
        }
        else if (input == "37") {
            RunPhysics3DComponentTests();
            WaitForInput();
        }
        else if (input == "38") {
            RunAnimationComponentTests();
            WaitForInput();
        }
        else if (input == "39") {
            RunMiscComponentTests();
            WaitForInput();
        }
        else if (input == "40") {
            RunSaveGameTests();
            WaitForInput();
        }
        else if (input == "41") {
            RunNetworkTests();
            WaitForInput();
        }
        else if (input == "42") {
            RunNavigation3DTests();
            WaitForInput();
        }
        else if (input == "43") {
            RunInterfaceTests();
            WaitForInput();
        }
        else if (input == "44") {
            RunCustomComponentChainTests();
            WaitForInput();
        }
        else if (input == "a" || input == "A" || input == "all") {
            RunAllTests();
            WaitForInput();
        }
        else if (input == "0" || input == "q" || input == "quit" || input == "exit") {
            running = false;
            std::cout << "Exiting Lupine Console. Goodbye!" << std::endl;
        }
        else {
            std::cout << "Invalid option. Please try again." << std::endl;
            WaitForInput();
        }

        if (running) {
            ClearScreen();
        }
    }

    return 0;
}

void ShowMenu() {
    std::cout << "What would you like to test?" << std::endl;
    std::cout << std::endl;
    std::cout << "  [1] Logger System" << std::endl;
    std::cout << "  [2] Math Library" << std::endl;
    std::cout << "  [3] Platform Module" << std::endl;
    std::cout << "  [4] Engine System (ECS, Serialization, Scenes)" << std::endl;
    std::cout << "  [5] Asset Loading (Images, Models, Fonts)" << std::endl;
    std::cout << "  [6] Scene Instantiation (Nested Scenes)" << std::endl;
    std::cout << "  [7] Physics2D System (Box2D Integration)" << std::endl;
    std::cout << "  [8] Physics3D System (Bullet3 Integration)" << std::endl;
    std::cout << "  [9] Component Property System" << std::endl;
    std::cout << " [10] Scripting System (Python & Lua)" << std::endl;
    std::cout << " [11] Signal / Event System" << std::endl;
    std::cout << " [12] Localization System" << std::endl;
    std::cout << " [13] Node System (Hierarchy, Groups, Transforms)" << std::endl;
    std::cout << " [14] Event Bus (Global Pub/Sub)" << std::endl;
    std::cout << " [15] Animation Interpolation (Easing, Lerp)" << std::endl;
    std::cout << " [16] Localization Manager (Format, Pseudo, Plurals)" << std::endl;
    std::cout << " [17] Serialization (UUID, Node Round-Trip)" << std::endl;
    std::cout << " [18] Archetype System (Temp Project, Inheritance)" << std::endl;
    std::cout << " [19] Localization Project (Translate, Plurals, Fallback)" << std::endl;
    std::cout << " [20] Tween Runtime (Interpolation, Signals, Looping)" << std::endl;
    std::cout << " [21] Signal Dispatcher (Deferred Calls, QueueFree)" << std::endl;
    std::cout << " [22] Curve / Path 2D + 3D (Sampling, Bezier, Path Queries)" << std::endl;
    std::cout << " [23] Scene (Structure, Groups, Save/Load)" << std::endl;
    std::cout << " [24] Input System (Actions, Axes, Devices, Mapping)" << std::endl;
    std::cout << " [25] Rendering (Camera Matrices, Materials)" << std::endl;
    std::cout << " [26] Audio System (Buses, Listener, Master, Serialization)" << std::endl;
    std::cout << " [27] Component Sweep (All Registered Types, Reflection)" << std::endl;
    std::cout << " [28] Components: 2D Sprites/Shapes/Lines/TileMap" << std::endl;
    std::cout << " [29] Components: 3D Meshes/Sprites/MultiMesh/Scatter" << std::endl;
    std::cout << " [30] Components: Lights & World Environment" << std::endl;
    std::cout << " [31] Components: UI Basic (Label/Button/Panel/Image/Progress)" << std::endl;
    std::cout << " [32] Components: UI Input (LineEdit/Slider/SpinBox/Check/Radio)" << std::endl;
    std::cout << " [33] Components: UI Lists (ItemList/Tree/Dropdown/Menu/Tabs)" << std::endl;
    std::cout << " [34] Components: UI Containers & UIControl Base" << std::endl;
    std::cout << " [35] Components: UI 3D (Label3D/Button3D/Panel3D/Progress3D)" << std::endl;
    std::cout << " [36] Components: Physics 2D Bodies & Controllers" << std::endl;
    std::cout << " [37] Components: Physics 3D Bodies & Controllers" << std::endl;
    std::cout << " [38] Components: Animation (Player/Tree/TweenSequence)" << std::endl;
    std::cout << " [39] Components: Audio/Timer/SubViewport/Shader Misc" << std::endl;
    std::cout << " [40] Save Game Toolkit (slots/schema/migration/scene-state)" << std::endl;
    std::cout << " [41] Networking (serializer/session/RPC/replication over loopback)" << std::endl;
    std::cout << " [42] Navigation 3D (Recast-style bake / NavMesh3D pathfinding)" << std::endl;
    std::cout << " [43] Interface Types (capability contracts / conformance queries)" << std::endl;
    std::cout << " [44] Custom Component Chains (multi-level inheritance / ancestry)" << std::endl;
    std::cout << "  [A] Run ALL Tests (aggregate report)" << std::endl;
    std::cout << "  [0] Exit" << std::endl;
    std::cout << std::endl;
}

void RunLoggerTest() {
    std::cout << "=== Lupine Engine Logger Test ===" << std::endl;
    std::cout << std::endl;

    // Initialize the logger
    lupine::Logger::Init("lupine.log", true);

    // Test all log levels with different categories
    LOG_CORE_INFO("=== Testing Core Category ===");
    LOG_CORE_TRACE("This is a TRACE message from Core");
    LOG_CORE_DEBUG("This is a DEBUG message from Core");
    LOG_CORE_INFO("This is an INFO message from Core");
    LOG_CORE_WARN("This is a WARN message from Core");
    LOG_CORE_ERROR("This is an ERROR message from Core");
    LOG_CORE_FATAL("This is a FATAL message from Core");

    LOG_CORE_INFO("=== Testing ECS Category ===");
    LOG_ECS_INFO("Entity {} created with {} components", 12345, 3);
    LOG_ECS_WARN("Component type '{}' already exists", "TransformComponent");
    LOG_ECS_ERROR("Failed to remove component from entity {}", 67890);

    LOG_CORE_INFO("=== Testing Render Category ===");
    LOG_RENDER_INFO("Initializing {} renderer", "OpenGL");
    LOG_RENDER_DEBUG("Setting viewport to {}x{}", 1920, 1080);
    LOG_RENDER_WARN("Texture '{}' not found, using fallback", "missing_texture.png");
    LOG_RENDER_ERROR("Shader compilation failed: {}", "syntax error on line 42");

    LOG_CORE_INFO("=== Testing Audio Category ===");
    LOG_AUDIO_INFO("Audio system initialized with {} channels", 16);
    LOG_AUDIO_DEBUG("Loading audio file: {}", "music/theme.ogg");
    LOG_AUDIO_ERROR("Failed to load audio file: {}", "sound/missing.wav");

    LOG_CORE_INFO("=== Testing Physics Category ===");
    LOG_PHYSICS_INFO("Physics engine initialized at {} Hz", 60);
    LOG_PHYSICS_DEBUG("Rigid body {} sleeping", 42);
    LOG_PHYSICS_WARN("Collision detected between entities {} and {}", 100, 200);

    LOG_CORE_INFO("=== Testing Asset Loading Category ===");
    LOG_ASSET_INFO("Loading asset: {}", "models/character.obj");
    LOG_ASSET_DEBUG("Asset cache size: {} MB", 128);
    LOG_ASSET_ERROR("Failed to load asset: {} - {}", "textures/wall.jpg", "File not found");

    LOG_CORE_INFO("=== Testing Scripting Category ===");
    LOG_SCRIPT_INFO("Lua VM initialized");
    LOG_SCRIPT_DEBUG("Executing script: {}", "init.lua");
    LOG_SCRIPT_ERROR("Script error: {} at line {}", "undefined variable", 15);

    LOG_CORE_INFO("=== Testing Network Category ===");
    LOG_NETWORK_INFO("Connecting to server at {}:{}", "127.0.0.1", 7777);
    LOG_NETWORK_WARN("Connection timeout after {} seconds", 30);
    LOG_NETWORK_ERROR("Failed to send packet: {}", "Connection lost");

    LOG_CORE_INFO("=== Testing Input Category ===");
    LOG_INPUT_DEBUG("Key pressed: {}", "Space");
    LOG_INPUT_INFO("Mouse position: ({}, {})", 640, 480);
    LOG_INPUT_WARN("Input device '{}' disconnected", "Gamepad 1");

    LOG_CORE_INFO("=== Testing UI Category ===");
    LOG_UI_INFO("UI system initialized");
    LOG_UI_DEBUG("Button '{}' clicked", "StartGame");
    LOG_UI_ERROR("Failed to load font: {}", "fonts/arial.ttf");

    // Test thread safety
    LOG_CORE_INFO("=== Testing Thread Safety ===");
    auto thread1 = std::thread([]() {
        for (int i = 0; i < 5; i++) {
            LOG_CORE_DEBUG("Thread 1 message {}", i);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    auto thread2 = std::thread([]() {
        for (int i = 0; i < 5; i++) {
            LOG_RENDER_DEBUG("Thread 2 message {}", i);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    thread1.join();
    thread2.join();

    LOG_CORE_INFO("=== Logger Test Complete ===");
    LOG_CORE_INFO("Check lupine.log for file output");

    // Flush and shutdown
    lupine::Logger::Flush();
    lupine::Logger::Shutdown();

    std::cout << std::endl;
    std::cout << "Test completed successfully!" << std::endl;
    std::cout << "Check lupine.log for the file output." << std::endl;
    std::cout << std::endl;
}

void RunAllTests() {
    std::cout << "\n##########################################" << std::endl;
    std::cout << "   RUNNING ALL LUPINE ENGINE TESTS" << std::endl;
    std::cout << "##########################################" << std::endl;

    lupine_test::ResetResults();

    RunAllMathTests();
    RunEngineTests();
    RunAllAssetTests();
    RunSceneInstantiationTests();
    RunPhysics2DTests();
    RunPhysics3DTests();
    RunPropertySystemTests();
    RunScriptAnnotationParserTests();
    RunSignalTests();
    RunLocalizationTests();
    RunNodeTests();
    RunEventBusTests();
    RunAnimationInterpTests();
    RunLocalizationManagerTests();
    RunSerializationTests();
    RunArchetypeTests();
    RunInterfaceTests();
    RunLocalizationProjectTests();
    RunTweenTests();
    RunSignalDispatcherTests();
    RunCurvePathTests();
    RunSceneTests();
    RunInputTests();
    RunRenderingTests();
    RunAudioTests();
    RunComponentSweepTests();
    RunSprite2DComponentTests();
    RunMesh3DComponentTests();
    RunLightComponentTests();
    RunUIBasicComponentTests();
    RunUIInputComponentTests();
    RunUIListComponentTests();
    RunUIContainerComponentTests();
    RunUI3DComponentTests();
    RunPhysics2DComponentTests();
    RunPhysics3DComponentTests();
    RunAnimationComponentTests();
    RunMiscComponentTests();
    RunSaveGameTests();
    RunNetworkTests();
    RunNavigation3DTests();
    RunCustomComponentChainTests();

    std::cout << "\n##########################################" << std::endl;
    std::cout << "   ALL TESTS COMPLETE" << std::endl;
    std::cout << "##########################################" << std::endl;

    lupine_test::PrintAggregateReport();
}

void WaitForInput() {
    std::cout << "Press Enter to continue...";
    std::string dummy;
    std::getline(std::cin, dummy);
}

void ClearScreen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}
