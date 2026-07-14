"""
Profiler Panel

Visual profiler for the running game: a summary strip of frame statistics, a
scrolling frame-time graph, a per-system timing breakdown, a flame graph of the
selected frame's nested zones, and a filterable counters/memory tree. Data is
polled from the live runtime (lupine_runtime) which owns the populated
profiling::Profiler singleton; it crosses the binding as a JSON string parsed
here with the standard library.
"""

import json
import shutil
from typing import Optional, List, Dict, Any, Tuple

from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel, QPushButton,
                             QCheckBox, QSpinBox, QHeaderView, QSizePolicy, QFileDialog,
                             QScrollArea, QFrame, QTabWidget, QLineEdit, QTreeWidget,
                             QTreeWidgetItem)
from PyQt6.QtCore import Qt, QTimer, QRectF, pyqtSignal
from PyQt6.QtGui import QPainter, QColor, QPen, QFont

from panels.base_panel import EditorPanel

try:
    import lupine_runtime as lr
except ImportError:
    lr = None


# Stable color per zone category, used by both the breakdown bar and the flame graph.
CATEGORY_COLORS = {
    "Input":      QColor(0x4F, 0xC3, 0xF7),
    "Update":     QColor(0x81, 0xC7, 0x84),
    "Physics":    QColor(0xFF, 0xB7, 0x4D),
    "Render":     QColor(0xBA, 0x68, 0xC8),
    "Scripting":  QColor(0xFF, 0xD5, 0x4F),
    "Networking": QColor(0x4D, 0xB6, 0xAC),
    "Audio":      QColor(0xF0, 0x62, 0x92),
    "Asset":      QColor(0xA1, 0x88, 0x7F),
    "GPU":        QColor(0xE5, 0x73, 0x73),
    "User":       QColor(0x90, 0xA4, 0xAE),
    "Unknown":    QColor(0x60, 0x60, 0x60),
}

# Common surface colors so the painted widgets read as one panel.
BG_PANEL = QColor(0x1E, 0x1E, 0x1E)
BG_SUNKEN = QColor(0x17, 0x17, 0x17)
GRID_LINE = QColor(0xFF, 0xFF, 0xFF, 0x14)
TEXT_DIM = QColor(0x9A, 0xA0, 0xA6)
TEXT_BRIGHT = QColor(0xE6, 0xE6, 0xE6)

# Frame-time thresholds (ms) for the good / warn / bad coloring.
MS_GOOD = 16.7    # 60 FPS
MS_WARN = 33.4    # 30 FPS


def _category_color(name: str) -> QColor:
    return CATEGORY_COLORS.get(name, CATEGORY_COLORS["Unknown"])


def _readable_text_color(bg: QColor) -> QColor:
    """Pick black or white text for legibility over an arbitrary fill color."""
    luminance = (0.299 * bg.red() + 0.587 * bg.green() + 0.114 * bg.blue())
    return QColor(0x10, 0x10, 0x10) if luminance > 140 else QColor(0xF5, 0xF5, 0xF5)


def _ms_color(ms: float) -> QColor:
    if ms <= MS_GOOD:
        return QColor(0x66, 0xBB, 0x6A)
    if ms <= MS_WARN:
        return QColor(0xFF, 0xCA, 0x28)
    return QColor(0xEF, 0x53, 0x50)


def _percentile(sorted_values: List[float], pct: float) -> float:
    if not sorted_values:
        return 0.0
    if len(sorted_values) == 1:
        return sorted_values[0]
    rank = pct / 100.0 * (len(sorted_values) - 1)
    lo = int(rank)
    hi = min(lo + 1, len(sorted_values) - 1)
    frac = rank - lo
    return sorted_values[lo] * (1.0 - frac) + sorted_values[hi] * frac


