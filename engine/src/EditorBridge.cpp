#include "lupine/engine/EditorBridge.hpp"
#include "lupine/engine/Engine.hpp"
#include "lupine/core/Project.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/CameraNodes.hpp"
#include "lupine/core/Component.hpp"
#include "lupine/core/ScriptComponent.hpp"
#include "lupine/core/Prefab.hpp"
#include "lupine/core/SceneInstance.hpp"
#include "lupine/core/EditorCommands.hpp"
#include "lupine/platform/Platform.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/rendering/Rendering.hpp"
#include "lupine/core/Serialization.hpp"
#include "lupine/components/Components.hpp"
#include "lupine/components/AnimatedSprite2D.hpp"
#include "lupine/components/AnimatedSprite3D.hpp"
#include "lupine/components/SkeletalMesh3D.hpp"
#include "lupine/components/AudioPlayer.hpp"
#include "lupine/components/AudioListener.hpp"
#include "lupine/components/WorldEnvironment.hpp"
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
#include "lupine/components/CharacterController2D.hpp"
#include "lupine/components/CharacterController3D.hpp"
#include "lupine/components/Label3D.hpp"
#include "lupine/components/Panel3D.hpp"
#include "lupine/components/Container.hpp"
#include "lupine/components/Button3D.hpp"
#include "lupine/components/ProgressBar.hpp"
#include "lupine/components/ProgressBar3D.hpp"
#include "lupine/components/YSort.hpp"
#include "lupine/components/MultiMeshGeneric.hpp"

#include "lupine/components/TestPlatform.hpp"
#include "lupine/components/TestTopdown.hpp"
#include "lupine/components/Test3D.hpp"

#include "lupine/audio/AudioManager.hpp"
#include "lupine/asset/AudioAsset.hpp"
#include "lupine/math/Ray.hpp"
#include "lupine/math/AABB.hpp"
#include <nlohmann/json.hpp>
#include <cmath>
#include <limits>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace lupine {
namespace engine {

using core::Node;
using core::Node2D;
using core::Node3D;

using components::Sprite2D;
using components::Sprite3D;
using components::AnimatedSprite2D;
using components::AnimatedSprite3D;
using components::PrimitiveMesh3D;
using components::StaticMesh3D;
using components::SkeletalMesh3D;
using components::Label;
using components::Label3D;
using components::DirectionalLight3D;
using components::OmniLight3D;
using components::SpotLight3D;
using components::Timer;
using components::ColorRect;
using components::Image2D;
using components::Panel;
using components::Panel3D;
using components::Container;
using components::ProgressBar;
using components::ProgressBar3D;
using components::Button;
using components::Button3D;
using components::Shape2D;
using components::Line2D;
using components::AudioPlayer;
using components::AudioListener;
using components::WorldEnvironment;
using components::CollisionBody2DComponent;
using components::RigidBody2DComponent;
using components::StaticBody2DComponent;
using components::KinematicBody2DComponent;
using components::AreaTrigger2DComponent;
using components::RigidBody3DComponent;
using components::StaticBody3DComponent;
using components::KinematicBody3DComponent;
using components::AreaTrigger3DComponent;
using components::CharacterController2D;
using components::CharacterController3D;
using components::Panel3D;
using components::ProgressBar;
using components::YSort;
using components::MultiMeshGeneric;

using components::TestPlatform;
using components::Test3D;

using core::Component;
using core::PythonScriptComponent;
using core::LuaScriptComponent;
using core::Prefab;

EditorBridge::EditorBridge()
    : m_Initialized(false)
    , m_Backend(GraphicsBackend::OpenGL)
    , m_SceneDirty(false)
    , m_CommandHistory(std::make_unique<core::CommandHistory>(100)) {
}

EditorBridge::~EditorBridge() {
    Shutdown();
}

bool EditorBridge::Initialize(GraphicsBackend backend) {
    if (m_Initialized) {

        return true;
    }

    m_Backend = backend;

    m_SceneManager = std::make_unique<core::SceneManager>();
    if (!m_SceneManager->Initialize()) {

        m_SceneManager.reset();
        return false;
    }

    m_RenderWorld = std::make_unique<RenderWorld>();
    if (!m_RenderWorld->initialize(backend)) {

        m_RenderWorld.reset();
        return false;
    }

    audio::AudioManager::GetInstance().Initialize();

    RegisterBuiltInNodeTypes();
    RegisterBuiltInComponentTypes();

    m_Initialized = true;

    return true;
}

void EditorBridge::Shutdown() {
    if (!m_Initialized) return;

    m_ActiveScene.reset();

    for (auto& pair : m_RenderViews) {
        if (m_RenderWorld && pair.second.swapchain.isValid()) {
            m_RenderWorld->getDevice()->destroySwapchain(pair.second.swapchain);
        }
        if (m_RenderWorld && pair.second.viewID != 0) {
            m_RenderWorld->destroyRenderView(pair.second.viewID);
        }
    }
    m_RenderViews.clear();

    if (m_RenderWorld) {
        m_RenderWorld->shutdown();
        m_RenderWorld.reset();
    }

    if (m_SceneManager) {
        m_SceneManager->Shutdown();
        m_SceneManager.reset();
    }

    audio::AudioManager::GetInstance().Shutdown();

    m_Initialized = false;

}

RenderViewID EditorBridge::CreateRenderView(void* windowHandle, uint32_t width, uint32_t height) {
    if (!m_Initialized || !m_RenderWorld) {

        return 0;
    }

    NativeWindowHandle nativeHandle;
    nativeHandle.platformHandle = windowHandle;

    SwapchainDesc swapchainDesc;
    swapchainDesc.window = nativeHandle;
    swapchainDesc.width = width;
    swapchainDesc.height = height;
    swapchainDesc.colorFormat = TextureFormat::RGBA8_SRGB;
    swapchainDesc.vsync = true;

    SwapchainHandle swapchain = m_RenderWorld->getDevice()->createSwapchain(swapchainDesc);
    if (!swapchain.isValid()) {

        return 0;
    }

    auto camera = std::make_unique<Camera3D>();
    camera->position = math::Vec3(0, 5, 10);
    camera->target = math::Vec3(0, 0, 0);
    camera->up = math::Vec3(0, 1, 0);
    camera->fov = 60.0f;
    camera->nearPlane = 0.1f;
    camera->farPlane = 1000.0f;

    camera->isEditorCamera = true;

    RenderViewID viewID = m_RenderWorld->createRenderView(std::move(camera));
    if (viewID == 0) {

        m_RenderWorld->getDevice()->destroySwapchain(swapchain);
        return 0;
    }

    lupine::RenderView* renderView = m_RenderWorld->getRenderView(viewID);
    if (renderView) {
        renderView->setSwapchain(swapchain);

        Viewport viewport;
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = static_cast<float>(width);
        viewport.height = static_cast<float>(height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        renderView->setViewport(viewport);

        ScissorRect scissor;
        scissor.x = 0;
        scissor.y = 0;
        scissor.width = 0;
        scissor.height = 0;
        renderView->setScissor(scissor);
    }

    RenderViewInfo info;
    info.viewID = viewID;
    info.swapchain = swapchain;
    info.width = width;
    info.height = height;
    info.isValid = true;
    info.viewMode = ViewMode::View3D;
    info.scene = nullptr;
    m_RenderViews[viewID] = info;

    return viewID;
}

void EditorBridge::DestroyRenderView(RenderViewID viewID) {
    auto it = m_RenderViews.find(viewID);
    if (it == m_RenderViews.end()) {

        return;
    }

    if (m_RenderWorld) {
        if (it->second.swapchain.isValid()) {
            m_RenderWorld->getDevice()->destroySwapchain(it->second.swapchain);
        }
        m_RenderWorld->destroyRenderView(viewID);
    }

    m_RenderViews.erase(it);

}

void EditorBridge::ResizeRenderView(RenderViewID viewID, uint32_t width, uint32_t height) {
    auto it = m_RenderViews.find(viewID);
    if (it == m_RenderViews.end()) {

        return;
    }

    if (!m_RenderWorld) return;

    if (width < 1 || height < 1) {

        return;
    }

    if (it->second.swapchain.isValid()) {
        m_RenderWorld->getDevice()->resizeSwapchain(it->second.swapchain, width, height);
    }

    lupine::RenderView* renderView = m_RenderWorld->getRenderView(viewID);
    if (renderView) {
        Viewport viewport;
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = static_cast<float>(width);
        viewport.height = static_cast<float>(height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        renderView->setViewport(viewport);

        ScissorRect scissor;
        scissor.x = 0;
        scissor.y = 0;
        scissor.width = 0;
        scissor.height = 0;
        renderView->setScissor(scissor);
    }

    it->second.width = width;
    it->second.height = height;

}

void EditorBridge::RenderView(RenderViewID viewID) {
    if (!m_Initialized || !m_RenderWorld) return;

    auto it = m_RenderViews.find(viewID);
    if (it == m_RenderViews.end()) {
        return;
    }

    if (it->second.width < 1 || it->second.height < 1) {
        return;
    }

    if (!it->second.swapchain.isValid()) {
        return;
    }

    if (it->second.scene) {

        float deltaTime = 1.0f / 60.0f;
        it->second.scene->Update(deltaTime);

    }

    try {

        m_RenderWorld->beginFrame();

        RenderVoxelBuilder(viewID);

        m_RenderWorld->renderView(viewID);

        m_RenderWorld->endFrame(false);

        m_RenderWorld->presentView(viewID);
    } catch (const std::exception& e) {

    } catch (...) {

    }
}

RenderViewInfo EditorBridge::GetRenderViewInfo(RenderViewID viewID) const {
    auto it = m_RenderViews.find(viewID);
    if (it != m_RenderViews.end()) {
        return it->second;
    }
    return RenderViewInfo();
}

void EditorBridge::SetViewMode(RenderViewID viewID, ViewMode mode) {
    auto it = m_RenderViews.find(viewID);
    if (it == m_RenderViews.end()) {

        return;
    }

    if (!m_RenderWorld) return;

    it->second.viewMode = mode;

    UpdateCameraForViewMode(viewID, mode);

}

ViewMode EditorBridge::GetViewMode(RenderViewID viewID) const {
    auto it = m_RenderViews.find(viewID);
    if (it != m_RenderViews.end()) {
        return it->second.viewMode;
    }
    return ViewMode::View3D;
}

void EditorBridge::SetViewScene(RenderViewID viewID, std::shared_ptr<Scene> scene) {
    auto it = m_RenderViews.find(viewID);
    if (it == m_RenderViews.end()) {

        return;
    }

    if (!m_RenderWorld) return;

    it->second.scene = scene;

    lupine::RenderView* renderView = m_RenderWorld->getRenderView(viewID);
    if (renderView) {
        renderView->setScene(scene.get());

    }
}

std::shared_ptr<Scene> EditorBridge::GetViewScene(RenderViewID viewID) const {
    auto it = m_RenderViews.find(viewID);
    if (it != m_RenderViews.end()) {
        return it->second.scene;
    }
    return nullptr;
}

void EditorBridge::SetViewClearColor(RenderViewID viewID, const math::Color& color) {
    if (!m_RenderWorld) return;

    lupine::RenderView* renderView = m_RenderWorld->getRenderView(viewID);
    if (renderView && renderView->getCamera()) {
        renderView->getCamera()->clearColor = color;
    }
}

void EditorBridge::SetViewDebugRenderingEnabled(RenderViewID viewID, bool enabled) {
    auto it = m_RenderViews.find(viewID);
    if (it != m_RenderViews.end()) {

        it->second.debugRenderingEnabled = enabled;

        if (m_RenderWorld) {
            lupine::RenderView* renderView = m_RenderWorld->getRenderView(viewID);
            if (renderView) {
                renderView->setDebugRenderingEnabled(enabled);
            }
        }

    }
}

void EditorBridge::SetViewGridRenderingEnabled(RenderViewID viewID, bool enabled) {
    if (m_RenderWorld) {
        lupine::RenderView* renderView = m_RenderWorld->getRenderView(viewID);
        if (renderView) {
            renderView->setGridRenderingEnabled(enabled);

        }
    }
}

void EditorBridge::SetViewCameraPreviewEnabled(RenderViewID viewID, bool enabled) {
    if (m_RenderWorld) {
        lupine::RenderView* renderView = m_RenderWorld->getRenderView(viewID);
        if (renderView) {
            renderView->setCameraPreviewEnabled(enabled);

        }
    }
}

void EditorBridge::SetViewCollisionShapesEnabled(RenderViewID viewID, bool enabled) {
    if (m_RenderWorld) {
        lupine::RenderView* renderView = m_RenderWorld->getRenderView(viewID);
        if (renderView) {
            renderView->setCollisionShapesEnabled(enabled);
        }
    }
}

void EditorBridge::SetProjectSettings(uint32_t windowWidth, uint32_t windowHeight) {
    m_ProjectWindowWidth = windowWidth;
    m_ProjectWindowHeight = windowHeight;

    if (m_RenderWorld) {
        m_RenderWorld->setProjectWindowSize(windowWidth, windowHeight);
    }

    if (m_ActiveScene) {
        float newAspectRatio = static_cast<float>(windowWidth) / static_cast<float>(windowHeight);
        std::shared_ptr<Node> root = m_ActiveScene->GetRoot();
        if (root) {
            UpdateCameraNodesRecursive(root, newAspectRatio, windowWidth, windowHeight);
        }
    }

}

void EditorBridge::SetTextureFiltering(FilterMode minFilter, FilterMode magFilter) {
    if (m_RenderWorld) {
        m_RenderWorld->setTextureFiltering(minFilter, magFilter);

    }
}

std::shared_ptr<Scene> EditorBridge::LoadScene(const std::string& scenePath) {
    auto scene = std::make_shared<Scene>();
    if (!scene->Load(scenePath)) {

        return nullptr;
    }

    return scene;
}

void EditorBridge::SetActiveScene(std::shared_ptr<Scene> scene) {
    if (m_ActiveScene) {
        m_ActiveScene->Shutdown();
    }

    m_ActiveScene = scene;
    m_SceneDirty = false;

    if (m_ActiveScene) {

        m_ActiveScene->SetInEditor(true);
        m_ActiveScene->Initialize();

    }
}

bool EditorBridge::SaveActiveScene() {
    if (!m_ActiveScene) {

        return false;
    }

    if (!m_ActiveScene->Save()) {

        return false;
    }

    m_SceneDirty = false;

    return true;
}

void EditorBridge::RegisterBuiltInNodeTypes() {
    m_NodeTypes.clear();

    m_NodeTypes.push_back(TypeInfo("Node", "Node", "Base", true));
    m_NodeTypes.push_back(TypeInfo("Node2D", "Node", "2D", true));
    m_NodeTypes.push_back(TypeInfo("Node3D", "Node", "3D", true));
    m_NodeTypes.push_back(TypeInfo("Camera2D", "Node", "2D", true));
    m_NodeTypes.push_back(TypeInfo("Camera3D", "Node", "3D", true));
    m_NodeTypes.push_back(TypeInfo("CameraUI", "Node", "UI", true));
    m_NodeTypes.push_back(TypeInfo("SceneInstance", "Node", "Scene", true));

    core::TypeRegistry::GetInstance().RegisterType("Node",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<Node>();
        });

    core::TypeRegistry::GetInstance().RegisterType("Node2D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<Node2D>();
        });

    core::TypeRegistry::GetInstance().RegisterType("Node3D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<Node3D>();
        });

