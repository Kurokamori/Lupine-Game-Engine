"""
Animation Tree (Blend Tree) Editor Panel

Authors .animgraph controllers: a state-machine graph (states + transitions on a
custom canvas), per-state blend trees (1D/2D/direct with a 2D blend-space
visualizer), transition conditions, parameters, and layers. Live parameter values
are pushed to a selected AnimationTree component (useful while the game runs).
"""

import json
import os

from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel, QPushButton,
                             QComboBox, QDoubleSpinBox, QSpinBox, QCheckBox, QSplitter,
                             QGroupBox, QInputDialog, QMessageBox, QFileDialog, QScrollArea,
                             QFormLayout, QLineEdit, QSizePolicy)
from PyQt6.QtCore import Qt, pyqtSignal, QRectF, QPointF
from PyQt6.QtGui import QPainter, QColor, QPen, QBrush, QPolygonF, QFont

from .base_panel import EditorPanel

from editor.animation import anim_model as M


STATE_W = 130
STATE_H = 44


class StateMachineCanvas(QWidget):
    """Custom canvas: state boxes + transition arrows, with drag and connect."""

    state_selected = pyqtSignal(int)
    transition_selected = pyqtSignal(int)
    selection_cleared = pyqtSignal()
    graph_changed = pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.sm = None
        self.sel_state = -1
        self.sel_transition = -1
        self.connect_mode = False
        self._connect_source = -1
        self._drag_state = -1
        self._drag_offset = QPointF(0, 0)
        self.setMinimumSize(300, 240)
        self.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)

    def set_state_machine(self, sm):
        self.sm = sm
        self.sel_state = -1
        self.sel_transition = -1
        self.update()

    def _state_rect(self, state):
        pos = state.get("pos", [0, 0])
        return QRectF(pos[0], pos[1], STATE_W, STATE_H)

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        painter.fillRect(self.rect(), QColor("#1b1726"))
        if not self.sm:
            return

        states = self.sm.get("states", [])
        entry = self.sm.get("entry", "")

        # Transitions.
        for ti, tr in enumerate(self.sm.get("transitions", [])):
            src = self._find_state(tr.get("from", ""))
            dst = self._find_state(tr.get("to", ""))
            if tr.get("from", "") in ("", "Any"):
                if dst:
                    self._draw_transition(painter, QPointF(20, 20), self._center(dst),
                                          ti == self.sel_transition, dashed=True)
                continue
            if src and dst:
                self._draw_transition(painter, self._center(src), self._center(dst),
                                      ti == self.sel_transition)

        # States.
        for si, state in enumerate(states):
            rect = self._state_rect(state)
            is_entry = state.get("name", "") == entry
            selected = si == self.sel_state
            fill = QColor("#3a3052") if not selected else QColor("#5a4a8a")
            if is_entry:
                fill = QColor("#2f6f4a") if not selected else QColor("#3f8f5a")
            painter.setPen(QPen(QColor("#b9a9e0") if selected else QColor("#5a4f70"), 2))
            painter.setBrush(QBrush(fill))
            painter.drawRoundedRect(rect, 8, 8)
            painter.setPen(QColor("#f0ecf8"))
            font = QFont()
            font.setBold(True)
            painter.setFont(font)
            painter.drawText(rect.adjusted(6, 4, -6, -STATE_H / 2),
                             Qt.AlignmentFlag.AlignLeft, state.get("name", ""))
            painter.setFont(QFont())
            sub = "blend tree" if state.get("type") == "blendtree" else state.get("clip", "(no clip)")
            painter.setPen(QColor("#c9bfe0"))
            painter.drawText(rect.adjusted(6, STATE_H / 2 - 2, -6, -4),
                             Qt.AlignmentFlag.AlignLeft, sub[:18])

        if self.connect_mode and self._connect_source >= 0:
            src = states[self._connect_source]
            painter.setPen(QColor("#ffd24c"))
            painter.drawText(8, self.height() - 8, "Click a target state to connect...")

    def _center(self, state):
        rect = self._state_rect(state)
        return QPointF(rect.center())

    def _find_state(self, name):
        for state in self.sm.get("states", []):
            if state.get("name") == name:
                return state
        return None

    def _draw_transition(self, painter, p0, p1, selected, dashed=False):
        pen = QPen(QColor("#ffd24c") if selected else QColor("#8a7fb0"), 2)
        if dashed:
            pen.setStyle(Qt.PenStyle.DashLine)
        painter.setPen(pen)
        painter.drawLine(p0, p1)
        # Arrowhead.
        import math
        ang = math.atan2(p1.y() - p0.y(), p1.x() - p0.x())
        mid = QPointF((p0.x() + p1.x()) / 2, (p0.y() + p1.y()) / 2)
        size = 9
        painter.setBrush(QBrush(pen.color()))
        painter.drawPolygon(QPolygonF([
            mid,
            QPointF(mid.x() - size * math.cos(ang - 0.4), mid.y() - size * math.sin(ang - 0.4)),
            QPointF(mid.x() - size * math.cos(ang + 0.4), mid.y() - size * math.sin(ang + 0.4))]))

    def _hit_state(self, pos):
        if not self.sm:
            return -1
        for si in reversed(range(len(self.sm.get("states", [])))):
            if self._state_rect(self.sm["states"][si]).contains(pos):
                return si
        return -1

    def _hit_transition(self, pos):
        if not self.sm:
            return -1
        import math
        for ti, tr in enumerate(self.sm.get("transitions", [])):
            src = self._find_state(tr.get("from", ""))
            dst = self._find_state(tr.get("to", ""))
            if not dst:
                continue
            p0 = QPointF(20, 20) if tr.get("from", "") in ("", "Any") else (self._center(src) if src else None)
            if p0 is None:
                continue
            p1 = self._center(dst)
            mid = QPointF((p0.x() + p1.x()) / 2, (p0.y() + p1.y()) / 2)
            if math.hypot(pos.x() - mid.x(), pos.y() - mid.y()) < 12:
                return ti
        return -1

    def mousePressEvent(self, event):
        pos = event.position()
        si = self._hit_state(pos)
        if self.connect_mode:
            if si >= 0:
                if self._connect_source < 0:
                    self._connect_source = si
                elif si != self._connect_source and self.sm:
                    frm = self.sm["states"][self._connect_source]["name"]
                    to = self.sm["states"][si]["name"]
                    self.sm["transitions"].append(M.new_transition(frm, to))
                    self.connect_mode = False
                    self._connect_source = -1
                    self.graph_changed.emit()
            self.update()
            return

        if si >= 0:
            self.sel_state = si
            self.sel_transition = -1
            self._drag_state = si
            rect = self._state_rect(self.sm["states"][si])
            self._drag_offset = QPointF(pos.x() - rect.x(), pos.y() - rect.y())
            self.state_selected.emit(si)
            self.update()
            return

        ti = self._hit_transition(pos)
        if ti >= 0:
            self.sel_transition = ti
            self.sel_state = -1
            self.transition_selected.emit(ti)
            self.update()
            return

        self.sel_state = -1
        self.sel_transition = -1
        self.selection_cleared.emit()
        self.update()

    def mouseMoveEvent(self, event):
        if self._drag_state >= 0 and self.sm:
            pos = event.position()
            state = self.sm["states"][self._drag_state]
            state["pos"] = [max(0.0, pos.x() - self._drag_offset.x()),
                            max(0.0, pos.y() - self._drag_offset.y())]
            self.update()

    def mouseReleaseEvent(self, event):
        if self._drag_state >= 0:
            self._drag_state = -1
            self.graph_changed.emit()


