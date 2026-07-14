#include "lupine/runtime/RuntimeApp.hpp"
#include "lupine/profiling/Profiler.hpp"
#include "lupine/runtime/SplashScreenNode.hpp"
#include "lupine/rendering/ViewportUtils.hpp"
#include "lupine/runtime/SDL2InputAdapter.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/rendering/gfx/GfxDeviceFactory.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/rendering/gfx/GfxCommandList.hpp"
#include "lupine/rendering/TextureCache.hpp"
#include "lupine/rendering/FontBaker.hpp"
#include "lupine/asset/ImageAsset.hpp"
#include "lupine/asset/AssetDatabase.hpp"
#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include <nlohmann/json.hpp>
#include "lupine/platform/FileSystem.hpp"
#include "lupine/platform/Path.hpp"
#include "lupine/platform/VirtualFileSystem.hpp"
#include "lupine/platform/PackFile.hpp"
#include "lupine/platform/UserDataStore.hpp"
#include "lupine/platform/DisplayServer.hpp"
#include "lupine/core/Core.hpp"
#include "lupine/core/CameraNodes.hpp"
#include "lupine/core/ExtensionManager.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/core/ScriptComponent.hpp"
#include "lupine/core/ArchetypeRegistry.hpp"
#include "lupine/core/ArchetypeRuntime.hpp"
#include "lupine/scripting/ScriptAPI.hpp"
#include "lupine/rendering/RenderCamera.hpp"
#include "lupine/components/Components.hpp"
#include <chrono>
#include <thread>
#include <deque>
#include <utility>
#include <vector>
#include <cstring>
#include <cctype>
#include <spdlog/sinks/base_sink.h>

#ifdef _WIN32
#include <Windows.h>
#include <gl/GL.h>
// Windows.h defines CreateDirectory as a macro aliasing CreateDirectoryA,
// which would rewrite calls to platform::FileSystem::CreateDirectory. Drop it.
#undef CreateDirectory
#endif

namespace lupine {

namespace {

std::mutex g_RuntimeLogMutex;
std::deque<std::pair<std::string, std::string>> g_RuntimeLogEntries;
constexpr size_t kMaxRuntimeLogEntries = 10000;
bool g_RuntimeLogCaptureInstalled = false;

/**
 * spdlog sink that buffers (level, message) pairs for the editor to drain.
 */
template<typename Mutex>
class RuntimeLogSink : public spdlog::sinks::base_sink<Mutex> {
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::string_view_t levelView = spdlog::level::to_string_view(msg.level);
        std::string level(levelView.data(), levelView.size());
        std::string text(msg.payload.data(), msg.payload.size());

        std::lock_guard<std::mutex> lock(g_RuntimeLogMutex);
        g_RuntimeLogEntries.emplace_back(std::move(level), std::move(text));
        if (g_RuntimeLogEntries.size() > kMaxRuntimeLogEntries) {
            g_RuntimeLogEntries.pop_front();
        }
    }

    void flush_() override {}
};

} // namespace

void InstallRuntimeLogCapture() {
    if (g_RuntimeLogCaptureInstalled) {
        return;
    }
    std::shared_ptr<spdlog::logger>& logger = Logger::GetLogger();
    if (logger) {
        logger->sinks().push_back(std::make_shared<RuntimeLogSink<std::mutex>>());
        g_RuntimeLogCaptureInstalled = true;
    }
}

std::vector<std::pair<std::string, std::string>> DrainRuntimeLogMessages() {
    std::lock_guard<std::mutex> lock(g_RuntimeLogMutex);
    std::vector<std::pair<std::string, std::string>> result(
        g_RuntimeLogEntries.begin(), g_RuntimeLogEntries.end());
    g_RuntimeLogEntries.clear();
    return result;
}

RuntimeApp::RuntimeApp() = default;

RuntimeApp::~RuntimeApp() {

    stop();

    shutdown();
}

