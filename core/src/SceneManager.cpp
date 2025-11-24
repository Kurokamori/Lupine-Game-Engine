#include "lupine/core/SceneManager.hpp"
#include "lupine/core/Component.hpp"
#include "lupine/core/ScriptComponent.hpp"
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

    m_Initialized = true;

    return true;
}

void SceneManager::Shutdown() {
    if (!m_Initialized) {
        return;
    }

    m_IsShuttingDown = true;

    UnloadCurrentScene();

    for (auto& scene : m_AutoloadScenes) {
        if (scene) {
            scene->Shutdown();
        }
    }
    m_AutoloadScenes.clear();

    if (m_RootScene) {
        m_RootScene->Shutdown();
        m_RootScene.reset();
    }

    m_Physics2DWorld.reset();
    m_Physics3DWorld.reset();

    UnloadProject();

    m_Initialized = false;

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

    m_PhysicsTickRate = settings.physicsTickRate;
    m_FixedDeltaTime = 1.0f / m_PhysicsTickRate;

    if (m_Physics2DWorld) {
        m_Physics2DWorld->SetGravity(settings.gravity2D);
    }
    if (m_Physics3DWorld) {
        m_Physics3DWorld->SetGravity(settings.gravity3D);
    }

    std::string projectDir = m_Project->GetProjectDirectory();
    LoadGlobals(projectDir);
    InitializeSingletons(projectDir);

    std::string mainScenePath = settings.mainScene;

    if (mainScenePath.find("res://") == 0) {
        mainScenePath = m_Project->GetProjectDirectory() + "/" + mainScenePath.substr(6);
    }

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

    m_PhysicsTickRate = settings.physicsTickRate;
    m_FixedDeltaTime = 1.0f / m_PhysicsTickRate;

    if (m_Physics2DWorld) {
        m_Physics2DWorld->SetGravity(settings.gravity2D);
    }
    if (m_Physics3DWorld) {
        m_Physics3DWorld->SetGravity(settings.gravity3D);
    }

    std::string projectDir = m_Project->GetProjectDirectory();
    LoadGlobals(projectDir);
    InitializeSingletons(projectDir);

    if (!LoadScene(scenePath)) {

        return false;
    }

    return true;
}

