"""
Audio Mixer Panel

A DAW-style mixer for audio buses and their DSP effect chains. Each bus is a
vertical channel strip (volume fader, mute/solo, effect rack); selecting an
effect opens a parameter inspector on the right. All edits drive the live engine
AudioManager through the EditorBridge so changes are audible in editor preview,
and the layout is persisted with the project.
"""

import sys
import json
import math
from pathlib import Path

from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel, QPushButton,
                             QSlider, QListWidget, QListWidgetItem, QFrame, QScrollArea,
                             QMenu, QDoubleSpinBox, QInputDialog, QSizePolicy, QSplitter)
from PyQt6.QtCore import Qt, pyqtSignal, QTimer, QRectF
from PyQt6.QtGui import QPainter, QColor

sys.path.insert(0, str(Path(__file__).parent.parent))
from theme import get_theme_manager
from panels.base_panel import EditorPanel


# Per-parameter UI metadata: (min, max, step, suffix). Anything not listed falls
# back to a 0..1 normalized range. Keeps the editor in sync with AudioDSP param
# names while giving each control a sensible range and unit.
PARAM_META = {
    "gain_db": (-60.0, 24.0, 0.5, " dB"),
    "cutoff_hz": (20.0, 20000.0, 10.0, " Hz"),
    "frequency_hz": (20.0, 20000.0, 10.0, " Hz"),
    "resonance": (0.1, 18.0, 0.05, ""),
    "time_ms": (0.0, 2000.0, 1.0, " ms"),
    "feedback": (0.0, 0.99, 0.01, ""),
    "wet": (0.0, 1.0, 0.01, ""),
    "dry": (0.0, 1.0, 0.01, ""),
    "room_size": (0.0, 1.0, 0.01, ""),
    "damping": (0.0, 1.0, 0.01, ""),
    "width": (0.0, 1.0, 0.01, ""),
    "threshold_db": (-60.0, 0.0, 0.5, " dB"),
    "ratio": (1.0, 20.0, 0.1, ":1"),
    "attack_ms": (0.1, 200.0, 0.1, " ms"),
    "release_ms": (1.0, 2000.0, 1.0, " ms"),
    "makeup_db": (0.0, 24.0, 0.5, " dB"),
    "drive_db": (0.0, 48.0, 0.5, " dB"),
    "rate_hz": (0.05, 10.0, 0.05, " Hz"),
    "depth_ms": (0.0, 20.0, 0.1, " ms"),
    "mix": (0.0, 1.0, 0.01, ""),
}

# Friendly parameter ordering/labels per nothing-special; show in a stable order.
PARAM_ORDER = list(PARAM_META.keys())


def _param_label(name):
    return name.replace("_db", " (dB)").replace("_hz", " (Hz)").replace("_ms", " (ms)").replace("_", " ").title()


class VUMeter(QWidget):
    """A stereo peak meter on a -60..0 dB scale with falling peak-hold marks."""

    def __init__(self, theme, parent=None):
        super().__init__(parent)
        self._theme = theme
        self._l = 0.0
        self._r = 0.0
        self._peak_l = 0.0
        self._peak_r = 0.0
        self.setMinimumWidth(20)
        self.setMinimumHeight(120)
        self.setSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Expanding)

    def set_levels(self, left, right):
        self._l = max(0.0, left)
        self._r = max(0.0, right)
        # Peak-hold: jump up instantly, fall slowly.
        self._peak_l = max(self._l, self._peak_l * 0.93)
        self._peak_r = max(self._r, self._peak_r * 0.93)
        self.update()

    @staticmethod
    def _to_fraction(level):
        if level <= 1e-6:
            return 0.0
        db = 20.0 * math.log10(min(level, 4.0))
        return max(0.0, min(1.0, (db + 60.0) / 60.0))

    def _draw_channel(self, painter, x, w, h, level, peak):
        # Track background.
        painter.fillRect(int(x), 0, int(w), int(h), QColor(20, 18, 28))
        frac = self._to_fraction(level)
        # Coloured zones (green / yellow / red) clipped to the filled portion.
        zones = [(0.0, 0.70, QColor(60, 200, 90)),
                 (0.70, 0.90, QColor(220, 200, 60)),
                 (0.90, 1.0, QColor(225, 70, 55))]
        for lo, hi, color in zones:
            top = min(frac, hi)
            if top <= lo:
                continue
            y0 = h * (1.0 - top)
            y1 = h * (1.0 - lo)
            painter.fillRect(QRectF(x, y0, w, y1 - y0), color)
        # Peak-hold mark.
        pfrac = self._to_fraction(peak)
        if pfrac > 0.001:
            py = h * (1.0 - pfrac)
            mark = QColor(240, 240, 240) if pfrac < 0.99 else QColor(255, 80, 60)
            painter.fillRect(QRectF(x, max(0.0, py - 1.0), w, 2.0), mark)

    def paintEvent(self, event):
        painter = QPainter(self)
        h = float(self.height())
        total = float(self.width())
        gap = 2.0
        col_w = (total - gap) / 2.0
        self._draw_channel(painter, 0.0, col_w, h, self._l, self._peak_l)
        self._draw_channel(painter, col_w + gap, col_w, h, self._r, self._peak_r)
        painter.end()