class BlendSpace2DWidget(QWidget):
    """Draggable 2D positions for a blend2d node's children."""

    changed = pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.children = []
        self.range = 8.0
        self._drag = -1
        self.setMinimumHeight(160)
        self.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

    def set_children(self, children):
        self.children = children
        self.update()

    def _to_screen(self, x, y):
        w, h = self.width(), self.height()
        sx = (x / self.range + 1.0) * 0.5 * (w - 20) + 10
        sy = (1.0 - (y / self.range + 1.0) * 0.5) * (h - 20) + 10
        return sx, sy

    def _to_data(self, sx, sy):
        w, h = self.width(), self.height()
        x = ((sx - 10) / max(w - 20, 1) * 2.0 - 1.0) * self.range
        y = (1.0 - (sy - 10) / max(h - 20, 1)) * 2.0 - 1.0
        return x, y * self.range

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        painter.fillRect(self.rect(), QColor("#211b2e"))
        painter.setPen(QPen(QColor("#3a3150"), 1))
        cx, cy = self._to_screen(0, 0)
        painter.drawLine(int(cx), 8, int(cx), self.height() - 8)
        painter.drawLine(8, int(cy), self.width() - 8, int(cy))
        for child in self.children:
            sx, sy = self._to_screen(child.get("posX", 0.0), child.get("posY", 0.0))
            painter.setBrush(QBrush(QColor("#4cc2ff")))
            painter.setPen(QPen(QColor("#1b2a3a"), 1))
            painter.drawEllipse(int(sx) - 6, int(sy) - 6, 12, 12)
            painter.setPen(QColor("#cfe6ff"))
            painter.drawText(int(sx) + 8, int(sy) + 4, str(child.get("node", {}).get("clip", "")) [:10])

    def mousePressEvent(self, event):
        pos = event.position()
        for i, child in enumerate(self.children):
            sx, sy = self._to_screen(child.get("posX", 0.0), child.get("posY", 0.0))
            if (pos.x() - sx) ** 2 + (pos.y() - sy) ** 2 < 64:
                self._drag = i
                return

    def mouseMoveEvent(self, event):
        if self._drag >= 0:
            pos = event.position()
            x, y = self._to_data(pos.x(), pos.y())
            self.children[self._drag]["posX"] = round(x, 3)
            self.children[self._drag]["posY"] = round(y, 3)
            self.update()
            self.changed.emit()

    def mouseReleaseEvent(self, event):
        self._drag = -1


