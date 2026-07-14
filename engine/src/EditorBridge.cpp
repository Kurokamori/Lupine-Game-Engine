#include "lupine/engine/EditorBridge.hpp"
#include "lupine/engine/Engine.hpp"
#include "lupine/core/Project.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/CameraNodes.hpp"
#include "lupine/core/UILayerNode.hpp"
#include "lupine/core/Component.hpp"
#include "lupine/core/ScriptComponent.hpp"
#include "lupine/core/ScriptedComponentWrapper.hpp"
#include "lupine/core/CustomComponentRegistry.hpp"
#include "lupine/core/ArchetypeRegistry.hpp"
#include "lupine/core/ArchetypeRuntime.hpp"
#include "lupine/scripting/ScriptAPI.hpp"
#include "lupine/core/InterfaceRegistry.hpp"
#include "lupine/core/ExtensionManager.hpp"
#include "lupine/localization/LocalizationManager.hpp"
#include "lupine/ui/ThemeManager.hpp"
#include "lupine/asset/ArchetypeInstance.hpp"
#include "lupine/core/Prefab.hpp"
#include "lupine/core/SceneInstance.hpp"
#include "lupine/core/PrefabInstance.hpp"
#include "lupine/core/EditorCommands.hpp"
#include "lupine/platform/Platform.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/rendering/Rendering.hpp"
#include "lupine/rendering/TextureCache.hpp"
#include "lupine/rendering/TextureUpload.hpp"
#include "lupine/rendering/ViewportUtils.hpp"
#include "lupine/core/Serialization.hpp"
#include <cctype>

#include "lupine/components/ComponentRegistry.hpp"
#include "lupine/components/Components.hpp"
#include "lupine/animation/AnimationPreview.hpp"

#include "lupine/audio/AudioManager.hpp"
#include "lupine/asset/AudioAsset.hpp"
#include "lupine/asset/ImageAsset.hpp"
#include "lupine/asset/AssetDatabase.hpp"
#include "lupine/math/Ray.hpp"
#include "lupine/math/AABB.hpp"
#include <nlohmann/json.hpp>
#include <cmath>
#include <limits>
#include <unordered_map>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace lupine {
namespace engine {

using core::Node;
using core::Node2D;
using core::Node3D;

using components::Sprite2D;
using components::Particles2D;
using components::Sprite3D;
using components::Particles3D;
using components::Light2D;
using components::LightOccluder2D;
using components::AnimatedSprite2D;
using components::AnimatedSprite3D;
using components::GifPlayer;
using components::VideoPlayer;
using components::PrimitiveMesh3D;
using components::StaticMesh3D;
using components::SkeletalMesh3D;
using components::Label;
using components::Label3D;
using components::DirectionalLight3D;
using components::OmniLight3D;
using components::SpotLight3D;
using components::Timer;
using components::Tween;
using components::TweenSequence;
using components::AnimationPlayer;
using components::AnimationTree;
using components::SubViewport;
using components::CameraEffectColorGrade;
using components::CameraEffectTonemap;
using components::CameraEffectVignette;
using components::CameraEffectFilmGrain;
using components::CameraEffectColorInvert;
using components::CameraEffectPosterize;
using components::CameraEffectHueShift;
using components::CameraEffectBlur;
using components::CameraEffectGlow;
using components::CameraEffectOutline;
using components::CameraEffectPixelate;
using components::CameraEffectSharpen;
using components::CameraEffectChromaticAberration;
using components::ColorRect;
using components::Image2D;
using components::Panel;
using components::Panel3D;
using components::Container;
using components::PaddingContainer;
using components::CenterContainer;
using components::HorizontalContainer;
using components::VerticalContainer;
using components::GridContainer;
using components::DockContainer;
using components::Stack;
using components::Wrap;
using components::SplitContainer;
using components::AspectRatioContainer;
using components::Spacer;
using components::LayoutSlot;
using components::ScrollContainer;
using components::TabContainer;
using components::Slider;
using components::LineEdit;
using components::SpinBox;
using components::TextEdit;
using components::ItemList;
using components::Dropdown;
using components::PopupMenu;
using components::RichTextLabel;
using components::Tree;
using components::ProgressBar;
using components::ProgressBar3D;
using components::Button;
using components::Button3D;
using components::Shape2D;
using components::Line2D;
using components::Curve2D;
using components::Path2D;
using components::Curve3D;
using components::Path3D;
using components::PathFollow3D;
using components::NavigationRegion2D;
using components::NavigationAgent2D;
using components::NavigationObstacle2D;
using components::NavigationRegion3D;
using components::NavigationAgent3D;
using components::NavigationObstacle3D;
using components::NetworkObject;
using components::NetworkSynchronizer;
using components::NetworkTransform2D;
using components::NetworkTransform3D;
using components::NetworkSpawner;
using components::NetworkController;
using components::NetworkAnimator;
using components::NetworkRigidBody2D;
using components::NetworkRigidBody3D;
using components::Empty2D;
using components::Empty3D;
using components::VectorGraphic2D;
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
using components::RayCast2D;
using components::RayCast3D;
using components::ShapeCast2D;
using components::ShapeCast3D;
using components::Panel3D;
using components::ProgressBar;
using components::YSort;
using components::ParallaxBackground;
using components::ParallaxLayer;
using components::MultiMeshGeneric;
using components::ScatterMultiMesh;
using components::CollisionScatterMultiMesh;
using components::TextureButton;
using components::ToggleButton;
using components::TextureToggleButton;
using components::RadioButton;
using components::RadioList;
using components::Checkbox;
using components::CheckList;
using components::NineSlicePanel;
using components::TileMap2D;

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
    swapchainDesc.colorFormat = TextureFormat::RGBA8_UNORM;
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

    if (!m_Initialized || !m_RenderWorld) {
        LOG_ERROR(LogCategory::Render, "[EditorBridge] RenderView: early exit - m_Initialized={}, m_RenderWorld={}",
                  m_Initialized, (void*)m_RenderWorld.get());
        return;
    }

    auto it = m_RenderViews.find(viewID);
    if (it == m_RenderViews.end()) {
        LOG_ERROR(LogCategory::Render, "[EditorBridge] RenderView: view {} not found", viewID);
        return;
    }

    if (it->second.width < 1 || it->second.height < 1) {
        LOG_ERROR(LogCategory::Render, "[EditorBridge] RenderView: invalid dimensions {}x{}",
                  it->second.width, it->second.height);
        return;
    }

    if (!it->second.swapchain.isValid()) {
        LOG_ERROR(LogCategory::Render, "[EditorBridge] RenderView: swapchain is invalid");
        return;
    }

    if (it->second.scene) {
        float deltaTime = 1.0f / 60.0f;
        it->second.scene->Update(deltaTime);
    }

    try {
        
        m_RenderWorld->beginFrame();

        RenderVoxelBuilder(viewID);
        RenderTileMap25D(viewID);

        // Camera-effects preview: link the active scene camera that carries effects so its
        // effect chain is applied to this editor view. Off by default; cleared each frame.
        if (lupine::RenderView* rv = m_RenderWorld->getRenderView(viewID)) {
            lupine::core::Node* previewCam = nullptr;
            if (m_CameraEffectsPreviewViews.count(viewID) != 0) {
                previewCam = FindPreviewCameraNode(it->second.scene.get());
            }
            rv->setSourceCameraNode(previewCam);
        }

        m_RenderWorld->renderView(viewID);

        m_RenderWorld->endFrame(false);

        m_RenderWorld->presentView(viewID);

    } catch (const std::exception& e) {
        LOG_ERROR(LogCategory::Render, "[EditorBridge] RenderView: exception caught: {}", e.what());
    } catch (...) {
        LOG_ERROR(LogCategory::Render, "[EditorBridge] RenderView: unknown exception caught");
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

void EditorBridge::SetViewCameraEffectsPreview(RenderViewID viewID, bool enabled) {
    if (enabled) {
        m_CameraEffectsPreviewViews.insert(viewID);
    } else {
        m_CameraEffectsPreviewViews.erase(viewID);
        // Drop any linked source camera so effects stop applying immediately.
        if (m_RenderWorld) {
            if (lupine::RenderView* renderView = m_RenderWorld->getRenderView(viewID)) {
                renderView->setSourceCameraNode(nullptr);
            }
        }
    }
}

bool EditorBridge::GetViewCameraEffectsPreview(RenderViewID viewID) const {
    return m_CameraEffectsPreviewViews.count(viewID) != 0;
}

namespace {
bool NodeHasEnabledCameraEffect(lupine::core::Node* node) {
    if (!node) {
        return false;
    }
    for (const auto& comp : node->GetComponents()) {
        if (comp && comp->IsEnabled() &&
            dynamic_cast<lupine::components::CameraEffect*>(comp.get()) != nullptr) {
            return true;
        }
    }
    return false;
}

bool NodeIsActiveCamera(lupine::core::Node* node) {
    if (auto* c3 = dynamic_cast<lupine::core::Camera3D*>(node)) return c3->IsActive();
    if (auto* c2 = dynamic_cast<lupine::core::Camera2D*>(node)) return c2->IsActive();
    if (auto* cu = dynamic_cast<lupine::core::CameraUI*>(node)) return cu->IsActive();
    return false;
}

lupine::core::Node* FindPreviewCameraRecursive(lupine::core::Node* node) {
    if (!node) {
        return nullptr;
    }
    if (node->IsActiveInHierarchy() && NodeIsActiveCamera(node) && NodeHasEnabledCameraEffect(node)) {
        return node;
    }
    for (const auto& child : node->GetChildren()) {
        if (lupine::core::Node* found = FindPreviewCameraRecursive(child.get())) {
            return found;
        }
    }
    return nullptr;
}
} // namespace

lupine::core::Node* EditorBridge::FindPreviewCameraNode(Scene* scene) const {
    if (!scene) {
        return nullptr;
    }
    return FindPreviewCameraRecursive(scene->GetRoot().get());
}

void EditorBridge::SetProjectSettings(uint32_t windowWidth, uint32_t windowHeight) {
    m_ProjectWindowWidth = windowWidth;
    m_ProjectWindowHeight = windowHeight;

    // The design resolution IS the logical canvas that UIControl anchor layout
    // resolves against (GetLogicalCanvasSize). The runtime sets this from the same
    // project window size; the editor must mirror it or UI is laid out against the
    // stale 1280x720 default while the editor camera renders at the design size,
    // making anchored/full-rect controls size and position wrong in the editor.
    SetLogicalCanvasSize(math::Vec2(
        static_cast<float>(windowWidth),
        static_cast<float>(windowHeight)
    ));

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
    m_NodeTypes.push_back(TypeInfo("UILayer", "Node", "UI", true));
    m_NodeTypes.push_back(TypeInfo("SceneInstance", "Node", "Scene", true));
    m_NodeTypes.push_back(TypeInfo("SceneInstance2D", "Node", "Scene", true));
    m_NodeTypes.push_back(TypeInfo("SceneInstance3D", "Node", "Scene", true));
    m_NodeTypes.push_back(TypeInfo("PrefabInstance", "Node", "Scene", true));
    m_NodeTypes.push_back(TypeInfo("PrefabInstance2D", "Node", "Scene", true));
    m_NodeTypes.push_back(TypeInfo("PrefabInstance3D", "Node", "Scene", true));

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

    core::TypeRegistry::GetInstance().RegisterType("UILayer",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<core::UILayer>();
        });

    core::TypeRegistry::GetInstance().RegisterType("SceneInstance",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<core::SceneInstance>();
        });

    core::TypeRegistry::GetInstance().RegisterType("SceneInstance2D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<core::SceneInstance2D>();
        });

    core::TypeRegistry::GetInstance().RegisterType("SceneInstance3D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<core::SceneInstance3D>();
        });

    core::TypeRegistry::GetInstance().RegisterType("PrefabInstance",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<core::PrefabInstance>();
        });

    core::TypeRegistry::GetInstance().RegisterType("PrefabInstance2D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<core::PrefabInstance2D>();
        });

    core::TypeRegistry::GetInstance().RegisterType("PrefabInstance3D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<core::PrefabInstance3D>();
        });

}

