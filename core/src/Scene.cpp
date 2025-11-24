#include "lupine/core/Scene.hpp"
#include "lupine/core/Component.hpp"
#include "lupine/platform/Platform.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/audio/AudioManager.hpp"
#include <nlohmann/json.hpp>

namespace lupine {
namespace core {

Scene::Scene()
    : m_Name("Scene"), m_Initialized(false), m_Active(true), m_IsInEditor(false), m_IsShuttingDown(false) {

    std::string rootName = m_Name.empty() ? "Root" : m_Name;
    m_Root = std::make_shared<Node>(rootName);
    m_Root->SetScene(this);
}

Scene::Scene(const std::string& name)
    : m_Name(name), m_Initialized(false), m_Active(true), m_IsInEditor(false), m_IsShuttingDown(false) {

    std::string rootName = m_Name.empty() ? "Root" : m_Name;
    m_Root = std::make_shared<Node>(rootName);
    m_Root->SetScene(this);
}

Scene::~Scene() {
    Shutdown();
}

void Scene::RegisterProperties() {
    RegisterProperty<std::string>("name", core::PropertyType::String,
        [this]() { return m_Name; },
        [this](const std::string& value) { m_Name = value; });
}

nlohmann::json Scene::Serialize() const {
    nlohmann::json json = core::ISerializable::Serialize();

    if (m_Root) {
        json["root"] = m_Root->Serialize();
    }

    return json;
}

void Scene::Deserialize(const nlohmann::json& json) {
    core::ISerializable::Deserialize(json);

    if (json.contains("root")) {
        const auto& rootJson = json["root"];
        if (rootJson.contains("type")) {
            std::string typeName = rootJson["type"].get<std::string>();
            auto root = std::dynamic_pointer_cast<Node>(
                core::TypeRegistry::GetInstance().CreateInstance(typeName));
            if (root) {
                root->Deserialize(rootJson);
                SetRoot(root);
            }
        }
    }
}

bool Scene::Load(const std::string& filepath) {
    m_FilePath = filepath;

    auto result = platform::FileSystem::ReadFile(filepath);
    if (!result.success) {

        return false;
    }

    try {
        nlohmann::json json = nlohmann::json::parse(result.data);

        RegisterProperties();

        Deserialize(json);

        return true;
    } catch (const std::exception& e) {

        return false;
    }
}

bool Scene::Save(const std::string& filepath) {
    m_FilePath = filepath;
    return Save();
}

bool Scene::Save() {
    if (m_FilePath.empty()) {

        return false;
    }

    RegisterProperties();

    if (m_Root) {
        RegisterNodePropertiesRecursive(m_Root);
    }

    nlohmann::json json = Serialize();
    json["lupine_scene_version"] = "0.1.0";

    std::string jsonStr = json.dump(2);
    auto result = platform::FileSystem::WriteFile(m_FilePath, jsonStr);

    if (!result.success) {

        return false;
    }

    return true;
}

void Scene::RegisterNodePropertiesRecursive(std::shared_ptr<Node> node) {
    if (!node) return;

    node->RegisterProperties();

    for (const auto& component : node->GetComponents()) {
        component->RegisterProperties();
    }

    for (const auto& child : node->GetChildren()) {
        RegisterNodePropertiesRecursive(child);
    }
}

void Scene::SetRoot(std::shared_ptr<Node> root) {
    if (m_Root) {
        m_Root->SetScene(nullptr);
    }

    m_Root = root;

    if (m_Root) {
        m_Root->SetScene(this);
        if (m_Initialized) {
            m_Root->OnReady();
        }
    }
}

void Scene::AddNode(std::shared_ptr<Node> node) {
    if (!node) {

        return;
    }

    if (!m_Root) {
        SetRoot(node);
    } else {
        m_Root->AddChild(node);
    }
}

void Scene::RemoveNode(std::shared_ptr<Node> node) {
    if (!node) return;

    if (m_Root == node) {
        m_Root->SetScene(nullptr);
        m_Root = nullptr;
    } else if (m_Root) {

        node->GetParent()->RemoveChild(node);
    }
}

std::shared_ptr<Node> Scene::FindNode(const std::string& path) const {
    if (!m_Root) return nullptr;

    if (path.empty() || path == "/") {
        return m_Root;
    }

    if (path[0] == '/') {

        std::string rootName = "/" + m_Root->GetName();
        if (path == rootName) {
            return m_Root;
        }

        if (path.find(rootName + "/") == 0) {
            std::string relativePath = path.substr(rootName.length() + 1);
            return m_Root->FindNode(relativePath);
        }

        return m_Root->FindNode(path.substr(1));
    }

    return m_Root->FindNode(path);
}

std::shared_ptr<Node> Scene::FindNodeByUUID(const core::UUID& uuid) const {
    if (!m_Root) return nullptr;

    std::shared_ptr<Node> result = nullptr;
    FindNodeByUUIDRecursive(m_Root, uuid, result);
    return result;
}

void Scene::FindNodeByUUIDRecursive(const std::shared_ptr<Node>& node,
                                     const core::UUID& uuid,
                                     std::shared_ptr<Node>& result) const {
    if (!node || result) return;

    if (node->GetUUID() == uuid) {
        result = node;
        return;
    }

    for (const auto& child : node->GetChildren()) {
        FindNodeByUUIDRecursive(child, uuid, result);
        if (result) return;
    }
}

void Scene::Initialize() {
    if (m_Initialized) return;

    if (m_Root) {
        m_Root->OnReady();
    }

    m_Initialized = true;
}

void Scene::Shutdown() {
    if (!m_Initialized) return;

    m_IsShuttingDown = true;

    if (m_Root) {
        m_Root->OnDestroy();
    }

    m_Initialized = false;
}

void Scene::Update(float deltaTime) {
    if (!m_Active || !m_Initialized) return;

    audio::AudioManager::GetInstance().Update(deltaTime);

    if (m_Root) {

        m_Root->OnProcess(deltaTime);

    }

}

void Scene::PhysicsUpdate(float deltaTime) {
    if (!m_Active || !m_Initialized) return;

    if (m_Root) {

        m_Root->OnPhysicsProcess(deltaTime);

    }

}

void Scene::Render() {
    if (!m_Active || !m_Initialized) return;

    if (m_Root) {

        m_Root->OnRender();

    }

}

void Scene::ProcessInput(float deltaTime) {
    if (!m_Active || !m_Initialized) return;

    if (m_Root) {

        m_Root->OnInput(deltaTime);

    }

}

}
}

