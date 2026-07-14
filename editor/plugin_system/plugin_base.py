"""
Base class for Lupine editor plugins.

Authors subclass :class:`EditorPlugin` in their plugin's editor script and
implement :meth:`on_enable` to register editor extensions (dock panels, menu
items, toolbar buttons, autoload singletons, ...) through the sandboxed
:class:`~editor.plugin_system.plugin_api.PluginAPI` exposed as ``self.api``.

Everything registered through ``self.api`` is tracked and removed
automatically when the plugin is disabled, so most plugins do not need to
implement :meth:`on_disable` at all.
"""

from __future__ import annotations

from typing import TYPE_CHECKING, Optional

if TYPE_CHECKING:
    from editor.plugin_system.plugin_api import PluginAPI
    from editor.plugin_system.manifest import PluginManifest


class EditorPlugin:
    """Base class every editor plugin must subclass.

    The plugin manager instantiates the subclass with no arguments, assigns
    :attr:`api` and :attr:`manifest`, then calls :meth:`on_enable`. When the
    plugin is disabled it calls :meth:`on_disable` and then tears down every
    extension registered through the API.
    """

    def __init__(self) -> None:
        self.api: Optional["PluginAPI"] = None
        self.manifest: Optional["PluginManifest"] = None

    def on_enable(self) -> None:
        """Called once when the plugin is enabled.

        Override this to build the plugin's editor UI and register extensions
        via ``self.api``. Raised exceptions are caught by the manager, logged,
        and cause the plugin to be rolled back to a disabled state.
        """

    def on_disable(self) -> None:
        """Called once when the plugin is disabled.

        Override only for cleanup the API cannot perform automatically (for
        example stopping a background timer the plugin created). All dock
        panels, menu items, toolbar buttons and autoloads registered through
        ``self.api`` are removed for you after this returns.
        """

    def on_project_saved(self) -> None:
        """Optional hook invoked when the editor saves the project."""

    @property
    def plugin_id(self) -> str:
        if self.manifest is not None:
            return self.manifest.plugin_id
        return self.__class__.__name__

    @property
    def display_name(self) -> str:
        if self.manifest is not None:
            return self.manifest.name
        return self.__class__.__name__
