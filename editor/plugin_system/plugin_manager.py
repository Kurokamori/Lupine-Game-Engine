"""
Discovery, lifecycle and persistence for editor plugins.

The manager scans ``<project>/plugins/`` for plugin folders (each containing a
``plugin.json``), tracks which plugins are enabled (persisted in
``<project>/plugins/plugins.json``), and enables / disables them at runtime.

Enabling a plugin:
  * applies any autoload singletons declared in its manifest,
  * dynamically imports its editor script (if any),
  * instantiates its :class:`EditorPlugin` subclass and calls ``on_enable``.

Disabling reverses all of that. Every failure is caught and logged so a broken
plugin can never take the editor down with it.
"""

from __future__ import annotations

import importlib.util
import json
import sys
import traceback
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List, Optional

from plugin_system.manifest import (
    MANIFEST_FILENAME,
    PluginManifest,
    PluginManifestError,
)
from plugin_system.plugin_api import PluginAPI
from plugin_system.plugin_base import EditorPlugin

PLUGINS_DIRNAME: str = "plugins"
STATE_FILENAME: str = "plugins.json"


@dataclass
class LoadedPlugin:
    """A plugin that is currently enabled."""

    manifest: PluginManifest
    api: PluginAPI
    instance: Optional[EditorPlugin]
    module_name: Optional[str]
    added_sys_path: Optional[str]


@dataclass
class PluginInfo:
    """Summary of a discovered plugin for UI consumption."""

    manifest: Optional[PluginManifest]
    plugin_id: str
    name: str
    enabled: bool
    error: str = ""


