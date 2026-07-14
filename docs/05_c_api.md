# 05 — C API (`lc_*`)

A stable C ABI for driving Lupine from C or any FFI-capable language. Sources: `capi/include/` (umbrella `lupine_c.h`), examples in `capi/examples/`.

## Conventions

- **Include**: `#include <lupine_c.h>` (pulls in all domain headers).
- **Naming**: every symbol prefixed `lc_`; exported via `LC_API` (extern "C").
- **Handles**: opaque pointers — `LCSceneHandle`, `LCNodeHandle`, `LCComponentHandle`, `LCProjectHandle`, etc. No direct struct access.
- **Returns**: functions return `LCResult` (`LC_SUCCESS`, `LC_ERROR_INVALID_HANDLE`, `LC_ERROR_NOT_FOUND`, `LC_ERROR_FILE_NOT_FOUND`, `LC_ERROR_SCENE_LOAD_FAILED`, …). Results delivered via out-params.
- **Errors**: `const char* lc_get_last_error(void)` — engine-owned, don't free.
- **Memory**:
  - Simple getters (`lc_node_get_name`) return engine-owned `const char*` — **don't free**.
  - Reflection/JSON output (`lc_node_get_property_json`) returns heap memory — **free with `lc_free()`**.
  - Arrays use caller buffer + count: `lc_node_get_children(node, buf, max, &count)`.
- **Reflection bridge**: dynamic properties / method args / typed values pass as UTF-8 **JSON strings** (`{"r":1,"g":0,"b":0,"a":1}`).

## Header map (`capi/include/`)

| Dir | Domain |
|---|---|
| `core/` | `lc_core` (init/log/fps), `lc_node`, `lc_scene`, `lc_scene_instance`, `lc_prefab`, `lc_reflection`, signals, `lc_project` |
| `components/` | Typed component APIs (sprite, mesh, light, particles, tilemap, ysort, animated sprites, …) |
| `physics/` | 2D/3D bodies, collisions, area triggers, character controllers, queries |
| `rendering/` | `lc_camera`, `lc_material`, debug draw |
| `ui/` | `lc_uicontrol`, button, panel, label, line/text edit, containers, `lc_layout_slot`, tree, dropdown, … |
| `audio/` | playback/streaming/buses |
| `input/` | input events & actions |
| `platform/` | `lc_display` (window: title/fullscreen/vsync/mouse mode/size) |
| `network/` | `lc_network`, `lc_rpc`, `lc_replication` |
| `navigation/` | navmesh/pathfinding |
| `i18n/` | localization |
| `io/` | `lc_filesystem` (`lc_fs_*` real FS, `lc_vfs_*` virtual FS) |
| `asset/` | asset loading |
| `math/` | `lc_math` (Vec2/3/4, Quat, Mat4, color, helpers) |
| `save/` | `lc_savegame` slots/versioning |
| `profiling/` | frame timing/zones/counters/export |
| `utility/` | misc |

## Core calls

```c
// Lifecycle
LCResult lc_init(void);
LCResult lc_shutdown(void);
bool     lc_is_initialized(void);
const char* lc_get_last_error(void);
void     lc_log(LCLogLevel level, const char* message);
float    lc_get_fps(void);
void     lc_free(void* ptr);            // free heap strings/arrays returned to you

// Scene
LCResult lc_scene_create(const char* name, LCSceneHandle* out);
LCResult lc_scene_load(const char* filepath, LCSceneHandle* out);
LCResult lc_scene_save(LCSceneHandle s, const char* filepath);
LCResult lc_scene_get_root(LCSceneHandle s, LCNodeHandle* out);
LCResult lc_scene_find_node(LCSceneHandle s, const char* path, LCNodeHandle* out);
LCResult lc_scene_initialize(LCSceneHandle s);
LCResult lc_scene_update(LCSceneHandle s, float dt);
LCResult lc_scene_physics_update(LCSceneHandle s, float dt);
LCResult lc_scene_render(LCSceneHandle s);

// Nodes
LCResult lc_node_create(LCNodeType type, const char* name, LCNodeHandle* out);
LCResult lc_node_destroy(LCNodeHandle n);
const char* lc_node_get_name(LCNodeHandle n);
LCResult lc_node_add_child(LCNodeHandle parent, LCNodeHandle child);
LCResult lc_node_get_child_by_name(LCNodeHandle n, const char* name, LCNodeHandle* out);
LCResult lc_node_find(LCNodeHandle n, const char* path, LCNodeHandle* out);

// Components (reflection)
LCResult lc_node_add_component_by_type(LCNodeHandle n, const char* type, LCComponentHandle* out);
LCResult lc_node_get_property_json(LCNodeHandle n, const char* prop, char** out_json);  // lc_free
LCResult lc_node_set_property_json(LCNodeHandle n, const char* prop, const char* json);

// Transforms
void lc_node2d_set_position(LCNodeHandle n, LCVec2 pos);
void lc_node2d_set_rotation(LCNodeHandle n, float radians);
void lc_node3d_set_position(LCNodeHandle n, LCVec3 pos);
void lc_node3d_set_rotation(LCNodeHandle n, LCQuat q);          // quaternion
void lc_node3d_set_rotation_euler(LCNodeHandle n, LCVec3 pyr);  // radians
```

