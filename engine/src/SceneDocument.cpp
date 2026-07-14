#include "lupine/engine/SceneDocument.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/core/Prefab.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/Component.hpp"
#include "lupine/platform/Platform.hpp"
#include "lupine/logger/Logger.hpp"

namespace lupine {
namespace engine {

SceneDocument::SceneDocument()
    : m_ID(core::UUID()), m_IsDirty(false) {
}

SceneDocument::SceneDocument(std::shared_ptr<Scene> scene)
    : m_ID(core::UUID()), m_Scene(scene), m_IsDirty(false) {
    if (m_Scene) {
        m_FilePath = m_Scene->GetFilePath();
    }
}

SceneDocument::SceneDocument(const std::string& scenePath)
    : m_ID(core::UUID()), m_FilePath(scenePath), m_IsDirty(false) {
}

SceneDocument::~SceneDocument() {
}

void SceneDocument::SetScene(std::shared_ptr<Scene> scene) {
    m_Scene = scene;
    if (m_Scene && m_FilePath.empty()) {
        m_FilePath = m_Scene->GetFilePath();
    }
    m_IsDirty = true;
}

void SceneDocument::SetFilePath(const std::string& filePath) {
    m_FilePath = filePath;
    if (m_Scene) {

    }
}

std::string SceneDocument::GetDisplayName() const {
    if (!m_FilePath.empty()) {

        std::string filename = platform::Path::GetFilename(m_FilePath);
        std::string nameWithoutExt = platform::Path::GetFilenameWithoutExtension(filename);
        return nameWithoutExt;
    } else if (m_Scene) {
        return m_Scene->GetName();
    }
    return "Untitled";
}

std::shared_ptr<SceneDocument> SceneDocument::CreateNew(const std::string& sceneName) {
    auto scene = std::make_shared<Scene>(sceneName);
    if (!scene) {

        return nullptr;
    }

    auto document = std::make_shared<SceneDocument>(scene);
    document->m_IsDirty = true;

    return document;
}

std::shared_ptr<SceneDocument> SceneDocument::Open(const std::string& scenePath) {
    if (!platform::FileSystem::Exists(scenePath)) {

        return nullptr;
    }

    auto scene = std::make_shared<Scene>();
    if (!scene->Load(scenePath)) {

        return nullptr;
    }

    auto document = std::make_shared<SceneDocument>(scene);
    document->m_FilePath = scenePath;
    document->m_IsDirty = false;

    return document;
}

std::shared_ptr<SceneDocument> SceneDocument::OpenPrefab(const std::string& prefabPath) {
    if (!platform::FileSystem::Exists(prefabPath)) {

        return nullptr;
    }

    core::Prefab prefab;
    if (!prefab.Load(prefabPath)) {

        return nullptr;
    }

    std::shared_ptr<core::Node> root = prefab.InstantiateForEditing();
    if (!root) {

        return nullptr;
    }

    std::string sceneName = prefab.GetName();
    if (sceneName.empty()) {
        sceneName = root->GetName();
    }

    auto scene = std::make_shared<Scene>(sceneName);
    scene->SetRoot(root);

    auto document = std::make_shared<SceneDocument>(scene);
    document->m_FilePath = prefabPath;
    document->m_Kind = DocumentKind::Prefab;
    document->m_IsDirty = false;

    return document;
}

namespace {
// Mirrors Scene::RegisterNodePropertiesRecursive (private there): ensures every
// node and component re-registers its property accessors so the subsequent
// Serialize() captures the live values, exactly as a normal scene save does.
void RegisterNodePropertiesRecursive(const std::shared_ptr<core::Node>& node) {
    if (!node) return;

    node->RegisterProperties();
    for (const auto& component : node->GetComponents()) {
        component->RegisterProperties();
    }
    for (const auto& child : node->GetChildren()) {
        RegisterNodePropertiesRecursive(child);
    }
}
} // namespace

bool SceneDocument::SaveAsPrefab(const std::string& filePath) {
    if (!m_Scene) {

        return false;
    }

    std::shared_ptr<core::Node> root = m_Scene->GetRoot();
    if (!root) {

        return false;
    }

    RegisterNodePropertiesRecursive(root);

    core::Prefab prefab(root->GetName());
    prefab.CreateFromNode(root);
    return prefab.Save(filePath);
}

bool SceneDocument::Save() {
    if (m_FilePath.empty()) {

        return false;
    }

    if (!m_Scene) {

        return false;
    }

    bool saved = (m_Kind == DocumentKind::Prefab)
        ? SaveAsPrefab(m_FilePath)
        : m_Scene->Save(m_FilePath);

    if (!saved) {

        return false;
    }

    m_IsDirty = false;

    return true;
}

bool SceneDocument::SaveAs(const std::string& newPath) {
    if (!m_Scene) {

        return false;
    }

    bool saved = (m_Kind == DocumentKind::Prefab)
        ? SaveAsPrefab(newPath)
        : m_Scene->Save(newPath);

    if (!saved) {

        return false;
    }

    m_FilePath = newPath;
    m_IsDirty = false;

    return true;
}

bool SceneDocument::Close() {
    if (m_Scene && m_Scene->IsInitialized()) {
        m_Scene->Shutdown();
    }

    return true;
}

}
}
