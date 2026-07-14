# 07 — Reference (enums, types, managers)

Source: `core/include/lupine/core/PropertyDescriptor.hpp`, `core/include/lupine/math/Math.hpp`, manager headers.

## `PropertyValueType` (numeric index = order)

Used in component properties and `.archetype` field `type`. Indices are stable (append-only).

| # | Name | JSON shape |
|---|---|---|
| 0 | `Int` | integer |
| 1 | `Float` | number |
| 2 | `String` | string |
| 3 | `Bool` | bool |
| 4 | `Vec2` | `{x,y}` |
| 5 | `Vec3` | `{x,y,z}` |
| 6 | `Vec4` | `{x,y,z,w}` |
| 7 | `Color` | `{r,g,b,a}` |
| 8 | `NodePath` | string (node ref) |
| 9 | `ScenePath` | string (scene file ref) |
| 10 | `Enum` | integer index |
| 11 | `StringArray` | `["…"]` |
| 12 | `Double` | number (64-bit) |
| 13 | `Quat` | `{w,x,y,z}` |
| 14 | `Rect` | `{x,y,w,h}` |
| 15 | `Resource` | string (`.ares` ref) |
| 16 | `IntArray` | `[…]` |
| 17 | `FloatArray` | `[…]` |
| 18 | `Array` | heterogeneous JSON array |
| 19 | `Dictionary` | string-keyed JSON object |

## `PropertyHintType` (field `hint.type`)

| # | Name | `hint_string` meaning |
|---|---|---|
| 0 | `None` | — |
| 1 | `Range` | `"min,max,step"` (Int/Float) |
| 2 | `Enum` | `"A,B,C"` allowed values |
| 3 | `File` | file filter (String) |
| 4 | `MultilineText` | — |
| 5 | `ExpRange` | exponential range |
| 6 | `Length` | string length constraint |
| 7 | `ColorNoAlpha` | — |
| 8 | `Dir` | directory path |
| 9 | `Layers2D` | 2D layer bitmask |
| 10 | `Layers3D` | 3D layer bitmask |
| 11 | `NodeType` | node class to filter a NodePath to |
| 12 | `ArchetypeType` | archetype class to filter a Resource to |
| 13 | `ScriptClass` | script/component class to filter to |
| 14 | `Flags` | `"A,B,C"` flag names (bitmask) |
| 15 | `ExpEasing` | easing-curve widget |

## `PropertyUsageFlags` (field `usage`, integer bitmask)

| bit | Name | Meaning |
|---|---|---|
| 1 | `ReadOnly` | shown but not editable |
| 2 | `Hidden` | hidden from the inspector (`[HideInInspector]`) |
| 4 | `NoSerialize` | not written to disk |
| 8 | `Required` | must be set (archetype validation) |
| 16 | `Unique` | identity field, unique among sibling instances |
| 32 | `Experimental` | flagged experimental |
| 64 | `Advanced` | collapsed by default |

## Extended descriptor metadata (serialized property keys)

Beyond `type`/`default`/`hint`/`description`/`group`, a property descriptor serializes
these optional keys (emitted only when set): `usage` (int bitmask above), `header`
(section label), `suffix` (unit text), `custom_widget` (inspector widget id),
`custom_widget_config` (JSON string), `object_type` + `object_schema` (inline-struct
type name + recursive field descriptors). See `04` for the script attribute syntax.

## Property macros (built-in C++ components)

```cpp
PROPERTY(name, type)
PROPERTY_DEFAULT(name, type, defaultVal)
PROPERTY_HINT(name, type, defaultVal, hintType, hintStr)
PROPERTY_INT_RANGE(name, defaultVal, min, max, step)
PROPERTY_FLOAT_RANGE(name, defaultVal, min, max, step)
PROPERTY_ENUM(name, defaultVal, "A,B,C")
PROPERTY_FILE(name, defaultVal, filter)
```
Native extensions use the `Export*` equivalents (see `06`).

## Math types (`math/Math.hpp`, namespace `lupine::math`)

`Vec2{x,y}`, `Vec3{x,y,z}`, `Vec4{x,y,z,w}`, `Quat{w,x,y,z}`, `Color{r,g,b,a}` (0–1 floats), `Rect{x,y,w,h}`, `Mat4`. Plus `Gradient` and `Curve` data types (JSON, multi-stop) used by particles/sampling (`sample_gradient`, `sample_curve`).

## Common enums

**Animation**: `TrackType` (position, rotation, scale, value, method, bezier) · `InterpolationMode` (linear, cubic) · `LoopMode` (no_loop, loop, loop_pingpong) · `PlaybackMode` (OneShot, Loop, Scheduled) · `PlaybackState` (Stopped, Playing, Paused).

**Physics**: `CollisionShape2DType` (circle, box, polygon, capsule) · `CollisionShape3DType` (box, sphere, capsule, mesh) · `MeshSolverType` (dynamic, static).

**UI**: `ButtonState` (Normal, Hover, Pressed, Disabled) · `ButtonStyleMode` (Automatic, Manual) · `ButtonScaleMode` (Fixed, FitToText, FitToTextWidth) · `CheckListOrientation` (horizontal, vertical) · `SizeMode`, `LayoutDirection`, `DockSide`.

**Graphics**: project `scale_mode` (letterbox, stretch, crop, ignore).

## Managers (singletons, `core/include/lupine/…`)

| Manager | Header | Role |
|---|---|---|
| `SceneManager` | `core/SceneManager.hpp` | Scene load/switch, autoloads/singletons, project settings, lifecycle, script host pump |
| `InputManager` | `input/InputManager.hpp` | Action/axis mapping, devices, per-frame state (edge detection) |
| `AudioManager` | `audio/AudioManager.hpp` | Playback, buses, DSP effects (miniaudio) |
| `LocalizationManager` | `localization/LocalizationManager.hpp` | Tables, locales, plurals, format args |
| `SaveGameManager` | `save/SaveGameManager.hpp` | Named save slots over `user://`, formats, transforms |
| `NetworkManager` | `network/NetworkManager.hpp` | Transports, RPC, replication, peers |
| `ThemeManager` | `ui/ThemeManager.hpp` | UI themes/palettes |
| `ExtensionManager` | `core/ExtensionManager.hpp` | Load `.lupineext`, register components |

## Key core headers

`core/Node.hpp`, `core/Component.hpp`, `core/Scene.hpp`, `core/SceneManager.hpp`, `core/Serialization.hpp` (`ISerializable`), `core/SignalObject.hpp` + `core/SignalDispatcher.hpp` (events/signals), `core/ArchetypeDefinition.hpp` + `core/ArchetypeRegistry.hpp`, `core/ComponentProperty.hpp` + `core/PropertyDescriptor.hpp`.

## Build outputs recap

`lupine_core` (static) · `lupine_engine_lib` (static) · `lupine_engine` (.pyd, editor) · `lupine_runtime_lib` (static) · `lupine_runtime_py` (.pyd) · `lupine_runtime` (exe) · `lupine_console` (exe) · `lupine_capi` (shared) · extension SDK (header-only + `lupine_add_extension()`). See `01_overview.md`.
