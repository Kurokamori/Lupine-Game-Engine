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
 * Runtime Application
 * Main entry point for the Lupine runtime executable and Python module
 */
class RuntimeApp {
public:
    struct Config {
        std::string title = "Lupine Runtime";
        int windowWidth = 1280;
        int windowHeight = 720;
        bool debugging = false;  // Enable debug console/logging
        bool vsync = true;
        bool resizable = true;
        
        // Optional: Load from project file
        std::string projectPath;
        
        // Optional: Load specific scene (requires projectPath)
        std::string scenePath;
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
     * Reload the current scene
     */
    void reloadScene();

    /**
     * Load a different scene
     */
    bool loadScene(const std::string& scenePath);

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
    void runInternal();  // Internal threaded loop

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

    // Viewport calculation based on scale mode
    Viewport calculateViewport(
        int windowWidth,
        int windowHeight,
        int targetWidth,
        int targetHeight,
        core::ProjectSettings::ViewportScaleMode scaleMode);

    Config m_config;
    std::unique_ptr<Window> m_window;
    std::unique_ptr<core::SceneManager> m_sceneManager;
    std::unique_ptr<RenderWorld> m_renderWorld;  // Owns the graphics device
    input::InputManager* m_inputManager;  // Points to singleton, not owned
    std::unique_ptr<runtime::SDL2InputAdapter> m_inputAdapter;
    SwapchainHandle m_swapchain;

    // Physics timing
    float m_physicsAccumulator = 0.0f;
    float m_fixedDeltaTime = 1.0f / 60.0f;  // Will be set from project settings
    std::chrono::high_resolution_clock::time_point m_lastFrameTime;

    // Threading
    std::unique_ptr<std::thread> m_runtimeThread;
    std::mutex m_stateMutex;

    // State flags (atomic for thread-safety)
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_paused{false};
    std::atomic<bool> m_isAsync{false};  // True if running in async mode
    bool m_initialized = false;

    // Scene reload tracking
    std::string m_currentScenePath;
};

} // namespace lupine
