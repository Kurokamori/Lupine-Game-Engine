"""
Plugins management dialog.

Lists every plugin discovered in the open project's ``plugins/`` directory and
lets the user enable or disable each one live. Enabling a plugin immediately
loads its editor extension and registers its autoload singletons; disabling it
removes everything it added.
"""

from __future__ import annotations

import os
import subprocess
import sys
from typing import Any, List, Optional

from PyQt6.QtCore import Qt
from PyQt6.QtWidgets import (
    QCheckBox,
    QDialog,
    QHBoxLayout,
    QLabel,
    QListWidget,
    QListWidgetItem,
    QPushButton,
    QTextEdit,
    QVBoxLayout,
    QWidget,
)

from plugin_system.plugin_manager import PluginInfo, PluginManager


class PluginsDialog(QDialog):
    """Dialog for enabling / disabling project plugins."""

    def __init__(self, plugin_manager: PluginManager,
                 parent: Optional[QWidget] = None) -> None:
        super().__init__(parent)
        self._manager: PluginManager = plugin_manager
        self.setWindowTitle("Plugins")
        self.setMinimumSize(640, 460)
        self._build_ui()
        self.refresh()

    def _build_ui(self) -> None:
        layout: QVBoxLayout = QVBoxLayout(self)
        layout.setContentsMargins(10, 10, 10, 10)
        layout.setSpacing(8)

        header: QLabel = QLabel(
            "Plugins are discovered in the project's <b>plugins/</b> folder. "
            "Enabling a plugin lets it extend the editor and register game "
            "autoload singletons."
        )
        header.setWordWrap(True)
        layout.addWidget(header)

        body: QHBoxLayout = QHBoxLayout()
        body.setSpacing(8)

        self._list: QListWidget = QListWidget()
        self._list.setMinimumWidth(240)
        self._list.currentItemChanged.connect(self._on_selection_changed)
        body.addWidget(self._list, 1)

        details_container: QWidget = QWidget()
        details_layout: QVBoxLayout = QVBoxLayout(details_container)
        details_layout.setContentsMargins(0, 0, 0, 0)
        details_layout.setSpacing(6)

        self._title_label: QLabel = QLabel("")
        self._title_label.setStyleSheet("font-size: 14px; font-weight: 600;")
        details_layout.addWidget(self._title_label)

        self._meta_label: QLabel = QLabel("")
        self._meta_label.setProperty("secondary", True)
        self._meta_label.setWordWrap(True)
        details_layout.addWidget(self._meta_label)

        self._enabled_checkbox: QCheckBox = QCheckBox("Enabled")
        self._enabled_checkbox.toggled.connect(self._on_enabled_toggled)
        details_layout.addWidget(self._enabled_checkbox)

        self._description: QTextEdit = QTextEdit()
        self._description.setReadOnly(True)
        details_layout.addWidget(self._description, 1)

        body.addWidget(details_container, 2)
        layout.addLayout(body, 1)

        button_row: QHBoxLayout = QHBoxLayout()
        refresh_btn: QPushButton = QPushButton("Rescan")
        refresh_btn.setToolTip("Re-scan the plugins folder for new plugins")
        refresh_btn.clicked.connect(self._on_rescan)
        button_row.addWidget(refresh_btn)

        open_folder_btn: QPushButton = QPushButton("Open Plugins Folder")
        open_folder_btn.clicked.connect(self._open_plugins_folder)
        button_row.addWidget(open_folder_btn)

        button_row.addStretch()

        close_btn: QPushButton = QPushButton("Close")
        close_btn.clicked.connect(self.accept)
        button_row.addWidget(close_btn)

        layout.addLayout(button_row)

    def refresh(self) -> None:
        """Reload the list of plugins from the manager."""
        previous_id: Optional[str] = self._current_plugin_id()
        self._list.blockSignals(True)
        self._list.clear()

        infos: List[PluginInfo] = self._manager.list_plugins()
        for info in infos:
            label: str = info.name
            if info.error:
                label = f"{info.name}  (error)"
            elif info.enabled:
                label = f"{info.name}  ●"
            item: QListWidgetItem = QListWidgetItem(label)
            item.setData(Qt.ItemDataRole.UserRole, info)
            self._list.addItem(item)

        self._list.blockSignals(False)

        restored: bool = False
        if previous_id is not None:
            for index in range(self._list.count()):
                info = self._list.item(index).data(Qt.ItemDataRole.UserRole)
                if info.plugin_id == previous_id:
                    self._list.setCurrentRow(index)
                    restored = True
                    break
        if not restored and self._list.count() > 0:
            self._list.setCurrentRow(0)
        elif self._list.count() == 0:
            self._show_empty()

    def _show_empty(self) -> None:
        self._title_label.setText("No plugins found")
        self._meta_label.setText("")
        self._enabled_checkbox.setEnabled(False)
        self._enabled_checkbox.blockSignals(True)
        self._enabled_checkbox.setChecked(False)
        self._enabled_checkbox.blockSignals(False)
        self._description.setPlainText(
            "Drop a plugin folder (containing plugin.json) into this project's "
            "plugins/ directory, then click Rescan."
        )

    def _current_info(self) -> Optional[PluginInfo]:
        item: Optional[QListWidgetItem] = self._list.currentItem()
        if item is None:
            return None
        return item.data(Qt.ItemDataRole.UserRole)

    def _current_plugin_id(self) -> Optional[str]:
        info: Optional[PluginInfo] = self._current_info()
        return info.plugin_id if info is not None else None

    def _on_selection_changed(self) -> None:
        info: Optional[PluginInfo] = self._current_info()
        if info is None:
            self._show_empty()
            return

        self._title_label.setText(info.name)

        if info.error:
            self._meta_label.setText(f"<span style='color:#e06c75;'>"
                                     f"Manifest error</span>")
            self._enabled_checkbox.setEnabled(False)
            self._enabled_checkbox.blockSignals(True)
            self._enabled_checkbox.setChecked(False)
            self._enabled_checkbox.blockSignals(False)
            self._description.setPlainText(info.error)
            return

        manifest = info.manifest
        meta_parts: List[str] = [f"id: {info.plugin_id}"]
        if manifest is not None:
            if manifest.version:
                meta_parts.append(f"version: {manifest.version}")
            if manifest.author:
                meta_parts.append(f"author: {manifest.author}")
            if manifest.has_editor_extension:
                meta_parts.append("provides editor extension")
            if manifest.autoloads:
                names: str = ", ".join(a.name for a in manifest.autoloads)
                meta_parts.append(f"autoloads: {names}")
        self._meta_label.setText("  •  ".join(meta_parts))

        self._enabled_checkbox.setEnabled(True)
        self._enabled_checkbox.blockSignals(True)
        self._enabled_checkbox.setChecked(info.enabled)
        self._enabled_checkbox.blockSignals(False)

        description: str = ""
        if manifest is not None:
            description = manifest.description or "(No description provided.)"
        self._description.setPlainText(description)

    def _on_enabled_toggled(self, checked: bool) -> None:
        info: Optional[PluginInfo] = self._current_info()
        if info is None or info.manifest is None:
            return
        self._manager.set_enabled(info.plugin_id, checked)
        self.refresh()

    def _on_rescan(self) -> None:
        self._manager.discover()
        self.refresh()

    def _open_plugins_folder(self) -> None:
        folder: str = os.path.join(self._manager_project_dir(), "plugins")
        os.makedirs(folder, exist_ok=True)
        try:
            if sys.platform.startswith("win"):
                os.startfile(folder)  # type: ignore[attr-defined]
            elif sys.platform == "darwin":
                subprocess.Popen(["open", folder])
            else:
                subprocess.Popen(["xdg-open", folder])
        except Exception as exc:
            print(f"PluginsDialog: could not open folder: {exc}")

    def _manager_project_dir(self) -> str:
        return str(getattr(self._manager, "_project_dir", ""))