class ParamRow(QWidget):
    """A labelled slider + spin box bound to one effect parameter."""

    value_changed = pyqtSignal(str, float)  # parameter_name, value

    def __init__(self, name, value, theme, parent=None):
        super().__init__(parent)
        self.name = name
        self._updating = False
        lo, hi, step, suffix = PARAM_META.get(name, (0.0, 1.0, 0.01, ""))
        self._lo, self._hi = lo, hi

        text_color = theme.colors.text_primary if theme else "#ffffff"

        layout = QHBoxLayout(self)
        layout.setContentsMargins(0, 1, 0, 1)
        layout.setSpacing(6)

        label = QLabel(_param_label(name))
        label.setMinimumWidth(110)
        label.setStyleSheet(f"color: {text_color}; background: transparent;")
        layout.addWidget(label)

        self.slider = QSlider(Qt.Orientation.Horizontal)
        self.slider.setMinimum(0)
        self.slider.setMaximum(1000)
        self.slider.setValue(self._to_slider(value))
        self.slider.valueChanged.connect(self._on_slider)
        layout.addWidget(self.slider, 1)

        self.spin = QDoubleSpinBox()
        self.spin.setRange(lo, hi)
        self.spin.setSingleStep(step)
        self.spin.setDecimals(2 if step < 1.0 else 0)
        self.spin.setSuffix(suffix)
        self.spin.setValue(value)
        self.spin.setMinimumWidth(90)
        self.spin.valueChanged.connect(self._on_spin)
        layout.addWidget(self.spin)

    def _to_slider(self, value):
        if self._hi <= self._lo:
            return 0
        frac = (value - self._lo) / (self._hi - self._lo)
        return int(max(0.0, min(1.0, frac)) * 1000)

    def _from_slider(self, sval):
        return self._lo + (sval / 1000.0) * (self._hi - self._lo)

    def _on_slider(self, sval):
        if self._updating:
            return
        value = self._from_slider(sval)
        self._updating = True
        self.spin.setValue(value)
        self._updating = False
        self.value_changed.emit(self.name, value)

    def _on_spin(self, value):
        if self._updating:
            return
        self._updating = True
        self.slider.setValue(self._to_slider(value))
        self._updating = False
        self.value_changed.emit(self.name, value)


