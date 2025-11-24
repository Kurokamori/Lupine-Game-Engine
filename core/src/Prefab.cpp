#include "lupine/core/Prefab.hpp"
#include "lupine/platform/Platform.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/core/Component.hpp"
#include <nlohmann/json.hpp>

namespace lupine {
namespace core {

Prefab::Prefab()
    : m_Name("Prefab"), m_RootNodeData(nullptr) {
}

Prefab::Prefab(const std::string& name)
    : m_Name(name), m_RootNodeData(nullptr) {
}

Prefab::~Prefab() {
}

void Prefab::RegisterProperties() {
    RegisterProperty<std::string>("name", core::PropertyType::String,
        [this]() { return m_Name; },
        [this](const std::string& value) { m_Name = value; });
}

nlohmann::json Prefab::Serialize() const {
    nlohmann::json json = core::ISerializable::Serialize();

    if (m_RootNodeData) {
        json["root_node"] = *m_RootNodeData;
    }

    return json;
}

void Prefab::Deserialize(const nlohmann::json& json) {
    RegisterProperties();
    core::ISerializable::Deserialize(json);

    if (json.contains("root_node")) {
        m_RootNodeData = std::make_shared<nlohmann::json>(json["root_node"]);
    }
}

bool Prefab::Load(const std::string& filepath) {
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

bool Prefab::Save(const std::string& filepath) {
    m_FilePath = filepath;
    return Save();
}

bool Prefab::Save() {
    if (m_FilePath.empty()) {

        return false;
    }

    if (!m_RootNodeData) {

        return false;
    }

    RegisterProperties();

    nlohmann::json json = Serialize();
    json["lupine_prefab_version"] = "0.1.0";
    json["type"] = GetTypeName();

    std::string jsonStr = json.dump(2);
    auto result = platform::FileSystem::WriteFile(m_FilePath, jsonStr);

    if (!result.success) {

        return false;
    }

    return true;
}

void Prefab::CreateFromNode(std::shared_ptr<Node> rootNode) {
    if (!rootNode) {

        return;
    }

    nlohmann::json nodeJson = rootNode->Serialize();
    m_RootNodeData = std::make_shared<nlohmann::json>(nodeJson);

}

std::shared_ptr<Node> Prefab::Instantiate() {
    if (!m_RootNodeData) {

        return nullptr;
    }

    try {

        std::shared_ptr<Node> instance = CloneNodeTree(*m_RootNodeData);

        if (instance) {

        }

        return instance;
    } catch (const std::exception& e) {

        return nullptr;
    }
}

std::shared_ptr<Node> Prefab::InstantiateAsChild(std::shared_ptr<Node> parent) {
    if (!parent) {

        return nullptr;
    }

    std::shared_ptr<Node> instance = Instantiate();
    if (instance) {
        parent->AddChild(instance);

    }

    return instance;
}

void Prefab::Clear() {
    m_RootNodeData = nullptr;

}

std::shared_ptr<Node> Prefab::CloneNodeTree(const nlohmann::json& nodeJson) {
    if (!nodeJson.contains("type")) {

        return nullptr;
    }

    std::string typeName = nodeJson["type"].get<std::string>();
    auto node = std::dynamic_pointer_cast<Node>(
        core::TypeRegistry::GetInstance().CreateInstance(typeName));

    if (!node) {

        return nullptr;
    }

    node->Deserialize(nodeJson);

    RegenerateUUIDs(node);

    return node;
}

void Prefab::RegenerateUUIDs(std::shared_ptr<Node> node) {
    if (!node) return;

    const_cast<core::UUID&>(node->GetUUID()) = core::UUID();

    for (const auto& component : node->GetComponents()) {
        const_cast<core::UUID&>(component->GetUUID()) = core::UUID();
    }

    for (const auto& child : node->GetChildren()) {
        RegenerateUUIDs(child);
    }
}

}
}