    core::TypeRegistry::GetInstance().RegisterType("Camera2D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<core::Camera2D>();
        });

    core::TypeRegistry::GetInstance().RegisterType("Camera3D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<core::Camera3D>();
        });

    core::TypeRegistry::GetInstance().RegisterType("CameraUI",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<core::CameraUI>();
        });

    core::TypeRegistry::GetInstance().RegisterType("SceneInstance",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<core::SceneInstance>();
        });

}

void EditorBridge::RegisterBuiltInComponentTypes() {
    m_ComponentTypes.clear();

    m_ComponentTypes.push_back(TypeInfo("Component", "Component", "Base", true));
    m_ComponentTypes.push_back(TypeInfo("PythonScriptComponent", "Component", "Scripting", true));
    m_ComponentTypes.push_back(TypeInfo("LuaScriptComponent", "Component", "Scripting", true));
    m_ComponentTypes.push_back(TypeInfo("Sprite2D", "Component", "2D", true));
    m_ComponentTypes.push_back(TypeInfo("Sprite3D", "Component", "3D", true));
    m_ComponentTypes.push_back(TypeInfo("AnimatedSprite2D", "Component", "2D", true));
    m_ComponentTypes.push_back(TypeInfo("AnimatedSprite3D", "Component", "3D", true));
    m_ComponentTypes.push_back(TypeInfo("PrimitiveMesh3D", "Component", "3D", true));
    m_ComponentTypes.push_back(TypeInfo("StaticMesh3D", "Component", "3D", true));
    m_ComponentTypes.push_back(TypeInfo("SkeletalMesh3D", "Component", "3D", true));
    m_ComponentTypes.push_back(TypeInfo("Label", "Component", "UI", true));
    m_ComponentTypes.push_back(TypeInfo("Label3D", "Component", "3D", true));
    m_ComponentTypes.push_back(TypeInfo("ProgressBar", "Component", "UI", true));
    m_ComponentTypes.push_back(TypeInfo("ProgressBar3D", "Component", "3D", true));
    m_ComponentTypes.push_back(TypeInfo("ColorRect", "Component", "UI", true));
    m_ComponentTypes.push_back(TypeInfo("Image2D", "Component", "UI", true));
    m_ComponentTypes.push_back(TypeInfo("Shape2D", "Component", "2D", true));
    m_ComponentTypes.push_back(TypeInfo("Line2D", "Component", "2D", true));
    m_ComponentTypes.push_back(TypeInfo("DirectionalLight3D", "Component", "3D/Lighting", true));
    m_ComponentTypes.push_back(TypeInfo("OmniLight3D", "Component", "3D/Lighting", true));
    m_ComponentTypes.push_back(TypeInfo("SpotLight3D", "Component", "3D/Lighting", true));
    m_ComponentTypes.push_back(TypeInfo("Timer", "Component", "Utility", true));
    m_ComponentTypes.push_back(TypeInfo("Panel", "Component", "UI", true));
    m_ComponentTypes.push_back(TypeInfo("Container", "Component", "UI/Containers", true));
    m_ComponentTypes.push_back(TypeInfo("Button", "Component", "UI", true));
    m_ComponentTypes.push_back(TypeInfo("Panel3D", "Component", "3D", true));
    m_ComponentTypes.push_back(TypeInfo("Button3D", "Component", "3D", true));
    m_ComponentTypes.push_back(TypeInfo("YSort", "Component", "2D", true));
    m_ComponentTypes.push_back(TypeInfo("MultiMeshGeneric", "Component", "3D", true));

    core::TypeRegistry::GetInstance().RegisterType("Component",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<Component>();
        });

    core::TypeRegistry::GetInstance().RegisterType("PythonScriptComponent",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<PythonScriptComponent>();
        });

    core::TypeRegistry::GetInstance().RegisterType("LuaScriptComponent",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<LuaScriptComponent>();
        });

    core::TypeRegistry::GetInstance().RegisterType("Sprite2D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<Sprite2D>();
        });

    core::TypeRegistry::GetInstance().RegisterType("Sprite3D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<Sprite3D>();
        });

    core::TypeRegistry::GetInstance().RegisterType("ProgressBar",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<ProgressBar>();
        });

    core::TypeRegistry::GetInstance().RegisterType("ProgressBar3D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<ProgressBar3D>();
        });

    core::TypeRegistry::GetInstance().RegisterType("AnimatedSprite2D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<AnimatedSprite2D>();
        });

    core::TypeRegistry::GetInstance().RegisterType("AnimatedSprite3D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<AnimatedSprite3D>();
        });

    core::TypeRegistry::GetInstance().RegisterType("PrimitiveMesh3D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<PrimitiveMesh3D>();
        });

    core::TypeRegistry::GetInstance().RegisterType("StaticMesh3D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<StaticMesh3D>();
        });

    core::TypeRegistry::GetInstance().RegisterType("SkeletalMesh3D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<SkeletalMesh3D>();
        });

    core::TypeRegistry::GetInstance().RegisterType("Label",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<Label>();
        });

    core::TypeRegistry::GetInstance().RegisterType("Label3D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<Label3D>();
        });

    core::TypeRegistry::GetInstance().RegisterType("DirectionalLight3D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<DirectionalLight3D>();
        });

    core::TypeRegistry::GetInstance().RegisterType("OmniLight3D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<OmniLight3D>();
        });

    core::TypeRegistry::GetInstance().RegisterType("SpotLight3D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<SpotLight3D>();
        });

    core::TypeRegistry::GetInstance().RegisterType("ColorRect",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<ColorRect>();
        });

    core::TypeRegistry::GetInstance().RegisterType("Image2D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<Image2D>();
        });

    core::TypeRegistry::GetInstance().RegisterType("Timer",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<Timer>();
        });

    core::TypeRegistry::GetInstance().RegisterType("Panel",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<Panel>();
        });

    core::TypeRegistry::GetInstance().RegisterType("Container",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<Container>();
        });

    core::TypeRegistry::GetInstance().RegisterType("Button",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<Button>();
        });

    core::TypeRegistry::GetInstance().RegisterType("Panel3D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<Panel3D>();
        });

    core::TypeRegistry::GetInstance().RegisterType("Button3D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<Button3D>();
        });

    core::TypeRegistry::GetInstance().RegisterType("Shape2D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<Shape2D>();
        });

    core::TypeRegistry::GetInstance().RegisterType("Line2D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<Line2D>();
        });
    core::TypeRegistry::GetInstance().RegisterType("YSort",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<YSort>();
        });
    core::TypeRegistry::GetInstance().RegisterType("MultiMeshGeneric",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<MultiMeshGeneric>();
        });

    m_ComponentTypes.push_back(TypeInfo("AudioPlayer", "Component", "Audio", true));
    m_ComponentTypes.push_back(TypeInfo("AudioListener", "Component", "Audio", true));

    core::TypeRegistry::GetInstance().RegisterType("AudioPlayer",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<components::AudioPlayer>();
        });

    core::TypeRegistry::GetInstance().RegisterType("AudioListener",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<components::AudioListener>();
        });

    m_ComponentTypes.push_back(TypeInfo("WorldEnvironment", "Component", "3D/Environment", true));

    core::TypeRegistry::GetInstance().RegisterType("WorldEnvironment",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<components::WorldEnvironment>();
        });

    m_ComponentTypes.push_back(TypeInfo("TestPlatform", "Component", "Test", true));
    m_ComponentTypes.push_back(TypeInfo("TestTopdown", "Component", "Test", true));
    m_ComponentTypes.push_back(TypeInfo("Test3D", "Component", "Test", true));

    core::TypeRegistry::GetInstance().RegisterType("TestPlatform",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<components::TestPlatform>();
        });
    core::TypeRegistry::GetInstance().RegisterType("TestTopdown",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<components::TestTopdown>();
        });
    core::TypeRegistry::GetInstance().RegisterType("Test3D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<components::Test3D>();
        });

    m_ComponentTypes.push_back(TypeInfo("RigidBody2DComponent", "Component", "Physics/2D", true));
    m_ComponentTypes.push_back(TypeInfo("StaticBody2DComponent", "Component", "Physics/2D", true));
    m_ComponentTypes.push_back(TypeInfo("KinematicBody2DComponent", "Component", "Physics/2D", true));
    m_ComponentTypes.push_back(TypeInfo("AreaTrigger2DComponent", "Component", "Physics/2D", true));
    m_ComponentTypes.push_back(TypeInfo("CollisionBody2DComponent", "Component", "Physics/2D", true));
    m_ComponentTypes.push_back(TypeInfo("CharacterController2D", "Component", "Physics/2D", true));

    core::TypeRegistry::GetInstance().RegisterType("RigidBody2DComponent",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<components::RigidBody2DComponent>();
        });

    core::TypeRegistry::GetInstance().RegisterType("StaticBody2DComponent",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<components::StaticBody2DComponent>();
        });

    core::TypeRegistry::GetInstance().RegisterType("KinematicBody2DComponent",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<components::KinematicBody2DComponent>();
        });

    core::TypeRegistry::GetInstance().RegisterType("AreaTrigger2DComponent",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<components::AreaTrigger2DComponent>();
        });

    core::TypeRegistry::GetInstance().RegisterType("CollisionBody2DComponent",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<components::CollisionBody2DComponent>();
        });

    core::TypeRegistry::GetInstance().RegisterType("CharacterController2D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<components::CharacterController2D>();
        });

    m_ComponentTypes.push_back(TypeInfo("RigidBody3DComponent", "Component", "Physics/3D", true));
    m_ComponentTypes.push_back(TypeInfo("StaticBody3DComponent", "Component", "Physics/3D", true));
    m_ComponentTypes.push_back(TypeInfo("KinematicBody3DComponent", "Component", "Physics/3D", true));
    m_ComponentTypes.push_back(TypeInfo("AreaTrigger3DComponent", "Component", "Physics/3D", true));
    m_ComponentTypes.push_back(TypeInfo("CollisionMesh3DComponent", "Component", "Physics/3D", true));
    m_ComponentTypes.push_back(TypeInfo("CharacterController3D", "Component", "Physics/3D", true));

    core::TypeRegistry::GetInstance().RegisterType("RigidBody3DComponent",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<components::RigidBody3DComponent>();
        });

    core::TypeRegistry::GetInstance().RegisterType("StaticBody3DComponent",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<components::StaticBody3DComponent>();
        });

    core::TypeRegistry::GetInstance().RegisterType("KinematicBody3DComponent",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<components::KinematicBody3DComponent>();
        });

    core::TypeRegistry::GetInstance().RegisterType("AreaTrigger3DComponent",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<components::AreaTrigger3DComponent>();
        });

    core::TypeRegistry::GetInstance().RegisterType("CollisionMesh3DComponent",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<components::CollisionMesh3DComponent>();
        });
    core::TypeRegistry::GetInstance().RegisterType("CharacterController3D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<components::CharacterController3D>();
        });
}

