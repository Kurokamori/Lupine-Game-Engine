"""
Custom Inspector Widget for CollisionBody2D Component

Provides shape type selection, point editing, and interactive polygon creation.
"""

from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel,
                              QPushButton, QComboBox, QListWidget, QListWidgetItem,
                              QDoubleSpinBox, QFrame, QScrollArea)
from PyQt6.QtCore import Qt, pyqtSignal
from PyQt6.QtGui import QFont
import json
import sys
from pathlib import Path

# Add parent directory to path to import theme
sys.path.insert(0, str(Path(__file__).parent.parent))
from theme import get_theme_manager


class CollisionBody2DPropertyWidget(QWidget):
    """Custom widget for CollisionBody2D component with polygon editing support"""

    value_changed = pyqtSignal(str, object)  # property_name, value
    polygon_mode_changed = pyqtSignal(bool)  # is_creating_polygon

    def __init__(self, component, editor_bridge, main_editor=None, parent=None):
        super().__init__(parent)
        self.component = component
        self.editor_bridge = editor_bridge
        self.main_editor = main_editor
        self.is_creating_polygon = False
        
        self._init_ui()
        self._load_values()

    def _init_ui(self):
        """Initialize the UI"""
        # Get theme colors
        theme = get_theme_manager().get_current_theme()
        bg_color = theme.colors.surface if theme else "#1e1a28"
        text_color = theme.colors.text_primary if theme else "#ffffff"
        border_color = theme.colors.border if theme else "#3d3650"
        button_color = theme.colors.accent_color if theme else "#5a4a7a"
        button_hover = theme.colors.accent_hover if theme else "#6a5a8a"

        layout = QVBoxLayout()
        layout.setContentsMargins(5, 5, 5, 5)
        layout.setSpacing(8)

        # Shape Type Selection
        shape_layout = QHBoxLayout()
        shape_label = QLabel("Shape Type:")
        shape_label.setStyleSheet(f"color: {text_color};")
        shape_layout.addWidget(shape_label)

        self.shape_combo = QComboBox()
        self.shape_combo.addItems(["Rectangle", "Circle", "Triangle", "Pentagon", "Hexagon", "Polygon"])
        self.shape_combo.setStyleSheet(f"""
            QComboBox {{
                background-color: {bg_color};
                color: {text_color};
                border: 1px solid {border_color};
                padding: 4px;
            }}
        """)
        self.shape_combo.currentIndexChanged.connect(self._on_shape_type_changed)
        shape_layout.addWidget(self.shape_combo)
        layout.addLayout(shape_layout)

        # Polygon Creation Button (only visible for Polygon shape)
        self.polygon_button = QPushButton("Create Polygon")
        self.polygon_button.setStyleSheet(f"""
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
        self.polygon_button.clicked.connect(self._toggle_polygon_creation)
        self.polygon_button.setVisible(False)
        layout.addWidget(self.polygon_button)

        # Vertex List (only visible for Polygon shape)
        self.vertex_frame = QFrame()
        self.vertex_frame.setStyleSheet(f"background-color: {bg_color}; border: 1px solid {border_color};")
        vertex_layout = QVBoxLayout(self.vertex_frame)
        vertex_layout.setContentsMargins(5, 5, 5, 5)

        vertex_header = QLabel("Vertices:")
        vertex_header.setStyleSheet(f"color: {text_color}; font-weight: bold;")
        vertex_layout.addWidget(vertex_header)

        self.vertex_list = QListWidget()
        self.vertex_list.setStyleSheet(f"""
            QListWidget {{
                background-color: {bg_color};
                color: {text_color};
                border: 1px solid {border_color};
            }}
        """)
        self.vertex_list.setMaximumHeight(150)
        vertex_layout.addWidget(self.vertex_list)

        # Vertex control buttons
        vertex_btn_layout = QHBoxLayout()
        self.clear_vertices_btn = QPushButton("Clear All")
        self.clear_vertices_btn.clicked.connect(self._clear_vertices)
        self.remove_vertex_btn = QPushButton("Remove Selected")
        self.remove_vertex_btn.clicked.connect(self._remove_selected_vertex)

        button_secondary = theme.colors.surface_hover if theme else "#3a3a3a"
        for btn in [self.clear_vertices_btn, self.remove_vertex_btn]:
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

        vertex_btn_layout.addWidget(self.clear_vertices_btn)
        vertex_btn_layout.addWidget(self.remove_vertex_btn)
        vertex_layout.addLayout(vertex_btn_layout)

        self.vertex_frame.setVisible(False)
        layout.addWidget(self.vertex_frame)

        # Help text for polygon mode
        text_secondary = theme.colors.text_secondary if theme else "#aaaaaa"
        self.help_label = QLabel()
        self.help_label.setWordWrap(True)
        self.help_label.setStyleSheet(f"color: {text_secondary}; font-size: 10px; padding: 5px;")
        self.help_label.setVisible(False)
        layout.addWidget(self.help_label)

        layout.addStretch()
        self.setLayout(layout)

    def _load_values(self):
        """Load current values from component"""
        if not self.component or not self.editor_bridge:
            return

        try:
            # Get shape type
            import json
            shape_type_json = self.editor_bridge.get_component_property(self.component, "shapeType")
            shape_type = json.loads(shape_type_json)
            if shape_type is not None:
                self.shape_combo.blockSignals(True)
                self.shape_combo.setCurrentIndex(shape_type)
                self.shape_combo.blockSignals(False)
                self._update_ui_for_shape(shape_type)

            # Load vertices if polygon shape
            if shape_type == 5:  # Polygon
                self._refresh_vertex_list()
        except Exception as e:
            print(f"Error loading CollisionBody2D values: {e}")

    def _on_shape_type_changed(self, index):
        """Handle shape type change"""
        self._update_ui_for_shape(index)

        # Set property on component
        if self.component and self.editor_bridge:
            try:
                self.editor_bridge.set_component_property(
                    self.component,
                    "shapeType",
                    json.dumps(index)
                )
                self.value_changed.emit("shapeType", index)
            except Exception as e:
                print(f"Error setting shape type: {e}")

    def _update_ui_for_shape(self, shape_index):
        """Update UI visibility based on selected shape"""
        is_polygon = (shape_index == 5)  # Polygon

        print(f"[CollisionBody2D] Updating UI for shape index: {shape_index}, is_polygon: {is_polygon}")

        self.polygon_button.setVisible(is_polygon)
        self.vertex_frame.setVisible(is_polygon)

        if is_polygon:
            self._refresh_vertex_list()

    def _toggle_polygon_creation(self):
        """Toggle polygon creation mode"""
        self.is_creating_polygon = not self.is_creating_polygon

        if self.is_creating_polygon:
            self.polygon_button.setText("Finish Creating Polygon")
            self.help_label.setText(
                "Polygon Creation Mode:\n"
                "• Left-click in viewport to add points\n"
                "• Right-click or Ctrl+click on a point to remove it\n"
                "• Click 'Finish Creating Polygon' when done"
            )
            self.help_label.setVisible(True)
        else:
            self.polygon_button.setText("Create Polygon")
            self.help_label.setVisible(False)

        # Emit signal to notify viewport
        self.polygon_mode_changed.emit(self.is_creating_polygon)

    def _refresh_vertex_list(self):
        """Refresh the vertex list from component"""
        self.vertex_list.clear()

        if not self.component or not self.editor_bridge:
            return

        try:
            # Get vertices from component via EditorBridge
            vertices_json = self.editor_bridge.get_collision_vertices(self.component)
            vertices = json.loads(vertices_json)

            for vertex in vertices:
                item = QListWidgetItem(f"({vertex['x']:.2f}, {vertex['y']:.2f})")
                self.vertex_list.addItem(item)
        except Exception as e:
            print(f"Error refreshing vertex list: {e}")

    def add_vertex(self, x, y):
        """Add a vertex to the polygon"""
        if not self.component or not self.editor_bridge:
            return

        try:
            # Add vertex via EditorBridge
            self.editor_bridge.add_collision_vertex(self.component, x, y)
            self._refresh_vertex_list()
        except Exception as e:
            print(f"Error adding vertex: {e}")

    def _remove_selected_vertex(self):
        """Remove the selected vertex"""
        current_row = self.vertex_list.currentRow()
        if current_row >= 0:
            try:
                # Remove vertex from component via EditorBridge
                self.editor_bridge.remove_collision_vertex(self.component, current_row)
                self._refresh_vertex_list()
            except Exception as e:
                print(f"Error removing vertex: {e}")

    def _clear_vertices(self):
        """Clear all vertices"""
        if not self.component or not self.editor_bridge:
            return

        try:
            # Clear vertices from component via EditorBridge
            self.editor_bridge.clear_collision_vertices(self.component)
            self._refresh_vertex_list()
        except Exception as e:
            print(f"Error clearing vertices: {e}")

    def is_polygon_creation_active(self):
        """Check if polygon creation mode is active"""
        return self.is_creating_polygon

    def set_value(self, value):
        """Set widget value (for compatibility with PropertyWidget interface)"""
        pass

    def get_value(self):
        """Get widget value (for compatibility with PropertyWidget interface)"""
        return None


