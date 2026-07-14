# 03 — Nodes & Components

Headers live in `core/include/lupine/components/<Name>.hpp` (use the `type` name in scenes/scripts). Core node/component classes in `core/include/lupine/core/`.

## Node model in one paragraph

A **Scene** owns one **root Node**. Nodes nest into a tree; each node carries a transform (if 2D/3D), zero or more **Components** (data + behaviour), child nodes, group tags, and signal connections. Engine systems (render, physics, audio, input) iterate the tree and act on the components they recognise. Gameplay is a `ScriptComponent` or a native/custom component. There is no separate "entity" — the Node *is* the entity, the Component *is* the behaviour.

## Node (`core/Node.hpp`)

Base scene-tree element (logic-only; no transform). Source: `core/src/Node.cpp`.

### Registered properties (these are the `properties` keys in a `.scene`)

| key | type | default | meaning |
|---|---|---|---|
| `name` | string | `"Node"` | Node name; also the path segment and (with the flag) the `%Name` key |
| `active` | bool | `true` | Logic gate. Inactive → no lifecycle/process ticks for it or descendants |
| `visible` | bool | `true` | Render gate. Invisible → not drawn (children inherit via `*_in_hierarchy`) |
| `unique_name_in_owner` | bool | `false` | When true, resolvable anywhere in its scope by `%name` |

`uuid` is serialized as a sibling of `properties` (stable identity), not as a property. It is **optional when hand-authoring** — a fresh one is generated if absent; set it only when something references the node by UUID (signal connections, animation tracks).

### Tree navigation & mutation (C++ — mirrored on NodeRef in scripting)

- Hierarchy: `AddChild`, `InsertChild(child,i)`, `RemoveChild`, `ReparentTo(parent)`, `GetParent`, `GetChild(name|index)`, `GetChildCount`, `GetChildren`.
- Order: `MoveChild(child,i)`, `GetChildIndex(child)`, `GetIndexInParent`, `SetSiblingIndex(i)` (pure reorder — does not change ancestry).
- Lookup: `FindNode(path)`, `GetPath()`, `ResolveUniqueName(name)` (the `%name` mechanism), `GetComponent<T>()` / `GetComponent(typeName)`.
- Identity/state: `GetName`/`SetName`, `GetUUID`, `IsActive`/`SetActive`, `IsActiveInHierarchy`, `IsVisible`/`SetVisible`, `IsVisibleInHierarchy`.

**Node paths** (`FindNode`): `/`-separated **child-name** segments, resolved **relative to the calling node** (`GetChild` per segment). A single name with no `/` is a direct child. There is **no** `..`, no leading-`/` absolute form, and no `%` handling inside `FindNode` itself — `%Name` is resolved separately by `ResolveUniqueName` / the scripting `get_node` layer. `GetPath()` returns the absolute `/Root/Child/Leaf` form (for display/serialized targets), but that string is **not** accepted back by `FindNode` from an arbitrary node — query from the root, or use UUID / `%uniquename`.

**Unique names (`%Name`).** Set `unique_name_in_owner: true` on a node, then `Lupine.get_node("%Name")` (scripting) or `ResolveUniqueName("Name")` (C++) finds it anywhere within its **scope** regardless of depth/reparenting. The scope is the instanced subtree of the nearest `SceneInstance` ancestor, or the scene root otherwise — resolution never crosses into a nested `SceneInstance`. Prefer `%Name` over brittle relative paths.

**Groups** (Godot-style tags). `AddToGroup`/`RemoveFromGroup`/`IsInGroup`/`GetGroups`; serialized as a `groups` array on the node. Collect every member with `Scene::GetNodesInGroup(name)` (scripting: query via the tree). Use for "all enemies", "all pickups", etc.

**Interfaces** (capability contracts; see `18_interfaces.md`). `ImplementsInterface`, `GetImplementedInterfaces`, `VerifyInterface`; `Scene::GetNodesImplementingInterface(name)` is the runtime "find every Damageable".

### Lifecycle (Node virtuals; scripts use `on_*`, see `04`)

