# 20 — Game Patterns (cookbook)

Complete, code-checked recipes wiring the systems in `01`–`19` together. Lua shown; MRuby/MicroPython are identical bar syntax and the `lupine.` prefix (`04`). Every call here is verified to exist; cross-reference the listed doc for the full surface.

Convention reminder (`04`): ambient engine functions need the language prefix (`Lupine.` / `lupine.`); lifecycle callbacks, `get_node`/`find_node`, `emit`, `delta_time`, exported-property globals and `draw_*` are bare globals; methods on another node are called on its **NodeRef**.

---

## Top-down movement (8-way)

`get_vector` returns a normalized 2-element array `{x, y}` (index `[1]`/`[2]`, not `.x`/`.y`) from four actions (`08`); `CharacterController2D` resolves collisions (`10`).

```lua
--@component_class "TopDownMover"
--@export speed float 220.0

function on_physics_process()
    local v = Lupine.get_vector("move_left", "move_right", "move_up", "move_down")
    Lupine.move_and_slide_2d(v[1] * speed, v[2] * speed)
end
```

## Platformer movement (gravity + jump)

Track vertical velocity yourself; `is_character_on_ground_2d` gates the jump.

```lua
--@component_class "Platformer"
--@export speed float 240.0
--@export jump_force float 520.0
--@export gravity float 1400.0

function on_ready() vy = 0.0 end

function on_physics_process()
    local dt = delta_time
    local dir = Lupine.get_axis("move_horizontal")
    if Lupine.is_character_on_ground_2d() then
        vy = 0.0
        if Lupine.is_action_just_pressed("jump") then vy = -jump_force end
    else
        vy = vy + gravity * dt
    end
    Lupine.move_and_slide_2d(dir * speed, vy)
end
```

## Spawn & despawn from a prefab

`Lupine.instantiate_prefab(path[, parent])` returns the new root **NodeRef** (`04`). Free with `ref:queue_free()` / `Lupine.queue_free_self()`.

```lua
function spawn_enemy(x, y)
    local e = Lupine.instantiate_prefab("res://prefab/enemy.prefab")  -- parent omitted → current scene root
    e:set_position_2d(x, y)
    e:add_to_group("enemies")
    return e
end

-- self-destruct after a delay (timer recipe below also applies)
function on_ready()
    Lupine.create_timer(5.0, function() Lupine.queue_free_self() end)
end
```

## Pickup via overlap + groups

Poll an overlap each physics tick; filter by group; emit a signal and free (`10`, `14`).

```lua
--@component_class "Coin"
--@signal collected(int value)
--@export value int 10

function on_physics_process()
    local center = Lupine.get_global_position_2d()
    for _, ref in ipairs(Lupine.overlap_circle(center, 16.0, 0xFFFFFFFF)) do
        if ref:is_in_group("players") then     -- ref method: no prefix
            Lupine.emit("collected", value)
            Lupine.queue_free_self()
            break
        end
    end
end
```

## Damage via a component method

Reach a sibling/other node's component and `call` a method on it (`04` ComponentRef). Pair with a hitbox `AreaTrigger2D` signal (`10`).

```lua
function on_ready()
    Lupine.get_node("%Hitbox"):connect("body_entered", self, "on_hit")
end

function on_hit(other)                         -- other is a NodeRef
    if other:is_in_group("enemies") then
        local hp = other:get_component("Health")  -- a custom component with take_damage(n)
        if hp then hp:call("take_damage", 25) end
    end
end
```

Health as a custom component (`04`/`18`):

```lua
--@component_class "Health"
--@export max_hp int 100
--@signal died

function on_ready() hp = max_hp end
function take_damage(n)
    hp = hp - n
    if hp <= 0 then Lupine.emit("died"); Lupine.queue_free_self() end
end
```

> For "hit anything that can take damage" regardless of concrete type, define a `Damageable` **interface** and query the scene for implementers / `ref:is_instance_of(...)` — see `18_interfaces.md`.

## Camera follow & screen shake

