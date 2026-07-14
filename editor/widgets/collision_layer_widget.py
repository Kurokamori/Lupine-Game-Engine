"""
Collision Layer Property Widget

A widget for selecting collision layers.
Shows a grid of checkboxes with layer names that can be configured in project settings.
"""

from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel,
                              QPushButton, QFrame, QGridLayout, QCheckBox,
                              QSizePolicy, QMenu, QWidgetAction, QScrollArea)
from PyQt6.QtCore import Qt, pyqtSignal, QSize
from PyQt6.QtGui import QFont
import sys
from pathlib import Path

# Add parent directory to path to import theme
sys.path.insert(0, str(Path(__file__).parent.parent))
from theme import get_theme_manager


def get_theme_spacing():
    """Get spacing values from theme"""
    theme = get_theme_manager().get_current_theme()
    if theme:
        return theme.colors.inspector_vertical_spacing, theme.colors.inspector_horizontal_padding
    return 8, 6  # defaults


class CollisionLayerPropertyWidget(QWidget):
    """Widget for editing collision layer bitmask properties"""
    value_changed = pyqtSignal(object)  # new value (int bitmask)

    def __init__(self, property_name, default_value=1, is_3d=False, layer_names=None, parent=None):
        super().__init__(parent)
        self.property_name = property_name
        self.default_value = default_value
        self.is_3d = is_3d
        self.layer_names = layer_names or [f"Layer {i+1}" for i in range(32)]
        self._current_value = default_value
        self.reset_button = None
        self.setup_ui()

    def setup_ui(self):
        """Setup the widget UI"""
        layout = QHBoxLayout()
        v_spacing, h_padding = get_theme_spacing()
        layout.setContentsMargins(0, v_spacing // 2, 0, v_spacing // 2)
        layout.setSpacing(h_padding)

        # Get theme colors
        theme = get_theme_manager().get_current_theme()
        bg_color = theme.colors.surface if theme else "#1e1a28"
        text_color = theme.colors.text_primary if theme else "#ffffff"
        border_color = theme.colors.border if theme else "#3d3650"
        accent_color = theme.colors.accent_color if theme else "#8b5cf6"

        # Label
        self.label = QLabel(self.property_name)
        self.label.setFixedWidth(120)
        self.label.setSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Fixed)
        layout.addWidget(self.label)

        # Button that shows current layer selection and opens popup
        self.layer_button = QPushButton()
        self.layer_button.setMinimumHeight(28)
        self.layer_button.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)
        self.layer_button.clicked.connect(self._show_layer_popup)
        self._update_button_text()

        self.layer_button.setStyleSheet(f"""
            QPushButton {{
                background-color: {bg_color};
                color: {text_color};
                border: 1px solid {border_color};
                border-radius: 4px;
                padding: 4px 8px;
                text-align: left;
            }}
            QPushButton:hover {{
                border-color: {accent_color};
            }}
        """)

        layout.addWidget(self.layer_button)
        layout.addWidget(self._create_reset_button())
        self.setLayout(layout)

    def _create_reset_button(self):
        """Helper to create a reset button"""
        reset_btn = QPushButton("\u27f2")  # Unicode circular arrow
        reset_btn.setFixedSize(24, 24)
        reset_btn.setToolTip("Reset to default value")
        reset_btn.clicked.connect(self.reset_to_default)

        theme = get_theme_manager().get_current_theme()
        if theme:
            reset_btn.setStyleSheet(f"""
                QPushButton {{
                    background-color: {theme.colors.surface};
                    color: {theme.colors.accent_color};
                    border: 1px solid {theme.colors.border};
                    border-radius: 4px;
                    font-size: 16px;
                    font-weight: bold;
                    padding: 0px;
                }}
                QPushButton:hover {{
                    background-color: {theme.colors.surface_hover};
                    border-color: {theme.colors.accent_color};
                }}
            """)

        self.reset_button = reset_btn
        return reset_btn

    def _show_layer_popup(self):
        """Show the layer selection popup"""
        menu = QMenu(self)

        # Get theme colors
        theme = get_theme_manager().get_current_theme()
        bg_color = theme.colors.surface if theme else "#1e1a28"
        text_color = theme.colors.text_primary if theme else "#ffffff"
        border_color = theme.colors.border if theme else "#3d3650"
        accent_color = theme.colors.accent_color if theme else "#8b5cf6"

        menu.setStyleSheet(f"""
            QMenu {{
                background-color: {bg_color};
                border: 1px solid {border_color};
                padding: 8px;
            }}
        """)

        # Create popup widget
        popup_widget = QWidget()
        popup_layout = QVBoxLayout(popup_widget)
        popup_layout.setContentsMargins(8, 8, 8, 8)
        popup_layout.setSpacing(4)

        # Title
        title = QLabel("Collision Layers" + (" (3D)" if self.is_3d else " (2D)"))
        title.setStyleSheet(f"color: {text_color}; font-weight: bold; padding-bottom: 4px;")
        popup_layout.addWidget(title)

        # Scroll area for checkboxes
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setMaximumHeight(300)
        scroll.setStyleSheet(f"""
            QScrollArea {{
                background-color: transparent;
                border: none;
            }}
            QScrollBar:vertical {{
                background-color: {bg_color};
                width: 8px;
            }}
            QScrollBar::handle:vertical {{
                background-color: {border_color};
                border-radius: 4px;
            }}
        """)

        # Grid of checkboxes (8 columns x 4 rows = 32 layers)
        grid_widget = QWidget()
        grid_layout = QGridLayout(grid_widget)
        grid_layout.setSpacing(4)
        grid_layout.setContentsMargins(0, 0, 0, 0)

        self.checkboxes = []
        for i in range(32):
            row = i // 8
            col = i % 8

            checkbox = QCheckBox()
            checkbox.setFixedSize(24, 24)
            checkbox.setChecked((self._current_value & (1 << i)) != 0)
            checkbox.setToolTip(f"{i+1}: {self.layer_names[i]}")
            checkbox.stateChanged.connect(lambda state, idx=i: self._on_checkbox_changed(idx, state))
            checkbox.setStyleSheet(f"""
                QCheckBox {{
                    spacing: 0px;
                }}
                QCheckBox::indicator {{
                    width: 20px;
                    height: 20px;
                    border: 1px solid {border_color};
                    border-radius: 3px;
                    background-color: {bg_color};
                }}
                QCheckBox::indicator:checked {{
                    background-color: {accent_color};
                    border-color: {accent_color};
                }}
                QCheckBox::indicator:hover {{
                    border-color: {accent_color};
                }}
            """)

            self.checkboxes.append(checkbox)
            grid_layout.addWidget(checkbox, row, col)

        scroll.setWidget(grid_widget)
        popup_layout.addWidget(scroll)

        # Quick actions
        actions_layout = QHBoxLayout()

        select_all_btn = QPushButton("Select All")
        select_all_btn.clicked.connect(self._select_all)
        select_all_btn.setStyleSheet(f"""
            QPushButton {{
                background-color: {bg_color};
                color: {text_color};
                border: 1px solid {border_color};
                border-radius: 4px;
                padding: 4px 8px;
            }}
            QPushButton:hover {{
                border-color: {accent_color};
            }}
        """)
        actions_layout.addWidget(select_all_btn)

        clear_all_btn = QPushButton("Clear All")
        clear_all_btn.clicked.connect(self._clear_all)
        clear_all_btn.setStyleSheet(f"""
            QPushButton {{
                background-color: {bg_color};
                color: {text_color};
                border: 1px solid {border_color};
                border-radius: 4px;
                padding: 4px 8px;
            }}
            QPushButton:hover {{
                border-color: {accent_color};
            }}
        """)
        actions_layout.addWidget(clear_all_btn)

        popup_layout.addLayout(actions_layout)

        # Layer legend
        legend_label = QLabel("Layers 1-32 (8 per row)")
        legend_label.setStyleSheet(f"color: {text_color}; font-size: 10px; opacity: 0.7;")
        popup_layout.addWidget(legend_label)

        widget_action = QWidgetAction(menu)
        widget_action.setDefaultWidget(popup_widget)
        menu.addAction(widget_action)

        # Show menu below button
        menu.exec(self.layer_button.mapToGlobal(self.layer_button.rect().bottomLeft()))

    def _on_checkbox_changed(self, index, state):
        """Handle checkbox state change"""
        if state == Qt.CheckState.Checked.value:
            self._current_value |= (1 << index)
        else:
            self._current_value &= ~(1 << index)

        self._update_button_text()
        self.value_changed.emit(self._current_value)

    def _select_all(self):
        """Select all layers"""
        self._current_value = 0xFFFFFFFF
        for checkbox in self.checkboxes:
            checkbox.blockSignals(True)
            checkbox.setChecked(True)
            checkbox.blockSignals(False)
        self._update_button_text()
        self.value_changed.emit(self._current_value)

    def _clear_all(self):
        """Clear all layers"""
        self._current_value = 0
        for checkbox in self.checkboxes:
            checkbox.blockSignals(True)
            checkbox.setChecked(False)
            checkbox.blockSignals(False)
        self._update_button_text()
        self.value_changed.emit(self._current_value)

    def _update_button_text(self):
        """Update the button text to show selected layers"""
        if self._current_value == 0:
            self.layer_button.setText("None")
        elif self._current_value == 0xFFFFFFFF:
            self.layer_button.setText("All Layers")
        else:
            # Show selected layer numbers/names
            selected = []
            for i in range(32):
                if self._current_value & (1 << i):
                    name = self.layer_names[i] if self.layer_names[i] != f"Layer {i+1}" else str(i+1)
                    selected.append(name)

            if len(selected) <= 3:
                self.layer_button.setText(", ".join(selected))
            else:
                self.layer_button.setText(f"{len(selected)} layers")

    def set_value(self, value):
        """Set the widget value"""
        self._current_value = int(value) if value is not None else self.default_value
        self._update_button_text()

    def get_value(self):
        """Get the widget value"""
        return self._current_value

    def reset_to_default(self):
        """Reset to default value"""
        self._current_value = self.default_value
        self._update_button_text()
        self.value_changed.emit(self._current_value)

    def set_layer_names(self, names):
        """Update the layer names"""
        if names and len(names) == 32:
            self.layer_names = names
            self._update_button_text()
