"""
Input Mapping Dialog
Godot-style input mapping editor for defining actions and keybindings
Supports keyboard, mouse, and gamepad inputs with multiple bindings per action
"""

from PyQt6.QtWidgets import (QDialog, QVBoxLayout, QHBoxLayout, QPushButton,
                             QTreeWidget, QTreeWidgetItem, QLabel, QWidget,
                             QLineEdit, QMessageBox, QMenu, QComboBox, QSpinBox,
                             QGroupBox, QFormLayout, QDoubleSpinBox, QSplitter,
                             QCheckBox, QFrame, QLayout, QSizePolicy)
from PyQt6.QtCore import Qt, pyqtSignal, QTimer
from PyQt6.QtGui import QKeyEvent, QIcon, QMouseEvent
import sys
import json
from pathlib import Path
from typing import Dict, List, Optional, Any

# Optional engine runtime module: provides windowless SDL game-controller polling
# (gamepad_capture_poll) so the binding editor can capture live gamepad input.
# Absent in some editor launch contexts or before the .pyd is built, in which case
# gamepad capture falls back to the manual dropdowns.
try:
    import lupine_runtime as _lupine_runtime
except Exception:
    _lupine_runtime = None


# Engine-canonical input names (must match lupine::input::*ToString in
# core/src/InputCodes.cpp so InputMap::FromJson resolves them). Each combo stores
# the canonical name as item data while showing a friendlier display label.
GAMEPAD_BUTTON_OPTIONS = [
    ("A / Cross (South)", "A"),
    ("B / Circle (East)", "B"),
    ("X / Square (West)", "X"),
    ("Y / Triangle (North)", "Y"),
    ("Left Bumper (LB / L1)", "LeftBumper"),
    ("Right Bumper (RB / R1)", "RightBumper"),
    ("Back / Select / Share", "Back"),
    ("Start / Options", "Start"),
    ("Guide / Home / PS", "Guide"),
    ("Left Stick Click (L3)", "LeftThumb"),
    ("Right Stick Click (R3)", "RightThumb"),
    ("D-Pad Up", "DPadUp"),
    ("D-Pad Right", "DPadRight"),
    ("D-Pad Down", "DPadDown"),
    ("D-Pad Left", "DPadLeft"),
]

GAMEPAD_AXIS_OPTIONS = [
    ("Left Stick X", "LeftX"),
    ("Left Stick Y", "LeftY"),
    ("Right Stick X", "RightX"),
    ("Right Stick Y", "RightY"),
    ("Left Trigger (LT / L2)", "LeftTrigger"),
    ("Right Trigger (RT / R2)", "RightTrigger"),
]

MOUSE_BUTTON_OPTIONS = [
    ("Left Click", "Left"),
    ("Right Click", "Right"),
    ("Middle Click", "Middle"),
    ("Button 4 (Back)", "Button4"),
    ("Button 5 (Forward)", "Button5"),
    ("Button 6", "Button6"),
    ("Button 7", "Button7"),
    ("Button 8", "Button8"),
]

# Engine GamepadButton enum order (index == enum value) and GamepadAxis order, used
# to resolve a canonical name to the integer the engine stores, and to render
# legacy bindings that only carry the index.
GAMEPAD_BUTTON_ORDER = [
    "A", "B", "X", "Y", "LeftBumper", "RightBumper", "Back", "Start", "Guide",
    "LeftThumb", "RightThumb", "DPadUp", "DPadRight", "DPadDown", "DPadLeft",
]
GAMEPAD_AXIS_ORDER = ["LeftX", "LeftY", "RightX", "RightY", "LeftTrigger", "RightTrigger"]
MOUSE_BUTTON_ORDER = ["Left", "Right", "Middle", "Button4", "Button5", "Button6", "Button7", "Button8"]

# Friendly display labels for canonical names (used by the bindings list).
GAMEPAD_BUTTON_DISPLAY = {canonical: display for display, canonical in GAMEPAD_BUTTON_OPTIONS}
GAMEPAD_AXIS_DISPLAY = {canonical: display for display, canonical in GAMEPAD_AXIS_OPTIONS}
MOUSE_BUTTON_DISPLAY = {canonical: display for display, canonical in MOUSE_BUTTON_OPTIONS}

# Engine KeyCode integer values (subset; mirrors core/include/lupine/input/InputCodes.hpp).
# The engine reads keyName at load time, so a missing code is harmless (defaults to 0),
# but storing the real code keeps the data correct.
KEYNAME_TO_CODE = {
    "Space": 32, "Apostrophe": 39, "Comma": 44, "Minus": 45, "Period": 46, "Slash": 47,
    "0": 48, "1": 49, "2": 50, "3": 51, "4": 52, "5": 53, "6": 54, "7": 55, "8": 56, "9": 57,
    "Semicolon": 59, "Equal": 61,
    "A": 65, "B": 66, "C": 67, "D": 68, "E": 69, "F": 70, "G": 71, "H": 72, "I": 73,
    "J": 74, "K": 75, "L": 76, "M": 77, "N": 78, "O": 79, "P": 80, "Q": 81, "R": 82,
    "S": 83, "T": 84, "U": 85, "V": 86, "W": 87, "X": 88, "Y": 89, "Z": 90,
    "LeftBracket": 91, "Backslash": 92, "RightBracket": 93, "GraveAccent": 96,
    "Escape": 256, "Enter": 257, "Tab": 258, "Backspace": 259, "Insert": 260, "Delete": 261,
    "Right": 262, "Left": 263, "Down": 264, "Up": 265, "PageUp": 266, "PageDown": 267,
    "Home": 268, "End": 269, "CapsLock": 280, "ScrollLock": 281, "NumLock": 282,
    "PrintScreen": 283, "Pause": 284,
    "F1": 290, "F2": 291, "F3": 292, "F4": 293, "F5": 294, "F6": 295, "F7": 296, "F8": 297,
    "F9": 298, "F10": 299, "F11": 300, "F12": 301,
    "LeftShift": 340, "LeftControl": 341, "LeftAlt": 342, "LeftSuper": 343,
    "RightShift": 344, "RightControl": 345, "RightAlt": 346, "RightSuper": 347, "Menu": 348,
}


# Conservative fallback palette mirroring the default Dark Purple theme, used only
# when the editor theme module can't be resolved (e.g. dialog opened standalone).
_PALETTE_FALLBACK = {
    "background": "#181719", "surface": "#141317", "surface_hover": "#0c0c0f",
    "tertiary_color": "#232029", "border": "#27242f", "border_focus": "#514864",
    "accent_color": "#76669b", "accent_hover": "#72648c",
    "secondary_accent_color": "#373246", "confirm_color": "#388e3c",
    "error_color": "#d32f2f", "warning_color": "#f57c00",
    "text_primary": "#e8e6f0", "text_secondary": "#b0a8c0",
    "text_disabled": "#6a6178", "text_on_accent": "#ffffff",
    "selection": "#3e384f",
}


def _to_rgb(hex_str: str):
    h = hex_str.lstrip("#")
    if len(h) == 3:
        h = "".join(ch * 2 for ch in h)
    h = (h + "000000")[:6]
    return int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16)