class FrameTimeGraph(QWidget):
    """Scrolling area chart of recent frame times with 60/30 FPS guide lines,
    a hover crosshair and a pinned-frame marker."""

    frame_picked = pyqtSignal(int)  # index into the currently displayed frame list

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumHeight(110)
        self.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Preferred)
        self._frames: List[float] = []
        self._selected: int = -1
        self._hover: int = -1
        self.setMouseTracking(True)

    def set_frames(self, frame_ms: List[float]) -> None:
        self._frames = frame_ms
        if self._selected >= len(self._frames):
            self._selected = -1
        self.update()

    def set_selected(self, index: int) -> None:
        self._selected = index
        self.update()

    def _index_at(self, px: float) -> int:
        if not self._frames:
            return -1
        w = max(self.width(), 1)
        idx = int(px / w * len(self._frames))
        return max(0, min(idx, len(self._frames) - 1))

    def mousePressEvent(self, event) -> None:
        idx = self._index_at(event.position().x())
        if idx < 0:
            return
        self._selected = idx
        self.frame_picked.emit(idx)
        self.update()

    def mouseMoveEvent(self, event) -> None:
        idx = self._index_at(event.position().x())
        if idx != self._hover:
            self._hover = idx
            if 0 <= idx < len(self._frames):
                self.setToolTip(f"Frame -{len(self._frames) - 1 - idx}    "
                                f"{self._frames[idx]:.2f} ms    "
                                f"{(1000.0 / self._frames[idx]) if self._frames[idx] > 0 else 0:.0f} FPS")
            self.update()

    def leaveEvent(self, event) -> None:
        self._hover = -1
        self.update()

    def paintEvent(self, event) -> None:
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        rect = self.rect()
        painter.fillRect(rect, BG_SUNKEN)

        if not self._frames:
            painter.setPen(TEXT_DIM)
            painter.drawText(rect, Qt.AlignmentFlag.AlignCenter, "No profiler data — Play the game")
            return

        w = rect.width()
        h = rect.height()
        # Scale so 33.3ms (30 FPS) sits comfortably; grow if frames are slower.
        peak = max(max(self._frames), 33.4)
        scale = (h - 8) / peak

        # Guide lines at 16.6ms (60 FPS) and 33.3ms (30 FPS).
        painter.setFont(QFont("", 7))
        for ms, color, label in ((16.667, QColor(0x4C, 0x8E, 0x4C), "60 FPS"),
                                 (33.333, QColor(0x9E, 0x7A, 0x3C), "30 FPS")):
            if ms > peak:
                continue
            y = h - ms * scale
            painter.setPen(QPen(color, 1, Qt.PenStyle.DashLine))
            painter.drawLine(0, int(y), w, int(y))
            painter.setPen(color)
            painter.drawText(3, int(y) - 2, label)

        n = len(self._frames)
        bar_w = w / n
        for i, ms in enumerate(self._frames):
            x = i * bar_w
            bar_h = ms * scale
            col = _ms_color(ms)
            if i == self._selected:
                col = col.lighter(150)
            elif i == self._hover:
                col = col.lighter(125)
            painter.fillRect(QRectF(x, h - bar_h, max(bar_w - 0.5, 0.5), bar_h), col)

        # Pinned-frame marker.
        if 0 <= self._selected < n:
            mx = self._selected * bar_w + bar_w * 0.5
            painter.setPen(QPen(QColor(0xFF, 0xFF, 0xFF, 0xB0), 1, Qt.PenStyle.DotLine))
            painter.drawLine(int(mx), 0, int(mx), h)


