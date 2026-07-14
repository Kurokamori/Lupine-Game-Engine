#pragma once

#include <memory>
#include <string>
#include <unordered_map>

namespace lupine {
namespace core {

class Node;

/**
 * Old UUID (string form) -> new UUID (string form).
 */
using UUIDRemapTable = std::unordered_map<std::string, std::string>;

/**
 * UUID renumbering for cloned node trees.
 *
 * Any time a serialized subtree is instantiated more than once - a prefab instance, a scene
 * instance - the copies must not share the source's UUIDs. But nodes and components address
 * their declarative signal connections by target UUID, so renumbering alone silently breaks
 * every connection inside the copy: the stale UUID either misses (connection dropped) or, if
 * the source scene is also loaded, resolves to the *other* copy's node and fires the wrong
 * handler.
 *
 * Renumbering and connection remapping therefore always travel together. Both Prefab and
 * SceneInstance go through these functions.
 */
namespace uuidremap {

/**
 * Assign fresh UUIDs to a node, its components, and its whole subtree, recording each
 * old->new mapping into outRemap.
 */
void ReassignRecursive(const std::shared_ptr<Node>& node, UUIDRemapTable& outRemap);

/**
 * Rewrite pending signal-connection target UUIDs for a node, its components, and its whole
 * subtree through the given old->new map. Targets outside the map (references to nodes beyond
 * the cloned subtree) are left untouched.
 */
void RemapConnectionTargetsRecursive(const std::shared_ptr<Node>& node, const UUIDRemapTable& remap);

/**
 * Renumber a freshly cloned subtree and remap its intra-subtree connections in one step.
 */
void RegenerateSubtree(const std::shared_ptr<Node>& node);

/**
 * Renumber a subtree a node has *adopted* (PrefabInstance), keeping the node's own UUID.
 *
 * The instance's UUID belongs to the scene that placed it - other nodes' connections address it
 * by that UUID - so it must survive. Everything adopted underneath (the node's components and
 * its whole child subtree) is freshly renumbered. Connections authored inside the source that
 * targeted the source's root are redirected onto the instance via sourceRootUuid, which may be
 * empty when the source recorded no UUID.
 */
void RegenerateAdoptedSubtree(const std::shared_ptr<Node>& node, const std::string& sourceRootUuid);

} // namespace uuidremap
} // namespace core
} // namespace lupine