class PluginManager:
    """Owns discovery and lifecycle of every plugin in the open project."""

    def __init__(self, main_editor: Any) -> None:
        self._main_editor: Any = main_editor
        self._project_dir: Path = Path(main_editor.project.get_directory())
        self._plugins_dir: Path = self._project_dir / PLUGINS_DIRNAME
        self._state_file: Path = self._plugins_dir / STATE_FILENAME

        self.discovered: Dict[str, PluginManifest] = {}
        self.manifest_errors: Dict[str, str] = {}
        self.loaded: Dict[str, LoadedPlugin] = {}
        self._enabled_ids: set[str] = self._load_state()

    # ------------------------------------------------------------------ #
    # Discovery
    # ------------------------------------------------------------------ #
    def discover(self) -> None:
        """Scan the project's ``plugins/`` directory for plugin folders."""
        self.discovered.clear()
        self.manifest_errors.clear()

        if not self._plugins_dir.is_dir():
            return

        for child in sorted(self._plugins_dir.iterdir()):
            if not child.is_dir():
                continue
            if not (child / MANIFEST_FILENAME).is_file():
                continue
            try:
                manifest: PluginManifest = PluginManifest.load(child)
            except PluginManifestError as exc:
                self.manifest_errors[child.name] = str(exc)
                self._log(f"Plugin '{child.name}' failed to load: {exc}",
                          "Error")
                continue
            if manifest.plugin_id in self.discovered:
                self.manifest_errors[manifest.plugin_id] = (
                    "Duplicate plugin id."
                )
                continue
            self.discovered[manifest.plugin_id] = manifest

    def load_enabled(self) -> None:
        """Enable every discovered plugin that is marked enabled on disk."""
        for plugin_id in list(self._enabled_ids):
            if plugin_id in self.discovered:
                self.enable_plugin(plugin_id, persist=False)

    # ------------------------------------------------------------------ #
    # Queries
    # ------------------------------------------------------------------ #
    def is_enabled(self, plugin_id: str) -> bool:
        return plugin_id in self.loaded

    def list_plugins(self) -> List[PluginInfo]:
        """Return summaries of every discovered plugin and manifest error."""
        infos: List[PluginInfo] = []
        for plugin_id, manifest in self.discovered.items():
            infos.append(PluginInfo(
                manifest=manifest,
                plugin_id=plugin_id,
                name=manifest.name,
                enabled=self.is_enabled(plugin_id),
            ))
        for folder, error in self.manifest_errors.items():
            infos.append(PluginInfo(
                manifest=None,
                plugin_id=folder,
                name=folder,
                enabled=False,
                error=error,
            ))
        infos.sort(key=lambda info: info.name.lower())
        return infos

    # ------------------------------------------------------------------ #
    # Enable / disable
    # ------------------------------------------------------------------ #
    def set_enabled(self, plugin_id: str, enabled: bool) -> bool:
        """Enable or disable a plugin, persisting the new state."""
        if enabled:
            return self.enable_plugin(plugin_id, persist=True)
        self.disable_plugin(plugin_id, persist=True)
        return True

    def enable_plugin(self, plugin_id: str, persist: bool = True) -> bool:
        """Enable a discovered plugin. Returns ``True`` on success."""
        if plugin_id in self.loaded:
            return True

        manifest: Optional[PluginManifest] = self.discovered.get(plugin_id)
        if manifest is None:
            self._log(f"Cannot enable unknown plugin '{plugin_id}'.", "Error")
            return False

        api: PluginAPI = PluginAPI(
            self._main_editor, manifest, str(self._project_dir)
        )
        module_name: Optional[str] = None
        added_sys_path: Optional[str] = None
        instance: Optional[EditorPlugin] = None

        try:
            for autoload in manifest.autoloads:
                api.register_autoload_singleton(autoload.name,
                                                autoload.script_path)

            if manifest.has_editor_extension:
                module, module_name, added_sys_path = self._import_editor_module(
                    manifest
                )
                plugin_cls = self._resolve_plugin_class(manifest, module)
                instance = plugin_cls()
                instance.api = api
                instance.manifest = manifest
                instance.on_enable()

            self.loaded[plugin_id] = LoadedPlugin(
                manifest=manifest,
                api=api,
                instance=instance,
                module_name=module_name,
                added_sys_path=added_sys_path,
            )

            if persist:
                self._enabled_ids.add(plugin_id)
                self._save_state()

            self._log(f"Enabled plugin '{manifest.name}'.")
            return True

        except Exception as exc:
            self._log(
                f"Failed to enable plugin '{manifest.name}': {exc}", "Error"
            )
            traceback.print_exc()
            try:
                if instance is not None:
                    instance.on_disable()
            except Exception:
                pass
            api.teardown()
            self._unload_module(module_name, added_sys_path)
            if persist:
                self._enabled_ids.discard(plugin_id)
                self._save_state()
            return False

    def disable_plugin(self, plugin_id: str, persist: bool = True) -> None:
        """Disable a plugin and remove everything it registered."""
        loaded: Optional[LoadedPlugin] = self.loaded.pop(plugin_id, None)
        if loaded is not None:
            if loaded.instance is not None:
                try:
                    loaded.instance.on_disable()
                except Exception as exc:
                    self._log(
                        f"Error in on_disable for '{plugin_id}': {exc}",
                        "Error",
                    )
                    traceback.print_exc()
            try:
                loaded.api.teardown()
            except Exception as exc:
                self._log(f"Error tearing down '{plugin_id}': {exc}", "Error")
            self._unload_module(loaded.module_name, loaded.added_sys_path)
            self._log(f"Disabled plugin '{loaded.manifest.name}'.")

        if persist:
            self._enabled_ids.discard(plugin_id)
            self._save_state()

    def shutdown(self) -> None:
        """Disable all loaded plugins without changing the persisted state.

        Used when the editor closes so plugin resources are released but the
        set of enabled plugins is preserved for the next session.
        """
        for plugin_id in list(self.loaded.keys()):
            self.disable_plugin(plugin_id, persist=False)

    # ------------------------------------------------------------------ #
    # Dynamic module loading
    # ------------------------------------------------------------------ #
    def _import_editor_module(self, manifest: PluginManifest):
        script_path: Optional[Path] = manifest.editor_script_path()
        if script_path is None or not script_path.is_file():
            raise PluginManifestError(
                f"Editor script '{manifest.editor_script}' not found."
            )

        added_sys_path: Optional[str] = None
        plugin_dir: str = str(manifest.directory)
        if plugin_dir not in sys.path:
            sys.path.insert(0, plugin_dir)
            added_sys_path = plugin_dir

        module_name: str = f"lupine_plugin_{manifest.plugin_id}"
        spec = importlib.util.spec_from_file_location(module_name, script_path)
        if spec is None or spec.loader is None:
            raise PluginManifestError(
                f"Could not create import spec for '{script_path}'."
            )
        module = importlib.util.module_from_spec(spec)
        sys.modules[module_name] = module
        spec.loader.exec_module(module)
        return module, module_name, added_sys_path

    def _resolve_plugin_class(self, manifest: PluginManifest, module: Any):
        if manifest.editor_class:
            plugin_cls = getattr(module, manifest.editor_class, None)
            if plugin_cls is None:
                raise PluginManifestError(
                    f"Class '{manifest.editor_class}' not found in "
                    f"'{manifest.editor_script}'."
                )
            if not (isinstance(plugin_cls, type)
                    and issubclass(plugin_cls, EditorPlugin)):
                raise PluginManifestError(
                    f"'{manifest.editor_class}' is not an EditorPlugin subclass."
                )
            return plugin_cls

        candidates = [
            value for value in vars(module).values()
            if isinstance(value, type)
            and issubclass(value, EditorPlugin)
            and value is not EditorPlugin
            and value.__module__ == module.__name__
        ]
        if not candidates:
            raise PluginManifestError(
                "No EditorPlugin subclass found in the editor script. Declare "
                "'editor_class' in plugin.json or define a subclass."
            )
        if len(candidates) > 1:
            raise PluginManifestError(
                "Multiple EditorPlugin subclasses found; set 'editor_class' "
                "in plugin.json to disambiguate."
            )
        return candidates[0]

    def _unload_module(self, module_name: Optional[str],
                       added_sys_path: Optional[str]) -> None:
        if module_name is not None:
            sys.modules.pop(module_name, None)
        if added_sys_path is not None and added_sys_path in sys.path:
            try:
                sys.path.remove(added_sys_path)
            except ValueError:
                pass

    # ------------------------------------------------------------------ #
    # State persistence
    # ------------------------------------------------------------------ #
    def _load_state(self) -> set[str]:
        if not self._state_file.is_file():
            return set()
        try:
            with open(self._state_file, "r", encoding="utf-8") as handle:
                data: Any = json.load(handle)
            enabled: Any = data.get("enabled", []) if isinstance(data, dict) else []
            if isinstance(enabled, list):
                return {str(item) for item in enabled}
        except (OSError, json.JSONDecodeError) as exc:
            print(f"PluginManager: could not read {self._state_file}: {exc}")
        return set()

    def _save_state(self) -> None:
        try:
            self._plugins_dir.mkdir(parents=True, exist_ok=True)
            with open(self._state_file, "w", encoding="utf-8") as handle:
                json.dump({"enabled": sorted(self._enabled_ids)}, handle, indent=2)
        except OSError as exc:
            print(f"PluginManager: could not write {self._state_file}: {exc}")

    # ------------------------------------------------------------------ #
    # Logging
    # ------------------------------------------------------------------ #
    def _log(self, message: str, level: str = "Info") -> None:
        panels: Dict[str, Any] = getattr(self._main_editor, "panels", {})
        console: Any = panels.get("console")
        if console is not None and hasattr(console, "log_message"):
            try:
                console.log_message(f"[Plugins] {message}", level)
                return
            except Exception:
                pass
        print(f"{level}: [Plugins] {message}")