void EditorBridge::RegisterBuiltInComponentTypes() {
    m_ComponentTypes.clear();

    m_ComponentTypes.push_back(TypeInfo("Component", "Component", "Base", true));
    m_ComponentTypes.push_back(TypeInfo("PythonScriptComponent", "Component", "Scripting", true));
    m_ComponentTypes.push_back(TypeInfo("LuaScriptComponent", "Component", "Scripting", true));
    m_ComponentTypes.push_back(TypeInfo("MRubyScriptComponent", "Component", "Scripting", true));
    m_ComponentTypes.push_back(TypeInfo("Sprite2D", "Component", "2D", true));
    m_ComponentTypes.push_back(TypeInfo("Particles2D", "Component", "2D", true));
    m_ComponentTypes.push_back(TypeInfo("Sprite3D", "Component", "3D", true));
    m_ComponentTypes.push_back(TypeInfo("Particles3D", "Component", "3D", true));
    m_ComponentTypes.push_back(TypeInfo("AnimatedSprite2D", "Component", "2D", true));
    m_ComponentTypes.push_back(TypeInfo("AnimatedSprite3D", "Component", "3D", true));
    m_ComponentTypes.push_back(TypeInfo("GifPlayer", "Component", "2D", true));
    m_ComponentTypes.push_back(TypeInfo("VideoPlayer", "Component", "2D", true));
    m_ComponentTypes.push_back(TypeInfo("PrimitiveMesh3D", "Component", "3D", true));
    m_ComponentTypes.push_back(TypeInfo("StaticMesh3D", "Component", "3D", true));
    m_ComponentTypes.push_back(TypeInfo("SkeletalMesh3D", "Component", "3D", true));
    m_ComponentTypes.push_back(TypeInfo("Label", "Component", "UI", true));
    m_ComponentTypes.push_back(TypeInfo("Label3D", "Component", "3D", true));
    m_ComponentTypes.push_back(TypeInfo("ProgressBar", "Component", "UI", true));
    m_ComponentTypes.push_back(TypeInfo("ProgressBar3D", "Component", "3D", true));
    m_ComponentTypes.push_back(TypeInfo("Slider", "Component", "UI", true));
    m_ComponentTypes.push_back(TypeInfo("LineEdit", "Component", "UI", true));
    m_ComponentTypes.push_back(TypeInfo("SpinBox", "Component", "UI", true));
    m_ComponentTypes.push_back(TypeInfo("TextEdit", "Component", "UI", true));
    m_ComponentTypes.push_back(TypeInfo("ItemList", "Component", "UI", true));
    m_ComponentTypes.push_back(TypeInfo("Dropdown", "Component", "UI", true));
    m_ComponentTypes.push_back(TypeInfo("PopupMenu", "Component", "UI", true));
    m_ComponentTypes.push_back(TypeInfo("RichTextLabel", "Component", "UI", true));
    m_ComponentTypes.push_back(TypeInfo("Tree", "Component", "UI", true));
    m_ComponentTypes.push_back(TypeInfo("ColorRect", "Component", "UI", true));
    m_ComponentTypes.push_back(TypeInfo("Image2D", "Component", "UI", true));
    m_ComponentTypes.push_back(TypeInfo("Shape2D", "Component", "2D", true));
    m_ComponentTypes.push_back(TypeInfo("Line2D", "Component", "2D", true));
    m_ComponentTypes.push_back(TypeInfo("Curve2D", "Component", "2D", true));
    m_ComponentTypes.push_back(TypeInfo("Path2D", "Component", "2D", true));
    m_ComponentTypes.push_back(TypeInfo("Curve3D", "Component", "3D", true));
    m_ComponentTypes.push_back(TypeInfo("Path3D", "Component", "3D", true));
    m_ComponentTypes.push_back(TypeInfo("PathFollow3D", "Component", "3D", true));
    m_ComponentTypes.push_back(TypeInfo("VectorGraphic2D", "Component", "2D", true));
    m_ComponentTypes.push_back(TypeInfo("Empty2D", "Component", "2D", true));
    m_ComponentTypes.push_back(TypeInfo("Empty3D", "Component", "3D", true));
    m_ComponentTypes.push_back(TypeInfo("NavigationRegion2D", "Component", "Navigation", true));
    m_ComponentTypes.push_back(TypeInfo("NavigationAgent2D", "Component", "Navigation", true));
    m_ComponentTypes.push_back(TypeInfo("NavigationObstacle2D", "Component", "Navigation", true));
    m_ComponentTypes.push_back(TypeInfo("NavigationRegion3D", "Component", "Navigation", true));
    m_ComponentTypes.push_back(TypeInfo("NavigationAgent3D", "Component", "Navigation", true));
    m_ComponentTypes.push_back(TypeInfo("NavigationObstacle3D", "Component", "Navigation", true));
    m_ComponentTypes.push_back(TypeInfo("NetworkObject", "Component", "Network", true));
    m_ComponentTypes.push_back(TypeInfo("NetworkSynchronizer", "Component", "Network", true));
    m_ComponentTypes.push_back(TypeInfo("NetworkTransform2D", "Component", "Network", true));
    m_ComponentTypes.push_back(TypeInfo("NetworkTransform3D", "Component", "Network", true));
    m_ComponentTypes.push_back(TypeInfo("NetworkSpawner", "Component", "Network", true));
    m_ComponentTypes.push_back(TypeInfo("NetworkController", "Component", "Network", true));
    m_ComponentTypes.push_back(TypeInfo("NetworkAnimator", "Component", "Network", true));
    m_ComponentTypes.push_back(TypeInfo("NetworkRigidBody2D", "Component", "Network", true));
    m_ComponentTypes.push_back(TypeInfo("NetworkRigidBody3D", "Component", "Network", true));
    m_ComponentTypes.push_back(TypeInfo("Light2D", "Component", "2D/Lighting", true));
    m_ComponentTypes.push_back(TypeInfo("LightOccluder2D", "Component", "2D/Lighting", true));
    m_ComponentTypes.push_back(TypeInfo("DirectionalLight3D", "Component", "3D/Lighting", true));
    m_ComponentTypes.push_back(TypeInfo("OmniLight3D", "Component", "3D/Lighting", true));
    m_ComponentTypes.push_back(TypeInfo("SpotLight3D", "Component", "3D/Lighting", true));
    m_ComponentTypes.push_back(TypeInfo("Timer", "Component", "Utility", true));
    m_ComponentTypes.push_back(TypeInfo("Tween", "Component", "Utility", true));
    m_ComponentTypes.push_back(TypeInfo("TweenSequence", "Component", "Utility", true));
    m_ComponentTypes.push_back(TypeInfo("AnimationPlayer", "Component", "Animation", true));
    m_ComponentTypes.push_back(TypeInfo("AnimationTree", "Component", "Animation", true));
    m_ComponentTypes.push_back(TypeInfo("SubViewport", "Component", "Rendering", true));
    m_ComponentTypes.push_back(TypeInfo("CameraEffectColorGrade", "Component", "Camera Effects", true));
    m_ComponentTypes.push_back(TypeInfo("CameraEffectTonemap", "Component", "Camera Effects", true));
    m_ComponentTypes.push_back(TypeInfo("CameraEffectVignette", "Component", "Camera Effects", true));
    m_ComponentTypes.push_back(TypeInfo("CameraEffectFilmGrain", "Component", "Camera Effects", true));
    m_ComponentTypes.push_back(TypeInfo("CameraEffectColorInvert", "Component", "Camera Effects", true));
    m_ComponentTypes.push_back(TypeInfo("CameraEffectPosterize", "Component", "Camera Effects", true));
    m_ComponentTypes.push_back(TypeInfo("CameraEffectHueShift", "Component", "Camera Effects", true));
    m_ComponentTypes.push_back(TypeInfo("CameraEffectBlur", "Component", "Camera Effects", true));
    m_ComponentTypes.push_back(TypeInfo("CameraEffectGlow", "Component", "Camera Effects", true));
    m_ComponentTypes.push_back(TypeInfo("CameraEffectOutline", "Component", "Camera Effects", true));
    m_ComponentTypes.push_back(TypeInfo("CameraEffectPixelate", "Component", "Camera Effects", true));
    m_ComponentTypes.push_back(TypeInfo("CameraEffectSharpen", "Component", "Camera Effects", true));
    m_ComponentTypes.push_back(TypeInfo("CameraEffectChromaticAberration", "Component", "Camera Effects", true));
    m_ComponentTypes.push_back(TypeInfo("Panel", "Component", "UI", true));
    m_ComponentTypes.push_back(TypeInfo("NineSlicePanel", "Component", "UI", true));
    m_ComponentTypes.push_back(TypeInfo("Container", "Component", "UI/Containers", true));
    m_ComponentTypes.push_back(TypeInfo("PaddingContainer", "Component", "UI/Containers", true));
    m_ComponentTypes.push_back(TypeInfo("CenterContainer", "Component", "UI/Containers", true));
    m_ComponentTypes.push_back(TypeInfo("HorizontalContainer", "Component", "UI/Containers", true));
    m_ComponentTypes.push_back(TypeInfo("VerticalContainer", "Component", "UI/Containers", true));
    m_ComponentTypes.push_back(TypeInfo("GridContainer", "Component", "UI/Containers", true));
    m_ComponentTypes.push_back(TypeInfo("DockContainer", "Component", "UI/Containers", true));
    m_ComponentTypes.push_back(TypeInfo("Stack", "Component", "UI/Containers", true));
    m_ComponentTypes.push_back(TypeInfo("Wrap", "Component", "UI/Containers", true));
    m_ComponentTypes.push_back(TypeInfo("SplitContainer", "Component", "UI/Containers", true));
    m_ComponentTypes.push_back(TypeInfo("AspectRatioContainer", "Component", "UI/Containers", true));
    m_ComponentTypes.push_back(TypeInfo("Spacer", "Component", "UI/Layout", true));
    m_ComponentTypes.push_back(TypeInfo("LayoutSlot", "Component", "UI/Layout", true));
    m_ComponentTypes.push_back(TypeInfo("ScrollContainer", "Component", "UI/Containers", true));
    m_ComponentTypes.push_back(TypeInfo("TabContainer", "Component", "UI/Containers", true));
    m_ComponentTypes.push_back(TypeInfo("Button", "Component", "UI", true));
    m_ComponentTypes.push_back(TypeInfo("TextureButton", "Component", "UI", true));
    m_ComponentTypes.push_back(TypeInfo("ToggleButton", "Component", "UI", true));
    m_ComponentTypes.push_back(TypeInfo("TextureToggleButton", "Component", "UI", true));
    m_ComponentTypes.push_back(TypeInfo("RadioButton", "Component", "UI", true));
    m_ComponentTypes.push_back(TypeInfo("RadioList", "Component", "UI", true));
    m_ComponentTypes.push_back(TypeInfo("Checkbox", "Component", "UI", true));
    m_ComponentTypes.push_back(TypeInfo("CheckList", "Component", "UI", true));
    m_ComponentTypes.push_back(TypeInfo("Panel3D", "Component", "3D", true));
    m_ComponentTypes.push_back(TypeInfo("Button3D", "Component", "3D", true));
    m_ComponentTypes.push_back(TypeInfo("YSort", "Component", "2D", true));
    m_ComponentTypes.push_back(TypeInfo("ParallaxBackground", "Component", "2D", true));
    m_ComponentTypes.push_back(TypeInfo("ParallaxLayer", "Component", "2D", true));
    m_ComponentTypes.push_back(TypeInfo("MultiMeshGeneric", "Component", "3D", true));
    m_ComponentTypes.push_back(TypeInfo("ScatterMultiMesh", "Component", "3D", true));
    m_ComponentTypes.push_back(TypeInfo("CollisionScatterMultiMesh", "Component", "3D", true));
    m_ComponentTypes.push_back(TypeInfo("NodeScatter", "Component", "3D", true));
    m_ComponentTypes.push_back(TypeInfo("TileMap2D", "Component", "2D", true));

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

    core::TypeRegistry::GetInstance().RegisterType("Particles2D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<Particles2D>();
        });

    core::TypeRegistry::GetInstance().RegisterType("Sprite3D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<Sprite3D>();
        });

    core::TypeRegistry::GetInstance().RegisterType("Particles3D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<Particles3D>();
        });

    core::TypeRegistry::GetInstance().RegisterType("Light2D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<Light2D>();
        });

    core::TypeRegistry::GetInstance().RegisterType("LightOccluder2D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<LightOccluder2D>();
        });

    core::TypeRegistry::GetInstance().RegisterType("ProgressBar",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<ProgressBar>();
        });

    core::TypeRegistry::GetInstance().RegisterType("Slider",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<Slider>();
        });

    core::TypeRegistry::GetInstance().RegisterType("LineEdit",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<LineEdit>();
        });

    core::TypeRegistry::GetInstance().RegisterType("SpinBox",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<SpinBox>();
        });

    core::TypeRegistry::GetInstance().RegisterType("TextEdit",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<TextEdit>();
        });

    core::TypeRegistry::GetInstance().RegisterType("ItemList",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<ItemList>();
        });

    core::TypeRegistry::GetInstance().RegisterType("Dropdown",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<Dropdown>();
        });

    core::TypeRegistry::GetInstance().RegisterType("PopupMenu",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<PopupMenu>();
        });

    core::TypeRegistry::GetInstance().RegisterType("RichTextLabel",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<RichTextLabel>();
        });

    core::TypeRegistry::GetInstance().RegisterType("Tree",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<Tree>();
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

    core::TypeRegistry::GetInstance().RegisterType("GifPlayer",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<GifPlayer>();
        });

    core::TypeRegistry::GetInstance().RegisterType("VideoPlayer",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<VideoPlayer>();
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
    core::TypeRegistry::GetInstance().RegisterType("Tween",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<Tween>();
        });
    core::TypeRegistry::GetInstance().RegisterType("TweenSequence",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<TweenSequence>();
        });
    core::TypeRegistry::GetInstance().RegisterType("AnimationPlayer",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<AnimationPlayer>();
        });
    core::TypeRegistry::GetInstance().RegisterType("AnimationTree",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<AnimationTree>();
        });
    core::TypeRegistry::GetInstance().RegisterType("SubViewport",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<SubViewport>();
        });
    core::TypeRegistry::GetInstance().RegisterType("CameraEffectColorGrade",
        []() -> std::shared_ptr<core::ISerializable> { return std::make_shared<CameraEffectColorGrade>(); });
    core::TypeRegistry::GetInstance().RegisterType("CameraEffectTonemap",
        []() -> std::shared_ptr<core::ISerializable> { return std::make_shared<CameraEffectTonemap>(); });
    core::TypeRegistry::GetInstance().RegisterType("CameraEffectVignette",
        []() -> std::shared_ptr<core::ISerializable> { return std::make_shared<CameraEffectVignette>(); });
    core::TypeRegistry::GetInstance().RegisterType("CameraEffectFilmGrain",
        []() -> std::shared_ptr<core::ISerializable> { return std::make_shared<CameraEffectFilmGrain>(); });
    core::TypeRegistry::GetInstance().RegisterType("CameraEffectColorInvert",
        []() -> std::shared_ptr<core::ISerializable> { return std::make_shared<CameraEffectColorInvert>(); });
    core::TypeRegistry::GetInstance().RegisterType("CameraEffectPosterize",
        []() -> std::shared_ptr<core::ISerializable> { return std::make_shared<CameraEffectPosterize>(); });
    core::TypeRegistry::GetInstance().RegisterType("CameraEffectHueShift",
        []() -> std::shared_ptr<core::ISerializable> { return std::make_shared<CameraEffectHueShift>(); });
    core::TypeRegistry::GetInstance().RegisterType("CameraEffectBlur",
        []() -> std::shared_ptr<core::ISerializable> { return std::make_shared<CameraEffectBlur>(); });
    core::TypeRegistry::GetInstance().RegisterType("CameraEffectGlow",
        []() -> std::shared_ptr<core::ISerializable> { return std::make_shared<CameraEffectGlow>(); });
    core::TypeRegistry::GetInstance().RegisterType("CameraEffectOutline",
        []() -> std::shared_ptr<core::ISerializable> { return std::make_shared<CameraEffectOutline>(); });
    core::TypeRegistry::GetInstance().RegisterType("CameraEffectPixelate",
        []() -> std::shared_ptr<core::ISerializable> { return std::make_shared<CameraEffectPixelate>(); });
    core::TypeRegistry::GetInstance().RegisterType("CameraEffectSharpen",
        []() -> std::shared_ptr<core::ISerializable> { return std::make_shared<CameraEffectSharpen>(); });
    core::TypeRegistry::GetInstance().RegisterType("CameraEffectChromaticAberration",
        []() -> std::shared_ptr<core::ISerializable> { return std::make_shared<CameraEffectChromaticAberration>(); });

    core::TypeRegistry::GetInstance().RegisterType("Panel",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<Panel>();
        });

    core::TypeRegistry::GetInstance().RegisterType("Container",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<Container>();
        });

    core::TypeRegistry::GetInstance().RegisterType("PaddingContainer",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<PaddingContainer>();
        });

    core::TypeRegistry::GetInstance().RegisterType("CenterContainer",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<CenterContainer>();
        });

    core::TypeRegistry::GetInstance().RegisterType("HorizontalContainer",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<HorizontalContainer>();
        });

    core::TypeRegistry::GetInstance().RegisterType("VerticalContainer",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<VerticalContainer>();
        });

    core::TypeRegistry::GetInstance().RegisterType("GridContainer",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<GridContainer>();
        });

    core::TypeRegistry::GetInstance().RegisterType("DockContainer",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<DockContainer>();
        });

    core::TypeRegistry::GetInstance().RegisterType("Stack",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<Stack>();
        });

    core::TypeRegistry::GetInstance().RegisterType("Wrap",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<Wrap>();
        });

    core::TypeRegistry::GetInstance().RegisterType("SplitContainer",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<SplitContainer>();
        });

    core::TypeRegistry::GetInstance().RegisterType("AspectRatioContainer",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<AspectRatioContainer>();
        });

    core::TypeRegistry::GetInstance().RegisterType("Spacer",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<Spacer>();
        });

    core::TypeRegistry::GetInstance().RegisterType("LayoutSlot",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<LayoutSlot>();
        });

    core::TypeRegistry::GetInstance().RegisterType("ScrollContainer",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<ScrollContainer>();
        });

    core::TypeRegistry::GetInstance().RegisterType("TabContainer",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<TabContainer>();
        });

    core::TypeRegistry::GetInstance().RegisterType("Button",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<Button>();
        });

    core::TypeRegistry::GetInstance().RegisterType("TextureButton",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<TextureButton>();
        });

    core::TypeRegistry::GetInstance().RegisterType("ToggleButton",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<ToggleButton>();
        });

    core::TypeRegistry::GetInstance().RegisterType("TextureToggleButton",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<TextureToggleButton>();
        });

    core::TypeRegistry::GetInstance().RegisterType("RadioButton",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<RadioButton>();
        });

    core::TypeRegistry::GetInstance().RegisterType("RadioList",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<RadioList>();
        });

    core::TypeRegistry::GetInstance().RegisterType("Checkbox",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<Checkbox>();
        });

    core::TypeRegistry::GetInstance().RegisterType("CheckList",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<CheckList>();
        });

    core::TypeRegistry::GetInstance().RegisterType("NineSlicePanel",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<NineSlicePanel>();
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
    core::TypeRegistry::GetInstance().RegisterType("Curve2D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<Curve2D>();
        });
    core::TypeRegistry::GetInstance().RegisterType("Path2D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<Path2D>();
        });
    core::TypeRegistry::GetInstance().RegisterType("Curve3D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<Curve3D>();
        });
    core::TypeRegistry::GetInstance().RegisterType("Path3D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<Path3D>();
        });
    core::TypeRegistry::GetInstance().RegisterType("PathFollow3D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<PathFollow3D>();
        });
    core::TypeRegistry::GetInstance().RegisterType("VectorGraphic2D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<VectorGraphic2D>();
        });
    core::TypeRegistry::GetInstance().RegisterType("Empty2D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<Empty2D>();
        });
    core::TypeRegistry::GetInstance().RegisterType("Empty3D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<Empty3D>();
        });
    core::TypeRegistry::GetInstance().RegisterType("NavigationRegion2D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<NavigationRegion2D>();
        });
    core::TypeRegistry::GetInstance().RegisterType("NavigationAgent2D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<NavigationAgent2D>();
        });
    core::TypeRegistry::GetInstance().RegisterType("NavigationObstacle2D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<NavigationObstacle2D>();
        });
    core::TypeRegistry::GetInstance().RegisterType("NavigationRegion3D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<NavigationRegion3D>();
        });
    core::TypeRegistry::GetInstance().RegisterType("NavigationAgent3D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<NavigationAgent3D>();
        });
    core::TypeRegistry::GetInstance().RegisterType("NavigationObstacle3D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<NavigationObstacle3D>();
        });
    core::TypeRegistry::GetInstance().RegisterType("NetworkObject",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<NetworkObject>();
        });
    core::TypeRegistry::GetInstance().RegisterType("NetworkSynchronizer",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<NetworkSynchronizer>();
        });
    core::TypeRegistry::GetInstance().RegisterType("NetworkTransform2D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<NetworkTransform2D>();
        });
    core::TypeRegistry::GetInstance().RegisterType("NetworkTransform3D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<NetworkTransform3D>();
        });
    core::TypeRegistry::GetInstance().RegisterType("NetworkSpawner",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<NetworkSpawner>();
        });
    core::TypeRegistry::GetInstance().RegisterType("NetworkController",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<NetworkController>();
        });
    core::TypeRegistry::GetInstance().RegisterType("NetworkAnimator",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<NetworkAnimator>();
        });
    core::TypeRegistry::GetInstance().RegisterType("NetworkRigidBody2D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<NetworkRigidBody2D>();
        });
    core::TypeRegistry::GetInstance().RegisterType("NetworkRigidBody3D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<NetworkRigidBody3D>();
        });
    core::TypeRegistry::GetInstance().RegisterType("TileMap2D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<TileMap2D>();
        });
    core::TypeRegistry::GetInstance().RegisterType("YSort",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<YSort>();
        });
    core::TypeRegistry::GetInstance().RegisterType("ParallaxBackground",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<ParallaxBackground>();
        });
    core::TypeRegistry::GetInstance().RegisterType("ParallaxLayer",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<ParallaxLayer>();
        });
    core::TypeRegistry::GetInstance().RegisterType("MultiMeshGeneric",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<MultiMeshGeneric>();
        });
    core::TypeRegistry::GetInstance().RegisterType("ScatterMultiMesh",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<ScatterMultiMesh>();
        });
    core::TypeRegistry::GetInstance().RegisterType("CollisionScatterMultiMesh",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<CollisionScatterMultiMesh>();
        });
    core::TypeRegistry::GetInstance().RegisterType("NodeScatter",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<components::NodeScatter>();
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
    m_ComponentTypes.push_back(TypeInfo("RayCast2D", "Component", "Physics/2D", true));
    m_ComponentTypes.push_back(TypeInfo("ShapeCast2D", "Component", "Physics/2D", true));

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

    core::TypeRegistry::GetInstance().RegisterType("RayCast2D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<components::RayCast2D>();
        });

    core::TypeRegistry::GetInstance().RegisterType("ShapeCast2D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<components::ShapeCast2D>();
        });

    m_ComponentTypes.push_back(TypeInfo("RigidBody3DComponent", "Component", "Physics/3D", true));
    m_ComponentTypes.push_back(TypeInfo("StaticBody3DComponent", "Component", "Physics/3D", true));
    m_ComponentTypes.push_back(TypeInfo("KinematicBody3DComponent", "Component", "Physics/3D", true));
    m_ComponentTypes.push_back(TypeInfo("AreaTrigger3DComponent", "Component", "Physics/3D", true));
    m_ComponentTypes.push_back(TypeInfo("CollisionMesh3DComponent", "Component", "Physics/3D", true));
    m_ComponentTypes.push_back(TypeInfo("CharacterController3D", "Component", "Physics/3D", true));
    m_ComponentTypes.push_back(TypeInfo("RayCast3D", "Component", "Physics/3D", true));
    m_ComponentTypes.push_back(TypeInfo("ShapeCast3D", "Component", "Physics/3D", true));

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

    core::TypeRegistry::GetInstance().RegisterType("RayCast3D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<components::RayCast3D>();
        });

    core::TypeRegistry::GetInstance().RegisterType("ShapeCast3D",
        []() -> std::shared_ptr<core::ISerializable> {
            return std::make_shared<components::ShapeCast3D>();
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

    // Scan for custom scripted components using CustomComponentRegistry
    // This finds scripts with @component_class directive or native class syntax
    ScanCustomScriptedComponents(projectDir);

    // Load native C++ extensions (.lupineext) and surface their component classes
    // in the editor's Add-Component menu. Their properties are edited through the
    // standard inspector path (SerializeWithMetadata) with no extra handling.
    ScanNativeExtensions(projectDir);

    // Scan for archetype definitions (data .archetype files and @archetype_class scripts)
    core::ArchetypeRegistry::GetInstance().ScanProject(projectDir);

    // Scan for interface definitions (.interface files and @interface_define scripts)
    core::InterfaceRegistry::GetInstance().ScanProject(projectDir);

    // Scan localization tables (.loctable JSON and .csv) + localization.json config
    // so the editor viewport can resolve Localization Key fields live.
    localization::LocalizationManager::GetInstance().LoadProject(projectDir);
    localization::LocalizationManager::GetInstance().SetHotReloadEnabled(true);

    // Load the project's default UI theme so the editor viewport renders UI
    // components with their themed values.
    ui::ThemeManager::GetInstance().LoadProject(projectDir, m_DefaultThemePath);
}

// Helper function to determine subcategory based on base component type
static std::string GetSubcategoryForBaseComponent(const std::string& baseType) {
    // Map base component types to their appropriate subcategories
    static const std::unordered_map<std::string, std::string> baseTypeToSubcategory = {
        // 2D Rendering
        {"Sprite2D", "Custom/2D"},
        {"Particles2D", "Custom/2D"},
        {"AnimatedSprite2D", "Custom/2D"},
        {"GifPlayer", "Custom/2D"},
        {"VideoPlayer", "Custom/2D"},
        {"ColorRect", "Custom/2D"},
        {"Image2D", "Custom/2D"},
        {"Shape2D", "Custom/2D"},
        {"Line2D", "Custom/2D"},
        {"Curve2D", "Custom/2D"},
        {"Path2D", "Custom/2D"},
        {"Curve3D", "Custom/3D"},
        {"Path3D", "Custom/3D"},
        {"PathFollow3D", "Custom/3D"},
        {"VectorGraphic2D", "Custom/2D"},
        {"Empty2D", "Custom/2D"},
        {"Empty3D", "Custom/3D"},
        {"Light2D", "Custom/2D/Lighting"},
        {"LightOccluder2D", "Custom/2D/Lighting"},
        {"YSort", "Custom/2D"},
        {"TileMap2D", "Custom/2D"},
        {"ParallaxBackground", "Custom/2D"},
        {"ParallaxLayer", "Custom/2D"},
        {"NavigationRegion2D", "Custom/Navigation"},
        {"NavigationAgent2D", "Custom/Navigation"},
        {"NavigationObstacle2D", "Custom/Navigation"},
        {"NavigationRegion3D", "Custom/Navigation"},
        {"NavigationAgent3D", "Custom/Navigation"},
        {"NavigationObstacle3D", "Custom/Navigation"},
        {"NetworkObject", "Custom/Network"},
        {"NetworkSynchronizer", "Custom/Network"},
        {"NetworkTransform2D", "Custom/Network"},
        {"NetworkTransform3D", "Custom/Network"},
        {"NetworkSpawner", "Custom/Network"},
        {"NetworkController", "Custom/Network"},
        {"NetworkAnimator", "Custom/Network"},
        {"NetworkRigidBody2D", "Custom/Network"},
        {"NetworkRigidBody3D", "Custom/Network"},

        // 3D Rendering
        {"Sprite3D", "Custom/3D"},
        {"Particles3D", "Custom/3D"},
        {"AnimatedSprite3D", "Custom/3D"},
        {"StaticMesh3D", "Custom/3D"},
        {"SkeletalMesh3D", "Custom/3D"},
        {"PrimitiveMesh3D", "Custom/3D"},
        {"Label3D", "Custom/3D"},
        {"Panel3D", "Custom/3D"},
        {"Button3D", "Custom/3D"},
        {"ProgressBar3D", "Custom/3D"},
        {"DirectionalLight3D", "Custom/3D/Lighting"},
        {"OmniLight3D", "Custom/3D/Lighting"},
        {"SpotLight3D", "Custom/3D/Lighting"},
        {"MultiMeshGeneric", "Custom/3D"},
        {"ScatterMultiMesh", "Custom/3D"},
        {"CollisionScatterMultiMesh", "Custom/3D"},
        {"NodeScatter", "Custom/3D"},

        // UI
        {"Label", "Custom/UI"},
        {"Button", "Custom/UI"},
        {"TextureButton", "Custom/UI"},
        {"ToggleButton", "Custom/UI"},
        {"TextureToggleButton", "Custom/UI"},
        {"RadioButton", "Custom/UI"},
        {"RadioList", "Custom/UI"},
        {"Checkbox", "Custom/UI"},
        {"CheckList", "Custom/UI"},
        {"Panel", "Custom/UI"},
        {"NineSlicePanel", "Custom/UI"},
        {"ProgressBar", "Custom/UI"},
        {"Slider", "Custom/UI"},
        {"LineEdit", "Custom/UI"},
        {"SpinBox", "Custom/UI"},
        {"TextEdit", "Custom/UI"},
        {"ItemList", "Custom/UI"},
        {"Dropdown", "Custom/UI"},
        {"PopupMenu", "Custom/UI"},
        {"RichTextLabel", "Custom/UI"},
        {"Tree", "Custom/UI"},
        {"Container", "Custom/UI/Containers"},
        {"PaddingContainer", "Custom/UI/Containers"},
        {"CenterContainer", "Custom/UI/Containers"},
        {"HorizontalContainer", "Custom/UI/Containers"},
        {"VerticalContainer", "Custom/UI/Containers"},
        {"GridContainer", "Custom/UI/Containers"},
        {"DockContainer", "Custom/UI/Containers"},
        {"Stack", "Custom/UI/Containers"},
        {"Wrap", "Custom/UI/Containers"},
        {"SplitContainer", "Custom/UI/Containers"},
        {"AspectRatioContainer", "Custom/UI/Containers"},
        {"Spacer", "Custom/UI/Layout"},
        {"LayoutSlot", "Custom/UI/Layout"},
        {"ScrollContainer", "Custom/UI/Containers"},
        {"TabContainer", "Custom/UI/Containers"},

        // Physics 2D (support both with and without Component suffix)
        {"CollisionBody2D", "Custom/Physics/2D"},
        {"CollisionBody2DComponent", "Custom/Physics/2D"},
        {"RigidBody2D", "Custom/Physics/2D"},
        {"RigidBody2DComponent", "Custom/Physics/2D"},
        {"StaticBody2D", "Custom/Physics/2D"},
        {"StaticBody2DComponent", "Custom/Physics/2D"},
        {"KinematicBody2D", "Custom/Physics/2D"},
        {"KinematicBody2DComponent", "Custom/Physics/2D"},
        {"AreaTrigger2D", "Custom/Physics/2D"},
        {"AreaTrigger2DComponent", "Custom/Physics/2D"},
        {"CharacterController2D", "Custom/Physics/2D"},
        {"CharacterController2DComponent", "Custom/Physics/2D"},
        {"RayCast2D", "Custom/Physics/2D"},
        {"ShapeCast2D", "Custom/Physics/2D"},

        // Physics 3D (support both with and without Component suffix)
        {"RigidBody3D", "Custom/Physics/3D"},
        {"RigidBody3DComponent", "Custom/Physics/3D"},
        {"StaticBody3D", "Custom/Physics/3D"},
        {"StaticBody3DComponent", "Custom/Physics/3D"},
        {"KinematicBody3D", "Custom/Physics/3D"},
        {"KinematicBody3DComponent", "Custom/Physics/3D"},
        {"AreaTrigger3D", "Custom/Physics/3D"},
        {"AreaTrigger3DComponent", "Custom/Physics/3D"},
        {"CharacterController3D", "Custom/Physics/3D"},
        {"CharacterController3DComponent", "Custom/Physics/3D"},
        {"RayCast3D", "Custom/Physics/3D"},
        {"ShapeCast3D", "Custom/Physics/3D"},

        // Audio
        {"AudioPlayer", "Custom/Audio"},
        {"AudioListener", "Custom/Audio"},

        // Utility
        {"Timer", "Custom/Utility"},
        {"AnimationPlayer", "Custom/Animation"},
        {"AnimationTree", "Custom/Animation"},
        {"SubViewport", "Custom/Rendering"},
        {"CameraEffectColorGrade", "Custom/Camera Effects"},
        {"CameraEffectTonemap", "Custom/Camera Effects"},
        {"CameraEffectVignette", "Custom/Camera Effects"},
        {"CameraEffectFilmGrain", "Custom/Camera Effects"},
        {"CameraEffectColorInvert", "Custom/Camera Effects"},
        {"CameraEffectPosterize", "Custom/Camera Effects"},
        {"CameraEffectHueShift", "Custom/Camera Effects"},
        {"CameraEffectBlur", "Custom/Camera Effects"},
        {"CameraEffectGlow", "Custom/Camera Effects"},
        {"CameraEffectOutline", "Custom/Camera Effects"},
        {"CameraEffectPixelate", "Custom/Camera Effects"},
        {"CameraEffectSharpen", "Custom/Camera Effects"},
        {"CameraEffectChromaticAberration", "Custom/Camera Effects"},
        {"WorldEnvironment", "Custom/Utility"},
        {"Camera2D", "Custom/Utility"},
        {"Camera3D", "Custom/Utility"},

        // Base
        {"Component", "Custom"},
    };

    auto it = baseTypeToSubcategory.find(baseType);
    if (it != baseTypeToSubcategory.end()) {
        return it->second;
    }
    return "Custom";  // Default fallback
}

void EditorBridge::ScanCustomScriptedComponents(const std::string& projectDir) {
    // Use CustomComponentRegistry to scan for custom component definitions
    auto& registry = core::CustomComponentRegistry::GetInstance();
    registry.ScanProject(projectDir);

    // Add discovered custom components to our component types list
    for (const auto& def : registry.GetDefinitions()) {
        if (!def.isValid) continue;

        // Check if already registered (avoid duplicates)
        bool alreadyExists = false;
        for (const auto& existing : m_ComponentTypes) {
            if (existing.typeName == def.className) {
                alreadyExists = true;
                break;
            }
        }
        if (alreadyExists) continue;

        TypeInfo info;
        info.typeName = def.className;
        info.category = "Component";

        // Determine subcategory based on inheritance
        if (!def.baseComponentType.empty()) {
            info.subcategory = GetSubcategoryForBaseComponent(def.baseComponentType);
        } else if (!def.subcategory.empty()) {
            info.subcategory = def.subcategory;
        } else {
            info.subcategory = "Custom";
        }

        info.filePath = def.scriptPath;
        info.isBuiltIn = false;

        m_ComponentTypes.push_back(info);

        LOG_DEBUG(LogCategory::Core, "EditorBridge: Registered custom component '{}' (extends {}) in category '{}'",
                  def.className, def.baseComponentType.empty() ? "none" : def.baseComponentType, info.subcategory);
    }

    // Set up callback for when custom component definitions change (for hot-reload)
    registry.SetDefinitionsChangedCallback([this]() {
        RefreshCustomScriptedComponents();
    });
}

void EditorBridge::ScanNativeExtensions(const std::string& projectDir) {
    // Load native C++ extension binaries from the project (idempotent per binary).
    int loaded = core::ExtensionManager::GetInstance().LoadExtensionsForProject(projectDir);
    if (loaded > 0) {
        LOG_INFO(LogCategory::Core, "EditorBridge: loaded {} native extension(s)", loaded);
    }

    // Surface every registered native component class in the Add-Component menu.
    for (const core::NativeComponentClass* cls :
         core::NativeExtensionRegistry::GetInstance().GetAllClasses()) {
        if (!cls) continue;

        bool alreadyExists = false;
        for (const auto& existing : m_ComponentTypes) {
            if (existing.typeName == cls->className) { alreadyExists = true; break; }
        }
        if (alreadyExists) continue;

        TypeInfo info;
        info.typeName = cls->className;
        info.category = "Component";

        if (!cls->displayCategory.empty()) {
            info.subcategory = cls->displayCategory;
        } else if (!cls->baseClassName.empty()) {
            info.subcategory = GetSubcategoryForBaseComponent(cls->baseClassName);
        } else {
            info.subcategory = "Native";
        }

        info.filePath.clear();
        info.isBuiltIn = false;
        m_ComponentTypes.push_back(info);

        LOG_DEBUG(LogCategory::Core, "EditorBridge: registered native component '{}' in category '{}'",
                  cls->className, info.subcategory);
    }
}

void EditorBridge::RefreshCustomScriptedComponents() {
    // Remove existing custom components from the list
    m_ComponentTypes.erase(
        std::remove_if(m_ComponentTypes.begin(), m_ComponentTypes.end(),
            [](const TypeInfo& info) {
                // Remove if it's a custom scripted component (not built-in, has file path with script extension)
                if (info.isBuiltIn) return false;
                std::string ext = platform::Path::GetExtension(info.filePath);
                return (ext == ".lua" || ext == ".py" || ext == ".rb");
            }),
        m_ComponentTypes.end()
    );

    // Re-add custom components from the registry
    auto& registry = core::CustomComponentRegistry::GetInstance();
    for (const auto& def : registry.GetDefinitions()) {
        if (!def.isValid) continue;

        TypeInfo info;
        info.typeName = def.className;
        info.category = "Component";

        // Determine subcategory based on inheritance
        if (!def.baseComponentType.empty()) {
            info.subcategory = GetSubcategoryForBaseComponent(def.baseComponentType);
        } else if (!def.subcategory.empty()) {
            info.subcategory = def.subcategory;
        } else {
            info.subcategory = "Custom";
        }

        info.filePath = def.scriptPath;
        info.isBuiltIn = false;

        m_ComponentTypes.push_back(info);
    }

    LOG_DEBUG(LogCategory::Core, "EditorBridge: Refreshed custom scripted components");
}

namespace {

/**
 * Resolve a res:// or filesystem path to a physical path for archetype file IO.
 */
std::string ResolveArchetypeIOPath(const std::string& path) {
    if (path.rfind("res://", 0) == 0) {
        return asset::Asset::ResolveAssetPath(path);
    }
    return path;
}

} // namespace

nlohmann::json EditorBridge::GetArchetypeDefinitions() {
    nlohmann::json definitions = nlohmann::json::array();
    core::ArchetypeRegistry& registry = core::ArchetypeRegistry::GetInstance();
    for (const core::ArchetypeDefinition& def : registry.GetDefinitions()) {
        if (!def.isValid) {
            continue;
        }
        nlohmann::json item;
        item["archetype_class"] = def.className;
        item["extends"] = def.baseClass;
        item["abstract"] = def.isAbstract;
        item["menu_path"] = def.menuPath;
        item["icon"] = def.iconPath;
        item["description"] = def.description;
        item["source"] = (def.source == core::ArchetypeSource::Script) ? "script" : "data";
        item["source_path"] = def.sourcePath;
        definitions.push_back(item);
    }
    return definitions;
}

nlohmann::json EditorBridge::GetArchetypeDefinition(const std::string& className) {
    const core::ArchetypeDefinition* def =
        core::ArchetypeRegistry::GetInstance().GetDefinition(className);
    if (!def) {
        return nlohmann::json(nullptr);
    }
    return def->Serialize();
}

bool EditorBridge::CreateArchetypeInstance(const std::string& className, const std::string& path) {
    const core::ArchetypeDefinition* def =
        core::ArchetypeRegistry::GetInstance().GetDefinition(className);
    if (!def) {
        LOG_ERROR(LogCategory::Core,
                  "EditorBridge: Cannot create instance, unknown archetype '{}'", className);
        return false;
    }

    nlohmann::json out;
    out["lupine_archetype_instance"] = 1;
    out["definition"] = def->sourcePath;

    asset::AssetDatabase& assetDb = asset::AssetDatabase::GetInstance();
    core::UUID defUuid = assetDb.GetUUIDForPath(def->sourcePath);
    out["definition_uuid"] = defUuid.IsValid() ? defUuid.ToString() : std::string();

    out["archetype_class"] = def->className;
    out["fields"] = core::ArchetypeRegistry::GetInstance().BuildEffectiveDefaultValues(className);

    std::string physical = ResolveArchetypeIOPath(path);
    platform::FileResult<> result = platform::FileSystem::WriteFile(physical, out.dump(2));
    return result.success;
}

nlohmann::json EditorBridge::LoadArchetypeInstance(const std::string& path) {
    asset::ArchetypeInstance instance;
    if (!instance.LoadFromFile(ResolveArchetypeIOPath(path))) {
        return nlohmann::json(nullptr);
    }

    nlohmann::json result;
    result["archetype_class"] = instance.GetArchetypeClass();
    result["definition"] = instance.GetDefinitionPath();
    result["properties"] = instance.GetResolvedFields();
    result["property_metadata"] =
        core::ArchetypeRegistry::GetInstance().BuildEffectivePropertyMetadata(instance.GetArchetypeClass());

    return result;
}

nlohmann::json EditorBridge::GetArchetypeEffectiveFields(const std::string& className) {
    return core::ArchetypeRegistry::GetInstance().BuildEffectiveFieldsJson(className);
}

bool EditorBridge::SaveArchetypeInstance(const std::string& path, const nlohmann::json& valuesJson) {
    std::string physical = ResolveArchetypeIOPath(path);

    nlohmann::json fileJson = nlohmann::json::object();
    platform::FileResult<std::string> readResult = platform::FileSystem::ReadFile(physical);
    if (readResult.success) {
        try {
            fileJson = nlohmann::json::parse(readResult.data);
        } catch (...) {
            fileJson = nlohmann::json::object();
        }
    }
    if (!fileJson.is_object()) {
        fileJson = nlohmann::json::object();
    }

    if (!fileJson.contains("lupine_archetype_instance")) {
        fileJson["lupine_archetype_instance"] = 1;
    }

    nlohmann::json fields =
        (fileJson.contains("fields") && fileJson["fields"].is_object())
            ? fileJson["fields"]
            : nlohmann::json::object();

    if (valuesJson.is_object()) {
        for (nlohmann::json::const_iterator it = valuesJson.begin(); it != valuesJson.end(); ++it) {
            fields[it.key()] = it.value();
        }
    }
    fileJson["fields"] = fields;

    platform::FileResult<> writeResult = platform::FileSystem::WriteFile(physical, fileJson.dump(2));
    if (!writeResult.success) {
        return false;
    }

    ReloadAsset(path);
    return true;
}

bool EditorBridge::CreateArchetypeDefinition(const std::string& path, const nlohmann::json& schemaJson) {
    nlohmann::json out = schemaJson.is_object() ? schemaJson : nlohmann::json::object();
    if (!out.contains("lupine_archetype")) {
        out["lupine_archetype"] = 1;
    }

    std::string physical = ResolveArchetypeIOPath(path);
    platform::FileResult<> result = platform::FileSystem::WriteFile(physical, out.dump(2));
    if (!result.success) {
        return false;
    }

    RescanArchetypes();
    return true;
}

nlohmann::json EditorBridge::LoadArchetypeDefinitionFile(const std::string& path) {
    std::string physical = ResolveArchetypeIOPath(path);
    platform::FileResult<std::string> readResult = platform::FileSystem::ReadFile(physical);
    if (!readResult.success) {
        return nlohmann::json(nullptr);
    }
    try {
        return nlohmann::json::parse(readResult.data);
    } catch (...) {
        return nlohmann::json(nullptr);
    }
}

void EditorBridge::RescanArchetypes() {
    if (m_ProjectPath.empty()) {
        return;
    }
    std::string projectDir = platform::Path::GetDirectory(m_ProjectPath);
    core::ArchetypeRegistry::GetInstance().ScanProject(projectDir);
}

// ===========================================================================
// Interface types
// ===========================================================================

nlohmann::json EditorBridge::GetInterfaceDefinitions() {
    nlohmann::json definitions = nlohmann::json::array();
    core::InterfaceRegistry& registry = core::InterfaceRegistry::GetInstance();
    for (const core::InterfaceDefinition& def : registry.GetDefinitions()) {
        if (!def.isValid) {
            continue;
        }
        nlohmann::json item;
        item["interface_name"] = def.name;
        item["description"] = def.description;
        item["extends"] = def.baseInterfaces;
        item["tags"] = def.tags;
        const char* source = "data";
        if (def.source == core::InterfaceSource::Script) {
            source = "script";
        } else if (def.source == core::InterfaceSource::Native) {
            source = "native";
        }
        item["source"] = source;
        item["source_path"] = def.sourcePath;
        item["method_count"] = static_cast<int>(def.methods.size());
        item["signal_count"] = static_cast<int>(def.signals.size());
        definitions.push_back(item);
    }
    return definitions;
}

nlohmann::json EditorBridge::GetInterfaceDefinition(const std::string& interfaceName) {
    const core::InterfaceDefinition* def =
        core::InterfaceRegistry::GetInstance().GetDefinition(interfaceName);
    if (!def) {
        return nlohmann::json(nullptr);
    }
    return def->Serialize();
}

bool EditorBridge::CreateInterfaceDefinition(const std::string& path, const nlohmann::json& schemaJson) {
    nlohmann::json out = schemaJson.is_object() ? schemaJson : nlohmann::json::object();
    if (!out.contains("lupine_interface")) {
        out["lupine_interface"] = 1;
    }

    std::string physical = ResolveArchetypeIOPath(path);
    platform::FileResult<> result = platform::FileSystem::WriteFile(physical, out.dump(2));
    if (!result.success) {
        return false;
    }

    RescanInterfaces();
    return true;
}

nlohmann::json EditorBridge::LoadInterfaceDefinitionFile(const std::string& path) {
    std::string physical = ResolveArchetypeIOPath(path);
    platform::FileResult<std::string> readResult = platform::FileSystem::ReadFile(physical);
    if (!readResult.success) {
        return nlohmann::json(nullptr);
    }
    try {
        return nlohmann::json::parse(readResult.data);
    } catch (...) {
        return nlohmann::json(nullptr);
    }
}

void EditorBridge::RescanInterfaces() {
    if (m_ProjectPath.empty()) {
        return;
    }
    std::string projectDir = platform::Path::GetDirectory(m_ProjectPath);
    core::InterfaceRegistry::GetInstance().ScanProject(projectDir);
}

nlohmann::json EditorBridge::GetNodeInterfaces(std::shared_ptr<Node> node) {
    nlohmann::json result;
    result["declared"] = nlohmann::json::array();
    result["implemented"] = nlohmann::json::array();
    result["conformance"] = nlohmann::json::array();
    if (!node) {
        return result;
    }

    std::vector<std::string> implemented = node->GetImplementedInterfaces();
    result["declared"] = node->GetDeclaredInterfaces();
    result["implemented"] = implemented;

    nlohmann::json conformance = nlohmann::json::array();
    for (const std::string& iface : implemented) {
        conformance.push_back(node->VerifyInterface(iface));
    }
    result["conformance"] = conformance;
    return result;
}

nlohmann::json EditorBridge::VerifyNodeInterface(std::shared_ptr<Node> node,
                                                 const std::string& interfaceName) {
    if (!node) {
        return nlohmann::json(nullptr);
    }
    return node->VerifyInterface(interfaceName);
}

nlohmann::json EditorBridge::GetArchetypeInterfaces(const std::string& className) {
    return core::ArchetypeRegistry::GetInstance().GetImplementedInterfaces(className);
}

nlohmann::json EditorBridge::GetInterfaceImplementers(const std::string& interfaceName) {
    nlohmann::json result;

    nlohmann::json nodes = nlohmann::json::array();
    core::Scene* scene = m_SceneManager ? m_SceneManager->GetCurrentScene() : nullptr;
    if (scene) {
        for (core::Node* n : scene->GetNodesImplementingInterface(interfaceName)) {
            if (n) {
                nodes.push_back(n->GetPath());
            }
        }
    }
    result["nodes"] = nodes;
    result["archetypes"] =
        core::ArchetypeRegistry::GetInstance().GetArchetypesImplementing(interfaceName);
    result["component_types"] =
        core::InterfaceRegistry::GetInstance().GetTypesImplementing(interfaceName);
    return result;
}

nlohmann::json EditorBridge::FindNodesByClass(const std::string& className) {
    nlohmann::json out = nlohmann::json::array();
    core::Scene* scene = m_SceneManager ? m_SceneManager->GetCurrentScene() : nullptr;
    if (!scene) {
        return out;
    }

    std::function<void(core::Node*)> walk = [&](core::Node* node) {
        if (!node) {
            return;
        }
        bool match = className.empty() || node->GetTypeName() == className;
        if (!match) {
            for (const std::shared_ptr<core::Component>& comp : node->GetComponents()) {
                if (!comp) {
                    continue;
                }
                if (comp->GetTypeName() == className) {
                    match = true;
                    break;
                }
                core::ScriptComponent* sc = dynamic_cast<core::ScriptComponent*>(comp.get());
                if (sc && sc->GetScriptDisplayName() == className) {
                    match = true;
                    break;
                }
            }
        }
        if (match) {
            nlohmann::json item;
            item["path"] = node->GetPath();
            item["name"] = node->GetName();
            item["type"] = node->GetTypeName();
            out.push_back(item);
        }
        for (const std::shared_ptr<core::Node>& child : node->GetChildren()) {
            walk(child.get());
        }
    };
    walk(scene->GetRoot().get());
    return out;
}

nlohmann::json EditorBridge::FindArchetypeInstances(const std::string& className) {
    nlohmann::json out = nlohmann::json::array();
    asset::AssetDatabase& db = asset::AssetDatabase::GetInstance();
    core::ArchetypeRegistry& reg = core::ArchetypeRegistry::GetInstance();

    for (const asset::AssetMeta& meta : db.GetAssetsByType(asset::AssetImportType::Archetype)) {
        if (platform::Path::GetExtension(meta.path) != ".ares") {
            continue;
        }
        platform::FileResult<std::string> read =
            platform::FileSystem::ReadFile(ResolveArchetypeIOPath(meta.path));
        if (!read.success) {
            continue;
        }
        nlohmann::json j;
        try {
            j = nlohmann::json::parse(read.data);
        } catch (...) {
            continue;
        }

        std::string instClass = j.value("archetype_class", std::string());
        if (!className.empty() && instClass != className && !reg.IsSubclassOf(instClass, className)) {
            continue;
        }

        std::string display;
        if (j.contains("fields") && j["fields"].is_object()) {
            const nlohmann::json& f = j["fields"];
            if (f.contains("display_name") && f["display_name"].is_string()) {
                display = f["display_name"].get<std::string>();
            } else if (f.contains("id") && f["id"].is_string()) {
                display = f["id"].get<std::string>();
            }
        }
        if (display.empty()) {
            display = platform::Path::GetFilenameWithoutExtension(meta.path);
        }

        nlohmann::json item;
        item["path"] = meta.path;
        item["display"] = display;
        item["archetype_class"] = instClass;
        out.push_back(item);
    }
    return out;
}

void EditorBridge::ReloadLocalization() {
    if (m_ProjectPath.empty()) {
        return;
    }
    localization::LocalizationManager& mgr = localization::LocalizationManager::GetInstance();
    std::string current = mgr.GetLocale();
    std::string projectDir = platform::Path::GetDirectory(m_ProjectPath);
    mgr.LoadProject(projectDir);
    if (!current.empty()) {
        mgr.SetLocale(current, false);
    }
}

void EditorBridge::ReloadTheme() {
    if (m_ProjectPath.empty()) {
        return;
    }
    // Clears the loaded-theme cache, reloads the project default theme (and any
    // per-node themes lazily on next resolve) from disk, and bumps the theme
    // version so UI components re-resolve their themed values.
    std::string projectDir = platform::Path::GetDirectory(m_ProjectPath);
    ui::ThemeManager::GetInstance().LoadProject(projectDir, m_DefaultThemePath);
}

void EditorBridge::SetPreviewLocale(const std::string& locale) {
    localization::LocalizationManager::GetInstance().SetLocale(locale, false);
}

std::string EditorBridge::GetLocalizationLocale() {
    return localization::LocalizationManager::GetInstance().GetLocale();
}

std::vector<std::string> EditorBridge::GetLocalizationLocales() {
    return localization::LocalizationManager::GetInstance().GetAvailableLocales();
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

bool EditorBridge::MoveNode(std::shared_ptr<Node> node, std::shared_ptr<Node> newParent, int targetIndex) {
    if (!node || !newParent) {
        return false;
    }

    auto command = std::make_shared<core::MoveNodeCommand>(node, newParent, targetIndex);
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

    // Include property metadata (type/hint/declaration order) like GetComponentProperties
    // so the inspector can build correctly-typed widgets for a node's own registered
    // properties (e.g. Camera2D zoom/ortho_size, CameraUI canvas_size/zoom), not just the
    // transform. Falls back to the plain form if metadata serialization fails.
    try {
        return node->SerializeWithMetadata();
    } catch (const std::exception&) {
        return node->Serialize();
    }
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

    // Anchor/layout edits on a UIControl cascade: setting anchorPreset (or anchorMin/
    // anchorMax/layoutMode) rewrites several derived properties via OnPropertyChanged.
    // A single-property command can't undo that, so capture the whole layout property
    // group and restore it verbatim on undo/redo.
    if (std::dynamic_pointer_cast<components::UIControl>(component)) {
        static const std::vector<std::string> kCascadingAnchorProps = {
            "anchorPreset", "anchorMin", "anchorMax", "layoutMode"
        };
        bool isCascading = false;
        for (const std::string& name : kCascadingAnchorProps) {
            if (propertyName == name) { isCascading = true; break; }
        }
        if (isCascading) {
            static const std::vector<std::string> kLayoutGroup = {
                "anchorPreset", "anchorMin", "anchorMax",
                "offsetMin", "offsetMax", "layoutMode", "width", "height"
            };
            auto groupCommand = std::make_shared<core::SetComponentPropertyGroupCommand>(
                component, propertyName, value, kLayoutGroup);
            return ExecuteCommand(groupCommand);
        }
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
            // dump() is the only way to detect values the JSON writer cannot encode
            // (invalid UTF-8, NaN/Inf). Its result is intentionally discarded.
            (void)result.dump();
        } catch (const std::exception& e) {
            LOG_ERROR(LogCategory::Tools, "EditorBridge: component '{}' produced unencodable metadata, falling back to Serialize(): {}",
                      component->GetTypeName(), e.what());
            return component->Serialize();
        }

        return result;
    } catch (const std::exception& e) {
        LOG_ERROR(LogCategory::Tools, "EditorBridge: SerializeWithMetadata failed for component '{}', falling back to Serialize(): {}", component->GetTypeName(), e.what());
        return component->Serialize();
    }
}

nlohmann::json EditorBridge::CallComponentMethod(std::shared_ptr<Component> component,
                                                 const std::string& method, const nlohmann::json& args) {
    if (!component) {
        return nlohmann::json();
    }
    return component->CallMethod(method, args);
}

nlohmann::json EditorBridge::PreviewAnimationClip(std::shared_ptr<Node> node,
                                                  const nlohmann::json& clipJson, float time) {
    if (!node) {
        return nlohmann::json::array();
    }
    return animation::PreviewClip(node.get(), clipJson, time);
}

nlohmann::json EditorBridge::CaptureAnimationPose(std::shared_ptr<Node> node,
                                                  const nlohmann::json& targetsJson) {
    if (!node) {
        return nlohmann::json::array();
    }
    return animation::CapturePose(node.get(), targetsJson);
}

void EditorBridge::RestoreAnimationPose(std::shared_ptr<Node> node, const nlohmann::json& capturedJson) {
    if (!node) {
        return;
    }
    animation::RestorePose(node.get(), capturedJson);
}

namespace {

const char* PropertyValueTypeToString(core::PropertyValueType type) {
    switch (type) {
        case core::PropertyValueType::Int:        return "int";
        case core::PropertyValueType::Float:      return "float";
        case core::PropertyValueType::String:     return "string";
        case core::PropertyValueType::Bool:       return "bool";
        case core::PropertyValueType::Vec2:       return "vec2";
        case core::PropertyValueType::Vec3:       return "vec3";
        case core::PropertyValueType::Vec4:       return "vec4";
        case core::PropertyValueType::Color:      return "color";
        case core::PropertyValueType::NodePath:   return "node";
        case core::PropertyValueType::ScenePath:  return "scene";
        case core::PropertyValueType::Enum:       return "enum";
        case core::PropertyValueType::StringArray: return "string_array";
        case core::PropertyValueType::Double:     return "double";
        case core::PropertyValueType::Quat:       return "quat";
        case core::PropertyValueType::Rect:       return "rect";
        case core::PropertyValueType::Resource:   return "resource";
        case core::PropertyValueType::IntArray:   return "int_array";
        case core::PropertyValueType::FloatArray: return "float_array";
        case core::PropertyValueType::Array:      return "array";
        case core::PropertyValueType::Dictionary: return "dictionary";
    }
    return "float";
}

nlohmann::json SignalArgsToJson(const std::vector<core::SignalArgDesc>& args) {
    nlohmann::json arr = nlohmann::json::array();
    for (const core::SignalArgDesc& arg : args) {
        nlohmann::json a;
        a["name"] = arg.name;
        a["type"] = PropertyValueTypeToString(arg.type);
        arr.push_back(std::move(a));
    }
    return arr;
}

// Resolver mapping a connection target (always a Node in the editor model) to
// its stable UUID and current scene path.
core::SignalObject::TargetResolveFn MakeTargetResolver() {
    return [](core::SignalObject* target) -> core::SignalObject::TargetLocator {
        if (Node* n = dynamic_cast<Node*>(target)) {
            return { n->GetUUID().ToString(), n->GetPath() };
        }
        return {};
    };
}

} // namespace

nlohmann::json EditorBridge::GetNodeSignals(std::shared_ptr<Node> node) {
    nlohmann::json result = nlohmann::json::array();
    if (!node) {
        return result;
    }

    for (const core::SignalDesc& desc : node->GetSignalDescriptors()) {
        nlohmann::json entry;
        entry["source"] = "node";
        entry["source_label"] = "Node";
        entry["signal"] = desc.name;
        entry["doc"] = desc.doc;
        entry["args"] = SignalArgsToJson(desc.args);
        result.push_back(std::move(entry));
    }

    for (const auto& component : node->GetComponents()) {
        if (!component) {
            continue;
        }
        const std::string compUuid = component->GetUUID().ToString();
        const std::string compLabel = component->GetTypeName();
        for (const core::SignalDesc& desc : component->GetSignalDescriptors()) {
            nlohmann::json entry;
            entry["source"] = compUuid;
            entry["source_label"] = compLabel;
            entry["signal"] = desc.name;
            entry["doc"] = desc.doc;
            entry["args"] = SignalArgsToJson(desc.args);
            result.push_back(std::move(entry));
        }
    }

    return result;
}

nlohmann::json EditorBridge::GetNodeConnections(std::shared_ptr<Node> node) {
    nlohmann::json result = nlohmann::json::array();
    if (!node) {
        return result;
    }

    core::SignalObject::TargetResolveFn resolver = MakeTargetResolver();

    auto appendFrom = [&result](const nlohmann::json& serialized, const std::string& source) {
        if (!serialized.is_array()) {
            return;
        }
        for (nlohmann::json conn : serialized) {
            conn["source"] = source;
            result.push_back(std::move(conn));
        }
    };

    appendFrom(node->SerializeConnections(resolver), "node");
    for (const auto& component : node->GetComponents()) {
        if (component) {
            appendFrom(component->SerializeConnections(resolver), component->GetUUID().ToString());
        }
    }

    return result;
}

bool EditorBridge::AddConnection(std::shared_ptr<Node> sourceNode, const std::string& sourceComponentUuid,
                                 const std::string& signal, std::shared_ptr<Node> targetNode,
                                 const std::string& method, uint32_t flags) {
    if (!sourceNode || !targetNode || signal.empty() || method.empty()) {
        return false;
    }

    core::SignalObject* source = sourceNode.get();
    if (!sourceComponentUuid.empty()) {
        source = nullptr;
        for (const auto& component : sourceNode->GetComponents()) {
            if (component && component->GetUUID().ToString() == sourceComponentUuid) {
                source = component.get();
                break;
            }
        }
        if (!source) {
            return false;
        }
    }

    source->Connect(signal, targetNode.get(), method, flags);
    MarkSceneDirty();
    return true;
}

bool EditorBridge::RemoveConnection(std::shared_ptr<Node> sourceNode, const std::string& sourceComponentUuid,
                                    const std::string& signal, std::shared_ptr<Node> targetNode,
                                    const std::string& method) {
    if (!sourceNode || !targetNode || signal.empty() || method.empty()) {
        return false;
    }

    core::SignalObject* source = sourceNode.get();
    if (!sourceComponentUuid.empty()) {
        source = nullptr;
        for (const auto& component : sourceNode->GetComponents()) {
            if (component && component->GetUUID().ToString() == sourceComponentUuid) {
                source = component.get();
                break;
            }
        }
        if (!source) {
            return false;
        }
    }

    source->DisconnectTarget(signal, targetNode.get(), method);
    MarkSceneDirty();
    return true;
}

bool EditorBridge::ReloadScriptComponent(std::shared_ptr<Component> component) {
    if (!component) {
        return false;
    }

    // Try to cast to ScriptComponent
    auto scriptComponent = std::dynamic_pointer_cast<core::ScriptComponent>(component);
    if (!scriptComponent) {
        return false;
    }

    // Reload the script - this re-parses export properties
    bool result = scriptComponent->ReloadScript();

    // Re-register properties after reload to update the property list
    if (result) {
        component->RegisterProperties();
    }

    return result;
}

int EditorBridge::ReloadAsset(const std::string& assetPath) {
    if (assetPath.empty()) {
        return 0;
    }

    // Archetype assets (.ares instances, .archetype definitions) are not file-typed
    // component properties, so the generic component reload below skips them. Drop
    // the cached archetype state instead so any in-editor archetype reads/method
    // calls pick up the change. Definitions also need a registry rescan.
    auto endsWithCI = [](const std::string& s, const std::string& suffix) -> bool {
        if (s.size() < suffix.size()) {
            return false;
        }
        const size_t offset = s.size() - suffix.size();
        for (size_t i = 0; i < suffix.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(s[offset + i])) !=
                std::tolower(static_cast<unsigned char>(suffix[i]))) {
                return false;
            }
        }
        return true;
    };
    if (endsWithCI(assetPath, ".ares")) {
        scripting::ScriptAPI::InvalidateArchetypeCaches();
    } else if (endsWithCI(assetPath, ".archetype")) {
        RescanArchetypes();
        core::ArchetypeRuntime::GetInstance().Clear();
        scripting::ScriptAPI::InvalidateArchetypeCaches();
    }

    // Use the AssetReloadManager to notify all registered callbacks (for texture cache invalidation, etc.)
    int affected = rendering::AssetReloadManager::NotifyAssetChanged(assetPath);

    // Resolve the asset path for comparison
    std::string resolvedAssetPath = asset::AssetDatabase::GetInstance().ResolveAsset(assetPath);

    // Use the base Component::OnAssetFileChanged method to handle all components generically
    if (m_ActiveScene) {
        std::function<void(std::shared_ptr<Node>)> notifyAllComponents = [&](std::shared_ptr<Node> node) {
            if (!node) return;

            for (const auto& component : node->GetComponents()) {
                // Call the base Component method which handles file-type properties automatically
                if (component->OnAssetFileChanged(assetPath, resolvedAssetPath)) {
                    affected++;
                }
            }

            // Recurse to children
            for (const auto& child : node->GetChildren()) {
                notifyAllComponents(child);
            }
        };

        // Start from the scene root
        notifyAllComponents(m_ActiveScene->GetRoot());
    }

    return affected;
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

    if (m_RenderViews.find(viewID) == m_RenderViews.end()) return;

    if (mode == ViewMode::View2D) {

        float orthoHeight = static_cast<float>(m_ProjectWindowHeight);

        auto camera2D = std::make_unique<Camera2D>();
        camera2D->position = math::Vec2(0.0f, 0.0f);
        camera2D->rotation = 0.0f;
        camera2D->zoom = 1.0f;
        camera2D->orthoSize = orthoHeight;

        camera2D->clearColor = math::Color(0.2f, 0.2f, 0.25f, 1.0f);

        camera2D->isEditorCamera = true;
        camera2D->backend = m_RenderWorld->getBackend();

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
        camera3D->backend = m_RenderWorld->getBackend();

        renderView->setCamera(std::move(camera3D));
    }
}

