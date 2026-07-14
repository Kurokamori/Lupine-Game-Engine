# 02 — Projects, Scenes & Prefabs

All formats are JSON. Sources: `editor/project_file.py`, `core/src/{Scene,Node,Serialization}.cpp`.

## Project file (`.lupine`)

Top-level JSON keyed by section. Sections (`ProjectData` in `project_file.py`):

| Section | Key fields |
|---|---|
| `project` | `name`, `version`, `creator`, `created_date`, `last_modified`, `icon`, `main_scene` (res:// path) |
| `structure` | `scenes_directory`, `assets_directory`, `scripts_directory` |
| `window` | `width`, `height` (design resolution / logical canvas), `fullscreen`, `vsync`, `target_fps`, `resizable`, `borderless`, `size_override` (open the OS window at a different physical size), `override_width`, `override_height` |
| `graphics` | `scale_mode` (`letterbox`/`stretch`/`crop`/`ignore`), `texture_filtering` |
| `physics` | `physics_tick_rate`, `gravity_2d`, `gravity_3d`, `collision_layers_2d`/`collision_layers_3d` (32 named layers each) |
| `audio` | `master_volume`, `music_volume`, `sfx_volume`, `buses_state` (serialized mixer) |
| `networking` | `transport` (`enet`/`websocket`/`loopback`), `default_port`, `max_peers`, `tick_rate`, `protocol_version`, `game_id` |
| `editor` | `save_on_play`, `max_undo_steps` |
| `runtime` | `instance_count`, `unique_user_dir`, `args` (multi-instance play) |
| `input_map` | `actions` and `axes` (see default `default_input_map.json`) |
| `splash_screens` | `enabled`, `allow_skip`, `entries` (image, fade in/hold/out) |
| `localization` | `enabled`, `default_locale`, `fallback_locale`, `tables_dir`, `csv_mode`, `pseudolocalization` |
| `misc` | `default_font`, `default_theme` (res:// paths) |

### Design resolution vs. window size

`window.width`/`window.height` are the **design resolution** — the fixed logical
canvas the game is authored in. UI anchor layout resolves against this size, and the
UI render camera projects it, so anchored controls keep the same relative positions at
any physical window size. The `graphics.scale_mode` then maps that canvas onto the
actual window (letterbox/stretch/crop/ignore).

By default the OS window opens at the design resolution. Enable `window.size_override`
to open it at `override_width`×`override_height` instead (e.g. design at 1920×1080 but
ship a 1280×720 window); the design canvas is scaled onto the override size by the
scale mode, so layout and anchoring are unaffected.

### Texture filtering & mipmaps

`graphics.texture_filtering` controls how content textures (sprites, images, UI
textures, tilemaps, mesh maps) are sampled, and — because the game renders directly at
the physical window resolution — whether they downscale smoothly when the window is
below the design resolution:

- `bilinear` (default) / `cubic` — smooth linear filtering **with a full mipmap chain**.
  Sprites and images minify cleanly instead of aliasing ("crunchy") when the window is
  smaller than the design size.
- `nearest` — crisp nearest-neighbour sampling with **no mipmaps**, for pixel-art games
  that want hard edges and no cross-frame bleeding on sprite sheets.

Mip generation is wired to this single setting (`RenderWorld::setTextureFiltering`), so
the runtime and the editor preview stay consistent. The mip chain is uploaded
backend-appropriately (OpenGL/WebGL generate it on the GPU; DirectX/Vulkan/Metal upload
the CPU-side chain), via the shared `CreateTexture2DFromImage` helper. Transparent edge
texels are colour-padded before mip generation ("fix alpha border") so minified sprites
do not show a bright fringe.

### Render-scale supersampling (sub-design window sizes)

When the physical window is **smaller than the design resolution** and a smooth filter
mode is selected, the runtime renders the whole frame into an offscreen target at the
design resolution and then downscales it to the window with linear filtering
(`RuntimeApp` + `RenderWorld::ensureFrameTarget`/`presentFrameTargetScaled`). This
supersamples the entire frame — sprites, UI and text stay crisp instead of being
rendered natively at the lower window resolution. It is equivalent to Godot's `viewport`
stretch mode and engages automatically; when the window is at or above the design
resolution the scene renders natively (no extra cost). Pixel-art projects (`nearest`
filtering) opt out and always render natively.

Autoload **singletons** are registered in project settings; at runtime they tick and are injected into every script state as bare-name globals (also `get_singleton(name)`).

## Scene file (`.scene`)

A serialized `Scene`: `type` + `properties` (from `ISerializable`) plus a `root` node tree.

```json
{
  "type": "Scene",
  "properties": { "name": "Main" },
  "root": { /* node object, see below */ }
}
```
(Saved scenes may also carry `"lupine_scene_version": "0.1.0"`.)

## Node object

Every node serializes as (verified against `Node::Serialize` / `Deserialize`):

```json
{
  "type": "Node2D",                       // registered node/type name (REQUIRED — drives instantiation)
  "uuid": "….",                           // optional; auto-generated if omitted
  "properties": {                          // node's own registered props (see 03 / 16)
    "name": "Player",
    "active": true,
    "visible": true,
    "unique_name_in_owner": true,
    "position": { "x": 100.0, "y": 50.0 },
    "rotation": 0.0,
    "scale": { "x": 1.0, "y": 1.0 },
    "z_index": 0
  },
  "components": [ /* component objects */ ],
  "children":   [ /* nested node objects */ ],
  "groups":     [ "players" ],            // optional
  "connections": [ … ]                    // optional, declarative signal connections (see below)
}
```

Only `type` is strictly required; everything else has defaults. `Deserialize` reads `properties` regardless of order, builds `components` then `children` (recursively), restores `groups`, and stashes `connections` for resolution after the whole tree is built.

### Transform property formats (the common authoring mistake)

| node base | `position` | `rotation` | `scale` | extra |
|---|---|---|---|---|
| `Node` | — | — | — | logic-only, no transform |
| `Node2D` | `{x,y}` | **float, RADIANS**, CCW about +Z (90° = `1.5708`) | `{x,y}` (default `1,1`) | `z_index` int |
| `Node3D` | `{x,y,z}` | **quaternion** `{x,y,z,w}` (identity = `0,0,0,1`) — *not* Euler | `{x,y,z}` (default `1,1,1`) | looks down local −Z |

UI/`Control` nodes use anchors/offsets/size-flags instead of a raw transform (see `12_ui_and_theme.md`). Full per-component property lists + defaults: `16_component_properties.md`.

### Signal connections (declarative)

A node's outgoing connections serialize under `connections` (key is `connections`, *not* `signal_connections`). Targets are addressed by UUID with a path fallback and resolved after the tree loads:

```json
"connections": [
  {
    "signal": "pressed",
    "target_uuid": "…",          // preferred — survives rename/reparent
    "target_path": "/Main/HUD/Score",
    "method": "on_start_pressed", // method/function called on the target
    "flags": 0,
    "binds": []                    // extra args prepended to the call
  }
]
```

This is the editor's "connect signal" output; you can also wire connections at runtime from script (`ref:connect(sig, target, method, flags)`, see `14_signals_events_groups.md`).

Declarative connections are also resolved for nodes spawned **at runtime**: when a fully-built subtree is attached to an already-live parent (`add_child`, `instantiate_prefab`, `instantiate_scene`, reparenting), its stashed `connections` are resolved across the subtree before `on_ready` runs — the same order the scene-load path uses. When a prefab is instantiated its node/component UUIDs are regenerated, and `target_uuid`s that point inside the same prefab are remapped to the new UUIDs so intra-prefab connections still resolve (targets outside the subtree continue to resolve by `target_uuid`/`target_path` against the live scene).

## Component object

```json
{
  "type": "Sprite2D",
  "properties": {
    "texturePath": "res://player.png",
    "modulate": { "r": 1.0, "g": 1.0, "b": 1.0, "a": 1.0 }
  }
}
```

- Property values serialize by `PropertyValueType` (see `07_reference.md`): scalars as JSON primitives; `Vec2`→`{x,y}`; `Vec3`→`{x,y,z}`; `Color`→`{r,g,b,a}`; `Enum`→integer index; arrays as JSON arrays.
- Editor saves also emit `property_metadata` (declaration order, type) — optional for hand-authoring; the engine reads `properties` regardless.
- A script is attached via a `ScriptComponent` with a `script_path` property pointing at a `res://…` script (the language is inferred from extension).

> **Property-name casing — the #1 hand-authoring footgun.** The casing differs by where the property is declared, so **do not guess**:
> - **Node base** properties are `snake_case`: `name`, `active`, `visible`, `unique_name_in_owner`, `position`, `rotation`, `scale`, `z_index`.
> - **Component** properties are almost all `camelCase`: `texturePath`, `shapeType`, `fontSize`, `cornerRadius`, `angularDamping`, `outlineColor`, … (a few are snake_case, e.g. `ScriptComponent.script_path`).
>
> An unrecognised key is silently ignored (the default is kept), so a mis-cased key fails quietly. Always take the exact key from `16_component_properties.md` (extracted from each component's `DefineProperties()`), which is authoritative.

## Worked example — a minimal playable 2D scene

A root `Node2D`, a player (sprite + body + script), and a UI overlay. Hand-authorable; the editor produces the same shape (plus `property_metadata` and `uuid`s).

```json
{
  "type": "Scene",
  "properties": { "name": "Level1" },
  "root": {
    "type": "Node2D",
    "properties": { "name": "Level1" },
    "children": [
      {
        "type": "Camera2D",
        "properties": { "name": "Camera", "active": true, "zoom": 1.0 }
      },
      {
        "type": "Node2D",
        "properties": { "name": "Player", "unique_name_in_owner": true, "position": { "x": 400.0, "y": 300.0 } },
        "groups": [ "players" ],
        "components": [
          { "type": "Sprite2D", "properties": { "texturePath": "res://art/player.png" } },
          { "type": "KinematicBody2DComponent", "properties": {} },
          { "type": "CollisionBody2DComponent", "properties": { "shapeType": 1 } },
          { "type": "ScriptComponent", "properties": { "script_path": "res://scripts/player.lua", "speed": 220.0 } }
        ]
      },
      {
        "type": "CameraUI",
        "properties": { "name": "HUD", "active": true },
        "children": [
          {
            "type": "Label",
            "properties": { "name": "Score", "unique_name_in_owner": true, "text": "0" }
          }
        ]
      }
    ]
  }
}
```

Notes that generalise:
- **One active camera per render space.** A 2D scene needs an active `Camera2D` to frame the world; UI draws through `CameraUI`; 3D through `Camera3D`. With none, the runtime supplies a default centered camera (see *Cameras* in `03`).
- **Exported script properties** (here `speed`) live alongside the component's own props in the `ScriptComponent` `properties` bag — the names come from the script's `@export` directives (`04`).
- **`unique_name_in_owner`** on Player and Score lets any script reach them with `Lupine.get_node("%Player")` / `Lupine.get_node("%Score")` without knowing the path (all scripting calls are namespaced — see `04`).
- **`main_scene`** in the `.lupine` project file (a `res://…` path) selects which scene boots. Switch scenes at runtime with `change_scene(path)` / overlay with `add_scene(path)`, which parents the overlay's root as a child of the current scene's root and re-parents it onto the next scene across a `change_scene` (`04`).

## Prefab (`.prefab`)

Same node-object format as a scene `root` (stored under `root_node`), representing a reusable subtree. Instantiate at runtime with `instantiate_prefab(path, parent)` (scripting) or the C-API prefab calls. Instances can override properties. Prefabs live in the project's `prefab/` folder and are discovered by the editor's type catalog (Add Node/Prefab dialog).

### Authoring and editing prefabs in the editor

- **Author from a node**: right-click a node in the Scene Tree → **Save as Prefab…** serializes that node and its whole subtree to a new `.prefab` (defaulting to `prefab/`). This uses `Prefab::CreateFromNode` + `Prefab::Save`.
- **Edit a prefab**: double-click a `.prefab` in the File Browser to open it in its own editor tab. The prefab is loaded into an editable single-root scene (via `EditorSession::OpenPrefab` → `SceneDocument::OpenPrefab`, preserving the prefab's UUIDs through `Prefab::InstantiateForEditing`), so the viewport, Scene Tree, Inspector and gizmos all work exactly as for a scene.
- **Save**: Ctrl+S / Save As on a prefab tab writes back to the `.prefab` format rather than `.scene`. The document tracks its kind via `SceneDocument::GetKind()` (`DocumentKind.Scene` / `DocumentKind.Prefab`); saving a prefab document routes through `SaveAsPrefab` so the round-trip stays in prefab format.

## Nested scenes (`SceneInstance`)

A `SceneInstance` node embeds an **external `.scene`** file as a live subtree — the composition primitive for levels built from reusable rooms/props, or a player scene reused across levels. It is a real node `type` you place in the tree:

```json
{
  "type": "SceneInstance",
  "properties": { "name": "Room_A", "scene_reference": "res://scenes/room.scene", "auto_reload": false, "position": { "x": 0, "y": 0 } }
}
```

- `scene_reference` (string, `res://…`) — the scene to instantiate. On load it clones that scene's whole node tree as children **with fresh UUIDs** (`StripUUIDs` + `CloneNodeTree`), so multiple instances of one scene don't collide.
- Each `SceneInstance` is a **unique-name boundary**: `%Name` resolution inside it is scoped to that instance and never leaks out or into a sibling instance (matches Godot owner semantics). This is what makes the same sub-scene safe to instance many times.
- Nestable recursively (scene → scene → scene). C++ API: `SetSceneReference`, `ReloadScene`, `GetInstancedRoot`.

**Prefab vs SceneInstance**: a prefab (`.prefab`) is a single-root reusable subtree, instantiated by copy (`instantiate_prefab`) and good for spawnable game objects; a `SceneInstance` references a full `.scene` by path and re-loads it, good for static level composition. Both clone with new UUIDs.

## Running a game

- **From the editor**: Play uses `runtime_controller.py` → `lupine_runtime.pyd`. Supports **multi-instance** play (N isolated processes, per-instance `user://`, `runtime.args` → `get_command_line_args()`), used for networking tests.
- **Standalone**: `lupine_runtime` exe loads `project.main_scene` and runs the main loop.

## Exporting

`editor/export/` (`export_manager.py`, `pack_file.py`). Project data + scenes + assets are packed (`PackFileWriter`) and appended to a per-platform template binary:

| Platform | Template → output |
|---|---|
| Windows x64 | `lupine_template_windows_x64.exe` → `.exe` (icon embedded) |
| Linux x64 | `lupine_template_linux_x64` → binary |
| macOS arm64 / x64 | `lupine_template_macos_*` → app/DMG |
| Web | HTML5 + WebAssembly → `index.html` + `.wasm` (itch.io-ready) |

### Debug templates

Every platform builds **two** templates (`cmake --build . --target lupine_export_templates`, or `web-build/build-web.*` for web). The debug one is the same runtime with symbols and function names preserved, so a crash in an exported game gives a readable stack trace instead of raw addresses:

| Platform | Debug template | What it adds |
|---|---|---|
| Windows x64 | `lupine_template_windows_x64_debug.exe` (+ `.pdb`) | PDB shipped next to the exe, frame pointers kept, console window attached |
| Linux x64 | `lupine_template_linux_x64_debug` | DWARF + `-rdynamic`, so `backtrace_symbols()` resolves names |
| macOS arm64 / x64 | `lupine_template_macos_*_debug` | DWARF in the binary |
| Web | `web/debug/index.*` | wasm name section kept (`-g2 --profiling-funcs`), `index.wasm.symbols`, assertions and stack-overflow checks on |

The exporter picks the debug template when a preset enables **Include debug symbols** (`ExportPreset.include_debug_symbols`); otherwise it uses the release one. If the requested variant isn't installed the export still runs from the other one and reports a warning. Debug templates define `LUPINE_DEBUG_TEMPLATE`, are larger, and stay optimized enough to be playable (`-O2` on web) — ship the release template.