def _mix(a: str, b: str, t: float) -> str:
    ar, ag, ab = _to_rgb(a)
    br, bg, bb = _to_rgb(b)
    return "#{:02x}{:02x}{:02x}".format(
        int(ar + (br - ar) * t), int(ag + (bg - ag) * t), int(ab + (bb - ab) * t))


def _alpha(hex_str: str, a: float) -> str:
    r, g, b = _to_rgb(hex_str)
    return f"rgba({r}, {g}, {b}, {max(0.0, min(1.0, a)):.3f})"


def _card_bg(p: Dict[str, str]) -> str:
    """A recessed panel color sitting a step darker than the dialog background, so
    the Actions / Settings / Bindings groups read as distinct sunken surfaces."""
    return _mix(p["background"], "#000000", 0.4)


def _resolve_palette() -> Dict[str, str]:
    """Resolve the active editor theme to the colors these dialogs paint with,
    falling back to the bundled Dark Purple values if the theme module is absent."""
    try:
        editor_dir = str(Path(__file__).resolve().parent.parent)
        if editor_dir not in sys.path:
            sys.path.insert(0, editor_dir)
        from theme import get_theme_manager
        theme = get_theme_manager().get_current_theme()
        colors = theme.colors
        return {k: getattr(colors, k, v) for k, v in _PALETTE_FALLBACK.items()}
    except Exception:
        return dict(_PALETTE_FALLBACK)


def _dialog_qss(p: Dict[str, str]) -> str:
    """Shared stylesheet for both input dialogs: flattens the heavy group-box chrome
    into quiet eyebrow sections, defines the keycap capture target, and tunes the
    bottom-anchored action buttons. Every color is theme-derived."""
    surface = p["surface"]
    accent = p["accent_color"]
    accent_wash = _alpha(accent, 0.10)
    panel_bg = _mix(p["background"], surface, 0.6)
    card = _card_bg(p)
    hairline = _alpha(p["border"], 0.7)
    return f"""
        QLabel#dlgTitle {{
            font-size: 17px;
            font-weight: 600;
            color: {p['text_primary']};
            background: transparent;
        }}
        QLabel[eyebrow="true"] {{
            color: {p['text_secondary']};
            font-size: 10px;
            font-weight: 600;
            letter-spacing: 2px;
            background: transparent;
        }}
        QLabel#dlgSubtitle {{
            color: {p['text_secondary']};
            font-size: 11px;
            background: transparent;
        }}
        QFrame#hairline {{
            background: {hairline};
            border: none;
            max-height: 1px;
            min-height: 1px;
        }}

        /* Recessed cards: each group sits a step darker than the dialog, with the
           section name floated above as an eyebrow. */
        QGroupBox {{
            background: {card};
            border: 1px solid {hairline};
            border-radius: 10px;
            margin-top: 20px;
            padding: 16px 14px 14px 14px;
        }}
        QGroupBox::title {{
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 2px;
            padding: 0 0 4px 2px;
            color: {p['text_secondary']};
            font-size: 10px;
            font-weight: 600;
            letter-spacing: 2px;
        }}

        /* The keycap capture target — the signature element. */
        QFrame#captureCap {{
            background: {panel_bg};
            border: 1px solid {hairline};
            border-radius: 12px;
        }}
        QFrame#captureCap[listening="true"] {{
            background: {accent_wash};
            border: 1px solid {accent};
        }}
        QLabel#capGlyph {{
            font-size: 22px;
            font-weight: 600;
            color: {p['text_primary']};
            background: transparent;
        }}
        QLabel#capGlyph[listening="true"] {{
            color: {accent};
        }}
        QLabel#capHint {{
            color: {p['text_secondary']};
            font-size: 10px;
            background: transparent;
        }}

        /* Padded sub-panels so the manual dropdowns never look squished. */
        QFrame#subPanel {{
            background: {panel_bg};
            border: 1px solid {hairline};
            border-radius: 8px;
        }}
        QFrame#subPanel QLabel {{
            color: {p['text_secondary']};
            background: transparent;
        }}

        QPushButton#captureBtn {{
            font-weight: 600;
            letter-spacing: 1px;
            padding: 11px 16px;
            border-radius: 8px;
        }}
    """


