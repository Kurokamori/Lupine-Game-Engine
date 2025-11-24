"""
Custom Inspector Widget for CollisionMesh3D Component

Provides shape type selection, primitive parameters, and mesh solver options.
"""

from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel,
                              QPushButton, QComboBox, QDoubleSpinBox, QFrame,
                              QLineEdit, QFileDialog, QGroupBox)
from PyQt6.QtCore import Qt, pyqtSignal
from PyQt6.QtGui import QFont
import json
import sys
from pathlib import Path

# Add parent directory to path to import theme
sys.path.insert(0, str(Path(__file__).parent.parent))
from theme import get_theme_manager


class CollisionMesh3DPropertyWidget(QWidget):
    """Custom widget for CollisionMesh3D component with dynamic shape parameters"""

    value_changed = pyqtSignal(str, object)  # property_name, value

    def __init__(self, component, editor_bridge, main_editor=None, parent=None):
        super().__init__(parent)
        self.component = component
        self.editor_bridge = editor_bridge
        self.main_editor = main_editor

        self._init_ui()
        self._load_values()

    def _init_ui(self):
        """Initialize the UI"""
        # Get theme colors
        theme = get_theme_manager().get_current_theme()
        bg_color = theme.colors.surface if theme else "#1e1a28"
        text_color = theme.colors.text_primary if theme else "#ffffff"
        border_color = theme.colors.border if theme else "#3d3650"
        text_secondary = theme.colors.text_secondary if theme else "#aaaaaa"

        layout = QVBoxLayout()
        layout.setContentsMargins(5, 5, 5, 5)
        layout.setSpacing(8)

        # Shape Type Selection
        shape_layout = QHBoxLayout()
        shape_label = QLabel("Shape Type:")
        shape_label.setStyleSheet(f"color: {text_color};")
        shape_layout.addWidget(shape_label)

        self.shape_combo = QComboBox()
        self.shape_combo.addItems(["Box", "Sphere", "Capsule", "Cylinder", "Cone", "Plane", "Mesh"])
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

        # Create parameter frames for different shapes
        self._create_box_params(layout, bg_color, text_color, border_color)
        self._create_sphere_params(layout, bg_color, text_color, border_color)
        self._create_capsule_params(layout, bg_color, text_color, border_color)
        self._create_cylinder_params(layout, bg_color, text_color, border_color)
        self._create_cone_params(layout, bg_color, text_color, border_color)
        self._create_plane_params(layout, bg_color, text_color, border_color)
        self._create_mesh_params(layout, bg_color, text_color, border_color)

        layout.addStretch()
        self.setLayout(layout)

    def _create_box_params(self, parent_layout, bg_color, text_color, border_color):
        """Create parameters for Box shape"""
        self.box_frame = QFrame()
        self.box_frame.setStyleSheet(f"background-color: {bg_color}; border: 1px solid {border_color};")
        box_layout = QVBoxLayout(self.box_frame)
        box_layout.setContentsMargins(5, 5, 5, 5)

        # Size (Vec3)
        size_label = QLabel("Size:")
        size_label.setStyleSheet(f"color: {text_color};")
        box_layout.addWidget(size_label)

        size_layout = QHBoxLayout()
        self.box_size_x = self._create_spinbox(bg_color, text_color, border_color)
        self.box_size_y = self._create_spinbox(bg_color, text_color, border_color)
        self.box_size_z = self._create_spinbox(bg_color, text_color, border_color)

        self.box_size_x.valueChanged.connect(lambda: self._on_size_changed())
        self.box_size_y.valueChanged.connect(lambda: self._on_size_changed())
        self.box_size_z.valueChanged.connect(lambda: self._on_size_changed())

        size_layout.addWidget(QLabel("X:", styleSheet=f"color: {text_color};"))
        size_layout.addWidget(self.box_size_x)
        size_layout.addWidget(QLabel("Y:", styleSheet=f"color: {text_color};"))
        size_layout.addWidget(self.box_size_y)
        size_layout.addWidget(QLabel("Z:", styleSheet=f"color: {text_color};"))
        size_layout.addWidget(self.box_size_z)
        box_layout.addLayout(size_layout)

        self.box_frame.setVisible(False)
        parent_layout.addWidget(self.box_frame)

    def _create_sphere_params(self, parent_layout, bg_color, text_color, border_color):
        """Create parameters for Sphere shape"""
        self.sphere_frame = QFrame()
        self.sphere_frame.setStyleSheet(f"background-color: {bg_color}; border: 1px solid {border_color};")
        sphere_layout = QVBoxLayout(self.sphere_frame)
        sphere_layout.setContentsMargins(5, 5, 5, 5)

        # Radius
        radius_layout = QHBoxLayout()
        radius_label = QLabel("Radius:")
        radius_label.setStyleSheet(f"color: {text_color};")
        radius_layout.addWidget(radius_label)

        self.sphere_radius = self._create_spinbox(bg_color, text_color, border_color)
        self.sphere_radius.valueChanged.connect(lambda: self._on_radius_changed())
        radius_layout.addWidget(self.sphere_radius)
        sphere_layout.addLayout(radius_layout)

        self.sphere_frame.setVisible(False)
        parent_layout.addWidget(self.sphere_frame)

    def _create_capsule_params(self, parent_layout, bg_color, text_color, border_color):
        """Create parameters for Capsule shape"""
        self.capsule_frame = QFrame()
        self.capsule_frame.setStyleSheet(f"background-color: {bg_color}; border: 1px solid {border_color};")
        capsule_layout = QVBoxLayout(self.capsule_frame)
        capsule_layout.setContentsMargins(5, 5, 5, 5)

        # Radius
        radius_layout = QHBoxLayout()
        radius_label = QLabel("Radius:")
        radius_label.setStyleSheet(f"color: {text_color};")
        radius_layout.addWidget(radius_label)

        self.capsule_radius = self._create_spinbox(bg_color, text_color, border_color)
        self.capsule_radius.valueChanged.connect(lambda: self._on_radius_changed())
        radius_layout.addWidget(self.capsule_radius)
        capsule_layout.addLayout(radius_layout)

        # Height
        height_layout = QHBoxLayout()
        height_label = QLabel("Height:")
        height_label.setStyleSheet(f"color: {text_color};")
        height_layout.addWidget(height_label)

        self.capsule_height = self._create_spinbox(bg_color, text_color, border_color)
        self.capsule_height.valueChanged.connect(lambda: self._on_height_changed())
        height_layout.addWidget(self.capsule_height)
        capsule_layout.addLayout(height_layout)

        self.capsule_frame.setVisible(False)
        parent_layout.addWidget(self.capsule_frame)

    def _create_cylinder_params(self, parent_layout, bg_color, text_color, border_color):
        """Create parameters for Cylinder shape"""
        self.cylinder_frame = QFrame()
        self.cylinder_frame.setStyleSheet(f"background-color: {bg_color}; border: 1px solid {border_color};")
        cylinder_layout = QVBoxLayout(self.cylinder_frame)
        cylinder_layout.setContentsMargins(5, 5, 5, 5)

        # Radius
        radius_layout = QHBoxLayout()
        radius_label = QLabel("Radius:")
        radius_label.setStyleSheet(f"color: {text_color};")
        radius_layout.addWidget(radius_label)

        self.cylinder_radius = self._create_spinbox(bg_color, text_color, border_color)
        self.cylinder_radius.valueChanged.connect(lambda: self._on_radius_changed())
        radius_layout.addWidget(self.cylinder_radius)
        cylinder_layout.addLayout(radius_layout)

        # Height
        height_layout = QHBoxLayout()
        height_label = QLabel("Height:")
        height_label.setStyleSheet(f"color: {text_color};")
        height_layout.addWidget(height_label)

        self.cylinder_height = self._create_spinbox(bg_color, text_color, border_color)
        self.cylinder_height.valueChanged.connect(lambda: self._on_height_changed())
        height_layout.addWidget(self.cylinder_height)
        cylinder_layout.addLayout(height_layout)

        self.cylinder_frame.setVisible(False)
        parent_layout.addWidget(self.cylinder_frame)

    def _create_cone_params(self, parent_layout, bg_color, text_color, border_color):
        """Create parameters for Cone shape"""
        self.cone_frame = QFrame()
        self.cone_frame.setStyleSheet(f"background-color: {bg_color}; border: 1px solid {border_color};")
        cone_layout = QVBoxLayout(self.cone_frame)
        cone_layout.setContentsMargins(5, 5, 5, 5)

        # Radius
        radius_layout = QHBoxLayout()
        radius_label = QLabel("Radius:")
        radius_label.setStyleSheet(f"color: {text_color};")
        radius_layout.addWidget(radius_label)

        self.cone_radius = self._create_spinbox(bg_color, text_color, border_color)
        self.cone_radius.valueChanged.connect(lambda: self._on_radius_changed())
        radius_layout.addWidget(self.cone_radius)
        cone_layout.addLayout(radius_layout)

        # Height
        height_layout = QHBoxLayout()
        height_label = QLabel("Height:")
        height_label.setStyleSheet(f"color: {text_color};")
        height_layout.addWidget(height_label)

        self.cone_height = self._create_spinbox(bg_color, text_color, border_color)
        self.cone_height.valueChanged.connect(lambda: self._on_height_changed())
        height_layout.addWidget(self.cone_height)
        cone_layout.addLayout(height_layout)

        self.cone_frame.setVisible(False)
        parent_layout.addWidget(self.cone_frame)

    def _create_plane_params(self, parent_layout, bg_color, text_color, border_color):
        """Create parameters for Plane shape"""
        self.plane_frame = QFrame()
        self.plane_frame.setStyleSheet(f"background-color: {bg_color}; border: 1px solid {border_color};")
        plane_layout = QVBoxLayout(self.plane_frame)
        plane_layout.setContentsMargins(5, 5, 5, 5)

        # Width
        width_label = QLabel("Width:")
        width_label.setStyleSheet(f"color: {text_color};")
        plane_layout.addWidget(width_label)

        self.plane_width = QDoubleSpinBox()
        self.plane_width.setRange(0.1, 1000.0)
        self.plane_width.setSingleStep(0.1)
        self.plane_width.setValue(10.0)
        self.plane_width.setStyleSheet(f"background-color: {bg_color}; color: {text_color}; border: 1px solid {border_color};")
        self.plane_width.valueChanged.connect(self._on_plane_width_changed)
        plane_layout.addWidget(self.plane_width)

        # Length
        length_label = QLabel("Length:")
        length_label.setStyleSheet(f"color: {text_color};")
        plane_layout.addWidget(length_label)

        self.plane_length = QDoubleSpinBox()
        self.plane_length.setRange(0.1, 1000.0)
        self.plane_length.setSingleStep(0.1)
        self.plane_length.setValue(10.0)
        self.plane_length.setStyleSheet(f"background-color: {bg_color}; color: {text_color}; border: 1px solid {border_color};")
        self.plane_length.valueChanged.connect(self._on_plane_length_changed)
        plane_layout.addWidget(self.plane_length)

        # Normal (Vec3)
        normal_label = QLabel("Normal:")
        normal_label.setStyleSheet(f"color: {text_color};")
        plane_layout.addWidget(normal_label)

        normal_layout = QHBoxLayout()
        self.plane_normal_x = self._create_spinbox(bg_color, text_color, border_color, min_val=-1.0, max_val=1.0)
        self.plane_normal_y = self._create_spinbox(bg_color, text_color, border_color, min_val=-1.0, max_val=1.0)
        self.plane_normal_z = self._create_spinbox(bg_color, text_color, border_color, min_val=-1.0, max_val=1.0)

        self.plane_normal_x.valueChanged.connect(lambda: self._on_plane_normal_changed())
        self.plane_normal_y.valueChanged.connect(lambda: self._on_plane_normal_changed())
        self.plane_normal_z.valueChanged.connect(lambda: self._on_plane_normal_changed())

        normal_layout.addWidget(QLabel("X:", styleSheet=f"color: {text_color};"))
        normal_layout.addWidget(self.plane_normal_x)
        normal_layout.addWidget(QLabel("Y:", styleSheet=f"color: {text_color};"))
        normal_layout.addWidget(self.plane_normal_y)
        normal_layout.addWidget(QLabel("Z:", styleSheet=f"color: {text_color};"))
        normal_layout.addWidget(self.plane_normal_z)
        plane_layout.addLayout(normal_layout)

        # Distance
        distance_layout = QHBoxLayout()
        distance_label = QLabel("Distance:")
        distance_label.setStyleSheet(f"color: {text_color};")
        distance_layout.addWidget(distance_label)

        self.plane_distance = self._create_spinbox(bg_color, text_color, border_color)
        self.plane_distance.valueChanged.connect(lambda: self._on_plane_distance_changed())
        distance_layout.addWidget(self.plane_distance)
        plane_layout.addLayout(distance_layout)

        self.plane_frame.setVisible(False)
        parent_layout.addWidget(self.plane_frame)


    def _create_mesh_params(self, parent_layout, bg_color, text_color, border_color):
        """Create parameters for Mesh shape"""
        self.mesh_frame = QFrame()
        self.mesh_frame.setStyleSheet(f"background-color: {bg_color}; border: 1px solid {border_color};")
        mesh_layout = QVBoxLayout(self.mesh_frame)
        mesh_layout.setContentsMargins(5, 5, 5, 5)

        # Mesh Path
        path_label = QLabel("Mesh Path:")
        path_label.setStyleSheet(f"color: {text_color};")
        mesh_layout.addWidget(path_label)

        path_layout = QHBoxLayout()
        self.mesh_path_edit = QLineEdit()
        self.mesh_path_edit.setStyleSheet(f"""
            QLineEdit {{
                background-color: {bg_color};
                color: {text_color};
                border: 1px solid {border_color};
                padding: 4px;
            }}
        """)
        self.mesh_path_edit.textChanged.connect(self._on_mesh_path_changed)
        path_layout.addWidget(self.mesh_path_edit)

        browse_btn = QPushButton("Browse...")
        browse_btn.clicked.connect(self._browse_mesh)
        path_layout.addWidget(browse_btn)
        mesh_layout.addLayout(path_layout)

        # Mesh Solver
        solver_layout = QHBoxLayout()
        solver_label = QLabel("Solver:")
        solver_label.setStyleSheet(f"color: {text_color};")
        solver_layout.addWidget(solver_label)

        self.mesh_solver_combo = QComboBox()
        self.mesh_solver_combo.addItems(["Bounding Box", "Convex", "Concave", "TriMesh"])
        self.mesh_solver_combo.setStyleSheet(f"""
            QComboBox {{
                background-color: {bg_color};
                color: {text_color};
                border: 1px solid {border_color};
                padding: 4px;
            }}
        """)
        self.mesh_solver_combo.currentIndexChanged.connect(self._on_mesh_solver_changed)
        solver_layout.addWidget(self.mesh_solver_combo)
        mesh_layout.addLayout(solver_layout)

        self.mesh_frame.setVisible(False)
        parent_layout.addWidget(self.mesh_frame)

    def _create_spinbox(self, bg_color, text_color, border_color, min_val=0.0, max_val=1000.0):
        """Helper to create a styled spinbox"""
        spinbox = QDoubleSpinBox()
        spinbox.setRange(min_val, max_val)
        spinbox.setSingleStep(0.1)
        spinbox.setDecimals(3)
        spinbox.setValue(1.0)
        spinbox.setStyleSheet(f"""
            QDoubleSpinBox {{
                background-color: {bg_color};
                color: {text_color};
                border: 1px solid {border_color};
                padding: 2px;
            }}
        """)
        return spinbox

    def _load_values(self):
        """Load current values from component"""
        if not self.component or not self.editor_bridge:
            return

        try:
            # Get shape type
            shape_type_json = self.editor_bridge.get_component_property(self.component, "shapeType")
            shape_type = json.loads(shape_type_json)
            if shape_type is not None:
                self.shape_combo.blockSignals(True)
                self.shape_combo.setCurrentIndex(shape_type)
                self.shape_combo.blockSignals(False)
                self._update_ui_for_shape(shape_type)

            # Load shape-specific parameters
            if shape_type == 0:  # Box
                size_json = self.editor_bridge.get_component_property(self.component, "size")
                size = json.loads(size_json)
                if size and len(size) == 3:
                    self.box_size_x.blockSignals(True)
                    self.box_size_y.blockSignals(True)
                    self.box_size_z.blockSignals(True)
                    self.box_size_x.setValue(size[0])
                    self.box_size_y.setValue(size[1])
                    self.box_size_z.setValue(size[2])
                    self.box_size_x.blockSignals(False)
                    self.box_size_y.blockSignals(False)
                    self.box_size_z.blockSignals(False)

            elif shape_type in [1, 2, 3, 4]:  # Sphere, Capsule, Cylinder, Cone
                radius_json = self.editor_bridge.get_component_property(self.component, "radius")
                radius = json.loads(radius_json)
                if radius is not None:
                    if shape_type == 1:  # Sphere
                        self.sphere_radius.blockSignals(True)
                        self.sphere_radius.setValue(radius)
                        self.sphere_radius.blockSignals(False)
                    elif shape_type == 2:  # Capsule
                        self.capsule_radius.blockSignals(True)
                        self.capsule_radius.setValue(radius)
                        self.capsule_radius.blockSignals(False)
                    elif shape_type == 3:  # Cylinder
                        self.cylinder_radius.blockSignals(True)
                        self.cylinder_radius.setValue(radius)
                        self.cylinder_radius.blockSignals(False)
                    elif shape_type == 4:  # Cone
                        self.cone_radius.blockSignals(True)
                        self.cone_radius.setValue(radius)
                        self.cone_radius.blockSignals(False)

                if shape_type in [2, 3, 4]:  # Capsule, Cylinder, Cone have height
                    height_json = self.editor_bridge.get_component_property(self.component, "height")
                    height = json.loads(height_json)
                    if height is not None:
                        if shape_type == 2:  # Capsule
                            self.capsule_height.blockSignals(True)
                            self.capsule_height.setValue(height)
                            self.capsule_height.blockSignals(False)
                        elif shape_type == 3:  # Cylinder
                            self.cylinder_height.blockSignals(True)
                            self.cylinder_height.setValue(height)
                            self.cylinder_height.blockSignals(False)
                        elif shape_type == 4:  # Cone
                            self.cone_height.blockSignals(True)
                            self.cone_height.setValue(height)
                            self.cone_height.blockSignals(False)

            elif shape_type == 5:  # Plane
                width_json = self.editor_bridge.get_component_property(self.component, "planeWidth")
                width = json.loads(width_json)
                if width is not None:
                    self.plane_width.blockSignals(True)
                    self.plane_width.setValue(width)
                    self.plane_width.blockSignals(False)

                length_json = self.editor_bridge.get_component_property(self.component, "planeLength")
                length = json.loads(length_json)
                if length is not None:
                    self.plane_length.blockSignals(True)
                    self.plane_length.setValue(length)
                    self.plane_length.blockSignals(False)

                normal_json = self.editor_bridge.get_component_property(self.component, "planeNormal")
                normal = json.loads(normal_json)
                if normal and len(normal) == 3:
                    self.plane_normal_x.blockSignals(True)
                    self.plane_normal_y.blockSignals(True)
                    self.plane_normal_z.blockSignals(True)
                    self.plane_normal_x.setValue(normal[0])
                    self.plane_normal_y.setValue(normal[1])
                    self.plane_normal_z.setValue(normal[2])
                    self.plane_normal_x.blockSignals(False)
                    self.plane_normal_y.blockSignals(False)
                    self.plane_normal_z.blockSignals(False)

                distance_json = self.editor_bridge.get_component_property(self.component, "planeDistance")
                distance = json.loads(distance_json)
                if distance is not None:
                    self.plane_distance.blockSignals(True)
                    self.plane_distance.setValue(distance)
                    self.plane_distance.blockSignals(False)

            elif shape_type == 6:  # Mesh
                path_json = self.editor_bridge.get_component_property(self.component, "meshPath")
                path = json.loads(path_json)
                if path:
                    self.mesh_path_edit.blockSignals(True)
                    self.mesh_path_edit.setText(path)
                    self.mesh_path_edit.blockSignals(False)

                solver_json = self.editor_bridge.get_component_property(self.component, "meshSolver")
                solver = json.loads(solver_json)
                if solver is not None:
                    self.mesh_solver_combo.blockSignals(True)
                    self.mesh_solver_combo.setCurrentIndex(solver)
                    self.mesh_solver_combo.blockSignals(False)

        except Exception as e:
            print(f"Error loading CollisionMesh3D values: {e}")

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
        # Hide all frames
        self.box_frame.setVisible(False)
        self.sphere_frame.setVisible(False)
        self.capsule_frame.setVisible(False)
        self.cylinder_frame.setVisible(False)
        self.cone_frame.setVisible(False)
        self.plane_frame.setVisible(False)
        self.mesh_frame.setVisible(False)

        # Show appropriate frame
        if shape_index == 0:  # Box
            self.box_frame.setVisible(True)
        elif shape_index == 1:  # Sphere
            self.sphere_frame.setVisible(True)
        elif shape_index == 2:  # Capsule
            self.capsule_frame.setVisible(True)
        elif shape_index == 3:  # Cylinder
            self.cylinder_frame.setVisible(True)
        elif shape_index == 4:  # Cone
            self.cone_frame.setVisible(True)
        elif shape_index == 5:  # Plane
            self.plane_frame.setVisible(True)
        elif shape_index == 6:  # Mesh
            self.mesh_frame.setVisible(True)

    def _on_size_changed(self):
        """Handle size change for Box"""
        if not self.component or not self.editor_bridge:
            return

        try:
            size = [self.box_size_x.value(), self.box_size_y.value(), self.box_size_z.value()]
            self.editor_bridge.set_component_property(
                self.component,
                "size",
                json.dumps(size)
            )
            self.value_changed.emit("size", size)
        except Exception as e:
            print(f"Error setting size: {e}")

    def _on_radius_changed(self):
        """Handle radius change for Sphere/Capsule/Cylinder/Cone"""
        if not self.component or not self.editor_bridge:
            return

        try:
            shape_type = self.shape_combo.currentIndex()
            radius = 0.0

            if shape_type == 1:  # Sphere
                radius = self.sphere_radius.value()
            elif shape_type == 2:  # Capsule
                radius = self.capsule_radius.value()
            elif shape_type == 3:  # Cylinder
                radius = self.cylinder_radius.value()
            elif shape_type == 4:  # Cone
                radius = self.cone_radius.value()

            self.editor_bridge.set_component_property(
                self.component,
                "radius",
                json.dumps(radius)
            )
            self.value_changed.emit("radius", radius)
        except Exception as e:
            print(f"Error setting radius: {e}")

    def _on_height_changed(self):
        """Handle height change for Capsule/Cylinder/Cone"""
        if not self.component or not self.editor_bridge:
            return

        try:
            shape_type = self.shape_combo.currentIndex()
            height = 0.0

            if shape_type == 2:  # Capsule
                height = self.capsule_height.value()
            elif shape_type == 3:  # Cylinder
                height = self.cylinder_height.value()
            elif shape_type == 4:  # Cone
                height = self.cone_height.value()

            self.editor_bridge.set_component_property(
                self.component,
                "height",
                json.dumps(height)
            )
            self.value_changed.emit("height", height)
        except Exception as e:
            print(f"Error setting height: {e}")

    def _on_plane_normal_changed(self):
        """Handle plane normal change"""
        if not self.component or not self.editor_bridge:
            return

        try:
            normal = [self.plane_normal_x.value(), self.plane_normal_y.value(), self.plane_normal_z.value()]
            self.editor_bridge.set_component_property(
                self.component,
                "planeNormal",
                json.dumps(normal)
            )
            self.value_changed.emit("planeNormal", normal)
        except Exception as e:
            print(f"Error setting plane normal: {e}")

    def _on_plane_distance_changed(self):
        """Handle plane distance change"""
        if not self.component or not self.editor_bridge:
            return

        try:
            distance = self.plane_distance.value()
            self.editor_bridge.set_component_property(
                self.component,
                "planeDistance",
                json.dumps(distance)
            )
            self.value_changed.emit("planeDistance", distance)
        except Exception as e:
            print(f"Error setting plane distance: {e}")

    def _on_plane_width_changed(self):
        """Handle plane width change"""
        if not self.component or not self.editor_bridge:
            return

        try:
            width = self.plane_width.value()
            self.editor_bridge.set_component_property(
                self.component,
                "planeWidth",
                json.dumps(width)
            )
            self.value_changed.emit("planeWidth", width)
        except Exception as e:
            print(f"Error setting plane width: {e}")

    def _on_plane_length_changed(self):
        """Handle plane length change"""
        if not self.component or not self.editor_bridge:
            return

        try:
            length = self.plane_length.value()
            self.editor_bridge.set_component_property(
                self.component,
                "planeLength",
                json.dumps(length)
            )
            self.value_changed.emit("planeLength", length)
        except Exception as e:
            print(f"Error setting plane length: {e}")

    def _on_mesh_path_changed(self):
        """Handle mesh path change"""
        if not self.component or not self.editor_bridge:
            return

        try:
            path = self.mesh_path_edit.text()
            self.editor_bridge.set_component_property(
                self.component,
                "meshPath",
                json.dumps(path)
            )
            self.value_changed.emit("meshPath", path)
        except Exception as e:
            print(f"Error setting mesh path: {e}")

    def _on_mesh_solver_changed(self, index):
        """Handle mesh solver change"""
        if not self.component or not self.editor_bridge:
            return

        try:
            self.editor_bridge.set_component_property(
                self.component,
                "meshSolver",
                json.dumps(index)
            )
            self.value_changed.emit("meshSolver", index)
        except Exception as e:
            print(f"Error setting mesh solver: {e}")

    def _browse_mesh(self):
        """Browse for mesh file"""
        from PyQt6.QtWidgets import QFileDialog

        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Select Mesh File",
            "",
            "Model Files (*.obj *.fbx *.gltf *.glb);;All Files (*.*)"
        )

        if file_path:
            self.mesh_path_edit.setText(file_path)

    def set_value(self, value):
        """Set widget value (for compatibility with PropertyWidget interface)"""
        pass

    def get_value(self):
        """Get widget value (for compatibility with PropertyWidget interface)"""
        return None

