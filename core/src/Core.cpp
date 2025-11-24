#include "lupine/core/Core.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/CameraNodes.hpp"
#include "lupine/core/SceneInstance.hpp"
#include "lupine/core/Component.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/core/Project.hpp"
#include "lupine/core/Prefab.hpp"
#include "lupine/core/Serialization.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/scripting/ScriptingCore.hpp"
#include "lupine/core/SceneInstance.hpp"
#include "lupine/core/ScriptComponent.hpp"
#include "lupine/components/Sprite2D.hpp"
#include "lupine/components/Sprite3D.hpp"
#include "lupine/components/AnimatedSprite2D.hpp"
#include "lupine/components/AnimatedSprite3D.hpp"
#include "lupine/components/PrimitiveMesh3D.hpp"
#include "lupine/components/StaticMesh3D.hpp"
#include "lupine/components/SkeletalMesh3D.hpp"
#include "lupine/components/MultiMeshGeneric.hpp"
#include "lupine/components/DirectionalLight3D.hpp"
#include "lupine/components/OmniLight3D.hpp"
#include "lupine/components/SpotLight3D.hpp"
#include "lupine/components/Label.hpp"
#include "lupine/components/Label3D.hpp"
#include "lupine/components/ColorRect.hpp"
#include "lupine/components/Image2D.hpp"
#include "lupine/components/Timer.hpp"
#include "lupine/components/Panel.hpp"
#include "lupine/components/Panel3D.hpp"
#include "lupine/components/Button.hpp"
#include "lupine/components/Button3D.hpp"
#include "lupine/components/ProgressBar.hpp"
#include "lupine/components/ProgressBar3D.hpp"
#include "lupine/components/Shape2D.hpp"
#include "lupine/components/Line2D.hpp"
#include "lupine/components/WorldEnvironment.hpp"
#include "lupine/components/AudioPlayer.hpp"
#include "lupine/components/AudioListener.hpp"
#include "lupine/components/RigidBody2DComponent.hpp"
#include "lupine/components/StaticBody2DComponent.hpp"
#include "lupine/components/KinematicBody2DComponent.hpp"
#include "lupine/components/AreaTrigger2DComponent.hpp"
#include "lupine/components/CollisionBody2DComponent.hpp"
#include "lupine/components/RigidBody3DComponent.hpp"
#include "lupine/components/StaticBody3DComponent.hpp"
#include "lupine/components/KinematicBody3DComponent.hpp"
#include "lupine/components/AreaTrigger3DComponent.hpp"
#include "lupine/components/CollisionMesh3DComponent.hpp"
#include "lupine/platform/VirtualFileSystem.hpp"
#include "lupine/components/CharacterController2D.hpp"
#include "lupine/components/CharacterController3D.hpp"


#include "lupine/components/TestTopdown.hpp"
#include "lupine/components/TestPlatform.hpp"
#include "lupine/components/Test3D.hpp"


#include <pybind11/embed.h>
#include <Python.h>
#include <string>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#endif

namespace py = pybind11;
namespace fs = std::filesystem;

namespace lupine {
namespace core {

static py::scoped_interpreter* g_PythonInterpreter = nullptr;
static bool g_CoreInitialized = false;

void InitializeCore() {

    if (g_CoreInitialized) {

        return;
    }

    auto& vfs = platform::VirtualFileSystem::GetInstance();
    if (!vfs.IsInitialized()) {
        if (!vfs.Initialize("")) {

        } else {

        }
    }

#ifdef LUPINE_PYTHON_MODULE

    bool shouldInitializePython = false;

#else

    bool shouldInitializePython = true;
#endif

    if (shouldInitializePython) {

        try {

#ifdef _WIN32

            wchar_t pythonPath[MAX_PATH];
            bool foundPython = false;

            FILE* pipe = _wpopen(L"py -3 -c \"import sys; print(sys.prefix)\"", L"r");
            if (pipe) {
                char buffer[MAX_PATH];
                if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {

                    size_t len = strlen(buffer);
                    if (len > 0 && buffer[len - 1] == '\n') {
                        buffer[len - 1] = '\0';
                    }

                    MultiByteToWideChar(CP_UTF8, 0, buffer, -1, pythonPath, MAX_PATH);
                    Py_SetPythonHome(pythonPath);
                    foundPython = true;

                }
                _pclose(pipe);
            }

            if (!foundPython) {
                pipe = _wpopen(L"python -c \"import sys; print(sys.prefix)\"", L"r");
                if (pipe) {
                    char buffer[MAX_PATH];
                    if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {

                        size_t len = strlen(buffer);
                        if (len > 0 && buffer[len - 1] == '\n') {
                            buffer[len - 1] = '\0';
                        }

                        MultiByteToWideChar(CP_UTF8, 0, buffer, -1, pythonPath, MAX_PATH);
                        Py_SetPythonHome(pythonPath);
                        foundPython = true;

                    }
                    _pclose(pipe);
                }
            }

            if (!foundPython) {

            }
#endif

            g_PythonInterpreter = new py::scoped_interpreter();

        }
        catch (const std::exception& e) {

        }
    }

    scripting::InitializeScripting();

    g_CoreInitialized = true;

}

void ShutdownCore() {
    if (!g_CoreInitialized) {

        return;
    }

    scripting::ShutdownScripting();

    if (g_PythonInterpreter) {
        delete g_PythonInterpreter;
        g_PythonInterpreter = nullptr;

    }

    auto& vfs = platform::VirtualFileSystem::GetInstance();
    if (vfs.IsInitialized()) {
        vfs.Shutdown();

    }

    g_CoreInitialized = false;

}

void RegisterBuiltInTypes() {

    TypeRegistry::GetInstance().RegisterType("Node",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<Node>();
        });

