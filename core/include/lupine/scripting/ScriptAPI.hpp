#pragma once

#include "lupine/core/Core.hpp"
#include "lupine/core/UUID.hpp"
#include "lupine/math/Math.hpp"
#include <string>
#include <memory>
#include <vector>

namespace lupine {

// Forward declarations
namespace core {
    class Node;
    class Component;
    class Scene;
    class SceneManager;
}

namespace asset {
    class ImageAsset;
    class AudioAsset;
    class ModelAsset;
    template<typename T> class AssetRef;
}

namespace scripting {

/**
 * Process mode for scripts
 * Determines when the script's process function is called
 */
enum class ProcessMode {
    Inherit,        // Use parent's process mode
    Pausable,       // Only process when game is not paused (default)
    WhenPaused,     // Only process when game is paused
    Always,         // Always process regardless of pause state
    Disabled        // Never process
};

/**
 * Script API Context
 * Provides access to the engine from scripts
 * This is the main interface that scripts use to interact with the engine
 */
class ScriptAPI {
public:
    ScriptAPI();
    ~ScriptAPI();
    
    // Set the owner node for this script context
    void SetOwner(core::Node* owner);
    core::Node* GetOwner() const { return m_Owner; }
    
    // ========================================================================
    // Game State Control
    // ========================================================================
    
    // Set game run state
    void SetGamePaused(bool paused);
    bool IsGamePaused() const;
    
    // Get delta time
    float GetDeltaTime() const;
    
    // ========================================================================
    // Input Handling
    // ========================================================================
    
    // Action-based input (mapped inputs)
    bool IsActionPressed(const std::string& action) const;
    bool IsActionJustPressed(const std::string& action) const;
    bool IsActionJustReleased(const std::string& action) const;
    float GetActionStrength(const std::string& action) const;
    
    // Axis input (mapped inputs)
    float GetAxis(const std::string& axis) const;
    math::Vec2 GetVector(const std::string& negativeX, const std::string& positiveX,
                         const std::string& negativeY, const std::string& positiveY) const;
    
    // Raw input (direct device access)
    bool IsKeyPressed(int keyCode) const;
    bool IsKeyJustPressed(int keyCode) const;
    bool IsKeyJustReleased(int keyCode) const;
    
    bool IsMouseButtonPressed(int button) const;
    bool IsMouseButtonJustPressed(int button) const;
    bool IsMouseButtonJustReleased(int button) const;
    math::Vec2 GetMousePosition() const;
    math::Vec2 GetMouseDelta() const;
    math::Vec2 GetMouseScrollDelta() const;
    
    // ========================================================================
    // Node/Scene Management
    // ========================================================================
    
    // Queue node for deletion (safe deletion at end of frame)
    void QueueFree(core::Node* node);
    void QueueFreeSelf();  // Queue the owner node for deletion
    
    // Destroy node immediately (use with caution)
    void DestroyNode(core::Node* node);
    
    // Add node to scene
    void AddChild(std::shared_ptr<core::Node> child);
    void AddSibling(std::shared_ptr<core::Node> sibling);
    
    // Remove node from scene
    void RemoveChild(core::Node* child);
    void RemoveChild(const std::string& name);
    
    // Find nodes
    core::Node* FindNode(const std::string& path) const;
    core::Node* FindNodeByUUID(const std::string& uuidStr) const;
    core::Node* GetParent() const;
    core::Node* GetChild(const std::string& name) const;
    core::Node* GetChild(int index) const;
    int GetChildCount() const;
    
    // ========================================================================
    // Component Management
    // ========================================================================
    
    // Find components
    core::Component* GetComponent(const std::string& typeName) const;
    std::vector<core::Component*> GetComponents(const std::string& typeName) const;
    core::Component* GetComponentInChildren(const std::string& typeName) const;
    core::Component* GetComponentInParent(const std::string& typeName) const;
    
    // Add/Remove components
    void AddComponent(std::shared_ptr<core::Component> component);
    void RemoveComponent(core::Component* component);
    