`OnReady()` (tree built, safe to find other nodes) → per tick `OnInput(dt)` / `OnInputEvent(event)` → `OnProcess(dt)` → `OnPhysicsProcess(dt)` → `OnRender()`; `OnDestroy()` on teardown. Components add `OnAwake` (before ready) and `OnLateUpdate`/`OnEnterTree`/`OnExitTree`. A component added to an **already-ready** node gets `OnAwake`+`OnReady` immediately (see `Node::AddComponent`); symmetrically, a **child node** attached to an already-ready parent at runtime (`AddChild`/`InsertChild`/`ReparentTo`) first has its subtree's declarative signal connections resolved (`ResolvePendingConnectionsRecursive`) and then runs its own `OnReady` immediately, recursing into its descendants and their components — the same connections-before-ready order the scene-load path uses. `OnReady` is guarded by an internal `m_Ready` flag, so it fires exactly once per node and re-adding is a no-op. During scene load nothing is prematurely readied — the root is readied once by `Scene::Initialize` after the whole tree is built. Topology emits the built-in signals `child_entered_tree` / `child_exiting_tree`.

### Base node `type`s

`Node` (logic-only) · `Node2D` (2D transform) · `Node3D` (3D transform) · `SceneInstance` (nests an external `.scene`; see `02`). UI nodes derive from `UIControl`. The camera nodes (`Camera2D`/`Camera3D`/`CameraUI`) are their own node types — see *Cameras* below.

## Node2D (`core/Node.hpp`) — 2D transform

Adds `position` (Vec2), `rotation` (float), `scale` (Vec2, default `1,1`), `z_index` (int, default 0). Transform = `T · R · S`; children inherit the parent's full global transform.

- **The `.scene` `rotation` field is in RADIANS**, counter-clockwise about +Z (it feeds `Mat4::Rotate`/`glm::rotate` and `std::cos/sin` directly). When hand-authoring a `.scene`, write radians (90° = `1.5708`). The **scripting** API (`set_rotation_2d`, etc.) takes **degrees** and converts — see `04`. C++ `Node2D::SetRotation` is the raw radian setter.
- **Global accessors**: `GetGlobalPosition` (applies parent rotation+scale+translation), `GetGlobalRotation` (sum of ancestor rotations), `GetGlobalScale` (product of ancestor scales), `GetGlobalTransformMatrix`. Only `Node2D` ancestors contribute — a `Node2D` parented under a plain `Node` uses its local transform as global.
- **`z_index`** orders 2D draw within the same camera (higher = in front); for sibling auto-sort by Y use a `YSort` container.

## Node3D (`core/Node.hpp`) — 3D transform

Adds `position` (Vec3), `rotation` (**Quat**, serialized as Vec4 `{x,y,z,w}`, default identity `0,0,0,1`), `scale` (Vec3, default `1,1,1`).

- **`rotation` is a quaternion** in the scene file — author it as `{"x":0,"y":0,"z":0,"w":1}`, not Euler angles. Scripting converts: `get/set_rotation_3d` use **Euler degrees** `(pitch,yaw,roll)` and convert to/from the quaternion internally.
- **Global accessors**: `GetGlobalPosition`/`GetGlobalRotation`/`GetGlobalScale`/`GetGlobalTransformMatrix` compose through `Node3D` ancestors via matrix multiply. `GetGlobalRotation` returns a **Quat** (`parentWorld * local`). Scripts reach it as `get_global_rotation_3d()` in **Euler degrees**, and `set_global_rotation_3d(p,y,r)` writes back the local rotation that lands on that world orientation (`local = parentWorld⁻¹ * world`) — see `04`. The C-API exposes both the quaternion (`lc_node3d_get/set_global_rotation`) and Euler (`lc_node3d_get/set_global_rotation_euler[_degrees]`) forms.
- Looks down local **−Z** (matters for `Camera3D` and any "forward" logic).

## Component model (`core/Component.hpp`)

Components inherit `core::Component`; rendering ones implement `IRenderableComponent`. Each declares `GetTypeName()` and registers typed properties (see `07_reference.md` for `PropertyValueType` and macros).

### Lifecycle hooks (C++)

