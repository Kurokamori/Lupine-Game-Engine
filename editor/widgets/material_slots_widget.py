"""
Material Slots Widget for StaticMesh3D Inspector

Displays multiple material slots with collapsible/toggleable sections for each slot.
Reuses MaterialOverridePropertyWidget internals for consistency.
"""

from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel,
                              QCheckBox, QPushButton, QFrame)
from PyQt6.QtCore import Qt, pyqtSignal
from PyQt6.QtGui import QFont
import json
import sys
from pathlib import Path

# Add parent directory to path to import theme
sys.path.insert(0, str(Path(__file__).parent.parent))
from theme import get_theme_manager


class MaterialSlotWidget(QWidget):
    """Widget for a single material slot with collapsible override properties"""

    value_changed = pyqtSignal(int, str, object)  # slot_index, property_name, value

    def __init__(self, slot_index, slot_name, component, editor_bridge, main_editor=None, parent=None):
        super().__init__(parent)
        self.slot_index = slot_index
        self.slot_name = slot_name
        self.component = component
        self.editor_bridge = editor_bridge
        self.main_editor = main_editor
        self.category_widgets = {}

        self._init_ui()
        self._load_values()

    def _log(self, message, level="Debug"):
        """Helper to log messages to console if available"""
        if self.main_editor and hasattr(self.main_editor, 'panels') and 'console' in self.main_editor.panels:
            self.main_editor.panels['console'].log_message(f"[MaterialSlotWidget] {message}", level)

    def _init_ui(self):
        """Initialize the UI"""
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(2)

        # Get theme colors
        theme = get_theme_manager().get_current_theme()
        bg_color = theme.colors.surface if theme else "#1e1a28"
        text_color = theme.colors.text_primary if theme else "#ffffff"
        border_color = theme.colors.border if theme else "#3d3650"

        # Header with collapse button and enable checkbox
        header_layout = QHBoxLayout()
        header_layout.setContentsMargins(0, 2, 0, 2)
        header_layout.setSpacing(4)

        # Collapse button (checkable)
        self.collapse_btn = QPushButton(f"▶ {self.slot_name}")
        self.collapse_btn.setCheckable(True)
        self.collapse_btn.setChecked(False)
        self.collapse_btn.setFlat(True)
        self.collapse_btn.setStyleSheet(f"""
            QPushButton {{
                text-align: left;
                padding: 4px;
                background-color: {bg_color};
                color: {text_color};
                border: 1px solid {border_color};
                border-radius: 3px;
            }}
            QPushButton:hover {{
                background-color: {theme.colors.surface_hover if theme else "#3a3a3a"};
            }}
        """)
        self.collapse_btn.toggled.connect(self._toggle_collapsed)
        header_layout.addWidget(self.collapse_btn, 1)

        # Enable override checkbox
        self.enable_checkbox = QCheckBox("Override")
        self.enable_checkbox.setChecked(False)
        self.enable_checkbox.setStyleSheet(f"color: {text_color};")
        self.enable_checkbox.stateChanged.connect(self._on_enabled_changed)
        header_layout.addWidget(self.enable_checkbox)

        layout.addLayout(header_layout)

        # Properties container (collapsible)
        self.properties_widget = QWidget()
        self.properties_layout = QVBoxLayout(self.properties_widget)
        self.properties_layout.setContentsMargins(15, 2, 0, 2)
        self.properties_layout.setSpacing(2)

        # Create categories similar to MaterialOverridePropertyWidget
        self._create_material_categories()

        layout.addWidget(self.properties_widget)

        # Initially collapsed
        self.properties_widget.setVisible(False)
    
    def _create_material_categories(self):
        """Create material property categories (reusing MaterialOverridePropertyWidget structure)"""
        from editor.panels.inspector_panel import (
            ColorPropertyWidget, FloatPropertyWidget, PathPropertyWidget
        )

        # Albedo category
        albedo_category = self._create_category("Albedo")
        self.category_widgets['Albedo'] = {
            'frame': albedo_category,
            'widgets': {}
        }

        albedo_color = ColorPropertyWidget("Color")
        albedo_color.value_changed.connect(lambda v: self._on_property_changed('albedoColor', v))
        albedo_category.layout().addWidget(albedo_color)
        self.category_widgets['Albedo']['widgets']['color'] = albedo_color

        albedo_texture = PathPropertyWidget("Texture")
        albedo_texture.value_changed.connect(lambda v: self._on_property_changed('albedoTexturePath', v))
        albedo_category.layout().addWidget(albedo_texture)
        self.category_widgets['Albedo']['widgets']['texture'] = albedo_texture

        self.properties_layout.addWidget(albedo_category)

        # Metallic/Roughness category
        metal_rough_category = self._create_category("Metallic/Roughness")
        self.category_widgets['MetallicRoughness'] = {
            'frame': metal_rough_category,
            'widgets': {}
        }

        metallic_widget = FloatPropertyWidget("Metallic")
        metallic_widget.value_changed.connect(lambda v: self._on_property_changed('metallic', v))
        metal_rough_category.layout().addWidget(metallic_widget)
        self.category_widgets['MetallicRoughness']['widgets']['metallic'] = metallic_widget

        roughness_widget = FloatPropertyWidget("Roughness")
        roughness_widget.value_changed.connect(lambda v: self._on_property_changed('roughness', v))
        metal_rough_category.layout().addWidget(roughness_widget)
        self.category_widgets['MetallicRoughness']['widgets']['roughness'] = roughness_widget

        metal_rough_texture = PathPropertyWidget("Texture")
        metal_rough_texture.value_changed.connect(lambda v: self._on_property_changed('metallicRoughnessTexturePath', v))
        metal_rough_category.layout().addWidget(metal_rough_texture)
        self.category_widgets['MetallicRoughness']['widgets']['texture'] = metal_rough_texture

        self.properties_layout.addWidget(metal_rough_category)

        # Normal category
        normal_category = self._create_category("Normal")
        self.category_widgets['Normal'] = {
            'frame': normal_category,
            'widgets': {}
        }

        normal_texture = PathPropertyWidget("Texture")
        normal_texture.value_changed.connect(lambda v: self._on_property_changed('normalTexturePath', v))
        normal_category.layout().addWidget(normal_texture)
        self.category_widgets['Normal']['widgets']['texture'] = normal_texture

        normal_scale = FloatPropertyWidget("Scale")
        normal_scale.value_changed.connect(lambda v: self._on_property_changed('normalScale', v))
        normal_category.layout().addWidget(normal_scale)
        self.category_widgets['Normal']['widgets']['scale'] = normal_scale

        self.properties_layout.addWidget(normal_category)

        # Emissive category
        emissive_category = self._create_category("Emissive")
        self.category_widgets['Emissive'] = {
            'frame': emissive_category,
            'widgets': {}
        }

        emissive_color = ColorPropertyWidget("Color")
        emissive_color.value_changed.connect(lambda v: self._on_property_changed('emissiveColor', v))
        emissive_category.layout().addWidget(emissive_color)
        self.category_widgets['Emissive']['widgets']['color'] = emissive_color

        emissive_strength = FloatPropertyWidget("Strength")
        emissive_strength.value_changed.connect(lambda v: self._on_property_changed('emissiveStrength', v))
        emissive_category.layout().addWidget(emissive_strength)
        self.category_widgets['Emissive']['widgets']['strength'] = emissive_strength

        emissive_texture = PathPropertyWidget("Texture")
        emissive_texture.value_changed.connect(lambda v: self._on_property_changed('emissiveTexturePath', v))
        emissive_category.layout().addWidget(emissive_texture)
        self.category_widgets['Emissive']['widgets']['texture'] = emissive_texture

        self.properties_layout.addWidget(emissive_category)

        # Alpha category
        alpha_category = self._create_category("Alpha")
        self.category_widgets['Alpha'] = {
            'frame': alpha_category,
            'widgets': {}
        }

        alpha_cutoff = FloatPropertyWidget("Cutoff")
        alpha_cutoff.value_changed.connect(lambda v: self._on_property_changed('alphaCutoff', v))
        alpha_category.layout().addWidget(alpha_cutoff)
        self.category_widgets['Alpha']['widgets']['cutoff'] = alpha_cutoff

        self.properties_layout.addWidget(alpha_category)

        # Add stretch at the end
        self.properties_layout.addStretch()

    def _create_category(self, title):
        """Create a category frame"""
        # Get theme colors
        theme = get_theme_manager().get_current_theme()
        bg_color = theme.colors.surface if theme else "#1e1a28"
        text_color = theme.colors.text_primary if theme else "#ffffff"
        border_color = theme.colors.border if theme else "#3d3650"

        frame = QFrame()
        frame.setFrameShape(QFrame.Shape.StyledPanel)
        frame.setStyleSheet(f"""
            QFrame {{
                background-color: {bg_color};
                border: 1px solid {border_color};
                border-radius: 3px;
                padding: 4px;
            }}
        """)

        layout = QVBoxLayout(frame)
        layout.setContentsMargins(6, 6, 6, 6)
        layout.setSpacing(2)

        # Category title
        title_label = QLabel(title)
        font = QFont()
        font.setBold(True)
        title_label.setFont(font)
        title_label.setStyleSheet(f"color: {text_color}; border: none; padding: 0px;")
        layout.addWidget(title_label)

        return frame

    def _toggle_collapsed(self, checked):
        """Toggle the collapsed state"""
        self.properties_widget.setVisible(checked)
        arrow = "▼" if checked else "▶"
        self.collapse_btn.setText(f"{arrow} {self.slot_name}")

    def _on_enabled_changed(self, state):
        """Handle enable override checkbox change"""
        enabled = (state == Qt.CheckState.Checked.value)
        self.properties_widget.setEnabled(enabled)
        self._on_property_changed('enableOverride', enabled)

    def _on_property_changed(self, property_name, value):
        """Handle property value change"""
        if not self.component or not self.editor_bridge:
            return

        try:
            # Convert value to JSON
            if property_name in ['albedoColor', 'emissiveColor']:
                # Color value - convert QColor to dict
                value_json = json.dumps({
                    'r': value.redF(),
                    'g': value.greenF(),
                    'b': value.blueF(),
                    'a': value.alphaF()
                })
            elif isinstance(value, (int, float, bool, str)):
                value_json = json.dumps(value)
            else:
                value_json = json.dumps(str(value))

            # Set via EditorBridge
            self.editor_bridge.set_material_slot_property(
                self.component,
                self.slot_index,
                property_name,
                value_json
            )

            # Emit signal
            self.value_changed.emit(self.slot_index, property_name, value)

        except Exception as e:
            self._log(f"Failed to set material slot property: {e}", "Warning")

    def _load_values(self):
        """Load current values from component"""
        if not self.component or not self.editor_bridge:
            return

        try:
            # Get slot properties from EditorBridge
            props_json = self.editor_bridge.get_material_slot_properties(self.component, self.slot_index)
            if not props_json:
                return
            properties = json.loads(props_json)

            # Set enable override checkbox
            if 'enableOverride' in properties:
                self.enable_checkbox.setChecked(properties['enableOverride'])
                self.properties_widget.setEnabled(properties['enableOverride'])

            # Load albedo
            if 'Albedo' in self.category_widgets:
                widgets = self.category_widgets['Albedo']['widgets']
                if 'color' in widgets and 'albedoColor' in properties:
                    albedo = properties['albedoColor']
                    # ColorPropertyWidget expects a tuple (r, g, b, a)
                    widgets['color'].set_value((albedo.get('r', 1.0), albedo.get('g', 1.0),
                                                albedo.get('b', 1.0), albedo.get('a', 1.0)))
                if 'texture' in widgets and 'albedoTexturePath' in properties:
                    widgets['texture'].set_value(properties['albedoTexturePath'])

            # Load metallic/roughness
            if 'MetallicRoughness' in self.category_widgets:
                widgets = self.category_widgets['MetallicRoughness']['widgets']
                if 'metallic' in widgets and 'metallic' in properties:
                    widgets['metallic'].set_value(properties['metallic'])
                if 'roughness' in widgets and 'roughness' in properties:
                    widgets['roughness'].set_value(properties['roughness'])
                if 'texture' in widgets and 'metallicRoughnessTexturePath' in properties:
                    widgets['texture'].set_value(properties['metallicRoughnessTexturePath'])

            # Load normal
            if 'Normal' in self.category_widgets:
                widgets = self.category_widgets['Normal']['widgets']
                if 'texture' in widgets and 'normalTexturePath' in properties:
                    widgets['texture'].set_value(properties['normalTexturePath'])
                if 'scale' in widgets and 'normalScale' in properties:
                    widgets['scale'].set_value(properties['normalScale'])

            # Load emissive
            if 'Emissive' in self.category_widgets:
                widgets = self.category_widgets['Emissive']['widgets']
                if 'color' in widgets and 'emissiveColor' in properties:
                    emissive = properties['emissiveColor']
                    # ColorPropertyWidget expects a tuple (r, g, b, a)
                    widgets['color'].set_value((emissive.get('r', 0.0), emissive.get('g', 0.0),
                                                emissive.get('b', 0.0), emissive.get('a', 1.0)))
                if 'strength' in widgets and 'emissiveStrength' in properties:
                    widgets['strength'].set_value(properties['emissiveStrength'])
                if 'texture' in widgets and 'emissiveTexturePath' in properties:
                    widgets['texture'].set_value(properties['emissiveTexturePath'])

            # Load alpha
            if 'Alpha' in self.category_widgets:
                widgets = self.category_widgets['Alpha']['widgets']
                if 'cutoff' in widgets and 'alphaCutoff' in properties:
                    widgets['cutoff'].set_value(properties['alphaCutoff'])

        except Exception as e:
            self._log(f"Failed to load material slot values: {e}", "Warning")