class InputBindingEditor(QDialog):
    """Dialog for editing a single input binding"""
    
    def __init__(self, binding_data: Optional[Dict] = None, parent=None):
        super().__init__(parent)
        self.binding_data = binding_data or {}
        self.is_capturing = False
        self.captured_device_type = None
        self.captured_key = None
        self.captured_key_name = None

        # Live gamepad capture: poll the engine's windowless SDL controller poller
        # on a timer while capturing (Qt has no native gamepad events).
        self._gamepad_available = bool(_lupine_runtime and hasattr(_lupine_runtime, "gamepad_capture_poll"))
        self._gamepad_timer = QTimer(self)
        self._gamepad_timer.setInterval(40)
        self._gamepad_timer.timeout.connect(self._poll_gamepad)

        self.setWindowTitle("Edit Input Binding")
        self.setModal(True)
        # Width is fixed-ish; height is driven by content via the layout's
        # SetMinimumSize constraint (set in _setup_ui), so adding the gamepad
        # fields grows the window instead of crushing the dropdowns.
        self.setMinimumWidth(440)

        self._palette = _resolve_palette()
        self._setup_ui()
        self._load_binding()
        self._on_device_changed(self.device_combo.currentIndex())
    
    def _eyebrow(self, text: str) -> QLabel:
        """A quiet uppercase section label (replaces the heavy group-box title)."""
        label = QLabel(text.upper())
        label.setProperty("eyebrow", True)
        return label

    def _setup_ui(self):
        """Setup the dialog UI"""
        self.setStyleSheet(_dialog_qss(self._palette))

        layout = QVBoxLayout()
        layout.setContentsMargins(20, 18, 20, 18)
        layout.setSpacing(12)
        # Height tracks content: the window can never be shorter than the layout's
        # minimum, so revealing the gamepad fields grows it rather than squishing.
        layout.setSizeConstraint(QLayout.SizeConstraint.SetMinimumSize)

        # Header
        title = QLabel("Configure Input Binding")
        title.setObjectName("dlgTitle")
        layout.addWidget(title)

        # Device type selection
        layout.addWidget(self._eyebrow("Input Device"))
        self.device_combo = QComboBox()
        self.device_combo.addItems(["Keyboard", "Mouse", "Gamepad Button", "Gamepad Axis"])
        self.device_combo.currentIndexChanged.connect(self._on_device_changed)
        layout.addWidget(self.device_combo)

        # Capture target — the keycap. Pressing Detect, then any input, binds it.
        layout.addSpacing(2)
        self.capture_cap = QFrame()
        self.capture_cap.setObjectName("captureCap")
        cap_layout = QVBoxLayout(self.capture_cap)
        cap_layout.setContentsMargins(16, 18, 16, 16)
        cap_layout.setSpacing(6)

        self.binding_label = QLabel("No input assigned")
        self.binding_label.setObjectName("capGlyph")
        self.binding_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.binding_label.setWordWrap(True)
        self.binding_label.setMinimumHeight(40)
        cap_layout.addWidget(self.binding_label)

        self.capture_hint = QLabel("Press Detect, then the key, button or stick you want")
        self.capture_hint.setObjectName("capHint")
        self.capture_hint.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.capture_hint.setWordWrap(True)
        cap_layout.addWidget(self.capture_hint)

        layout.addWidget(self.capture_cap)

        self.capture_btn = QPushButton("Detect input")
        self.capture_btn.setObjectName("captureBtn")
        self.capture_btn.setMinimumHeight(44)
        self.capture_btn.clicked.connect(self._start_capture)
        layout.addWidget(self.capture_btn)

        # Mouse dropdown (manual override)
        self.mouse_dropdown_widget = QFrame()
        self.mouse_dropdown_widget.setObjectName("subPanel")
        mouse_layout = QFormLayout(self.mouse_dropdown_widget)
        mouse_layout.setContentsMargins(14, 12, 14, 12)
        mouse_layout.setSpacing(8)

        self.mouse_button_combo = QComboBox()
        for display, canonical in MOUSE_BUTTON_OPTIONS:
            self.mouse_button_combo.addItem(display, canonical)
        self.mouse_button_combo.currentIndexChanged.connect(self._on_mouse_button_changed)
        mouse_layout.addRow("Mouse Button", self.mouse_button_combo)

        self.mouse_dropdown_widget.hide()
        layout.addWidget(self.mouse_dropdown_widget)

        # Gamepad selection (manual override)
        self.gamepad_selection_widget = QFrame()
        self.gamepad_selection_widget.setObjectName("subPanel")
        gamepad_selection_layout = QFormLayout(self.gamepad_selection_widget)
        gamepad_selection_layout.setContentsMargins(14, 12, 14, 12)
        gamepad_selection_layout.setSpacing(8)

        self.gamepad_button_combo = QComboBox()
        for display, canonical in GAMEPAD_BUTTON_OPTIONS:
            self.gamepad_button_combo.addItem(display, canonical)
        self.gamepad_button_combo.currentIndexChanged.connect(self._on_gamepad_button_changed)
        self.gamepad_button_row_label = QLabel("Button")
        gamepad_selection_layout.addRow(self.gamepad_button_row_label, self.gamepad_button_combo)

        self.gamepad_axis_combo = QComboBox()
        for display, canonical in GAMEPAD_AXIS_OPTIONS:
            self.gamepad_axis_combo.addItem(display, canonical)
        self.gamepad_axis_combo.currentIndexChanged.connect(self._on_gamepad_axis_changed)
        self.gamepad_axis_row_label = QLabel("Axis")
        gamepad_selection_layout.addRow(self.gamepad_axis_row_label, self.gamepad_axis_combo)

        self.gamepad_id_spin = QSpinBox()
        self.gamepad_id_spin.setMinimum(0)
        self.gamepad_id_spin.setMaximum(7)
        self.gamepad_id_spin.setToolTip("0 = any gamepad, 1-7 = specific gamepad")
        gamepad_selection_layout.addRow("Gamepad ID", self.gamepad_id_spin)

        self.axis_scale_spin = QDoubleSpinBox()
        self.axis_scale_spin.setMinimum(-10.0)
        self.axis_scale_spin.setMaximum(10.0)
        self.axis_scale_spin.setValue(1.0)
        self.axis_scale_spin.setSingleStep(0.1)
        self.axis_scale_spin.setToolTip("Scale factor for axis (negative to invert)")
        self.axis_scale_row_label = QLabel("Axis Scale")
        gamepad_selection_layout.addRow(self.axis_scale_row_label, self.axis_scale_spin)

        self.gamepad_selection_widget.hide()
        layout.addWidget(self.gamepad_selection_widget)

        # gamepad_controls retained as an alias so existing show/hide logic keeps
        # working; the former separate controls panel now lives inside the
        # selection panel above, which keeps everything in one bordered group.
        self.gamepad_controls = self.gamepad_selection_widget

        layout.addStretch()

        # Footer hairline + actions
        footer_rule = QFrame()
        footer_rule.setObjectName("hairline")
        layout.addWidget(footer_rule)

        button_layout = QHBoxLayout()
        button_layout.addStretch()

        cancel_btn = QPushButton("Cancel")
        cancel_btn.setProperty("secondary", True)
        cancel_btn.clicked.connect(self.reject)
        button_layout.addWidget(cancel_btn)

        ok_btn = QPushButton("Save binding")
        ok_btn.setProperty("success", True)
        ok_btn.clicked.connect(self._on_accept)
        button_layout.addWidget(ok_btn)

        layout.addLayout(button_layout)

        self.setLayout(layout)
    
    @staticmethod
    def _select_combo(combo, canonical, order, legacy_index):
        """Select a combo entry by canonical item-data, falling back to a legacy
        integer index when older binding data has no (or a stale display) name."""
        idx = combo.findData(canonical) if canonical else -1
        if idx < 0 and legacy_index is not None and 0 <= legacy_index < len(order):
            idx = combo.findData(order[legacy_index])
        if idx >= 0:
            combo.setCurrentIndex(idx)

    def _load_binding(self):
        """Load existing binding data"""
        if not self.binding_data:
            self.binding_label.setText("No input assigned")
            return

        device_type = self.binding_data.get("deviceType", 0)
        self.device_combo.setCurrentIndex(device_type)

        if device_type == 0:  # Keyboard
            key_name = self.binding_data.get("keyName", "")
            self.captured_key_name = key_name
            self.captured_key = KEYNAME_TO_CODE.get(key_name, self.binding_data.get("keyCode", 0))
            self.binding_label.setText(f"Key: {key_name}")
        elif device_type == 1:  # Mouse
            self._select_combo(self.mouse_button_combo, self.binding_data.get("buttonName", ""),
                               MOUSE_BUTTON_ORDER, self.binding_data.get("mouseButton"))
            self.binding_label.setText(f"Mouse: {self.mouse_button_combo.currentText()}")
        elif device_type == 2 or device_type == 3:  # Gamepad button or axis
            if "axisName" in self.binding_data or "gamepadAxis" in self.binding_data:
                self.device_combo.setCurrentIndex(3)
                self._select_combo(self.gamepad_axis_combo, self.binding_data.get("axisName", ""),
                                   GAMEPAD_AXIS_ORDER, self.binding_data.get("gamepadAxis"))
                self.axis_scale_spin.setValue(self.binding_data.get("scale", 1.0))
                self.binding_label.setText(
                    f"Gamepad Axis: {self.gamepad_axis_combo.currentText()} (scale: {self.axis_scale_spin.value()})")
            else:
                self.device_combo.setCurrentIndex(2)
                self._select_combo(self.gamepad_button_combo, self.binding_data.get("buttonName", ""),
                                   GAMEPAD_BUTTON_ORDER, self.binding_data.get("gamepadButton"))
                self.binding_label.setText(f"Gamepad: {self.gamepad_button_combo.currentText()}")

            self.gamepad_id_spin.setValue(self.binding_data.get("gamepadID", 0))

    def _on_device_changed(self, index):
        """Handle device type change"""
        # The capture target applies to every device, so keep it visible.
        self.mouse_dropdown_widget.setVisible(index == 1)
        self.gamepad_selection_widget.setVisible(index >= 2)

        is_button = index == 2
        is_axis = index == 3
        # Button row only for Gamepad Button; axis + axis-scale rows only for Gamepad Axis.
        self.gamepad_button_row_label.setVisible(is_button)
        self.gamepad_button_combo.setVisible(is_button)
        self.gamepad_axis_row_label.setVisible(is_axis)
        self.gamepad_axis_combo.setVisible(is_axis)
        self.axis_scale_row_label.setVisible(is_axis)
        self.axis_scale_spin.setVisible(is_axis)

        if is_button:
            self._on_gamepad_button_changed()
        elif is_axis:
            self._on_gamepad_axis_changed()
        elif index == 1:  # Mouse
            self._on_mouse_button_changed()
        elif index == 0:  # Keyboard
            if self.captured_key_name:
                self.binding_label.setText(f"Key: {self.captured_key_name}")
            else:
                self.binding_label.setText("No input assigned")

        # Re-fit: the SetMinimumSize constraint guarantees content fits; adjustSize
        # also pulls the window back in when switching to a device with fewer fields.
        self.adjustSize()

    def _set_listening(self, listening: bool):
        """Toggle the keycap's accent 'listening' ring (a dynamic QSS property)."""
        for widget in (self.capture_cap, self.binding_label):
            widget.setProperty("listening", listening)
            widget.style().unpolish(widget)
            widget.style().polish(widget)

    def _start_capture(self):
        """Begin unified live capture (keyboard, mouse buttons and gamepad)."""
        self.is_capturing = True
        self._glyph_before_capture = self.binding_label.text()
        self.capture_btn.setText("Listening… press Esc to cancel")
        self.capture_btn.setEnabled(False)
        self.binding_label.setText("Listening…")
        self._set_listening(True)
        if self._gamepad_available:
            self.capture_hint.setText("Keyboard, mouse button or gamepad — press it now")
            self._gamepad_timer.start()
        else:
            self.capture_hint.setText(
                "Gamepad live-detect unavailable — use the Gamepad dropdowns below")
        self.setFocus()

    def _on_mouse_button_changed(self, *args):
        """Handle mouse button selection"""
        if self.mouse_button_combo.currentData():
            self.binding_label.setText(f"Mouse: {self.mouse_button_combo.currentText()}")

    def _on_gamepad_button_changed(self, *args):
        """Handle gamepad button selection"""
        if self.gamepad_button_combo.currentData():
            self.binding_label.setText(f"Gamepad: {self.gamepad_button_combo.currentText()}")

    def _on_gamepad_axis_changed(self, *args):
        """Handle gamepad axis selection"""
        if self.gamepad_axis_combo.currentData():
            scale = self.axis_scale_spin.value()
            self.binding_label.setText(f"Gamepad Axis: {self.gamepad_axis_combo.currentText()} (scale: {scale})")

    def _poll_gamepad(self):
        """Timer tick: poll the engine for a pressed gamepad button / deflected axis."""
        if not self.is_capturing or not self._gamepad_available:
            return
        try:
            result = _lupine_runtime.gamepad_capture_poll(0.6)
        except Exception:
            result = None
        if not result:
            return

        kind = result.get("kind")
        name = result.get("name")
        gamepad_id = int(result.get("gamepad_id", 0))
        if kind == "button":
            self.device_combo.setCurrentIndex(2)
            self._select_combo(self.gamepad_button_combo, name, GAMEPAD_BUTTON_ORDER, None)
            self.gamepad_id_spin.setValue(gamepad_id)
            self._on_gamepad_button_changed()
        elif kind == "axis":
            self.device_combo.setCurrentIndex(3)
            self._select_combo(self.gamepad_axis_combo, name, GAMEPAD_AXIS_ORDER, None)
            self.axis_scale_spin.setValue(float(result.get("scale", 1.0)))
            self.gamepad_id_spin.setValue(gamepad_id)
            self._on_gamepad_axis_changed()
        else:
            return
        self._stop_capture()

    def mousePressEvent(self, event: QMouseEvent):
        """Capture mouse-button input while listening."""
        if not self.is_capturing:
            super().mousePressEvent(event)
            return
        button_map = {
            Qt.MouseButton.LeftButton: "Left",
            Qt.MouseButton.RightButton: "Right",
            Qt.MouseButton.MiddleButton: "Middle",
            Qt.MouseButton.BackButton: "Button4",
            Qt.MouseButton.ForwardButton: "Button5",
        }
        canonical = button_map.get(event.button())
        if not canonical:
            super().mousePressEvent(event)
            return
        self.device_combo.setCurrentIndex(1)
        self._select_combo(self.mouse_button_combo, canonical, MOUSE_BUTTON_ORDER, None)
        self._on_mouse_button_changed()
        self._stop_capture()
        event.accept()

    def closeEvent(self, event):
        self._gamepad_timer.stop()
        super().closeEvent(event)

    def keyPressEvent(self, event: QKeyEvent):
        """Capture keyboard input while listening (any device mode auto-switches)."""
        if not self.is_capturing:
            super().keyPressEvent(event)
            return

        key = event.key()

        # Escape cancels capture without binding; restore the prior glyph.
        if key == Qt.Key.Key_Escape:
            self.binding_label.setText(getattr(self, "_glyph_before_capture", "No input assigned"))
            self._stop_capture()
            return

        key_name = self._qt_key_to_name(key)
        self.captured_key_name = key_name
        self.captured_key = KEYNAME_TO_CODE.get(key_name, 0)
        self.captured_device_type = 0
        self.device_combo.setCurrentIndex(0)
        self.binding_label.setText(f"Key: {key_name}")

        self._stop_capture()
        event.accept()

    def _stop_capture(self):
        """Stop capturing input"""
        self.is_capturing = False
        self._gamepad_timer.stop()
        self._set_listening(False)
        self.capture_hint.setText("Press Detect, then the key, button or stick you want")
        self.capture_btn.setText("Detect input")
        self.capture_btn.setEnabled(True)

    def _qt_key_to_name(self, key: int) -> str:
        """Convert a Qt key code to the engine-canonical key name (see InputCodes.cpp)."""
        key_map = {
            Qt.Key.Key_Space: "Space",
            Qt.Key.Key_Return: "Enter",
            Qt.Key.Key_Enter: "Enter",
            Qt.Key.Key_Escape: "Escape",
            Qt.Key.Key_Tab: "Tab",
            Qt.Key.Key_Backspace: "Backspace",
            Qt.Key.Key_Delete: "Delete",
            Qt.Key.Key_Insert: "Insert",
            Qt.Key.Key_Home: "Home",
            Qt.Key.Key_End: "End",
            Qt.Key.Key_PageUp: "PageUp",
            Qt.Key.Key_PageDown: "PageDown",
            Qt.Key.Key_Left: "Left",
            Qt.Key.Key_Right: "Right",
            Qt.Key.Key_Up: "Up",
            Qt.Key.Key_Down: "Down",
            Qt.Key.Key_Shift: "LeftShift",
            Qt.Key.Key_Control: "LeftControl",
            Qt.Key.Key_Alt: "LeftAlt",
            Qt.Key.Key_Meta: "LeftSuper",
            Qt.Key.Key_CapsLock: "CapsLock",
            Qt.Key.Key_Comma: "Comma",
            Qt.Key.Key_Period: "Period",
            Qt.Key.Key_Minus: "Minus",
            Qt.Key.Key_Equal: "Equal",
            Qt.Key.Key_Semicolon: "Semicolon",
            Qt.Key.Key_Slash: "Slash",
            Qt.Key.Key_Apostrophe: "Apostrophe",
            Qt.Key.Key_BracketLeft: "LeftBracket",
            Qt.Key.Key_BracketRight: "RightBracket",
            Qt.Key.Key_Backslash: "Backslash",
            Qt.Key.Key_QuoteLeft: "GraveAccent",
        }
        for n in range(1, 13):
            key_map[getattr(Qt.Key, f"Key_F{n}")] = f"F{n}"

        if key in key_map:
            return key_map[key]

        # Letters and digits map directly to their canonical engine names.
        if Qt.Key.Key_A <= key <= Qt.Key.Key_Z:
            return chr(key)
        if Qt.Key.Key_0 <= key <= Qt.Key.Key_9:
            return chr(key)

        return f"Key_{key}"

    def _on_accept(self):
        """Accept the dialog and build binding data"""
        device_index = self.device_combo.currentIndex()
        
        if device_index == 0:  # Keyboard
            if not self.captured_key_name:
                QMessageBox.warning(self, "No Input", "Please capture a key before saving.")
                return
            # keyName is the canonical engine name (the engine resolves it on load);
            # keyCode is stored too for completeness.
            self.binding_data = {
                "deviceType": 0,
                "keyCode": KEYNAME_TO_CODE.get(self.captured_key_name, self.captured_key or 0),
                "keyName": self.captured_key_name
            }

        elif device_index == 1:  # Mouse
            canonical = self.mouse_button_combo.currentData() or "Left"
            index = MOUSE_BUTTON_ORDER.index(canonical) if canonical in MOUSE_BUTTON_ORDER else 0
            self.binding_data = {
                "deviceType": 1,
                "mouseButton": index,
                "buttonName": canonical
            }

        elif device_index == 2:  # Gamepad Button
            canonical = self.gamepad_button_combo.currentData() or "A"
            index = GAMEPAD_BUTTON_ORDER.index(canonical) if canonical in GAMEPAD_BUTTON_ORDER else 0
            self.binding_data = {
                "deviceType": 2,
                "gamepadButton": index,
                "buttonName": canonical,
                "gamepadID": self.gamepad_id_spin.value()
            }

        elif device_index == 3:  # Gamepad Axis
            canonical = self.gamepad_axis_combo.currentData() or "LeftX"
            index = GAMEPAD_AXIS_ORDER.index(canonical) if canonical in GAMEPAD_AXIS_ORDER else 0
            self.binding_data = {
                "deviceType": 2,
                "gamepadAxis": index,
                "axisName": canonical,
                "scale": self.axis_scale_spin.value(),
                "gamepadID": self.gamepad_id_spin.value()
            }

        self.accept()
    
    def get_binding_data(self) -> Dict:
        """Get the configured binding data"""
        return self.binding_data