`OnAwake()` → `OnReady()` → per-frame `OnUpdate(dt)` / `OnPhysicsUpdate(dt)` / `OnLateUpdate(dt)`, plus `OnEnterTree()`, `OnExitTree()`, `OnRender()`, `OnDestroy()`. Script lifecycle uses `on_*` names (see `04_scripting.md`).

---

## Component catalog

`type` is what you write in a `.scene` / pass to `add_component(type)`. All under `components/<Name>.hpp`.

### 2D graphics & rendering
| type | Notes |
|---|---|
| `Sprite2D` | Textured sprite; `texture`, `modulate`, region |
| `AnimatedSprite2D` | Spritesheet frame animation |
| `Image2D` | UI/canvas image; supports custom `.lsh` shaders |
| `Light2D` | 2D light; optional hard shadows from `LightOccluder2D` (set `shadowEnabled`) |
| `LightOccluder2D` | Occluder polygon/polyline that blocks `Light2D` shadows |
| `Line2D` | Polyline; `points` |
| `Shape2D` | Filled primitive shapes |
| `VectorGraphic2D` | Vector path graphics |
| `Particles2D` | CPU particle emitter (CPUParticles parity) |
| `TileMap2D` | Tile grid |
| `ParallaxBackground` | Drives `ParallaxLayer` children from the active Camera2D for multi-depth scrolling |
| `ParallaxLayer` | One parallax depth layer; `motionScale` sets scroll rate, `motionMirroring` for seamless tiling |
| `GifPlayer` | Animated GIF playback (stb); loop/ping-pong, fps override |
| `VideoPlayer` | Video playback (ffmpeg, gated `LUPINE_ENABLE_VIDEO`); audio→`AudioManager` bus |
| `Empty2D` | Editor-only marker for a `Node2D`; `point` crosshair or resizable `volume` rectangle. Renders nothing at runtime |

### 3D graphics & rendering
| type | Notes |
|---|---|
| `Sprite3D` | Billboard/world sprite |
| `AnimatedSprite3D` | Animated 3D sprite |
| `StaticMesh3D` | Static mesh render |
| `SkeletalMesh3D` | Skinned/skeletal mesh |
| `PrimitiveMesh3D` | Built-in primitive (box/sphere/…) |
| `MultiMeshGeneric` | GPU-instanced mesh + discrete LODs |
| `Label3D`, `Panel3D`, `ProgressBar3D`, `Button3D` | 3D-space UI |
| `Particles3D` | 3D CPU particle emitter |
| `DirectionalLight3D`, `OmniLight3D`, `SpotLight3D` | Lights |
| `WorldEnvironment` | Post-process stack (bloom/tonemap/SSAO/vignette/grain/etc.), per-view |
| `SubViewport` | Render a nested subtree to a texture (content must nest under it) |
| `CameraEffect*` | Stackable per-camera post-effects (blur/glow/outline/color grade/…); see *Camera effects* below |
| `Empty3D` | Editor-only marker for a `Node3D`; `point` three-axis cross or resizable `volume` wireframe cube. Renders nothing at runtime |

### Cameras

Three camera node types, one per render space. A scene renders each space through the **first active camera** of that type found in the tree (depth-first); add a camera node, set it active, and that space renders through it. With no camera of a type, the runtime supplies a default centered one.

| node | space | base | key properties |
|---|---|---|---|
| `Camera3D` | 3D world (Node3D subtree) | `Node3D` | `projection_type`, `fov`, `near_plane`/`far_plane`, `ortho_size`; pose from the node transform (looks down local −Z) |
| `Camera2D` | 2D world (Node2D subtree) | `Node2D` | `zoom`, `ortho_size`, `aspect_ratio`, `offset`; **position/rotation from the node transform** |
| `CameraUI` | UI / canvas (screen-space) | `Node` | `canvas_size`, `origin`, `position`, **`rotation`**, **`zoom`**, `scale_factor` (HiDPI), `pixel_perfect` |