bool RuntimeApp::initialize(const Config& config) {
    if (m_initialized) {
        return false;  // Already initialized
    }

    m_config = config;

    m_targetFrameRate = m_config.targetFrameRate;
    if (m_config.physicsTickRate > 0.0f) {
        m_fixedDeltaTime = 1.0f / m_config.physicsTickRate;
    }

    Logger::Init("lupine_runtime.log", m_config.debugging);
    InstallRuntimeLogCapture();

    Logger::Flush();

    // Publish the runtime arguments before any scene (and therefore any script
    // autoload or _ready) runs, so games can branch on their launch role.
    core::SetCommandLineArgs(m_config.args);

    core::InitializeCore();

    // Re-point user:// at the per-instance directory when one was requested.
    // InitializeCore() mounts user:// to the platform default; the editor's
    // multi-instance play overrides it so each instance keeps an isolated
    // user-data folder (saves, configs, networking state).
    if (!m_config.userPath.empty() &&
        !platform::PackFileSystem::Instance().isPackMode()) {
        if (!platform::FileSystem::Exists(m_config.userPath)) {
            platform::FileSystem::CreateDirectory(m_config.userPath, true);
        }
        auto& vfs = platform::VirtualFileSystem::GetInstance();
        if (!vfs.Mount("user://", m_config.userPath, false)) {
            LOG_ERROR(LogCategory::Core,
                      "RuntimeApp: failed to mount user:// at '{}'", m_config.userPath);
        } else {
            LOG_INFO(LogCategory::Core,
                     "RuntimeApp: user:// -> '{}'", m_config.userPath);
        }
    }

    Logger::Flush();

    core::RegisterBuiltInTypes();

    components::InitializeComponents();

    if (m_config.debugging) {
        Logger::SetLogLevel(spdlog::level::debug);

    } else {
        Logger::SetLogLevel(spdlog::level::info);

    }

    // Design resolution / logical canvas (drives UI layout and the render canvas).
    int windowWidth = config.windowWidth;
    int windowHeight = config.windowHeight;

    // Physical size the OS window actually opens at. Defaults to the design
    // resolution; a project (or config) window-size override decouples the two so
    // the window can be a different size than the canvas it renders.
    int actualWindowWidth = config.windowWidth;
    int actualWindowHeight = config.windowHeight;

    input::InputManager::Initialize();
    m_inputManager = &input::InputManager::Get();

    m_sceneManager = std::make_unique<core::SceneManager>();
    if (!m_sceneManager->Initialize()) {

        return false;
    }
    m_sceneManager->ClearQuitRequest();

    if (!m_config.projectPath.empty()) {
        // The runtime mounts res:// to <cwd>/resources during InitializeCore() before the
        // project path is known. Re-point res:// at the project root now so that VFS reads
        // (Lupine.read_text, the script module loader's import(), etc.) resolve project files.
        if (!platform::PackFileSystem::Instance().isPackMode()) {
            std::string resourceRoot = platform::Path::GetDirectory(m_config.projectPath);
            if (!resourceRoot.empty()) {
                auto& vfs = platform::VirtualFileSystem::GetInstance();
                if (!vfs.Mount("res://", resourceRoot, false)) {
                    LOG_ERROR(LogCategory::Core, "RuntimeApp: failed to mount res:// at project root '{}'", resourceRoot);
                    return false;
                }
            }
        }

        // Load native C++ extensions (.lupineext) before any scene is loaded so
        // their custom component types are registered with the TypeRegistry and
        // can be instantiated during scene deserialization.
        {
            std::string projectDir = platform::Path::GetDirectory(m_config.projectPath);
            if (!projectDir.empty()) {
                int loadedExtensions =
                    core::ExtensionManager::GetInstance().LoadExtensionsForProject(projectDir);
                if (loadedExtensions > 0) {
                    LOG_INFO(LogCategory::Core, "RuntimeApp: loaded {} native extension(s)", loadedExtensions);
                }
            }
        }

        if (!m_config.scenePath.empty()) {
            // Specific scene requested, skip splash screens
            if (!m_sceneManager->LoadProjectWithScene(m_config.projectPath, m_config.scenePath)) {

                return false;
            }
        } else {
            // Use splash screen support (will show splash screens if enabled, then main scene)
            if (!m_sceneManager->LoadProjectWithSplashScreens(m_config.projectPath,
                    static_cast<float>(windowWidth), static_cast<float>(windowHeight))) {

                return false;
            }

            // If splash screens are being shown, create and add the SplashScreenNode
            if (m_sceneManager->IsShowingSplashScreens()) {
                setupSplashScreenNode(static_cast<float>(windowWidth), static_cast<float>(windowHeight));
            }
        }

        if (m_sceneManager->GetProject()) {
            const auto& settings = m_sceneManager->GetProject()->GetSettings();
            if (settings.physicsTickRate > 0.0f) {
                m_fixedDeltaTime = 1.0f / settings.physicsTickRate;
            }

            windowWidth = settings.windowWidth;
            windowHeight = settings.windowHeight;

            // The physical window opens at the override size when enabled, else at
            // the design resolution. Layout/canvas keep using the design resolution
            // (windowWidth/windowHeight) regardless; only the OS window differs.
            if (settings.windowSizeOverride &&
                settings.windowSizeOverrideWidth > 0 &&
                settings.windowSizeOverrideHeight > 0) {
                actualWindowWidth = settings.windowSizeOverrideWidth;
                actualWindowHeight = settings.windowSizeOverrideHeight;
            } else {
                actualWindowWidth = settings.windowWidth;
                actualWindowHeight = settings.windowHeight;
            }

            // Window mode and frame pacing come from the project, not the
            // hardcoded launcher defaults, so the editor's settings are honored.
            m_config.fullscreen = settings.fullscreen;
            m_config.vsync = settings.vsync;
            m_config.resizable = settings.windowResizable;
            m_config.borderless = settings.windowBorderless;
            m_targetFrameRate = settings.targetFrameRate;
            if (!settings.projectName.empty()) {
                m_config.title = settings.projectName;
            }

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
        // No project loaded (export template mode) - set logical canvas size from config

        SetLogicalCanvasSize(math::Vec2(
            static_cast<float>(config.windowWidth),
            static_cast<float>(config.windowHeight)
        ));

        if (config.windowSizeOverride &&
            config.windowSizeOverrideWidth > 0 &&
            config.windowSizeOverrideHeight > 0) {
            actualWindowWidth = config.windowSizeOverrideWidth;
            actualWindowHeight = config.windowSizeOverrideHeight;
        } else {
            actualWindowWidth = config.windowWidth;
            actualWindowHeight = config.windowHeight;
        }

        if (platform::FileSystem::Exists("default_input_map.json")) {

            if (!m_inputManager->LoadInputMap("default_input_map.json")) {

            }
        } else {

        }
    }

    m_window = std::make_unique<Window>();
    Window::Config windowConfig;
    windowConfig.title = m_config.title;
    windowConfig.width = actualWindowWidth;
    windowConfig.height = actualWindowHeight;
    windowConfig.resizable = m_config.resizable;
    windowConfig.vsync = m_config.vsync;
    windowConfig.fullscreen = m_config.fullscreen;
    windowConfig.borderless = m_config.borderless;

    Logger::Flush();

    if (!m_window->create(windowConfig)) {

        return false;
    }

    Logger::Flush();

    applyWindowIcon();

    m_inputManager->SetWindowSize(actualWindowWidth, actualWindowHeight);

    Logger::Flush();

    auto gfxDevice = GfxDeviceFactory::create(config.backend);
    if (!gfxDevice) {
        
        return false;
    }

    Logger::Flush();

    if (!gfxDevice->initialize()) {
        
        return false;
    }

    Logger::Flush();

    int width, height;
    m_window->getSize(width, height);

    m_inputManager->SetWindowSize(width, height);

    Logger::Flush();

    SwapchainDesc swapchainDesc;
    swapchainDesc.window.platformHandle = m_window->getNativeHandle();

    Logger::Flush();

    swapchainDesc.width = static_cast<uint32_t>(width);
    swapchainDesc.height = static_cast<uint32_t>(height);
    swapchainDesc.vsync = m_config.vsync;
    swapchainDesc.isolated = true;

    Logger::Flush();

    m_swapchain = gfxDevice->createSwapchain(swapchainDesc);
    if (!m_swapchain.isValid()) {

        return false;
    }

    // Track initial swapchain size for resize detection
    m_lastSwapchainWidth = width;
    m_lastSwapchainHeight = height;

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

        // Drives both the per-texture sampler filter and (via setTextureFiltering)
        // whether 2D content textures are given a mip chain: Nearest = crisp,
        // mip-free pixel-art mode; Linear = smooth mipmapped downscaling.
        m_renderWorld->setTextureFiltering(minFilter, magFilter);

    }

    m_inputAdapter = std::make_unique<runtime::SDL2InputAdapter>();
    if (!m_inputAdapter->Initialize(m_inputManager)) {

        return false;
    }

    // Install SDL clipboard backend so UI text fields can copy/paste without
    // core depending on SDL directly.
    m_inputManager->SetClipboardProvider(
        []() -> std::string {
            char* text = SDL_GetClipboardText();
            std::string result = text ? text : "";
            if (text) {
                SDL_free(text);
            }
            return result;
        },
        [](const std::string& text) {
            SDL_SetClipboardText(text.c_str());
        });

    // Install the window/display control backend so scripts can drive the window
    // (title, fullscreen, vsync, mouse mode, ...) without core depending on SDL.
    installDisplayProvider();

    // Set initial DPI scale for high-DPI display support
    float dpiScale = m_window->getDPIScale();
    m_inputManager->SetDPIScale(dpiScale);

    // For desktop, content scale is typically 1.0 since we render at window size
    // This may change if we implement letterboxing or fixed aspect ratio rendering
    m_inputManager->SetContentScale(1.0f, 1.0f);

    m_window->setEventCallback([this](const SDL_Event& event) {
        // Handle resize events to update content scale
        if (event.type == SDL_WINDOWEVENT &&
            (event.window.event == SDL_WINDOWEVENT_RESIZED ||
             event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)) {
            // Update DPI scale (may change if window moved to different monitor)
            float dpiScale = m_window->getDPIScale();
            m_inputManager->SetDPIScale(dpiScale);
        }
        return m_inputAdapter->ProcessEvent(event);
    });

    m_sceneManager->SetInputManager(m_inputManager);

    // Initialize viewport BEFORE first frame to ensure mouse coordinates work correctly
    // This must happen after window and project are set up but before any input processing
    updateWindowAndViewport();

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

    // Ensure graphics context is current before the main loop starts.
    // This is critical because initialize() may end with the context not current,
    // and the first frame's buildDrawCommands() may create GPU resources (meshes, buffers).
    // Without a current context, buffer creation fails silently on some backends.
    if (m_renderWorld && m_renderWorld->getDevice()) {
        m_renderWorld->getDevice()->makeContextCurrent(m_swapchain);
    }

    m_lastFrameTime = std::chrono::high_resolution_clock::now();
    m_physicsAccumulator = 0.0f;
    m_frameDeadline = std::chrono::high_resolution_clock::time_point{};

    while (m_running) {
        if (!runFrame()) {
            break;
        }
        limitFrameRate();
    }

}