> **Units: the radian/quaternion calls are the RAW layer.** The functions above use **radians** for 2D and **radians-euler / quaternion** for 3D — they bind straight to the C++ object model, matching the raw `.scene` file (`02`).
>
> **Degree convenience setters/getters** mirror them for a degree-based workflow (same underlying state, just converted): `lc_node2d_set_rotation_degrees` / `_get_rotation_degrees` / `_rotate_degrees` / `_get_global_rotation_degrees` / `_set_global_rotation_degrees`, and 3D `lc_node3d_set_rotation_euler_degrees` / `_get_rotation_euler_degrees` / `_rotate_euler_degrees` / `_get_global_rotation_euler_degrees` / `_set_global_rotation_euler_degrees`. These match the **scripting** API's degree convention (`04`). Pick one convention per call site; don't mix radians and degrees on the same node.
>
> **World (global) rotation in 3D**: `lc_node3d_get_global_rotation` (quaternion) and `lc_node3d_get_global_rotation_euler` (radians) read the node's orientation composed through every `Node3D` ancestor; `lc_node3d_set_global_rotation` / `_set_global_rotation_euler` write the local rotation that lands on the requested world orientation.

## Scripting-API parity

The C-API exposes the **entire** scripting `ScriptAPI` surface, so a new language can be bound purely through `lc_*`. The pieces that mirror the script globals (not tied to a single component) live in these headers:

- **Game state** (`core/lc_core.h`): `lc_set_game_paused` / `lc_is_game_paused`, `lc_get_delta_time`, `lc_get_time`, `lc_get_frame_count`, plus the existing `lc_set_time_scale` / `lc_quit` / `lc_cmdline_arg_*`.
- **Globals** (`core/lc_core.h`): `lc_get_global_int/float/string/bool`, `lc_set_global_*`, and JSON-valued `lc_get_global_value` / `lc_set_global_value`.
- **Math/utility** (`math/lc_math.h`): `lc_random_range`, `lc_random_range_int`, `lc_move_toward`, `lc_smoothstep`, `lc_inverse_lerp`, `lc_remap` (alongside the existing lerp/clamp/wrap/ease/colour helpers).
- **Node transforms & tree** (`core/lc_node.h`): `lc_node2d/3d_translate`, `lc_node2d_rotate` / `lc_node3d_rotate_euler`, `lc_node2d/3d_set_global_position`, `lc_node2d_set_global_rotation`, `lc_node3d_get/set_global_rotation(_euler[_degrees])`, `lc_node3d_get_forward/right/up`, `lc_node2d_look_at`, `lc_node2d/3d_distance_to_point/_node`, `lc_node2d/3d_move_toward`, `lc_node_get_scene/_root/_singleton/_find_by_uuid`, `lc_node_queue_free(_deferred)/_free`, `lc_node_add_sibling`, `lc_node_reparent`, `lc_node_duplicate`, `lc_node_get_component_in_children/_in_parent`.
- **Groups** (`core/lc_node.h`): `lc_node_add_to_group`, `lc_node_remove_from_group`, `lc_node_is_in_group`, `lc_node_get_groups_json`, `lc_node_get_nodes_in_group` (+ existing `lc_node_get_first_node_in_group`).
- **Scene & instantiation** (`core/lc_scene.h`, `lc_scene_instance.h`, `lc_reflection.h`): `lc_scene_change`, `lc_scene_reload`, `lc_scene_get_current`, `lc_scene_get_current_path`, `lc_instantiate_scene`, `lc_instantiate_prefab`, `lc_create_node_child`.
- **Mixer audio** (`audio/lc_audio.h`): `lc_audio_play(_3d/_scheduled/_scheduled_3d)`, `lc_audio_stop/_pause/_resume`, `lc_audio_source_is_playing/_is_finished`, `lc_audio_set_bus_volume/_muted`, `lc_audio_get_bus_volume`, `lc_audio_is_bus_muted` (fire-and-forget playback by UUID, on top of the existing component/bus/DSP APIs).
- **Input** (`input/lc_input.h`): `lc_input_get_action_strength`, `lc_input_get_vector`, gamepad introspection/rumble (`lc_input_is_gamepad_connected`, `lc_input_get_gamepad_count/_name`, `lc_input_get_connected_gamepad_ids`, `lc_input_set/stop_gamepad_vibration`), `lc_input_get_active_contexts`, `lc_input_get_player_gamepads`, `lc_input_get_action_glyphs`, and the `lc_input_event_is_action(_pressed/_released)` matchers.
- **Signals / timers / tweens** (`core/lc_signal.h`, `utility/lc_utility.h`): `lc_node_call_deferred`; `lc_tween_create` / `lc_tween_list`; `lc_timer_create_on_node`, `lc_timer_set/get_repeat_count`, `lc_timer_list`.
- **Localization / file / save / archetype**: table/args-aware `lc_localization_tr_format_in_table` & `lc_localization_tr_plural_full`; `lc_vfs_is_sandboxed_path`, `lc_vfs_append_text`; `lc_savegame_get_last_error`; synchronous `lc_archetype_load_instance` + `lc_archetype_invalidate_caches`.

