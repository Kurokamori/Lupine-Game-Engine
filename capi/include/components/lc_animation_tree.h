/**
 * @file lc_animation_tree.h
 * @brief Lupine Engine C API - AnimationTree component
 *
 * Parameter-driven animation graph (state machines + blend trees) that samples a
 * linked AnimationPlayer's clips and composites layered, masked poses.
 */

#ifndef LUPINE_CAPI_ANIMATION_TREE_H
#define LUPINE_CAPI_ANIMATION_TREE_H

#include "core/lc_core.h"
#include "core/lc_node.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Create an AnimationTree component. */
LC_API LCResult lc_animation_tree_create(const char* name, LCComponentHandle* out_component);

/* ============================================================================
 * Configuration
 * ============================================================================ */

LC_API LCResult lc_animation_tree_set_graph_path(LCComponentHandle component, const char* path);
LC_API LCResult lc_animation_tree_set_animation_player(LCComponentHandle component, const char* node_path);
LC_API LCResult lc_animation_tree_set_root_node(LCComponentHandle component, const char* node_path);
LC_API LCResult lc_animation_tree_set_active(LCComponentHandle component, bool active);
LC_API LCResult lc_animation_tree_is_active(LCComponentHandle component, bool* out_active);
LC_API LCResult lc_animation_tree_reload_graph(LCComponentHandle component);

/* ============================================================================
 * Parameters
 * ============================================================================ */

LC_API LCResult lc_animation_tree_set_float(LCComponentHandle component, const char* name, float value);
LC_API LCResult lc_animation_tree_set_int(LCComponentHandle component, const char* name, int value);
LC_API LCResult lc_animation_tree_set_bool(LCComponentHandle component, const char* name, bool value);
LC_API LCResult lc_animation_tree_set_trigger(LCComponentHandle component, const char* name);
LC_API LCResult lc_animation_tree_get_float(LCComponentHandle component, const char* name, float* out_value);
LC_API LCResult lc_animation_tree_get_int(LCComponentHandle component, const char* name, int* out_value);
LC_API LCResult lc_animation_tree_get_bool(LCComponentHandle component, const char* name, bool* out_value);

/* ============================================================================
 * State machine
 * ============================================================================ */

LC_API LCResult lc_animation_tree_travel(LCComponentHandle component, const char* state, int layer);
LC_API LCResult lc_animation_tree_get_current_state(LCComponentHandle component, int layer, char* out_state, int state_size);

#ifdef __cplusplus
}
#endif

#endif /* LUPINE_CAPI_ANIMATION_TREE_H */
