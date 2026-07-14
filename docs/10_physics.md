# 10 — Physics (2D & 3D)

2D physics is Box2D v3; 3D has its own bodies. Bodies are components on nodes; collision shapes are carried by collision components. Layer/mask filtering uses 32 named layers (project settings). Script API on `Lupine`/`lupine` (see `04_scripting.md`); C-API under `capi/include/physics/`.

## Body components

| 2D | 3D | Role |
|---|---|---|
| `RigidBody2DComponent` | `RigidBody3DComponent` | Dynamic, forces/gravity-driven |
| `StaticBody2DComponent` | `StaticBody3DComponent` | Immovable collider |
| `KinematicBody2DComponent` | `KinematicBody3DComponent` | Script-moved, collides |
| `CollisionBody2DComponent` | `CollisionMesh3DComponent` | Collision shape carrier |
| `AreaTrigger2DComponent` | `AreaTrigger3DComponent` | Sensor; overlap signals, no solid response |
| `CharacterController2D` | `CharacterController3D` | Move-and-slide controller |

Shapes: 2D `circle`/`box`/`polygon`/`capsule`; 3D `box`/`sphere`/`capsule`/`mesh` (`MeshSolverType` dynamic/static).

## Layers & masks

Project settings define `collision_layers_2d` / `collision_layers_3d` (32 named bits each). A body's **layer** is what it is; its **mask** is what it scans for. 2D fully supports the layer/mask split. 3D bodies advertise one bitmask as both layer and mask (they collide when they share any layer), and every 3D query takes a `layerMask` that is matched against it.

## Character controllers

```lua
-- in on_physics_process
local vx = Lupine.get_axis("move_horizontal") * speed
Lupine.set_character_velocity_2d(vx, vy)        -- or:
Lupine.move_and_slide_2d(vx, vy)
if Lupine.is_character_on_ground_2d() then ... end
```
3D equivalents: `move_and_slide_3d`, `is_character_on_ground_3d`, `set_character_velocity_3d`.

## Rigid body control

2D: `get/set_linear_velocity_2d`, `get/set_angular_velocity_2d`, `apply_force_2d(x,y)`, `apply_impulse_2d(x,y)`, `apply_torque_2d(t)`, `set_gravity_scale_2d(s)`. 3D: same with 3D vectors. Call on `self` (ambient) or on a NodeRef for another node.

## Queries (raycast / overlap / shapecast)

```lua
local hit = Lupine.raycast_2d({x=px,y=py}, {x=1,y=0}, 200.0, 0xFFFFFFFF)
-- hit: { hit=bool, node=NodeRef, point={x,y}, normal={x,y}, distance=... }
for _, h in ipairs(Lupine.overlap_circle({x=px,y=py}, 32.0, mask)) do ... end
```
2D: `raycast_2d`, `raycast_all_2d`, `overlap_circle`, `overlap_rect`, `circle_cast_2d`. 3D: `raycast_3d`, `raycast_all_3d`, `overlap_sphere`, `overlap_box`, `sphere_cast_3d`. `mask` is a 32-bit layer mask (`0xFFFFFFFF` = all). Query results map body ids → `NodeRef`.

### Polling cast components

For per-frame "is something in front of me" checks without driving the query yourself, attach a `RayCast2D`/`RayCast3D` (thin ray) or `ShapeCast2D`/`ShapeCast3D` (swept circle/sphere). Set `targetPosition` (node-local), read `IsColliding()`/`GetCollider()`/`GetCollisionPoint()`/`GetCollisionNormal()` each frame, `ForceUpdate` to re-cast immediately. 2D and 3D casts both honour a `collisionMask`. `visibleInGame` keeps the editor gizmo drawn at runtime.

## C-API (`capi/include/physics/`)

Every body/collider/area/controller/cast component above has a typed `lc_*` mirror, plus the one-shot query and world API:

- **Queries** (`lc_physics_query2d.h` / `lc_physics_query3d.h`): `lc_physics{2,3}d_raycast[_ignore]`, `_raycast_all[_ignore]`, `_overlap_*`, `_circle_cast`/`_sphere_cast[_ignore]`. The 2D queries also have `_masked` variants taking a 64-bit `collisionMask`; the C++ 3D queries take a 32-bit `layerMask` parameter.
- **Body ↔ node**: query results carry a body UUID; `lc_physics{2,3}d_get_body_node(bodyId, &node)` resolves it to the owning node, and `lc_physics{2,3}d_find_body_for_node(node, &bodyId)` is the inverse (use it to build an ignore filter for a node's own body).
- **World config**: `lc_physics{2,3}d_get/set_gravity`, `_get/set_time_step`; 2D also has `_get/set_velocity_iterations` and `_get/set_position_iterations`.
- **AreaTrigger2D** owns its own sensor collider, so its shape is configured directly: `lc_area_trigger2d_get/set_shape_type` / `_size` / `_radius` / `_offset` / `_collision_layers` / `_collision_mask` and `_get_vertex_count` / `_get_vertex` / `_set_vertices` (AreaTrigger3D instead reads shapes from child `CollisionMesh3D` components).

## Collision & trigger signals

Body and area components emit signals you connect to (see `14_signals_events_groups.md`): area enter/exit and body contact signals. Connect in `on_ready`:
```lua
Lupine.get_node("%Hitbox"):connect("body_entered", self, "on_body_entered")
function on_body_entered(other)  -- other is a NodeRef
    if other:is_in_group("enemies") then other:get_component("Health"):call("take_damage", 10) end
end
```
Exact signal names are declared on each body/area component — read `core/include/lupine/components/<Body>.hpp`.

## Settings

`physics` section of `.lupine`: `physics_tick_rate`, `gravity_2d`, `gravity_3d`, and the 32 named collision layers per dimension. Physics runs in `on_physics_process` / `OnPhysicsUpdate` at the fixed tick rate.
