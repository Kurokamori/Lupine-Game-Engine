# 01 — Overview & Architecture

## Concept

Lupine = scene-tree engine (Godot model). A **Scene** owns a **root Node**; nodes nest into a tree. Each node carries **Components** (data + behaviour). Rendering, physics, audio, etc. are engine systems that act on components. Gameplay logic is a `ScriptComponent` (Lua/MRuby/MicroPython) or a native C++ component.

## Layers

```
editor/ (PyQt6)  ──imports──►  lupine_engine.pyd   (scene/component inspection bindings)
                              lupine_runtime.pyd   (playback control bindings)
                                     │
runtime/ (game exe)  ──────────────►│
console/ (test REPL) ──────────────►│
capi/  lupine_capi.dll (lc_* C ABI) │
                                     ▼
        lupine_core (static)  +  lupine_engine_lib (static)
        ECS, scene graph,        graphics, physics, audio,
        components, scripting     networking, input, viewport
```

## Repo layout

| Dir | Contents |
|---|---|
| `core/` | Engine core: `include/lupine/{core,components,math,rendering,physics2d,physics3d,audio,input,animation,navigation,network,scripting,ui,save,localization,asset,...}` + `src/` |
| `engine/` | Graphics/physics/audio/viewport systems + `lupine_engine.pyd` editor bindings |
| `runtime/` | Window + main loop; `lupine_runtime` exe + `lupine_runtime.pyd` |
| `capi/` | C ABI: `include/` headers (`lupine_c.h` umbrella), examples, test suite |
| `editor/` | PyQt6 editor: `main_editor.py`, `panels/`, `export/`, `project_file.py`, `runtime_controller.py` |
| `extension/` | Native extension SDK (`.lupineext`), C ABI interface + C++ SDK headers, examples |
| `console/` | `lupine_console` C++ test harness |
| `resources/` | Sample scripts, boot files, defaults |
| `docs/` | This documentation |
| `external/`, `vcpkg/` | Third-party deps |

## Build targets (`CMakeLists.txt`)

| Target | Type | Purpose |
|---|---|---|
| `lupine_core` | static lib | ECS, scene graph, components, scripting VMs, extension loader |
| `lupine_engine_lib` | static lib | Graphics, physics, audio, networking, input, viewport |
| `lupine_engine` | Python `.pyd` | Editor bindings (scene/component inspection) |
| `lupine_runtime_lib` | static lib | Window + main loop integration |
| `lupine_runtime_py` | Python `.pyd` | Runtime playback control from the editor |
| `lupine_runtime` | executable | Standalone game runtime / export template |
| `lupine_console` | executable | Interactive C++ test/debug console (desktop only) |
| `lupine_capi` | shared lib | C ABI for foreign-language bindings |
| extension SDK | header-only + cmake | `lupine_add_extension()` macro + author headers |

## Graphics backends

OpenGL, Vulkan, DirectX 11, DirectX 12, Metal, WebGL (export). Selected per platform at build/runtime. Shaders authored in `.lsh` (see `core/shaders/`); a runtime translator targets each backend.

## Optional features (compile gates)

- mRuby and MicroPython scripting are optional (`LUPINE_HAS_MRUBY`, MicroPython gate); Lua is always on.
- `LUPINE_ENABLE_NETWORKING`, `LUPINE_ENABLE_PROFILER` gate networking and the profiler (zero-cost when off).

## Namespaces (C++)

`lupine::core` (Node, Component, Scene, SceneManager, serialization, signals), `lupine::components` (all components), `lupine::math`, `lupine::asset`, `lupine::rendering`, `lupine::physics2d` / `physics3d`, `lupine::audio`, `lupine::input`, `lupine::scripting`, `lupine::ui`, `lupine::save`, `lupine::network`, `lupine::navigation`.

## Documentation map

| Doc | Topic |
|---|---|
| `02_projects_and_scenes` | `.lupine`/`.scene`/`.prefab` formats, node-object JSON, transforms, nested scenes, export |
| `03_nodes_and_components` | Node/Node2D/Node3D, tree/paths/groups/unique-names, lifecycle, full component catalog, cameras |
| `04_scripting` | Lua/MRuby/MicroPython API, annotations/exports, lifecycle, NodeRef/ComponentRef |
| `05_c_api` | `lc_*` C ABI |
| `06_extensions_and_archetypes` | Native C++ extensions, data archetypes |
| `07_reference` | `PropertyValueType`, serialization primitives |
| `08_input` | Actions/axes, devices, glyphs, contexts, local MP |
| `09_networking` · `10_physics` · `11_audio` · `12_ui_and_theme` | Subsystems |
| `13_animation_tween_timers` · `14_signals_events_groups` · `15_assets_save_localization` | Subsystems |
| `16_component_properties` | Every component's properties + defaults (source-extracted) |
| `17_editor_run_ipc` · `18_interfaces` · `19_shaders` | Editor play/IPC, capability contracts, `.lsh` shaders |
| `20_game_patterns` | **Cookbook** — complete recipes for building games |

Start at `02`+`03`+`04` to author scenes and script behaviour; jump to `20` for worked gameplay recipes.
