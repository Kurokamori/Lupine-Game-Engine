#include "lupine/core/SceneInstance.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/core/Component.hpp"
#include "lupine/platform/Platform.hpp"
#include "lupine/logger/Logger.hpp"
#include <nlohmann/json.hpp>
#include "lupine/core/Serialization.hpp"

namespace lupine {
namespace core {

SceneInstance::SceneInstance()
    : Node("SceneInstance"), m_IsLoaded(false), m_AutoReload(false) {
}

SceneInstance::SceneInstance(const std::string& name)
    : Node(name), m_IsLoaded(false), m_AutoReload(false) {
}

SceneInstance::~SceneInstance() {
    ClearInstance();
}

void SceneInstance::RegisterProperties() {
    Node::RegisterProperties();

    RegisterProperty<std::string>("scene_reference", core::PropertyType::String,
        [this]() { return m_SceneFilePath; },
        [this](const std::string& value) {
            if (value != m_SceneFilePath) {
                SetSceneReference(value);
            }
        });

    RegisterProperty<bool>("auto_reload", core::PropertyType::Bool,
        [this]() { return m_AutoReload; },
        [this](const bool& value) { m_AutoReload = value; });
}

nlohmann::json SceneInstance::Serialize() const {
    nlohmann::json json = Node::Serialize();

    json["scene_reference"] = m_SceneFilePath;
    json["auto_reload"] = m_AutoReload;

    return json;
}

void SceneInstance::Deserialize(const nlohmann::json& json) {
    RegisterProperties();

    Node::Deserialize(json);

    if (json.contains("scene_reference")) {
        std::string sceneRef = json["scene_reference"].get<std::string>();
        if (!sceneRef.empty()) {
            SetSceneReference(sceneRef);
        }
    }

    if (json.contains("auto_reload")) {
        m_AutoReload = json["auto_reload"].get<bool>();
    }
}

bool SceneInstance::SetSceneReference(const std::string& filepath) {
    if (filepath.empty()) {

        return false;
    }

    ClearInstance();

    m_SceneFilePath = filepath;

    return LoadAndInstantiate();
}

bool SceneInstance::ReloadScene() {
    if (m_SceneFilePath.empty()) {

        return false;
    }

    ClearInstance();

    return LoadAndInstantiate();
}

void SceneInstance::ClearInstance() {

    auto children = GetChildren();
    for (const auto& child : children) {
        RemoveChild(child);
    }

    m_InstancedRoot = nullptr;
    m_IsLoaded = false;
}

void SceneInstance::OnReady() {
    Node::OnReady();

    if (m_InstancedRoot) {
        m_InstancedRoot->OnReady();
    }
}

void SceneInstance::OnDestroy() {
    ClearInstance();
    Node::OnDestroy();
}

void SceneInstance::OnProcess(float deltaTime) {
    Node::OnProcess(deltaTime);

}

void SceneInstance::OnPhysicsProcess(float deltaTime) {
    Node::OnPhysicsProcess(deltaTime);

}

void SceneInstance::OnRender() {
    Node::OnRender();

}

bool SceneInstance::LoadAndInstantiate() {
    if (m_SceneFilePath.empty()) {

        return false;
    }

    auto result = platform::FileSystem::ReadFile(m_SceneFilePath);
    if (!result.success) {

        return false;
    }

    try {
        nlohmann::json sceneJson = nlohmann::json::parse(result.data);

        if (!sceneJson.contains("root")) {

            return false;
        }

        const auto& rootJson = sceneJson["root"];
        m_InstancedRoot = CloneNodeTree(rootJson);

        if (!m_InstancedRoot) {

            return false;
        }

        AddChild(m_InstancedRoot);

        m_IsLoaded = true;

        return true;

    } catch (const std::exception& e) {

        return false;
    }
}

std::shared_ptr<Node> SceneInstance::CloneNodeTree(const nlohmann::json& nodeJson) {
    if (!nodeJson.contains("type")) {

        return nullptr;
    }

    std::string typeName = nodeJson["type"].get<std::string>();

    auto node = std::dynamic_pointer_cast<Node>(
        core::TypeRegistry::GetInstance().CreateInstance(typeName));

    if (!node) {

        return nullptr;
    }

    nlohmann::json modifiedJson = nodeJson;

    if (modifiedJson.contains("uuid")) {
        modifiedJson.erase("uuid");
    }

    StripUUIDs(modifiedJson);

    node->Deserialize(modifiedJson);

    return node;
}

void SceneInstance::StripUUIDs(nlohmann::json& json) {

    if (json.contains("uuid")) {
        json.erase("uuid");
    }

    if (json.contains("components")) {
        if (json["components"].is_array()) {
            for (auto& componentJson : json["components"]) {
                if (componentJson.contains("uuid")) {
                    componentJson.erase("uuid");
                }
            }
        }
    }

    if (json.contains("children")) {
        if (json["children"].is_array()) {
            for (auto& childJson : json["children"]) {
                StripUUIDs(childJson);
            }
        }
    }
}

}
}
