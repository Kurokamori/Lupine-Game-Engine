#pragma once

#include "lupine/core/Core.hpp"
#include "lupine/core/Node.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <vector>

namespace lupine {
namespace core {
namespace instancing {

/**
 * Shared machinery behind SceneInstance* and PrefabInstance*.
 *
 * Both families answer the same question - "reproduce this authored subtree here, without
 * copying it into my scene file" - and differ only in where the subtree comes from and
 * whether the instance node *hosts* the subtree or *becomes* its root:
 *
 *   SceneInstance*  (child semantics)  the referenced scene's root is added as a child.
 *   PrefabInstance* (adopt semantics)  the instance node takes on the prefab root's
 *                                      components, children and groups directly, so the
 *                                      instance node *is* the object. Scripts that address an
 *                                      object by node name, and `%`-scoped lookups reaching out
 *                                      of the object, then behave exactly as they would if the
 *                                      subtree had been pasted into the scene by hand.
 */

/**
 * A single per-instance property override.
 *
 * `path` is a slash-separated chain of child names relative to the instanced root; empty
 * targets the root itself. `component` names the component type to patch; when empty the
 * override applies to the node's own properties instead.
 */
struct PropertyOverride {
    std::string path;
    std::string component;
    nlohmann::json properties = nlohmann::json::object();
};

/** Which key the referenced file stores its root node under. */
enum class SourceKind {
    Scene,   // .scene  -> "root"
    Prefab   // .prefab -> "root_node"
};

std::vector<PropertyOverride> ParseOverrides(const nlohmann::json& json);
nlohmann::json SerializeOverrides(const std::vector<PropertyOverride>& overrides);

/**
 * Read a .scene or .prefab off the pack file or the real filesystem (resolving res://) and
 * hand back its root node JSON.
 */
bool LoadRootNodeJson(const std::string& filepath, SourceKind kind, nlohmann::json& outRootJson);

/**
 * Patch a root-node JSON tree in place with the given overrides. Overrides that name a path or
 * component that does not exist are reported and skipped, never silently dropped.
 */
void ApplyOverrides(nlohmann::json& rootJson, const std::vector<PropertyOverride>& overrides,
                    const std::string& sourceLabel);

/** Instantiate a node subtree from JSON with fresh UUIDs (child semantics). */
std::shared_ptr<Node> CloneNodeTree(const nlohmann::json& nodeJson);

/**
 * Adopt semantics: give `target` the components, children and groups of `rootJson`.
 *
 * `authoredProperties` are the properties the *instance* declared in its own scene file; they
 * win over the source root's values, so a prefab supplies defaults and the instance overrides
 * them. The source root's name and UUID are never adopted - the instance keeps its own
 * identity, and connections inside the source that targeted its root are remapped onto the
 * instance.
 *
 * `authoredGroups` are merged with the source root's groups rather than replacing them, so an
 * instance can add a level-specific group on top of the prefab's own.
 */
void AdoptRootNode(const std::shared_ptr<Node>& target,
                   const nlohmann::json& rootJson,
                   const nlohmann::json& authoredProperties,
                   const std::vector<std::string>& authoredGroups);

} // namespace instancing
} // namespace core
} // namespace lupine