class InputMappingDialog(QDialog):
    """Dialog for managing input actions and their bindings"""
    
    input_map_changed = pyqtSignal(dict)  # Emits the complete input map
    
    def __init__(self, input_map_data: Optional[Dict] = None, parent=None):
        super().__init__(parent)
        self.input_map = input_map_data or {"actions": [], "axes": []}
        self.has_unsaved_changes = False
        self.current_action_index = None  # Track by index instead of reference
        
        self.setWindowTitle("Input Map")
        self.setModal(True)
        self.setMinimumSize(900, 700)

        self._palette = _resolve_palette()
        self._setup_ui()
        self._load_input_map()
    
    def _eyebrow(self, text: str) -> QLabel:
        """A quiet uppercase section label, matching the binding editor."""
        label = QLabel(text.upper())
        label.setProperty("eyebrow", True)
        return label

    def _tree_qss(self, card: bool = True) -> str:
        """Theme-derived styling for the trees. `card=True` paints the tree as its
        own recessed (darker) panel; `card=False` makes it transparent so it sits
        inside an enclosing card. Branch backgrounds are forced transparent in every
        state to remove the stray hover box/spine beside child binding rows."""
        p = self._palette
        bg = _card_bg(p) if card else "transparent"
        border = f"1px solid {_alpha(p['border'], 0.7)}" if card else "none"
        header_bg = _mix(p["surface"], "#000000", 0.2)
        # surface_hover sits too close to the recessed card tone to read, so the
        # hover wash is an accent-tinted lift — obvious, but lighter than the
        # solid selection bar so the two states stay distinct.
        hover_bg = _mix(_card_bg(p), p["accent_color"], 0.30)
        return f"""
            QTreeWidget {{
                background-color: {bg};
                border: {border};
                border-radius: 8px;
                padding: 4px;
                outline: none;
            }}
            QTreeWidget::item {{
                padding: 5px 4px;
                border-radius: 4px;
            }}
            QTreeWidget::item:hover {{
                background-color: {hover_bg};
            }}
            QTreeWidget::item:selected {{
                background-color: {p['selection']};
                color: {p['text_primary']};
            }}
            QTreeWidget::branch,
            QTreeWidget::branch:hover,
            QTreeWidget::branch:selected,
            QTreeWidget::branch:has-siblings,
            QTreeWidget::branch:!has-children {{
                background: transparent;
                border-image: none;
            }}
            QHeaderView::section {{
                background-color: {header_bg};
                color: {p['text_secondary']};
                border: none;
                border-bottom: 1px solid {_alpha(p['border'], 0.7)};
                padding: 5px 6px;
            }}
        """

    def _setup_ui(self):
        """Setup the dialog UI"""
        self.setStyleSheet(_dialog_qss(self._palette))

        layout = QVBoxLayout()
        layout.setContentsMargins(20, 18, 20, 18)
        layout.setSpacing(12)

        # Header: eyebrow + title on the left, one-line purpose on the right.
        header = QHBoxLayout()
        header.setSpacing(12)

        title_stack = QVBoxLayout()
        title_stack.setSpacing(2)
        title_stack.addWidget(self._eyebrow("Project Input"))
        title = QLabel("Input Map")
        title.setObjectName("dlgTitle")
        title_stack.addWidget(title)
        header.addLayout(title_stack)
        header.addStretch()

        help_label = QLabel("Define the named actions your game listens for, and the\n"
                            "keys, mouse buttons and gamepad inputs that trigger them.")
        help_label.setObjectName("dlgSubtitle")
        help_label.setAlignment(Qt.AlignmentFlag.AlignRight | Qt.AlignmentFlag.AlignVCenter)
        header.addWidget(help_label)
        layout.addLayout(header)

        header_rule = QFrame()
        header_rule.setObjectName("hairline")
        layout.addWidget(header_rule)

        # Main content splitter
        splitter = QSplitter(Qt.Orientation.Horizontal)
        splitter.setHandleWidth(16)
        splitter.setChildrenCollapsible(False)

        # Left side - Actions list
        actions_widget = QWidget()
        actions_layout = QVBoxLayout()
        actions_layout.setContentsMargins(0, 0, 0, 0)
        actions_layout.setSpacing(8)

        actions_header = QHBoxLayout()
        actions_header.addWidget(self._eyebrow("Actions"))
        actions_header.addStretch()

        add_action_btn = QPushButton("+ Action")
        add_action_btn.setProperty("success", True)
        add_action_btn.clicked.connect(self._add_action)
        actions_header.addWidget(add_action_btn)

        actions_layout.addLayout(actions_header)

        # Actions tree
        self.actions_tree = QTreeWidget()
        self.actions_tree.setHeaderLabels(["Action / Binding", "Device", "Input"])
        self.actions_tree.setColumnWidth(0, 250)
        self.actions_tree.setColumnWidth(1, 100)
        # Bindings are always shown expanded, so the disclosure/branch control is
        # pure noise — dropping it also removes the stray hover box beside the rows.
        self.actions_tree.setRootIsDecorated(False)
        self.actions_tree.setIndentation(14)
        self.actions_tree.setContextMenuPolicy(Qt.ContextMenuPolicy.CustomContextMenu)
        self.actions_tree.customContextMenuRequested.connect(self._show_context_menu)
        self.actions_tree.itemDoubleClicked.connect(self._edit_item)
        self.actions_tree.currentItemChanged.connect(self._on_tree_selection_changed)
        self.actions_tree.setStyleSheet(self._tree_qss(card=True))

        actions_layout.addWidget(self.actions_tree)

        actions_widget.setLayout(actions_layout)
        splitter.addWidget(actions_widget)

        # Right side - Action properties
        properties_widget = QWidget()
        properties_layout = QVBoxLayout()
        properties_layout.setContentsMargins(0, 0, 0, 0)
        properties_layout.setSpacing(8)

        properties_layout.addWidget(self._eyebrow("Action Properties"))

        # Properties form
        self.properties_group = QGroupBox("Settings")
        properties_form = QFormLayout()
        properties_form.setSpacing(8)

        self.action_name_edit = QLineEdit()
        self.action_name_edit.setPlaceholderText("e.g. jump, attack, move_forward")
        self.action_name_edit.textChanged.connect(self._on_property_changed)
        properties_form.addRow("Action Name:", self.action_name_edit)
        
        self.deadzone_spin = QDoubleSpinBox()
        self.deadzone_spin.setMinimum(0.0)
        self.deadzone_spin.setMaximum(1.0)
        self.deadzone_spin.setValue(0.5)
        self.deadzone_spin.setSingleStep(0.05)
        self.deadzone_spin.setToolTip("Minimum input value to register as pressed")
        self.deadzone_spin.valueChanged.connect(self._on_property_changed)
        properties_form.addRow("Deadzone:", self.deadzone_spin)

        # Context / action set: an empty context is always active; a named context
        # only contributes input while it is enabled at runtime (see
        # InputManager::EnableContext). Lets a game toggle whole groups of actions.
        self.context_edit = QLineEdit()
        self.context_edit.setPlaceholderText("(always active)")
        self.context_edit.setToolTip(
            "Action set this action belongs to. Empty = always active. A named "
            "context only fires while enabled at runtime (Input.enable_context).")
        self.context_edit.textChanged.connect(self._on_property_changed)
        properties_form.addRow("Context:", self.context_edit)

        self.enabled_check = QCheckBox("Enabled")
        self.enabled_check.setChecked(True)
        self.enabled_check.setToolTip("When unchecked, this action never registers as pressed.")
        self.enabled_check.stateChanged.connect(self._on_property_changed)
        properties_form.addRow("", self.enabled_check)

        self.properties_group.setLayout(properties_form)
        self.properties_group.setEnabled(False)
        properties_layout.addWidget(self.properties_group)
        
        # Bindings management
        bindings_group = QGroupBox("Bindings")
        bindings_layout = QVBoxLayout()
        
        # Bindings list to show current action's bindings
        self.bindings_list = QTreeWidget()
        self.bindings_list.setHeaderLabels(["Device", "Input"])
        self.bindings_list.setMaximumHeight(200)
        self.bindings_list.setRootIsDecorated(False)
        self.bindings_list.setSelectionMode(QTreeWidget.SelectionMode.SingleSelection)
        self.bindings_list.itemDoubleClicked.connect(lambda item, col: self._edit_binding())
        self.bindings_list.setStyleSheet(self._tree_qss(card=False))

        bindings_layout.addWidget(self.bindings_list)
        
        bindings_buttons = QHBoxLayout()
        
        add_binding_btn = QPushButton("+ Add Binding")
        add_binding_btn.setProperty("success", True)
        add_binding_btn.clicked.connect(self._add_binding)
        bindings_buttons.addWidget(add_binding_btn)
        
        edit_binding_btn = QPushButton("Edit")
        edit_binding_btn.clicked.connect(self._edit_binding)
        bindings_buttons.addWidget(edit_binding_btn)
        
        remove_binding_btn = QPushButton("Remove")
        remove_binding_btn.setProperty("danger", True)
        remove_binding_btn.clicked.connect(self._remove_binding)
        bindings_buttons.addWidget(remove_binding_btn)
        
        bindings_layout.addLayout(bindings_buttons)
        bindings_group.setLayout(bindings_layout)
        properties_layout.addWidget(bindings_group)
        
        properties_layout.addStretch()
        
        properties_widget.setLayout(properties_layout)
        splitter.addWidget(properties_widget)
        
        splitter.setStretchFactor(0, 2)
        splitter.setStretchFactor(1, 1)
        
        layout.addWidget(splitter, 1)

        footer_rule = QFrame()
        footer_rule.setObjectName("hairline")
        layout.addWidget(footer_rule)

        # Bottom buttons
        button_layout = QHBoxLayout()

        reset_btn = QPushButton("Reset to Default")
        reset_btn.setProperty("secondary", True)
        reset_btn.clicked.connect(self._reset_to_default)
        button_layout.addWidget(reset_btn)

        button_layout.addStretch()

        close_btn = QPushButton("Close")
        close_btn.setProperty("secondary", True)
        close_btn.clicked.connect(self._on_close)
        button_layout.addWidget(close_btn)

        save_btn = QPushButton("Save")
        save_btn.setProperty("success", True)
        save_btn.clicked.connect(self._on_save)
        button_layout.addWidget(save_btn)

        layout.addLayout(button_layout)

        self.setLayout(layout)
    
    def _load_input_map(self):
        """Load input map into the tree"""
        # Store current selection by index (more reliable than object reference)
        selected_action_index = self.current_action_index

        self.actions_tree.clear()

        # Load actions
        for i, action in enumerate(self.input_map.get("actions", [])):
            action_item = QTreeWidgetItem(self.actions_tree)
            action_item.setText(0, action.get("name", "Unnamed"))
            action_item.setData(0, Qt.ItemDataRole.UserRole, {"type": "action", "data": action, "index": i})
            action_item.setExpanded(True)

            # Add bindings as children
            for binding in action.get("bindings", []):
                binding_item = QTreeWidgetItem(action_item)
                device_name, input_name = self._get_binding_display_names(binding)
                binding_item.setText(0, "  → Binding")
                binding_item.setText(1, device_name)
                binding_item.setText(2, input_name)
                binding_item.setData(0, Qt.ItemDataRole.UserRole, {"type": "binding", "data": binding, "parent_action": action, "parent_index": i})

        # Restore selection by index
        if selected_action_index is not None and selected_action_index < self.actions_tree.topLevelItemCount():
            item_to_select = self.actions_tree.topLevelItem(selected_action_index)
            if item_to_select:
                self.actions_tree.setCurrentItem(item_to_select)
                self._load_action_properties(item_to_select)
    
    def _on_tree_selection_changed(self, current, previous):
        """Handle tree selection change"""
        if not current:
            self.properties_group.setEnabled(False)
            self.current_action_index = None
            return

        item_data = current.data(0, Qt.ItemDataRole.UserRole)
        if not item_data:
            self.properties_group.setEnabled(False)
            self.current_action_index = None
            return

        if item_data.get("type") == "action":
            self.current_action_index = item_data.get("index")
            self._load_action_properties(current)
        elif item_data.get("type") == "binding":
            # If a binding is selected, load the parent action's properties
            self.current_action_index = item_data.get("parent_index")
            parent = current.parent()
            if parent:
                parent_data = parent.data(0, Qt.ItemDataRole.UserRole)
                if parent_data and parent_data.get("type") == "action":
                    self._load_action_properties(parent)
            else:
                self.properties_group.setEnabled(False)
                self.current_action_index = None
    
    def _add_action(self):
        """Add a new action"""
        # Generate a unique action name
        base_name = "new_action"
        action_names = {action.get("name", "") for action in self.input_map.get("actions", [])}
        
        counter = 1
        new_name = base_name
        while new_name in action_names:
            new_name = f"{base_name}_{counter}"
            counter += 1
        
        # Create a new action with default values
        new_action = {
            "name": new_name,
            "deadzone": 0.5,
            "context": "",
            "enabled": True,
            "bindings": []
        }
        
        self.input_map.setdefault("actions", []).append(new_action)
        self._mark_unsaved()
        self._load_input_map()
        
        # Select the new action
        root = self.actions_tree.invisibleRootItem()
        if root.childCount() > 0:
            last_item = root.child(root.childCount() - 1)
            self.actions_tree.setCurrentItem(last_item)
            self._load_action_properties(last_item)
    
    def _add_binding(self):
        """Add a new binding to the selected action"""
        if self.current_action_index is None:
            QMessageBox.information(self, "No Selection", "Please select an action to add a binding.")
            return

        # Get the action data directly from input_map using current_action_index
        action_data = self.input_map["actions"][self.current_action_index]

        # Open binding editor
        dialog = InputBindingEditor(parent=self)
        if dialog.exec() == QDialog.DialogCode.Accepted:
            binding_data = dialog.get_binding_data()

            # Add binding to action
            if "bindings" not in action_data:
                action_data["bindings"] = []
            action_data["bindings"].append(binding_data)

            self._mark_unsaved()

            # Reload the tree to show the new binding (selection will be restored automatically)
            self._load_input_map()
    
    def _edit_binding(self):
        """Edit the selected binding"""
        binding_data = None

        # Try to get binding from the bindings_list first
        binding_item = self.bindings_list.currentItem()
        if binding_item:
            binding_data = binding_item.data(0, Qt.ItemDataRole.UserRole)
        else:
            # Fall back to tree selection
            current = self.actions_tree.currentItem()
            if current:
                item_data = current.data(0, Qt.ItemDataRole.UserRole)
                if item_data and item_data.get("type") == "binding":
                    binding_data = item_data["data"]

        if not binding_data:
            QMessageBox.information(self, "No Selection", "Please select a binding to edit.")
            return

        # Open binding editor with existing data
        dialog = InputBindingEditor(binding_data.copy(), parent=self)
        if dialog.exec() == QDialog.DialogCode.Accepted:
            updated_binding = dialog.get_binding_data()
            # Update the binding in place
            binding_data.clear()
            binding_data.update(updated_binding)
            self._mark_unsaved()
            self._load_input_map()
    
    def _remove_binding(self):
        """Remove the selected binding"""
        if self.current_action_index is None:
            QMessageBox.information(self, "No Selection", "Please select an action first.")
            return

        binding_data = None

        # Try to get binding from the bindings_list first
        binding_item = self.bindings_list.currentItem()
        if binding_item:
            binding_data = binding_item.data(0, Qt.ItemDataRole.UserRole)
        else:
            # Fall back to tree selection
            current = self.actions_tree.currentItem()
            if current:
                item_data = current.data(0, Qt.ItemDataRole.UserRole)
                if item_data and item_data.get("type") == "binding":
                    binding_data = item_data["data"]

        if not binding_data:
            QMessageBox.information(self, "No Selection", "Please select a binding to remove.")
            return

        reply = QMessageBox.question(self, "Remove Binding",
                                     "Are you sure you want to remove this binding?",
                                     QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No)

        if reply == QMessageBox.StandardButton.Yes:
            action_data = self.input_map["actions"][self.current_action_index]
            if binding_data in action_data.get("bindings", []):
                action_data["bindings"].remove(binding_data)
                self._mark_unsaved()
                self._load_input_map()
    
    def _edit_item(self, item, column):
        """Handle double-click on item"""
        item_data = item.data(0, Qt.ItemDataRole.UserRole)
        if item_data.get("type") == "action":
            self._load_action_properties(item)
        elif item_data.get("type") == "binding":
            self._edit_binding()
    
    def _load_action_properties(self, item):
        """Load action properties into the properties panel"""
        if not item:
            self.bindings_list.clear()
            self.properties_group.setEnabled(False)
            return

        item_data = item.data(0, Qt.ItemDataRole.UserRole)
        if not item_data or item_data.get("type") != "action":
            self.properties_group.setEnabled(False)
            self.bindings_list.clear()
            return

        # Get the action index from item data (already set by _on_tree_selection_changed)
        action_index = item_data.get("index")
        if action_index is None or action_index >= len(self.input_map.get("actions", [])):
            self.properties_group.setEnabled(False)
            self.bindings_list.clear()
            return

        action_data = self.input_map["actions"][action_index]
        self.properties_group.setEnabled(True)

        # Disconnect signal temporarily to prevent recursive updates
        self.action_name_edit.textChanged.disconnect(self._on_property_changed)
        self.deadzone_spin.valueChanged.disconnect(self._on_property_changed)
        self.context_edit.textChanged.disconnect(self._on_property_changed)
        self.enabled_check.stateChanged.disconnect(self._on_property_changed)

        self.action_name_edit.setText(action_data.get("name", ""))
        self.deadzone_spin.setValue(action_data.get("deadzone", 0.5))
        self.context_edit.setText(action_data.get("context", ""))
        self.enabled_check.setChecked(action_data.get("enabled", True))

        # Reconnect signals
        self.action_name_edit.textChanged.connect(self._on_property_changed)
        self.deadzone_spin.valueChanged.connect(self._on_property_changed)
        self.context_edit.textChanged.connect(self._on_property_changed)
        self.enabled_check.stateChanged.connect(self._on_property_changed)

        # Populate bindings list
        self.bindings_list.clear()
        for binding in action_data.get("bindings", []):
            device_name, input_name = self._get_binding_display_names(binding)
            binding_item = QTreeWidgetItem(self.bindings_list)
            binding_item.setText(0, device_name)
            binding_item.setText(1, input_name)
            binding_item.setData(0, Qt.ItemDataRole.UserRole, binding)
    
    def _on_property_changed(self):
        """Handle property change"""
        if self.current_action_index is None:
            return

        # Update the action data directly in the input_map
        new_name = self.action_name_edit.text()
        new_deadzone = self.deadzone_spin.value()

        action = self.input_map["actions"][self.current_action_index]
        action["name"] = new_name
        action["deadzone"] = new_deadzone
        action["context"] = self.context_edit.text()
        action["enabled"] = self.enabled_check.isChecked()

        # Update tree item text directly without full reload
        current = self.actions_tree.currentItem()
        if current:
            item_data = current.data(0, Qt.ItemDataRole.UserRole)
            if item_data and item_data.get("type") == "action":
                current.setText(0, new_name)

        self._mark_unsaved()
    
    def _show_context_menu(self, position):
        """Show context menu for tree items"""
        item = self.actions_tree.itemAt(position)
        if not item:
            return
        
        menu = QMenu()
        
        item_data = item.data(0, Qt.ItemDataRole.UserRole)
        if item_data.get("type") == "action":
            add_binding_action = menu.addAction("Add Binding")
            add_binding_action.triggered.connect(lambda: self._context_add_binding(item))
            
            menu.addSeparator()
            
            duplicate_action = menu.addAction("Duplicate Action")
            duplicate_action.triggered.connect(lambda: self._duplicate_action(item))
            
            remove_action = menu.addAction("Remove Action")
            remove_action.triggered.connect(lambda: self._remove_action(item))
        elif item_data.get("type") == "binding":
            edit_action = menu.addAction("Edit Binding")
            edit_action.triggered.connect(self._edit_binding)
            
            remove_action = menu.addAction("Remove Binding")
            remove_action.triggered.connect(self._remove_binding)
        
        menu.exec(self.actions_tree.mapToGlobal(position))
    
    def _context_add_binding(self, item):
        """Add binding from context menu"""
        self.actions_tree.setCurrentItem(item)
        self._add_binding()
    
    def _duplicate_action(self, item):
        """Duplicate an action"""
        item_data = item.data(0, Qt.ItemDataRole.UserRole)
        if item_data.get("type") != "action":
            return
        
        import copy
        action_data = copy.deepcopy(item_data["data"])
        action_data["name"] = action_data["name"] + "_copy"
        
        self.input_map.setdefault("actions", []).append(action_data)
        self._mark_unsaved()
        self._load_input_map()
    
    def _remove_action(self, item):
        """Remove an action"""
        item_data = item.data(0, Qt.ItemDataRole.UserRole)
        if item_data.get("type") != "action":
            return
        
        action_data = item_data["data"]
        action_name = action_data.get("name", "Unknown")
        
        reply = QMessageBox.question(self, "Remove Action",
                                     f"Are you sure you want to remove the action '{action_name}'?",
                                     QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No)
        
        if reply == QMessageBox.StandardButton.Yes:
            self.input_map["actions"].remove(action_data)
            self._mark_unsaved()
            self._load_input_map()
    
    def _reset_to_default(self):
        """Reset input map to default"""
        reply = QMessageBox.question(self, "Reset to Default",
                                     "This will reset all input actions to default values. Continue?",
                                     QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No)
        
        if reply == QMessageBox.StandardButton.Yes:
            self.input_map = self._get_default_input_map()
            self._load_input_map()
            self._mark_unsaved()
    
    def _get_default_input_map(self) -> Dict:
        """Get default input map"""
        return {
            "actions": [
                {
                    "name": "ui_accept",
                    "deadzone": 0.5,
                    "bindings": [
                        {"deviceType": 0, "keyCode": 257, "keyName": "Enter"},
                        {"deviceType": 0, "keyCode": 32, "keyName": "Space"}
                    ]
                },
                {
                    "name": "ui_cancel",
                    "deadzone": 0.5,
                    "bindings": [
                        {"deviceType": 0, "keyCode": 256, "keyName": "Escape"}
                    ]
                },
                {
                    "name": "jump",
                    "deadzone": 0.5,
                    "bindings": [
                        {"deviceType": 0, "keyCode": 32, "keyName": "Space"}
                    ]
                }
            ],
            "axes": []
        }
    
    def _mark_unsaved(self):
        """Mark that there are unsaved changes"""
        self.has_unsaved_changes = True
        self.setWindowTitle("Input Map *")
    
    def _get_binding_display_names(self, binding: Dict) -> tuple:
        """Get display names for a binding.

        Prefers the canonical name stored on the binding (keyName/buttonName/
        axisName, what the engine actually resolves) and falls back to the
        engine-correct integer-index order for legacy bindings."""
        device_type = binding.get("deviceType", 0)

        if device_type == 0:  # Keyboard
            return "Keyboard", binding.get("keyName", "Unknown")

        if device_type == 1:  # Mouse
            canonical = binding.get("buttonName")
            if not canonical:
                idx = binding.get("mouseButton", 0)
                canonical = MOUSE_BUTTON_ORDER[idx] if 0 <= idx < len(MOUSE_BUTTON_ORDER) else None
            return "Mouse", MOUSE_BUTTON_DISPLAY.get(canonical, canonical or "Unknown")

        if device_type in (2, 3):  # Gamepad button or axis
            if "axisName" in binding or "gamepadAxis" in binding:
                canonical = binding.get("axisName")
                if not canonical:
                    idx = binding.get("gamepadAxis", 0)
                    canonical = GAMEPAD_AXIS_ORDER[idx] if 0 <= idx < len(GAMEPAD_AXIS_ORDER) else None
                scale = binding.get("scale", 1.0)
                label = GAMEPAD_AXIS_DISPLAY.get(canonical, canonical or "Unknown")
                if scale < 0:
                    label += " (-)"
                return "Gamepad Axis", label

            canonical = binding.get("buttonName")
            if not canonical:
                idx = binding.get("gamepadButton", 0)
                canonical = GAMEPAD_BUTTON_ORDER[idx] if 0 <= idx < len(GAMEPAD_BUTTON_ORDER) else None
            return "Gamepad", GAMEPAD_BUTTON_DISPLAY.get(canonical, canonical or "Unknown")

        return "Unknown", "Unknown"
    
    def _on_save(self):
        """Save the input map"""
        self.input_map_changed.emit(self.input_map)
        self.has_unsaved_changes = False
        self.setWindowTitle("Input Map")
        QMessageBox.information(self, "Saved", "Input map saved successfully!")
    
    def _on_close(self):
        """Handle close button"""
        if self.has_unsaved_changes:
            reply = QMessageBox.question(self, "Unsaved Changes",
                                        "You have unsaved changes. Close without saving?",
                                        QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No)
            if reply == QMessageBox.StandardButton.No:
                return
        
        self.accept()
    
    def get_input_map(self) -> Dict:
        """Get the current input map"""
        return self.input_map
