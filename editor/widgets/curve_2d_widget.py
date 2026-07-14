"""
Custom Inspector Widget for Curve2D and Path2D Components

Provides point editing with Bezier curve support, interactive path creation,
and visual curve preview.
"""

from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel,
                              QPushButton, QListWidget, QListWidgetItem,
                              QDoubleSpinBox, QFrame, QCheckBox,
                              QGroupBox, QSizePolicy)
from PyQt6.QtCore import Qt, pyqtSignal
from PyQt6.QtGui import QPainter, QPen, QPainterPath, QColor
import json
import sys
from pathlib import Path

# Add parent directory to path to import theme
sys.path.insert(0, str(Path(__file__).parent.parent))
from theme import get_theme_manager


class CurveBezierPreviewWidget(QWidget):
    """Widget that shows a preview of the Bezier curve"""
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self.points = []
        self.setMinimumHeight(100)
        self.setMaximumHeight(150)
        self.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)
        
    def set_points(self, points):
        """Set points data for preview"""
        self.points = points
        self.update()
        
    def paintEvent(self, event):
        if not self.points or len(self.points) < 2:
            return
            
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        
        theme = get_theme_manager().get_current_theme()
        bg_color = QColor(theme.colors.surface) if theme else QColor("#1e1a28")
        line_color = QColor(theme.colors.accent_color) if theme else QColor("#5a4a7a")
        handle_color = QColor("#ff8844")
        point_color = QColor("#44ff88")
        
        # Fill background
        painter.fillRect(self.rect(), bg_color)
        
        # Calculate bounds and scale
        min_x = min(p['x'] for p in self.points)
        max_x = max(p['x'] for p in self.points)
        min_y = min(p['y'] for p in self.points)
        max_y = max(p['y'] for p in self.points)
        
        # Add padding for control points
        for p in self.points:
            if p.get('useBezier', False):
                min_x = min(min_x, p['x'] + p.get('ctrlInX', 0), p['x'] + p.get('ctrlOutX', 0))
                max_x = max(max_x, p['x'] + p.get('ctrlInX', 0), p['x'] + p.get('ctrlOutX', 0))
                min_y = min(min_y, p['y'] + p.get('ctrlInY', 0), p['y'] + p.get('ctrlOutY', 0))
                max_y = max(max_y, p['y'] + p.get('ctrlInY', 0), p['y'] + p.get('ctrlOutY', 0))
        
        range_x = max(max_x - min_x, 1)
        range_y = max(max_y - min_y, 1)
        
        padding = 15
        w = self.width() - 2 * padding
        h = self.height() - 2 * padding
        scale = min(w / range_x, h / range_y) * 0.9
        
        def to_screen(x, y):
            sx = padding + (x - min_x) * scale + (w - range_x * scale) / 2
            # Flip Y axis so positive Y is up (like in viewport)
            sy = self.height() - (padding + (y - min_y) * scale + (h - range_y * scale) / 2)
            return sx, sy
        
        # Draw curve
        path = QPainterPath()
        first_pt = self.points[0]
        sx, sy = to_screen(first_pt['x'], first_pt['y'])
        path.moveTo(sx, sy)
        
        for i in range(len(self.points) - 1):
            p0 = self.points[i]
            p1 = self.points[i + 1]
            
            if p0.get('useBezier', False) or p1.get('useBezier', False):
                # Bezier curve
                x0, y0 = to_screen(p0['x'], p0['y'])
                x1, y1 = to_screen(p0['x'] + p0.get('ctrlOutX', 0), p0['y'] + p0.get('ctrlOutY', 0))
                x2, y2 = to_screen(p1['x'] + p1.get('ctrlInX', 0), p1['y'] + p1.get('ctrlInY', 0))
                x3, y3 = to_screen(p1['x'], p1['y'])
                path.cubicTo(x1, y1, x2, y2, x3, y3)
            else:
                # Straight line
                sx, sy = to_screen(p1['x'], p1['y'])
                path.lineTo(sx, sy)
        
        painter.setPen(QPen(line_color, 2))
        painter.drawPath(path)
        
        # Draw control handles for Bezier points
        painter.setPen(QPen(handle_color, 1, Qt.PenStyle.DashLine))
        for p in self.points:
            if p.get('useBezier', False):
                px, py = to_screen(p['x'], p['y'])
                cin_x, cin_y = to_screen(p['x'] + p.get('ctrlInX', 0), p['y'] + p.get('ctrlInY', 0))
                cout_x, cout_y = to_screen(p['x'] + p.get('ctrlOutX', 0), p['y'] + p.get('ctrlOutY', 0))
                painter.drawLine(int(px), int(py), int(cin_x), int(cin_y))
                painter.drawLine(int(px), int(py), int(cout_x), int(cout_y))
                # Control handle circles
                painter.setBrush(handle_color)
                painter.drawEllipse(int(cin_x) - 3, int(cin_y) - 3, 6, 6)
                painter.drawEllipse(int(cout_x) - 3, int(cout_y) - 3, 6, 6)
        
        # Draw points
        painter.setPen(QPen(point_color, 1))
        painter.setBrush(point_color)
        for p in self.points:
            px, py = to_screen(p['x'], p['y'])
            painter.drawEllipse(int(px) - 4, int(py) - 4, 8, 8)


