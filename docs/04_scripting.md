# 04 — Scripting (Lua / MRuby / MicroPython)

Sources: `core/include/lupine/scripting/{ScriptingCore,ScriptAPI,NodeRef}.hpp`, `core/src/scripting/{Lua,MRuby,MicroPython}Environment.cpp`, `core/src/ScriptComponent.cpp`.

## Languages

| Lang | Ext | Comment prefix | Host |
|---|---|---|---|
| Lua (sol2) | `.lua` | `--` | `LuaEnvironment` (always available) |
| MRuby | `.rb` | `#` | `MRubyEnvironment` (gated `LUPINE_HAS_MRUBY`) |
| MicroPython | `.py` | `#` | `MicroPythonEnvironment` (gated) |

One shared VM per language; each script instance gets its own namespace. A script runs by attaching a `ScriptComponent` (`script_path` = `res://…`) to a node. The API is symmetric across all three languages — only syntax differs.

## Annotation directives (parsed from comments)

Prefix with the language's comment marker (`--@` for Lua, `#@` for Ruby/Python).

| Directive | Meaning |
|---|---|
| `@component_class "Name"` | Declares this script as a named component class (inheritable, instantiable) |
| `@extends_component "Base"` | Inherit from another component (e.g. `CharacterController2D`) |
| `@export name type [default]` | Editor-visible/serialized property (see Export properties below) |
| `@export_group "Name"` … `@export_group_end` | Group following exports |
| `@struct Name { f:type=default, … }` | Declare an inline struct type usable as an export type |
| `@signal name(args)` | Declare a signal on the component |
| `@tool` | Run this script's lifecycle callbacks in the editor as well as at runtime |

Exported names become **globals** inside the script (read current value, assign to set). Example: `--@export speed float 200.0` → use `speed` directly. Non-scalar exports (vectors, colors, arrays, dictionaries, inline structs) are delivered as native tables/dicts and read back the same way.

### Tool scripts (`@tool`)

By default a script is a **game script**: its lifecycle callbacks (`on_ready`, `on_process`, signal handlers, `on_draw`, …) run **only at runtime** — including the editor's *Play* (which launches a separate runtime process). Opening a scene in the editor never executes them, so editing a scene has no gameplay side effects.

Add `--@tool` (Lua) / `#@tool` (Ruby/Python) anywhere in the file to mark it a **tool script**. Its callbacks then also run while the scene is open in the editor, for build-time helpers that draw gizmos, lay out children, or validate configuration live. Use `is_editor()` inside a tool script to branch on context. A `@tool` base script keeps its derived scripts in tool mode too.

The script *body* (top-level code that defines functions and default values) always executes on load regardless of `@tool`, because the editor needs it to discover `@export` properties — only the lifecycle **callbacks** are gated.

### Inheritance chains

`@extends_component "Base"` may name a built-in component (`Sprite2D`, `Label`, …) **or another custom component**, to any depth — e.g. `FireSprite` extends `MySprite` extends `Sprite2D`. Each level:

- runs the base's lifecycle first, then its own (`on_x` extends; `on_x_override` replaces the base entirely — same for `on_draw`/`on_draw_override`);
- inherits the base's exported properties (merged through the whole chain) and its rendering — extending a renderable base draws the base, and `on_draw` adds to it;
- knows its full ancestry: `component:is_instance_of("Sprite2D")` is true at every level, and `component:get_type_chain()` returns `{"FireSprite","MySprite","Sprite2D"}`.

Inheritance cycles (including a class extending itself) are detected at project scan time and the offending base link is dropped with an error, so instantiation can never recurse forever.

**Native class syntax works too, and a custom component is a real base class.** Every custom component class is registered into the script VM under its own name, so you can inherit from one with the language's own class syntax exactly as you would from a built-in:

```lua
-- sim_object.lua
SimObject = Sprite2D:extend()

-- sim_boulder.lua  — extends the custom SimObject, not a built-in
SimBoulder = SimObject:extend()

-- crystal_boulder.lua — chains another level
CrystalBoulder = SimBoulder:extend()
```