class MaterialSlotsPropertyWidget(QWidget):
    """Widget for managing multiple material slots for StaticMesh3D"""

    value_changed = pyqtSignal(object)  # Emits when any slot changes

    def __init__(self, component, editor_bridge, main_editor=None, parent=None):
        super().__init__(parent)
        self.component = component
        self.editor_bridge = editor_bridge
        self.main_editor = main_editor
        self.slot_widgets = []
        self._loading = False  # Flag to prevent re-entrant calls
        self._retry_count = 0  # Track retry attempts for deferred refresh
        self._max_retries = 10  # Maximum number of retry attempts

        self._init_ui()
        self._load_slots()
        self._log(f"MaterialSlotsPropertyWidget initialized for component: {component.get_type_name() if component else 'None'}")

        # Schedule a deferred refresh to catch slots that are created after initialization
        # This handles the case where a model is already loaded but slots aren't ready yet
        from PyQt6.QtCore import QTimer
        QTimer.singleShot(100, self._deferred_refresh)

    def _log(self, message, level="Debug"):
        """Helper to log messages to console if available"""
        if self.main_editor and hasattr(self.main_editor, 'panels') and 'console' in self.main_editor.panels:
            self.main_editor.panels['console'].log_message(f"[MaterialSlotsWidget] {message}", level)

    def _init_ui(self):
        """Initialize the UI"""
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(4)

        # Get theme colors
        theme = get_theme_manager().get_current_theme()
        text_color = theme.colors.text_primary if theme else "#ffffff"

        # Title
        title_label = QLabel("Material Slots")
        font = QFont()
        font.setBold(True)
        font.setPointSize(10)
        title_label.setFont(font)
        title_label.setStyleSheet(f"color: {text_color}; padding: 4px;")
        layout.addWidget(title_label)

        # Container for slot widgets (no scroll area - let parent handle scrolling)
        self.slots_container = QWidget()
        self.slots_layout = QVBoxLayout(self.slots_container)
        self.slots_layout.setContentsMargins(0, 0, 0, 0)
        self.slots_layout.setSpacing(4)
        self.slots_layout.addStretch()

        layout.addWidget(self.slots_container)

    def _load_slots(self):
        """Load material slots from component"""
        self._log(f"_load_slots for component: {self.component.get_type_name() if self.component else 'None'}")

        if not self.component or not self.editor_bridge:
            self._log("Missing component or bridge!", "Warning")
            return

        # Prevent re-entrant calls
        if hasattr(self, '_loading') and self._loading:
            self._log("Already loading, skipping re-entrant call", "Warning")
            return

        self._loading = True
        try:
            # Get slot count
            slot_count = self.editor_bridge.get_material_slot_count(self.component)
            self._log(f"Slot count: {slot_count}")

            # Clear existing widgets - do this carefully to avoid crashes
            for widget in self.slot_widgets:
                try:
                    self.slots_layout.removeWidget(widget)
                    widget.setParent(None)  # Explicitly unparent before deleting
                    widget.deleteLater()
                except Exception as e:
                    self._log(f"Error removing widget: {e}", "Warning")
            self.slot_widgets.clear()

            # If no slots, show a message
            if slot_count == 0:
                no_slots_label = QLabel("No material slots available.\nLoad a model to see material slots.")
                no_slots_label.setWordWrap(True)
                no_slots_label.setStyleSheet("color: #888; font-style: italic; padding: 10px;")
                self.slots_layout.insertWidget(0, no_slots_label)
                self.slot_widgets.append(no_slots_label)  # Track for cleanup
                self._log("No slots, added message label")
                return

            # Create widget for each slot
            for i in range(slot_count):
                self._log(f"Creating slot {i}")

                # Get slot properties to get the name
                props_json = self.editor_bridge.get_material_slot_properties(self.component, i)
                self._log(f"Slot {i} props_json: {props_json[:100] if props_json else 'None'}...")

                properties = json.loads(props_json) if props_json else {}
                slot_name = properties.get('name', f'Material {i}')
                self._log(f"Slot {i} name: {slot_name}")

                # Create slot widget
                slot_widget = MaterialSlotWidget(i, slot_name, self.component, self.editor_bridge, self.main_editor)
                slot_widget.value_changed.connect(self._on_slot_changed)

                # Insert before stretch
                self.slots_layout.insertWidget(self.slots_layout.count() - 1, slot_widget)
                self.slot_widgets.append(slot_widget)
                self._log(f"Added slot widget {i}")

            self._log(f"Completed, created {len(self.slot_widgets)} widgets")

        except Exception as e:
            self._log(f"ERROR: {e}", "Error")
            import traceback
            traceback.print_exc()
        finally:
            self._loading = False

    def _on_slot_changed(self, slot_index, property_name, value):
        """Handle slot property change"""
        self.value_changed.emit(value)

    def _deferred_refresh(self):
        """Deferred refresh called after initialization to catch late-loading slots"""
        if not self.component or not self.editor_bridge:
            return

        try:
            current_count = self.editor_bridge.get_material_slot_count(self.component)
            self._log(f"Deferred refresh triggered (attempt {self._retry_count + 1}/{self._max_retries}), slot count: {current_count}")

            # If slots are available now, refresh
            if current_count > 0:
                self._log(f"Slots are ready ({current_count}), refreshing")
                self._retry_count = 0  # Reset retry count
                self.refresh()
            elif self._retry_count < self._max_retries:
                # Slots not ready yet, retry after a delay
                self._retry_count += 1
                self._log(f"Slots not ready yet, scheduling retry {self._retry_count}/{self._max_retries}")
                from PyQt6.QtCore import QTimer
                QTimer.singleShot(100, self._deferred_refresh)
            else:
                # Max retries reached
                self._log(f"Max retries reached, slots still not available", "Warning")
                self._retry_count = 0
        except Exception as e:
            self._log(f"Error in deferred refresh: {e}", "Error")
            self._retry_count = 0

    def refresh(self):
        """Refresh all slot values and reload slots if count changed"""
        # Check if slot count has changed
        if self.component and self.editor_bridge:
            try:
                current_count = self.editor_bridge.get_material_slot_count(self.component)
                if current_count != len(self.slot_widgets):
                    # Slot count changed, reload all slots
                    self._log(f"Slot count changed from {len(self.slot_widgets)} to {current_count}, reloading")
                    self._load_slots()
                    return
            except Exception as e:
                self._log(f"Error checking slot count: {e}", "Error")
                return

        # Just refresh values for existing widgets
        for widget in self.slot_widgets:
            if hasattr(widget, '_load_values'):
                try:
                    widget._load_values()
                except Exception as e:
                    self._log(f"Error refreshing widget values: {e}", "Warning")