void EditorBridge::ScanProjectTypes(const std::string& projectPath) {
    m_ProjectPath = projectPath;

    std::string projectDir = platform::Path::GetDirectory(projectPath);

    std::string nodeDir = platform::Path::Join(projectDir, "node");
    if (platform::FileSystem::Exists(nodeDir)) {
        ScanNodesDirectory(nodeDir);
    }

    std::string componentDir = platform::Path::Join(projectDir, "component");
    if (platform::FileSystem::Exists(componentDir)) {
        ScanComponentsDirectory(componentDir);
    }

    std::string prefabDir = platform::Path::Join(projectDir, "prefab");
    if (platform::FileSystem::Exists(prefabDir)) {
        ScanPrefabsDirectory(prefabDir);
    }

}

void EditorBridge::ScanNodesDirectory(const std::string& directory) {

    auto result = platform::FileSystem::ListDirectory(directory);
    if (!result.success) {

        return;
    }

    for (const auto& fullPath : result.data) {
        if (platform::FileSystem::IsDirectory(fullPath)) {

            ScanNodesDirectory(fullPath);
        } else {

            std::string ext = platform::Path::GetExtension(fullPath);
            if (ext == ".py" || ext == ".lua") {

                std::string nodeName = platform::Path::GetFilenameWithoutExtension(fullPath);

                TypeInfo info;
                info.typeName = nodeName;
                info.category = "Node";
                info.subcategory = "Custom";
                info.filePath = fullPath;
                info.isBuiltIn = false;

                m_NodeTypes.push_back(info);

            }
        }
    }
}

void EditorBridge::ScanComponentsDirectory(const std::string& directory) {

    auto result = platform::FileSystem::ListDirectory(directory);
    if (!result.success) {

        return;
    }

    for (const auto& fullPath : result.data) {
        if (platform::FileSystem::IsDirectory(fullPath)) {

            ScanComponentsDirectory(fullPath);
        } else {

            std::string ext = platform::Path::GetExtension(fullPath);
            if (ext == ".py" || ext == ".lua") {

                std::string componentName = platform::Path::GetFilenameWithoutExtension(fullPath);

                TypeInfo info;
                info.typeName = componentName;
                info.category = "Component";
                info.subcategory = "Custom";
                info.filePath = fullPath;
                info.isBuiltIn = false;

                m_ComponentTypes.push_back(info);

            }
        }
    }
}

void EditorBridge::ScanPrefabsDirectory(const std::string& directory) {

    auto result = platform::FileSystem::ListDirectory(directory);
    if (!result.success) {

        return;
    }

    for (const auto& fullPath : result.data) {
        if (platform::FileSystem::IsDirectory(fullPath)) {

            ScanPrefabsDirectory(fullPath);
        } else {

            std::string ext = platform::Path::GetExtension(fullPath);
            if (ext == ".prefab") {

                std::string prefabName = platform::Path::GetFilenameWithoutExtension(fullPath);

                TypeInfo info;
                info.typeName = prefabName;
                info.category = "Prefab";
                info.subcategory = "Custom";
                info.filePath = fullPath;
                info.isBuiltIn = false;

                m_PrefabTypes.push_back(info);

            }
        }
    }
}

std::vector<TypeInfo> EditorBridge::GetNodeTypes() const {
    return m_NodeTypes;
}

std::vector<TypeInfo> EditorBridge::GetComponentTypes() const {
    return m_ComponentTypes;
}

std::vector<TypeInfo> EditorBridge::GetPrefabTypes() const {
    return m_PrefabTypes;
}

std::shared_ptr<Prefab> EditorBridge::LoadPrefab(const std::string& prefabPath) {
    auto prefab = std::make_shared<Prefab>();
    if (!prefab->Load(prefabPath)) {

        return nullptr;
    }

    return prefab;
}

std::shared_ptr<Node> EditorBridge::GetNode(const core::UUID& nodeUUID) {
    if (!m_ActiveScene) {

        return nullptr;
    }

    return m_ActiveScene->FindNodeByUUID(nodeUUID);
}

std::shared_ptr<Node> EditorBridge::GetNodeByPath(const std::string& path) {
    if (!m_ActiveScene) {

        return nullptr;
    }

    return m_ActiveScene->FindNode(path);
}

std::shared_ptr<Node> EditorBridge::GetRootNode() {
    if (!m_ActiveScene) {

        return nullptr;
    }

    return m_ActiveScene->GetRoot();
}

std::shared_ptr<Node> EditorBridge::CreateNode(const std::string& typeName, const std::string& nodeName) {

    auto node = std::dynamic_pointer_cast<Node>(
        core::TypeRegistry::GetInstance().CreateInstance(typeName));

    if (!node) {

        return nullptr;
    }

    if (!nodeName.empty()) {
        node->SetName(nodeName);
    } else {
        node->SetName(typeName);
    }

    node->RegisterProperties();

    if (typeName == "Camera2D") {
        core::Camera2D* camera2D = dynamic_cast<core::Camera2D*>(node.get());
        if (camera2D) {
            float aspectRatio = static_cast<float>(m_ProjectWindowWidth) / static_cast<float>(m_ProjectWindowHeight);
            camera2D->SetAspectRatio(aspectRatio);
            camera2D->SetOrthoSize(static_cast<float>(m_ProjectWindowHeight));

        }
    } else if (typeName == "CameraUI") {
        core::CameraUI* cameraUI = dynamic_cast<core::CameraUI*>(node.get());
        if (cameraUI) {
            cameraUI->SetCanvasSize(math::Vec2(static_cast<float>(m_ProjectWindowWidth),
                                               static_cast<float>(m_ProjectWindowHeight)));

        }
    }

    return node;
}

bool EditorBridge::AddNode(std::shared_ptr<Node> node, std::shared_ptr<Node> parent) {
    if (!m_ActiveScene) {

        return false;
    }

    if (!node) {

        return false;
    }

    auto command = std::make_shared<core::AddNodeCommand>(m_ActiveScene, node, parent);
    return ExecuteCommand(command);
}

bool EditorBridge::RemoveNode(std::shared_ptr<Node> node) {
    if (!m_ActiveScene) {

        return false;
    }

    if (!node) {

        return false;
    }

    auto command = std::make_shared<core::RemoveNodeCommand>(m_ActiveScene, node);
    return ExecuteCommand(command);
}

bool EditorBridge::ReparentNode(std::shared_ptr<Node> node, std::shared_ptr<Node> newParent) {
    if (!node) {

        return false;
    }

    auto command = std::make_shared<core::ReparentNodeCommand>(node, newParent);
    return ExecuteCommand(command);
}

std::vector<std::shared_ptr<Node>> EditorBridge::GetChildren(std::shared_ptr<Node> node) {
    if (!node) {
        return std::vector<std::shared_ptr<Node>>();
    }

    return node->GetChildren();
}

std::shared_ptr<Component> EditorBridge::CreateComponent(const std::string& typeName) {

    auto component = std::dynamic_pointer_cast<Component>(
        core::TypeRegistry::GetInstance().CreateInstance(typeName));

    if (!component) {

        return nullptr;
    }

    component->RegisterProperties();

    return component;
}

bool EditorBridge::AddComponent(std::shared_ptr<Node> node, std::shared_ptr<Component> component) {
    if (!node) {

        return false;
    }

    if (!component) {

        return false;
    }

    auto command = std::make_shared<core::AddComponentCommand>(node, component);
    return ExecuteCommand(command);
}

bool EditorBridge::AddComponentDirect(std::shared_ptr<Node> node, std::shared_ptr<Component> component) {
    if (!node) {

        return false;
    }

    if (!component) {

        return false;
    }

    node->AddComponent(component);
    return true;
}

