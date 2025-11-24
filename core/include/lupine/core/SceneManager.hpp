#pragma once

#include "Scene.hpp"
#include "Project.hpp"
#include "lupine/physics2d/Physics2DWorld.hpp"
#include "lupine/physics3d/Physics3DWorld.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/input/InputManager.hpp"
#include "lupine/scripting/GlobalsManager.hpp"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace lupine {
namespace core {

/**
 * SceneManager - Manages scene lifecycle, switching, and autoloads
 * 
 * Responsibilities:
 * - Load and manage project settings
 * - Handle scene loading and switching
 * - Manage autoload scenes (persistent across scene changes)
 * - Coordinate lifecycle callbacks (OnAwake, OnReady, OnProcess, etc.)
 * - Manage physics tick rate and timing
 * - Handle render pipeline integration
 */
class SceneManager {
public:
    SceneManager();
    ~SceneManager();

    // Singleton access
    static SceneManager* GetInstance() { return s_Instance; }

    // Project management
    bool LoadProject(const std::string& projectPath);
    bool LoadProjectWithScene(const std::string& projectPath, const std::string& scenePath);
    void UnloadProject();
    
    core::Project* GetProject() { return m_Project.get(); }
    const core::Project* GetProject() const { return m_Project.get(); }

    // Scene management
    bool LoadScene(const std::string& scenePath);
    bool SwitchScene(const std::string& scenePath);
    void UnloadCurrentScene();
    
    core::Scene* GetCurrentScene() { return m_CurrentScene.get(); }
    const core::Scene* GetCurrentScene() const { return m_CurrentScene.get(); }

    // Autoload management
    bool AddAutoloadScene(const std::string& scenePath);
    void RemoveAutoloadScene(const std::string& scenePath);
    void ClearAutoloads();
    
    // Lifecycle updates (called by RuntimeApp)
    void ProcessInput(float deltaTime);
    void Update(float deltaTime);
    void PhysicsUpdate(float fixedDeltaTime);
    void Render(RenderWorld* renderWorld);

    // Initialization
    bool Initialize();
    void Shutdown();

    // State
    bool IsInitialized() const { return m_Initialized; }
    bool IsShuttingDown() const { return m_IsShuttingDown; }
    bool HasActiveScene() const { return m_CurrentScene != nullptr; }

    // Physics integration
    physics2d::Physics2DWorld* GetPhysics2DWorld() { return m_Physics2DWorld.get(); }
    physics3d::Physics3DWorld* GetPhysics3DWorld() { return m_Physics3DWorld.get(); }

    // Input integration
    void SetInputManager(input::InputManager* inputManager) { m_InputManager = inputManager; }
    input::InputManager* GetInputManager() { return m_InputManager; }

    // Globals integration
    scripting::GlobalsManager* GetGlobalsManager() { return m_GlobalsManager.get(); }
    const scripting::GlobalsManager* GetGlobalsManager() const { return m_GlobalsManager.get(); }

    // Global variable accessors
    int GetGlobalInt(const std::string& name, int defaultValue = 0) const;
    float GetGlobalFloat(const std::string& name, float defaultValue = 0.0f) const;
    std::string GetGlobalString(const std::string& name, const std::string& defaultValue = "") const;
    bool GetGlobalBool(const std::string& name, bool defaultValue = false) const;

    void SetGlobalInt(const std::string& name, int value);
    void SetGlobalFloat(const std::string& name, float value);
    void SetGlobalString(const std::string& name, const std::string& value);
    void SetGlobalBool(const std::string& name, bool value);

private:
    // Internal scene loading
    std::shared_ptr<core::Scene> LoadSceneInternal(const std::string& scenePath);

    // Lifecycle helpers
    void CallOnAwakeRecursive(std::shared_ptr<core::Node> node);
    void CallOnReadyRecursive(std::shared_ptr<core::Node> node);
    void CallOnDestroyRecursive(std::shared_ptr<core::Node> node);

    // Setup ScriptAPI for all script components in the node tree
    void SetupScriptAPIRecursive(std::shared_ptr<core::Node> node);

    // Load globals and singletons from project
    bool LoadGlobals(const std::string& projectDir);
    void InitializeSingletons(const std::string& projectDir);

    // Root scene that holds autoloads and singletons
    std::shared_ptr<core::Scene> m_RootScene;
    
    // Currently loaded scene
    std::shared_ptr<core::Scene> m_CurrentScene;
    
    // Autoload scenes (persistent)
    std::vector<std::shared_ptr<core::Scene>> m_AutoloadScenes;
    
    // Project
    std::unique_ptr<core::Project> m_Project;
    
    // Physics worlds
    std::unique_ptr<physics2d::Physics2DWorld> m_Physics2DWorld;
    std::unique_ptr<physics3d::Physics3DWorld> m_Physics3DWorld;
    
    // Input manager (not owned)
    input::InputManager* m_InputManager = nullptr;

    // Globals manager
    std::unique_ptr<scripting::GlobalsManager> m_GlobalsManager;

    // Runtime global variables storage (for get/set operations)
    std::unordered_map<std::string, int> m_GlobalInts;
    std::unordered_map<std::string, float> m_GlobalFloats;
    std::unordered_map<std::string, std::string> m_GlobalStrings;
    std::unordered_map<std::string, bool> m_GlobalBools;

    // Timing for physics
    float m_PhysicsAccumulator;
    float m_PhysicsTickRate;  // Hz
    float m_FixedDeltaTime;   // 1.0 / tickRate

    bool m_Initialized;
    bool m_SceneNeedsReady;  // Flag to call OnReady on next update
    bool m_IsShuttingDown;   // Flag to prevent operations during shutdown

    // Singleton instance
    static SceneManager* s_Instance;
};

} // namespace core
} // namespace lupine
