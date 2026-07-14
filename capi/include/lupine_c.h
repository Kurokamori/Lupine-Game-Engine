/**
 * @file lupine_c.h
 * @brief Lupine Engine C API - Main umbrella header
 *
 * Include this header to get access to all Lupine Engine C API functionality.
 *
 * @example basic_example.c
 * @code
 * #include <lupine_c.h>
 * #include <stdio.h>
 *
 * int main(void) {
 *     // Initialize engine
 *     LCResult result = lc_init();
 *     if (result != LC_SUCCESS) {
 *         printf("Failed to initialize: %s/n", lc_get_last_error());
 *         return 1;
 *     }
 *
 *     // Use math functions
 *     LCVec3 a = lc_vec3(1.0f, 2.0f, 3.0f);
 *     LCVec3 b = lc_vec3(4.0f, 5.0f, 6.0f);
 *     LCVec3 c = lc_vec3_add(a, b);
 *
 *     printf("Result: %.2f, %.2f, %.2f/n", c.x, c.y, c.z);
 *
 *     // Shutdown engine
 *     lc_shutdown();
 *     return 0;
 * }
 * @endcode
 */

#ifndef LUPINE_C_H
#define LUPINE_C_H

/* Core functionality */
#include "core/lc_core.h"

/* Math types and operations */
#include "math/lc_math.h"

/* Node system and hierarchy */
#include "core/lc_node.h"

/* Generic reflection / object-model bridge (type registry, property get/set by
 * name, component introspection, CallMethod, JSON serialization) */
#include "core/lc_reflection.h"

/* Signal / event system */
#include "core/lc_signal.h"

/* Interface types (capability contracts: query/verify interfaces, find every
 * node/archetype implementing one) */
#include "core/lc_interface.h"

/* Scene management */
#include "core/lc_scene.h"

/* Project file and settings */
#include "core/lc_project.h"

/* Rendering components */
#include "rendering/lc_camera.h"
#include "rendering/lc_material.h"
#include "rendering/lc_debug_draw.h"
#include "components/lc_rendering.h"
#include "components/lc_light.h"
#include "components/lc_primitive_mesh3d.h"
#include "components/lc_line2d.h"
#include "components/lc_particles2d.h"
#include "components/lc_particles3d.h"
#include "components/lc_tilemap2d.h"
#include "components/lc_world_environment.h"
#include "components/lc_parallax_background.h"
#include "components/lc_parallax_layer.h"

/* Animation components */
#include "components/lc_animated_sprite2d.h"
#include "components/lc_animated_sprite3d.h"
#include "components/lc_skeletal_mesh3d.h"
#include "components/lc_animation_player.h"
#include "components/lc_animation_tree.h"

/* Media playback components */
#include "components/lc_gif_player.h"
#include "components/lc_video_player.h"

/* Physics */
#include "physics/lc_physics2d.h"
#include "physics/lc_physics3d.h"
#include "physics/lc_collision2d.h"
#include "physics/lc_collision3d.h"
#include "physics/lc_area_trigger2d.h"
#include "physics/lc_area_trigger3d.h"
#include "physics/lc_character_controller2d.h"
#include "physics/lc_character_controller3d.h"
#include "physics/lc_physics_query2d.h"
#include "physics/lc_physics_query3d.h"
#include "physics/lc_raycast2d.h"
#include "physics/lc_raycast3d.h"
#include "physics/lc_shapecast2d.h"
#include "physics/lc_shapecast3d.h"

/* MultiMesh */
#include "components/lc_multimesh.h"

/* Scene organization */
#include "components/lc_node_scatter.h"
#include "components/lc_ysort.h"
#include "components/lc_shape2d.h"

/* Navigation */
#include "navigation/lc_navigation.h"
#include "navigation/lc_navigation3d.h"

/* Networking / Multiplayer */
#include "network/lc_network.h"
#include "network/lc_rpc.h"
#include "network/lc_replication.h"

/* Scene Instantiation */
#include "core/lc_scene_instance.h"
#include "core/lc_prefab.h"

/* Input */
#include "input/lc_input.h"

/* Window / Display server */
#include "platform/lc_display.h"

/* Audio */
#include "audio/lc_audio.h"

/* Localization */
#include "i18n/lc_localization.h"

/* UI */
#include "ui/lc_ui.h"
#include "ui/lc_uicontrol.h"
#include "ui/lc_button.h"
#include "ui/lc_panel.h"
#include "ui/lc_color_rect.h"
#include "ui/lc_image2d.h"
#include "ui/lc_progress_bar.h"
#include "ui/lc_label3d.h"
#include "ui/lc_container.h"
#include "ui/lc_button3d.h"
#include "ui/lc_texture_button.h"
#include "ui/lc_toggle_button.h"
#include "ui/lc_radio_button.h"
#include "ui/lc_checkbox.h"
#include "ui/lc_checklist.h"
#include "ui/lc_radiolist.h"
#include "ui/lc_padding_container.h"
#include "ui/lc_nine_slice_panel.h"
#include "ui/lc_dock_container.h"
#include "ui/lc_layout_slot.h"
#include "ui/lc_scroll_container.h"
#include "ui/lc_slider.h"
#include "ui/lc_line_edit.h"
#include "ui/lc_spin_box.h"
#include "ui/lc_text_edit.h"
#include "ui/lc_tab_container.h"
#include "ui/lc_item_list.h"
#include "ui/lc_dropdown.h"
#include "ui/lc_popup_menu.h"
#include "ui/lc_rich_text_label.h"
#include "ui/lc_tree.h"
#include "ui/lc_theme.h"

/* Utility */
#include "utility/lc_utility.h"

/* File system and Virtual File System */
#include "io/lc_filesystem.h"

/* Asset Loading */
#include "asset/lc_asset.h"

/* Asynchronous priority-streaming loader (archetype instances + definitions) */
#include "asset/lc_async_asset.h"

/* Save-game toolkit (slots, schema versioning, migration) */
#include "save/lc_savegame.h"

/* Profiler instrumentation (frame timing, zones, counters, capture export) */
#include "profiling/lc_profiler.h"

#endif /* LUPINE_C_H */