void EditorBridge::RenderDebugOverlay(RenderViewID, const RenderViewInfo&) {

}

void EditorBridge::PanCamera2D(RenderViewID viewID, float deltaX, float deltaY) {
    if (!m_RenderWorld) return;

    lupine::RenderView* renderView = m_RenderWorld->getRenderView(viewID);
    if (!renderView) return;

    lupine::RenderCamera* camera = renderView->getCamera();
    Camera2D* camera2D = dynamic_cast<Camera2D*>(camera);
    if (!camera2D) return;

    const Viewport& viewport = renderView->getViewport();
    float viewportHeight = static_cast<float>(viewport.height);
    if (viewportHeight <= 0.0f || camera2D->zoom == 0.0f) return;

    float worldPerPixel = camera2D->orthoSize / (camera2D->zoom * viewportHeight);
    camera2D->position.x -= deltaX * worldPerPixel;
    camera2D->position.y += deltaY * worldPerPixel;
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

    // Try StaticMesh3D first
    StaticMesh3D* staticMesh = dynamic_cast<StaticMesh3D*>(component.get());
    if (staticMesh) {
        return staticMesh->GetMaterialSlotCount();
    }

    // Try SkeletalMesh3D
    SkeletalMesh3D* skeletalMesh = dynamic_cast<SkeletalMesh3D*>(component.get());
    if (skeletalMesh) {
        return skeletalMesh->GetMaterialSlotCount();
    }

    return 0;
}

