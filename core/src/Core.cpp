#include "lupine/core/Core.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/CameraNodes.hpp"
#include "lupine/core/UILayerNode.hpp"
#include "lupine/core/SceneInstance.hpp"
#include "lupine/core/Component.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/core/Project.hpp"
#include "lupine/core/Prefab.hpp"
#include "lupine/core/Serialization.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/scripting/ScriptingCore.hpp"
#include "lupine/core/PrefabInstance.hpp"
#include "lupine/core/ScriptComponent.hpp"
#include "lupine/components/ComponentRegistry.hpp"

#include "lupine/platform/VirtualFileSystem.hpp"
#include "lupine/platform/PackFile.hpp"

#include <string>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace lupine {
namespace core {

static bool g_CoreInitialized = false;

static std::vector<std::string> g_CommandLineArgs;

void SetCommandLineArgs(const std::vector<std::string>& args) {
    g_CommandLineArgs = args;
}

const std::vector<std::string>& GetCommandLineArgs() {
    return g_CommandLineArgs;
}

void InitializeCore() {

    if (g_CoreInitialized) {

        return;
    }

    // The VFS is initialized in pack mode too. The PackFileSystem is a read-only
    // archive that only backs packed (res://) content - it does not provide the
    // writable user:// space, so without these mounts an exported game cannot resolve
    // user://, and every save, settings write and config read fails with ENOENT on the
    // unresolved virtual path. VFS reads consult the pack first, so the res:// mount
    // here is only a fallback for anything the pack does not hold.
    platform::VirtualFileSystem& vfs = platform::VirtualFileSystem::GetInstance();
    if (!vfs.IsInitialized()) {
        if (!vfs.Initialize("")) {
            LOG_ERROR(LogCategory::Core,
                      "InitializeCore: virtual filesystem failed to initialize; "
                      "user:// (saves, settings) will not resolve");
        }
    }

    scripting::InitializeScripting();

    RegisterBuiltInTypes();

    g_CoreInitialized = true;

}

void ShutdownCore() {
    if (!g_CoreInitialized) {

        return;
    }

    scripting::ShutdownScripting();

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

    TypeRegistry::GetInstance().RegisterType("UILayer",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<UILayer>();
        });

    TypeRegistry::GetInstance().RegisterType("Component",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<Component>();
        });

#ifdef LUPINE_HAS_MICROPYTHON
    TypeRegistry::GetInstance().RegisterType("PythonScriptComponent",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<PythonScriptComponent>();
        });
#endif

    TypeRegistry::GetInstance().RegisterType("LuaScriptComponent",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<LuaScriptComponent>();
        });

#ifdef LUPINE_HAS_MRUBY
    TypeRegistry::GetInstance().RegisterType("MRubyScriptComponent",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<MRubyScriptComponent>();
        });
