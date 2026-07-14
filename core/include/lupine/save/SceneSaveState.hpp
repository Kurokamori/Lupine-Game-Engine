#pragma once

#include "lupine/save/SaveData.hpp"
#include <string>
#include <nlohmann/json.hpp>

namespace lupine {

namespace core {
    class Node;
    class Scene;
}

namespace save {

/**
 * SceneSaveState - capture and restore live scene-node state for saves.
 *
 * This bridges the save toolkit to the engine's existing property/reflection
 * serialization (Node::Serialize / Node::Deserialize). Rather than persisting the
 * whole scene, games tag the nodes whose runtime state should survive a save with
 * a group (default "persistent"); CaptureGroup serializes exactly those nodes and
 * RestoreGroup writes their state back onto the matching live nodes.
 *
 * Nodes are matched on restore by UUID first (stable across sessions because the
 * UUID is serialized), then by scene-relative path as a fallback. Captured nodes
 * that no longer exist are skipped. All helpers are static and side-effect free
 * apart from the explicit restore mutations.
 */
class SceneSaveState {
public:
    // Default group whose members are captured/restored.
    static const std::string kDefaultGroup;

    // Serialize every node of `group` in `scene` into a JSON array of records,
    // each holding the node's uuid, scene path, and full serialized state.
    static nlohmann::json CaptureGroup(core::Scene* scene, const std::string& group);

    // Restore previously captured group records onto matching live nodes. Returns
    // the number of nodes successfully restored.
    static int RestoreGroup(core::Scene* scene, const nlohmann::json& captured);

    // Capture / restore a single node's full serialized state (components + child
    // subtree). RestoreNode returns false if `data` is not a usable object.
    static nlohmann::json CaptureNode(core::Node* node);
    static bool RestoreNode(core::Node* node, const nlohmann::json& data);

    // Convenience: capture group state into / restore it from a SaveData under
    // `key`. RestoreGroupFrom returns the number of nodes restored.
    static void CaptureGroupInto(SaveData& data, const std::string& key, core::Scene* scene,
                                 const std::string& group = kDefaultGroup);
    static int RestoreGroupFrom(const SaveData& data, const std::string& key, core::Scene* scene,
                                const std::string& group = kDefaultGroup);
};

} // namespace save
} // namespace lupine
