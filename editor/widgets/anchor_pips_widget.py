"""
Anchor Pips + Layout Diagnostics widgets for UIControl-derived components.

Two things the UI layout system had no instrument for at all:

1. AnchorPipsPropertyWidget -- draggable anchor handles. Setting a custom anchor used to mean
   typing raw floats into two Vec2 spinboxes; there were no pips, which is the first thing a
   Godot or Unity user looks for. The four pips here drag in normalized parent space
   (0..1, y measured DOWNWARD from the parent's top edge, matching UIControl's anchor
   convention) and write anchorMin/anchorMax.

2. LayoutDiagnosticsWidget -- a read-only readout of what the solver actually decided: the
   resolved rect, the computed minimum, the desired size, and -- most importantly -- which
   property is really driving each axis. When a control lands in the wrong place, this is
   how you find out why.
"""

from PyQt6.QtWidgets import QWidget, QVBoxLayout, QLabel, QSizePolicy, QGridLayout
from PyQt6.QtCore import Qt, pyqtSignal, QRectF, QPointF
from PyQt6.QtGui import QPainter, QPen, QBrush, QColor, QFont

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent))

# Must match lupine::components::UIControl::AxisDriver.
AXIS_WIDTH_HEIGHT = 0
AXIS_OFFSETS = 1
AXIS_CONTAINER_DRIVEN = 2

_AXIS_DRIVER_TEXT = {
    AXIS_WIDTH_HEIGHT: "width/height",
    AXIS_OFFSETS: "offsets (anchor-stretched)",
    AXIS_CONTAINER_DRIVEN: "the parent container",
}


