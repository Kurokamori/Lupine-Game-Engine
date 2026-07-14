#pragma once

#include "Window.hpp"
#include "SDL2InputAdapter.hpp"
#include "lupine/core/SceneManager.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/input/InputManager.hpp"
#include "lupine/core/Project.hpp"
#include <memory>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <utility>

namespace lupine {

// Forward declarations
namespace core {
    class Node;
    class Scene;
    class Camera3D;
    class Camera2D;
    class CameraUI;
}

/**
 * Install a log sink that buffers runtime log messages so the editor can mirror
 * them into its Console panel. Call once after Logger::Init. Idempotent.
 */
void InstallRuntimeLogCapture();

/**
 * Drain the buffered runtime log messages as (level, message) pairs, clearing
 * the buffer. Thread-safe; intended to be polled from the editor's main thread.
 */
std::vector<std::pair<std::string, std::string>> DrainRuntimeLogMessages();

/**
 * Runtime Application
 * Main entry point for the Lupine runtime executable and Python module
 */
class RuntimeApp {
public:
    struct Config {
        std::string title = "Lupine Runtime";
        // Design resolution / logical canvas. UI layout and the render canvas use
        // these; the physical window may differ (see the override fields below).
        int windowWidth = 1280;
        int windowHeight = 720;
        // Actual OS window-size override (export template mode). When enabled the
        // window opens at this physical size while the design resolution above is
        // letterboxed/scaled onto it. Mirrors ProjectSettings::windowSizeOverride*.
        bool windowSizeOverride = false;
        int windowSizeOverrideWidth = 1280;
        int windowSizeOverrideHeight = 720;
        bool debugging = false;  // Enable debug console/logging
        bool vsync = true;
        bool resizable = true;
        bool fullscreen = false;  // Start in fullscreen mode
        bool borderless = false;  // Create the window without a border / title bar
        int targetFrameRate = 0;  // Software frame cap when vsync is off (0 = uncapped)
        float physicsTickRate = 60.0f;  // Fixed-step physics rate (Hz)
        GraphicsBackend backend = GraphicsBackend::OpenGL;  // Graphics backend to use

        // Viewport scaling mode (letterbox, stretch, crop, ignore)
        // Used when projectPath is empty (export template mode)
        core::ProjectSettings::ViewportScaleMode viewportScaleMode =
            core::ProjectSettings::ViewportScaleMode::Letterbox;

        // Optional: Load from project file
        std::string projectPath;

        // Optional: Load specific scene (requires projectPath)
        std::string scenePath;

        // Optional: Override the user:// mount with this physical directory.
        // The editor's multi-instance play points each instance at its own
        // folder so save data, configs and networking state stay isolated.
        // Empty = use the platform default user data path.
        std::string userPath;

        // Extra command-line / runtime arguments forwarded to the game. Read
        // back from scripts via get_cmdline_args() and the C-API. Used by the
        // editor to give each instance a distinct role (e.g. --server).
        std::vector<std::string> args;
    };

    RuntimeApp();
    ~RuntimeApp();

    // Delete copy
    RuntimeApp(const RuntimeApp&) = delete;
    RuntimeApp& operator=(const RuntimeApp&) = delete;

    /**
     * Initialize the runtime with config
     */
    bool initialize(const Config& config);

    /**
     * Run the main loop (blocking)
     */
    void run();

    /**
     * Run the main loop in a separate thread (non-blocking)
     */
    void runAsync();

    /**
     * Run a single frame (for manual control)
     * Returns false if the app should exit
     */
    bool runFrame();

    /**
     * Process SDL events only (must be called from main thread when running async)
     * Returns false if the app should exit
     */
    bool processEvents();

    /**
     * Stop the runtime (exits main loop gracefully)
     */
    void stop();

    /**
     * Shutdown the runtime (cleanup resources)
     */
    void shutdown();

    /**
     * Pause the runtime
     */
    void pause();

    /**
     * Resume the runtime
     */
    void resume();