```python
# sim_boulder.py
class SimBoulder(SimObject):
    pass
```

```ruby
# sim_boulder.rb
class SimBoulder < SimObject
end
```

In Python and Ruby the class is defined under its *actual* base, so the language's own `isinstance()` / `is_a?` and MRO/`ancestors` agree with the engine's type chain. Either authoring style (the `@component_class`/`@extends_component` directives, or native class syntax) produces the same component; use whichever reads better.

## Export properties

`@export name type [default] [attributes]`. The parser is shared by all three languages and by archetype definitions (`06`), so the full vocabulary below works everywhere.

### Types

`int` · `float` · `double` · `string` · `bool` · `vec2` · `vec3` · `vec4` · `color` · `quat` · `rect` · `enum` · `nodepath` · `scenepath` · `resource` · `file` · `dir` · `multiline` · `stringarray` · `intarray` · `floatarray` · `array` · `dictionary`. Multi-component defaults (vec/color/quat/rect) are comma-separated with no spaces, or quoted: `vec3 "1,2,3"`.

A type token that names a declared `@struct` makes the field an **inline struct** (edited as a foldout). A token that names a script/component class makes the field a **reference** to a node owning that class.

### Attributes (block-annotation-above is the documented style)

Put attributes in the comment block immediately above the `@export`. Prose lines become the tooltip; tag lines accept both the bracket form `[Tag(args)]` and the legacy `@tag value` form. Trailing tags on the `@export` line also work.

```lua
--- Walk speed in px/s.
--[Header("Movement")]
--[Range(0, 1000, 0.5)]
--[Suffix("px")]
--@export move_speed float 200.0
```

| Attribute | Effect |
|---|---|
| `[Range(min,max,step)]` / `@range "min,max,step"` | Slider range |
| `[ExpRange(min,max)]` · `[Step(s)]` | Exponential range · step override |
| `[Enum(A,B,C)]` / `@enum "A,B,C"` | Enum options (for `enum` type) |
| `[Tooltip("…")]` / `@desc "…"` | Description (prose block does the same) |
| `[Header("…")]` | Section header rendered above the row |
| `[Group("…")]` / `@group "…"` | Property group |
| `[Suffix("m")]` | Unit suffix shown after the value |
| `[HideInInspector]` | Hide from the inspector |
| `[ReadOnly]` | Shown but not editable |
| `[Required]` · `[Unique]` | Archetype validation: must be set · identity field |
| `[Multiline]` · `[ColorNoAlpha]` | Multi-line text · color without alpha |
| `[File("*.png")]` · `[Dir]` · `[Resource("*.ares")]` | Path pickers |
| `[Layers2D]` · `[Layers3D]` | Collision-layer bitmask |
| `[NodeType("Sprite2D")]` | NodePath filtered to a node class |
| `[ArchetypeType("Weapon")]` | Resource filtered to an archetype class |
| `[Custom("widget_id", "config")]` | Render with a custom inspector widget (`config` = JSON string) |

### Inline structs

```lua
--@struct Stats { hp:int=10, mp:int=5, name:string="hero" }
--@export base_stats Stats
```

`base_stats` is a `dictionary` keyed by field name, edited inline as a mini-inspector and delivered to the script as a native table/dict.

### Custom widgets

`[Custom("id")]` resolves `id` against the editor's custom-widget registry. Built-in ids include `slider` and `password`; projects can add their own by dropping a self-registering module in `addons/inspector_widgets/`. The metadata contract is just the string id + JSON config, so it stays decoupled from any specific editor frontend.

### Archetype directives (data classes, see `06`)

`@archetype_class`, `@archetype_extends`, `@archetype_menu`, `@archetype_description` + `@export` for fields (same attribute vocabulary as above). Archetype methods take `self` (resolved fields) as first arg.

## Lifecycle callbacks

Define top-level functions; the engine calls those present. Identical names across languages.