#endif

    TypeRegistry::GetInstance().RegisterType("Sprite2D",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::Sprite2D>();
        });

    TypeRegistry::GetInstance().RegisterType("Sprite3D",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::Sprite3D>();
        });

    TypeRegistry::GetInstance().RegisterType("Light2D",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::Light2D>();
        });

    TypeRegistry::GetInstance().RegisterType("AnimatedSprite2D",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::AnimatedSprite2D>();
        });

    TypeRegistry::GetInstance().RegisterType("AnimatedSprite3D",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::AnimatedSprite3D>();
        });

    TypeRegistry::GetInstance().RegisterType("GifPlayer",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::GifPlayer>();
        });

    TypeRegistry::GetInstance().RegisterType("VideoPlayer",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::VideoPlayer>();
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

    TypeRegistry::GetInstance().RegisterType("ScatterMultiMesh",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::ScatterMultiMesh>();
        });

    TypeRegistry::GetInstance().RegisterType("CollisionScatterMultiMesh",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::CollisionScatterMultiMesh>();
        });

    TypeRegistry::GetInstance().RegisterType("NodeScatter",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::NodeScatter>();
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
        TypeRegistry::GetInstance().RegisterType("SubViewport",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::SubViewport>();
        });
        TypeRegistry::GetInstance().RegisterType("CameraEffectColorGrade",
        []() -> std::shared_ptr<ISerializable> { return std::make_shared<components::CameraEffectColorGrade>(); });
        TypeRegistry::GetInstance().RegisterType("CameraEffectTonemap",
        []() -> std::shared_ptr<ISerializable> { return std::make_shared<components::CameraEffectTonemap>(); });
        TypeRegistry::GetInstance().RegisterType("CameraEffectVignette",
        []() -> std::shared_ptr<ISerializable> { return std::make_shared<components::CameraEffectVignette>(); });
        TypeRegistry::GetInstance().RegisterType("CameraEffectFilmGrain",
        []() -> std::shared_ptr<ISerializable> { return std::make_shared<components::CameraEffectFilmGrain>(); });
        TypeRegistry::GetInstance().RegisterType("CameraEffectColorInvert",
        []() -> std::shared_ptr<ISerializable> { return std::make_shared<components::CameraEffectColorInvert>(); });
        TypeRegistry::GetInstance().RegisterType("CameraEffectPosterize",
        []() -> std::shared_ptr<ISerializable> { return std::make_shared<components::CameraEffectPosterize>(); });
        TypeRegistry::GetInstance().RegisterType("CameraEffectHueShift",
        []() -> std::shared_ptr<ISerializable> { return std::make_shared<components::CameraEffectHueShift>(); });
        TypeRegistry::GetInstance().RegisterType("CameraEffectBlur",
        []() -> std::shared_ptr<ISerializable> { return std::make_shared<components::CameraEffectBlur>(); });
        TypeRegistry::GetInstance().RegisterType("CameraEffectGlow",
        []() -> std::shared_ptr<ISerializable> { return std::make_shared<components::CameraEffectGlow>(); });
        TypeRegistry::GetInstance().RegisterType("CameraEffectOutline",
        []() -> std::shared_ptr<ISerializable> { return std::make_shared<components::CameraEffectOutline>(); });
        TypeRegistry::GetInstance().RegisterType("CameraEffectPixelate",
        []() -> std::shared_ptr<ISerializable> { return std::make_shared<components::CameraEffectPixelate>(); });
        TypeRegistry::GetInstance().RegisterType("CameraEffectSharpen",
        []() -> std::shared_ptr<ISerializable> { return std::make_shared<components::CameraEffectSharpen>(); });
        TypeRegistry::GetInstance().RegisterType("CameraEffectChromaticAberration",
        []() -> std::shared_ptr<ISerializable> { return std::make_shared<components::CameraEffectChromaticAberration>(); });
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
        TypeRegistry::GetInstance().RegisterType("Container",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::Container>();
        });
        TypeRegistry::GetInstance().RegisterType("PaddingContainer",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::PaddingContainer>();
        });
        TypeRegistry::GetInstance().RegisterType("CenterContainer",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::CenterContainer>();
        });
        TypeRegistry::GetInstance().RegisterType("HorizontalContainer",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::HorizontalContainer>();
        });
        TypeRegistry::GetInstance().RegisterType("VerticalContainer",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::VerticalContainer>();
        });
        TypeRegistry::GetInstance().RegisterType("GridContainer",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::GridContainer>();
        });
        TypeRegistry::GetInstance().RegisterType("DockContainer",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::DockContainer>();
        });
        TypeRegistry::GetInstance().RegisterType("Stack",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::Stack>();
        });
        TypeRegistry::GetInstance().RegisterType("Wrap",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::Wrap>();
        });
        TypeRegistry::GetInstance().RegisterType("SplitContainer",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::SplitContainer>();
        });
        TypeRegistry::GetInstance().RegisterType("AspectRatioContainer",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::AspectRatioContainer>();
        });
        TypeRegistry::GetInstance().RegisterType("Spacer",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::Spacer>();
        });

        TypeRegistry::GetInstance().RegisterType("VectorGraphic2D",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::VectorGraphic2D>();
        });

    // Networking components. Registered here (not only via REGISTER_COMPONENT_TYPE
    // in Components.cpp) so they are creatable in every context that calls
    // InitializeCore - including the C-API DLL, which does not link Components.cpp.
    TypeRegistry::GetInstance().RegisterType("NetworkObject",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::NetworkObject>();
        });
    TypeRegistry::GetInstance().RegisterType("NetworkSynchronizer",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::NetworkSynchronizer>();
        });
    TypeRegistry::GetInstance().RegisterType("NetworkTransform2D",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::NetworkTransform2D>();
        });
    TypeRegistry::GetInstance().RegisterType("NetworkTransform3D",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::NetworkTransform3D>();
        });
    TypeRegistry::GetInstance().RegisterType("NetworkSpawner",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::NetworkSpawner>();
        });
    TypeRegistry::GetInstance().RegisterType("NetworkController",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::NetworkController>();
        });
    TypeRegistry::GetInstance().RegisterType("NetworkAnimator",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::NetworkAnimator>();
        });
    TypeRegistry::GetInstance().RegisterType("NetworkRigidBody2D",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::NetworkRigidBody2D>();
        });
    TypeRegistry::GetInstance().RegisterType("NetworkRigidBody3D",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<components::NetworkRigidBody3D>();
        });

    TypeRegistry::GetInstance().RegisterType("Scene",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<Scene>();
        });

    TypeRegistry::GetInstance().RegisterType("SceneInstance",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<SceneInstance>();
        });

    TypeRegistry::GetInstance().RegisterType("SceneInstance2D",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<SceneInstance2D>();
        });

    TypeRegistry::GetInstance().RegisterType("SceneInstance3D",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<SceneInstance3D>();
        });

    TypeRegistry::GetInstance().RegisterType("PrefabInstance",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<PrefabInstance>();
        });

    TypeRegistry::GetInstance().RegisterType("PrefabInstance2D",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<PrefabInstance2D>();
        });

    TypeRegistry::GetInstance().RegisterType("PrefabInstance3D",
        []() -> std::shared_ptr<ISerializable> {
            return std::make_shared<PrefabInstance3D>();
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