    /**
     * Advance exactly one update+physics frame while staying paused.
     * No-op when not paused (the loop is already advancing). Thread-safe: the
     * editor calls this from its main thread while the runtime thread loops.
     */
    void step();

    /**
     * Reload the current scene
     */
    void reloadScene();

    /**
     * Hot-reload every script component's source in the current scene in place,
     * preserving the node graph, then re-run their OnAwake/OnReady hooks so the
     * new code takes effect. Used by the editor's IPC "reload scripts" command.
     */
    void reloadScripts();

    /**
     * Apply a batch of live edits pushed from the editor while the game is
     * running, so changes made in the editor (node/component properties, node
     * transforms, asset file edits) take effect in the running instance without
     * a restart and without losing gameplay state.
     *
     * The argument is a JSON array of edit operations. Each element is an object
     * with a "kind" discriminator:
     *   - {"kind":"node",      "node":"<uuid>", "name":"<prop>", "value":<json>}
     *   - {"kind":"component", "node":"<uuid>", "index":<int>, "name":"<prop>", "value":<json>}
     *   - {"kind":"rename",    "node":"<uuid>", "value":"<new name>"}
     *   - {"kind":"asset",     "path":"<res:// or physical path>"}
     *   - {"kind":"scripts"}                       (hot-reload all script sources)
     *
     * Nodes are matched by UUID (stable across the editor/runtime processes
     * because both load the same scene file). Components are matched by their
     * owner node's UUID plus their index within that node's component list, which
     * is identical in both processes since components deserialize in array order.
     * Targets that cannot be resolved (e.g. a node added in the editor after Play
     * began) are skipped silently. Thread-safe (locks the scene state mutex like
     * reloadScene()).
     */
    void applyLiveEdits(const std::string& editsJson);

    /**
     * Load a different scene
     */
    bool loadScene(const std::string& scenePath);

    /**
     * Check if splash screens are currently being displayed
     */
    bool isShowingSplashScreens() const;

    /**
     * Skip splash screens and load main scene immediately
     */
    void skipSplashScreens();

    /**
     * Setup splash screens manually (for export template mode without project file)
     * Call this after initialize() but before run().
     * @param settings Splash screen settings
     * @param canvasWidth Canvas width for sizing
     * @param canvasHeight Canvas height for sizing
     * @param onComplete Callback when splash screens complete (use to load main scene)
     */
    void setupManualSplashScreens(
        const core::SplashScreenSettings& settings,
        float canvasWidth,
        float canvasHeight,
        std::function<void()> onComplete);

    /**
     * Check if app is running
     */
    bool isRunning() const { return m_running; }

    /**
     * Check if app is paused
     */
    bool isPaused() const { return m_paused; }

    /**
     * Get the last loaded config (for relaunching)
     */
    const Config& getConfig() const { return m_config; }

    /**
     * Get the window
     */
    Window* getWindow() { return m_window.get(); }

    /**
     * Get the graphics device (owned by RenderWorld)
     */
    IGfxDevice* getGfxDevice() { return m_renderWorld ? m_renderWorld->getDevice() : nullptr; }

    /**
     * Get the scene manager
     */
    core::SceneManager* getSceneManager() { return m_sceneManager.get(); }

    /**
     * Get the render world
     */
    RenderWorld* getRenderWorld() { return m_renderWorld.get(); }

    /**
     * Get the input manager
     */
    input::InputManager* getInputManager() { return m_inputManager; }

private:
    void processInput(float deltaTime);
    void update(float deltaTime);
    void physicsUpdate(float fixedDeltaTime);
    void render();

    // Scene load without acquiring m_stateMutex. Used by runFrame() (which already
    // holds the mutex for the whole frame) for deferred scene changes, and by the
    // locking public loadScene() wrapper.
    bool loadSceneInternal(const std::string& scenePath);
    void runInternal();  // Internal threaded loop
    void limitFrameRate();  // Sleep to honor m_targetFrameRate when vsync is off

    // Setup splash screen node after project load
    void setupSplashScreenNode(float canvasWidth, float canvasHeight);

    // Apply the loaded project's icon to the window (taskbar + title bar)
    void applyWindowIcon();

