#pragma once

#include "Scene.hpp"
#include "Component.hpp"
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
#include <functional>

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

    /**
     * Load project with splash screen support
     * If splash screens are enabled, shows them first before loading the main scene.
     * If disabled, loads the main scene directly.
     * @param projectPath Path to the project file
     * @param canvasWidth Canvas width for splash screen sizing
     * @param canvasHeight Canvas height for splash screen sizing
     * @return true if project loaded successfully
     */
    bool LoadProjectWithSplashScreens(const std::string& projectPath, float canvasWidth, float canvasHeight);

    /**
     * Take ownership of an already-parsed project (e.g. one loaded from a pack
     * file by the export runtime) and apply its settings to the physics and
     * audio subsystems. Used when the project JSON does not live on disk and so
     * cannot go through the path-based LoadProject* entry points.
     */
    void AdoptProject(std::unique_ptr<core::Project> project);

    /**
     * Check if splash screens are currently being displayed
     */
    bool IsShowingSplashScreens() const { return m_showingSplashScreens; }

    /**
     * Skip splash screens and load main scene immediately
     */
    void SkipSplashScreens();
    
    core::Project* GetProject() { return m_Project.get(); }
    const core::Project* GetProject() const { return m_Project.get(); }

    // Scene management
    bool LoadScene(const std::string& scenePath);
    bool SwitchScene(const std::string& scenePath);
    void UnloadCurrentScene();

    /**
     * Create an empty scene and set it as the current scene.
     * Used for manual splash screen setup in export template mode.
     * @param name Name for the scene
     * @return Pointer to the created scene, or nullptr on failure
     */
    core::Scene* CreateEmptyScene(const std::string& name);

    core::Scene* GetCurrentScene() { return m_CurrentScene.get(); }
    const core::Scene* GetCurrentScene() const { return m_CurrentScene.get(); }

    // Autoload management
    bool AddAutoloadScene(const std::string& scenePath);
    void RemoveAutoloadScene(const std::string& scenePath);
    void ClearAutoloads();
    // Autoload (add_scene) scenes are overlays that draw on top of the current
    // scene. Their roots are parented as children of the current scene's root, so
    // the current-scene traversals (update/physics/input/render) and the runtime's
    // 2D/3D camera passes - which gather from the current scene only - reach them.
    // The list is retained for ownership and for path-based removal.
    const std::vector<std::shared_ptr<core::Scene>>& GetAutoloadScenes() const { return m_AutoloadScenes; }

    // The root scene parents the current scene and the singleton nodes (and any
    // overlay added while no scene is loaded). The runtime renders the UI pass over
    // this so a single gather covers the current screen plus every overlay
    // (transition wipe, pause/settings menus); the global canvas-layer sort then
    // orders them (UILayer drives draw order).
    core::Scene* GetRootScene() { return m_RootScene.get(); }
    
    // Lifecycle updates (called by RuntimeApp)
    void ProcessInput(float deltaTime);
    void Update(float deltaTime);
    void PhysicsUpdate(float fixedDeltaTime);
    void Render(RenderWorld* renderWorld);

    // Global time scale (1.0 = normal). The runtime multiplies gameplay/physics
    // delta time by this each frame, so 0.5 is slow-motion, 2.0 fast-forward and
    // 0.0 freezes simulation while input keeps flowing. Clamped to >= 0.
    void SetTimeScale(float timeScale) { m_TimeScale = timeScale < 0.0f ? 0.0f : timeScale; }
    float GetTimeScale() const { return m_TimeScale; }

    // The unscaled delta time of the most recent Update(), in seconds. Cached by
    // Update so host code and the C-API have a single engine-wide source of the
    // current frame delta (scripts read their per-instance copy instead).
    float GetDeltaTime() const { return m_LastDeltaTime; }

    // Total number of frames Update() has run since initialization. Incremented at
    // the top of every Update().
    uint64_t GetFrameCount() const { return m_FrameCount; }

    // Global pause state. When paused, per-frame gameplay Update() is gated for the
    // current scene + autoloads + singletons; physics and pausable scripts halt,
    // while input continues to flow. The runtime still renders. Mirrors the
    // ProcessMode::Pausable contract.
    void SetGamePaused(bool paused) { m_GamePaused = paused; }
    bool IsGamePaused() const { return m_GamePaused; }

    // Initialization
    bool Initialize();
    void Shutdown();

    // State
    bool IsInitialized() const { return m_Initialized; }
    bool IsShuttingDown() const { return m_IsShuttingDown; }
    bool HasActiveScene() const { return m_CurrentScene != nullptr; }

    // Application quit request. Scripts (or the C-API) call RequestQuit to ask the
    // host application to exit; the runtime's main loop polls IsQuitRequested each
    // frame and stops cleanly. ClearQuitRequest resets the flag (the runtime clears
    // it on initialize so a new play session starts from a clean slate).
    void RequestQuit() { m_QuitRequested = true; }
    bool IsQuitRequested() const { return m_QuitRequested; }
    void ClearQuitRequest() { m_QuitRequested = false; }

    // Deferred scene change. Scripts call change_scene() which routes here instead
    // of switching immediately: unloading the running scene from inside a script
    // callback frees the very node/state still executing (use-after-free). The
    // host's main loop polls HasPendingSceneChange() between frames (outside the
    // script pump) and performs the swap safely via TakePendingSceneChange().
    void RequestSceneChange(const std::string& scenePath) {
        m_PendingScenePath = scenePath;
        m_HasPendingSceneChange = true;
    }
    bool HasPendingSceneChange() const { return m_HasPendingSceneChange; }
    std::string TakePendingSceneChange() {
        m_HasPendingSceneChange = false;
        std::string path = m_PendingScenePath;
        m_PendingScenePath.clear();
        return path;
    }
    void ClearPendingSceneChange() { m_HasPendingSceneChange = false; m_PendingScenePath.clear(); }

    // True while a scene swap is unloading the outgoing scene. Physics-body
    // components query this to skip per-body teardown (the world is cleared
    // wholesale afterwards), which avoids double-freeing colliders that are owned
    // by both the body and the CollisionBody component.
    bool IsChangingScene() const { return m_IsChangingScene; }

    // Bring a subtree spawned at runtime (a freshly instantiated prefab/scene or a
    // procedurally built node) up to the same script state the scene-load path
    // gives every node: wire each script component's ScriptAPI to this manager and
    // seed the project globals/singletons into their environments. Call on the
    // detached subtree just before attaching it, so the seeding precedes OnReady.
    // Declarative signal connections and OnReady are handled by Node::AddChild when
    // the subtree is attached to a live parent.
    void PrepareRuntimeSubtree(std::shared_ptr<core::Node> node);

    // Physics integration
    physics2d::Physics2DWorld* GetPhysics2DWorld() { return m_Physics2DWorld.get(); }
    physics3d::Physics3DWorld* GetPhysics3DWorld() { return m_Physics3DWorld.get(); }

    // Input integration
    void SetInputManager(input::InputManager* inputManager) { m_InputManager = inputManager; }
    input::InputManager* GetInputManager() { return m_InputManager; }

    // Globals integration
    scripting::GlobalsManager* GetGlobalsManager() { return m_GlobalsManager.get(); }
    const scripting::GlobalsManager* GetGlobalsManager() const { return m_GlobalsManager.get(); }

    // Global variable accessors. Every global is stored as a JSON value (the single
    // source of truth); the typed scalar accessors below are convenience views over
    // it, while GetGlobalValue/SetGlobalValue carry the structured engine types
    // (vectors, colors, quaternions, rects, arrays, dictionaries).
    int GetGlobalInt(const std::string& name, int defaultValue = 0) const;
    float GetGlobalFloat(const std::string& name, float defaultValue = 0.0f) const;
    std::string GetGlobalString(const std::string& name, const std::string& defaultValue = "") const;
    bool GetGlobalBool(const std::string& name, bool defaultValue = false) const;

    void SetGlobalInt(const std::string& name, int value);
    void SetGlobalFloat(const std::string& name, float value);
    void SetGlobalString(const std::string& name, const std::string& value);
    void SetGlobalBool(const std::string& name, bool value);

    // Generic JSON-valued global access. Supports any engine type; structured types
    // are encoded as JSON objects/arrays following the engine's conventions
    // (vec2 {x,y}, color {r,g,b,a}, quat {w,x,y,z}, rect {x,y,w,h}, ...).
    nlohmann::json GetGlobalValue(const std::string& name,
                                  const nlohmann::json& defaultValue = nlohmann::json()) const;
    void SetGlobalValue(const std::string& name, const nlohmann::json& value);

    // Resolve a singleton/autoload node by its configured global name. These nodes
    // are created once from globals.json and persist across scene changes.
    core::Node* GetSingletonNode(const std::string& name) const;