bool EditorBridge::RemoveComponent(std::shared_ptr<Node> node, std::shared_ptr<Component> component) {
    if (!node) {

        return false;
    }

    if (!component) {

        return false;
    }

    auto command = std::make_shared<core::RemoveComponentCommand>(node, component);
    return ExecuteCommand(command);
}

std::vector<std::shared_ptr<Component>> EditorBridge::GetComponents(std::shared_ptr<Node> node) {
    if (!node) {
        return std::vector<std::shared_ptr<Component>>();
    }

    return node->GetComponents();
}

nlohmann::json EditorBridge::GetNodeProperty(std::shared_ptr<Node> node, const std::string& propertyName) {
    if (!node) {

        return nlohmann::json();
    }

    node->RegisterProperties();

    auto allProperties = node->Serialize();
    if (allProperties.contains("properties") && allProperties["properties"].contains(propertyName)) {
        return allProperties["properties"][propertyName];
    }

    return nlohmann::json();
}

bool EditorBridge::SetNodeProperty(std::shared_ptr<Node> node, const std::string& propertyName, const nlohmann::json& value) {
    if (!node) {

        return false;
    }

    auto command = std::make_shared<core::SetNodePropertyCommand>(node, propertyName, value);
    return ExecuteCommand(command);
}

nlohmann::json EditorBridge::GetNodeProperties(std::shared_ptr<Node> node) {
    if (!node) {

        return nlohmann::json();
    }

    node->RegisterProperties();

    return node->Serialize();
}

nlohmann::json EditorBridge::GetComponentProperty(std::shared_ptr<Component> component, const std::string& propertyName) {
    if (!component) {

        return nlohmann::json();
    }

    auto value = component->GetPropertyValue<nlohmann::json>(propertyName);
    if (!value.is_null()) {
        return value;
    }

    auto allProperties = component->Serialize();
    if (allProperties.contains("properties") && allProperties["properties"].contains(propertyName)) {
        return allProperties["properties"][propertyName];
    }

    return nlohmann::json();
}

bool EditorBridge::SetComponentProperty(std::shared_ptr<Component> component, const std::string& propertyName, const nlohmann::json& value) {
    if (!component) {

        return false;
    }

    auto command = std::make_shared<core::SetComponentPropertyCommand>(component, propertyName, value);
    return ExecuteCommand(command);
}

bool EditorBridge::SetComponentPropertyDirect(std::shared_ptr<Component> component, const std::string& propertyName, const nlohmann::json& value) {
    if (!component) {

        return false;
    }

    auto& registry = component->GetPropertyRegistry();
    auto prop = registry.GetProperty(propertyName);
    if (prop) {
        prop->SetValueFromJson(value);
        component->OnPropertyChanged(propertyName, value);
        return true;
    }

    return false;
}

bool EditorBridge::SetNodePropertyDirect(std::shared_ptr<Node> node, const std::string& propertyName, const nlohmann::json& value) {
    if (!node) {

        return false;
    }

    node->RegisterProperties();

    nlohmann::json nodeJson;
    nodeJson["properties"][propertyName] = value;
    node->Deserialize(nodeJson);

    return true;
}

bool EditorBridge::DeserializeComponentDirect(std::shared_ptr<Component> component, const nlohmann::json& componentData) {
    if (!component) {

        return false;
    }

    component->Deserialize(componentData);

    return true;
}

nlohmann::json EditorBridge::GetComponentProperties(std::shared_ptr<Component> component) {
    if (!component) {

        return nlohmann::json();
    }

    component->RegisterProperties();

    try {
        auto result = component->SerializeWithMetadata();

        try {
            std::string jsonStr = result.dump();

        } catch (const std::exception& e) {

            return component->Serialize();
        }

        return result;
    } catch (const std::exception& e) {

        return component->Serialize();
    }
}

std::shared_ptr<Node> EditorBridge::InstantiatePrefab(std::shared_ptr<Prefab> prefab, std::shared_ptr<Node> parent) {
    if (!prefab) {

        return nullptr;
    }

    std::shared_ptr<Node> instance;

    if (parent) {
        instance = prefab->InstantiateAsChild(parent);
    } else {
        instance = prefab->Instantiate();
        if (instance && m_ActiveScene) {
            m_ActiveScene->AddNode(instance);
        }
    }

    if (instance) {
        MarkSceneDirty();

    }

    return instance;
}

void EditorBridge::UpdateCameraForViewMode(RenderViewID viewID, ViewMode mode) {
    if (!m_RenderWorld) return;

    lupine::RenderView* renderView = m_RenderWorld->getRenderView(viewID);
    if (!renderView) return;

    auto it = m_RenderViews.find(viewID);
    if (it == m_RenderViews.end()) return;

    uint32_t width = it->second.width;
    uint32_t height = it->second.height;

    if (mode == ViewMode::View2D) {

        float orthoHeight = static_cast<float>(m_ProjectWindowHeight);

        auto camera2D = std::make_unique<Camera2D>();
        camera2D->position = math::Vec2(0.0f, 0.0f);
        camera2D->rotation = 0.0f;
        camera2D->zoom = 1.0f;
        camera2D->orthoSize = orthoHeight;

        camera2D->clearColor = math::Color(0.2f, 0.2f, 0.25f, 1.0f);

        camera2D->isEditorCamera = true;

        renderView->setCamera(std::move(camera2D));
    } else {

        auto camera3D = std::make_unique<Camera3D>();
        camera3D->position = math::Vec3(0.0f, 5.0f, 10.0f);
        camera3D->target = math::Vec3(0.0f, 0.0f, 0.0f);
        camera3D->up = math::Vec3(0.0f, 1.0f, 0.0f);
        camera3D->fov = 60.0f;
        camera3D->nearPlane = 0.1f;
        camera3D->farPlane = 1000.0f;

        camera3D->clearColor = math::Color(0.2f, 0.2f, 0.25f, 1.0f);

        camera3D->isEditorCamera = true;

        renderView->setCamera(std::move(camera3D));
    }
}

void EditorBridge::RenderDebugOverlay(RenderViewID viewID, const RenderViewInfo& viewInfo) {

}

void EditorBridge::PanCamera2D(RenderViewID viewID, float deltaX, float deltaY) {
    if (!m_RenderWorld) return;

    lupine::RenderView* renderView = m_RenderWorld->getRenderView(viewID);
    if (!renderView) return;

    lupine::RenderCamera* camera = renderView->getCamera();
    Camera2D* camera2D = dynamic_cast<Camera2D*>(camera);
    if (!camera2D) return;

    float panSpeed = 1.0f / camera2D->zoom;
    camera2D->position.x -= deltaX * panSpeed;
    camera2D->position.y += deltaY * panSpeed;
}

void EditorBridge::ZoomCamera2D(RenderViewID viewID, float delta) {
    if (!m_RenderWorld) return;

    lupine::RenderView* renderView = m_RenderWorld->getRenderView(viewID);
    if (!renderView) return;

    lupine::RenderCamera* camera = renderView->getCamera();
    Camera2D* camera2D = dynamic_cast<Camera2D*>(camera);
    if (!camera2D) return;

    float zoomSpeed = 0.1f;
    camera2D->zoom += delta * zoomSpeed;

    camera2D->zoom = std::max(0.1f, std::min(camera2D->zoom, 10.0f));
}

void EditorBridge::PanCamera3D(RenderViewID viewID, float deltaX, float deltaY) {
    if (!m_RenderWorld) return;

    lupine::RenderView* renderView = m_RenderWorld->getRenderView(viewID);
    if (!renderView) return;

    lupine::RenderCamera* camera = renderView->getCamera();
    Camera3D* camera3D = dynamic_cast<Camera3D*>(camera);
    if (!camera3D) return;

    math::Vec3 forward = (camera3D->target - camera3D->position).Normalized();
    math::Vec3 right = forward.Cross(camera3D->up).Normalized();
    math::Vec3 up = right.Cross(forward).Normalized();

    float distance = (camera3D->target - camera3D->position).Length();
    float panSpeed = distance * 0.001f;

    math::Vec3 offset = right * (-deltaX * panSpeed) + up * (deltaY * panSpeed);
    math::Vec3 newPosition = camera3D->position + offset;
    math::Vec3 newTarget = camera3D->target + offset;

    auto it = m_RenderViews.find(viewID);
    if (it != m_RenderViews.end() && it->second.respectFloorEnabled) {
        if (it->second.respectFloorPositiveSide) {

            if (newPosition.y < 0.0f) {
                newPosition.y = 0.0f;
            }
            if (newTarget.y < 0.0f) {
                newTarget.y = 0.0f;
            }
        } else {

            if (newPosition.y > 0.0f) {
                newPosition.y = 0.0f;
            }
            if (newTarget.y > 0.0f) {
                newTarget.y = 0.0f;
            }
        }
    }

    camera3D->position = newPosition;
    camera3D->target = newTarget;
}

void EditorBridge::OrbitCamera3D(RenderViewID viewID, float deltaX, float deltaY) {
    if (!m_RenderWorld) return;

    lupine::RenderView* renderView = m_RenderWorld->getRenderView(viewID);
    if (!renderView) return;

    lupine::RenderCamera* camera = renderView->getCamera();
    Camera3D* camera3D = dynamic_cast<Camera3D*>(camera);
    if (!camera3D) return;

    math::Vec3 offset = camera3D->position - camera3D->target;
    float distance = offset.Length();

    float theta = std::atan2(offset.x, offset.z);
    float phi = std::acos(offset.y / distance);

    float orbitSpeed = 0.005f;
    theta -= deltaX * orbitSpeed;
    phi -= deltaY * orbitSpeed;

    const float epsilon = 0.01f;
    phi = std::max(epsilon, std::min(static_cast<float>(M_PI) - epsilon, phi));

    offset.x = distance * std::sin(phi) * std::sin(theta);
    offset.y = distance * std::cos(phi);
    offset.z = distance * std::sin(phi) * std::cos(theta);

    math::Vec3 newPosition = camera3D->target + offset;

    auto it = m_RenderViews.find(viewID);
    if (it != m_RenderViews.end() && it->second.respectFloorEnabled) {
        if (it->second.respectFloorPositiveSide) {

            if (newPosition.y < 0.0f) {
                newPosition.y = 0.0f;
            }
        } else {

            if (newPosition.y > 0.0f) {
                newPosition.y = 0.0f;
            }
        }
    }

    camera3D->position = newPosition;
}

void EditorBridge::ZoomCamera3D(RenderViewID viewID, float delta) {
    if (!m_RenderWorld) return;

    lupine::RenderView* renderView = m_RenderWorld->getRenderView(viewID);
    if (!renderView) return;

    lupine::RenderCamera* camera = renderView->getCamera();
    Camera3D* camera3D = dynamic_cast<Camera3D*>(camera);
    if (!camera3D) return;

    math::Vec3 direction = (camera3D->position - camera3D->target).Normalized();
    float distance = (camera3D->position - camera3D->target).Length();

    float zoomSpeed = distance * 0.1f;
    distance -= delta * zoomSpeed;

    distance = std::max(0.5f, std::min(distance, 1000.0f));

    math::Vec3 newPosition = camera3D->target + direction * distance;

    auto it = m_RenderViews.find(viewID);
    if (it != m_RenderViews.end() && it->second.respectFloorEnabled) {
        if (it->second.respectFloorPositiveSide) {

            if (newPosition.y < 0.0f) {
                newPosition.y = 0.0f;
            }
        } else {

            if (newPosition.y > 0.0f) {
                newPosition.y = 0.0f;
            }
        }
    }

    camera3D->position = newPosition;
}