**Custom-component rendering (`on_draw`) and editor-only gizmo drawing** are not part of `lc_*`: they require an active render context and a component lifecycle, so they live in the companion **extension ABI** (`extension/include/lupine/lupine_extension_interface.h`, `LupineHostInterface`: `render_draw_*` / `debug_draw_*`). A new language authoring drawable/scripted components uses that ABI for the component vtable and `lc_*` for everything else. (ABI v3 adds `debug_draw_rect_2d` for full `EditorDraw*` parity.)

## Typed vs reflection

- **Typed APIs** (e.g. `lc_sprite2d_*`, `lc_camera_*`) — compile-checked, per component.
- **Reflection** (`lc_node_add_component_by_type`, `lc_*_get/set_property_json`, method invoke) — generic, for tooling/bindings over any component.

The stackable **`CameraEffect*`** components (camera blur/glow/outline/color grade/…) have no typed C-API of their own — drive them through reflection: `lc_node_add_component_by_type(cam, "CameraEffectBlur", &fx)`, then `lc_component_set_property_json(fx, "radius", "12.0")` and `lc_component_set_enabled(fx, false)`. The reflection sweep test covers them automatically.

## UI layout (`ui/lc_uicontrol.h`)

The full `UIControl` layout surface: size (`lc_uicontrol_get/set_width|height|size`), constraints
(`custom_min_size` / `custom_max_size`), `layout_mode`, `anchor_min` / `anchor_max`,
`offset_min` / `offset_max`, `anchor_preset` (+ `lc_uicontrol_apply_anchor_preset`),
`grow_direction_h` / `grow_direction_v`, the container size flags, `lc_uicontrol_get_rect`, and
the mouse-filter / focus calls.

Two things worth knowing before you call any of it:

- **The setters carry their side effects.** `lc_uicontrol_set_anchor_min()` switches the control
  into `Anchors` mode and marks the preset `Custom` for you; `lc_uicontrol_set_width()` on a
  container child dirties the parent container. (Previously they were bare property writes:
  setting an anchor on a default control was a *silent no-op*, because a `Position`-mode control
  never reads its anchors, and nothing said so.)
- **Anchors and offsets are screen-oriented.** `anchor.y = 0` is the parent's **top** edge, and a
  positive offset moves its edge **down**. `lc_uicontrol_get_rect` returns the rect in canvas
  coordinates, where the canvas is **Y-up** and the rect's `y` is its **bottom** edge. See
  [12_ui_and_theme.md](12_ui_and_theme.md#the-canvas-is-y-up-read-this-first).

**Removed:** `lc_uicontrol_get_pivot` / `lc_uicontrol_set_pivot`. The engine's `pivot` property
was never read by the layout solver — every UI control is center-pivoted — so the pair only
round-tripped a value that affected nothing. There is no replacement, because there was never
any behaviour. Delete the calls.

`ui/lc_layout_slot.h` exposes **`LayoutSlot`**, the per-child attached-property component that
`DockContainer` and `Stack` read (`dock_side`, `alignment`, `z_index`, `match_parent`,
`ignore_layout`).

## Project & runtime

`core/lc_project.h` exposes `LCProjectHandle` with all ProjectSettings get/set + splash. The C-API drives nodes/scenes/IO for **embedding, scripting, and tooling**; for a full windowed main loop use the engine runtime (`lupine_runtime`) or the optional runtime C-API wrapper. Window control for headless/embedded is in `platform/lc_display.h`.

## Skeleton

```c
#include <lupine_c.h>
int main(void) {
    if (lc_init() != LC_SUCCESS) return 1;
    LCSceneHandle scene;
    lc_scene_load("res://scenes/main.scene", &scene);
    lc_scene_initialize(scene);
    for (int i = 0; i < 600; ++i) {
        lc_scene_update(scene, 1.0f/60.0f);
        lc_scene_physics_update(scene, 1.0f/60.0f);
        lc_scene_render(scene);
    }
    lc_shutdown();
    return 0;
}
```
