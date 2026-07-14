"""
Inspector visual language.

Centralizes the colors, sizing tokens and small styled-widget factories used by the
property inspector so every property row, section and component card shares one coherent
look. Every color here is derived from the active editor Theme - nothing is hardcoded -
so switching or editing themes restyles the inspector automatically.
"""

from __future__ import annotations

import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Optional, Tuple

from PyQt6.QtWidgets import (QFrame, QLabel, QPushButton, QVBoxLayout, QHBoxLayout,
                             QWidget, QSizePolicy)
from PyQt6.QtCore import Qt, pyqtSignal

sys.path.insert(0, str(Path(__file__).parent.parent))
from theme import get_theme_manager


_DARK_FALLBACK = {
    "background": "#16131d",
    "surface": "#1e1a28",
    "surface_hover": "#28233a",
    "tertiary_color": "#2a2537",
    "border": "#3d3650",
    "border_focus": "#7b5fb8",
    "accent_color": "#7b5fb8",
    "accent_hover": "#8f6fcc",
    "secondary_accent_color": "#4a3f68",
    "text_primary": "#e8e6f0",
    "text_secondary": "#b0a8c0",
    "text_disabled": "#6a6178",
    "text_on_accent": "#ffffff",
    "error_color": "#d32f2f",
}


def _clamp(value: float) -> int:
    return max(0, min(255, int(round(value))))


def _to_rgb(hex_str: str) -> Tuple[int, int, int]:
    """Parse '#rgb', '#rrggbb' or '#rrggbbaa'/'#aarrggbb'-ish strings to an (r, g, b) tuple."""
    h = hex_str.lstrip("#")
    if len(h) == 3:
        h = "".join(ch * 2 for ch in h)
    if len(h) >= 6:
        h = h[:6]
    return int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16)


def mix(hex_a: str, hex_b: str, t: float) -> str:
    """Linear blend of two hex colors; t=0 returns a, t=1 returns b."""
    ar, ag, ab = _to_rgb(hex_a)
    br, bg, bb = _to_rgb(hex_b)
    return "#{:02x}{:02x}{:02x}".format(
        _clamp(ar + (br - ar) * t),
        _clamp(ag + (bg - ag) * t),
        _clamp(ab + (bb - ab) * t),
    )


def with_alpha(hex_str: str, alpha: float) -> str:
    """Return a Qt 'rgba(...)' color string for the given hex at alpha (0..1)."""
    r, g, b = _to_rgb(hex_str)
    return f"rgba({r}, {g}, {b}, {max(0.0, min(1.0, alpha)):.3f})"


def lighten(hex_str: str, amount: float) -> str:
    return mix(hex_str, "#ffffff", amount)


def darken(hex_str: str, amount: float) -> str:
    return mix(hex_str, "#000000", amount)


@dataclass(frozen=True)
class InspectorTokens:
    """Resolved color + sizing tokens for the current theme."""
    background: str
    surface: str
    surface_hover: str
    tertiary: str
    border: str
    border_focus: str
    accent: str
    accent_hover: str
    accent_muted: str
    text_primary: str
    text_secondary: str
    text_disabled: str
    text_on_accent: str
    error: str

    label_width: int
    control_height: int
    card_radius: int
    spine_width: int
    row_v_pad: int
    h_gap: int

    # Derived washes used repeatedly across the inspector.
    row_hover: str
    header_bg: str
    hairline: str


def tokens() -> InspectorTokens:
    """Resolve the current theme into inspector design tokens (with safe fallbacks)."""
    theme = get_theme_manager().get_current_theme()
    if theme is not None:
        c = theme.colors

        def get(name: str) -> str:
            return getattr(c, name, _DARK_FALLBACK.get(name, "#000000"))

        label_width = int(getattr(c, "inspector_label_width", 128))
        control_height = int(getattr(c, "inspector_control_height", 26))
        card_radius = int(getattr(c, "inspector_card_radius", 6))
        spine_width = int(getattr(c, "inspector_spine_width", 3))
        row_v_pad = max(1, int(getattr(c, "inspector_vertical_spacing", 8)) // 2)
        h_gap = int(getattr(c, "inspector_horizontal_padding", 6))
    else:
        get = _DARK_FALLBACK.get  # type: ignore[assignment]
        label_width, control_height, card_radius, spine_width, row_v_pad, h_gap = 128, 26, 6, 3, 4, 6

    surface = get("surface")
    accent = get("accent_color")
    return InspectorTokens(
        background=get("background"),
        surface=surface,
        surface_hover=get("surface_hover"),
        tertiary=get("tertiary_color"),
        border=get("border"),
        border_focus=get("border_focus"),
        accent=accent,
        accent_hover=get("accent_hover"),
        accent_muted=get("secondary_accent_color"),
        text_primary=get("text_primary"),
        text_secondary=get("text_secondary"),
        text_disabled=get("text_disabled"),
        text_on_accent=get("text_on_accent"),
        error=get("error_color"),
        label_width=label_width,
        control_height=control_height,
        card_radius=card_radius,
        spine_width=spine_width,
        row_v_pad=row_v_pad,
        h_gap=h_gap,
        row_hover=with_alpha(get("surface_hover"), 0.5),
        header_bg=mix(surface, accent, 0.08),
        hairline=with_alpha(get("border"), 0.6),
    )


def make_property_label(text: str, tooltip: Optional[str] = None) -> QLabel:
    """Build the left-column label for a property row: muted, fixed width, full-name tooltip."""
    t = tokens()
    label = QLabel(text)
    label.setFixedWidth(t.label_width)
    label.setSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Fixed)
    label.setAlignment(Qt.AlignmentFlag.AlignLeft | Qt.AlignmentFlag.AlignVCenter)
    label.setToolTip(tooltip if tooltip is not None else text)
    label.setStyleSheet(
        f"QLabel {{ color: {t.text_secondary}; background: transparent; padding-right: 4px; }}"
    )
    return label


