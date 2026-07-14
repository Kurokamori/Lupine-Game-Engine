#include "lupine/core/SceneManager.hpp"
#include "lupine/profiling/Profiler.hpp"
#include "lupine/asset/Asset.hpp"
#include "lupine/rendering/debug/DebugDrawQueue.hpp"
#include "lupine/core/Component.hpp"
#include "lupine/core/ScriptComponent.hpp"
#include "lupine/core/SignalDispatcher.hpp"
#include "lupine/core/EventBus.hpp"
#include "lupine/input/InputManager.hpp"
#include "lupine/asset/AssetDatabase.hpp"
#include "lupine/asset/AsyncAssetLoader.hpp"
#include "lupine/asset/AsyncImageLoader.hpp"
#include "lupine/asset/ImageCache.hpp"
#include "lupine/core/ArchetypeRegistry.hpp"
#include "lupine/core/InterfaceRegistry.hpp"
#include "lupine/localization/LocalizationManager.hpp"
#include "lupine/audio/AudioManager.hpp"
#include "lupine/network/NetworkManager.hpp"
#include "lupine/network/ReplicationManager.hpp"
#include "lupine/ui/ThemeManager.hpp"
#include "lupine/core/ArchetypeRuntime.hpp"
#include "lupine/core/CustomComponentRegistry.hpp"
#include "lupine/rendering/TextureCache.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/platform/Platform.hpp"
#include <algorithm>

