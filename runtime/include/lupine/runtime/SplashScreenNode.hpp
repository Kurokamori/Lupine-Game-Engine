#pragma once

#include "lupine/core/Node.hpp"
#include "lupine/core/Project.hpp"
#include "lupine/core/CameraNodes.hpp"
#include "lupine/components/Sprite2D.hpp"
#include "lupine/asset/ImageAsset.hpp"
#include "lupine/input/InputManager.hpp"
#include <vector>
#include <memory>
#include <functional>

namespace lupine {
namespace runtime {

/**
 * SplashScreenNode - Scene-integrated splash screen display
 *
 * A Node2D that displays a sequence of splash screens with fade transitions.
 * Integrates with the normal scene system instead of running a separate loop.
 *
 * Usage:
 * 1. Create and add to scene before main content loads
 * 2. Call Initialize() with splash screen settings
 * 3. The node handles fade in/hold/fade out automatically in OnProcess()
 * 4. When complete, calls the completion callback and removes itself
 */
class SplashScreenNode : public core::Node2D {
public:
    SplashScreenNode();
    explicit SplashScreenNode(const std::string& name);
    ~SplashScreenNode() override;

    std::string GetTypeName() const override { return "SplashScreenNode"; }

    /**
     * Initialize the splash screen node with settings
     * @param settings Splash screen settings from project
     * @param projectDirectory Project directory for resolving res:// paths
     * @param canvasWidth Canvas width for full-screen sizing
     * @param canvasHeight Canvas height for full-screen sizing
     * @return true if initialization successful
     */
    bool Initialize(
        const core::SplashScreenSettings& settings,
        const std::string& projectDirectory,
        float canvasWidth,
        float canvasHeight);

    /**
     * Set callback for when splash screens complete
     * The callback is invoked when all splash screens finish or user skips
     */
    void SetCompletionCallback(std::function<void()> callback);

    /**
     * Set the input manager for skip detection
     */
    void SetInputManager(input::InputManager* inputManager);

    /**
     * Check if splash screens are enabled and have entries
     */
    bool HasContent() const;

    /**
     * Check if splash sequence is complete
     */
    bool IsComplete() const { return m_complete; }

    /**
     * Skip to end immediately
     */
    void Skip();

    // Node lifecycle overrides
    void OnReady() override;
    void OnProcess(float deltaTime) override;
    void OnInput(float deltaTime) override;

private:
    enum class State {
        FadeIn,
        Hold,
        FadeOut
    };

    // Load image assets for all splash screens
    bool LoadImages();

    // Create/update the display image for current splash
    void SetupCurrentImage();

    // Advance to next splash screen
    void AdvanceToNext();

    // Check if user wants to skip
    bool CheckSkipInput();

    // Complete and clean up
    void Complete();

    // Settings
    core::SplashScreenSettings m_settings;
    std::string m_projectDirectory;
    float m_canvasWidth = 1280.0f;
    float m_canvasHeight = 720.0f;

    // Loaded image assets
    std::vector<asset::AssetRef<asset::ImageAsset>> m_images;

    // Camera for rendering the splash screen
    std::shared_ptr<core::Camera2D> m_camera;

    // Display node with Sprite2D component
    std::shared_ptr<core::Node2D> m_imageNode;
    std::shared_ptr<components::Sprite2D> m_imageComponent;

    // State machine
    size_t m_currentIndex = 0;
    State m_state = State::FadeIn;
    float m_stateTime = 0.0f;
    bool m_complete = false;
    bool m_initialized = false;

    // Completion callback
    std::function<void()> m_completionCallback;

    // Input manager for skip detection (not owned)
    input::InputManager* m_inputManager = nullptr;
};

} // namespace runtime
} // namespace lupine
