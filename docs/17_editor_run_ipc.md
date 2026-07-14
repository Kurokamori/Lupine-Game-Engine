# Editor Run / Runtime IPC

Every "Play" launch from the editor runs the game in its **own OS process** —
its own window, its own engine singletons (VFS, InputManager, script hosts,
graphics device) and its own main thread. Running one instance or many is the
same mechanism; the editor just spawns one process per instance. The editor
drives those processes over a small IPC control channel and mirrors their log
output into the Console panel.

This replaces the previous split where a single instance ran *in-process* (the
game sharing the editor's process singletons) and only multi-instance play used
separate processes. Sharing the editor's process meant the game could never have
its own clean SDL/OpenGL/VFS/script-host state or its own main thread; going
always-out-of-process fixes that and makes single- and multi-instance play
behave identically.

## Pieces

- `editor/runtime_ipc.py` — transport. Localhost TCP, length-prefixed JSON
  frames (4-byte big-endian length + UTF-8 JSON object), per-session random
  token auth.
  - `IpcServer` (editor): binds an ephemeral `127.0.0.1` port, accepts one
    connection per spawned instance, sends commands, buffers inbound events for
    the Qt main thread to drain.
  - `IpcClient` (child): connects back, drains commands from a background reader
    thread into a queue, streams events back.
- `editor/runtime_instance_runner.py` — the child entry point spawned for every
  instance. Runs the runtime on its own thread (`app.run_async()`); its main
  thread pumps SDL events (`app.process_events()`, required on the
  window-creating thread) and applies editor commands, while streaming
  `drain_log_messages()` and status changes back over IPC.
- `editor/runtime_controller.py` — `RuntimeController`. Always spawns
  process(es), wires each to a fresh `IpcServer`, and routes
  pause/resume/step/stop/reload-scene/load-scene/reload-scripts to the children
  as broadcast IPC commands. A single Qt timer drains IPC events (mirroring logs
  into the console, tracking paused/closed status) and prunes exited processes.

## Commands (editor → child)

| Command          | Effect in the child                                              |
|------------------|------------------------------------------------------------------|
| `pause`          | `app.pause()` — halts update/physics, keeps rendering            |
| `resume`         | `app.resume()`                                                   |
| `step`           | one update+physics frame while staying paused (`app.step()`)     |
| `reload_scene`   | `app.reload_scene()` — reloads current scene from disk           |
| `load_scene`     | `app.load_scene(path)`                                           |
| `reload_scripts` | `app.reload_scripts()` — hot-reload script sources in place      |
| `live_edit`      | `app.apply_live_edits(json)` — apply a coalesced batch of editor edits (node/component properties, node transforms, asset reloads, script reloads) to the running scene in place |
| `stop`           | shut the runtime down gracefully                                 |
| `set_time_scale` | honored only on builds exposing a time-scale setter (else no-op) |
| `ping`           | replies `pong`                                                   |

Events (child → editor): `hello`, `ready`, `log`, `status`, `scene`, `pong`,
`closed`, `error`.

If the editor link drops (editor closed/crashed) the child receives a synthetic
stop and shuts itself down, so no orphaned game windows are left behind. The
controller also terminates all instances on editor close.

## Toolbar

`▶ Play Game` / `▶ Play Scene` / `⟳ Relaunch` / `⏸ Pause` / `⏭ Step` / `⏹ Stop`.
Step is enabled only while paused; Stop while running. There is no longer a
manual "Hot Reload" button — editor edits hot-reload into the running game
automatically (see below).

## Native support (`RuntimeApp`)

`step()` and `reloadScripts()` were added to `RuntimeApp` (+ pybind
`step` / `reload_scripts`):

- `step()` sets a one-shot flag consumed by `runFrame()`; while paused it lets
  exactly one update+physics frame run, then re-freezes.
- `reloadScripts()` walks the current scene, calls `ScriptComponent::ReloadScript()`
  on every script component, then re-runs `OnAwake()`/`OnReady()` so the new code
  takes effect. The node graph and component instances are preserved; only
  script-side state is rebuilt (expected hot-reload semantics).

`applyLiveEdits(json)` parses a JSON array of edit ops and applies each to the
current scene under the scene state mutex (like `reloadScene()`), so edits made
in the editor while the game runs take effect without a restart and without
losing gameplay state. Op kinds: `node` (a node's own registered property, e.g.
transform — applied via `Node::Deserialize` like `SetNodePropertyDirect`),
`component` (a component property — `registry.SetValueFromJson` +
`OnPropertyChanged`, like `SetComponentPropertyDirect`), `rename`, `asset`
(mirrors `EditorBridge::ReloadAsset` — `AssetReloadManager::NotifyAssetChanged`
then `Component::OnAssetFileChanged` over the tree), and `scripts` (same as
`reloadScripts`). Nodes are matched by **UUID** (stable across the editor and
runtime processes, both loading the same scene file); components are matched by
their owner node's UUID plus their **index** in that node's component list
(identical in both processes because components deserialize in array order).
Targets that don't resolve — e.g. a node added in the editor after Play began —
are skipped silently; structural changes (add/remove/reparent nodes) are not
synced live.

The Python side is `hasattr`-gated, so the full feature works without these
bindings too: `step` falls back to a brief resume/pause, `reload_scripts` falls
back to `reload_scene`, and `apply_live_edits` falls back to `reload_scene`
(scripts still hot-reload) so edits remain visible at the cost of a state reset.
Building the runtime/`.pyd` enables the precise native paths.

## Automatic hot reload (editor edits → running game)

There is no manual reload button. While one or more play instances are running,
the editor pushes edits to them automatically over the `live_edit` command:

- **Node property + position changes** — the inspector's
  `_on_node_property_changed` and the viewport gizmo (`gizmo_drag_ended` →
  `InspectorPanel.push_transform_live()`) push the changed node properties.
- **Component property changes** — the inspector's
  `_on_component_property_changed` pushes the changed component property by
  `(node uuid, component index)`.
- **Asset changes** — `AssetFileWatcher` → `_on_asset_updated` pushes a reload of
  the changed asset path (textures/models/fonts/audio refresh live); script
  saves push a `scripts` reload.

`RuntimeController` coalesces these per target (so a rapid slider/gizmo drag
collapses to the latest value) and flushes them to every instance on a ~40 ms
timer (`push_node_property` / `push_component_property` / `push_node_rename` /
`push_asset_reload` / `push_reload_scripts`).

## Notes / limitations

- The profiler reads each instance's history from the JSON dump file it streams
  (`<user>/profiler/<project>/instance_<n>.json`); there is no longer an
  in-process "read live" profiler source.
- The auth token is passed on the child's command line — acceptable for
  localhost developer tooling.
- Live edits preserve gameplay state but only cover property/transform/asset/
  script changes. To re-sync edited *scene structure* (added/removed/reparented
  nodes), use `reload_scene` via the controller (state is reset).
