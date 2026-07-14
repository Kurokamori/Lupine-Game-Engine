# 14 — Signals, Events & Groups

Three decoupling mechanisms: **signals** (per-object, Godot-style), the **EventBus** (global pub/sub), and **groups** (named node sets). Sources: `core/include/lupine/core/{SignalObject,SignalDispatcher}.hpp`; scripting in `*Environment.cpp`.

## Signals (per-object)

`Node` and `Component` are `SignalObject`s. Built-in components declare signals (e.g. `Button` `pressed`, area `body_entered`, `Timer` `timeout`); scripts declare their own with `@signal`.

Declare + emit:
```lua
--@signal health_changed(int amount)
function take_damage(n)
    health = health - n
    Lupine.emit("health_changed", health)   -- ambient emit acts on self
end
```

Connect (in `on_ready`):
```lua
Lupine.get_node("%Hero"):connect("health_changed", self, "on_health_changed")
function on_health_changed(amount) update_bar(amount) end
```

NodeRef/ComponentRef signal API: `emit(sig, ...)`, `connect(sig, target, method[, flags])`, `disconnect(sig, id)`, `is_connected(sig)`, `add_user_signal(name)`, `get_signal_list()`, `await_signal(sig)` (coroutine yield until fired). Connections made in the editor's **Node Signals** panel persist in the scene (`signal_connections`). Dispatch supports deferred connections and is integrated with deferred `QueueFree` via `SignalDispatcher`.

### Built-in component signals (exact names + args)

Verified from each component's `DefineSignals()`. The handler receives the listed args.

| Component | Signal(args) |
|---|---|
| `Button`, `Button3D` | `pressed()`, `released()`, `hovered()`, `state_changed(state:int)` |
| `TextureButton` | `pressed()`, `toggled(checked:bool)` |
| `ToggleButton` | `toggled(toggled:bool)` |
| `Checkbox` | `toggled(checked:bool)` |
| `RadioButton` | `selected(value:int)` |
| `RadioList` | `selection_changed(index:int, value:int)` |
| `CheckList` | `group_changed(checked:int, total:int)` |
| `Slider`, `SpinBox` | `value_changed(value:float)` |
| `LineEdit` | `text_changed(text:string)`, `text_submitted(text:string)` |
| `TextEdit` | `text_changed(text:string)` |
| `Dropdown`, `PopupMenu` | `item_selected(index:int)` |
| `PopupMenu` | `popup_closed()` |
| `ItemList`, `Tree` | `item_selected(index:int)`, `item_activated(index:int)` |
| `TabContainer` | `tab_changed(index:int)` |
| `ScrollContainer` | `scrolled(h:float, v:float)` |
| `AreaTrigger2DComponent`, `AreaTrigger3DComponent` | `body_entered(body:NodePath)`, `body_exited(body:NodePath)` |
| `RigidBody2DComponent`, `StaticBody2DComponent`, `KinematicBody2DComponent` | `body_entered(body:NodePath)`, `body_exited(body:NodePath)` |
| `AnimationPlayer` | `animation_started(name)`, `animation_finished(name)`, `animation_changed(old,new)`, `animation_looped(name)`, `method_track()` |
| `AnimationTree` | `state_changed()` (layer, from, to) |
| `AnimatedSprite2D`, `AnimatedSprite3D` | `animation_finished()`, `frame_changed(frame:int)` |
| `Timer` | `timeout()` |
| `Tween` | `finished()`, `step()` |
| `TweenSequence` | `finished()`, `step_finished()` |
| `NavigationAgent2D` | `path_changed()`, `waypoint_reached()`, `target_reached()`, `navigation_finished()`, `velocity_computed()` |
| `NavigationRegion2D` | `baked()` |
| `NetworkObject` | `authority_changed(peer:int)` |

Note: areas and physics bodies emit `body_entered`/`body_exited` (there is no `area_entered`). 3D bodies (`RigidBody3DComponent` etc.) follow the same `body_entered`/`body_exited` convention — confirm in the header.

## EventBus (global pub/sub)

For broadcast events with no direct object reference:
```lua
Lupine.emit_event("level_complete", score)
Lupine.subscribe("level_complete", "on_level_complete")   -- calls self:on_level_complete(score)
function on_level_complete(score) ... end
```
Use signals when you have the object; use the EventBus for global, many-listener events.

## Groups

Tag nodes into named sets and query/iterate them:
```lua
Lupine.add_to_group("enemies")                    -- self into group
for _, ref in ipairs(Lupine.get_nodes_in_group("enemies")) do
    ref:call_method_or...                         -- ref is a NodeRef
end
```
NodeRef group API: `add_to_group(g)`, `remove_from_group(g)`, `is_in_group(g)`, `get_groups()`. Groups also serialize per-node (`"groups": [...]`, see `02_projects_and_scenes.md`) so they can be set in the editor. Common pattern: tag `players`/`enemies`/`pickups`, then filter physics-query hits by group (see `10_physics.md`).

For a *capability* declared by a script/archetype/component type — with a verifiable method+signal contract and auto-shared signals — use an **interface** instead of (or alongside) a group: `get_nodes_with_interface("Damageable")`, `node:implements_interface("Damageable")`. See [18 — Interface Types](18_interfaces.md).

## Scene tree access

`get_tree()` returns a tree handle for scene-wide operations (group calls, scene queries). `get_node(path)` / `find_node(path)` resolve nodes (`%Name` = unique name in owner).