class CategoryBar(QWidget):
    """Horizontal stacked bar of average ms per zone category for the window,
    with a wrapping legend showing ms and percentage per category."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumHeight(56)
        self.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.MinimumExpanding)
        self._totals: Dict[str, float] = {}

    def set_totals(self, totals: Dict[str, float]) -> None:
        self._totals = totals
        # Grow if the wrapping legend needs more than one row.
        self.updateGeometry()
        self.update()

    def paintEvent(self, event) -> None:
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        rect = self.rect()
        painter.fillRect(rect, BG_SUNKEN)
        total = sum(self._totals.values())
        if total <= 0.0:
            painter.setPen(TEXT_DIM)
            painter.drawText(rect, Qt.AlignmentFlag.AlignCenter, "No category data")
            return

        bar_rect = rect.adjusted(8, 8, -8, 0)
        bar_h = 18
        x = float(bar_rect.x())
        ordered = sorted(self._totals.items(), key=lambda kv: kv[1], reverse=True)
        for name, ms in ordered:
            frac = ms / total
            seg_w = frac * bar_rect.width()
            seg = QRectF(x, bar_rect.y(), seg_w, bar_h)
            painter.fillRect(seg, _category_color(name))
            x += seg_w

        # Wrapping legend row(s) underneath.
        painter.setFont(QFont("", 8))
        fm = painter.fontMetrics()
        lx = 8.0
        ly = bar_rect.y() + bar_h + 6 + fm.ascent()
        line_h = fm.height() + 3
        for name, ms in ordered:
            pct = ms / total * 100.0
            text = f"{name} {ms:.2f}ms ({pct:.0f}%)"
            chip_w = 10 + 4 + fm.horizontalAdvance(text) + 14
            if lx + chip_w > rect.width() - 4 and lx > 8.0:
                lx = 8.0
                ly += line_h
            painter.fillRect(QRectF(lx, ly - fm.ascent() + 1, 9, 9), _category_color(name))
            painter.setPen(QColor(0xCF, 0xCF, 0xCF))
            painter.drawText(int(lx) + 13, int(ly), text)
            lx += chip_w

    def sizeHint(self):
        from PyQt6.QtCore import QSize
        # Estimate legend rows so the scroll area reserves enough height.
        rows = max(1, (len(self._totals) + 2) // 3)
        return QSize(200, 8 + 18 + 6 + rows * 16 + 6)

    def minimumSizeHint(self):
        return self.sizeHint()


class FlameGraph(QWidget):
    """Nested-zone flame graph for a single frame, laid out by depth and duration,
    with hover highlighting and a duration scale."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumHeight(90)
        self.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Preferred)
        self._zones: List[Dict[str, Any]] = []
        self._frame_ms: float = 0.0
        self._hover_zone: int = -1
        self.setMouseTracking(True)

    def set_frame(self, frame: Optional[Dict[str, Any]]) -> None:
        if frame is None:
            self._zones = []
            self._frame_ms = 0.0
        else:
            self._zones = frame.get("zones", [])
            self._frame_ms = float(frame.get("frameMs", 0.0))
        self._hover_zone = -1
        # Grow to fit the deepest nesting so the scroll area can reach every row.
        max_depth = 0
        for z in self._zones:
            max_depth = max(max_depth, int(z.get("depth", 0)))
        self.setMinimumHeight(max(int((max_depth + 1) * self._row_height()) + 6, 90))
        self.update()

    def mouseMoveEvent(self, event) -> None:
        idx = self._zone_index_at(event.position().x(), event.position().y())
        if idx != self._hover_zone:
            self._hover_zone = idx
            self.setToolTip(self._tooltip_for(idx))
            self.update()

    def leaveEvent(self, event) -> None:
        if self._hover_zone != -1:
            self._hover_zone = -1
            self.update()

    def _row_height(self) -> float:
        return 19.0

    def _zone_index_at(self, px: float, py: float) -> int:
        span_ns = self._frame_ms * 1.0e6
        if span_ns <= 0 or not self._zones:
            return -1
        w = max(self.width(), 1)
        for i, z in enumerate(self._zones):
            depth = int(z.get("depth", 0))
            y = depth * self._row_height()
            if not (y <= py <= y + self._row_height()):
                continue
            x = z.get("startNs", 0) / span_ns * w
            zw = max(z.get("durNs", 0) / span_ns * w, 1.0)
            if x <= px <= x + zw:
                return i
        return -1

    def _tooltip_for(self, idx: int) -> str:
        span_ns = self._frame_ms * 1.0e6
        if idx < 0 or idx >= len(self._zones) or span_ns <= 0:
            return ""
        z = self._zones[idx]
        ms = z.get("durNs", 0) / 1.0e6
        pct = z.get("durNs", 0) / span_ns * 100.0
        return f"{z.get('name', '?')} — {ms:.3f} ms ({pct:.1f}%)  [{z.get('category', '')}]"

    def paintEvent(self, event) -> None:
        painter = QPainter(self)
        rect = self.rect()
        painter.fillRect(rect, BG_SUNKEN)
        span_ns = self._frame_ms * 1.0e6
        if span_ns <= 0 or not self._zones:
            painter.setPen(TEXT_DIM)
            painter.drawText(rect, Qt.AlignmentFlag.AlignCenter,
                             "Click a frame in the graph above to inspect its zones")
            return

        w = rect.width()
        rh = self._row_height()
        painter.setFont(QFont("", 8))
        fm = painter.fontMetrics()
        for i, z in enumerate(self._zones):
            depth = int(z.get("depth", 0))
            x = z.get("startNs", 0) / span_ns * w
            zw = max(z.get("durNs", 0) / span_ns * w, 1.0)
            y = depth * rh
            cell = QRectF(x, y, zw, rh - 1.0)
            fill = _category_color(z.get("category", "Unknown"))
            if i == self._hover_zone:
                fill = fill.lighter(135)
            painter.fillRect(cell, fill)
            painter.setPen(QColor(0, 0, 0, 0x55))
            painter.drawRect(cell)
            if zw > 30:
                painter.setPen(_readable_text_color(fill))
                name = z.get("name", "")
                ms = z.get("durNs", 0) / 1.0e6
                label = f"{name}  {ms:.2f}ms" if zw > 90 else name
                label = fm.elidedText(label, Qt.TextElideMode.ElideRight, int(zw) - 6)
                painter.drawText(cell.adjusted(4, 0, -2, 0),
                                 Qt.AlignmentFlag.AlignVCenter | Qt.AlignmentFlag.AlignLeft, label)


