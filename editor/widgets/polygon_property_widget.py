"""
Reusable Inspector Widget for editing a flat FloatArray polygon property.

Provides the same interactive click-to-draw polygon workflow used by the
CollisionBody2D widget, but generalized to drive any component property that
stores a flat [x0, y0, x1, y1, ...] vertex list (e.g. NavigationRegion2D's
'outline' or NavigationObstacle2D's 'vertices'). Vertices are read and written
through the generic component-property API, so adding/removing a point fires the
component's OnPropertyChanged and is undoable.
"""

from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel,
                              QPushButton, QListWidget, QListWidgetItem, QFrame)
from PyQt6.QtCore import pyqtSignal
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent))
from theme import get_theme_manager


class PolygonPropertyWidget(QWidget):
    """Custom widget for editing a flat FloatArray polygon property."""

    value_changed = pyqtSignal(str, object)   # property_name, value
    polygon_mode_changed = pyqtSignal(bool)    # is_creating_polygon

    def __init__(self, component, editor_bridge, main_editor=None,
                 property_name="outline", title="Polygon", parent=None):
        super().__init__(parent)
        self.component = component
        self.editor_bridge = editor_bridge
        self.main_editor = main_editor
        self.property_name = property_name
        self.title = title
        self.is_creating_polygon = False

        self._init_ui()
        self._refresh_vertex_list()

    def _init_ui(self):
        theme = get_theme_manager().get_current_theme()
        bg_color = theme.colors.surface if theme else "#1e1a28"
        text_color = theme.colors.text_primary if theme else "#ffffff"
        border_color = theme.colors.border if theme else "#3d3650"
        button_color = theme.colors.accent_color if theme else "#5a4a7a"
        button_hover = theme.colors.accent_hover if theme else "#6a5a8a"
        button_secondary = theme.colors.surface_hover if theme else "#3a3a3a"
        text_secondary = theme.colors.text_secondary if theme else "#aaaaaa"

        layout = QVBoxLayout()
        layout.setContentsMargins(5, 5, 5, 5)
        layout.setSpacing(8)

        # Draw toggle button.
        self.polygon_button = QPushButton(f"Draw {self.title}")
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
        layout.addWidget(self.polygon_button)

        # Vertex list.
        self.vertex_frame = QFrame()
        self.vertex_frame.setStyleSheet(f"background-color: {bg_color}; border: 1px solid {border_color};")
        vertex_layout = QVBoxLayout(self.vertex_frame)
        vertex_layout.setContentsMargins(5, 5, 5, 5)

        vertex_header = QLabel(f"{self.title} Vertices:")
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

        vertex_btn_layout = QHBoxLayout()
        self.clear_vertices_btn = QPushButton("Clear All")
        self.clear_vertices_btn.clicked.connect(self._clear_vertices)
        self.remove_vertex_btn = QPushButton("Remove Selected")
        self.remove_vertex_btn.clicked.connect(self._remove_selected_vertex)

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
        layout.addWidget(self.vertex_frame)

        self.help_label = QLabel()
        self.help_label.setWordWrap(True)
        self.help_label.setStyleSheet(f"color: {text_secondary}; font-size: 10px; padding: 5px;")
        self.help_label.setVisible(False)
        layout.addWidget(self.help_label)

        layout.addStretch()
        self.setLayout(layout)

    def _get_flat_vertices(self):
        """Return the property's flat [x0, y0, ...] list (or [])."""
        if not self.component or not self.editor_bridge:
            return []
        try:
            raw = self.editor_bridge.get_component_property(self.component, self.property_name)
            verts = json.loads(raw) if raw else []
            return verts if isinstance(verts, list) else []
        except Exception as e:
            print(f"Error reading {self.property_name}: {e}")
            return []

    def _set_flat_vertices(self, flat):
        """Write the flat vertex list back to the component property."""
        if not self.component or not self.editor_bridge:
            return
        try:
            self.editor_bridge.set_component_property(self.component, self.property_name, json.dumps(flat))
            self.value_changed.emit(self.property_name, flat)
        except Exception as e:
            print(f"Error writing {self.property_name}: {e}")

    def _toggle_polygon_creation(self):
        self.is_creating_polygon = not self.is_creating_polygon
        if self.is_creating_polygon:
            self.polygon_button.setText(f"Finish Drawing {self.title}")
            self.help_label.setText(
                f"{self.title} Draw Mode:\n"
                "• Left-click in the 2D viewport to add points (in node-local space)\n"
                "• Use 'Remove Selected' / 'Clear All' to edit\n"
                f"• Click 'Finish Drawing {self.title}' when done"
            )
            self.help_label.setVisible(True)
        else:
            self.polygon_button.setText(f"Draw {self.title}")
            self.help_label.setVisible(False)
        self.polygon_mode_changed.emit(self.is_creating_polygon)

    def _refresh_vertex_list(self):
        self.vertex_list.clear()
        flat = self._get_flat_vertices()
        for i in range(0, len(flat) - 1, 2):
            try:
                x = float(flat[i])
                y = float(flat[i + 1])
                self.vertex_list.addItem(QListWidgetItem(f"({x:.2f}, {y:.2f})"))
            except (TypeError, ValueError):
                continue

    def _remove_selected_vertex(self):
        row = self.vertex_list.currentRow()
        if row < 0:
            return
        flat = self._get_flat_vertices()
        idx = row * 2
        if idx + 1 < len(flat):
            del flat[idx:idx + 2]
            self._set_flat_vertices(flat)
            self._refresh_vertex_list()

    def _clear_vertices(self):
        self._set_flat_vertices([])
        self._refresh_vertex_list()

    def is_polygon_creation_active(self):
        return self.is_creating_polygon

    def set_value(self, value):
        pass

    def get_value(self):
        return None
