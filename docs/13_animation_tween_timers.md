# 13 — Animation, Tweens & Timers

Three tools: `AnimationPlayer`/`AnimationTree` (authored keyframe animation), `Tween`/`TweenSequence` (procedural interpolation), `Timer` (delayed/repeating callbacks). Components in `core/include/lupine/components/`.

## AnimationPlayer

Plays keyframe **clips** that animate node/component properties over tracks. Control via `ComponentRef:call(...)`:
```lua
local anim = get_node("%Hero"):get_component("AnimationPlayer")
anim:call("play", "run")
anim:call("stop")
anim:call("set_speed", 1.5)
if anim:call("is_playing") then ... end
```
**`call` methods** (`AnimationPlayer.cpp`): `play`, `play_backwards`, `stop`, `pause`, `resume`, `seek`, `queue`, `clear_queue`, `is_playing`, `current_animation`, `current_time`, `has_animation`, `get_animation_list`, `set_speed`, `get_speed`, `sample_at`, `add_clip`, `remove_clip`, `reload_clips`, `get_clip_library`.

**Signals**: `animation_started(name)`, `animation_finished(name)`, `animation_changed(old,new)`, `animation_looped(name)`, `method_track`.

Track types: position, rotation, scale, value (any property), method (call), bezier. Interpolation: linear / cubic. Loop modes: no_loop / loop / loop_pingpong. Authoring is done in the editor Animation Timeline panel.

**Stable node targeting**: each track stores both a `node` path (resolved relative to the player root — portable across prefab instances) and an optional `nodeUuid` (the target node's stable scene-wide UUID). Resolution prefers `nodeUuid`: a track keeps animating its node even after the node is moved, renamed, or reparented in the scene tree, falling back to `node` only when the UUID is absent or not found in the current scene (e.g. a different prefab instance). The editor backfills `nodeUuid` onto legacy path-only clips on load and rewrites the stored `node` path to the node's current location when saving.

## AnimationTree

Blend-tree / state machine over clips for blended, parameter-driven animation (locomotion blends, state transitions). Drive blend params and transitions via `call`. Author in the editor Blend Tree panel.

## Tween (procedural)

Interpolate a transform or property over time with an easing. The ambient form tweens the script's own node (call on another node's ref as `ref:create_tween(...)`):
```lua
local t = Lupine.create_tween("position_2d", {400, 100}, 0.5, "quad_out")
```
`Lupine.create_tween(channel, toValue, duration, easing)` returns a **TweenRef** (prefix required, like all ambient calls — see `04`).

**Channels** (`core/src/components/Tween.cpp`): built-in transform channels `position_2d`, `global_position_2d`, `rotation_2d`, `scale_2d`, `position_3d`, `rotation_3d`, `scale_3d`. Any other string is treated as a **property name** on the node's property bag (node + components) — e.g. `"modulate"`, `"opacity"`, an `@export` var. Vector targets pass as arrays (`{x,y}` order); scalar channels (`rotation_2d`) take a number.

**Easing names** (`core/src/animation/AnimationInterp.cpp`, Penner curves; unknown ⇒ `linear`):
`linear` · `sine_in`/`sine_out`/`sine_in_out` · `quad_*` · `cubic_*` · `quart_*` · `quint_*` · `expo_*` · `circ_*` · `back_*` · `elastic_*` · `bounce_*` (each with `_in`/`_out`/`_in_out`) · aliases `ease_in`/`ease_out`/`ease_in_out`.

The `Tween` component exposes `duration`, `elapsed`, `easing`, `loop`, `autoRemove`, `running` properties; signals `finished` and `step`.

## TweenSequence

Chain steps (sequential + parallel) with a builder:
```lua
local seq = Lupine.create_sequence()   -- returns SequenceRef
-- append steps, parallel groups, callbacks, delays via the sequence builder
```
Use `TweenSequence` component for editor-authored sequences. (See `project_scripting_tween_reorder` / `project_scripting_async_await`.)

## Timer

```lua
local id = Lupine.create_timer(2.0, function() Lupine.log_info("2s elapsed") end)
local r  = Lupine.create_repeating_timer(0.5, on_tick, 10)   -- interval, callback, repeat count (0 = forever)
```
Or use the `Timer` component for editor-configured one-shot/repeating timers with a `timeout` signal.

## Async / await integration

Tweens, timers, and `await_signal` integrate with the per-language scheduler (Lua coroutines / MRuby Fibers / MicroPython generators), pumped each frame:
```lua
function on_ready()
    local t = Lupine.create_tween("modulate", {1, 1, 1, 0}, 1.0, "linear")
    t:as_component():connect("finished", self, "on_faded")  -- Tween emits "finished"
end
function on_faded() Lupine.queue_free_self() end
```

**TweenRef** methods: `play`, `pause`, `stop`, `restart`, `kill`, `is_running`, `is_finished`, `get_progress`, `get/set_duration`, `get/set_easing`, `get/set_loop`, `set_auto_remove`, `get_owner`, `as_component`.

**SequenceRef** builder: `append(channel,to,duration[,easing,parallel])`, `append_on(target,channel,to,…)`, `append_interval(duration[,parallel])`, `append_callback(method[,parallel])`, `append_callback_on(target,method,…)`, then `play`, `stop`, `reset`, `restart`, `kill`, `is_running`, `is_finished`, `set_loops`. Emits `finished` and `step_finished`.

For coroutine-style waiting use `await_signal(...)` on a node/component, or a `Timer`. See `04_scripting.md` (Async / await).
