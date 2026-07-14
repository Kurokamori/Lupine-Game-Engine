#include "lupine/engine/Engine.hpp"
#include "lupine/logger/Logger.hpp"
#include <iostream>
#include <filesystem>

using namespace lupine;
using namespace lupine::engine;
using namespace lupine::core;

void RunScriptingTests() {
    std::cout << "=== Lupine Engine Scripting System Test ===" << std::endl;
    std::cout << std::endl;

    InitializeEngine();

    std::cout << "\n[Test 1] Python Script Component" << std::endl;
    std::cout << "===================================" << std::endl;

    {
        auto node = std::make_shared<Node>("TestNode");

        auto pythonScript = std::make_shared<PythonScriptComponent>("TestPythonScript");

        std::string pythonPath = "test_python_script.py";
        if (!std::filesystem::exists(pythonPath)) {
            std::cout << "ERROR: Python test script not found: " << pythonPath << std::endl;
        } else {
            pythonScript->SetScriptPath(pythonPath);
            node->AddComponent(pythonScript);

            std::cout << "Loading Python script..." << std::endl;
            if (pythonScript->LoadScript()) {
                std::cout << "[OK] Python script loaded successfully!" << std::endl;

                std::cout << "\nExport Properties:" << std::endl;
                const auto& exportProps = pythonScript->GetExportProperties();
                for (const auto& prop : exportProps) {
                    std::cout << "  - " << prop.name << " (";
                    switch (prop.descriptor.type) {
                        case core::PropertyValueType::Int:
                            std::cout << "int): " << pythonScript->GetPropertyValue<int>(prop.name);
                            break;
                        case core::PropertyValueType::Float:
                            std::cout << "float): " << pythonScript->GetPropertyValue<float>(prop.name);
                            break;
                        case core::PropertyValueType::String:
                            std::cout << "string): " << pythonScript->GetPropertyValue<std::string>(prop.name);
                            break;
                        case core::PropertyValueType::Bool:
                            std::cout << "bool): " << (pythonScript->GetPropertyValue<bool>(prop.name) ? "true" : "false");
                            break;
                        default:
                            std::cout << "unknown)";
                    }
                    std::cout << std::endl;
                }

                std::cout << "\nTesting lifecycle hooks:" << std::endl;
                std::cout << "  Calling OnAwake()..." << std::endl;
                pythonScript->OnAwake();

                std::cout << "  Calling OnReady()..." << std::endl;
                pythonScript->OnReady();

                std::cout << "  Calling OnProcess() 5 times..." << std::endl;
                for (int i = 0; i < 5; i++) {
                    pythonScript->OnProcess(0.016f);
                }

                std::cout << "  Calling OnDestroy()..." << std::endl;
                pythonScript->OnDestroy();

                std::cout << "\nTesting property modification:" << std::endl;
                pythonScript->SetPropertyValue("health", 50);
                pythonScript->SetPropertyValue("player_name", std::string("Hero"));
                std::cout << "  Set health=50, player_name=Hero" << std::endl;
            } else {
                std::cout << "ERROR: Failed to load Python script!" << std::endl;
            }
        }
    }

    std::cout << "\n[Test 2] Lua Script Component" << std::endl;
    std::cout << "===================================" << std::endl;

    {
        auto node = std::make_shared<Node>("TestNode");

        auto luaScript = std::make_shared<LuaScriptComponent>("TestLuaScript");

        std::string luaPath = "test_lua_script.lua";
        if (!std::filesystem::exists(luaPath)) {
            std::cout << "ERROR: Lua test script not found: " << luaPath << std::endl;
        } else {
            luaScript->SetScriptPath(luaPath);
            node->AddComponent(luaScript);

            std::cout << "Loading Lua script..." << std::endl;
            if (luaScript->LoadScript()) {
                std::cout << "[OK] Lua script loaded successfully!" << std::endl;

                std::cout << "\nExport Properties:" << std::endl;
                const auto& exportProps = luaScript->GetExportProperties();
                for (const auto& prop : exportProps) {
                    std::cout << "  - " << prop.name << " (";
                    switch (prop.descriptor.type) {
                        case core::PropertyValueType::Int:
                            std::cout << "int): " << luaScript->GetPropertyValue<int>(prop.name);
                            break;
                        case core::PropertyValueType::Float:
                            std::cout << "float): " << luaScript->GetPropertyValue<float>(prop.name);
                            break;
                        case core::PropertyValueType::String:
                            std::cout << "string): " << luaScript->GetPropertyValue<std::string>(prop.name);
                            break;
                        case core::PropertyValueType::Bool:
                            std::cout << "bool): " << (luaScript->GetPropertyValue<bool>(prop.name) ? "true" : "false");
                            break;
                        default:
                            std::cout << "unknown)";
                    }
                    std::cout << std::endl;
                }

                std::cout << "\nTesting lifecycle hooks:" << std::endl;
                std::cout << "  Calling OnAwake()..." << std::endl;
                luaScript->OnAwake();

                std::cout << "  Calling OnReady()..." << std::endl;
                luaScript->OnReady();

                std::cout << "  Calling OnProcess() 5 times..." << std::endl;
                for (int i = 0; i < 5; i++) {
                    luaScript->OnProcess(0.016f);
                }

                std::cout << "  Calling OnDestroy()..." << std::endl;
                luaScript->OnDestroy();

                std::cout << "\nTesting property modification:" << std::endl;
                luaScript->SetPropertyValue("health", 75);
                luaScript->SetPropertyValue("player_name", std::string("Warrior"));
                std::cout << "  Set health=75, player_name=Warrior" << std::endl;
            } else {
                std::cout << "ERROR: Failed to load Lua script!" << std::endl;
            }
        }
    }

    std::cout << "\n[Test 3] Script Serialization" << std::endl;
    std::cout << "===================================" << std::endl;

    {
        auto pythonScript = std::make_shared<PythonScriptComponent>("SerializationTest");
        pythonScript->SetScriptPath("test_python_script.py");

        if (std::filesystem::exists("test_python_script.py") && pythonScript->LoadScript()) {
            pythonScript->SetPropertyValue("health", 42);
            pythonScript->SetPropertyValue("speed", 7.5f);
            pythonScript->SetPropertyValue("player_name", std::string("Serialized Hero"));

            std::cout << "Serializing script component..." << std::endl;
            auto json = pythonScript->Serialize();

            std::cout << "Deserializing into a new component..." << std::endl;
            auto newPythonScript = std::make_shared<PythonScriptComponent>("Deserialized");
            newPythonScript->Deserialize(json);
            std::cout << "[OK] Serialization round-trip complete" << std::endl;
        } else {
            std::cout << "SKIP: Python test script not available for serialization test" << std::endl;
        }
    }

    std::cout << "\n[Test 4] Multiple Script Instances" << std::endl;
    std::cout << "===================================" << std::endl;

    {
        for (int i = 1; i <= 3; i++) {
            auto script = std::make_shared<PythonScriptComponent>("Instance_" + std::to_string(i));
            if (std::filesystem::exists("test_python_script.py")) {
                script->SetScriptPath("test_python_script.py");
                if (script->LoadScript()) {
                    script->SetPropertyValue("health", 100 * i);
                    script->SetPropertyValue("player_name", std::string("Player") + std::to_string(i));

                    script->OnAwake();
                    std::cout << "  Instance " << i << " loaded (health=" << (100 * i) << ")" << std::endl;
                }
            }
        }
    }

    std::cout << "\n[Test 5] Lua Scripting API" << std::endl;
    std::cout << "===================================" << std::endl;

    {
        auto scene = std::make_shared<Scene>("LuaTestScene");
        auto node = std::make_shared<Node>("LuaTestNode");
        scene->AddNode(node);

        auto luaScript = std::make_shared<LuaScriptComponent>("LuaAPITest");
        std::string luaPath = "test_lua_api.lua";

        if (!std::filesystem::exists(luaPath)) {
            std::cout << "SKIP: Lua API test script not found: " << luaPath << std::endl;
        } else {
            luaScript->SetScriptPath(luaPath);
            node->AddComponent(luaScript);
            if (luaScript->LoadScript()) {
                std::cout << "[OK] Lua API script loaded" << std::endl;

                luaScript->OnAwake();
                luaScript->OnReady();

                std::cout << "  Running 5 frames (input/process/physics/render)..." << std::endl;
                for (int i = 0; i < 5; i++) {
                    luaScript->OnInput(0.016f);
                    luaScript->OnProcess(0.016f);
                    luaScript->OnPhysicsProcess(0.016f);
                    luaScript->OnRender();
                }

                luaScript->OnDestroy();
                std::cout << "[OK] Lua API lifecycle complete" << std::endl;
            } else {
                std::cout << "ERROR: Failed to load Lua API script!" << std::endl;
            }
        }
    }

#ifdef LUPINE_HAS_MRUBY
    std::cout << "\n[Test 6] MRuby Scripting API" << std::endl;
    std::cout << "===================================" << std::endl;

    {
        auto scene = std::make_shared<Scene>("MRubyTestScene");
        auto node = std::make_shared<Node>("MRubyTestNode");
        scene->AddNode(node);

        auto mrubyScript = std::make_shared<MRubyScriptComponent>("MRubyAPITest");
        std::string mrubyPath = "test_mruby_api.rb";

        if (!std::filesystem::exists(mrubyPath)) {
            std::cout << "SKIP: MRuby API test script not found: " << mrubyPath << std::endl;
        } else {
            mrubyScript->SetScriptPath(mrubyPath);
            node->AddComponent(mrubyScript);
            if (mrubyScript->LoadScript()) {
                std::cout << "[OK] MRuby API script loaded" << std::endl;

                mrubyScript->OnAwake();
                mrubyScript->OnReady();

                std::cout << "  Running 5 frames (input/process/physics/render)..." << std::endl;
                for (int i = 0; i < 5; i++) {
                    mrubyScript->OnInput(0.016f);
                    mrubyScript->OnProcess(0.016f);
                    mrubyScript->OnPhysicsProcess(0.016f);
                    mrubyScript->OnRender();
                }

                mrubyScript->OnDestroy();
                std::cout << "[OK] MRuby API lifecycle complete" << std::endl;
            } else {
                std::cout << "ERROR: Failed to load MRuby API script!" << std::endl;
            }
        }
    }
#else
    std::cout << "\n[Test 6] MRuby Scripting API - SKIPPED (MRuby support not compiled in)" << std::endl;
#endif

#ifdef LUPINE_HAS_MRUBY
    std::cout << "\n[Test 7] Lua vs MRuby Comparison" << std::endl;
    std::cout << "===================================" << std::endl;

    {
        auto scene = std::make_shared<Scene>("ComparisonTestScene");

        auto luaNode = std::make_shared<Node>("LuaNode");
        auto luaScript = std::make_shared<LuaScriptComponent>("LuaComparison");
        luaScript->SetScriptPath("test_lua_api.lua");
        luaNode->AddComponent(luaScript);
        scene->AddNode(luaNode);

        auto mrubyNode = std::make_shared<Node>("MRubyNode");
        auto mrubyScript = std::make_shared<MRubyScriptComponent>("MRubyComparison");
        mrubyScript->SetScriptPath("test_mruby_api.rb");
        mrubyNode->AddComponent(mrubyScript);
        scene->AddNode(mrubyNode);
        bool luaLoaded = luaScript->LoadScript();
        bool mrubyLoaded = mrubyScript->LoadScript();

        if (luaLoaded && mrubyLoaded) {
            std::cout << "[OK] Both Lua and MRuby scripts loaded" << std::endl;

            luaScript->OnAwake();
            mrubyScript->OnAwake();

            luaScript->OnReady();
            mrubyScript->OnReady();

            std::cout << "  Running 3 process frames on both..." << std::endl;
            for (int i = 0; i < 3; i++) {
                luaScript->OnProcess(0.016f);
                mrubyScript->OnProcess(0.016f);
            }

            luaScript->OnDestroy();
            mrubyScript->OnDestroy();
            std::cout << "[OK] Comparison lifecycle complete" << std::endl;
        } else {
            if (!luaLoaded)
                std::cout << "SKIP: Lua comparison script (test_lua_api.lua) not available" << std::endl;
            if (!mrubyLoaded)
                std::cout << "SKIP: MRuby comparison script (test_mruby_api.rb) not available" << std::endl;
        }
    }
#endif

    ShutdownEngine();

    std::cout << "\n=== Scripting System Test Complete ===" << std::endl;
}
