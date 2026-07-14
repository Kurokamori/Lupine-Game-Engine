# 15 — Assets, Save Games & Localization

Three game-dev systems: asset/resource loading, the save-game toolkit, and localization. Scripting on `Lupine`/`lupine`; C-API under `capi/include/{asset,save,i18n,io}/`.

## Assets & files

Virtual paths (sandboxed; `..` rejected): `res://` project root (read-only at runtime), `user://` per-user save dir (read-write), `temp://` scratch.

File I/O (see `04_scripting.md`): `read_text_file`, `write_text_file`, `append_text_file`, `read_bytes_file`, `write_bytes_file`, `file_exists`, `make_directory`, `list_directory`, `delete_file_path`. JSON: `to_json`, `from_json`, `read_json`, `write_json`. C-API: `io/lc_filesystem.h` (`lc_fs_*` real FS, `lc_vfs_*` virtual FS).

Textures/audio/models/fonts are referenced by `res://` path in component properties and loaded by the asset manager (refcounted). C-API: `asset/lc_asset.h`.

### Archetypes / resources

Data assets (`.archetype` definitions, `.ares` instances) are the "ScriptableObject" equivalent — see `06_extensions_and_archetypes.md`. Load instances synchronously or asynchronously:
```lua
local inst = load_archetype("res://data/sword.ares")
-- async (3rd arg = streaming priority, higher streams in first):
local h = load_archetype_async("res://data/sword.ares", nil, Lupine.STREAM_PRIORITY_HIGH)
-- poll(h) each frame, or await_archetype(h)
```
Property type `Resource` (#15) on a component references an `.ares` asset.

#### Async priority streaming

The async loader reads + parses off the main thread (registry finalize happens on the
main thread when the engine pumps it each frame) and **streams by priority**: each request
carries a priority and only up to a *streaming budget* of requests run on worker threads at
once. A higher-priority request submitted while lower-priority ones are still queued jumps
ahead of them; an already-running request is never preempted. This lets gameplay-critical
assets load ahead of speculative/background ones and bounds the I/O a burst of requests
causes.

Priority bands (plain ints, higher = first): `STREAM_PRIORITY_LOW` (-100), `_NORMAL` (0),
`_HIGH` (100), `_CRITICAL` (1000). Scripting API (Lua/MRuby/MicroPython, on `Lupine`/`lupine`):
```lua
local h = load_archetype_async(path, callback_or_nil, Lupine.STREAM_PRIORITY_HIGH)
load_archetype_definition_async(path, callback_or_nil, priority)
set_archetype_load_priority(h, Lupine.STREAM_PRIORITY_CRITICAL)  -- re-prioritize while queued
get_archetype_load_priority(h)
set_archetype_streaming_budget(2)      -- max concurrent worker loads (0 = auto = thread count)
get_archetype_streaming_budget()
get_archetype_inflight_count()         -- running on a worker now
get_archetype_queued_count()           -- queued, not yet started
```
C-API: `asset/lc_async_asset.h` — `lc_async_load_archetype_instance` / `_definition` (with a
`priority` arg), `lc_async_set_priority` / `_get_priority`, `lc_async_set_max_concurrent` /
`_get_max_concurrent`, `lc_async_get_inflight_count` / `_queued_count` / `_pending_count`,
`lc_async_pump`, `lc_async_get_status` / `_is_complete`, `lc_async_get_resolved_fields_json` /
`_get_instance_path`, `lc_async_cancel` / `_forget` / `_reset`, and the `LC_ASYNC_PRIORITY_*`
constants. Core: `asset::AsyncAssetLoader` (`SetMaxConcurrentLoads` / `SetPriority` / the
`priority` arg on the load methods).

## Save games

`save::SaveGameManager` over `user://`: named slots, atomic write + backup, schema versioning/migration, pluggable formats (json/cbor/msgpack) and transforms (e.g. XOR obfuscation). Full design: `docs/old/SAVE_SYSTEM.md`.

```lua
save_game("slot1", { hp = 80, level = 3, pos = {x=10,y=20} }, { playtime = 3600 })
local data = load_game("slot1")
if save_slot_exists("slot1") then ... end
```
API: `save_game(slot,data,meta)`, `load_game(slot)`, `save_slot_exists(slot)`, `delete_save_slot(slot)`, `list_save_slots()`, `list_save_slot_infos()`, `get_save_slot_info(slot)`, `quick_save_game`/`quick_load_game`, `auto_save_game`. Config: `set_save_format(fmt)`, `set_save_obfuscation_key(key)`. `data` is any JSON-serializable table; `meta` is shown in slot listings. C-API: `save/lc_savegame.h`.

Capture/restore scene state into a save with `capture_scene_state([group])` (group capture/restore via `SceneSaveState`).

## Localization

`LocalizationManager`: translation tables (`.loctable`/`.csv`), keys, tags, plurals, format args, pseudolocalization, CSV hot-reload. Full design: `docs/old/LOCALIZATION.md`.

```lua
local s  = tr("menu.start")                       -- lookup
local s2 = tr_fmt("hud.score", { score = 1200 })  -- format args
local s3 = tr_plural("inv.items", count, { n = count })
set_locale("fr"); Lupine.log_info(get_locale())
```
API: `tr(key[,table])`, `tr_fmt(key,args[,table])`, `tr_plural(key,count,args[,table])`, `set_locale(l)`, `get_locale()`, `get_fallback_locale()`, `get_locales()`, `reload_localization()`. C-API: `i18n/`.

Project settings (`localization` section of `.lupine`): `enabled`, `default_locale`, `fallback_locale`, `tables_dir`, `csv_mode`, `pseudolocalization`. Text components (`Label`, etc.) can resolve keys automatically. Edit tables in the editor's localization dialog/settings tab.
