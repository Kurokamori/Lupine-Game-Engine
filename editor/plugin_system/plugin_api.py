"""
The sandboxed API surface handed to editor plugins.

A :class:`PluginAPI` instance is created per enabled plugin and exposed as
``self.api`` on the plugin object. It is the only supported way for a plugin
to modify the editor. Every extension a plugin adds through this object is
tracked, so :meth:`teardown` can remove all of them cleanly when the plugin is
disabled or the editor closes.

The API intentionally exposes a curated set of capabilities rather than the
raw :class:`MainEditor`. This keeps plugins decoupled from editor internals
and lets the editor revoke everything a plugin added in one call.
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Tuple

from PyQt6.QtCore import QObject, Qt, pyqtSignal
from PyQt6.QtGui import QAction
from PyQt6.QtWidgets import QToolBar, QWidget

from plugin_system.manifest import PluginManifest
from plugin_system.plugin_dock import PluginDockPanel

_DOCK_AREAS: Dict[str, Qt.DockWidgetArea] = {
    "left": Qt.DockWidgetArea.LeftDockWidgetArea,
    "right": Qt.DockWidgetArea.RightDockWidgetArea,
    "top": Qt.DockWidgetArea.TopDockWidgetArea,
    "bottom": Qt.DockWidgetArea.BottomDockWidgetArea,
}

GLOBALS_FILENAME: str = "globals.json"


class PluginAPI(QObject):
    """Curated, self-cleaning interface a plugin uses to extend the editor."""

    selection_changed = pyqtSignal(object)

    def __init__(self, main_editor: Any, manifest: PluginManifest,
                 project_dir: str) -> None:
        super().__init__(main_editor)
        self._main_editor: Any = main_editor
        self._manifest: PluginManifest = manifest
        self._project_dir: Path = Path(project_dir)
        self._plugin_dir: Path = manifest.directory

        self._docks: List[PluginDockPanel] = []
        self._panel_keys: List[str] = []
        self._menu_actions: List[QAction] = []
        self._toolbar_actions: List[QAction] = []
        self._autoload_names: List[str] = []
        self._selected_node: Any = None
        self._scene_tree: Optional[QObject] = None
        self._torn_down: bool = False

        self._connect_selection_source()

    # ------------------------------------------------------------------ #
    # Read-only context accessors
    # ------------------------------------------------------------------ #
    @property
    def manifest(self) -> PluginManifest:
        return self._manifest

    @property
    def plugin_id(self) -> str:
        return self._manifest.plugin_id

    def get_main_window(self) -> Any:
        """Return the :class:`MainEditor` window.

        Provided as an escape hatch for advanced plugins. Prefer the dedicated
        helpers below so the plugin can be torn down cleanly.
        """
        return self._main_editor

    def get_editor_bridge(self) -> Any:
        """Return the C++ editor bridge (rendering / scene services)."""
        return getattr(self._main_editor, "editor_bridge", None)

    def get_editor_session(self) -> Any:
        """Return the editor session object."""
        return getattr(self._main_editor, "editor_session", None)

    def get_project_path(self) -> str:
        """Absolute path to the open project directory."""
        return str(self._project_dir)

    def get_plugin_path(self) -> str:
        """Absolute path to this plugin's own directory."""
        return str(self._plugin_dir)

    def resolve_plugin_path(self, relative: str) -> str:
        """Resolve a path relative to the plugin folder to an absolute path."""
        return str((self._plugin_dir / relative).resolve())

    def get_selected_node(self) -> Any:
        """Return the node currently selected in the Scene Tree, or ``None``."""
        return self._selected_node

    # ------------------------------------------------------------------ #
    # Logging
    # ------------------------------------------------------------------ #
    def log(self, message: str, level: str = "Info") -> None:
        """Write a message to the editor Console panel (falls back to stdout)."""
        prefixed: str = f"[{self._manifest.name}] {message}"
        console: Any = self._panel("console")
        if console is not None and hasattr(console, "log_message"):
            try:
                console.log_message(prefixed, level)
                return
            except Exception:
                pass
        print(f"{level}: {prefixed}")

    # ------------------------------------------------------------------ #
    # Dock panels
    # ------------------------------------------------------------------ #
    def add_dock_panel(self, content: QWidget, title: Optional[str] = None,
                       area: str = "right",
                       panel_id: Optional[str] = None) -> PluginDockPanel:
        """Dock a widget into the main editor window and return its dock.

        ``area`` is one of ``"left"``, ``"right"``, ``"top"`` or ``"bottom"``.
        The dock is registered with the floating-dock guard and removed when
        the plugin is disabled.
        """
        resolved_title: str = title or self._manifest.name
        resolved_panel_id: str = panel_id or "panel"
        object_name: str = f"PluginDock_{self.plugin_id}_{resolved_panel_id}"
        panel_key: str = f"plugin:{self.plugin_id}:{resolved_panel_id}"

        dock: PluginDockPanel = PluginDockPanel(
            resolved_title, content, object_name, self.plugin_id, self._main_editor
        )

        dock_area: Qt.DockWidgetArea = _DOCK_AREAS.get(
            area.lower(), Qt.DockWidgetArea.RightDockWidgetArea
        )

        if hasattr(self._main_editor, "_register_panel"):
            self._main_editor._register_panel(panel_key, dock)
        self._main_editor.addDockWidget(dock_area, dock)
        # Keep the dock attached to the requested area. Forcing floating off
        # after the add guards against Qt promoting it to a top-level window
        # (which would strand it at screen origin with only its minimum size).
        dock.setFloating(False)
        dock.setVisible(True)
        dock.raise_()

        self._docks.append(dock)
        self._panel_keys.append(panel_key)
        return dock

    def add_bottom_panel(self, content: QWidget, title: Optional[str] = None,
                         panel_id: Optional[str] = None) -> PluginDockPanel:
        """Convenience wrapper that docks a widget into the bottom area."""
        return self.add_dock_panel(content, title=title, area="bottom",
                                   panel_id=panel_id)

    # ------------------------------------------------------------------ #
    # Menus and toolbars
    # ------------------------------------------------------------------ #
    def add_tool_menu_item(self, label: str, callback: Callable[[], None],
                           shortcut: Optional[str] = None) -> Optional[QAction]:
        """Add an item to the editor's ``Tools`` menu.

        Returns the created :class:`QAction`, or ``None`` if the Tools menu is
        unavailable.
        """
        tools_menu: Any = getattr(self._main_editor, "tools_menu", None)
        if tools_menu is None:
            self.log("Tools menu is unavailable; cannot add menu item.",
                     "Warning")
            return None

        action: QAction = QAction(label, self._main_editor)
        if shortcut:
            action.setShortcut(shortcut)
        action.triggered.connect(lambda _checked=False: self._safe_call(callback))
        tools_menu.addAction(action)
        self._menu_actions.append(action)
        return action

    def add_toolbar_button(self, text: str, callback: Callable[[], None],
                           tooltip: Optional[str] = None) -> QAction:
        """Add a button to the shared editor "Plugins" toolbar."""
        toolbar: QToolBar = self._ensure_plugin_toolbar()
        action: QAction = QAction(text, self._main_editor)
        if tooltip:
            action.setToolTip(tooltip)
        action.triggered.connect(lambda _checked=False: self._safe_call(callback))
        toolbar.addAction(action)
        self._toolbar_actions.append(action)
        return action

    # ------------------------------------------------------------------ #
    # Game-side autoload singletons
    # ------------------------------------------------------------------ #
    def register_autoload_singleton(self, global_name: str,
                                    plugin_relative_script: str) -> bool:
        """Register a game autoload singleton backed by a plugin script.

        ``plugin_relative_script`` is resolved relative to the plugin folder
        and written into the project's ``globals.json`` so the runtime loads
        it at play time. The entry is tagged with this plugin's id and removed
        automatically when the plugin is disabled.
        """
        if not global_name.isidentifier():
            self.log(f"Invalid singleton name '{global_name}'.", "Error")
            return False

        script_path: Path = (self._plugin_dir / plugin_relative_script).resolve()
        if not script_path.is_file():
            self.log(
                f"Singleton script not found: {plugin_relative_script}", "Error"
            )
            return False

        try:
            project_relative: str = script_path.relative_to(
                self._project_dir
            ).as_posix()
        except ValueError:
            self.log(
                "Singleton script must live inside the project directory.",
                "Error",
            )
            return False

        data: Dict[str, Any] = self._read_globals()
        singletons: List[Dict[str, Any]] = data.setdefault("singletons", [])

        for entry in singletons:
            if entry.get("global_name") == global_name:
                owner: Optional[str] = entry.get("plugin")
                if owner not in (None, self.plugin_id):
                    self.log(
                        f"Singleton '{global_name}' is already defined "
                        f"elsewhere; skipping.",
                        "Warning",
                    )
                    return False
                entry["script_path"] = project_relative
                entry["plugin"] = self.plugin_id
                break
        else:
            singletons.append({
                "global_name": global_name,
                "script_path": project_relative,
                "plugin": self.plugin_id,
            })

        if self._write_globals(data):
            if global_name not in self._autoload_names:
                self._autoload_names.append(global_name)
            self.log(f"Registered autoload singleton '{global_name}'.")
            return True
        return False

    def unregister_autoload_singleton(self, global_name: str) -> bool:
        """Remove a previously registered plugin singleton from globals.json."""
        data: Dict[str, Any] = self._read_globals()
        singletons: List[Dict[str, Any]] = data.get("singletons", [])
        remaining: List[Dict[str, Any]] = [
            entry for entry in singletons
            if not (entry.get("global_name") == global_name
                    and entry.get("plugin") == self.plugin_id)
        ]
        if len(remaining) == len(singletons):
            return False
        data["singletons"] = remaining
        result: bool = self._write_globals(data)
        if global_name in self._autoload_names:
            self._autoload_names.remove(global_name)
        return result

    # ------------------------------------------------------------------ #
    # Teardown
    # ------------------------------------------------------------------ #
    def teardown(self) -> None:
        """Remove every extension this plugin registered. Idempotent."""
        if self._torn_down:
            return
        self._torn_down = True

        self._disconnect_selection_source()

        for name in list(self._autoload_names):
            try:
                self.unregister_autoload_singleton(name)
            except Exception as exc:
                print(f"PluginAPI: failed to remove autoload '{name}': {exc}")

        for action in self._menu_actions:
            tools_menu: Any = getattr(self._main_editor, "tools_menu", None)
            if tools_menu is not None:
                try:
                    tools_menu.removeAction(action)
                except Exception:
                    pass
        self._menu_actions.clear()

        toolbar: Optional[QToolBar] = getattr(
            self._main_editor, "_plugin_toolbar", None
        )
        for action in self._toolbar_actions:
            if toolbar is not None:
                try:
                    toolbar.removeAction(action)
                except Exception:
                    pass
        self._toolbar_actions.clear()
        if toolbar is not None and toolbar.actions() == []:
            toolbar.setVisible(False)

        panels: Dict[str, Any] = getattr(self._main_editor, "panels", {})
        for key in self._panel_keys:
            panels.pop(key, None)
        self._panel_keys.clear()

        for dock in self._docks:
            try:
                self._main_editor.removeDockWidget(dock)
                dock.setParent(None)
                dock.deleteLater()
            except Exception as exc:
                print(f"PluginAPI: failed to remove dock: {exc}")
        self._docks.clear()

    # ------------------------------------------------------------------ #
    # Internal helpers
    # ------------------------------------------------------------------ #
    def _panel(self, panel_id: str) -> Any:
        panels: Dict[str, Any] = getattr(self._main_editor, "panels", {})
        return panels.get(panel_id)

    def _ensure_plugin_toolbar(self) -> QToolBar:
        toolbar: Optional[QToolBar] = getattr(
            self._main_editor, "_plugin_toolbar", None
        )
        if toolbar is None:
            toolbar = QToolBar("Plugins")
            toolbar.setObjectName("PluginsToolbar")
            toolbar.setMovable(True)
            toolbar.setFloatable(True)
            self._main_editor.addToolBar(
                Qt.ToolBarArea.TopToolBarArea, toolbar
            )
            self._main_editor._plugin_toolbar = toolbar
        toolbar.setVisible(True)
        return toolbar

    def _connect_selection_source(self) -> None:
        scene_tree: Any = self._panel("scene_tree")
        if scene_tree is not None and hasattr(scene_tree, "node_selected"):
            try:
                scene_tree.node_selected.connect(self._on_node_selected)
                self._scene_tree = scene_tree
            except Exception:
                self._scene_tree = None

    def _disconnect_selection_source(self) -> None:
        if self._scene_tree is not None:
            try:
                self._scene_tree.node_selected.disconnect(self._on_node_selected)
            except Exception:
                pass
            self._scene_tree = None

    def _on_node_selected(self, node: Any) -> None:
        self._selected_node = node
        self.selection_changed.emit(node)

    def _safe_call(self, callback: Callable[[], None]) -> None:
        try:
            callback()
        except Exception as exc:
            self.log(f"Callback error: {exc}", "Error")
            print(f"PluginAPI[{self.plugin_id}]: callback error: {exc}")

    def _globals_path(self) -> Path:
        return self._project_dir / GLOBALS_FILENAME

    def _read_globals(self) -> Dict[str, Any]:
        path: Path = self._globals_path()
        if not path.is_file():
            return {"singletons": [], "variables": []}
        try:
            with open(path, "r", encoding="utf-8") as handle:
                data: Any = json.load(handle)
            if isinstance(data, dict):
                data.setdefault("singletons", [])
                data.setdefault("variables", [])
                return data
        except (OSError, json.JSONDecodeError) as exc:
            self.log(f"Could not read globals.json: {exc}", "Error")
        return {"singletons": [], "variables": []}

    def _write_globals(self, data: Dict[str, Any]) -> bool:
        path: Path = self._globals_path()
        try:
            with open(path, "w", encoding="utf-8") as handle:
                json.dump(data, handle, indent=2)
            return True
        except OSError as exc:
            self.log(f"Could not write globals.json: {exc}", "Error")
            return False