class _StatChip(QWidget):
    """A small labeled value cell for the summary strip."""

    def __init__(self, caption: str, parent=None):
        super().__init__(parent)
        layout = QVBoxLayout(self)
        layout.setContentsMargins(10, 4, 10, 4)
        layout.setSpacing(0)
        self._value = QLabel("—")
        self._value.setStyleSheet("color: #e6e6e6; font-size: 15px; font-weight: 700;")
        cap = QLabel(caption)
        cap.setStyleSheet("color: #8a9096; font-size: 9px; text-transform: uppercase;")
        layout.addWidget(self._value)
        layout.addWidget(cap)

    def set_value(self, text: str, color: Optional[str] = None) -> None:
        self._value.setText(text)
        style = "font-size: 15px; font-weight: 700;"
        self._value.setStyleSheet(f"color: {color or '#e6e6e6'}; {style}")


class _ProfilerView(QWidget):
    """
    One source's visualization: summary strip, frame-time graph, system
    breakdown, flame graph and counters/memory tree. A panel hosts one of these
    per profiler source (the in-process game, or each out-of-process instance)
    inside a tab.
    """

    def __init__(self, source_index: int, source_path: Optional[str], parent=None):
        super().__init__(parent)
        # source_path is None for the editor's in-process runtime (read live via
        # the profiler API); otherwise it is the JSON dump an instance streams to.
        self.source_index = source_index
        self.source_path = source_path
        self._frames: List[Dict[str, Any]] = []
        # Frame currently shown in the flame graph/table. None => follow live (last).
        self._pinned_frame: Optional[Dict[str, Any]] = None
        self._filter_text: str = ""
        # Row structure currently materialized in the counters tree; used to decide
        # between an in-place value update and a full rebuild.
        self._tree_signature: Optional[Tuple] = None
        self._build()

    def _build(self) -> None:
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.Shape.NoFrame)
        scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)

        body = QWidget()
        body_layout = QVBoxLayout(body)
        body_layout.setContentsMargins(8, 6, 8, 8)
        body_layout.setSpacing(8)

        # --- summary strip ---
        strip = QFrame()
        strip.setStyleSheet("QFrame { background-color: #232323; border-radius: 5px; }")
        strip_layout = QHBoxLayout(strip)
        strip_layout.setContentsMargins(4, 4, 4, 4)
        strip_layout.setSpacing(2)
        self._chip_fps = _StatChip("FPS")
        self._chip_cur = _StatChip("Frame")
        self._chip_avg = _StatChip("Avg")
        self._chip_min = _StatChip("Min")
        self._chip_max = _StatChip("Max")
        self._chip_p99 = _StatChip("P99")
        for chip in (self._chip_fps, self._chip_cur, self._chip_avg,
                     self._chip_min, self._chip_max, self._chip_p99):
            strip_layout.addWidget(chip)
        strip_layout.addStretch()
        body_layout.addWidget(strip)

        body_layout.addWidget(_section_label("Frame Time (click to pin a frame)"))
        self._graph = FrameTimeGraph()
        self._graph.frame_picked.connect(self._on_frame_picked)
        body_layout.addWidget(self._graph)

        body_layout.addWidget(_section_label("System Breakdown (avg ms / frame)"))
        self._category_bar = CategoryBar()
        body_layout.addWidget(self._category_bar)

        # --- flame header with live/pinned state ---
        flame_header = QHBoxLayout()
        flame_header.addWidget(_section_label("Frame Zones"))
        flame_header.addStretch()
        self._frame_state = QLabel("Live")
        self._frame_state.setStyleSheet("color: #66bb6a; font-size: 10px; font-weight: 600;")
        flame_header.addWidget(self._frame_state)
        self._live_btn = QPushButton("Follow Live")
        self._live_btn.setFixedHeight(20)
        self._live_btn.setVisible(False)
        self._live_btn.clicked.connect(self._resume_live)
        flame_header.addWidget(self._live_btn)
        body_layout.addLayout(flame_header)

        self._flame = FlameGraph()
        body_layout.addWidget(self._flame)

        # --- counters & memory with filter ---
        ct_header = QHBoxLayout()
        ct_header.addWidget(_section_label("Counters & Memory"))
        ct_header.addStretch()
        self._filter_edit = QLineEdit()
        self._filter_edit.setPlaceholderText("Filter…")
        self._filter_edit.setClearButtonEnabled(True)
        self._filter_edit.setFixedWidth(160)
        self._filter_edit.textChanged.connect(self._on_filter_changed)
        ct_header.addWidget(self._filter_edit)
        body_layout.addLayout(ct_header)

        self._tree = QTreeWidget()
        self._tree.setColumnCount(2)
        self._tree.setHeaderLabels(["Counter / Memory", "Value"])
        self._tree.header().setSectionResizeMode(0, QHeaderView.ResizeMode.Stretch)
        self._tree.header().setSectionResizeMode(1, QHeaderView.ResizeMode.ResizeToContents)
        self._tree.setRootIsDecorated(True)
        self._tree.setAlternatingRowColors(True)
        self._tree.setMinimumHeight(120)
        self._tree.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Minimum)
        body_layout.addWidget(self._tree)

        body_layout.addStretch()
        scroll.setWidget(body)

        outer = QVBoxLayout(self)
        outer.setContentsMargins(0, 0, 0, 0)
        outer.addWidget(scroll)

    # --- data flow ---

    def update_doc(self, doc: Dict[str, Any], fps: Optional[float]) -> None:
        # ToJson() emits frames oldest-first, which is the order the graph wants.
        self._frames = doc.get("frames", [])
        frame_ms_list = [float(f.get("frameMs", 0.0)) for f in self._frames]
        self._graph.set_frames(frame_ms_list)
        self._update_stats(frame_ms_list, fps)
        self._category_bar.set_totals(self._aggregate_categories())

        # Show the pinned frame if one is held; otherwise follow the latest.
        shown = self._pinned_frame if self._pinned_frame is not None else (
            self._frames[-1] if self._frames else None)
        self._flame.set_frame(shown)
        self._populate_tree(shown)

    def clear(self) -> None:
        self._frames = []
        self._pinned_frame = None
        self._graph.set_frames([])
        self._graph.set_selected(-1)
        self._category_bar.set_totals({})
        self._flame.set_frame(None)
        self._tree.clear()
        self._tree_signature = None
        self._set_live_state(True)
        for chip, cap in ((self._chip_fps, "—"), (self._chip_cur, "—"),
                          (self._chip_avg, "—"), (self._chip_min, "—"),
                          (self._chip_max, "—"), (self._chip_p99, "—")):
            chip.set_value(cap)

    def save_capture(self, path: str) -> bool:
        """Persist this source's current data to a .lprof file."""
        if self.source_path is None:
            if lr is not None and hasattr(lr, "profiler_save_capture"):
                try:
                    lr.profiler_save_capture(path)
                    return True
                except Exception:
                    return False
            return False
        try:
            shutil.copyfile(self.source_path, path)
            return True
        except OSError:
            return False

    # --- stats ---

    def _update_stats(self, frame_ms_list: List[float], fps: Optional[float]) -> None:
        if not frame_ms_list:
            return
        cur = frame_ms_list[-1]
        ordered = sorted(frame_ms_list)
        avg = sum(frame_ms_list) / len(frame_ms_list)
        lo = ordered[0]
        hi = ordered[-1]
        p99 = _percentile(ordered, 99.0)
        if fps is None or fps <= 0.0:
            fps = 1000.0 / avg if avg > 0.0 else 0.0
        self._chip_fps.set_value(f"{fps:.0f}", _ms_color(1000.0 / fps if fps > 0 else 999).name())
        self._chip_cur.set_value(f"{cur:.2f} ms", _ms_color(cur).name())
        self._chip_avg.set_value(f"{avg:.2f} ms")
        self._chip_min.set_value(f"{lo:.2f} ms")
        self._chip_max.set_value(f"{hi:.2f} ms", _ms_color(hi).name())
        self._chip_p99.set_value(f"{p99:.2f} ms", _ms_color(p99).name())

    # --- frame pin/live ---

    def _on_frame_picked(self, index: int) -> None:
        # Graph displays oldest-first; pin a *copy* so ring-buffer scrolling does
        # not drift the inspected frame out from under the user.
        if 0 <= index < len(self._frames):
            self._pinned_frame = self._frames[index]
            self._flame.set_frame(self._pinned_frame)
            self._populate_tree(self._pinned_frame)
            self._set_live_state(False)

    def _resume_live(self) -> None:
        self._pinned_frame = None
        self._graph.set_selected(-1)
        self._set_live_state(True)
        shown = self._frames[-1] if self._frames else None
        self._flame.set_frame(shown)
        self._populate_tree(shown)

    def _set_live_state(self, live: bool) -> None:
        if live:
            self._frame_state.setText("Live")
            self._frame_state.setStyleSheet("color: #66bb6a; font-size: 10px; font-weight: 600;")
            self._live_btn.setVisible(False)
        else:
            self._frame_state.setText("Pinned")
            self._frame_state.setStyleSheet("color: #ffca28; font-size: 10px; font-weight: 600;")
            self._live_btn.setVisible(True)

    def _aggregate_categories(self) -> Dict[str, float]:
        totals: Dict[str, float] = {}
        if not self._frames:
            return totals
        for frame in self._frames:
            for z in frame.get("zones", []):
                # Only top-level zones to avoid double-counting nested time.
                if int(z.get("depth", 0)) != 0:
                    continue
                cat = z.get("category", "Unknown")
                totals[cat] = totals.get(cat, 0.0) + z.get("durNs", 0) / 1.0e6
        n = float(len(self._frames))
        return {k: v / n for k, v in totals.items()}

    # --- counters/memory tree ---

    def _on_filter_changed(self, text: str) -> None:
        self._filter_text = text.strip().lower()
        shown = self._pinned_frame if self._pinned_frame is not None else (
            self._frames[-1] if self._frames else None)
        self._populate_tree(shown)

    def _passes_filter(self, name: str) -> bool:
        return not self._filter_text or self._filter_text in name.lower()

    def _compute_tree_groups(self, frame: Optional[Dict[str, Any]]) -> List[Tuple[str, List[Tuple[str, str]]]]:
        """Build the ordered (group, rows) structure shown in the counters tree."""
        if frame is None:
            return []
        counters = frame.get("counters", {})
        memory = frame.get("memory", {})

        # Bucket counters by their dotted prefix so related metrics group together.
        buckets: Dict[str, List[Tuple[str, str]]] = {
            "Components": [], "GPU": [], "General": [], "Memory": [],
        }
        for name in sorted(counters.keys()):
            if not self._passes_filter(name):
                continue
            value = _format_counter(name, counters[name])
            if name.startswith("comp."):
                buckets["Components"].append((name, value))
            elif name.startswith("gpu."):
                buckets["GPU"].append((name, value))
            else:
                buckets["General"].append((name, value))
        for name in sorted(memory.keys()):
            if not self._passes_filter(name):
                continue
            raw = int(memory[name])
            text = _format_bytes(raw) if "bytes" in name.lower() else f"{raw:,}"
            buckets["Memory"].append((name, text))

        ordered: List[Tuple[str, List[Tuple[str, str]]]] = []
        for group_name in ("General", "Components", "GPU", "Memory"):
            rows = buckets[group_name]
            if rows:
                ordered.append((group_name, rows))
        return ordered

    def _populate_tree(self, frame: Optional[Dict[str, Any]]) -> None:
        groups = self._compute_tree_groups(frame)
        # Signature = the row structure (group names + ordered counter names).
        # While it is unchanged we update values in place instead of clearing and
        # rebuilding, which would reset the tree's scroll position on every poll
        # and make the section impossible to scroll.
        signature = tuple((name, tuple(n for n, _ in rows)) for name, rows in groups)
        if signature == self._tree_signature:
            for gi, (_group_name, rows) in enumerate(groups):
                parent = self._tree.topLevelItem(gi)
                if parent is None:
                    continue
                for ci, (_name, value) in enumerate(rows):
                    child = parent.child(ci)
                    if child is not None and child.text(1) != value:
                        child.setText(1, value)
            return

        self._tree_signature = signature
        self._tree.clear()
        for group_name, rows in groups:
            parent = QTreeWidgetItem(self._tree, [f"{group_name}  ({len(rows)})", ""])
            font = parent.font(0)
            font.setBold(True)
            parent.setFont(0, font)
            parent.setForeground(0, TEXT_DIM)
            parent.setFirstColumnSpanned(False)
            for name, value in rows:
                child = QTreeWidgetItem(parent, [name, value])
                child.setTextAlignment(1, Qt.AlignmentFlag.AlignRight | Qt.AlignmentFlag.AlignVCenter)
                child.setForeground(1, TEXT_BRIGHT)
            parent.setExpanded(True)