void EditorBridge::SetRespectFloor(RenderViewID viewID, bool enabled) {
    auto it = m_RenderViews.find(viewID);
    if (it == m_RenderViews.end()) return;

    if (enabled && !it->second.respectFloorEnabled) {
        if (m_RenderWorld) {
            lupine::RenderView* renderView = m_RenderWorld->getRenderView(viewID);
            if (renderView) {
                lupine::RenderCamera* camera = renderView->getCamera();
                Camera3D* camera3D = dynamic_cast<Camera3D*>(camera);
                if (camera3D) {

                    it->second.respectFloorPositiveSide = (camera3D->position.y >= 0.0f);
                }
            }
        }
    }

    it->second.respectFloorEnabled = enabled;
}

bool EditorBridge::GetRespectFloor(RenderViewID viewID) const {
    auto it = m_RenderViews.find(viewID);
    if (it == m_RenderViews.end()) return false;
    return it->second.respectFloorEnabled;
}

void EditorBridge::UpdateCameraNodesRecursive(std::shared_ptr<Node> node, float aspectRatio, uint32_t width, uint32_t height) {
    if (!node) return;

    core::Camera2D* camera2D = dynamic_cast<core::Camera2D*>(node.get());
    if (camera2D) {
        camera2D->SetAspectRatio(aspectRatio);

    }

    core::CameraUI* cameraUI = dynamic_cast<core::CameraUI*>(node.get());
    if (cameraUI) {
        cameraUI->SetCanvasSize(math::Vec2(static_cast<float>(width), static_cast<float>(height)));

    }

    for (auto& child : node->GetChildren()) {
        UpdateCameraNodesRecursive(child, aspectRatio, width, height);
    }
}

uint32_t EditorBridge::GetMaterialSlotCount(std::shared_ptr<Component> component) {
    if (!component) {
        return 0;
    }

    StaticMesh3D* staticMesh = dynamic_cast<StaticMesh3D*>(component.get());
    if (!staticMesh) {
        return 0;
    }

    return staticMesh->GetMaterialSlotCount();
}

nlohmann::json EditorBridge::GetMaterialSlotProperties(std::shared_ptr<Component> component, uint32_t slotIndex) {
    nlohmann::json result;

    if (!component) {
        return result;
    }

    StaticMesh3D* staticMesh = dynamic_cast<StaticMesh3D*>(component.get());
    if (!staticMesh) {
        return result;
    }

    const components::MaterialSlot* slot = staticMesh->GetMaterialSlot(slotIndex);
    if (!slot) {
        return result;
    }

    result["name"] = slot->name;
    result["materialIndex"] = slot->materialIndex;
    result["enableOverride"] = slot->enableOverride;

    result["albedoColor"] = {
        {"r", slot->albedoColor.r},
        {"g", slot->albedoColor.g},
        {"b", slot->albedoColor.b},
        {"a", slot->albedoColor.a}
    };
    result["albedoTexturePath"] = slot->albedoTexturePath;

    result["metallic"] = slot->metallic;
    result["roughness"] = slot->roughness;
    result["metallicRoughnessTexturePath"] = slot->metallicRoughnessTexturePath;

    result["normalTexturePath"] = slot->normalTexturePath;
    result["normalScale"] = slot->normalScale;

    result["emissiveColor"] = {
        {"r", slot->emissiveColor.r},
        {"g", slot->emissiveColor.g},
        {"b", slot->emissiveColor.b},
        {"a", slot->emissiveColor.a}
    };
    result["emissiveTexturePath"] = slot->emissiveTexturePath;
    result["emissiveStrength"] = slot->emissiveStrength;

    result["alphaCutoff"] = slot->alphaCutoff;

    return result;
}

bool EditorBridge::SetMaterialSlotProperty(std::shared_ptr<Component> component, uint32_t slotIndex, const std::string& propertyName, const nlohmann::json& value) {
    if (!component) {
        return false;
    }

    StaticMesh3D* staticMesh = dynamic_cast<StaticMesh3D*>(component.get());
    if (!staticMesh) {
        return false;
    }

    components::MaterialSlot* slot = staticMesh->GetMaterialSlot(slotIndex);
    if (!slot) {
        return false;
    }

    if (propertyName == "enableOverride") {
        slot->enableOverride = value.get<bool>();
    }
    else if (propertyName == "albedoColor") {
        slot->albedoColor = Color(
            value["r"].get<float>(),
            value["g"].get<float>(),
            value["b"].get<float>(),
            value["a"].get<float>()
        );
    }
    else if (propertyName == "albedoTexturePath") {
        slot->albedoTexturePath = value.get<std::string>();
        slot->texturesNeedUpload = true;
    }
    else if (propertyName == "metallic") {
        slot->metallic = value.get<float>();
    }
    else if (propertyName == "roughness") {
        slot->roughness = value.get<float>();
    }
    else if (propertyName == "metallicRoughnessTexturePath") {
        slot->metallicRoughnessTexturePath = value.get<std::string>();
        slot->texturesNeedUpload = true;
    }
    else if (propertyName == "normalTexturePath") {
        slot->normalTexturePath = value.get<std::string>();
        slot->texturesNeedUpload = true;
    }
    else if (propertyName == "normalScale") {
        slot->normalScale = value.get<float>();
    }
    else if (propertyName == "emissiveColor") {
        slot->emissiveColor = Color(
            value["r"].get<float>(),
            value["g"].get<float>(),
            value["b"].get<float>(),
            value["a"].get<float>()
        );
    }
    else if (propertyName == "emissiveTexturePath") {
        slot->emissiveTexturePath = value.get<std::string>();
        slot->texturesNeedUpload = true;
    }
    else if (propertyName == "emissiveStrength") {
        slot->emissiveStrength = value.get<float>();
    }
    else if (propertyName == "alphaCutoff") {
        slot->alphaCutoff = value.get<float>();
    }
    else {
        return false;
    }

    MarkSceneDirty();
    return true;
}

std::shared_ptr<lupine::core::Node> EditorBridge::PickNodeAtScreenPosition(RenderViewID viewID, float screenX, float screenY) {

    auto it = m_RenderViews.find(viewID);
    if (it == m_RenderViews.end()) {

        return nullptr;
    }

    const RenderViewInfo& viewInfo = it->second;
    if (!viewInfo.scene) {
        return nullptr;
    }

    lupine::RenderView* renderView = m_RenderWorld->getRenderView(viewID);
    if (!renderView) {
        return nullptr;
    }

    lupine::RenderCamera* camera = renderView->getCamera();
    if (!camera) {
        return nullptr;
    }

    if (viewInfo.viewMode == ViewMode::View2D) {

        Camera2D* cam2D = dynamic_cast<Camera2D*>(camera);
        if (!cam2D) {
            return nullptr;
        }

        float ndcX = (2.0f * screenX / viewInfo.width) - 1.0f;
        float ndcY = 1.0f - (2.0f * screenY / viewInfo.height);

        float aspectRatio = static_cast<float>(viewInfo.width) / static_cast<float>(viewInfo.height);
        math::Mat4 viewMatrix = cam2D->getViewMatrix();
        math::Mat4 projMatrix = cam2D->getProjectionMatrix(aspectRatio);

        math::Mat4 viewProjMatrix = projMatrix * viewMatrix;
        math::Mat4 invViewProj = viewProjMatrix.Inverse();

        math::Vec4 worldPos4 = invViewProj * math::Vec4(ndcX, ndcY, 0.0f, 1.0f);
        math::Vec2 worldPos(worldPos4.x / worldPos4.w, worldPos4.y / worldPos4.w);

        std::shared_ptr<lupine::core::Node> pickedNode = nullptr;
        float closestZ = -std::numeric_limits<float>::infinity();

        std::function<void(std::shared_ptr<lupine::core::Node>)> traverseNodes;
        traverseNodes = [&](std::shared_ptr<lupine::core::Node> node) {
            if (!node) return;

            if (!node->IsVisible() || !node->IsActive()) {
                return;
            }

            auto& children = node->GetChildren();
            for (auto it = children.rbegin(); it != children.rend(); ++it) {
                traverseNodes(*it);
            }

            if (TestNode2DPick(node, worldPos)) {

                float zIndex = 0.0f;
                Node2D* node2D = dynamic_cast<Node2D*>(node.get());
                if (node2D) {
                    zIndex = node2D->GetZIndex();
                }

                if (zIndex > closestZ) {
                    closestZ = zIndex;
                    pickedNode = node;
                }
            }
        };

        traverseNodes(viewInfo.scene->GetRoot());
        return pickedNode;
    }
    else {
        Camera3D* cam3D = dynamic_cast<Camera3D*>(camera);
        if (!cam3D) {
            return nullptr;
        }

        float ndcX = (2.0f * screenX / viewInfo.width) - 1.0f;
        float ndcY = 1.0f - (2.0f * screenY / viewInfo.height);

        float aspectRatio = static_cast<float>(viewInfo.width) / static_cast<float>(viewInfo.height);
        math::Mat4 viewMatrix = cam3D->getViewMatrix();
        math::Mat4 projMatrix = cam3D->getProjectionMatrix(aspectRatio);

        math::Mat4 viewProjMatrix = projMatrix * viewMatrix;
        math::Mat4 invViewProj = viewProjMatrix.Inverse();

        math::Vec4 nearPoint4 = invViewProj * math::Vec4(ndcX, ndcY, -1.0f, 1.0f);
        math::Vec4 farPoint4 = invViewProj * math::Vec4(ndcX, ndcY, 1.0f, 1.0f);

        math::Vec3 nearPoint(nearPoint4.x / nearPoint4.w, nearPoint4.y / nearPoint4.w, nearPoint4.z / nearPoint4.w);
        math::Vec3 farPoint(farPoint4.x / farPoint4.w, farPoint4.y / farPoint4.w, farPoint4.z / farPoint4.w);

        math::Ray ray(nearPoint, (farPoint - nearPoint).Normalized());

        std::shared_ptr<lupine::core::Node> pickedNode = nullptr;
        float closestDistance = std::numeric_limits<float>::infinity();

        std::function<void(std::shared_ptr<lupine::core::Node>)> traverseNodes;
        traverseNodes = [&](std::shared_ptr<lupine::core::Node> node) {
            if (!node) return;

            if (!node->IsVisible() || !node->IsActive()) {
                return;
            }

            Node3D* node3D = dynamic_cast<Node3D*>(node.get());
            if (node3D) {
                float distance = 0.0f;
                if (TestNode3DPick(node, ray, distance)) {
                    if (distance < closestDistance) {
                        closestDistance = distance;
                        pickedNode = node;
                    }
                }
            }

            for (auto& child : node->GetChildren()) {
                traverseNodes(child);
            }
        };

        traverseNodes(viewInfo.scene->GetRoot());
        return pickedNode;
    }
}

