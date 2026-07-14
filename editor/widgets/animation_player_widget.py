"""
Inspector widget for the AnimationPlayer component.

Shows the clip library and offers add/remove plus a button to open the animation
timeline editor for the selected player.
"""

import json
import os

from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel, QPushButton,
                             QListWidget, QFileDialog)
from PyQt6.QtCore import pyqtSignal


class AnimationPlayerPropertyWidget(QWidget):
    """Clip-library editor + 'Open Animation Timeline' launcher."""

    def __init__(self, component, editor_bridge, main_editor=None, parent=None):
        super().__init__(parent)
        self.component = component
        self.editor_bridge = editor_bridge
        self.main_editor = main_editor
        self._init_ui()
        self._reload_clips()

    def _init_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(5, 5, 5, 5)
        layout.setSpacing(6)

        layout.addWidget(QLabel("Animation Clips:"))
        self.clips_list = QListWidget()
        self.clips_list.setMaximumHeight(120)
        layout.addWidget(self.clips_list)

        buttons = QHBoxLayout()
        add_btn = QPushButton("Add Clip")
        add_btn.clicked.connect(self._on_add_clip)
        buttons.addWidget(add_btn)
        remove_btn = QPushButton("Remove")
        remove_btn.clicked.connect(self._on_remove_clip)
        buttons.addWidget(remove_btn)
        layout.addLayout(buttons)

        open_btn = QPushButton("Open Animation Timeline")
        open_btn.clicked.connect(self._on_open_editor)
        layout.addWidget(open_btn)

    def _reload_clips(self):
        self.clips_list.clear()
        if not self.component or not self.editor_bridge:
            return
        try:
            lib = json.loads(self.editor_bridge.call_component_method(
                self.component, "get_clip_library", "[]")) or []
            for entry in lib:
                name = entry.get("name", "")
                path = entry.get("path", "")
                self.clips_list.addItem("%s  ->  %s" % (name, path or "(inline)"))
        except Exception as exc:
            print("AnimationPlayerWidget: get_clip_library failed:", exc)

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

    def _on_add_clip(self):
        path, _ = QFileDialog.getOpenFileName(self, "Select .animclip", self._project_root() or "",
                                              "Animation Clip (*.animclip)")
        if not path:
            return
        name = os.path.splitext(os.path.basename(path))[0]
        try:
            self.editor_bridge.call_component_method(
                self.component, "add_clip", json.dumps([name, self._to_res(path)]))
            self.editor_bridge.call_component_method(self.component, "reload_clips", "[]")
        except Exception as exc:
            print("AnimationPlayerWidget: add_clip failed:", exc)
        self._reload_clips()

    def _on_remove_clip(self):
        item = self.clips_list.currentItem()
        if not item:
            return
        name = item.text().split("  ->  ")[0]
        try:
            self.editor_bridge.call_component_method(self.component, "remove_clip", json.dumps([name]))
            self.editor_bridge.call_component_method(self.component, "reload_clips", "[]")
        except Exception as exc:
            print("AnimationPlayerWidget: remove_clip failed:", exc)
        self._reload_clips()

    def _on_open_editor(self):
        if self.main_editor and "animation_timeline" in getattr(self.main_editor, "panels", {}):
            panel = self.main_editor.panels["animation_timeline"]
            if hasattr(self.main_editor, "_open_panel"):
                self.main_editor._open_panel("animation_timeline")
            panel.set_animation_player(self.component)
            panel.setVisible(True)
            panel.raise_()
