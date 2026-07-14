"""
Animation Timeline Editor Panel

A keyframe timeline for editing .animclip files on an AnimationPlayer: track list,
playhead scrubbing with live (non-destructive) scene preview, keyframe insert / move
/ delete, per-key interpolation, and playback. Pairs with the engine AnimationPlayer
component and the EditorBridge preview hooks.
"""

import copy
import json
import os

from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel, QPushButton,
                             QComboBox, QDoubleSpinBox, QSpinBox, QCheckBox, QSplitter,
                             QGroupBox, QInputDialog, QMessageBox, QFileDialog, QDialog,
                             QFormLayout, QLineEdit, QDialogButtonBox, QSizePolicy,
                             QTreeWidget, QTreeWidgetItem)
from PyQt6.QtCore import Qt, pyqtSignal, QTimer, QRectF, QPointF
from PyQt6.QtGui import QPainter, QColor, QPen, QBrush, QPolygonF

from .base_panel import EditorPanel

from editor.animation import anim_model as M


RULER_H = 26
ROW_H = 22
LABEL_W = 190
PAD = 8


class TimelineCanvas(QWidget):
    """Self-contained timeline: track labels, ruler, keyframe diamonds, playhead."""

    playhead_changed = pyqtSignal(float)
    key_selected = pyqtSignal(int, int)   # track index, key index (-1 if none)
    key_moved = pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.tracks = []
        self.length = 1.0
        self.fps = 30.0
        self.snap = True
        self.playhead = 0.0
        self.sel_track = -1
        self.sel_key = -1
        self._pps = 100.0
        self._dragging_key = False
        self._scrubbing = False
        self.setMinimumHeight(120)
        self.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)
        self.setFocusPolicy(Qt.FocusPolicy.StrongFocus)

    def set_data(self, tracks, length, fps):
        self.tracks = tracks
        self.length = max(float(length), 0.0001)
        self.fps = max(float(fps), 1.0)
        self.update()

    def _recalc_pps(self):
        avail = max(self.width() - LABEL_W - PAD * 2, 10)
        self._pps = avail / max(self.length, 0.0001)

    def time_to_x(self, t):
        return LABEL_W + PAD + t * self._pps

    def x_to_time(self, x):
        t = (x - LABEL_W - PAD) / max(self._pps, 0.0001)
        return max(0.0, min(t, self.length))

    def _snap_time(self, t):
        if self.snap and self.fps > 0:
            step = 1.0 / self.fps
            return round(t / step) * step
        return t

    def resizeEvent(self, event):
        self._recalc_pps()
        super().resizeEvent(event)

    def paintEvent(self, event):
        self._recalc_pps()
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)

        bg = QColor("#1e1a28")
        label_bg = QColor("#262030")
        grid = QColor("#332b40")
        text_col = QColor("#cfc6e0")
        key_col = QColor("#4cc2ff")
        key_sel = QColor("#ffd24c")
        method_col = QColor("#7cffa0")
        playhead_col = QColor("#ff5a7a")

        painter.fillRect(self.rect(), bg)
        painter.fillRect(0, 0, LABEL_W, self.height(), label_bg)

        # Ruler ticks (one per 0.1s, label per 0.5s when room allows).
        painter.setPen(QPen(grid, 1))
        painter.drawLine(LABEL_W, RULER_H, self.width(), RULER_H)
        painter.setPen(QPen(text_col, 1))
        t = 0.0
        while t <= self.length + 1e-4:
            x = self.time_to_x(t)
            painter.drawLine(int(x), RULER_H - 5, int(x), RULER_H)
            if abs((t * 2) - round(t * 2)) < 1e-4:
                painter.drawText(int(x) + 2, RULER_H - 8, "%.1f" % t)
            t += 0.1

        # Track rows.
        for i, track in enumerate(self.tracks):
            y = RULER_H + i * ROW_H
            if i % 2 == 0:
                painter.fillRect(LABEL_W, y, self.width() - LABEL_W, ROW_H, QColor("#221d2e"))
            painter.setPen(QPen(text_col, 1))
            painter.drawText(6, y + ROW_H - 7, M.track_label(track)[:28])

            is_method = track.get("type") == "method"
            cy = y + ROW_H // 2
            for ki, key in enumerate(track.get("keys", [])):
                x = self.time_to_x(key["t"])
                selected = (i == self.sel_track and ki == self.sel_key)
                color = key_sel if selected else (method_col if is_method else key_col)
                self._draw_diamond(painter, x, cy, 5, color)

        # Playhead.
        px = self.time_to_x(self.playhead)
        painter.setPen(QPen(playhead_col, 2))
        painter.drawLine(int(px), 0, int(px), self.height())
        painter.setBrush(QBrush(playhead_col))
        painter.drawPolygon(QPolygonF([
            QPointF(px - 5, 0), QPointF(px + 5, 0), QPointF(px, 8)]))

    def _draw_diamond(self, painter, x, y, r, color):
        painter.setPen(QPen(color.darker(160), 1))
        painter.setBrush(QBrush(color))
        painter.drawPolygon(QPolygonF([
            QPointF(x, y - r), QPointF(x + r, y), QPointF(x, y + r), QPointF(x - r, y)]))

    def _hit_key(self, pos):
        for i, track in enumerate(self.tracks):
            cy = RULER_H + i * ROW_H + ROW_H // 2
            if abs(pos.y() - cy) > 8:
                continue
            for ki, key in enumerate(track.get("keys", [])):
                x = self.time_to_x(key["t"])
                if abs(pos.x() - x) < 7:
                    return i, ki
        return -1, -1

    def mousePressEvent(self, event):
        # Take keyboard focus so Ctrl+C/Ctrl+V/Delete route to the timeline.
        self.setFocus(Qt.FocusReason.MouseFocusReason)
        pos = event.position()
        ti, ki = self._hit_key(pos)
        if ki >= 0:
            self.sel_track, self.sel_key = ti, ki
            self._dragging_key = True
            self.key_selected.emit(ti, ki)
            self.update()
            return
        # Otherwise scrub the playhead.
        if pos.x() >= LABEL_W:
            self._scrubbing = True
            self.playhead = self._snap_time(self.x_to_time(pos.x()))
            self.playhead_changed.emit(self.playhead)
            self.update()
        else:
            # Clicked a label: select the track.
            row = int((pos.y() - RULER_H) // ROW_H)
            if 0 <= row < len(self.tracks):
                self.sel_track, self.sel_key = row, -1
                self.key_selected.emit(row, -1)
                self.update()

    def mouseMoveEvent(self, event):
        pos = event.position()
        if self._dragging_key and self.sel_track >= 0 and self.sel_key >= 0:
            track = self.tracks[self.sel_track]
            new_t = self._snap_time(self.x_to_time(pos.x()))
            track["keys"][self.sel_key]["t"] = new_t
            track["keys"].sort(key=lambda kk: kk["t"])
            # Re-find index after sort.
            for ki, key in enumerate(track["keys"]):
                if abs(key["t"] - new_t) < 1e-6:
                    self.sel_key = ki
                    break
            self.key_moved.emit()
            self.update()
        elif self._scrubbing:
            self.playhead = self._snap_time(self.x_to_time(pos.x()))
            self.playhead_changed.emit(self.playhead)
            self.update()

    def mouseReleaseEvent(self, event):
        self._dragging_key = False
        self._scrubbing = False


class AddTrackDialog(QDialog):
    """Pick ANY node in the scene, then any channel/property on it.

    Stored path form (resolved by the engine relative to the player root):
      ""           the player's own node
      "Child/Sub"  a descendant of the player root (reusable across instances)
      "/Abs/Path"  any other node, addressed from the scene root
    """

    def __init__(self, scene_root, player_root, get_channels, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Add Animation Track")
        self.resize(380, 460)
        self.scene_root = scene_root
        self.player_root = player_root
        self.get_channels = get_channels
        self._item_nodes = {}
        self._item_paths = {}
        self._player_item = None

        layout = QVBoxLayout(self)
        layout.addWidget(QLabel("Node to animate (any node in the scene):"))
        self.tree = QTreeWidget()
        self.tree.setHeaderHidden(True)
        self.tree.itemSelectionChanged.connect(self._on_node_changed)
        layout.addWidget(self.tree)

        layout.addWidget(QLabel("Channel / property:"))
        self.channel_combo = QComboBox()
        self.channel_combo.setEditable(True)
        layout.addWidget(self.channel_combo)

        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok |
                                   QDialogButtonBox.StandardButton.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

        self._build_tree()

    def _uuid(self, node):
        try:
            return node.get_uuid().to_string()
        except Exception:
            return None

    def _same(self, a, b):
        if a is None or b is None:
            return False
        ua = self._uuid(a)
        return ua is not None and ua == self._uuid(b)

    def _name(self, node):
        try:
            return node.get_name()
        except Exception:
            return "?"

    def _children(self, node):
        try:
            return node.get_children()
        except Exception:
            return []

    def _build_tree(self):
        root = self.scene_root or self.player_root
        if not root:
            return
        self._walk(None, root, "", None)
        self.tree.expandAll()
        if self._player_item is not None:
            self.tree.setCurrentItem(self._player_item)
        elif self.tree.topLevelItemCount():
            self.tree.setCurrentItem(self.tree.topLevelItem(0))

    def _walk(self, parent_item, node, abs_path, rel_path):
        is_player = self._same(node, self.player_root)
        if is_player:
            rel_path = ""
        label = self._name(node) + ("  (player root)" if is_player else "")
        item = QTreeWidgetItem([label])
        if parent_item is None:
            self.tree.addTopLevelItem(item)
        else:
            parent_item.addChild(item)

        if rel_path is not None:
            stored = rel_path
        else:
            stored = ("/" + abs_path) if abs_path else "/"
        self._item_nodes[id(item)] = node
        self._item_paths[id(item)] = stored
        if is_player:
            self._player_item = item

        for child in self._children(node):
            cname = self._name(child)
            cabs = cname if not abs_path else "%s/%s" % (abs_path, cname)
            if rel_path is None:
                crel = None
            elif rel_path == "":
                crel = cname
            else:
                crel = "%s/%s" % (rel_path, cname)
            self._walk(item, child, cabs, crel)

    def _selected(self):
        items = self.tree.selectedItems()
        if not items:
            return None, ""
        item = items[0]
        return self._item_nodes.get(id(item)), self._item_paths.get(id(item), "")

    def _on_node_changed(self):
        node, _ = self._selected()
        self.channel_combo.clear()
        if node and self.get_channels:
            for name, vtype in self.get_channels(node):
                self.channel_combo.addItem(name, vtype)

    def result_track(self):
        node, path = self._selected()
        channel = self.channel_combo.currentText().strip()
        vtype = self.channel_combo.currentData() or M.infer_value_type(channel)
        node_uuid = (self._uuid(node) or "") if node else ""
        return path, channel, vtype, node_uuid


class AnimationTimelinePanel(EditorPanel):
    """Keyframe timeline editor for AnimationPlayer clips."""

    def __init__(self, parent=None):
        super().__init__("Animation", parent)
        self.setObjectName("AnimationTimelinePanel")
        self.editor_bridge = None
        self.main_editor = None

        self.current_node = None
        self.player_component = None
        self.clip = None
        self.clip_path = None
        self._dirty = False

        self._preview_snapshot = None
        self._clipboard = None
        self._play_timer = QTimer(self)
        self._play_timer.timeout.connect(self._on_play_tick)
        self._playing = False
        # Debounced autosave so the .animclip file always reflects the authored
        # keyframes (runtime loads the FILE; the editor preview uses the in-memory
        # model, so without this an unsaved clip plays empty at runtime).
        self._save_timer = QTimer(self)
        self._save_timer.setSingleShot(True)
        self._save_timer.timeout.connect(self.flush_pending)

    # ----- UI -----

    def _setup_panel(self):
        toolbar = QHBoxLayout()
        toolbar.setContentsMargins(4, 2, 4, 2)

        self.clip_combo = QComboBox()
        self.clip_combo.setMinimumWidth(150)
        self.clip_combo.currentIndexChanged.connect(self._on_clip_selected)
        toolbar.addWidget(QLabel("Clip:"))
        toolbar.addWidget(self.clip_combo)

        new_btn = QPushButton("New")
        new_btn.clicked.connect(self._on_new_clip)
        toolbar.addWidget(new_btn)
        save_btn = QPushButton("Save")
        save_btn.clicked.connect(self._on_save_clip)
        toolbar.addWidget(save_btn)

        toolbar.addSpacing(10)
        self.play_btn = QPushButton("Play")
        self.play_btn.clicked.connect(self._on_play_pause)
        toolbar.addWidget(self.play_btn)
        stop_btn = QPushButton("Stop")
        stop_btn.clicked.connect(self._on_stop)
        toolbar.addWidget(stop_btn)
        self.loop_check = QCheckBox("Loop")
        self.loop_check.toggled.connect(self._on_loop_check_toggled)
        toolbar.addWidget(self.loop_check)

        toolbar.addSpacing(10)
        toolbar.addWidget(QLabel("Len:"))
        self.length_spin = QDoubleSpinBox()
        self.length_spin.setRange(0.05, 3600.0)
        self.length_spin.setValue(1.0)
        self.length_spin.setSingleStep(0.1)
        self.length_spin.valueChanged.connect(self._on_length_changed)
        toolbar.addWidget(self.length_spin)
        toolbar.addWidget(QLabel("FPS:"))
        self.fps_spin = QSpinBox()
        self.fps_spin.setRange(1, 240)
        self.fps_spin.setValue(30)
        self.fps_spin.valueChanged.connect(self._on_fps_changed)
        toolbar.addWidget(self.fps_spin)
        self.loop_mode_combo = QComboBox()
        self.loop_mode_combo.addItems(M.LOOP_MODES)
        self.loop_mode_combo.currentTextChanged.connect(self._on_loop_mode_changed)
        toolbar.addWidget(QLabel("Wrap:"))
        toolbar.addWidget(self.loop_mode_combo)
        self.snap_check = QCheckBox("Snap")
        self.snap_check.setChecked(True)
        self.snap_check.toggled.connect(self._on_snap_toggled)
        toolbar.addWidget(self.snap_check)
        toolbar.addStretch()
        self.content_layout.addLayout(toolbar)

        # Track buttons.
        track_bar = QHBoxLayout()
        track_bar.setContentsMargins(4, 0, 4, 0)
        add_track_btn = QPushButton("+ Track")
        add_track_btn.clicked.connect(self._on_add_track)
        track_bar.addWidget(add_track_btn)
        del_track_btn = QPushButton("- Track")
        del_track_btn.clicked.connect(self._on_remove_track)
        track_bar.addWidget(del_track_btn)
        key_btn = QPushButton("Key Selected Track")
        key_btn.clicked.connect(self._on_key_selected_track)
        track_bar.addWidget(key_btn)
        key_all_btn = QPushButton("Key All")
        key_all_btn.clicked.connect(self._on_key_all)
        track_bar.addWidget(key_all_btn)
        del_key_btn = QPushButton("Delete Key")
        del_key_btn.clicked.connect(self._on_delete_key)
        track_bar.addWidget(del_key_btn)
        track_bar.addStretch()
        self.content_layout.addLayout(track_bar)

        splitter = QSplitter(Qt.Orientation.Vertical)
        self.canvas = TimelineCanvas()
        self.canvas.playhead_changed.connect(self._on_playhead_changed)
        self.canvas.key_selected.connect(self._on_key_selected)
        self.canvas.key_moved.connect(self._on_key_moved)
        splitter.addWidget(self.canvas)

        self.key_editor = QGroupBox("Keyframe")
        self.key_editor_layout = QFormLayout(self.key_editor)
        self.key_time_spin = QDoubleSpinBox()
        self.key_time_spin.setRange(0.0, 3600.0)
        self.key_time_spin.setSingleStep(0.01)
        self.key_time_spin.valueChanged.connect(self._on_key_time_edited)
        self.key_editor_layout.addRow("Time:", self.key_time_spin)
        self.interp_combo = QComboBox()
        self.interp_combo.addItems(M.INTERP_MODES)
        self.interp_combo.currentTextChanged.connect(self._on_interp_changed)
        self.key_editor_layout.addRow("Track Interp:", self.interp_combo)
        self._value_widgets = []
        splitter.addWidget(self.key_editor)
        splitter.setSizes([320, 120])
        self.content_layout.addWidget(splitter)

    # ----- Node / clip wiring -----

    def set_node(self, node):
        # Keep the current animation player / clip / timeline UNLESS a DIFFERENT node
        # that itself has an AnimationPlayer is selected. Selecting an unrelated node
        # (or a child to move/key it) must not clear the editor or restore the preview.
        player = self._find_player(node)
        if player is None:
            return
        if self._same_node(node, self.current_node):
            self.current_node = node          # refresh the (possibly new) handle
            self.player_component = player
            return
        # A different node with an AnimationPlayer -> switch to it.
        self._end_preview()
        self.current_node = node
        self.player_component = player
        self.clip = None
        self.clip_path = None
        self.canvas.sel_track = -1
        self.canvas.sel_key = -1
        self.canvas.set_data([], 1.0, 30)
        self._populate_clip_combo()

    def _same_node(self, a, b):
        if a is None or b is None:
            return a is b
        try:
            return a.get_uuid().to_string() == b.get_uuid().to_string()
        except Exception:
            return a is b

    def set_animation_player(self, component):
        self.player_component = component
        self._populate_clip_combo()

    def _find_player(self, node):
        if not node:
            return None
        try:
            for comp in node.get_components():
                if comp.get_type_name() == "AnimationPlayer":
                    return comp
        except Exception:
            return None
        return None

    def _populate_clip_combo(self):
        self.clip_combo.blockSignals(True)
        self.clip_combo.clear()
        if self.player_component and self.editor_bridge:
            try:
                lib_json = self.editor_bridge.call_component_method(
                    self.player_component, "get_clip_library", "[]")
                for entry in json.loads(lib_json) or []:
                    label = entry.get("name", "")
                    path = entry.get("path", "")
                    self.clip_combo.addItem(label if path else "%s (inline)" % label, path)
            except Exception as exc:
                print("AnimationTimeline: get_clip_library failed:", exc)
        self.clip_combo.blockSignals(False)
        # The items above are added with signals blocked, so Qt's implicit
        # selection of index 0 never fires _on_clip_selected and the first clip's
        # keyframes stay unloaded until the user reselects. Load the current
        # selection explicitly here.
        if self.clip_combo.count() > 0:
            self._on_clip_selected(self.clip_combo.currentIndex())

    def _on_clip_selected(self, index):
        if index < 0:
            return
        path = self.clip_combo.itemData(index)
        if not path:
            # Inline clips have no file backing; the timeline edits file-based clips.
            return
        self.load_clip_file(self._resolve_path(path))

    def _resolve_path(self, path):
        if path.startswith("res://"):
            root = None
            if self.main_editor and getattr(self.main_editor, "project", None):
                root = self.main_editor.project.get_directory()
            if root:
                return os.path.join(root, path[len("res://"):])
        return path

    def load_clip_file(self, path):
        self._end_preview()
        try:
            self.clip = M.load_json(path)
        except Exception as exc:
            QMessageBox.warning(self, "Animation", "Failed to load clip:\n%s" % exc)
            return
        self.clip_path = path
        self._dirty = False
        self._resync_track_paths()
        self._sync_clip_to_ui()
        self._begin_preview()

    def _scene_root(self):
        if not self.editor_bridge:
            return None
        try:
            return self.editor_bridge.get_root_node()
        except Exception:
            return None

    @staticmethod
    def _node_uuid_str(node):
        try:
            return node.get_uuid().to_string()
        except Exception:
            return None

    def _stored_path_for_uuid(self, scene_root, player_root, target_uuid):
        """Recompute a track's stored path string for the node with target_uuid,
        following the same rules as AddTrackDialog (""=player root, relative for
        descendants, "/abs" otherwise). Returns None when the node is not found."""
        if not scene_root or not target_uuid:
            return None
        player_uuid = self._node_uuid_str(player_root) if player_root else None

        def walk(node, abs_path, rel_path):
            is_player = player_uuid is not None and self._node_uuid_str(node) == player_uuid
            cur_rel = "" if is_player else rel_path
            if self._node_uuid_str(node) == target_uuid:
                if cur_rel is not None:
                    return cur_rel
                return ("/" + abs_path) if abs_path else "/"
            try:
                children = node.get_children()
            except Exception:
                children = []
            for child in children:
                try:
                    cname = child.get_name()
                except Exception:
                    cname = "?"
                cabs = cname if not abs_path else "%s/%s" % (abs_path, cname)
                if cur_rel is None:
                    crel = None
                elif cur_rel == "":
                    crel = cname
                else:
                    crel = "%s/%s" % (cur_rel, cname)
                found = walk(child, cabs, crel)
                if found is not None:
                    return found
            return None

        return walk(scene_root, "", None)

    @staticmethod
    def _children(node):
        try:
            return node.get_children()
        except Exception:
            return []

    def _child_by_names(self, node, names):
        cur = node
        for name in names:
            if not name:
                continue
            nxt = None
            for child in self._children(cur):
                try:
                    if child.get_name() == name:
                        nxt = child
                        break
                except Exception:
                    pass
            if nxt is None:
                return None
            cur = nxt
        return cur

    def _find_by_name_anywhere(self, node, name):
        try:
            if node.get_name() == name:
                return node
        except Exception:
            pass
        for child in self._children(node):
            found = self._find_by_name_anywhere(child, name)
            if found is not None:
                return found
        return None

    def _node_for_path(self, scene_root, player_root, path):
        """Resolve a stored track path to a live node, mirroring the engine's
        ResolveTargetNode rules, so a legacy (UUID-less) track can be anchored to a
        stable UUID while its path still resolves."""
        if path in ("", "."):
            return player_root
        if path.startswith("/"):
            rel = path[1:]
            if not rel:
                return scene_root
            return self._child_by_names(scene_root, rel.split("/"))
        if path.startswith("%"):
            return self._find_by_name_anywhere(scene_root, path[1:])
        return self._child_by_names(player_root or scene_root, path.split("/"))

    def _resync_track_paths(self):
        """Keep each track anchored to its target node across scene-tree edits.

        Backfills a stable `nodeUuid` onto legacy (path-only) tracks while their
        path still resolves, then refreshes each track's stored `node` path from
        that UUID so a node that is moved/renamed/reparented keeps animating and the
        saved clip stores the node's current location."""
        if not self.clip:
            return
        scene_root = self._scene_root()
        if not scene_root:
            return
        player_root = self.current_node
        changed = False
        for track in self.clip.get("tracks", []):
            node_uuid = track.get("nodeUuid")
            if not node_uuid:
                node = self._node_for_path(scene_root, player_root, track.get("node", ""))
                node_uuid = self._node_uuid_str(node) if node else None
                if not node_uuid:
                    continue
                track["nodeUuid"] = node_uuid
                changed = True
            new_path = self._stored_path_for_uuid(scene_root, player_root, node_uuid)
            if new_path is not None and new_path != track.get("node"):
                track["node"] = new_path
                changed = True
        if changed:
            self._mark_dirty()

    def _sync_clip_to_ui(self):
        if not self.clip:
            return
        self.length_spin.blockSignals(True)
        self.length_spin.setValue(float(self.clip.get("length", 1.0)) or 1.0)
        self.length_spin.blockSignals(False)
        self.fps_spin.blockSignals(True)
        self.fps_spin.setValue(int(self.clip.get("fps", 30)))
        self.fps_spin.blockSignals(False)
        mode = self.clip.get("loop", "none")
        self.loop_mode_combo.blockSignals(True)
        self.loop_mode_combo.setCurrentText(mode)
        self.loop_mode_combo.blockSignals(False)
        self.loop_check.blockSignals(True)
        self.loop_check.setChecked(mode != "none")
        self.loop_check.blockSignals(False)
        self._refresh_canvas()

    def _refresh_canvas(self):
        if self.clip:
            self.canvas.set_data(self.clip["tracks"], M.clip_length(self.clip),
                                 self.clip.get("fps", 30))

    # ----- Clip ops -----

    def _on_new_clip(self):
        name, ok = QInputDialog.getText(self, "New Animation", "Clip name:")
        if not ok or not name.strip():
            return
        default_dir = self._project_root() or ""
        path, _ = QFileDialog.getSaveFileName(self, "New .animclip",
                                              os.path.join(default_dir, name.strip() + ".animclip"),
                                              "Animation Clip (*.animclip)")
        if not path:
            return
        clip = M.new_clip(name.strip())
        M.save_json(path, clip)
        if self.player_component and self.editor_bridge:
            res_path = self._to_res(path)
            try:
                self.editor_bridge.call_component_method(
                    self.player_component, "add_clip", json.dumps([name.strip(), res_path]))
                self.editor_bridge.call_component_method(self.player_component, "reload_clips", "[]")
            except Exception as exc:
                print("AnimationTimeline: add_clip failed:", exc)
        self._populate_clip_combo()
        self.load_clip_file(path)

    def _project_root(self):
        if self.main_editor and getattr(self.main_editor, "project", None):
            return self.main_editor.project.path
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

    def _on_save_clip(self):
        if not self.clip or not self.clip_path:
            return
        self.clip["length"] = float(self.length_spin.value())
        self._resync_track_paths()
        try:
            M.save_json(self.clip_path, self.clip)
            self._dirty = False
            if self.player_component and self.editor_bridge:
                self.editor_bridge.call_component_method(self.player_component, "reload_clips", "[]")
        except Exception as exc:
            QMessageBox.warning(self, "Animation", "Failed to save:\n%s" % exc)

    def handle_save(self):
        if self.clip and self.clip_path:
            self._on_save_clip()
            return True
        return False

    def _on_length_changed(self, value):
        if self.clip:
            self.clip["length"] = float(value)
            self._mark_dirty()
            self._refresh_canvas()

    def _on_fps_changed(self, value):
        if self.clip:
            self.clip["fps"] = int(value)
            self._mark_dirty()
            self._refresh_canvas()

    def _on_loop_mode_changed(self, text):
        if self.clip:
            self.clip["loop"] = text
            self.loop_check.blockSignals(True)
            self.loop_check.setChecked(text != "none")
            self.loop_check.blockSignals(False)
            self._mark_dirty()

    def _on_loop_check_toggled(self, checked):
        # The Loop checkbox is a convenience proxy for the clip's persisted loop mode
        # (so it actually loops at runtime, not just in the editor preview).
        if not self.clip:
            return
        mode = self.clip.get("loop", "none")
        if checked:
            new_mode = mode if mode in ("loop", "pingpong") else "loop"
        else:
            new_mode = "none"
        self.clip["loop"] = new_mode
        self.loop_mode_combo.blockSignals(True)
        self.loop_mode_combo.setCurrentText(new_mode)
        self.loop_mode_combo.blockSignals(False)
        self._mark_dirty()

    def _on_snap_toggled(self, checked):
        self.canvas.snap = checked

    # ----- Tracks -----

    def _channels_for_node(self, node):
        """Every animatable channel on a node: transforms + ALL of its components'
        properties + the node's own properties (any property type is exposed)."""
        channels = []
        seen = set()

        def add(name, vtype):
            if name and name not in seen:
                seen.add(name)
                channels.append((name, vtype))

        for ch in M.TRANSFORM_CHANNELS_2D + M.TRANSFORM_CHANNELS_3D:
            add(ch, M.infer_value_type(ch))

        if not (node and self.editor_bridge):
            return channels

        try:
            for comp in node.get_components():
                data = json.loads(self.editor_bridge.get_component_properties(comp))
                for name, info in data.get("property_metadata", {}).items():
                    vtype = M.value_type_for_property(info.get("type"))
                    if vtype:
                        add(name, vtype)
        except Exception as exc:
            print("AnimationTimeline: component channel scan failed:", exc)

        try:
            data = json.loads(self.editor_bridge.get_node_properties(node))
            for name, info in data.get("property_metadata", {}).items():
                if name in ("position", "rotation", "scale"):
                    continue
                vtype = M.value_type_for_property(info.get("type"))
                if vtype:
                    add(name, vtype)
        except Exception as exc:
            print("AnimationTimeline: node channel scan failed:", exc)

        return channels

    def _on_add_track(self):
        if not self.clip:
            return
        scene_root = None
        if self.editor_bridge:
            try:
                scene_root = self.editor_bridge.get_root_node()
            except Exception:
                scene_root = None
        dialog = AddTrackDialog(scene_root, self.current_node, self._channels_for_node, self)
        if dialog.exec() != QDialog.DialogCode.Accepted:
            return
        node, channel, vtype, node_uuid = dialog.result_track()
        if not channel:
            return
        if M.find_track(self.clip, node, channel):
            return
        self.clip["tracks"].append(M.new_value_track(node, channel, vtype, node_uuid=node_uuid))
        self._mark_dirty()
        self._refresh_canvas()

    def _on_remove_track(self):
        if not self.clip or self.canvas.sel_track < 0:
            return
        idx = self.canvas.sel_track
        if 0 <= idx < len(self.clip["tracks"]):
            self.clip["tracks"].pop(idx)
            self.canvas.sel_track = -1
            self.canvas.sel_key = -1
            self._mark_dirty()
            self._refresh_canvas()
            self._apply_preview()

    def _on_key_selected_track(self):
        if self.canvas.sel_track >= 0:
            self._key_track(self.canvas.sel_track)

    def _on_key_all(self):
        if not self.clip:
            return
        for i in range(len(self.clip["tracks"])):
            self._key_track(i)

    def _key_track(self, track_index):
        if not self.clip or not self.current_node or not self.editor_bridge:
            return
        track = self.clip["tracks"][track_index]
        if track.get("type") == "method":
            return
        target = {"node": track["node"], "channel": track["channel"],
                  "valueType": track.get("valueType", M.infer_value_type(track["channel"]))}
        try:
            captured = json.loads(self.editor_bridge.capture_animation_pose(
                self.current_node, json.dumps([target])))
        except Exception as exc:
            print("AnimationTimeline: capture failed:", exc)
            return
        if not captured:
            return
        value = captured[0].get("value")
        if value is None:
            return
        key = M.insert_key(track, self.canvas.playhead, value)
        self.canvas.sel_track = track_index
        try:
            self.canvas.sel_key = track["keys"].index(key)
        except ValueError:
            self.canvas.sel_key = -1
        self._mark_dirty()
        self._refresh_canvas()
        self._rebuild_key_editor()
        self._apply_preview()

    def _on_delete_key(self):
        if not self.clip or self.canvas.sel_track < 0 or self.canvas.sel_key < 0:
            return
        track = self.clip["tracks"][self.canvas.sel_track]
        M.remove_key(track, self.canvas.sel_key)
        self.canvas.sel_key = -1
        self._mark_dirty()
        self._refresh_canvas()
        self._rebuild_key_editor()
        self._apply_preview()

    # ----- Clipboard (Ctrl+C / Ctrl+V routed from the main window when focused) -----

    def copy_keys(self):
        """Copy the selected keyframe, or all keys of the selected track."""
        if not self.clip:
            return
        ti = self.canvas.sel_track
        if ti < 0 or ti >= len(self.clip["tracks"]):
            return
        track = self.clip["tracks"][ti]
        keys = track.get("keys", [])
        if not keys:
            return
        ki = self.canvas.sel_key
        selected = [keys[ki]] if 0 <= ki < len(keys) else list(keys)
        base = min(k["t"] for k in selected)
        self._clipboard = {
            "base": base,
            "track_type": track.get("type", "value"),
            "keys": [copy.deepcopy(k) for k in selected],
        }

    def paste_keys(self):
        """Paste copied keyframes onto the selected track, starting at the playhead."""
        if not self.clip or not self._clipboard:
            return
        ti = self.canvas.sel_track
        if ti < 0 or ti >= len(self.clip["tracks"]):
            return
        track = self.clip["tracks"][ti]
        if track.get("type", "value") != self._clipboard.get("track_type", "value"):
            return
        offset = self.canvas.playhead - self._clipboard["base"]
        last = None
        for src in self._clipboard["keys"]:
            new_key = copy.deepcopy(src)
            new_key["t"] = max(0.0, float(new_key["t"]) + offset)
            self._insert_key_dict(track, new_key)
            last = new_key
        track["keys"].sort(key=lambda kk: kk["t"])
        if last is not None:
            try:
                self.canvas.sel_track = ti
                self.canvas.sel_key = track["keys"].index(last)
            except ValueError:
                pass
        self._mark_dirty()
        self._refresh_canvas()
        self._rebuild_key_editor()
        self._apply_preview()

    def _insert_key_dict(self, track, new_key):
        keys = track.setdefault("keys", [])
        for i, existing in enumerate(keys):
            if abs(existing["t"] - new_key["t"]) < 1e-4:
                keys[i] = new_key
                return
        keys.append(new_key)

    # ----- Keyframe editor -----

    def _on_key_selected(self, track_index, key_index):
        self._rebuild_key_editor()

    def _on_key_moved(self):
        self._mark_dirty()
        self._rebuild_key_editor()
        self._apply_preview()

    def _clear_value_widgets(self):
        for widget in self._value_widgets:
            self.key_editor_layout.removeRow(widget)
        self._value_widgets = []

    def _rebuild_key_editor(self):
        self._clear_value_widgets()
        ti, ki = self.canvas.sel_track, self.canvas.sel_key
        if not self.clip or ti < 0 or ti >= len(self.clip["tracks"]):
            return
        track = self.clip["tracks"][ti]
        self.interp_combo.blockSignals(True)
        self.interp_combo.setCurrentText(track.get("interp", "linear"))
        self.interp_combo.blockSignals(False)
        if ki < 0 or ki >= len(track.get("keys", [])):
            return
        key = track["keys"][ki]
        self.key_time_spin.blockSignals(True)
        self.key_time_spin.setValue(float(key["t"]))
        self.key_time_spin.blockSignals(False)

        if track.get("type") == "method":
            method_edit = QLineEdit(key.get("method", ""))
            method_edit.textChanged.connect(lambda txt, k=key: self._set_method(k, txt))
            self.key_editor_layout.addRow("Method:", method_edit)
            self._value_widgets.append(method_edit)
            return

        vtype = track.get("valueType", "float")
        value = key.get("v", M.default_value(vtype))
        if vtype == "bool":
            check = QCheckBox()
            check.setChecked(bool(value))
            check.toggled.connect(lambda v, k=key: self._set_scalar(k, bool(v)))
            self.key_editor_layout.addRow("Value:", check)
            self._value_widgets.append(check)
        elif vtype == "string":
            edit = QLineEdit(value if isinstance(value, str) else "")
            edit.textChanged.connect(lambda txt, k=key: self._set_scalar(k, txt))
            self.key_editor_layout.addRow("Value:", edit)
            self._value_widgets.append(edit)
        elif M.value_component_count(vtype) == 1:
            spin = QDoubleSpinBox()
            spin.setRange(-1e9, 1e9)
            spin.setDecimals(4)
            spin.setValue(float(value if not isinstance(value, list) else (value[0] if value else 0)))
            spin.valueChanged.connect(lambda v, k=key, vt=vtype:
                                      self._set_scalar(k, int(v) if vt == "int" else float(v)))
            self.key_editor_layout.addRow("Value:", spin)
            self._value_widgets.append(spin)
        else:
            comps = M.value_component_count(vtype)
            labels = (["R", "G", "B", "A"] if vtype == "color" else ["X", "Y", "Z", "W"])[:comps]
            arr = value if isinstance(value, list) else [0.0] * comps
            while len(arr) < comps:
                arr.append(0.0)
            row = QWidget()
            row_layout = QHBoxLayout(row)
            row_layout.setContentsMargins(0, 0, 0, 0)
            for ci in range(comps):
                row_layout.addWidget(QLabel(labels[ci]))
                spin = QDoubleSpinBox()
                spin.setRange(-1e9, 1e9)
                spin.setDecimals(4)
                spin.setValue(float(arr[ci]))
                spin.valueChanged.connect(lambda v, k=key, idx=ci: self._set_component(k, idx, float(v)))
                row_layout.addWidget(spin)
            self.key_editor_layout.addRow("Value:", row)
            self._value_widgets.append(row)

    def _set_scalar(self, key, value):
        key["v"] = value
        self._mark_dirty()
        self._apply_preview()

    def _set_component(self, key, index, value):
        if not isinstance(key.get("v"), list):
            key["v"] = [0.0, 0.0, 0.0, 0.0]
        while len(key["v"]) <= index:
            key["v"].append(0.0)
        key["v"][index] = value
        self._mark_dirty()
        self._apply_preview()

    def _set_method(self, key, text):
        key["method"] = text
        self._mark_dirty()

    def _on_key_time_edited(self, value):
        ti, ki = self.canvas.sel_track, self.canvas.sel_key
        if not self.clip or ti < 0 or ki < 0:
            return
        track = self.clip["tracks"][ti]
        track["keys"][ki]["t"] = float(value)
        track["keys"].sort(key=lambda kk: kk["t"])
        self._mark_dirty()
        self._refresh_canvas()
        self._apply_preview()

    def _on_interp_changed(self, text):
        ti = self.canvas.sel_track
        if self.clip and 0 <= ti < len(self.clip["tracks"]):
            self.clip["tracks"][ti]["interp"] = text
            self._mark_dirty()
            self._apply_preview()

    # ----- Preview / playback -----

    def _begin_preview(self):
        if self._preview_snapshot is not None or not self.clip or not self.current_node or not self.editor_bridge:
            return
        try:
            self._resync_track_paths()
            targets = M.clip_targets(self.clip)
            self._preview_snapshot = self.editor_bridge.capture_animation_pose(
                self.current_node, json.dumps(targets))
            self._apply_preview()
        except Exception as exc:
            print("AnimationTimeline: begin preview failed:", exc)
            self._preview_snapshot = None

    def _apply_preview(self):
        if not self.clip or not self.current_node or not self.editor_bridge:
            return
        if self._preview_snapshot is None:
            self._begin_preview()
            return
        try:
            self.editor_bridge.preview_animation_clip(
                self.current_node, json.dumps(self.clip), float(self.canvas.playhead))
            self._refresh_viewport()
        except Exception as exc:
            print("AnimationTimeline: preview failed:", exc)

    def _end_preview(self):
        self._stop_playback()
        if self._preview_snapshot is not None and self.current_node and self.editor_bridge:
            try:
                self.editor_bridge.restore_animation_pose(self.current_node, self._preview_snapshot)
                self._refresh_viewport()
            except Exception as exc:
                print("AnimationTimeline: restore failed:", exc)
        self._preview_snapshot = None

    def _refresh_viewport(self):
        if self.main_editor and hasattr(self.main_editor, "viewport_tabs"):
            try:
                viewport = self.main_editor.viewport_tabs.get_current_viewport()
                if viewport:
                    # Drive an immediate engine render rather than QWidget.update().
                    # The viewport surface is owned by the engine; a Qt repaint would
                    # only risk a background fill (flicker). request_render() pushes a
                    # fresh engine frame reflecting the scrubbed pose right away.
                    if hasattr(viewport, "request_render"):
                        viewport.request_render()
                    else:
                        viewport.update()
            except Exception:
                pass

    def _on_playhead_changed(self, value):
        self._apply_preview()

    def _on_play_pause(self):
        if self._playing:
            self._stop_playback()
        elif self.clip:
            self._begin_preview()
            self._playing = True
            self.play_btn.setText("Pause")
            self._play_timer.start(int(1000 / max(self.fps_spin.value(), 1)))

    def _stop_playback(self):
        if self._playing:
            self._play_timer.stop()
            self._playing = False
            self.play_btn.setText("Play")

    def _on_stop(self):
        self._stop_playback()
        self.canvas.playhead = 0.0
        self.canvas.update()
        self._apply_preview()

    def _on_play_tick(self):
        if not self.clip:
            self._stop_playback()
            return
        length = M.clip_length(self.clip)
        step = 1.0 / max(self.fps_spin.value(), 1)
        t = self.canvas.playhead + step
        if t > length:
            if self.clip.get("loop", "none") != "none":
                t = 0.0
            else:
                t = length
                self._stop_playback()
        self.canvas.playhead = t
        self.canvas.update()
        self._apply_preview()

    def _mark_dirty(self):
        self._dirty = True
        if self.clip_path:
            self._save_timer.start(800)

    def flush_pending(self):
        """Write pending clip edits to the .animclip file (autosave + pre-play).

        This is the bridge between the in-memory authoring model and the file the
        runtime loads. Called on a debounce, on panel hide, and before the game runs.
        """
        if not self.clip or not self.clip_path or not self._dirty:
            return
        self.clip["length"] = float(self.length_spin.value())
        try:
            M.save_json(self.clip_path, self.clip)
            self._dirty = False
            if self.player_component and self.editor_bridge:
                self.editor_bridge.call_component_method(self.player_component, "reload_clips", "[]")
        except Exception as exc:
            print("AnimationTimeline: autosave failed:", exc)

    # ----- Lifecycle -----

    def hideEvent(self, event):
        self.flush_pending()
        self._end_preview()
        super().hideEvent(event)

    def closeEvent(self, event):
        self.flush_pending()
        self._end_preview()
        super().closeEvent(event)