def _section_label(text: str) -> QLabel:
    label = QLabel(text)
    label.setStyleSheet("color: #9aa0a6; font-size: 10px; font-weight: 600; "
                        "text-transform: uppercase; padding-top: 2px;")
    return label


class ProfilerPanel(EditorPanel):
    """
    Editor dock that visualizes live profiler data from the running game.

    Shows one tab per profiler source: the single in-process game, or — when a
    multi-instance (networking) play session is active — one tab per spawned
    instance, each fed from that instance's streamed JSON dump.
    """

    def __init__(self, parent=None):
        super().__init__("Profiler", parent)
        self._paused = False
        self._enable_synced = False
        # Active views keyed by source index; tabs are rebuilt as sources change.
        self._views: Dict[int, _ProfilerView] = {}

        self._timer = QTimer(self)
        self._timer.setInterval(100)  # ~10 Hz
        self._timer.timeout.connect(self._poll)
        self._timer.start()

    def _setup_panel(self) -> None:
        layout = self.content_layout

        # --- top control bar ---
        bar = QHBoxLayout()
        bar.setContentsMargins(6, 4, 6, 4)
        bar.setSpacing(8)

        self._enable_cb = QCheckBox("Enabled")
        self._enable_cb.setChecked(True)
        self._enable_cb.toggled.connect(self._on_enable_toggled)
        bar.addWidget(self._enable_cb)

        self._pause_btn = QPushButton("Pause")
        self._pause_btn.setCheckable(True)
        self._pause_btn.toggled.connect(self._on_pause_toggled)
        bar.addWidget(self._pause_btn)

        clear_btn = QPushButton("Clear")
        clear_btn.clicked.connect(self._on_clear)
        bar.addWidget(clear_btn)

        bar.addWidget(QLabel("History:"))
        self._history_spin = QSpinBox()
        self._history_spin.setRange(30, 2000)
        self._history_spin.setValue(300)
        self._history_spin.setSuffix(" frames")
        self._history_spin.valueChanged.connect(self._on_history_changed)
        bar.addWidget(self._history_spin)

        save_btn = QPushButton("Save .lprof")
        save_btn.clicked.connect(self._on_save)
        bar.addWidget(save_btn)

        bar.addStretch()

        bar_widget = QWidget()
        bar_widget.setLayout(bar)
        bar_widget.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)
        layout.addWidget(bar_widget)

        # --- one tab per profiler source ---
        self._tabs = QTabWidget()
        self._tabs.setDocumentMode(True)
        layout.addWidget(self._tabs)

        # Keep the dock genuinely shrinkable when tabbed beside the console.
        self.setMinimumHeight(0)
        self.content_widget.setMinimumHeight(0)

    # --- runtime availability helpers ---

    @staticmethod
    def _runtime_ready() -> bool:
        return lr is not None and hasattr(lr, "profiler_get_history_json")

    def _current_view(self) -> Optional[_ProfilerView]:
        widget = self._tabs.currentWidget()
        return widget if isinstance(widget, _ProfilerView) else None

    # --- control handlers ---

    def _on_enable_toggled(self, enabled: bool) -> None:
        if self._runtime_ready():
            try:
                lr.profiler_set_enabled(enabled)
            except Exception:
                pass

    def _on_pause_toggled(self, paused: bool) -> None:
        self._paused = paused
        self._pause_btn.setText("Resume" if paused else "Pause")

    def _on_clear(self) -> None:
        # Clearing the engine ring buffer is only possible for the in-process
        # source; for instance dumps we just clear the panel's current view.
        view = self._current_view()
        if view is not None and view.source_path is None and self._runtime_ready():
            try:
                lr.profiler_clear()
            except Exception:
                pass
        if view is not None:
            view.clear()

    def _on_history_changed(self, value: int) -> None:
        if self._runtime_ready():
            try:
                lr.profiler_set_history_size(value)
            except Exception:
                pass

    def _on_save(self) -> None:
        view = self._current_view()
        if view is None:
            return
        path, _ = QFileDialog.getSaveFileName(self, "Save Profiler Capture", "capture.lprof",
                                              "Profiler Capture (*.lprof);;All Files (*)")
        if path:
            view.save_capture(path)

    # --- polling ---

    def _poll(self) -> None:
        try:
            from runtime_controller import get_profiler_sources
            sources = get_profiler_sources()
        except Exception:
            sources = []
        # Always keep at least the in-process "Game" tab so the panel shows its
        # idle "Play the game" hint and stays usable when nothing is running.
        if not sources:
            sources = [{"index": 0, "label": "Game", "path": None}]

        self._sync_tabs(sources)

        if not self._runtime_ready():
            self._enable_synced = False
        elif not self._enable_synced:
            # Push the panel's enable/history state once the runtime appears, since
            # the engine-side profiler starts disabled and is recreated each play.
            try:
                lr.profiler_set_enabled(self._enable_cb.isChecked())
                lr.profiler_set_history_size(self._history_spin.value())
                self._enable_synced = True
            except Exception:
                pass

        if self._paused:
            return

        for source in sources:
            view = self._views.get(source["index"])
            if view is None:
                continue
            if source["path"] is None:
                self._update_inprocess(view)
            else:
                self._update_from_file(view, source["path"])

    def _sync_tabs(self, sources: List[Dict[str, Any]]) -> None:
        """Add/remove/relabel tabs so they mirror the active source list."""
        wanted = {s["index"]: s for s in sources}

        for index in list(self._views.keys()):
            if index not in wanted:
                view = self._views.pop(index)
                pos = self._tabs.indexOf(view)
                if pos >= 0:
                    self._tabs.removeTab(pos)
                view.deleteLater()

        for index in sorted(wanted.keys()):
            source = wanted[index]
            view = self._views.get(index)
            if view is None:
                view = _ProfilerView(index, source["path"])
                self._views[index] = view
                self._tabs.addTab(view, source["label"])
            else:
                view.source_path = source["path"]
                pos = self._tabs.indexOf(view)
                if pos >= 0 and self._tabs.tabText(pos) != source["label"]:
                    self._tabs.setTabText(pos, source["label"])

        # The single-source case reads cleaner without a lone tab bar.
        self._tabs.tabBar().setVisible(len(self._views) > 1)

    def _update_inprocess(self, view: _ProfilerView) -> None:
        if not self._runtime_ready():
            return
        try:
            raw = lr.profiler_get_history_json()
            fps = lr.profiler_fps()
        except Exception:
            return
        if not raw:
            return
        try:
            doc = json.loads(raw)
        except (ValueError, TypeError):
            return
        view.update_doc(doc, fps)

    def _update_from_file(self, view: _ProfilerView, path: str) -> None:
        try:
            with open(path, "r", encoding="utf-8") as handle:
                raw = handle.read()
        except OSError:
            return
        if not raw:
            return
        try:
            doc = json.loads(raw)
        except (ValueError, TypeError):
            return
        view.update_doc(doc, None)

    def closeEvent(self, event) -> None:
        self._timer.stop()
        super().closeEvent(event)


def _format_counter(name: str, value: float) -> str:
    # Component hook timings and frame-time-like counters read best as milliseconds;
    # GPU submission counts read best as plain integers.
    if name.startswith("comp.") or name.endswith(".ms"):
        return f"{value:.3f} ms"
    if name.startswith("gpu.") or float(value).is_integer():
        return f"{int(value):,}"
    return f"{value:g}"


def _format_bytes(n: int) -> str:
    units = ["B", "KB", "MB", "GB"]
    value = float(n)
    for unit in units:
        if abs(value) < 1024.0 or unit == units[-1]:
            return f"{value:.1f} {unit}" if unit != "B" else f"{int(value)} B"
        value /= 1024.0
    return f"{n} B"
