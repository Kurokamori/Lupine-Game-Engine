#include "lupine/save/SceneSaveState.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/core/UUID.hpp"

namespace lupine {
namespace save {

const std::string SceneSaveState::kDefaultGroup = "persistent";

nlohmann::json SceneSaveState::CaptureNode(core::Node* node) {
    if (!node) {
        return nlohmann::json::object();
    }
    return node->Serialize();
}

bool SceneSaveState::RestoreNode(core::Node* node, const nlohmann::json& data) {
    if (!node || !data.is_object()) {
        return false;
    }
    node->Deserialize(data);
    return true;
}

nlohmann::json SceneSaveState::CaptureGroup(core::Scene* scene, const std::string& group) {
    nlohmann::json records = nlohmann::json::array();
    if (!scene) {
        return records;
    }

    std::vector<core::Node*> nodes = scene->GetNodesInGroup(group);
    for (core::Node* node : nodes) {
        if (!node) {
            continue;
        }
        nlohmann::json record = nlohmann::json::object();
        record["uuid"] = node->GetUUID().ToString();
        record["path"] = node->GetPath();
        record["state"] = node->Serialize();
        records.push_back(std::move(record));
    }
    return records;
}

int SceneSaveState::RestoreGroup(core::Scene* scene, const nlohmann::json& captured) {
    if (!scene || !captured.is_array()) {
        return 0;
    }

    int restored = 0;
    for (const nlohmann::json& record : captured) {
        if (!record.is_object() || !record.contains("state")) {
            continue;
        }

        std::shared_ptr<core::Node> target;

        if (record.contains("uuid") && record["uuid"].is_string()) {
            core::UUID uuid = core::UUID::FromString(record["uuid"].get<std::string>());
            if (uuid.IsValid()) {
                target = scene->FindNodeByUUID(uuid);
            }
        }
        if (!target && record.contains("path") && record["path"].is_string()) {
            target = scene->FindNode(record["path"].get<std::string>());
        }

        if (target && RestoreNode(target.get(), record["state"])) {
            ++restored;
        }
    }
    return restored;
}

void SceneSaveState::CaptureGroupInto(SaveData& data, const std::string& key, core::Scene* scene,
                                      const std::string& group) {
    data.SetJson(key, CaptureGroup(scene, group));
}

int SceneSaveState::RestoreGroupFrom(const SaveData& data, const std::string& key, core::Scene* scene,
                                     const std::string& group) {
    (void)group;
    nlohmann::json captured = data.GetJson(key, nlohmann::json::array());
    return RestoreGroup(scene, captured);
}

} // namespace save
} // namespace lupine
