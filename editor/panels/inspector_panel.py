"""
Inspector Panel
Display and edit properties of selected objects
"""

from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel, QLineEdit,
                             QPushButton, QScrollArea, QGroupBox, QSpinBox, QDoubleSpinBox,
                             QCheckBox, QComboBox, QColorDialog, QFileDialog, QFrame,
                             QSizePolicy, QMessageBox)
from PyQt6.QtCore import Qt, pyqtSignal, QTimer
from PyQt6.QtGui import QColor
from .base_panel import EditorPanel
from dialogs import AddComponentDialog
import lupine_engine as le
import json
import re
import math
import sys
from pathlib import Path

# Add parent directory to path to import theme
sys.path.insert(0, str(Path(__file__).parent.parent))
from theme import get_theme_manager


def format_property_name(name: str) -> str:
    """Convert camelCase or snake_case to Title Case with spaces.

    Examples:
        'castShadow' -> 'Cast Shadow'
        'doubleSided' -> 'Double Sided'
        'z_index' -> 'Z Index'
    """
    # Replace underscores with spaces
    name = name.replace('_', ' ')

    # Insert space before uppercase letters (camelCase)
    name = re.sub(r'([a-z])([A-Z])', r'\1 \2', name)

    # Capitalize each word
    name = ' '.join(word.capitalize() for word in name.split())

    return name


def get_theme_spacing():
    """Get theme spacing values for inspector widgets"""
    theme = get_theme_manager().get_current_theme()
    if theme:
        return theme.colors.inspector_vertical_spacing, theme.colors.inspector_horizontal_padding
    return 8, 6  # defaults


class CollapsibleGroupBox(QWidget):
    """A collapsible group box for organizing properties"""
    
    def __init__(self, title, parent=None):
        super().__init__(parent)
        self.title = title
        self.is_collapsed = False
        
        # Get theme colors
        theme = get_theme_manager().get_current_theme()
        if theme:
            colors = theme.colors
            bg_color = colors.surface
            hover_color = colors.surface_hover
            text_color = colors.text_primary
        else:
            bg_color = "#2d2d2d"
            hover_color = "#3a3a3a"
            text_color = "#aaaaaa"
        
        # Main layout
        main_layout = QVBoxLayout()
        main_layout.setContentsMargins(0, 0, 0, 0)
        main_layout.setSpacing(0)
        
        # Header button
        self.header_button = QPushButton()
        self.header_button.setCheckable(True)
        self.header_button.setChecked(True)
        self.header_button.clicked.connect(self.toggle_collapsed)
        self.update_header_text()
        self.header_button.setStyleSheet(f"""
            QPushButton {{
                background-color: {bg_color};
                border: none;
                padding: 6px;
                text-align: left;
                font-weight: bold;
                color: {text_color};
            }}
            QPushButton:hover {{
                background-color: {hover_color};
            }}
            QPushButton:checked {{
                background-color: {bg_color};
            }}
        """)
        main_layout.addWidget(self.header_button)
        
        # Content widget
        self.content_widget = QWidget()
        self.content_layout = QVBoxLayout()
        self.content_layout.setContentsMargins(8, 4, 0, 4)
        self.content_layout.setSpacing(2)
        self.content_widget.setLayout(self.content_layout)
        main_layout.addWidget(self.content_widget)
        
        self.setLayout(main_layout)
    
    def update_header_text(self):
        """Update the header text with collapse indicator"""
        arrow = "▼" if not self.is_collapsed else "▶"
        self.header_button.setText(f"{arrow} {self.title}")
    
    def toggle_collapsed(self):
        """Toggle the collapsed state"""
        self.is_collapsed = not self.is_collapsed
        self.content_widget.setVisible(not self.is_collapsed)
        self.update_header_text()
    
    def add_widget(self, widget):
        """Add a widget to the content area"""
        self.content_layout.addWidget(widget)


