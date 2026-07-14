"""
Material Slots Widget for StaticMesh3D Inspector

Displays multiple material slots with collapsible/toggleable sections for each slot.
Reuses MaterialOverridePropertyWidget internals for consistency.
"""

from PyQt6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel,
                              QCheckBox, QPushButton, QFrame, QComboBox,
                              QLineEdit, QFileDialog)
from PyQt6.QtCore import Qt, pyqtSignal
from PyQt6.QtGui import QFont, QColor
import json
import sys
from pathlib import Path

# Add parent directory to path to import theme
sys.path.insert(0, str(Path(__file__).parent.parent))
from theme import get_theme_manager


class MaterialSlotWidget(QWidget):
    """Widget for a single material slot with collapsible override properties.

    Supports shader selection (built-in and custom) with dynamic parameter categories
    based on the selected shader type.
    """

    value_changed = pyqtSignal(int, str, object)  # slot_index, property_name, value

    def __init__(self, slot_index, slot_name, component, editor_bridge, main_editor=None, parent=None):
        super().__init__(parent)
        self.slot_index = slot_index
        self.slot_name = slot_name
        self.component = component
        self.editor_bridge = editor_bridge
        self.main_editor = main_editor
        self.category_widgets = {}
        self.current_shader_name = "PBR"  # Default shader
        self._custom_shader_path = ""

        # Import shader registry
        from editor.widgets.shader_definitions import get_shader_registry
        self.shader_registry = get_shader_registry()

        # Set backend from editor bridge if available
        self._update_backend_from_engine()

        self._init_ui()
        self._load_values()

    def _update_backend_from_engine(self):
        """Update the shader registry with the current graphics backend from the engine"""
        if self.editor_bridge:
            try:
                engine_backend = self.editor_bridge.get_backend()
                self.shader_registry.set_current_backend_from_engine(engine_backend)
            except Exception as e:
                print(f"[Warning] Could not get graphics backend: {e}")

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

        # Shader selection dropdown
        self._create_shader_selector()

        # Custom shader path (initially hidden)
        self._create_custom_shader_path()

        # Custom .lsh shader path (always visible; takes precedence over shaderType)
        self._create_lsh_shader_path()

        # Dynamic categories container
        self.dynamic_categories_widget = QWidget()
        self.dynamic_categories_layout = QVBoxLayout()
        self.dynamic_categories_layout.setContentsMargins(0, 0, 0, 0)
        self.dynamic_categories_layout.setSpacing(2)
        self.dynamic_categories_widget.setLayout(self.dynamic_categories_layout)
        self.properties_layout.addWidget(self.dynamic_categories_widget)

        # Build categories for default shader (PBR)
        self._rebuild_categories()

        # Add stretch at the end
        self.properties_layout.addStretch()

        layout.addWidget(self.properties_widget)

        # Initially collapsed
        self.properties_widget.setVisible(False)

    def _create_shader_selector(self):
        """Create the shader selection dropdown"""
        theme = get_theme_manager().get_current_theme()
        colors = theme.colors

        shader_layout = QHBoxLayout()
        shader_layout.setContentsMargins(0, 2, 0, 4)
        shader_layout.setSpacing(4)

        shader_label = QLabel("Shader:")
        shader_label.setStyleSheet(f"color: {colors.text_primary}; font-weight: bold;")
        shader_layout.addWidget(shader_label)

        self.shader_combo = QComboBox()
        # Add built-in shaders
        for shader_name in self.shader_registry.get_builtin_shader_names():
            self.shader_combo.addItem(shader_name)
        # Add "Custom" option
        self.shader_combo.addItem("Custom")

        self.shader_combo.setCurrentText("PBR")
        self.shader_combo.currentTextChanged.connect(self._on_shader_changed)
        self.shader_combo.setStyleSheet(f"""
            QComboBox {{
                background-color: {colors.surface};
                color: {colors.text_primary};
                border: 1px solid {colors.border};
                border-radius: 3px;
                padding: 3px 6px;
            }}
            QComboBox:hover {{
                background-color: {colors.surface_hover};
            }}
            QComboBox::drop-down {{
                border: none;
            }}
            QComboBox::down-arrow {{
                image: none;
                border-left: 4px solid transparent;
                border-right: 4px solid transparent;
                border-top: 6px solid {colors.text_secondary};
            }}
            QComboBox QAbstractItemView {{
                background-color: {colors.surface};
                color: {colors.text_primary};
                selection-background-color: {colors.accent_color};
            }}
        """)
        shader_layout.addWidget(self.shader_combo, 1)

        self.properties_layout.addLayout(shader_layout)

    def _create_custom_shader_path(self):
        """Create the custom shader path browser (hidden by default)"""
        theme = get_theme_manager().get_current_theme()
        colors = theme.colors

        # Get backend for appropriate labels
        backend = self.shader_registry.get_current_backend()

        self.custom_shader_widget = QWidget()
        custom_layout = QVBoxLayout()
        custom_layout.setContentsMargins(0, 0, 0, 4)
        custom_layout.setSpacing(2)

        # Vertex shader path - use backend-specific label
        vert_layout = QHBoxLayout()
        vert_layout.setSpacing(4)
        # Shorten the label for UI (e.g., "Vertex Shader (HLSL)" -> "Vertex:")
        vert_label_text = backend.vertex_shader_name.split()[0] + ":"
        self.vert_shader_label = QLabel(vert_label_text)
        self.vert_shader_label.setStyleSheet(f"color: {colors.text_secondary};")
        self.vert_shader_label.setFixedWidth(60)
        self.vert_shader_label.setToolTip(backend.vertex_shader_name)
        vert_layout.addWidget(self.vert_shader_label)

        self.vert_shader_edit = QLineEdit()
        self.vert_shader_edit.setPlaceholderText(backend.vertex_file_placeholder)
        self.vert_shader_edit.setStyleSheet(f"""
            QLineEdit {{
                background-color: {colors.surface};
                color: {colors.text_primary};
                border: 1px solid {colors.border};
                border-radius: 3px;
                padding: 2px;
            }}
        """)
        vert_layout.addWidget(self.vert_shader_edit, 1)

        vert_browse = QPushButton("...")
        vert_browse.setFixedWidth(25)
        vert_browse.clicked.connect(lambda: self._browse_shader("vertex"))
        vert_layout.addWidget(vert_browse)
        custom_layout.addLayout(vert_layout)

        # Fragment shader path - use backend-specific label
        frag_layout = QHBoxLayout()
        frag_layout.setSpacing(4)
        # Shorten the label for UI (e.g., "Pixel Shader (HLSL)" -> "Pixel:")
        frag_label_text = backend.fragment_shader_name.split()[0] + ":"
        self.frag_shader_label = QLabel(frag_label_text)
        self.frag_shader_label.setStyleSheet(f"color: {colors.text_secondary};")
        self.frag_shader_label.setFixedWidth(60)
        self.frag_shader_label.setToolTip(backend.fragment_shader_name)
        frag_layout.addWidget(self.frag_shader_label)

        self.frag_shader_edit = QLineEdit()
        self.frag_shader_edit.setPlaceholderText(backend.fragment_file_placeholder)
        self.frag_shader_edit.setStyleSheet(f"""
            QLineEdit {{
                background-color: {colors.surface};
                color: {colors.text_primary};
                border: 1px solid {colors.border};
                border-radius: 3px;
                padding: 2px;
            }}
        """)
        frag_layout.addWidget(self.frag_shader_edit, 1)

        frag_browse = QPushButton("...")
        frag_browse.setFixedWidth(25)
        frag_browse.clicked.connect(lambda: self._browse_shader("fragment"))
        frag_layout.addWidget(frag_browse)
        custom_layout.addLayout(frag_layout)

        # Apply button
        apply_layout = QHBoxLayout()
        apply_layout.addStretch()
        self.apply_shader_btn = QPushButton("Apply")
        self.apply_shader_btn.clicked.connect(self._apply_custom_shader)
        self.apply_shader_btn.setStyleSheet(f"""
            QPushButton {{
                background-color: {colors.accent_color};
                color: {colors.text_primary};
                border: none;
                border-radius: 3px;
                padding: 3px 10px;
            }}
            QPushButton:hover {{
                background-color: {colors.accent_hover};
            }}
        """)
        apply_layout.addWidget(self.apply_shader_btn)
        custom_layout.addLayout(apply_layout)

        self.custom_shader_widget.setLayout(custom_layout)
        self.custom_shader_widget.setVisible(False)
        self.properties_layout.addWidget(self.custom_shader_widget)

    def _create_lsh_shader_path(self):
        """Create the custom .lsh shader path row (always visible). A .lsh shader is
        translated to the active backend at runtime and its #render_mode drives the
        pipeline state; it takes precedence over the selected shader type when set."""
        theme = get_theme_manager().get_current_theme()
        colors = theme.colors

        self.lsh_shader_widget = QWidget()
        lsh_layout = QHBoxLayout()
        lsh_layout.setContentsMargins(0, 0, 0, 4)
        lsh_layout.setSpacing(4)

        lsh_label = QLabel("LSH:")
        lsh_label.setStyleSheet(f"color: {colors.text_secondary};")
        lsh_label.setFixedWidth(60)
        lsh_label.setToolTip("Custom .lsh shader (overrides shader type when set)")
        lsh_layout.addWidget(lsh_label)

        self.lsh_shader_edit = QLineEdit()
        self.lsh_shader_edit.setPlaceholderText("res:// path to a .lsh shader")
        self.lsh_shader_edit.setStyleSheet(f"""
            QLineEdit {{
                background-color: {colors.surface};
                color: {colors.text_primary};
                border: 1px solid {colors.border};
                border-radius: 3px;
                padding: 2px;
            }}
        """)
        self.lsh_shader_edit.editingFinished.connect(
            lambda: self._on_property_changed('customLshShader', self.lsh_shader_edit.text()))
        lsh_layout.addWidget(self.lsh_shader_edit, 1)

        lsh_browse = QPushButton("...")
        lsh_browse.setFixedWidth(25)
        lsh_browse.clicked.connect(self._browse_lsh_shader)
        lsh_layout.addWidget(lsh_browse)

        lsh_clear = QPushButton("x")
        lsh_clear.setFixedWidth(25)
        lsh_clear.setToolTip("Clear the .lsh shader")
        lsh_clear.clicked.connect(self._clear_lsh_shader)
        lsh_layout.addWidget(lsh_clear)

        self.lsh_shader_widget.setLayout(lsh_layout)
        self.properties_layout.addWidget(self.lsh_shader_widget)

    def _browse_lsh_shader(self):
        """Browse for a .lsh shader file and apply it to this slot."""
        from panels.inspector_panel import get_project_root, convert_to_res_path
        start_dir = get_project_root() or ""
        file_path, _ = QFileDialog.getOpenFileName(
            self, "Select .lsh Shader", start_dir, "Lupine Shader (*.lsh)")
        if file_path:
            res_path = convert_to_res_path(file_path)
            self.lsh_shader_edit.setText(res_path)
            self._on_property_changed('customLshShader', res_path)

    def _clear_lsh_shader(self):
        """Clear the .lsh shader from this slot."""
        self.lsh_shader_edit.setText("")
        self._on_property_changed('customLshShader', "")

    def _browse_shader(self, shader_type):
        """Browse for a shader file with backend-appropriate extensions"""
        # Get appropriate file filter based on current backend
        backend = self.shader_registry.get_current_backend()
        from editor.widgets.shader_definitions import GraphicsBackend

        if backend in (GraphicsBackend.OPENGL, GraphicsBackend.WEBGL):
            file_filter = "GLSL Shader (*.vert *.frag *.glsl)"
        elif backend == GraphicsBackend.VULKAN:
            file_filter = "SPIR-V Shader (*.spv);;GLSL Source (*.vert *.frag *.glsl)"
        elif backend in (GraphicsBackend.DIRECTX11, GraphicsBackend.DIRECTX12):
            file_filter = "HLSL Shader (*.hlsl)"
        elif backend == GraphicsBackend.METAL:
            file_filter = "Metal Shader (*.metal)"
        else:
            file_filter = "Vertex Shader (*.vert *.glsl)" if shader_type == "vertex" else "Fragment Shader (*.frag *.glsl)"

        from panels.inspector_panel import get_project_root, convert_to_res_path
        start_dir = get_project_root() or ""
        file_path, _ = QFileDialog.getOpenFileName(self, f"Select {shader_type.title()} Shader ({backend.value})", start_dir, file_filter)
        if file_path:
            # Convert to res:// path
            res_path = convert_to_res_path(file_path)
            if shader_type == "vertex":
                self.vert_shader_edit.setText(res_path)
            else:
                self.frag_shader_edit.setText(res_path)

    def _apply_custom_shader(self):
        """Apply the custom shader and reload parameters"""
        from editor.widgets.shader_definitions import CustomShaderLoader

        vert_path = self.vert_shader_edit.text()
        frag_path = self.frag_shader_edit.text()

        if not vert_path and not frag_path:
            return

        shader_def = CustomShaderLoader.load_from_file(vert_path or frag_path)
        if shader_def:
            self.shader_registry.register_custom_shader(shader_def)
            self._custom_shader_path = vert_path or frag_path
            self._rebuild_categories(shader_def)

            # Notify engine of custom shader change
            self._on_property_changed('customShaderVert', vert_path)
            self._on_property_changed('customShaderFrag', frag_path)

    def _on_shader_changed(self, shader_name):
        """Handle shader selection change"""
        self.current_shader_name = shader_name

        # Show/hide custom shader path widget
        is_custom = (shader_name == "Custom")
        self.custom_shader_widget.setVisible(is_custom)

        if not is_custom:
            # Rebuild categories for built-in shader
            shader_def = self.shader_registry.get_shader_by_display_name(shader_name)
            if shader_def:
                self._rebuild_categories(shader_def)
                # Notify engine of shader change
                self._on_property_changed('shaderType', shader_def.name)
                # Initialize all shader params with default values for modular shaders
                self._initialize_shader_params(shader_def)

    def _initialize_shader_params(self, shader_def):
        """Initialize all shader parameters with default values when shader is selected.
        This ensures uniforms like u_GlowParams are set even before user changes values."""
        if not self.component or not self.editor_bridge:
            return

        # Group parameters by uniform name
        uniform_groups = {}
        for category in shader_def.categories:
            for param in category.parameters:
                if param.uniform_name and param.uniform_component >= 0:
                    if param.uniform_name not in uniform_groups:
                        uniform_groups[param.uniform_name] = {}
                    uniform_groups[param.uniform_name][param.uniform_component] = param.default_value if param.default_value is not None else 0.0

        # Send each uniform Vec4 with default values
        for uniform_name, components in uniform_groups.items():
            vec4 = [0.0, 0.0, 0.0, 0.0]
            for idx, value in components.items():
                if 0 <= idx < 4:
                    vec4[idx] = float(value)

            value_json = json.dumps({
                "type": "vec4",
                "value": vec4
            })

            try:
                self.editor_bridge.set_material_slot_property(
                    self.component,
                    self.slot_index,
                    uniform_name,
                    value_json
                )
            except Exception as e:
                self._log(f"Failed to initialize shader param {uniform_name}: {e}", "Warning")

    def _rebuild_categories(self, shader_def=None):
        """Rebuild the material categories based on the current shader"""
        from editor.widgets.shader_definitions import ParameterType

        # Get shader definition if not provided
        if shader_def is None:
            shader_def = self.shader_registry.get_shader_by_display_name(self.current_shader_name)
            if shader_def is None:
                shader_def = self.shader_registry.get_shader("PBR")

        # Clear existing categories
        self._clear_categories()

        # Build new categories from shader definition
        for category in shader_def.categories:
            cat_widgets = self._create_category_dynamic(category.display_name)

            for param in category.parameters:
                widget = self._create_parameter_widget(param)
                if widget:
                    cat_widgets['layout'].addWidget(widget)
                    cat_widgets['widgets'][param.name] = widget

    def _clear_categories(self):
        """Clear all category widgets"""
        while self.dynamic_categories_layout.count():
            item = self.dynamic_categories_layout.takeAt(0)
            if item.widget():
                item.widget().deleteLater()
        self.category_widgets.clear()

    def _create_category_dynamic(self, title):
        """Create a category frame for dynamic shader parameters"""
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

        self.dynamic_categories_layout.addWidget(frame)

        category = {
            'frame': frame,
            'layout': layout,
            'widgets': {}
        }
        self.category_widgets[title] = category
        return category

    def _create_parameter_widget(self, param):
        """Create a widget for a shader parameter based on its type"""
        from editor.widgets.shader_definitions import ParameterType
        from editor.panels.inspector_panel import (
            ColorPropertyWidget, FloatPropertyWidget, PathPropertyWidget,
            Vec2PropertyWidget, Vec3PropertyWidget, Vec4PropertyWidget,
            IntPropertyWidget, BoolPropertyWidget
        )

        if param.param_type == ParameterType.FLOAT:
            widget = FloatPropertyWidget(param.display_name)
            if param.default_value is not None:
                widget.set_value(param.default_value)
            widget.value_changed.connect(lambda v, p=param.name: self._on_property_changed(p, v))
            return widget

        elif param.param_type == ParameterType.COLOR:
            widget = ColorPropertyWidget(param.display_name)
            if param.default_value:
                widget.set_value(QColor.fromRgbF(*param.default_value[:4]))
            widget.value_changed.connect(lambda v, p=param.name: self._on_property_changed(p, v))
            return widget

        elif param.param_type == ParameterType.TEXTURE:
            widget = PathPropertyWidget(param.display_name)
            widget.value_changed.connect(lambda v, p=param.name: self._on_property_changed(p + "Path", v))
            return widget

        elif param.param_type == ParameterType.VEC2:
            widget = Vec2PropertyWidget(param.display_name)
            widget.value_changed.connect(lambda v, p=param.name: self._on_property_changed(p, v))
            return widget

        elif param.param_type == ParameterType.VEC3:
            widget = Vec3PropertyWidget(param.display_name)
            widget.value_changed.connect(lambda v, p=param.name: self._on_property_changed(p, v))
            return widget

        elif param.param_type == ParameterType.VEC4:
            widget = Vec4PropertyWidget(param.display_name)
            widget.value_changed.connect(lambda v, p=param.name: self._on_property_changed(p, v))
            return widget

        elif param.param_type == ParameterType.INT:
            widget = IntPropertyWidget(param.display_name)
            if param.default_value is not None:
                widget.set_value(param.default_value)
            widget.value_changed.connect(lambda v, p=param.name: self._on_property_changed(p, v))
            return widget

        elif param.param_type == ParameterType.BOOL:
            widget = BoolPropertyWidget(param.display_name)
            if param.default_value is not None:
                widget.set_value(param.default_value)
            widget.value_changed.connect(lambda v, p=param.name: self._on_property_changed(p, v))
            return widget

        return None

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
            # Check if this parameter has uniform mapping (for modular shader params)
            shader_def = self.shader_registry.get_shader_by_display_name(self.current_shader_name)
            if shader_def:
                param_def = self._find_parameter_definition(shader_def, property_name)
                if param_def and param_def.uniform_name:
                    # Use modular shader params system
                    self._set_shader_param_vec4(shader_def, param_def.uniform_name, property_name, value)
                    self.value_changed.emit(self.slot_index, property_name, value)
                    return

            # Legacy path - convert value to JSON
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

    def _find_parameter_definition(self, shader_def, param_name):
        """Find a parameter definition by name in the shader definition"""
        for category in shader_def.categories:
            for param in category.parameters:
                if param.name == param_name:
                    return param
        return None

    def _get_all_uniform_params(self, shader_def, uniform_name):
        """Get all parameters that map to the same uniform"""
        params = []
        for category in shader_def.categories:
            for param in category.parameters:
                if param.uniform_name == uniform_name:
                    params.append(param)
        return params

    def _set_shader_param_vec4(self, shader_def, uniform_name, changed_param_name, new_value):
        """Build and send a Vec4 shader param from all parameters mapping to the uniform"""
        # Get all parameters for this uniform
        params = self._get_all_uniform_params(shader_def, uniform_name)

        # Build Vec4 from current widget values, with the new value for the changed param
        vec4 = [0.0, 0.0, 0.0, 0.0]
        for param in params:
            if param.uniform_component >= 0 and param.uniform_component < 4:
                if param.name == changed_param_name:
                    vec4[param.uniform_component] = float(new_value)
                else:
                    # Get current value from widget
                    current_value = self._get_widget_value(param.name)
                    if current_value is not None:
                        vec4[param.uniform_component] = float(current_value)
                    elif param.default_value is not None:
                        vec4[param.uniform_component] = float(param.default_value)

        # Send as shaderParams format: {"type": "vec4", "value": [x, y, z, w]}
        value_json = json.dumps({
            "type": "vec4",
            "value": vec4
        })

        self.editor_bridge.set_material_slot_property(
            self.component,
            self.slot_index,
            uniform_name,
            value_json
        )

    def _get_widget_value(self, param_name):
        """Get current value from a parameter widget"""
        for category_name, category_data in self.category_widgets.items():
            widgets = category_data.get('widgets', {})
            if param_name in widgets:
                widget = widgets[param_name]
                if hasattr(widget, 'get_value'):
                    return widget.get_value()
                elif hasattr(widget, 'value'):
                    return widget.value()
        return None

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

            # Set shader type if stored
            shader_type = properties.get('shaderType', 'PBR')
            if shader_type and shader_type != self.current_shader_name:
                shader_def = self.shader_registry.get_shader(shader_type)
                if shader_def:
                    self.shader_combo.blockSignals(True)
                    self.shader_combo.setCurrentText(shader_def.display_name)
                    self.shader_combo.blockSignals(False)
                    self.current_shader_name = shader_def.display_name
                    self._rebuild_categories(shader_def)

            # Load the custom .lsh shader path
            if hasattr(self, 'lsh_shader_edit'):
                self.lsh_shader_edit.blockSignals(True)
                self.lsh_shader_edit.setText(properties.get('customLshShader', ''))
                self.lsh_shader_edit.blockSignals(False)

            # Load parameter values dynamically
            self._load_parameter_values(properties)

        except Exception as e:
            self._log(f"Failed to load material slot values: {e}", "Warning")

    def _load_parameter_values(self, properties):
        """Load parameter values from properties into widgets"""
        for category_name, category_data in self.category_widgets.items():
            widgets = category_data.get('widgets', {})
            for param_name, widget in widgets.items():
                if param_name in properties:
                    value = properties[param_name]
                    try:
                        # Handle color values
                        if isinstance(value, dict) and 'r' in value:
                            qcolor = QColor.fromRgbF(
                                value.get('r', 1.0),
                                value.get('g', 1.0),
                                value.get('b', 1.0),
                                value.get('a', 1.0)
                            )
                            widget.set_value(qcolor)
                        else:
                            widget.set_value(value)
                    except Exception as e:
                        self._log(f"Failed to set widget value for {param_name}: {e}", "Warning")


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

