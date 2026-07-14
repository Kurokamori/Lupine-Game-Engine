"""
Particle Color-Over-Life Gradient Editor

A multi-stop gradient editor for Particles2D / Particles3D. It edits the
component's `colorGradient` property, stored as JSON matching the engine's
lupine::math::Gradient format:

    {"stops": [{"pos": float, "color": {"r","g","b","a"}}, ...]}

Each stop has a position spinbox (0..1), an alpha-aware color swatch, and a
remove button. Clicking the gradient bar adds a stop at that position. A valid
gradient (two or more stops) overrides the simple colorStart/colorEnd ramp; on
first use the editor seeds itself from those two colors so the strip starts from
the emitter's current look.
"""

import json

from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel, QPushButton,
                             QColorDialog, QDoubleSpinBox, QSizePolicy)
from PyQt6.QtCore import pyqtSignal, Qt
from PyQt6.QtGui import QColor, QPainter, QLinearGradient, QBrush, QPen

from editor.theme import get_theme_manager


class _GradientBar(QWidget):
    """Draws the multi-stop ramp over an alpha checkerboard; click adds a stop."""

    clicked_at = pyqtSignal(float)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._stops = []  # list[(pos, QColor)]
        self.setMinimumHeight(30)
        self.setCursor(Qt.CursorShape.PointingHandCursor)
        self.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)
        self.setToolTip("Click to add a color stop")

    def set_stops(self, stops):
        self._stops = stops
        self.update()

    def mousePressEvent(self, event):
        width = max(1, self.width() - 1)
        pos = min(1.0, max(0.0, event.position().x() / width))
        self.clicked_at.emit(pos)

    def paintEvent(self, event):
        painter = QPainter(self)
        rect = self.rect().adjusted(0, 0, -1, -1)

        cell = 6
        light = QColor(120, 120, 120)
        dark = QColor(90, 90, 90)
        for y in range(rect.top(), rect.bottom() + 1, cell):
            for x in range(rect.left(), rect.right() + 1, cell):
                checker = ((x // cell) + (y // cell)) % 2 == 0
                painter.fillRect(x, y, cell, cell, light if checker else dark)

        if self._stops:
            gradient = QLinearGradient(float(rect.left()), 0.0, float(rect.right()), 0.0)
            if len(self._stops) == 1:
                gradient.setColorAt(0.0, self._stops[0][1])
                gradient.setColorAt(1.0, self._stops[0][1])
            else:
                for pos, color in self._stops:
                    gradient.setColorAt(min(1.0, max(0.0, pos)), color)
            painter.fillRect(rect, QBrush(gradient))

            # Stop markers.
            for pos, _ in self._stops:
                x = rect.left() + int(pos * rect.width())
                painter.setPen(QPen(QColor(0, 0, 0), 2))
                painter.drawLine(x, rect.top(), x, rect.bottom())
                painter.setPen(QPen(QColor(255, 255, 255), 1))
                painter.drawLine(x, rect.top(), x, rect.bottom())

        painter.setPen(QPen(QColor(0, 0, 0, 120)))
        painter.drawRect(rect)
        painter.end()


class ParticleGradientWidget(QWidget):
    """Edit a particle emitter's colorGradient as a multi-stop ramp."""

    value_changed = pyqtSignal(object)

    def __init__(self, component, editor_bridge, main_editor=None, parent=None):
        super().__init__(parent)
        self.component = component
        self.editor_bridge = editor_bridge
        self.main_editor = main_editor
        self._stops = []  # list[(pos, QColor)], kept sorted by pos

        self._init_ui()
        self._load_from_component()

    # ------------------------------------------------------------------ logging
    def _log(self, message, level="Debug"):
        if (self.main_editor and hasattr(self.main_editor, 'panels')
                and 'console' in self.main_editor.panels):
            self.main_editor.panels['console'].log_message(f"[ParticleGradient] {message}", level)

    # ------------------------------------------------------------------ UI setup
    def _init_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 4, 0, 4)
        layout.setSpacing(4)

        theme = get_theme_manager().get_current_theme()
        text_color = theme.colors.text_primary if theme else "#ffffff"

        title = QLabel("Color Over Life")
        title.setStyleSheet(f"color: {text_color}; font-weight: 600;")
        layout.addWidget(title)

        self._bar = _GradientBar()
        self._bar.clicked_at.connect(self._add_stop_at)
        layout.addWidget(self._bar)

        self._stops_container = QVBoxLayout()
        self._stops_container.setSpacing(2)
        layout.addLayout(self._stops_container)

        button_row = QHBoxLayout()
        add_button = QPushButton("Add Stop")
        add_button.setCursor(Qt.CursorShape.PointingHandCursor)
        add_button.clicked.connect(lambda: self._add_stop_at(0.5))
        button_row.addWidget(add_button)
        button_row.addStretch()
        layout.addLayout(button_row)

    # ------------------------------------------------------------- component I/O
    def _read_property(self, prop_name):
        """Read an object/value property (single JSON decode), e.g. a Color."""
        try:
            raw = self.editor_bridge.get_component_property(self.component, prop_name)
            if not raw:
                return None
            return json.loads(raw) if isinstance(raw, str) else raw
        except Exception as e:
            self._log(f"read '{prop_name}' failed: {e}", "Warning")
            return None

    def _read_json_string_property(self, prop_name):
        """Read a String property whose value is itself a JSON string.

        get_component_property returns the JSON-encoded property value; for a
        String property that decodes to the inner string, which is then parsed
        as JSON. Mirrors ShaderAttachmentWidget's shaderParameters handling.
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

    def _color_from_dict(self, data, fallback):
        if isinstance(data, dict):
            return QColor.fromRgbF(
                float(data.get('r', 1.0)),
                float(data.get('g', 1.0)),
                float(data.get('b', 1.0)),
                float(data.get('a', 1.0)))
        return fallback

    def _load_from_component(self):
        gradient = self._read_json_string_property("colorGradient")
        stops = []
        if isinstance(gradient, dict) and isinstance(gradient.get("stops"), list):
            for stop in gradient["stops"]:
                pos = float(stop.get("pos", 0.0))
                color = self._color_from_dict(stop.get("color"), QColor(255, 255, 255, 255))
                stops.append((pos, color))

        if not stops:
            # Seed from the legacy two-stop color so the strip reflects the
            # emitter's current look before the user customizes it.
            start = self._color_from_dict(self._read_property("colorStart"), QColor(255, 255, 255, 255))
            end = self._color_from_dict(self._read_property("colorEnd"), QColor(255, 255, 255, 0))
            stops = [(0.0, start), (1.0, end)]

        self._stops = sorted(stops, key=lambda s: s[0])
        self._rebuild_rows()
        self._refresh_bar()

    def _write_gradient(self):
        payload = {"stops": [
            {
                "pos": pos,
                "color": {"r": color.redF(), "g": color.greenF(),
                          "b": color.blueF(), "a": color.alphaF()},
            }
            for pos, color in self._stops
        ]}
        try:
            self._write_json_string_property("colorGradient", payload)
            if hasattr(self.editor_bridge, 'mark_scene_dirty'):
                self.editor_bridge.mark_scene_dirty()
        except Exception as e:
            self._log(f"write colorGradient failed: {e}", "Warning")
        self._refresh_bar()
        self.value_changed.emit(None)

    # ----------------------------------------------------------------- behaviour
    def _add_stop_at(self, pos):
        color = QColorDialog.getColor(
            QColor(255, 255, 255, 255), self, "Stop Color",
            options=QColorDialog.ColorDialogOption.ShowAlphaChannel)
        if not color.isValid():
            return
        self._stops.append((float(pos), color))
        self._stops.sort(key=lambda s: s[0])
        self._rebuild_rows()
        self._write_gradient()

    def _edit_stop_color(self, index):
        if index < 0 or index >= len(self._stops):
            return
        pos, current = self._stops[index]
        color = QColorDialog.getColor(
            current, self, "Stop Color",
            options=QColorDialog.ColorDialogOption.ShowAlphaChannel)
        if not color.isValid():
            return
        self._stops[index] = (pos, color)
        self._rebuild_rows()
        self._write_gradient()

    def _change_stop_pos(self, index, value):
        if index < 0 or index >= len(self._stops):
            return
        _, color = self._stops[index]
        self._stops[index] = (float(value), color)
        self._stops.sort(key=lambda s: s[0])
        self._rebuild_rows()
        self._write_gradient()

    def _remove_stop(self, index):
        if index < 0 or index >= len(self._stops):
            return
        del self._stops[index]
        self._rebuild_rows()
        self._write_gradient()

    # --------------------------------------------------------------------- views
    def _clear_rows(self):
        while self._stops_container.count():
            item = self._stops_container.takeAt(0)
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
        for index, (pos, color) in enumerate(self._stops):
            row = QHBoxLayout()
            row.setSpacing(4)

            pos_spin = QDoubleSpinBox()
            pos_spin.setRange(0.0, 1.0)
            pos_spin.setSingleStep(0.05)
            pos_spin.setDecimals(3)
            pos_spin.setValue(pos)
            pos_spin.setMaximumWidth(70)
            pos_spin.valueChanged.connect(lambda v, i=index: self._change_stop_pos(i, v))
            row.addWidget(pos_spin)

            swatch = QPushButton()
            swatch.setMinimumHeight(24)
            swatch.setCursor(Qt.CursorShape.PointingHandCursor)
            swatch.setStyleSheet(
                f"QPushButton {{ background-color: rgba({color.red()}, {color.green()}, "
                f"{color.blue()}, {color.alpha()}); border: 1px solid #222; border-radius: 4px; }}")
            swatch.clicked.connect(lambda _checked=False, i=index: self._edit_stop_color(i))
            row.addWidget(swatch, 1)

            remove = QPushButton("✕")
            remove.setMaximumWidth(28)
            remove.setMinimumHeight(24)
            remove.setCursor(Qt.CursorShape.PointingHandCursor)
            remove.setEnabled(len(self._stops) > 1)
            remove.clicked.connect(lambda _checked=False, i=index: self._remove_stop(i))
            row.addWidget(remove)

            self._stops_container.addLayout(row)

    def _refresh_bar(self):
        self._bar.set_stops(self._stops)