namespace lupine {
namespace core {

SceneManager* SceneManager::s_Instance = nullptr;

SceneManager::SceneManager()
    : m_PhysicsAccumulator(0.0f),
      m_PhysicsTickRate(60.0f),
      m_FixedDeltaTime(1.0f / 60.0f),
      m_Initialized(false),
      m_IsShuttingDown(false) {

    if (s_Instance == nullptr) {
        s_Instance = this;
    }
}

SceneManager::~SceneManager() {

    if (s_Instance == this) {
        s_Instance = nullptr;
    }
    Shutdown();
}

bool SceneManager::Initialize() {
    if (m_Initialized) {

        return true;
    }

    m_RootScene = std::make_shared<core::Scene>("__Root__");
    auto rootNode = std::make_shared<core::Node>("Root");
    m_RootScene->SetRoot(rootNode);

    m_Physics2DWorld = std::make_unique<physics2d::Physics2DWorld>();
    m_Physics3DWorld = std::make_unique<physics3d::Physics3DWorld>();

    m_GlobalsManager = std::make_unique<scripting::GlobalsManager>();

    // Install the deferred-free handler so QueueFree performs proper lifecycle
    // teardown (OnDestroy on the subtree) before the node is detached and freed.
    SignalDispatcher::Get().SetFreeHandler([this](const std::shared_ptr<core::Node>& node) {
        if (!node) {
            return;
        }
        CallOnDestroyRecursive(node);
        if (node->GetParent()) {
            node->GetParent()->RemoveChild(node);
        }
    });

    m_Initialized = true;

    return true;
}

void SceneManager::Shutdown() {
    if (!m_Initialized) {
        return;
    }

    m_IsShuttingDown = true;

    // Drop any queued deferred work and global subscriptions before tearing down
    // the scene graph so nothing fires against half-destroyed objects.
    SignalDispatcher::Get().Clear();
    SignalDispatcher::Get().SetFreeHandler(nullptr);
    EventBus::Get().Clear();

    // End any active networking session so transports/sockets close cleanly
    // before the scene graph (which may own networked nodes) is torn down.
    network::NetworkManager::GetInstance().Disconnect();

    UnloadCurrentScene();

    // Tear down singletons while the persistent root still exists so their nodes
    // can detach cleanly (before m_RootScene is destroyed below).
    ClearSingletons();

    for (auto& scene : m_AutoloadScenes) {
        if (scene) {
            scene->Shutdown();
        }
    }
    m_AutoloadScenes.clear();
    m_PendingAutoloadRemovals.clear();

    if (m_RootScene) {
        m_RootScene->Shutdown();
        m_RootScene.reset();
    }

    m_Physics2DWorld.reset();
    m_Physics3DWorld.reset();

    UnloadProject();

    m_Initialized = false;

}

void SceneManager::ApplyProjectSettings(const core::ProjectSettings& settings) {
    m_PhysicsTickRate = settings.physicsTickRate;
    if (m_PhysicsTickRate <= 0.0f) {
        m_PhysicsTickRate = 60.0f;
    }
    m_FixedDeltaTime = 1.0f / m_PhysicsTickRate;

    if (m_Physics2DWorld) {
        m_Physics2DWorld->SetGravity(settings.gravity2D);
    }
    if (m_Physics3DWorld) {
        m_Physics3DWorld->SetGravity(settings.gravity3D);
    }

    auto& audio = audio::AudioManager::GetInstance();
    audio.SetMasterVolume(settings.masterVolume);
    if (audio.HasBus("Music")) {
        audio.SetBusVolume("Music", settings.musicVolume);
    }
    if (audio.HasBus("SFX")) {
        audio.SetBusVolume("SFX", settings.sfxVolume);
    }

    // Seed the networking default config from the project so a game's
    // Network.start_*()/lc_net_* calls inherit the project's transport, rates,
    // compatibility gate, and LAN discovery options without restating them.
    network::NetworkConfig netConfig;
    if (settings.networkTransport == "websocket") {
        netConfig.transport = network::TransportKind::WebSocket;
    } else if (settings.networkTransport == "loopback") {
        netConfig.transport = network::TransportKind::Loopback;
    } else {
        netConfig.transport = network::TransportKind::ENet;
    }
    netConfig.port = static_cast<uint16_t>(settings.networkDefaultPort);
    netConfig.maxPeers = static_cast<uint32_t>(settings.networkMaxPeers);
    netConfig.tickRate = settings.networkTickRate;
    netConfig.interpDelayMs = settings.networkInterpDelayMs;
    netConfig.keyframeIntervalMs = settings.networkKeyframeIntervalMs;
    netConfig.pingIntervalSeconds = settings.networkPingIntervalSeconds;
    netConfig.interestRadius = settings.networkInterestRadius;
    netConfig.protocolVersion = static_cast<uint32_t>(settings.networkProtocolVersion);
    netConfig.gameId = settings.networkGameId;
    netConfig.enableLanDiscovery = settings.networkEnableLanDiscovery;
    netConfig.discoveryPort = static_cast<uint16_t>(settings.networkDiscoveryPort);
    netConfig.serverName = settings.networkServerName;
    netConfig.enablePrediction = settings.networkEnablePrediction;
    netConfig.inputRedundancy = static_cast<uint32_t>(settings.networkInputRedundancy);
    netConfig.autoReconnect = settings.networkAutoReconnect;
    netConfig.reconnectAttempts = static_cast<uint32_t>(settings.networkReconnectAttempts);
    netConfig.reconnectDelaySeconds = settings.networkReconnectDelaySeconds;
    netConfig.resumeTimeoutSeconds = settings.networkResumeTimeoutSeconds;
    netConfig.maxMessagesPerSecond = static_cast<uint32_t>(settings.networkMaxMessagesPerSecond);
    netConfig.maxBytesPerSecond = static_cast<uint32_t>(settings.networkMaxBytesPerSecond);
    network::NetworkManager::GetInstance().SetDefaultConfig(netConfig);
}

void SceneManager::AdoptProject(std::unique_ptr<core::Project> project) {
    m_Project = std::move(project);
    if (!m_Project) {
        return;
    }

    const auto& settings = m_Project->GetSettings();

    ApplyProjectSettings(settings);

    // Mirror the full project initialization performed by LoadProject so that
    // exported (pack-mode) runs, which adopt an in-memory project instead of
    // loading one from disk, set up the AssetDatabase, default font, globals,
    // registries, localization and the default theme exactly the same way.
    std::string projectDir = m_Project->GetProjectDirectory();
    LoadGlobals(projectDir);
    InitializeSingletons(projectDir);
    asset::AsyncAssetLoader::GetInstance().Reset();
    asset::AsyncImageLoader::GetInstance().Reset();
    asset::ImageCache::GetInstance().Clear();
    core::CustomComponentRegistry::GetInstance().ScanProject(projectDir);
    core::ArchetypeRegistry::GetInstance().ScanProject(projectDir);
    core::InterfaceRegistry::GetInstance().ScanProject(projectDir);
    core::ArchetypeRuntime::GetInstance().Clear();
    localization::LocalizationManager::GetInstance().LoadProject(projectDir);

    auto& assetDb = asset::AssetDatabase::GetInstance();
    if (!assetDb.IsInitialized()) {
        assetDb.Initialize(projectDir);
    }

    assetDb.SetDefaultFontPath(settings.defaultFont);

    ui::ThemeManager::GetInstance().LoadProject(projectDir, settings.defaultTheme);
}

bool SceneManager::LoadProject(const std::string& projectPath) {
    if (!m_Initialized) {

        return false;
    }

    m_Project = std::make_unique<core::Project>();
    if (!m_Project->Load(projectPath)) {

        m_Project.reset();
        return false;
    }

    const auto& settings = m_Project->GetSettings();

    ApplyProjectSettings(settings);

    std::string projectDir = m_Project->GetProjectDirectory();
    LoadGlobals(projectDir);
    InitializeSingletons(projectDir);
    asset::AsyncAssetLoader::GetInstance().Reset();
    asset::AsyncImageLoader::GetInstance().Reset();
    asset::ImageCache::GetInstance().Clear();
    core::CustomComponentRegistry::GetInstance().ScanProject(projectDir);
    core::ArchetypeRegistry::GetInstance().ScanProject(projectDir);
    core::InterfaceRegistry::GetInstance().ScanProject(projectDir);
    core::ArchetypeRuntime::GetInstance().Clear();
    localization::LocalizationManager::GetInstance().LoadProject(projectDir);

    // Initialize AssetDatabase with project root
    auto& assetDb = asset::AssetDatabase::GetInstance();
    if (!assetDb.IsInitialized()) {
        assetDb.Initialize(projectDir);
    }

    // Set the default font from project settings
    assetDb.SetDefaultFontPath(settings.defaultFont);

    // Load the project's default UI theme (res://default.uitheme) so UI
    // components resolve themed values. Runs after AssetDatabase init so the
    // theme's res:// palette/base references resolve correctly.
    ui::ThemeManager::GetInstance().LoadProject(projectDir, settings.defaultTheme);

    const std::string mainScenePath = ResolveResPath(settings.mainScene);

    if (!LoadScene(mainScenePath)) {

        return false;
    }

    return true;
}

bool SceneManager::LoadProjectWithScene(const std::string& projectPath, const std::string& scenePath) {
    if (!m_Initialized) {

        return false;
    }

    m_Project = std::make_unique<core::Project>();
    if (!m_Project->Load(projectPath)) {

        m_Project.reset();
        return false;
    }

    const auto& settings = m_Project->GetSettings();

    ApplyProjectSettings(settings);

    std::string projectDir = m_Project->GetProjectDirectory();
    LoadGlobals(projectDir);
    InitializeSingletons(projectDir);
    asset::AsyncAssetLoader::GetInstance().Reset();
    asset::AsyncImageLoader::GetInstance().Reset();
    asset::ImageCache::GetInstance().Clear();
    core::CustomComponentRegistry::GetInstance().ScanProject(projectDir);
    core::ArchetypeRegistry::GetInstance().ScanProject(projectDir);
    core::InterfaceRegistry::GetInstance().ScanProject(projectDir);
    core::ArchetypeRuntime::GetInstance().Clear();
    localization::LocalizationManager::GetInstance().LoadProject(projectDir);

    // Initialize AssetDatabase with project root
    auto& assetDb = asset::AssetDatabase::GetInstance();
    if (!assetDb.IsInitialized()) {
        assetDb.Initialize(projectDir);
    }

    // Set the default font from project settings
    assetDb.SetDefaultFontPath(settings.defaultFont);

    // Load the project's default UI theme (res://default.uitheme) so UI
    // components resolve themed values. Runs after AssetDatabase init so the
    // theme's res:// palette/base references resolve correctly.
    ui::ThemeManager::GetInstance().LoadProject(projectDir, settings.defaultTheme);

    if (!LoadScene(scenePath)) {

        return false;
    }

    return true;
}

bool SceneManager::LoadProjectWithSplashScreens(const std::string& projectPath, float, float) {
    if (!m_Initialized) {
        LOG_ERROR(LogCategory::Core, "SceneManager: Not initialized");
        return false;
    }

    m_Project = std::make_unique<core::Project>();
    if (!m_Project->Load(projectPath)) {
        LOG_ERROR(LogCategory::Core, "SceneManager: Failed to load project: {}", projectPath);
        m_Project.reset();
        return false;
    }

    const auto& settings = m_Project->GetSettings();

    ApplyProjectSettings(settings);

    std::string projectDir = m_Project->GetProjectDirectory();
    LoadGlobals(projectDir);
    InitializeSingletons(projectDir);
    asset::AsyncAssetLoader::GetInstance().Reset();
    asset::AsyncImageLoader::GetInstance().Reset();
    asset::ImageCache::GetInstance().Clear();
    core::CustomComponentRegistry::GetInstance().ScanProject(projectDir);
    core::ArchetypeRegistry::GetInstance().ScanProject(projectDir);
    core::InterfaceRegistry::GetInstance().ScanProject(projectDir);
    core::ArchetypeRuntime::GetInstance().Clear();
    localization::LocalizationManager::GetInstance().LoadProject(projectDir);

    // Initialize AssetDatabase with project root
    auto& assetDb = asset::AssetDatabase::GetInstance();
    if (!assetDb.IsInitialized()) {
        assetDb.Initialize(projectDir);
    }

    // Set the default font from project settings
    assetDb.SetDefaultFontPath(settings.defaultFont);

    // Load the project's default UI theme (res://default.uitheme) so UI
    // components resolve themed values. Runs after AssetDatabase init so the
    // theme's res:// palette/base references resolve correctly.
    ui::ThemeManager::GetInstance().LoadProject(projectDir, settings.defaultTheme);

    // Resolve main scene path
    const std::string mainScenePath = ResolveResPath(settings.mainScene);

    // Check if splash screens are enabled and have content
    const auto& splashSettings = settings.splashScreenSettings;
    if (splashSettings.enabled && !splashSettings.entries.empty()) {

        // Store the main scene path to load after splash screens complete
        m_pendingMainScenePath = mainScenePath;
        m_showingSplashScreens = true;

        // Create a temporary scene for splash screens (caller will add splash screen node)
        m_CurrentScene = std::make_shared<core::Scene>("__SplashScene__");
        auto splashRoot = std::make_shared<core::Node2D>("SplashRoot");
        m_CurrentScene->SetRoot(splashRoot);

        // Add to root scene
        if (m_RootScene && m_RootScene->GetRoot()) {
            m_RootScene->GetRoot()->AddChild(splashRoot);
        }

        // Initialize the splash scene
        m_CurrentScene->Initialize();

        return true;
    }

    // No splash screens, load main scene directly
    
    return LoadScene(mainScenePath);
}

void SceneManager::SkipSplashScreens() {
    if (!m_showingSplashScreens) {
        return;
    }

    OnSplashScreensComplete();
}

void SceneManager::OnSplashScreensComplete() {
    if (!m_showingSplashScreens) {
        return;
    }

    m_showingSplashScreens = false;

    // Unload splash scene
    UnloadCurrentScene();

    // Invalidate all texture caches to prevent stale handles from splash screen
    // being reused by the main scene (splash Sprite2D textures get cached but
    // the underlying GPU resources may become invalid after scene unload)
    rendering::TextureCache::InvalidateAll();

    // Load the actual main scene
    if (!m_pendingMainScenePath.empty()) {
        if (!LoadScene(m_pendingMainScenePath)) {
            LOG_ERROR(LogCategory::Core, "SceneManager: Failed to load main scene: {}", m_pendingMainScenePath);
        }
        m_pendingMainScenePath.clear();
    }
}

void SceneManager::UnloadProject() {
    // End any networking session tied to the outgoing project before its scenes
    // (and any networked nodes) are released.
    network::NetworkManager::GetInstance().Disconnect();
    ClearSingletons();
    if (m_Project) {

        m_Project.reset();
    }
}

std::shared_ptr<core::Scene> SceneManager::LoadSceneInternal(const std::string& scenePath) {

    auto scene = std::make_shared<core::Scene>();
    if (!scene->Load(scenePath)) {

        return nullptr;
    }

    return scene;
}

std::string SceneManager::ResolveResPath(const std::string& path) const {
    if (path.rfind("res://", 0) != 0 || !m_Project) {
        return path;
    }

    const std::string relative = path.substr(6);

    // In pack mode the project is parsed from a string inside the pack and so has no
    // directory on disk. Joining against the empty directory would yield a leading "/",
    // turning a relative pack key into an absolute path - which the pack sandbox rejects,
    // since pack entries are only ever stored relative.
    const std::string projectDirectory = m_Project->GetProjectDirectory();
    if (projectDirectory.empty()) {
        return relative;
    }

    return projectDirectory + "/" + relative;
}

bool SceneManager::LoadScene(const std::string& scenePath) {
    // Scenes are read from the real filesystem (Scene::Load), so a res:// virtual
    // path (as passed by scripts via change_scene) must first be resolved against
    // the project root. Paths that are already absolute pass through unchanged.
    std::string resolvedPath = ResolveResPath(scenePath);
    // Mark the swap so physics-body components skip their per-body teardown while
    // the outgoing scene is destroyed; the world is then cleared wholesale below.
    m_IsChangingScene = true;

    // The physics worlds are cleared wholesale below, which destroys every body in them -
    // including the bodies belonging to overlays, which (unlike the outgoing scene) survive
    // the swap. Let those components snapshot whatever the body is the sole owner of
    // (velocities) while it is still alive; they rebuild against the fresh world further down.
    RebuildAutoloadPhysics(core::Component::PhysicsWorldRebuildPhase::SaveState);

    // Overlays are children of the current scene, so they would be torn down with it.
    // Lift them out before the unload and re-parent them onto the incoming scene
    // below, which keeps an overlay (and its state) alive across a change_scene.
    DetachAutoloadRoots();
    UnloadCurrentScene();
    if (m_Physics2DWorld) {
        m_Physics2DWorld->Clear();
    }
    if (m_Physics3DWorld) {
        m_Physics3DWorld->Clear();
    }
    m_IsChangingScene = false;

    m_CurrentScene = LoadSceneInternal(resolvedPath);
    if (!m_CurrentScene) {
        // The overlays are alive but unparented. Put them back (under the root scene,
        // since there is no current scene now) so a failed load does not silently
        // strand them outside the tree - and rebuild their physics, because the worlds
        // were cleared regardless of whether the new scene loaded.
        ReattachAutoloadRoots();
        RebuildAutoloadPhysics(core::Component::PhysicsWorldRebuildPhase::RecreateBodies);
        RebuildAutoloadPhysics(core::Component::PhysicsWorldRebuildPhase::AttachColliders);
        LOG_ERROR(LogCategory::Core, "SceneManager: failed to load scene '{}'", resolvedPath);
        return false;
    }

    if (m_RootScene && m_RootScene->GetRoot() && m_CurrentScene->GetRoot()) {
        m_RootScene->GetRoot()->AddChild(m_CurrentScene->GetRoot());
    }

    if (m_CurrentScene->GetRoot()) {
        SetupScriptAPIRecursive(m_CurrentScene->GetRoot());
        ResolveConnectionsRecursive(m_CurrentScene->GetRoot());
    }

    if (m_CurrentScene->GetRoot()) {
        CallOnAwakeRecursive(m_CurrentScene->GetRoot());
    }

    // Expose the persistent singletons to this scene's freshly-initialized scripts
    // before they run OnReady.
    InjectGlobalsRecursive(m_CurrentScene->GetRoot());

    m_CurrentScene->Initialize();

    // The incoming scene is ready; hang the surviving overlays back off its root.
    ReattachAutoloadRoots();

    // Their bodies died with the old world, so rebuild them in the fresh one - after the
    // re-parent, so each body is placed at the node's final world transform. Bodies first,
    // then colliders, so a collider always finds a live body to attach to.
    RebuildAutoloadPhysics(core::Component::PhysicsWorldRebuildPhase::RecreateBodies);
    RebuildAutoloadPhysics(core::Component::PhysicsWorldRebuildPhase::AttachColliders);

    LOG_INFO(LogCategory::Core, "SceneManager: loaded scene '{}'", resolvedPath);

    return true;
}

bool SceneManager::SwitchScene(const std::string& scenePath) {

    return LoadScene(scenePath);
}

void SceneManager::UnloadCurrentScene() {
    if (!m_CurrentScene) {
        return;
    }

    if (m_CurrentScene->GetRoot() && m_CurrentScene->GetRoot()->GetParent()) {
        m_CurrentScene->GetRoot()->GetParent()->RemoveChild(m_CurrentScene->GetRoot());
    }

    if (m_CurrentScene->GetRoot()) {
        CallOnDestroyRecursive(m_CurrentScene->GetRoot());
    }

    m_CurrentScene->Shutdown();
    m_CurrentScene.reset();
}

Scene* SceneManager::CreateEmptyScene(const std::string& name) {
    // Lift the overlays clear of the outgoing scene so they are not destroyed with
    // it; they are re-parented onto the new scene below. See LoadScene.
    DetachAutoloadRoots();

    // Unload any existing scene
    UnloadCurrentScene();

    // Create a new empty scene
    m_CurrentScene = std::make_shared<core::Scene>(name);
    auto root = std::make_shared<core::Node2D>(name + "Root");
    m_CurrentScene->SetRoot(root);

    // Add to root scene if it exists
    if (m_RootScene && m_RootScene->GetRoot()) {
        m_RootScene->GetRoot()->AddChild(root);
    }

    // Initialize the scene
    m_CurrentScene->Initialize();

    ReattachAutoloadRoots();

    return m_CurrentScene.get();
}

bool SceneManager::AddAutoloadScene(const std::string& scenePath) {
    // Resolve res:// the same way LoadScene does: Scene::Load reads from the real
    // filesystem, so a raw res:// path would not be found and the overlay would
    // silently fail to load. The resolved path also becomes the scene's stored
    // file path, which RemoveAutoloadScene matches against.
    std::string resolvedPath = ResolveResPath(scenePath);

    auto scene = LoadSceneInternal(resolvedPath);
    if (!scene) {
        return false;
    }

    // Wake the overlay's scripts before attaching it. AddChild readies a subtree
    // immediately when its new parent is already ready (which the current scene's
    // root is), and OnReady must never run ahead of OnAwake or before the scripts
    // have their ScriptAPI and the singleton globals.
    if (scene->GetRoot()) {
        SetupScriptAPIRecursive(scene->GetRoot());
        CallOnAwakeRecursive(scene->GetRoot());
        InjectGlobalsRecursive(scene->GetRoot());
    }

    AttachAutoloadRoot(scene);

    if (scene->GetRoot()) {
        ResolveConnectionsRecursive(scene->GetRoot());
    }

    scene->Initialize();

    m_AutoloadScenes.push_back(scene);

    return true;
}

void SceneManager::AttachAutoloadRoot(const std::shared_ptr<core::Scene>& scene) {
    if (!scene || !scene->GetRoot()) {
        return;
    }

    // An overlay belongs to the scene it overlays: the runtime's 2D and 3D camera
    // passes gather draw commands from the current scene's subtree only, so an
    // overlay parented anywhere else renders no world content at all (before this,
    // overlays hung off the root scene as siblings of the current scene and only
    // their UI showed up, via the separate root-scene UI gather).
    core::Node* parent = nullptr;
    if (m_CurrentScene && m_CurrentScene->GetRoot()) {
        parent = m_CurrentScene->GetRoot().get();
    } else if (m_RootScene && m_RootScene->GetRoot()) {
        parent = m_RootScene->GetRoot().get();
    }

    if (parent) {
        parent->AddChild(scene->GetRoot());
    }
}

void SceneManager::DetachAutoloadRoots() {
    for (const std::shared_ptr<core::Scene>& scene : m_AutoloadScenes) {
        if (!scene || !scene->GetRoot()) {
            continue;
        }
        if (core::Node* parent = scene->GetRoot()->GetParent()) {
            parent->RemoveChild(scene->GetRoot());
        }
    }
}

void SceneManager::ReattachAutoloadRoots() {
    for (const std::shared_ptr<core::Scene>& scene : m_AutoloadScenes) {
        AttachAutoloadRoot(scene);
    }
}

void SceneManager::NotifyPhysicsWorldRebuildRecursive(
    const std::shared_ptr<core::Node>& node,
    core::Component::PhysicsWorldRebuildPhase phase) {
    if (!node) {
        return;
    }

    for (const std::shared_ptr<core::Component>& component : node->GetComponents()) {
        if (component) {
            component->OnPhysicsWorldRebuild(phase);
        }
    }

    for (const std::shared_ptr<core::Node>& child : node->GetChildren()) {
        NotifyPhysicsWorldRebuildRecursive(child, phase);
    }
}

void SceneManager::RebuildAutoloadPhysics(core::Component::PhysicsWorldRebuildPhase phase) {
    for (const std::shared_ptr<core::Scene>& scene : m_AutoloadScenes) {
        if (scene && scene->GetRoot()) {
            NotifyPhysicsWorldRebuildRecursive(scene->GetRoot(), phase);
        }
    }
}

bool SceneManager::IsAutoloadDrivenByCurrentScene(const std::shared_ptr<core::Scene>& scene) const {
    if (!scene || !scene->GetRoot() || !m_CurrentScene || !m_CurrentScene->GetRoot()) {
        return false;
    }

    const core::Node* currentRoot = m_CurrentScene->GetRoot().get();
    for (const core::Node* node = scene->GetRoot()->GetParent(); node; node = node->GetParent()) {
        if (node == currentRoot) {
            return true;
        }
    }
    return false;
}

void SceneManager::RemoveAutoloadScene(const std::string& scenePath) {
    // Match against the resolved path: AddAutoloadScene stores the res://-resolved
    // path as the scene's file path, so callers passing a res:// path must be
    // resolved the same way or the overlay would never be found and removed.
    std::string resolvedPath = ResolveResPath(scenePath);

    // Defer the removal when a script callback requests it from inside an autoload
    // traversal (overlay Back button, tween/sequence callback, on_process/ui_cancel
    // handler). Removing now would erase from m_AutoloadScenes while a range-for over
    // it is live and Shutdown() the very scene whose Lua frame is on the stack —
    // iterator invalidation plus use-after-free, crashing the game when an overlay
    // closes. FlushPendingAutoloadRemovals applies it once the traversal unwinds.
    if (m_AutoloadIterationDepth != 0) {
        m_PendingAutoloadRemovals.push_back(resolvedPath);
        return;
    }

    RemoveAutoloadSceneImmediate(resolvedPath);
}

void SceneManager::RemoveAutoloadSceneImmediate(const std::string& resolvedPath) {
    // Collect the matching scenes into owning handles before touching the vector.
    // std::remove_if would move the *kept* elements over the matched ones, leaving the tail
    // holding moved-from (null) shared_ptrs and releasing the matched scene inside remove_if -
    // so the detach/Shutdown loop over the tail would see only nulls and the scene would be
    // destroyed while its root was still parented into the live tree.
    std::vector<std::shared_ptr<core::Scene>> removed;
    for (auto it = m_AutoloadScenes.begin(); it != m_AutoloadScenes.end();) {
        if (*it && (*it)->GetFilePath() == resolvedPath) {
            removed.push_back(*it);
            it = m_AutoloadScenes.erase(it);
        } else {
            ++it;
        }
    }

    for (const std::shared_ptr<core::Scene>& scene : removed) {
        if (!scene) {
            continue;
        }

        if (scene->GetRoot() && scene->GetRoot()->GetParent()) {
            scene->GetRoot()->GetParent()->RemoveChild(scene->GetRoot());
        }

        scene->Shutdown();
    }
}

void SceneManager::FlushPendingAutoloadRemovals() {
    // Only safe to mutate m_AutoloadScenes once no autoload traversal is live.
    if (m_AutoloadIterationDepth != 0) {
        return;
    }
    if (m_PendingAutoloadRemovals.empty()) {
        return;
    }

    // Swap the queue out before processing so a removal's teardown (OnDestroy)
    // that itself queues further removals is handled on the next flush rather than
    // mutating the container being iterated here.
    std::vector<std::string> pending;
    pending.swap(m_PendingAutoloadRemovals);
    for (const std::string& resolvedPath : pending) {
        RemoveAutoloadSceneImmediate(resolvedPath);
    }
}

void SceneManager::ClearAutoloads() {
    for (auto& scene : m_AutoloadScenes) {
        if (scene) {
            if (scene->GetRoot() && scene->GetRoot()->GetParent()) {
                scene->GetRoot()->GetParent()->RemoveChild(scene->GetRoot());
            }
            scene->Shutdown();
        }
    }
    m_AutoloadScenes.clear();
    // Drop any deferred removals: the scenes they referenced are gone now, so a
    // later flush must not try to remove stale paths.
    m_PendingAutoloadRemovals.clear();
}

void SceneManager::ProcessInput(float deltaTime) {
    if (!m_Initialized) return;

    ProcessInputSingletons(deltaTime);

    // Guard the autoload traversals below: an overlay's input handler (e.g. a Back
    // button or a ui_cancel press) may call remove_scene to close itself, which is
    // deferred while this depth is non-zero and applied by the flush at the end.
    ++m_AutoloadIterationDepth;

    // Index-based, re-reading size() each step and holding a shared_ptr copy of the
    // element: an input handler may open a child overlay (add_scene → push_back),
    // which can reallocate m_AutoloadScenes mid-traversal and would dangle a range-
    // for iterator/reference. The copy keeps the in-flight scene alive across a
    // reallocation, and a freshly added overlay simply gets serviced this frame.
    // An overlay attached under the current scene is reached by the current scene's
    // own traversal below, so driving it here as well would double-dispatch it.
    for (size_t i = 0; i < m_AutoloadScenes.size(); ++i) {
        std::shared_ptr<core::Scene> scene = m_AutoloadScenes[i];
        if (scene && !IsAutoloadDrivenByCurrentScene(scene)) {
            scene->ProcessInput(deltaTime);
        }
    }

    if (m_CurrentScene) {
        m_CurrentScene->ProcessInput(deltaTime);
    }

    // Event-based input: deliver each discrete input event collected this frame
    // to the scene tree's on_input_event handlers. The InputManager double-buffers
    // its frame events, so they are stable here and cleared on the next Update().
    const std::vector<input::InputEvent>& events = input::InputManager::Get().GetFrameEvents();
    if (!events.empty()) {
        for (const input::InputEvent& event : events) {
            nlohmann::json eventJson = event.ToJson();

            DispatchInputEventSingletons(eventJson);

            for (size_t i = 0; i < m_AutoloadScenes.size(); ++i) {
                std::shared_ptr<core::Scene> scene = m_AutoloadScenes[i];
                if (scene && !IsAutoloadDrivenByCurrentScene(scene)) {
                    scene->DispatchInputEvent(eventJson);
                }
            }

            if (m_CurrentScene) {
                m_CurrentScene->DispatchInputEvent(eventJson);
            }
        }
    }

    --m_AutoloadIterationDepth;
    FlushPendingAutoloadRemovals();
}

void SceneManager::Update(float deltaTime) {
    if (!m_Initialized) return;

    // Record this frame's delta + advance the frame counter so host code and the
    // C-API have a single engine-wide source for both.
    m_LastDeltaTime = deltaTime;
    ++m_FrameCount;

    // Guard the entire script-driven region of the frame (autoload Update loop,
    // the coroutine pump, and the deferred signal flush). Any remove_scene a
    // script issues from a tween/sequence callback, on_process, or a deferred
    // signal handler is queued while this is non-zero and applied by the flush at
    // the very end of Update, once the autoload container is no longer being
    // walked and the requesting Lua frame has returned.
    ++m_AutoloadIterationDepth;

    // Age the user debug-draw queue before scripts push this frame's primitives,
    // so one-shot draws live exactly the frame they were submitted in.
    core::DebugDrawQueue::Get().Tick(deltaTime);

    // Poll localization tables for on-disk changes (dev hot-reload). Throttled so
    // the filesystem check costs nothing meaningful per frame.
    {
        localization::LocalizationManager& loc = localization::LocalizationManager::GetInstance();
        if (loc.IsHotReloadEnabled()) {
            static int s_LocPollCounter = 0;
            if (++s_LocPollCounter >= 30) {
                s_LocPollCounter = 0;
                loc.PollHotReload();
            }
        }
    }

    // Finalize any asynchronous asset loads that completed on worker threads
    // (resolve archetype instances, register archetype definitions) and fire
    // their completion callbacks before per-frame script logic runs.
    {
        LUPINE_PROFILE_ZONE("AssetLoader.Pump", profiling::ZoneCategory::Asset);
        asset::AsyncAssetLoader::GetInstance().Pump();
    }

    // Receive pass: deliver inbound network traffic (RPCs, state, peer events)
    // before per-frame script logic runs so handlers see this frame's data.
    // No-op (single predictable branch) when no networking session is active.
    {
        LUPINE_PROFILE_ZONE("Network.Poll", profiling::ZoneCategory::Networking);
        network::NetworkManager::GetInstance().Poll(deltaTime);
    }

    // When a networking session is active, deterministically assign scene-placed
    // NetworkIds for the current scene (once per scene). Every peer loads the same
    // scene and reproduces identical ids, so no id table needs to be replicated.
    {
        network::NetworkManager& net = network::NetworkManager::GetInstance();
        if (net.IsActive() && m_CurrentScene) {
            if (m_CurrentScene.get() != m_LastNetworkScene) {
                net.Replication().AssignSceneNetworkIds(m_CurrentScene->GetRoot().get());
                m_LastNetworkScene = m_CurrentScene.get();
            }
        } else if (!net.IsActive()) {
            m_LastNetworkScene = nullptr;
        }
    }

    // Global pause gates per-frame gameplay (singletons, autoloads, the current
    // scene, and the scripting coroutine schedulers) while leaving housekeeping
    // (debug-draw aging, asset/network pumps, deferred signal flushing) running so
    // input still flows and the runtime keeps rendering.
    if (!m_GamePaused) {
        UpdateSingletons(deltaTime);

        // Index-based with a held shared_ptr copy: a script ticked here may open a
        // child overlay (add_scene → push_back), reallocating m_AutoloadScenes mid-
        // traversal. See the matching note in ProcessInput. remove_scene is deferred
        // (m_AutoloadIterationDepth > 0) so erasure never happens under this loop.
        // Overlays attached under the current scene are ticked by its traversal.
        for (size_t i = 0; i < m_AutoloadScenes.size(); ++i) {
            std::shared_ptr<core::Scene> scene = m_AutoloadScenes[i];
            if (scene && !IsAutoloadDrivenByCurrentScene(scene)) {
                scene->Update(deltaTime);
            }
        }

        if (m_CurrentScene) {
            LUPINE_PROFILE_ZONE("Scene.Update", profiling::ZoneCategory::Update);
            m_CurrentScene->Update(deltaTime);
        }

        // Each scripting language now runs on a single shared VM, so its coroutine/
        // await scheduler is process-global and is advanced exactly once per frame
        // here, rather than once per script component. Pump is a cheap no-op when the
        // language's VM was never initialised or has no live coroutines.
        {
            LUPINE_PROFILE_ZONE("Scripting.Pump", profiling::ZoneCategory::Scripting);
            scripting::LuaHost::Instance().Pump(deltaTime);
#ifdef LUPINE_HAS_MRUBY
            scripting::MRubyHost::Instance().Pump(deltaTime);
#endif
#ifdef LUPINE_HAS_MICROPYTHON
            scripting::MicroPythonHost::Instance().Pump(deltaTime);
#endif
        }
    }

    // Send pass: flush queued outbound network traffic after all per-frame logic
    // (scripts, RPC calls, replication writes) has run. No-op when offline.
    network::NetworkManager::GetInstance().Flush();

    // Drain deferred signal emissions, call_deferred requests, and queued node
    // frees once per frame, after all per-frame logic has run.
    SignalDispatcher::Get().Flush();

    // Memory snapshot for the profiler. Cheap opt-in counters (no allocator hook):
    // debug-queue depth and live script-VM heap usage. Skipped when capture is off.
    {
        profiling::Profiler& profiler = profiling::Profiler::Get();
        if (profiler.IsEnabled()) {
            profiler.SetMemory("debugDrawPrimitives", static_cast<int64_t>(core::DebugDrawQueue::Get().Size()));
            profiler.SetMemory("luaHeapBytes", scripting::LuaHost::Instance().GetHeapBytes());
            profiler.SetMemory("assetBytes", asset::Asset::GetTotalTrackedBytes());
            profiler.SetMemory("assetBytes.image", asset::Asset::GetTrackedBytesByType(asset::AssetType::Image));
            profiler.SetMemory("assetBytes.model", asset::Asset::GetTrackedBytesByType(asset::AssetType::Model));
            profiler.SetMemory("assetBytes.audio", asset::Asset::GetTrackedBytesByType(asset::AssetType::Audio));
            profiler.SetMemory("assetBytes.font", asset::Asset::GetTrackedBytesByType(asset::AssetType::Font));
#ifdef LUPINE_HAS_MICROPYTHON
            profiler.SetMemory("microPythonHeapBytes", static_cast<int64_t>(scripting::MicroPythonHost::Instance().GetHeapSize()));
#endif
        }
    }

    // Script-driven region complete: apply any overlay closes (remove_scene)
    // requested during this frame now that no autoload traversal is live.
    --m_AutoloadIterationDepth;
    FlushPendingAutoloadRemovals();
}

void SceneManager::PhysicsUpdate(float fixedDeltaTime) {
    if (!m_Initialized || m_IsShuttingDown) return;
    if (m_GamePaused) return;

    if (m_Physics2DWorld) {
        LUPINE_PROFILE_ZONE("Physics2D.Step", profiling::ZoneCategory::Physics);
        m_Physics2DWorld->Step(fixedDeltaTime);

        static int physicsUpdateCount = 0;
        physicsUpdateCount++;
        if (physicsUpdateCount % 60 == 0) {

        }
    }
    if (m_Physics3DWorld) {
        LUPINE_PROFILE_ZONE("Physics3D.Step", profiling::ZoneCategory::Physics);
        m_Physics3DWorld->Step(fixedDeltaTime);
    }

    PhysicsUpdateSingletons(fixedDeltaTime);

    // Guard the whole physics traversal - the autoload loop *and* the current scene.
    // Overlays are children of the current scene now, so an overlay's
    // on_physics_process handler that closes itself via remove_scene would otherwise
    // Shutdown() the very subtree the current-scene walk is iterating, with its own
    // script frame still on the stack. Deferring to the flush below keeps that safe.
    ++m_AutoloadIterationDepth;

    // Index-based with a held shared_ptr copy so an add_scene (push_back) from a
    // physics handler cannot dangle the traversal; see the note in ProcessInput.
    // Overlays under the current scene are stepped by its traversal below.
    for (size_t i = 0; i < m_AutoloadScenes.size(); ++i) {
        std::shared_ptr<core::Scene> scene = m_AutoloadScenes[i];
        if (scene && !IsAutoloadDrivenByCurrentScene(scene)) {
            scene->PhysicsUpdate(fixedDeltaTime);
        }
    }

    if (m_CurrentScene) {
        m_CurrentScene->PhysicsUpdate(fixedDeltaTime);
    }

    --m_AutoloadIterationDepth;
    FlushPendingAutoloadRemovals();

    // Generate + send replication snapshots from the authority on the fixed tick,
    // after physics has solved so replicated transforms reflect the final state.
    // Rate-limited internally; a no-op when no session is active.
    network::NetworkManager::GetInstance().GenerateSnapshot(fixedDeltaTime);
}

void SceneManager::Render(RenderWorld* renderWorld) {
    if (!m_Initialized || !renderWorld) return;

    RenderSingletons();

    // Guarded across both traversals so any remove_scene reached from a render-time
    // hook defers rather than mutating a subtree mid-walk; the next frame's flush
    // applies it. Overlays are appended as the last children of the current scene's
    // root, so the current-scene walk already draws them last - on top of the scene
    // they overlay, matching the documented add_scene semantics. Only an overlay that
    // is not attached under the current scene (none loaded) needs driving here, with
    // the same index-based add_scene reallocation safety as the update traversals.
    ++m_AutoloadIterationDepth;

    if (m_CurrentScene) {
        m_CurrentScene->Render();
    }

    for (size_t i = 0; i < m_AutoloadScenes.size(); ++i) {
        std::shared_ptr<core::Scene> scene = m_AutoloadScenes[i];
        if (scene && !IsAutoloadDrivenByCurrentScene(scene)) {
            scene->Render();
        }
    }

    --m_AutoloadIterationDepth;
}

void SceneManager::CallOnAwakeRecursive(std::shared_ptr<core::Node> node) {
    if (!node) return;

    for (const auto& component : node->GetComponents()) {
        if (component && component->IsEnabled()) {
            component->OnAwake();
        }
    }

    for (const auto& child : node->GetChildren()) {
        CallOnAwakeRecursive(child);
    }
}

void SceneManager::CallOnReadyRecursive(std::shared_ptr<core::Node> node) {
    if (!node) return;

    for (const auto& component : node->GetComponents()) {
        if (component && component->IsEnabled()) {
            component->OnReady();
        }
    }

    for (const auto& child : node->GetChildren()) {
        CallOnReadyRecursive(child);
    }
}

void SceneManager::CallOnDestroyRecursive(std::shared_ptr<core::Node> node) {
    if (!node) return;

    for (const auto& component : node->GetComponents()) {
        if (component) {
            component->DispatchDestroy();
        }
    }

    for (const auto& child : node->GetChildren()) {
        CallOnDestroyRecursive(child);
    }
}

void SceneManager::SetupScriptAPIRecursive(std::shared_ptr<core::Node> node) {
    if (!node) return;

    for (const auto& component : node->GetComponents()) {
        if (auto* scriptComp = dynamic_cast<ScriptComponent*>(component.get())) {
            if (scriptComp->GetScriptAPI()) {
                scriptComp->GetScriptAPI()->SetSceneManager(this);
            }
        }
    }

    for (const auto& child : node->GetChildren()) {
        SetupScriptAPIRecursive(child);
    }
}

void SceneManager::ResolveConnectionsRecursive(std::shared_ptr<core::Node> node) {
    if (!node) return;

    node->ResolvePendingConnections();

    for (const auto& child : node->GetChildren()) {
        ResolveConnectionsRecursive(child);
    }
}

void SceneManager::PrepareRuntimeSubtree(std::shared_ptr<core::Node> node) {
    if (!node) return;

    SetupScriptAPIRecursive(node);
    InjectGlobalsRecursive(node);
}

bool SceneManager::LoadGlobals(const std::string& projectDir) {
    if (!m_GlobalsManager) {

        return false;
    }

    // In pack mode the project has no directory on disk, and joining against the empty
    // string would yield a leading "/" that the pack sandbox rejects as an absolute path.
    const std::string globalsPath =
        projectDir.empty() ? std::string("globals.json") : (projectDir + "/globals.json");

    if (!m_GlobalsManager->LoadGlobalsConfig(globalsPath)) {

        return true;
    }

    // Start from the configured defaults on every project (re)load.
    m_GlobalValues.clear();

    const auto& variables = m_GlobalsManager->GetGlobalVariables();
    for (const auto& var : variables) {
        m_GlobalValues[var.name] = var.value;
    }

    return true;
}

void SceneManager::InitializeSingletons(const std::string& projectDir) {
    (void)projectDir;
    if (!m_GlobalsManager || !m_RootScene || !m_RootScene->GetRoot()) {
        return;
    }

    // Rebuild from scratch: a (re)load of the project recreates singletons, while
    // scene changes leave them alone (LoadScene never calls this).
    ClearSingletons();

    const auto& singletons = m_GlobalsManager->GetSingletons();
    if (singletons.empty()) {
        return;
    }

    for (const auto& singleton : singletons) {
        if (singleton.globalName.empty()) {
            continue;
        }
        if (m_SingletonsByName.find(singleton.globalName) != m_SingletonsByName.end()) {
            LOG_WARN(LogCategory::Core,
                     "SceneManager: duplicate singleton global name '{}' ignored",
                     singleton.globalName);
            continue;
        }

        std::string ext;
        size_t dotPos = singleton.scriptPath.find_last_of('.');
        if (dotPos != std::string::npos) {
            ext = singleton.scriptPath.substr(dotPos);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        }

        std::shared_ptr<ScriptComponent> scriptComp;

        if (ext == ".py") {
#ifdef LUPINE_HAS_MICROPYTHON
            scriptComp = std::make_shared<PythonScriptComponent>();
#else
            LOG_WARN(LogCategory::Core,
                     "SceneManager: singleton '{}' uses a Python script but MicroPython "
                     "is not enabled in this build; skipping", singleton.globalName);
            continue;
#endif
        } else if (ext == ".lua") {
            scriptComp = std::make_shared<LuaScriptComponent>();
        } else if (ext == ".rb") {
#ifdef LUPINE_HAS_MRUBY
            scriptComp = std::make_shared<MRubyScriptComponent>();
#else
            LOG_WARN(LogCategory::Core,
                     "SceneManager: singleton '{}' uses an mRuby script but mRuby is not "
                     "enabled in this build; skipping", singleton.globalName);
            continue;
#endif
        } else {
            LOG_WARN(LogCategory::Core,
                     "SceneManager: singleton '{}' has unsupported script extension '{}'",
                     singleton.globalName, ext);
            continue;
        }

        auto singletonNode = std::make_shared<Node>(singleton.globalName);
        scriptComp->SetScriptPath(singleton.scriptPath);
        singletonNode->AddComponent(scriptComp);

        m_RootScene->GetRoot()->AddChild(singletonNode);

        m_SingletonNodes.push_back(singletonNode);
        m_SingletonsByName[singleton.globalName] = singletonNode.get();
    }

    // Bring singletons online in dependency-friendly order: wire the ScriptAPI and
    // resolve declarative connections, then OnAwake (which loads each script and
    // initializes its environment). Only once every singleton's environment exists
    // do we inject the named singleton globals into all of them, so a singleton can
    // safely reference any other singleton. OnReady runs last.
    for (const auto& node : m_SingletonNodes) {
        SetupScriptAPIRecursive(node);
    }
    for (const auto& node : m_SingletonNodes) {
        ResolveConnectionsRecursive(node);
    }
    for (const auto& node : m_SingletonNodes) {
        CallOnAwakeRecursive(node);
    }
    for (const auto& node : m_SingletonNodes) {
        InjectGlobalsRecursive(node);
    }
    for (const auto& node : m_SingletonNodes) {
        CallOnReadyRecursive(node);
    }
}

core::Node* SceneManager::GetSingletonNode(const std::string& name) const {
    auto it = m_SingletonsByName.find(name);
    return it != m_SingletonsByName.end() ? it->second : nullptr;
}

void SceneManager::ClearSingletons() {
    if (m_SingletonNodes.empty()) {
        m_SingletonsByName.clear();
        return;
    }

    for (auto& node : m_SingletonNodes) {
        if (!node) continue;
        CallOnDestroyRecursive(node);
        if (node->GetParent()) {
            node->GetParent()->RemoveChild(node);
        }
    }

    m_SingletonNodes.clear();
    m_SingletonsByName.clear();
}

void SceneManager::InjectGlobalsRecursive(std::shared_ptr<core::Node> node) {
    if (!node) return;

    const bool hasGlobals = !m_SingletonsByName.empty() || !m_GlobalValues.empty();
    if (!hasGlobals) return;

    for (const auto& component : node->GetComponents()) {
        auto* scriptComp = dynamic_cast<ScriptComponent*>(component.get());
        if (!scriptComp) continue;

        scripting::IScriptEnvironment* env = scriptComp->GetScriptEnvironment();
        if (!env) continue;

        // Seed configured global variables by name (initial values). The live,
        // shared value is always available through get/set_global[_*] (backed by the
        // SceneManager), so these bare globals are a convenience snapshot. SetGlobalJson
        // delivers scalars as native scalars and structured types as native
        // tables/dicts.
        for (const auto& entry : m_GlobalValues) env->SetGlobalJson(entry.first, entry.second);

        // Bind every singleton/autoload by its global name (a live node object).
        for (const auto& entry : m_SingletonsByName) {
            env->SetGlobalNode(entry.first, entry.second);
        }
    }

    for (const auto& child : node->GetChildren()) {
        InjectGlobalsRecursive(child);
    }
}

void SceneManager::UpdateSingletons(float deltaTime) {
    for (auto& node : m_SingletonNodes) {
        if (node) node->OnProcess(deltaTime);
    }
}

void SceneManager::PhysicsUpdateSingletons(float fixedDeltaTime) {
    for (auto& node : m_SingletonNodes) {
        if (node) node->OnPhysicsProcess(fixedDeltaTime);
    }
}

void SceneManager::ProcessInputSingletons(float deltaTime) {
    for (auto& node : m_SingletonNodes) {
        if (node) node->OnInput(deltaTime);
    }
}

void SceneManager::DispatchInputEventSingletons(const nlohmann::json& event) {
    for (auto& node : m_SingletonNodes) {
        if (node) node->OnInputEvent(event);
    }
}

void SceneManager::RenderSingletons() {
    for (auto& node : m_SingletonNodes) {
        if (node) node->OnRender();
    }
}

int SceneManager::GetGlobalInt(const std::string& name, int defaultValue) const {
    auto it = m_GlobalValues.find(name);
    if (it != m_GlobalValues.end() && it->second.is_number()) {
        return it->second.get<int>();
    }
    return defaultValue;
}

float SceneManager::GetGlobalFloat(const std::string& name, float defaultValue) const {
    auto it = m_GlobalValues.find(name);
    if (it != m_GlobalValues.end() && it->second.is_number()) {
        return it->second.get<float>();
    }
    return defaultValue;
}

std::string SceneManager::GetGlobalString(const std::string& name, const std::string& defaultValue) const {
    auto it = m_GlobalValues.find(name);
    if (it != m_GlobalValues.end() && it->second.is_string()) {
        return it->second.get<std::string>();
    }
    return defaultValue;
}

bool SceneManager::GetGlobalBool(const std::string& name, bool defaultValue) const {
    auto it = m_GlobalValues.find(name);
    if (it != m_GlobalValues.end() && it->second.is_boolean()) {
        return it->second.get<bool>();
    }
    return defaultValue;
}

void SceneManager::SetGlobalInt(const std::string& name, int value) {
    m_GlobalValues[name] = value;
}

void SceneManager::SetGlobalFloat(const std::string& name, float value) {
    m_GlobalValues[name] = value;
}

void SceneManager::SetGlobalString(const std::string& name, const std::string& value) {
    m_GlobalValues[name] = value;
}

void SceneManager::SetGlobalBool(const std::string& name, bool value) {
    m_GlobalValues[name] = value;
}

nlohmann::json SceneManager::GetGlobalValue(const std::string& name, const nlohmann::json& defaultValue) const {
    auto it = m_GlobalValues.find(name);
    if (it != m_GlobalValues.end()) {
        return it->second;
    }
    return defaultValue;
}

void SceneManager::SetGlobalValue(const std::string& name, const nlohmann::json& value) {
    m_GlobalValues[name] = value;
}

}
}
