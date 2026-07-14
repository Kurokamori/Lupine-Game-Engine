# 08 — Input

Action/axis abstraction over keyboard, mouse, and gamepads, with cross-device glyphs, input contexts, local-multiplayer player slots, runtime rebinding, and capture. Sources: `core/include/lupine/input/InputManager.hpp`, `core/src/scripting/*Environment.cpp`, `default_input_map.json`, `default.inputglyphs`. All functions are on the `Lupine`/`lupine` script table (see `04_scripting.md`); C-API under `capi/include/input/`.

## Model

- **Actions** = named buttons (`"jump"`), matched against one or more **bindings**. Poll with `is_action_pressed/just_pressed/just_released`, strength with `get_action_strength`.
- **Axes** = named 1D values (`"move_horizontal"`), each binding contributes a `scale`; combine with `get_axis`. `get_vector(negX,posX,negY,posY)` builds a 2D vector from four actions.
- Defined in the project's **input map** (`input_map` section of `.lupine`, or a standalone JSON like `default_input_map.json`). Editable in the editor and at runtime.

## Input map JSON

```json
{
  "actions": [
    { "name": "jump", "deadzone": 0.5, "context": "", "enabled": true,
      "bindings": [
        { "deviceType": 0, "keyCode": 32, "keyName": "Space" },
        { "deviceType": 2, "gamepadButton": 0, "buttonName": "A", "gamepadID": 0 }
      ] }
  ],
  "axes": [
    { "name": "move_horizontal", "deadzone": 0.2, "context": "", "enabled": true,
      "sensitivity": 3.0, "gravity": 3.0, "snap": true,
      "bindings": [
        { "deviceType": 0, "keyCode": 65, "keyName": "A", "scale": -1.0 },
        { "deviceType": 0, "keyCode": 68, "keyName": "D", "scale":  1.0 },
        { "deviceType": 2, "gamepadAxis": 0, "axisName": "LeftX", "scale": 1.0, "gamepadID": 0 }
      ] }
  ]
}
```

- `deviceType`: **0 = Keyboard, 1 = Mouse, 2 = Gamepad**.
- Binding fields by device: keyboard `keyCode`/`keyName`; mouse `mouseButton`/`buttonName`; gamepad button `gamepadButton`/`buttonName`/`gamepadID`, gamepad axis `gamepadAxis`/`axisName`/`scale`/`gamepadID`. `gamepadID: 0` = any gamepad.
- Axis tuning: `sensitivity` (key→value ramp speed), `gravity` (return-to-zero speed), `snap` (instant sign flip), `deadzone`.
- `context` empties to "always active"; otherwise gated by input contexts (below).
- Key codes are **GLFW-style** (table below). Mouse buttons: 0=Left,1=Right,2=Middle. Gamepad buttons: 0=A/Cross,1=B/Circle,2=X/Square,3=Y/Triangle,…; gamepad axes: 0=LeftX,1=LeftY,2=RightX,3=RightY,4=LeftTrigger,5=RightTrigger.

### Key codes (GLFW)

| Key | Code | Key | Code |
|---|---|---|---|
| Space | 32 | Escape | 256 |
| Apostrophe `'` | 39 | Enter | 257 |
| Comma `,` | 44 | Tab | 258 |
| Minus `-` | 45 | Backspace | 259 |
| Period `.` | 46 | Insert | 260 |
| Slash `/` | 47 | Delete | 261 |
| 0–9 | 48–57 | Right | 262 |
| Semicolon `;` | 59 | Left | 263 |
| Equal `=` | 61 | Down | 264 |
| A–Z | 65–90 | Up | 265 |
| Left bracket `[` | 91 | Page Up | 266 |
| Backslash `\` | 92 | Page Down | 267 |
| Right bracket `]` | 93 | Home | 268 |
| Grave `` ` `` | 96 | End | 269 |
| F1–F12 | 290–301 | Caps Lock | 280 |
| Keypad 0–9 | 320–329 | Print Screen | 283 |
| Left Shift | 340 | Right Shift | 344 |
| Left Control | 341 | Right Control | 345 |
| Left Alt | 342 | Right Alt | 346 |
| Left Super | 343 | Menu | 348 |

Letters: A=65 … Z=90. Digits: 0=48 … 9=57. (Standard GLFW key constants; `keyName` in the map is the human label.)

## Polling (per frame, in `on_process`/`on_input`)

