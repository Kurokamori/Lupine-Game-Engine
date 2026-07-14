#pragma once

#include <nlohmann/json.hpp>

namespace lupine {
namespace core { class Node; }
namespace animation {

/**
 * Editor preview helpers.
 *
 * Apply in-memory animation data to the live scene without going through the
 * components, so the editor can scrub unsaved clip edits, and capture/restore the
 * affected channel values so previewing is non-destructive. The EditorBridge wraps
 * these for the PyQt animation tools.
 */

// Sample a clip (as JSON, .animclip schema) at an absolute time and apply its pose
// to `root`. Returns the affected targets ([{node, channel, valueType}]).
nlohmann::json PreviewClip(core::Node* root, const nlohmann::json& clipJson, float time);

// Read the current value of each target ([{node, channel, valueType}]) relative to
// `root`. Returns [{node, channel, valueType, value}] for a later RestorePose.
nlohmann::json CapturePose(core::Node* root, const nlohmann::json& targetsJson);

// Write captured values ([{node, channel, valueType, value}]) back to the scene.
void RestorePose(core::Node* root, const nlohmann::json& capturedJson);

} // namespace animation
} // namespace lupine