math::Vec2 EditorBridge::GetNodeBounds2D(std::shared_ptr<lupine::core::Node> node) {
    math::Vec2 bounds(0.0f, 0.0f);

    if (!node) return bounds;

    if (auto camera2D = std::dynamic_pointer_cast<core::Camera2D>(node)) {

        float orthoSize = camera2D->GetOrthoSize();
        float aspectRatio = camera2D->GetAspectRatio();
        float width = orthoSize * aspectRatio;
        float height = orthoSize;

        bounds = math::Vec2(std::max(width, 50.0f), std::max(height, 50.0f));
        return bounds;
    }

    if (auto cameraUI = std::dynamic_pointer_cast<core::CameraUI>(node)) {

        math::Vec2 canvasSize = cameraUI->GetCanvasSize();

        bounds = math::Vec2(std::max(canvasSize.x * 0.1f, 100.0f), std::max(canvasSize.y * 0.1f, 100.0f));
        return bounds;
    }

    for (auto& component : node->GetComponents()) {

        if (auto sprite = std::dynamic_pointer_cast<Sprite2D>(component)) {

            bounds = math::Vec2(sprite->GetSize().x, sprite->GetSize().y);
            break;
        }

        else if (auto colorRect = std::dynamic_pointer_cast<ColorRect>(component)) {
            math::Vec2 size = colorRect->GetSize();

            bounds = math::Vec2(std::max(size.x, 10.0f), std::max(size.y, 10.0f));
            break;
        }

        else if (auto panel = std::dynamic_pointer_cast<Panel>(component)) {
            float width = panel->GetWidth();
            float height = panel->GetHeight();

            bounds = math::Vec2(std::max(width, 10.0f), std::max(height, 10.0f));
            break;
        }

        else if (auto button = std::dynamic_pointer_cast<Button>(component)) {
            bounds = math::Vec2(button->GetWidth(), button->GetHeight());
            break;
        }

        else if (auto label = std::dynamic_pointer_cast<Label>(component)) {
            float fontSize = label->GetFontSize();
            float textLength = static_cast<float>(label->GetText().length());

            if (textLength > 0 && fontSize > 0) {

                float charWidth = fontSize * 0.6f;
                bounds = math::Vec2(textLength * charWidth, fontSize * 1.2f);
            } else if (fontSize > 0) {

                bounds = math::Vec2(fontSize * 2.0f, fontSize * 1.2f);
            } else {

                bounds = math::Vec2(50.0f, 24.0f);
            }
            break;
        }

        else if (auto image = std::dynamic_pointer_cast<Image2D>(component)) {
            math::Vec2 offsetMin = image->GetOffsetMin();
            math::Vec2 offsetMax = image->GetOffsetMax();
            bounds = math::Vec2(offsetMax.x - offsetMin.x, offsetMax.y - offsetMin.y);

            if (bounds.x < 0) bounds.x = -bounds.x;
            if (bounds.y < 0) bounds.y = -bounds.y;

            if (bounds.x < 1.0f) bounds.x = 100.0f;
            if (bounds.y < 1.0f) bounds.y = 100.0f;
            break;
        }
    }

    return bounds;
}

math::Vec3 EditorBridge::GetNodeBounds3D(std::shared_ptr<lupine::core::Node> node) {
    math::Vec3 bounds(0.0f, 0.0f, 0.0f);

    if (!node) return bounds;

    if (auto camera3D = std::dynamic_pointer_cast<core::Camera3D>(node)) {

        bounds = math::Vec3(1.0f, 1.0f, 1.5f);
        return bounds;
    }

    for (auto& component : node->GetComponents()) {

        if (auto sprite = std::dynamic_pointer_cast<Sprite3D>(component)) {
            bounds = math::Vec3(sprite->GetSize().x, sprite->GetSize().y, 0.1f);
            break;
        }

        else if (auto primMesh = std::dynamic_pointer_cast<PrimitiveMesh3D>(component)) {

            bounds = math::Vec3(1.0f, 1.0f, 1.0f);
            break;
        }

        else if (auto staticMesh = std::dynamic_pointer_cast<StaticMesh3D>(component)) {
            bounds = math::Vec3(1.0f, 1.0f, 1.0f);
            break;
        }

        else if (auto dirLight = std::dynamic_pointer_cast<DirectionalLight3D>(component)) {
            bounds = math::Vec3(0.5f, 0.5f, 0.5f);
            break;
        }
        else if (auto omniLight = std::dynamic_pointer_cast<OmniLight3D>(component)) {
            float range = omniLight->GetRange();
            bounds = math::Vec3(range, range, range);
            break;
        }
        else if (auto spotLight = std::dynamic_pointer_cast<SpotLight3D>(component)) {
            float range = spotLight->GetRange();
            bounds = math::Vec3(range, range, range);
            break;
        }
    }

    return bounds;
}

bool EditorBridge::TestNode2DPick(std::shared_ptr<Node> node, const math::Vec2& worldPos) {
    if (!node) return false;

    math::AABB bounds;

    Node2D* node2D = dynamic_cast<Node2D*>(node.get());
    if (node2D) {

        for (auto& component : node->GetComponents()) {
            auto renderable = std::dynamic_pointer_cast<IRenderableComponent>(component);
            if (renderable) {
                bounds = renderable->getWorldBounds();
                break;
            }
        }
    } else if (auto cameraUI = dynamic_cast<core::CameraUI*>(node.get())) {
        bounds = cameraUI->getWorldBounds();
    } else if (auto camera2D = dynamic_cast<core::Camera2D*>(node.get())) {
        bounds = camera2D->getWorldBounds();
    } else {

        return false;
    }

    math::Vec3 min = bounds.min;
    math::Vec3 max = bounds.max;
    if (max.x <= min.x || max.y <= min.y) {
        return false;
    }

    if (dynamic_cast<core::Camera2D*>(node.get()) || dynamic_cast<core::CameraUI*>(node.get())) {
        float edgeMargin = 2.0f;
        bool onLeft   = std::abs(worldPos.x - min.x) <= edgeMargin && worldPos.y >= min.y && worldPos.y <= max.y;
        bool onRight  = std::abs(worldPos.x - max.x) <= edgeMargin && worldPos.y >= min.y && worldPos.y <= max.y;
        bool onTop    = std::abs(worldPos.y - min.y) <= edgeMargin && worldPos.x >= min.x && worldPos.x <= max.x;
        bool onBottom = std::abs(worldPos.y - max.y) <= edgeMargin && worldPos.x >= min.x && worldPos.x <= max.x;
        return onLeft || onRight || onTop || onBottom;
    }

    if (node2D) {
        float rotation = node2D->GetGlobalRotation();

        if (std::abs(rotation) > 0.0001f) {
            math::Vec2 globalPos = node2D->GetGlobalPosition();
            math::Vec2 scale = node2D->GetGlobalScale();

            math::Vec2 unscaledSize(0.0f, 0.0f);

            math::OBB obb;
            bool hasOBB = false;

            for (auto& component : node->GetComponents()) {
                auto renderable = std::dynamic_pointer_cast<IRenderableComponent>(component);
                if (!renderable) continue;

                obb = renderable->getOrientedBounds();
                if (obb.extents.x > 0.0f && obb.extents.y > 0.0f) {

                    math::Vec3 worldPos3D(worldPos.x, worldPos.y, 0.0f);
                    hasOBB = obb.Contains(worldPos3D);
                    break;
                }

                if (auto sprite2D = std::dynamic_pointer_cast<components::Sprite2D>(component)) {
                    unscaledSize = sprite2D->CalculateRenderSize();
                    break;
                }
                else if (auto shape2D = std::dynamic_pointer_cast<components::Shape2D>(component)) {
                    unscaledSize = shape2D->GetSize();
                    break;
                }
            }

            if (hasOBB) {
                return obb.Contains(math::Vec3(worldPos.x, worldPos.y, 0.0f));
            }

            if (unscaledSize.x == 0.0f && unscaledSize.y == 0.0f) {
                return false;
            }

            math::Vec2 scaledSize(unscaledSize.x * std::abs(scale.x), unscaledSize.y * std::abs(scale.y));
            math::Vec2 halfSize = scaledSize * 0.5f;

            math::Vec2 localPos = worldPos - globalPos;

            float cosR = std::cos(-rotation);
            float sinR = std::sin(-rotation);
            math::Vec2 rotatedLocal(
                localPos.x * cosR - localPos.y * sinR,
                localPos.x * sinR + localPos.y * cosR
            );

            return (std::abs(rotatedLocal.x) <= halfSize.x &&
                    std::abs(rotatedLocal.y) <= halfSize.y);
        }
    }

    return (worldPos.x >= min.x && worldPos.x <= max.x &&
            worldPos.y >= min.y && worldPos.y <= max.y);
}

bool EditorBridge::TestNode3DPick(std::shared_ptr<Node> node, const math::Ray& ray, float& distance) {
    Node3D* node3D = dynamic_cast<Node3D*>(node.get());
    if (!node3D) return false;

    const auto& components = node->GetComponents();
    bool foundRenderableComponent = false;
    float closestHit = std::numeric_limits<float>::infinity();

    for (const auto& component : components) {
        auto renderable = std::dynamic_pointer_cast<IRenderableComponent>(component);
        if (renderable) {
            foundRenderableComponent = true;

            float hitDistance = 0.0f;
            if (renderable->IntersectRay(ray, hitDistance)) {
                if (hitDistance < closestHit) {
                    closestHit = hitDistance;
                }
            }
        }
    }

    if (foundRenderableComponent && closestHit < std::numeric_limits<float>::infinity()) {
        distance = closestHit;
        return true;
    }

    math::Vec3 bounds = GetNodeBounds3D(node);

    if (bounds.x <= 0.0f || bounds.y <= 0.0f || bounds.z <= 0.0f) {
        return false;
    }

    math::Vec3 nodePos = node3D->GetGlobalPosition();
    math::Vec3 nodeScale = node3D->GetGlobalScale();

    bounds.x *= std::abs(nodeScale.x);
    bounds.y *= std::abs(nodeScale.y);
    bounds.z *= std::abs(nodeScale.z);

    math::Vec3 halfExtents = bounds * 0.5f;
    math::AABB aabb(nodePos - halfExtents, nodePos + halfExtents);

    return ray.IntersectAABB(aabb, distance);
}

void EditorBridge::SetSelectedNode(RenderViewID viewID, std::shared_ptr<lupine::core::Node> node) {

    m_SelectedNodes[viewID] = node;

    if (m_RenderWorld) {
        lupine::RenderView* renderView = m_RenderWorld->getRenderView(viewID);
        if (renderView) {
            renderView->setSelectedNode(node);

        }
    }
}

std::shared_ptr<lupine::core::Node> EditorBridge::GetSelectedNode(RenderViewID viewID) const {
    auto it = m_SelectedNodes.find(viewID);
    if (it != m_SelectedNodes.end()) {
        return it->second;
    }
    return nullptr;
}

void EditorBridge::SetSelectedNodes(RenderViewID viewID, const std::vector<std::shared_ptr<lupine::core::Node>>& nodes) {

    if (m_RenderWorld) {
        lupine::RenderView* renderView = m_RenderWorld->getRenderView(viewID);
        if (renderView) {
            renderView->setSelectedNodes(nodes);

            if (!nodes.empty()) {
                m_SelectedNodes[viewID] = nodes[0];
                renderView->setSelectedNode(nodes[0]);

            } else {
                m_SelectedNodes[viewID] = nullptr;
                renderView->setSelectedNode(nullptr);

            }
        }
    }
}

