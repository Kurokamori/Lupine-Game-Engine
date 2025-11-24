#include "lupine/engine/EditorSession.hpp"
#include "lupine/core/Core.hpp"
#include "lupine/core/Project.hpp"
#include "lupine/platform/Platform.hpp"
#include "lupine/platform/VirtualFileSystem.hpp"
#include "lupine/logger/Logger.hpp"
#include <nlohmann/json.hpp>

#if defined(LUPINE_PLATFORM_WINDOWS)
    #ifdef CreateDirectory
        #undef CreateDirectory
    #endif
    #ifdef DeleteFile
        #undef DeleteFile
    #endif
    #ifdef CopyFile
        #undef CopyFile
    #endif
    #ifdef MoveFile
        #undef MoveFile
    #endif
#endif

namespace lupine {
namespace engine {

EditorSession::EditorSession()
    : m_Initialized(false) {
}

EditorSession::~EditorSession() {
    Shutdown();
}

bool EditorSession::Initialize() {
    if (m_Initialized) {

        return true;
    }

    core::InitializeCore();
    core::RegisterBuiltInTypes();

    if (!m_RecentProjects.Load()) {

    }

    m_EditorBridge = std::make_unique<EditorBridge>();

    m_Initialized = true;

    return true;
}

void EditorSession::Shutdown() {
    if (!m_Initialized) {
        return;
    }

    CloseAllScenes(false);

    if (m_Project) {
        CloseProject(false);
    }

    if (m_EditorBridge) {
        m_EditorBridge->Shutdown();
        m_EditorBridge.reset();
    }

    m_RecentProjects.Save();

    m_Initialized = false;

}

bool EditorSession::CreateProject(const std::string& projectPath, const std::string& projectName) {
    if (!m_Initialized) {

        return false;
    }

    if (m_Project) {
        if (!CloseProject(true)) {

            return false;
        }
    }

    m_Project = std::make_shared<Project>();
    m_Project->GetSettings().projectName = projectName;

    std::string projectDir = platform::Path::GetDirectory(projectPath);
    auto dirResult = platform::FileSystem::CreateDirectory(projectDir, true);
    if (!dirResult.success) {

        m_Project.reset();
        return false;
    }

    std::string baseDir = platform::Path::GetDirectory(projectPath);
    platform::FileSystem::CreateDirectory(platform::Path::Join(baseDir, "scenes"), true);
    platform::FileSystem::CreateDirectory(platform::Path::Join(baseDir, "assets"), true);
    platform::FileSystem::CreateDirectory(platform::Path::Join(baseDir, "scripts"), true);
    platform::FileSystem::CreateDirectory(platform::Path::Join(baseDir, "node"), true);
    platform::FileSystem::CreateDirectory(platform::Path::Join(baseDir, "component"), true);
    platform::FileSystem::CreateDirectory(platform::Path::Join(baseDir, "prefab"), true);

    if (!m_Project->SaveAs(projectPath)) {

        m_Project.reset();
        return false;
    }

    m_RecentProjects.AddOrUpdateProject(projectPath, projectName);
    m_RecentProjects.Save();

    return true;
}

bool EditorSession::OpenProject(const std::string& projectPath) {
    if (!m_Initialized) {

        return false;
    }

    if (!platform::FileSystem::Exists(projectPath)) {

        return false;
    }

    if (m_Project) {
        if (!CloseProject(true)) {

            return false;
        }
    }

    m_Project = std::make_shared<Project>(projectPath);
    if (!m_Project->IsLoaded()) {

        m_Project.reset();
        return false;
    }

    m_RecentProjects.AddOrUpdateProject(projectPath, m_Project->GetSettings().projectName);
    m_RecentProjects.Save();

    return true;
}

bool EditorSession::CloseProject(bool saveOpenScenes) {
    if (!m_Project) {
        return true;
    }

    if (saveOpenScenes) {
        if (!SaveAllScenes()) {

        }
    }

    CloseAllScenes(false);

    m_Project.reset();

    return true;
}

std::shared_ptr<SceneDocument> EditorSession::CreateScene(const std::string& sceneName) {
    if (!m_Initialized) {

        return nullptr;
    }

    auto sceneDoc = SceneDocument::CreateNew(sceneName);
    if (!sceneDoc) {

        return nullptr;
    }

    AddSceneDocument(sceneDoc);

    if (!m_ActiveSceneID.IsValid()) {
        SetActiveScene(sceneDoc->GetID());
    }

    return sceneDoc;
}

std::shared_ptr<SceneDocument> EditorSession::OpenScene(const std::string& scenePath) {
    if (!m_Initialized) {

        return nullptr;
    }

    auto existingDoc = GetSceneDocumentByPath(scenePath);
    if (existingDoc) {

        SetActiveScene(existingDoc->GetID());
        return existingDoc;
    }

    auto sceneDoc = SceneDocument::Open(scenePath);
    if (!sceneDoc) {

        return nullptr;
    }

    AddSceneDocument(sceneDoc);

    if (!m_ActiveSceneID.IsValid()) {
        SetActiveScene(sceneDoc->GetID());
    }

    return sceneDoc;
}

bool EditorSession::CloseScene(const core::UUID& sceneID, bool saveIfDirty) {
    auto sceneDoc = GetSceneDocument(sceneID);
    if (!sceneDoc) {

        return false;
    }

    if (saveIfDirty && sceneDoc->IsDirty()) {
        if (!SaveScene(sceneID)) {

            return false;
        }
    }

    sceneDoc->Close();

    RemoveSceneDocument(sceneID);

    if (m_ActiveSceneID == sceneID) {
        m_ActiveSceneID = core::UUID(0);

        if (!m_OpenScenes.empty()) {
            SetActiveScene(m_OpenScenes.begin()->first);
        }
    }

    return true;
}

bool EditorSession::CloseAllScenes(bool saveIfDirty) {
    std::vector<core::UUID> sceneIDs;
    for (const auto& pair : m_OpenScenes) {
        sceneIDs.push_back(pair.first);
    }

    bool allSuccess = true;
    for (const auto& sceneID : sceneIDs) {
        if (!CloseScene(sceneID, saveIfDirty)) {
            allSuccess = false;
        }
    }

    return allSuccess;
}

bool EditorSession::SaveScene(const core::UUID& sceneID) {
    auto sceneDoc = GetSceneDocument(sceneID);
    if (!sceneDoc) {

        return false;
    }

    if (sceneDoc->GetFilePath().empty()) {

        return false;
    }

    return sceneDoc->Save();
}

bool EditorSession::SaveSceneAs(const core::UUID& sceneID, const std::string& newPath) {
    auto sceneDoc = GetSceneDocument(sceneID);
    if (!sceneDoc) {

        return false;
    }

    return sceneDoc->SaveAs(newPath);
}

bool EditorSession::SaveAllScenes() {
    bool allSuccess = true;
    for (const auto& pair : m_OpenScenes) {
        auto sceneDoc = pair.second;
        if (sceneDoc->IsDirty()) {
            if (sceneDoc->GetFilePath().empty()) {

                allSuccess = false;
                continue;
            }

            if (!sceneDoc->Save()) {

                allSuccess = false;
            }
        }
    }
    return allSuccess;
}

std::shared_ptr<SceneDocument> EditorSession::GetSceneDocument(const core::UUID& sceneID) const {
    auto it = m_OpenScenes.find(sceneID);
    if (it != m_OpenScenes.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<SceneDocument> EditorSession::GetSceneDocumentByPath(const std::string& scenePath) const {
    for (const auto& pair : m_OpenScenes) {
        if (pair.second->GetFilePath() == scenePath) {
            return pair.second;
        }
    }
    return nullptr;
}

std::vector<std::shared_ptr<SceneDocument>> EditorSession::GetOpenScenes() const {
    std::vector<std::shared_ptr<SceneDocument>> scenes;
    for (const auto& pair : m_OpenScenes) {
        scenes.push_back(pair.second);
    }
    return scenes;
}

bool EditorSession::IsSceneOpen(const core::UUID& sceneID) const {
    return m_OpenScenes.find(sceneID) != m_OpenScenes.end();
}

bool EditorSession::IsSceneOpenByPath(const std::string& scenePath) const {
    return GetSceneDocumentByPath(scenePath) != nullptr;
}

bool EditorSession::SetActiveScene(const core::UUID& sceneID) {
    auto sceneDoc = GetSceneDocument(sceneID);
    if (!sceneDoc) {

        return false;
    }

    m_ActiveSceneID = sceneID;

    return true;
}

std::shared_ptr<SceneDocument> EditorSession::GetActiveSceneDocument() const {
    if (!m_ActiveSceneID.IsValid()) {
        return nullptr;
    }
    return GetSceneDocument(m_ActiveSceneID);
}

std::shared_ptr<Scene> EditorSession::GetActiveScene() const {
    auto sceneDoc = GetActiveSceneDocument();
    if (sceneDoc) {
        return sceneDoc->GetScene();
    }
    return nullptr;
}

void EditorSession::AddSceneDocument(std::shared_ptr<SceneDocument> sceneDoc) {
    m_OpenScenes[sceneDoc->GetID()] = sceneDoc;

}

void EditorSession::RemoveSceneDocument(const core::UUID& sceneID) {
    auto it = m_OpenScenes.find(sceneID);
    if (it != m_OpenScenes.end()) {

        m_OpenScenes.erase(it);
    }
}

}
}