class _AnchorCanvas(QWidget):
    """The draggable pip surface: a parent-rect box with four anchor handles."""

    anchors_changed = pyqtSignal(float, float, float, float)  # minX, minY, maxX, maxY

    _PIP_RADIUS = 6
    _MARGIN = 14

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumSize(150, 110)
        self.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)
        self.setMouseTracking(True)

        self._anchor_min = [0.0, 0.0]
        self._anchor_max = [0.0, 0.0]
        self._dragging = None  # ('min'|'max', axis_pair) -- which pip is held

    def set_anchors(self, anchor_min, anchor_max):
        self._anchor_min = [float(anchor_min[0]), float(anchor_min[1])]
        self._anchor_max = [float(anchor_max[0]), float(anchor_max[1])]
        self.update()

    def _box(self):
        """The rect representing the PARENT, inset so pips at 0 or 1 stay fully visible."""
        return QRectF(
            self._MARGIN,
            self._MARGIN,
            max(1.0, self.width() - 2 * self._MARGIN),
            max(1.0, self.height() - 2 * self._MARGIN),
        )

    def _to_pixels(self, ax, ay):
        """Normalized anchor -> widget pixels. Anchor y runs DOWNWARD from the parent's top."""
        box = self._box()
        return QPointF(box.left() + ax * box.width(), box.top() + ay * box.height())

    def _to_anchor(self, point):
        box = self._box()
        ax = (point.x() - box.left()) / box.width()
        ay = (point.y() - box.top()) / box.height()
        return (min(1.0, max(0.0, ax)), min(1.0, max(0.0, ay)))

    def _pips(self):
        """The four draggable handles: each corner of the anchor rectangle."""
        return [
            ("min", "min", self._anchor_min[0], self._anchor_min[1]),
            ("max", "min", self._anchor_max[0], self._anchor_min[1]),
            ("min", "max", self._anchor_min[0], self._anchor_max[1]),
            ("max", "max", self._anchor_max[0], self._anchor_max[1]),
        ]

    def mousePressEvent(self, event):
        if event.button() != Qt.MouseButton.LeftButton:
            return
        pos = event.position()
        for x_side, y_side, ax, ay in self._pips():
            center = self._to_pixels(ax, ay)
            if (abs(center.x() - pos.x()) <= self._PIP_RADIUS + 3 and
                    abs(center.y() - pos.y()) <= self._PIP_RADIUS + 3):
                self._dragging = (x_side, y_side)
                return

    def mouseMoveEvent(self, event):
        if not self._dragging:
            return
        x_side, y_side = self._dragging
        ax, ay = self._to_anchor(event.position())

        if x_side == "min":
            self._anchor_min[0] = ax
            # The anchor rect cannot invert: min never passes max.
            self._anchor_max[0] = max(self._anchor_max[0], ax)
        else:
            self._anchor_max[0] = ax
            self._anchor_min[0] = min(self._anchor_min[0], ax)

        if y_side == "min":
            self._anchor_min[1] = ay
            self._anchor_max[1] = max(self._anchor_max[1], ay)
        else:
            self._anchor_max[1] = ay
            self._anchor_min[1] = min(self._anchor_min[1], ay)

        self.update()
        self.anchors_changed.emit(
            self._anchor_min[0], self._anchor_min[1],
            self._anchor_max[0], self._anchor_max[1])

    def mouseReleaseEvent(self, event):
        self._dragging = None

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)

        box = self._box()

        # The parent rect.
        painter.setPen(QPen(QColor(120, 120, 130), 1, Qt.PenStyle.DashLine))
        painter.setBrush(QBrush(QColor(40, 42, 48)))
        painter.drawRect(box)

        # The anchor region: for a point anchor this collapses to a cross, for a stretched
        # one it is the band the control is anchored across.
        top_left = self._to_pixels(self._anchor_min[0], self._anchor_min[1])
        bottom_right = self._to_pixels(self._anchor_max[0], self._anchor_max[1])
        anchor_rect = QRectF(top_left, bottom_right).normalized()

        painter.setPen(QPen(QColor(90, 160, 240), 1))
        painter.setBrush(QBrush(QColor(90, 160, 240, 45)))
        if anchor_rect.width() > 0.5 or anchor_rect.height() > 0.5:
            painter.drawRect(anchor_rect)

        # The pips.
        painter.setPen(QPen(QColor(20, 22, 26), 1))
        painter.setBrush(QBrush(QColor(120, 190, 255)))
        for _x_side, _y_side, ax, ay in self._pips():
            center = self._to_pixels(ax, ay)
            painter.drawEllipse(center, self._PIP_RADIUS, self._PIP_RADIUS)

        # Orientation reminder: anchor y = 0 is the parent's TOP edge, which is the single
        # most misread fact in this coordinate system.
        font = QFont()
        font.setPointSize(7)
        painter.setFont(font)
        painter.setPen(QPen(QColor(140, 140, 150)))
        painter.drawText(QPointF(box.left(), box.top() - 3), "y=0 (top)")

        painter.end()


