"""
Size Flags Property Widget

A friendlier editor for a UIControl container size-flag bitmask
(`sizeFlagsHorizontal` / `sizeFlagsVertical`).
- Fill and Expand are independent checkboxes.
- Shrink alignment (Begin / Center / End) is a mutually-exclusive choice that
  applies when the control is not filling.

Bit layout must match lupine::components::SizeFlagBits:
  Fill = 1, Expand = 2, ShrinkCenter = 4, ShrinkEnd = 8, ShrinkBegin = 0.
"""

from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel,
                              QCheckBox, QRadioButton, QButtonGroup, QSizePolicy)
from PyQt6.QtCore import Qt, pyqtSignal

FILL = 1
EXPAND = 2
SHRINK_CENTER = 4
SHRINK_END = 8


class SizeFlagsPropertyWidget(QWidget):
    """Editor for a single size-flags int bitmask property."""
    value_changed = pyqtSignal(object)  # new value (int bitmask)

    def __init__(self, property_name, default_value=FILL, parent=None):
        super().__init__(parent)
        self.property_name = property_name
        try:
            self.default_value = int(default_value)
        except (TypeError, ValueError):
            self.default_value = FILL
        self._current_value = self.default_value
        self._updating = False
        self.reset_button = None
        self.setup_ui()

    def setup_ui(self):
        root = QVBoxLayout()
        root.setContentsMargins(0, 2, 0, 2)
        root.setSpacing(2)

        title = QLabel(self.property_name)
        title.setSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Fixed)
        root.addWidget(title)

        flags_row = QHBoxLayout()
        flags_row.setSpacing(8)
        self.fill_check = QCheckBox("Fill")
        self.fill_check.toggled.connect(self._on_changed)
        self.expand_check = QCheckBox("Expand")
        self.expand_check.toggled.connect(self._on_changed)
        flags_row.addWidget(self.fill_check)
        flags_row.addWidget(self.expand_check)
        flags_row.addStretch()
        root.addLayout(flags_row)

        align_row = QHBoxLayout()
        align_row.setSpacing(8)
        align_row.addWidget(QLabel("Shrink:"))
        self.align_group = QButtonGroup(self)
        self.align_begin = QRadioButton("Begin")
        self.align_center = QRadioButton("Center")
        self.align_end = QRadioButton("End")
        for rb in (self.align_begin, self.align_center, self.align_end):
            self.align_group.addButton(rb)
            rb.toggled.connect(self._on_changed)
            align_row.addWidget(rb)
        align_row.addStretch()
        self.align_begin.setChecked(True)
        root.addLayout(align_row)

        self.setLayout(root)
        self.set_value(self._current_value)

    def _on_changed(self, _checked=False):
        if self._updating:
            return
        value = 0
        if self.fill_check.isChecked():
            value |= FILL
        if self.expand_check.isChecked():
            value |= EXPAND
        if self.align_center.isChecked():
            value |= SHRINK_CENTER
        elif self.align_end.isChecked():
            value |= SHRINK_END
        # Begin contributes no bits (ShrinkBegin == 0).
        self._current_value = value
        self.value_changed.emit(value)

    def set_value(self, value):
        try:
            value = int(value)
        except (TypeError, ValueError):
            value = self.default_value
        self._updating = True
        self._current_value = value
        self.fill_check.setChecked(bool(value & FILL))
        self.expand_check.setChecked(bool(value & EXPAND))
        if value & SHRINK_END:
            self.align_end.setChecked(True)
        elif value & SHRINK_CENTER:
            self.align_center.setChecked(True)
        else:
            self.align_begin.setChecked(True)
        self._updating = False

    def get_value(self):
        return self._current_value

    def reset_to_default(self):
        self.set_value(self.default_value)
        self.value_changed.emit(self.default_value)
