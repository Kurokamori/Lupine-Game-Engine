#include "lupine/runtime/RuntimeApp.hpp"
#include "lupine/rendering/ViewportUtils.hpp"
#include "lupine/runtime/SDL2InputAdapter.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/rendering/gfx/GfxDeviceFactory.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/rendering/gfx/GfxCommandList.hpp"
#include "lupine/platform/FileSystem.hpp"
#include "lupine/platform/Path.hpp"
#include "lupine/core/Core.hpp"
#include "lupine/core/CameraNodes.hpp"
#include "lupine/rendering/RenderCamera.hpp"
#include "lupine/components/Components.hpp"
#include <chrono>
#include <thread>

#ifdef _WIN32
#include <Windows.h>
#include <gl/GL.h>
#endif

namespace lupine {

RuntimeApp::RuntimeApp() = default;

RuntimeApp::~RuntimeApp() {

    stop();

    shutdown();
}

bool RuntimeApp::initialize(const Config& config) {
    m_config = config;

    Logger::Init("lupine_runtime.log", m_config.debugging);

    core::InitializeCore();
    core::RegisterBuiltInTypes();

    components::InitializeComponents();

    if (m_config.debugging) {
        Logger::SetLogLevel(spdlog::level::debug);

    } else {
        Logger::SetLogLevel(spdlog::level::info);

    }

    int windowWidth = config.windowWidth;
    int windowHeight = config.windowHeight;

    input::InputManager::Initialize();
    m_inputManager = &input::InputManager::Get();

    m_sceneManager = std::make_unique<core::SceneManager>();
    if (!m_sceneManager->Initialize()) {

        return false;
    }

    if (!m_config.projectPath.empty()) {
        if (!m_config.scenePath.empty()) {

            if (!m_sceneManager->LoadProjectWithScene(m_config.projectPath, m_config.scenePath)) {

                return false;
            }
        } else {

            if (!m_sceneManager->LoadProject(m_config.projectPath)) {

                return false;
            }
        }

        if (m_sceneManager->GetProject()) {
            const auto& settings = m_sceneManager->GetProject()->GetSettings();
            m_fixedDeltaTime = 1.0f / settings.physicsTickRate;

            windowWidth = settings.windowWidth;
            windowHeight = settings.windowHeight;

            SetLogicalCanvasSize(math::Vec2(
                static_cast<float>(settings.windowWidth),
                static_cast<float>(settings.windowHeight)
            ));
        }

        if (!m_config.scenePath.empty()) {
            m_currentScenePath = m_config.scenePath;
        } else if (m_sceneManager->GetCurrentScene()) {

            m_currentScenePath = "";
        }

        std::string projectDir = platform::Path::GetDirectory(m_config.projectPath);
        std::string inputMapPath = platform::Path::Join(projectDir, "input_map.json");

        if (platform::FileSystem::Exists(inputMapPath)) {

            if (!m_inputManager->LoadInputMap(inputMapPath)) {

            }
        } else {

            if (platform::FileSystem::Exists("default_input_map.json")) {

                m_inputManager->LoadInputMap("default_input_map.json");
            }
        }
    } else {

        if (platform::FileSystem::Exists("default_input_map.json")) {

            if (!m_inputManager->LoadInputMap("default_input_map.json")) {

            }
        } else {

        }
    }

    m_window = std::make_unique<Window>();
    Window::Config windowConfig;
    windowConfig.title = config.title;
    windowConfig.width = windowWidth;
    windowConfig.height = windowHeight;
    windowConfig.resizable = config.resizable;
    windowConfig.vsync = config.vsync;

    if (!m_window->create(windowConfig)) {

        return false;
    }

    m_inputManager->SetWindowSize(windowWidth, windowHeight);

    auto gfxDevice = GfxDeviceFactory::create(GraphicsBackend::OpenGL);
    if (!gfxDevice) {

        return false;
    }

    if (!gfxDevice->initialize()) {

        return false;
    }

    int width, height;
    m_window->getSize(width, height);

    m_inputManager->SetWindowSize(width, height);

    SwapchainDesc swapchainDesc;
    swapchainDesc.window.platformHandle = m_window->getNativeHandle();
    swapchainDesc.width = static_cast<uint32_t>(width);
    swapchainDesc.height = static_cast<uint32_t>(height);
    swapchainDesc.vsync = config.vsync;
    swapchainDesc.isolated = true;

    m_swapchain = gfxDevice->createSwapchain(swapchainDesc);
    if (!m_swapchain.isValid()) {

        return false;
    }

    m_renderWorld = std::make_unique<RenderWorld>();
    if (!m_renderWorld->initialize(std::move(gfxDevice))) {

        return false;
    }

    if (m_sceneManager && m_sceneManager->GetProject()) {
        const auto& settings = m_sceneManager->GetProject()->GetSettings();
        FilterMode minFilter = FilterMode::Linear;
        FilterMode magFilter = FilterMode::Linear;

        switch (settings.textureFiltering) {
            case core::ProjectSettings::TextureFiltering::NearestNeighbor:
                minFilter = FilterMode::Nearest;
                magFilter = FilterMode::Nearest;
                break;
            case core::ProjectSettings::TextureFiltering::Bilinear:
                minFilter = FilterMode::Linear;
                magFilter = FilterMode::Linear;
                break;
            case core::ProjectSettings::TextureFiltering::Cubic:

                minFilter = FilterMode::Linear;
                magFilter = FilterMode::Linear;

                break;
        }

        m_renderWorld->setTextureFiltering(minFilter, magFilter);

    }

    m_inputAdapter = std::make_unique<runtime::SDL2InputAdapter>();
    if (!m_inputAdapter->Initialize(m_inputManager)) {

        return false;
    }

    m_window->setEventCallback([this](const SDL_Event& event) {
        return m_inputAdapter->ProcessEvent(event);
    });

    m_sceneManager->SetInputManager(m_inputManager);

    m_initialized = true;
    m_running = true;

#ifdef _WIN32
    wglMakeCurrent(nullptr, nullptr);
#endif

    return true;
}

void RuntimeApp::run() {
    if (!m_initialized) {

        return;
    }

    m_lastFrameTime = std::chrono::high_resolution_clock::now();
    m_physicsAccumulator = 0.0f;

    while (m_running) {
        if (!runFrame()) {
            break;
        }
    }

}

void RuntimeApp::runAsync() {
    if (!m_initialized) {

        return;
    }

    if (m_runtimeThread && m_runtimeThread->joinable()) {

        return;
    }

    m_isAsync = true;
    m_runtimeThread = std::make_unique<std::thread>(&RuntimeApp::runInternal, this);
}

bool RuntimeApp::runFrame() {
    if (!m_initialized || !m_running) {
        return false;
    }

    auto currentTime = std::chrono::high_resolution_clock::now();
    float deltaTime = std::chrono::duration<float>(currentTime - m_lastFrameTime).count();
    m_lastFrameTime = currentTime;

    if (deltaTime > 0.25f) {
        deltaTime = 0.25f;
    }

    if (!m_isAsync) {
        if (!m_window->processEvents()) {
            m_running = false;
            return false;
        }
    }

    m_inputAdapter->PollInput();
    m_inputManager->Update(deltaTime);

    processInput(deltaTime);

    if (!m_paused) {

        update(deltaTime);

        m_physicsAccumulator += deltaTime;
        while (m_physicsAccumulator >= m_fixedDeltaTime) {
            physicsUpdate(m_fixedDeltaTime);
            m_physicsAccumulator -= m_fixedDeltaTime;
        }
    }

    render();

    return m_running;
}

bool RuntimeApp::processEvents() {
    if (!m_initialized || !m_running) {
        return false;
    }

    if (!m_window->processEvents()) {
        m_running = false;
        return false;
    }

    return m_running;
}

void RuntimeApp::runInternal() {

    auto device = m_renderWorld->getDevice();
    if (device) {
        auto backbuffer = device->getSwapchainBackbuffer(m_swapchain);

        auto cmdList = device->beginFrame(backbuffer);
        if (cmdList) {

            cmdList.reset();
        } else {

        }
    }

    m_lastFrameTime = std::chrono::high_resolution_clock::now();
    m_physicsAccumulator = 0.0f;

    while (m_running) {
        if (!runFrame()) {
            break;
        }
    }

    if (!m_running) {

        m_running = false;
    }
}

void RuntimeApp::stop() {

    bool wasRunning = m_running.exchange(false);

    if (wasRunning) {

    }

    if (m_runtimeThread && m_runtimeThread->joinable()) {
        m_runtimeThread->join();
        m_runtimeThread.reset();
    }
}

void RuntimeApp::pause() {
    if (!m_paused) {

        m_paused = true;
    }
}

void RuntimeApp::resume() {
    if (m_paused) {

        m_paused = false;
    }
}

void RuntimeApp::reloadScene() {
    if (!m_sceneManager) {

        return;
    }

    std::lock_guard<std::mutex> lock(m_stateMutex);

    if (!m_currentScenePath.empty()) {

        if (!m_sceneManager->LoadScene(m_currentScenePath)) {

        }
    } else if (!m_config.projectPath.empty()) {

        m_sceneManager->UnloadCurrentScene();
        if (!m_sceneManager->LoadProject(m_config.projectPath)) {

        }
    } else {

    }
}

bool RuntimeApp::loadScene(const std::string& scenePath) {
    if (!m_sceneManager) {

        return false;
    }

    std::lock_guard<std::mutex> lock(m_stateMutex);

    if (m_sceneManager->LoadScene(scenePath)) {
        m_currentScenePath = scenePath;
        return true;
    }

    return false;
}

void RuntimeApp::processInput(float deltaTime) {

    static bool debugEnabled = true;

    if (debugEnabled && m_inputManager) {

        if (m_inputManager->IsActionJustPressed("jump")) {

        }
        if (m_inputManager->IsActionPressed("jump")) {

        }
        if (m_inputManager->IsActionJustReleased("jump")) {

        }

        if (m_inputManager->IsActionJustPressed("attack")) {

        }

        if (m_inputManager->IsActionJustPressed("interact")) {

        }

        if (m_inputManager->IsActionJustPressed("ui_accept")) {

        }

        if (m_inputManager->IsActionJustPressed("ui_cancel")) {

        }

        float moveX = m_inputManager->GetAxisValue("move_horizontal");
        float moveY = m_inputManager->GetAxisValue("move_vertical");

        if (std::abs(moveX) > 0.01f || std::abs(moveY) > 0.01f) {

        }

        float lookX = m_inputManager->GetAxisValue("look_horizontal");
        float lookY = m_inputManager->GetAxisValue("look_vertical");

        if (std::abs(lookX) > 0.01f || std::abs(lookY) > 0.01f) {

        }

        float accelerate = m_inputManager->GetAxisValue("accelerate");
        if (accelerate > 0.01f) {

        }

        if (m_inputManager->IsKeyJustPressed(input::KeyCode::Space)) {

        }
        if (m_inputManager->IsKeyJustPressed(input::KeyCode::W)) {

        }
        if (m_inputManager->IsKeyJustPressed(input::KeyCode::A)) {

        }
        if (m_inputManager->IsKeyJustPressed(input::KeyCode::S)) {

        }
        if (m_inputManager->IsKeyJustPressed(input::KeyCode::D)) {

        }
        if (m_inputManager->IsKeyJustPressed(input::KeyCode::E)) {

        }

        if (m_inputManager->IsMouseButtonJustPressed(input::MouseButton::Left)) {

        }
        if (m_inputManager->IsMouseButtonJustPressed(input::MouseButton::Right)) {

        }

        auto scrollDelta = m_inputManager->GetMouseScrollDelta();
        if (std::abs(scrollDelta.y) > 0.01f) {

        }

        static int frameCount = 0;
        frameCount++;
        if (frameCount > 1800) {
            debugEnabled = false;

        }
    }

    if (m_sceneManager) {
        m_sceneManager->ProcessInput(deltaTime);
    }
}

void RuntimeApp::update(float deltaTime) {
    if (m_sceneManager) {
        m_sceneManager->Update(deltaTime);
    }
}

void RuntimeApp::physicsUpdate(float fixedDeltaTime) {
    if (m_sceneManager) {
        m_sceneManager->PhysicsUpdate(fixedDeltaTime);
    }
}

Viewport RuntimeApp::calculateViewport(int windowWidth, int windowHeight, int targetWidth, int targetHeight, core::ProjectSettings::ViewportScaleMode scaleMode) {
    Viewport viewport;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    float windowAspect = static_cast<float>(windowWidth) / static_cast<float>(windowHeight);
    float targetAspect = static_cast<float>(targetWidth) / static_cast<float>(targetHeight);

    switch (scaleMode) {
        case core::ProjectSettings::ViewportScaleMode::Letterbox: {

            if (windowAspect > targetAspect) {

                float scaledHeight = static_cast<float>(windowHeight);
                float scaledWidth = scaledHeight * targetAspect;
                viewport.x = (static_cast<float>(windowWidth) - scaledWidth) * 0.5f;
                viewport.y = 0.0f;
                viewport.width = scaledWidth;
                viewport.height = scaledHeight;
            } else {

                float scaledWidth = static_cast<float>(windowWidth);
                float scaledHeight = scaledWidth / targetAspect;
                viewport.x = 0.0f;
                viewport.y = (static_cast<float>(windowHeight) - scaledHeight) * 0.5f;
                viewport.width = scaledWidth;
                viewport.height = scaledHeight;
            }
            break;
        }
        case core::ProjectSettings::ViewportScaleMode::Stretch: {

            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(windowWidth);
            viewport.height = static_cast<float>(windowHeight);
            break;
        }
        case core::ProjectSettings::ViewportScaleMode::Crop: {

            if (windowAspect > targetAspect) {

                float scaledWidth = static_cast<float>(windowWidth);
                float scaledHeight = scaledWidth / targetAspect;
                viewport.x = 0.0f;
                viewport.y = (static_cast<float>(windowHeight) - scaledHeight) * 0.5f;
                viewport.width = scaledWidth;
                viewport.height = scaledHeight;
            } else {

                float scaledHeight = static_cast<float>(windowHeight);
                float scaledWidth = scaledHeight * targetAspect;
                viewport.x = (static_cast<float>(windowWidth) - scaledWidth) * 0.5f;
                viewport.y = 0.0f;
                viewport.width = scaledWidth;
                viewport.height = scaledHeight;
            }
            break;
        }
        case core::ProjectSettings::ViewportScaleMode::Ignore:
        default: {

            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(windowWidth);
            viewport.height = static_cast<float>(windowHeight);
            break;
        }
    }

    return viewport;
}

void RuntimeApp::render() {
    if (!m_sceneManager || !m_renderWorld) {
        return;
    }

    auto scene = m_sceneManager->GetCurrentScene();
    if (!scene) {
        return;
    }

    int width, height;
    m_window->getSize(width, height);
    float aspectRatio = static_cast<float>(width) / static_cast<float>(height);

    std::shared_ptr<core::Camera3D> camera3D = nullptr;
    std::shared_ptr<core::Camera2D> camera2D = nullptr;
    std::shared_ptr<core::CameraUI> cameraUI = nullptr;

    if (scene->GetRoot()) {
        findCamerasRecursive(scene->GetRoot(), camera3D, camera2D, cameraUI);
    }

    if (!camera3D && !camera2D && !cameraUI) {

        camera2D = std::make_shared<core::Camera2D>();
        camera2D->SetName("DefaultCamera2D");
        camera2D->SetActive(true);
        camera2D->SetPosition(math::Vec2(0.0f, 0.0f));

        camera3D = std::make_shared<core::Camera3D>();
        camera3D->SetName("DefaultCamera3D");
        camera3D->SetActive(true);
        camera3D->SetPosition(math::Vec3(0.0f, 5.0f, 10.0f));

        camera3D->SetRotation(math::Quat::FromEuler(math::Vec3(-25.0f, 0.0f, 0.0f)));

        cameraUI = std::make_shared<core::CameraUI>();
        cameraUI->SetName("DefaultCameraUI");
        cameraUI->SetActive(true);
    }

    bool isFirstCamera = true;

    if (camera3D && camera3D->IsActive()) {

        renderWithCamera(camera3D.get(), scene, aspectRatio, isFirstCamera);
        isFirstCamera = false;
    }

    if (camera2D && camera2D->IsActive()) {

        renderWithCamera(camera2D.get(), scene, aspectRatio, isFirstCamera);
        isFirstCamera = false;
    }

    if (cameraUI && cameraUI->IsActive()) {

        renderWithCamera(cameraUI.get(), scene, aspectRatio, isFirstCamera);
        isFirstCamera = false;
    }

    if (isFirstCamera) {

    }

    m_renderWorld->getDevice()->present(m_swapchain);
}

void RuntimeApp::shutdown() {
    if (!m_initialized) {
        return;
    }

    m_running = false;

    if (m_inputAdapter) {
        m_inputAdapter->Shutdown();
        m_inputAdapter.reset();
    }

    if (m_inputManager) {
        input::InputManager::Shutdown();
        m_inputManager = nullptr;
    }

    if (m_sceneManager) {
        m_sceneManager->Shutdown();
        m_sceneManager.reset();
    }

    if (m_swapchain.isValid() && m_renderWorld) {
        m_renderWorld->getDevice()->destroySwapchain(m_swapchain);
        m_swapchain = SwapchainHandle();
    }

    if (m_renderWorld) {
        m_renderWorld.reset();
    }

    if (m_window) {
        m_window->destroy();
        m_window.reset();
    }

    m_initialized = false;
    m_running = false;

}

void RuntimeApp::findCamerasRecursive(
    std::shared_ptr<core::Node> node,
    std::shared_ptr<core::Camera3D>& camera3D,
    std::shared_ptr<core::Camera2D>& camera2D,
    std::shared_ptr<core::CameraUI>& cameraUI) {

    if (!node) return;

    bool isUsable = node->IsActiveInHierarchy() && node->IsVisibleInHierarchy();

    if (isUsable && !camera3D) {
        auto cam3D = std::dynamic_pointer_cast<core::Camera3D>(node);
        if (cam3D && cam3D->IsActive()) {
            camera3D = cam3D;
        }
    }
    if (isUsable && !camera2D) {
        auto cam2D = std::dynamic_pointer_cast<core::Camera2D>(node);
        if (cam2D && cam2D->IsActive()) {
            camera2D = cam2D;
        }
    }
    if (isUsable && !cameraUI) {
        auto camUI = std::dynamic_pointer_cast<core::CameraUI>(node);
        if (camUI && camUI->IsActive()) {
            cameraUI = camUI;
        }
    }

    for (const auto& child : node->GetChildren()) {
        findCamerasRecursive(child, camera3D, camera2D, cameraUI);

        if (camera3D && camera2D && cameraUI) {
            return;
        }
    }
}

void RuntimeApp::renderWithCamera(
    core::Node* cameraNode,
    core::Scene* scene,
    float aspectRatio,
    bool isFirstCamera) {

    if (!cameraNode || !scene || !m_renderWorld) return;

    std::unique_ptr<RenderCamera> renderCamera;

    if (auto* cam3D = dynamic_cast<core::Camera3D*>(cameraNode)) {
        auto camera = std::make_unique<Camera3D>();

        math::Vec3 pos = cam3D->GetGlobalPosition();
        math::Quat rot = cam3D->GetGlobalRotation();

        camera->position = pos;

        math::Vec3 forward = rot * math::Vec3(0.0f, 0.0f, -1.0f);
        math::Vec3 up = rot * math::Vec3(0.0f, 1.0f, 0.0f);
        camera->target = pos + forward;
        camera->up = up;

        camera->projectionType = (cam3D->GetProjectionType() == core::Camera3D::ProjectionType::Perspective)
            ? ProjectionType::Perspective
            : ProjectionType::Orthographic;
        camera->fov = cam3D->GetFOV();
        camera->nearPlane = cam3D->GetNearPlane();
        camera->farPlane = cam3D->GetFarPlane();
        camera->orthoSize = cam3D->GetOrthoSize();

        if (isFirstCamera) {
            camera->clearFlags = CameraClearFlags::All;
            if (m_sceneManager->GetProject()) {
                camera->clearColor = m_sceneManager->GetProject()->GetSettings().clearColor;
            }
        } else {
            camera->clearFlags = CameraClearFlags::Depth;
        }

        renderCamera = std::move(camera);

    } else if (auto* cam2D = dynamic_cast<core::Camera2D*>(cameraNode)) {
        auto camera = std::make_unique<Camera2D>();

        math::Vec2 pos = cam2D->GetGlobalPosition();
        float rot = cam2D->GetGlobalRotation();

        camera->position = pos;
        camera->rotation = rot;
        camera->zoom = cam2D->GetZoom();
        camera->orthoSize = cam2D->GetOrthoSize();

        if (isFirstCamera) {
            camera->clearFlags = CameraClearFlags::All;
            if (m_sceneManager->GetProject()) {
                camera->clearColor = m_sceneManager->GetProject()->GetSettings().clearColor;
            }
        } else {
            camera->clearFlags = CameraClearFlags::Depth;
        }

        renderCamera = std::move(camera);

    } else if (auto* camUI = dynamic_cast<core::CameraUI*>(cameraNode)) {
        auto camera = std::make_unique<CameraCanvas>();

        if (m_sceneManager->GetProject()) {
            const auto& settings = m_sceneManager->GetProject()->GetSettings();
            camera->canvasSize = math::Vec2(
                static_cast<float>(settings.windowWidth),
                static_cast<float>(settings.windowHeight)
            );
        } else {

            int width, height;
            m_window->getSize(width, height);
            camera->canvasSize = math::Vec2(static_cast<float>(width), static_cast<float>(height));
        }

        if (isFirstCamera) {
            camera->clearFlags = CameraClearFlags::All;
            if (m_sceneManager->GetProject()) {
                camera->clearColor = m_sceneManager->GetProject()->GetSettings().clearColor;
            }
        } else {
            camera->clearFlags = CameraClearFlags::Depth;
        }

        renderCamera = std::move(camera);
    }

    if (!renderCamera) return;

    auto device = m_renderWorld->getDevice();
    if (!device) return;

    auto backbuffer = device->getSwapchainBackbuffer(m_swapchain);
    auto cmdList = device->beginFrame(backbuffer);
    if (!cmdList) {

        return;
    }

    RenderViewID viewID = m_renderWorld->createRenderView(std::move(renderCamera));
    if (viewID == 0) {

        return;
    }

    RenderView* view = m_renderWorld->getRenderView(viewID);
    if (!view) {
        m_renderWorld->destroyRenderView(viewID);
        return;
    }

    view->setSwapchain(m_swapchain);
    view->setScene(scene);

    view->setDebugRenderingEnabled(false);

    int width, height;
    m_window->getSize(width, height);

    Viewport viewport;
    if (m_sceneManager && m_sceneManager->GetProject()) {
        const auto& settings = m_sceneManager->GetProject()->GetSettings();
        viewport = calculateViewport(width, height, settings.windowWidth, settings.windowHeight, settings.viewportScaleMode);
    } else {

        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(width);
        viewport.height = static_cast<float>(height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
    }
    view->setViewport(viewport);

    SetCurrentViewport(viewport);

    m_renderWorld->renderView(viewID);

    m_renderWorld->destroyRenderView(viewID);
}

}