std::vector<std::shared_ptr<lupine::core::Node>> EditorBridge::GetSelectedNodes(RenderViewID viewID) const {
    if (m_RenderWorld) {
        lupine::RenderView* renderView = m_RenderWorld->getRenderView(viewID);
        if (renderView) {
            return renderView->getSelectedNodes();
        }
    }
    return {};
}

void EditorBridge::SetGizmoEnabled(RenderViewID viewID, bool enabled) {
    if (m_RenderWorld) {
        lupine::RenderView* renderView = m_RenderWorld->getRenderView(viewID);
        if (renderView) {
            renderView->setGizmoEnabled(enabled);
        }
    }
}

bool EditorBridge::IsGizmoEnabled(RenderViewID viewID) const {
    if (m_RenderWorld) {
        lupine::RenderView* renderView = m_RenderWorld->getRenderView(viewID);
        if (renderView) {
            return renderView->isGizmoEnabled();
        }
    }
    return true;
}

void EditorBridge::SetGizmoType(RenderViewID viewID, GizmoType type) {
    if (m_RenderWorld) {
        lupine::RenderView* renderView = m_RenderWorld->getRenderView(viewID);
        if (renderView) {
            renderView->setGizmoType(type);
        }
    }
}

GizmoType EditorBridge::GetGizmoType(RenderViewID viewID) const {
    if (m_RenderWorld) {
        lupine::RenderView* renderView = m_RenderWorld->getRenderView(viewID);
        if (renderView) {
            return renderView->getGizmoType();
        }
    }
    return GizmoType::All;
}

void EditorBridge::SetTransformSpace(RenderViewID viewID, TransformSpace space) {
    if (m_RenderWorld) {
        lupine::RenderView* renderView = m_RenderWorld->getRenderView(viewID);
        if (renderView) {
            renderView->setTransformSpace(space);
        }
    }
}

TransformSpace EditorBridge::GetTransformSpace(RenderViewID viewID) const {
    if (m_RenderWorld) {
        lupine::RenderView* renderView = m_RenderWorld->getRenderView(viewID);
        if (renderView) {
            return renderView->getTransformSpace();
        }
    }
    return TransformSpace::World;
}

bool EditorBridge::ExecuteCommand(std::shared_ptr<core::Command> command) {
    if (!m_CommandHistory) {

        return false;
    }

    bool result = m_CommandHistory->ExecuteCommand(command);
    if (result) {
        MarkSceneDirty();
    }
    return result;
}

void EditorBridge::RecordCommand(std::shared_ptr<core::Command> command) {
    if (!m_CommandHistory) {

        return;
    }

    m_CommandHistory->RecordCommand(command);
    MarkSceneDirty();
}

std::shared_ptr<core::CompositeCommand> EditorBridge::CreateCompositeCommand(const std::string& description) {
    return std::make_shared<core::CompositeCommand>(description);
}

std::shared_ptr<core::Command> EditorBridge::CreateAddNodeCommand(std::shared_ptr<Node> node, std::shared_ptr<Node> parent) {
    if (!m_ActiveScene) {

        return nullptr;
    }
    if (!node) {

        return nullptr;
    }
    return std::make_shared<core::AddNodeCommand>(m_ActiveScene, node, parent);
}

std::shared_ptr<core::Command> EditorBridge::CreateAddComponentCommand(std::shared_ptr<Node> node, std::shared_ptr<Component> component) {
    if (!node) {

        return nullptr;
    }
    if (!component) {

        return nullptr;
    }
    return std::make_shared<core::AddComponentCommand>(node, component);
}

std::shared_ptr<core::Command> EditorBridge::CreateSetNodePropertyCommand(std::shared_ptr<Node> node, const std::string& propertyName, const nlohmann::json& value) {
    if (!node) {

        return nullptr;
    }
    return std::make_shared<core::SetNodePropertyCommand>(node, propertyName, value);
}

std::shared_ptr<core::Command> EditorBridge::CreateSetComponentPropertyCommand(std::shared_ptr<Component> component, const std::string& propertyName, const nlohmann::json& value) {
    if (!component) {

        return nullptr;
    }
    return std::make_shared<core::SetComponentPropertyCommand>(component, propertyName, value);
}

bool EditorBridge::Undo() {
    if (!m_CommandHistory) {

        return false;
    }

    bool result = m_CommandHistory->Undo();
    if (result) {
        MarkSceneDirty();
    }
    return result;
}

bool EditorBridge::Redo() {
    if (!m_CommandHistory) {

        return false;
    }

    bool result = m_CommandHistory->Redo();
    if (result) {
        MarkSceneDirty();
    }
    return result;
}

bool EditorBridge::CanUndo() const {
    return m_CommandHistory && m_CommandHistory->CanUndo();
}

bool EditorBridge::CanRedo() const {
    return m_CommandHistory && m_CommandHistory->CanRedo();
}

std::string EditorBridge::GetUndoDescription() const {
    if (!m_CommandHistory) {
        return "";
    }
    return m_CommandHistory->GetUndoDescription();
}

std::string EditorBridge::GetRedoDescription() const {
    if (!m_CommandHistory) {
        return "";
    }
    return m_CommandHistory->GetRedoDescription();
}

void EditorBridge::ClearHistory() {
    if (m_CommandHistory) {
        m_CommandHistory->Clear();
    }
}

void EditorBridge::SetMaxUndoSteps(size_t maxSteps) {
    if (m_CommandHistory) {
        m_CommandHistory->SetMaxHistorySize(maxSteps);
    }
}

size_t EditorBridge::GetMaxUndoSteps() const {
    if (!m_CommandHistory) {
        return 0;
    }
    return m_CommandHistory->GetMaxHistorySize();
}

std::string EditorBridge::PlayAudioFile(const std::string& filePath, const std::string& busName, bool loop, float volume) {

    auto it = m_PreviewAudioAssets.begin();
    while (it != m_PreviewAudioAssets.end()) {
        core::UUID oldUUID;
        oldUUID.FromString(it->first);

        const audio::AudioSource* source = audio::AudioManager::GetInstance().GetSource(oldUUID);
        bool isPlaying = source && source->state == audio::PlaybackState::Playing;
        if (!isPlaying) {
            it = m_PreviewAudioAssets.erase(it);
        } else {
            ++it;
        }
    }

    auto audioAsset = std::make_shared<asset::AudioAsset>();

    audio::PlaybackMode mode = loop ? audio::PlaybackMode::Loop : audio::PlaybackMode::OneShot;

    if (!audioAsset->LoadFromFile(filePath, asset::AudioLoadMode::Preload)) {

        return "";
    }

    asset::AssetRef<asset::AudioAsset> assetRef(audioAsset.get());
    core::UUID sourceUUID = audio::AudioManager::GetInstance().Play(assetRef, busName, mode, volume);

    if (!sourceUUID.IsValid()) {

        return "";
    }

    std::string uuidStr = sourceUUID.ToString();
    m_PreviewAudioAssets[uuidStr] = audioAsset;

    return uuidStr;
}

void EditorBridge::StopAudio(const std::string& sourceUUID) {
    core::UUID uuid;
    uuid.FromString(sourceUUID);

    if (uuid.IsValid()) {
        audio::AudioManager::GetInstance().Stop(uuid);

    }
}

void EditorBridge::StopAllAudio() {
    audio::AudioManager::GetInstance().StopAll();

    m_PreviewAudioAssets.clear();

}

void EditorBridge::PlaySceneAudio(std::shared_ptr<Scene> scene) {
    if (!scene) {
        return;
    }

    scene->SetInEditor(false);

    std::function<void(std::shared_ptr<Node>)> playNodeAudio = [&](std::shared_ptr<Node> node) {
        if (!node) return;

        auto audioPlayer = node->GetComponent<components::AudioPlayer>();
        if (audioPlayer) {
            audioPlayer->Play();
        }

        for (auto& child : node->GetChildren()) {
            playNodeAudio(child);
        }
    };

    auto root = scene->GetRoot();
    if (root) {
        playNodeAudio(root);
    }
}

void EditorBridge::StopSceneAudio(std::shared_ptr<Scene> scene) {
    if (!scene) {
        return;
    }

    std::function<void(std::shared_ptr<Node>)> stopNodeAudio = [&](std::shared_ptr<Node> node) {
        if (!node) return;

        auto audioPlayer = node->GetComponent<components::AudioPlayer>();
        if (audioPlayer) {
            audioPlayer->Stop();
        }

        for (auto& child : node->GetChildren()) {
            stopNodeAudio(child);
        }
    };

    auto root = scene->GetRoot();
    if (root) {
        stopNodeAudio(root);
    }

    scene->SetInEditor(true);
}

void EditorBridge::SetMasterVolume(float volume) {
    audio::AudioManager::GetInstance().SetMasterVolume(volume);
}

float EditorBridge::GetMasterVolume() const {
    return audio::AudioManager::GetInstance().GetMasterVolume();
}

void EditorBridge::SetBusVolume(const std::string& busName, float volume) {
    audio::AudioManager::GetInstance().SetBusVolume(busName, volume);
}

float EditorBridge::GetBusVolume(const std::string& busName) const {
    return audio::AudioManager::GetInstance().GetBusVolume(busName);
}

math::Vec2 EditorBridge::ScreenToWorld2D(RenderViewID viewID, float screenX, float screenY) {
    if (!m_RenderWorld) {
        return math::Vec2(0.0f, 0.0f);
    }

    lupine::RenderView* renderView = m_RenderWorld->getRenderView(viewID);
    if (!renderView) {
        return math::Vec2(0.0f, 0.0f);
    }

    auto camera = renderView->getCamera();
    if (!camera || camera->getType() != CameraType::Camera2D) {
        return math::Vec2(0.0f, 0.0f);
    }

    const Viewport& viewport = renderView->getViewport();
    float viewportWidth = viewport.width;
    float viewportHeight = viewport.height;

    float ndcX = ((screenX - viewport.x) / viewportWidth) * 2.0f - 1.0f;
    float ndcY = 1.0f - ((screenY - viewport.y) / viewportHeight) * 2.0f;

    float aspectRatio = viewportWidth / viewportHeight;
    math::Mat4 viewMatrix = camera->getViewMatrix();
    math::Mat4 projMatrix = camera->getProjectionMatrix(aspectRatio);

    math::Mat4 invViewProj = (projMatrix * viewMatrix).Inverse();

    math::Vec4 worldPos4 = invViewProj * math::Vec4(ndcX, ndcY, 0.0f, 1.0f);

    if (std::abs(worldPos4.w) > 0.0001f) {
        worldPos4.x /= worldPos4.w;
        worldPos4.y /= worldPos4.w;
    }

    return math::Vec2(worldPos4.x, worldPos4.y);
}