nlohmann::json EditorBridge::GetMaterialSlotProperties(std::shared_ptr<Component> component, uint32_t slotIndex) {
    nlohmann::json result;

    if (!component) {
        return result;
    }

    // Try StaticMesh3D first
    StaticMesh3D* staticMesh = dynamic_cast<StaticMesh3D*>(component.get());
    if (staticMesh) {
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

        // Shader type
        switch (slot->shaderType) {
            case ShaderType::PBR:
                result["shaderType"] = "PBR";
                break;
            case ShaderType::Toon:
                result["shaderType"] = "Toon";
                break;
            case ShaderType::Stylized:
                result["shaderType"] = "Stylized";
                break;
            case ShaderType::Unlit:
                result["shaderType"] = "Unlit";
                break;
            case ShaderType::Standard3D:
                result["shaderType"] = "Standard3D";
                break;
            case ShaderType::Transparent:
                result["shaderType"] = "Transparent";
                break;
            case ShaderType::Glow:
                result["shaderType"] = "Glow";
                break;
            case ShaderType::Custom:
                result["shaderType"] = "Custom";
                break;
            default:
                result["shaderType"] = "PBR";
                break;
        }
        result["customShaderVert"] = slot->customVertShaderPath;
        result["customShaderFrag"] = slot->customFragShaderPath;
        result["customLshShader"] = slot->customLshShaderPath;

        // Toon shader parameters
        result["shadowBands"] = slot->shadowBands;
        result["shadowThreshold"] = slot->shadowThreshold;
        result["shadowSoftness"] = slot->shadowSoftness;
        result["specularBands"] = slot->specularBands;
        result["specularPower"] = slot->specularPower;
        result["rimIntensity"] = slot->rimIntensity;
        result["rimPower"] = slot->rimPower;

        // Stylized shader parameters
        result["stylizedShadowSoftness"] = slot->stylizedShadowSoftness;
        result["stylizedSpecularSoftness"] = slot->stylizedSpecularSoftness;
        result["stylizedShadowBrightness"] = slot->stylizedShadowBrightness;
        result["stylizedShadowWarmth"] = slot->stylizedShadowWarmth;
        result["stylizedSpecularIntensity"] = slot->stylizedSpecularIntensity;
        result["stylizedHalfLambertPower"] = slot->stylizedHalfLambertPower;

        // Generic shader parameters (uniform name -> value)
        nlohmann::json shaderParamsJson = nlohmann::json::object();
        for (const auto& [uniformName, value] : slot->shaderParams) {
            std::visit([&shaderParamsJson, &uniformName](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, float>) {
                    shaderParamsJson[uniformName] = {{"type", "float"}, {"value", arg}};
                } else if constexpr (std::is_same_v<T, int>) {
                    shaderParamsJson[uniformName] = {{"type", "int"}, {"value", arg}};
                } else if constexpr (std::is_same_v<T, bool>) {
                    shaderParamsJson[uniformName] = {{"type", "bool"}, {"value", arg}};
                } else if constexpr (std::is_same_v<T, Vec2>) {
                    shaderParamsJson[uniformName] = {{"type", "vec2"}, {"value", {arg.x, arg.y}}};
                } else if constexpr (std::is_same_v<T, Vec3>) {
                    shaderParamsJson[uniformName] = {{"type", "vec3"}, {"value", {arg.x, arg.y, arg.z}}};
                } else if constexpr (std::is_same_v<T, Vec4>) {
                    shaderParamsJson[uniformName] = {{"type", "vec4"}, {"value", {arg.x, arg.y, arg.z, arg.w}}};
                } else if constexpr (std::is_same_v<T, Color>) {
                    shaderParamsJson[uniformName] = {{"type", "color"}, {"value", {arg.r, arg.g, arg.b, arg.a}}};
                }
            }, value);
        }
        result["shaderParams"] = shaderParamsJson;

        return result;
    }

    // Try SkeletalMesh3D
    SkeletalMesh3D* skeletalMesh = dynamic_cast<SkeletalMesh3D*>(component.get());
    if (skeletalMesh) {
        const components::SkeletalMaterialSlot* slot = skeletalMesh->GetMaterialSlot(slotIndex);
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

        // Shader type
        switch (slot->shaderType) {
            case ShaderType::PBR:
                result["shaderType"] = "PBR";
                break;
            case ShaderType::Toon:
                result["shaderType"] = "Toon";
                break;
            case ShaderType::Stylized:
                result["shaderType"] = "Stylized";
                break;
            case ShaderType::Unlit:
                result["shaderType"] = "Unlit";
                break;
            case ShaderType::Standard3D:
                result["shaderType"] = "Standard3D";
                break;
            case ShaderType::Transparent:
                result["shaderType"] = "Transparent";
                break;
            case ShaderType::Glow:
                result["shaderType"] = "Glow";
                break;
            case ShaderType::Custom:
                result["shaderType"] = "Custom";
                break;
            default:
                result["shaderType"] = "PBR";
                break;
        }
        result["customShaderVert"] = slot->customVertShaderPath;
        result["customShaderFrag"] = slot->customFragShaderPath;
        result["customLshShader"] = slot->customLshShaderPath;

        // Toon shader parameters
        result["shadowBands"] = slot->shadowBands;
        result["shadowThreshold"] = slot->shadowThreshold;
        result["shadowSoftness"] = slot->shadowSoftness;
        result["specularBands"] = slot->specularBands;
        result["specularPower"] = slot->specularPower;
        result["rimIntensity"] = slot->rimIntensity;
        result["rimPower"] = slot->rimPower;

        // Stylized shader parameters
        result["stylizedShadowSoftness"] = slot->stylizedShadowSoftness;
        result["stylizedSpecularSoftness"] = slot->stylizedSpecularSoftness;
        result["stylizedShadowBrightness"] = slot->stylizedShadowBrightness;
        result["stylizedShadowWarmth"] = slot->stylizedShadowWarmth;
        result["stylizedSpecularIntensity"] = slot->stylizedSpecularIntensity;
        result["stylizedHalfLambertPower"] = slot->stylizedHalfLambertPower;

        // Generic shader parameters (uniform name -> value)
        nlohmann::json shaderParamsJson = nlohmann::json::object();
        for (const auto& [uniformName, value] : slot->shaderParams) {
            std::visit([&shaderParamsJson, &uniformName](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, float>) {
                    shaderParamsJson[uniformName] = {{"type", "float"}, {"value", arg}};
                } else if constexpr (std::is_same_v<T, int>) {
                    shaderParamsJson[uniformName] = {{"type", "int"}, {"value", arg}};
                } else if constexpr (std::is_same_v<T, bool>) {
                    shaderParamsJson[uniformName] = {{"type", "bool"}, {"value", arg}};
                } else if constexpr (std::is_same_v<T, Vec2>) {
                    shaderParamsJson[uniformName] = {{"type", "vec2"}, {"value", {arg.x, arg.y}}};
                } else if constexpr (std::is_same_v<T, Vec3>) {
                    shaderParamsJson[uniformName] = {{"type", "vec3"}, {"value", {arg.x, arg.y, arg.z}}};
                } else if constexpr (std::is_same_v<T, Vec4>) {
                    shaderParamsJson[uniformName] = {{"type", "vec4"}, {"value", {arg.x, arg.y, arg.z, arg.w}}};
                } else if constexpr (std::is_same_v<T, Color>) {
                    shaderParamsJson[uniformName] = {{"type", "color"}, {"value", {arg.r, arg.g, arg.b, arg.a}}};
                }
            }, value);
        }
        result["shaderParams"] = shaderParamsJson;

        return result;
    }

    return result;
}