private:
    // Apply project settings (physics tick rate + gravity, audio bus volumes)
    // to the live subsystems. Shared by every project-load entry point.
    void ApplyProjectSettings(const core::ProjectSettings& settings);

    // Internal scene loading
    std::shared_ptr<core::Scene> LoadSceneInternal(const std::string& scenePath);

    // Resolve a res:// virtual path to a real filesystem path under the project
    // directory. Paths that are already absolute (or have no project to resolve
    // against) are returned unchanged. Scenes are read from disk by Scene::Load,
    // so every load/remove entry point must resolve res:// the same way for the
    // paths to match.
    std::string ResolveResPath(const std::string& path) const;

    // Perform the actual autoload removal for an already-res://-resolved path:
    // detach the scene root and Shutdown() every autoload whose stored file path
    // matches. Split out from RemoveAutoloadScene so the public entry point can
    // defer (queue) the removal while an autoload traversal is in flight and the
    // flush can apply it once the traversal has unwound.
    void RemoveAutoloadSceneImmediate(const std::string& resolvedPath);

    // Apply any autoload removals queued while an autoload traversal was active.
    // A no-op unless m_AutoloadIterationDepth is zero (i.e. we are not inside an
    // autoload loop or the per-frame script-driven region), so it is safe to call
    // at the end of each frame phase.
    void FlushPendingAutoloadRemovals();

    // Parent an overlay's root under the current scene's root, so it is a genuine
    // child of the scene it overlays. The root scene is the fallback for an overlay
    // added while no scene is loaded; ReattachAutoloadRoots moves it onto the real
    // scene as soon as one is loaded.
    void AttachAutoloadRoot(const std::shared_ptr<core::Scene>& scene);

    // Detach every overlay root from its parent, without shutting the overlay down.
    // Called before the outgoing scene is unloaded so that overlays survive a
    // change_scene instead of being destroyed along with the subtree they hang off.
    void DetachAutoloadRoots();

    // Re-parent every overlay root onto the (new) current scene. Pairs with
    // DetachAutoloadRoots across a scene swap.
    void ReattachAutoloadRoots();

    // Deliver one phase of a physics-world rebuild to every component in `node`'s subtree.
    void NotifyPhysicsWorldRebuildRecursive(const std::shared_ptr<core::Node>& node,
                                            core::Component::PhysicsWorldRebuildPhase phase);

    // Run one phase of a physics-world rebuild across every surviving overlay. LoadScene
    // clears the physics worlds wholesale, which destroys the bodies of overlays too - but
    // unlike the outgoing scene's components, an overlay's components live on and would be
    // left holding handles into a destroyed world. See Component::PhysicsWorldRebuildPhase.
    void RebuildAutoloadPhysics(core::Component::PhysicsWorldRebuildPhase phase);

    // True when `scene`'s root is a descendant of the current scene's root, i.e. the
    // current-scene traversal already ticks/renders it. The per-phase autoload loops
    // skip those scenes so an overlay is never updated or drawn twice; an overlay
    // that is not attached to the current scene (none loaded, or a script re-parented
    // it out) is still driven directly by those loops.
    bool IsAutoloadDrivenByCurrentScene(const std::shared_ptr<core::Scene>& scene) const;

    // Lifecycle helpers
    void CallOnAwakeRecursive(std::shared_ptr<core::Node> node);
    void CallOnReadyRecursive(std::shared_ptr<core::Node> node);
    void CallOnDestroyRecursive(std::shared_ptr<core::Node> node);

    // Setup ScriptAPI for all script components in the node tree
    void SetupScriptAPIRecursive(std::shared_ptr<core::Node> node);

    // Resolve declarative signal connections loaded from a scene file, once the
    // full node tree is built (target nodes become addressable by UUID/path).
    void ResolveConnectionsRecursive(std::shared_ptr<core::Node> node);

    // Load globals and singletons from project
    bool LoadGlobals(const std::string& projectDir);
    void InitializeSingletons(const std::string& projectDir);

    // Tear down all singleton nodes (called on project unload so a reload starts
    // from a clean slate instead of accumulating duplicates).
    void ClearSingletons();

    // Walk a freshly-set-up subtree and seed every script component's environment
    // with the project globals: each configured global variable is bound by name
    // (initial value) and every registered singleton is bound as a named node
    // global, so scripts can reference both by name. Must run after the subtree's
    // scripts have been loaded (environments initialized) and before OnReady.
    void InjectGlobalsRecursive(std::shared_ptr<core::Node> node);

    // Tick every singleton subtree for one frame phase. Singletons live directly
    // under the persistent root (siblings of the current scene), so they are not
    // reached by the current-scene/autoload traversals and must be driven here.
    void UpdateSingletons(float deltaTime);
    void PhysicsUpdateSingletons(float fixedDeltaTime);
    void ProcessInputSingletons(float deltaTime);
    void DispatchInputEventSingletons(const nlohmann::json& event);
    void RenderSingletons();

    // Root scene that holds autoloads and singletons
    std::shared_ptr<core::Scene> m_RootScene;

    // Singleton/autoload nodes created from globals.json, in declaration order.
    // They persist for the lifetime of the project (across scene changes).
    std::vector<std::shared_ptr<core::Node>> m_SingletonNodes;

    // Fast lookup of singleton nodes by their configured global name.
    std::unordered_map<std::string, core::Node*> m_SingletonsByName;
    
    // Currently loaded scene
    std::shared_ptr<core::Scene> m_CurrentScene;

    // The scene that networking NetworkIds were last deterministically assigned
    // for. When a session is active and the current scene differs from this, the
    // ReplicationManager re-walks the tree to assign scene-placed NetworkIds.
    core::Scene* m_LastNetworkScene = nullptr;
    
    // Autoload scenes (persistent)
    std::vector<std::shared_ptr<core::Scene>> m_AutoloadScenes;

    // Re-entrancy guard for autoload removal. Removing an autoload from inside a
    // script callback that itself runs during an autoload traversal (e.g. an
    // overlay's Back button closing the overlay through a tween/sequence callback,
    // or an on_process/ui_cancel handler) would erase from m_AutoloadScenes while
    // a range-for over it is live AND Shutdown() the very scene whose script is on
    // the stack — iterator invalidation plus a use-after-free, the same hazard
    // RequestSceneChange guards against for the current scene. While any autoload
    // traversal (or the per-frame script pump) is in flight this depth is > 0, so
    // RemoveAutoloadScene queues the resolved path instead of removing it; the
    // queue is flushed once the depth returns to zero at the end of the frame
    // phase.
    int m_AutoloadIterationDepth = 0;
    std::vector<std::string> m_PendingAutoloadRemovals;
    
    // Project
    std::unique_ptr<core::Project> m_Project;
    
    // Physics worlds
    std::unique_ptr<physics2d::Physics2DWorld> m_Physics2DWorld;
    std::unique_ptr<physics3d::Physics3DWorld> m_Physics3DWorld;
    
    // Input manager (not owned)
    input::InputManager* m_InputManager = nullptr;

    // Globals manager
    std::unique_ptr<scripting::GlobalsManager> m_GlobalsManager;

    // Runtime global variable storage (single source of truth for get/set ops).
    // Values are kept as JSON so any engine type round-trips; the typed scalar
    // accessors read/write the relevant JSON alternative.
    std::unordered_map<std::string, nlohmann::json> m_GlobalValues;

    // Timing for physics
    float m_PhysicsAccumulator;
    float m_PhysicsTickRate;  // Hz
    float m_FixedDeltaTime;   // 1.0 / tickRate

    float m_TimeScale = 1.0f;  // Global gameplay/physics time multiplier
    float m_LastDeltaTime = 0.0f;  // Unscaled delta of the most recent Update()
    uint64_t m_FrameCount = 0;  // Frames Update() has run since initialization
    bool m_GamePaused = false;  // Global pause: gates per-frame gameplay Update()

    bool m_Initialized;
    bool m_SceneNeedsReady;  // Flag to call OnReady on next update
    bool m_IsShuttingDown;   // Flag to prevent operations during shutdown
    bool m_QuitRequested = false;  // Set by RequestQuit; polled by the runtime loop
    bool m_HasPendingSceneChange = false;  // Set by RequestSceneChange; polled by the host loop
    std::string m_PendingScenePath;        // Target scene for the deferred change
    bool m_IsChangingScene = false;        // True while unloading the outgoing scene during a swap

    // Splash screen support
    bool m_showingSplashScreens = false;
    std::string m_pendingMainScenePath;  // Main scene to load after splash screens complete

    // Callback for splash screen completion
    void OnSplashScreensComplete();

    // Singleton instance
    static SceneManager* s_Instance;
};

} // namespace core
} // namespace lupine