    TypeRegistry::GetInstance().RegisterType("Node2D",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<Node2D>();
        });

    TypeRegistry::GetInstance().RegisterType("Node3D",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<Node3D>();
        });

    TypeRegistry::GetInstance().RegisterType("Camera3D",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<Camera3D>();
        });

    TypeRegistry::GetInstance().RegisterType("Camera2D",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<Camera2D>();
        });

    TypeRegistry::GetInstance().RegisterType("CameraUI",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<CameraUI>();
        });

    TypeRegistry::GetInstance().RegisterType("Component",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<Component>();
        });

    TypeRegistry::GetInstance().RegisterType("PythonScriptComponent",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<PythonScriptComponent>();
        });

    TypeRegistry::GetInstance().RegisterType("LuaScriptComponent",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<LuaScriptComponent>();
        });

    TypeRegistry::GetInstance().RegisterType("Sprite2D",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::Sprite2D>();
        });

    TypeRegistry::GetInstance().RegisterType("Sprite3D",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::Sprite3D>();
        });

    TypeRegistry::GetInstance().RegisterType("AnimatedSprite2D",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::AnimatedSprite2D>();
        });

    TypeRegistry::GetInstance().RegisterType("AnimatedSprite3D",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::AnimatedSprite3D>();
        });

    TypeRegistry::GetInstance().RegisterType("PrimitiveMesh3D",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::PrimitiveMesh3D>();
        });

    TypeRegistry::GetInstance().RegisterType("StaticMesh3D",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::StaticMesh3D>();
        });

    TypeRegistry::GetInstance().RegisterType("SkeletalMesh3D",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::SkeletalMesh3D>();
        });

    TypeRegistry::GetInstance().RegisterType("MultiMeshGeneric",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::MultiMeshGeneric>();
        });

    TypeRegistry::GetInstance().RegisterType("DirectionalLight3D",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::DirectionalLight3D>();
        });

    TypeRegistry::GetInstance().RegisterType("OmniLight3D",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::OmniLight3D>();
        });

    TypeRegistry::GetInstance().RegisterType("SpotLight3D",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::SpotLight3D>();
        });

    TypeRegistry::GetInstance().RegisterType("Panel",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::Panel>();
        });

    TypeRegistry::GetInstance().RegisterType("Button",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::Button>();
        });

    TypeRegistry::GetInstance().RegisterType("RigidBody2DComponent",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::RigidBody2DComponent>();
        });

    TypeRegistry::GetInstance().RegisterType("StaticBody2DComponent",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::StaticBody2DComponent>();
        });

    TypeRegistry::GetInstance().RegisterType("KinematicBody2DComponent",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::KinematicBody2DComponent>();
        });

    TypeRegistry::GetInstance().RegisterType("AreaTrigger2DComponent",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::AreaTrigger2DComponent>();
        });

    TypeRegistry::GetInstance().RegisterType("CollisionBody2DComponent",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::CollisionBody2DComponent>();
        });

    TypeRegistry::GetInstance().RegisterType("RigidBody3DComponent",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::RigidBody3DComponent>();
        });

    TypeRegistry::GetInstance().RegisterType("StaticBody3DComponent",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::StaticBody3DComponent>();
        });

    TypeRegistry::GetInstance().RegisterType("KinematicBody3DComponent",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::KinematicBody3DComponent>();
        });

    TypeRegistry::GetInstance().RegisterType("AreaTrigger3DComponent",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::AreaTrigger3DComponent>();
        });

    TypeRegistry::GetInstance().RegisterType("CollisionMesh3DComponent",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::CollisionMesh3DComponent>();
        });

    TypeRegistry::GetInstance().RegisterType("CharacterController2D",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::CharacterController2D>();
        });

    TypeRegistry::GetInstance().RegisterType("CharacterController3D",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::CharacterController3D>();
        });
    TypeRegistry::GetInstance().RegisterType("TestTopdown",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::TestTopdown>();
        });
        TypeRegistry::GetInstance().RegisterType("TestPlatform",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::TestPlatform>();
        });
        TypeRegistry::GetInstance().RegisterType("Test3D",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::Test3D>();
        });
        TypeRegistry::GetInstance().RegisterType("WorldEnvironment",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::WorldEnvironment>();
        });
        TypeRegistry::GetInstance().RegisterType("AudioListener",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::AudioListener>();
        });
        TypeRegistry::GetInstance().RegisterType("AudioPlayer",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::AudioPlayer>();
        });
        TypeRegistry::GetInstance().RegisterType("Timer",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::Timer>();
        });
        TypeRegistry::GetInstance().RegisterType("Label",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::Label>();
        });
        TypeRegistry::GetInstance().RegisterType("Label3D",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::Label3D>();
        });
        TypeRegistry::GetInstance().RegisterType("ColorRect",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::ColorRect>();
        });
        TypeRegistry::GetInstance().RegisterType("Image2D",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::Image2D>();
        });
        TypeRegistry::GetInstance().RegisterType("Panel3D",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::Panel3D>();
        });
        TypeRegistry::GetInstance().RegisterType("Button3D",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::Button3D>();
        });

    TypeRegistry::GetInstance().RegisterType("Scene",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<Scene>();
        });

    TypeRegistry::GetInstance().RegisterType("SceneInstance",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<SceneInstance>();
        });

    TypeRegistry::GetInstance().RegisterType("ProjectSettings",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<ProjectSettings>();
        });

    TypeRegistry::GetInstance().RegisterType("Prefab",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<Prefab>();
        });

}

}
}