class PropertyWidget(QWidget):
    """Base class for property editing widgets"""
    value_changed = pyqtSignal(object)  # new value

    def __init__(self, property_name, default_value=None, parent=None):
        super().__init__(parent)
        self.property_name = property_name
        self.default_value = default_value
        self.reset_button = None  # Will be created by subclasses if they add it
        self.setup_ui()

    def setup_ui(self):
        """Setup the widget UI - override in subclasses"""
        pass

    def set_value(self, value):
        """Set the widget value - override in subclasses"""
        pass

    def get_value(self):
        """Get the widget value - override in subclasses"""
        return None

    def reset_to_default(self):
        """Reset the widget to its default value - override in subclasses"""
        if self.default_value is not None:
            self.set_value(self.default_value)
            # Emit value_changed to propagate the change to the backend
            self.value_changed.emit(self.default_value)

    def _create_reset_button(self):
        """Helper to create a reset button with consistent styling"""
        reset_btn = QPushButton("⟲")  # Unicode circular arrow
        reset_btn.setFixedSize(24, 24)
        reset_btn.setToolTip("Reset to default value")
        reset_btn.clicked.connect(self.reset_to_default)

        # Style the button with more visible appearance
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
                    background-color: {theme.colors.accent_color};
                    color: {theme.colors.text_on_accent};
                    border-color: {theme.colors.accent_color};
                }}
                QPushButton:pressed {{
                    background-color: {theme.colors.accent_pressed};
                    border-color: {theme.colors.accent_pressed};
                }}
            """)

        self.reset_button = reset_btn
        return reset_btn


class IntPropertyWidget(PropertyWidget):
    """Widget for editing integer properties"""

    def setup_ui(self):
        layout = QHBoxLayout()
        v_spacing, h_padding = get_theme_spacing()
        layout.setContentsMargins(0, v_spacing // 2, 0, v_spacing // 2)
        layout.setSpacing(h_padding)

        self.label = QLabel(self.property_name)
        self.label.setFixedWidth(120)
        self.label.setSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Fixed)

        self.spinbox = QSpinBox()
        self.spinbox.setRange(-2147483648, 2147483647)
        self.spinbox.setMinimumWidth(60)
        self.spinbox.setMinimumHeight(28)
        self.spinbox.setMinimumHeight(28)
        self.spinbox.valueChanged.connect(lambda v: self.value_changed.emit(v))
        self.spinbox.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        layout.addWidget(self.label)
        layout.addWidget(self.spinbox)
        layout.addWidget(self._create_reset_button())
        self.setLayout(layout)

    def set_value(self, value):
        self.spinbox.blockSignals(True)
        self.spinbox.setValue(int(value))
        self.spinbox.blockSignals(False)

    def get_value(self):
        return self.spinbox.value()


class FloatPropertyWidget(PropertyWidget):
    """Widget for editing float properties"""

    def setup_ui(self):
        layout = QHBoxLayout()
        v_spacing, h_padding = get_theme_spacing()
        layout.setContentsMargins(0, v_spacing // 2, 0, v_spacing // 2)
        layout.setSpacing(h_padding)

        self.label = QLabel(self.property_name)
        self.label.setFixedWidth(120)
        self.label.setSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Fixed)

        self.spinbox = QDoubleSpinBox()
        self.spinbox.setRange(-1e10, 1e10)
        self.spinbox.setDecimals(3)
        self.spinbox.setSingleStep(0.1)
        self.spinbox.setMinimumWidth(60)
        self.spinbox.setMinimumHeight(28)
        self.spinbox.valueChanged.connect(lambda v: self.value_changed.emit(v))
        self.spinbox.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        layout.addWidget(self.label)
        layout.addWidget(self.spinbox)
        layout.addWidget(self._create_reset_button())
        self.setLayout(layout)

    def set_value(self, value):
        self.spinbox.blockSignals(True)
        self.spinbox.setValue(float(value))
        self.spinbox.blockSignals(False)

    def get_value(self):
        return self.spinbox.value()


class BoolPropertyWidget(PropertyWidget):
    """Widget for editing boolean properties"""

    def setup_ui(self):
        layout = QHBoxLayout()
        v_spacing, h_padding = get_theme_spacing()
        layout.setContentsMargins(0, v_spacing // 2, 0, v_spacing // 2)

        self.checkbox = QCheckBox(self.property_name)
        self.checkbox.setMinimumHeight(28)
        self.checkbox.toggled.connect(lambda checked: self.value_changed.emit(checked))

        layout.addWidget(self.checkbox)
        layout.addStretch()
        self.setLayout(layout)

    def set_value(self, value):
        self.checkbox.blockSignals(True)
        self.checkbox.setChecked(bool(value))
        self.checkbox.blockSignals(False)

    def get_value(self):
        return self.checkbox.isChecked()


class EnumPropertyWidget(PropertyWidget):
    """Widget for editing enum properties"""

    def __init__(self, property_name, enum_values=None, default_value=None, parent=None):
        self.enum_values = enum_values or []
        super().__init__(property_name, default_value, parent)

    def setup_ui(self):
        layout = QHBoxLayout()
        v_spacing, h_padding = get_theme_spacing()
        layout.setContentsMargins(0, v_spacing // 2, 0, v_spacing // 2)
        layout.setSpacing(h_padding)

        self.label = QLabel(self.property_name)
        self.label.setFixedWidth(120)
        self.label.setSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Fixed)

        self.combobox = QComboBox()
        self.combobox.setMinimumHeight(28)
        self.combobox.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        # Add enum values to combobox
        if self.enum_values:
            self.combobox.addItems(self.enum_values)

        self.combobox.currentIndexChanged.connect(lambda index: self.value_changed.emit(index))

        layout.addWidget(self.label)
        layout.addWidget(self.combobox)
        layout.addWidget(self._create_reset_button())
        self.setLayout(layout)

    def set_value(self, value):
        self.combobox.blockSignals(True)
        # Value is the enum index
        if isinstance(value, int) and 0 <= value < self.combobox.count():
            self.combobox.setCurrentIndex(value)
        self.combobox.blockSignals(False)

    def get_value(self):
        return self.combobox.currentIndex()


class StringPropertyWidget(PropertyWidget):
    """Widget for editing string properties"""

    def setup_ui(self):
        layout = QHBoxLayout()
        v_spacing, h_padding = get_theme_spacing()
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(h_padding)

        self.label = QLabel(self.property_name)
        self.label.setFixedWidth(120)
        self.label.setSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Fixed)

        self.line_edit = QLineEdit()
        self.line_edit.setMinimumWidth(60)
        self.line_edit.editingFinished.connect(lambda: self.value_changed.emit(self.line_edit.text()))
        self.line_edit.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        layout.addWidget(self.label)
        layout.addWidget(self.line_edit)
        layout.addWidget(self._create_reset_button())
        self.setLayout(layout)

    def set_value(self, value):
        self.line_edit.blockSignals(True)
        self.line_edit.setText(str(value) if value is not None else "")
        self.line_edit.blockSignals(False)

    def get_value(self):
        return self.line_edit.text()


class Vec2PropertyWidget(PropertyWidget):
    """Widget for editing 2D vector properties"""

    def setup_ui(self):
        layout = QHBoxLayout()
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(2)

        self.label = QLabel(self.property_name)
        self.label.setFixedWidth(120)
        self.label.setSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Fixed)

        self.x_spinbox = QDoubleSpinBox()
        self.x_spinbox.setPrefix("X:")
        self.x_spinbox.setRange(-1e10, 1e10)
        self.x_spinbox.setDecimals(2)
        self.x_spinbox.valueChanged.connect(self._on_value_changed)
        self.x_spinbox.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        self.y_spinbox = QDoubleSpinBox()
        self.y_spinbox.setPrefix("Y:")
        self.y_spinbox.setRange(-1e10, 1e10)
        self.y_spinbox.setDecimals(2)
        self.y_spinbox.setMinimumWidth(50)
        self.y_spinbox.valueChanged.connect(self._on_value_changed)
        self.y_spinbox.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        layout.addWidget(self.label)
        layout.addWidget(self.x_spinbox)
        layout.addWidget(self.y_spinbox)
        layout.addWidget(self._create_reset_button())
        self.setLayout(layout)

    def _on_value_changed(self):
        self.value_changed.emit((self.x_spinbox.value(), self.y_spinbox.value()))

    def set_value(self, value):
        self.x_spinbox.blockSignals(True)
        self.y_spinbox.blockSignals(True)
        if hasattr(value, '__iter__') and len(value) >= 2:
            self.x_spinbox.setValue(float(value[0]))
            self.y_spinbox.setValue(float(value[1]))
        self.x_spinbox.blockSignals(False)
        self.y_spinbox.blockSignals(False)

    def get_value(self):
        return (self.x_spinbox.value(), self.y_spinbox.value())

    def reset_to_default(self):
        """Override to emit the correct tuple format"""
        if self.default_value is not None:
            self.set_value(self.default_value)
            self.value_changed.emit(self.get_value())


class Vec3PropertyWidget(PropertyWidget):
    """Widget for editing 3D vector properties"""

    def setup_ui(self):
        layout = QHBoxLayout()
        layout.setContentsMargins(0, 3, 0, 3)
        layout.setSpacing(2)

        self.label = QLabel(self.property_name)
        self.label.setFixedWidth(120)
        self.label.setSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Fixed)

        self.x_spinbox = QDoubleSpinBox()
        self.x_spinbox.setPrefix("X:")
        self.x_spinbox.setRange(-1e10, 1e10)
        self.x_spinbox.setDecimals(2)
        self.x_spinbox.setMinimumWidth(45)
        self.x_spinbox.setMinimumHeight(28)
        self.x_spinbox.valueChanged.connect(self._on_value_changed)
        self.x_spinbox.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        self.y_spinbox = QDoubleSpinBox()
        self.y_spinbox.setPrefix("Y:")
        self.y_spinbox.setRange(-1e10, 1e10)
        self.y_spinbox.setDecimals(2)
        self.y_spinbox.setMinimumWidth(45)
        self.y_spinbox.setMinimumHeight(28)
        self.y_spinbox.valueChanged.connect(self._on_value_changed)
        self.y_spinbox.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        self.z_spinbox = QDoubleSpinBox()
        self.z_spinbox.setPrefix("Z:")
        self.z_spinbox.setRange(-1e10, 1e10)
        self.z_spinbox.setDecimals(2)
        self.z_spinbox.setMinimumWidth(45)
        self.z_spinbox.setMinimumHeight(28)
        self.z_spinbox.valueChanged.connect(self._on_value_changed)
        self.z_spinbox.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        layout.addWidget(self.label)
        layout.addWidget(self.x_spinbox)
        layout.addWidget(self.y_spinbox)
        layout.addWidget(self.z_spinbox)
        layout.addWidget(self._create_reset_button())

        self.setLayout(layout)

    def _on_value_changed(self):
        self.value_changed.emit((self.x_spinbox.value(), self.y_spinbox.value(), self.z_spinbox.value()))

    def set_value(self, value):
        self.x_spinbox.blockSignals(True)
        self.y_spinbox.blockSignals(True)
        self.z_spinbox.blockSignals(True)
        if hasattr(value, '__iter__') and len(value) >= 3:
            self.x_spinbox.setValue(float(value[0]))
            self.y_spinbox.setValue(float(value[1]))
            self.z_spinbox.setValue(float(value[2]))
        self.x_spinbox.blockSignals(False)
        self.y_spinbox.blockSignals(False)
        self.z_spinbox.blockSignals(False)

    def get_value(self):
        return (self.x_spinbox.value(), self.y_spinbox.value(), self.z_spinbox.value())

    def reset_to_default(self):
        """Override to emit the correct tuple format"""
        if self.default_value is not None:
            self.set_value(self.default_value)
            self.value_changed.emit(self.get_value())


class EulerRotationWidget(PropertyWidget):
    """Widget for editing 3D rotation as Euler angles (degrees) with mode toggle."""

    def __init__(self, property_name="Rotation", default_value=None, parent=None):
        self.use_euler = True  # Default to Euler mode
        self.current_quat = {"x": 0.0, "y": 0.0, "z": 0.0, "w": 1.0}  # Store current quaternion
        super().__init__(property_name, default_value, parent)

    def setup_ui(self):
        layout = QVBoxLayout()
        layout.setContentsMargins(0, 3, 0, 3)
        layout.setSpacing(2)

        # Top row: Label on left, reset button and mode toggle button on right
        top_layout = QHBoxLayout()
        top_layout.setSpacing(2)

        self.label = QLabel(self.property_name)
        self.label.setFixedWidth(120)
        self.label.setSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Fixed)
        top_layout.addWidget(self.label)

        top_layout.addStretch()  # Push buttons to the right

        top_layout.addWidget(self._create_reset_button())

        self.mode_button = QPushButton("Euler (deg)")
        self.mode_button.setFixedWidth(90)
        self.mode_button.setFixedHeight(24)
        self.mode_button.clicked.connect(self._toggle_mode)
        top_layout.addWidget(self.mode_button)

        layout.addLayout(top_layout)

        # Bottom row: X, Y, Z, W spinboxes (W hidden in Euler mode)
        spinbox_layout = QHBoxLayout()
        spinbox_layout.setSpacing(2)
        spinbox_layout.setContentsMargins(0, 0, 0, 0)

        self.x_spinbox = QDoubleSpinBox()
        self.x_spinbox.setPrefix("X:")
        self.x_spinbox.setRange(-360, 360)
        self.x_spinbox.setDecimals(1)
        self.x_spinbox.setMinimumWidth(40)
        self.x_spinbox.setMinimumHeight(28)
        self.x_spinbox.valueChanged.connect(self._on_value_changed)
        self.x_spinbox.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        self.y_spinbox = QDoubleSpinBox()
        self.y_spinbox.setPrefix("Y:")
        self.y_spinbox.setRange(-360, 360)
        self.y_spinbox.setDecimals(1)
        self.y_spinbox.setMinimumWidth(40)
        self.y_spinbox.setMinimumHeight(28)
        self.y_spinbox.valueChanged.connect(self._on_value_changed)
        self.y_spinbox.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        self.z_spinbox = QDoubleSpinBox()
        self.z_spinbox.setPrefix("Z:")
        self.z_spinbox.setRange(-360, 360)
        self.z_spinbox.setDecimals(1)
        self.z_spinbox.setMinimumWidth(40)
        self.z_spinbox.setMinimumHeight(28)
        self.z_spinbox.valueChanged.connect(self._on_value_changed)
        self.z_spinbox.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        self.w_spinbox = QDoubleSpinBox()
        self.w_spinbox.setPrefix("W:")
        self.w_spinbox.setRange(-1, 1)
        self.w_spinbox.setDecimals(3)
        self.w_spinbox.setMinimumWidth(40)
        self.w_spinbox.setMinimumHeight(28)
        self.w_spinbox.valueChanged.connect(self._on_value_changed)
        self.w_spinbox.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)
        self.w_spinbox.setVisible(False)  # Hidden in Euler mode by default

        spinbox_layout.addWidget(self.x_spinbox)
        spinbox_layout.addWidget(self.y_spinbox)
        spinbox_layout.addWidget(self.z_spinbox)
        spinbox_layout.addWidget(self.w_spinbox)

        layout.addLayout(spinbox_layout)
        self.setLayout(layout)

    def _toggle_mode(self):
        """Toggle between Euler and Quaternion display modes."""
        self.use_euler = not self.use_euler
        if self.use_euler:
            self.mode_button.setText("Euler (deg)")
            self.x_spinbox.setRange(-360, 360)
            self.y_spinbox.setRange(-360, 360)
            self.z_spinbox.setRange(-360, 360)
            self.x_spinbox.setDecimals(1)
            self.y_spinbox.setDecimals(1)
            self.z_spinbox.setDecimals(1)
            self.w_spinbox.setVisible(False)
            # Convert current quaternion to Euler
            euler = self._quat_to_euler(self.current_quat)
            self.set_euler_values(euler)
        else:
            self.mode_button.setText("Quaternion")
            self.x_spinbox.setRange(-1, 1)
            self.y_spinbox.setRange(-1, 1)
            self.z_spinbox.setRange(-1, 1)
            self.x_spinbox.setDecimals(3)
            self.y_spinbox.setDecimals(3)
            self.z_spinbox.setDecimals(3)
            self.w_spinbox.setVisible(True)
            # Show all quaternion components (x, y, z, w)
            self.set_quat_xyzw_values(self.current_quat)

    def _quat_to_euler(self, quat):
        """Convert quaternion to Euler angles in degrees (ZYX order)."""
        x, y, z, w = quat["x"], quat["y"], quat["z"], quat["w"]
        
        # Convert to Euler angles (in radians)
        # Using ZYX rotation order (yaw-pitch-roll)
        sinr_cosp = 2 * (w * x + y * z)
        cosr_cosp = 1 - 2 * (x * x + y * y)
        roll = math.atan2(sinr_cosp, cosr_cosp)
        
        sinp = 2 * (w * y - z * x)
        if abs(sinp) >= 1:
            pitch = math.copysign(math.pi / 2, sinp)
        else:
            pitch = math.asin(sinp)
        
        siny_cosp = 2 * (w * z + x * y)
        cosy_cosp = 1 - 2 * (y * y + z * z)
        yaw = math.atan2(siny_cosp, cosy_cosp)
        
        # Convert to degrees
        return (math.degrees(roll), math.degrees(pitch), math.degrees(yaw))

    def _euler_to_quat(self, euler_deg):
        """Convert Euler angles in degrees to quaternion (ZYX order)."""
        roll, pitch, yaw = math.radians(euler_deg[0]), math.radians(euler_deg[1]), math.radians(euler_deg[2])
        
        cy = math.cos(yaw * 0.5)
        sy = math.sin(yaw * 0.5)
        cp = math.cos(pitch * 0.5)
        sp = math.sin(pitch * 0.5)
        cr = math.cos(roll * 0.5)
        sr = math.sin(roll * 0.5)
        
        w = cr * cp * cy + sr * sp * sy
        x = sr * cp * cy - cr * sp * sy
        y = cr * sp * cy + sr * cp * sy
        z = cr * cp * sy - sr * sp * cy
        
        return {"x": x, "y": y, "z": z, "w": w}

    def _on_value_changed(self):
        if self.use_euler:
            # User edited Euler angles - convert to quaternion
            euler = (self.x_spinbox.value(), self.y_spinbox.value(), self.z_spinbox.value())
            self.current_quat = self._euler_to_quat(euler)
        else:
            # User edited quaternion components - read all 4 values
            self.current_quat["x"] = self.x_spinbox.value()
            self.current_quat["y"] = self.y_spinbox.value()
            self.current_quat["z"] = self.z_spinbox.value()
            self.current_quat["w"] = self.w_spinbox.value()
            # Normalize the quaternion
            mag = math.sqrt(self.current_quat["x"]**2 + self.current_quat["y"]**2 + 
                          self.current_quat["z"]**2 + self.current_quat["w"]**2)
            if mag > 0.0001:
                self.current_quat["x"] /= mag
                self.current_quat["y"] /= mag
                self.current_quat["z"] /= mag
                self.current_quat["w"] /= mag
                # Update display with normalized values
                self.set_quat_xyzw_values(self.current_quat)
        
        # Always emit the quaternion
        self.value_changed.emit(self.current_quat)

    def set_euler_values(self, euler):
        """Set spinbox values from Euler angles (degrees)."""
        self.x_spinbox.blockSignals(True)
        self.y_spinbox.blockSignals(True)
        self.z_spinbox.blockSignals(True)
        self.x_spinbox.setValue(float(euler[0]))
        self.y_spinbox.setValue(float(euler[1]))
        self.z_spinbox.setValue(float(euler[2]))
        self.x_spinbox.blockSignals(False)
        self.y_spinbox.blockSignals(False)
        self.z_spinbox.blockSignals(False)

    def set_quat_xyzw_values(self, quat):
        """Set spinbox values from quaternion x, y, z, w components."""
        self.x_spinbox.blockSignals(True)
        self.y_spinbox.blockSignals(True)
        self.z_spinbox.blockSignals(True)
        self.w_spinbox.blockSignals(True)
        self.x_spinbox.setValue(float(quat["x"]))
        self.y_spinbox.setValue(float(quat["y"]))
        self.z_spinbox.setValue(float(quat["z"]))
        self.w_spinbox.setValue(float(quat["w"]))
        self.x_spinbox.blockSignals(False)
        self.y_spinbox.blockSignals(False)
        self.z_spinbox.blockSignals(False)
        self.w_spinbox.blockSignals(False)

    def set_value(self, value):
        """Set value from quaternion dict."""
        if isinstance(value, dict) and all(k in value for k in ("x", "y", "z", "w")):
            self.current_quat = {
                "x": float(value["x"]),
                "y": float(value["y"]),
                "z": float(value["z"]),
                "w": float(value["w"])
            }
            if self.use_euler:
                euler = self._quat_to_euler(self.current_quat)
                self.set_euler_values(euler)
            else:
                self.set_quat_xyzw_values(self.current_quat)

    def get_value(self):
        """Always return quaternion dict."""
        return self.current_quat

    def reset_to_default(self):
        """Override to emit the correct quaternion format"""
        if self.default_value is not None:
            self.set_value(self.default_value)
            # Emit as quaternion dict
            self.value_changed.emit(self.get_value())


class Vec4PropertyWidget(PropertyWidget):
    """Widget for editing 4D vector properties (e.g., quaternions)."""

    def setup_ui(self):
        main_layout = QVBoxLayout()
        main_layout.setContentsMargins(0, 3, 0, 3)
        main_layout.setSpacing(2)

        # First row: label and reset button
        top_layout = QHBoxLayout()
        top_layout.setSpacing(2)

        self.label = QLabel(self.property_name)
        self.label.setFixedWidth(120)
        self.label.setSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Fixed)
        top_layout.addWidget(self.label)
        top_layout.addStretch()
        top_layout.addWidget(self._create_reset_button())

        main_layout.addLayout(top_layout)

        # Second row: spinboxes
        spinbox_layout = QHBoxLayout()
        spinbox_layout.setSpacing(2)

        self.x_spinbox = QDoubleSpinBox()
        self.x_spinbox.setPrefix("X:")
        self.x_spinbox.setRange(-1e10, 1e10)
        self.x_spinbox.setDecimals(2)
        self.x_spinbox.setMinimumWidth(40)
        self.x_spinbox.setMinimumHeight(28)
        self.x_spinbox.valueChanged.connect(self._on_value_changed)
        self.x_spinbox.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        self.y_spinbox = QDoubleSpinBox()
        self.y_spinbox.setPrefix("Y:")
        self.y_spinbox.setRange(-1e10, 1e10)
        self.y_spinbox.setDecimals(2)
        self.y_spinbox.setMinimumWidth(40)
        self.y_spinbox.setMinimumHeight(28)
        self.y_spinbox.valueChanged.connect(self._on_value_changed)
        self.y_spinbox.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        self.z_spinbox = QDoubleSpinBox()
        self.z_spinbox.setPrefix("Z:")
        self.z_spinbox.setRange(-1e10, 1e10)
        self.z_spinbox.setDecimals(2)
        self.z_spinbox.setMinimumWidth(40)
        self.z_spinbox.setMinimumHeight(28)
        self.z_spinbox.valueChanged.connect(self._on_value_changed)
        self.z_spinbox.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        self.w_spinbox = QDoubleSpinBox()
        self.w_spinbox.setPrefix("W:")
        self.w_spinbox.setRange(-1e10, 1e10)
        self.w_spinbox.setDecimals(2)
        self.w_spinbox.setMinimumWidth(40)
        self.w_spinbox.setMinimumHeight(28)
        self.w_spinbox.valueChanged.connect(self._on_value_changed)
        self.w_spinbox.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        spinbox_layout.addWidget(self.x_spinbox)
        spinbox_layout.addWidget(self.y_spinbox)
        spinbox_layout.addWidget(self.z_spinbox)
        spinbox_layout.addWidget(self.w_spinbox)

        main_layout.addLayout(spinbox_layout)
        self.setLayout(main_layout)

    def _on_value_changed(self):
        self.value_changed.emit(
            (
                self.x_spinbox.value(),
                self.y_spinbox.value(),
                self.z_spinbox.value(),
                self.w_spinbox.value(),
            )
        )

    def set_value(self, value):
        self.x_spinbox.blockSignals(True)
        self.y_spinbox.blockSignals(True)
        self.z_spinbox.blockSignals(True)
        self.w_spinbox.blockSignals(True)
        if hasattr(value, "__iter__") and len(value) >= 4:
            self.x_spinbox.setValue(float(value[0]))
            self.y_spinbox.setValue(float(value[1]))
            self.z_spinbox.setValue(float(value[2]))
            self.w_spinbox.setValue(float(value[3]))
        self.x_spinbox.blockSignals(False)
        self.y_spinbox.blockSignals(False)
        self.z_spinbox.blockSignals(False)
        self.w_spinbox.blockSignals(False)

    def get_value(self):
        return (
            self.x_spinbox.value(),
            self.y_spinbox.value(),
            self.z_spinbox.value(),
            self.w_spinbox.value(),
        )

    def reset_to_default(self):
        """Override to emit the correct tuple format"""
        if self.default_value is not None:
            self.set_value(self.default_value)
            self.value_changed.emit(self.get_value())


class LinkedVec4PropertyWidget(PropertyWidget):
    """Widget for editing 4D vector properties with linked/unlinked control."""

    def __init__(self, property_name, linked_prop_name=None, default_value=None, parent=None):
        self.linked_prop_name = linked_prop_name or f"{property_name}Linked"
        self.is_linked = True
        self._updating_from_backend = False  # Flag to prevent toggle conflicts
        super().__init__(property_name, default_value, parent)

    def setup_ui(self):
        layout = QVBoxLayout()
        layout.setContentsMargins(0, 3, 0, 3)
        layout.setSpacing(2)

        # First row: property label and reset button
        label_layout = QHBoxLayout()
        label_layout.setSpacing(4)

        self.label = QLabel(self.property_name)
        self.label.setFixedWidth(120)
        self.label.setSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Fixed)
        label_layout.addWidget(self.label)
        label_layout.addStretch()
        label_layout.addWidget(self._create_reset_button())

        layout.addLayout(label_layout)

        # Second row: Edit Linked toggle
        linked_layout = QHBoxLayout()
        linked_layout.setSpacing(4)
        linked_layout.setContentsMargins(0, 0, 0, 0)

        linked_label = QLabel("Edit Linked")
        linked_label.setFixedWidth(120)
        linked_label.setSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Fixed)
        linked_layout.addWidget(linked_label)

        # Link/unlink toggle button
        self.link_button = QPushButton("")
        self.link_button.setFixedSize(28, 28)
        self.link_button.setCheckable(False)
        self.link_button.setToolTip("Click to toggle linked editing")
        self.link_button.clicked.connect(self._on_link_clicked)
        self._update_button_style()
        linked_layout.addWidget(self.link_button)

        linked_layout.addStretch()

        layout.addLayout(linked_layout)

        # Third row: spinboxes
        spinbox_layout = QHBoxLayout()
        spinbox_layout.setSpacing(2)
        spinbox_layout.setContentsMargins(0, 0, 0, 0)

        self.x_spinbox = QDoubleSpinBox()
        self.x_spinbox.setPrefix("X:")
        self.x_spinbox.setRange(0, 1e10)
        self.x_spinbox.setDecimals(2)
        self.x_spinbox.setSingleStep(0.1)
        self.x_spinbox.setMinimumWidth(40)
        self.x_spinbox.setMinimumHeight(28)
        self.x_spinbox.valueChanged.connect(lambda: self._on_individual_value_changed(0))
        self.x_spinbox.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        self.y_spinbox = QDoubleSpinBox()
        self.y_spinbox.setPrefix("Y:")
        self.y_spinbox.setRange(0, 1e10)
        self.y_spinbox.setDecimals(2)
        self.y_spinbox.setSingleStep(0.1)
        self.y_spinbox.setMinimumWidth(40)
        self.y_spinbox.setMinimumHeight(28)
        self.y_spinbox.valueChanged.connect(lambda: self._on_individual_value_changed(1))
        self.y_spinbox.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        self.z_spinbox = QDoubleSpinBox()
        self.z_spinbox.setPrefix("Z:")
        self.z_spinbox.setRange(0, 1e10)
        self.z_spinbox.setDecimals(2)
        self.z_spinbox.setSingleStep(0.1)
        self.z_spinbox.setMinimumWidth(40)
        self.z_spinbox.setMinimumHeight(28)
        self.z_spinbox.valueChanged.connect(lambda: self._on_individual_value_changed(2))
        self.z_spinbox.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        self.w_spinbox = QDoubleSpinBox()
        self.w_spinbox.setPrefix("W:")
        self.w_spinbox.setRange(0, 1e10)
        self.w_spinbox.setDecimals(2)
        self.w_spinbox.setSingleStep(0.1)
        self.w_spinbox.setMinimumWidth(40)
        self.w_spinbox.setMinimumHeight(28)
        self.w_spinbox.valueChanged.connect(lambda: self._on_individual_value_changed(3))
        self.w_spinbox.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        spinbox_layout.addWidget(self.x_spinbox)
        spinbox_layout.addWidget(self.y_spinbox)
        spinbox_layout.addWidget(self.z_spinbox)
        spinbox_layout.addWidget(self.w_spinbox)

        layout.addLayout(spinbox_layout)
        self.setLayout(layout)

    def _on_link_clicked(self):
        # Toggle the linked state
        self.is_linked = not self.is_linked
        
        # Update button appearance
        self._update_button_style()
        
        if self.is_linked:
            # When linking, sync all values to X
            value = self.x_spinbox.value()
            self._set_all_spinboxes(value)
        
        # Emit value change to update backend
        self._emit_value_change()

    def _update_button_style(self):
        """Update button style based on linked state"""
        theme = get_theme_manager().get_current_theme()
        if theme:
            colors = theme.colors
            if self.is_linked:
                # Linked state: filled with accent color (like checkbox checked)
                self.link_button.setStyleSheet(f"""
                    QPushButton {{
                        background-color: {colors.accent_color};
                        color: {colors.text_on_accent};
                        border: 2px solid {colors.accent_color};
                        border-radius: 3px;
                    }}
                    QPushButton:hover {{
                        background-color: {colors.accent_hover};
                        border-color: {colors.accent_hover};
                    }}
                """)
            else:
                # Unlinked state: empty with border (like checkbox unchecked)
                self.link_button.setStyleSheet(f"""
                    QPushButton {{
                        background-color: {colors.surface};
                        color: {colors.text_primary};
                        border: 2px solid {colors.border};
                        border-radius: 3px;
                    }}
                    QPushButton:hover {{
                        background-color: {colors.surface_hover};
                        border-color: {colors.border_focus};
                    }}
                """)
        else:
            self.link_button.setStyleSheet("")


    def _on_individual_value_changed(self, index):
        if self.is_linked:
            # Get the value of the changed spinbox
            spinboxes = [self.x_spinbox, self.y_spinbox, self.z_spinbox, self.w_spinbox]
            value = spinboxes[index].value()
            # Update all other spinboxes
            self._set_all_spinboxes(value)
        
        self._emit_value_change()

    def _set_all_spinboxes(self, value):
        """Set all spinboxes to the same value without triggering signals."""
        self.x_spinbox.blockSignals(True)
        self.y_spinbox.blockSignals(True)
        self.z_spinbox.blockSignals(True)
        self.w_spinbox.blockSignals(True)
        
        self.x_spinbox.setValue(value)
        self.y_spinbox.setValue(value)
        self.z_spinbox.setValue(value)
        self.w_spinbox.setValue(value)
        
        self.x_spinbox.blockSignals(False)
        self.y_spinbox.blockSignals(False)
        self.z_spinbox.blockSignals(False)
        self.w_spinbox.blockSignals(False)

    def _emit_value_change(self):
        """Emit value change with both Vec4 and linked state."""
        value_dict = {
            'vec4': (
                self.x_spinbox.value(),
                self.y_spinbox.value(),
                self.z_spinbox.value(),
                self.w_spinbox.value(),
            ),
            'linked': self.is_linked
        }
        self.value_changed.emit(value_dict)

    def set_value(self, value):
        """Set value from Vec4 tuple/list."""
        self.x_spinbox.blockSignals(True)
        self.y_spinbox.blockSignals(True)
        self.z_spinbox.blockSignals(True)
        self.w_spinbox.blockSignals(True)
        
        if hasattr(value, "__iter__") and len(value) >= 4:
            self.x_spinbox.setValue(float(value[0]))
            self.y_spinbox.setValue(float(value[1]))
            self.z_spinbox.setValue(float(value[2]))
            self.w_spinbox.setValue(float(value[3]))
        
        self.x_spinbox.blockSignals(False)
        self.y_spinbox.blockSignals(False)
        self.z_spinbox.blockSignals(False)
        self.w_spinbox.blockSignals(False)

    def set_linked(self, linked):
        """Set the linked state."""
        self.is_linked = linked
        self._update_button_style()

    def get_value(self):
        return {
            'vec4': (
                self.x_spinbox.value(),
                self.y_spinbox.value(),
                self.z_spinbox.value(),
                self.w_spinbox.value(),
            ),
            'linked': self.is_linked
        }

    def reset_to_default(self):
        """Override to emit the correct dict format"""
        if self.default_value is not None:
            self.set_value(self.default_value)
            self.value_changed.emit(self.get_value())



class LinkedColorPropertyWidget(PropertyWidget):
    """Widget for editing 4 color properties with linked/unlinked control (for borders, etc)."""

    def __init__(self, property_name, color_names=None, linked_prop_name=None, default_value=None, parent=None):
        """
        Args:
            property_name: Base name for the property (e.g., "borderColor")
            color_names: List of 4 names for each color (e.g., ["Left", "Right", "Top", "Bottom"])
            linked_prop_name: Name of the linked boolean property
            default_value: Default color tuple (r, g, b, a) to use for all 4 colors
        """
        self.color_names = color_names or ["Color 1", "Color 2", "Color 3", "Color 4"]
        self.linked_prop_name = linked_prop_name or f"{property_name}Linked"
        self.individual_prop_names = [f"{property_name}{name}" for name in self.color_names]
        self.is_linked = True
        self._updating_from_backend = False
        super().__init__(property_name, default_value, parent)

    def setup_ui(self):
        layout = QVBoxLayout()
        layout.setContentsMargins(0, 3, 0, 3)
        layout.setSpacing(2)

        # First row: property label and reset button
        label_layout = QHBoxLayout()
        label_layout.setSpacing(4)

        self.label = QLabel(self.property_name)
        self.label.setFixedWidth(120)
        self.label.setSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Fixed)
        label_layout.addWidget(self.label)
        label_layout.addStretch()
        label_layout.addWidget(self._create_reset_button())

        layout.addLayout(label_layout)

        # Second row: Edit Linked toggle
        linked_layout = QHBoxLayout()
        linked_layout.setSpacing(4)
        linked_layout.setContentsMargins(0, 0, 0, 0)

        linked_label = QLabel("Edit Linked")
        linked_label.setFixedWidth(120)
        linked_label.setSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Fixed)
        linked_layout.addWidget(linked_label)

        # Link/unlink toggle button
        self.link_button = QPushButton("")
        self.link_button.setFixedSize(28, 28)
        self.link_button.setCheckable(False)
        self.link_button.setToolTip("Click to toggle linked editing")
        self.link_button.clicked.connect(self._on_link_clicked)
        self._update_button_style()
        linked_layout.addWidget(self.link_button)

        linked_layout.addStretch()

        layout.addLayout(linked_layout)

        # Third row: color buttons in a 2x2 grid
        grid_layout = QVBoxLayout()
        grid_layout.setSpacing(4)
        grid_layout.setContentsMargins(0, 0, 0, 0)

        self.color_buttons = []
        self.current_colors = [QColor(0, 0, 0, 255) for _ in range(4)]

        # Top row: Color 1 and Color 2
        top_row = QHBoxLayout()
        top_row.setSpacing(4)
        for i in range(2):
            color_widget = QWidget()
            color_layout = QVBoxLayout()
            color_layout.setContentsMargins(0, 0, 0, 0)
            color_layout.setSpacing(2)

            color_label = QLabel(self.color_names[i])
            color_label.setAlignment(Qt.AlignmentFlag.AlignCenter)

            color_button = QPushButton()
            color_button.setFixedSize(60, 40)
            color_button.clicked.connect(lambda checked, idx=i: self._choose_color(idx))
            self.color_buttons.append(color_button)

            color_layout.addWidget(color_label)
            color_layout.addWidget(color_button)
            color_widget.setLayout(color_layout)
            top_row.addWidget(color_widget)

        grid_layout.addLayout(top_row)

        # Bottom row: Color 3 and Color 4
        bottom_row = QHBoxLayout()
        bottom_row.setSpacing(4)
        for i in range(2, 4):
            color_widget = QWidget()
            color_layout = QVBoxLayout()
            color_layout.setContentsMargins(0, 0, 0, 0)
            color_layout.setSpacing(2)

            color_label = QLabel(self.color_names[i])
            color_label.setAlignment(Qt.AlignmentFlag.AlignCenter)

            color_button = QPushButton()
            color_button.setFixedSize(60, 40)
            color_button.clicked.connect(lambda checked, idx=i: self._choose_color(idx))
            self.color_buttons.append(color_button)

            color_layout.addWidget(color_label)
            color_layout.addWidget(color_button)
            color_widget.setLayout(color_layout)
            bottom_row.addWidget(color_widget)

        grid_layout.addLayout(bottom_row)

        layout.addLayout(grid_layout)
        self.setLayout(layout)

        # Update button colors initially
        for i in range(4):
            self._update_button_color(i)

    def _on_link_clicked(self):
        # Toggle the linked state
        self.is_linked = not self.is_linked
        
        # Update button appearance
        self._update_button_style()
        
        if self.is_linked:
            # When linking, sync all colors to the first color
            first_color = self.current_colors[0]
            for i in range(1, 4):
                self.current_colors[i] = QColor(first_color)
                self._update_button_color(i)
        
        # Emit value change to update backend
        self._emit_value_change()

    def _update_button_style(self):
        """Update button style based on linked state"""
        theme = get_theme_manager().get_current_theme()
        if theme:
            colors = theme.colors
            if self.is_linked:
                self.link_button.setStyleSheet(f"""
                    QPushButton {{
                        background-color: {colors.accent_color};
                        color: {colors.text_on_accent};
                        border: 2px solid {colors.accent_color};
                        border-radius: 3px;
                    }}
                    QPushButton:hover {{
                        background-color: {colors.accent_hover};
                        border-color: {colors.accent_hover};
                    }}
                """)
            else:
                self.link_button.setStyleSheet(f"""
                    QPushButton {{
                        background-color: {colors.surface};
                        color: {colors.text_primary};
                        border: 2px solid {colors.border};
                        border-radius: 3px;
                    }}
                    QPushButton:hover {{
                        background-color: {colors.surface_hover};
                        border-color: {colors.border_focus};
                    }}
                """)
        else:
            self.link_button.setStyleSheet("")

    def _choose_color(self, index):
        """Open color dialog for a specific color"""
        color = QColorDialog.getColor(self.current_colors[index], self)
        if color.isValid():
            self.current_colors[index] = color
            self._update_button_color(index)

            if self.is_linked:
                # If linked, update all colors to match
                for i in range(4):
                    if i != index:
                        self.current_colors[i] = QColor(color)
                        self._update_button_color(i)

            self._emit_value_change()

    def _update_button_color(self, index):
        """Update the visual appearance of a color button"""
        color = self.current_colors[index]
        self.color_buttons[index].setStyleSheet(
            f"background-color: rgb({color.red()}, {color.green()}, {color.blue()}); "
            f"border: 1px solid #888;"
        )

    def _emit_value_change(self):
        """Emit value change with all 4 colors and linked state."""
        value_dict = {
            'colors': [
                (c.redF(), c.greenF(), c.blueF(), c.alphaF())
                for c in self.current_colors
            ],
            'linked': self.is_linked,
            'individual_names': self.individual_prop_names
        }
        self.value_changed.emit(value_dict)

    def set_value(self, colors_list):
        """Set values from a list of 4 color tuples or dicts."""
        if not hasattr(colors_list, "__iter__") or len(colors_list) < 4:
            return

        for i in range(4):
            color = colors_list[i]
            
            # Handle dict format (from C++ backend with r, g, b, a keys)
            if isinstance(color, dict):
                r = int(color.get('r', 0) * 255) if color.get('r', 0) <= 1.0 else int(color.get('r', 0))
                g = int(color.get('g', 0) * 255) if color.get('g', 0) <= 1.0 else int(color.get('g', 0))
                b = int(color.get('b', 0) * 255) if color.get('b', 0) <= 1.0 else int(color.get('b', 0))
                a = int(color.get('a', 1) * 255) if color.get('a', 1) <= 1.0 else int(color.get('a', 1))
                self.current_colors[i] = QColor(r, g, b, a)
                self._update_button_color(i)
            # Handle tuple/list format
            elif hasattr(color, '__iter__') and len(color) >= 3:
                r = int(color[0] * 255) if color[0] <= 1.0 else int(color[0])
                g = int(color[1] * 255) if color[1] <= 1.0 else int(color[1])
                b = int(color[2] * 255) if color[2] <= 1.0 else int(color[2])
                a = int(color[3] * 255) if len(color) > 3 and color[3] <= 1.0 else 255
                self.current_colors[i] = QColor(r, g, b, a)
                self._update_button_color(i)

    def set_linked(self, linked):
        """Set the linked state."""
        self.is_linked = linked
        self._update_button_style()

    def get_value(self):
        return {
            'colors': [
                (c.redF(), c.greenF(), c.blueF(), c.alphaF())
                for c in self.current_colors
            ],
            'linked': self.is_linked,
            'individual_names': self.individual_prop_names
        }

    def reset_to_default(self):
        """Override to emit the correct dict format"""
        if self.default_value is not None:
            # Assume default_value is a single color tuple, apply to all 4
            self.set_value([self.default_value] * 4)
            self.value_changed.emit(self.get_value())



class ColorPropertyWidget(PropertyWidget):
    """Widget for editing color properties"""

    def setup_ui(self):
        layout = QHBoxLayout()
        layout.setContentsMargins(0, 3, 0, 3)
        layout.setSpacing(4)

        self.label = QLabel(self.property_name)
        self.label.setFixedWidth(120)
        self.label.setSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Fixed)

        self.color_button = QPushButton()
        self.color_button.setFixedSize(50, 28)
        self.color_button.clicked.connect(self._choose_color)
        self.color_button.setSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Fixed)

        self.current_color = QColor(255, 255, 255)
        self._update_button_color()

        layout.addWidget(self.label)
        layout.addWidget(self.color_button)
        layout.addWidget(self._create_reset_button())
        layout.addStretch()
        self.setLayout(layout)

    def _choose_color(self):
        color = QColorDialog.getColor(self.current_color, self)
        if color.isValid():
            self.current_color = color
            self._update_button_color()
            self.value_changed.emit((color.redF(), color.greenF(), color.blueF(), color.alphaF()))

    def _update_button_color(self):
        self.color_button.setStyleSheet(
            f"background-color: rgb({self.current_color.red()}, "
            f"{self.current_color.green()}, {self.current_color.blue()}); "
            f"border: 1px solid #888;"
        )

    def set_value(self, value):
        if hasattr(value, '__iter__') and len(value) >= 3:
            r = int(value[0] * 255) if value[0] <= 1.0 else int(value[0])
            g = int(value[1] * 255) if value[1] <= 1.0 else int(value[1])
            b = int(value[2] * 255) if value[2] <= 1.0 else int(value[2])
            a = int(value[3] * 255) if len(value) > 3 and value[3] <= 1.0 else 255
            self.current_color = QColor(r, g, b, a)
            self._update_button_color()

    def get_value(self):
        return (self.current_color.redF(), self.current_color.greenF(),
                self.current_color.blueF(), self.current_color.alphaF())

    def reset_to_default(self):
        """Override to emit the correct color format"""
        if self.default_value is not None:
            self.set_value(self.default_value)
            # Emit as tuple of floats (0-1 range)
            self.value_changed.emit(self.get_value())


class PathPropertyWidget(PropertyWidget):
    """Widget for editing file path properties"""

    def setup_ui(self):
        layout = QHBoxLayout()
        layout.setContentsMargins(0, 3, 0, 3)
        layout.setSpacing(4)

        self.label = QLabel(self.property_name)
        self.label.setFixedWidth(120)
        self.label.setSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Fixed)

        self.line_edit = QLineEdit()
        self.line_edit.setMinimumWidth(60)
        self.line_edit.setMinimumHeight(28)
        self.line_edit.editingFinished.connect(lambda: self.value_changed.emit(self.line_edit.text()))
        self.line_edit.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        self.browse_button = QPushButton("...")
        self.browse_button.setFixedSize(30, 28)
        self.browse_button.clicked.connect(self._browse_path)

        layout.addWidget(self.label)
        layout.addWidget(self.line_edit)
        layout.addWidget(self.browse_button)
        layout.addWidget(self._create_reset_button())
        self.setLayout(layout)

    def _browse_path(self):
        file_path, _ = QFileDialog.getOpenFileName(self, f"Select {self.property_name}")
        if file_path:
            self.line_edit.setText(file_path)
            self.value_changed.emit(file_path)

    def set_value(self, value):
        self.line_edit.blockSignals(True)
        self.line_edit.setText(str(value) if value is not None else "")
        self.line_edit.blockSignals(False)

    def get_value(self):
        return self.line_edit.text()

    def reset_to_default(self):
        """Override to emit the correct string format"""
        if self.default_value is not None:
            self.set_value(self.default_value)
            self.value_changed.emit(self.get_value())


class AudioFilePropertyWidget(PropertyWidget):
    """Widget for editing audio file properties with playback controls"""

    def __init__(self, property_name, editor_bridge=None, parent=None):
        self.editor_bridge = editor_bridge
        self.playing_source_uuid = None
        super().__init__(property_name, parent)

    def setup_ui(self):
        layout = QHBoxLayout()
        layout.setContentsMargins(0, 3, 0, 3)
        layout.setSpacing(4)

        self.label = QLabel(self.property_name)
        self.label.setFixedWidth(120)
        self.label.setSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Fixed)

        self.line_edit = QLineEdit()
        self.line_edit.setMinimumWidth(60)
        self.line_edit.setMinimumHeight(28)
        self.line_edit.editingFinished.connect(lambda: self.value_changed.emit(self.line_edit.text()))
        self.line_edit.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)

        self.browse_button = QPushButton("...")
        self.browse_button.setFixedSize(30, 28)
        self.browse_button.clicked.connect(self._browse_path)

        # Audio playback controls
        self.play_button = QPushButton("▶")
        self.play_button.setFixedSize(28, 28)
        self.play_button.setToolTip("Play audio")
        self.play_button.clicked.connect(self._play_audio)

        self.pause_button = QPushButton("⏸")
        self.pause_button.setFixedSize(28, 28)
        self.pause_button.setToolTip("Pause audio")
        self.pause_button.clicked.connect(self._pause_audio)
        self.pause_button.setEnabled(False)

        self.stop_button = QPushButton("⏹")
        self.stop_button.setFixedSize(28, 28)
        self.stop_button.setToolTip("Stop audio")
        self.stop_button.clicked.connect(self._stop_audio)
        self.stop_button.setEnabled(False)

        layout.addWidget(self.label)
        layout.addWidget(self.line_edit)
        layout.addWidget(self.browse_button)
        layout.addWidget(self.play_button)
        layout.addWidget(self.pause_button)
        layout.addWidget(self.stop_button)
        layout.addWidget(self._create_reset_button())
        self.setLayout(layout)

    def _browse_path(self):
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            f"Select {self.property_name}",
            "",
            "Audio Files (*.wav *.mp3 *.ogg *.flac);;All Files (*.*)"
        )
        if file_path:
            self.line_edit.setText(file_path)
            self.value_changed.emit(file_path)

    def _play_audio(self):
        audio_path = self.line_edit.text()
        if not audio_path:
            return

        # Stop any currently playing audio
        self._stop_audio()

        # Play the audio file through editor bridge
        if self.editor_bridge:
            try:
                self.playing_source_uuid = self.editor_bridge.play_audio_file(
                    audio_path,
                    "Master",  # Use Master bus for preview
                    False,     # Don't loop
                    1.0        # Full volume
                )
                # Update button states
                self.play_button.setEnabled(False)
                self.pause_button.setEnabled(True)
                self.stop_button.setEnabled(True)
            except Exception as e:
                print(f"Error playing audio: {e}")

    def _pause_audio(self):
        # Note: miniaudio doesn't have true pause, so we'll just stop for now
        self._stop_audio()

    def _stop_audio(self):
        if self.playing_source_uuid and self.editor_bridge:
            try:
                self.editor_bridge.stop_audio(self.playing_source_uuid)
            except Exception as e:
                print(f"Error stopping audio: {e}")
            finally:
                self.playing_source_uuid = None
                # Update button states
                self.play_button.setEnabled(True)
                self.pause_button.setEnabled(False)
                self.stop_button.setEnabled(False)

    def set_value(self, value):
        self.line_edit.blockSignals(True)
        self.line_edit.setText(str(value) if value is not None else "")
        self.line_edit.blockSignals(False)

    def get_value(self):
        return self.line_edit.text()

    def reset_to_default(self):
        """Override to emit the correct string format"""
        # Stop any playing audio when resetting
        self._stop_audio()
        if self.default_value is not None:
            self.set_value(self.default_value)
            self.value_changed.emit(self.get_value())


class MaterialOverridePropertyWidget(PropertyWidget):
    """Widget for editing material override properties with collapsible categories"""

    def __init__(self, property_name, component, editor_bridge=None, parent=None):
        self.component = component
        self.editor_bridge = editor_bridge
        self.category_widgets = {}
        super().__init__(property_name, parent)

    def setup_ui(self):
        main_layout = QVBoxLayout()
        main_layout.setContentsMargins(0, 0, 0, 0)
        main_layout.setSpacing(2)

        # Header with enable checkbox and save/load buttons
        header_layout = QHBoxLayout()
        header_layout.setContentsMargins(0, 3, 0, 3)
        header_layout.setSpacing(4)

        self.enabled_checkbox = QCheckBox("Material Override")
        self.enabled_checkbox.setChecked(False)
        self.enabled_checkbox.toggled.connect(self._on_enabled_changed)
        header_layout.addWidget(self.enabled_checkbox)

        header_layout.addStretch()

        # TODO: Add save/load material buttons
        # save_btn = QPushButton("Save")
        # save_btn.setFixedWidth(50)
        # load_btn = QPushButton("Load")
        # load_btn.setFixedWidth(50)
        # header_layout.addWidget(save_btn)
        # header_layout.addWidget(load_btn)

        main_layout.addLayout(header_layout)

        # Container for collapsible categories
        self.categories_widget = QWidget()
        self.categories_layout = QVBoxLayout()
        self.categories_layout.setContentsMargins(10, 0, 0, 0)
        self.categories_layout.setSpacing(2)
        self.categories_widget.setLayout(self.categories_layout)
        self.categories_widget.setVisible(False)

        # Create collapsible categories
        self._create_albedo_category()
        self._create_metallic_roughness_category()
        self._create_normal_category()
        self._create_emissive_category()

        main_layout.addWidget(self.categories_widget)
        self.setLayout(main_layout)

    def _create_albedo_category(self):
        """Create Albedo category with color and texture"""
        category = self._create_category("Albedo")

        # Albedo Color
        color_widget = ColorPropertyWidget("Color")
        color_widget.value_changed.connect(lambda v: self._set_albedo_color(v))
        category['layout'].addWidget(color_widget)
        category['widgets']['color'] = color_widget

        # Albedo Texture
        texture_widget = PathPropertyWidget("Texture")
        texture_widget.value_changed.connect(lambda v: self._set_albedo_texture(v))
        category['layout'].addWidget(texture_widget)
        category['widgets']['texture'] = texture_widget

    def _create_metallic_roughness_category(self):
        """Create Metallic/Roughness category"""
        category = self._create_category("Metallic / Roughness")

        # Metallic
        metallic_widget = FloatPropertyWidget("Metallic")
        metallic_widget.value_changed.connect(lambda v: self._set_metallic(v))
        category['layout'].addWidget(metallic_widget)
        category['widgets']['metallic'] = metallic_widget

        # Roughness
        roughness_widget = FloatPropertyWidget("Roughness")
        roughness_widget.value_changed.connect(lambda v: self._set_roughness(v))
        category['layout'].addWidget(roughness_widget)
        category['widgets']['roughness'] = roughness_widget

        # Metallic/Roughness Texture
        texture_widget = PathPropertyWidget("Texture")
        texture_widget.value_changed.connect(lambda v: self._set_metallic_roughness_texture(v))
        category['layout'].addWidget(texture_widget)
        category['widgets']['texture'] = texture_widget

    def _create_normal_category(self):
        """Create Normal category"""
        category = self._create_category("Normal")

        # Normal Texture
        texture_widget = PathPropertyWidget("Texture")
        texture_widget.value_changed.connect(lambda v: self._set_normal_texture(v))
        category['layout'].addWidget(texture_widget)
        category['widgets']['texture'] = texture_widget

        # Normal Scale
        scale_widget = FloatPropertyWidget("Scale")
        scale_widget.value_changed.connect(lambda v: self._set_normal_scale(v))
        category['layout'].addWidget(scale_widget)
        category['widgets']['scale'] = scale_widget

    def _create_emissive_category(self):
        """Create Emissive category"""
        category = self._create_category("Emissive")

        # Emissive Color
        color_widget = ColorPropertyWidget("Color")
        color_widget.value_changed.connect(lambda v: self._set_emissive_color(v))
        category['layout'].addWidget(color_widget)
        category['widgets']['color'] = color_widget

        # Emissive Texture
        texture_widget = PathPropertyWidget("Texture")
        texture_widget.value_changed.connect(lambda v: self._set_emissive_texture(v))
        category['layout'].addWidget(texture_widget)
        category['widgets']['texture'] = texture_widget

        # Emissive Strength
        strength_widget = FloatPropertyWidget("Strength")
        strength_widget.value_changed.connect(lambda v: self._set_emissive_strength(v))
        category['layout'].addWidget(strength_widget)
        category['widgets']['strength'] = strength_widget

    def _create_category(self, name):
        """Create a collapsible category group"""
        # Get theme colors
        from editor.theme import get_theme_manager
        theme = get_theme_manager().get_current_theme()
        colors = theme.colors
        
        # Category header button
        header_btn = QPushButton(f"▶ {name}")
        header_btn.setCheckable(True)
        header_btn.setStyleSheet(f"""
            QPushButton {{
                text-align: left;
                padding: 4px;
                border: 1px solid {colors.border};
                background-color: {colors.surface};
                color: {colors.text_primary};
            }}
            QPushButton:checked {{
                background-color: {colors.surface_hover};
            }}
            QPushButton:hover {{
                background-color: {colors.surface_hover};
            }}
        """)

        # Category content widget
        content_widget = QWidget()
        content_layout = QVBoxLayout()
        content_layout.setContentsMargins(10, 2, 0, 2)
        content_layout.setSpacing(2)
        content_widget.setLayout(content_layout)
        content_widget.setVisible(False)

        # Connect toggle
        def toggle_category():
            is_expanded = header_btn.isChecked()
            content_widget.setVisible(is_expanded)
            header_btn.setText(f"{'▼' if is_expanded else '▶'} {name}")

        header_btn.toggled.connect(toggle_category)

        # Add to categories layout
        self.categories_layout.addWidget(header_btn)
        self.categories_layout.addWidget(content_widget)

        # Store category info
        category = {
            'header': header_btn,
            'content': content_widget,
            'layout': content_layout,
            'widgets': {}
        }
        self.category_widgets[name] = category
        return category

    def _on_enabled_changed(self, enabled):
        """Handle material override enabled/disabled"""
        self.categories_widget.setVisible(enabled)
        if self.component and self.editor_bridge:
            try:
                import json
                self.editor_bridge.set_component_property(self.component, 'materialOverrideEnabled', json.dumps(enabled))
            except Exception as e:
                print(f"[Warning] Failed to set material override enabled: {e}")
        self.value_changed.emit(enabled)

    # Material property setters
    def _set_albedo_color(self, color):
        if self.component and self.editor_bridge:
            try:
                import json
                # Handle both QColor and tuple (r, g, b, a) formats
                if isinstance(color, QColor):
                    color_dict = {'r': color.redF(), 'g': color.greenF(), 'b': color.blueF(), 'a': color.alphaF()}
                elif isinstance(color, (tuple, list)) and len(color) >= 3:
                    color_dict = {'r': float(color[0]), 'g': float(color[1]), 'b': float(color[2]), 'a': float(color[3]) if len(color) > 3 else 1.0}
                else:
                    print(f"[Warning] Invalid color format: {color}")
                    return
                self.editor_bridge.set_component_property(self.component, 'albedoColor', json.dumps(color_dict))
            except Exception as e:
                print(f"[Warning] Failed to set albedo color: {e}")

    def _set_albedo_texture(self, path):
        # TODO: Load texture and set handle
        pass

    def _set_metallic(self, value):
        if self.component and self.editor_bridge:
            try:
                import json
                self.editor_bridge.set_component_property(self.component, 'metallic', json.dumps(float(value)))
            except Exception as e:
                print(f"[Warning] Failed to set metallic: {e}")

    def _set_roughness(self, value):
        if self.component and self.editor_bridge:
            try:
                import json
                self.editor_bridge.set_component_property(self.component, 'roughness', json.dumps(float(value)))
            except Exception as e:
                print(f"[Warning] Failed to set roughness: {e}")

    def _set_metallic_roughness_texture(self, path):
        # TODO: Load texture and set handle
        pass

    def _set_normal_texture(self, path):
        # TODO: Load texture and set handle
        pass

    def _set_normal_scale(self, value):
        if self.component and self.editor_bridge:
            try:
                import json
                self.editor_bridge.set_component_property(self.component, 'normalScale', json.dumps(float(value)))
            except Exception as e:
                print(f"[Warning] Failed to set normal scale: {e}")

    def _set_emissive_color(self, color):
        if self.component and self.editor_bridge:
            try:
                import json
                # Handle both QColor and tuple (r, g, b, a) formats
                if isinstance(color, QColor):
                    color_dict = {'r': color.redF(), 'g': color.greenF(), 'b': color.blueF(), 'a': color.alphaF()}
                elif isinstance(color, (tuple, list)) and len(color) >= 3:
                    color_dict = {'r': float(color[0]), 'g': float(color[1]), 'b': float(color[2]), 'a': float(color[3]) if len(color) > 3 else 1.0}
                else:
                    print(f"[Warning] Invalid color format: {color}")
                    return
                self.editor_bridge.set_component_property(self.component, 'emissiveColor', json.dumps(color_dict))
            except Exception as e:
                print(f"[Warning] Failed to set emissive color: {e}")

    def _set_emissive_texture(self, path):
        # TODO: Load texture and set handle
        pass

    def _set_emissive_strength(self, value):
        if self.component and self.editor_bridge:
            try:
                import json
                self.editor_bridge.set_component_property(self.component, 'emissiveStrength', json.dumps(float(value)))
            except Exception as e:
                print(f"[Warning] Failed to set emissive strength: {e}")

    def set_value(self, value):
        """Set material override values from component"""
        if not self.component or not self.editor_bridge:
            return

        # Set enabled state - use EditorBridge to get properties
        has_override = False
        try:
            import json
            props_json = self.editor_bridge.get_component_properties(self.component)
            properties = json.loads(props_json) if props_json else {}
            if 'properties' in properties and 'materialOverrideEnabled' in properties['properties']:
                has_override = properties['properties']['materialOverrideEnabled']
        except Exception as e:
            print(f"[Warning] Failed to get material override state: {e}")

        self.enabled_checkbox.blockSignals(True)
        self.enabled_checkbox.setChecked(has_override)
        self.enabled_checkbox.blockSignals(False)
        self.categories_widget.setVisible(has_override)

        if has_override:
            try:
                import json
                props_json = self.editor_bridge.get_component_properties(self.component)
                all_properties = json.loads(props_json) if props_json else {}
                properties = all_properties.get('properties', {})

                # Update albedo
                if 'albedoColor' in properties and 'color' in self.category_widgets.get('Albedo', {}).get('widgets', {}):
                    albedo_color = properties['albedoColor']
                    qcolor = QColor.fromRgbF(albedo_color.get('r', 1.0), albedo_color.get('g', 1.0),
                                             albedo_color.get('b', 1.0), albedo_color.get('a', 1.0))
                    self.category_widgets['Albedo']['widgets']['color'].set_value(qcolor)

                # Update metallic/roughness
                if 'Metallic / Roughness' in self.category_widgets:
                    widgets = self.category_widgets['Metallic / Roughness']['widgets']
                    if 'metallic' in widgets and 'metallic' in properties:
                        widgets['metallic'].set_value(properties['metallic'])
                    if 'roughness' in widgets and 'roughness' in properties:
                        widgets['roughness'].set_value(properties['roughness'])

                # Update normal scale
                if 'Normal' in self.category_widgets and 'scale' in self.category_widgets['Normal']['widgets'] and 'normalScale' in properties:
                    self.category_widgets['Normal']['widgets']['scale'].set_value(properties['normalScale'])

                # Update emissive
                if 'Emissive' in self.category_widgets:
                    widgets = self.category_widgets['Emissive']['widgets']
                    if 'emissiveColor' in properties and 'color' in widgets:
                        emissive_color = properties['emissiveColor']
                        qcolor = QColor.fromRgbF(emissive_color.get('r', 0.0), emissive_color.get('g', 0.0),
                                                 emissive_color.get('b', 0.0), emissive_color.get('a', 1.0))
                        widgets['color'].set_value(qcolor)
                    if 'strength' in widgets and 'emissiveStrength' in properties:
                        widgets['strength'].set_value(properties['emissiveStrength'])
            except Exception as e:
                print(f"[Warning] Failed to load material override values: {e}")

    def get_value(self):
        """Get current material override state"""
        return self.enabled_checkbox.isChecked()


class ButtonStatePropertyWidget(PropertyWidget):
    """Widget for editing Button state properties with automatic/manual mode toggle"""

    def __init__(self, property_name, component, editor_bridge=None, parent=None):
        self.component = component
        self.editor_bridge = editor_bridge
        self.state_widgets = {}
        self.is_automatic_mode = True
        super().__init__(property_name, parent)

    def setup_ui(self):
        main_layout = QVBoxLayout()
        main_layout.setContentsMargins(0, 0, 0, 0)
        main_layout.setSpacing(2)

        # Header with mode toggle
        header_layout = QHBoxLayout()
        header_layout.setContentsMargins(0, 3, 0, 3)
        header_layout.setSpacing(4)

        header_label = QLabel("Button States")
        header_label.setStyleSheet("font-weight: bold;")
        header_layout.addWidget(header_label)

        header_layout.addStretch()

        # Mode toggle
        self.mode_combo = QComboBox()
        self.mode_combo.addItems(["Automatic", "Manual"])
        self.mode_combo.currentIndexChanged.connect(self._on_mode_changed)
        header_layout.addWidget(QLabel("Mode:"))
        header_layout.addWidget(self.mode_combo)

        main_layout.addLayout(header_layout)

        # Container for state editors
        self.states_widget = QWidget()
        self.states_layout = QVBoxLayout()
        self.states_layout.setContentsMargins(10, 0, 0, 0)
        self.states_layout.setSpacing(2)
        self.states_widget.setLayout(self.states_layout)

        # Create state editors
        self._create_state_editor("Normal", 0)
        self._create_state_editor("Hover", 1)
        self._create_state_editor("Pressed", 2)
        self._create_state_editor("Disabled", 3)

        main_layout.addWidget(self.states_widget)
        self.setLayout(main_layout)

    def _create_state_editor(self, state_name, state_index):
        """Create a collapsible state editor"""
        from editor.theme import get_theme_manager
        theme = get_theme_manager().get_current_theme()
        colors = theme.colors if theme else None

        # State header button
        header_btn = QPushButton(f"▶ {state_name}")
        header_btn.setCheckable(True)
        if colors:
            header_btn.setStyleSheet(f"""
                QPushButton {{
                    text-align: left;
                    padding: 4px;
                    border: 1px solid {colors.border};
                    background-color: {colors.surface};
                    color: {colors.text_primary};
                }}
                QPushButton:checked {{
                    background-color: {colors.surface_hover};
                }}
                QPushButton:hover {{
                    background-color: {colors.surface_hover};
                }}
            """)

        # State content widget
        content_widget = QWidget()
        content_layout = QVBoxLayout()
        content_layout.setContentsMargins(10, 2, 0, 2)
        content_layout.setSpacing(2)
        content_widget.setLayout(content_layout)
        content_widget.setVisible(False)

        # Modulation color (for automatic mode)
        modulation_widget = ColorPropertyWidget(f"{state_name} Modulation")
        modulation_widget.value_changed.connect(
            lambda v: self._set_state_modulation(state_name.lower(), v))
        content_layout.addWidget(modulation_widget)

        # StyleBox (for manual mode) - show background color, border, corner radius
        stylebox_container = QWidget()
        stylebox_layout = QVBoxLayout()
        stylebox_layout.setContentsMargins(0, 0, 0, 0)
        stylebox_layout.setSpacing(2)
        stylebox_container.setLayout(stylebox_layout)
        content_layout.addWidget(stylebox_container)

        # StyleBox background color
        stylebox_bg_widget = ColorPropertyWidget(f"{state_name} BG Color")
        stylebox_bg_widget.value_changed.connect(
            lambda v: self._set_state_stylebox_property(state_name.lower(), 'BackgroundColor', v))
        stylebox_layout.addWidget(stylebox_bg_widget)

        # StyleBox border color
        stylebox_border_widget = ColorPropertyWidget(f"{state_name} Border Color")
        stylebox_border_widget.value_changed.connect(
            lambda v: self._set_state_stylebox_property(state_name.lower(), 'BorderColor', v))
        stylebox_layout.addWidget(stylebox_border_widget)

        # Sound path
        sound_widget = PathPropertyWidget(f"{state_name} Sound")
        sound_widget.value_changed.connect(
            lambda v: self._set_state_sound(state_name.lower(), v))
        content_layout.addWidget(sound_widget)

        # Tween enabled checkbox
        tween_enabled_widget = BoolPropertyWidget(f"{state_name} Tween Enabled")
        tween_enabled_widget.value_changed.connect(
            lambda v: self._on_tween_enabled_changed(state_name, v))
        content_layout.addWidget(tween_enabled_widget)

        # Tween settings container (hidden by default)
        tween_container = QWidget()
        tween_layout = QVBoxLayout()
        tween_layout.setContentsMargins(10, 2, 0, 2)
        tween_layout.setSpacing(2)
        tween_container.setLayout(tween_layout)
        tween_container.setVisible(False)
        content_layout.addWidget(tween_container)

        # Tween scale
        tween_scale_widget = Vec2PropertyWidget(f"{state_name} Tween Scale")
        tween_scale_widget.value_changed.connect(
            lambda v: self._set_state_tween_property(state_name.lower(), 'Scale', v))
        tween_layout.addWidget(tween_scale_widget)

        # Tween rotation
        tween_rotation_widget = FloatPropertyWidget(f"{state_name} Tween Rotation")
        tween_rotation_widget.value_changed.connect(
            lambda v: self._set_state_tween_property(state_name.lower(), 'Rotation', v))
        tween_layout.addWidget(tween_rotation_widget)

        # Tween position
        tween_position_widget = Vec2PropertyWidget(f"{state_name} Tween Position")
        tween_position_widget.value_changed.connect(
            lambda v: self._set_state_tween_property(state_name.lower(), 'Position', v))
        tween_layout.addWidget(tween_position_widget)

        # Tween duration
        tween_duration_widget = FloatPropertyWidget(f"{state_name} Tween Duration")
        tween_duration_widget.value_changed.connect(
            lambda v: self._set_state_tween_property(state_name.lower(), 'Duration', v))
        tween_layout.addWidget(tween_duration_widget)

        # Preview button
        preview_btn = QPushButton(f"Preview {state_name} Tween")
        preview_btn.clicked.connect(lambda: self._preview_tween(state_name))
        tween_layout.addWidget(preview_btn)

        # Connect toggle
        def toggle_state():
            is_expanded = header_btn.isChecked()
            content_widget.setVisible(is_expanded)
            header_btn.setText(f"{'▼' if is_expanded else '▶'} {state_name}")

        header_btn.toggled.connect(toggle_state)

        # Add to states layout
        self.states_layout.addWidget(header_btn)
        self.states_layout.addWidget(content_widget)

        # Store state info
        self.state_widgets[state_name] = {
            'header': header_btn,
            'content': content_widget,
            'layout': content_layout,
            'modulation': modulation_widget,
            'stylebox_container': stylebox_container,
            'stylebox_bg': stylebox_bg_widget,
            'stylebox_border': stylebox_border_widget,
            'sound': sound_widget,
            'tween_enabled': tween_enabled_widget,
            'tween_container': tween_container,
            'tween_scale': tween_scale_widget,
            'tween_rotation': tween_rotation_widget,
            'tween_position': tween_position_widget,
            'tween_duration': tween_duration_widget,
            'preview_btn': preview_btn
        }

    def _on_mode_changed(self, index):
        """Handle mode change between Automatic and Manual"""
        self.is_automatic_mode = (index == 0)

        # Update visibility of mode-specific widgets
        for state_name, widgets in self.state_widgets.items():
            if 'modulation' in widgets:
                widgets['modulation'].setVisible(self.is_automatic_mode)
            if 'stylebox_container' in widgets:
                widgets['stylebox_container'].setVisible(not self.is_automatic_mode)

        # Set property on component
        if self.component and self.editor_bridge:
            try:
                import json
                self.editor_bridge.set_component_property(
                    self.component, 'styleMode', json.dumps(index))
            except Exception as e:
                print(f"[Warning] Failed to set button style mode: {e}")

    def _set_state_modulation(self, state_name, color):
        """Set state modulation color"""
        if self.component and self.editor_bridge:
            try:
                import json
                prop_name = f"{state_name}Modulation"
                if isinstance(color, QColor):
                    color_dict = {'r': color.redF(), 'g': color.greenF(),
                                'b': color.blueF(), 'a': color.alphaF()}
                elif isinstance(color, (tuple, list)) and len(color) >= 3:
                    color_dict = {'r': float(color[0]), 'g': float(color[1]),
                                'b': float(color[2]), 'a': float(color[3]) if len(color) > 3 else 1.0}
                else:
                    return
                self.editor_bridge.set_component_property(
                    self.component, prop_name, json.dumps(color_dict))
            except Exception as e:
                print(f"[Warning] Failed to set {state_name} modulation: {e}")

    def _set_state_sound(self, state_name, path):
        """Set state sound path"""
        if self.component and self.editor_bridge:
            try:
                import json
                prop_name = f"{state_name}SoundPath"
                self.editor_bridge.set_component_property(
                    self.component, prop_name, json.dumps(path))
            except Exception as e:
                print(f"[Warning] Failed to set {state_name} sound: {e}")

    def _set_state_stylebox_property(self, state_name, prop_suffix, value):
        """Set state StyleBox property (BackgroundColor, BorderColor, etc.)"""
        if self.component and self.editor_bridge:
            try:
                import json
                # For manual mode, we need to set properties on the StyleBox
                # For now, we'll just log - full StyleBox implementation needed
                print(f"[Info] Set {state_name} StyleBox {prop_suffix}: {value}")
                # TODO: Implement StyleBox property setting through bridge
            except Exception as e:
                print(f"[Warning] Failed to set {state_name} StyleBox {prop_suffix}: {e}")

    def _set_state_tween_enabled(self, state_name, enabled):
        """Set state tween enabled"""
        if self.component and self.editor_bridge:
            try:
                import json
                prop_name = f"{state_name}TweenEnabled"
                self.editor_bridge.set_component_property(
                    self.component, prop_name, json.dumps(enabled))
            except Exception as e:
                print(f"[Warning] Failed to set {state_name} tween enabled: {e}")

    def _on_tween_enabled_changed(self, state_name, enabled):
        """Handle tween enabled checkbox change"""
        # Update tween container visibility
        widgets = self.state_widgets.get(state_name, {})
        if 'tween_container' in widgets:
            widgets['tween_container'].setVisible(enabled)

        # Set property
        self._set_state_tween_enabled(state_name.lower(), enabled)

    def _set_state_tween_property(self, state_name, prop_suffix, value):
        """Set state tween property (Scale, Rotation, Position, Duration)"""
        if self.component and self.editor_bridge:
            try:
                import json
                prop_name = f"{state_name}Tween{prop_suffix}"

                # Convert value to appropriate format
                if isinstance(value, (tuple, list)) and len(value) >= 2:
                    # Vec2
                    value_dict = {'x': float(value[0]), 'y': float(value[1])}
                    self.editor_bridge.set_component_property(
                        self.component, prop_name, json.dumps(value_dict))
                else:
                    # Float
                    self.editor_bridge.set_component_property(
                        self.component, prop_name, json.dumps(float(value)))
            except Exception as e:
                print(f"[Warning] Failed to set {state_name} tween {prop_suffix}: {e}")

    def _preview_tween(self, state_name):
        """Preview tween for a specific state"""
        if self.component and self.editor_bridge:
            try:
                # Call PreviewTween method on the component
                # Map state name to ButtonState enum value
                state_map = {'Normal': 0, 'Hover': 1, 'Pressed': 2, 'Disabled': 3}
                state_value = state_map.get(state_name, 0)

                # Call the C++ method directly
                # The component has a PreviewTween(ButtonState) method
                self.component.PreviewTween(state_value)
                print(f"[Info] Previewed tween for {state_name} (state={state_value})")
            except Exception as e:
                print(f"[Warning] Failed to preview tween for {state_name}: {e}")

    def set_value(self, value):
        """Set button state values from component"""
        if not self.component or not self.editor_bridge:
            return

        try:
            import json
            props_json = self.editor_bridge.get_component_properties(self.component)
            properties = json.loads(props_json) if props_json else {}
            props = properties.get('properties', {})

            # Set mode
            if 'styleMode' in props:
                self.mode_combo.blockSignals(True)
                self.mode_combo.setCurrentIndex(props['styleMode'])
                self.mode_combo.blockSignals(False)
                self.is_automatic_mode = (props['styleMode'] == 0)

            # Update state widgets
            for state_name in ['Normal', 'Hover', 'Pressed', 'Disabled']:
                state_lower = state_name.lower()
                widgets = self.state_widgets.get(state_name, {})

                # Modulation color
                mod_prop = f"{state_lower}Modulation"
                if mod_prop in props and 'modulation' in widgets:
                    color = props[mod_prop]
                    qcolor = QColor.fromRgbF(color.get('r', 1.0), color.get('g', 1.0),
                                           color.get('b', 1.0), color.get('a', 1.0))
                    widgets['modulation'].set_value(qcolor)

                # Sound path
                sound_prop = f"{state_lower}SoundPath"
                if sound_prop in props and 'sound' in widgets:
                    widgets['sound'].set_value(props[sound_prop])

                # Tween enabled
                tween_enabled_prop = f"{state_lower}TweenEnabled"
                tween_enabled = props.get(tween_enabled_prop, False)
                if 'tween_enabled' in widgets:
                    widgets['tween_enabled'].set_value(tween_enabled)

                # Show/hide tween container based on enabled state
                if 'tween_container' in widgets:
                    widgets['tween_container'].setVisible(tween_enabled)

                # Tween scale
                tween_scale_prop = f"{state_lower}TweenScale"
                if tween_scale_prop in props and 'tween_scale' in widgets:
                    scale = props[tween_scale_prop]
                    widgets['tween_scale'].set_value((scale.get('x', 0.0), scale.get('y', 0.0)))

                # Tween rotation
                tween_rotation_prop = f"{state_lower}TweenRotation"
                if tween_rotation_prop in props and 'tween_rotation' in widgets:
                    widgets['tween_rotation'].set_value(props[tween_rotation_prop])

                # Tween position
                tween_position_prop = f"{state_lower}TweenPosition"
                if tween_position_prop in props and 'tween_position' in widgets:
                    pos = props[tween_position_prop]
                    widgets['tween_position'].set_value((pos.get('x', 0.0), pos.get('y', 0.0)))

                # Tween duration
                tween_duration_prop = f"{state_lower}TweenDuration"
                if tween_duration_prop in props and 'tween_duration' in widgets:
                    widgets['tween_duration'].set_value(props[tween_duration_prop])

            # Update visibility based on mode
            for state_name, widgets in self.state_widgets.items():
                if 'modulation' in widgets:
                    widgets['modulation'].setVisible(self.is_automatic_mode)
                if 'stylebox_container' in widgets:
                    widgets['stylebox_container'].setVisible(not self.is_automatic_mode)

        except Exception as e:
            print(f"[Warning] Failed to load button state properties: {e}")

    def get_value(self):
        """Get current button state mode"""
        return self.mode_combo.currentIndex()


class WorldEnvironmentPropertyWidget(PropertyWidget):
    """Custom widget for WorldEnvironment component with collapsable categories and dynamic fields"""

    def __init__(self, property_name, component, editor_bridge=None, parent=None):
        self.component = component
        self.editor_bridge = editor_bridge
        self.category_widgets = {}
        super().__init__(property_name, parent)

    def setup_ui(self):
        main_layout = QVBoxLayout()
        main_layout.setContentsMargins(0, 0, 0, 0)
        main_layout.setSpacing(2)

        # Create collapsable categories
        self._create_skybox_category(main_layout)
        self._create_fog_category(main_layout)
        self._create_ambient_light_category(main_layout)
        self._create_volumetric_fog_category(main_layout)

        self.setLayout(main_layout)

    def _create_float_widget(self, label_text, default_value, min_val=None, max_val=None, step=None, decimals=3):
        """Create a custom float widget with specified parameters"""
        widget = QWidget()
        layout = QHBoxLayout()
        layout.setContentsMargins(0, 2, 0, 2)
        layout.setSpacing(5)

        label = QLabel(label_text)
        label.setFixedWidth(120)
        layout.addWidget(label)

        spinbox = QDoubleSpinBox()
        spinbox.setRange(min_val if min_val is not None else -1e10,
                        max_val if max_val is not None else 1e10)
        spinbox.setDecimals(decimals)
        spinbox.setSingleStep(step if step is not None else 0.1)
        spinbox.setValue(default_value)
        spinbox.setMinimumWidth(60)
        spinbox.setMinimumHeight(28)
        layout.addWidget(spinbox)

        widget.setLayout(layout)
        widget.spinbox = spinbox  # Store reference for easy access
        return widget

    def _create_category_header(self, title, is_expanded=True):
        """Create a collapsable category header button"""
        from editor.theme import get_theme_manager
        theme = get_theme_manager().get_current_theme()
        colors = theme.colors if theme else None

        header_btn = QPushButton(f"{'▼' if is_expanded else '▶'} {title}")
        header_btn.setCheckable(True)
        header_btn.setChecked(is_expanded)
        if colors:
            header_btn.setStyleSheet(f"""
                QPushButton {{
                    text-align: left;
                    padding: 6px;
                    border: 1px solid {colors.border};
                    background-color: {colors.surface};
                    color: {colors.text_primary};
                    font-weight: bold;
                }}
                QPushButton:checked {{
                    background-color: {colors.surface_hover};
                }}
                QPushButton:hover {{
                    background-color: {colors.surface_hover};
                }}
            """)

        return header_btn

    def _create_skybox_category(self, parent_layout):
        """Create skybox settings category"""
        # Header
        header_btn = self._create_category_header("Skybox", True)
        parent_layout.addWidget(header_btn)

        # Content widget
        content_widget = QWidget()
        content_layout = QVBoxLayout()
        content_layout.setContentsMargins(10, 2, 0, 2)
        content_layout.setSpacing(2)
        content_widget.setLayout(content_layout)

        # Skybox type enum
        skybox_type_widget = EnumPropertyWidget("Skybox Type",
            ["None", "Color", "Procedural", "Cubemap", "Panoramic"], 0)
        skybox_type_widget.value_changed.connect(self._on_skybox_type_changed)
        content_layout.addWidget(skybox_type_widget)

        # Color skybox settings (visible when type = Color)
        color_container = QWidget()
        color_layout = QVBoxLayout()
        color_layout.setContentsMargins(10, 0, 0, 0)
        color_layout.setSpacing(2)
        color_container.setLayout(color_layout)

        skybox_color_widget = ColorPropertyWidget("Skybox Color", (0.5, 0.7, 1.0, 1.0))
        skybox_color_widget.value_changed.connect(lambda v: self._set_property('skyboxColor', v))
        color_layout.addWidget(skybox_color_widget)

        content_layout.addWidget(color_container)
        color_container.setVisible(False)

        # Procedural skybox settings (visible when type = Procedural)
        procedural_container = QWidget()
        procedural_layout = QVBoxLayout()
        procedural_layout.setContentsMargins(10, 0, 0, 0)
        procedural_layout.setSpacing(2)
        procedural_container.setLayout(procedural_layout)

        sky_top_widget = ColorPropertyWidget("Sky Top Color", (0.1, 0.3, 0.8, 1.0))
        sky_top_widget.value_changed.connect(lambda v: self._set_property('skyTopColor', v))
        procedural_layout.addWidget(sky_top_widget)

        sky_horizon_widget = ColorPropertyWidget("Sky Horizon Color", (0.6, 0.7, 0.9, 1.0))
        sky_horizon_widget.value_changed.connect(lambda v: self._set_property('skyHorizonColor', v))
        procedural_layout.addWidget(sky_horizon_widget)

        sky_bottom_widget = ColorPropertyWidget("Sky Bottom Color", (0.8, 0.8, 0.8, 1.0))
        sky_bottom_widget.value_changed.connect(lambda v: self._set_property('skyBottomColor', v))
        procedural_layout.addWidget(sky_bottom_widget)

        content_layout.addWidget(procedural_container)
        procedural_container.setVisible(False)

        # Cubemap skybox settings (visible when type = Cubemap)
        cubemap_container = QWidget()
        cubemap_layout = QVBoxLayout()
        cubemap_layout.setContentsMargins(10, 0, 0, 0)
        cubemap_layout.setSpacing(2)
        cubemap_container.setLayout(cubemap_layout)

        for face_name in ["PosX", "NegX", "PosY", "NegY", "PosZ", "NegZ"]:
            face_widget = PathPropertyWidget(f"Cubemap {face_name}", "")
            face_widget.value_changed.connect(
                lambda v, name=face_name: self._set_property(f'cubemap{name}', v))
            cubemap_layout.addWidget(face_widget)

        content_layout.addWidget(cubemap_container)
        cubemap_container.setVisible(False)

        # Panoramic skybox settings (visible when type = Panoramic)
        panoramic_container = QWidget()
        panoramic_layout = QVBoxLayout()
        panoramic_layout.setContentsMargins(10, 0, 0, 0)
        panoramic_layout.setSpacing(2)
        panoramic_container.setLayout(panoramic_layout)

        panoramic_widget = PathPropertyWidget("Panoramic Texture", "")
        panoramic_widget.value_changed.connect(lambda v: self._set_property('panoramicTexture', v))
        panoramic_layout.addWidget(panoramic_widget)

        content_layout.addWidget(panoramic_container)
        panoramic_container.setVisible(False)

        parent_layout.addWidget(content_widget)

        # Store cubemap widgets for later access
        cubemap_widgets = {}
        for i, face_name in enumerate(["PosX", "NegX", "PosY", "NegY", "PosZ", "NegZ"]):
            cubemap_widgets[face_name.lower()] = cubemap_layout.itemAt(i).widget()

        # Store widgets for later access
        self.category_widgets['skybox'] = {
            'header': header_btn,
            'content': content_widget,
            'type_widget': skybox_type_widget,
            'color_container': color_container,
            'color_widget': skybox_color_widget,
            'procedural_container': procedural_container,
            'sky_top_widget': sky_top_widget,
            'sky_horizon_widget': sky_horizon_widget,
            'sky_bottom_widget': sky_bottom_widget,
            'cubemap_container': cubemap_container,
            'cubemap_widgets': cubemap_widgets,
            'panoramic_container': panoramic_container,
            'panoramic_widget': panoramic_widget
        }

        # Connect header toggle
        header_btn.toggled.connect(lambda checked: self._toggle_category('skybox', checked))

    def _create_fog_category(self, parent_layout):
        """Create fog settings category"""
        # Header
        header_btn = self._create_category_header("Fog", False)
        parent_layout.addWidget(header_btn)

        # Content widget
        content_widget = QWidget()
        content_layout = QVBoxLayout()
        content_layout.setContentsMargins(10, 2, 0, 2)
        content_layout.setSpacing(2)
        content_widget.setLayout(content_layout)
        content_widget.setVisible(False)

        # Fog enabled
        fog_enabled_widget = BoolPropertyWidget("Fog Enabled", False)
        fog_enabled_widget.value_changed.connect(lambda v: self._set_property('fogEnabled', v))
        content_layout.addWidget(fog_enabled_widget)

        # Fog color
        fog_color_widget = ColorPropertyWidget("Fog Color", (0.5, 0.5, 0.5, 1.0))
        fog_color_widget.value_changed.connect(lambda v: self._set_property('fogColor', v))
        content_layout.addWidget(fog_color_widget)

        # Fog mode
        fog_mode_widget = EnumPropertyWidget("Fog Mode",
            ["Linear", "Exponential", "Exponential Squared"], 0)
        fog_mode_widget.value_changed.connect(lambda v: self._set_property('fogMode', v))
        content_layout.addWidget(fog_mode_widget)

        # Fog density
        fog_density_widget = self._create_float_widget("Fog Density", 0.01, 0.0, 1.0, 0.001)
        fog_density_widget.spinbox.valueChanged.connect(lambda v: self._set_property('fogDensity', v))
        content_layout.addWidget(fog_density_widget)

        # Fog start
        fog_start_widget = self._create_float_widget("Fog Start", 10.0, 0.0, 1000.0, 1.0)
        fog_start_widget.spinbox.valueChanged.connect(lambda v: self._set_property('fogStart', v))
        content_layout.addWidget(fog_start_widget)

        # Fog end
        fog_end_widget = self._create_float_widget("Fog End", 100.0, 0.0, 10000.0, 1.0)
        fog_end_widget.spinbox.valueChanged.connect(lambda v: self._set_property('fogEnd', v))
        content_layout.addWidget(fog_end_widget)

        parent_layout.addWidget(content_widget)

        # Store widgets
        self.category_widgets['fog'] = {
            'header': header_btn,
            'content': content_widget,
            'enabled_widget': fog_enabled_widget,
            'color_widget': fog_color_widget,
            'mode_widget': fog_mode_widget,
            'density_widget': fog_density_widget,
            'start_widget': fog_start_widget,
            'end_widget': fog_end_widget
        }

        # Connect header toggle
        header_btn.toggled.connect(lambda checked: self._toggle_category('fog', checked))

    def _create_ambient_light_category(self, parent_layout):
        """Create ambient light settings category"""
        # Header
        header_btn = self._create_category_header("Ambient Light", False)
        parent_layout.addWidget(header_btn)

        # Content widget
        content_widget = QWidget()
        content_layout = QVBoxLayout()
        content_layout.setContentsMargins(10, 2, 0, 2)
        content_layout.setSpacing(2)
        content_widget.setLayout(content_layout)
        content_widget.setVisible(False)

        # Ambient light enabled
        ambient_enabled_widget = BoolPropertyWidget("Ambient Light Enabled", True)
        ambient_enabled_widget.value_changed.connect(lambda v: self._set_property('ambientLightEnabled', v))
        content_layout.addWidget(ambient_enabled_widget)

        # Ambient light color
        ambient_color_widget = ColorPropertyWidget("Ambient Light Color", (1.0, 1.0, 1.0, 1.0))
        ambient_color_widget.value_changed.connect(lambda v: self._set_property('ambientLightColor', v))
        content_layout.addWidget(ambient_color_widget)

        # Ambient light intensity
        ambient_intensity_widget = self._create_float_widget("Ambient Light Intensity", 0.2, 0.0, 10.0, 0.01)
        ambient_intensity_widget.spinbox.valueChanged.connect(lambda v: self._set_property('ambientLightIntensity', v))
        content_layout.addWidget(ambient_intensity_widget)

        parent_layout.addWidget(content_widget)

        # Store widgets
        self.category_widgets['ambient'] = {
            'header': header_btn,
            'content': content_widget,
            'enabled_widget': ambient_enabled_widget,
            'color_widget': ambient_color_widget,
            'intensity_widget': ambient_intensity_widget
        }

        # Connect header toggle
        header_btn.toggled.connect(lambda checked: self._toggle_category('ambient', checked))

    def _create_volumetric_fog_category(self, parent_layout):
        """Create volumetric fog settings category (disabled for OpenGL)"""
        # Header
        header_btn = self._create_category_header("Volumetric Fog (Not Available in OpenGL)", False)
        parent_layout.addWidget(header_btn)

        # Content widget
        content_widget = QWidget()
        content_layout = QVBoxLayout()
        content_layout.setContentsMargins(10, 2, 0, 2)
        content_layout.setSpacing(2)
        content_widget.setLayout(content_layout)
        content_widget.setVisible(False)

        # Disabled message
        disabled_label = QLabel("Volumetric fog is not available in the OpenGL backend.")
        disabled_label.setWordWrap(True)
        disabled_label.setStyleSheet("color: gray; font-style: italic;")
        content_layout.addWidget(disabled_label)

        parent_layout.addWidget(content_widget)

        # Store widgets
        self.category_widgets['volumetric'] = {
            'header': header_btn,
            'content': content_widget
        }

        # Connect header toggle
        header_btn.toggled.connect(lambda checked: self._toggle_category('volumetric', checked))

    def _toggle_category(self, category_name, checked):
        """Toggle category visibility"""
        if category_name in self.category_widgets:
            widgets = self.category_widgets[category_name]
            widgets['content'].setVisible(checked)
            # Update arrow
            header = widgets['header']
            text = header.text()
            if checked:
                header.setText(text.replace('▶', '▼'))
            else:
                header.setText(text.replace('▼', '▶'))

    def _on_skybox_type_changed(self, skybox_type):
        """Handle skybox type change - show/hide appropriate fields"""
        if 'skybox' not in self.category_widgets:
            return

        widgets = self.category_widgets['skybox']

        # Hide all containers
        widgets['color_container'].setVisible(False)
        widgets['procedural_container'].setVisible(False)
        widgets['cubemap_container'].setVisible(False)
        widgets['panoramic_container'].setVisible(False)

        # Show appropriate container based on type
        if skybox_type == 1:  # Color
            widgets['color_container'].setVisible(True)
        elif skybox_type == 2:  # Procedural
            widgets['procedural_container'].setVisible(True)
        elif skybox_type == 3:  # Cubemap
            widgets['cubemap_container'].setVisible(True)
        elif skybox_type == 4:  # Panoramic
            widgets['panoramic_container'].setVisible(True)

        # Set property
        self._set_property('skyboxType', skybox_type)

    def _set_property(self, prop_name, value):
        """Set component property through editor bridge"""
        if self.component and self.editor_bridge:
            try:
                import json
                # Convert color tuples to dict format
                if isinstance(value, (tuple, list)) and len(value) == 4:
                    value_dict = {'r': float(value[0]), 'g': float(value[1]),
                                  'b': float(value[2]), 'a': float(value[3])}
                    self.editor_bridge.set_component_property(
                        self.component, prop_name, json.dumps(value_dict))
                else:
                    self.editor_bridge.set_component_property(
                        self.component, prop_name, json.dumps(value))
            except Exception as e:
                print(f"[Warning] Failed to set WorldEnvironment property {prop_name}: {e}")

    def set_value(self, value):
        """Load values from component"""
        if not self.component or not self.editor_bridge:
            return

        try:
            import json
            props_json = self.editor_bridge.get_component_properties(self.component)
            properties = json.loads(props_json) if props_json else {}
            props = properties.get('properties', {})

            # Helper function to convert color dict to tuple
            def color_to_tuple(color_dict):
                if isinstance(color_dict, dict):
                    return (color_dict.get('r', 0.0), color_dict.get('g', 0.0),
                            color_dict.get('b', 0.0), color_dict.get('a', 1.0))
                return (0.5, 0.5, 0.5, 1.0)

            # Load skybox properties
            if 'skybox' in self.category_widgets:
                skybox = self.category_widgets['skybox']

                # Skybox type
                skybox_type = props.get('skyboxType', 0)
                if 'type_widget' in skybox:
                    skybox['type_widget'].set_value(skybox_type)
                    # Update visibility of containers based on type
                    self._on_skybox_type_changed(skybox_type)

                # Color skybox
                if 'color_widget' in skybox and 'skyboxColor' in props:
                    skybox['color_widget'].set_value(color_to_tuple(props['skyboxColor']))

                # Procedural skybox
                if 'sky_top_widget' in skybox and 'skyTopColor' in props:
                    skybox['sky_top_widget'].set_value(color_to_tuple(props['skyTopColor']))
                if 'sky_horizon_widget' in skybox and 'skyHorizonColor' in props:
                    skybox['sky_horizon_widget'].set_value(color_to_tuple(props['skyHorizonColor']))
                if 'sky_bottom_widget' in skybox and 'skyBottomColor' in props:
                    skybox['sky_bottom_widget'].set_value(color_to_tuple(props['skyBottomColor']))

                # Cubemap skybox
                if 'cubemap_widgets' in skybox:
                    for face_key, widget in skybox['cubemap_widgets'].items():
                        prop_key = f'cubemap{face_key.capitalize()}'
                        if prop_key in props:
                            widget.set_value(props[prop_key])

                # Panoramic skybox
                if 'panoramic_widget' in skybox and 'panoramicTexture' in props:
                    skybox['panoramic_widget'].set_value(props['panoramicTexture'])

            # Load fog properties
            if 'fog' in self.category_widgets:
                fog = self.category_widgets['fog']

                if 'enabled_widget' in fog and 'fogEnabled' in props:
                    fog['enabled_widget'].set_value(props['fogEnabled'])
                if 'color_widget' in fog and 'fogColor' in props:
                    fog['color_widget'].set_value(color_to_tuple(props['fogColor']))
                if 'mode_widget' in fog and 'fogMode' in props:
                    fog['mode_widget'].set_value(props['fogMode'])
                if 'density_widget' in fog and 'fogDensity' in props:
                    fog['density_widget'].spinbox.setValue(props['fogDensity'])
                if 'start_widget' in fog and 'fogStart' in props:
                    fog['start_widget'].spinbox.setValue(props['fogStart'])
                if 'end_widget' in fog and 'fogEnd' in props:
                    fog['end_widget'].spinbox.setValue(props['fogEnd'])

            # Load ambient light properties
            if 'ambient' in self.category_widgets:
                ambient = self.category_widgets['ambient']

                if 'enabled_widget' in ambient and 'ambientLightEnabled' in props:
                    ambient['enabled_widget'].set_value(props['ambientLightEnabled'])
                if 'color_widget' in ambient and 'ambientLightColor' in props:
                    ambient['color_widget'].set_value(color_to_tuple(props['ambientLightColor']))
                if 'intensity_widget' in ambient and 'ambientLightIntensity' in props:
                    ambient['intensity_widget'].spinbox.setValue(props['ambientLightIntensity'])

        except Exception as e:
            print(f"[Warning] Failed to load WorldEnvironment values: {e}")

    def get_value(self):
        """Get current values"""
        return None


class InspectorPanel(EditorPanel):
    """Properties inspector panel for editing object properties"""

    # Signals
    property_changed = pyqtSignal(str, object, object)  # property name, old value, new value
    component_added = pyqtSignal(str)  # component type
    component_removed = pyqtSignal(object)  # component instance

    def __init__(self, parent=None):
        super().__init__("Inspector", parent)
        self.setObjectName("InspectorPanel")
        self.main_editor = parent
        self.editor_bridge = None
        self.current_node = None
        self.property_widgets = {}
        self.update_timer = QTimer()
        self.update_timer.setSingleShot(True)
        self.update_timer.timeout.connect(self._apply_name_change)
        self._active_connections = []  # Track active signal connections
        self._material_widgets = {}  # Track material widgets per component for refresh

    def _setup_panel(self):
        """Setup inspector panel UI"""
        # Header with node name editor
        header_layout = QVBoxLayout()
        header_layout.setContentsMargins(3, 3, 3, 3)
        header_layout.setSpacing(2)

        name_label = QLabel("Node Name")
        name_label.setStyleSheet("font-weight: bold;")
        header_layout.addWidget(name_label)

        self.node_name_edit = QLineEdit()
        self.node_name_edit.setPlaceholderText("No Selection")
        self.node_name_edit.setEnabled(False)
        self.node_name_edit.textChanged.connect(self._on_name_changed)
        header_layout.addWidget(self.node_name_edit)

        # Scrollable properties area with horizontal scrolling support
        scroll_area = QScrollArea()
        scroll_area.setWidgetResizable(True)
        scroll_area.setFrameShape(QScrollArea.Shape.NoFrame)
        # Allow horizontal scrolling when panel is too narrow
        scroll_area.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAsNeeded)

        self.properties_widget = QWidget()
        self.properties_layout = QVBoxLayout()
        v_spacing, h_padding = get_theme_spacing()
        self.properties_layout.setContentsMargins(h_padding, h_padding, h_padding, h_padding)
        self.properties_layout.setSpacing(v_spacing)
        self.properties_widget.setLayout(self.properties_layout)

        scroll_area.setWidget(self.properties_widget)

        # Add to layout
        self.content_layout.addLayout(header_layout)
        self.content_layout.addWidget(scroll_area)

    def set_node(self, node):
        """Set the node to inspect"""
        self.current_node = node
        self.refresh_properties()

    def _clear_layout(self, layout):
        """Recursively clear a layout and delete all contained widgets and sub-layouts."""
        while layout.count():
            item = layout.takeAt(0)
            widget = item.widget()
            child_layout = item.layout()
            if widget is not None:
                # Disconnect all signals from the widget and its children before deleting
                self._disconnect_widget_signals(widget)
                widget.deleteLater()
            elif child_layout is not None:
                self._clear_layout(child_layout)
    
    def _disconnect_widget_signals(self, widget):
        """Recursively disconnect all signals from a widget and its children"""
        try:
            # Disconnect custom value_changed signal if it exists
            if hasattr(widget, 'value_changed'):
                try:
                    widget.value_changed.disconnect()
                except:
                    pass
            
            # Disconnect common Qt widget signals
            if hasattr(widget, 'valueChanged'):
                try:
                    widget.valueChanged.disconnect()
                except:
                    pass
            
            if hasattr(widget, 'textChanged'):
                try:
                    widget.textChanged.disconnect()
                except:
                    pass
            
            if hasattr(widget, 'clicked'):
                try:
                    widget.clicked.disconnect()
                except:
                    pass
            
            if hasattr(widget, 'editingFinished'):
                try:
                    widget.editingFinished.disconnect()
                except:
                    pass
            
            # Recursively disconnect signals from child widgets
            for child in widget.findChildren(QWidget):
                self._disconnect_widget_signals(child)
        except:
            pass  # Ignore any errors during disconnect
    
    def _track_connection(self, signal, slot):
        """Connect a signal to a slot and track it for later disconnection"""
        signal.connect(slot)
        self._active_connections.append((signal, slot))

    def refresh_properties(self):
        """Refresh all properties from the current node"""
        # DEBUG: Log refresh calls
        if self.main_editor and 'console' in self.main_editor.panels:
            import traceback
            stack = ''.join(traceback.format_stack()[-4:-1])  # Get last 3 stack frames
            self.main_editor.panels['console'].log_message(
                f"[DEBUG] refresh_properties called\nStack:\n{stack}", "Debug")
        
        # Disconnect all tracked connections before clearing
        for connection in self._active_connections:
            try:
                connection[0].disconnect(connection[1])
            except:
                pass
        self._active_connections.clear()

        # Clear material widgets tracking
        self._material_widgets.clear()
        
        # Clear existing widgets
        self.property_widgets.clear()
        self._clear_layout(self.properties_layout)

        if not self.current_node:
            self.node_name_edit.setText("")
            self.node_name_edit.setEnabled(False)
            self.node_name_edit.setPlaceholderText("No Selection")
            return

        # Set node name
        self.node_name_edit.blockSignals(True)
        self.node_name_edit.setText(self.current_node.get_name())
        self.node_name_edit.setEnabled(True)
        self.node_name_edit.setPlaceholderText("")
        self.node_name_edit.blockSignals(False)

        # Add Node Properties section
        self._add_node_properties_section()

        # Add Transform section (for Node2D/Node3D)
        self._add_transform_section()

        # Add components sections
        self._add_components_section()

        # Add stretch at the end
        self.properties_layout.addStretch()

    def _add_node_properties_section(self):
        """Add node properties section"""
        group = QGroupBox("Node")
        group_layout = QVBoxLayout()
        group_layout.setContentsMargins(3, 3, 3, 3)
        group_layout.setSpacing(2)

        # Active checkbox
        active_widget = BoolPropertyWidget("Active")
        active_widget.set_value(self.current_node.is_active())
        self._track_connection(active_widget.value_changed, lambda v: self.current_node.set_active(v))
        group_layout.addWidget(active_widget)

        # Visible checkbox
        visible_widget = BoolPropertyWidget("Visible")
        visible_widget.set_value(self.current_node.is_visible())
        self._track_connection(visible_widget.value_changed, lambda v: self.current_node.set_visible(v))
        group_layout.addWidget(visible_widget)

        # Path (read-only)
        path_layout = QHBoxLayout()
        path_layout.setSpacing(4)
        path_label = QLabel("Path:")
        path_label.setFixedWidth(120)
        path_label.setSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Fixed)
        path_value = QLabel(self.current_node.get_path())
        path_value.setWordWrap(True)
        path_value.setTextInteractionFlags(Qt.TextInteractionFlag.TextSelectableByMouse)
        path_layout.addWidget(path_label)
        path_layout.addWidget(path_value, 1)
        group_layout.addLayout(path_layout)

        group.setLayout(group_layout)
        self.properties_layout.addWidget(group)

    def _add_transform_section(self):
        """Add transform properties section (position/rotation/scale/z-index).

        This queries the node's registered properties via the editor bridge and, if
        transform-like properties are present, shows them in a dedicated group.
        """
        if not self.current_node or not self.editor_bridge:
            return

        try:
            properties_json = self.editor_bridge.get_node_properties(self.current_node)
            # `get_node_properties` returns a JSON string; be defensive if it's already parsed
            if isinstance(properties_json, str):
                data = json.loads(properties_json)
            else:
                data = properties_json or {}

            properties = data.get("properties", {})
            if not isinstance(properties, dict):
                return

            # Extract potential transform properties
            position = properties.get("position")
            rotation = properties.get("rotation")
            scale = properties.get("scale")
            z_index = properties.get("z_index")

            # Only add a section if the node actually has transform-like properties
            if position is None and rotation is None and scale is None and z_index is None:
                return

            group = QGroupBox("Transform")
            group.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Preferred)
            group.setStyleSheet(
                """
                QGroupBox {
                    font-weight: bold;
                    border: 1px solid #888;
                    border-radius: 4px;
                    margin-top: 8px;
                    padding-top: 10px;
                }
                QGroupBox::title {
                    subcontrol-origin: margin;
                    subcontrol-position: top left;
                    padding: 2px 5px;
                    left: 10px;
                }
                """
            )

            layout = QVBoxLayout()
            layout.setContentsMargins(3, 3, 3, 3)
            layout.setSpacing(2)

            # Position: Vec2 (2D) or Vec3 (3D)
            if isinstance(position, dict) and "x" in position and "y" in position:
                if "z" in position:
                    widget = Vec3PropertyWidget("Position")
                    widget.set_value(
                        (
                            float(position.get("x", 0.0)),
                            float(position.get("y", 0.0)),
                            float(position.get("z", 0.0)),
                        )
                    )
                else:
                    widget = Vec2PropertyWidget("Position")
                    widget.set_value(
                        (
                            float(position.get("x", 0.0)),
                            float(position.get("y", 0.0)),
                        )
                    )

                widget.value_changed.connect(
                    lambda v, name="position": self._on_node_property_changed(name, v)
                )
                layout.addWidget(widget)

            # Rotation:
            # - 2D nodes expose a simple float rotation
            # - 3D nodes expose a quaternion rotation as {x, y, z, w}
            if rotation is not None:
                if isinstance(rotation, dict):
                    # Assume quaternion (Node3D/Camera3D) - use Euler widget
                    if all(k in rotation for k in ("x", "y", "z", "w")):
                        # Default quaternion is identity: (0, 0, 0, 1)
                        widget = EulerRotationWidget("Rotation", {"x": 0.0, "y": 0.0, "z": 0.0, "w": 1.0})
                        widget.set_value(rotation)
                        self._track_connection(
                            widget.value_changed,
                            lambda v, name="rotation": self._on_node_property_changed(name, v)
                        )
                        layout.addWidget(widget)
                else:
                    widget = FloatPropertyWidget("Rotation", 0.0)
                    widget.set_value(float(rotation))
                    widget.value_changed.connect(
                        lambda v, name="rotation": self._on_node_property_changed(name, v)
                    )
                    layout.addWidget(widget)

            # Scale: Vec2 (2D) or Vec3 (3D)
            if isinstance(scale, dict) and "x" in scale and "y" in scale:
                if "z" in scale:
                    widget = Vec3PropertyWidget("Scale", default_value=(1.0, 1.0, 1.0))
                    widget.set_value(
                        (
                            float(scale.get("x", 1.0)),
                            float(scale.get("y", 1.0)),
                            float(scale.get("z", 1.0)),
                        )
                    )
                else:
                    widget = Vec2PropertyWidget("Scale", default_value=(1.0, 1.0))
                    widget.set_value(
                        (
                            float(scale.get("x", 1.0)),
                            float(scale.get("y", 1.0)),
                        )
                    )

                self._track_connection(
                    widget.value_changed,
                    lambda v, name="scale": self._on_node_property_changed(name, v)
                )
                layout.addWidget(widget)

            # Z index for 2D nodes
            if z_index is not None:
                widget = IntPropertyWidget("Z Index")
                widget.set_value(int(z_index))
                self._track_connection(
                    widget.value_changed,
                    lambda v, name="z_index": self._on_node_property_changed(name, v)
                )
                layout.addWidget(widget)

            if layout.count() == 0:
                # No usable transform values
                return

            group.setLayout(layout)
            self.properties_layout.addWidget(group)
        except Exception as e:
            error_label = QLabel(f"Error loading transform: {str(e)}")
            error_label.setWordWrap(True)
            error_label.setStyleSheet("color: #ff4444;")
            self.properties_layout.addWidget(error_label)

    def _add_components_section(self):
        """Add components section"""
        # Get all components
        components = self.current_node.get_components()

        for component in components:
            self._add_component_group(component)

        # Add Component button
        add_comp_layout = QHBoxLayout()
        add_comp_btn = QPushButton("+ Add Component")
        add_comp_btn.clicked.connect(self._on_add_component_clicked)
        add_comp_layout.addWidget(add_comp_btn)
        add_comp_layout.addStretch()
        self.properties_layout.addLayout(add_comp_layout)

    def _add_component_group(self, component):
        """Add a component group with its properties"""
        group = QGroupBox(component.get_type_name())
        group.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Preferred)
        group.setStyleSheet("""
            QGroupBox {
                font-weight: bold;
                border: 1px solid #888;
                border-radius: 4px;
                margin-top: 8px;
                padding-top: 10px;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                subcontrol-position: top left;
                padding: 2px 5px;
                left: 10px;
            }
        """)

        group_layout = QVBoxLayout()
        group_layout.setContentsMargins(3, 3, 3, 3)
        group_layout.setSpacing(2)

        # Header row: enabled checkbox + remove button
        header_layout = QHBoxLayout()
        header_layout.setSpacing(2)

        enabled_widget = BoolPropertyWidget("Enabled")
        enabled_widget.set_value(component.is_enabled())
        self._track_connection(enabled_widget.value_changed, lambda v, c=component: c.set_enabled(v))
        header_layout.addWidget(enabled_widget)

        header_layout.addStretch()

        remove_btn = QPushButton("Remove")
        remove_btn.setFixedWidth(70)
        remove_btn.clicked.connect(
            lambda checked=False, c=component: self._on_remove_component_clicked(c)
        )
        header_layout.addWidget(remove_btn)

        group_layout.addLayout(header_layout)

        # Get component properties from editor bridge
        if self.editor_bridge:
            try:
                properties_json = self.editor_bridge.get_component_properties(component)

                # Parse JSON string to dictionary
                try:
                    if isinstance(properties_json, str):
                        data = json.loads(properties_json)
                    else:
                        data = properties_json
                except json.JSONDecodeError as e:
                    if 'console' in self.main_editor.panels:
                        self.main_editor.panels['console'].log_message(
                            f"JSON Parse Error: {str(e)}\nRaw: {properties_json[:200]}", "Error")
                    raise

                # Debug: Log the raw JSON structure
                if 'console' in self.main_editor.panels:
                    self.main_editor.panels['console'].log_message(
                        f"Raw JSON keys: {list(data.keys())}", "Debug")
                    self.main_editor.panels['console'].log_message(
                        f"Raw JSON: {json.dumps(data, indent=2)[:500]}", "Debug")

                # Extract properties and metadata
                properties = data.get("properties", {})
                metadata = data.get("property_metadata", {})

                # Debug: Log the structure
                if 'console' in self.main_editor.panels:
                    self.main_editor.panels['console'].log_message(
                        f"Component {component.get_type_name()} properties: {list(properties.keys())}", "Debug")
                    if metadata:
                        self.main_editor.panels['console'].log_message(
                            f"Metadata keys: {list(metadata.keys())}", "Debug")

                # Filter out 'type' and 'enabled' properties as they're handled separately
                if "type" in properties:
                    del properties["type"]
                if "enabled" in properties:
                    del properties["enabled"]

                # Handle case where properties might not be a dict
                if properties is None or len(properties) == 0:
                    info_label = QLabel("No editable properties")
                    info_label.setStyleSheet("color: #888; font-style: italic;")
                    group_layout.addWidget(info_label)
                elif isinstance(properties, dict):
                    # Special handling for material widgets - store for later
                    material_widget = None

                    print(f"[Inspector] Processing component type: {component.get_type_name()}")
                    print(f"[Inspector] Component: {component}, Properties count: {len(properties)}")

                    if component.get_type_name() == "PrimitiveMesh3D":
                        # Create material override widget but don't add it yet
                        material_widget = MaterialOverridePropertyWidget("Material Override", component, self.editor_bridge)
                        material_widget.set_value(None)  # Will read from component

                        # Filter out material override properties from regular display
                        material_props = {
                            'materialOverrideEnabled', 'albedoColor', 'albedoTexture',
                            'metallic', 'roughness', 'metallicRoughnessTexture',
                            'normalTexture', 'normalScale', 'emissiveColor',
                            'emissiveTexture', 'emissiveStrength'
                        }
                        properties = {k: v for k, v in properties.items() if k not in material_props}

                    elif component.get_type_name() == "StaticMesh3D":
                        # Create material slots widget for StaticMesh3D
                        if self.main_editor and 'console' in self.main_editor.panels:
                            self.main_editor.panels['console'].log_message(
                                "[Inspector] Creating MaterialSlotsPropertyWidget for StaticMesh3D", "Debug")
                        from editor.widgets.material_slots_widget import MaterialSlotsPropertyWidget
                        material_widget = MaterialSlotsPropertyWidget(component, self.editor_bridge, self.main_editor)
                        if self.main_editor and 'console' in self.main_editor.panels:
                            self.main_editor.panels['console'].log_message(
                                f"[Inspector] MaterialSlotsPropertyWidget created: {material_widget}", "Debug")
                        # Store reference for later refresh
                        self._material_widgets[component] = material_widget
                        # No need to filter properties for StaticMesh3D - material slots are separate

                    elif component.get_type_name() == "CollisionBody2DComponent":
                        # Create collision body 2D widget but don't add it yet
                        from editor.widgets.collision_body_2d_widget import CollisionBody2DPropertyWidget
                        collision_body_widget = CollisionBody2DPropertyWidget(component, self.editor_bridge, self.main_editor)

                        # Connect polygon mode signal to viewport
                        if self.main_editor and hasattr(self.main_editor, 'viewport_tabs'):
                            viewport = self.main_editor.viewport_tabs.get_current_viewport()
                            if viewport:
                                collision_body_widget.polygon_mode_changed.connect(viewport.set_polygon_creation_mode)
                                # Connect viewport's vertex added signal to widget's refresh
                                viewport.polygon_vertex_added.connect(collision_body_widget._refresh_vertex_list)

                        # Filter out properties that are handled by the custom widget
                        collision_props = {'shapeType'}
                        properties = {k: v for k, v in properties.items() if k not in collision_props}

                    elif component.get_type_name() == "CollisionMesh3DComponent":
                        # Create collision mesh 3D widget
                        from editor.widgets.collision_mesh_3d_widget import CollisionMesh3DPropertyWidget
                        collision_mesh_widget = CollisionMesh3DPropertyWidget(component, self.editor_bridge, self.main_editor)

                        # Filter out properties that are handled by the custom widget
                        collision_mesh_props = {'shapeType', 'size', 'radius', 'height', 'planeNormal', 'planeDistance', 'meshPath', 'meshSolver'}
                        properties = {k: v for k, v in properties.items() if k not in collision_mesh_props}

                    elif component.get_type_name() == "Button":
                        # Create button state widget
                        button_state_widget = ButtonStatePropertyWidget("Button States", component, self.editor_bridge)
                        button_state_widget.set_value(None)  # Will read from component

                        # Filter out button state properties from regular display
                        button_state_props = {
                            'styleMode', 'normalModulation', 'hoverModulation', 'pressedModulation', 'disabledModulation',
                            'normalSoundPath', 'hoverSoundPath', 'pressedSoundPath', 'disabledSoundPath',
                            'normalTweenEnabled', 'normalTweenScale', 'normalTweenRotation', 'normalTweenPosition', 'normalTweenDuration',
                            'hoverTweenEnabled', 'hoverTweenScale', 'hoverTweenRotation', 'hoverTweenPosition', 'hoverTweenDuration',
                            'pressedTweenEnabled', 'pressedTweenScale', 'pressedTweenRotation', 'pressedTweenPosition', 'pressedTweenDuration',
                            'disabledTweenEnabled', 'disabledTweenScale', 'disabledTweenRotation', 'disabledTweenPosition', 'disabledTweenDuration'
                        }
                        properties = {k: v for k, v in properties.items() if k not in button_state_props}

                    elif component.get_type_name() == "WorldEnvironment":
                        # Create custom WorldEnvironment widget
                        world_env_widget = WorldEnvironmentPropertyWidget("World Environment", component, self.editor_bridge)
                        world_env_widget.set_value(None)  # Will read from component

                        # Filter out all properties since they're handled by the custom widget
                        world_env_props = {
                            'skyboxType', 'skyboxColor', 'skyTopColor', 'skyHorizonColor', 'skyBottomColor',
                            'cubemapPosX', 'cubemapNegX', 'cubemapPosY', 'cubemapNegY', 'cubemapPosZ', 'cubemapNegZ',
                            'panoramicTexture', 'fogEnabled', 'fogColor', 'fogDensity', 'fogStart', 'fogEnd', 'fogMode',
                            'ambientLightEnabled', 'ambientLightColor', 'ambientLightIntensity', 'volumetricFogEnabled'
                        }
                        properties = {k: v for k, v in properties.items() if k not in world_env_props}

                    # Organize properties by group
                    grouped_properties = {}  # {group_name: [(prop_name, prop_value, prop_metadata), ...]}
                    ungrouped_properties = []  # [(prop_name, prop_value, prop_metadata), ...]
                    
                    for prop_name, prop_value in properties.items():
                        # Skip nested properties dict (handle it separately if needed)
                        if prop_name == "properties" and isinstance(prop_value, dict):
                            # Unfold nested properties
                            for nested_name, nested_value in prop_value.items():
                                nested_meta = metadata.get("properties", {}).get(nested_name) if "properties" in metadata else None
                                # Check for group in nested metadata
                                group_name = nested_meta.get("group", "") if nested_meta else ""
                                if group_name:
                                    if group_name not in grouped_properties:
                                        grouped_properties[group_name] = []
                                    grouped_properties[group_name].append((nested_name, nested_value, nested_meta))
                                else:
                                    ungrouped_properties.append((nested_name, nested_value, nested_meta))
                        else:
                            # Get metadata for this property
                            prop_metadata = metadata.get(prop_name)
                            # Check for group in metadata
                            group_name = prop_metadata.get("group", "") if prop_metadata else ""
                            if group_name:
                                if group_name not in grouped_properties:
                                    grouped_properties[group_name] = []
                                grouped_properties[group_name].append((prop_name, prop_value, prop_metadata))
                            else:
                                ungrouped_properties.append((prop_name, prop_value, prop_metadata))
                    
                    # Add ungrouped properties first
                    for prop_name, prop_value, prop_metadata in ungrouped_properties:
                        widget = self._create_property_widget(prop_name, prop_value, prop_metadata)
                        if widget:
                            # If this is a LinkedVec4PropertyWidget, set the linked state
                            if isinstance(widget, LinkedVec4PropertyWidget):
                                linked_prop_name = f"{prop_name}Linked"
                                if linked_prop_name in properties:
                                    widget.set_linked(properties[linked_prop_name])
                            # If this is a LinkedColorPropertyWidget, set the linked state and colors
                            elif isinstance(widget, LinkedColorPropertyWidget):
                                linked_prop_name = "borderColorLinked"
                                if linked_prop_name in properties:
                                    widget.set_linked(properties[linked_prop_name])
                                # Set the 4 colors from individual properties
                                if all(name in properties for name in ['borderColorLeft', 'borderColorRight', 'borderColorTop', 'borderColorBottom']):
                                    colors = [
                                        properties['borderColorLeft'],
                                        properties['borderColorRight'],
                                        properties['borderColorTop'],
                                        properties['borderColorBottom'],
                                    ]
                                    widget.set_value(colors)
                            
                            self._track_connection(
                                widget.value_changed,
                                lambda v, c=component, p=prop_name: self._on_component_property_changed(c, p, v)
                            )
                            group_layout.addWidget(widget)
                    
                    # Add grouped properties with collapsible group boxes
                    for group_name in sorted(grouped_properties.keys()):
                        # Create collapsible group box
                        collapsible_group = CollapsibleGroupBox(group_name)
                        
                        # Add properties in this group
                        for prop_name, prop_value, prop_metadata in grouped_properties[group_name]:
                            widget = self._create_property_widget(prop_name, prop_value, prop_metadata)
                            if widget:
                                # If this is a LinkedVec4PropertyWidget, set the linked state
                                if isinstance(widget, LinkedVec4PropertyWidget):
                                    linked_prop_name = f"{prop_name}Linked"
                                    if linked_prop_name in properties:
                                        widget.set_linked(properties[linked_prop_name])
                                # If this is a LinkedColorPropertyWidget, set the linked state and colors
                                elif isinstance(widget, LinkedColorPropertyWidget):
                                    linked_prop_name = "borderColorLinked"
                                    if linked_prop_name in properties:
                                        widget.set_linked(properties[linked_prop_name])
                                    # Set the 4 colors from individual properties
                                    if all(name in properties for name in ['borderColorLeft', 'borderColorRight', 'borderColorTop', 'borderColorBottom']):
                                        colors = [
                                            properties['borderColorLeft'],
                                            properties['borderColorRight'],
                                            properties['borderColorTop'],
                                            properties['borderColorBottom'],
                                        ]
                                        widget.set_value(colors)
                                
                                self._track_connection(
                                    widget.value_changed,
                                    lambda v, c=component, p=prop_name: self._on_component_property_changed(c, p, v)
                                )
                                collapsible_group.add_widget(widget)
                        
                        group_layout.addWidget(collapsible_group)
                    
                    # Add material override widget at the bottom if it exists
                    if material_widget is not None:
                        print(f"[Inspector] Adding material_widget to layout: {material_widget}")
                        group_layout.addWidget(material_widget)
                    else:
                        print(f"[Inspector] material_widget is None, not adding")

                    # Add collision body 2D widget at the bottom if it exists
                    if 'collision_body_widget' in locals() and collision_body_widget is not None:
                        group_layout.addWidget(collision_body_widget)

                    # Add collision mesh 3D widget at the bottom if it exists
                    if 'collision_mesh_widget' in locals() and collision_mesh_widget is not None:
                        group_layout.addWidget(collision_mesh_widget)

                    # Add button state widget at the bottom if it exists
                    if 'button_state_widget' in locals() and button_state_widget is not None:
                        group_layout.addWidget(button_state_widget)

                    # Add world environment widget at the bottom if it exists
                    if 'world_env_widget' in locals() and world_env_widget is not None:
                        group_layout.addWidget(world_env_widget)
                else:
                    error_label = QLabel(f"Unexpected properties format: {type(properties).__name__}")
                    error_label.setWordWrap(True)
                    error_label.setStyleSheet("color: #ff8800;")
                    group_layout.addWidget(error_label)
            except Exception as e:
                import traceback
                error_msg = f"Error loading properties: {str(e)}\n{traceback.format_exc()}"
                if 'console' in self.main_editor.panels:
                    self.main_editor.panels['console'].log_message(error_msg, "Error")
                error_label = QLabel(f"Error loading properties: {str(e)}")
                error_label.setWordWrap(True)
                error_label.setStyleSheet("color: #ff4444;")
                group_layout.addWidget(error_label)

        group.setLayout(group_layout)
        self.properties_layout.addWidget(group)

    def _title_case_property_name(self, prop_name):
        """Convert property name to title case for display"""
        # Use the global format_property_name function
        return format_property_name(prop_name)

    def _create_property_widget(self, prop_name, prop_value, prop_metadata=None):
        """Create appropriate widget based on property type and metadata"""
        # Use title cased name for display
        display_name = self._title_case_property_name(prop_name)

        # Parse metadata if available
        prop_type = None
        hint_type = None
        hint_string = ""
        default_value = None

        if prop_metadata:
            try:
                prop_type = prop_metadata.get("type")  # PropertyValueType enum value
                # Ensure prop_type is an int if it exists
                if prop_type is not None and not isinstance(prop_type, int):
                    prop_type = int(prop_type)

                hint_info = prop_metadata.get("hint", {})
                if isinstance(hint_info, dict):
                    hint_type = hint_info.get("type")
                    if hint_type is not None and not isinstance(hint_type, int):
                        hint_type = int(hint_type)
                    hint_string = hint_info.get("hint_string", "")

                # Extract default value from metadata
                default_value = prop_metadata.get("defaultValue")
            except (ValueError, TypeError) as e:
                if 'console' in self.main_editor.panels:
                    self.main_editor.panels['console'].log_message(
                        f"Error parsing metadata for property '{prop_name}': {str(e)}", "Warning")

        # PropertyValueType enum mapping (from PropertyDescriptor.hpp)
        # 0: Int, 1: Float, 2: String, 3: Bool, 4: Vec2, 5: Vec3, 6: Vec4, 7: Color, 8: NodePath, 9: ScenePath, 10: Enum

        # PropertyHintType enum mapping
        # 0: None, 1: Range, 2: Enum, 3: File, 4: MultilineText, 5: ExpRange, 6: Length, 7: ColorNoAlpha, 8: Dir, 9: Layers2D, 10: Layers3D

        # Check for linked Vec4 properties (cornerRadius, borderWidth)
        # Skip the *Linked properties themselves
        if not prop_name.endswith('Linked') and prop_name in ['cornerRadius', 'borderWidth']:
            # This is a linked property - use special widget
            # Convert default value if it's a dict
            default_val = (0, 0, 0, 0)  # Default fallback
            if default_value is not None:
                if isinstance(default_value, dict):
                    default_val = (default_value.get('x', 0), default_value.get('y', 0),
                                 default_value.get('z', 0), default_value.get('w', 0))
                elif hasattr(default_value, "__iter__") and len(default_value) >= 4:
                    default_val = default_value
            widget = LinkedVec4PropertyWidget(display_name, f"{prop_name}Linked", default_val)
            if isinstance(prop_value, dict):
                widget.set_value((
                    prop_value.get('x', 0),
                    prop_value.get('y', 0),
                    prop_value.get('z', 0),
                    prop_value.get('w', 0),
                ))
            elif hasattr(prop_value, "__iter__") and len(prop_value) >= 4:
                widget.set_value(prop_value)
            # Will set linked state from separate property later
            return widget
        elif prop_name == 'borderColorLeft':
            # This is the first border color property - create linked color widget for all 4
            # Check if we have all 4 border color properties
            # Note: component parameter needs to be passed from _add_component_group context
            # For now, try to get from prop_metadata or skip the check
            try:
                # We're being called from _add_component_group, so properties dict should have all props
                # Just create the widget - the properties will be set from the properties dict
                default_color = (0, 0, 0, 1) if default_value is None else default_value
                widget = LinkedColorPropertyWidget(
                    'Border Color',
                    color_names=['Left', 'Right', 'Top', 'Bottom'],
                    linked_prop_name='borderColorLinked',
                    default_value=default_color
                )
                # Value will be set by caller after checking all properties exist
                return widget
            except Exception:
                # Fall through to regular color widget if creation fails
                pass
        elif prop_name in ['borderColorRight', 'borderColorTop', 'borderColorBottom', 'borderColorLinked']:
            # Skip these - they're handled by the LinkedColorPropertyWidget for borderColorLeft
            return None
        elif prop_name.endswith('Linked'):
            # Skip rendering the *Linked bool properties - they're handled by LinkedVec4PropertyWidget
            return None

        # Use metadata type hint if available, otherwise infer from value
        if prop_type == 3 or (prop_type is None and isinstance(prop_value, bool)):
            # Bool
            widget = BoolPropertyWidget(display_name, default_value if default_value is not None else False)
            widget.set_value(prop_value)
            return widget
        elif prop_type == 10 or hint_type == 2:  # Enum type or Enum hint
            # Create enum widget with hint string
            enum_values = [v.strip() for v in hint_string.split(',')] if hint_string else []
            widget = EnumPropertyWidget(display_name, enum_values, default_value if default_value is not None else 0)
            widget.set_value(prop_value)
            return widget
        elif prop_type == 0 or (prop_type is None and isinstance(prop_value, int) and not isinstance(prop_value, bool)):
            # Int
            widget = IntPropertyWidget(display_name, default_value if default_value is not None else 0)
            widget.set_value(prop_value)
            return widget
        elif prop_type == 1 or (prop_type is None and isinstance(prop_value, float)):
            # Float
            widget = FloatPropertyWidget(display_name, default_value if default_value is not None else 0.0)
            widget.set_value(prop_value)
            return widget
        elif prop_type == 7 or (prop_type is None and "color" in prop_name.lower()):
            # Color
            # Convert default value if it's a dict
            default_val = None
            if default_value is not None:
                if isinstance(default_value, dict):
                    default_val = (default_value.get('r', 1), default_value.get('g', 1),
                                 default_value.get('b', 1), default_value.get('a', 1))
                elif hasattr(default_value, "__iter__") and len(default_value) >= 3:
                    default_val = default_value
            else:
                default_val = (1, 1, 1, 1)  # Default white
            widget = ColorPropertyWidget(display_name, default_val)
            if isinstance(prop_value, dict):
                # Color from dict format {r, g, b, a}
                widget.set_value((prop_value.get('r', 0), prop_value.get('g', 0),
                                prop_value.get('b', 0), prop_value.get('a', 1)))
            else:
                widget.set_value(prop_value)
            return widget
        elif prop_type == 2 or (prop_type is None and isinstance(prop_value, str)):
            # String - check for file/dir hints or path-related property names
            if hint_type == 3:  # File hint
                # Check if it's an audio file property
                if "audio" in prop_name.lower():
                    widget = AudioFilePropertyWidget(display_name, self.editor_bridge)
                    widget.default_value = default_value if default_value is not None else ""
                    widget.set_value(prop_value)
                    return widget
                else:
                    widget = PathPropertyWidget(display_name, default_value if default_value is not None else "")
                    widget.set_value(prop_value)
                    return widget
            elif hint_type == 8:  # Dir hint
                widget = PathPropertyWidget(display_name, default_value if default_value is not None else "")
                widget.set_value(prop_value)
                return widget
            elif "audio" in prop_name.lower() and ("path" in prop_name.lower() or "file" in prop_name.lower() or "asset" in prop_name.lower()):
                # Audio file property detected by name
                widget = AudioFilePropertyWidget(display_name, self.editor_bridge)
                widget.default_value = default_value if default_value is not None else ""
                widget.set_value(prop_value)
                return widget
            elif "path" in prop_name.lower() or "file" in prop_name.lower():
                # Fallback: detect path properties by name
                widget = PathPropertyWidget(display_name, default_value if default_value is not None else "")
                widget.set_value(prop_value)
                return widget
            else:
                widget = StringPropertyWidget(display_name, default_value if default_value is not None else "")
                widget.set_value(prop_value)
                return widget
        elif prop_type == 4 or (prop_type is None and isinstance(prop_value, dict) and 'x' in prop_value and 'y' in prop_value and 'z' not in prop_value):
            # Vec2
            # Convert default value if it's a dict
            default_val = None
            if default_value is not None:
                if isinstance(default_value, dict):
                    default_val = (default_value.get('x', 0), default_value.get('y', 0))
                elif hasattr(default_value, "__iter__") and len(default_value) >= 2:
                    default_val = default_value
            else:
                default_val = (0, 0)
            widget = Vec2PropertyWidget(display_name, default_val)
            if isinstance(prop_value, dict):
                widget.set_value((prop_value.get('x', 0), prop_value.get('y', 0)))
            else:
                widget.set_value(prop_value)
            return widget
        elif prop_type == 5 or (prop_type is None and isinstance(prop_value, dict) and 'x' in prop_value and 'y' in prop_value and 'z' in prop_value and 'w' not in prop_value):
            # Vec3
            # Convert default value if it's a dict
            default_val = None
            if default_value is not None:
                if isinstance(default_value, dict):
                    default_val = (default_value.get('x', 0), default_value.get('y', 0), default_value.get('z', 0))
                elif hasattr(default_value, "__iter__") and len(default_value) >= 3:
                    default_val = default_value
            else:
                default_val = (0, 0, 0)
            widget = Vec3PropertyWidget(display_name, default_val)
            if isinstance(prop_value, dict):
                widget.set_value((prop_value.get('x', 0), prop_value.get('y', 0), prop_value.get('z', 0)))
            else:
                widget.set_value(prop_value)
            return widget
        elif prop_type == 6 or (
            prop_type is None
            and isinstance(prop_value, dict)
            and 'x' in prop_value
            and 'y' in prop_value
            and 'z' in prop_value
            and 'w' in prop_value
        ):
            # Vec4 (e.g., quaternion) - but check if it's a linked property first
            if not prop_name.endswith('Linked') and prop_name in ['cornerRadius', 'borderWidth']:
                # This is a linked property - use special widget
                # Convert default value if it's a dict
                default_val = (0, 0, 0, 0)  # Default fallback
                if default_value is not None:
                    if isinstance(default_value, dict):
                        default_val = (default_value.get('x', 0), default_value.get('y', 0),
                                     default_value.get('z', 0), default_value.get('w', 0))
                    elif hasattr(default_value, "__iter__") and len(default_value) >= 4:
                        default_val = default_value
                widget = LinkedVec4PropertyWidget(display_name, f"{prop_name}Linked", default_val)
                if isinstance(prop_value, dict):
                    widget.set_value((
                        prop_value.get('x', 0),
                        prop_value.get('y', 0),
                        prop_value.get('z', 0),
                        prop_value.get('w', 0),
                    ))
                elif hasattr(prop_value, "__iter__") and len(prop_value) >= 4:
                    widget.set_value(prop_value)
                return widget

            # Regular Vec4
            # Convert default value if it's a dict
            default_val = None
            if default_value is not None:
                if isinstance(default_value, dict):
                    default_val = (default_value.get('x', 0), default_value.get('y', 0),
                                 default_value.get('z', 0), default_value.get('w', 0))
                elif hasattr(default_value, "__iter__") and len(default_value) >= 4:
                    default_val = default_value
            else:
                default_val = (0, 0, 0, 0)
            widget = Vec4PropertyWidget(display_name, default_val)
            if isinstance(prop_value, dict):
                widget.set_value(
                    (
                        prop_value.get('x', 0),
                        prop_value.get('y', 0),
                        prop_value.get('z', 0),
                        prop_value.get('w', 0),
                    )
                )
            else:
                widget.set_value(prop_value)
            return widget
        elif isinstance(prop_value, (tuple, list)):
            if len(prop_value) == 2:
                widget = Vec2PropertyWidget(display_name, default_value if default_value is not None else (0, 0))
                widget.set_value(prop_value)
                return widget
            elif len(prop_value) == 3:
                widget = Vec3PropertyWidget(display_name, default_value if default_value is not None else (0, 0, 0))
                widget.set_value(prop_value)
                return widget
            elif len(prop_value) == 4:
                # Could be Vec4 or Color - check prop name
                if "color" in prop_name.lower():
                    widget = ColorPropertyWidget(display_name, default_value if default_value is not None else (1, 1, 1, 1))
                    widget.set_value(prop_value)
                    return widget
                else:
                    widget = Vec4PropertyWidget(display_name, default_value if default_value is not None else (0, 0, 0, 0))
                    widget.set_value(prop_value)
                    return widget

        # Default: string widget
        widget = StringPropertyWidget(display_name, default_value if default_value is not None else "")
        widget.set_value(str(prop_value))
        return widget

    def _on_name_changed(self, new_name):
        """Handle node name change with debouncing"""
        if self.current_node and new_name:
            # Restart timer for debouncing
            self.update_timer.stop()
            self.update_timer.start(500)  # 500ms delay

    def _apply_name_change(self):
        """Apply the name change to the node"""
        if self.current_node:
            new_name = self.node_name_edit.text()
            old_name = self.current_node.get_name()
            if new_name != old_name:
                self.current_node.set_name(new_name)

                # Mark scene as dirty
                if self.editor_bridge:
                    self.editor_bridge.mark_scene_dirty()

                # Update scene tree
                if self.main_editor and 'scene_tree' in self.main_editor.panels:
                    self.main_editor.panels['scene_tree'].refresh_tree()

    def _on_node_property_changed(self, prop_name, value):
        """Handle node property change (used for transform properties)."""
        if not self.editor_bridge or not self.current_node:
            return
        
        # DEBUG: Log to see if this is being called multiple times
        if 'console' in self.main_editor.panels:
            self.main_editor.panels['console'].log_message(
                f"[DEBUG] _on_node_property_changed called: {prop_name} = {value}", "Debug")

        try:
            payload = value
            # Convert tuples/lists from vector widgets into dicts expected by C++ side
            if isinstance(value, (tuple, list)):
                if len(value) == 2:
                    payload = {"x": float(value[0]), "y": float(value[1])}
                elif len(value) == 3:
                    payload = {"x": float(value[0]), "y": float(value[1]), "z": float(value[2])}
                elif len(value) == 4:
                    payload = {
                        "x": float(value[0]),
                        "y": float(value[1]),
                        "z": float(value[2]),
                        "w": float(value[3]),
                    }
            elif isinstance(value, dict):
                # Already a dict (from EulerRotationWidget quaternion output)
                payload = value

            value_json = json.dumps(payload)
            self.editor_bridge.set_node_property(self.current_node, prop_name, value_json)
            # Mark scene as dirty so changes are saved and rendered
            self.editor_bridge.mark_scene_dirty()
        except Exception as e:
            QMessageBox.warning(self, "Error", f"Failed to set node property: {str(e)}")

    def _on_component_property_changed(self, component, prop_name, value):
        """Handle component property change"""
        if not self.editor_bridge or not component:
            return

        try:
            # Handle special linked Vec4 property format
            if isinstance(value, dict) and 'vec4' in value and 'linked' in value:
                # This is from a LinkedVec4PropertyWidget
                vec4_value = value['vec4']
                is_linked = value['linked']
                
                # Set the Vec4 property
                value_json = json.dumps({
                    "x": float(vec4_value[0]),
                    "y": float(vec4_value[1]),
                    "z": float(vec4_value[2]),
                    "w": float(vec4_value[3]),
                })
                self.editor_bridge.set_component_property(component, prop_name, value_json)
                
                # Set the linked property
                linked_prop_name = f"{prop_name}Linked"
                linked_json = json.dumps(is_linked)
                self.editor_bridge.set_component_property(component, linked_prop_name, linked_json)
                
                # Mark scene as dirty
                self.editor_bridge.mark_scene_dirty()
                return
            
            # Handle special linked color property format (4 colors with linked state)
            if isinstance(value, dict) and 'colors' in value and 'linked' in value and 'individual_names' in value:
                # This is from a LinkedColorPropertyWidget
                colors = value['colors']
                is_linked = value['linked']
                individual_names = value['individual_names']
                
                # Set each individual color property
                for i, prop_name_individual in enumerate(individual_names):
                    if i < len(colors):
                        color = colors[i]
                        color_json = json.dumps({
                            "r": float(color[0]),
                            "g": float(color[1]),
                            "b": float(color[2]),
                            "a": float(color[3]),
                        })
                        self.editor_bridge.set_component_property(component, prop_name_individual, color_json)
                
                # Set the linked property
                linked_prop_name = f"{prop_name.replace(' ', '')}Linked"  # Remove spaces from display name
                linked_json = json.dumps(is_linked)
                self.editor_bridge.set_component_property(component, linked_prop_name, linked_json)
                
                # Mark scene as dirty
                self.editor_bridge.mark_scene_dirty()
                return
            
            # Convert value to JSON - handle different types
            if isinstance(value, bool):
                # Ensure booleans are properly serialized
                value_json = json.dumps(value)
            elif isinstance(value, (tuple, list)):
                # Convert tuples/lists to dicts for vector types
                if len(value) == 2:
                    value_json = json.dumps({"x": float(value[0]), "y": float(value[1])})
                elif len(value) == 3:
                    value_json = json.dumps({"x": float(value[0]), "y": float(value[1]), "z": float(value[2])})
                elif len(value) == 4:
                    # Check if this is a color property (colors use r,g,b,a instead of x,y,z,w)
                    if "color" in prop_name.lower() or "modulate" in prop_name.lower() or "tint" in prop_name.lower():
                        value_json = json.dumps({
                            "r": float(value[0]),
                            "g": float(value[1]),
                            "b": float(value[2]),
                            "a": float(value[3]),
                        })
                    else:
                        # Treat as Vec4
                        value_json = json.dumps({
                            "x": float(value[0]),
                            "y": float(value[1]),
                            "z": float(value[2]),
                            "w": float(value[3]),
                        })
                else:
                    value_json = json.dumps(value)
            else:
                value_json = json.dumps(value)

            self.editor_bridge.set_component_property(component, prop_name, value_json)
            # Mark scene as dirty
            self.editor_bridge.mark_scene_dirty()

            # If this is a StaticMesh3D and modelPath changed, refresh material slots
            if prop_name == "modelPath" and component in self._material_widgets:
                if self.main_editor and 'console' in self.main_editor.panels:
                    self.main_editor.panels['console'].log_message(
                        "[Inspector] modelPath changed, scheduling deferred refresh for material slots widget", "Debug")
                material_widget = self._material_widgets[component]
                # Reset retry count and trigger deferred refresh with retry logic
                if hasattr(material_widget, '_deferred_refresh'):
                    material_widget._retry_count = 0
                    from PyQt6.QtCore import QTimer
                    QTimer.singleShot(200, material_widget._deferred_refresh)
        except Exception as e:
            QMessageBox.warning(self, "Error", f"Failed to set property: {str(e)}")


    def _on_remove_component_clicked(self, component):
        """Handle remove component button click."""
        if not self.current_node or not self.editor_bridge or not component:
            return

        try:
            if self.editor_bridge.remove_component(self.current_node, component):
                # Mark scene as dirty and refresh inspector
                self.editor_bridge.mark_scene_dirty()
                self.refresh_properties()

                # Emit signal for anyone interested
                self.component_removed.emit(component)

                # Log to console if available
                if self.main_editor and 'console' in self.main_editor.panels:
                    self.main_editor.panels['console'].log_message(
                        f"Removed component '{component.get_type_name()}' from '{self.current_node.get_name()}'",
                        "Info",
                    )
            else:
                QMessageBox.critical(self, "Error", "Failed to remove component from node")
        except Exception as e:
            QMessageBox.critical(self, "Error", f"Error removing component: {str(e)}")

    def _on_add_component_clicked(self):
        """Handle add component button click"""
        if not self.current_node or not self.editor_bridge:
            return

        dialog = AddComponentDialog(self.editor_bridge, self)
        if dialog.exec():
            component_type = dialog.get_selected_type()
            if component_type:
                try:
                    # Create and add component
                    component = self.editor_bridge.create_component(component_type)
                    if component:
                        if self.editor_bridge.add_component(self.current_node, component):
                            # Mark scene as dirty
                            self.editor_bridge.mark_scene_dirty()

                            # Refresh inspector
                            self.refresh_properties()

                            if self.main_editor and 'console' in self.main_editor.panels:
                                self.main_editor.panels['console'].log_message(
                                    f"Added component '{component_type}' to '{self.current_node.get_name()}'", "Info")
                        else:
                            QMessageBox.critical(self, "Error", "Failed to add component to node")
                    else:
                        QMessageBox.critical(self, "Error", f"Failed to create component of type: {component_type}")
                except Exception as e:
                    QMessageBox.critical(self, "Error", f"Error adding component: {str(e)}")
