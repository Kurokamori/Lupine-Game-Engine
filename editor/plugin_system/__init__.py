"""
Lupine editor plugin system.

Plugins live in a project's ``plugins/`` directory. Each plugin is a folder
containing a ``plugin.json`` manifest and (optionally) an editor entry script
that subclasses :class:`EditorPlugin`. Plugins extend the PyQt editor through
the sandboxed :class:`PluginAPI` and may register game-side autoload
singletons that are accessible from the running game.

Plugin authors typically write::

    from plugin_system import EditorPlugin

    class MyPlugin(EditorPlugin):
        def on_enable(self):
            self.api.add_tool_menu_item("Hello", self._hello)
        def _hello(self):
            self.api.log("Hello from my plugin!")
"""

from plugin_system.manifest import (
    MANIFEST_FILENAME,
    MANIFEST_FORMAT_VERSION,
    PluginAutoload,
    PluginManifest,
    PluginManifestError,
)
from plugin_system.plugin_api import PluginAPI
from plugin_system.plugin_base import EditorPlugin
from plugin_system.plugin_dock import PluginDockPanel
from plugin_system.plugin_manager import (
    LoadedPlugin,
    PluginInfo,
    PluginManager,
)

__all__ = [
    "MANIFEST_FILENAME",
    "MANIFEST_FORMAT_VERSION",
    "PluginAutoload",
    "PluginManifest",
    "PluginManifestError",
    "PluginAPI",
    "EditorPlugin",
    "PluginDockPanel",
    "LoadedPlugin",
    "PluginInfo",
    "PluginManager",
]
