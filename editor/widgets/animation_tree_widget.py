"""
Inspector widget for the AnimationTree component.

Shows the graph path + a launcher for the blend-tree editor.
"""

import json
import os

from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel, QPushButton,
                             QLineEdit, QFileDialog)
from PyQt6.QtCore import pyqtSignal


class AnimationTreePropertyWidget(QWidget):
    """Graph-path editor + 'Open Blend Tree' launcher."""

    def __init__(self, component, editor_bridge, main_editor=None, parent=None):
        super().__init__(parent)
        self.component = component
        self.editor_bridge = editor_bridge
        self.main_editor = main_editor
        self._init_ui()
        self._load_values()

    def _init_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(5, 5, 5, 5)
        layout.setSpacing(6)

        row = QHBoxLayout()
        row.addWidget(QLabel("Graph:"))
        self.path_edit = QLineEdit()
        self.path_edit.setReadOnly(True)
        row.addWidget(self.path_edit)
        browse_btn = QPushButton("...")
        browse_btn.setMaximumWidth(30)
        browse_btn.clicked.connect(self._on_browse)
        row.addWidget(browse_btn)
        layout.addLayout(row)

        open_btn = QPushButton("Open Blend Tree Editor")
        open_btn.clicked.connect(self._on_open_editor)
        layout.addWidget(open_btn)

    def _load_values(self):
        if not self.component or not self.editor_bridge:
            return
        try:
            value_json = self.editor_bridge.get_component_property(self.component, "graphPath")
            value = json.loads(value_json)
            if isinstance(value, str):
                self.path_edit.setText(value)
        except Exception as exc:
            print("AnimationTreeWidget: get graphPath failed:", exc)

    def _project_root(self):
        if self.main_editor and getattr(self.main_editor, "project", None):
            return self.main_editor.project.get_directory()
        return None

    def _to_res(self, path):
        root = self._project_root()
        if root:
            try:
                rel = os.path.relpath(path, root).replace("\\", "/")
                if not rel.startswith(".."):
                    return "res://" + rel
            except ValueError:
                pass
        return path

    def _resolve(self, path):
        if path.startswith("res://"):
            root = self._project_root()
            if root:
                return os.path.join(root, path[len("res://"):])
        return path

    def _on_browse(self):
        path, _ = QFileDialog.getOpenFileName(self, "Select .animgraph", self._project_root() or "",
                                              "Animation Graph (*.animgraph)")
        if not path:
            return
        res = self._to_res(path)
        self.path_edit.setText(res)
        try:
            self.editor_bridge.set_component_property(self.component, "graphPath", json.dumps(res))
            self.editor_bridge.call_component_method(self.component, "reload_graph", "[]")
        except Exception as exc:
            print("AnimationTreeWidget: set graphPath failed:", exc)

    def _on_open_editor(self):
        if not (self.main_editor and "blend_tree" in getattr(self.main_editor, "panels", {})):
            return
        panel = self.main_editor.panels["blend_tree"]
        if hasattr(self.main_editor, "_open_panel"):
            self.main_editor._open_panel("blend_tree")
        path = self._resolve(self.path_edit.text().strip())
        if path and os.path.exists(path):
            panel.load_graph_file(path)
        panel.setVisible(True)
        panel.raise_()
