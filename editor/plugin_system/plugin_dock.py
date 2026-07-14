"""
Dock panel wrapper used to host plugin-provided widgets.

Plugins hand the API a plain ``QWidget``; the API wraps it in a
:class:`PluginDockPanel` so it gains the same custom title bar, dock / pop-out
behaviour and floating-dock crash protection as the editor's built-in panels.
"""

from __future__ import annotations

from typing import Optional

from PyQt6.QtWidgets import QWidget

from panels.base_panel import EditorPanel


class PluginDockPanel(EditorPanel):
    """An :class:`EditorPanel` that hosts a single plugin-supplied widget."""

    def __init__(self, title: str, content: QWidget, object_name: str,
                 owner_plugin_id: str, parent: Optional[QWidget] = None) -> None:
        self._pending_content: Optional[QWidget] = content
        self.owner_plugin_id: str = owner_plugin_id
        super().__init__(title, parent)
        self.setObjectName(object_name)
        if self._pending_content is not None:
            self.content_layout.addWidget(self._pending_content)
            self._pending_content.setParent(self.content_widget)

    def _setup_panel(self) -> None:
        """Content is attached after construction in :meth:`__init__`."""

    def get_panel_id(self) -> str:
        return self.objectName() or self.__class__.__name__