    // Camera finding and rendering helpers
    void findCamerasRecursive(
        std::shared_ptr<core::Node> node,
        std::shared_ptr<core::Camera3D>& camera3D,
        std::shared_ptr<core::Camera2D>& camera2D,
        std::shared_ptr<core::CameraUI>& cameraUI);

    void renderWithCamera(
        core::Node* cameraNode,
        core::Scene* scene,
        float aspectRatio,
        bool isFirstCamera);

    // Install the platform::DisplayServer backend (window/display control for
    // scripts) backed by SDL/this window. Cleared at shutdown.
    void installDisplayProvider();

    // Recreate the swapchain with a new vsync setting (runtime vsync toggle).
    void setVSyncEnabled(bool enabled);

    // Viewport calculation based on scale mode
    Viewport calculateViewport(
        int windowWidth,
        int windowHeight,
        int targetWidth,
        int targetHeight,
        core::ProjectSettings::ViewportScaleMode scaleMode);

    // Update window size and viewport for input processing
    // Must be called BEFORE processInput() to ensure correct mouse coordinate transforms
    void updateWindowAndViewport();

    Config m_config;
    std::unique_ptr<Window> m_window;
    std::unique_ptr<core::SceneManager> m_sceneManager;
    std::unique_ptr<RenderWorld> m_renderWorld;  // Owns the graphics device
    input::InputManager* m_inputManager;  // Points to singleton, not owned
    std::unique_ptr<runtime::SDL2InputAdapter> m_inputAdapter;
    SwapchainHandle m_swapchain;

    // Window size tracking for swap chain resize detection
    int m_lastSwapchainWidth = 0;
    int m_lastSwapchainHeight = 0;

    // Render-scale supersampling: when the physical window is smaller than the design
    // resolution, render every camera into a shared offscreen frame target at the design
    // resolution and downscale it to the window, so the whole frame is supersampled and
    // stays crisp instead of being rendered natively at the low window resolution.
    // Decided each frame in updateWindowAndViewport(); consumed by render()/renderWithCamera().
    bool m_superSampleActive = false;
    int m_superSampleWidth = 0;
    int m_superSampleHeight = 0;

    // Persistent render views (one per active camera type). Created lazily on first
    // use and reused every frame so the view's shadow maps, framebuffers, and
    // ring-buffered light UBOs are NOT freed and reallocated each frame. Released in
    // shutdown(). A value of 0 means "not yet created".
    RenderViewID m_view3D = 0;
    RenderViewID m_view2D = 0;
    RenderViewID m_viewUI = 0;

    // Physics timing
    float m_physicsAccumulator = 0.0f;
    float m_fixedDeltaTime = 1.0f / 60.0f;  // Will be set from project settings
    std::chrono::high_resolution_clock::time_point m_lastFrameTime;

    // Software frame-rate cap (used when vsync is off). 0 = uncapped.
    int m_targetFrameRate = 0;
    std::chrono::high_resolution_clock::time_point m_frameDeadline{};

    // Threading. The state mutex serializes the frame loop against the
    // editor-facing scene mutators (loadScene/reloadScene/reloadScripts/
    // applyLiveEdits) arriving from the IPC thread in async play mode. It is
    // recursive because callbacks invoked inside the locked frame (splash
    // completion, scripts) legitimately re-enter the public API on the same
    // thread (e.g. the export template's splash callback calls loadScene()).
    std::unique_ptr<std::thread> m_runtimeThread;
    std::recursive_mutex m_stateMutex;

    // State flags (atomic for thread-safety)
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_paused{false};
    std::atomic<bool> m_isAsync{false};  // True if running in async mode
    std::atomic<bool> m_stepRequested{false};  // One-shot single-frame advance while paused
    bool m_initialized = false;

    // Scene reload tracking
    std::string m_currentScenePath;

    // Splash screen completion flag (deferred to avoid scene unload during update)
    std::atomic<bool> m_splashScreensCompleted{false};

    // Manual splash screen completion callback (for template mode)
    std::function<void()> m_manualSplashCallback;
};

} // namespace lupine