class AnchorPipsPropertyWidget(QWidget):
    """Drag-to-set anchors, bound to the `anchorMin` property row.

    Emits the new anchorMin as its own value_changed; the paired anchorMax is written
    through the `sibling_changed` signal, which the inspector routes to the same component.
    """

    value_changed = pyqtSignal(object)                 # anchorMin  {x, y}
    sibling_changed = pyqtSignal(str, object)          # ("anchorMax", {x, y})

    def __init__(self, property_name, default_value=None, parent=None):
        super().__init__(parent)
        self.property_name = property_name
        self.default_value = default_value or {"x": 0.0, "y": 0.0}
        self._anchor_min = dict(self.default_value)
        self._anchor_max = {"x": 0.0, "y": 0.0}

        root = QVBoxLayout()
        root.setContentsMargins(0, 2, 0, 2)
        root.setSpacing(3)

        title = QLabel("Anchors")
        title.setToolTip(
            "Drag the pips to set anchorMin / anchorMax.\n"
            "Anchors are fractions of the PARENT rect: (0,0) is its top-left, (1,1) its "
            "bottom-right.\n"
            "When the two anchors differ on an axis, the control is STRETCHED across that "
            "span and its size comes from the offsets, not from width/height.")
        root.addWidget(title)

        self.canvas = _AnchorCanvas()
        self.canvas.anchors_changed.connect(self._on_canvas_changed)
        root.addWidget(self.canvas)

        self.readout = QLabel("")
        self.readout.setStyleSheet("color: palette(mid); font-size: 10px;")
        root.addWidget(self.readout)

        self.setLayout(root)
        self._refresh()

    @staticmethod
    def _as_xy(value, fallback=(0.0, 0.0)):
        if isinstance(value, dict):
            return (float(value.get("x", fallback[0])), float(value.get("y", fallback[1])))
        if isinstance(value, (list, tuple)) and len(value) >= 2:
            return (float(value[0]), float(value[1]))
        return fallback

    def set_value(self, value):
        x, y = self._as_xy(value)
        self._anchor_min = {"x": x, "y": y}
        self._refresh()

    def set_anchor_max(self, value):
        """Called by the inspector so the widget knows the paired anchorMax."""
        x, y = self._as_xy(value)
        self._anchor_max = {"x": x, "y": y}
        self._refresh()

    def get_value(self):
        return dict(self._anchor_min)

    def _refresh(self):
        self.canvas.set_anchors(
            (self._anchor_min["x"], self._anchor_min["y"]),
            (self._anchor_max["x"], self._anchor_max["y"]))
        self.readout.setText(
            f"min ({self._anchor_min['x']:.2f}, {self._anchor_min['y']:.2f})   "
            f"max ({self._anchor_max['x']:.2f}, {self._anchor_max['y']:.2f})")

    def _on_canvas_changed(self, min_x, min_y, max_x, max_y):
        self._anchor_min = {"x": min_x, "y": min_y}
        self._anchor_max = {"x": max_x, "y": max_y}
        self.readout.setText(
            f"min ({min_x:.2f}, {min_y:.2f})   max ({max_x:.2f}, {max_y:.2f})")
        self.value_changed.emit(dict(self._anchor_min))
        self.sibling_changed.emit("anchorMax", dict(self._anchor_max))

    def reset_to_default(self):
        self.set_value(self.default_value)
        self.value_changed.emit(dict(self._anchor_min))


class LayoutDiagnosticsWidget(QWidget):
    """Read-only: what the layout solver actually decided, and what drives each axis.

    This is the instrument that did not exist. Nothing in the renderer, the editor bridge or
    the editor drew the resolved rect, the computed minimum, or the driving property -- so
    when a control landed somewhere unexpected there was no way to find out why.
    """

    def __init__(self, component, parent=None):
        super().__init__(parent)
        self._component = component

        root = QVBoxLayout()
        root.setContentsMargins(0, 2, 0, 2)
        root.setSpacing(2)

        title = QLabel("Layout Diagnostics")
        root.addWidget(title)

        self._grid = QGridLayout()
        self._grid.setSpacing(2)
        self._rows = {}
        for row, key in enumerate(
                ["Resolved rect", "Desired size", "Minimum size",
                 "Width driven by", "Height driven by"]):
            name = QLabel(key)
            name.setStyleSheet("color: palette(mid); font-size: 10px;")
            value = QLabel("-")
            value.setStyleSheet("font-size: 10px;")
            value.setTextInteractionFlags(Qt.TextInteractionFlag.TextSelectableByMouse)
            self._grid.addWidget(name, row, 0)
            self._grid.addWidget(value, row, 1)
            self._rows[key] = value
        root.addLayout(self._grid)

        self.setLayout(root)
        self.refresh()

    def refresh(self):
        component = self._component
        if component is None or not hasattr(component, "get_axis_driver"):
            return

        try:
            rect = component.get_rect()
            desired = component.get_desired_size()
            minimum = component.get_min_size()
            driver_h = component.get_axis_driver(False)
            driver_v = component.get_axis_driver(True)
        except Exception:
            return

        self._rows["Resolved rect"].setText(
            f"x {rect['x']:.0f}  y {rect['y']:.0f}   {rect['width']:.0f} x {rect['height']:.0f}")
        self._rows["Desired size"].setText(f"{desired[0]:.0f} x {desired[1]:.0f}")
        self._rows["Minimum size"].setText(f"{minimum[0]:.0f} x {minimum[1]:.0f}")
        self._rows["Width driven by"].setText(_AXIS_DRIVER_TEXT.get(driver_h, "?"))
        self._rows["Height driven by"].setText(_AXIS_DRIVER_TEXT.get(driver_v, "?"))
