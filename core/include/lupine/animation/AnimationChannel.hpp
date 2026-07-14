#pragma once

#include "lupine/animation/AnimationTypes.hpp"
#include "lupine/scripting/NodeRef.hpp"
#include <nlohmann/json.hpp>
#include <string>

namespace lupine {
namespace core { class Node; }
namespace animation {

/**
 * Resolve a track's target-node string relative to a player root.
 *
 * The animated node does NOT have to be a child of the AnimationPlayer:
 *   "" / "."     -> the player root itself
 *   "%Unique"    -> unique-name lookup, scoped scene-wide
 *   "/Abs/Path"  -> absolute path from the scene root (anywhere in the scene)
 *   "Rel/Path"   -> relative to the player root (descendants; reusable across instances)
 * Returns an invalid handle when the target cannot be found (callers no-op).
 */
scripting::NodeRef ResolveTargetNode(core::Node* root, const std::string& path);

/**
 * UUID-first resolution: when `nodeUuid` is a non-empty hex UUID and a node with
 * that id exists anywhere in the player's scene, that node is returned regardless
 * of where it has been moved/renamed in the tree. Otherwise this falls back to the
 * path rules of ResolveTargetNode(root, path) so prefab instances (whose nodes get
 * fresh UUIDs) and legacy clips keep working.
 */
scripting::NodeRef ResolveTargetNode(core::Node* root, const std::string& path,
                                     const std::string& nodeUuid);

/**
 * Channel read/write against a resolved node.
 *
 * Generalizes components::Tween's channel logic so any part of the animation
 * runtime can read or write a transform channel ("position_2d", "rotation_3d",
 * ...) or an arbitrary component/node property by name.
 *
 * The animation runtime works in array form for vectors ([x,y], [r,g,b,a]), but
 * component properties store vectors/colors as JSON objects ({"x":..}, {"r":..}).
 * The `type` argument lets these helpers convert at the boundary so property
 * channels round-trip correctly; transform channels ignore it.
 */

// Read the current value of a channel on `ref` (as array/number). Returns null
// when ref is invalid.
nlohmann::json ReadChannel(const scripting::NodeRef& ref, const std::string& channel,
                           TrackValueType type = TrackValueType::Float);

// Write a value (array/number) to a channel on `ref`. No-op when ref is invalid.
void WriteChannel(scripting::NodeRef& ref, const std::string& channel,
                  const nlohmann::json& value, TrackValueType type = TrackValueType::Float);

} // namespace animation
} // namespace lupine