class Curve2DPropertyWidget(QWidget):
    """Custom widget for Curve2D component with Bezier curve editing support"""

    value_changed = pyqtSignal(str, object)  # property_name, value
    curve_mode_changed = pyqtSignal(bool)  # is_creating_curve
    bezier_edit_changed = pyqtSignal(object)  # component or None when bezier editing state changes

    def __init__(self, component, editor_bridge, main_editor=None, parent=None):
        super().__init__(parent)
        self.component = component
        self.editor_bridge = editor_bridge
        self.main_editor = main_editor
        self.is_creating_curve = False
        self.selected_point_index = -1
        self.component_type = component.get_type_name() if component else "Curve2D"
        
        self._init_ui()
        self._load_values()

    def _init_ui(self):
        """Initialize the UI"""
        theme = get_theme_manager().get_current_theme()
        bg_color = theme.colors.surface if theme else "#1e1a28"
        text_color = theme.colors.text_primary if theme else "#ffffff"
        border_color = theme.colors.border if theme else "#3d3650"
        button_color = theme.colors.accent_color if theme else "#5a4a7a"
        button_hover = theme.colors.accent_hover if theme else "#6a5a8a"
        text_secondary = theme.colors.text_secondary if theme else "#aaaaaa"
        button_secondary = theme.colors.surface_hover if theme else "#3a3a3a"

        layout = QVBoxLayout()
        layout.setContentsMargins(5, 5, 5, 5)
        layout.setSpacing(8)

        # Preview widget
        preview_label = QLabel("Curve Preview:")
        preview_label.setStyleSheet(f"color: {text_color}; font-weight: bold;")
        layout.addWidget(preview_label)

        self.preview_widget = CurveBezierPreviewWidget()
        self.preview_widget.setStyleSheet(f"border: 1px solid {border_color};")
        layout.addWidget(self.preview_widget)

        # Curve Creation Button
        btn_text = f"Create {self.component_type} (Click in Viewport)"
        self.create_curve_button = QPushButton(btn_text)
        self.create_curve_button.setStyleSheet(f"""
            QPushButton {{
                background-color: {button_color};
                color: {text_color};
                border: none;
                padding: 6px;
                border-radius: 3px;
            }}
            QPushButton:hover {{
                background-color: {button_hover};
            }}
        """)
        self.create_curve_button.clicked.connect(self._toggle_curve_creation)
        layout.addWidget(self.create_curve_button)

        # Points list
        points_frame = QFrame()
        points_frame.setStyleSheet(f"background-color: {bg_color}; border: 1px solid {border_color};")
        points_layout = QVBoxLayout(points_frame)
        points_layout.setContentsMargins(5, 5, 5, 5)

        points_header = QLabel("Points:")
        points_header.setStyleSheet(f"color: {text_color}; font-weight: bold; border: none;")
        points_layout.addWidget(points_header)

        self.points_list = QListWidget()
        self.points_list.setStyleSheet(f"""
            QListWidget {{
                background-color: {bg_color};
                color: {text_color};
                border: 1px solid {border_color};
            }}
            QListWidget::item:selected {{
                background-color: {button_color};
            }}
        """)
        self.points_list.setMaximumHeight(150)
        self.points_list.currentRowChanged.connect(self._on_point_selected)
        points_layout.addWidget(self.points_list)

        # Point control buttons
        point_btn_layout = QHBoxLayout()
        self.clear_points_btn = QPushButton("Clear All")
        self.clear_points_btn.clicked.connect(self._clear_points)
        self.remove_point_btn = QPushButton("Remove Selected")
        self.remove_point_btn.clicked.connect(self._remove_selected_point)

        for btn in [self.clear_points_btn, self.remove_point_btn]:
            btn.setStyleSheet(f"""
                QPushButton {{
                    background-color: {button_secondary};
                    color: {text_color};
                    border: none;
                    padding: 4px;
                    border-radius: 3px;
                }}
                QPushButton:hover {{
                    background-color: {button_hover};
                }}
            """)

        point_btn_layout.addWidget(self.clear_points_btn)
        point_btn_layout.addWidget(self.remove_point_btn)
        points_layout.addLayout(point_btn_layout)

        layout.addWidget(points_frame)

        # Bezier controls for selected point
        self.bezier_frame = QGroupBox("Bezier Controls")
        self.bezier_frame.setStyleSheet(f"""
            QGroupBox {{
                color: {text_color};
                border: 1px solid {border_color};
                border-radius: 3px;
                margin-top: 6px;
                padding-top: 10px;
            }}
            QGroupBox::title {{
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 3px 0 3px;
            }}
        """)
        bezier_layout = QVBoxLayout(self.bezier_frame)

        # Use Bezier checkbox
        self.use_bezier_check = QCheckBox("Use Bezier Curve")
        self.use_bezier_check.setStyleSheet(f"color: {text_color};")
        self.use_bezier_check.stateChanged.connect(self._on_use_bezier_changed)
        bezier_layout.addWidget(self.use_bezier_check)

        # Symmetric handles checkbox
        self.symmetric_check = QCheckBox("Symmetric Handles")
        self.symmetric_check.setStyleSheet(f"color: {text_color};")
        self.symmetric_check.setChecked(True)
        self.symmetric_check.stateChanged.connect(self._on_symmetric_changed)
        bezier_layout.addWidget(self.symmetric_check)

        # Control In
        ctrl_in_layout = QHBoxLayout()
        ctrl_in_label = QLabel("Control In:")
        ctrl_in_label.setStyleSheet(f"color: {text_secondary};")
        ctrl_in_label.setFixedWidth(70)
        ctrl_in_layout.addWidget(ctrl_in_label)

        self.ctrl_in_x = QDoubleSpinBox()
        self.ctrl_in_x.setRange(-1000, 1000)
        self.ctrl_in_x.setDecimals(1)
        self.ctrl_in_x.valueChanged.connect(self._on_control_changed)
        ctrl_in_layout.addWidget(self.ctrl_in_x)

        self.ctrl_in_y = QDoubleSpinBox()
        self.ctrl_in_y.setRange(-1000, 1000)
        self.ctrl_in_y.setDecimals(1)
        self.ctrl_in_y.valueChanged.connect(self._on_control_changed)
        ctrl_in_layout.addWidget(self.ctrl_in_y)
        bezier_layout.addLayout(ctrl_in_layout)

        # Control Out
        ctrl_out_layout = QHBoxLayout()
        ctrl_out_label = QLabel("Control Out:")
        ctrl_out_label.setStyleSheet(f"color: {text_secondary};")
        ctrl_out_label.setFixedWidth(70)
        ctrl_out_layout.addWidget(ctrl_out_label)

        self.ctrl_out_x = QDoubleSpinBox()
        self.ctrl_out_x.setRange(-1000, 1000)
        self.ctrl_out_x.setDecimals(1)
        self.ctrl_out_x.valueChanged.connect(self._on_control_changed)
        ctrl_out_layout.addWidget(self.ctrl_out_x)

        self.ctrl_out_y = QDoubleSpinBox()
        self.ctrl_out_y.setRange(-1000, 1000)
        self.ctrl_out_y.setDecimals(1)
        self.ctrl_out_y.valueChanged.connect(self._on_control_changed)
        ctrl_out_layout.addWidget(self.ctrl_out_y)
        bezier_layout.addLayout(ctrl_out_layout)

        self.bezier_frame.setVisible(False)
        layout.addWidget(self.bezier_frame)

        # Help text
        self.help_label = QLabel()
        self.help_label.setWordWrap(True)
        self.help_label.setStyleSheet(f"color: {text_secondary}; font-size: 10px; padding: 5px;")
        self.help_label.setVisible(False)
        layout.addWidget(self.help_label)

        layout.addStretch()
        self.setLayout(layout)

    def _load_values(self):
        """Load current values from component"""
        self._refresh_points_list()

    def _toggle_curve_creation(self):
        """Toggle curve creation mode"""
        self.is_creating_curve = not self.is_creating_curve

        if self.is_creating_curve:
            self.create_curve_button.setText(f"Finish Creating {self.component_type}")
            self.help_label.setText(
                f"{self.component_type} Creation Mode:\n"
                "• Left-click in viewport to add points\n"
                "• Right-click or Ctrl+click on a point to remove it\n"
                f"• Click 'Finish Creating {self.component_type}' when done"
            )
            self.help_label.setVisible(True)
            self._set_show_handles(True)
        else:
            self.create_curve_button.setText(f"Create {self.component_type} (Click in Viewport)")
            self.help_label.setVisible(False)
            self._set_show_handles(False)

        self.curve_mode_changed.emit(self.is_creating_curve)

    def _set_show_handles(self, show):
        """Enable or disable Bezier handle display in viewport"""
        if self.editor_bridge and self.component:
            try:
                self.editor_bridge.set_curve2d_show_handles(self.component, show)
            except Exception as e:
                print(f"Error setting show handles: {e}")

    def _set_selected_point(self, index):
        """Set the selected point index for handle display"""
        if self.editor_bridge and self.component:
            try:
                self.editor_bridge.set_curve2d_selected_point(self.component, index)
            except Exception as e:
                print(f"Error setting selected point: {e}")

    def _refresh_points_list(self):
        """Refresh the points list from component, preserving selection"""
        saved_selection = self.selected_point_index

        self.points_list.blockSignals(True)
        self.points_list.clear()

        if not self.component or not self.editor_bridge:
            self.points_list.blockSignals(False)
            return

        try:
            points_json = self.editor_bridge.get_curve2d_points(self.component)
            points = json.loads(points_json)

            for i, pt in enumerate(points):
                bezier_marker = " [B]" if pt.get('useBezier', False) else ""
                item = QListWidgetItem(f"{i}: ({pt['x']:.1f}, {pt['y']:.1f}){bezier_marker}")
                self.points_list.addItem(item)

            self.preview_widget.set_points(points)

            if saved_selection >= 0 and saved_selection < len(points):
                self.points_list.setCurrentRow(saved_selection)
                self.selected_point_index = saved_selection

        except Exception as e:
            print(f"Error refreshing {self.component_type} points list: {e}")
        finally:
            self.points_list.blockSignals(False)

    def _on_point_selected(self, row):
        """Handle point selection in list"""
        self.selected_point_index = row

        if row < 0:
            self.bezier_frame.setVisible(False)
            if not self.is_creating_curve:
                self._set_show_handles(False)
                self._set_selected_point(-1)
                self.bezier_edit_changed.emit(None)
            return

        self.bezier_frame.setVisible(True)
        self._set_show_handles(True)
        self._set_selected_point(row)
        self.bezier_edit_changed.emit(self.component)

        try:
            points_json = self.editor_bridge.get_curve2d_points(self.component)
            points = json.loads(points_json)

            if row < len(points):
                pt = points[row]

                self.use_bezier_check.blockSignals(True)
                self.symmetric_check.blockSignals(True)
                self.ctrl_in_x.blockSignals(True)
                self.ctrl_in_y.blockSignals(True)
                self.ctrl_out_x.blockSignals(True)
                self.ctrl_out_y.blockSignals(True)

                self.use_bezier_check.setChecked(pt.get('useBezier', False))
                self.symmetric_check.setChecked(pt.get('symmetricHandles', True))
                self.ctrl_in_x.setValue(pt.get('ctrlInX', 0))
                self.ctrl_in_y.setValue(pt.get('ctrlInY', 0))
                self.ctrl_out_x.setValue(pt.get('ctrlOutX', 0))
                self.ctrl_out_y.setValue(pt.get('ctrlOutY', 0))

                self.use_bezier_check.blockSignals(False)
                self.symmetric_check.blockSignals(False)
                self.ctrl_in_x.blockSignals(False)
                self.ctrl_in_y.blockSignals(False)
                self.ctrl_out_x.blockSignals(False)
                self.ctrl_out_y.blockSignals(False)
        except Exception as e:
            print(f"Error loading point data: {e}")

    def _on_use_bezier_changed(self, state):
        """Handle use bezier checkbox change"""
        if self.selected_point_index < 0 or not self.editor_bridge:
            return

        try:
            self.editor_bridge.set_curve2d_point_use_bezier(
                self.component,
                self.selected_point_index,
                state == Qt.CheckState.Checked.value
            )
            self._refresh_points_list()
        except Exception as e:
            print(f"Error setting bezier: {e}")

    def _on_symmetric_changed(self, state):
        """Handle symmetric checkbox change"""
        if self.selected_point_index < 0 or not self.editor_bridge:
            return
        self._on_control_changed()

    def _on_control_changed(self):
        """Handle control point value changes"""
        if self.selected_point_index < 0 or not self.editor_bridge:
            return

        try:
            ctrl_in_x = self.ctrl_in_x.value()
            ctrl_in_y = self.ctrl_in_y.value()
            ctrl_out_x = self.ctrl_out_x.value()
            ctrl_out_y = self.ctrl_out_y.value()

            if self.symmetric_check.isChecked():
                ctrl_out_x = -ctrl_in_x
                ctrl_out_y = -ctrl_in_y

                self.ctrl_out_x.blockSignals(True)
                self.ctrl_out_y.blockSignals(True)
                self.ctrl_out_x.setValue(ctrl_out_x)
                self.ctrl_out_y.setValue(ctrl_out_y)
                self.ctrl_out_x.blockSignals(False)
                self.ctrl_out_y.blockSignals(False)

            self.editor_bridge.update_curve2d_point_bezier(
                self.component,
                self.selected_point_index,
                ctrl_in_x, ctrl_in_y,
                ctrl_out_x, ctrl_out_y,
                self.symmetric_check.isChecked()
            )
            self._refresh_points_list()
        except Exception as e:
            print(f"Error updating control points: {e}")

    def add_point(self, x, y):
        """Add a point to the curve"""
        if not self.component or not self.editor_bridge:
            return

        try:
            self.editor_bridge.add_curve2d_point(self.component, x, y)
            self._refresh_points_list()
        except Exception as e:
            print(f"Error adding point: {e}")

    def _remove_selected_point(self):
        """Remove the selected point"""
        current_row = self.points_list.currentRow()
        if current_row >= 0:
            try:
                self.editor_bridge.remove_curve2d_point(self.component, current_row)
                self._refresh_points_list()
            except Exception as e:
                print(f"Error removing point: {e}")

    def _clear_points(self):
        """Clear all points"""
        if not self.component or not self.editor_bridge:
            return

        try:
            self.editor_bridge.clear_curve2d_points(self.component)
            self._refresh_points_list()
        except Exception as e:
            print(f"Error clearing points: {e}")

    def is_curve_creation_active(self):
        """Check if curve creation mode is active"""
        return self.is_creating_curve

    def set_value(self, value):
        """Set widget value (for compatibility with PropertyWidget interface)"""
        pass

    def get_value(self):
        """Get widget value (for compatibility with PropertyWidget interface)"""
        return None


# Alias for Path2D - same widget works for both
Path2DPropertyWidget = Curve2DPropertyWidget