| Callback | Args | When |
|---|---|---|
| `on_awake()` | — | Entered scene, before ready |
| `on_ready()` | — | Tree ready; safe to find other nodes |
| `on_process(delta)` | float | Every frame (respects pause) |
| `on_physics_process(delta)` | float | Physics tick |
| `on_input(delta)` | float | Input polling phase |
| `on_input_event(event)` | event | Per-event dispatch |
| `on_unhandled_input(delta)` | float | Unhandled input |
| `on_late_update(delta)` | float | After `on_process` |
| `on_enter_tree()` / `on_exit_tree()` | — | Added / removed from tree |
| `on_visibility_changed()` | — | Visibility toggled (sets `visible` global) |
| `on_draw()` | — | Gather stage; record runtime-visible geometry with `draw_*` (see below) |
| `on_render()` | — | Render pass; draw **editor-only** gizmos with `editor_draw_*` |
| `get_render_bounds()` | — | Optional; return this component's world-space bounds (see below) |
| `on_destroy()` | — | Node destroyed |

`on_awake`/`on_ready` run once per node when it enters a *live* tree, not only at scene load. A node spawned at runtime — via `create_node`, `instantiate_prefab`, `instantiate_scene`, `add_child`, or reparenting into an already-ready parent — runs its (and its descendants' and components') ready lifecycle immediately on attach. Build a detached subtree first, then add it, and the whole subtree readies in one pass. `on_ready` is idempotent, so re-adding an already-ready node does not call it again.

A custom component that defines `on_draw` (or `on_draw_override`) becomes renderable — the engine treats it as an `IRenderableComponent`, so it is frustum-culled, picks up a selection outline in the editor, and participates in draw ordering. `on_draw_override` replaces a base component's drawing; `on_draw` draws in addition to it (see the inheritance chain section in `06`).

`delta_time` is a global updated before `on_process`/`on_physics_process`/`on_input`/`on_unhandled_input`/`on_late_update` (so callbacks can omit the arg and read `delta_time`).

## Calling convention — what is prefixed and what is bare

This is the single most important thing to get right, and it is **verified against the bindings** (`LuaEnvironment.cpp`, `MRubyEnvironment.cpp`, `MicroPythonEnvironment.cpp`).

### Almost everything is namespaced — including `get_node`/`emit`/`create_*`/`draw_*`

The **entire engine API** lives under one module/table and must be called with the language prefix. This includes the object-model entry points and helpers that older docs wrongly listed as bare: `get_node`, `find_node`, `emit`, `has_node`, `get_self`, `get_singleton`, `instantiate_prefab`, `change_scene`/`add_scene`/`reload_scene`, `queue_free_self`, `create_timer`/`create_tween`/`create_sequence`, and **every** `draw_*` / `editor_draw_*` helper — all are members of the module, **not** bare globals.

| Lang | Prefix | Example |
|---|---|---|
| Lua | `Lupine.` | `Lupine.log_info("hi")`, `Lupine.get_node("%Player")`, `Lupine.emit("died")` |
| MicroPython | `lupine.` (after `import lupine`) | `lupine.log_info("hi")`, `lupine.get_node("%Player")` |
| MRuby | `Lupine.` | `Lupine.log_info("hi")`, `Lupine.get_node("%Player")` |

In the tables below names are written **unprefixed for brevity** — e.g. `log_info(s)` means `Lupine.log_info(s)` (Lua/MRuby) / `lupine.log_info(s)` (MicroPython). Apply the prefix to every one of them, including `get_node`/`find_node`/`emit`.

### The only bare (unprefixed) names

| Bare name | What it is |
|---|---|
| `on_ready`, `on_process`, … | Lifecycle callbacks **you define** (bare because your script declares them) |
| `delta_time` | Frame delta, injected per-instance before each tick callback |
| your `@export` names | Each export becomes a bare global (read the value / assign to set it) |
| `self` | The owning node as a **NodeRef** (use for ops with no ambient form, e.g. `self:get_component(...)`) |
| singleton names, component-type names | Autoload singletons (e.g. `Boot`) and component proxies (`Sprite2D`, …) are bare globals |