```lua
if Lupine.is_action_just_pressed("jump") then ... end
local move = Lupine.get_vector("move_left","move_right","move_up","move_down")
local steer = Lupine.get_axis("move_horizontal")
```
Optional trailing `player` index targets a local-MP slot (default `-1` = any). Raw input: `is_key_pressed(code)`, `is_mouse_button_pressed(b)`, `get_mouse_position()`, `get_mouse_delta()`, `get_mouse_scroll_delta()`.

> Edge functions (`*_just_pressed`, scroll, text) only work because the runtime snapshots previous state **before** the event pump each frame. They are reliable in `on_process`/`on_input`.

## Event-driven (`on_input_event(event)`)

`event` is a JSON-like object. `type` ∈ `key_down`/`key_up`/`mouse_button_down`/`mouse_button_up`/`gamepad_button_down`/`gamepad_button_up`/`gamepad_axis` (+ mouse motion). Fields: `key`, `button`, `axis`, `value`, `gamepad_id`. Match against actions with helpers:

```lua
function on_input_event(event)
    if Lupine.event_is_action_pressed(event, "jump") then jump() end
    -- also: event_is_action(event, name), event_is_action_released(event, name)
end
```

## Gamepad

`is_gamepad_connected(id)`, `get_gamepad_count()`, `get_gamepad_name(id)`, `is_gamepad_button_pressed(b,id)`, `get_gamepad_axis(a,id)`, `set_gamepad_vibration(id,left,right,duration)`. Type/active-device: `get_active_device_type()`, `get_last_gamepad_id()`, `get_gamepad_type(id)` (Xbox/PS/Switch).

## Cross-device glyphs / prompts

The engine generates device-aware glyph ids (`key_space`, `gamepad_xbox_a`, `mouse_left`). Resolve a prompt for an action based on the active device:

```lua
local g = Lupine.get_action_glyph("jump")   -- { id=..., label=..., art=... }
-- all bindings: Lupine.get_action_glyphs("jump")
```
Override labels/art: `set_glyph_label(id,label)`, `set_glyph_art(id,path)`, `clear_glyph_override(id)`, `load_glyph_map(path)`, `save_glyph_map(path)`. Glyph map file (`.inputglyphs`) entries: `{ "label": "...", "art": "res://x.png" }` (both optional).

## Input contexts / action sets

Enable/disable groups of actions by `context` tag (e.g. gameplay vs menu):
`enable_input_context(c)`, `disable_input_context(c)`, `set_input_context_active(c,bool)`, `is_input_context_active(c)`, `set_exclusive_input_context(c)`, `get_active_input_contexts()`. Per-action toggles: `set_action_enabled(a,bool)`, `set_axis_enabled(a,bool)`.

## Local multiplayer (player slots)

`set_player_count(n)`, `get_player_count()`, `clear_player_assignments()`, `assign_keyboard_mouse_to_player(p)`, `assign_gamepad_to_player(p, gamepadId)`, `unassign_gamepad(id)`, `get_player_for_gamepad(id)`, `get_player_for_keyboard_mouse()`, `player_owns_keyboard_mouse(p)`, `get_player_gamepads(p)`, `set_auto_join_enabled(bool)`, `is_auto_join_enabled()`. Then pass the player index to the polling functions.

## Runtime rebinding & capture (settings menus)

Mutate bindings: `add_action_key(a,code)`, `add_action_mouse_button(a,btn)`, `add_action_gamepad_button(a,btn[,id])`, `add_action_gamepad_axis(a,axis[,scale,id])`, `remove_action_binding(a,index)`, `clear_action_bindings(a)`, `get_action_bindings(a)`. Axis equivalents: `add_axis_key`, `add_axis_gamepad_axis`, `remove_axis_binding`, `clear_axis_bindings`, `get_axis_bindings`. Persist: `save_input_map(path)`, `load_input_map(path)`.

Capture a key for a "press a key…" prompt:
```lua
Lupine.start_input_capture()                       -- or start_input_capture_mask(kb,mouse,pad)
-- poll each frame:
if Lupine.is_input_capture_complete() then
    Lupine.apply_captured_binding_to_action("jump") -- or get_captured_binding() to inspect
end
-- Lupine.cancel_input_capture(), is_capturing_input(), clear_captured_binding()
```

## Action delegation (callbacks instead of polling)

```lua
local id = Lupine.connect_action("jump", "on_jump")   -- calls self:on_jump() on the event
Lupine.disconnect_action("jump", id)
Lupine.connect_device_changed("on_device_changed")    -- active device switched
Lupine.connect_input_captured("on_captured")          -- capture finished
```

## C-API

`capi/include/input/` exposes the same surface: action/axis polling, rebinding, contexts, glyphs (`lc_input_load_glyph_map`), capture, player slots.