math::Vec3 EditorBridge::ScreenToWorld3D(RenderViewID viewID, float screenX, float screenY, float planeY) {
    if (!m_RenderWorld) {
        return math::Vec3(0.0f, planeY, 0.0f);
    }

    lupine::RenderView* renderView = m_RenderWorld->getRenderView(viewID);
    if (!renderView) {
        return math::Vec3(0.0f, planeY, 0.0f);
    }

    auto camera = renderView->getCamera();
    if (!camera || camera->getType() != CameraType::Camera3D) {
        return math::Vec3(0.0f, planeY, 0.0f);
    }

    const Viewport& viewport = renderView->getViewport();
    float viewportWidth = static_cast<float>(viewport.width);
    float viewportHeight = static_cast<float>(viewport.height);

    if (viewportWidth <= 0.0f || viewportHeight <= 0.0f) {
        return math::Vec3(0.0f, planeY, 0.0f);
    }

    float ndcX = ((screenX - viewport.x) / viewportWidth) * 2.0f - 1.0f;
    float ndcY = 1.0f - ((screenY - viewport.y) / viewportHeight) * 2.0f;

    float aspectRatio = viewportWidth / viewportHeight;
    math::Mat4 viewMatrix = camera->getViewMatrix();
    math::Mat4 projMatrix = camera->getProjectionMatrix(aspectRatio);

    math::Mat4 invViewProj = (projMatrix * viewMatrix).Inverse();

    math::Vec4 nearPoint = invViewProj * math::Vec4(ndcX, ndcY, -1.0f, 1.0f);

    math::Vec4 farPoint = invViewProj * math::Vec4(ndcX, ndcY, 1.0f, 1.0f);

    if (std::abs(nearPoint.w) > 0.0001f) {
        nearPoint.x /= nearPoint.w;
        nearPoint.y /= nearPoint.w;
        nearPoint.z /= nearPoint.w;
    }
    if (std::abs(farPoint.w) > 0.0001f) {
        farPoint.x /= farPoint.w;
        farPoint.y /= farPoint.w;
        farPoint.z /= farPoint.w;
    }

    math::Vec3 rayOrigin(nearPoint.x, nearPoint.y, nearPoint.z);
    math::Vec3 rayDir = math::Vec3(farPoint.x - nearPoint.x, farPoint.y - nearPoint.y, farPoint.z - nearPoint.z).Normalized();

    if (std::abs(rayDir.y) < 0.0001f) {

        return math::Vec3(0.0f, planeY, 0.0f);
    }

    float t = (planeY - rayOrigin.y) / rayDir.y;

    if (t < 0.0f) {
        return math::Vec3(0.0f, planeY, 0.0f);
    }

    math::Vec3 intersectionPoint = rayOrigin + rayDir * t;

    return intersectionPoint;
}

void EditorBridge::AddCollisionVertex(std::shared_ptr<core::Component> component, float x, float y) {
    if (!component) {
        return;
    }

    auto collisionBody = std::dynamic_pointer_cast<components::CollisionBody2DComponent>(component);
    if (!collisionBody) {

        return;
    }

    collisionBody->AddVertex(math::Vec2(x, y));

    MarkSceneDirty();
}

void EditorBridge::RemoveCollisionVertex(std::shared_ptr<core::Component> component, int index) {
    if (!component) {
        return;
    }

    auto collisionBody = std::dynamic_pointer_cast<components::CollisionBody2DComponent>(component);
    if (!collisionBody) {

        return;
    }

    collisionBody->RemoveVertex(index);

    MarkSceneDirty();
}

void EditorBridge::UpdateCollisionVertex(std::shared_ptr<core::Component> component, int index, float x, float y) {
    if (!component) {
        return;
    }

    auto collisionBody = std::dynamic_pointer_cast<components::CollisionBody2DComponent>(component);
    if (!collisionBody) {

        return;
    }

    collisionBody->UpdateVertex(index, math::Vec2(x, y));

    MarkSceneDirty();
}

std::string EditorBridge::GetCollisionVertices(std::shared_ptr<core::Component> component) {
    if (!component) {
        return "[]";
    }

    auto collisionBody = std::dynamic_pointer_cast<components::CollisionBody2DComponent>(component);
    if (!collisionBody) {

        return "[]";
    }

    const auto& vertices = collisionBody->GetVertices();

    nlohmann::json jsonArray = nlohmann::json::array();
    for (const auto& vertex : vertices) {
        nlohmann::json vertexObj;
        vertexObj["x"] = vertex.x;
        vertexObj["y"] = vertex.y;
        jsonArray.push_back(vertexObj);
    }

    return jsonArray.dump();
}

void EditorBridge::ClearCollisionVertices(std::shared_ptr<core::Component> component) {
    if (!component) {
        return;
    }

    auto collisionBody = std::dynamic_pointer_cast<components::CollisionBody2DComponent>(component);
    if (!collisionBody) {

        return;
    }

    collisionBody->ClearVertices();

    MarkSceneDirty();
}

std::string EditorBridge::GetNodeGlobalPosition(std::shared_ptr<Node> node) {
    if (!node) {
        return "{}";
    }

    nlohmann::json posJson;

    if (auto node2D = std::dynamic_pointer_cast<Node2D>(node)) {
        math::Vec2 globalPos = node2D->GetGlobalPosition();
        posJson["x"] = globalPos.x;
        posJson["y"] = globalPos.y;
    }

    else if (auto node3D = std::dynamic_pointer_cast<Node3D>(node)) {
        math::Vec3 globalPos = node3D->GetGlobalPosition();
        posJson["x"] = globalPos.x;
        posJson["y"] = globalPos.y;
        posJson["z"] = globalPos.z;
    }
    else {

        posJson["x"] = 0.0f;
        posJson["y"] = 0.0f;
    }

    return posJson.dump();
}

float EditorBridge::GetNodeGlobalRotation(std::shared_ptr<Node> node) {
    if (!node) {
        return 0.0f;
    }

    if (auto node2D = std::dynamic_pointer_cast<Node2D>(node)) {
        return node2D->GetGlobalRotation();
    }

    else if (auto node3D = std::dynamic_pointer_cast<Node3D>(node)) {

        return 0.0f;
    }

    return 0.0f;
}

void EditorBridge::RenderVoxelBuilder(RenderViewID viewID) {

    auto vbIt = m_VoxelBuilders.find(viewID);
    if (vbIt == m_VoxelBuilders.end() || vbIt->second.empty()) {
        return;
    }

    lupine::RenderView* renderView = m_RenderWorld->getRenderView(viewID);
    if (!renderView) {

        return;
    }

    for (VoxelBuilder* builder : vbIt->second) {
        if (!builder) {
            continue;
        }

        size_t voxelCount = builder->GetVoxelCount();

        if (voxelCount > 0) {
            static size_t lastLoggedCount = 0;
            if (voxelCount != lastLoggedCount) {

                lastLoggedCount = voxelCount;
            }
        }

        MeshHandle mesh = builder->GetMesh();

        if (!mesh.isValid()) {
            if (voxelCount > 0) {

            }
            continue;
        }

        renderView->addCustomDrawMesh(mesh, MaterialHandle(), math::Mat4::Identity());

        static bool logged = false;
        if (!logged) {

            logged = true;
        }
    }
}

uint64_t EditorBridge::CreateVoxelBuilder() {
    auto* builder = new VoxelBuilder();
    if (m_RenderWorld) {
        builder->InitializeRendering(m_RenderWorld.get());
    }
    return reinterpret_cast<uint64_t>(builder);
}

void EditorBridge::DestroyVoxelBuilder(uint64_t builderID) {
    auto* builder = reinterpret_cast<VoxelBuilder*>(builderID);
    if (builder) {
        delete builder;
    }
}

void EditorBridge::VoxelBuilderPlaceVoxel(uint64_t builderID, int32_t x, int32_t y, int32_t z, float r, float g, float b, float a) {
    auto* builder = reinterpret_cast<VoxelBuilder*>(builderID);
    if (builder) {
        builder->PlaceVoxel(x, y, z, math::Color(r, g, b, a));
    }
}

void EditorBridge::VoxelBuilderEraseVoxel(uint64_t builderID, int32_t x, int32_t y, int32_t z) {
    auto* builder = reinterpret_cast<VoxelBuilder*>(builderID);
    if (builder) {
        builder->EraseVoxel(x, y, z);
    }
}

void EditorBridge::VoxelBuilderClear(uint64_t builderID) {
    auto* builder = reinterpret_cast<VoxelBuilder*>(builderID);
    if (builder) {
        builder->Clear();
    }
}

bool EditorBridge::VoxelBuilderHasVoxel(uint64_t builderID, int32_t x, int32_t y, int32_t z) {
    auto* builder = reinterpret_cast<VoxelBuilder*>(builderID);
    if (builder) {
        return builder->HasVoxel(x, y, z);
    }
    return false;
}

size_t EditorBridge::VoxelBuilderGetVoxelCount(uint64_t builderID) {
    auto* builder = reinterpret_cast<VoxelBuilder*>(builderID);
    if (builder) {
        return builder->GetVoxelCount();
    }
    return 0;
}

void EditorBridge::VoxelBuilderRender(uint64_t builderID, RenderViewID viewID) {
    auto* builder = reinterpret_cast<VoxelBuilder*>(builderID);
    if (builder) {

        auto& builders = m_VoxelBuilders[viewID];
        auto it = std::find(builders.begin(), builders.end(), builder);
        if (it == builders.end()) {
            builders.push_back(builder);
        }
    } else {

        m_VoxelBuilders.erase(viewID);
    }
}

std::string EditorBridge::VoxelBuilderToJSON(uint64_t builderID) {
    auto* builder = reinterpret_cast<VoxelBuilder*>(builderID);
    if (builder) {
        return builder->ToJSON();
    }
    return "{}";
}

bool EditorBridge::VoxelBuilderFromJSON(uint64_t builderID, const std::string& json) {
    auto* builder = reinterpret_cast<VoxelBuilder*>(builderID);
    if (builder) {
        return builder->FromJSON(json);
    }
    return false;
}

std::string EditorBridge::VoxelBuilderExportOBJ(uint64_t builderID, bool mergeFaces, bool textureAtlas) {
    auto* builder = reinterpret_cast<VoxelBuilder*>(builderID);
    if (builder) {
        return builder->ExportToOBJ(mergeFaces, textureAtlas);
    }
    return "";
}

std::string EditorBridge::VoxelBuilderExportGLTF(uint64_t builderID, bool mergeFaces, bool textureAtlas) {
    auto* builder = reinterpret_cast<VoxelBuilder*>(builderID);
    if (builder) {
        return builder->ExportToGLTF(mergeFaces, textureAtlas);
    }
    return "{}";
}

void EditorBridge::DebugDrawLine(RenderViewID viewID, float x1, float y1, float z1, float x2, float y2, float z2, float r, float g, float b, float a) {
    auto it = m_RenderViews.find(viewID);
    if (it == m_RenderViews.end() || !it->second.isValid) {
        return;
    }

    if (m_RenderWorld) {
        DebugRenderer* debugRenderer = m_RenderWorld->getDebugRenderer();
        if (debugRenderer) {
            math::Vec3 start(x1, y1, z1);
            math::Vec3 end(x2, y2, z2);
            math::Color color(r, g, b, a);
            debugRenderer->drawLine(start, end, color);
        }
    }
}

void EditorBridge::DebugDrawAABB(RenderViewID viewID, float minX, float minY, float minZ, float maxX, float maxY, float maxZ, float r, float g, float b, float a) {
    auto it = m_RenderViews.find(viewID);
    if (it == m_RenderViews.end() || !it->second.isValid) {
        return;
    }

    if (m_RenderWorld) {
        DebugRenderer* debugRenderer = m_RenderWorld->getDebugRenderer();
        if (debugRenderer) {
            math::AABB aabb(math::Vec3(minX, minY, minZ), math::Vec3(maxX, maxY, maxZ));
            math::Color color(r, g, b, a);
            debugRenderer->draw3DBoundingBox(aabb, color);
        }
    }
}

}
}