### Two ways to act on a node

1. **Ambient API on `self`** — `Lupine.*` functions implicitly target the script's own node: `Lupine.get_position_2d()`, `Lupine.is_action_pressed("jump")`, `Lupine.emit("died")`, `Lupine.queue_free_self()`.
2. **Object model (NodeRef)** — `Lupine.get_node(path)` / `Lupine.find_node(path)` returns a **NodeRef** for *another* node; then call methods on the ref **without** the prefix: `ref:get_position_2d()`, `ref:get_component("Health")`, `ref:connect(...)`. `self` is itself a NodeRef, so `self:get_component(...)` works for ops the ambient API doesn't expose. `%Name` resolves a unique-named node anywhere in its owner scope.

> Rule of thumb: a leading identifier that is a **free function call** → needs `Lupine.`/`lupine.`. A call after `:` (Lua/MRuby) or `.` on a ref object (MicroPython) → it's a NodeRef/ComponentRef **method**, no module prefix.

---

## Ambient API (`Lupine.` Lua/Ruby · `lupine.` Python)

Every function in this section requires the namespace prefix — e.g. `Lupine.log_info(s)` in Lua, `lupine.log_info(s)` in MicroPython. The names are listed unprefixed for brevity.

### Logging / time / misc
`log_info(s)`, `log_warning(s)`, `log_error(s)`, `log_debug(s)`, `get_delta_time()`, `is_editor()`, `request_quit()`, `get_command_line_args()`.

### Random / math
`random_range(min,max)`, `random_range_int(min,max)`, `lerp(a,b,t)`, `clamp(v,min,max)` (+ vector/angle helpers).

### Input (action-based; optional `player` index for local MP)
`is_action_pressed(a[,p])`, `is_action_just_pressed(a[,p])`, `is_action_just_released(a[,p])`, `get_action_strength(a[,p])`, `get_axis(name[,p])`, `get_vector(negX,posX,negY,posY[,p])`.
**Raw**: `is_key_pressed(code)`, `is_mouse_button_pressed(b)`, `get_mouse_position()`, `get_mouse_delta()`, `get_mouse_scroll_delta()`.
**Gamepad**: `is_gamepad_connected(id)`, `get_gamepad_count()`, `get_gamepad_name(id)`, `is_gamepad_button_pressed(b,id)`, `get_gamepad_axis(a,id)`, `set_gamepad_vibration(id,left,right,dur)`.

