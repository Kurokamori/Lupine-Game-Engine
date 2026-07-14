"""
Custom Inspector Widget for Curve3D and Path3D Components

Provides 3D point editing with Bezier curve support, per-point tilt, interactive
viewport point creation, and a top-down (XZ) curve preview. Interactive control
points are dragged directly in the 3D viewport (see viewport_widget.py).
"""

from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel,
                             QPushButton, QListWidget, QListWidgetItem,
                             QDoubleSpinBox, QFrame, QCheckBox,
                             QGroupBox, QSizePolicy)
from PyQt6.QtCore import Qt, pyqtSignal
from PyQt6.QtGui import QPainter, QPen, QPainterPath, QColor
import json
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent))
from theme import get_theme_manager


class Curve3DTopDownPreview(QWidget):
    """Top-down (XZ-plane) preview of a Curve3D/Path3D."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.points = []
        self.setMinimumHeight(100)
        self.setMaximumHeight(150)
        self.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

    def set_points(self, points):
        self.points = points
        self.update()

    def paintEvent(self, event):
        if not self.points or len(self.points) < 2:
            return

        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)

        theme = get_theme_manager().get_current_theme()
        bg_color = QColor(theme.colors.surface) if theme else QColor("#1e1a28")
        line_color = QColor(theme.colors.accent_color) if theme else QColor("#5a4a7a")
        handle_color = QColor("#ff8844")
        point_color = QColor("#44ff88")

        painter.fillRect(self.rect(), bg_color)

        # Project onto the XZ plane (top-down).
        def cx(p):
            return p['x']

        def cz(p):
            return p['z']

        min_x = min(cx(p) for p in self.points)
        max_x = max(cx(p) for p in self.points)
        min_z = min(cz(p) for p in self.points)
        max_z = max(cz(p) for p in self.points)

        for p in self.points:
            if p.get('useBezier', False):
                min_x = min(min_x, p['x'] + p.get('ctrlInX', 0), p['x'] + p.get('ctrlOutX', 0))
                max_x = max(max_x, p['x'] + p.get('ctrlInX', 0), p['x'] + p.get('ctrlOutX', 0))
                min_z = min(min_z, p['z'] + p.get('ctrlInZ', 0), p['z'] + p.get('ctrlOutZ', 0))
                max_z = max(max_z, p['z'] + p.get('ctrlInZ', 0), p['z'] + p.get('ctrlOutZ', 0))

        range_x = max(max_x - min_x, 1)
        range_z = max(max_z - min_z, 1)

        padding = 15
        w = self.width() - 2 * padding
        h = self.height() - 2 * padding
        scale = min(w / range_x, h / range_z) * 0.9

        def to_screen(x, z):
            sx = padding + (x - min_x) * scale + (w - range_x * scale) / 2
            # +Z draws downward (top-down view).
            sy = padding + (z - min_z) * scale + (h - range_z * scale) / 2
            return sx, sy

        path = QPainterPath()
        sx, sy = to_screen(self.points[0]['x'], self.points[0]['z'])
        path.moveTo(sx, sy)

        for i in range(len(self.points) - 1):
            p0 = self.points[i]
            p1 = self.points[i + 1]
            if p0.get('useBezier', False) or p1.get('useBezier', False):
                x1, y1 = to_screen(p0['x'] + p0.get('ctrlOutX', 0), p0['z'] + p0.get('ctrlOutZ', 0))
                x2, y2 = to_screen(p1['x'] + p1.get('ctrlInX', 0), p1['z'] + p1.get('ctrlInZ', 0))
                x3, y3 = to_screen(p1['x'], p1['z'])
                path.cubicTo(x1, y1, x2, y2, x3, y3)
            else:
                sx, sy = to_screen(p1['x'], p1['z'])
                path.lineTo(sx, sy)

        painter.setPen(QPen(line_color, 2))
        painter.drawPath(path)

        painter.setPen(QPen(handle_color, 1, Qt.PenStyle.DashLine))
        for p in self.points:
            if p.get('useBezier', False):
                px, py = to_screen(p['x'], p['z'])
                cin_x, cin_y = to_screen(p['x'] + p.get('ctrlInX', 0), p['z'] + p.get('ctrlInZ', 0))
                cout_x, cout_y = to_screen(p['x'] + p.get('ctrlOutX', 0), p['z'] + p.get('ctrlOutZ', 0))
                painter.drawLine(int(px), int(py), int(cin_x), int(cin_y))
                painter.drawLine(int(px), int(py), int(cout_x), int(cout_y))
                painter.setBrush(handle_color)
                painter.drawEllipse(int(cin_x) - 3, int(cin_y) - 3, 6, 6)
                painter.drawEllipse(int(cout_x) - 3, int(cout_y) - 3, 6, 6)

        painter.setPen(QPen(point_color, 1))
        painter.setBrush(point_color)
        for p in self.points:
            px, py = to_screen(p['x'], p['z'])
            painter.drawEllipse(int(px) - 4, int(py) - 4, 8, 8)


class Curve3DPropertyWidget(QWidget):
    """Custom widget for Curve3D/Path3D with 3D Bezier + tilt editing."""

    value_changed = pyqtSignal(str, object)
    curve3d_mode_changed = pyqtSignal(bool)        # is_creating_curve
    curve3d_edit_changed = pyqtSignal(object)      # component or None

    def __init__(self, component, editor_bridge, main_editor=None, parent=None):
        super().__init__(parent)
        self.component = component
        self.editor_bridge = editor_bridge
        self.main_editor = main_editor
        self.is_creating_curve = False
        self.selected_point_index = -1
        self.component_type = component.get_type_name() if component else "Curve3D"

        self._init_ui()
        self._load_values()

    def _init_ui(self):
        theme = get_theme_manager().get_current_theme()
        bg_color = theme.colors.surface if theme else "#1e1a28"
        text_color = theme.colors.text_primary if theme else "#ffffff"
        border_color = theme.colors.border if theme else "#3d3650"
        button_color = theme.colors.accent_color if theme else "#5a4a7a"
        button_hover = theme.colors.accent_hover if theme else "#6a5a8a"
        text_secondary = theme.colors.text_secondary if theme else "#aaaaaa"
        button_secondary = theme.colors.surface_hover if theme else "#3a3a3a"

        layout = QVBoxLayout()
        layout.setContentsMargins(5, 5, 5, 5)
        layout.setSpacing(8)

        preview_label = QLabel("Curve Preview (top-down XZ):")
        preview_label.setStyleSheet(f"color: {text_color}; font-weight: bold;")
        layout.addWidget(preview_label)

        self.preview_widget = Curve3DTopDownPreview()
        self.preview_widget.setStyleSheet(f"border: 1px solid {border_color};")
        layout.addWidget(self.preview_widget)

        btn_text = f"Create {self.component_type} (Click in 3D Viewport)"
        self.create_curve_button = QPushButton(btn_text)
        self.create_curve_button.setStyleSheet(f"""
            QPushButton {{
                background-color: {button_color};
                color: {text_color};
                border: none;
                padding: 6px;
                border-radius: 3px;
            }}
            QPushButton:hover {{
                background-color: {button_hover};
            }}
        """)
        self.create_curve_button.clicked.connect(self._toggle_curve_creation)
        layout.addWidget(self.create_curve_button)

        points_frame = QFrame()
        points_frame.setStyleSheet(f"background-color: {bg_color}; border: 1px solid {border_color};")
        points_layout = QVBoxLayout(points_frame)
        points_layout.setContentsMargins(5, 5, 5, 5)

        points_header = QLabel("Points:")
        points_header.setStyleSheet(f"color: {text_color}; font-weight: bold; border: none;")
        points_layout.addWidget(points_header)

        self.points_list = QListWidget()
        self.points_list.setStyleSheet(f"""
            QListWidget {{
                background-color: {bg_color};
                color: {text_color};
                border: 1px solid {border_color};
            }}
            QListWidget::item:selected {{
                background-color: {button_color};
            }}
        """)
        self.points_list.setMaximumHeight(150)
        self.points_list.currentRowChanged.connect(self._on_point_selected)
        points_layout.addWidget(self.points_list)

        point_btn_layout = QHBoxLayout()
        self.clear_points_btn = QPushButton("Clear All")
        self.clear_points_btn.clicked.connect(self._clear_points)
        self.remove_point_btn = QPushButton("Remove Selected")
        self.remove_point_btn.clicked.connect(self._remove_selected_point)

        for btn in [self.clear_points_btn, self.remove_point_btn]:
            btn.setStyleSheet(f"""
                QPushButton {{
                    background-color: {button_secondary};
                    color: {text_color};
                    border: none;
                    padding: 4px;
                    border-radius: 3px;
                }}
                QPushButton:hover {{
                    background-color: {button_hover};
                }}
            """)

        point_btn_layout.addWidget(self.clear_points_btn)
        point_btn_layout.addWidget(self.remove_point_btn)
        points_layout.addLayout(point_btn_layout)

        layout.addWidget(points_frame)

        # Per-point controls (position, tilt, bezier handles).
        self.point_frame = QGroupBox("Selected Point")
        self.point_frame.setStyleSheet(f"""
            QGroupBox {{
                color: {text_color};
                border: 1px solid {border_color};
                border-radius: 3px;
                margin-top: 6px;
                padding-top: 10px;
            }}
            QGroupBox::title {{
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 3px 0 3px;
            }}
        """)
        point_layout = QVBoxLayout(self.point_frame)

        # Position X/Y/Z.
        pos_layout = QHBoxLayout()
        pos_label = QLabel("Position:")
        pos_label.setStyleSheet(f"color: {text_secondary};")
        pos_label.setFixedWidth(70)
        pos_layout.addWidget(pos_label)
        self.pos_x = self._make_spin()
        self.pos_y = self._make_spin()
        self.pos_z = self._make_spin()
        for s in (self.pos_x, self.pos_y, self.pos_z):
            s.valueChanged.connect(self._on_position_changed)
            pos_layout.addWidget(s)
        point_layout.addLayout(pos_layout)

        # Tilt.
        tilt_layout = QHBoxLayout()
        tilt_label = QLabel("Tilt (deg):")
        tilt_label.setStyleSheet(f"color: {text_secondary};")
        tilt_label.setFixedWidth(70)
        tilt_layout.addWidget(tilt_label)
        self.tilt_spin = QDoubleSpinBox()
        self.tilt_spin.setRange(-360.0, 360.0)
        self.tilt_spin.setDecimals(1)
        self.tilt_spin.valueChanged.connect(self._on_tilt_changed)
        tilt_layout.addWidget(self.tilt_spin)
        point_layout.addLayout(tilt_layout)

        self.use_bezier_check = QCheckBox("Use Bezier Curve")
        self.use_bezier_check.setStyleSheet(f"color: {text_color};")
        self.use_bezier_check.stateChanged.connect(self._on_use_bezier_changed)
        point_layout.addWidget(self.use_bezier_check)

        self.symmetric_check = QCheckBox("Symmetric Handles")
        self.symmetric_check.setStyleSheet(f"color: {text_color};")
        self.symmetric_check.setChecked(True)
        self.symmetric_check.stateChanged.connect(self._on_control_changed)
        point_layout.addWidget(self.symmetric_check)

        ctrl_in_layout = QHBoxLayout()
        ctrl_in_label = QLabel("Control In:")
        ctrl_in_label.setStyleSheet(f"color: {text_secondary};")
        ctrl_in_label.setFixedWidth(70)
        ctrl_in_layout.addWidget(ctrl_in_label)
        self.ctrl_in_x = self._make_spin()
        self.ctrl_in_y = self._make_spin()
        self.ctrl_in_z = self._make_spin()
        for s in (self.ctrl_in_x, self.ctrl_in_y, self.ctrl_in_z):
            s.valueChanged.connect(self._on_control_changed)
            ctrl_in_layout.addWidget(s)
        point_layout.addLayout(ctrl_in_layout)

        ctrl_out_layout = QHBoxLayout()
        ctrl_out_label = QLabel("Control Out:")
        ctrl_out_label.setStyleSheet(f"color: {text_secondary};")
        ctrl_out_label.setFixedWidth(70)
        ctrl_out_layout.addWidget(ctrl_out_label)
        self.ctrl_out_x = self._make_spin()
        self.ctrl_out_y = self._make_spin()
        self.ctrl_out_z = self._make_spin()
        for s in (self.ctrl_out_x, self.ctrl_out_y, self.ctrl_out_z):
            s.valueChanged.connect(self._on_control_changed)
            ctrl_out_layout.addWidget(s)
        point_layout.addLayout(ctrl_out_layout)

        self.point_frame.setVisible(False)
        layout.addWidget(self.point_frame)

        self.help_label = QLabel()
        self.help_label.setWordWrap(True)
        self.help_label.setStyleSheet(f"color: {text_secondary}; font-size: 10px; padding: 5px;")
        self.help_label.setVisible(False)
        layout.addWidget(self.help_label)

        layout.addStretch()
        self.setLayout(layout)

    def _make_spin(self):
        spin = QDoubleSpinBox()
        spin.setRange(-100000.0, 100000.0)
        spin.setDecimals(3)
        spin.setSingleStep(0.1)
        return spin

    def _load_values(self):
        self._refresh_points_list()

    def _toggle_curve_creation(self):
        self.is_creating_curve = not self.is_creating_curve
        if self.is_creating_curve:
            self.create_curve_button.setText(f"Finish Creating {self.component_type}")
            self.help_label.setText(
                f"{self.component_type} Creation Mode:\n"
                "• Left-click in the 3D viewport to add points on the node's height plane\n"
                "• Select a point and drag its handles in the viewport to shape the curve\n"
                f"• Click 'Finish Creating {self.component_type}' when done"
            )
            self.help_label.setVisible(True)
            self._set_show_handles(True)
        else:
            self.create_curve_button.setText(f"Create {self.component_type} (Click in 3D Viewport)")
            self.help_label.setVisible(False)
            self._set_show_handles(False)
        self.curve3d_mode_changed.emit(self.is_creating_curve)

    def _set_show_handles(self, show):
        if self.editor_bridge and self.component:
            try:
                self.editor_bridge.set_curve3d_show_handles(self.component, show)
            except Exception as e:
                print(f"Error setting show handles: {e}")

    def _set_selected_point(self, index):
        if self.editor_bridge and self.component:
            try:
                self.editor_bridge.set_curve3d_selected_point(self.component, index)
            except Exception as e:
                print(f"Error setting selected point: {e}")

    def _refresh_points_list(self):
        saved_selection = self.selected_point_index
        self.points_list.blockSignals(True)
        self.points_list.clear()

        if not self.component or not self.editor_bridge:
            self.points_list.blockSignals(False)
            return

        try:
            points = json.loads(self.editor_bridge.get_curve3d_points(self.component))
            for i, pt in enumerate(points):
                bezier_marker = " [B]" if pt.get('useBezier', False) else ""
                item = QListWidgetItem(
                    f"{i}: ({pt['x']:.1f}, {pt['y']:.1f}, {pt['z']:.1f}){bezier_marker}"
                )
                self.points_list.addItem(item)
            self.preview_widget.set_points(points)
            if 0 <= saved_selection < len(points):
                self.points_list.setCurrentRow(saved_selection)
                self.selected_point_index = saved_selection
        except Exception as e:
            print(f"Error refreshing {self.component_type} points list: {e}")
        finally:
            self.points_list.blockSignals(False)

    def _block_point_signals(self, blocked):
        for w in (self.pos_x, self.pos_y, self.pos_z, self.tilt_spin,
                  self.use_bezier_check, self.symmetric_check,
                  self.ctrl_in_x, self.ctrl_in_y, self.ctrl_in_z,
                  self.ctrl_out_x, self.ctrl_out_y, self.ctrl_out_z):
            w.blockSignals(blocked)

    def _on_point_selected(self, row):
        self.selected_point_index = row
        if row < 0:
            self.point_frame.setVisible(False)
            if not self.is_creating_curve:
                self._set_show_handles(False)
                self._set_selected_point(-1)
                self.curve3d_edit_changed.emit(None)
            return

        self.point_frame.setVisible(True)
        self._set_show_handles(True)
        self._set_selected_point(row)
        self.curve3d_edit_changed.emit(self.component)

        try:
            points = json.loads(self.editor_bridge.get_curve3d_points(self.component))
            if row < len(points):
                pt = points[row]
                self._block_point_signals(True)
                self.pos_x.setValue(pt.get('x', 0.0))
                self.pos_y.setValue(pt.get('y', 0.0))
                self.pos_z.setValue(pt.get('z', 0.0))
                self.tilt_spin.setValue(math.degrees(pt.get('tilt', 0.0)))
                self.use_bezier_check.setChecked(pt.get('useBezier', False))
                self.symmetric_check.setChecked(pt.get('symmetricHandles', True))
                self.ctrl_in_x.setValue(pt.get('ctrlInX', 0.0))
                self.ctrl_in_y.setValue(pt.get('ctrlInY', 0.0))
                self.ctrl_in_z.setValue(pt.get('ctrlInZ', 0.0))
                self.ctrl_out_x.setValue(pt.get('ctrlOutX', 0.0))
                self.ctrl_out_y.setValue(pt.get('ctrlOutY', 0.0))
                self.ctrl_out_z.setValue(pt.get('ctrlOutZ', 0.0))
                self._block_point_signals(False)
        except Exception as e:
            print(f"Error loading point data: {e}")

    def _on_position_changed(self):
        if self.selected_point_index < 0 or not self.editor_bridge:
            return
        try:
            self.editor_bridge.update_curve3d_point(
                self.component, self.selected_point_index,
                self.pos_x.value(), self.pos_y.value(), self.pos_z.value()
            )
            self._refresh_points_list()
        except Exception as e:
            print(f"Error updating point position: {e}")

    def _on_tilt_changed(self):
        if self.selected_point_index < 0 or not self.editor_bridge:
            return
        try:
            self.editor_bridge.set_curve3d_point_tilt(
                self.component, self.selected_point_index, math.radians(self.tilt_spin.value())
            )
        except Exception as e:
            print(f"Error updating tilt: {e}")

    def _on_use_bezier_changed(self, state):
        if self.selected_point_index < 0 or not self.editor_bridge:
            return
        try:
            self.editor_bridge.set_curve3d_point_use_bezier(
                self.component, self.selected_point_index, state == Qt.CheckState.Checked.value
            )
            self._refresh_points_list()
        except Exception as e:
            print(f"Error setting bezier: {e}")

    def _on_control_changed(self):
        if self.selected_point_index < 0 or not self.editor_bridge:
            return
        try:
            cin = [self.ctrl_in_x.value(), self.ctrl_in_y.value(), self.ctrl_in_z.value()]
            cout = [self.ctrl_out_x.value(), self.ctrl_out_y.value(), self.ctrl_out_z.value()]
            if self.symmetric_check.isChecked():
                cout = [-cin[0], -cin[1], -cin[2]]
                self._block_point_signals(True)
                self.ctrl_out_x.setValue(cout[0])
                self.ctrl_out_y.setValue(cout[1])
                self.ctrl_out_z.setValue(cout[2])
                self._block_point_signals(False)
            self.editor_bridge.update_curve3d_point_bezier(
                self.component, self.selected_point_index,
                cin[0], cin[1], cin[2], cout[0], cout[1], cout[2],
                self.symmetric_check.isChecked()
            )
            self._refresh_points_list()
        except Exception as e:
            print(f"Error updating control points: {e}")

    def _remove_selected_point(self):
        current_row = self.points_list.currentRow()
        if current_row >= 0:
            try:
                self.editor_bridge.remove_curve3d_point(self.component, current_row)
                self._refresh_points_list()
            except Exception as e:
                print(f"Error removing point: {e}")

    def _clear_points(self):
        if not self.component or not self.editor_bridge:
            return
        try:
            self.editor_bridge.clear_curve3d_points(self.component)
            self._refresh_points_list()
        except Exception as e:
            print(f"Error clearing points: {e}")

    def is_curve_creation_active(self):
        return self.is_creating_curve

    def set_value(self, value):
        pass

    def get_value(self):
        return None


# Alias for Path3D - same widget works for both
Path3DPropertyWidget = Curve3DPropertyWidget
