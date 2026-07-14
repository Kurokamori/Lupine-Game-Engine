"""
Particle Scale-Over-Life Curve Editor

A multi-point curve editor for Particles2D / Particles3D. It edits the
component's `scaleCurve` property, stored as JSON matching the engine's
lupine::math::Curve format:

    {"points": [{"pos": float, "value": float}, ...]}

Each point has a position spinbox (0..1), a value spinbox, and a remove button.
Clicking the preview adds a point at that position. A valid curve (one or more
points) overrides the simple scaleEnd ramp; an empty curve falls back to it.
"""

import json

from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel, QPushButton,
                             QDoubleSpinBox, QSizePolicy)
from PyQt6.QtCore import pyqtSignal, Qt, QPointF
from PyQt6.QtGui import QPainter, QPen, QColor, QPolygonF

from editor.theme import get_theme_manager


class _CurveView(QWidget):
    """Paints the curve polyline; click adds a point at that normalized x."""

    clicked_at = pyqtSignal(float, float)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._points = []  # list[(pos, value)]
        self._value_max = 2.0
        self.setMinimumHeight(80)
        self.setCursor(Qt.CursorShape.PointingHandCursor)
        self.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)
        self.setToolTip("Click to add a point")

    def set_points(self, points, value_max):
        self._points = points
        self._value_max = max(0.001, value_max)
        self.update()

    def _plot_rect(self):
        return self.rect().adjusted(5, 5, -6, -6)

    def mousePressEvent(self, event):
        rect = self._plot_rect()
        w = max(1, rect.width())
        h = max(1, rect.height())
        pos = min(1.0, max(0.0, (event.position().x() - rect.left()) / w))
        value = (1.0 - min(1.0, max(0.0, (event.position().y() - rect.top()) / h))) * self._value_max
        self.clicked_at.emit(pos, value)

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing, True)

        painter.fillRect(self.rect(), QColor(34, 34, 38))
        rect = self._plot_rect()
        if rect.width() <= 0 or rect.height() <= 0:
            painter.end()
            return

        left = rect.left()
        right = rect.right()
        top = rect.top()
        bottom = rect.bottom()
        width = rect.width()
        height = rect.height()

        # Grid.
        painter.setPen(QPen(QColor(60, 60, 66), 1))
        for i in range(1, 4):
            y = int(top + i * height / 4)
            painter.drawLine(left, y, right, y)
            x = int(left + i * width / 4)
            painter.drawLine(x, top, x, bottom)

        def to_px(pos, value):
            x = left + pos * width
            y = bottom - (value / self._value_max) * height
            return QPointF(float(x), float(y))

        if self._points:
            polyline = QPolygonF()
            if self._points[0][0] > 0.0:
                polyline.append(to_px(0.0, self._points[0][1]))
            for pos, value in self._points:
                polyline.append(to_px(pos, value))
            if self._points[-1][0] < 1.0:
                polyline.append(to_px(1.0, self._points[-1][1]))

            painter.setPen(QPen(QColor(120, 190, 255), 2))
            painter.drawPolyline(polyline)

            painter.setBrush(QColor(255, 255, 255))
            painter.setPen(QPen(QColor(34, 34, 38), 1))
            for pos, value in self._points:
                painter.drawEllipse(to_px(pos, value), 3.0, 3.0)
        else:
            # No curve points -> the emitter uses the simple scaleEnd ramp; show a
            # flat reference line at scale = 1.0 so the empty state reads clearly.
            pen = QPen(QColor(110, 110, 120), 1, Qt.PenStyle.DashLine)
            painter.setPen(pen)
            y = to_px(0.0, 1.0).y()
            painter.drawLine(left, int(y), right, int(y))

        # Reset the brush: drawEllipse above left a white fill brush set, which
        # would otherwise fill the whole border rect white.
        painter.setBrush(Qt.BrushStyle.NoBrush)
        painter.setPen(QPen(QColor(80, 80, 88), 1))
        painter.drawRect(left, top, width, height)
        painter.end()