    // ========================================================================
    // Scene Management
    // ========================================================================
    
    // Change scene
    void ChangeScene(const std::string& scenePath);
    
    // Add scene (additive loading)
    void AddScene(const std::string& scenePath);
    
    // Remove scene
    void RemoveScene(const std::string& sceneName);
    
    // Get current scene
    core::Scene* GetCurrentScene() const;
    
    // ========================================================================
    // Asset Loading
    // ========================================================================

    // Load image asset (returns true if successful)
    bool LoadImageAsset(const std::string& assetPath);

    // Load audio asset (returns true if successful)
    // loadMode: "preload" for sound effects, "stream" for music
    bool LoadAudioAsset(const std::string& assetPath, const std::string& loadMode = "preload");

    // Load model asset (returns true if successful)
    bool LoadModelAsset(const std::string& assetPath);

    // Preload multiple assets at once
    void PreloadAssets(const std::vector<std::string>& assetPaths);

    // ========================================================================
    // Audio
    // ========================================================================

    // Play audio (returns audio source UUID as string)
    std::string PlayAudio(const std::string& audioPath, const std::string& busName = "Master",
                         bool loop = false, float volume = 1.0f);
    std::string PlayAudio3D(const std::string& audioPath, const math::Vec3& position,
                           const std::string& busName = "Master", bool loop = false, float volume = 1.0f);

    // Control audio playback
    void StopAudio(const std::string& sourceUUID);
    void PauseAudio(const std::string& sourceUUID);
    void ResumeAudio(const std::string& sourceUUID);

    // Audio bus control
    void SetBusVolume(const std::string& busName, float volume);
    float GetBusVolume(const std::string& busName) const;
    void SetBusMuted(const std::string& busName, bool muted);
    bool IsBusMuted(const std::string& busName) const;

    // ========================================================================
    // Logging
    // ========================================================================

    void LogInfo(const std::string& message);
    void LogWarning(const std::string& message);
    void LogError(const std::string& message);
    void LogDebug(const std::string& message);

    // ========================================================================
    // Utility
    // ========================================================================

    // Random
    float RandomRange(float min, float max);
    int RandomRangeInt(int min, int max);

    // Time
    float GetTime() const;  // Time since game started
    int GetFrameCount() const;

    // Math helpers
    float Lerp(float a, float b, float t);
    float Clamp(float value, float min, float max);
    float Abs(float value);
    float Sign(float value);

    // ========================================================================
    // Global Variables Access
    // ========================================================================

    // Get global variables
    int GetGlobalInt(const std::string& name, int defaultValue = 0) const;
    float GetGlobalFloat(const std::string& name, float defaultValue = 0.0f) const;
    std::string GetGlobalString(const std::string& name, const std::string& defaultValue = "") const;
    bool GetGlobalBool(const std::string& name, bool defaultValue = false) const;

    // Set global variables
    void SetGlobalInt(const std::string& name, int value);
    void SetGlobalFloat(const std::string& name, float value);
    void SetGlobalString(const std::string& name, const std::string& value);
    void SetGlobalBool(const std::string& name, bool value);

    // Internal setters (used by ScriptComponent)
    void SetDeltaTime(float deltaTime) { m_DeltaTime = deltaTime; }
    void SetSceneManager(core::SceneManager* sceneManager) { m_SceneManager = sceneManager; }

private:
    core::Node* m_Owner = nullptr;
    core::SceneManager* m_SceneManager = nullptr;
    float m_DeltaTime = 0.0f;

    // Asset cache - stores loaded assets by path
    std::unordered_map<std::string, asset::AssetRef<asset::ImageAsset>> m_ImageCache;
    std::unordered_map<std::string, asset::AssetRef<asset::AudioAsset>> m_AudioCache;
    std::unordered_map<std::string, asset::AssetRef<asset::ModelAsset>> m_ModelCache;

    friend class ScriptComponent;
};

} // namespace scripting
} // namespace lupine

