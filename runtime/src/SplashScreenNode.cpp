#include "lupine/runtime/SplashScreenNode.hpp"
#include "lupine/platform/PackFile.hpp"
#include "lupine/logger/Logger.hpp"
#include <algorithm>

namespace lupine {
namespace runtime {

SplashScreenNode::SplashScreenNode()
    : Node2D("SplashScreenNode")
{
}

SplashScreenNode::SplashScreenNode(const std::string& name)
    : Node2D(name)
{
}

SplashScreenNode::~SplashScreenNode() {
    m_images.clear();
}

bool SplashScreenNode::Initialize(
    const core::SplashScreenSettings& settings,
    const std::string& projectDirectory,
    float canvasWidth,
    float canvasHeight)
{
    m_settings = settings;
    m_projectDirectory = projectDirectory;
    m_canvasWidth = canvasWidth;
    m_canvasHeight = canvasHeight;

    if (!HasContent()) {
        
        m_complete = true;
        return true;
    }

    // Load image assets
    if (!LoadImages()) {
        LOG_WARN(LogCategory::Render, "SplashScreenNode: Failed to load some splash images");
    }

    // Create a Camera2D for rendering the splash screen
    m_camera = std::make_shared<core::Camera2D>("SplashCamera");
    m_camera->SetPosition(math::Vec2(m_canvasWidth * 0.5f, m_canvasHeight * 0.5f));
    m_camera->SetOrthoSize(m_canvasHeight);
    m_camera->SetAspectRatio(m_canvasWidth / m_canvasHeight);
    m_camera->SetActive(true);
    AddChild(m_camera);

    // Create the image display node
    m_imageNode = std::make_shared<core::Node2D>("SplashImage");
    m_imageNode->SetPosition(math::Vec2(m_canvasWidth * 0.5f, m_canvasHeight * 0.5f));

    // Create Sprite2D component
    m_imageComponent = std::make_shared<components::Sprite2D>("Sprite");
    m_imageComponent->RegisterProperties();  // Initialize the property system
    m_imageComponent->SetCentered(true);

    m_imageNode->AddComponent(m_imageComponent);
    AddChild(m_imageNode);

    m_initialized = true;

    return true;
}

void SplashScreenNode::SetCompletionCallback(std::function<void()> callback) {
    m_completionCallback = std::move(callback);
}

void SplashScreenNode::SetInputManager(input::InputManager* inputManager) {
    m_inputManager = inputManager;
}

bool SplashScreenNode::HasContent() const {
    return m_settings.enabled && !m_settings.entries.empty();
}

void SplashScreenNode::Skip() {
    if (!m_complete) {
        
        Complete();
    }
}

void SplashScreenNode::OnReady() {
    Node2D::OnReady();

    if (!m_initialized || m_complete) {
        return;
    }

    // Find first valid image
    m_currentIndex = 0;
    while (m_currentIndex < m_images.size() && !m_images[m_currentIndex].IsValid()) {
        m_currentIndex++;
    }

    if (m_currentIndex >= m_images.size()) {
        LOG_WARN(LogCategory::Render, "SplashScreenNode: No valid images found");
        Complete();
        return;
    }

    // Setup initial state
    m_state = State::FadeIn;
    m_stateTime = 0.0f;

    SetupCurrentImage();

}

void SplashScreenNode::OnProcess(float deltaTime) {
    Node2D::OnProcess(deltaTime);

    if (!m_initialized || m_complete) {
        return;
    }

    // Update state machine
    m_stateTime += deltaTime;

    const auto& currentEntry = m_settings.entries[m_currentIndex];
    float alpha = 0.0f;

    switch (m_state) {
        case State::FadeIn:
            if (currentEntry.fadeInDuration > 0.0f) {
                alpha = std::min(1.0f, m_stateTime / currentEntry.fadeInDuration);
            } else {
                alpha = 1.0f;
            }
            if (m_stateTime >= currentEntry.fadeInDuration) {
                m_state = State::Hold;
                m_stateTime = 0.0f;
            }
            break;

        case State::Hold:
            alpha = 1.0f;
            if (m_stateTime >= currentEntry.holdDuration) {
                m_state = State::FadeOut;
                m_stateTime = 0.0f;
            }
            break;

        case State::FadeOut:
            if (currentEntry.fadeOutDuration > 0.0f) {
                alpha = 1.0f - std::min(1.0f, m_stateTime / currentEntry.fadeOutDuration);
            } else {
                alpha = 0.0f;
            }
            if (m_stateTime >= currentEntry.fadeOutDuration) {
                AdvanceToNext();
                if (m_complete) {
                    return;
                }
            }
            break;
    }

    // Update image alpha
    if (m_imageComponent) {
        m_imageComponent->SetModulate(math::Color(1.0f, 1.0f, 1.0f, alpha));
    }
}

void SplashScreenNode::OnInput(float deltaTime) {
    Node2D::OnInput(deltaTime);

    if (!m_initialized || m_complete) {
        return;
    }

    if (m_settings.allowSkip && CheckSkipInput()) {
        Skip();
    }
}

bool SplashScreenNode::LoadImages() {
    auto& packFS = platform::PackFileSystem::Instance();

    m_images.resize(m_settings.entries.size());

    for (size_t i = 0; i < m_settings.entries.size(); ++i) {
        const auto& entry = m_settings.entries[i];
        const std::string& imagePath = entry.imagePath;

        auto image = asset::AssetRef<asset::ImageAsset>(new asset::ImageAsset());
        bool loaded = false;

        // Check if we're in pack mode (exported game)
        if (packFS.isPackMode()) {
            std::string packPath = imagePath;
            if (packPath.find("res://") == 0) {
                packPath = packPath.substr(6);
            }
            std::vector<uint8_t> imageData = packFS.readFile(packPath);
            if (!imageData.empty()) {
                loaded = image->LoadFromMemory(imageData.data(), imageData.size(), true);
                if (loaded) {
                    
                }
            }
        }

        // Try project-relative path
        if (!loaded && !m_projectDirectory.empty()) {
            std::string resolvedPath = imagePath;
            if (resolvedPath.find("res://") == 0) {
                resolvedPath = m_projectDirectory + "/" + resolvedPath.substr(6);
            }
            loaded = image->LoadFromFile(resolvedPath, true);
            if (loaded) {
                
            }
        }

        // Final fallback: direct path
        if (!loaded) {
            std::string directPath = imagePath;
            if (directPath.find("res://") == 0) {
                directPath = directPath.substr(6);
            }
            loaded = image->LoadFromFile(directPath, true);
            if (loaded) {
                
            }
        }

        if (loaded && image->GetWidth() > 0 && image->GetHeight() > 0) {
            m_images[i] = image;
            
        } else {
            LOG_WARN(LogCategory::Render, "SplashScreenNode: Failed to load: {}", entry.imagePath);
        }
    }

    return true;
}

void SplashScreenNode::SetupCurrentImage() {
    if (m_currentIndex >= m_images.size() || !m_images[m_currentIndex].IsValid()) {
        return;
    }

    auto& image = m_images[m_currentIndex];
    const auto& entry = m_settings.entries[m_currentIndex];

    // Set the texture on the Sprite2D component
    m_imageComponent->SetTexture(image);
    // Explicitly set the texture path so Sprite2D can find it for rendering
    m_imageComponent->SetTexturePath(entry.imagePath);

    // Calculate size to fit canvas while preserving aspect ratio
    float imgWidth = static_cast<float>(image->GetWidth());
    float imgHeight = static_cast<float>(image->GetHeight());

    float canvasAspect = m_canvasWidth / m_canvasHeight;
    float imageAspect = imgWidth / imgHeight;

    float displayWidth, displayHeight;
    if (canvasAspect > imageAspect) {
        // Canvas is wider - fit to height
        displayHeight = m_canvasHeight;
        displayWidth = displayHeight * imageAspect;
    } else {
        // Canvas is taller - fit to width
        displayWidth = m_canvasWidth;
        displayHeight = displayWidth / imageAspect;
    }

    m_imageComponent->SetSize(math::Vec2(displayWidth, displayHeight));

    // Start with alpha 0 for fade in
    m_imageComponent->SetModulate(math::Color(1.0f, 1.0f, 1.0f, 0.0f));

}

void SplashScreenNode::AdvanceToNext() {
    m_currentIndex++;

    // Skip entries with missing images
    while (m_currentIndex < m_images.size() && !m_images[m_currentIndex].IsValid()) {
        m_currentIndex++;
    }

    if (m_currentIndex >= m_settings.entries.size()) {
        Complete();
    } else {
        m_state = State::FadeIn;
        m_stateTime = 0.0f;
        SetupCurrentImage();
        
    }
}

bool SplashScreenNode::CheckSkipInput() {
    if (!m_inputManager) {
        return false;
    }

    // Check keyboard keys
    if (m_inputManager->IsKeyJustPressed(input::KeyCode::Space) ||
        m_inputManager->IsKeyJustPressed(input::KeyCode::Enter) ||
        m_inputManager->IsKeyJustPressed(input::KeyCode::Escape) ||
        m_inputManager->IsKeyJustPressed(input::KeyCode::KPEnter)) {
        return true;
    }

    // Check mouse buttons
    if (m_inputManager->IsMouseButtonJustPressed(input::MouseButton::Left) ||
        m_inputManager->IsMouseButtonJustPressed(input::MouseButton::Right)) {
        return true;
    }

    // Check gamepad buttons
    if (m_inputManager->IsGamepadButtonJustPressed(input::GamepadButton::A, 0) ||
        m_inputManager->IsGamepadButtonJustPressed(input::GamepadButton::B, 0) ||
        m_inputManager->IsGamepadButtonJustPressed(input::GamepadButton::Start, 0)) {
        return true;
    }

    return false;
}

void SplashScreenNode::Complete() {
    if (m_complete) {
        return;  // Already completed
    }

    m_complete = true;

    // Call completion callback - this typically triggers scene unload,
    // so we don't try to remove ourselves from the parent
    if (m_completionCallback) {
        m_completionCallback();
    }
}

} // namespace runtime
} // namespace lupine