def reset_button_qss(t: Optional[InspectorTokens] = None) -> str:
    """Quiet revert button - invisible until its row is hovered, soft tint on hover."""
    t = t or tokens()
    return f"""
        QPushButton {{
            background-color: transparent;
            border: none;
            border-radius: {t.control_height // 2}px;
        }}
        QPushButton:hover {{
            background-color: {t.surface_hover};
        }}
        QPushButton:pressed {{
            background-color: {t.accent_muted};
        }}
    """


def style_icon_button(button: QPushButton, *, danger: bool = False) -> None:
    """Style a small flat header glyph button (e.g. remove)."""
    t = tokens()
    hover_bg = t.error if danger else t.surface_hover
    hover_fg = t.text_on_accent if danger else t.text_primary
    button.setStyleSheet(f"""
        QPushButton {{
            background-color: transparent;
            color: {t.text_secondary};
            border: none;
            border-radius: 4px;
            font-size: 13px;
            font-weight: bold;
            padding: 0px;
        }}
        QPushButton:hover {{
            background-color: {hover_bg};
            color: {hover_fg};
        }}
    """)


class InspectorCard(QFrame):
    """A titled, collapsible container for an inspector section.

    Renders a header bar (collapse chevron, optional eyebrow, title, trailing controls)
    above a body that carries a left accent spine. Add property rows with `add_body_widget`
    and header controls (enable toggle, remove button) with `add_header_widget`.
    """

    toggled = pyqtSignal(bool)  # emitted with the new "expanded" state

    def __init__(self, title: str, *, eyebrow: Optional[str] = None,
                 collapsible: bool = True, spine: bool = True, parent=None):
        super().__init__(parent)
        self._expanded = True
        self._collapsible = collapsible
        t = tokens()
        self.setObjectName("inspectorCard")
        self.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Preferred)
        self.setStyleSheet(
            f"QFrame#inspectorCard {{ background-color: {t.surface};"
            f" border: 1px solid {t.border}; border-radius: {t.card_radius}px; }}"
        )

        outer = QVBoxLayout(self)
        outer.setContentsMargins(0, 0, 0, 0)
        outer.setSpacing(0)

        # Header bar -------------------------------------------------------
        self._header = QFrame()
        self._header.setObjectName("cardHeader")
        self._header.setStyleSheet(f"""
            QFrame#cardHeader {{
                background-color: {t.header_bg};
                border-top-left-radius: {t.card_radius}px;
                border-top-right-radius: {t.card_radius}px;
                border-bottom: 1px solid {t.border};
            }}
        """)
        if self._collapsible:
            self._header.setCursor(Qt.CursorShape.PointingHandCursor)
            self._header.mousePressEvent = self._on_header_clicked  # type: ignore[assignment]

        header_layout = QHBoxLayout(self._header)
        header_layout.setContentsMargins(8, 6, 6, 6)
        header_layout.setSpacing(6)

        self._chevron = QLabel("▾" if self._expanded else "▸")
        self._chevron.setFixedWidth(12)
        self._chevron.setStyleSheet(
            f"QLabel {{ color: {t.text_secondary}; background: transparent; font-size: 10px; }}")
        header_layout.addWidget(self._chevron)
        if not self._collapsible:
            self._chevron.hide()

        title_box = QVBoxLayout()
        title_box.setContentsMargins(0, 0, 0, 0)
        title_box.setSpacing(0)
        if eyebrow:
            eb = QLabel(eyebrow.upper())
            eb.setStyleSheet(
                f"QLabel {{ color: {with_alpha(t.text_secondary, 0.75)}; background: transparent;"
                f" font-size: 8px; font-weight: bold; letter-spacing: 1px; }}")
            title_box.addWidget(eb)
        self._title = QLabel(title)
        self._title.setStyleSheet(
            f"QLabel {{ color: {t.text_primary}; background: transparent;"
            f" font-size: 12px; font-weight: 600; letter-spacing: 0.3px; }}")
        title_box.addWidget(self._title)
        header_layout.addLayout(title_box)
        header_layout.addStretch()

        self._header_trailing = header_layout

        outer.addWidget(self._header)

        # Body with accent spine ------------------------------------------
        self._body = QFrame()
        self._body.setObjectName("cardBody")
        spine_rule = (f"border-left: {t.spine_width}px solid {t.accent};"
                      if spine else "")
        self._body.setStyleSheet(f"""
            QFrame#cardBody {{
                background-color: {t.surface};
                {spine_rule}
                border-bottom-left-radius: {t.card_radius}px;
                border-bottom-right-radius: {t.card_radius}px;
            }}
        """)
        self.body_layout = QVBoxLayout(self._body)
        self.body_layout.setContentsMargins(8, 6, 8, 8)
        self.body_layout.setSpacing(t.row_v_pad)
        outer.addWidget(self._body)

    def add_header_widget(self, widget: QWidget) -> None:
        """Add a control (enable toggle, remove button, ...) to the right of the header."""
        self._header_trailing.addWidget(widget)

    def add_body_widget(self, widget: QWidget) -> None:
        self.body_layout.addWidget(widget)

    def add_body_layout(self, layout) -> None:
        self.body_layout.addLayout(layout)

    def set_expanded(self, expanded: bool) -> None:
        self._expanded = expanded
        self._body.setVisible(expanded)
        self._chevron.setText("▾" if expanded else "▸")

    def _on_header_clicked(self, event) -> None:
        if event.button() == Qt.MouseButton.LeftButton:
            self.set_expanded(not self._expanded)
            self.toggled.emit(self._expanded)