class ChannelStrip(QFrame):
    """A single bus's channel strip: fader, mute/solo, and effect rack."""

    def __init__(self, bus, mixer, theme, parent=None):
        super().__init__(parent)
        self.bus = bus
        self.mixer = mixer
        self.theme = theme
        self.bus_name = bus.get("name", "Bus")
        self._updating = False

        bg = theme.colors.surface if theme else "#1e1a28"
        border = theme.colors.border if theme else "#3d3650"
        text = theme.colors.text_primary if theme else "#ffffff"
        accent = theme.colors.accent_color if theme else "#5a4a7a"

        self.setFrameShape(QFrame.Shape.StyledPanel)
        self.setStyleSheet(f"ChannelStrip {{ background-color: {bg}; border: 1px solid {border}; border-radius: 4px; }}")
        self.setFixedWidth(160)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(6, 6, 6, 6)
        layout.setSpacing(4)

        # Bus name.
        name_label = QLabel(self.bus_name)
        name_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        name_label.setStyleSheet(f"color: {text}; font-weight: bold; background: transparent;")
        layout.addWidget(name_label)

        if self.bus.get("parentBus"):
            parent_label = QLabel(f"→ {self.bus['parentBus']}")
            parent_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
            parent_label.setStyleSheet(f"color: {self._dim(text)}; font-size: 9px; background: transparent;")
            layout.addWidget(parent_label)

        # Volume fader + live VU meter.
        fader_row = QHBoxLayout()
        fader_row.setContentsMargins(0, 0, 0, 0)
        fader_row.setSpacing(6)
        self.fader = QSlider(Qt.Orientation.Vertical)
        self.fader.setMinimum(0)
        self.fader.setMaximum(100)
        self.fader.setValue(int(round(bus.get("volume", 1.0) * 100)))
        self.fader.setMinimumHeight(130)
        self.fader.valueChanged.connect(self._on_volume)
        self.meter = VUMeter(theme)
        fader_row.addStretch()
        fader_row.addWidget(self.fader)
        fader_row.addWidget(self.meter)
        fader_row.addStretch()
        layout.addLayout(fader_row)

        self.volume_label = QLabel(f"{self.fader.value()}%")
        self.volume_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.volume_label.setStyleSheet(f"color: {self._dim(text)}; font-size: 10px; background: transparent;")
        layout.addWidget(self.volume_label)

        # Mute / Solo.
        ms_row = QHBoxLayout()
        ms_row.setSpacing(4)
        self.mute_btn = QPushButton("M")
        self.mute_btn.setCheckable(True)
        self.mute_btn.setChecked(bool(bus.get("muted", False)))
        self.mute_btn.setToolTip("Mute")
        self.mute_btn.clicked.connect(self._on_mute)
        self._style_toggle(self.mute_btn, "#c0563a")
        self.solo_btn = QPushButton("S")
        self.solo_btn.setCheckable(True)
        self.solo_btn.setChecked(bool(bus.get("solo", False)))
        self.solo_btn.setToolTip("Solo")
        self.solo_btn.clicked.connect(self._on_solo)
        self._style_toggle(self.solo_btn, "#c0a23a")
        ms_row.addWidget(self.mute_btn)
        ms_row.addWidget(self.solo_btn)
        layout.addLayout(ms_row)

        # Effect rack.
        fx_label = QLabel("Effects")
        fx_label.setStyleSheet(f"color: {self._dim(text)}; font-size: 9px; background: transparent;")
        layout.addWidget(fx_label)

        self.fx_list = QListWidget()
        self.fx_list.setStyleSheet(
            f"QListWidget {{ background-color: {self._dim_bg(bg)}; color: {text}; border: 1px solid {border}; }}")
        self.fx_list.setMaximumHeight(110)
        self.fx_list.itemChanged.connect(self._on_fx_item_changed)
        self.fx_list.currentRowChanged.connect(self._on_fx_selected)
        layout.addWidget(self.fx_list)
        self._populate_effects()

        # Effect rack buttons.
        fx_btns = QHBoxLayout()
        fx_btns.setSpacing(2)
        add_btn = QPushButton("+")
        add_btn.setToolTip("Add effect")
        add_btn.clicked.connect(self._show_add_effect_menu)
        rem_btn = QPushButton("−")
        rem_btn.setToolTip("Remove selected effect")
        rem_btn.clicked.connect(self._remove_selected_effect)
        up_btn = QPushButton("↑")
        up_btn.setToolTip("Move effect up")
        up_btn.clicked.connect(lambda: self._move_selected_effect(-1))
        down_btn = QPushButton("↓")
        down_btn.setToolTip("Move effect down")
        down_btn.clicked.connect(lambda: self._move_selected_effect(1))
        for b in (add_btn, rem_btn, up_btn, down_btn):
            b.setFixedWidth(34)
            b.setStyleSheet(
                f"QPushButton {{ background-color: {accent}; color: {text}; border: none; "
                f"padding: 3px; border-radius: 3px; }} QPushButton:hover {{ background-color: {self._lighten(accent)}; }}")
            fx_btns.addWidget(b)
        layout.addLayout(fx_btns)

        layout.addStretch()

    # ----- styling helpers -----
    def _dim(self, color):
        return self.theme.colors.text_secondary if self.theme else "#aaaaaa"

    def _dim_bg(self, color):
        return color

    def _lighten(self, color):
        return self.theme.colors.accent_hover if self.theme else "#6a5a8a"

    def _style_toggle(self, btn, active_color):
        text = self.theme.colors.text_primary if self.theme else "#ffffff"
        base = self.theme.colors.surface_hover if self.theme else "#3a3a3a"
        btn.setFixedHeight(22)
        btn.setStyleSheet(f"""
            QPushButton {{ background-color: {base}; color: {text}; border: none; border-radius: 3px; font-weight: bold; }}
            QPushButton:checked {{ background-color: {active_color}; color: #ffffff; }}
        """)

    # ----- effects -----
    def _populate_effects(self):
        self._updating = True
        self.fx_list.clear()
        for effect in self.bus.get("effects", []):
            item = QListWidgetItem(effect.get("type", "Effect"))
            item.setFlags(item.flags() | Qt.ItemFlag.ItemIsUserCheckable)
            item.setCheckState(Qt.CheckState.Checked if effect.get("enabled", True) else Qt.CheckState.Unchecked)
            self.fx_list.addItem(item)
        self._updating = False

    def _on_fx_item_changed(self, item):
        if self._updating:
            return
        index = self.fx_list.row(item)
        enabled = item.checkState() == Qt.CheckState.Checked
        self.mixer.set_effect_enabled(self.bus_name, index, enabled)

    def _on_fx_selected(self, row):
        if row < 0 or row >= len(self.bus.get("effects", [])):
            return
        self.mixer.show_effect_parameters(self.bus_name, row, self.bus["effects"][row])

    def _show_add_effect_menu(self):
        menu = QMenu(self)
        for effect_type in self.mixer.effect_types:
            menu.addAction(effect_type, lambda t=effect_type: self.mixer.add_effect(self.bus_name, t))
        menu.exec(self.mapToGlobal(self.rect().center()))

    def _remove_selected_effect(self):
        row = self.fx_list.currentRow()
        if row >= 0:
            self.mixer.remove_effect(self.bus_name, row)

    def _move_selected_effect(self, delta):
        row = self.fx_list.currentRow()
        if row < 0:
            return
        target = row + delta
        if 0 <= target < self.fx_list.count():
            self.mixer.move_effect(self.bus_name, row, target)

    # ----- bus controls -----
    def _on_volume(self, value):
        self.volume_label.setText(f"{value}%")
        if not self._updating:
            self.mixer.set_volume(self.bus_name, value / 100.0)

    def _on_mute(self):
        self.mixer.set_muted(self.bus_name, self.mute_btn.isChecked())

    def _on_solo(self):
        self.mixer.set_solo(self.bus_name, self.solo_btn.isChecked())

    def set_levels(self, left, right):
        self.meter.set_levels(left, right)