void RuntimeApp::limitFrameRate() {
    // vsync already paces the frame when enabled; only apply the software cap
    // when vsync is off and a positive target rate is configured.
    if (m_targetFrameRate <= 0 || m_config.vsync) {
        m_frameDeadline = std::chrono::high_resolution_clock::time_point{};
        return;
    }

    const auto frameDuration = std::chrono::duration_cast<std::chrono::high_resolution_clock::duration>(
        std::chrono::duration<double>(1.0 / static_cast<double>(m_targetFrameRate)));

    auto now = std::chrono::high_resolution_clock::now();
    if (m_frameDeadline.time_since_epoch().count() == 0) {
        m_frameDeadline = now + frameDuration;
        return;
    }

    if (now < m_frameDeadline) {
        std::this_thread::sleep_for(m_frameDeadline - now);
    }

    m_frameDeadline += frameDuration;

    // If we fell far behind (e.g. a long stall), resync to avoid a burst of
    // catch-up frames with zero delay.
    auto after = std::chrono::high_resolution_clock::now();
    if (m_frameDeadline < after) {
        m_frameDeadline = after + frameDuration;
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
    static int frameCount = 0;
    frameCount++;

    if (!m_initialized || !m_running) {
        return false;
    }

    // Serialize the whole frame against the editor-facing scene mutators
    // (loadScene/reloadScene/reloadScripts/applyLiveEdits). In async play mode
    // those arrive on the editor IPC (main) thread while this frame runs on the
    // runtime thread; without the frame holding the same mutex they could unload
    // or rewrite the scene graph mid-update. The deferred scene change below uses
    // loadSceneInternal() so it does not re-acquire this lock.
    std::lock_guard<std::recursive_mutex> frameLock(m_stateMutex);

    // Check if splash screens completed (deferred from callback to avoid scene unload during update)
    if (m_splashScreensCompleted.exchange(false)) {

        // Check if this is manual mode (template) or automatic mode (project)
        if (m_manualSplashCallback) {
            // Manual mode - call the stored callback
            auto callback = std::move(m_manualSplashCallback);
            m_manualSplashCallback = nullptr;
            callback();
        } else if (m_sceneManager) {
            // Automatic mode - let scene manager handle it
            m_sceneManager->SkipSplashScreens();
        }
    }

    if (frameCount <= 5) {

    }

    auto currentTime = std::chrono::high_resolution_clock::now();
    float deltaTime = std::chrono::duration<float>(currentTime - m_lastFrameTime).count();
    m_lastFrameTime = currentTime;

    if (deltaTime > 0.25f) {
        deltaTime = 0.25f;
    }

    ::lupine::profiling::Profiler::Get().BeginFrame(static_cast<uint64_t>(frameCount));

    // Snapshot the previous-frame input state (button/key edges, scroll delta, typed
    // text) BEFORE the platform pumps this frame's events. processEvents() below is the
    // real event pump: Window::processEvents() dispatches SDL events to the input
    // adapter, which writes the current button/scroll/text state. Snapshotting after
    // that pump (as before) left previous == current, so Is*JustPressed/JustReleased and
    // GetMouseScrollDelta always read empty and edge-triggered widgets (dropdowns,
    // sliders, scrollbar drag, spin boxes) never fired; only held-state input worked.
    {
        static bool s_diagLoggedInputOrder = false;
        if (!s_diagLoggedInputOrder) {
            s_diagLoggedInputOrder = true;
            LOG_INPUT_INFO("[diag] runFrame input order live: BeginFrame -> processEvents -> PollInput -> Update");
        }
    }
    m_inputManager->BeginFrame();

    if (!m_isAsync) {
        if (!m_window->processEvents()) {
            m_running = false;
            return false;
        }
    }

    // Check running state after event processing (may have been set to false)
    if (!m_running) {
        return false;
    }

    // Continuously-sampled state (mouse position, gamepad axes) is polled after the
    // event pump; Update() then finalizes the mouse delta and refreshes action/axis
    // states and promotes this frame's events for the scene to read.
    m_inputAdapter->PollInput();
    m_inputManager->Update(deltaTime);

    // CRITICAL: Update window size and viewport BEFORE processing input
    // This ensures mouse coordinate transformations use correct viewport dimensions
    updateWindowAndViewport();

    processInput(deltaTime);

    // While paused, a single queued step request lets the editor advance exactly
    // one update+physics frame; it is consumed here so the next frame re-freezes.
    const bool doStep = m_paused && m_stepRequested.exchange(false);
    if (!m_paused || doStep) {
        if (frameCount <= 5) {

        }

        // Apply the scene's global time scale to gameplay + physics (slow-mo /
        // fast-forward / freeze). Input above is intentionally left at real time.
        float timeScale = m_sceneManager ? m_sceneManager->GetTimeScale() : 1.0f;
        float scaledDelta = deltaTime * timeScale;

        {
            LUPINE_PROFILE_ZONE("Update", ::lupine::profiling::ZoneCategory::Update);
            update(scaledDelta);
        }

        m_physicsAccumulator += scaledDelta;
        if (m_physicsAccumulator >= m_fixedDeltaTime) {
            LUPINE_PROFILE_ZONE("Physics", ::lupine::profiling::ZoneCategory::Physics);
            while (m_physicsAccumulator >= m_fixedDeltaTime) {
                physicsUpdate(m_fixedDeltaTime);
                m_physicsAccumulator -= m_fixedDeltaTime;
            }
        }
    }

    // Honor a script/C-API quit request raised during input/update this frame. The
    // request lives on the scene manager (core) so the loop can poll it without
    // core depending on the runtime layer.
    if (m_sceneManager && m_sceneManager->IsQuitRequested()) {
        m_running = false;
        return false;
    }

    // Honor a deferred scene change requested by a script (change_scene) during
    // this frame's update. Performed here, after the update/physics pump has fully
    // unwound, so unloading the outgoing scene never frees state still on the call
    // stack (which is what made a synchronous switch from a script crash).
    if (m_sceneManager && m_sceneManager->HasPendingSceneChange()) {
        std::string nextScene = m_sceneManager->TakePendingSceneChange();
        if (!nextScene.empty()) {
            LOG_INFO(LogCategory::Core, "RuntimeApp: deferred scene change -> '{}'", nextScene);
            loadSceneInternal(nextScene);
        }
    }

    // Check running state before render (which can block on GPU resources)
    if (!m_running) {
        return false;
    }

    if (frameCount <= 5) {
        
        Logger::Flush();
    }
    {
        LUPINE_PROFILE_ZONE("Render", ::lupine::profiling::ZoneCategory::Render);
        render();
    }
    if (frameCount <= 5) {

        Logger::Flush();
    }

    ::lupine::profiling::Profiler::Get().EndFrame();

    // Push any user:// changes made this frame to durable storage. This is a no-op
    // unless something actually wrote, and on web it kicks off an asynchronous IndexedDB
    // write rather than blocking the frame.
    platform::UserDataStore::GetInstance().Flush();

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
    // Ensure graphics context is current before the main loop starts.
    // This is critical because initialize() may end with the context not current,
    // and the first frame's buildDrawCommands() may create GPU resources (meshes, buffers).
    // Without a current context, buffer creation fails silently on some backends.
    if (m_renderWorld && m_renderWorld->getDevice()) {
        m_renderWorld->getDevice()->makeContextCurrent(m_swapchain);
    }

    m_lastFrameTime = std::chrono::high_resolution_clock::now();
    m_physicsAccumulator = 0.0f;
    m_frameDeadline = std::chrono::high_resolution_clock::time_point{};

    while (m_running) {
        if (!runFrame()) {
            break;
        }
        limitFrameRate();
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

void RuntimeApp::step() {
    // Only meaningful while paused; runFrame() consumes the flag for one frame.
    if (m_paused) {
        m_stepRequested = true;
    }
}

namespace {

// Depth-first gather of every ScriptComponent under a node (inclusive).
void CollectScriptComponents(const std::shared_ptr<core::Node>& node,
                             std::vector<core::ScriptComponent*>& out) {
    if (!node) {
        return;
    }
    for (const std::shared_ptr<core::Component>& component : node->GetComponents()) {
        if (auto* script = dynamic_cast<core::ScriptComponent*>(component.get())) {
            out.push_back(script);
        }
    }
    for (const std::shared_ptr<core::Node>& child : node->GetChildren()) {
        CollectScriptComponents(child, out);
    }
}

} // namespace

void RuntimeApp::reloadScripts() {
    if (!m_sceneManager) {
        return;
    }

    // Guard against the runtime thread mutating the scene while we walk it,
    // mirroring reloadScene()/loadScene().
    std::lock_guard<std::recursive_mutex> lock(m_stateMutex);

    core::Scene* scene = m_sceneManager->GetCurrentScene();
    if (!scene) {
        return;
    }
    std::shared_ptr<core::Node> root = scene->GetRoot();
    if (!root) {
        return;
    }

    std::vector<core::ScriptComponent*> scripts;
    CollectScriptComponents(root, scripts);

    // Reload each script's source in place, then re-run its lifecycle so the new
    // code's setup actually executes (ReloadScript only reloads the source). The
    // node graph and component instances are preserved; only script-side state
    // is rebuilt, which is the expected semantics of a script hot-reload.
    for (core::ScriptComponent* script : scripts) {
        if (script->ReloadScript()) {
            script->OnAwake();
            script->OnReady();
        }
    }

    LOG_INFO(LogCategory::Core, "RuntimeApp: hot-reloaded {} script component(s)", scripts.size());
}

void RuntimeApp::reloadScene() {
    if (!m_sceneManager) {

        return;
    }

    std::lock_guard<std::recursive_mutex> lock(m_stateMutex);

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

    std::lock_guard<std::recursive_mutex> lock(m_stateMutex);

    return loadSceneInternal(scenePath);
}

bool RuntimeApp::loadSceneInternal(const std::string& scenePath) {
    if (!m_sceneManager) {
        return false;
    }

    if (m_sceneManager->LoadScene(scenePath)) {
        m_currentScenePath = scenePath;
        return true;
    }

    return false;
}

namespace {

// Apply a single node property edit in place, mirroring
// EditorBridge::SetNodePropertyDirect so the runtime and editor share one
// deserialize-from-JSON code path for a node's own registered properties
// (transform, camera params, ...).
void ApplyNodePropertyEdit(const std::shared_ptr<core::Node>& node,
                           const std::string& name,
                           const nlohmann::json& value) {
    if (!node) {
        return;
    }
    node->RegisterProperties();
    nlohmann::json nodeJson;
    nodeJson["properties"][name] = value;
    node->Deserialize(nodeJson);
}

// Apply a single component property edit in place, mirroring
// EditorBridge::SetComponentPropertyDirect (registry SetValueFromJson +
// OnPropertyChanged so side effects run).
void ApplyComponentPropertyEdit(core::Component* component,
                                const std::string& name,
                                const nlohmann::json& value) {
    if (!component) {
        return;
    }
    // Ensure the property registry is populated before lookup, exactly as
    // SetComponentPropertyCommand does. A live runtime component may not have
    // registered its reflected properties yet (lazy DefineProperties), in which
    // case GetProperty() would return null and the edit would be silently lost.
    component->RegisterProperties();
    core::ComponentPropertyRegistry& registry = component->GetPropertyRegistry();
    core::ComponentProperty* prop = registry.GetProperty(name);
    if (prop) {
        prop->SetValueFromJson(value);
        component->OnPropertyChanged(name, value);
    }
}

// Case-insensitive check for a file extension (suffix includes the dot).
bool HasExtensionCI(const std::string& path, const std::string& ext) {
    if (path.size() < ext.size()) {
        return false;
    }
    const size_t offset = path.size() - ext.size();
    for (size_t i = 0; i < ext.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(path[offset + i])) !=
            std::tolower(static_cast<unsigned char>(ext[i]))) {
            return false;
        }
    }
    return true;
}

// Reload assets in place for an asset file edit, mirroring
// EditorBridge::ReloadAsset: invalidate caches by path, then let every component
// in the scene reload any file-typed property pointing at the changed file.
void ApplyAssetEdit(core::Scene* scene, const std::string& path) {
    if (path.empty()) {
        return;
    }
    rendering::AssetReloadManager::NotifyAssetChanged(path);

    if (!scene) {
        return;
    }
    const std::string resolvedPath =
        asset::AssetDatabase::GetInstance().ResolveAsset(path);

    std::function<void(const std::shared_ptr<core::Node>&)> notify =
        [&](const std::shared_ptr<core::Node>& node) {
            if (!node) {
                return;
            }
            for (const std::shared_ptr<core::Component>& component : node->GetComponents()) {
                component->OnAssetFileChanged(path, resolvedPath);
            }
            for (const std::shared_ptr<core::Node>& child : node->GetChildren()) {
                notify(child);
            }
        };
    notify(scene->GetRoot());
}

} // namespace

void RuntimeApp::applyLiveEdits(const std::string& editsJson) {
    if (!m_sceneManager || editsJson.empty()) {
        return;
    }

    nlohmann::json edits;
    try {
        edits = nlohmann::json::parse(editsJson);
    } catch (const std::exception& e) {
        LOG_WARN(LogCategory::Core, "RuntimeApp: applyLiveEdits got invalid JSON: {}", e.what());
        return;
    }
    if (!edits.is_array()) {
        return;
    }

    // Guard against the runtime thread mutating the scene while we apply edits,
    // mirroring reloadScene()/reloadScripts().
    std::lock_guard<std::recursive_mutex> lock(m_stateMutex);

    core::Scene* scene = m_sceneManager->GetCurrentScene();
    if (!scene) {
        return;
    }

    for (const nlohmann::json& edit : edits) {
        if (!edit.is_object() || !edit.contains("kind")) {
            continue;
        }
        const std::string kind = edit.value("kind", std::string());

        if (kind == "asset") {
            const std::string path = edit.value("path", std::string());
            ApplyAssetEdit(scene, path);

            // Archetype assets carry no file-typed component property, so the
            // generic ApplyAssetEdit above does not refresh anything for them.
            // Drop the per-script archetype caches so load_archetype() re-reads
            // the changed .ares; for a definition (.archetype) also rescan the
            // registry and clear the cached method environments.
            if (HasExtensionCI(path, ".ares")) {
                scripting::ScriptAPI::InvalidateArchetypeCaches();
            } else if (HasExtensionCI(path, ".archetype")) {
                if (m_sceneManager->GetProject()) {
                    const std::string projectDir =
                        m_sceneManager->GetProject()->GetProjectDirectory();
                    if (!projectDir.empty()) {
                        core::ArchetypeRegistry::GetInstance().ScanProject(projectDir);
                    }
                }
                core::ArchetypeRuntime::GetInstance().Clear();
                scripting::ScriptAPI::InvalidateArchetypeCaches();
            }
            continue;
        }

        if (kind == "scripts") {
            std::shared_ptr<core::Node> root = scene->GetRoot();
            if (root) {
                std::vector<core::ScriptComponent*> scripts;
                CollectScriptComponents(root, scripts);
                for (core::ScriptComponent* script : scripts) {
                    if (script->ReloadScript()) {
                        script->OnAwake();
                        script->OnReady();
                    }
                }
            }
            // A reloaded script may define an archetype class; drop the cached
            // method environments so the next call_archetype runs the new code.
            core::ArchetypeRuntime::GetInstance().Clear();
            scripting::ScriptAPI::InvalidateArchetypeCaches();
            continue;
        }

        // The remaining kinds all target a node identified by UUID.
        const std::string uuidStr = edit.value("node", std::string());
        if (uuidStr.empty()) {
            continue;
        }
        std::shared_ptr<core::Node> node =
            scene->FindNodeByUUID(core::UUID::FromString(uuidStr));
        if (!node) {
            continue;  // e.g. a node added in the editor after Play began
        }

        if (kind == "rename") {
            if (edit.contains("value") && edit["value"].is_string()) {
                node->SetName(edit["value"].get<std::string>());
            }
        } else if (kind == "node") {
            if (edit.contains("name") && edit.contains("value")) {
                ApplyNodePropertyEdit(node, edit["name"].get<std::string>(), edit["value"]);
            }
        } else if (kind == "component") {
            if (!edit.contains("name") || !edit.contains("value") || !edit.contains("index")) {
                continue;
            }
            const int index = edit.value("index", -1);
            const std::vector<std::shared_ptr<core::Component>>& components = node->GetComponents();
            if (index < 0 || static_cast<size_t>(index) >= components.size()) {
                continue;
            }
            ApplyComponentPropertyEdit(components[static_cast<size_t>(index)].get(),
                                       edit["name"].get<std::string>(), edit["value"]);
        }
    }
}

bool RuntimeApp::isShowingSplashScreens() const {
    return m_sceneManager && m_sceneManager->IsShowingSplashScreens();
}

void RuntimeApp::skipSplashScreens() {
    if (m_sceneManager) {
        m_sceneManager->SkipSplashScreens();
    }
}

void RuntimeApp::setupManualSplashScreens(
    const core::SplashScreenSettings& settings,
    float canvasWidth,
    float canvasHeight,
    std::function<void()> onComplete)
{
    if (!m_initialized) {
        LOG_WARN(LogCategory::Render, "RuntimeApp: Cannot setup splash screens - not initialized");
        if (onComplete) onComplete();
        return;
    }

    // Check if splash screens are enabled and have content
    if (!settings.enabled || settings.entries.empty()) {
        LOG_INFO(LogCategory::Render, "RuntimeApp: Splash screens disabled or empty, skipping");
        if (onComplete) onComplete();
        return;
    }

    // Store the completion callback
    m_manualSplashCallback = onComplete;

    // Create a temporary scene for splash screens if we don't have one
    if (!m_sceneManager->GetCurrentScene()) {
        LOG_INFO(LogCategory::Render, "RuntimeApp: Creating empty scene for manual splash screens");
        auto* scene = m_sceneManager->CreateEmptyScene("__ManualSplashScene__");
        if (!scene) {
            LOG_ERROR(LogCategory::Render, "RuntimeApp: Failed to create empty scene for splash screens");
            if (onComplete) onComplete();
            return;
        }
    }

    // Create splash screen node
    auto splashNode = std::make_shared<runtime::SplashScreenNode>("SplashScreens");
    splashNode->SetInputManager(m_inputManager);
    splashNode->SetCompletionCallback([this]() {
        // Set flag to complete splash screens next frame
        m_splashScreensCompleted = true;
    });

    // For manual mode, use empty project directory - images should be in pack
    std::string projectDir = "";

    if (!splashNode->Initialize(settings, projectDir, canvasWidth, canvasHeight)) {
        LOG_WARN(LogCategory::Render, "RuntimeApp: Failed to initialize manual splash screens");
        if (onComplete) onComplete();
        return;
    }

    // Add splash node to the current scene
    auto* scene = m_sceneManager->GetCurrentScene();
    if (scene && scene->GetRoot()) {
        scene->GetRoot()->AddChild(splashNode);
        splashNode->OnReady();
        LOG_INFO(LogCategory::Render, "RuntimeApp: Manual splash screens initialized successfully");
    } else {
        LOG_ERROR(LogCategory::Render, "RuntimeApp: No scene to add splash screens to, calling callback immediately");
        if (onComplete) onComplete();
    }
}

void RuntimeApp::applyWindowIcon() {
    if (!m_window || !m_sceneManager || !m_sceneManager->GetProject()) {
        return;
    }

    const std::string& iconPath = m_sceneManager->GetProject()->GetSettings().iconPath;
    if (iconPath.empty()) {
        return;
    }

    asset::ImageAsset image;
    if (!image.LoadFromFile(iconPath, false)) {
        LOG_WARN(LogCategory::Core, "RuntimeApp: failed to load window icon '{}'", iconPath);
        return;
    }

    const uint32_t width = image.GetWidth();
    const uint32_t height = image.GetHeight();
    const uint8_t* src = image.GetData();
    if (!src || width == 0 || height == 0) {
        return;
    }

    // ImageAsset decodes with vertical flip enabled (for GL texture upload), so
    // its row 0 is the bottom of the image. The window icon needs top-down rows,
    // so flip the rows back into a contiguous RGBA8 buffer for SDL.
    const size_t stride = static_cast<size_t>(width) * 4;
    std::vector<uint8_t> rgba(stride * height);
    for (uint32_t y = 0; y < height; ++y) {
        std::memcpy(&rgba[y * stride], &src[(height - 1 - y) * stride], stride);
    }

    m_window->setIcon(rgba.data(), static_cast<int>(width), static_cast<int>(height));
    LOG_INFO(LogCategory::Core, "RuntimeApp: applied window icon '{}'", iconPath);
}

void RuntimeApp::setupSplashScreenNode(float canvasWidth, float canvasHeight) {
    if (!m_sceneManager || !m_sceneManager->GetProject()) {
        LOG_WARN(LogCategory::Render, "RuntimeApp: Cannot setup splash screens - no project loaded");
        return;
    }

    auto* scene = m_sceneManager->GetCurrentScene();
    if (!scene || !scene->GetRoot()) {
        LOG_WARN(LogCategory::Render, "RuntimeApp: Cannot setup splash screens - no current scene");
        return;
    }

    const auto& settings = m_sceneManager->GetProject()->GetSettings();
    const auto& splashSettings = settings.splashScreenSettings;
    std::string projectDir = m_sceneManager->GetProject()->GetProjectDirectory();

    // Create splash screen node
    auto splashNode = std::make_shared<runtime::SplashScreenNode>("SplashScreens");
    splashNode->SetInputManager(m_inputManager);
    splashNode->SetCompletionCallback([this]() {
        // Set flag to complete splash screens next frame (deferred to avoid scene unload during update)
        m_splashScreensCompleted = true;
    });

    if (!splashNode->Initialize(splashSettings, projectDir, canvasWidth, canvasHeight)) {
        LOG_WARN(LogCategory::Render, "RuntimeApp: Failed to initialize splash screens, loading main scene directly");
        m_splashScreensCompleted = true;  // Trigger immediate load of main scene
        return;
    }

    // Add to current scene
    scene->GetRoot()->AddChild(splashNode);

    // Call lifecycle methods on the splash node
    splashNode->OnReady();

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

void RuntimeApp::updateWindowAndViewport() {
    if (!m_window) return;

    // Get current window size
    int width, height;
    m_window->getSize(width, height);

    // Skip update if window is minimized (zero dimensions)
    if (width <= 0 || height <= 0) return;

    // Resize swapchain if window size has changed
    // This is critical for DirectX 12 and other backends that require explicit swapchain resize
    // Without this, the backbuffer stays at the original size causing a "zoom" effect
    if (m_renderWorld && m_swapchain.isValid() &&
        (width != m_lastSwapchainWidth || height != m_lastSwapchainHeight)) {

        m_renderWorld->getDevice()->resizeSwapchain(
            m_swapchain,
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        );

        m_lastSwapchainWidth = width;
        m_lastSwapchainHeight = height;
    }

    // Update InputManager with current window size
    m_inputManager->SetWindowSize(width, height);

    // Calculate and set viewport for mouse coordinate transforms. The design
    // resolution is the logical canvas (the single source of truth that UI layout
    // and the render canvas also use), so the viewport always maps the physical
    // window onto the same canvas the UI was laid out in - independent of any
    // window-size override.
    Viewport viewport;
    const math::Vec2 logicalCanvas = GetLogicalCanvasSize();
    int designWidth = static_cast<int>(logicalCanvas.x);
    int designHeight = static_cast<int>(logicalCanvas.y);
    core::ProjectSettings::ViewportScaleMode scaleMode =
        (m_sceneManager && m_sceneManager->GetProject())
            ? m_sceneManager->GetProject()->GetSettings().viewportScaleMode
            : m_config.viewportScaleMode;
    viewport = calculateViewport(width, height, designWidth, designHeight, scaleMode);

    // Set viewport for input coordinate transforms (used by Button, etc.)
    SetCurrentViewport(viewport);

    // Render-scale supersampling decision. When the physical window is smaller than the
    // design resolution on either axis, rendering natively at the window resolution
    // throws away detail (aliased sprites, fuzzy text). Instead render the whole frame
    // into an offscreen target at the design resolution and downscale it to the window -
    // supersampling that keeps everything crisp. When the window is >= the design
    // resolution we render natively (the existing path); no extra cost or quality loss.
    // Pixel-art projects (Nearest filtering) opt out: they want pure native rendering
    // with no smoothing, so supersampling is reserved for the smooth filter modes.
    bool smoothFiltering = true;
    if (m_sceneManager && m_sceneManager->GetProject()) {
        smoothFiltering = m_sceneManager->GetProject()->GetSettings().textureFiltering
                          != core::ProjectSettings::TextureFiltering::NearestNeighbor;
    }
    m_superSampleActive = smoothFiltering && (designWidth > 0 && designHeight > 0) &&
                          (width < designWidth || height < designHeight);
    m_superSampleWidth = designWidth;
    m_superSampleHeight = designHeight;

    // Keep glyph atlases baked at the actual on-screen pixel density. UI/text are
    // laid out in the fixed design-resolution canvas and mapped to the window by
    // the viewport, so when the window differs from the design resolution a logical
    // pixel no longer covers one device pixel and atlases baked at the logical font
    // size are minified (or magnified) at draw time, looking fuzzy. The oversample
    // factor is the device-pixels-per-logical-pixel ratio (max of both axes so text
    // is never under-sampled). Changing it re-bakes every live atlas in place.
    //
    // When supersampling, the scene is rendered at the design resolution, so a logical
    // pixel already covers one render pixel: bake at 1.0 and let the final downscale do
    // the antialiasing (text is then supersampled, the crispest result).
    if (designWidth > 0 && designHeight > 0) {
        float oversample;
        if (m_superSampleActive) {
            oversample = 1.0f;
        } else {
            float scaleX = viewport.width / static_cast<float>(designWidth);
            float scaleY = viewport.height / static_cast<float>(designHeight);
            float rawScale = (scaleX > scaleY) ? scaleX : scaleY;
            oversample = QuantizeFontOversample(rawScale);
        }
        if (std::fabs(oversample - GetFontOversample()) > 0.01f) {
            SetFontOversample(oversample);
            if (m_renderWorld && m_renderWorld->getDevice()) {
                m_renderWorld->getDevice()->refreshFontAtlases();
            }
        }
    }

}

void RuntimeApp::render() {
    static int renderCount = 0;
    renderCount++;
    bool debugRender = renderCount <= 5;

    if (debugRender) {
        
        Logger::Flush();
    }

    if (!m_sceneManager || !m_renderWorld) {
        return;
    }

    if (debugRender) {
        
        Logger::Flush();
    }

    auto scene = m_sceneManager->GetCurrentScene();
    if (!scene) {
        return;
    }

    if (debugRender) {
        
        Logger::Flush();
    }

    int width, height;
    m_window->getSize(width, height);
    float aspectRatio = static_cast<float>(width) / static_cast<float>(height);

    if (debugRender) {
        
        Logger::Flush();
    }

    std::shared_ptr<core::Camera3D> camera3D = nullptr;
    std::shared_ptr<core::Camera2D> camera2D = nullptr;
    std::shared_ptr<core::CameraUI> cameraUI = nullptr;

    if (scene->GetRoot()) {
        findCamerasRecursive(scene->GetRoot(), camera3D, camera2D, cameraUI);
    }

    // Debug: Log which cameras were found

    if (debugRender) {

        Logger::Flush();
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

    // A scene can have a world camera (2D/3D) but no CameraUI. Without one, UI elements
    // have no canvas to render into and fall through unrendered at runtime. Fall back to
    // a default UI camera whenever none is present so UI always displays — mirroring the
    // 2D default above. (The block above only triggers when every camera type is absent.)
    if (!cameraUI) {
        cameraUI = std::make_shared<core::CameraUI>();
        cameraUI->SetName("DefaultCameraUI");
        cameraUI->SetActive(true);
    }

    bool isFirstCamera = true;

    // When supersampling, allocate/resize the shared offscreen frame target at the design
    // resolution. If it can't be created, fall back to native window rendering this frame.
    bool superSample = m_superSampleActive;
    if (superSample) {
        superSample = m_renderWorld->ensureFrameTarget(
            static_cast<uint32_t>(m_superSampleWidth),
            static_cast<uint32_t>(m_superSampleHeight));
    }
    // Reflect the effective decision (cleared if the target couldn't be allocated) so
    // renderWithCamera points the views at the frame target for this frame.
    m_superSampleActive = superSample;

    // Begin frame - must be called before any renderView calls
    m_renderWorld->beginFrame();

    if (camera3D && camera3D->IsActive()) {
        if (debugRender) {
            
            Logger::Flush();
        }
        renderWithCamera(camera3D.get(), scene, aspectRatio, isFirstCamera);
        if (debugRender) {
            
            Logger::Flush();
        }
        isFirstCamera = false;
    }

    if (camera2D && camera2D->IsActive()) {
        if (debugRender) {
            
            Logger::Flush();
        }
        renderWithCamera(camera2D.get(), scene, aspectRatio, isFirstCamera);
        if (debugRender) {
            
            Logger::Flush();
        }
        isFirstCamera = false;
    }

    if (cameraUI && cameraUI->IsActive()) {
        if (debugRender) {

            Logger::Flush();
        }
        // Render the UI pass over the ROOT scene, not just the current scene, so a
        // single canvas gather covers the current screen, the singleton nodes and
        // every autoload overlay (the shader transition wipe, pause/settings menus)
        // — including an overlay added while no scene is loaded, which hangs off the
        // root scene rather than the current one. Doing this in one pass (rather than
        // a separate renderView per overlay) keeps the persistent UI view rendered
        // exactly once per frame — repeated renderView calls on the same view
        // flickered — and lets the global canvas-layer sort order everything (UILayer
        // drives draw order: the transition's UILayer 300 composites above the
        // screen). World (3D/2D) passes stay scoped to the current scene above, which
        // is what reaches an overlay's 2D/3D content: overlay roots are children of
        // the current scene's root.
        core::Scene* uiScene = m_sceneManager->GetRootScene();
        if (!uiScene) {
            uiScene = scene;
        }
        renderWithCamera(cameraUI.get(), uiScene, aspectRatio, isFirstCamera);
        if (debugRender) {

            Logger::Flush();
        }
        isFirstCamera = false;
    }

    if (isFirstCamera) {

    }

    // Supersampling: every camera composited into the offscreen frame target at the
    // design resolution. Downscale it into the window (letterboxed by the scale mode)
    // with linear filtering for a crisp, supersampled result.
    if (superSample) {
        core::ProjectSettings::ViewportScaleMode scaleMode =
            (m_sceneManager && m_sceneManager->GetProject())
                ? m_sceneManager->GetProject()->GetSettings().viewportScaleMode
                : m_config.viewportScaleMode;
        Viewport presentViewport = calculateViewport(
            width, height, m_superSampleWidth, m_superSampleHeight, scaleMode);
        m_renderWorld->presentFrameTargetScaled(
            m_swapchain, presentViewport,
            static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    }

    if (debugRender) {

        Logger::Flush();
    }

    // End frame - must be called after all renderView calls, before present
    m_renderWorld->endFrame(false);

    m_renderWorld->getDevice()->present(m_swapchain);

    if (debugRender) {

        Logger::Flush();
    }
}

void RuntimeApp::shutdown() {
    if (!m_initialized) {
        return;
    }

    // Mark as not initialized first to prevent re-entry
    m_initialized = false;
    m_running = false;

    // Clear event callback first to prevent callbacks during cleanup
    if (m_window) {
        m_window->setEventCallback(nullptr);
    }

    // Clear the display backend so its captured `this` cannot dangle past shutdown.
    platform::DisplayServer::Get().ClearProvider();

    // Shutdown input systems
    if (m_inputAdapter) {
        m_inputAdapter->Shutdown();
        m_inputAdapter.reset();
    }

    if (m_inputManager) {
        input::InputManager::Shutdown();
        m_inputManager = nullptr;
    }

    // Shutdown scene manager
    if (m_sceneManager) {
        m_sceneManager->Shutdown();
        m_sceneManager.reset();
    }

    // Invalidate all texture caches before destroying GPU resources.
    // This clears static texture caches (Sprite2D, Sprite3D, StaticMesh3D, etc.)
    // so that stale TextureHandles aren't reused on subsequent runs.
    // Without this, subsequent scene runs would use invalid GPU texture handles
    // and show black/corrupted textures.
    rendering::TextureCache::InvalidateAll();

    // Destroy GPU resources before window
    if (m_swapchain.isValid() && m_renderWorld && m_renderWorld->getDevice()) {
        m_renderWorld->getDevice()->destroySwapchain(m_swapchain);
        m_swapchain = SwapchainHandle();
    }

    // Release the persistent per-camera render views (and their GPU shadow maps,
    // framebuffers, and light UBOs) before tearing down the render world.
    if (m_renderWorld) {
        if (m_view3D != 0) { m_renderWorld->destroyRenderView(m_view3D); m_view3D = 0; }
        if (m_view2D != 0) { m_renderWorld->destroyRenderView(m_view2D); m_view2D = 0; }
        if (m_viewUI != 0) { m_renderWorld->destroyRenderView(m_viewUI); m_viewUI = 0; }
    }

    if (m_renderWorld) {
        m_renderWorld.reset();
    }

    // Destroy window last (SDL cleanup)
    if (m_window) {
        m_window->destroy();
        m_window.reset();
    }

}

void RuntimeApp::installDisplayProvider() {
    platform::DisplayServer::Provider provider;

    provider.setTitle = [this](const std::string& title) {
        if (m_window) m_window->setTitle(title);
    };
    provider.getTitle = [this]() -> std::string {
        return m_window ? m_window->getTitle() : std::string();
    };
    provider.setFullscreen = [this](bool fullscreen) {
        if (m_window) m_window->setFullscreen(fullscreen);
    };
    provider.isFullscreen = [this]() -> bool {
        return m_window ? m_window->isFullscreen() : false;
    };
    provider.setVSync = [this](bool enabled) {
        setVSyncEnabled(enabled);
    };
    provider.isVSync = [this]() -> bool {
        return m_config.vsync;
    };
    provider.setWindowSize = [this](int width, int height) {
        if (m_window) m_window->setSize(width, height);
    };
    provider.getWindowSize = [this]() -> math::Vec2 {
        int width = 0, height = 0;
        if (m_window) m_window->getSize(width, height);
        return math::Vec2(static_cast<float>(width), static_cast<float>(height));
    };
    provider.maximizeWindow = [this]() {
        if (m_window) m_window->maximize();
    };
    provider.minimizeWindow = [this]() {
        if (m_window) m_window->minimize();
    };
    provider.restoreWindow = [this]() {
        if (m_window) m_window->restore();
    };
    provider.getScreenSize = [this]() -> math::Vec2 {
        int width = 0, height = 0;
        if (m_window) m_window->getScreenSize(width, height);
        return math::Vec2(static_cast<float>(width), static_cast<float>(height));
    };
    provider.setMouseMode = [this](platform::MouseMode mode) {
        SDL_Window* win = m_window ? m_window->getSDLWindow() : nullptr;
        switch (mode) {
            case platform::MouseMode::Visible:
                SDL_SetRelativeMouseMode(SDL_FALSE);
                SDL_ShowCursor(SDL_ENABLE);
                if (win) SDL_SetWindowGrab(win, SDL_FALSE);
                break;
            case platform::MouseMode::Hidden:
                SDL_SetRelativeMouseMode(SDL_FALSE);
                SDL_ShowCursor(SDL_DISABLE);
                if (win) SDL_SetWindowGrab(win, SDL_FALSE);
                break;
            case platform::MouseMode::Captured:
                SDL_SetRelativeMouseMode(SDL_TRUE);
                break;
            case platform::MouseMode::Confined:
                SDL_SetRelativeMouseMode(SDL_FALSE);
                SDL_ShowCursor(SDL_ENABLE);
                if (win) SDL_SetWindowGrab(win, SDL_TRUE);
                break;
            case platform::MouseMode::ConfinedHidden:
                SDL_SetRelativeMouseMode(SDL_FALSE);
                SDL_ShowCursor(SDL_DISABLE);
                if (win) SDL_SetWindowGrab(win, SDL_TRUE);
                break;
        }
    };

    provider.openURL = [](const std::string& url) -> bool {
        return SDL_OpenURL(url.c_str()) == 0;
    };

    platform::DisplayServer::Get().SetProvider(provider);
}

void RuntimeApp::setVSyncEnabled(bool enabled) {
    if (m_config.vsync == enabled) {
        return;
    }
    m_config.vsync = enabled;

    if (!m_renderWorld || !m_renderWorld->getDevice() || !m_window) {
        return;
    }

    int width = 0, height = 0;
    m_window->getSize(width, height);
    if (width <= 0 || height <= 0) {
        return;
    }

    IGfxDevice* device = m_renderWorld->getDevice();
    if (m_swapchain.isValid()) {
        device->destroySwapchain(m_swapchain);
        m_swapchain = SwapchainHandle();
    }

    SwapchainDesc swapchainDesc;
    swapchainDesc.window.platformHandle = m_window->getNativeHandle();
    swapchainDesc.width = static_cast<uint32_t>(width);
    swapchainDesc.height = static_cast<uint32_t>(height);
    swapchainDesc.vsync = enabled;
    swapchainDesc.isolated = true;

    m_swapchain = device->createSwapchain(swapchainDesc);
    if (m_swapchain.isValid()) {
        device->makeContextCurrent(m_swapchain);
    }

    m_lastSwapchainWidth = width;
    m_lastSwapchainHeight = height;
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

    static int renderCamCount = 0;
    renderCamCount++;
    bool debugCam = renderCamCount <= 10;

    if (debugCam) {
        
        Logger::Flush();
    }

    if (!cameraNode || !scene || !m_renderWorld) return;

    if (debugCam) {
        
        Logger::Flush();
    }

    std::unique_ptr<RenderCamera> renderCamera;
    RenderViewID* viewSlot = nullptr;

    if (auto* cam3D = dynamic_cast<core::Camera3D*>(cameraNode)) {
        auto camera = std::make_unique<Camera3D>();
        camera->backend = m_renderWorld->getBackend();

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
        viewSlot = &m_view3D;

    } else if (auto* cam2D = dynamic_cast<core::Camera2D*>(cameraNode)) {
        auto camera = std::make_unique<Camera2D>();
        camera->backend = m_renderWorld->getBackend();

        math::Vec2 pos = cam2D->GetEffectivePosition();
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
        viewSlot = &m_view2D;

    } else if (auto* camUI = dynamic_cast<core::CameraUI*>(cameraNode)) {
        auto camera = std::make_unique<CameraCanvas>();
        camera->backend = m_renderWorld->getBackend();

        // The UI canvas is Y-up (centered layout), coinciding with the editor's
        // Camera2D so UI renders right-side up on every backend (GL would otherwise
        // flip via the legacy top-left swap).
        camera->yUp = true;

        // Honor the CameraUI node's canvas origin (defaults to centered (0.5,0.5) so the
        // UI canvas coincides with the centered Camera2D default view) and scaling.
        camera->origin = camUI->GetOrigin();
        camera->scaleFactor = camUI->GetScaleFactor();
        camera->pixelPerfect = camUI->IsPixelPerfect();
        camera->position = camUI->GetEffectivePosition();
        camera->rotation = camUI->GetRotation();
        camera->zoom = camUI->GetZoom();

        // The UI render canvas MUST match the canvas that anchor layout resolves
        // against (UIControl uses GetLogicalCanvasSize()), otherwise anchored
        // controls drift whenever the physical window differs from the design
        // resolution (window-size override, fullscreen, user resize). Reading the
        // single logical-canvas source of truth here - rather than re-deriving it
        // from window settings - keeps layout, rendering, and the viewport mapping
        // in one coordinate space at every window size. The viewport scale mode
        // then handles the design-canvas -> physical-window mapping on its own.
        camera->canvasSize = GetLogicalCanvasSize();

        // Publish the exact inverse view-projection used to draw the UI so hit-testing
        // (UIControl::GetCanvasMousePosition) maps the cursor straight back into control
        // space — accounting for origin, scale_factor, pixel snapping, zoom, rotation
        // and position in one matrix rather than re-deriving them.
        math::Mat4 canvasViewProj = camera->getProjectionMatrix(aspectRatio) * camera->getViewMatrix();
        SetCanvasPickMatrix(canvasViewProj.Inverse());

        if (isFirstCamera) {
            camera->clearFlags = CameraClearFlags::All;
            if (m_sceneManager->GetProject()) {
                camera->clearColor = m_sceneManager->GetProject()->GetSettings().clearColor;
            }
        } else {
            camera->clearFlags = CameraClearFlags::Depth;
        }

        renderCamera = std::move(camera);
        viewSlot = &m_viewUI;
    }

    if (!renderCamera || !viewSlot) return;

    if (debugCam) {
        
        Logger::Flush();
    }

    // Note: Don't call beginFrame() here - RenderWorld::executeRenderPasses() handles that.
    // Calling beginFrame() here would create a separate command buffer that gets abandoned,
    // causing issues on Metal where each beginFrame() acquires a new drawable and waits on
    // the frame semaphore.

    // Reuse the persistent RenderView for this camera type instead of creating and
    // destroying one every frame. Per-frame destruction would free and reallocate the
    // view's shadow maps, framebuffers, and ring-buffered light UBOs each frame - the
    // dominant cost for scenes with shadow-casting lights. Only the camera is updated on
    // the existing view; the view and its GPU resources live until shutdown().
    RenderViewID viewID = *viewSlot;
    if (viewID != 0 && m_renderWorld->getRenderView(viewID)) {
        m_renderWorld->getRenderView(viewID)->setCamera(std::move(renderCamera));
    } else {
        viewID = m_renderWorld->createRenderView(std::move(renderCamera));
        if (viewID == 0) {
            return;
        }
        *viewSlot = viewID;
    }

    if (debugCam) {
        
        Logger::Flush();
    }

    RenderView* view = m_renderWorld->getRenderView(viewID);
    if (!view) {
        m_renderWorld->destroyRenderView(viewID);
        *viewSlot = 0;
        return;
    }

    if (debugCam) {
        
        Logger::Flush();
    }

    // When supersampling, all cameras composite into the shared offscreen frame target
    // (rendered at the design resolution, downscaled to the window after the camera loop).
    // Otherwise the view renders straight into the swapchain at the window resolution.
    const bool superSample = m_superSampleActive && m_renderWorld->hasFrameTarget();
    if (superSample) {
        view->setRenderTarget(m_renderWorld->getFrameTarget());
    } else {
        view->setSwapchain(m_swapchain);
    }
    view->setScene(scene);
    // Link the originating camera node so the renderer can gather its stacked
    // CameraEffect components (blur/glow/outline/color grade/...).
    view->setSourceCameraNode(cameraNode);

    view->setDebugRenderingEnabled(false);

    int width, height;
    m_window->getSize(width, height);

    // CRITICAL: Keep InputManager window size in sync with viewport calculations
    // This ensures mouse coordinate transforms (especially Y flip) use the same
    // window dimensions as the viewport. Without this sync, resizing can cause
    // mouse input offset issues.
    m_inputManager->SetWindowSize(width, height);

    // Map the physical window onto the design resolution (the logical canvas that
    // UI layout and the UI render camera both use), so a window-size override or a
    // resized/fullscreen window letterboxes the same canvas instead of shifting the
    // coordinate space the UI was anchored in.
    const math::Vec2 logicalCanvas = GetLogicalCanvasSize();
    core::ProjectSettings::ViewportScaleMode scaleMode =
        (m_sceneManager && m_sceneManager->GetProject())
            ? m_sceneManager->GetProject()->GetSettings().viewportScaleMode
            : m_config.viewportScaleMode;
    Viewport viewport = calculateViewport(
        width, height,
        static_cast<int>(logicalCanvas.x), static_cast<int>(logicalCanvas.y),
        scaleMode);

    // Input/hit-testing always maps the cursor through the WINDOW-space letterbox
    // viewport (mouse is in window pixels, resolved into the design canvas).
    SetCurrentViewport(viewport);

    // Rendering uses the full frame target (design resolution) when supersampling - the
    // letterboxing is applied later by the downscale present - otherwise the window
    // viewport. Both keep the same design canvas, so projection/anchors are unchanged.
    if (superSample) {
        view->setViewport(Viewport{
            0.0f, 0.0f,
            static_cast<float>(m_superSampleWidth),
            static_cast<float>(m_superSampleHeight),
            0.0f, 1.0f});
    } else {
        view->setViewport(viewport);
    }

    if (debugCam) {
        
        Logger::Flush();
    }

    m_renderWorld->renderView(viewID);

    if (debugCam) {

        Logger::Flush();
    }

    // Persistent view: not destroyed here. It is reused next frame (camera updated via
    // setCamera above) and released in shutdown(), which keeps its shadow maps,
    // framebuffers, and ring-buffered light UBOs alive across frames.
}

}