**How transforms read.** `Camera2D` is a Node2D: its `rotation` rotates the view; magnify with the `zoom` property (the node `scale` is *not* used by the camera). `CameraUI` is a plain Node with no transform, so it carries its **own** `position`, `rotation` and `zoom` properties (don't confuse `zoom` — a canvas magnification — with `scale_factor`, which only compensates for HiDPI). On every camera, larger `zoom` magnifies content (shows less of the world); rotation is in radians and a positive value turns the visible scene in the same direction.

**Authoring vs. runtime — the common point of confusion.** The editor viewport always shows the scene through its **own free editor camera**, *not* through your `Camera2D`/`CameraUI`. So zoom, rotation, `offset`, follow, limits and shake on a camera node produce **no change in the editor viewport** — they only take effect when you **Play**. To preview framing while authoring, each camera draws a gizmo: `Camera2D` a pink frame + crosshair, `CameraUI` a blue frame + crosshair, both reflecting the node's rotation and zoom so the outline encloses exactly what the camera will show at runtime (the frame counter-rotates against `rotation`, which is correct — it is the captured world region). `Camera3D` draws a frustum gizmo.

**Follow / limits / shake** (Camera2D, CameraUI): `follow_target` (node path or `%UniqueName`) with `follow_smoothing`/`follow_speed`, `drag_*` dead-zone margins, `limit_*` bounds, and script-driven shake. These run **at runtime only** (gated to non-editor) so the editor view never drifts. Script via `node:call`-style camera methods (`camera_set_follow_target`, `camera_shake`, `camera_smooth_move_to`, …); see `04_scripting.md`. Mouse hit-testing for UI maps the cursor through the inverse of the exact view-projection the UI was drawn with, so a moved/rotated/zoomed UI camera keeps clicks precisely aligned with what it draws.

### Camera effects (stackable post-effects)

Each camera node (`Camera2D`/`Camera3D`/`CameraUI`) can carry any number of **`CameraEffect` components**. They form an ordered **stack** (component/attachment order) applied to that camera's image **after** the WorldEnvironment post-process, ping-ponged between HDR targets — so different cameras get entirely different looks, and you can combine and even repeat effects (e.g. two blurs, glow → outline → color grade) for a unique result. Every effect is an independent component: toggle it with the component **Enabled** flag, and drive every parameter from the inspector or any scripting language via generic property get/set (no per-effect bindings). This runs at runtime (Play); the editor viewport uses its own camera.

Two camera-node properties govern the stack:
- `effects_enabled` (bool, default true) — master on/off for the whole stack on that camera.
- `effect_render_mode` (enum, `0 = Separate`, `1 = Composite`) — **Separate** runs every effect as its own full-screen pass in exact order (each effect samples the previous one's output; most flexible). **Composite** folds consecutive *pointwise* effects into a single generated pass (fewer draws, identical result); multi-tap effects still run as their own passes.

| effect | kind | key properties |
|---|---|---|
| `CameraEffectColorGrade` | pointwise | `contrast`, `saturation`, `brightness`, `temperature`, `tint`, `colorFilter`, `colorLift`/`colorGamma`/`colorGain` |
| `CameraEffectTonemap` | pointwise | `exposure`, `mode` (Linear/Reinhard/ReinhardExtended/ACES/Filmic/AGX), `whitePoint` |
| `CameraEffectVignette` | pointwise | `vignetteColor`, `intensity`, `smoothness`, `roundness`, `centerX`/`centerY` |
| `CameraEffectFilmGrain` | pointwise | `intensity`, `size` (animated) |
| `CameraEffectColorInvert` | pointwise | `strength` |
| `CameraEffectPosterize` | pointwise | `levels`, `strength` |
| `CameraEffectHueShift` | pointwise | `hueDegrees`, `saturation`, `value` |
| `CameraEffectBlur` | multi-tap | `mode` (Box/Directional), `radius`, `samples`, `direction` |
| `CameraEffectGlow` | multi-tap | `threshold`, `intensity`, `radius`, `samples` |
| `CameraEffectOutline` | multi-tap | `mode` (Color/Depth), `outlineColor`, `thickness`, `threshold` |
| `CameraEffectPixelate` | multi-tap | `pixelSize` |
| `CameraEffectSharpen` | multi-tap | `amount` |
| `CameraEffectChromaticAberration` | multi-tap | `amount` |

*Pointwise* effects can fold together in Composite mode; *multi-tap* effects sample neighbouring pixels and always take a dedicated pass. `CameraEffectOutline` in `Depth` mode uses scene depth (3D/perspective cameras); on 2D cameras it falls back to the color (luminance) edge detector.

**Compositing.** Each camera's effects operate on that camera's *own* layer, captured with alpha. A camera that clears color (the first/base camera, or a SubViewport) overwrites its target; an overlay camera (e.g. a `CameraUI` drawn over a `Camera2D` scene) **alpha-blends** its effected layer over the cameras beneath, so putting a blur on the UI camera blurs the UI and leaves the world showing through — it no longer blacks out the lower cameras.

**Where effects apply.** Runtime cameras, and **`SubViewport`** cameras (effects on the camera inside a SubViewport subtree apply to its render-to-texture). In the **editor**, effects are previewed via the viewport's **Preview FX** toolbar toggle (off by default): when on, the active scene camera that carries effects is applied to the editor view (you still navigate with the editor's free camera; this previews the *look*, applied to the whole editor image).

### UI controls (derive from `UIControl`)
`UIControl` (base: anchors/offsets/size-flags/presets) · `Button` · `Label` · `RichTextLabel` · `LineEdit` · `TextEdit` · `Checkbox` · `CheckList` · `RadioButton` · `RadioList` · `ToggleButton` · `TextureButton` · `TextureToggleButton` · `Slider` · `SpinBox` · `ProgressBar` · `Dropdown` · `ItemList` · `Tree` · `PopupMenu` · `Panel` · `NineSlicePanel` · `ColorRect` · `StyleBox` · `UIImageDraw`.

UI hit-testing/drawing uses `GetBoundsSize()` (anchored size), not raw size.

### Layout containers
`Container` · `HorizontalContainer` · `VerticalContainer` · `GridContainer` · `CenterContainer` · `PaddingContainer` · `DockContainer` · `Stack` · `Wrap` · `ScrollContainer` · `TabContainer` · `SplitContainer` (draggable two-pane split) · `AspectRatioContainer` (holds a width:height ratio) · `LayoutSlot` (per-child attached properties for Dock/Stack) · `Spacer` · `YSort` (y-sorted draw order).

Containers drive their children's rects: a child under a container has its anchors, offsets and `width`/`height` overwritten every layout pass, and talks to the container through its **size flags** and `customMinSize` instead. See [12_ui_and_theme.md](12_ui_and_theme.md#containers-auto-layout).

### Physics 2D
| type | Notes |
|---|---|
| `RigidBody2DComponent` | Dynamic body (Box2D v3) |
| `StaticBody2DComponent` | Static collider |
| `KinematicBody2DComponent` | Script-moved body |
| `CollisionBody2DComponent` | Collision shape carrier |
| `AreaTrigger2DComponent` | Sensor / overlap signals |
| `CharacterController2D` | Move-and-slide controller |
| `RayCast2D` | Polling ray that reports the first body hit each physics frame |
| `ShapeCast2D` | Polling swept-circle cast (thick ray) reporting first contact + fraction |

### Physics 3D
`RigidBody3DComponent` · `StaticBody3DComponent` · `KinematicBody3DComponent` · `CollisionMesh3DComponent` · `AreaTrigger3DComponent` · `CharacterController3D` · `RayCast3D` (polling ray) · `ShapeCast3D` (polling swept sphere). RayCast3D/ShapeCast3D have no collision mask — 3D bodies have no layer/mask.

Collision uses 32 named layers/masks (project settings). Layer/mask split applies to 2D (3D mask support is limited — verify against the body).

### Navigation
2D: `NavigationRegion2D` (navmesh region) · `NavigationAgent2D` (pathfollow + RVO/ORCA avoidance) · `NavigationObstacle2D` (avoidance disc / static carve).

3D: `NavigationRegion3D` (Recast-style navmesh baked from collision/mesh geometry; voxel heightfield → slope/ledge/height filter → regions → mesh) · `NavigationAgent3D` (XZ funnel/A* pathfollow + ORCA avoidance) · `NavigationObstacle3D` (avoidance disc + static carve volume).

### Animation & tween
| type | Notes |
|---|---|
| `AnimationPlayer` | Keyframe clips; `call`-dispatched methods (`play`, `stop`, …) |
| `AnimationTree` | Blend-tree / state machine |
| `Tween` | Property interpolation w/ easings |
| `TweenSequence` | Chained tween steps |
| `Timer` | One-shot / repeating timer w/ callback |

### Audio
`AudioPlayer` (2D/3D playback) · `AudioListener` (listener pose). Buses + DSP via `AudioManager` (script `Lupine.*_bus_*`).

### Curves, paths, scatter
`Curve2D` · `Path2D` · `Curve3D` · `Path3D` · `PathFollow3D` · `ScatterMultiMesh` · `CollisionScatterMultiMesh` · `NodeScatter`.

`Curve3D`/`Path3D` are the 3D spline equivalents (camera rails, moving platforms, cutscene tracks, spline AI movement). Points live in the owning `Node3D`'s local space; debug rendering and the `*_world` query helpers transform through the node's global matrix, so the curve respects node rotation/scale. Each point also carries a **tilt** (roll about the tangent); `sample_up_vector`/`sample_orientation` build a tilt-aware orientation frame for banked rails. Scriptable in all languages via `call` (e.g. `node:call("Path3D", "start_following")`, `get_current_position_world`, `get_closest_progress`, `sample_orientation_world`).

`PathFollow3D` drives its owning `Node3D` along a `Curve3D`/`Path3D` (moving platforms, follow cameras, patrols). It locates the path via its `pathNode` property, or — when empty — the parent node (the common case: the follower is a child of the path node). Each follower keeps its own `progressRatio`, so many can ride one path. `rotationMode` (None/Forward/YawOnly) orients the node to the tangent (+ tilt), with `hOffset`/`vOffset` lateral/vertical offsets and `speed`/`loop`/`pingPong` auto-advance. Live-previews in the editor while scrubbing `progressRatio`. Scriptable via `call` (`set_progress_ratio`, `start_following`, `set_speed`, …).

`Curve3D`/`Path3D` have an interactive 3D editor: selecting one shows the curve-point inspector widget (point list, per-point position/tilt, bezier toggle, top-down preview). Anchors and the selected point's bezier handles render in the viewport as boxes; click-drag moves them on the view-facing plane, and "Create" mode adds points by clicking on the node's height plane.

### Networking (gated `LUPINE_ENABLE_NETWORKING`)
`NetworkObject` · `NetworkSpawner` · `NetworkSynchronizer` · `NetworkTransform2D` · `NetworkTransform3D` · `NetworkController` (client prediction) · `NetworkAnimator` · `NetworkRigidBody2D` · `NetworkRigidBody3D`. RPC via NodeRef (`rpc`, `rpc_id`), authority APIs, diagnostics via `Network.get_stats()`.

### Scripting / misc
`ScriptComponent` (attach a `.lua`/`.rb`/`.py` via `script_path`) · `CustomShaderParams` (typed params for `.lsh` shaders) · `ParticleTextures`.

### Custom `.lsh` shaders
2D UI/sprite components (`ColorRect`, `Image2D`, `Panel`, `Shape2D`) and 3D mesh components (`StaticMesh3D` + `SkeletalMesh3D` per material slot via the "LSH" field; `PrimitiveMesh3D` via `customLshShaderPath`) can each attach a custom `.lsh` shader that is translated to the active backend at runtime. A `.lsh` shader can declare `#render_mode` (blend/cull/depth/two-sided), use built-ins like `TIME`/`FRONT_FACING`/`TEXTURE_PIXEL_SIZE`/`PRIMARY_SHADOW_ATTENUATION`, sampler hints (`@source_color`/`@filter`/`@repeat`), and `u_SceneTexture` for mid-scene screen reads. See `19_shaders.md` for the full language reference.

---

For the exact **properties + defaults** of any component, see `16_component_properties.md` (every component, extracted from source). For a component's **signals**, see the catalog in `14_signals_events_groups.md`. For end-to-end **gameplay recipes** (movement, spawning, pickups, damage, scene flow, save/load) see `20_game_patterns.md`. The authoritative source for any property is the component's `DefineProperties()` in `core/src/components/<Name>.cpp`.
