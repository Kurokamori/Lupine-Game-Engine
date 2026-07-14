"""
Anchor Preset Property Widget

Anchor preset picker for UIControl-derived components. Presents a 3x3
grid of point/corner presets plus a row of wide (stretch) presets. Selecting a preset
writes the `anchorPreset` enum index; the engine applies it (anchors + offsets) via
UIControl::OnPropertyChanged -> ApplyAnchorPreset.

The enum order must match lupine::components::AnchorPreset:
  0 TopLeft 1 TopCenter 2 TopRight 3 CenterLeft 4 Center 5 CenterRight
  6 BottomLeft 7 BottomCenter 8 BottomRight 9 LeftWide 10 TopWide 11 RightWide
  12 BottomWide 13 VCenterWide 14 HCenterWide 15 FullRect 16 Custom
"""

from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QGridLayout,
                              QLabel, QPushButton, QSizePolicy)
from PyQt6.QtCore import Qt, pyqtSignal
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent))
from theme import get_theme_manager


PRESET_CUSTOM = 16
# Sentinel emitted when the active preset is clicked again: anchoring is disabled and the
# control reverts to free (Position) layout. The engine maps anchorPreset < 0 to Position mode.
PRESET_NONE = -1

# 3x3 point/corner presets: (grid_row, grid_col, short_label, full_name, enum_index)
_POINT_PRESETS = [
    (0, 0, "TL", "Top Left", 0),
    (0, 1, "T", "Top Center", 1),
    (0, 2, "TR", "Top Right", 2),
    (1, 0, "L", "Center Left", 3),
    (1, 1, "C", "Center", 4),
    (1, 2, "R", "Center Right", 5),
    (2, 0, "BL", "Bottom Left", 6),
    (2, 1, "B", "Bottom Center", 7),
    (2, 2, "BR", "Bottom Right", 8),
]

# Wide / stretch presets: (short_label, full_name, enum_index)
_WIDE_PRESETS = [
    ("L Wide", "Left Wide", 9),
    ("T Wide", "Top Wide", 10),
    ("R Wide", "Right Wide", 11),
    ("B Wide", "Bottom Wide", 12),
    ("V Wide", "VCenter Wide", 13),
    ("H Wide", "HCenter Wide", 14),
    ("Full", "Full Rect", 15),
]


class AnchorPresetPropertyWidget(QWidget):
    """Anchor preset picker bound to the `anchorPreset` enum property."""
    value_changed = pyqtSignal(object)  # new value (int preset index)

    def __init__(self, property_name, default_value=0, parent=None):
        super().__init__(parent)
        self.property_name = property_name
        self.default_value = default_value if isinstance(default_value, int) else 0
        self._current_value = self.default_value
        self._buttons = {}  # enum index -> QPushButton
        self.reset_button = None
        self.setup_ui()

    def _style_button(self, btn):
        theme = get_theme_manager().get_current_theme()
        if not theme:
            return
        btn.setStyleSheet(f"""
            QPushButton {{
                background-color: {theme.colors.surface};
                color: {theme.colors.text_primary};
                border: 1px solid {theme.colors.border};
                border-radius: 3px;
                padding: 2px;
            }}
            QPushButton:hover {{
                border-color: {theme.colors.accent_color};
            }}
            QPushButton:checked {{
                background-color: {theme.colors.accent_color};
                color: {theme.colors.text_on_accent};
                border-color: {theme.colors.accent_color};
            }}
        """)

    # Spelled out on every button, because it is otherwise completely undiscoverable: there
    # is no other way to turn anchoring back off from this widget.
    _TOGGLE_HINT = "Click the active preset again to turn anchoring off (free/Position layout)."

    def setup_ui(self):
        root = QVBoxLayout()
        root.setContentsMargins(0, 2, 0, 2)
        root.setSpacing(4)

        title = QLabel("Anchor Preset")
        title.setSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Fixed)
        title.setToolTip(self._TOGGLE_HINT)
        root.addWidget(title)

        # 3x3 grid of point/corner presets.
        grid = QGridLayout()
        grid.setSpacing(2)
        for row, col, label, full, index in _POINT_PRESETS:
            btn = QPushButton(label)
            btn.setToolTip(f"{full}\n\n{self._TOGGLE_HINT}")
            btn.setCheckable(True)
            btn.setAutoExclusive(False)
            btn.setFixedSize(34, 26)
            self._style_button(btn)
            btn.clicked.connect(lambda _checked, i=index: self._on_preset_clicked(i))
            self._buttons[index] = btn
            grid.addWidget(btn, row, col)
        grid_host = QHBoxLayout()
        grid_host.addLayout(grid)
        grid_host.addStretch()
        root.addLayout(grid_host)

        # Wide / stretch presets, wrapped across two rows of buttons.
        wide_row = QHBoxLayout()
        wide_row.setSpacing(2)
        for i, (label, full, index) in enumerate(_WIDE_PRESETS):
            btn = QPushButton(label)
            btn.setToolTip(f"{full}\n\n{self._TOGGLE_HINT}")
            btn.setCheckable(True)
            btn.setAutoExclusive(False)
            btn.setMinimumHeight(24)
            self._style_button(btn)
            btn.clicked.connect(lambda _checked, ix=index: self._on_preset_clicked(ix))
            self._buttons[index] = btn
            wide_row.addWidget(btn)
        root.addLayout(wide_row)

        hint = QLabel("Click the active preset again to disable anchoring")
        hint.setWordWrap(True)
        hint.setStyleSheet("color: palette(mid); font-size: 10px;")
        root.addWidget(hint)

        self.setLayout(root)
        self._update_highlight()

    def _on_preset_clicked(self, index):
        # Clicking the currently-active preset toggles anchoring off (back to free layout).
        if self._current_value == index:
            self._current_value = PRESET_NONE
            self._update_highlight()
            self.value_changed.emit(PRESET_NONE)
            return
        self._current_value = index
        self._update_highlight()
        self.value_changed.emit(index)

    def _update_highlight(self):
        # Only the 3x3 / wide presets (0..15) are highlightable; Custom (16) and the
        # disabled sentinel (-1) leave every button unchecked.
        active = (0 <= self._current_value <= 15)
        for index, btn in self._buttons.items():
            btn.setChecked(active and index == self._current_value)

    def set_value(self, value):
        try:
            self._current_value = int(value)
        except (TypeError, ValueError):
            self._current_value = self.default_value
        self._update_highlight()

    def get_value(self):
        return self._current_value

    def reset_to_default(self):
        self.set_value(self.default_value)
        self.value_changed.emit(self.default_value)