Follow/limits/shake live on the camera node and run **at runtime only** (`03`). They are **NodeRef methods** on the camera (call them on the camera's ref, not as ambient functions):

```lua
function on_ready()
    local cam = Lupine.get_node("%Camera")            -- a Camera2D / CameraUI node
    cam:camera_set_follow_target(Lupine.get_node("%Player"))
end

function on_explosion()
    Lupine.get_node("%Camera"):camera_shake(8.0, 0.3) -- amplitude, duration [, frequency]
    -- or move there directly: cam:camera_smooth_move_to(x, y [, speed])
end
```

## Hit-flash & fade-out (tweens)

`Lupine.create_tween(channel, toValue, duration, easing)` (`13`). Transform channels are built in; any other string targets a property (`modulate`, `opacity`, an `@export`).

```lua
-- flash white then back by tweening the sprite modulate
function flash()
    Lupine.create_tween("modulate", {1, 0, 0, 1}, 0.05, "linear")
    Lupine.create_timer(0.05, function()
        Lupine.create_tween("modulate", {1, 1, 1, 1}, 0.15, "quad_out")
    end)
end

-- fade out then free
function on_died()
    local t = Lupine.create_tween("modulate", {1, 1, 1, 0}, 0.4, "quad_out")
    t:as_component():connect("finished", self, "on_faded")   -- t is a TweenRef
end
function on_faded() Lupine.queue_free_self() end
```

## Scene flow: switch, overlay, reload

`change_scene` replaces the running scene; `add_scene`/`remove_scene` overlay additively (e.g. a HUD or pause menu kept across levels) (`02`/`04`).

```lua
Lupine.change_scene("res://scenes/level2.scene")     -- full transition
Lupine.add_scene("res://scenes/pause_menu.scene")    -- overlay on top of current
Lupine.remove_scene("res://scenes/pause_menu.scene") -- remove that overlay
Lupine.reload_scene()                                 -- restart current (death/retry)
```

An overlay's root becomes a **child of the current scene's root**, so it is an ordinary part of the tree — findable with `get_node`/`find_node`, ticked and drawn by the current scene, and its 2D/3D world content is rendered by the camera passes (not just its UI). It still survives a `change_scene`: it is re-parented onto the incoming scene with its state intact, which is what keeps a HUD or pause menu alive across levels.

## Pause menu

Pause stops gameplay `on_process` ticks but UI/script that ignores pause keeps running; toggle with `set_game_paused`. Use `set_time_scale` for slow-mo / bullet-time.

```lua
function on_process()
    if Lupine.is_action_just_pressed("pause") then
        local paused = not Lupine.is_game_paused()
        Lupine.set_game_paused(paused)
        if paused then Lupine.add_scene("res://scenes/pause_menu.scene")
        else Lupine.remove_scene("res://scenes/pause_menu.scene") end
    end
end
-- elsewhere: Lupine.set_time_scale(0.3)  -- slow motion;  1.0 = normal
```

## Cross-script state & save/load

Globals are typed key/value shared across all scripts; the save system persists named slots (`04`/`15`).

```lua
Lupine.set_global_int("score", Lupine.get_global_int("score", 0) + 10)

-- save / load a slot (data is any table; meta is optional)
Lupine.save_game("slot1", { score = Lupine.get_global_int("score", 0), level = "level2" })
local data = Lupine.load_game("slot1")
if data then Lupine.set_global_int("score", data.score) end
```

## Periodic logic with timers

```lua
function on_ready()
    -- fire every 0.5s forever (count 0 = forever); store id to cancel later if needed
    Lupine.create_repeating_timer(0.5, function() spawn_enemy(rand_x(), -20) end, 0)
end
```

## Audio feedback

One-shot SFX on a bus; music loop; ducking via bus volume (`11`).

```lua
Lupine.play_audio("res://sfx/coin.wav", "SFX", false, 1.0)         -- path, bus, loop, volume
local music = Lupine.play_audio("res://music/theme.ogg", "Music", true, 0.8)
Lupine.set_bus_volume("Music", 0.3)                                -- duck under dialogue
```

---

## Wiring checklist for a new gameplay object

1. **Node base**: `Node2D`/`Node3D` for world objects; `Control`/UI node for HUD (`03`).
2. **Visual**: `Sprite2D` / `AnimatedSprite2D` / mesh component (`03`/`16`).
3. **Physics** (if it collides): a body component + a collision-shape carrier; pick `CharacterController2D` for the player, `RigidBody*` for physics props, `AreaTrigger*` for sensors (`10`).
4. **Behaviour**: a `ScriptComponent` with `@export` tunables, or a custom component class (`04`).
5. **Identity**: set `unique_name_in_owner` if scripts must find it by `%Name`; add it to a **group** if systems query it in bulk (`03`).
6. **Reuse**: save it as a `.prefab` and spawn with `instantiate_prefab`, or compose levels from `.scene` files via `SceneInstance` (`02`).
7. **Wire events**: connect body/area/UI signals in `on_ready` (`14`).