### Self transform
2D: `get_position_2d()`/`set_position_2d(x,y)`/`translate_2d(dx,dy)`, `get_rotation_2d()`/`set_rotation_2d(deg)`/`rotate_2d(deg)`, `get_scale_2d()`/`set_scale_2d(x,y)`, `get_global_position_2d()`/`set_global_position_2d(x,y)`, `get_global_rotation_2d()`/`set_global_rotation_2d(deg)`, `get_global_scale_2d()`.
3D: same pattern with `_3d`; rotation in **Euler degrees** `(pitch,yaw,roll)` (converted to/from the node's quaternion): `get_position_3d()`/`set_position_3d(x,y,z)`/`translate_3d(dx,dy,dz)`, `get_rotation_3d()`/`set_rotation_3d(p,y,r)`/`rotate_3d(p,y,r)`, `get_scale_3d()`/`set_scale_3d(x,y,z)`, `get_global_position_3d()`/`set_global_position_3d(x,y,z)`, `get_global_rotation_3d()`/`set_global_rotation_3d(p,y,r)`, `get_global_scale_3d()`.

**World vs local.** The plain getters return the node's transform *relative to its parent*; the `global_` forms return (and set) it in **world space**, composed through every ancestor. `get_global_rotation_3d()` is the 3D world-rotation getter — it returns the world orientation as Euler degrees, and `set_global_rotation_3d(p,y,r)` writes the local rotation needed to land on that world orientation (`local = parentWorld⁻¹ * world`). The same set is available on any node handle: `node:get_global_rotation_3d()`, `node:set_global_rotation_3d(p,y,r)`, and so on for position/scale in 2D and 3D.

> **Rotation units in scripting are degrees, everywhere.** Both the 2D and 3D rotation getters/setters (`get/set/rotate_rotation_2d`, `get/set_global_rotation_2d`, and the `_3d` forms) work in **degrees** and convert to/from the engine's internal representation (2D `rotation` float in **radians**; 3D `rotation` **quaternion**). The raw `.scene` *file* still stores radians / quaternion (see `02`/`03`) — only the scripting API is in degrees. Use `deg_to_rad(d)` / `rad_to_deg(r)` if you ever need to bridge to a raw field.

### Scene / node (self)
`get_parent()`, `get_child_count()`, `has_node(path)`, `get_name()`/`set_name(n)`, `is_active()`/`set_active(b)`, `is_visible()`/`set_visible(b)`, `get_node(path)`/`find_node(path)` → NodeRef, `queue_free_self()`, `duplicate_self()`, `instantiate_prefab(path[,parent])`, `change_scene(path)`, `reload_scene()`, `add_scene(path)`, `remove_scene(path)`.

`add_scene(path)` loads a scene additively and `remove_scene(path)` detaches/shuts down a previously added overlay matched by its file path. Both are also available on the scene tree as `tree:add_scene(path)`/`tree:remove_scene(path)`.

The overlay's root is added as a **child of the current scene's root**, so it is a normal part of the scene tree: it is reachable by `get_node`/`find_node` from the scene, it is ticked and rendered by the current scene's traversal, and its 2D/3D world content is picked up by the camera passes (which only ever gather from the current scene). Because overlays are appended as the last children, they draw on top of the scene they overlay. Adding the same path twice creates two overlays, and `remove_scene` removes every overlay matching that path.

An overlay **survives a `change_scene`**: it is re-parented onto the incoming scene with its state intact, which is what makes a persistent pause menu, HUD or transition wipe work. A scene change rebuilds the physics worlds from scratch, so an overlay's physics bodies and colliders are **recreated in the new world** — velocities are carried across, and the bodies are re-placed at their nodes' world transforms — meaning a physics-bearing overlay keeps simulating rather than silently going inert.

### Audio
`play_audio(path,bus,loop,vol)`, `play_audio_3d(path,pos,bus,loop,vol)`, `stop_audio(id)`, `pause_audio(id)`, `resume_audio(id)`, `set_bus_volume(bus,v)`, `get_bus_volume(bus)`, `set_bus_muted(bus,b)`, `get_bus_level(bus)`, `add_bus_effect(bus,type)`, `set_bus_effect_parameter(bus,idx,param,val)`.

### File I/O (sandboxed: `res://`/`user://`/`temp://`)
`file_exists(p)`, `is_file(p)`, `is_dir(p)`, `read_text(p)`, `write_text(p,t)`, `append_text(p,t)`, `read_bytes(p)`, `write_bytes(p,d)`, `remove_file(p)`/`delete_file(p)`, `make_dir(p)`/`ensure_dir(p)` (recursive, create-if-missing), `list_dir(p)`, `file_size(p)`. JSON: `to_json`/`from_json`/`read_json`/`write_json`.

### Save system
`save_game(slot,data,meta)`, `load_game(slot)`, `save_slot_exists(slot)`, `delete_save_slot(slot)`, `list_save_slots()`, `list_save_slot_infos()`, `get_save_slot_info(slot)`, `quick_save_game`/`quick_load_game`, `auto_save_game`, `set_save_format(fmt)`, `set_save_obfuscation_key(k)`.

### Physics queries
2D: `raycast_2d(from,dir,maxDist,mask)`, `raycast_all_2d(...)`, `overlap_circle(c,r,mask)`, `overlap_rect(c,half,mask)`, `circle_cast_2d(from,to,r,mask)`.
3D: `raycast_3d`, `raycast_all_3d`, `overlap_sphere`, `overlap_box`, `sphere_cast_3d`.

### Physics body control (when node has a body)
2D rigid: `get/set_linear_velocity_2d`, `get/set_angular_velocity_2d`, `apply_force_2d`, `apply_impulse_2d`, `apply_torque_2d`, `set_gravity_scale_2d`. 3D analogues with 3D vectors. Character: `move_and_slide_2d(vel)`, `is_character_on_ground_2d()`, `set_character_velocity_2d(vel)` (+ 3D).

### Timers & tweens
`create_timer(delay,cb)`, `create_repeating_timer(interval,cb,count)`, `create_tween(channel,toValue,duration,easing)`.

### Localization / theme / color
`tr(key[,table])`, `tr_fmt(key,args[,table])`, `tr_plural(key,count,args[,table])`, `set_locale`/`get_locale`/`get_locales`/`reload_localization`. `set_theme(path)`, `get_theme_color(type,entry)`, `get_theme_constant`, `set_palette_color`, `set_theme_variable`. `color_from_hex`, `color_to_hex`, `color_from_hsv`, `color_lerp`, `sample_gradient(g,t)`, `sample_curve(c,t)`.

### Game state & globals
`set_time_scale(s)`/`get_time_scale()`, `is_game_paused()`/`set_game_paused(b)`. Cross-script globals: `get_global_int/float/string/bool(name,default)`, `set_global_int/float/string/bool(name,value)`.

---

## NodeRef (returned by `get_node`/`find_node`)

- **Tree**: `get_parent()`, `get_child(name)`, `get_child_at(i)`, `find_node(path)`, `get_children()`, `has_node(path)`, `get_child_count()`, `get_sibling_index()`, `set_sibling_index(i)`, `move_child(child,i)`.
- **Mutate**: `add_child(c)`, `remove_child(c)`, `reparent_to(p)`, `queue_free()`, `queue_free_deferred()`, `free()`, `duplicate()`.
- **Identity/state**: `get_name`/`set_name`, `get_uuid`, `get_path`, `get_type_name`, `is_active`/`set_active`, `is_visible`/`set_visible`, `set_unique_name_in_owner(b)`.
- **Transform**: full 2D/3D position/rotation/scale, local + global (same names as ambient, called on the ref; rotation in degrees, see the caveat above).
- **Camera** (on a `Camera2D`/`CameraUI` node ref; runtime-only, `03`): `camera_set_follow_target(targetRef)`, `camera_shake(amplitude, duration[, frequency])`, `camera_smooth_move_to(x, y[, speed])`.
- **Components**: `get_component(type)`, `get_components(type)`, `add_component(type)`, `remove_component(c)`, `has_component(type)`.
- **Properties** (node + components bag): `has_property(name)`, `get(name)`, `set(name,value)`.
- **Signals**: `emit(sig,...)`, `connect(sig,target,method,flags)`, `disconnect(sig,id)`, `is_connected(sig)`, `add_user_signal(name)`, `get_signal_list()`, `await_signal(sig)` (coroutine-friendly).
- **Groups**: `add_to_group(g)`, `remove_from_group(g)`, `is_in_group(g)`, `get_groups()`.
- **Tweens**: `create_tween(channel,toValue,duration,easing)`, `create_sequence()`.
- **RPC**: `rpc(method,args)`, `rpc_id(peer,method,args)`, `rpc_unreliable(method,args)`, `set_multiplayer_authority(peer)`, `get_multiplayer_authority()`, `is_multiplayer_authority()`, `get_network_id()`.

## ComponentRef (from `get_component`)

`get_type_name()`, `get_name()`, `is_enabled()`/`set_enabled(b)`, `has_property(name)`, `get(name)`/`set(name,value)`, `call(method, args)` (invoke control methods, e.g. `anim:call("play", "run")`), plus signal/event methods.

- **Ancestry**: `is_instance_of(type_name)` returns true if the component is, or derives from, `type_name` — true at every level of a custom inheritance chain down to the built-in base. `get_type_chain()` returns the ordered list from most-derived to least-derived, e.g. `{"FireSprite", "MySprite", "Sprite2D"}`.

This generic get/set is how **stacked camera effects** are driven — no per-effect bindings exist or are needed. `get_component`/`add_component` are **NodeRef methods**, so call them on a node ref (`self`, or a `Lupine.get_node(...)` result). With a `CameraEffect` component on this node:
```lua
local glow = self:get_component("CameraEffectGlow")
glow:set("intensity", 2.0)        -- drive any parameter live
glow:set_enabled(false)            -- toggle the effect on/off
-- add another effect to the stack at runtime
local blur = self:add_component("CameraEffectBlur")
blur:set("radius", 12.0)
```

### UI focus & neighbor navigation

Every UI control (anything deriving from **UIControl**) exposes a keyboard/gamepad focus API through the same generic `call(method, ...)`. A single control holds focus across the whole UI at a time. Methods returning a control hand back the **owning node** (or `nil`/`None` when there is no target); call `get_component(...)` on it to reach the control again.

| Method | Args | Returns | Purpose |
|---|---|---|---|
| `grab_focus` | – | – | Force focus onto this control |
| `release_focus` | – | – | Drop focus if this control holds it |
| `has_focus` | – | bool | Whether this control currently holds focus |
| `get_focused_control` | – | node / nil | Owning node of the globally focused control (call on any control) |
| `clear_focus` | – | – | Clear focus globally |
| `set_focus_mode` / `get_focus_mode` | int | – / int | `0` None, `1` Click, `2` All |
| `can_grab_focus` | – | bool | True when focus mode is All and the node is visible+active |
| `set_focus_neighbor` / `get_focus_neighbor` | side[, path] | – / path | Per-side explicit override (`0` left, `1` top, `2` right, `3` bottom) |
| `set_focus_next` / `get_focus_next` | [path] | – / path | Explicit tab-next override |
| `set_focus_previous` / `get_focus_previous` | [path] | – / path | Explicit tab-previous override |
| `find_valid_focus_neighbor` | side | node / nil | Resolve neighbor (explicit, else nearest control in that direction) |
| `find_next_valid_focus` / `find_prev_valid_focus` | – | node / nil | Resolve tab next/previous (explicit, else scene-tree order, wrapping) |
| `grab_focus_neighbor` | side | node / nil | Resolve neighbor and grab focus on it |
| `grab_focus_next` / `grab_focus_previous` | – | node / nil | Resolve tab next/previous and grab focus on it |

```lua
-- Wire D-pad navigation between menu buttons.
local btn = self:get_component("Button")
if Lupine.is_action_just_pressed("ui_down") then
    btn:call("grab_focus_neighbor", 3)   -- 3 = bottom
elseif Lupine.is_action_just_pressed("ui_accept") and btn:call("has_focus") then
    -- act on the focused button
end

-- Tab order, and reading the global focus holder:
btn:call("grab_focus_next")
local focused_node = btn:call("get_focused_control")
```

The same method names work from MicroPython and mruby via `component.call("grab_focus", ...)`, and from the C-API through the generic reflection call.

## Custom component rendering

A custom component renders by defining `on_draw()` and calling the `draw_*` functions inside it. These record real, **runtime-visible** geometry into the current frame and are only valid during `on_draw` (a no-op elsewhere). Like the rest of the ambient API they live under the language namespace (`lupine.draw_quad(…)` / `Lupine.draw_quad(…)`); the table below lists the bare names. Positions/sizes are in the owner node's spatial space (world units for 2D/3D, canvas pixels for UI); colors are `r,g,b,a` in `[0,1]`; `blend` is `0` Alpha, `1` Additive, `2` Multiply, `3` Opaque, `4` Overlay.

| Function | Purpose |
|---|---|
| `draw_quad(x,y,z, w,h, r,g,b,a[, blend])` | Colored quad centered at the position |
| `draw_textured_quad(x,y,z, w,h, r,g,b,a, path[, blend])` | Textured quad (`res://` image) |
| `draw_sprite(path, x,y, w,h[, r,g,b,a, rotation, blend])` | 2D sprite centered at the position |
| `draw_rect(x,y, w,h, r,g,b,a[, filled, thickness])` | Top-left rectangle, filled or outlined |
| `draw_line(x1,y1,z1, x2,y2,z2, r,g,b,a[, thickness])` | Line segment |
| `draw_circle(x,y,z, radius, r,g,b,a[, filled])` | Circle |
| `draw_polygon(x,y, radius, sides, r,g,b,a[, rotation, blend])` | Regular polygon |
| `draw_box(x,y,z, w,h,d, r,g,b,a[, wireframe])` | 3D box |
| `draw_rounded_rect(x,y, w,h, corner_radius, r,g,b,a[, blend])` | Rounded rectangle |
| `is_drawing()` | True while inside `on_draw` |

### Editor-only debug draw (no runtime visual)

Call these from `on_render` to draw gizmo geometry that appears **only in the editor** and never in the running game (they route through the editor debug overlay). This is the path for a scalable debug box on a custom component. Contrast with the `debug_draw_*` family, which renders in both the editor and shipped builds.

`editor_draw_line`, `editor_draw_box(cx,cy,cz, w,h,d, r,g,b,a[, wireframe])`, `editor_draw_sphere`, `editor_draw_circle`, `editor_draw_rect_2d(cx,cy, w,h, r,g,b,a)`, `editor_draw_text(x,y,z, text, r,g,b,a)`, and `is_editor_draw_available()`.

### Render bounds

By default a renderable custom component's world bounds (for frustum culling and the editor selection outline) are derived from the node transform and scale with the node. To supply exact bounds, define `get_render_bounds()` returning world-space bounds as `{min={x,y,z}, max={x,y,z}}`, `{center={x,y,z}, size={x,y,z}}`, or a flat `[minx,miny,minz, maxx,maxy,maxz]` array. The result is cached and only re-queried when the scene changes.

```lua
--@component_class "DebugBox"
--@extends_component "Node2D"
--@export size float 64.0
--@export color color {1,0,0,1}

-- Editor-only outline that scales with the node (no runtime visual).
-- Ambient draw functions live under the language namespace (lupine. / Lupine.).
function on_render()
  if not lupine.is_editor_draw_available() then return end
  local p = lupine.get_position_2d()
  lupine.editor_draw_rect_2d(p[1], p[2], size, size, color[1], color[2], color[3], color[4])
end
```

## Async / await

Lua coroutines, MRuby Fibers, MicroPython generators are pumped each frame. `await_signal(sig)` yields until a signal fires; tween/timer callbacks integrate with the scheduler.

---

## Examples

### Lua
```lua
--@component_class "Coin"
--@signal collected(int value)
--@export value int 10

function on_ready()
    local player = Lupine.get_node("%Player")     -- unique-named node (prefixed!)
    if player then Lupine.log_info("found player") end
end

function on_physics_process()
    local hits = Lupine.overlap_circle(Lupine.get_global_position_2d(), 16.0, 0xFFFFFFFF)
    for _, ref in ipairs(hits) do
        if ref:is_in_group("players") then    -- ref method: no prefix
            Lupine.emit("collected", value)
            Lupine.queue_free_self()
            break
        end
    end
end
```

### MicroPython
```python
#@component_class "Spinner"
#@export rpm float 60.0

import lupine

def on_process():
    deg = rpm * 6.0 * delta_time      # 360 deg / 60 s
    lupine.rotate_2d(deg)
```

### MRuby
```ruby
#@component_class "Health"
#@export max_hp int 100
#@signal died

def on_ready
  $hp = max_hp
end

def take_damage(amount)
  $hp -= amount
  Lupine.emit("died") if $hp <= 0
end
```