class BlendTreePanel(EditorPanel):
    """Animator-style .animgraph editor."""

    def __init__(self, parent=None):
        super().__init__("Animation Tree", parent)
        self.setObjectName("BlendTreePanel")
        self.editor_bridge = None
        self.main_editor = None

        self.current_node = None
        self.tree_component = None
        self.graph = None
        self.graph_path = None
        self.layer_index = 0

    # ----- UI -----

    def _setup_panel(self):
        toolbar = QHBoxLayout()
        toolbar.setContentsMargins(4, 2, 4, 2)
        self.path_label = QLabel("(no graph)")
        toolbar.addWidget(self.path_label)
        new_btn = QPushButton("New")
        new_btn.clicked.connect(self._on_new_graph)
        toolbar.addWidget(new_btn)
        save_btn = QPushButton("Save")
        save_btn.clicked.connect(self._on_save_graph)
        toolbar.addWidget(save_btn)
        toolbar.addSpacing(10)
        toolbar.addWidget(QLabel("Layer:"))
        self.layer_combo = QComboBox()
        self.layer_combo.currentIndexChanged.connect(self._on_layer_changed)
        toolbar.addWidget(self.layer_combo)
        add_layer_btn = QPushButton("+ Layer")
        add_layer_btn.clicked.connect(self._on_add_layer)
        toolbar.addWidget(add_layer_btn)
        toolbar.addStretch()
        self.content_layout.addLayout(toolbar)

        graph_bar = QHBoxLayout()
        graph_bar.setContentsMargins(4, 0, 4, 0)
        add_state_btn = QPushButton("+ Clip State")
        add_state_btn.clicked.connect(lambda: self._on_add_state(False))
        graph_bar.addWidget(add_state_btn)
        add_bt_btn = QPushButton("+ Blend Tree State")
        add_bt_btn.clicked.connect(lambda: self._on_add_state(True))
        graph_bar.addWidget(add_bt_btn)
        self.connect_btn = QPushButton("Connect")
        self.connect_btn.setCheckable(True)
        self.connect_btn.toggled.connect(self._on_connect_toggled)
        graph_bar.addWidget(self.connect_btn)
        del_btn = QPushButton("Delete")
        del_btn.clicked.connect(self._on_delete)
        graph_bar.addWidget(del_btn)
        entry_btn = QPushButton("Set Entry")
        entry_btn.clicked.connect(self._on_set_entry)
        graph_bar.addWidget(entry_btn)
        graph_bar.addStretch()
        self.content_layout.addLayout(graph_bar)

        splitter = QSplitter(Qt.Orientation.Horizontal)
        self.canvas = StateMachineCanvas()
        self.canvas.state_selected.connect(self._on_state_selected)
        self.canvas.transition_selected.connect(self._on_transition_selected)
        self.canvas.selection_cleared.connect(self._on_selection_cleared)
        self.canvas.graph_changed.connect(self._on_graph_changed)
        splitter.addWidget(self.canvas)

        right = QScrollArea()
        right.setWidgetResizable(True)
        right.setMinimumWidth(320)
        self.right_widget = QWidget()
        self.right_layout = QVBoxLayout(self.right_widget)
        right.setWidget(self.right_widget)
        splitter.addWidget(right)
        splitter.setSizes([460, 340])
        self.content_layout.addWidget(splitter)

        self._build_parameters_group()
        self._build_state_group()
        self._build_transition_group()
        self._build_layer_group()
        self.right_layout.addStretch()

    def _build_parameters_group(self):
        self.params_group = QGroupBox("Parameters")
        layout = QVBoxLayout(self.params_group)
        self.params_container = QVBoxLayout()
        layout.addLayout(self.params_container)
        add_bar = QHBoxLayout()
        add_btn = QPushButton("+ Parameter")
        add_btn.clicked.connect(self._on_add_param)
        add_bar.addWidget(add_btn)
        add_bar.addStretch()
        layout.addLayout(add_bar)
        self.right_layout.addWidget(self.params_group)

    def _build_state_group(self):
        self.state_group = QGroupBox("State")
        self.state_form = QFormLayout(self.state_group)
        self.state_name_edit = QLineEdit()
        self.state_name_edit.editingFinished.connect(self._on_state_name_edited)
        self.state_form.addRow("Name:", self.state_name_edit)
        self.state_speed_spin = QDoubleSpinBox()
        self.state_speed_spin.setRange(-8.0, 8.0)
        self.state_speed_spin.setSingleStep(0.05)
        self.state_speed_spin.setValue(1.0)
        self.state_speed_spin.valueChanged.connect(self._on_state_speed_edited)
        self.state_form.addRow("Speed:", self.state_speed_spin)
        self.state_clip_combo = QComboBox()
        self.state_clip_combo.setEditable(True)
        self.state_clip_combo.currentTextChanged.connect(self._on_state_clip_edited)
        self.state_form.addRow("Clip:", self.state_clip_combo)
        # Blend tree sub-editor.
        self.bt_type_combo = QComboBox()
        self.bt_type_combo.addItems(M.BLEND_NODE_TYPES)
        self.bt_type_combo.currentTextChanged.connect(self._on_bt_type_changed)
        self.state_form.addRow("Blend Type:", self.bt_type_combo)
        self.bt_paramx_combo = QComboBox()
        self.bt_paramx_combo.setEditable(True)
        self.bt_paramx_combo.currentTextChanged.connect(self._on_bt_paramx_changed)
        self.state_form.addRow("Param X:", self.bt_paramx_combo)
        self.bt_paramy_combo = QComboBox()
        self.bt_paramy_combo.setEditable(True)
        self.bt_paramy_combo.currentTextChanged.connect(self._on_bt_paramy_changed)
        self.state_form.addRow("Param Y:", self.bt_paramy_combo)
        self.bt_children_container = QVBoxLayout()
        self.state_form.addRow("Children:", QWidget())
        self.state_form.addRow(self._wrap(self.bt_children_container))
        add_child_btn = QPushButton("+ Child Clip")
        add_child_btn.clicked.connect(self._on_add_bt_child)
        self.state_form.addRow(add_child_btn)
        self.blend_space = BlendSpace2DWidget()
        self.blend_space.changed.connect(self._on_graph_changed)
        self.state_form.addRow(self.blend_space)
        self.right_layout.addWidget(self.state_group)
        self.state_group.setVisible(False)

    def _build_transition_group(self):
        self.trans_group = QGroupBox("Transition")
        self.trans_form = QFormLayout(self.trans_group)
        self.trans_label = QLabel("")
        self.trans_form.addRow("Edge:", self.trans_label)
        self.trans_exit_check = QCheckBox("Has Exit Time")
        self.trans_exit_check.toggled.connect(self._on_trans_edited)
        self.trans_form.addRow(self.trans_exit_check)
        self.trans_exit_spin = QDoubleSpinBox()
        self.trans_exit_spin.setRange(0.0, 1.0)
        self.trans_exit_spin.setSingleStep(0.05)
        self.trans_exit_spin.valueChanged.connect(self._on_trans_edited)
        self.trans_form.addRow("Exit Time:", self.trans_exit_spin)
        self.trans_dur_spin = QDoubleSpinBox()
        self.trans_dur_spin.setRange(0.0, 10.0)
        self.trans_dur_spin.setSingleStep(0.05)
        self.trans_dur_spin.valueChanged.connect(self._on_trans_edited)
        self.trans_form.addRow("Duration:", self.trans_dur_spin)
        self.cond_container = QVBoxLayout()
        self.trans_form.addRow("Conditions:", QWidget())
        self.trans_form.addRow(self._wrap(self.cond_container))
        add_cond_btn = QPushButton("+ Condition")
        add_cond_btn.clicked.connect(self._on_add_condition)
        self.trans_form.addRow(add_cond_btn)
        self.right_layout.addWidget(self.trans_group)
        self.trans_group.setVisible(False)

    def _build_layer_group(self):
        self.layer_group = QGroupBox("Layer")
        form = QFormLayout(self.layer_group)
        self.layer_weight_spin = QDoubleSpinBox()
        self.layer_weight_spin.setRange(0.0, 1.0)
        self.layer_weight_spin.setSingleStep(0.05)
        self.layer_weight_spin.setValue(1.0)
        self.layer_weight_spin.valueChanged.connect(self._on_layer_edited)
        form.addRow("Weight:", self.layer_weight_spin)
        self.layer_mode_combo = QComboBox()
        self.layer_mode_combo.addItems(["override", "additive"])
        self.layer_mode_combo.currentTextChanged.connect(self._on_layer_edited)
        form.addRow("Blend Mode:", self.layer_mode_combo)
        self.layer_mask_edit = QLineEdit()
        self.layer_mask_edit.setPlaceholderText("comma-separated node\\x1Fchannel or node paths")
        self.layer_mask_edit.editingFinished.connect(self._on_layer_mask_edited)
        form.addRow("Mask:", self.layer_mask_edit)
        self.layer_mask_mode = QComboBox()
        self.layer_mask_mode.addItems(["include", "exclude"])
        self.layer_mask_mode.currentTextChanged.connect(self._on_layer_edited)
        form.addRow("Mask Mode:", self.layer_mask_mode)
        self.right_layout.addWidget(self.layer_group)

    def _wrap(self, layout):
        widget = QWidget()
        widget.setLayout(layout)
        return widget

    # ----- Node wiring -----

    def set_node(self, node):
        self.current_node = node
        self.tree_component = self._find_tree(node)

    def _find_tree(self, node):
        if not node:
            return None
        try:
            for comp in node.get_components():
                if comp.get_type_name() == "AnimationTree":
                    return comp
        except Exception:
            return None
        return None

    def _player_clip_names(self):
        names = []
        if self.current_node and self.editor_bridge:
            try:
                for comp in self.current_node.get_components():
                    if comp.get_type_name() == "AnimationPlayer":
                        lib = json.loads(self.editor_bridge.call_component_method(
                            comp, "get_clip_library", "[]")) or []
                        names = [e.get("name", "") for e in lib]
            except Exception:
                pass
        return names

    # ----- Graph IO -----

    def _project_root(self):
        if self.main_editor and getattr(self.main_editor, "project", None):
            return self.main_editor.project.path
        return None

    def _on_new_graph(self):
        default_dir = self._project_root() or ""
        path, _ = QFileDialog.getSaveFileName(self, "New .animgraph",
                                              os.path.join(default_dir, "controller.animgraph"),
                                              "Animation Graph (*.animgraph)")
        if not path:
            return
        self.graph = M.new_graph()
        M.save_json(path, self.graph)
        self.graph_path = path
        self._sync_graph_to_ui()

    def load_graph_file(self, path):
        try:
            self.graph = M.load_json(path)
        except Exception as exc:
            QMessageBox.warning(self, "Animation Tree", "Failed to load graph:\n%s" % exc)
            return
        self.graph_path = path
        if not self.graph.get("layers"):
            self.graph["layers"] = [M.new_layer("Base")]
        self._sync_graph_to_ui()

    def _on_save_graph(self):
        if not self.graph or not self.graph_path:
            return
        try:
            M.save_json(self.graph_path, self.graph)
            if self.tree_component and self.editor_bridge:
                self.editor_bridge.call_component_method(self.tree_component, "reload_graph", "[]")
        except Exception as exc:
            QMessageBox.warning(self, "Animation Tree", "Failed to save:\n%s" % exc)

    def handle_save(self):
        if self.graph and self.graph_path:
            self._on_save_graph()
            return True
        return False

    def _sync_graph_to_ui(self):
        if not self.graph:
            return
        self.path_label.setText(os.path.basename(self.graph_path or "(unsaved)"))
        self.layer_combo.blockSignals(True)
        self.layer_combo.clear()
        for layer in self.graph.get("layers", []):
            self.layer_combo.addItem(layer.get("name", "Layer"))
        self.layer_combo.blockSignals(False)
        self.layer_index = 0
        self._refresh_params_ui()
        self._refresh_layer_ui()
        self._refresh_canvas()

    def _layer(self):
        layers = self.graph.get("layers", []) if self.graph else []
        if 0 <= self.layer_index < len(layers):
            return layers[self.layer_index]
        return None

    def _refresh_canvas(self):
        layer = self._layer()
        self.canvas.set_state_machine(layer.get("stateMachine") if layer else None)
        self.state_group.setVisible(False)
        self.trans_group.setVisible(False)

    def _on_layer_changed(self, index):
        self.layer_index = index
        self._refresh_layer_ui()
        self._refresh_canvas()

    def _on_add_layer(self):
        if not self.graph:
            return
        name, ok = QInputDialog.getText(self, "Add Layer", "Layer name:")
        if not ok or not name.strip():
            return
        self.graph["layers"].append(M.new_layer(name.strip()))
        self._sync_graph_to_ui()
        self.layer_combo.setCurrentIndex(len(self.graph["layers"]) - 1)

    # ----- States / transitions -----

    def _on_add_state(self, blend_tree):
        layer = self._layer()
        if not layer:
            return
        sm = layer["stateMachine"]
        base = "BlendState" if blend_tree else "State"
        name = base
        i = 1
        existing = {s["name"] for s in sm["states"]}
        while name in existing:
            i += 1
            name = "%s%d" % (base, i)
        pos = (40 + len(sm["states"]) * 30, 40 + (len(sm["states"]) % 4) * 60)
        state = M.new_blendtree_state(name, pos) if blend_tree else M.new_clip_state(name, "", pos)
        sm["states"].append(state)
        if not sm.get("entry"):
            sm["entry"] = name
        self._refresh_canvas()

    def _on_connect_toggled(self, checked):
        self.canvas.connect_mode = checked
        self.canvas._connect_source = -1
        self.canvas.update()

    def _on_delete(self):
        layer = self._layer()
        if not layer:
            return
        sm = layer["stateMachine"]
        if self.canvas.sel_state >= 0:
            name = sm["states"][self.canvas.sel_state]["name"]
            sm["states"].pop(self.canvas.sel_state)
            sm["transitions"] = [t for t in sm["transitions"]
                                 if t.get("from") != name and t.get("to") != name]
            if sm.get("entry") == name:
                sm["entry"] = sm["states"][0]["name"] if sm["states"] else ""
            self.canvas.sel_state = -1
        elif self.canvas.sel_transition >= 0:
            sm["transitions"].pop(self.canvas.sel_transition)
            self.canvas.sel_transition = -1
        self._refresh_canvas()

    def _on_set_entry(self):
        layer = self._layer()
        if layer and self.canvas.sel_state >= 0:
            layer["stateMachine"]["entry"] = layer["stateMachine"]["states"][self.canvas.sel_state]["name"]
            self.canvas.update()

    def _on_graph_changed(self):
        self.canvas.update()

    def _on_selection_cleared(self):
        self.state_group.setVisible(False)
        self.trans_group.setVisible(False)

    def _sel_state(self):
        layer = self._layer()
        if layer and 0 <= self.canvas.sel_state < len(layer["stateMachine"]["states"]):
            return layer["stateMachine"]["states"][self.canvas.sel_state]
        return None

    def _sel_transition(self):
        layer = self._layer()
        if layer and 0 <= self.canvas.sel_transition < len(layer["stateMachine"]["transitions"]):
            return layer["stateMachine"]["transitions"][self.canvas.sel_transition]
        return None

    def _on_state_selected(self, index):
        self.trans_group.setVisible(False)
        state = self._sel_state()
        if not state:
            return
        self.state_group.setVisible(True)
        is_bt = state.get("type") == "blendtree"
        self.state_name_edit.setText(state.get("name", ""))
        self.state_speed_spin.blockSignals(True)
        self.state_speed_spin.setValue(float(state.get("speed", 1.0)))
        self.state_speed_spin.blockSignals(False)

        clip_names = self._player_clip_names()
        self.state_clip_combo.blockSignals(True)
        self.state_clip_combo.clear()
        self.state_clip_combo.addItems(clip_names)
        if not is_bt:
            self.state_clip_combo.setCurrentText(state.get("clip", ""))
        self.state_clip_combo.blockSignals(False)

        clip_label = self.state_form.labelForField(self.state_clip_combo)
        if clip_label is not None:
            clip_label.setVisible(not is_bt)
        self.state_clip_combo.setVisible(not is_bt)
        for widget in (self.bt_type_combo, self.bt_paramx_combo, self.bt_paramy_combo, self.blend_space):
            widget.setVisible(is_bt)
        if is_bt:
            self._refresh_blend_tree_ui(state.get("blendTree", M.new_blend_node("clip")))
        else:
            self._clear_layout(self.bt_children_container)

    def _on_transition_selected(self, index):
        self.state_group.setVisible(False)
        tr = self._sel_transition()
        if not tr:
            return
        self.trans_group.setVisible(True)
        self.trans_label.setText("%s -> %s" % (tr.get("from") or "Any", tr.get("to")))
        self.trans_exit_check.blockSignals(True)
        self.trans_exit_check.setChecked(bool(tr.get("hasExitTime", False)))
        self.trans_exit_check.blockSignals(False)
        self.trans_exit_spin.blockSignals(True)
        self.trans_exit_spin.setValue(float(tr.get("exitTime", 1.0)))
        self.trans_exit_spin.blockSignals(False)
        self.trans_dur_spin.blockSignals(True)
        self.trans_dur_spin.setValue(float(tr.get("duration", 0.2)))
        self.trans_dur_spin.blockSignals(False)
        self._refresh_conditions_ui(tr)

    # ----- State editing -----

    def _on_state_name_edited(self):
        state = self._sel_state()
        if not state:
            return
        old = state.get("name", "")
        new = self.state_name_edit.text().strip()
        if not new or new == old:
            return
        layer = self._layer()
        sm = layer["stateMachine"]
        state["name"] = new
        if sm.get("entry") == old:
            sm["entry"] = new
        for tr in sm["transitions"]:
            if tr.get("from") == old:
                tr["from"] = new
            if tr.get("to") == old:
                tr["to"] = new
        self.canvas.update()

    def _on_state_speed_edited(self, value):
        state = self._sel_state()
        if state:
            state["speed"] = float(value)

    def _on_state_clip_edited(self, text):
        state = self._sel_state()
        if state and state.get("type") != "blendtree":
            state["clip"] = text
            self.canvas.update()

    def _on_bt_type_changed(self, text):
        state = self._sel_state()
        if not state or state.get("type") != "blendtree":
            return
        bt = state.get("blendTree", {})
        if bt.get("type") == text:
            return
        new_bt = M.new_blend_node(text)
        # Carry over child clip nodes when possible.
        if "children" in new_bt and bt.get("children"):
            new_bt["children"] = bt["children"]
        state["blendTree"] = new_bt
        self._refresh_blend_tree_ui(new_bt)

    def _on_bt_paramx_changed(self, text):
        state = self._sel_state()
        if state and state.get("type") == "blendtree":
            bt = state["blendTree"]
            key = "parameterX" if bt.get("type") == "blend2d" else "parameter"
            bt[key] = text

    def _on_bt_paramy_changed(self, text):
        state = self._sel_state()
        if state and state.get("type") == "blendtree":
            state["blendTree"]["parameterY"] = text

    def _refresh_blend_tree_ui(self, bt):
        node_type = bt.get("type", "clip")
        self.bt_type_combo.blockSignals(True)
        self.bt_type_combo.setCurrentText(node_type)
        self.bt_type_combo.blockSignals(False)
        param_names = [p["name"] for p in self.graph.get("parameters", [])]
        for combo, key in ((self.bt_paramx_combo, "parameterX" if node_type == "blend2d" else "parameter"),
                           (self.bt_paramy_combo, "parameterY")):
            combo.blockSignals(True)
            combo.clear()
            combo.addItems(param_names)
            combo.setCurrentText(bt.get(key, ""))
            combo.blockSignals(False)
        self.bt_paramy_combo.setVisible(node_type == "blend2d")
        self.bt_paramx_combo.setVisible(node_type in ("blend1d", "blend2d", "blend2", "add2", "timescale"))
        self.blend_space.setVisible(node_type == "blend2d")
        if node_type == "blend2d":
            self.blend_space.set_children(bt.get("children", []))
        self._refresh_bt_children(bt)

    def _refresh_bt_children(self, bt):
        self._clear_layout(self.bt_children_container)
        node_type = bt.get("type", "clip")
        if "children" not in bt:
            return
        clip_names = self._player_clip_names()
        param_names = [p["name"] for p in self.graph.get("parameters", [])]
        for ci, child in enumerate(bt["children"]):
            row = QHBoxLayout()
            clip_combo = QComboBox()
            clip_combo.setEditable(True)
            clip_combo.addItems(clip_names)
            clip_combo.setCurrentText(child.get("node", {}).get("clip", ""))
            clip_combo.currentTextChanged.connect(
                lambda text, c=child: c.setdefault("node", M.new_blend_node("clip")).__setitem__("clip", text))
            row.addWidget(clip_combo)
            if node_type == "blend1d":
                thr = QDoubleSpinBox()
                thr.setRange(-1e6, 1e6)
                thr.setValue(float(child.get("threshold", 0.0)))
                thr.valueChanged.connect(lambda v, c=child: c.__setitem__("threshold", float(v)))
                row.addWidget(QLabel("thr"))
                row.addWidget(thr)
            elif node_type == "blend2d":
                for axis in ("posX", "posY"):
                    spin = QDoubleSpinBox()
                    spin.setRange(-1e6, 1e6)
                    spin.setValue(float(child.get(axis, 0.0)))
                    spin.valueChanged.connect(lambda v, c=child, a=axis: self._set_child_axis(c, a, float(v)))
                    row.addWidget(spin)
            elif node_type == "direct":
                pcombo = QComboBox()
                pcombo.setEditable(True)
                pcombo.addItems(param_names)
                pcombo.setCurrentText(child.get("parameter", ""))
                pcombo.currentTextChanged.connect(lambda text, c=child: c.__setitem__("parameter", text))
                row.addWidget(pcombo)
            remove = QPushButton("x")
            remove.setMaximumWidth(24)
            remove.clicked.connect(lambda _, idx=ci, b=bt: self._remove_bt_child(b, idx))
            row.addWidget(remove)
            self.bt_children_container.addLayout(row)

    def _set_child_axis(self, child, axis, value):
        child[axis] = value
        state = self._sel_state()
        if state and state.get("type") == "blendtree":
            self.blend_space.set_children(state["blendTree"].get("children", []))

    def _on_add_bt_child(self):
        state = self._sel_state()
        if not state or state.get("type") != "blendtree":
            return
        bt = state["blendTree"]
        if "children" not in bt:
            return
        child = {"node": M.new_blend_node("clip"), "threshold": 0.0, "posX": 0.0, "posY": 0.0, "parameter": ""}
        bt["children"].append(child)
        self._refresh_blend_tree_ui(bt)

    def _remove_bt_child(self, bt, index):
        if 0 <= index < len(bt.get("children", [])):
            bt["children"].pop(index)
            self._refresh_blend_tree_ui(bt)

    # ----- Transition editing -----

    def _on_trans_edited(self, *_):
        tr = self._sel_transition()
        if not tr:
            return
        tr["hasExitTime"] = self.trans_exit_check.isChecked()
        tr["exitTime"] = float(self.trans_exit_spin.value())
        tr["duration"] = float(self.trans_dur_spin.value())

    def _refresh_conditions_ui(self, tr):
        self._clear_layout(self.cond_container)
        param_names = [p["name"] for p in self.graph.get("parameters", [])]
        for ci, cond in enumerate(tr.get("conditions", [])):
            row = QHBoxLayout()
            pcombo = QComboBox()
            pcombo.setEditable(True)
            pcombo.addItems(param_names)
            pcombo.setCurrentText(cond.get("param", ""))
            pcombo.currentTextChanged.connect(lambda text, c=cond: c.__setitem__("param", text))
            row.addWidget(pcombo)
            opcombo = QComboBox()
            opcombo.addItems(M.CONDITION_OPS)
            opcombo.setCurrentText(cond.get("op", "greater"))
            opcombo.currentTextChanged.connect(lambda text, c=cond: c.__setitem__("op", text))
            row.addWidget(opcombo)
            vspin = QDoubleSpinBox()
            vspin.setRange(-1e6, 1e6)
            vspin.setValue(float(cond.get("value", 0.0)))
            vspin.valueChanged.connect(lambda v, c=cond: c.__setitem__("value", float(v)))
            row.addWidget(vspin)
            remove = QPushButton("x")
            remove.setMaximumWidth(24)
            remove.clicked.connect(lambda _, idx=ci, t=tr: self._remove_condition(t, idx))
            row.addWidget(remove)
            self.cond_container.addLayout(row)

    def _on_add_condition(self):
        tr = self._sel_transition()
        if tr is not None:
            tr.setdefault("conditions", []).append(M.new_condition())
            self._refresh_conditions_ui(tr)

    def _remove_condition(self, tr, index):
        if 0 <= index < len(tr.get("conditions", [])):
            tr["conditions"].pop(index)
            self._refresh_conditions_ui(tr)

    # ----- Parameters -----

    def _refresh_params_ui(self):
        self._clear_layout(self.params_container)
        if not self.graph:
            return
        for pi, param in enumerate(self.graph.get("parameters", [])):
            row = QHBoxLayout()
            row.addWidget(QLabel(param.get("name", "")))
            row.addWidget(QLabel("[%s]" % param.get("type", "float")))
            ptype = param.get("type", "float")
            if ptype in ("bool", "trigger"):
                check = QCheckBox()
                check.setChecked(bool(param.get("default", False)))
                check.toggled.connect(lambda v, p=param: self._set_param_live(p, bool(v)))
                row.addWidget(check)
            else:
                spin = QDoubleSpinBox()
                spin.setRange(-1e6, 1e6)
                spin.setValue(float(param.get("default", 0)))
                spin.valueChanged.connect(lambda v, p=param: self._set_param_live(p, v))
                row.addWidget(spin)
            remove = QPushButton("x")
            remove.setMaximumWidth(24)
            remove.clicked.connect(lambda _, idx=pi: self._remove_param(idx))
            row.addWidget(remove)
            self.params_container.addLayout(row)

    def _on_add_param(self):
        if not self.graph:
            return
        name, ok = QInputDialog.getText(self, "Add Parameter", "Parameter name:")
        if not ok or not name.strip():
            return
        ptype, ok = QInputDialog.getItem(self, "Add Parameter", "Type:", M.PARAM_TYPES, 0, False)
        if not ok:
            return
        self.graph["parameters"].append(M.new_param(name.strip(), ptype))
        self._refresh_params_ui()

    def _remove_param(self, index):
        if self.graph and 0 <= index < len(self.graph["parameters"]):
            self.graph["parameters"].pop(index)
            self._refresh_params_ui()

    def _set_param_live(self, param, value):
        if not self.tree_component or not self.editor_bridge:
            return
        ptype = param.get("type", "float")
        method = {"float": "set_float", "int": "set_int", "bool": "set_bool", "trigger": "set_trigger"}.get(ptype)
        try:
            if ptype == "trigger":
                if value:
                    self.editor_bridge.call_component_method(self.tree_component, "set_trigger",
                                                             json.dumps([param["name"]]))
            else:
                arg = int(value) if ptype == "int" else (bool(value) if ptype == "bool" else float(value))
                self.editor_bridge.call_component_method(self.tree_component, method,
                                                         json.dumps([param["name"], arg]))
        except Exception as exc:
            print("BlendTree: set param failed:", exc)

    # ----- Layers -----

    def _refresh_layer_ui(self):
        layer = self._layer()
        if not layer:
            return
        self.layer_weight_spin.blockSignals(True)
        self.layer_weight_spin.setValue(float(layer.get("weight", 1.0)))
        self.layer_weight_spin.blockSignals(False)
        self.layer_mode_combo.blockSignals(True)
        self.layer_mode_combo.setCurrentText(layer.get("blendMode", "override"))
        self.layer_mode_combo.blockSignals(False)
        mask = layer.get("mask", {"mode": "include", "targets": []})
        self.layer_mask_edit.setText(", ".join(mask.get("targets", [])))
        self.layer_mask_mode.blockSignals(True)
        self.layer_mask_mode.setCurrentText(mask.get("mode", "include"))
        self.layer_mask_mode.blockSignals(False)

    def _on_layer_edited(self, *_):
        layer = self._layer()
        if not layer:
            return
        layer["weight"] = float(self.layer_weight_spin.value())
        layer["blendMode"] = self.layer_mode_combo.currentText()
        layer.setdefault("mask", {})["mode"] = self.layer_mask_mode.currentText()

    def _on_layer_mask_edited(self):
        layer = self._layer()
        if not layer:
            return
        text = self.layer_mask_edit.text().strip()
        targets = [t.strip() for t in text.split(",") if t.strip()]
        layer.setdefault("mask", {"mode": "include"})["targets"] = targets

    # ----- Utility -----

    def _clear_layout(self, layout):
        while layout.count():
            item = layout.takeAt(0)
            if item.layout():
                self._clear_layout(item.layout())
                item.layout().deleteLater()
            elif item.widget():
                item.widget().deleteLater()