class ParticleCurveWidget(QWidget):
    """Edit a particle emitter's scaleCurve as a multi-point curve."""

    value_changed = pyqtSignal(object)

    def __init__(self, component, editor_bridge, main_editor=None, parent=None):
        super().__init__(parent)
        self.component = component
        self.editor_bridge = editor_bridge
        self.main_editor = main_editor
        self._points = []  # list[(pos, value)], kept sorted by pos

        self._init_ui()
        self._load_from_component()

    # ------------------------------------------------------------------ logging
    def _log(self, message, level="Debug"):
        if (self.main_editor and hasattr(self.main_editor, 'panels')
                and 'console' in self.main_editor.panels):
            self.main_editor.panels['console'].log_message(f"[ParticleCurve] {message}", level)

    # ------------------------------------------------------------------ UI setup
    def _init_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 4, 0, 4)
        layout.setSpacing(4)

        theme = get_theme_manager().get_current_theme()
        text_color = theme.colors.text_primary if theme else "#ffffff"

        title = QLabel("Scale Over Life")
        title.setStyleSheet(f"color: {text_color}; font-weight: 600;")
        layout.addWidget(title)

        self._view = _CurveView()
        self._view.clicked_at.connect(self._add_point)
        layout.addWidget(self._view)

        self._points_container = QVBoxLayout()
        self._points_container.setSpacing(2)
        layout.addLayout(self._points_container)

        button_row = QHBoxLayout()
        add_button = QPushButton("Add Point")
        add_button.setCursor(Qt.CursorShape.PointingHandCursor)
        add_button.clicked.connect(lambda: self._add_point(0.5, 1.0))
        button_row.addWidget(add_button)

        clear_button = QPushButton("Clear")
        clear_button.setCursor(Qt.CursorShape.PointingHandCursor)
        clear_button.setToolTip("Remove all points (fall back to scaleEnd)")
        clear_button.clicked.connect(self._clear_points)
        button_row.addWidget(clear_button)
        button_row.addStretch()
        layout.addLayout(button_row)

    # ------------------------------------------------------------- component I/O
    def _read_json_string_property(self, prop_name):
        """Read a String property whose value is itself a JSON string.

        get_component_property returns the JSON-encoded property value; for a
        String property that decodes to the inner string, which is then parsed
        as JSON (double-decode, matching the engine's String-property storage).
        """
        try:
            raw = self.editor_bridge.get_component_property(self.component, prop_name)
            if not raw:
                return None
            decoded = json.loads(raw) if isinstance(raw, str) else raw
            if not isinstance(decoded, str) or not decoded:
                return None
            return json.loads(decoded)
        except Exception as e:
            self._log(f"read '{prop_name}' failed: {e}", "Warning")
            return None

    def _write_json_string_property(self, prop_name, payload):
        """Write a String property whose value is a JSON string (double-encode)."""
        inner = json.dumps(payload)
        self.editor_bridge.set_component_property(self.component, prop_name, json.dumps(inner))

    def _load_from_component(self):
        points = []
        data = self._read_json_string_property("scaleCurve")
        if isinstance(data, dict) and isinstance(data.get("points"), list):
            for pt in data["points"]:
                points.append((float(pt.get("pos", 0.0)), float(pt.get("value", 1.0))))

        self._points = sorted(points, key=lambda p: p[0])
        self._rebuild_rows()
        self._refresh_view()

    def _write_curve(self):
        payload = {"points": [{"pos": pos, "value": value} for pos, value in self._points]}
        try:
            self._write_json_string_property("scaleCurve", payload)
            if hasattr(self.editor_bridge, 'mark_scene_dirty'):
                self.editor_bridge.mark_scene_dirty()
        except Exception as e:
            self._log(f"write scaleCurve failed: {e}", "Warning")
        self._refresh_view()
        self.value_changed.emit(None)

    # ----------------------------------------------------------------- behaviour
    def _add_point(self, pos, value):
        self._points.append((float(pos), float(value)))
        self._points.sort(key=lambda p: p[0])
        self._rebuild_rows()
        self._write_curve()

    def _change_point(self, index, pos=None, value=None):
        if index < 0 or index >= len(self._points):
            return
        cur_pos, cur_value = self._points[index]
        new_pos = cur_pos if pos is None else float(pos)
        new_value = cur_value if value is None else float(value)
        self._points[index] = (new_pos, new_value)
        if pos is not None:
            self._points.sort(key=lambda p: p[0])
            self._rebuild_rows()
        self._write_curve()

    def _remove_point(self, index):
        if index < 0 or index >= len(self._points):
            return
        del self._points[index]
        self._rebuild_rows()
        self._write_curve()

    def _clear_points(self):
        self._points = []
        self._rebuild_rows()
        self._write_curve()

    # --------------------------------------------------------------------- views
    def _clear_rows(self):
        while self._points_container.count():
            item = self._points_container.takeAt(0)
            sub = item.layout()
            if sub is not None:
                while sub.count():
                    w = sub.takeAt(0).widget()
                    if w is not None:
                        w.deleteLater()
            widget = item.widget()
            if widget is not None:
                widget.deleteLater()

    def _rebuild_rows(self):
        self._clear_rows()
        for index, (pos, value) in enumerate(self._points):
            row = QHBoxLayout()
            row.setSpacing(4)

            pos_label = QLabel("t")
            row.addWidget(pos_label)
            pos_spin = QDoubleSpinBox()
            pos_spin.setRange(0.0, 1.0)
            pos_spin.setSingleStep(0.05)
            pos_spin.setDecimals(3)
            pos_spin.setValue(pos)
            pos_spin.setMaximumWidth(70)
            pos_spin.valueChanged.connect(lambda v, i=index: self._change_point(i, pos=v))
            row.addWidget(pos_spin)

            val_label = QLabel("scale")
            row.addWidget(val_label)
            val_spin = QDoubleSpinBox()
            val_spin.setRange(0.0, 64.0)
            val_spin.setSingleStep(0.1)
            val_spin.setDecimals(3)
            val_spin.setValue(value)
            val_spin.setMaximumWidth(80)
            val_spin.valueChanged.connect(lambda v, i=index: self._change_point(i, value=v))
            row.addWidget(val_spin)

            remove = QPushButton("✕")
            remove.setMaximumWidth(28)
            remove.setMinimumHeight(24)
            remove.setCursor(Qt.CursorShape.PointingHandCursor)
            remove.clicked.connect(lambda _checked=False, i=index: self._remove_point(i))
            row.addWidget(remove)
            row.addStretch()

            self._points_container.addLayout(row)

    def _refresh_view(self):
        value_max = 2.0
        for _, value in self._points:
            value_max = max(value_max, value)
        self._view.set_points(self._points, value_max)