void SceneManager::UnloadProject() {
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

bool SceneManager::LoadScene(const std::string& scenePath) {

    UnloadCurrentScene();

    m_CurrentScene = LoadSceneInternal(scenePath);
    if (!m_CurrentScene) {
        return false;
    }

    if (m_RootScene && m_RootScene->GetRoot() && m_CurrentScene->GetRoot()) {
        m_RootScene->GetRoot()->AddChild(m_CurrentScene->GetRoot());
    }

    if (m_CurrentScene->GetRoot()) {
        SetupScriptAPIRecursive(m_CurrentScene->GetRoot());
    }

    if (m_CurrentScene->GetRoot()) {
        CallOnAwakeRecursive(m_CurrentScene->GetRoot());
    }

    m_CurrentScene->Initialize();

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

bool SceneManager::AddAutoloadScene(const std::string& scenePath) {

    auto scene = LoadSceneInternal(scenePath);
    if (!scene) {
        return false;
    }

    if (m_RootScene && m_RootScene->GetRoot() && scene->GetRoot()) {
        m_RootScene->GetRoot()->AddChild(scene->GetRoot());
    }

    if (scene->GetRoot()) {
        SetupScriptAPIRecursive(scene->GetRoot());
    }

    if (scene->GetRoot()) {
        CallOnAwakeRecursive(scene->GetRoot());
    }

    scene->Initialize();

    m_AutoloadScenes.push_back(scene);

    return true;
}

void SceneManager::RemoveAutoloadScene(const std::string& scenePath) {
    auto it = std::remove_if(m_AutoloadScenes.begin(), m_AutoloadScenes.end(),
        [&scenePath](const std::shared_ptr<core::Scene>& scene) {
            return scene->GetFilePath() == scenePath;
        });

    if (it != m_AutoloadScenes.end()) {
        for (auto iter = it; iter != m_AutoloadScenes.end(); ++iter) {
            if (*iter) {

                if ((*iter)->GetRoot() && (*iter)->GetRoot()->GetParent()) {
                    (*iter)->GetRoot()->GetParent()->RemoveChild((*iter)->GetRoot());
                }

                (*iter)->Shutdown();
            }
        }
        m_AutoloadScenes.erase(it, m_AutoloadScenes.end());
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
}

void SceneManager::ProcessInput(float deltaTime) {
    if (!m_Initialized) return;

    for (auto& scene : m_AutoloadScenes) {
        if (scene) {
            scene->ProcessInput(deltaTime);
        }
    }

    if (m_CurrentScene) {
        m_CurrentScene->ProcessInput(deltaTime);
    }
}

void SceneManager::Update(float deltaTime) {
    if (!m_Initialized) return;

    for (auto& scene : m_AutoloadScenes) {
        if (scene) {
            scene->Update(deltaTime);
        }
    }

    if (m_CurrentScene) {
        m_CurrentScene->Update(deltaTime);
    }
}

void SceneManager::PhysicsUpdate(float fixedDeltaTime) {
    if (!m_Initialized || m_IsShuttingDown) return;

    if (m_Physics2DWorld) {
        m_Physics2DWorld->Step(fixedDeltaTime);

        static int physicsUpdateCount = 0;
        physicsUpdateCount++;
        if (physicsUpdateCount % 60 == 0) {

        }
    }
    if (m_Physics3DWorld) {
        m_Physics3DWorld->Step(fixedDeltaTime);
    }

    for (auto& scene : m_AutoloadScenes) {
        if (scene) {
            scene->PhysicsUpdate(fixedDeltaTime);
        }
    }

    if (m_CurrentScene) {
        m_CurrentScene->PhysicsUpdate(fixedDeltaTime);
    }
}

void SceneManager::Render(RenderWorld* renderWorld) {
    if (!m_Initialized || !renderWorld) return;

    for (auto& scene : m_AutoloadScenes) {
        if (scene) {
            scene->Render();
        }
    }

    if (m_CurrentScene) {
        m_CurrentScene->Render();
    }
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
            component->OnDestroy();
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

bool SceneManager::LoadGlobals(const std::string& projectDir) {
    if (!m_GlobalsManager) {

        return false;
    }

    std::string globalsPath = projectDir + "/globals.json";

    if (!m_GlobalsManager->LoadGlobalsConfig(globalsPath)) {

        return true;
    }

    const auto& variables = m_GlobalsManager->GetGlobalVariables();
    for (const auto& var : variables) {
        if (var.type == "int") {
            m_GlobalInts[var.name] = var.intValue;
        } else if (var.type == "float") {
            m_GlobalFloats[var.name] = var.floatValue;
        } else if (var.type == "bool") {
            m_GlobalBools[var.name] = var.boolValue;
        } else if (var.type == "string") {
            m_GlobalStrings[var.name] = var.stringValue;
        }
    }

    return true;
}

void SceneManager::InitializeSingletons(const std::string& projectDir) {
    if (!m_GlobalsManager || !m_RootScene) {
        return;
    }

    const auto& singletons = m_GlobalsManager->GetSingletons();
    if (singletons.empty()) {

        return;
    }

    for (const auto& singleton : singletons) {

        auto singletonNode = std::make_shared<Node>(singleton.globalName);

        std::string ext;
        size_t dotPos = singleton.scriptPath.find_last_of('.');
        if (dotPos != std::string::npos) {
            ext = singleton.scriptPath.substr(dotPos);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        }

        std::shared_ptr<ScriptComponent> scriptComp;

        if (ext == ".py") {
            scriptComp = std::make_shared<PythonScriptComponent>();
        } else if (ext == ".lua") {
            scriptComp = std::make_shared<LuaScriptComponent>();
        } else if (ext == ".rb") {
#ifdef LUPINE_HAS_MRUBY
            scriptComp = std::make_shared<MRubyScriptComponent>();
#else

            continue;
#endif
        } else {

            continue;
        }

        scriptComp->SetScriptPath(singleton.scriptPath);

        singletonNode->AddComponent(scriptComp);

        m_RootScene->GetRoot()->AddChild(singletonNode);

    }

    SetupScriptAPIRecursive(m_RootScene->GetRoot());

    CallOnAwakeRecursive(m_RootScene->GetRoot());
    CallOnReadyRecursive(m_RootScene->GetRoot());

}

int SceneManager::GetGlobalInt(const std::string& name, int defaultValue) const {
    auto it = m_GlobalInts.find(name);
    if (it != m_GlobalInts.end()) {
        return it->second;
    }
    return defaultValue;
}

float SceneManager::GetGlobalFloat(const std::string& name, float defaultValue) const {
    auto it = m_GlobalFloats.find(name);
    if (it != m_GlobalFloats.end()) {
        return it->second;
    }
    return defaultValue;
}

std::string SceneManager::GetGlobalString(const std::string& name, const std::string& defaultValue) const {
    auto it = m_GlobalStrings.find(name);
    if (it != m_GlobalStrings.end()) {
        return it->second;
    }
    return defaultValue;
}

bool SceneManager::GetGlobalBool(const std::string& name, bool defaultValue) const {
    auto it = m_GlobalBools.find(name);
    if (it != m_GlobalBools.end()) {
        return it->second;
    }
    return defaultValue;
}

void SceneManager::SetGlobalInt(const std::string& name, int value) {
    m_GlobalInts[name] = value;
}

void SceneManager::SetGlobalFloat(const std::string& name, float value) {
    m_GlobalFloats[name] = value;
}

void SceneManager::SetGlobalString(const std::string& name, const std::string& value) {
    m_GlobalStrings[name] = value;
}

void SceneManager::SetGlobalBool(const std::string& name, bool value) {
    m_GlobalBools[name] = value;
}

}
}
