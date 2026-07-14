#include "lupine/core/NodeInstancing.hpp"
#include "lupine/core/Component.hpp"
#include "lupine/core/UUIDRemap.hpp"
#include "lupine/platform/Platform.hpp"
#include "lupine/platform/PackFile.hpp"
#include "lupine/asset/Asset.hpp"
#include "lupine/logger/Logger.hpp"
#include <sstream>

namespace lupine {
namespace core {
namespace instancing {

namespace {

const char* RootKeyFor(SourceKind kind) {
    return kind == SourceKind::Scene ? "root" : "root_node";
}

std::vector<std::string> SplitPath(const std::string& path) {
    std::vector<std::string> segments;
    std::stringstream stream(path);
    std::string segment;
    while (std::getline(stream, segment, '/')) {
        if (!segment.empty()) {
            segments.push_back(segment);
        }
    }
    return segments;
}

/** Resolve a slash path of child names against a node JSON tree. Returns null when unmatched. */
nlohmann::json* ResolveNodeJson(nlohmann::json& rootJson, const std::string& path) {
    nlohmann::json* current = &rootJson;

    for (const std::string& segment : SplitPath(path)) {
        if (!current->contains("children") || !(*current)["children"].is_array()) {
            return nullptr;
        }

        nlohmann::json* match = nullptr;
        for (nlohmann::json& childJson : (*current)["children"]) {
            if (childJson.contains("properties") &&
                childJson["properties"].contains("name") &&
                childJson["properties"]["name"].is_string() &&
                childJson["properties"]["name"].get<std::string>() == segment) {
                match = &childJson;
                break;
            }
        }

        if (!match) {
            return nullptr;
        }
        current = match;
    }

    return current;
}

/**
 * Merge override values into a component's JSON.
 *
 * Script components carry their authored @export values under "export_properties" - that is the
 * map ScriptComponent stashes and replays once the script has been parsed - while native
 * components read everything through the reflected "properties" map. Writing both keeps a single
 * override schema working for either kind without the caller having to know which it is.
 */
void MergeComponentProperties(nlohmann::json& componentJson, const nlohmann::json& values) {
    for (nlohmann::json::const_iterator it = values.begin(); it != values.end(); ++it) {
        componentJson["properties"][it.key()] = it.value();

        if (componentJson.contains("export_properties")) {
            componentJson["export_properties"][it.key()] = it.value();
        }
    }
}

bool IsScriptComponent(const nlohmann::json& componentJson) {
    return componentJson.contains("script_path") ||
           (componentJson.contains("type") &&
            componentJson["type"].is_string() &&
            componentJson["type"].get<std::string>().find("ScriptComponent") != std::string::npos);
}

} // namespace

std::vector<PropertyOverride> ParseOverrides(const nlohmann::json& json) {
    std::vector<PropertyOverride> overrides;

    if (!json.contains("overrides") || !json["overrides"].is_array()) {
        return overrides;
    }

    for (const nlohmann::json& entry : json["overrides"]) {
        if (!entry.is_object() || !entry.contains("properties") || !entry["properties"].is_object()) {
            continue;
        }

        PropertyOverride override;
        if (entry.contains("path") && entry["path"].is_string()) {
            override.path = entry["path"].get<std::string>();
        }
        if (entry.contains("component") && entry["component"].is_string()) {
            override.component = entry["component"].get<std::string>();
        }
        override.properties = entry["properties"];
        overrides.push_back(override);
    }

    return overrides;
}

nlohmann::json SerializeOverrides(const std::vector<PropertyOverride>& overrides) {
    nlohmann::json json = nlohmann::json::array();

    for (const PropertyOverride& override : overrides) {
        nlohmann::json entry;
        entry["path"] = override.path;
        if (!override.component.empty()) {
            entry["component"] = override.component;
        }
        entry["properties"] = override.properties;
        json.push_back(entry);
    }

    return json;
}

bool LoadRootNodeJson(const std::string& filepath, SourceKind kind, nlohmann::json& outRootJson) {
    if (filepath.empty()) {
        return false;
    }

    std::string fileContents;

    platform::PackFileSystem& packFS = platform::PackFileSystem::Instance();
    if (packFS.isPackMode() && packFS.exists(filepath)) {
        fileContents = packFS.readFileAsString(filepath);
        if (fileContents.empty()) {
            LOG_ERROR(LogCategory::Core, "Instancing: '{}' is empty in the pack file", filepath);
            return false;
        }
    } else {
        // Asset::ResolveAssetPath, not the VFS directly: res:// is mounted to the *engine's*
        // resources in an editor session, so resolving it through the VFS looks for project
        // scenes and prefabs under the engine install and finds nothing. The AssetDatabase
        // knows the project root.
        std::string realPath = asset::Asset::ResolveAssetPath(filepath);
        if (realPath.empty()) {
            realPath = filepath;
        }

        auto result = platform::FileSystem::ReadFile(realPath);
        if (!result.success) {
            LOG_ERROR(LogCategory::Core, "Instancing: cannot read '{}' (resolved to '{}'): {}",
                      filepath, realPath, result.error);
            return false;
        }
        fileContents = result.data;
    }

    try {
        nlohmann::json json = nlohmann::json::parse(fileContents);

        const char* rootKey = RootKeyFor(kind);
        if (!json.contains(rootKey)) {
            LOG_ERROR(LogCategory::Core, "Instancing: '{}' has no '{}' node", filepath, rootKey);
            return false;
        }

        outRootJson = json[rootKey];
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR(LogCategory::Core, "Instancing: failed to parse '{}': {}", filepath, e.what());
        return false;
    }
}

void ApplyOverrides(nlohmann::json& rootJson, const std::vector<PropertyOverride>& overrides,
                    const std::string& sourceLabel) {
    for (const PropertyOverride& override : overrides) {
        nlohmann::json* targetJson = ResolveNodeJson(rootJson, override.path);
        if (!targetJson) {
            LOG_ERROR(LogCategory::Core,
                      "Instancing: override path '{}' does not exist in '{}'",
                      override.path, sourceLabel);
            continue;
        }

        if (override.component.empty()) {
            for (nlohmann::json::const_iterator it = override.properties.begin();
                 it != override.properties.end(); ++it) {
                (*targetJson)["properties"][it.key()] = it.value();
            }
            continue;
        }

        bool matched = false;
        if (targetJson->contains("components") && (*targetJson)["components"].is_array()) {
            for (nlohmann::json& componentJson : (*targetJson)["components"]) {
                if (!componentJson.contains("type") || !componentJson["type"].is_string()) {
                    continue;
                }
                if (componentJson["type"].get<std::string>() != override.component) {
                    continue;
                }

                if (IsScriptComponent(componentJson) && !componentJson.contains("export_properties")) {
                    componentJson["export_properties"] = nlohmann::json::object();
                }

                MergeComponentProperties(componentJson, override.properties);
                matched = true;
                break;
            }
        }

        if (!matched) {
            LOG_ERROR(LogCategory::Core,
                      "Instancing: override targets component '{}' at path '{}', which '{}' does not have",
                      override.component, override.path.empty() ? "<root>" : override.path, sourceLabel);
        }
    }
}

std::shared_ptr<Node> CloneNodeTree(const nlohmann::json& nodeJson) {
    if (!nodeJson.contains("type") || !nodeJson["type"].is_string()) {
        return nullptr;
    }

    const std::string typeName = nodeJson["type"].get<std::string>();
    std::shared_ptr<Node> node = std::dynamic_pointer_cast<Node>(
        core::TypeRegistry::GetInstance().CreateInstance(typeName));

    if (!node) {
        LOG_ERROR(LogCategory::Core, "Instancing: unknown node type '{}'", typeName);
        return nullptr;
    }

    // Deserialize with the source UUIDs intact, then renumber the subtree and rewrite its
    // connection targets through the same old->new map (see UUIDRemap).
    node->Deserialize(nodeJson);
    core::uuidremap::RegenerateSubtree(node);

    return node;
}

void AdoptRootNode(const std::shared_ptr<Node>& target,
                   const nlohmann::json& rootJson,
                   const nlohmann::json& authoredProperties,
                   const std::vector<std::string>& authoredGroups) {
    if (!target) {
        return;
    }

    // Source-root properties act as defaults: apply only the ones this instance did not author
    // itself. "name" is never adopted - the instance's name is its identity in the level, and
    // objectives and object-to-object wiring address it by that name.
    if (rootJson.contains("properties") && rootJson["properties"].is_object()) {
        nlohmann::json defaults = nlohmann::json::object();

        for (nlohmann::json::const_iterator it = rootJson["properties"].begin();
             it != rootJson["properties"].end(); ++it) {
            if (it.key() == "name") {
                continue;
            }
            if (authoredProperties.is_object() && authoredProperties.contains(it.key())) {
                continue;
            }
            defaults[it.key()] = it.value();
        }

        if (!defaults.empty()) {
            nlohmann::json wrapper;
            wrapper["properties"] = defaults;
            target->ISerializable::Deserialize(wrapper);
        }
    }

    if (rootJson.contains("components") && rootJson["components"].is_array()) {
        for (const nlohmann::json& componentJson : rootJson["components"]) {
            if (!componentJson.contains("type") || !componentJson["type"].is_string()) {
                continue;
            }

            const std::string typeName = componentJson["type"].get<std::string>();
            std::shared_ptr<Component> component = std::dynamic_pointer_cast<Component>(
                core::TypeRegistry::GetInstance().CreateInstance(typeName));

            if (!component) {
                LOG_ERROR(LogCategory::Core, "Instancing: unknown component type '{}'", typeName);
                continue;
            }

            component->RegisterProperties();
            component->Deserialize(componentJson);
            target->AddComponent(component);
        }
    }

    if (rootJson.contains("children") && rootJson["children"].is_array()) {
        for (const nlohmann::json& childJson : rootJson["children"]) {
            std::shared_ptr<Node> child = std::dynamic_pointer_cast<Node>(
                core::TypeRegistry::GetInstance().CreateInstance(
                    childJson.value("type", std::string())));

            if (!child) {
                continue;
            }

            child->Deserialize(childJson);
            target->AddChild(child);
        }
    }

    // Groups are merged, not replaced: the source declares what the object always is
    // ("sim_objects", "torches") and the instance adds what it is in this level ("west_torches").
    if (rootJson.contains("groups") && rootJson["groups"].is_array()) {
        for (const nlohmann::json& groupJson : rootJson["groups"]) {
            if (groupJson.is_string()) {
                target->AddToGroup(groupJson.get<std::string>());
            }
        }
    }

    for (const std::string& group : authoredGroups) {
        target->AddToGroup(group);
    }

    // The instance keeps the UUID its scene authored; everything adopted underneath it gets
    // fresh ones, with connections that pointed at the source root redirected onto the instance.
    const std::string sourceRootUuid = rootJson.value("uuid", std::string());
    core::uuidremap::RegenerateAdoptedSubtree(target, sourceRootUuid);
}

} // namespace instancing
} // namespace core
} // namespace lupine