class AudioMixerPanel(EditorPanel):
    """Dockable DAW-style audio bus mixer."""

    def __init__(self, parent=None):
        self.editor_bridge = None
        self.main_editor = None
        self.effect_types = []
        self._selected_bus = None
        self._selected_effect_index = -1
        super().__init__("Audio Mixer", parent)

    def _setup_panel(self):
        theme = get_theme_manager().get_current_theme()
        self._theme = theme
        text = theme.colors.text_primary if theme else "#ffffff"
        accent = theme.colors.accent_color if theme else "#5a4a7a"

        root = QWidget()
        root_layout = QVBoxLayout(root)
        root_layout.setContentsMargins(6, 6, 6, 6)
        root_layout.setSpacing(6)

        # Toolbar.
        toolbar = QHBoxLayout()
        add_bus_btn = QPushButton("+ Add Bus")
        add_bus_btn.clicked.connect(self._add_bus)
        del_bus_btn = QPushButton("Remove Bus")
        del_bus_btn.clicked.connect(self._remove_selected_bus)
        refresh_btn = QPushButton("Refresh")
        refresh_btn.clicked.connect(self.refresh)
        for b in (add_bus_btn, del_bus_btn, refresh_btn):
            b.setStyleSheet(
                f"QPushButton {{ background-color: {accent}; color: {text}; border: none; "
                f"padding: 4px 10px; border-radius: 3px; }}")
        toolbar.addWidget(add_bus_btn)
        toolbar.addWidget(del_bus_btn)
        toolbar.addStretch()
        toolbar.addWidget(refresh_btn)
        root_layout.addLayout(toolbar)

        # Main split: strips on the left, parameter inspector on the right.
        splitter = QSplitter(Qt.Orientation.Horizontal)

        self._strips_scroll = QScrollArea()
        self._strips_scroll.setWidgetResizable(True)
        self._strips_host = QWidget()
        self._strips_layout = QHBoxLayout(self._strips_host)
        self._strips_layout.setContentsMargins(2, 2, 2, 2)
        self._strips_layout.setSpacing(6)
        self._strips_layout.addStretch()
        self._strips_scroll.setWidget(self._strips_host)
        splitter.addWidget(self._strips_scroll)

        # Effect parameter inspector.
        self._inspector = QWidget()
        self._inspector_layout = QVBoxLayout(self._inspector)
        self._inspector_layout.setContentsMargins(6, 6, 6, 6)
        self._inspector_layout.setSpacing(4)
        self._inspector_title = QLabel("Select an effect")
        self._inspector_title.setStyleSheet(f"color: {text}; font-weight: bold; background: transparent;")
        self._inspector_layout.addWidget(self._inspector_title)
        self._params_host = QWidget()
        self._params_layout = QVBoxLayout(self._params_host)
        self._params_layout.setContentsMargins(0, 0, 0, 0)
        self._params_layout.setSpacing(2)
        self._inspector_layout.addWidget(self._params_host)
        self._inspector_layout.addStretch()
        inspector_scroll = QScrollArea()
        inspector_scroll.setWidgetResizable(True)
        inspector_scroll.setWidget(self._inspector)
        inspector_scroll.setMinimumWidth(280)
        splitter.addWidget(inspector_scroll)
        splitter.setStretchFactor(0, 3)
        splitter.setStretchFactor(1, 2)

        root_layout.addWidget(splitter, 1)
        self.content_layout.addWidget(root)

        self._strips = []
        self._strips_by_name = {}

        # Live VU meter poll (~20 fps); only runs while the panel is visible.
        self._meter_timer = QTimer(self)
        self._meter_timer.setInterval(50)
        self._meter_timer.timeout.connect(self._poll_levels)

    # ============================ data / refresh ============================

    def showEvent(self, event):
        super().showEvent(event)
        if not self.effect_types and self.editor_bridge:
            self.refresh()
        if self.editor_bridge:
            self._meter_timer.start()

    def hideEvent(self, event):
        self._meter_timer.stop()
        super().hideEvent(event)

    def _poll_levels(self):
        if not self.editor_bridge or not self._strips_by_name:
            return
        try:
            levels = json.loads(self.editor_bridge.get_audio_bus_levels_json())
        except Exception:
            return
        for name, strip in self._strips_by_name.items():
            pair = levels.get(name)
            if pair and len(pair) >= 2:
                strip.set_levels(float(pair[0]), float(pair[1]))
            else:
                strip.set_levels(0.0, 0.0)

    def refresh(self):
        """Rebuild all channel strips from the live engine state."""
        if not self.editor_bridge:
            return
        try:
            self.effect_types = json.loads(self.editor_bridge.get_audio_effect_types_json())
        except Exception:
            self.effect_types = []
        try:
            buses = json.loads(self.editor_bridge.get_audio_buses_json())
        except Exception as e:
            print(f"[AudioMixer] Failed to read buses: {e}")
            buses = []

        # Master first, then alphabetical.
        buses.sort(key=lambda b: (b.get("name") != "Master", b.get("name", "")))

        # Clear existing strips (keep the trailing stretch).
        for strip in self._strips:
            strip.setParent(None)
            strip.deleteLater()
        self._strips = []
        self._strips_by_name = {}

        for bus in buses:
            strip = ChannelStrip(bus, self, self._theme)
            self._strips.append(strip)
            self._strips_by_name[strip.bus_name] = strip
            self._strips_layout.insertWidget(self._strips_layout.count() - 1, strip)

        # Restore the parameter inspector if its bus/effect still exists.
        self._restore_inspector(buses)

        if self.editor_bridge and self.isVisible() and not self._meter_timer.isActive():
            self._meter_timer.start()

    def _restore_inspector(self, buses):
        if self._selected_bus is None:
            return
        for bus in buses:
            if bus.get("name") == self._selected_bus:
                effects = bus.get("effects", [])
                if 0 <= self._selected_effect_index < len(effects):
                    self.show_effect_parameters(self._selected_bus, self._selected_effect_index,
                                                effects[self._selected_effect_index])
                    return
        self._clear_inspector()

    def _clear_inspector(self):
        self._selected_bus = None
        self._selected_effect_index = -1
        self._inspector_title.setText("Select an effect")
        self._rebuild_params([], None, -1)

    # ============================ inspector ============================

    def show_effect_parameters(self, bus_name, index, effect):
        self._selected_bus = bus_name
        self._selected_effect_index = index
        self._inspector_title.setText(f"{bus_name}  ·  {effect.get('type', 'Effect')}  [#{index}]")
        params = effect.get("parameters", {})
        ordered = [n for n in PARAM_ORDER if n in params]
        ordered += [n for n in params if n not in PARAM_ORDER]
        self._rebuild_params(ordered, params, index, bus_name)

    def _rebuild_params(self, ordered, params, index, bus_name=None):
        # Clear existing param rows.
        while self._params_layout.count():
            item = self._params_layout.takeAt(0)
            w = item.widget()
            if w:
                w.deleteLater()
        if not ordered or params is None:
            return
        for name in ordered:
            row = ParamRow(name, float(params.get(name, 0.0)), self._theme)
            row.value_changed.connect(
                lambda pname, value, bn=bus_name, idx=index: self._on_param_changed(bn, idx, pname, value))
            self._params_layout.addWidget(row)

    def _on_param_changed(self, bus_name, index, parameter, value):
        if self.editor_bridge and bus_name is not None:
            self.editor_bridge.set_bus_effect_parameter(bus_name, index, parameter, value)
            self._mark_dirty()

    # ============================ bus / effect ops ============================

    def set_volume(self, bus_name, volume):
        if self.editor_bridge:
            self.editor_bridge.set_bus_volume(bus_name, volume)
            self._mark_dirty()

    def set_muted(self, bus_name, muted):
        if self.editor_bridge:
            self.editor_bridge.set_bus_muted(bus_name, muted)
            self._mark_dirty()

    def set_solo(self, bus_name, solo):
        if self.editor_bridge:
            self.editor_bridge.set_bus_solo(bus_name, solo)
            self._mark_dirty()

    def add_effect(self, bus_name, effect_type):
        if self.editor_bridge:
            self.editor_bridge.add_bus_effect(bus_name, effect_type)
            self._mark_dirty()
            self.refresh()

    def remove_effect(self, bus_name, index):
        if self.editor_bridge:
            self.editor_bridge.remove_bus_effect(bus_name, index)
            self._mark_dirty()
            self.refresh()

    def move_effect(self, bus_name, from_index, to_index):
        if self.editor_bridge:
            self.editor_bridge.move_bus_effect(bus_name, from_index, to_index)
            self._mark_dirty()
            self.refresh()

    def set_effect_enabled(self, bus_name, index, enabled):
        if self.editor_bridge:
            self.editor_bridge.set_bus_effect_enabled(bus_name, index, enabled)
            self._mark_dirty()

    def _add_bus(self):
        if not self.editor_bridge:
            return
        name, ok = QInputDialog.getText(self, "Add Audio Bus", "Bus name:")
        if not ok or not name.strip():
            return
        name = name.strip()
        # Offer a parent choice from existing buses (default Master).
        parents = ["Master"]
        try:
            parents = [b.get("name") for b in json.loads(self.editor_bridge.get_audio_buses_json())]
        except Exception:
            pass
        parent, ok = QInputDialog.getItem(self, "Parent Bus",
                                          "Route output to:", parents, 0, False)
        if not ok:
            parent = "Master"
        self.editor_bridge.create_audio_bus(name, parent)
        self._mark_dirty()
        self.refresh()

    def _remove_selected_bus(self):
        if not self.editor_bridge:
            return
        try:
            names = [b.get("name") for b in json.loads(self.editor_bridge.get_audio_buses_json())]
        except Exception:
            names = []
        removable = [n for n in names if n and n != "Master"]
        if not removable:
            return
        name, ok = QInputDialog.getItem(self, "Remove Audio Bus",
                                        "Bus to remove:", removable, 0, False)
        if not ok or not name:
            return
        self.editor_bridge.destroy_audio_bus(name)
        if self._selected_bus == name:
            self._clear_inspector()
        self._mark_dirty()
        self.refresh()

    def _mark_dirty(self):
        # Persist the mixer with the project; mark the project dirty if supported.
        if self.main_editor and hasattr(self.main_editor, "mark_project_dirty"):
            try:
                self.main_editor.mark_project_dirty()
            except Exception:
                pass

    def get_panel_id(self) -> str:
        return "audio_mixer"