bool EditorBridge::SetMaterialSlotProperty(std::shared_ptr<Component> component, uint32_t slotIndex, const std::string& propertyName, const nlohmann::json& value) {
    if (!component) {
        return false;
    }

    // Try StaticMesh3D first
    StaticMesh3D* staticMesh = dynamic_cast<StaticMesh3D*>(component.get());
    if (staticMesh) {
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
        // Shader selection
        else if (propertyName == "shaderType") {
            std::string shaderName = value.get<std::string>();
            if (shaderName == "PBR" || shaderName == "Skeletal") {
                slot->shaderType = ShaderType::PBR;
            } else if (shaderName == "Toon" || shaderName == "SkeletalToon") {
                slot->shaderType = ShaderType::Toon;
            } else if (shaderName == "Stylized" || shaderName == "SkeletalStylized") {
                slot->shaderType = ShaderType::Stylized;
            } else if (shaderName == "Unlit") {
                slot->shaderType = ShaderType::Unlit;
            } else if (shaderName == "Standard3D") {
                slot->shaderType = ShaderType::Standard3D;
            } else if (shaderName == "Transparent") {
                slot->shaderType = ShaderType::Transparent;
            } else if (shaderName == "Glow") {
                slot->shaderType = ShaderType::Glow;
            } else if (shaderName == "Custom") {
                slot->shaderType = ShaderType::Custom;
            }
        }
        else if (propertyName == "customShaderVert") {
            slot->customVertShaderPath = value.get<std::string>();
        }
        else if (propertyName == "customShaderFrag") {
            slot->customFragShaderPath = value.get<std::string>();
        }
        else if (propertyName == "customLshShader") {
            slot->customLshShaderPath = value.get<std::string>();
        }
        // Toon shader parameters
        else if (propertyName == "shadowBands") {
            slot->shadowBands = value.get<float>();
        }
        else if (propertyName == "shadowThreshold") {
            slot->shadowThreshold = value.get<float>();
        }
        else if (propertyName == "shadowSoftness") {
            slot->shadowSoftness = value.get<float>();
        }
        else if (propertyName == "specularBands") {
            slot->specularBands = value.get<float>();
        }
        else if (propertyName == "specularPower") {
            slot->specularPower = value.get<float>();
        }
        else if (propertyName == "rimIntensity") {
            slot->rimIntensity = value.get<float>();
        }
        else if (propertyName == "rimPower") {
            slot->rimPower = value.get<float>();
        }
        // Stylized shader parameters
        else if (propertyName == "stylizedShadowSoftness") {
            slot->stylizedShadowSoftness = value.get<float>();
        }
        else if (propertyName == "stylizedSpecularSoftness") {
            slot->stylizedSpecularSoftness = value.get<float>();
        }
        else if (propertyName == "stylizedShadowBrightness") {
            slot->stylizedShadowBrightness = value.get<float>();
        }
        else if (propertyName == "stylizedShadowWarmth") {
            slot->stylizedShadowWarmth = value.get<float>();
        }
        else if (propertyName == "stylizedSpecularIntensity") {
            slot->stylizedSpecularIntensity = value.get<float>();
        }
        else if (propertyName == "stylizedHalfLambertPower") {
            slot->stylizedHalfLambertPower = value.get<float>();
        }
        // Handle generic shader parameters (uniform name -> typed value)
        // Format: {"type": "vec4", "value": [x, y, z, w]}
        else if (value.is_object() && value.contains("type") && value.contains("value")) {
            std::string type = value["type"].get<std::string>();
            const auto& val = value["value"];

            if (type == "float") {
                slot->shaderParams[propertyName] = val.get<float>();
            } else if (type == "int") {
                slot->shaderParams[propertyName] = val.get<int>();
            } else if (type == "bool") {
                slot->shaderParams[propertyName] = val.get<bool>();
            } else if (type == "vec2" && val.is_array() && val.size() >= 2) {
                slot->shaderParams[propertyName] = Vec2(val[0].get<float>(), val[1].get<float>());
            } else if (type == "vec3" && val.is_array() && val.size() >= 3) {
                slot->shaderParams[propertyName] = Vec3(val[0].get<float>(), val[1].get<float>(), val[2].get<float>());
            } else if (type == "vec4" && val.is_array() && val.size() >= 4) {
                slot->shaderParams[propertyName] = Vec4(val[0].get<float>(), val[1].get<float>(), val[2].get<float>(), val[3].get<float>());
            } else if (type == "color" && val.is_array() && val.size() >= 4) {
                slot->shaderParams[propertyName] = Color(val[0].get<float>(), val[1].get<float>(), val[2].get<float>(), val[3].get<float>());
            } else {
                return false;
            }
        }
        else {
            return false;
        }

        MarkSceneDirty();
        return true;
    }

    // Try SkeletalMesh3D
    SkeletalMesh3D* skeletalMesh = dynamic_cast<SkeletalMesh3D*>(component.get());
    if (skeletalMesh) {
        components::SkeletalMaterialSlot* slot = skeletalMesh->GetMaterialSlot(slotIndex);
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
        // Shader selection
        else if (propertyName == "shaderType") {
            std::string shaderName = value.get<std::string>();
            if (shaderName == "PBR" || shaderName == "Skeletal") {
                slot->shaderType = ShaderType::PBR;
            } else if (shaderName == "Toon" || shaderName == "SkeletalToon") {
                slot->shaderType = ShaderType::Toon;
            } else if (shaderName == "Stylized" || shaderName == "SkeletalStylized") {
                slot->shaderType = ShaderType::Stylized;
            } else if (shaderName == "Unlit") {
                slot->shaderType = ShaderType::Unlit;
            } else if (shaderName == "Standard3D") {
                slot->shaderType = ShaderType::Standard3D;
            } else if (shaderName == "Transparent") {
                slot->shaderType = ShaderType::Transparent;
            } else if (shaderName == "Glow") {
                slot->shaderType = ShaderType::Glow;
            } else if (shaderName == "Custom") {
                slot->shaderType = ShaderType::Custom;
            }
        }
        else if (propertyName == "customShaderVert") {
            slot->customVertShaderPath = value.get<std::string>();
        }
        else if (propertyName == "customShaderFrag") {
            slot->customFragShaderPath = value.get<std::string>();
        }
        else if (propertyName == "customLshShader") {
            slot->customLshShaderPath = value.get<std::string>();
        }
        // Toon shader parameters
        else if (propertyName == "shadowBands") {
            slot->shadowBands = value.get<float>();
        }
        else if (propertyName == "shadowThreshold") {
            slot->shadowThreshold = value.get<float>();
        }
        else if (propertyName == "shadowSoftness") {
            slot->shadowSoftness = value.get<float>();
        }
        else if (propertyName == "specularBands") {
            slot->specularBands = value.get<float>();
        }
        else if (propertyName == "specularPower") {
            slot->specularPower = value.get<float>();
        }
        else if (propertyName == "rimIntensity") {
            slot->rimIntensity = value.get<float>();
        }
        else if (propertyName == "rimPower") {
            slot->rimPower = value.get<float>();
        }
        // Stylized shader parameters
        else if (propertyName == "stylizedShadowSoftness") {
            slot->stylizedShadowSoftness = value.get<float>();
        }
        else if (propertyName == "stylizedSpecularSoftness") {
            slot->stylizedSpecularSoftness = value.get<float>();
        }
        else if (propertyName == "stylizedShadowBrightness") {
            slot->stylizedShadowBrightness = value.get<float>();
        }
        else if (propertyName == "stylizedShadowWarmth") {
            slot->stylizedShadowWarmth = value.get<float>();
        }
        else if (propertyName == "stylizedSpecularIntensity") {
            slot->stylizedSpecularIntensity = value.get<float>();
        }
        else if (propertyName == "stylizedHalfLambertPower") {
            slot->stylizedHalfLambertPower = value.get<float>();
        }
        // Handle generic shader parameters (uniform name -> typed value)
        // Format: {"type": "vec4", "value": [x, y, z, w]}
        else if (value.is_object() && value.contains("type") && value.contains("value")) {
            std::string type = value["type"].get<std::string>();
            const auto& val = value["value"];

            if (type == "float") {
                slot->shaderParams[propertyName] = val.get<float>();
            } else if (type == "int") {
                slot->shaderParams[propertyName] = val.get<int>();
            } else if (type == "bool") {
                slot->shaderParams[propertyName] = val.get<bool>();
            } else if (type == "vec2" && val.is_array() && val.size() >= 2) {
                slot->shaderParams[propertyName] = Vec2(val[0].get<float>(), val[1].get<float>());
            } else if (type == "vec3" && val.is_array() && val.size() >= 3) {
                slot->shaderParams[propertyName] = Vec3(val[0].get<float>(), val[1].get<float>(), val[2].get<float>());
            } else if (type == "vec4" && val.is_array() && val.size() >= 4) {
                slot->shaderParams[propertyName] = Vec4(val[0].get<float>(), val[1].get<float>(), val[2].get<float>(), val[3].get<float>());
            } else if (type == "color" && val.is_array() && val.size() >= 4) {
                slot->shaderParams[propertyName] = Color(val[0].get<float>(), val[1].get<float>(), val[2].get<float>(), val[3].get<float>());
            } else {
                return false;
            }
        }
        else {
            return false;
        }

        MarkSceneDirty();
        return true;
    }

    return false;
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
                    zIndex = static_cast<float>(node2D->GetZIndex());
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

    if (!m_UndoRecordingEnabled) {
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

void EditorBridge::SetBusMuted(const std::string& busName, bool muted) {
    audio::AudioManager::GetInstance().SetBusMuted(busName, muted);
}

bool EditorBridge::IsBusMuted(const std::string& busName) const {
    return audio::AudioManager::GetInstance().IsBusMuted(busName);
}

void EditorBridge::SetBusSolo(const std::string& busName, bool solo) {
    audio::AudioManager::GetInstance().SetBusSolo(busName, solo);
}

bool EditorBridge::IsBusSolo(const std::string& busName) const {
    return audio::AudioManager::GetInstance().IsBusSolo(busName);
}

void EditorBridge::CreateAudioBus(const std::string& busName, const std::string& parentBus) {
    audio::AudioManager::GetInstance().CreateBus(busName, parentBus);
}

void EditorBridge::DestroyAudioBus(const std::string& busName) {
    audio::AudioManager::GetInstance().DestroyBus(busName);
}

int EditorBridge::AddBusEffect(const std::string& busName, const std::string& effectType) {
    audio::AudioEffectType type;
    if (!audio::AudioEffectTypeFromString(effectType, type)) {
        return -1;
    }
    return audio::AudioManager::GetInstance().AddBusEffect(busName, type);
}

void EditorBridge::RemoveBusEffect(const std::string& busName, int index) {
    audio::AudioManager::GetInstance().RemoveBusEffect(busName, index);
}

void EditorBridge::MoveBusEffect(const std::string& busName, int fromIndex, int toIndex) {
    audio::AudioManager::GetInstance().MoveBusEffect(busName, fromIndex, toIndex);
}

void EditorBridge::ClearBusEffects(const std::string& busName) {
    audio::AudioManager::GetInstance().ClearBusEffects(busName);
}

void EditorBridge::SetBusEffectEnabled(const std::string& busName, int index, bool enabled) {
    audio::AudioManager::GetInstance().SetBusEffectEnabled(busName, index, enabled);
}

void EditorBridge::SetBusEffectParameter(const std::string& busName, int index,
                                         const std::string& parameter, float value) {
    audio::AudioManager::GetInstance().SetBusEffectParameter(busName, index, parameter, value);
}

std::string EditorBridge::GetAudioBusesJson() const {
    audio::AudioManager& manager = audio::AudioManager::GetInstance();
    nlohmann::json buses = nlohmann::json::array();
    for (const auto& entry : manager.GetBuses()) {
        const audio::AudioBus& bus = entry.second;
        nlohmann::json busJson;
        busJson["name"] = bus.name;
        busJson["volume"] = bus.volume;
        busJson["muted"] = bus.muted;
        busJson["solo"] = bus.solo;
        busJson["parentBus"] = bus.parentBus;
        nlohmann::json effects = nlohmann::json::array();
        for (const audio::AudioEffectDesc& effect : bus.effects) {
            effects.push_back(effect.Serialize());
        }
        busJson["effects"] = effects;
        buses.push_back(busJson);
    }
    return buses.dump();
}

std::string EditorBridge::GetAudioEffectTypesJson() const {
    static const audio::AudioEffectType kTypes[] = {
        audio::AudioEffectType::Gain, audio::AudioEffectType::LowPass,
        audio::AudioEffectType::HighPass, audio::AudioEffectType::BandPass,
        audio::AudioEffectType::Peak, audio::AudioEffectType::LowShelf,
        audio::AudioEffectType::HighShelf, audio::AudioEffectType::Delay,
        audio::AudioEffectType::Reverb, audio::AudioEffectType::Compressor,
        audio::AudioEffectType::Distortion, audio::AudioEffectType::Chorus
    };
    nlohmann::json types = nlohmann::json::array();
    for (audio::AudioEffectType type : kTypes) {
        types.push_back(audio::AudioEffectTypeToString(type));
    }
    return types.dump();
}

std::string EditorBridge::GetAudioBusLevelsJson() const {
    audio::AudioManager& manager = audio::AudioManager::GetInstance();
    nlohmann::json levels = nlohmann::json::object();
    for (const auto& entry : manager.GetBuses()) {
        float left = 0.0f;
        float right = 0.0f;
        manager.GetBusLevels(entry.first, left, right);
        levels[entry.first] = nlohmann::json::array({left, right});
    }
    return levels.dump();
}

std::string EditorBridge::SerializeAudioBuses() const {
    return audio::AudioManager::GetInstance().Serialize().dump();
}

void EditorBridge::DeserializeAudioBuses(const std::string& json) {
    try {
        audio::AudioManager::GetInstance().Deserialize(nlohmann::json::parse(json));
    } catch (...) {
    }
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

math::Vec3 EditorBridge::ScreenToWorldOnPlane(RenderViewID viewID, float screenX, float screenY,
                                               float planeNormalX, float planeNormalY, float planeNormalZ,
                                               float planePointX, float planePointY, float planePointZ) {
    if (!m_RenderWorld) {
        return math::Vec3(0.0f, 0.0f, 0.0f);
    }

    lupine::RenderView* renderView = m_RenderWorld->getRenderView(viewID);
    if (!renderView) {
        return math::Vec3(0.0f, 0.0f, 0.0f);
    }

    auto camera = renderView->getCamera();
    if (!camera || camera->getType() != CameraType::Camera3D) {
        return math::Vec3(0.0f, 0.0f, 0.0f);
    }

    const Viewport& viewport = renderView->getViewport();
    float viewportWidth = static_cast<float>(viewport.width);
    float viewportHeight = static_cast<float>(viewport.height);

    if (viewportWidth <= 0.0f || viewportHeight <= 0.0f) {
        return math::Vec3(0.0f, 0.0f, 0.0f);
    }

    // Convert screen to NDC
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

    // Plane equation: dot(normal, point - planePoint) = 0
    // Ray equation: point = rayOrigin + t * rayDir
    // Substituting: dot(normal, rayOrigin + t * rayDir - planePoint) = 0
    // t = dot(normal, planePoint - rayOrigin) / dot(normal, rayDir)

    math::Vec3 planeNormal(planeNormalX, planeNormalY, planeNormalZ);
    math::Vec3 planePoint(planePointX, planePointY, planePointZ);

    float denom = planeNormal.Dot(rayDir);
    if (std::abs(denom) < 0.0001f) {
        // Ray is parallel to plane
        return math::Vec3(0.0f, 0.0f, 0.0f);
    }

    float t = planeNormal.Dot(planePoint - rayOrigin) / denom;

    if (t < 0.0f) {
        // Intersection is behind the camera
        return math::Vec3(0.0f, 0.0f, 0.0f);
    }

    math::Vec3 intersectionPoint = rayOrigin + rayDir * t;
    return intersectionPoint;
}

bool EditorBridge::GetCameraRay(RenderViewID viewID, float screenX, float screenY,
                                 math::Vec3& outOrigin, math::Vec3& outDirection) {
    if (!m_RenderWorld) {
        return false;
    }

    lupine::RenderView* renderView = m_RenderWorld->getRenderView(viewID);
    if (!renderView) {
        return false;
    }

    auto camera = renderView->getCamera();
    if (!camera || camera->getType() != CameraType::Camera3D) {
        return false;
    }

    const Viewport& viewport = renderView->getViewport();
    float viewportWidth = static_cast<float>(viewport.width);
    float viewportHeight = static_cast<float>(viewport.height);

    if (viewportWidth <= 0.0f || viewportHeight <= 0.0f) {
        return false;
    }

    // Convert screen to NDC
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

    outOrigin = math::Vec3(nearPoint.x, nearPoint.y, nearPoint.z);
    outDirection = math::Vec3(farPoint.x - nearPoint.x, farPoint.y - nearPoint.y, farPoint.z - nearPoint.z).Normalized();

    return true;
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

// ========================================================================
// Line2D Point Editing
// ========================================================================

std::string EditorBridge::GetLine2DPoints(std::shared_ptr<core::Component> component) {
    if (!component) {
        return "[]";
    }

    auto line2D = std::dynamic_pointer_cast<components::Line2D>(component);
    if (!line2D) {
        return "[]";
    }

    const auto& points = line2D->GetLine2DPoints();
    nlohmann::json jsonArray = nlohmann::json::array();

    for (const auto& pt : points) {
        nlohmann::json pointObj;
        pointObj["x"] = pt.position.x;
        pointObj["y"] = pt.position.y;
        pointObj["ctrlInX"] = pt.controlIn.x;
        pointObj["ctrlInY"] = pt.controlIn.y;
        pointObj["ctrlOutX"] = pt.controlOut.x;
        pointObj["ctrlOutY"] = pt.controlOut.y;
        pointObj["useBezier"] = pt.useBezier;
        pointObj["symmetric"] = pt.symmetricHandles;
        jsonArray.push_back(pointObj);
    }

    return jsonArray.dump();
}

void EditorBridge::AddLine2DPoint(std::shared_ptr<core::Component> component, float x, float y) {
    if (!component) {
        return;
    }

    auto line2D = std::dynamic_pointer_cast<components::Line2D>(component);
    if (!line2D) {
        return;
    }

    // Create point data for undo
    nlohmann::json pointData;
    pointData["x"] = x;
    pointData["y"] = y;
    pointData["ctrlInX"] = 0.0f;
    pointData["ctrlInY"] = 0.0f;
    pointData["ctrlOutX"] = 0.0f;
    pointData["ctrlOutY"] = 0.0f;
    pointData["useBezier"] = false;
    pointData["symmetricHandles"] = true;

    int pointIndex = static_cast<int>(line2D->GetPointCount());

    // Create and execute command
    auto cmd = std::make_shared<core::AddPointCommand>(component, pointIndex, pointData);
    ExecuteCommand(cmd);
    MarkSceneDirty();
}

void EditorBridge::AddLine2DPointWithBezier(std::shared_ptr<core::Component> component,
                                             float x, float y,
                                             float ctrlInX, float ctrlInY,
                                             float ctrlOutX, float ctrlOutY,
                                             bool useBezier) {
    if (!component) {
        return;
    }

    auto line2D = std::dynamic_pointer_cast<components::Line2D>(component);
    if (!line2D) {
        return;
    }

    components::Line2DPoint pt;
    pt.position = math::Vec2(x, y);
    pt.controlIn = math::Vec2(ctrlInX, ctrlInY);
    pt.controlOut = math::Vec2(ctrlOutX, ctrlOutY);
    pt.useBezier = useBezier;
    pt.symmetricHandles = true;

    line2D->AddLine2DPoint(pt);
    MarkSceneDirty();
}

void EditorBridge::UpdateLine2DPoint(std::shared_ptr<core::Component> component, int index, float x, float y) {
    if (!component) {
        return;
    }

    auto line2D = std::dynamic_pointer_cast<components::Line2D>(component);
    if (!line2D) {
        return;
    }

    line2D->SetPoint(static_cast<size_t>(index), math::Vec2(x, y));
    MarkSceneDirty();
}

void EditorBridge::UpdateLine2DPointBezier(std::shared_ptr<core::Component> component, int index,
                                            float ctrlInX, float ctrlInY,
                                            float ctrlOutX, float ctrlOutY,
                                            bool symmetric) {
    if (!component) {
        return;
    }

    auto line2D = std::dynamic_pointer_cast<components::Line2D>(component);
    if (!line2D) {
        return;
    }

    line2D->SetPointControlHandles(static_cast<size_t>(index),
                                    math::Vec2(ctrlInX, ctrlInY),
                                    math::Vec2(ctrlOutX, ctrlOutY),
                                    symmetric);
    MarkSceneDirty();
}

void EditorBridge::SetLine2DPointUseBezier(std::shared_ptr<core::Component> component, int index, bool useBezier) {
    if (!component) {
        return;
    }

    auto line2D = std::dynamic_pointer_cast<components::Line2D>(component);
    if (!line2D) {
        return;
    }

    line2D->SetPointUseBezier(static_cast<size_t>(index), useBezier);
    MarkSceneDirty();
}

void EditorBridge::RemoveLine2DPoint(std::shared_ptr<core::Component> component, int index) {
    if (!component) {
        return;
    }

    auto line2D = std::dynamic_pointer_cast<components::Line2D>(component);
    if (!line2D) {
        return;
    }

    size_t idx = static_cast<size_t>(index);
    if (idx >= line2D->GetPointCount()) {
        return;
    }

    // Save point data for undo
    auto pt = line2D->GetLine2DPoint(idx);
    nlohmann::json pointData;
    pointData["x"] = pt.position.x;
    pointData["y"] = pt.position.y;
    pointData["ctrlInX"] = pt.controlIn.x;
    pointData["ctrlInY"] = pt.controlIn.y;
    pointData["ctrlOutX"] = pt.controlOut.x;
    pointData["ctrlOutY"] = pt.controlOut.y;
    pointData["useBezier"] = pt.useBezier;
    pointData["symmetricHandles"] = pt.symmetricHandles;

    // Create and execute command
    auto cmd = std::make_shared<core::RemovePointCommand>(component, index, pointData);
    ExecuteCommand(cmd);
    MarkSceneDirty();
}

void EditorBridge::ClearLine2DPoints(std::shared_ptr<core::Component> component) {
    if (!component) {
        return;
    }

    auto line2D = std::dynamic_pointer_cast<components::Line2D>(component);
    if (!line2D) {
        return;
    }

    // Save all points for undo
    nlohmann::json oldPointsData = nlohmann::json::array();
    for (size_t i = 0; i < line2D->GetPointCount(); ++i) {
        auto pt = line2D->GetLine2DPoint(i);
        nlohmann::json pointData;
        pointData["x"] = pt.position.x;
        pointData["y"] = pt.position.y;
        pointData["ctrlInX"] = pt.controlIn.x;
        pointData["ctrlInY"] = pt.controlIn.y;
        pointData["ctrlOutX"] = pt.controlOut.x;
        pointData["ctrlOutY"] = pt.controlOut.y;
        pointData["useBezier"] = pt.useBezier;
        pointData["symmetricHandles"] = pt.symmetricHandles;
        oldPointsData.push_back(pointData);
    }

    // Create and execute command
    auto cmd = std::make_shared<core::ClearPointsCommand>(component, oldPointsData);
    ExecuteCommand(cmd);
    MarkSceneDirty();
}

void EditorBridge::SetLine2DShowHandles(std::shared_ptr<core::Component> component, bool show) {
    if (!component) {
        return;
    }

    auto line2D = std::dynamic_pointer_cast<components::Line2D>(component);
    if (!line2D) {
        return;
    }

    line2D->SetShowBezierHandles(show);
}

void EditorBridge::SetLine2DSelectedPoint(std::shared_ptr<core::Component> component, int index) {
    if (!component) {
        return;
    }

    auto line2D = std::dynamic_pointer_cast<components::Line2D>(component);
    if (!line2D) {
        return;
    }

    line2D->SetSelectedPointIndex(index);
}

int EditorBridge::HitTestLine2DControlPoint(std::shared_ptr<core::Component> component, float worldX, float worldY, float radius) {
    if (!component) {
        return -1;
    }

    auto line2D = std::dynamic_pointer_cast<components::Line2D>(component);
    if (!line2D) {
        return -1;
    }

    return line2D->HitTestControlPoint(math::Vec2(worldX, worldY), radius);
}

void EditorBridge::MoveLine2DControlPoint(std::shared_ptr<core::Component> component, int controlPointId, float worldX, float worldY) {
    if (!component) {
        return;
    }

    auto line2D = std::dynamic_pointer_cast<components::Line2D>(component);
    if (!line2D) {
        return;
    }

    // Decode control point ID: pointIndex * 10 + handleType (0=anchor, 1=controlIn, 2=controlOut)
    int pointIndex = controlPointId / 10;
    size_t idx = static_cast<size_t>(pointIndex);

    if (idx >= line2D->GetPointCount()) {
        return;
    }

    // Save old state for undo
    auto pt = line2D->GetLine2DPoint(idx);
    nlohmann::json oldPointData;
    oldPointData["x"] = pt.position.x;
    oldPointData["y"] = pt.position.y;
    oldPointData["ctrlInX"] = pt.controlIn.x;
    oldPointData["ctrlInY"] = pt.controlIn.y;
    oldPointData["ctrlOutX"] = pt.controlOut.x;
    oldPointData["ctrlOutY"] = pt.controlOut.y;
    oldPointData["useBezier"] = pt.useBezier;
    oldPointData["symmetricHandles"] = pt.symmetricHandles;

    // Move the control point
    line2D->MoveControlPoint(controlPointId, math::Vec2(worldX, worldY));

    // Get new state
    auto newPt = line2D->GetLine2DPoint(idx);
    nlohmann::json newPointData;
    newPointData["x"] = newPt.position.x;
    newPointData["y"] = newPt.position.y;
    newPointData["ctrlInX"] = newPt.controlIn.x;
    newPointData["ctrlInY"] = newPt.controlIn.y;
    newPointData["ctrlOutX"] = newPt.controlOut.x;
    newPointData["ctrlOutY"] = newPt.controlOut.y;
    newPointData["useBezier"] = newPt.useBezier;
    newPointData["symmetricHandles"] = newPt.symmetricHandles;

    // Create and record command (already executed)
    auto cmd = std::make_shared<core::ModifyPointCommand>(component, pointIndex, oldPointData, newPointData);
    RecordCommand(cmd);
    MarkSceneDirty();
}

// ========================================================================
// Curve2D Point Editing (also works for Path2D)
// ========================================================================

std::string EditorBridge::GetCurve2DPoints(std::shared_ptr<core::Component> component) {
    if (!component) {
        return "[]";
    }

    auto curve2D = std::dynamic_pointer_cast<components::Curve2D>(component);
    if (!curve2D) {
        return "[]";
    }

    const auto& points = curve2D->GetCurvePoints();
    nlohmann::json jsonArray = nlohmann::json::array();

    for (const auto& pt : points) {
        nlohmann::json pointObj;
        pointObj["x"] = pt.position.x;
        pointObj["y"] = pt.position.y;
        pointObj["ctrlInX"] = pt.controlIn.x;
        pointObj["ctrlInY"] = pt.controlIn.y;
        pointObj["ctrlOutX"] = pt.controlOut.x;
        pointObj["ctrlOutY"] = pt.controlOut.y;
        pointObj["useBezier"] = pt.useBezier;
        pointObj["symmetricHandles"] = pt.symmetricHandles;
        jsonArray.push_back(pointObj);
    }

    return jsonArray.dump();
}

void EditorBridge::AddCurve2DPoint(std::shared_ptr<core::Component> component, float x, float y) {
    if (!component) {
        return;
    }

    auto curve2D = std::dynamic_pointer_cast<components::Curve2D>(component);
    if (!curve2D) {
        return;
    }

    // Create point data for undo
    nlohmann::json pointData;
    pointData["x"] = x;
    pointData["y"] = y;
    pointData["ctrlInX"] = 0.0f;
    pointData["ctrlInY"] = 0.0f;
    pointData["ctrlOutX"] = 0.0f;
    pointData["ctrlOutY"] = 0.0f;
    pointData["useBezier"] = false;
    pointData["symmetricHandles"] = true;

    int pointIndex = static_cast<int>(curve2D->GetPointCount());

    // Create and execute command
    auto cmd = std::make_shared<core::AddPointCommand>(component, pointIndex, pointData);
    ExecuteCommand(cmd);
    MarkSceneDirty();
}

void EditorBridge::AddCurve2DPointWithBezier(std::shared_ptr<core::Component> component,
                                             float x, float y,
                                             float ctrlInX, float ctrlInY,
                                             float ctrlOutX, float ctrlOutY,
                                             bool useBezier) {
    if (!component) {
        return;
    }

    auto curve2D = std::dynamic_pointer_cast<components::Curve2D>(component);
    if (!curve2D) {
        return;
    }

    components::Curve2DPoint pt;
    pt.position = math::Vec2(x, y);
    pt.controlIn = math::Vec2(ctrlInX, ctrlInY);
    pt.controlOut = math::Vec2(ctrlOutX, ctrlOutY);
    pt.useBezier = useBezier;
    pt.symmetricHandles = true;

    curve2D->AddCurvePoint(pt);
    MarkSceneDirty();
}

void EditorBridge::UpdateCurve2DPoint(std::shared_ptr<core::Component> component, int index, float x, float y) {
    if (!component) {
        return;
    }

    auto curve2D = std::dynamic_pointer_cast<components::Curve2D>(component);
    if (!curve2D) {
        return;
    }

    curve2D->SetPointPosition(static_cast<size_t>(index), math::Vec2(x, y));
    MarkSceneDirty();
}

void EditorBridge::UpdateCurve2DPointBezier(std::shared_ptr<core::Component> component, int index,
                                            float ctrlInX, float ctrlInY,
                                            float ctrlOutX, float ctrlOutY,
                                            bool symmetric) {
    if (!component) {
        return;
    }

    auto curve2D = std::dynamic_pointer_cast<components::Curve2D>(component);
    if (!curve2D) {
        return;
    }

    curve2D->SetPointControlHandles(static_cast<size_t>(index),
                                    math::Vec2(ctrlInX, ctrlInY),
                                    math::Vec2(ctrlOutX, ctrlOutY),
                                    symmetric);
    MarkSceneDirty();
}

void EditorBridge::SetCurve2DPointUseBezier(std::shared_ptr<core::Component> component, int index, bool useBezier) {
    if (!component) {
        return;
    }

    auto curve2D = std::dynamic_pointer_cast<components::Curve2D>(component);
    if (!curve2D) {
        return;
    }

    curve2D->SetPointUseBezier(static_cast<size_t>(index), useBezier);
    MarkSceneDirty();
}

void EditorBridge::RemoveCurve2DPoint(std::shared_ptr<core::Component> component, int index) {
    if (!component) {
        return;
    }

    auto curve2D = std::dynamic_pointer_cast<components::Curve2D>(component);
    if (!curve2D) {
        return;
    }

    size_t idx = static_cast<size_t>(index);
    if (idx >= curve2D->GetPointCount()) {
        return;
    }

    // Save point data for undo
    const auto& points = curve2D->GetCurvePoints();
    const auto& pt = points[idx];
    nlohmann::json pointData;
    pointData["x"] = pt.position.x;
    pointData["y"] = pt.position.y;
    pointData["ctrlInX"] = pt.controlIn.x;
    pointData["ctrlInY"] = pt.controlIn.y;
    pointData["ctrlOutX"] = pt.controlOut.x;
    pointData["ctrlOutY"] = pt.controlOut.y;
    pointData["useBezier"] = pt.useBezier;
    pointData["symmetricHandles"] = pt.symmetricHandles;

    // Create and execute command
    auto cmd = std::make_shared<core::RemovePointCommand>(component, index, pointData);
    ExecuteCommand(cmd);
    MarkSceneDirty();
}

void EditorBridge::ClearCurve2DPoints(std::shared_ptr<core::Component> component) {
    if (!component) {
        return;
    }

    auto curve2D = std::dynamic_pointer_cast<components::Curve2D>(component);
    if (!curve2D) {
        return;
    }

    // Save all points for undo
    nlohmann::json oldPointsData = nlohmann::json::array();
    const auto& points = curve2D->GetCurvePoints();
    for (const auto& pt : points) {
        nlohmann::json pointData;
        pointData["x"] = pt.position.x;
        pointData["y"] = pt.position.y;
        pointData["ctrlInX"] = pt.controlIn.x;
        pointData["ctrlInY"] = pt.controlIn.y;
        pointData["ctrlOutX"] = pt.controlOut.x;
        pointData["ctrlOutY"] = pt.controlOut.y;
        pointData["useBezier"] = pt.useBezier;
        pointData["symmetricHandles"] = pt.symmetricHandles;
        oldPointsData.push_back(pointData);
    }

    // Create and execute command
    auto cmd = std::make_shared<core::ClearPointsCommand>(component, oldPointsData);
    ExecuteCommand(cmd);
    MarkSceneDirty();
}

void EditorBridge::SetCurve2DShowHandles(std::shared_ptr<core::Component> component, bool show) {
    if (!component) {
        return;
    }

    auto curve2D = std::dynamic_pointer_cast<components::Curve2D>(component);
    if (!curve2D) {
        return;
    }

    curve2D->SetShowBezierHandles(show);
}

void EditorBridge::SetCurve2DSelectedPoint(std::shared_ptr<core::Component> component, int index) {
    if (!component) {
        return;
    }

    auto curve2D = std::dynamic_pointer_cast<components::Curve2D>(component);
    if (!curve2D) {
        return;
    }

    curve2D->SetSelectedPointIndex(index);
}

int EditorBridge::HitTestCurve2DControlPoint(std::shared_ptr<core::Component> component, float worldX, float worldY, float radius) {
    if (!component) {
        return -1;
    }

    auto curve2D = std::dynamic_pointer_cast<components::Curve2D>(component);
    if (!curve2D) {
        return -1;
    }

    return curve2D->HitTestControlPoint(math::Vec2(worldX, worldY), radius);
}

void EditorBridge::MoveCurve2DControlPoint(std::shared_ptr<core::Component> component, int controlPointId, float worldX, float worldY) {
    if (!component) {
        return;
    }

    auto curve2D = std::dynamic_pointer_cast<components::Curve2D>(component);
    if (!curve2D) {
        return;
    }

    // Decode control point ID: pointIndex * 10 + handleType (0=anchor, 1=controlIn, 2=controlOut)
    int pointIndex = controlPointId / 10;
    size_t idx = static_cast<size_t>(pointIndex);

    if (idx >= curve2D->GetPointCount()) {
        return;
    }

    // Save old state for undo
    const auto& points = curve2D->GetCurvePoints();
    const auto& pt = points[idx];
    nlohmann::json oldPointData;
    oldPointData["x"] = pt.position.x;
    oldPointData["y"] = pt.position.y;
    oldPointData["ctrlInX"] = pt.controlIn.x;
    oldPointData["ctrlInY"] = pt.controlIn.y;
    oldPointData["ctrlOutX"] = pt.controlOut.x;
    oldPointData["ctrlOutY"] = pt.controlOut.y;
    oldPointData["useBezier"] = pt.useBezier;
    oldPointData["symmetricHandles"] = pt.symmetricHandles;

    // Move the control point
    curve2D->MoveControlPoint(controlPointId, math::Vec2(worldX, worldY));

    // Get new state
    const auto& newPoints = curve2D->GetCurvePoints();
    const auto& newPt = newPoints[idx];
    nlohmann::json newPointData;
    newPointData["x"] = newPt.position.x;
    newPointData["y"] = newPt.position.y;
    newPointData["ctrlInX"] = newPt.controlIn.x;
    newPointData["ctrlInY"] = newPt.controlIn.y;
    newPointData["ctrlOutX"] = newPt.controlOut.x;
    newPointData["ctrlOutY"] = newPt.controlOut.y;
    newPointData["useBezier"] = newPt.useBezier;
    newPointData["symmetricHandles"] = newPt.symmetricHandles;

    // Create and record command (already executed)
    auto cmd = std::make_shared<core::ModifyPointCommand>(component, pointIndex, oldPointData, newPointData);
    RecordCommand(cmd);
    MarkSceneDirty();
}

// ========================================================================
// Curve3D Point Editing (also works for Path3D)
// ========================================================================

namespace {

// World transform of the component owner (identity if not a Node3D).
math::Mat4 Curve3DOwnerWorld(const std::shared_ptr<core::Component>& component) {
    if (component && component->GetOwner()) {
        if (auto* node3D = dynamic_cast<Node3D*>(component->GetOwner())) {
            return node3D->GetGlobalTransformMatrix();
        }
    }
    return math::Mat4();
}

// Project a world-space point to screen pixels. Returns false if behind camera.
bool Curve3DWorldToScreen(lupine::RenderView* renderView, const math::Vec3& world,
                          float& outX, float& outY) {
    auto camera = renderView->getCamera();
    if (!camera) {
        return false;
    }
    const Viewport& viewport = renderView->getViewport();
    float vw = static_cast<float>(viewport.width);
    float vh = static_cast<float>(viewport.height);
    if (vw <= 0.0f || vh <= 0.0f) {
        return false;
    }
    float aspect = vw / vh;
    math::Mat4 viewProj = camera->getProjectionMatrix(aspect) * camera->getViewMatrix();
    math::Vec4 clip = viewProj * math::Vec4(world.x, world.y, world.z, 1.0f);
    if (clip.w <= 0.0001f) {
        return false;
    }
    float ndcX = clip.x / clip.w;
    float ndcY = clip.y / clip.w;
    outX = (ndcX * 0.5f + 0.5f) * vw + viewport.x;
    outY = (1.0f - (ndcY * 0.5f + 0.5f)) * vh + viewport.y;
    return true;
}

} // namespace

std::string EditorBridge::GetCurve3DPoints(std::shared_ptr<core::Component> component) {
    auto curve3D = std::dynamic_pointer_cast<components::Curve3D>(component);
    if (!curve3D) {
        return "[]";
    }

    nlohmann::json jsonArray = nlohmann::json::array();
    for (const auto& pt : curve3D->GetCurvePoints()) {
        nlohmann::json pointObj;
        pointObj["x"] = pt.position.x;
        pointObj["y"] = pt.position.y;
        pointObj["z"] = pt.position.z;
        pointObj["ctrlInX"] = pt.controlIn.x;
        pointObj["ctrlInY"] = pt.controlIn.y;
        pointObj["ctrlInZ"] = pt.controlIn.z;
        pointObj["ctrlOutX"] = pt.controlOut.x;
        pointObj["ctrlOutY"] = pt.controlOut.y;
        pointObj["ctrlOutZ"] = pt.controlOut.z;
        pointObj["useBezier"] = pt.useBezier;
        pointObj["symmetricHandles"] = pt.symmetricHandles;
        pointObj["tilt"] = pt.tilt;
        jsonArray.push_back(pointObj);
    }
    return jsonArray.dump();
}

void EditorBridge::AddCurve3DPoint(std::shared_ptr<core::Component> component, float x, float y, float z) {
    auto curve3D = std::dynamic_pointer_cast<components::Curve3D>(component);
    if (!curve3D) {
        return;
    }
    curve3D->AddPoint(math::Vec3(x, y, z));
    MarkSceneDirty();
}

void EditorBridge::UpdateCurve3DPoint(std::shared_ptr<core::Component> component, int index, float x, float y, float z) {
    auto curve3D = std::dynamic_pointer_cast<components::Curve3D>(component);
    if (!curve3D) {
        return;
    }
    curve3D->SetPointPosition(static_cast<size_t>(index), math::Vec3(x, y, z));
    MarkSceneDirty();
}

void EditorBridge::UpdateCurve3DPointBezier(std::shared_ptr<core::Component> component, int index,
                                            float ctrlInX, float ctrlInY, float ctrlInZ,
                                            float ctrlOutX, float ctrlOutY, float ctrlOutZ,
                                            bool symmetric) {
    auto curve3D = std::dynamic_pointer_cast<components::Curve3D>(component);
    if (!curve3D) {
        return;
    }
    curve3D->SetPointControlHandles(static_cast<size_t>(index),
                                    math::Vec3(ctrlInX, ctrlInY, ctrlInZ),
                                    math::Vec3(ctrlOutX, ctrlOutY, ctrlOutZ),
                                    symmetric);
    MarkSceneDirty();
}

void EditorBridge::SetCurve3DPointUseBezier(std::shared_ptr<core::Component> component, int index, bool useBezier) {
    auto curve3D = std::dynamic_pointer_cast<components::Curve3D>(component);
    if (!curve3D) {
        return;
    }
    curve3D->SetPointUseBezier(static_cast<size_t>(index), useBezier);
    MarkSceneDirty();
}

void EditorBridge::SetCurve3DPointTilt(std::shared_ptr<core::Component> component, int index, float tilt) {
    auto curve3D = std::dynamic_pointer_cast<components::Curve3D>(component);
    if (!curve3D) {
        return;
    }
    curve3D->SetPointTilt(static_cast<size_t>(index), tilt);
    MarkSceneDirty();
}

void EditorBridge::RemoveCurve3DPoint(std::shared_ptr<core::Component> component, int index) {
    auto curve3D = std::dynamic_pointer_cast<components::Curve3D>(component);
    if (!curve3D) {
        return;
    }
    curve3D->RemovePoint(static_cast<size_t>(index));
    MarkSceneDirty();
}

void EditorBridge::ClearCurve3DPoints(std::shared_ptr<core::Component> component) {
    auto curve3D = std::dynamic_pointer_cast<components::Curve3D>(component);
    if (!curve3D) {
        return;
    }
    curve3D->ClearPoints();
    MarkSceneDirty();
}

void EditorBridge::SetCurve3DSelectedPoint(std::shared_ptr<core::Component> component, int index) {
    auto curve3D = std::dynamic_pointer_cast<components::Curve3D>(component);
    if (!curve3D) {
        return;
    }
    curve3D->SetSelectedPointIndex(index);
}

void EditorBridge::SetCurve3DShowHandles(std::shared_ptr<core::Component> component, bool show) {
    auto curve3D = std::dynamic_pointer_cast<components::Curve3D>(component);
    if (!curve3D) {
        return;
    }
    curve3D->SetShowBezierHandles(show);
}

int EditorBridge::HitTestCurve3DControlPoint(std::shared_ptr<core::Component> component, RenderViewID viewID,
                                             float screenX, float screenY, float pixelRadius) {
    auto curve3D = std::dynamic_pointer_cast<components::Curve3D>(component);
    if (!curve3D || !m_RenderWorld) {
        return -1;
    }
    lupine::RenderView* renderView = m_RenderWorld->getRenderView(viewID);
    if (!renderView) {
        return -1;
    }

    math::Mat4 world = Curve3DOwnerWorld(component);
    const auto& points = curve3D->GetCurvePoints();
    int selected = curve3D->GetSelectedPointIndex();

    int bestId = -1;
    float bestDistSq = pixelRadius * pixelRadius;

    auto consider = [&](const math::Vec3& localPos, int id) {
        math::Vec3 worldPos = world.TransformPoint(localPos);
        float sx = 0.0f, sy = 0.0f;
        if (!Curve3DWorldToScreen(renderView, worldPos, sx, sy)) {
            return;
        }
        float dx = sx - screenX;
        float dy = sy - screenY;
        float distSq = dx * dx + dy * dy;
        if (distSq <= bestDistSq) {
            bestDistSq = distSq;
            bestId = id;
        }
    };

    // Selected point's bezier handles take priority (tested first, smaller wins on ties).
    if (selected >= 0 && selected < static_cast<int>(points.size())) {
        const auto& pt = points[selected];
        if (pt.useBezier) {
            consider(pt.position + pt.controlIn, selected * 10 + 1);
            consider(pt.position + pt.controlOut, selected * 10 + 2);
        }
    }

    // All anchors.
    for (size_t i = 0; i < points.size(); ++i) {
        consider(points[i].position, static_cast<int>(i) * 10 + 0);
    }

    return bestId;
}

void EditorBridge::MoveCurve3DControlPoint(std::shared_ptr<core::Component> component, int controlPointId,
                                           RenderViewID viewID, float screenX, float screenY) {
    auto curve3D = std::dynamic_pointer_cast<components::Curve3D>(component);
    if (!curve3D || !m_RenderWorld || controlPointId < 0) {
        return;
    }
    lupine::RenderView* renderView = m_RenderWorld->getRenderView(viewID);
    if (!renderView) {
        return;
    }
    auto camera = renderView->getCamera();
    if (!camera) {
        return;
    }

    int pointIndex = controlPointId / 10;
    int handleType = controlPointId % 10;
    size_t idx = static_cast<size_t>(pointIndex);
    if (idx >= curve3D->GetPointCount()) {
        return;
    }

    const components::Curve3DPoint pt = curve3D->GetCurvePoint(idx);
    math::Mat4 world = Curve3DOwnerWorld(component);

    // Current world position of the dragged control point.
    math::Vec3 localAnchor = pt.position;
    math::Vec3 localTarget = localAnchor;
    if (handleType == 1) localTarget = pt.position + pt.controlIn;
    else if (handleType == 2) localTarget = pt.position + pt.controlOut;
    math::Vec3 worldTarget = world.TransformPoint(localTarget);

    // Drag on the plane through the point facing the camera.
    math::Mat4 invView = camera->getViewMatrix().Inverse();
    math::Vec3 camForward = invView.TransformDirection(math::Vec3(0.0f, 0.0f, -1.0f)).Normalized();

    math::Vec3 newWorld = ScreenToWorldOnPlane(viewID, screenX, screenY,
                                               camForward.x, camForward.y, camForward.z,
                                               worldTarget.x, worldTarget.y, worldTarget.z);
    if (newWorld.x == 0.0f && newWorld.y == 0.0f && newWorld.z == 0.0f) {
        return; // parallel / behind-camera failure
    }

    math::Vec3 newLocal = world.Inverse().TransformPoint(newWorld);

    if (handleType == 0) {
        curve3D->SetPointPosition(idx, newLocal);
    } else if (handleType == 1) {
        math::Vec3 ctrlIn = newLocal - localAnchor;
        math::Vec3 ctrlOut = pt.symmetricHandles ? math::Vec3(-ctrlIn.x, -ctrlIn.y, -ctrlIn.z) : pt.controlOut;
        curve3D->SetPointControlHandles(idx, ctrlIn, ctrlOut, pt.symmetricHandles);
    } else if (handleType == 2) {
        math::Vec3 ctrlOut = newLocal - localAnchor;
        math::Vec3 ctrlIn = pt.symmetricHandles ? math::Vec3(-ctrlOut.x, -ctrlOut.y, -ctrlOut.z) : pt.controlIn;
        curve3D->SetPointControlHandles(idx, ctrlIn, ctrlOut, pt.symmetricHandles);
    }

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

// ========================================================================
// TileMap 2.5D Builder Implementation
// ========================================================================

uint64_t EditorBridge::CreateTileMap25DBuilder() {
    auto* builder = new TileMap25DBuilder();
    if (m_RenderWorld) {
        builder->InitializeRendering(m_RenderWorld.get());
    }
    return reinterpret_cast<uint64_t>(builder);
}

void EditorBridge::DestroyTileMap25DBuilder(uint64_t builderID) {
    auto* builder = reinterpret_cast<TileMap25DBuilder*>(builderID);
    if (builder) {
        delete builder;
    }
}

std::string EditorBridge::TileMap25DAddFace(uint64_t builderID,
    float v0x, float v0y, float v0z,
    float v1x, float v1y, float v1z,
    float v2x, float v2y, float v2z,
    float v3x, float v3y, float v3z,
    float uv0u, float uv0v,
    float uv1u, float uv1v,
    float uv2u, float uv2v,
    float uv3u, float uv3v,
    int32_t tilesetIndex, int32_t tileIndex,
    bool twoSided) {
    auto* builder = reinterpret_cast<TileMap25DBuilder*>(builderID);
    if (!builder) {
        return "";
    }

    TileFace25D face;
    face.vertices[0] = math::Vec3(v0x, v0y, v0z);
    face.vertices[1] = math::Vec3(v1x, v1y, v1z);
    face.vertices[2] = math::Vec3(v2x, v2y, v2z);
    face.vertices[3] = math::Vec3(v3x, v3y, v3z);

    face.uvs[0] = math::Vec2(uv0u, uv0v);
    face.uvs[1] = math::Vec2(uv1u, uv1v);
    face.uvs[2] = math::Vec2(uv2u, uv2v);
    face.uvs[3] = math::Vec2(uv3u, uv3v);

    face.tilesetIndex = tilesetIndex;
    face.tileIndex = tileIndex;
    face.twoSided = twoSided;

    return builder->AddFace(face);
}

void EditorBridge::TileMap25DRemoveFace(uint64_t builderID, const std::string& faceId) {
    auto* builder = reinterpret_cast<TileMap25DBuilder*>(builderID);
    if (builder) {
        builder->RemoveFace(faceId);
    }
}

void EditorBridge::TileMap25DClear(uint64_t builderID) {
    auto* builder = reinterpret_cast<TileMap25DBuilder*>(builderID);
    if (builder) {
        builder->ClearFaces();
    }
}

size_t EditorBridge::TileMap25DGetFaceCount(uint64_t builderID) {
    auto* builder = reinterpret_cast<TileMap25DBuilder*>(builderID);
    if (builder) {
        return builder->GetFaceCount();
    }
    return 0;
}

void EditorBridge::TileMap25DSetFaceVertex(uint64_t builderID, const std::string& faceId, int vertexIndex, float x, float y, float z) {
    auto* builder = reinterpret_cast<TileMap25DBuilder*>(builderID);
    if (builder) {
        builder->SetFaceVertex(faceId, vertexIndex, math::Vec3(x, y, z));
    }
}

void EditorBridge::TileMap25DRender(uint64_t builderID, RenderViewID viewID) {
    auto* builder = reinterpret_cast<TileMap25DBuilder*>(builderID);
    if (builder) {
        // Register the builder with this view for rendering
        auto& builders = m_TileMap25DBuilders[viewID];
        auto it = std::find(builders.begin(), builders.end(), builder);
        if (it == builders.end()) {
            builders.push_back(builder);
        }
    } else {
        // If builderID is 0/null, unregister all builders from this view
        m_TileMap25DBuilders.erase(viewID);
    }
}

void EditorBridge::RenderTileMap25D(RenderViewID viewID) {
    auto tmIt = m_TileMap25DBuilders.find(viewID);
    if (tmIt == m_TileMap25DBuilders.end() || tmIt->second.empty()) {
        return;
    }

    lupine::RenderView* renderView = m_RenderWorld->getRenderView(viewID);
    if (!renderView) {
        return;
    }

    for (TileMap25DBuilder* builder : tmIt->second) {
        if (!builder) {
            continue;
        }

        size_t faceCount = builder->GetFaceCount();

        // Get or regenerate mesh
        MeshHandle mesh = builder->GetMesh();

        if (!mesh.isValid()) {
            if (faceCount > 0) {
                // Mesh not valid but has faces - try to regenerate
                builder->RegenerateMesh();
                mesh = builder->GetMesh();
            }
            if (!mesh.isValid()) {
                continue;
            }
        }

        // Get material from the builder
        MaterialHandle material = builder->GetMaterial();

        // Add mesh to render view with identity transform
        renderView->addCustomDrawMesh(mesh, material, math::Mat4::Identity());
    }
}

std::string EditorBridge::TileMap25DToJSON(uint64_t builderID) {
    auto* builder = reinterpret_cast<TileMap25DBuilder*>(builderID);
    if (builder) {
        return builder->ToJSON();
    }
    return "{}";
}

bool EditorBridge::TileMap25DFromJSON(uint64_t builderID, const std::string& json) {
    auto* builder = reinterpret_cast<TileMap25DBuilder*>(builderID);
    if (builder) {
        return builder->FromJSON(json);
    }
    return false;
}

std::string EditorBridge::TileMap25DExportOBJ(uint64_t builderID, bool mergeVertices, bool useTextureAtlas) {
    auto* builder = reinterpret_cast<TileMap25DBuilder*>(builderID);
    if (builder) {
        return builder->ExportToOBJ(mergeVertices, useTextureAtlas);
    }
    return "";
}

std::string EditorBridge::TileMap25DExportMTL(uint64_t builderID, bool useTextureAtlas) {
    auto* builder = reinterpret_cast<TileMap25DBuilder*>(builderID);
    if (builder) {
        return builder->ExportToMTL(useTextureAtlas);
    }
    return "";
}

bool EditorBridge::TileMap25DLoadTilesetTexture(uint64_t builderID, int tilesetIndex) {
    auto* builder = reinterpret_cast<TileMap25DBuilder*>(builderID);
    if (builder) {
        return builder->LoadTilesetTexture(tilesetIndex);
    }
    return false;
}

bool EditorBridge::TileMap25DSetTexture(uint64_t builderID, const std::string& texturePath) {
    auto* builder = reinterpret_cast<TileMap25DBuilder*>(builderID);
    if (!builder || !m_RenderWorld) {
        return false;
    }

    IGfxDevice* device = m_RenderWorld->getDevice();
    if (!device) {
        return false;
    }

    // Load image from file using ImageAsset
    asset::ImageAsset imageAsset;
    if (!imageAsset.LoadFromFile(texturePath, true, asset::ImageColorSpace::sRGB)) {
        return false;
    }

    // Create the tileset texture with a full mip chain (when smooth filtering is
    // enabled) via the shared helper, which uploads the chain backend-correctly.
    TextureHandle texture = CreateTexture2DFromImage(device, imageAsset, TextureFormat::RGBA8_SRGB);
    if (!texture.isValid()) {
        return false;
    }

    builder->SetTexture(texture);
    return true;
}

void EditorBridge::TileMap25DUnregister(uint64_t builderID, RenderViewID viewID) {
    auto* builder = reinterpret_cast<TileMap25DBuilder*>(builderID);
    if (!builder) {
        return;
    }

    auto it = m_TileMap25DBuilders.find(viewID);
    if (it != m_TileMap25DBuilders.end()) {
        auto& builders = it->second;
        builders.erase(std::remove(builders.begin(), builders.end(), builder), builders.end());
        if (builders.empty()) {
            m_TileMap25DBuilders.erase(it);
        }
    }
}

void EditorBridge::DebugDrawLine(RenderViewID viewID, float x1, float y1, float z1, float x2, float y2, float z2, float r, float g, float b, float a, bool depthTest) {
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
            // Use duration of 0.02s (just over 1 frame at 60fps) so lines persist through the render loop
            debugRenderer->drawLine(start, end, color, 0.02f, depthTest);
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

